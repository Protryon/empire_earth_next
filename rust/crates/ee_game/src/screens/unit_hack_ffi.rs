//! `extern "C"` for the main game form's unit hack (`unit_hack.rs`). The host is Rust's (`unit_hack_calls.rs`, over
//! `User Interface Thread/EEUIFMainGame.cpp`'s shims): `form` is the `EEUIFMainGame`.

use core::ffi::c_void;

use crate::screens::unit_hack::{
    UnitHack, UnitHackHost, EE_SCREENS_UH_FILL_COMBO_BOX, EE_SCREENS_UH_LOAD_UNIT_DATA, EE_SCREENS_UH_PLACE_UNIT,
    EE_SCREENS_UH_UPDATE,
};

struct Host(*mut c_void);

impl UnitHackHost for Host {
    fn op(&mut self, op: u32, a: i32, b: i32, f: &mut [f32; 2], out: &mut u32, text: &mut [u8]) -> i32 {
        crate::screens::unit_hack_calls::call(&mut crate::screens::game_cpp::Ffi, self.0 as usize, op, a, b, f, out, text)
            .unwrap_or(0)
    }
}

/// The unit hack's record (opaque to C++).
pub struct EeScreensUnitHack(UnitHack);

/// The hack made for the form (the class list read through the host). Freed by `ee_screens_unit_hack_free`.
#[no_mangle]
pub extern "C" fn ee_screens_unit_hack_new(form: *mut c_void) -> *mut EeScreensUnitHack {
    Box::into_raw(Box::new(EeScreensUnitHack(UnitHack::new(&mut Host(form)))))
}

/// # Safety
/// `hack` comes from `ee_screens_unit_hack_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_unit_hack_free(hack: *mut EeScreensUnitHack) {
    if !hack.is_null() {
        drop(unsafe { Box::from_raw(hack) });
    }
}

/// One of the hack's methods (`EE_SCREENS_UH_*`); `x`, `y` the point of the place and update calls. The record is
/// taken out of its box around the call: no Rust reference into the box lives across the host's calls.
///
/// # Safety
/// `hack` comes from `ee_screens_unit_hack_new`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_unit_hack_call(
    hack: *mut EeScreensUnitHack,
    form: *mut c_void,
    call: u32,
    x: f32,
    y: f32,
) {
    let record = unsafe { core::mem::take(&mut (*hack).0) };
    let host = &mut Host(form);
    match call {
        EE_SCREENS_UH_FILL_COMBO_BOX => record.fill_combo_box(host),
        EE_SCREENS_UH_LOAD_UNIT_DATA => record.load_unit_data(host),
        EE_SCREENS_UH_PLACE_UNIT => record.place_unit(host, [x, y]),
        EE_SCREENS_UH_UPDATE => record.update(host, [x, y]),
        _ => {}
    }
    unsafe { (*hack).0 = record };
}
