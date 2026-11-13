//! The campaign editor's events whole (form 24): the original `EEUIECampaignEditorEvents.cpp` (the form's open and
//! close events, its 13 control events, the deferred save and the "save before ..." questions' and the delete
//! question's callbacks) over a host ([`CampaignEditorHost`]) whose calls are each one toolkit call or one call of the
//! editor's model (`EECampaignEditorBehavior`, which stays C++ with its one global, `gCampaignEditor`, in
//! `UI Events/EEScreens.cpp`). The form keeps no state of its own; the original's `dynamic_cast` checks are the
//! host's [`EE_SCREENS_OP_CE_IS`], asked before each control's read or write, as the original asked them.

use core::ffi::c_long;

use crate::custom_campaign::{
    EE_SCREENS_OP_FORM_LIST_COUNT, EE_SCREENS_TEXT_CAMPAIGN_EXTENSION, EE_SCREENS_TEXT_MESSAGE_NONE_FOUND,
};
use crate::menus::{
    EE_SCREENS_GAME_CREATE_EDITOR, EE_SCREENS_GAME_START_EDITOR, EE_SCREENS_TEXT_EDIT, EE_SCREENS_TEXT_LABEL,
    EE_SCREENS_TEXT_MESSAGE, EE_SCREENS_TEXT_STRING,
};
use crate::save_load::{EE_SCREENS_OP_FORM_LIST_SELECTED, EE_SCREENS_OP_POST_EVENT};
use crate::settings_form::{
    EE_SCREENS_OP_CLOSE_FORM, EE_SCREENS_OP_COMBO_SELECT, EE_SCREENS_OP_GROUP, EE_SCREENS_OP_LIST_CLEAR,
    EE_SCREENS_TEXT_COMBO_DATA, EE_SCREENS_TEXT_LIST_DATA,
};

// ---- the events (EEScreenEvent::mEvent; the editor is form 24)

pub const EE_SCREENS_CE_OPEN: u32 = 0x1801;
/// The form's close event and the name box's change: nothing (the original's shared `return 0`).
pub const EE_SCREENS_CE_CLOSE: u32 = 0x1802;
pub const EE_SCREENS_CE_ADD_SCENARIO: u32 = 0x1803;
pub const EE_SCREENS_CE_REMOVE_SCENARIO: u32 = 0x1804;
pub const EE_SCREENS_CE_MOVE_UP: u32 = 0x1805;
pub const EE_SCREENS_CE_MOVE_DOWN: u32 = 0x1806;
pub const EE_SCREENS_CE_SAVE: u32 = 0x1807;
/// The save posted by [`EE_SCREENS_CE_SAVE`] (it runs on form 24: the original's event held the form).
pub const EE_SCREENS_CE_SAVE_CONTINUED: u32 = 0x1808;
pub const EE_SCREENS_CE_SELECTED: u32 = 0x1809;
/// The password box's Enter and the password page's OK.
pub const EE_SCREENS_CE_PASSWORD: u32 = 0x180a;
pub const EE_SCREENS_CE_DELETE: u32 = 0x180b;
pub const EE_SCREENS_CE_NEW: u32 = 0x180c;
pub const EE_SCREENS_CE_MAIN_SCREEN: u32 = 0x180d;
pub const EE_SCREENS_CE_EDITOR: u32 = 0x180e;
pub const EE_SCREENS_CE_NAME_CHANGED: u32 = 0x180f;
/// The questions' answers (the host's callbacks, on the question's form).
pub const EE_SCREENS_CE_SAVE_BEFORE_OPEN: u32 = 0x1810;
pub const EE_SCREENS_CE_DISCARD_BEFORE_OPEN: u32 = 0x1811;
pub const EE_SCREENS_CE_SAVE_BEFORE_NEW: u32 = 0x1812;
pub const EE_SCREENS_CE_DISCARD_BEFORE_NEW: u32 = 0x1813;
pub const EE_SCREENS_CE_SAVE_BEFORE_MAIN: u32 = 0x1814;
pub const EE_SCREENS_CE_SHOW_MAIN: u32 = 0x1815;
pub const EE_SCREENS_CE_SAVE_BEFORE_EDITOR: u32 = 0x1816;
pub const EE_SCREENS_CE_SHOW_EDITOR: u32 = 0x1817;
pub const EE_SCREENS_CE_DELETE_YES: u32 = 0x1818;

pub fn is_campaign_editor_event(event: u32) -> bool {
    (EE_SCREENS_CE_OPEN..=EE_SCREENS_CE_DELETE_YES).contains(&event)
}

