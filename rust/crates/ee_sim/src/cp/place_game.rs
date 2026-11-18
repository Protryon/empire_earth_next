//! The placement's and resource centres' host in the game ([`PlaceHost`] over `ee_cp_place_host`, EECPPlacement.cpp,
//! and `ee_cp_center_host`, EECPResourceCenter.cpp) and the entry points the C++ classes forward to. Absent from a
//! test binary.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast)]
#![allow(clippy::too_many_arguments)]

use std::ffi::c_void;
use std::os::raw::c_long;

use super::fields::EeCpFields;
use super::file::SaveFile;
use super::place_host::{EeCpUnitInfo, PlaceHost, Rect, RectFilter, Tile};
use super::placement::{self as pl, EeCpPlaceResult, EeCpPlacement};
use super::resource_center as rc;
use crate::units::player_save::EeUnitsPlayerSaveFn;

/// What the C++ hands every call: the computer player and its placement.
#[repr(C)]
pub struct EeCpPlaceCtx {
    /// the `EEComputerPlayer*` (its `EEPlayer*`: the first base)
    pub player: u64,
    /// `mPlayerID`
    pub player_id: u32,
    /// the player's record (`static_cast<EeCpFields*>(player)`)
    pub fields: *const EeCpFields,
    /// `mPlacement->mTownCenterTile`'s `mX`, `mY` (null: no placement)
    pub town: *mut c_long,
    /// `mPlacement->mRust` (null: no placement)
    pub placement: *mut EeCpPlacement,
    /// `EESimCaptureRecord::Active()`: the tile lists go through the C++ wrappers (their TEST ONLY notes)
    pub capture: u8,
}

/// One host operation's values.
#[repr(C)]
pub struct EeCpPlaceIo {
    /// up to six integer arguments and results
    pub v: [i64; 6],
    /// float results
    pub f: [f32; 3],
    /// UNIT_INFO's result
    pub unit: EeCpUnitInfo,
    /// the list an operation starts from (`input_len` pairs; the sort's keys: `input_len` single values)
    pub input: *const i64,
    pub input_len: usize,
    /// the list an operation pushes onto (`ee_cp_place_push`)
    pub list: *mut c_void,
    /// the sort's order out (`input_len` indices)
    pub order: *mut u32,
}

