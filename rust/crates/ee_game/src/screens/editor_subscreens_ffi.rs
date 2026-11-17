//! `extern "C"` for the scenario editor's condition and effect screens whole in Rust (`editor_subscreens.rs`): the
//! objects `EESETSCondition` and `EESETSEffect` hold, made, called and freed by them. The hosts are the shared
//! maker's (`ee_screenscpp_sepl`) and the types' (`ee_screenscpp_sect`, `ee_screenscpp_seet`).

use core::ffi::c_void;

use crate::screens::editor_effects::EffectTypes;
use crate::screens::editor_players::SeplHost;
use crate::screens::editor_subscreens::{ConditionScreen, EffectScreen};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_sect(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_seet(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

macro_rules! host {
    ($name:ident, $f:ident) => {
        struct $name;

        impl SeplHost for $name {
            fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
                let p = p.map(|x| x as *mut c_void);
                unsafe { sys::$f(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr()) }
            }
        }
    };
}

// (the control maker's world ops traced: SGVW)
struct Sepl;

impl SeplHost for Sepl {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            crate::screens::editor_world::host(op, i, f, |i, f| {
                sys::ee_screenscpp_sepl(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            })
        }
    }
}

host!(Sect, ee_screenscpp_sect);
host!(Seet, ee_screenscpp_seet);

/// The condition screen (opaque to C++).
pub struct EeScreensSeConditionScreen(ConditionScreen);

/// The effect screen (opaque to C++).
pub struct EeScreensSeEffectScreen(EffectScreen);

/// The effect types' handle their events carry (`ee_screens_seet_event`'s `types`).
fn me(s: &EffectScreen) -> usize {
    &s.types as *const EffectTypes as usize
}

/// `EESETSCondition`'s constructor on the form: its controls from the control index `*index` (advanced; the screen
/// font is `gSeplScreenFont`). Freed by `ee_screens_secd_free`.
///
/// # Safety
/// `form` is the editor form; `index` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_secd_new(form: *mut c_void, index: *mut u32) -> *mut EeScreensSeConditionScreen {
    let index = unsafe { &mut *index };
    let s = ConditionScreen::new(&mut Sepl, &mut Sect, form as usize, index);
    Box::into_raw(Box::new(EeScreensSeConditionScreen(s)))
}

/// The destructor: the controls deleted, the types' event released, the object freed.
///
/// # Safety
/// `screen` comes from `ee_screens_secd_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_secd_free(screen: *mut EeScreensSeConditionScreen) {
    if screen.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(screen) };
    b.0.destroy(&mut Sepl, &mut Sect);
}

/// The slot `what` (`EE_SCREENS_SESC_*`) with the trigger tab's selection `selection`.
///
/// # Safety
/// `screen` comes from `ee_screens_secd_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_secd_call(screen: *const EeScreensSeConditionScreen, what: u32, selection: i32) {
    if screen.is_null() {
        return;
    }
    // shared: a host call can come back into the screen (the type combo box's event)
    let s = unsafe { &(*screen).0 };
    s.call(&mut Sepl, &mut Sect, what, selection);
}

/// `GetNames`: the scenario's user conditions' names into the `std::vector<UWideString>` `names`.
///
/// # Safety
/// `screen` comes from `ee_screens_secd_new` (or is NULL); `names` is a vector.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_secd_names(screen: *const EeScreensSeConditionScreen, names: *mut c_void) {
    if screen.is_null() {
        return;
    }
    let s = unsafe { &(*screen).0 };
    s.names(&mut Sect, names as usize);
}

/// The screen's controls (0: `EESETSConditionControls`), type combo box (1) or title (2).
///
/// # Safety
/// `screen` comes from `ee_screens_secd_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_secd_handle(screen: *const EeScreensSeConditionScreen, which: u32) -> *mut c_void {
    if screen.is_null() {
        return core::ptr::null_mut();
    }
    let s = unsafe { &(*screen).0 };
    (match which {
        0 => s.controls,
        1 => s.type_combo,
        _ => s.title,
    }) as *mut c_void
}

/// `EESETSEffect`'s constructor on the form: its controls from the control index `*index` (advanced; the screen
/// font is `gSeplScreenFont`). Freed by `ee_screens_sefx_free`.
///
/// # Safety
/// `form` is the editor form; `index` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefx_new(form: *mut c_void, index: *mut u32) -> *mut EeScreensSeEffectScreen {
    let index = unsafe { &mut *index };
    let s = EffectScreen::new(&mut Sepl, &mut Seet, form as usize, index);
    Box::into_raw(Box::new(EeScreensSeEffectScreen(s)))
}

/// The destructor: the controls and the title deleted, the types' events released, the object freed.
///
/// # Safety
/// `screen` comes from `ee_screens_sefx_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefx_free(screen: *mut EeScreensSeEffectScreen) {
    if screen.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(screen) };
    b.0.destroy(&mut Sepl, &mut Seet, me(&b.0));
}

/// The slot `what` (`EE_SCREENS_SESC_*`) with the trigger tab's selection `selection`.
///
/// # Safety
/// `screen` comes from `ee_screens_sefx_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefx_call(screen: *const EeScreensSeEffectScreen, what: u32, selection: i32) {
    if screen.is_null() {
        return;
    }
    // shared: a host call can come back into the screen (the type and action combo boxes' events)
    let s = unsafe { &(*screen).0 };
    s.call(&mut Sepl, &mut Seet, me(s), what, selection);
}

/// `GetNames`: the scenario's user effects' names into the `std::vector<UWideString>` `names`.
///
/// # Safety
/// `screen` comes from `ee_screens_sefx_new` (or is NULL); `names` is a vector.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefx_names(screen: *const EeScreensSeEffectScreen, names: *mut c_void) {
    if screen.is_null() {
        return;
    }
    let s = unsafe { &(*screen).0 };
    s.names(&mut Seet, names as usize);
}

/// The screen's controls (0: `EESETSEffectControls`), type combo box (1) or title (2).
///
/// # Safety
/// `screen` comes from `ee_screens_sefx_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefx_handle(screen: *const EeScreensSeEffectScreen, which: u32) -> *mut c_void {
    if screen.is_null() {
        return core::ptr::null_mut();
    }
    let s = unsafe { &(*screen).0 };
    (match which {
        0 => s.controls,
        1 => s.type_combo,
        _ => s.title,
    }) as *mut c_void
}
