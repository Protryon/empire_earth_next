//! The ground and sea classes' unit bodies over their C++ statements (the units' lane, round 7; were
//! EEUnitClassGround.cpp): `AmbientAddToTile`/`AmbientRemoveFromTile`, `ResourceAddToTile`,
//! `ResourceSimpleUnitCreated`, `LandUnitCreated`, `LandCallParatroopPlane` (its path cls_move.rs's, as `PathToward`
//! called it), `NavalUnitCreated`, `NavalHasArrived`, `UpdateResourceUnit`. Each C++ statement one `EE_UNITS_UCG_*`
//! case of `ee_units_ucg_host` (EEUnitClassGround.cpp; the class host's statements, `EE_UNITS_UC_*`, reach
//! EEUnitClass.cpp's through it), in the original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_produce::*;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::GeTransformation;
use ee_base::math::um::floor;
use std::os::raw::c_long;

// ---- the statements (from 1600)
/// unit `a`'s `mTileX`
pub const EE_UNITS_UCG_TILE_X_OF: i32 = 1600;
/// unit `a`'s `mTileY`
pub const EE_UNITS_UCG_TILE_Y_OF: i32 = 1601;
/// tile `a`'s `mUnits = AddUnit(unit b, mUnits)`
pub const EE_UNITS_UCG_TILE_ADD: i32 = 1602;
/// tile `a`'s `RemoveUnit(unit b)`
pub const EE_UNITS_UCG_TILE_REMOVE: i32 = 1603;
/// unit `a`'s `mIsQueuedForDelete`
pub const EE_UNITS_UCG_QUEUED_FOR_DELETE: i32 = 1604;
/// `class->BaseSimpleUnitCreated(unit a)`
pub const EE_UNITS_UCG_BASE_SIMPLE_CREATED: i32 = 1605;
/// unit `a`'s `SetResourceType(class->mResourceType)`
pub const EE_UNITS_UCG_SET_RESOURCE_TYPE: i32 = 1606;
/// unit `a`'s `mResourceType`
pub const EE_UNITS_UCG_RESOURCE_TYPE: i32 = 1607;
/// unit `a`'s `SetResourceAmount(class->mResourceAmount)`
pub const EE_UNITS_UCG_SET_RESOURCE_AMOUNT: i32 = 1608;
/// `unit->mUnitClass->mUnitType`
pub const EE_UNITS_UCG_UNIT_TYPE: i32 = 1609;
/// `unit->SetPower(unit->mUnitClass->mPower)`
pub const EE_UNITS_UCG_SET_CLASS_POWER: i32 = 1610;
/// `::gWorld.GetMap()->GetTile(b, c)`
pub const EE_UNITS_UCG_MAP_TILE: i32 = 1611;
/// `f[0] = ` class `a`'s `mCruiseAltitude`
pub const EE_UNITS_UCG_CRUISE_ALTITUDE_OF: i32 = 1612;
/// class `a`'s `CreateUnitAt(the transformation at b, 0, 0, unit)->mComplexUnit`
pub const EE_UNITS_UCG_CREATE_PLANE: i32 = 1613;
/// `EECreateCarrierAircraft(complex unit a, unit->mPlayer->mPlayerID)`
pub const EE_UNITS_UCG_CARRIER_AIRCRAFT: i32 = 1614;
/// `EEUGRust::NewUnloadTransport(complex unit a, b, c, false)`
pub const EE_UNITS_UCG_NEW_UNLOAD_TRANSPORT: i32 = 1615;
/// complex unit `a`'s `SetFlightTime(b)`
pub const EE_UNITS_UCG_SET_FLIGHT_TIME_OF: i32 = 1616;
/// `unit->SetTransformation(the transformation at a, true)`
pub const EE_UNITS_UCG_SET_TRANSFORMATION: i32 = 1617;
/// `unit->mCollisionState`
pub const EE_UNITS_UCG_COLLISION_STATE: i32 = 1618;
/// `unit->mWaypoints.size()`
pub const EE_UNITS_UCG_WAYPOINT_COUNT: i32 = 1619;
/// `unit->GetNextWaypointPoint(f[0..3])`
pub const EE_UNITS_UCG_NEXT_WAYPOINT: i32 = 1620;
/// `f[0] = unit->mCollisionRadius`
pub const EE_UNITS_UCG_COLLISION_RADIUS: i32 = 1621;
/// `unit->UpdateResourcesRotting()`
pub const EE_UNITS_UCG_RESOURCES_ROTTING: i32 = 1622;

