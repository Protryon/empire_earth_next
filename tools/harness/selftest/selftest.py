"""TEST ONLY: the harness's self test without the game. Checks that exe_boot64.exe (the x64 debugger harness) allows,
refuses and stops every guarded API exactly as exe_boot.exe (i686) does, and that the time shim and the presentation
bridge load and patch an x64 exe as they do an i686 one.

  python -I tools/harness/selftest/selftest.py [--only CASE,..] [--legacy REV|none] [-v]

guard_test.c (built for both targets) calls the guarded APIs, one case per run (EE_SELFTEST_CASE), under each harness
in build/boot/selftest_stage. For every case the harness log must hold the case's expected decisions and STOP, and
the decision lines of the i686 and x64 runs, with addresses and system-DLL noise masked, must be identical. --legacy
REV (default main) also runs the i686 harness of that revision (git show) on the cases it supports: the port must
not change a 32-bit decision. shim_test.c checks the time shim (IAT patch of QPC/Sleep/waits, the renamed WINMM
import, timeSetEvent) and bridge_test.c the presentation bridge (ee_sym by RVA, the GX present hook and frame dump,
the UI fixture's form state and a click through thiscall/x64 member calls) on both targets.

Nothing outside build/boot/selftest*, HKCU\\Software\\EE-harness-selftest* and %LOCALAPPDATA%\\D3DSCache (refused) is
touched: every run passes --reg-root Software\\EE-harness-selftest (the legacy harness, which has no --reg-root, only
runs cases that create no key), so the game's HKCU\\Software\\Mad Doc Software is never written.
"""
import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time
import winreg

HERE = pathlib.Path(__file__).resolve().parent
HARNESS = HERE.parent
ROOT = HARNESS.parent.parent
sys.path.insert(0, str(HARNESS))
import exe_boot as boot      # noqa: E402

BOOT = ROOT / "build" / "boot"
SELF = BOOT / "selftest"
STAGE = BOOT / "selftest_stage"
GCC = boot.HARNESS_ARCH
REG_ROOT = r"Software\EE-harness-selftest"
REG_KEYS = [REG_ROOT, REG_ROOT + "-outside", REG_ROOT + "-victim"]
ARCHES = ("x86", "x64")


def gcc(arch):
    return GCC[arch][1]


def compile_c(arch, out, sources, libs=(), extra=()):
    g = gcc(arch)
    cmd = [str(g), "-O1", "-Wall", "-Wno-format-truncation", *extra, "-o", str(out), *map(str, sources), *libs]
    subprocess.run(cmd, check=True, env=dict(os.environ, PATH=str(g.parent) + os.pathsep + os.environ["PATH"]))
    return out


def symbols(arch, exe):
    """{name: RVA} of the exe's defined symbols (mingw nm) and its preferred image base."""
    import pefile
    base = pefile.PE(str(exe), fast_load=True).OPTIONAL_HEADER.ImageBase
    nm = gcc(arch).parent / "nm.exe"
    out = subprocess.run([str(nm), str(exe)], capture_output=True, text=True, check=True,
                         env=dict(os.environ, PATH=str(nm.parent) + os.pathsep + os.environ["PATH"])).stdout
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3:
            name = parts[2][1:] if arch == "x86" and parts[2].startswith("_") else parts[2]
            syms[name] = int(parts[0], 16) - base
    return syms, base


# ---- registry: the self test's own keys only ----
def reg_delete_tree(path):
    if not path.startswith(REG_ROOT):
        raise ValueError(path)
    for sub in reg_subkeys(path) or []:
        reg_delete_tree(path + "\\" + sub)
    try:
        winreg.DeleteKey(winreg.HKEY_CURRENT_USER, path)
    except FileNotFoundError:
        pass


def reg_reset():
    for k in REG_KEYS:
        reg_delete_tree(k)
    winreg.CreateKey(winreg.HKEY_CURRENT_USER, REG_ROOT + r"\EE-AOC").Close()   # the game key exists (as the game's does)
    with winreg.CreateKey(winreg.HKEY_CURRENT_USER, REG_ROOT + "-victim") as v:
        winreg.SetValueEx(v, "Keep", 0, winreg.REG_DWORD, 1)


def reg_values(path):
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path) as k:
            out, i = {}, 0
            while True:
                try:
                    n, v, _ = winreg.EnumValue(k, i)
                except OSError:
                    return out
                out[n] = v
                i += 1
    except FileNotFoundError:
        return None


