"""db file <-> text: the fields the game reads as keys/columns, the containers as document structure.

Text model (both containers): a record is a mapping key -> value. Keys: `row` (the row position, checked; empty or
absent = the next position), `label` (the row's name: text only, the game never reads it; dbworld's labels are the
constants' names and are checked), the lead strings, every field the game reads by its spec name, and the trailing
list (`effects`, `parents`). Arrays are YAML flow sequences (trailing zero elements left out) or CSV columns name[i]
(pairs: name[i][0], name[i][1]). A missing key or an empty cell is zero (empty string). Fields the game never reads
are not in the text; the importer writes the struct's defaults (zero unless specs.json says otherwise). Rows not
listed are holes (empty database rows): the importer writes the table's hole record there.
"""
import re

import binfmt
import scalars
from binfmt import FormatError, Row, Table, Epoch
from scalars import ValueError_
from textio import Scalar, Text, TextError, emit_csv, emit_yaml, parse_csv, parse_yaml

COLUMN = re.compile(r'^([^\[\]]+)((?:\[\d+\])*)$')


def _zero_value(f, v):
    """True when a decoded element is all zero bytes."""
    return v == [0, 0] if f['type'] == 'pair_f32' else v == 0


# ---------------------------------------------------------------- record -> text values

def _scalar_text(f, v):
    t = f['type']
    if t == 'f32':
        return scalars.format_f32(v)
    if t == 'pair_f32':
        return [scalars.format_f32(x) for x in v]
    return scalars.format_int(v)


def record_items(fields, rec, yaml, only=None):
    """[(key, value)] of a record's exported fields; value: str (plain), Text (quoted) or list (flow).
    only: the field names to write (dbworld: the values its row's readers use); the others are left out."""
    out = []
    for f in binfmt.exported(fields):
        t, n, name = f['type'], f['count'], f['name']
        if only is not None and name not in only:
            continue
        v = binfmt.field_value(f, rec)
        if t == 'str':
            out.append((name, Text(scalars.decode_str(v[0][:n - 1]))))
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


def row_items(tspec, fields, r, yaml, pos=None, label=None, only=None):
    items = []
    if pos is not None:
        items.append(('row', str(pos)))
    if label is not None:
        items.append(('label', Text(label)))
    items += lead_items(tspec, r.lead)
    items += record_items(fields, r.rec, yaml, only)
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


def build_record(fields, base, g):
    """The record bytes: base (the struct's defaults) with every exported field set from the text (missing = 0)."""
    buf = bytearray(base)
    try:
        for f in binfmt.exported(fields):
            t, n, name = f['type'], f['count'], f['name']
            if t == 'str':
                text = _text(g.get(name), g.where, name)
                if len(text) >= n:
                    raise TextError(f'{g.where}: {name}: {len(text)} bytes, the field holds {n - 1} (+ NUL)')
                buf[f['off']:f['off'] + n] = bytes(n)
                binfmt.put_field(f, buf, (text, b''))
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
    if any(isinstance(x, (list, dict)) for x in v):
        raise TextError(f'{g.where}: {name}: a list where a number belongs')
    try:
        return [scalars.parse_int(x, typ) for x in v]
    except ValueError_ as e:
        raise TextError(f'{g.where}: {name}: {e}') from None


def build_row(tspec, fields, base, m, where, pos, row_names):
    g = _Getter(m, where)
    g.get('row')
    if row_names:   # the label is the constant's name: checked
        label = g.get('label')
        if label is not None and label != '' and pos in row_names and label != row_names[pos]:
            raise TextError(f'{where}: label {label!r} does not match row {pos} ({row_names[pos]!r})')
    r = Row(build_record(fields, base, g), build_lead(tspec, g), build_list(tspec, g) if 'list' in tspec else [])
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


def _place(placed, total):
    n = max([total] + [p + 1 for p, _ in placed])
    rows = [None] * n
    for p, r in placed:
        rows[p] = r
    return rows


# ---------------------------------------------------------------- export

def _row_names(spec, name):
    return {int(k): v for k, v in spec.get('row_names', {}).get(name, {}).items()}


def _row_fields(tspec):
    return {int(k): set(v) for k, v in tspec.get('row_fields', {}).items()}


