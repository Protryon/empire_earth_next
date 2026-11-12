//! The in-game options menu (form 28) whole: the original `EEUIEMainGameOptionsEvents.cpp` (its 15 event classes and
//! the four question answers) over a host ([`OptionsHost`]) whose methods are each one toolkit or game call
//! (`UI Events/EEScreens.cpp`). The one state here is the Play Scenario button's posted flag: the original's event
//! object (made anew when an opening hooks the button up) posted itself on its first press and loaded on its later runs.

use core::ffi::c_long;
use std::sync::Mutex;

use crate::menus::{EE_SCREENS_OP_CLOSE_EVENT, EE_SCREENS_OP_OPEN_EVENT, EE_SCREENS_OP_OPEN_FORM, EE_SCREENS_OP_SHOW};
use crate::save_load::{
    EE_SCREENS_GAME_CTS_PROCESSED, EE_SCREENS_GAME_IN_EDITOR, EE_SCREENS_GAME_MULTIPLAYER_SESSION,
    EE_SCREENS_OP_POST_EVENT, EE_SCREENS_SL_LOAD_SCREEN_GAME, EE_SCREENS_SL_SAVE_SCREEN_GAME,
    EE_SCREENS_SL_SAVE_SCREEN_SCENARIO,
};
use crate::settings_form::{
    EE_SCREENS_GAME_IS_SERVER, EE_SCREENS_GAME_PAUSE, EE_SCREENS_GS_IN_GAME_OPEN, EE_SCREENS_OP_CLOSE_FORM,
    EE_SCREENS_OP_TRIGGER_HERE,
};

// ---- the events (EEScreenEvent::mEvent): form 28's, the main game form's briefing, the questions' answers

pub const EE_SCREENS_GO_RETURN_TO_GAME: u32 = 0x1c01;
pub const EE_SCREENS_GO_RESIGN: u32 = 0x1c02;
pub const EE_SCREENS_GO_RESTART: u32 = 0x1c03;
pub const EE_SCREENS_GO_RETURN_TO_EDITOR: u32 = 0x1c04;
pub const EE_SCREENS_GO_LOAD_SAVED_GAME: u32 = 0x1c05;
pub const EE_SCREENS_GO_SAVE_GAME: u32 = 0x1c06;
pub const EE_SCREENS_GO_PAUSE: u32 = 0x1c07;
pub const EE_SCREENS_GO_PREVIOUS_MESSAGES: u32 = 0x1c08;
pub const EE_SCREENS_GO_LOAD_SCENARIO: u32 = 0x1c09;
pub const EE_SCREENS_GO_BRIEFING: u32 = 0x1c0a;
pub const EE_SCREENS_GO_SETTINGS: u32 = 0x1c0b;
pub const EE_SCREENS_GO_EXIT: u32 = 0x1c0c;
/// The form's open event (the main game form's options button) and its close event (nothing).
pub const EE_SCREENS_GO_OPEN: u32 = 0x1c0d;
pub const EE_SCREENS_GO_CLOSE: u32 = 0x1c0e;
/// The main game form's briefing (`EEUIEMGDisplayScenarioIntro`: the briefing's body, the manager's factory).
pub const EE_SCREENS_GO_DISPLAY_INTRO: u32 = 0x1c0f;
/// The questions answered yes (no event object: the host's callbacks).
pub const EE_SCREENS_GO_RESIGN_YES: u32 = 0x1c10;
pub const EE_SCREENS_GO_RESTART_YES: u32 = 0x1c11;
pub const EE_SCREENS_GO_RETURN_TO_EDITOR_YES: u32 = 0x1c12;
pub const EE_SCREENS_GO_EXIT_YES: u32 = 0x1c13;
/// Play Scenario's posted run (the original posted its own object, its flag set: that run loads).
pub const EE_SCREENS_GO_LOAD_SCENARIO_POSTED: u32 = 0x1c14;

pub fn is_options_event(event: u32) -> bool {
    (EE_SCREENS_GO_RETURN_TO_GAME..=EE_SCREENS_GO_LOAD_SCENARIO_POSTED).contains(&event)
}

