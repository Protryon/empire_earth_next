//! TEST ONLY: TSGT, the tile sight's capture record (ee/EESightCapture.h; tools/sim_capture.py; was
//! Map/EESightCapture.cpp's `EESightScope`), over the world views and the terrain mesh's Rust object.
//!
//! Every sight function opens a scope ([`ee_app_sight_enter`]); when the outermost one on the thread returns
//! ([`ee_app_sight_leave`]), one record holds its kind, the player, its arguments, the tile range it can reach and an
//! FNV-1a of that range after the call: a tile's call hashes that tile's visibility words, the others every tile of
//! the range (its visibility words and the mesh tile's reference count; a missing tile a marker). Nested sight calls
//! record nothing.
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use std::cell::{Cell, RefCell};
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use ee_render::ge::tmobj::{ee_ge_tm_is_valid, ee_ge_tm_tile, EeGeTerrainMesh};
use ee_sim::app::capture;
use ee_sim::world::{MapView, PlayerView, TileView, WorldRead};

use crate::app::map_capture::{tag, Fnv, Payload};

/// The kinds of ee/EESightCapture.h that matter here
const SIGHT_PLAYER_REMOVE_OWN: u32 = 7;
const SIGHT_MAP_ADD: u32 = 8;
const SIGHT_MAP_REMOVE: u32 = 9;
const SIGHT_RING_ADD: u32 = 12;
const SIGHT_RING_REMOVE: u32 = 13;
const SIGHT_CIRCLE_MOVE: u32 = 14;

/// The scope's three constructors: a tile's call, a circle or step (the float arguments), the whole map
pub const EE_APP_SIGHT_TILE: u32 = 0;
pub const EE_APP_SIGHT_CIRCLE: u32 = 1;
pub const EE_APP_SIGHT_WHOLE: u32 = 2;

thread_local! {
    static DEPTH: Cell<i32> = const { Cell::new(0) };
}

/// The tiles from floor(c) - reach to floor(c) + reach on both axes (left, right, bottom, top): `Around`
fn around(x: f32, y: f32, radius: f32, clamp: i64) -> [i64; 4] {
    let mut reach: i64 = 0;
    if (0.0..100000.0).contains(&radius) {
        reach = radius.ceil() as c_long as i64;
    }
    if clamp >= 0 && reach > clamp {
        reach = clamp;
    }
    reach += 1;
    let cx = if x > -100000.0 && x < 100000.0 { x.floor() as c_long as i64 } else { 0 };
    let cy = if y > -100000.0 && y < 100000.0 { y.floor() as c_long as i64 } else { 0 };
    [cx - reach, cx + reach, cy - reach, cy + reach]
}

/// The rectangle the scope recorded: a circle's or step's reach (the circles clamp their radius to 32 tiles, the map's
/// sight does not), {0, -1, 0, -1} for a tile's call, {1, 0, 1, 0} for the whole map (left > right: the whole map)
fn rect(shape: u32, kind: u32, args: &[f32; 5]) -> [i64; 4] {
    match shape {
        EE_APP_SIGHT_TILE => [0, -1, 0, -1],
        EE_APP_SIGHT_CIRCLE => {
            let clamp = if kind == SIGHT_MAP_ADD || kind == SIGHT_MAP_REMOVE { -1 } else { 32 };
            let mut r = around(args[0], args[1], args[4], clamp);
            if kind == SIGHT_RING_ADD || kind == SIGHT_RING_REMOVE || kind == SIGHT_CIRCLE_MOVE {
                let o = around(args[2], args[3], args[4], clamp);
                r = [r[0].min(o[0]), r[1].max(o[1]), r[2].min(o[2]), r[3].max(o[3])];
            }
            r
        }
        _ => [1, 0, 1, 0],
    }
}

/// A tile's visibility words into the hash
fn tile_words(h: &mut Fnv, tile: TileView, words: usize) {
    for w in 0..words {
        h.u32(tile.visibility_word(w).unwrap_or(0));
    }
}

/// `RectHash`: every tile of the rectangle (clipped to the map), in row order
fn rect_hash(map: MapView, mesh: *const EeGeTerrainMesh, rect: [i64; 4]) -> (u64, u32) {
    let mut h = Fnv::default();
    let mut tiles = 0u32;
    let size = map.size();
    let [mut left, mut right, mut bottom, mut top] = rect;
    if left > right {
        left = 0;
        right = size - 1;
        bottom = 0;
        top = size - 1;
    }
    left = left.max(0);
    bottom = bottom.max(0);
    right = right.min(size - 1);
    top = top.min(size - 1);
    let words = map.visibility_words();
    for y in bottom..=top {
        for x in left..=right {
            tiles += 1;
            let Some(tile) = map.tile(x, y).filter(|t| t.has_visibility()) else {
                h.u32(0xfffffff1);
                continue;
            };
            tile_words(&mut h, tile, words);
            let (cx, cy) = (x as c_long as c_ulong, y as c_long as c_ulong);
            // SAFETY (each): the map's live mesh, a cell it checks
            if unsafe { ee_ge_tm_is_valid(mesh, cx, cy, true) } {
                h.u16(unsafe { (*ee_ge_tm_tile(mesh, cx, cy)).reference_count });
            } else {
                h.u32(0xfffffff2);
            }
        }
    }
    (h.0, tiles)
}

