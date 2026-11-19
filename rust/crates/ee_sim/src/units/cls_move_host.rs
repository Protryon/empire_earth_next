//! The movement bodies' host in the game ([`MoveHost`] over the unit utilities' world reads and the class bodies'
//! C++ switch, `ee_units_cls_host` in EEUnitClassTransports.cpp) and the entry points the `EEUnitClass` members
//! call (cls_move.rs). Absent from a test binary.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 on LP64: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use ee_base::math::types::{Ge3DPoint, GeTransformation};

use crate::units::cls_host::{ClsGame, EeUnitsClsIo, EeUnitsClsListHandle};
use crate::units::cls_move::{self as mv, MoveHost};
use crate::units::unit_class_fields::EeUnitsUnitClassFields;
use crate::units::uu::{EeUnitsUuUnit, UuHost};
use crate::units::uu_ffi;
use crate::units::uu_host::Game;
use crate::units::uu_note::Note;
use crate::units::uu_tiles::Tile;
use crate::units::uu_world;

// ---- the host operations (`ee_units_cls_host`'s `op`, after cls_host.rs's)
/// `gWorld.mMap->GetClosestTile(io.p, ...)`'s `mHeight` into `io.f` (0: no tile)
pub const EE_UNITS_CLS_MV_CLOSEST_TILE_HEIGHT: i32 = 39;
/// `EEGetPursuerOf(unit a)`
pub const EE_UNITS_CLS_MV_PURSUER: i32 = 40;
/// `EEGetDirectionTo(a, b)`
pub const EE_UNITS_CLS_MV_DIRECTION_TO: i32 = 41;
/// unit `a`'s `GetLineOfSight()` into `io.f`
pub const EE_UNITS_CLS_MV_LINE_OF_SIGHT: i32 = 42;
/// unit `a`'s `GetMaxVelocity()` into `io.p[0]` (c 0) or `GetMaxAngularVelocity()` (c 1)
pub const EE_UNITS_CLS_MV_MAX_VELOCITY: i32 = 43;
/// `gPhysics->GetRandom(1L, 2L)`
pub const EE_UNITS_CLS_MV_RANDOM_1_2: i32 = 44;
/// unit `a`'s `mGoalState` (c 0), `mActiveGoal` (1), `mBlockingUnit` (2), `mLastAttacker` (3)
pub const EE_UNITS_CLS_MV_GOAL: i32 = 45;
/// unit `a`'s `mLastAttacker` dereferenced and cleared
pub const EE_UNITS_CLS_MV_CLEAR_LAST_ATTACKER: i32 = 46;
/// goal `b`'s `mKind`
pub const EE_UNITS_CLS_MV_GOAL_KIND: i32 = 47;
/// goal `b`'s `GetGoalLocation` (c 0) or `GetDestination(unit a)` (1) of `io.p` (both read it as an input) into `io.p`
pub const EE_UNITS_CLS_MV_GOAL_LOCATION: i32 = 48;
/// goal `b`'s `mB1C` (c 0), `mB1D` (1), `GetIsAutoWaypoint()` (2), `GetGroupPathState()` (3), a gather goal on wood (4)
pub const EE_UNITS_CLS_MV_GOAL_FLAG: i32 = 49;
/// goal `b`'s `mPathAttempts += c`
pub const EE_UNITS_CLS_MV_PATH_ATTEMPTS: i32 = 50;
/// goal `b`'s `GetSidestepCount()` (c -1), else `SetSidestepCount(c)`
pub const EE_UNITS_CLS_MV_SIDESTEP: i32 = 51;
/// goal `b`'s `SetGroupPathState(c)`
pub const EE_UNITS_CLS_MV_SET_GROUP_PATH_STATE: i32 = 52;
/// goal `b`'s `GetAttackTarget()->GetComplexUnit()`
pub const EE_UNITS_CLS_MV_ATTACK_TARGET: i32 = 53;
/// unit `a`'s `IgnoreTarget(b)`
pub const EE_UNITS_CLS_MV_IGNORE_TARGET: i32 = 54;
/// `EEFindNewTarget` over `EEFOFindNewTargetInMeleeRange(a, b)` with attack goal `c` from `a`'s tile
pub const EE_UNITS_CLS_MV_MELEE_RETARGET: i32 = 55;
/// unit `a`'s `SetBlockingUnit(b)`
pub const EE_UNITS_CLS_MV_SET_BLOCKING: i32 = 56;
/// unit `a`'s `AddWaypoint(io.p, c != 0)`
pub const EE_UNITS_CLS_MV_ADD_WAYPOINT: i32 = 57;
/// unit `a`'s `mWaypoints[c].mLocation` into `io.p`
pub const EE_UNITS_CLS_MV_WAYPOINT: i32 = 58;
/// unit `a`'s `GetSecondWaypoint(io.p)` (its answer: 0 when there is one)
pub const EE_UNITS_CLS_MV_SECOND_WAYPOINT: i32 = 59;
/// unit `a`'s `NextWaypoint()`
pub const EE_UNITS_CLS_MV_NEXT_WAYPOINT: i32 = 60;
/// unit `a`'s `DistanceSquared3D(io.p)` into `io.f`
pub const EE_UNITS_CLS_MV_DISTANCE_3D: i32 = 61;
/// unit `a`'s `mBoundedPathTiles.clear()` (c 0) or `push_back((io.a, io.b))` (1)
pub const EE_UNITS_CLS_MV_BOUNDED: i32 = 62;
/// `gPathManager.FindBoundedPath(a, map, c, a's tile, (io.a, io.b), path, 50, true, &reached)`: the path pushed
pub const EE_UNITS_CLS_MV_FIND_BOUNDED_PATH: i32 = 63;
/// `EEPathManager::MergeWaypoints(the list's points, a's mWaypoints, 0)`: the list replaced
pub const EE_UNITS_CLS_MV_MERGE_WAYPOINTS: i32 = 64;
/// unit `a`'s `SetVelocity(io.f)`
pub const EE_UNITS_CLS_MV_SET_VELOCITY: i32 = 65;
/// unit `a`'s `StartAction(c)`
pub const EE_UNITS_CLS_MV_START_ACTION: i32 = 66;
/// class `b`'s `ActionChanged(unit a)`
pub const EE_UNITS_CLS_MV_ACTION_CHANGED: i32 = 67;
/// unit `a`'s `UpdateWaterState(true)`
pub const EE_UNITS_CLS_MV_UPDATE_WATER_STATE: i32 = 68;
/// `gPathManager.CancelPaths(a, true)`
pub const EE_UNITS_CLS_MV_CANCEL_PATHS: i32 = 69;
/// unit `a`'s `mInitialContactLocation = io.p` (when it differs)
pub const EE_UNITS_CLS_MV_INITIAL_CONTACT: i32 = 70;
/// `PathToGoal`'s request for unit `a`, goal `b` to `(io.a, io.b)`: revisions `c`, graph `d & 1`, arg8 `d & 2`
pub const EE_UNITS_CLS_MV_REQUEST_PATH: i32 = 71;
/// unit `a`'s flight goal to `(io.p[0], io.p[1])`, keeping the current goal when `c`
pub const EE_UNITS_CLS_MV_FLEE_GOAL: i32 = 72;

