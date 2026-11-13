//! `extern "C"` for the scenario editor's actions (`editor_actions.rs`): the paint actions' records
//! (`EESEAPaintTerrain`, `EESEAPaintElevation`, `EESEAPaintMoutains` hold one each) and the trigger area's rectangle.
//! The world-edit host is `ee_screenscpp_sea`, `ee_screenscpp_sea_units` and `ee_screenscpp_sea_remove`
//! (`Scenario Editor Actions/EESEAPaint.cpp`).

use core::ffi::c_void;

use crate::editor_actions::{
    apply_click, apply_lasso, delete_units, lasso_select, rotate_selection, SePaint, SePaintKind, SeaHost, SeuDelete,
    SeuHost,
};

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
        pub fn ee_screenscpp_seu(op: u32, unit: *mut c_void, a: isize, f: f32) -> usize;
        pub fn ee_screenscpp_seu_pick(
            rectangle: *const c_void,
            unit_type: i32,
            ids: *mut u32,
            capacity: usize,
        ) -> usize;
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

// ---- group 2: the selection's actions (`EESEAMouseClick`, `EESEALasso`, `EESEARotateUnits`, `EESEADeleteUnit`;
// the host `ee_screenscpp_seu`, `ee_screenscpp_seu_pick` in `EESEADeleteUnit.cpp`)

struct Selection;

impl SeuHost for Selection {
    fn op(&mut self, op: u32, unit: usize, a: isize, f: f32) -> usize {
        unsafe { sys::ee_screenscpp_seu(op, unit as *mut c_void, a, f) }
    }
    fn pick(&mut self, rectangle: usize, unit_type: i32) -> Vec<u32> {
        let mut capacity = 64;
        loop {
            let mut ids = vec![0u32; capacity];
            let n = unsafe {
                sys::ee_screenscpp_seu_pick(rectangle as *const c_void, unit_type, ids.as_mut_ptr(), capacity)
            };
            if n <= capacity {
                ids.truncate(n);
                return ids;
            }
            capacity = n;
        }
    }
}

/// `EESEAMouseClick::Virtual4`: the click's message (an `EEMessage*`, not NULL) applied and released.
#[no_mangle]
pub extern "C" fn ee_screens_seu_click(message: *mut c_void) {
    apply_click(&mut Selection, message as usize);
}

/// `EESEALasso::Virtual4`: the lasso's message (an `EEMSelectUnits*`) applied over its rectangle (a
/// `GERectangle*`) and released.
#[no_mangle]
pub extern "C" fn ee_screens_seu_lasso(message: *mut c_void, rectangle: *const c_void) {
    apply_lasso(&mut Selection, message as usize, rectangle as usize);
}

/// `EESEALasso::SelectUnits`.
#[no_mangle]
pub extern "C" fn ee_screens_seu_lasso_select(
    rectangle: *const c_void,
    unit_type: i32,
    deselect: u8,
    deselect_only: u8,
) {
    lasso_select(&mut Selection, rectangle as usize, unit_type, deselect != 0, deselect_only != 0);
}

/// `EESEARotateUnits::Virtual4`.
#[no_mangle]
pub extern "C" fn ee_screens_seu_rotate(yaw: f32) {
    rotate_selection(&mut Selection, yaw);
}

/// `EESEADeleteUnit::Virtual4`: the unit `id` of the player `player`, or (id 0xffffffff) the selection, whole
/// (`group`) or its first unit.
#[no_mangle]
pub extern "C" fn ee_screens_seu_delete(id: u32, player: i32, group: u8) {
    let what = if id != u32::MAX { SeuDelete::Unit { player, id } } else { SeuDelete::Selection { group: group != 0 } };
    delete_units(&mut Selection, what);
}
