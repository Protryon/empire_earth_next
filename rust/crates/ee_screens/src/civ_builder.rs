//! The civilization builder's events whole: the original `EECBUIEvents.cpp` (the builder form's and its load, save
//! and name dialogs' 18 control events) and the builder form's open and close events (`EEUIECBShowOrHideButtons`,
//! `EEUIECBCleanUp`, out of `EECBCivilizationFiles.cpp`), over a host ([`CivBuilderHost`]) whose calls are each one of
//! the builder's helpers (`CivBuilder*`, which stay C++ with the edited civilization) or a toolkit call
//! (`UI Events/EEScreens.cpp`). The one state of the original unit, whether the next accepted name change is the first
//! one, is a [`CivBuilder`] record here.

use core::ffi::c_long;
use std::sync::Mutex;

use crate::save_load::EE_SCREENS_GAME_MULTIPLAYER_SESSION;

// ---- the events (EEScreenEvent::mEvent; the builder is form 7)

pub const EE_SCREENS_CB_SHOW_CHANGE_NAME: u32 = 0x0701;
pub const EE_SCREENS_CB_ACCEPT_CHANGE_NAME: u32 = 0x0702;
pub const EE_SCREENS_CB_SHOW_LOAD: u32 = 0x0703;
pub const EE_SCREENS_CB_ACCEPT_LOAD: u32 = 0x0704;
pub const EE_SCREENS_CB_SHOW_SAVE: u32 = 0x0705;
/// The save, load and name dialogs' cancel buttons (one body in the original, three classes).
pub const EE_SCREENS_CB_CANCEL_SAVE: u32 = 0x0706;
pub const EE_SCREENS_CB_CANCEL_LOAD: u32 = 0x0707;
pub const EE_SCREENS_CB_CANCEL_NAME: u32 = 0x0708;
pub const EE_SCREENS_CB_ACCEPT_SAVE: u32 = 0x0709;
pub const EE_SCREENS_CB_SAVE_NAME_SELECTED: u32 = 0x070a;
pub const EE_SCREENS_CB_PURCHASED_SELECTED: u32 = 0x070b;
pub const EE_SCREENS_CB_POTENTIAL_SELECTED: u32 = 0x070c;
pub const EE_SCREENS_CB_PURCHASE: u32 = 0x070d;
pub const EE_SCREENS_CB_RETURNED: u32 = 0x070e;
pub const EE_SCREENS_CB_UPDATE_CONTROLS: u32 = 0x070f;
pub const EE_SCREENS_CB_CANCEL_CHANGES: u32 = 0x0710;
pub const EE_SCREENS_CB_ACCEPT_CHANGES: u32 = 0x0711;
/// The load and save dialogs' delete buttons.
pub const EE_SCREENS_CB_DELETE: u32 = 0x0712;
/// The builder form's close event: the edited civilization deleted.
pub const EE_SCREENS_CB_CLEAN_UP: u32 = 0x0713;
/// The builder form's open event: this code plus its five flags (bit 0 the accept button shown, 1 exit, 2 load,
/// 3 save, 4 the points hidden).
pub const EE_SCREENS_CB_BUTTONS: u32 = 0x0720;

pub fn is_civ_builder_event(event: u32) -> bool {
    (EE_SCREENS_CB_SHOW_CHANGE_NAME..=EE_SCREENS_CB_CLEAN_UP).contains(&event)
        || (EE_SCREENS_CB_BUTTONS..EE_SCREENS_CB_BUTTONS + 32).contains(&event)
}

// ---- the host's ops (ee_screenscpp_call on the event's form)

