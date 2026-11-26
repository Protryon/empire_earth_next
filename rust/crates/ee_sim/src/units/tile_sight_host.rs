//! The tile sight's world in the game (tile_sight.rs): the map, tiles and players read through the world views
//! (ee_sim::world), the visibility words written through each tile's `mVisibility`, and the C++ calls through
//! the sight host `ee_units_sight_host` (EEComplexUnit.cpp; units round 14: was `ee_units_tile_sight_host`). The entry
//! points the C++ forwarders and the sight bodies call, each inside its TSGT capture scope (ee_game sight_capture.rs;
//! units round 14: were EESightRings.cpp's helpers). Absent from a test binary.

#![cfg(not(test))]

use crate::units::sight_host::{
    ee_units_sight_host, EE_UNITS_SIGHT_SHOW, EE_UNITS_SIGHT_SHOW_OFF, EE_UNITS_SIGHT_SHOW_TILE, EE_UNITS_SIGHT_SHOW_TILE_OFF,
};
use crate::units::tile_sight::{self, SightWorld};
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};
use crate::world::{MapView, PlayerView, TileView, WorldRead, WorldView};

/// `gDoppelgangerManager.TileShown(tile a, x c, y d, player b)`
pub const EE_UNITS_TILE_SIGHT_SHOWN: i32 = 14040;
/// `gDoppelgangerManager.TileHidden(tile a, player b)`
pub const EE_UNITS_TILE_SIGHT_HIDDEN: i32 = 14041;
/// player a's `mSavedGroups.Reset(0x18, 1)`
pub const EE_UNITS_TILE_SIGHT_FIRST: i32 = 14042;
// (the minimap's and the terrain mesh's counts are the sight host's EE_UNITS_SIGHT_SHOW* statements)