/// The control event ids (`dbuicontrolevents` 190..202, `EECreateCampaignEditorEvent`) to their codes; 0 for another.
pub fn event_for_control(id: core::ffi::c_ulong) -> u32 {
    match id {
        190 => EE_SCREENS_CE_ADD_SCENARIO,
        191 => EE_SCREENS_CE_DELETE,
        192 => EE_SCREENS_CE_NAME_CHANGED,
        193 => EE_SCREENS_CE_SAVE,
        194 => EE_SCREENS_CE_SELECTED,
        195 => EE_SCREENS_CE_MOVE_UP,
        196 => EE_SCREENS_CE_MOVE_DOWN,
        197 => EE_SCREENS_CE_REMOVE_SCENARIO,
        198 => EE_SCREENS_CE_MAIN_SCREEN,
        199 => EE_SCREENS_CE_EDITOR,
        200 | 201 => EE_SCREENS_CE_PASSWORD,
        202 => EE_SCREENS_CE_NEW,
        _ => 0,
    }
}

// ---- the host's ops (ee_screenscpp_call on the event's form)

/// (control, kind: [`EE_SCREENS_CE_IS_EDIT`], [`EE_SCREENS_CE_IS_LABEL`], [`EE_SCREENS_CE_IS_LIST`], [`EE_SCREENS_CE_IS_COMBO`]) -> the control is of that class
pub const EE_SCREENS_OP_CE_IS: u32 = 0x320;
/// The background combo box's set-up (the scenarios' `.jpg`/`.JPG` files, a "none" line selected)
pub const EE_SCREENS_GAME_CE_OPEN: u32 = 0x321;
pub const EE_SCREENS_GAME_CE_RESET: u32 = 0x322; // gCampaignEditor.Reset()
pub const EE_SCREENS_GAME_CE_NEW: u32 = 0x323; // -> gCampaignEditor.New()
pub const EE_SCREENS_GAME_CE_UNLOCKED: u32 = 0x324; // -> GetIsUnlocked()
pub const EE_SCREENS_GAME_CE_PASSWORD_ENTERED: u32 = 0x325; // -> GetPasswordEntered()
pub const EE_SCREENS_GAME_CE_SAVE: u32 = 0x326; // -> Save(the error)
/// (control, line, scroll) a list box's ChangeSelectionIndex(line, false, scroll)
pub const EE_SCREENS_OP_CE_LIST_SELECT: u32 = 0x327;
pub const EE_SCREENS_OP_CE_CLEAR_SELECTION: u32 = 0x328; // (control) a list box's ClearSelection
/// (control) -> a combo box's selection, `u32::MAX` for none
pub const EE_SCREENS_OP_CE_COMBO_SELECTION: u32 = 0x329;
pub const EE_SCREENS_OP_CE_COMBO_COUNT: u32 = 0x32a; // (control) -> its list's line count
/// (control) the combo box's "none" line: the string, CreateEntries(1), InsertLine at 0, selected
pub const EE_SCREENS_OP_CE_BG_NONE: u32 = 0x32b;
pub const EE_SCREENS_GAME_CE_LIST_OK: u32 = 0x32c; // -> the last scenario list's result
/// (control, field) -> the edit box's first line differs from the model's field (`UWideString` `!=`)
pub const EE_SCREENS_GAME_CE_CHANGED: u32 = 0x32d;
/// Texts in (ee_screenscpp_text) and out (ee_screenscpp_put_text).
pub const EE_SCREENS_TEXT_CE_FIELD: u32 = 0x300; // (field) the model's
pub const EE_SCREENS_TEXT_CE_LIST: u32 = 0x301; // (list) the model's, each ended by 0 (the scenario list's result kept)
pub const EE_SCREENS_TEXT_CE_ERROR: u32 = 0x302; // the last Save/Load/Delete/scenario list's error
pub const EE_SCREENS_TEXT_CE_ALL_TEXT: u32 = 0x303; // (control) an edit box's GetAllText
pub const EE_SCREENS_TEXT_CE_SET_FIELD: u32 = 0x304; // put (field) the model's setter
pub const EE_SCREENS_TEXT_CE_ADD: u32 = 0x305; // put -> AddScenario
pub const EE_SCREENS_TEXT_CE_REMOVE: u32 = 0x306; // put -> RemoveScenario
pub const EE_SCREENS_TEXT_CE_MOVE: u32 = 0x307; // put (delta) -> MoveScenario
pub const EE_SCREENS_TEXT_CE_DELETE: u32 = 0x308; // put -> Delete (the error kept)
pub const EE_SCREENS_TEXT_CE_LOAD: u32 = 0x309; // put -> Load (the error kept)
pub const EE_SCREENS_TEXT_CE_QUESTION: u32 = 0x30a; // put (CE_ASK_*) the question on the event's form
pub const EE_SCREENS_TEXT_CE_APPEND: u32 = 0x30b; // put (control) AppendStringArray of the texts, each ended by 0
pub const EE_SCREENS_TEXT_CE_BG_EQUALS: u32 = 0x30c; // put (line) -> the combo box's line EqualsIgnoreCase
pub const EE_SCREENS_TEXT_CE_TRANSLATE: u32 = 0x30d; // put: EETranslateString, kept
pub const EE_SCREENS_TEXT_CE_TRANSLATED: u32 = 0x30e; // the kept translation

