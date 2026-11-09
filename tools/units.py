"""The build's modules, targets and compile jobs.

Units come from link/<module>/units.txt (in link order), or every .cpp of the module's source folder when the module
has no unit list; each compiles with its module's flags (ee.MODULE_FLAGS) or its own `// UNIT-FLAGS:` line. Library
code built from source (LIB_UNITS: dmusic for the engine, miniaudio for the mixer)
is compiled with the modules that link it.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import LINK_DIR, MODULE_FLAGS, ROOT, module_units

# Every module is linked into the one exe (no DLLs): the game, its image libraries, the engine with libjpeg, and the
# plug-ins (TCP protocol, mixer, Default random maps, the DX7 T&L renderer), registered in PIMModule.cpp.
MODULES = ["tcp", "mixer", "default", "lle", "jpeg", "dx7hrtnl", "exeimg", "won", "exe"]
TARGETS = ["exe"]
TARGET_MODULES = {"exe": ["exe", "exeimg", "won", "lle", "jpeg", "tcp", "mixer", "default", "dx7hrtnl"]}
PART_OF = {m: "exe" for m in MODULES if m != "exe"}
# units a module links besides its own: the platform layer's implementation (include/platform/, src/Platform/<os>/)
# with the engine, the open model/texture formats (glTF, PNG: tools/eemodel) with the engine, and new game units that
# have no address in the original (the db text loader, ...)
# (PlatConfig, PlatPath, PlatString, PlatTime, PlatMessage, PlatSim, PlatFile, PlatThread, PlatSocket, PlatWindow and
# PlatFont are Rust: rust/crates/ee_plat_core, ee_plat_sys, ee_plat_ui and ee_plat_sdl, linked through ee_rust)
PLATFORM_UNITS = [
                  # the fonts built in (PORT.md step 4.5; the C compiler's #embed, PlatFont itself is Rust)
                  "Platform/Common/PlatFontData.c"]
OPEN_FORMAT_UNITS = ["Low-Level Engine/Graphics Engine/GEOpenFormatsFile.cpp"]   # the converters are Rust (ee_openformats)
# the DX7 renderer's C++ trampolines into Rust (ee_dx7, docs/RUST_PLAN.md R4.2): the classes' deleted bodies live there
DX7_TRAMPOLINES = ["Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/Common/DX7Trampolines.cpp",
                   "Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/Common/DX7DrawSeam.cpp"]
# the UI toolkit's C++ trampolines into Rust (ee_ui, docs/RUST_PLAN.md R6): the moved units' members live there
UI_TRAMPOLINES = ["Low-Level Engine/User Interface/UITrampolines.cpp"]
EXTRA_UNITS = {"lle": PLATFORM_UNITS + OPEN_FORMAT_UNITS + UI_TRAMPOLINES,
               "dx7hrtnl": DX7_TRAMPOLINES,
               "exe": ["Empire Earth/Database/EEDataTextFile.cpp", "Empire Earth/Database/EERules.cpp",
                       # the computer-player decision trace and cheat switches (docs/AI_RESEARCH.md stage 0)
                       "Empire Earth/AI Thread/EECPTrace.cpp",
                       # the next computer player's tactics layer (docs/AI_LUA_PLAN.md §2, stage S1; ee/EECPNext.h)
                       "Empire Earth/AI Thread/EECPNext.cpp", "Empire Earth/AI Thread/EECPNextTactics.cpp",
                       "Empire Earth/AI Thread/EECPNextView.cpp",
                       # its army: the groups (docs/AI_LUA_PLAN.md §5, stage S4)
                       "Empire Earth/AI Thread/EECPNextArmy.cpp",
                       # TEST ONLY: the next engine's C++ test opening (stage S1's proof of the verbs)
                       "Empire Earth/AI Thread/EECPNextTest.cpp",
                       # the computer player's engine per seat: classic or next (docs/AI_LUA_PLAN.md S0)
                       "Empire Earth/AI Thread/EECPEngine.cpp",
                       # the computer player's Lua runtime: the strategy scripts' sandbox (docs/AI_LUA_PLAN.md §3)
                       "Empire Earth/AI Thread/EECPLua.cpp",
                       # the next engine's strategy: the personality's Lua script behind the verbs (§3.2)
                       "Empire Earth/AI Thread/EECPNextLua.cpp",
                       # the tile A* searches' view of the world (docs/RUST_PLAN.md R5b.1; ee/EEAIAStarView.h)
                       "Empire Earth/AI Thread/EEAIAStarView.cpp",
                       # TEST ONLY: the capture of the tile A* searches (EE_SIM_CAPTURE; ee/EEAIAStarCapture.h)
                       "Empire Earth/AI Thread/EEAIAStarCapture.cpp",
                       # the start of the Rust library (ee_rust_init)
                       "Empire Earth/Empire Earth/EERust.cpp",
                       # the game log: every game, human and computer players (ee/EEGameLog.h)
                       "Empire Earth/Empire Earth/EEGameLog.cpp",
                       # TEST ONLY: the headless simulation's driver (EE-SIM.exe: build.py --sim links its entry)
                       "Empire Earth/Empire Earth/EESim.cpp",
                       # TEST ONLY: the simulation capture of the Rust leaves' gates (EE_SIM_CAPTURE; tools/sim_capture.py)
                       "Empire Earth/Empire Earth/EESimCapture.cpp",
                       # TEST ONLY: the tech tree's, events' and calamities' capture records (ee/EETechCapture.h)
                       "Empire Earth/Tech Tree/EETechCapture.cpp",
                       # TEST ONLY: the POSIX test driver's in-process UI fixture (EE_TEST_UI; tools/harness/posix_game.py)
                       "Empire Earth/Empire Earth/EETestFixture.cpp",
                       # the language strings, generated from Language.rc (tools/lang_strings.py)
                       "Empire Earth/Language/LanguageStrings.cpp",
                       # a unit with no code range of its own (every member was ICF-folded into another unit's)
                       "Empire Earth/Calamities/EEGlobalCalamityClass.cpp"]}


def _won_extra_units():
    """The WON library's units that no unit list names (link/won/units.txt lists the ones with an original address;
    the lobby glue, WONLobby/GameCode, is in the exe's list): every other .cpp under src/Empire Earth/WONAPI."""
    listed = {p.resolve() for mod in ("won", "exe") for _, _, p in module_units(mod)}
    won = ROOT / "src" / "Empire Earth" / "WONAPI"
    return [p.relative_to(ROOT / "src").as_posix() for p in sorted(won.rglob("*.cpp"))
            if p.resolve() not in listed and p.name not in WON_NOT_LINKED]