extern "C" {
    // the TSGT scope (ee_game sight_capture.rs, as EESightScope's constructors and destructor; renamed here and left
    // out of the headers, cbindgen.toml: the game's own declarations are rust/game/sight_capture.h's)
    #[link_name = "ee_app_sight_open_tile"]
    fn ee_units_sight_scope_tile(kind: u32, player: *const c_void, tile: *const c_void, x: c_long, y: c_long, value: c_ulong);
    #[allow(clippy::too_many_arguments)]
    #[link_name = "ee_app_sight_open_circle"]
    fn ee_units_sight_scope_circle(
        kind: u32,
        player: *const c_void,
        x0: f32,
        y0: f32,
        x1: f32,
        y1: f32,
        radius: f32,
        value: c_ulong,
    );
    #[link_name = "ee_app_sight_close"]
    fn ee_units_sight_scope_close();
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
        unsafe { ee_units_sight_host(op, 0, a, b, c, d, &mut [0.0; 3], std::ptr::null_mut()) };
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
        let (count, mesh) = if on {
            (EE_UNITS_SIGHT_SHOW, EE_UNITS_SIGHT_SHOW_TILE)
        } else {
            (EE_UNITS_SIGHT_SHOW_OFF, EE_UNITS_SIGHT_SHOW_TILE_OFF)
        };
        self.host(count, 0, 0, x, y);
        self.host(mesh, 0, 0, x, y);
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

/// `RTSTile::AddSight(player)` (`on` 1) / `RemoveSight(player)` of tile `tile`, in the TSGT scope `scope`
/// (`kSightTileAdd`, `kSightTileRemove`)
#[no_mangle]
pub extern "C" fn ee_units_tile_sight_tile(world: u64, scope: u32, on: u8, tile: u64, player: u64) {
    // SAFETY: the scope's records read the live player and tile
    unsafe { ee_units_sight_scope_tile(scope, player as usize as *const c_void, tile as usize as *const c_void, -1, -1, 0) };
    with_game(world, 0, |g| tile_sight::tile_sight(g, on != 0, tile, player));
    // SAFETY: the scope opened above
    unsafe { ee_units_sight_scope_close() };
}

/// EEPlayer's `AddSharedSight(tile, x, y)` (`on` 1, `shared` 1), `RemoveSharedSight(tile)` (0, 1), `AddOwnSight`
/// (1, 0), `RemoveOwnSight` (0, 0), in the TSGT scope `scope` (a removal names no coordinates: -1 to the capture, 0
/// to the body, as the original's calls)
#[no_mangle]
#[allow(clippy::too_many_arguments)] // the C++ signatures'
pub extern "C" fn ee_units_tile_sight_player(
    world: u64,
    scope: u32,
    on: u8,
    shared: u8,
    player: u64,
    tile: u64,
    x: i64,
    y: i64,
) {
    // SAFETY: the scope's records read the live player and tile
    unsafe {
        ee_units_sight_scope_tile(
            scope,
            player as usize as *const c_void,
            tile as usize as *const c_void,
            x as c_long,
            y as c_long,
            0,
        )
    };
    let (x, y) = if on != 0 { (x, y) } else { (0, 0) };
    with_game(world, 0, |g| tile_sight::player_sight(g, on != 0, shared != 0, player, tile, x, y));
    // SAFETY: the scope opened above
    unsafe { ee_units_sight_scope_close() };
}

/// `EEAddSightCircle` (kind 0) / `EERemoveSightCircle` (1) of (x0, y0); `EEAddSightRingStep` (2),
/// `EERemoveSightRingStep` (3), `EEMoveSightCircle` (4) from (x0, y0) to (x1, y1); in the TSGT scope `scope`
#[no_mangle]
#[allow(clippy::too_many_arguments)] // the C++ signatures'
pub extern "C" fn ee_units_tile_sight_circle(
    world: u64,
    scope: u32,
    kind: u32,
    x0: f32,
    y0: f32,
    x1: f32,
    y1: f32,
    radius: f32,
    player: u64,
) {
    // SAFETY: the scope's records read the live player
    unsafe { ee_units_sight_scope_circle(scope, player as usize as *const c_void, x0, y0, x1, y1, radius, 0) };
    with_game(world, 0, |g| match kind {
        0 | 1 => tile_sight::circle(g, kind == 0, x0, y0, radius, player),
        2 | 3 => tile_sight::ring_step(g, kind == 2, x0, y0, x1, y1, radius, player),
        _ => tile_sight::move_circle(g, x0, y0, x1, y1, radius, player),
    });
    // SAFETY: the scope opened above
    unsafe { ee_units_sight_scope_close() };
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

/// `EEMap::ReserveTiles`' `new uint32_t[count]` and memset (map_words.rs)
#[no_mangle]
pub extern "C" fn ee_units_map_words_new(count: u64) -> *mut u32 {
    crate::units::map_words::alloc(count as usize)
}

/// `delete[] mVisibilityWords` (null: nothing)
///
/// # Safety
/// `words` is null or a buffer of `ee_units_map_words_new`/`_resize`, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_units_map_words_free(words: *mut u32) {
    // SAFETY: the caller's contract
    unsafe { crate::units::map_words::free(words) }
}

/// `EEMap::SetPlayerCount`'s new words for map `map`: `tiles * new_words` words, each tile found (y outer, x inner)
/// given the next old slice's words and pointed at its new slice; the old buffer freed. Returns the new buffer.
///
/// # Safety
/// `map` is a live map, `old` null or its live buffer of `old_words` words a tile.
#[no_mangle]
pub unsafe extern "C" fn ee_units_map_words_resize(
    map: u64,
    old: *mut u32,
    tiles: u64,
    old_words: u64,
    new_words: u64,
) -> *mut u32 {
    // SAFETY: the C++ holds the map (its own member function)
    let token = unsafe { WorldRead::begin() };
    let mut found = Vec::new();
    if !old.is_null() {
        // SAFETY: the caller's contract
        if let Some(m) = unsafe { MapView::of(&token, map) } {
            let size = m.size();
            for y in 0..size {
                for x in 0..size {
                    if let Some(t) = m.tile(x, y) {
                        found.push(t.handle());
                    }
                }
            }
        }
    }
    let at = crate::world::layout::layout().tile_visibility as usize;
    // SAFETY: the caller's contract; each found tile is live and its `mVisibility` member at `at`
    unsafe {
        let words = crate::units::map_words::resize(
            old,
            &found,
            tiles as usize,
            old_words as usize,
            new_words as usize,
            |t, p| ((t as usize + at) as *mut *mut u32).write(p),
        );
        crate::units::map_words::free(old);
        words
    }
}
