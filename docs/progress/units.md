# The units (`src/Empire Earth/Units`, the actions, formations, the physics thread, the unit utilities) whole in Rust

The lane's state (one file per lane; each round's story is a PORT_LEARNINGS.md section: "The units' lane, round 1",
"The units' lane, round 2"). Rust home: `ee_sim::units`, `ee_sim::combat`, `ee_sim::formations`, over the world
views (`ee_sim::world`, [world-views.md](world-views.md)). Earlier rounds over the same code:
[r5b4-world-and-units.md](r5b4-world-and-units.md), [r5b2-combat-and-unit-formulas.md](r5b2-combat-and-unit-formulas.md),
[r5b-unit-utilities.md](r5b-unit-utilities.md).

## Rounds

| Round | Branch | Net C++ | What |
|---|---|---|---|
| 1 | `rust-units1` | -546 | `UpdateHidden` whole in Rust; the update, combat query, goal, unit, active group and citizens hosts' plain reads on the views |
| 2 | `rust-units2` | -1,833 | the formations one class over Rust shapes (9 units, 12 headers gone); the group command sweep's dry host whole in Rust (units/dry.rs); the unit host's and the combat goals' plain reads on the views; `EEUnitOrderPush/Erase/Clear` deleted |
| behaviour 1 | `rust-unitsbeh1` | -456 | the goal, combat-goal, citizen, class and group hosts' plain reads, stores and duplicate world calls answered in Rust (units/unit_geom.rs, `uu_direct::with_game`); the gather and build goals' farm and resource searches in Rust (units/gather_world.rs) |
| behaviour 2 | `rust-unitsbeh2` | -1,568 | the group command, pick and Fill sweeps, the step's notes and the combat records' batches whole in Rust (TEST ONLY); the update, combat world, goal, unit, sight, active group, trigger, class and physics hosts' effects, stores and plain reads in Rust |
| behaviour 3 | `rust-unitsbeh3` | -1,134 | the goal objects and their queues whole in Rust (goal_obj.rs, goals.rs: made, cloned, deleted, saved and stepped in Rust; the mirror gone); the goals' accessors and context-free virtuals inline over Rust; the unit's queue calls (SetGoal, GetQueuedGoal, RefreshGoals, RefreshActiveGoal, DropUnsuitedGoals, DropFinishedGoals) and the hosts' goal cases in Rust; 46 unit-utility forwarders gone |
| 4 | `rust-units4` | -712 | the unit utilities' one-call forwarders gone (40: callers call the Rust); the unit host's goal constructions, clones, queue calls and plain reads in Rust (units/goal_spec.rs); 34 one-call members of `EEUnitClass` and `EEActiveGroup` inline in their headers |

## Round 1 (branch `rust-units1`, from origin/rust 48c8472f0, 2026-11-18)

