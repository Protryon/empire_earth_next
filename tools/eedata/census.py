"""The field census (eedata.py census): every read of every db record member in the sources.

Renames every member of every record struct (the structs in overlay.STRUCT_SOURCES) to <name>__P in its source,
compiles every unit of every module syntax-only (clang-cl /Zs), and collects clang's "no member named X in Y"
errors: one per use site. The sources are restored afterwards, also on failure (do not edit or build the checkout
while it runs).

Reports, per struct, the members with use sites and the members without (a member whose only use is the loader's
copy into a class member that nothing reads still counts as read here: those dead copies are named mUnused* by
hand, with the evidence in docs/DATA_FORMATS.md section 8). Fails when a member the text carries has no use site
(an mUnused* member with use sites is listed: those must be the loader's copies into dead class members). Slow:
about 6 minutes at -j 2.
"""
import collections
import concurrent.futures
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / 'tools'))

import overlay   # noqa: E402
import specgen   # noqa: E402

ERR = re.compile(r"^(.*?)\((\d+),(\d+)\)\s*:\s*error: no member named '(\w+)' in '(?:struct )?([\w:]+)'", re.M)


def _renamed(text, sname):
    """text with every field of struct sname renamed to <name>__P (inside the struct body only)."""
    m = re.search(r'struct\s+' + sname + r'\s*\{', text)
    i = j = m.end()
    depth = 1
    while depth:
        depth += {'{': 1, '}': -1}.get(text[j], 0)
        j += 1
    lines = text[i:j - 1].split('\n')
    d = 0
    for k, ln in enumerate(lines):
        s = ln.strip()
        if d:
            d += s.count('{') - s.count('}')
            continue
        if '(' in s.split('//')[0]:
            d = max(0, s.count('{') - s.count('}'))
            continue
        fm = specgen.FIELD.match(ln)
        if fm and fm.group(1) not in ('static', 'return', 'class', 'struct', 'virtual'):
            lines[k] = re.sub(r'\b' + fm.group(2) + r'\b', fm.group(2) + '__P', ln, count=1)
    return text[:i] + '\n'.join(lines) + text[j - 1:]


def sites(jobs=2):
    """{(struct, member): [(file, line)]} over every unit."""
    import clang_build
    import units
    import verify
    files = collections.defaultdict(list)
    for sname, src in overlay.STRUCT_SOURCES.items():
        files[src].append(sname)
    orig = {}
    try:
        for src, snames in files.items():
            p = ROOT / src
            orig[p] = p.read_bytes()
            text = orig[p].decode('latin-1')
            for sname in snames:
                text = _renamed(text, sname)
            p.write_bytes(text.encode('latin-1'))
        work = [j for m in units.MODULES for j in units.jobs_for(m)]
        work = [j for j in work if 'ThirdParty' not in str(j[1])]
        for j in work:
            verify.include_paths(verify.stage_source(j[1]))
        obj = verify.ORIG_SRC_ROOT.parent / (verify.ORIG_SRC_ROOT.name + '-census') / 'x.obj'
        out = collections.defaultdict(set)

        def one(job):
            return clang_build.compile_to(job[1], job[2], obj, '-O0', extra=('/Zs',))
        with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
            for job, (ok, log) in zip(work, ex.map(one, work)):
                if ok:
                    continue
                found = ERR.findall(log)
                other = [l for l in log.splitlines() if ': error:' in l and 'no member named' not in l]
                if other:
                    raise SystemExit(f'{job[1]}: {other[0]}')
                for f, line, col, member, struct in found:
                    out[(struct, member)].add((pathlib.Path(f).name, int(line)))
        return out
    finally:
        for p, raw in orig.items():
            p.write_bytes(raw)


def run(a, spec):
    uses = sites(a.j)
    bad = 0
    rows = []
    for sname, st in spec['structs'].items():
        if st['source'] is None:
            continue
        for f in st['fields']:
            if f['cls'] == 'pad':
                continue
            u = sorted(uses.get((sname, f['member']), ()))
            rows.append((sname, f['member'], f['cls'], u))
            if f['cls'] == 'named' and not u:
                print(f'{sname}.{f["member"]}: the text carries it, but nothing reads it')
                bad += 1
    dead = [f'{s}.{m}' for s, m, c, u in rows if c == 'unused' and u]
    if dead:
        print('unused fields the loaders still copy (into class members nothing reads): ' + ', '.join(dead))
    if a.out:
        pathlib.Path(a.out).write_text(''.join(f'{s}\t{m}\t{c}\t' + ' '.join(f'{x}:{y}' for x, y in u) + '\n'
                                               for s, m, c, u in rows), encoding='utf-8')
    n = collections.Counter((c, bool(u)) for _, _, c, u in rows)
    print(f'{sum(len(v) for v in uses.values())} use sites; named fields read: {n[("named", True)]}, '
          f'named but unread: {n[("named", False)]}; unused fields with a use site (the loader\'s dead copies): '
          f'{n[("unused", True)]}, without: {n[("unused", False)]}; labels read: {n[("label", True)]}')
    return 1 if bad else 0
