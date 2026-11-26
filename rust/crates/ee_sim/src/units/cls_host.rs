//! The unit class bodies' host in the game (R5b.4, the thirty-third round): [`TrHost`] over the unit utilities'
//! world reads ([`crate::uu_host::Game`]) and one C++ switch for the rest (`ee_units_cls_host`,
//! EEUnitClassTransports.cpp), and the entry points the C++ dispatchers and members call. Vectors cross as their
//! C++ objects (read and refilled through the unit utilities' host), tiles as `U2DPoint<long>`s (`[c_long; 2]`),
//! units and classes as their objects' addresses. Absent from a test binary.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 on LP64: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::c_long;

use ee_base::math::types::{Ge3DPoint, GeTransformation};

use crate::units::cls_air::{self as air, AirHost, ConditionHost, FlightHost};
use crate::units::cls_building::{self as bld, BuildingHost};
use crate::units::cls_transport::{self as tr, Cls, TrHost};
use crate::units::unit_class_fields::EeUnitsUnitClassFields;
use crate::units::uu::{EeUnitsUuUnit, UuHost};
use crate::units::uu_host::{set_tiles, set_units, tiles_of, units_of, Game};
use crate::units::uu_tiles::Tile;

/// One host operation's values.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeUnitsClsIo {
    /// a tile or two values in and out
    pub a: i64,
    pub b: i64,
    pub f: f32,
    /// a point's x, y, z
    pub p: [f32; 3],
    /// a second point
    pub q: [f32; 3],
    /// the list the operation pushes onto (`ee_units_cls_push`)
    pub list: *mut c_void,
}

/// The waypoint operations (the paths lane's round 1: the list and its bodies are Rust, waypoints.rs); `a` the unit.
fn waypoint_op(op: i32, a: u64, c: i64, io: &mut EeUnitsClsIo) -> Option<i64> {
    use crate::units::cls_move_host::{
        EE_UNITS_CLS_MV_ADD_WAYPOINT, EE_UNITS_CLS_MV_NEXT_WAYPOINT, EE_UNITS_CLS_MV_SECOND_WAYPOINT,
        EE_UNITS_CLS_MV_WAYPOINT,
    };
    use crate::units::waypoints;
    // SAFETY (each arm): the operation's unit `a` is a live complex unit
    Some(match op {
        EE_UNITS_CLS_WAYPOINTS => unsafe { waypoints::list(a) }.len() as i64,
        EE_UNITS_CLS_NEXT_WAYPOINT => {
            // GetNextWaypointPoint into a zero point
            io.p = unsafe { waypoints::list(a) }.front().map_or([0.0; 3], |w| w.location);
            if unsafe { waypoints::list(a) }.is_empty() {
                waypoints::EE_UNITS_WAYPOINTS_NONE as i64
            } else {
                0
            }
        }
        EE_UNITS_CLS_DROP_PATH => {
            unsafe { waypoints::drop_path(a) };
            0
        }
        EE_UNITS_CLS_MV_ADD_WAYPOINT => {
            unsafe { waypoints::add(a, io.p, c != 0) };
            0
        }
        EE_UNITS_CLS_MV_WAYPOINT => {
            io.p = unsafe { waypoints::list(a) }.get(c as usize).expect("a waypoint index").location;
            0
        }
        EE_UNITS_CLS_MV_SECOND_WAYPOINT => match unsafe { waypoints::list(a) }.second() {
            Some(p) => {
                io.p = p;
                0
            }
            None => waypoints::EE_UNITS_WAYPOINTS_NONE as i64,
        },
        EE_UNITS_CLS_MV_NEXT_WAYPOINT => {
            unsafe { waypoints::next(a) };
            0
        }
        _ => return None,
    })
}

/// The list a host operation fills (pushed one pair at a time).
#[derive(Default)]
pub struct EeUnitsClsListHandle {
    pub(crate) tiles: Vec<Tile>,
}

// ---- the host operations (`ee_units_cls_host`'s `op`)
/// `gContinentManager.IsNextToArea(c, d, a)`
pub const EE_UNITS_CLS_NEXT_TO_AREA: i32 = 1;
/// continent `a`'s `GetShoreInRegion(b, list, c)`
pub const EE_UNITS_CLS_SHORE_IN_REGION: i32 = 2;
/// continent `a`'s `GetShoreNextTo(b, list)`
pub const EE_UNITS_CLS_SHORE_NEXT_TO: i32 = 3;
/// `gContinentManager.GetAdjacentIDs(list, b)` (each id pushed as a pair's x)
pub const EE_UNITS_CLS_ADJACENT_IDS: i32 = 4;
/// `EEGetBlockingUnits(player a, unit type c, tile b, 0, ...)`
pub const EE_UNITS_CLS_BLOCKED: i32 = 5;
/// `EEGetTilesInRadius(c, d, 2, true, list)`
pub const EE_UNITS_CLS_TILES_IN_RADIUS: i32 = 6;
/// `U2DPoint(c, d).GetLineTiles(U2DPoint(io.a, io.b), 1.0f, list)`
pub const EE_UNITS_CLS_LINE_TILES: i32 = 7;
/// `EEGetPathLengthTo(unit a, tile (io.a, io.b), 0x10, b != 0)` into `io.f`, the tile written back
pub const EE_UNITS_CLS_PATH_LENGTH: i32 = 8;
/// `gWorld.mMap->GetTileLocation(c, d, io.p)`
pub const EE_UNITS_CLS_TILE_LOCATION: i32 = 9;
/// unit `a`'s `SetLocation(io.p, false)`
pub const EE_UNITS_CLS_SET_LOCATION: i32 = 10;
/// unit `a`'s class's `GetSurroundingTiles(a, list, 0, false)`
pub const EE_UNITS_CLS_SURROUNDING: i32 = 11;
/// unit `a`'s load transport goal's `GetLoadUnitTile()` into `io.a`, `io.b` (0: no such goal)
pub const EE_UNITS_CLS_LOAD_GOAL_TILE: i32 = 12;
/// unit `a`'s `mCargo[c]`
pub const EE_UNITS_CLS_CARGO: i32 = 13;
/// unit `a`'s `mContainer`
pub const EE_UNITS_CLS_CONTAINER: i32 = 14;
/// unit `a`'s `mWaypoints.size()`
pub const EE_UNITS_CLS_WAYPOINTS: i32 = 15;
/// unit `a`'s `GetNextWaypointPoint(io.p)` (its answer: 0 when there is one)
pub const EE_UNITS_CLS_NEXT_WAYPOINT: i32 = 16;
/// unit `a`'s `GetQueueGoals(c).size()`
pub const EE_UNITS_CLS_QUEUE_GOALS: i32 = 17;
/// unit `a`'s `mStats->GetMaxVelocity()` into `io.p[0]`, `GetMaxAngularVelocity()` into `io.p[1]`
pub const EE_UNITS_CLS_TURN_SPEEDS: i32 = 18;
/// `EEGetTilesNearPoint(io.p, io.f, list)`
pub const EE_UNITS_CLS_TILES_NEAR_POINT: i32 = 19;
/// unit `a`'s `mHomeBase`
pub const EE_UNITS_CLS_HOME_BASE: i32 = 20;
/// unit `a`'s goal's flight target into `io.p` (0: none; cls_air.rs `FlightHost::flight_target`)
pub const EE_UNITS_CLS_FLIGHT_TARGET: i32 = 21;
/// unit `a`'s `DropPath()`
pub const EE_UNITS_CLS_DROP_PATH: i32 = 22;
/// class `b`'s `AddWaypointAt(unit a, io.p, c != 0)`
pub const EE_UNITS_CLS_ADD_WAYPOINT: i32 = 23;
/// class `b`'s `PathToward(unit a, io.p, io.q, c != 0)`
pub const EE_UNITS_CLS_PATH_TOWARD: i32 = 24;
/// class `b`'s `FlyToGoal(io.p, unit a)`
pub const EE_UNITS_CLS_FLY_TO_GOAL: i32 = 25;
/// unit `a`'s runway move goal at `io.p`
pub const EE_UNITS_CLS_RUNWAY_GOAL: i32 = 26;
/// `gWorld.mMap->mTerrainMesh.GetIsSpaceMap()`
pub const EE_UNITS_CLS_IS_SPACE_MAP: i32 = 27;
/// `gWorld.mMap->mSize`
pub const EE_UNITS_CLS_MAP_SIZE: i32 = 28;
/// continent `a`'s `GetHullCenter()` into `io.a`, `io.b`
pub const EE_UNITS_CLS_HULL_CENTER: i32 = 29;
/// `EEGetUnitsNear(io.p, io.f, out)`: each unit pushed as its address's low and high halves
pub const EE_UNITS_CLS_UNITS_NEAR: i32 = 30;
/// unit `a`'s command's type (0: none)
pub const EE_UNITS_CLS_COMMAND_TYPE: i32 = 31;
/// class `b`'s `GetGroundHeight(io.p, &io.f)`
pub const EE_UNITS_CLS_GROUND_HEIGHT: i32 = 32;
/// class `b`'s `IsBombOnTarget(unit a)`
pub const EE_UNITS_CLS_BOMB_ON_TARGET: i32 = 33;
/// class `b`'s `IsAwayFromGoal(unit a)`
pub const EE_UNITS_CLS_AWAY_FROM_GOAL: i32 = 34;
/// class `b`'s `mProjectileClass`' `mCategory` (-1: none)
pub const EE_UNITS_CLS_PROJECTILE_CATEGORY: i32 = 35;
/// unit `a`'s attack goal has a target with hit points
pub const EE_UNITS_CLS_ATTACK_TARGET_ALIVE: i32 = 36;
/// `EEGoalHasLocation(unit a's goal)`
pub const EE_UNITS_CLS_GOAL_HAS_LOCATION: i32 = 37;
/// unit `a`'s air traffic control has it queued (`GetQueuePosition(a, 1)` or `(a, 0)` at least 0)
pub const EE_UNITS_CLS_REQUEST_QUEUED: i32 = 38;

