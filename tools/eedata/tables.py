"""db file <-> text: the fields the game reads as keys/columns, the containers as document structure.

Text model (both containers, format 3): a record is a mapping key -> value. Keys: `row` (the row position: the
row's runtime integer, its key; absent = the next free position above every listed row and the `rows` count),
`id` (the row's identity: unique in the table, lowercase ASCII, ids.ID_RE; required), `label` (display text only,
the game never reads it), the lead strings, every field the game reads by its spec name, and the trailing list
(`effects`, `parents`). Arrays are YAML flow sequences (trailing zero elements left out) or CSV columns name[i]
(pairs: name[i][0], name[i][1]). A missing key or an empty cell is zero (empty string).

References (specs.json tables[t].refs): a field (or the list) that holds a row of another table holds that row's
id; an integer stays only where it names no listed row of the target, or is one of the field's "none" values. On
import every id is resolved to the target row's key (its row position; dbtechtree: its mID); an unknown id is an
error. The table's self field (specs.json tables[t].self: mUnitType, mID ...) holds the row's own key: left out
when equal, written by the importer.

Fields the game never reads are not in the text; the importer writes the struct's defaults (zero unless specs.json
says otherwise). Rows not listed are holes (empty database rows): the importer writes the table's hole record.

Content is the single place where tables meet: it holds every table's text, assigns each row its key, indexes the
ids and resolves the references. A later mod layer feeds it merged entries instead of files.
"""
import re

import binfmt
import ids as idsmod
import scalars
from binfmt import FormatError, Row, Table, Epoch
from scalars import ValueError_
from textio import Scalar, Text, TextError, emit_csv, emit_yaml, parse_csv, parse_yaml

COLUMN = re.compile(r'^([^\[\]]+)((?:\[\d+\])*)$')


def _zero_value(f, v):
    """True when a decoded element is all zero bytes."""
    return v == [0, 0] if f['type'] == 'pair_f32' else v == 0


def _missing(v):
    return v is None or (isinstance(v, Scalar) and not v.quoted and v in ('', '~', 'null'))


# ---------------------------------------------------------------- record -> text values

def _scalar_text(f, v):
    t = f['type']
    if t == 'f32':
        return scalars.format_f32(v)
    if t == 'pair_f32':
        return [scalars.format_f32(x) for x in v]
    return scalars.format_int(v)


def _ref_text(v, ref, ids):
    """An integer of a reference field -> the target row's id, or the integer when it names no row."""
    if v in ref['none']:
        return scalars.format_int(v)
    i = ids.get(ref['table'], {}).get(v)
    return i if i is not None else scalars.format_int(v)


def record_items(tspec, fields, rec, yaml, ids, key, only=None):
    """[(key, value)] of a record's exported fields; value: str (plain), Text (quoted) or list (flow).
    only: the field names to write (dbworld: the values its row's readers use); the others are left out."""
    out = []
    refs = tspec.get('refs', {})
    self_ = tspec.get('self')
    for f in binfmt.exported(fields):
        t, n, name = f['type'], f['count'], f['name']
        if only is not None and name not in only:
            continue
        v = binfmt.field_value(f, rec)
        ref = refs.get(name)
        if self_ and name == self_[0] and key is not None and v == key + self_[1]:
            continue
        if t == 'str':
            out.append((name, Text(scalars.decode_str(v[0][:n - 1]))))
        elif n == 1:
            out.append((name, _ref_text(v, ref, ids) if ref else _scalar_text(f, v)))
        else:
            if yaml:
                while v and _zero_value(f, v[-1]):
                    v = v[:-1]
            out.append((name, [_ref_text(x, ref, ids) if ref else _scalar_text(f, x) for x in v]))
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


def row_items(tspec, fields, r, yaml, ids, key, rid, pos=None, only=None):
    items = []
    if pos is not None:
        items.append(('row', str(pos)))
    items.append(('id', rid))
    items += lead_items(tspec, r.lead)
    items += record_items(tspec, fields, r.rec, yaml, ids, key, only)
    if 'list' in tspec:
        ref = tspec.get('refs', {}).get('@list')
        items.append((tspec['list'][0], [_ref_text(x, ref, ids) if ref else str(x) for x in r.lst]))
    return items


