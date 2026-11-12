//! The civilization choice form (form 35, the predefined civilizations a player picks in game) whole: the original
//! `EEUIEPDCSEvents.cpp` and its header's in-class bodies (the form's open and close events, the list's selection,
//! the posted load of the selected civilization, Ok, Cancel and the statistics' clearing; the file helper
//! `GetSelectedCivilizationFile`) over a host ([`CivChoiceHost`]) whose calls are each one toolkit or game call
//! (`UI Events/EEScreens.cpp`). The civilizations reach Rust as their name and file texts by index.
//!
//! The form's controls: 0 Cancel, 1 Ok, 3 the civilization list (`EECBUIListBox`: lines "category/name/..."), 4 the
//! selected civilization's statistics (its upgrades), 5 the "Please wait" label.

use core::ffi::c_long;
use std::collections::BTreeMap;
use std::sync::Mutex;

use crate::main_game_options::EE_SCREENS_OP_GO_HOT_KEY;
use crate::menus::{EE_SCREENS_OP_POP, EE_SCREENS_OP_SHOW, EE_SCREENS_TEXT_LABEL, EE_SCREENS_TEXT_MESSAGE};
use crate::save_load::EE_SCREENS_OP_POST_EVENT;
use crate::scenario_screens::EE_SCREENS_OP_TAB_STOP;
use crate::settings_form::{EE_SCREENS_OP_CLOSE_FORM, EE_SCREENS_OP_LIST_CLEAR, EE_SCREENS_OP_TRIGGER_HERE};

// ---- the events (EEScreenEvent::mEvent)

pub const EE_SCREENS_PDCS_OPEN: u32 = 0x2301;
/// The form's close event (empty).
pub const EE_SCREENS_PDCS_CLOSE: u32 = 0x2302;
/// The civilization list's selection changed.
pub const EE_SCREENS_PDCS_SELECT: u32 = 0x2303;
/// Ok (its button, Enter, the keypad's Enter, a double click on the list).
pub const EE_SCREENS_PDCS_OK: u32 = 0x2304;
pub const EE_SCREENS_PDCS_CANCEL: u32 = 0x2305;
/// A button pressed on the civilization list: the statistics cleared without a selection past the first line.
pub const EE_SCREENS_PDCS_CLEAR: u32 = 0x2306;
/// The posted load of the selected civilization: `EE_SCREENS_PDCS_LOAD | slot << 16`, the slot (1..=255) the file's
/// in [`CivChoice`].
pub const EE_SCREENS_PDCS_LOAD: u32 = 0x2307;

pub fn is_civ_choice_event(event: u32) -> bool {
    (EE_SCREENS_PDCS_OPEN..=EE_SCREENS_PDCS_CLEAR).contains(&event)
        || (event & 0xffff == EE_SCREENS_PDCS_LOAD && event >> 24 == 0)
}

// ---- the host's ops (ee_screenscpp_call; "the form" is form 35 by the manager, "the event's" the event's form)