/// The class bodies' host's plain reads over the world views (the units' behaviour lane, round 2; they were
/// EEUnitClassTransports.cpp `ee_units_cls_host`'s cases): the unit `a`'s cargo, container, command, home base and
/// stats' turn speeds, the class `b`'s projectile's category, the map's size; None for the C++ host's.
fn cls_view(op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsClsIo) -> Option<i64> {
    use crate::world::view::{ClassView, UnitView, WorldRead, WorldView};
    if let Some(v) = waypoint_op(op, a, c, io) {
        return Some(v);
    }
    if let Some(v) = crate::units::cls_mv_view::cls_mv_view(op, a, b, c, d, io) {
        return Some(v);
    }
    if !matches!(
        op,
        EE_UNITS_CLS_CARGO
            | EE_UNITS_CLS_CONTAINER
            | EE_UNITS_CLS_COMMAND_TYPE
            | EE_UNITS_CLS_HOME_BASE
            | EE_UNITS_CLS_TURN_SPEEDS
            | EE_UNITS_CLS_PROJECTILE_CATEGORY
            | EE_UNITS_CLS_MAP_SIZE
    ) {
        return None;
    }
    // SAFETY: the class bodies run inside the C++ caller's hold of the world; their handles are live
    let t = unsafe { WorldRead::begin() };
    if op == EE_UNITS_CLS_MAP_SIZE {
        return Some(WorldView::global(&t)?.map()?.size());
    }
    if op == EE_UNITS_CLS_PROJECTILE_CATEGORY {
        // SAFETY: as above (`b` a class)
        let class = unsafe { ClassView::of(&t, b) }?;
        // SAFETY: the class's projectile class (0: none)
        return Some(unsafe { ClassView::of(&t, class.projectile_class_handle()) }.map_or(-1, |p| p.category() as i64));
    }
    // SAFETY: as above (`a` a complex unit)
    let u = unsafe { UnitView::of(&t, a) }?;
    if !u.is_complex() {
        return None;
    }
    Some(match op {
        EE_UNITS_CLS_CARGO => {
            if c < 0 || c as usize >= u.cargo_len() {
                return None;
            }
            u.cargo_handle(c as usize) as i64
        }
        EE_UNITS_CLS_CONTAINER => u.container_handle() as i64,
        EE_UNITS_CLS_COMMAND_TYPE => i64::from(u.command_type()),
        EE_UNITS_CLS_HOME_BASE => u.home_base_handle() as i64,
        _ => {
            let s = u.stats()?.fields();
            io.p[0] = crate::world_field!(s, mMaxVelocity);
            io.p[1] = crate::world_field!(s, mMaxAngularVelocity);
            0
        }
    })
}

extern "C" {
    fn ee_units_cls_host(op: i32, a: u64, b: u64, c: i64, d: i64, io: *mut EeUnitsClsIo) -> i64;
}

// ---- round 9's statements (3400..3449: EEUnitClassMoveOp's switch, EEUnitClass.cpp, takes every op from 39)
/// `p = gWorld.mMap->GetTileLocation(c, d)` (untouched when there is no tile)
pub const EE_UNITS_CLS_R_TILE_LOCATION: i32 = 3400;
/// `EELoadTransportGoalCast(unit->GetGoal())`
pub const EE_UNITS_CLS_R_LOAD_GOAL: i32 = 3401;
/// goal `b`'s `GetLoadUnitTile()` (x, y << 32)
pub const EE_UNITS_CLS_R_LOAD_GOAL_TILE: i32 = 3402;
/// `EEAttackUnitCast(unit->GetGoal())`
pub const EE_UNITS_CLS_R_ATTACK_GOAL: i32 = 3403;
/// goal `b`'s `GetAttackTarget()`
pub const EE_UNITS_CLS_R_ATTACK_TARGET_OF: i32 = 3404;
/// unit `b`'s `mHitPoints`
pub const EE_UNITS_CLS_R_HIT_POINTS_OF: i32 = 3405;
/// `unit->mPlayer->mAirTraffic[unit->GetUnitClass()->mAirTrafficType - 1]`
pub const EE_UNITS_CLS_R_ATC: i32 = 3406;
/// continent `b`'s `GetHullCenter()` (x, y << 32)
pub const EE_UNITS_CLS_R_HULL_CENTER: i32 = 3407;
/// `p = ` goal `b`'s `GetGroundTarget()`
pub const EE_UNITS_CLS_R_GROUND_TARGET: i32 = 3408;
/// unit `b`'s `mComplexUnit`
pub const EE_UNITS_CLS_R_COMPLEX_OF: i32 = 3409;
/// `p = ` complex unit `b`'s location
pub const EE_UNITS_CLS_R_LOCATION_OF: i32 = 3410;
/// `p = ` unit `b`'s tile centre and its tile's height
pub const EE_UNITS_CLS_R_TILE_CENTER_OF: i32 = 3411;
/// goal `b`'s `GetPatrolTarget()`
pub const EE_UNITS_CLS_R_PATROL_TARGET: i32 = 3412;
/// goal `b`'s `GetExploreTileCenter(p)`
pub const EE_UNITS_CLS_R_EXPLORE_CENTER: i32 = 3413;
/// `unit->SetGoal(goal b, c, 0)`
pub const EE_UNITS_CLS_R_SET_GOAL_KEEP: i32 = 3414;

