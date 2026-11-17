//! The flying kinds' heights of `EEUnitClass` (R5b.4, the thirty-third round): `GetHeightAt`, the height a unit
//! flies at towards a point, whose seven bodies (the helicopter's, the helicopter priest's and transport's, the
//! spaceship's and space transport's, the landable's and the air animal's: EEUnitClassAir.cpp,
//! EEUnitClassTransports.cpp, EEUnitClassPeople.cpp until this round) were one tile walk with three ways to clamp
//! it, and the dispatcher's kind tests deepest first.
//!
//! With them the landable's flight plans (`GetNextFlightState`, `ApproachRunway`, `TakeOffFromRunway`), the
//! landing kinds' arrivals (`LandableHasArrived`, `AirAnimalHasArrived`) and trigger answers
//! (`LandableEvaluateCondition`, `AirAnimalEvaluateCondition`) over the [`FlightHost`] and [`ConditionHost`].
//!
//! The walk: the highest corner of every tile on the way (the line from the unit to the point; for a landable or an
//! air animal circling, the tiles around the point ahead on its turning circle) plus a base, over the point's
//! height. The line, the tiles near a point and the terrain mesh are the host's (the C++ leaves keep their CBTL and
//! UUTL records).
//!
//! Determinism (RUST_PLAN 2.1): the C++'s float operations in order (the corners in the C++'s order, each compared
//! as it comes), `UMFloor` for the tiles.

// `c_long` is 32 bits on Windows and 64 on LP64 (the casts are needed there); the class records the entry points take
// are live objects the C++ hands over (as every unit record read in place); the C++'s statements as written (its
// float constants' bits, `x = a + x`, a default point set field by field).
#![allow(
    clippy::unnecessary_cast,
    clippy::not_unsafe_ptr_arg_deref,
    clippy::approx_constant,
    clippy::assign_op_pattern,
    clippy::field_reassign_with_default
)]

use crate::units::kinds::*;
use crate::units::unit_class_fields::EeUnitsUnitClassFields;
use crate::units::uu::EeUnitsUuUnit;
use crate::units::uu_tiles::Tile;
use ee_base::math::types::{Ge3DPoint, GeTransformation, GeVector};
use ee_base::math::um::floor;

/// The world the heights ask (the game's: cls_host.rs).
pub trait AirHost {
    /// a unit's records (the unit utilities' read)
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit;
    /// a complex unit's `mPhysicalModel.GetLocation()`
    fn location(&mut self, unit: u64) -> GeTransformation;
    /// the unit's `mStats`' `GetMaxVelocity()`, `GetMaxAngularVelocity()`
    fn turn_speeds(&mut self, unit: u64) -> (f32, f32);
    /// `gWorld.mMap->GetTile(x, y)` (its unsigned bounds test; 0: off the map), the tile's `GetHeight()`,
    /// `mTerrainMesh.GetZ(x, y)`
    fn map_tile(&mut self, x: i32, y: i32) -> u64;
    fn tile_height(&mut self, tile: u64) -> f32;
    fn mesh_z(&mut self, x: i32, y: i32) -> f32;
    /// `EEGetTileLine(x1, y1, x2, y2, &out, 1, 0)`; `EEGetTilesNearPoint(point, distance, out)`
    fn tile_line(&mut self, from: Tile, to: Tile) -> Vec<Tile>;
    fn tiles_near_point(&mut self, point: Ge3DPoint, distance: f32) -> Vec<Tile>;
}

/// How a body clamps the walk's height (the ceiling is 12 in every body).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Clamp {
    /// `if (h > 12) h = 12`
    Ceiling,
    /// the helicopter's: `if (h >= 12) h = 12; if (h <= base) h = base`
    CeilingAndBase,
    /// the spaceships': `h = min(h, 12); h = max(h, base)`
    MinMax,
}

const CEILING: f32 = 12.0;
/// the look-ahead beyond the turning circle (the landable's and the air animal's 2)
const AHEAD: f32 = 2.0;

/// `GetHeightAt`'s dispatch over the class's kinds (deepest first): the point's height for the kinds that do not fly.
pub fn height_at<H: AirHost + ?Sized>(
    h: &mut H,
    cls: *const EeUnitsUnitClassFields,
    unit: u64,
    location: Ge3DPoint,
) -> f32 {
    // SAFETY: a live class's record
    let c = unsafe { &*cls };
    let category = c.mCategory as usize;
    let kinds = if category < 256 { EE_UNIT_CLASS_KINDS[category] } else { 0 };
    let cruise = c.mCruiseAltitude;
    if kinds & EE_CLASS_KIND_AIR_ANIMAL != 0 {
        // the air animal's body is the landable's twin
        return ground_height(h, unit, location);
    }
    if kinds & (EE_CLASS_KIND_HELICOPTER_PRIEST | EE_CLASS_KIND_HELICOPTER_TRANSPORT) != 0 {
        return line_height(h, unit, location, cruise, Clamp::Ceiling);
    }
    if kinds & EE_CLASS_KIND_SPACE_TRANSPORT != 0 {
        return line_height(h, unit, location, cruise, Clamp::MinMax);
    }
    if kinds & EE_CLASS_KIND_HELICOPTER != 0 {
        return line_height(h, unit, location, cruise, Clamp::CeilingAndBase);
    }
    if kinds & EE_CLASS_KIND_LANDABLE != 0 {
        return ground_height(h, unit, location);
    }
    if kinds & EE_CLASS_KIND_SPACESHIP != 0 {
        return line_height(h, unit, location, cruise, Clamp::MinMax);
    }
    location.z
}

