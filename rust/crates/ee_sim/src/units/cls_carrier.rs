//! The landing aircraft's flight plans of `EEUnitClass` (the unit classes cluster, round 1; EEUnitClassAir.cpp until
//! this round): the take-off from the ground (`LandableReturnToAirport`), the carrier's take-off point
//! (`RouteDockWaypoint`), the carrier approach and take-off (`ApproachCarrier`, `TakeOffFromCarrier`), whether the
//! carrier moved (`IsAwayFromGoal`), and the airplane's two `ActionChanged` operations (the circling side, the
//! holding pattern). The C++ members keep their UCBD notes around one call of the entry points in
//! cls_carrier_host.rs.
//!
//! The world is the [`CarrierHost`] (the movement bodies' [`MoveHost`] and the carrier's tag points, the plane's
//! orientation, scale and goal, the members with notes of their own). `AddWaypointAt` and `PathToward` are
//! cls_move.rs' bodies, called directly (they have no notes).
//!
//! Determinism (RUST_PLAN 2.1): the C++'s float operations in order (`Apply` as ee_base's `apply_point`, `UMCeil`,
//! `UMTan`, `UMSin`, `UMCos`, `UMArcTan2` as ee_base's), `(long)` of a float as a truncation, the random draws
//! where the C++ drew them.

// The C++'s negated float comparisons are kept as written; `c_long` casts are needed on LP64.
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::unnecessary_cast)]

use crate::units::cls_move::{add_waypoint_at, path_toward, MoveHost};
use crate::units::unit_class_fields::EeUnitsUnitClassFields;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use ee_base::math::types::{Ge3DPoint, GeOrientation, GeTransformation};
use ee_base::math::um::{lrintf, um_arctan2, um_cos, um_sin, um_tan};

/// `kActionTakeoff`, `kActionBrake`, the Sp15 space fighter's unit type, the approach's angle (`kLandableAngle`)
/// cbindgen:ignore
mod k {
    pub const ACTION_TAKEOFF: i32 = 1000;
    pub const ACTION_BRAKE: i32 = 1014;
    pub const TYPE_SP15_SPACE_FIGHTER: u64 = 0x2f6;
    #[allow(clippy::approx_constant)] // the original's constant (0x447040)
    pub const LANDABLE_ANGLE: f32 = 0.523_598_8;
    pub const HALF_PI: f32 = 1.570_796_4;
}
use k::*;

/// The world the flight plans ask (the game's: cls_carrier_host.rs).
pub trait CarrierHost: MoveHost {
    /// the unit's `mStats` record
    fn stats(&mut self, unit: u64) -> *const EeUnitsUnitClassUpgradeFields;
    /// `mHomeBase`
    fn home_base(&mut self, unit: u64) -> u64;
    /// the carrier's tag point `index` of its animation at the plane's frame in the carrier's location scaled by its
    /// stats (`EETagPointList::Scale`, `GetPoint`); None when the carrier's model has no tag points there
    fn tag(&mut self, carrier: u64, plane: u64, index: i32) -> Option<Ge3DPoint>;
    /// `mOrientation = o`
    fn set_orientation(&mut self, unit: u64, o: GeOrientation);
    /// the current command's type (0: none)
    fn command_type(&mut self, unit: u64) -> i32;
    /// `SetTransformation(its location at p, true)`; `mPhysicalModel.SetScale(s)`
    fn set_position(&mut self, unit: u64, p: Ge3DPoint);
    fn set_scale(&mut self, unit: u64, s: f32);
    /// the class's `RouteDockWaypoint(unit)` (its UCBD note) and `FlyToGoal(p, unit)`
    fn route_dock_waypoint(&mut self, class: u64, unit: u64);
    fn fly_to_goal(&mut self, class: u64, p: Ge3DPoint, unit: u64);
    /// the player's air traffic control of the unit's class is not NULL
    fn has_air_traffic(&mut self, unit: u64) -> bool;
    /// the approach's move goal: the location goal's point set to `deck`, else a new location goal there (the
    /// queue cleared first)
    fn deck_goal(&mut self, unit: u64, deck: Ge3DPoint);
    /// `EELocationGoalPoint(unit->GetGoal())`
    fn location_goal_point(&mut self, unit: u64) -> Ge3DPoint;
    /// without a home base: the first air traffic slot's airport, else the unit killed; the home base after
    fn home_first_slot(&mut self, unit: u64) -> u64;
    /// `GetNextWaypointPoint(out)`: Some(out) when it answers 0
    fn next_waypoint_at(&mut self, unit: u64) -> Option<Ge3DPoint>;
    /// `gPhysics->mRandom.Generate()`
    fn random(&mut self) -> f32;
    /// the class's `GetHeightAt(unit, p)`
    fn height_at(&mut self, class: u64, unit: u64, p: Ge3DPoint) -> f32;
    /// `gWorld.mMap->mTerrainMesh.GetIsSpaceMap()`; `gContinentManager.CanGetTo(unit, x, y)`
    fn is_space_map(&mut self) -> bool;
    fn can_get_to(&mut self, unit: u64, x: i32, y: i32) -> bool;
}

