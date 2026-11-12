"""TEST ONLY: the core of the live-game drivers that work through the game's own UI fixture
(src/Empire Earth/Empire Earth/EETestFixture.cpp, on when EE_TEST_UI names a directory): posix_game.py (Linux,
macOS) and fixture_game.py (Windows, the gates).

Nothing here reads the game's memory or names a symbol: the game writes <ui>/ui-state.txt every 250 ms (the form
stack, the controls, the game telemetry) and executes the newest line of <ui>/test.command through the controls' own
methods; <ui>/frame.request asks for the next presented frame as <ui>/frame.bmp. The game log (Logs/game-*.log,
EE-GAME lines) is the second source. The lobby options, the dismiss table and the state parser are ai_game.py's.

  Game(out, argv, env, cwd=None)   the game process with its fixture directory out/ui and its output out/game.log
  parse_telemetry(text)            the fixture's telemetry lines -> dict
  setup_lobby(g, a), dismiss(g, st), civ_preempt(g), wait_running(g, ...), play(g, a, ...)   the shared exercises
  frame(g, name)                   the next presented frame -> BMP bytes (also out/<name>.bmp)
  window_frame(g, name)            Windows: the game window's content (PrintWindow) -> BMP bytes
  capture_frame(g, name)           window_frame on Windows, frame elsewhere
  game_log(directory)              the newest EE-GAME log there, summarised
"""
import json
import os
import pathlib
import re
import shutil
import signal
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from ai_game import CITIZENS, DEFAULTS, DISMISS, LOBBY, GAME, NAMED, PLAYER_TYPE, TEAM, VISIBLE_SLOTS, parse_state, resolve

MAIN, SINGLE, SETTINGS = 12, 6, 5
SETTINGS_BUTTON, SETTINGS_OK = 3, 18   # main menu 'Game Settings Button', settings 'GS OK Button'
GAME_SPEED = 209
WINDOWS = sys.platform == 'win32'


def parse_telemetry(text):
    """The fixture's telemetry lines (EETestFixture.cpp WriteTelemetry). ticks are the completed physics ticks (PTs),
    ct the communications ticks processed; population is EEPlayer::mPopulationCount (the units the old harness read as
    mLB14), victory_units mEndOfGameCount (mLB18)."""
    t = dict(players=[])
    for line in text.splitlines():
        p = line.split()
        try:
            if p[:2] == ['game', 'ms'] and len(p) >= 13:
                t.update(game_ms=int(p[2]), ticks=int(p[4]), over=int(p[6]), world_players=int(p[8]), map=int(p[10]),
                         local=int(p[12]))
            elif p[:2] == ['comm', 'ct'] and len(p) >= 9:
                t.update(ct=int(p[2]), last_tick=int(p[4]), paused=int(p[6]), multiplayer=int(p[8]))
            elif p[:1] == ['player'] and len(p) >= 10:
                row = dict(index=int(p[1]), observer=int(p[3]), result=int(p[5]), population=int(p[7]),
                           victory_units=int(p[9]))
                if len(p) >= 15 and p[10] == 'civ' and p[13] == 'stock':
                    row.update(civ_locked=int(p[11]), civ_chosen=int(p[12]), stock=[int(v) for v in p[14].split('/')])
                t['players'].append(row)
            elif p[:1] == ['clock'] and len(p) >= 6:
                num, _, den = p[1].partition('/')
                t.update(clock=[int(num), int(den or 1)], clock_ms=int(p[3]), frames=int(p[5]))
            elif p[:1] == ['camera'] and len(p) == 16 and p[4] == 'target' and p[14] == 'size':
                # "camera <x> <y> <z> target <x> <y> zoom <z> mode <m> valid <0|1> <0|1> size <n>": the focus now and
                # the move's target (tiles), whether each lies on the map's mesh, the map's tiles per side
                t['camera'] = dict(x=float(p[1]), y=float(p[2]), z=float(p[3]), tx=float(p[5]), ty=float(p[6]),
                                   zoom=float(p[8]), mode=int(p[10]), valid=int(p[12]), valid_target=int(p[13]),
                                   size=int(p[15]))
        except (IndexError, ValueError):
            pass
    return t


def _windows_dialogs(pid):
    """The visible message boxes (class #32770) of process pid -> [their text]"""
    import ctypes
    import ctypes.wintypes as w
    user = ctypes.windll.user32
    found = []

    @ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    def visit(hwnd, _):
        owner = w.DWORD()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        name = ctypes.create_unicode_buffer(64)
        user.GetClassNameW(hwnd, name, 64)
        if owner.value == pid and name.value == '#32770' and user.IsWindowVisible(hwnd):
            texts = []

            @ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
            def child(h, _):
                buffer = ctypes.create_unicode_buffer(512)
                user.GetWindowTextW(h, buffer, 512)
                if buffer.value:
                    texts.append(buffer.value)
                return True
            user.EnumChildWindows(hwnd, child, 0)
            found.append(' | '.join(texts))
        return True
    user.EnumWindows(visit, 0)
    return found


