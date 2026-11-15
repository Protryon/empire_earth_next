"""TEST ONLY: harness slots, so that several live-game sessions can run on this machine at once.

A slot K (0..N-1) is a lock directory <main checkout>/build/harness.slot.K, taken by mkdir (atomic) and released on
exit. It is shared by every worktree of the repository (the main checkout is found through git's common directory;
EE_HARNESS_SLOT_ROOT names another folder). Its owner.json records the owner, the holding process (pid and its start
time) and the command; a slot whose process is gone is stale and is taken over. A session in slot K keeps off every
other slot's resources (fixture_game.py, lan_game.py, game_session.py read EE_HARNESS_SLOT or --slot):

  - the single-instance mutex: EE_TEST_INSTANCE=s<K> (s<K>-host / s<K>-joiner for lan_game.py);
  - the TCP protocol's ports 33334/33335 and the WON lobby's ports (LAN broadcast 8585, lobby 8888): moved by
    port_offset(K) = 16 * (K + 1) through EE_TEST_PORT_OFFSET (the game reads it under EE_TEST_UI); the legacy harness
    uses --instance K + 1 (exe_boot.c --port-offset, the same 16 * (K + 1)); offset 0 stays for unslotted runs;
  - the stage build/fixture_game/stage-s<K> and evidence directories named ...-s<K>.

  python -I tools/harness/slot.py run [--owner NAME] [--slots N] [--wait S] -- COMMAND ...
  python -I tools/harness/slot.py status
  python -I tools/harness/slot.py release K      (a slot of yours left behind; a stale one is taken over anyway)

`run` waits for the first free slot (polling every 10 s, --wait seconds at most, default: forever), runs COMMAND with
EE_HARNESS_SLOT=K in its environment (on Windows in a job object, so the game dies with the command), releases the
slot and exits with COMMAND's code. N: --slots, else EE_HARNESS_SLOTS, else 3. While the old single lock
(build/harness.lock) is held, it counts as one slot taken.
"""
import argparse
import json
import os
import pathlib
import shutil
import socket
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
DEFAULT_SLOTS = 3
MAX_SLOTS = 8
PORT_STEP = 16          # as game_session.py's --instance: 33334 + 16 k
NO_OWNER_GRACE = 120    # seconds a slot directory may lack its owner.json (being written) before it is stale
WINDOWS = sys.platform == 'win32'


# ---- what a slot means (the drivers' side) ----

def current(arg=None):
    """The slot of this session: --slot (arg), else EE_HARNESS_SLOT, else None (unslotted: offset 0, the default
    mutex, the old stage names)."""
    value = arg if arg is not None else os.environ.get('EE_HARNESS_SLOT')
    if value is None or value == '':
        return None
    k = int(value)
    if not 0 <= k < MAX_SLOTS:
        raise ValueError(f'harness slot {k} is not in 0..{MAX_SLOTS - 1}')
    return k


def port_offset(k):
    """The TCP protocol's and the WON lobby's port offset in slot k (0 unslotted)."""
    return 0 if k is None else PORT_STEP * (k + 1)


def legacy_instance(k):
    """game_session.py's --instance for slot k (1..: its ports move by 16 * instance, as port_offset)."""
    return None if k is None else k + 1


def instance_name(k, label=None):
    """EE_TEST_INSTANCE for slot k (None: unset, unless a label names the instance as lan_game.py's ends)."""
    if k is None:
        return label
    return f's{k}' + (f'-{label}' if label else '')


def suffix(k):
    """The stage/evidence name suffix of slot k ('' unslotted)."""
    return '' if k is None else f'-s{k}'


def game_env(env, k, label=None):
    """env with this slot's game switches (EE_TEST_INSTANCE, EE_TEST_PORT_OFFSET) and no inherited ones."""
    env = dict(env)
    env.pop('EE_TEST_INSTANCE', None)
    env.pop('EE_TEST_PORT_OFFSET', None)
    name = instance_name(k, label)
    if name:
        env['EE_TEST_INSTANCE'] = name
    if k is not None:
        env['EE_TEST_PORT_OFFSET'] = str(port_offset(k))
    return env


