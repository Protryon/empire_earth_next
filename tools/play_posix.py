"""Play the POSIX game (macOS, Linux) under lldb, keeping the evidence of any crash.

Every session gets a folder build/playtest/<time>/:
  game.log      the game's and lldb's output (stdout and stderr)
  crash.txt     on a crash or sanitizer stop: the stop reason, every thread's backtrace, the registers
  core          on a crash: a stack-only core (lldb `process save-core --style stack`), for `lldb -c core EXE`
  asan.*        AddressSanitizer reports (ASAN_OPTIONS log_path), when the sanitizer build stops
  session.json  exe, build time, git revision and dirty state, data root, exit status
A clean exit leaves only game.log and session.json.

Default: the AddressSanitizer build (build/xbuild/<target>/game-address/ee-aoc), which stops at the first bad
memory access instead of crashing later from its damage; it runs about half as fast as the plain build.
--fast plays the plain build (still under lldb, near full speed). --build (re)builds the chosen build first.

usage: python3.12 -I tools/play_posix.py [--fast] [--build] [--data DIR] [-- GAME ARGS...]
"""
import argparse, datetime, json, os, pathlib, platform, re, shutil, subprocess, sys, time

ROOT = pathlib.Path(__file__).resolve().parent.parent


def target_name():
    system = {'Darwin': 'macos', 'Linux': 'linux'}.get(platform.system())
    if system is None:
        sys.exit('play_posix.py: macOS or Linux only (on Windows: tools/harness/play.py)')
    machine = platform.machine().lower()
    arch = 'arm64' if machine in ('arm64', 'aarch64') else 'x86_64'
    return f'{system}-{arch}'


def git(*args):
    r = subprocess.run(['git', *args], cwd=ROOT, capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--fast', action='store_true', help='the plain build instead of the AddressSanitizer one')
    ap.add_argument('--build', action='store_true', help='build the chosen exe first (tools/xbuild.py game)')
    ap.add_argument('--data', default=str(ROOT / 'data'), help='the data root (EE_DATA_ROOT), default data/')
    ap.add_argument('--exe', help='another exe to play (default: the chosen build)')
    ap.add_argument('--out', help='the session folder (default build/playtest/<time>)')
    ap.add_argument('game_args', nargs=argparse.REMAINDER, help='arguments for the game, after --')
    a = ap.parse_args()

    if shutil.which('lldb') is None:
        sys.exit('play_posix.py: lldb not found (macOS: xcode-select --install)')
    target = target_name()
    build_dir = ROOT / 'build/xbuild' / target / ('game' if a.fast else 'game-address')
    exe = pathlib.Path(a.exe).resolve() if a.exe else build_dir / 'ee-aoc'
    if a.build:
        cmd = [sys.executable, '-I', str(ROOT / 'tools/xbuild.py'), 'game', '-j', str(os.cpu_count() or 8)]
        if not a.fast:
            cmd += ['--sanitize', 'address']
        if subprocess.run(cmd, cwd=ROOT).returncode != 0:
            sys.exit('play_posix.py: the build failed')
    if not exe.is_file():
        sys.exit(f'play_posix.py: {exe} is missing (add --build)')
    data = pathlib.Path(a.data).resolve()
    if not (data / 'Data').is_dir():
        sys.exit(f'play_posix.py: {data} has no Data folder (tools/fetch_data.py)')

    out = pathlib.Path(a.out) if a.out else ROOT / 'build/playtest' / datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    out = out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    game_args = a.game_args[1:] if a.game_args[:1] == ['--'] else a.game_args

    env = dict(os.environ)
    env['EE_DATA_ROOT'] = str(data)
    # abort_on_error: the sanitizer's stop becomes a SIGABRT that lldb catches (crash.txt gets the backtraces)
    env['ASAN_OPTIONS'] = ':'.join(filter(None, [env.get('ASAN_OPTIONS'),
                                                 f'log_path={out}/asan', 'abort_on_error=1', 'print_summary=1']))

    exe_time = datetime.datetime.fromtimestamp(exe.stat().st_mtime).isoformat(timespec='seconds')
    session = {
        'exe': str(exe), 'exe_built': exe_time, 'sanitizer': None if a.fast else 'address',
        'git': git('rev-parse', '--short', 'HEAD'), 'git_dirty': bool(git('status', '--porcelain', '--untracked-files=no')),
        'data': str(data), 'args': game_args, 'started': datetime.datetime.now().isoformat(timespec='seconds'),
    }
    newest_source = git('log', '-1', '--format=%cI')
    if newest_source and newest_source[:19] > exe_time:
        print(f'play_posix.py: note: {exe.name} ({exe_time}) is older than the last commit ({newest_source[:19]}); '
              'add --build for the current source')

    crash = out / 'crash.txt'
    lldb = ['lldb', '--batch',
            # the game ignores nothing lldb would otherwise stop on: let the usual noise through
            '-o', 'process handle SIGPIPE SIGUSR1 SIGUSR2 -n false -p true -s false',
            '-o', 'run',
            # -k: only when the process stops (a crash or the sanitizer's abort), never after a clean exit
            '-k', 'process status',
            '-k', 'thread backtrace all',
            '-k', 'register read',
            '-k', f'process save-core --style stack "{out}/core"',
            '-k', 'process kill',
            '-k', 'quit',
            '--', str(exe), *game_args]

    print(f'play_posix.py: {exe} under lldb; evidence -> {out}/')
    started = time.time()
    with open(out / 'game.log', 'w', errors='replace') as log:
        p = subprocess.Popen(lldb, cwd=data, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, errors='replace')
        try:
            for line in p.stdout:
                log.write(line)
                log.flush()
                sys.stdout.write(line)
        except KeyboardInterrupt:
            p.terminate()
        try:
            p.wait(timeout=120)
        except subprocess.TimeoutExpired:
            p.kill()

    text = (out / 'game.log').read_text(errors='replace')
    # crash.txt: the log from lldb's first stop report (the stop reason, the backtraces, the registers) on
    lines = text.splitlines(keepends=True)
    stop = next((i for i, l in enumerate(lines) if re.match(r'Process \d+ stopped', l)), None)
    crashed = stop is not None
    if crashed:
        crash.write_text(''.join(lines[stop:]))
    exited = next((l for l in reversed(text.splitlines()) if 'exited with status' in l), None)
    session.update({'ended': datetime.datetime.now().isoformat(timespec='seconds'),
                    'minutes': round((time.time() - started) / 60, 1),
                    'crashed': crashed, 'exit': exited,
                    'asan_reports': sorted(x.name for x in out.glob('asan.*'))})
    (out / 'session.json').write_text(json.dumps(session, indent=2) + '\n')
    if crashed:
        print(f'\nplay_posix.py: CRASH -> {crash}' +
              (f' (and {", ".join(session["asan_reports"])})' if session['asan_reports'] else ''))
        return 1
    print(f'\nplay_posix.py: {exited or "ended"} -> {out}/')
    return 0


if __name__ == '__main__':
    sys.exit(main())
