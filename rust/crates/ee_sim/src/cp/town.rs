//! `EETown`'s bodies (the AI Thread cluster, round 2): a player's town as an AI area (EETown.cpp 0xabbaf..0xac7b9).
//! Its counters, continent, reach and flags are the record [`EeTownFields`] the C++ class derives from (`class EETown :
//! public EEAIArea, public EeTownFields`, ee/EETown.h, the original's names and meanings); the owner and the patrol
//! group stay the class's typed members, reached through the host. Every body is here over the town's world
//! ([`TownHost`]: the units, the area the town is, the continent manager, the patrol groups; the game's host is
//! `ee_cp_town_host`, EETown.cpp), in the original's order of calls.

#![allow(non_snake_case)]
// `c_long`/`c_ulong` are 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast, clippy::useless_conversion)]

use std::os::raw::{c_long, c_ulong};

use super::herd::{random_area_point, EeHerdIo, EeHerdPoint};

/// `EETown`'s counters, continent, reach and flags (+0x70 .. +0x99 in the original).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTownFields {
    /// +0x70 the town's continent (the first tile's)
    pub mContinent: u16,
    /// +0x74 the town id (the constructor's)
    pub mTownID: c_long,
    /// +0x78 the buildings' influence (walls a tenth)
    pub mValue: c_ulong,
    pub mNumWalls: c_long,
    /// +0x80 the buildings that count (not walls or wonders)
    pub mL80: c_long,
    pub mNumWonders: c_long,
    pub mNumDocks: c_long,
    pub mNumAirports: c_long,
    pub mNumUnits: c_long,
    /// +0x94 the reach (squared tiles) a building may stand from the hull and join
    pub mReach: c_long,
    pub mNearWater: bool,
    pub mNearMapEdge: bool,
}

/// A building as the town reads it (`EEUnit` and its class; one host call).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeTownUnit {
    /// the class's `GetInfluence()`
    pub influence: c_long,
    /// the unit's tech node is a wonder (`mData->mKind == 4`)
    pub wonder: bool,
    pub wall: bool,
    pub unit_type: c_long,
    /// `FindAreaTile(true, 1)`
    pub area_x: c_long,
    pub area_y: c_long,
    /// `mPlayer`
    pub player: u64,
}

// ---- the host operations (`a` a unit or a town, `io.v` values; from 101: the area's operations the random point
// shares are the herd's codes, below 100)
/// unit `a` into `io.v` (the fields of [`EeTownUnit`] in order: influence, wonder, wall, type, area x, area y, player)
pub const EE_CP_TOWN_UNIT: i32 = 101;
/// `EETown::IsTownUnit(a)`
pub const EE_CP_TOWN_IS_TOWN_UNIT: i32 = 102;
/// the town's `mPlayer`
pub const EE_CP_TOWN_PLAYER: i32 = 103;
/// `GetNumTiles()`
pub const EE_CP_TOWN_NUM_TILES: i32 = 104;
/// `HullContains((v0, v1))`
pub const EE_CP_TOWN_HULL_CONTAINS: i32 = 105;
/// the hull's points (pushed)
pub const EE_CP_TOWN_HULL_POINTS: i32 = 106;
/// `GetTileSet().Insert((v0, v1))`
pub const EE_CP_TOWN_TILE_INSERT: i32 = 107;
/// `GetTileSet().Contains((v0, v1))`
pub const EE_CP_TOWN_TILE_CONTAINS: i32 = 108;
/// `GetTileSet().Remove((v0, v1))`
pub const EE_CP_TOWN_TILE_REMOVE: i32 = 109;
/// `GetTileSet().BuildPointList(points, 1)` of town `a` (0: this one), pushed
pub const EE_CP_TOWN_TILES: i32 = 110;
/// `Create` (the area's) over the input points
pub const EE_CP_TOWN_CREATE: i32 = 111;
/// `SetHullPoints` over the input points
pub const EE_CP_TOWN_SET_HULL: i32 = 112;
pub const EE_CP_TOWN_CLEAR_HULL: i32 = 113;
/// `GetClosestHullPoint((v0, v1))` into `v0`, `v1`
pub const EE_CP_TOWN_CLOSEST_HULL: i32 = 114;
/// `gContinentManager.GetContinentID(v0, v1)`
pub const EE_CP_TOWN_CONTINENT_AT: i32 = 115;
/// `gContinentManager.GetConnectedIDs(ids, v0)` (pushed)
pub const EE_CP_TOWN_CONNECTED: i32 = 116;
/// continent `v0` (`GetContinentFromID`): -1 none, else its category, and when `v1` its `GetClosestPoints(this
/// town)`: found into `v2`, the points into `v3..v6`
pub const EE_CP_TOWN_CONTINENT: i32 = 117;
/// whether the town has a patrol group
pub const EE_CP_TOWN_HAS_PATROL: i32 = 118;
/// `FindPatrolGroup` (0xac116: a group of an own path point on a hull vertex, referenced)
pub const EE_CP_TOWN_FIND_PATROL: i32 = 119;
/// a new town patrol group (`mIsTownPatrol`), added to the owner
pub const EE_CP_TOWN_NEW_PATROL: i32 = 120;
/// the patrol group's points set to the input points for the owner
pub const EE_CP_TOWN_SET_PATROL: i32 = 121;
/// the patrol group dereferenced and cleared
pub const EE_CP_TOWN_DROP_PATROL: i32 = 122;
/// `gLoadingSavedGame` (bit 0), `gWorldClearing` (bit 1)
pub const EE_CP_TOWN_FLAGS: i32 = 123;

