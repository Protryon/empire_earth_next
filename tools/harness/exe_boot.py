"""Differential guarded boot check: original EE-AOC.exe vs a relinked one (tools/boot/exe_boot.c).

Opt-in test overlays: --windowed --modes run [--software-display] [--silent-audio]
[--checkpoint graphics|audio].
See docs/WINDOWED_TESTING.md. Only staged build artifacts are patched. Windowed runs
compare real graphics milestones, cleanup and game preferences; full traces remain
diagnostic evidence. Default success requires a presented frame, never an early guard.

  python -I tools/boot/exe_boot.py [<relinked exe>] [--modes oep,winmain,run] [--timeout MS]

Stages build/boot/exe_o (orig-bin/aoc/EE-AOC.unwrapped.exe) and build/boot/exe_r (the relinked exe, default
build/exe_roundtrip/EE-AOC.exe), each with copies of the retail DLLs from orig-bin/aoc/retail (Language.dll is the
retail one). Nothing is read from or written to the game install by this script; the processes run with their
staging directory as the working directory under exe_boot.exe's guards (every write-capable API stops and kills the
process before it executes). Each mode's log is normalised (staging directory names) and the two are diffed:
  oep      IAT after the loader bound it (module+offset per slot)
  winmain  IAT + .data/.bss after every static initializer (dumps in build/boot/exe_*.data.bin). Heap addresses
           are randomised per process (the original against itself differs in ~2700 dwords), so the original runs
           twice: a dword may differ from the relinked run only if it also differs between the two original runs,
           or if both values are heap-like pointers (0x10000..0x10000000, outside the exe image).
  run      the trace of file opens (read-only anywhere, writes only inside the staging directory), windows and
           guards up to the first STOP; compared order-insensitively (other threads interleave differently), the
           STOP line exactly.
Prints PASS/FAIL per mode (and the first differing lines); exit 1 on any FAIL. Needs gcc from MSYS2 mingw32
(the harness is built if missing).

"Registry + window" (owner decision 2026-10-23):
  - every process run is wrapped in a snapshot/restore of HKCU\\Software\\Mad Doc Software\\EE-AOC, the game's own
    key and the only one the harness may touch (reg export before; after: delete + reg import, checked byte for byte
    against the snapshot; the key is deleted if it was absent). The parent key's subkeys are listed before and
    after: a run that creates anything else under it FAILs (left for the owner; never deleted here).
    A marker file (build/boot/reg_restore_pending.txt) makes the next start restore first if a run was interrupted.
    The key each run leaves behind is exported to build/boot/exe_{o,r}.<mode>.reg and compared.
  - the run mode stages a copy of the game data (Data, Users, redist, *.cfg; copied once from the install, read
    only, into build/boot/aoc_install_copy), allows windows and DirectDraw initialisation, and captures the largest
    window at the STOP to build/boot/exe_{o,r}.run.bmp. --reg-set NAME=dword:N writes a value into the EE-AOC key
    inside the snapshot (e.g. to try a windowed option).
"""
import argparse
import hashlib
import json
import difflib
import re
import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
BOOT = ROOT / "build" / "boot"


HARNESS_SOURCES = ["exe_boot.c", "differential/ptsync.h", "differential/ptwatch.h", "differential/coverage.h"]   # ptsync.h: agent hash's opt-in --pt-sync (round 39)


def _harness_stale(exe):
    return not exe.exists() or any(exe.stat().st_mtime < (HERE / n).stat().st_mtime
                                   for n in HARNESS_SOURCES if (HERE / n).exists())