def reg_subkeys(path):
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path) as k:
            out, i = [], 0
            while True:
                try:
                    out.append(winreg.EnumKey(k, i))
                except OSError:
                    return sorted(out)
                i += 1
    except FileNotFoundError:
        return None


# ---- the cases ----
def P(ctx):
    return 8 if ctx["arch"] == "x64" else 4


def static_args(ctx):
    s = ctx["syms"]
    return ["--static-exe"] + [f"{s[n]:x}" for n in ("selftest_native_error", "selftest_abort", "selftest_generate",
                                                       "selftest_next")]


def windowed_args(ctx):
    s = ctx["syms"]
    return ["--windowed", "--audio-entry", f"{s['selftest_sounds']:x}"]


def live_args(ctx):
    s = ctx["syms"]
    trace = STAGE / "trace.txt"
    trace.write_text(f"{s['selftest_traced']:x}\n", encoding="ascii")
    cov = STAGE / "coverage.txt"
    cov.write_text(f"{s['selftest_covered']:x} covered\n", encoding="ascii")
    return (windowed_args(ctx) + ["--live-session", "--renderer-guards", f"{s['selftest_activate']:x}",
            f"{s['selftest_show']:x}"] + static_args(ctx)
            + ["--trace-functions", str(trace), "--coverage", str(cov), str(STAGE / "coverage.tsv")])


def rng_args(ctx):
    (STAGE / "rng.arm").write_text("arm\n", encoding="ascii")
    s = ctx["syms"]
    a = ["--live-session", "--rng-trace", f"{ctx['base'] + s['selftest_physics']:x}"] + static_args(ctx)
    if not ctx["legacy"]:
        a += ["--image-base", f"{ctx['base']:x}", "--rng-offset", f"{6 * P(ctx):x}"]
    return a


def nhd_args(ctx):
    s, p = ctx["syms"], P(ctx)
    a = ["--no-human-defeat", f"{s['selftest_out']:x}", f"{s['selftest_result']:x}",
         f"{ctx['base'] + s['selftest_world']:x}", f"{2 * p:x}", f"{2 * p + 17 * p:x}"]
    return a if ctx["legacy"] else a + ["--image-base", f"{ctx['base']:x}"]


def victim_kept():
    return (BOOT / "selftest_victim.txt").exists()


def victim_reg(name, present):
    return lambda: (reg_values(REG_ROOT + "-victim") or {}).get(name) is not None if present else \
        name not in (reg_values(REG_ROOT + "-victim") or {})


