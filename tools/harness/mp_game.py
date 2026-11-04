"""TEST ONLY (round 40, agent mp): one multiplayer game over loopback between two staged game processes (original and/or
full link), both humans idle, computers in the other slots; checks that both processes stay in sync.

Recipe (original host + full-link client, then the reverse, then the control):
  python -I tools/boot/mp_game.py --host original --client fulllink --diff
  python -I tools/boot/mp_game.py --host fulllink --client original --diff
  python -I tools/boot/mp_game.py --host original --client original --diff

How (docs/LIVE_GAME_TESTING.md "Multiplayer differential test (round 40)"):
- two game_session.py processes, --instance A (host) and B (client), each with --mp-role (tools/boot/mp/session_mp.py):
  the harness skips EEStartExternalLobby (the WON lobby is never opened), TestLobby.dll (tools/boot/mp/lobby_driver.c)
  stands in for the lobby: the host's last-game-options player table, EEHostGameFromCommandLine /
  EEJoinByIPFromCommandLine (the game's -hostgame / -joinbyip code) and the lobby's own start EELobbyFn29775D, all on
  the UI thread through EELobbyPostAction. Loopback: host 127.0.0.A, client 127.0.0.B, same TCP Protocol ports.
- this driver: both main menus -> host go -> 'hosted' -> client go -> 'client-in' (slot 1 connected) -> start -> both
  reach the game form; then it samples both every few seconds until --max-game-seconds (game clock of the host), the
  game's end, a process exit or a desync (the game's own out-of-synch report: an "OOS *.log" dump in a stage or the
  OutOfSynch.wav load in a raw log), then stops both and, with --diff, compares the two per-PT hash streams
  (differential/compare.py): identical at every common PT is the pass.
- --engine [PATH] stages one engine DLL (default the pure relink build/lle/Low-Level Engine.dll) at BOTH ends. The game
  admits a client only with the host's engine bytes: EEInitializeNetwork (exe 0x5431dc) sums the file checksums of
  Low-Level Engine.dll, sxlrt233.dll, TCP Protocol.dll and the data files into gBinaryChecksum and appends its low
  13 bits to gGameVersion; EEServer::ProcessConnectionRequest refuses another version (kConnectionVersionMismatch).
  An engine on one end only therefore never gets past "client in slot 1" (round 40, mpengine) and is refused here.
- --cross-engine host|client (with --engine; owner-approved 2026-11-02): the engine goes to that end only and that
  process gets the original engine's checksum: exe_boot adds (byte sum of the original engine DLL - byte sum of the
  staged one) to the value EEInitializeNetwork stores in gBinaryChecksum (EEGetFileChecksum is a byte sum), so its
  gBinaryChecksum and gGameVersion suffix are the original's (session_mp --mp-checksum-delta; logged; image unpatched).
- --fixed-ends LABEL[,LABEL] --fixed-engine PATH --fixed-tcp PATH (TEST ONLY, fixes branch lane net): those ends get the
  engine and/or TCP Protocol.dll staged (game_session --engine / --tcp) and the --cross-engine checksum intervention with
  DELTA = sum over the staged DLLs of (retail byte sum - staged byte sum), so they keep the retail gBinaryChecksum; any
  number of ends, 2-4 processes. DELTA 0 (DLLs built by tools/build.py, which compensates the byte sums to the retail
  ones) installs no intervention: the end runs with the checksum the game computes from the staged files.
- 3-4 processes (final campaign, lane mp): --clients SPEC,SPEC[,SPEC] (instead of --client) gives one client per
  slot 1..; humans = 1 + clients, computers follow. Every end gets the other ends' loopback octets (exe_boot
  --mp-peers: the host answers each client at its own address; a migrated host is reached at its own). Clients join
  one after another (the host reports "slot-in K"). --diff compares every pair of ends.
- --kill LABEL@PT (repeatable; labels host, client, client2, client3): once a surviving end's sampled PT reaches PT,
  that end's game process is terminated (TerminateProcess on the staged EE-AOC.exe of that instance, checked by image
  path: a dropped peer / a host that leaves); the others go on (a client drop, or the game's own server migration
  EEClient::HandleConnectionLost -> EEAServerMigration when the host goes). Wall-clock timed: compare the survivors
  of one game, not two games after the kill.
Exit 0 pass (in sync, streams identical when compared), 1 desync / stream difference, 2 harness failure.
"""
import argparse, datetime, json, os, pathlib, re, subprocess, sys, time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
PY = sys.executable
GAME = 11
MENU = 12
DEFAULT_OPTIONS = ['Map Size=1', 'Map Type=sz:Continental', 'Game Variant=2', 'Difficulty Level=2', 'Starting Resources=4',
                   'Starting Epoch=0', 'Ending Epoch=14', 'Game Unit Limit=1200', 'Wonders For Victory=0', 'Game Speed=3',
                   'Reveal Map=0', 'Cheat Codes=0', 'Allow Custom Civs=1', 'Lock Teams=1', 'Lock Speed=1', 'Record Game=0']