# ---------------------------------------------------------------- text values -> record

class _Getter:
    """A record's text mapping with the keys consumed checked off."""
    def __init__(self, m, where):
        self.m, self.where, self.used = m, where, set()

    def get(self, k):
        self.used.add(k)
        v = self.m.get(k)
        if _missing(v):
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


def _num(f, s, where, ref=None, content=None):
    t = f['type']
    if _missing(s):
        return [0, 0] if t == 'pair_f32' else 0
    try:
        if t == 'pair_f32':
            e = _elems(s, 2, where, f['name'])
            return [_num(dict(f, type='f32'), x, where) for x in e]
        if isinstance(s, (list, dict)):
            raise TextError(f'{where}: {f["name"]}: a list where a number belongs')
        if t == 'f32':
            return scalars.parse_f32(s)
        if ref is not None:
            return content.resolve(ref, s, t, where, f['name'])
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


def build_record(tspec, fields, base, g, content, key):
    """The record bytes: base (the struct's defaults) with every exported field set from the text (missing = 0)."""
    buf = bytearray(base)
    refs = tspec.get('refs', {})
    self_ = tspec.get('self')
    try:
        for f in binfmt.exported(fields):
            t, n, name = f['type'], f['count'], f['name']
            ref = refs.get(name)
            if t == 'str':
                text = _text(g.get(name), g.where, name)
                if len(text) >= n:
                    raise TextError(f'{g.where}: {name}: {len(text)} bytes, the field holds {n - 1} (+ NUL)')
                buf[f['off']:f['off'] + n] = bytes(n)
                binfmt.put_field(f, buf, (text, b''))
            elif n == 1:
                v = g.get(name)
                if v is None and self_ and name == self_[0] and key is not None:
                    binfmt.put_field(f, buf, key + self_[1])
                else:
                    binfmt.put_field(f, buf, _num(f, v, g.where, ref, content))
            else:
                binfmt.put_field(f, buf, [_num(f, x, g.where, ref, content)
                                          for x in _elems(g.get(name), n, g.where, name)])
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


def build_list(tspec, g, content):
    name, typ = tspec['list']
    ref = tspec.get('refs', {}).get('@list')
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
        if ref is not None:
            return [content.resolve(ref, x, typ, g.where, name) for x in v]
        return [scalars.parse_int(x, typ) for x in v]
    except ValueError_ as e:
        raise TextError(f'{g.where}: {name}: {e}') from None


def build_row(tspec, fields, base, m, where, key, content):
    g = _Getter(m, where)
    g.get('row')
    g.get('id')
    r = Row(build_record(tspec, fields, base, g, content, key), build_lead(tspec, g),
            build_list(tspec, g, content) if 'list' in tspec else [])
    g.leftover()
    return r


def _line(m):
    """The line a record mapping starts on (its first scalar's)."""
    for v in m.values():
        if isinstance(v, Scalar):
            return v.line
        if isinstance(v, list) and v and isinstance(v[0], Scalar):
            return v[0].line
    return 0


def _positions(records, total, where, lines=None):
    """[(position, mapping, line)] in file order. An explicit `row` must be after the previous explicit one; a
    record without one takes the next free position above every explicit row and total (new rows go above)."""
    explicit, nxt = [], 0
    for k, m in enumerate(records):
        if not isinstance(m, dict):
            raise TextError(f'{where} item {k}: not a mapping')
        line = lines[k] if lines else _line(m)
        v = m.get('row')
        if _missing(v):
            explicit.append(None)
            continue
        try:
            pos = scalars.parse_int(v, 's32')
        except ValueError_ as e:
            raise TextError(f'{where} line {line}: row: {e}') from None
        if pos < nxt:
            raise TextError(f'{where} line {line}: row {pos} is not after the previous row ({nxt - 1})')
        explicit.append(pos)
        nxt = pos + 1
    out, free = [], max(total, nxt)
    for k, m in enumerate(records):
        pos = explicit[k]
        if pos is None:
            pos, free = free, free + 1
        out.append((pos, m, lines[k] if lines else _line(m)))
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


