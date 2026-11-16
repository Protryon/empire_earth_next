//! `extern "C"` for the improvement button (`upgrade.rs`): `EEUIBUpgrade`'s members forward here with their call
//! record (`EEUIBUpgradeCall`); the host is `ee_screenscpp_upgrade` (`User Interface Thread/EEUIBUpgrade.cpp`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;

use crate::upgrade::{self, UpgradeHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_upgrade(call: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64;
    }
}

struct Call(*mut c_void);

impl UpgradeHost for Call {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        unsafe { sys::ee_screenscpp_upgrade(self.0, op, a, b, c) }
    }
}

/// `EEUIBUpgrade::ButtonUp` (the call's out text is the reason's).
#[no_mangle]
pub extern "C" fn ee_screens_upgrade_button_up(call: *mut c_void) {
    upgrade::button_up(&mut Call(call));
}

/// `EEUIBUpgrade::GetCanUpgrade`: `unit` gets the unit it stores (left alone when none).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_upgrade_can_upgrade(call: *mut c_void, unit: *mut i64) -> bool {
    let (can, stored) = upgrade::can_upgrade(&mut Call(call));
    if let Some(u) = stored {
        *unit = u;
    }
    can
}

/// `EEUIBUpgrade::GetDisabled`.
#[no_mangle]
pub extern "C" fn ee_screens_upgrade_disabled(call: *mut c_void) -> bool {
    upgrade::disabled(&mut Call(call))
}

/// `EEUIBUpgrade::GetHelpString` (appending to the call's out text).
#[no_mangle]
pub extern "C" fn ee_screens_upgrade_help(call: *mut c_void) -> bool {
    upgrade::help(&mut Call(call))
}

/// `EEUIBUpgrade::MouseMove` (the call's location).
#[no_mangle]
pub extern "C" fn ee_screens_upgrade_mouse_move(call: *mut c_void) -> i64 {
    upgrade::mouse_move(&mut Call(call))
}

/// `EEUIBUpgrade::MouseRButtonUp`.
#[no_mangle]
pub extern "C" fn ee_screens_upgrade_mouse_r_up(call: *mut c_void) -> i64 {
    upgrade::mouse_r_up(&mut Call(call))
}

/// `EEUIBUpgrade::UpdateCost` (the call's out text is the new cost).
#[no_mangle]
pub extern "C" fn ee_screens_upgrade_update_cost(call: *mut c_void) {
    upgrade::update_cost(&mut Call(call));
}

/// `EEUIFMGStatusBar`'s mouse handler `which` (`EE_SCREENS_UP_BAR_*`; the call's location is `MouseMove`'s).
#[no_mangle]
pub extern "C" fn ee_screens_upgrade_bar_mouse(call: *mut c_void, which: u32) -> i64 {
    upgrade::bar_mouse(&mut Call(call), which)
}
