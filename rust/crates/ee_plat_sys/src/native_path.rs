//! POSIX: the game's paths on the native file system, through the C API of PlatPath (`include/platform/PlatPath.h`),
//! which another crate (or `src/Platform/Posix/PlatPath.cpp`) implements. Isolated here so that the extern
//! declarations have one substitute for `cargo test`, which links no PlatPath.

use std::ffi::{c_char, CStr, CString};

// PlatPath's native paths are bounded by the game's PLAT_MAX_PATH plus a native root; this is far above both.
const NATIVE_CAPACITY: usize = 16 * 1024;

#[cfg(not(test))]
extern "C" {
    fn PlatPathToNative(path: *const c_char, native: *mut c_char, size: u32) -> u32;
    fn PlatPathFromNative(native: *const c_char, path: *mut c_char, size: u32) -> u32;
}

#[cfg(test)]
use stand_in::{PlatPathFromNative, PlatPathToNative};

/// `PlatPosix::ResolvePath`: the native path of a game path -> `None` (with the last error set) when the path is
/// empty, NULL or too long.
///
/// # Safety
/// `path` must be NULL or a valid NUL-terminated string.
pub unsafe fn to_native(path: *const c_char) -> Option<CString> {
    let mut buffer = vec![0u8; NATIVE_CAPACITY];
    let n = PlatPathToNative(path, buffer.as_mut_ptr().cast(), NATIVE_CAPACITY as u32);
    if n == 0 {
        return None;
    }
    buffer.truncate(n as usize);
    CString::new(buffer).ok()
}

/// `PlatPosix::GameForm`: the game form of a native absolute path -> `None` (with the last error set) when no volume
/// contains it.
pub fn to_game_form(native: &CStr) -> Option<Vec<u8>> {
    let mut buffer = vec![0u8; NATIVE_CAPACITY];
    // SAFETY: `native` is NUL-terminated, the buffer holds NATIVE_CAPACITY bytes.
    let n = unsafe { PlatPathFromNative(native.as_ptr(), buffer.as_mut_ptr().cast(), NATIVE_CAPACITY as u32) };
    if n == 0 {
        return None;
    }
    buffer.truncate(n as usize);
    Some(buffer)
}

/// A stand-in for PlatPath in `cargo test`: volumes map to the root, separators become '/', no case lookup (that is
/// PlatPath's job and its own tests'), the tail as written.
#[cfg(test)]
mod stand_in {
    use std::ffi::{c_char, CStr};

    pub unsafe fn PlatPathToNative(path: *const c_char, native: *mut c_char, size: u32) -> u32 {
        if path.is_null() || *path == 0 {
            crate::last_error::set(crate::last_error::ERROR_PATH_NOT_FOUND);
            return 0;
        }
        let mut text = CStr::from_ptr(path).to_bytes().to_vec();
        if text.len() >= 2 && text[0].is_ascii_alphabetic() && text[1] == b':' {
            text.drain(..2);
        }
        let mut out: Vec<u8> = text.iter().map(|&c| if c == b'\\' { b'/' } else { c }).collect();
        while out.len() > 1 && out.last() == Some(&b'/') {
            out.pop();
        }
        if out.first() != Some(&b'/') {
            let mut full = std::env::current_dir().expect("cwd").into_os_string().into_encoded_bytes();
            full.push(b'/');
            full.extend_from_slice(&out);
            out = full;
        }
        if out.len() + 1 > size as usize {
            return 0;
        }
        std::ptr::copy_nonoverlapping(out.as_ptr().cast(), native, out.len());
        *native.add(out.len()) = 0;
        out.len() as u32
    }

    pub unsafe fn PlatPathFromNative(native: *const c_char, path: *mut c_char, size: u32) -> u32 {
        let bytes = CStr::from_ptr(native).to_bytes();
        let mut out = b"C:".to_vec();
        out.extend(bytes.iter().map(|&c| if c == b'/' { b'\\' } else { c }));
        if out.len() + 1 > size as usize {
            return 0;
        }
        std::ptr::copy_nonoverlapping(out.as_ptr().cast(), path, out.len());
        *path.add(out.len()) = 0;
        out.len() as u32
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn stubs_map_a_game_path_both_ways() {
        let path = CString::new("C:\\tmp\\x\\").unwrap();
        let native = unsafe { to_native(path.as_ptr()) }.unwrap();
        assert_eq!(native.as_bytes(), b"/tmp/x");
        let game = to_game_form(&native).unwrap();
        assert_eq!(game, b"C:\\tmp\\x");
    }

    #[test]
    fn empty_path_fails_with_the_error_set() {
        let empty = CString::new("").unwrap();
        assert!(unsafe { to_native(empty.as_ptr()) }.is_none());
        assert_eq!(crate::last_error::get(), crate::last_error::ERROR_PATH_NOT_FOUND);
    }
}
