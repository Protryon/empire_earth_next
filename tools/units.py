"""The build's modules, targets and compile jobs.

Units come from link/<module>/units.txt (in link order), or every .cpp of the module's source folder when the module
has no unit list; each compiles with its module's flags (ee.MODULE_FLAGS) or its own `// UNIT-FLAGS:` line. Library
code built from source (LIB_UNITS: the PKWARE DCL implode/explode, dmusic, cgltf and stb_image for the engine,
miniaudio for the mixer)
is compiled with the modules that link it.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import LINK_DIR, MODULE_FLAGS, ROOT, module_units

# Every module is linked into the one exe (no DLLs): the game, its image libraries, the engine with libjpeg, and the
# plug-ins (TCP protocol, mixer, Default random maps, the DX7 T&L renderer), registered in PIMModule.cpp.
MODULES = ["tcp", "mixer", "default", "lle", "jpeg", "dx7hrtnl", "exeimg", "exe"]
TARGETS = ["exe"]
TARGET_MODULES = {"exe": ["exe", "exeimg", "lle", "jpeg", "tcp", "mixer", "default", "dx7hrtnl"]}
PART_OF = {m: "exe" for m in MODULES if m != "exe"}
# units a module links besides its own: the platform layer's implementation (include/platform/, src/Platform/<os>/)
# with the engine, the open model/texture formats (glTF, PNG: tools/eemodel) with the engine, and new game units that
# have no address in the original (the db text loader, the WON library's stand-ins)
PLATFORM_UNITS = ["Platform/Win32/PlatTime.cpp", "Platform/Win32/PlatThread.cpp", "Platform/Win32/PlatString.cpp",
                  "Platform/Common/PlatConfig.cpp", "Platform/Win32/PlatFile.cpp", "Platform/Win32/PlatPath.cpp",
                  "Platform/Win32/PlatSocket.cpp", "Platform/Win32/PlatMessage.cpp",
                  # the window, its events and input (SDL3; PORT.md step 4.3)
                  "Platform/SDL/PlatWindow.cpp",
                  # text (stb_truetype) with the fonts built in (PORT.md step 4.5)
                  "Platform/Common/PlatFont.cpp", "Platform/Common/PlatFontData.c",
                  # the headless simulation's switches (EE-SIM.exe; all off in the game)
                  "Platform/Common/PlatSim.cpp"]
OPEN_FORMAT_UNITS = ["Low-Level Engine/Graphics Engine/GEOpenFormats.cpp",
                     "Low-Level Engine/Graphics Engine/GEOpenFormatsFile.cpp"]
EXTRA_UNITS = {"lle": PLATFORM_UNITS + OPEN_FORMAT_UNITS,
               "exe": ["Empire Earth/Database/EEDataText.cpp", "Empire Earth/Database/EEDataTextFile.cpp",
                       "Empire Earth/Database/EEDataDump.cpp", "Empire Earth/Database/EERules.cpp",
                       "Empire Earth/WONAPI/WONAPIStubs.cpp",
                       # the computer-player decision trace and cheat switches (docs/AI_RESEARCH.md stage 0)
                       "Empire Earth/AI Thread/EECPTrace.cpp",
                       # the next computer player's tactics layer (docs/AI_LUA_PLAN.md §2, stage S1; ee/EECPNext.h)
                       "Empire Earth/AI Thread/EECPNext.cpp", "Empire Earth/AI Thread/EECPNextTactics.cpp",
                       "Empire Earth/AI Thread/EECPNextView.cpp",
                       # TEST ONLY: the next engine's C++ test opening (stage S1's proof of the verbs)
                       "Empire Earth/AI Thread/EECPNextTest.cpp",
                       # the computer player's engine per seat: classic or next (docs/AI_LUA_PLAN.md S0)
                       "Empire Earth/AI Thread/EECPEngine.cpp",
                       # the computer player's Lua runtime: the strategy scripts' sandbox (docs/AI_LUA_PLAN.md §3)
                       "Empire Earth/AI Thread/EECPLua.cpp",
                       # the game log: every game, human and computer players (ee/EEGameLog.h)
                       "Empire Earth/Empire Earth/EEGameLog.cpp",
                       # TEST ONLY: the headless simulation's driver (EE-SIM.exe: build.py --sim links its entry)
                       "Empire Earth/Empire Earth/EESim.cpp",
                       # TEST ONLY: the POSIX test driver's in-process UI fixture (EE_TEST_UI; tools/harness/posix_game.py)
                       "Empire Earth/Empire Earth/EETestFixture.cpp",
                       # the language strings, generated from Language.rc (tools/lang_strings.py)
                       "Empire Earth/Language/LanguageStrings.cpp",
                       # a unit with no code range of its own (every member was ICF-folded into another unit's)
                       "Empire Earth/Calamities/EEGlobalCalamityClass.cpp"]}
LIB_UNITS = {"miniaudio": "ThirdParty/miniaudio/miniaudio.c", "pl_mpeg": "ThirdParty/pl_mpeg/pl_mpeg.c",
             "implode": "ThirdParty/pkdcl/implode.c", "explode": "ThirdParty/pkdcl/explode.c",
             "cgltf": "ThirdParty/cgltf/cgltf.c", "stb_image": "ThirdParty/stb_image/stb_image.c",
             # Lua 5.4 for the computer player's strategy scripts (docs/AI_LUA_PLAN.md §3)
             "lua": "ThirdParty/lua/ee_lua.c"}
# dmusic (DirectMusic reimplementation, src/ThirdParty/dmusic): every source (the POSIX thread file is empty on Windows)
_DMUSIC = sorted(p for p in (ROOT / "src" / "ThirdParty" / "dmusic").rglob("*.c"))
LIB_UNITS.update({"dmusic-" + p.relative_to(ROOT / "src" / "ThirdParty" / "dmusic").with_suffix("").as_posix()
                  .replace("/", "-"): p.relative_to(ROOT / "src").as_posix() for p in _DMUSIC})
LIB_MEMBERS = {"mixer": ["miniaudio"], "dx7hrtnl": ["pl_mpeg"], "exe": ["lua"], "lle": ["implode", "explode", "cgltf", "stb_image"] + sorted(
    n for n in LIB_UNITS if n.startswith("dmusic-"))}
LANGUAGE_RC = pathlib.PurePosixPath("Empire Earth/Language/Language.rc")
EXE_RC = ROOT / "src" / "Empire Earth" / "EE-AOC.rc"


def rel_src(path):
    p = pathlib.Path(path).resolve()
    if not p.is_relative_to(ROOT / "src"):
        raise SystemExit(f"{path} is not under src/")
    return pathlib.PurePosixPath(p.relative_to(ROOT / "src").as_posix())


def jobs_for(mod):
    """[(module, source, flags)] of a module's units (and the shared extra units it links)."""
    paths = [p for _, _, p in module_units(mod)] + [ROOT / "src" / rel for rel in EXTRA_UNITS.get(mod, [])]
    for p in paths:
        if not p.exists():
            sys.exit(f"{mod}: unit {p} does not exist")
    return [(mod, p, MODULE_FLAGS[mod]) for p in paths]


def lib_jobs(mods):
    """[("lib", source, flags)] of the library members the modules link."""
    return [("lib", ROOT / "src" / LIB_UNITS[n], MODULE_FLAGS[m]) for m in mods for n in LIB_MEMBERS.get(m, [])]


def obj_path(out, job):
    return pathlib.Path(out) / "obj" / job[0] / rel_src(job[1]).with_suffix(".obj")
