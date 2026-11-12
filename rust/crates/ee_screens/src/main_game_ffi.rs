//! The main game form's events' host over the C++ calls (`main_game.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::c_long;

use crate::main_game::MainGameHost;
use crate::menus::EE_SCREENS_TEXT_STRING;
use crate::menus_ffi::Form;

/// One of the main game form's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::main_game::dispatch(host, event)
}

impl MainGameHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        self.op(op, a, b, 0, 0)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        self.put(which, arg, text);
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_STRING, id)
    }
}