/// The walk: each tile on the map raises the height to its corners' plus `base`.
fn walk<H: AirHost + ?Sized>(h: &mut H, tiles: &[Tile], base: f32, mut height: f32) -> f32 {
    for t in tiles {
        let (x, y) = (t[0], t[1]);
        if h.map_tile(x, y) == 0 {
            continue;
        }
        for (cx, cy) in [(x, y), (x.wrapping_add(1), y), (x, y.wrapping_add(1)), (x.wrapping_add(1), y.wrapping_add(1))]
        {
            let z = h.mesh_z(cx, cy) + base;
            if z > height {
                height = z;
            }
        }
    }
    height
}

/// The line bodies (helicopters, the helicopter priest and transport, spaceships, the space transport): the
/// cruise altitude over the corners on the line to the point.
fn line_height<H: AirHost + ?Sized>(h: &mut H, unit: u64, location: Ge3DPoint, base: f32, clamp: Clamp) -> f32 {
    let at = h.location(unit);
    let from = [floor(at.m[0][3]), floor(at.m[1][3])];
    let tiles = h.tile_line(from, [floor(location.x), floor(location.y)]);
    let mut height = walk(h, &tiles, base, location.z);
    match clamp {
        Clamp::Ceiling => {
            if height > CEILING {
                height = CEILING;
            }
        }
        Clamp::CeilingAndBase => {
            if height >= CEILING {
                height = CEILING;
            }
            if height <= base {
                height = base;
            }
        }
        Clamp::MinMax => {
            // the C++ min/max macros
            height = if height < CEILING { height } else { CEILING };
            height = if height > base { height } else { base };
        }
    }
    height
}

/// The landable's and the air animal's body: the point's height above its tile over the corners ahead (the turning
/// circle's tiles while circling, else the line to the point), at most 12.
fn ground_height<H: AirHost + ?Sized>(h: &mut H, unit: u64, location: Ge3DPoint) -> f32 {
    let at = h.location(unit);
    let mut position = Ge3DPoint { x: at.m[0][3], y: at.m[1][3], z: at.m[2][3] };
    let height = location.z;
    let tile = h.map_tile(floor(location.x), floor(location.y));
    let base = if tile != 0 && h.tile_height(tile) >= 0.0 { height - h.tile_height(tile) } else { height };
    let view = h.unit(unit);
    // SAFETY: a live complex unit's record
    let state = unsafe { (*view.cx).mCurrentPhysicsOrder };
    let tiles = if state == 0 || state == 1 {
        let (velocity, angular) = h.turn_speeds(unit);
        let distance = velocity / angular;
        let step = GeVector { x: if state != 0 { -distance } else { distance }, y: 0.0, z: 0.0 };
        let offset = at.apply_vector(step);
        position.x += offset.x;
        position.y += offset.y;
        position.z += offset.z;
        h.tiles_near_point(position, AHEAD + distance)
    } else {
        h.tile_line([floor(position.x), floor(position.y)], [floor(location.x), floor(location.y)])
    };
    let mut height = walk(h, &tiles, base, height);
    if height > CEILING {
        height = CEILING;
    }
    height
}

/// The world the landable's flight plans ask and change (the game's: cls_host.rs).
pub trait FlightHost: AirHost {
    /// the unit's `mHomeBase` (0: none), `mWaypoints.size()`, `GetNextWaypointPoint(out)`
    fn home_base(&mut self, unit: u64) -> u64;
    fn waypoints(&mut self, unit: u64) -> i32;
    fn next_waypoint(&mut self, unit: u64) -> Ge3DPoint;
    /// where the goal (no waypoints left) leads: a location or ground attack goal's ground target, an attacked
    /// unit's location (a simple unit's tile centre at its tile's height), a patrol target's location, the explore
    /// goal's tile centre; None for no goal, no attacked unit or another goal
    fn flight_target(&mut self, unit: u64) -> Option<Ge3DPoint>;
    /// `unit->DropPath()`; the class's `AddWaypointAt(unit, x, y, z, snap)`, `PathToward(unit, from, to, snap)`,
    /// `FlyToGoal(point, unit)`
    fn drop_path(&mut self, unit: u64);
    fn add_waypoint(&mut self, class: u64, unit: u64, point: Ge3DPoint, snap: bool);
    fn path_toward(&mut self, class: u64, unit: u64, from: Ge3DPoint, to: Ge3DPoint, snap: bool);
    fn fly_to_goal(&mut self, class: u64, point: Ge3DPoint, unit: u64);
    /// the runway's move goal: a new location goal at the point set as the unit's goal, the queue cleared unless
    /// its goal explores (then kept behind it)
    fn runway_goal(&mut self, unit: u64, point: Ge3DPoint);
}

/// the approach's 30 degrees (0x447040)
const APPROACH_ANGLE: f32 = 0.5235988;
/// the touch-down height (0x447048)
const TOUCH_DOWN: f32 = 0.1;
const TYPE_NAVY_YARD: u32 = 0x41;

