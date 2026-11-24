//! `Utilities.cpp` in Rust (LLE lane round 7): the engine's shutdown list (`URegisterShutdown`, `UShutdown`,
//! `UShutdownInProgress`) and the system questions (`UGetOSType`, `UGetOSLanguage`), over the platform layer
//! (`ee_plat_core` message.rs).
//!
//! `UShutdown` calls the registered functions last first, each popped before it runs (one may register another, as
//! the C++ vector allowed). With `terminate`, the process ends through the C runtime's `exit(0)` (forced, running
//! the `atexit` handlers) or `abort()`, as the original: not Rust's `process::exit`/`abort`.

use std::ffi::{c_long, c_ulong};
#[cfg(not(test))]
use std::sync::atomic::{AtomicBool, Ordering};
#[cfg(not(test))]
use std::sync::Mutex;

/// A shutdown function (`void (*)(bool inForced)`).
pub type EeUtilShutdownFn = unsafe extern "C" fn(forced: bool);

#[cfg(not(test))]
static SHUTDOWN: Mutex<Vec<EeUtilShutdownFn>> = Mutex::new(Vec::new());
#[cfg(not(test))]
static IN_PROGRESS: AtomicBool = AtomicBool::new(false);

// This crate's tests: a list and a flag per thread. The tests of the other systems register their shutdowns (the
// Input System's, the Time Manager's, the Thread System's) from their own test threads; with one list, the test of
// the list here ran them all, among them `ee_input_shutdown` while the Input System's tests were using its devices
// (PORT_LEARNINGS "The ee_base heap corruption").
#[cfg(test)]
thread_local! {
    static SHUTDOWN: std::cell::RefCell<Vec<EeUtilShutdownFn>> = const { std::cell::RefCell::new(Vec::new()) };
    static IN_PROGRESS: std::cell::Cell<bool> = const { std::cell::Cell::new(false) };
}

#[cfg(not(test))]
extern "C" {
    fn exit(code: std::ffi::c_int) -> !;
    fn abort() -> !;
}

/// Runs `body` over the list (never while a registered function runs: one may register another).
fn with_list<R>(body: impl FnOnce(&mut Vec<EeUtilShutdownFn>) -> R) -> R {
    #[cfg(not(test))]
    return body(&mut SHUTDOWN.lock().unwrap_or_else(|e| e.into_inner()));
    #[cfg(test)]
    SHUTDOWN.with_borrow_mut(body)
}

fn pop() -> Option<EeUtilShutdownFn> {
    with_list(Vec::pop)
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
    with_list(|list| list.push(function));
    0
}

/// `UShutdown`: the registered functions, last first; with `terminate`, the platform's shutdown and the end of the
/// process (`exit(0)` when forced, else `abort()`).
#[no_mangle]
pub extern "C" fn ee_util_shutdown(forced: bool, terminate: bool) {
    #[cfg(not(test))]
    IN_PROGRESS.store(true, Ordering::SeqCst);
    #[cfg(test)]
    IN_PROGRESS.set(true);
    while let Some(function) = pop() {
        // SAFETY: a function the engine registered for this call
        unsafe { function(forced) };
    }
    if !terminate {
        return;
    }
    crate::plat_core::message::PlatSystemShutdown();
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
    #[cfg(not(test))]
    return IN_PROGRESS.load(Ordering::SeqCst);
    #[cfg(test)]
    IN_PROGRESS.get()
}

/// `UGetOSType`.
#[no_mangle]
pub extern "C" fn ee_util_os_type() -> c_ulong {
    os_type(crate::plat_core::message::PlatSystemVersion()) as c_ulong
}

/// `UGetOSLanguage`.
#[no_mangle]
pub extern "C" fn ee_util_os_language() -> u16 {
    crate::plat_core::message::PlatSystemLanguageId()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU32, Ordering};

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

    /// A shutdown run by one test must not run what another test's thread registered: that freed the Input System's
    /// devices under its tests (the list is per thread in this crate's tests).
    #[test]
    fn another_threads_shutdown_does_not_run_this_threads_functions() {
        static RAN: AtomicU32 = AtomicU32::new(0);
        unsafe extern "C" fn mine(_: bool) {
            RAN.fetch_add(1, Ordering::SeqCst);
        }
        ee_util_register_shutdown(mine);
        std::thread::spawn(|| ee_util_shutdown(false, false)).join().unwrap();
        assert_eq!(RAN.load(Ordering::SeqCst), 0);
        assert!(!ee_util_shutdown_in_progress());
        ee_util_shutdown(false, false);
        assert_eq!(RAN.load(Ordering::SeqCst), 1);
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