// ---- the host's ops (ee_screenscpp_call on the event's form; the shared ones are the earlier rounds')

pub const EE_SCREENS_GAME_GO_RESIGN: u32 = 0x200; // EEMResign to the local input
pub const EE_SCREENS_GAME_GO_RESTART: u32 = 0x201; // PostRestartGame
pub const EE_SCREENS_GAME_GO_TO_EDITOR: u32 = 0x202; // the physics' 0x402, PostEnterScenarioEditor, PostPauseInformation
pub const EE_SCREENS_GAME_GO_USER_PAUSE: u32 = 0x203; // PostAction(new EEAUserPauseGame)
pub const EE_SCREENS_GAME_GO_REQUEST_PAUSE: u32 = 0x204; // NEMClientRequestPause to the local input
pub const EE_SCREENS_GAME_GO_FORM_EXISTS: u32 = 0x205; // (form) -> the manager has the form
pub const EE_SCREENS_GAME_GO_TO_SCENARIO_LOAD: u32 = 0x206; // the scenario load form swapped in, the game paused
pub const EE_SCREENS_GAME_GO_BRIEFING: u32 = 0x207; // EEDisplayScenarioIntro
pub const EE_SCREENS_GAME_GO_CANNOT_EXIT: u32 = 0x208; // -> the host migrating, or a playback session
pub const EE_SCREENS_GAME_GO_EXIT: u32 = 0x209; // EEExitGame
pub const EE_SCREENS_GAME_GO_OPTIONS_BLINK_OFF: u32 = 0x20a; // the main game form's options button stops blinking
pub const EE_SCREENS_GAME_GO_USER_PAUSED: u32 = 0x20b; // -> gWorld.mUserPaused
pub const EE_SCREENS_OP_GO_REMOVE_HOT_KEYS: u32 = 0x20c; // RemoveAllHotKeys
pub const EE_SCREENS_OP_GO_HOT_KEY: u32 = 0x20d; // (key, code) AddHotKey(key, none, none, none, the code's event, ...)
pub const EE_SCREENS_OP_GO_STRING_ID: u32 = 0x20e; // (control, id) SetStringTableID
pub const EE_SCREENS_GAME_GO_HIDE_RESIGN: u32 = 0x20f; // -> the local player observes or the game is a scenario
/// Texts (ee_screenscpp_put_text).
pub const EE_SCREENS_TEXT_GO_QUESTION: u32 = 0x70; // (answer code) DisplayQuestion(text, true, NULL, its callback, 0)
pub const EE_SCREENS_TEXT_GO_BUTTON: u32 = 0x71; // (control) a button's SetText in its own font

// the controls (ee/EEUIManager.h), triggers (lle/UIControl.h), forms, keys and strings (ee/EEStringIDs.h)
const GO_FORM: u32 = 28;
const GO_FORM_SETTINGS: u32 = 5;
const GO_FORM_INTRO: u32 = 19;
const GO_FORM_CHAT_LOG: u32 = 29;
const GO_RETURN: u32 = 0;
const GO_RESIGN: u32 = 1;
const GO_PAUSE: u32 = 2;
const GO_RESTART: u32 = 3;
const GO_MESSAGES: u32 = 4;
const GO_SAVED_GAME: u32 = 5;
const GO_SCENARIO: u32 = 6;
const GO_SAVE: u32 = 7;
const GO_BRIEFING: u32 = 8;
const GO_SETTINGS: u32 = 9;
const GO_EXIT: u32 = 10;
const GO_BUTTON_UP: u32 = 0x3f0;
const GO_KEY_ESCAPE: u32 = 1;
const GO_SHOWN: u32 = 1;
const GO_HIDDEN: u32 = 0;
const GO_IDS_QUIT_AND_VIEW: u32 = 8549;
const GO_IDS_QUIT: u32 = 10043;
const GO_IDS_RESIGN: u32 = 10044;
const GO_IDS_RESTART: u32 = 10045;
const GO_IDS_TO_EDITOR: u32 = 10046;
const GO_IDS_UNPAUSE: u32 = 30666;
const GO_IDS_RETURN_TO_EDITOR: u32 = 31257;
const GO_IDS_CANNOT_EXIT: u32 = 32807;

