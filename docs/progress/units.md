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
| 5 | `rust-units5` | -887 | the game log's human orders, the units sweep and the active groups' sweep whole in Rust (EEActiveGroupSweep.cpp deleted); the gather goal's spot search and accessibility; the combat records of a unit's state (CBSO, CBXA, CBQR, CBXR, CBAD); EEFindLandingPoint, EEGetRandomPointAhead; host cases answered over the views and the Rust continent manager |
| 6 | `rust-units6` | -603 (logic -1,358) | GoalOp's compound cases split into one-statement cases; 51 `EEComplexUnit` and `EEUnitClass` bodies (Kill, Invalidate, StartProduction, ProduceUnit, the cargo, garrison and production queue, Heal, UpdateBuilding, ...) in Rust over one-statement hosts |
| 7 | `rust-units7` | -2,737 (logic -4,623) | three forks: the EEUnitClass family (creation, LaunchShot, Animate, the media, the buildings', shots', people's, ground and transport bodies), EEComplexUnit's remaining bodies with the doppelganger load/save and RepairPointers, EEUnit's TakeHit and Kill, the placement ghosts, the doppelganger manager, the unit utilities (EEConvertUnit, EEReplaceUnit, the searches), the cloaking utilities whole, EEGatherNextTree; each C++ statement one host case |
| 8 | `rust-units8` | -2,416 (logic -2,969) | three forks: EEUnitClassAir, the kinds, the constructor, gates and impact tests; EEUnitHost, the save operation's sections, EEUnit's and EEComplexUnit's bodies, the store check, GoalOp's list cases, the trigger host; the target searches, line of fire, EEWorldOp, EECombatCondition/Step, EEUpdateAct (each body in Rust, its C++ statements one-statement host cases) |
| 9 | `rust-units9` | logic -2,445 | three forks and the lead: the unit constants' enums Rust's (ee_sim unit_ids.rs); the EEUnitClass family's remaining bodies, the kind dispatchers, EEShotOp; the doppelgangers, ghosts, AIStep whole, the store fill, the constructors; the combat queries' entry points, the goal accessors, the unit utilities' adapters |
| 10 | `rust-units10` | logic -1,409 (net -3,552) | three forks: the unit class record Rust's (DbUnitClass); about 300 EEUnit.h members inline; the units' pointer members in their Rust records; NavalBuildingHasSunk/COND_SUNK one function; the utilities', goals' and combat queries' entry points inline, the region walk in Rust |

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

## Round 5 (branch `rust-units5`, from round 4 dd648eb02, 2026-11-20)

The brief: net -3,000 C++ lines by whole bodies, the hosts collapsed with their callees. What moved wholly is every
body whose callees are already Rust or a view: the TEST ONLY drivers, the game log, the gather spot search (the
continent manager is Rust since the AI lane's round 5), four capture builders, two EEWheel.cpp helpers. The hosts'
remaining cases call other lanes' C++ (see "What is left"); the round ended at -887, short of the target.

| Commit | What | Net C++ |
|---|---|---|
| 34f0a70f2 | the game log's human orders and selections (`EEPhysicsLogOrder`, `EEPhysicsLogSelection` and helpers) in units/order_log.rs over the message records (ee_netmsg records.rs) and the views, one C++ host for the names and the line; `EEPhysicsUnitSweep` in units/unit_sweep.rs and `EEActiveGroupCaptureSweep` in units/ag_sweep.rs over one host (EEPhysicsSweep.cpp: unit creation, production, groups, goals, the AGPL record); EEActiveGroupSweep.cpp deleted; EESim.cpp calls the Rust sweeps | -565 |
| adc081ac9 | `EEGatherFindSpot` (and its sort order) and `EEGatherAccessible` in units/gather_spot.rs over ee_sim::astar, the unit utilities and the views (`GetTerrainFlags` as `terrain_flags`); `HasGatherPoint` keeps its UCBD note through the uu host's world case 18; GoalOp's GATHER_SPOT, ACCESSIBLE, UNIT_IN_SET, CELL_TAKEN, TECH_NODE; EEWorldOp's TILE_AT, CORNER_BLOCKED, CAPTURE, DAMAGE_CAPTURE; the trigger host's DISTANCE_TO, INITIAL_CONTACT, TILE, CRUSADER, MINE_DEPLETED | -139 |
| 0b33cfdeb | `EECombatCaptureShot`, `EECombatCaptureXAction`, `EECombatCaptureHostRead`, `EECombatCaptureXRead` in combat/capture_units.rs over the views (inline calls in ee/EECombatGoals.h); EEWorldOp's AREA_CAPTURE and HIT_CAPTURE | -120 |
| (landing) | `EEFindLandingPoint` and `EEGetRandomPointAhead` in units/landing.rs (the AI lane's `CanGetTo` through the uu continent host's case 13) | -63 |

Round total: -887 net C++ lines (src, include/ee).

Call-site edits outside the lane: EESim.cpp (Game core: the two sweeps by their Rust names, the two declarations
deleted).

Verified on the final tree (origin/rust fa7e8674a merged, no conflict): both builds and the test profile 0/0; the
oracle seeds 2-7 identical; the plain saveload 18/18 identical (exit 0); the Rust gate PASS (5,022 tests); the twelve capture profiles at seeds 2-3 PASS (oracle 212, late 214, space 210, calamity 222, units 175, effects 6, missile 22, app 26, scenario 43, transport 185, groups 2, placement 4 digests); the matrix `--expect` PASS (exit 0); scenario_check 27/27; sim_editor PASS (14,448 lines) and `--civbuilder` PASS (4,062); eedata check and cppgate PASS; the tour `--exercise random-game --box-select --orders --seed 7` PASS (gather, repair, queued repair, move; patrol and attack-ground skipped by the fixture); `gates.sh` GATES PASS (tour; ai-game time-cap, units [4, 42, 42], alive_computers [2, 3], `engines ['classic_lua', 'classic_lua']`; lan-game). Only a game with the game log on (not EE-SIM) runs
order_log.rs: the tour exercises it.

## Round 6 (branch `rust-units6`, from the groups lane's round 2 tip 9f2b3d970, 2026-11-20)

The brief: C++ logic lines removed (target -3,000) under the owner's new rule: a body moves to Rust even when it
calls other C++, each call one statement of a C++ host. What moved is GoalOp's compound cases and 51 class bodies of
`EEComplexUnit` and `EEUnitClass` (each now one call); the C++ they call is a one-statement case of a new host per
file (`ee_units_cu_host` in EEComplexUnit.cpp, `ee_units_uc_host` in EEUnitClass.cpp).

| Commit | What | Logic / net C++ |
|---|---|---|
| 3d2240436 | GoalOp's compound cases split into one-statement cases (units/goal_split.rs, called by goal_obj.rs `base_host_op` for every operation): START_ACTION, FIND_UNIT, TELL_GROUP, the file's WRITE/READ, APPEND, the calamity class/info/creation, CONTACT_RANGE, the explore goal's area, tile screen, map dimensions and continent point, NOTICE_FOREIGN, the unload/load transports' cases, the sounds, GARRISON_UPGRADE, DEPOSIT, REPLACE_CLASS, NODE_INFO, COMPLETE_BUILDING, FILL_PARTS, CANCEL_FOUNDATION, BUILD_EVENT, FOUNDATION_STARTED, STORAGE_SEARCH, HUNT, FIND_GHOST, TAKE_ONE, WORK_TIMER | -239 / -82 |
| d29aa826d | `EEComplexUnit::Kill`, `Invalidate`, `StartProduction`, `SetBuilt`, `StartAction`, `SetAnimation`, `PayInsurance`, `FindContacts` (units/cu_body.rs); `EEUnitClass::Heal`, `UpdateBuilding`, `UpdateAnimal`, `RefreshSound`, `CreateDeathUnit` (units/uc_body.rs) | -459 / -148 |
| 44bbf5232 | `UpdateJITManufacturing`, `CancelQueuedProduction`, `ToggleJITManufacturing`, `RemoveResearch`, `ReplaceResearch`, `QueueResearchUncaptured`, `LoadCargo`, `GarrisonUnit`, `AddGarrison`, `KillCargo`, `ReleaseCargo`, `ReplaceCargo`, `UnloadCargoAt`, `UnloadCargoAtTile`, `UnloadCargoNow`, `OrderRepair`, `AddBuildTarget`, `AddContact` (units/cu_cargo.rs); `TakeOffMap`, `UpdateWaterState`, `LoadMedia`, `SetDisabled`, `UpdateCamouflage`, `SetGatheredResource`, `PlayEffect`, `ReleaseFireEffect`, `Halt`, `FleeAfterCalamity`, `LinkPart`, `ClearGoalQueue` (units/cu_misc.rs) | -523 / -299 |
| 595dc49f2 | `EEUnitClass::ProduceUnit` (units/uc_produce.rs) | -137 / -74 |

Round total (logic_lines.py over src, include/ee, include/lle; 9f2b3d970 -> 595dc49f2): logic 85,080 -> 83,722
(-1,358); lines 171,112 -> 170,509 (-603). Shims added: about 480 one-statement host cases (GoalOp's 110
`EE_UNITS_GOAL_HOST_X_*`, ee_units_cu_host's ~290, ee_units_uc_host's ~145) and the 51 one-call members; shims
deleted: none (the hosts' old compound cases were logic). Target -3,000: short by about 1,640 (see "What is left").

New Rust: units::goal_split (`split`, `GoalSplitHost`, `FfiSplit`), units::cu_body (`CuHost`, `ee_units_cu_body`),
units::cu_cargo (`CuCargoHost`, `Placement`), units::cu_misc, units::uc_body (`UcHost`, `ee_units_uc_body`),
units::uc_produce (`UcRust`, `FindPlacement`, `ee_units_uc_produce`); each with its tests (op sequences over
recording mocks: 18 + 13 + 11 + 8 + 7 + 5).

Gates (origin/rust a84903ce6 merged): builds and the test profile 0/0, oracle 2-7 identical, Rust gate PASS (5,360
tests), the twelve capture profiles PASS, saveload 18/18 and the matrix PASS (exit 0), scenario_check 27/27, sim_editor
and `--civbuilder` PASS, eedata check and cppgate PASS, the tour PASS, gates.sh GATES PASS: see PORT_LEARNINGS.md
"The units' lane, round 6".

## Round 7 (branch `rust-units7`, from round 6's tip 5e4f3833f, 2026-11-20/21)

The brief: C++ logic lines removed (target -3,000) by round 6's method (the body in Rust, each C++ statement one case
of its file's host, the capture-note calls C++ statements in place and order, a recording-mock test per module). The
round ran as three forks in their own worktrees (A: the EEUnitClass family; B: EEComplexUnit, the doppelgangers, the
ghosts, EEUnit; C: the unit utilities, the cloaking utilities, EEGatherNextTree, EEPhysics), merged with `--no-ff`
(no conflict), then merged with the world lane's round 1 (`rust-world1`, 76b3a54af), which had moved EEPhysics
wholly (`ee_physcpp`, ee_game physics.rs): world's EEPhysics.cpp/.h won, and fork C's EEPhysics part (phys_body.rs
and `ee_units_ph_host`) was dropped (nothing else called it).

| Commit | What | Logic (per file, rust-world1 -> tip) |
|---|---|---|
| d06d1b54c, e153dcb20, af601dd28 (A) | EEUnitClass: the creation family (CreateUnitAtTile/AtPoint, BaseCreateUnitAt, BaseCreateUnit), ReplantFarm, CreateRemainsUnit, LaunchShot, LaunchFromTurret, UnitDied, FindSafeLocation, MoveToTileCentre (uc_create.rs); Animate, FireOnAttackFrame, the turrets, trails and ripples, HealGarrison, Regenerate, the regions (uc_anim.rs); the class media and buttons, the destructor (uc_media.rs). EEUnitClassBuildings: the release choice, ReleaseUnit, the creations, UpdateArmed (ucb_release.rs). EEUnitClassShots: the projectile, bomb and missile creations and launches (ucs_shots.rs). EEUnitClassPeople: UpdateCitizen, the priests' conversion, the temple's effects, the step timers (ucp_people.rs). EEUnitClassGround (ucg_ground.rs), EEUnitClassTransports (ucx_transports.rs). uc_body's UpdateBuilding/UpdateAnimal call the moved bodies directly | EEUnitClass 1,263 -> 535, Buildings 728 -> 347, People 422 -> 145, Shots 395 -> 270, Ground 257 -> 149, Transports 239 -> 128 |
| c6c911e2a, b36576033, f856391b5, 2c2c3b2c8, 458f2bc0b, 11a80cd77, 8ec9a55ee (B) | RepairPointers (dop_repair.rs), the load's container sections and live state (dop_load.rs: a Rust allocation applied at the first tick), the doppelganger's Load/Save/Initialize (dop_file.rs); CopyFrom, ResetEEComplexUnit, Init, FreeNode, SwapGraphic, StepAside, UpdateFanaticismColor, GetChecksum (cu_life.rs) and ~36 more EEComplexUnit bodies (cu_world.rs); OutOfSynchDump, Describe (cu_dump.rs); EEUnit's TakeHit (eu_hit.rs: CBTH, CBDE and the morale records stay C++ statements), MoveTo, Kill, CanGather, FindAreaTile, SetHitPoints, DepleteResource (eu_body.rs); the placement ghosts (ghost_body.rs, ghost_make.rs); EEDoppelgangerManager (dm_body.rs) | EEComplexUnit 1,280 -> 448, EEComplexDoppelganger 720 -> 248, EEGhostUnit 535 -> 98, EEDoppelgangerManager 271 -> 21, EEUnit 450 -> 182 |
| 42931276f, 61f816360, b5614d4d0 (C) | EEConvertUnit with its adaptation grant (the shuffle one C++ statement), EEReplaceUnit, EEMakeUnitCopy, EEMakeBuilding, EEReplaceQueuedTechnology, EEWorkFarm, EERetargetAttack, EEFindNewTargetNearOld, EEUpgradeHitPoints, EEMovePassengers, EETakeOverLoad, EEFindTargetNear, EEFindNearbyTarget, EECanBallisticHit, EECanPriestConvert, EEPreviewWall, the space energy walls and more (uu_body.rs, host `ee_units_ub_host`); the cloaking utilities whole (uu_cloak.rs); EEGatherNextTree (uu_gather_tree.rs) | EEFOFindNewTargetNearOldTarget 1,287 -> 884, EECloakingUtilities 203 -> 11, EEUGRust 285 -> 246 |

Round total, the units' contribution measured on the merged tree (logic_lines.py over src, include/ee, include/lle;
rust-world1 -> the tip, so EEPhysics, which world moved, is not counted): logic 65,109 -> 60,486 (**-4,623**); lines
156,163 -> 153,426 (-2,737). With fork C's dropped EEPhysics part the forks alone measured -5,180 (5e4f3833f -> the
fork merge); -557 of that was EEPhysics.cpp and is world's now. Shims added: about 2,000 one-statement host cases
(A ~590 and 88 one-call members, B ~908, C ~335 without the dropped `ee_units_ph_host`'s ~225); shims deleted: 9
(A 4: UpdateBuilding/UpdateAnimal's HealGarrison, Animate, RefreshSound, FireOnAttackFrame; B 5: CLEAR_CONTACTS,
RELEASE_HELD_UNITS, CLEAR_BLOCKING, CLEAR_LAST_RESOURCE, CLEAR_BUILD_TARGETS).

New hosts: `ee_units_uc_call`/`ee_units_uc_create_at` and the per-file ucb/ucs/ucp/ucg/ucx hosts (each passing unknown
ops on to `ee_units_uc_host`), `ee_units_dop_host`, `ee_units_eu_host`, `ee_units_ghost_host`, `ee_units_dm_host`,
`ee_units_ub_host`, `ee_units_ck_host`, `ee_units_gt_host`. New Rust (ee_sim::units): uc_create, uc_anim, uc_media,
ucb_release, ucs_shots, ucp_people, ucg_ground, ucx_transports, uc_mock (a shared recording mock), dop_repair,
dop_load, dop_file, cu_life, cu_world, cu_dump, eu_hit, eu_body, ghost_body, ghost_make, dm_body, uu_body, uu_cloak,
uu_gather_tree; each with op-sequence tests.

Gates on the final tree (72451b917: the forks, rust-world1 7f68eb2bc and the CBLS/CBLT fix; origin/rust 439543b4b is
contained): both builds 0/0 (EE-AOC 413 inputs, EE-SIM 414) and the test profile 0/0; the Rust gate PASS (5,619
tests, no flake this time); the oracle seeds 2-7 identical (6087, 5300, 6072, 7649, 7842, 6540 lines; before and after
the fix); the twelve capture profiles at seeds 2-3 PASS (oracle 212, late 214, space 210, calamity 222, units 175,
transport 185 on the fixed build; effects 6, missile 22, app 26, scenario 43, groups 2, placement 4 on the merged
build, which the tag fix does not reach); the plain saveload 18/18 identical (exit 0); the matrix `--settings all
--seeds 2-3 --at 1000,1900 --expect tools/saveload_expect.txt` exit 0, `saveload expect: PASS`; scenario_check
27/27; sim_editor PASS (14,448 lines) and `--civbuilder` PASS (4,062); eedata check and cppgate PASS; through
slot.py on the main checkout's tree, the tour `--exercise random-game --box-select --orders --seed 7` RESULT PASS
(gather, repair, queued repair, move; patrol and attack-ground skipped by the fixture); `gates.sh` GATES PASS (tour;
ai-game time-cap PT 12471, units [4, 38, 39], alive_computers [2, 3], `engines ['classic_lua', 'classic_lua']`;
lan-game PT 2718/2718). The saveloads, scenario_check, the editor gates and eedata ran on the merged build before
the tag fix (it changes only a TEST ONLY capture record's tag).

## Round 8 (branch `rust-units8`, from round 7's tip 77c7ac956, 2026-11-21/22)

The brief: C++ logic lines removed (target -3,000) by round 7's method, the files round 7 left. The round ran as three
forks in their own worktrees with disjoint files and disjoint host op ranges (A: the EEUnitClass family, UC 600-999,
UCB 1100-1199, UCS 1230-1399, UCA 2000-2199, UCK 2200-2399; B: the unit objects, CU 1000-1499, DOP 400-699, EU
200-399, GOAL 800-1099, UNIT 200-499 and the new US/UH/UAI/EUM hosts; C: the unit utilities and combat hosts, UB
300-699, WORLD 100-299, UPD 340-380 and the new SR/LF/CG hosts), merged with `--no-ff` without a conflict, then
merged with origin/rust 5143a9696 (batch 15; only PORT_LEARNINGS.md conflicted). EEPhysicsModel was left to the
world lane (its round 2, rust-world2y dae093b54, moves it).

| Commit | What | Logic (per file, 77c7ac956 -> tip) |
|---|---|---|
| bb629020a, e118fc3f4, 18e8a970f, 18626e2f7, 000f7603e, 66054376a, f57fcf858 (A) | EEUnitClassAir's bodies (uca_air.rs: FlyToGoal, GetGroundHeight, EELandableActionOperation, IsBombOnTarget, EECreateCarrierAircraft, FlyOffMap, the air animal's, planet spaceship's and helicopter's bodies); EEUnitClassMoveOp's compound cases split (the cls hosts); the gates and the buildings' remaining bodies (ucb_gate.rs); EEUnitClass's constructor fill and CreateUnitClasses (uc_class.rs); the shots' impact tests (ucs_impact.rs); the kind dispatchers' compound bodies (uck_kinds.rs) | EEUnitClassAir 503 -> 66, EEUnitClass 535 -> 301, Buildings 347 -> 138, Shots 270 -> 138, Kinds 266 -> 185 |
| b262bd0c0, 408e01939, 3e37b5189, 28381a1da, 0679319f6, be5f17f31, 0c9ee696a, 03353434c (B) | AttackNearby and the trigger host's compound cases (uai_cond.rs); EEUnitSaveOperation's sections and loads, the waypoint and goal-queue loads, the doppelganger's Reset/Clear/RepairPointers, FinishLoad (dop_save.rs, dop_more.rs); GoalOp's BOARDING, SURROUNDING_TILES, NEXT_RESOURCE, RESOURCE_OF_KIND, MINING_BEAM (goal_more.rs); the TEST ONLY store check whole (us_check.rs, USCK written from Rust); the unit host's multi-statement bodies (uh_body.rs); EEUnit's Remove, CopyFrom, Inits, ResetEEUnit, LoadMedia, RegrowForest, SetLastAttacker (eu_more.rs over `ee_units_eum_host`, no preamble: Init runs before mPlayer is set); QueueResearch with TQRS, ResizeTurretAngles, AddCenterOffset, GetEffectHeight, PlaceOnGround, the facings (cu_more.rs) | EEUnitHost 291 -> 25, EEComplexDoppelganger 248 -> 86, EEComplexUnit 448 -> 319, EEUnitAI 156 -> 30, EEUnit 182 -> 87, EEUnitStore 170 -> 77, EEUGRust 246 -> 169 |
| 5a60be9b3, 642b9c2e7, 308dc4e90, e580fd118, 30612d9cf (C) | EEWorldOp's and EEUpdateAct's compound cases split (combat cg_world.rs, units upd_split.rs); the target searches, the line of fire, the uu world host's compound cases and the formula fills (`ee_units_sr_host`, `ee_units_lf_host`); EECombatCondition, EECombatStep, their reads and step operations, CBGS (`ee_units_cg_host`); EEUpdateContact | EEFOFindNewTargetNearOldTarget 884 -> 349, EECombatGoals 471 -> 223, EEUnitUpdate 185 -> 53 |
| (the merge fix) | the air animal's and the landable's sound parameters call an unnoted Doppler pitch (`ee_units_uu_doppler_pitch_unnoted`, `EEUnitClass::AirGetSoundParameters`): fork A had pointed them at `EEGetDopplerPitch`, whose UUTL note (kind 83) the original's inline bodies never wrote; the calamity capture's seed 2 had one extra call at tick 11401 | +4 |

Round total (logic_lines.py over src, include/ee, include/lle; 77c7ac956 -> the forks' merge 4ef8cd8e5, so batch 15
is not counted): logic 60,308 -> 57,339 (**-2,969**; the fix adds 4); lines 153,434 -> 151,018 (-2,416). Target
-3,000: short by about 35. Shims added: about 1,000 one-statement host cases (A ~360, B ~330, C ~300); shims
deleted: the uu searches' old two-call protocols and `EEComplexUnitLoadSection`.

New Rust (ee_sim::units unless noted): uca_air, uck_kinds, uc_class, ucb_gate, ucs_impact, uai_cond, dop_save,
dop_more, goal_more, us_check, uh_body, eu_more, cu_more, upd_split, combat::cg_world and the search, line-of-fire
and combat-goal host modules; each with recording-mock op-sequence tests. New capture tags written from Rust and
pinned by tests: USCK (0x4b435355), TQRS (0x53525154).

Gates on the final tree. The full chain ran on the merge eabdfe150 (the forks and origin/rust 5143a9696); the
calamity failure it found was fixed in 5e3cdadaa, and the gates that fix can reach were run again on that tip.
- Builds: both 0/0 (EE-AOC 412 inputs, EE-SIM 413) and the test profile 0/0.
- Headers: 305 current.
- Rust gate: PASS (6,080 tests). On the merge it failed once on ee_audio's real-time test
  `the_null_device_pulls_the_mix_in_real_time` under load; it passed on the rerun.
- Oracle seeds 2-7: identical (6087, 5300, 6072, 7649, 7842, 6540 lines), on the merge and on the fix.
- Captures at seeds 2-3 on the merge: oracle 212, late 214, space 210, units 175, effects 6, missile 22, app 26,
  scenario 43, transport 185, groups 2, placement 4. Calamity failed there (UUTL, seed 2) and PASSES on the fix
  (222). Oracle and space were run again on the fix: PASS.
- Saveloads: the plain run PASS; the matrix `--settings all --seeds 2-3 --at 1000,1900 --expect` PASS.
- Editors and data: scenario_check 27/27; sim_editor PASS (14,448 lines) and `--civbuilder` PASS (4,062); eedata
  check and cppgate PASS.
- The tour `random-game --box-select --orders --seed 7`: RESULT PASS on the merge and on the fix (gather, repair,
  queued repair, move; patrol and attack-ground skipped by the fixture).
- `gates.sh` on the merge: GATES PASS (tour; ai-game time-cap PT 13196, units [4, 41, 40], alive_computers [2, 3],
  `engines ['classic_lua', 'classic_lua']`; lan-game PT 2689/2689).
- The fix changes only which C++ calls the same Doppler math (no TEST ONLY note now), so the saveloads, the editor
  gates, eedata and gates.sh were not run again on it.

## Round 9 (branch `rust-units9`, from round 8's tip 3cae25114, 2026-11-22)

The brief: C++ logic lines removed (target -3,000) in the lane's files, by round 8's method, plus the lane's constant
enums. Three forks ran in their own worktrees with disjoint files and host op ranges (A: the EEUnitClass family, ops
3000-3999; B: the unit objects, ops 4000-4999; C: the unit utilities, the combat queries and goals, ops 5000-5999),
merged with `--no-ff` (one trivial mod.rs conflict, the generated headers regenerated). The lead moved the lane's enums
to Rust and merged origin/rust's batches 16, 17 and 18.

| Commit | What | Logic |
|---|---|---|
| a653c7f91, 0eff7ee42 (lead) | the unit constants Rust's: EEUnitClassIDs.h's enums (AI kinds, categories, families, unit types, the movement, kind, hidden and media flags), EEUnitAction.h's action and trigger ids and EEUnitGoal.h's goal kinds are `#[repr(C)]` enums of ee_sim::unit_ids that cbindgen emits back as plain C++ enums (rust/sim/unit_ids.h, LLE round 10's method); the header readers (eedata rules.py/rulesgen.py, the editor and scenario constant tests, script.rs, header_consts.rs) read the generated header | -752 |
| 32c61811c .. 0f291f631 (A) | the dead constants and declarations; the sea and ambient classes (ucg_sea.rs); the remaining class bodies (uc_rest.rs: GetElevationFactor, the sound parameters, FindTargetNear, GetContainerContinent, the tile and registry bodies, EruptVolcano, ...); the class bodies' host's compound cases (cls_host.rs); the kind dispatchers (uck_dispatch.rs); EEShotOp's compound cases (combat shots.rs over EEShotOp64); FacesTarget, BaseGetSurroundingTiles, Upgrade::Init, the UCBD note (ucbd_note.rs) | class family 1,330 -> 642 (-688) |
| d0b7f6510 .. d0adacb3d (B) | EEDoppelganger and the manager's Release (dm_dop.rs); the ghosts' media, Reset, Load, Save, AddChecksum (ghost_more.rs); EEWheel (cu_wheel.rs); Describe/DescribeLong (eu_describe.rs); AIStep whole with its USST notes, the AI host table replaced by `ee_units_ai_host`; the sight and sunk cases split; 12 small EEComplexUnit bodies (cu_small.rs); the store's Fill (us_fill.rs); the AI script file read (`ee_units_script_host`); the dump fill; EEUnit's and EEComplexUnit's constructors and destructor | -474 |
| 8fe7db0cc .. ac7429727 (C) | the combat queries' entry points over one trampoline (combat cg_query.rs, the host-read scope a Rust thread_local); the goal accessors and the update entry points (goal_calls.rs, upd_entry.rs); the unit utilities' adapters (uu_adapt.rs); EECombatGoals.h's capture code enums Rust constants | -528 |
| b098958de, 754676fe1, 42e3e6cc1, 22f9ff8b8 (lead) | the AI host table's stale declarations; header_consts.rs reads unit_ids.h; EEGoalPointOf over batch 18's renamed EeGe3DPoint fields; rustfmt | -3 |

Round total (logic_lines.py over src, include/ee, include/lle; the units' commits alone): logic **-2,445**
(the enums fed74b2a7 -> a653c7f91 -740; the forks, the goal kinds and the cleanup 2fcab79b2 -> b098958de -1,705; on
the final tree d968d89bd -> HEAD 32,816 -> 30,371). Target -3,000: short by about 555. Per file (3cae25114 -> the
forks' merge, other lanes' files left out): EEUnitAction.h 431 -> 9, EEUnitClassIDs.h 321 -> 0, EEComplexUnit 319 ->
164, EEUnitClassGround 149 -> 9, EEUnitClass 301 -> 165, EEFOFindNewTargetNearOldTarget 349 -> 224, EECombatGoals 223
-> 101, EEUnitClassKinds 185 -> 76, EEUnitGoal.h 149 -> 56, Transports 128 -> 37, People 145 -> 55, EEDoppelganger 74
-> 4, EEGhostUnit 98 -> 39, EEUGRust 169 -> 110, EEUGRustCombat 86 -> 27, Shots 138 -> 79, EEUnitAction.cpp 62 -> 12,
EEUnit 87 -> 46, Buildings 138 -> 98, EECombatGoals.h 76 -> 39, EEUnitStore 77 -> 47, EEWheel 33 -> 5,
EEUnitUtilities.h 77 -> 50, EEUnitUpdate 53 -> 36, Upgrade 40 -> 26, EEComplexDoppelganger 86 -> 73, EEUnitAI 30 ->
17, EESightRings 48 -> 40, Air 66 -> 58, EEDoppelgangerManager 21 -> 16, EEUGRust.h 10 -> 8, EEUnit.h 745 -> 743.

New Rust (ee_sim unless noted): unit_ids (the enums), units ucg_sea, uc_rest, cls_host, uck_dispatch, ucbd_note,
dm_dop, ghost_more, cu_wheel, eu_describe, cu_small, us_fill, goal_calls, upd_entry, uu_adapt; combat cg_query and
shots' compose; each with recording-mock op-sequence tests.

Gates on the final tree (22f9ff8b8: the forks, origin/rust d968d89bd = batch 18):
- Builds: both 0/0 (EE-AOC 412 inputs, EE-SIM 413) and the test profile 0/0; headers current (312).
- Rust gate: PASS, 6,469 tests (after the rustfmt fix 22f9ff8b8; the chain's run on 42e3e6cc1 failed on rustfmt only).
- Oracle seeds 2-7: identical (6087, 5300, 6072, 7649, 7842, 6540 lines).
- Captures at seeds 2-3: all twelve PASS (oracle 212, late 214, space 210, calamity 222, units 175, effects 6,
  missile 22, app 26, scenario 43, transport 185, groups 2, placement 4 digests).
- Saveloads: the plain run PASS; the matrix `--settings all --seeds 2-3 --at 1000,1900 --expect` PASS.
- Editors and data: scenario_check 27/27; sim_editor PASS (14,448 lines) and `--civbuilder` PASS (4,062); eedata
  check and cppgate PASS.
- The tour `random-game --box-select --orders --seed 7`: RESULT PASS (gather, repair, queued repair, move; patrol and
  attack-ground skipped by the fixture).
- `gates.sh`: GATES PASS (tour; ai-game time-cap PT 12241, units [4, 37, 38], alive_computers [2, 3],
  `engines ['classic_lua', 'classic_lua']`; lan-game PT 2780/2774).
- The rustfmt commit 22f9ff8b8 changes a test's formatting only; the other gates ran on 42e3e6cc1.

## Round 10 (branch `rust-units10`, from round 9's tip 0e7decbd3, 2026-11-23)

The brief: about -2,000 logic lines, whole classes and Rust-owned data over more forwarder splitting. Three forks
ran in their own worktrees, with disjoint files and host op ranges:
- A: the class family, EEUnitClassRecord.h and EEUnit.h's EEUnitClassUpgrade/EEUnitClass parts, ops 6000-6999;
- B: the unit objects and EEUnit.h's preamble and EEUnit/EEComplexUnit parts, 7000-7999;
- C: the unit utilities, goals and combat queries, 8000-8999.

They merged with `--no-ff` (only cbindgen.toml conflicted). Then origin/rust 49707901b (batch 20) was merged.

| Commit | What | Logic |
|---|---|---|
| a7956761f (A) | the unit class record is the Rust `DbUnitClass` (ee_data tables.rs) emitted as EEUnitClassRecord; overlay.py reads it; ee/EEUnitClassRecord.h deleted | -247 |
| 4560ff4c6, 99015e1bc (A) | about 188 one-statement EEUnitClass and EEUnitClassUpgrade members inline in ee/EEUnit.h | |
| 85c77ad2b (A) | NavalBuildingHasSunk and COND_SUNK one Rust function (cls_condition.rs) | |
| a6716711b, 0048a3624, 696d77e8f, 8f75c8ed8 (A) | the UCBD-noted members (ucn_noted.rs), the shots' ActionChanged/Update steps, the kind predicates (uc_is.rs); rustfmt | A total -678 |
| 492c9bbae, f676f1e6b (B) | EEUnit's, EEComplexUnit's, EEGhostUnit's, EEComplexDoppelganger's one-statement members inline | |
| cdf29ce5b (B) | the unused save-key statics, the store's dead enums, EESelectPick; EEUnitStoreWrote is EEUnitStoreMoved | |
| 235cbb872, 59d9f260f (B) | the units' pointer members in their Rust records; the layout derives their offsets (EEWorldLayout's offsetof lines gone) | |
| d6dd7e003, 667fa0e6d, b1b04ad3e, 5d706e9b1 (B) | the ghost's and doppelganger's plain members Rust records (obj_fields.rs); FindAreaTile; cu_inline.rs | B total -372 |
| 1e9ad666d, ecd18ca3b, 6cafea7ac (C) | the utilities', goals' and combat queries' entry points inline; EEUnitHost.h's dead extern block; the hosts' preambles; CreateGoal, the UUTL record, the region walk and FindImpactUnit's region read in Rust; EEUnitRegions' records deleted | C total -354 |
| lead | EECombatGoals.h's dead shot declarations; the merges, headers | -5 |

Round total (logic_lines.py, 0e7decbd3 -> 156f43c46, the forks merged before origin/rust): logic **-1,409**
(30,371 -> 28,962). The lane's files went 3,618 -> 2,212. Net C++ lines -3,552 (+1,677 / -5,229), Rust +2,276 / -36.
Target -2,000: short by about 590.

Per file (logic, 0e7decbd3 -> 156f43c46):

| File | Before | After |
|---|---|---|
| EEUnit.h | 743 | 302 |
| EEUnitClassRecord.h | 247 | 0 |
| EEFOFindNewTargetNearOldTarget.cpp | 224 | 135 |
| EEWorldLayout | 166 | 146 |
| EEUnitClass | 165 | 159 |
| EEComplexUnit | 164 | 150 |
| EEUGRust | 110 | 73 |
| EECombatGoals | 101 | 68 |
| Buildings | 98 | 45 |
| Shots | 79 | 46 |
| Kinds | 76 | 24 |
| EEComplexDoppelganger | 73 | 64 |
| Air | 58 | 24 |
| EEUnitGoal.h | 56 | 37 |
| People | 55 | 39 |
| EEUnitUtilities.h | 50 | 29 |
| EEUnitHost.h | 47 | 3 |
| EEUnitStore | 47 | 41 |
| EEUnit | 46 | 40 |
| EEGhostUnit.h | 43 | 14 |
| EEGhostUnit | 39 | 30 |
| EEUnitStore.h | 39 | 12 |
| EECombatGoals.h | 39 | 13 |
| EEUnitCollector.h | 38 | 26 |
| Transports | 37 | 26 |
| EEUnitUpdate | 36 | 21 |
| EEGoalQueueContainer | 33 | 25 |
| EEComplexDoppelganger.h | 32 | 2 |
| EEUGRustCombat | 27 | 16 |
| EEUnitRegions.h | 18 | 0 |

Gates on the final tree (63df6887e: the forks plus origin/rust 49707901b = batch 20):
- Builds: both 0/0 (EE-AOC 412 inputs, EE-SIM 413) and the test profile 0/0; headers current (327).
- Rust gate: PASS.
- Oracle seeds 2-7: identical (6087, 5300, 6072, 7649, 7842, 6540 lines).
- Captures at seeds 2-3: all twelve PASS (oracle 212, late 214, space 210, calamity 222, units 175, effects 6,
  missile 22, app 26, scenario 43, transport 185, groups 2, placement 4 digests).
- Saveloads: plain PASS; the matrix `--settings all --seeds 2-3 --at 1000,1900 --expect` PASS.
- Editors and data: scenario_check 27/27; sim_editor PASS (14,448 lines) and `--civbuilder` PASS (4,062); eedata
  check and cppgate PASS.
- The tour `random-game --box-select --orders --seed 7`: RESULT PASS (BOX SELECT PASS; ORDERS gather, repair,
  repair-queued, move; patrol and attack-ground skipped by the fixture).
- `gates.sh`: GATES PASS (tour; ai-game time-cap PT 12098, units [4, 36, 37], alive_computers [2, 3],
  classic_lua x 2; lan-game PT 2936/2929).
- Not run: the POSIX builds.

## Round 11 (branch `rust-units11`, from round 10's tip b04764c69 with batch 21, 2026-11-23/24)

The brief: about -1,200 logic lines and real class deletions. The round was stopped twice by a usage limit and once
by a full disk; the lead was replaced mid-round and the three forks were resumed from uncommitted edits. Forks, in
their own worktrees, with disjoint files and host op ranges:
- A: the class family and EEUnit.h's EEUnitClass/EEUnitClassUpgrade parts, ops 9000-9999;
- B: the unit objects, the store, the goal queue, EEUnit.h's EEUnit/EEComplexUnit parts, 10000-10999;
- C: the unit utilities, goals and sight rings, 11000-11999.

They merged with `--no-ff` (cbindgen.toml, layout.rs and the generated world.h conflicted). Then origin/rust
17682d86c (batch 22) was merged.

| Commit | What | Logic |
|---|---|---|
| abfcc2047 (A) | the unit class's and the upgrade's pointer members are the Rust records `EeUnitsUnitClassLinks` and `EeUnitsUnitClassUpgradeLinks` (bases, typed through cbindgen renames); the layout derives four offsets | -59 |
| 8e05fb509 (A) | EEUCNote deleted (EETurnGatePart, GetPart in ucn_noted.rs); FindImpactUnit, FacesPoint, FireShot, Attacked with their CBQR/CBXA records in ucn_noted.rs; the constructor's member-pointer and record-id tables in unit_class_fields.rs; EEUnitClassImprovementTable and its entry deleted (`mImprovements` is `EeDataUpgradeFields*`); 15 one-call members inline | -182 |
| ff04b6663 (B) | the goal queue's Load, Save and per-tick refresh in Rust over the C++ file (goals.rs); EEGoalQueueContainer.cpp deleted | |
| 7a6718990 (B) | the unit store's entry points in Rust (us_fill.rs) | |
| c264d9e34, 0ab3afdc9 (B) | EEDoppelganger's and the manager's one-call bodies inline, EEDoppelganger.cpp deleted; EEWheel.cpp's bodies in EEComplexUnit.cpp, EEWheel.cpp deleted | |
| 8c8edfad9, aa0d84bd2 (B) | EEProductionEntry and EEComplexUnitAttacker are the Rust records `EeUnitsProductionEntry` and `EeUnitsIgnoredTarget`; the layout derives the build entry's and the goal's offsets | |
| ad73efbc0, e4d6fecc9 (B) | the complex doppelganger's field reads, writes and copies in Rust (dop_file.rs); the units' six Load/Save bodies over one helper, EEUnitFile | |
| 4c97324ae, 8c5153d53, 836fc108f, 332598ad1 (B) | eleven one-call EEComplexUnit members inline; EEGoalDeque trimmed; EEComplexUnitGrowthLess a lambda; rustfmt | B total -180 |
| 2a959dffa (C) | GoalOp's file calls in Rust through CppFile (bool read/write added to the FSFile calls table); EEUnitGoal derives from `EeUnitsGoalBase`; EEHitPointUpgrader deleted; EEGetRandomInt, the wall piece orientation and EEPickWallPieces over Rust | |
| 2c0e26cc5, 4d29fd82b (C) | the sight calls' capture scopes in three helpers; an unused picker member and a dead declaration; rustfmt | C total -77 |
| lead | the merges, the regenerated world.h, one clippy allow and rustfmt (1ae2125e5) | 0 |

Round total: the lane's files (perfile.py, b04764c69 -> the forks' merge 9a580dbb2) **2,177 -> 1,679 (-498)**; the
whole tree (logic_lines.py, 56a3ada9d -> 9a580dbb2) 22,337 -> 21,804 (-533). Net C++ +351 / -1,404, Rust +1,554 /
-191. Target -1,200: short by about 700.

Classes (classes_tree.py, 56a3ada9d -> 9a580dbb2): 714 -> 707. Gone: EEUCNote, EEUnitClassImprovementTable,
EEUnitClassImprovementEntry, EEComplexUnitAttacker, EEProductionEntry, EEGoalQueueIO, EEGoalDeque::const_iterator;
also EEHitPointUpgrader and EEComplexUnitGrowthLess, which the script does not count.

.cpp files in `src/Empire Earth/Units`: 31 -> 28 (EEGoalQueueContainer.cpp, EEDoppelganger.cpp, EEWheel.cpp).

Per file (logic, b04764c69 -> 9a580dbb2): EEUnit.h 302 -> 197, EEUnitClass.cpp 159 -> 95, EEComplexUnit.cpp 150 ->
138, EEWorldLayout.cpp 146 -> 137, EEFOFindNewTargetNearOldTarget.cpp 135 -> 110, EEUGRust.cpp 73 -> 55,
EEComplexDoppelganger.cpp 64 -> 53, EEUnitStore.cpp 41 -> 8, EEUnitVectorItem.h 43 -> 17, EESightRings.cpp 40 -> 17,
EEUnitClassImprovement.cpp 39 -> 15, EEUnitClassBuildings.cpp 45 -> 25, EEUnitClassKinds.cpp 24 -> 6,
EEGoalQueueContainer.cpp 25 -> gone.

Gates on 1ae2125e5 (the forks, origin/rust 17682d86c): the builds and the test profile 0/0; headers current (334);
Rust gate PASS; oracle seeds 2-7 identical; the twelve capture profiles PASS at seeds 2-3; saveloads plain and
matrix (`--expect`) PASS; scenario_check 27/27; sim_editor and `--civbuilder` PASS; eedata check and cppgate PASS;
the tour `random-game --box-select --orders --seed 7` PASS; gates.sh GATES PASS. Not run: the POSIX builds.

## Round 12 (branch `rust-units12`, from round 11's tip 2a53f648c with batch 23, 2026-11-24)

The brief: about -1,000 logic lines: EEUnit.h's containers and value members, the files round 11 did not start
(EECombatGoals, EEActiveGroup, EEGroup, the picker, the handle, the set manager, the melee screen, the goal queue's
LoadGoal), the layout's units offsets, and the goal context cbindgen never emitted. Two forks (A: EEUnit.h, the unit
objects, the update host, ops 12000-12999; B: the combat queries, the groups, the unit utilities, ops 13000-13999) were
stopped by a usage limit after their first leg; the resumed lead merged them (`--no-ff`; cbindgen.toml's list
unioned, units.h regenerated) with origin/rust bbb0654d2 (batch 24), could not relaunch them (the session's subagent
limit) and did the second leg alone.

| Commit | What | Logic |
|---|---|---|
| 44639686d (A) | EEComplexUnit's small bodies in Rust (u12a_small.rs: the pool, the location setters, SetScaledModel, IsInReach, SetOwner, ClearHolder, HasEffect) or inline; the collision tile and the three effect handles fields of `EeUnitsComplexUnitFields` (`EEEffectHandle` is `EeUnitsEffectHandle`, a cbindgen rename) | -115 |
| d405415d9, dc0ed520d, f0ee3d679 (A) | the update host's context is the Rust record; the goal queue's copy in Rust (goals.rs); rustfmt, the header | -18 |
| bd93ca658 (B) | the combat queries' footprint read, turret part and compound reads without C++ bodies (cg_query.rs) | -37 |
| b5a80794c, 79cd43b85 (B) | the active group host's footprint, closest point, goal location and new location, the group host's path goals in Rust (ag_host.rs, group_host.rs); the area outlines as `U2DRectangle<long>`s | -60 |
| 6e50c4e1e, 003b5be48, 5389e9393 (B) | the unit utilities' tile lists read in place, the surrounding tiles and the tile line, EEGetUnitsInRectangle's walk, tile test and sort in Rust (uu_host.rs, uu_ffi.rs); the wall rise's box top | -73 |
| 5d55492d4, a3dabb7e5 (B) | one-statement bodies in EEUnitKind.h, EEUnitHandle.h, EEUGRustCombat.cpp, EESightRings.cpp; clippy and rustfmt | -16 |
| 8a03ec39b (lead) | `EeUnitsGoalContext` emitted (it belongs in cbindgen.toml's `[export] include`, not `[parse] include`); EEUGRust.cpp's mirror struct gone, a size assert | -4 |
| 29a7c01d3 (lead) | the melee screen's test and its CBFS capture record in Rust (combat/screen.rs `ee_units_melee_screen`); the context, the distance callback and EEMeleeScreen gone | -14 |
| 838893ba3 (lead) | the unit sets' ForEach walks in Rust (app/unit_sets.rs `ee_unit_set_for_each`, `ee_unit_sets_for_each`) | -5 |
| 66c88c697 (lead) | the unit handle's typed halves are casts in ee/EEUnitHandle.h (RTSUnit is the first base at offset 0); the overloads in EEUnit.cpp and RTSUnit.cpp gone; FromSavedID one statement | -18 |
| f36b7491f (lead) | EEUnitGoal::IsActive and AddLoadUnits in Rust (goal_calls.rs) | -8 |
| aacabef72, dd2ded36d (lead) | SetInitialContactLocation, ContinueProduction inline; x/y scratch statics as arrays; EESortUnique one body; EEGetConversionCost one statement | -16 |

Round total: the lane's files (perfile.py, 3b7cd6eff -> dd2ded36d) **1,679 -> 1,295 (-384)**: A -133, B -186, the
lead -65. The whole tree (logic_lines.py) -394 without batch 24's own change (3b7cd6eff 18,496 -> the forks' merge
5ee5420f3 18,167; the batch-24 merge 81d8780e8 18,159 -> dd2ded36d 18,094). Target -1,000: short by about 600.

Classes (classes_tree.py, 3b7cd6eff -> dd2ded36d): 672 -> 669 (EEEffectHandle, now the Rust record; EEUUUnitIDLess and EEUUUnitIDEqual, the sort in Rust). .cpp files in `src/Empire Earth/Units`: 28 -> 28.

Per file (logic, 3b7cd6eff -> dd2ded36d): EEUnit.h 197 -> 143, EEComplexUnit.cpp 138 -> 80,
EEFOFindNewTargetNearOldTarget.cpp 110 -> 63, EEUnitClass.cpp 95 -> 81, EECombatGoals.cpp 68 -> 31, EEUGRust.cpp
55 -> 51, EEActiveGroup.cpp 53 -> 12, EEGroup.cpp 48 -> 29, EEUnitHandle.h 31 -> 14, EEUnitSetManager.h 19 -> 14,
EEUnitGoal.h 28 -> 20, EEUnitCollector.h 26 -> 0, EESightRings.cpp 17 -> 13, EEWorldLayout.cpp 137 -> 134.

Gates on dd2ded36d: the builds and the test profile 0/0; headers current (347); Rust gate PASS; oracle seeds 2-7 identical; the twelve capture profiles PASS at seeds 2-3; saveloads plain and matrix (`--expect`) PASS; scenario_check 27/27; sim_editor and `--civbuilder` PASS; eedata check and cppgate PASS; the tour `random-game --box-select --orders --seed 7` and the editor PASS; gates.sh GATES PASS (ai-game units [4, 34, 36], alive_computers [2, 3], classic_lua twice; lan-game). Not run: the POSIX builds.

## What is left (behaviour)

- **Round 12's measure** (perfile.py at dd2ded36d, lane total 1,295): EEUnit.h 143 (the containers, the points and
  orientations, `mPatrolGroup`: their users are spread over the hosts and other lanes' files), EEWorldLayout.cpp 134
  (other lanes' offsets but a few), EEUnitClass.cpp 81, EEComplexUnit.cpp 80, EEFOFindNewTargetNearOldTarget.cpp 63,
  EEComplexDoppelganger.cpp 53, EEUGRust.cpp 51 (WonderStartedChat, the sGoal statics, NewLoad's sort/unique),
  the class family files 15-38 each, EEUnitPicker.h 32 and .cpp 19, EECombatGoals.cpp 31 (EEQueryContext),
  EEGhostUnit.cpp 30, EEGroup.cpp 29, EEUnitUtilities.h 29; the rest under 28. Almost all of it is host preambles,
  scratch statics and declarations other lanes call; items save 1-10 lines each. Not attempted: the goal queue's
  LoadGoal, EEUcAlignYaw, the containers of EEUnit.h.


- **Round 11's measure** (perfile.py at 9a580dbb2, lane total 1,679):
  - EEUnit.h 197 (declarations whose bodies need types incomplete there or live in other lanes' files; the
    containers and value members `mIgnoredTargets`, `mBuildQueue`, the points, orientations, effect handles and
    `mPatrolGroup`, whose users are spread over the utilities and EESim.cpp);
  - EEComplexUnit.cpp 138, EEWorldLayout.cpp 137 (almost all offsets of other lanes' classes: EEWorld, EEPlayer,
    EEMap, EETile), EEFOFindNewTargetNearOldTarget.cpp 110 (the ub/sr/lf hosts' statics, UUTiles/UUPairs, the deposit
    trace), EEUnitClass.cpp 95 (host preambles, scratch statics, the melee screen's bodies, EEUcAlignYaw),
    EECombatGoals.cpp 68 (untouched: EEQueryContext, the turret callback, EEQueryIntersects), EEUGRust.cpp 55
    (WonderStartedChat, the sGoal statics, NewLoad's sort/unique), EEComplexDoppelganger.cpp 53;
  - EEActiveGroup.cpp 53, EEGroup.cpp 48, EEUnitPicker.h 32, EEUnitHandle.h 31, EEUnitSetManager.h 19: not started;
  - the rest under 40 each. Most items left save 1-10 lines.
  - Not attempted: the goal queue's LoadGoal beyond the queue's Load, EEFOFindNewTargetInMeleeRange's bodies,
    EECombatGoals.

- **Round 10's measure** (perfile.py at 156f43c46, lane total 2,212):
  - EEUnit.h 302 (the declarations other lanes call, the containers and C++-typed members);
  - EEUnitClass.cpp 159, EEComplexUnit.cpp 150, EEWorldLayout 146 (the registrations of members that are not yet
    record fields), EEFOFindNewTargetNearOldTarget.cpp 135 (WonderStartedChat, EEGetRandomInt, RTSMap::IsEdgeTile,
    the wall orientation, EEHitPointUpgrader, the hosts' scratch statics), EEUGRust 73 (GoalOp's file reads: they need
    u32 and bool reads in the FSFile calls table, File System.cpp, game core's), EECombatGoals 68 (the multi-statement
    query cases: SHOT_CLASS, TAG_COUNT, the facing turret, Intersects, FootprintTile), EEComplexDoppelganger 64;
  - Buildings 45, Shots 46, People 39, EEUnitStore 41, EEUnit 40, EEUnitGoal.h 37, EEGhostUnit 30, EEUnitUtilities.h
    29, EEUnitClassKinds.h 29, Transports 26, EEUnitCollector.h 26, EEGoalQueueContainer 25 (Load/Save/LoadGoal),
    Kinds 24, Air 24, the rest under 25.
  - The goal queue's file reads and EEFOFindNewTargetInMeleeRange's bodies (in EEUnitClass.cpp) were not attempted.
- Hosts' one-statement cases that wrap across lines count as logic in logic_lines.py; they were not reformatted.

- **Round 9's measure** (logic_lines.py's per-file count after the forks' merge): EEUnit.h 743 (declarations and
  one-line accessors whose callers are other lanes'), EEFOFindNewTargetNearOldTarget.cpp 224 (the uu host callbacks
  marshalling C++ vectors and tiles, the sr/lf/ub hosts' preambles and file statics, EEComputeSoundParameters over the
  camera and viewport, EEGetRandomInt over URandom, the wall piece orientation, the deposit case's computer-player
  trace, RTSMap::IsEdgeTile), EEWorldLayout 166 (the C++ layout registry: stays), EEUnitClass.cpp 165, EEComplexUnit.cpp
  164 (GetModel, the SetLocation family and IsInReach left as hot or scope-bracketed paths; the hosts' preambles and
  scratch statics), EEUGRust 110 (the goal context, ReadInto and the file helpers, WonderStartedChat, CreateGoal,
  NewLoad's sort/unique), EECombatGoals 101 (the query context and its callbacks, the turret callback), Buildings 98,
  Shots 79, EEUnitClassKinds 76, EEComplexDoppelganger 73 (Load/Save bodies building C++ save contexts), Air 58,
  EEUnitGoal.h 56, People 55, EEUnitUtilities.h 50, EEUnitStore 47 (EEUnitStoreWrote), EEUnit 46, EESightRings 40,
  EEGhostUnit 39, EECombatGoals.h 39, Transports 37, EEUnitUpdate 36, EEGoalQueueContainer 33 (its callbacks, not
  attempted), EEUGRustCombat 27, Upgrade 26. EEUnitClassRecord.h (247) stays C++: tools/eedata/overlay.py reads it as
  the record's layout source.
- Hosts' one-statement cases that wrap across lines count as logic in logic_lines.py; they were not reformatted.

- **Round 8's measure** (logic_lines.py's per-file count at the tip): EEFOFindNewTargetNearOldTarget.cpp 349,
  EEComplexUnit.cpp 319 (EECuDumpFill, EEInsuranceChat, EEReportLeakingUnits, AIStep and its note, IsInReach, the
  hosts' preambles), EEUnitClass.cpp 301, EECombatGoals.cpp 223, EEUnitClassKinds 185, EEUGRust 169 (the helpers,
  GoalOp's preamble; PATH_REACHABLE is the Paths lane's), EEWorldLayout 166 (untouched), EEUnitClassGround 149,
  EEUnitClassPeople 145, EEUnitClassBuildings 138, EEUnitClassShots 138, EEUnitClassTransports 128, EEGhostUnit 98,
  EEUnit 87 (Describe/DescribeLong, the eu preamble), EEComplexDoppelganger 86 (the field switches), EEUGRustCombat
  86, EEUnitStore 77 (Fill and the bridges), EEUnitClassAir 66, EEUnitUpdate 53, EEUnitAI 30 (COND_SUNK).
  EEPhysicsModel and EEPhysicsSweep are the world lane's (its round 2).
- **A shared utility is not a twin of an unnoted body**: when two inline bodies equal a noted unit utility
  (EEGetDopplerPitch, UUTL kind 83), call the utility's math without its note, or the capture counts a call the
  original never made (round 8's calamity seed 2).
- **Never hand an `EEUnitClass*` straight to a fields-taking Rust function**: the static_cast to the base shifts the
  pointer (fork A's one crash); read the field through a host case.

- **Round 7's measure** (logic_lines.py's per-file count at the tip): EEFOFindNewTargetNearOldTarget.cpp 884 (the
  search and line-of-fire host callbacks with their CB* records, EESearchTarget and its siblings, EEFindNewTarget,
  the uu host's compound world cases), EEUnitClass.cpp 535 (the constructor's fill, CreateUnitClasses,
  EEUnitClassMoveOp's compound cases, the FacesTarget/FacesPoint/FindImpactUnit capture wrappers,
  GetElevationFactor, BaseGetSoundParameters, FindTargetNear, GetContainerContinent), EEUnitClassAir 503 (untouched:
  EELandableActionOperation, FlyToGoal, the sound and air-animal bodies), EEComplexUnit.cpp 448 (the AIStep notes,
  QueueResearch with its TQRS record, PlaceOnGround, the facing transforms, GetEffectHeight, AddCenterOffset,
  EEReportLeakingUnits, EEInsuranceChat, the hosts' preambles; the Paths lane's `ee_units_wp_host`),
  EEPhysicsModel 403 (not started), EEUnitClassBuildings 347 (the gates, FindNearestTargetBuilding,
  NavalBuildingHasSunk, EruptVolcano), EEUnitClassShots 270 (the impact tests, EEShotOp), EEUnitClassKinds 266
  (untouched), EEComplexDoppelganger 248 (EEUnitSaveOperation's compound save sections and load sets, the waypoint
  and goal-queue load sections, FinishLoad), EEUGRust 246 (GoalOp's BOARDING, LINE_TO_UNIT_BLOCKED, MINING_BEAM,
  CAN_CAST_AT, SURROUNDING_TILES, NEXT_RESOURCE, RESOURCE_OF_KIND; PATH_REACHABLE is the Paths lane's), EEUnit 182
  (RegrowForest, Init, Describe, Remove), EEUnitStore 170 (TEST ONLY check), EECombatGoals' EEWorldOp compound cases
  and EECombatGoalHostOp's TELL_GROUP, POSITION, VERIFY_TARGET, EEUnitUpdate's EEUpdateAct. EEPhysics is the world
  lane's (moved wholly in its round 1). The TEST ONLY EEPhysicsSweep host can now call uc_create.rs's creation.
- **A host list kept as a static must be cleared where the original made a fresh local** (round 7 fork C's two
  oracle crashes): the uu searches append to the list they are handed.

- **Round 6's measure** (the logic left in the lane's files, logic_lines.py's per-file count): EEComplexUnit.cpp ~1,500
  (CopyFrom, ResetEEComplexUnit, SwapGraphic, StepAside, ReplaceUnits, GetChecksum, OutOfSynchDump, the area effects,
  FreeNode, UpdateFanaticismColor with its function statics, the cu host's preamble), EEUnitClass.cpp ~1,320 (Animate,
  LaunchShot, BaseCreateUnit/BaseCreateUnitAt, ReplantFarm, LaunchFromTurret, MoveToTileCentre, HealGarrison and
  Regenerate (their CBTM/CBHL/CBRG captures stay C++ calls), the media and button loads, EEUnitClassMoveOp's non-path
  cases), EEFOFindNewTargetNearOldTarget.cpp ~1,320 (EEConvertUnit: its adaptation block is strings, a shuffle and a
  goto; EEReplaceUnit; the search and line-of-fire hosts), EEUnitClassBuildings 728, EEComplexDoppelganger 720
  (RepairPointers, the load sections), EEPhysics 692, EEGhostUnit 535, EEUnitClassAir 513, EECombatGoals' EEWorldOp
  (~60 compound lines: HIT_CLASS, DAMAGE_IN, TILE_SEEN, ATTACKED_GOAL, HELP, UNIT_EFFECT), EEUnitUpdate's EEUpdateAct
  (mostly one statement already; the notes at its top would have to stay per original code), EEUnitClassShots' EEShotOp.
  Each moves the same way as this round's: the body in Rust, each C++ statement one case of the file's host.
- GoalOp's compound cases left: PATH_REACHABLE (the path manager; the Paths lane's), CAN_CAST_AT, BOARDING,
  SURROUNDING_TILES, LINE_TO_UNIT_BLOCKED, MINING_BEAM, NEXT_RESOURCE and RESOURCE_OF_KIND (their list marshalling);
  EECombatGoalHostOp's TELL_GROUP, POSITION, VERIFY_TARGET.


- **Round 5's measure**: the TEST ONLY drivers and the bodies over Rust callees are gone. What stays in the hosts
  (GoalOp ~480 lines, EEUpdateAct 377, EEWorldOp ~230, ee_units_ag_host 230, EEUnitClassMoveOp 210, the trigger
  host ~185, EEShotOp 203) is each a call of another class's C++: `StartAction`/`ActionChanged`, the path manager
  (`RequestPath`, `FindPath`, `CancelPaths`: AI lane), sounds and effects with the class's media, the calamity
  classes, the tech tree and the player (`Pay`, `CanProduce`, `AddResource`: Game core), `CanGetTo` and
  `AreConnectedForUnit` (AI lane's C++), the waypoints deque, the ignored-targets deque and the class's virtuals.
  The class bodies (ProduceUnit, ReleaseUnit, Animate, Heal, TakeHit, Kill) each need ten or more of those as host
  operations: a third of their lines saved, and the most simulation risk per line.
  Small cases still answerable in Rust: the uu continent host's cases 0-4, 6-11 (inline wrappers over ee_astar),
  GoalOp's NODE_INFO (needs the epoch's GetBuildTime), the unit sweep host's group ops (the unit host's
  `ee_units_host_*` already make groups).

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