const GOAL_LOCATION: i64 = 1;
const GOAL_ATTACK_UNIT: i64 = 2;
const GOAL_ATTACK_GROUND: i64 = 3;
const GOAL_PATROL: i64 = 9;
const GOAL_EXPLORE: i64 = 10;

/// One of the air classes' bodies (uca_air.rs's `ee_units_uca_call`, as the C++ member called it).
pub(crate) fn uca_call(what: i32, class: u64, unit: u64, a: u64, p: &mut [f32; 4]) {
    #[cfg(not(test))]
    // SAFETY: the live class and unit the caller handed over; `p` a local point
    unsafe {
        crate::units::uca_air::game::ee_units_uca_call(
            what,
            class as usize as *mut c_void,
            unit as usize as *mut c_void,
            a,
            0,
            0,
            p.as_mut_ptr(),
        );
    }
    #[cfg(test)]
    let _ = (what, class, unit, a, p);
}

/// Pushes a pair onto a host operation's list.
///
/// # Safety
/// `list` is the `EeUnitsClsIo::list` of a running operation.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_push(list: *mut EeUnitsClsListHandle, x: i32, y: i32) {
    // SAFETY: the caller's contract
    unsafe { (*list).tiles.push([x, y]) }
}

/// The game's world for the class bodies.
pub struct ClsGame(pub Game);

/// The naval building's `HasSunk` (the dock): the trigger's sunk test ([`cond::sunk`]) over the same statements.
pub(crate) fn has_sunk(unit: u64) -> bool {
    cond::sunk(&mut ClsGame(Game), unit)
}

impl ClsGame {
    pub(crate) fn call(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsClsIo) -> i64 {
        if let Some(v) = cls_view(op, a, b, c, d, io) {
            return v;
        }
        // SAFETY: the C++ switch reads live objects the bodies hand it
        unsafe { ee_units_cls_host(op, a, b, c, d, io) }
    }
    fn list(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, ab: Tile) -> Vec<Tile> {
        let mut handle = EeUnitsClsListHandle::default();
        let mut io = EeUnitsClsIo {
            a: ab[0] as i64,
            b: ab[1] as i64,
            list: std::ptr::addr_of_mut!(handle).cast(),
            ..Default::default()
        };
        self.call(op, a, b, c, d, &mut io);
        handle.tiles
    }
}

impl TrHost for ClsGame {
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        self.0.unit(obj)
    }
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields {
        self.0.class(class).fields
    }
    fn map_tile(&mut self, x: i32, y: i32) -> u64 {
        let map = self.0.map();
        self.0.map_tile(map, x, y)
    }
    fn tile_terrain(&mut self, tile: u64) -> i32 {
        self.0.tile_terrain(tile)
    }
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool {
        self.0.is_edge_tile(x, y)
    }
    fn tile_visible_to(&mut self, tile: u64, player: u64) -> bool {
        self.0.tile_visible_to(tile, player)
    }
    fn tile_location(&mut self, x: i32, y: i32) -> Ge3DPoint {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_R_TILE_LOCATION, 0, 0, x as i64, y as i64, &mut io);
        Ge3DPoint { x: io.p[0], y: io.p[1], z: io.p[2] }
    }
    fn continent_id(&mut self, x: i32, y: i32) -> u16 {
        self.0.continent_id(x, y)
    }
    fn continent_at(&mut self, x: i32, y: i32) -> u64 {
        self.0.continent_at(x, y)
    }
    fn continent_number(&mut self, continent: u64) -> u16 {
        self.0.continent_number(continent)
    }
    fn continent_category(&mut self, continent: u64) -> u8 {
        self.0.continent_category(continent)
    }
    fn continent_by_id(&mut self, id: u16) -> u64 {
        self.0.continent_by_id(id)
    }
    fn continent_of(&mut self, x: i32, y: i32, kind: i32) -> u16 {
        self.0.continent_of(x, y, kind)
    }
    fn continent_link(&mut self, row: u16, column: u16) -> bool {
        self.0.continent_link(row, column)
    }
    fn next_to_area(&mut self, x: i32, y: i32, id: u16) -> bool {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_NEXT_TO_AREA, u64::from(id), 0, x as i64, y as i64, &mut io) != 0
    }
    fn adjacent_ids(&mut self, id: u16) -> Vec<u16> {
        self.list(EE_UNITS_CLS_ADJACENT_IDS, 0, u64::from(id), 0, 0, [0, 0]).iter().map(|t| t[0] as u16).collect()
    }
    fn shore_in_region(&mut self, continent: u64, id: u16, kind: i32) -> Vec<Tile> {
        self.list(EE_UNITS_CLS_SHORE_IN_REGION, continent, u64::from(id), kind as i64, 0, [0, 0])
    }
    fn shore_next_to(&mut self, continent: u64, id: u16) -> Vec<Tile> {
        self.list(EE_UNITS_CLS_SHORE_NEXT_TO, continent, u64::from(id), 0, 0, [0, 0])
    }
    fn blocked(&mut self, player: u64, unit_type: u32, tile: u64) -> bool {
        // EEGetBlockingUnits' answer (ee_units_uu_tile_units_standing's, its note included)
        crate::units::uu_ffi::tile_units_standing(player, unit_type as i32, tile, false, 2).0
    }
    fn line_blocked(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) -> bool {
        self.0.line_blocked(x1, y1, x2, y2)
    }
    fn tile_line(&mut self, from: Tile, to: Tile) -> Vec<Tile> {
        self.0.tile_line(from[0], from[1], to[0], to[1], true, false)
    }
    fn tiles_in_radius(&mut self, at: Tile) -> Vec<Tile> {
        self.list(EE_UNITS_CLS_TILES_IN_RADIUS, 0, 0, at[0] as i64, at[1] as i64, [0, 0])
    }
    fn line_tiles(&mut self, from: Tile, to: Tile) -> Vec<Tile> {
        self.list(EE_UNITS_CLS_LINE_TILES, 0, 0, from[0] as i64, from[1] as i64, to)
    }
    fn path_length(&mut self, unit: u64, tile: &mut Tile, walls: bool) -> f32 {
        // EEGetPathLengthTo(unit, tile, 0x10, walls): ee_units_uu_path_length_to (its note included)
        let mut at = [tile[0], tile[1]];
        // SAFETY: a live unit, a local tile
        let length = unsafe { crate::units::uu_ffi::ee_units_uu_path_length_to(unit, &mut at, 0x10, u8::from(walls)) };
        *tile = [at[0], at[1]];
        length
    }
    fn set_location(&mut self, unit: u64, point: Ge3DPoint) {
        let mut io = EeUnitsClsIo { p: [point.x, point.y, point.z], ..Default::default() };
        self.call(EE_UNITS_CLS_SET_LOCATION, unit, 0, 0, 0, &mut io);
    }
    fn surrounding_tiles(&mut self, unit: u64) -> Vec<Tile> {
        self.list(EE_UNITS_CLS_SURROUNDING, unit, 0, 0, 0, [0, 0])
    }
    fn load_goal_tile(&mut self, unit: u64) -> Option<Tile> {
        let mut io = EeUnitsClsIo::default();
        let goal = self.call(EE_UNITS_CLS_R_LOAD_GOAL, unit, 0, 0, 0, &mut io) as u64;
        if goal == 0 {
            return None;
        }
        let tile = self.call(EE_UNITS_CLS_R_LOAD_GOAL_TILE, unit, goal, 0, 0, &mut io);
        Some([tile as i32, (tile >> 32) as i32])
    }
    fn cargo(&mut self, unit: u64, index: i32) -> u64 {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_CARGO, unit, 0, index as i64, 0, &mut io) as u64
    }
    fn container(&mut self, unit: u64) -> u64 {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_CONTAINER, unit, 0, 0, 0, &mut io) as u64
    }
    fn waypoints(&mut self, unit: u64) -> i32 {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_WAYPOINTS, unit, 0, 0, 0, &mut io) as i32
    }
    fn next_waypoint(&mut self, unit: u64) -> Ge3DPoint {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_NEXT_WAYPOINT, unit, 0, 0, 0, &mut io);
        Ge3DPoint { x: io.p[0], y: io.p[1], z: io.p[2] }
    }
    fn queue_goals(&mut self, unit: u64, index: i32) -> i32 {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_QUEUE_GOALS, unit, 0, index as i64, 0, &mut io) as i32
    }
    fn goal_kind(&mut self, unit: u64) -> i32 {
        self.0.goal_kind(unit)
    }
}

