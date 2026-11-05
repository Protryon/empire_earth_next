"""The converter gates (eedata.py check).

(a) round trip: every table's text (exported from the retail file) compiles to a db file that exports to the same
    text again, and that db file is a fixed point (text -> db -> text -> db gives the same bytes).
(b) game-visible values: the compiled db file holds, in every row, the retail value of every field the game reads,
    and the same lead strings and lists. Hole rows (the retail database's empty rows) compile to the table's hole
    record; their game-read fields equal the retail ones except where the retail bytes are the converter's 0xCC
    fill (counted and listed). dbworld: the values each constant's readers use.
(c) specs: specs.json equals what specgen.py makes from the sources now (and the C++ EEDataTextSpecs.h what
    cppgen.py makes from specs.json), and every struct's size equals its static_assert (or the size its source
    states, for the one without one).
"""
import collections
import pathlib
import time

import binfmt
import specgen
import tables

SIZES = binfmt.SIZES


def _rows(t):
    if t.epochs is not None:
        out = []
        for e in t.epochs:
            out.append(('epoch', e.epoch))
            out += [('node', r) for r in e.nodes]
        return out
    return [('row', r) for r in t.rows]


def _junk(f, rec):
    b = rec[f['off']:f['off'] + SIZES[f['type']] * f['count']]
    return len(b) > 0 and set(b) == {0xCC}


def hole_values(n, data, spec):
    """The retail hole template's game-read values that differ from the struct's base record (0xCC fill left out):
    the table's overlay.HOLES entry."""
    tspec = spec['tables'][n]
    st = spec['structs'][tspec['struct']]
    tmpl = binfmt.retail_template(binfmt.parse(n, data, spec))
    if tmpl is None:
        return {}
    base = binfmt.base_record(st)
    out = {}
    for f in binfmt.exported(st['fields']):
        if _junk(f, tmpl):
            continue
        v, b = binfmt.field_value(f, tmpl), binfmt.field_value(f, base)
        if f['type'] == 'str':
            v, b = v[0], b[0]
            if v != b:
                out[f['name']] = v.decode('latin-1')
            continue
        if v != b:
            if f['type'] == 'f32':
                import struct
                conv = lambda x: struct.unpack('<f', struct.pack('<I', x))[0]
                v = [conv(x) for x in v] if isinstance(v, list) else conv(v)
            if isinstance(v, list) and len(set(map(str, v))) == 1:
                v = v[0]
            out[f['name']] = v
    return out