The measure (lines of C++ per file at the start): EEComplexUnit 3,720, EEUnitClass 3,207, EEUGRust 2,031 (GoalOp
972), EEUnitHost 1,906, EECombatGoals 1,308 (EEWorldOp 325, EEQueryRead 116), EEUnitClassBuildings 1,243,
EEComplexDoppelganger 1,215 (the unit's save operation 317, its load sections 193, RepairPointers 275), EEActiveGroup
1,179 (its host 334), EEUnitClassAir 1,028, EEUnitClassPeople 902 (the citizens' host 132), EEUnit 841, EEUnitUpdate
742 (its read host 153, its act host 467). `tools/dead_members.py report` finds nothing left in this area (the LLE
lane's round 2 ran it to a fixpoint; the 11 names it lists here are vtable slot placeholders and one-word accessors
of other areas).

What is a host kept alive by Rust calling back for data the views now read: the read switches of the update and
`ActionChanged` host, the combat query host, the goal host's record fills and plain reads, the unit host's plain
reads, the active group's and the citizens' unit fills. This round moved those, and one whole method pair.

| Commit | What | Net C++ |
|---|---|---|
| ddc53c80e | `EEUnit::UpdateHidden` and `EEComplexUnit::UpdateHidden` whole in Rust (units/hidden.rs over a host trait; the views write `mHiddenFlags` in place); the renderer's opacity calls it directly (its host callback gone); the physics' game time and tick in the views (`WorldView::physics`, `game_time`, `tick_ms`; layout `physics_pointer`, `physics_saved`) | -170 |
| 066621d3d | The update / `ActionChanged` host's 44 plain reads (units/update_views.rs `ViewUpdateHost`; the table carries the host's class) | -91 |
| 692e01050 | The combat query host's 16 plain reads (combat/query_views.rs, asked first by `EeCombatQueryHost::read`) | -25 |
| 4ebeef429 | The goal host: `BUILD_UNIT`'s record fill (rust_goal.rs `build_unit_fields`), `LOAD_CLASS`, `UNDETECTABLE`, `LINE_OF_SIGHT`, `FIRES_OVER_WALLS`, `SAME_PLAYER`, `UNIT_LOCATION`, `PLAYER_ID`, `CLASS_UNIT_TYPE`, `FRIENDLY`, `IN_CINEMATIC`, `UNIT_CATEGORY` | -103 |
| 6c68f5a67 | The unit host's 9 plain reads (`object_hit_points`, `unit_max_hit_points`, `unit_garrison_capacity`, `unit_has_tile_node`, `object_record`, `unit_air_traffic_type`, `unit_class_count`, `unit_class_info`, `map_has_tile`) | -71 |
| 353e4d8f6 | The active group's unit fill (ag_host.rs `ag_unit_fields`) and the citizens' unit fill, tiles and player lists (citizen_host.rs `cz_unit_fields`) | -86 |

Round total: -546 net C++ lines (target 4,000). Merged origin/rust 9a277da24 (AI lane C) cleanly.

Verified on the merge: both builds 0/0, the Rust gate (4,578 tests), the oracle seeds 2-7 identical, the twelve
capture profiles at seeds 2-3, the plain saveload 18/18, the matrix `--expect` PASS, scenario_check 27/27, the
editor gate, the test-profile build; gates.sh: see PORT_LEARNINGS "The units' lane, round 1".
Only a tour exercises the renderer's direct UpdateHidden call (ee_render fill.rs).

## Round 2 (branch `rust-units2`, 2026-11-18)

| Commit | What | Net C++ |
|---|---|---|
| 873399560 | the formations one class: `EEFormation` concrete, `Place` one `ee_formation_place` call by type, the capture's members from Rust (`ee_formation_shape_words`); EEFCircle, EEFLine, EEFWedge, the 6 presets and 12 headers deleted (with EEHerd.cpp's cast and includes, AI Thread) | -949 |
| fe3ac7dff | `EEUnitOrderPush`/`Erase`/`Clear` and their Rust entry points deleted (no caller) | -18 |
| c9fe4de05 | the dry host whole in Rust (`units::dry::DryHost`, its tests in dry_tests.rs); EEUnitHost.cpp's `sDry` branches and dry machinery deleted; the sweep calls `ee_units_dry_begin`/`_end` | -814 |
| cf8e9f9d1 | the unit host's `object_unit_type`, `unit_complex_id`, `player_num_capitols`, `unit_power`, `player_hack_class` on the views; `unit_class_targetable` calls `ee_units_uu_targetable` | -40 |
| f14f017a9 | the combat goals' 12 plain reads on the views (goal_objects.rs `view_read`) | -12 |

Round 2 total: -1,833 net C++ lines (target 4,000).

## Behaviour round 1 (branch `rust-unitsbeh1`, from origin/rust 00c2f7673, 2026-11-19)

