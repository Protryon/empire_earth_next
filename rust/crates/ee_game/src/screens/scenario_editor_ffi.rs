//! The scenario editor's events' host over the C++ calls (`scenario_editor.rs`; `ee_screenscpp_se`,
//! `Scenario Editor/EESEEvents.cpp`).

use core::ffi::c_long;

use crate::screens::menus_ffi::Form;
use crate::screens::scenario_editor::SeHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_se(form: *mut c_void, entity: *mut c_void, op: u32, a: u32, b: u32) -> u32;
    }
}

struct Editor<'a>(&'a mut Form);

impl SeHost for Editor<'_> {
    // (the toolkit's ops and the texts answered in Rust: editor_direct.rs)
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        if let Some(r) = crate::screens::editor_direct::se(self.0 .0, self.0 .1, op, a, b) {
            return r;
        }
        unsafe { sys::ee_screenscpp_se(self.0 .0, self.0 .1, op, a, b) }
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        crate::screens::editor_direct::se_text(self.0 .1, which, arg)
    }
    fn set_edit_text(&mut self, text: &[u8]) {
        crate::screens::editor_direct::se_set_text(self.0 .1, text)
    }
}

/// One of the editor's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::screens::scenario_editor::dispatch(&mut Editor(host), event)
}
