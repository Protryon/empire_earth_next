"""TEST ONLY: the live-game driver for the Linux and macOS builds (tools/xbuild.py game).

The Windows harness (game_session.py) drives the game through its debugger and presentation bridge; neither exists off
Windows. Here the game carries the same UI fixture itself (src/Empire Earth/Empire Earth/EETestFixture.cpp, on when
EE_TEST_UI names a directory): it writes ui-state.txt (the bridge's format plus game telemetry) and executes the
newest line of test.command through the controls' own methods. The lobby options, the dismiss table and the state
parser are ai_game.py's.

  python -I tools/harness/posix_game.py [--exe EXE] [--data DIR] [--out DIR]
         --exercise menu|settings|ui-captures|random-game|ai-game [--config block.key=VALUE ...]
         [--settings-select CONTROL=INDEX ...]
         [--ai-players N] [--map-size Small] [--map-type Continental] [--difficulty Easy] [--game-speed 'Very Fast']
         [--play-seconds S] [--max-game-seconds S] [--capture-every S] [--timeout S] [--zoom-captures P ...]

  menu         main menu reached, captured; the game is closed
  settings     main menu -> game settings (video tab) captured; each --settings-select sets a combo box, then OK
               (the settings are saved: summary.json has the config.yaml the game wrote) and the menu is captured
  ui-captures  the same screens every run, through the real pointer hit test (the fixture's "pointer"/"lclick": the
               game's pointer moved to window fractions, a left click as the platform delivers it), the pointer parked
               at the same place for each capture: menu, menu_hover (the settings button under the pointer), settings
               (opened by clicking that button), then a random map and game (the town centre at the window's centre
               clicked: its command buttons and status box); summary.json has the hover/click results
  random-game  single player -> random map -> Start with the lobby's defaults (session_exercise's random-game);
               gameplay captured, the physics ticks must advance; --play-seconds S plays on
  ai-game      the lobby set up as ai_game.py does (the human idles in slot 0, N computers), Start, the civilization
               timer pre-empted (builder opened and cancelled), then played until the game ends (EEPhysics::mGameOver,
               or the human's defeat), --max-game-seconds of game time or --play-seconds of wall time

Evidence goes to --out (default build/posix_game/<time>): game.log, ui/ (the fixture's files), <name>.png (macOS:
screencapture of the game window; Linux: none), samples.json and summary.json. The game's settings go to a fresh
config.yaml there (EE_CONFIG), so the data root is not written; --config block.key=VALUE seeds it. Exit 0
on PASS.
"""
import argparse
import json
import os
import pathlib
import platform
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import config_yaml
import fixture_driver
from ai_game import DISMISS, LOBBY, GAME, VISIBLE_SLOTS
from fixture_driver import (GAME_SPEED, MAIN, PARK, SETTINGS, SETTINGS_BUTTON, SETTINGS_OK, SINGLE, dismiss,
                            parse_telemetry, pointer, setup_lobby, window_state)


class Game(fixture_driver.Game):
    """The game under the fixture (fixture_driver.Game), started with its data root and a fresh config.yaml in the
    evidence directory; captures through macOS's screencapture."""

    def __init__(self, a):
        out = pathlib.Path(a.out).resolve()   # absolute: the game changes into its data root
        out.mkdir(parents=True, exist_ok=True)
        if a.config:   # seeds the fresh config.yaml (platform/PlatConfig.h; config_yaml.py)
            (out / 'config.yaml').write_text(config_yaml.config_text(a.config))
        elif (out / 'config.yaml').exists():
            (out / 'config.yaml').unlink()
        env = dict(os.environ, EE_DATA_ROOT=str(pathlib.Path(a.data).resolve()), EE_CONFIG=str(out / 'config.yaml'))
        super().__init__(out, [str(a.exe)], env)
        self.a = a

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
        for attempt in range(3):   # the window id can go stale while the game switches forms or modes
            r = subprocess.run([str(helper), 'ee-aoc'], capture_output=True, text=True)
            if r.returncode or not r.stdout.split():
                self.note(f'CAPTURE {name} failed: no game window')
                return None
            if subprocess.run(['screencapture', '-x', '-o', '-l', r.stdout.split()[0], str(path)]).returncode == 0:
                self.note(f'CAPTURE {name} -> {path}')
                return str(path)
            time.sleep(1)
        self.note(f'CAPTURE {name} failed: screencapture')
        return None


