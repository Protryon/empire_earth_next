"""db file <-> text: the record fields as keys/columns, the containers as document structure.

Text model (both containers): a record is a mapping key -> value. Keys: `row` (the row position, checked; empty
or absent = the next position), `label` (text-only checked alias, tables in ROW_NAMES), the lead strings, every
record field by its spec name, `<str field>~tail` (hex of the bytes after the NUL; exact mode, only when not
zero), pad fields (exact mode, only when not zero), and the trailing list (`effects`, `parents`). Arrays are YAML
flow sequences (trailing zero elements left out) or CSV columns name[i] (pairs: name[i][0], name[i][1]).
A missing key or an empty cell is zero (empty string). Rows not listed are holes: copies of the `hole` template.
"""
import re

import binfmt
import scalars
from binfmt import FormatError, Row, Table, Epoch
from scalars import ValueError_
from textio import Scalar, Text, TextError, emit_csv, emit_yaml, parse_csv, parse_yaml

MODES = ('exact', 'clean')
COLUMN = re.compile(r'^([^\[\]]+)((?:\[\d+\])*)$')


def _zero_value(f, v):
    """True when a decoded element is all zero bytes."""
    t = f['type']
    if t == 'pair_f32':
        return v == [0, 0]
    return v == 0


# ---------------------------------------------------------------- record -> text values

def _scalar_text(f, v):
    t = f['type']
    if t in ('f32',):
        return scalars.format_f32(v)
    if t == 'pair_f32':
        return [scalars.format_f32(x) for x in v]
    return scalars.format_int(v)


def record_items(fields, rec, mode, yaml):
    """[(key, value)] of a record; value: str (plain), Text (quoted), list (flow) or None (CSV: per element)."""
    out = []
    for f in fields:
        t, n, name = f['type'], f['count'], f['name']
        v = binfmt.field_value(f, rec)
        if t == 'str':
            text, tail = v
            out.append((name, Text(scalars.decode_str(text))))
            if mode == 'exact' and tail.rstrip(b'\0'):
                out.append((name + '~tail', Text(scalars.format_hex(tail))))
        elif t == 'raw':
            out.append((name, Text(scalars.format_hex(v))))
        elif t == 'pad':
            if mode == 'exact' and v.rstrip(b'\0'):
                out.append((name, Text(scalars.format_hex(v))))
        elif n == 1:
            out.append((name, _scalar_text(f, v)))
        else:
            if yaml:
                while v and _zero_value(f, v[-1]):
                    v = v[:-1]
            out.append((name, [_scalar_text(f, x) for x in v]))
    return out


def lead_items(tspec, lead):
    out = []
    i = 0
    for spec in tspec.get('lead', []):
        if '|' in spec:
            a, rest = spec.split('|')
            b, n = rest[:-1].split('[')
            n = int(n)
            out.append((a, [Text(scalars.decode_str(lead[i + 2 * k])) for k in range(n)]))
            out.append((b, [Text(scalars.decode_str(lead[i + 2 * k + 1])) for k in range(n)]))
            i += 2 * n
        elif '[' in spec:
            a, n = spec[:-1].split('[')
            out.append((a, [Text(scalars.decode_str(x)) for x in lead[i:i + int(n)]]))
            i += int(n)
        else:
            out.append((spec, Text(scalars.decode_str(lead[i]))))
            i += 1
    return out


def row_items(tspec, fields, r, mode, yaml, pos=None, label=None):
    items = []
    if pos is not None:
        items.append(('row', str(pos)))
    if label is not None:
        items.append(('label', Text(label)))
    items += lead_items(tspec, r.lead)
    items += record_items(fields, r.rec, mode, yaml)
    if 'list' in tspec:
        items.append((tspec['list'][0], [str(x) for x in r.lst]))
    return items


# ---------------------------------------------------------------- text values -> record

class _Getter:
    """A record's text mapping with the keys consumed checked off."""
    def __init__(self, m, where):
        self.m, self.where, self.used = m, where, set()

    def get(self, k):
        self.used.add(k)
        v = self.m.get(k)
        if isinstance(v, Scalar) and not v.quoted and v in ('', '~', 'null'):
            return None
        return v

    def leftover(self):
        extra = [k for k in self.m if k not in self.used]
        if extra:
            raise TextError(f'{self.where}: unknown key(s) {extra[:5]}')


def _elems(v, n, where, name):
    """An array value (YAML list, or CSV {index: value}) -> list of n elements, None = missing."""
    if v is None:
        return [None] * n
    if isinstance(v, dict):
        if any(k >= n for k in v):
            raise TextError(f'{where}: {name}[{max(v)}] is past the end ({n} elements)')
        return [v.get(k) for k in range(n)]
    if not isinstance(v, list):
        raise TextError(f'{where}: {name} must be a list')
    if len(v) > n:
        raise TextError(f'{where}: {name} has {len(v)} elements, the field has {n}')
    return list(v) + [None] * (n - len(v))