// ---- the bodies (`ee_units_ucg_call`'s `what`)
pub const EE_UNITS_UCG_DO_AMBIENT_ADD: i32 = 1;
pub const EE_UNITS_UCG_DO_AMBIENT_REMOVE: i32 = 2;
pub const EE_UNITS_UCG_DO_RESOURCE_ADD: i32 = 3;
pub const EE_UNITS_UCG_DO_RESOURCE_CREATED: i32 = 4;
pub const EE_UNITS_UCG_DO_LAND_CREATED: i32 = 5;
pub const EE_UNITS_UCG_DO_CALL_PARATROOP_PLANE: i32 = 6;
pub const EE_UNITS_UCG_DO_NAVAL_CREATED: i32 = 7;
pub const EE_UNITS_UCG_DO_NAVAL_ARRIVED: i32 = 8;
pub const EE_UNITS_UCG_DO_UPDATE_RESOURCE_UNIT: i32 = 9;

const RESOURCE_NONE: i64 = 0;
const UNIT_TYPE_RADIO_MAN: i64 = 0x2ef;
const UNIT_TYPE_PARATROOPER_PLANE: i64 = 0x2e1;
const ACTION_FLY: i64 = 1006;
const ACTION_RESOURCES_ROTTING: i64 = 6001;
const ACTION_UNIT_ROTTING: i64 = 6002;
const GOAL_LOCATION: i64 = 1;
/// `3.1415927f` (the paratroop plane's yaw)
const YAW_PI: f32 = 3.1415927;
/// a slow request's speed (`NavalHasArrived`)
const ARRIVED_SPEED: f32 = 0.05;

/// The movement's Rust a body calls (cls_move.rs's FFI function, as `PathToward` called it).
pub trait UcgRust {
    /// `ee_units_mv_path_toward(unit, from, to, snap)`
    fn path_toward(&mut self, unit: u64, from: [f32; 3], to: [f32; 3], snap: bool);
}

/// The tiles of a footprint (`size` from tile `x`, `y`), its own tile first: `op` on each that exists
fn each_footprint_tile<H: UcHost + ?Sized>(s: &mut S<'_, H>, unit: u64, op: i32) {
    let size = s.s(EE_UNITS_UC_TILE_SIZE) as c_long;
    let x = s.a(EE_UNITS_UCG_TILE_X_OF, unit) as c_long;
    let y = s.a(EE_UNITS_UCG_TILE_Y_OF, unit) as c_long;
    footprint(s, unit, op, size, x, y);
}

fn footprint<H: UcHost + ?Sized>(s: &mut S<'_, H>, unit: u64, op: i32, size: c_long, x: c_long, y: c_long) {
    let (end_x, end_y) = (x.wrapping_add(size), y.wrapping_add(size));
    let tile = s.abc(EE_UNITS_UC_GET_TILE, 0, i64::from(x), i64::from(y)) as u64;
    if tile != 0 {
        s.ab(op, tile, unit as i64);
    }
    for i in x..end_x {
        for j in y..end_y {
            if i == x && j == y {
                continue;
            }
            let tile = s.abc(EE_UNITS_UC_GET_TILE, 0, i64::from(i), i64::from(j)) as u64;
            if tile != 0 {
                s.ab(op, tile, unit as i64);
            }
        }
    }
}

/// `AmbientAddToTile(unit)` (`add`) / `AmbientRemoveFromTile(unit)`: the unit on (off) every map tile of its
/// footprint, its own tile first.
pub fn ambient_tiles<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, add: bool) {
    let mut s = S { h, class, unit: 0 };
    each_footprint_tile(&mut s, unit, if add { EE_UNITS_UCG_TILE_ADD } else { EE_UNITS_UCG_TILE_REMOVE });
}

/// `ResourceAddToTile(unit)`: the resource on every tile of its footprint (its own tile first), unless it is queued
/// for deletion.
pub fn resource_add<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit: 0 };
    let size = s.s(EE_UNITS_UC_TILE_SIZE) as c_long;
    let x = s.a(EE_UNITS_UCG_TILE_X_OF, unit) as c_long;
    let y = s.a(EE_UNITS_UCG_TILE_Y_OF, unit) as c_long;
    if s.a(EE_UNITS_UCG_QUEUED_FOR_DELETE, unit) != 0 {
        return;
    }
    footprint(&mut s, unit, EE_UNITS_UCG_TILE_ADD, size, x, y);
}

