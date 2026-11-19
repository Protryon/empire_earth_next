//! The landing aircraft's flight plans' host in the game ([`CarrierHost`] over the movement bodies' host and the
//! class bodies' C++ switch, `ee_units_cls_host`'s `EEUnitClassMoveOp`) and the entry points the `EEUnitClass`
//! members and the airplane's action operations call (cls_carrier.rs). Absent from a test binary.

#![cfg(not(test))]

use ee_base::math::types::{Ge3DPoint, GeOrientation};

use crate::units::cls_carrier::{self as cr, CarrierHost};
use crate::units::cls_host::{ClsGame, EeUnitsClsIo, EE_UNITS_CLS_COMMAND_TYPE, EE_UNITS_CLS_FLY_TO_GOAL};
use crate::units::cls_host::{EE_UNITS_CLS_HOME_BASE, EE_UNITS_CLS_IS_SPACE_MAP, EE_UNITS_CLS_NEXT_WAYPOINT};
use crate::units::uu_host::Game;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use crate::world::{UnitView, WorldRead};

/// the carrier `a`'s tag point `c` at plane `b`'s frame in its scaled location into `io.p` (0: no tag points)
pub const EE_UNITS_CLS_CR_TAG: i32 = 73;
/// unit `a`'s `mOrientation = (io.p[0], io.p[1], io.p[2])`
pub const EE_UNITS_CLS_CR_SET_ORIENTATION: i32 = 74;
/// unit `a`'s `SetTransformation(its location at io.p, true)`
pub const EE_UNITS_CLS_CR_SET_POSITION: i32 = 75;
/// unit `a`'s `mPhysicalModel.SetScale(io.f)`
pub const EE_UNITS_CLS_CR_SET_SCALE: i32 = 76;
/// class `b`'s `RouteDockWaypoint(unit a)`
pub const EE_UNITS_CLS_CR_ROUTE_DOCK: i32 = 77;
/// unit `a`'s player's air traffic control of its class is not NULL
pub const EE_UNITS_CLS_CR_HAS_AIR_TRAFFIC: i32 = 78;
/// unit `a`'s approach goal at `io.p` (the location goal's point, else a new location goal)
pub const EE_UNITS_CLS_CR_DECK_GOAL: i32 = 79;
/// `EELocationGoalPoint(unit a's goal)` into `io.p`
pub const EE_UNITS_CLS_CR_LOCATION_GOAL_POINT: i32 = 80;
/// unit `a` without a home base: the first slot's airport, else killed; the home base after
pub const EE_UNITS_CLS_CR_HOME_FIRST_SLOT: i32 = 81;
/// `gPhysics->mRandom.Generate()` into `io.f`
pub const EE_UNITS_CLS_CR_RANDOM: i32 = 82;
/// class `b`'s `GetHeightAt(unit a, io.p)` into `io.f`
pub const EE_UNITS_CLS_CR_HEIGHT_AT: i32 = 83;
/// `gContinentManager.CanGetTo(unit a, c, d)`
pub const EE_UNITS_CLS_CR_CAN_GET_TO: i32 = 84;

impl ClsGame {
    #[allow(clippy::too_many_arguments)] // the switch's operands
    fn cr(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, p: Ge3DPoint, f: f32) -> (i64, EeUnitsClsIo) {
        let mut io = EeUnitsClsIo { p: [p.x, p.y, p.z], f, ..Default::default() };
        let r = self.call(op, a, b, c, d, &mut io);
        (r, io)
    }
    fn cr0(&mut self, op: i32, a: u64, b: u64) -> (i64, EeUnitsClsIo) {
        self.cr(op, a, b, 0, 0, Ge3DPoint::default(), 0.0)
    }
}

fn point(p: [f32; 3]) -> Ge3DPoint {
    Ge3DPoint { x: p[0], y: p[1], z: p[2] }
}

