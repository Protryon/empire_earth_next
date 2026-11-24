//! The Rust objects a C++ reference holds (R6 WON round 34): `HTTPConnection` and `HTTPSession` (WONRouting.h) were
//! C++ classes whose whole state was a Rust object ([`EeWonHttpConn`], [`EeWonHttpSession`], httpengine.rs) and a
//! lock; they are one C++ `WonRustRef` now, the `RefCount` the ops' completions and the session's map hold, its kind
//! saying which Rust object it owns. This makes and frees the kind's object for the reference.

use std::ffi::c_void;
use std::ptr::null_mut;

use crate::cppcore::{EeWonCoreHooks, EeWonCoreObject};
use crate::httpcpp::{ee_won_http_conn_new_core, ee_won_http_session_new_core};
use crate::httpengine::{ee_won_http_conn_free, ee_won_http_session_free, EeWonHttpConn, EeWonHttpSession};

/// The kind's Rust object for the C++ reference `obj` (its calls over the core table): an `HTTPConnection`'s
/// connection (idle, pipelining on) or an `HTTPSession`'s map of connections; null for another kind.
///
/// # Safety
/// `core` is the library's static table; `obj` outlives the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_rustref_new(
    kind: EeWonCoreObject,
    obj: *mut c_void,
    core: *const EeWonCoreHooks,
) -> *mut c_void {
    match kind {
        // SAFETY: the contract
        EeWonCoreObject::HttpConn => unsafe { ee_won_http_conn_new_core(obj, core) }.cast(),
        EeWonCoreObject::HttpSession => unsafe { ee_won_http_session_new_core(obj, core) }.cast(),
        _ => null_mut(),
    }
}

/// The reference's destructor: the connection's socket, queues and completions, or the session's map.
///
/// # Safety
/// `p` is null or the object `ee_won_rustref_new` made for `kind`, not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_rustref_free(kind: EeWonCoreObject, p: *mut c_void) {
    if p.is_null() {
        return;
    }
    match kind {
        // SAFETY: the contract
        EeWonCoreObject::HttpConn => unsafe { ee_won_http_conn_free(p.cast::<EeWonHttpConn>()) },
        EeWonCoreObject::HttpSession => unsafe { ee_won_http_session_free(p.cast::<EeWonHttpSession>()) },
        _ => {}
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn another_kind_has_no_object() {
        // SAFETY: no object is made or freed for these kinds
        unsafe {
            assert!(ee_won_rustref_new(EeWonCoreObject::Lock, null_mut(), std::ptr::null()).is_null());
            ee_won_rustref_free(EeWonCoreObject::Op, null_mut());
            ee_won_rustref_free(EeWonCoreObject::HttpConn, null_mut());
        }
    }
}
