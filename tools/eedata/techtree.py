"""The tech tree's two text tables: epochs.yaml (the epoch registry) and dbtechtree.yaml (the techs), which compile
together into the one binary dbtechtree file (an epoch block per epoch: the epoch record, then its node slots).

epochs.yaml: `records:` in epoch order, one per epoch: `id` (stone_age ...), `label`, `slots` (the node slots of its
block), `music` and `citizen_models` (named slots, not read yet: the songs and citizen models per epoch are still
dbmusic's flags and code), then the epoch record's fields. The epoch's position is its runtime id (the original's
tech ids 0..14); the fields that follow from the position are left out where they do (mID, mEpoch = position,
mNextEpoch = position + 1, mFirstNodeID = (position + 1) * 1000). mTitleID is the epoch's title string (the start
epoch option's label, "Bronze Age"; the original's data has 0 there and the code adds the position to 30145: the
text writes the string).

dbtechtree.yaml: `records:` of techs, each with `epoch` (the epoch's id: the block it goes in), `row` (its slot in
that block), `id` and its fields; mID is its key (references name techs by id; an epoch is a tech too, under its own
id). A tech without mID gets (position + 1) * 1000 + slot.

The count of epochs is the number of entries (the game has no 15 any more).
"""
import binfmt
from binfmt import Epoch, Table
from textio import Scalar, TextError, emit_yaml, parse_yaml

TEXT_ONLY = ('music', 'citizen_models', 'slots')   # epochs.yaml keys the compiler takes (slots) or keeps (the slots)


def derived(spec, index):
    """{field: value} of an epoch record's fields that follow from its position."""
    return {f: a * index + b for f, (a, b) in spec['tables']['epochs'].get('derived', {}).items()}


def export_texts(t, spec, ids, tables):
    """The parsed dbtechtree Table -> (dbtechtree text, epochs text)."""
    tt, ep = spec['tables']['dbtechtree'], spec['tables']['epochs']
    fields = spec['structs'][tt['struct']]['fields']
    keyf = next(f for f in fields if f['name'] == 'mID')
    mine = ids['dbtechtree']
    epochs, nodes = [], []
    for k, e in enumerate(t.epochs):
        eid = mine[binfmt.field_value(keyf, e.epoch.rec)]
        items = tables.row_items(ep, fields, e.epoch, True, ids, None, eid)
        der = derived(spec, k)
        m = {'id': eid}
        byname = {f['name']: f for f in fields}
        for key, v in items:
            if key == 'id':
                continue
            if key in der and binfmt.field_value(byname[key], e.epoch.rec) == der[key]:
                continue
            if key == 'mTitleID' and str(v) == '0':
                v = str(30145 + k)   # the original's title rule, written out
            m[key] = v
            if key == 'label':
                m['slots'] = str(len(e.nodes))
                m['music'] = []
                m['citizen_models'] = []
        epochs.append(m)
        for i, r in enumerate(e.nodes):
            if r is None:
                continue
            row = dict(tables.row_items(tt, fields, r, True, ids, None, mine[binfmt.field_value(keyf, r.rec)], pos=i))
            nodes.append({'epoch': eid, **row})
    banner = ('# {}: Empire Earth: The Art of Conquest db table (tools/eedata; record layout EETechTreeRecord, '
              f'{spec["structs"][tt["struct"]]["size"]} bytes, in tools/eedata/specs.json)\n')
    ep_banner = ('# epochs: Empire Earth: The Art of Conquest: every epoch, in order (tools/eedata/techtree.py; it '
                 'compiles with dbtechtree.yaml into db\\dbtechtree.dat). `music` and `citizen_models` are named\n'
                 '# slots the game does not read yet.\n')
    tt_doc = {'eedata': 'dbtechtree', 'format': str(spec['format']), 'records': nodes}
    ep_doc = {'eedata': 'epochs', 'format': str(spec['format']), 'records': epochs}
    return ((banner.format('dbtechtree') + emit_yaml(tt_doc)).encode('utf-8'),
            (ep_banner + emit_yaml(ep_doc)).encode('utf-8'))


