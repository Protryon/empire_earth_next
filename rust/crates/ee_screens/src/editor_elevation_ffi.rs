//! `extern "C"` for the scenario editor's elevation tab whole in Rust (`editor_elevation.rs`): the object
//! `EEUIFSEElevation` holds, made, called and freed by it. The shared control ops (below 100) are
//! `ee_screenscpp_sepl` (`Scenario Editor/EESERBUnitTypes.cpp`), the tab's own `ee_screenscpp_seel`
//! (`Scenario Editor/EEUIFSEElevation.cpp`).

use core::ffi::c_void;

use crate::editor_elevation::{ElevationTab, EE_SCREENS_SEEL_MOUSE_MOVE, EE_SCREENS_SEEL_UPDATE};
use crate::editor_players::SeplHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_seel(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            if op >= 900 {
                sys::ee_screenscpp_seel(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            } else {
                sys::ee_screenscpp_sepl(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            }
        }
    }
}

/// The elevation tab (opaque to C++).
pub struct EeScreensSeElevation(ElevationTab);

/// `EEUIFSEElevation`'s constructor: every control made on `form` from the control index `*index` (advanced), the
/// paint area over the form's edge `scroll` and `brushes`. Freed by `ee_screens_seel_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seel_new(
    form: *mut c_void,
    scroll: *mut c_void,
    brushes: *mut c_void,
    index: *mut u32,
) -> *mut EeScreensSeElevation {
    let mut next = unsafe { *index };
    let t = ElevationTab::new(&mut Host, form as usize, scroll as usize, brushes as usize, &mut next);
    unsafe { *index = next };
    Box::into_raw(Box::new(EeScreensSeElevation(t)))
}

/// The destructor: every control deleted (the original's order), the object freed.
///
/// # Safety
/// `tab` comes from `ee_screens_seel_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seel_free(tab: *mut EeScreensSeElevation) {
    if tab.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(tab) };
    b.0.destroy(&mut Host);
}

/// `Update` (`EE_SCREENS_SEEL_UPDATE`) or `MouseMove` at (`x`, `y`).
///
/// # Safety
/// `tab` comes from `ee_screens_seel_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seel_call(tab: *const EeScreensSeElevation, what: u32, x: f32, y: f32) {
    if tab.is_null() {
        return;
    }
    let t = unsafe { &(*tab).0 };
    match what {
        EE_SCREENS_SEEL_UPDATE => t.update(&mut Host),
        EE_SCREENS_SEEL_MOUSE_MOVE => t.mouse_move(&mut Host, [x, y]),
        _ => {}
    }
}