/// (control, flags, red = green bits, blue bits) the event's form's list's style: no auto select, the selection
/// colour; flags 1 its lines sorted, 2 the statistics' (no points, every line selectable).
pub const EE_SCREENS_OP_PDCS_STYLE: u32 = 0x2bd;
/// `gCivilizationManager.FillCivilizationList` on the event's form's civilization list.
pub const EE_SCREENS_GAME_PDCS_FILL: u32 = 0x2be;
/// (control) -> the event's form's list has a selection (`GetHasSelection`).
pub const EE_SCREENS_OP_PDCS_HAS_SELECTION: u32 = 0x2bf;
/// `gWorld.PauseGame()`.
pub const EE_SCREENS_GAME_PDCS_PAUSE: u32 = 0x2c0;
/// -> the number of civilizations (`gCivilizationManager.GetCivilizations`, kept for the texts).
pub const EE_SCREENS_GAME_PDCS_CIVS: u32 = 0x2c1;
/// The form's statistics list's `DeleteAllEntries`.
pub const EE_SCREENS_OP_PDCS_CLEAR_STATS: u32 = 0x2c2;
/// -> the form's civilization list's selection plus 1, 0 when it has none (`GetCurrentSelection`).
pub const EE_SCREENS_OP_PDCS_SELECTION: u32 = 0x2c3;
/// -> 1 when the form has its civilization list, statistics list and label.
pub const EE_SCREENS_OP_PDCS_CONTROLS: u32 = 0x2c4;
/// The kept load's upgrades into the form's statistics list (`SetCivilizationUpgrades`).
pub const EE_SCREENS_OP_PDCS_SET_UPGRADES: u32 = 0x2c5;
/// The form's label hidden.
pub const EE_SCREENS_OP_PDCS_HIDE_LABEL: u32 = 0x2c6;
/// The form's civilization list's `ResetSelection`.
pub const EE_SCREENS_OP_PDCS_RESET: u32 = 0x2c7;
/// The kept load's civilization requested for the current player (`EEMCivUpgradesRequested`: to the server in a
/// multiplayer session, else executed at once).
pub const EE_SCREENS_GAME_PDCS_REQUEST: u32 = 0x2c8;
/// Texts in: the form's civilization list's selected line (missing without a selection); civilization `i`'s name (its
/// string) and file.
pub const EE_SCREENS_TEXT_PDCS_LINE: u32 = 0x83;
pub const EE_SCREENS_TEXT_PDCS_CIV_NAME: u32 = 0x84;
pub const EE_SCREENS_TEXT_PDCS_CIV_FILE: u32 = 0x85;
/// Put: a civilization file loaded (`EELoadCivilization` from the civilizations' directory; its name and upgrades
/// kept) -> the loader's result. Put: the form's label's text.
pub const EE_SCREENS_TEXT_PDCS_LOAD: u32 = 0x86;
pub const EE_SCREENS_TEXT_PDCS_LABEL: u32 = 0x87;

// the controls, triggers, keys, strings
const PDCS_CANCEL: u32 = 0;
const PDCS_OK: u32 = 1;
const PDCS_CIVS: u32 = 3;
const PDCS_STATS: u32 = 4;
const PDCS_LABEL: u32 = 5;
const PDCS_TRIGGER_SELECTION_CHANGED: u32 = 1003;
const PDCS_TRIGGER_DOUBLE_CLICKED: u32 = 1004;
const PDCS_TRIGGER_BUTTON_DOWN: u32 = 1007;
const PDCS_TRIGGER_BUTTON_UP: u32 = 1008;
const PDCS_DIK_RETURN: u32 = 0x1c;
const PDCS_DIK_NUMPADENTER: u32 = 0x9c;
const PDCS_IDS_FILE_COULDNT_BE_OPENED: u32 = 7152;
const PDCS_IDS_PLEASE_WAIT: u32 = 31842;
const PDCS_IDS_PLEASE_SELECT_CIVILIZATION_FROM_LIST: u32 = 31846;
const PDCS_SHOWN: u32 = 1;
/// The list's style flags (`EE_SCREENS_OP_PDCS_STYLE`).
const PDCS_STYLE_SORTED: u32 = 1;
const PDCS_STYLE_STATISTICS: u32 = 2;

/// The posted loads' files by slot (the original's `EEUIEPDCSLoadCiv::mFile`, which the event carried).
#[derive(Debug, Default)]
pub struct CivChoice {
    files: BTreeMap<u32, Vec<u8>>,
    last: u32,
}

impl CivChoice {
    pub const fn new() -> Self {
        CivChoice { files: BTreeMap::new(), last: 0 }
    }
    /// A posted load's code for `file`.
    fn post(&mut self, file: Vec<u8>) -> u32 {
        self.last = self.last % 255 + 1;
        self.files.insert(self.last, file);
        EE_SCREENS_PDCS_LOAD | self.last << 16
    }
}

/// The calls of the civilization choice form.
pub trait CivChoiceHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to four arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32;
    /// A text of the game; `None` when it is missing.
    fn text(&mut self, which: u32, arg: u32) -> Option<Vec<u8>>;
    /// A text into the game, and the host's answer.
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32;
}

fn op<H: CivChoiceHost + ?Sized>(h: &mut H, op: u32, a: u32) -> u32 {
    h.call(op, a, 0, 0, 0)
}
fn string<H: CivChoiceHost + ?Sized>(h: &mut H, id: u32) -> Vec<u8> {
    h.text(crate::menus::EE_SCREENS_TEXT_STRING, id).unwrap_or_default()
}

