"""The binary side: the containers of the db files, record field decoding/encoding and hole detection.

A table in memory: Table(name, rows, epochs). A row is Row(rec, lead, lst): the record bytes, the leading strings
(bytes; dbsounds/dbterrain/dbgraphics) and the trailing list (ints; dbevents/dbtechtree). None in a row list is a
hole: an empty database row, written as the table's hole record (hole_record: the struct's defaults plus the table's
hole values, no lead text, empty list). dbtechtree keeps its rows in epochs: [Epoch(record row, slots: [Row|None])].
"""
import collections
import struct
from dataclasses import dataclass, field

import scalars

SIZES = {'s8': 1, 'u8': 1, 'bool': 1, 's16': 2, 'u16': 2, 's32': 4, 'u32': 4, 'f32': 4, 'pair_f32': 8, 'str': 1,
         'raw': 1, 'pad': 1}
INT_FMT = {'s8': 'b', 'u8': 'B', 'bool': 'B', 's16': 'h', 'u16': 'H', 's32': 'i', 'u32': 'I'}


class FormatError(Exception):
    pass


@dataclass
class Row:
    rec: bytes
    lead: list = field(default_factory=list)
    lst: list = field(default_factory=list)


@dataclass
class Epoch:
    epoch: Row
    nodes: list


@dataclass
class Table:
    name: str
    rows: list = None          # [Row | None] (all containers but techtree)
    epochs: list = None        # [Epoch] (techtree)


def lead_count(tspec):
    n = 0
    for name in tspec.get('lead', []):
        n += 2 * int(name.split('[')[1][:-1]) if '|' in name else (int(name.split('[')[1][:-1]) if '[' in name else 1)
    return n


class _Reader:
    def __init__(self, data, table):
        self.d, self.p, self.table = data, 0, table

    def need(self, n):
        if self.p + n > len(self.d):
            raise FormatError(f'{self.table}: truncated at byte {self.p} (need {n} more)')

    def u32(self):
        self.need(4)
        v, = struct.unpack_from('<I', self.d, self.p)
        self.p += 4
        return v

    def take(self, n):
        self.need(n)
        b = self.d[self.p:self.p + n]
        self.p += n
        return b

    def ints(self, n, signed):
        self.need(4 * n)
        v = list(struct.unpack_from('<%d%s' % (n, 'i' if signed else 'I'), self.d, self.p))
        self.p += 4 * n
        return v

    def done(self):
        return self.p == len(self.d)


