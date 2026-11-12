"""TEST ONLY: the in-process parts of the self test (selftest.py), each for the x64 harness, each returning
(ok, detail):
  shim      the time shim (timeshim.stage_timeshim) loaded by shim_test.exe staged as EE-AOC.exe (WINMM renamed)
  bridge    the presentation bridge (presentation_bridge.compile_bridge) loaded by bridge_test.exe
  probe     game_state_probe.sample on probe_test.exe laid out by the x64 record layouts
  plumbing  hbuild.Build on a stand-in build (the guard test exe, a lld-format map of the game's x64 names, the real
            layouts): bridge defines, renderer/static-exe RVAs, the production
            bridge build (build_presentation_bridge), the no-human-defeat and mutex-bypass locators, the frame-cap range
  fixture   fixture_driver.py (the gates' fixture driver: commands, verdicts, frames, the civilization pre-empt, the
            play loop's time cap, the telemetry and game log parsers) against fixture_standin.py, the fixture's protocol
            without a game
Stages: build/boot/selftest_stage, build/boot/selftest_fixture (fixture) and (probe, plumbing: the paths those tools accept) build/boot/windowed_o_32."""
import pathlib
import re
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
HARNESS = HERE.parent
sys.path.insert(0, str(HARNESS))
sys.path.insert(0, str(HERE))
import selftest as st          # noqa: E402

BRIDGE_NAMES = {
    "?Enter@TSCriticalSection@@QEAAXXZ": "selftest_enter",
    "?Leave@TSCriticalSection@@QEAAXXZ": "selftest_leave",
    "?TryEnter@TSCriticalSection@@QEAA_NXZ": "selftest_try_enter",
    "?GetTriggerEvent@UIControl@@QEAAPEAVUIEvent@@K@Z": "selftest_get_trigger",
    "?FireTriggerEvent@UIControl@@QEAAXK@Z": "selftest_fire_trigger",
    "??_7UICheckBox@@6B@": "selftest_checkbox_vtbl",
    "?SetChecked@UICheckBox@@QEAAX_N@Z": "selftest_set_checked",
    "?GetIsChecked@UICheckBox@@QEBA_NXZ": "selftest_get_checked",
    "GXTestSetPresentHook": "selftest_set_present_hook",
    "GXTestReadFrame": "selftest_read_frame",
}


def fresh_stage():
    if st.STAGE.exists():
        if st.STAGE.resolve().parent != st.BOOT.resolve():
            raise ValueError("stage must be inside build/boot")
        shutil.rmtree(st.STAGE)
    st.STAGE.mkdir(parents=True)
    return st.STAGE


def lld_map(path, base, symbols):
    """A minimal lld-link map (the format LinkMap reads) of symbols {name: rva}."""
    rows = "".join(f" 0001:{rva - 0x1000:08x}       {name} {base + rva:016x}     selftest.obj\n"
                   for name, rva in sorted(symbols.items(), key=lambda kv: kv[1]))
    pathlib.Path(path).write_text(f" EE-AOC\n\n Preferred load address is {base:016x}\n\n"
                                  "  Address         Publics by Value              Rva+Base               Lib:Object\n\n"
                                  + rows, encoding="latin-1")


