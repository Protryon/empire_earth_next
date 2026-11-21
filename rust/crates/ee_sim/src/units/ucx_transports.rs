//! The transports' unit bodies over their C++ statements (the units' lane, round 7; were EEUnitClassTransports.cpp):
//! `TransportFindBoardingLocation` (the searches the unit utilities' Rust, as the C++ called them),
//! `TransportUnitCreated`, `TransportUnloadAt`, `LandTransportBadlyHurt`, `HelicopterTransportUnitCreated`,
//! `EEHelicopterTransportPathToGoal`, `SpaceTransportUnitCreated`, `SpaceTransportAddToTile`/`RemoveFromTile`,
//! `SpaceshipVirtual78`, `FortressHasRoom`, `FortressUnitCreated`, `FortressUnloadAt`. Each C++ statement one
//! `EE_UNITS_UCX_*` case of `ee_units_ucx_host` (EEUnitClassTransports.cpp; the class host's statements,
//! `EE_UNITS_UC_*`, reach EEUnitClass.cpp's through it), in the original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_anim::*;
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_produce::*;
use crate::units::ucg_ground::UcgRust;
use ee_base::math::types::GeTransformation;
use ee_base::math::um::{floor_unsigned, lrintf};
use std::os::raw::c_long;

// ---- the statements (from 1800)
/// complex unit `a`'s `mUnitClass->mDomains`
pub const EE_UNITS_UCX_DOMAINS_OF: i32 = 1800;
/// `f[0] = ` complex unit `a`'s `mStats->mMaxVelocity`
pub const EE_UNITS_UCX_MAX_VELOCITY_OF: i32 = 1801;
/// the tiles list's `clear()`
pub const EE_UNITS_UCX_TILES_CLEAR: i32 = 1802;
/// `ee_units_uu_tiles_around(unit, complex unit a, 1, 0, 0, &the tiles list)`
pub const EE_UNITS_UCX_TILES_AROUND: i32 = 1803;
/// `ee_units_uu_pick_tile_near(&the tiles list, unit, complex unit a, 0, the point at b)`
pub const EE_UNITS_UCX_PICK_TILE_NEAR: i32 = 1804;
/// complex unit `a`'s `mUnitClass->mTileSize`
pub const EE_UNITS_UCX_TILE_SIZE_OF: i32 = 1805;
/// the point at `b` = `GE3DPoint(f[0], f[1], f[2])`
pub const EE_UNITS_UCX_SET_POINT: i32 = 1806;
/// unit `a`'s `SetResourceType(class->mResourceType)`
pub const EE_UNITS_UCX_SET_RESOURCE_TYPE: i32 = 1807;
/// unit `a`'s `mResourceType`
pub const EE_UNITS_UCX_RESOURCE_TYPE: i32 = 1808;
/// unit `a`'s `SetResourceAmount(class->mResourceAmount)`
pub const EE_UNITS_UCX_SET_RESOURCE_AMOUNT: i32 = 1809;
/// `unit->UnloadCargoAtTile(b, c, a != 0)`
pub const EE_UNITS_UCX_UNLOAD_AT_TILE: i32 = 1810;
/// unit `a`'s `mComplexUnit`
pub const EE_UNITS_UCX_COMPLEX_FIELD_OF: i32 = 1811;
/// complex unit `a`'s `GetNumCargoUnits()`
pub const EE_UNITS_UCX_CARGO_COUNT_OF: i32 = 1812;
/// complex unit `a`'s `GetCargo(b)`
pub const EE_UNITS_UCX_CARGO_OF: i32 = 1813;
/// complex unit `a`'s `UnloadCargoNow(complex unit b)`
pub const EE_UNITS_UCX_UNLOAD_NOW_OF: i32 = 1814;
/// `f[0] = class->GetHeightAt(unit, GE3DPoint(f[0], f[1], f[2]))`
pub const EE_UNITS_UCX_HEIGHT_AT: i32 = 1815;
/// `unit->SetTransformation(the transformation at a, true)`
pub const EE_UNITS_UCX_SET_TRANSFORMATION: i32 = 1816;
/// unit `a`'s `mTileX`
pub const EE_UNITS_UCX_TILE_X_OF: i32 = 1817;
/// unit `a`'s `mTileY`
pub const EE_UNITS_UCX_TILE_Y_OF: i32 = 1818;
/// tile `a`'s `mUnits = AddUnit(unit b, mUnits)`
pub const EE_UNITS_UCX_TILE_ADD: i32 = 1819;
/// tile `a`'s `RemoveUnit(unit b)`
pub const EE_UNITS_UCX_TILE_REMOVE: i32 = 1820;
/// `unit->mPlayer->GetTotalPopulation(false, false)`
pub const EE_UNITS_UCX_TOTAL_POPULATION: i32 = 1821;
/// `unit->SetBuilt(0, 1, 1)`
pub const EE_UNITS_UCX_SET_BUILT: i32 = 1822;
/// complex unit `a`'s `mUnitClass->mUnitType`
pub const EE_UNITS_UCX_TYPE_OF: i32 = 1823;
/// `unit->GetPower()`
pub const EE_UNITS_UCX_POWER: i32 = 1824;
/// `gFortressUnloadCost`
pub const EE_UNITS_UCX_UNLOAD_COST: i32 = 1825;
/// `unit->SetPower(b)`
pub const EE_UNITS_UCX_SET_POWER: i32 = 1826;
/// `unit->mPlayer->mEvents.AddEvent(U2DPoint<long>(b, c), 2, unit->GetPlayer(), 0x274e, false)`
pub const EE_UNITS_UCX_UNLOAD_EVENT: i32 = 1827;
/// goal `a`'s `GetDestination(unit, f[0..3])`
pub const EE_UNITS_UCX_DESTINATION: i32 = 1828;
/// `f[0] = class->mCruiseAltitude`
pub const EE_UNITS_UCX_CRUISE_ALTITUDE: i32 = 1829;
/// `unit->mUnitClass->mTransportCapacity`
pub const EE_UNITS_UCX_TRANSPORT_CAPACITY: i32 = 1830;

