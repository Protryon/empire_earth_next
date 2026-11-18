# AI Thread, lane B: the computer player and its economy (`ee_sim::cp`)

The lane's state (one file per lane; each round rewrites it; the round's story is in PORT_LEARNINGS.md "AI lane B").
The owner unlocked the AI Thread's C++ for porting with whole classes moving into Rust. Lane B has the computer
player and its economy: `EEComputerPlayer.cpp`, `EECPPlacement.cpp`, `EEHerd.cpp`, `EECPResourceCenter.cpp`,
`EETown.cpp`, `EEInfluenceTable.cpp`, `EECPLegacyLoad.cpp` (7.7k lines at the start). Lane A has the Lua AI host
(EECPNext*, EECPLua, EECPEngine*, EECPReflexes, EECPTrace, EECPCombatManager), lane C the path manager, continents,
areas and the A* goals.

## What is where (rounds 1 and 2, branch rust-ai-cp1)

`rust/crates/ee_sim/src/cp/` (a module of ee_sim; C interface `rust/sim/cp.h`):

- `placement.rs` (round 2): **`EECPPlacement` whole**: the "unit near" tests (`HasUnitNear`, `HasAntiAirNear`,
  `HasTownCenterNear`, `HasGateNear`, `HasTowerNear`, `HasWallNear`, `HasEnemyNear`), `CollectSiteTiles`,
  `CanBuildAt` (with its border rule), `FindSite` and every finder (airport, anti-air, centre, granary, production,
  shore, space, tower, wonder), `IsNearAirportEnd`, `HasBuilderFor`, `SetTownCenter`, `UnitAdded`, `Reset`,
  `Restore`, the epoch's classes (`EECPAntiAirClass` ..), `EECPIsSiteClass`, `EECPCountOwn` and the Lua verbs'
  `EECPNextPlaceSearch` with its searches (ring, tower ring, adjacent, the dock by the fish). The class's own data is
  Rust's: `EeCpPlacement` (the dock and space site tiles, whether they were collected, `UnitAdded`'s classes), owned
  by the C++ object as `mRust`. `mTownCenterTile` and `mPlayer` stay C++ members (lane A's files read the tile).
- `resource_center.rs` (round 2): **`EECPResourceCenter`'s bodies**: `FindBuildSite` (towers, settlements, houses,
  production buildings, temples, universities, hospitals), `GetCenter`, `GetMoraleFill`, `IsInAreaEffect`,
  `GetMinEffectRadius`, `FindResourceUnit`, `IsWithinRadius`, `AdoptDropSite`'s test, `Contains`, and the saved
  game's `Load`, `Save` and `Restore`. `mUnits`, `mUnit` and `mIsActive` stay C++ members (EEComputerPlayer, lane A's
  view and trace read them); the references and the vector's growth stay C++ (constructor, destructor, `AddUnit`,
  `SetUnit`, `ReleaseUnit`; `SetActive` sets the flag and asks Rust's `GetCenter`, discarded as before).
- `place_host.rs` (round 2): the world both ask, one trait (`PlaceHost`), and the helpers: the tile lists (the pure
  math of `units::uu_tiles`, the same Rust the C++ wrappers call), `EEAIRandomShuffle` (Dinkum's draws through the
  host's `GetRandom`), the C++ `std::sort` by key for the sorts with ties (`sort_by_key`).
- `place_game.rs` (round 2): the game's host (`ee_cp_place_host` in EECPPlacement.cpp, `ee_cp_center_host` in
  EECPResourceCenter.cpp: one switch each; a unit's members in one `UNIT_INFO` read, since `IsLandBuilding` and
  `IsDropSite` are class methods) and the C ABI the forwarders call (`ee_cp_*`, with a context `EeCpPlaceCtx` per
  call: the player, its record, the town tile, the Rust data, whether the capture records).