# ---- the locks ----

def lock_root():
    """The folder holding the slot locks: EE_HARNESS_SLOT_ROOT, else the main checkout's build/ (shared by all
    worktrees: git's common directory's parent), else this checkout's build/."""
    if os.environ.get('EE_HARNESS_SLOT_ROOT'):
        return pathlib.Path(os.environ['EE_HARNESS_SLOT_ROOT'])
    try:
        common = subprocess.run(['git', 'rev-parse', '--path-format=absolute', '--git-common-dir'], cwd=ROOT,
                                capture_output=True, text=True, timeout=30).stdout.strip()
        if common:
            return pathlib.Path(common).parent / 'build'
    except (OSError, subprocess.SubprocessError):
        pass
    return ROOT / 'build'


def slot_count(arg=None):
    n = arg if arg is not None else int(os.environ.get('EE_HARNESS_SLOTS') or DEFAULT_SLOTS)
    if not 1 <= n <= MAX_SLOTS:
        raise ValueError(f'--slots must be 1..{MAX_SLOTS}')
    return n


def process_start(pid):
    """An identity for a live process (its start time), None when no process has this pid."""
    if WINDOWS:
        import ctypes
        from ctypes import wintypes
        k32 = ctypes.WinDLL('kernel32', use_last_error=True)
        k32.OpenProcess.restype = wintypes.HANDLE
        handle = k32.OpenProcess(0x1000, False, pid)   # PROCESS_QUERY_LIMITED_INFORMATION
        if not handle:
            return None if ctypes.get_last_error() == 87 else 'unknown'   # 87: no such process; else (denied) alive
        try:
            code = wintypes.DWORD()
            if k32.GetExitCodeProcess(handle, ctypes.byref(code)) and code.value != 259:   # STILL_ACTIVE
                return None
            times = [wintypes.FILETIME() for _ in range(4)]
            if not k32.GetProcessTimes(handle, *[ctypes.byref(t) for t in times]):
                return 'unknown'
            return str((times[0].dwHighDateTime << 32) | times[0].dwLowDateTime)
        finally:
            k32.CloseHandle(handle)
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return None
    except PermissionError:
        return 'unknown'
    try:
        return pathlib.Path(f'/proc/{pid}/stat').read_text().split()[21]
    except (OSError, IndexError):
        return 'unknown'


def read_owner(path):
    try:
        return json.loads((path / 'owner.json').read_text())
    except (OSError, ValueError):
        return None


def stale_reason(path, now=None):
    """Why the slot lock at path is stale, or None when its holder lives (or it is too young to judge)."""
    owner = read_owner(path)
    now = time.time() if now is None else now
    if owner is None:
        try:
            age = now - path.stat().st_mtime
        except OSError:
            return None   # gone already
        return f'no owner.json for {age:.0f} s' if age > NO_OWNER_GRACE else None
    start = process_start(int(owner.get('pid', 0)))
    if start is None:
        return f'process {owner.get("pid")} ({owner.get("owner")}) is gone'
    if start != 'unknown' and owner.get('pid_start') not in (None, 'unknown', start):
        return f'process {owner.get("pid")} is another process now (pid reused)'
    return None


def break_stale(path, reason):
    """Removes a stale lock: renamed first (atomic: one remover wins), checked again, then deleted."""
    grave = path.with_name(f'{path.name}.stale.{os.getpid()}.{time.monotonic_ns()}')
    try:
        os.rename(path, grave)
    except OSError:
        return False
    if stale_reason(grave) is None and read_owner(grave) is not None:   # taken again meanwhile: put it back
        try:
            os.rename(grave, path)
            return False
        except OSError:
            pass
    shutil.rmtree(grave, ignore_errors=True)
    print(f'SLOT {path.name}: stale lock removed ({reason})', file=sys.stderr, flush=True)
    return True


