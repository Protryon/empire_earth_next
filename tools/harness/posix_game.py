"""TEST ONLY: the live-game driver for the Linux and macOS builds (tools/xbuild.py game).

The Windows harness (game_session.py) drives the game through its debugger and presentation bridge; neither exists off
Windows. Here the game carries the same UI fixture itself (src/Empire Earth/Empire Earth/EETestFixture.cpp, on when
EE_TEST_UI names a directory): it writes ui-state.txt (the bridge's format plus game telemetry) and executes the
newest line of test.command through the controls' own methods. The lobby options, the dismiss table and the state
parser are ai_game.py's.

  python -I tools/harness/posix_game.py [--exe EXE] [--data DIR] [--out DIR] --exercise menu|random-game|ai-game
         [--ai-players N] [--map-size Small] [--map-type Continental] [--difficulty Easy] [--game-speed 'Very Fast']
         [--play-seconds S] [--max-game-seconds S] [--capture-every S] [--timeout S] [--zoom-captures P ...]

  menu         main menu reached, captured; the game is closed
  random-game  single player -> random map -> Start with the lobby's defaults (session_exercise's random-game);
               gameplay captured, the physics ticks must advance; --play-seconds S plays on
  ai-game      the lobby set up as ai_game.py does (the human idles in slot 0, N computers), Start, the civilization
               timer pre-empted (builder opened and cancelled), then played until the game ends (EEPhysics::mGameOver,
               or the human's defeat), --max-game-seconds of game time or --play-seconds of wall time

Evidence goes to --out (default build/posix_game/<time>): game.log, ui/ (the fixture's files), <name>.png (macOS:
screencapture of the game window; Linux: none), samples.json and summary.json. The game's settings go to a fresh
config.ini there (EE_CONFIG), so the data root is not written. Exit 0 on PASS.
"""
import argparse
import json
import os
import pathlib
import platform
import shutil
import signal
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
from ai_game import CITIZENS, DEFAULTS, DISMISS, LOBBY, GAME, NAMED, PLAYER_TYPE, TEAM, VISIBLE_SLOTS, parse_state, resolve

MAIN, SINGLE = 12, 6
GAME_SPEED = 209


def parse_telemetry(text):
    t = dict(players=[])
    for line in text.splitlines():
        p = line.split()
        if p[:2] == ['game', 'ms'] and len(p) >= 13:
            t.update(game_ms=int(p[2]), ticks=int(p[4]), over=int(p[6]), world_players=int(p[8]), map=int(p[10]),
                     local=int(p[12]))
        elif p[:1] == ['player'] and len(p) >= 10:
            t['players'].append(dict(index=int(p[1]), observer=int(p[3]), result=int(p[5]), population=int(p[7]),
                                     victory_units=int(p[9])))
    return t


