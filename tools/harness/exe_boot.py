"""The guarded live-game runner the sessions use (tools/harness/exe_boot.c): builds the debugger harness, stages
the build under test (stage) and runs it under exe_boot.exe's guards (run). game_session.py is the entry point.

Settings + window:
  - the game keeps its settings in config.ini in its own directory (platform/PlatConfig.h); the registry is not used.
    Every run writes a fresh config.ini into the staged copy with the run's pins (config_set: "Name=value" or
    "Name=dword:N" in the [Game] section, e.g. Game Bit Depth=32) and copies the file the game leaves behind to
    config_out. exe_boot.c refuses every registry write (a run that writes one STOPs).
  - the run mode stages a copy of the game data (Data, Users, redist, *.cfg; copied once from the install, read
    only, into build/boot/aoc_install_copy; or the unified open data, EE_HARNESS_OPEN_DATA), allows windows and the
    renderer's initialisation, and captures the largest window at the STOP to build/boot/<stage>.run.bmp.
"""
import argparse
import hashlib
import json
import difflib
import os
import re
import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
BOOT = ROOT / "build" / "boot"


HARNESS_SOURCES = ["exe_boot.c", "differential/coverage.h"]


def _harness_stale(exe):
    return not exe.exists() or any(exe.stat().st_mtime < (HERE / n).stat().st_mtime
                                   for n in HARNESS_SOURCES if (HERE / n).exists())


# the debugger harness is native to its target, the x86_64 exe: exe_boot.exe from exe_boot.c with mingw64 gcc (MSYS2
# mingw-w64-x86_64-gcc)
HARNESS_GCC = pathlib.Path(r"C:\msys64\mingw64\bin\gcc.exe")


def build_harness(out_dir=None):
    """Rebuild exe_boot.exe when a source is newer. Parallel sessions (round 39): built under a lock file into a
    temporary name and renamed, so no session ever starts a half-written harness. out_dir: default build/boot (the
    self test builds into its own directory)."""
    import os
    import time
    stem, gcc = "exe_boot", HARNESS_GCC
    boot = pathlib.Path(out_dir) if out_dir else BOOT
    exe = boot / f"{stem}.exe"
    if not _harness_stale(exe):
        return exe
    if not gcc.exists():
        raise SystemExit(f"{gcc} is missing: C:\\msys64\\usr\\bin\\bash -lc \"pacman -S --noconfirm "
                         f"mingw-w64-x86_64-gcc\"")
    boot.mkdir(parents=True, exist_ok=True)
    lock = boot / f"{stem}.build.lock"
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
            tmp = boot / f"{stem}.{os.getpid()}.tmp.exe"
            subprocess.run([str(gcc), "-O1", "-Wall", "-Wno-format-truncation", "-static", "-o", str(tmp),
                            str(HERE / "exe_boot.c"), "-lgdi32", "-ldxguid", "-ldbghelp"], check=True,
                           env={"PATH": str(gcc.parent), "SYSTEMROOT": r"C:\Windows", "TEMP": str(boot)})
            for old in boot.glob(f"{stem}.old.*.exe"):   # earlier images no session runs any more
                try:
                    old.unlink()
                except OSError:
                    pass
            try:
                os.replace(tmp, exe)
            except PermissionError:   # a running session executes exe_boot.exe: Windows still allows a rename
                exe.rename(boot / f"{stem}.old.{time.time_ns()}.exe")
                os.replace(tmp, exe)
    finally:
        lock.unlink(missing_ok=True)
    return exe


INSTALL = pathlib.Path(r"C:\Program Files (x86)\Empire Earth\Empire Earth - The Art of Conquest")
# game data the run mode needs, copied (read-only access to the install) once into a cache under build/boot and
# from there into every staging directory; the community DDraw.dll/dreXmod/discord files are not copied
GAME_FILES = ["Data", "Users", "redist", "WONLobby.cfg", "zzWONVersion.cfg"]
CACHE = BOOT / "aoc_install_copy"
CONFIG_SECTION = "Game"   # the section of config.ini the run pins go to (EEPreferences.cpp kEEGameSection)


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


OPEN_DATA = ROOT / "build" / "open_data"


def data_tree(kind):
    """The unified data tree the runs stage (tools/open_data.py: both installs' data.ssa and loose data in one Data
    folder; the game needs neither an archive nor the original game). kind "open": db text, glTF, PNG
    (build/open_data/tree, or the tree EE_HARNESS_OPEN_DATA names when that is a directory); "binary": the same files
    unconverted (build/open_data/tree-binary). Built once when missing."""
    env = os.environ.get("EE_HARNESS_OPEN_DATA")
    if kind == "open" and env and env not in ("1", "yes") and pathlib.Path(env).is_dir():
        tree = pathlib.Path(env)
    else:
        tree = OPEN_DATA / ("tree" if kind == "open" else "tree-binary")
    if not (tree / "open_data.json").exists():
        print(f"data: building {tree} (tools/open_data.py build)", flush=True)
        cmd = [sys.executable, "-I", str(ROOT / "tools" / "open_data.py"), "build", str(tree), "--force"]
        if kind == "binary":
            cmd.append("--binary")
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError(f"open_data build failed:\n{r.stdout}{r.stderr}")
    return tree


WRITABLE = {"saved games", "scenarios"}   # Data folders the game writes into: copied, not linked