/// `kContinentCategoryWater`, `kContinentCategorySpace`.
const CONTINENT_WATER: i64 = 2;
const CONTINENT_SPACE: i64 = 3;

/// The world the town asks: its own operations, and the area ones the herd's random point shares
/// ([`random_area_point`]'s codes, through `area`).
pub trait TownHost {
    fn call(&mut self, op: i32, a: u64, b: u64, io: &mut TownIo) -> i64;
    fn area(&mut self, op: i32, v: &[i64]) -> (i64, EeHerdIo);
}

/// One host operation's values.
#[repr(C)]
pub struct TownIo {
    pub v: [i64; 8],
    /// the points an operation reads (x, y pairs)
    pub input: *const i64,
    pub input_len: usize,
    /// the list an operation pushes onto (`ee_cp_herd_push`: four values a row)
    pub list: *mut std::ffi::c_void,
}

impl Default for TownIo {
    fn default() -> Self {
        TownIo { v: [0; 8], input: std::ptr::null(), input_len: 0, list: std::ptr::null_mut() }
    }
}

type Point = (c_long, c_long);

/// The town over its record and its host.
pub struct Town<'h, H: TownHost + ?Sized> {
    h: &'h mut H,
    f: *mut EeTownFields,
}

impl<'h, H: TownHost + ?Sized> Town<'h, H> {
    /// # Safety
    /// `f` is a live record for as long as the value lives.
    pub unsafe fn new(h: &'h mut H, f: *mut EeTownFields) -> Self {
        Town { h, f }
    }

