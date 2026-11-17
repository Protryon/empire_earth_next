//! `extern "C"` for the status box (`status_box.rs`). The host is `ee_screenscpp_status_box`
//! (`User Interface Thread/EEUIFMGStatusBox.cpp`): `call` is its record of one call (the box, the unit, the held
//! text).

use core::ffi::c_void;

use crate::screens::hud::EeScreensHudUnit;
use crate::screens::status_box::{self, BoxHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    use crate::screens::hud::EeScreensHudUnit;

    extern "C" {
        pub fn ee_screenscpp_status_box(call: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64;
        pub fn ee_screenscpp_status_box_view(call: *mut c_void, fill: bool, out: *mut EeScreensHudUnit);
    }
}

struct Host(*mut c_void);

impl BoxHost for Host {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        unsafe { sys::ee_screenscpp_status_box(self.0, op, a, b, c) }
    }
    fn view(&mut self, fill: bool) -> EeScreensHudUnit {
        let mut out = EeScreensHudUnit::default();
        unsafe { sys::ee_screenscpp_status_box_view(self.0, fill, &mut out) };
        out
    }
}

/// `EEUIFMGStatusBox::Fill` (`unit`: the call's unit is set).
#[no_mangle]
pub extern "C" fn ee_screens_status_box_fill(call: *mut c_void, unit: bool) {
    status_box::fill(&mut Host(call), unit);
}

/// `EEUIFMGStatusBox::Refresh`.
#[no_mangle]
pub extern "C" fn ee_screens_status_box_refresh(call: *mut c_void) {
    status_box::refresh(&mut Host(call));
}

/// `EEUIFMGStatusBox::UpdateQueuedCounts`.
#[no_mangle]
pub extern "C" fn ee_screens_status_box_update_queued(call: *mut c_void) {
    status_box::update_queued_counts(&mut Host(call));
}

/// `EEUIFMGStatusBox::HideBackgrounds`.
#[no_mangle]
pub extern "C" fn ee_screens_status_box_hide_backgrounds(call: *mut c_void) {
    status_box::hide_backgrounds(&mut Host(call));
}

/// `EEUIFMGStatusBox::LoadIcons`.
#[no_mangle]
pub extern "C" fn ee_screens_status_box_load_icons(call: *mut c_void) {
    status_box::load_icons(&mut Host(call));
}
