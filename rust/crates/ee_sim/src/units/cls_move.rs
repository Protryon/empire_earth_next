//! The movement bodies of `EEUnitClass` (the unit classes cluster, round 1; EEUnitClass.cpp until this round):
//! `FindPath` (the melee sidestep, the bounded path toward the next waypoints), the class's `NextWaypoint` (the
//! blocked-tile and cut-corner tests), `PathToGoal`, `PathToward`, `AddWaypointAt`, `BaseHasArrived`, `Flee` and
//! `FleeAttacker`. The C++ members are one-line calls of the entry points in cls_move_host.rs.
//!
//! The world is the [`MoveHost`]: the unit's and class's plain members are read (and the plain stores the C++ made
//! on them written) in place on the records their objects derive from ([`EeUnitsUuUnit`]), never across a host call;
//! the unit utilities the C++ called (`EECanStandOn`, `EEIsTileTopAbove`, `EEGetUnitTiles`, `EEGetTileLine`,
//! `EEMoveInDirection`, `EETurnAway`) are the same Rust entry points with their UUTL notes; everything else (the
//! goals, the waypoints, the path manager, the random draws, the actions) is a host question or statement, asked in
//! the C++'s order.
//!
//! Determinism (RUST_PLAN 2.1): the C++'s float operations in order, `long` as `i32`, `UMFloor` and
//! `UMFloorUnsigned` as ee_base's, `UMFloatToLong` as `lrintf`.

// The C++'s negated float comparisons (`!(a < b)`) are kept as written: they differ from the flipped comparison for
// a NaN.
// `c_long` is 32 bits on Windows and 64 on LP64: the casts are needed there.
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::unnecessary_cast)]

use crate::units::unit_class_fields::EeUnitsUnitClassFields;
use crate::units::unit_fields::EeUnitsComplexUnitFields;
use crate::units::uu::EeUnitsUuUnit;
use crate::units::uu_tiles::Tile;
use ee_base::math::types::{Ge3DPoint, GeTransformation, GeVector};
use ee_base::math::um::{floor, floor_unsigned, lrintf, um_arctan2};

/// The C++ enums the bodies test.
/// cbindgen:ignore
pub mod k {
    pub const MV_MOVEMENT_AIR: u8 = 1;
    pub const MV_MOVEMENT_LAND: u8 = 2;
    pub const MV_MOVEMENT_SEA: u8 = 4;
    pub const MV_MOVEMENT_SPACE: u8 = 8;
    pub const MV_GOAL_LOCATION: i32 = 1;
    pub const MV_GOAL_ATTACK_UNIT: i32 = 2;
    pub const MV_GOAL_EXPLORE: i32 = 10;
    pub const MV_ACTION_FIND_NEW_RESOURCE: i32 = 4006;
    pub const MV_ACTION_IDLE: i32 = 5000;
    pub const MV_ACTION_PREPARE_TO_MOVE: i32 = 8000;
    /// `kHeadingUnset`
    pub const MV_HEADING_UNSET: f32 = -10000.0;
    /// `kMinWaypointDistanceSquared`
    pub const MV_MIN_WAYPOINT_DISTANCE_SQUARED: f32 = 36.0;
    /// `kFleeDistanceScale`
    pub const MV_FLEE_DISTANCE_SCALE: f32 = 1.5;
    /// `k447AF0` (the near point of `PathToward`)
    pub const MV_NEAR: f32 = 0.01;
    /// the squeezed side's count
    pub const MV_SQUEEZED: i32 = 0x7fff_ffff;
}
use k::*;

