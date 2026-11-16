//! `EEMap`'s C ABI (include/rust/ee_app.h): one host record per call (the C++ callback, its context, the terrain
//! mesh, the map's numbers). The mesh is ee_ge's object, read and edited here directly; the rest are host ops.
#![allow(clippy::unnecessary_cast)]

use crate::map::{self, EeAppMapDims, MapHost, Terrain};
use ee_ge::tmobj::{
    ee_ge_tm_is_space_tile, ee_ge_tm_is_valid, ee_ge_tm_point, ee_ge_tm_point_edit, ee_ge_tm_tile, ee_ge_tm_tile_edit,
    EeGeTerrainMesh,
};
use ee_ge::types::GeVector;
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// One host operation's arguments and answers.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAppMapIo {
    pub x: c_long,
    pub y: c_long,
    pub a: c_long,
    pub b: c_long,
    pub c: c_long,
    pub flag: bool,
    /// the point and radius of `TILES_NEAR`
    pub f: [f32; 3],
    /// `TILES_NEAR`'s answer: `a` tiles of two `long`s (the C++ keeps them until its next call)
    pub tiles: *const c_long,
}

impl Default for EeAppMapIo {
    fn default() -> Self {
        EeAppMapIo { x: 0, y: 0, a: 0, b: 0, c: 0, flag: false, f: [0.0; 3], tiles: std::ptr::null() }
    }
}

/// The host: `(ctx, op, io)`.
pub type EeAppMapFn = unsafe extern "C" fn(*mut c_void, i32, *mut EeAppMapIo);

// ---- the operations (io's fields: in -> out)
/// x, y -> flag: `GetTile(x, y) != NULL`
pub const EE_APP_MAP_OP_TILE: i32 = 1;
/// x, y -> a: the tile's terrain type; x, y, a: `SetTerrainType(a)`
pub const EE_APP_MAP_OP_TERRAIN_TYPE: i32 = 2;
pub const EE_APP_MAP_OP_SET_TERRAIN_TYPE: i32 = 3;
/// x, y, a, flag: `SetTileTerrain(x, y, a, flag)`
pub const EE_APP_MAP_OP_SET_TILE_TERRAIN: i32 = 4;
/// a -> a, b, c: `EETerrainTable::Get(a)`'s texture set, terrain type and colour type
pub const EE_APP_MAP_OP_TERRAIN: i32 = 5;
/// a -> a: `EETerrainTypeTable::Get(a)->mColorSetID`
pub const EE_APP_MAP_OP_COLOR_SET: i32 = 6;
/// a -> a, b: `EETerrainTable::sCliffTable[a]`'s first and last terrains
pub const EE_APP_MAP_OP_CLIFF: i32 = 7;
/// x, y -> a: the tile's `GetChecksum(players)`
pub const EE_APP_MAP_OP_TILE_CHECKSUM: i32 = 8;
/// -> flag: `gSightUpdatesEnabled`; flag (shared) -> flag: the local player sees through the player's sight
pub const EE_APP_MAP_OP_SIGHT_ENABLED: i32 = 9;
pub const EE_APP_MAP_OP_LOCAL_SEES: i32 = 10;
/// f -> tiles, a: `EEGetTilesNearPoint`
pub const EE_APP_MAP_OP_TILES_NEAR: i32 = 11;
/// x, y, flag (add), a (shared): the player's sight at the tile
pub const EE_APP_MAP_OP_SIGHT_AT: i32 = 12;
/// x, y, flag (on): `gMiniMap->CountCellOn/Off`
pub const EE_APP_MAP_OP_COUNT_CELL: i32 = 13;

/// A call's host: the callback and its context, the mesh (`EeGeTerrainMesh`) and the map's numbers.
#[repr(C)]
pub struct EeAppMapHost {
    pub call: EeAppMapFn,
    pub ctx: *mut c_void,
    pub mesh: *mut c_void,
    pub dims: EeAppMapDims,
}

struct Host<'a> {
    h: &'a EeAppMapHost,
}

impl Host<'_> {
    fn op(&mut self, op: i32, io: EeAppMapIo) -> EeAppMapIo {
        let mut io = io;
        // SAFETY: the C++ passed a live context and its callback for the entry's duration
        unsafe { (self.h.call)(self.h.ctx, op, &mut io) };
        io
    }
    fn at(&mut self, op: i32, x: c_long, y: c_long) -> EeAppMapIo {
        self.op(op, EeAppMapIo { x, y, ..Default::default() })
    }
    fn mesh(&self) -> *mut EeGeTerrainMesh {
        self.h.mesh.cast()
    }
}