impl AirHost for ClsGame {
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        self.0.unit(obj)
    }
    fn location(&mut self, unit: u64) -> GeTransformation {
        self.0.location(unit)
    }
    fn turn_speeds(&mut self, unit: u64) -> (f32, f32) {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_TURN_SPEEDS, unit, 0, 0, 0, &mut io);
        (io.p[0], io.p[1])
    }
    fn map_tile(&mut self, x: i32, y: i32) -> u64 {
        let map = self.0.map();
        self.0.map_tile(map, x, y)
    }
    fn tile_height(&mut self, tile: u64) -> f32 {
        self.0.tile_height(tile)
    }
    fn mesh_z(&mut self, x: i32, y: i32) -> f32 {
        let map = self.0.map();
        self.0.mesh_z(map, x, y)
    }
    fn tile_line(&mut self, from: Tile, to: Tile) -> Vec<Tile> {
        self.0.tile_line(from[0], from[1], to[0], to[1], true, false)
    }
    fn tiles_near_point(&mut self, point: Ge3DPoint, distance: f32) -> Vec<Tile> {
        let mut handle = EeUnitsClsListHandle::default();
        let mut io = EeUnitsClsIo {
            f: distance,
            p: [point.x, point.y, point.z],
            list: std::ptr::addr_of_mut!(handle).cast(),
            ..Default::default()
        };
        self.call(EE_UNITS_CLS_TILES_NEAR_POINT, 0, 0, 0, 0, &mut io);
        handle.tiles
    }
}

impl ClsGame {
    fn point_op(&mut self, op: i32, unit: u64, class: u64, flag: i64, p: Ge3DPoint, q: Ge3DPoint) -> i64 {
        let mut io = EeUnitsClsIo { p: [p.x, p.y, p.z], q: [q.x, q.y, q.z], ..Default::default() };
        self.call(op, unit, class, flag, 0, &mut io)
    }
}

impl FlightHost for ClsGame {
    fn home_base(&mut self, unit: u64) -> u64 {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_HOME_BASE, unit, 0, 0, 0, &mut io) as u64
    }
    fn waypoints(&mut self, unit: u64) -> i32 {
        TrHost::waypoints(self, unit)
    }
    fn next_waypoint(&mut self, unit: u64) -> Ge3DPoint {
        TrHost::next_waypoint(self, unit)
    }
    fn flight_target(&mut self, unit: u64) -> Option<Ge3DPoint> {
        // GetNextFlightState's goal target (0x615ccf): the goal's ground target, its attack target's point (its
        // tile's centre and height for a simple unit), its patrol target's point or its explore tile's centre
        let mut io = EeUnitsClsIo::default();
        let goal = self.call(crate::units::cls_move_host::EE_UNITS_CLS_MV_GOAL, unit, 0, 0, 0, &mut io) as u64;
        if goal == 0 {
            return None;
        }
        match self.call(crate::units::cls_move_host::EE_UNITS_CLS_MV_GOAL_KIND, unit, goal, 0, 0, &mut io) {
            GOAL_LOCATION | GOAL_ATTACK_GROUND => {
                self.call(EE_UNITS_CLS_R_GROUND_TARGET, unit, goal, 0, 0, &mut io);
            }
            GOAL_ATTACK_UNIT => {
                let other = self.call(EE_UNITS_CLS_R_ATTACK_TARGET_OF, unit, goal, 0, 0, &mut io) as u64;
                if other == 0 {
                    return None;
                }
                let complex = self.call(EE_UNITS_CLS_R_COMPLEX_OF, unit, other, 0, 0, &mut io) as u64;
                if complex != 0 {
                    self.call(EE_UNITS_CLS_R_LOCATION_OF, unit, complex, 0, 0, &mut io);
                } else {
                    self.call(EE_UNITS_CLS_R_TILE_CENTER_OF, unit, other, 0, 0, &mut io);
                }
            }
            GOAL_PATROL => {
                let target = self.call(EE_UNITS_CLS_R_PATROL_TARGET, unit, goal, 0, 0, &mut io) as u64;
                self.call(EE_UNITS_CLS_R_LOCATION_OF, unit, target, 0, 0, &mut io);
            }
            GOAL_EXPLORE => {
                self.call(EE_UNITS_CLS_R_EXPLORE_CENTER, unit, goal, 0, 0, &mut io);
            }
            _ => return None,
        }
        Some(Ge3DPoint { x: io.p[0], y: io.p[1], z: io.p[2] })
    }
    fn drop_path(&mut self, unit: u64) {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_DROP_PATH, unit, 0, 0, 0, &mut io);
    }
    fn add_waypoint(&mut self, class: u64, unit: u64, point: Ge3DPoint, snap: bool) {
        self.point_op(EE_UNITS_CLS_ADD_WAYPOINT, unit, class, i64::from(snap), point, Ge3DPoint::default());
    }
    fn path_toward(&mut self, _class: u64, unit: u64, from: Ge3DPoint, to: Ge3DPoint, snap: bool) {
        // PathToward's one call
        crate::units::cls_move_host::ee_units_mv_path_toward(
            unit,
            from.x,
            from.y,
            from.z,
            to.x,
            to.y,
            to.z,
            u8::from(snap),
        );
    }
    fn fly_to_goal(&mut self, class: u64, point: Ge3DPoint, unit: u64) {
        // FlyToGoal's one call (uca_air.rs)
        let mut p = [point.x, point.y, point.z, 0.0];
        uca_call(crate::units::uca_air::EE_UNITS_UCA_DO_FLY_TO_GOAL, class, unit, 0, &mut p);
    }
    fn runway_goal(&mut self, unit: u64, point: Ge3DPoint) {
        // ApproachRunway's move goal (0x615f7d): kept behind an exploration, else the only goal
        let goal = self.point_op(
            crate::units::cls_carrier_host::EE_UNITS_CLS_CR_DECK_GOAL,
            unit,
            0,
            0,
            point,
            Ge3DPoint::default(),
        );
        let mut io = EeUnitsClsIo::default();
        let state = self.call(crate::units::cls_move_host::EE_UNITS_CLS_MV_GOAL, unit, 0, 0, 0, &mut io) as u64;
        let keep = state != 0
            && self.call(crate::units::cls_move_host::EE_UNITS_CLS_MV_GOAL_KIND, unit, state, 0, 0, &mut io)
                == GOAL_EXPLORE;
        if !keep {
            self.call(crate::units::cls_carrier_host::EE_UNITS_CLS_CR_CLEAR_GOAL_QUEUE, unit, 0, 0, 0, &mut io);
        }
        self.call(EE_UNITS_CLS_R_SET_GOAL_KEEP, unit, goal as u64, i64::from(keep), 0, &mut io);
    }
}

