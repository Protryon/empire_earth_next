//! The message box's `Display` inputs while Rust shows it (once EEUIFError.cpp's statics): the message (a
//! `UWideString*`), the callbacks' data, the two callbacks, the form the box's Cancel closes; `ee_screens_dialog_show`
//! stores them and asks the box's kind (`shells_dialog.rs`) to show it.
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;
use std::sync::Mutex;

use crate::screens::shells_dialog::EE_SCREENS_SKOP_DIALOG_SHOW;

#[derive(Clone, Copy)]
struct Inputs {
    message: usize,
    data: usize,
    callbacks: [usize; 2],
    close: usize,
}

/// The last box's inputs (the UI thread's).
static INPUTS: Mutex<Inputs> = Mutex::new(Inputs { message: 0, data: 0, callbacks: [0; 2], close: 0 });

fn inputs() -> Inputs {
    *INPUTS.lock().unwrap_or_else(|e| e.into_inner())
}

/// `EEUIFErrorShow` (`EEUIFErrorDisplay` through the game's UI table): the new box `dialog` (its `UIForm*`) shown as
/// `kind` with the message, its alignment, the callbacks and their data, the form Cancel closes and the flag.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_dialog_show(
    dialog: *mut c_void,
    kind: u32,
    message: *const c_void,
    alignment: u32,
    data: *mut c_void,
    first: *const c_void,
    second: *const c_void,
    close: *mut c_void,
    flag: bool,
) {
    *INPUTS.lock().unwrap_or_else(|e| e.into_inner()) = Inputs {
        message: message as usize,
        data: data as usize,
        callbacks: [first as usize, second as usize],
        close: close as usize,
    };
    // SAFETY: the box is a live shell (its kind registered at its Make)
    unsafe {
        crate::screens::shells_ffi::ee_screens_shell_op_at(
            dialog,
            EE_SCREENS_SKOP_DIALOG_SHOW,
            i64::from(kind),
            i64::from(alignment) | i64::from(flag) << 32,
            core::ptr::null_mut(),
        );
    }
}

/// The box's callback `index` (0 OK/Yes, 1 No).
#[no_mangle]
pub extern "C" fn ee_screens_dlg_callback(index: u32) -> usize {
    inputs().callbacks.get(index as usize).copied().unwrap_or(0)
}

/// The callbacks' data.
#[no_mangle]
pub extern "C" fn ee_screens_dlg_data() -> usize {
    inputs().data
}

/// The form the box's Cancel closes.
#[no_mangle]
pub extern "C" fn ee_screens_dlg_close_form() -> *mut c_void {
    inputs().close as *mut c_void
}

/// The message (a `const UWideString*`).
#[no_mangle]
pub extern "C" fn ee_screens_dlg_message() -> *const c_void {
    inputs().message as *const c_void
}
