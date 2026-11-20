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

What is left in C++ of the cluster's continent side: EEAIContinentManager.cpp (about 300 lines: the class's ctor and
dtor, CanGetTo and AreConnectedForUnit (they read unit classes), UpdateWallUnit (footprints, the path manager),
BuildRegions' scenario inputs, the forests' RegrowTrees (it makes units)); EEAIContinentSave.cpp (the save's file
part, the host for a town's or herd's owner, a herd's Create, the region size global, the AI value write, the random
stream, and the load's re-filing of the towns and herds); EEAIRectangleArea.cpp (the random points of the kinds, the
hull's update).

## Goldens and gates

ASRN (every tile search: the machine's questions, the view's answers in order, the storage) is the view's and the
tile modifiers' golden; ASGR (the graph searches) the route planners'; CNTQ the manager's queries the view still asks
through the C++; STRM (the app profile's storms) the storm shaper's and planners'; saveload the saved route planner.
Results: PORT_LEARNINGS "AI Thread cluster, round 4".

## Left (next rounds)

- **The path manager's host** (EEPathManager.cpp, about 1,400 lines): the units' and requests' operations the Rust
  manager asks. Not started this round.
- **The continent manager's rest**: CanGetTo and AreConnectedForUnit need the unit classes' passable flags and
  terrain tests in Rust (the Units lane's); UpdateWallUnit needs the footprint tiles and the path manager;
  RegrowTrees makes units. The save unit's town and herd re-filing needs the players' towns and herds in Rust.
- **The computer player's class** (EEComputerPlayer.cpp): it can only go when EEPlayer is Rust's.
- **The herd paths no game reaches** (round 2's list) stay unit-tested only.
