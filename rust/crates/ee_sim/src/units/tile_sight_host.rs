//! The tile sight's world in the game (tile_sight.rs): the map, tiles and players read through the world views
//! (ee_sim::world), the visibility words written through each tile's `mVisibility`, and the C++ calls through
//! `ee_units_tile_sight_host` (EESightRings.cpp). The entry points the C++ forwarders call. Absent from a test binary.

#![cfg(not(test))]

use crate::units::tile_sight::{self, SightWorld};
use crate::world::{MapView, PlayerView, TileView, WorldRead, WorldView};

/// `gDoppelgangerManager.TileShown(tile a, x c, y d, player b)`
pub const EE_UNITS_TILE_SIGHT_SHOWN: i32 = 1;
/// `gDoppelgangerManager.TileHidden(tile a, player b)`
pub const EE_UNITS_TILE_SIGHT_HIDDEN: i32 = 2;
/// player a's `mSavedGroups.Reset(0x18, 1)`
pub const EE_UNITS_TILE_SIGHT_FIRST: i32 = 3;
/// `gMiniMap->CountCellOn/Off(c, d)` (a != 0: on), the terrain mesh's `ReferenceTile/DereferenceTile(c, d)`
pub const EE_UNITS_TILE_SIGHT_SHOW: i32 = 4;

extern "C" {
    fn ee_units_tile_sight_host(op: i32, a: u64, b: u64, c: i64, d: i64) -> i64;
}

struct Game<'t> {
    world: Option<WorldView<'t>>,
    map: Option<MapView<'t>>,
    token: &'t WorldRead,
}

impl<'t> Game<'t> {
    fn player(&self, p: u64) -> Option<PlayerView<'t>> {
        // SAFETY: the C++ passes live players (or 0)
        unsafe { PlayerView::of(self.token, p) }
    }
    fn tile_view(&self, t: u64) -> Option<TileView<'t>> {
        // SAFETY: a tile of the map (or 0)
        unsafe { TileView::of(self.token, t) }
    }
    fn host(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64) {
        // SAFETY: the C++ switch calls the world objects the body names
        unsafe { ee_units_tile_sight_host(op, a, b, c, d) };
    }
}

impl SightWorld for Game<'_> {
    fn tile(&mut self, x: i64, y: i64) -> u64 {
        self.map.and_then(|m| m.tile(x, y)).map_or(0, |t| t.handle())
    }
    fn map_size(&mut self) -> i64 {
        self.map.map_or(0, |m| m.size())
    }
    fn map_players(&mut self) -> u64 {
        self.map.map_or(0, |m| m.player_count())
    }
    fn word(&mut self, tile: u64, w: usize) -> u32 {
        self.tile_view(tile).and_then(|t| t.visibility_word(w)).unwrap_or(0)
    }
    fn set_word(&mut self, tile: u64, w: usize, value: u32) {
        let Some(words) = self.tile_view(tile).map(|t| t.visibility_words_ptr()) else { return };
        if !words.is_null() {
            // SAFETY: one of the tile's words in the map's visibility words (the caller's index is the player's)
            unsafe { words.add(w).write(value) };
        }
    }
    fn player_id(&mut self, player: u64) -> u32 {
        self.player(player).map_or(0, |p| p.id())
    }
    fn shared_los(&mut self, player: u64) -> bool {
        self.player(player).is_some_and(|p| p.shared_los())
    }
    fn enemy(&mut self, player: u64, id: u32) -> u64 {
        self.player(player).map_or(0, |p| p.enemy(id as usize))
    }
    fn allies(&mut self, player: u64) -> usize {
        self.player(player).map_or(0, |p| p.allies_len())
    }
    fn ally(&mut self, player: u64, i: usize) -> u64 {
        self.player(player).map_or(0, |p| p.ally_handle(i))
    }
    fn current_player(&mut self) -> u64 {
        self.world.and_then(|w| w.current_player()).map_or(0, |p| p.handle())
    }
    fn world_player(&mut self, i: usize) -> u64 {
        self.world.and_then(|w| w.player(i)).map_or(0, |p| p.handle())
    }
    fn tile_shown(&mut self, tile: u64, x: i64, y: i64, player: u64) {
        self.host(EE_UNITS_TILE_SIGHT_SHOWN, tile, player, x, y);
    }
    fn tile_hidden(&mut self, tile: u64, player: u64) {
        self.host(EE_UNITS_TILE_SIGHT_HIDDEN, tile, player, 0, 0);
    }
    fn first_sighting(&mut self, player: u64) {
        self.host(EE_UNITS_TILE_SIGHT_FIRST, player, 0, 0, 0);
    }
    fn show(&mut self, on: bool, x: i64, y: i64) {
        self.host(EE_UNITS_TILE_SIGHT_SHOW, on as u64, 0, x, y);
    }
}

