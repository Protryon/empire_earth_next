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

## What is left (behaviour)

- GoalOp (~85 cases) is world calls into classes that are still C++: the patrol groups (their points are C++
  vectors), the calamity classes and manager, the transport and gate class virtuals, the path manager and the
  continent manager (AI lane), the effects, sounds and UI notices, the goals' own C++ fields (`mB1C`, `mPathAttempts`,
  `mIsActive`: no layout entry), the goal queues (C++ deques). Each needs its callee in Rust or a layout entry from
  the state lane first.
- The remaining gather helpers (`EEGatherFindSpot`, `EEGatherNextTree`, `EEGatherAccessible`) need the continent
  manager's path subsectors, forests and terrain flags (AI lane, C++) and `CanGather`/`HasGatherPoint` (UCBD-noted
  class calls): about eight new host operations for ~120 lines.
- The class bodies (`ProduceUnit`, `Animate`, `Heal`, the creation family) call the effects manager (ee_render's,
  which ee_sim may not call), sounds, the UI and goal clones: each needs a host op per statement (the 3:1 yield the
  earlier rounds met).
- `EECombatCondition`/`EECombatStep` have C++ callers in the class kinds (torpedo, projectile, missile): a Rust
  read host for them waits for those class bodies.

## What is left (from round 2)

- `EEComplexUnit`'s remaining bodies (Kill, CopyFrom, SetGroupPathGoals, StepAside, FindContacts, the cargo
  calls) are world-call sequences (effects, sounds, the path manager, goals, the unit classes' virtuals): each moves
  as a Rust body over a host trait only with the classes it calls, the pattern of units/hidden.rs.
- The goal host (`GoalOp`, ~770 lines) is almost all world calls now (actions, goal objects, the transport and
  calamity classes' virtuals, files); its remaining plain reads need layout members (`mWaypoints`, the goal queues,
  the patrol group's points, the class's projectile pointer).
- The unit host (EEUnitHost.cpp, ~1,000 lines) is the game's world calls only (round 2 moved its dry branches); the
  rest goes with `EEActiveGroup` (its Keep/Spread methods forward to ag.rs already; the host could call ag.rs
  directly once the group's handle is the Rust record's) and the patrol groups.
- `EEActiveGroup`, `EEGroup`, `EESelectedGroup`, `EEPPatrolGroup`, `EEAirTrafficControl`: their bodies are Rust
  (ag.rs, group.rs, patrol_group.rs, atc.rs); the C++ classes keep the members their many holders read (players,
  the UI, the scenario effects, the messages). Moving the state into Rust records is the next whole-class step;
  their callers span the app, UI and scenario clusters.
- The save operation (`EEUnitSaveOperation`, EEComplexDoppelganger.cpp ~550 lines) needs the pointer members in
  the layout; then saveload 18/18 and the matrix gate it.
- The TEST ONLY drivers (EEGroupCommandSweep's case builder, EEActiveGroupSweep, EEPhysicsSweep) read tech trees,
  calamity classes and class virtuals: Rust only once those are views.
- Small: `EEIsSeenBy`'s host case (needs the combat world host), `EEGetAttackPart`/`EECanHeal` in the combat goal
  host (the handle conventions differ: combat's are `EEComplexUnit*`, the utilities' `EEUnit*`), `EEPlanWall` and
  `EECanPlaceOnTile` (the unit host's game paths; their Rust entry points take the C++ vector).
