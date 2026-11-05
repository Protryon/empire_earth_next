"""The tech tree's two text tables: epochs.yaml (the epoch registry) and dbtechtree.yaml (the techs), which compile
together into the one binary dbtechtree file (an epoch block per epoch: the epoch record, then its node slots).

epochs.yaml: `records:` in epoch order, one per epoch: `id` (stone_age ...), `label`, `slots` (the node slots of its
block), `music` (the epoch's songs, dbmusic ids: they compile into dbmusic's per-epoch flags, overlay.EPOCH_FLAGS,
and the game reads the lists, EERules.cpp) and `citizen` (the epoch's citizen media: EERules.cpp EEEpochMedia), then
the epoch record's fields. The epoch's position is its runtime id (the original's
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

TEXT_ONLY = ('music', 'citizen', 'slots')   # epochs.yaml keys the compiler takes (slots) or leaves to others (media)

# the original's per-epoch citizen rules by epoch position (EERules.cpp EEDefaultEpochMedia; rows of dbgfxeffects and
# dbsounds): nano age and later: the high-tech tools; a unit removed by a research: effects, sound, thunder
_LIGHTNING_UNIT, _DUST, _DISINTEGRATION, _SOUND_LIGHTNING, _SOUND_THUNDER = 0x15, 0xab, 0xec, 0x176, 0x1ba


def citizen_media(k):
    """epoch position -> the citizen block's values (integers for the references)."""
    if k <= 3:   # prehistoric .. bronze
        effects, every, pitch = [_LIGHTNING_UNIT, _DUST, _DUST, _DUST], 2, 0.0
    elif k <= 8:   # dark .. industrial
        effects, every, pitch = [_DISINTEGRATION, _DUST], 3, 0.0
    elif k <= 11:   # the atomic ages
        effects, every, pitch = [_DISINTEGRATION, _DUST], 0, -0.2
    else:
        effects, every, pitch = [_DISINTEGRATION, _DUST], 0, 0.0
    return dict(high_tech_tools=k >= 13, removal_effects=effects, removal_sound=_SOUND_LIGHTNING,
                removal_pitch_offset=pitch, removal_thunder=_SOUND_THUNDER, removal_thunder_every=every)


def _f32(x):
    import struct
    import scalars
    return scalars.format_f32(struct.unpack('<I', struct.pack('<f', x))[0])


def citizen_text(k, ids):
    m = citizen_media(k)
    return {'high_tech_tools': '1' if m['high_tech_tools'] else '0',
            'removal_effects': [ids['dbgfxeffects'][e] for e in m['removal_effects']],
            'removal_sound': ids['dbsounds'][m['removal_sound']],
            'removal_pitch_offset': _f32(m['removal_pitch_offset']),
            'removal_thunder': ids['dbsounds'][m['removal_thunder']],
            'removal_thunder_every': str(m['removal_thunder_every'])}


def epoch_music(music, spec, ids, k):
    """The ids of the songs that play in epoch position k: dbmusic's flag k (the last flag for the epochs past the
    flags: the original's space age plays the nano age's songs)."""
    if music is None:
        return []
    fn, _ = spec['tables']['dbmusic']['epoch_flags']
    f = next(x for x in spec['structs'][spec['tables']['dbmusic']['struct']]['fields'] if x['name'] == fn)
    if k > f['count']:
        return []
    slot = min(k, f['count'] - 1)
    return [ids['dbmusic'][i] for i, r in enumerate(music.rows) if r is not None and r.rec[f['off'] + slot]]


def derived(spec, index):
    """{field: value} of an epoch record's fields that follow from its position."""
    return {f: a * index + b for f, (a, b) in spec['tables']['epochs'].get('derived', {}).items()}


def export_texts(t, spec, ids, tables, music=None):
    """The parsed dbtechtree Table -> (dbtechtree text, epochs text); music: the parsed dbmusic Table (each epoch's
    songs)."""
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
                m['music'] = epoch_music(music, spec, ids, k)
                m['citizen'] = citizen_text(k, ids)
        epochs.append(m)
        for i, r in enumerate(e.nodes):
            if r is None:
                continue
            row = dict(tables.row_items(tt, fields, r, True, ids, None, mine[binfmt.field_value(keyf, r.rec)], pos=i))
            nodes.append({'epoch': eid, **row})
    banner = ('# {}: Empire Earth: The Art of Conquest db table (tools/eedata; record layout EETechTreeRecord, '
              f'{spec["structs"][tt["struct"]]["size"]} bytes, in tools/eedata/specs.json)\n')
    ep_banner = ('# epochs: Empire Earth: The Art of Conquest: every epoch, in order (tools/eedata/techtree.py; it '
                 'compiles with dbtechtree.yaml into db\\dbtechtree.dat). `music`: the epoch\'s songs (dbmusic ids);\n'
                 '# `citizen`: its citizen media (high-tech tools from the nano age; the effects, sound and thunder of a unit a\n'
                 '# research removes).\n')
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
