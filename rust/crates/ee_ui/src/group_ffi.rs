//! `extern "C"` for the entity groups' switch (`User Interface/UIEntityGroup.cpp`) and the string list's
//! type-to-find (`UILBString.cpp`): the group's entities and the list are C++ objects, asked through
//! `ee_uicpp_group` and `ee_uicpp_string_list_*` on each step.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_int, c_long, c_ulong, c_void};

use crate::entity_group::{self, GroupHost};
use crate::find_key::{self, FindHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_ulong, c_void};

    extern "C" {
        // call: 0 GetEntityCount; for entity i: 1 is a button, 2 shown, 3 on its manager's current form,
        // 4 AddDirtyRectangle, 5 HideControl, 6 SetDirty(true), 7 its form's tabbing, 8 it is the tab stop,
        // 9 its form's Tab(), 10 the hot key's SetEnabled(arg)
        pub fn ee_uicpp_group(group: *mut c_void, call: c_int, i: c_ulong, arg: bool) -> c_ulong;
        // the list's virtual GetNumLines(); line i's first byte, -1 when the line is empty
        pub fn ee_uicpp_string_list_lines(list: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_string_list_first(list: *mut c_void, i: c_ulong) -> c_int;
    }
}

struct Group(*mut c_void);

impl Group {
    fn call(&self, call: c_int, i: usize, arg: bool) -> c_ulong {
        unsafe { sys::ee_uicpp_group(self.0, call, i as c_ulong, arg) }
    }
}

impl GroupHost for Group {
    fn count(&self) -> usize {
        self.call(0, 0, false) as usize
    }
    fn is_button(&mut self, i: usize) -> bool {
        self.call(1, i, false) != 0
    }
    fn shown(&mut self, i: usize) -> bool {
        self.call(2, i, false) != 0
    }
    fn on_current_form(&mut self, i: usize) -> bool {
        self.call(3, i, false) != 0
    }
    fn add_dirty(&mut self, i: usize) {
        self.call(4, i, false);
    }
    fn hide(&mut self, i: usize) {
        self.call(5, i, false);
    }
    fn set_dirty(&mut self, i: usize) {
        self.call(6, i, false);
    }
    fn tabbing(&mut self, i: usize) -> bool {
        self.call(7, i, false) != 0
    }
    fn is_tab_stop(&mut self, i: usize) -> bool {
        self.call(8, i, false) != 0
    }
    fn tab(&mut self, i: usize) {
        self.call(9, i, false);
    }
    fn set_enabled(&mut self, i: usize, on: bool) {
        self.call(10, i, on);
    }
}

/// `UIEntityGroup::Disable`'s loop (the C++ sets `mDisableCount` first).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_group_disable(group: *mut c_void) {
    entity_group::disable(&mut Group(group));
}

/// `UIEntityGroup::Enable`'s loop.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_group_enable(group: *mut c_void) {
    entity_group::enable(&mut Group(group));
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
