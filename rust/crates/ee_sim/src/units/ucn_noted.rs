//! TEST ONLY notes around the unit classes' bodies (UCBD; the units' lane, round 10): each `ee_units_ucn_*` is an
//! `EEUnitClass` member's (or a building helper's) note and its body, the words in the order the C++ wrapper wrote
//! them (ee/EEUnitClassKinds.h `EEUCNote` until this round): the inputs, the body's call, the outputs. The note
//! builder is ucbd_note.rs's [`Note`]; the world it reads is the views ([`W`]).

use std::ffi::{c_long, c_void};

use crate::units::ucbd_note::{Note, NoteWorld, UcbdGame};
use crate::world::{ClassView, UnitView, WorldRead};

/// The views a note reads.
struct W;

fn read<R>(f: impl FnOnce(&WorldRead) -> R) -> R {
    // SAFETY: the class bodies run on the simulation thread, which holds the world
    let t = unsafe { WorldRead::begin() };
    f(&t)
}

impl NoteWorld for W {
    fn unit_id(&mut self, unit: u64) -> u32 {
        // SAFETY: a live unit the body was given
        read(|t| unsafe { UnitView::of(t, unit) }.map_or(0, |u| u.id()))
    }
    fn class_type(&mut self, class: u64) -> u32 {
        // SAFETY: a live class
        read(|t| unsafe { ClassView::of(t, class) }.map_or(0, |c| c.unit_type() as u32))
    }
    fn path(&mut self, unit: u64) -> Vec<[f32; 5]> {
        // SAFETY: a live complex unit; the list is read here only
        let list = unsafe { crate::units::waypoints::list(unit) };
        (0..list.len())
            .filter_map(|i| list.get(i))
            .map(|w| [w.location[0], w.location[1], w.location[2], w.distance, w.start_distance])
            .collect()
    }
    fn location(&mut self, unit: u64) -> [f32; 3] {
        // SAFETY: a live complex unit
        read(|t| unsafe { UnitView::of(t, unit) }.map_or([0.0; 3], |u| u.location()))
    }
    fn goal(&mut self, unit: u64) -> Option<(u8, Option<[f32; 3]>)> {
        // SAFETY: a live complex unit
        let goal = read(|t| unsafe { UnitView::of(t, unit) }.map_or(0, |u| u.goal_handle()));
        if goal == 0 {
            return None;
        }
        // SAFETY: the unit's live goal
        let kind = read(|t| unsafe { UnitView::of(t, unit) }.and_then(|u| u.goal()).map_or(0, |g| g.0));
        // EEGoalHasLocation: a location goal or a heal goal
        let point = (kind == crate::units::location::GOAL_LOCATION || kind == crate::combat::goal_heal::GOAL_HEAL_UNIT)
            .then(|| {
                // SAFETY: the unit's live goal
                let p = unsafe {
                    crate::units::goal_calls::ee_units_goal_location_point_any(
                        goal as usize as *const crate::units::goal_obj::EeUnitsGoalBase,
                    )
                };
                [p.x, p.y, p.z]
            });
        Some((kind, point))
    }
    fn tiles(&mut self, v: u64) -> Vec<[i32; 2]> {
        // SAFETY: a live std::vector<U2DPoint<long> >
        unsafe { crate::units::uu_host::tiles_of(v as usize as *const c_void) }
    }
    fn units(&mut self, v: u64) -> Vec<u64> {
        // SAFETY: a live std::vector of unit pointers
        unsafe { crate::units::uu_host::units_of(v as usize as *const c_void) }
    }
    fn home_base(&mut self, unit: u64) -> u64 {
        // SAFETY: a live complex unit
        read(|t| unsafe { UnitView::of(t, unit) }.map_or(0, |u| u.home_base_handle()))
    }
    fn resource(&mut self, unit: u64) -> (i32, u32) {
        // SAFETY: a live unit
        read(|t| unsafe { UnitView::of(t, unit) }.map_or((0, 0), |u| (u.resource_amount() as i32, u.resource_type() as u32)))
    }
}

