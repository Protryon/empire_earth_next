# AI Thread cluster (the A* layer, the continent manager, the path manager, the computer player, the herd, the town)

The lane's state (one file per lane; each round rewrites it; the round's story is in PORT_LEARNINGS.md "AI Thread
cluster"). From 2026-11-18 the AI Thread's C++ moves by clusters of classes that call each other, whole classes
into Rust with their state as Rust records. The cluster's files: everything left in src/Empire Earth/AI Thread,
their headers in include/ee, the AI Rust in ee_sim (ai, cp, astar). It takes over the earlier lanes' files: R5b.1
(docs/progress/r5b1-a-pathfinding-continents-choke-points.md: the path manager, the continents, the A* goals),
lane B (docs/progress/ai-computer-player.md: the computer player and its economy) and lane A (the Lua AI host).

## What is where (round 1, branch rust-aicluster2)

`rust/crates/ee_sim/src/astar/` (C interface `rust/sim/astar.h`), new this round:

- `flood.rs`: **the continent manager's flood fills whole** (`EeAStarFlood`: the machine's members, the node
  storage, the goal's fill state, the storage's id grid) for the continent, hill, path area, path sector and choke
  point fills, and **the continental chains' graph search** (`ee_astar_number_chains`, over a host of the
  continents: `EeAStarChainCallbacks`). The C++ passes drive the fills and make the areas of the collected tiles.
- `path_search.rs`: **the path manager's tile machines whole** (`PathSearch`: the path machine, the bounded
  machine), their goal a C record (`EeAStarPathGoalRec`) the C++ modifiers hold; `intercept` answers the path
  manager's machine operations before its C++ host; the bounded machine lives in the Rust manager.
- `graph_search.rs`: **the path manager's graph machines whole** (`GraphSearch`, `EeAStarGraphGoalRec` held by the
  route planners), `intercept` for the graph operations, the saved graph searches.
- `search_note.rs`: the A* capture's side of the Rust searches (the hooks' notes, the ASRN/ASGR records), over
  EEAIAStarCapture.cpp's C interface.

Deleted C++ (src + include/ee): the A* goals, storages and machines (`EEAIAStar*Goal`, `EEAIAStar*Storage`,
`EEAIAStarGoalBase.h`, `EEAIAStarGraphGoalBase.h`, `EEAIAStarMachine.h`, `EEAIAStarMachineSteps.h`): 21 units and 26
headers. What stays of the A* layer in C++: `EEAIAStarView` (the world as the searches see it: the host of their
tile queries, with its capture notes), `EEAIAStarCapture` (TEST ONLY), the modifiers and route planners (other
clusters make them: the units, the storm, the computer player's walls), `AStarGraphableObjectBase`.

## Goldens and gates

The A* capture (ASRN, ASGR in every sim_capture profile: every search's inputs, a digest of its hooks' answers and
view queries in order, its storage) is the gate of every search moved; PMRQ and PMSP (the path manager), CNTR and
CNTQ (the continents), HULL/AREA (the areas made), the oracle and saveload (the saved tile and graph searches).
Results: PORT_LEARNINGS "AI Thread cluster, round 1".

## Left (next rounds)

- The continent manager's passes (`CreateContinents` .. `Rebuild`, `EEAIBuildChokePoints`) as Rust over a host
  that makes the areas; then the areas' objects as Rust records with C++ handles (round 4's plan in r5b1's file).
- The path manager's last C++ host operations (the units, the requests, the owners awaiting loaded requests, the
  modifiers): they move with the units cluster and with the modifiers as Rust enums (their makers are the units,
  the storm and the walls).
- `EEAIAStarView` (759 lines: the tile queries over the map, the units and the continent manager) moves when its
  answers can be read from Rust records (the map's and the units' clusters).
- The computer player with `EECPNext` and `EECPNextUnits`, then the herd and the town (the brief's step 2): not
  started this round.