// SAFETY (every mesh call below): the C++ passed its map's live mesh; the coordinates are the C++'s own (a point or
// tile the original read without a test is one the mesh holds)
impl MapHost for Host<'_> {
    fn tile(&mut self, x: c_long, y: c_long) -> bool {
        self.at(EE_APP_MAP_OP_TILE, x, y).flag
    }
    fn terrain_type(&mut self, x: c_long, y: c_long) -> c_long {
        self.at(EE_APP_MAP_OP_TERRAIN_TYPE, x, y).a
    }
    fn set_terrain_type(&mut self, x: c_long, y: c_long, terrain_type: c_long) {
        self.op(EE_APP_MAP_OP_SET_TERRAIN_TYPE, EeAppMapIo { x, y, a: terrain_type, ..Default::default() });
    }
    fn set_tile_terrain(&mut self, x: c_long, y: c_long, texture_set: c_long, unload: bool) {
        self.op(
            EE_APP_MAP_OP_SET_TILE_TERRAIN,
            EeAppMapIo { x, y, a: texture_set, flag: unload, ..Default::default() },
        );
    }
    fn terrain(&mut self, id: c_long) -> Terrain {
        let io = self.op(EE_APP_MAP_OP_TERRAIN, EeAppMapIo { a: id, ..Default::default() });
        Terrain { texture_set: io.a, terrain_type: io.b, color_type: io.c }
    }
    fn color_set(&mut self, color_type: c_long) -> c_ulong {
        self.op(EE_APP_MAP_OP_COLOR_SET, EeAppMapIo { a: color_type, ..Default::default() }).a as c_ulong
    }
    fn cliff(&mut self, id: c_long) -> (c_long, c_long) {
        let io = self.op(EE_APP_MAP_OP_CLIFF, EeAppMapIo { a: id, ..Default::default() });
        (io.a, io.b)
    }
    fn tile_checksum(&mut self, x: c_long, y: c_long) -> c_long {
        self.at(EE_APP_MAP_OP_TILE_CHECKSUM, x, y).a
    }
    fn z(&mut self, x: c_ulong, y: c_ulong) -> f32 {
        unsafe { (*ee_ge_tm_point(self.mesh(), x, y)).z }
    }
    fn normal(&mut self, x: c_ulong, y: c_ulong) -> GeVector {
        unsafe { *(*ee_ge_tm_point(self.mesh(), x, y)).normal }
    }
    fn texture_set(&mut self, x: c_ulong, y: c_ulong) -> c_ulong {
        unsafe { (*ee_ge_tm_tile(self.mesh(), x, y)).texture_set_id as c_ulong }
    }
    fn valid_tile(&mut self, x: c_ulong, y: c_ulong) -> bool {
        unsafe { ee_ge_tm_is_valid(self.mesh(), x, y, true) }
    }
    fn valid_point(&mut self, x: c_ulong, y: c_ulong) -> bool {
        unsafe { ee_ge_tm_is_valid(self.mesh(), x, y, false) }
    }
    fn water_tile(&mut self, x: c_ulong, y: c_ulong) -> bool {
        // `kWaterInfoWaterMask`: every bit but space
        unsafe { (*ee_ge_tm_tile(self.mesh(), x, y)).water_info & 0x7f != 0 }
    }
    fn space_tile(&mut self, x: c_ulong, y: c_ulong) -> bool {
        unsafe { ee_ge_tm_is_space_tile(self.mesh(), x, y) }
    }
    fn set_color_set(&mut self, x: c_ulong, y: c_ulong, set: c_ulong) {
        unsafe { ee_ge_tm_point_edit(self.mesh(), x, y, 2, set, 0.0) }
    }
    fn set_water_color_set(&mut self, x: c_ulong, y: c_ulong) {
        unsafe { ee_ge_tm_point_edit(self.mesh(), x, y, 3, 0, 0.0) }
    }
    fn reference_tile(&mut self, x: c_ulong, y: c_ulong, on: bool) {
        unsafe { ee_ge_tm_tile_edit(self.mesh(), x, y, if on { 0 } else { 1 }, 0) }
    }
    fn sight_enabled(&mut self) -> bool {
        self.op(EE_APP_MAP_OP_SIGHT_ENABLED, EeAppMapIo::default()).flag
    }
    fn local_sees(&mut self, shared: bool) -> bool {
        self.op(EE_APP_MAP_OP_LOCAL_SEES, EeAppMapIo { flag: shared, ..Default::default() }).flag
    }
    fn tiles_near(&mut self, x: f32, y: f32, radius: f32, out: &mut Vec<(c_long, c_long)>) {
        let io = self.op(EE_APP_MAP_OP_TILES_NEAR, EeAppMapIo { f: [x, y, radius], ..Default::default() });
        for i in 0..io.a.max(0) as usize {
            // SAFETY: the C++ answers `a` pairs that live until its next call
            unsafe { out.push((*io.tiles.add(2 * i), *io.tiles.add(2 * i + 1))) };
        }
    }
    fn sight_at(&mut self, x: c_long, y: c_long, add: bool, shared: bool) {
        self.op(EE_APP_MAP_OP_SIGHT_AT, EeAppMapIo { x, y, flag: add, a: shared as c_long, ..Default::default() });
    }
    fn count_cell(&mut self, x: c_long, y: c_long, on: bool) {
        self.op(EE_APP_MAP_OP_COUNT_CELL, EeAppMapIo { x, y, flag: on, ..Default::default() });
    }
}

