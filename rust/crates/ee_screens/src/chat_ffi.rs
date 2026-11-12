//! The chat forms' host over the C++ calls (`chat.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::c_long;

use crate::chat::ChatHost;
use crate::menus::EE_SCREENS_TEXT_STRING;
use crate::menus_ffi::Form;

/// One of the chat forms' events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::chat::dispatch(host, event)
}

impl ChatHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        self.op(op, a, b, c, 0)
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        self.get(which, arg)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        self.put(which, arg, text);
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_STRING, id)
    }
}
