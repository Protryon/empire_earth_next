//! The landing aircraft's flight plans' host in the game ([`CarrierHost`] over the movement bodies' host and the
//! class bodies' C++ switch, `ee_units_cls_host`'s `EEUnitClassMoveOp`) and the entry points the `EEUnitClass`
//! members and the airplane's action operations call (cls_carrier.rs). Absent from a test binary.

#![cfg(not(test))]

use ee_base::math::types::{Ge3DPoint, GeOrientation, GeTransformation};

use crate::units::cls_carrier::{self as cr, CarrierHost};
use crate::units::cls_host::{ClsGame, EeUnitsClsIo, EE_UNITS_CLS_COMMAND_TYPE};
use crate::units::cls_host::{EE_UNITS_CLS_HOME_BASE, EE_UNITS_CLS_IS_SPACE_MAP, EE_UNITS_CLS_NEXT_WAYPOINT};
use crate::units::uu_host::Game;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use crate::world::{UnitView, WorldRead};

/// tag list `d`'s point `c` at plane `b`'s frame in the transformation `io.list` into `io.p`
pub const EE_UNITS_CLS_CR_TAG: i32 = 73;
/// unit `a`'s `mOrientation = (io.p[0], io.p[1], io.p[2])`
pub const EE_UNITS_CLS_CR_SET_ORIENTATION: i32 = 74;
/// unit `a`'s `mPhysicalModel.SetScale(io.f)`
pub const EE_UNITS_CLS_CR_SET_SCALE: i32 = 76;
/// class `b`'s `RouteDockWaypoint(unit a)`
pub const EE_UNITS_CLS_CR_ROUTE_DOCK: i32 = 77;
/// unit `a`'s player's air traffic control of its class is not NULL
pub const EE_UNITS_CLS_CR_HAS_AIR_TRAFFIC: i32 = 78;
/// `EEUGRust::NewLocation(&io.p, false, true, false, 0.0f, false, false)`
pub const EE_UNITS_CLS_CR_DECK_GOAL: i32 = 79;
/// `EELocationGoalPoint(unit a's goal)` into `io.p`
pub const EE_UNITS_CLS_CR_LOCATION_GOAL_POINT: i32 = 80;
/// unit `a`'s `SetHomeBase(` its air traffic control's first slot's airport `)`
pub const EE_UNITS_CLS_CR_HOME_FIRST_SLOT: i32 = 81;
/// `gPhysics->mRandom.Generate()` into `io.f`
pub const EE_UNITS_CLS_CR_RANDOM: i32 = 82;
/// class `b`'s `GetHeightAt(unit a, io.p)` into `io.f`
pub const EE_UNITS_CLS_CR_HEIGHT_AT: i32 = 83;
/// carrier `a`'s `GetModel()->mTagPoints[mAnimation]`
pub const EE_UNITS_CLS_CR_TAG_LIST: i32 = 215;
/// tag list `b`'s `mPoints.size()`
pub const EE_UNITS_CLS_CR_TAG_COUNT: i32 = 216;
/// unit `a`'s `GetGoal()`
pub const EE_UNITS_CLS_CR_GOAL: i32 = 220;
/// `EEGoalHasLocation(goal b)`
pub const EE_UNITS_CLS_CR_HAS_LOCATION: i32 = 221;
/// unit `a`'s `ClearGoalQueue(0)`
pub const EE_UNITS_CLS_CR_CLEAR_GOAL_QUEUE: i32 = 222;
/// unit `a`'s `SetGoal(goal b, 0, 0)`
pub const EE_UNITS_CLS_CR_SET_GOAL: i32 = 223;
/// `EESetLocationGoalPoint(goal b, io.p)`
pub const EE_UNITS_CLS_CR_SET_LOCATION_POINT: i32 = 224;
/// `ee_units_atc_slot_count(a's air traffic control)`
pub const EE_UNITS_CLS_CR_ATC_SLOTS: i32 = 225;
/// unit `a`'s `Kill(true, false, false)`
pub const EE_UNITS_CLS_CR_KILL: i32 = 226;

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

