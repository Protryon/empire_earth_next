//! `extern "C"` for the message box (`dialog.rs`): `EEUIFError::Display` and `Open` call these; the host is
//! `ee_screenscpp_dialog` (`User Interface Thread/EEUIFError.cpp`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;

use crate::dialog::DialogHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_dialog(dialog: *mut c_void, op: u32, a: u32, b: u32) -> u32;
    }
}

/// The box (`EEUIFError*`).
struct Dialog(*mut c_void);

impl DialogHost for Dialog {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        unsafe { sys::ee_screenscpp_dialog(self.0, op, a, b) }
    }
}

/// `EEUIFError::Display`: shows the box as the kind (`EE_SCREENS_DIALOG_*`); `flag` is the message's OK shown or
/// the prompt's password box.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_dialog_show(dialog: *mut c_void, kind: u32, alignment: u32, flag: bool) {
    crate::dialog::show(&mut Dialog(dialog), kind, alignment, flag);
}

/// `EEUIFError::Open`: lays the box's controls out for its mode.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_dialog_open(dialog: *mut c_void, mode: u32) {
    crate::dialog::open(&mut Dialog(dialog), mode);
}
