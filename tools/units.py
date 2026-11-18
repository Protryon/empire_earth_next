"""The build's modules, targets and compile jobs.

Units come from link/<module>/units.txt (in link order), or every .cpp of the module's source folder when the module
has no unit list; each compiles with its module's flags (ee.MODULE_FLAGS) or its own `// UNIT-FLAGS:` line. Library
code built from source (LIB_UNITS: pl_mpeg for the movies, Lua for the computer players)
is compiled with the modules that link it.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import LINK_DIR, MODULE_FLAGS, ROOT, module_units

# Every module is linked into the one exe (no DLLs): the game, the engine with libjpeg (the lobby's JPEG too), and the
# plug-ins (TCP protocol, mixer, Default random maps, the DX7 T&L renderer), registered in PIMModule.cpp.
MODULES = ["tcp", "mixer", "default", "lle", "jpeg", "dx7hrtnl", "won", "exe"]
TARGETS = ["exe"]
TARGET_MODULES = {"exe": ["exe", "won", "lle", "jpeg", "tcp", "mixer", "default", "dx7hrtnl"]}
PART_OF = {m: "exe" for m in MODULES if m != "exe"}
# units a module links besides its own: the platform layer's implementation (include/platform/, src/Platform/<os>/)
# with the engine, and new game units that have no address in the original (the db text loader, ...)
# (PlatConfig, PlatPath, PlatString, PlatTime, PlatMessage, PlatSim, PlatFile, PlatThread, PlatSocket, PlatWindow and
# PlatFont are Rust: rust/crates/ee_base/src/plat_core, ee_plat_sys, ee_plat_ui and ee_plat_sdl, linked through ee_rust)
PLATFORM_UNITS = [
                  # the fonts built in (PORT.md step 4.5; the C compiler's #embed, PlatFont itself is Rust)
                  "Platform/Common/PlatFontData.c"]
# the UI toolkit's C++ trampolines into Rust (ee_ui, docs/RUST_PLAN.md R6): the moved units' members live there
UI_TRAMPOLINES = ["Low-Level Engine/User Interface/UITrampolines.cpp"]
# the Graphics Engine's value types' out-of-line members, fifteen units folded into one (R4.3 part 16)
GE_UNITS = ["Low-Level Engine/Graphics Engine/GEValues.cpp"]
EXTRA_UNITS = {"lle": PLATFORM_UNITS + UI_TRAMPOLINES + GE_UNITS,
               "exe": ["Empire Earth/Database/EEDataTextFile.cpp", "Empire Earth/Database/EERules.cpp",
                       # the EE lobby glue's shells and hosts (R6 WONLobby round 6; ee_won eecolor.rs, eeglue.rs): the
                       # game's code, built as the GameCode units it replaced were
                       "Empire Earth/WONAPI/WONLobby/EELobbyGlueHost.cpp",
                       # the effects' constructors and hosts (docs/RUST_PLAN.md R4.4; ee_gfx effect_rising.rs, effect_2d.rs)
                       "Empire Earth/Graphics Thread/EEEffectHost.cpp",
                       # the host of the game's controls whose logic is Rust (ee_screens hud_ctl.rs; ee/EEUIRustControl.h)
                       "Empire Earth/User Interface Thread/EEUIRustControl.cpp",
                       # the computer-player decision trace and cheat switches (docs/AI_RESEARCH.md stage 0)
                       "Empire Earth/AI Thread/EECPTrace.cpp",
                       # the next computer player's tactics layer (docs/AI_LUA_PLAN.md §2, stage S1; ee/EECPNext.h)
                       "Empire Earth/AI Thread/EECPNext.cpp", "Empire Earth/AI Thread/EECPPlacement.cpp",
                       "Empire Earth/AI Thread/EECPReflexes.cpp",
                       # TEST ONLY: the placement and resource centres' capture (PLCE; ee/EECPPlaceCapture.h)
                       "Empire Earth/AI Thread/EECPPlaceCapture.cpp",
                       "Empire Earth/AI Thread/EECPNextView.cpp",
                       # its army: the groups (docs/AI_LUA_PLAN.md §5, stage S4)
                       "Empire Earth/AI Thread/EECPNextArmy.cpp",
                       # the computer player's engine per seat: classic or next (docs/AI_LUA_PLAN.md S0)
                       "Empire Earth/AI Thread/EECPEngine.cpp",
                       # engine helpers that lived in the combat units stage 5 deleted (docs/AI_RESEARCH.md §8.90)
                       "Empire Earth/AI Thread/EECPEngineHelpers.cpp",
                       # the computer player's Lua runtime: the strategy scripts' sandbox (docs/AI_LUA_PLAN.md §3)
                       "Empire Earth/AI Thread/EECPLua.cpp",
                       # the next engine's strategy: the personality's Lua script behind the verbs (§3.2)
                       "Empire Earth/AI Thread/EECPNextLua.cpp",
                       # the tile A* searches' view of the world (docs/RUST_PLAN.md R5b.1; ee/EEAIAStarView.h)
                       "Empire Earth/AI Thread/EEAIAStarView.cpp",
                       # TEST ONLY: the capture of the tile A* searches (EE_SIM_CAPTURE; ee/EEAIAStarCapture.h)
                       "Empire Earth/AI Thread/EEAIAStarCapture.cpp",
                       # TEST ONLY: the capture of the continent manager's analysis (ee/EEAIContinentCapture.h)
                       "Empire Earth/AI Thread/EEAIContinentCapture.cpp",
                       # the continent manager's regions in a saved game (save version 0.263; ee/EESaveGameFiles.h)
                       "Empire Earth/AI Thread/EEAIContinentSave.cpp",
                       # the start of the Rust library (ee_rust_init)
                       "Empire Earth/Empire Earth/EERust.cpp",
                       # its per-unit Lua queries and human-level commands (docs/AI_LUA_UNITS.md)
                       "Empire Earth/AI Thread/EECPNextUnits.cpp",
                       # the game log: every game, human and computer players (ee/EEGameLog.h)
                       "Empire Earth/Empire Earth/EEGameLog.cpp",
                       # TEST ONLY: the headless simulation's driver (EE-SIM.exe: build.py --sim links its entry)
                       "Empire Earth/Empire Earth/EESim.cpp",
                       # TEST ONLY: its scripted scenario editor (EE-SIM.exe --editor-script; ee/EESimEditor.h)
                       "Empire Earth/Empire Earth/EESimEditor.cpp",
                       # TEST ONLY: its civilization builder steps (the "cb" command; tools/sim_editor/civbuilder.txt)
                       "Empire Earth/Empire Earth/EESimCivBuilder.cpp",
                       # the hosts of the scenario editor's actions whole in Rust (EEARust kinds, ee/EEARust.h): the
                       # paints' ranges 0x1a830a..0x1a9112 (EESEAPaintRust; ee_screens editor_actions.rs), the
                       # deletion's 0x1a6b45..0x1a7331, the lasso's and the click's 0x1a7358..0x1a75e0; the editor's
                       # command control's 0x197261..0x197393 (the trigger tab's selection control's 0x197393..0x19764b
                       # is EEUICSelectUnits' Rust: ee/EESEUICTriggerSelectUnits.h)
                       "Empire Earth/Scenario Editor Actions/EESEARust.cpp",
                       # the host of the game's actions whole in Rust (ee/EEARust.h; ee_screens game_actions.rs): the
                       # Actions units' ranges 0x1000..0x52f3 but EEAWonNet's, EEAQuickSaveLoad's 0x1a5529..0x1a5881
                       # and EESEAQuickSaveLoad's 0x1aa70e..0x1aa723
                       "Empire Earth/Actions/EEARust.cpp",
                       # TEST ONLY: the simulation capture of the Rust leaves' gates (EE_SIM_CAPTURE; tools/sim_capture.py)
                       "Empire Earth/Empire Earth/EESimCapture.cpp",
                       # TEST ONLY: the app and world lane's capture records (ee/EEAppCapture.h: CAMU)
                       "Empire Earth/Empire Earth/EEAppCapture.cpp",
                       # TEST ONLY: its map, mini map, cheat and storm records and the --app-sweep (MAPS ... STRM)
                       "Empire Earth/Empire Earth/EEAppCaptureWorld.cpp",
                       # TEST ONLY: the tile sight's records (ee/EESightCapture.h: TSGT) and the app profile's end
                       # sweep of EEMap's sight and checksum (MAPS, MAPK)
                       "Empire Earth/Map/EESightCapture.cpp",
                       # TEST ONLY: the tech tree's, events' and calamities' capture records (ee/EETechCapture.h)
                       "Empire Earth/Tech Tree/EETechCapture.cpp",
                       # TEST ONLY: the units sweep of EE-SIM --unit-sweep (the capture's units profile)
                       "Empire Earth/Physics Thread/EEPhysicsSweep.cpp",
                       # TEST ONLY: the scenario manager's capture records (ee/EEScenarioCapture.h; RUST_PLAN R5b.5)
                       "Empire Earth/Scenario Manager/EEScenarioCapture.cpp",
                       # the scenario logic's host: the world's reads for ee_scenario (ee/EEScenarioHost.h; RUST_PLAN R5b.5)
                       "Empire Earth/Scenario Manager/EEScenarioHost.cpp",
                       # the host of the calamity classes' Rust updates (ee/EECalamityUpdate.h; ee_tech calamity_update.rs)
                       "Empire Earth/Calamities/EECalamityUpdate.cpp",
                       # the host of the calamity classes' Rust cast tests (ee_tech calamity_cast.rs)
                       "Empire Earth/Calamities/EECalamityCast.cpp",
                       # TEST ONLY: the calamity lane's capture records (ee/EECalamityCapture.h: CALI, CALK)
                       "Empire Earth/Calamities/EECalamityCapture.cpp",
                       # TEST ONLY: the area effects' capture records (ee/EEAECapture.h: AETP, AEFX)
                       "Empire Earth/Civilization Builder/EEAECapture.cpp",
                       # TEST ONLY: the civilization builder lane's capture records (ee/EECivCapture.h: CVMG, CVAP, CVLD, CVAF)
                       "Empire Earth/Civilization Builder/EECivCapture.cpp",
                       # TEST ONLY: the game messages' capture records (ee/EEMessageCapture.h; RUST_PLAN R5b.4)
                       "Empire Earth/Communications/EEMessageCapture.cpp",
                       # the game messages' class: EEMessage's codec, EEMRecord (split from EEMessage.cpp)
                       "Empire Earth/Communications/EEMRecord.cpp",
                       # the unit store's C++ side: the holders' reports, the insert, the check (ee/EEUnitStore.h)
                       "Empire Earth/Units/EEUnitStore.cpp",
                       # the world's layout for Rust's views (ee/EEWorldLayout.h; ee_sim world layout.rs)
                       "Empire Earth/Units/EEWorldLayout.cpp",
                       # TEST ONLY: the world views' check (ee/EEWorldLayout.h: VWCK)
                       "Empire Earth/Units/EEWorldViewCheck.cpp",
                       # the world calls of the Rust decisions: groups as ids, goals as handles (ee_units host.rs)
                       "Empire Earth/Units/EEUnitHost.cpp",
                       # the host of the unit classes' Update sequences (ee/EEUnitUpdate.h; ee_units update.rs)
                       "Empire Earth/Units/EEUnitUpdate.cpp",
                       # the unit classes' kinds (the EEUC* classes until R5b.4 round 32)
                       "Empire Earth/Units/EEUnitClassKinds.cpp",
                       "Empire Earth/Units/EEUnitClassAir.cpp",
                       "Empire Earth/Units/EEUnitClassBuildings.cpp",
                       "Empire Earth/Units/EEUnitClassTransports.cpp",
                       "Empire Earth/Units/EEUnitClassShots.cpp",
                       "Empire Earth/Units/EEUnitClassPeople.cpp",
                       "Empire Earth/Units/EEUnitClassGround.cpp",
                       # the goals whose class moved whole to Rust: EEUGPatrol's range 0x227de8..0x2283bc,
                       # EEUGCastCalamity's 0x220986..0x2213dd, EEUGExplore's 0x2213dd..0x221d98,
                       # EEUGUnloadTransport's 0x2283bc..0x228c4c, the load goals' 0x22585a..0x227621
                       # (ee/EEUGRust.h; ee_units rust_goal.rs)
                       "Empire Earth/Units/EEUGRust.cpp",
                       # the combat lane's goals on EEUGRust: EEUGAttackGround's and EEUGAttackUnit's ranges
                       # 0x21ce6c..0x21e0c1, EEUGHealUnit's 0x22528d..0x22585a (ee_combat goal_objects.rs,
                       # goal_attack.rs, goal_heal.rs)
                       "Empire Earth/Units/EEUGRustCombat.cpp",
                       # the goals' and shots' trigger tests and steps: their plain reads and CBTC/CBGS records
                       # (ee/EECombatGoals.h; ee_combat goals.rs)
                       "Empire Earth/Units/EECombatGoals.cpp",
                       # TEST ONLY: the group command sweep over the dry host (ee/EEUnitHost.h; USGC records)
                       "Empire Earth/Units/EEGroupCommandSweep.cpp",
                       # TEST ONLY: the active groups' sweep at a groups game's end (AGPL records; R5b.4 round 35)
                       "Empire Earth/Units/EEActiveGroupSweep.cpp",
                       # TEST ONLY: the POSIX test driver's in-process UI fixture (EE_TEST_UI; tools/harness/posix_game.py)
                       "Empire Earth/Empire Earth/EETestFixture.cpp",
                       # TEST ONLY: the fixture's orders (EE_TEST_UI; tools/harness/fixture_driver.py orders)
                       "Empire Earth/Empire Earth/EETestFixtureOrders.cpp",
                       # the render phase's snapshot of the world (ee/EERenderSnapshot.h; RUST_PLAN R5a.3)
                       "Empire Earth/Graphics Thread/EERenderSnapshot.cpp",
                       # TEST ONLY: RFIL, the render fill's capture (EE_RENDER_CAPTURE; ee/EERenderCapture.h, tools/rfil.py)
                       "Empire Earth/Graphics Thread/EERenderCapture.cpp",
                       # the host of the screens that are Rust's whole: their event class and toolkit and game calls
                       # (ee/EEScreenEvent.h; ee_screens menus.rs: the main menu, the editors' menu, single player, play campaign;
                       # settings_form.rs: the game settings; custom_campaign.rs: the custom campaign form)
                       "Empire Earth/UI Events/EEScreens.cpp",
                       # a unit with no code range of its own (every member was ICF-folded into another unit's)
                       "Empire Earth/Calamities/EEGlobalCalamityClass.cpp"]}


def _won_extra_units():
    """The WON library's units that no unit list names (link/won/units.txt lists the ones with an original address;
    the lobby glue, WONLobby/GameCode and EELobbyGlueHost.cpp, is the exe's): every other .cpp under src/Empire Earth/WONAPI."""
    listed = {p.resolve() for mod in ("won", "exe") for _, _, p in module_units(mod)}
    # the exe's own units under WONAPI (the lobby glue's hosts)
    listed |= {(ROOT / "src" / rel).resolve() for rel in EXTRA_UNITS["exe"]}
    won = ROOT / "src" / "Empire Earth" / "WONAPI"
    return [p.relative_to(ROOT / "src").as_posix() for p in sorted(won.rglob("*.cpp"))
            if p.resolve() not in listed and p.name not in WON_NOT_LINKED]


# WONAPI sources not linked: none (the GameSpy query/report support, WON SDK code the original exe did not contain
# and nothing references, is ee_won gamespy.rs since net round 28; its class is header-only). The WON GUI toolkit's
# Win32/GDI backend (MS*.cpp, DisplayContext, Clipboard), the Win32 open/save file dialog and WinSkin are deleted:
# the backend is WONGUI/Plat*.cpp on the platform layer on every OS. The Winsock event-driven socket thread backends
# (SocketThreadWinsock2, SocketThreadEx) are deleted: the game runs WON's polling SocketThreadSimple
WON_NOT_LINKED = set()


EXTRA_UNITS["won"] = _won_extra_units()
LIB_UNITS = {"pl_mpeg": "ThirdParty/pl_mpeg/pl_mpeg.c",
             # Lua 5.4 for the computer player's strategy scripts (docs/AI_LUA_PLAN.md §3)
             "lua": "ThirdParty/lua/ee_lua.c"}
# (the sound device, the decoders and the music are Rust: rust/crates/ee_audio, on cpal, symphonia and rustysynth)
LIB_MEMBERS = {"dx7hrtnl": ["pl_mpeg"], "exe": ["lua"]}
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
