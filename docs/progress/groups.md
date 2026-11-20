# The groups lane: the unit groups as Rust objects, EEPlayer, EEUnitClassUpgrade and EEUnitClassImprovement

The lane's state (one file per lane; each round's story is its PORT_LEARNINGS.md section: "The groups lane, round 1",
"The groups lane, round 2"). Round 1 (carved out of the Units and Game core lanes): the group classes, `ee_units_ag_host`,
EEUnitClassUpgrade, the town and herd offsets of EEWorldLayout.cpp and the EEPlayer.cpp parts that collapse with them.
Round 2: all of src/Empire Earth/Player and include/ee/EEPlayer*.h, the groups, EEUnitClassUpgrade and
EEUnitClassImprovement (not the towns' and herds' hosts: AI round 7; not the path slice: the Paths lane). Rust home:
`ee_sim::units` group_obj.rs (the group objects and the player's group lists), group_host.rs (the groups' game host),
selection.rs, upgrade_media.rs, player_body.rs (EEPlayer's bodies), player_split.rs, player_save_split.rs and
player_tick.rs's `split` (the player hosts' compound cases), improvement.rs (EEUnitClassImprovement), ag_host.rs (the
active groups' game host), group_obj_tests.rs.

## Round 1 (branch `rust-groups1`, from ef45e89ba: game core round 5 merged with rust-batch9, 2026-11-20)

| Commit | What | Net C++ |
|---|---|---|
| 16c63f042 | the groups as Rust objects: one allocation headed by `EeUnitsGroup` (what the C++ reads in place: the player, the formation, the kind flags, a selection's saved group, change time and sort flag, a hot-key group's index, an active group's goal and record `mFields`), the rest Rust's (the units, the type counts, the path requests, the destinations, the two point lists); the four C++ classes name it (no data, every member one call; the virtuals dispatch on `mGroupKind`); EEGroupPathRequest and EEGroupCluster are Rust structs; MakeClusters, MoveAlong, the sorters (MSVC's `std::sort`), ShowSelectionAreas, the saves (0.279's active groups too) in Rust; EESelectedGroup.cpp, EESavedGroup.cpp, EEGroupPathRequest.h, EEGroupCluster.h, EEActiveGroup.h deleted; EEGroup.cpp is the bodies' C++ host (`ee_units_group_host`); the units host's 21 group functions (EEUnitHost.cpp) and the ag host's group cases call the objects | -1,758 |
| 04229ccfc | EEPlayer's group parts (destructor, Reset, Clear, RemovalsDone, the tick's validation) as `ee_units_groups_player`; GetActiveGroupOf inline; the ag host's 17 goal cases (clone, delete, the location, unload, attack, heal, explore and gather accessors, the unload and attack constructions) and its reference and angle cases in Rust | -126 |
| d26448d06 | EEPlayer's selection members (UpdateSelection, TrySelectUnit, SelectUnit, SelectUnits, AddUnitToSelectedGroup, AddUnitsToSelectedGroup, AddAndRemoveUnitsFromSelectedGroup, RemoveUnitFromSelectedGroup, DeselectUnit, PlayAcknowledgement) and RefreshUpgrades in Rust (selection.rs); EEUnitClassUpgrade's media bodies (LoadMedia, UnloadMedia, LoadButtons, UnloadButtons, the six setters, the destructor's unloads, the two sounds' round robins) in Rust (upgrade_media.rs) | -237 |

Round total (src and include, the generated include/rust aside): 2,585 C++ lines deleted, 464 added, net -2,121
(target 3,000). The added lines are the one-statement shims (the group host's ~60 cases, the upgrade host's 17), the
inline handles of ee/EEPlayer.h and ee/EESavedGroup.h (one call each) and comments; the logic deleted is the four
group classes, EEGroupPathRequest, EEGroupCluster, the active groups' save, EEPlayer's group, selection and
RefreshUpgrades bodies, the units host's 21 group functions, the ag host's 30 cases and EEUnitClassUpgrade's media
bodies (the "calling C++ from Rust is fine" rule came mid-round: the third commit follows it).

### What moved wholly

- `EEGroup` (all of it: Add, Remove, Contains, GetUnit, GetNumUnits, Load, UpdateKindFlags, Clear, RemoveUnit,
  RepairPointers, Save, MoveAlong, MoveAlongFoundPaths, GetBounds, CountUnitsOfKind, CopyUnits, CopyGroup,
  FindUnitOfCategory, FindUnitWithFlags, FindPathRequest, RequestPath, CancelPathRequest, RemoveAt, MakeClusters,
  the type counts, GetFlags' body), `EESavedGroup` (Add, RemoveUnit, Validate, Assign), `EESelectedGroup`
  (CopySelection, SelectSavedGroup, Add, AddIfAbsent, AddUnit, Load, Save, Clear, RemoveUnit, RepairPointers,
  SortUnits with EESGUnitSorter, Validate, ShowSelectionAreas, RefreshSelectionAreas, GetContinent),
  `EEActiveGroup`'s own C++ (the constructor and destructor, AddMember, RequestPaths, RequestAllPaths, Fill with
  EEAGIncreasingVelocityUnitSorter, ResetUnitsPaths, Finish, SetGoal, Clear, KeepNonCitizens, KeepGroundAttackers,
  KeepCitizens, RemoveUnit, TellGroup, Validate, ClearGoal, the destinations), `EEGroupPathRequest`,
  `EEGroupCluster`, `EEComplexUnit::SetActiveGroup`, the active groups' save and restore (EESaveActiveGroups,
  EELoadActiveGroups: the C++ entry names are two-line wrappers).