def ui_captures(g, a, summary):
    caps, checks = summary['captures'], []
    summary['pointer_checks'] = checks
    pointer(g, MAIN, *PARK)
    caps['menu'] = g.capture('menu')
    hover = pointer(g, MAIN, 0.5, 0.75)   # the main menu's 'Game Settings Button' (form index 3)
    checks.append(dict(what='hover settings button', expect=SETTINGS_BUTTON, **hover))
    caps['menu_hover'] = g.capture('menu_hover')
    click = pointer(g, MAIN, 0.5, 0.75, 'lclick')
    st = g.wait(lambda s: s['form'] == SETTINGS, 'settings opened by a click', 30)
    checks.append(dict(what='click settings button', expect_form=SETTINGS, form_after=st['form'], **click))
    time.sleep(1.5)
    pointer(g, SETTINGS, *PARK)
    caps['settings'] = g.capture('settings')
    click = pointer(g, SETTINGS, 0.131, 0.971, 'lclick')   # 'GS OK Button'
    st = g.wait(lambda s: s['form'] == MAIN, 'menu again by clicking OK', 30)
    checks.append(dict(what='click settings OK', expect_form=MAIN, form_after=st['form'], **click))
    g.click(MAIN, 0, SINGLE, 'single player')
    g.click(SINGLE, 1, LOBBY, 'random map setup')
    time.sleep(5)
    g.click(LOBBY, 10, GAME, 'start game', timeout=300)
    first = g.state()['telemetry']
    g.wait(lambda s: s['telemetry'].get('ticks', 0) > first.get('ticks', 0) and s['telemetry'].get('map'),
           'physics ticks advancing', 120)
    time.sleep(3)
    click = pointer(g, GAME, 0.44, 0.47, 'lclick')   # the town centre the camera starts on
    checks.append(dict(what='click the town centre', **click))
    time.sleep(1.5)
    pointer(g, GAME, *PARK)
    caps['game'] = g.capture('game')
    summary['result'] = 'PASS'
    return summary


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
        if a.exercise == 'ui-captures':
            return ui_captures(g, a, summary)
        if a.exercise == 'settings':
            g.click(MAIN, SETTINGS_BUTTON, SETTINGS, 'game settings')
            time.sleep(2)
            summary['captures']['settings'] = g.capture('settings')
            summary['combos_before'] = g.state()['combos']
            for item in a.settings_select:
                control, _, index = item.partition('=')
                control, index = int(control), int(index)
                seq = g.command(f'select {SETTINGS} {control} {index}')
                g.wait(lambda s: s['command'] >= seq and s['combos'].get(control, {}).get('sel') == index,
                       f'combo {control} = {index}', 30)
                g.note(f'SETTINGS combo {control} = {index} {g.state()["combos"][control]["lines"][index]!r}')
            time.sleep(1)
            summary['captures']['settings_selected'] = g.capture('settings_selected')
            g.click(SETTINGS, SETTINGS_OK, MAIN, 'settings OK')
            time.sleep(1)
            summary['config'] = (g.out / 'config.yaml').read_text(encoding='latin-1')
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
            summary['civ_preempt'] = fixture_driver.civ_preempt(g)
        tel = fixture_driver.wait_running(g, a.ai_players + 2 if a.exercise == 'ai-game' else None)
        time.sleep(3)
        summary['captures']['gameplay'] = g.capture('gameplay')
        for percent in a.zoom_captures:
            seq = g.command(f'zoom {GAME} 0 {percent}')
            g.wait(lambda st: st['command'] >= seq, f'zoom {percent}', 30)
            time.sleep(2)
            summary['captures'][f'zoom{percent}'] = g.capture(f'zoom{percent}')
        played = fixture_driver.play(g, a, a.exercise == 'ai-game', capture=g.capture)
        samples, outcome = played['samples'], played['outcome']
        time.sleep(2)
        summary['captures']['end'] = g.capture('end')
        if a.box_select and a.exercise == 'random-game':
            summary['box_select'] = fixture_driver.box_select(g, GAME)
        if a.orders and a.exercise == 'random-game':
            summary['orders'] = fixture_driver.orders(g, GAME)
        summary.update(outcome=outcome, final=g.state()['telemetry'], wall_seconds=played['wall_seconds'])
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
    ap.add_argument('--exercise', choices=['menu', 'settings', 'ui-captures', 'random-game', 'ai-game'],
                    default='ai-game')
    ap.add_argument('--config', action='append', default=[], help="block.key=VALUE in the fresh config.yaml (e.g. audio.music.enabled=false)")
    ap.add_argument('--settings-select', action='append', default=[],
                    help='settings: CONTROL=INDEX, a combo box of the settings form selected before OK')
    ap.add_argument('--ai-players', type=int, default=2)
    ap.add_argument('--map-size', default='Small')
    ap.add_argument('--map-type', default='Continental')
    ap.add_argument('--difficulty', default='Easy')
    ap.add_argument('--game-speed', default='Very Fast')
    ap.add_argument('--play-seconds', type=float, default=None, help='wall seconds of play after the start')
    ap.add_argument('--max-game-seconds', type=float, default=0, help='stop at this game time')
    ap.add_argument('--capture-every', type=float, default=0, help='capture the window every S wall seconds')
    ap.add_argument('--box-select', action='store_true',
                    help='random-game: box selections in the game view after the play (fixture_driver.box_select)')
    ap.add_argument('--orders', action='store_true',
                    help='random-game: orders through the real input path after the play (fixture_driver.orders)')
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
