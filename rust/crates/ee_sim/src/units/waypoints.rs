//! A complex unit's path (`EEComplexUnit::mWaypoints`, the paths lane's round 1): the waypoint deque is a Rust
//! object ([`EeUnitsWaypoints`]) the C++ member (`EEWaypointList`, ee/EEUnit.h) owns by its handle, and the bodies
//! over it are here: `AddWaypoint`, `NextWaypoint`, `DropPath`, `GetPathStart`, `GetPathLength`,
//! `GetNextWaypointPoint`, `GetSecondWaypoint`, `SnapLastWaypoint`, `TakePath` with the classes' `AdjustPath` and
//! `EEPathManager::MergeWaypoints`. Rust reaches a unit's list through the world's layout (`complex_waypoints`: the
//! member's first word is the handle).
//!
//! The elements' layout is `EEComplexUnitWaypoint`'s (a `GE3DPoint`, the running distance, the distance at its
//! start): C++ reads them in place (`EEWaypointList::operator[]`, valid until the list changes).

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::collections::VecDeque;

use ee_base::math::um;

/// One waypoint (`EEComplexUnitWaypoint`, 20 bytes).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsWaypoint {
    /// `mLocation`
    pub location: [f32; 3],
    /// `mDistance`: the path's length up to this waypoint
    pub distance: f32,
    /// `mStartDistance`: the path's length up to the waypoint before
    pub start_distance: f32,
}

/// The deque (`std::deque<EEComplexUnitWaypoint>` until this round).
#[derive(Clone, Debug, Default)]
pub struct EeUnitsWaypoints {
    list: VecDeque<EeUnitsWaypoint>,
}

/// `GetPathStart`'s and the others' "no path" answer.
pub const EE_UNITS_WAYPOINTS_NONE: i32 = 7;

// ---- `ee_units_wp_host`'s operations (EEComplexUnit.cpp: the C++ calls of the path bodies, one statement each;
// `unit` the unit, `arg` a goal or a value)
/// `SetGoal(arg, 1, 0)`, `mGoalQueues[0].Insert(1, arg)`, `RefreshGoals()`, `RefreshActiveGoal()`
pub const EE_UNITS_WP_SET_GOAL: i32 = 0;
pub const EE_UNITS_WP_INSERT_GOAL: i32 = 1;
pub const EE_UNITS_WP_REFRESH_GOALS: i32 = 2;
pub const EE_UNITS_WP_REFRESH_ACTIVE_GOAL: i32 = 3;
/// `StartAction(arg)`, the class's `ActionChanged(unit)`
pub const EE_UNITS_WP_START_ACTION: i32 = 4;
pub const EE_UNITS_WP_ACTION_CHANGED: i32 = 5;
/// the first goal queue's length and last goal
pub const EE_UNITS_WP_QUEUE_LENGTH: i32 = 6;
pub const EE_UNITS_WP_QUEUE_BACK: i32 = 7;
/// the player's `IsHuman()`; `mBoundedPathTiles.clear()`; `gEEAIRegionSize`
pub const EE_UNITS_WP_IS_HUMAN: i32 = 8;
pub const EE_UNITS_WP_CLEAR_BOUNDED: i32 = 9;
// (10 was RefreshPath: refresh_path)
pub const EE_UNITS_WP_REGION_SIZE: i32 = 11;
/// `EELocationGoalPoint(arg)` into the point; the world map's closest tile to the point (x | y << 32)
pub const EE_UNITS_WP_GOAL_POINT: i32 = 12;
pub const EE_UNITS_WP_CLOSEST_TILE: i32 = 13;

/// `kGoalHealUnit`; `kMovementSea`; `kPathSpeedScale` (0x446dbc)
#[cfg_attr(test, allow(dead_code))]
const GOAL_HEAL_UNIT: u8 = 15;
#[cfg_attr(test, allow(dead_code))]
const MOVEMENT_SEA: u8 = 4;
const PATH_SPEED_SCALE: f32 = 5.0;

/// `UMCeil`: rounds to nearest, then up when that went down.
pub fn um_ceil(v: f32) -> i32 {
    let r = um::lrintf(v);
    if (r as f32) < v {
        r.wrapping_add(1)
    } else {
        r
    }
}

/// `ShouldMoveToNextAutoWaypoint`'s waypoint limit for a far path: two region sizes, a sea unit's one (scaled by
/// its formation rank when it has one).
pub fn auto_waypoint_limit(sea: bool, region_size: i32, formation_rank: i32) -> i32 {
    if !sea {
        return region_size.wrapping_mul(2);
    }
    if formation_rank != 0 {
        um_ceil(formation_rank as f32 / PATH_SPEED_SCALE * region_size as f32)
    } else {
        region_size
    }
}

