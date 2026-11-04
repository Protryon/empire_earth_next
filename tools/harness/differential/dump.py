"""Round 39 differential harness (TEST ONLY): dump mode. Re-run both images with the same pins, stop each at PT X with
full snapshots at X-1 and X, list the differing objects/fields and map each field to the source functions that write it.

  python -I tools/boot/differential/dump.py --pt X --label L [--seed 12345 --pt-rate 40] [--b-fulllink PATH]
         [-- <extra game_session args, e.g. --exercise ai-game --ai-players 4 --time-factor 8>]
  python -I tools/boot/differential/dump.py --pt X --a A.stream.gz --b B.stream.gz     # analyse existing dumps only

Runs (A = original, B = full link) go in parallel as game_session --instance 29/30, each with
--diff-full-at X-1,X --diff-stop-after X --diff-snapshot-every 0; outputs in build/scratch_round39/hash/dump_<L>/.
AI objects (computer players, ai_layout.py) are diffed by object path and their members join the writer index.
Writers: every `mName =`/`op=`/`++`/`--` of the member in src/**/*.cpp, with the enclosing `// FUNCTION: EXE 0x..`
marker and that function's round-38 score (build/base_r38_exe_all.json): non-exact writers first.
"""
import argparse, json, pathlib, re, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE))
import compare as C
import layout as L

SCORES = ROOT / 'build/base_r38_exe_all.json'


def setters(names):
    """{setter method: member} for inline header methods whose body assigns one of the members
    (e.g. `void SetF230(float inValue) { if (mF230 != inValue) mF230 = inValue; }`)."""
    out = {}
    body = re.compile(r'\b(\w+)\s*\([^;{)]*\)\s*(?:const\s*)?\{([^{}]*)\}')
    for h in (ROOT / 'include').rglob('*.h'):
        for m in body.finditer(h.read_text(encoding='latin-1')):
            for n in names:
                if re.search(r'\b%s\b\s*(?:[-+*/|&^]|<<|>>)?=(?!=)|\b%s\s*(?:\+\+|--)|(?:\+\+|--)\s*%s\b' % (n, n, n), m.group(2)):
                    out.setdefault(m.group(1), n)
    return out


def writer_index(names):
    """{member: [(file, line, va, function text)]} for writes of the given member names in src/: direct stores
    (`m =`, `op=`, `++`, `--`) and calls of inline header setters that store it. Matching is by member NAME, so
    same-named members of other classes are included."""
    want = {n for n in names if n}
    if not want:
        return {}
    sets = setters(want)
    alt = '|'.join(map(re.escape, want))
    pat = re.compile(r'\b(%s)\b\s*(\[[^\]]*\]\s*)?(?:[-+*/|&^]|<<|>>)?=(?!=)|(?:\+\+|--)\s*(?:\w+(?:->|\.))*(%s)\b|\b(%s)\s*(?:\+\+|--)'
                     % (alt, alt, alt))
    spat = re.compile(r'(?:->|\.|\b)(%s)\s*\(' % '|'.join(map(re.escape, sets))) if sets else None
    marker = re.compile(r'//\s*FUNCTION:\s*EXE\s+(0x[0-9a-fA-F]+)')
    out = {n: [] for n in want}
    for f in sorted((ROOT / 'src').rglob('*.cpp')):
        va, sig = None, ''
        lines = f.read_text(encoding='latin-1').splitlines()
        for i, l in enumerate(lines):
            m = marker.search(l)
            if m:
                va = m.group(1).lower()
                sig = next((x.strip() for x in lines[i + 1:i + 12] if x.strip() and not x.strip().startswith('//')), '')
                continue
            code = l.split('//')[0]
            if 'Read(' in code:
                continue
            for m in pat.finditer(code):
                n = m.group(1) or m.group(3) or m.group(4)
                if n in out:
                    out[n].append((str(f.relative_to(ROOT)), i + 1, va, sig))
            if spat:
                for m in spat.finditer(code):
                    out[sets[m.group(1)]].append((str(f.relative_to(ROOT)), i + 1, va, sig + '  [via ' + m.group(1) + ']'))
    return out


def scores():
    try:
        data = json.loads(SCORES.read_text(encoding='utf-8'))
    except OSError:
        return {}
    return {va: tuple(v) for unit in data.values() for va, v in unit.items()}


def base_name(field):
    return re.sub(r'(\[\d+\]|\+0x[0-9a-f]+| -> unit id)$', '', field.split(' ')[0])


def ai_member(name):
    """Member name of an AI record field for the writer index: 'mTask.mPlayer' -> 'mPlayer', '#mUnits' -> 'mUnits',
    'mQueues[3]' -> 'mQueues', 'v.second' (container element) -> skipped as ''."""
    n = name.lstrip('#').split('.')[-1]
    n = re.sub(r'\[\d+\]$', '', n)
    return n if n.startswith('m') else ''