/// The class's resource on a new unit (its amount when it has one)
fn resource<H: UcHost + ?Sized>(s: &mut S<'_, H>, unit: u64) {
    s.a(EE_UNITS_UCG_SET_RESOURCE_TYPE, unit);
    if s.a(EE_UNITS_UCG_RESOURCE_TYPE, unit) != RESOURCE_NONE {
        s.a(EE_UNITS_UCG_SET_RESOURCE_AMOUNT, unit);
    }
}

/// `ResourceSimpleUnitCreated(unit)`: a new resource unit with the class's resource type and amount.
pub fn resource_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit: 0 };
    s.a(EE_UNITS_UCG_BASE_SIMPLE_CREATED, unit);
    resource(&mut s, unit);
}

/// `LandUnitCreated(unit)`: a new land unit with its class's resource; the radio man's power.
pub fn land_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    base_unit_created(&mut *h, class, unit);
    let mut s = S { h, class, unit };
    resource(&mut s, unit);
    if s.s(EE_UNITS_UCG_UNIT_TYPE) == UNIT_TYPE_RADIO_MAN {
        s.s(EE_UNITS_UCG_SET_CLASS_POWER);
    }
}

/// `LandCallParatroopPlane(unit)`: a paratroop plane over the last map tile beyond the unit in +y, flying to the
/// unit's goal location to unload its cargo.
pub fn call_paratroop_plane<H: UcHost + UcgRust + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let plane_class = s.b(EE_UNITS_UC_CLASS_AT, UNIT_TYPE_PARATROOPER_PLANE) as u64;
    let mut t = GeTransformation::IDENTITY;
    let x = s.a(EE_UNITS_UCG_TILE_X_OF, unit) as c_long;
    let mut y = s.a(EE_UNITS_UCG_TILE_Y_OF, unit) as c_long;
    while s.abc(EE_UNITS_UCG_MAP_TILE, 0, i64::from(x), i64::from(y)) != 0 {
        y = y.wrapping_add(1);
    }
    t.m[0][3] = x as f32;
    t.m[1][3] = y.wrapping_sub(1) as f32;
    t.m[2][3] = s.float(EE_UNITS_UCG_CRUISE_ALTITUDE_OF, plane_class, 0);
    t.set_orientation(RotationOrder::Ypr, YAW_PI, 0.0, 0.0);
    let plane = s.ab(EE_UNITS_UCG_CREATE_PLANE, plane_class, addr(&mut t.m) as i64) as u64;
    s.a(EE_UNITS_UCG_CARRIER_AIRCRAFT, plane);
    let mut f = [YAW_PI, 0.0, 0.0, 0.0];
    s.f(EE_UNITS_UC_SET_ORIENTATION, plane, 0, 0, &mut f);
    let mut target = [0.0f32; 3];
    let goal = s.s(EE_UNITS_UC_GOAL_STATE_SELF) as u64;
    s.point(EE_UNITS_UC_GOAL_LOCATION, goal, 0, 0, &mut target);
    let unload =
        s.abc(EE_UNITS_UCG_NEW_UNLOAD_TRANSPORT, plane, target[0] as c_long as i64, target[1] as c_long as i64);
    s.ab(EE_UNITS_UC_QUEUE_GOAL_ON, plane, unload);
    s.ab(EE_UNITS_UCG_SET_FLIGHT_TIME_OF, plane, 1_000_000);
    let start = [t.m[0][3], t.m[1][3], t.m[2][3]];
    h.path_toward(plane, start, target, true);
    let mut s = S { h, class, unit };
    s.ab(EE_UNITS_UC_START_ACTION, plane, ACTION_FLY);
    s.a(EE_UNITS_UC_ACTION_CHANGED, plane);
}

/// `NavalUnitCreated(unit)`: a new ship at sea level (z 0), with the class's resource type and amount.
pub fn naval_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut t = GeTransformation::IDENTITY;
    h.uc(EE_UNITS_UC_LOCATION_MATRIX, class, unit, unit, addr(&mut t.m) as i64, 0, &mut [0.0; 4]);
    base_unit_created(&mut *h, class, unit);
    let mut s = S { h, class, unit };
    t.m[2][3] = 0.0;
    s.a(EE_UNITS_UCG_SET_TRANSFORMATION, addr(&mut t.m));
    resource(&mut s, unit);
}