// ---- the bodies (`ee_units_ucx_call`'s `what`)
pub const EE_UNITS_UCX_DO_TRANSPORT_CREATED: i32 = 1;
pub const EE_UNITS_UCX_DO_UNLOAD_AT: i32 = 2;
pub const EE_UNITS_UCX_DO_BADLY_HURT: i32 = 3;
pub const EE_UNITS_UCX_DO_HELICOPTER_CREATED: i32 = 4;
pub const EE_UNITS_UCX_DO_HELICOPTER_PATH: i32 = 5;
pub const EE_UNITS_UCX_DO_SPACE_CREATED: i32 = 6;
pub const EE_UNITS_UCX_DO_SPACE_ADD: i32 = 7;
pub const EE_UNITS_UCX_DO_SPACE_REMOVE: i32 = 8;
pub const EE_UNITS_UCX_DO_SPACESHIP_CREATED: i32 = 9;
pub const EE_UNITS_UCX_DO_FORTRESS_HAS_ROOM: i32 = 10;
pub const EE_UNITS_UCX_DO_FORTRESS_CREATED: i32 = 11;
pub const EE_UNITS_UCX_DO_FORTRESS_UNLOAD_AT: i32 = 12;

const MOVEMENT_LAND: i64 = 2;
const RESOURCE_NONE: i64 = 0;
const UNIT_TYPE_SIEGE04_TOWER: i64 = 0x67;
const UNIT_TYPE_SIEGE06_HEAVY_TOWER: i64 = 0x134;
const UNIT_TYPE_TELEPORTER: i64 = 0x313;
const GOAL_EXPLORE: i64 = 10;