def shim(arch, d):
    import timeshim
    exe = st.compile_c(arch, d / "shim_test.exe", [HERE / "shim_test.c"], ["-lwinmm"])
    syms, base = st.symbols(arch, exe)
    stage = fresh_stage()
    baseline = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60, cwd=stage).stdout.strip()
    shutil.copy(exe, stage / "EE-AOC.exe")
    # the frame cap's range comes from the map by RTSGraphics::Main's name
    lld_map(stage / "EE-AOC.map", base, {timeshim.RENDER_MAIN: syms["render_main"], "?End@@YAXXZ": syms["render_main_end"]})
    # the DLL is built under the self test's own directory, staged by stage_timeshim as the sessions do
    timeshim.BUILD = st.SELF / "timeshim" / "x64"
    m = timeshim.stage_timeshim(stage, 8, frame_ms=40, fulllink_map=stage / "EE-AOC.map")
    out = subprocess.run([str(stage / "EE-AOC.exe")], capture_output=True, text=True, timeout=60, cwd=stage).stdout.strip()
    v = dict(re.findall(r"(\w+) ([\d.]+)", out))
    b = dict(re.findall(r"(\w+) ([\d.]+)", baseline))
    f = lambda k: float(v.get(k, "nan"))   # noqa: E731
    ok = (m["arch"] == arch and m["engine_import_renamed"] and 60 <= f("sleep_ms") <= 250 and int(f("wfso")) == 258
          and 60 <= f("wfso_ms") <= 250 and int(f("wfmo")) == 258 and 60 <= f("wfmo_ms") <= 250 and 20 <= f("ticks") <= 60
          and f("render_ms") >= 150 and 6.0 <= f("qpc_ratio") <= 8.5 and 6.0 <= f("tgt_ratio") <= 8.5
          and float(b.get("sleep_ms", 0)) >= 790 and 0.9 <= float(b.get("qpc_ratio", 0)) <= 1.1)
    return ok, (f"factor 8, render rva {m['render_main']}: {out}  |  unshimmed: {baseline}")


def bridge(arch, d):
    import presentation_bridge as pb
    import hbuild
    exe = st.compile_c(arch, d / "bridge_test.exe", [HERE / "bridge_test.c"], [])
    syms, base = st.symbols(arch, exe)
    stage = fresh_stage()
    offsets = subprocess.run([str(exe), "--offsets"], capture_output=True, text=True, timeout=30, check=True).stdout
    defines = {k: int(v) for k, v in re.findall(r"(\w+) (\d+)", offsets)}
    defines["EE_UI_MANAGER_RVA"] = syms["selftest_ui_manager"]
    defines["EE_SIZE_UWIDESTRING"] = 0x20
    # the names the host does not implement point at data: a call through one would crash the test
    rvas = {n: syms[BRIDGE_NAMES[n]] if n in BRIDGE_NAMES else syms["selftest_unused"] for n in pb.symbol_names()}
    (d / "bridge-build").mkdir(exist_ok=True)
    pb.compile_bridge(stage / "TestPresentation.dll", defines, rvas, st.gcc(arch), work=d / "bridge-build")
    shutil.copy(exe, stage / "bridge_test.exe")
    r = subprocess.run([str(stage / "bridge_test.exe")], capture_output=True, text=True, timeout=60, cwd=stage)
    out = r.stdout.strip()
    want = "bridge 1 hooked 1 frame 1 reads 6 state 1 event_rva 1 clicked 1 checked 1 critical 0"
    machine = hbuild.machine(stage / "TestPresentation.dll")
    return out == want and machine == arch, f"{machine} TestPresentation.dll, offsets {defines}: {out or r.returncode}"


# game_state_probe reads only staged game processes: <worktree>/build/boot/windowed_[orf](_K)/EE-AOC.exe. The self test
# uses instance 32 of the "o" kind of its own checkout (never a session's windowed_f).
PROBE_STAGE = st.BOOT / "windowed_o_32"
PROBE_FIELDS = {   # define: (class, VC6 offset) as game_state_probe.py reads them (layout.off)
    "NESC_LAST_TICK": ("NESynchronousCommunications", 4), "NESC_TICKS": ("NESynchronousCommunications", 0x10),
    "NESC_PAUSED": ("NESynchronousCommunications", 0x2d), "NESC_MP": ("NESynchronousCommunications", 0x2e),
    "PHYS_MS": ("EEPhysics", 0xd0), "PHYS_OVER": ("EEPhysics", 0xe6), "PHYS_PT": ("EEPhysics", 0xb4),
    "WORLD_COUNT": ("EEWorld", 0xcc), "WORLD_LOCAL": ("EEWorld", 0xb84), "WORLD_VICTORY": ("EEWorld", 0xb96),
    "WORLD_MAP": ("EEWorld", 0xbc), "WORLD_PLAYERS": ("EEWorld", 0x74),
    "PLAYER_OBSERVER": ("EEPlayer", 0x467), "PLAYER_RESULT": ("EEPlayer", 0xa28), "PLAYER_RESOURCES": ("EEPlayer", 0xafc),
    "PLAYER_LB14": ("EEPlayer", 0xb14), "PLAYER_LB18": ("EEPlayer", 0xb18), "PLAYER_BB49": ("EEPlayer", 0xb49),
    "PLAYER_BB4A": ("EEPlayer", 0xb4a),
}


