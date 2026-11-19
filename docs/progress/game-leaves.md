# The game's leaves (tools/cpp_deps.py's leaves and near-leaves of src/Empire Earth)

The lane's state. Each round rewrites this file to the current state; each round's story goes in PORT_LEARNINGS.md
("The game's leaves, round N"). The census is `python -I tools/cpp_deps.py report` (after a build: it reads the
objects): `build/cpp_deps/leaves.txt`, `near_leaves.txt` and `waves.txt`; `cpp_deps.py unit FILE` names one unit's
edges.

## Moved

| C++ unit (deleted) | Rust home | What callers keep |
|---|---|---|
| **Round 2** (branch `rust-leaves2`) | | |
| EECPPlaceCapture.cpp | ee_sim cp `place_capture.rs` (PLCE, CPEV: the depths, the event thread, the records, the placement's and the player's state through the world layout: `cp_placement`, `placement_town`, `placement_rust`) | ee/EECPPlaceCapture.h: `EECPPlaceScope` inline over `ee_cp_scope_*` |
| EEAIArea.cpp | ee_sim astar `area_ops.rs` (the hull's queries and rebuilds, the closest points, the tiles; HULL 1/2/6, AREA 1/3/4/5) | ee/EEAIArea.h inline; `GetRandomPoint`, `UpdateHull`, `UpdateBounds` in EEAIRectangleArea.cpp (the random stream, the influence table) |
| EEColorTable.cpp | ee_data `tables.rs` (`EeDataList::Colors`, `ee_data_player_color`, `_find`) | ee/EEColorTable.h: static readers returning `GEColor` by value; `EEGetPlayerColor` by value |
| EECPBehaviorTable.cpp | ee_data `tables.rs` (`gCPBehaviors` itself, `ee_data_cp_behavior_load`) | `extern "C" EECPBehavior gCPBehaviors[129]`; `Load` a header template |
| EEUnitBehavior.cpp | ee_data `tables.rs` (the world constants `ee_data_world_*`, `gWeaponToHitFactors`, `ee_data_to_hit_load`); ee_sim combat `damage_table.rs` (CBDT) | ee/EEDatabaseTables.h: `EEWorldTable`, `EEToHitTable::Load`, `EETerrainTypeTable`'s loads inline; ee/EEUnitBehavior.h header-only |
| EEDatabase.cpp | ee_data `tables.rs` (`ee_data_objects_*`: dbobjects' records) | ee/EEDatabase.h inline; `EEDbObject` the record's bytes |
| EEUnitSet.cpp, EEUnitSetManager.cpp | ee_sim app `unit_sets.rs` (the records, the table, the links, `Contains`, the walk) | ee/EEUnitSetManager.h: inline readers; an `EEUnitSet*` is a Rust set |
| EEGameVariant.cpp | (ee_data's list already) | ee/EEGameVariant.h header-only (`GetFactor` over the unit sets); `gGameVariants` in EEGlobals.cpp |
| EETeam.cpp | ee_sim app `team_save.rs` (the players' load and save over `EESaveFilePrimitive`) | ee/EETeam.h header-only |
| EEHumanPlayer.cpp, EECDKeyStrings.cpp | (none: the overrides were EEPlayer's own; the key strings were never read) | ee/EEHumanPlayer.h inline constructors |
| (EETerrainTypeTable.cpp, part; EEAIPathSubSector.cpp, part) | ee_sim combat `damage_table.rs`; ee_sim astar `sub_sector_pass.rs` (`IsPassable` over the views) | `GetAttackFactor`, `EEFamilyCount`, `EEDamageFactor` stay; `gDamageTable` and its twins went |
| **Round 1** (branch `rust-leaves1`) | | |
| EESimCapture.cpp, EEAIAStarCapture, EEAIContinentCapture, EEWorldViewCheck, EEVoting, EECPCombatManager, EEStartingResources, EEUCUpgradeTable, EEAIUnitTargetingTable, EECPEngine; EEUnitStore.cpp part | PORT_LEARNINGS "The game's leaves, round 1" | |

## Left, and why

- **EERules** (208): `tools/eedata/cppgate.py` compiles `src/Empire Earth/Database/EERules.cpp` into the converter's
  gate (its line 45, with `cppgate/eedata_gate.cpp`). Moving it needs: the gate's source list changed (a converter
  change: every lane's data tree is rebuilt), the rules' C++ readers (`EECombatRules::IsElevation*`, the civ
  builder's and powers' `Find`, `EEAIPersonalityRules::ForPlayer`, `EEDifficultyAI*`, `EERules::Epoch`) as inline
  readers over `ee_data_rules_*` (rust/crates/ee_data rules.rs holds the data already), and `eedata.py cppgate`
  passing on the converter's side.
- **EEWorldLayout** (218): `offsetof` and base casts, what only the C++ compiler knows.
- **EEUnitStore**'s rest (~390): the insert's `Fill` (the objects' members), `mInUnitStore` (an RTSUnit member), the
  USCK cross-check (`GetChecksum` virtual over the chain, the goal queues' C++ deques, the update order).
- **EEUnitClassImprovement**, **EESoundObject**, **EEAnimal**, **EETerrainColorTable**, **EEButtonObject**,
  **EETerrainTable**: C++ media objects (`EEMedia`, GE textures, the sound manager) or saves through raw `FSFile`
  reads; each needs its media as a Rust object first.
- **EEVCWonder**: the wonders (`EEPlayer::mWonders`, a C++ container of complex units) are not in the views.
- **EEMessageCapture**, **EEMRecord**, **NEMessage**, **NESynchronousClient**: the message classes' vtables (the
  network engine's C++ calls them); the tree moves whole with the network lane.
- **EEFormation** (the units' lane's, round 2), the Scenario Editor's and the UI's shells (their lanes').

## Next

EEUnitStore's `Fill` over the views (the store record from a handle) and the USCK check's walks; the media objects
as Rust objects (the sound and button tables first); EEVCWonder once the wonders are in the views.
