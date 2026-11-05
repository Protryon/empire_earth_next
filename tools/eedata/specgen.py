"""Generate the table specs (specs.json) from the record structs in the sources plus overlay.py.

Each struct is parsed from its source file (STRUCT_SOURCES) with MSVC x86 natural alignment; its size is checked
against the source's static_assert (or COMMENT_SIZES). Gaps the survey decoded are split (overlay.SPLITS); every
other byte stays an explicit field: typed fields keep their source names, undecoded bytes become u_0x<off> raw
fields, and compiler padding becomes pad fields.

Field types: s8 u8 s16 u16 s32 u32 f32 bool (one byte, raw value kept), str (char[N], NUL-terminated text plus a
tail), raw (bytes, hex), pad (alignment padding, hex), pair_f32 (two floats). count > 1 makes an array (str: N).
Classes: named, inferred, provisional, unknown, pad (DATA_FORMATS.md, "Field classes").
"""
import json
import pathlib
import re

import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import overlay  # noqa: E402

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SPECS = HERE / 'specs.json'
FORMAT_VERSION = 1

TYPES = {'char': ('s8', 1), 'signed char': ('s8', 1), 'unsigned char': ('u8', 1), 'bool': ('bool', 1),
         'short': ('s16', 2), 'unsigned short': ('u16', 2), 'long': ('s32', 4), 'unsigned long': ('u32', 4),
         'int': ('s32', 4), 'unsigned int': ('u32', 4), 'float': ('f32', 4), 'EECalamityKind': ('s32', 4),
         'int32': ('s32', 4), 'EECivilizationUpgradeValue': ('pair_f32', 8), 'WORD': ('u16', 2),
         'DWORD': ('u32', 4), 'BYTE': ('u8', 1)}
SIZES = {'s8': 1, 'u8': 1, 'bool': 1, 's16': 2, 'u16': 2, 's32': 4, 'u32': 4, 'f32': 4, 'pair_f32': 8, 'str': 1,
         'raw': 1, 'pad': 1}
FIELD = re.compile(r'^\s*(?:volatile\s+)?((?:unsigned\s+|signed\s+)?\w+)\s+(\w+)\s*(\[[^\]]+\])?\s*;\s*(?://\s*(.*))?$')
GAP = re.compile(r'^(_gap\w*|_u[0-9A-Fa-f]+|mUndecoded\w*)$')
PADNAME = re.compile(r'^(mPad\w*|_pad\w*)$')
UNKNOWN = re.compile(r'^(mUnknown\w*|_u\w*)$')
PROVISIONAL = re.compile(r'^(m[LUCBF][0-9A-Fa-f]{2,3}|mU[0-9a-f]{3}|mBoolPlaceholder\d|mIntParam\d|mFloatParam\d|'
                         r'mParam\d+)$')


def _eval_size(expr):
    if not re.fullmatch(r'[0-9a-fA-FxX+*/() -]+', expr):
        raise ValueError(f'array size {expr!r}')
    return int(eval(expr.replace('/', '//')))   # literals and arithmetic only (checked above)


def _body(text, name):
    m = re.search(r'struct\s+' + name + r'\s*\{', text)
    if not m:
        raise SystemExit(f'struct {name} not found')
    i = j = m.end()
    depth = 1
    while depth:
        depth += {'{': 1, '}': -1}.get(text[j], 0)
        j += 1
    return text[i:j - 1]


def source_fields(name):
    """[(offset, type, source name, count, comment)] of a source struct, x86 natural layout, with pad entries."""
    text = (ROOT / overlay.STRUCT_SOURCES[name]).read_text(encoding='latin-1')
    out, off, maxal, depth = [], 0, 1, 0
    for ln in _body(text, name).split('\n'):
        s = ln.strip()
        if depth:   # inside an inline member function body
            depth += s.count('{') - s.count('}')
            continue
        if '(' in s.split('//')[0]:
            depth = max(0, s.count('{') - s.count('}'))
            continue
        m = FIELD.match(ln)
        if not m:
            continue
        t, n, arr, cmt = m.groups()
        if t in ('static', 'return', 'class', 'struct', 'virtual'):
            continue
        if t not in TYPES:
            raise SystemExit(f'{name}.{n}: unknown type {t}')
        code, sz = TYPES[t]
        cnt = _eval_size(arr[1:-1]) if arr else 1
        al = 4 if code == 'pair_f32' else sz
        maxal = max(maxal, al)
        if off % al:
            out.append((off, 'pad', None, al - off % al, 'alignment padding'))
            off += al - off % al
        out.append((off, code, n, cnt, (cmt or '').strip()))
        off += sz * cnt
    if off % maxal:
        out.append((off, 'pad', None, maxal - off % maxal, 'tail padding'))
        off += maxal - off % maxal
    return out, off