/// `SetGroupPathGoals`' skip of the path's first points near the start sector: `near(i)` whether point `i`'s
/// sector's centre is within one of the start's (at most three skipped); the points kept.
pub fn skip_near_start(path: &[[f32; 3]], near: &mut dyn FnMut(usize) -> bool) -> Vec<[f32; 3]> {
    let mut out = Vec::with_capacity(path.len());
    let mut skip = true;
    let mut skipped = 0;
    for (i, p) in path.iter().enumerate() {
        if skip {
            if skipped >= 3 {
                skip = false;
            } else if near(i) {
                skipped += 1;
            } else {
                skip = false;
            }
        }
        if !skip {
            out.push(*p);
        }
    }
    out
}

impl EeUnitsWaypoints {
    pub fn len(&self) -> usize {
        self.list.len()
    }
    pub fn is_empty(&self) -> bool {
        self.list.is_empty()
    }
    pub fn get(&self, i: usize) -> Option<&EeUnitsWaypoint> {
        self.list.get(i)
    }
    pub fn front(&self) -> Option<&EeUnitsWaypoint> {
        self.list.front()
    }
    pub fn back(&self) -> Option<&EeUnitsWaypoint> {
        self.list.back()
    }
    pub fn push_back(&mut self, w: EeUnitsWaypoint) {
        self.list.push_back(w);
    }
    pub fn pop_front(&mut self) -> Option<EeUnitsWaypoint> {
        self.list.pop_front()
    }
    pub fn pop_back(&mut self) -> Option<EeUnitsWaypoint> {
        self.list.pop_back()
    }
    pub fn clear(&mut self) {
        self.list.clear();
    }
    pub fn locations(&self) -> Vec<[f32; 3]> {
        self.list.iter().map(|w| w.location).collect()
    }

    /// `AddWaypoint`'s element: the step from the last waypoint (or `from`, the unit's location, for the first),
    /// its length summed in the original's order.
    pub fn add(&mut self, at: [f32; 3], from: [f32; 3]) {
        let (start, origin) = match self.list.back() {
            Some(last) => (last.distance, last.location),
            None => (0.0, from),
        };
        let (dx, dy, dz) = (at[0] - origin[0], at[1] - origin[1], at[2] - origin[2]);
        let step = (dx * dx + dy * dy + dz * dz).sqrt();
        let distance = if self.list.is_empty() { step } else { start + step };
        self.list.push_back(EeUnitsWaypoint { location: at, distance, start_distance: start });
    }

    /// `GetPathStart`: the first waypoint and the remaining length.
    pub fn path_start(&self) -> Option<([f32; 3], f32)> {
        let first = self.list.front()?;
        let last = self.list.back()?;
        Some((first.location, last.distance - first.start_distance))
    }

    /// `GetSecondWaypoint`
    pub fn second(&self) -> Option<[f32; 3]> {
        self.list.get(1).map(|w| w.location)
    }
}

/// `SnapLastWaypoint`'s goal kinds: location, attack unit, build/repair, 6, load transport, load garrison.
pub fn snaps_to_goal(kind: u8) -> bool {
    !(kind == 0 || (kind > 2 && kind != 4 && (kind <= 5 || (kind > 7 && kind != 12))))
}

/// `TakePath`'s first index skipped from the end: 2 when the path's last point is on the unit's tile.
pub fn take_path_skip(tile: (i64, i64), path: &[[f32; 3]]) -> usize {
    match path.last() {
        Some(p) if path.len() > 1 && tile.0 == um::floor(p[0]) as i64 && tile.1 == um::floor(p[1]) as i64 => 2,
        _ => 1,
    }
}

/// The naval citizens' and priests' `AdjustPath`: when smoothing, every `step`-th point and both ends.
pub fn thin_path(path: &[[f32; 3]], step: i32) -> Vec<[f32; 3]> {
    let n = path.len() as i32;
    (0..n).filter(|&i| i == n - 1 || i == 0 || i % step == 0).map(|i| path[i as usize]).collect()
}

/// `EEPathManager::MergeWaypoints`: when one of the waypoints (last first) lies on a tile of `path`, the waypoints
/// after it appended.
pub fn merge(path: &mut Vec<[f32; 3]>, waypoints: &[[f32; 3]]) {
    let pts: Vec<crate::astar::path_splice::EePathPoint> =
        path.iter().map(|p| crate::astar::path_splice::EePathPoint { x: p[0], y: p[1], z: p[2] }).collect();
    let wps: Vec<crate::astar::path_splice::EePathPoint> =
        waypoints.iter().map(|p| crate::astar::path_splice::EePathPoint { x: p[0], y: p[1], z: p[2] }).collect();
    if let Some(found) = crate::astar::path_splice::merge_waypoints(&pts, &wps) {
        path.extend_from_slice(&waypoints[found + 1..]);
    }
}

