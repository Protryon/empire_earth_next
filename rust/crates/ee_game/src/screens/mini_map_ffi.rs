//! `extern "C"` for the mini map control (`mini_map.rs`). The host of the left release and drag is
//! `EEUIRustControl`'s (`ee_screenscpp_hud_ctl`, the mini map's own ops through its `RustOp`) on the control.

use core::ffi::c_void;

use crate::screens::mini_map::{self};
use crate::screens::ui_thread::UiHost;

struct MapHost(*mut c_void);

impl UiHost for MapHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        // the RustOp cases that are Rust's over the C++ shims (mini_map_calls.rs)
        let mouse = if op == mini_map::EE_SCREENS_MM_OP_READ_MOUSE {
            // SAFETY: c is the caller's live EeScreensMiniMapMouse
            Some(unsafe { &mut *(c as *mut mini_map::EeScreensMiniMapMouse) })
        } else {
            None
        };
        let map = self.0 as usize;
        if let Some(r) =
            crate::screens::mini_map_calls::rust_op(&mut crate::screens::game_cpp::Ffi, map, op, a, b, mouse)
        {
            return r;
        }
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

/// cbindgen:ignore
mod press_sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_mm_release(map: *mut c_void);
    }
}

/// `EEUIMiniMap::MouseLButtonDown` (ICF-folded 0x25ae3b): captures the mouse; 0.
///
/// # Safety
/// `map` is the live mini map.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_mini_map_left_down(map: *mut c_void) -> core::ffi::c_long {
    crate::screens::game_cpp::Cpp::mm_capture(&mut crate::screens::game_cpp::Ffi, map as usize);
    0
}

/// `EEUIMiniMap::MouseRButtonUp` (0x275523): releases the captured mouse; 0.
///
/// # Safety
/// `map` is the live mini map.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_mini_map_right_up(map: *mut c_void) -> core::ffi::c_long {
    // SAFETY: the contract
    unsafe { press_sys::ee_screenscpp_mm_release(map) };
    0
}