fn note(kind: u32) -> Note {
    Note::begin(&mut UcbdGame, kind)
}

fn done(n: Note) {
    n.end(&mut UcbdGame);
}

fn ptr(v: u64) -> *mut c_void {
    v as usize as *mut c_void
}

fn uck(what: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: f32) -> i64 {
    // SAFETY: the noted member's live class and unit
    unsafe { crate::units::uck_kinds::game::ee_units_uck_call(what, ptr(class), ptr(unit), a, b, c, f) }
}

fn tile_of(p: *const [c_long; 2]) -> [i32; 2] {
    // SAFETY: the caller's live tile
    let t = unsafe { *p };
    [t[0] as i32, t[1] as i32]
}

use crate::units::uck_kinds as k;

// ---- EEUnitClassKinds.cpp's dispatchers

/// `CanUnloadAt` (note 2).
#[no_mangle]
pub extern "C" fn ee_units_ucn_can_unload_at(class: u64, transport: u64, x: i32, y: i32) -> bool {
    let mut n = note(2);
    n.c(&mut W, class).u(&mut W, transport).i(i64::from(x)).i(i64::from(y));
    let r = crate::units::cls_host::ee_units_tr_can_unload_at(class, transport, x, y);
    n.i(i64::from(r));
    done(n);
    r
}

/// `FilterLoadable` (note 6).
///
/// # Safety
/// `units` and `rejected` are live `std::vector<EEComplexUnit*>`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_ucn_filter_loadable(class: u64, units: u64, transport: u64, rejected: u64) {
    let mut n = note(6);
    n.c(&mut W, class).us(&mut W, units).u(&mut W, transport).us(&mut W, rejected);
    // SAFETY: the caller's contract
    unsafe { crate::units::cls_host::ee_units_tr_filter_loadable(class, ptr(units), transport, ptr(rejected)) };
    n.us(&mut W, units).us(&mut W, rejected);
    done(n);
}

/// `FindBoardingLocation` (note 32).
///
/// # Safety
/// `at` is a live point, `out` a writable one.
#[no_mangle]
pub unsafe extern "C" fn ee_units_ucn_boarding_location(class: u64, unit: u64, transport: u64, at: *const f32, out: *mut f32) -> bool {
    let mut n = note(32);
    // SAFETY: the caller's contract
    let p = unsafe { [*at, *at.add(1), *at.add(2)] };
    n.c(&mut W, class).u(&mut W, unit).u(&mut W, transport).p(p);
    let r = uck(k::EE_UNITS_UCK_DO_BOARDING_LOCATION, class, unit, transport, at as usize as i64, out as usize as i64, 0.0) != 0;
    n.i(i64::from(r));
    // SAFETY: the caller's contract
    n.p(unsafe { [*out, *out.add(1), *out.add(2)] });
    done(n);
    r
}

/// `FindRendezvousTiles` (note 4).
///
/// # Safety
/// `units` a live `std::vector<EEComplexUnit*>`, the outs writable tiles.
#[no_mangle]
pub unsafe extern "C" fn ee_units_ucn_rendezvous_tiles(
    class: u64,
    units: u64,
    transport: u64,
    out_transport: *mut [c_long; 2],
    out_unit: *mut [c_long; 2],
) {
    let mut n = note(4);
    n.c(&mut W, class).us(&mut W, units).u(&mut W, transport);
    // SAFETY: the caller's contract
    unsafe { crate::units::cls_host::ee_units_tr_rendezvous_tiles(class, ptr(units), transport, out_transport, out_unit) };
    n.t(tile_of(out_transport)).t(tile_of(out_unit));
    done(n);
}

/// `FindUnloadTiles` (note 5).
///
/// # Safety
/// `destination` a live tile, the outs writable tiles.
#[no_mangle]
pub unsafe extern "C" fn ee_units_ucn_unload_tiles(
    class: u64,
    transport: u64,
    destination: *const [c_long; 2],
    out_unload: *mut [c_long; 2],
    out_first: *mut [c_long; 2],
    out_tile: *mut [c_long; 2],
) {
    let mut n = note(5);
    n.c(&mut W, class).u(&mut W, transport).t(tile_of(destination));
    // SAFETY: the caller's contract
    unsafe { crate::units::cls_host::ee_units_tr_unload_tiles(class, transport, destination, out_unload, out_first, out_tile) };
    n.t(tile_of(out_unload)).t(tile_of(out_first)).t(tile_of(out_tile));
    done(n);
}

