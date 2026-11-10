"""TEST ONLY: a LAN game between two instances of the game on this machine, through the WON lobby's own UI.

The host and the joiner are the test-profile exe started directly under the UI fixture (fixture_driver.Game;
EETestFixture.cpp, its lobby hooks: "lobby ..." / "wonctl ..." / "wonlist ..." lines in ui-state.txt and the
"wonclick" / "wonkey" / "wonselect" commands, run on the lobby's own message loop). Nothing reads the game's memory.
mp_game.py bypasses the lobby; this driver does not: the host opens MultiPlayer -> Other Multiplayer (the LAN
screen) -> Host Game -> a name -> Create; the joiner opens the LAN screen, waits for the game in the LAN list (the
discovery broadcasts), selects it, Join, Ready; the host presses Start; both go through the connect dialog into the
game, and the game must run (physics ticks advancing in both, the same player count) for --play-seconds.

  python -I tools/harness/lan_game.py [--exe EXE] [--data TREE] [--out DIR] [--play-seconds 60] [--timeout 900]

Evidence: --out (default build/lan_game/<time>): host/ and joiner/ (game.log, ui/, the frames), summary.json; the
last line printed is "RESULT PASS|FAIL lan-game ...". Exit 0 on PASS. Two game processes of this machine's: run it
only when no other game runs.
"""
import argparse
import json
import os
import pathlib
import re
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import fixture_driver
import fixture_game
from ai_game import GAME
from fixture_driver import MAIN

MULTIPLAYER = 1              # main menu: 'MultiPlayer' (EEUIEMSEMultiPlayer -> EEStartExternalLobby)
GAME_SCREEN = 30             # the form EELobbyShowGameScreen pushes when the lobby game starts
# WONLobbyResourceGlobals.h LobbyResourceCtrlId (200000 + n) and WONLobbyGUI.h ControlIdType
ID_LAN, ID_LOGOUT, ID_HOST, ID_JOIN, ID_LEAVE, ID_START, ID_REFRESH = 200009, 200010, 200011, 200013, 200014, 200015, 200016
ID_ADD_LAN_NAME = 200019
ID_OK, ID_CANCEL = 100001, 100002
TAB_DIRECT, TAB_GAME, TAB_OPTIONS = 1, 2, 3   # the LAN screen's tab buttons (WONTabCtrl: ids by index; the LAN tab is 0)
LOBBY_KEYS = ('shown', 'screen', 'lan', 'login', 'modal', 'game', 'host', 'clients', 'ready', 'allready', 'name')


def parse_lobby(text):
    """The fixture's lobby lines -> dict(open, lobby={...}, ctls={id: dict(x, y, w, h, enabled, text)}, lists=[...])"""
    out = dict(open=False, lobby=None, ctls={}, lists=[])
    for line in text.splitlines():
        p = line.split()
        try:
            if p[:2] == ['lobbyopen', '1']:
                out['open'] = True
            elif p[:1] == ['lobby'] and len(p) >= 23:
                row = {}
                for k in range(1, 23, 2):
                    row[p[k]] = p[k + 1] if p[k] == 'name' else int(p[k + 1])
                out['lobby'] = row
            elif p[:1] == ['wonctl'] and len(p) >= 8:
                out['ctls'][int(p[1])] = dict(x=int(p[2]), y=int(p[3]), w=int(p[4]), h=int(p[5]), enabled=int(p[6]),
                                              text=p[7])
            elif p[:1] == ['wonlist'] and len(p) >= 6:
                out['lists'].append(dict(items=int(p[3]), sel=int(p[5])))
            elif p[:1] == ['wontext'] and len(p) >= 2:
                out.setdefault('texts', []).append(p[1])
        except (IndexError, ValueError):
            pass
    return out


