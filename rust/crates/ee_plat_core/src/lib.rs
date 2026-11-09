//! The platform layer's leaves in Rust (docs/RUST_PLAN.md R1.1, R1.2). Each `#[no_mangle] extern "C"` item here is a
//! function or variable `include/platform/Plat*.h` declares: those headers stay the contract, and the C++ that
//! implemented them (`src/Platform/{Common,Win32,Posix}`) is gone. Text and paths are the game's bytes (RUST_PLAN.md
//! 2.3), never `String`.
//!
//! The rest of the platform layer is still elsewhere and this crate calls it by name: `PlatEventSet` (POSIX timers)
//! and, on POSIX, the thread's last error (`seam`).

#![allow(non_snake_case, non_upper_case_globals, clippy::missing_safety_doc)]

pub mod config;
pub mod path;
pub mod sim;
pub mod string;
pub mod time;

/// Copies between C strings and the game's byte strings.
pub(crate) mod c_text {
    use std::ffi::{c_char, CStr};

    /// The bytes of a C string, without its terminator; empty for NULL.
    pub(crate) unsafe fn bytes<'a>(text: *const c_char) -> &'a [u8] {
        if text.is_null() {
            &[]
        } else {
            CStr::from_ptr(text).to_bytes()
        }
    }

    /// `text` and a terminator, cut at its first NUL (a C string cannot go further).
    pub(crate) fn terminated(text: &[u8]) -> Vec<u8> {
        let end = text.iter().position(|&b| b == 0).unwrap_or(text.len());
        let mut out = text[..end].to_vec();
        out.push(0);
        out
    }
}

/// The calls this crate makes into the part of the platform layer that is not Rust yet (POSIX only; Windows has
/// `SetLastError`). On a POSIX link `src/Platform/Posix/PlatPosixBridge.cpp` provides the two last-error functions.
#[cfg(unix)]
pub(crate) mod seam {
    use std::ffi::{c_int, c_void};

    extern "C" {
        /// Sets the calling thread's last error (`PlatGetLastError`) to a Win32 error code.
        pub fn ee_plat_posix_set_last_error(error: u32);
        /// Sets the last error from an errno value (`PlatPosix::ErrorFromErrno`).
        pub fn ee_plat_posix_set_last_error_errno(error: c_int);
        pub fn PlatEventSet(event: *mut c_void) -> c_int;
    }
}

/// Stand-ins for `seam` in this crate's tests, which link no C++.
#[cfg(all(test, unix))]
pub(crate) mod test_seam {
    use std::ffi::{c_int, c_void};
    use std::sync::atomic::{AtomicU32, Ordering};

    pub static LAST_ERROR: AtomicU32 = AtomicU32::new(0);
    pub static EVENTS_SET: AtomicU32 = AtomicU32::new(0);

    #[no_mangle]
    pub extern "C" fn ee_plat_posix_set_last_error(error: u32) {
        LAST_ERROR.store(error, Ordering::SeqCst);
    }
    #[no_mangle]
    pub extern "C" fn ee_plat_posix_set_last_error_errno(error: c_int) {
        LAST_ERROR.store(0x2000_0000 | error as u32, Ordering::SeqCst);
    }
    #[no_mangle]
    pub extern "C" fn PlatEventSet(_event: *mut c_void) -> c_int {
        EVENTS_SET.fetch_add(1, Ordering::SeqCst);
        1
    }
}

/// Serialises the tests that touch process-wide state (environment, current directory, the multibyte code page).
#[cfg(test)]
pub(crate) static TEST_GLOBALS: std::sync::Mutex<()> = std::sync::Mutex::new(());

/// Takes `TEST_GLOBALS`, also after another test failed while holding it.
#[cfg(test)]
pub(crate) fn test_globals() -> std::sync::MutexGuard<'static, ()> {
    TEST_GLOBALS.lock().unwrap_or_else(std::sync::PoisonError::into_inner)
}

/// A process environment change for a test (the C runtime's copy too, which `getenv` reads).
#[cfg(test)]
pub(crate) fn test_set_env(name: &str, value: &str) {
    use std::ffi::CString;
    let (n, v) = (CString::new(name).unwrap(), CString::new(value).unwrap());
    #[cfg(windows)]
    unsafe {
        extern "C" {
            fn _putenv_s(name: *const std::ffi::c_char, value: *const std::ffi::c_char) -> i32;
        }
        assert_eq!(_putenv_s(n.as_ptr(), v.as_ptr()), 0);
    }
    #[cfg(unix)]
    unsafe {
        assert_eq!(libc::setenv(n.as_ptr(), v.as_ptr(), 1), 0);
    }
}

/// A fresh directory for a test's files, as bytes ending in a separator.
#[cfg(test)]
pub(crate) fn test_dir(name: &str) -> std::path::PathBuf {
    let dir = std::env::temp_dir().join(format!("ee_plat_core-{}-{}", std::process::id(), name));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    dir
}