/// Pushes a point onto a host operation's list.
///
/// # Safety
/// `list` is the `EeUnitsClsIo::list` of a running operation.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_push_point(list: *mut EeUnitsClsListHandle, x: f32, y: f32, z: f32) {
    // SAFETY: the caller's contract
    unsafe { (*list).points.push(Ge3DPoint { x, y, z }) }
}

/// A host operation's points (x, y, z each), `*count` of them.
///
/// # Safety
/// `list` is the `EeUnitsClsIo::list` of a running operation; `count` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_points(list: *mut EeUnitsClsListHandle, count: *mut usize) -> *const f32 {
    // SAFETY: the caller's contract
    unsafe {
        *count = (*list).points.len();
        (*list).points.as_ptr().cast()
    }
}

/// Empties a host operation's points (before the C++ pushes their replacement).
///
/// # Safety
/// As [`ee_units_cls_points`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_clear_points(list: *mut EeUnitsClsListHandle) {
    // SAFETY: the caller's contract
    unsafe { (*list).points.clear() }
}

impl ClsGame {
    fn mv(&mut self, op: i32, a: u64, b: u64, c: i64) -> (i64, EeUnitsClsIo) {
        let mut io = EeUnitsClsIo::default();
        let r = self.call(op, a, b, c, 0, &mut io);
        (r, io)
    }
    fn mv_p(&mut self, op: i32, a: u64, b: u64, c: i64, p: Ge3DPoint) -> (i64, EeUnitsClsIo) {
        let mut io = EeUnitsClsIo { p: [p.x, p.y, p.z], ..Default::default() };
        let r = self.call(op, a, b, c, 0, &mut io);
        (r, io)
    }
}