/// The map's bodies by entry (`what`): 0 `UpdateWater(a, b)`, 1 `CreateMap`'s fill, 2 `UpdateCliffs(a, b, c, d)`,
/// 3 `RemoveFlatCliffs(a, b, c, d)`, 4 `PaintTerrain(a, b, c)`, 5 `PaintTerrainRect(a, b, c, d, e, f)`,
/// 6 `SetCliffColorSets(a, b, c, d, e)`.
///
/// # Safety
/// `host` is a live record whose callback, context and mesh stay live for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_app_map_edit(host: *const EeAppMapHost, what: u32, args: *const c_long) {
    // SAFETY: the function's contract; `args` holds the entry's arguments (six)
    let (h, a) = unsafe { (&*host, std::slice::from_raw_parts(args, 6)) };
    let d = h.dims;
    let mut host = Host { h };
    match what {
        0 => map::update_water(&mut host, &d, a[0], a[1]),
        1 => map::fill_base_terrain(&mut host, &d),
        2 => map::update_cliffs(&mut host, &d, a[0], a[1], a[2], a[3]),
        3 => map::remove_flat_cliffs(&mut host, &d, a[0] as c_ulong, a[1] as c_ulong, a[2] as c_ulong, a[3] as c_ulong),
        4 => map::paint_terrain(&mut host, &d, a[0], a[1], a[2]),
        5 => map::paint_terrain_rect(&mut host, &d, a[0], a[1], a[2], a[3], a[4], a[5]),
        _ => map::set_cliff_color_sets(&mut host, &d, a[0], a[1], a[2], a[3], a[4]),
    }
}

/// `AddSight` (`add`) / `RemoveSight` of the player the context names.
///
/// # Safety
/// As [`ee_app_map_edit`].
#[no_mangle]
pub unsafe extern "C" fn ee_app_map_sight(
    host: *const EeAppMapHost,
    x: f32,
    y: f32,
    radius: f32,
    shared: bool,
    add: bool,
) {
    // SAFETY: the function's contract
    let mut host = Host { h: unsafe { &*host } };
    map::sight(&mut host, x, y, radius, shared, add);
}

/// `ComputeChecksum`: the window's sum; `cursor` advanced.
///
/// # Safety
/// As [`ee_app_map_edit`]; `cursor` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_app_map_checksum(host: *const EeAppMapHost, cursor: *mut c_ulong) -> c_long {
    // SAFETY: the function's contract
    let (h, cursor) = unsafe { (&*host, &mut *cursor) };
    let d = h.dims;
    map::compute_checksum(&mut Host { h }, &d, cursor)
}

/// `GetTileCount`.
///
/// # Safety
/// `dims` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_app_map_tile_count(dims: *const EeAppMapDims) -> c_ulong {
    // SAFETY: the function's contract
    map::tile_count(unsafe { &*dims })
}
