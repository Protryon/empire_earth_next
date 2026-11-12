//! The load and save screens whole: the in-game save screen (form 3) and load screen (form 4), the single player
//! load screen (form 13) and the scenario load screen (form 20); the original `EEUIESaveLoadEvents.cpp`,
//! `EEUIESinglePlayerLoadEvents.cpp` and `EEUIEScenarioLoadEvents.cpp` (and `EEUIESPLECancel` of `EEUIManager.cpp`)
//! over a host ([`SaveLoadHost`]) whose methods are each one toolkit or game call (`UI Events/EEScreens.cpp`).
//!
//! The original events were parametric (a file extension and a directory, given by their creators); each pair they
//! were ever made with is a code of its own here (the saved games' or the scenarios'). The one state is the save
//! screen's "saved" flag (a save was handed on: the button is ignored until the screen opens again).

use std::sync::Mutex;

// ---- the events (EEScreenEvent::mEvent)

/// The save form's activate event (clears the saved flag).
pub const EE_SCREENS_SL_ACTIVATE: u32 = 0x0301;
/// The save form's Save (its button, Enter in the name box, a double click in the list): a saved game (the saved
/// games' extension, no directory: the message names the file) or a scenario (the editor's: `data\Scenarios\`).
pub const EE_SCREENS_SL_SAVE_GAME: u32 = 0x0302;
pub const EE_SCREENS_SL_SAVE_SCENARIO: u32 = 0x0303;
/// The save screen's opening (`EEUIESLSaveScreen`, posted by the in-game options and the editor): the saved games of
/// `data\Saved Games\` or the scenarios of `data\Scenarios\`.
pub const EE_SCREENS_SL_SAVE_SCREEN_GAME: u32 = 0x0304;
pub const EE_SCREENS_SL_SAVE_SCREEN_SCENARIO: u32 = 0x0305;
/// The save list's focused file into the name box.
pub const EE_SCREENS_SL_DISPLAY_NEW_FILE: u32 = 0x0306;
/// The save form's Cancel.
pub const EE_SCREENS_SL_CANCEL_SAVE: u32 = 0x0307;
/// Delete on the save or load form (their list, control 4): the question, the selected file deleted on yes.
pub const EE_SCREENS_SL_DELETE: u32 = 0x0308;
/// The save handed on (the overwrite question's yes and the direct path: the host's callbacks).
pub const EE_SCREENS_SL_SAVED: u32 = 0x0309;
/// The load form's Load (its button, a double click): a saved game of `Saved Games\` or a scenario of `Scenarios\`.
pub const EE_SCREENS_SL_LOAD_GAME: u32 = 0x0401;
pub const EE_SCREENS_SL_LOAD_SCENARIO: u32 = 0x0402;
/// The load screen's opening (`EEUIESLLoadScreen`): saved games or scenarios as the save screen's.
pub const EE_SCREENS_SL_LOAD_SCREEN_GAME: u32 = 0x0403;
pub const EE_SCREENS_SL_LOAD_SCREEN_SCENARIO: u32 = 0x0404;
/// The single player load screen: its open event (posts the fill), the fill, Load, Cancel, Delete.
pub const EE_SCREENS_SPL_OPEN: u32 = 0x0d01;
pub const EE_SCREENS_SPL_LOAD_SCREEN: u32 = 0x0d02;
pub const EE_SCREENS_SPL_LOAD: u32 = 0x0d03;
pub const EE_SCREENS_SPL_CANCEL: u32 = 0x0d04;
pub const EE_SCREENS_SPL_DELETE: u32 = 0x0d05;
/// The scenario load screen: its open event, the fill, Load (into the scenario intro), Cancel, Delete.
pub const EE_SCREENS_SCL_OPEN: u32 = 0x1401;
pub const EE_SCREENS_SCL_LOAD_SCREEN: u32 = 0x1402;
pub const EE_SCREENS_SCL_LOAD: u32 = 0x1403;
pub const EE_SCREENS_SCL_CANCEL: u32 = 0x1404;
pub const EE_SCREENS_SCL_DELETE: u32 = 0x1405;

