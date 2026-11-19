# The units (`src/Empire Earth/Units`, ~29k lines) whole in Rust

The lane's state (one file per lane; the round's story is PORT_LEARNINGS.md "The units' lane, round 1"). Rust home:
`ee_sim::units` and `ee_sim::combat`, over the world views (`ee_sim::world`, [world-views.md](world-views.md)).
Earlier rounds over the same code: [r5b4-world-and-units.md](r5b4-world-and-units.md),
[r5b2-combat-and-unit-formulas.md](r5b2-combat-and-unit-formulas.md).

## Round 1 (branch `rust-units1`, from origin/rust 48c8472f0, 2026-11-18)

The measure (lines of C++ per file at the start): EEComplexUnit 3,720, EEUnitClass 3,207, EEUGRust 2,031 (GoalOp
972), EEUnitHost 1,906, EECombatGoals 1,308 (EEWorldOp 325, EEQueryRead 116), EEUnitClassBuildings 1,243,
EEComplexDoppelganger 1,215 (the unit's save operation 317, its load sections 193, RepairPointers 275), EEActiveGroup
1,179 (its host 334), EEUnitClassAir 1,028, EEUnitClassPeople 902 (the citizens' host 132), EEUnit 841, EEUnitUpdate
742 (its read host 153, its act host 467). `tools/dead_members.py report` finds nothing left in this area (the LLE
lane's round 2 ran it to a fixpoint; the 11 names it lists here are vtable slot placeholders and one-word accessors
of other areas).

What is a host kept alive by Rust calling back for data the views now read: the read switches of the update and
`ActionChanged` host, the combat query host, the goal host's record fills and plain reads, the unit host's plain
reads, the active group's and the citizens' unit fills. This round moved those, and one whole method pair.

| Commit | What | Net C++ |
|---|---|---|
| ddc53c80e | `EEUnit::UpdateHidden` and `EEComplexUnit::UpdateHidden` whole in Rust (units/hidden.rs over a host trait; the views write `mHiddenFlags` in place); the renderer's opacity calls it directly (its host callback gone); the physics' game time and tick in the views (`WorldView::physics`, `game_time`, `tick_ms`; layout `physics_pointer`, `physics_saved`) | -170 |
| 066621d3d | The update / `ActionChanged` host's 44 plain reads (units/update_views.rs `ViewUpdateHost`; the table carries the host's class) | -91 |
| 692e01050 | The combat query host's 16 plain reads (combat/query_views.rs, asked first by `EeCombatQueryHost::read`) | -25 |
| 4ebeef429 | The goal host: `BUILD_UNIT`'s record fill (rust_goal.rs `build_unit_fields`), `LOAD_CLASS`, `UNDETECTABLE`, `LINE_OF_SIGHT`, `FIRES_OVER_WALLS`, `SAME_PLAYER`, `UNIT_LOCATION`, `PLAYER_ID`, `CLASS_UNIT_TYPE`, `FRIENDLY`, `IN_CINEMATIC`, `UNIT_CATEGORY` | -103 |
| 6c68f5a67 | The unit host's 9 plain reads (`object_hit_points`, `unit_max_hit_points`, `unit_garrison_capacity`, `unit_has_tile_node`, `object_record`, `unit_air_traffic_type`, `unit_class_count`, `unit_class_info`, `map_has_tile`) | -71 |
| 353e4d8f6 | The active group's unit fill (ag_host.rs `ag_unit_fields`) and the citizens' unit fill, tiles and player lists (citizen_host.rs `cz_unit_fields`) | -86 |

Round total: -546 net C++ lines (target 4,000). Merged origin/rust 9a277da24 (AI lane C) cleanly.

Verified on the merge: both builds 0/0, the Rust gate (4,578 tests), the oracle seeds 2-7 identical, the twelve
capture profiles at seeds 2-3, the plain saveload 18/18, the matrix `--expect` PASS, scenario_check 27/27, the
editor gate, the test-profile build; gates.sh: see PORT_LEARNINGS "The units' lane, round 1".
Only a tour exercises the renderer's direct UpdateHidden call (ee_render fill.rs).

## What is left (next)

- The host switches' remaining cases are world calls (methods, the path manager, the effects, sounds, the goal
  objects' own members); each moves only with the class it calls. The plain reads still in C++ need a layout
  member first: `mWaypoints`, `mMine`, `mHomeBase`, `mCitizenBuildQueue`, the class's effect and projectile
  pointers, the physical model's frame and scale, `mInitialContactLocation` (not in the records).
- The shot host (`EEShotOp`: 8 plain reads) needs the unit and class in its table, as the update host now has.
- The save operation's id reads (`EEUnitSaveOperation`'s `EE_UNITS_UNIT_SAVE_ID`) need the pointer members in the
  layout; then saveload 18/18 and the matrix gate them.
- Whole classes: `EEComplexUnit`'s remaining bodies (FindContacts, StepAside, SetGroupPathGoals, Kill) and
  `EEUnitClass::FindPath`, `ProduceUnit` are world-call sequences: a Rust body over a host each, the pattern of
  units/hidden.rs (a trait the views implement, a mock in the tests).