/// [`EE_SCREENS_OP_CE_IS`]'s classes.
pub const EE_SCREENS_CE_IS_EDIT: u32 = 0;
pub const EE_SCREENS_CE_IS_LABEL: u32 = 1;
pub const EE_SCREENS_CE_IS_LIST: u32 = 2;
pub const EE_SCREENS_CE_IS_COMBO: u32 = 3;
/// The model's fields ([`EE_SCREENS_TEXT_CE_FIELD`], [`EE_SCREENS_TEXT_CE_SET_FIELD`]).
pub const EE_SCREENS_CE_FIELD_NAME: u32 = 0;
pub const EE_SCREENS_CE_FIELD_AUTHOR: u32 = 1;
pub const EE_SCREENS_CE_FIELD_EMAIL: u32 = 2;
pub const EE_SCREENS_CE_FIELD_BACKGROUND: u32 = 3;
pub const EE_SCREENS_CE_FIELD_CIVILIZATION: u32 = 4;
pub const EE_SCREENS_CE_FIELD_DESCRIPTION: u32 = 5;
pub const EE_SCREENS_CE_FIELD_PASSWORD: u32 = 6;
/// The model's lists ([`EE_SCREENS_TEXT_CE_LIST`]).
pub const EE_SCREENS_CE_LIST_CAMPAIGNS: u32 = 0;
pub const EE_SCREENS_CE_LIST_AVAILABLE: u32 = 1;
pub const EE_SCREENS_CE_LIST_SCENARIOS: u32 = 2;
pub const EE_SCREENS_CE_LIST_SCENARIO_LIST: u32 = 3;
/// The questions ([`EE_SCREENS_TEXT_CE_QUESTION`]): before opening, creating, the main screen, the editor; delete.
pub const EE_SCREENS_CE_ASK_OPEN: u32 = 0;
pub const EE_SCREENS_CE_ASK_NEW: u32 = 1;
pub const EE_SCREENS_CE_ASK_MAIN: u32 = 2;
pub const EE_SCREENS_CE_ASK_EDITOR: u32 = 3;
pub const EE_SCREENS_CE_ASK_DELETE: u32 = 4;

// the form's controls (ee/EEUIManager.h EECampaignEditorControl) and strings (ee/EEStringIDs.h)
const FORM: u32 = 24;
const NAME_EDIT: u32 = 2;
const AUTHOR_EDIT: u32 = 3;
const EMAIL_EDIT: u32 = 7;
const FILE_LIST: u32 = 10;
const DESCRIPTION_EDIT: u32 = 11;
const AVAILABLE_LIST: u32 = 14;
const ASSETS_LIST: u32 = 15;
const AUTHOR_LABEL: u32 = 25;
const EMAIL_LABEL: u32 = 26;
const DESCRIPTION_LABEL: u32 = 27;
const PASSWORD_EDIT: u32 = 35;
const SCENARIOS_LIST: u32 = 40;
const BACKGROUND_COMBO: u32 = 44;
const BACKGROUND_LABEL: u32 = 45;
const CIVILIZATION_EDIT: u32 = 47;
const CIVILIZATION_LABEL: u32 = 48;
const IDS_SAVE_BEFORE_CREATING: u32 = 7155;
const IDS_SAVE_BEFORE_LOADING: u32 = 7161;
const IDS_SAVE_BEFORE_EXITING: u32 = 7162;
const IDS_SELECT_CAMPAIGN_FIRST: u32 = 7208;
const IDS_DELETE_SELECTED_FILE: u32 = 10013;
const IDS_PLEASE_WAIT: u32 = 31842;
const NONE: u32 = u32::MAX;

/// The toolkit's and the model's calls, on the event's form. Texts are the game's UTF-8 bytes.
pub trait CampaignEditorHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to three arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32;
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8>;
    /// A text into the game; the host's answer.
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32;
}