#[cfg(not(test))]
pub use game::*;

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::cls_dispatch::{EE_UNITS_BODY_ADJUST_PATH, EE_UNIT_CLASS_BODIES};
    use crate::units::goal_obj;
    use crate::units::rust_goal::{EeUnitsGoalIo, EE_UNITS_GOAL_LOCATION, EE_UNITS_GOAL_PATH_TAKEN};
    use crate::units::unit_fields::EeUnitsComplexUnitFields;
    use crate::world::layout::layout;
    use crate::world::{UnitView, WorldRead, WorldView};
    use std::os::raw::c_long;

    extern "C" {
        /// the path bodies' C++ calls (`EE_UNITS_WP_*`, EEComplexUnit.cpp)
        fn ee_units_wp_host(op: i32, unit: u64, arg: u64, point: *mut f32) -> i64;
    }

    fn token() -> WorldRead {
        // SAFETY: a unit's path changes on the simulation thread, which holds the world
        unsafe { WorldRead::begin() }
    }

    /// One of the path bodies' C++ calls (`EE_UNITS_WP_*`).
    pub fn host(op: i32, unit: u64, arg: u64) -> i64 {
        // SAFETY: the operation's live unit (and goal)
        unsafe { ee_units_wp_host(op, unit, arg, std::ptr::null_mut()) }
    }

    /// The path manager's call `what` on unit `unit` (`gPathManager.Call`: its PMRQ scope).
    fn manager_call(what: i32, unit: u64, values: &[i64]) -> i64 {
        let mut io = crate::astar::pm::EePathIo::default();
        io.h[0] = unit;
        io.i[..values.len()].copy_from_slice(values);
        // SAFETY: the game's live manager
        unsafe { crate::astar::pm::ee_path_manager_call(crate::astar::pm::global(), what, &mut io) }
    }

    /// `EEComplexUnit::RefreshPath`: the unit's pending group path completed (`CompleteAutoPath` of its active
    /// group's request for it, while not done) and, with at most two waypoints left, its own (`CompletePath`).
    ///
    /// # Safety
    /// `unit` a live complex unit.
    pub unsafe fn refresh_path(unit: u64) {
        let (group, id) = {
            let t = token();
            // SAFETY: the caller's contract
            unsafe { UnitView::of(&t, unit) }.map_or((0, 0), |u| (u.active_group_handle(), u.id()))
        };
        if group != 0 {
            // the group's GetUnitPath (group_obj.rs)
            // SAFETY: the unit's live active group
            let path =
                unsafe { crate::units::group_host::ee_units_group_get(group as *mut _, 6, i64::from(id)) } as u64;
            // SAFETY: the group's live request
            if path != 0 && !unsafe { crate::astar::request::get(path) }.r.done {
                manager_call(10, path, &[]);
            }
        }
        // SAFETY: the caller's contract
        if unsafe { list(unit) }.len() <= 2 {
            manager_call(9, unit, &[]);
        }
    }

    /// `gPathManager.IsPathDone(unit)`, `HasAutoPath(unit)`, `CancelPaths(unit, true)`
    pub fn is_path_done(unit: u64) -> bool {
        manager_call(12, unit, &[]) != 0
    }
    pub fn has_auto_path(unit: u64) -> bool {
        manager_call(11, unit, &[]) != 0
    }
    pub fn cancel_paths(unit: u64) {
        manager_call(7, unit, &[1]);
    }

    /// The path sector at a tile (`GetPathSector`: 0 off the map) and its centre.
    fn sector_at(x: i64, y: i64) -> u64 {
        let Some(r) = crate::astar::world_view::cm() else { return 0 };
        match r.sector_index(x as c_long, y as c_long) {
            Some(i) => {
                r.tables().tables[crate::astar::cm::EeAStarCmTable::PathSectors as usize].get(i).copied().unwrap_or(0)
            }
            None => 0,
        }
    }
    fn sector_center(h: u64) -> (i64, i64) {
        // SAFETY: a listed sector is a live object
        let c = unsafe { crate::astar::cm_io::sector(h).center };
        (c.x as i64, c.y as i64)
    }
    fn distance_squared(a: (i64, i64), b: (i64, i64)) -> i64 {
        let (dx, dy) = ((a.0 - b.0) as i32, (a.1 - b.1) as i32);
        i64::from(dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)))
    }

    /// `ShouldMoveToNextAutoWaypoint()`.
    ///
    /// # Safety
    /// `unit` a live complex unit.
    pub unsafe fn next_auto(unit: u64) -> bool {
        let (goal, location) = {
            let t = token();
            let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return false };
            (u.goal_handle(), u.goal().is_some_and(|(k, _)| k == crate::units::location::GOAL_LOCATION))
        };
        if !location
            || !crate::units::location::get(goal).auto_waypoint
            || host(EE_UNITS_WP_QUEUE_LENGTH, unit, 0) <= 1
            || !is_path_done(unit)
            || has_auto_path(unit)
        {
            return false;
        }
        let region_size = host(EE_UNITS_WP_REGION_SIZE, unit, 0) as i32;
        // SAFETY: the caller's contract
        let l = unsafe { list(unit) };
        let n = l.len() as i32;
        let result = n <= region_size;
        if result || n <= 0 {
            return result;
        }
        let back = l.back().map(|w| w.location).expect("a waypoint");
        let t = token();
        let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return false };
        let (tx, ty) = u.tile_xy();
        let from = sector_at(tx, ty);
        if from == 0 {
            return false;
        }
        let to = sector_at(i64::from(um::floor(back[0])), i64::from(um::floor(back[1])));
        if to == 0 {
            return false;
        }
        let sea = u.class().is_some_and(|c| crate::world_field!(c.fields(), mDomains) & MOVEMENT_SEA != 0);
        let rank = crate::world_field!(u.complex(), mFormationRank) as i32;
        let limit = auto_waypoint_limit(sea, region_size, rank);
        n <= limit && distance_squared(sector_center(from), sector_center(to)) < 2
    }

    /// The trigger host's waypoint step (`EE_UNITS_COND_WAYPOINT_STEP`, `c`): no more waypoints (0), the next one
    /// retrieved (1), arrived (2), one left with the path done (3).
    ///
    /// # Safety
    /// `unit` a live complex unit.
    pub unsafe fn waypoint_step(unit: u64, c: i64) -> i64 {
        // SAFETY: the caller's contract
        let len = || unsafe { list(unit) }.len();
        let flag = match c {
            0 => {
                let mut f = len() == 0;
                if f && (!has_auto_path(unit) || !is_path_done(unit)) {
                    refresh_path(unit);
                    f = len() == 0;
                }
                f
            }
            1 => {
                if len() == 0 {
                    refresh_path(unit);
                    return i64::from(len() > 0);
                }
                if len() == 1 {
                    refresh_path(unit);
                }
                return 1;
            }
            2 => {
                let t = token();
                unsafe { UnitView::of(&t, unit) }.is_some_and(|u| crate::world_field!(u.complex(), mHasArrived))
            }
            _ => return i64::from(len() == 1 && is_path_done(unit)),
        };
        if flag {
            let t = token();
            if let Some(u) = unsafe { UnitView::of(&t, unit) } {
                let c = u.complex().ptr() as *mut EeUnitsComplexUnitFields;
                // SAFETY: the unit's own record
                unsafe {
                    if (*c).mCollisionState != 0 {
                        (*c).mCollisionState = 0;
                    }
                }
            }
        }
        i64::from(flag)
    }

    /// `FindWaypointAtTile(x, y)`: the first waypoint on the tile (an off-map one at its closest tile).
    ///
    /// # Safety
    /// `unit` a live complex unit.
    pub unsafe fn at_tile(unit: u64, x: i64, y: i64) -> Option<usize> {
        // SAFETY: the caller's contract
        let points = unsafe { list(unit) }.locations();
        for (i, p) in points.iter().enumerate() {
            let (mut wx, mut wy) = (i64::from(um::floor(p[0])), i64::from(um::floor(p[1])));
            let on = {
                let t = token();
                WorldView::global(&t).and_then(|w| w.map()).is_some_and(|m| m.tile(wx, wy).is_some())
            };
            if !on {
                let mut q = *p;
                // SAFETY: three floats
                let packed = unsafe { ee_units_wp_host(EE_UNITS_WP_CLOSEST_TILE, unit, 0, q.as_mut_ptr()) };
                wx = i64::from(packed as u32 as i32);
                wy = i64::from((packed >> 32) as u32 as i32);
            }
            if wx == x && wy == y {
                return Some(i);
            }
        }
        None
    }

    /// The landing aircraft's path check (`EE_UNITS_ACT_FLY_CHECK_PATH`): a next waypoint it cannot reach (unless
    /// an empty paratrooper bomber's) is replaced by its landing point.
    ///
    /// # Safety
    /// `unit` a live complex unit.
    pub unsafe fn fly_check_path(unit: u64) {
        // SAFETY: the caller's contract
        let Some((next, _)) = unsafe { list(unit) }.path_start() else { return };
        let ok = {
            let t = token();
            let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return };
            let bomber = u.class().is_some_and(|c| crate::world_field!(c.fields(), mIsParatrooperBomber));
            !bomber || u.cargo_len() != 0
        };
        if ok && !crate::units::uu_host::can_get_to(unit, next[0] as c_long as i32, next[1] as c_long as i32) {
            unsafe { drop_path(unit) };
            let mut target = next;
            // SAFETY: a live unit, three floats
            unsafe { crate::units::landing::ee_units_find_landing_point(unit, &mut target) };
            unsafe { add(unit, target, false) };
        }
    }

    /// `EEUGRust::NewLocation(at, arg, forced, false, 0, true, 0)`: an auto-waypoint location goal.
    pub fn new_auto_waypoint(at: [f32; 3], arg: bool, forced: bool) -> u64 {
        let goal = goal_obj::goal_new(crate::units::location::GOAL_LOCATION, arg);
        let mut io = EeUnitsGoalIo::default();
        io.i[0] = i64::from(forced);
        io.i[2] = 1;
        io.f[..3].copy_from_slice(&at);
        // SAFETY: the goal made above
        unsafe { goal_obj::call(crate::units::rust_goal::EE_UNITS_GOAL_NEW_LOCATION, goal, 0, 0, &mut io) };
        goal
    }

    fn set_group_path_state(goal: u64, state: u8) {
        let mut r = crate::units::location::get(goal);
        r.group_path_state = state;
        crate::units::location::put(goal, r);
    }

    fn goal_kind(g: u64) -> u8 {
        if g == 0 {
            0
        } else {
            // SAFETY: a live goal
            unsafe { goal_obj::base(g) }.kind
        }
    }

    fn goal_of(unit: u64) -> u64 {
        let t = token();
        // SAFETY: a live complex unit
        unsafe { UnitView::of(&t, unit) }.map_or(0, |u| u.goal_handle())
    }

    fn finish(unit: u64) {
        host(EE_UNITS_WP_REFRESH_GOALS, unit, 0);
        host(EE_UNITS_WP_REFRESH_ACTIVE_GOAL, unit, 0);
    }

    /// `SetGroupPathGoals(path)`: a group path given to the unit as auto-waypoint location goals (replacing its
    /// location goal, or after its pending attack), its first points near its start sector skipped, the unit set
    /// moving.
    ///
    /// # Safety
    /// `unit` a live complex unit.
    pub unsafe fn group_path_goals(unit: u64, path: &[[f32; 3]]) {
        use crate::units::location::GOAL_LOCATION;
        let goal = goal_of(unit);
        let kind = goal_kind(goal);
        let attack = kind == 2;
        let has_location = kind == GOAL_LOCATION || kind == GOAL_HEAL_UNIT;
        let location = kind == GOAL_LOCATION;
        let (alive, start, command) = {
            let t = token();
            // SAFETY: the caller's contract
            let u = unsafe { UnitView::of(&t, unit) }.expect("a live unit");
            let (tx, ty) = u.tile_xy();
            let alive = u.hit_points() >= 1 && !crate::world_field!(u.complex(), mIsDisabled) && u.tile().is_some();
            (alive, sector_at(tx, ty), u.command_type())
        };
        let mut keep_attack = false;
        let mut replaced = false;
        if !alive {
            return finish(unit);
        }
        if !has_location {
            if !attack {
                if command != crate::units::location::ACTION_IDLE as u32 {
                    return finish(unit);
                }
            } else if !unsafe { goal_obj::base(goal) }.b1c {
                keep_attack = true;
                if host(EE_UNITS_WP_QUEUE_LENGTH, unit, 0) < 2 {
                    return finish(unit);
                }
            }
        } else if location {
            set_group_path_state(goal, 3);
        }
        if !keep_attack {
            if host(EE_UNITS_WP_QUEUE_LENGTH, unit, 0) == 0 {
                return finish(unit);
            }
            let back = host(EE_UNITS_WP_QUEUE_BACK, unit, 0) as u64;
            if goal_kind(back) == GOAL_LOCATION && crate::units::location::get(back).auto_waypoint {
                return finish(unit);
            }
        }
        if path.len() >= 2 {
            let kept = skip_near_start(path, &mut |i| {
                let s = sector_at(i64::from(um::floor(path[i][0])), i64::from(um::floor(path[i][1])));
                distance_squared(sector_center(start), sector_center(s)) < 2
            });
            for p in kept.iter().rev() {
                let current = goal_of(unit);
                let arg = current != 0 && unsafe { goal_obj::base(current) }.b1c;
                let forced = location && crate::units::location::get(goal).forced_move;
                let g = new_auto_waypoint(*p, arg, forced);
                if !keep_attack {
                    host(EE_UNITS_WP_SET_GOAL, unit, g);
                    replaced = true;
                } else {
                    host(EE_UNITS_WP_INSERT_GOAL, unit, g);
                }
            }
        }
        host(EE_UNITS_WP_REFRESH_GOALS, unit, 0);
        host(EE_UNITS_WP_REFRESH_ACTIVE_GOAL, unit, 0);
        if !replaced {
            let current = goal_of(unit);
            let k = goal_kind(current);
            if !(k == GOAL_LOCATION || k == GOAL_HEAL_UNIT) {
                return finish(unit);
            }
            let mut at = [0.0f32; 3];
            // SAFETY: the unit's live goal; three floats
            unsafe { ee_units_wp_host(EE_UNITS_WP_GOAL_POINT, unit, current, at.as_mut_ptr()) };
            let (x, y) = (i64::from(um::floor(at[0])), i64::from(um::floor(at[1])));
            let tile = {
                let t = token();
                unsafe { UnitView::of(&t, unit) }.map_or((0, 0), |u| u.tile_xy())
            };
            if tile == (x, y) {
                return finish(unit);
            }
            if let Some(b) = unsafe { list(unit) }.back() {
                if i64::from(um::floor(b.location[0])) == x && i64::from(um::floor(b.location[1])) == y {
                    return finish(unit);
                }
            }
            if k == GOAL_LOCATION {
                set_group_path_state(current, 4);
            }
        }
        host(EE_UNITS_WP_START_ACTION, unit, crate::units::location::ACTION_PREPARE_TO_MOVE as u64);
        host(EE_UNITS_WP_ACTION_CHANGED, unit, 0);
        finish(unit);
    }

    /// The unit's list (`unit` a live `EEComplexUnit*`).
    ///
    /// # Safety
    /// `unit` is a live complex unit; the list is not used across a call that could delete the unit.
    pub unsafe fn list<'a>(unit: u64) -> &'a mut EeUnitsWaypoints {
        // SAFETY: the caller's contract; the member's first word is the list's handle, never null
        unsafe {
            &mut *(std::ptr::read((unit as usize + layout().complex_waypoints as usize) as *const usize)
                as *mut EeUnitsWaypoints)
        }
    }

    fn reset_collision_count(u: &UnitView<'_>) {
        // SAFETY: the unit's own record, written on the simulation thread
        unsafe { (*(u.complex().ptr() as *mut EeUnitsComplexUnitFields)).mCollisionResolutionCount = 0.0 };
    }

    /// `AddWaypoint(at, snap)`: off-map points are refused unless snapped (the snap's closest tile is not read).
    ///
    /// # Safety
    /// `unit` is a live complex unit.
    pub unsafe fn add(unit: u64, at: [f32; 3], snap: bool) {
        let t = token();
        // SAFETY: the caller's contract
        let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return };
        if !snap {
            if let Some(m) = WorldView::global(&t).and_then(|w| w.map()) {
                let (x, y) = (um::floor(at[0]) as c_long, um::floor(at[1]) as c_long);
                let f = m.rts();
                let (a, b, c): (c_long, c_long, c_long) = (
                    crate::world_field!(f, mL195608),
                    crate::world_field!(f, mL19560C),
                    crate::world_field!(f, mL195610),
                );
                if x + y == a || y - x == c || x + y == b || x - y == c {
                    return;
                }
            }
        }
        let from = u.location();
        // SAFETY: the caller's contract
        unsafe { list(unit) }.add(at, from);
    }

    /// `NextWaypoint()`: the first dropped (7: none); the collision count reset either way.
    ///
    /// # Safety
    /// `unit` is a live complex unit.
    pub unsafe fn next(unit: u64) -> i32 {
        let t = token();
        // SAFETY: the caller's contract
        let r = if unsafe { list(unit) }.pop_front().is_some() { 0 } else { EE_UNITS_WAYPOINTS_NONE };
        if let Some(u) = unsafe { UnitView::of(&t, unit) } {
            reset_collision_count(&u);
        }
        r
    }

    /// `DropPath()`
    ///
    /// # Safety
    /// `unit` is a live complex unit.
    pub unsafe fn drop_path(unit: u64) {
        let t = token();
        // SAFETY: the caller's contract
        unsafe { list(unit) }.clear();
        if let Some(u) = unsafe { UnitView::of(&t, unit) } {
            reset_collision_count(&u);
        }
    }

    /// `SnapLastWaypoint()`: the last waypoint re-added at the goal's location when both share a tile.
    ///
    /// # Safety
    /// `unit` is a live complex unit.
    pub unsafe fn snap_last(unit: u64) {
        let t = token();
        // SAFETY: the caller's contract
        let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return };
        let l = unsafe { list(unit) };
        let goal = u.goal_handle();
        if l.is_empty() || goal == 0 {
            return;
        }
        let Some((kind, _)) = u.goal() else { return };
        if !snaps_to_goal(kind) {
            return;
        }
        let mut io = EeUnitsGoalIo::default();
        // SAFETY: the unit's live goal
        unsafe { goal_obj::call(EE_UNITS_GOAL_LOCATION, goal, 0, 0, &mut io) };
        let at = [io.f[0], io.f[1], io.f[2]];
        let l = unsafe { list(unit) };
        let Some(last) = l.back().copied() else { return };
        if um::floor(last.location[0]) == um::floor(at[0]) && um::floor(last.location[1]) == um::floor(at[1]) {
            l.pop_back();
            // SAFETY: as above
            unsafe { add(unit, at, false) };
        }
    }

    /// The path manager's `EE_PATH_UNIT_SNAP`: the snap, then the goal's `PathTaken(unit)` when `taken`.
    ///
    /// # Safety
    /// `unit` is a live complex unit.
    pub unsafe fn snap_and_take(unit: u64, taken: bool) {
        // SAFETY: the caller's contract
        unsafe { snap_last(unit) };
        if taken {
            let t = token();
            // SAFETY: as above
            let goal = unsafe { UnitView::of(&t, unit) }.map_or(0, |u| u.goal_handle());
            if goal != 0 {
                // SAFETY: the unit's live goal
                unsafe { goal_obj::call(EE_UNITS_GOAL_PATH_TAKEN, goal, unit, 0, std::ptr::null_mut()) };
            }
        }
    }

    /// `EEUnitClass::AdjustPath(unit, path, smooth)`: the naval classes' bodies (ee_sim::units cls_dispatch.rs).
    ///
    /// # Safety
    /// `unit` is a live complex unit.
    pub unsafe fn adjust_path(unit: u64, path: &mut Vec<[f32; 3]>, smooth: bool) {
        let t = token();
        // SAFETY: the caller's contract
        let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return };
        let Some(c) = u.class() else { return };
        let category = c.category();
        let body = if category < 256 { EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_ADJUST_PATH][category as usize] } else { 0 };
        let sphere: f32 = crate::world_field!(c.fields(), mInnerCollisionSphere);
        let class = c.handle();
        match body {
            // the naval citizen's (a step under 1 is 1), the priest ship's (a step of 0 is 1)
            1 | 2 | 5 => {
                if !smooth {
                    return;
                }
                let mut step = um::lrintf(sphere);
                if (body == 1 && step <= 0) || step == 0 {
                    step = 1;
                }
                *path = thin_path(path, step);
            }
            3 => {
                // EEAdjustNavalPath: the kept points of ee_units_uu_naval_path (its UUNT note)
                let mut kept = vec![0u32; path.len() + 1];
                // SAFETY: the path's points and room for every index
                let n = unsafe {
                    crate::units::uu_ffi::ee_units_uu_naval_path(
                        unit,
                        if path.is_empty() { std::ptr::null() } else { path.as_ptr() },
                        path.len() as u32,
                        smooth as u8,
                        kept.as_mut_ptr(),
                    )
                };
                if smooth {
                    *path = kept[..n as usize].iter().map(|&i| path[i as usize]).collect();
                }
            }
            4 => {
                // SAFETY: the caller's live class and unit
                unsafe { crate::units::ucg_sea::sea_animal_path(class, unit, path) }
            }
            _ => {}
        }
    }

    /// `TakePath(path, smooth)`: the class's `AdjustPath`, then the points as waypoints, last first, skipping the
    /// start (and the unit's own tile).
    ///
    /// # Safety
    /// `unit` is a live complex unit.
    pub unsafe fn take_path(unit: u64, path: &mut Vec<[f32; 3]>, smooth: bool) {
        let t = token();
        // SAFETY: the caller's contract
        let tile = unsafe { UnitView::of(&t, unit) }.map_or((0, 0), |u| u.tile_xy());
        let skip = take_path_skip(tile, path);
        // SAFETY: as above
        unsafe { adjust_path(unit, path, smooth) };
        let mut i = path.len() as i64 - skip as i64;
        while i >= 0 {
            // SAFETY: as above
            unsafe { add(unit, path[i as usize], false) };
            i -= 1;
        }
    }
}

