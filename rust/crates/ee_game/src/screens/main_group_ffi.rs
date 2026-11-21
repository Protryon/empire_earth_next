//! `extern "C"` for the main game form's main panel group (`main_group.rs`). The host is Rust over the
//! group's C++ shims (`main_group_calls.rs`; `User Interface Thread/EEUIFMGMainGroup.cpp`).

use core::ffi::c_void;

use crate::screens::main_group::{self, GroupHost};

struct Host(*mut c_void);

impl Host {
    /// `EE_SCREENS_MGR_OP_FORMS`: the group's form (0) or the manager's main game form (1).
    fn form(&self, which: i64) -> *mut c_void {
        use crate::screens::main_group::EE_SCREENS_MGR_OP_FORMS;
        Self::host(self.0, EE_SCREENS_MGR_OP_FORMS, which, 0, 0) as *mut c_void
    }
}

impl Host {
    /// The group's host ops in Rust over the C++ shims (main_group_calls.rs).
    fn host(group: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64 {
        crate::screens::main_group_calls::call(&mut crate::screens::game_cpp::Ffi, group as usize, op, a, b, c)
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
        if let Some(r) = crate::screens::hud_direct::main_group(|| self.form(0), op, a, b) {
            return r;
        }
        Self::host(self.0, op, a, b, c)
    }
}

/// `EEUIFMGMainGroup`'s member `which` (`EE_SCREENS_MGR_DO_*`) on the group with the unit `unit` (an `EEUnit*`) and
/// `b` (a command button's button | its control << 32): `GetSelectedUnit`'s unit, `WantsRefresh`'s answer, else 0.
#[no_mangle]
pub extern "C" fn ee_screens_main_group_member(group: *mut c_void, which: u32, unit: *mut c_void, b: i64) -> i64 {
    main_group::member(&mut Host(group), which, unit as i64, b)
}
