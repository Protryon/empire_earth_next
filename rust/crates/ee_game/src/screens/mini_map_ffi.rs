//! `extern "C"` for the mini map control (`mini_map.rs`). The host of the left release and drag is
//! `EEUIRustControl`'s (`ee_screenscpp_hud_ctl`, the mini map's own ops through its `RustOp`) on the control.

use core::ffi::c_void;

use crate::screens::mini_map::{self, EeScreensMiniMapCopy, EeScreensMiniMapDraw, EeScreensMiniMapUpdate};
use crate::screens::ui_thread::UiHost;

struct MapHost(*mut c_void);

impl UiHost for MapHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        let (op, a) = mini_map::hud_op(op, a);
        let mut none = [0.0f32; 0];
        // the mouse's read hands its record (c) as the op's floats
        let f: &mut [f32] =
            if op == crate::screens::hud_ctl::EE_SCREENS_HC_CLASS + mini_map::EE_SCREENS_MM_OP_READ_MOUSE {
                // SAFETY: c is the caller's live EeScreensMiniMapMouse, twelve f32s
                unsafe { core::slice::from_raw_parts_mut(c as *mut f32, 12) }
            } else {
                &mut none
            };
        crate::screens::hud_ctl::HudHost::op(&mut crate::screens::hud_ctl::Cpp(self.0), op, a, b, f)
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