fn is<H: CampaignEditorHost + ?Sized>(h: &mut H, control: u32, kind: u32) -> bool {
    h.call(EE_SCREENS_OP_CE_IS, control, kind, 0) != 0
}
fn field<H: CampaignEditorHost + ?Sized>(h: &mut H, which: u32) -> Vec<u8> {
    h.text(EE_SCREENS_TEXT_CE_FIELD, which)
}
fn set_field<H: CampaignEditorHost + ?Sized>(h: &mut H, which: u32, text: &[u8]) {
    h.put_text(EE_SCREENS_TEXT_CE_SET_FIELD, which, text);
}
fn translate<H: CampaignEditorHost + ?Sized>(h: &mut H, text: &[u8]) -> Vec<u8> {
    h.put_text(EE_SCREENS_TEXT_CE_TRANSLATE, 0, text);
    h.text(EE_SCREENS_TEXT_CE_TRANSLATED, 0)
}
fn message<H: CampaignEditorHost + ?Sized>(h: &mut H, text: &[u8]) {
    h.put_text(EE_SCREENS_TEXT_MESSAGE, 1, text);
}
fn error<H: CampaignEditorHost + ?Sized>(h: &mut H) -> Vec<u8> {
    h.text(EE_SCREENS_TEXT_CE_ERROR, 0)
}
/// A list box's selection (`GetCurrentSelection` is `kErrorOK`).
fn selection<H: CampaignEditorHost + ?Sized>(h: &mut H, control: u32) -> Option<u32> {
    let s = h.call(EE_SCREENS_OP_FORM_LIST_SELECTED, 0, control, 0);
    (s != NONE).then_some(s)
}
fn list_line<H: CampaignEditorHost + ?Sized>(h: &mut H, control: u32, line: u32) -> Vec<u8> {
    h.text(EE_SCREENS_TEXT_LIST_DATA, control << 16 | (line & 0xffff))
}
fn select<H: CampaignEditorHost + ?Sized>(h: &mut H, control: u32, line: u32, scroll: bool) {
    h.call(EE_SCREENS_OP_CE_LIST_SELECT, control, line, scroll as u32);
}

/// An edit box's first line, or with `all` its whole text; `None` when the control is not an edit box.
fn read_edit<H: CampaignEditorHost + ?Sized>(h: &mut H, control: u32, all: bool) -> Option<Vec<u8>> {
    if !is(h, control, EE_SCREENS_CE_IS_EDIT) {
        return None;
    }
    Some(if all { h.text(EE_SCREENS_TEXT_CE_ALL_TEXT, control) } else { h.text(EE_SCREENS_TEXT_EDIT, control) })
}

/// `ReadAuthor`: the background combo box's selected file (empty for none); `None` when it is not one.
fn read_background<H: CampaignEditorHost + ?Sized>(h: &mut H) -> Option<Vec<u8>> {
    if !is(h, BACKGROUND_COMBO, EE_SCREENS_CE_IS_COMBO) {
        return None;
    }
    let s = h.call(EE_SCREENS_OP_CE_COMBO_SELECTION, BACKGROUND_COMBO, 0, 0);
    Some(if s != NONE { h.text(EE_SCREENS_TEXT_COMBO_DATA, BACKGROUND_COMBO << 16 | (s & 0xffff)) } else { Vec::new() })
}

/// `ReadForm`: the form's fields into the model in the original's order, stopping at the first missing control.
fn read_form<H: CampaignEditorHost + ?Sized>(h: &mut H) -> bool {
    let reads: [(u32, bool, u32); 6] = [
        (NAME_EDIT, false, EE_SCREENS_CE_FIELD_NAME),
        (DESCRIPTION_EDIT, true, EE_SCREENS_CE_FIELD_DESCRIPTION),
        (AUTHOR_EDIT, false, EE_SCREENS_CE_FIELD_AUTHOR),
        (EMAIL_EDIT, false, EE_SCREENS_CE_FIELD_EMAIL),
        (PASSWORD_EDIT, true, EE_SCREENS_CE_FIELD_PASSWORD),
        (CIVILIZATION_EDIT, true, EE_SCREENS_CE_FIELD_CIVILIZATION),
    ];
    for (control, all, which) in reads {
        match read_edit(h, control, all) {
            Some(text) => set_field(h, which, &text),
            None => return false,
        }
    }
    match read_background(h) {
        Some(text) => {
            set_field(h, EE_SCREENS_CE_FIELD_BACKGROUND, &text);
            true
        }
        None => false,
    }
}

