//! `extern "C"` for the selected unit's portrait (`portrait.rs`): `EEUICPortrait`'s members forward here with the
//! portrait; the host is `EEUIRustControl`'s (`ee_screenscpp_hud_ctl`, the class's own ops through its `RustOp`) (`User Interface Thread/EEUICPortrait.cpp`).

use core::ffi::c_void;

use crate::screens::portrait::{self, PortraitHost};

struct Portrait(*mut c_void);

impl PortraitHost for Portrait {
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
        let (op, a) = crate::screens::portrait::hud_op(op, a);
        let class = crate::screens::hud_ctl::EE_SCREENS_HC_CLASS;
        if op >= class {
            if let Some(r) = crate::screens::hud_views::portrait(op - class, a) {
                return r;
            }
        }
        crate::screens::hud_ctl::HudHost::op(&mut crate::screens::hud_ctl::Cpp(self.0), op, a, b, f)
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
