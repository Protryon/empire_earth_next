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
        if op >= class {
            if let Some(r) = crate::screens::hud_draw::portrait(self.0, op - class, a, f) {
                return r;
            }
        }
        if op >= class {
            if let Some(m) =
                crate::screens::hud_sends::portrait(op - class, || crate::screens::hud_views::unit_id(a), b)
            {
                crate::screens::hud_sends::send(&m);
                return 0;
            }
        }
        if op >= class {
            let ffi = &mut crate::screens::game_cpp::Ffi;
            if let Some(r) = crate::screens::portrait_calls::op(ffi, self.0 as usize, op - class, a, b, f) {
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

/// `EEUICPortrait::GetHelpString`: whether there is a tooltip; `out` (a `UWideString*`) emptied, then the host builds
/// the text there.
///
/// # Safety
/// `out` is the caller's live string.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_portrait_help(portrait: *mut c_void, out: *mut c_void) -> bool {
    // SAFETY: the contract
    unsafe {
        sys::ee_screenscpp_pt_help_target(out);
        sys::ee_screenscpp_pt_help_clear();
    }
    portrait::help(&mut Portrait(portrait))
}

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_pt_help_target(out: *mut c_void);
        pub fn ee_screenscpp_pt_help_clear();
    }
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

/// The constructor's body (`EEUICPortrait::EEUICPortrait`: the rectangle, the level shown).
#[no_mangle]
pub extern "C" fn ee_screens_portrait_init(
    portrait: *mut c_void,
    left: f32,
    top: f32,
    right: f32,
    bottom: f32,
    show: bool,
) {
    crate::screens::portrait_calls::init(
        &mut crate::screens::game_cpp::Ffi,
        portrait as usize,
        [left, top, right, bottom],
        show,
    )
}

/// The destructor's body (`EEUICPortrait::~EEUICPortrait`).
#[no_mangle]
pub extern "C" fn ee_screens_portrait_free(portrait: *mut c_void) {
    crate::screens::portrait_calls::free(&mut crate::screens::game_cpp::Ffi, portrait as usize)
}

/// `EEUICPortrait::DrawTextA` -> its result.
#[no_mangle]
pub extern "C" fn ee_screens_portrait_draw_text(portrait: *mut c_void) -> i64 {
    crate::screens::portrait_calls::draw_text(&mut crate::screens::game_cpp::Ffi, portrait as usize)
}

/// `EEUICPortrait::MouseMove` -> 0.
#[no_mangle]
pub extern "C" fn ee_screens_portrait_mouse_move(portrait: *mut c_void) -> i64 {
    crate::screens::portrait_calls::mouse_move(&mut crate::screens::game_cpp::Ffi, portrait as usize)
}