/// `map` 0: the world's (`gWorld.mMap`)
fn with_game(world: u64, map: u64, f: impl FnOnce(&mut Game<'_>)) {
    // SAFETY: called from the simulation's sight calls, which hold the world as the C++ bodies did
    let token = unsafe { WorldRead::begin() };
    // SAFETY: `&gWorld`
    let w = unsafe { WorldView::of(&token, world) };
    // SAFETY: a live map (the C++'s `this`)
    let m = if map != 0 { unsafe { MapView::of(&token, map) } } else { w.and_then(|w| w.map()) };
    let mut game = Game { world: w, map: m, token: &token };
    f(&mut game);
}

/// `RTSTile::AddSight(player)` (`on` 1) / `RemoveSight(player)` of tile `tile`
#[no_mangle]
pub extern "C" fn ee_units_tile_sight_tile(world: u64, on: u8, tile: u64, player: u64) {
    with_game(world, 0, |g| tile_sight::tile_sight(g, on != 0, tile, player));
}

/// EEPlayer's `AddSharedSight(tile, x, y)` (`on` 1, `shared` 1), `RemoveSharedSight(tile)` (0, 1), `AddOwnSight`
/// (1, 0), `RemoveOwnSight` (0, 0)
#[no_mangle]
pub extern "C" fn ee_units_tile_sight_player(world: u64, on: u8, shared: u8, player: u64, tile: u64, x: i64, y: i64) {
    with_game(world, 0, |g| tile_sight::player_sight(g, on != 0, shared != 0, player, tile, x, y));
}

/// `EEAddSightCircle` (kind 0) / `EERemoveSightCircle` (1) of (x0, y0); `EEAddSightRingStep` (2),
/// `EERemoveSightRingStep` (3), `EEMoveSightCircle` (4) from (x0, y0) to (x1, y1)
#[no_mangle]
#[allow(clippy::too_many_arguments)] // the C++ signatures'
pub extern "C" fn ee_units_tile_sight_circle(
    world: u64,
    kind: u32,
    x0: f32,
    y0: f32,
    x1: f32,
    y1: f32,
    radius: f32,
    player: u64,
) {
    with_game(world, 0, |g| match kind {
        0 | 1 => tile_sight::circle(g, kind == 0, x0, y0, radius, player),
        2 | 3 => tile_sight::ring_step(g, kind == 2, x0, y0, x1, y1, radius, player),
        _ => tile_sight::move_circle(g, x0, y0, x1, y1, radius, player),
    });
}

/// `RTSMap::FogSeen(player)` (kind 0), `ExploreUnseen(player)` (1), `EEMap::PrepareSave`'s visibility pass (2)
/// of map `map`
#[no_mangle]
pub extern "C" fn ee_units_tile_sight_map(world: u64, map: u64, kind: u32, player: u64) {
    with_game(world, map, |g| match kind {
        0 | 1 => tile_sight::fog_or_explore(g, kind == 0, player),
        _ => tile_sight::prepare_save(g),
    });
}