// ---- the host operations (`op`; `a`, `b` objects, `io.v` values). The placement's (EECPPlacement.cpp):
pub const EE_CP_PLACE_EPOCH: i32 = 1;
pub const EE_CP_PLACE_PASSABILITY: i32 = 2;
/// `mUnitClassUpgrades[v0]->mRange` into `f0`
pub const EE_CP_PLACE_RANGE: i32 = 3;
/// `GetTechTree()->Get(v0)`: its `mData->mKind` into `v0`, whether there is one returned
pub const EE_CP_PLACE_TECH_KIND: i32 = 4;
pub const EE_CP_PLACE_HAS_PATHFINDING: i32 = 5;
pub const EE_CP_PLACE_CLAIM_RANGE_ON: i32 = 6;
pub const EE_CP_PLACE_CLAIM_RANGE: i32 = 7;
/// the player's units from `mFirstUnit` (pushed)
pub const EE_CP_PLACE_OWN_UNITS: i32 = 8;
/// `EEGetPlayerUnitsOfType(v0, v1, units)` on the input list (the result pushed)
pub const EE_CP_PLACE_UNITS_OF_TYPE: i32 = 9;
/// player 0's units (pushed); 0 returned without a player 0
pub const EE_CP_PLACE_NATURE_UNITS: i32 = 10;
/// `mTowns`: (continent or -1 for a null town, the hull centre packed x << 32 | y) pushed
pub const EE_CP_PLACE_TOWNS: i32 = 11;
/// `mResourceCenters` (pushed)
pub const EE_CP_PLACE_CENTERS: i32 = 12;
pub const EE_CP_PLACE_HAS_PLAYER0: i32 = 13;
/// `gWorld.GetUnit(v0)`
pub const EE_CP_PLACE_WORLD_UNIT: i32 = 14;
/// unit `a` into `io.unit`
pub const EE_CP_PLACE_UNIT_INFO: i32 = 15;
pub const EE_CP_PLACE_CLASS_COUNT: i32 = 17;
pub const EE_CP_PLACE_CLASS_EXISTS: i32 = 18;
pub const EE_CP_PLACE_CLASS_TILE_SIZE: i32 = 19;
pub const EE_CP_PLACE_CLASS_TERRAIN: i32 = 20;
pub const EE_CP_PLACE_CLASS_TECHNOLOGY: i32 = 21;
pub const EE_CP_PLACE_HAS_MAP: i32 = 22;
pub const EE_CP_PLACE_MAP_FLAGS: i32 = 23;
/// `GetTileAtLocation(v0, v1)`'s terrain (-1: no tile)
pub const EE_CP_PLACE_TILE_TERRAIN: i32 = 24;
/// `GetTile(v0, v1)`'s terrain (-1: no tile)
pub const EE_CP_PLACE_MAP_TILE_TERRAIN: i32 = 25;
/// `GetTileAtLocation(v0, v1)`: -1 no tile, 1 its `mUnits` NULL, 0 not
pub const EE_CP_PLACE_TILE_FREE: i32 = 26;
pub const EE_CP_PLACE_IS_EDGE: i32 = 27;
pub const EE_CP_PLACE_LARGEST_CONTINENT: i32 = 28;
pub const EE_CP_PLACE_CONTINENT_FROM_ID: i32 = 29;
pub const EE_CP_PLACE_CONTINENT_BY_ID: i32 = 30;
pub const EE_CP_PLACE_CONTINENT_AT: i32 = 31;
pub const EE_CP_PLACE_CONTINENT_ID: i32 = 32;
pub const EE_CP_PLACE_CONTINENT_FLAG: i32 = 33;
/// continent `a`'s `GetShoreNextTo(v0, tiles)` on the input tiles (the result pushed)
pub const EE_CP_PLACE_SHORE_NEXT_TO: i32 = 34;
pub const EE_CP_PLACE_SHORE_TILES: i32 = 35;
/// continent (`v0` 0) or choke point (`v0` 1) `a`'s `GetHullCenter()` into `v0`, `v1`
pub const EE_CP_PLACE_HULL_CENTER: i32 = 36;
pub const EE_CP_PLACE_ARE_CONNECTED: i32 = 37;
pub const EE_CP_PLACE_TILE_CONTINENT: i32 = 38;
pub const EE_CP_PLACE_UNIT_CONTINENT: i32 = 39;
pub const EE_CP_PLACE_CONTINENT_ID_AT: i32 = 40;
pub const EE_CP_PLACE_CHOKE_AT: i32 = 41;
pub const EE_CP_PLACE_HILL_AT: i32 = 42;
pub const EE_CP_PLACE_PATH_AREA_AT: i32 = 43;
pub const EE_CP_PLACE_UNIT_PATH_AREA: i32 = 44;
pub const EE_CP_PLACE_CHOKE_POINTS: i32 = 45;
/// area `a`'s `GetHullBounds()` into `v0..v3` (left, top, right, bottom)
pub const EE_CP_PLACE_HULL_BOUNDS: i32 = 46;
/// choke point `a`'s `GetTileSet().BuildPointList(tiles, 1)` on the input tiles
pub const EE_CP_PLACE_CHOKE_TILES: i32 = 47;
pub const EE_CP_PLACE_BLOCK_TAKES: i32 = 48;
pub const EE_CP_PLACE_FACING_QUARTER: i32 = 49;
pub const EE_CP_PLACE_SITE_THREAT: i32 = 50;
pub const EE_CP_PLACE_TILE_UNIT_OF_TYPE: i32 = 51;
/// `EEGetUnitsInRectangle(rect v0..v3, units, filter v4, true)` on the input units
pub const EE_CP_PLACE_UNITS_IN_RECT: i32 = 52;
pub const EE_CP_PLACE_RANDOM: i32 = 53;
pub const EE_CP_PLACE_SPACE_DOCK_OPEN: i32 = 54;
/// `std::sort` of the input keys: the order into `io.order`
pub const EE_CP_PLACE_SORT: i32 = 55;
/// the tile lists through their C++ wrappers (the capture's notes)
pub const EE_CP_PLACE_SQUARE_RING_TILES: i32 = 56;
pub const EE_CP_PLACE_SQUARE_BLOCK_RING: i32 = 57;
pub const EE_CP_PLACE_SQUARE_RING: i32 = 58;
pub const EE_CP_PLACE_SIDE_TILES: i32 = 59;
pub const EE_CP_PLACE_CORNER_TILES: i32 = 60;
pub const EE_CP_PLACE_TILES_IN_RADIUS: i32 = 61;
// the centres' (EECPResourceCenter.cpp; `a` the centre)
/// `mUnits`' saved ids (pushed)
pub const EE_CP_CENTER_UNITS: i32 = 100;
pub const EE_CP_CENTER_UNIT: i32 = 101;
pub const EE_CP_CENTER_ACTIVE: i32 = 102;
/// `mUnits` from the input ids
pub const EE_CP_CENTER_SET_ORDER: i32 = 103;
pub const EE_CP_CENTER_CLEAR: i32 = 104;
/// `AddUnit(b)`
pub const EE_CP_CENTER_ADD_UNIT: i32 = 105;
/// Restore's main unit from saved id `v0`
pub const EE_CP_CENTER_RESTORE_MAIN: i32 = 106;
/// unit `a`'s `DistanceSquaredToUnit(b)` into `f0`
pub const EE_CP_CENTER_DISTANCE_TO_UNIT: i32 = 107;
pub const EE_CP_CENTER_HAS_AREA_EFFECT: i32 = 108;
/// unit `a`'s morale-from-buildings effect: found returned, its radius in `f0`
pub const EE_CP_CENTER_MORALE_RADIUS: i32 = 109;
/// the same recounted: found returned, level `v0`, maximum `v1`, radius `f0`
pub const EE_CP_CENTER_MORALE_RECOUNT: i32 = 110;
/// class `v0`'s upgrade's four area effect templates into `v0..v3`
pub const EE_CP_CENTER_EFFECT_TEMPLATES: i32 = 111;
pub const EE_CP_CENTER_TEMPLATE_RADIUS: i32 = 112;

// (the context's placement pointer is opaque to the C++, which only hands it back)
#[allow(improper_ctypes)]
extern "C" {
    fn ee_cp_place_host(ctx: *const EeCpPlaceCtx, op: i32, a: u64, b: u64, io: *mut EeCpPlaceIo) -> i64;
    fn ee_cp_center_host(ctx: *const EeCpPlaceCtx, op: i32, a: u64, b: u64, io: *mut EeCpPlaceIo) -> i64;
}

/// Pushes a pair onto a host operation's list.
///
/// # Safety
/// `list` is the `EeCpPlaceIo::list` of a running operation.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_place_push(list: *mut c_void, a: i64, b: i64) {
    // SAFETY: the caller's contract
    unsafe { (*(list as *mut Vec<[i64; 2]>)).push([a, b]) }
}

struct Game {
    ctx: *const EeCpPlaceCtx,
}

