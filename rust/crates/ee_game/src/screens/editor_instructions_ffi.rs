//! `extern "C"` for the scenario editor's instructions tab whole in Rust (`editor_instructions.rs`): the object
//! `EESEInstructions` holds, made, called and freed by it. The shared control ops (below 100) are
//! `ee_screenscpp_sepl` (`Scenario Editor/EESERBUnitTypes.cpp`), the tab's own `ee_screenscpp_sein`
//! (`Scenario Editor/EESEInstructions.cpp`).

use core::ffi::c_void;

use crate::screens::editor_instructions::InstructionsTab;
use crate::screens::editor_players::SeplHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_sein(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            if op >= 700 {
                sys::ee_screenscpp_sein(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            } else {
                crate::screens::sgvw::editor(op, i, f, |i, f| {
                    sys::ee_screenscpp_sepl(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
                })
            }
        }
    }
}

/// The instructions tab (opaque to C++).
pub struct EeScreensSeInstructions(InstructionsTab);

/// `EESEInstructions`' constructor: its controls made on `form` from the control index `*index` (advanced). Freed by
/// `ee_screens_sein_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sein_new(form: *mut c_void, index: *mut u32) -> *mut EeScreensSeInstructions {
    let mut next = unsafe { *index };
    let t = InstructionsTab::new(&mut Host, form as usize, &mut next);
    unsafe { *index = next };
    Box::into_raw(Box::new(EeScreensSeInstructions(t)))
}

/// The destructor: every control deleted (the original's order), the object freed.
///
/// # Safety
/// `tab` comes from `ee_screens_sein_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sein_free(tab: *mut EeScreensSeInstructions) {
    if tab.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(tab) };
    b.0.destroy(&mut Host);
}

/// One of the tab's members (`EE_SCREENS_SEIN_LOAD`, `_ENTER`, `_STORE`).
///
/// # Safety
/// `tab` comes from `ee_screens_sein_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sein_call(tab: *const EeScreensSeInstructions, what: u32) {
    if !tab.is_null() {
        unsafe { &(*tab).0 }.call(&mut Host, what);
    }
}
