# Lane: game headers (include/ee's headers no other lane owns)

## Round 1 (branch rust-headers1, 2026-11-24): ids and generated headers as Rust's, the one-statement members inline

State: done; target (-1,000 logic lines in the lane's headers) met. The lane's headers (include/ee without the
EEUnit*, EEPlayer*, EETech*, EECP*, EECiv*, EEUI* and EEWon* families; `build/hd1/list.py`) went from 4,647 to
3,265 logic lines (-1,382), 199 to 196 headers; 8 `.cpp` units are deleted (367 to 359).

### What moved

- **Ids as Rust enums** (hd1a): the random map, effect, sound, improvement, save tag, area effect, resource,
  storm, AI kind, terrain, calamity and damage form ids are `#[repr(C)]` enums in ee_sim `rmap_ids.rs` and
  `game_ids.rs`, emitted back through cbindgen (`/// cbindgen:enum-class=false`, the evidence comments as doc
  comments). The old headers keep their names and include `rust/sim/rmap_ids.h` / `rust/sim/game_ids.h`; the
  readers of the old enumerators moved with them (`tools/eedata/rules.py`, the `cpp_consts` tests, ee_sim's
  `header_consts` test).
- **eedata's generated C++ headers** (hd1a): `EERulesGen.h` is gone; the rules documents' records and enums are
  ee_data `rules_gen.rs` through cbindgen, and the dbworld constants are ee_data `world_constants.rs`
  (`EEWorldConstants.h` keeps one inline helper). `eedata.py cppgen`/`check` read and check the Rust side.
- **The dbcalamity record** is Rust's `EeCalamityData` (ee_sim `tech/calamity_class.rs`, a size assert for
  specgen); `EECalamityClass.h` keeps the name as a typedef; `tools/eedata/overlay.py` points at the Rust struct.
- **The combat formulas' captured calls** (hd1b): the capture record (tag, input, chain, output) is written by
  ee_sim `combat/captured.rs`; `EECombatFormulas.h`'s wrappers are one call each.
