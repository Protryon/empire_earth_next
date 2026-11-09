"""TEST ONLY: the Windows live-game driver over the game's own UI fixture: the gates' path (tools/gates.sh).

It reads nothing from the game's memory and hooks no symbol or import: no record layout (layout.json), no mangled
name, no debugger. The game is the test-profile exe started directly with EE_TEST_UI (EETestFixture.cpp: ui-state.txt
with the form stack, the controls and the game telemetry; test.command; frame.request -> frame.bmp); the game log
(Logs/game-*.log, EE-GAME lines) is the second source. The harness clock is the game's own (EE_TEST_TIME_FACTOR ->
PlatSimSetTimeFactor), the seed PlatSim's (EE_TEST_SEED), the null 3D device GX's (EE_GX_NULL3D). The exercises are
fixture_driver.py's, shared with posix_game.py.

  python -I tools/harness/fixture_game.py --exercise tour [--play-seconds 120] [--input-check]
  python -I tools/harness/fixture_game.py --exercise ai-game --ai-players 2 --map-size Small --difficulty Hard \\
         --max-game-seconds 600 --time-factor 32 --no-render --seed 2 --pt-rate 40
  options: [--exe EXE] [--data TREE] [--out DIR] [--timeout S] [--config block.key=VALUE ...]

  menu         the main menu reached and its frame captured
  tour         as game_session.py's tour: main menu, the settings form's video/audio/interface tabs and Cancel, single
               player -> random map -> Start; the gameplay frame must be lit and varied (session_exercise's rendering
               check), physics ticks must advance; --play-seconds S plays on (civilization forms cancelled), then the
               play frame and, from 10 s, its mini map are checked; --input-check presses F3 twice through window
               messages and requires the game to pause and resume (telemetry 'comm ... paused')
  random-game  the tour without the settings
  ai-game      as game_session.py's ai-game: the lobby set up (the human idles in slot 0, N computers), Start, the
               civilization timer pre-empted, played until the game ends, --max-game-seconds of game time
               (outcome time-cap) or --play-seconds of wall time

Data: --data TREE stages that unified open data tree (tools/open_data.py); else build/open_data/tree (or the tree
EE_HARNESS_OPEN_DATA names, as exe_boot.py), built when missing. The game reads only the open forms (RUST_PLAN D9):
there is no binary tree. The stage is build/fixture_game/stage (the exe and the tree as hard links; the game's data
root is the exe's directory), with a fresh config.yaml holding the run's pins.
Evidence: --out (default build/fixture_game/<time>): game.log (the game's output), ui/, the frames (<name>.bmp),
rendering.json, samples.json, the game log (game-*.log), summary.json; the last line printed is
"RESULT PASS|FAIL <exercise> ..." (an ai-game adds outcome, final PT and per-player units). Exit 0 on PASS.
"""
import argparse
import json
import os
import pathlib
import shutil
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import config_yaml
import fixture_driver
from ai_game import GAME, LOBBY, VISIBLE_SLOTS
from fixture_driver import MAIN, SETTINGS, SINGLE, WINDOWS

STAGE = ROOT / 'build' / 'fixture_game' / 'stage'
EXE = 'EE-AOC.exe' if WINDOWS else 'ee-aoc'


def data_tree(a):
    """The unified data tree the run stages (--data, else exe_boot.data_tree: build/open_data/tree)."""
    if a.data:
        tree = pathlib.Path(a.data).resolve()
        if not (tree / 'Data').is_dir():
            raise SystemExit(f'{tree} is not a data tree (no Data folder)')
        return tree
    import exe_boot
    return exe_boot.data_tree()