fn point(p: [f32; 3]) -> Ge3DPoint {
    Ge3DPoint { x: p[0], y: p[1], z: p[2] }
}

impl MoveHost for ClsGame {
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        self.0.unit(obj)
    }
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields {
        self.0.class(class).fields
    }
    fn position(&mut self, obj: u64) -> Ge3DPoint {
        point(self.0.position(obj))
    }
    fn location(&mut self, unit: u64) -> GeTransformation {
        self.0.location(unit)
    }
    fn map_tile(&mut self, x: i32, y: i32) -> u64 {
        let map = self.0.map();
        self.0.map_tile(map, x, y)
    }
    fn tile_height(&mut self, tile: u64) -> f32 {
        self.0.tile_height(tile)
    }
    fn tile_units(&mut self, tile: u64) -> Vec<u64> {
        let mut out = Vec::new();
        if tile != 0 {
            self.0.tile_units(tile, &mut out);
        }
        out
    }
    fn closest_tile_height(&mut self, p: Ge3DPoint) -> Option<f32> {
        let (r, io) = self.mv_p(EE_UNITS_CLS_MV_CLOSEST_TILE_HEIGHT, 0, 0, 0, p);
        (r != 0).then_some(io.f)
    }
    fn passable_by(&mut self, obj: u64, by: u64) -> bool {
        self.0.passable_by(obj, by)
    }
    fn can_stand_on(&mut self, tile: Tile, unit: u64) -> bool {
        uu_ffi::ee_units_uu_can_stand_on(tile[0], tile[1], unit) != 0
    }
    fn tile_top_above(&mut self, x: i32, y: i32) -> bool {
        let mut wall = 0u64;
        // SAFETY: `wall` is writable
        unsafe { uu_ffi::ee_units_uu_tile_top_above(x, y, 0.0, 1, &mut wall) != 0 }
    }
    fn unit_tiles(&mut self, unit: u64, margin: f32) -> Vec<Tile> {
        // `EEGetUnitTiles` (53) on an empty vector, noted as its entry point notes it
        let n = Note::begin(53);
        let mut tiles = Vec::new();
        n.u(unit).f(margin).t(&tiles);
        uu_world::unit_tiles(&mut Game, unit, &mut tiles, margin);
        n.t(&tiles);
        tiles
    }
    fn tile_line(&mut self, from: Tile, to: Tile, ends: bool, thin: bool) -> Vec<Tile> {
        self.0.tile_line(from[0], from[1], to[0], to[1], ends, thin)
    }
    fn move_in_direction(&mut self, x: f32, y: f32, direction: i32, distance: i32) -> [f32; 2] {
        let mut out = [0.0f32; 2];
        // SAFETY: two writable floats
        unsafe { uu_ffi::ee_units_uu_move_in_direction(x, y, direction, distance, &mut out) };
        out
    }
    fn turn_away(&mut self, direction: i32, spread: bool) -> i32 {
        uu_ffi::ee_units_uu_turn_away(direction, i32::from(spread))
    }
    fn pursuer_of(&mut self, unit: u64) -> u64 {
        self.mv(EE_UNITS_CLS_MV_PURSUER, unit, 0, 0).0 as u64
    }
    fn direction_to(&mut self, from: u64, to: u64) -> i32 {
        self.mv(EE_UNITS_CLS_MV_DIRECTION_TO, from, to, 0).0 as i32
    }
    fn line_of_sight(&mut self, unit: u64) -> f32 {
        self.mv(EE_UNITS_CLS_MV_LINE_OF_SIGHT, unit, 0, 0).1.f
    }
    fn max_velocity(&mut self, unit: u64) -> f32 {
        self.mv(EE_UNITS_CLS_MV_MAX_VELOCITY, unit, 0, 0).1.p[0]
    }
    fn max_angular_velocity(&mut self, unit: u64) -> f32 {
        self.mv(EE_UNITS_CLS_MV_MAX_VELOCITY, unit, 0, 1).1.p[0]
    }
    fn random_1_2(&mut self) -> i32 {
        self.mv(EE_UNITS_CLS_MV_RANDOM_1_2, 0, 0, 0).0 as i32
    }
    fn goal(&mut self, unit: u64) -> u64 {
        self.mv(EE_UNITS_CLS_MV_GOAL, unit, 0, 0).0 as u64
    }
    fn active_goal(&mut self, unit: u64) -> u64 {
        self.mv(EE_UNITS_CLS_MV_GOAL, unit, 0, 1).0 as u64
    }
    fn blocking_unit(&mut self, unit: u64) -> u64 {
        self.mv(EE_UNITS_CLS_MV_GOAL, unit, 0, 2).0 as u64
    }
    fn last_attacker(&mut self, unit: u64) -> u64 {
        self.mv(EE_UNITS_CLS_MV_GOAL, unit, 0, 3).0 as u64
    }
    fn clear_last_attacker(&mut self, unit: u64) {
        self.mv(EE_UNITS_CLS_MV_CLEAR_LAST_ATTACKER, unit, 0, 0);
    }
    fn goal_kind(&mut self, goal: u64) -> i32 {
        self.mv(EE_UNITS_CLS_MV_GOAL_KIND, 0, goal, 0).0 as i32
    }
    fn goal_location(&mut self, goal: u64) -> Ge3DPoint {
        point(self.mv(EE_UNITS_CLS_MV_GOAL_LOCATION, 0, goal, 0).1.p)
    }
    fn goal_destination(&mut self, goal: u64, unit: u64, from: Ge3DPoint) -> Ge3DPoint {
        point(self.mv_p(EE_UNITS_CLS_MV_GOAL_LOCATION, unit, goal, 1, from).1.p)
    }
    fn goal_flag(&mut self, goal: u64, b1d: bool) -> bool {
        self.mv(EE_UNITS_CLS_MV_GOAL_FLAG, 0, goal, i64::from(b1d)).0 != 0
    }
    fn add_path_attempts(&mut self, goal: u64, n: i32) {
        self.mv(EE_UNITS_CLS_MV_PATH_ATTEMPTS, 0, goal, i64::from(n));
    }
    fn sidestep_count(&mut self, goal: u64) -> u32 {
        self.mv(EE_UNITS_CLS_MV_SIDESTEP, 0, goal, -1).0 as u32
    }
    fn set_sidestep_count(&mut self, goal: u64, n: u32) {
        self.mv(EE_UNITS_CLS_MV_SIDESTEP, 0, goal, i64::from(n));
    }
    fn attack_target(&mut self, goal: u64) -> u64 {
        self.mv(EE_UNITS_CLS_MV_ATTACK_TARGET, 0, goal, 0).0 as u64
    }
    fn auto_waypoint(&mut self, goal: u64) -> bool {
        self.mv(EE_UNITS_CLS_MV_GOAL_FLAG, 0, goal, 2).0 != 0
    }
    fn group_path_state(&mut self, goal: u64) -> u8 {
        self.mv(EE_UNITS_CLS_MV_GOAL_FLAG, 0, goal, 3).0 as u8
    }
    fn set_group_path_state(&mut self, goal: u64, state: u8) {
        self.mv(EE_UNITS_CLS_MV_SET_GROUP_PATH_STATE, 0, goal, i64::from(state));
    }
    fn gathers_wood(&mut self, goal: u64) -> bool {
        self.mv(EE_UNITS_CLS_MV_GOAL_FLAG, 0, goal, 4).0 != 0
    }
    fn ignore_target(&mut self, unit: u64, target: u64) {
        self.mv(EE_UNITS_CLS_MV_IGNORE_TARGET, unit, target, 0);
    }
    fn melee_retarget(&mut self, unit: u64, target: u64, attack: u64) -> bool {
        self.mv(EE_UNITS_CLS_MV_MELEE_RETARGET, unit, target, attack as i64).0 != 0
    }
    fn set_blocking_unit(&mut self, unit: u64, blocker: u64) {
        self.mv(EE_UNITS_CLS_MV_SET_BLOCKING, unit, blocker, 0);
    }
    fn drop_path(&mut self, unit: u64) {
        self.mv(crate::units::cls_host::EE_UNITS_CLS_DROP_PATH, unit, 0, 0);
    }
    fn add_waypoint(&mut self, unit: u64, p: Ge3DPoint, snap: bool) {
        self.mv_p(EE_UNITS_CLS_MV_ADD_WAYPOINT, unit, 0, i64::from(snap), p);
    }
    fn waypoints(&mut self, unit: u64) -> i32 {
        self.mv(crate::units::cls_host::EE_UNITS_CLS_WAYPOINTS, unit, 0, 0).0 as i32
    }
    fn waypoint(&mut self, unit: u64, index: i32) -> Ge3DPoint {
        point(self.mv(EE_UNITS_CLS_MV_WAYPOINT, unit, 0, i64::from(index)).1.p)
    }
    fn second_waypoint(&mut self, unit: u64) -> Option<Ge3DPoint> {
        let (r, io) = self.mv(EE_UNITS_CLS_MV_SECOND_WAYPOINT, unit, 0, 0);
        (r == 0).then_some(point(io.p))
    }
    fn next_waypoint(&mut self, unit: u64) {
        self.mv(EE_UNITS_CLS_MV_NEXT_WAYPOINT, unit, 0, 0);
    }
    fn next_waypoint_point(&mut self, unit: u64) -> Ge3DPoint {
        point(self.mv(crate::units::cls_host::EE_UNITS_CLS_NEXT_WAYPOINT, unit, 0, 0).1.p)
    }
    fn queue_goals(&mut self, unit: u64) -> i32 {
        self.mv(crate::units::cls_host::EE_UNITS_CLS_QUEUE_GOALS, unit, 0, 0).0 as i32
    }
    fn distance_squared_3d(&mut self, unit: u64, p: Ge3DPoint) -> f32 {
        self.mv_p(EE_UNITS_CLS_MV_DISTANCE_3D, unit, 0, 0, p).1.f
    }
    fn clear_bounded_tiles(&mut self, unit: u64) {
        self.mv(EE_UNITS_CLS_MV_BOUNDED, unit, 0, 0);
    }
    fn push_bounded_tile(&mut self, unit: u64, tile: Tile) {
        let mut io = EeUnitsClsIo { a: tile[0] as i64, b: tile[1] as i64, ..Default::default() };
        self.call(EE_UNITS_CLS_MV_BOUNDED, unit, 0, 1, 0, &mut io);
    }
    fn find_bounded_path(&mut self, unit: u64, kind: i32, to: Tile) -> Vec<Ge3DPoint> {
        let mut handle = EeUnitsClsListHandle::default();
        let mut io = EeUnitsClsIo {
            a: to[0] as i64,
            b: to[1] as i64,
            list: std::ptr::addr_of_mut!(handle).cast(),
            ..Default::default()
        };
        self.call(EE_UNITS_CLS_MV_FIND_BOUNDED_PATH, unit, 0, i64::from(kind), 0, &mut io);
        handle.points
    }
    fn merge_waypoints(&mut self, unit: u64, path: &mut Vec<Ge3DPoint>) {
        let mut handle = EeUnitsClsListHandle { points: std::mem::take(path), ..Default::default() };
        let mut io = EeUnitsClsIo { list: std::ptr::addr_of_mut!(handle).cast(), ..Default::default() };
        self.call(EE_UNITS_CLS_MV_MERGE_WAYPOINTS, unit, 0, 0, 0, &mut io);
        *path = handle.points;
    }
    fn set_velocity(&mut self, unit: u64, v: f32) {
        let mut io = EeUnitsClsIo { f: v, ..Default::default() };
        self.call(EE_UNITS_CLS_MV_SET_VELOCITY, unit, 0, 0, 0, &mut io);
    }
    fn start_action(&mut self, unit: u64, action: i32) {
        self.mv(EE_UNITS_CLS_MV_START_ACTION, unit, 0, i64::from(action));
    }
    fn action_changed(&mut self, class: u64, unit: u64) {
        self.mv(EE_UNITS_CLS_MV_ACTION_CHANGED, unit, class, 0);
    }
    fn update_water_state(&mut self, unit: u64) {
        self.mv(EE_UNITS_CLS_MV_UPDATE_WATER_STATE, unit, 0, 0);
    }
    fn cancel_paths(&mut self, unit: u64) {
        self.mv(EE_UNITS_CLS_MV_CANCEL_PATHS, unit, 0, 0);
    }
    fn set_initial_contact_location(&mut self, unit: u64, p: Ge3DPoint) {
        self.mv_p(EE_UNITS_CLS_MV_INITIAL_CONTACT, unit, 0, 0, p);
    }
    fn request_path(&mut self, unit: u64, goal: u64, to: Tile, revs: i32, graph: bool, arg8: bool) {
        let mut io = EeUnitsClsIo { a: to[0] as i64, b: to[1] as i64, ..Default::default() };
        let flags = i64::from(graph) | (i64::from(arg8) << 1);
        self.call(EE_UNITS_CLS_MV_REQUEST_PATH, unit, goal, i64::from(revs), flags, &mut io);
    }
    fn flee_goal(&mut self, unit: u64, to: [f32; 2], keep: bool) {
        self.mv_p(EE_UNITS_CLS_MV_FLEE_GOAL, unit, 0, i64::from(keep), Ge3DPoint { x: to[0], y: to[1], z: 0.0 });
    }
}

