# The unit classes, data and calamities cluster (`src/Empire Earth/Units/EEUnitClass*.cpp`, Database, Tech Tree, Calamities, Scenario Manager, Map, Random Map)

The lane's state (one file per lane; the round's story is PORT_LEARNINGS.md "Unit classes, data and calamities
cluster, round 1"). It follows the calamities and scenario manager lane (docs/progress/calamities-scenario.md) and the
R5b.4 rounds that collapsed the 36 `EEUC*` classes into one `EEUnitClass` keyed by its kind (PORT_LEARNINGS rounds
32-35). The Rust homes are `ee_sim::units` (`cls_*.rs`), `ee_sim::tech`, `ee_sim::scenario`, `ee_data`.

## The measure (round 1, at origin/rust 8d68d2786)

| Area | Lines | What is left |
|---|---|---|
| Units/EEUnitClass*.cpp | 9,301 | the class's members, its bodies (world glue: units created, goals cloned, effects, sounds) and four hosts (`ee_units_cls_host`, `ee_units_cz_host`, `EEShotOp`, `EELandableActionOperation`) |
| Database | 1,797 (+507 headers) | thin loaders over ee_data's records (`EEDbReadRecords`, `EEDbLoadList`), the media lookups |
| Tech Tree | 2,052 | `EETechHost` and the walks' adapters (the tree's state and walks already ee_tech's), the TEST ONLY capture (791) |
| Calamities | 3,363 | see calamities-scenario.md |
| Scenario Manager | 4,755 | see calamities-scenario.md |
| Map | 1,469 | the map's C++ members, the mini map (UI), the sight capture (TEST ONLY) |
| Random Map | 1,392 | the PIM steps, the generator list, the state's set-up (the game passes already ee_rmap's) |

The record-only parts of these classes were already Rust (the class's fields `EeUnitsUnitClassFields`, the dispatch
table, the conditions, the transports' and buildings' searches, the tech nodes' fields and walks, the db records).
What remains is world glue: each statement a call into a C++ unit, goal, player, effect or the path manager. A body
moves by becoming Rust over a host of one-line C++ operations; the yield is the body's lines minus the host's.

## Done (round 1, branch `rust-classes1`: -1,397 net C++ lines)

- **Includes**: the unneeded project includes of the unit-class, database, tech tree, map and random map units
  (-412).
- **The movement bodies** (cls_move.rs, cls_move_host.rs): `FindPath`, `NextWaypoint`, `PathToGoal`, `PathToward`,
  `AddWaypointAt`, `BaseHasArrived`, `Flee`, `FleeAttacker`.
- **The record-only answers** (cls_answers.rs): `CanEnter`, `GetNumGoalQueues`, `SharesRallyPoint`,
  `SharesResearch`, `CanMoveOnTerrain`, `GetPassableFlags`.
- **The landing aircraft's flight plans** (cls_carrier.rs): `LandableReturnToAirport`, `RouteDockWaypoint`,
  `ApproachCarrier`, `TakeOffFromCarrier`, `IsAwayFromGoal`, the airplane's circling side and holding pattern.
- **The ground levelling** (cls_level.rs): `LevelGround`, `CreateRunways`, the four `LevelRunway*`.
- **The status ticks** (cls_status.rs): `UpdateCamouflageTimer`, `RegeneratePower`, `UpdateCloakDetection`,
  `UnitDamaged`.
- **C++ dedupe**: `CreateUnitAtTile` and `CreateUnitAtPoint` share one body; `EEHighlightGateWalls`' four
  branches are one loop.
- **A fix found by the oracle capture profile**: `GetDestination` reads its out point as an input (the
  goal's `PointOut`); `FindPath` hands it the point `GetGoalLocation` wrote, as the C++ did.

The hosts are one C++ switch: `ee_units_cls_host` (EEUnitClassTransports.cpp) passes the new operations
(`EE_UNITS_CLS_MV_*`, `_CR_*`, `_LV_*`, `_ST_*`) to `EEUnitClassMoveOp` (EEUnitClass.cpp, where the path manager's
and the screens' headers are).

## Next

- The remaining `EEUnitClass.cpp` bodies by size: `ProduceUnit` (194, nearly all world glue: the placement, the
  goal copies), `Animate` (108: the sounds; its clock is Rust), the creation family (`BaseCreateUnit`,
  `BaseCreateUnitAt`, the media loads), `Heal`, `MoveToTileCentre` (a transformation across the host), the effects
  (`UpdateTrail`, `UpdateRipples`, `CreateRipples`: drawn only, tour-only).
- EEUnitClassAir.cpp: `FlyToGoal`, `IsBombOnTarget` (the ballistic arc: `SetOrientationYPR` needs a Rust twin
  checked against the C++), the sound parameters (tour-only).
- EEUnitClassBuildings.cpp: `ReleaseUnit` (152, goal copies), the gate builders (`EEBuildGate`, `GateGateReplaced`,
  `EEHighlightGateWalls`).
- The lead's other targets for this cluster: `EECalamity`'s members into its record, the scenario elements whole
  (the editor gate's in-place reads first), the map and the random map's state. Not started this round.

## The units' state lane, round 1 (branch `rust-unitstate1`, 2026-11-19: -485 net C++ lines)

The state layer of this cluster and of the units (the sister lane, units behaviour, moves the bodies). Story and
gates: PORT_LEARNINGS.md "The units' state lane, round 1".

| Commit | What | Net C++ |
|---|---|---|
| 283cc3d35 | the unit save's ids, maxima, floats, vector and contact sections and the loads' plain stores in place (units/unit_save_views.rs); layout entries for the complex unit's pointer members, vectors, contact list, collision tile, scale, frame, effect handles, and the class's projectile class and 29 effect pointers | -101 |
| 21e43a611 | `EEAirTrafficControl` and `EEPPatrolGroup` deleted: `EeUnitsAtc` (atc.rs, atc_views.rs) and `EeUnitsPatrolGroup` (patrol_group.rs) are the objects their holders keep | -329 |
| 32c0609d9 | `EEActiveGroup`'s plain members as the record `EeUnitsActiveGroupFields` (units/ag_fields.rs); the group host's plain cases in place | -57 |
| 4efa9f10a | two includes after the merge (rust_includes.py) | +2 |

Found while measuring: `EEUnitClass` has no subclass hierarchy left (R5b.4 rounds 32-35) and its plain data is the
record `EeUnitsUnitClassFields` already; the cross-lane `->mUnitClass->mX` reads are record reads in place, so the
accessor sweep was skipped (agreed with the lead). What keeps the class is its bodies and its non-record members
(effects, sounds, models, the AI script, the projectile and death classes).

Next for the state layer: `EEGroup`'s plain bodies over `GroupView`, the path request map as a Rust object, then the
group classes; the save's deques (they need Rust containers first); the class's media and effect members as Rust
handles; the map's remaining C++ members are the graphics lane's terrain mesh and vertex colours.

## Gates

See PORT_LEARNINGS.md "Unit classes, data and calamities cluster, round 1".