fn class<H: CarrierHost + ?Sized>(h: &mut H, class: u64) -> &'static EeUnitsUnitClassFields {
    // SAFETY: a live class's record (classes live for the game)
    unsafe { &*h.class(class) }
}

fn stats<H: CarrierHost + ?Sized>(h: &mut H, unit: u64) -> EeUnitsUnitClassUpgradeFields {
    // SAFETY: the unit's live stats record (copied at once)
    unsafe { *h.stats(unit) }
}

fn at(x: f32, y: f32, z: f32) -> Ge3DPoint {
    Ge3DPoint { x, y, z }
}

/// `UMCeil`: rounds to nearest, then steps up when that went down.
pub fn um_ceil(value: f32) -> i32 {
    let r = lrintf(value);
    if (r as f32) < value {
        r.wrapping_add(1)
    } else {
        r
    }
}

fn set_position(t: &mut GeTransformation, p: Ge3DPoint) {
    t.m[0][3] = p.x;
    t.m[1][3] = p.y;
    t.m[2][3] = p.z;
}

/// `EEUnitClass::LandableReturnToAirport(unit)`: rolls 3 ahead, climbs 7 ahead to 3/4 cruising height (plus the
/// ground when it is higher), then flies to the goal; true (nothing done without an air traffic control).
pub fn return_to_airport<H: CarrierHost + ?Sized>(h: &mut H, cls: u64, unit: u64) -> bool {
    let t = h.location(unit);
    if unit == 0 || !h.has_air_traffic(unit) {
        return true;
    }
    let tile = h.unit(unit).tile;
    let ground = if tile != 0 { h.tile_height(tile) } else { 0.0 };
    h.drop_path(unit);
    let p = t.apply_point(at(0.0, 3.0, 0.0));
    add_waypoint_at(h, unit, p.x, p.y, ground, true);
    let mut p = t.apply_point(at(0.0, 7.0, 0.0));
    let mut height = class(h, cls).mCruiseAltitude * 0.75;
    if ground > height {
        height += ground;
    }
    add_waypoint_at(h, unit, p.x, p.y, height, true);
    p.z = height;
    h.set_orientation(unit, t.orientation());
    h.fly_to_goal(cls, p, unit);
    true
}

/// `EEUnitClass::RouteDockWaypoint(unit)`: drops the path and adds a waypoint at the home carrier's take-off point
/// (tag point 2; its centre without tags).
pub fn route_dock_waypoint<H: CarrierHost + ?Sized>(h: &mut H, unit: u64) {
    let carrier = h.home_base(unit);
    let c = h.location(carrier);
    h.drop_path(unit);
    let target = h.tag(carrier, unit, 2).unwrap_or(at(c.x(), c.y(), 1.0));
    add_waypoint_at(h, unit, target.x, target.y, target.z, true);
}