/// `UWideString::Tokenize` on "/": the runs between separators (no empty token).
fn tokens(line: &[u8]) -> Vec<&[u8]> {
    line.split(|&c| c == b'/').filter(|t| !t.is_empty()).collect()
}

/// `GetSelectedCivilizationFile`: the file of the civilization whose name is the selected line's second token (the
/// first such civilization); `None` without a selection, with fewer than three tokens or an empty file.
fn selected_file<H: CivChoiceHost + ?Sized>(h: &mut H) -> Option<Vec<u8>> {
    let line = h.text(EE_SCREENS_TEXT_PDCS_LINE, 0)?;
    let tokens = tokens(&line);
    if tokens.len() <= 2 {
        return None;
    }
    let name = tokens[1];
    let count = op(h, EE_SCREENS_GAME_PDCS_CIVS, 0);
    let mut file = Vec::new();
    for i in 0..count {
        if h.text(EE_SCREENS_TEXT_PDCS_CIV_NAME, i).unwrap_or_default() == name {
            file = h.text(EE_SCREENS_TEXT_PDCS_CIV_FILE, i).unwrap_or_default();
            break;
        }
    }
    (!file.is_empty()).then_some(file)
}

/// `EEUIEPDCSFormOpen`: Cancel, Ok (its button, Enter, the keypad's Enter, the list's double click), the selection
/// and the clearing hooked up, both lists styled, the civilizations listed, the tab stop on the list, the selection's
/// statistics shown (the selection event run on the list), the game paused.
fn open<H: CivChoiceHost + ?Sized>(h: &mut H, state: &Mutex<CivChoice>) {
    h.call(EE_SCREENS_OP_TRIGGER_HERE, PDCS_CANCEL, PDCS_TRIGGER_BUTTON_UP, EE_SCREENS_PDCS_CANCEL, 0);
    h.call(EE_SCREENS_OP_TRIGGER_HERE, PDCS_OK, PDCS_TRIGGER_BUTTON_UP, EE_SCREENS_PDCS_OK, 0);
    h.call(EE_SCREENS_OP_GO_HOT_KEY, PDCS_DIK_RETURN, EE_SCREENS_PDCS_OK, 0, 0);
    h.call(EE_SCREENS_OP_GO_HOT_KEY, PDCS_DIK_NUMPADENTER, EE_SCREENS_PDCS_OK, 0, 0);
    h.call(EE_SCREENS_OP_TRIGGER_HERE, PDCS_CIVS, PDCS_TRIGGER_DOUBLE_CLICKED, EE_SCREENS_PDCS_OK, 0);
    h.call(EE_SCREENS_OP_TRIGGER_HERE, PDCS_CIVS, PDCS_TRIGGER_SELECTION_CHANGED, EE_SCREENS_PDCS_SELECT, 0);
    h.call(EE_SCREENS_OP_TRIGGER_HERE, PDCS_CIVS, PDCS_TRIGGER_BUTTON_DOWN, EE_SCREENS_PDCS_CLEAR, 0);
    h.call(EE_SCREENS_OP_PDCS_STYLE, PDCS_CIVS, 0, 0.0f32.to_bits(), 0.39f32.to_bits());
    h.call(
        EE_SCREENS_OP_PDCS_STYLE,
        PDCS_STATS,
        PDCS_STYLE_SORTED | PDCS_STYLE_STATISTICS,
        0.18f32.to_bits(),
        0.28f32.to_bits(),
    );
    op(h, EE_SCREENS_GAME_PDCS_FILL, PDCS_CIVS);
    op(h, EE_SCREENS_OP_TAB_STOP, PDCS_CIVS);
    if op(h, EE_SCREENS_OP_PDCS_HAS_SELECTION, PDCS_CIVS) != 0 {
        select(h, state);
    }
    op(h, EE_SCREENS_GAME_PDCS_PAUSE, 0);
}

