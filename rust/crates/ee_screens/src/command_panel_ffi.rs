//! `extern "C"` for the command panel (`command_panel.rs`; `User Interface Thread/EEUIFMGStatusButton.cpp`,
//! `EEUIFMGMainGroup.cpp`): plain values in and out, no host calls.
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_ulong;

use crate::command_panel::{
    self, EeScreensCommandMode, EeScreensPanelButton, EeScreensPanelResearch, EeScreensPanelView,
};

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        core::slice::from_raw_parts(p, n)
    }
}

/// The targeting mode of command `button`: false when it starts none.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_command_mode(button: c_ulong, out: *mut EeScreensCommandMode) -> bool {
    match command_panel::command_mode(button) {
        Some(mode) => {
            out.write(mode);
            true
        }
        None => false,
    }
}

/// The panel's gate before the player's `IsHuman` (every other condition).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_panel_gate(view: *const EeScreensPanelView) -> bool {
    command_panel::panel_gate(&*view)
}

/// Whether the panel shows any button.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_panel_shown(view: *const EeScreensPanelView) -> bool {
    command_panel::panel_shown(&*view)
}

/// `ShowCommands`' buttons in order into `out` (room for `capacity`); returns how many.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_panel_buttons(
    view: *const EeScreensPanelView,
    research: *const EeScreensPanelResearch,
    count: usize,
    out: *mut EeScreensPanelButton,
    capacity: usize,
) -> usize {
    let buttons = command_panel::panel_buttons(&*view, slice(research, count));
    let n = buttons.len().min(capacity);
    for (k, b) in buttons.iter().take(n).enumerate() {
        out.add(k).write(*b);
    }
    n
}