pub const EE_SCREENS_GAME_CB_SHOW_NAME_DIALOG: u32 = 0x2a0; // -> CivBuilderShowNameDialog
pub const EE_SCREENS_GAME_CB_TYPED: u32 = 0x2a1; // (file) -> CivBuilderGetTypedName / GetTypedFileName; the text kept
pub const EE_SCREENS_GAME_CB_ACCEPT_NAME: u32 = 0x2a2; // -> CivBuilderAcceptName
pub const EE_SCREENS_GAME_CB_CLOSE: u32 = 0x2a3; // (apply) -> CivBuilderClose
pub const EE_SCREENS_GAME_CB_SAVE_AS: u32 = 0x2a4; // -> CivBuilderSaveAs
pub const EE_SCREENS_GAME_CB_SHOW_LOAD: u32 = 0x2a5; // -> CivBuilderShowLoad
pub const EE_SCREENS_GAME_CB_SHOW_SAVE: u32 = 0x2a6; // -> CivBuilderShowSave
/// (list: 0 CivBuilderGetLoadFileList, 1 the event form's save list) -> its GetCurrentSelection, or
/// [`EE_SCREENS_CB_NO_LIST`]; the load list's selected file name kept
pub const EE_SCREENS_GAME_CB_SELECTION: u32 = 0x2a7;
pub const EE_SCREENS_GAME_CB_SET_NAME_FROM_CIVILIZATION: u32 = 0x2a8; // -> CivBuilderSetName(the edited one's name)
pub const EE_SCREENS_GAME_CB_CLOSE_FORM: u32 = 0x2a9; // -> CivBuilderCloseForm
pub const EE_SCREENS_GAME_CB_UNCHANGED: u32 = 0x2aa; // -> gEditableCivilization->GetIsUnchanged()
pub const EE_SCREENS_GAME_CB_SAVE_NAME_SELECTED: u32 = 0x2ab; // -> CivBuilderSaveNameSelected
pub const EE_SCREENS_GAME_CB_CAN_CHANGE: u32 = 0x2ac; // -> CivBuilderCanChange
pub const EE_SCREENS_GAME_CB_RESET_AVAILABLE: u32 = 0x2ad; // -> CivBuilderResetAvailableSelection(form)
pub const EE_SCREENS_GAME_CB_RESET_BOUGHT: u32 = 0x2ae; // -> CivBuilderResetBoughtSelection(form)
pub const EE_SCREENS_GAME_CB_LOCKED: u32 = 0x2af; // -> the upgrade lists are read-only (gB565566)
pub const EE_SCREENS_GAME_CB_BUY: u32 = 0x2b0; // -> CivBuilderBuyUpgrade(0)
pub const EE_SCREENS_GAME_CB_RETURN: u32 = 0x2b1; // -> CivBuilderReturnUpgrade
pub const EE_SCREENS_GAME_CB_POINTS_LEFT: u32 = 0x2b2; // -> gEditableCivilization->GetPointsLeft()
pub const EE_SCREENS_GAME_CB_ASK_ACCEPT: u32 = 0x2b3; // -> CivBuilderAskAccept(form)
pub const EE_SCREENS_GAME_CB_ACCEPT_CHANGES: u32 = 0x2b4; // CivBuilderAcceptChanges(NULL)
pub const EE_SCREENS_GAME_CB_FORM_ID: u32 = 0x2b5; // -> the event form's id
pub const EE_SCREENS_GAME_CB_ASK_DELETE: u32 = 0x2b6; // -> CivBuilderAskDelete(form, entity)
pub const EE_SCREENS_GAME_CB_CLEAN_UP: u32 = 0x2b7; // gEditableCivilization deleted
pub const EE_SCREENS_OP_CB_SHOW: u32 = 0x2b8; // (control << 1 | shown) the builder form's control shown or hidden
pub const EE_SCREENS_GAME_CB_HIDE_POINTS: u32 = 0x2b9; // CivBuilderHidePoints
/// Texts in (ee_screenscpp_text) and out (ee_screenscpp_put_text).
pub const EE_SCREENS_TEXT_CB_NAME: u32 = 0x7e; // the kept text (typed, or the load list's file)
pub const EE_SCREENS_TEXT_CB_CIVILIZATION_NAME: u32 = 0x7f; // put: CivBuilderSetCivilizationName
pub const EE_SCREENS_TEXT_CB_SET_NAME: u32 = 0x80; // put -> CivBuilderSetName
pub const EE_SCREENS_TEXT_CB_LOAD: u32 = 0x81; // put -> CivBuilderLoadCivilization

/// `EE_SCREENS_GAME_CB_SELECTION`'s answer for no list.
pub const EE_SCREENS_CB_NO_LIST: u32 = 0xffff_ffff;
const ERROR_NOT_FOUND: c_long = 3;
const FORM_LOAD_CIVILIZATION: u32 = 17;
/// The builder form's accept, exit, load and save buttons (the open event's flags 0..3).
const CB_BUTTONS: [u32; 4] = [17, 16, 2, 15];

/// The builder's events' state: whether the next accepted name change is the first one (the original's file static,
/// initially true).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct CivBuilder {
    pub first_change: bool,
}

impl CivBuilder {
    pub const fn new() -> Self {
        Self { first_change: true }
    }
}