EXIT0 = r"^STOP exit: process exited with 0$"
CASES = [
    # name, harness options (ctx -> list), expected lines (regexes, in order), final STOP, post-check, legacy ok
    ("file_read", None, [r"^open NtCreateFile access \w+ disp 1: \\\?\?\\C:\\Windows\\win\.ini ",
                         r"^debugstring SELFTEST file_read opened"], EXIT0, None, True),
    ("file_write_in", None, [r"^write NtCreateFile access \w+ disp 5: .*selftest_stage\\inside\.txt ",
                             r"^debugstring SELFTEST file_write_in create 1 write 1 moved 1 deleted 1"], EXIT0,
     lambda: not (STAGE / "inside.txt").exists() and not (STAGE / "inside2.txt").exists(), True),
    ("file_write_out", None, [], r"^STOP NtCreateFile access \w+ disp 5: .*selftest_outside\.txt ",
     lambda: not (BOOT / "selftest_outside.txt").exists(), True),
    ("file_delete_on_close", None, [], r"^STOP NtCreateFile access \w+ disp 1: .*selftest_victim\.txt ", victim_kept, True),
    ("ntopen_write_out", None, [], r"^STOP NtOpenFile access \w+ disp 1: .*selftest_victim\.txt ", victim_kept, True),
    ("nt_delete_file", None, [], r"^STOP NtDeleteFile ", victim_kept, True),
    ("shader_cache", None, [r"^refused NtCreateFile .*\\D3DSCache\\ee_selftest\\cache\.bin \(D3D shader cache, from .*STATUS_ACCESS_DENIED$",
                            r"^debugstring SELFTEST shader_cache handle invalid error 5$"], EXIT0, None, True),
    ("reg_write_in", None, [r"^regcreate \\REGISTRY\\USER\\[^\\]+\\Software\\EE-harness-selftest\\EE-AOC\\selftest ",
                            r"^regset .*\\EE-AOC\\selftest \[Value\] type 4 size 4 = 42$",
                            r"^regwrite NtDeleteValueKey .*\\EE-AOC\\selftest \[Value\]",
                            r"^debugstring SELFTEST reg_write_in create 0 set 0 delete 0 set 0$"], EXIT0,
     lambda: reg_values(REG_ROOT + r"\EE-AOC\selftest") == {"Kept": 42}, False),
    ("reg_write_out", None, [], r"^STOP NtCreateKey \\REGISTRY\\USER\\[^\\]+\\Software\\EE-harness-selftest-outside(\\x)? access",
     lambda: reg_values(REG_ROOT + "-outside") is None, True),
    ("reg_open_ro", None, [r"^regopen \\REGISTRY\\USER\\[^\\]+\\Software\\Microsoft \(from",
                           r"^debugstring SELFTEST reg_open_ro 0$"], EXIT0, None, True),
    ("reg_set_out", None, [r"^debugstring SELFTEST victim open 0$"],
     r"^STOP NtSetValueKey \\REGISTRY\\USER\\[^\\]+\\Software\\EE-harness-selftest-victim \[Injected\]",
     victim_reg("Injected", False), True),
    ("reg_delete_out", None, [], r"^STOP NtDeleteValueKey .*\\EE-harness-selftest-victim \[Keep\]", victim_reg("Keep", True), True),
    ("reg_save", None, [r"^write NtCreateFile access \w+ disp 2: .*selftest_stage\\saved\.hiv "], r"^STOP NtSaveKey ",
     lambda: not (STAGE / "saved.hiv").exists() or (STAGE / "saved.hiv").stat().st_size == 0, True),
    ("reg_subtree", lambda c: ["--registry-subtree", "windowed-test selftest"],
     [r"^regredirect-instance create .*\\EE-harness-selftest\\EE-AOC -> HKCU\\Software\\EE-harness-selftest\\EE-AOC\\windowed-test selftest$",
      r"^regset .*\\EE-AOC\\windowed-test selftest \[Instance\] type 4 size 4 = 7$",
      r"^regredirect-instance open .*\\EE-AOC -> HKCU\\Software\\EE-harness-selftest\\EE-AOC\\windowed-test selftest$",
      r"^regredirect-instance open .*\\EE-AOC\\absent -> .*\\windowed-test selftest\\absent \(absent\)$",
      r"^debugstring SELFTEST reg_subtree create 0 set 0 open 0 query 0 value 7 absent 2$"], EXIT0,
     lambda: reg_values(REG_ROOT + r"\EE-AOC\windowed-test selftest") == {"Instance": 7}
     and "Instance" not in (reg_values(REG_ROOT + r"\EE-AOC") or {}), False),
    ("ddraw_key_redirect", windowed_args,
     [r"^regredirect \\REGISTRY\\MACHINE\\Software\\Microsoft\\DirectDraw\\MostRecentApplication -> snapshot-restored",
      r"^regset .*\\EE-AOC\\windowed-test DirectDraw\\MostRecentApplication \[ID\] type 4 size 4 = 5$",
      r"^debugstring SELFTEST ddraw_key_redirect create 0 set 0$"], EXIT0,
     lambda: reg_values(REG_ROOT + r"\EE-AOC\windowed-test DirectDraw\MostRecentApplication") == {"ID": 5}, False),
    ("window", None, [r'^window CreateWindow class "[^"]*" name "selftest" style ',
                      r'^window CreateWindow class "[^"]*" name "selftest message" style \w+ parent fffffffd ',
                      r"^debugstring SELFTEST window 1 message 1$"], EXIT0, None, True),
    ("msgbox", None, [], r'^STOP MessageBoxA: "selftest box" \[selftest\]', None, True),
    ("create_process", None, [], r"^STOP NtCreateUserProcess ", None, True),
    ("display", None, [], r"^STOP ChangeDisplaySettingsA ", None, True),
    ("gamma", None, [], r"^STOP SetDeviceGammaRamp ", None, True),
    ("spi_get", None, [r"^spi  SystemParametersInfo action 0x30 ", r"^debugstring SELFTEST spi_get 1$"], EXIT0, None, True),
    ("spi_set", None, [], r"^STOP SystemParametersInfo action 0x2 ", None, True),
    ("net_stop", None, [], r"^STOP NtCreateFile access \w+ disp 3: \\Device\\Afd\\Endpoint ", None, True),
    ("net_loopback", lambda c: ["--loopback-only"],
     [r"^loopback socket family 2 type 1$", r"^loopback AFD endpoint within guarded socket creation$",
      r"^loopback bind address 00000000 -> 127\.0\.0\.1$", r"^loopback bind result 00000000$",
      r"^loopback listen on verified loopback bind$", r"^loopback connect address 08080808 -> 127\.0\.0\.1$",
      r"^loopback WSAIoctl 8004667e$", r"^loopback sendto address 04040808 -> 127\.0\.0\.1$",
      r"^debugstring SELFTEST net_loopback bind 0 127\.0\.0\.1 listen 0 connect 0 ioctl 0 sendto 1$"], EXIT0, None, True),
    ("net_raw", lambda c: ["--loopback-only"], [], r"^STOP unsupported socket creation$", None, True),
    ("net_ioctl_bad", lambda c: ["--loopback-only"], [], r"^STOP WSAIoctl c8000006$", None, True),
    ("net_port_offset", lambda c: ["--loopback-only", "--port-offset", "16"],
     [r"^TEST instance port 33334 -> 33350$", r"^debugstring SELFTEST net_port_offset bind 0 port 33350$"], EXIT0, None, True),
    ("mp_loopback", lambda c: ["--loopback-only", "--mp-loopback", "2", "3"],
     [r"^TEST mp loopback self 0200007f peer 0300007f$", r"^TEST mp bind address 00000000 -> 0200007f$",
      r"^TEST mp sendto address 04030201 -> 0300007f$",
      r"^TEST mp refused NtCreateFile \\Device\\RasAcd \(gethostbyname autodial helper, from .*STATUS_ACCESS_DENIED$",
      r"^debugstring SELFTEST mp_loopback bind 0 127\.0\.0\.2 sendto 1 rasacd c0000022$"], EXIT0, None, True),
    ("seed_pin", lambda c: ["--seed-pin", f"{c['syms']['selftest_execute']:x}", f"{5 * P(c):x}", "4242"],
     [r"^TEST seed pin armed rva \w+ \[ecx\+(14|28)\] := 4242$", r"^TEST seed pin write at \w+ \(this \w+\) old 1 new 4242$",
      r"^debugstring SELFTEST seed 4242$", r"^TEST seed pin write at \w+ \(this \w+\) old 4242 new 4242$",
      r"^debugstring SELFTEST seed 4242$"], EXIT0, None, True),
    ("nhd", nhd_args,
     [r"^TEST no-human-defeat armed ",
      r"^TEST no-human-defeat skip out Fn1391F2 player 1 \(victorious 0; local 1\)",
      r"^TEST no-human-defeat pass out Fn1391F2 player 2 \(victorious 0; local 1\)",
      r"^TEST no-human-defeat pass out Fn1391F2 player 1 \(victorious 1; local 1\)",
      r"^TEST no-human-defeat skip result Fn16AF5C player 1 \(state 2; local 1\)",
      r"^TEST no-human-defeat pass result Fn16AF5C player 1 \(state 1; local 1\)",
      r"^TEST no-human-defeat pass result Fn16AF5C player 2 \(state 2; local 1\)",
      r"^debugstring SELFTEST nhd out calls 2 result calls 2 local state 1$"], EXIT0, None, True),
    ("mutex_bypass", lambda c: ["--mutex-bypass", f"{c['syms']['selftest_mutex_at']:x}"],
     [r"^TEST mutex bypass armed rva ", r"^TEST mutex bypass at rva \w+: GetLastError 183 \(ERROR_ALREADY_EXISTS\) -> 0, instance proceeds$",
      r"^TEST mutex bypass at rva \w+: GetLastError 5 \(unexpected; left as is\)$",
      r"^debugstring SELFTEST mutex 183 -> 0, 5 -> 5$"], EXIT0, None, True),
    ("mp_skip", lambda c: ["--mp-skip", f"{c['syms']['selftest_skip']:x}"],
     [r"^TEST mp skip at rva ", r"^debugstring SELFTEST mp_skip calls 0$"], EXIT0, None, True),
    ("mp_checksum", lambda c: ["--mp-checksum", f"{c['syms']['selftest_checksum_at']:x}", "10"],
     [r"^TEST mp checksum at rva \w+: gBinaryChecksum 000003e8 -> 000003f8 ", r"^debugstring SELFTEST checksum 1016$"],
     EXIT0, None, True),
    ("rng_trace", rng_args,
     [r"^TEST rng trace resolved ", r"^TEST rng trace armed$", r"^rng 77 ret ", r"^rng \d+ ret ", r"^rng \d+ ret ",
      r"^debugstring SELFTEST rng seed "], EXIT0, None, True),
    ("static_exe", live_args,
     [r"^TEST coverage armed 1 of 1 entries", r"^checkpoint game window uses nonexclusive capture$",
      r"^audio result EEInitializeSounds: true$", r"^graphics result ActivateScreen: 00000000$",
      r"^native error 00001234 count 1 ", r"^native error 00001234 count 2 ", r"^native error 00000099 count 1 ",
      r"^graphics result ShowScene: 00000000$", r"^checkpoint windowed frame presented$",
      r"^experimental hit rva \w+$"], r"^STOP <windowed session complete> reached$",
     lambda: re.search(r"^covered\t\w+\thit\t", (STAGE / "coverage.tsv").read_text(), re.M) is not None, True),
    ("abort", live_args, [r"^checkpoint windowed frame presented$", r"^  abort stack\+00 "], r"^STOP abort ", None, True),
    ("ddraw", lambda c: windowed_args(c),
     [r"^debugstring SELFTEST ddraw create 00000000$", r"^ddcoop IDirectDraw::SetCooperativeLevel flags 00000008 ",
      r"^graphics result SetCooperativeLevel: 00000000$", r"^debugstring SELFTEST ddraw normal 00000000$"],
     r"^STOP IDirectDraw::SetCooperativeLevel flags 00000011 ", None, False),
    ("crash", None, [r"^exception c0000005 at EXE\+\w+ \(first chance\)$"],
     r"^STOP crash: exception c0000005 at EXE\+\w+ \(second chance\)$", None, True),
    ("mode_oep", None, [r"^iat ", r"^IAT: \d+ slots, fnv \w+$"], r"^STOP <entry point> reached$", None, True),
    ("mode_winmain", lambda c: ["--winmain-rva", f"{c['syms']['selftest_main_stand_in']:x}"],
     [r"^\.data\+\.bss at WinMain: \d+ bytes, fnv \w+"], r"^STOP <WinMain> reached$", None, True),
]