/// `EEUIEPDCSSelectCiv`: the statistics cleared; for a selected civilization with a file, "Please wait" shown and its
/// load posted.
fn select<H: CivChoiceHost + ?Sized>(h: &mut H, state: &Mutex<CivChoice>) {
    op(h, EE_SCREENS_OP_LIST_CLEAR, PDCS_STATS);
    let Some(file) = selected_file(h) else { return };
    let code = state.lock().unwrap().post(file);
    let wait = string(h, PDCS_IDS_PLEASE_WAIT);
    h.put_text(EE_SCREENS_TEXT_LABEL, PDCS_LABEL, &wait);
    h.call(EE_SCREENS_OP_SHOW, PDCS_LABEL, PDCS_SHOWN, 0, 0);
    op(h, EE_SCREENS_OP_POST_EVENT, code);
}

/// `EEUIEPDCSLoadCiv`: the posted civilization's file loaded: its upgrades in the statistics and the label hidden, or
/// the list's selection reset and the error shown.
fn load<H: CivChoiceHost + ?Sized>(h: &mut H, file: &[u8]) {
    if op(h, EE_SCREENS_OP_PDCS_CONTROLS, 0) == 0 {
        return;
    }
    if h.put_text(EE_SCREENS_TEXT_PDCS_LOAD, 0, file) == 0 {
        op(h, EE_SCREENS_OP_PDCS_SET_UPGRADES, 0);
        op(h, EE_SCREENS_OP_PDCS_HIDE_LABEL, 0);
    } else {
        op(h, EE_SCREENS_OP_PDCS_RESET, 0);
        let error = string(h, PDCS_IDS_FILE_COULDNT_BE_OPENED);
        h.put_text(EE_SCREENS_TEXT_PDCS_LABEL, 0, &error);
    }
}

/// `EEUIEPDCSOk`: the selected civilization requested for the current player and the form closed, or why not shown.
fn ok<H: CivChoiceHost + ?Sized>(h: &mut H) {
    let message = match selected_file(h) {
        Some(file) => {
            if h.put_text(EE_SCREENS_TEXT_PDCS_LOAD, 0, &file) == 0 {
                op(h, EE_SCREENS_GAME_PDCS_REQUEST, 0);
                op(h, EE_SCREENS_OP_CLOSE_FORM, 0);
                return;
            }
            PDCS_IDS_FILE_COULDNT_BE_OPENED
        }
        None => PDCS_IDS_PLEASE_SELECT_CIVILIZATION_FROM_LIST,
    };
    let text = string(h, message);
    h.put_text(EE_SCREENS_TEXT_MESSAGE, 1, &text);
}

/// One of the form's events; `None` for another form's, else its `ProcessEvent` result (always 0).
pub fn dispatch<H: CivChoiceHost + ?Sized>(h: &mut H, state: &Mutex<CivChoice>, event: u32) -> Option<c_long> {
    if !is_civ_choice_event(event) {
        return None;
    }
    match event {
        EE_SCREENS_PDCS_OPEN => open(h, state),
        EE_SCREENS_PDCS_CLOSE => {}
        EE_SCREENS_PDCS_SELECT => select(h, state),
        EE_SCREENS_PDCS_OK => ok(h),
        EE_SCREENS_PDCS_CANCEL => {
            op(h, EE_SCREENS_OP_POP, 0);
        }
        EE_SCREENS_PDCS_CLEAR => {
            // no selection, or the first line
            if op(h, EE_SCREENS_OP_PDCS_SELECTION, 0) <= 1 {
                op(h, EE_SCREENS_OP_PDCS_CLEAR_STATS, 0);
            }
        }
        _ => {
            let file = state.lock().unwrap().files.remove(&(event >> 16)).unwrap_or_default();
            load(h, &file);
        }
    }
    Some(0)
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn events_and_tokens() {
        assert!(is_civ_choice_event(EE_SCREENS_PDCS_OPEN) && is_civ_choice_event(EE_SCREENS_PDCS_CLEAR));
        assert!(is_civ_choice_event(EE_SCREENS_PDCS_LOAD | 7 << 16));
        assert!(!is_civ_choice_event(EE_SCREENS_PDCS_LOAD | 0x0f00_0000) && !is_civ_choice_event(0x2300));
        assert_eq!(tokens(b"//a/b//c/"), [&b"a"[..], b"b", b"c"]);
        let mut s = CivChoice::new();
        s.last = 254;
        assert_eq!(s.post(b"x".to_vec()) >> 16, 255);
        assert_eq!(s.post(b"y".to_vec()) >> 16, 1);
    }
}