- **The game actions' kinds** are Rust's `EE_SCREENS_GA_*` (EEARust.h's `kEEA*` enum gone).
- **One-statement members inline**: about 600 out-of-line definitions whose body is one statement (mostly one
  `ee_*` call) are now the inline bodies of their declarations (a tool, `build/hd1/tools-a/inl.py`: scan, apply,
  revert, finalize). The declaration line stops counting as logic and the `.cpp` shim goes. Bodies needing a type
  the header only forward-declares stay out of line (`deny.txt`: e.g. `vector<EETeam>` in EEGameState and
  EEWorld, EECameraControl's `vector<EECameraKey>`, EETeam's EEVCWonder).
- **Emptied units deleted**: EEButtonObject.cpp, EESMCondition.cpp, EESMMediaStrings.cpp, EECheatCodes.cpp,
  EEROEffects.cpp, EEPMultiplayerHelper.cpp, EEVCWonder.cpp, EECalamityClass.cpp (`link/exe/units.txt`);
  EEEffectMesh.h too (its store is the effect host's, its one caller).
- **Dead declarations**: unused and repeated forward declarations; EECreateGameTimer, gContinentManagerClearing,
  gLoadingPlayerMedia, gSavedMainGameForm's extern, EESMDialogMessage's defaulted copies.

Per header (logic before -> after), the biggest: EERulesGen 179 -> 0, EERandomMapIDs 123 -> 0, EECombatFormulas
136 -> 39, EEMap 174 -> 108, EEWorld 136 -> 92, EEPhysics 92 -> 51, EECalamityClass 84 -> 19, EEScenarioManager
50 -> 25, EECampaignFormManager 48 -> 24, EECameraControl 39 -> 13, EEMiniMap 86 -> 68.

### Left

- EEMessageID.h (109): game core 13 (batch 25) makes the message ids Rust enums; merge it, don't redo it.
- EmpireEarth.h (109) and EEARust.h (89) are real declarations: globals and free functions. Cutting them means
  moving the globals into Rust (callers read the generated accessors), not deleting dead ones.
- EEMap.h and EEWorld.h are mostly data members now; EEWorld's are read by Rust through an offset table (gamecpp
  `world.rs` `O::W*`), a job for the world's owner. The RTSMap members behind the `.cpp`-local EEMapRaiseOps host,
  EETerrainTypeTable::LoadDamageTables (a template body) and about 27 candidates blocked by `.cpp`-local helpers
  (EEClient's EESession, RTSMouseFrameCall, EE_AMBIENT_HOST, gPhysics, gMediaTable) stay out of line.
- EEComputerPlayer.h, EETown.h, EEHerd.h and RTSUnit.h were left to their lanes (the same pass applies).

### Gates

| Gate | Result |
|---|---|
| `build.py -j 10 --sim`, `--test-profile windowed-software` | 0 unresolved, 0 duplicates (EE-AOC, EE-SIM; test 402 inputs) |
| `rust_build.py header` / `gate` | current / PASS |
| oracle seeds 2-7 | identical to ref (before and after the harness hooks fix) |
| the 12 capture profiles | PASS each (oracle, late, space, calamity, units, effects, missile, app, scenario, groups, placement, transport) |
| saveload plain (seeds 2-7 at 600, 1350, 2100 s) / matrix (`--settings all --seeds 2-3 --at 1000,1900 --expect`) | 18/18 identical / PASS |
| `scenario_check.py` | 27/27 |
| `sim_editor.py`, `--civbuilder` | PASS (14,448 lines), PASS (4,062 lines) |
| `eedata.py check`, `cppgate` | PASS, PASS |
| tour, `random-game --box-select --orders --seed 7`, editor (slot.py) | PASS, PASS, PASS |
| `gates.sh` | GATES PASS: tour; ai-game time-cap, units [4, 38, 38], alive_computers [2, 3], engines classic_lua x 2; lan-game |

All of the above ran on the tip merged with batch 25 (e88c6d339; ai-game units [4, 38, 38], test build 387 inputs).

port_metrics, batch 25 (04ccdbcf3) -> the merged tip: cpp_lines 109,934 -> 106,330 (-3,604), cpp_files 354 -> 346
(-8), logic 17,218 -> 15,867 (-1,351), classes 810 -> 792 (-18), extern_c +14, rust_hdrs +4, unsafe +34,
layout -14. The lane's headers after batch 25: 3,078 logic lines (list.py).

The test profile's harness hooks (tools/build.py `HARNESS_HOOKS`) were inlined by the pass and broke only the
test build: EEWorld::RemovePlayerFromGame, EEPhysics::Tick and ReportStatistics are out of line again.

## Round 2 (branch rust-headers2, 2026-11-25): the paused lanes' headers, stale includes, uncalled members

State: done; target (-3,000 C++ lines) met by removals alone. Base: the round 1 tip merged with batch 26
(4930a8c9d). port_metrics 4930a8c9d -> the merged tip: cpp_lines 103,917 -> 96,936 (**-6,981**), compiled files
330 -> 325 (-5), logic 14,509 -> 14,289 (-220), code -3,064, shim -2,844; classes, extern_c, rust_hdrs, unsafe and
layout unchanged.

| Kind | lead | hd2a (units) | hd2b (AI, scenario, UI, lle) | total |
|---|---|---|---|---|
| one-statement members inline (inl.py) | -374 (77 members, EEPPlayerHelper.cpp gone) | -205 (31) | about -480 (77; EEUIEOpenCivBuilder.cpp gone) | about -1,060 |
| uncalled members and declarations | | -135 (88 members) | about -95 (ee) and -126 (about 70 lle) | about -355 |
| stale project includes (trim.py) | -736 (74 units) | -435 (30 units) | -1,352 (89 units; UICheckBox.cpp, UIRadioButton.cpp, User Interface.cpp gone) | -2,523 |
| line joins (formatting only: compact.py, casejoin.py) | | -1,065 | -1,982 | -3,047 |

Removals: about -3,934; formatting: -3,047. Commits: lead 51d05f4d4 (inline, world-side headers), 4763b4234 (calamity
includes), ed8d8f235 (includes); hd2a 8550ee9f3, 9348dae9d, e07e2d3ce, 6dd145240; hd2b 81d8ff25f, 18a63d369,
e33aa3d74, 68ab393f8, 57e8ec73d.

### What moved

- **The inline pass over the paused lanes' headers** (units: EEUnit.h, EEUnitAction.h, EEUnitClassImprovement.h,
  EEUnitPicker.h, RTSUnit.h; the AI: EEComputerPlayer, EETown, EEHerd, EEAIArea, EEAIContinentManager, EEAIForest,
  EEAIRectangleArea; the editor: EEUIFScenarioEditor, EEUIFSETerrain, EEUIFSEBrushGroup; EEUIFMGMainGroup) and over
  the world-side headers round 1's skip list had left (EEAmbientSoundManager, EEAnimal, EECampaignProgress,
  EEContagionFunctor/Manager, EEEMStorm, EEFormation, EEGraphicEffect, EEPCalamityHelper, EEPPlayerHelper, EEROUnits,
  EERules, EEEnvironmentManager, EEMiniMap, EECameraControl, RTSGraphics). Blocked ones unblocked by hand:
  EECombatShotStep is declared in EEUnit.h (the five shot steps inline); EEHerd/EETown/EEAIRectangleArea
  GetRandomPoint unpack the Rust point in the body; EEHerd's Load/Save see EESaveFilePrimitive.