# ---- log comparison ----
NOISE = re.compile(r"^(load |harness x64$|image base |TEST registry policy root |TEST native-windowed|screenshot |"
                   r"window-at-end |process gone|   |  stack |diagnostic |deferred concurrent|step-over stalled|"
                   r"ddraw: |exception 406d1388|exception 000006ba|exception 80000003|exception 4000001f)")


def decisions(lines):
    """The lines that carry a policy decision, with what differs by bitness masked: addresses, module offsets, the
    caller, system DLL opens and registry opens the loader makes (WOW64 vs native paths)."""
    out = []
    for ln in lines:
        if NOISE.match(ln):
            continue
        if ln.startswith(("open ", "regopen ")) and "selftest" not in ln.lower() and "win.ini" not in ln.lower() \
                and "\\software\\microsoft (" not in ln.lower():
            continue
        if ln.startswith("spi ") and "action 0x30 " not in ln:
            continue
        if ln.startswith("exception ") and "c0000005" not in ln:
            continue
        if ln.startswith("debugstring ") and not ln.startswith("debugstring SELFTEST "):
            continue   # the system DLLs' own debug output (addresses, thread ids)
        ln = re.sub(r"\bfrom [\w.-]+\+[0-9a-f]+", "from <caller>", ln)
        ln = re.sub(r"\(from [^)]*\)", "(from <caller>)", ln)
        ln = re.sub(r"^loopback socket result [0-9a-f]+$", "loopback socket result <handle>", ln)
        ln = re.sub(r"\btid \d+", "tid <tid>", ln)
        ln = re.sub(r"\bEXE\+[0-9a-f]+", "EXE+<off>", ln)
        ln = re.sub(r"\b[0-9a-f]{16}\b", "<addr>", ln)
        ln = re.sub(r"\b(at|this|ret|up|arg|rva|world) [0-9a-f]{8}\b", r"\1 <addr>", ln)
        ln = re.sub(r"\[(e|r)cx\+[0-9a-f]+\]", "[this+<off>]", ln)
        ln = re.sub(r"players \+[0-9a-f]+ current \+[0-9a-f]+", "players <off> current <off>", ln)
        ln = re.sub(r"\\REGISTRY\\USER\\S-[0-9-]+", r"\\REGISTRY\\USER\\<sid>", ln)
        ln = re.sub(r"access [0-9a-f]{8}", "access <mask>", ln) if ln.startswith(("open ", "regopen ", "regcreate ")) else ln
        ln = re.sub(r"IAT: \d+ slots, fnv \w+", "IAT: <n> slots", ln)
        ln = re.sub(r"\.data\+\.bss at WinMain: \d+ bytes, fnv \w+", ".data+.bss at WinMain: <n>", ln)
        ln = re.sub(r"^rng (\d+) ret .*", r"rng \1", ln)
        ln = re.sub(r"^TEST rng trace resolved .*", "TEST rng trace resolved", ln)
        ln = re.sub(r"(native error \w+ count \d+) .*", r"\1", ln)
        ln = re.sub(r"(TEST coverage armed \d+ of \d+ entries).*", r"\1", ln)
        ln = re.sub(r"(MostRecentApplication \[\w+\] type \d+ size \d+) = \d+$", r"\1 = <value>", ln)   # DirectDraw's ids
        if ln.startswith("iat ") or (ln.startswith("  abort stack+") and not ln.startswith("  abort stack+00 ")):
            continue   # raw stack slots: a diagnostic of each bitness's own frame
        out.append(ln)
    return out


