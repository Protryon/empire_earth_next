//! `extern "C"` for the command panel (`command_panel.rs`; `User Interface Thread/EEUIFMGStatusButton.cpp`,
//! `EEUIFMGMainGroup.cpp`): plain values in and out, no host calls.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong};

use crate::command_panel::{
    self, EeScreensCommandMode, EeScreensPanelButton, EeScreensPanelResearch, EeScreensPanelView, EeScreensTechHelp,
};

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        core::slice::from_raw_parts(p, n)
    }
}

/// A command button's first tooltip string.
#[no_mangle]
pub extern "C" fn ee_screens_command_help_string(button: c_ulong) -> c_long {
    command_panel::command_help_string(button)
}

/// Whether a command's tooltip adds the selected priest's conversion cost.
#[no_mangle]
pub extern "C" fn ee_screens_command_help_priest(button: c_ulong) -> bool {
    command_panel::command_help_priest(button)
}

/// A technology button's tooltip plan.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_button_tech_help(
    has_class: bool,
    upgrade_named: bool,
    population: c_long,
    kind: c_long,
    calamity_cost: c_long,
    makes_unit: bool,
    name_id: c_long,
    out: *mut EeScreensTechHelp,
) {
    out.write(command_panel::tech_help(has_class, upgrade_named, population, kind, calamity_cost, makes_unit, name_id));
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