class Game:
    """The game process and its fixture. out: the evidence directory (absolute: the game changes into its data
    root); argv/env/cwd: how to start it (EE_TEST_UI is added); deadline: a monotonic time the run must end by."""

    def __init__(self, out, argv, env, cwd=None, deadline=None):
        self.out = pathlib.Path(out).resolve()
        self.ui = self.out / 'ui'
        self.ui.mkdir(parents=True, exist_ok=True)
        for f in self.ui.iterdir():
            f.unlink()
        self.seq = 0
        self.log_lines = []
        self.deadline = deadline
        env = dict(env, EE_TEST_UI=str(self.ui))
        self.log = open(self.out / 'game.log', 'wb')
        if WINDOWS:
            import ctypes
            # no Windows Error Reporting dialog for a crash: the process ends and the driver reports it (inherited)
            ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
            self.proc = subprocess.Popen(argv, env=env, cwd=cwd, stdout=self.log, stderr=subprocess.STDOUT,
                                         creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
        else:
            self.proc = subprocess.Popen(argv, env=env, cwd=cwd, stdout=self.log, stderr=subprocess.STDOUT,
                                         start_new_session=True)
        self.note(f'START {argv[0]} pid {self.proc.pid}')

    def note(self, text):
        line = f'{time.strftime("%H:%M:%S")} {text}'
        print(line, flush=True)
        self.log_lines.append(line)

    def alive(self):
        return self.proc.poll() is None

    def game_output(self):
        return (self.out / 'game.log').read_bytes().decode('latin-1')

    def check_alive(self):
        # a failed verification (a db load error at startup, ...) fails the run even while the process lingers
        # (stopped in a debugger, a crash reporter or a message box)
        log = self.game_output()
        if 'Verification failure' in log:
            line = next(v for v in log.splitlines() if 'Verification failure' in v)
            raise RuntimeError(f'the game failed a verification: {line}')
        if not self.alive():
            tail = log[-3000:]
            raise RuntimeError(f'the game exited with {self.proc.returncode} ({self.proc.returncode & 0xffffffff:#x}); '
                               f'log tail:\n{tail}')
        if WINDOWS:
            dialogs = _windows_dialogs(self.proc.pid)
            if dialogs:
                raise RuntimeError(f'the game shows a message box: {dialogs}')
        if self.deadline is not None and time.monotonic() > self.deadline:
            raise RuntimeError('the session timeout ran out')

    def raw(self):
        for _ in range(20):   # Windows: the fixture's rename can briefly hold the file
            try:
                return (self.ui / 'ui-state.txt').read_text(encoding='latin-1')
            except FileNotFoundError:
                return ''
            except OSError:
                time.sleep(0.05)
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

    def verdict(self, seq):
        """The fixture's verdict on command seq ("TEST UI <verdict> <op> ..." on stderr), None before it ran."""
        found = re.findall(rf'^TEST UI (\S+) .*\(seq {seq}, top form', self.game_output(), re.M)
        return found[-1] if found else None

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
        """A capture of the game window; the drivers override it (macOS: screencapture; Windows: the fixture's frame)"""
        self.note(f'CAPTURE {name} skipped (no capture on this platform)')
        return None

    def stop(self):
        if self.alive():
            if WINDOWS:
                self.proc.kill()   # TerminateProcess: the game keeps nothing the run needs
            else:
                os.killpg(self.proc.pid, signal.SIGTERM)
            try:
                self.proc.wait(10)
            except subprocess.TimeoutExpired:
                if not WINDOWS:
                    os.killpg(self.proc.pid, signal.SIGKILL)
                self.proc.wait()
        self.log.close()


def frame(g, name, timeout=15):
    """The next frame the game presents after this call, through the fixture (frame.request -> frame.bmp) -> BMP bytes,
    saved as out/<name>.bmp. Raises when no frame comes or it is blank."""
    target = g.ui / 'frame.bmp'
    target.unlink(missing_ok=True)
    before = g.state()['telemetry'].get('frames', 0)
    (g.ui / 'frame.request').write_bytes(b'')
    end = time.monotonic() + timeout
    while True:
        g.check_alive()
        if target.exists() and g.state()['telemetry'].get('frames', 0) > before:
            try:
                pixels = target.read_bytes()
            except OSError:
                pixels = b''
            if len(pixels) >= 54 and pixels[:2] == b'BM' and int.from_bytes(pixels[2:6], 'little') == len(pixels):
                break
        if time.monotonic() > end:
            raise RuntimeError(f'{name}: no frame presented within {timeout} s')
        time.sleep(0.25)
    if len(set(pixels[54:])) < 16:
        raise RuntimeError(f'{name}: the frame is blank')
    (g.out / f'{name}.bmp').write_bytes(pixels)
    g.note(f'FRAME {name}: {int.from_bytes(pixels[18:22], "little")}x{abs(int.from_bytes(pixels[22:26], "little", signed=True))}')
    return pixels


def window_bmp(hwnd):
    """Windows: the client area of window hwnd as a 32-bit top-down BMP (PrintWindow with PW_CLIENTONLY |
    PW_RENDERFULLCONTENT: the composed swap chain, as on screen) -> bytes, None when it cannot."""
    import ctypes
    import ctypes.wintypes as w
    import struct
    user, gdi = ctypes.windll.user32, ctypes.windll.gdi32
    user.GetDC.restype = w.HDC; gdi.CreateCompatibleDC.restype = w.HDC; gdi.CreateDIBSection.restype = w.HBITMAP
    gdi.SelectObject.restype = w.HGDIOBJ
    gdi.CreateCompatibleDC.argtypes = [w.HDC]
    gdi.CreateDIBSection.argtypes = [w.HDC, ctypes.c_void_p, w.UINT, ctypes.POINTER(ctypes.c_void_p), w.HANDLE, w.DWORD]
    gdi.SelectObject.argtypes = [w.HDC, w.HGDIOBJ]
    gdi.DeleteObject.argtypes = [w.HGDIOBJ]; gdi.DeleteDC.argtypes = [w.HDC]
    user.ReleaseDC.argtypes = [w.HWND, w.HDC]; user.PrintWindow.argtypes = [w.HWND, w.HDC, w.UINT]
    user.GetClientRect.argtypes = [w.HWND, ctypes.POINTER(w.RECT)]
    r = w.RECT(); user.GetClientRect(w.HWND(hwnd), ctypes.byref(r))
    width, height = r.right - r.left, r.bottom - r.top
    if width <= 0 or height <= 0:
        return None
    screen = user.GetDC(None); dc = gdi.CreateCompatibleDC(screen)
    class BIH(ctypes.Structure):
        _fields_ = [('biSize', w.DWORD), ('biWidth', w.LONG), ('biHeight', w.LONG), ('biPlanes', w.WORD),
                    ('biBitCount', w.WORD), ('biCompression', w.DWORD), ('biSizeImage', w.DWORD),
                    ('biXPelsPerMeter', w.LONG), ('biYPelsPerMeter', w.LONG), ('biClrUsed', w.DWORD), ('biClrImportant', w.DWORD)]
    info = BIH(ctypes.sizeof(BIH), width, -height, 1, 32, 0, 0, 0, 0, 0, 0)
    bits = ctypes.c_void_p()
    bitmap = gdi.CreateDIBSection(dc, ctypes.byref(info), 0, ctypes.byref(bits), None, 0)
    old = gdi.SelectObject(dc, bitmap)
    ok = user.PrintWindow(w.HWND(hwnd), dc, 3)
    pixels = ctypes.string_at(bits, width * height * 4)
    gdi.SelectObject(dc, old); gdi.DeleteObject(bitmap); gdi.DeleteDC(dc); user.ReleaseDC(None, screen)
    if not ok:
        return None
    header = b'BM' + struct.pack('<IHHI', 54 + len(pixels), 0, 0, 54) + bytes(info)
    return header + pixels


def window_frame(g, name, settle=1.0):
    """Windows: the game window's content now (window_bmp, after `settle` s for the last command's frame) -> BMP
    bytes, saved as out/<name>.bmp; raises when there is no window or the capture is blank. GX's read-back
    (frame()) times out on WARP (PORT_LEARNINGS "R5a.7"), a window capture does not touch the renderer."""
    from session_exercise import game_window
    time.sleep(settle)
    end = time.monotonic() + 15
    while True:
        g.check_alive()
        hwnd = game_window(g.proc.pid)
        pixels = window_bmp(hwnd) if hwnd else None
        if pixels and len(set(pixels[54:])) >= 16:
            break
        if time.monotonic() > end:
            raise RuntimeError(f'{name}: ' + ('the window capture is blank' if pixels else 'no game window to capture'))
        time.sleep(0.5)
    (g.out / f'{name}.bmp').write_bytes(pixels)
    g.note(f'FRAME {name}: window {int.from_bytes(pixels[18:22], "little")}x'
           f'{abs(int.from_bytes(pixels[22:26], "little", signed=True))}')
    return pixels


def capture_frame(g, name):
    """The frame for a milestone: the fixture's presented frame (GX's read-back). On Windows a window capture only
    when the read-back fails: PrintWindow can return a flip-model swap chain's old content, so a stale window picture
    is not proof that the game stopped presenting. (The read-back's WARP time-outs were GX's unbounded backlog,
    PORT_LEARNINGS "GX on WARP".)"""
    if not WINDOWS:
        return frame(g, name)
    try:
        return frame(g, name)
    except RuntimeError as e:
        g.note(f'FRAME {name}: read-back failed ({e}); the window capture instead')
        return window_frame(g, name)


def window_state(g):
    """The fixture's 'window <w> <h>' and 'mouseover <i>' lines"""
    out = dict(window=None, mouseover=None)
    for line in g.raw().splitlines():
        part = line.split()
        if part[:1] == ['window'] and len(part) == 3:
            out['window'] = (int(part[1]), int(part[2]))
        elif part[:1] == ['mouseover'] and len(part) == 2:
            out['mouseover'] = int(part[1])
    return out


PARK = (0.08, 0.2)   # where the pointer waits during captures (window fractions)


def pointer(g, form, fx, fy, op='pointer'):
    st = window_state(g)
    if not st['window']:
        raise RuntimeError('the fixture reports no window size')
    x, y = int(fx * st['window'][0]), int(fy * st['window'][1])
    seq = g.command(f'{op} {form} {x} {y}')
    g.wait(lambda s: s['command'] >= seq, f'{op} {x},{y}', 30)
    time.sleep(1.0)
    after = window_state(g)
    g.note(f'POINTER {op} form {form} at {x},{y} (window {st["window"]}): mouseover {after["mouseover"]}')
    return dict(op=op, form=form, x=x, y=y, window=st['window'], mouseover=after['mouseover'])


def dismiss(g, st):
    """Cancels a modal form of ai_game.DISMISS (the civilization forms: no simulation input) -> True when it did."""
    if st['form'] in DISMISS:
        form = st['form']
        seq = g.command(DISMISS[form])
        g.wait(lambda s: s['command'] >= seq and s['form'] != form, f'form {form} closed', 30)
        g.note(f'PLAY form {form} cancelled ({DISMISS[form]})')
        return True
    return False


def setup_lobby(g, a):
    """The lobby set up as ai_game.py does: the human idles in slot 0, a.ai_players computers, every option absolute
    (a.map_size, a.map_type, a.difficulty, a.game_speed; a.lobby CONTROL=VALUE; a.lobby_check CONTROL=0|1)."""
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
    for item in getattr(a, 'lobby', None) or []:   # CONTROL=VALUE (as ai_game.py's --lobby)
        control, _, value = item.partition('=')
        options[int(control)] = value
    for k in range(a.ai_players + 1):
        options.setdefault(TEAM + k, '-')
        options.setdefault(CITIZENS + k, '5')
    for control, value in options.items():
        chosen.append(g.select(control, value, f'control {control}'))
    for item in getattr(a, 'lobby_check', None) or []:   # CONTROL=0|1 (as ai_game.py's --lobby-check)
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


def local_row(tel):
    return next((p for p in tel.get('players', []) if p['index'] == tel.get('local')), None)


def civ_preempt(g):
    """The civilization timer pre-empt (ai_game.py): open the civilization builder (game form control 5) and cancel it
    at once, so the timer's inputs never land on a wall-time-dependent PT -> evidence dict."""
    before = g.state()['telemetry']
    seq = g.command(f'click {GAME} 5')
    st = g.wait(lambda s: s['command'] >= seq and s['form'] in DISMISS, 'the civilization builder', 30)
    opened = st['telemetry']
    dismiss(g, st)
    after = g.state()['telemetry']
    local = local_row(after) or {}
    record = dict(form=st['form'], pt_before=before.get('ticks'), pt_open=opened.get('ticks'),
                  pt_after=after.get('ticks'), game_ms_open=opened.get('game_ms'),
                  civ_chosen=local.get('civ_chosen'), civ_locked=local.get('civ_locked'))
    g.note(f'CIV PREEMPT form {st["form"]} opened at PT {opened.get("ticks")} (game ms {opened.get("game_ms")}), '
           f'cancelled; local civ chosen {local.get("civ_chosen")} locked {local.get("civ_locked")}')
    if 'civ_chosen' in local and not local['civ_chosen']:
        raise RuntimeError('the civilization pre-empt did not mark the local civilization chosen')
    return record


def wait_running(g, players=None, timeout=120):
    """Waits for the game: a map, physics ticks advancing (and, given, that many world players) -> telemetry."""
    first = g.state()['telemetry']
    st = g.wait(lambda s: s['telemetry'].get('ticks', 0) > first.get('ticks', 0) and s['telemetry'].get('map'),
                'physics ticks advancing', timeout)
    tel = st['telemetry']
    g.note(f'GAME running: players {tel["world_players"]} local {tel["local"]} PT {tel["ticks"]} CT {tel.get("ct")} '
           f'game ms {tel["game_ms"]}')
    if players is not None and tel['world_players'] != players:
        raise RuntimeError(f'expected {players} world players, got {tel["world_players"]}')
    return tel


def play(g, a, ai_game, capture=None, stall=90):
    """Plays on until the game ends (over, or the idle human has a result in an AI game), a.max_game_seconds of game
    time or a.play_seconds of wall time; cancels the civilization forms; fails when the physics stalls `stall` s.
    -> dict(outcome game-over|human-result|max-game-seconds|play-seconds, samples, forms, results, defeat_order,
    wall_seconds). capture(name): called every a.capture_every wall seconds."""
    tel = g.state()['telemetry']
    start = time.monotonic()
    samples, forms, results, defeat_order = [], {}, {}, []
    last_sample = last_capture = last_note = 0
    last_ticks, last_advance = (tel.get('ticks'), tel.get('game_ms')), time.monotonic()
    outcome = 'play-seconds'
    while True:
        g.check_alive()
        st = g.state()
        tel = st['telemetry']
        if st['form'] not in (GAME, None) and st['form'] not in forms:
            forms[st['form']] = dict(first_game_ms=tel.get('game_ms'), first_pt=tel.get('ticks'), stack=st['stack'])
            (g.out / f'form{st["form"]}.txt').write_text(g.raw(), encoding='latin-1')
            g.note(f'UI MODAL form {st["form"]} stack {st["stack"]} at game ms {tel.get("game_ms")} PT {tel.get("ticks")}')
        if dismiss(g, st):
            forms[st['form']].setdefault('dismissed', []).append(dict(game_ms=tel.get('game_ms'), pt=tel.get('ticks')))
            last_advance = time.monotonic()
            continue
        now = time.monotonic()
        progress = (tel.get('ticks'), tel.get('game_ms'))
        if progress != last_ticks:
            last_ticks, last_advance = progress, now
        elif now - last_advance > stall:
            raise RuntimeError(f'physics stalled for {stall} s at PT {tel.get("ticks")} on form {st["form"]} '
                               f'(stack {st["stack"]})')
        for p in tel.get('players', []):
            if p['index'] >= 1 and p['result'] and results.get(p['index']) != p['result']:
                results[p['index']] = p['result']
                kind = {1: 'victorious', 2: 'defeated'}.get(p['result'], p['result'])
                g.note(f'RESULT player {p["index"]} {kind} at game ms {tel.get("game_ms")} PT {tel.get("ticks")}')
                if p['result'] == 2:
                    defeat_order.append(dict(player=p['index'], pt=tel.get('ticks'), game_ms=tel.get('game_ms')))
        if now - last_sample >= 10:
            last_sample = now
            samples.append(dict(wall_s=round(now - start, 1), form=st['form'], **tel))
            (g.out / 'samples.json').write_text(json.dumps(samples, indent=1) + '\n')
        if now - last_note >= (60 if ai_game else 10):
            last_note = now
            pops = ' '.join(f'{p["index"]}:{p["population"]}{"*" if p["result"] else ""}' for p in tel['players'])
            g.note(f'PLAY t+{now - start:.0f}s game {tel.get("game_ms", 0) / 1000:.0f} s PT {tel.get("ticks")} '
                   f'CT {tel.get("ct")} paused {tel.get("paused")} population {pops}')
        if capture and getattr(a, 'capture_every', 0) and now - last_capture >= a.capture_every:
            last_capture = now
            capture(f'play_{int(now - start):05d}')
        if tel.get('over'):
            outcome = 'game-over'
            break
        local = local_row(tel)
        if ai_game and local and local['result']:
            outcome = 'human-result'
            break
        if getattr(a, 'max_game_seconds', 0) and tel.get('game_ms', 0) >= a.max_game_seconds * 1000:
            outcome = 'max-game-seconds'
            break
        if getattr(a, 'play_seconds', None) is not None and now - start >= a.play_seconds:
            break
        time.sleep(1)
    return dict(outcome=outcome, samples=samples, forms=forms, results=results, defeat_order=defeat_order,
                wall_seconds=round(time.monotonic() - start, 1))


def _ids(count, text):
    return [] if text == '-' else [int(v) for v in text.split(',')][:int(count)]


def box_state(text):
    """The fixture's box selection lines (EETestFixture.cpp "the box selection") -> dict(selection=[ids] or None,
    view=(l, t, r, b) or None, box=dict(seq, mode, phase, lasso, before, expected, match, dead, dead_in_lasso,
    dead_state) or None)."""
    out = dict(selection=None, view=None, box=None)
    for line in text.splitlines():
        p = line.split()
        try:
            if p[:1] == ['selection'] and len(p) == 3:
                out['selection'] = _ids(p[1], p[2])
            elif p[:1] == ['view'] and len(p) == 5:
                out['view'] = tuple(int(v) for v in p[1:5])
            elif p[:1] == ['boxselect'] and len(p) == 24:
                out['box'] = dict(seq=int(p[2]), mode=int(p[4]), phase=int(p[6]), lasso=_ids(p[8], p[9]),
                                  before=_ids(p[11], p[12]), expected=_ids(p[14], p[15]), match=int(p[17]),
                                  dead=int(p[19]), dead_in_lasso=int(p[21]), dead_state=p[23])
        except (IndexError, ValueError):
            pass
    return out


BOX_MODES = {'select': 0, 'add': 1, 'dead': 2}


def box_step(g, form, name, mode, box, timeout=30):
    """One box selection through the fixture's boxselect command (the real press, drag and release) -> the step's
    evidence; raises when the fixture refuses it, no box selection message is built, or the selected group does not
    settle on the expected units (the original's rule over the box's ids) within timeout s."""
    x0, y0, x1, y1 = (int(v) for v in box)
    seq = g.command(f'boxselect {form} {BOX_MODES[mode]} {x0} {y0} {x1} {y1}')
    end = time.monotonic() + 20
    while g.verdict(seq) not in ('box-released', 'box-busy', 'box-no-candidate', 'rejected') and time.monotonic() < end:
        g.check_alive()
        time.sleep(0.2)
    verdict = g.verdict(seq)
    if verdict != 'box-released':
        raise RuntimeError(f'box {name}: the fixture says {verdict!r} for boxselect mode {mode} {box}')
    end = time.monotonic() + timeout
    matched, last = 0, None
    while time.monotonic() < end:
        g.check_alive()
        dismiss(g, g.state())
        last = box_state(g.raw())
        b = last['box']
        if b and b['seq'] == seq and b['phase'] == 3 and b['match'] and \
                sorted(last['selection'] or []) == sorted(b['expected']):
            matched += 1
            if matched >= 2:   # two polls in a row: the group settled
                break
        else:
            matched = 0
        time.sleep(0.3)
    b = (last or {}).get('box') or {}
    row = dict(step=name, mode=mode, box=[x0, y0, x1, y1], seq=seq, phase=b.get('phase'), lasso=b.get('lasso'),
               before=b.get('before'), expected=b.get('expected'), selection=(last or {}).get('selection'),
               match=matched >= 2, dead=b.get('dead'), dead_in_lasso=b.get('dead_in_lasso'),
               dead_state=b.get('dead_state'))
    g.note(f'BOX {name}: mode {mode} box {row["box"]} lasso {len(row["lasso"] or [])} selection '
           f'{len(row["selection"] or [])} expected {len(row["expected"] or [])} match {row["match"]}'
           + (f' dead {row["dead"]} in lasso {row["dead_in_lasso"]} state {row["dead_state"]}' if mode == 'dead' else ''))
    if b.get('seq') != seq or b.get('phase') != 3:
        raise RuntimeError(f'box {name}: no box selection message was built (phase {b.get("phase")}): {row}')
    if not row['match']:
        raise RuntimeError(f'box {name}: the selected group did not settle on the expected units: {row}')
    return row


def box_select(g, form):
    """The box selection exercise on the game form (EETestFixture.cpp "the box selection"): a box over the middle of
    the game view (it must catch some of the local player's units), its left half, the right half added with Shift
    (the group must keep the left half's units), and the whole box again with one unit of the selection disbanded
    just before the release (the message must carry the dead unit's id, dead_in_lasso, and the unit must be dead by
    the check, dead_state not alive). Each step must settle on the expected group -> evidence; raises on a failure."""
    st = box_state(g.raw())
    if not st['view']:
        raise RuntimeError('box selection: the fixture reports no game view')
    left, top, right, bottom = st['view']
    w, h = right - left, bottom - top
    full = (left + 0.2 * w, top + 0.2 * h, right - 0.2 * w, bottom - 0.2 * h)
    middle = left + 0.5 * w
    steps = [box_step(g, form, 'select', 'select', full)]
    if not steps[-1]['selection']:
        raise RuntimeError(f'box select: the box {steps[-1]["box"]} caught none of the local player\'s units')
    steps.append(box_step(g, form, 'select-left', 'select', (full[0], full[1], middle, full[3])))
    steps.append(box_step(g, form, 'add-right', 'add', (middle, full[1], full[2], full[3])))
    dropped = set(steps[-2]['selection']) - set(steps[-1]['selection'])
    if dropped:
        raise RuntimeError(f'box add-right: the added box dropped units of the selection: {sorted(dropped)}')
    steps.append(box_step(g, form, 'dead', 'dead', full))
    dead = steps[-1]
    if dead['dead'] is None or dead['dead'] < 0 or not dead['dead_in_lasso'] or dead['dead_state'] == 'alive':
        raise RuntimeError(f'box dead: the case was not reached (unit {dead["dead"]} disbanded; in the box\'s ids: '
                           f'{dead["dead_in_lasso"]}; its state: {dead["dead_state"]})')
    return dict(view=st['view'], grew=len(steps[2]['selection']) > len(steps[1]['selection']), steps=steps)


ORDER_KINDS = {'gather': 0, 'repair': 1, 'repair-queued': 2, 'patrol': 3, 'attack-ground': 4, 'move': 5}
# the refusals a random map's start may give (no siege unit, no button for the group, nothing in view): a skipped step
ORDER_SKIPS = ('no-unit', 'no-button', 'no-target', 'no-point')
# the steps a random map's start must give (citizens, a tree or mine and a building in view)
ORDER_REQUIRED = ('gather', 'repair', 'repair-queued')


def order_state(text):
    """The fixture's order lines (EETestFixtureOrders.cpp) -> dict(order=dict(seq, kind, phase, type, target, point,
    queued, units, ok, bad, skip, note) or None, units=[dict(seq, id, verdict, expected, found)])."""
    out = dict(order=None, units=[])
    for line in text.splitlines():
        p = line.split()
        try:
            if p[:1] == ['order'] and len(p) == 26:
                out['order'] = dict(seq=int(p[2]), kind=int(p[4]), phase=int(p[6]), type=int(p[8]), target=int(p[10]),
                                    point=(float(p[12]), float(p[13])), queued=int(p[15]), units=int(p[17]),
                                    ok=int(p[19]), bad=int(p[21]), skip=int(p[23]), note=p[25])
            elif p[:1] == ['orderunit'] and len(p) == 6:
                out['units'].append(dict(seq=int(p[1]), id=int(p[2]), verdict=p[3], expected=p[4], found=p[5]))
        except (IndexError, ValueError):
            pass
    return out


def order_step(g, form, name, timeout=None):
    """One order through the fixture's order command (EETestFixtureOrders.cpp: the group selected, the order given
    through the real input path, every unit judged by the original's rule after the message executed) -> the step's
    evidence (skipped: the refusal when the start does not give the case); raises on a bad unit, no message or no
    verdict."""
    if timeout is None:   # gather may first walk to a resource out of view (up to 60 s, "scouting"); a click with no
        timeout = 120 if name == 'gather' else 60   # message is retried once from a fresh view (note "retry", ~20 s)
    seq = g.command(f'order {form} {ORDER_KINDS[name]}')
    end = time.monotonic() + timeout
    while g.verdict(seq) not in ('order-done', 'order-refused', 'order-busy', 'rejected') and time.monotonic() < end:
        g.check_alive()
        time.sleep(0.25)
    verdict = g.verdict(seq)
    last = None
    end = time.monotonic() + 10
    while time.monotonic() < end:   # the next poll's lines
        g.check_alive()
        last = order_state(g.raw())
        o = last['order']
        if o and o['seq'] == seq and o['phase'] in (3, 9):
            break
        time.sleep(0.25)
    o = (last or {}).get('order') or {}
    units = [u for u in (last or {}).get('units', []) if u['seq'] == seq]
    row = dict(step=name, seq=seq, verdict=verdict, phase=o.get('phase'), note=o.get('note'), type=o.get('type'),
               target=o.get('target'), point=o.get('point'), queued=o.get('queued'), ok=o.get('ok'), bad=o.get('bad'),
               skip=o.get('skip'), units=units, skipped=None)
    if verdict == 'order-refused' and o.get('note') in ORDER_SKIPS:
        row['skipped'] = o['note']
        g.note(f'ORDER {name}: skipped ({o["note"]})')
        return row
    g.note(f'ORDER {name}: {verdict} target {row["target"]} queued {row["queued"]} units {len(units)} ok {row["ok"]} '
           f'bad {row["bad"]} skip {row["skip"]} note {row["note"]}')
    for u in units:
        if u['verdict'] == 'bad':
            g.note(f'ORDER {name}: unit {u["id"]} expected {u["expected"]} found {u["found"]}')
    if verdict != 'order-done' or o.get('seq') != seq or o.get('phase') != 3:
        raise RuntimeError(f'order {name}: the fixture says {verdict!r}, phase {o.get("phase")} note {o.get("note")}: {row}')
    if row['bad'] or not row['ok']:
        raise RuntimeError(f'order {name}: {row["bad"]} units differ from the original rule, {row["ok"]} agree: {row}')
    return row


def orders(g, form, names=tuple(ORDER_KINDS)):
    """The orders exercise on the game form (EETestFixtureOrders.cpp): gather, repair, repair queued (Shift), patrol,
    attack ground and move, each on a group of the local player's units, each unit judged after the message executed.
    The steps in ORDER_REQUIRED must run; the others may be skipped when the start does not give them (no siege unit,
    no Patrol button for the group) -> evidence; raises on a failure."""
    steps = [order_step(g, form, name) for name in names]
    missing = [r['step'] for r in steps if r['skipped'] and r['step'] in ORDER_REQUIRED]
    if missing:
        raise RuntimeError(f'orders: the required steps {missing} were not reached: '
                           f'{[(r["step"], r["skipped"]) for r in steps]}')
    return dict(steps=steps, ran=[r['step'] for r in steps if not r['skipped']],
                skipped={r['step']: r['skipped'] for r in steps if r['skipped']})


DIK = dict(up=0xC8, down=0xD0, left=0xCB, right=0xCD, zoom_in=0x1B, zoom_out=0x1A)   # ']' zooms in, '[' out


def camera(g):
    """The fixture's camera telemetry now (parse_telemetry 'camera'); raises when the fixture reports none."""
    cam = g.state()['telemetry'].get('camera')
    if cam is None:
        raise RuntimeError('the fixture reports no camera (no "camera" telemetry line: an older game?)')
    return cam


def _on_map(cam):
    return bool(cam['valid'] and cam['valid_target'] and 0 <= cam['tx'] <= cam['size'] and 0 <= cam['ty'] <= cam['size'])


def _fmt(cam):
    return (f'({cam["tx"]:.2f},{cam["ty"]:.2f}) at ({cam["x"]:.2f},{cam["y"]:.2f},{cam["z"]:.2f}) zoom {cam["zoom"]:.3f} '
            f'mode {cam["mode"]} on map {int(_on_map(cam))} size {cam["size"]}')


def _command(g, text, verdicts, what, timeout=20):
    seq = g.command(text)
    end = time.monotonic() + timeout
    while g.verdict(seq) is None and time.monotonic() < end:
        g.check_alive()
        time.sleep(0.1)
    verdict = g.verdict(seq)
    if verdict not in verdicts:
        raise RuntimeError(f'{what}: the fixture says {verdict!r} for {text!r}')
    return seq


def hold_key(g, form, name, ms):
    """A key held ms milliseconds through the fixture's key command (EETestFixture.cpp: a key down, then up, through
    ISHandleEvent, the entry the window layer feeds a real key press to)."""
    _command(g, f'key {form} {DIK[name]} {ms}', ('key-pressed',), f'key {name}')
    time.sleep(ms / 1000)


def scroll_check(g, form, hold_ms=1500, settle=2.0, captures=True):
    """The camera moved in play (RUST_PLAN R4.4: the camera's math in Rust): on the game form, the camera scrolled by
    keyboard (an arrow key held through the real input path; the opposite key when the first is at the map's edge)
    and by the screen edge (the game's pointer at the window's left edge, else the right one; EEAScroll), each move's
    target and focus checked through the fixture's camera telemetry to have changed and to lie on the map; then the
    zoom set to 1 and changed by the zoom hot keys (']' in, '[' out) and by the wheel (3 notches each way), each in
    its direction. Captures after each part (captures=False: none). -> evidence; raises 'SCROLL CHECK FAIL ...' with
    the numbers on a failure; notes 'SCROLL CHECK PASS ...'."""
    rows = []

    def fail(text):
        g.note(f'SCROLL CHECK FAIL {text}')
        raise RuntimeError(f'SCROLL CHECK FAIL {text}')

    def moved(before, after):
        return ((after['tx'] - before['tx']) ** 2 + (after['ty'] - before['ty']) ** 2) ** 0.5

    def step(kind, name, act):
        dismiss(g, g.state())
        before = camera(g)
        act()
        time.sleep(settle)
        after = camera(g)
        row = dict(kind=kind, how=name, before=before, after=after, moved=round(moved(before, after), 3),
                   on_map=_on_map(after))
        rows.append(row)
        g.note(f'SCROLL {kind} {name}: {_fmt(before)} -> {_fmt(after)}, moved {row["moved"]}')
        if not row['on_map']:
            fail(f'{kind} {name}: the camera left the map: {_fmt(after)}')
        return row

    start = camera(g)
    if not _on_map(start):
        fail(f'the camera starts off the map: {_fmt(start)}')
    for kind, ways in (('key', (('right', lambda: hold_key(g, form, 'right', hold_ms)),
                                ('left', lambda: hold_key(g, form, 'left', hold_ms)))),
                       ('edge', (('left-edge', lambda: edge(g, form, 0.0, hold_ms)),
                                 ('right-edge', lambda: edge(g, form, 1.0, hold_ms))))):
        for name, act in ways:
            if step(kind, name, act)['moved'] >= 0.5:
                break
        else:
            fail(f'{kind}: the camera did not move ' + ', '.join(f'{r["how"]} {r["moved"]}' for r in rows if r['kind'] == kind))
        if captures:
            g.capture(f'scroll-{kind}')
    _command(g, f'zoom {form} 0 100', ('zoomed',), 'zoom 100')
    time.sleep(settle)
    zooms = [camera(g)['zoom']]
    if abs(zooms[0] - 1.0) > 1e-3:
        fail(f'the fixture set zoom 1, the camera reports {zooms[0]}')
    for name, act, sign in (('key-in', lambda: hold_key(g, form, 'zoom_in', hold_ms // 3), -1),
                            ('key-out', lambda: hold_key(g, form, 'zoom_out', hold_ms // 3), 1),
                            ('wheel-in', lambda: wheel(g, form, 0, 3), -1),
                            ('wheel-out', lambda: wheel(g, form, 1, 3), 1)):
        dismiss(g, g.state())
        act()
        time.sleep(settle)
        cam = camera(g)
        zooms.append(cam['zoom'])
        change = zooms[-1] - zooms[-2]
        rows.append(dict(kind='zoom', how=name, zoom=zooms[-1], change=round(change, 4), camera=cam))
        g.note(f'SCROLL zoom {name}: {zooms[-2]:.3f} -> {zooms[-1]:.3f}')
        if change * sign <= 1e-4:
            fail(f'zoom {name}: {zooms[-2]:.4f} -> {zooms[-1]:.4f} (expected {"a decrease" if sign < 0 else "an increase"})')
        if not _on_map(cam):
            fail(f'zoom {name}: the camera left the map: {_fmt(cam)}')
        if captures and name.startswith('key'):
            g.capture(f'zoom-{name}')
    keyed = next(r for r in rows if r['kind'] == 'key' and r['moved'] >= 0.5)
    edged = next(r for r in rows if r['kind'] == 'edge' and r['moved'] >= 0.5)
    g.note(f'SCROLL CHECK PASS key {keyed["how"]} moved {keyed["moved"]} edge {edged["how"]} moved {edged["moved"]} '
           f'zoom ' + ' -> '.join(f'{z:.3f}' for z in zooms))
    return dict(steps=rows, zooms=zooms, key=keyed['how'], edge=edged['how'])


def edge(g, form, fx, ms):
    """The game's pointer at the window's left (fx 0) or right (fx 1) edge, mid height, for ms milliseconds, then back
    to the middle of the window (EEUIFMainGame's update hands the pointer to the edge scroll, EEAScroll::Update)."""
    st = window_state(g)
    if not st['window']:
        raise RuntimeError('the fixture reports no window size')
    w, h = st['window']
    x = 0 if fx < 0.5 else w - 1
    _command(g, f'pointer {form} {x} {h // 2}', ('pointer-moved',), f'pointer {x}')
    time.sleep(ms / 1000)
    _command(g, f'pointer {form} {w // 2} {h // 2}', ('pointer-moved',), 'pointer back')


def wheel(g, form, direction, notches):
    """notches of the mouse wheel through the fixture's wheel command (ISHandleEvent's wheel event; direction 0:
    away from the user, zoom in; 1: towards, zoom out)."""
    _command(g, f'wheel {form} {direction} {notches}', ('wheeled',), f'wheel {direction}')
    time.sleep(0.15 * notches)


def zoom_captures(g, form, percents):
    """As posix_game.py --zoom-captures: the camera zoom set to each percent (100 = the original's farthest) through
    the fixture's zoom command, each captured as zoom<percent> -> {name: captured}."""
    out = {}
    for percent in percents:
        _command(g, f'zoom {form} 0 {percent}', ('zoomed',), f'zoom {percent}')
        time.sleep(2)
        out[f'zoom{percent}'] = g.capture(f'zoom{percent}') is not None
    return out


GAME_LINE = re.compile(r'^EE-GAME (\d+) p(\d+) (\w+)(.*)$')


def game_log(directory):
    """The newest game log (Logs/game-*.log, EE-GAME lines; ee/EEGameLog.h) in directory -> summary dict or None:
    the start options, each player's kind/civ/personality, its last sample (pop, army, buildings, epoch) and the end."""
    logs = sorted(pathlib.Path(directory).glob('game-*.log'), key=lambda p: p.stat().st_mtime)
    if not logs:
        return None
    out = dict(file=str(logs[-1]), start=None, players={}, last_sample={}, end=None, epochs={})
    for line in logs[-1].read_text(encoding='latin-1', errors='replace').splitlines():
        m = GAME_LINE.match(line)
        if not m:
            continue
        ms, player, kind, rest = int(m[1]), int(m[2]), m[3], m[4]
        fields = dict(kv.split('=', 1) for kv in rest.split() if '=' in kv)
        if kind == 'start':
            out['start'] = fields
        elif kind == 'player':
            out['players'][player] = fields
        elif kind == 'sample':
            out['last_sample'][player] = dict(game_ms=ms, **fields)
        elif kind == 'epoch':
            out['epochs'][player] = fields.get('to')
        elif kind == 'end':
            out['end'] = dict(game_ms=ms, **fields)
    return out


def copy_game_logs(data_root, out):
    """Copies the game's Logs/game-*.log (the data root's) into out/ -> the newest one's summary (game_log)."""
    logs = pathlib.Path(data_root) / 'Logs'
    if not logs.is_dir():
        return None
    for f in logs.glob('game-*.log'):
        shutil.copy(f, pathlib.Path(out) / f.name)
    return game_log(out)
