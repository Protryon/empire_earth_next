# The groups lane: EEGroup, EESavedGroup, EESelectedGroup, EEActiveGroup (and EEPlayer's group parts) as Rust objects

The lane's state (one file per lane; the round's story is PORT_LEARNINGS.md "The groups lane, round 1"). Carved out
of the Units and Game core lanes for round 1: the group classes (src/Empire Earth/Units/EEGroup.cpp,
EEActiveGroup.cpp, EESelectedGroup.cpp, EESavedGroup.cpp, their headers in include/ee), `ee_units_ag_host`,
EEUnitClassUpgrade, the town and herd offsets of EEWorldLayout.cpp and the EEPlayer.cpp parts that collapse with
them. Rust home: `ee_sim::units` group_obj.rs (the objects and every body), group_host.rs (the game's host and the
entry points), group_obj_tests.rs.

## Round 1 (branch `rust-groups1`, from ef45e89ba: game core round 5 merged with rust-batch9, 2026-11-20)

| Commit | What | Net C++ |
|---|---|---|
| 16c63f042 | the groups as Rust objects: one allocation headed by `EeUnitsGroup` (what the C++ reads in place: the player, the formation, the kind flags, a selection's saved group, change time and sort flag, a hot-key group's index, an active group's goal and record `mFields`), the rest Rust's (the units, the type counts, the path requests, the destinations, the two point lists); the four C++ classes name it (no data, every member one call; the virtuals dispatch on `mGroupKind`); EEGroupPathRequest and EEGroupCluster are Rust structs; MakeClusters, MoveAlong, the sorters (MSVC's `std::sort`), ShowSelectionAreas, the saves (0.279's active groups too) in Rust; EESelectedGroup.cpp, EESavedGroup.cpp, EEGroupPathRequest.h, EEGroupCluster.h, EEActiveGroup.h deleted; EEGroup.cpp is the bodies' C++ host (`ee_units_group_host`); the units host's 21 group functions (EEUnitHost.cpp) and the ag host's group cases call the objects | -1,758 |
| 04229ccfc | EEPlayer's group parts (destructor, Reset, Clear, RemovalsDone, the tick's validation) as `ee_units_groups_player`; GetActiveGroupOf inline; the ag host's 17 goal cases (clone, delete, the location, unload, attack, heal, explore and gather accessors, the unload and attack constructions) and its reference and angle cases in Rust | -126 |
| d26448d06 | EEPlayer's selection members (UpdateSelection, TrySelectUnit, SelectUnit, SelectUnits, AddUnitToSelectedGroup, AddUnitsToSelectedGroup, AddAndRemoveUnitsFromSelectedGroup, RemoveUnitFromSelectedGroup, DeselectUnit, PlayAcknowledgement) and RefreshUpgrades in Rust (selection.rs); EEUnitClassUpgrade's media bodies (LoadMedia, UnloadMedia, LoadButtons, UnloadButtons, the six setters, the destructor's unloads, the two sounds' round robins) in Rust (upgrade_media.rs) | -237 |

Round total (src and include, the generated include/rust aside): 2,585 C++ lines deleted, 464 added, net -2,121
(target 3,000). The added lines are the one-statement shims (the group host's ~60 cases, the upgrade host's 17), the
inline handles of ee/EEPlayer.h and ee/EESavedGroup.h (one call each) and comments; the logic deleted is the four
group classes, EEGroupPathRequest, EEGroupCluster, the active groups' save, EEPlayer's group, selection and
RefreshUpgrades bodies, the units host's 21 group functions, the ag host's 30 cases and EEUnitClassUpgrade's media
bodies (the "calling C++ from Rust is fine" rule came mid-round: the third commit follows it).

### What moved wholly

- `EEGroup` (all of it: Add, Remove, Contains, GetUnit, GetNumUnits, Load, UpdateKindFlags, Clear, RemoveUnit,
  RepairPointers, Save, MoveAlong, MoveAlongFoundPaths, GetBounds, CountUnitsOfKind, CopyUnits, CopyGroup,
  FindUnitOfCategory, FindUnitWithFlags, FindPathRequest, RequestPath, CancelPathRequest, RemoveAt, MakeClusters,
  the type counts, GetFlags' body), `EESavedGroup` (Add, RemoveUnit, Validate, Assign), `EESelectedGroup`
  (CopySelection, SelectSavedGroup, Add, AddIfAbsent, AddUnit, Load, Save, Clear, RemoveUnit, RepairPointers,
  SortUnits with EESGUnitSorter, Validate, ShowSelectionAreas, RefreshSelectionAreas, GetContinent),
  `EEActiveGroup`'s own C++ (the constructor and destructor, AddMember, RequestPaths, RequestAllPaths, Fill with
  EEAGIncreasingVelocityUnitSorter, ResetUnitsPaths, Finish, SetGoal, Clear, KeepNonCitizens, KeepGroundAttackers,
  KeepCitizens, RemoveUnit, TellGroup, Validate, ClearGoal, the destinations), `EEGroupPathRequest`,
  `EEGroupCluster`, `EEComplexUnit::SetActiveGroup`, the active groups' save and restore (EESaveActiveGroups,
  EELoadActiveGroups: the C++ entry names are two-line wrappers).
- The C++ left for the groups: `ee_units_group_host` (EEGroup.cpp, ~45 one-statement cases: reference counts,
  EEFormation, the path and continent managers, the user interface, the map's outlines, the saved path requests,
  EEAwaitAutoPathRequest and EEAfterLoad), the inline handles in ee/EEPlayer.h and ee/EESavedGroup.h, the seven speed
  getters a unit reads through its group (EEActiveGroup.cpp: they read the unit's C++ stats) and `ee_units_ag_host`'s
  remaining world cases (the units' goal queues and actions, the formation's Arrange, the launch, the map's tiles,
  the C++ goal constructors NewLocation and GetGoalLocation of EEUGRust.cpp).

### Gates (the final tree; origin/rust 9e5b09552 is contained, the merge was a no-op)

See PORT_LEARNINGS.md "The groups lane, round 1" for the numbers.

## What is left

- **EEUnitClassUpgrade**: its stats (stats.rs, improve.rs) and media (upgrade_media.rs) are Rust; the object, its
  name (UWideString) and its improvement levels (EEUnitClassImprovement, read by the UI and the tech tree) are C++;
  Init, Load, Save and the improvement forwarders stay. EEPlayer's improvement cases and the class media forwarders
  (one call each) remain.
- **EEPlayer**: ExecuteSelectionMessage (swaps the two selections around a message's Execute) and UnitsSwapped's
  group part are C++.
- **The towns' and herds' host cases** (EEPlayer.cpp's ADD_TO_TOWN .. TOWN_PATROL, the tick's herd cases, the
  town save host): they call EETown's and EEHerd's C++ forwarders (AI lane); the base offsets in the world layout
  were not added (nothing reads them yet).
- **The player's group lists** (mRunningGroups, mPendingGroups, mFinishedGroups, the 20 hot-key groups, the two
  selections) are still C++ members (EEPhysics.cpp, EESMEffect.cpp, EEScenarioCapture.cpp, EEActiveGroupSweep.cpp read
  them); the Rust bodies reach them through the host.
- **EEAutoPathRequest's mPath readers** (for the AI lane): the group's reads are now one host case
  (`EE_UNITS_GROUP_OP_PATH_TILES`, EEGroup.cpp) and `EE_UNITS_GROUP_OP_PATH_DONE`/`_ID`; EEUGRust's are the Units
  lane's.
- EEActiveGroupSweep.cpp (TEST ONLY, 300 lines) makes and orders units: it moves with unit creation.
