//! `extern "C"` for the hot keys' name (`User Interface/UIHotKey.cpp` `GetHotKeyName`); the hot key is whole in Rust
//! (`hotkeyobj.rs`).
#![allow(clippy::missing_safety_doc)]

#[cfg(not(any(test, feature = "no_cpp")))]
use core::ffi::c_char;

#[cfg(not(any(test, feature = "no_cpp")))]
use crate::hotkey;

#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn c_bytes<'a>(s: *const c_char) -> Option<&'a [u8]> {
    if s.is_null() {
        None
    } else {
        Some(core::ffi::CStr::from_ptr(s).to_bytes())
    }
}

/// `UIHotKey::GetHotKeyName`: the record's modifiers' and key's entity names (ee_base's
/// `ee_input_entity_name`) as the display name, e.g. "Ctrl + Shift + A"; valid until the next call on this thread.
///
/// # Safety
/// `rec` a live hot key record whose key is set.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_hotkey_display_name(rec: *const crate::hotkeyobj::EeUiHotKeyFields) -> *const c_char {
    thread_local! {
        static NAME: core::cell::RefCell<Vec<u8>> = const { core::cell::RefCell::new(Vec::new()) };
    }
    // SAFETY: the contract; each entity's name lives as long as the entity
    let name_of = |e: *mut core::ffi::c_void| {
        if e.is_null() {
            None
        } else {
            unsafe { c_bytes(ee_base::input::ffi::ee_input_entity_name(e.cast())) }
        }
    };
    // SAFETY: the contract
    let (modifiers, key) = unsafe { ((*rec).mModifiers, (*rec).mKey) };
    let mut name = hotkey::hot_key_name(
        [name_of(modifiers[0]), name_of(modifiers[1]), name_of(modifiers[2])],
        name_of(key).unwrap_or(b""),
    );
    name.push(0);
    NAME.with(|n| {
        *n.borrow_mut() = name;
        n.borrow().as_ptr() as *const c_char
    })
}
