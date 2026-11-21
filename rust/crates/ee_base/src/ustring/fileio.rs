//! `UString::Load`/`Save` and `UWideString::Load`/`Save` (the inline bodies of `include/lle/FSFile.h`): a 32-bit
//! length and the text, through the C++ `FSFile`'s calls ([`CppFile`]; any [`FsFile`] in the tests).

use std::ffi::{c_long, c_ulong, c_void};
use std::slice;

use super::{
    ee_ustring_load_buffer, ee_ustring_load_end, ee_uwstring_bytes_to_save, ee_uwstring_clear, ee_uwstring_load_buffer,
    ee_uwstring_load_end, EeUString, EeUWideString,
};
use crate::fs::status::{FAILED, OK};
use crate::fs::{CppFile, FsFile, FsWrite};

/// The `length` bytes of a string's buffer (one zero byte for a missing buffer).
unsafe fn bytes<'a>(string: *const std::ffi::c_char, length: c_ulong) -> &'a [u8] {
    if string.is_null() {
        return &[0];
    }
    // SAFETY: the caller's: the buffer holds `length` bytes
    unsafe { slice::from_raw_parts(string.cast(), length as usize) }
}

/// `UString::Load(inFile)`: the stored length, then the text; on a failure the string is emptied.
///
/// # Safety
/// `s` is a live record.
pub unsafe fn ustring_load(s: *mut EeUString, file: &mut dyn FsFile) -> i32 {
    let mut stored: u32 = 0;
    let mut result = file.read_u32(&mut stored);
    let length = stored as c_ulong;
    if result == OK && length != 0 {
        // SAFETY: the buffer holds `length + 1` bytes
        let buffer = unsafe { slice::from_raw_parts_mut(ee_ustring_load_buffer(s, length).cast::<u8>(), length as usize) };
        let (status, read) = file.read_text(buffer);
        result = status;
        if result == OK && read as c_ulong != length {
            result = FAILED;
        }
    }
    // SAFETY: after the buffer of `length` when it was read
    unsafe { ee_ustring_load_end(s, result == OK && length != 0, length) };
    result
}

/// `UString::Save(inFile)`: the length with the terminator, then the text and its terminator.
///
/// # Safety
/// `s` is a live record.
pub unsafe fn ustring_save(s: *const EeUString, file: &mut dyn FsWrite) -> i32 {
    // SAFETY: the caller's contract
    let (string, length) = unsafe { ((*s).string, (*s).length.wrapping_add(1)) };
    let result = file.write_u32(length as u32);
    if result != OK {
        return result;
    }
    // SAFETY: the buffer holds the text and its terminator
    let (result, written) = file.write_text(unsafe { bytes(string, length) });
    if result == OK && (written as c_ulong) < length {
        return FAILED;
    }
    result
}

/// `UWideString::Load(inFile)`: the byte size, then the bytes; on a failure the string is cleared.
///
/// # Safety
/// `s` is a live record.
pub unsafe fn uwstring_load(s: *mut EeUWideString, file: &mut dyn FsFile) -> i32 {
    let mut stored: u32 = 0;
    let mut result = file.read_u32(&mut stored);
    let size = stored as c_ulong;
    if result == OK {
        // SAFETY: the buffer holds `size + 1` bytes
        let buffer = unsafe { slice::from_raw_parts_mut(ee_uwstring_load_buffer(s, size).cast::<u8>(), size as usize) };
        let (status, read) = file.read_text(buffer);
        result = status;
        if result == OK {
            // SAFETY: the buffer of `size`
            unsafe { ee_uwstring_load_end(s, size) };
            if read as c_ulong != size {
                result = FAILED;
            }
        }
    }
    if result != OK {
        // SAFETY: a live record
        unsafe { ee_uwstring_clear(s) };
    }
    result
}

/// `UWideString::Save(inFile)`: the byte size (without the terminator's), then the bytes.
///
/// # Safety
/// `s` is a live record.
pub unsafe fn uwstring_save(s: *const EeUWideString, file: &mut dyn FsWrite) -> i32 {
    // SAFETY: the caller's contract
    let (string, size) = unsafe { ((*s).string, ee_uwstring_bytes_to_save(s).wrapping_sub(4)) };
    let result = file.write_u32(size as u32);
    if result != OK {
        return result;
    }
    // SAFETY: the buffer holds the bytes to save
    let (result, written) = file.write_text(unsafe { bytes(string, size) });
    if result == OK && (written as c_ulong) < size {
        return FAILED;
    }
    result
}

