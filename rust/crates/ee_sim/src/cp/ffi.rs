//! The C ABI of the computer player's Rust (`rust/sim/cp.h`).

use std::ffi::c_void;

use super::file::SaveFile;
use super::legacy_load;
use crate::units::player_save::EeUnitsPlayerSaveFn;

/// The last save version whose computer players hold the deleted C++ managers' blocks in the original's layout.
pub const EE_CP_LEGACY_LAST_VERSION: u32 = 250;

/// Steps over a retail file's combat manager block (`EECPCombatManager::Load` at 0.228-0.250).
///
/// # Safety
/// `host` must serve the file primitives on `file` (`EESaveFilePrimitive` over an `FSFile*`).
#[no_mangle]
pub unsafe extern "C" fn ee_cp_skip_legacy_combat(
    file: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    major: u32,
    minor: u32,
) -> i32 {
    // SAFETY: the caller's contract
    let mut f = unsafe { SaveFile::new(file, host) };
    legacy_load::skip_combat_manager(&mut f, major, minor)
}

/// Steps over a retail file's civilization manager block and its resource, build, research, unit and wall managers;
/// `town_center` (x, y) gets the build manager's town centre tile, read in place.
///
/// # Safety
/// As [`ee_cp_skip_legacy_combat`]; `town_center` points to two `int32_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_skip_legacy_civilization(
    file: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    major: u32,
    minor: u32,
    town_center: *mut i32,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut f, tile) = unsafe { (SaveFile::new(file, host), &mut *(town_center as *mut [i32; 2])) };
    legacy_load::skip_civilization_manager(&mut f, major, minor, tile)
}

/// Steps over a retail file's exploration manager block.
///
/// # Safety
/// As [`ee_cp_skip_legacy_combat`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_skip_legacy_exploration(
    file: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    major: u32,
    minor: u32,
) -> i32 {
    // SAFETY: the caller's contract
    let mut f = unsafe { SaveFile::new(file, host) };
    legacy_load::skip_exploration_manager(&mut f, major, minor)
}
