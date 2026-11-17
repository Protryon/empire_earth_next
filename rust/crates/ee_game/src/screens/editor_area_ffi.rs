//! `extern "C"` for the trigger tab's area sub-screen whole in Rust (`editor_area.rs`): the object `EESETSArea`
//! holds, made, called and freed by it. The shared control ops (below 100) are `ee_screenscpp_sepl`
//! (`Scenario Editor/EESERBUnitTypes.cpp`), the screen's own `ee_screenscpp_sear` (`Scenario Editor/EESETSArea.cpp`).

use core::ffi::c_void;

use crate::screens::editor_area::AreaScreen;
use crate::screens::editor_players::SeplHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_sear(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            if op >= 600 {
                sys::ee_screenscpp_sear(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            } else {
                crate::screens::editor_world::host(op, i, f, |i, f| {
                    sys::ee_screenscpp_sepl(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
                })
            }
        }
    }
}

/// The area sub-screen (opaque to C++).
pub struct EeScreensSeArea(AreaScreen);

/// `EESETSArea`'s constructor: its controls made on `form` from the control index `*index` (advanced). Freed by
/// `ee_screens_sear_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sear_new(form: *mut c_void, index: *mut u32) -> *mut EeScreensSeArea {
    let mut next = unsafe { *index };
    let s = AreaScreen::new(&mut Host, form as usize, &mut next);
    unsafe { *index = next };
    Box::into_raw(Box::new(EeScreensSeArea(s)))
}

/// The destructor: the controls deleted, a paint action released, the object freed.
///
/// # Safety
/// `screen` comes from `ee_screens_sear_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sear_free(screen: *mut EeScreensSeArea) {
    if screen.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(screen) };
    b.0.destroy(&mut Host);
}

/// The slot `what` (`EE_SCREENS_SEAR_SORT` ...) with the sub-screen's selection `sel` and the argument `a`.
///
/// # Safety
/// `screen` comes from `ee_screens_sear_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sear_call(screen: *const EeScreensSeArea, what: u32, sel: i32, a: i32) {
    if !screen.is_null() {
        unsafe { &(*screen).0 }.call(&mut Host, what, sel, a);
    }
}

/// `MouseMove` at (`x`, `y`).
///
/// # Safety
/// `screen` comes from `ee_screens_sear_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sear_mouse_move(screen: *const EeScreensSeArea, x: f32, y: f32) {
    if !screen.is_null() {
        unsafe { &(*screen).0 }.mouse_move(&mut Host, x, y);
    }
}

/// `GetCenter`: whether there is one; it into `out[0..2]`.
///
/// # Safety
/// `screen` comes from `ee_screens_sear_new` (or is NULL); `out` holds two floats.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sear_center(screen: *const EeScreensSeArea, out: *mut f32) -> bool {
    if screen.is_null() {
        return false;
    }
    match unsafe { &(*screen).0 }.center(&mut Host) {
        Some(c) => {
            unsafe { core::ptr::copy_nonoverlapping(c.as_ptr(), out, 2) };
            true
        }
        None => false,
    }
}

/// The state the editor gate reads: the painted rectangle into `rect[0..4]`, the anchor point into `point[0..2]`
/// -> the flags (1 rectangle mode, 2 a rectangle, 4 a point, 8 a rectangle being painted).
///
/// # Safety
/// `screen` comes from `ee_screens_sear_new` (or is NULL); `rect` holds four floats, `point` two.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sear_state(screen: *const EeScreensSeArea, rect: *mut f32, point: *mut f32) -> u32 {
    if screen.is_null() {
        return 0;
    }
    let s = unsafe { &(*screen).0 };
    unsafe {
        core::ptr::copy_nonoverlapping(s.rectangle.get().as_ptr(), rect, 4);
        core::ptr::copy_nonoverlapping(s.point.get().as_ptr(), point, 2);
    }
    s.flags()
}

/// `GetNames`: the user areas' names into the `std::vector<UWideString>` `names`.
///
/// # Safety
/// `screen` comes from `ee_screens_sear_new` (or is NULL); `names` is a `std::vector<UWideString>`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sear_names(screen: *const EeScreensSeArea, names: *mut c_void) {
    if !screen.is_null() {
        unsafe { &(*screen).0 }.names(&mut Host, names as usize);
    }
}
