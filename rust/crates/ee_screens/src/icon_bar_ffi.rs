//! `extern "C"` for the selected-units icon bar (`icon_bar.rs`): `EEUICUnitIconBar`'s members forward here with
//! their state (`mState`, from [`ee_screens_icon_bar_new`]) and the bar; the host is `ee_screenscpp_icon_bar`
//! (`User Interface Thread/EEUICUnitIconBar.cpp`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;

use crate::icon_bar::{IconBar, IconBarHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_icon_bar(bar: *mut c_void, op: u32, a: i64, b: i64, f: *mut f32) -> i64;
    }
}

struct Bar(*mut c_void);

impl IconBarHost for Bar {
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
        unsafe { sys::ee_screenscpp_icon_bar(self.0, op, a, b, f.as_mut_ptr()) }
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