def run(a, spec, load_db):
    ok = True
    t0 = time.time()
    # (c)
    new = specgen.dumps(specgen.generate())
    if specgen.SPECS.read_bytes() != new:
        print('(c) FAIL specs.json is stale: run eedata.py specs')
        ok = False
    import cppgen
    if not cppgen.OUT.exists() or cppgen.OUT.read_bytes() != cppgen.generate(spec):
        print(f'(c) FAIL {cppgen.OUT.name} is stale: run eedata.py cppgen')
        ok = False
    nsize = 0
    for s in spec['structs'].values():
        want, how = s['size_check'] or (None, None)
        if how is None:
            continue
        nsize += 1
        if want != s['size']:
            print(f'(c) FAIL {s["name"]}: layout {s["size"]} bytes, {how} says {want}')
            ok = False
    print(f'(c) {nsize} struct sizes match their sources; the dbevents row head (no source struct) is checked by '
          f'parsing to the end of the file')
    db = load_db(a, spec)
    textdir = pathlib.Path(a.text).resolve() if a.text else None
    if textdir:
        textdir.mkdir(parents=True, exist_ok=True)
    na = nb = 0
    cls_fields, cls_bytes = collections.Counter(), collections.Counter()
    for s in spec['structs'].values():
        for f in s['fields']:
            cls_fields[f['cls']] += 1
            cls_bytes[f['cls']] += SIZES[f['type']] * f['count']
    junk_total = 0
    try:   # (a) every table at once: the references name rows of the others
        texts = tables.export_all(db, spec)
        c = tables.Content(texts, spec)
        comp = {n: c.build(n) for n in tables.binary_tables(spec)}
        ids = {n: {k: i for i, k in c.index(n).items()} for n in texts}   # the text's ids, not regenerated
        texts2 = tables.export_all(comp, spec, ids)
        c2 = tables.Content(texts2, spec)
        comp2 = {n: c2.build(n) for n in tables.binary_tables(spec)}
    except Exception as e:
        print(f'(a) FAIL: {type(e).__name__}: {e}')
        return 1
    binary = tables.binary_tables(spec)
    if textdir:   # the text-only epoch registry
        (textdir / 'epochs.yaml').write_bytes(texts['epochs'])
    if texts2.get('epochs') != texts['epochs']:
        print('(a) FAIL epochs: the text does not survive text -> db -> text')
        ok = False
    for n in binary:
        b = db[n]
        tspec = spec['tables'][n]
        st = spec['structs'][tspec['struct']]
        fields = st['fields']
        exp = binfmt.exported(fields)
        # (a)
        text, cb, text2, cb2 = texts[n], comp[n], texts2[n], comp2[n]
        if text2 != text or cb2 != cb:
            print(f'(a) FAIL {n}: the text does not survive text -> db -> text')
            ok = False
            continue
        na += 1
        if textdir:
            (textdir / (n + ('.yaml' if tspec['format'] == 'yaml' else '.csv'))).write_bytes(text)
        # (b)
        tr = binfmt.parse(n, b, spec)
        tmpl = binfmt.retail_template(tr)
        tc = binfmt.parse(n, cb, spec)
        rr, rc = _rows(tr), _rows(tc)
        names = tables._row_names(spec, n)   # dbworld: its unread rows are not compared
        only = tables._row_fields(tspec)
        bad = None
        junk = collections.Counter()
        holes = 0
        if len(rr) != len(rc):
            bad = f'{len(rc)} rows for {len(rr)}'
        for i, ((kind, x), (_, y)) in enumerate(zip(rr, rc)):
            if bad:
                break
            if names and i not in names:
                holes += 1
                continue   # a dbworld row nobody reads
            hole = tmpl is not None and x.rec == tmpl and not any(x.lead) and not x.lst
            holes += hole
            if x.lead != y.lead or x.lst != y.lst:
                bad = f'{kind} {i}: lead strings or list'
                break
            for f in exp:
                if f['cls'] == 'label' or (only and f['name'] not in only.get(i, ())):
                    continue
                vx, vy = binfmt.field_value(f, x.rec), binfmt.field_value(f, y.rec)
                if f['type'] == 'str':
                    vx, vy = vx[0], vy[0]
                if n == 'dbtechtree' and kind == 'epoch' and f['name'] == 'mTitleID' and vx == 0:
                    vx = 30145 + sum(1 for k, _ in rr[:i] if k == 'epoch')   # the original's rule, now data
                if vx != vy:
                    if hole and _junk(f, x.rec):
                        junk[f['name']] += 1
                        continue
                    bad = f'{kind} {i}: {f["name"]} {vx!r} -> {vy!r}'
                    break
        if bad:
            print(f'(b) FAIL {n}: a game-read value changes ({bad})')
            ok = False
            continue
        nb += 1
        junk_total += sum(junk.values())
        print(f'  {n:24s} {len(b):9,d} B -> {len(text):9,d} B text, {len(rc) - holes:5d} rows + {holes:4d} holes or unread'
              + (f'; 0xCC fill in hole rows (not loaded as values): {", ".join(sorted(junk))}' if junk else ''))
    print(f'(a) {na}/{len(binary)} db files (+ epochs.yaml): text -> db -> text identical')
    print(f'(b) {nb}/{len(binary)} db files: every game-read value equals retail ({junk_total:,d} hole-row '
          f'fields were 0xCC fill)')
    print('fields by class (all record layouts): ' + ', '.join(
        f'{k} {cls_fields[k]} ({cls_bytes[k]:,d} B)' for k in ('named', 'label', 'unused', 'pad')))
    print(f'{"PASS" if ok else "FAIL"} in {time.time() - t0:.0f} s')
    return 0 if ok and na == nb == len(binary) else 1