- The C++ left for the groups: `ee_units_group_host` (EEGroup.cpp, ~45 one-statement cases: reference counts,
  EEFormation, the path and continent managers, the user interface, the map's outlines, the saved path requests,
  EEAwaitAutoPathRequest and EEAfterLoad), the inline handles in ee/EEPlayer.h and ee/EESavedGroup.h, the seven speed
  getters a unit reads through its group (EEActiveGroup.cpp: they read the unit's C++ stats) and `ee_units_ag_host`'s
  remaining world cases (the units' goal queues and actions, the formation's Arrange, the launch, the map's tiles,
  the C++ goal constructors NewLocation and GetGoalLocation of EEUGRust.cpp).

### Gates (the final tree; origin/rust 9e5b09552 is contained, the merge was a no-op)

See PORT_LEARNINGS.md "The groups lane, round 1" for the numbers.

## Round 2 (branch `rust-groups2`, from round 1's 5c6b5b2e1 merged with rust-batch10, 2026-11-20)

Scope: all of src/Empire Earth/Player and include/ee/EEPlayer*.h, the groups, EEUnitClassUpgrade and
EEUnitClassImprovement. Not this lane's: the towns' and herds' hosts (EEPlayer.cpp's ADD_TO_TOWN .. TOWN_PATROL cases,
EETownSaveHost, the tick's herd cases: AI round 7's), the path slice (the `EE_UNITS_GROUP_OP_PATH_*` cases).

The merge of rust-batch10 (e0223771f): PORT_LEARNINGS.md is rust-batch10's with round 1's section after it;
include/rust/sim/units.h regenerated; EEActiveGroupSweep.cpp stays deleted (units round 5 moved it to ag_sweep.rs and
`ee_units_sweep_host`, EEPhysicsSweep.cpp), and the sweep host makes its groups as the group objects
(`EEActiveGroup::New`, `mFields.mSpeed`/`mMovesTogether`/`mAction`/`mIsSpecialAttack`, `GetPlayer()`).

| Commit | What | C++ logic / net lines |
|---|---|---|
| e0223771f | the merge of rust-batch10 (units 5, game core 5, WON 27): the sweep host over the group objects | (the merge) |
| 72049b7b2 | the player's group lists as a Rust object; EEPlayer's bodies (player_body.rs); the compound host cases of EEPlayerHost, the save, unit and tick operations, the ag and group hosts as one-statement cases (player_split.rs, player_save_split.rs, player_tick.rs, ag_host.rs, group_host.rs); EEUnitClassImprovement's bodies and EEUnitClassUpgrade's saves (improvement.rs) | -772 / -224 |

