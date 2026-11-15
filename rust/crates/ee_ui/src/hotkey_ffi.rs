//! `extern "C"` for the hot keys' name (`User Interface/UIHotKey.cpp` `GetHotKeyName`); the hot key is whole in Rust
//! (`hotkeyobj.rs`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_char;

use crate::hotkey;

unsafe fn c_bytes<'a>(s: *const c_char) -> Option<&'a [u8]> {
    if s.is_null() {
        None
    } else {
        Some(core::ffi::CStr::from_ptr(s).to_bytes())
    }
}

/// `UIHotKey::GetHotKeyName`: the modifiers' names (NULL: no modifier) and the key's, as C strings -> the name in
/// `out` (`capacity` bytes, NUL-terminated); returns its length. The C++ sizes `out` from the names' lengths.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_hotkey_name(
    m0: *const c_char,
    m1: *const c_char,
    m2: *const c_char,
    key: *const c_char,
    out: *mut u8,
    capacity: usize,
) -> usize {
    let name = hotkey::hot_key_name([c_bytes(m0), c_bytes(m1), c_bytes(m2)], c_bytes(key).unwrap_or(b""));
    let n = name.len().min(capacity.saturating_sub(1));
    core::ptr::copy_nonoverlapping(name.as_ptr(), out, n);
    out.add(n).write(0);
    n
}
