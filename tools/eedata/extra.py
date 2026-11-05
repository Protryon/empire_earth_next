"""Extra content past the original caps, for the cap tests (eedata.py extra): a scratch copy of the text tables with
rows the original fixed tables had no room for.

  python -I tools/eedata/eedata.py extra TEXTDIR OUT [--units N] [--effects]

* --units N: N more unit classes, clones of `citizen` (ids extra_unit_1..N, no `row`: they go above the 848 retail
  rows, past the original's 871 class slots when N > 23). Nothing builds them (no tech tree node names them).
* --effects: the effects of every event the tech tree names move to new rows above the original's 2000-row effect
  table, and every such event to a new row above its 1500-row event table, with the tech tree pointing at the new
  events (ids <event>_x, <effect>_x). Every effect is the same, so a game plays as with the vanilla tables: the
  differential test of the lifted event and effect caps.

The input and output are text tables (eedata.py export); the output folder must be outside the repository.
"""
import pathlib
import re

ROW = re.compile(r'^(\s*- )row: \d+\n', re.M)


def _csv_rows(text):
    lines = text.split('\n')
    hdr = next(i for i, ln in enumerate(lines) if not ln.startswith('#'))
    return lines, hdr


def _yaml_records(text):
    """[(start, end)] of the '  - row:' record blocks of a records: list."""
    starts = [m.start() for m in re.finditer(r'^  - row: ', text, re.M)]
    return [(s, starts[k + 1] if k + 1 < len(starts) else len(text)) for k, s in enumerate(starts)]


def add_units(texts, n):
    t = texts['dbobjects.yaml']
    block = next(t[s:e] for s, e in _yaml_records(t) if '\n    id: citizen\n' in t[s:e])
    out = []
    for k in range(1, n + 1):
        b = re.sub(r'^  - row: \d+\n    id: citizen\n', f'  - id: extra_unit_{k}\n', block)
        b = re.sub(r'^    mName: "[^"]*"$', f'    mName: "Extra Unit {k}"', b, count=1, flags=re.M)
        out.append(b if b.endswith('\n') else b + '\n')
    texts['dbobjects.yaml'] = t.rstrip('\n') + '\n' + ''.join(out)


def move_events(texts):
    tt, ev, ef = texts['dbtechtree.yaml'], texts['dbevents.yaml'], texts['dbeffects.csv']
    used = sorted(set(re.findall(r'^\s+m(?:Researched|Obsolete)Event: ([a-z]\w*)$', tt, re.M)))
    lines, hdr = _csv_rows(ef)
    cols = lines[hdr].split(',')
    eff = {ln.split(',')[cols.index('id')]: ln.split(',') for ln in lines[hdr + 1:] if ln}
    blocks = {}
    for s, e in _yaml_records(ev):
        blocks[re.search(r'^    id: (\w+)$', ev[s:e], re.M).group(1)] = ev[s:e]
    new_eff, new_ev, done = [], [], {}
    for name in used:
        b = blocks[name]
        ids = [x.strip() for x in re.search(r'^    effects: \[(.*)\]$', b, re.M).group(1).split(',') if x.strip()]
        for x in ids:
            if x not in done:
                cells = list(eff[x])
                cells[cols.index('row')] = ''
                cells[cols.index('id')] = done[x] = x + '_x'
                new_eff.append(','.join(cells))
        nb = re.sub(r'^  - row: \d+\n    id: ' + name + r'\n', f'  - id: {name}_x\n', b)
        nb = re.sub(r'^    effects: \[.*\]$', '    effects: [' + ', '.join(done[x] for x in ids) + ']', nb,
                    flags=re.M)
        new_ev.append(nb if nb.endswith('\n') else nb + '\n')
        tt = re.sub(r'^(\s+m(?:Researched|Obsolete)Event): ' + name + '$', r'\1: ' + name + '_x', tt, flags=re.M)
    texts['dbtechtree.yaml'] = tt
    texts['dbevents.yaml'] = ev.rstrip('\n') + '\n' + ''.join(new_ev)
    texts['dbeffects.csv'] = ef.rstrip('\n') + '\n' + '\n'.join(new_eff) + '\n'
    return len(used), len(new_eff)


def run(a):
    src, out = pathlib.Path(a.text), pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    texts = {p.name: p.read_bytes().decode('utf-8') for p in src.iterdir() if p.suffix in ('.csv', '.yaml')}
    if a.units:
        add_units(texts, a.units)
        print(f'{a.units} extra unit classes (rows 848..{848 + a.units - 1})')
    if a.effects:
        nev, nef = move_events(texts)
        print(f'{nev} events moved above row 1499 and {nef} effects above row 1999 (the tech tree points at them)')
    for name, t in texts.items():
        (out / name).write_bytes(t.encode('utf-8'))
    print(f'{len(texts)} tables -> {out}')
    return 0
