"""Build the game from source: one statically linked x86_64 EE-AOC.exe (clang-cl + lld-link, the MSVC STL and the
static UCRT).

  python -I tools/build.py [-j N] [--out DIR] [--opt X] [--test-profile windowed-software]
                           [--sanitize-null] [--compile-only]

-> <out>/exe/EE-AOC.exe with its .map and .pdb (default out: build/clang), linked at the fixed image base 0x400000
(as the x64 build has always run). x86_64 is the only target (--x64 is accepted and ignored). The exe holds the game
with its image libraries (libpng/zlib/libjpeg), the engine, the plug-ins the original loaded as DLLs (TCP
protocol, the mixer, the Default random map generator, the DX7 T&L renderer; registered in PIMModule.cpp) and the
language resources, with miniaudio, dmusic and pl_mpeg for sound, music and movies. Only Windows system DLLs are
imported. Every unit is compiled first (tools/clang_build.py, objects in <out>/obj), then linked: one plain lld-link
of every object, the image libraries (an archive), the Windows SDK import libraries, the static libraries (SDL3,
wgpu-native) and runtime, and the resources.
Every symbol is defined in source (the WON library too: module won).

--test-profile windowed-software (TEST ONLY; default out build/clang-test): images for the headless live-game
harness (tools/harness). The windowed and 32-bit display policy is compiled into the exe by text rules on the
staged copies of a few units (TEST_PROFILES, each refusing unless it matches its expected count), and
every function the harness hooks at its entry is kept out of line (HARNESS_HOOKS). src/ is never changed.

--sim (TEST ONLY): also links <out>/exe/EE-SIM.exe, the headless simulation: the same objects plus its console
entry (src/Empire Earth/Empire Earth/EESimMain.cpp -> EESim.cpp; platform/PlatSim.h). tools/sim.py runs it.

--sanitize-null (TEST ONLY; out gains -nullsan): with the null-pointer and pointer-overflow
UBSan checks in recover mode and tools/harness/ubsan_minimal.c's handlers in every image: each distinct site the
game reaches is logged once ("EE-UBSAN <check> <module>+<rva>").

The build also writes <out>/layout.json (the record layouts the harness reads offsets from; tools/harness/layout_gen.py).
"""
import argparse
import concurrent.futures
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import clang_build
import units
import verify
import ee
from ee import CLANG, LLD_LINK, LLVM_LIB, MODULE_FLAGS, ROOT, RUNTIME_LIBS, lib_paths, module_units

TEST_PROFILES = {
    "windowed-software": {
        # windowed: the settings default keeps mFullScreen false, and the window_mode setting (which decides it in
        # EEGamePreferences::Load) is overridden to windowed
        "Empire Earth/Empire Earth/EEPreferences.cpp": [
            ("    this->mFullScreen = true;", "    this->mFullScreen = false;   // TEST PROFILE windowed", 1),
            ("    this->mSettings.mFullScreen = this->mSettings.mWindowMode == 0;",
             "    this->mSettings.mFullScreen = false;   // TEST PROFILE windowed", 1)],
        # 32-bit display: the bit-depth checks accept and store 32 where they accepted/stored 16, and the
        # no-matching-mode fallback is 32 (the renderer runs on wgpu, WARP on a machine without a GPU); WinMain
        # loads the harness's presentation bridge when it is staged (the DLL builds' plug-in scan loaded every DLL
        # of the folder)
        "Empire Earth/Empire Earth/Empire Earth.cpp": [
            ("int inShowCommand)\n{\n",
             "int inShowCommand)\n{\n    LoadLibraryA(\"TestPresentation.dll\");   // TEST PROFILE: the harness's presentation bridge\n", 1),
            ("mGameBitDepth != 16)", "mGameBitDepth != 32)", 1),
            ("mTextureBitDepth != 16)", "mTextureBitDepth != 32)", 1),
            ("mGameBitDepth = 16;", "mGameBitDepth = 32;", 2),
            ("mTextureBitDepth = 16;", "mTextureBitDepth = 32;", 1)],
    },
}
# Functions the harness hooks at their entry with a debugger breakpoint: kept out of line in the test profile, so a
# call inside their own unit cannot be inlined past the hook.
HARNESS_HOOKS = [
    ("Empire Earth/Empire Earth/EEWorld.cpp", "void EEWorld::RemovePlayerFromGame(bool inHasWon, unsigned long inPlayerID)"),
    ("Empire Earth/Player/EEPlayer.cpp", "void EEPlayer::SetGameResult(long inState)"),
    ("Empire Earth/Physics Thread/EEPhysics.cpp", "void EEPhysics::ReportStatistics()"),
    ("Empire Earth/Physics Thread/EEPhysics.cpp", "void EEPhysics::Tick()"),
    ("Empire Earth/Communications/EEMessage.cpp", "void EEMStartLoading::Execute()"),
    ("Empire Earth/Empire Earth/Empire Earth.cpp", "bool EECreateInstanceMutex()"),
    ("Empire Earth/User Interface Thread/EEWonNetAvailableGameFormManager.cpp",
     "bool EEPointUnitOrTile(const GE2DPoint& inPoint, EETile*& outTile, GE3DPoint& outPoint, bool inFlag)"),
    ("RTS Engine/Graphics/RTSGraphics.cpp", "unsigned long RTSGraphics::Main(void* inGraphics)"),
]
for _unit, _decl in HARNESS_HOOKS:
    TEST_PROFILES["windowed-software"].setdefault(_unit, []).append(
        ("\n" + _decl + "\n", "\n__declspec(noinline) " + _decl + "   // TEST PROFILE: harness hook\n", 1))