def report(a_stream, b_stream, pt, out_json=None):
    pts = [pt - 1, pt]
    sa, sb = C.snapshots(a_stream, set(pts)), C.snapshots(b_stream, set(pts))
    res = dict(pt=pt, a=str(a_stream), b=str(b_stream), snapshots={})
    fields = set()
    for p in pts:
        if p not in sa or p not in sb:
            print('no snapshot for PT %d in %s' % (p, 'A' if p not in sa else 'B'))
            continue
        d = C.diff_snapshots(sa[p], sb[p], max_units=10 ** 6)
        res['snapshots'][p] = d
        for u in d['units']:
            for f in u['fields']:
                if f[2] in ('sim', 'render'):
                    fields.add(base_name(f[0]))
        for pl in d['players']:
            for f in pl['fields']:
                fields.add(base_name(f[0]))
        if 'ai' in sa[p] and 'ai' in sb[p]:   # AI objects (ai_layout.py records): by object path
            d['ai'] = C.diff_ai(sa[p], sb[p], max_objects=10 ** 6)
            for o in d['ai']['objects']:
                for name, _, _ in o['fields']:
                    if name != 'class (RTTI)':
                        fields.add(ai_member(name))
    idx = writer_index(fields)
    sc = scores()
    res['writers'] = {}
    for p, d in res['snapshots'].items():
        print('\n== PT %d: physics %s, players differing %d, units differing %d (%d sim), only A %s, only B %s' % (
            p, [x[0] for x in d['physics']], len(d['players']), d['units_differing'], d['sim_units'], d['only_a'][:10], d['only_b'][:10]))
        for u in d['units'][:40]:
            print(' unit %d (player %d, %s, class %s)' % (u['id'], u['player'], u['kind'], u['cls']))
            for f in u['fields'][:16]:
                print('   %-30s [%s] %s | %s' % (f[0], f[2], f[3], f[4]))
        if d.get('ai'):
            C.print_ai(d['ai'], indent=' ')
    print('\n== writers of the differing members (src/, non-exact first; scores round 38)')
    for name in sorted(fields):
        rows = []
        for file, line, va, sig in idx.get(name, []):
            st = sc.get(va, ('?', None)) if va else ('no marker', None)
            rows.append(dict(file=file, line=line, va=va, function=sig, status=st[0], score=st[1]))
        rows.sort(key=lambda r: (r['status'] == 'exact', r['score'] or 0))
        res['writers'][name] = rows
        funcs = {}
        for r in rows:   # one line per writing function, with its source lines
            funcs.setdefault((r['va'], r['file']), [r, []])[1].append(r['line'])
        print(' %s: %d write sites in %d functions' % (name, len(rows), len(funcs)))
        for (va, file), (r, ls) in list(funcs.items())[:15]:
            print('   %-6s %6s %s  %s:%s  %s' % (r['status'], r['score'], va, file, ','.join(map(str, ls)), r['function'][:60]))
    if out_json:
        pathlib.Path(out_json).write_text(json.dumps(res, indent=1, default=str))
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--pt', type=int, required=True)
    ap.add_argument('--label', default='dump')
    ap.add_argument('--also', default='', help='more PTs with full snapshots (comma list; diff them with compare.py --at)')
    ap.add_argument('--a', help='existing stream A (skip running)')
    ap.add_argument('--b', help='existing stream B')
    ap.add_argument('--seed', default='12345')
    ap.add_argument('--pt-rate', default='40')
    ap.add_argument('--b-fulllink', default=str(ROOT / 'build/exe_fulllink/EE-AOC.exe'))
    ap.add_argument('--a-fulllink', help='run A on this full link instead of the original')
    ap.add_argument('--exercise', default='random-game')
    ap.add_argument('--play-seconds', default='600')
    ap.add_argument('--timeout', default='3600000')
    ap.add_argument('extra', nargs='*', help='after --: extra game_session args')
    a = ap.parse_args()
    out = ROOT / 'build/scratch_round39/hash' / ('dump_' + a.label)
    out.mkdir(parents=True, exist_ok=True)
    if not (a.a and a.b):
        procs = []
        for tag, inst, img in (('a', 29, ['--fulllink', a.a_fulllink] if a.a_fulllink else ['--original']),
                               ('b', 30, ['--fulllink', a.b_fulllink])):
            cmd = [sys.executable, '-I', str(ROOT / 'tools/boot/game_session.py'), *img, '--instance', str(inst),
                   '--exercise', a.exercise, '--seed', a.seed, '--pt-rate', a.pt_rate, '--play-seconds', a.play_seconds,
                   '--timeout', a.timeout, '--diff-stream', str(out / (tag + '.stream.txt')), '--diff-snapshot-every', '0',
                   '--diff-full-at', ','.join(['%d' % (a.pt - 1), '%d' % a.pt] + [x for x in a.also.split(',') if x]), '--diff-stop-after', str(a.pt), *a.extra]
            log = open(out / (tag + '.session.log'), 'w')
            procs.append((tag, subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT, cwd=ROOT), log))
            print('started', tag, ' '.join(cmd[2:]), flush=True)
        for tag, p, log in procs:
            p.wait(); log.close(); print(tag, 'session exit', p.returncode, flush=True)
        a.a, a.b = out / 'a.stream.txt', out / 'b.stream.txt'
    report(a.a, a.b, a.pt, out / 'report.json')
    print('\nreport ->', out / 'report.json')


if __name__ == '__main__':
    main()
