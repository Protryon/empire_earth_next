//! The C ABI of the contagion manager and its helpers (contagion_store.rs) and the game's world under them: the C++'s
//! `ee_contagion_world` (`EEContagionManager.cpp`) and the world views.
#![cfg(not(test))]
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::tech::contagion_functor::{params, EeContagionFunctorFields};
use crate::tech::contagion_save::EeContagionHelperFields;
use crate::tech::contagion_store::*;
use crate::world::{PlayerView, UnitView, WorldRead};

extern "C" {
    fn ee_contagion_world(op: u32, a: u64, b: u64, c: u64, x: f32, io: *mut c_void) -> i64;
}

/// The game's world: the C++'s `ee_contagion_world`, the world views.
pub struct CppWorld;

fn call(op: u32, a: u64, b: u64, c: u64, x: f32, io: *mut c_void) -> i64 {
    // SAFETY: the C++ host's contract: every op answers on live objects of the running game
    unsafe { ee_contagion_world(op, a, b, c, x, io) }
}

impl ContagionWorld for CppWorld {
    fn unit(&mut self, unit: u64) -> ContagionUnit {
        // SAFETY: the simulation thread inside a call from C++; a held or listed unit is live (referenced)
        let t = unsafe { WorldRead::begin() };
        match unsafe { UnitView::of(&t, unit) } {
            Some(v) => ContagionUnit {
                alive: v.hit_points() > 0,
                complex: v.complex_unit_handle(),
                id: v.id() as c_ulong,
                player: v.player().map_or(0, |p| p.handle()),
            },
            None => ContagionUnit::default(),
        }
    }
    fn enemies(&mut self, player: u64, other_player: u64) -> bool {
        // SAFETY: as `unit`; units' players are live
        let t = unsafe { WorldRead::begin() };
        let (Some(p), Some(o)) = (unsafe { PlayerView::of(&t, player) }, unsafe { PlayerView::of(&t, other_player) })
        else {
            return false;
        };
        p.enemy(o.id() as usize) != 0
    }
    fn reference(&mut self, unit: u64, on: bool) {
        call(EE_CONTAGION_WORLD_REFERENCE, unit, on as u64, 0, 0.0, std::ptr::null_mut());
    }
    fn mark(&mut self, unit: u64, calamity: u64, on: bool) {
        call(EE_CONTAGION_WORLD_MARK, unit, calamity, on as u64, 0.0, std::ptr::null_mut());
    }
    fn infect(&mut self, functor: u64, unit: u64, calamity: u64) {
        call(EE_CONTAGION_WORLD_INFECT, functor, unit, calamity, 0.0, std::ptr::null_mut());
    }
    fn cure(&mut self, functor: u64, unit: u64, calamity: u64) {
        call(EE_CONTAGION_WORLD_CURE, functor, unit, calamity, 0.0, std::ptr::null_mut());
    }
    fn apply(&mut self, functor: u64, unit: u64, calamity: u64, elapsed: f32) {
        call(EE_CONTAGION_WORLD_APPLY, functor, unit, calamity, elapsed, std::ptr::null_mut());
    }
    fn functor_update(&mut self, functor: u64, calamity: u64, elapsed: f32) {
        call(EE_CONTAGION_WORLD_FUNCTOR_UPDATE, functor, 0, calamity, elapsed, std::ptr::null_mut());
    }
    fn near(&mut self, unit: u64, radius: f32, out: &mut Vec<u64>) {
        let mut list = EeContagionUnitList(std::mem::take(out));
        call(EE_CONTAGION_WORLD_NEAR, unit, 0, 0, radius, &mut list as *mut EeContagionUnitList as *mut c_void);
        *out = list.0;
    }
    fn can_infect(&mut self, functor: u64, unit: u64) -> bool {
        call(EE_CONTAGION_WORLD_CAN_INFECT, functor, unit, 0, 0.0, std::ptr::null_mut()) != 0
    }
    fn immune(&mut self, unit: u64) -> bool {
        call(EE_CONTAGION_WORLD_IMMUNE, unit, 0, 0, 0.0, std::ptr::null_mut()) != 0
    }
    fn random(&mut self) -> f32 {
        f32::from_bits(call(EE_CONTAGION_WORLD_RANDOM, 0, 0, 0, 0.0, std::ptr::null_mut()) as u32)
    }
    fn distance_squared(&mut self, unit: u64, other: u64) -> f32 {
        f32::from_bits(call(EE_CONTAGION_WORLD_DISTANCE, unit, other, 0, 0.0, std::ptr::null_mut()) as u32)
    }
    fn unit_by_id(&mut self, id: c_ulong) -> u64 {
        call(EE_CONTAGION_WORLD_UNIT, id as u64, 0, 0, 0.0, std::ptr::null_mut()) as u64
    }
    fn calamity_checksum(&mut self, calamity: u64) -> c_long {
        call(EE_CONTAGION_WORLD_CALAMITY_CHECKSUM, calamity, 0, 0, 0.0, std::ptr::null_mut()) as c_long
    }
    fn calamity_index(&mut self, calamity: u64) -> Option<c_ulong> {
        let index = call(EE_CONTAGION_WORLD_CALAMITY_INDEX, calamity, 0, 0, 0.0, std::ptr::null_mut());
        (index >= 0).then_some(index as c_ulong)
    }
    fn calamity_functor(&mut self, calamity: u64) -> u64 {
        call(EE_CONTAGION_WORLD_CALAMITY_FUNCTOR, calamity, 0, 0, 0.0, std::ptr::null_mut()) as u64
    }
    fn calamity_dump(&mut self, calamity: u64, file: u64) {
        call(EE_CONTAGION_WORLD_CALAMITY_DUMP, calamity, file, 0, 0.0, std::ptr::null_mut());
    }
    fn text(&mut self, file: u64, text: &str) {
        let mut bytes = text.as_bytes().to_vec();
        bytes.push(0);
        call(EE_CONTAGION_WORLD_TEXT, 0, file, 0, 0.0, bytes.as_mut_ptr() as *mut c_void);
    }
    fn file(&mut self, file: u64, op: u32, value: *mut c_void) -> c_long {
        call(EE_CONTAGION_WORLD_FILE, op as u64, file, 0, 0.0, value) as c_long
    }
    fn capture(&mut self, kind: i32, a: i32, b: i32, x: f32, y: f32) {
        let mut y = y;
        call(
            EE_CONTAGION_WORLD_CAPTURE,
            kind as u32 as u64,
            a as u32 as u64,
            b as u32 as u64,
            x,
            &mut y as *mut f32 as *mut c_void,
        );
    }
}

