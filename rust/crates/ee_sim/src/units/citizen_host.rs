//! The citizens' searches' host in the game (`ee_units_cz_host`, EEUnitClassPeople.cpp) and the entry points the
//! `EEUnitClass` members call (citizen.rs). Absent from a test binary.

#![cfg(not(test))]

use std::ffi::c_void;

use crate::units::citizen::{self, CzHost, CzTile, EeUnitsCzUnit};

/// One host operation's values.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUnitsCzIo {
    /// up to five integer arguments
    pub v: [i64; 5],
    /// a point in or out
    pub f: [f32; 3],
    pub unit: EeUnitsCzUnit,
    /// the tiles an operation starts from (`tiles_len` of them)
    pub tiles: *const CzTile,
    pub tiles_len: usize,
    /// the list the operation pushes onto (`ee_units_cz_push_tile`, `ee_units_cz_push_unit`)
    pub list: *mut c_void,
}

impl Default for EeUnitsCzIo {
    fn default() -> Self {
        EeUnitsCzIo {
            v: [0; 5],
            f: [0.0; 3],
            unit: EeUnitsCzUnit::default(),
            tiles: std::ptr::null(),
            tiles_len: 0,
            list: std::ptr::null_mut(),
        }
    }
}

/// The lists a host operation fills.
#[derive(Default)]
pub struct EeUnitsCzList {
    tiles: Vec<CzTile>,
    units: Vec<u64>,
}

// ---- the host operations (`ee_units_cz_host`'s `op`; `a`, `b` objects, `io.v` values)
/// unit `a` into `io.unit`
pub const EE_UNITS_CZ_UNIT: i32 = 1;
/// `gUnitClassTable[v0]->GetTileSize()`
pub const EE_UNITS_CZ_CLASS_TILE_SIZE: i32 = 2;
/// `gMapWidth` into `v0`, `gMapHeight` into `v1`
pub const EE_UNITS_CZ_MAP_SIZE: i32 = 3;
/// `EEGetSquareRingTiles(v0, v1, v2, tiles)` (pushed)
pub const EE_UNITS_CZ_SQUARE_RING_TILES: i32 = 4;
/// `EEGetSideTiles(v0, v1, v2, tiles)` from `io.tiles` (the result pushed)
pub const EE_UNITS_CZ_SIDE_TILES: i32 = 5;
/// `EEGetRingCorners(v0, v1, v2, v3, v4, tiles)` from `io.tiles`
pub const EE_UNITS_CZ_RING_CORNERS: i32 = 6;
/// `EEGetSquareRing(v0, v1, v2, v3, tiles)` from `io.tiles`
pub const EE_UNITS_CZ_SQUARE_RING: i32 = 7;
/// `EEAIRandomShuffle` over `io.tiles`
pub const EE_UNITS_CZ_SHUFFLE: i32 = 8;
/// `gContinentManager.GetContinent(U2DPoint(v0, v1), kAIPathLandMass)`
pub const EE_UNITS_CZ_TILE_CONTINENT: i32 = 9;
/// `gContinentManager.GetContinent(unit a, kAIPathLandMass)`
pub const EE_UNITS_CZ_UNIT_CONTINENT: i32 = 10;
/// `EEBlockTakes(a, v0, v1, v2, v3, v3, 0)`
pub const EE_UNITS_CZ_BLOCK_TAKES: i32 = 11;
/// `gWorld.mMap->GetTile(v0, v1)`
pub const EE_UNITS_CZ_TILE: i32 = 12;
/// tile `a`'s units (pushed)
pub const EE_UNITS_CZ_TILE_UNITS: i32 = 13;
/// tile `a`'s `mTerrainType`
pub const EE_UNITS_CZ_TERRAIN: i32 = 14;
/// tile `a`'s `GetHeight()` into `f[0]`
pub const EE_UNITS_CZ_HEIGHT: i32 = 15;
/// unit `a`'s `CanGather(b, v0, v1, false, false)`
pub const EE_UNITS_CZ_CAN_GATHER: i32 = 16;
/// `gContinentManager.CanGetToUnit(a, b)`
pub const EE_UNITS_CZ_CAN_GET_TO_UNIT: i32 = 17;
/// unit `a`'s `mComplexUnit->DistanceSquared2D(GE2DPoint(f[0], f[1]))` into `f[0]`
pub const EE_UNITS_CZ_DISTANCE_2D: i32 = 18;
/// unit `a`'s `GetTileDistanceSquared(b)` into `f[0]`
pub const EE_UNITS_CZ_TILE_DISTANCE: i32 = 19;
/// `gContinentManager.GetPathAreaIDAt(EETileLocation(v0, v1))`
pub const EE_UNITS_CZ_PATH_AREA: i32 = 20;
/// gather goal `a`'s `GetGatherStorage()`
pub const EE_UNITS_CZ_GATHER_STORAGE: i32 = 21;
/// gather goal `a`'s `GetGatherResource()`
pub const EE_UNITS_CZ_GATHER_RESOURCE: i32 = 22;
/// gather goal `a`'s `IsGathering()`
pub const EE_UNITS_CZ_IS_GATHERING: i32 = 23;
/// build goal `a`'s `GetBuildTargetClass()->mIsWall`
pub const EE_UNITS_CZ_BUILD_IS_WALL: i32 = 24;
/// build goal `a`'s `GetBuildTargetType()`
pub const EE_UNITS_CZ_BUILD_TYPE: i32 = 25;
/// `EEGetFarmDropSites(a, sites)` (pushed)
pub const EE_UNITS_CZ_FARM_DROP_SITES: i32 = 26;
/// complex unit `a`'s `CanTake(b, v0)`
pub const EE_UNITS_CZ_CAN_TAKE: i32 = 27;
/// unit `a`'s `SetResourceAmount(0)`, `SetResourceType(v0)`
pub const EE_UNITS_CZ_EMPTY_LOAD: i32 = 28;
/// unit `a`'s `DistanceSquaredToUnit(b)` into `f[0]`
pub const EE_UNITS_CZ_DISTANCE_TO_UNIT: i32 = 29;
/// unit `a`'s `DistanceSquaredTo(b)` into `f[0]`
pub const EE_UNITS_CZ_DISTANCE_TO: i32 = 30;
/// player `a`'s units from `mFirstUnit` (pushed)
pub const EE_UNITS_CZ_PLAYER_UNITS: i32 = 31;