def stage(a, tree):
    """build/fixture_game/stage: the exe and the data tree (exe_boot.stage_tree: hard links, the folders the game
    writes into copied) and a fresh config.yaml with the run's pins."""
    import exe_boot
    if STAGE.resolve().parent != (ROOT / 'build' / 'fixture_game').resolve():
        raise ValueError('the stage must stay inside build/fixture_game')
    if STAGE.exists():
        shutil.rmtree(STAGE)
    STAGE.mkdir(parents=True)
    shutil.copy(a.exe, STAGE / EXE)
    exe_boot.stage_tree(tree, STAGE)
    pins = ['audio.music.enabled=true']   # as game_session.py: the mixer runs (the null device without audio)
    if a.pt_rate:
        pins += [f'network.pt_rate.min={a.pt_rate}', f'network.pt_rate.max={a.pt_rate}']
    pins += a.config
    (STAGE / 'config.yaml').write_bytes(config_yaml.config_text(pins).encode('cp1252'))
    files = json.loads((tree / 'open_data.json').read_text()).get('files') if (tree / 'open_data.json').exists() else '?'
    print(f'data: {tree} ({files} files) staged in {STAGE}', flush=True)
    return pins


class Game(fixture_driver.Game):
    """The staged exe under the fixture; captures are window captures on Windows (fixture_driver.capture_frame)."""

    def __init__(self, a, out):
        env = dict(os.environ)
        for name in ('EE_TEST_TIME_FACTOR', 'EE_TEST_SEED', 'EE_TEST_NO_HUMAN_DEFEAT', 'EE_GX_NULL3D', 'EE_CONFIG',
                     'EE_DATA_ROOT'):
            env.pop(name, None)   # only this run's switches
        if a.time_factor:
            env['EE_TEST_TIME_FACTOR'] = a.time_factor
        if a.seed is not None:
            env['EE_TEST_SEED'] = str(a.seed)
        if a.no_render:
            env['EE_GX_NULL3D'] = str(a.render_key_ms)
        if not WINDOWS:
            env['EE_DATA_ROOT'] = str(STAGE)
        super().__init__(out, [str(STAGE / EXE)], env, cwd=str(STAGE), deadline=time.monotonic() + a.timeout)

    def capture(self, name):
        return fixture_driver.capture_frame(self, name)


def switches_taken(g, a):
    """The switches the game announced ("TEST UI clock factor ...", "TEST UI seed ...") -> evidence; raises when one
    asked for was not taken."""
    log = g.game_output()
    taken = [line for line in log.splitlines() if line.startswith('TEST UI ') and
             line.split()[2] in ('clock', 'seed', 'no', 'fixture')]
    if a.time_factor and not any(line.startswith('TEST UI clock factor') for line in taken):
        raise RuntimeError('the game did not take EE_TEST_TIME_FACTOR')
    if a.seed is not None and f'TEST UI seed {a.seed}' not in taken:
        raise RuntimeError('the game did not take EE_TEST_SEED')
    return taken


def gx_errors(g):
    """GX's wgpu errors in the game's debug output so far (debugstring lines) -> {first cause line: count}."""
    lines = g.game_output().splitlines()
    found = {}
    for k, line in enumerate(lines):
        if line.startswith('debugstring GX: wgpu error'):
            cause = next((v.strip() for v in lines[k + 1:k + 8] if v.startswith('    ')), line)
            found[cause] = found.get(cause, 0) + 1
    return found


def milestone(g, results, name, form, check=None, minimap=False, differ=None):
    """A milestone: the form on top, its frame captured (and checked: session_exercise.check_rendering). differ: the
    name of an earlier capture this one must differ from (a window that shows no new frame keeps its last picture)."""
    st = g.wait(lambda s: s['form'] == form, f'form {form} for {name}', 60)
    time.sleep(0.5)
    pixels = fixture_driver.capture_frame(g, name)
    if differ is not None:
        before = (g.out / f'{differ}.bmp').read_bytes()
        if pixels == before:   # a running game changes its picture (units, the clock), if only a little (0.2%)
            raise RuntimeError(f'{name}: the window shows the same picture as {differ} (no new frame presented); '
                               f'GX errors: {gx_errors(g)}')
    (g.out / f'{name}.txt').write_text(g.raw(), encoding='latin-1')
    row = dict(milestone=name, form=st['form'], command=st['command'])
    if check:
        from session_exercise import check_rendering
        row['rendering'] = check_rendering(pixels, name, minimap)
    results.append(row)
    (g.out / 'milestones.json').write_text(json.dumps(results, indent=2) + '\n')
    g.note(f'UI MILESTONE {name}: form {form}')
    return pixels


