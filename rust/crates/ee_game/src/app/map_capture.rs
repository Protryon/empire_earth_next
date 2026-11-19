//! TEST ONLY: the capture's map records (ee/EEAppCapture.h; tools/sim_capture.py app profile; was
//! EEAppCaptureWorld.cpp's `MapRectHash`, `EEAppMapScope` and `EEAppCaptureSight`), over the world views and the
//! terrain mesh's Rust object.
//!
//! - MAPC: one of EEMap's terrain edits entered from outside the map; when the outermost one returns, its kind and
//!   arguments and an FNV-1a of the map around its rectangle (the tiles' terrain types, the mesh's tiles, points and
//!   normals). Nested edits record nothing.
//! - MAPS: after a sight change (AddSight 0, RemoveSight 1): its arguments and an FNV-1a of the tiles near the point
//!   (each tile's visibility words and the mesh tile's reference count).
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use std::cell::Cell;
use std::os::raw::{c_long, c_ulong};

use ee_render::ge::tmobj::{ee_ge_tm_is_valid, ee_ge_tm_point, ee_ge_tm_tile, EeGeTerrainMesh};
use ee_sim::app::capture;
use ee_sim::units::uu_tiles::tiles_near_point;
use ee_sim::world::{MapView, WorldRead, WorldView};

const FNV_BASIS: u64 = 0xcbf29ce484222325;

/// The FNV-1a of the C++ capture (`Fnv`, `FnvValue`).
#[derive(Clone, Copy)]
pub struct Fnv(pub u64);

impl Default for Fnv {
    fn default() -> Self {
        Fnv(FNV_BASIS)
    }
}

impl Fnv {
    pub fn bytes(&mut self, data: &[u8]) {
        for &b in data {
            self.0 = (self.0 ^ b as u64).wrapping_mul(0x100000001b3);
        }
    }
    pub fn u8(&mut self, v: u8) {
        self.bytes(&[v]);
    }
    pub fn u16(&mut self, v: u16) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn i32(&mut self, v: i32) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn u32(&mut self, v: u32) {
        self.bytes(&v.to_le_bytes());
    }
    pub fn f32(&mut self, v: f32) {
        self.bytes(&v.to_le_bytes());
    }
}

/// A record's payload being written (the C++'s `U32`, `I32`, `F32`, `Hash64`).
#[derive(Default)]
pub struct Payload(pub Vec<u8>);

impl Payload {
    pub fn u32(&mut self, v: u32) -> &mut Self {
        self.0.extend_from_slice(&v.to_le_bytes());
        self
    }
    pub fn i32(&mut self, v: i32) -> &mut Self {
        self.0.extend_from_slice(&v.to_le_bytes());
        self
    }
    pub fn f32(&mut self, v: f32) -> &mut Self {
        self.0.extend_from_slice(&v.to_le_bytes());
        self
    }
    pub fn hash64(&mut self, v: u64) -> &mut Self {
        self.u32(v as u32).u32((v >> 32) as u32)
    }
}

/// The capture's tag of a four-letter name (`EECaptureTag`).
pub fn tag(name: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*name)
}

/// The mesh's cell (the C++ passes a `long` where the mesh takes an `unsigned long`).
fn cell(v: i64) -> c_ulong {
    v as c_long as c_ulong
}

/// `MapRectHash`: the map around the tile rectangle x in [left, right], y in [bottom, top] (one tile and point more
/// on each side); left > right: the whole map.
fn map_rect_hash(map: Option<MapView>, mesh: *const EeGeTerrainMesh, rect: [i64; 4]) -> u64 {
    let mut h = Fnv::default();
    let Some(map) = map else { return h.0 };
    let [mut left, mut right, mut bottom, mut top] = rect;
    if left > right {
        left = 0;
        right = map.size() - 1;
        bottom = 0;
        top = map.size() - 1;
    }
    for y in bottom - 1..=top + 1 {
        for x in left - 1..=right + 1 {
            h.i32(map.tile(x, y).map(|t| t.terrain_type() as i32).unwrap_or(-12345));
            // SAFETY (each): the map's live mesh, a cell it checks
            if unsafe { ee_ge_tm_is_valid(mesh, cell(x), cell(y), true) } {
                let t = unsafe { &*ee_ge_tm_tile(mesh, cell(x), cell(y)) };
                h.u8(t.texture_set_id);
                h.u8(t.water_info);
            } else {
                h.u32(0xffffffff);
            }
        }
    }
    for y in bottom - 1..=top + 2 {
        for x in left - 1..=right + 2 {
            // SAFETY (each): as above
            if !unsafe { ee_ge_tm_is_valid(mesh, cell(x), cell(y), false) } {
                h.u32(0xffffffff);
                continue;
            }
            let p = unsafe { &*ee_ge_tm_point(mesh, cell(x), cell(y)) };
            h.f32(p.z);
            // SAFETY: a valid point's normal
            let n = unsafe { &*p.normal };
            h.f32(n.x);
            h.f32(n.y);
            h.f32(n.z);
            h.u8(p.color_table_index);
        }
    }
    h.0
}