/// Unit `unit`'s `mPhysicalModel.GetLocation()`; `scaled`: `EETagPointList::Scale`d by its stats' scale (the
/// rotation rows, not the translation).
fn carrier_location(unit: u64, scaled: bool) -> GeTransformation {
    // SAFETY: the bodies' caller holds the world; a live unit
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let u = unsafe { UnitView::of(&t, unit) }.expect("a live unit");
    let mut m = GeTransformation { m: u.transform() };
    if scaled {
        let s = u.stats().expect("a unit's stats").scale();
        for row in m.m.iter_mut().take(3) {
            for v in row.iter_mut().take(3) {
                *v *= s;
            }
        }
    }
    m
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
        // the carrier's tags at the plane's frame, the location scaled by the carrier's stats
        let tags = self.cr0(EE_UNITS_CLS_CR_TAG_LIST, carrier, 0).0 as u64;
        if tags == 0 || self.cr0(EE_UNITS_CLS_CR_TAG_COUNT, 0, tags).0 == 0 {
            return None;
        }
        let mut scratch = carrier_location(carrier, true);
        let mut io = EeUnitsClsIo { list: (&mut scratch as *mut GeTransformation).cast(), ..Default::default() };
        self.call(EE_UNITS_CLS_CR_TAG, carrier, plane, i64::from(index), tags as i64, &mut io);
        Some(point(io.p))
    }
    fn set_orientation(&mut self, unit: u64, o: GeOrientation) {
        self.cr(EE_UNITS_CLS_CR_SET_ORIENTATION, unit, 0, 0, 0, Ge3DPoint { x: o.yaw, y: o.pitch, z: o.roll }, 0.0);
    }
    fn command_type(&mut self, unit: u64) -> i32 {
        self.cr0(EE_UNITS_CLS_COMMAND_TYPE, unit, 0).0 as i32
    }
    fn set_position(&mut self, unit: u64, p: Ge3DPoint) {
        // the location with the position p, then `SetTransformation(it, true)` (u12a_small.rs)
        let mut scratch = carrier_location(unit, false);
        scratch.m[0][3] = p.x;
        scratch.m[1][3] = p.y;
        scratch.m[2][3] = p.z;
        // SAFETY: the member's entry point on the live unit, with the transformation and its x and y
        unsafe {
            crate::units::u12a_small::game::ee_units_cu_tiny(
                crate::units::u12a_small::EE_UNITS_CU_T_DO_SET_TRANSFORMATION,
                unit as usize as *mut std::os::raw::c_void,
                (&scratch as *const GeTransformation).cast(),
                1,
                0,
                scratch.m[0][3],
                scratch.m[1][3],
                0.0,
            )
        };
    }
    fn set_scale(&mut self, unit: u64, s: f32) {
        self.cr(EE_UNITS_CLS_CR_SET_SCALE, unit, 0, 0, 0, Ge3DPoint::default(), s);
    }
    fn route_dock_waypoint(&mut self, class: u64, unit: u64) {
        self.cr0(EE_UNITS_CLS_CR_ROUTE_DOCK, unit, class);
    }
    fn fly_to_goal(&mut self, class: u64, p: Ge3DPoint, unit: u64) {
        // FlyToGoal's one call (uca_air.rs)
        let mut f = [p.x, p.y, p.z, 0.0];
        crate::units::cls_host::uca_call(crate::units::uca_air::EE_UNITS_UCA_DO_FLY_TO_GOAL, class, unit, 0, &mut f);
    }
    fn has_air_traffic(&mut self, unit: u64) -> bool {
        self.cr0(EE_UNITS_CLS_CR_HAS_AIR_TRAFFIC, unit, 0).0 != 0
    }
    fn deck_goal(&mut self, unit: u64, deck: Ge3DPoint) {
        // the location goal (or heal goal) moved to the deck, else a new move goal there
        let mut location = 0;
        if self.cr0(crate::units::cls_move_host::EE_UNITS_CLS_MV_GOAL, unit, 0).0 != 0 {
            let goal = self.cr0(EE_UNITS_CLS_CR_GOAL, unit, 0).0 as u64;
            if self.cr0(EE_UNITS_CLS_CR_HAS_LOCATION, unit, goal).0 != 0 {
                location = self.cr0(EE_UNITS_CLS_CR_GOAL, unit, 0).0 as u64;
            }
        }
        if location == 0 {
            let goal = self.cr(EE_UNITS_CLS_CR_DECK_GOAL, unit, 0, 0, 0, deck, 0.0).0 as u64;
            self.cr0(EE_UNITS_CLS_CR_CLEAR_GOAL_QUEUE, unit, 0);
            self.cr0(EE_UNITS_CLS_CR_SET_GOAL, unit, goal);
        } else {
            self.cr(EE_UNITS_CLS_CR_SET_LOCATION_POINT, unit, location, 0, 0, deck, 0.0);
        }
    }
    fn location_goal_point(&mut self, unit: u64) -> Ge3DPoint {
        point(self.cr0(EE_UNITS_CLS_CR_LOCATION_GOAL_POINT, unit, 0).1.p)
    }
    fn home_first_slot(&mut self, unit: u64) -> u64 {
        if self.cr0(EE_UNITS_CLS_CR_ATC_SLOTS, unit, 0).0 as u32 > 0 {
            self.cr0(EE_UNITS_CLS_CR_HOME_FIRST_SLOT, unit, 0);
        } else {
            self.cr0(EE_UNITS_CLS_CR_KILL, unit, 0);
        }
        self.home_base(unit)
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
        crate::units::uu_host::can_get_to(unit, x, y)
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