# the WON GUI toolkit's Win32/GDI backend (link/won/units.txt lists it): replaced on every OS by the backend on the
# platform layer (WONGUI/Plat*.cpp, include/ee/wonapi/WONLobbyPlatform.h; PORT_LEARNINGS.md "The WON library from
# source")
WON_REPLACED = {"MSWindowManager.cpp", "MSWindow.cpp", "MSGraphics.cpp", "MSNativeImage.cpp", "MSRawImage.cpp",
                "MSFont.cpp", "DisplayContext.cpp", "BMPDecoderPlatform.cpp", "Clipboard.cpp", "MSSound.cpp"}
# WONAPI sources not linked: a reconstruction whose every function the original unit StringParser.cpp already defines
# (same bodies; linking both is a duplicate symbol); WON SDK sources the original exe did not contain and nothing
# references (the GameSpy query/report support, the Win32 open/save file dialog, the Winsock event-driven socket
# thread backends: the game runs WON's polling SocketThreadSimple; WinSkin, the Windows desktop-theme reader)
WON_NOT_LINKED = {"StringParserFormat.cpp", "GameSpySupport.cpp", "MSFileDialog.cpp", "SocketThreadWinsock2.cpp",
                  "SocketThreadEx_Windows.cpp", "WinSkin.cpp"}


EXTRA_UNITS["won"] = _won_extra_units()
LIB_UNITS = {"miniaudio": "ThirdParty/miniaudio/miniaudio.c", "pl_mpeg": "ThirdParty/pl_mpeg/pl_mpeg.c",
             # Lua 5.4 for the computer player's strategy scripts (docs/AI_LUA_PLAN.md §3)
             "lua": "ThirdParty/lua/ee_lua.c"}
# dmusic (DirectMusic reimplementation, src/ThirdParty/dmusic): every source (the POSIX thread file is empty on Windows)
_DMUSIC = sorted(p for p in (ROOT / "src" / "ThirdParty" / "dmusic").rglob("*.c"))
LIB_UNITS.update({"dmusic-" + p.relative_to(ROOT / "src" / "ThirdParty" / "dmusic").with_suffix("").as_posix()
                  .replace("/", "-"): p.relative_to(ROOT / "src").as_posix() for p in _DMUSIC})
LIB_MEMBERS = {"mixer": ["miniaudio"], "dx7hrtnl": ["pl_mpeg"], "exe": ["lua"], "lle": sorted(
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
    if mod == "won":
        paths = [p for p in paths if p.name not in WON_REPLACED]
    for p in paths:
        if not p.exists():
            sys.exit(f"{mod}: unit {p} does not exist")
    return [(mod, p, MODULE_FLAGS[mod]) for p in paths]


def lib_jobs(mods):
    """[("lib", source, flags)] of the library members the modules link."""
    return [("lib", ROOT / "src" / LIB_UNITS[n], MODULE_FLAGS[m]) for m in mods for n in LIB_MEMBERS.get(m, [])]


def obj_path(out, job):
    return pathlib.Path(out) / "obj" / job[0] / rel_src(job[1]).with_suffix(".obj")
