//! `extern "C"` for the main game form's panel groups (`panel_groups.rs`): `group` is the `EEUIFMGGRust`; its
//! values are one-statement shims each (`User Interface Thread/EEUIFMGMainGroup.cpp`), its texts printed here and
//! set on the form's label through the screens' shims.

use core::ffi::c_void;

use crate::screens::game_calls::string;
use crate::screens::game_cpp::{Cpp, EeScreensShimOut, EeScreensShimText, Ffi};
use crate::screens::panel_groups::*;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        /// The group's main game form.
        pub fn ee_screenscpp_group_owner(group: *mut c_void) -> usize;
        pub fn ee_screenscpp_group_conversions() -> i32;
        pub fn ee_screenscpp_group_conversions_max() -> i32;
        pub fn ee_screenscpp_group_wonders_to_win() -> i32;
        pub fn ee_screenscpp_group_start_epoch() -> i32;
        pub fn ee_screenscpp_group_end_epoch() -> i32;
        pub fn ee_screenscpp_group_epoch_title(epoch: i32) -> i32;
        pub fn ee_screenscpp_group_has_player() -> bool;
    }
}

struct Host(*mut c_void);

impl GroupHost for Host {
    fn value(&mut self, which: u32) -> i32 {
        // SAFETY: the shims read the world
        unsafe {
            match which {
                EE_SCREENS_GROUP_VALUE_CONVERSIONS => sys::ee_screenscpp_group_conversions(),
                EE_SCREENS_GROUP_VALUE_CONVERSIONS_MAX => sys::ee_screenscpp_group_conversions_max(),
                EE_SCREENS_GROUP_VALUE_WONDERS_TO_WIN => sys::ee_screenscpp_group_wonders_to_win(),
                EE_SCREENS_GROUP_VALUE_EPOCH_TITLE => sys::ee_screenscpp_group_epoch_title(shown_epoch(
                    sys::ee_screenscpp_group_start_epoch(),
                    sys::ee_screenscpp_group_end_epoch(),
                )),
                EE_SCREENS_GROUP_VALUE_HAS_PLAYER => sys::ee_screenscpp_group_has_player() as i32,
                _ => 0,
            }
        }
    }
    fn print(&mut self, t: &GroupText) {
        let first = string(t.strings[0] as u32);
        let second = if t.strings[1] != -1 { string(t.strings[1] as u32) } else { Vec::new() };
        let text = group_line(t, &first, &second);
        // SAFETY: the group is live; its owner is the main game form
        let form = unsafe { sys::ee_screenscpp_group_owner(self.0) };
        let label = Ffi.form_control(form, t.control);
        if t.if_changed {
            let mut shown = Vec::new();
            Ffi.label_text(label, EeScreensShimOut::of(&mut shown));
            if shown == text {
                return;
            }
        }
        Ffi.label_set_text(label, EeScreensShimText::of(&text));
    }
}

/// A panel group's `Refresh` or `Select` (`EE_SCREENS_GROUP_REFRESH` / `_SELECT`) for its kind.
#[no_mangle]
pub extern "C" fn ee_screens_group_call(group: *mut c_void, kind: u32, call: u32) {
    group_call(kind, call, &mut Host(group));
}