def stage_tree(tree, d):
    """Stages a data tree into d: Data's read-only files as hard links (fast; copies when linking fails), the folders
    the game writes into, Users and the root files as copies."""
    for item in tree.iterdir():
        if item.name == "open_data.json":
            continue
        if item.is_file():
            shutil.copy(item, d / item.name)
        elif item.name.lower() != "data":
            shutil.copytree(item, d / item.name)
    for src in (tree / "Data").rglob("*"):
        rel = src.relative_to(tree / "Data")
        dst = d / "Data" / rel
        if src.is_dir():
            dst.mkdir(parents=True, exist_ok=True)
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        if rel.parts[0].lower() in WRITABLE or len(rel.parts) == 1:
            shutil.copy(src, dst)
            continue
        try:
            os.link(src, dst)
        except OSError:
            shutil.copy(src, dst)


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


def imports_dll(path, dll):
    import pefile
    pe = pefile.PE(str(path), fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    return any(e.dll.decode().lower() == dll.lower() for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []))


def stage(name, build, data=True, silent_audio=True):
    """Stage the build under test (hbuild.Build) in build/boot/<name>: its exe (the test profile compiles the
    windowed/software policy in, nothing is patched), the third-party DLLs, the RDP mode-list proxy when the display
    needs it, and a copy of the game data. silent_audio: kept for the callers (the game's own mixer falls back to a
    null audio device on a machine without one)."""
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
    if rdp_modes.needed() and not imports_dll(d / "EE-AOC.exe", "DDRAW.dll"):
        print("display: RDP session, but the exe does not import DDRAW.dll (GX renderer): no mode-list proxy", flush=True)
    elif rdp_modes.needed():
        rename_import(d / "EE-AOC.exe", "DDRAW.dll", "DDRDP.dll")   # the renderer is inside the exe
        rdp_modes.build_proxy(d)
        print('display: RDP mode-list proxy staged (DDRDP.dll)', flush=True)
    if data:
        text_db, open_assets = os.environ.get("EE_HARNESS_TEXT_DB"), os.environ.get("EE_HARNESS_OPEN_ASSETS")
        kind = "open" if os.environ.get("EE_HARNESS_OPEN_DATA") or (text_db and open_assets) else "binary"
        c = data_tree(kind)
        stage_tree(c, d)
        print(f"data: the unified {kind} data tree {c} ({json.loads((c / 'open_data.json').read_text())['files']} files)",
              flush=True)
        if kind == "open":
            text_db = open_assets = None   # already in the tree
        if text_db:
            # TEST: the db tables as text (tools/eedata, clean mode) in the staged copy's Data\db; the game prefers
            # them over data.ssa
            r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "eedata" / "eedata.py"), "install", str(d)],
                               capture_output=True, text=True)
            if r.returncode:
                raise RuntimeError(f"eedata install failed:\n{r.stdout}{r.stderr}")
            print(f"data: db text tables installed ({len(list((d / 'Data' / 'db').glob('*.*')))} files)", flush=True)
        if open_assets:
            # TEST: every model as glTF and every texture as PNG (tools/eemodel) in the staged copy's Data\Models and
            # Data\Textures; the game prefers them over the .cem/.sst/.tga
            r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "eemodel" / "eemodel.py"), "install", str(d)],
                               capture_output=True, text=True)
            if r.returncode:
                raise RuntimeError(f"eemodel install failed:\n{r.stdout}{r.stderr}")
            print(f"data: open models/textures installed ({r.stdout.strip().splitlines()[-1] if r.stdout.strip() else ''})",
                  flush=True)
    return d


def config_text(config_set):
    """config.ini holding the run's pins: "Name=value" or "Name=dword:N" items (the old registry pin syntax)."""
    lines = [f"[{CONFIG_SECTION}]"]
    for item in config_set:
        name, value = item.split("=", 1)
        if value.startswith(("dword:", "sz:")):
            value = value.split(":", 1)[1]
        lines.append(f"{name.strip()} = {value}")
    return "\n".join(lines) + "\n"


def batch_begin(instances=()):
    """tools/harness/game_batch.py: nothing to snapshot (every staged copy has its own config.ini)."""
    return dict(lease=None, pruned=[], snapshot=None, existed=False)


def batch_end(begin):
    return True


def shared_recover():
    return []


def run(harness, d, mode, timeout, dump=None, config_set=(), config_out=None, windowed=False, checkpoint='frame', trace_functions=None, live_log=None, loopback=False, harness_extra=(),
        registry='self', reg_subtree=None, on_start=None):
    """config_set: the run's settings pins, written to a fresh config.ini in the staged copy d (config_text);
    config_out: where the config.ini the game leaves behind is copied. registry and reg_subtree are accepted for
    the callers and ignored (the game uses no registry). on_start(): called right before the harness starts."""
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
    (d / "config.ini").write_bytes(config_text(config_set).encode("cp1252"))
    (d / "config.ini.new").unlink(missing_ok=True)
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
    if config_out:
        if (d / "config.ini").exists():
            shutil.copy(d / "config.ini", config_out)
        else:
            pathlib.Path(config_out).write_text("(no config.ini)\n")
    if not any(ln.startswith('STOP ') for ln in out.splitlines()) or 'process gone: yes; windows left: 0' not in out:
        out += '\nHARNESS FAILURE: missing stop/clean process termination\n'
    if not live_log:
        (BOOT/f'{d.name}.{mode}.raw.txt').write_text(out,encoding='utf-8')
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


def windowed_config(raw, directory):
    """A run's config.ini for comparison (the staging directory's name masked)."""
    text = raw.decode('cp1252', errors='replace')
    return re.sub(re.escape(str(directory)), '<staging>', text, flags=re.I).strip().encode('utf-8')


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
