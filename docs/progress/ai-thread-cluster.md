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

## Goldens and gates

ASRN (every tile search: the machine's questions, the view's answers in order, the storage) is the view's and the
tile modifiers' golden; ASGR (the graph searches) the route planners'; CNTQ the manager's queries the view still asks
through the C++; STRM (the app profile's storms) the storm shaper's and planners'; saveload the saved route planner.
Results: PORT_LEARNINGS "AI Thread cluster, round 4".

## Left (next rounds)

- **The census** (`tools/cpp_deps.py report`): before the round 27 AI Thread units, 12,153 lines, 11,184 of them
  in cycles; after it 25 units, 10,774 lines, 9,732 in cycles. Leaves: the two capture units and EECPEngineHelpers;
  near-leaves: EECPEngine, EEAIArea, EEAIChokePoint (new), EEAIContinentCapture, EEAIRectangleArea, the modifiers'
  remains (new), EEAIPathSector, EEHumanPlayer. The biggest cycle went from 236 units and 114,201 lines to 233 and
  112,645; EEAIContinentManager.cpp is now its third keystone (603 lines out of the cycle with it).
- **The continent manager's build passes** (CreateContinents, CreateHills, CreatePathAreas, CreatePathSectors,
  Rebuild, the choke point pass's CreateChokePoint) still make the C++ area objects; the areas as Rust records behind
  handles would let them and the area classes (EEAIArea, EEAIContinent, EEAIChokePoint, EEAIPathSector,
  EEAIRectangleArea) move (R5b.1's plan).
- **The path manager's host** (EEPathManager.cpp, 1,260 lines): the units' and requests' operations the Rust manager
  asks. The view operations 130/131 could go (the search could make its Rust view itself) once the path search
  tests' fake views take another seam.
- **The computer player's class** (EEComputerPlayer.cpp): it can only go when EEPlayer is Rust's.
- **The herd paths no game reaches** (round 2's list) stay unit-tested only.