extern "C" {
    fn ee_units_cz_host(op: i32, a: u64, b: u64, io: *mut EeUnitsCzIo) -> i64;
}

/// Pushes a tile onto a host operation's list.
///
/// # Safety
/// `list` is the `EeUnitsCzIo::list` of a running operation.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cz_push_tile(list: *mut EeUnitsCzList, x: i32, y: i32) {
    // SAFETY: the caller's contract
    unsafe { (*list).tiles.push([x, y]) }
}

/// Pushes a unit onto a host operation's list.
///
/// # Safety
/// As [`ee_units_cz_push_tile`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_cz_push_unit(list: *mut EeUnitsCzList, unit: u64) {
    // SAFETY: the caller's contract
    unsafe { (*list).units.push(unit) }
}

struct Game;

impl Game {
    fn run(&mut self, op: i32, a: u64, b: u64, v: [i64; 5], tiles: &[CzTile]) -> (i64, EeUnitsCzIo, EeUnitsCzList) {
        let mut list = EeUnitsCzList::default();
        let mut io = EeUnitsCzIo {
            v,
            tiles: tiles.as_ptr(),
            tiles_len: tiles.len(),
            list: std::ptr::addr_of_mut!(list).cast(),
            ..Default::default()
        };
        // SAFETY: the C++ switch reads live objects the bodies hand it and the tiles for the call
        let r = unsafe { ee_units_cz_host(op, a, b, &mut io) };
        (r, io, list)
    }
    fn q(&mut self, op: i32, a: u64, b: u64, v: [i64; 5]) -> i64 {
        self.run(op, a, b, v, &[]).0
    }
    fn f(&mut self, op: i32, a: u64, b: u64, f: [f32; 3]) -> f32 {
        let mut io = EeUnitsCzIo { f, ..Default::default() };
        // SAFETY: as `run`
        unsafe { ee_units_cz_host(op, a, b, &mut io) };
        io.f[0]
    }
    fn tiles(&mut self, op: i32, v: [i64; 5], tiles: &mut Vec<CzTile>) {
        let list = self.run(op, 0, 0, v, tiles).2;
        *tiles = list.tiles;
    }
}