// ---- the host's ops (ee_screenscpp_call; menus.rs, settings_form.rs and custom_campaign.rs have the shared ones)

pub const EE_SCREENS_OP_SWAP_FORM: u32 = 0x170; // (form) the manager's SwapForm
pub const EE_SCREENS_OP_CURRENT_FORM_IS: u32 = 0x171; // (form) -> the manager's current form is it
pub const EE_SCREENS_OP_POST_EVENT: u32 = 0x172; // (code) gUserInterface->PostEvent(the code's event, NULL)
pub const EE_SCREENS_OP_FORM_LIST_SELECTED: u32 = 0x173; // (form + 1 or 0: the event's, control) -> the selection, MAX: none
pub const EE_SCREENS_GAME_FREE_CAMERA: u32 = 0x174; // -> gFreeCamera
pub const EE_SCREENS_GAME_MULTIPLAYER_SESSION: u32 = 0x175; // -> gCommunications->GetIsMultiplayerSession()
pub const EE_SCREENS_GAME_SCREEN_ACTIVE: u32 = 0x176; // -> gGameScreenActive
pub const EE_SCREENS_GAME_IN_EDITOR: u32 = 0x177; // -> the manager's mInScenarioEditor
pub const EE_SCREENS_GAME_PAUSE_SERVER: u32 = 0x178; // the local server, when running: PauseGame, the pause posted
pub const EE_SCREENS_GAME_CTS_PROCESSED: u32 = 0x179; // -> gCommunications->GetNumberOfCTsProcessedInPhysics()
pub const EE_SCREENS_GAME_SAVE_SEND: u32 = 0x17a; // the pending save message to every player (then SL_SAVED)
pub const EE_SCREENS_GAME_SAVE_POST: u32 = 0x17b; // the pending save action posted (then SL_SAVED)
pub const EE_SCREENS_GAME_SAVED_MAIN_FORM: u32 = 0x17c; // -> gSavedMainGameForm is set
pub const EE_SCREENS_GAME_OPEN_SAVED_MAIN_FORM: u32 = 0x17d; // OpenForm(gSavedMainGameForm)
/// Texts (ee_screenscpp_text / _put_text); `a\0b` joins a put's two texts.
pub const EE_SCREENS_TEXT_SAVED_EXTENSION: u32 = 0x40; // gSavedGameExtension
pub const EE_SCREENS_TEXT_SCENARIO_EXTENSION: u32 = 0x41; // gScenarioExtension
pub const EE_SCREENS_TEXT_LOAD_SAVED_NAME: u32 = 0x42; // gLoadSavedGameName; put: the settings' SetLoadSavedGameName
pub const EE_SCREENS_TEXT_LOAD_SCENARIO_NAME: u32 = 0x43; // gLoadScenarioName; put: SetLoadScenarioName
pub const EE_SCREENS_TEXT_FILE_LIST: u32 = 0x44; // put (form << 16 | control, extension\0directory\0name): the fill
pub const EE_SCREENS_TEXT_QUESTION_DELETE: u32 = 0x45; // put (list control of the event's form): the delete question
pub const EE_SCREENS_TEXT_LOAD_POST: u32 = 0x46; // put (file\0directory): EEOpenSaveGameFile, PostLoadSavedGame -> 1
pub const EE_SCREENS_TEXT_SAVE_MESSAGE: u32 = 0x47; // put (file): the pending save, an EEMLoadOrSaveGame
pub const EE_SCREENS_TEXT_SAVE_ACTION: u32 = 0x48; // put (file\0directory): the pending save, an EEASaveGame
pub const EE_SCREENS_TEXT_FILE_EXISTS: u32 = 0x49; // put (file\0directory below the startup one) -> 1: it opens
pub const EE_SCREENS_TEXT_SAVE_QUESTION: u32 = 0x4a; // put (0 the message, 1 the action): the overwrite question
pub const EE_SCREENS_TEXT_LOAD_SAVED_GAME: u32 = 0x4b; // put (file\0directory): the single player load (or why not)
pub const EE_SCREENS_TEXT_SCENARIO_INTRO: u32 = 0x4c; // put (file\0directory) -> 2: the intro form manager set up
pub const EE_SCREENS_TEXT_FORM_EDIT: u32 = 0x4d; // put (form << 16 | control): an edit box's SetText

