# Paths (the path slice across the AI, Units and Game core lanes)

The lane's state (one file per lane; each round rewrites it; the rounds' stories are in PORT_LEARNINGS.md "The paths
lane"). The lane exists because the Units, AI and Game core lanes stalled on each other's C++: the units' hosts call
the path manager, the path manager calls unit and goal methods. It moves the path slice end to end, wherever it
lives: EEPathManager.cpp's host, the waypoint deque, CanGetTo / AreConnectedForUnit, the hosts' path cases.

From round 1's middle the owner's rule applies: a body moves to Rust even when it calls other C++, each call a
one-statement C++ shim (an `extern "C"` case, no logic). The score is C++ logic lines removed.

## What is where (round 1, branch `rust-paths1`)

- `rust/crates/ee_sim/src/units/waypoints.rs`: **the waypoint deque whole in Rust**. `EeUnitsWaypoints` (a
  `VecDeque` of `EeUnitsWaypoint`, the C++ `EEComplexUnitWaypoint`'s layout) is owned by `EEComplexUnit::mWaypoints`,
  now an `EEWaypointList` (ee/EEUnit.h: the handle and the deque members its readers use, `size`, `[]`, `back`,
  `clear`, `push_back`, assignment). Rust finds a unit's list through the world layout (`complex_waypoints`). The
  bodies: `AddWaypoint`, `NextWaypoint`, `DropPath`, `GetPathStart`, `GetPathLength`, `GetNextWaypointPoint`,
  `GetSecondWaypoint`, `SnapLastWaypoint` (the goal's location and `PathTaken` through `ee_units_goal_call`),
  `TakePath` with the classes' `AdjustPath` (the naval citizen's and priest ship's thinning, `EEAdjustNavalPath`
  through `ee_units_uu_naval_path` and its UUNT note, the sea animal's depths through a C++ shim: the physics'
  random stream), `MergeWaypoints`, `FindWaypointAtTile`, `ShouldMoveToNextAutoWaypoint`, `SetGroupPathGoals`, the
  trigger host's waypoint step, the landing aircraft's path check (`EE_UNITS_ACT_FLY_CHECK_PATH`). The C++ members
  are inline forwarders; their C++ calls are `ee_units_wp_host` (EEComplexUnit.cpp: SetGoal, the first queue's
  Insert, length and last goal, RefreshGoals, RefreshActiveGoal, StartAction, ActionChanged, IsHuman, the bounded
  tiles' clear, RefreshPath, gEEAIRegionSize, EELocationGoalPoint, the map's closest tile).
- `rust/crates/ee_sim/src/astar/cm_unit.rs`: **CanGetTo, AreConnectedForUnit, GetFootprintContinent** with their CNTQ
  records (kinds 6, 13), the inner queries through the manager's own entry points; C++ keeps the space-map flag and
  an aircraft's missing planet id (`ee_astar_cm_unit_host`).
- `rust/crates/ee_sim/src/astar/pm_entry.rs`: **FindPath and FindBoundedPath** with their PMRQ records (kinds 3, 4):
  Rust lists out (the searches append to `Vec`s, no C++ vector op); the game's manager is `pm::global()`. The C++
  `EEPathManager::FindPath` (the storm, the classic_lua wall planner) is a copy over `ee_path_find_path`;
  `FindBoundedPath` is gone (its one caller is Rust).
- `rust/crates/ee_sim/src/astar/pm_apply.rs`: **RequestUnitPaths, the request count, the path-done flag, the
  capture's unit facts, ClampTile** (the C++ calls through `ee_units_wp_host`, the request's path through
  `EE_PATH_REQUEST_SET_PATH`, the map's closest tile through `EE_PATH_CLOSEST_TILE`).
- `astar/pm_host.rs`: the path manager host's unit operations answered in Rust (`unit_path_op`: the waypoints, drop,
  take, snap, the auto-waypoint and goal tiles, the pending unit, the graph goal's terrain flags).
- `units/gather_spot.rs` `class_terrain_flags`: **EEUnitClass::GetTerrainFlags** (`ee_units_terrain_flags`; the C++
  member is one call).
- The hosts' path cases in Rust: the uu continent host's cases 0-13 (`uu_host.rs` over the manager's record and
  entry points; `ee_units_uu_host_continent` deleted), `ee_units_uu_host_find_path`, the combat world host's
  CAN_GET_TO_UNIT, CONTINENT_AT, CONTINENT_LINK, CONNECTED_FOR_UNIT, FIND_PATH (`world_views.rs reach_op`), the
  class hosts' waypoint operations (`cls_host.rs waypoint_op`), the carrier's CanGetTo, the movement host's
  FindBoundedPath, MergeWaypoints and CancelPaths, the trigger host's AUTO_WAYPOINT, WAYPOINT_STEP, WAYPOINTS, the
  update host's CANCEL_PATHS, UNIT_NEXT_WAYPOINT, FLY_CHECK_PATH and R_WAYPOINTS (noted as the C++ act host noted
  them), the physics model host's four waypoint reads.

