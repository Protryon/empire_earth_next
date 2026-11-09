//! POSIX: the per-thread last error behind `PlatGetLastError`. The values are the Win32 error codes the engine
//! already records and a few callers compare against, so a trace reads the same on every system; an errno without a
//! Win32 twin is `0x20000000 | errno` (the Win32 "customer" bit: never a system code).
//!
//! `ee_plat_posix_set_last_error` and `..._errno` are the link-time seam `ee_plat_core` calls, so the last error of
//! PlatPath, PlatTime and the like lives here too.

use std::cell::Cell;
use std::ffi::c_int;

pub const ERROR_FILE_NOT_FOUND: u32 = 2;
pub const ERROR_PATH_NOT_FOUND: u32 = 3;
pub const ERROR_TOO_MANY_OPEN_FILES: u32 = 4;
pub const ERROR_ACCESS_DENIED: u32 = 5;
pub const ERROR_INVALID_HANDLE: u32 = 6;
pub const ERROR_NOT_ENOUGH_MEMORY: u32 = 8;
pub const ERROR_NOT_SAME_DEVICE: u32 = 17;
pub const ERROR_NO_MORE_FILES: u32 = 18;
pub const ERROR_WRITE_PROTECT: u32 = 19;
pub const ERROR_NOT_SUPPORTED: u32 = 50;
pub const ERROR_FILE_EXISTS: u32 = 80;
pub const ERROR_INVALID_PARAMETER: u32 = 87;
pub const ERROR_DISK_FULL: u32 = 112;
pub const ERROR_NEGATIVE_SEEK: u32 = 131;
pub const ERROR_DIR_NOT_EMPTY: u32 = 145;
pub const ERROR_BUSY: u32 = 170;
pub const ERROR_ALREADY_EXISTS: u32 = 183;
pub const ERROR_FILENAME_EXCED_RANGE: u32 = 206;
pub const ERROR_FILE_INVALID: u32 = 1006;
pub const ERROR_TIMEOUT: u32 = 1460;

thread_local! {
    static LAST_ERROR: Cell<u32> = const { Cell::new(0) };
}

pub fn set(error: u32) {
    LAST_ERROR.with(|e| e.set(error));
}

pub fn get() -> u32 {
    LAST_ERROR.with(Cell::get)
}

/// The Win32 code for an errno value.
pub fn from_errno(errno: i32) -> u32 {
    #[rustfmt::skip]
    const TABLE: &[(i32, u32)] = &[
        (0, 0),
        (libc::ENOENT, ERROR_FILE_NOT_FOUND),
        (libc::ENOTDIR, ERROR_PATH_NOT_FOUND),
        (libc::EMFILE, ERROR_TOO_MANY_OPEN_FILES),
        (libc::ENFILE, ERROR_TOO_MANY_OPEN_FILES),
        (libc::EACCES, ERROR_ACCESS_DENIED),
        (libc::EPERM, ERROR_ACCESS_DENIED),
        (libc::EISDIR, ERROR_ACCESS_DENIED),
        (libc::EBADF, ERROR_INVALID_HANDLE),
        (libc::ENOMEM, ERROR_NOT_ENOUGH_MEMORY),
        (libc::EROFS, ERROR_WRITE_PROTECT),
        (libc::EXDEV, ERROR_NOT_SAME_DEVICE),
        (libc::EEXIST, ERROR_ALREADY_EXISTS),
        (libc::EINVAL, ERROR_INVALID_PARAMETER),
        (libc::ENOSPC, ERROR_DISK_FULL),
        (libc::ENOTEMPTY, ERROR_DIR_NOT_EMPTY),
        (libc::EBUSY, ERROR_BUSY),
        (libc::ENAMETOOLONG, ERROR_FILENAME_EXCED_RANGE),
        (libc::ETIMEDOUT, ERROR_TIMEOUT),
        (libc::ENOSYS, ERROR_NOT_SUPPORTED),
        (libc::ENOTSUP, ERROR_NOT_SUPPORTED),
    ];
    TABLE.iter().find(|(e, _)| *e == errno).map_or(0x2000_0000 | errno as u32, |(_, w)| *w)
}

/// errno after a failed call.
pub fn errno() -> i32 {
    std::io::Error::last_os_error().raw_os_error().unwrap_or(0)
}

/// `set(from_errno(errno()))` -> 0, so a failing function can `return fail_errno();`.
pub fn fail_errno() -> c_int {
    set(from_errno(errno()));
    0
}

/// `set(error)` -> 0.
pub fn fail(error: u32) -> c_int {
    set(error);
    0
}

/// For `ee_plat_core`, which cannot depend on this crate: sets the calling thread's last error (POSIX; Windows has
/// `SetLastError`).
#[cfg(unix)]
#[no_mangle]
pub extern "C" fn ee_plat_posix_set_last_error(error: u32) {
    set(error);
}

/// For `ee_plat_core`: sets the calling thread's last error from an errno value.
#[cfg(unix)]
#[no_mangle]
pub extern "C" fn ee_plat_posix_set_last_error_errno(errno: c_int) {
    set(from_errno(errno));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mapping_matches_the_cpp_table() {
        assert_eq!(from_errno(0), 0);
        assert_eq!(from_errno(libc::ENOENT), 2);
        assert_eq!(from_errno(libc::ENOTDIR), 3);
        assert_eq!(from_errno(libc::EMFILE), 4);
        assert_eq!(from_errno(libc::EACCES), 5);
        assert_eq!(from_errno(libc::EPERM), 5);
        assert_eq!(from_errno(libc::EISDIR), 5);
        assert_eq!(from_errno(libc::EBADF), 6);
        assert_eq!(from_errno(libc::ENOMEM), 8);
        assert_eq!(from_errno(libc::EEXIST), 183);
        assert_eq!(from_errno(libc::EINVAL), 87);
        assert_eq!(from_errno(libc::ENOTEMPTY), 145);
        assert_eq!(from_errno(libc::ENAMETOOLONG), 206);
        assert_eq!(from_errno(libc::EAGAIN), 0x2000_0000 | libc::EAGAIN as u32);
        // no twin
    }

    #[test]
    fn last_error_is_per_thread() {
        set(5);
        std::thread::spawn(|| {
            assert_eq!(get(), 0);
            set(7);
        })
        .join()
        .unwrap();
        assert_eq!(get(), 5);
    }

    #[test]
    fn the_seam_sets_the_error_the_getter_returns() {
        ee_plat_posix_set_last_error(183);
        assert_eq!(get(), 183);
        ee_plat_posix_set_last_error_errno(libc::ENOENT);
        assert_eq!(get(), 2);
    }
}