/// A text into an edit box and its read-only label, each if it is one.
fn show_edit_and_label<H: CampaignEditorHost + ?Sized>(h: &mut H, edit: u32, label: u32, text: &[u8]) {
    if is(h, edit, EE_SCREENS_CE_IS_EDIT) {
        h.put_text(EE_SCREENS_TEXT_EDIT, edit, text);
    }
    if is(h, label, EE_SCREENS_CE_IS_LABEL) {
        h.put_text(EE_SCREENS_TEXT_LABEL, label, text);
    }
}

fn show_password<H: CampaignEditorHost + ?Sized>(h: &mut H, text: &[u8]) {
    if is(h, PASSWORD_EDIT, EE_SCREENS_CE_IS_EDIT) {
        h.put_text(EE_SCREENS_TEXT_EDIT, PASSWORD_EDIT, text);
    }
}

/// `ShowAuthor`: the background selected by name (case-insensitive), else a new "none" line; its label.
fn show_background<H: CampaignEditorHost + ?Sized>(h: &mut H, text: &[u8]) {
    let mut found = false;
    if is(h, BACKGROUND_COMBO, EE_SCREENS_CE_IS_COMBO) {
        let mut i = 0;
        while i < h.call(EE_SCREENS_OP_CE_COMBO_COUNT, BACKGROUND_COMBO, 0, 0) {
            if h.put_text(EE_SCREENS_TEXT_CE_BG_EQUALS, i, text) != 0 {
                found = true;
                h.call(EE_SCREENS_OP_COMBO_SELECT, BACKGROUND_COMBO, i, 0);
                break;
            }
            i += 1;
        }
        if !found {
            h.call(EE_SCREENS_OP_CE_BG_NONE, BACKGROUND_COMBO, 0, 0);
        }
    }
    if is(h, BACKGROUND_LABEL, EE_SCREENS_CE_IS_LABEL) {
        h.put_text(EE_SCREENS_TEXT_LABEL, BACKGROUND_LABEL, if found { text } else { b"" });
    }
}

/// `ShowCampaigns`, `ShowAvailableScenarios`, `ShowCampaignScenarios`: a list box refilled from the model, its
/// selected line index kept.
fn show_list<H: CampaignEditorHost + ?Sized>(h: &mut H, list: u32, control: u32) {
    let lines = h.text(EE_SCREENS_TEXT_CE_LIST, list);
    if !is(h, control, EE_SCREENS_CE_IS_LIST) {
        return;
    }
    let kept = selection(h, control);
    h.call(EE_SCREENS_OP_LIST_CLEAR, control, 0, 0);
    h.put_text(EE_SCREENS_TEXT_CE_APPEND, control, &lines);
    if let Some(line) = kept {
        select(h, control, line, true);
    }
}

/// `ShowScenarioList`: the files the campaign uses, and the model's error when it reports one.
fn show_scenario_list<H: CampaignEditorHost + ?Sized>(h: &mut H) {
    let lines = h.text(EE_SCREENS_TEXT_CE_LIST, EE_SCREENS_CE_LIST_SCENARIO_LIST);
    let ok = h.call(EE_SCREENS_GAME_CE_LIST_OK, 0, 0, 0) != 0;
    if is(h, ASSETS_LIST, EE_SCREENS_CE_IS_LIST) {
        h.call(EE_SCREENS_OP_LIST_CLEAR, ASSETS_LIST, 0, 0);
        h.put_text(EE_SCREENS_TEXT_CE_APPEND, ASSETS_LIST, &lines);
    }
    if !ok {
        let text = error(h);
        message(h, &text);
    }
}

fn show_scenario_lists<H: CampaignEditorHost + ?Sized>(h: &mut H) {
    show_list(h, EE_SCREENS_CE_LIST_AVAILABLE, AVAILABLE_LIST);
    show_list(h, EE_SCREENS_CE_LIST_SCENARIOS, SCENARIOS_LIST);
    show_scenario_list(h);
}

