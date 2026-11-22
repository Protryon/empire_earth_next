# Paths (the path slice across the AI, Units and Game core lanes)

The lane's state (one file per lane; each round rewrites it; the rounds' stories are in PORT_LEARNINGS.md "The paths
lane, round 1" and "round 2"). The lane exists because the Units, AI and Game core lanes stalled on each other's C++:
the units' hosts call the path manager, the path manager calls unit and goal methods. It moves the path slice end to
end, wherever it lives: EEPathManager.cpp's host, the waypoint deque, CanGetTo / AreConnectedForUnit, the path
requests, the hosts' path cases.

The owner's rule: a body moves to Rust even when it calls other C++, each call a one-statement C++ shim (an
`extern "C"` case, no logic). The score is C++ logic lines removed.

## What is where

Round 1 (branch `rust-paths1`):

- `ee_sim::units::waypoints` (waypoints.rs): the waypoint deque whole in Rust behind `EEComplexUnit::mWaypoints`
  (`EEWaypointList`, ee/EEUnit.h) and its bodies (AddWaypoint, NextWaypoint, DropPath, TakePath and the classes'
  AdjustPath, MergeWaypoints, SnapLastWaypoint, SetGroupPathGoals, ...); the C++ they call is `ee_units_wp_host`
  (EEComplexUnit.cpp).
- `ee_sim::astar::cm_unit` (cm_unit.rs): CanGetTo, AreConnectedForUnit, GetFootprintContinent (CNTQ kinds 6, 13).
- `ee_sim::astar::pm_entry` (pm_entry.rs): FindPath and FindBoundedPath with their PMRQ records.
- `ee_sim::astar::pm_apply` (pm_apply.rs): RequestUnitPaths, the request count, the path-done flag, ClampTile.
- `astar/pm_host.rs` `unit_path_op`: the path manager host's unit operations.

Round 2 (branch `rust-paths2`):

- `ee_sim::astar::request` (request.rs): **EEAutoPathRequest whole in Rust.** A request is a Rust record (the
  saved record's fields: ends, player, id, length, done, the walker flags and their player, and the found path's
  tiles) with its reference count, behind the opaque `EEAutoPathRequest*` (ee/EEAutoPathRequest.h is now one
  forward declaration). The path manager's host operations on requests (NEW, REFERENCE, DEREFERENCE, SAVE, LOAD,
  the path set of RequestUnitPaths) are answered in pm_host.rs. The C++ reads a request through
  `ee_path_request_done`, `_tiles`, `_reference`, `_dereference`, `_cancel` (rust/sim/astar.h). Also there: the
  owners waiting for a loaded request (`EEAwaitAutoPathRequest`: a `BTreeMap` of Rust closures by request id, the
  resolve's four steps of `EE_PATH_AWAIT`, the resolve queued once through `ee_path_after_load_last`, the one C++
  statement `EEAfterLoadLast(f)`), the Rust callers' `request_path` / `cancel`, and the goal host's reach test
  (`path_reachable`, was EEUGRust.cpp's PATH_REACHABLE).
- The group host's path cases are plain Rust calls (group_host.rs: the request, cancel, references, done, id,
  tiles, the save and load of a group's requests through the player save's file primitives, the loaded request's
  wait as a closure over `group_obj::path_resolved`, the terrain flags through `class_terrain_flags`).
- `EEComplexUnit::RefreshPath` (waypoints.rs `refresh_path`: the active group's request for the unit completed
  while not done, then the unit's own path with two waypoints or fewer), with ee/EEPlayer.h's `GetUnitPath` and the
  manager's `CompleteAutoPath` / `CompletePath` inline members gone (their callers are Rust).
- ai.wall_path, ai.wall_route, ai.wall_route_result (ai units_more.rs): the wall path's FindPath (pm_entry.rs) and
  the routes' requests held by the Rust units state (`UnitsState::routes`), not EECPNextUnits.cpp's C++ map.
- `EEAIContinentManager::Rebuild` (pm_host.rs answers EE_PATH_REBUILD over cm_passes.rs) and `UpdateWallUnit`
  (cm_unit.rs `update_wall_unit` with its CNTQ kind 24; the player units' caller is Rust); the C++
  `EEContinentQueryCapture` is gone with its last user.
- The herd's path request cancel (cp herd.rs: `request::cancel_and_release`; the herd's request is never set).
- The storm's request reads (EEEMStorm.cpp: done, the tiles taken reversed, the dereference) through the accessors.
- The movement host's PathToGoal request (cls_move_host.rs `request_path`: the enemy avoidance modifier, the goal's
  attempt, `RequestUnitPath`; the C++ keeps only the player's virtual `AvoidsEnemies`) and its tile direction.

## What is left (in this slice)

- EEPathManager.cpp: the FSFile glue of the manager's three save parts (`EEPathFileWrite` / `EEPathFileRead`: the
  storage bytes need a byte primitive the player save's file has not got), the dumps' `fwrite`, the
  `EEPathManager::FindPath` copy (its last C++ caller is the storm's Begin), `Call`, the closest-tile and
  stranded-unit shims, `EEPathWorldUnit` (gWorld.GetUnit: kept, the registry's lookup may differ for dying units).
- EEAIContinentManager.cpp: BuildRegions' scenario areas (the scenario manager's EESMArea objects: Scenario lane),
  `ee_astar_cm_unit_host` (the space map flag, the aircraft's planet id with its trace), `EEAIForest::RegrowTrees`
  (the physics' random stream and `CreateUnit`; its caller EEUnit.cpp is the Units lane's).
- The storm's `Begin` and `RequestRoute` (Game core: the whole storm can move once its effect is Rust).
- The movement host's other cases (EEUnitClass.cpp `EEUnitClassMoveOp`): the goal fields (mB1C/mB1D, the path
  attempts, the sidestep count, the group path state) need the goal base's layout in Rust; the unit reads
  (mBlockingUnit) need a layout offset; the virtuals (GetDestination, GetGoalLocation, AvoidsEnemies, IgnoreTarget,
  the melee retarget) stay shims.
- The one-statement continent and path reads in other lanes' hosts (EEUnitClassPeople.cpp, EEComputerPlayer.cpp,
  EECalamity*.cpp, EEFOFindNewTargetNearOldTarget.cpp, EEActiveGroup.cpp's CanGetTo, EEComplexDoppelganger.cpp's
  GetPathGoal / AddPending), and EEComplexUnit's continent bodies (FindAreaTile, IsOnOtherContinent: the Units
  lane's class bodies).

## Round 2's numbers

See PORT_LEARNINGS.md "The paths lane, round 2".