/// The searches and the landing point these bodies call (the unit utilities' FFI functions, as the C++ called them).
pub trait UcxRust: UcRust + UcgRust {
    /// `ee_units_find_landing_point(unit, target)`
    fn landing_point(&mut self, unit: u64, target: &mut [f32; 3]);
}

/// The class's resource on a new unit (its amount when it has one)
fn resource<H: UcHost + ?Sized>(s: &mut S<'_, H>, unit: u64) {
    s.a(EE_UNITS_UCX_SET_RESOURCE_TYPE, unit);
    if s.a(EE_UNITS_UCX_RESOURCE_TYPE, unit) != RESOURCE_NONE {
        s.a(EE_UNITS_UCX_SET_RESOURCE_AMOUNT, unit);
    }
}

/// `TransportFindBoardingLocation(transport, cargo, location, out)`: where cargo leaves the transport (a tile around
/// it for a unit that cannot move, else a free placement near the location; water for non-land units); whether one
/// was found. `out` is the caller's point (written as the original wrote it).
pub fn boarding_location<H: UcHost + UcRust + ?Sized>(
    h: &mut H,
    class: u64,
    transport: u64,
    cargo: u64,
    location: [f32; 3],
    out: u64,
) -> bool {
    let mut s = S { h: &mut *h, class, unit: transport };
    let allow_water = s.a(EE_UNITS_UCX_DOMAINS_OF, cargo) & MOVEMENT_LAND == 0;
    if s.float(EE_UNITS_UCX_MAX_VELOCITY_OF, cargo, 0) <= 0.0 {
        s.s(EE_UNITS_UCX_TILES_CLEAR);
        s.a(EE_UNITS_UCX_TILES_AROUND, cargo);
        return s.ab(EE_UNITS_UCX_PICK_TILE_NEAR, cargo, out as i64) != 0;
    }
    let mut position = [0.0f32; 3];
    s.point(EE_UNITS_UC_POSITION, transport, 0, 0, &mut position);
    let size = s.a(EE_UNITS_UCX_TILE_SIZE_OF, cargo) as c_long;
    let y = floor_unsigned(location[1]);
    let x = floor_unsigned(location[0]);
    let mut t = GeTransformation::IDENTITY;
    let p = FindPlacement {
        point: position,
        unit: transport,
        class: 0,
        x,
        y,
        size: size as i32,
        rings: 1,
        flags: [false, allow_water, false, true],
        target: 0,
    };
    let found = h.find_placement(&p, &mut t.m);
    let mut s = S { h, class, unit: transport };
    let mut point = if found { [t.m[0][3], t.m[1][3], t.m[2][3]] } else { location };
    s.point(EE_UNITS_UCX_SET_POINT, 0, out as i64, 0, &mut point);
    found
}

/// `TransportUnitCreated(unit)`: a new transport set up as any unit, with its class's resource type and amount.
pub fn transport_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    base_unit_created(&mut *h, class, unit);
    let mut s = S { h, class, unit };
    resource(&mut s, unit);
}

/// `TransportUnloadAt(transport, x, y)`: the cargo unloaded at a tile (siege towers with the tower flag set).
pub fn unload_at<H: UcHost + ?Sized>(h: &mut H, class: u64, transport: u64, x: i64, y: i64) {
    let mut s = S { h, class, unit: transport };
    let unit_type = s.a(EE_UNITS_UC_CLASS_UNIT_TYPE, class);
    let tower = unit_type == UNIT_TYPE_SIEGE04_TOWER || s.a(EE_UNITS_UC_CLASS_UNIT_TYPE, class) == UNIT_TYPE_SIEGE06_HEAVY_TOWER;
    s.abc(EE_UNITS_UCX_UNLOAD_AT_TILE, u64::from(tower), x, y);
}