/// `EESightScope`'s constructors: true for the outermost sight call of this thread.
#[no_mangle]
pub extern "C" fn ee_app_sight_enter() -> bool {
    DEPTH.with(|d| {
        let outer = d.get() == 0;
        d.set(d.get() + 1);
        outer
    })
}

/// `EESightScope`'s destructor: leaves the scope; false when no record is due (a nested call, no capture).
#[no_mangle]
pub extern "C" fn ee_app_sight_leave(outer: bool) -> bool {
    DEPTH.with(|d| d.set(d.get() - 1));
    outer && capture::ee_app_capture_active()
}

/// The outermost scope's TSGT record. `map` is the world's `EEMap` and `mesh` its mesh; `player` the
/// `EEPlayer*` (0: none), `tile` the tile's `RTSTile*` (0: none) and `tile_index` its place in the map's tile array
/// (-1: not one of them); `args` the five float arguments.
///
/// # Safety
/// The caller holds the world; `map`, `mesh`, `player` and `tile` are live (or 0); `args` points to 5 floats.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_app_sight_record(
    shape: u32,
    kind: u32,
    player: u64,
    tile: u64,
    tile_index: c_long,
    tile_x: c_long,
    tile_y: c_long,
    value: c_ulong,
    args: *const f32,
    map: u64,
    mesh: *const EeGeTerrainMesh,
) {
    // SAFETY: the caller's contract
    let args = unsafe { *(args as *const [f32; 5]) };
    // SAFETY: the caller holds the world
    let token = unsafe { WorldRead::begin() };
    let Some(map) = (unsafe { MapView::of(&token, map) }) else { return };
    let rect = rect(shape, kind, &args);
    let (hash, tiles, index) = if tile != 0 || kind <= SIGHT_PLAYER_REMOVE_OWN {
        let mut h = Fnv::default();
        // SAFETY: the caller's live tile (or 0)
        match unsafe { TileView::of(&token, tile) }.filter(|t| t.has_visibility()) {
            Some(t) => {
                tile_words(&mut h, t, map.visibility_words());
                (h.0, 1, tile_index)
            }
            None => (h.0, 0, tile_index),
        }
    } else {
        let (h, n) = rect_hash(map, mesh, rect);
        (h, n, -1)
    };
    // SAFETY: the caller's live player (or 0)
    let player = unsafe { PlayerView::of(&token, player) }.map_or(0xffffffff, |p| p.id() as u32);
    let mut r = Payload::default();
    r.u32(kind).u32(player).u32(value as u32).i32(index as i32).i32(tile_x as i32).i32(tile_y as i32);
    for a in args {
        r.f32(a);
    }
    for side in rect {
        r.i32(side as i32);
    }
    r.u32(tiles).hash64(hash);
    capture::commit(tag(b"TSGT"), &r.0);
}

// ---- the scope (ee/EESightCapture.h's EESightScope: each constructor opens one, the destructor closes the last)

/// An open scope: whether it is the outermost, and what its record holds.
struct Scope {
    outer: bool,
    shape: u32,
    kind: u32,
    player: u64,
    tile: u64,
    tile_x: c_long,
    tile_y: c_long,
    value: c_ulong,
    args: [f32; 5],
}

thread_local! {
    static SCOPES: RefCell<Vec<Scope>> = const { RefCell::new(Vec::new()) };
}

fn open(shape: u32, kind: u32, player: *const c_void, tile: *const c_void, at: (c_long, c_long), value: c_ulong, args: [f32; 5]) {
    let outer = ee_app_sight_enter();
    let (tile_x, tile_y) = at;
    let scope = Scope { outer, shape, kind, player: player as u64, tile: tile as u64, tile_x, tile_y, value, args };
    SCOPES.with(|s| s.borrow_mut().push(scope));
}

/// `EESightScope`'s tile constructor: a tile's call (`x`, `y` the coordinates it takes, -1 when none; `value`
/// SetVisibility's).
#[no_mangle]
pub extern "C" fn ee_app_sight_open_tile(
    kind: u32,
    player: *const c_void,
    tile: *const c_void,
    x: c_long,
    y: c_long,
    value: c_ulong,
) {
    open(EE_APP_SIGHT_TILE, kind, player, tile, (x, y), value, [0.0; 5]);
}

