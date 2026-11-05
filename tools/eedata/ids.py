"""Row ids (format 3): every listed row of every db table has an `id`, a stable, unique, lowercase ASCII key.

The converter generates them once, from the row's name (overlay.ID_NAMES: the label, mName or a lead string; the
reader's name for dbworld and dbcpbehavior; the first event that lists it for dbeffects), with _2, _3 ... where two
rows would get the same id. From then on the text owns the id: it is the entry's identity, the name or label is
display text. References between tables are ids (tables.py), resolved to the target row's key on import.

Ids match ID_RE: a letter, then letters, digits and '_'. A name that gives no id makes `<table>_<key>`.
"""
import pathlib
import re
import unicodedata

import binfmt
import overlay
import scalars

ID_RE = re.compile(r'^[a-z][a-z0-9_]*$')
ROOT = pathlib.Path(__file__).resolve().parent.parent.parent


def slug(name):
    s = unicodedata.normalize('NFKD', name).encode('ascii', 'ignore').decode('ascii').lower()
    s = re.sub(r'[^a-z0-9]+', '_', s.replace("'", '')).strip('_')
    if s and not s[0].isalpha():
        s = 'n' + s
    return s


def cpbehavior_names():
    """{record index: id} from the kCPBehavior* enumerators of include/ee/EECPBehavior.h (the readers' names)."""
    text = (ROOT / 'include' / 'ee' / 'EECPBehavior.h').read_text(encoding='latin-1')
    return {int(v, 0): overlay.snake(n) for n, v in re.findall(r'\bkCPBehavior(\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+)', text)}


def assign(named, prefix):
    """[(key, name or '')] in row order -> {key: id}: unique, first come keeps the plain form."""
    out, used = {}, set()
    for key, name in named:
        base = slug(name) if name else ''
        if not base:
            base = f'{prefix}_{key}'.replace('-', 'm')
        i, cand = 1, base
        while cand in used:
            i += 1
            cand = f'{base}_{i}'
        used.add(cand)
        out[key] = cand
    return out


def _name_of(tspec, st, row, sources):
    byname = {f['name']: f for f in st['fields']}
    lead = []
    for spec in tspec.get('lead', []):   # only single lead strings name a row
        lead.append(spec if '[' not in spec else None)
    for src in sources:
        if src in byname and byname[src]['type'] == 'str':
            v = binfmt.field_value(byname[src], row.rec)[0]
            if v.strip():
                return scalars.decode_str(v)
        elif src in lead:
            k = binfmt_lead_index(tspec, src)
            if k is not None and row.lead[k].strip():
                return scalars.decode_str(row.lead[k])
    return ''


def binfmt_lead_index(tspec, name):
    i = 0
    for spec in tspec.get('lead', []):
        if spec == name:
            return i
        if '|' in spec:
            i += 2 * int(spec.split('[')[1][:-1])
        elif '[' in spec:
            i += int(spec.split('[')[1][:-1])
        else:
            i += 1
    return None


def key_of(tspec, st, row, pos):
    """The row's runtime integer: its position, or the key field (dbtechtree mID)."""
    if tspec['key'] == 'row':
        return pos
    f = next(f for f in st['fields'] if f['name'] == tspec['key'])
    return binfmt.field_value(f, row.rec)


def listed(name, t, spec):
    """[(key, Row)] of the rows the text lists, in file order (dbtechtree: each epoch record, then its nodes)."""
    tspec = spec['tables'][name]
    st = spec['structs'][tspec['struct']]
    out = []
    if name == 'epochs':
        return []
    if t.epochs is not None:
        for e in t.epochs:
            out.append((key_of(tspec, st, e.epoch, None), e.epoch))
            out += [(key_of(tspec, st, r, i), r) for i, r in enumerate(e.nodes) if r is not None]
        return out
    names = spec.get('row_names', {}).get(name)
    return [(i, r) for i, r in enumerate(t.rows) if r is not None and (not names or str(i) in names)]


def generate(tables, spec):
    """{table: Table (holes detected)} -> {table: {key: id}} for every listed row."""
    out = {}
    for name in sorted(tables):
        if name == 'dbeffects':
            continue
        tspec = spec['tables'][name]
        st = spec['structs'][tspec['struct']]
        rows = listed(name, tables[name], spec)
        src = tspec['id_names']
        prefix = name[2:] if name.startswith('db') else name
        if src == ['@world']:
            names = {int(k): v for k, v in spec['row_names'][name].items()}
            out[name] = {k: names[k] for k, _ in rows}
            continue
        if src == ['@cpbehavior']:
            cp = cpbehavior_names()
            named = [(k, cp.get(k, '')) for k, _ in rows]
            prefix = 'behavior'
        else:
            named = [(k, _name_of(tspec, st, r, src)) for k, r in rows]
        named = [(k, overlay.ID_OVERRIDES.get(name, {}).get(k, n)) for k, n in named]
        out[name] = assign(named, prefix)
    if 'dbtechtree' in out:   # the epochs: the epoch records of dbtechtree, keyed by their position
        out['epochs'] = {k: out['dbtechtree'][k] for k in range(len(tables['dbtechtree'].epochs))}
    if 'dbeffects' in tables:   # an effect is named after the first event that lists it: <event>_<n>
        ev = out.get('dbevents', {})
        named = {}
        for k, r in listed('dbevents', tables['dbevents'], spec) if 'dbevents' in tables else []:
            for n, e in enumerate(r.lst, 1):
                named.setdefault(e, f'{ev[k]}_{n}')
        rows = listed('dbeffects', tables['dbeffects'], spec)
        out['dbeffects'] = assign([(k, named.get(k, '')) for k, _ in rows], 'effect')
    return out