def legacy_lock_held(root):
    return (root / 'harness.lock').is_dir()


def try_take(root, n, owner, command):
    """Takes the first free slot -> (k, path), or None when all n are held."""
    root.mkdir(parents=True, exist_ok=True)
    usable = n - (1 if legacy_lock_held(root) else 0)
    for k in range(max(usable, 0)):
        path = root / f'harness.slot.{k}'
        try:
            path.mkdir()
        except FileExistsError:
            reason = stale_reason(path)
            if reason is None or not break_stale(path, reason):
                continue
            try:
                path.mkdir()
            except FileExistsError:
                continue
        pid = os.getpid()
        record = dict(owner=owner, pid=pid, pid_start=process_start(pid), host=socket.gethostname(), cwd=os.getcwd(),
                      command=command, taken=time.strftime('%Y-%m-%d %H:%M:%S'), slot=k)
        tmp = path / 'owner.json.tmp'
        tmp.write_text(json.dumps(record, indent=1) + '\n')
        os.replace(tmp, path / 'owner.json')
        return k, path
    return None


def release(path):
    owner = read_owner(path)
    if owner is not None and owner.get('pid') != os.getpid():
        raise RuntimeError(f'{path} is held by {owner.get("owner")} (pid {owner.get("pid")}), not by this process')
    shutil.rmtree(path, ignore_errors=True)


def acquire(n=None, owner=None, command='', wait=None, poll=None, root=None):
    """Blocks until a slot is free -> (k, path); raises TimeoutError after `wait` seconds."""
    root = lock_root() if root is None else root
    n = slot_count(n)
    poll = float(os.environ.get('EE_HARNESS_SLOT_POLL') or 10.0) if poll is None else poll
    owner = owner or os.environ.get('EE_HARNESS_OWNER') or default_owner()
    end = None if wait is None else time.monotonic() + wait
    said = False
    while True:
        taken = try_take(root, n, owner, command)
        if taken:
            return taken
        if end is not None and time.monotonic() > end:
            raise TimeoutError(f'no free harness slot of {n} in {wait} s')
        if not said:
            print(f'SLOT waiting: all {n} slots in {root} held' +
                  (' (the old harness.lock counts as one)' if legacy_lock_held(root) else ''), file=sys.stderr, flush=True)
            said = True
        time.sleep(poll)