/// The world the movement bodies ask (the game's: cls_move_host.rs). Units, classes and goals are their objects'
/// addresses; 0 is NULL.
pub trait MoveHost {
    /// a unit's records and pointer members (`obj` an `EEUnit*`)
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit;
    /// a class object's record
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields;
    /// `obj->GetPosition(out)` (virtual)
    fn position(&mut self, obj: u64) -> Ge3DPoint;
    /// the unit's `mPhysicalModel.GetLocation()`
    fn location(&mut self, unit: u64) -> GeTransformation;
    /// `gWorld.mMap->GetTile(x, y)` (0: off the map); its height; its unit nodes' units in order (0 for a NULL one)
    fn map_tile(&mut self, x: i32, y: i32) -> u64;
    fn tile_height(&mut self, tile: u64) -> f32;
    fn tile_units(&mut self, tile: u64) -> Vec<u64>;
    /// `gWorld.mMap->GetClosestTile(&point, &x, &y)`'s height (None: no tile)
    fn closest_tile_height(&mut self, point: Ge3DPoint) -> Option<f32>;
    /// `obj->IsPassableBy(by)`
    fn passable_by(&mut self, obj: u64, by: u64) -> bool;
    /// the unit utilities (their UUTL notes): `EECanStandOn`, `EEIsTileTopAbove(x, y, 0, &wall, true)`,
    /// `EEGetUnitTiles(unit, out, margin)`, `EEGetTileLine(.., ends, thin)`, `EEMoveInDirection`, `EETurnAway`
    fn can_stand_on(&mut self, tile: Tile, unit: u64) -> bool;
    fn tile_top_above(&mut self, x: i32, y: i32) -> bool;
    fn unit_tiles(&mut self, unit: u64, margin: f32) -> Vec<Tile>;
    fn tile_line(&mut self, from: Tile, to: Tile, ends: bool, thin: bool) -> Vec<Tile>;
    fn move_in_direction(&mut self, x: f32, y: f32, direction: i32, distance: i32) -> [f32; 2];
    fn turn_away(&mut self, direction: i32, spread: bool) -> i32;
    /// `EEGetPursuerOf(unit)`, `EEGetDirectionTo(from, to)`, the unit's `GetLineOfSight()`, `GetMaxVelocity()`,
    /// `GetMaxAngularVelocity()`, `gPhysics->GetRandom(1, 2)`
    fn pursuer_of(&mut self, unit: u64) -> u64;
    fn direction_to(&mut self, from: u64, to: u64) -> i32;
    fn line_of_sight(&mut self, unit: u64) -> f32;
    fn max_velocity(&mut self, unit: u64) -> f32;
    fn max_angular_velocity(&mut self, unit: u64) -> f32;
    fn random_1_2(&mut self) -> i32;
    /// the unit's `mGoalState`, `mActiveGoal`, `mBlockingUnit`, `mLastAttacker` (released and cleared by
    /// `clear_last_attacker`)
    fn goal(&mut self, unit: u64) -> u64;
    fn active_goal(&mut self, unit: u64) -> u64;
    fn blocking_unit(&mut self, unit: u64) -> u64;
    fn last_attacker(&mut self, unit: u64) -> u64;
    fn clear_last_attacker(&mut self, unit: u64);
    /// a goal's `mKind`, `GetGoalLocation`, `GetDestination(unit)`, `mB1C`, `mB1D`, `mPathAttempts += n`, the
    /// attack's `GetSidestepCount`/`SetSidestepCount`, `GetAttackTarget()->GetComplexUnit()`, the location goal's
    /// `GetIsAutoWaypoint`, `GetGroupPathState`/`SetGroupPathState`; a gather goal on wood (`EEGatherGoalCast`)
    fn goal_kind(&mut self, goal: u64) -> i32;
    fn goal_location(&mut self, goal: u64) -> Ge3DPoint;
    fn goal_destination(&mut self, goal: u64, unit: u64) -> Ge3DPoint;
    fn goal_flag(&mut self, goal: u64, b1d: bool) -> bool;
    fn add_path_attempts(&mut self, goal: u64, n: i32);
    fn sidestep_count(&mut self, goal: u64) -> u32;
    fn set_sidestep_count(&mut self, goal: u64, n: u32);
    fn attack_target(&mut self, goal: u64) -> u64;
    fn auto_waypoint(&mut self, goal: u64) -> bool;
    fn group_path_state(&mut self, goal: u64) -> u8;
    fn set_group_path_state(&mut self, goal: u64, state: u8);
    fn gathers_wood(&mut self, goal: u64) -> bool;
    /// `IgnoreTarget(target)`, the melee retarget (`EEFindNewTarget` over `EEFOFindNewTargetInMeleeRange(unit,
    /// target)`, from the unit's tile, with the attack goal), `SetBlockingUnit`, `DropPath`, `AddWaypoint`, the
    /// waypoints' count and location `i`, `GetSecondWaypoint` (Some when 0), `NextWaypoint()`,
    /// `GetNextWaypointPoint`, `GetQueueGoals(0).size()`, `DistanceSquared3D`
    fn ignore_target(&mut self, unit: u64, target: u64);
    fn melee_retarget(&mut self, unit: u64, target: u64, attack: u64) -> bool;
    fn set_blocking_unit(&mut self, unit: u64, blocker: u64);
    fn drop_path(&mut self, unit: u64);
    fn add_waypoint(&mut self, unit: u64, point: Ge3DPoint, snap: bool);
    fn waypoints(&mut self, unit: u64) -> i32;
    fn waypoint(&mut self, unit: u64, index: i32) -> Ge3DPoint;
    fn second_waypoint(&mut self, unit: u64) -> Option<Ge3DPoint>;
    fn next_waypoint(&mut self, unit: u64);
    fn next_waypoint_point(&mut self, unit: u64) -> Ge3DPoint;
    fn queue_goals(&mut self, unit: u64) -> i32;
    fn distance_squared_3d(&mut self, unit: u64, point: Ge3DPoint) -> f32;
    /// `mBoundedPathTiles.clear()` / `push_back`
    fn clear_bounded_tiles(&mut self, unit: u64);
    fn push_bounded_tile(&mut self, unit: u64, tile: Tile);
    /// `gPathManager.FindBoundedPath(unit, map, kind, unit tile, to, out, 50, true, &reached)`'s path;
    /// `EEPathManager::MergeWaypoints(path, unit->mWaypoints, 0)`
    fn find_bounded_path(&mut self, unit: u64, kind: i32, to: Tile) -> Vec<Ge3DPoint>;
    fn merge_waypoints(&mut self, unit: u64, path: &mut Vec<Ge3DPoint>);
    /// `SetVelocity`, `StartAction`, the class's `ActionChanged(unit)`, `UpdateWaterState(true)`,
    /// `gPathManager.CancelPaths(unit, true)`, `mInitialContactLocation = point`
    fn set_velocity(&mut self, unit: u64, v: f32);
    fn start_action(&mut self, unit: u64, action: i32);
    fn action_changed(&mut self, class: u64, unit: u64);
    fn update_water_state(&mut self, unit: u64);
    fn cancel_paths(&mut self, unit: u64);
    fn set_initial_contact_location(&mut self, unit: u64, point: Ge3DPoint);
    /// `PathToGoal`'s request: an enemy-avoidance modifier when the player `AvoidsEnemies(unit)`, the goal's
    /// `mPathAttempts++`, `gPathManager.RequestUnitPath(unit, unit tile, to, false, revs, graph, modifier, arg8)`
    fn request_path(&mut self, unit: u64, goal: u64, to: Tile, revs: i32, graph: bool, arg8: bool);
    /// `SetGoal(EEUGRust::NewLocation(GE2DPoint(x, y), true, true, false, 0, false, 0), keep, 0)`
    fn flee_goal(&mut self, unit: u64, to: [f32; 2], keep: bool);
}