/// `NavalHasArrived(unit, value)`: whether the ship may stop (already sinking, a slow request, or at the path's end
/// tile with its speed reached).
pub fn naval_arrived<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, value: f32) -> bool {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UCG_COLLISION_STATE) == 8 || value <= ARRIVED_SPEED {
        return true;
    }
    let count = s.s(EE_UNITS_UCG_WAYPOINT_COUNT) as u32;
    let mut position = [0.0f32; 3];
    s.point(EE_UNITS_UCG_NEXT_WAYPOINT, 0, 0, 0, &mut position);
    position[2] = 0.0;
    if s.s(EE_UNITS_UC_PRODUCER_TILE_X) as c_long == floor(position[0])
        && s.s(EE_UNITS_UC_PRODUCER_TILE_Y) as c_long == floor(position[1])
        && (count > 1
            || s.float(EE_UNITS_UCG_COLLISION_RADIUS, 0, 0) >= 2.0
            || (s.b(EE_UNITS_UC_QUEUE_LEN, 0) as u32 > 1 && {
                let goal = s.s(EE_UNITS_UC_GOAL_STATE_SELF) as u64;
                goal != 0 && s.a(EE_UNITS_UC_GOAL_KIND, goal) == GOAL_LOCATION
            }))
    {
        return !(s.float(EE_UNITS_UCG_COLLISION_RADIUS, 0, 0) < value);
    }
    false
}

/// `UpdateResourceUnit(unit)`: a fish's orbitals; its resources rot, or a rotting unit counts its time down.
pub fn update_resource_unit<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    s.s(EE_UNITS_UC_UPDATE_ORBITALS);
    let command = s.s(EE_UNITS_UC_COMMAND_KIND);
    if command == ACTION_RESOURCES_ROTTING {
        s.s(EE_UNITS_UCG_RESOURCES_ROTTING);
    } else if command == ACTION_UNIT_ROTTING {
        s.s(EE_UNITS_UC_UPDATE_ROTTING);
    }
}

/// Runs body `what` of `class` on `unit` (`a` a simple unit, `f` a value); the choice, or 0.
pub fn run<H: UcHost + UcgRust + ?Sized>(h: &mut H, what: i32, class: u64, unit: u64, a: u64, f: f32) -> i64 {
    match what {
        EE_UNITS_UCG_DO_AMBIENT_ADD => ambient_tiles(h, class, a, true),
        EE_UNITS_UCG_DO_AMBIENT_REMOVE => ambient_tiles(h, class, a, false),
        EE_UNITS_UCG_DO_RESOURCE_ADD => resource_add(h, class, a),
        EE_UNITS_UCG_DO_RESOURCE_CREATED => resource_created(h, class, a),
        EE_UNITS_UCG_DO_LAND_CREATED => land_created(h, class, unit),
        EE_UNITS_UCG_DO_CALL_PARATROOP_PLANE => call_paratroop_plane(h, class, unit),
        EE_UNITS_UCG_DO_NAVAL_CREATED => naval_created(h, class, unit),
        EE_UNITS_UCG_DO_NAVAL_ARRIVED => return i64::from(naval_arrived(h, class, unit, f)),
        EE_UNITS_UCG_DO_UPDATE_RESOURCE_UNIT => update_resource_unit(h, class, unit),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        fn ee_units_ucg_host(
            op: i32,
            class: *mut c_void,
            unit: *mut c_void,
            a: u64,
            b: i64,
            c: i64,
            f: *mut [f32; 4],
        ) -> i64;
    }

    /// The ground classes' host (`ee_units_ucg_host`; the class host's statements pass through it).
    struct GameG;

    impl UcHost for GameG {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: the body runs inside the C++ member that handed over its live class and unit
            unsafe { ee_units_ucg_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    impl UcgRust for GameG {
        fn path_toward(&mut self, unit: u64, from: [f32; 3], to: [f32; 3], snap: bool) {
            crate::units::cls_move_host::ee_units_mv_path_toward(
                unit,
                from[0],
                from[1],
                from[2],
                to[0],
                to[1],
                to[2],
                u8::from(snap),
            );
        }
    }

    /// The ground classes' body `what` (`EE_UNITS_UCG_DO_*`) of `class` on `unit`: `a` a simple unit, `f` a value;
    /// the choice, or 0.
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass`, `unit` a live `EEComplexUnit` (or NULL), `a` a live `EEUnit` (or 0), on the
    /// simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucg_call(what: i32, class: *mut c_void, unit: *mut c_void, a: u64, f: f32) -> i64 {
        run(&mut GameG, what, class as usize as u64, unit as usize as u64, a, f)
    }
}

#[cfg(test)]
#[path = "ucg_ground_tests.rs"]
mod tests;
