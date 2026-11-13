//! `extern "C"` for the scenario editor's actions (`editor_actions.rs`): the paint actions' records
//! (`EESEAPaintTerrain`, `EESEAPaintElevation`, `EESEAPaintMoutains` hold one each) and the trigger area's rectangle.
//! The world-edit host is `ee_screenscpp_sea`, `ee_screenscpp_sea_units` and `ee_screenscpp_sea_remove`
//! (`Scenario Editor Actions/EESEAPaint.cpp`).

use core::ffi::c_void;

use crate::editor_actions::{area_rectangle, SePaint, SePaintKind, SeaHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sea(op: u32, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_sea_units(
            x: i32,
            y: i32,
            units: *mut *mut c_void,
            paths: *mut u8,
            capacity: usize,
        ) -> usize;
        pub fn ee_screenscpp_sea_remove(unit: *mut c_void);
    }
}

struct Host;

impl SeaHost for Host {
    fn op(&mut self, op: u32, i: &mut [i32; 6], f: &mut [f32; 5]) -> i32 {
        unsafe { sys::ee_screenscpp_sea(op, i.as_mut_ptr(), f.as_mut_ptr()) }
    }
    fn tile_units(&mut self, x: i32, y: i32) -> Vec<(usize, bool)> {
        // the host references the units it writes; a second call with room for all when the first had too little
        // (the host writes nothing then)
        let mut capacity = 16;
        loop {
            let mut units = vec![core::ptr::null_mut::<c_void>(); capacity];
            let mut paths = vec![0u8; capacity];
            let n = unsafe { sys::ee_screenscpp_sea_units(x, y, units.as_mut_ptr(), paths.as_mut_ptr(), capacity) };
            if n <= capacity {
                return (0..n).map(|k| (units[k] as usize, paths[k] != 0)).collect();
            }
            capacity = n;
        }
    }
    fn remove_unit(&mut self, unit: usize) {
        unsafe { sys::ee_screenscpp_sea_remove(unit as *mut c_void) }
    }
}

/// A paint action's record (opaque to C++).
pub struct EeScreensSeaPaint(SePaint);

/// `code` over the record taken out of its box: no Rust reference into the box lives across the host's calls into
/// the world.
///
/// # Safety
/// `paint` comes from `ee_screens_sea_paint_new`.
unsafe fn with_record<R>(paint: *mut EeScreensSeaPaint, code: impl FnOnce(&mut SePaint) -> R) -> R {
    let mut record = unsafe { core::mem::replace(&mut (*paint).0, SePaint::new(SePaintKind::Bulldozer)) };
    let result = code(&mut record);
    unsafe { (*paint).0 = record };
    result
}

/// A paint action's record: the terrain paint (a terrain, b texture), the elevation paint (a the level) or the
/// bulldozer. Freed by `ee_screens_sea_paint_free` (the action's destructor).
#[no_mangle]
pub extern "C" fn ee_screens_sea_paint_new(kind: u32, a: i32, b: i32) -> *mut EeScreensSeaPaint {
    let kind = SePaintKind::from_code(kind, a, b);
    Box::into_raw(Box::new(EeScreensSeaPaint(SePaint::new(kind))))
}

/// # Safety
/// `paint` comes from `ee_screens_sea_paint_new` (or is NULL) and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sea_paint_free(paint: *mut EeScreensSeaPaint) {
    if !paint.is_null() {
        drop(unsafe { Box::from_raw(paint) });
    }
}

/// The action's `SetArea`: queues the area when the paint changes anything there; 1 when it queued.
///
/// # Safety
/// `paint` comes from `ee_screens_sea_paint_new`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sea_paint_set_area(
    paint: *mut EeScreensSeaPaint,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
) -> u8 {
    unsafe { with_record(paint, |record| u8::from(record.set_area(&mut Host, left, top, right, bottom))) }
}

/// The action's `Virtual4`: every queued area painted (under the UI copy lock and the world's write lock).
///
/// # Safety
/// `paint` comes from `ee_screens_sea_paint_new`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sea_paint_run(paint: *mut EeScreensSeaPaint) {
    unsafe { with_record(paint, |record| record.run(&mut Host)) }
}

/// `EESEAPaintArea::SetCorner`'s rectangle -> out[0..4] = left, top, right, bottom.
///
/// # Safety
/// `out` points at four integers.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sea_area_rectangle(
    start_x: i32,
    start_y: i32,
    end_x: i32,
    end_y: i32,
    out: *mut i32,
) {
    let (l, t, r, b) = area_rectangle(start_x, start_y, end_x, end_y);
    unsafe { core::slice::from_raw_parts_mut(out, 4).copy_from_slice(&[l, t, r, b]) };
}