/// `EESightScope`'s circle constructor: a circle around (`x0`, `y0`), or a step from it to (`x1`, `y1`); `value` the
/// map sight's shared flag.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_app_sight_open_circle(
    kind: u32,
    player: *const c_void,
    x0: f32,
    y0: f32,
    x1: f32,
    y1: f32,
    radius: f32,
    value: c_ulong,
) {
    open(EE_APP_SIGHT_CIRCLE, kind, player, std::ptr::null(), (-1, -1), value, [x0, y0, x1, y1, radius]);
}

/// `EESightScope`'s whole-map constructor.
#[no_mangle]
pub extern "C" fn ee_app_sight_open_whole(kind: u32, player: *const c_void) {
    open(EE_APP_SIGHT_WHOLE, kind, player, std::ptr::null(), (-1, -1), 0, [0.0; 5]);
}

/// `EESightScope`'s destructor: the last scope closed; the outermost one's TSGT record when the capture runs.
#[no_mangle]
pub extern "C" fn ee_app_sight_close() {
    let Some(s) = SCOPES.with(|v| v.borrow_mut().pop()) else { return };
    if ee_app_sight_leave(s.outer) {
        crate::gamecpp::map::ee_gamecpp_map_sight_record(
            s.shape,
            s.kind,
            s.player,
            s.tile,
            s.tile_x,
            s.tile_y,
            s.value,
            s.args.as_ptr(),
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_scopes_nest_and_only_the_outermost_is_outer() {
        ee_app_sight_open_whole(15, std::ptr::null());
        ee_app_sight_open_tile(1, std::ptr::null(), std::ptr::null(), 2, 3, 0);
        SCOPES.with(|s| {
            let s = s.borrow();
            assert_eq!(s.len(), 2);
            assert!(s[0].outer && !s[1].outer);
            assert_eq!((s[1].shape, s[1].tile_x, s[1].tile_y), (EE_APP_SIGHT_TILE, 2, 3));
        });
        ee_app_sight_close();
        ee_app_sight_close();
        ee_app_sight_open_circle(8, std::ptr::null(), 1.0, 2.0, 0.0, 0.0, 3.0, 1);
        SCOPES.with(|s| {
            let s = s.borrow();
            assert!(s[0].outer);
            assert_eq!(s[0].args, [1.0, 2.0, 0.0, 0.0, 3.0]);
        });
        ee_app_sight_close();
        SCOPES.with(|s| assert!(s.borrow().is_empty()));
    }

    #[test]
    fn rects_as_the_scope_made_them() {
        assert_eq!(rect(EE_APP_SIGHT_TILE, 1, &[0.0; 5]), [0, -1, 0, -1]);
        assert_eq!(rect(EE_APP_SIGHT_WHOLE, 19, &[0.0; 5]), [1, 0, 1, 0]);
        // a circle of radius 2.5 at (10.2, 3.9): reach ceil(2.5) + 1 = 4
        assert_eq!(rect(EE_APP_SIGHT_CIRCLE, 10, &[10.2, 3.9, 0.0, 0.0, 2.5]), [6, 14, -1, 7]);
        // the circles clamp the reach to 32 tiles, the map's sight does not
        assert_eq!(rect(EE_APP_SIGHT_CIRCLE, 10, &[0.0, 0.0, 0.0, 0.0, 50.0]), [-33, 33, -33, 33]);
        assert_eq!(rect(EE_APP_SIGHT_CIRCLE, SIGHT_MAP_ADD, &[0.0, 0.0, 0.0, 0.0, 50.0]), [-51, 51, -51, 51]);
        // a step covers both ends
        assert_eq!(rect(EE_APP_SIGHT_CIRCLE, SIGHT_CIRCLE_MOVE, &[0.5, 0.5, 5.5, -2.5, 1.0]), [-2, 7, -5, 2]);
        // a radius out of range reaches one tile; a centre out of range is at 0
        assert_eq!(around(1e9, -1e9, -1.0, 32), [-1, 1, -1, 1]);
        assert_eq!(around(f32::NAN, 2.0, f32::NAN, -1), [-1, 1, 1, 3]);
    }

    #[test]
    fn nesting() {
        assert!(ee_app_sight_enter());
        assert!(!ee_app_sight_enter());
        assert!(!ee_app_sight_leave(false));
        assert!(!ee_app_sight_leave(true)); // no capture in a test
        assert!(ee_app_sight_enter());
        ee_app_sight_leave(true);
    }
}
