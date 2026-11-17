//! `extern "C"` for the main game form's main panel group (`main_group.rs`). The host is `ee_screenscpp_main_group`
//! (`User Interface Thread/EEUIFMGMainGroup.cpp`) on the group.

use core::ffi::c_void;

use crate::screens::main_group::{self, GroupHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_main_group(group: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64;
    }
}

struct Host(*mut c_void);

impl GroupHost for Host {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        unsafe { sys::ee_screenscpp_main_group(self.0, op, a, b, c) }
    }
}

/// `EEUIFMGMainGroup`'s member `which` (`EE_SCREENS_MGR_DO_*`) on the group with the unit `unit` (an `EEUnit*`) and
/// `b` (a command button's button | its control << 32): `GetSelectedUnit`'s unit, `WantsRefresh`'s answer, else 0.
#[no_mangle]
pub extern "C" fn ee_screens_main_group_member(group: *mut c_void, which: u32, unit: *mut c_void, b: i64) -> i64 {
    main_group::member(&mut Host(group), which, unit as i64, b)
}