def parse_all(dbs, spec):
    """{table: db file bytes} -> {table: Table} with the holes detected (and dbworld's unread rows left out)."""
    out = {}
    for name, data in dbs.items():
        t = binfmt.detect_holes(binfmt.parse(name, data, spec), spec)
        names = _row_names(spec, name)
        if names:   # rows without a name have no reader: left out (holes)
            t.rows = [r if i in names else None for i, r in enumerate(t.rows)]
        out[name] = t
    return out


def export_all(dbs, spec, ids=None):
    """{table: db file bytes} -> {table: text bytes}. ids: {table: {key: id}} (default: generated, ids.py)."""
    tabs = parse_all(dbs, spec)
    ids = ids if ids is not None else idsmod.generate(tabs, spec)
    return {name: export_table(name, tabs[name], spec, ids) for name in dbs}


def export_table(name, t, spec, ids):
    """A parsed table -> text (UTF-8 bytes, LF)."""
    tspec = spec['tables'][name]
    st = spec['structs'][tspec['struct']]
    fields = st['fields']
    mine = ids.get(name, {})
    only = _row_fields(tspec)
    head = [('eedata', name), ('format', str(spec['format']))]
    if t.epochs is None:
        head.append(('rows', str(len(t.rows))))
    yaml = tspec['format'] == 'yaml'
    keyf = None if tspec['key'] == 'row' else next(f for f in fields if f['name'] == tspec['key'])

    def rid(r, pos):
        return mine[pos if keyf is None else binfmt.field_value(keyf, r.rec)]

    if yaml:
        doc = dict(head)
        if t.epochs is not None:
            doc['epochs'] = [{'slots': str(len(e.nodes)),
                              'epoch': dict(row_items(tspec, fields, e.epoch, True, ids, None, rid(e.epoch, None))),
                              'nodes': [dict(row_items(tspec, fields, r, True, ids, None, rid(r, i), pos=i))
                                        for i, r in enumerate(e.nodes) if r is not None]}
                             for e in t.epochs]
        else:
            doc['records'] = [dict(row_items(tspec, fields, r, True, ids, i, rid(r, i), pos=i))
                              for i, r in enumerate(t.rows) if r is not None]
        banner = (f'# {name}: Empire Earth: The Art of Conquest db table (tools/eedata; record layout '
                  f'{tspec["struct"]}, {st["size"]} bytes, in tools/eedata/specs.json)\n')
        return (banner + emit_yaml(doc)).encode('utf-8')
    flat_rows = []
    for i, r in enumerate(t.rows):
        if r is None:
            continue
        flat = {}
        for k, v in row_items(tspec, fields, r, False, ids, i, rid(r, i), pos=i, only=only.get(i)):
            _flatten(k, v, flat)
        flat_rows.append(flat)
    cols = csv_columns(tspec, fields)
    return emit_csv(head, cols, [[fr.get(c, '') for c in cols] for fr in flat_rows]).encode('utf-8')


def export(name, data, spec, dbs=None):
    """One table's text; dbs: the other tables' db file bytes (a reference to a table not given stays an
    integer: such a text is for reading, it does not import)."""
    dbs = dict(dbs or {})
    dbs[name] = data
    tabs = parse_all(dbs, spec)
    return export_table(name, tabs[name], spec, idsmod.generate(tabs, spec))


def _flatten(k, v, out):
    if isinstance(v, list):
        for i, x in enumerate(v):
            _flatten(f'{k}[{i}]', x, out)
    else:
        out[k] = str(v)


def csv_columns(tspec, fields):
    cols = ['row', 'id']
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


class _Doc:
    """A parsed table text: rows [(pos, mapping, line)] or epochs [(epoch mapping, line, slots, rows)]."""
    def __init__(self):
        self.rows, self.epochs, self.total = None, None, 0