impl CarrierHost for ClsGame {
    fn stats(&mut self, unit: u64) -> *const EeUnitsUnitClassUpgradeFields {
        // SAFETY: the bodies' caller holds the world; a live unit
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        unsafe { UnitView::of(&t, unit) }.and_then(|u| u.stats()).map_or(std::ptr::null(), |s| s.fields().ptr())
    }
    fn home_base(&mut self, unit: u64) -> u64 {
        self.cr0(EE_UNITS_CLS_HOME_BASE, unit, 0).0 as u64
    }
    fn tag(&mut self, carrier: u64, plane: u64, index: i32) -> Option<Ge3DPoint> {
        let (r, io) = self.cr(EE_UNITS_CLS_CR_TAG, carrier, plane, i64::from(index), 0, Ge3DPoint::default(), 0.0);
        (r != 0).then_some(point(io.p))
    }
    fn set_orientation(&mut self, unit: u64, o: GeOrientation) {
        self.cr(EE_UNITS_CLS_CR_SET_ORIENTATION, unit, 0, 0, 0, Ge3DPoint { x: o.yaw, y: o.pitch, z: o.roll }, 0.0);
    }
    fn command_type(&mut self, unit: u64) -> i32 {
        self.cr0(EE_UNITS_CLS_COMMAND_TYPE, unit, 0).0 as i32
    }
    fn set_position(&mut self, unit: u64, p: Ge3DPoint) {
        self.cr(EE_UNITS_CLS_CR_SET_POSITION, unit, 0, 0, 0, p, 0.0);
    }
    fn set_scale(&mut self, unit: u64, s: f32) {
        self.cr(EE_UNITS_CLS_CR_SET_SCALE, unit, 0, 0, 0, Ge3DPoint::default(), s);
    }
    fn route_dock_waypoint(&mut self, class: u64, unit: u64) {
        self.cr0(EE_UNITS_CLS_CR_ROUTE_DOCK, unit, class);
    }
    fn fly_to_goal(&mut self, class: u64, p: Ge3DPoint, unit: u64) {
        self.cr(EE_UNITS_CLS_FLY_TO_GOAL, unit, class, 0, 0, p, 0.0);
    }
    fn has_air_traffic(&mut self, unit: u64) -> bool {
        self.cr0(EE_UNITS_CLS_CR_HAS_AIR_TRAFFIC, unit, 0).0 != 0
    }
    fn deck_goal(&mut self, unit: u64, deck: Ge3DPoint) {
        self.cr(EE_UNITS_CLS_CR_DECK_GOAL, unit, 0, 0, 0, deck, 0.0);
    }
    fn location_goal_point(&mut self, unit: u64) -> Ge3DPoint {
        point(self.cr0(EE_UNITS_CLS_CR_LOCATION_GOAL_POINT, unit, 0).1.p)
    }
    fn home_first_slot(&mut self, unit: u64) -> u64 {
        self.cr0(EE_UNITS_CLS_CR_HOME_FIRST_SLOT, unit, 0).0 as u64
    }
    fn next_waypoint_at(&mut self, unit: u64) -> Option<Ge3DPoint> {
        let (r, io) = self.cr0(EE_UNITS_CLS_NEXT_WAYPOINT, unit, 0);
        (r == 0).then_some(point(io.p))
    }
    fn random(&mut self) -> f32 {
        self.cr0(EE_UNITS_CLS_CR_RANDOM, 0, 0).1.f
    }
    fn height_at(&mut self, class: u64, unit: u64, p: Ge3DPoint) -> f32 {
        self.cr(EE_UNITS_CLS_CR_HEIGHT_AT, unit, class, 0, 0, p, 0.0).1.f
    }
    fn is_space_map(&mut self) -> bool {
        self.cr0(EE_UNITS_CLS_IS_SPACE_MAP, 0, 0).0 != 0
    }
    fn can_get_to(&mut self, unit: u64, x: i32, y: i32) -> bool {
        self.cr(EE_UNITS_CLS_CR_CAN_GET_TO, unit, 0, i64::from(x), i64::from(y), Ge3DPoint::default(), 0.0).0 != 0
    }
}

fn game() -> ClsGame {
    ClsGame(Game)
}

/// `EEUnitClass::LandableReturnToAirport(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_cr_return_to_airport(class: u64, unit: u64) -> u8 {
    u8::from(cr::return_to_airport(&mut game(), class, unit))
}

/// `EEUnitClass::RouteDockWaypoint(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_cr_route_dock_waypoint(unit: u64) {
    cr::route_dock_waypoint(&mut game(), unit);
}

/// `EEUnitClass::ApproachCarrier(unit, steep)`.
#[no_mangle]
pub extern "C" fn ee_units_cr_approach_carrier(class: u64, unit: u64, steep: u8) {
    cr::approach_carrier(&mut game(), class, unit, steep != 0);
}

/// `EEUnitClass::TakeOffFromCarrier(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_cr_take_off_from_carrier(class: u64, unit: u64) {
    cr::take_off_from_carrier(&mut game(), class, unit);
}

/// `EEUnitClass::IsAwayFromGoal(unit)`.
#[no_mangle]
pub extern "C" fn ee_units_cr_is_away_from_goal(unit: u64) -> u8 {
    u8::from(cr::is_away_from_goal(&mut game(), unit))
}

/// `EEAirplaneCircleSide(class, unit)`.
#[no_mangle]
pub extern "C" fn ee_units_cr_circle_side(unit: u64) {
    cr::circle_side(&mut game(), unit);
}

/// `EEAirplaneHoldingPattern(class, unit)`.
#[no_mangle]
pub extern "C" fn ee_units_cr_holding_pattern(class: u64, unit: u64) {
    cr::holding_pattern(&mut game(), class, unit);
}