impl Game {
    fn ctx(&self) -> &EeCpPlaceCtx {
        // SAFETY: the C++ caller's context lives for the call
        unsafe { &*self.ctx }
    }
    fn io() -> EeCpPlaceIo {
        EeCpPlaceIo {
            v: [0; 6],
            f: [0.0; 3],
            unit: EeCpUnitInfo::default(),
            input: std::ptr::null(),
            input_len: 0,
            list: std::ptr::null_mut(),
            order: std::ptr::null_mut(),
        }
    }
    fn call(&mut self, op: i32, a: u64, b: u64, io: &mut EeCpPlaceIo) -> i64 {
        // SAFETY: the C++ switch reads the live objects the bodies hand it
        unsafe {
            if op >= 100 {
                ee_cp_center_host(self.ctx, op, a, b, io)
            } else {
                ee_cp_place_host(self.ctx, op, a, b, io)
            }
        }
    }
    fn q(&mut self, op: i32, a: u64, v: &[i64]) -> i64 {
        let mut io = Self::io();
        io.v[..v.len()].copy_from_slice(v);
        self.call(op, a, 0, &mut io)
    }
    fn qf(&mut self, op: i32, a: u64, b: u64, v: &[i64]) -> (i64, EeCpPlaceIo) {
        let mut io = Self::io();
        io.v[..v.len()].copy_from_slice(v);
        let r = self.call(op, a, b, &mut io);
        (r, io)
    }
    /// An operation that pushes a list, from an input list of pairs.
    fn list(&mut self, op: i32, a: u64, v: &[i64], input: &[[i64; 2]]) -> (i64, Vec<[i64; 2]>) {
        let mut out: Vec<[i64; 2]> = Vec::new();
        let mut io = Self::io();
        io.v[..v.len()].copy_from_slice(v);
        io.input = input.as_ptr() as *const i64;
        io.input_len = input.len();
        io.list = std::ptr::addr_of_mut!(out).cast();
        let r = self.call(op, a, 0, &mut io);
        (r, out)
    }
    fn tiles(&mut self, op: i32, a: u64, v: &[i64], input: &[Tile]) -> Vec<Tile> {
        let pairs: Vec<[i64; 2]> = input.iter().map(|t| [t[0] as i64, t[1] as i64]).collect();
        self.list(op, a, v, &pairs).1.into_iter().map(|p| [p[0] as i32, p[1] as i32]).collect()
    }
    fn handles(&mut self, op: i32, a: u64, v: &[i64], input: &[u64]) -> (i64, Vec<u64>) {
        let pairs: Vec<[i64; 2]> = input.iter().map(|&u| [u as i64, 0]).collect();
        let (r, out) = self.list(op, a, v, &pairs);
        (r, out.into_iter().map(|p| p[0] as u64).collect())
    }
    fn capture(&self) -> bool {
        self.ctx().capture != 0
    }
}

fn xy(t: Tile) -> [i64; 2] {
    [t[0] as i64, t[1] as i64]
}