fn v2(a: i32, b: i32) -> [i64; 5] {
    [a as i64, b as i64, 0, 0, 0]
}

impl CzHost for Game {
    fn unit(&mut self, u: u64) -> EeUnitsCzUnit {
        // SAFETY: the host's unit is live
        let t = unsafe { crate::world::WorldRead::begin() };
        unsafe { crate::world::UnitView::of(&t, u) }.map_or_else(EeUnitsCzUnit::default, |v| cz_unit_fields(&v))
    }
    fn class_tile_size(&mut self, unit_type: i32) -> i32 {
        // SAFETY: as above (`gUnitClassTable[type]`, a listed class)
        let t = unsafe { crate::world::WorldRead::begin() };
        crate::world::view::ClassView::by_index(&t, unit_type as usize).map_or(0, |c| c.tile_size() as i32)
    }
    fn map_size(&mut self) -> (u32, u32) {
        let io = self.run(EE_UNITS_CZ_MAP_SIZE, 0, 0, [0; 5], &[]).1;
        (io.v[0] as u32, io.v[1] as u32)
    }
    fn square_ring_tiles(&mut self, x: i32, y: i32, radius: i32) -> Vec<CzTile> {
        self.run(EE_UNITS_CZ_SQUARE_RING_TILES, 0, 0, [x as i64, y as i64, radius as i64, 0, 0], &[]).2.tiles
    }
    fn side_tiles(&mut self, x: i32, y: i32, size: i32, tiles: &mut Vec<CzTile>) {
        self.tiles(EE_UNITS_CZ_SIDE_TILES, [x as i64, y as i64, size as i64, 0, 0], tiles)
    }
    fn ring_corners(&mut self, corners: i32, x: i32, y: i32, width: i32, height: i32, tiles: &mut Vec<CzTile>) {
        let v = [corners as i64, x as i64, y as i64, width as i64, height as i64];
        self.tiles(EE_UNITS_CZ_RING_CORNERS, v, tiles)
    }
    fn square_ring(&mut self, x: i32, y: i32, width: i32, height: i32, tiles: &mut Vec<CzTile>) {
        self.tiles(EE_UNITS_CZ_SQUARE_RING, [x as i64, y as i64, width as i64, height as i64, 0], tiles)
    }
    fn shuffle(&mut self, tiles: &mut Vec<CzTile>) {
        self.tiles(EE_UNITS_CZ_SHUFFLE, [0; 5], tiles)
    }
    fn tile_continent(&mut self, x: i32, y: i32) -> u16 {
        self.q(EE_UNITS_CZ_TILE_CONTINENT, 0, 0, v2(x, y)) as u16
    }
    fn unit_continent(&mut self, u: u64) -> u16 {
        self.q(EE_UNITS_CZ_UNIT_CONTINENT, u, 0, [0; 5]) as u16
    }
    fn block_takes(&mut self, u: u64, unit_type: i32, x: i32, y: i32, size: i32) -> bool {
        self.q(EE_UNITS_CZ_BLOCK_TAKES, u, 0, [unit_type as i64, x as i64, y as i64, size as i64, 0]) != 0
    }
    fn tile(&mut self, x: i32, y: i32) -> u64 {
        // SAFETY: as above
        let t = unsafe { crate::world::WorldRead::begin() };
        crate::world::WorldView::global(&t)
            .and_then(|w| w.map())
            .and_then(|m| m.tile(i64::from(x), i64::from(y)))
            .map_or(0, |v| v.handle())
    }
    fn tile_units(&mut self, tile: u64) -> Vec<u64> {
        // SAFETY: as above; the tile handle is a live tile
        let t = unsafe { crate::world::WorldRead::begin() };
        unsafe { crate::world::TileView::of(&t, tile) }
            .map_or_else(Vec::new, |v| v.units().map(|u| u.map_or(0, |u| u.handle())).collect())
    }
    fn tile_terrain(&mut self, tile: u64) -> i32 {
        // SAFETY: as above
        let t = unsafe { crate::world::WorldRead::begin() };
        unsafe { crate::world::TileView::of(&t, tile) }.map_or(0, |v| v.terrain_type() as i32)
    }
    fn tile_height(&mut self, tile: u64) -> f32 {
        // SAFETY: as above
        let t = unsafe { crate::world::WorldRead::begin() };
        unsafe { crate::world::TileView::of(&t, tile) }.map_or(0.0, |v| v.height())
    }
    fn can_gather(&mut self, u: u64, target: u64, x: i32, y: i32) -> bool {
        self.q(EE_UNITS_CZ_CAN_GATHER, u, target, v2(x, y)) != 0
    }
    fn can_get_to_unit(&mut self, a: u64, b: u64) -> bool {
        self.q(EE_UNITS_CZ_CAN_GET_TO_UNIT, a, b, [0; 5]) != 0
    }
    fn distance_2d(&mut self, u: u64, x: f32, y: f32) -> f32 {
        self.f(EE_UNITS_CZ_DISTANCE_2D, u, 0, [x, y, 0.0])
    }
    fn tile_distance(&mut self, u: u64, center: u64) -> f32 {
        self.f(EE_UNITS_CZ_TILE_DISTANCE, u, center, [0.0; 3])
    }
    fn path_area(&mut self, x: i32, y: i32) -> u16 {
        self.q(EE_UNITS_CZ_PATH_AREA, 0, 0, v2(x, y)) as u16
    }
    fn gather_storage(&mut self, goal: u64) -> u64 {
        self.q(EE_UNITS_CZ_GATHER_STORAGE, goal, 0, [0; 5]) as u64
    }
    fn gather_resource(&mut self, goal: u64) -> u64 {
        self.q(EE_UNITS_CZ_GATHER_RESOURCE, goal, 0, [0; 5]) as u64
    }
    fn is_gathering(&mut self, goal: u64) -> bool {
        self.q(EE_UNITS_CZ_IS_GATHERING, goal, 0, [0; 5]) != 0
    }
    fn build_target_is_wall(&mut self, goal: u64) -> bool {
        self.q(EE_UNITS_CZ_BUILD_IS_WALL, goal, 0, [0; 5]) != 0
    }
    fn build_target_type(&mut self, goal: u64) -> u32 {
        self.q(EE_UNITS_CZ_BUILD_TYPE, goal, 0, [0; 5]) as u32
    }
    fn farm_drop_sites(&mut self, farm: u64) -> Vec<u64> {
        self.run(EE_UNITS_CZ_FARM_DROP_SITES, farm, 0, [0; 5], &[]).2.units
    }
    fn can_take(&mut self, storage: u64, u: u64, resource_type: u32) -> bool {
        self.q(EE_UNITS_CZ_CAN_TAKE, storage, u, [resource_type as i64, 0, 0, 0, 0]) != 0
    }
    fn empty_load(&mut self, u: u64, resource_type: u32) {
        self.q(EE_UNITS_CZ_EMPTY_LOAD, u, 0, [resource_type as i64, 0, 0, 0, 0]);
    }
    fn distance_to_unit(&mut self, a: u64, b: u64) -> f32 {
        self.f(EE_UNITS_CZ_DISTANCE_TO_UNIT, a, b, [0.0; 3])
    }
    fn distance_to(&mut self, a: u64, b: u64) -> f32 {
        self.f(EE_UNITS_CZ_DISTANCE_TO, a, b, [0.0; 3])
    }
    fn player_units(&mut self, player: u64) -> Vec<u64> {
        // SAFETY: as above; the player handle is a live player
        let t = unsafe { crate::world::WorldRead::begin() };
        unsafe { crate::world::PlayerView::of(&t, player) }
            .map_or_else(Vec::new, |p| p.units().map(|u| u.handle()).collect())
    }
}