def parse(name, data, spec):
    """bytes of one db file -> Table (holes not yet detected: every row is a Row)."""
    tspec = spec['tables'][name]
    size = spec['structs'][tspec['struct']]['size']
    kind = tspec['container']
    r = _Reader(data, name)
    t = Table(name)
    signed = tspec.get('list', ('', 's32'))[1] == 's32'
    if kind == 'count':
        t.rows = [Row(r.take(size)) for _ in range(r.u32())]
    elif kind == 'lead':
        k = lead_count(tspec)
        t.rows = []
        for _ in range(r.u32()):
            lead = [r.take(r.u32()) for _ in range(k)]
            t.rows.append(Row(r.take(size), lead))
    elif kind == 'fixed':
        if len(data) % size:
            raise FormatError(f'{name}: {len(data)} bytes is not a multiple of the {size}-byte record')
        t.rows = [Row(r.take(size)) for _ in range(len(data) // size)]
    elif kind == 'events':
        t.rows = []
        while not r.done():
            rec = r.take(size)
            t.rows.append(Row(rec, [], r.ints(r.u32(), signed)))
    elif kind == 'techtree':
        t.epochs = []
        while not r.done():
            n = r.u32()
            rec = r.take(size)
            ep = Row(rec, [], r.ints(r.u32(), signed))
            nodes = []
            for _ in range(n):
                rec = r.take(size)
                nodes.append(Row(rec, [], r.ints(r.u32(), signed)))
            t.epochs.append(Epoch(ep, nodes))
    else:
        raise FormatError(f'{name}: unknown container {kind}')
    if not r.done():
        raise FormatError(f'{name}: {len(data) - r.p} bytes left after the last record')
    return t


def _all_row_lists(t):
    return [e.nodes for e in t.epochs] if t.epochs is not None else [t.rows]


def exported(fields):
    return [f for f in fields if f['cls'] in ('named', 'label')]


def _put_raw_int(f, buf, v):
    n = SIZES[f['type']] * f['count']
    buf[f['off']:f['off'] + n] = (v & ((1 << (8 * n)) - 1)).to_bytes(n, 'little')


def base_record(st):
    """The record the importer starts every row from: zero, plus the struct's defaults for unread fields."""
    buf = bytearray(st['size'])
    for f in st['fields']:
        if f['name'] in st['defaults']:
            v = st['defaults'][f['name']]
            if f['type'] == 'raw':
                _put_raw_int(f, buf, v)
            else:
                put_field(f, buf, v if f['count'] == 1 or isinstance(v, list) else [v] * f['count'])
    return bytes(buf)


def hole_record(tspec, st):
    """An empty database row: the base record plus the table's hole values (a scalar for an array fills it)."""
    buf = bytearray(base_record(st))
    for f in st['fields']:
        if f['name'] in tspec['hole']:
            v = tspec['hole'][f['name']]
            if f['type'] == 'str':
                v = (scalars.encode_str(v), b'')
            elif f['count'] > 1 and not isinstance(v, list):
                v = [v] * f['count']
            put_field(f, buf, v)
    return bytes(buf)


def project(fields, r):
    """What the game can see of a row: the exported fields' values (strings up to the NUL), lead strings, list."""
    out = []
    for f in exported(fields):
        v = field_value(f, r.rec)
        out.append(v[0] if f['type'] == 'str' else v)
    return out, list(r.lead), list(r.lst)


def retail_template(t):
    """The retail files' hole template: the most frequent record among the rows with no lead text and an empty
    list, if it occurs at least twice and holds some 0xCC fill (the converter's uninitialised struct,
    DATA_FORMATS.md 2.4) or makes up a tenth of the rows. None when the table has no holes."""
    cnt = collections.Counter(r.rec for rows in _all_row_lists(t) for r in rows
                              if r is not None and not any(r.lead) and not r.lst)
    total = sum(len(rows) for rows in _all_row_lists(t))
    for rec, n in cnt.most_common(1):
        if n >= 2 and (rec.count(0xCC) >= 4 or n * 10 >= total):
            return rec
    return None


def detect_holes(t, spec):
    """Replace the hole rows by None: rows equal to the retail template, or rows the game sees exactly as the
    table's hole record."""
    tspec = spec['tables'][t.name]
    st = spec['structs'][tspec['struct']]
    tmpl = retail_template(t)
    empty = project(st['fields'], Row(hole_record(tspec, st), [b''] * lead_count(tspec), []))
    for rows in _all_row_lists(t):
        for i, r in enumerate(rows):
            if r is None or any(r.lead) or r.lst:
                continue
            if r.rec == tmpl or project(st['fields'], r) == empty:
                rows[i] = None
    return t


def serialize(t, spec):
    """Table -> the db file bytes."""
    tspec = spec['tables'][t.name]
    st = spec['structs'][tspec['struct']]
    size = st['size']
    kind = tspec['container']
    signed = tspec.get('list', ('', 's32'))[1] == 's32'
    out = bytearray()
    hole = Row(hole_record(tspec, st), [b''] * lead_count(tspec), [])

    def rec(r):
        if len(r.rec) != size:
            raise FormatError(f'{t.name}: record of {len(r.rec)} bytes, want {size}')
        out.extend(r.rec)

    def lst(r):
        out.extend(struct.pack('<I', len(r.lst)))
        out.extend(struct.pack('<%d%s' % (len(r.lst), 'i' if signed else 'I'), *r.lst))

    if kind == 'techtree':
        for e in t.epochs:
            out.extend(struct.pack('<I', len(e.nodes)))
            rec(e.epoch)
            lst(e.epoch)
            for r in e.nodes:
                r = r or hole
                rec(r)
                lst(r)
        return bytes(out)
    rows = [r or hole for r in t.rows]
    if kind in ('count', 'lead'):
        out.extend(struct.pack('<I', len(rows)))
    k = lead_count(tspec)
    for r in rows:
        if kind == 'lead':
            if len(r.lead) != k:
                raise FormatError(f'{t.name}: {len(r.lead)} lead strings, want {k}')
            for s in r.lead:
                out.extend(struct.pack('<I', len(s)))
                out.extend(s)
        rec(r)
        if kind == 'events':
            lst(r)
    return bytes(out)


# ---- record fields ----

def field_value(f, b):
    """Decode one field from record bytes -> python value (by type; see scalars for the text forms):
    ints -> int, f32 -> int bits, str -> (text bytes, tail bytes), raw/pad -> bytes; arrays -> list."""
    o, t, n = f['off'], f['type'], f['count']
    if t == 'str':
        s = b[o:o + n]
        z = s.find(b'\0')
        return (s, b'') if z < 0 else (s[:z], s[z + 1:])
    if t in ('raw', 'pad'):
        return b[o:o + n]
    if t == 'f32':
        v = list(struct.unpack_from('<%dI' % n, b, o))
    elif t == 'pair_f32':
        flat = struct.unpack_from('<%dI' % (2 * n), b, o)
        v = [list(flat[2 * i:2 * i + 2]) for i in range(n)]
    else:
        v = list(struct.unpack_from('<%d%s' % (n, INT_FMT[t]), b, o))
    return v[0] if n == 1 else v


def put_field(f, buf, v):
    """Inverse of field_value into bytearray buf."""
    o, t, n = f['off'], f['type'], f['count']
    if t == 'str':
        text, tail = v
        raw = text + (b'\0' + tail if len(text) < n else b'')
        if len(text) > n or len(raw) > n:
            raise FormatError(f'{f["name"]}: {len(text)} characters (+ tail {len(tail)}) do not fit char[{n}]')
        buf[o:o + len(raw)] = raw
        return
    if t in ('raw', 'pad'):
        if len(v) != n:
            raise FormatError(f'{f["name"]}: {len(v)} bytes, want {n}')
        buf[o:o + n] = v
        return
    vals = [v] if n == 1 else list(v)
    if len(vals) != n:
        raise FormatError(f'{f["name"]}: {len(vals)} values, want {n}')
    if t == 'pair_f32':
        struct.pack_into('<%dI' % (2 * n), buf, o, *[x for p in vals for x in p])
    elif t == 'f32':
        struct.pack_into('<%dI' % n, buf, o, *vals)
    else:
        lo, hi = scalars.int_range(t)
        for x in vals:
            if not lo <= x <= hi:
                raise FormatError(f'{f["name"]}: {x} out of range for {t}')
        struct.pack_into('<%d%s' % (n, INT_FMT[t]), buf, o, *vals)