/// The records a body reads and writes in place.
struct U {
    obj: u64,
    rec: EeUnitsUuUnit,
}

impl U {
    fn of<H: MoveHost + ?Sized>(h: &mut H, obj: u64) -> U {
        U { obj, rec: h.unit(obj) }
    }
    fn cx(&self) -> &EeUnitsComplexUnitFields {
        // SAFETY: a live complex unit's record (read at once, never across a host call)
        unsafe { &*self.rec.cx }
    }
    // the record is the C++ object's, not borrowed from `self`: a store through it is the C++'s member store
    #[allow(clippy::mut_from_ref)]
    fn cx_mut(&self) -> &mut EeUnitsComplexUnitFields {
        // SAFETY: as cx; the C++ stores these members in the same places
        unsafe { &mut *(self.rec.cx as *mut EeUnitsComplexUnitFields) }
    }
    fn class(&self) -> &EeUnitsUnitClassFields {
        // SAFETY: the unit's class record
        unsafe { &*self.rec.class_fields }
    }
    fn tile(&self) -> Tile {
        // SAFETY: the unit's RTSUnit record
        let r = unsafe { &*self.rec.rts };
        [r.mTileX as i32, r.mTileY as i32]
    }
    fn set_collision_state(&self, v: u16) {
        if self.cx().mCollisionState != v {
            self.cx_mut().mCollisionState = v;
        }
    }
}

