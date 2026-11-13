//! The scenario editor's events' host over the C++ calls (`scenario_editor.rs`; `ee_screenscpp_se`,
//! `Scenario Editor/EESEEvents.cpp`).

use core::ffi::c_long;

use crate::menus_ffi::Form;
use crate::scenario_editor::SeHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_se(form: *mut c_void, entity: *mut c_void, op: u32, a: u32, b: u32) -> u32;
    }
}

struct Editor<'a>(&'a mut Form);

impl SeHost for Editor<'_> {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        unsafe { sys::ee_screenscpp_se(self.0 .0, self.0 .1, op, a, b) }
    }
}

/// One of the editor's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::scenario_editor::dispatch(&mut Editor(host), event)
}