class Game:
    def __init__(self, a):
        self.a = a
        self.out = pathlib.Path(a.out).resolve()   # absolute: the game changes into its data root
        self.ui = self.out / 'ui'
        self.ui.mkdir(parents=True, exist_ok=True)
        for f in self.ui.iterdir():
            f.unlink()
        self.seq = 0
        self.log_lines = []
        env = dict(os.environ, EE_TEST_UI=str(self.ui), EE_DATA_ROOT=str(pathlib.Path(a.data).resolve()),
                   EE_CONFIG=str(self.out / 'config.ini'))
        self.log = open(self.out / 'game.log', 'wb')
        self.proc = subprocess.Popen([str(a.exe)], env=env, stdout=self.log, stderr=subprocess.STDOUT,
                                     start_new_session=True)
        self.note(f'START {a.exe} pid {self.proc.pid}')

    def note(self, text):
        line = f'{time.strftime("%H:%M:%S")} {text}'
        print(line, flush=True)
        self.log_lines.append(line)

    def alive(self):
        return self.proc.poll() is None

    def check_alive(self):
        if not self.alive():
            tail = (self.out / 'game.log').read_bytes()[-3000:].decode('latin-1')
            raise RuntimeError(f'the game exited with {self.proc.returncode}; log tail:\n{tail}')

    def raw(self):
        try:
            return (self.ui / 'ui-state.txt').read_text(encoding='latin-1')
        except OSError:
            return ''

    def state(self):
        text = self.raw()
        st = parse_state(text)
        st['telemetry'] = parse_telemetry(text)
        return st

    def command(self, text):
        self.seq += 1
        tmp = self.ui / 'test.command.tmp'
        tmp.write_text(f'{self.seq} {text}\n', encoding='ascii')
        os.replace(tmp, self.ui / 'test.command')
        return self.seq

    def wait(self, pred, what, timeout=60):
        end = time.monotonic() + timeout
        while True:
            self.check_alive()
            st = self.state()
            if st['form'] is not None and pred(st):
                return st
            if time.monotonic() > end:
                raise RuntimeError(f'timeout waiting for {what}: form {st["form"]} stack {st["stack"]}')
            time.sleep(0.25)

    def click(self, form, control, then_form, what, timeout=60):
        seq = self.command(f'click {form} {control}')
        st = self.wait(lambda s: s['command'] >= seq and s['form'] == then_form, what, timeout)
        self.note(f'UI {what}: form {form} control {control} -> form {then_form}')
        return st

    def select(self, control, value, label):
        st = self.state()
        combo = st['combos'].get(control)
        if combo is None:
            raise RuntimeError(f'{label}: control {control} is not a combo on form {st["form"]}')
        index = resolve(value, combo['lines'], label)
        if combo['sel'] != index:
            seq = self.command(f'select {LOBBY} {control} {index}')
            self.wait(lambda s: s['command'] >= seq and s['combos'].get(control, {}).get('sel') == index,
                      f'{label} = {value}', 30)
            time.sleep(0.5)
        self.note(f'LOBBY {label}: control {control} = {index} {combo["lines"][index]!r}')
        return dict(control=control, label=label, value=value, index=index, line=combo['lines'][index])

    def capture(self, name):
        path = self.out / f'{name}.png'
        if platform.system() != 'Darwin':
            self.note(f'CAPTURE {name} skipped (macOS only)')
            return None
        helper = ROOT / 'build' / 'posix_game' / 'macos_window'
        source = HERE / 'macos_window.swift'
        if not helper.exists() or helper.stat().st_mtime < source.stat().st_mtime:
            helper.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(['swiftc', '-O', str(source), '-o', str(helper)], check=True, capture_output=True)
        r = subprocess.run([str(helper), 'ee-aoc'], capture_output=True, text=True)
        if r.returncode or not r.stdout.split():
            self.note(f'CAPTURE {name} failed: no game window')
            return None
        subprocess.run(['screencapture', '-x', '-o', '-l', r.stdout.split()[0], str(path)], check=True)
        self.note(f'CAPTURE {name} -> {path}')
        return str(path)

    def stop(self):
        if self.alive():
            os.killpg(self.proc.pid, signal.SIGTERM)
            try:
                self.proc.wait(10)
            except subprocess.TimeoutExpired:
                os.killpg(self.proc.pid, signal.SIGKILL)
                self.proc.wait()
        self.log.close()


def dismiss(g, st):
    if st['form'] in DISMISS:
        form = st['form']
        seq = g.command(DISMISS[form])
        g.wait(lambda s: s['command'] >= seq and s['form'] != form, f'form {form} closed', 30)
        g.note(f'PLAY form {form} cancelled ({DISMISS[form]})')
        return True
    return False