/// A home base's facing as a runway direction: 0 +y, 1 -y, 2 +x, any other -x.
pub fn runway_direction(facing: u32) -> (i32, i32) {
    match facing {
        0 => (0, 1),
        1 => (0, -1),
        2 => (1, 0),
        _ => (-1, 0),
    }
}

/// `GetNextFlightState`: 0 (left) or 1 (right) when the next waypoint or goal lies inside the turning circle beside or
/// behind the plane, else 5.
pub fn next_flight_state<H: FlightHost + ?Sized>(h: &mut H, unit: u64) -> u32 {
    let (velocity, angular) = h.turn_speeds(unit);
    let radius = velocity / angular;
    let radius2 = radius * radius;
    let target = if h.waypoints(unit) > 0 {
        h.next_waypoint(unit)
    } else {
        match h.flight_target(unit) {
            Some(p) => p,
            None => return 5,
        }
    };
    let inverse = h.location(unit).inverse();
    let mut offset = inverse.apply_point(target);
    offset.z = 0.0;
    let left = Ge3DPoint { x: -radius, y: 0.0, z: 0.0 };
    let right = Ge3DPoint { x: radius, y: 0.0, z: 0.0 };
    let ahead = Ge3DPoint { x: 0.0, y: -radius, z: 0.0 };
    let near_left = left.distance_to_squared(offset) <= radius2;
    let near_right = right.distance_to_squared(offset) <= radius2;
    let near_ahead = ahead.distance_to_squared(offset) <= radius2;
    if near_left || near_right || near_ahead {
        if offset.x < 0.0 {
            0
        } else {
            1
        }
    } else {
        5
    }
}

/// `ApproachRunway`: a land plane's approach along (dx, dy): onto the line, the descent and the touch-down at its
/// airport (a navy yard: its pier), then the airport its move goal.
pub fn approach_runway<H: FlightHost + ?Sized>(
    h: &mut H,
    class: u64,
    cls: *const EeUnitsUnitClassFields,
    unit: u64,
    dx: i32,
    dy: i32,
) {
    // SAFETY: a live class's record
    let cruise = unsafe { (*cls).mCruiseAltitude };
    let base = h.home_base(unit);
    let at = h.location(unit);
    let mut position = Ge3DPoint { x: at.m[0][3], y: at.m[1][3], z: at.m[2][3] };
    let base_at = h.location(base);
    let (mut y, mut x) = (base_at.m[1][3], base_at.m[0][3]);
    let base_view = h.unit(base);
    // SAFETY: a live class's record
    let navy_yard = unsafe { (*base_view.class_fields).mUnitType } as u32 == TYPE_NAVY_YARD;
    let (far, ground, near, last, low, flare, snap) = if navy_yard {
        x = dx as f32 * 1.5 + x;
        y = dy as f32 * 1.5 + y;
        (3.0f32, 0.0f32, 2.0f32, 0.8f32, 0.3f32, 0.05f32, true)
    } else {
        (7.0, TOUCH_DOWN, 3.0, 1.0, 0.5, 0.15, false)
    };
    h.drop_path(unit);
    let (velocity, angular) = h.turn_speeds(unit);
    let count = crate::units::uu_tiles::ceil(velocity / angular);
    let (fdx, radius, fdy) = (dx as f32, count as f32, dy as f32);
    let start_x = fdx * far + x;
    let run = radius * 1.5;
    let start_y = fdy * far + y;
    let turn = run / ee_base::math::um::um_tan(APPROACH_ANGLE) + run;
    let (offset_y, offset_x) = (fdy * turn, fdx * turn);
    let dist_y = position.y - (start_y - offset_y);
    let dist_x = position.x - (start_x - offset_x);
    if dist_y * dist_y + dist_x * dist_x <= radius * 2.5 {
        let waypoint = Ge3DPoint { x: start_x, y: start_y, z: cruise };
        h.path_toward(class, unit, position, waypoint, true);
        position = waypoint;
    }
    let waypoint = Ge3DPoint { x: offset_x + start_x, y: offset_y + start_y, z: cruise * last };
    h.path_toward(class, unit, position, waypoint, true);
    h.add_waypoint(class, unit, Ge3DPoint { x: fdx * run + start_x, y: fdy * run + start_y, z: cruise * low }, snap);
    h.add_waypoint(class, unit, Ge3DPoint { x: start_x, y: start_y, z: cruise * flare }, false);
    h.add_waypoint(class, unit, Ge3DPoint { x: fdx * near + x, y: fdy * near + y, z: ground }, false);
    h.add_waypoint(class, unit, Ge3DPoint { x: fdx + x, y: fdy + y, z: ground }, false);
    h.add_waypoint(class, unit, Ge3DPoint { x, y, z: ground }, false);
    let tile = h.map_tile(floor(x), floor(y));
    let mut height = h.tile_height(tile);
    if height < 0.0 {
        height = 0.0;
    }
    h.runway_goal(unit, Ge3DPoint { x, y, z: height });
}

