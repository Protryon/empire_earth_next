//! The load and save screens' host over the C++ calls (`save_load.rs`; `UI Events/EEScreens.cpp`), their state and
//! their control-event tables.
// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the host's `u32` stay
#![allow(clippy::unnecessary_cast)]

use std::sync::Mutex;

use crate::screens::menus::EE_SCREENS_TEXT_STRING;
use crate::screens::menus_ffi::Form;
use crate::screens::save_load::*;

/// The screens' state (the UI thread's; locked only around its own reads and writes).
static SAVE_LOAD: Mutex<SaveLoad> = Mutex::new(SaveLoad { saved: false });

/// One of the load and save screens' events; false for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> bool {
    is_save_load_event(event) && crate::screens::save_load::dispatch(host, &SAVE_LOAD, event)
}

impl SaveLoadHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        self.op(op, a, b, c, 0)
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        self.get(which, arg)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        self.put(which, arg, text)
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_STRING, id)
    }
}