impl PlaceHost for Game {
    fn player_id(&mut self) -> u32 {
        self.ctx().player_id
    }
    fn player_handle(&mut self) -> u64 {
        self.ctx().player
    }
    fn epoch(&mut self) -> i32 {
        self.q(EE_CP_PLACE_EPOCH, 0, &[]) as i32
    }
    fn drop_off_distance(&mut self) -> i32 {
        // SAFETY: the computer player's record
        unsafe { (*self.ctx().fields).mDropOffDistance as i32 }
    }
    fn tower_spacing(&mut self) -> f32 {
        // SAFETY: as above
        unsafe { (*self.ctx().fields).mTowerSpacing }
    }
    fn town(&mut self) -> Tile {
        let p = self.ctx().town;
        // SAFETY: the placement's U2DPoint<long> (every caller of a town read has a placement)
        unsafe { [*p as i32, *p.add(1) as i32] }
    }
    fn set_town(&mut self, t: Tile) {
        let p = self.ctx().town;
        // SAFETY: as above
        unsafe {
            *p = t[0] as c_long;
            *p.add(1) = t[1] as c_long;
        }
    }
    fn passability(&mut self) -> i32 {
        self.q(EE_CP_PLACE_PASSABILITY, 0, &[]) as i32
    }
    fn range(&mut self, class: i32) -> f32 {
        self.qf(EE_CP_PLACE_RANGE, 0, 0, &[class as i64]).1.f[0]
    }
    fn tech_kind(&mut self, id: i32) -> Option<i32> {
        let (r, io) = self.qf(EE_CP_PLACE_TECH_KIND, 0, 0, &[id as i64]);
        (r != 0).then_some(io.v[0] as i32)
    }
    fn has_pathfinding(&mut self) -> bool {
        self.q(EE_CP_PLACE_HAS_PATHFINDING, 0, &[]) != 0
    }
    fn claim_range_on(&mut self) -> bool {
        self.q(EE_CP_PLACE_CLAIM_RANGE_ON, 0, &[]) != 0
    }
    fn claim_range(&mut self) -> i32 {
        self.q(EE_CP_PLACE_CLAIM_RANGE, 0, &[]) as i32
    }
    fn own_units(&mut self) -> Vec<u64> {
        self.handles(EE_CP_PLACE_OWN_UNITS, 0, &[], &[]).1
    }
    fn units_of_type(&mut self, unit_type: i32, player_id: u32, units: &mut Vec<u64>) {
        *units = self.handles(EE_CP_PLACE_UNITS_OF_TYPE, 0, &[unit_type as i64, player_id as i64], units).1;
    }
    fn nature_units(&mut self) -> Option<Vec<u64>> {
        let (r, units) = self.handles(EE_CP_PLACE_NATURE_UNITS, 0, &[], &[]);
        (r != 0).then_some(units)
    }
    fn towns(&mut self) -> Vec<Option<(u16, Tile)>> {
        let (_, list) = self.list(EE_CP_PLACE_TOWNS, 0, &[], &[]);
        list.into_iter()
            .map(|p| (p[0] >= 0).then(|| (p[0] as u16, [(p[1] >> 32) as i32, p[1] as u32 as i32])))
            .collect()
    }
    fn centers(&mut self) -> Vec<u64> {
        self.handles(EE_CP_PLACE_CENTERS, 0, &[], &[]).1
    }
    fn center_units(&mut self, center: u64) -> Vec<u32> {
        if center == 0 {
            return Vec::new();
        }
        self.handles(EE_CP_CENTER_UNITS, center, &[], &[]).1.into_iter().map(|u| u as u32).collect()
    }
    fn center_unit(&mut self, center: u64) -> u32 {
        if center == 0 {
            return u32::MAX;
        }
        self.q(EE_CP_CENTER_UNIT, center, &[]) as u32
    }
    fn center_active(&mut self, center: u64) -> bool {
        self.q(EE_CP_CENTER_ACTIVE, center, &[]) != 0
    }
    fn center_set_order(&mut self, center: u64, ids: &[u32]) {
        let pairs: Vec<[i64; 2]> = ids.iter().map(|&id| [id as i64, 0]).collect();
        self.list(EE_CP_CENTER_SET_ORDER, center, &[], &pairs);
    }
    fn center_clear_units(&mut self, center: u64) {
        self.q(EE_CP_CENTER_CLEAR, center, &[]);
    }
    fn center_add_unit(&mut self, center: u64, unit: u64) {
        let mut io = Self::io();
        self.call(EE_CP_CENTER_ADD_UNIT, center, unit, &mut io);
    }
    fn center_restore_main(&mut self, center: u64, id: u32) -> i32 {
        self.q(EE_CP_CENTER_RESTORE_MAIN, center, &[id as i64]) as i32
    }
    fn has_player0(&mut self) -> bool {
        self.q(EE_CP_PLACE_HAS_PLAYER0, 0, &[]) != 0
    }
    fn resolve(&mut self, id: u32) -> u64 {
        crate::units::ffi::ee_units_resolve(id) as u64
    }
    fn world_unit(&mut self, id: u32) -> u64 {
        self.q(EE_CP_PLACE_WORLD_UNIT, 0, &[id as i64]) as u64
    }
    fn unit(&mut self, unit: u64) -> EeCpUnitInfo {
        if unit == 0 {
            return EeCpUnitInfo::default();
        }
        self.qf(EE_CP_PLACE_UNIT_INFO, unit, 0, &[]).1.unit
    }
    fn distance_to_unit(&mut self, a: u64, b: u64) -> f32 {
        self.qf(EE_CP_CENTER_DISTANCE_TO_UNIT, a, b, &[]).1.f[0]
    }
    fn class_count(&mut self) -> u32 {
        self.q(EE_CP_PLACE_CLASS_COUNT, 0, &[]) as u32
    }
    fn class_exists(&mut self, class: i32) -> bool {
        self.q(EE_CP_PLACE_CLASS_EXISTS, 0, &[class as i64]) != 0
    }
    fn class_tile_size(&mut self, class: i32) -> i32 {
        self.q(EE_CP_PLACE_CLASS_TILE_SIZE, 0, &[class as i64]) as i32
    }
    fn class_terrain(&mut self, class: i32, terrain: i32) -> bool {
        self.q(EE_CP_PLACE_CLASS_TERRAIN, 0, &[class as i64, terrain as i64]) != 0
    }
    fn class_technology(&mut self, class: i32) -> i32 {
        self.q(EE_CP_PLACE_CLASS_TECHNOLOGY, 0, &[class as i64]) as i32
    }
    fn has_map(&mut self) -> bool {
        self.q(EE_CP_PLACE_HAS_MAP, 0, &[]) != 0
    }
    fn map_flags(&mut self) -> u32 {
        self.q(EE_CP_PLACE_MAP_FLAGS, 0, &[]) as u32
    }
    fn tile_terrain(&mut self, t: Tile) -> Option<i32> {
        let r = self.q(EE_CP_PLACE_TILE_TERRAIN, 0, &xy(t));
        (r >= 0).then_some(r as i32)
    }
    fn map_tile_terrain(&mut self, t: Tile) -> Option<i32> {
        let r = self.q(EE_CP_PLACE_MAP_TILE_TERRAIN, 0, &xy(t));
        (r >= 0).then_some(r as i32)
    }
    fn tile_free(&mut self, t: Tile) -> Option<bool> {
        let r = self.q(EE_CP_PLACE_TILE_FREE, 0, &xy(t));
        (r >= 0).then_some(r != 0)
    }
    fn is_edge(&mut self, t: Tile) -> bool {
        self.q(EE_CP_PLACE_IS_EDGE, 0, &xy(t)) != 0
    }
    fn largest_continent(&mut self, kind: i32) -> i32 {
        self.q(EE_CP_PLACE_LARGEST_CONTINENT, 0, &[kind as i64]) as i32
    }
    fn continent_from_id(&mut self, id: u16) -> u64 {
        self.q(EE_CP_PLACE_CONTINENT_FROM_ID, 0, &[id as i64]) as u64
    }
    fn continent_by_id(&mut self, id: u16) -> u64 {
        self.q(EE_CP_PLACE_CONTINENT_BY_ID, 0, &[id as i64]) as u64
    }
    fn continent_at(&mut self, t: Tile) -> u64 {
        self.q(EE_CP_PLACE_CONTINENT_AT, 0, &xy(t)) as u64
    }
    fn continent_id(&mut self, continent: u64) -> u16 {
        self.q(EE_CP_PLACE_CONTINENT_ID, continent, &[]) as u16
    }
    fn continent_flag(&mut self, a: u16, b: u16) -> bool {
        self.q(EE_CP_PLACE_CONTINENT_FLAG, 0, &[a as i64, b as i64]) != 0
    }
    fn shore_next_to(&mut self, continent: u64, id: u16, tiles: &mut Vec<Tile>) {
        *tiles = self.tiles(EE_CP_PLACE_SHORE_NEXT_TO, continent, &[id as i64], tiles);
    }
    fn shore_tiles(&mut self, continent: u64) -> Vec<Tile> {
        self.tiles(EE_CP_PLACE_SHORE_TILES, continent, &[], &[])
    }
    fn hull_center(&mut self, area: u64, choke: bool) -> Tile {
        let io = self.qf(EE_CP_PLACE_HULL_CENTER, area, 0, &[choke as i64]).1;
        [io.v[0] as i32, io.v[1] as i32]
    }
    fn are_connected(&mut self, a: u64, b: u64, flags: i32) -> bool {
        self.qf(EE_CP_PLACE_ARE_CONNECTED, a, b, &[flags as i64]).0 != 0
    }
    fn tile_continent(&mut self, t: Tile) -> u16 {
        self.q(EE_CP_PLACE_TILE_CONTINENT, 0, &xy(t)) as u16
    }
    fn unit_continent(&mut self, unit: u64) -> u16 {
        self.q(EE_CP_PLACE_UNIT_CONTINENT, unit, &[]) as u16
    }
    fn continent_id_at(&mut self, t: Tile) -> u16 {
        self.q(EE_CP_PLACE_CONTINENT_ID_AT, 0, &xy(t)) as u16
    }
    fn choke_at(&mut self, t: Tile) -> bool {
        self.q(EE_CP_PLACE_CHOKE_AT, 0, &xy(t)) != 0
    }
    fn hill_at(&mut self, t: Tile) -> bool {
        self.q(EE_CP_PLACE_HILL_AT, 0, &xy(t)) != 0
    }
    fn path_area_at(&mut self, t: Tile) -> u64 {
        self.q(EE_CP_PLACE_PATH_AREA_AT, 0, &xy(t)) as u64
    }
    fn unit_path_area(&mut self, unit: u64) -> u64 {
        self.q(EE_CP_PLACE_UNIT_PATH_AREA, unit, &[]) as u64
    }
    fn choke_points(&mut self, id: u16) -> Vec<u64> {
        self.handles(EE_CP_PLACE_CHOKE_POINTS, 0, &[id as i64], &[]).1
    }
    fn hull_bounds(&mut self, area: u64) -> Rect {
        let io = self.qf(EE_CP_PLACE_HULL_BOUNDS, area, 0, &[]).1;
        Rect { left: io.v[0] as i32, top: io.v[1] as i32, right: io.v[2] as i32, bottom: io.v[3] as i32 }
    }
    fn choke_point_tiles(&mut self, choke: u64, tiles: &mut Vec<Tile>) {
        *tiles = self.tiles(EE_CP_PLACE_CHOKE_TILES, choke, &[], tiles);
    }
    fn block_takes(&mut self, unit_type: i32, x: i32, y: i32, w: i32, h: i32) -> bool {
        self.q(EE_CP_PLACE_BLOCK_TAKES, 0, &[unit_type as i64, x as i64, y as i64, w as i64, h as i64]) != 0
    }
    fn facing_quarter(&mut self, t: Tile) -> i32 {
        self.q(EE_CP_PLACE_FACING_QUARTER, 0, &xy(t)) as i32
    }
    fn site_threat(&mut self, t: Tile) -> bool {
        self.q(EE_CP_PLACE_SITE_THREAT, 0, &xy(t)) != 0
    }
    fn tile_unit_of_type(&mut self, t: Tile, unit_type: i32) -> bool {
        self.q(EE_CP_PLACE_TILE_UNIT_OF_TYPE, 0, &[t[0] as i64, t[1] as i64, unit_type as i64]) != 0
    }
    fn units_in_rect(&mut self, rect: Rect, filter: RectFilter, units: &mut Vec<u64>) {
        let v = [rect.left as i64, rect.top as i64, rect.right as i64, rect.bottom as i64, filter as i64];
        *units = self.handles(EE_CP_PLACE_UNITS_IN_RECT, 0, &v, units).1;
    }
    fn random(&mut self, max: i32) -> i32 {
        self.q(EE_CP_PLACE_RANDOM, 0, &[max as i64]) as i32
    }
    fn space_dock_open(&mut self, t: Tile) -> bool {
        self.q(EE_CP_PLACE_SPACE_DOCK_OPEN, 0, &xy(t)) != 0
    }
    fn sort_order(&mut self, keys: &[i64]) -> Vec<usize> {
        let mut order = vec![0u32; keys.len()];
        let mut io = Self::io();
        io.input = keys.as_ptr();
        io.input_len = keys.len();
        io.order = order.as_mut_ptr();
        self.call(EE_CP_PLACE_SORT, 0, 0, &mut io);
        order.into_iter().map(|i| i as usize).collect()
    }
    fn has_area_effect(&mut self, kind: i32, x: i32, y: i32, player: u32) -> bool {
        self.q(EE_CP_CENTER_HAS_AREA_EFFECT, 0, &[kind as i64, x as i64, y as i64, player as i64]) != 0
    }
    fn morale_radius(&mut self, unit: u64) -> Option<f32> {
        let (r, io) = self.qf(EE_CP_CENTER_MORALE_RADIUS, unit, 0, &[]);
        (r != 0).then_some(io.f[0])
    }
    fn morale_recount(&mut self, unit: u64) -> Option<(i32, i32, f32)> {
        let (r, io) = self.qf(EE_CP_CENTER_MORALE_RECOUNT, unit, 0, &[]);
        (r != 0).then_some((io.v[0] as i32, io.v[1] as i32, io.f[0]))
    }
    fn effect_templates(&mut self, class: i32) -> [u32; 4] {
        let io = self.qf(EE_CP_CENTER_EFFECT_TEMPLATES, 0, 0, &[class as i64]).1;
        [io.v[0] as u32, io.v[1] as u32, io.v[2] as u32, io.v[3] as u32]
    }
    fn template_radius(&mut self, template: u32) -> f32 {
        self.qf(EE_CP_CENTER_TEMPLATE_RADIUS, 0, 0, &[template as i64]).1.f[0]
    }
    // the tile lists: the pure math, or the C++ wrappers (which call the same Rust) while the capture records
    fn square_ring_tiles(&mut self, t: Tile, radius: i32) -> Vec<Tile> {
        if self.capture() {
            return self.tiles(EE_CP_PLACE_SQUARE_RING_TILES, 0, &[t[0] as i64, t[1] as i64, radius as i64], &[]);
        }
        crate::units::uu_tiles::square_ring_tiles(t[0], t[1], radius)
    }
    fn square_block_ring(&mut self, t: Tile, size: i32, tiles: &mut Vec<Tile>) {
        if self.capture() {
            *tiles = self.tiles(EE_CP_PLACE_SQUARE_BLOCK_RING, 0, &[t[0] as i64, t[1] as i64, size as i64], tiles);
            return;
        }
        *tiles = super::place_host::square_block_ring_all(t[0], t[1], size);
    }
    fn square_ring(&mut self, t: Tile, w: i32, h: i32) -> Vec<Tile> {
        if self.capture() {
            return self.tiles(EE_CP_PLACE_SQUARE_RING, 0, &[t[0] as i64, t[1] as i64, w as i64, h as i64], &[]);
        }
        crate::units::uu_tiles::square_ring(t[0], t[1], w, h)
    }
    fn side_tiles(&mut self, t: Tile, size: i32) -> Vec<Tile> {
        if self.capture() {
            return self.tiles(EE_CP_PLACE_SIDE_TILES, 0, &[t[0] as i64, t[1] as i64, size as i64], &[]);
        }
        crate::units::uu_tiles::square_ring(t[0], t[1], size, size)
    }
    fn corner_tiles(&mut self, t: Tile, size: i32) -> Vec<Tile> {
        if self.capture() {
            return self.tiles(EE_CP_PLACE_CORNER_TILES, 0, &[t[0] as i64, t[1] as i64, size as i64], &[]);
        }
        super::place_host::corner_tiles_all(t[0], t[1], size)
    }
    fn tiles_in_radius(&mut self, t: Tile, radius: u32, inclusive: bool, tiles: &mut Vec<Tile>) {
        if self.capture() {
            let v = [t[0] as i64, t[1] as i64, radius as i64, inclusive as i64];
            *tiles = self.tiles(EE_CP_PLACE_TILES_IN_RADIUS, 0, &v, tiles);
            return;
        }
        crate::units::uu_tiles::tiles_in_radius(t[0], t[1], radius, inclusive, tiles)
    }
}

