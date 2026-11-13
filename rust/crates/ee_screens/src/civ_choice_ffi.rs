//! The civilization choice form's host over the C++ calls (`civ_choice.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::c_long;
use std::sync::Mutex;

use crate::civ_choice::{CivChoice, CivChoiceHost};
use crate::menus_ffi::Form;

static CIV_CHOICE: Mutex<CivChoice> = Mutex::new(CivChoice::new());

/// One of the form's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::civ_choice::dispatch(host, &CIV_CHOICE, event)
}

impl CivChoiceHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        self.op(op, a, b, c, d)
    }
    fn text(&mut self, which: u32, arg: u32) -> Option<Vec<u8>> {
        self.get_checked(which, arg)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        self.put(which, arg, text)
    }
}
