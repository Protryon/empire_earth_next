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
/// `gContinentManager.GetPathAreaIDAt(EETileLocation(v0, v1))`
pub const EE_UNITS_CZ_PATH_AREA: i32 = 20;
/// complex unit `a`'s `CanTake(b, v0)`
pub const EE_UNITS_CZ_CAN_TAKE: i32 = 27;
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

use crate::units::{unit_geom, uu_direct};

/// A unit's tile (`mTileX`, `mTileY`, 32-bit longs)
fn tile_of(u: u64) -> (i32, i32) {
    let (x, y) = unit_geom::with_unit(u, |v| v.tile_xy()).expect("a live unit");
    (x as i32, y as i32)
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
    // the tile rings: the unit utilities' noted entry points (the C++ forwarders' replaced the list)
    fn square_ring_tiles(&mut self, x: i32, y: i32, radius: i32) -> Vec<CzTile> {
        uu_direct::square_ring_tiles(x, y, radius)
    }
    fn side_tiles(&mut self, x: i32, y: i32, size: i32, tiles: &mut Vec<CzTile>) {
        *tiles = uu_direct::square_ring(x, y, size, size, true);
    }
    fn ring_corners(&mut self, corners: i32, x: i32, y: i32, width: i32, height: i32, tiles: &mut Vec<CzTile>) {
        *tiles = uu_direct::ring_corners(corners, x, y, width, height, false);
    }
    fn square_ring(&mut self, x: i32, y: i32, width: i32, height: i32, tiles: &mut Vec<CzTile>) {
        *tiles = uu_direct::square_ring(x, y, width, height, false);
    }
    fn shuffle(&mut self, tiles: &mut Vec<CzTile>) {
        uu_direct::ai_random_shuffle(tiles, |lo, hi| uu_direct::with_game(|g| g.random_long(lo, hi)));
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
        // the unit's complex unit's DistanceSquared2D
        unit_geom::with_unit(u, |v| {
            let l = v.complex_unit().expect("a complex unit").location();
            unit_geom::distance_squared_2d([l[0], l[1]], [x, y])
        })
        .expect("a live unit")
    }
    fn tile_distance(&mut self, u: u64, center: u64) -> f32 {
        // GetTileDistanceSquared: the tiles' long differences, y first, squared and summed, then a float
        let (a, b) = (tile_of(u), tile_of(center));
        let dy = a.1.wrapping_sub(b.1);
        let dx = a.0.wrapping_sub(b.0);
        dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) as f32
    }
    fn path_area(&mut self, x: i32, y: i32) -> u16 {
        self.q(EE_UNITS_CZ_PATH_AREA, 0, 0, v2(x, y)) as u16
    }
    // the goals' fields: their Rust records (the C++ accessors' answers)
    fn gather_storage(&mut self, goal: u64) -> u64 {
        crate::units::gather::fields_of(goal).1
    }
    fn gather_resource(&mut self, goal: u64) -> u64 {
        crate::units::gather::fields_of(goal).0
    }
    fn is_gathering(&mut self, goal: u64) -> bool {
        crate::units::gather::fields_of(goal).2
    }
    fn build_target_is_wall(&mut self, goal: u64) -> bool {
        let class = crate::units::build::target_type_and_class(goal).1;
        // SAFETY: the goal's live class (GetBuildTargetClass()->mIsWall)
        let t = unsafe { crate::world::WorldRead::begin() };
        unsafe { crate::world::ClassView::of(&t, class) }.expect("a build target's class").wall()
    }
    fn build_target_type(&mut self, goal: u64) -> u32 {
        crate::units::build::target_type_and_class(goal).0 as u32
    }
    fn farm_drop_sites(&mut self, farm: u64) -> Vec<u64> {
        uu_direct::farm_drop_sites(farm).1
    }
    fn can_take(&mut self, storage: u64, u: u64, resource_type: u32) -> bool {
        self.q(EE_UNITS_CZ_CAN_TAKE, storage, u, [resource_type as i64, 0, 0, 0, 0]) != 0
    }
    fn empty_load(&mut self, u: u64, resource_type: u32) {
        // SetResourceAmount(0), SetResourceType(type): the unit record's member stores
        unit_geom::with_unit(u, |v| {
            let r = v.fields().ptr() as *mut crate::world::unit_fields::EeUnitsUnitFields;
            // SAFETY: a live unit's record, no other reference held
            unsafe {
                (*r).mResourceAmount = 0;
                (*r).mResourceType = resource_type as _;
            }
        });
    }
    fn distance_to_unit(&mut self, a: u64, b: u64) -> f32 {
        unit_geom::distance_squared_to_unit_of(a, b)
    }
    fn distance_to(&mut self, a: u64, b: u64) -> f32 {
        unit_geom::with_unit(a, |x| unit_geom::with_unit(b, |y| unit_geom::distance_squared_to(x, y)))
            .flatten()
            .expect("live units")
    }
    fn player_units(&mut self, player: u64) -> Vec<u64> {
        // SAFETY: as above; the player handle is a live player
        let t = unsafe { crate::world::WorldRead::begin() };
        unsafe { crate::world::PlayerView::of(&t, player) }
            .map_or_else(Vec::new, |p| p.units().map(|u| u.handle()).collect())
    }
}

// ---- the entry points (the `EEUnitClass` members; units and goals as their addresses)

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