def setup_lobby(g, a):
    g.wait(lambda s: s['form'] == LOBBY and all(PLAYER_TYPE + k in s['combos'] for k in range(VISIBLE_SLOTS))
           and all(c in s['combos'] for c in NAMED.values()), 'the lobby combos', 60)
    time.sleep(3)   # local-server readiness and queued lobby updates (as ai_game.py)
    (g.out / 'lobby_before.txt').write_text(g.raw(), encoding='latin-1')
    chosen = [g.select(PLAYER_TYPE, '#0', 'slot 0 human')]
    for k in range(1, VISIBLE_SLOTS):
        chosen.append(g.select(PLAYER_TYPE + k, 'Computer' if k <= a.ai_players else 'Open', f'slot {k}'))
    options = dict(DEFAULTS)
    options[NAMED['map_size']] = a.map_size
    options[NAMED['map_type']] = a.map_type
    options[NAMED['difficulty']] = a.difficulty
    options[GAME_SPEED] = a.game_speed
    for k in range(a.ai_players + 1):
        options.setdefault(TEAM + k, '-')
        options.setdefault(CITIZENS + k, '5')
    for control, value in options.items():
        chosen.append(g.select(control, value, f'control {control}'))
    for item in a.lobby_check:   # CONTROL=0|1 (as ai_game.py's --lobby-check)
        control, _, value = item.partition('=')
        control, value = int(control), int(value)
        if g.state()['checks'].get(control) != value:
            seq = g.command(f'check {LOBBY} {control} {value}')
            g.wait(lambda s2: s2['command'] >= seq and s2['checks'].get(control) == value, f'check {control}', 30)
        g.note(f'LOBBY check {control} = {value}')
    time.sleep(2)
    st = g.state()
    drift = [c for c in chosen if st['combos'].get(c['control'], {}).get('sel') != c['index']]
    if drift:
        raise RuntimeError(f'lobby selections changed after setting: {drift}')
    (g.out / 'lobby.json').write_text(json.dumps(dict(chosen=chosen, combos=st['combos']), indent=2) + '\n')
    return chosen