    #[allow(clippy::mut_from_ref)]
    fn f(&self) -> &mut EeTownFields {
        // SAFETY: the contract of `new`; never borrowed across a host call
        unsafe { &mut *self.f }
    }
    fn q(&mut self, op: i32, a: u64, v: &[i64]) -> (i64, TownIo) {
        let mut io = TownIo::default();
        io.v[..v.len()].copy_from_slice(v);
        let r = self.h.call(op, a, 0, &mut io);
        (r, io)
    }
    fn points(&mut self, op: i32, a: u64, v: &[i64]) -> Vec<Point> {
        let mut rows: Vec<[i64; 4]> = Vec::new();
        let mut io = TownIo { list: &mut rows as *mut Vec<[i64; 4]> as *mut std::ffi::c_void, ..Default::default() };
        io.v[..v.len()].copy_from_slice(v);
        self.h.call(op, a, 0, &mut io);
        rows.iter().map(|r| (r[0] as c_long, r[1] as c_long)).collect()
    }
    fn with_points(&mut self, op: i32, points: &[Point]) {
        let flat: Vec<i64> = points.iter().flat_map(|p| [i64::from(p.0), i64::from(p.1)]).collect();
        let mut io = TownIo { input: flat.as_ptr(), input_len: points.len(), ..Default::default() };
        self.h.call(op, 0, 0, &mut io);
    }
    fn unit(&mut self, u: u64) -> EeTownUnit {
        let (_, io) = self.q(EE_CP_TOWN_UNIT, u, &[]);
        let v = io.v;
        EeTownUnit {
            influence: v[0] as c_long,
            wonder: v[1] != 0,
            wall: v[2] != 0,
            unit_type: v[3] as c_long,
            area_x: v[4] as c_long,
            area_y: v[5] as c_long,
            player: v[6] as u64,
        }
    }
    fn flags(&mut self) -> (bool, bool) {
        let r = self.q(EE_CP_TOWN_FLAGS, 0, &[]).0;
        (r & 1 != 0, r & 2 != 0)
    }
    fn hull_points(&mut self) -> Vec<Point> {
        self.points(EE_CP_TOWN_HULL_POINTS, 0, &[])
    }
    fn has_patrol(&mut self) -> bool {
        self.q(EE_CP_TOWN_HAS_PATROL, 0, &[]).0 != 0
    }
    fn tile_op(&mut self, op: i32, t: Point) -> bool {
        self.q(op, 0, &[i64::from(t.0), i64::from(t.1)]).0 != 0
    }

    /// The patrol group found or made once the hull has two points (`!gLoadingSavedGame` already tested by the
    /// callers that need it), its points the hull's.
    fn patrol_hull(&mut self, make: bool) {
        if make && !self.has_patrol() && self.hull_points().len() > 1 {
            self.q(EE_CP_TOWN_FIND_PATROL, 0, &[]);
            if !self.has_patrol() {
                self.q(EE_CP_TOWN_NEW_PATROL, 0, &[]);
            }
        }
        if self.has_patrol() {
            let points = self.hull_points();
            self.with_points(EE_CP_TOWN_SET_PATROL, &points);
        }
    }

    /// `AddUnit` (0xabc27): a building joins when it is the first or the town can take it: its tile added, the patrol
    /// group's points set to the hull, the counters (walls at a tenth of their value).
    pub fn add_unit(&mut self, unit: u64) -> bool {
        let u = self.unit(unit);
        let mut value = u.influence as c_ulong;
        let mut count: c_long = 1;
        let mut result = false;
        if self.q(EE_CP_TOWN_NUM_TILES, 0, &[]).0 != 0 || self.can_take(unit) {
            let tile = (u.area_x, u.area_y);
            result = true;
            self.add_tile(tile);
            if !self.flags().0 {
                self.patrol_hull(true);
            }
            if u.wall {
                self.f().mNumWalls = self.f().mNumWalls.wrapping_add(1);
                value = (value as f32 * 0.1f32) as c_ulong;
                count = 0;
            }
            match u.unit_type {
                TYPE_AIRPORT => self.f().mNumAirports = self.f().mNumAirports.wrapping_add(1),
                TYPE_DOCK | TYPE_NAVY_YARD => self.f().mNumDocks = self.f().mNumDocks.wrapping_add(1),
                _ => {}
            }
            if u.wonder {
                self.f().mNumWonders = self.f().mNumWonders.wrapping_add(1);
                count -= 1;
            }
            let f = self.f();
            f.mNumUnits = f.mNumUnits.wrapping_add(1);
            if count > 0 {
                f.mL80 = f.mL80.wrapping_add(1);
            }
            f.mValue = f.mValue.wrapping_add(value);
        }
        result
    }