// SAFETY (every function below): the C++ passes this module's live objects and the running game's.

/// `new EEContagionManager`: no helpers, the last id 0.
#[no_mangle]
pub extern "C" fn ee_contagion_manager_new() -> *mut EeContagionManager {
    Box::into_raw(Box::default())
}

/// `delete` of the manager (its list only: the helpers are not deleted, as the original's destructor).
///
/// # Safety
/// `m` from `ee_contagion_manager_new`, or null.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_free(m: *mut EeContagionManager) {
    if !m.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(m) });
    }
}

/// The manager's `operator=`: the other's list (the same helpers), last id and indexes.
///
/// # Safety
/// Both managers live.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_assign(m: *mut EeContagionManager, other: *const EeContagionManager) {
    // SAFETY: the caller's contract
    unsafe { *m = (*other).clone() };
}

/// `CreateContagionHelper`; the new id.
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_create(
    m: *mut EeContagionManager,
    functor: u64,
    calamity: u64,
    target: u64,
) -> c_long {
    // SAFETY: as the module's
    unsafe { (*m).create(&mut CppWorld, functor, calamity, target) }
}

/// `DestroyContagionHelper` (always true).
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_destroy(m: *mut EeContagionManager, id: c_long) -> bool {
    // SAFETY: as the module's
    unsafe { (*m).destroy(&mut CppWorld, id) }
}

/// `GetContagionHelper` (null for none).
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_helper(
    m: *const EeContagionManager,
    id: c_long,
) -> *mut EeContagionHelper {
    // SAFETY: as the module's
    unsafe { (*m).find(id).unwrap_or(std::ptr::null_mut()) }
}

/// The number of helpers.
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_count(m: *const EeContagionManager) -> usize {
    // SAFETY: as the module's
    unsafe { (*m).helpers.len() }
}

/// The helper at `i` of the list.
///
/// # Safety
/// As the module's; `i` below the count.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_at(m: *const EeContagionManager, i: usize) -> *mut EeContagionHelper {
    // SAFETY: as the module's
    unsafe { (&(*m).helpers)[i] }
}

/// `mNextHelperID`.
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_next_id(m: *const EeContagionManager) -> c_long {
    // SAFETY: as the module's
    unsafe { (*m).next_id }
}

/// The loaded helper `i`'s calamity index (`mCalamityIndexes[i]`).
///
/// # Safety
/// As the module's; `i` below the loaded count.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_calamity_index(m: *const EeContagionManager, i: usize) -> c_ulong {
    // SAFETY: as the module's
    unsafe { (&(*m).calamity_indexes)[i] }
}

/// `GetOutOfSynchChecksum`.
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_checksum(m: *const EeContagionManager) -> c_long {
    // SAFETY: as the module's
    unsafe { (*m).checksum(&mut CppWorld) }
}

