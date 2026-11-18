//! `extern "C"` for the selected-units icon bar (`icon_bar.rs`): `EEUICUnitIconBar`'s members forward here with
//! their state (`mState`, from [`ee_screens_icon_bar_new`]) and the bar; the host is `EEUIRustControl`'s (`ee_screenscpp_hud_ctl`, the class's own ops through its `RustOp`)
//! (`User Interface Thread/EEUICUnitIconBar.cpp`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;

use crate::screens::icon_bar::{IconBar, IconBarHost};

struct Bar(*mut c_void);

impl IconBarHost for Bar {
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
        let (op, a) = crate::screens::icon_bar::hud_op(op, a);
        let class = crate::screens::hud_ctl::EE_SCREENS_HC_CLASS;
        if op >= class {
            if let Some(r) = crate::screens::hud_views::icon_bar(op - class, a, b) {
                return r;
            }
        }
        if op >= class && ee_render::ge::list2d::ready() {
            if let Some(r) = crate::screens::hud_draw::icon_bar(op - class, a, f) {
                return r;
            }
        }
        if op >= class {
            if let Some(m) = crate::screens::hud_sends::icon_bar(op - class, a, b) {
                crate::screens::hud_sends::send(&m);
                return 0;
            }
        }
        crate::screens::hud_ctl::HudHost::op(&mut crate::screens::hud_ctl::Cpp(self.0), op, a, b, f)
    }
}

/// The state of a bar `width` x `height`; `icon` (four floats: left, top, right, bottom) gets one icon's rectangle.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_icon_bar_new(width: f32, height: f32, icon: *mut f32) -> *mut c_void {
    let state = IconBar::new(width, height);
    let r = state.icon;
    core::slice::from_raw_parts_mut(icon, 4).copy_from_slice(&[r.left, r.top, r.right, r.bottom]);
    Box::into_raw(Box::new(state)) as *mut c_void
}

/// The destructor: the snapshot's buttons released, the state freed.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_icon_bar_free(state: *mut c_void, bar: *mut c_void) {
    if state.is_null() {
        return;
    }
    let mut state = Box::from_raw(state as *mut IconBar);
    state.release(&mut Bar(bar));
}

/// `CopyData`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_icon_bar_copy_data(state: *mut c_void, bar: *mut c_void) -> i64 {
    (*(state as *mut IconBar)).copy_data(&mut Bar(bar))
}

/// `DrawPolygons`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_icon_bar_draw(state: *mut c_void, bar: *mut c_void) -> i64 {
    (*(state as *const IconBar)).draw(&mut Bar(bar))
}

/// `MouseLButtonUp`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_icon_bar_mouse_l_up(state: *mut c_void, bar: *mut c_void) -> i64 {
    (*(state as *mut IconBar)).mouse_l_up(&mut Bar(bar))
}

/// `MouseRButtonUp`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_icon_bar_mouse_r_up(state: *mut c_void, bar: *mut c_void) -> i64 {
    (*(state as *mut IconBar)).mouse_r_up(&mut Bar(bar))
}

/// `MouseMove`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_icon_bar_mouse_move(state: *mut c_void, bar: *mut c_void) -> i64 {
    (*(state as *mut IconBar)).mouse_move(&mut Bar(bar))
}

/// `Update`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_icon_bar_update(state: *mut c_void, bar: *mut c_void) {
    (*(state as *mut IconBar)).update(&mut Bar(bar))
}
