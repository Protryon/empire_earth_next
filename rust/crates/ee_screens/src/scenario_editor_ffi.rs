//! The scenario editor's events' host over the C++ calls (`scenario_editor.rs`; `ee_screenscpp_se`,
//! `ee_screenscpp_se_text`, `ee_screenscpp_se_set_text`, `Scenario Editor/EESEEvents.cpp`).

use core::ffi::c_long;

use crate::menus_ffi::Form;
use crate::scenario_editor::SeHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_se(form: *mut c_void, entity: *mut c_void, op: u32, a: u32, b: u32) -> u32;
        pub fn ee_screenscpp_se_text(
            form: *mut c_void,
            entity: *mut c_void,
            which: u32,
            arg: u32,
            out: *mut u8,
            capacity: usize,
        ) -> usize;
        pub fn ee_screenscpp_se_set_text(entity: *mut c_void, text: *const u8, length: usize);
    }
}

struct Editor<'a>(&'a mut Form);

impl SeHost for Editor<'_> {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        unsafe { sys::ee_screenscpp_se(self.0 .0, self.0 .1, op, a, b) }
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        let mut out = vec![0u8; 256];
        let n = unsafe { sys::ee_screenscpp_se_text(self.0 .0, self.0 .1, which, arg, out.as_mut_ptr(), out.len()) };
        if n > out.len() {
            out.resize(n, 0);
            let m =
                unsafe { sys::ee_screenscpp_se_text(self.0 .0, self.0 .1, which, arg, out.as_mut_ptr(), out.len()) };
            out.truncate(m.min(n));
        } else {
            out.truncate(n);
        }
        out
    }
    fn set_edit_text(&mut self, text: &[u8]) {
        unsafe { sys::ee_screenscpp_se_set_text(self.0 .1, text.as_ptr(), text.len()) }
    }
}

/// One of the editor's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::scenario_editor::dispatch(&mut Editor(host), event)
}
