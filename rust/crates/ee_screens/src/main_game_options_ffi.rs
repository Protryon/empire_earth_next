//! The in-game options menu's host over the C++ calls (`main_game_options.rs`; `UI Events/EEScreens.cpp`) and its
//! state.

use core::ffi::c_long;
use std::sync::Mutex;

use crate::main_game_options::*;
use crate::menus::EE_SCREENS_TEXT_STRING;
use crate::menus_ffi::Form;

/// The Play Scenario event's posted flag (the UI thread's; locked only around its own reads and writes).
static MAIN_GAME_OPTIONS: Mutex<MainGameOptions> = Mutex::new(MainGameOptions::new());

/// One of the menu's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::main_game_options::dispatch(host, &MAIN_GAME_OPTIONS, event)
}

impl OptionsHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        self.op(op, a, b, c, 0)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        self.put(which, arg, text);
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_STRING, id)
    }
}