SANITIZE_CHECKS = "null,pointer-overflow,nonnull-attribute,returns-nonnull-attribute"


SIM_MAIN = ROOT / "src" / "Empire Earth" / "Empire Earth" / "EESimMain.cpp"   # EE-SIM.exe's entry (--sim)


def compile_all(targets, out, opt, jobs, overlays=None, extra=(), more=()):
    """Compile every unit the targets link, in parallel -> objs(source, flags) -> object path; exits on a failure."""
    mods = [m for t in targets for m in units.TARGET_MODULES.get(t, [t])]
    work = [j for m in mods for j in units.jobs_for(m)] + units.lib_jobs(mods) + list(more)
    for j in work:
        verify.include_paths(verify.stage_source(j[1]))
    table, failed = {}, []
    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for job, ok, log in ex.map(lambda j: clang_build.compile_one(j, out, opt, overlays, extra), work):
            if ok:
                table[(str(units.rel_src(job[1])), verify.unit_flags(job[1], job[2]))] = units.obj_path(out, job)
            else:
                failed.append(job)
                print(f"FAILED {job[0]} {units.rel_src(job[1])}\n{log[-2000:]}", flush=True)
    print(f"{len(work) - len(failed)}/{len(work)} units compiled in {time.time() - t0:.0f} s -> {out / 'obj'}",
          flush=True)
    if failed:
        sys.exit(1)

    def objs(src, flags):
        key = (str(units.rel_src(src)), verify.unit_flags(src, flags))
        if key not in table:
            sys.exit(f"{key[0]} was not compiled with flags {key[1]!r}")
        return table[key]
    return objs


def static_libs():
    """System libraries the engine and the window layer link statically (SDL3: tools/xbuild.py sdl3), then the
    renderer's wgpu-native of the target (static, tools/fetch_wgpu.py) with the system libraries its DX12/Vulkan
    backends and the Rust standard library import."""
    return ["ole32.lib", "winmm.lib", "user32.lib",
            # the window layer: SDL3 (static) and the system libraries it imports besides the ones below
            str(ee.sdl3_dir() / "lib" / "SDL3-static.lib"), "setupapi.lib", "version.lib", "cfgmgr32.lib",
            "imm32.lib",
            str(ee.wgpu_dir() / "lib" / "wgpu_native.lib"), "gdi32.lib", "advapi32.lib", "ws2_32.lib",
            "userenv.lib", "ntdll.lib", "bcrypt.lib", "oleaut32.lib", "d3d12.lib", "dxgi.lib", "d3dcompiler.lib",
            "opengl32.lib", "propsys.lib", "runtimeobject.lib", "uuid.lib", "synchronization.lib", "shell32.lib",
            "dwmapi.lib"]


LINK_FLAGS = ["/INCREMENTAL:NO", "/OPT:REF", "/OPT:ICF", "/MACHINE:X64", "/SUBSYSTEM:WINDOWS", "/NODEFAULTLIB",
              "/FIXED", "/BASE:0x400000", "/HIGHENTROPYVA:NO", "/ENTRY:WinMainCRTStartup", "/ERRORLIMIT:0", "/DEBUG"]
