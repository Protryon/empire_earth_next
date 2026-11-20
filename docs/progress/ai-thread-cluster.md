# AI Thread cluster (the A* layer, the continent manager, the path manager, the computer player, the herd, the town)

The lane's state (one file per lane; each round rewrites it; the rounds' stories are in PORT_LEARNINGS.md "AI Thread
cluster"). From 2026-11-18 the AI Thread's C++ moves by clusters of classes that call each other, whole classes
into Rust with their state as Rust records. The cluster's files: everything left in src/Empire Earth/AI Thread,
their headers in include/ee, the AI Rust in ee_sim (ai, cp, astar). It took over the earlier lanes' files: R5b.1
(docs/progress/r5b1-a-pathfinding-continents-choke-points.md), lane B (docs/progress/ai-computer-player.md) and
lane A (the Lua AI host).

## What is where

Round 1 (rust-aicluster2): the A* layer whole in `rust/crates/ee_sim/src/astar/` (flood.rs, path_search.rs,
graph_search.rs, search_note.rs; C interface `rust/sim/astar.h`).

Round 2 (rust-aicluster3), in `rust/crates/ee_sim/src/cp/` (C interface `rust/sim/cp.h`): `herd.rs` (**EEHerd
whole**), `town.rs` (**EETown's bodies**), `lists.rs` (**the resource centres whole**, the favourite history, the
units to kill, the danger zones); EECPCombatManager deleted.

Round 4 (rust-aicluster4), in `rust/crates/ee_sim/src/astar/`:

- `world_view.rs`: **EEAIAStarView whole**. The A* searches' view of the world is a Rust record
  (`EeAStarWorldView`: the map, quiet or not) and its callbacks (`ee_astar_world_view_callbacks`): every tile,
  unit and class fact through the world views (`crate::world`), the continent manager's facts from its Rust record
  (registered once: `ee_astar_world_view_set_manager`), the ASRN note of each answer of a loud view. Still C++
  calls: a unit's `IsPassableBy` (`ee_units_pm_host_passable_by`), the manager's `GetPathAreaAt` and `GetHillAt`
  (`ee_astar_view_cm_*` in EEAIContinentManager.cpp: they write CNTQ records) and the physics stream's draw. The C++
  class is a header record (ee/EEAIAStarView.h); EEAIAStarView.cpp deleted. `EEAIHasResourceUnit` is an inline over
  `ee_astar_tile_resource_kind`.
- `choke.rs`: the choke point pass's grids are Rust's (`ee_astar_choke_grids_build/_tile/_free`; the three C++
  globals gone).
- `modifier_obj.rs`: **the path goals' modifiers and route planners whole** (EEAIPathGoalModifierBase,
  EEAIBoundedPathGoalModifierBase, the sea and space modifiers of both goals, EnemyIsAttackableByUnit,
  WallsDoNotBlock, the wall pather, the storm shaper; EEAIGraphPathGoalModifierBase, the enemy avoidance, the wall
  planner, the storms' rain and snow planners): one Rust object `EeAStarModifier` (a reference count, the goal
  records it was attached to, its kind and members), made by `ee_astar_modifier_new_*` and dropped by
  `ee_astar_modifier_release`; the searches (path_search.rs, graph_search.rs, pm.rs's saved searches) call it
  directly. The sub sectors' gate tests (`IsPassable`, the wall planner's `IsGateOpen`), `EEAIGraphHasPassableLink`
  and `AreLinksBlocked` read the sub sectors' records and the tiles' units. Still C++: a unit's reference count and
  the world's influence table (`ee_astar_mod_host_*`, what is left of EEAIAStarBoundedPathGoalModifiers.cpp, with
  the gates' `IsSidePart`). Deleted: EEAIPathGoalModifierBase.h, EEAIBoundedPathGoalModifierBase.h,
  EEAIPathGoalModifiers.h, EEStormModifiers.h, EEAIPathSubSector.cpp.

Round 5 (rust-aicluster5), the continent manager and its objects, in `rust/crates/ee_sim/src/astar/`:

- `objects.rs`: **the manager's objects are Rust allocations**. Each kind (continent, path area, forest, hill, choke
  point, rectangle area, path sector, sub sector) is one `#[repr(C)]` record (`EeAStar*Obj`: the area's record, its
  tile set's handle, the kind's record) made and freed by `ee_astar_object_new/_delete` (a sector frees its sub
  sectors). The C++ classes (ee/EEAIContinent.h, ...) are views of them with no constructor and no vtable, checked
  against the Rust layout at start (EEAIRegisterContinentState). EEAIArea, EETown and EEHerd have no vtable: the
  slots a kind had its own of dispatch by the area type (Create, GetRandomPoint: EEAIRectangleArea.cpp; RemoveTile,
  GetClosestPoints: Rust). `U2DSparseArrayPointContainer` has no vtable.
- `cm_queries.rs`, `cm_capture.rs`: **every query and change of the manager** (the lookups by tile or unit, the
  region ids, the adjacent ids, the choke points, the random areas, GetTilesNextTo, IsNextToArea, RepairPathArea,
  RemoveForestTile, the serial table) with its CNTQ record; the members are inline forwarders in
  ee/EEAIContinentManager.h. The CNTQ scope's depth is shared with the three queries left in C++.
- `cm_passes.rs`: **the build passes whole**: Clear, BuildRegions, Rebuild, Reset, Shutdown (the continents, hills,
  path areas, forests, sectors and sub sectors, links, region ids, choke points and their pruning, scenario areas,
  chains). EEAIChokePoint.cpp deleted.
