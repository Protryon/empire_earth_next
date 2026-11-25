//! TEST ONLY: `extern "C"` for the civilization capture's records (`civ_capture.rs`, UI lane round 12) the C++ makes:
//! CVLD (EELoadCivilization's shim) and CVAF (EEApplyCivilizationFile). CVAP is civ_manager_obj.rs' own.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_long, CStr};

use crate::screens::civ_capture;
use crate::screens::game_cpp::Ffi;

/// The bytes of a C string (none for null).
unsafe fn bytes<'a>(text: *const c_char) -> &'a [u8] {
    if text.is_null() {
        &[]
    } else {
        // SAFETY: the caller's NUL-terminated string
        unsafe { CStr::from_ptr(text) }.to_bytes()
    }
}

/// CVLD (the C strings and the upgrades live for the call).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_civc_load(
    name: *const c_char,
    directory: *const c_char,
    result: c_long,
    civ_name: *const c_char,
    upgrades: *const u32,
    count: usize,
) {
    // SAFETY: the caller's strings and upgrades
    let (name, directory, civ_name) = unsafe { (bytes(name), bytes(directory), bytes(civ_name)) };
    let upgrades = if count == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(upgrades, count) } };
    civ_capture::load(&mut Ffi, name, directory, result, civ_name, upgrades, &mut ee_sim::app::capture::commit)
}

/// CVAF (`player` the `EEPlayer*`).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_civc_apply_file(name: *const c_char, player: usize, result: c_long) {
    // SAFETY: the caller's string
    let name = unsafe { bytes(name) };
    civ_capture::apply_file(&mut Ffi, name, player, result, &mut ee_sim::app::capture::commit)
}