/// `RefreshForm`: the model's fields and every list, then the page for its lock state.
fn refresh<H: CampaignEditorHost + ?Sized>(h: &mut H) {
    let name = field(h, EE_SCREENS_CE_FIELD_NAME);
    if is(h, NAME_EDIT, EE_SCREENS_CE_IS_EDIT) {
        let text = translate(h, &name);
        h.put_text(EE_SCREENS_TEXT_EDIT, NAME_EDIT, &text);
    }
    let author = field(h, EE_SCREENS_CE_FIELD_AUTHOR);
    show_edit_and_label(h, AUTHOR_EDIT, AUTHOR_LABEL, &author);
    let email = field(h, EE_SCREENS_CE_FIELD_EMAIL);
    show_edit_and_label(h, EMAIL_EDIT, EMAIL_LABEL, &email);
    let background = field(h, EE_SCREENS_CE_FIELD_BACKGROUND);
    show_background(h, &background);
    let civilization = field(h, EE_SCREENS_CE_FIELD_CIVILIZATION);
    let text = translate(h, &civilization);
    show_edit_and_label(h, CIVILIZATION_EDIT, CIVILIZATION_LABEL, &text);
    let description = field(h, EE_SCREENS_CE_FIELD_DESCRIPTION);
    let text = translate(h, &description);
    show_edit_and_label(h, DESCRIPTION_EDIT, DESCRIPTION_LABEL, &text);
    show_list(h, EE_SCREENS_CE_LIST_CAMPAIGNS, FILE_LIST);
    show_scenario_lists(h);
    // the editing page, or the password page for a locked campaign
    h.call(EE_SCREENS_OP_GROUP, 2, 0, 0);
    h.call(EE_SCREENS_OP_GROUP, 1, 1, 0);
    if h.call(EE_SCREENS_GAME_CE_UNLOCKED, 0, 0, 0) == 0 {
        h.call(EE_SCREENS_OP_GROUP, 1, 0, 0);
        h.call(EE_SCREENS_OP_GROUP, 2, 1, 0);
    }
}

/// `CheckUnsavedChanges`: an unlocked campaign with an entered password or a changed name, author, email or password.
fn unsaved<H: CampaignEditorHost + ?Sized>(h: &mut H) -> bool {
    h.call(EE_SCREENS_GAME_CE_UNLOCKED, 0, 0, 0) != 0
        && (h.call(EE_SCREENS_GAME_CE_PASSWORD_ENTERED, 0, 0, 0) != 0
            || [
                (NAME_EDIT, EE_SCREENS_CE_FIELD_NAME),
                (AUTHOR_EDIT, EE_SCREENS_CE_FIELD_AUTHOR),
                (EMAIL_EDIT, EE_SCREENS_CE_FIELD_EMAIL),
                (PASSWORD_EDIT, EE_SCREENS_CE_FIELD_PASSWORD),
            ]
            .iter()
            .any(|&(control, which)| h.call(EE_SCREENS_GAME_CE_CHANGED, control, which, 0) != 0))
}

/// `NewCampaign`: the model reset to a new campaign, the file selection cleared and the form refreshed.
fn new_campaign<H: CampaignEditorHost + ?Sized>(h: &mut H) -> c_long {
    let mut r = h.call(EE_SCREENS_GAME_CE_NEW, 0, 0, 0) as i32 as c_long;
    if r == 0 {
        if is(h, FILE_LIST, EE_SCREENS_CE_IS_LIST) {
            h.call(EE_SCREENS_OP_CE_CLEAR_SELECTION, FILE_LIST, 0, 0);
            show_password(h, b"");
            refresh(h);
        } else {
            r = 3;
        }
    }
    r
}

/// `GetSelectedCampaign`: the selected campaign's file name (with the campaign extension).
fn selected_campaign<H: CampaignEditorHost + ?Sized>(h: &mut H) -> Option<Vec<u8>> {
    if !is(h, FILE_LIST, EE_SCREENS_CE_IS_LIST) {
        return None;
    }
    let line = selection(h, FILE_LIST)?;
    let mut name = list_line(h, FILE_LIST, line);
    // ((const char*) of the line: up to a 0 byte)
    if let Some(end) = name.iter().position(|&b| b == 0) {
        name.truncate(end);
    }
    name.extend(h.text(EE_SCREENS_TEXT_CAMPAIGN_EXTENSION, 0));
    Some(name)
}

/// The selected line of a scenario list; the available list's must not be empty.
fn selected_scenario<H: CampaignEditorHost + ?Sized>(h: &mut H, control: u32) -> Option<Vec<u8>> {
    if !is(h, control, EE_SCREENS_CE_IS_LIST) {
        return None;
    }
    let line = selection(h, control)?;
    let name = list_line(h, control, line);
    (control != AVAILABLE_LIST || !name.is_empty()).then_some(name)
}

/// `OpenCampaign`: the selected campaign loaded and shown; a load error shown and a new campaign started. 3 when
/// none is selected.
fn open_campaign<H: CampaignEditorHost + ?Sized>(h: &mut H) -> c_long {
    let Some(name) = selected_campaign(h) else {
        return 3;
    };
    if h.put_text(EE_SCREENS_TEXT_CE_LOAD, 0, &name) != 0 {
        show_password(h, b"");
        refresh(h);
        0
    } else {
        let text = error(h);
        message(h, &text);
        new_campaign(h);
        3
    }
}

