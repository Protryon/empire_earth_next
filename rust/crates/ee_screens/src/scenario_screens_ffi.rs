//! The campaign scenario select and scenario intro forms' host over the C++ calls (`scenario_screens.rs`;
//! `UI Events/EEScreens.cpp`), their state and the intro's control-event table.
// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the host's `u32` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_ulong, c_void};
use std::sync::Mutex;

use crate::menus::EE_SCREENS_TEXT_STRING;
use crate::menus_ffi::{sys, Form};
use crate::scenario_screens::*;

extern "C" {
    /// The manager's form of an id (`gUIManager->GetFormByID`).
    pub(crate) fn ee_screenscpp_form(id: u32) -> *mut c_void;
}

/// The intro's briefing sound (the UI thread's; locked only around its own reads and writes).
static SCENARIO_SCREENS: Mutex<ScenarioScreens> = Mutex::new(ScenarioScreens::new());

/// One of the two forms' events; false for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> bool {
    is_scenario_event(event) && crate::scenario_screens::dispatch(host, &SCENARIO_SCREENS, event)
}

impl Form {
    /// The host on form `form` (the same entity).
    fn on(&self, form: u32) -> Form {
        Form(unsafe { ee_screenscpp_form(form) }, self.1)
    }
}

impl ScenarioHost for Form {
    fn call(&mut self, form: u32, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        self.on(form).op(op, a, b, c, d)
    }
    fn text(&mut self, form: u32, which: u32, arg: u32) -> Vec<u8> {
        self.on(form).get(which, arg)
    }
    fn put_text(&mut self, form: u32, which: u32, arg: u32, text: &[u8]) -> u32 {
        self.on(form).put(which, arg, text)
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_STRING, id)
    }
    fn combo_selection(&mut self, form: u32, control: u32) -> u32 {
        let mut out = 0u32;
        unsafe { sys::ee_screenscpp_selection(self.on(form).0, control, false, &mut out) };
        out
    }
}

/// The scenario intro form's event code for a control event id; 0 for another form's.
#[no_mangle]
pub extern "C" fn ee_screens_scenario_intro_event_for(id: c_ulong) -> u32 {
    scenario_intro_event_for(id as u32)
}