fn game() -> ClsGame {
    ClsGame(Game)
}

/// `EEUnitClass::FindPath(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_mv_find_path(class: u64, unit: u64) {
    mv::find_path(&mut game(), class, unit);
}

/// `EEUnitClass::NextWaypoint(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_mv_next_waypoint(class: u64, unit: u64) {
    mv::next_waypoint(&mut game(), class, unit);
}

/// `EEUnitClass::BaseHasArrived(unit, value)`.
#[no_mangle]
pub extern "C" fn ee_units_mv_has_arrived(class: u64, unit: u64, value: f32) -> u8 {
    u8::from(mv::base_has_arrived(&mut game(), class, unit, value))
}

/// `EEUnitClass::AddWaypointAt(unit, x, y, z, snap)`.
#[no_mangle]
pub extern "C" fn ee_units_mv_add_waypoint_at(unit: u64, x: f32, y: f32, z: f32, snap: u8) {
    mv::add_waypoint_at(&mut game(), unit, x, y, z, snap != 0);
}

/// `EEUnitClass::PathToward(unit, from, to, snap)` (the points' x, y, z).
#[no_mangle]
#[allow(clippy::too_many_arguments)] // the two points' coordinates
pub extern "C" fn ee_units_mv_path_toward(unit: u64, fx: f32, fy: f32, fz: f32, tx: f32, ty: f32, tz: f32, snap: u8) {
    let (from, to) = (Ge3DPoint { x: fx, y: fy, z: fz }, Ge3DPoint { x: tx, y: ty, z: tz });
    mv::path_toward(&mut game(), unit, from, to, snap != 0);
}

/// `EEUnitClass::PathToGoal(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_mv_path_to_goal(class: u64, unit: u64) {
    mv::path_to_goal(&mut game(), class, unit);
}

/// `EEUnitClass::Flee(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_mv_flee(class: u64, unit: u64) {
    mv::flee(&mut game(), class, unit);
}

/// `EEUnitClass::FleeAttacker(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_mv_flee_attacker(class: u64, unit: u64) {
    mv::flee_attacker(&mut game(), class, unit);
}