fn class_rec<H: MoveHost + ?Sized>(h: &mut H, class: u64) -> &'static EeUnitsUnitClassFields {
    // SAFETY: a live class's record (classes live for the game)
    unsafe { &*h.class(class) }
}

fn floor_tile(p: Ge3DPoint) -> Tile {
    [floor(p.x), floor(p.y)]
}

/// The sidestep's side test: the tile's unit count, or the squeeze when the tile is a diagonal step past two
/// raised corners (`None`: the side cannot be stood on).
fn side_count<H: MoveHost + ?Sized>(h: &mut H, u: &U, at: Tile, count: &mut i32) -> Option<bool> {
    if !h.can_stand_on(at, u.obj) {
        return None;
    }
    let t = u.tile();
    if (at[0] - t[0]).abs() == 1
        && (at[1] - t[1]).abs() == 1
        && h.tile_top_above(at[0], t[1])
        && h.tile_top_above(t[0], at[1])
    {
        *count = MV_SQUEEZED;
        return Some(true);
    }
    let tile = h.map_tile(at[0], at[1]);
    *count = count.wrapping_add(h.tile_units(tile).len() as i32);
    Some(false)
}

/// `EEUnitClass::FindPath(unit)` (`class` the `this`).
pub fn find_path<H: MoveHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let goal = h.goal(unit);
    let attack = h.active_goal(unit);
    let position = h.position(unit);
    if goal != 0 {
        let goal_point = h.goal_location(goal);
        let u = U::of(h, unit);
        if u.class().mIsMelee && u.cx().mVelocity > 0.0 && attack != 0 && h.goal_kind(attack) == MV_GOAL_ATTACK_UNIT
        {
            let sphere = u.class().mInnerCollisionSphere;
            let range = sphere + sphere + 5.0;
            let first = if goal_point.x > 0.1 && position.distance_to_squared(goal_point) < range * range {
                let first = h.sidestep_count(attack) < 1;
                h.drop_path(unit);
                first
            } else {
                false
            };
            if first {
                sidestep(h, class, &u, goal, attack, goal_point);
                return;
            }
        }
    }
    if attack != 0 && h.goal_kind(attack) == MV_GOAL_ATTACK_UNIT {
        h.set_sidestep_count(attack, 0);
    }
    let u = U::of(h, unit);
    let domains = u.class().mDomains;
    let count = h.waypoints(unit);
    let mut goal_point = if count != 0 && (domains & MV_MOVEMENT_SEA == 0 || count >= 3) {
        h.waypoint(unit, 0)
    } else if goal != 0 {
        h.goal_destination(goal, unit)
    } else {
        h.position(unit)
    };
    let mut tile = floor_tile(goal_point);
    let unit_tile = u.tile();
    if tile == unit_tile {
        u.cx_mut().mHasArrived = true;
        u.cx_mut().mCollisionResolutionCount = 0.0;
        h.set_blocking_unit(unit, 0);
        u.set_collision_state(8);
        return;
    }
    h.clear_bounded_tiles(unit);
    let index = if class_rec(h, class).mDomains & (MV_MOVEMENT_AIR | MV_MOVEMENT_SEA) == 0 {
        if count > 7 {
            7
        } else {
            count - 1
        }
    } else {
        let mut found = count - 1;
        for i in 0..count {
            let p = h.waypoint(unit, i);
            if h.distance_squared_3d(unit, p) >= MV_MIN_WAYPOINT_DISTANCE_SQUARED {
                found = i;
                break;
            }
        }
        found
    };
    let carrier = h.blocking_unit(unit);
    if carrier != 0 {
        let c = U::of(h, carrier);
        if c.class().mInnerCollisionSphere >= 2.0 {
            for t in h.unit_tiles(carrier, 0.0) {
                h.push_bounded_tile(unit, t);
            }
        } else {
            h.push_bounded_tile(unit, c.tile());
        }
    }
    u.cx_mut().mCollisionResolutionCount = 0.0;
    h.set_blocking_unit(unit, 0);
    let from = [floor_unsigned(position.x) as i32, floor_unsigned(position.y) as i32];
    let to = [floor_unsigned(goal_point.x) as i32, floor_unsigned(goal_point.y) as i32];
    let mut line = h.tile_line(from, to, false, true);
    if (from[0] - to[0]).abs() == 1 && (from[1] - to[1]).abs() == 1 {
        line.push([from[0], to[1]]);
        line.push([to[0], from[1]]);
    }
    if count > 1 {
        line.push(floor_tile(h.waypoint(unit, 0)));
    }
    goal_point = if index >= 0 { h.waypoint(unit, index) } else { h.position(unit) };
    tile = floor_tile(goal_point);
    for t in line {
        if t[0] != tile[0] || t[1] != tile[1] {
            h.push_bounded_tile(unit, t);
        }
    }
    let unit_domains = U::of(h, unit).class().mDomains;
    let kind = if unit_domains & MV_MOVEMENT_SEA == 0 {
        if unit_domains & MV_MOVEMENT_SPACE != 0 {
            16
        } else {
            1
        }
    } else {
        2
    };
    let mut path = h.find_bounded_path(unit, kind, tile);
    if path.len() > 1 {
        let last = *path.last().expect("a path");
        if floor(last.x) == tile[0] && floor(last.y) == tile[1] {
            path.pop();
            path.push(goal_point);
            h.merge_waypoints(unit, &mut path);
            h.drop_path(unit);
            let here = U::of(h, unit).tile();
            let start = usize::from(floor(path[0].x) == here[0] && floor(path[0].y) == here[1]);
            for p in path.iter().take(path.len() - 1).skip(start) {
                h.add_waypoint(unit, *p, false);
            }
            arrived_moving(h, unit);
            return;
        }
        let count = h.waypoints(unit);
        if count > 1 {
            let p = h.waypoint(unit, count - 2);
            if floor(p.x) == tile[0] && floor(p.y) == tile[1] {
                h.merge_waypoints(unit, &mut path);
                h.drop_path(unit);
                for p in &path {
                    h.add_waypoint(unit, *p, false);
                }
                arrived_moving(h, unit);
                return;
            }
        }
    }
    let u = U::of(h, unit);
    if u.cx().mCollisionState == 2 {
        h.set_blocking_unit(unit, 0);
        u.cx_mut().mCollisionResolutionCount = 0.0;
        h.set_velocity(unit, 0.0);
        if u.cx().mCurrentPhysicsAction != 6 {
            u.cx_mut().mCurrentPhysicsAction = 6;
        }
        let action = if h.gathers_wood(goal) { MV_ACTION_FIND_NEW_RESOURCE } else { MV_ACTION_IDLE };
        h.start_action(unit, action);
        let unit_class = U::of(h, unit).rec.class_obj;
        h.action_changed(unit_class, unit);
        U::of(h, unit).set_collision_state(0);
    } else {
        u.cx_mut().mCollisionState = 2;
    }
}

