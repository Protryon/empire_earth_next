//! `extern "C"` for the scenario editor's map tab whole in Rust (`editor_map.rs`): the object `EEUIFSEMap` holds,
//! made, called and freed by it, and the kind radio buttons' `Check`. The shared control ops (below 100) are
//! `ee_screenscpp_sepl` (`Scenario Editor/EESERBUnitTypes.cpp`), the tab's own `ee_screenscpp_semp`
//! (`Scenario Editor/EEUIFSEMap.cpp`).

use core::ffi::c_void;

use crate::editor_map::{
    kind_check, MapTab, EE_SCREENS_SEMP_CREATE_MAP, EE_SCREENS_SEMP_ENTER, EE_SCREENS_SEMP_SHOW_ELEVATION,
};
use crate::editor_players::SeplHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_semp(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            if op >= 500 {
                sys::ee_screenscpp_semp(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            } else {
                sys::ee_screenscpp_sepl(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            }
        }
    }
}

/// The map tab (opaque to C++).
pub struct EeScreensSeMap(MapTab);

/// `EEUIFSEMap`'s constructor: every control made on `form` from the control index `*index` (advanced); `shell` is
/// the C++ `EEUIFSEMap` (the create button's and the elevation file event's). Freed by `ee_screens_semp_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_semp_new(
    shell: *mut c_void,
    form: *mut c_void,
    index: *mut u32,
) -> *mut EeScreensSeMap {
    let mut next = unsafe { *index };
    let t = MapTab::new(&mut Host, shell as usize, form as usize, &mut next);
    unsafe { *index = next };
    Box::into_raw(Box::new(EeScreensSeMap(t)))
}

/// The destructor: every control deleted (the original's order), the object freed.
///
/// # Safety
/// `tab` comes from `ee_screens_semp_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_semp_free(tab: *mut EeScreensSeMap) {
    if tab.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(tab) };
    b.0.destroy(&mut Host);
}

/// One of the tab's members (`EE_SCREENS_SEMP_ENTER` ...) with its argument `a`.
///
/// # Safety
/// `tab` comes from `ee_screens_semp_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_semp_call(tab: *const EeScreensSeMap, what: u32, a: i32) {
    if tab.is_null() {
        return;
    }
    let t = unsafe { &(*tab).0 };
    let h = &mut Host;
    match what {
        EE_SCREENS_SEMP_ENTER => t.enter(h),
        EE_SCREENS_SEMP_CREATE_MAP => t.create_map(h),
        EE_SCREENS_SEMP_SHOW_ELEVATION => t.show_elevation(h, a != 0),
        _ => {}
    }
}

/// A kind radio button's `Check` after the radio button's own (`id`: 0x6e blank, 0x6f random, 0x6d seeded) or its
/// `HideControls` (`id` 0), over the controls of `form` by id.
///
/// # Safety
/// `form` is the editor form.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_semp_kind(form: *mut c_void, id: u32) {
    kind_check(&mut Host, form as usize, id);
}