/// The host over a call's context, and the placement's own data (None: no placement).
///
/// # Safety
/// `ctx` is the C++ caller's context, valid for the call.
unsafe fn game(ctx: *const EeCpPlaceCtx) -> Game {
    Game { ctx }
}

/// # Safety
/// `ctx`'s placement is live (or null) and not otherwise borrowed during the call.
unsafe fn state<'a>(ctx: *const EeCpPlaceCtx) -> Option<&'a mut EeCpPlacement> {
    // SAFETY: the caller's contract
    unsafe { (*ctx).placement.as_mut() }
}

fn tile_in(p: *const i32) -> Tile {
    // SAFETY: the callers pass two int32_t
    unsafe { [*p, *p.add(1)] }
}

fn tile_out(p: *mut i32, t: Tile) {
    // SAFETY: as above
    unsafe {
        *p = t[0];
        *p.add(1) = t[1];
    }
}

// ---- the placement's data

/// A placement's own data, for `EECPPlacement::mRust` (`ee_cp_placement_free` frees it).
#[no_mangle]
pub extern "C" fn ee_cp_placement_new() -> *mut EeCpPlacement {
    Box::into_raw(Box::default())
}

/// # Safety
/// `p` is null or came from [`ee_cp_placement_new`] and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_placement_free(p: *mut EeCpPlacement) {
    if !p.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// TEST ONLY (PLCE): the data's digest into `out[8]`: the water sites' count and FNV-1a 64 (low, high words), the