def layout_dir(arch):
    """The record layouts of this checkout (layout_gen.py, cached under build/boot/selftest/<arch>)."""
    d = st.SELF / arch / "layout"
    if not (d / "layout.json").exists():
        d.mkdir(parents=True, exist_ok=True)
        subprocess.run([sys.executable, "-I", str(HARNESS / "layout_gen.py"), "--out", str(d)], check=True)
    return d


def probe(arch, d):
    import time
    import layout as L
    import game_state_probe
    lay = layout_dir(arch)
    L.use(lay)
    defs = {k: L.off(c, o) for k, (c, o) in PROBE_FIELDS.items()}
    defs["WORLD_BYTES"] = L._CURRENT.size("EEWorld")
    defs["PLAYER_BYTES"] = L._CURRENT.size("EEPlayer")
    (d / "probe_offsets.h").write_text("".join(f"#define {k} {v:#x}\n" for k, v in defs.items()), encoding="ascii")
    exe = st.compile_c(arch, d / "probe_test.exe", [HERE / "probe_test.c"], [], [f"-I{d}"])
    syms, base = st.symbols(arch, exe)
    if PROBE_STAGE.exists():
        shutil.rmtree(PROBE_STAGE)
    PROBE_STAGE.mkdir(parents=True)
    shutil.copy(exe, PROBE_STAGE / "EE-AOC.exe")
    proc = subprocess.Popen([str(PROBE_STAGE / "EE-AOC.exe")], cwd=PROBE_STAGE, stdout=subprocess.DEVNULL)
    try:
        for _ in range(100):
            if (PROBE_STAGE / "probe.ready").exists():
                break
            time.sleep(0.05)
        addresses = dict(communications=base + syms["selftest_communications"], world=base + syms["selftest_world_object"],
                         physics=base + syms["selftest_physics"], image_base=base)
        s = game_state_probe.sample(proc.pid, addresses, players=True)
    finally:
        proc.kill()
        proc.wait()
        L._CURRENT = None
    rows = {r["index"]: r for r in s["player_rows"]}
    p = 8
    ok = (s["pointer_size"] == p and s["last_tick_received"] == 1234 and s["physics_ticks_processed"] == 5678
          and s["paused"] and not s["multiplayer"] and s["world_players"] == 3 and s["world_map"] != "0x0"
          and s["game_ms"] == 4321 and s["game_over_be6"] == 0 and s["pt_ub4"] == 99 and s["local_player"] == 1
          and s["victory_enabled"] == 1 and sorted(rows) == [1, 2]
          and all(rows[i]["vtable"] == hex(0x1000 + i) and rows[i]["observer_b467"] == (i == 2)
                  and rows[i]["result_a28"] == i and rows[i]["resources"] == [10 * i + r for r in range(5)]
                  and rows[i]["lb14"] == 7 * i and rows[i]["victory_units_lb18"] == 8 * i
                  and rows[i]["civ_wait_bb49"] == 1 and rows[i]["civ_chosen_bb4a"] == 0 for i in (1, 2)))
    shutil.rmtree(PROBE_STAGE)
    return ok, (f"{p}-byte pointers, image base {base:#x} (ASLR: the probe relocates), EEWorld {defs['WORLD_BYTES']:#x} "
                f"bytes, mPlayers +{defs['WORLD_PLAYERS']:#x}: {'values read back' if ok else s}")