// ---- the entry points (the `EEUnitClass` members; units and goals as their addresses)

/// `EEUnitClass::FindWoodSite(unit, unit type, out)`: 1 and the tile in `out` when found.
///
/// # Safety
/// `out` is two floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cz_find_wood_site(unit: u64, unit_type: i32, out: *mut [f32; 2]) -> u8 {
    match citizen::find_wood_site(&mut Game, unit, unit_type) {
        Some(p) => {
            // SAFETY: the caller's contract
            unsafe { *out = p };
            1
        }
        None => 0,
    }
}

/// `EEUnitClass::FindResourceOfKind` of a class of `category` (its `mCategory`); `taken` the `count` units of
/// `ioUnits`.
///
/// # Safety
/// `taken` is `count` units (or null for none).
#[no_mangle]
#[allow(clippy::too_many_arguments)] // the member's arguments
pub unsafe extern "C" fn ee_units_cz_find_resource_of_kind(
    category: u32,
    unit: u64,
    center: u64,
    wanted: u64,
    range: i32,
    kind: u32,
    taken: *const u64,
    count: usize,
    farms: u8,
) -> u64 {
    let taken: &[u64] = if taken.is_null() || count == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(taken, count) }
    };
    citizen::find_resource_of_kind(&mut Game, category, unit, center, wanted, range, kind, taken, farms != 0)
}

