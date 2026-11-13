//! `extern "C"` for the main game form's panel groups (`panel_groups.rs`). The host is `ee_screenscpp_group`
//! (`User Interface Thread/EEUIFMGMainGroup.cpp`): `group` is the `EEUIFMGGRust`.

use core::ffi::{c_char, c_void};

use crate::panel_groups::{group_call, GroupHost, GroupText};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_char, c_void};

    extern "C" {
        /// `print` false: the value `which`; `print` true: the label `control`'s text (see `GroupText`).
        pub fn ee_screenscpp_group(
            group: *mut c_void,
            print: bool,
            which: u32,
            control: u32,
            if_changed: bool,
            format: *const c_char,
            strings: *const i32,
            values: *const i32,
        ) -> i32;
    }
}

struct Host(*mut c_void);

impl GroupHost for Host {
    fn value(&mut self, which: u32) -> i32 {
        unsafe {
            sys::ee_screenscpp_group(
                self.0,
                false,
                which,
                0,
                false,
                core::ptr::null(),
                core::ptr::null(),
                core::ptr::null(),
            )
        }
    }
    fn print(&mut self, t: &GroupText) {
        unsafe {
            sys::ee_screenscpp_group(
                self.0,
                true,
                0,
                t.control,
                t.if_changed,
                t.format.as_ptr() as *const c_char,
                t.strings.as_ptr(),
                t.values.as_ptr(),
            );
        }
    }
}

/// A panel group's `Refresh` or `Select` (`EE_SCREENS_GROUP_REFRESH` / `_SELECT`) for its kind.
#[no_mangle]
pub extern "C" fn ee_screens_group_call(group: *mut c_void, kind: u32, call: u32) {
    group_call(kind, call, &mut Host(group));
}