def plumbing(arch, d):
    """The session plumbing on a stand-in build of arch (no game)."""
    import hbuild
    import layout as L
    import presentation_bridge as pb
    import no_human_defeat
    import timeshim
    sys.path.insert(0, str(HARNESS))
    import game_session
    exe = d / "guard_test.exe"   # built by selftest.build_all
    syms, base = st.symbols(arch, exe)
    fake = d / "fakebuild"
    (fake / "exe").mkdir(parents=True, exist_ok=True)
    shutil.copy(exe, fake / "exe" / "EE-AOC.exe")
    shutil.copy(layout_dir(arch) / "layout.json", fake / "layout.json")
    # every game name the session plumbing looks up, at distinct stand-in RVAs
    names = set(pb.symbol_names()) | {hbuild.SHOWSCENE, hbuild.ACTIVATE, hbuild.NATIVE_ERROR, hbuild.ABORT,
                                      hbuild.RNG_GENERATE, hbuild.RNG_NEXT, no_human_defeat.OUT_SYMBOL,
                                      no_human_defeat.RESULT_SYMBOL, no_human_defeat.WORLD_SYMBOL, timeshim.RENDER_MAIN,
                                      "?gUIManager@@3PEAVEEUIManager@@EA", "?gMouse@@3PEAVEEMouse@@EA",
                                      game_session.SEED_SYMBOL, game_session.PHYSICS_SYMBOL}
    table = {n: 0x10000 + 0x10 * k for k, n in enumerate(sorted(names))}
    table[game_session.MUTEX_SYMBOL] = syms["selftest_instance_mutex"]
    table[game_session.MUTEX_GETLASTERROR] = syms[game_session.MUTEX_GETLASTERROR]
    lld_map(fake / "exe" / "EE-AOC.map", base, table)
    b = hbuild.Build(fake)
    problems = []
    if (b.arch, b.ptr, b.base) != (arch, 8, base):
        problems.append(f"arch {b.arch} ptr {b.ptr} base {b.base:#x}")
    want = lambda n: table[n]   # noqa: E731
    if b.renderer_guards() != (want(hbuild.ACTIVATE), want(hbuild.SHOWSCENE)):
        problems.append(f"renderer guards {b.renderer_guards()}")
    if b.static_exe_args()[1:] != [f"{want(n):x}" for n in (hbuild.NATIVE_ERROR, hbuild.ABORT, hbuild.RNG_GENERATE, hbuild.RNG_NEXT)]:
        problems.append(f"static exe {b.static_exe_args()}")
    defines = b.bridge_defines()
    lay = L.Layout(fake)
    if defines["EE_OFF_UIM_FORMS_LAST"] - defines["EE_OFF_UIM_FORMS_FIRST"] != b.ptr \
            or defines["EE_SIZE_UWIDESTRING"] != lay.size("UWideString"):
        problems.append(f"bridge defines {defines}")
    if PROBE_STAGE.exists():
        shutil.rmtree(PROBE_STAGE)
    PROBE_STAGE.mkdir(parents=True)
    out = pb.build_presentation_bridge(PROBE_STAGE, b)
    if hbuild.machine(out) != arch:
        problems.append(f"bridge machine {hbuild.machine(out)}")
    h = (PROBE_STAGE / "presentation-build" / "bridge_syms.h").read_text()
    if f'{{"GXTestReadFrame", {want("GXTestReadFrame"):#x}}}' not in h:
        problems.append("bridge_syms.h lacks the GXTestReadFrame RVA")
    L.use(fake)
    try:
        (o, r, w), extra = no_human_defeat.harness_args(PROBE_STAGE, fake / "exe" / "EE-AOC.map")
    finally:
        L._CURRENT = None
    if (o, r, w) != (want(no_human_defeat.OUT_SYMBOL), want(no_human_defeat.RESULT_SYMBOL),
                     base + want(no_human_defeat.WORLD_SYMBOL)) or extra[4] != f"{lay.translate('EEWorld', 0x74):x}":
        problems.append(f"no-human-defeat {o:#x} {r:#x} {w:#x} {extra}")
    lo, hi = timeshim.render_main_extent(fake / "exe" / "EE-AOC.map")
    if lo != want(timeshim.RENDER_MAIN) or hi <= lo:
        problems.append(f"render range {lo:#x}..{hi:#x}")
    mutex = None
    # by map: the instruction after the rip-relative GetLastError call
    mutex = game_session.mutex_bypass_rva(exe.read_bytes(), b.map("exe"))
    body = exe.read_bytes()
    import pefile
    pe = pefile.PE(data=body, fast_load=True)
    if pe.get_data(mutex - 6, 2) != b"\xff\x15" or mutex - 6 - syms["selftest_instance_mutex"] >= 0x80:
        problems.append(f"mutex bypass rva {mutex:#x}")
    shutil.rmtree(PROBE_STAGE)
    return not problems, (f"{len(table)} map names, layout {fake / 'layout.json'}; bridge {out.name} {arch}"
                          + (f"; mutex test at rva {mutex:#x}" if mutex else "") + (f"; PROBLEMS {problems}" if problems else ""))