def trigger(g, form, control, then_form, what):
    """A click the fixture must execute ('triggered'), then the form expected on top."""
    seq = g.command(f'click {form} {control}')
    st = g.wait(lambda s: s['command'] >= seq and s['form'] == then_form, what, 60)
    end = time.monotonic() + 5
    while g.verdict(seq) is None and time.monotonic() < end:
        time.sleep(0.1)
    verdict = g.verdict(seq)
    if verdict != 'triggered':
        raise RuntimeError(f'{what}: the fixture says {verdict!r} for click {form} {control}')
    g.note(f'UI {what}: form {form} control {control} -> form {then_form}')
    return st


def input_check(g):
    """F3 (the game form's pause hot key) twice, posted to the game window as WM_KEYDOWN/UP: paused, then running
    again, read from the fixture's telemetry -> evidence; raises on failure (session_exercise.input_check's test)."""
    if not WINDOWS:
        raise RuntimeError('--input-check posts window messages: Windows only')
    from session_exercise import game_window, press_key
    hwnd = game_window(g.proc.pid)
    if not hwnd:
        raise RuntimeError('input check: no visible game window')

    def paused():
        return g.state()['telemetry'].get('paused')
    end = time.monotonic() + 30
    while paused() and time.monotonic() < end:
        fixture_driver.dismiss(g, g.state())
        time.sleep(1)
    states = [paused()]
    for _ in range(2):
        press_key(hwnd, 0x72, 0x3d)   # VK_F3, scan code DIK_F3
        end = time.monotonic() + 15
        while time.monotonic() < end:
            time.sleep(0.5)
            now = paused()
            if now != states[-1]:
                break
        states.append(now)
    ok = states == [0, 1, 0]
    g.note(f'INPUT CHECK F3 paused {states[0]} -> {states[1]} -> {states[2]}: {"ok" if ok else "FAILED"}')
    if not ok:
        raise RuntimeError(f'input check failed: paused states {states}')
    return dict(key='F3', paused=states, ok=ok)


def tour(g, a, summary):
    results = summary['milestones']
    if a.exercise == 'tour':
        trigger(g, MAIN, 3, SETTINGS, 'game settings')
        milestone(g, results, 'settings-video', SETTINGS)
        trigger(g, SETTINGS, 2, SETTINGS, 'audio tab')
        milestone(g, results, 'settings-audio', SETTINGS)
        trigger(g, SETTINGS, 3, SETTINGS, 'interface tab')
        milestone(g, results, 'settings-interface', SETTINGS)
        trigger(g, SETTINGS, 19, MAIN, 'settings closed')
        milestone(g, results, 'settings-closed', MAIN)
    trigger(g, MAIN, 0, SINGLE, 'single player')
    milestone(g, results, 'single-player', SINGLE)
    trigger(g, SINGLE, 1, LOBBY, 'random map setup')
    milestone(g, results, 'random-map-setup', LOBBY)
    time.sleep(5)   # local-server readiness and queued lobby updates
    seq = g.command(f'click {LOBBY} 10')
    g.note('UI ACTION Start Game queued')
    g.wait(lambda s: s['command'] >= seq and s['form'] == GAME, 'the game form', 150)
    tel = fixture_driver.wait_running(g, timeout=60)
    if tel['world_players'] < 2:
        raise RuntimeError(f'only {tel["world_players"]} world players')
    summary['simulation'] = tel
    milestone(g, results, 'gameplay', GAME, check=True, differ='random-map-setup')
    g.note('GAME LAUNCHED: world rendered; physics ticks advancing')
    if a.play_seconds:
        played = fixture_driver.play(g, a, ai_game=False)
        summary['play'] = {k: v for k, v in played.items() if k != 'samples'}
        time.sleep(2)
        fixture_driver.dismiss(g, g.state())   # the form may open between the last sample and the capture
        milestone(g, results, 'play', GAME, check=True, minimap=a.play_seconds >= 10, differ='gameplay')
        if a.input_check:
            summary['input'] = input_check(g)
    (g.out / 'rendering.json').write_text(json.dumps([r['rendering'] for r in results if 'rendering' in r],
                                                     indent=2) + '\n')
    summary['result'] = 'PASS'