class End(fixture_game.Game):
    """One end of the LAN game: the staged exe under the fixture, its evidence in out/<label>."""

    def __init__(self, a, out, label):
        self.label = label
        os.environ['EE_TEST_INSTANCE'] = label   # its own single-instance mutex (EECreateInstanceMutex, TEST ONLY)
        super().__init__(a, pathlib.Path(out) / label)

    def note(self, text):
        super().note(f'[{self.label}] {text}')

    def lobby(self):
        return parse_lobby(self.raw())

    def wait_lobby(self, pred, what, timeout=60):
        end = time.monotonic() + timeout
        while True:
            self.check_alive()
            st = self.lobby()
            if pred(st):
                return st
            if time.monotonic() > end:
                lines = [v for v in self.raw().splitlines()
                         if v.split(' ')[0] in ('lobbyopen', 'lobby', 'wonctl', 'wonlist', 'wontext', 'form', 'stack')]
                raise RuntimeError(f'[{self.label}] timeout waiting for {what}; the lobby state:\n' + '\n'.join(lines))
            time.sleep(0.25)

    def lobby_verdict(self, seq, timeout=15):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            self.check_alive()
            found = re.findall(rf'^TEST UI (lobby-\S+) .*\(seq {seq}, top form', self.game_output(), re.M)
            done = [v for v in found if v != 'lobby-posted']
            if done:
                return done[-1]
            if found and self.proc.poll() is not None:
                break
            time.sleep(0.2)
        return None

    def wonclick(self, control, what, timeout=60):
        """Waits for the control (visible, enabled), clicks it through the lobby's event queue."""
        self.wait_lobby(lambda s: s['ctls'].get(control, {}).get('enabled') == 1, f'control {control} ({what}) enabled', timeout)
        seq = self.command(f'wonclick {control} 0')
        verdict = self.lobby_verdict(seq)
        if verdict != 'lobby-clicked':
            raise RuntimeError(f'[{self.label}] {what}: the lobby says {verdict!r} for wonclick {control}')
        self.note(f'LOBBY {what}: control {control} clicked')
        return seq

    def wonclickat(self, x, y, what):
        seq = self.command(f'wonclickat {x} {y}')
        verdict = self.lobby_verdict(seq)
        if verdict != 'lobby-clicked':
            raise RuntimeError(f'[{self.label}] {what}: the lobby says {verdict!r} for wonclickat {x} {y}')
        self.note(f'LOBBY {what}: clicked at {x},{y}')

    def wontype(self, text, what):
        for ch in text:
            seq = self.command(f'wonkey {ord(ch)} 0')
            verdict = self.lobby_verdict(seq)
            if verdict != 'lobby-typed':
                raise RuntimeError(f'[{self.label}] {what}: the lobby says {verdict!r} for wonkey {ord(ch)}')
        self.note(f'LOBBY {what}: typed {text!r}')

    def wonselect(self, index, item, what):
        seq = self.command(f'wonselect {index} {item}')
        verdict = self.lobby_verdict(seq)
        if verdict != 'lobby-selected':
            raise RuntimeError(f'[{self.label}] {what}: the lobby says {verdict!r} for wonselect {index} {item}')
        self.note(f'LOBBY {what}: list {index} item {item} selected')

    def capture(self, name):
        try:
            return fixture_driver.capture_frame(self, f'{self.label}-{name}')
        except RuntimeError as error:   # evidence only
            self.note(f'CAPTURE {name} skipped: {error}')
            return None


def open_lan_screen(g):
    """Main menu -> MultiPlayer -> the lobby -> Other Multiplayer (the LAN screen)."""
    g.wait(lambda s: s['form'] == MAIN, 'the main menu', 180)
    g.note('MAIN MENU reached')
    seq = g.command(f'click {MAIN} {MULTIPLAYER}')
    g.wait(lambda s: s['command'] >= seq, 'the MultiPlayer click', 30)
    g.wait_lobby(lambda s: s['lobby'] is not None and s['lobby']['shown'] == 1, 'the lobby shown', 120)
    g.note('LOBBY shown')
    time.sleep(2)
    g.capture('lobby-open')
    g.wonclick(ID_LAN, 'Other Multiplayer', 90)
    st = g.wait_lobby(lambda s: s['lobby'] is not None and s['lobby']['lan'] == 1, 'the LAN screen', 60)
    g.note(f'LOBBY LAN screen: lists {st["lists"]}')
    time.sleep(1)
    g.capture('lan-screen')


