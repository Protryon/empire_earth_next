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

impl Host {
    /// `EE_SCREENS_MGR_OP_FORMS`: the group's form (0) or the manager's main game form (1).
    fn form(&self, which: i64) -> *mut c_void {
        use crate::screens::main_group::EE_SCREENS_MGR_OP_FORMS;
        unsafe { sys::ee_screenscpp_main_group(self.0, EE_SCREENS_MGR_OP_FORMS, which, 0, 0) as *mut c_void }
    }
}

impl GroupHost for Host {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        if let Some(rec) = crate::screens::main_form_fields::group_record(self.0) {
            // SAFETY: the live group's record; no call out while the reference lives
            if let Some(r) = crate::screens::main_form_fields::group_op(unsafe { &mut *rec }, op, a, b) {
                return r;
            }
        }
        if op == crate::screens::main_group::EE_SCREENS_MGR_OP_UI_STATE {
            if let Some(rec) = crate::screens::main_form_fields::record(self.form(0)) {
                // SAFETY: the group's live form's record (its mOwner, an EEUIFMainGame)
                return unsafe { (*rec).mUIState } as i64;
            }
        }
        if let Some(r) = crate::screens::hud_views::main_group(op, a, b, c) {
            return r;
        }
        if let Some(r) = crate::screens::hud_views::main_group_toolkit(|| self.form(0), || self.form(1), op, a, b) {
            return r;
        }
        unsafe { sys::ee_screenscpp_main_group(self.0, op, a, b, c) }
    }
}

/// `EEUIFMGMainGroup`'s member `which` (`EE_SCREENS_MGR_DO_*`) on the group with the unit `unit` (an `EEUnit*`) and
/// `b` (a command button's button | its control << 32): `GetSelectedUnit`'s unit, `WantsRefresh`'s answer, else 0.
#[no_mangle]
pub extern "C" fn ee_screens_main_group_member(group: *mut c_void, which: u32, unit: *mut c_void, b: i64) -> i64 {
    main_group::member(&mut Host(group), which, unit as i64, b)
}