def ai_game(g, a, summary):
    n = a.ai_players
    g.click(MAIN, 0, SINGLE, 'single player')
    g.click(SINGLE, 1, LOBBY, 'random map setup')
    summary['lobby'] = fixture_driver.setup_lobby(g, a)
    if not a.no_render:   # the null 3D device presents nothing a window capture sees: no captures then
        try:
            fixture_driver.capture_frame(g, 'ai-lobby')
        except RuntimeError as error:   # evidence only
            g.note(f'CAPTURE ai-lobby skipped: {error}')
    seq = g.command(f'click {LOBBY} 10')
    g.note('UI ACTION Start Game queued')
    g.wait(lambda s: s['command'] >= seq and s['form'] == GAME, 'the game form', 600)
    summary['civ_preempt'] = fixture_driver.civ_preempt(g)
    fixture_driver.wait_running(g, n + 2)
    if not a.no_render:
        try:
            fixture_driver.capture_frame(g, 'gameplay')
        except RuntimeError as error:
            g.note(f'CAPTURE gameplay skipped: {error}')
    played = fixture_driver.play(g, a, ai_game=True)
    time.sleep(3)   # as ai_game.py: the final sample 3 s after the stop
    final = g.state()['telemetry']
    local = final.get('local')
    rows = [p for p in final['players'] if p['index'] >= 1]
    outcome = {'max-game-seconds': 'time-cap', 'game-over': 'game-end', 'human-result': 'game-end'}.get(
        played['outcome'], played['outcome'])
    summary.update(
        outcome=outcome, final_pt=final.get('ticks'), final_ct=final.get('ct'), game_ms=final.get('game_ms'),
        game_over=final.get('over'), local_player=local, paused=final.get('paused'), clock=final.get('clock'),
        winners=[p['index'] for p in rows if p['result'] == 1], defeated=[p['index'] for p in rows if p['result'] == 2],
        players=[dict(index=p['index'], human=p['index'] == local, result=p['result'], units=p['population'],
                      victory_units=p['victory_units'], resources=p.get('stock')) for p in rows],
        alive_computers=[p['index'] for p in rows if p['index'] != local and p['victory_units'] > 0 and p['result'] != 2],
        defeat_order=played['defeat_order'], forms=played['forms'], wall_seconds=played['wall_seconds'])
    g.note(f'AI GAME {outcome}: PT {summary["final_pt"]} game s {(summary["game_ms"] or 0) / 1000:.0f} winners '
           f'{summary["winners"]} defeated {summary["defeated"]} units '
           + ' '.join(f'{p["index"]}:{p["units"]}/{p["victory_units"]}' for p in summary['players']))
    summary['result'] = 'PASS'