// the forms, controls and strings (ee/EEUIManager.h, ee/EEStringIDs.h)
const SL_FORM_SAVE: u32 = 3;
const SL_FORM_LOAD: u32 = 4;
const SL_FORM_SP_LOAD: u32 = 13;
const SL_FORM_INTRO: u32 = 19;
const SL_FORM_SCENARIO_LOAD: u32 = 20;
const SL_FORM_GAME_OPTIONS: u32 = 28;
const SL_LIST: u32 = 4; // the save, load and single player load forms' file list
const SL_SCENARIO_LIST: u32 = 3; // the scenario load form's
const SL_NAME_BOX: u32 = 5; // the save form's name edit box
const SL_IDS_SAVE_GAME_ISNT_VALID: u32 = 7153;
const SL_IDS_DELETE_SELECTED_FILE: u32 = 10013;
const SL_IDS_YOU_MUST_ENTER_FILE_NAME: u32 = 10015;
const SL_IDS_FILE_ALREADY_EXISTS: u32 = 10051;
const SL_NONE: u32 = u32::MAX;

// the control events (ee/EEControlEventID.h)
const SL_EVENT_LOAD_DELETE: u32 = 11;
const SL_EVENT_SAVE_DELETE: u32 = 12;
const SL_EVENT_LOAD_GAME: u32 = 13;
const SL_EVENT_SAVE_GAME: u32 = 14;
const SL_EVENT_CANCEL_SAVE: u32 = 16;
const SL_EVENT_LOAD_BY_DOUBLE_CLICK: u32 = 17;
const SL_EVENT_SAVE_BY_ENTER: u32 = 18;
const SL_EVENT_SAVE_BY_DOUBLE_CLICK: u32 = 19;
const SL_EVENT_SAVE_FOCUSED_FILE: u32 = 20;
const SL_EVENT_SP_LOAD: u32 = 41;
const SL_EVENT_SP_LOAD_BY_DOUBLE_CLICK: u32 = 42;
const SL_EVENT_SP_CANCEL: u32 = 43;
const SL_EVENT_SP_DELETE: u32 = 44;
const SL_EVENT_SCENARIO_LOAD: u32 = 101;
const SL_EVENT_SCENARIO_LOAD_BY_DOUBLE_CLICK: u32 = 102;
const SL_EVENT_SCENARIO_CANCEL: u32 = 103;
const SL_EVENT_SCENARIO_DELETE: u32 = 104;

/// The screens' state: a save was handed on (`sSaved`).
#[derive(Default)]
pub struct SaveLoad {
    pub saved: bool,
}

/// The toolkit's and the game's calls the load and save screens make. Texts are the game's UTF-8 bytes.
pub trait SaveLoadHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to three arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32;
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8>;
    /// A text into the game; the host's answer.
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32;
    /// A resource string (whatever `GetString` left when it fails).
    fn string(&mut self, id: u32) -> Vec<u8>;
}

/// The save/load form's event (forms 3 and 4) for a control event id; 0 for another (the load form's Cancel,
/// `EEUIECancel` over a form, stays C++'s). Made when the form is created: the screen and the editor then decide
/// the saved games' or the scenarios' kind.
pub fn save_load_event_for(id: u32, screen_active: bool, in_editor: bool) -> u32 {
    match id {
        SL_EVENT_LOAD_DELETE | SL_EVENT_SAVE_DELETE => EE_SCREENS_SL_DELETE,
        SL_EVENT_LOAD_GAME | SL_EVENT_LOAD_BY_DOUBLE_CLICK => {
            if screen_active || !in_editor {
                EE_SCREENS_SL_LOAD_GAME
            } else {
                EE_SCREENS_SL_LOAD_SCENARIO
            }
        }
        SL_EVENT_SAVE_GAME | SL_EVENT_SAVE_BY_ENTER | SL_EVENT_SAVE_BY_DOUBLE_CLICK => {
            if in_editor {
                EE_SCREENS_SL_SAVE_SCENARIO
            } else {
                EE_SCREENS_SL_SAVE_GAME
            }
        }
        SL_EVENT_CANCEL_SAVE => EE_SCREENS_SL_CANCEL_SAVE,
        SL_EVENT_SAVE_FOCUSED_FILE => EE_SCREENS_SL_DISPLAY_NEW_FILE,
        _ => 0,
    }
}

