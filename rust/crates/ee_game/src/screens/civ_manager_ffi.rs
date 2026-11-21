//! `extern "C"` for the civilization manager's bodies (`civ_manager_obj.rs`, UI lane round 12): each C++ member's
//! one forward. Out parameters are the members' own.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_long, c_ulong, c_void};

use crate::screens::civ_manager_obj as m;
use crate::screens::game_cpp::Ffi;

/// `Load` (0), `LoadPremadeCivilizations` (1), `EECivPowersManager::Load` (2) of `file` (an `FSFile*`).
#[no_mangle]
pub extern "C" fn ee_screens_civm_load(file: usize, which: u32) -> c_long {
    m::load(&mut Ffi, file, which)
}

/// `GetCivilizations(out)` (`all`) / `GetCivilizationsOfType(out, era)`.
#[no_mangle]
pub extern "C" fn ee_screens_civm_civilizations(out: usize, era: i32, all: bool) {
    m::civilizations(&mut Ffi, out, if all { None } else { Some(era) })
}

#[no_mangle]
pub extern "C" fn ee_screens_civm_apply_upgrade(upgrade: u32, player: c_long) -> bool {
    m::apply_upgrade(&mut Ffi, upgrade, player)
}

/// `EECivPowersManager::HasPower(player, id)` (`player` an `EEPlayer*`).
#[no_mangle]
pub extern "C" fn ee_screens_civm_has_power(player: usize, id: c_long) -> bool {
    m::has_power(&mut Ffi, player, id)
}

/// `SplitAttribute(upgrade, category, index)`: each out written when given.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_civm_split(upgrade: u32, category: *mut c_ulong, index: *mut c_ulong) {
    let (c, i) = m::split(upgrade);
    // SAFETY: the caller's outs (or null)
    unsafe {
        if !category.is_null() {
            *category = c;
        }
        if !index.is_null() {
            *index = i;
        }
    }
}

/// `MatchesPremadeCivilization(file, name, upgrades, name_id)` (`characters` the name's `GetLength()`).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_civm_matches(
    file: *const c_char,
    name: *const c_char,
    characters: u32,
    upgrades: *const u32,
    count: usize,
    name_id: *mut c_long,
) -> bool {
    let mut id = 0i32;
    // SAFETY: the caller's strings and upgrades
    let result =
        unsafe { ee_sim::tech::civ::ee_tech_civ_matches_premade(file, name, characters, upgrades, count, &mut id) };
    // SAFETY: the caller's out
    unsafe { *name_id = id as c_long };
    result
}

/// `GetCategory(category, out)`: the record in place, `out` left alone without one.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_civm_category(category: u32, out: *mut *mut c_void) -> bool {
    let entry = ee_sim::tech::civ::ee_tech_civ_category(category);
    if !entry.is_null() {
        // SAFETY: the caller's out
        unsafe { *out = entry as *mut c_void };
    }
    !entry.is_null()
}

/// `GetCategoryNameID(category, out)`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_civm_category_name_id(category: u32, out: *mut c_long) -> bool {
    let entry = ee_sim::tech::civ::ee_tech_civ_category(category);
    if entry.is_null() {
        return false;
    }
    // SAFETY: a category in place; the caller's out
    unsafe { *out = (*entry).mNameID as c_long };
    true
}

/// `ToLegacy(upgrade, out)`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_civm_to_legacy(upgrade: u32, out: *mut c_ulong) -> bool {
    let mut legacy = 0u32;
    // SAFETY: a local out
    let result = unsafe { ee_sim::tech::civ::ee_tech_civ_to_legacy(upgrade, &mut legacy) };
    // SAFETY: the caller's out
    unsafe { *out = legacy as c_ulong };
    result
}

