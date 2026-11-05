"""Generate the markdown field tables (layout from the sources + observations from the AoC data)."""
import struct, json, os, sys, collections, math, re

HERE = os.path.dirname(os.path.abspath(__file__))
sys.argv = [sys.argv[0]]
exec(open(os.path.join(HERE, 'analyze.py')).read().split('\ndef main')[0])
sys.path.insert(0, HERE)
from structs import is_undecoded

UNITCPP = r"C:\Users\max\Desktop\empire_earth_next\src\Empire Earth\Units\EEUnitClass.cpp"
ctor = {}
for ln in open(UNITCPP, encoding='latin-1'):
    m = re.search(r'(this->[\w\[\]]+(?:\s*=\s*this->[\w\[\]]+)*)\s*=\s*(.*?)inRecord->(m\w+)', ln)
    if m and m.group(3) not in ctor:
        lhs = m.group(1).replace('this->', '')
        wrap = m.group(2).strip()
        ctor[m.group(3)] = lhs + (f' via {wrap.rstrip("(")}' if wrap and 'Get' in wrap else '')


def obs(recs, off, typ, count):
    if typ == 'pad':
        vals = [r[off:off + count] for r in recs]
        c = collections.Counter(vals)
        return 'bytes ' + ', '.join(f'{k.hex()}x{v}' for k, v in c.most_common(2))
    if typ == 's8' and count > 1:
        vals = [r[off:off + count] for r in recs]
        s = [v.split(b'\0')[0] for v in vals]
        ne = sum(1 for x in s if x)
        junk = sum(1 for v in vals if b'\0' in v and v.split(b'\0', 1)[1].strip(b'\0'))
        ml = max(len(x) for x in s)
        ex = next((x.decode('latin-1') for x in s if x), '')
        r = f'text {ne}/{len(vals)}, max len {ml}'
        if junk:
            r += f', junk after NUL in {junk}'
        if ex:
            r += f', e.g. "{ex[:28]}"'
        return r
    fmt = {'s32': 'i', 'u32': 'I', 'f32': 'f', 'bool': 'B', 'u8': 'B', 's8': 'b', 'u16': 'H', 's16': 'h', 'pair_f32': 'f'}[typ]
    sz = struct.calcsize(fmt)
    vals = [struct.unpack_from('<' + fmt, r, off)[0] for r in recs]
    c = collections.Counter(vals)
    if typ == 'f32' or typ == 'pair_f32':
        v2 = [round(v, 4) for v in vals if not math.isnan(v)]
        if len(c) == 1:
            return f'const {v2[0]:g}'
        return f'{min(v2):g}..{max(v2):g} ({len(c)} distinct)'
    hole = 0xcccccccc if typ == 'u32' else (-858993460 if typ == 's32' else 0xcc)
    real = [v for v in vals if v != hole]
    note = f'; 0xCC in {len(vals) - len(real)}' if len(real) != len(vals) and real else ''
    if not real:
        return 'always 0xCC fill'
    cr = collections.Counter(real)
    if len(cr) == 1:
        return f'const {real[0]}' + note
    return f'{min(real)}..{max(real)} ({len(cr)} distinct)' + note


def split_gap(recs, f):
    """Split a leading/inner undecoded byte gap into inferred pieces using the data."""
    out = []
    off, size = f['off'], f['size']
    end = off + size
    o = off
    while o + 4 <= end:
        if o + 100 <= end:
            s = [r[o:o + 100].split(bytes(1))[0] for r in recs]
            if sum(1 for x in s if x and is_text(x)) > len(recs) * 0.5:
                out.append(dict(off=o, type='s8', name='(name)', count=100, size=100,
                                comment='INFERRED from data: the row name (char[100], NUL-terminated)', inferred=True))
                o += 100
                continue
        iv = [struct.unpack_from('<i', r, o)[0] for r in recs]
        idx = sum(1 for i, v in enumerate(iv) if v == i)
        nm = '(unknown)'
        cm = 'undecoded'
        if idx >= len(recs) * 0.6:
            nm, cm = '(index)', 'INFERRED: equals the 0-based row index'
        elif len(set(iv)) >= len(recs) * 0.6 and all(-1 <= v < 100000 for v in iv):
            nm, cm = '(db id)', 'INFERRED: unique per row, gappy: the database row id'
        out.append(dict(off=o, type='s32', name=nm, count=1, size=4, comment=cm, inferred=nm != '(unknown)'))
        o += 4
    if o < end:
        out.append(dict(off=o, type='pad', name='(unknown)', count=end - o, size=end - o, comment='undecoded'))
    return out


def table(fname, sname, kind, ctor_map=None):
    d, recs, extra, end = records(fname, sname, kind)
    real = [r for r in recs]
    st = S[sname]
    rows = []
    cls = collections.Counter()
    for f in st['fields']:
        pieces = [f]
        if (f['name'].startswith('_') or f['name'].startswith('mUndecoded') or f['name'].startswith('mUnknown00')) and f['size'] >= 4:
            pieces = split_gap(real, f)
        for p in pieces:
            semantic = not is_undecoded(p) and not p['name'].startswith('(')
            if p.get('inferred'):
                k = 'inferred'
            elif semantic:
                k = 'named'
            elif p['type'] == 'pad' or p['name'] == '(unknown)' or p['name'].startswith('_') or 'Unknown' in p['name'] or 'Undecoded' in p['name']:
                k = 'unknown'
            else:
                k = 'provisional'
            if ctor_map is not None and k == 'provisional' and p['name'] not in ctor_map:
                k = 'unknown'
                if p['type'] == 's8' and p['count'] == 1:
                    p = dict(p, comment='padding byte (0, or 0xCC in holes); not read')
                else:
                    p = dict(p, comment=(p['comment'] + '; ' if p['comment'] else '') + 'not read by the EEUnitClass ctor')
            cls[k] += p['size']
            t = p['type'] + (f'[{p["count"]}]' if p['count'] > 1 and p['type'] != 'pad' else '')
            if p['type'] == 's8' and p['count'] > 1:
                t = f'char[{p["count"]}]'
            o = obs(real, p['off'], p['type'] if p['count'] == 1 or p['type'] in ('s8', 'pad') else p['type'], p['count'] if p['type'] in ('s8', 'pad') else 1)
            if p['count'] > 1 and p['type'] not in ('s8', 'pad'):
                o = '[0]: ' + o
            cmt = p['comment'].replace('|', '/')
            if ctor_map and p['name'] in ctor_map:
                cmt = (cmt + '; ' if cmt else '') + 'ctor -> ' + ctor_map[p['name']]
            rows.append(f"| 0x{p['off']:03x} | {t} | `{p['name']}` | {k} | {cmt} | {o} |")
    tot = st['size']
    summ = {k: round(100 * v / tot) for k, v in cls.items()}
    return rows, summ, len(recs), d


if __name__ == '__main__':
    res = {}
    for fname, sname, kind in TABLES:
        rows, summ, n, d = table(fname, sname, kind, ctor if sname == 'EEUnitClassRecord' else None)
        res[fname] = dict(struct=sname, size=S[sname]['size'], records=n, filesize=len(d), classes=summ, rows=rows)
        print(fname, sname, n, summ)
    json.dump(res, open(os.path.join(HERE, 'tables.json'), 'w'), indent=1)