/// The single player load form's event for a control event id; 0 for another.
pub fn sp_load_event_for(id: u32) -> u32 {
    match id {
        SL_EVENT_SP_LOAD | SL_EVENT_SP_LOAD_BY_DOUBLE_CLICK => EE_SCREENS_SPL_LOAD,
        SL_EVENT_SP_CANCEL => EE_SCREENS_SPL_CANCEL,
        SL_EVENT_SP_DELETE => EE_SCREENS_SPL_DELETE,
        _ => 0,
    }
}

/// The scenario load form's event for a control event id; 0 for another.
pub fn scenario_load_event_for(id: u32) -> u32 {
    match id {
        SL_EVENT_SCENARIO_LOAD | SL_EVENT_SCENARIO_LOAD_BY_DOUBLE_CLICK => EE_SCREENS_SCL_LOAD,
        SL_EVENT_SCENARIO_CANCEL => EE_SCREENS_SCL_CANCEL,
        SL_EVENT_SCENARIO_DELETE => EE_SCREENS_SCL_DELETE,
        _ => 0,
    }
}

pub fn is_save_load_event(event: u32) -> bool {
    (EE_SCREENS_SL_ACTIVATE..=EE_SCREENS_SL_SAVED).contains(&event)
        || (EE_SCREENS_SL_LOAD_GAME..=EE_SCREENS_SL_LOAD_SCREEN_SCENARIO).contains(&event)
        || (EE_SCREENS_SPL_OPEN..=EE_SCREENS_SPL_DELETE).contains(&event)
        || (EE_SCREENS_SCL_OPEN..=EE_SCREENS_SCL_DELETE).contains(&event)
}

fn join(parts: &[&[u8]]) -> Vec<u8> {
    parts.join(&0u8)
}

/// The saved games' or the scenarios' extension.
fn extension<H: SaveLoadHost>(host: &mut H, scenario: bool) -> Vec<u8> {
    host.text(if scenario { EE_SCREENS_TEXT_SCENARIO_EXTENSION } else { EE_SCREENS_TEXT_SAVED_EXTENSION }, 0)
}

/// The last loaded name a list preselects: the scenario's in the editor, else the saved game's.
fn last_name<H: SaveLoadHost>(host: &mut H) -> Vec<u8> {
    let in_editor = host.call(EE_SCREENS_GAME_IN_EDITOR, 0, 0, 0) != 0;
    host.text(if in_editor { EE_SCREENS_TEXT_LOAD_SCENARIO_NAME } else { EE_SCREENS_TEXT_LOAD_SAVED_NAME }, 0)
}

/// A file list filled from a directory below the startup one with an extension, `name` selected when listed.
fn fill<H: SaveLoadHost>(host: &mut H, form: u32, control: u32, scenario: bool, name: &[u8]) {
    let extension = extension(host, scenario);
    let directory: &[u8] = if scenario { b"data\\Scenarios\\" } else { b"data\\Saved Games\\" };
    host.put_text(EE_SCREENS_TEXT_FILE_LIST, form << 16 | control, &join(&[&extension, directory, name]));
}

/// The selected line of a list (`form` + 1, or 0: the event's form), none when nothing is selected.
fn selected<H: SaveLoadHost>(host: &mut H, form: u32, control: u32) -> Option<u32> {
    let line = host.call(EE_SCREENS_OP_FORM_LIST_SELECTED, form, control, 0);
    (line != SL_NONE).then_some(line)
}

