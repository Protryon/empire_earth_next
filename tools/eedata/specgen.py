"""Generate the table specs (specs.json) from the record structs in the sources plus overlay.py.

Each struct is parsed from its source file (STRUCT_SOURCES) with MSVC x86 natural alignment; its size is checked
against the source's static_assert (or COMMENT_SIZES). Every byte of a record gets a field of one class:

  named   a member the game reads: exported to the text under its source name
  label   the row's name where the game never reads it (a member called mLabel): exported as the text-only
          `label` (written back to the same bytes, so a person can tell the rows apart)
  unused  a member the game never reads (mUnused*, the source's _gap/mUndecoded gaps): not in the text; the
          importer writes overlay.DEFAULTS or zero
  pad     alignment padding: not in the text, zero

Which members the game reads is decided by the sources (tools/eedata/census.py lists every read of every record
member); an exported field must have a real name, never an offset name (checked here).

Field types: s8 u8 s16 u16 s32 u32 f32 bool (one byte), str (char[N], NUL-terminated text), raw (bytes; unused
only), pad, pair_f32 (two floats). count > 1 makes an array (str: N).
"""
import json
import pathlib
import re
import struct

import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import overlay  # noqa: E402

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SPECS = HERE / 'specs.json'
FORMAT_VERSION = 3

TYPES = {'char': ('s8', 1), 'signed char': ('s8', 1), 'unsigned char': ('u8', 1), 'bool': ('bool', 1),
         'short': ('s16', 2), 'unsigned short': ('u16', 2), 'long': ('s32', 4), 'unsigned long': ('u32', 4),
         'int': ('s32', 4), 'unsigned int': ('u32', 4), 'float': ('f32', 4), 'EECalamityKind': ('s32', 4),
         'int32': ('s32', 4), 'int32_t': ('s32', 4), 'uint32_t': ('u32', 4), 'int16_t': ('s16', 2), 'uint16_t': ('u16', 2), 'uint8_t': ('u8', 1), 'int8_t': ('s8', 1), 'EECivilizationUpgradeValue': ('pair_f32', 8), 'WORD': ('u16', 2),
         'DWORD': ('u32', 4), 'BYTE': ('u8', 1)}
SIZES = {'s8': 1, 'u8': 1, 'bool': 1, 's16': 2, 'u16': 2, 's32': 4, 'u32': 4, 'f32': 4, 'pair_f32': 8, 'str': 1,
         'raw': 1, 'pad': 1}
EXPORTED = ('named', 'label')
FIELD = re.compile(r'^\s*(?:volatile\s+)?((?:unsigned\s+|signed\s+)?\w+)\s+(\w+)\s*(\[[^\]]+\])?\s*;\s*(?://\s*(.*))?$')
GAP = re.compile(r'^(_gap\w*|_u[0-9A-Fa-f]+|mUndecoded\w*)$')
PADNAME = re.compile(r'^(mPad\w*|_pad\w*)$')
# offset-only or placeholder names: never allowed for a field the text carries
OFFSET_NAME = re.compile(r'^(m[LUCBF][0-9A-Fa-f]{2,3}|mU[0-9a-f]{3}|mUnknown\w*|mBoolPlaceholder\d|u_0x\w+|_\w+)$')


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


def _field(name, off, typ, count, cls, comment, member=None):
    return dict(name=name, member=member or name, off=off, type=typ, count=count, cls=cls, comment=comment)


def _split_gap(sname, gname, off, size, out):
    """A source gap: bytes nothing reads."""
    out.append(_field(gname, off, 'raw', size, 'unused', 'unread gap'))


def _classify(sname, n):
    if PADNAME.match(n):
        return 'pad'
    if n == 'mLabel':
        return 'label'
    if n.startswith('mUnused'):
        return 'unused'
    if OFFSET_NAME.match(n):
        raise SystemExit(f'{sname}.{n}: an offset-only name for a field the game reads (name it from its readers, '
                         f'or mUnused* when nothing reads it)')
    return 'named'


