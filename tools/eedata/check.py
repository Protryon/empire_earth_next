"""The converter gates (eedata.py check).

(a) exact: every table round-trips db -> text -> db byte-identical (and the text re-exports identically).
(b) clean: the clean text compiles to a db file whose every field value equals the exact file's, except the junk
    the clean mode drops (bytes after a string's NUL, alignment padding); the hole rows stay holes.
(c) specs: specs.json equals what specgen.py makes from the sources now (and the C++ EEDataTextSpecs.h what
    cppgen.py makes from specs.json), and every struct's size equals its
    static_assert (or the size its source states, for the two without one).
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
            out.append(e.epoch)
            out += e.nodes
        return out
    return t.rows


def _values(fields, r):
    out = []
    for f in fields:
        if f['type'] == 'pad':
            continue
        v = binfmt.field_value(f, r.rec)
        out.append(v[0] if f['type'] == 'str' else v)
    return out, r.lead, r.lst


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
    virtual = sorted(s['name'] for s in spec['structs'].values() if not s['size_check'])
    print(f'(c) {nsize} struct sizes match their sources ({sum(1 for s in spec["structs"].values() if s["size_check"] and s["size_check"][1] == "comment")} by '
          f'the size comment); {len(virtual)} layouts decoded from the data ({", ".join(virtual)}) are checked by '
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
    total_junk = 0
    for n in sorted(spec['tables']):
        b = db[n]
        tspec = spec['tables'][n]
        fields = spec['structs'][tspec['struct']]['fields']
        # (a)
        try:
            text = tables.export(n, b, spec, 'exact')
            back = tables.import_(n, text, spec)
        except Exception as e:   # report every table
            print(f'(a) FAIL {n}: {type(e).__name__}: {e}')
            ok = False
            continue
        if back != b:
            k = next((i for i in range(min(len(b), len(back))) if b[i] != back[i]), min(len(b), len(back)))
            print(f'(a) FAIL {n}: {len(back)} bytes back for {len(b)}; first difference at {k}')
            ok = False
            continue
        if tables.export(n, back, spec, 'exact') != text:
            print(f'(a) FAIL {n}: the text does not re-export identically')
            ok = False
            continue
        na += 1
        if textdir:
            (textdir / (n + ('.yaml' if tspec['format'] == 'yaml' else '.csv'))).write_bytes(text)
        # (b)
        try:
            cb = tables.import_(n, tables.export(n, b, spec, 'clean'), spec)
        except Exception as e:
            print(f'(b) FAIL {n}: {type(e).__name__}: {e}')
            ok = False
            continue
        te = binfmt.detect_holes(binfmt.parse(n, b, spec), spec)
        tc = binfmt.parse(n, cb, spec)
        tc.hole = binfmt.clean_record(fields, te.hole) if te.hole is not None else None
        re_, rc = _rows(te), _rows(tc)
        bad = None
        if len(re_) != len(rc) or te.header != tc.header:
            bad = 'row count/header'
        else:
            for i, (x, y) in enumerate(zip(re_, rc)):
                x = x or binfmt.Row(te.hole, [b''] * binfmt.lead_count(tspec), [])
                if _values(fields, x) != _values(fields, y):
                    bad = f'row {i}'
                    break
        if bad:
            print(f'(b) FAIL {n}: clean changes a field value ({bad})')
            ok = False
            continue
        junk = sum(1 for i in range(len(b)) if b[i] != cb[i]) if len(b) == len(cb) else None
        total_junk += junk or 0
        nb += 1
        print(f'  {n:24s} {len(b):9,d} B  exact ok  clean ok ({junk:,d} junk bytes zeroed)')
    print(f'(a) {na}/{len(spec["tables"])} tables round-trip byte-identical in exact mode')
    print(f'(b) {nb}/{len(spec["tables"])} tables: clean keeps every field value '
          f'({total_junk:,d} junk bytes zeroed in all)')
    print('fields by class (all record layouts): ' + ', '.join(
        f'{k} {cls_fields[k]} ({cls_bytes[k]:,d} B)' for k in ('named', 'inferred', 'provisional', 'unknown', 'pad')))
    print(f'{"PASS" if ok else "FAIL"} in {time.time() - t0:.0f} s')
    return 0 if ok and na == nb == len(spec['tables']) else 1