def set_lan_name(g, name):
    """The LAN player name through the Options tab (Add -> the name dialog -> OK), then back to the LAN tab.
    The default name carries a number made from the local IP address, so both instances start with the same one
    and the host refuses the joiner ("already being used by another player")."""
    g.wonclick(TAB_OPTIONS, 'the Options tab')
    g.wonclick(ID_ADD_LAN_NAME, 'Add (LAN name)')
    g.wait_lobby(lambda s: s['lobby'] is not None and s['lobby']['modal'] == 1 and ID_OK in s['ctls'], 'the LAN name dialog', 30)
    g.wontype(name, 'the LAN name')
    g.wonclick(ID_OK, 'OK (LAN name)')
    st = g.wait_lobby(lambda s: s['lobby'] is not None and s['lobby']['modal'] == 0 and TAB_DIRECT in s['ctls'],
                      'the name dialog closed', 30)
    tab = st['ctls'][TAB_DIRECT]   # the LAN tab (id 0: not listed) sits left of the Direct Connect tab
    g.wonclickat(tab['x'] - tab['w'] // 2, tab['y'] + tab['h'] // 2, 'the LAN tab')
    g.wait_lobby(lambda s: s['ctls'].get(ID_HOST, {}).get('enabled') == 1, 'the LAN tab shown', 30)
    g.note(f'LOBBY LAN name set to {name!r}')


def host_game(g, name):
    g.wonclick(ID_HOST, 'Host Game')
    g.wait_lobby(lambda s: s['lobby'] is not None and s['lobby']['modal'] == 1 and ID_OK in s['ctls'], 'the Create Game dialog', 30)
    g.capture('create-dialog')
    g.wontype(name, 'the game name')
    g.wonclick(ID_OK, 'Create')
    st = g.wait_lobby(lambda s: s['lobby'] is not None and s['lobby']['game'] == 1 and s['lobby']['host'] == 1, 'the hosted game', 60)
    g.note(f'LOBBY hosting {st["lobby"]["name"]!r}: clients {st["lobby"]["clients"]}')
    time.sleep(1)
    g.capture('staging-host')


def join_game(g, name, timeout):
    """Waits for the LAN list to show a game (refreshing every 20 s), selects it and joins."""
    end = time.monotonic() + timeout
    last_refresh = time.monotonic()
    while True:
        g.check_alive()
        st = g.lobby()
        lists = [(k, v) for k, v in enumerate(st['lists']) if v['items'] > 0]
        if lists:
            break
        if time.monotonic() > end:
            raise RuntimeError(f'[{g.label}] no LAN game appeared in {timeout} s; lists {st["lists"]}')
        if time.monotonic() - last_refresh > 20 and st['ctls'].get(ID_REFRESH, {}).get('enabled') == 1:
            last_refresh = time.monotonic()
            g.wonclick(ID_REFRESH, 'Refresh List')
        time.sleep(1)
    index, found = lists[0]
    g.note(f'LOBBY LAN list {index} shows {found["items"]} game(s)')
    g.capture('lan-list')
    g.wonselect(index, 0, 'the first LAN game')
    g.wonclick(ID_JOIN, 'Join Game')
    st = g.wait_lobby(lambda s: s['lobby'] is not None and s['lobby']['game'] == 1, 'the joined game', 90)
    if name and st['lobby']['name'] != name:
        raise RuntimeError(f'[{g.label}] joined {st["lobby"]["name"]!r}, not {name!r}')
    g.note(f'LOBBY joined {st["lobby"]["name"]!r}: clients {st["lobby"]["clients"]}')
    time.sleep(1)
    g.capture('staging-joiner')


def run(a):
    out = pathlib.Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    tree = fixture_game.data_tree(a)
    pins = fixture_game.stage(a, tree)
    summary = dict(exercise='lan-game', exe=str(a.exe), data=str(tree), config=pins, started=time.strftime('%Y-%m-%d %H:%M:%S'),
                   result='FAIL', game_name=a.game_name)
    ends = []
    try:
        host = End(a, out, 'host')
        ends.append(host)
        time.sleep(3)   # the game log's name is the start second
        joiner = End(a, out, 'joiner')
        ends.append(joiner)
        open_lan_screen(host)
        open_lan_screen(joiner)
        set_lan_name(host, 'lanhost')
        set_lan_name(joiner, 'lanjoiner')
        host_game(host, a.game_name)
        join_game(joiner, a.game_name, a.discovery_timeout)
        st = host.wait_lobby(lambda s: s['lobby'] is not None and s['lobby']['clients'] >= 2, 'the joiner in the host\'s game', 60)
        host.note(f'LOBBY the game has {st["lobby"]["clients"]} clients')
        summary['staging'] = dict(host=st['lobby'], joiner=joiner.lobby()['lobby'])
        joiner.wonclick(ID_START, 'Ready')
        joiner.wait_lobby(lambda s: s['lobby'] is not None and s['lobby']['ready'] == 1, 'the joiner ready', 30)
        host.wait_lobby(lambda s: s['lobby'] is not None and s['lobby']['allready'] == 1, 'everyone ready', 30)
        host.capture('staging-ready')
        host.wonclick(ID_START, 'Start Game')
        for g in ends:
            # the lobby's snapshot stops with its loop: the game screen (form 30, EELobbyShowGameScreen) is pushed
            # before the lobby hides, then the game's own form once it runs
            g.wait(lambda s: GAME_SCREEN in s['stack'] or s['form'] == GAME, 'the game screen (the lobby leaving)', 180)
            g.note(f'GAME screen; forms {g.state()["stack"]}')
        tel = {}
        for g in ends:
            tel[g.label] = fixture_driver.wait_running(g, timeout=240)
        summary['start'] = tel
        if tel['host']['world_players'] != tel['joiner']['world_players']:
            raise RuntimeError(f'the ends see different player counts: {tel}')
        if tel['host']['world_players'] < 3:   # the two humans and Gaia
            raise RuntimeError(f'only {tel["host"]["world_players"]} world players')
        for g in ends:
            g.capture('game')
        start = time.monotonic()
        samples = []
        last = {g.label: g.state()['telemetry'] for g in ends}
        stalled = {g.label: time.monotonic() for g in ends}
        while time.monotonic() - start < a.play_seconds:
            time.sleep(5)
            row = dict(wall_s=round(time.monotonic() - start, 1))
            for g in ends:
                g.check_alive()
                st = g.state()
                fixture_driver.dismiss(g, st)
                t = st['telemetry']
                row[g.label] = dict(pt=t.get('ticks'), ct=t.get('ct'), game_ms=t.get('game_ms'), over=t.get('over'),
                                    multiplayer=t.get('multiplayer'), form=st['form'])
                if t.get('ticks') != last[g.label].get('ticks'):
                    stalled[g.label] = time.monotonic()
                elif time.monotonic() - stalled[g.label] > 60:
                    raise RuntimeError(f'[{g.label}] physics stalled for 60 s at PT {t.get("ticks")}')
                last[g.label] = t
                if t.get('over'):
                    raise RuntimeError(f'[{g.label}] the game ended at PT {t.get("ticks")}')
            samples.append(row)
            host.note(f'PLAY t+{row["wall_s"]:.0f}s host PT {row["host"]["pt"]} CT {row["host"]["ct"]} joiner PT '
                      f'{row["joiner"]["pt"]} CT {row["joiner"]["ct"]}')
        (out / 'samples.json').write_text(json.dumps(samples, indent=1) + '\n')
        summary['final'] = {g.label: g.state()['telemetry'] for g in ends}
        for g in ends:
            g.capture('play')
        for g in ends:
            if summary['final'][g.label].get('ticks', 0) <= tel[g.label]['ticks']:
                raise RuntimeError(f'[{g.label}] no physics tick during the play')
            if not summary['final'][g.label].get('multiplayer'):
                raise RuntimeError(f'[{g.label}] the communications are not a multiplayer session')
        summary['result'] = 'PASS'
    except Exception as e:
        summary['error'] = str(e)
        print(f'FAIL {e}', flush=True)
    finally:
        for g in ends:
            g.stop()
        try:
            summary['game_logs'] = [str(p) for p in (fixture_game.STAGE / 'Logs').glob('game-*.log')]
            fixture_driver.copy_game_logs(fixture_game.STAGE, out)
        except OSError as error:
            summary['game_log_error'] = str(error)
        summary['log'] = sum((g.log_lines for g in ends), [])
        summary['gx_errors'] = {g.label: fixture_game.gx_errors(g) for g in ends}
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    line = f'RESULT {summary["result"]} lan-game'
    if 'final' in summary:
        line += ' PT ' + ' '.join(f'{k}={v.get("ticks")}' for k, v in summary['final'].items())
    if summary.get('error'):
        line += f' error: {summary["error"]}'
    print(f'EVIDENCE {out}', flush=True)
    print(line, flush=True)
    return summary


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split('\n', 2)[2])
    ap.add_argument('--exe', type=pathlib.Path, default=ROOT / 'build' / 'clang-test' / 'exe' / 'EE-AOC.exe')
    ap.add_argument('--data', help='the unified open data tree to stage (default: build/open_data/tree)')
    ap.add_argument('--out', default=str(ROOT / 'build' / 'lan_game' / time.strftime('%Y%m%d-%H%M%S')))
    ap.add_argument('--timeout', type=float, default=1200, help='wall seconds the whole run may take')
    ap.add_argument('--config', action='append', default=[], help='block.key=VALUE in the fresh config.yaml')
    ap.add_argument('--play-seconds', type=float, default=60, help='wall seconds of play after both ends run')
    ap.add_argument('--discovery-timeout', type=float, default=120, help='seconds for the LAN list to show the game')
    ap.add_argument('--game-name', default='lantest')
    ap.add_argument('--pt-rate', type=int, help='pin the PT rate (network.pt_rate.min = max = N)')
    a = ap.parse_args()
    a.time_factor = None
    a.seed = None
    a.no_render = False
    a.render_key_ms = 1000
    a.exe = a.exe.resolve()
    if not a.exe.exists():
        ap.error(f'{a.exe} does not exist: python -I build.py --test-profile windowed-software')
    return 0 if run(a)['result'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
