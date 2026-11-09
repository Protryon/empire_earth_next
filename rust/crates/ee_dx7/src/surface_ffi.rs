//! `extern "C"` for `DX7Surface` (`Common/DX7Surface.cpp`, the forwarders). `s` is the surface's `GXSurface*`; rectangles
//! are 4 floats (left, top, right, bottom) in the caller's logical units with their surface's metrics (`NULL`: none).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;

use crate::filter::Rect;
use crate::gx::{RawGx, RawHost};
use crate::surface::{self, LockOutcome, LogicalMetrics};

unsafe fn rect(p: *const f32) -> Rect {
    unsafe { Rect { left: *p, top: *p.add(1), right: *p.add(2), bottom: *p.add(3) } }
}

unsafe fn pixel_rect(p: *const f32, metrics: *const LogicalMetrics) -> Rect {
    surface::pixel_rect(unsafe { rect(p) }, unsafe { metrics.as_ref() })
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_clear(s: *mut c_void, fill: u32) -> i32 {
    surface::clear(&mut RawGx, &mut RawHost, s, None, fill)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_clear_rect(
    s: *mut c_void,
    r: *const f32,
    metrics: *const LogicalMetrics,
    fill: u32,
) -> i32 {
    let rect = surface::to_irect(unsafe { pixel_rect(r, metrics) });
    surface::clear(&mut RawGx, &mut RawHost, s, Some(rect), fill)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_copy_to(
    source: *mut c_void,
    target: *mut c_void,
    source_rect: *const f32,
    source_metrics: *const LogicalMetrics,
    destination_rect: *const f32,
    target_metrics: *const LogicalMetrics,
    transparent: bool,
) -> i32 {
    let s = surface::to_irect(unsafe { pixel_rect(source_rect, source_metrics) });
    let d = surface::to_irect(unsafe { pixel_rect(destination_rect, target_metrics) });
    surface::copy_to(&mut RawGx, &mut RawHost, source, target, s, d, transparent)
}

/// `DX7PixelRectangle` for `CopyWithFilter`: `out` is 4 floats.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_pixel_rect(r: *const f32, metrics: *const LogicalMetrics, out: *mut f32) {
    let p = unsafe { pixel_rect(r, metrics) };
    unsafe {
        *out = p.left;
        *out.add(1) = p.top;
        *out.add(2) = p.right;
        *out.add(3) = p.bottom;
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_flip(s: *mut c_void, wait: bool) -> i32 {
    surface::flip(&mut RawGx, &mut RawHost, s, wait)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_lock(s: *mut c_void, out: *mut LockOutcome) {
    unsafe { *out = surface::lock(&mut RawGx, &mut RawHost, s) };
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_unlock(s: *mut c_void, cleared: *mut bool) -> i32 {
    let (result, was_cleared) = surface::unlock(&mut RawGx, &mut RawHost, s);
    unsafe { *cleared = was_cleared };
    result
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_restore(s: *mut c_void) -> i32 {
    surface::restore(&mut RawGx, &mut RawHost, s)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_attach(s: *mut c_void, other: *mut c_void) -> i32 {
    surface::attach(&mut RawGx, &mut RawHost, s, other)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_detach(s: *mut c_void, other: *mut c_void) -> i32 {
    surface::detach(&mut RawGx, &mut RawHost, s, other)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_set_transparency_color(s: *mut c_void, color: u32) {
    surface::set_transparency_color(&mut RawGx, s, color);
}

#[no_mangle]
pub extern "C" fn ee_dx7s_own_size(pixel_depth: u32, height: u32, width: u32) -> u32 {
    surface::own_size(pixel_depth, height, width)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_text_lock(locked: *mut bool) -> i32 {
    let (next, result) = surface::text_lock(unsafe { *locked });
    unsafe { *locked = next };
    result
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7s_text_unlock(locked: *mut bool) -> i32 {
    let (next, result) = surface::text_unlock(unsafe { *locked });
    unsafe { *locked = next };
    result
}