impl Default for CivBuilder {
    fn default() -> Self {
        Self::new()
    }
}

/// The builder's helpers and the toolkit's calls, on the event's form.
pub trait CivBuilderHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with an argument and an answer.
    fn call(&mut self, op: u32, a: u32) -> u32;
    /// A text of the game.
    fn text(&mut self, which: u32) -> Vec<u8>;
    /// A text into the game; its answer.
    fn put_text(&mut self, which: u32, text: &[u8]) -> u32;
}

fn result(value: u32) -> c_long {
    value as i32 as c_long
}
fn op<H: CivBuilderHost + ?Sized>(h: &mut H, op: u32) -> c_long {
    result(h.call(op, 0))
}

/// `EEUIECBAcceptChangeName`: the typed name the civilization's, the edits applied and the builder closed; after an
/// unchanged civilization's save (the flag cleared) the file saved instead and the flag set again.
fn accept_change_name<H: CivBuilderHost + ?Sized>(h: &mut H, state: &Mutex<CivBuilder>) -> c_long {
    let r = result(h.call(EE_SCREENS_GAME_CB_TYPED, 0));
    if r != 0 {
        return r;
    }
    let name = h.text(EE_SCREENS_TEXT_CB_NAME);
    h.put_text(EE_SCREENS_TEXT_CB_CIVILIZATION_NAME, &name);
    let r = op(h, EE_SCREENS_GAME_CB_ACCEPT_NAME);
    if r != 0 {
        return r;
    }
    let first = state.lock().unwrap().first_change;
    if first {
        result(h.call(EE_SCREENS_GAME_CB_CLOSE, 1))
    } else {
        let r = op(h, EE_SCREENS_GAME_CB_SAVE_AS);
        state.lock().unwrap().first_change = true;
        r
    }
}

/// `EEUIECBAcceptLoadCivilization`: the load list's selected file loaded, its name shown, the dialog closed.
fn accept_load<H: CivBuilderHost + ?Sized>(h: &mut H) -> c_long {
    let selection = h.call(EE_SCREENS_GAME_CB_SELECTION, 0);
    if selection == EE_SCREENS_CB_NO_LIST {
        return ERROR_NOT_FOUND;
    }
    if selection != 0 {
        return result(selection);
    }
    let file = h.text(EE_SCREENS_TEXT_CB_NAME);
    let mut r = result(h.put_text(EE_SCREENS_TEXT_CB_LOAD, &file));
    if r == 0 {
        r = op(h, EE_SCREENS_GAME_CB_SET_NAME_FROM_CIVILIZATION);
        if r == 0 {
            r = op(h, EE_SCREENS_GAME_CB_CLOSE_FORM);
        }
    }
    r
}

/// `EEUIECBAcceptSaveCivilization`: the civilization saved; an unchanged one first takes the typed file name as its
/// name (the first-change flag cleared).
fn accept_save<H: CivBuilderHost + ?Sized>(h: &mut H, state: &Mutex<CivBuilder>) -> c_long {
    if h.call(EE_SCREENS_GAME_CB_UNCHANGED, 0) == 0 {
        return op(h, EE_SCREENS_GAME_CB_SAVE_AS);
    }
    let mut r = result(h.call(EE_SCREENS_GAME_CB_TYPED, 1));
    if r == 0 {
        let name = h.text(EE_SCREENS_TEXT_CB_NAME);
        h.put_text(EE_SCREENS_TEXT_CB_CIVILIZATION_NAME, &name);
        r = result(h.put_text(EE_SCREENS_TEXT_CB_SET_NAME, &name));
        state.lock().unwrap().first_change = false;
        if r == 0 {
            r = op(h, EE_SCREENS_GAME_CB_SAVE_AS);
        }
    }
    r
}

/// `EEUIECBShowOrHideButtons`: the builder's accept, exit, load and save buttons shown or hidden, the points
/// hidden with the fifth flag.
fn show_or_hide_buttons<H: CivBuilderHost + ?Sized>(h: &mut H, flags: u32) {
    for (bit, control) in CB_BUTTONS.iter().enumerate() {
        // (the host takes the control and the flag)
        h.call(EE_SCREENS_OP_CB_SHOW, control << 1 | (flags >> bit & 1));
    }
    if flags & 0x10 != 0 {
        op(h, EE_SCREENS_GAME_CB_HIDE_POINTS);
    }
}