impl BuildingHost for ClsGame {
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        self.0.unit(obj)
    }
    fn map_tile(&mut self, x: i32, y: i32) -> u64 {
        let map = self.0.map();
        self.0.map_tile(map, x, y)
    }
    fn tile_terrain(&mut self, tile: u64) -> i32 {
        self.0.tile_terrain(tile)
    }
    fn raised_corners(&mut self, x: i32, y: i32) -> i32 {
        let map = self.0.map();
        self.0.raised_corners(map, x, y)
    }
    fn mesh_z(&mut self, x: i32, y: i32) -> f32 {
        let map = self.0.map();
        self.0.mesh_z(map, x, y)
    }
    fn is_space_map(&mut self) -> bool {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_IS_SPACE_MAP, 0, 0, 0, 0, &mut io) != 0
    }
    fn map_size(&mut self) -> i32 {
        let mut io = EeUnitsClsIo::default();
        self.call(EE_UNITS_CLS_MAP_SIZE, 0, 0, 0, 0, &mut io) as i32
    }
    fn continent_at(&mut self, x: i32, y: i32) -> u64 {
        self.0.continent_at(x, y)
    }
    fn hull_center(&mut self, continent: u64) -> Tile {
        let mut io = EeUnitsClsIo::default();
        let center = self.call(EE_UNITS_CLS_R_HULL_CENTER, 0, continent, 0, 0, &mut io);
        [center as i32, (center >> 32) as i32]
    }
    fn position(&mut self, unit: u64) -> [f32; 3] {
        self.0.position(unit)
    }
    fn units_near(&mut self, point: [f32; 3], radius: f32) -> Vec<u64> {
        // EEGetUnitsNear (ee_units_uu_units_near's units)
        crate::units::uu_ffi::units_near(point, radius)
    }
    fn unit_set_contains(&mut self, unit_type: u32, set: i32) -> bool {
        self.0.unit_set_contains(unit_type, set)
    }
    fn enemy(&mut self, player: u64, other_id: i32) -> bool {
        self.0.enemy(player, other_id)
    }
    fn part(&mut self, complex: u64, next: bool) -> u64 {
        self.0.part(complex, next)
    }
}

impl ClsGame {
    fn ask(&mut self, op: i32, unit: u64, class: u64) -> i64 {
        let mut io = EeUnitsClsIo::default();
        self.call(op, unit, class, 0, 0, &mut io)
    }
}

impl ConditionHost for ClsGame {
    fn command_type(&mut self, unit: u64) -> i32 {
        self.ask(EE_UNITS_CLS_COMMAND_TYPE, unit, 0) as i32
    }
    fn next_flight_point(&mut self, unit: u64) -> Option<Ge3DPoint> {
        let mut io = EeUnitsClsIo::default();
        (self.call(EE_UNITS_CLS_NEXT_WAYPOINT, unit, 0, 0, 0, &mut io) == 0).then_some(Ge3DPoint {
            x: io.p[0],
            y: io.p[1],
            z: io.p[2],
        })
    }
    fn position(&mut self, unit: u64) -> Ge3DPoint {
        let p = self.0.position(unit);
        Ge3DPoint { x: p[0], y: p[1], z: p[2] }
    }
    fn ground_height(&mut self, point: Ge3DPoint) -> f32 {
        // GetGroundHeight's one call (uca_air.rs; the C++ handed its class, which the body does not read)
        let mut height = 0.0f32;
        let mut p = [point.x, point.y, point.z, 0.0];
        uca_call(
            crate::units::uca_air::EE_UNITS_UCA_DO_GROUND_HEIGHT,
            0,
            0,
            std::ptr::addr_of_mut!(height) as u64,
            &mut p,
        );
        height
    }
    fn bomb_on_target(&mut self, class: u64, unit: u64) -> bool {
        self.ask(EE_UNITS_CLS_BOMB_ON_TARGET, unit, class) != 0
    }
    fn away_from_goal(&mut self, class: u64, unit: u64) -> bool {
        self.ask(EE_UNITS_CLS_AWAY_FROM_GOAL, unit, class) != 0
    }
    fn projectile_category(&mut self, class: u64) -> i32 {
        self.ask(EE_UNITS_CLS_PROJECTILE_CATEGORY, 0, class) as i32
    }
    fn attack_target_alive(&mut self, unit: u64) -> bool {
        let goal = self.ask(EE_UNITS_CLS_R_ATTACK_GOAL, unit, 0) as u64;
        goal != 0 && {
            let target = self.ask(EE_UNITS_CLS_R_ATTACK_TARGET_OF, unit, goal) as u64;
            target != 0 && self.ask(EE_UNITS_CLS_R_HIT_POINTS_OF, unit, target) > 0
        }
    }
    fn goal_has_location(&mut self, unit: u64) -> bool {
        self.ask(EE_UNITS_CLS_GOAL_HAS_LOCATION, unit, 0) != 0
    }
    fn request_queued(&mut self, unit: u64) -> bool {
        // the unit in its air traffic control's landing or take-off queue
        let control = self.ask(EE_UNITS_CLS_R_ATC, unit, 0) as usize as *mut crate::units::atc::EeUnitsAtc;
        let plane = unit as usize as *mut c_void;
        // SAFETY: the player's live control and unit
        unsafe {
            crate::units::atc::ee_units_atc_queue_position(control, plane, 1) >= 0
                || crate::units::atc::ee_units_atc_queue_position(control, plane, 0) >= 0
        }
    }
}

