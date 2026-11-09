"""TEST ONLY: a stand-in for the game's UI fixture (EETestFixture.cpp), for the self test of fixture_driver.py and
fixture_game.py without a game. It speaks the fixture's protocol in $EE_TEST_UI: ui-state.txt every 100 ms (the form
stack, a few controls and combos, the telemetry lines), the newest test.command line executed once per sequence
number with a "TEST UI <verdict> ..." line on stderr, frame.request -> frame.bmp, and a game log (Logs/game-*.log) in
its current directory. Forms: main menu 12 -(click 12 0)-> 6 -(click 6 1)-> lobby 26 -(click 26 10)-> game 11, whose
clock runs at 1000 game ms per 100 ms; "click 11 5" opens the civilization builder 7, "click 7 16" closes it. Prints
"TEST UI clock factor ..." and "TEST UI seed ..." for EE_TEST_TIME_FACTOR / EE_TEST_SEED, as the game does.
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
    (ui / 'ui-state.tmp').write_text('\n'.join(lines) + '\n')
    os.replace(ui / 'ui-state.tmp', ui / 'ui-state.txt')
    try:
        part = (ui / 'test.command').read_text().split()
        seq, op, a, b = int(part[0]), part[1], int(part[2]), int(part[3])
    except (OSError, IndexError, ValueError):
        seq = 0
    if seq > last:
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