/// `SetHasArrived(false)` and collision state 1 after a new path.
fn arrived_moving<H: MoveHost + ?Sized>(h: &mut H, unit: u64) {
    let u = U::of(h, unit);
    u.cx_mut().mHasArrived = false;
    u.set_collision_state(1);
}

/// `FindPath`'s melee sidestep: retarget, else step to the side with fewer units beside the unit and its goal.
fn sidestep<H: MoveHost + ?Sized>(h: &mut H, class: u64, u: &U, goal: u64, attack: u64, mut goal_point: Ge3DPoint) {
    let unit = u.obj;
    let target = h.attack_target(attack);
    let target_is_wall = U::of(h, target).class().mIsWall;
    if target_is_wall || !h.goal_flag(goal, false) {
        h.ignore_target(unit, target);
        if h.melee_retarget(unit, target, attack) {
            h.set_sidestep_count(attack, 0);
            return;
        }
    }
    let u = U::of(h, unit);
    u.cx_mut().mCollisionResolutionCount = 0.0;
    h.set_blocking_unit(unit, 0);
    let mut position = h.position(unit);
    let radius = U::of(h, unit).class().mInnerCollisionSphere;
    let mut side = GeVector::new(-(goal_point.y - position.y), goal_point.x - position.x, 0.0);
    let mut left = 0i32;
    let mut right = 0i32;
    side.normalize();
    let scale = radius + radius;
    side.x *= scale;
    side.y *= scale;
    side.z *= scale;
    // the left side: beside the unit, then beside the goal
    position.y += side.y;
    position.x += side.x;
    let left_squeezed = side_count(h, &u, floor_tile(position), &mut left) == Some(true);
    goal_point.y += side.y;
    goal_point.x += side.x;
    if !left_squeezed {
        side_count(h, &u, floor_tile(goal_point), &mut left);
    }
    // the right side (a squeeze beside the unit settles it)
    position.y -= side.y + side.y;
    position.x -= side.x + side.x;
    let squeezed = side_count(h, &u, floor_tile(position), &mut right) == Some(true);
    if !squeezed {
        goal_point.y -= side.y + side.y;
        goal_point.x -= side.x + side.x;
        side_count(h, &u, floor_tile(goal_point), &mut right);
    }
    if right < left && right != MV_SQUEEZED {
        h.add_waypoint(unit, position, false);
    } else if left != MV_SQUEEZED {
        position.y += side.y + side.y;
        position.x += side.x + side.x;
        h.add_waypoint(unit, position, false);
        goal_point.y += side.y + side.y;
        goal_point.x += side.x + side.x;
    } else {
        h.set_sidestep_count(attack, 1);
        find_path(h, class, unit);
        return;
    }
    h.add_waypoint(unit, goal_point, false);
    h.add_path_attempts(goal, 8);
    let n = h.sidestep_count(attack);
    h.set_sidestep_count(attack, n.wrapping_add(1));
}

