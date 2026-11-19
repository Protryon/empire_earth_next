# The game's leaves (tools/cpp_deps.py's leaves and near-leaves of src/Empire Earth)

The lane's state. Each round rewrites this file to the current state; each round's story goes in PORT_LEARNINGS.md
("The game's leaves, round N"). The census is `python -I tools/cpp_deps.py report` (after a build: it reads the
objects): `build/cpp_deps/leaves.txt` and `near_leaves.txt`; `cpp_deps.py unit FILE` names one unit's edges.

## Moved (round 1, branch `rust-leaves1`)

| C++ unit (deleted) | Rust home | What callers keep |
|---|---|---|
| (EESimCapture.cpp: moved here and by the app lane at once; the merge kept the app lane's ee_sim app `capture.rs`) | ee_sim app `capture_record.rs` (Rust's records over the writer) | |
| EEAIAStarCapture.cpp/.h | ee_sim astar `search_note.rs` (the run stack, events, digests) | the view's note: `ee_astar_capture_note` |
| EEAIContinentCapture.cpp/.h | ee_sim astar `cm_io.rs` `ee_astar_cm_capture`; ee_sim app `capture_record.rs` the HULL/AREA records | lle/U2DPolygon.h's `EEGeomCapture*` inline forwarders |
| EEWorldViewCheck.cpp | ee_sim world `check.rs` `ee_world_view_check` (VWCK from the views alone) | EEPhysics::Tick's call |
| EEVoting.cpp | ee_sim app `voting.rs` (the record, Propose) | `EEVoting` header-only over `EeAppVoting` (no vtable) |
| EEUnitStore.cpp, part | ee_sim units `step_note.rs` (USTM, USST), `store_sweep.rs` (USPK, USFL, their sweeps, the Fill test by objects) | ee/EEUnitStore.h inline forwarders |
| EECPCombatManager.cpp | (empty saves; IsMilitaryClass had no caller) | header-only here; origin/rust deleted the class |
| EEStartingResources.cpp, EEUCUpgradeTable.cpp | ee_data's lists (already) | header-only: `Load` a template over `EEDbLoadList` |
| EEAIUnitTargetingTable.cpp | ee_data `tables.rs` (the 3 x 10 weights) | header-only loader; the target search reads `ee_data_ai_targeting_table` |
| EECPEngine.cpp | ee_game `cp_engine.rs` (the manifest's files and read, the choices, EE_AI_ENGINE, the resolve, the folder hash, the log) | ee/EECPEngine.h inline forwarders; the seats' six functions in EECPNext.cpp |

## Left, and why

- **EERules** (208): `eedata.py cppgate` compiles EERules.cpp into the converter's gate; moving it changes the
  converter (tools/eedata), which the lane must not touch without the lead.
- **EEWorldLayout** (218): the layout is `offsetof` and base casts: what only the C++ compiler knows.
- **EEColorTable**, **EECPBehaviorTable**, **EETeam**, **EEUnitClassImprovement**, **EESoundObject**: C++ object
  storage (GEColor members and a vector of objects; a global record array read in place; FSFile saves through
  vtables; UWideString names): each needs its storage as a Rust list first.
- **EECPPlaceCapture** (183, a leaf after the merge): its lists' digests are Rust's since the AI Thread cluster's
  round 3; the scope's depth, thread and record can move, but the class's inline constructor needs the computer
  player's id and `State`/`Player` read C++ members (the town centre tile, EeCpFields' bytes): next round.
- **EEUnitStore**'s rest (~390): the insert's seed (offsets of the C++ members), the USCK cross-check (virtual
  `GetChecksum` over the chain), the goal queue reports.
- **EEAIArea**, **EEMRecord**, **NEMessage**, **NESynchronousClient**: whole C++ classes (vtables, the area
  hierarchy, the message classes).
- SEConductor, UITrampolines, UIEPolledEvent: LLE (the LLE lane's).

## Next

The tables as ee_data lists (EEColorTable, EECPBehavior, EEAnimal, EEUnitBehavior's to-hit and world tables), then
EETeam over ee_sim app teams.rs, then the captures that call only Rust once their targets move.
