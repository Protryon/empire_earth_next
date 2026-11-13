//! The loading screen's host over the C++ calls (`splash.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::c_long;

use crate::menus_ffi::Form;
use crate::splash::SplashHost;

/// The loading screen's event and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::splash::dispatch(host, event)
}

impl SplashHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        self.op(op, a, b, c, d)
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        self.get(which, arg)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        self.put(which, arg, text);
    }
}