/// Walks a tile's unit nodes while `stop` is false: the last node's unit becomes the blocker, and a unit that does
/// not let the walker pass stops the walk (the C++'s `for (node; node && !flag; ...)`).
fn blocked_on<H: MoveHost + ?Sized>(h: &mut H, tile: u64, unit: u64, blocker: &mut u64) -> bool {
    for other in h.tile_units(tile) {
        *blocker = other;
        if other != 0 && !h.passable_by(other, unit) {
            return true;
        }
    }
    false
}

/// `EEUnitClass::NextWaypoint(unit)`: a land unit whose next waypoint's tile is blocked, or whose diagonal step
/// cuts two blocked corners, notes the blocker (collision state 2); then the unit's `NextWaypoint()`.
pub fn next_waypoint<H: MoveHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut first_corner = false;
    let mut second_corner = false;
    let mut blocker = 0u64;
    let mut blocked = false;
    if class_rec(h, class).mDomains & MV_MOVEMENT_LAND != 0 {
        if let Some(next) = h.second_waypoint(unit) {
            let tile = h.map_tile(floor(next.x), floor(next.y));
            if tile != 0 {
                blocked = blocked_on(h, tile, unit, &mut blocker);
            }
            if !blocked {
                let here = U::of(h, unit).rec.location;
                if (floor(here[0]) - floor(next.x)).abs() == 1 && (floor(here[1]) - floor(next.y)).abs() == 1 {
                    let tile = h.map_tile(floor(here[0]), floor(next.y));
                    first_corner = tile == 0 || blocked_on(h, tile, unit, &mut blocker);
                    if first_corner {
                        let tile = h.map_tile(floor(next.x), floor(here[1]));
                        second_corner = tile == 0 || blocked_on(h, tile, unit, &mut blocker);
                    }
                }
            }
            if blocked || first_corner && second_corner {
                h.set_blocking_unit(unit, blocker);
                U::of(h, unit).set_collision_state(2);
            }
        }
    }
    h.next_waypoint(unit);
}