/// One of the builder's events; `None` for another form's, else its `ProcessEvent` result.
pub fn dispatch<H: CivBuilderHost + ?Sized>(h: &mut H, state: &Mutex<CivBuilder>, event: u32) -> Option<c_long> {
    if !is_civ_builder_event(event) {
        return None;
    }
    let locked = |h: &mut H| h.call(EE_SCREENS_GAME_CB_LOCKED, 0) != 0;
    Some(match event {
        EE_SCREENS_CB_SHOW_CHANGE_NAME => op(h, EE_SCREENS_GAME_CB_SHOW_NAME_DIALOG),
        EE_SCREENS_CB_ACCEPT_CHANGE_NAME => accept_change_name(h, state),
        EE_SCREENS_CB_SHOW_LOAD => op(h, EE_SCREENS_GAME_CB_SHOW_LOAD),
        EE_SCREENS_CB_ACCEPT_LOAD => accept_load(h),
        EE_SCREENS_CB_SHOW_SAVE => op(h, EE_SCREENS_GAME_CB_SHOW_SAVE),
        EE_SCREENS_CB_CANCEL_SAVE | EE_SCREENS_CB_CANCEL_LOAD | EE_SCREENS_CB_CANCEL_NAME => {
            op(h, EE_SCREENS_GAME_CB_CLOSE_FORM)
        }
        EE_SCREENS_CB_ACCEPT_SAVE => accept_save(h, state),
        EE_SCREENS_CB_SAVE_NAME_SELECTED => op(h, EE_SCREENS_GAME_CB_SAVE_NAME_SELECTED),
        EE_SCREENS_CB_PURCHASED_SELECTED => {
            if h.call(EE_SCREENS_GAME_CB_CAN_CHANGE, 0) != 0 {
                op(h, EE_SCREENS_GAME_CB_RESET_AVAILABLE)
            } else {
                0
            }
        }
        EE_SCREENS_CB_POTENTIAL_SELECTED => op(h, EE_SCREENS_GAME_CB_RESET_BOUGHT),
        EE_SCREENS_CB_PURCHASE => {
            if locked(h) {
                0
            } else {
                op(h, EE_SCREENS_GAME_CB_BUY)
            }
        }
        EE_SCREENS_CB_RETURNED => {
            if locked(h) {
                0
            } else {
                op(h, EE_SCREENS_GAME_CB_RETURN)
            }
        }
        EE_SCREENS_CB_UPDATE_CONTROLS => 0,
        EE_SCREENS_CB_CANCEL_CHANGES => result(h.call(EE_SCREENS_GAME_CB_CLOSE, 0)),
        EE_SCREENS_CB_ACCEPT_CHANGES => {
            // in multiplayer with points left over the player is asked first
            let can_change = h.call(EE_SCREENS_GAME_CB_CAN_CHANGE, 0) != 0;
            if h.call(EE_SCREENS_GAME_MULTIPLAYER_SESSION, 0) != 0
                && can_change
                && h.call(EE_SCREENS_GAME_CB_POINTS_LEFT, 0) > 0
            {
                op(h, EE_SCREENS_GAME_CB_ASK_ACCEPT)
            } else {
                op(h, EE_SCREENS_GAME_CB_ACCEPT_CHANGES);
                0
            }
        }
        EE_SCREENS_CB_DELETE => {
            let list = (h.call(EE_SCREENS_GAME_CB_FORM_ID, 0) != FORM_LOAD_CIVILIZATION) as u32;
            if h.call(EE_SCREENS_GAME_CB_SELECTION, list) == 0 {
                op(h, EE_SCREENS_GAME_CB_ASK_DELETE)
            } else {
                0
            }
        }
        EE_SCREENS_CB_CLEAN_UP => {
            op(h, EE_SCREENS_GAME_CB_CLEAN_UP);
            0
        }
        _ => {
            show_or_hide_buttons(h, event - EE_SCREENS_CB_BUTTONS);
            0
        }
    })
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn events() {
        assert!(is_civ_builder_event(EE_SCREENS_CB_SHOW_CHANGE_NAME) && is_civ_builder_event(EE_SCREENS_CB_CLEAN_UP));
        assert!(is_civ_builder_event(EE_SCREENS_CB_BUTTONS) && is_civ_builder_event(EE_SCREENS_CB_BUTTONS + 31));
        assert!(!is_civ_builder_event(0x0700) && !is_civ_builder_event(0x0714) && !is_civ_builder_event(0x0740));
        assert!(CivBuilder::default().first_change);
    }
}