// ---- the C interface (`EEWaypointList` and EEComplexUnit's inline members)

/// A new empty list.
#[no_mangle]
pub extern "C" fn ee_units_waypoints_new() -> *mut EeUnitsWaypoints {
    Box::into_raw(Box::default())
}

/// Frees a list.
///
/// # Safety
/// `list` was made by [`ee_units_waypoints_new`] (or is null) and is not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_free(list: *mut EeUnitsWaypoints) {
    if !list.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(list) });
    }
}

/// `*to = *from` (the deque's assignment).
///
/// # Safety
/// Both are live lists.
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_assign(to: *mut EeUnitsWaypoints, from: *const EeUnitsWaypoints) {
    if !std::ptr::eq(to, from) {
        // SAFETY: the caller's contract
        unsafe { (*to).list.clone_from(&(*from).list) };
    }
}

/// The waypoints' count.
///
/// # Safety
/// A live list.
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_len(list: *const EeUnitsWaypoints) -> usize {
    // SAFETY: the caller's contract
    unsafe { (*list).len() }
}

/// Waypoint `i` in place (null past the end): valid until the list changes.
///
/// # Safety
/// A live list.
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_at(list: *const EeUnitsWaypoints, i: usize) -> *const EeUnitsWaypoint {
    // SAFETY: the caller's contract
    unsafe { (*list).get(i) }.map_or(std::ptr::null(), |w| w as *const EeUnitsWaypoint)
}