/// `GetHeightAt` (note 11).
#[no_mangle]
pub extern "C" fn ee_units_ucn_height_at(class: u64, unit: u64, x: f32, y: f32, z: f32) -> f32 {
    let mut n = note(11);
    n.c(&mut W, class).u(&mut W, unit).p([x, y, z]);
    let r = crate::units::cls_host::ee_units_cls_height_at(class, unit, x, y, z);
    n.f(r);
    done(n);
    r
}

/// `GetSurroundingTiles` (note 31): `tiles` a live `std::vector<EETileCoordinate>`.
#[no_mangle]
pub extern "C" fn ee_units_ucn_surrounding_tiles(class: u64, unit: u64, tiles: u64, corners: i32, append: bool) {
    let mut n = note(31);
    n.c(&mut W, class).u(&mut W, unit).i(i64::from(corners)).i(i64::from(append)).ts(&mut W, tiles);
    uck(k::EE_UNITS_UCK_DO_SURROUNDING_TILES, class, unit, tiles, i64::from(corners), i64::from(append), 0.0);
    n.ts(&mut W, tiles);
    done(n);
}

/// `HasArrived` (note 7).
#[no_mangle]
pub extern "C" fn ee_units_ucn_has_arrived(class: u64, unit: u64, value: f32) -> bool {
    let mut n = note(7);
    n.c(&mut W, class).u(&mut W, unit).f(value);
    let r = uck(k::EE_UNITS_UCK_DO_HAS_ARRIVED, class, unit, 0, 0, 0, value) != 0;
    n.i(i64::from(r));
    done(n);
    r
}

/// `HasRoom` (note 10).
#[no_mangle]
pub extern "C" fn ee_units_ucn_has_room(class: u64, unit: u64, cargo: u64) -> bool {
    let mut n = note(10);
    n.c(&mut W, class).u(&mut W, unit).u(&mut W, cargo);
    let r = uck(k::EE_UNITS_UCK_DO_HAS_ROOM, class, unit, cargo, 0, 0, 0.0) != 0;
    n.i(i64::from(r));
    done(n);
    r
}

/// `SelectUnitToRelease` (note 30).
#[no_mangle]
pub extern "C" fn ee_units_ucn_select_to_release(class: u64, unit: u64) -> bool {
    let mut n = note(30);
    n.c(&mut W, class).u(&mut W, unit);
    let r = uck(k::EE_UNITS_UCK_DO_SELECT_TO_RELEASE, class, unit, 0, 0, 0, 0.0) != 0;
    n.i(i64::from(r));
    done(n);
    r
}

// ---- EEUnitClassAir.cpp: the landable's flight

/// `LandableReturnToAirport` (note 19).
#[no_mangle]
pub extern "C" fn ee_units_ucn_return_to_airport(class: u64, unit: u64) -> bool {
    let mut n = note(19);
    n.c(&mut W, class).s(&mut W, unit);
    let r = crate::units::cls_carrier_host::ee_units_cr_return_to_airport(class, unit) != 0;
    n.i(i64::from(r));
    n.s(&mut W, unit);
    done(n);
    r
}

/// `RouteDockWaypoint` (note 20).
#[no_mangle]
pub extern "C" fn ee_units_ucn_route_dock_waypoint(class: u64, unit: u64) {
    let mut n = note(20);
    n.c(&mut W, class).s(&mut W, unit).home(&mut W, unit);
    crate::units::cls_carrier_host::ee_units_cr_route_dock_waypoint(unit);
    n.s(&mut W, unit);
    done(n);
}

