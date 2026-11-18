# AI Thread, lane B: the computer player and its economy (`ee_sim::cp`)

The lane's state (one file per lane; each round rewrites it; the round's story is in PORT_LEARNINGS.md "AI lane B").
The owner unlocked the AI Thread's C++ for porting with whole classes moving into Rust. Lane B has the computer
player and its economy: `EEComputerPlayer.cpp`, `EECPPlacement.cpp`, `EEHerd.cpp`, `EECPResourceCenter.cpp`,
`EETown.cpp`, `EEInfluenceTable.cpp`, `EECPLegacyLoad.cpp` (7.7k lines at the start). Lane A has the Lua AI host
(EECPNext*, EECPLua, EECPEngine*, EECPReflexes, EECPTrace, EECPCombatManager), lane C the path manager, continents,
areas and the A* goals.

## What is where (round 1, branch rust-ai-cp1)

`rust/crates/ee_sim/src/cp/` (a module of ee_sim; C interface `rust/sim/cp.h`):

- `legacy_load.rs`: **`EECPLegacyLoad.cpp` whole**: the readers that step over a retail file's (0.229-0.250: the
  campaigns' scenarios) deleted C++ combat, civilization and exploration manager blocks; the town centre tile goes to
  the placement. The C++ file and its header are gone.
- `fields.rs`: **`EEComputerPlayer`'s 238 plain members** (`mLBD4` .. `mAttackStarted`: the tuning values, timers
  and flags) as the record `EeCpFields` the class derives from (`class EEComputerPlayer : public EEPlayer, public
  EeCpFields`). The names, types and order are the C++'s, so every reader (lane A's files included) is unchanged.
  The containers (`mResourceCenters`, `mFavoriteTypeHistory`, `mUnitsToKill`, `mDangerZones`), the managers'
  pointers and the engines' states stay C++ members.
- `init.rs`: **`Init`'s values** from the behaviour table (`EeCpBehavior` is `EECPBehavior`'s record; the C++ passes
  `gCPBehaviors`, the lobby's AI level and the cost cheat). The record is zeroed first.
- `save.rs`: **`EEComputerPlayer::Load` and `Save`** as one field order (`seg_*` lists) used by both, over the player
  save's host (`EeUnitsPlayerSaveFn`): the file's primitives pass to `EESavePrimitive`; the combat manager's and the
  placement's blocks, the resource centres and the favourite types' history are host operations
  (`EE_CP_SAVE_*`, `EECPSaveOperation` in EEComputerPlayer.cpp). The version refusals (before 0.229, 0.251-0.280),
  the legacy blocks, the 0.232/0.236/0.237 fields, the 0.256 running state (returned in `EeCpLoaded`; the C++
  registers the two `EEAfterLoad` restores as before) are Rust's.
- `player.rs`: the per-difficulty scales (`GetAttackDelayModifier` .. `GetIslandExpansionTimer`: inline forwarders in
  the header now), their scenario setters, `ScaleCost`, `SetNextAttackTime` (the game's generator through a callback)
  and `RestoreManagers`' field steps (the time rebase, the attack bounds, the drop-off root, the regroup, the
  pre-0.229 timers).
- `influence.rs`: **`EEInfluenceTable` whole**: the per-region, per-player influence values are a Rust array; every
  body (values, sums, the enemies' sums through the world views, AddUnit/RemoveUnit reading the unit's class
  influence and owner through `UnitView`) is Rust. `ee/EEInfluenceTable.h` is a shim (no RTSRegionTable base any
  more; it keeps `mRegionSize` and `GetRegions` for the readers that divide tile coordinates by it).
  `EEInfluenceTable.cpp` is gone; `EEWorld::CreateInfluenceTable` passes the map's size.
- `timers.rs`: **the experiments' timers** (the 0.258 block: ten arrays of 64, the claim ranges of the 0.265
  resource state block) are Rust's (`timers::STATE`; the C++ global `gCPExperimentTimers` and the struct are gone,
  nothing read them since main's stages 5 and 6), with both blocks' save and load. The danger zones stay the
  computer players' (`mDangerZones`, read by lane A's reflexes) over `EECPStateOperation`; a load's copies are put in
  place at the first tick (`ee_cp_timers_apply`, the `EEAfterLoad` restores as before).
