//! C string helpers shared by the implementations.

use std::alloc::{alloc, Layout};
use std::ffi::c_char;

#[cfg(any(windows, test))]
/// `strncpy` of a string without its terminator: at most `dst.len()` bytes copied, the rest of `dst` zero-filled. The
/// result is terminated only when `src` is shorter than `dst`, as `strncpy`.
pub fn strncpy(dst: &mut [u8], src: &[u8]) {
    let n = src.len().min(dst.len());
    dst[..n].copy_from_slice(&src[..n]);
    dst[n..].fill(0);
}

/// The bytes of a NUL-terminated string (without the terminator).
///
/// # Safety
/// `text` must be a valid NUL-terminated string that outlives the result.
pub unsafe fn cstr_bytes<'a>(text: *const c_char) -> &'a [u8] {
    std::ffi::CStr::from_ptr(text).to_bytes()
}

/// A `Box::new` that returns NULL when out of memory instead of aborting (the C++ `new (std::nothrow)`). The result
/// is freed with `Box::from_raw`.
pub fn try_box<T>(value: T) -> *mut T {
    let layout = Layout::new::<T>();
    assert!(layout.size() != 0, "try_box is for records and handles");
    // SAFETY: the layout has a non-zero size.
    let p = unsafe { alloc(layout) }.cast::<T>();
    if !p.is_null() {
        // SAFETY: p is valid for writes of T and unaliased.
        unsafe { p.write(value) };
    }
    p
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn strncpy_copies_and_pads() {
        let mut d = [0xAAu8; 8];
        strncpy(&mut d[..5], b"abc");
        assert_eq!(&d, b"abc\0\0\xAA\xAA\xAA");
    }

    #[test]
    fn strncpy_truncates_without_terminator() {
        let mut d = [0xAAu8; 4];
        strncpy(&mut d, b"abcdef");
        assert_eq!(&d, b"abcd");
        let mut empty = [1u8; 2];
        strncpy(&mut empty, b"");
        assert_eq!(empty, [0, 0]);
    }

    #[test]
    fn cstr_bytes_stops_at_terminator() {
        let text = b"hi\0there\0";
        assert_eq!(unsafe { cstr_bytes(text.as_ptr().cast()) }, b"hi");
    }

    #[test]
    fn try_box_holds_the_value() {
        let p = try_box(0x1234_5678u64);
        assert!(!p.is_null());
        let b = unsafe { Box::from_raw(p) };
        assert_eq!(*b, 0x1234_5678);
    }
}