/// The game host and a class object's record.
fn game(class: u64) -> (ClsGame, Cls) {
    let mut h = ClsGame(Game);
    let fields = TrHost::class(&mut h, class);
    (h, Cls(fields))
}

/// A `U2DPoint<long>`.
///
/// # Safety
/// `p` points to two `long`s.
unsafe fn tile(p: *const [c_long; 2]) -> Tile {
    // SAFETY: the caller's contract
    unsafe { [(*p)[0] as i32, (*p)[1] as i32] }
}

/// Writes a `U2DPoint<long>`.
///
/// # Safety
/// `p` points to two writable `long`s.
unsafe fn set_tile(p: *mut [c_long; 2], t: Tile) {
    // SAFETY: the caller's contract
    unsafe { *p = [t[0] as c_long, t[1] as c_long] }
}

/// `EEUnitClass::CanCarry` (`class` the transport's class object).
#[no_mangle]
pub extern "C" fn ee_units_tr_can_carry(class: u64, unit1: u64, unit2: u64) -> bool {
    let (mut h, cls) = game(class);
    tr::can_carry(&mut h, cls, unit1, unit2)
}

/// `EEUnitClass::CanUnloadAt`.
#[no_mangle]
pub extern "C" fn ee_units_tr_can_unload_at(class: u64, transport: u64, x: i32, y: i32) -> bool {
    let (mut h, cls) = game(class);
    tr::can_unload_at(&mut h, cls, transport, x, y)
}

/// `EEUnitClass::CanLoad`.
#[no_mangle]
pub extern "C" fn ee_units_tr_can_load(class: u64, transport: u64, unit: u64) -> bool {
    let (mut h, cls) = game(class);
    tr::can_load(&mut h, cls, transport, unit)
}

/// `EEUnitClass::CanUnloadNear` (`tiles`: the unload, first cargo and destination tiles).
///
/// # Safety
/// `tiles` points to three `U2DPoint<long>`s.
#[no_mangle]
pub unsafe extern "C" fn ee_units_tr_can_unload_near(
    class: u64,
    transport: u64,
    cargo: u64,
    unload: *const [c_long; 2],
    first: *const [c_long; 2],
    destination: *const [c_long; 2],
) -> bool {
    let (mut h, cls) = game(class);
    // SAFETY: the caller's contract
    let (u, f, d) = unsafe { (tile(unload), tile(first), tile(destination)) };
    tr::can_unload_near(&mut h, cls, transport, cargo, u, f, d)
}

/// `EEUnitClass::FindGatheringTile` (`units` a `std::vector<EEComplexUnit*>`).
///
/// # Safety
/// `units` is a live vector; `out` two writable `long`s.
#[no_mangle]
pub unsafe extern "C" fn ee_units_tr_gathering_tile(
    class: u64,
    units: *const c_void,
    x: i32,
    y: i32,
    out: *mut [c_long; 2],
) -> i32 {
    let (mut h, cls) = game(class);
    // SAFETY: the caller's contract
    unsafe {
        let mut t = tile(out);
        let r = tr::find_gathering_tile(&mut h, cls, &units_of(units), [x, y], &mut t);
        set_tile(out, t);
        r
    }
}

/// `EEUnitClass::FindRendezvousTiles`.
///
/// # Safety
/// As [`ee_units_tr_gathering_tile`], two out tiles.
#[no_mangle]
pub unsafe extern "C" fn ee_units_tr_rendezvous_tiles(
    class: u64,
    units: *const c_void,
    transport: u64,
    out_transport: *mut [c_long; 2],
    out_unit: *mut [c_long; 2],
) {
    let (mut h, cls) = game(class);
    // SAFETY: the caller's contract
    unsafe {
        let (mut a, mut b) = (tile(out_transport), tile(out_unit));
        tr::rendezvous_tiles(&mut h, cls, &units_of(units), transport, &mut a, &mut b);
        set_tile(out_transport, a);
        set_tile(out_unit, b);
    }
}

/// `EEUnitClass::FindUnloadTiles`.
///
/// # Safety
/// The destination and three writable out tiles.
#[no_mangle]
pub unsafe extern "C" fn ee_units_tr_unload_tiles(
    class: u64,
    transport: u64,
    destination: *const [c_long; 2],
    out_unload: *mut [c_long; 2],
    out_first: *mut [c_long; 2],
    out_tile: *mut [c_long; 2],
) {
    let (mut h, cls) = game(class);
    // SAFETY: the caller's contract
    unsafe {
        let mut out = [tile(out_unload), tile(out_first), tile(out_tile)];
        if tr::unload_tiles(&mut h, cls, transport, tile(destination), &mut out) {
            set_tile(out_unload, out[0]);
            set_tile(out_first, out[1]);
            set_tile(out_tile, out[2]);
        }
    }
}

/// `EEUnitClass::FilterLoadable` (`units` and `rejected` `std::vector<EEComplexUnit*>`s).
///
/// # Safety
/// Two live vectors.
#[no_mangle]
pub unsafe extern "C" fn ee_units_tr_filter_loadable(
    class: u64,
    units: *mut c_void,
    transport: u64,
    rejected: *mut c_void,
) {
    let (mut h, cls) = game(class);
    // SAFETY: the caller's contract
    unsafe {
        let mut list = units_of(units);
        let mut out = units_of(rejected);
        let before = out.len();
        tr::filter_loadable(&mut h, cls, &mut list, transport, &mut out);
        if out.len() != before {
            set_units(units, &list);
            set_units(rejected, &out);
        }
    }
}

/// `EEUnitClass::HasArrived` of the naval transport.
#[no_mangle]
pub extern "C" fn ee_units_tr_naval_has_arrived(unit: u64, value: f32) -> bool {
    let mut h = ClsGame(Game);
    tr::naval_has_arrived(&mut h, unit, value)
}

/// `EEUnitClass::GetHeightAt` (`class` the unit's class object).
#[no_mangle]
pub extern "C" fn ee_units_cls_height_at(class: u64, unit: u64, x: f32, y: f32, z: f32) -> f32 {
    let mut h = ClsGame(Game);
    let fields = TrHost::class(&mut h, class);
    air::height_at(&mut h, fields, unit, Ge3DPoint { x, y, z })
}

/// `EEUnitClass::GetNextFlightState`.
#[no_mangle]
pub extern "C" fn ee_units_cls_next_flight_state(unit: u64) -> u32 {
    air::next_flight_state(&mut ClsGame(Game), unit)
}

/// `EEUnitClass::ApproachRunway` (`class` the plane's class object).
#[no_mangle]
pub extern "C" fn ee_units_cls_approach_runway(class: u64, unit: u64, dx: i32, dy: i32) {
    let (mut h, cls) = game(class);
    air::approach_runway(&mut h, class, cls.0, unit, dx, dy);
}

/// `EEUnitClass::TakeOffFromRunway`.
#[no_mangle]
pub extern "C" fn ee_units_cls_take_off_from_runway(class: u64, unit: u64, dx: i32, dy: i32) {
    let (mut h, cls) = game(class);
    air::take_off_from_runway(&mut h, class, cls.0, unit, dx, dy);
}

