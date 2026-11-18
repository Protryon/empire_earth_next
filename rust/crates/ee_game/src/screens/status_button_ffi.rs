//! `extern "C"` for the command buttons' presses (`status_button.rs`), over the main game form's host
//! (`ee_screenscpp_main_form`, `User Interface Thread/EEUIFMainGame.cpp`): `call` is the form's call record of one
//! press (or `CopyData` or `GetHelpString` call), holding the button, the first selected unit (the unload's transport),
//! the held text (`CanProduce`'s reason, the help text's pieces) and the help string's result.
//!
//! The button's ops that the form's host already answers go there as the form's ops ([`form_op`]); the others are the
//! form's host's `EE_SCREENS_MF_OP_SB + op` (its status button part, `EEStatusButtonOp`), c in io.n[0].

use core::ffi::c_void;

use crate::screens::status_button::*;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    use crate::screens::main_form::EeScreensMainFormIo;

    extern "C" {
        pub fn ee_screenscpp_main_form(call: *mut c_void, op: u32, a: i64, b: i64, io: *mut EeScreensMainFormIo)
            -> i64;
    }
}

struct Host(*mut c_void);

impl ButtonHost for Host {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        ee_screens_status_button_op(self.0, op, a, b, c)
    }
}

/// One button op (`EE_SCREENS_SB_OP_*`) over the form's call record, as [`form_op`] sends it to the form's host.
#[no_mangle]
pub extern "C" fn ee_screens_status_button_op(call: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64 {
    if let Some(r) = crate::screens::hud_views::status_button(op, a) {
        return r;
    }
    // SAFETY: call is the form's live call record
    form_op(&mut |op, a, b, io| unsafe { sys::ee_screenscpp_main_form(call, op, a, b, io) }, op, a, b, c)
}

/// `EEUIFMGStatusButton`'s member `which` (`EE_SCREENS_SB_DO_*`) over the form's call record: `CopyData`'s
/// `UIButton::CopyData` result, `GetHelpString`'s true, else 0.
#[no_mangle]
pub extern "C" fn ee_screens_status_button_member(call: *mut c_void, which: u32) -> i64 {
    let h = &mut Host(call);
    match which {
        EE_SCREENS_SB_DO_UP => {
            button_up(h);
            0
        }
        EE_SCREENS_SB_DO_RIGHT_UP => {
            right_button_up(h);
            0
        }
        EE_SCREENS_SB_DO_COPY_DATA => copy_data(h),
        _ => help_string(h) as i64,
    }
}