def _records(name, text, spec):
    try:
        s = text.decode('utf-8')
    except UnicodeDecodeError as e:
        raise TextError(f'{name}: not UTF-8 ({e})') from None
    doc = parse_yaml(s)
    if not isinstance(doc, dict):
        raise TextError(f'{name}: the document is not a mapping')
    bad = [k for k in doc if k not in ('eedata', 'format', 'records')]
    if bad:
        raise TextError(f'{name}: unknown top-level key(s) {bad}')
    if str(doc.get('eedata', name)) != name:
        raise TextError(f'{name}: the file says it is {doc.get("eedata")!r}')
    if 'format' in doc and str(doc['format']) != str(spec['format']):
        raise TextError(f'{name}: format {doc["format"]}, this tool reads format {spec["format"]}')
    recs = doc.get('records') or []
    if not isinstance(recs, list) or any(not isinstance(m, dict) for m in recs):
        raise TextError(f'{name}: records must be a list of mappings')
    return recs


class Layout:
    """The two texts placed: per epoch (its mapping, line, slots, [(slot, node mapping, line)])."""

    def __init__(self, texts, spec, tables):
        if 'epochs' not in texts:
            raise TextError('dbtechtree: needs epochs.yaml (the epochs)')
        eps = _records('epochs', texts['epochs'], spec)
        nodes = _records('dbtechtree', texts['dbtechtree'], spec)
        self.epochs = []
        index = {}
        for k, m in enumerate(eps):
            line = tables._line(m)
            if 'row' in m:
                raise TextError(f'epochs line {line}: an epoch has no row (its place in the file is its position)')
            v = m.get('slots')
            slots = tables._head_int(v, 'slots', f'epochs line {line}') if not tables._missing(v) else 0
            eid = m.get('id')
            if isinstance(eid, str) and not tables._missing(eid):
                index.setdefault(str(eid), k)
            self.epochs.append([m, line, slots, []])
        groups = [[] for _ in self.epochs]
        for m in nodes:
            line = tables._line(m)
            e = m.get('epoch')
            if tables._missing(e) or not isinstance(e, str) or str(e) not in index:
                raise TextError(f'dbtechtree line {line}: epoch {e!r} is not an id of epochs')
            groups[index[str(e)]].append((m, line))
        for k, g in enumerate(groups):
            placed = tables._positions([m for m, _ in g], self.epochs[k][2], f'dbtechtree epoch {k}',
                                       [ln for _, ln in g])
            self.epochs[k][3] = placed


def node_key(m, epoch, slot, where, tables, spec):
    f = next(f for f in spec['structs']['EETechTreeRecord']['fields'] if f['name'] == 'mID')
    v = m.get('mID')
    if tables._missing(v):
        return (epoch + 1) * 1000 + slot
    return tables._num(f, v, where)


def records(layout, which, tables, spec):
    """[(key, mapping, where)]: which 'epochs' (the epochs) or 'dbtechtree' (the epochs, then the techs)."""
    out = [(k, m, f'epochs line {line}') for k, (m, line, _, _) in enumerate(layout.epochs)]
    if which == 'epochs':
        return out
    for k, (_, _, _, nodes) in enumerate(layout.epochs):
        for slot, m, line in nodes:
            out.append((node_key(m, k, slot, f'dbtechtree line {line}', tables, spec), m, f'dbtechtree line {line}'))
    return out


def build(layout, spec, content, tables):
    tt, ep = spec['tables']['dbtechtree'], spec['tables']['epochs']
    st = spec['structs'][tt['struct']]
    fields = st['fields']
    base = binfmt.base_record(st)
    t = Table('dbtechtree')
    t.epochs = []
    for k, (em, eline, slots, nodes) in enumerate(layout.epochs):
        m = dict(em)
        for key in TEXT_ONLY:
            m.pop(key, None)
        w = f'epochs line {eline}' + (f' (id {em.get("id")})' if isinstance(em.get('id'), str) else '')
        erow = tables.build_row(ep, fields, base, m, w, None, content)
        rec = bytearray(erow.rec)
        byname = {f['name']: f for f in fields}
        for f, v in derived(spec, k).items():   # the fields that follow from the position, where the text has none
            if tables._missing(m.get(f)):
                binfmt.put_field(byname[f], rec, v)
        erow.rec = bytes(rec)
        placed = []
        for slot, nm, line in nodes:
            n = dict(nm)
            n.pop('epoch', None)
            if tables._missing(n.get('mID')):
                n['mID'] = Scalar(str((k + 1) * 1000 + slot), False, line)
            w = f'dbtechtree line {line}' + (f' (id {nm.get("id")})' if isinstance(nm.get('id'), str) else '')
            placed.append((slot, tables.build_row(tt, fields, base, n, w, None, content)))
        t.epochs.append(Epoch(erow, tables._place(placed, slots)))
    return binfmt.serialize(t, spec)