/// `EEUnitClass::ApproachCarrier(unit, steep)`: the turn, descent and touch-down onto the home carrier's deck (tag
/// point 0; `steep`: the full approach at any height), or the deck's stop point (tag 2) when braking; the deck
/// becomes the move goal.
pub fn approach_carrier<H: CarrierHost + ?Sized>(h: &mut H, cls: u64, unit: u64, steep: bool) {
    let carrier = h.home_base(unit);
    let mut position = h.position(unit);
    let height = position.z;
    h.drop_path(unit);
    if h.tag(carrier, unit, 0).is_none() {
        return;
    }
    let cruise = class(h, cls).mCruiseAltitude;
    let deck;
    if h.command_type(unit) != ACTION_BRAKE {
        deck = h.tag(carrier, unit, 0).expect("tags");
        let mut t = h.location(carrier);
        set_position(&mut t, deck);
        let s = stats(h, unit);
        let length = s.mMaxVelocity / s.mMaxAngularVelocity;
        let count = um_ceil(length);
        let run = count as f32 * 1.5;
        let far = run / um_tan(LANDABLE_ANGLE) + run;
        let dx = position.x - (deck.x - far);
        if dx * dx + (position.y - deck.y) * (position.y - deck.y) <= count as f32 * 2.5 {
            let waypoint = at(deck.x, deck.y, cruise * 0.7);
            path_toward(h, unit, position, waypoint, true);
            position = waypoint;
        }
        if steep || cruise <= height {
            let mut waypoint = t.apply_point(at(0.0, -far, 0.0));
            waypoint.z = cruise;
            path_toward(h, unit, position, waypoint, true);
        }
        if steep || deck.z + (cruise - deck.z) * 0.3 <= height {
            let mut waypoint = t.apply_point(at(0.0, -run, 0.0));
            waypoint.z = deck.z + (cruise - deck.z) * 0.3;
            add_waypoint_at(h, unit, waypoint.x, waypoint.y, waypoint.z, true);
        }
        add_waypoint_at(h, unit, deck.x, deck.y, deck.z, true);
        let c = h.location(carrier);
        add_waypoint_at(h, unit, c.x(), c.y(), c.z(), true);
    } else {
        // braking: the deck's stop point is tag point 2, and the plane takes the carrier's own heading
        deck = h.tag(carrier, unit, 2).expect("tags");
        add_waypoint_at(h, unit, deck.x, deck.y, deck.z, true);
        let o = h.location(carrier).orientation();
        h.set_orientation(unit, o);
    }
    h.deck_goal(unit, deck);
}

/// `EEUnitClass::TakeOffFromCarrier(unit)`: on take-off, sits at the deck start (tag point 0) scaled by its height
/// and heads for the take-off point; otherwise flies off the deck end (tag point 2) toward the goal.
pub fn take_off_from_carrier<H: CarrierHost + ?Sized>(h: &mut H, cls: u64, unit: u64) {
    let carrier = h.home_base(unit);
    let c = h.location(carrier);
    let (x, y) = (c.x(), c.y());
    h.drop_path(unit);
    let (start, end) = match h.tag(carrier, unit, 0) {
        Some(start) => (start, h.tag(carrier, unit, 2).expect("tags")),
        None => (at(x, y, 1.0), at(x, y, 1.0)),
    };
    let cruise = class(h, cls).mCruiseAltitude;
    if h.command_type(unit) == ACTION_TAKEOFF {
        h.set_position(unit, start);
        let scale = stats(h, unit).mScale;
        if start.z >= cruise {
            h.set_scale(unit, scale);
        } else {
            let half = start.z / cruise * 0.5;
            h.set_scale(unit, scale * (half + 0.5));
        }
        h.route_dock_waypoint(cls, unit);
    } else {
        let mut t = h.location(carrier);
        set_position(&mut t, end);
        // SAFETY: the unit's live class record
        let unit_type = unsafe { (*h.unit(unit).class_fields).mUnitType as u64 };
        let ahead = if unit_type == TYPE_SP15_SPACE_FIGHTER { 3.0 } else { 10.0 };
        let target = t.apply_point(at(0.0, ahead, 0.0));
        add_waypoint_at(h, unit, target.x, target.y, cruise * 0.7, true);
        h.fly_to_goal(cls, target, unit);
    }
}