/// The selected line's text of a list of the event's form.
fn selected_name<H: SaveLoadHost>(host: &mut H, control: u32) -> Option<Vec<u8>> {
    let line = selected(host, 0, control)?;
    Some(host.text(crate::settings_form::EE_SCREENS_TEXT_LIST_DATA, control << 16 | (line & 0xffff)))
}

fn delete<H: SaveLoadHost>(host: &mut H, control: u32) {
    let text = host.string(SL_IDS_DELETE_SELECTED_FILE);
    host.put_text(EE_SCREENS_TEXT_QUESTION_DELETE, control, &text);
}

fn message<H: SaveLoadHost>(host: &mut H, id: u32) {
    let text = host.string(id);
    host.put_text(crate::menus::EE_SCREENS_TEXT_MESSAGE, 1, &text);
}

/// Save under the typed name: in a running game the host's save message (asking before an overwrite), in the
/// editor (or before the game screen) a save action; a name is required; ignored after a save was handed on.
fn save<H: SaveLoadHost>(host: &mut H, state: &Mutex<SaveLoad>, scenario: bool) {
    if state.lock().unwrap().saved {
        return;
    }
    let directory: &[u8] = if scenario { b"data\\Scenarios\\" } else { b"" };
    let name = host.text(crate::menus::EE_SCREENS_TEXT_EDIT, SL_NAME_BOX);
    if name.is_empty() {
        message(host, SL_IDS_YOU_MUST_ENTER_FILE_NAME);
        return;
    }
    let extension = extension(host, scenario);
    let file = [name.as_slice(), &extension].concat();
    if host.call(EE_SCREENS_GAME_IN_EDITOR, 0, 0, 0) == 0 && host.call(EE_SCREENS_GAME_SCREEN_ACTIVE, 0, 0, 0) != 0 {
        host.put_text(EE_SCREENS_TEXT_LOAD_SAVED_NAME, 0, &name);
        if host.call(crate::settings_form::EE_SCREENS_GAME_IS_SERVER, 0, 0, 0) != 0
            && host.call(EE_SCREENS_GAME_CTS_PROCESSED, 0, 0, 0) > 0
        {
            host.put_text(EE_SCREENS_TEXT_SAVE_MESSAGE, 0, &[directory, &file].concat());
            if host.put_text(EE_SCREENS_TEXT_FILE_EXISTS, 0, &join(&[&file, b"data\\Saved Games\\"])) != 0 {
                let text = host.string(SL_IDS_FILE_ALREADY_EXISTS);
                host.put_text(EE_SCREENS_TEXT_SAVE_QUESTION, 0, &text);
            } else {
                host.call(EE_SCREENS_GAME_SAVE_SEND, 0, 0, 0);
            }
        }
    } else {
        host.put_text(EE_SCREENS_TEXT_LOAD_SCENARIO_NAME, 0, &name);
        host.put_text(EE_SCREENS_TEXT_SAVE_ACTION, 0, &join(&[&file, directory]));
        if host.put_text(EE_SCREENS_TEXT_FILE_EXISTS, 0, &join(&[&file, directory])) != 0 {
            let text = host.string(SL_IDS_FILE_ALREADY_EXISTS);
            host.put_text(EE_SCREENS_TEXT_SAVE_QUESTION, 1, &text);
        } else {
            host.call(EE_SCREENS_GAME_SAVE_POST, 0, 0, 0);
        }
    }
}

