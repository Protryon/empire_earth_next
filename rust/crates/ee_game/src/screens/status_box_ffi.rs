//! `extern "C"` for the status box (`status_box.rs`). The host is `ee_screenscpp_status_box`
//! (`User Interface Thread/EEUIFMGStatusBox.cpp`): `call` is its record of one call (the box, the unit, the held
//! text).

use core::ffi::c_void;

use crate::screens::hud::EeScreensHudUnit;
use crate::screens::status_box::{self, BoxHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_status_box(call: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64;
        pub fn ee_screenscpp_status_box_unit(call: *mut c_void, two_bars: *mut bool) -> i64;
    }
}

struct Host(*mut c_void);

impl BoxHost for Host {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        // SAFETY: the host's live call record (its head asserted in EEUIFMGStatusBox.cpp)
        if let Some(r) = unsafe { crate::screens::hud_direct::status_box(self.0.cast(), op, a, b, c) } {
            return r;
        }
        unsafe { sys::ee_screenscpp_status_box(self.0, op, a, b, c) }
    }
    fn view(&mut self, fill: bool) -> EeScreensHudUnit {
        let mut two_bars = false;
        let unit = unsafe { sys::ee_screenscpp_status_box_unit(self.0, &mut two_bars) };
        crate::screens::hud_views::status_box_view(unit, two_bars, fill)
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