/// `TakeOffFromRunway`: rolls 3 tiles low along (dx, dy), climbs to 3/4 of the cruise altitude 7 tiles out, then
/// flies to its goal.
pub fn take_off_from_runway<H: FlightHost + ?Sized>(
    h: &mut H,
    class: u64,
    cls: *const EeUnitsUnitClassFields,
    unit: u64,
    dx: i32,
    dy: i32,
) {
    // SAFETY: a live class's record
    let cruise = unsafe { (*cls).mCruiseAltitude };
    let at = h.location(unit);
    let (x, y) = (at.m[0][3], at.m[1][3]);
    h.drop_path(unit);
    h.add_waypoint(class, unit, Ge3DPoint { x: dx as f32 * 3.0 + x, y: dy as f32 * 3.0 + y, z: TOUCH_DOWN }, false);
    let end_y = dy as f32 * 7.0 + y;
    let end_x = dx as f32 * 7.0 + x;
    h.add_waypoint(class, unit, Ge3DPoint { x: end_x, y: end_y, z: cruise * 0.75 }, false);
    h.fly_to_goal(class, Ge3DPoint { x: end_x, y: end_y, z: cruise * 0.75 }, unit);
}

/// The landing kinds' trigger questions (the game's: cls_host.rs).
pub trait ConditionHost: FlightHost {
    /// the unit's current command's type (0: none)
    fn command_type(&mut self, unit: u64) -> i32;
    /// `GetNextWaypointPoint(out)`: the point when it answers 0
    fn next_flight_point(&mut self, unit: u64) -> Option<Ge3DPoint>;
    /// `GetPosition(out)` (virtual)
    fn position(&mut self, unit: u64) -> Ge3DPoint;
    /// `GetGroundHeight(point)`: the point's tile's height (the closest tile's off the map), 0 under water
    fn ground_height(&mut self, point: Ge3DPoint) -> f32;
    /// the class's `IsBombOnTarget(unit)`, `IsAwayFromGoal(unit)` (C++ members with their UCBD notes), its
    /// `mProjectileClass`'s category (-1: none)
    fn bomb_on_target(&mut self, class: u64, unit: u64) -> bool;
    fn away_from_goal(&mut self, class: u64, unit: u64) -> bool;
    fn projectile_category(&mut self, class: u64) -> i32;
    /// the unit's attack goal has a living target; its goal has a location (`EEGoalHasLocation`); its landing or
    /// take-off request is queued at its air traffic control
    fn attack_target_alive(&mut self, unit: u64) -> bool;
    fn goal_has_location(&mut self, unit: u64) -> bool;
    fn request_queued(&mut self, unit: u64) -> bool;
}

const ACTION_BRAKE: i32 = 1014;
const CATEGORY_BOMB: i32 = 0xb;
const CATEGORY_NAVAL_BUILDING: u32 = 0x15;
const CATEGORY_SPACE_BUILDING: u32 = 0x20;
const MOVEMENT_SPACE: u8 = 8;
// the triggers the landing kinds answer
const TRIGGER_FLIGHT_TIME_ALARM: u32 = 1000;
const TRIGGER_LANDING_CLEARANCE_GRANTED: u32 = 1001;
const TRIGGER_HOLDING_PATTERN_COMPUTED: u32 = 1002;
const TRIGGER_AIRPORT_DESTROYED: u32 = 1003;
const TRIGGER_SECONDARY_TARGET_FOUND: u32 = 1004;
const TRIGGER_AIRTRAFFIC_CONTROLLER_MOVED: u32 = 1007;
const TRIGGER_REQUEST_STILL_VALID: u32 = 1008;
const TRIGGER_UNIT_IN_WEAPON_RANGE: u32 = 2013;
const TRIGGER_DYING_ANIMATION_FINISHED: u32 = 6000;
const TRIGGER_MOVE_PREPARATION_COMPLETE: u32 = 8000;
const TRIGGER_WAYPOINT_ON_GROUND: u32 = 9008;
const TRIGGER_WAYPOINT_IN_AIR: u32 = 9009;
const TRIGGER_WAYPOINT_IS_HIGHER_ELEVATION: u32 = 9010;
const TRIGGER_WAYPOINT_IS_SAME_ELEVATION: u32 = 9011;
const TRIGGER_WAYPOINT_IS_LOWER_ELEVATION: u32 = 9012;

/// A unit's plain values the conditions read (from its records, read in place now).
#[derive(Clone, Copy, Debug, Default)]
struct Plain {
    tile: u64,
    velocity: f32,
    arrived: bool,
    ammunition: u32,
    flight_time: i32,
    clearance: bool,
    hit_points: i32,
    domains: u8,
    traffic: i32,
    category: u32,
}

fn plain<H: AirHost + ?Sized>(h: &mut H, unit: u64) -> Plain {
    let v = h.unit(unit);
    // SAFETY: a live unit's records, read now
    unsafe {
        let mut p = Plain { tile: v.tile, ..Default::default() };
        if !v.cx.is_null() {
            let cx = &*v.cx;
            p.velocity = cx.mVelocity;
            p.arrived = crate::units::unit_fields::bool_byte(&cx.mHasArrived) != 0;
            p.ammunition = cx.mAmmunition as u32;
            p.flight_time = cx.mFlightTime as i32;
            p.clearance = crate::units::unit_fields::bool_byte(&cx.mIsClearanceGranted) != 0;
        }
        if !v.unit.is_null() {
            p.hit_points = (*v.unit).mHitPoints as i32;
        }
        if !v.class_fields.is_null() {
            let c = &*v.class_fields;
            p.domains = c.mDomains;
            p.traffic = c.mAirTrafficType as i32;
            p.category = c.mCategory as u32;
        }
        p
    }
}

