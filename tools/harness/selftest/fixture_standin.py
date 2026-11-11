"""TEST ONLY: a stand-in for the game's UI fixture (EETestFixture.cpp), for the self test of fixture_driver.py and
fixture_game.py without a game. It speaks the fixture's protocol in $EE_TEST_UI: ui-state.txt every 100 ms (the form
stack, a few controls and combos, the telemetry lines), the newest test.command line executed once per sequence
number with a "TEST UI <verdict> ..." line on stderr, frame.request -> frame.bmp, and a game log (Logs/game-*.log) in
its current directory. Forms: main menu 12 -(click 12 0)-> 6 -(click 6 1)-> lobby 26 -(click 26 10)-> game 11, whose
clock runs at 1000 game ms per 100 ms; "click 11 5" opens the civilization builder 7, "click 7 16" closes it. Prints
"TEST UI clock factor ..." and "TEST UI seed ..." for EE_TEST_TIME_FACTOR / EE_TEST_SEED, as the game does. On the game
form, "boxselect 11 <mode> <x0> <y0> <x1> <y1>" selects among ten units at x = 100 + 60 k, y = 250 of the view
0 0 800 500 (mode 1 adds, mode 2 deletes the selection's first unit before the box executes), with the fixture's
"selection", "view" and "boxselect" lines; "order 11 <kind>" judges two units ok ("order" and "orderunit" lines), but
attack ground (kind 4), which it refuses with "no-unit", as a start without a siege unit does. The camera ("camera"
telemetry line) starts at the map's right edge (63.8, 30 of 64 tiles), so the right arrow cannot move it:
"key 11 <dik> <ms>" moves it 1 tile a 100 ms held (arrows; ']' and '[' zoom by 0.1, from 0 to 2), clamped to the
map; "pointer 11 0 <y>" scrolls it 0.5 tile a 100 ms left until the pointer leaves the edge; "wheel 11 <0|1> <n>"
zooms by 0.05 a notch; "zoom 11 0 <percent>" sets the zoom.
"""
import os
import pathlib
import struct
import sys
import time

ui = pathlib.Path(os.environ['EE_TEST_UI'])
if os.environ.get('EE_TEST_TIME_FACTOR'):
    print(f'TEST UI clock factor {os.environ["EE_TEST_TIME_FACTOR"]}/1', file=sys.stderr, flush=True)
if os.environ.get('EE_TEST_SEED'):
    print(f'TEST UI seed {os.environ["EE_TEST_SEED"]}', file=sys.stderr, flush=True)
stack, last, frames, game_ms, ticks, ct, civ = [12], 0, 0, 0, 0, 0, 0
units = {100 + k: 100 + 60 * k for k in range(10)}   # id -> x (all at y 250)
selection, box, order = [], None, None
cam = dict(x=63.8, y=30.0, zoom=1.0, held=None, until=0.0, edge=False)
KEYS = {0xC8: (0, 1), 0xD0: (0, -1), 0xCB: (-1, 0), 0xCD: (1, 0)}


def ids(values):
    values = sorted(values)
    return f'{len(values)} ' + (','.join(map(str, values)) or '-')


def box_select(mode, x0, y0, x1, y1):
    global selection, box
    dead = selection[0] if mode == 2 and selection else -1
    lasso = [i for i, x in units.items() if min(x0, x1) <= x <= max(x0, x1) and min(y0, y1) <= 250 <= max(y0, y1)]
    before = list(selection)
    if dead >= 0:
        del units[dead]
    picked = [i for i in lasso if i in units]
    selection = sorted(set(before) | set(picked)) if mode == 1 else picked
    box = dict(seq=last, mode=mode, lasso=lasso, before=before, dead=dead,
               dead_in_lasso=int(dead in lasso), dead_state='gone' if dead >= 0 else 'none')