Round total (logic_lines.py over src, include/ee, include/lle; e0223771f -> 72049b7b2): logic 85,852 -> 85,080
(-772), lines 171,336 -> 171,112 (-224; 944 deleted, 720 added). Shims: 269 one-statement host cases added (the player
body host's 147, the player hosts' split cases' 92, the ag host's 14, the group host's 13, the improvement host's 3), the four
one-call list accessors of ee/EEPlayer.h, and ~35 members that became one call; 47 case labels deleted (the
compound cases and the group host's four list cases). The target was -3,000: what is left in this scope is below.

### What moved

- **The player's group lists** (were `mRunningGroups`, `mPendingGroups`, `mFinishedGroups`): a Rust object
  `EeUnitsPlayerGroups` (group_obj.rs), the player's `mGroupLists` (made by the constructors, freed last by the
  destructor; `player_group_lists` in the world layout). The group bodies' host reads them in place; the C++ reads
  them through `GetNumRunningGroups`, `GetRunningGroup`, `GetNumPendingGroups`, `AddPendingGroup` (one call each):
  EEPhysics.cpp's out-of-synch dump, EEPhysicsSweep.cpp, EESMEffect.cpp (3), EEScenarioCapture.cpp, EEActiveGroup.cpp.
  The group host's four list cases are gone.
- **EEPlayer's bodies** (player_body.rs over `ee_units_player_body_host`, EEPlayer.cpp, one statement a case): the
  constructors, the destructor, `Reset`, `Clear`, `RepairPointers`, `ExecuteSelectionMessage`,
  `SetSelectedViewedUnit`, `UnitsSwapped` (the selection's and hot-key groups' parts over the group objects),
  `UnselectAllUnits`, `ClearSavedGroup`, `RestorePatrols`, `KillStrandedUnits`, `UnloadAllMedia`,
  `LoadPlayerEffects`, `UnloadPlayerEffects`, `PulseSight`, `ApplyMapRestrictions`, `EnableCamouflagePower`,
  `DisableCamouflagePower`, `GetNearestConstructionSite`, `GetCitizenPassability`, `GetTeamIndex`,
  `SwapClassButtonMedia`, `RefreshTerrainVisibility`, `GetPatrolGroup`, `DamageTaken`, `ResetTechTree`,
  `GetNextComplexUnit`.
- **The hosts' compound cases** (each now one statement a case; the choice, loop or test in Rust): `EEPlayerHost`'s
  CAMOUFLAGE, SHARE_BONUS, SIGHT_SHARER, COMPLEX_SIGHT, SET_VALUE, EVENTS_SIGHT, ALLY, SHARERS, MINIMAP_REDRAW,
  WONDER_LIST, UNITS_SIGHT, WONDER_STARTED, REMOVE_FOREST, TECH_ALLOW, UPGRADE_TYPE, REASON, IMPROVEMENT_TIME
  (player_split.rs, in the game's `FnHost`: the bodies and their tests ask the same operations as before);
  `EEPlayerSaveOperation`'s UPGRADE_DUMMY_NEW, UPGRADE_LOAD, TECH_LOAD, PATROL_LOAD, WONDERS_CLEAR, TECH_SAVE,
  PATROL_KIND (player_save_split.rs); `EEPlayerTickOperation`'s VALIDATE, AIR_AND_EVENTS, HEAL_VALUE, UNIT_NEXT
  (player_tick.rs `split`); `ee_units_ag_host`'s LAUNCH (now Rust: the goal, the action, the pending list, `Plan`,
  the acknowledgement), QUEUE_ADD, ACT, SQUADRON, TILE_AT, CAN_CAST (ag_host.rs); `ee_units_group_host`'s formation
  state and centre, CONTINENT_AT, SUBSECTOR_AT, SLOT_AREA_AT, POST_SAVED_GROUP, DOPPELGANGER, PLAY_SOUND
  (group_host.rs).
