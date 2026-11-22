//! The statements' host for `editor_seax.rs`: `ee_screenscpp_seax` (`Scenario Editor Actions/EESEARust.cpp`).

use core::ffi::c_void;

use crate::screens::editor_seax::{SeaxArgs, SeaxHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_seax(
            op: u32,
            p: *mut c_void,
            q: *mut c_void,
            t: *mut f32,
            i: *mut i32,
            f: *mut f32,
        ) -> isize;
    }
}

/// The C++ statements.
pub(crate) struct Seax;

impl SeaxHost for Seax {
    fn x(&mut self, op: u32, a: &mut SeaxArgs) -> isize {
        unsafe {
            sys::ee_screenscpp_seax(
                op,
                a.p as *mut c_void,
                a.q as *mut c_void,
                a.t.as_mut_ptr(),
                a.i.as_mut_ptr(),
                a.f.as_mut_ptr(),
            )
        }
    }
}