/// `IsBombOnTarget` (note 17).
#[no_mangle]
pub extern "C" fn ee_units_ucn_bomb_on_target(class: u64, unit: u64) -> bool {
    let mut n = note(17);
    n.c(&mut W, class).s(&mut W, unit);
    // SAFETY: the live class and unit
    let r = unsafe {
        crate::units::uca_air::game::ee_units_uca_call(
            crate::units::uca_air::EE_UNITS_UCA_DO_BOMB_ON_TARGET,
            ptr(class),
            ptr(unit),
            0,
            0,
            0,
            std::ptr::null_mut(),
        )
    } != 0;
    n.i(i64::from(r));
    done(n);
    r
}

/// `ApproachCarrier` (note 13).
#[no_mangle]
pub extern "C" fn ee_units_ucn_approach_carrier(class: u64, unit: u64, steep: bool) {
    let mut n = note(13);
    n.c(&mut W, class).s(&mut W, unit).home(&mut W, unit).i(i64::from(steep));
    crate::units::cls_carrier_host::ee_units_cr_approach_carrier(class, unit, u8::from(steep));
    n.s(&mut W, unit);
    done(n);
}

/// `TakeOffFromCarrier` (note 14): the unit's state and its model's scale after.
#[no_mangle]
pub extern "C" fn ee_units_ucn_take_off_from_carrier(class: u64, unit: u64) {
    let mut n = note(14);
    n.c(&mut W, class).s(&mut W, unit).home(&mut W, unit);
    crate::units::cls_carrier_host::ee_units_cr_take_off_from_carrier(class, unit);
    // SAFETY: the live complex unit
    let scale = read(|t| unsafe { UnitView::of(t, unit) }.map_or(0.0, |u| u.model_scale()));
    n.s(&mut W, unit).f(scale);
    done(n);
}

/// `GetNextFlightState` (note 12).
#[no_mangle]
pub extern "C" fn ee_units_ucn_next_flight_state(class: u64, unit: u64) -> u32 {
    let mut n = note(12);
    n.c(&mut W, class).s(&mut W, unit);
    let r = crate::units::cls_host::ee_units_cls_next_flight_state(unit);
    n.i(i64::from(r));
    done(n);
    r
}

/// `ApproachRunway` (note 15).
#[no_mangle]
pub extern "C" fn ee_units_ucn_approach_runway(class: u64, unit: u64, dx: i32, dy: i32) {
    let mut n = note(15);
    n.c(&mut W, class).s(&mut W, unit).home(&mut W, unit).i(i64::from(dx)).i(i64::from(dy));
    crate::units::cls_host::ee_units_cls_approach_runway(class, unit, dx, dy);
    n.s(&mut W, unit);
    done(n);
}

/// `TakeOffFromRunway` (note 16).
#[no_mangle]
pub extern "C" fn ee_units_ucn_take_off_from_runway(class: u64, unit: u64, dx: i32, dy: i32) {
    let mut n = note(16);
    n.c(&mut W, class).s(&mut W, unit).i(i64::from(dx)).i(i64::from(dy));
    crate::units::cls_host::ee_units_cls_take_off_from_runway(class, unit, dx, dy);
    n.s(&mut W, unit);
    done(n);
}

/// `IsAwayFromGoal` (note 18).
#[no_mangle]
pub extern "C" fn ee_units_ucn_away_from_goal(class: u64, unit: u64) -> bool {
    let mut n = note(18);
    n.c(&mut W, class).s(&mut W, unit).home(&mut W, unit);
    let r = crate::units::cls_carrier_host::ee_units_cr_is_away_from_goal(unit) != 0;
    n.i(i64::from(r));
    done(n);
    r
}

// ---- EEUnitClassPeople.cpp: the citizens' searches

/// `FindResourceOfKind` (note 41): `units` a live `std::vector<EEUnit*>` (the units already taken).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_units_ucn_resource_of_kind(
    class: u64,
    category: u32,
    unit: u64,
    center: u64,
    wanted: u64,
    range: i32,
    kind: u32,
    units: u64,
    farms: bool,
) -> u64 {
    let mut n = note(41);
    n.c(&mut W, class).u(&mut W, unit).u(&mut W, center).u(&mut W, wanted).i(i64::from(range)).i(i64::from(kind));
    n.us(&mut W, units).i(i64::from(farms));
    let taken = W.units(units);
    // SAFETY: the taken units' slice
    let r = unsafe {
        crate::units::citizen_host::ee_units_cz_find_resource_of_kind(
            category,
            unit,
            center,
            wanted,
            range,
            kind,
            if taken.is_empty() { std::ptr::null() } else { taken.as_ptr() },
            taken.len(),
            u8::from(farms),
        )
    };
    n.u(&mut W, r);
    done(n);
    r
}