- `file.rs`: the saved game's primitives as these use them (`SaveFile`), and a byte file for the tests.
- Dead code deleted: `GetEpochFrequencyModifier`, `IsIslandExpansionDue` (no caller), `GetTileValue`'s declaration.

## Goldens and how they were taken

- **The record** (round 1): a TEMP dump of all 238 fields, the history and the centre count (never committed;
  `build/cp/patch_dump.py`) at the end of `Init`, of `RestoreManagers` and at the start of `Save`, on the unmodified
  C++ and again on the moved code: saveload seed 2 at 600 and 1350 s (14 lines: the 6 constructions, 4 restores and
  4 saves) equal byte for byte.
- **The saves**: the players chunk (tag 5: every computer player's block) of the saves at 600, 1350 and 2100 s of
  seeds 2 and 3 equal to the unmodified build's, decompressed, byte for byte. Chunks 4, 8 and 12 differ between two
  runs of the same unmodified build too (not a change).
- **The legacy reader**: `tools/scenario_check.py` (all 27 campaign scenarios and the corrupt case) on the unmodified
  build and on each commit: every report's outcome, end time, players, samples (less the wall time), alive
  computers, defeat order and epochs equal.
- **The timers and the resource state**: the world chunk (tag 7, which holds both blocks) of the six saves equal to
  the unmodified build's; saveload seeds 2-3 continue identically.
- The oracle (seeds 2-3 per commit) identical to `build/rust_oracle/ref`.

## Not moved yet (what is left)

- `EEComputerPlayer.cpp` (~1,380 lines): the favourite type choices (the screeners), `Regroup`, `UpdateGroups`,
  `Update`, `DamageTaken`, `WorkOutPopulation`, the unit events (`UnitPlaced`, `UnitTaken`, `UnitGoalDone`,
  `SendToNearestBase`, the transport idle), `EnterEpoch`/`ChooseCivilization`, `GetObjectsToGroup`, the host
  operations of the moved saves, and `RestoreManagers`' C++ parts (the favourites' re-choice, the stranded units).
- `EECPPlacement.cpp` (1,749): the site searches (`FindSite` and its finders, `CanBuildAt`, the "unit near" tests).
- `EECPResourceCenter.cpp` (743): `FindBuildSite`, `GetCenter`, `GetMoraleFill`, the area-effect tests, its
  Load/Save/Restore (the C++ calls them through `EE_CP_SAVE_CENTER_*`).
- `EEHerd.cpp` (1,365) and `EETown.cpp` (407): both derive from `EEAIArea` (lane C's); the player lane's
  `player_areas.rs` and `town_save.rs` already drive their bookkeeping and saves from Rust.

## Results (round 1, 2026-11-17)

Commits on `rust-ai-cp1` (from origin/rust 0bfe280ce; origin/rust did not move during the round, so the merge was
empty): 49368a116 (legacy readers, C++ -404), e8cdb3c38 (the record, Init, Load, Save, the scales: -1,490),
fde0a48c9 (the influence table: -128), 338cd1ddb (the experiment timers: -65), the docs. **C++ -2,087 net** (+244
-2,331; the target was 3,000). Rust: `ee_sim::cp`, 3,739 lines with tests (23 unit tests).

On the final tree: both builds 0/0 (EE-AOC, EE-SIM, the test profile); the Rust gate PASS (4,439 tests); oracle
seeds 2-7 identical to the reference; the captures at seeds 2-3: oracle 204, late 206, space 202, calamity 214
digests equal to their goldens; the plain saveload 18/18 identical; the matrix (`--settings all --seeds 2-3 --at
1000,1900 --expect tools/saveload_expect.txt`) `saveload expect: PASS` (calamity seed 2 has no 1900 s save, next
seed 2 parts at 915 and 276 ticks, as expected); `scenario_check.py` 27/27 with every report equal to the unmodified
build's; sim_editor PASS (14,448 lines); `gates.sh` (slot 1): tour, ai-game (time-cap, alive [2, 3], classic_lua)
and lan-game PASS, GATES PASS.