/// The Play Scenario event's posted flag (`EEUIEMGOELoadScenario::mPosted`, cleared when an opening hooks the button up).
pub struct MainGameOptions {
    pub posted: bool,
}

impl MainGameOptions {
    pub const fn new() -> Self {
        MainGameOptions { posted: false }
    }
}

impl Default for MainGameOptions {
    fn default() -> Self {
        Self::new()
    }
}

/// The toolkit's and the game's calls of the menu, on the event's form (none for a posted event or an answer).
pub trait OptionsHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to four arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32;
    /// A text into the game.
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]);
    /// A resource string.
    fn string(&mut self, id: u32) -> Vec<u8>;
}

fn op<H: OptionsHost + ?Sized>(h: &mut H, op: u32, a: u32) -> u32 {
    h.call(op, a, 0, 0)
}
fn ask<H: OptionsHost + ?Sized>(h: &mut H, id: u32, answer: u32) {
    let text = h.string(id);
    h.put_text(EE_SCREENS_TEXT_GO_QUESTION, answer, &text);
}
fn button_up<H: OptionsHost + ?Sized>(h: &mut H, control: u32, event: u32) {
    h.call(EE_SCREENS_OP_TRIGGER_HERE, control, GO_BUTTON_UP, event);
}

/// `EEUIEMGOEOpenOptionsScreen`: the game paused, the options button's blinking stopped, Pause or Unpause, every
/// button's event; Save for the host only, restart and the loads in single player only, Exit or Return to Editor,
/// Resign hidden for an observer or a scenario.
fn open<H: OptionsHost + ?Sized>(h: &mut H, s: &Mutex<MainGameOptions>) {
    op(h, EE_SCREENS_GAME_PAUSE, 0);
    op(h, EE_SCREENS_GAME_GO_OPTIONS_BLINK_OFF, 0);
    if op(h, EE_SCREENS_GAME_GO_USER_PAUSED, 0) != 0 {
        let text = h.string(GO_IDS_UNPAUSE);
        h.put_text(EE_SCREENS_TEXT_GO_BUTTON, GO_PAUSE, &text);
    }
    h.call(EE_SCREENS_OP_CLOSE_EVENT, GO_FORM, EE_SCREENS_GO_CLOSE, 0);
    // divergence: Escape closes the menu (the other half of the main game's Escape toggle); the form keeps its hot
    // keys between openings, so the previous opening's go first
    op(h, EE_SCREENS_OP_GO_REMOVE_HOT_KEYS, 0);
    h.call(EE_SCREENS_OP_GO_HOT_KEY, GO_KEY_ESCAPE, EE_SCREENS_GO_RETURN_TO_GAME, 0);
    button_up(h, GO_RETURN, EE_SCREENS_GO_RETURN_TO_GAME);
    button_up(h, GO_RESIGN, EE_SCREENS_GO_RESIGN);
    button_up(h, GO_PAUSE, EE_SCREENS_GO_PAUSE);
    button_up(h, GO_MESSAGES, EE_SCREENS_GO_PREVIOUS_MESSAGES);
    button_up(h, GO_BRIEFING, EE_SCREENS_GO_BRIEFING);
    button_up(h, GO_SETTINGS, EE_SCREENS_GO_SETTINGS);
    if op(h, EE_SCREENS_GAME_IS_SERVER, 0) != 0 {
        button_up(h, GO_SAVE, EE_SCREENS_GO_SAVE_GAME);
        h.call(EE_SCREENS_OP_SHOW, GO_SAVE, GO_SHOWN, 0);
    } else {
        h.call(EE_SCREENS_OP_SHOW, GO_SAVE, GO_HIDDEN, 0);
    }
    if op(h, EE_SCREENS_GAME_MULTIPLAYER_SESSION, 0) != 0 {
        h.call(EE_SCREENS_OP_SHOW, GO_RESTART, GO_HIDDEN, 0);
        h.call(EE_SCREENS_OP_SHOW, GO_SAVED_GAME, GO_HIDDEN, 0);
        h.call(EE_SCREENS_OP_SHOW, GO_SCENARIO, GO_HIDDEN, 0);
    } else {
        h.call(EE_SCREENS_OP_SHOW, GO_RESTART, GO_SHOWN, 0);
        h.call(EE_SCREENS_OP_SHOW, GO_SAVED_GAME, GO_SHOWN, 0);
        h.call(EE_SCREENS_OP_SHOW, GO_SCENARIO, GO_SHOWN, 0);
        button_up(h, GO_SAVED_GAME, EE_SCREENS_GO_LOAD_SAVED_GAME);
        button_up(h, GO_SCENARIO, EE_SCREENS_GO_LOAD_SCENARIO);
        s.lock().unwrap().posted = false; // a new Play Scenario event on the button
        button_up(h, GO_RESTART, EE_SCREENS_GO_RESTART);
    }
    if op(h, EE_SCREENS_GAME_IN_EDITOR, 0) != 0 {
        button_up(h, GO_EXIT, EE_SCREENS_GO_RETURN_TO_EDITOR);
        let text = h.string(GO_IDS_RETURN_TO_EDITOR);
        h.put_text(EE_SCREENS_TEXT_GO_BUTTON, GO_EXIT, &text);
        h.call(EE_SCREENS_OP_GO_STRING_ID, GO_EXIT, GO_IDS_TO_EDITOR, 0);
    } else {
        button_up(h, GO_EXIT, EE_SCREENS_GO_EXIT);
        h.call(EE_SCREENS_OP_GO_STRING_ID, GO_EXIT, GO_IDS_QUIT_AND_VIEW, 0);
    }
    if op(h, EE_SCREENS_GAME_GO_HIDE_RESIGN, 0) != 0 {
        h.call(EE_SCREENS_OP_SHOW, GO_RESIGN, GO_HIDDEN, 0);
    }
}