# Windows system DLLs the game imports (the Windows SDK's import libraries), in the order of the original images'
# import tables: a name both WSOCK32 (Winsock 1.1: the TCP protocol's) and WS2_32 export resolves to WSOCK32's
SDK_LIBS = ["imm32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "shell32.lib", "ole32.lib", "wsock32.lib",
            "advapi32.lib", "winmm.lib", "iphlpapi.lib", "ws2_32.lib"]



def run(cmd, cwd):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return r.returncode, (r.stdout + r.stderr).strip()


def link_exe(out, objs, extra_libs, name="EE-AOC", entry=None):
    """One lld-link of the exe: every object (the engine's and the plug-ins' first: global constructors run in
    object order, and the game's use engine globals, as when the engine DLL loaded first), the image libraries as an
    archive (only the members the game uses), the import and static libraries, the resources and the static
    runtime -> 0 when it links with no unresolved and no duplicate symbol."""
    import rsrc_rebuild
    print(f"--- exe: link {name}.exe", flush=True)
    dst = out / "exe"
    lib = dst / ("lib" if name == "EE-AOC" else f"lib-{name}")
    shutil.rmtree(lib, ignore_errors=True)
    lib.mkdir(parents=True)
    game, img = [], []
    for mod in ("exe", "exeimg"):
        for _, _, p in module_units(mod):
            if mod == "exeimg" and units.rel_src(p).parts[1] == "libjpeg":
                continue   # the engine's libjpeg (module jpeg, every file) serves the game too
            (game if mod == "exe" else img).append(objs(p, MODULE_FLAGS[mod]))
    rest = [m for m in units.TARGET_MODULES["exe"] if m not in ("exe", "exeimg")]
    first = [objs(j[1], j[2]) for m in rest for j in units.jobs_for(m)] + [objs(j[1], j[2]) for j in units.lib_jobs(rest)]
    game += [objs(ROOT / "src" / rel, MODULE_FLAGS["exe"]) for rel in units.EXTRA_UNITS.get("exe", [])]
    game += [objs(j[1], j[2]) for j in units.lib_jobs(["exe"])]   # the game's own libraries (Lua)
    (lib / "exeimg.rsp").write_text("".join(f'"{o}"\n' for o in img))
    for cmd in ([str(LLVM_LIB), "/nologo", "/out:exeimg.lib", "@exeimg.rsp"],):
        code, log = run(cmd, lib)
        if code:
            sys.exit(f"{cmd[-1]}: {log}")
    res = [rsrc_rebuild.compile_rc(units.EXE_RC, out / "rsrc"),
           rsrc_rebuild.compile_rc(ROOT / "src" / units.LANGUAGE_RC, out / "rsrc-language")]
    # the language strings are a generated table (LanguageStrings.cpp): it must be current and equal to rc.exe's
    import lang_strings
    if lang_strings.OUT.read_bytes().decode("latin-1") != lang_strings.generate():
        sys.exit(f"{lang_strings.OUT} is stale: python -I tools/lang_strings.py")
    _, theRc = lang_strings.parse_rc()
    if {k: v for k, v in theRc.items() if v} != lang_strings.parse_res(res[1]):
        sys.exit("the language string table differs from rc.exe's string tables (tools/lang_strings.py --check-res)")
    inputs = [*first, *[x for x in extra_libs if str(x).lower().endswith(".obj")], *game, lib / "exeimg.lib",
              *SDK_LIBS, *[x for x in extra_libs if not str(x).lower().endswith(".obj")],
              *static_libs(), *res, *RUNTIME_LIBS]
    if entry is not None:   # --sim: the console entry first among the game's objects
        inputs.insert(len(first), entry)
    tag = "" if name == "EE-AOC" else f"-{name}"
    rsp = dst / f"link{tag}.rsp"
    rsp.write_text("".join(f'"{x}"\n' for x in inputs))
    exe = dst / f"{name}.exe"
    exe.unlink(missing_ok=True)
    flags = LINK_FLAGS if entry is None else [
        {"/SUBSYSTEM:WINDOWS": "/SUBSYSTEM:CONSOLE", "/ENTRY:WinMainCRTStartup": "/ENTRY:mainCRTStartup"}.get(f, f)
        for f in LINK_FLAGS]
    code, log = run([str(LLD_LINK), "/nologo", *flags, f"/OUT:{exe.name}", f"/MAP:{exe.stem}.map",
                     f"/PDB:{exe.stem}.pdb", *lib_paths(), f"@{rsp.name}"], dst)
    log = "\n".join(l for l in log.splitlines() if "ignoring section .debug$S with unrecognized magic" not in l)
    (dst / f"link{tag}.log").write_text(log)
    unresolved = sorted(set(re.findall(r"undefined symbol: (.+)", log)))
    duplicates = sorted(set(re.findall(r"duplicate symbol: (.+)", log)))
    for kind, names in (("unresolved", unresolved), ("duplicate", duplicates)):
        if names:
            print(f"exe: {len(names)} {kind} symbol(s): " + "; ".join(names[:20]))
    ok = code == 0 and not unresolved and not duplicates and exe.exists()
    print(f"exe: {'ok' if ok else 'FAILED (' + str(dst / f'link{tag}.log') + ')'}: {len(inputs)} inputs, "
          f"{len(unresolved)} unresolved, {len(duplicates)} duplicates -> {exe}", flush=True)
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", help="output root (default build/clang, build/clang-test with --test-profile)")
    ap.add_argument("--opt", default="-O1", help="clang-cl optimisation (default -O1)")
    ap.add_argument("--x64", action="store_true", help=argparse.SUPPRESS)   # x86_64 is the only target
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--test-profile", choices=sorted(TEST_PROFILES), help="TEST ONLY: see the module doc")
    ap.add_argument("--sanitize-null", action="store_true", help="TEST ONLY: see the module doc")
    ap.add_argument("--compile-only", action="store_true", help="compile every unit, link nothing")
    ap.add_argument("--sim", action="store_true", help="TEST ONLY: also link EE-SIM.exe (see the module doc)")
    a = ap.parse_args()
    for tool in (CLANG, LLD_LINK):
        if not tool.exists():
            sys.exit(f"{tool} is missing: install LLVM (winget install LLVM.LLVM) or set EE_LLVM")
    if not (ee.wgpu_dir() / "lib" / "wgpu_native.lib").exists():
        sys.exit(f"{ee.wgpu_dir()} is missing: python -I tools/fetch_wgpu.py")
    if not (ee.sdl3_dir() / "lib" / "SDL3-static.lib").exists():   # the window layer: SDL3 from its pinned sources
        r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "xbuild.py"), "sdl3", "--target",
                            "windows-x86_64", "-j", str(a.j)])
        if r.returncode or not (ee.sdl3_dir() / "lib" / "SDL3-static.lib").exists():
            sys.exit("SDL3 did not build: python -I tools/fetch_posix.py sdl3, then python -I tools/xbuild.py sdl3")
    kind = "clang-test" if a.test_profile else "clang"
    out = pathlib.Path(a.out or ROOT / "build" / (kind + ("-nullsan" if a.sanitize_null else ""))).resolve()
    out.mkdir(parents=True, exist_ok=True)
    overlays = TEST_PROFILES.get(a.test_profile)
    extra, extra_libs = (), []
    if a.sanitize_null:
        extra = (f"-fsanitize={SANITIZE_CHECKS}", "-fsanitize-minimal-runtime",
                 f"-fsanitize-recover={SANITIZE_CHECKS}")
        ubsan = out / "ubsan_minimal.obj"
        r = subprocess.run([str(CLANG), "--target=x86_64-pc-windows-msvc", "/nologo", "/c", "/O1", "/Zl", "/GS-",
                            str(ROOT / "tools" / "harness" / "ubsan_minimal.c"), f"/Fo{ubsan}"],
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"ubsan_minimal.c: {r.stdout}{r.stderr}")
        extra_libs.append(str(ubsan))
    t0 = time.time()
    sim_job = ("exe", SIM_MAIN, MODULE_FLAGS["exe"])
    objs = compile_all(units.TARGETS, out, a.opt, a.j, overlays, extra, more=[sim_job] if a.sim else [])
    if a.compile_only:
        return
    result = link_exe(out, objs, extra_libs)
    if a.sim and result == 0:
        result = link_exe(out, objs, extra_libs, name="EE-SIM", entry=objs(SIM_MAIN, MODULE_FLAGS["exe"]))
    sys.path.insert(0, str(ROOT / "tools" / "harness"))   # the record layouts the harness reads offsets from
    import layout_gen
    layout_gen.generate(out, overlays, extra)
    print(f"built in {time.time() - t0:.0f} s -> {out}")
    sys.exit(result)


if __name__ == "__main__":
    main()
