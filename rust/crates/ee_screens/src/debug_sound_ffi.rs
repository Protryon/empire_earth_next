//! The debug sound panel's host over the C++ calls (`debug_sound.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::{c_long, c_ulong};

use crate::debug_sound::DebugSoundHost;
use crate::menus_ffi::Form;

/// One of the panel's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::debug_sound::dispatch(host, event)
}

impl DebugSoundHost for Form {
    fn call(&mut self, op: u32, a: u32) -> u32 {
        self.op(op, a, 0, 0, 0)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        self.put(which, arg, text);
    }
}

/// The debug sound panel's event code for a control event id, 0 for another form's.
#[no_mangle]
pub extern "C" fn ee_screens_debug_sound_event_for(id: c_ulong) -> u32 {
    crate::debug_sound::event_for_control(id)
}