/// space sites' the same, whether the tiles were collected, the classes seen's count.
///
/// # Safety
/// `p` is a live placement's data; `out` has 8 words.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_placement_digest(p: *const EeCpPlacement, out: *mut u32) {
    // SAFETY: the caller's contract
    let (p, out) = unsafe { (&*p, std::slice::from_raw_parts_mut(out, 8)) };
    for (k, list) in [&p.water_sites, &p.space_sites].into_iter().enumerate() {
        let mut h: u64 = 0xcbf29ce484222325;
        for t in list {
            for w in [t[0] as u32, t[1] as u32] {
                for b in w.to_le_bytes() {
                    h = (h ^ b as u64).wrapping_mul(0x100000001b3);
                }
            }
        }
        out[k * 3] = list.len() as u32;
        out[k * 3 + 1] = h as u32;
        out[k * 3 + 2] = (h >> 32) as u32;
    }
    out[6] = p.site_tiles_collected as u32;
    out[7] = p.types_seen.len() as u32;
}

// ---- EECPPlacement's members

/// `HasUnitNear`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_has_unit_near(
    ctx: *const EeCpPlaceCtx,
    unit_type: i32,
    x: i32,
    y: i32,
    squared: f32,
) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::has_unit_near(&mut h, unit_type, [x, y], squared)
}