thread_local! {
    static DEPTH: Cell<i32> = const { Cell::new(0) };
}

/// `EEAppMapScope`'s constructor: true for the outermost edit of this thread.
#[no_mangle]
pub extern "C" fn ee_app_capture_map_enter() -> bool {
    DEPTH.with(|d| {
        let outer = d.get() == 0;
        d.set(d.get() + 1);
        outer
    })
}

/// `EEAppMapScope`'s destructor: the MAPC record of the outermost edit. `args` holds 6 arguments, `rect` the
/// rectangle (left, right, bottom, top); `map` is the edited `EEMap` (not always the world's), `mesh` its mesh.
///
/// # Safety
/// `args` and `rect` point to 6 and 4 values; `map` and `mesh` are live (or 0) and the caller holds them.
#[no_mangle]
pub unsafe extern "C" fn ee_app_capture_map_leave(
    outer: bool,
    map: u64,
    mesh: *const EeGeTerrainMesh,
    kind: u32,
    args: *const c_long,
    rect: *const c_long,
) {
    DEPTH.with(|d| d.set(d.get() - 1));
    if !outer || !capture::ee_app_capture_active() {
        return;
    }
    // SAFETY: the caller's contract
    let (args, rect) = unsafe { (*(args as *const [c_long; 6]), *(rect as *const [c_long; 4])) };
    // SAFETY: the caller holds the map
    let token = unsafe { WorldRead::begin() };
    let view = unsafe { MapView::of(&token, map) };
    let mut r = Payload::default();
    r.u32(kind);
    for a in args {
        r.i32(a as i32);
    }
    r.hash64(map_rect_hash(view, mesh, rect.map(|v| v as i64)));
    capture::commit(tag(b"MAPC"), &r.0);
}

/// MAPS: after a sight change of the world's map (`player` its id, 0xffffffff none; `mesh` the map's).
///
/// # Safety
/// The caller holds the world; `mesh` is the world map's live mesh.
#[no_mangle]
pub unsafe extern "C" fn ee_app_capture_sight(
    kind: u32,
    player: u32,
    x: f32,
    y: f32,
    radius: f32,
    shared: bool,
    mesh: *const EeGeTerrainMesh,
) {
    if !capture::ee_app_capture_active() {
        return;
    }
    // SAFETY: the caller holds the world
    let token = unsafe { WorldRead::begin() };
    let Some(map) = WorldView::global(&token).and_then(|w| w.map()) else { return };
    let mut tiles = Vec::new();
    tiles_near_point(x, y, radius, &mut tiles);
    let words = map.visibility_words();
    let mut h = Fnv::default();
    for &[tx, ty] in &tiles {
        h.i32(tx);
        h.i32(ty);
        let Some(tile) = map.tile(tx as i64, ty as i64) else { continue };
        for w in 0..words {
            h.u32(tile.visibility_word(w).unwrap_or(0));
        }
        // SAFETY (each): the map's live mesh
        if unsafe { ee_ge_tm_is_valid(mesh, cell(tx as i64), cell(ty as i64), true) } {
            h.u16(unsafe { (*ee_ge_tm_tile(mesh, cell(tx as i64), cell(ty as i64))).reference_count });
        }
    }
    let mut r = Payload::default();
    r.u32(kind).u32(player).f32(x).f32(y).f32(radius).u32(shared as u32).u32(tiles.len() as u32).hash64(h.0);
    capture::commit(tag(b"MAPS"), &r.0);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hashes_and_payloads() {
        let mut h = Fnv::default();
        h.bytes(b"a");
        assert_eq!(h.0, 0xaf63dc4c8601ec8c);
        assert_eq!(map_rect_hash(None, std::ptr::null(), [0, 0, 0, 0]), FNV_BASIS);
        let mut p = Payload::default();
        p.u32(1).i32(-1).hash64(0x1122334455667788);
        assert_eq!(p.0, [1, 0, 0, 0, 255, 255, 255, 255, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11]);
        assert_eq!(tag(b"MAPC"), u32::from_le_bytes(*b"MAPC"));
        assert!(ee_app_capture_map_enter());
        assert!(!ee_app_capture_map_enter());
        DEPTH.with(|d| d.set(0));
    }
}