/// A home base's facing as the runway's direction (`out` x, y).
///
/// # Safety
/// `out` points to two writable `long`s.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_runway_direction(facing: u32, out: *mut [c_long; 2]) {
    let (x, y) = air::runway_direction(facing);
    // SAFETY: the caller's contract
    unsafe { set_tile(out, [x, y]) }
}

/// `EEGetFacingQuarter`.
#[no_mangle]
pub extern "C" fn ee_units_cls_facing_quarter(x: i32, y: i32) -> i32 {
    bld::facing_quarter(&mut ClsGame(Game), x, y)
}

/// A navy yard's facing at a tile (`GetNavyYardFacing`).
#[no_mangle]
pub extern "C" fn ee_units_cls_navy_yard_facing(x: u32, y: u32) -> i32 {
    bld::navy_yard_facing(&mut ClsGame(Game), x, y)
}

/// `EEGetDockFacing`.
#[no_mangle]
pub extern "C" fn ee_units_cls_dock_facing(x: u32, y: u32) -> i32 {
    bld::dock_facing(&mut ClsGame(Game), x, y)
}

/// `EETurnGatePart`'s answer: the piece's animation, and the yaw it turns to in `yaw` (`turned` false: kept).
///
/// # Safety
/// `yaw` and `turned` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_gate_part_turn(
    direction: i32,
    index: i32,
    yaw: *mut f32,
    turned: *mut bool,
) -> i32 {
    let (action, turn) = bld::gate_part_turn(direction, index);
    // SAFETY: the caller's contract
    unsafe {
        *turned = turn.is_some();
        *yaw = turn.unwrap_or(0.0);
    }
    action
}

/// `EEGetWallGateTiles` (appended to `out`, a `std::vector<U2DPoint<long> >`).
///
/// # Safety
/// `out` is a live vector.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_wall_gate_tiles(x: i32, y: i32, direction: i32, out: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(out);
        tiles.extend(bld::wall_gate_tiles([x, y], direction));
        set_tiles(out, &tiles);
    }
}

/// `EEAreTilesLevel` (`tiles` a `std::vector<U2DPoint<long> >`).
///
/// # Safety
/// `tiles` is a live vector.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_tiles_level(tiles: *const c_void) -> bool {
    // SAFETY: the caller's contract
    let list = unsafe { tiles_of(tiles) };
    bld::tiles_level(&mut ClsGame(Game), &list)
}

/// The gate's `AreEnemiesNear` (`passers` false) or `IsPasserNear`.
#[no_mangle]
pub extern "C" fn ee_units_cls_gate_near(unit: u64, passers: bool) -> bool {
    let mut h = ClsGame(Game);
    if passers {
        bld::passer_near(&mut h, unit)
    } else {
        bld::enemies_near(&mut h, unit)
    }
}

/// `EEUnitClass::HasArrived` of the landing kinds (`animal`: the air animal's).
#[no_mangle]
pub extern "C" fn ee_units_cls_landing_has_arrived(class: u64, unit: u64, value: f32, animal: bool) -> bool {
    let (mut h, cls) = game(class);
    air::landing_has_arrived(&mut h, cls.0, unit, value, animal)
}

/// `EEUnitClass::EvaluateCondition` of the landing kinds (`animal`: the air animal's): 0 or 1, -1 for a trigger the
/// common unit class answers.
#[no_mangle]
pub extern "C" fn ee_units_cls_landing_condition(class: u64, unit: u64, event: u32, animal: bool) -> i32 {
    let (mut h, cls) = game(class);
    let answer = if animal {
        air::air_animal_condition(&mut h, unit, event)
    } else {
        air::landable_condition(&mut h, class, cls.0, unit, event)
    };
    answer.unwrap_or(-1)
}

// ---- the trigger answers (cls_condition.rs) over `ee_units_cls_cond_host` (EEUnitAI.cpp)
use crate::units::cls_condition::{self as cond, Answer, CombatKind, CondHost, EeUnitsCondIo};

extern "C" {
    fn ee_units_cls_cond_host(op: i32, a: u64, b: u64, c: i64, d: i64, io: *mut EeUnitsCondIo) -> i64;
}

/// The trigger host's plain reads of the unit `a` over the world views (the units' behaviour lane, round 2; they
/// were EEUnitAI.cpp `ee_units_cls_cond_host`'s cases): its goals, last attacker, line of sight, cargo count and
/// position; None for the C++ host's.
fn cond_view(op: i32, a: u64, b: u64, c: i64, io: &mut EeUnitsCondIo) -> Option<i64> {
    use crate::units::cls_condition::{
        EE_UNITS_COND_CARGO, EE_UNITS_COND_CRUSADER, EE_UNITS_COND_DISTANCE_TO, EE_UNITS_COND_GOAL,
        EE_UNITS_COND_INITIAL_CONTACT, EE_UNITS_COND_LAST_ATTACKER, EE_UNITS_COND_LINE_OF_SIGHT,
        EE_UNITS_COND_MINE_DEPLETED, EE_UNITS_COND_POSITION, EE_UNITS_COND_TILE,
    };
    /// `IsCrusader()`'s unit types (the four crusaders)
    const CRUSADER_TYPES: [u64; 4] = [0x2e6, 0x2e7, 0x2e8, 0x2e9];
    if !matches!(
        op,
        EE_UNITS_COND_GOAL
            | EE_UNITS_COND_LAST_ATTACKER
            | EE_UNITS_COND_LINE_OF_SIGHT
            | EE_UNITS_COND_CARGO
            | EE_UNITS_COND_POSITION
            | EE_UNITS_COND_DISTANCE_TO
            | EE_UNITS_COND_INITIAL_CONTACT
            | EE_UNITS_COND_TILE
            | EE_UNITS_COND_CRUSADER
            | EE_UNITS_COND_MINE_DEPLETED
    ) {
        return None;
    }
    crate::units::unit_geom::with_unit(a, |u| {
        if !u.is_complex() {
            return None;
        }
        Some(match op {
            EE_UNITS_COND_GOAL => (if c != 0 { u.goal_handle() } else { u.active_goal_handle() }) as i64,
            EE_UNITS_COND_LAST_ATTACKER => u.last_attacker_handle() as i64,
            EE_UNITS_COND_LINE_OF_SIGHT => {
                io.f = crate::units::unit_geom::line_of_sight(u);
                0
            }
            EE_UNITS_COND_CARGO => u.cargo_len() as i64,
            EE_UNITS_COND_DISTANCE_TO => {
                // DistanceSquaredTo(other complex unit)
                io.f = crate::units::unit_geom::with_unit(b, |o| crate::units::unit_geom::distance_squared_to(u, o))?;
                0
            }
            EE_UNITS_COND_INITIAL_CONTACT => {
                io.p = u.contact_location();
                0
            }
            EE_UNITS_COND_TILE => {
                // mTile->GetHeight() out, mTile->mTerrainType
                let tile = u.tile()?;
                io.f = tile.height();
                tile.terrain_type()
            }
            EE_UNITS_COND_CRUSADER => i64::from(CRUSADER_TYPES.contains(&u.class()?.unit_type())),
            EE_UNITS_COND_MINE_DEPLETED => {
                // mMine == NULL || !mMine->mIsValid || mMine->mResourceAmount <= 0
                let mine = u.mine_handle();
                i64::from(mine == 0 || u.unit_at(mine).is_none_or(|m| !m.valid() || m.resource_amount() <= 0))
            }
            _ => {
                io.p = crate::units::unit_geom::position(u);
                0
            }
        })
    })?
}

