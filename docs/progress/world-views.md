# Lane V: the world views (`ee_sim::world`)

Lane state (one file per lane). Branch `rust-worldviews`, from `origin/rust` 4aeedb1a1. The foundation lanes U, R, P
and S build on: "Rust layout, C++ allocation" (the object storage plan). The records stay inline in the C++ objects;
Rust reads them in place through typed, read-only views.

## What is where

- `rust/crates/ee_sim/src/world/` (a module of ee_sim, beside `units`; other crates reach it as `ee_sim::world`):
  - the records the C++ classes derive from, moved out of `units` (same type names `EeUnits*`, same C names; `units`
    re-exports each at its old path, so `crate::units::unit_fields::EeUnitsRtsUnitFields` etc. still compile):
    `unit_fields.rs` (`EeUnitsRtsUnitFields`, `EeUnitsUnitFields`, `EeUnitsComplexUnitFields`, `bool_byte`),
    `unit_class_fields.rs` (`EeUnitsUnitClassFields`), `stats_fields.rs` (`EeUnitsUnitClassUpgradeFields`),
    `player_fields.rs` (`EeUnitsRtsPlayerFields`, `EeUnitsPlayerSaved`, `EeUnitsPlayerFields` and the two Rust
    objects it points to, `EeUnitsQueued`, `EeUnitsPlayerWonders`), `map_fields.rs` (`EeUnitsRtsMapFields`,
    `EeUnitsMapFields`). Their C declarations are in `rust/sim/world.h`, which `rust/sim/units.h` includes.
  - `layout.rs`: `EeWorldLayout` (replaces the unit store's `EeUnitsStoreSlots`).
  - `view.rs`: `WorldRead`, `Record`, `world_field!`, `UnitView`, `ClassView`, `StatsView`, `PlayerView`,
    `WorldView`, `MapView`, `TileView`.
  - `check.rs` (TEST ONLY): the VWCK digest through the views.
  - `view_tests.rs`: the unit tests (fake objects laid out like the C++ classes).
- C++: `include/ee/EEWorldLayout.h`, `src/Empire Earth/Units/EEWorldLayout.cpp` (the registration: `offsetof` and
  base casts, static_asserts, runtime checks; called first thing by `EEWorld::EEWorld()`), and the TEST ONLY
  `src/Empire Earth/Units/EEWorldViewCheck.cpp` (VWCK).

## The API (for lanes U, R, P, S)

```rust
use ee_sim::world::{WorldRead, UnitView, PlayerView, WorldView, MapView, TileView, ClassView, StatsView};

// 1. a token: the caller holds the world (the simulation thread inside a call from C++, or the world's lock)
let t = unsafe { WorldRead::begin() };
// 2. a view from a handle C++ passed in (unsafe: the handle must be a live object of that class) ...
let u = unsafe { UnitView::of(&t, unit_handle) }?;      // None for 0
// ... or from a store lookup (safe: a listed unit's record names a live object)
let u = UnitView::by_id(&t, id)?;
// 3. read
let (x, y) = u.tile_xy();
let hp = u.hit_points();                                 // i64 (a C++ long)
let class = u.class()?;                                  // ClassView: category(), unit_type(), family(), range() ...
let range = u.stats()?.range();
let owner = u.player()?.id();
let facing: std::os::raw::c_ulong = ee_sim::world_field!(u.complex(), mFacing);  // any record member
```

- Handles are addresses as `u64`: a unit's is its `RTSUnit*` (= `EEUnit*` = `EEComplexUnit*`), a player's its
  `RTSPlayer*` (= `EEPlayer*`), the map's its `EEMap*` (= `RTSMap*`), a tile's its `EETile*`, the world's `&gWorld`.
  `EEWorldLayoutRegister` aborts if any of these first bases is not at offset 0.
- `UnitView`: `handle`, `kind` (`mObjectKind`), `is_unit`, `is_complex`; RTSUnit: `id`, `tile_xy`, `valid`,
  `queued_for_delete`, `node_freed`, `next` (owner's list), `tile`, `player`, `has_tile_node`; EEUnit: `hit_points`,
  `resource_amount`, `resource_type`, `hidden_flags`, `class`, `stats`, `complex_unit(_handle)`; EEComplexUnit:
  `height`, `sight`, `facing`, `is_built`, `commandable`, `cloaked`, `cloakable`, `experience`, `formation_slot`,
  `power`, `transform` (4 x 4), `location` ([x, y, z]), `command_handle`, `command_type`, `goal_handle`, `goal`
  ((kind, id)), `active_goal_handle`, `active_goal`, `active_group_handle`, `herd_handle`, `container(_handle)`,
  `cargo_len`, `cargo_handle(i)`, `cargo(i)`, `previous_part_handle`, `next_part_handle`, `current_research`,
  `build_queue_len`, `build_queue_technology(i)`. With `ClassView`'s flags this covers Lane P's `UNIT_INFO` but
  `GetPopulationCost()` (a class method: stays a call, or a Rust port of it). The records: `rts()`, `fields()` (EEUnit), `complex()`.
- `ClassView` (`fields()`, `category`, `unit_type`, `family`, `range`, `technology`, `ai_kind`, `land_building`,
  `vital`, `inactive`, `blocks_tile`, `wall`, `tree`, `unload_range`), `StatsView` (`fields()`,
  `range`, `scale`, `max_hit_points`).
- `PlayerView`: `rts()`, `saved()`, `fields()`, `id`, `slot`, `sees_all`, `observer`, `color`, `selected_group_handle`,
  `enemy(i)`, `first_unit`, `units()` (the list in order).
- `WorldView`: `num_players`, `current_player_index`, `player(i)` (17 slots), `current_player`, `map`, `game_speed`,
  `population_limit`, `wonders_to_win`, `game_options_handle` (`&mGameOptions`), `ghost_units_len`,
  `ghost_unit_handle(i)`, and the flags `victory_enabled`, `game_screen_active`, `loading_saved_game`,
  `cheats_enabled`, `in_cinematic`, `user_paused`, `using_carbon` (Lane S's list). Another EEWorld member: a
  `world_*` layout field, one `offsetof` line in `EEWorldLayout.cpp`, one accessor (the `flag` helper for a bool).
- `MapView`: `rts()`, `fields()`, `size`, `player_count`, `visibility_words`, `tile(x, y)` (`GetTile`'s unsigned bounds).
- `TileView`: `terrain_type`, `height`, `ai_value`, `has_visibility`, `visibility_word(w)`, `visibility(player)`
  (`GetVisibility`), `units()` (the node list; `None` for a node of no unit).
- Add an accessor for a member you need: a record member needs nothing (`world_field!`); a pointer member needs a
  layout field (`EeWorldLayout` in layout.rs, filled in `EEWorldLayout.cpp` with `offsetof`, then
  `rust_build.py header`).

## The rules

- `WorldRead::begin` and `ee_world_read_begin` take **no lock**. Lane S measured the editor: two thirds of its reads
  run while the reading thread holds `gWorld.mLock` for write (`EE_SCREENS_ACTION_LOCK`, the tab switch), where a
  read lock on top deadlocks EeTsLock, and some run with no lock (the editor's construction thread, the main menu,
  the lobby). So the token borrows whatever the caller holds; a caller that holds nothing and races the simulation
  must take the read lock itself before the token. A "does this thread hold the world lock" test (Lane S's TEST ONLY
  ee_base ts/held.rs) can later turn the debug count into a check.
- A view borrows a `WorldRead` and cannot outlive it. Never store a view, a handle-derived view, or a token across
  ticks or in a static. Tokens are counted per thread in debug builds (`read_depth()`); C++ can bracket a region with
  `ee_world_read_begin`/`ee_world_read_end`.
- Views hold raw addresses only: each accessor copies the member's bytes. A call back into C++ while a view exists is
  fine; reading through a view an object that call freed is not.
- Views never touch reference counts.
- `bool` members are read as their byte (non-zero is true), also inside `world_field!` and arrays.
- Debug builds check the kind byte when a unit view is made (`<= 2`) and that `EEUnit`/`EEComplexUnit` members are
  read only from objects of that class (`need_unit`, `need_complex`). Release builds (the game, EE-SIM) do not.
- In unit tests there is no registered layout (`layout::get()` is None, so the store and the stats reads see none);
  set one for your test's thread with `layout::with_test_layout(...)` (view_tests.rs `fake_layout` is the pattern).
  Never call `layout::set` in a test: it is the process's.
- `c_long`/`c_ulong` are 64 bits on LP64: named accessors return `i64`/`u64` (ids `u32`), so code is the same on
  both data models.

## The layout

`EeWorldLayout` (`size` is checked against Rust's): the record bases (`rts_unit_fields`, `unit_fields`,
`complex_unit_fields`, `unit_class_fields`, `stats_fields`, `rts_player_fields`, `player_saved`, `player_fields`,
`rts_map_fields`, `map_fields`); RTSUnit `unit_object_kind`, `unit_tile_node`, `unit_next`, `unit_tile`,
`unit_player`; EEUnit `unit_class`, `unit_complex_unit`, `unit_stats`; EEComplexUnit `complex_location`
(mPhysicalModel's GETransformation), `complex_active_group`, `complex_container`, `complex_command`,
`complex_goal_state`, `complex_active_goal`, `complex_herd`, `complex_cargo`; `goal_kind`, `goal_id`, `action_type`;
the player's `player_color`, `player_first_unit`, `player_selected_group` (`mEnemies` is in `EeUnitsPlayerSaved`);
the world's `world_players`, `world_map`, `world_num_players`, `world_current_player`; the map's `map_tiles` (`mSize`
is in `EeUnitsRtsMapFields`); the tile's `tile_stride` (`sizeof(EETile)`), `tile_terrain_type`, `tile_units`,
`tile_visibility`, `tile_height`, `tile_ai_value`; the tile unit node's `unit_node_unit`, `unit_node_next`.
`EEWorldLayoutRegister` also checks that a `std::vector`'s first two words are its begin and end (MSVC, libc++ and
libstdc++ all do), which `cargo_*` rely on.

## VWCK (TEST ONLY)

Every physics tick while the capture runs (`EEPhysics::Tick`, after USCK), `EEWorldViewCheck` folds a fixed list
(the world's counts and slots; per player its ids, flags, colour, selected group, enemies, resource caps, population,
result, tribute fee, heroes and its units; per unit its RTSUnit, EEUnit, class, stats and EEComplexUnit fields, its
location, command, goal, active goal and cargo; the map's record and per tile its terrain, height, AI value,
visibility words and unit ids) into an FNV-1a 64 of 32-bit words through the C++ members, and
`ee_world_view_digest` folds the same list through the views. A difference stops EE-SIM (exit 3, `EE-WORLD-VIEWS:`).
The record (tick, players, units, tiles, the C++ digest) is in every profile; its golden lines (oracle, late,
calamity, seeds 2-7) were recorded from 51225ab08, the C++ side on the unmodified tree.

## Converted callers (the proof)

`rust_goal.rs`'s `OpHost` reads 15 plain operations through `read_unit` (the views) instead of the C++ host:
`UNIT_ID`, `UNIT_TILE`, `UNIT_VALID`, `UNIT_COMMAND`, `STATS_RANGE`, `UNIT_HEIGHT`, `HIT_POINTS` (3 callers),
`COMPLEX_UNIT` (3), `PLAYER_SEES_ALL`, `CONTAINER` (2), `UNIT_FAMILY`, `UNLOAD_RANGE`, `CLASS_TYPE`, `CARGO_COUNT`,
`CARGO_CATEGORY`; their cases left `EEUGRust.cpp`'s `GoalOp`. The `EE_UNITS_GOAL_HOST_*` numbers stay.

## Not in this round (lanes P and S asked)

- A tech node view (Lane P: `NODE_INFO`, `NODE_AVAILABLE`, `NODE_UNIT_CLASS`, `NODE_IS_AVAILABLE`,
  `TECH_OBSOLETE`): the nodes are `EETechTreeNode` objects; a `NodeView` is a layout block and a view like the others.
- The world table (`IMPROVEMENT_TIME`, `WONDER_FACTORS`): ee_data's records, read directly, not a view.
- `TileView::visibility` reads through each tile's `mVisibility` pointer, as `GetVisibility`'s 82 readers do, so
  Lane P's move of the words to a Rust `Vec` must keep the tiles' pointers into it.

## Results (round 1, 2026-11-17)

Commits: 51225ab08 (VWCK's C++ side), 7f9371798 (its golden lines from it), f54bd9db0 (the module, the layout, the
views, VWCK's Rust side), 0f46c5dd3 (the goal host's fifteen reads), the rustfmt, the PORT_LEARNINGS section, the
merge of origin/rust 48b2f4f04.

- C++: +118 net (EEWorldLayout.cpp +147, its header +16, EEWorld/EEPhysics +5; EEUGRust.cpp -34, EEUnitStore.cpp's
  slot block -16), plus the TEST ONLY EEWorldViewCheck.cpp +231.
- Rust: 9 new unit tests in world::view_tests (the layout, tokens, bool bytes, every view over fake objects, the
  debug kind checks, the VWCK word list, the store lookup); `rust_build.py gate` PASS (4,353 tests).
- VWCK: the C++ digests of 51225ab08 (unmodified tree) are the golden lines of oracle, late and calamity, seeds 2-7;
  the views' digest equals the C++'s every tick of all 18 games (no `EE-WORLD-VIEWS`), and the C++ digests equal
  the golden lines (`sim_capture.py check` PASS: oracle 584, late 590, calamity 615 digests; space 2-3 PASS).
- Oracle seeds 2-7 identical to the reference; saveload `--expect` PASS (all settings, seeds 2-3, 1000 and 1900 s);
  both builds 0/0; `gates.sh` via slot.py: tour, ai-game (time-cap, alive [2, 3], classic_lua) and lan-game PASS,
  GATES PASS.
- After the merge of origin/rust: both builds 0/0, oracle 2-7 identical, the Rust gate PASS, captures oracle, late,
  calamity and space (seeds 2-3) PASS.
- Linux: `cargo check` and clippy of ee_sim (lib and tests) for x86_64-unknown-linux-gnu are clean (the views and
  the records compile with 64-bit `c_long`); not run on Linux.