def _num(f, s, where):
    t = f['type']
    if s is None or (isinstance(s, Scalar) and not s.quoted and s in ('', '~', 'null')):
        return [0, 0] if t == 'pair_f32' else 0
    try:
        if t == 'pair_f32':
            e = _elems(s, 2, where, f['name'])
            return [_num(dict(f, type='f32'), x, where) for x in e]
        if isinstance(s, (list, dict)):
            raise TextError(f'{where}: {f["name"]}: a list where a number belongs')
        if t == 'f32':
            return scalars.parse_f32(s)
        return scalars.parse_int(s, t)
    except ValueError_ as e:
        raise TextError(f'{where}: {f["name"]}: {e}') from None


def _text(v, where, name):
    if v is None:
        return b''
    if isinstance(v, (list, dict)):
        raise TextError(f'{where}: {name}: a list where a string belongs')
    try:
        return scalars.encode_str(v)
    except ValueError_ as e:
        raise TextError(f'{where}: {name}: {e}') from None


def _hex(v, n, where, name):
    if v is None:
        return bytes(n) if n is not None else b''
    if isinstance(v, (list, dict)):
        raise TextError(f'{where}: {name}: a list where hex belongs')
    try:
        return scalars.parse_hex(v, n)
    except ValueError_ as e:
        raise TextError(f'{where}: {name}: {e}') from None


def build_record(fields, size, g):
    buf = bytearray(size)
    try:
        for f in fields:
            t, n, name = f['type'], f['count'], f['name']
            if t == 'str':
                text = _text(g.get(name), g.where, name)
                tail = _hex(g.get(name + '~tail'), None, g.where, name + '~tail')
                if len(text) >= n and tail:
                    raise TextError(f'{g.where}: {name}: a tail but no room for the NUL')
                if len(text) > n:
                    raise TextError(f'{g.where}: {name}: {len(text)} bytes, the field holds {n - 1} '
                                    f'(+ NUL)')
                if tail and len(text) + 1 + len(tail) > n:
                    raise TextError(f'{g.where}: {name}~tail: {len(tail)} bytes do not fit after the text')
                binfmt.put_field(f, buf, (text, tail))
            elif t in ('raw', 'pad'):
                binfmt.put_field(f, buf, _hex(g.get(name), n, g.where, name))
            elif n == 1:
                binfmt.put_field(f, buf, _num(f, g.get(name), g.where))
            else:
                binfmt.put_field(f, buf, [_num(f, x, g.where) for x in _elems(g.get(name), n, g.where, name)])
    except FormatError as e:
        raise TextError(f'{g.where}: {e}') from None
    return bytes(buf)


def build_lead(tspec, g):
    out = []
    for spec in tspec.get('lead', []):
        if '|' in spec:
            a, rest = spec.split('|')
            b, n = rest[:-1].split('[')
            n = int(n)
            la = [_text(x, g.where, a) for x in _elems(g.get(a), n, g.where, a)]
            lb = [_text(x, g.where, b) for x in _elems(g.get(b), n, g.where, b)]
            for k in range(n):
                out += [la[k], lb[k]]
        elif '[' in spec:
            a, n = spec[:-1].split('[')
            out += [_text(x, g.where, a) for x in _elems(g.get(a), int(n), g.where, a)]
        else:
            out.append(_text(g.get(spec), g.where, spec))
    return out


def build_list(tspec, g):
    name, typ = tspec['list']
    v = g.get(name)
    if v is None:
        return []
    if isinstance(v, dict):
        v = _elems(v, max(v) + 1 if v else 0, g.where, name)
    if not isinstance(v, list):
        raise TextError(f'{g.where}: {name} must be a list')
    try:
        return [scalars.parse_int(x, typ) for x in v]
    except (ValueError_, TypeError) as e:
        raise TextError(f'{g.where}: {name}: {e}') from None


def build_row(tspec, fields, size, m, where, table, pos, row_names):
    g = _Getter(m, where)
    g.get('row')
    label = g.get('label')
    if label is not None and label != '' and pos in row_names and label != row_names[pos]:
        raise TextError(f'{where}: label {label!r} does not match row {pos} ({row_names[pos]!r})')
    r = Row(build_record(fields, size, g), build_lead(tspec, g), build_list(tspec, g) if 'list' in tspec else [])
    g.leftover()
    return r