- **Uncalled members**: names whose only mention in src, include, rust/crates, tools and tests is their own
  declaration (non-virtual only: a virtual's slot is layout). uncalled.py skips uses inside the declaring header, so
  the forks counted repo-wide instead.
- **Stale includes**: trim.py removes each project `#include "..."` of a unit, last first, and keeps the removal
  only if the unit still compiles (clang-cl /Zs, the unit's own flags). `<...>` headers stay; hd2b also skipped
  includes inside `#if` blocks, and the lead kept the rust/ and platform/ headers of Empire Earth.cpp (its POSIX
  `main` uses them). No dropped header defines a macro its unit tests in `#if` (checked).
- **Emptied units deleted**: EEPPlayerHelper.cpp, EEUIEOpenCivBuilder.cpp, UICheckBox.cpp, UIRadioButton.cpp,
  User Interface.cpp (UIInitialize inline in lle/User Interface.h), with their link/*/units.txt lines.

### Left

- Members whose body needs a type the header only forward-declares (about 30: ~EEComplexUnit, EEComplexUnit(),
  GetQueueGoals, SetRange, SetMaxVelocity, 10 EEComputerPlayer members casting EEComplexUnit*, EEUIFMGStatusButton's 4
  (EEUIFMainGame.h), EEUIManager's (EEUIManagerCall lives in the .cpp), EEGraphicEffect's Rust calls declared in its
  .cpp, EEContagionFunctor's dtor, EEPhysics's dtor (TSScheduler)). Out-of-class `inline` definitions at the end of
  a header would save about 3 lines each.
- EmpireEarth.h's and EEARust.h's globals: their remaining C++ users are ee_screens game_cpp.rs shims behind the
  Cpp trait with Fake tests; each move saves 1-2 C++ lines and costs a test edit. Not done.
- Host-case collapses (Rust calling Rust where a C++ case only forwards): about 10 pure ones in the units' hosts
  (EE_COMBAT_H_GOAL_TARGET, EE_UNITS_UCK_CONDITION, EE_UNITS_UC_RECORD_RELATED, the EE_UNITS_UCB_* ring family,
  EE_UNITS_CU_CAN_STAND_ON); hd2b's collapse_scan lists about 1,000 one-call ee_*cpp_* shims needing a stricter filter.
- EESaveActiveGroups/EELoadActiveGroups have no caller (declared in EESaveGameFiles.h).
- The include trim of the headers themselves (each trial is a full build).
- A POSIX game build to confirm the include trim (only Windows clang-cl checked it).

### Gates (on 50072c064, batch 26 in; the reduced lane set)

| Gate | Result |
|---|---|
| `build.py -j 10 --sim`, `--test-profile windowed-software` | 0 unresolved, 0 duplicates (test 366 inputs) |
| `rust_build.py header` / `gate` | current / PASS |
| oracle seeds 2-7 | identical |
| captures oracle, late, calamity, units, effects, scenario, groups | PASS each |
| saveload matrix (`--settings all --seeds 2-3 --at 1000,1900 --expect`) | PASS |
| `scenario_check.py` | 27/27 |
| `sim_editor.py`, `--civbuilder` | PASS (14,448), PASS (4,062) |
| tour, `random-game --box-select --orders --seed 7`, editor | PASS, PASS, PASS |
| `won_host_coverage.py`, `xbuild.py wontest` | PASS, PASS (183 drivers) |
| `gates.sh` | GATES PASS: ai-game time-cap units [4, 34, 36], alive_computers [2, 3], classic_lua x 2; lan-game |

### After batch 28 (merged 4a88b155a, fixes cf9303955, origin/rust 6cd0018bc)

The merge conflicted in about 85 files, nearly all include lines: an include-only hunk took batch 28's lines less
the ones this round dropped; code hunks took batch 28's bodies (or none where a body is inline here). 21 units then
lacked includes the new bodies need: their includes were restored in batch 28's order and trimmed again.
port_metrics 8eba20040 -> the tip: cpp_lines 96,416 -> 90,211 (-6,205), files 295 -> 290 (-5), logic -220.
Regate (reduced set): both builds 0/0 (test 331 inputs), headers current, Rust gate PASS, oracle seeds 2-7
identical, captures oracle, late, units, scenario PASS, saveload matrix PASS, scenario_check 27/27, sim_editor and
`--civbuilder` PASS, editor tour PASS, won_host_coverage PASS, wontest PASS (185 drivers), gates.sh GATES PASS
(ai-game units [4, 36, 38], alive_computers [2, 3], classic_lua x 2; lan-game).

## Round 3 (branch rust-headers3, 2026-11-26): the string ids as Rust's, dead declarations, header includes

State: done; target (-3,000 C++ lines) met by removals alone, no line joins. Base: batch 29 (47e9ad712, which has
round 2). port_metrics 47e9ad712 -> the tip: cpp_lines 88,934 -> 83,943 (**-4,991**), compiled files 269 -> 268,
code -635, shim -548, logic 12,147 -> 12,060 (-87), classes 724 -> 717; extern_c, rust_hdrs, unsafe and layout
unchanged; rust_lines +3,794 (the string table).

| Kind | lines | commits |
|---|---|---|
| EEStringIDs.h: 3,765 ids -> rust/crates/ee_base/src/string_ids.rs; the header keeps the 164 the C++ uses | -3,601 | 841be00ca |
| dead declarations (fork hd3a: names nothing else mentions, inline members no object emits, out-of-line functions no relocation names) | -1,127 | 1b4b29f5c .. 8f437116f (11) |
| headers nothing includes (EEAIAStarView, EEAutoPathRequest, EECPExperimentTimers, EEEffectKindIDs, EERenderCapture, EEStormType) | -98 | db5e028e0 |
| header includes (htrim.py) and the six headers that left empty (EEAIKindIDs, EEAreaEffectKindIDs, EECivPowerIDs, EEImprovementIDs, EEUIEChatLogEvents, EEUnitClassIDs) | -162 | 5d61f45dd .. e16793be3 |
| EESaveActiveGroups/EELoadActiveGroups declarations | -3 | 072520cdc |

### What moved

- **The string ids**: `ee_base::string_ids` holds the whole table (`pub const IDS_...: u32`, `ALL`, `by_name`) under
  the names the header had; its test checks every `#define IDS_` left in `ee/EEStringIDs.h` equals it. The header
  keeps what C++ (src, include, tests) names. The Rust tests that parsed the header for their private ids use
  `by_name`, or read the .rs file (its `NAME = value,` lines parse like enumerators); tools/eedata rules.py and
  rulesgen.py read the .rs.
- **Dead declarations** (fork a): a scan of header names nobody else mentions, then two object scans of an `-O0`
  build of both profiles: in-class inline members that no object emits, and out-of-line functions no relocation
  names. Virtuals, constructors, the WON headers and what the harness names stay. Five headers no one included went
  with RTSVisibleMap.cpp (gMapTileSize moved to RTSMap.cpp).
- **Header includes**: build/hd3/htrim.py drops a header's `#include "..."` when every exe unit that includes the
  header (per-unit include sets from `/showIncludes`) still compiles (/Zs) and the header still compiles alone after
  the units' prelude (the first user's `<...>` includes and lle/LowLevelEngine.h). Headers that do not compile alone
  are left as they are (73 of 198), the WON headers too. No dropped header defines a macro anything tests in `#if`;
  none of the changed headers has a platform branch. The list is build/hd3/TRIMMED.txt (64 files).

### Left

- The members whose body needs a type the header only forward-declares (round 2's list): each needs EEUnit.h in an
  AI or UI header, a new edge into a hot header; out-of-class inline definitions would save only comment lines.
- The host-case collapses (EE_COMBAT_H_GOAL_TARGET, EE_UNITS_UCK_CONDITION, ...): about one C++ line each against a
  Rust host trait and its fake-host tests.
- Another `-O0` object pass (the next layer of dead code); the member-template and virtual-name cases fork a left;
  the Rust `ee_gamecpp_*` exports whose C++ wrappers went.
- The 73 headers that do not compile alone: making them self-contained first would let the trim reach them.
- A POSIX build of the trimmed headers (build/hd3/TRIMMED.txt); only Windows clang-cl checked them.

### Gates (on e16793be3, batch 29 in; the reduced lane set)

| Gate | Result |
|---|---|
| `build.py -j 10 --sim`, `--test-profile windowed-software` | 0 unresolved, 0 duplicates (EE-AOC 309, EE-SIM 310, test 309 inputs) |
| `rust_build.py header` / `gate` | current (367 headers) / PASS |
| oracle seeds 2-7 (against batch 29's b29q) | identical |
| captures oracle, late, units, scenario, calamity, effects, groups | PASS each (598, 598, 510, 113, 623, 18, 6 digests) |
| saveload matrix (`--settings oracle,calamity,late,space,next,plains-medium-3ai --seeds 2 --at 1000,1900 --expect`) | PASS |
| `scenario_check.py` | 27/27 |
| `sim_editor.py`, `--civbuilder` | PASS (14,448), PASS (4,062) |
| `eedata.py check`, `cppgate` | PASS, PASS |
| editor tour, `random-game --box-select --orders --seed 7` | PASS, PASS (BOX SELECT, ORDERS) |
| `won_host_coverage.py`, `xbuild.py wontest` | PASS, exit 0 (185 drivers) |
| `gates.sh` | GATES PASS: ai-game time-cap units [4, 36, 37], alive_computers [2, 3], classic_lua x 2; lan-game |

Captures run: oracle and late (always); units, scenario, calamity, effects and groups because the dead
declarations and include trims touched their code (EEActiveGroup.cpp, EEContagionManager.cpp, the scenario and
effect headers). Not run: space, missile, app, placement, transport; plain saveload; a POSIX build.