class Content:
    """Every table's text; parses each once, assigns the keys, indexes the ids and resolves the references.
    texts: {table: text bytes}. The single place a later mod merge feeds (merged entries instead of files)."""

    def __init__(self, texts, spec):
        self.texts, self.spec = texts, spec
        self.docs, self.indexes, self.keysets = {}, {}, {}

    def doc(self, name):
        if name not in self.docs:
            if name not in self.texts:
                raise TextError(f'{name}: referenced, but the table has no text form here')
            self.docs[name] = parse_doc(name, self.texts[name], self.spec)
        return self.docs[name]

    def records(self, name):
        """[(key, mapping, where)] of every listed record (dbtechtree: each epoch record, then its nodes)."""
        d = self.doc(name)
        tspec = self.spec['tables'][name]
        if d.epochs is None:
            return [(pos, m, f'{name} line {line}') for pos, m, line in d.rows]
        f = next(f for f in self.spec['structs'][tspec['struct']]['fields'] if f['name'] == tspec['key'])
        out = []
        for em, eline, _, rows in d.epochs:
            for m, line in [(em, eline)] + [(m, line) for _, m, line in rows]:
                where = f'{name} line {line}'
                out.append((_num(f, m.get(tspec['key']), where), m, where))
        return out

    def index(self, name):
        """{id: key} of a table; a missing, malformed or duplicate id is an error naming the line."""
        if name not in self.indexes:
            names = _row_names(self.spec, name)
            idx, where_of, keys = {}, {}, {}
            for key, m, where in self.records(name):
                v = m.get('id')
                if _missing(v) or isinstance(v, (list, dict)):
                    raise TextError(f'{where}: no id')
                if not idsmod.ID_RE.match(v):
                    raise TextError(f'{where}: id {v!r}: an id is a lowercase letter, then a-z, 0-9 or _')
                if v in idx:
                    raise TextError(f'{where}: duplicate id {v!r} (also {where_of[v]})')
                if key in keys:
                    raise TextError(f'{where}: id {v!r}: key {key} is also {keys[key]!r}')
                if names and key in names and v != names[key]:
                    raise TextError(f'{where}: id {v!r} does not match row {key} ({names[key]!r})')
                idx[v], where_of[v], keys[key] = key, where, v
            self.indexes[name] = idx
            self.keysets[name] = set(keys)
        return self.indexes[name]

    def resolve(self, ref, s, typ, where, field):
        """A reference cell -> the integer the record holds."""
        if isinstance(s, (list, dict)):
            raise TextError(f'{where}: {field}: a list where a reference belongs')
        target = ref['table']
        if idsmod.ID_RE.match(s):
            idx = self.index(target)
            if s not in idx:
                raise TextError(f'{where}: {field}: {s!r} is not an id of {target}')
            v = idx[s]
            lo, hi = scalars.int_range(typ)
            if not lo <= v <= hi:
                raise TextError(f'{where}: {field}: {s!r} is key {v}, out of range for {typ}')
            return v
        v = scalars.parse_int(s, typ)
        self.index(target)
        if v not in ref['none'] and v in self.keysets[target]:
            raise TextError(f'{where}: {field}: {s} names a row of {target}: write its id')
        return v

    def build(self, name):
        """The table's db file bytes."""
        return build_table(name, self.doc(name), self.spec, self)