def check_case(lines, expect, stop):
    """Problems of one run against its expectations (in-order regex matches, the single STOP)."""
    problems, at = [], 0
    for rx in expect:
        r = re.compile(rx)
        while at < len(lines) and not r.search(lines[at]):
            at += 1
        if at == len(lines):
            problems.append(f"missing {rx}")
            at = 0
        else:
            at += 1
    stops = [ln for ln in lines if ln.startswith("STOP ")]
    if len(stops) != 1 or not re.search(stop, stops[0]):
        problems.append(f"stop {stops} != {stop}")
    if "process gone: yes; windows left: 0" not in lines:
        problems.append("the target did not end cleanly")
    if any("NOT STOPPED" in ln for ln in lines):
        problems.append("a guarded call ran")
    return problems


def run_case(harness, exe, case, ctx, timeout_ms=30000):
    name, opts, expect, stop, post, _ = case
    if STAGE.exists():
        if STAGE.resolve().parent != BOOT.resolve():
            raise ValueError("stage must be inside build/boot")
        shutil.rmtree(STAGE)
    STAGE.mkdir(parents=True)
    shutil.copy(exe, STAGE / "guard_test.exe")
    for f in ("selftest_outside.txt",):
        (BOOT / f).unlink(missing_ok=True)
    (BOOT / "selftest_victim.txt").write_text("victim\n", encoding="ascii")
    reg_reset()
    mode = {"mode_oep": "oep", "mode_winmain": "winmain"}.get(name, "run")
    args = (opts(ctx) if opts else []) + ([] if ctx["legacy"] else ["--reg-root", REG_ROOT])
    cmd = [str(harness), str(STAGE), "guard_test.exe", mode, str(timeout_ms), str(STAGE / "shot.bmp"), *args]
    env = dict(os.environ, EE_SELFTEST_CASE=name)
    t0 = time.monotonic()
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout_ms / 1000 + 30, env=env)
    lines = r.stdout.splitlines()
    problems = check_case(lines, expect, stop)
    if r.returncode not in (0,):
        problems.append(f"harness exit {r.returncode}: {r.stderr.strip()[:300]}")
    if post and not post():
        problems.append("post-check failed (a persistent change, or the effect is missing)")
    reg_reset()
    return lines, problems, time.monotonic() - t0