- `events.rs` (round 2, second task): **`EEComputerPlayer`'s own bodies**: `WorkOutPopulation`, the centres'
  grouping (`GetObjectRegion`, `GetObjectsToGroup`: gone from the class, Rust only; `Regroup`, `UpdateGroups`), the
  favourite types (one Rust choice by kind for the four `ChooseFavorite*Type`, `GetScreenedTypesByLine` gone; the
  screener `EEAIFOFavoriteUnitScreener` stays C++ and is asked per class), `RestoreManagers`' missing favourites,
  `ChooseCivilization`'s pick (the names a Rust table), `EECPRetireObsoleteUnits`, `EECPPendingEpoch`,
  `AvoidsEnemies`. The world is a third host, `ee_cp_player_host` (EEComputerPlayer.cpp); the centres' members it calls
  are their C++ forwarders (with their PLCE scopes) and the centres' sort is the original `std::sort` with
  `EECPCenterDistanceLess` (its comparator asks each centre's GetCenter), so the placement records stay the same.
- `legacy_load.rs` (round 1): **`EECPLegacyLoad.cpp` whole** (a retail file's deleted manager blocks).
- `fields.rs` (round 1): **`EEComputerPlayer`'s 238 plain members** as the record `EeCpFields` the class derives
  from.
- `init.rs`, `save.rs`, `player.rs` (round 1): `Init`'s values, `Load`/`Save` as one field order, the
  per-difficulty scales and `RestoreManagers`' field steps.
- `influence.rs` (round 1): **`EEInfluenceTable` whole**. `timers.rs` (round 1): the experiments' timers.
- `file.rs`: the saved game's primitives (`SaveFile`), and a byte file for the tests.

What is left in the two C++ files: the forwarders with their TEST ONLY PLCE scopes, the two host switches, the town
tile's `Load`/`Save` (U2DPoint's own two-word read and write), the two `EETileDistanceLess` comparators other units
sort with (defined in EECPPlacement.cpp since the original) and `EEComplexUnit::DistanceSquaredToUnit` (defined in
EECPResourceCenter.cpp since the original; six other callers).

## Goldens and how they were taken

- **PLCE** (round 2, committed: ee/EECPPlaceCapture.h, src/Empire Earth/AI Thread/EECPPlaceCapture.cpp): one record
  at the return of every outermost call into the placement, a resource centre or their free functions (one
  thread-local depth for all), with the method, the player, the inputs and the outputs (results, found tiles, the
  placement's town tile and site lists, a centre's unit ids and main unit, the player's centres after the searches).
  Recorded on the unmodified C++ (commit 1ccecbcda) for seeds 2-3 of oracle, late, space, calamity and the new
  `--profile placement` (the oracle's game with the boom and Age of AI scripts). The gates' classic_lua builds through
  ai.find_site, so the four standard profiles record the centres' bookkeeping only (UnitAdded, AddUnit,
  AdoptDropSite, IsWithinRadius, SetActive, GetCenter, the epoch classes); the placement profile runs the site
  searches (313 and 48 outermost ai.place calls, with every finder and CanBuildAt under them). Records per seed 2 /
  3: oracle 9,976 / 8,929, late 15,006 / 14,144, space 17,690 / 13,777, calamity 5,573 / 7,494, placement 10,246 /
  8,869. Two runs of the unmodified build give the same digests.
- **The rest**: every other capture tag of the five profiles, the oracle, scenario_check 27/27, saveload.

- **CPEV** (round 2, committed with PLCE's scope class, its own depth): the computer player's own calls (the bodies
  above and the unit events round them) with their arguments, results and the player's state when they end (the
  record EeCpFields to its last member, the favourite history, the units to kill, the danger zones, the centres with
  their active flags, the town tile); only the thread that runs the computer players' Update records. Recorded on the
  unmodified C++ (5c9af0341) for seeds 2-3 of the five profiles: oracle 302,791 / 302,713, late 444,564 / 440,606,
  space 447,140 / 440,473, calamity 120,478 / 275,567, placement 18,344 / 15,042 (mostly CanProduce, then
  AvoidsEnemies, EECPPendingEpoch, the unit events; WorkOutPopulation 38-135, ChooseCivilization 2-3, EpochReached with
  the favourites and the retire cheat under it 7-32). Two runs of oracle and late give the same digests. Regroup and
  UpdateGroups have no CPEV record on that thread (Regroup runs before the first Update, UpdateGroups after a load):
  PLCE records every centre call they make, and saveload runs UpdateGroups.

## Not moved yet (what is left)

- `EEComputerPlayer.cpp` (1,263 lines with the host and the TEST ONLY scopes): `Update`, `DamageTaken` and the unit
  events (`UnitPlaced`, `UnitTaken`, `UnitCompleted`, `UnitGoalDone` with `EECPTransportIdle` and
  `SendToNearestBase`, `UnitsSwapped`, `UnitStranded`), `SetActive`, `EpochReached`, `CanProduce`, `Pay`, the
  constructors and `Init`, `Load`/`Save` with their host operations, `RestoreManagers`' other parts (the stranded
  units' scan), the experiment timers' and resource state's save hosts. They are mostly calls into lane A's hooks and
  the units' goals: a Rust body would be one host operation per C++ call.
- `EEHerd.cpp` (1,365) and `EETown.cpp` (407): both derive from `EEAIArea` (lane C's).
- The two shims could shrink further once the centres' unit lists are Rust's (C++ vectors of handles that
  EEComputerPlayer and lane A's files read) and the town tile is read through Rust.

## Results (round 2, 2026-11-17)

Commits on `rust-ai-cp1`: 1ccecbcda (PLCE, TEST ONLY, with its goldens: C++ +549), 0dac90b5a (the placement and the
centres' bodies: C++ -1,519 net, +737 -2,256), bacdd41ff (the merge of origin/rust cb6e60d50; clean), the docs.
Rust: `ee_sim::cp` placement.rs, resource_center.rs, place_host.rs, place_game.rs and their tests (4,541 lines, 15
unit tests).

Per the move's commit: both builds 0/0; the Rust gate PASS (4,454 tests); oracle seeds 2-3 identical; the captures
at seeds 2-3 equal to their goldens (placement 2, oracle 206, late 208, space 204, calamity 216 digests, PLCE in
each); scenario_check 27/27; saveload seeds 2-3 PASS (all six saves identical).

On the tree merged with origin/rust (cb6e60d50; the merge needed nothing): both builds 0/0 (EE-AOC, EE-SIM, and the
test profile's EE-AOC); the Rust gate PASS (4,508 tests); oracle seeds 2-7 identical to the reference; the captures at
seeds 2-3 equal to their goldens (placement 2, oracle 206, late 208, space 204, calamity 216 digests); scenario_check
27/27. Not run here: the harness gates (gates.sh: the lead's), the saveload matrix.

Second task of the round (from the merged tree): 5c9af0341 (CPEV, TEST ONLY, with its goldens: C++ +151), b1d8a94c0
(EEComputerPlayer's bodies above into events.rs: C++ -416 net, +154 -570), b26973dc6 (the placement's forwarders
through two helpers: -81), 25eb99ec8 (the merge of origin/rust 4929244f3: lane A round 2, WONLobby round 14; a
PORT_LEARNINGS.md conflict of two appended sections, both kept), the strip_markers commit, the docs. Round 2's C++ in
all: -1,316 net (PLCE +549, the placement and centres -1,519, CPEV +151, the player -416, the forwarders -81); the
target was 3,000. Rust: events.rs and its tests (835 lines, 6 tests).

On the final tree, merged with origin/rust 4929244f3: both builds 0/0 (EE-AOC, EE-SIM, the test profile's EE-AOC); the
Rust gate PASS (4,523 tests); oracle seeds 2-7 identical to the reference; the captures at seeds 2-3 equal to their
goldens (placement 4, oracle 208, late 210, space 206, calamity 218 digests; PLCE and CPEV in each; no golden needed
re-recording after the merge); scenario_check 27/27; saveload 18/18 identical; the matrix (--settings all --seeds 2-3
--at 1000,1900 --expect tools/saveload_expect.txt) `saveload expect: PASS` (the expected calamity seed 2 missing 1900
s save and the late seed 2 partings at 915 and 276 ticks). Not run here: gates.sh (the lead's). main merge 4 had not
landed on origin/rust by the final fetch.