def run(a):
    g = Game(a)
    summary = dict(exercise=a.exercise, exe=str(a.exe), started=time.strftime('%Y-%m-%d %H:%M:%S'), result='FAIL')
    samples = []
    try:
        g.wait(lambda s: s['form'] == MAIN, 'the main menu', a.timeout)
        g.note('MAIN MENU reached')
        time.sleep(2)
        summary['captures'] = {'menu': g.capture('menu')}
        if a.exercise == 'menu':
            summary['result'] = 'PASS'
            return summary
        g.click(MAIN, 0, SINGLE, 'single player')
        g.click(SINGLE, 1, LOBBY, 'random map setup')
        if a.exercise == 'ai-game':
            summary['lobby'] = setup_lobby(g, a)
        else:
            time.sleep(5)
        g.capture('lobby')
        t0 = time.monotonic()
        g.click(LOBBY, 10, GAME, 'start game', timeout=300)
        g.note(f'GAME form reached in {time.monotonic() - t0:.0f} s')
        if a.exercise == 'ai-game':
            # the civilization timer pre-empt (ai_game.py): open the builder and cancel it, so no timer form pauses
            seq = g.command(f'click {GAME} 5')
            st = g.wait(lambda s: s['command'] >= seq and s['form'] in DISMISS, 'the civilization builder', 30)
            dismiss(g, st)
        first = g.state()['telemetry']
        st = g.wait(lambda s: s['telemetry'].get('ticks', 0) > first.get('ticks', 0) and s['telemetry'].get('map'),
                    'physics ticks advancing', 120)
        tel = st['telemetry']
        g.note(f'GAME running: players {tel["world_players"]} local {tel["local"]} ticks {tel["ticks"]} '
               f'game ms {tel["game_ms"]}')
        if a.exercise == 'ai-game' and tel['world_players'] != a.ai_players + 2:
            raise RuntimeError(f'expected {a.ai_players + 2} world players, got {tel["world_players"]}')
        time.sleep(3)
        summary['captures']['gameplay'] = g.capture('gameplay')
        for percent in a.zoom_captures:
            seq = g.command(f'zoom {GAME} 0 {percent}')
            g.wait(lambda st: st['command'] >= seq, f'zoom {percent}', 30)
            time.sleep(2)
            summary['captures'][f'zoom{percent}'] = g.capture(f'zoom{percent}')
        start = time.monotonic()
        last_sample = last_capture = 0
        last_ticks, last_advance = tel['ticks'], time.monotonic()
        outcome = 'play-seconds'
        while True:
            g.check_alive()
            st = g.state()
            if dismiss(g, st):
                continue
            tel = st['telemetry']
            now = time.monotonic()
            if tel.get('ticks', 0) != last_ticks:
                last_ticks, last_advance = tel['ticks'], now
            elif now - last_advance > 90:
                raise RuntimeError(f'physics stalled for 90 s on form {st["form"]} (stack {st["stack"]})')
            if now - last_sample >= 10:
                last_sample = now
                samples.append(dict(wall_s=round(now - start, 1), form=st['form'], **tel))
                pops = ' '.join(f'{p["index"]}:{p["population"]}{"*" if p["result"] else ""}' for p in tel['players'])
                g.note(f'PLAY t+{now - start:.0f}s game {tel["game_ms"] / 1000:.0f} s ticks {tel["ticks"]} '
                       f'population {pops}')
                (g.out / 'samples.json').write_text(json.dumps(samples, indent=1) + '\n')
            if a.capture_every and now - last_capture >= a.capture_every:
                last_capture = now
                g.capture(f'play_{int(now - start):05d}')
            if tel.get('over'):
                outcome = 'game-over'
                break
            local = next((p for p in tel['players'] if p['index'] == tel.get('local')), None)
            if a.exercise == 'ai-game' and local and local['result']:
                outcome = 'human-result'
                break
            if a.max_game_seconds and tel.get('game_ms', 0) >= a.max_game_seconds * 1000:
                outcome = 'max-game-seconds'
                break
            if a.play_seconds is not None and now - start >= a.play_seconds:
                break
            time.sleep(1)
        time.sleep(2)
        summary['captures']['end'] = g.capture('end')
        summary.update(outcome=outcome, final=g.state()['telemetry'], wall_seconds=round(time.monotonic() - start))
        g.note(f'END {outcome}: {summary["final"]}')
        summary['result'] = 'PASS'
        return summary
    except Exception as e:
        summary['error'] = str(e)
        g.note(f'FAIL {e}')
        try:
            g.capture('failure')
        except Exception:
            pass
        return summary
    finally:
        g.stop()
        summary['log'] = g.log_lines
        (g.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        print(f'{summary["result"]} -> {g.out}', flush=True)


def main():
    host = {'Darwin': 'macos-' + ('arm64' if platform.machine() == 'arm64' else 'x86_64')}.get(platform.system(),
                                                                                             'linux-x86_64')
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--exe', type=pathlib.Path, default=ROOT / 'build' / 'xbuild' / host / 'game' / 'ee-aoc')
    ap.add_argument('--data', default=os.environ.get('EE_DATA_ROOT', str(ROOT / 'data')))
    ap.add_argument('--out', default=str(ROOT / 'build' / 'posix_game' / time.strftime('%Y%m%d-%H%M%S')))
    ap.add_argument('--exercise', choices=['menu', 'random-game', 'ai-game'], default='ai-game')
    ap.add_argument('--ai-players', type=int, default=2)
    ap.add_argument('--map-size', default='Small')
    ap.add_argument('--map-type', default='Continental')
    ap.add_argument('--difficulty', default='Easy')
    ap.add_argument('--game-speed', default='Very Fast')
    ap.add_argument('--play-seconds', type=float, default=None, help='wall seconds of play after the start')
    ap.add_argument('--max-game-seconds', type=float, default=0, help='stop at this game time')
    ap.add_argument('--capture-every', type=float, default=0, help='capture the window every S wall seconds')
    ap.add_argument('--timeout', type=float, default=120, help='seconds to reach the main menu')
    ap.add_argument('--lobby-check', action='append', default=[], help='ai-game: CONTROL=0|1, a lobby check box')
    ap.add_argument('--zoom-captures', type=int, nargs='*', default=[],
                    help='once in the game: set the camera zoom to each percent (100 = the original farthest) and capture')
    a = ap.parse_args()
    if a.exercise == 'random-game' and a.play_seconds is None:
        a.play_seconds = 0
    if not 1 <= a.ai_players < VISIBLE_SLOTS:
        ap.error('--ai-players must be 1..7')
    if not a.exe.exists():
        ap.error(f'{a.exe} does not exist: python -I tools/xbuild.py game')
    return 0 if run(a)['result'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