/// `FindStorage` (note 43).
#[no_mangle]
pub extern "C" fn ee_units_ucn_find_storage(class: u64, unit: u64, any_storage: bool) -> u64 {
    let mut n = note(43);
    n.c(&mut W, class).u(&mut W, unit).i(i64::from(any_storage)).resource(&mut W, unit);
    let r = crate::units::citizen_host::ee_units_cz_find_storage(unit, u8::from(any_storage));
    n.u(&mut W, r).resource(&mut W, unit);
    done(n);
    r
}

/// `HasGatherPoint` (note 45).
#[no_mangle]
pub extern "C" fn ee_units_ucn_has_gather_point(class: u64, unit: u64, resource: u64) -> bool {
    let mut n = note(45);
    n.c(&mut W, class).u(&mut W, unit).u(&mut W, resource);
    let r = crate::units::citizen_host::ee_units_cz_has_gather_point(unit, resource) != 0;
    n.i(i64::from(r));
    done(n);
    r
}

// ---- EEUnitClassTransports.cpp

/// `CanLoad` (note 8).
#[no_mangle]
pub extern "C" fn ee_units_ucn_can_load(class: u64, transport: u64, unit: u64) -> bool {
    let mut n = note(8);
    n.c(&mut W, class).u(&mut W, transport).u(&mut W, unit);
    let r = crate::units::cls_host::ee_units_tr_can_load(class, transport, unit);
    n.i(i64::from(r));
    done(n);
    r
}

/// `CanUnloadNear` (note 9).
///
/// # Safety
/// Three live tiles.
#[no_mangle]
pub unsafe extern "C" fn ee_units_ucn_can_unload_near(
    class: u64,
    transport: u64,
    cargo: u64,
    unload: *const [c_long; 2],
    first: *const [c_long; 2],
    destination: *const [c_long; 2],
) -> bool {
    let mut n = note(9);
    n.c(&mut W, class).u(&mut W, transport).u(&mut W, cargo).t(tile_of(unload)).t(tile_of(first)).t(tile_of(destination));
    // SAFETY: the caller's contract
    let r = unsafe { crate::units::cls_host::ee_units_tr_can_unload_near(class, transport, cargo, unload, first, destination) };
    n.i(i64::from(r));
    n.s(&mut W, cargo);
    done(n);
    r
}

// ---- EEUnitClassBuildings.cpp: the docks', gates' and walls' helpers

/// `EEGetDockFacing` (note 23), `GetNavyYardFacing` (22), `EEGetFacingQuarter` (21): `which` 0, 1, 2.
#[no_mangle]
pub extern "C" fn ee_units_ucn_facing(which: i32, x: i32, y: i32) -> i32 {
    let mut n = note([23, 22, 21][which.clamp(0, 2) as usize]);
    n.i(i64::from(x)).i(i64::from(y));
    let r = match which {
        0 => crate::units::cls_host::ee_units_cls_dock_facing(x as u32, y as u32),
        1 => crate::units::cls_host::ee_units_cls_navy_yard_facing(x as u32, y as u32),
        _ => crate::units::cls_host::ee_units_cls_facing_quarter(x, y),
    };
    n.i(i64::from(r));
    done(n);
    r
}

fn gate(what: i32, class: u64, a: u64, b: i64, c: i64) -> i64 {
    // SAFETY: the noted helper's live objects
    unsafe { crate::units::ucb_gate::game::ee_units_ucb_gate(what, ptr(class), std::ptr::null_mut(), a, b, c, 0.0) }
}