/// `LandTransportBadlyHurt(unit)`: all cargo unloaded at once.
pub fn badly_hurt<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit: 0 };
    let transport = s.a(EE_UNITS_UCX_COMPLEX_FIELD_OF, unit) as u64;
    let count = s.a(EE_UNITS_UCX_CARGO_COUNT_OF, transport) as u32;
    let mut cargo = Vec::new();
    for i in 0..count {
        let one = s.ab(EE_UNITS_UCX_CARGO_OF, transport, i64::from(i)) as u64;
        if one != 0 {
            cargo.push(one);
        }
    }
    for one in cargo {
        s.ab(EE_UNITS_UCX_UNLOAD_NOW_OF, transport, one as i64);
    }
}

/// The new unit lifted to its flying height (the helicopter's and the spaceship's creation tail), from the location
/// it had before its set up
fn lift<H: UcHost + ?Sized>(s: &mut S<'_, H>, t: &mut GeTransformation) {
    let unit = s.unit;
    let mut p = [t.m[0][3], t.m[1][3], t.m[2][3], 0.0];
    s.f(EE_UNITS_UCX_HEIGHT_AT, 0, 0, 0, &mut p);
    let z = p[0];
    t.m[2][3] = z;
    s.a(EE_UNITS_UCX_SET_TRANSFORMATION, addr(&mut t.m));
    s.with_float(EE_UNITS_UC_SET_HEIGHT_OF, unit, z);
}

/// `HelicopterTransportUnitCreated(unit)`: a new transport helicopter set up as a transport, lifted to its flying
/// height.
pub fn helicopter_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut t = GeTransformation::IDENTITY;
    h.uc(EE_UNITS_UC_LOCATION_MATRIX, class, unit, unit, addr(&mut t.m) as i64, 0, &mut [0.0; 4]);
    transport_created(&mut *h, class, unit);
    lift(&mut S { h, class, unit }, &mut t);
}

/// `SpaceshipVirtual78(unit)`: a new spaceship set up as any unit, lifted to its flying height.
pub fn spaceship_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut t = GeTransformation::IDENTITY;
    h.uc(EE_UNITS_UC_LOCATION_MATRIX, class, unit, unit, addr(&mut t.m) as i64, 0, &mut [0.0; 4]);
    base_unit_created(&mut *h, class, unit);
    lift(&mut S { h, class, unit }, &mut t);
}

/// `SpaceTransportUnitCreated(unit)`: a new space transport at its flying height, with the class's resource.
pub fn space_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    spaceship_created(&mut *h, class, unit);
    let mut s = S { h, class, unit };
    resource(&mut s, unit);
}

/// `SpaceTransportAddToTile(unit)` (`add`) / `SpaceTransportRemoveFromTile(unit)`: the transport on (off) its own
/// tile only.
pub fn space_tile<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, add: bool) {
    let mut s = S { h, class, unit: 0 };
    let complex = s.a(EE_UNITS_UCX_COMPLEX_FIELD_OF, unit);
    let x = s.a(EE_UNITS_UCX_TILE_X_OF, unit);
    let y = s.a(EE_UNITS_UCX_TILE_Y_OF, unit);
    let tile = s.abc(EE_UNITS_UC_GET_TILE, 0, x, y) as u64;
    if tile != 0 {
        s.ab(if add { EE_UNITS_UCX_TILE_ADD } else { EE_UNITS_UCX_TILE_REMOVE }, tile, complex);
    }
}

/// `EEHelicopterTransportPathToGoal(class, unit)`: the path to the goal's destination (an explore goal's at the
/// cruise altitude), at its landing point.
pub fn helicopter_path<H: UcHost + UcxRust + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let mut position = [0.0f32; 3];
    s.point(EE_UNITS_UC_LOCATION, unit, 0, 0, &mut position);
    let goal = s.s(EE_UNITS_UC_GOAL_STATE_SELF) as u64;
    let mut target = [0.0f32; 3];
    s.point(EE_UNITS_UCX_DESTINATION, goal, 0, 0, &mut target);
    if s.a(EE_UNITS_UC_GOAL_KIND, goal) == GOAL_EXPLORE {
        target[2] = s.float(EE_UNITS_UCX_CRUISE_ALTITUDE, 0, 0);
    }
    h.landing_point(unit, &mut target);
    h.path_toward(unit, position, target, false);
}

