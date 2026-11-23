//! `extern "C"` for the scenario editor's trigger screen whole in Rust (`editor_trigger.rs`): the object
//! `EESETSTrigger` holds, made, called and freed by it. The shared control ops (below 100) are
//! `ee_screenscpp_sepl` (`Scenario Editor/EESERBUnitTypes.cpp`), the screen's own `ee_screenscpp_setr`
//! (`Scenario Editor/EESETSTrigger.cpp`).

use core::ffi::c_void;

use crate::screens::editor_players::SeplHost;
use crate::screens::editor_trigger::Trigger;
use crate::screens::editor_trigger::*;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        // (the file's statement host answers these ops too since round 6)
        #[link_name = "ee_screenscpp_setrx"]
        pub fn ee_screenscpp_setr(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            if op >= 200 {
                if let Some(r) = crate::screens::editor_direct::sets(op, p.map(|x| x as usize), i) {
                    return r;
                }
                if let Some(r) = crate::screens::editor_trigx_ffi::setr(op, p.map(|x| x as usize), i) {
                    return r;
                }
                sys::ee_screenscpp_setr(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            } else {
                crate::screens::editor_world::host(op, i, f, |i, f| {
                    crate::screens::editor_direct::sepl(op, p.as_ptr(), i, f)
                })
            }
        }
    }
}

/// The trigger screen (opaque to C++).
pub struct EeScreensSeTrigger(Trigger);

/// `EESETSTrigger`'s constructor: every control made on `form` from the control index `*index` (advanced).
/// Freed by `ee_screens_setr_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_setr_new(form: *mut c_void, index: *mut u32) -> *mut EeScreensSeTrigger {
    let mut next = unsafe { *index };
    let t = Trigger::new(&mut Host, form as usize, &mut next);
    unsafe { *index = next };
    Box::into_raw(Box::new(EeScreensSeTrigger(t)))
}

/// The destructor: every control deleted (the original's order), the object freed.
///
/// # Safety
/// `screen` comes from `ee_screens_setr_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_setr_free(screen: *mut EeScreensSeTrigger) {
    if screen.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(screen) };
    b.0.destroy(&mut Host);
}

/// One of the screen's members (`EE_SCREENS_SETR_LIST_OP` ...) on the trigger `selection` with its argument.
///
/// # Safety
/// `screen` comes from `ee_screens_setr_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_setr_call(screen: *const EeScreensSeTrigger, what: u32, selection: u32, a: u32) {
    if screen.is_null() {
        return;
    }
    // shared: a host call can come back into the screen (an operator combo's event)
    let t = unsafe { &(*screen).0 };
    let h = &mut Host;
    match what {
        EE_SCREENS_SETR_LIST_OP => t.list_op(h, a as i32, selection),
        EE_SCREENS_SETR_ACTIVATE => t.activate(h),
        EE_SCREENS_SETR_STORE => t.store(h, selection),
        EE_SCREENS_SETR_LOAD => t.load(h, selection),
        EE_SCREENS_SETR_HIDE => t.hide(h),
        EE_SCREENS_SETR_SHOW => t.show(h),
        EE_SCREENS_SETR_CHANGE_OPERATOR => t.change_operator(h, selection, a),
        EE_SCREENS_SETR_REMOVE_ENTRY => t.remove_entry(h, selection, a),
        EE_SCREENS_SETR_STEP => t.step(h, selection, a),
        _ => {}
    }
}

/// `GetNames`: the triggers' names into the C++ `std::vector<UWideString>` `out`.
///
/// # Safety
/// `screen` comes from `ee_screens_setr_new` (or is NULL); `out` is a vector.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_setr_get_names(screen: *const EeScreensSeTrigger, out: *mut c_void) {
    if !screen.is_null() {
        unsafe { &(*screen).0 }.get_names(&mut Host, out as usize);
    }
}