def fixture(arch, d):
    """fixture_driver.py against fixture_standin.py (no game, no layout, no debugger)."""
    import os
    import time
    import types
    import fixture_driver
    out = st.BOOT / "selftest_fixture"
    if out.exists():
        if out.resolve().parent != st.BOOT.resolve():
            raise ValueError("the fixture stage must be inside build/boot")
        shutil.rmtree(out)
    (out / "root").mkdir(parents=True)
    env = dict(os.environ, EE_TEST_TIME_FACTOR="32", EE_TEST_SEED="2")
    g = fixture_driver.Game(out, [sys.executable, "-I", str(HERE / "fixture_standin.py")], env, cwd=str(out / "root"),
                            deadline=time.monotonic() + 150)
    problems = []
    try:
        g.wait(lambda s: s["form"] == fixture_driver.MAIN, "the main menu", 30)
        pixels = fixture_driver.frame(g, "main-menu")
        if pixels[18:26] != (64).to_bytes(4, "little") + (-48).to_bytes(4, "little", signed=True):
            problems.append("frame size")
        g.click(12, 0, 6, "single player")
        if g.verdict(g.seq) != "triggered":
            problems.append(f"verdict {g.verdict(g.seq)}")
        seq = g.command("click 6 99")
        g.wait(lambda s: s["command"] >= seq, "a rejected click", 10)
        time.sleep(0.3)
        if g.verdict(seq) != "rejected":
            problems.append(f"rejected verdict {g.verdict(seq)}")
        g.click(6, 1, 26, "random map setup")
        g.click(26, 10, 11, "start")
        pre = fixture_driver.civ_preempt(g)
        if pre["form"] != 7 or pre["civ_chosen"] != 1:
            problems.append(f"preempt {pre}")
        tel = fixture_driver.wait_running(g, 4)
        boxes = fixture_driver.box_select(g, 11)
        steps = [(r["step"], r["selection"]) for r in boxes["steps"]]
        every = list(range(101, 110))
        if steps != [("select", every), ("select-left", every[:5]), ("add-right", every), ("dead", every[1:])]                 or boxes["steps"][3]["dead"] != 101 or not boxes["grew"]:
            problems.append(f"box select {steps} dead {boxes['steps'][3]['dead']}")
        seq = g.command("boxselect 11 7 0 0 1 1")
        g.wait(lambda s: s["command"] >= seq, "a rejected box", 10)
        time.sleep(0.3)
        if g.verdict(seq) != "rejected":
            problems.append(f"rejected box verdict {g.verdict(seq)}")
        ordered = fixture_driver.orders(g, 11)
        if ordered["ran"] != ["gather", "repair", "repair-queued", "patrol", "move"]                 or ordered["skipped"] != {"attack-ground": "no-unit"}                 or [len(r["units"]) for r in ordered["steps"]] != [2, 2, 2, 2, 0, 2]                 or ordered["steps"][2]["queued"] != 1 or ordered["steps"][1]["note"] != "retried":
            problems.append(f"orders {ordered}")
        try:   # the second gather's scouting ends "scout-unseen": not a skip, the step fails
            fixture_driver.order_step(g, 11, "gather")
            problems.append("a gather whose scouting saw nothing passed")
        except RuntimeError as error:
            if "scout-unseen" not in str(error):
                problems.append(f"scout-unseen gather: {error}")
        seq = g.command("order 11 9")
        g.wait(lambda s: s["command"] >= seq, "a rejected order", 10)
        time.sleep(0.3)
        if g.verdict(seq) != "rejected":
            problems.append(f"rejected order verdict {g.verdict(seq)}")
        g.capture = lambda name: fixture_driver.frame(g, name)
        scrolled = fixture_driver.scroll_check(g, 11, hold_ms=600, settle=0.6)
        hows = [(r["kind"], r["how"]) for r in scrolled["steps"]]
        if hows != [("key", "right"), ("key", "left"), ("edge", "left-edge"), ("zoom", "key-in"), ("zoom", "key-out"),
                    ("zoom", "wheel-in"), ("zoom", "wheel-out")] or scrolled["key"] != "left"                 or scrolled["steps"][0]["moved"] >= 0.5 or scrolled["zooms"][0] != 1.0:
            problems.append(f"scroll check {hows} {scrolled['key']} {scrolled['zooms']}")
        for name in ("scroll-key", "scroll-edge", "zoom-key-in", "zoom-key-out"):
            if not (out / f"{name}.bmp").exists():
                problems.append(f"no capture {name}")
        if fixture_driver.zoom_captures(g, 11, [50, 200]) != {"zoom50": True, "zoom200": True}                 or fixture_driver.camera(g)["zoom"] != 2.0:
            problems.append(f"zoom captures {fixture_driver.camera(g)}")
        held, failed = fixture_driver.hold_key, None
        fixture_driver.hold_key = lambda *_: None   # keys that never reach the game: the check must fail with numbers
        try:
            fixture_driver.scroll_check(g, 11, hold_ms=300, settle=0.3, captures=False)
        except RuntimeError as error:
            failed = str(error)
        finally:
            fixture_driver.hold_key = held
        if not failed or not failed.startswith("SCROLL CHECK FAIL key: the camera did not move right 0.0, left 0.0"):
            problems.append(f"scroll check failure {failed!r}")
        if tel.get("clock") != [1, 1] or tel.get("paused") != 0 or tel["players"][2].get("stock") != [1, 2, 3, 4, 5]:
            problems.append(f"telemetry {tel}")
        played = fixture_driver.play(g, types.SimpleNamespace(max_game_seconds=40, play_seconds=None, capture_every=0),
                                     ai_game=True)
        if played["outcome"] != "max-game-seconds":
            problems.append(f"outcome {played['outcome']}")
        taken = [line for line in g.game_output().splitlines() if line.startswith("TEST UI clock") or
                 line.startswith("TEST UI seed")]
        if len(taken) != 2:
            problems.append(f"switches {taken}")
    except Exception as error:
        problems.append(repr(error))
    finally:
        g.stop()
    log = fixture_driver.copy_game_logs(out / "root", out)
    if not log or [p["kind"] for p in log["players"].values()].count("computer") != 2 or 3 not in log["last_sample"]:
        problems.append(f"game log {log}")
    return not problems, "; ".join(problems) or "commands, verdicts, frames, pre-empt, box selection, orders, scroll check, zoom captures, time cap, telemetry, game log"