/// `FortressHasRoom(unit, cargo)`: whether the player's population has room for the cargo's cost.
pub fn fortress_has_room<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, cargo: u64) -> bool {
    let mut s = S { h, class, unit };
    let cargo_class = s.a(EE_UNITS_UC_CLASS_OF, cargo) as u64;
    let total = s.s(EE_UNITS_UCX_TOTAL_POPULATION) as c_long;
    let cost = s.a(EE_UNITS_UC_CLASS_POPULATION_COST, cargo_class) as c_long;
    total.wrapping_add(cost) <= s.s(EE_UNITS_UC_POPULATION_LIMIT) as c_long
}

/// `FortressUnitCreated(unit)`: a new fortress set up as built, with a 1 s regeneration timer.
pub fn fortress_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    h.uc(EE_UNITS_UCX_SET_BUILT, class, unit, 0, 0, 0, &mut [0.0; 4]);
    base_unit_created(&mut *h, class, unit);
    let mut s = S { h, class, unit };
    s.b(EE_UNITS_UC_SET_REGENERATION_TIMER, 1000);
}

/// `FortressUnloadAt(transport, x, y)`: a teleporter needs its unload cost in faith and spends it in proportion to
/// the cargo unloaded (else warns its owner); other fortresses unload as transports.
pub fn fortress_unload_at<H: UcHost + ?Sized>(h: &mut H, class: u64, transport: u64, x: i64, y: i64) {
    let mut s = S { h: &mut *h, class, unit: transport };
    if s.a(EE_UNITS_UCX_TYPE_OF, transport) != UNIT_TYPE_TELEPORTER {
        unload_at(h, class, transport, x, y);
        return;
    }
    let cost = s.s(EE_UNITS_UCX_UNLOAD_COST) as u16;
    if (s.s(EE_UNITS_UCX_POWER) as c_long) < c_long::from(cost) {
        s.abc(EE_UNITS_UCX_UNLOAD_EVENT, 0, x, y);
        return;
    }
    let before = (s.s(EE_UNITS_UCX_TRANSPORT_CAPACITY) as u32).wrapping_sub(s.s(EE_UNITS_UC_CARGO_ROOM) as u32);
    unload_at(&mut *h, class, transport, x, y);
    let mut s = S { h, class, unit: transport };
    let capacity = s.s(EE_UNITS_UCX_TRANSPORT_CAPACITY) as c_long;
    let after = (capacity as u32).wrapping_sub(s.s(EE_UNITS_UC_CARGO_ROOM) as u32);
    if before != after {
        let fraction = before.wrapping_sub(after) as f32 / capacity as f32;
        let power = s.s(EE_UNITS_UCX_POWER) as c_long;
        s.b(EE_UNITS_UCX_SET_POWER, i64::from(power.wrapping_sub(lrintf(fraction * f32::from(cost)) as c_long)));
    }
}