/// `EEUnitClass::IsAwayFromGoal(unit)`: the move goal is more than 1 from the home carrier's deck point (tag 0).
pub fn is_away_from_goal<H: CarrierHost + ?Sized>(h: &mut H, unit: u64) -> bool {
    let carrier = h.home_base(unit);
    let t = h.location(carrier);
    let deck = h.tag(carrier, unit, 0).unwrap_or(at(t.x(), t.y(), 1.0));
    let goal = h.location_goal_point(unit);
    !(deck.distance_to_squared(goal) <= 1.0)
}

/// Sets the physics order (`SetCurrentPhysicsOrder`: stored when it differs).
fn set_order<H: CarrierHost + ?Sized>(h: &mut H, unit: u64, order: u64) {
    let cx = h.unit(unit).cx as *mut crate::units::unit_fields::EeUnitsComplexUnitFields;
    // SAFETY: the unit's live record, written at once as the C++'s inline setter writes it
    unsafe {
        if (*cx).mCurrentPhysicsOrder as u64 != order {
            (*cx).mCurrentPhysicsOrder = order as _;
        }
    }
}

fn random_side<H: CarrierHost + ?Sized>(h: &mut H, unit: u64) {
    let order = if h.random() < 0.5 { 0 } else { 1 };
    set_order(h, unit, order);
}

/// `EEAirplaneCircleSide`: the circling side (physics order 0 left, 1 right); on space maps toward the side the
/// plane can reach (one turning circle out, then two), else at random.
pub fn circle_side<H: CarrierHost + ?Sized>(h: &mut H, unit: u64) {
    if !h.is_space_map() {
        random_side(h, unit);
        return;
    }
    let s = stats(h, unit);
    let distance = um_ceil(s.mMaxVelocity / s.mMaxAngularVelocity) as f32;
    let t = h.location(unit);
    let angle = um_arctan2(t.m[1][0], t.m[0][0]);
    let left = angle - HALF_PI;
    let right = angle + HALF_PI;
    let near = |h: &mut H, side: f32, d: f32| {
        let x = (t.x() - um_sin(side) * d) as i32;
        let y = (um_cos(side) * d + t.y()) as i32;
        h.can_get_to(unit, x, y)
    };
    let left_near = near(h, left, distance);
    let right_near = near(h, right, distance);
    if left_near && !right_near {
        set_order(h, unit, 0);
    } else if !left_near && right_near {
        set_order(h, unit, 1);
    } else {
        let left_near = near(h, left, distance * 2.0);
        let right_near = near(h, right, distance * 2.0);
        if left_near && !right_near {
            set_order(h, unit, 0);
        } else if !left_near && right_near {
            set_order(h, unit, 1);
        } else {
            random_side(h, unit);
        }
    }
}

/// `EEAirplaneHoldingPattern`: circles over its home airport at cruising height (a home base found first; killed
/// when there is none), not commandable.
pub fn holding_pattern<H: CarrierHost + ?Sized>(h: &mut H, cls: u64, unit: u64) {
    let t = h.location(unit);
    let position = at(t.x(), t.y(), t.z());
    if h.home_base(unit) == 0 && h.home_first_slot(unit) == 0 {
        return;
    }
    h.drop_path(unit);
    let home = h.home_base(unit);
    let tile = {
        let r = h.unit(home);
        // SAFETY: the home base's live record
        unsafe { [(*r.rts).mTileX as i32, (*r.rts).mTileY as i32] }
    };
    let target = at(tile[0] as f32 + 1.5, tile[1] as f32 + 1.5, class(h, cls).mCruiseAltitude);
    path_toward(h, unit, position, target, true);
    random_side(h, unit);
    let cx = h.unit(unit).cx as *mut crate::units::unit_fields::EeUnitsComplexUnitFields;
    // SAFETY: as set_order (`SetIsCommandable(false)`)
    unsafe {
        if (*cx).mIsCommandable {
            (*cx).mIsCommandable = false;
        }
    }
    if let Some(next) = h.next_waypoint_at(unit) {
        let height = h.height_at(cls, unit, next);
        // SAFETY: as set_order (`SetHeight`)
        unsafe {
            if (*cx).mHeight != height {
                (*cx).mHeight = height;
            }
        }
    }
}

#[cfg(test)]
#[path = "cls_carrier_tests.rs"]
mod tests;