def export(name, data, spec):
    """db file bytes -> text (UTF-8 bytes, LF)."""
    tspec = spec['tables'][name]
    st = spec['structs'][tspec['struct']]
    fields = st['fields']
    t = binfmt.detect_holes(binfmt.parse(name, data, spec), spec)
    names = _row_names(spec, name)
    only = _row_fields(tspec)
    if names:   # rows without a name have no reader: left out (holes)
        t.rows = [r if i in names else None for i, r in enumerate(t.rows)]
    head = [('eedata', name), ('format', str(spec['format']))]
    if t.epochs is None:
        head.append(('rows', str(len(t.rows))))
    yaml = tspec['format'] == 'yaml'
    if yaml:
        doc = dict(head)
        if t.epochs is not None:
            doc['epochs'] = [{'slots': str(len(e.nodes)),
                              'epoch': dict(row_items(tspec, fields, e.epoch, True)),
                              'nodes': [dict(row_items(tspec, fields, r, True, pos=i))
                                        for i, r in enumerate(e.nodes) if r is not None]}
                             for e in t.epochs]
        else:
            doc['records'] = [dict(row_items(tspec, fields, r, True, pos=i, label=names.get(i)))
                              for i, r in enumerate(t.rows) if r is not None]
        banner = (f'# {name}: Empire Earth: The Art of Conquest db table (tools/eedata; record layout '
                  f'{tspec["struct"]}, {st["size"]} bytes, in tools/eedata/specs.json)\n')
        return (banner + emit_yaml(doc)).encode('utf-8')
    flat_rows = []
    for i, r in enumerate(t.rows):
        if r is None:
            continue
        flat = {}
        for k, v in row_items(tspec, fields, r, False, pos=i, label=names.get(i), only=only.get(i)):
            _flatten(k, v, flat)
        flat_rows.append(flat)
    cols = csv_columns(tspec, fields, names)
    return emit_csv(head, cols, [[fr.get(c, '') for c in cols] for fr in flat_rows]).encode('utf-8')


def _flatten(k, v, out):
    if isinstance(v, list):
        for i, x in enumerate(v):
            _flatten(f'{k}[{i}]', x, out)
    else:
        out[k] = str(v)


def csv_columns(tspec, fields, names):
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
    for f in binfmt.exported(fields):
        t, n, name = f['type'], f['count'], f['name']
        if t == 'str' or n == 1:
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
    fields = st['fields']
    base = binfmt.base_record(st)
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
        bad = [k for k in doc if k not in ('eedata', 'format', 'rows', 'records', 'epochs')]
        if bad:
            raise TextError(f'{name}: unknown top-level key(s) {bad}')
    else:
        head, header, body, line0 = parse_csv(s)
        bad = [k for k in head if k not in ('eedata', 'format', 'rows')]
        if bad:
            raise TextError(f'{name}: unknown directive(s) {bad}')
        known = set(csv_columns(tspec, fields, names))
        cols = []
        for c in header:
            m = COLUMN.match(c)
            if c not in known or not m:
                raise TextError(f'{name}: unknown column {c!r}')
            cols.append((m.group(1), [int(x) for x in re.findall(r'\d+', m.group(2))]))
        records = []
        for r in body:
            m = {}
            for (base_, idx), cell in zip(cols, r):
                if cell == '':
                    continue
                if not idx:
                    m[base_] = Scalar(cell, True)
                    continue
                node = m.setdefault(base_, {})
                for j in idx[:-1]:
                    node = node.setdefault(j, {})
                node[idx[-1]] = Scalar(cell, True)
            records.append(m)
    if str(head.get('eedata', name)) != name:
        raise TextError(f'{name}: the file says it is {head.get("eedata")!r}')
    if 'format' in head and str(head['format']) != str(spec['format']):
        raise TextError(f'{name}: format {head["format"]}, this tool reads format {spec["format"]}')
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
            ep = build_row(tspec, fields, base, e['epoch'], where, None, {})
            nodes = e.get('nodes') or []
            if not isinstance(nodes, list):
                raise TextError(f'{where}: nodes must be a list')
            placed = [(p, build_row(tspec, fields, base, m, f'{where} node row {p}', p, {}))
                      for p, m in _positions(nodes, where)]
            slots = _head_int(e['slots'], 'slots', where) if e.get('slots') not in (None, '') else 0
            t.epochs.append(Epoch(ep, _place(placed, slots)))
        return binfmt.serialize(t, spec)
    if tspec['format'] == 'yaml':
        records = doc.get('records') or []
        if not isinstance(records, list):
            raise TextError(f'{name}: records must be a list')
    placed = [(p, build_row(tspec, fields, base, m, f'{name} row {p}', p, names))
              for p, m in _positions(records, name)]
    t.rows = _place(placed, total)
    return binfmt.serialize(t, spec)
