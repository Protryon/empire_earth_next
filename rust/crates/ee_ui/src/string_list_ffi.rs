//! `extern "C"` for the string list's type-to-find (`UILBString.cpp`): the list is a C++ object, asked through
//! `ee_uicpp_string_list_*` on each step (the entity groups' switch went with `groupobj.rs`, R6 ui round 21).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_long, c_ulong, c_void};

use crate::find_key::{self, FindHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_ulong, c_void};

    extern "C" {
        // the list's virtual GetNumLines(); line i's first byte, -1 when the line is empty
        pub fn ee_uicpp_string_list_lines(list: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_string_list_first(list: *mut c_void, i: c_ulong) -> c_int;
    }
}

struct List(*mut c_void);

impl FindHost for List {
    fn lines(&mut self) -> usize {
        unsafe { sys::ee_uicpp_string_list_lines(self.0) as usize }
    }
    fn first(&mut self, i: usize) -> Option<u8> {
        let b = unsafe { sys::ee_uicpp_string_list_first(self.0, i as c_ulong) };
        (b >= 0).then_some(b as u8)
    }
}

/// `UILBString::UpdateTranslatedKey`'s search: the typed key (a C string) and `mCurrentSelection` -> the line to
/// select, -1 when none starts with the key.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_string_list_find(list: *mut c_void, key: *const c_char, selection: c_ulong) -> c_long {
    let key = if key.is_null() { &[][..] } else { core::ffi::CStr::from_ptr(key).to_bytes() };
    match find_key::find(key, selection, &mut List(list)) {
        Some(i) => i as c_long,
        None => -1,
    }
}
