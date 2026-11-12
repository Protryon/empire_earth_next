//! The hot key events' host over the C++ calls (`hot_keys.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::c_long;

use crate::hot_keys::{EeScreensHotKeyUnit, HotKeyHost};
use crate::menus_ffi::Form;

/// cbindgen:ignore
mod sys {
    use crate::hot_keys::EeScreensHotKeyUnit;

    extern "C" {
        /// The current player's units (the aircraft's snapshot with the distances) into `out`, up to `capacity`;
        /// their count.
        pub fn ee_screenscpp_hot_key_units(aircraft: bool, out: *mut EeScreensHotKeyUnit, capacity: usize) -> usize;
    }
}

/// One of the hot key events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::hot_keys::dispatch(host, event)
}

impl HotKeyHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        self.op(op, a, b, c, 0)
    }
    fn units(&mut self, aircraft: bool) -> Vec<EeScreensHotKeyUnit> {
        let mut out = vec![EeScreensHotKeyUnit::default(); 1024];
        let mut n = unsafe { sys::ee_screenscpp_hot_key_units(aircraft, out.as_mut_ptr(), out.len()) };
        if n > out.len() {
            out.resize(n, EeScreensHotKeyUnit::default());
            n = unsafe { sys::ee_screenscpp_hot_key_units(aircraft, out.as_mut_ptr(), out.len()) }.min(n);
        }
        out.truncate(n);
        out
    }
}