/// Appends a waypoint as it is (a load's).
///
/// # Safety
/// A live list and waypoint.
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_push(list: *mut EeUnitsWaypoints, w: *const EeUnitsWaypoint) {
    // SAFETY: the caller's contract
    unsafe { (*list).push_back(*w) }
}

/// Empties the list (the unit's reset; `DropPath` is [`ee_units_waypoints_drop`]).
///
/// # Safety
/// A live list.
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_clear(list: *mut EeUnitsWaypoints) {
    // SAFETY: the caller's contract
    unsafe { (*list).clear() }
}

/// `GetPathStart(out, length)` (`length` null: `GetPathLength`'s; `out` null: only the length), 0, or 7 without a
/// path (the outputs untouched).
///
/// # Safety
/// A live list; `out` null or three floats, `length` null or one.
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_start(
    list: *const EeUnitsWaypoints,
    out: *mut f32,
    length: *mut f32,
) -> i32 {
    // SAFETY: the caller's contract
    match unsafe { (*list).path_start() } {
        Some((p, l)) => {
            unsafe {
                if !out.is_null() {
                    std::ptr::copy_nonoverlapping(p.as_ptr(), out, 3);
                }
                if !length.is_null() {
                    *length = l;
                }
            }
            0
        }
        None => EE_UNITS_WAYPOINTS_NONE,
    }
}