def parse_doc(name, text, spec):
    tspec = spec['tables'][name]
    st = spec['structs'][tspec['struct']]
    fields = st['fields']
    try:
        s = text.decode('utf-8')
    except UnicodeDecodeError as e:
        raise TextError(f'{name}: not UTF-8 ({e})') from None
    d = _Doc()
    lines = None
    if tspec['format'] == 'yaml':
        doc = parse_yaml(s)
        if not isinstance(doc, dict):
            raise TextError(f'{name}: the document is not a mapping')
        head = doc
        bad = [k for k in doc if k not in ('eedata', 'format', 'rows', 'records', 'epochs')]
        if bad:
            raise TextError(f'{name}: unknown top-level key(s) {bad}')
    else:
        head, header, body, lines = parse_csv(s)
        bad = [k for k in head if k not in ('eedata', 'format', 'rows')]
        if bad:
            raise TextError(f'{name}: unknown directive(s) {bad}')
        known = set(csv_columns(tspec, fields))
        cols = []
        for c in header:
            m = COLUMN.match(c)
            if c not in known or not m:
                raise TextError(f'{name}: unknown column {c!r}')
            cols.append((m.group(1), [int(x) for x in re.findall(r'\d+', m.group(2))]))
        records = []
        for r, line in zip(body, lines):
            m = {}
            for (base_, idx), cell in zip(cols, r):
                if cell == '':
                    continue
                if not idx:
                    m[base_] = Scalar(cell, True, line)
                    continue
                node = m.setdefault(base_, {})
                for j in idx[:-1]:
                    node = node.setdefault(j, {})
                node[idx[-1]] = Scalar(cell, True, line)
            records.append(m)
    if str(head.get('eedata', name)) != name:
        raise TextError(f'{name}: the file says it is {head.get("eedata")!r}')
    if 'format' in head and str(head['format']) != str(spec['format']):
        raise TextError(f'{name}: format {head["format"]}, this tool reads format {spec["format"]}')
    d.total = _head_int(head['rows'], 'rows', name) if head.get('rows') not in (None, '') else 0
    if tspec['container'] == 'techtree':
        eps = doc.get('epochs')
        if not isinstance(eps, list):
            raise TextError(f'{name}: epochs must be a list')
        d.epochs = []
        for k, e in enumerate(eps):
            where = f'{name} epoch {k}'
            if not isinstance(e, dict) or not isinstance(e.get('epoch'), dict):
                raise TextError(f'{where}: needs an epoch: mapping')
            bad = [x for x in e if x not in ('slots', 'epoch', 'nodes')]
            if bad:
                raise TextError(f'{where}: unknown key(s) {bad}')
            nodes = e.get('nodes') or []
            if not isinstance(nodes, list):
                raise TextError(f'{where}: nodes must be a list')
            slots = _head_int(e['slots'], 'slots', where) if e.get('slots') not in (None, '') else 0
            d.epochs.append((e['epoch'], _line(e['epoch']), slots, _positions(nodes, slots, where)))
        return d
    if tspec['format'] == 'yaml':
        records = doc.get('records') or []
        if not isinstance(records, list):
            raise TextError(f'{name}: records must be a list')
    d.rows = _positions(records, d.total, name, lines)
    return d


def build_table(name, d, spec, content):
    tspec = spec['tables'][name]
    st = spec['structs'][tspec['struct']]
    fields = st['fields']
    base = binfmt.base_record(st)
    content.index(name)   # ids checked even where nothing references the table
    t = Table(name)

    def where(m, line):
        v = m.get('id')
        return f'{name} line {line}' + (f' (id {v})' if isinstance(v, str) and not _missing(v) else '')

    if d.epochs is not None:
        t.epochs = []
        for em, eline, slots, rows in d.epochs:
            ep = build_row(tspec, fields, base, em, where(em, eline), None, content)
            placed = [(p, build_row(tspec, fields, base, m, where(m, line), None, content)) for p, m, line in rows]
            t.epochs.append(Epoch(ep, _place(placed, slots)))
        return binfmt.serialize(t, spec)
    placed = [(p, build_row(tspec, fields, base, m, where(m, line), p, content)) for p, m, line in d.rows]
    t.rows = _place(placed, d.total)
    return binfmt.serialize(t, spec)


def import_all(texts, spec, names=None):
    """{table: text bytes} -> {table: db file bytes} (names: only these). Raises TextError at the first problem."""
    c = Content(texts, spec)
    return {n: c.build(n) for n in (names or sorted(texts))}


def import_(name, text, spec, texts=None):
    """One table's db file bytes; texts: the other tables' texts (references resolve against them)."""
    all_ = dict(texts or {})
    all_[name] = text
    return Content(all_, spec).build(name)