def static_assert_size(name):
    text = (ROOT / overlay.STRUCT_SOURCES[name]).read_text(encoding='latin-1')
    m = re.search(r'static_assert\s*\(\s*sizeof\s*\(\s*' + name + r'\s*\)\s*==\s*(0x[0-9a-fA-F]+|\d+)', text)
    if m:
        return int(m.group(1), 0), 'static_assert'
    if name in overlay.COMMENT_SIZES:
        return overlay.COMMENT_SIZES[name], 'comment'
    return None, None


def _raw_pieces(off, end, out):
    """Undecoded bytes [off, end): aligned dwords as s32 u_0x<off>, unaligned runs as raw bytes."""
    o = off
    while o < end:
        if o % 4 == 0 and o + 4 <= end:
            out.append(dict(name=f'u_0x{o:03x}', off=o, type='s32', count=1, cls='unknown', comment='undecoded'))
            o += 4
        else:
            n = min(end, (o // 4 + 1) * 4) - o
            out.append(dict(name=f'u_0x{o:03x}', off=o, type='raw', count=n, cls='unknown', comment='undecoded'))
            o += n


def _split_gap(sname, off, size, out):
    splits = overlay.SPLITS.get(sname, {})
    end = off + size
    o = off
    for so in sorted(k for k in splits if off <= k < end):
        _raw_pieces(o, so, out)
        kind = splits[so]
        if kind == 'name':
            out.append(dict(name='name', off=so, type='str', count=100, cls='inferred',
                            comment='the row name (inferred from the data)'))
            o = so + 100
        else:
            out.append(dict(name=kind, off=so, type='s32', count=1, cls='inferred',
                            comment={'dbId': 'the database row id', 'index': 'the 0-based row index'}[kind] +
                            ' (inferred from the data)'))
            o = so + 4
    _raw_pieces(o, end, out)


def _classify(n):
    if PADNAME.match(n):
        return 'pad'
    if UNKNOWN.match(n):
        return 'unknown'
    if PROVISIONAL.match(n):
        return 'provisional'
    return 'named'


def struct_spec(sname):
    if sname in overlay.VIRTUAL_STRUCTS:
        fields, off = [], 0
        for n, t, c, cls in overlay.VIRTUAL_STRUCTS[sname]:
            fields.append(dict(name=n, off=off, type=t, count=c, cls=cls, comment='layout decoded from the data'))
            off += SIZES[t] * c
        return dict(name=sname, size=off, source=None, size_check=None, fields=fields)
    raw, size = source_fields(sname)
    fields = []
    for off, code, n, cnt, cmt in raw:
        if code == 'pad':
            fields.append(dict(name=f'pad_0x{off:03x}', off=off, type='pad', count=cnt, cls='pad', comment=cmt))
        elif GAP.match(n) or (n in ('mUnknown00',) and sname in overlay.SPLITS):
            _split_gap(sname, off, SIZES[code] * cnt, fields)
        elif code in ('s8', 'u8') and cnt > 1:
            cls = _classify(n)
            fields.append(dict(name=n if cls != 'pad' else f'pad_0x{off:03x}', off=off,
                               type='pad' if cls == 'pad' else ('str' if code == 's8' else 'raw'), count=cnt,
                               cls=cls, comment=cmt))
        else:
            fields.append(dict(name=n, off=off, type=code, count=cnt, cls=_classify(n), comment=cmt))
    want, how = static_assert_size(sname)
    return dict(name=sname, size=size, source=overlay.STRUCT_SOURCES[sname], size_check=[want, how], fields=fields)


def generate():
    structs = {}
    for t in overlay.TABLES.values():
        if t['struct'] not in structs:
            structs[t['struct']] = struct_spec(t['struct'])
    for s in structs.values():   # every byte covered exactly once, names unique
        o = 0
        names = set()
        for f in s['fields']:
            assert f['off'] == o, (s['name'], f)
            assert f['name'] not in names, (s['name'], f['name'])
            names.add(f['name'])
            o += SIZES[f['type']] * f['count']
        assert o == s['size'], s['name']
    return dict(format=FORMAT_VERSION, generator='tools/eedata/specgen.py', tables=overlay.TABLES,
                row_names=overlay.ROW_NAMES, structs=structs)


def dumps(spec):
    return (json.dumps(spec, indent=1, sort_keys=False) + '\n').encode('utf-8')


def load():
    return json.loads(SPECS.read_bytes())


if __name__ == '__main__':
    SPECS.write_bytes(dumps(generate()))
    print(f'-> {SPECS}')
