//! The town's game host (`ee_cp_town_host`, `ee_cp_area_host`: EETown.cpp, EEHerd.cpp) and the entry points
//! `EETown`'s members forward to. Absent from a test binary.

#![cfg(not(test))]

use std::ffi::c_void;

use super::herd::{EeHerdIo, EeHerdPoint};
use super::town::{EeTownFields, Town, TownHost, TownIo};

extern "C" {
    /// The town's world (EETown.cpp): `town` the `EETown*`.
    fn ee_cp_town_host(town: *mut c_void, op: i32, a: u64, b: u64, io: *mut TownIo) -> i64;
    /// An area's operations the random point asks (EEHerd.cpp): `area` the `EEAIArea*`.
    fn ee_cp_area_host(area: *mut c_void, op: i32, io: *mut EeHerdIo) -> i64;
}

struct Game {
    town: *mut c_void,
    area: *mut c_void,
}

impl TownHost for Game {
    fn call(&mut self, op: i32, a: u64, b: u64, io: &mut TownIo) -> i64 {
        // SAFETY: the town is live for the call
        unsafe { ee_cp_town_host(self.town, op, a, b, io) }
    }
    fn area(&mut self, op: i32, v: &[i64]) -> (i64, EeHerdIo) {
        let mut io = EeHerdIo::default();
        io.v[..v.len()].copy_from_slice(v);
        // SAFETY: as above
        let r = unsafe { ee_cp_area_host(self.area, op, &mut io) };
        (r, io)
    }
}

/// # Safety
/// `town` is the live `EETown*` whose area is `area` and record `f`.
unsafe fn with<R>(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    body: impl FnOnce(&mut Town<'_, Game>) -> R,
) -> R {
    let mut game = Game { town, area };
    // SAFETY: the caller's contract
    let mut t = unsafe { Town::new(&mut game, f) };
    body(&mut t)
}

/// `EETown::AddUnit`.
///
/// # Safety
/// As `with`; `unit` a live `EEUnit*`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_add_unit(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    unit: u64,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.add_unit(unit)) }
}

/// `EETown::RemoveUnit`.
///
/// # Safety
/// As [`ee_cp_town_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_remove_unit(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    unit: u64,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.remove_unit(unit)) }
}

/// `EETown::CanTake`.
///
/// # Safety
/// As [`ee_cp_town_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_can_take(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    unit: u64,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.can_take(unit)) }
}

/// `EETown::AddTile`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_add_tile(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    x: i64,
    y: i64,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.add_tile((x as _, y as _))) }
}

/// `EETown::RemoveHullTile`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_remove_hull_tile(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    x: i64,
    y: i64,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.remove_hull_tile((x as _, y as _))) }
}

/// `EETown::Merge` of the town `other` (its record `o`).
///
/// # Safety
/// As `with`; `other` a live `EETown*` with its record `o`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_merge(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    other: u64,
    o: *const EeTownFields,
) {
    // SAFETY: the caller's contract (the other's record copied first)
    let o = unsafe { *o };
    // SAFETY: as above
    unsafe { with(town, area, f, |t| t.merge(other, &o)) }
}

/// `EETown::Reset`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_reset(town: *mut c_void, area: *mut c_void, f: *mut EeTownFields) {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.reset()) }
}

/// `EETown::UpdateNearWater`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_update_near_water(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.update_near_water()) }
}

/// `EETown::RefreshPatrolGroup`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_refresh_patrol_group(town: *mut c_void, area: *mut c_void, f: *mut EeTownFields) {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.refresh_patrol_group()) }
}

/// `EETown::GetRandomPoint` into `out`.
///
/// # Safety
/// As `with`; `out` a live point.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_random_point(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    out: *mut EeHerdPoint,
) {
    // SAFETY: the caller's contract
    unsafe { *out = with(town, area, f, |t| t.random_point()) }
}
