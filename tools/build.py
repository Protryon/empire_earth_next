"""Build every image of the game from source: clang-cl + lld-link, 32-bit, the MSVC STL and the UCRT.

  python -I tools/build.py [-j N] [--out DIR] [--opt X] [--test-profile windowed-software] [--sanitize-null]
                           [--compile-only] [target ...]

Targets (default: all): tcp mixer default lle dx7hr dx7hrtnl language exe -> <out>/<target>/ (default out:
build/clang): TCP Protocol.dll, Miles Sound System Mixer.dll, Default.dll, Low-Level Engine.dll (with libjpeg),
DX7HRDisplay.dll, DX7HRTnLDisplay.dll, Language.dll (resources only), EE-AOC.exe (with libpng/zlib/libjpeg), each
with its .map (8-digit addresses) and .pdb. Every unit is compiled first (tools/clang_build.py, objects in
<out>/obj), then the engine is linked (the other images link against its import library), then the rest.

--test-profile windowed-software (TEST ONLY; default out build/clang-test): images for the headless live-game
harness (tools/harness). The windowed and software-display policy is compiled into the exe and the renderer by text
rules on the staged copies of a few units (TEST_PROFILES, each refusing unless it matches its expected count), and
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
import relink
import units
import verify
from ee import CLANG, LLD_LINK, MODULE_FLAGS, ROOT, module_units

TEST_PROFILES = {
    "windowed-software": {
        # windowed: the settings default and EEGamePreferences::Load keep mFullScreen false
        "Empire Earth/Empire Earth/EEPreferences.cpp": [
            ("    this->mFullScreen = true;", "    this->mFullScreen = false;   // TEST PROFILE windowed", 1),
            ("    this->mSettings.mFullScreen = true;",
             "    this->mSettings.mFullScreen = false;   // TEST PROFILE windowed", 1)],
        # software display: the bit-depth checks accept and store 32 where they accepted/stored 16, and the
        # no-matching-mode fallback is 32
        "Empire Earth/Empire Earth/Empire Earth.cpp": [
            ("mGameBitDepth != 16)", "mGameBitDepth != 32)", 1),
            ("mTextureBitDepth != 16)", "mTextureBitDepth != 32)", 1),
            ("mGameBitDepth = 16;", "mGameBitDepth = 32;", 2),
            ("mTextureBitDepth = 16;", "mTextureBitDepth = 32;", 1)],
        # the renderer's software profile: the D3D7 RGB software device instead of a HAL, every surface in system
        # memory, no texture management
        "Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/Common/DX7Rasterizer.cpp": [
            ("(inCaps->dwDevCaps & D3DDEVCAPS_HWRASTERIZATION) == D3DDEVCAPS_HWRASTERIZATION;",
             "(inCaps->dwDevCaps & D3DDEVCAPS_FLOATTLVERTEX) == D3DDEVCAPS_FLOATTLVERTEX;   // TEST PROFILE software", 1),
            ("if ((theCaps.dwCaps & DDCAPS_3D) == DDCAPS_3D) {",
             "if (true) {   // TEST PROFILE software: adapters without DDCAPS_3D", 2),
            ("IID_IDirect3DHALDevice", "IID_IDirect3DRGBDevice", 1)],
        "Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/Common/DX7Screen.cpp": [
            ("this->GetMode(i).mHeight == 600 && this->GetMode(i).mBitCount == 16)",
             "this->GetMode(i).mHeight == 600 && this->GetMode(i).mBitCount == 32)", 1),
            ("IID_IDirect3DHALDevice", "IID_IDirect3DRGBDevice", 1),
            ("        theDesc.ddsCaps.dwCaps = inDescription.mIs3DRenderable ?\n"
             "            DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM | DDSCAPS_3DDEVICE :\n"
             "            DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;\n",
             "        theDesc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;   // TEST PROFILE software\n", 1),
            ("DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM",
             "DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY", 2),
            ("DDSCAPS_ZBUFFER | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;", "DDSCAPS_ZBUFFER | DDSCAPS_SYSTEMMEMORY;", 1),
            ("DDSCAPS_TEXTURE | DDSCAPS_3DDEVICE | DDSCAPS_VIDEOMEMORY |\n                                     DDSCAPS_LOCALVIDMEM;",
             "DDSCAPS_TEXTURE | DDSCAPS_3DDEVICE | DDSCAPS_SYSTEMMEMORY;", 1),
            ("            theDesc.ddsCaps.dwCaps = DDSCAPS_TEXTURE;\n"
             "            theDesc.ddsCaps.dwCaps2 = inDescription.mSurfaceVolatility ?\n"
             "                DDSCAPS2_TEXTUREMANAGE | DDSCAPS2_HINTSTATIC :\n"
             "                DDSCAPS2_TEXTUREMANAGE | DDSCAPS2_HINTDYNAMIC;\n",
             "            theDesc.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;   // TEST PROFILE software\n", 1),
            ("            DDSCAPS_TEXTURE | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP | DDSCAPS_3DDEVICE :\n"
             "            DDSCAPS_TEXTURE | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;\n"
             "        theDesc.ddsCaps.dwCaps2 = inDescription.mSurfaceVolatility ?\n"
             "            DDSCAPS2_TEXTUREMANAGE | DDSCAPS2_HINTSTATIC :\n"
             "            DDSCAPS2_TEXTUREMANAGE | DDSCAPS2_HINTDYNAMIC;\n",
             "            DDSCAPS_TEXTURE | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP | DDSCAPS_3DDEVICE | DDSCAPS_SYSTEMMEMORY :\n"
             "            DDSCAPS_TEXTURE | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP | DDSCAPS_SYSTEMMEMORY;   // TEST PROFILE\n", 1)],
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


def link_exe(out, objs, extra_libs):
    import exe_fulllink
    print("--- exe: full link", flush=True)
    dst = out / "exe"
    dst.mkdir(parents=True, exist_ok=True)
    items = []
    for mod in ("exe", "exeimg"):
        for lo, hi, p in module_units(mod):
            items.append({"mod": mod, "lo": lo, "hi": hi, "path": p, "flags": MODULE_FLAGS[mod],
                          "rel": str(units.rel_src(p)), "obj": objs(p, MODULE_FLAGS[mod])})
    exe_fulllink.EXTRA_LIBS = list(extra_libs)
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


def link_language(out):
    import rsrc_rebuild
    print("--- language: resources", flush=True)
    dll = rsrc_rebuild.build_dll(ROOT / "src" / units.LANGUAGE_RC, out / "language", "Language.dll")
    return 0 if dll.exists() else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("targets", nargs="*", metavar="target", help=f"default: all ({' '.join(units.TARGETS)})")
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
    targets = a.targets or units.TARGETS
    for t in targets:
        if t in units.PART_OF:
            sys.exit(f"{t} is linked into {units.PART_OF[t]}: build {units.PART_OF[t]}")
        if t not in units.TARGETS:
            sys.exit(f"unknown target {t!r}; known: {' '.join(units.TARGETS)}")
    targets = sorted(targets, key=lambda t: t != "lle")   # the engine first: the others link its import library
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
    objs = compile_all([t for t in targets if t != "language"], out, a.opt, a.j, overlays, extra)
    if a.compile_only:
        return
    if "exe" in targets:   # the exe link places the member aliases by this build's record layouts
        sys.path.insert(0, str(ROOT / "tools" / "harness"))
        import layout_gen
        layout_gen.generate(out, overlays, extra)
    engine_lib = out / "lle" / "Low-Level Engine.lib"
    results = {}
    for t in targets:
        if t == "language":
            results[t] = link_language(out)
            continue
        if t != "lle" and not engine_lib.exists():
            sys.exit(f"{t}: {engine_lib} is missing: build lle first")
        libs = extra_libs + ([str(engine_lib)] if t != "lle" else [])
        if t == "exe":
            results[t] = link_exe(out, objs, libs)
        else:
            print(f"--- {t}: link", flush=True)
            results[t] = relink.relink(t, out / t, objs, libs)
    print()
    for t in targets:
        print(f"{t:9} {'ok' if results[t] == 0 else 'FAILED'}")
    print(f"built in {time.time() - t0:.0f} s -> {out}")
    sys.exit(1 if any(results.values()) else 0)


if __name__ == "__main__":
    main()