# the round-39 long-game lobby (Small Continental, Deathmatch (variant 2), Hard (2), resources 4, epochs 0..14, 1200 units,
# Very Fast) as the game itself stored it in "Game Options" after a lobby start


def image_args(spec):
    if spec == 'original':
        return ['--original'], 'o'
    if spec.startswith('fulllink'):
        path = spec.partition('=')[2]
        return (['--fulllink', path] if path else ['--fulllink']), 'f'
    raise ValueError(spec)


def ui_form(stage):
    try:
        lines = (stage / 'ui-state.txt').read_text(encoding='ascii', errors='replace').splitlines()
        return int(lines[0].split()[1]), int(lines[2].split()[1])
    except (OSError, IndexError, ValueError):
        return None, None


def read(path):
    try:
        return path.read_text(encoding='ascii', errors='replace')
    except OSError:
        return ''


def terminate(pid, stage):
    """Terminate the staged game process pid (refuses any other image)."""
    import ctypes
    from ctypes import wintypes
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.OpenProcess.restype = wintypes.HANDLE
    k.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    h = k.OpenProcess(0x1001, False, pid)   # PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION
    if not h:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        buf = ctypes.create_unicode_buffer(32768); n = wintypes.DWORD(len(buf))
        k.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        if not k.QueryFullProcessImageNameW(h, 0, buf, ctypes.byref(n)):
            raise ctypes.WinError(ctypes.get_last_error())
        if pathlib.Path(buf.value).resolve() != (stage / 'EE-AOC.exe').resolve():
            raise ValueError(f'pid {pid} is {buf.value}, not this run\'s staged game; not killed')
        if not k.TerminateProcess(h, 0xdead):
            raise ctypes.WinError(ctypes.get_last_error())
    finally:
        k.CloseHandle(h)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--host', default='original', help='original | fulllink[=PATH]')
    ap.add_argument('--client', default='original', help='original | fulllink[=PATH]')
    ap.add_argument('--clients', help='SPEC,SPEC[,SPEC]: one client per slot 1.. (3-4 process game; replaces --client)')
    ap.add_argument('--kill', action='append', default=[], metavar='LABEL@PT', help='terminate that end once a survivor reaches PT')
    ap.add_argument('--instances', help='host,client[,..] instance numbers (also their loopback octets; default 28,29,..)')
    ap.add_argument('--computers', type=int, default=2)
    ap.add_argument('--seed', type=lambda v: int(v, 0), default=1234)
    ap.add_argument('--pt-rate', type=int, default=40)
    ap.add_argument('--time-factor', default=None)
    ap.add_argument('--max-game-seconds', type=float, default=600)
    ap.add_argument('--wall-timeout', type=int, default=3600, help='seconds for the whole run')
    ap.add_argument('--diff', action='store_true', help='per-PT hash streams in both (--diff-stream auto) and compare')
    ap.add_argument('--diff-snapshot-every', type=int, default=500)
    ap.add_argument('--no-human-defeat', action='store_true', help='both humans protected in both processes (--nhd-players 6)')
    ap.add_argument('--option', action='append', default=[], metavar='NAME=DWORD|NAME=sz:TEXT', help='override a Game Options value')
    ap.add_argument('--no-debug-heap', action='store_true', help='_NO_DEBUG_HEAP=1 for both games (default: the debug heap every harness session has)')
    ap.add_argument('--out', type=pathlib.Path)
    ap.add_argument('--extra', action='append', default=[], help='extra game_session argument for both ends (repeatable)')
    ap.add_argument('--engine', nargs='?', const=str(ROOT / 'build/lle/Low-Level Engine.dll'),
                    help='stage this engine DLL at both ends (game_session --engine; default the pure relink)')
    ap.add_argument('--cross-engine', choices=['host', 'client'],
                    help='--engine on this end only, with the original engine checksum (owner-approved 2026-11-02)')
    ap.add_argument('--fixed-ends', help='TEST ONLY (fixes branch, lane net): comma list of end labels (host, client, '
                    'client2, client3) that get --fixed-engine / --fixed-tcp staged, each with the checksum delta that '
                    'gives it the retail DLLs\' gBinaryChecksum (the --cross-engine intervention, per end)')
    ap.add_argument('--fixed-engine', type=pathlib.Path, help='with --fixed-ends: the engine DLL for those ends')
    ap.add_argument('--fixed-tcp', type=pathlib.Path, help='with --fixed-ends: the TCP Protocol.dll for those ends')
    ap.add_argument('--host-extra', action='append', default=[], help='extra game_session argument(s) for the host only')
    ap.add_argument('--client-extra', action='append', default=[], help='extra game_session argument(s) for the client only')
    a = ap.parse_args()
    client_specs = a.clients.split(',') if a.clients else [a.client]
    instances = [int(v) for v in a.instances.split(',')] if a.instances else list(range(28, 29 + len(client_specs)))
    if len(instances) != 1 + len(client_specs) or len(set(instances)) != len(instances) or \
            not all(28 <= v <= 31 for v in instances):
        ap.error('instances: one per end, distinct, 28..31 (final-campaign lane mp)')
    hi, ci = instances[0], instances[1]
    humans = len(instances)
    labels = ['host', 'client'] + [f'client{k}' for k in range(2, humans)]
    kills = {}
    for item in a.kill:
        label, _, pt = item.partition('@')
        if label not in labels or not pt.isdigit():
            ap.error(f'--kill {item}: LABEL@PT with LABEL in {labels}')
        kills[label] = int(pt)
    if len(kills) >= humans:
        ap.error('--kill: at least one end must survive')
    if a.cross_engine and humans > 2:
        ap.error('--cross-engine: two-process games only')
    one_sided = [r for r, extra in (('host', a.host_extra), ('client', a.client_extra)) if '--engine' in ' '.join(extra).split()]
    if len(one_sided) == 1:   # the engine bytes are part of the game's version check (see the docstring): never admitted
        ap.error(f'--engine in --{one_sided[0]}-extra only: the other end would refuse the version; use --engine (both ends)')
    cross_delta = None
    if a.cross_engine:
        if not a.engine:
            ap.error('--cross-engine needs --engine')
        retail = ROOT / 'orig-bin/aoc/retail/Low-Level Engine.dll'
        installed = pathlib.Path(r'C:/Program Files (x86)/Empire Earth/Empire Earth - The Art of Conquest/Low-Level Engine.dll')
        if installed.read_bytes() != retail.read_bytes():
            ap.error('the installed engine is not the retail one; the original end would not have its checksum')
        ours = pathlib.Path(a.engine).read_bytes()
        cross_delta = (sum(retail.read_bytes()) - sum(ours)) & 0xffffffff   # EEGetFileChecksum: a byte sum
        if a.time_factor:   # the time shim renames the WINMM.dll import in BOTH staged engines: same bytes, same delta
            import pefile
            def winmm(data):
                pe = pefile.PE(data=data, fast_load=True); pe.parse_data_directories()
                return [d.dll for d in pe.DIRECTORY_ENTRY_IMPORT if d.dll.lower() == b'winmm.dll']
            if winmm(retail.read_bytes()) != winmm(ours):
                ap.error('--cross-engine with --time-factor: the engines spell the WINMM.dll import differently')
    fixed_ends, fixed_delta = set(), None
    if a.fixed_ends:   # TEST ONLY (fixes branch, lane net): the --cross-engine intervention per end, engine and/or TCP
        fixed_ends = set(a.fixed_ends.split(','))
        if not fixed_ends <= set(labels):
            ap.error(f'--fixed-ends: labels from {labels}')
        if a.engine or a.cross_engine:
            ap.error('--fixed-ends excludes --engine/--cross-engine')
        if not (a.fixed_engine or a.fixed_tcp):
            ap.error('--fixed-ends needs --fixed-engine and/or --fixed-tcp')
        installed_dir = pathlib.Path(r'C:/Program Files (x86)/Empire Earth/Empire Earth - The Art of Conquest')
        fixed_delta = 0
        for name, path in (('Low-Level Engine.dll', a.fixed_engine), ('TCP Protocol.dll', a.fixed_tcp)):
            if not path:
                continue
            retail = (ROOT / 'orig-bin/aoc/retail' / name).read_bytes()
            if (installed_dir / name).read_bytes() != retail:
                ap.error(f'the installed {name} is not the retail one; the original end would not have its checksum')
            ours = path.read_bytes()
            fixed_delta += sum(retail) - sum(ours)   # EEGetFileChecksum: a byte sum
            if name == 'Low-Level Engine.dll' and a.time_factor:   # the time shim renames WINMM.dll in every staged engine
                import pefile
                def winmm(data):
                    pe = pefile.PE(data=data, fast_load=True); pe.parse_data_directories()
                    return [d.dll for d in pe.DIRECTORY_ENTRY_IMPORT if d.dll.lower() == b'winmm.dll']
                if winmm(retail) != winmm(ours):
                    ap.error('--fixed-engine with --time-factor: the engines spell the WINMM.dll import differently')
        fixed_delta &= 0xffffffff
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    out = a.out or ROOT / 'build/scratch_round40/mp/runs' / f'{stamp}_{a.host.split("=")[0]}-{"-".join(s.split("=")[0] for s in client_specs)}'
    out.mkdir(parents=True, exist_ok=True)
    options = dict(o.split('=', 1) for o in DEFAULT_OPTIONS)
    options.update(dict(o.split('=', 1) for o in a.option))
    ends = {}
    plan = [('host', 'host', a.host, hi, ci, 0)] + [(labels[k], 'client', spec, instances[k], hi, k)
                                                    for k, spec in enumerate(client_specs, 1)]
    for label, role, spec, inst, peer, slot in plan:
        img, letter = image_args(spec)
        stage = ROOT / 'build/boot' / f'windowed_{letter}_{inst}'
        for name in ('ui-state.txt', 'lobby.go', 'lobby.start', 'lobby.status', 'lobby.log', 'stop.request'):
            (stage / name).unlink(missing_ok=True)   # this run's instance stage only (stale state of an earlier run)
        for old in stage.glob('OOS *.log'):
            old.unlink()
        cmd = [PY, '-I', str(HERE / 'game_session.py'), *img, '--instance', str(inst), '--timeout', str(a.wall_timeout * 1000),
               '--seed', str(a.seed), '--pt-rate', str(a.pt_rate), '--mp-role', role, '--mp-self', str(inst),
               '--mp-peer', str(peer), '--mp-port-offset', str(16 * hi), '--mp-computers', str(a.computers)]
        if humans > 2:   # the other ends besides --mp-peer keep their own loopback addresses
            cmd += ['--mp-humans', str(humans), '--mp-peers', ','.join(str(v) for v in instances if v not in (inst, peer))]
            if role == 'client':
                cmd += ['--mp-slot', str(slot)]
        if role == 'host':
            for k, v in options.items():
                cmd += ['--mp-option', f'{k}={v}']
        if a.time_factor:
            cmd += ['--time-factor', a.time_factor]
        if a.diff:
            cmd += ['--diff-stream', 'auto', '--diff-snapshot-every', str(a.diff_snapshot_every)]
        if a.no_human_defeat:
            cmd += ['--mp-no-human-defeat']
        for e in a.extra + (a.host_extra if role == 'host' else a.client_extra):
            cmd += e.split()
        if a.diff and any('--mp-human-script' in e for e in a.extra + a.host_extra + a.client_extra):
            cmd += ['--mp-human-ui-mask']   # every end hashes the human's acknowledgement stamps masked (layout.py)
        if a.engine and (not a.cross_engine or a.cross_engine == role):   # one argument: the path may contain spaces
            cmd += ['--engine', str(pathlib.Path(a.engine).resolve())]
            if a.cross_engine:
                cmd += ['--mp-checksum-delta', hex(cross_delta)]
        if label in fixed_ends:
            if a.fixed_engine:
                cmd += ['--engine', str(a.fixed_engine.resolve())]
            if a.fixed_tcp:
                cmd += ['--tcp', str(a.fixed_tcp.resolve())]
            if fixed_delta:   # 0: the staged DLLs already have the retail byte sums (build.py's compensation): no intervention
                cmd += ['--mp-checksum-delta', hex(fixed_delta)]
        log = open(out / f'{label}.session.log', 'w', encoding='utf-8')
        ends[label] = dict(role=role, slot=slot, spec=spec, instance=inst, stage=stage, cmd=cmd, log=log, raw=ROOT / 'build/boot' / f'windowed_{letter}_{inst}.session.raw.txt')
        if letter == 'f':   # telemetry sites of the relaid image, by map name
            from fulllink_locator import LinkMap
            exe = pathlib.Path(spec.partition('=')[2] or ROOT / 'build/exe_fulllink/EE-AOC.exe').resolve()
            lm = LinkMap(exe.with_suffix('.map'))
            ends[label]['addresses'] = dict(communications=lm.address('?gCommunications@@3PAVNESynchronousCommunications@@A'),
                                           world=lm.address('?gWorld@@3VEEWorld@@A'), physics=lm.address('?gPhysics@@3PAVEEPhysics@@A'))
    record = dict(test_only=True, started=stamp, host=a.host, client=a.client, clients=client_specs, kills=kills, engine=a.engine, cross_engine=a.cross_engine,
                  cross_checksum_delta=None if cross_delta is None else hex(cross_delta), instances=instances, seed=a.seed,
                  fixed_ends=sorted(fixed_ends), fixed_engine=a.fixed_engine and str(a.fixed_engine),
                  fixed_tcp=a.fixed_tcp and str(a.fixed_tcp), fixed_checksum_delta=None if fixed_delta is None else hex(fixed_delta),
                  pt_rate=a.pt_rate, time_factor=a.time_factor, computers=a.computers, options=options,
                  no_human_defeat=a.no_human_defeat, no_debug_heap=a.no_debug_heap, events=[], samples=[])

    def note(text):
        line = f'[{time.strftime("%H:%M:%S")}] {text}'
        print(line, flush=True); record['events'].append(line)

    for role in ends:
        e = ends[role]
        env = dict(os.environ)
        if a.no_debug_heap:
            env['_NO_DEBUG_HEAP'] = '1'   # inherited by exe_boot and the game (CreateProcess with the parent environment)
        e['proc'] = subprocess.Popen(e['cmd'], stdout=e['log'], stderr=subprocess.STDOUT, cwd=str(ROOT), env=env)
        note(f'{role} {e["spec"]} instance {e["instance"]} pid {e["proc"].pid}')
    deadline = time.monotonic() + a.wall_timeout
    verdict, failure = None, None

    killed = {}   # label -> PT of the survivor that triggered the kill

    def alive():
        return all(e['proc'].poll() is None for r, e in ends.items() if r not in killed)

    def wait(what, pred, secs):
        end = min(time.monotonic() + secs, deadline)
        while time.monotonic() < end:
            if pred():
                return True
            if not alive():
                raise RuntimeError(f'a session ended while waiting for {what}')
            time.sleep(0.5)
        raise RuntimeError(f'timeout waiting for {what}')

    H, C = ends['host'], ends['client']
    try:
        wait('every main menu and driver', lambda: all(ui_form(e['stage'])[0] == MENU and 'started role' in read(e['stage'] / 'lobby.log')
                                                        for e in ends.values()), 600)
        note('every end at the main menu; host go')
        (H['stage'] / 'lobby.go').write_text('go\n', encoding='ascii')
        wait('hosted', lambda: 'hosted' in read(H['stage'] / 'lobby.status'), 60)
        note(f'host: {read(H["stage"] / "lobby.status").strip().splitlines()[-1]}; client go')
        for label in labels[1:]:
            time.sleep(2)
            e = ends[label]
            (e['stage'] / 'lobby.go').write_text('go\n', encoding='ascii')
            if humans > 2:
                wait(f'{label} in slot {e["slot"]}', lambda: f'slot-in {e["slot"]}' in read(H['stage'] / 'lobby.status'), 120)
                note(f'{label} holds slot {e["slot"]}')
        wait('every client in its slot', lambda: 'client-in' in read(H['stage'] / 'lobby.status'), 120)
        note('every client holds its slot; start in 5 s')
        time.sleep(5)
        (H['stage'] / 'lobby.start').write_text('start\n', encoding='ascii')
        wait('every game form', lambda: all(ui_form(e['stage'])[0] == GAME for e in ends.values()), 600)
        note('every end in the game form')
        import game_state_probe as probe
        pids = {r: ui_form(e['stage'])[1] for r, e in ends.items()}
        last = time.monotonic(); stall = {r: (None, time.monotonic()) for r in ends}
        while True:
            if not alive():
                failure = 'a session ended during the game'; break
            live = [r for r in ends if r not in killed]
            oos = [str(p) for r in live for p in ends[r]['stage'].glob('OOS *.log')]
            oos_sound = [r for r in live if 'OutOfSynch.wav' in read(ends[r]['raw'])[-2000000:]]
            if oos or oos_sound:
                verdict = 'desync'; note(f'OUT OF SYNCH: dumps {oos} sound in {oos_sound}'); break
            row = {}
            for r in live:
                try:
                    s = probe.sample(pids[r], ends[r].get('addresses'), players=True)
                    row[r] = dict(pt=s.get('pt_ub4'), game_ms=s.get('game_ms'), paused=s.get('paused'), over=s.get('game_over_be6'),mp=s.get('multiplayer'),local=s.get('local_player'),
                                  form=ui_form(ends[r]['stage'])[0],
                                  results=[(p['index'], p['result_a28']) for p in s.get('player_rows', [])])
                except Exception as error:
                    row[r] = dict(error=repr(error))
            record['samples'].append(row)
            if time.monotonic() - last > 20:
                note(' | '.join(f'{r} {row[r]}' for r in live)); last = time.monotonic()
            for label, at in kills.items():
                if label in killed:
                    continue
                reached = [row[r].get('pt') or 0 for r in live if r != label]
                if reached and max(reached) >= at:
                    terminate(pids[label], ends[label]['stage'])
                    killed[label] = max(reached)
                    note(f'KILL {label} (instance {ends[label]["instance"]}, pid {pids[label]}) at survivor PT {max(reached)}; '
                         f'its own last sampled PT {row[label].get("pt")}')
            live = [r for r in ends if r not in killed]
            ref = row[live[0]] if live[0] in row else {}
            hm = ref.get('game_ms') or 0
            if a.max_game_seconds and hm >= a.max_game_seconds * 1000:
                verdict = 'max-game-seconds'; note(f'max game seconds reached ({hm} ms, {live[0]})'); break
            if all(row.get(r, {}).get('over') for r in live):
                verdict = 'game-over'; note('game over in every surviving end'); break
            if a.no_human_defeat:   # humans kept in: the game names no winner; stop once <= 1 computer is undecided
                comp = [res for idx, res in ref.get('results', []) if idx > humans]
                if len(comp) == a.computers and sum(res == 0 for res in comp) <= 1 and \
                        all(row.get(r, {}).get('results') == ref.get('results') for r in live):
                    verdict = 'last-computer-standing'; note(f'last computer standing: results {ref["results"]}'); break
            for r in live:   # a form over the game that stalls physics (civ timer forms: see ai_game.DISMISS)
                pt = row.get(r, {}).get('pt')
                if pt != stall[r][0]:
                    stall[r] = (pt, time.monotonic())
                elif time.monotonic() - stall[r][1] > 120:
                    raise RuntimeError(f'{r} PT stalled at {pt} for 120 s (form {row[r].get("form")})')
            if time.monotonic() > deadline:
                verdict = 'wall-timeout'; note('wall timeout'); break
            time.sleep(3)
    except Exception as error:
        failure = str(error); note(f'HARNESS: {error}')
        for e in ends.values():
            (e['stage'] / 'lobby.abort').write_text('abort\n', encoding='ascii')
    finally:
        for e in ends.values():
            if e['proc'].poll() is None:
                (e['stage'] / 'stop.request').write_text('stop\n', encoding='ascii')
        for e in ends.values():
            try:
                e['proc'].wait(timeout=180)
            except subprocess.TimeoutExpired:
                note(f'{e["spec"]} instance {e["instance"]} did not stop; left running (harness timeout ends it)')
            e['log'].close()
    record['killed'] = killed
    for role, e in ends.items():
        text = read(out / f'{role}.session.log')
        m = re.search(r'^EVIDENCE (.*)$', text, re.M)
        e['evidence'] = m.group(1).strip() if m else None
        record[f'{role}_evidence'] = e['evidence']
        record[f'{role}_status'] = read(e['stage'] / 'lobby.status').splitlines()
        raw = read(e['raw'])
        record[f'{role}_stops'] = [l for l in raw.splitlines() if l.startswith('STOP ')]
        record[f'{role}_mp_skips'] = sum(l.startswith('TEST mp skip at') for l in raw.splitlines())
        for p in e['stage'].glob('OOS *.log'):
            (out / f'{role}.{p.name}').write_bytes(p.read_bytes())
    record['verdict'] = verdict; record['failure'] = failure
    rc = 2 if failure else 1 if verdict == 'desync' else 0
    if a.diff:
        streams = {}
        for r, e in ends.items():
            ev = pathlib.Path(e['evidence']) if e.get('evidence') else None
            found = [ev / n for n in ('diff.stream.txt.gz', 'diff.stream.txt', 'diff.stream.gz') if ev and (ev / n).exists()]
            streams[r] = str(found[0]) if found else None
        record['streams'] = streams
        record['compare'] = {}
        pairs = [(labels[i], labels[j]) for i in range(humans) for j in range(i + 1, humans)]
        for x, y in pairs:
            if not (streams[x] and streams[y]):
                note(f'streams missing: {x} {streams[x]} {y} {streams[y]}'); continue
            name = 'compare' if humans == 2 else f'compare.{x}-{y}'
            r = subprocess.run([PY, '-I', str(HERE / 'differential/compare.py'), streams[x], streams[y], '--json', str(out / f'{name}.json')],
                               capture_output=True, text=True)
            (out / f'{name}.txt').write_text(r.stdout + r.stderr, encoding='utf-8')
            record['compare'][f'{x}-{y}'] = r.returncode
            if humans == 2:
                record['compare_exit'] = r.returncode
            note(f'compare {x}-{y} exit {r.returncode}: ' + ' | '.join(r.stdout.strip().splitlines()[:6]))
            if r.returncode not in (0, 2) and rc == 0:   # 2: render-only (the two local players see different things)
                rc = 1
    (out / 'mp_game.json').write_text(json.dumps(record, indent=2, default=str) + '\n', encoding='utf-8')
    note(f'RESULT {"PASS" if rc == 0 else "FAIL"} verdict {verdict} failure {failure}; record {out / "mp_game.json"}')
    sys.exit(rc)


if __name__ == '__main__':
    main()
