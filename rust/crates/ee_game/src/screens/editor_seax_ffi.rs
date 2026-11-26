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

/// An op of the C++'s merged hosts in `ee_screenscpp_seax` (`EE_SCREENS_SEAX_SEA` + an `EE_SCREENS_SEA_*` op, `_SEW` +
/// `EE_SCREENS_SEW_OP_*`, `_SEP` + `EE_SCREENS_SEP_*`, `_SEU` + `EE_SCREENS_SEU_*`).
///
/// # Safety
/// The operands are what the op reads and writes.
pub(crate) unsafe fn host(op: u32, p: *mut c_void, q: *mut c_void, t: *mut f32, i: *mut i32, f: *mut f32) -> isize {
    if let Some(r) = crate::screens::editor_tech::seax(op, p as usize) {
        return r;
    }
    unsafe { sys::ee_screenscpp_seax(op, p, q, t, i, f) }
}

/// The C++ statements.
pub(crate) struct Seax;

impl SeaxHost for Seax {
    fn x(&mut self, op: u32, a: &mut SeaxArgs) -> isize {
        if let Some(r) = crate::screens::editor_tech::seax(op, a.p as usize) {
            return r;
        }
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
