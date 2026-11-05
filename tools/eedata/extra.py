"""Extra content past the original caps, for the cap tests (eedata.py extra): a scratch copy of the text tables with
rows the original fixed tables had no room for.

  python -I tools/eedata/eedata.py extra TEXTDIR OUT [--units N] [--effects]

* --units N: N more unit classes, clones of `citizen` (ids extra_unit_1..N, no `row`: they go above the 848 retail
  rows, past the original's 871 class slots when N > 23). Nothing builds them (no tech tree node names them).
* --effects: the effects of every event the tech tree names move to new rows above the original's 2000-row effect
  table, and every such event to a new row above its 1500-row event table, with the tech tree pointing at the new
  events (ids <event>_x, <effect>_x). Every effect is the same, so a game plays as with the vanilla tables: the
  differential test of the lifted event and effect caps.
* --epoch: a 16th epoch, tin_age, between the copper and the bronze age (a copy of the bronze age's entry with no
  techs of its own; it and the bronze age need no techs to advance, so a game can pass through it). Every later
  epoch moves up one place; every tech keeps its id.
* --options: game options past the original's lists (gameoptions.yaml): a 7th map size, colossal (260 tiles, the
  gigantic scripts), a 5th speed, ludicrous (3.0), and a 4th difficulty, brutal (the hard computer players; the
  damage table x 2.0 when only the target is human, x 0.5 when only the attacker is).
* --rules-demo: modded rules for the behaviour checks: combat minimum damage 5, map size small 50 tiles, positive
  morale 0.5 (combat.yaml, gameoptions.yaml).

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


def add_epoch(texts):
    s = texts['epochs.yaml']
    a = s.index('  - id: bronze_age\n')
    b = s.index('  - id: dark_age\n')
    bronze = s[a:b]
    tin = bronze.replace('  - id: bronze_age\n', '  - id: tin_age\n', 1).replace('mEpoch: bronze_age', 'mEpoch: tin_age')
    tin = re.sub(r'^    label: "[^"]*"$', '    label: "Epoch Tin"', tin, count=1, flags=re.M)
    tin = re.sub(r'^    slots: \d+$', '    slots: 0', tin, count=1, flags=re.M)
    tin = re.sub(r'^    mRequiredSOL: \d+$', '    mRequiredSOL: 0', tin, count=1, flags=re.M)
    bronze = re.sub(r'^    mRequiredSOL: \d+$', '    mRequiredSOL: 0', bronze, count=1, flags=re.M)
    texts['epochs.yaml'] = s[:a] + tin + bronze + s[b:]


def move_events(texts):
    tt, ev, ef = texts['dbtechtree.yaml'] + texts['epochs.yaml'], texts['dbevents.yaml'], texts['dbeffects.csv']
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
        for t in ('dbtechtree.yaml', 'epochs.yaml'):
            texts[t] = re.sub(r'^(\s+m(?:Researched|Obsolete)Event): ' + name + '$', r'\1: ' + name + '_x', texts[t],
                              flags=re.M)
    texts['dbevents.yaml'] = ev.rstrip('\n') + '\n' + ''.join(new_ev)
    texts['dbeffects.csv'] = ef.rstrip('\n') + '\n' + '\n'.join(new_eff) + '\n'
    return len(used), len(new_eff)


def add_options(texts):
    s = texts['gameoptions.yaml']
    s = s.replace('map_size_default: small\n',
                  '  - id: colossal\n    name: 29705\n    tiles: 260\n    script_size: 5\nmap_size_default: small\n', 1)
    s = s.replace('speed_default: normal\n',
                  '  - id: ludicrous\n    name: 31713\n    factor: 3.0\nspeed_default: normal\n', 1)
    s = s.replace('unit_limit:\n', '  - id: brutal\n    name: 30270\n    ai_level: 2\n'
                  '    damage_scale_human_target: 2.0\n    damage_scale_human_attacker: 0.5\nunit_limit:\n', 1)
    assert s.count('colossal') == 1 and s.count('ludicrous') == 1 and s.count('brutal') == 1
    texts['gameoptions.yaml'] = s


def rules_demo(texts):
    c = texts['combat.yaml']
    c = re.sub(r'^  minimum: 1( .*)?$', '  minimum: 5', c, count=1, flags=re.M)
    c = re.sub(r'^  positive_factor: 0.8( .*)?$', '  positive_factor: 0.5', c, count=1, flags=re.M)
    g = texts['gameoptions.yaml']
    g = re.sub(r'^(  - id: small\n    name: \d+.*\n)    tiles: 58\n', r'\1    tiles: 50\n', g, count=1, flags=re.M)
    assert '  minimum: 5' in c and '  positive_factor: 0.5' in c and '    tiles: 50\n' in g
    texts['combat.yaml'], texts['gameoptions.yaml'] = c, g


def run(a):
    src, out = pathlib.Path(a.text), pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    texts = {p.name: p.read_bytes().decode('utf-8') for p in src.iterdir() if p.suffix in ('.csv', '.yaml')}
    if a.units:
        add_units(texts, a.units)
        print(f'{a.units} extra unit classes (rows 848..{848 + a.units - 1})')
    if getattr(a, 'epoch', False):
        add_epoch(texts)
        print('a 16th epoch, tin_age, between the copper and the bronze age')
    if getattr(a, 'options', False):
        add_options(texts)
        print('gameoptions: a 7th map size (colossal), a 5th speed (ludicrous), a 4th difficulty (brutal)')
    if getattr(a, 'rules_demo', False):
        rules_demo(texts)
        print('rules: combat minimum damage 5, positive morale 0.5, map size small 50 tiles')
    if a.effects:
        nev, nef = move_events(texts)
        print(f'{nev} events moved above row 1499 and {nef} effects above row 1999 (the tech tree points at them)')
    for name, t in texts.items():
        (out / name).write_bytes(t.encode('utf-8'))
    print(f'{len(texts)} tables -> {out}')
    return 0