- `area_continent.rs`, `area_shore.rs`: **EEAIContinent's members whole** (GetClosestPoints with AREA 2, the shore
  members with AREA 6); EEAIContinent.cpp deleted. EEAIPathSector.cpp deleted (its lookup is Rust).
- `cm_host.rs`: the continent state's host questions Rust answers (Contains, RemoveTile, the kinds' Create, the
  tiles' presence, terrain, AI value, AI kind, wall, wood weight).

Round 6 (rust-aicluster6), the path manager's host and the areas' hosts:

- `astar/pm_host.rs`: **the path manager host's world reads** (`answer`, asked before the C++ host for every
  operation of the manager and its searches): the units' ids, the registry, references, tiles, locations, players
  and class tests through the world views; the sub sectors and their sectors from the continent manager's records;
  the searches' views (`EeAStarWorldView` boxes); the sea walkers' blocked tiles (`EEAIPathBlockerCollector`, with
  `EEGetUnitTiles`' UUNT note); the game time, the map, the loading flag, the region delay; the capture's records;
  the graph goal's request flags.
- `astar/pm_capture.rs`: **PMRQ whole in Rust** (`EEPathRequestCapture` gone): a thread's stack of scopes; the
  manager's calls open theirs in `ee_path_manager_call` (the inputs, the results, the queues and the units and
  requests that left them), the two bounded searches the C++ keeps open theirs (`ee_path_capture_open/_word/_close`).
  The dumps' text (`DumpSim`'s bounded line, `DumpOutOfSynch`) is pm.rs's; `EEPathManager`'s entry points are inline
  forwarders in ee/EEPathManager.h.
- `cp/town_game.rs`, `cp/herd_game.rs`: the town's and the herd's area, continent, registry and player reads in Rust
  (`area_op`, `world_op`: tiles, hull, create, closest points, continents, connected ids, the serial table, the
  hull distance, a player's herds); `ai/units_bind.rs` `world_op`: the classic_lua units host's time, continents,
  areas, forests, last attackers, transports. `astar/cm_host.rs`: `area_create`, `update_hull` (`EEAIArea::UpdateHull`
  inline over it).

What is left in C++ of the cluster:

- EEPathManager.cpp (about 750 lines): the host operations that change a unit or read what only C++ holds (DropPath,
  TakePath with the class's AdjustPath, SnapLastWaypoint and the goal's PathTaken, the path-done flag with the
  bounded tiles' vector, the waypoint deque, the location goal's tile and auto-waypoint flag, the request count with
  `EEPlayer::IsHuman`, `UnitStranded`, `GetTerrainFlags`, RequestUnitPaths' goals), the requests (`EEAutoPathRequest`
  objects: the Units lane's group and goal code reads their `mPath`), the owners awaiting a loaded request
  (`std::function`), ClampTile (the terrain mesh's GetClosestTile), the FSFile glue of the save parts, FindPath and
  FindBoundedPath (their PMRQ words over C++ vectors), MergeWaypoints.
- EEAIContinentManager.cpp (about 300 lines): ctor and dtor, CanGetTo and AreConnectedForUnit (the unit classes'
  passable flags and terrain tests: the Units lane's), UpdateWallUnit, BuildRegions' scenario inputs, RegrowTrees.
- EEAIContinentSave.cpp: the save's file part, the host for a town's or herd's owner (their ids are C++ members),
  the herd's Create, the region size global (`gPathManager.mLC8`), the AI value write, the random stream
  (`URandom::Generate`), the load's re-filing of the towns and herds.
- EEAIRectangleArea.cpp: the kinds' Create and GetRandomPoint dispatch (the town's and herd's records are C++ bases,
  the random stream is C++'s), the rectangle's random point, UpdateBounds.
- EETown.cpp, EEHerd.cpp: the hosts' unit, goal, patrol group, formation, animal and random operations.
- EEComputerPlayer.cpp (waits on EEPlayer), EECPNext*.cpp (the classic_lua host's player and unit operations),
  EECPPlacement.cpp, EECPResourceCenter.cpp, EECPTrace.cpp, EEAIFunctionObjects.cpp, EECPEngineHelpers.cpp.

## Goldens and gates

ASRN (every tile search: the machine's questions, the view's answers in order, the storage) is the view's and the
tile modifiers' golden; ASGR (the graph searches) the route planners'; CNTQ the manager's queries the view still asks
through the C++; STRM (the app profile's storms) the storm shaper's and planners'; saveload the saved route planner.
Results: PORT_LEARNINGS "AI Thread cluster, round 4".

## Left (next rounds)

- **The path manager's last host operations** wait on the Units lane: the unit's path and goal members (DropPath,
  TakePath, SnapLastWaypoint, the waypoint deque, the location goal, SetGoal and the location goals
  RequestUnitPaths makes), `EEUnitClass::GetTerrainFlags`, and `EEPlayer::IsHuman` / `UnitStranded` (EEPlayer's).
  `EEAutoPathRequest` can become a Rust record once its readers (EEGroup, EEActiveGroup, EEUGRust, the wall
  manager) read it through accessors.
- **The continent manager's rest**: CanGetTo and AreConnectedForUnit need the unit classes' passable flags and
  terrain tests in Rust (the Units lane's); UpdateWallUnit needs the footprint tiles; RegrowTrees makes units. The
  save unit's town and herd re-filing needs the players' towns and herds (their ids) in Rust records.
- **The computer player's class** (EEComputerPlayer.cpp): it can only go when EEPlayer is Rust's.
- **The herd paths no game reaches** (round 2's list) stay unit-tested only.