The lane is now cut by layer: this lane moves bodies (goals, actions, class methods), the units STATE lane turns
the members into records. This round took the hosts' operations that no longer need C++: every case whose answer
the world views read, whose store is a record member, or whose world call another host already makes (the unit
utilities' game host, `uu_host::Game`, reached through `uu_direct::with_game`), and the goals' C++ helpers whose
callees are all Rust or one existing host operation.

| Commit | What | Net C++ |
|---|---|---|
| 27120b195 | GoalOp (23 cases), the combat goals' host (22), the citizens' host (16), the active group's position, the uu host's position and distance: answered by the views (units/unit_geom.rs), the records, the noted uu entry points or GoalOp's own cases | -275 |
| fdcb51128 | EETendNextFarm, EEGatherNearBuilding, IsAtResourceSpot, SendToNextFarm, HasGatherRoom (and the utilities' EEWorkFarm) in Rust (units/gather_world.rs); FIND_FISH folded into RESOURCE_OF_KIND | -139 |
| aca0c079f | GoalOp RING_TILES, FARM_TILE, AT_RESOURCE; the class host's line of sight, random draws, vital-building and clock reads; the combat goals' attack part | -32 |
| 8c05cd9ba | GoalOp PATH_AREA_ID, TRANSPORT_KIND | -10 |

New Rust: `units::unit_geom` (GetPosition, DistanceSquaredToUnit/To/2D, GetLineOfSight over the views),
`units::gather_world` (the five searches over `GatherWorldHost`, with tests over build.rs's mock world),
`uu_direct::with_game`, `square_ring`, `ring_corners`, `square_ring_tiles`, `farm_drop_sites`, `find_free_farm`,
`resource_spots`, `place_on_tile`, `within_reach`, `ai_random_shuffle` (Dinkum's random_shuffle over the AI's draws).

## Behaviour round 2 (branch `rust-unitsbeh2`, from origin/rust 7779d7db1 with rust-cuts1 27cebd6c5 and local rust 645799684, 2026-11-19)

The three blockers the round started from: effects from ee_sim (`world::effects`), the patrol groups and the air
traffic as Rust objects, a smaller cycle (SCC 1 220 units, 106,210 lines at the start). What moved whole is the TEST
ONLY C++ that only reads the world and calls Rust bodies (the group command sweep, the pick and Fill sweeps, the
step's notes, the combat records' batches); what moved from the hosts are the cases that are one effect, one member
store or one plain read.

| Commit | What | Net C++ |
|---|---|---|
| 0047691e2 | the group command sweep in Rust (units/group_sweep.rs: its cases from the views and the tech tree's views, its USGC records); EEGroupCommandSweep.cpp deleted | -741 |
| 4f1f3d51d | the unit step's notes in Rust (units/notes.rs: USTM, USST, the update and ActionChanged operations); the physics host's unit, class, stats, location, command, goal kind and unit reads on the views | -227 |
| 9a999753d | the update and ActionChanged hosts' one-member stores, the stop and the death effect (update_views.rs `view_act`, `halt`; the layout's angular velocity and orientation) | -86 |
| 86e042cb0 | the combat world host's effects, one-member stores and plain reads (combat/world_views.rs; `Effects::graphic_effect`; the host carries its call's class) | -46 |
| 25b0909ad | GoalOp's patrol group calls, its halt and the abandoned building's no-op (rust_goal.rs) | -32 |
| 5c67c4bf5 | the unit host's goal kind, group action and queued flag, patrol points and group, unload test, technology class (host.rs) | -59 |
| 4d7ab0160 | the pick and Fill sweeps and their USPK and USFL records (units/pick_sweep.rs); EEFillMembers deleted | -141 |
| bdaaa0f1f | other lanes' files: EEPlayer.cpp's pick record and EESim.cpp's two sweeps by their Rust names | +1 |
| 8509ab808 | the sight host's current player, shared sight, sharers, tiles and players (sight_host.rs) | -13 |
| dea90fb2d | the active group host's goal kind, gather and patrol goals' units, unit sphere, regions' size (ag_host.rs) | -16 |
| 14b449799 | the trigger host's and the class host's plain reads (cls_host.rs `cond_view`, `cls_view`) | -31 |
| 2af15cb4a | the physics host's angular velocity and orientation on the layout (pm_host.rs) | -26 |
| 70b7d887f | the combat records' batches (combat/capture_fold.rs: CBTC .. CBGO); the goal host's projectile test and its CBGO and CBSO records (goal_objects.rs) | -151 |
| be16e258f, 6fe00632a | group_sweep.rs's test-build dead code; include/rust regenerated | 0 |

Round total: -1,568 net C++ lines (target 4,000).

Verified on the final tree (origin/rust 7779d7db1 is contained: the merge before the report was a no-op): both
builds and the test profile 0/0; the Rust gate PASS (4,895 tests); the oracle seeds 2-7 identical; the twelve capture
profiles at seeds 2-3 PASS (oracle 212, late 214, space 210, calamity 222, units 175, effects 6, missile 22, scenario
43, transport 185, groups 2, placement 4, app 26 digests); the plain saveload 18/18 identical; the matrix
`--expect` PASS; scenario_check 27/27; the editor gate PASS; `gates.sh` GATES PASS (tour; ai-game time-cap, units
[4, 41, 41], alive_computers [2, 3], `engines ['classic_lua', 'classic_lua']`; lan-game); the tour with
`--box-select --orders` PASS (gather, repair, queued repair, move; patrol and attack-ground skipped by the fixture).
Only a game draws what the moved effect calls make (EE-SIM makes the effects, the captures' EFST/EFMU/CB* records
check them, but nothing renders them): the tours cover the muzzle flashes, shots and death effects of the fights
they reach.

New Rust: `units::group_sweep` (the sweep's cases as plain facts, `player_cases`, `record`; its tests),
`units::pick_sweep`, `units::notes`, `combat::world_views` (`world_op`), `combat::capture_fold`,
`update_views::view_act` and `halt`, `cls_host::cls_view` and `cond_view`, the sight host's and the unit host's
reads, `Effects::graphic_effect`, the layout's `complex_angular_velocity` and `complex_orientation`.

## Behaviour round 3 (branch `rust-unitsbeh3`, from the lead's integration tree 0a17fc666, 2026-11-19)

The brief: move the foundations the class bodies wait on (the goal queues and the goal objects' fields, Clone, unit
creation), then the bodies. What moved wholly is the goal object and the goal queue; unit creation and the class
bodies did not (see "What is left").

| Commit | What | Net C++ |
|---|---|---|
| 00ea20b6c | the goal objects whole in Rust (units/goal_obj.rs): a goal is a Rust allocation headed by `EeUnitsGoalBase` (EEUnitGoal's members, asserted), made (`EEUnitGoal::Make`), cloned and deleted in Rust, the counter (`sGoalCounter`) Rust's; the base fields' host operations answered in Rust; the queues whole in Rust (units/goals.rs `EeUnitsGoalQueue`: unit, goals, timer; `EEUnitVectorItem` is its handle, `EEGoalDeque` its interface), the mirror, its reports and USCK's queue check gone; the queue's save, load and update over the queue; `GetQueuedGoal`, `RefreshGoals`, `RefreshActiveGoal`, `SetGoal`, `DropUnsuitedGoals` in Rust (layout `complex_goal_queues`, `UnitView::goal_queues`) | -444 |
| 76c415741 | the kinds' New functions and accessors inline in ee/EEUnitGoal.h over `ee_units_goal_call` and the records; GoalOp's `SET_GOAL`, `NEW_GATHER`, `LOCATION_GOAL`, `RESET_ACTIVE_PATH` in Rust | -302 |
| f1100291d | `DropFinishedGoals` in Rust; the trigger host's `GOAL_INFO`, `GOAL_CONDITION`, `GOAL_ELAPSED`, `GOAL_TARGET`, `LOCATION_QUEUED`, `QUEUED_GOAL`, `SECONDARY_TARGET` (cls_host.rs `cond_goal`) | -63 |
| ecd41f7b8 | 31 unit-utility forwarders (EEFOFindNewTargetNearOldTarget.cpp) gone: each single caller calls the Rust; five uncalled ones deleted | -244 |
| fc48c9398 | ten more forwarders gone (two callers in the calamities lane's EECalamityUpdate.cpp) | -81 |

Round total: -1,134 net C++ lines (src, include/ee, include/lle; target 3,000).

Call-site edits outside the lane (each a line or two): EEComputerPlayer.cpp (`GetUnit()` for the queue's `mUnit`),
EEEffectHost.cpp (the out-of-line `GetGoalAt` deleted), EEPlayer.cpp (the goal counter's two save cases deleted:
Rust answers them), ee/EmpireEarth.h (`sGoalCounter`'s declaration deleted), EECalamityUpdate.cpp (the storm
reply's distance and radius call the Rust).

Verified on the final tree (00ea20b6c..the clippy commit; origin/rust 3449a04fd is contained, so the merge before
the report was a no-op): both builds 0/0 (EE-AOC 508 inputs, EE-SIM 509) and the test profile 0/0; the Rust gate
PASS (4,970 tests); the oracle seeds 2-7 identical; the twelve capture profiles at seeds 2-3 PASS (oracle 212, late
214, space 210, calamity 222, units 175, effects 6, missile 22, app 26, scenario 43, groups 2, placement 4,
transport 185 digests); the plain saveload 18/18 identical; the matrix `--expect` PASS; scenario_check 27/27;
sim_editor PASS (14,448 lines); `gates.sh` GATES PASS (tour; ai-game time-cap, units [4, 42, 42], alive_computers [2, 3], `engines ['classic_lua',
'classic_lua']`; lan-game); the tour with `--box-select --orders --seed 7` PASS (gather, repair, queued repair, move;
patrol and attack-ground skipped by the fixture). Only the tours exercise the human's orders through the moved
goal calls (`SetGoal`, the queues' timers: the rally and scramble timers of EEMessage.cpp) in a drawn game.

## Round 4 (branch `rust-units4`, from rust-batch7 c2819fe2d, 2026-11-19)

The brief: net -3,000 C++ lines by whole class moves, UI-first (`cpp_deps.py` over the UI units). Measured at the
start: the UI units (User Interface Thread, UI Events, the LLE UI; EEFOFindNewTargetNearOldTarget.cpp aside) name
only 56 distinct Units-lane symbols, mostly `EEGroup`/`EEUnitClass` accessors, `EEGhostUnit`'s statics and
`EEPhysics`' clock; no Units-lane class blocks them alone. What the round moved is the forwarder layer the brief
named and the hosts' calls whose answer is already Rust; the class bodies did not move (see "What is left").

| Commit | What | Net C++ |
|---|---|---|
| a50ab82b0 | 40 one-call forwarders of EEFOFindNewTargetNearOldTarget.cpp (`EEGetUnitsNear`, `EEGetTilesNearPoint`, `EEFindPlacement` x2, `EEFindDropLocation`, `EEIsSeenBy`, `EECanStandOn`, `EECanPlaceOnTile`, `EEGetUnitsOn*`, the rings, wall lines and tile lists, `EEPlanWall`, `EEGetDockShip*`, ...) and their declarations deleted: every caller (37 files) calls `ee_units_uu_*` or the combat goals' `EECombat*` directly; `EEMoveInDirection` had no caller; `ee_units_uu_can_place_unit_on_tile` takes a null flag | -333 |
| 2aade7fad | the unit host's goal constructions (`ee_units_host_goal_new`: goal_spec.rs `plan` + `make`, units resolved through the registry), release, clone, the unload goal's transport and can-unload, `SetGoal`, the can-place test, the queues' timer and clear, `RefreshGoals` in Rust (GameHost) | -121 |
| ec7a84aca | 23 `EEUnitClass` and 11 `EEActiveGroup` members whose body was one Rust call defined inline in ee/EEUnit.h and ee/EEPlayer.h | -219 |
| c4b96c455 | the unit host's `unit_player`, `unit_shares_rally_point`, `unit_shares_research`, `unit_num_goal_queues`, `unit_goal_clone`, `unit_research_state` on the views and cls_answers.rs | -39 |

Round total: -712 net C++ lines (src, include/ee, include/lle, include/gx, include/dx7; target 3,000).

Call-site edits outside the lane (each the forwarder's call replaced by its Rust call, the include added): AI Thread
(EEAIContinentManager, EECPNext, EECPNextUnits, EEComputerPlayer, EEPathManager), Calamities (EECalamityCast,
EECalamityClass, EECalamityUpdate), Civilization Builder (EEAreaEffect), Empire Earth (EECameraControl,
EETestFixture), Formations (EEFormation), Graphics Thread (EEEffectHost), Map (EEMap, EEMiniMap), Player (EEPlayer),
Random Map (EERandomMap), Scenario Editor Actions (EESEARust), Scenario Manager (EEEvent, EESMEffect, EESMObject),
UI Events (EEScreens: `EEGetLeadUnitID`), User Interface Thread (EEUIFMainGame); the headers ee/EEAreaUtilities.h,
ee/EEAIFOUnitScreeners.h, ee/EERandomMapClass.h (the declarations), ee/EEPlayer.h (EEActiveGroup's inline members);
strip_markers after the merge: ee/EEAIContinentManager.h, ee/EEUnitVectorItem.h, ee/wonapi/WONLobbyGUI.h.

Verified on the final tree (origin/rust 4fb6c7119 merged: no change): both builds 0/0 (EE-AOC 482 inputs, EE-SIM
483) and the test profile 0/0; the Rust gate PASS (4,994 tests; two ee_base timing tests failed once under load and
passed on the rerun); the oracle seeds 2-7 identical; the twelve capture profiles at seeds 2-3 PASS (oracle 212, late
214, space 210, calamity 222, units 175, effects 6, missile 22, app 26, scenario 43, groups 2, placement 4, transport
185 digests); the plain saveload 18/18 identical (exit 0); the matrix `--expect` PASS (exit 0); scenario_check 27/27;
sim_editor PASS (14,448 lines) and `--civbuilder` PASS (4,062 lines); eedata check and cppgate PASS; `gates.sh` GATES PASS (tour; ai-game time-cap,
units [4, 40, 40], alive_computers [2, 3], `engines ['classic_lua', 'classic_lua']`; lan-game). The human orders' goal constructions (EEMPathTo, EEMUnloadTransport,
EEMSetRallyGoal, EEMCastCalamity through goal_spec.rs) run in EE-SIM only where a computer player or the group
command sweep sends them; the tours' `--orders` exercise them in a drawn game.

## What is left (behaviour)

- **Round 4's measure**: the UI layer names few Units-lane symbols (56, `cpp_deps.py` edges.tsv from the UI
  units); the units' own C++ is world glue around Rust (GoalOp ~550 lines, `EEUpdateAct` 374, `EEWorldOp` 277,
  `ee_units_ag_host` 227, `EEUnitClassMoveOp` 207, `ee_units_cls_cond_host` 204, `EEShotOp` 200, the unit's
  `RepairPointers` 272 and load sections 190): each case is a call into a C++ class (actions, sounds, the path
  manager, calamities, the tech tree), so a case moves only with its callee. The TEST ONLY sweeps
  (EEPhysicsSweep.cpp 234, EEActiveGroupSweep.cpp 305) create and order units through `CreateUnit`,
  `ProduceUnit` and `EEActiveGroup`: they move with unit creation.
- **Unit creation** (`BaseCreateUnit`, `BaseCreateUnitAt`, `EECreateUnitAt`, the mining building's and the
  buildings' variants; EEUnitClass.cpp ~200 lines) constructs `EEComplexUnit`s: it moves with the unit object, not
  before it. The class bodies the brief named (`ProduceUnit` 191 lines, `ReleaseUnit` 149, `Animate` 106, `Heal`
  52, the gate builders, `FlyToGoal` 52, `IsBombOnTarget` 65) need, besides the goals (now Rust), the creation,
  `LoadCargo`, the placements (`EEFindPlacement`, `EEGetDockShipPlacement`: transformations), the transformation
  math (`ConvertToOrientation`, `SetOrientationYPR`, `Apply` with tag points), the area effects, the sounds and
  `StartAction`/`ActionChanged`: each is a body over a new host, about a third of its lines saved.
- `ClearGoals`, `ClearGoalQueue` and `QueueGoal` (AI lane's file) wait on `SetGatheredResource` (the world's
  resource users, the waiting gatherer's restart) and the air traffic queues.
- GoalOp (~550 lines) is world calls now (actions, transport and calamity classes, files); its file cases (`READ`,
  `WRITE`, `FILE_OPEN`) and the queue host's typed `FSFile` calls are one shared typed-file shim away from Rust.
- The unit utilities (EEFOFindNewTargetNearOldTarget.cpp, 2,327 lines after round 4): the forwarders are gone; what
  stays is C++ bodies (`EEConvertUnit` 140, `EEReplaceUnit`, `EEComputeSoundParameters`, `EECanHeal`'s record fill,
  the target searches' and line of fire's hosts ~330, `EEUpdateUnitTile`, the wall and gate helpers) and the uu
  hosts (`ee_units_uu_host_*`, ~360 lines).
- From the earlier rounds, still open: the calamity classes' world steps, the capture writers over C++ objects
  (EECalamityCapture.cpp, EEScenarioCapture.cpp, the combat records' builders), the save operation's pointer members,
  the TEST ONLY drivers (EEPhysicsSweep, EEActiveGroupSweep: unit creation and production), `EEActiveGroup`'s and the
  groups' C++ members, the active groups' save (formations, path requests).
