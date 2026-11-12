//! The load and save screens' host over the C++ calls (`save_load.rs`; `UI Events/EEScreens.cpp`), their state and
//! their control-event tables.
// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the host's `u32` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::c_ulong;
use std::sync::Mutex;

use crate::menus::EE_SCREENS_TEXT_STRING;
use crate::menus_ffi::Form;
use crate::save_load::*;

/// The screens' state (the UI thread's; locked only around its own reads and writes).
static SAVE_LOAD: Mutex<SaveLoad> = Mutex::new(SaveLoad { saved: false });

/// One of the load and save screens' events; false for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> bool {
    is_save_load_event(event) && crate::save_load::dispatch(host, &SAVE_LOAD, event)
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

/// The save/load form's event code for a control event id (the screen active and the editor on when the form is
/// made); 0 for another form's (and for the load form's Cancel, C++'s `EEUIECancel`).
#[no_mangle]
pub extern "C" fn ee_screens_save_load_event_for(id: c_ulong, screen_active: bool, in_editor: bool) -> u32 {
    save_load_event_for(id as u32, screen_active, in_editor)
}

/// The single player load form's event code for a control event id; 0 for another form's.
#[no_mangle]
pub extern "C" fn ee_screens_sp_load_event_for(id: c_ulong) -> u32 {
    sp_load_event_for(id as u32)
}

/// The scenario load form's event code for a control event id; 0 for another form's.
#[no_mangle]
pub extern "C" fn ee_screens_scenario_load_event_for(id: c_ulong) -> u32 {
    scenario_load_event_for(id as u32)
}