moves = {('click', 12, 0): 6, ('click', 6, 1): 26, ('click', 26, 10): 11, ('click', 11, 5): 7, ('click', 7, 16): 11}
pathlib.Path('Logs').mkdir(exist_ok=True)
log = open('Logs/game-20260101-000000.log', 'w')
log.write('EE-GAME 0 p0 start map=Continental size=1 difficulty=2 players=4\n')
for i, kind in ((1, 'human'), (2, 'computer'), (3, 'computer')):
    log.write(f'EE-GAME 0 p{i} player kind={kind} name=P{i} civ=Greek personality=- cheats=- engine=-\n')
log.flush()
while True:
    form = stack[-1]
    if form == 11:
        game_ms, ticks, ct = game_ms + 1000, ticks + 25, ct + 5
        if cam['held'] is not None and time.monotonic() < cam['until']:
            dx, dy = KEYS.get(cam['held'], (0, 0))
            cam['x'] = min(max(cam['x'] + dx, 0.0), 64.0)
            cam['y'] = min(max(cam['y'] + dy, 0.0), 64.0)
            if cam['held'] in (0x1A, 0x1B):
                cam['zoom'] = min(max(cam['zoom'] + (0.1 if cam['held'] == 0x1A else -0.1), 0.0), 2.0)
        if cam['edge']:
            cam['x'] = max(cam['x'] - 0.5, 0.0)
        if game_ms % 30000 == 0:
            for i in (1, 2, 3):
                log.write(f'EE-GAME {game_ms} p{i} sample epoch=1 pop={5 * i} army=0\n')
            log.flush()
    lines = [f'form {form} controls 20', f'command {last}', f'pid {os.getpid()}', 'stack ' + ' '.join(map(str, stack)),
             'mouseover -1', 'window 800 600', 'control 0 event-rva 0']
    if form == 26:
        lines += [f'combo {30 + k} sel 0 lines 2 Open|Computer' if k else 'combo 30 sel 0 lines 2 Me|OBSERVE'
                  for k in range(8)]
    lines.append(f'game ms {game_ms} ticks {ticks} over 0 players {4 if form in (11, 7) else 0} map '
                 f'{int(form in (11, 7))} local 1')
    lines.append(f'comm ct {ct} last {ct} paused {int(form == 7)} multiplayer 0')
    for i in range(4):
        lines.append(f'player {i} observer 0 result 0 population {5 * i} victory {i} civ 0 {int(i == 1 and civ)} '
                     f'stock 1/2/3/4/5')
    lines.append(f'clock 1/1 ms {int(time.monotonic() * 1000)} frames {frames}')
    if form == 11:
        lines += [f'selection {ids(selection)}', 'view 0 0 800 500']
        lines.append(f'camera {cam["x"]:.3f} {cam["y"]:.3f} 8.000 target {cam["x"]:.3f} {cam["y"]:.3f} zoom '
                     f'{cam["zoom"]:.4f} mode 0 valid 1 1 size 64')
        if box:
            lines.append(f'boxselect seq {box["seq"]} mode {box["mode"]} phase 3 lasso {ids(box["lasso"])} before '
                         f'{ids(box["before"])} expected {ids(selection)} match 1 dead {box["dead"]} dead_in_lasso '
                         f'{box["dead_in_lasso"]} dead_state {box["dead_state"]}')
    if form == 11 and order:
        lines.append(f'order seq {order["seq"]} kind {order["kind"]} phase {order["phase"]} type 0 target 7 point 0.000 '
                     f'0.000 queued {int(order["kind"] == 2)} units {len(order["units"])} ok {len(order["units"])} bad 0 '
                     f'skip 0 note {order["note"]}')
        lines += [f'orderunit {order["seq"]} {u} ok gather:7 gather:7' for u in order['units']]
    (ui / 'ui-state.tmp').write_text('\n'.join(lines) + '\n')
    for attempt in range(20):   # Windows refuses the rename while the driver reads the file
        try:
            os.replace(ui / 'ui-state.tmp', ui / 'ui-state.txt')
            break
        except PermissionError:
            time.sleep(0.01)
    try:
        part = (ui / 'test.command').read_text().split()
        seq, op, a, b = int(part[0]), part[1], int(part[2]), int(part[3])
    except (OSError, IndexError, ValueError):
        seq = 0
    if seq > last and op == 'boxselect':
        last = seq
        if a == form == 11 and len(part) == 8 and 0 <= b <= 2:
            box_select(b, *map(int, part[4:8]))
            print(f'TEST UI box-started boxselect control {b} value 0 on form {a} (seq {seq}, top form {form})',
                  file=sys.stderr, flush=True)
            print(f'TEST UI box-released boxselect control {b} value 0 on form 0 (seq {seq}, top form 0)',
                  file=sys.stderr, flush=True)
        else:
            print(f'TEST UI rejected boxselect control {b} value 0 on form {a} (seq {seq}, top form {form})',
                  file=sys.stderr, flush=True)
    elif seq > last and op in ('key', 'wheel', 'pointer', 'zoom'):
        last = seq
        ok = a == form == 11 and len(part) == 5
        if ok and op == 'key':
            cam['held'], cam['until'] = b, time.monotonic() + int(part[4]) / 1000
        elif ok and op == 'wheel':
            cam['zoom'] = min(max(cam['zoom'] + (0.05 if b else -0.05) * int(part[4]), 0.0), 2.0)
        elif ok and op == 'pointer':
            cam['edge'] = b == 0
        elif ok and op == 'zoom':
            cam['zoom'] = int(part[4]) / 100
        verdict = {'key': 'key-pressed', 'wheel': 'wheeled', 'pointer': 'pointer-moved', 'zoom': 'zoomed'}[op]
        print(f'TEST UI {verdict if ok else "rejected"} {op} control {b} value 0 on form {a} (seq {seq}, top form '
              f'{form})', file=sys.stderr, flush=True)
    elif seq > last and op == 'order':
        last = seq
        if a == form == 11 and 0 <= b <= 5:
            print(f'TEST UI order-started order control {b} value 0 on form {a} (seq {seq}, top form {form})',
                  file=sys.stderr, flush=True)
            refused = b == 4
            order = dict(seq=seq, kind=b, phase=9 if refused else 3, note='no-unit' if refused else '-',
                         units=[] if refused else [101, 102])
            print(f'TEST UI order-{"refused" if refused else "done"} order control {b} value 0 on form 0 (seq {seq}, '
                  f'top form 0)', file=sys.stderr, flush=True)
        else:
            print(f'TEST UI rejected order control {b} value 0 on form {a} (seq {seq}, top form {form})',
                  file=sys.stderr, flush=True)
    elif seq > last:
        last = seq
        target = moves.get((op, a, b)) if a == form else None
        if target is not None:
            civ = civ or target == 7
            if target == 7 or (form == 12 and target == 6) or (form == 6 and target == 26):
                stack.append(target)
            elif target == 11 and form == 26:
                stack = [11]
            else:
                stack.pop()
        verdict = 'triggered' if target is not None else 'rejected'
        print(f'TEST UI {verdict} {op} control {b} value 0 on form {a} (seq {seq}, top form {form})', file=sys.stderr,
              flush=True)
    if (ui / 'frame.request').exists():
        (ui / 'frame.request').unlink()
        width, height = 64, 48
        pixels = bytes((x * 4 + y * 3 + k * 50) & 0xff for y in range(height) for x in range(width) for k in range(4))
        header = b'BM' + struct.pack('<IHHI', 54 + len(pixels), 0, 0, 54) + struct.pack(
            '<IiiHHIIiiII', 40, width, -height, 1, 32, 0, 0, 0, 0, 0, 0)
        (ui / 'frame.tmp').write_bytes(header + pixels)
        (ui / 'frame.bmp').unlink(missing_ok=True)
        os.replace(ui / 'frame.tmp', ui / 'frame.bmp')
        frames += 1
    time.sleep(0.1)
