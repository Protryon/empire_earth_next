//! `extern "C"` for the scenario editor's terrain tab whole in Rust (`editor_terrain.rs`): the object `EEUIFSETerrain`
//! holds, made, called and freed by it, the paint radios' `Check` and the paint area's `CreatePaintAction`. The
//! shared control ops (below 100) are `ee_screenscpp_sepl` (`Scenario Editor/EESERBUnitTypes.cpp`), a control by id
//! the map tab's `ee_screenscpp_semp` (500..), the mouse and the highlight the elevation tab's `ee_screenscpp_seel`
//! (900..), the tab's own `ee_screenscpp_setn` (1000.., `Scenario Editor/EEUIFSETerrain.cpp`).

use core::ffi::c_void;

use crate::screens::editor_players::SeplHost;
use crate::screens::editor_terrain::{
    paint_type_check, TerrainTab, EE_SCREENS_SETN_COLOR_CHANGE, EE_SCREENS_SETN_ENTER, EE_SCREENS_SETN_LEAVE,
    EE_SCREENS_SETN_MOUSE_MOVE, EE_SCREENS_SETN_TYPE_CHANGE, EE_SCREENS_SETN_UPDATE,
};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_semp(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_seel(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_setn(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        let (p, i, f) = (p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr());
        unsafe {
            match op {
                1000.. => sys::ee_screenscpp_setn(op, p, i, f),
                900.. => sys::ee_screenscpp_seel(op, p, i, f),
                500.. => sys::ee_screenscpp_semp(op, p, i, f),
                _ => sys::ee_screenscpp_sepl(op, p, i, f),
            }
        }
    }
}

/// The terrain tab (opaque to C++).
pub struct EeScreensSeTerrain(TerrainTab);

/// `EEUIFSETerrain`'s constructor: every control made on `form` from the control index `*index` (advanced), the
/// lists' events over the C++ `tab`, the paint area over the form's edge `scroll` and `brushes`. Freed by
/// `ee_screens_setn_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_setn_new(
    tab: *mut c_void,
    form: *mut c_void,
    scroll: *mut c_void,
    brushes: *mut c_void,
    index: *mut u32,
) -> *mut EeScreensSeTerrain {
    let mut next = unsafe { *index };
    let t = TerrainTab::new(&mut Host, tab as usize, form as usize, scroll as usize, brushes as usize, &mut next);
    unsafe { *index = next };
    Box::into_raw(Box::new(EeScreensSeTerrain(t)))
}

/// The destructor: every control deleted (the original's order), the previews released, the object freed.
///
/// # Safety
/// `tab` comes from `ee_screens_setn_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_setn_free(tab: *mut EeScreensSeTerrain) {
    if tab.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(tab) };
    b.0.destroy(&mut Host);
}

/// A member (`EE_SCREENS_SETN_UPDATE`, `_MOUSE_MOVE` at (`x`, `y`), `_ENTER`, `_LEAVE`) or a list's event
/// (`_TYPE_CHANGE`, `_COLOR_CHANGE` of the list `list`).
///
/// # Safety
/// `tab` comes from `ee_screens_setn_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_setn_call(
    tab: *const EeScreensSeTerrain,
    what: u32,
    list: *mut c_void,
    x: f32,
    y: f32,
) {
    if tab.is_null() {
        return;
    }
    let t = unsafe { &(*tab).0 };
    match what {
        EE_SCREENS_SETN_UPDATE => t.update(&mut Host),
        EE_SCREENS_SETN_MOUSE_MOVE => t.mouse_move(&mut Host, [x, y]),
        EE_SCREENS_SETN_ENTER => t.enter(&mut Host),
        EE_SCREENS_SETN_LEAVE => t.leave(&mut Host),
        EE_SCREENS_SETN_TYPE_CHANGE => t.type_change(&mut Host, list as usize),
        EE_SCREENS_SETN_COLOR_CHANGE => t.colour_change(&mut Host, list as usize),
        _ => {}
    }
}

/// The paint area `paint`'s `CreatePaintAction` -> the new `EESEAPaint` (NULL: none).
///
/// # Safety
/// `tab` comes from `ee_screens_setn_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_setn_paint(tab: *const EeScreensSeTerrain, paint: *mut c_void) -> *mut c_void {
    if tab.is_null() {
        return core::ptr::null_mut();
    }
    unsafe { &(*tab).0 }.create_paint_action(&mut Host, paint as usize) as *mut c_void
}

/// A paint type radio's `Check` on `form` (after the base's): its id's controls shown or hidden.
///
/// # Safety
/// `form` is the editor form.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_setn_check(form: *mut c_void, id: u32) {
    paint_type_check(&mut Host, form as usize, id);
}

/// The gate's reads (`EE_SCREENS_SETN_GET_*`, `k` an index).
///
/// # Safety
/// `tab` comes from `ee_screens_setn_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_setn_get(tab: *const EeScreensSeTerrain, what: u32, k: u32) -> i32 {
    if tab.is_null() {
        return 0;
    }
    unsafe { &(*tab).0 }.get(what, k)
}

/// `EEUIFSEBrushGroup`'s constructor's controls on `form` at (`x`, `y`) from the control index `*index` (advanced),
/// the radios over the C++ `group`: the title into `out[0]`, the tiny .. huge radios into `out[1..6]`.
///
/// # Safety
/// `index` points at the form's next control index, `out` at six handles.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sebr_build(
    group: *mut c_void,
    form: *mut c_void,
    x: f32,
    y: f32,
    index: *mut u32,
    out: *mut usize,
) {
    let mut next = unsafe { *index };
    let c = crate::screens::editor_brush::build(&mut Host, group as usize, form as usize, [x, y], &mut next);
    unsafe {
        *index = next;
        core::ptr::copy_nonoverlapping(c.as_ptr(), out, c.len());
    }
}

/// `EEUIFSEBrushGroup::SetHighlight`'s square (left, top, right, bottom into `out`) around (`x`, `y`) for the brush
/// `size`, `centred` or not.
///
/// # Safety
/// `out` points at four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sebr_highlight(x: f32, y: f32, size: u32, centred: bool, out: *mut f32) {
    let r = crate::screens::editor_brush::highlight(x, y, size, centred);
    unsafe { core::ptr::copy_nonoverlapping(r.as_ptr(), out, 4) };
}