def _positions(records, where):
    """[(position, mapping)] in file order; `row` must increase; absent = previous + 1."""
    out, nxt = [], 0
    for k, m in enumerate(records):
        if not isinstance(m, dict):
            raise TextError(f'{where} item {k}: not a mapping')
        v = m.get('row')
        if v is None or (isinstance(v, Scalar) and not v.quoted and v in ('', '~', 'null')):
            pos = nxt
        else:
            try:
                pos = scalars.parse_int(v, 's32')
            except ValueError_ as e:
                raise TextError(f'{where} item {k}: row: {e}') from None
            if pos < nxt:
                raise TextError(f'{where} item {k}: row {pos} is not after the previous row ({nxt - 1})')
        out.append((pos, m))
        nxt = pos + 1
    return out


def _place(placed, total, where):
    n = max([total] + [p + 1 for p, _ in placed])
    rows = [None] * n
    for p, r in placed:
        rows[p] = r
    return rows


# ---------------------------------------------------------------- export

def _row_names(spec, name):
    return {int(k): v for k, v in spec.get('row_names', {}).get(name, {}).items()}


def export(name, data, spec, mode='exact'):
    """db file bytes -> text (UTF-8 bytes, LF)."""
    if mode not in MODES:
        raise ValueError(mode)
    tspec = spec['tables'][name]
    st = spec['structs'][tspec['struct']]
    fields = st['fields']
    t = binfmt.detect_holes(binfmt.parse(name, data, spec), spec)
    if mode == 'clean':
        binfmt.clean_table(t, spec)
    names = _row_names(spec, name)
    head = [('eedata', name), ('format', str(spec['format'])), ('mode', mode)]
    if t.header is not None:
        head.append(('header', str(t.header)))
    if t.epochs is None:
        head.append(('rows', str(len(t.rows))))
    if t.hole is not None:
        head.append(('hole', Text(scalars.format_hex(t.hole, trim=False))))
    yaml = tspec['format'] == 'yaml'
    if yaml:
        doc = dict(head)
        if t.epochs is not None:
            doc['epochs'] = [{'slots': str(len(e.nodes)),
                              'epoch': dict(row_items(tspec, fields, e.epoch, mode, True)),
                              'nodes': [dict(row_items(tspec, fields, r, mode, True, pos=i))
                                        for i, r in enumerate(e.nodes) if r is not None]}
                             for e in t.epochs]
        else:
            doc['records'] = [dict(row_items(tspec, fields, r, mode, True, pos=i,
                                             label=names.get(i) if names else None))
                              for i, r in enumerate(t.rows) if r is not None]
        banner = (f'# {name}: Empire Earth: The Art of Conquest db table (tools/eedata; record layout '
                  f'{tspec["struct"]}, {st["size"]} bytes, in tools/eedata/specs.json)\n')
        return (banner + emit_yaml(doc)).encode('utf-8')
    # CSV: flatten every row, then the union of the columns in a fixed order
    flat_rows = []
    for i, r in enumerate(t.rows):
        if r is None:
            continue
        flat = {}
        for k, v in row_items(tspec, fields, r, mode, False, pos=i, label=names.get(i) if names else None):
            _flatten(k, v, flat)
        flat_rows.append(flat)
    cols = csv_columns(tspec, fields, mode, names)
    present = set(k for fr in flat_rows for k in fr)
    optional = set(c for c in cols if c.endswith('~tail') or c.startswith('pad_0x'))
    cols = [c for c in cols if c not in optional or c in present]
    extra = present - set(cols)
    assert not extra, extra
    directives = [(k, v) for k, v in head]
    return emit_csv(directives, cols, [[fr.get(c, '') for c in cols] for fr in flat_rows]).encode('utf-8')


def _flatten(k, v, out):
    if isinstance(v, list):
        for i, x in enumerate(v):
            _flatten(f'{k}[{i}]', x, out)
    else:
        out[k] = str(v)


def csv_columns(tspec, fields, mode, names):
    cols = ['row'] + (['label'] if names else [])
    for spec in tspec.get('lead', []):
        if '|' in spec:
            a, rest = spec.split('|')
            b, n = rest[:-1].split('[')
            cols += [f'{a}[{k}]' for k in range(int(n))] + [f'{b}[{k}]' for k in range(int(n))]
        elif '[' in spec:
            a, n = spec[:-1].split('[')
            cols += [f'{a}[{k}]' for k in range(int(n))]
        else:
            cols.append(spec)
    for f in fields:
        t, n, name = f['type'], f['count'], f['name']
        if t == 'str':
            cols += [name, name + '~tail']
        elif t in ('raw', 'pad') or n == 1:
            cols.append(name)
        elif t == 'pair_f32':
            cols += [f'{name}[{k}][{j}]' for k in range(n) for j in range(2)]
        else:
            cols += [f'{name}[{k}]' for k in range(n)]
    return cols


# ---------------------------------------------------------------- import