/// `GetSecondWaypoint(out)`: 0, or 7 with fewer than two (`out` untouched).
///
/// # Safety
/// A live list; `out` three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_second(list: *const EeUnitsWaypoints, out: *mut f32) -> i32 {
    // SAFETY: the caller's contract
    match unsafe { (*list).second() } {
        Some(p) => {
            unsafe { std::ptr::copy_nonoverlapping(p.as_ptr(), out, 3) };
            0
        }
        None => EE_UNITS_WAYPOINTS_NONE,
    }
}

/// `AddWaypoint(at, snap)` of the complex unit `unit`.
///
/// # Safety
/// `unit` is a live complex unit, `at` three floats.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_add(unit: u64, at: *const f32, snap: bool) {
    // SAFETY: the caller's contract
    unsafe { add(unit, [*at, *at.add(1), *at.add(2)], snap) }
}

/// `NextWaypoint()` of the complex unit `unit`: 0, or 7 without one.
///
/// # Safety
/// `unit` is a live complex unit.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_next(unit: u64) -> i32 {
    // SAFETY: the caller's contract
    unsafe { next(unit) }
}

/// `DropPath()` of the complex unit `unit`.
///
/// # Safety
/// `unit` is a live complex unit.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_drop(unit: u64) {
    // SAFETY: the caller's contract
    unsafe { drop_path(unit) }
}