def build_all(arches, legacy):
    SELF.mkdir(parents=True, exist_ok=True)
    out = {}
    for arch in arches:
        d = SELF / arch
        d.mkdir(exist_ok=True)
        harness = boot.build_harness(arch, d)
        exe = compile_c(arch, d / "guard_test.exe", [HERE / "guard_test.c"], ["-lws2_32", "-ldxguid", "-lgdi32", "-mwindows"],   # GUI, as the game: no console
                        ["-Wl,--disable-dynamicbase"] if arch == "x86" else [])   # x86 fixed like the game; x64 ASLR
        syms, base = symbols(arch, exe)
        out[arch] = dict(harness=harness, exe=exe, syms=syms, base=base)
    if legacy:
        d = SELF / "legacy"
        src = d / "src"
        (src / "differential").mkdir(parents=True, exist_ok=True)
        for rel in ("exe_boot.c", "differential/ptsync.h", "differential/ptwatch.h", "differential/coverage.h"):
            data = subprocess.run(["git", "-C", str(ROOT), "show", f"{legacy}:tools/harness/{rel}"], capture_output=True,
                                  check=True).stdout
            (src / rel).write_bytes(data)
        g = gcc("x86")
        exe = d / "exe_boot_legacy.exe"
        subprocess.run([str(g), "-O1", "-Wall", "-Wno-format-truncation", "-static", "-o", str(exe), str(src / "exe_boot.c"),
                        "-lgdi32", "-ldxguid", "-ldbghelp"], check=True,
                       env={"PATH": str(g.parent), "SYSTEMROOT": r"C:\Windows", "TEMP": str(d)})
        out["legacy"] = dict(out["x86"], harness=exe)
    return out


