//! `extern "C"` for the civilization manager's bodies (`civ_manager_obj.rs`, UI lane round 12): each C++ member's
//! one forward. Out parameters are the members' own.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong};

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

/// `EELoadCivilization(name, directory, out name, out upgrades)` (`civ_load.rs`; the `UString*`s, the
/// `UWideString*` and the `std::vector<EECivUpgrade>*`) -> its result.
#[no_mangle]
pub extern "C" fn ee_screens_civl_load(name: usize, directory: usize, out_name: usize, out_upgrades: usize) -> c_long {
    crate::screens::civ_load::load_civilization(&mut Ffi, name, directory, out_name, out_upgrades)
}
