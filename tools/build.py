"""Build the game from source: one statically linked EE-AOC.exe (clang-cl + lld-link, 32-bit, the MSVC STL and
the static UCRT).

  python -I tools/build.py [-j N] [--out DIR] [--opt X] [--test-profile windowed-software] [--sanitize-null]
                           [--compile-only]

-> <out>/exe/EE-AOC.exe with its .map (8-digit addresses) and .pdb (default out: build/clang). The exe holds the
game with its image libraries (libpng/zlib/libjpeg), the engine, the plug-ins the original loaded as DLLs (TCP
protocol, the mixer, the Default random map generator, the DX7 T&L renderer; registered in PIMModule.cpp) and the
language resources, with miniaudio, dmusic and pl_mpeg for sound, music and movies. Only Windows system DLLs are
imported. Every unit is compiled first (tools/clang_build.py, objects in <out>/obj), then linked.

--test-profile windowed-software (TEST ONLY; default out build/clang-test): images for the headless live-game
harness (tools/harness). The windowed and 32-bit display policy is compiled into the exe by text rules on the
staged copies of a few units (TEST_PROFILES, each refusing unless it matches its expected count), and
every function the harness hooks at its entry is kept out of line (HARNESS_HOOKS). src/ is never changed.

--sanitize-null (TEST ONLY; out gains -nullsan): with the null-pointer and pointer-overflow
UBSan checks in recover mode and tools/harness/ubsan_minimal.c's handlers in every image: each distinct site the
game reaches is logged once ("EE-UBSAN <check> <module>+<rva>").

The exe links stubs for what the source does not define yet (the WON library; tools/exe_fulllink.py). Data the
original placed inside other objects (link/exe/fulllink_members.tsv, VC6 offsets) is placed at the same member's
offset in this build, from the build's record layouts (tools/harness/layout_gen.py, written to build/harness).
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
from ee import CLANG, LINK_DIR, LLD_LINK, MODULE_FLAGS, ROOT, TOOLCHAIN, module_units

TEST_PROFILES = {
    "windowed-software": {
        # windowed: the settings default and EEGamePreferences::Load keep mFullScreen false
        "Empire Earth/Empire Earth/EEPreferences.cpp": [
            ("    this->mFullScreen = true;", "    this->mFullScreen = false;   // TEST PROFILE windowed", 1),
            ("    this->mSettings.mFullScreen = true;",
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


def compile_all(targets, out, opt, jobs, overlays=None, extra=()):
    """Compile every unit the targets link, in parallel -> objs(source, flags) -> object path; exits on a failure."""
    mods = [m for t in targets for m in units.TARGET_MODULES.get(t, [t])]
    work = [j for m in mods for j in units.jobs_for(m)] + units.lib_jobs(mods)
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


def member_offsets(out):
    """exe_fulllink.link_exe's member_offsets: the member table's VC6 offset -> the same member's offset in this
    build (tools/harness/layout.py translate; the container's class from its mangled name). A Dinkum vector's _First
    (+4) is this library's first-element pointer (+0). Offset 0 and tables of pointers stay."""
    sys.path.insert(0, str(ROOT / "tools" / "harness"))
    import layout as L
    lay = L.Layout(out)

    def translate(cont, off):
        if off == 0 or re.match(r"\?[^@]+@(?:[^@]+@)*@[23]P", cont):
            return off
        m = re.match(r"\?[^@]+@(?:[^@]+@)*@[23]([VU])([A-Za-z_0-9]+)@@", cont)
        if m:
            return lay.translate(m.group(2), off)
        if "?$vector@" in cont and off == 4:
            return 0
        raise SystemExit(f"member alias container {cont}+{off:#x}: no translation for this build's layout")
    return translate


# system libraries the engine and the renderer linked statically (dinput8: the c_dfDI* data formats), then the
# renderer's wgpu-native (static, tools/fetch_wgpu.py) with the system libraries its DX12/Vulkan backends and the
# Rust standard library import
STATIC_LIBS = ["dxguid.lib", "dinput8.lib", "ole32.lib", "winmm.lib", "user32.lib",
               str(TOOLCHAIN / "wgpu" / "i686" / "lib" / "wgpu_native.lib"), "gdi32.lib", "advapi32.lib", "ws2_32.lib",
               "userenv.lib", "ntdll.lib", "bcrypt.lib", "oleaut32.lib", "d3d12.lib", "dxgi.lib", "d3dcompiler.lib",
               "opengl32.lib", "propsys.lib", "runtimeobject.lib", "uuid.lib", "synchronization.lib", "shell32.lib",
               "dwmapi.lib"]


def link_exe(out, objs, extra_libs):
    import exe_fulllink
    import rsrc_rebuild
    print("--- exe: link", flush=True)
    dst = out / "exe"
    dst.mkdir(parents=True, exist_ok=True)
    items = []
    for mod in ("exe", "exeimg"):
        for lo, hi, p in module_units(mod):
            if mod == "exeimg" and units.rel_src(p).parts[1] == "libjpeg":
                continue   # the engine's libjpeg (module jpeg, every file) serves the game too
            items.append({"mod": mod, "lo": lo, "hi": hi, "path": p, "flags": MODULE_FLAGS[mod],
                          "rel": str(units.rel_src(p)), "obj": objs(p, MODULE_FLAGS[mod])})
    # the rest of the game: every unit of the engine (with libjpeg) and the plug-ins, and their library units
    rest = [m for m in units.TARGET_MODULES["exe"] if m not in ("exe", "exeimg")]
    others = [objs(j[1], j[2]) for m in rest for j in units.jobs_for(m)] +              [objs(j[1], j[2]) for j in units.lib_jobs(rest)]
    exe_fulllink.EXTRA_LIBS = [str(o) for o in others] + list(extra_libs) + STATIC_LIBS
    exe_fulllink.IMPORT_MODULES = [m for m in units.TARGET_MODULES["exe"] if (LINK_DIR / m / "imports.txt").exists()]
    exe_fulllink.EXTRA_RES = [rsrc_rebuild.compile_rc(ROOT / "src" / units.LANGUAGE_RC, out / "rsrc-language")]
    rep, _ = exe_fulllink.link_exe(dst, items, {}, members=units.EXE_MEMBERS, member_offsets=member_offsets(out))
    import json
    (dst / "link_report.json").write_text(json.dumps(rep, indent=1, default=str))
    p1 = rep.get("pass1", {}).get("unresolved", {})
    traps = sorted(s for s, v in p1.items() if v.get("class") not in ("won", "icf", "vslot", "pure", "data", "library"))
    if traps:
        print(f"exe: {len(traps)} unresolved first-party symbol(s) (trap stubs): {traps[:5]}")
    p2 = rep.get("pass2", {})
    ma = rep.get("member_alias", {"exit": 0, "unresolved": {}, "duplicates_link": []})
    for k in ("unresolved", "duplicates_link", "other"):
        if p2.get(k):
            print(f"pass 2 {k}: {list(p2[k])[:15]}")
    ok = (p2.get("exit") == 0 and not p2.get("unresolved") and not p2.get("duplicates_link")
          and ma["exit"] == 0 and not ma["unresolved"] and not ma["duplicates_link"])
    print(f"exe: {'ok' if ok else 'FAILED'} -> {dst / 'EE-AOC.exe'}", flush=True)
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", help="output root (default build/clang, build/clang-test with --test-profile)")
    ap.add_argument("--opt", default="-O1", help="clang-cl optimisation (default -O1)")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--test-profile", choices=sorted(TEST_PROFILES), help="TEST ONLY: see the module doc")
    ap.add_argument("--sanitize-null", action="store_true", help="TEST ONLY: see the module doc")
    ap.add_argument("--compile-only", action="store_true", help="compile every unit, link nothing")
    a = ap.parse_args()
    for tool in (CLANG, LLD_LINK):
        if not tool.exists():
            sys.exit(f"{tool} is missing: install LLVM (winget install LLVM.LLVM) or set EE_LLVM")
    out = pathlib.Path(a.out or ROOT / "build" / (("clang-test" if a.test_profile else "clang")
                                                  + ("-nullsan" if a.sanitize_null else ""))).resolve()
    out.mkdir(parents=True, exist_ok=True)
    overlays = TEST_PROFILES.get(a.test_profile)
    extra, extra_libs = (), []
    if a.sanitize_null:
        extra = (f"-fsanitize={SANITIZE_CHECKS}", "-fsanitize-minimal-runtime",
                 f"-fsanitize-recover={SANITIZE_CHECKS}")
        ubsan = out / "ubsan_minimal.obj"
        r = subprocess.run([str(CLANG), "--target=i686-pc-windows-msvc", "/nologo", "/c", "/O1", "/Zl", "/GS-",
                            str(ROOT / "tools" / "harness" / "ubsan_minimal.c"), f"/Fo{ubsan}"],
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"ubsan_minimal.c: {r.stdout}{r.stderr}")
        extra_libs.append(str(ubsan))
    t0 = time.time()
    objs = compile_all(units.TARGETS, out, a.opt, a.j, overlays, extra)
    if a.compile_only:
        return
    sys.path.insert(0, str(ROOT / "tools" / "harness"))   # the link places the member aliases by the record layouts
    import layout_gen
    layout_gen.generate(out, overlays, extra)
    result = link_exe(out, objs, extra_libs)
    print(f"built in {time.time() - t0:.0f} s -> {out}")
    sys.exit(result)


if __name__ == "__main__":
    main()