/// `HasAntiAirNear`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_has_anti_air_near(ctx: *const EeCpPlaceCtx, x: i32, y: i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::has_anti_air_near(&mut h, [x, y])
}

/// `HasTownCenterNear`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_has_town_center_near(ctx: *const EeCpPlaceCtx, x: i32, y: i32, distance: f32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::has_town_center_near(&mut h, [x, y], distance)
}

/// `HasGateNear`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_has_gate_near(ctx: *const EeCpPlaceCtx, x: i32, y: i32, radius: f32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::has_gate_near(&mut h, [x, y], radius)
}

/// `HasTowerNear`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_has_tower_near(ctx: *const EeCpPlaceCtx, x: i32, y: i32, squared: f32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::has_tower_near(&mut h, [x, y], squared)
}

/// `HasWallNear`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_has_wall_near(ctx: *const EeCpPlaceCtx, x: i32, y: i32, squared: f32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::has_wall_near(&mut h, [x, y], squared)
}

/// `HasEnemyNear`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_has_enemy_near(ctx: *const EeCpPlaceCtx, x: i32, y: i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::has_enemy_near(&mut h, [x, y])
}

/// `CollectSiteTiles`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_collect_site_tiles(ctx: *const EeCpPlaceCtx, kind: i32) {
    // SAFETY: the caller's contract
    let (mut h, s) = unsafe { (game(ctx), state(ctx)) };
    if let Some(s) = s {
        pl::collect_site_tiles(&mut h, s, kind)
    }
}

/// `FindAirportSite`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_airport_site(ctx: *const EeCpPlaceCtx, out: *mut i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = pl::find_airport_site(&mut h, &mut t);
    tile_out(out, t);
    r
}

/// `FindAntiAirSiteNear`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_anti_air_site_near(ctx: *const EeCpPlaceCtx, unit: u64, out: *mut i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = pl::find_anti_air_site_near(&mut h, unit, &mut t);
    tile_out(out, t);
    r
}

/// `FindAntiAirSiteAround`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_anti_air_site_around(
    ctx: *const EeCpPlaceCtx,
    x: i32,
    y: i32,
    size: i32,
    out: *mut i32,
) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = pl::find_anti_air_site_around(&mut h, [x, y], size, &mut t);
    tile_out(out, t);
    r
}

/// `FindAntiAirSite`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_anti_air_site(ctx: *const EeCpPlaceCtx, out: *mut i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = pl::find_anti_air_site(&mut h, &mut t);
    tile_out(out, t);
    r
}

/// `FindCenterSite`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_center_site(ctx: *const EeCpPlaceCtx, class: i32, out: *mut i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = pl::find_center_site(&mut h, class, &mut t);
    tile_out(out, t);
    r
}

/// `FindGranarySite`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_granary_site(ctx: *const EeCpPlaceCtx, out: *mut i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = pl::find_granary_site(&mut h, &mut t);
    tile_out(out, t);
    r
}

/// `FindProductionSite`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_production_site(ctx: *const EeCpPlaceCtx, class: i32, out: *mut i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = pl::find_production_site(&mut h, class, &mut t);
    tile_out(out, t);
    r
}

/// `FindShoreSite`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_shore_site(ctx: *const EeCpPlaceCtx, class: i32, size: i32, out: *mut i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = s.is_some_and(|s| pl::find_shore_site(&mut h, s, class, size, &mut t));
    tile_out(out, t);
    r
}

/// `FindSpaceSite`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_space_site(ctx: *const EeCpPlaceCtx, class: i32, size: i32, out: *mut i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = s.is_some_and(|s| pl::find_space_site(&mut h, s, class, size, &mut t));
    tile_out(out, t);
    r
}

/// `FindTowerSite`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_tower_site(ctx: *const EeCpPlaceCtx, class: i32, size: i32, out: *mut i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = pl::find_tower_site(&mut h, class, size, &mut t);
    tile_out(out, t);
    r
}

/// `FindWonderSite`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_wonder_site(
    ctx: *const EeCpPlaceCtx,
    class: i32,
    size: u32,
    out: *mut i32,
) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = pl::find_wonder_site(&mut h, class, size, &mut t);
    tile_out(out, t);
    r
}

/// `FindSite` (`out`: the tile in and out)
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_find_site(ctx: *const EeCpPlaceCtx, class: i32, out: *mut i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = s.is_some_and(|s| pl::find_site(&mut h, s, class, &mut t));
    tile_out(out, t);
    r
}

/// `IsNearAirportEnd`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_is_near_airport_end(
    ctx: *const EeCpPlaceCtx,
    x: i32,
    y: i32,
    size: i32,
    distance: f32,
) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::is_near_airport_end(&mut h, [x, y], size, distance)
}

/// `HasBuilderFor`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_has_builder_for(ctx: *const EeCpPlaceCtx, x: i32, y: i32, class: i32) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::has_builder_for(&mut h, [x, y], class)
}

/// `SetTownCenter`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_set_town_center(ctx: *const EeCpPlaceCtx, unit: u64) {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::set_town_center(&mut h, unit)
}

/// `UnitAdded`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_unit_added(ctx: *const EeCpPlaceCtx, unit: u64) {
    // SAFETY: the caller's contract
    let (mut h, s) = unsafe { (game(ctx), state(ctx)) };
    if let Some(s) = s {
        pl::unit_added(&mut h, s, unit)
    }
}

/// `Reset`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_placement_reset(ctx: *const EeCpPlaceCtx) {
    // SAFETY: the caller's contract
    let (mut h, s) = unsafe { (game(ctx), state(ctx)) };
    if let Some(s) = s {
        pl::reset(&mut h, s)
    }
}

/// `Restore`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_placement_restore(ctx: *const EeCpPlaceCtx) -> i32 {
    // SAFETY: the caller's contract
    let (mut h, s) = unsafe { (game(ctx), state(ctx)) };
    match s {
        Some(s) => pl::restore(&mut h, s),
        None => 0,
    }
}

