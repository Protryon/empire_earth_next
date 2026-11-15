//! `Utilities.cpp` in Rust (LLE lane round 7): the engine's shutdown list (`URegisterShutdown`, `UShutdown`,
//! `UShutdownInProgress`) and the system questions (`UGetOSType`, `UGetOSLanguage`), over the platform layer
//! (`ee_plat_core` message.rs).
//!
//! `UShutdown` calls the registered functions last first, each popped before it runs (one may register another, as
//! the C++ vector allowed). With `terminate`, the process ends through the C runtime's `exit(0)` (forced, running
//! the `atexit` handlers) or `abort()`, as the original: not Rust's `process::exit`/`abort`.

use std::ffi::{c_long, c_ulong};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;

/// A shutdown function (`void (*)(bool inForced)`).
pub type EeUtilShutdownFn = unsafe extern "C" fn(forced: bool);

static SHUTDOWN: Mutex<Vec<EeUtilShutdownFn>> = Mutex::new(Vec::new());
static IN_PROGRESS: AtomicBool = AtomicBool::new(false);

#[cfg(not(test))]
extern "C" {
    fn exit(code: std::ffi::c_int) -> !;
    fn abort() -> !;
}

fn pop() -> Option<EeUtilShutdownFn> {
    SHUTDOWN.lock().unwrap_or_else(|e| e.into_inner()).pop()
}

/// `UGetOSType` from a `GetVersion` value: 1 = 95, 2 = 98/ME, 3 = NT 5 or later, 4 = NT 4 or older (or a 9x major
/// below 4).
pub fn os_type(version: u32) -> u32 {
    let major = version & 0xff;
    let minor = (version >> 8) & 0xff;
    if version < 0x8000_0000 {
        if major < 5 {
            4
        } else {
            3
        }
    } else if major < 4 {
        4
    } else if major > 4 || (major == 4 && minor >= 10) {
        2
    } else {
        1
    }
}

/// `URegisterShutdown`: always 0 (`kErrorOK`).
#[no_mangle]
pub extern "C" fn ee_util_register_shutdown(function: EeUtilShutdownFn) -> c_long {
    SHUTDOWN.lock().unwrap_or_else(|e| e.into_inner()).push(function);
    0
}

/// `UShutdown`: the registered functions, last first; with `terminate`, the platform's shutdown and the end of the
/// process (`exit(0)` when forced, else `abort()`).
#[no_mangle]
pub extern "C" fn ee_util_shutdown(forced: bool, terminate: bool) {
    IN_PROGRESS.store(true, Ordering::SeqCst);
    while let Some(function) = pop() {
        // SAFETY: a function the engine registered for this call
        unsafe { function(forced) };
    }
    if !terminate {
        return;
    }
    ee_plat_core::message::PlatSystemShutdown();
    #[cfg(not(test))]
    // SAFETY: the C runtime's process exits
    unsafe {
        if forced {
            exit(0)
        } else {
            abort()
        }
    }
}

/// `UShutdownInProgress`.
#[no_mangle]
pub extern "C" fn ee_util_shutdown_in_progress() -> bool {
    IN_PROGRESS.load(Ordering::SeqCst)
}

/// `UGetOSType`.
#[no_mangle]
pub extern "C" fn ee_util_os_type() -> c_ulong {
    os_type(ee_plat_core::message::PlatSystemVersion()) as c_ulong
}

/// `UGetOSLanguage`.
#[no_mangle]
pub extern "C" fn ee_util_os_language() -> u16 {
    ee_plat_core::message::PlatSystemLanguageId()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::AtomicU32;

    static CALLS: AtomicU32 = AtomicU32::new(0);

    unsafe extern "C" fn first(forced: bool) {
        assert!(forced);
        CALLS.fetch_add(1, Ordering::SeqCst);
        assert_eq!(CALLS.load(Ordering::SeqCst), 3, "the first registered runs last");
    }
    unsafe extern "C" fn second(_: bool) {
        CALLS.fetch_add(1, Ordering::SeqCst);
        // registered during the shutdown: runs next
        ee_util_register_shutdown(third);
    }
    unsafe extern "C" fn third(_: bool) {
        CALLS.fetch_add(1, Ordering::SeqCst);
        assert_eq!(CALLS.load(Ordering::SeqCst), 2);
    }

    #[test]
    fn shutdown_runs_last_first() {
        assert!(!ee_util_shutdown_in_progress());
        assert_eq!(ee_util_register_shutdown(first), 0);
        ee_util_register_shutdown(second);
        ee_util_shutdown(true, false);
        assert_eq!(CALLS.load(Ordering::SeqCst), 3);
        assert!(ee_util_shutdown_in_progress());
    }

    #[test]
    fn os_types() {
        assert_eq!(os_type(0x0a00_000a), 3); // NT 10.0
        assert_eq!(os_type(0x0000_0004), 4); // NT 4
        assert_eq!(os_type(0x8000_0004), 1); // 95
        assert_eq!(os_type(0x8000_0a04), 2); // 98
        assert_eq!(os_type(0x8000_5a04), 2); // ME
        assert_eq!(os_type(0x8000_0003), 4);
        assert_eq!(os_type(0x8000_0005), 2);
    }
}
