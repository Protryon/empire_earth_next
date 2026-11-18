//! The hot key events' host over the C++ calls (`hot_keys.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::c_long;

use crate::screens::hot_keys::{EeScreensHotKeyUnit, HotKeyHost};
use crate::screens::menus_ffi::Form;

/// cbindgen:ignore
mod sys {

    extern "C" {
        /// The aircraft's target: the map point under the game view's centre (or its tile's centre), x and y.
        pub fn ee_screenscpp_hot_key_target(out: *mut f32);
        /// The snapshot's complex units (`HK_CENTER`'s, by the snapshot's index).
        pub fn ee_screenscpp_hot_key_keep(units: *const u64, count: usize);
    }
}

/// One of the hot key events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::screens::hot_keys::dispatch(host, event)
}

impl HotKeyHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        self.op(op, a, b, c, 0)
    }
    fn units(&mut self, aircraft: bool) -> Vec<EeScreensHotKeyUnit> {
        let begun = crate::screens::sgvw::begin();
        let out = hot_key_units(aircraft);
        if let Some(g) = begun {
            crate::screens::sgvw::record(g, &format!("HK {} {out:?}", aircraft as u32));
        }
        out
    }
}

/// The current player's units as the hot keys see them (`hud_views::hot_key_units`), the aircraft's target from
/// the C++; the snapshot's complex units kept for `HK_CENTER`.
pub(crate) fn hot_key_units(aircraft: bool) -> Vec<EeScreensHotKeyUnit> {
    let mut target = [0.0f32; 2];
    if aircraft {
        // SAFETY: two floats
        unsafe { sys::ee_screenscpp_hot_key_target(target.as_mut_ptr()) };
    }
    let mut complexes = Vec::new();
    let out = crate::screens::hud_views::hot_key_units(aircraft, target, &mut complexes);
    // SAFETY: the handles' count
    unsafe { sys::ee_screenscpp_hot_key_keep(complexes.as_ptr(), complexes.len()) };
    out
}
