//! `extern "C"` for the command buttons' presses (`status_button.rs`). The host is `ee_screenscpp_status_button`
//! (`User Interface Thread/EEUIFMGStatusButton.cpp`): `press` is its record of one press (or `CopyData` call) (the button, the unload's
//! transport, `CanProduce`'s reason).

use core::ffi::c_void;

use crate::status_button::{button_up, copy_data, help_string, right_button_up, ButtonHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_status_button(press: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64;
    }
}

struct Host(*mut c_void);

impl ButtonHost for Host {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        unsafe { sys::ee_screenscpp_status_button(self.0, op, a, b, c) }
    }
}

/// `EEUIFMGStatusButton::ButtonUp` over the host's press record.
#[no_mangle]
pub extern "C" fn ee_screens_status_button_up(press: *mut c_void) {
    button_up(&mut Host(press));
}

/// `EEUIFMGStatusButton::MouseRButtonUp` over the host's press record.
#[no_mangle]
pub extern "C" fn ee_screens_status_button_right_up(press: *mut c_void) {
    right_button_up(&mut Host(press));
}

/// `EEUIFMGStatusButton::CopyData` over the host's press record: `UIButton::CopyData`'s result.
#[no_mangle]
pub extern "C" fn ee_screens_status_button_copy_data(press: *mut c_void) -> i64 {
    copy_data(&mut Host(press))
}

/// `EEUIFMGStatusButton::GetHelpString` over the host's press record (its result string): always true.
#[no_mangle]
pub extern "C" fn ee_screens_status_button_help(press: *mut c_void) -> bool {
    help_string(&mut Host(press))
}