/// `EEUnitClass::FindStorage(unit, any storage)`.
#[no_mangle]
pub extern "C" fn ee_units_cz_find_storage(unit: u64, any_storage: u8) -> u64 {
    citizen::find_storage(&mut Game, unit, any_storage != 0)
}

/// `EEUnitClass::FindGatherPoint(unit, x, y, size x, size y, out)`.
///
/// # Safety
/// `out` is three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cz_find_gather_point(
    unit: u64,
    x: u32,
    y: u32,
    size_x: i32,
    size_y: i32,
    out: *mut [f32; 3],
) {
    let p = citizen::find_gather_point(&mut Game, unit, x, y, size_x, size_y);
    // SAFETY: the caller's contract
    unsafe { *out = p };
}

/// `EEUnitClass::HasGatherPoint(unit, resource)`.
#[no_mangle]
pub extern "C" fn ee_units_cz_has_gather_point(unit: u64, resource: u64) -> u8 {
    citizen::has_gather_point(&mut Game, unit, resource) as u8
}

/// The citizen bodies' reads of a unit (`EE_UNITS_CZ_UNIT`, once a C++ fill) over the views: the unit's, its
/// complex unit's (`mComplexUnit`) and its class's.
// (`c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there)
#[allow(clippy::unnecessary_cast)]
fn cz_unit_fields(u: &crate::world::UnitView) -> EeUnitsCzUnit {
    let class = u.class();
    let x = u.complex_unit();
    let (tx, ty) = u.tile_xy();
    let mut o = EeUnitsCzUnit {
        obj: u.handle(),
        class_obj: class.map_or(0, |c| c.handle()),
        complex: u.complex_unit_handle(),
        player: u.player().map_or(0, |p| p.handle()),
        goal_state: x.map_or(0, |x| x.goal_handle()),
        goal_kind: x.and_then(|x| x.goal()).map_or(-1, |g| i32::from(g.0)),
        tile_x: tx as i32,
        tile_y: ty as i32,
        hit_points: u.hit_points() as i32,
        resource_type: u.resource_type() as u32,
        on_map: u8::from(u.has_tile_node()),
        disabled: u8::from(x.is_some_and(|x| crate::world_field!(x.complex(), mIsDisabled))),
        built: u8::from(x.is_some_and(|x| x.is_built())),
        foundation: u8::from(x.is_some_and(|x| crate::world_field!(x.complex(), mIsFoundation))),
        ..EeUnitsCzUnit::default()
    };
    if let Some(c) = class {
        o.category = c.category() as u32;
        o.unit_type = c.unit_type() as u32;
        o.family = c.family() as u32;
        o.tile_size = c.tile_size() as i32;
        o.drop_site = u8::from(crate::units::rust_goal::class_drop_site(&c));
    }
    o
}