def run(a):
    out = pathlib.Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    tree = data_tree(a)
    pins = stage(a, tree)
    g = Game(a, out)
    summary = dict(exercise=a.exercise, exe=str(a.exe), data=str(tree), config=pins, started=time.strftime('%Y-%m-%d %H:%M:%S'),
                   time_factor=a.time_factor, seed=a.seed, no_render=a.no_render, result='FAIL', milestones=[])
    try:
        g.wait(lambda s: s['form'] == MAIN, 'the main menu', 180)
        summary['switches'] = switches_taken(g, a)
        g.note('MAIN MENU reached')
        time.sleep(2)
        milestone(g, summary['milestones'], 'main-menu', MAIN)
        if a.exercise == 'menu':
            summary['result'] = 'PASS'
        elif a.exercise in ('tour', 'random-game'):
            tour(g, a, summary)
        else:
            ai_game(g, a, summary)
    except Exception as e:
        summary['error'] = str(e)
        g.note(f'FAIL {e}')
    finally:
        g.stop()
        try:
            summary['game_log'] = fixture_driver.copy_game_logs(STAGE, out)
        except OSError as error:
            summary['game_log_error'] = str(error)
        if a.exercise == 'ai-game' and summary.get('game_log') and summary['result'] == 'PASS':
            kinds = [p.get('kind') for p in summary['game_log']['players'].values()]
            if kinds.count('computer') != a.ai_players:
                summary['result'] = 'FAIL'
                summary['error'] = f'the game log names {kinds.count("computer")} computer players: {kinds}'
        try:
            summary['gx_errors'] = gx_errors(g)
        except OSError:
            pass
        summary['log'] = g.log_lines
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    line = f'RESULT {summary["result"]} {a.exercise}'
    if a.exercise == 'ai-game' and 'outcome' in summary:
        line += (f' outcome {summary["outcome"]} PT {summary["final_pt"]} game_s {(summary["game_ms"] or 0) // 1000}'
                 f' units {[p["units"] for p in summary["players"]]} alive_computers {summary["alive_computers"]}')
    if summary.get('error'):
        line += f' error: {summary["error"]}'
    print(f'EVIDENCE {out}', flush=True)
    print(line, flush=True)
    return summary


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split('\n', 2)[2])
    ap.add_argument('--exe', type=pathlib.Path, default=ROOT / 'build' / 'clang-test' / 'exe' / 'EE-AOC.exe',
                    help='the game (default: the test-profile build)')
    ap.add_argument('--data', help='the unified open data tree to stage (default: build/open_data/tree)')
    ap.add_argument('--tree', choices=['open'], help='(kept for old command lines: open is the only tree)')
    ap.add_argument('--out', default=str(ROOT / 'build' / 'fixture_game' / time.strftime('%Y%m%d-%H%M%S')))
    ap.add_argument('--exercise', choices=['menu', 'tour', 'random-game', 'ai-game'], default='tour')
    ap.add_argument('--timeout', type=float, default=1800, help='wall seconds the whole run may take')
    ap.add_argument('--config', action='append', default=[], help='block.key=VALUE in the fresh config.yaml')
    ap.add_argument('--play-seconds', type=float, default=None, help='wall seconds of play after the start')
    ap.add_argument('--input-check', action='store_true', help='tour/random-game with --play-seconds: F3 twice')
    ap.add_argument('--ai-players', type=int, default=2)
    ap.add_argument('--map-size', default='Small')
    ap.add_argument('--map-type', default='Continental')
    ap.add_argument('--difficulty', default='Easy')
    ap.add_argument('--game-speed', default='Very Fast')
    ap.add_argument('--lobby', action='append', default=[], metavar='CONTROL=VALUE', help='ai-game: any lobby combo')
    ap.add_argument('--lobby-check', action='append', default=[], metavar='CONTROL=0|1', help='ai-game: a check box')
    ap.add_argument('--max-game-seconds', type=float, default=0, help='ai-game: stop at this game time (time-cap)')
    ap.add_argument('--time-factor', help='the harness clock: the game runs F (N/D, a decimal) x real time')
    ap.add_argument('--no-render', action='store_true', help="GX's null 3D device (EE_GX_NULL3D)")
    ap.add_argument('--render-key-ms', type=int, default=1000, help='with --no-render: one drawn scene every MS real ms')
    ap.add_argument('--seed', type=lambda v: int(v, 0), help='the start seed (EE_TEST_SEED)')
    ap.add_argument('--pt-rate', type=int, help='pin the PT rate (network.pt_rate.min = max = N)')
    a = ap.parse_args()
    if not 1 <= a.ai_players < VISIBLE_SLOTS:
        ap.error('--ai-players must be 1..7')
    if a.seed is not None and not 0 <= a.seed <= 0xffffffff:
        ap.error('--seed must fit in 32 bits')
    if a.time_factor:
        sys.path.insert(0, str(HERE))
        import timeshim
        try:
            timeshim.parse_factor(a.time_factor)
        except ValueError as error:
            ap.error(str(error))
    if a.input_check and not a.play_seconds:
        ap.error('--input-check needs --play-seconds')
    if a.exercise in ('tour', 'random-game') and a.play_seconds is None:
        a.play_seconds = 0
    a.exe = a.exe.resolve()
    if not a.exe.exists():
        ap.error(f'{a.exe} does not exist: python -I build.py --test-profile windowed-software')
    return 0 if run(a)['result'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