## What is left (in this slice)

- **EEAutoPathRequest's data** (EEPathManager.cpp's request glue: `EENewLoadedRequest`, `EERequestRecordOf`, the
  REQUEST_LOAD/NEW/SAVE cases, `EESaveAutoPathRequest`/`EELoadAutoPathRequest`, `EESetRequestPath`; about 90
  lines). Its readers are the Groups lane's (EEGroup.cpp's `ee_units_group_host` cases on rust-groups1:
  PATH_TILES/DONE/ID/REQUEST/CANCEL/SAVE_PATH/LOAD_PATH/AWAIT_PATH), EECPNextUnits.cpp (the classic_lua routes and
  wall paths), EEUGRust.cpp (PATH_REACHABLE), EEEMStorm.cpp (the storm's path) and EEComplexUnit::RefreshPath. The
  design for the next round: a Rust record (the request's tiles, flags, ends, ids, length, done) behind the
  `EEAutoPathRequest*` handle with accessor functions (`ee_path_request_tiles`, `_done`, `_id`, ...), so each of
  those cases becomes a plain call; then the load/save glue and the C++ class go.
- The owners awaiting a loaded request (`EEAwaitAutoPathRequest`: `std::function`s), the FSFile glue of the save
  parts, the dumps' `fwrite`, `EEPathWorldUnit` (`gWorld.GetUnit`), `UnitStranded`, `Rebuild`.
- The goal host's PATH_REACHABLE (EEUGRust.cpp: a request's last tile), EECPNextUnits.cpp's path cases, the storm's
  `TakePath` and `FindPath` (Game core), EEActiveGroup's CanGetTo call (Groups lane).
- One-statement host reads still C++: the combat query's WAYPOINTS, the herd's waypoint reads, EEUGRust's waypoint
  reads, EEPlayer.cpp's and EECalamityUpdate.cpp's NextWaypoint loops (other lanes' files).

## Round 1's numbers

C++ (src, include/ee, include/lle) 200,059 -> 199,139: net -920; code lines deleted 969, added 124 (one-statement
shims, inline forwarders, `EEWaypointList`, the C++ FindPath's copy). The rounds' table and lessons: PORT_LEARNINGS
"The paths lane, round 1". Gates on the merged tree (a80082034): both builds and the test profile 0/0; the Rust gate
PASS (5,077 tests); the oracle seeds 2-7 identical; the twelve capture profiles at seeds 2-3 PASS; the plain saveload
18/18 identical (exit 0); the matrix `--expect` PASS (exit 0); scenario_check 27/27; sim_editor and `--civbuilder`
PASS; eedata check and cppgate PASS; the tour with `--box-select --orders --seed 7` PASS; gates.sh GATES PASS.

Call-site edits in other lanes' files: EEWorldLayout.cpp (Groups: one line, `complex_waypoints`),
EERenderObject.cpp (Game core: the waypoint pointer const). Groups-side cases that become plain calls once the
request's data is Rust (next round): EEGroup.cpp's `ee_units_group_host` EE_UNITS_GROUP_OP_PATH_TILES, _DONE, _ID,
_REQUEST, _CANCEL, _SAVE_PATH, _LOAD_PATH, _AWAIT_PATH (rust-groups1) and EEComplexUnit::RefreshPath's
`EEGroup::GetUnitPath`.