    /// `AddTile` (0xabdf3): a tile joins (the first sets the continent); the hull is made again, and the water flags
    /// worked out, unless the tile is inside the hull and not one of its points.
    pub fn add_tile(&mut self, t: Point) -> bool {
        if self.f().mContinent == 0 {
            self.f().mContinent = self.q(EE_CP_TOWN_CONTINENT_AT, 0, &[i64::from(t.0), i64::from(t.1)]).0 as u16;
        }
        let remake = if self.tile_op(EE_CP_TOWN_HULL_CONTAINS, t) {
            let on_hull = self.hull_points().contains(&t);
            self.tile_op(EE_CP_TOWN_TILE_INSERT, t);
            on_hull
        } else {
            self.tile_op(EE_CP_TOWN_TILE_INSERT, t);
            true
        };
        if remake {
            let points = self.points(EE_CP_TOWN_TILES, 0, &[]);
            self.with_points(EE_CP_TOWN_CREATE, &points);
            if !self.flags().0 {
                self.update_near_water();
            }
        }
        true
    }

    /// `Merge` (0xabede): the other town's counters and tiles added (the hull is not made again).
    pub fn merge(&mut self, other: u64, o: &EeTownFields) {
        let points = self.points(EE_CP_TOWN_TILES, other, &[]);
        let f = self.f();
        f.mNumAirports = f.mNumAirports.wrapping_add(o.mNumAirports);
        f.mNumUnits = f.mNumUnits.wrapping_add(o.mNumUnits);
        f.mL80 = f.mL80.wrapping_add(o.mL80);
        f.mNumDocks = f.mNumDocks.wrapping_add(o.mNumDocks);
        f.mValue = f.mValue.wrapping_add(o.mValue);
        f.mNumWalls = f.mNumWalls.wrapping_add(o.mNumWalls);
        f.mNumWonders = f.mNumWonders.wrapping_add(o.mNumWonders);
        for &t in points.iter().rev() {
            self.tile_op(EE_CP_TOWN_TILE_INSERT, t);
        }
    }

    /// `Reset` (0xabf8f): the counters and continent zeroed, the reach 256, the patrol group released.
    pub fn reset(&mut self) {
        let f = self.f();
        f.mNumAirports = 0;
        f.mNumUnits = 0;
        f.mL80 = 0;
        f.mNumDocks = 0;
        f.mValue = 0;
        f.mNumWalls = 0;
        f.mNumWonders = 0;
        f.mContinent = 0;
        f.mReach = 0x100;
        self.q(EE_CP_TOWN_DROP_PATROL, 0, &[]);
    }