def build_harness():
    """Rebuild exe_boot.exe when a source is newer. Parallel sessions (round 39): built under a lock file into a
    temporary name and renamed, so no session ever starts a half-written harness."""
    import os
    import time
    exe = BOOT / "exe_boot.exe"
    if not _harness_stale(exe):
        return exe
    BOOT.mkdir(parents=True, exist_ok=True)
    lock = BOOT / "exe_boot.build.lock"
    end = time.monotonic() + 300
    while True:
        try:
            fd = os.open(str(lock), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            break
        except FileExistsError:
            if time.monotonic() > end or time.time() - lock.stat().st_mtime > 300:
                lock.unlink(missing_ok=True)   # a stale lock from a killed build
                continue
            time.sleep(0.5)
    try:
        os.close(fd)
        if _harness_stale(exe):
            gcc = pathlib.Path(r"C:\msys64\mingw32\bin\gcc.exe")
            tmp = BOOT / f"exe_boot.{os.getpid()}.tmp.exe"
            subprocess.run([str(gcc), "-O1", "-Wall", "-Wno-format-truncation", "-static", "-o", str(tmp),
                            str(HERE / "exe_boot.c"), "-lgdi32", "-ldxguid", "-ldbghelp"], check=True,
                           env={"PATH": str(gcc.parent), "SYSTEMROOT": r"C:\Windows", "TEMP": str(BOOT)})
            for old in BOOT.glob("exe_boot.old.*.exe"):   # earlier images no session runs any more
                try:
                    old.unlink()
                except OSError:
                    pass
            try:
                os.replace(tmp, exe)
            except PermissionError:   # a running session executes exe_boot.exe: Windows still allows a rename
                exe.rename(BOOT / f"exe_boot.old.{time.time_ns()}.exe")
                os.replace(tmp, exe)
    finally:
        lock.unlink(missing_ok=True)
    return exe


INSTALL = pathlib.Path(r"C:\Program Files (x86)\Empire Earth\Empire Earth - The Art of Conquest")
# game data the run mode needs, copied (read-only access to the install) once into a cache under build/boot and
# from there into every staging directory; the community DDraw.dll/dreXmod/discord files are not copied
GAME_FILES = ["Data", "Users", "redist", "WONLobby.cfg", "zzWONVersion.cfg"]
CACHE = BOOT / "aoc_install_copy"
REG_PARENT = r"HKCU\Software\Mad Doc Software"
REG_KEY = REG_PARENT + r"\EE-AOC"     # the only key the harness snapshots, writes (--reg-set) and restores
REG_PENDING = BOOT / "reg_restore_pending.txt"   # present while a run may have changed the key: restore at next start
REG_SNAP = BOOT / "reg_snapshot.reg"
# Round 39 (parallel sessions): the shared snapshot of lease holders (batches, --instance sessions); see REG_LEASES.
REG_BATCH_PENDING = BOOT / "reg_batch_pending.txt"
REG_BATCH_SNAP = BOOT / "reg_batch_snapshot.reg"
INSTANCE_SUBTREE = "windowed-test instance {}"   # per-instance view of the game key (exe_boot.c --registry-subtree)


def install_cache():
    if (CACHE / ".complete").exists():
        return CACHE
    if CACHE.exists():
        if CACHE.resolve().parent != BOOT.resolve():
            raise ValueError('Cache deletion must remain inside build/boot')
        shutil.rmtree(CACHE)
    CACHE.mkdir(parents=True)
    for n in GAME_FILES:
        src = INSTALL / n
        if src.is_dir():
            shutil.copytree(src, CACHE / n)
        elif src.exists():
            shutil.copy(src, CACHE / n)
    (CACHE / ".complete").write_text("copied from the install (read only)\n")
    return CACHE


def rename_import(path, old, new):
    """TEST ONLY, on a staged copy: rename an imported DLL (same length) in the import descriptors' name strings."""
    import pefile
    data = bytearray(pathlib.Path(path).read_bytes())
    pe = pefile.PE(data=bytes(data), fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    hits = [e for e in pe.DIRECTORY_ENTRY_IMPORT if e.dll.lower() == old.lower()]
    if len(hits) != 1 or len(old) != len(new):
        raise ValueError(f"{path}: cannot rename import {old} -> {new}")
    off = pe.get_offset_from_rva(hits[0].struct.Name)
    data[off:off + len(new)] = new.encode()
    pathlib.Path(path).write_bytes(bytes(data))


def stage(name, build, data=True, silent_audio=True):
    """Stage the build under test (hbuild.Build) in build/boot/<name>: its images (the test profile compiles the
    windowed/software policy in, nothing is patched), the third-party DLLs, Language.dll, the silent mixer built for
    the build, the RDP mode-list proxy when the display needs it, and a copy of the game data."""
    d = BOOT / name
    if d.resolve().parent != BOOT.resolve():
        raise ValueError('Staging deletion must remain immediately inside build/boot')
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    for n, src in build.stage_files().items():
        shutil.copy(src, d / n)
    shutil.copy(build.exe_map_path, d / "EE-AOC.map")
    sys.path.insert(0, str(HERE))
    import rdp_modes   # RDP session: the primary display lacks 640x480/800x600x32 (owner 2026-11-01)
    if rdp_modes.needed():
        rename_import(d / "DX7HRDisplay.dll", "DDRAW.dll", "DDRDP.dll")
        rdp_modes.build_proxy(d)
        print('display: RDP mode-list proxy staged (DDRDP.dll)', flush=True)
    if silent_audio:
        from silent_mixer import build_silent_mixer
        build_silent_mixer(d, build)
    if data:
        c = install_cache()
        for n in GAME_FILES:
            if (c / n).is_dir():
                shutil.copytree(c / n, d / n)
            elif (c / n).exists():
                shutil.copy(c / n, d / n)
    return d


def reg(*args):
    r = subprocess.run(["reg.exe", *args], capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr).strip()


def reg_export(path):
    """Export the EE-AOC key to path; False if the key does not exist."""
    if pathlib.Path(path).exists():
        pathlib.Path(path).unlink()
    return reg("export", REG_KEY, str(path), "/y")[0] == 0


REG_LOCK = BOOT / "reg_owner.lock"   # round 39: a plain (whole-key) session's exclusive ownership of the key
REG_LOCK_WAIT_S = 3600
# Round 39: instance sessions and batches share the key under leases (one file per owner process, pid + start time):
# the first lease snapshots the key (REG_BATCH_SNAP), the last one to leave restores it byte-exact; a lease whose
# process is gone is pruned by the next caller (an interrupted batch or session is restored that way). Plain sessions
# need the key alone: they wait while any live lease exists, and leases wait while a plain session owns it.
REG_LEASES = BOOT / "reg_leases"
REG_MUTEX = BOOT / "reg_state.mutex"   # held for the few seconds a state change (snapshot/restore/lease) takes


def _owner_fields(path):
    try:
        lines = path.read_text().splitlines()
    except OSError:
        return None
    fields = dict(ln.split(" ", 1) for ln in lines if " " in ln)
    return int(fields.get("pid", "0") or 0), fields.get("start", "")


def _owner_text(what, extra=""):
    import os
    return f"{what}\npid {os.getpid()}\nstart {_process_start(os.getpid())}\n" + extra


def _live(path):
    owner = _owner_fields(path)
    return bool(owner and owner[0] and str(_process_start(owner[0])) == owner[1])


class _StateMutex:
    def __enter__(self):
        import os
        import time
        BOOT.mkdir(parents=True, exist_ok=True)
        end = time.monotonic() + 300
        while True:
            try:
                fd = os.open(str(REG_MUTEX), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                os.write(fd, _owner_text("registry state change").encode())
                os.close(fd)
                return self
            except FileExistsError:
                try:
                    age = time.time() - REG_MUTEX.stat().st_mtime
                except OSError:
                    continue
                if (age > 2 and not _live(REG_MUTEX)) or age > 120 or time.monotonic() > end:
                    REG_MUTEX.unlink(missing_ok=True)   # left by a killed process
                    continue
                time.sleep(0.2)

    def __exit__(self, *exc):
        REG_MUTEX.unlink(missing_ok=True)


def _prune_leases():
    """Remove leases of dead processes; return (live lease paths, the dead ones' texts)."""
    REG_LEASES.mkdir(parents=True, exist_ok=True)
    live, dead = [], []
    for lease in sorted(REG_LEASES.glob("*.lease")):
        if _live(lease):
            live.append(lease)
        else:
            try:
                dead.append(lease.read_text())
            except OSError:
                pass
            lease.unlink(missing_ok=True)
    return live, dead


def _restore_shared_if_idle(live):
    if not live and REG_BATCH_PENDING.exists():
        print("registry: restoring the shared snapshot (every lease holder has ended)", flush=True)
        reg_restore(REG_BATCH_PENDING, REG_BATCH_SNAP)
        return True
    return False


def shared_acquire(what, extra=""):
    """Take a lease on the key (batch or instance session). Returns (lease path, texts of dead leases pruned)."""
    import os
    import time
    end, told, pruned = time.monotonic() + REG_LOCK_WAIT_S, False, []
    while True:
        with _StateMutex():
            if REG_LOCK.exists() and not _live(REG_LOCK):
                print("registry: a plain session that owned the key is gone; taking over", flush=True)
                REG_LOCK.unlink(missing_ok=True)
            if not REG_LOCK.exists():
                live, dead = _prune_leases()
                pruned += dead
                if not live:
                    _restore_shared_if_idle(live)   # an interrupted lease set
                    if REG_PENDING.exists():
                        print("registry: restoring after an interrupted run", flush=True)
                        reg_restore()
                    existed = reg_export(REG_BATCH_SNAP)
                    REG_BATCH_PENDING.write_text(("existed" if existed else "absent") + "\n"
                                                 + json_line(siblings=reg_siblings()))
                lease = REG_LEASES / f"{what.split()[0]}.{os.getpid()}.lease"
                lease.write_text(_owner_text(what, extra))
                return lease, pruned
        if not told:
            print(f"registry: {REG_KEY} is owned by a plain session (pid {(_owner_fields(REG_LOCK) or (0,))[0]}); "
                  f"{what} waits", flush=True)
            told = True
        if time.monotonic() > end:
            raise SystemExit(f"registry: still owned by a plain session after {REG_LOCK_WAIT_S} s")
        time.sleep(3)


def shared_release(lease):
    """Drop a lease; the last holder restores byte-exact. Returns True if this call restored the key, else the pids
    of the leases still live (the restore is theirs)."""
    with _StateMutex():
        pathlib.Path(lease).unlink(missing_ok=True)
        live, _ = _prune_leases()
        if live:
            return [(_owner_fields(x) or (0,))[0] for x in live]
        siblings = shared_siblings()
        _restore_shared_if_idle(live)
    after = reg_siblings()
    if siblings is not False and after != siblings and not (siblings is None and after == []):
        raise SystemExit(f"registry: the runs changed the subkeys of {REG_PARENT} outside EE-AOC (before {siblings}, "
                         f"after {after}); left as is for the owner")
    return True


def json_line(**kw):
    import json
    return "json " + json.dumps(kw) + "\n"


def shared_siblings():
    import json
    try:
        for ln in REG_BATCH_PENDING.read_text().splitlines():
            if ln.startswith("json "):
                return json.loads(ln[5:]).get("siblings")
    except OSError:
        pass
    return False


def shared_recover():
    """Prune dead leases; restore the shared snapshot if no live lease is left (also a dead plain owner's). Returns the
    live lease paths."""
    with _StateMutex():
        live, _ = _prune_leases()
        if not live and not (REG_LOCK.exists() and _live(REG_LOCK)):
            _restore_shared_if_idle(live)
            if REG_PENDING.exists():
                print("registry: restoring after an interrupted run", flush=True)
                reg_restore()
                REG_LOCK.unlink(missing_ok=True)
        return live


def shared_live():
    with _StateMutex():
        return _prune_leases()[0]


def reg_lock_acquire(what):
    """A plain session's exclusive ownership: wait while a live plain session or any live lease holds the key; take
    over from dead owners (their snapshots are then restored by the caller's interrupted-run checks)."""
    import time
    end, told = time.monotonic() + REG_LOCK_WAIT_S, False
    while True:
        with _StateMutex():
            if REG_LOCK.exists() and not _live(REG_LOCK):
                print("registry: the previous owner of the key is gone; taking over", flush=True)
                REG_LOCK.unlink(missing_ok=True)
            live, _ = _prune_leases()
            if not REG_LOCK.exists() and not live:
                _restore_shared_if_idle(live)
                REG_LOCK.write_text(_owner_text(what))
                return
            holder = (_owner_fields(REG_LOCK) or (0,))[0] if REG_LOCK.exists() else [(_owner_fields(x) or (0,))[0] for x in live]
        if not told:
            print(f"registry: {REG_KEY} is in use by pid {holder}; {what} waits (parallel sessions: "
                  f"tools/boot/game_batch.py or --instance)", flush=True)
            told = True
        if time.monotonic() > end:
            raise SystemExit(f"registry: still in use after {REG_LOCK_WAIT_S} s")
        time.sleep(3)


def reg_lock_release():
    import os
    owner = _owner_fields(REG_LOCK)
    if owner and owner[0] == os.getpid():
        REG_LOCK.unlink(missing_ok=True)


def reg_snapshot():
    """Snapshot before a plain run (restoring first if a previous run did not finish its restore). Round 39: under the
    exclusive owner lock, so concurrent plain sessions wait instead of clobbering each other's snapshot."""
    reg_lock_acquire("session")
    if REG_PENDING.exists():
        print("registry: restoring after an interrupted run")
        reg_restore()
    existed = reg_export(REG_SNAP)
    REG_PENDING.write_text("existed\n" if existed else "absent\n")
    return existed


def reg_restore(pending=REG_PENDING, snap=REG_SNAP):
    existed = pending.exists() and pending.read_text().startswith("existed")
    if reg("query", REG_KEY)[0] == 0:
        code, log = reg("delete", REG_KEY, "/f")
        if code:
            raise SystemExit(f"registry restore failed (delete): {log}")
    if existed:
        code, log = reg("import", str(snap))
        if code:
            raise SystemExit(f"registry restore failed (import): {log}")
        check = BOOT / f"reg_check.{pending.stem}.reg"
        reg_export(check)
        if check.read_bytes() != snap.read_bytes():
            raise SystemExit("registry restore: the key differs from its snapshot")
        check.unlink()
    elif reg("query", REG_KEY)[0] == 0:
        raise SystemExit("registry restore: the key still exists")
    pending.unlink(missing_ok=True)


def _process_start(pid):
    """Creation time (FILETIME int) of a live process, or None when it is gone (ctypes; no psutil here)."""
    import ctypes
    from ctypes import wintypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.restype = wintypes.HANDLE
    h = k.OpenProcess(0x1000, False, int(pid))   # PROCESS_QUERY_LIMITED_INFORMATION
    if not h:
        return None
    try:
        code = wintypes.DWORD()
        times = [wintypes.FILETIME() for _ in range(4)]
        if not k.GetExitCodeProcess(h, ctypes.byref(code)) or code.value != 259:   # STILL_ACTIVE
            return None
        if not k.GetProcessTimes(h, *[ctypes.byref(t) for t in times]):
            return None
        return (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
    finally:
        k.CloseHandle(h)


def batch_begin(instances=()):
    """Batch-level lease (tools/boot/game_batch.py): the snapshot is taken by the first lease and kept across the
    whole batch. Returns the lease record plus the dead leases pruned (an interrupted batch's instances)."""
    lease, pruned = shared_acquire("batch", json_line(instances=list(instances)))
    return dict(lease=str(lease), pruned=pruned, snapshot=str(REG_BATCH_SNAP),
                existed=REG_BATCH_PENDING.read_text().startswith("existed"))


def batch_end(begin):
    """Release the batch lease. True: this call restored the key byte-exact; a list: the pids of other live lease
    holders (instance sessions of other users), the last of which restores."""
    return shared_release(begin["lease"])


def seed_instance_subtree(subtree, snapshot, reg_set=()):
    """(Re)create EE-AOC\\<subtree> as a copy of the snapshot's game key (without any "windowed-test *" subtree)
    plus the run's pins: the instance's private view of its preferences (exe_boot.c --registry-subtree)."""
    if not subtree.startswith("windowed-test ") or "\\" in subtree:
        raise ValueError("instance subtree must be a single 'windowed-test ...' key name")
    key = REG_KEY + "\\" + subtree
    if reg("query", key)[0] == 0:
        code, log = reg("delete", key, "/f")
        if code:
            raise RuntimeError(f"instance registry reset failed: {log}")
    raw = pathlib.Path(snapshot).read_bytes() if pathlib.Path(snapshot).exists() else b""
    sections = 0
    if raw[:2] == b"\xff\xfe":
        root = "[HKEY_CURRENT_USER\\Software\\Mad Doc Software\\EE-AOC"
        out, keep = ["Windows Registry Editor Version 5.00", ""], False
        for line in raw.decode("utf-16").splitlines()[1:]:
            if line.startswith("["):
                rest = line[len(root):] if line.lower().startswith(root.lower()) else None
                keep = rest is not None and (rest == "]" or (rest.startswith("\\")
                                                            and not rest.lower().startswith("\\windowed-test ")))
                if keep:
                    line = root + "\\" + subtree + rest
                    sections += 1
            if keep:
                out.append(line)
        if sections:
            seed = BOOT / f"reg_seed.{subtree.replace(' ', '_')}.reg"
            seed.write_bytes(b"\xff\xfe" + ("\r\n".join(out) + "\r\n").encode("utf-16-le"))
            code, log = reg("import", str(seed))
            if code:
                raise RuntimeError(f"instance registry seed failed: {log}")
    if not sections:
        code, log = reg("add", key, "/f")
        if code:
            raise RuntimeError(f"instance registry create failed: {log}")
    for item in reg_set:
        reg_add(key, item)
    return key


def reg_add(key, item):
    name, value = item.split("=", 1)
    typ, val = value.split(":", 1) if ":" in value else ("sz", value)
    code, log = reg("add", key, "/v", name, "/t", {"dword": "REG_DWORD", "sz": "REG_SZ"}[typ], "/d", val, "/f")
    if code:
        raise RuntimeError(f'Test registry setup failed: {log}')


def reg_siblings():
    """The parent key's subkeys (None if the parent does not exist): a run must not add any but EE-AOC."""
    code, out = reg("query", REG_PARENT)
    return None if code else sorted(ln.strip() for ln in out.splitlines() if ln.strip().upper().startswith("HKEY_"))


def run(harness, d, mode, timeout, dump=None, reg_set=(), reg_out=None, windowed=False, checkpoint='frame', trace_functions=None, live_log=None, loopback=False, harness_extra=(),
        registry='self', reg_subtree=None, on_start=None):
    """registry='self' (default): snapshot/restore around this run. registry='batch' (round 39): a batch
    lease (batch_begin/batch_end) holds the shared snapshot; this run neither snapshots nor restores. registry='self'
    with reg_subtree: this run takes its own lease on the shared snapshot (parallel --instance sessions). reg_subtree (round 39,
    parallel instances): the game key is redirected to EE-AOC\\<reg_subtree>, seeded from the snapshot plus reg_set;
    reg_set and reg_out then apply to that subtree. on_start(): called right before the harness starts."""
    cmd = [str(harness), str(d), "EE-AOC.exe", mode, str(timeout)] + ([str(dump)] if dump else [])
    if windowed:
        if not dump:
            cmd.append("")
        cmd.append("--windowed")
        if checkpoint == 'graphics':
            cmd.append('--graphics-checkpoint')
        elif checkpoint == 'audio':
            cmd.append('--audio-checkpoint')
        elif checkpoint == 'session':
            cmd.append('--live-session')
    if trace_functions:
        cmd += ['--trace-functions', str(trace_functions)]
    if loopback:
        cmd.append('--loopback-only')
    cmd += list(harness_extra)   # opt-in (full-link layout: --audio-entry RVA)
    if reg_subtree:
        cmd += ["--registry-subtree", reg_subtree]
    if registry not in ('self', 'batch'):
        raise ValueError(registry)
    plain = registry == 'self' and not reg_subtree   # the default: this run owns the whole key
    lease = None
    if registry == 'batch':
        if not reg_subtree:
            raise SystemExit("registry: batch-managed runs need their own instance subtree")
        if not shared_live():
            raise SystemExit("registry: --registry-managed-by-batch but no live batch holds a lease on the key")
    before = reg_siblings() if plain else None
    if plain:
        reg_snapshot()
    elif registry == 'self':   # an --instance session outside a batch: its own lease on the shared snapshot
        lease, _ = shared_acquire(f"session {d.name}")
    try:
        if reg_subtree:
            reg_key = seed_instance_subtree(reg_subtree, REG_SNAP if plain else REG_BATCH_SNAP, reg_set)
        else:
            reg_key = REG_KEY
            for item in reg_set:   # values the harness writes into the (snapshot-restored) EE-AOC key before the run
                reg_add(REG_KEY, item)
        if on_start:
            on_start()
        try:
            if live_log:
                with pathlib.Path(live_log).open('w',encoding='utf-8') as stream:
                    r=subprocess.run(cmd,stdout=stream,stderr=subprocess.STDOUT,text=True,timeout=timeout/1000+60)
                out=pathlib.Path(live_log).read_text(encoding='utf-8')
            else:
                r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout / 1000 + 60)
                out = r.stdout
            if r.returncode:
                out += f'\nHARNESS FAILURE exit={r.returncode}: {(r.stderr or "").strip()}\n'
        except subprocess.TimeoutExpired as e:   # killing the harness kills the debuggee (kill-on-exit)
            out = ((pathlib.Path(live_log).read_text(encoding='utf-8') if live_log else (e.stdout or b'').decode(errors='replace'))
                   + '\nSTOP harness timeout (killed)\n')
        if reg_out:
            if pathlib.Path(reg_out).exists():
                pathlib.Path(reg_out).unlink()
            if reg("export", reg_key, str(reg_out), "/y")[0]:
                pathlib.Path(reg_out).write_text("(key absent)\n")
    finally:
        if plain:
            try:
                reg_restore()
            finally:
                reg_lock_release()
        elif lease:
            shared_release(lease)
    if not any(ln.startswith('STOP ') for ln in out.splitlines()) or 'process gone: yes; windows left: 0' not in out:
        out += '\nHARNESS FAILURE: missing stop/clean process termination\n'
    if not live_log:
        (BOOT/f'{d.name}.{mode}.raw.txt').write_text(out,encoding='utf-8')
    after = reg_siblings() if plain else before
    if after != before and not (before is None and after == []):   # (an emptied parent the game created)
        raise SystemExit(f"registry: the run changed the subkeys of {REG_PARENT} outside EE-AOC (before {before}, "
                         f"after {after}); left as is for the owner")
    # Debugger scheduling bookkeeping varies independently of application behavior.
    # Preserve it in raw logs, exclude it from the differential behavioral trace.
    lines = [re.sub(re.escape(d.name), "<dir>", ln, flags=re.I) for ln in out.splitlines()
             if not ln.startswith(('deferred concurrent guard breakpoint','diagnostic idle','diagnostic thread','  stack '))]
    # window handles differ per run; keep the special parents (0 = top level, fffffffd = message-only)
    return [re.sub(r"parent (?!00000000|fffffffd)[0-9a-f]{8}", "parent <hwnd>", ln) for ln in lines]


def data_verdict(o1, o2, r, image=(0x400000, 0x400000 + 0x67b000)):
    import struct
    a, b, c = (pathlib.Path(p).read_bytes() for p in (o1, o2, r))
    n = min(len(a), len(c)) // 4
    A, B, C = (struct.unpack_from(f"<{n}I", x) for x in (a, b, c))

    def heap(v):
        return 0x10000 <= v < 0x10000000 and not image[0] <= v < image[1]
    noisy = sum(1 for i in range(n) if A[i] != B[i])
    diff = [i for i in range(n) if A[i] != C[i]]
    explained = [i for i in diff if A[i] != B[i] or (heap(A[i]) and heap(C[i]))]
    rest = [i for i in diff if i not in set(explained)]
    msg = (f"{len(diff)} dwords differ from the original, {len(explained)} explained (heap pointers / also differing "
           f"between two original runs: {noisy}), {len(rest)} unexplained"
           + (": " + ", ".join(f"{0x4b4000 + 4 * i:#x} {A[i]:#x}/{C[i]:#x}" for i in rest[:8]) if rest else ""))
    return not rest and len(a) == len(c), msg


def windowed_registry(raw, directory):
    """Exclude only harness bookkeeping; retain raw exports for inspection."""
    if raw[:2] != b'\xff\xfe':
        return raw
    text = re.sub(re.escape(str(directory).replace('\\', '\\\\')), '<staging>', raw.decode('utf-16'), flags=re.I)
    kept, excluded = [], False
    roots = [r'HKEY_CURRENT_USER\Software\Mad Doc Software\EE-AOC\windowed-test '+n
             for n in ('DirectDraw', 'Direct3D', 'DirectInput')]
    for line in text.splitlines():
        if line.startswith('['):
            key = line[1:-1].lower()
            excluded = any(key == root.lower() or key.startswith(root.lower()+'\\') for root in roots)
        if not excluded:
            kept.append(line)
    return '\n'.join(kept).strip().encode('utf-8')


def windowed_verdict(lines, checkpoint):
    milestone = {'graphics': 'graphics ready', 'audio': 'audio ready', 'frame': 'frame presented'}[checkpoint]
    expected = f'STOP <windowed {milestone}> reached'
    stops = [line for line in lines if line.startswith('STOP ')]
    errors = [line for line in lines if line.startswith('HARNESS FAILURE')]
    results = [line for line in lines if line.startswith('graphics result ')]
    bad_results = [line for line in results if int(line.rsplit(': ', 1)[1], 16) != 0]
    ok = (stops == [expected] and not errors and not bad_results
          and 'process gone: yes; windows left: 0' in lines
          and 'checkpoint game window uses nonexclusive capture' in lines
          and 'graphics result CreateDevice: 00000000' in lines
          and 'graphics result ActivateScreen: 00000000' in lines)
    if checkpoint in ('audio', 'frame'):
        ok = ok and 'audio result EEInitializeSounds: true' in lines
    return ok, dict(stops=stops, errors=errors, graphics_results=results, failed_graphics=bad_results,
                    audio_results=[line for line in lines if line.startswith('audio result ')],
                    cleanup='process gone: yes; windows left: 0' in lines)


def experimental_hits(lines, manifest):
    hits = {int(line.rsplit(' ', 1)[1], 16) for line in lines if line.startswith('experimental hit rva ')}
    expected = {row['candidate_rva'] for row in manifest['functions']}
    return bool(hits) and hits <= expected, [dict(original_rva=row['rva'], candidate_rva=row['candidate_rva'],
                                                executed=row['candidate_rva'] in hits)
                                           for row in manifest['functions']]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe", nargs="?", default=str(ROOT / "build" / "exe_roundtrip" / "EE-AOC.exe"))
    ap.add_argument("--modes", default="oep,winmain,run")
    ap.add_argument("--timeout", type=int, default=30000)
    ap.add_argument("--windowed", action="store_true",
                    help="TEST ONLY: build a two-byte native-windowed overlay on staged copies; isolate DirectDraw bookkeeping")
    ap.add_argument('--software-display', action='store_true',
                    help='TEST ONLY: native D3D7 RGB device/system-memory renderer overlay for GPU-less VMs')
    ap.add_argument('--silent-audio', action='store_true', help='TEST ONLY: staged silent mixer; disables music in restored test preferences')
    ap.add_argument('--experimental-manifest', type=pathlib.Path, help='TEST ONLY: validate and trace a generated nonexact EXE; at least one candidate must execute')
    ap.add_argument('--checkpoint', choices=['frame', 'graphics', 'audio'], default='frame',
                    help='Windowed run success milestone; graphics or audio stop before later startup systems')
    ap.add_argument("--reg-set", action="append", default=[],
                    help="NAME=dword:N or NAME=sz:TEXT, written into HKCU\\Software\\Mad Doc Software\\EE-AOC before "
                         "each run (inside the snapshot/restore)")
    a = ap.parse_args()
    experimental = None
    if a.experimental_manifest:
        if not a.windowed:
            ap.error('--experimental-manifest requires --windowed')
        experimental = json.loads(a.experimental_manifest.read_text(encoding='utf-8'))
        if (experimental.get('kind') != 'experimental-near-v1' or not experimental.get('test_only')
            or not 1 <= len(experimental.get('functions', [])) <= 8
            or experimental.get('output_sha256') != hashlib.sha256(pathlib.Path(a.exe).read_bytes()).hexdigest()):
            ap.error('Experimental manifest does not match the selected input image')
    else:
        import pefile
        image = pefile.PE(a.exe, fast_load=True)
        if any(section.Name.rstrip(b'\0') == b'.nxcode' for section in image.sections):
            ap.error('Experimental images require --experimental-manifest so candidate execution is checked')
    if a.software_display and not a.windowed:
        ap.error('--software-display requires --windowed')
    if a.silent_audio and not a.windowed:
        ap.error('--silent-audio requires --windowed')
    if a.silent_audio:
        a.reg_set += ['Music Enabled=dword:0']
    if a.checkpoint != 'frame' and not a.windowed:
        ap.error('--checkpoint requires --windowed')
    if any(mode not in ('oep', 'winmain', 'run') for mode in a.modes.split(',')):
        ap.error('--modes must contain oep, winmain, or run')
    if a.software_display:
        a.reg_set += ['Game Bit Depth=dword:32', 'Texture Bit Depth=dword:32']
    BOOT.mkdir(parents=True, exist_ok=True)
    if REG_PENDING.exists():   # (round 39: under the owner lock, so a live session's pending snapshot is never restored)
        reg_lock_acquire("exe_boot interrupted-run check")
        try:
            if REG_PENDING.exists():
                print("registry: restoring after an interrupted run")
                reg_restore()
        finally:
            reg_lock_release()
    harness = build_harness()
    fail = 0
    for mode in a.modes.split(","):
        # fresh staging per mode (a run writes logs into its directory); game data only for the run mode
        prefix = "windowed" if a.windowed else "exe"
        do = stage(prefix+"_o", ROOT / "orig-bin" / "aoc" / "EE-AOC.unwrapped.exe", data=mode == "run", windowed=a.windowed, software_display=a.software_display, silent_audio=a.silent_audio)
        dr = stage(prefix+"_r", pathlib.Path(a.exe), data=mode == "run", windowed=a.windowed, software_display=a.software_display, silent_audio=a.silent_audio)
        trace = None
        if experimental:
            trace = dr/'experimental.trace'
            trace.write_text(''.join(f'{row["candidate_rva"]:x}\n' for row in experimental['functions']), encoding='ascii')
        dump = {"winmain": ".data.bin", "run": ".run.bmp"}.get(mode)
        lo = run(harness, do, mode, a.timeout, BOOT / f"{prefix}_o{dump}" if dump else None, a.reg_set,
                 BOOT / f"{prefix}_o.{mode}.reg", windowed=a.windowed, checkpoint=a.checkpoint)
        lr = run(harness, dr, mode, a.timeout, BOOT / f"{prefix}_r{dump}" if dump else None, a.reg_set,
                 BOOT / f"{prefix}_r.{mode}.reg", windowed=a.windowed, checkpoint=a.checkpoint, trace_functions=trace)
        if a.windowed and mode == 'run':
            reports = {}
            for name, lines, directory in [('o', lo, do), ('r', lr, dr)]:
                ok, report = windowed_verdict(lines, a.checkpoint)
                if name == 'r' and experimental:
                    hit_ok, report['experimental_execution'] = experimental_hits(lines, experimental)
                    ok = ok and hit_ok
                report['pass'] = ok
                report['image_sha256'] = hashlib.sha256((directory/'EE-AOC.exe').read_bytes()).hexdigest()
                report['overlays'] = [json.loads(path.read_text(encoding='utf-8'))
                                      for path in directory.glob('*.json')]
                reports[name] = report
                (BOOT/f'{prefix}_{name}.{mode}.txt').write_text('\n'.join(lines)+'\n', encoding='utf-8')
            ro, rr = [windowed_registry((BOOT/f'{prefix}_{name}.{mode}.reg').read_bytes(), directory)
                      for name, directory in [('o', do), ('r', dr)]]
            same_registry = ro == rr
            passed = all(report['pass'] for report in reports.values()) and same_registry
            result = json.dumps(dict(test_only=True, checkpoint=a.checkpoint,
                software_display=a.software_display, silent_audio=a.silent_audio, runs=reports, game_registry_equal=same_registry,
                experimental=experimental,
                registry_exclusions=['windowed-test DirectDraw', 'windowed-test Direct3D', 'windowed-test DirectInput'],
                comparison='Required real graphics/audio milestone, HRESULTs, cleanup and game preferences; full traces retained as diagnostics, OS event interleaving is not compared',
                passed=passed), indent=2)+'\n'
            (BOOT/'windowed.result.json').write_text(result, encoding='utf-8')
            (BOOT/f'windowed.{a.checkpoint}.result.json').write_text(result, encoding='utf-8')
            fail |= not passed
            print(f'{"PASS" if passed else "FAIL"} windowed run ({a.checkpoint}); registry equal={same_registry}')
            for name, report in reports.items():
                print(f'  {name}: ' + '; '.join(report['stops'] or ['missing STOP']))
            continue
        bad = [ln for lines in [lo, lr] for ln in lines
               if ln.startswith(('HARNESS FAILURE', 'STOP crash:', 'STOP timeout:', 'STOP harness timeout'))]
        if bad:
            fail = 1
            print(f'FAIL {mode}: ' + '; '.join(bad))
            for name, lines in [('o', lo), ('r', lr)]:
                (BOOT / f'{prefix}_{name}.{mode}.txt').write_text('\n'.join(lines)+'\n', encoding='utf-8')
            continue
        ro, rr = ((BOOT / f"{prefix}_{x}.{mode}.reg").read_bytes() for x in "or")
        if a.windowed:
            ro, rr = windowed_registry(ro, do), windowed_registry(rr, dr)
        if ro != rr:
            fail = 1
            print(f"FAIL {mode}: the registry the two runs left under {REG_KEY} differs "
                  f"(build/boot/exe_o.{mode}.reg vs exe_r.{mode}.reg)")
        elif mode == "run":
            txt = ro.decode("utf-16", errors="replace") if ro[:2] == b"\xff\xfe" else ro.decode(errors="replace")
            n = sum(1 for ln in txt.splitlines() if ln.startswith('"'))
            print(f"  registry written under {REG_KEY}: identical in both runs ({n} values); restored after each run")
        if mode == "winmain":
            lo2 = run(harness, do, mode, a.timeout, BOOT / f"{prefix}_o2.data.bin", windowed=a.windowed)
            verdict = data_verdict(BOOT / f"{prefix}_o.data.bin", BOOT / f"{prefix}_o2.data.bin", BOOT / f"{prefix}_r.data.bin")
            print(f"  .data/.bss at WinMain: {verdict[1]}")
            fail |= not verdict[0]
            # the file-open/DLL-load trace up to WinMain depends on other threads' timing (the original differs from
            # itself); it is compared in the run mode. Here: IAT, exceptions, the STOP line.
            strip = lambda ls: [x for x in ls if not x.startswith((".data", "open ", "load "))]   # noqa: E731
            lo, lr = strip(lo), strip(lr)
            if strip(lo2) != lo:
                print("  note: the two original runs logged differently (thread interleaving)")
        stops = [[x for x in ls if x.startswith("STOP")] for ls in (lo, lr)]
        if mode in ("run", "winmain") and lo != lr and sorted(lo) == sorted(lr) and stops[0] == stops[1]:
            stop = next((ln for ln in lo if ln.startswith("STOP")), "(no stop line)")
            print(f"PASS {mode}: same events, order-insensitive ({len(lo)} lines); {stop}")
            (BOOT / f"{prefix}_o.{mode}.txt").write_text("\n".join(lo) + "\n")
            (BOOT / f"{prefix}_r.{mode}.txt").write_text("\n".join(lr) + "\n")
            continue
        (BOOT / f"{prefix}_o.{mode}.txt").write_text("\n".join(lo) + "\n")
        (BOOT / f"{prefix}_r.{mode}.txt").write_text("\n".join(lr) + "\n")
        stop = next((ln for ln in lo if ln.startswith("STOP")), "(no stop line)")
        summary = [ln for ln in lo if ln.startswith(("IAT:", ".data"))]
        if lo == lr:
            print(f"PASS {mode}: identical ({len(lo)} lines); {stop}" + ("; " + "; ".join(summary) if summary else ""))
        else:
            fail = 1
            print(f"FAIL {mode}: logs differ; original {stop}")
            for ln in list(difflib.unified_diff(lo, lr, "original", "relinked", lineterm="", n=0))[:20]:
                print("   ", ln)
    sys.exit(fail)


if __name__ == "__main__":
    main()
