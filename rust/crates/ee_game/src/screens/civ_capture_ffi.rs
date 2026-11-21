//! TEST ONLY: `extern "C"` for the civilization capture's records (`civ_capture.rs`, UI lane round 12): the targets of
//! `ee/EECivCapture.h`'s hooks (`EECivCapture.cpp` sets them). `apply_begin` hands its state to C++ as an opaque
//! pointer that `apply_end` takes back.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_long, c_void, CStr};
use std::sync::atomic::AtomicBool;

use crate::screens::civ_capture::{self, Apply};
use crate::screens::game_cpp::Ffi;

static MANAGER: AtomicBool = AtomicBool::new(false);

/// The bytes of a C string (none for null).
unsafe fn bytes<'a>(text: *const c_char) -> &'a [u8] {
    if text.is_null() {
        &[]
    } else {
        // SAFETY: the caller's NUL-terminated string
        unsafe { CStr::from_ptr(text) }.to_bytes()
    }
}

/// CVAP's first half: its state (null when the capture is off).
#[no_mangle]
pub extern "C" fn ee_screens_civc_apply_begin(upgrade: u32, player: c_long) -> *mut c_void {
    match civ_capture::apply_begin(&mut Ffi, upgrade, player, &MANAGER, &mut ee_sim::app::capture::commit) {
        Some(a) => Box::into_raw(a) as *mut c_void,
        None => core::ptr::null_mut(),
    }
}

/// CVAP's second half (nothing for a null state).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_civc_apply_end(state: *mut c_void, result: bool) {
    if state.is_null() {
        return;
    }
    // SAFETY: the state `ee_screens_civc_apply_begin` handed out, taken back once
    let a = unsafe { *Box::from_raw(state as *mut Apply) };
    civ_capture::apply_end(&mut Ffi, a, result, &mut ee_sim::app::capture::commit)
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