/// `EEUnitClass::BaseHasArrived(unit, value)`: whether the unit has reached its current waypoint, given the
/// distance left.
pub fn base_has_arrived<H: MoveHost + ?Sized>(h: &mut H, class: u64, unit: u64, value: f32) -> bool {
    let u = U::of(h, unit);
    if u.cx().mCollisionState == 8 {
        return true;
    }
    let count = h.waypoints(unit);
    let mut location = h.next_waypoint_point(unit);
    location.z = 0.0;
    let goal = h.goal(unit);
    if count > 1
        || (goal != 0
            && ((h.queue_goals(unit) > 1 && h.goal_kind(goal) == MV_GOAL_LOCATION)
                || h.goal_kind(goal) == MV_GOAL_EXPLORE))
    {
        if value <= class_rec(h, class).mInnerCollisionSphere {
            let tile = U::of(h, unit).tile();
            tile == floor_tile(location) || value < 0.05
        } else {
            false
        }
    } else if count == 1 {
        value < 0.05
    } else {
        false
    }
}

/// `EEUnitClass::AddWaypointAt(unit, x, y, z, snap)`: a waypoint `z` above the tile's ground (on water at `z`);
/// off the map it snaps to the closest tile or is dropped.
pub fn add_waypoint_at<H: MoveHost + ?Sized>(h: &mut H, unit: u64, x: f32, y: f32, z: f32, snap: bool) {
    let tile = h.map_tile(floor(x), floor(y));
    let height = if tile != 0 {
        h.tile_height(tile)
    } else {
        if !snap {
            return;
        }
        match h.closest_tile_height(Ge3DPoint { x, y, z }) {
            Some(height) => height,
            None => return,
        }
    };
    let z = if height >= 0.0 { height + z } else { z };
    h.add_waypoint(unit, Ge3DPoint { x, y, z }, snap);
}

/// `EEUnitClass::PathToward(unit, from, to, snap)`: waypoints along the line at turning-radius steps and cruise
/// altitude (a near point: one waypoint).
pub fn path_toward<H: MoveHost + ?Sized>(h: &mut H, unit: u64, from: Ge3DPoint, to: Ge3DPoint, snap: bool) {
    let step = h.max_velocity(unit) * 2.0 / h.max_angular_velocity(unit);
    let height = U::of(h, unit).class().mCruiseAltitude;
    let direction = GeVector::new(to.x - from.x, to.y - from.y, 0.0);
    if direction.magnitude_squared() > MV_NEAR * MV_NEAR {
        let mut points = Vec::new();
        from.interpolate_to(to, step, |p| points.push(p));
        for p in points.iter().skip(1) {
            add_waypoint_at(h, unit, p.x, p.y, height, snap);
        }
    } else {
        add_waypoint_at(h, unit, to.x, to.y, to.z, snap);
    }
}