/// `CanBuildAt`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_can_build_at(
    ctx: *const EeCpPlaceCtx,
    continent: u16,
    class: i32,
    x: i32,
    y: i32,
    size: i32,
    margin: bool,
    strict: bool,
    from_x: i32,
    from_y: i32,
) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::can_build_at(&mut h, continent, class, [x, y], size, margin, strict, [from_x, from_y])
}

/// `EECPCountOwn`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_count_own(ctx: *const EeCpPlaceCtx, unit_type: i32) -> i32 {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    pl::count_own(&mut h, unit_type)
}

/// `EECPNextPlaceSearch` (`units`: `count` ids)
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_next_place_search(
    ctx: *const EeCpPlaceCtx,
    class: i32,
    mode: i32,
    x: i32,
    y: i32,
    first: i32,
    last: i32,
    units: *const i64,
    count: usize,
    site: i32,
) -> EeCpPlaceResult {
    // SAFETY: the caller's contract
    let (mut h, s) = unsafe { (game(ctx), state(ctx)) };
    let ids: &[i64] = if count == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract: `count` ids
        unsafe { std::slice::from_raw_parts(units, count) }
    };
    pl::next_place_search(&mut h, s.as_deref(), class, mode, [x, y], first, last, ids, site)
}

/// `EECPAntiAirClass` of the player's epoch.
#[no_mangle]
pub extern "C" fn ee_cp_anti_air_class(epoch: i32) -> i32 {
    pl::anti_air_class(epoch)
}
/// `EECPFishingBoatClass`
#[no_mangle]
pub extern "C" fn ee_cp_fishing_boat_class(epoch: i32) -> i32 {
    pl::fishing_boat_class(epoch)
}
/// `EECPGateClass`
#[no_mangle]
pub extern "C" fn ee_cp_gate_class(epoch: i32) -> i32 {
    pl::gate_class(epoch)
}
/// `EECPTowerClass`
#[no_mangle]
pub extern "C" fn ee_cp_tower_class(epoch: i32) -> i32 {
    pl::tower_class(epoch)
}
/// `EECPWallClass`
#[no_mangle]
pub extern "C" fn ee_cp_wall_class(epoch: i32) -> i32 {
    pl::wall_class(epoch)
}
/// `EECPIsSiteClass`
#[no_mangle]
pub extern "C" fn ee_cp_is_site_class(class: i32) -> bool {
    pl::is_site_class(class)
}

// ---- EECPResourceCenter's members (`center`: the `EECPResourceCenter*`)

/// `Contains`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_contains(ctx: *const EeCpPlaceCtx, center: u64, unit: u64) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    rc::contains(&mut h, center, unit)
}

/// `AdoptDropSite`'s test (the C++ takes the reference)
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_adopts(ctx: *const EeCpPlaceCtx, center: u64, unit: u64) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    rc::adopt_drop_site(&mut h, center, unit)
}

/// `FindBuildSite`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_find_build_site(
    ctx: *const EeCpPlaceCtx,
    center: u64,
    class: i32,
    out: *mut i32,
) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    let r = rc::find_build_site(&mut h, center, class, &mut t);
    tile_out(out, t);
    r
}

/// `GetMinEffectRadius`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_min_effect_radius(ctx: *const EeCpPlaceCtx, class: i32) -> i32 {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    rc::min_effect_radius(&mut h, class)
}

/// `GetMoraleFill`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_morale_fill(ctx: *const EeCpPlaceCtx, center: u64) -> f32 {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    rc::morale_fill(&mut h, center)
}

/// `GetCenter` (`out`: the tile in and out)
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_get_center(ctx: *const EeCpPlaceCtx, center: u64, out: *mut i32) {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    let mut t = tile_in(out);
    rc::get_center(&mut h, center, &mut t);
    tile_out(out, t);
}

/// `FindResourceUnit`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_find_resource_unit(ctx: *const EeCpPlaceCtx, center: u64, resource: u32) -> u64 {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    rc::find_resource_unit(&mut h, center, resource)
}

/// `IsInAreaEffect`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_is_in_area_effect(
    ctx: *const EeCpPlaceCtx,
    x: i32,
    y: i32,
    class: i32,
    kind: i32,
) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    rc::is_in_area_effect(&mut h, [x, y], class, kind)
}

/// `IsWithinRadius`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_is_within_radius(ctx: *const EeCpPlaceCtx, center: u64, unit: u64) -> bool {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    rc::is_within_radius(&mut h, center, unit)
}

/// `Restore`
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_restore(ctx: *const EeCpPlaceCtx, center: u64) -> i32 {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    rc::restore(&mut h, center)
}

/// `Save` (`file`, `host`: the saved game's primitives)
///
/// # Safety
/// `ctx` is the C++ caller's context; the pointer arguments are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_save(
    ctx: *const EeCpPlaceCtx,
    center: u64,
    file: *mut c_void,
    host: EeUnitsPlayerSaveFn,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut h, _s) = unsafe { (game(ctx), state(ctx)) };
    // SAFETY: the caller's contract
    let mut f = unsafe { SaveFile::new(file, host) };
    rc::save(&mut h, &mut f, center)
}

/// `EECPResourceCenter::Load`: `host` serves the file's primitives and the centre's operations
/// (`EE_CP_CENTER_FILE_*`) on `file`.
///
/// # Safety
/// `host` must accept `file` with those operations for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_center_load(file: *mut c_void, host: EeUnitsPlayerSaveFn, minor: u32) -> i32 {
    // SAFETY: the caller's contract
    let mut f = unsafe { SaveFile::new(file, host) };
    rc::load(&mut f, minor)
}
