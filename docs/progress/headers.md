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