/// Play Scenario's load: the scenario load form swapped in for a single player host's game.
fn load_scenario<H: OptionsHost + ?Sized>(h: &mut H) {
    let single_host = op(h, EE_SCREENS_GAME_IS_SERVER, 0) != 0
        && op(h, EE_SCREENS_GAME_CTS_PROCESSED, 0) > 0
        && op(h, EE_SCREENS_GAME_MULTIPLAYER_SESSION, 0) == 0;
    if single_host && op(h, EE_SCREENS_GAME_GO_FORM_EXISTS, GO_FORM_INTRO) != 0 {
        op(h, EE_SCREENS_GAME_GO_TO_SCENARIO_LOAD, 0);
    }
}

/// One of the menu's events; `None` for another form's, else its `ProcessEvent` result (Return to Game's and
/// Pause's: `CloseForm`'s; Game Settings': `OpenForm`'s; the rest `kErrorOK`).
pub fn dispatch<H: OptionsHost + ?Sized>(h: &mut H, s: &Mutex<MainGameOptions>, event: u32) -> Option<c_long> {
    if !is_options_event(event) {
        return None;
    }
    let single_host = |h: &mut H| {
        op(h, EE_SCREENS_GAME_IS_SERVER, 0) != 0
            && op(h, EE_SCREENS_GAME_CTS_PROCESSED, 0) > 0
            && op(h, EE_SCREENS_GAME_MULTIPLAYER_SESSION, 0) == 0
    };
    let mut result: c_long = 0;
    match event {
        EE_SCREENS_GO_RETURN_TO_GAME => result = op(h, EE_SCREENS_OP_CLOSE_FORM, 0) as i32 as c_long,
        EE_SCREENS_GO_RESIGN => ask(h, GO_IDS_RESIGN, EE_SCREENS_GO_RESIGN_YES),
        EE_SCREENS_GO_RESIGN_YES => {
            op(h, EE_SCREENS_GAME_GO_RESIGN, 0);
            op(h, EE_SCREENS_OP_CLOSE_FORM, 0);
        }
        EE_SCREENS_GO_RESTART => ask(h, GO_IDS_RESTART, EE_SCREENS_GO_RESTART_YES),
        EE_SCREENS_GO_RESTART_YES => {
            op(h, EE_SCREENS_GAME_GO_RESTART, 0);
        }
        EE_SCREENS_GO_RETURN_TO_EDITOR => ask(h, GO_IDS_TO_EDITOR, EE_SCREENS_GO_RETURN_TO_EDITOR_YES),
        EE_SCREENS_GO_RETURN_TO_EDITOR_YES => {
            if single_host(h) {
                op(h, EE_SCREENS_GAME_GO_TO_EDITOR, 0);
            }
        }
        EE_SCREENS_GO_LOAD_SAVED_GAME => {
            op(h, EE_SCREENS_OP_POST_EVENT, EE_SCREENS_SL_LOAD_SCREEN_GAME);
        }
        EE_SCREENS_GO_SAVE_GAME => {
            let editor = op(h, EE_SCREENS_GAME_IN_EDITOR, 0) != 0;
            let screen = if editor { EE_SCREENS_SL_SAVE_SCREEN_SCENARIO } else { EE_SCREENS_SL_SAVE_SCREEN_GAME };
            op(h, EE_SCREENS_OP_POST_EVENT, screen);
        }
        EE_SCREENS_GO_PAUSE => {
            if op(h, EE_SCREENS_GAME_CTS_PROCESSED, 0) > 0 {
                if op(h, EE_SCREENS_GAME_IS_SERVER, 0) != 0 {
                    op(h, EE_SCREENS_GAME_GO_USER_PAUSE, 0);
                } else {
                    op(h, EE_SCREENS_GAME_GO_REQUEST_PAUSE, 0);
                }
            }
            result = op(h, EE_SCREENS_OP_CLOSE_FORM, 0) as i32 as c_long;
        }
        EE_SCREENS_GO_PREVIOUS_MESSAGES => {
            op(h, EE_SCREENS_OP_OPEN_FORM, GO_FORM_CHAT_LOG);
        }
        EE_SCREENS_GO_LOAD_SCENARIO => {
            // the first press posts the event again; its later runs load (a single player host's)
            let posted = std::mem::replace(&mut s.lock().unwrap().posted, true);
            if !posted {
                op(h, EE_SCREENS_OP_POST_EVENT, EE_SCREENS_GO_LOAD_SCENARIO_POSTED);
            } else {
                load_scenario(h);
            }
        }
        EE_SCREENS_GO_LOAD_SCENARIO_POSTED => load_scenario(h),
        EE_SCREENS_GO_BRIEFING | EE_SCREENS_GO_DISPLAY_INTRO => {
            op(h, EE_SCREENS_GAME_GO_BRIEFING, 0);
        }
        EE_SCREENS_GO_SETTINGS => {
            h.call(EE_SCREENS_OP_OPEN_EVENT, GO_FORM_SETTINGS, EE_SCREENS_GS_IN_GAME_OPEN, 0);
            result = op(h, EE_SCREENS_OP_OPEN_FORM, GO_FORM_SETTINGS) as i32 as c_long;
        }
        EE_SCREENS_GO_EXIT => ask(h, GO_IDS_QUIT, EE_SCREENS_GO_EXIT_YES),
        EE_SCREENS_GO_EXIT_YES => {
            if op(h, EE_SCREENS_GAME_GO_CANNOT_EXIT, 0) == 0 {
                op(h, EE_SCREENS_GAME_GO_EXIT, 0);
            } else {
                let text = h.string(GO_IDS_CANNOT_EXIT);
                h.put_text(crate::menus::EE_SCREENS_TEXT_MESSAGE, 1, &text);
            }
        }
        EE_SCREENS_GO_OPEN => open(h, s),
        EE_SCREENS_GO_CLOSE => {}
        _ => return None,
    }
    Some(result)
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn events() {
        assert!(is_options_event(EE_SCREENS_GO_RETURN_TO_GAME) && is_options_event(EE_SCREENS_GO_EXIT_YES));
        assert!(!is_options_event(0x1c00) && !is_options_event(0x1c15));
    }
}