    /// `UpdateNearWater` (0xabfda): within 10 tiles of a connected water continent (and of a space one: the map-edge
    /// flag); the water flag returned.
    pub fn update_near_water(&mut self) -> bool {
        self.f().mNearWater = false;
        self.f().mNearMapEdge = false;
        let continent = self.f().mContinent;
        if continent != 0 && self.q(EE_CP_TOWN_NUM_TILES, 0, &[]).0 > 0 {
            let ids = self.points(EE_CP_TOWN_CONNECTED, 0, &[i64::from(continent)]);
            for &(id, _) in ids.iter().rev() {
                if self.f().mNearWater {
                    break;
                }
                let (category, _) = self.q(EE_CP_TOWN_CONTINENT, 0, &[i64::from(id), 0]);
                if category == CONTINENT_WATER || category == CONTINENT_SPACE {
                    let (_, io) = self.q(EE_CP_TOWN_CONTINENT, 0, &[i64::from(id), 1]);
                    if io.v[2] != 0 {
                        let dx = (io.v[3] as c_long).wrapping_sub(io.v[5] as c_long);
                        let dy = (io.v[4] as c_long).wrapping_sub(io.v[6] as c_long);
                        let near = dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) <= 100;
                        if category == CONTINENT_WATER {
                            self.f().mNearWater = near;
                        } else {
                            self.f().mNearMapEdge = near;
                        }
                    }
                }
            }
        }
        self.f().mNearWater
    }

    /// `GetRandomPoint` (0xac203).
    pub fn random_point(&mut self) -> EeHerdPoint {
        random_area_point(|op, v| self.h.area(op, v))
    }

    /// `RemoveUnit` (0xac351): the mirror of `AddUnit`, the counters down to 0 at least; whether its tile was in the
    /// town (true when the town cannot take it).
    pub fn remove_unit(&mut self, unit: u64) -> bool {
        let u = self.unit(unit);
        let mut value = u.influence as c_ulong;
        let mut count: c_long = 1;
        let mut result = true;
        let down = |n: c_long| if n > 0 { n - 1 } else { 0 };
        if self.can_take(unit) {
            result = self.remove_hull_tile((u.area_x, u.area_y));
            if u.wall {
                self.f().mNumWalls = down(self.f().mNumWalls);
                count = 0;
                value = (value as f32 * 0.1f32) as c_ulong;
            }
            match u.unit_type {
                TYPE_AIRPORT => self.f().mNumAirports = down(self.f().mNumAirports),
                TYPE_DOCK | TYPE_NAVY_YARD => self.f().mNumDocks = down(self.f().mNumDocks),
                _ => {}
            }
            if u.wonder {
                self.f().mNumWonders = down(self.f().mNumWonders);
                count -= 1;
            }
            let f = self.f();
            f.mNumUnits = down(f.mNumUnits);
            if count > 0 {
                f.mL80 = down(f.mL80);
            }
            if f.mValue > value {
                f.mValue -= value;
            } else {
                f.mValue = 0;
            }
        }
        result
    }

    /// `RefreshPatrolGroup` (0xac4b6): the patrol group found or made, its points the hull's.
    pub fn refresh_patrol_group(&mut self) {
        self.patrol_hull(true);
    }

    /// `RemoveHullTile` (0xac573): a tile leaves; when it was a hull point the hull is made again (cleared when no tile
    /// is left) and the patrol points set. Whether the tile was in the town.
    pub fn remove_hull_tile(&mut self, t: Point) -> bool {
        let on_hull = self.hull_points().contains(&t);
        let mut result = false;
        if self.tile_op(EE_CP_TOWN_TILE_CONTAINS, t) {
            result = true;
            self.tile_op(EE_CP_TOWN_TILE_REMOVE, t);
        }
        if on_hull {
            let points = self.points(EE_CP_TOWN_TILES, 0, &[]);
            let (loading, clearing) = self.flags();
            if !points.is_empty() {
                self.with_points(EE_CP_TOWN_SET_HULL, &points);
                if !loading && !clearing {
                    self.update_near_water();
                }
            } else {
                self.q(EE_CP_TOWN_CLEAR_HULL, 0, &[]);
            }
            if !self.flags().0 {
                if !self.has_patrol() {
                    self.q(EE_CP_TOWN_FIND_PATROL, 0, &[]);
                }
                self.patrol_hull(false);
            }
        }
        result
    }

    /// `CanTake` (0xac6f7): an own town building belongs when the town is empty, else when it stands on the town's
    /// continent inside the hull or within the reach of its closest hull point.
    pub fn can_take(&mut self, unit: u64) -> bool {
        let mut result = false;
        let player = self.q(EE_CP_TOWN_PLAYER, 0, &[]).0 as u64;
        let u = self.unit(unit);
        if u.player == player && self.q(EE_CP_TOWN_IS_TOWN_UNIT, unit, &[]).0 != 0 {
            result = self.q(EE_CP_TOWN_NUM_TILES, 0, &[]).0 == 0;
            if !result {
                let t = (u.area_x, u.area_y);
                let id = self.q(EE_CP_TOWN_CONTINENT_AT, 0, &[i64::from(t.0), i64::from(t.1)]).0 as u16;
                let continent = self.f().mContinent;
                if id == continent || continent == 0 {
                    result = self.tile_op(EE_CP_TOWN_HULL_CONTAINS, t);
                    if !result {
                        let (_, io) = self.q(EE_CP_TOWN_CLOSEST_HULL, 0, &[i64::from(t.0), i64::from(t.1)]);
                        let dx = (io.v[0] as c_long).wrapping_sub(t.0);
                        let dy = (io.v[1] as c_long).wrapping_sub(t.1);
                        result = dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) <= self.f().mReach;
                    }
                } else {
                    result = false;
                }
            }
        }
        result
    }
}

/// `kUnitTypeAirport`, `kUnitTypeDock`, `kUnitTypeNavyYard` (ee/EEUnitClassIDs.h).
const TYPE_AIRPORT: c_long = 0x42;
const TYPE_DOCK: c_long = 0x24;
const TYPE_NAVY_YARD: c_long = 0x41;