/// `SnapLastWaypoint()` of the complex unit `unit`.
///
/// # Safety
/// `unit` is a live complex unit.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_snap(unit: u64) {
    // SAFETY: the caller's contract
    unsafe { snap_last(unit) }
}

/// `TakePath(path, smooth)` of the complex unit `unit` (`points` the path, changed in place to the adjusted one:
/// its new count returned; room for `count`).
///
/// # Safety
/// `unit` is a live complex unit, `points` `count` points.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_take_path(
    unit: u64,
    points: *mut [f32; 3],
    count: u32,
    smooth: bool,
) -> u32 {
    // SAFETY: the caller's contract
    let mut path =
        if count == 0 { Vec::new() } else { unsafe { std::slice::from_raw_parts(points, count as usize) }.to_vec() };
    unsafe { take_path(unit, &mut path, smooth) };
    let n = path.len().min(count as usize);
    // SAFETY: room for `count`
    unsafe { std::ptr::copy_nonoverlapping(path.as_ptr(), points, n) };
    n as u32
}

/// `ShouldMoveToNextAutoWaypoint()` of complex unit `unit`.
///
/// # Safety
/// `unit` is a live complex unit.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_next_auto(unit: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe { next_auto(unit) }
}

/// `SetGroupPathGoals(path)` of complex unit `unit` (`points` `count` x, y, z points).
///
/// # Safety
/// `unit` is a live complex unit; `points` holds `count` points (or is null with none).
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_group_path_goals(unit: u64, points: *const f32, count: usize) {
    let path: Vec<[f32; 3]> = if count == 0 {
        Vec::new()
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(points as *const [f32; 3], count) }.to_vec()
    };
    // SAFETY: the caller's contract
    unsafe { group_path_goals(unit, &path) }
}

/// `FindWaypointAtTile(x, y)` of complex unit `unit`: whether found, its index in `index`.
///
/// # Safety
/// `unit` is a live complex unit; `index` writable.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_waypoints_at_tile(
    unit: u64,
    x: std::os::raw::c_long,
    y: std::os::raw::c_long,
    index: *mut usize,
) -> bool {
    // SAFETY: the caller's contract
    match unsafe { at_tile(unit, x as i64, y as i64) } {
        Some(i) => {
            unsafe { *index = i };
            true
        }
        None => false,
    }
}

#[cfg(test)]
#[path = "waypoints_tests.rs"]
mod tests;