/// `UString::Load(FSFile&)` on the C++ file `file`.
///
/// # Safety
/// `s` is a live record; `file` a live C++ `FSFile`.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_load_file(s: *mut EeUString, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe { ustring_load(s, &mut CppFile(file)) as c_long }
}

/// `UString::Save(FSFile&)` on the C++ file `file`.
///
/// # Safety
/// As [`ee_ustring_load_file`].
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_save_file(s: *const EeUString, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe { ustring_save(s, &mut CppFile(file)) as c_long }
}

/// `UWideString::Load(FSFile&)` on the C++ file `file`.
///
/// # Safety
/// `s` is a live record; `file` a live C++ `FSFile`.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_load_file(s: *mut EeUWideString, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe { uwstring_load(s, &mut CppFile(file)) as c_long }
}

/// `UWideString::Save(FSFile&)` on the C++ file `file`.
///
/// # Safety
/// As [`ee_uwstring_load_file`].
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_save_file(s: *const EeUWideString, file: *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe { uwstring_save(s, &mut CppFile(file)) as c_long }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fs::cppfile::tests::with_file;
    use crate::ustring::{ee_ustring_free, ee_ustring_init_text, ee_uwstring_free, ee_uwstring_init_text};
    use std::ffi::{c_char, CStr};
    use std::mem::MaybeUninit;

    fn ustring(text: &CStr) -> EeUString {
        let mut s = MaybeUninit::<EeUString>::uninit();
        // SAFETY: the record is initialised by the call
        unsafe {
            ee_ustring_init_text(s.as_mut_ptr(), text.as_ptr() as *const c_char);
            s.assume_init()
        }
    }
    fn uwstring(text: &CStr) -> EeUWideString {
        let mut s = MaybeUninit::<EeUWideString>::uninit();
        // SAFETY: as `ustring`
        unsafe {
            ee_uwstring_init_text(s.as_mut_ptr(), text.as_ptr() as *const c_char);
            s.assume_init()
        }
    }

    #[test]
    fn a_ustring_saves_and_loads_its_length_and_text() {
        let _g = crate::ustring::GOLDEN_LOCK.lock().unwrap_or_else(|e| e.into_inner());
        let mut s = ustring(c"abc");
        let (r, m) = with_file(&[], |f| unsafe { ee_ustring_save_file(&s, f.0) });
        assert_eq!(r, 0);
        assert_eq!(m.data, [4, 0, 0, 0, b'a', b'b', b'c', 0]);
        assert_eq!(m.log, ["write_u32 4", "write_text 4"]);
        let mut t = ustring(c"");
        let (r, m) = with_file(&m.data, |f| unsafe { ee_ustring_load_file(&mut t, f.0) });
        assert_eq!((r, t.length), (0, 3));
        assert_eq!(m.log, ["read_u32", "read_text 4"]);
        // a short text fails and empties the string
        let (r, _) = with_file(&[9, 0, 0, 0, b'x'], |f| unsafe { ee_ustring_load_file(&mut t, f.0) });
        assert_eq!((r, t.length), (FAILED as c_long, 0));
        // an empty file: the length's read fails
        let (r, m) = with_file(&[], |f| unsafe { ee_ustring_load_file(&mut t, f.0) });
        assert_eq!((r, t.length, m.log.len()), (0x12, 0, 1));
        unsafe {
            ee_ustring_free(&mut s);
            ee_ustring_free(&mut t);
        }
    }

    #[test]
    fn a_wide_string_saves_and_loads_its_bytes() {
        let _g = crate::ustring::GOLDEN_LOCK.lock().unwrap_or_else(|e| e.into_inner());
        let mut s = uwstring(c"hello");
        let (r, m) = with_file(&[], |f| unsafe { ee_uwstring_save_file(&s, f.0) });
        assert_eq!(r, 0);
        assert_eq!(&m.data[..4], &(m.data.len() as u32 - 4).to_le_bytes());
        assert_eq!(m.log.len(), 2);
        let mut t = uwstring(c"");
        let (r, _) = with_file(&m.data, |f| unsafe { ee_uwstring_load_file(&mut t, f.0) });
        assert_eq!((r, t.length), (0, 5));
        let (r, _) = with_file(&[9, 0, 0, 0, b'x'], |f| unsafe { ee_uwstring_load_file(&mut t, f.0) });
        assert_eq!((r, t.length), (FAILED as c_long, 0));
        unsafe {
            ee_uwstring_free(&mut s);
            ee_uwstring_free(&mut t);
        }
    }
}