- **EEUnitClassImprovement and EEUnitClassUpgrade's saves** (improvement.rs): the constructor's levels,
  `CopyLevelsTo`, `Reapply`, `Load`, `Save` and `EEUnitClassUpgrade::Load`/`Save` over the record's levels and points;
  the C++ they call is `EEImprovementHost` (ChangeLevel, the name's load and save, the file's primitives; two new
  primitives `EE_UNITS_PLAYER_SAVE_READ_LEVELS`/`WRITE_LEVELS` move the 0x40 raw bytes).

### Gates (the final tree, b0ee19646 + the clippy fix; origin/rust 9e024ffb3 merged: the same tree as rust-batch10)

| Gate | Result |
|---|---|
| `build.py -j 3 --sim` | 0 / 0 (EE-AOC, EE-SIM) at every step and on the final tree |
| `build.py --test-profile windowed-software` | 0 / 0 |
| `rust_build.py gate` | PASS, 5,100 tests (the first run failed `ee_won_lobby` `eecolor::tests::golden_replay` and `the_palette_is_the_originals_until_the_game_sets_it` under load: passed alone, and the whole gate passed on the rerun; an earlier run failed clippy on improvement.rs, fixed in its own commit) |
| oracle against the main checkout's `ref` | seeds 2-3 identical after the merge and at each step; seeds 2-7 identical on the final tree |
| capture profiles (seeds 2-3) | all 12 PASS: oracle 212, late 214, space 210, calamity 222, units 175, effects 6, missile 22, app 26, scenario 43, transport 185, groups 2, placement 4 digests |
| plain saveload (seeds 2-7) | 18/18 identical, `saveload: PASS`, exit 0 |
| the matrix (`--settings all --seeds 2-3 --at 1000,1900 --expect tools/saveload_expect.txt`) | `saveload expect: PASS`, exit 0 (calamity 3/4, next 2/4 as expected, the rest 4/4) |
| scenario_check | 27/27 |
| sim_editor / `--civbuilder` | PASS (14,448 lines) / PASS (4,062 lines) |
| eedata check / cppgate | PASS / PASS |
| the tour (`fixture_game.py --exercise random-game --box-select --orders --seed 7`, slot.py) | `RESULT PASS random-game`: box select 4 steps, orders gather, repair, queued repair, move (patrol and attack-ground skipped by the fixture) |
| `gates.sh` (EE_GATES_TREE the main checkout's tree) | `RESULT PASS tour`, `RESULT PASS ai-game outcome time-cap PT 12011` units [4, 36, 37] alive [2, 3] engines classic_lua x 2, `RESULT PASS lan-game PT host=2874 joiner=2873`, GATES PASS |

### Left

- EEPlayer.cpp: `ClearFootprint` (64 logic lines: the unit utilities' C++ vectors and a transformation per unit),
  `SetGameResult` and `ReloadTechTree` (strings, a memory file), the capture scopes (PLEC, PLST), `EESavePrimitive`,
  `EEPlayerUnitOperation`, the remaining record-filling host cases (NODE_INFO, CLASS, IMPROVEMENT, UPGRADE_VALUES,
  WONDER_FACTORS, PULSE_EXPLORATION), the towns' and herds' parts (not this lane's), `EEPPlayerEventList`'s host
  (SHOW/HIDE, SOUND, PING, TEXT), its constructor and destructor, EEPlayerStatistics.cpp.
- The groups: the formation's `Arrange`/`Place` and the area outlines (C++ vectors), EECPASGFlags' marshalling
  (TERRAIN_FLAGS, the path requests: the Paths lane's), the unit's speed getters (EEActiveGroup.cpp, one call each).
- EEUnitClassUpgrade's `Init` (the class's media members into the upgrade's setters) and `DumpMediaCount`;
  EEUnitClassImprovement's `GetGainText`/`GetName` (strings) and `ChangeLevel`'s step factors.
- EEPlayer.h's declarations: the one-call members could be inline (each declaration a shim).