/// The trigger host's goal answers (the units' behaviour lane, round 3: the goals and their queues are Rust objects,
/// goal_obj.rs and goals.rs; they were EEUnitAI.cpp `ee_units_cls_cond_host`'s cases); None for the C++ host's.
fn cond_goal(op: i32, a: u64, b: u64, c: i64) -> Option<i64> {
    use crate::units::cls_condition::{
        EE_UNITS_COND_GOAL_CONDITION, EE_UNITS_COND_GOAL_ELAPSED, EE_UNITS_COND_GOAL_INFO, EE_UNITS_COND_GOAL_TARGET,
        EE_UNITS_COND_LOCATION_QUEUED, EE_UNITS_COND_QUEUED_GOAL, EE_UNITS_COND_SECONDARY_TARGET,
    };
    use crate::units::goal_obj;
    use crate::units::location::{self, GOAL_LOCATION};
    use std::os::raw::c_ulong;
    // a location goal's forced-move flag (false for another kind)
    let forced = |goal: u64| {
        // SAFETY: a live goal of the unit
        (unsafe { goal_obj::base(goal).kind == GOAL_LOCATION }) && location::get(goal).forced_move
    };
    Some(match op {
        EE_UNITS_COND_GOAL_INFO => {
            // SAFETY: a live goal
            let g = unsafe { *goal_obj::base(b) };
            i64::from(g.kind) | if g.b1c { 0x100 } else { 0 } | if forced(b) { 0x200 } else { 0 }
        }
        EE_UNITS_COND_GOAL_CONDITION => {
            let event = c as c_ulong as u64;
            // SAFETY: as above
            i64::from(
                unsafe {
                    goal_obj::call(crate::units::rust_goal::EE_UNITS_GOAL_EVALUATE, b, a, event, std::ptr::null_mut())
                } != 0,
            )
        }
        EE_UNITS_COND_GOAL_ELAPSED => {
            // (unsigned long)(gPhysics->mGameTime - GetStartTime()) >= (unsigned long)inMs
            // SAFETY: as above; the caller holds the world
            let start = unsafe { goal_obj::base(b).start_time };
            let t = unsafe { crate::world::WorldRead::begin() };
            let now = crate::world::WorldView::global(&t).map_or(0, |w| w.game_time()) as std::os::raw::c_long;
            i64::from((now as c_ulong).wrapping_sub(start) >= c as c_ulong)
        }
        EE_UNITS_COND_GOAL_TARGET => crate::combat::goal_attack::attack_of(b).target as i64,
        EE_UNITS_COND_LOCATION_QUEUED => {
            // EEIsLocationGoalQueued: a move goal on the first queue that is no forced move
            let queues = crate::units::goals::unit_queues(a);
            // SAFETY: the unit's live main queue (the C++ read it unchecked)
            let goals = queues.first().map(|q| unsafe { (**q).goals.clone() }).unwrap_or_default();
            i64::from(
                goals.iter().any(|g| *g != 0 && unsafe { goal_obj::base(*g).kind } == GOAL_LOCATION && !forced(*g)),
            )
        }
        EE_UNITS_COND_QUEUED_GOAL => i64::from(crate::units::goals::queued_goal(a, c as c_ulong as usize) != 0),
        EE_UNITS_COND_SECONDARY_TARGET => {
            // the current goal attacks a living unit
            let goal = crate::units::unit_geom::with_unit(a, |u| u.goal_handle()).unwrap_or(0);
            // SAFETY: the unit's live goal
            if goal == 0 || unsafe { goal_obj::base(goal).kind } != crate::combat::goal_dispatch::GOAL_ATTACK_UNIT {
                return Some(0);
            }
            let target = crate::combat::goal_attack::attack_of(goal).target;
            i64::from(crate::units::unit_geom::with_unit(target, |u| u.hit_points()).unwrap_or(0) > 0)
        }
        _ => return None,
    })
}

impl crate::units::uai_cond::UaiHost for ClsGame {
    fn uai(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsCondIo) -> i64 {
        // SAFETY: the C++ statement reads live objects the bodies hand it
        unsafe { ee_units_cls_cond_host(op, a, b, c, d, io) }
    }
    fn uai_location(&mut self, unit: u64) -> [f32; 3] {
        CondHost::location(self, unit)
    }
}

impl CondHost for ClsGame {
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        self.0.unit(obj)
    }
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields {
        self.0.class(class).fields
    }
    fn location(&mut self, unit: u64) -> [f32; 3] {
        let m = self.0.location(unit);
        [m.m[0][3], m.m[1][3], m.m[2][3]]
    }
    fn op(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsCondIo) -> i64 {
        if let Some(v) = cond_view(op, a, b, c, io) {
            return v;
        }
        // the unit's path (the paths lane's round 1: waypoints.rs)
        // SAFETY (each): the trigger's live complex unit `a`
        match op {
            cond::EE_UNITS_COND_AUTO_WAYPOINT => return i64::from(unsafe { crate::units::waypoints::next_auto(a) }),
            cond::EE_UNITS_COND_WAYPOINT_STEP => return unsafe { crate::units::waypoints::waypoint_step(a, c) },
            cond::EE_UNITS_COND_WAYPOINTS => return unsafe { crate::units::waypoints::list(a) }.len() as i64,
            _ => {}
        }
        if let Some(v) = cond_goal(op, a, b, c) {
            return v;
        }
        // the compound answers (uai_cond.rs over the C++ statements)
        if let Some(v) = crate::units::uai_cond::compound(self, op, a, b, c, io) {
            return v;
        }
        // SAFETY: the C++ switch reads live objects the bodies hand it
        unsafe { ee_units_cls_cond_host(op, a, b, c, d, io) }
    }
    fn landing(&mut self, class: u64, unit: u64, event: u32, animal: bool) -> Option<i32> {
        if animal {
            air::air_animal_condition(self, unit, event)
        } else {
            let fields = TrHost::class(self, class);
            air::landable_condition(self, class, fields, unit, event)
        }
    }
}

/// `EEUnitClass::EvaluateCondition`: the answer (0, 1, 2), or a shot's kind for ee_combat's answer (-1 the torpedo,
/// -2 the missile, -3 the projectile).
#[no_mangle]
pub extern "C" fn ee_units_cls_condition(class: u64, unit: u64, event: u32) -> i32 {
    match cond::condition(&mut ClsGame(Game), class, unit, event) {
        Answer::Value(v) => v,
        Answer::Combat(CombatKind::Torpedo) => -1,
        Answer::Combat(CombatKind::Missile) => -2,
        Answer::Combat(CombatKind::Projectile) => -3,
    }
}

/// `EEUnitClass::BaseEvaluateCondition` (the common answer of the class `class`).
#[no_mangle]
pub extern "C" fn ee_units_cls_base_condition(class: u64, unit: u64, event: u32) -> i32 {
    let mut h = ClsGame(Game);
    let fields = CondHost::class(&mut h, class);
    cond::base(&mut h, class, fields, unit, event)
}