/// Runs body `what` of `class` on `unit` (`a` an object, `b`, `c` values); the choice, or 0.
#[allow(clippy::too_many_arguments)]
pub fn run<H: UcHost + UcxRust + ?Sized>(h: &mut H, what: i32, class: u64, unit: u64, a: u64, b: i64, c: i64) -> i64 {
    match what {
        EE_UNITS_UCX_DO_TRANSPORT_CREATED => transport_created(h, class, unit),
        EE_UNITS_UCX_DO_UNLOAD_AT => unload_at(h, class, unit, b, c),
        EE_UNITS_UCX_DO_BADLY_HURT => badly_hurt(h, class, a),
        EE_UNITS_UCX_DO_HELICOPTER_CREATED => helicopter_created(h, class, unit),
        EE_UNITS_UCX_DO_HELICOPTER_PATH => helicopter_path(h, class, unit),
        EE_UNITS_UCX_DO_SPACE_CREATED => space_created(h, class, unit),
        EE_UNITS_UCX_DO_SPACE_ADD => space_tile(h, class, a, true),
        EE_UNITS_UCX_DO_SPACE_REMOVE => space_tile(h, class, a, false),
        EE_UNITS_UCX_DO_SPACESHIP_CREATED => spaceship_created(h, class, unit),
        EE_UNITS_UCX_DO_FORTRESS_HAS_ROOM => return i64::from(fortress_has_room(h, class, unit, a)),
        EE_UNITS_UCX_DO_FORTRESS_CREATED => fortress_created(h, class, unit),
        EE_UNITS_UCX_DO_FORTRESS_UNLOAD_AT => fortress_unload_at(h, class, unit, b, c),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::uc_produce::FindPlacement;
    use std::os::raw::c_void;

    extern "C" {
        fn ee_units_ucx_host(
            op: i32,
            class: *mut c_void,
            unit: *mut c_void,
            a: u64,
            b: i64,
            c: i64,
            f: *mut [f32; 4],
        ) -> i64;
    }

    /// The transports' host (`ee_units_ucx_host`; the class host's statements pass through it).
    struct GameX;

    impl UcHost for GameX {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: the body runs inside the C++ member that handed over its live class and unit
            unsafe { ee_units_ucx_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    impl UcRust for GameX {
        fn find_placement(&mut self, p: &FindPlacement, m: &mut [[f32; 4]; 4]) -> bool {
            // SAFETY: the buffers are live for the call; the units are the body's, live
            unsafe {
                crate::units::uu_ffi::ee_units_uu_find_placement(
                    &p.point,
                    p.unit,
                    p.class,
                    p.x,
                    p.y,
                    p.size,
                    p.rings,
                    u8::from(p.flags[0]),
                    u8::from(p.flags[1]),
                    u8::from(p.flags[2]),
                    u8::from(p.flags[3]),
                    m,
                    p.target,
                ) != 0
            }
        }
        fn dock_ship_placement(
            &mut self,
            _point: [f32; 3],
            _class: u64,
            _unit: u64,
            _face: bool,
            _facing_only: bool,
            _m: &mut [[f32; 4]; 4],
        ) -> bool {
            unreachable!("no transport body docks")
        }
        fn can_stand_on(&mut self, _x: i32, _y: i32, _unit: u64) -> bool {
            unreachable!("no transport body tests standing")
        }
    }

    impl UcgRust for GameX {
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

    impl UcxRust for GameX {
        fn landing_point(&mut self, unit: u64, target: &mut [f32; 3]) {
            // SAFETY: the target is live for the call; the unit is the body's, live
            unsafe { crate::units::landing::ee_units_find_landing_point(unit, target) }
        }
    }

    /// The transports' body `what` (`EE_UNITS_UCX_DO_*`) of `class` on `unit`: `a` an object, `b`, `c` values; the
    /// choice, or 0.
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass`, `unit` a live `EEComplexUnit` (or NULL), `a` a live unit (or 0), on the
    /// simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucx_call(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        b: i64,
        c: i64,
    ) -> i64 {
        run(&mut GameX, what, class as usize as u64, unit as usize as u64, a, b, c)
    }

    /// `TransportFindBoardingLocation(transport, cargo, location, out)` of `class`.
    ///
    /// # Safety
    /// `class` a live `EEUnitClass`, the units live `EEComplexUnit`s, `location` and `out` three floats each.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucx_boarding(
        class: *mut c_void,
        transport: *mut c_void,
        cargo: *mut c_void,
        location: *const [f32; 3],
        out: *mut [f32; 3],
    ) -> bool {
        // SAFETY: the caller's point
        let at = unsafe { *location };
        boarding_location(
            &mut GameX,
            class as usize as u64,
            transport as usize as u64,
            cargo as usize as u64,
            at,
            out as usize as u64,
        )
    }
}

#[cfg(test)]
#[path = "ucx_transports_tests.rs"]
mod tests;