/// `LandableHasArrived` (`animal` false) and `AirAnimalHasArrived`: within 0.5 of its next waypoint; braking onto
/// its last one (not a carrier plane's), whether it has (nearly) stopped.
pub fn landing_has_arrived<H: ConditionHost + ?Sized>(
    h: &mut H,
    cls: *const EeUnitsUnitClassFields,
    unit: u64,
    value: f32,
    animal: bool,
) -> bool {
    // SAFETY: a live class's record
    let traffic = unsafe { (*cls).mAirTrafficType } as i32;
    let count = h.waypoints(unit);
    let at = h.location(unit);
    let position = Ge3DPoint { x: at.m[0][3], y: at.m[1][3], z: 0.0 };
    let mut target = h.next_waypoint(unit);
    target.z = 0.0;
    if animal {
        if h.command_type(unit) == ACTION_BRAKE && count <= 1 && traffic != 2 {
            return value == 0.0;
        }
    } else if h.command_type(unit) == ACTION_BRAKE && count <= 1 && traffic != 2 && traffic != 4 {
        return value <= 0.05;
    }
    target.distance_to_squared(position) <= 0.25
}

/// `LandableEvaluateCondition`: the plane's flight triggers; None for the others (the common unit class's).
pub fn landable_condition<H: ConditionHost + ?Sized>(
    h: &mut H,
    class: u64,
    cls: *const EeUnitsUnitClassFields,
    unit: u64,
    event: u32,
) -> Option<i32> {
    // SAFETY: a live class's record
    let ammunition = unsafe { (*cls).mAmmunition } as u32;
    let flag = match event {
        TRIGGER_DYING_ANIMATION_FINISHED => {
            let p = plain(h, unit);
            p.domains & MOVEMENT_SPACE != 0 || p.velocity == 0.0
        }
        TRIGGER_MOVE_PREPARATION_COMPLETE => h.waypoints(unit) > 0,
        TRIGGER_WAYPOINT_ON_GROUND
        | TRIGGER_WAYPOINT_IN_AIR
        | TRIGGER_WAYPOINT_IS_HIGHER_ELEVATION
        | TRIGGER_WAYPOINT_IS_SAME_ELEVATION
        | TRIGGER_WAYPOINT_IS_LOWER_ELEVATION => {
            // `GetNextFlightPoint`: the next waypoint and the climb to it
            let position = h.position(unit);
            match h.next_flight_point(unit) {
                None => false,
                Some(target) => {
                    let climb = target.z - position.z;
                    match event {
                        TRIGGER_WAYPOINT_ON_GROUND | TRIGGER_WAYPOINT_IN_AIR => {
                            let ground = h.ground_height(target);
                            let on = if event == TRIGGER_WAYPOINT_ON_GROUND {
                                target.z == ground
                            } else {
                                target.z > ground
                            };
                            plain(h, unit).arrived && h.waypoints(unit) > 0 && on
                        }
                        TRIGGER_WAYPOINT_IS_HIGHER_ELEVATION => {
                            plain(h, unit).arrived && h.waypoints(unit) > 0 && climb > 0.0
                        }
                        _ => {
                            let count = h.waypoints(unit);
                            let p = plain(h, unit);
                            if p.traffic == 2 || p.traffic == 4 {
                                if event == TRIGGER_WAYPOINT_IS_SAME_ELEVATION {
                                    count <= 1
                                } else {
                                    count > 0
                                }
                            } else {
                                p.arrived
                                    && count > 0
                                    && if event == TRIGGER_WAYPOINT_IS_SAME_ELEVATION {
                                        climb == 0.0
                                    } else {
                                        climb < 0.0
                                    }
                            }
                        }
                    }
                }
            }
        }
        TRIGGER_UNIT_IN_WEAPON_RANGE => {
            if h.projectile_category(class) == CATEGORY_BOMB {
                plain(h, unit).ammunition < ammunition || h.bomb_on_target(class, unit)
            } else {
                return None;
            }
        }
        TRIGGER_FLIGHT_TIME_ALARM => {
            let p = plain(h, unit);
            let z = h.location(unit).m[2][3];
            z != h.tile_height(p.tile) && p.flight_time == 0
        }
        TRIGGER_LANDING_CLEARANCE_GRANTED => plain(h, unit).clearance,
        TRIGGER_HOLDING_PATTERN_COMPUTED => true,
        TRIGGER_AIRPORT_DESTROYED => {
            let base = h.home_base(unit);
            base == 0 || plain(h, base).hit_points == 0
        }
        TRIGGER_SECONDARY_TARGET_FOUND => h.attack_target_alive(unit),
        TRIGGER_AIRTRAFFIC_CONTROLLER_MOVED => {
            let base = h.home_base(unit);
            if base != 0 {
                let b = plain(h, base);
                (b.category == CATEGORY_NAVAL_BUILDING || b.category == CATEGORY_SPACE_BUILDING)
                    && b.traffic != 0
                    && h.goal_has_location(unit)
                    && h.away_from_goal(class, unit)
            } else {
                false
            }
        }
        TRIGGER_REQUEST_STILL_VALID => h.request_queued(unit),
        _ => return None,
    };
    Some(i32::from(flag))
}