/// `FindNearestTargetBuilding` (note 35).
#[no_mangle]
pub extern "C" fn ee_units_ucn_nearest_target_building(class: u64, unit: u64) -> u64 {
    let mut n = note(35);
    n.c(&mut W, class).u(&mut W, unit);
    let r = gate(crate::units::ucb_gate::EE_UNITS_UCB_DO_NEAREST_TARGET_BUILDING, class, unit, 0, 0) as u64;
    n.u(&mut W, r);
    done(n);
    r
}

/// `EEDoesWallGateFit` (note 25): `units` a live `std::vector<EEUnit*>` (the units in the way, out).
#[no_mangle]
pub extern "C" fn ee_units_ucn_wall_gate_fit(x: i32, y: i32, direction: i32, unit_type: u32, units: u64) -> bool {
    let mut n = note(25);
    n.t([x, y]).i(i64::from(direction)).i(i64::from(unit_type));
    let r = gate(
        crate::units::ucb_gate::EE_UNITS_UCB_DO_WALL_GATE_FIT,
        0,
        units,
        i64::from(x as u32) | (i64::from(y as u32) << 32),
        i64::from(direction as u32) | (i64::from(unit_type) << 32),
    ) != 0;
    n.i(i64::from(r));
    n.us(&mut W, units);
    done(n);
    r
}

/// `EEGetWallGateTiles` (note 33): `tiles` a live `std::vector<U2DPoint<long> >`.
#[no_mangle]
pub extern "C" fn ee_units_ucn_wall_gate_tiles(x: i32, y: i32, direction: i32, tiles: u64) {
    let mut n = note(33);
    n.t([x, y]).i(i64::from(direction)).ts(&mut W, tiles);
    // SAFETY: the live vector
    unsafe { crate::units::cls_host::ee_units_cls_wall_gate_tiles(x, y, direction, ptr(tiles)) };
    n.ts(&mut W, tiles);
    done(n);
}

/// `AreEnemiesNear` (note 27) and `IsPasserNear` (note 28).
#[no_mangle]
pub extern "C" fn ee_units_ucn_gate_near(class: u64, unit: u64, passers: bool) -> bool {
    let mut n = note(if passers { 28 } else { 27 });
    n.c(&mut W, class).u(&mut W, unit);
    let r = crate::units::cls_host::ee_units_cls_gate_near(unit, passers);
    n.i(i64::from(r));
    done(n);
    r
}

/// `EEAreTilesLevel` (note 26): `tiles` a live `std::vector<U2DPoint<long> >`.
#[no_mangle]
pub extern "C" fn ee_units_ucn_tiles_level(tiles: u64) -> bool {
    let mut n = note(26);
    n.ts(&mut W, tiles);
    // SAFETY: the live vector
    let r = unsafe { crate::units::cls_host::ee_units_cls_tiles_level(ptr(tiles) as *const c_void) };
    n.i(i64::from(r));
    done(n);
    r
}

// ---- EEUnitClass.cpp: the facing test's host-read record

/// `FacesTarget` within its host-read scope (slot 0); its CBQR record (`EECombatCaptureHostRead`) when outermost.
#[no_mangle]
pub extern "C" fn ee_units_ucn_faces_target(class: u64, unit: u64, target: u64, tolerance: bool) -> bool {
    let outer = crate::combat::cg_query::ee_combat_host_read_enter(0);
    // SAFETY: the member's live class, unit and target
    let faces = unsafe {
        crate::units::uc_rest::game::ee_units_uc_rest(
            crate::units::uc_rest::EE_UNITS_UC_DO_R_FACES_TARGET,
            ptr(class),
            ptr(unit),
            target,
            i64::from(tolerance),
            0,
            std::ptr::null_mut(),
        )
    } != 0;
    if outer {
        crate::combat::cg_query::ee_combat_capture_read_of(
            0,
            crate::combat::cg_query::EE_COMBAT_HR_FACES_TARGET,
            unit,
            target,
            i32::from(faces),
            0,
            0,
            u32::from(tolerance),
        );
    }
    crate::combat::cg_query::ee_combat_host_read_leave(0);
    faces
}