def _head_int(v, what, where):
    try:
        return scalars.parse_int(v, 'u32')
    except ValueError_ as e:
        raise TextError(f'{where}: {what}: {e}') from None


def import_(name, text, spec):
    """text (bytes) -> db file bytes. Raises TextError with the place of the first problem."""
    tspec = spec['tables'][name]
    st = spec['structs'][tspec['struct']]
    fields, size = st['fields'], st['size']
    names = _row_names(spec, name)
    try:
        s = text.decode('utf-8')
    except UnicodeDecodeError as e:
        raise TextError(f'{name}: not UTF-8 ({e})') from None
    t = Table(name)
    if tspec['format'] == 'yaml':
        doc = parse_yaml(s)
        if not isinstance(doc, dict):
            raise TextError(f'{name}: the document is not a mapping')
        head = doc
        allowed = {'eedata', 'format', 'mode', 'rows', 'hole', 'header', 'records', 'epochs'}
        bad = [k for k in doc if k not in allowed]
        if bad:
            raise TextError(f'{name}: unknown top-level key(s) {bad}')
    else:
        head, header, body, line0 = parse_csv(s)
        allowed = {'eedata', 'format', 'mode', 'rows', 'hole', 'header'}
        bad = [k for k in head if k not in allowed]
        if bad:
            raise TextError(f'{name}: unknown directive(s) {bad}')
        known = set(csv_columns(tspec, fields, 'exact', names))
        cols = []
        for c in header:
            m = COLUMN.match(c)
            if c not in known or not m:
                raise TextError(f'{name}: unknown column {c!r}')
            cols.append((m.group(1), [int(x) for x in re.findall(r'\d+', m.group(2))]))
        records = []
        for r in body:
            m = {}
            for (base, idx), cell in zip(cols, r):
                if cell == '':
                    continue
                if not idx:
                    m[base] = Scalar(cell, True)
                    continue
                node = m.setdefault(base, {})
                for j in idx[:-1]:
                    node = node.setdefault(j, {})
                node[idx[-1]] = Scalar(cell, True)
            records.append(m)
    if str(head.get('eedata', name)) != name:
        raise TextError(f'{name}: the file says it is {head.get("eedata")!r}')
    if 'format' in head and str(head['format']) != str(spec['format']):
        raise TextError(f'{name}: format {head["format"]}, this tool writes format {spec["format"]}')
    hole = head.get('hole')
    t.hole = _hex(hole, size, name, 'hole') if hole not in (None, '') else None
    if t.hole is not None and len(scalars.parse_hex(hole)) != size:
        raise TextError(f'{name}: the hole template has {len(scalars.parse_hex(hole))} bytes, the record {size}')
    if tspec['container'] == 'header':
        if 'header' not in head:
            raise TextError(f'{name}: the header word is missing')
        t.header = _head_int(head['header'], 'header', name)
    total = _head_int(head['rows'], 'rows', name) if head.get('rows') not in (None, '') else 0
    if tspec['container'] == 'techtree':
        eps = doc.get('epochs')
        if not isinstance(eps, list):
            raise TextError(f'{name}: epochs must be a list')
        t.epochs = []
        for k, e in enumerate(eps):
            where = f'{name} epoch {k}'
            if not isinstance(e, dict) or not isinstance(e.get('epoch'), dict):
                raise TextError(f'{where}: needs an epoch: mapping')
            bad = [x for x in e if x not in ('slots', 'epoch', 'nodes')]
            if bad:
                raise TextError(f'{where}: unknown key(s) {bad}')
            ep = build_row(tspec, fields, size, e['epoch'], where, name, None, {})
            nodes = e.get('nodes') or []
            if not isinstance(nodes, list):
                raise TextError(f'{where}: nodes must be a list')
            placed = [(p, build_row(tspec, fields, size, m, f'{where} node row {p}', name, p, {}))
                      for p, m in _positions(nodes, where)]
            slots = _head_int(e['slots'], 'slots', where) if e.get('slots') not in (None, '') else 0
            rows = _place(placed, slots, where)
            if t.hole is None and any(r is None for r in rows):
                raise TextError(f'{where}: node rows missing but no hole template')
            t.epochs.append(Epoch(ep, rows))
        return binfmt.serialize(t, spec)
    if tspec['format'] == 'yaml':
        records = doc.get('records') or []
        if not isinstance(records, list):
            raise TextError(f'{name}: records must be a list')
    placed = [(p, build_row(tspec, fields, size, m, f'{name} row {p}', name, p, names))
              for p, m in _positions(records, name)]
    t.rows = _place(placed, total, name)
    if t.hole is None and any(r is None for r in t.rows):
        raise TextError(f'{name}: rows missing ({t.rows.index(None)}...) but no hole template')
    return binfmt.serialize(t, spec)
