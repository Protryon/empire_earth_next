# Lane A: the Lua AI host (EECPNextUnits, EECPNextLua, EECPNextView, ...) in Rust

Lane state (one file per lane). Round 2: branch `rust-ai-lua2`, from `origin/rust` cb6e60d50 (round 1 merged). The
lane moves the AI Thread's Lua host (EECPNextUnits.cpp, EECPNextLua.cpp, EECPNextView.cpp, EECPNextArmy.cpp,
EECPNext.cpp, EECPLua.cpp, EECPEngine*.cpp, EECPReflexes.cpp, EECPTrace.cpp, EECPCombatManager.cpp) into
`ee_sim::ai`. The Lua scripts (data/ai) do not change, and must see the same values in the same order. The Lua C
library stays (src/ThirdParty/lua; the mlua switch waits on the owner, PORT_LEARNINGS "Lane A, round 1").

## The golden

- `EE_AI_BINDING_TRACE` (TEST ONLY, `ee_sim::ai::binding_trace`): every `ai.*` binding call writes the FNV-1a 64 of
  its arguments' and its results' text, every strategy step the hash of its view and its orders (round 1).
- `tools/ai_binding_golden.py run NAME` plays 15 seeded EE-SIM games with the trace on (build/ai_golden/NAME);
  `compare BASE NAME` names the first differing line per game. About 6 minutes, `--jobs 2`.
- Round 2's base is `build/ai_golden/c0`, recorded on cb6e60d50 (the round's start) twice, equal; it also equals
  round 1's `base` (8c4258aa7): the merges between changed no binding's answers. Main merge 4 (classic_lua changes)
  had not reached origin/rust by the end of the round: re-record on it (two equal runs) after that merge.

## What is where

- `rust/crates/ee_sim/src/ai/units_state.rs`: `UnitsState` (round 1), plus the continent and wall bindings' scratch
  (`tiles`, `sites`).
- `rust/crates/ee_sim/src/ai/units_bind.rs` (game only): the state's C ABI, the hooks, round 1's 57 bindings.
- `rust/crates/ee_sim/src/ai/units_more.rs` (game only; a child module of units_bind): the other 32 bindings:
  class, can_hurt, wonders, techs, epoch, improve_cost, tuning; improve, gate, tribute, trade, diplomacy,
  garrison_fill, test_spawn; can_place, enemy_near, site_blocked, build_unsafe, place, used; area, area_point,
  forest_point, hill_point, choke_point, water_sites, unexplored, wall_line, wall_stretches, wall_path, wall_route,
  wall_route_result. Every binding of the `ai` table is Rust now; the C++ registration table names them.
- The host (`ee_ai_units_host`, EECPNextUnits.cpp) answers what the views do not hold, ops 1-44 (round 1) and:
  - 50-78: class methods with bodies (FliesOver, GetPopulationCost), the damage table, a player's team and
    wonders, the tech node methods with bodies (GetCost with ScaleCost, GetIsAvailable, GetIsObsolete: the walks
    and the capture's records), tech names, CanProduce, the citizen-upgrade event test, the improvement cost
    (EECPUnitsImproveCost), CanBuy, the gate class and side, the tribute's moves, the stance, the garrison fill,
    the view's safety tests (EECPNextSiteBlocked, BuildUnsafe, SiteThreat), the classic's enemy test through the
    fog, the place search, the tuning's non-record values (`EeAiUnitsTuning`: options, map, continents, cheats,
    personality and experiments, the behaviour table);
  - 80-93 (`EECPUnitsWallsHost`): **the only reads of the continent manager's objects** (areas at a tile or by id,
    their facts and random points, forest/hill/choke picks, the shore, a forest at a tile) and the wall utilities
    and path manager requests (EEGetWallLine, IsPathBlocked, EEAreTilesLevel, FindPath, RequestPath and the routes
    the C++ shell keeps). Lane C re-points these ops when the continents move.
  - The command messages gained ImproveUnit, HackANewUnit, BuildGate, ExchangeResources and two run modes (2: the
    borrowed slot unchecked, 3: checked; neither clears the selection).
- `rust/crates/ee_sim/src/world/tech_view.rs`: `TechTreeView` (`PlayerView::tech_tree`, `current_epoch`, `epochs`,
  `epoch(i)`, `node(id)`, `epoch_node_ids`) and `NodeView` (`fields`, `data`, `id`, `researched`, `epoch`,
  `obsolete_epoch`, `required_sol`, `current_sol`, `unit_classes_len`, `unit_class(i)`), and
  `PlayerView::computer_fields` (`EeCpFields`). Layout fields `player_technologies`, `tech_tree_fields`,
  `node_fields`, `node_data`, `node_data_fields`, `node_data_unit_classes`, `tech_host` (the address of
  `EETechHost`, whose lookups the tree view makes), `cp_fields`.
- `rust/crates/ee_sim/src/ai/military.rs`: EECPCombatManager's class tests (IsMilitaryClass, IsMilitaryType).
- `rust/crates/ee_sim/src/ai/switches.rs`: the TEST ONLY switches EE_AI_PERSONALITY, EE_AI_DEMAND, EE_AI_CHEATS,
  EE_AI_EXP (EECPTraceInit's parse, the per-player picks, the cheat names).
- C++ left in EECPNextUnits.cpp (1,145 lines, was 2,504): the hooks into the Rust state, the registration table and
  its TEST ONLY traced wrappers, the micro step's loop, the save, and the host.

## Not done (next)

- EECPNextUnits.cpp's registration, micro loop and save could follow into Rust (the host stays C++ while the
  objects it calls are).
- EECPNextLua.cpp (the view's Lua table, the orders' run), EECPNextView.cpp, EECPNextArmy.cpp, EECPReflexes.cpp,
  EECPEngine*.cpp, EECPNext.cpp, EECPLua.cpp, the rest of EECPTrace.cpp (the trace lines, the stats, the out of
  synch dump): untouched. They hold `EECPNextState`'s C++ members (vectors of spots, threats, events; the Lua state
  handle) and write the EE-AI lines the oracle compares: the next move is EECPNextState as a Rust record with the
  army scan and the view's threat memory over the world views.

## Results (round 2, 2026-11-17)

Commits: c590229af (the 32 bindings, the tech views: C++ -1,349), 015e458be (the military tests: -90), 54f081277 (the
switches: -89), the tiles fix (no list computed twice: +12), the docs. C++ -1,516 net.

- Binding golden (base c0) identical in all 15 games after each commit; mutation (ai.techs' obsolete_epoch + 1)
  caught at classic-2 line 24 (its first ai.techs).
- The switches: a game with EE_AI_EXP, EE_AI_CHEATS, EE_AI_PERSONALITY and EE_AI_DEMAND set gives the old EE-SIM's
  EE-AI lines (all but the wall-clock EE-AIPROF lines).
- Final tree: oracle seeds 2-7 identical; captures oracle (204 digests), late (206), space (202), calamity (214),
  seeds 2-3, PASS (the oracle capture caught the shore computed twice before the fix: CNTN records); saveload 18/18;
  the matrix `--expect` PASS; both builds 0/0; the Rust gate PASS.
- Not run by this lane: gates.sh (the lead's harness sessions). Only a tour exercises nothing here: every change is
  simulation code the oracle, the captures and the golden reach.
