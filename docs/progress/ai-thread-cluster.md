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

Round 2 (rust-aicluster3), in `rust/crates/ee_sim/src/cp/` (C interface `rust/sim/cp.h`):

- `herd.rs`: **EEHerd whole**: its members are the record `EeHerdFields` the class derives from (the original's
  names), its units, looks and a loaded game's tiles are Rust lists, every body (AddUnit .. UpdateSurrounding,
  Load, Save, RepairPointers) is Rust over `HerdHost`. `herd_game.rs`: the game host (EEHerd.cpp
  `ee_cp_herd_host`, `ee_cp_area_host`) and the entry points. EEHerd keeps its vtable slots and its callers'
  members as forwarders.
- `town.rs`: **EETown's bodies**: its counters, continent, reach and flags are the record `EeTownFields`; the owner
  and the patrol group stay typed C++ members; `town_game.rs` its host (EETown.cpp `ee_cp_town_host`). The random
  point is shared with the herd (one body in the original, by ICF).
- `lists.rs`: **the computer player's lists**: the resource centres whole (EECPResourceCenter deleted: its members
  a Rust record `CpCenter`, its forwarders with their PLCE records), the favourite types' history, the units to
  kill, the danger zones; held by `EeCpFields::mLists`. The C++ keeps the centres' world (EECPResourceCenter.cpp
  `ee_cp_center_host`: references, area effects), the centres' `std::sort` (each comparison's GetCenter records
  PLCE) and the capture's scopes (`ee_cp_scope_*`).
- EECPCombatManager deleted (empty since stage 6).

## Goldens and gates

The oracle's sim hash (unit checksums and the random seed every 10 s) is the herd's and the town's golden; AREA
(the areas' geometry calls) caught a wrong continent category in the town; PLCE and CPEV are the centres' and the
lists'; saveload the herd's, centres' and history's saved blocks. Results: PORT_LEARNINGS "AI Thread cluster,
round 2".

## Left (next rounds)

- **The computer player's class itself.** What remains of EEComputerPlayer.cpp is its EEPlayer overrides (they call
  EEPlayer's C++ bodies and the units' methods), the save host and the player host: it can only go when EEPlayer
  (the app cluster) is Rust's. EECPPlacement (the town tile and the placement host), EECPNext.cpp and
  EECPNextUnits.cpp are hosts of Rust bodies: they shrink as the world views cover the units', the tech tree's and
  the continent manager's reads.
- **Herd paths no game reaches**: a predator's strike back, the herd's attack back, Surround, FollowNearestHerd with
  a target, the hunt's ring (MoveTo) and the attack and surround states: no gate game hits an animal or has a
  predator herd find prey (unit tests only; see PORT_LEARNINGS).
- The continent manager's passes as Rust over an area-making host; the areas as Rust records behind handles; the
  path manager's last C++ host operations; EEAIAStarView (as round 1 left them).
