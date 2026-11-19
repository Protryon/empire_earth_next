//! The herd's game host (`ee_cp_herd_host`, EEHerd.cpp) and the entry points `EEHerd`'s members forward to. Absent
//! from a test binary.

#![cfg(not(test))]
// `c_long`/`c_ulong` are 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast, clippy::useless_conversion)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use super::file::SaveFile;
use super::herd::{self, EeHerdFields, EeHerdIo, EeHerdPoint, Herd, HerdHost};
use crate::units::player_save::EeUnitsPlayerSaveFn;

extern "C" {
    /// The herd's world (EEHerd.cpp): `herd` the `EEHerd*`.
    fn ee_cp_herd_host(herd: *mut c_void, op: i32, a: u64, b: u64, io: *mut EeHerdIo) -> i64;
}

struct Game {
    herd: *mut c_void,
}

impl HerdHost for Game {
    fn this(&self) -> u64 {
        self.herd as u64
    }
    fn call(&mut self, op: i32, a: u64, b: u64, io: &mut EeHerdIo) -> i64 {
        // SAFETY: the herd is live for the call; io is a live value
        unsafe { ee_cp_herd_host(self.herd, op, a, b, io) }
    }
}

/// Runs `body` over the herd.
///
/// # Safety
/// `herd` is the live `EEHerd*` whose record is `f`.
unsafe fn with<R>(herd: *mut c_void, f: *mut EeHerdFields, body: impl FnOnce(&mut Herd<'_, Game>) -> R) -> R {
    let mut game = Game { herd };
    // SAFETY: the caller's contract
    let mut h = unsafe { Herd::new(&mut game, f) };
    body(&mut h)
}

/// The record's lists made (the constructors, first).
///
/// # Safety
/// `f` is a live record without lists.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_init(f: *mut EeHerdFields) {
    // SAFETY: the caller's contract
    unsafe { (*f).mLists = herd::new_lists() };
}

/// The record's lists dropped (the destructor, last).
///
/// # Safety
/// `f` is a live record whose lists are not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_free(f: *mut EeHerdFields) {
    // SAFETY: the caller's contract
    unsafe {
        herd::free_lists((*f).mLists);
        (*f).mLists = std::ptr::null_mut();
    }
}

/// `EEHerd()`: no units, leader, target or path request, then `Clear`.
///
/// # Safety
/// `herd` is the live `EEHerd*` whose record is `f` (its lists made).
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_construct_default(herd: *mut c_void, f: *mut EeHerdFields) {
    // SAFETY: the caller's contract
    unsafe {
        (*f).mLeader = herd::EE_CP_HERD_NONE;
        (*f).mTarget = herd::EE_CP_HERD_NONE;
        (*f).mPathRequest = 0;
        with(herd, f, |h| h.clear());
    }
}

/// `EEHerd(player, animal, id)`.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_construct(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    player: u64,
    animal: c_long,
    id: u16,
) {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.construct(player, animal, id)) }
}

/// `Clear` (the destructor's).
///
/// # Safety
/// As [`ee_cp_herd_construct_default`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_clear(herd: *mut c_void, f: *mut EeHerdFields) {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.clear()) }
}

/// `AddUnit`.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`]; `unit` a live `EEComplexUnit*`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_add_unit(herd: *mut c_void, f: *mut EeHerdFields, unit: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.add_unit(unit)) }
}

/// `RemoveUnit`.
///
/// # Safety
/// As [`ee_cp_herd_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_remove_unit(herd: *mut c_void, f: *mut EeHerdFields, unit: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.remove_unit(unit)) }
}

/// `HasUnit`.
///
/// # Safety
/// As [`ee_cp_herd_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_has_unit(herd: *mut c_void, f: *mut EeHerdFields, unit: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.has_unit(unit)) }
}

/// `CanTake`.
///
/// # Safety
/// As [`ee_cp_herd_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_can_take(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    unit: u64,
    check_range: bool,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.can_take(unit, check_range)) }
}

/// `HandleUnitHit`.
///
/// # Safety
/// As [`ee_cp_herd_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_handle_unit_hit(herd: *mut c_void, f: *mut EeHerdFields, unit: u64) {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.handle_unit_hit(unit)) }
}

/// `AttackBack`.
///
/// # Safety
/// As [`ee_cp_herd_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_attack_back(herd: *mut c_void, f: *mut EeHerdFields, unit: u64, all: bool) {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.attack_back(unit, all)) }
}

/// `Update`.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_update(herd: *mut c_void, f: *mut EeHerdFields, time: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.update(time)) }
}

/// `Create` (the area's override) over `count` tiles.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`]; `tiles` holds `count` points.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_create(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    tiles: *const EeHerdPoint,
    count: usize,
) {
    // SAFETY: the caller's contract (copied before any host call)
    let tiles: Vec<EeHerdPoint> =
        if count == 0 { Vec::new() } else { unsafe { std::slice::from_raw_parts(tiles, count) }.to_vec() };
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.create(&tiles)) }
}

/// `GetRandomPoint` (the area's override) into `out`.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`]; `out` a live point.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_random_point(herd: *mut c_void, f: *mut EeHerdFields, out: *mut EeHerdPoint) {
    // SAFETY: the caller's contract
    unsafe { *out = with(herd, f, |h| h.random_point()) }
}

/// `Load` over the saved game's file (`host` its primitives).
///
/// # Safety
/// As [`ee_cp_herd_construct_default`]; `host` accepts `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_load(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    major: u32,
    minor: u32,
    ctx: *mut c_void,
    host: EeUnitsPlayerSaveFn,
) -> i32 {
    // SAFETY: the caller's contract
    let mut file = unsafe { SaveFile::new(ctx, host) };
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.load(major, minor, &mut file)) }
}

/// `Save`.
///
/// # Safety
/// As [`ee_cp_herd_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_save(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    major: u32,
    minor: u32,
    ctx: *mut c_void,
    host: EeUnitsPlayerSaveFn,
) -> i32 {
    // SAFETY: the caller's contract
    let mut file = unsafe { SaveFile::new(ctx, host) };
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.save(major, minor, &mut file)) }
}

/// `RepairPointers`.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_repair_pointers(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    major: u32,
    minor: u32,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.repair_pointers(major, minor)) }
}

/// The units' count (`mUnits.size()`).
///
/// # Safety
/// `f` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_unit_count(f: *const EeHerdFields) -> usize {
    // SAFETY: the caller's contract
    unsafe { (*(*f).mLists).units.as_slice().len() }
}

/// Unit `i`'s handle (`mUnits[i]`: a unit id).
///
/// # Safety
/// `f` is a live record; `i` below the count.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_unit_handle(f: *const EeHerdFields, i: usize) -> u32 {
    // SAFETY: the caller's contract
    unsafe { (*(*f).mLists).units.as_slice()[i] }
}
