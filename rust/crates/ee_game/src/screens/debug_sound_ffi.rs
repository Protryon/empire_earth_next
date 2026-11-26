//! The debug sound panel's host over the C++ calls (`debug_sound.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::c_long;

use crate::screens::debug_sound::DebugSoundHost;
use crate::screens::menus_ffi::Form;

/// One of the panel's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::screens::debug_sound::dispatch(host, event)
}

impl DebugSoundHost for Form {
    fn call(&mut self, op: u32, a: u32) -> u32 {
        self.op(op, a, 0, 0, 0)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        self.put(which, arg, text);
    }
}
