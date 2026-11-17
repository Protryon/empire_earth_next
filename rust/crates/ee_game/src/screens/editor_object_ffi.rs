//! `extern "C"` for the scenario editor's object screen whole in Rust (`editor_object.rs`): the object
//! `EESETSObject` holds, made, called and freed by it. The shared control ops (below 100) are
//! `ee_screenscpp_sepl` (`Scenario Editor/EESERBUnitTypes.cpp`), the screen's own `ee_screenscpp_seob`
//! (`Scenario Editor/EESETSObject.cpp`).

use core::ffi::c_void;

use crate::screens::editor_object::Object;
use crate::screens::editor_players::SeplHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_seob(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            if op >= 100 {
                sys::ee_screenscpp_seob(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            } else {
                sys::ee_screenscpp_sepl(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            }
        }
    }
}

/// The object screen (opaque to C++).
pub struct EeScreensSeObject(Object);

/// `Refresh`, `Show`, `Hide`, `CheckChanged(a: id, b: state)`, `ClearUnitList`, `Deactivate`, a list operation
/// (`a`: `EE_SCREENS_SEOB_SM_*`, `b`: the selection), `Activate`, `Store(a)`, `Load(a)`, `UpdateSelection(a)`,
/// `GetIsSelectOnMap` (answered), the mode set (`a`), the picked units' ids cleared.
pub const EE_SCREENS_SEOB_REFRESH: u32 = 1;
pub const EE_SCREENS_SEOB_SHOW: u32 = 2;
pub const EE_SCREENS_SEOB_HIDE: u32 = 3;
pub const EE_SCREENS_SEOB_CHECK_CHANGED: u32 = 4;
pub const EE_SCREENS_SEOB_CLEAR_UNIT_LIST: u32 = 5;
pub const EE_SCREENS_SEOB_DEACTIVATE: u32 = 6;
pub const EE_SCREENS_SEOB_LIST_OP: u32 = 7;
pub const EE_SCREENS_SEOB_ACTIVATE: u32 = 8;
pub const EE_SCREENS_SEOB_STORE: u32 = 9;
pub const EE_SCREENS_SEOB_LOAD: u32 = 10;
pub const EE_SCREENS_SEOB_UPDATE_SELECTION: u32 = 11;
pub const EE_SCREENS_SEOB_IS_SELECT_ON_MAP: u32 = 12;
pub const EE_SCREENS_SEOB_SET_SELECT_ON_MAP: u32 = 13;
pub const EE_SCREENS_SEOB_CLEAR_UNIT_IDS: u32 = 14;

/// `EESETSObject`'s constructor: every control made on `form` from the control index `*index` (advanced).
/// Freed by `ee_screens_seob_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seob_new(form: *mut c_void, index: *mut u32) -> *mut EeScreensSeObject {
    let mut next = unsafe { *index };
    let o = Object::new(&mut Host, form as usize, &mut next);
    unsafe { *index = next };
    Box::into_raw(Box::new(EeScreensSeObject(o)))
}

/// The destructor: every control deleted (the original's order), the object freed.
///
/// # Safety
/// `screen` comes from `ee_screens_seob_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seob_free(screen: *mut EeScreensSeObject) {
    if screen.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(screen) };
    b.0.destroy(&mut Host);
}

/// One of the screen's members (`EE_SCREENS_SEOB_REFRESH` ...) with its arguments; `GetIsSelectOnMap`'s answer.
///
/// # Safety
/// `screen` comes from `ee_screens_seob_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seob_call(screen: *const EeScreensSeObject, what: u32, a: u32, b: u32) -> u32 {
    if screen.is_null() {
        return 0;
    }
    // shared: a host call can come back into the screen (a mode radio's event refreshes it)
    let o = unsafe { &(*screen).0 };
    let h = &mut Host;
    match what {
        EE_SCREENS_SEOB_REFRESH => o.refresh(h),
        EE_SCREENS_SEOB_SHOW => o.show(h),
        EE_SCREENS_SEOB_HIDE => o.hide(h),
        EE_SCREENS_SEOB_CHECK_CHANGED => o.check_changed(h, a, b as i32),
        EE_SCREENS_SEOB_CLEAR_UNIT_LIST => o.clear_unit_list(h),
        EE_SCREENS_SEOB_DEACTIVATE => o.deactivate(h),
        EE_SCREENS_SEOB_LIST_OP => o.list_op(h, a as i32, b),
        EE_SCREENS_SEOB_ACTIVATE => o.activate(h),
        EE_SCREENS_SEOB_STORE => o.store(h, a),
        EE_SCREENS_SEOB_LOAD => o.load(h, a),
        EE_SCREENS_SEOB_UPDATE_SELECTION => o.update_selection(h, a),
        EE_SCREENS_SEOB_IS_SELECT_ON_MAP => return o.is_select_on_map(h) as u32,
        EE_SCREENS_SEOB_SET_SELECT_ON_MAP => o.select_on_map.set(a != 0),
        EE_SCREENS_SEOB_CLEAR_UNIT_IDS => o.unit_ids.borrow_mut().clear(),
        _ => {}
    }
    0
}

/// `MouseMove` at (x, y).
///
/// # Safety
/// `screen` comes from `ee_screens_seob_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seob_mouse_move(screen: *const EeScreensSeObject, x: f32, y: f32) {
    if !screen.is_null() {
        unsafe { &(*screen).0 }.mouse_move(&mut Host, x, y);
    }
}

/// `GetNames`: the user's objects' names into the C++ `std::vector<UWideString>` `out`.
///
/// # Safety
/// `screen` comes from `ee_screens_seob_new` (or is NULL); `out` is a vector.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seob_get_names(screen: *const EeScreensSeObject, out: *mut c_void) {
    if !screen.is_null() {
        unsafe { &(*screen).0 }.get_names(&mut Host, out as usize);
    }
}