/// `SaveCampaignNow`: the form read and the campaign saved at once (the questions' saves); 0 when saved.
fn save_now<H: CampaignEditorHost + ?Sized>(h: &mut H) -> c_long {
    if !read_form(h) {
        refresh(h);
        return 1;
    }
    if h.call(EE_SCREENS_GAME_CE_SAVE, 0, 0, 0) == 0 {
        let text = error(h);
        message(h, &text);
        return 1;
    }
    show_list(h, EE_SCREENS_CE_LIST_CAMPAIGNS, FILE_LIST);
    0
}

/// `MoveSelection`: the campaign's scenario list's selection moved (not above the top).
fn move_selection<H: CampaignEditorHost + ?Sized>(h: &mut H, delta: i32) -> bool {
    if !is(h, SCENARIOS_LIST, EE_SCREENS_CE_IS_LIST) {
        return false;
    }
    let Some(line) = selection(h, SCENARIOS_LIST) else {
        return false;
    };
    // (long)(inDelta + unsigned long): 32-bit, wrapping
    let index = (line as i32).wrapping_add(delta);
    if index < 0 {
        return false;
    }
    select(h, SCENARIOS_LIST, index as u32, true);
    true
}

fn ask<H: CampaignEditorHost + ?Sized>(h: &mut H, string: u32, which: u32) {
    let text = h.text(EE_SCREENS_TEXT_STRING, string);
    h.put_text(EE_SCREENS_TEXT_CE_QUESTION, which, &text);
}

fn show_editor<H: CampaignEditorHost + ?Sized>(h: &mut H) {
    h.call(EE_SCREENS_GAME_CREATE_EDITOR, 0, 0, 0);
    h.call(EE_SCREENS_GAME_START_EDITOR, 0, 0, 0);
}

fn close_form<H: CampaignEditorHost + ?Sized>(h: &mut H) {
    h.call(EE_SCREENS_OP_CLOSE_FORM, 0, 0, 0);
}