/// `DumpOutOfSynch` to the file (a `FILE*`).
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_dump(m: *const EeContagionManager, file: u64) {
    // SAFETY: as the module's
    unsafe { (*m).dump(&mut CppWorld, file) }
}

/// `SwapUnit`.
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_swap_unit(m: *mut EeContagionManager, old: u64, new: u64) {
    // SAFETY: as the module's
    unsafe { (*m).swap_unit(&mut CppWorld, old, new) }
}

/// `Load` from the file (an `FSFile*`).
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_load(
    m: *mut EeContagionManager,
    file: u64,
    major: c_ulong,
    minor: c_ulong,
    counter: bool,
) -> c_long {
    // SAFETY: as the module's
    unsafe { (*m).load(&mut CppWorld, file, major, minor, counter) }
}

/// `Save` to the file (an `FSFile*`).
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_save(m: *mut EeContagionManager, file: u64, counter: bool) -> c_long {
    // SAFETY: as the module's
    unsafe { (*m).save(&mut CppWorld, file, counter) }
}

/// `RestorePointers` over the loaded calamities (`EECalamity*`, nulls for the deleted).
///
/// # Safety
/// As the module's; `calamities` valid for `count` reads.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_restore(
    m: *mut EeContagionManager,
    calamities: *const u64,
    count: usize,
) -> c_long {
    // SAFETY: as the module's
    let list = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(calamities, count) } };
    // SAFETY: as the module's
    unsafe { (*m).restore(&mut CppWorld, list) };
    0
}

/// `AddUnit`.
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_add_unit(h: *mut EeContagionHelper, unit: u64, notify: bool) -> bool {
    // SAFETY: as the module's
    unsafe { (*h).add_unit(&mut CppWorld, unit, notify) }
}

/// `Update`: the functor's getters, the immunity flag of the calamity's class.
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_update(
    h: *mut EeContagionHelper,
    blocked_by_immunity: bool,
    elapsed: f32,
) {
    // SAFETY: as the module's; the functor is an EEContagionFunctor, its first base the record
    unsafe {
        let p = params(&*((*h).functor as *const EeContagionFunctorFields), blocked_by_immunity);
        (*h).update(&mut CppWorld, &p, elapsed);
    }
}

/// The helper's numbers.
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_fields(h: *const EeContagionHelper) -> EeContagionHelperFields {
    // SAFETY: as the module's
    unsafe { (*h).fields }
}

/// `mCalamity`.
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_calamity(h: *const EeContagionHelper) -> u64 {
    // SAFETY: as the module's
    unsafe { (*h).calamity }
}

/// `mUnits.size()` (`ids`: `mUnitIDs.size()`).
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_count(h: *const EeContagionHelper, ids: bool) -> usize {
    // SAFETY: as the module's
    unsafe {
        if ids {
            (*h).unit_ids.len()
        } else {
            (*h).units.len()
        }
    }
}

/// `mUnits[i]` (`ids`: `mUnitIDs[i]`).
///
/// # Safety
/// As the module's; `i` below the count.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_unit(h: *const EeContagionHelper, ids: bool, i: usize) -> u64 {
    // SAFETY: as the module's
    unsafe {
        if ids {
            (&(*h).unit_ids)[i] as u64
        } else {
            (&(*h).units)[i]
        }
    }
}

/// `GetOutOfSynchChecksum`.
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_checksum(h: *const EeContagionHelper) -> c_long {
    // SAFETY: as the module's
    unsafe { (*h).checksum(&mut CppWorld) }
}

/// `Save` to the file (an `FSFile*`).
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_save(h: *mut EeContagionHelper, file: u64) -> c_long {
    // SAFETY: as the module's
    unsafe { (*h).save(&mut CppWorld, file) }
}

/// A fresh helper's `Load` from the file (TEST ONLY: the capture's old-layout reload): its numbers and the count
/// of unit ids it read into `*fields` and `*ids`; the result.
///
/// # Safety
/// As the module's; `fields`, `ids` valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_load_probe(
    file: u64,
    major: c_ulong,
    minor: c_ulong,
    fields: *mut EeContagionHelperFields,
    ids: *mut usize,
) -> c_long {
    let mut h = EeContagionHelper::default();
    let result = h.load(&mut CppWorld, file, major, minor);
    // SAFETY: the caller's contract
    unsafe {
        *fields = h.fields;
        *ids = h.unit_ids.len();
    }
    result
}

/// Every helper deleted (TEST ONLY: the capture's reloaded copy, whose helpers hold no units).
///
/// # Safety
/// As the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_delete_helpers(m: *mut EeContagionManager) {
    // SAFETY: as the module's
    unsafe {
        for h in std::mem::take(&mut (*m).helpers) {
            destroy(&mut CppWorld, h);
        }
    }
}