/// One of the screens' events; false for another code (nothing called). No lock is held across a host call.
pub fn dispatch<H: SaveLoadHost>(host: &mut H, state: &Mutex<SaveLoad>, event: u32) -> bool {
    match event {
        EE_SCREENS_SL_ACTIVATE | EE_SCREENS_SL_SAVED => {
            state.lock().unwrap().saved = event == EE_SCREENS_SL_SAVED;
        }
        EE_SCREENS_SL_DELETE | EE_SCREENS_SPL_DELETE => delete(host, SL_LIST),
        EE_SCREENS_SCL_DELETE => delete(host, SL_SCENARIO_LIST),
        EE_SCREENS_SL_LOAD_GAME | EE_SCREENS_SL_LOAD_SCENARIO => {
            // the selected file posted for loading, its name the last loaded one
            let scenario = event == EE_SCREENS_SL_LOAD_SCENARIO;
            let Some(name) = selected_name(host, SL_LIST) else { return true };
            let extension = extension(host, scenario);
            let directory: &[u8] = if scenario { b"Scenarios\\" } else { b"Saved Games\\" };
            let file = [name.as_slice(), &extension].concat();
            if host.put_text(EE_SCREENS_TEXT_LOAD_POST, 0, &join(&[&file, directory])) != 0 {
                let which = if host.call(EE_SCREENS_GAME_SCREEN_ACTIVE, 0, 0, 0) == 0
                    && host.call(EE_SCREENS_GAME_IN_EDITOR, 0, 0, 0) != 0
                {
                    EE_SCREENS_TEXT_LOAD_SCENARIO_NAME
                } else {
                    EE_SCREENS_TEXT_LOAD_SAVED_NAME
                };
                host.put_text(which, 0, &name);
            }
        }
        EE_SCREENS_SL_SAVE_GAME | EE_SCREENS_SL_SAVE_SCENARIO => {
            save(host, state, event == EE_SCREENS_SL_SAVE_SCENARIO)
        }
        EE_SCREENS_SL_LOAD_SCREEN_GAME | EE_SCREENS_SL_LOAD_SCREEN_SCENARIO => {
            // single player only: over the in-game options, the game paused
            if host.call(EE_SCREENS_GAME_MULTIPLAYER_SESSION, 0, 0, 0) == 0 {
                if host.call(EE_SCREENS_GAME_FREE_CAMERA, 0, 0, 0) == 0
                    && host.call(EE_SCREENS_OP_CURRENT_FORM_IS, SL_FORM_GAME_OPTIONS, 0, 0) != 0
                {
                    host.call(crate::menus::EE_SCREENS_OP_POP, 0, 0, 0);
                }
                host.call(EE_SCREENS_OP_SWAP_FORM, SL_FORM_LOAD, 0, 0);
                host.call(EE_SCREENS_GAME_PAUSE_SERVER, 0, 0, 0);
                let name = last_name(host);
                fill(host, SL_FORM_LOAD, SL_LIST, event == EE_SCREENS_SL_LOAD_SCREEN_SCENARIO, &name);
            }
        }
        EE_SCREENS_SL_SAVE_SCREEN_GAME | EE_SCREENS_SL_SAVE_SCREEN_SCENARIO => {
            if host.call(EE_SCREENS_GAME_FREE_CAMERA, 0, 0, 0) == 0
                && host.call(EE_SCREENS_OP_CURRENT_FORM_IS, SL_FORM_GAME_OPTIONS, 0, 0) != 0
            {
                host.call(EE_SCREENS_OP_SWAP_FORM, SL_FORM_SAVE, 0, 0);
            } else {
                host.call(crate::menus::EE_SCREENS_OP_PUSH, SL_FORM_SAVE, 0, 0);
            }
            host.call(EE_SCREENS_GAME_PAUSE_SERVER, 0, 0, 0);
            let name = last_name(host);
            fill(host, SL_FORM_SAVE, SL_LIST, event == EE_SCREENS_SL_SAVE_SCREEN_SCENARIO, &name);
            state.lock().unwrap().saved = false;
        }
        EE_SCREENS_SL_DISPLAY_NEW_FILE => {
            if let Some(line) = selected(host, SL_FORM_SAVE + 1, SL_LIST) {
                let text = host.text(
                    crate::custom_campaign::EE_SCREENS_TEXT_FORM_LIST_DATA,
                    SL_FORM_SAVE << 24 | SL_LIST << 16 | (line & 0xffff),
                );
                host.put_text(EE_SCREENS_TEXT_FORM_EDIT, SL_FORM_SAVE << 16 | SL_NAME_BOX, &text);
            }
        }
        EE_SCREENS_SL_CANCEL_SAVE => {
            host.call(crate::settings_form::EE_SCREENS_OP_CLOSE_FORM, 0, 0, 0);
        }
        EE_SCREENS_SPL_OPEN => {
            host.call(EE_SCREENS_OP_POST_EVENT, EE_SCREENS_SPL_LOAD_SCREEN, 0, 0);
        }
        EE_SCREENS_SCL_OPEN => {
            host.call(EE_SCREENS_OP_POST_EVENT, EE_SCREENS_SCL_LOAD_SCREEN, 0, 0);
        }
        EE_SCREENS_SPL_LOAD_SCREEN => {
            let name = last_name(host);
            fill(host, SL_FORM_SP_LOAD, SL_LIST, false, &name);
        }
        EE_SCREENS_SCL_LOAD_SCREEN => {
            let name = host.text(EE_SCREENS_TEXT_LOAD_SCENARIO_NAME, 0);
            fill(host, SL_FORM_SCENARIO_LOAD, SL_SCENARIO_LIST, true, &name);
        }
        EE_SCREENS_SPL_LOAD => {
            // the selected saved game validated and loaded in a new local server (the host reports an invalid one)
            let Some(name) = selected_name(host, SL_LIST) else { return true };
            let extension = extension(host, false);
            let file = [name.as_slice(), &extension].concat();
            host.put_text(EE_SCREENS_TEXT_LOAD_SAVED_GAME, 0, &join(&[&file, b"Saved Games\\"]));
        }
        EE_SCREENS_SCL_LOAD => {
            // the selected scenario's header into the scenario intro, which opens; an invalid file reported
            let Some(name) = selected_name(host, SL_SCENARIO_LIST) else { return true };
            let extension = extension(host, true);
            let file = [name.as_slice(), &extension].concat();
            if host.put_text(EE_SCREENS_TEXT_SCENARIO_INTRO, 0, &join(&[&file, b"Scenarios\\"])) == 2 {
                host.call(crate::menus::EE_SCREENS_OP_OPEN_FORM, SL_FORM_INTRO, 0, 0);
            } else {
                message(host, SL_IDS_SAVE_GAME_ISNT_VALID);
            }
        }
        EE_SCREENS_SCL_CANCEL => {
            if host.call(EE_SCREENS_GAME_SAVED_MAIN_FORM, 0, 0, 0) != 0 {
                host.call(EE_SCREENS_GAME_OPEN_SAVED_MAIN_FORM, 0, 0, 0);
            } else {
                host.call(crate::menus::EE_SCREENS_OP_POP, 0, 0, 0);
            }
        }
        EE_SCREENS_SPL_CANCEL => {
            host.call(crate::menus::EE_SCREENS_OP_POP, 0, 0, 0);
        }
        _ => return false,
    }
    true
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn factories() {
        assert_eq!(save_load_event_for(11, false, false), EE_SCREENS_SL_DELETE);
        assert_eq!(save_load_event_for(13, false, true), EE_SCREENS_SL_LOAD_SCENARIO);
        assert_eq!(save_load_event_for(17, true, true), EE_SCREENS_SL_LOAD_GAME);
        assert_eq!(save_load_event_for(19, true, true), EE_SCREENS_SL_SAVE_SCENARIO);
        assert_eq!(save_load_event_for(18, true, false), EE_SCREENS_SL_SAVE_GAME);
        assert_eq!(save_load_event_for(15, false, false), 0); // the load form's Cancel: C++'s
        assert_eq!(sp_load_event_for(42), EE_SCREENS_SPL_LOAD);
        assert_eq!(sp_load_event_for(40), 0);
        assert_eq!(scenario_load_event_for(104), EE_SCREENS_SCL_DELETE);
        assert_eq!(scenario_load_event_for(105), 0);
        for code in [
            EE_SCREENS_SL_ACTIVATE,
            EE_SCREENS_SL_SAVED,
            EE_SCREENS_SL_LOAD_SCREEN_SCENARIO,
            EE_SCREENS_SPL_DELETE,
            EE_SCREENS_SCL_OPEN,
        ] {
            assert!(is_save_load_event(code));
        }
        for code in [0x0300, 0x030a, 0x0405, 0x0d06, 0x1406] {
            assert!(!is_save_load_event(code));
        }
    }
}