def struct_spec(sname):
    if sname in overlay.VIRTUAL_STRUCTS:
        fields, off = [], 0
        for n, t, c, cls in overlay.VIRTUAL_STRUCTS[sname]:
            fields.append(_field('label' if cls == 'label' else n, off, t, c, cls, 'layout decoded from the data', n))
            off += SIZES[t] * c
        return dict(name=sname, size=off, source=None, size_check=None, fields=fields, defaults={})
    raw, size = source_fields(sname)
    fields = []
    for off, code, n, cnt, cmt in raw:
        if code == 'pad':
            fields.append(_field(f'pad_0x{off:03x}', off, 'pad', cnt, 'pad', cmt))
        elif GAP.match(n):
            _split_gap(sname, n, off, SIZES[code] * cnt, fields)
        else:
            cls = _classify(sname, n)
            if code in ('s8', 'u8') and cnt > 1:
                code = 'str' if code == 's8' and cls in EXPORTED else 'raw'
            if cls == 'pad':
                code = 'pad'
            if code == 'raw' and cls in EXPORTED:
                raise SystemExit(f'{sname}.{n}: a byte array the game reads (declare it with its element type)')
            if cls == 'unused' and code != 'raw':   # an unread member: its bytes only (the type is not the text's)
                cnt, code = cnt * SIZES[code], 'raw'
            fields.append(_field('label' if cls == 'label' else n, off, code, cnt, cls, cmt, n))
    want, how = static_assert_size(sname)
    defaults = {}
    for member, v in overlay.DEFAULTS.get(sname, {}).items():
        f = next((f for f in fields if f['member'] == member), None)
        if f is None:
            raise SystemExit(f'overlay.DEFAULTS: {sname}.{member} is not a member')
        defaults[f['name']] = v
    return dict(name=sname, size=size, source=overlay.STRUCT_SOURCES[sname], size_check=[want, how], fields=fields,
                defaults=defaults)


def _bits(f, v):
    """A spec value (int, float, list) -> the field's stored integer(s) (f32 as bits)."""
    if isinstance(v, list):
        return [_bits(f, x) for x in v]
    if f['type'] == 'str':
        return str(v)
    if f['type'] == 'f32':
        return struct.unpack('<I', struct.pack('<f', float(v)))[0]
    return int(v)


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
    tables = {}
    for name, t in overlay.TABLES.items():
        t = dict(t)
        st = structs[t['struct']]
        byname = {f['name']: f for f in st['fields']}
        hole = {}
        for k, v in overlay.HOLES.get(name, {}).items():
            if k not in byname or byname[k]['cls'] not in EXPORTED:
                raise SystemExit(f'overlay.HOLES: {name}.{k} is not a field the text carries')
            hole[k] = _bits(byname[k], v)
        t['hole'] = hole
        if name in overlay.ROW_FIELDS:
            t['row_fields'] = {str(k): v for k, v in overlay.ROW_FIELDS[name].items()}
        t['key'] = overlay.KEYS.get(name, 'row')
        if name in overlay.DERIVED:
            t['derived'] = {k: list(v) for k, v in overlay.DERIVED[name].items()}
        if name in overlay.EPOCH_FLAGS:
            fn, key = overlay.EPOCH_FLAGS[name]
            if fn not in byname or byname[fn]['cls'] != 'named' or byname[fn]['type'] != 'bool':
                raise SystemExit(f'overlay.EPOCH_FLAGS: {name}.{fn} is not a bool array the game reads')
            t['epoch_flags'] = [fn, key]
        if t['key'] not in ('row', 'position') and (t['key'] not in byname or byname[t['key']]['cls'] != 'named'):
            raise SystemExit(f'overlay.KEYS: {name}.{t["key"]} is not a field the text carries')
        if name in overlay.SELF:
            fn, off = overlay.SELF[name]
            if fn not in byname or byname[fn]['cls'] != 'named' or byname[fn]['count'] != 1:
                raise SystemExit(f'overlay.SELF: {name}.{fn} is not a scalar field the text carries')
            t['self'] = [fn, off]
        refs = {}
        for fn, r in overlay.REFS.get(name, {}).items():
            r = r if isinstance(r, dict) else {'table': r}
            if r['table'] not in overlay.TABLES:
                raise SystemExit(f'overlay.REFS: {name}.{fn} -> {r["table"]}: not a table')
            if fn == '@list':
                if 'list' not in t:
                    raise SystemExit(f'overlay.REFS: {name} has no list')
            elif fn not in byname or byname[fn]['cls'] != 'named' or byname[fn]['type'] in ('f32', 'pair_f32', 'str'):
                raise SystemExit(f'overlay.REFS: {name}.{fn} is not an integer field the text carries')
            refs[fn] = {'table': r['table'], 'none': list(r.get('none', []))}
            if r.get('never'):
                refs[fn]['never'] = True
        t['refs'] = refs
        t['id_names'] = overlay.ID_NAMES.get(name, overlay.ID_DEFAULT)
        tables[name] = t
    return dict(format=FORMAT_VERSION, generator='tools/eedata/specgen.py', tables=tables,
                row_names={t: {str(k): v for k, v in r.items()} for t, r in overlay.ROW_NAMES.items()},
                structs=structs)


def dumps(spec):
    return (json.dumps(spec, indent=1, sort_keys=False) + '\n').encode('utf-8')


def load():
    return json.loads(SPECS.read_bytes())


if __name__ == '__main__':
    SPECS.write_bytes(dumps(generate()))
    print(f'-> {SPECS}')