/// `EEUnitClass::PathToGoal(unit)`: a path request from the unit's tile to its goal (its paths cancelled when it
/// stands there), the start heading when unset; physics action 4 when it has no waypoints afterwards.
pub fn path_to_goal<H: MoveHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let goal = h.goal(unit);
    h.drop_path(unit);
    let goal_point = h.goal_destination(goal, unit);
    if class_rec(h, class).mDomains & (MV_MOVEMENT_SEA | MV_MOVEMENT_SPACE) != 0 {
        h.update_water_state(unit);
    }
    if U::of(h, unit).rec.stats_max_velocity > 0.0 {
        let location = h.location(unit);
        let (x, y, z) = (location.x(), location.y(), location.z());
        if x == goal_point.x && y == goal_point.y && z == goal_point.z {
            h.cancel_paths(unit);
        } else {
            let position = Ge3DPoint { x, y, z: 0.0 };
            let u = U::of(h, unit);
            if u.cx().mInitialContactHeading == MV_HEADING_UNSET {
                let direction = location.apply_vector(GeVector::new(0.0, 1.0, 0.0));
                let heading = um_arctan2(-direction.x, direction.y);
                h.set_initial_contact_location(unit, position);
                if u.cx().mInitialContactHeading != heading {
                    u.cx_mut().mInitialContactHeading = heading;
                }
            }
            let mut revs = -1;
            let mut arg8 = false;
            let graph;
            if h.goal_kind(goal) == MV_GOAL_LOCATION {
                graph = !h.auto_waypoint(goal) && !h.goal_flag(goal, true);
                if h.group_path_state(goal) == 1 {
                    revs = 15;
                    h.set_group_path_state(goal, 2);
                } else {
                    arg8 = !graph;
                }
            } else {
                graph = !h.goal_flag(goal, true);
            }
            h.request_path(unit, goal, floor_tile(goal_point), revs, graph, arg8);
        }
    }
    if h.waypoints(unit) == 0 {
        let u = U::of(h, unit);
        if u.cx().mCurrentPhysicsAction != 4 {
            u.cx_mut().mCurrentPhysicsAction = 4;
        }
    }
}

/// The flight of `Flee` and `FleeAttacker` from `enemy`: the first standable spot of up to 8 directions, true when
/// one was found (the goal set, the unit preparing to move).
fn flee_from<H: MoveHost + ?Sized>(h: &mut H, class: u64, unit: u64, enemy: u64, old_goal: u64) -> bool {
    let distance;
    let mut direction;
    if U::of(h, unit).rec.player_id != 0 {
        distance = lrintf(h.line_of_sight(unit) * MV_FLEE_DISTANCE_SCALE);
        direction = h.direction_to(unit, enemy);
        direction = h.turn_away(direction, true);
    } else {
        distance = h.random_1_2();
        direction = h.direction_to(unit, enemy);
        direction = h.turn_away(direction, false);
    }
    let mut count = 0;
    let mut destination;
    loop {
        let here = U::of(h, unit).rec.location;
        destination = h.move_in_direction(here[0], here[1], direction, distance);
        direction = direction.wrapping_add(1) % 8;
        count += 1;
        if count > 7 {
            break;
        }
        if h.can_stand_on([floor(destination[0]), floor(destination[1])], unit) {
            break;
        }
    }
    if count <= 7 {
        let keep = old_goal != 0 && h.goal_kind(old_goal) == MV_GOAL_EXPLORE;
        h.flee_goal(unit, destination, keep);
        h.start_action(unit, MV_ACTION_PREPARE_TO_MOVE);
        h.action_changed(class, unit);
        return true;
    }
    false
}

/// Idles the unit through its own class (`mUnitClass->ActionChanged`).
fn idle<H: MoveHost + ?Sized>(h: &mut H, unit: u64) {
    h.start_action(unit, MV_ACTION_IDLE);
    let unit_class = U::of(h, unit).rec.class_obj;
    h.action_changed(unit_class, unit);
}

/// `EEUnitClass::Flee(unit)`: runs from the unit's pursuer, else idles.
pub fn flee<H: MoveHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let old_goal = h.goal(unit);
    let enemy = h.pursuer_of(unit);
    if enemy != 0 && flee_from(h, class, unit, enemy, old_goal) {
        return;
    }
    idle(h, unit);
}

/// `EEUnitClass::FleeAttacker(unit)`: runs from the last attacker (else the pursuer; neither: `Flee`), then
/// releases the last attacker.
pub fn flee_attacker<H: MoveHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let old_goal = h.goal(unit);
    let mut enemy = h.last_attacker(unit);
    if enemy == 0 {
        enemy = h.pursuer_of(unit);
    }
    if enemy != 0 {
        if !flee_from(h, class, unit, enemy, old_goal) {
            idle(h, unit);
        }
    } else {
        flee(h, class, unit);
    }
    h.clear_last_attacker(unit);
}

#[cfg(test)]
#[path = "cls_move_tests.rs"]
mod tests;