/// One of the editor's events; `None` for another form's, else its `ProcessEvent` result (a callback's is 0).
pub fn dispatch<H: CampaignEditorHost + ?Sized>(h: &mut H, event: u32) -> Option<c_long> {
    if !is_campaign_editor_event(event) {
        return None;
    }
    Some(match event {
        EE_SCREENS_CE_OPEN => {
            h.call(EE_SCREENS_GAME_CE_OPEN, BACKGROUND_COMBO, 0, 0);
            h.call(EE_SCREENS_GAME_CE_RESET, 0, 0, 0);
            let r = new_campaign(h);
            refresh(h);
            r
        }
        EE_SCREENS_CE_ADD_SCENARIO | EE_SCREENS_CE_REMOVE_SCENARIO => {
            let (control, op) = if event == EE_SCREENS_CE_ADD_SCENARIO {
                (AVAILABLE_LIST, EE_SCREENS_TEXT_CE_ADD)
            } else {
                (SCENARIOS_LIST, EE_SCREENS_TEXT_CE_REMOVE)
            };
            match selected_scenario(h, control) {
                Some(name) if h.put_text(op, 0, &name) != 0 => {
                    show_scenario_lists(h);
                    0
                }
                _ => 1,
            }
        }
        EE_SCREENS_CE_MOVE_UP | EE_SCREENS_CE_MOVE_DOWN => {
            let delta = if event == EE_SCREENS_CE_MOVE_UP { -1 } else { 1 };
            match selected_scenario(h, SCENARIOS_LIST) {
                Some(name)
                    if h.put_text(EE_SCREENS_TEXT_CE_MOVE, delta as u32, &name) != 0 && move_selection(h, delta) =>
                {
                    refresh(h);
                    0
                }
                _ => 1,
            }
        }
        EE_SCREENS_CE_SAVE => {
            // Please wait, and the save posted (it runs after the message shows)
            let text = h.text(EE_SCREENS_TEXT_STRING, IDS_PLEASE_WAIT);
            h.put_text(EE_SCREENS_TEXT_MESSAGE, 0, &text);
            h.call(EE_SCREENS_OP_POST_EVENT, EE_SCREENS_CE_SAVE_CONTINUED, 0, 0);
            0
        }
        EE_SCREENS_CE_SAVE_CONTINUED => {
            if read_form(h) {
                if h.call(EE_SCREENS_GAME_CE_SAVE, 0, 0, 0) == 0 {
                    // the error's message closes the wait message
                    let text = error(h);
                    h.put_text(EE_SCREENS_TEXT_MESSAGE_NONE_FOUND, 0, &text);
                    return Some(0);
                }
                show_list(h, EE_SCREENS_CE_LIST_CAMPAIGNS, FILE_LIST);
            }
            refresh(h);
            close_form(h);
            0
        }
        EE_SCREENS_CE_SELECTED => {
            if unsaved(h) {
                ask(h, IDS_SAVE_BEFORE_LOADING, EE_SCREENS_CE_ASK_OPEN);
                0
            } else {
                open_campaign(h)
            }
        }
        EE_SCREENS_CE_PASSWORD => {
            if let Some(password) = read_edit(h, PASSWORD_EDIT, true) {
                read_form(h);
                set_field(h, EE_SCREENS_CE_FIELD_PASSWORD, &password);
                refresh(h);
            }
            3
        }
        EE_SCREENS_CE_DELETE => {
            if selected_campaign(h).is_some() {
                ask(h, IDS_DELETE_SELECTED_FILE, EE_SCREENS_CE_ASK_DELETE);
            } else {
                let text = h.text(EE_SCREENS_TEXT_STRING, IDS_SELECT_CAMPAIGN_FIRST);
                message(h, &text);
            }
            0
        }
        EE_SCREENS_CE_NEW => {
            if unsaved(h) {
                ask(h, IDS_SAVE_BEFORE_CREATING, EE_SCREENS_CE_ASK_NEW);
                0
            } else {
                new_campaign(h)
            }
        }
        EE_SCREENS_CE_MAIN_SCREEN => {
            if unsaved(h) {
                ask(h, IDS_SAVE_BEFORE_EXITING, EE_SCREENS_CE_ASK_MAIN);
            } else {
                close_form(h);
            }
            0
        }
        EE_SCREENS_CE_EDITOR => {
            if unsaved(h) {
                ask(h, IDS_SAVE_BEFORE_EXITING, EE_SCREENS_CE_ASK_EDITOR);
            } else {
                show_editor(h);
            }
            0
        }
        EE_SCREENS_CE_SAVE_BEFORE_OPEN => {
            // a save that adds a file to the list moves the selection down a line first
            let count = h.call(EE_SCREENS_OP_FORM_LIST_COUNT, FORM, FILE_LIST, 0);
            if save_now(h) == 0 {
                if count < h.call(EE_SCREENS_OP_FORM_LIST_COUNT, FORM, FILE_LIST, 0) {
                    if let Some(line) = selection(h, FILE_LIST) {
                        select(h, FILE_LIST, line.wrapping_add(1), false);
                    }
                }
                open_campaign(h);
            }
            0
        }
        EE_SCREENS_CE_DISCARD_BEFORE_OPEN => {
            open_campaign(h);
            0
        }
        EE_SCREENS_CE_SAVE_BEFORE_NEW => {
            if save_now(h) == 0 {
                new_campaign(h);
            }
            0
        }
        EE_SCREENS_CE_DISCARD_BEFORE_NEW => {
            new_campaign(h);
            0
        }
        EE_SCREENS_CE_SAVE_BEFORE_MAIN => {
            if save_now(h) == 0 {
                close_form(h);
            }
            0
        }
        EE_SCREENS_CE_SHOW_MAIN => {
            close_form(h);
            0
        }
        EE_SCREENS_CE_SAVE_BEFORE_EDITOR => {
            if save_now(h) == 0 {
                show_editor(h);
            }
            0
        }
        EE_SCREENS_CE_SHOW_EDITOR => {
            show_editor(h);
            0
        }
        EE_SCREENS_CE_DELETE_YES => {
            // the selected file deleted, the next selection opened (or a new campaign); the error shown
            // (the error is the delete's, empty when nothing was selected)
            let deleted = selected_campaign(h).map(|name| h.put_text(EE_SCREENS_TEXT_CE_DELETE, 0, &name) != 0);
            if deleted == Some(true) {
                refresh(h);
                if open_campaign(h) == 3 {
                    h.call(EE_SCREENS_GAME_CE_NEW, 0, 0, 0);
                    refresh(h);
                }
            } else {
                let text = if deleted.is_some() { error(h) } else { Vec::new() };
                message(h, &text);
            }
            0
        }
        // the close event and the name's change
        _ => 0,
    })
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn events() {
        assert!(is_campaign_editor_event(EE_SCREENS_CE_OPEN) && is_campaign_editor_event(EE_SCREENS_CE_DELETE_YES));
        assert!(!is_campaign_editor_event(0x1800) && !is_campaign_editor_event(0x1819));
        assert_eq!(event_for_control(189), 0);
        assert_eq!(event_for_control(200), EE_SCREENS_CE_PASSWORD);
        assert_eq!(event_for_control(201), EE_SCREENS_CE_PASSWORD);
        assert_eq!(event_for_control(203), 0);
    }
}
