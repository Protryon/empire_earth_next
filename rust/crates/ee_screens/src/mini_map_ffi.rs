//! `extern "C"` for the mini map control (`mini_map.rs`). The host of the left release and drag is
//! `ee_screenscpp_mini_map` (`User Interface Thread/EEUIMiniMap.cpp`) on the control.

use core::ffi::c_void;

use crate::mini_map::{self, EeScreensMiniMapCopy, EeScreensMiniMapDraw, EeScreensMiniMapUpdate};
use crate::ui_thread::UiHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_mini_map(map: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64;
    }
}

struct MapHost(*mut c_void);

impl UiHost for MapHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        unsafe { sys::ee_screenscpp_mini_map(self.0, op, a, b, c) }
    }
}

/// `EEUIMiniMap::CopyData`'s numbers over the record the C++ read (`copy`'s in fields; its out fields written).
///
/// # Safety
/// `copy` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_mini_map_copy_data(copy: *mut EeScreensMiniMapCopy) {
    mini_map::copy_data(&mut *copy);
}

/// `EEUIMiniMap::DrawPolygons`' plan: the flare shown or not, the lines' draw flags (bit i: line i) and the
/// copied draw rectangle.
#[no_mangle]
pub extern "C" fn ee_screens_mini_map_draw_plan(
    show_flare: bool,
    draw_lines: u32,
    left: f32,
    top: f32,
    right: f32,
    bottom: f32,
) -> EeScreensMiniMapDraw {
    let lines = [draw_lines & 1 != 0, draw_lines & 2 != 0, draw_lines & 4 != 0, draw_lines & 8 != 0];
    mini_map::draw_plan(show_flare, lines, [left, top, right, bottom])
}

/// `EEUIMiniMap::Update`'s numbers over the record the C++ read.
///
/// # Safety
/// `update` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_mini_map_update(update: *mut EeScreensMiniMapUpdate) {
    mini_map::update(&mut *update);
}

/// `EEUIMiniMap::MouseLButtonUp` on the control `map`: its result.
#[no_mangle]
pub extern "C" fn ee_screens_mini_map_mouse_l_up(map: *mut c_void) -> i64 {
    mini_map::mouse_l_up(&mut MapHost(map))
}

/// `EEUIMiniMap::MouseMove` on the control `map`: its result.
#[no_mangle]
pub extern "C" fn ee_screens_mini_map_mouse_move(map: *mut c_void) -> i64 {
    mini_map::mouse_move(&mut MapHost(map))
}
