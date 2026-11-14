//! `extern "C"` for the selected unit's portrait (`portrait.rs`): `EEUICPortrait`'s members forward here with the
//! portrait; the host is `ee_screenscpp_portrait` (`User Interface Thread/EEUICPortrait.cpp`).

use core::ffi::c_void;

use crate::portrait::{self, PortraitHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_portrait(portrait: *mut c_void, op: u32, a: i64, b: i64, f: *mut f32) -> i64;
    }
}

struct Portrait(*mut c_void);

impl PortraitHost for Portrait {
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
        unsafe { sys::ee_screenscpp_portrait(self.0, op, a, b, f.as_mut_ptr()) }
    }
}

/// `EEUICPortrait::Update`.
#[no_mangle]
pub extern "C" fn ee_screens_portrait_update(portrait: *mut c_void) {
    portrait::update(&mut Portrait(portrait));
}

/// `EEUICPortrait::DrawPolygons`.
#[no_mangle]
pub extern "C" fn ee_screens_portrait_draw(portrait: *mut c_void) -> i64 {
    portrait::draw(&mut Portrait(portrait))
}

/// `EEUICPortrait::GetHelpString`: whether there is a tooltip (the host builds its text).
#[no_mangle]
pub extern "C" fn ee_screens_portrait_help(portrait: *mut c_void) -> bool {
    portrait::help(&mut Portrait(portrait))
}

/// `EEUICPortrait::MouseLButtonDown` (and the right press).
#[no_mangle]
pub extern "C" fn ee_screens_portrait_mouse_down(portrait: *mut c_void) -> i64 {
    portrait::mouse_down(&mut Portrait(portrait))
}

/// `EEUICPortrait::SetUnit(unit)` (an `EEUnit*`, NULL for none).
#[no_mangle]
pub extern "C" fn ee_screens_portrait_set_unit(portrait: *mut c_void, unit: *mut c_void) {
    portrait::set_unit(&mut Portrait(portrait), unit as i64);
}