def default_owner():
    try:
        branch = subprocess.run(['git', 'rev-parse', '--abbrev-ref', 'HEAD'], cwd=ROOT, capture_output=True, text=True,
                                timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        branch = ''
    return branch or ROOT.name


def kill_on_close_job():
    """Windows: a job object whose processes die when this process ends (its handle closes), or None."""
    if not WINDOWS:
        return None
    import ctypes
    from ctypes import wintypes

    class IoCounters(ctypes.Structure):
        _fields_ = [(n, ctypes.c_ulonglong) for n in ('r', 'w', 'o', 'rb', 'wb', 'ob')]

    class Basic(ctypes.Structure):
        _fields_ = [('PerProcessUserTimeLimit', ctypes.c_longlong), ('PerJobUserTimeLimit', ctypes.c_longlong),
                    ('LimitFlags', wintypes.DWORD), ('MinimumWorkingSetSize', ctypes.c_size_t),
                    ('MaximumWorkingSetSize', ctypes.c_size_t), ('ActiveProcessLimit', wintypes.DWORD),
                    ('Affinity', ctypes.c_size_t), ('PriorityClass', wintypes.DWORD),
                    ('SchedulingClass', wintypes.DWORD)]

    class Extended(ctypes.Structure):
        _fields_ = [('Basic', Basic), ('Io', IoCounters), ('ProcessMemoryLimit', ctypes.c_size_t),
                    ('JobMemoryLimit', ctypes.c_size_t), ('PeakProcessMemoryUsed', ctypes.c_size_t),
                    ('PeakJobMemoryUsed', ctypes.c_size_t)]
    k32 = ctypes.WinDLL('kernel32', use_last_error=True)
    k32.CreateJobObjectW.restype = wintypes.HANDLE
    job = k32.CreateJobObjectW(None, None)
    if not job:
        return None
    info = Extended()
    info.Basic.LimitFlags = 0x2000   # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
    if not k32.SetInformationJobObject(wintypes.HANDLE(job), 9, ctypes.byref(info), ctypes.sizeof(info)):
        k32.CloseHandle(wintypes.HANDLE(job))
        return None
    return job


def assign_job(job, proc):
    if job is None:
        return False
    import ctypes
    from ctypes import wintypes
    return bool(ctypes.windll.kernel32.AssignProcessToJobObject(wintypes.HANDLE(job), wintypes.HANDLE(int(proc._handle))))


def run(command, n=None, owner=None, wait=None):
    k, path = acquire(n, owner, ' '.join(command), wait)
    print(f'SLOT {k} taken ({path}): EE_HARNESS_SLOT={k}, port offset {port_offset(k)}', file=sys.stderr, flush=True)
    try:
        env = dict(os.environ, EE_HARNESS_SLOT=str(k))
        job = kill_on_close_job()
        proc = subprocess.Popen(command, env=env)
        assign_job(job, proc)
        try:
            return proc.wait()
        except KeyboardInterrupt:
            proc.terminate()
            return proc.wait()
    finally:
        release(path)
        print(f'SLOT {k} released', file=sys.stderr, flush=True)


def status(n=None):
    root = lock_root()
    n = slot_count(n)
    print(f'slots in {root} (N={n})' + ('; the old harness.lock is held' if legacy_lock_held(root) else ''))
    for path in sorted(root.glob('harness.slot.*')):
        if '.stale.' in path.name:
            continue
        owner = read_owner(path) or {}
        reason = stale_reason(path)
        print(f'  {path.name}: {owner.get("owner", "?")} pid {owner.get("pid", "?")} since {owner.get("taken", "?")}'
              + (f' STALE ({reason})' if reason else '') + f'\n      {owner.get("command", "")}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split('\n', 2)[2])
    sub = ap.add_subparsers(dest='action', required=True)
    r = sub.add_parser('run', help='take a slot, run COMMAND in it, release it')
    r.add_argument('--owner', help='who holds the slot (default: EE_HARNESS_OWNER, else the branch)')
    r.add_argument('--slots', type=int, help=f'how many slots the machine has (default EE_HARNESS_SLOTS or {DEFAULT_SLOTS})')
    r.add_argument('--wait', type=float, help='give up after S seconds (exit 75)')
    r.add_argument('command', nargs=argparse.REMAINDER)
    s = sub.add_parser('status', help='the slots and their holders')
    s.add_argument('--slots', type=int)
    rel = sub.add_parser('release', help='remove slot K (its holder must be gone or be you)')
    rel.add_argument('k', type=int)
    a = ap.parse_args()
    if a.action == 'status':
        status(a.slots)
        return 0
    if a.action == 'release':
        path = lock_root() / f'harness.slot.{a.k}'
        reason = stale_reason(path)
        if path.exists() and reason is None:
            owner = read_owner(path) or {}
            print(f'{path} is held by a live process: {owner.get("owner")} pid {owner.get("pid")}; not removed')
            return 1
        shutil.rmtree(path, ignore_errors=True)
        print(f'{path} removed')
        return 0
    command = a.command[1:] if a.command[:1] == ['--'] else a.command
    if not command:
        ap.error('run: no COMMAND')
    try:
        return run(command, a.slots, a.owner, a.wait)
    except TimeoutError as error:
        print(f'SLOT {error}', file=sys.stderr, flush=True)
        return 75


if __name__ == '__main__':
    sys.exit(main())