/// `AirAnimalEvaluateCondition`: the flying animal's waypoint triggers; None for the others.
pub fn air_animal_condition<H: ConditionHost + ?Sized>(h: &mut H, unit: u64, event: u32) -> Option<i32> {
    let z = h.location(unit).m[2][3];
    let count = h.waypoints(unit);
    let mut target = Ge3DPoint::default();
    let mut delta = 0.0f32;
    let mut ground = 0.0f32;
    if let Some(t) = h.next_flight_point(unit) {
        target = t;
        delta = t.z - z;
        ground = h.ground_height(t);
    }
    let p = plain(h, unit);
    let flag = match event {
        TRIGGER_WAYPOINT_IS_LOWER_ELEVATION => {
            if p.traffic != 2 {
                p.arrived && count > 0 && delta < 0.0
            } else {
                count > 0
            }
        }
        TRIGGER_WAYPOINT_IS_SAME_ELEVATION => {
            if p.traffic != 2 {
                p.arrived && count > 0 && delta == 0.0
            } else {
                count <= 1
            }
        }
        TRIGGER_WAYPOINT_IS_HIGHER_ELEVATION => p.arrived && count > 0 && delta > 0.0,
        TRIGGER_WAYPOINT_IN_AIR => p.arrived && count > 0 && target.z > ground,
        TRIGGER_WAYPOINT_ON_GROUND => p.arrived && count > 0 && target.z == ground,
        TRIGGER_MOVE_PREPARATION_COMPLETE => h.waypoints(unit) > 0,
        TRIGGER_DYING_ANIMATION_FINISHED => p.velocity == 0.0,
        _ => return None,
    };
    Some(i32::from(flag))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::units::unit_fields::EeUnitsComplexUnitFields;

    struct Mock {
        cx: Box<EeUnitsComplexUnitFields>,
        at: GeTransformation,
        speeds: (f32, f32),
        /// corner heights z = x * 0.5 (a slope), the map 0..20 square, the tiles' heights 1
        near: Vec<(Ge3DPoint, f32)>,
        /// the flight plans' world: the waypoints, the goal's target, the home base's record and location, the log
        waypoints: Vec<Ge3DPoint>,
        target: Option<Ge3DPoint>,
        base_class: Box<EeUnitsUnitClassFields>,
        base_at: GeTransformation,
        log: Vec<String>,
        command: i32,
        bomb: bool,
    }

    impl ConditionHost for Mock {
        fn command_type(&mut self, _unit: u64) -> i32 {
            self.command
        }
        fn next_flight_point(&mut self, _unit: u64) -> Option<Ge3DPoint> {
            self.waypoints.first().copied()
        }
        fn position(&mut self, _unit: u64) -> Ge3DPoint {
            Ge3DPoint { x: self.at.m[0][3], y: self.at.m[1][3], z: self.at.m[2][3] }
        }
        fn ground_height(&mut self, _point: Ge3DPoint) -> f32 {
            1.0
        }
        fn bomb_on_target(&mut self, _class: u64, _unit: u64) -> bool {
            self.log.push("bomb".into());
            self.bomb
        }
        fn away_from_goal(&mut self, _class: u64, _unit: u64) -> bool {
            true
        }
        fn projectile_category(&mut self, _class: u64) -> i32 {
            CATEGORY_BOMB
        }
        fn attack_target_alive(&mut self, _unit: u64) -> bool {
            false
        }
        fn goal_has_location(&mut self, _unit: u64) -> bool {
            true
        }
        fn request_queued(&mut self, _unit: u64) -> bool {
            true
        }
    }

    impl FlightHost for Mock {
        fn home_base(&mut self, _unit: u64) -> u64 {
            2
        }
        fn waypoints(&mut self, _unit: u64) -> i32 {
            self.waypoints.len() as i32
        }
        fn next_waypoint(&mut self, _unit: u64) -> Ge3DPoint {
            self.waypoints[0]
        }
        fn flight_target(&mut self, _unit: u64) -> Option<Ge3DPoint> {
            self.target
        }
        fn drop_path(&mut self, _unit: u64) {
            self.log.push("drop".into());
        }
        fn add_waypoint(&mut self, _class: u64, _unit: u64, p: Ge3DPoint, snap: bool) {
            self.log.push(format!("add {} {} {} {}", p.x, p.y, p.z, snap));
        }
        fn path_toward(&mut self, _class: u64, _unit: u64, from: Ge3DPoint, to: Ge3DPoint, snap: bool) {
            self.log.push(format!("path {} {} -> {} {} {} {}", from.x, from.y, to.x, to.y, to.z, snap));
        }
        fn fly_to_goal(&mut self, _class: u64, p: Ge3DPoint, _unit: u64) {
            self.log.push(format!("fly {} {} {}", p.x, p.y, p.z));
        }
        fn runway_goal(&mut self, _unit: u64, p: Ge3DPoint) {
            self.log.push(format!("goal {} {} {}", p.x, p.y, p.z));
        }
    }

    impl AirHost for Mock {
        fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
            EeUnitsUuUnit { obj, cx: &*self.cx, class_fields: &*self.base_class, ..Default::default() }
        }
        fn location(&mut self, unit: u64) -> GeTransformation {
            if unit == 2 {
                self.base_at
            } else {
                self.at
            }
        }
        fn turn_speeds(&mut self, _unit: u64) -> (f32, f32) {
            self.speeds
        }
        fn map_tile(&mut self, x: i32, y: i32) -> u64 {
            u64::from((0..20).contains(&x) && (0..20).contains(&y))
        }
        fn tile_height(&mut self, _tile: u64) -> f32 {
            1.0
        }
        fn mesh_z(&mut self, x: i32, _y: i32) -> f32 {
            x as f32 * 0.5
        }
        fn tile_line(&mut self, from: Tile, to: Tile) -> Vec<Tile> {
            let n = (to[0] - from[0]).abs();
            (0..=n).map(|i| [from[0] + (to[0] - from[0]).signum() * i, from[1]]).collect()
        }
        fn tiles_near_point(&mut self, point: Ge3DPoint, distance: f32) -> Vec<Tile> {
            self.near.push((point, distance));
            vec![[floor(point.x), floor(point.y)]]
        }
    }

    fn at(x: f32, y: f32, z: f32) -> GeTransformation {
        let mut t = GeTransformation::default();
        t.m = [[1.0, 0.0, 0.0, x], [0.0, 1.0, 0.0, y], [0.0, 0.0, 1.0, z], [0.0, 0.0, 0.0, 1.0]];
        t
    }

    fn mock(order: u32) -> Mock {
        let cx = EeUnitsComplexUnitFields { mCurrentPhysicsOrder: order as _, ..Default::default() };
        Mock {
            cx: Box::new(cx),
            at: at(2.5, 3.5, 4.0),
            speeds: (2.0, 0.5),
            near: Vec::new(),
            waypoints: Vec::new(),
            target: None,
            base_class: class(2, 0.0),
            base_at: at(10.5, 10.5, 0.0),
            log: Vec::new(),
            command: 0,
            bomb: false,
        }
    }

    fn class(category: u32, cruise: f32) -> Box<EeUnitsUnitClassFields> {
        Box::new(EeUnitsUnitClassFields { mCategory: category as _, mCruiseAltitude: cruise, ..Default::default() })
    }

    #[test]
    fn a_helicopter_flies_its_cruise_altitude_over_the_highest_corner_on_the_line() {
        let mut m = mock(5);
        // the helicopter (7): the line from x 2 to 6, the last corner x 7: 3.5 + 1.5
        let c = class(7, 1.5);
        assert_eq!(height_at(&mut m, &*c, 1, Ge3DPoint { x: 6.5, y: 3.5, z: 0.0 }), 5.0);
        // at most 12, at least the cruise altitude
        assert_eq!(height_at(&mut m, &*c, 1, Ge3DPoint { x: 6.5, y: 3.5, z: 20.0 }), 12.0);
        let high = class(7, 14.0);
        assert_eq!(height_at(&mut m, &*high, 1, Ge3DPoint { x: 2.5, y: 3.5, z: 0.0 }), 14.0);
    }

    #[test]
    fn the_priest_and_the_transport_helicopters_only_cap_it() {
        let mut m = mock(5);
        let c = class(0x11, 14.0);
        assert_eq!(height_at(&mut m, &*c, 1, Ge3DPoint { x: 2.5, y: 3.5, z: 0.0 }), 12.0);
        let low = class(0x11, 0.0);
        assert_eq!(height_at(&mut m, &*low, 1, Ge3DPoint { x: 2.5, y: 3.5, z: 0.0 }), 1.5);
    }

    #[test]
    fn a_landable_circling_looks_at_the_point_ahead_on_its_circle() {
        let mut m = mock(1);
        // the airplane (3, a landable), the point 2 above its tile (height 1)
        let c = class(3, 3.0);
        let h = height_at(&mut m, &*c, 1, Ge3DPoint { x: 2.5, y: 3.5, z: 3.0 });
        // the turning radius 4, to the right while circling left (order 1: -4): x -1.5, the tile -2 is off the map
        assert_eq!(m.near, vec![(Ge3DPoint { x: -1.5, y: 3.5, z: 4.0 }, 6.0)]);
        assert_eq!(h, 3.0);
        // in a straight line: the corners up to x 7 plus 2
        let mut m = mock(5);
        assert_eq!(height_at(&mut m, &*c, 1, Ge3DPoint { x: 6.5, y: 3.5, z: 3.0 }), 5.5);
    }

    #[test]
    fn the_spaceships_take_the_min_and_max_and_others_the_point() {
        let mut m = mock(5);
        let c = class(0x21, 13.0);
        assert_eq!(height_at(&mut m, &*c, 1, Ge3DPoint { x: 2.5, y: 3.5, z: 0.0 }), 13.0);
        let ground = class(8, 1.0);
        assert_eq!(height_at(&mut m, &*ground, 1, Ge3DPoint { x: 2.5, y: 3.5, z: 0.25 }), 0.25);
    }

    #[test]
    fn the_flight_state_turns_toward_a_point_inside_the_circle() {
        let mut m = mock(5);
        // radius 4: a point 1 to the left of the plane is inside the left circle
        m.waypoints = vec![Ge3DPoint { x: 1.5, y: 3.5, z: 0.0 }];
        assert_eq!(next_flight_state(&mut m, 1), 0);
        m.waypoints = vec![Ge3DPoint { x: 3.5, y: 3.5, z: 0.0 }];
        assert_eq!(next_flight_state(&mut m, 1), 1);
        // far ahead: outside every circle
        m.waypoints = vec![Ge3DPoint { x: 2.5, y: 20.0, z: 0.0 }];
        assert_eq!(next_flight_state(&mut m, 1), 5);
        // no waypoints: the goal's target, none: 5
        m.waypoints.clear();
        assert_eq!(next_flight_state(&mut m, 1), 5);
        m.target = Some(Ge3DPoint { x: 2.5, y: 0.5, z: 0.0 });
        assert_eq!(next_flight_state(&mut m, 1), 1);
    }

    #[test]
    fn a_runway_approach_lines_up_descends_and_lands_at_the_airport() {
        let mut m = mock(5);
        let c = class(3, 4.0);
        approach_runway(&mut m, 9, &*c, 1, 0, 1);
        // the turning radius ceil(4) = 4: run 6, the turn 6 / tan(30) + 6
        let turn = 6.0 / ee_base::math::um::um_tan(APPROACH_ANGLE) + 6.0;
        assert_eq!(m.log[0], "drop");
        assert_eq!(m.log[1], format!("path 2.5 3.5 -> 10.5 {} 4 true", turn + 17.5));
        assert_eq!(
            m.log[2..].to_vec(),
            vec![
                "add 10.5 23.5 2 false".to_string(),
                "add 10.5 17.5 0.6 false".into(),
                "add 10.5 13.5 0.1 false".into(),
                "add 10.5 11.5 0.1 false".into(),
                "add 10.5 10.5 0.1 false".into(),
                "goal 10.5 10.5 1".into(),
            ]
        );
    }

    #[test]
    fn a_navy_yard_approach_ends_on_its_pier_and_snaps_the_low_point() {
        let mut m = mock(5);
        m.base_class = class(0x15, 0.0);
        m.base_class.mUnitType = TYPE_NAVY_YARD as _;
        let c = class(3, 4.0);
        approach_runway(&mut m, 9, &*c, 1, 1, 0);
        assert!(m.log.contains(&"add 21 10.5 1.2 true".to_string()), "{:?}", m.log);
        assert_eq!(m.log.last().unwrap(), "goal 12 10.5 1");
    }

    #[test]
    fn a_runway_take_off_rolls_low_then_climbs() {
        let mut m = mock(5);
        let c = class(3, 4.0);
        take_off_from_runway(&mut m, 9, &*c, 1, -1, 0);
        assert_eq!(m.log, vec!["drop", "add -0.5 3.5 0.1 false", "add -4.5 3.5 3 false", "fly -4.5 3.5 3"]);
        assert_eq!(runway_direction(0), (0, 1));
        assert_eq!(runway_direction(7), (-1, 0));
    }

    #[test]
    fn a_plane_arrives_within_half_a_tile_or_braking_when_slow() {
        let mut m = mock(5);
        let c = class(3, 4.0);
        m.waypoints = vec![Ge3DPoint { x: 2.9, y: 3.5, z: 9.0 }];
        assert!(landing_has_arrived(&mut m, &*c, 1, 1.0, false));
        m.waypoints = vec![Ge3DPoint { x: 3.1, y: 3.5, z: 0.0 }];
        assert!(!landing_has_arrived(&mut m, &*c, 1, 1.0, false));
        m.command = ACTION_BRAKE;
        assert!(landing_has_arrived(&mut m, &*c, 1, 0.05, false));
        assert!(!landing_has_arrived(&mut m, &*c, 1, 0.05, true), "an animal stops at 0");
        assert!(landing_has_arrived(&mut m, &*c, 1, 0.0, true));
    }

    #[test]
    fn the_landing_triggers_read_the_next_waypoint_and_the_plane() {
        let mut m = mock(5);
        let c = class(3, 4.0);
        // the unit at height 4, its next waypoint at 6 over ground 1: higher, in the air
        m.waypoints = vec![Ge3DPoint { x: 2.5, y: 3.5, z: 6.0 }];
        m.cx.mHasArrived = true;
        assert_eq!(landable_condition(&mut m, 9, &*c, 1, TRIGGER_WAYPOINT_IS_HIGHER_ELEVATION), Some(1));
        assert_eq!(landable_condition(&mut m, 9, &*c, 1, TRIGGER_WAYPOINT_IN_AIR), Some(1));
        assert_eq!(landable_condition(&mut m, 9, &*c, 1, TRIGGER_WAYPOINT_ON_GROUND), Some(0));
        assert_eq!(landable_condition(&mut m, 9, &*c, 1, TRIGGER_WAYPOINT_IS_LOWER_ELEVATION), Some(0));
        assert_eq!(air_animal_condition(&mut m, 1, TRIGGER_WAYPOINT_IS_HIGHER_ELEVATION), Some(1));
        // the bomb: ammunition spent, else the drop test
        assert_eq!(landable_condition(&mut m, 9, &*c, 1, TRIGGER_UNIT_IN_WEAPON_RANGE), Some(0));
        assert_eq!(m.log, vec!["bomb"]);
        // the unit's own class (the mock's view class) answers the space domain test of a dying plane
        m.cx.mVelocity = 1.0;
        assert_eq!(landable_condition(&mut m, 9, &*c, 1, TRIGGER_DYING_ANIMATION_FINISHED), Some(0));
        assert_eq!(landable_condition(&mut m, 9, &*c, 1, TRIGGER_HOLDING_PATTERN_COMPUTED), Some(1));
        assert_eq!(landable_condition(&mut m, 9, &*c, 1, 4242), None);
        assert_eq!(air_animal_condition(&mut m, 1, TRIGGER_FLIGHT_TIME_ALARM), None);
    }
}