def guard_suite(builds, only, verbose, legacy):
    results, failures = {}, 0
    runs = [a for a in ARCHES if a in builds] + (["legacy"] if legacy else [])
    for case in CASES:
        name = case[0]
        if only and name not in only:
            continue
        per = {}
        for run in runs:
            if run == "legacy" and not case[5]:
                continue
            b = builds[run]
            ctx = dict(arch="x86" if run == "legacy" else run, syms=b["syms"], base=b["base"], legacy=run == "legacy")
            lines, problems, secs = run_case(b["harness"], b["exe"], case, ctx)
            per[run] = (decisions(lines), problems, lines)
            if verbose or problems:
                print(f"--- {name} [{run}] {secs:.1f} s" + ("" if not problems else f": {problems}"))
                if verbose or problems:
                    print("\n".join("    " + ln for ln in lines if not ln.startswith("load ")))
        same = {}
        base = per.get("x86")
        for run, (d, _, _) in per.items():
            if run != "x86" and base is not None:
                same[run] = d == base[0]
                if not same[run]:
                    print(f"--- {name}: {run} decisions differ from x86:")
                    import difflib
                    print("\n".join("    " + x for x in difflib.unified_diff(base[0], d, "x86", run, lineterm="", n=1)))
        ok = all(not p for _, p, _ in per.values()) and all(same.values())
        failures += not ok
        results[name] = dict(ok=ok, runs={r: not p for r, (_, p, _) in per.items()}, same=same)
        cells = " ".join(f"{r}:{'ok' if not p else 'FAIL'}" for r, (_, p, _) in per.items())
        eq = " ".join(f"{r}=x86" if v else f"{r}!=x86" for r, v in same.items())
        print(f"{'PASS' if ok else 'FAIL'} {name:22} {cells}  {eq}", flush=True)
    return results, failures


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--only", help="comma-separated case names (guard cases, 'shim', 'bridge')")
    ap.add_argument("--legacy", default="main", help="also run this revision's i686 harness (git show; 'none': skip)")
    ap.add_argument("-v", action="store_true")
    a = ap.parse_args()
    only = set(a.only.split(",")) if a.only else None
    legacy = None if a.legacy == "none" else a.legacy
    madoc_before = reg_subkeys(r"Software\Mad Doc Software\EE-AOC")
    builds = build_all(ARCHES, legacy)
    results, failures = guard_suite(builds, only, a.v, legacy)
    sys.path.insert(0, str(HERE))
    import selftest_inproc
    for part in ("shim", "bridge", "probe", "plumbing"):
        if only and part not in only:
            continue
        for arch in ARCHES:
            ok, detail = getattr(selftest_inproc, part)(arch, SELF / arch)
            failures += not ok
            print(f"{'PASS' if ok else 'FAIL'} {part:22} {arch}: {detail}", flush=True)
    madoc_after = reg_subkeys(r"Software\Mad Doc Software\EE-AOC")
    added = sorted(set(madoc_after or []) - set(madoc_before or []))
    if any(k in ("exe_boot scratch",) or "selftest" in k for k in added):
        print(f"FAIL the game's key gained {added}")
        failures += 1
    for k in REG_KEYS:
        reg_delete_tree(k)
    (BOOT / "selftest_victim.txt").unlink(missing_ok=True)
    print(f"{'ALL PASS' if not failures else f'{failures} FAILED'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
