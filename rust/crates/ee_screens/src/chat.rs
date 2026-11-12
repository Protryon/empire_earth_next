//! The in-game chat form (form 18) and the chat log form's events (form 29) whole: the original
//! `EEUIEChatEvents.cpp` (its 8 event classes) and `EEUIEChatLogEvents.cpp`'s three events over a host ([`ChatHost`])
//! whose calls are each one toolkit or game call (`UI Events/EEScreens.cpp`). The chat log's entries stay with their
//! form manager in C++ (`EEUIECLEFormManager`: the saved games' chat log); the log form reads them through the host.
//! The events have no state: the original's objects held only their constructor's argument, a code each here.

use core::ffi::c_long;

use crate::main_game::EE_SCREENS_GAME_MG_HAS_PLAYER;
use crate::main_game_options::EE_SCREENS_OP_GO_REMOVE_HOT_KEYS;
use crate::menus::{EE_SCREENS_OP_POP, EE_SCREENS_OP_SHOW, EE_SCREENS_TEXT_EDIT};
use crate::save_load::EE_SCREENS_OP_POST_EVENT;
use crate::settings_form::{
    EE_SCREENS_GAME_PAUSE, EE_SCREENS_OP_CHECK_RADIO, EE_SCREENS_OP_RADIO, EE_SCREENS_OP_TRIGGER_HERE,
};

// ---- the events (EEScreenEvent::mEvent)

/// The chat form's open and close events and its mouse press outside (`EECreateFormEvent`).
pub const EE_SCREENS_CH_OPEN: u32 = 0x1201;
pub const EE_SCREENS_CH_CLOSE: u32 = 0x1202;
pub const EE_SCREENS_CH_CANCEL: u32 = 0x1203;
/// Enter (the hot key and the edit box's Enter): the line sent.
pub const EE_SCREENS_CH_SEND: u32 = 0x1204;
/// The form's keyboard event: the edit box takes the keyboard.
pub const EE_SCREENS_CH_KEYBOARD_FOCUS: u32 = 0x1205;
/// The recipients' radio buttons (`EEUIECEChangeSendMessageType(0..3)`).
pub const EE_SCREENS_CH_SEND_TYPE_ALL: u32 = 0x1206;
pub const EE_SCREENS_CH_SEND_TYPE_ALLIES: u32 = 0x1207;
pub const EE_SCREENS_CH_SEND_TYPE_ENEMIES: u32 = 0x1208;
pub const EE_SCREENS_CH_SEND_TYPE_CUSTOM: u32 = 0x1209;
pub const EE_SCREENS_CH_ESCAPE: u32 = 0x120a;
/// Tab and Shift+Tab (`EEUIECEChangeUserChatType(false / true)`).
pub const EE_SCREENS_CH_NEXT_TYPE: u32 = 0x120b;
pub const EE_SCREENS_CH_PREVIOUS_TYPE: u32 = 0x120c;
/// The chat log form's open event, its posted close and its mouse press outside.
pub const EE_SCREENS_CL_OPEN: u32 = 0x1d01;
pub const EE_SCREENS_CL_CLOSE: u32 = 0x1d02;
pub const EE_SCREENS_CL_MOUSE_PRESSED: u32 = 0x1d03;

pub fn is_chat_event(event: u32) -> bool {
    (EE_SCREENS_CH_OPEN..=EE_SCREENS_CH_PREVIOUS_TYPE).contains(&event)
        || (EE_SCREENS_CL_OPEN..=EE_SCREENS_CL_MOUSE_PRESSED).contains(&event)
}

// ---- the host's ops (ee_screenscpp_call on the event's form)

pub const EE_SCREENS_GAME_CH_CHAT_TYPE: u32 = 0x240; // (type) EEMSelectPlayerChatType to the local input
pub const EE_SCREENS_OP_CH_SEND_KEYS: u32 = 0x241; // (code) Enter: a hot key object and the edit box's Enter trigger
pub const EE_SCREENS_OP_CH_HOT_KEY: u32 = 0x242; // (key, modifier, code) AddHotKey(key, modifier, none, none, ...)
pub const EE_SCREENS_GAME_CH_OBSERVER: u32 = 0x243; // -> the current player observes
pub const EE_SCREENS_GAME_CH_PLAYER_TYPE: u32 = 0x244; // -> the current player's chat type
pub const EE_SCREENS_OP_CH_TABBED_TO: u32 = 0x245; // (control) TabbedTo
pub const EE_SCREENS_OP_CH_KEYBOARD_EVENT: u32 = 0x246; // (code) SetFormKeyboardCapturedEvent
pub const EE_SCREENS_OP_CH_KEYBOARD_CAPTURED: u32 = 0x247; // -> GetIsKeyboardCaptured
pub const EE_SCREENS_OP_CH_TAB_STOP: u32 = 0x248; // (stop) SetTabStop
pub const EE_SCREENS_GAME_CH_CLOSE: u32 = 0x249; // -> CloseForm(the main game form)
pub const EE_SCREENS_OP_CH_MOUSE_INSIDE: u32 = 0x24a; // -> the form's rectangle holds its mouse location
pub const EE_SCREENS_GAME_CH_CHEATS: u32 = 0x24b; // -> gWorld's cheats enabled
pub const EE_SCREENS_OP_CL_BEGIN: u32 = 0x24c; // the log's label emptied, left aligned
pub const EE_SCREENS_GAME_CL_COUNT: u32 = 0x24d; // -> the chat log's entries
pub const EE_SCREENS_OP_CL_CENTER: u32 = 0x24e; // the label centred
pub const EE_SCREENS_OP_CL_END: u32 = 0x24f; // the label shows its last line
pub const EE_SCREENS_GAME_CL_UNPAUSE: u32 = 0x250; // gWorld.UnpauseGame()
/// Texts in (ee_screenscpp_text) and out (ee_screenscpp_put_text).
pub const EE_SCREENS_TEXT_CH_CHEAT: u32 = 0x75; // put: the cheat parser; its code (if any) to the local input
pub const EE_SCREENS_TEXT_CH_CHAT: u32 = 0x76; // put: EEMInGameChat to the local input
pub const EE_SCREENS_TEXT_CL_NAME: u32 = 0x77; // (i) entry i's sender
pub const EE_SCREENS_TEXT_CL_TEXT: u32 = 0x78; // (i) entry i's text
pub const EE_SCREENS_TEXT_CL_PLAYER: u32 = 0x79; // the current player's name
pub const EE_SCREENS_TEXT_CL_APPEND: u32 = 0x7a; // put: the log's label AppendText

// the controls (ee/EEUIManager.h), triggers (lle/UIControl.h), keys (lle/ISKeyCode.h) and strings
const CH_EDIT: u32 = 1;
const CH_ALLY: u32 = 2;
const CH_ALL: u32 = 3;
const CH_ENEMY: u32 = 4;
const CH_CUSTOM: u32 = 5;
const CH_TRIGGER_CHECKED: u32 = 0x3e9;
const CH_HIDDEN: u32 = 0;
const KEY_ESCAPE: u32 = 0x01;
const KEY_TAB: u32 = 0x0f;
const KEY_NONE: u32 = 0x7fff;
const KEY_SHIFT: u32 = 0xe1;
const IDS_NO_CHAT_MESSAGES: u32 = 31707;

/// The toolkit's and the game's calls of the chat forms, on the event's form.
pub trait ChatHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to three arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32;
    /// A text of the game.
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8>;
    /// A text into the game.
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]);
    /// A resource string.
    fn string(&mut self, id: u32) -> Vec<u8>;
}

fn op<H: ChatHost + ?Sized>(h: &mut H, op: u32, a: u32) -> u32 {
    h.call(op, a, 0, 0)
}
fn checked<H: ChatHost + ?Sized>(h: &mut H, radio: u32) -> bool {
    op(h, EE_SCREENS_OP_RADIO, radio) != 0
}

/// `EEUIECEOpenChat`: Enter sends, Escape closes, Tab and Shift+Tab cycle the recipients; the player's chat type
/// checked (all, the others hidden, for an observer); the edit box focused; the radios' and the keyboard's events.
fn open<H: ChatHost + ?Sized>(h: &mut H) {
    op(h, EE_SCREENS_OP_CH_SEND_KEYS, EE_SCREENS_CH_SEND);
    h.call(EE_SCREENS_OP_CH_HOT_KEY, KEY_ESCAPE, KEY_NONE, EE_SCREENS_CH_ESCAPE);
    h.call(EE_SCREENS_OP_CH_HOT_KEY, KEY_TAB, KEY_NONE, EE_SCREENS_CH_NEXT_TYPE);
    h.call(EE_SCREENS_OP_CH_HOT_KEY, KEY_TAB, KEY_SHIFT, EE_SCREENS_CH_PREVIOUS_TYPE);
    if op(h, EE_SCREENS_GAME_MG_HAS_PLAYER, 0) != 0 {
        if op(h, EE_SCREENS_GAME_CH_OBSERVER, 0) != 0 {
            op(h, EE_SCREENS_OP_CHECK_RADIO, CH_ALL);
            for radio in [CH_ALL, CH_ALLY, CH_ENEMY, CH_CUSTOM] {
                h.call(EE_SCREENS_OP_SHOW, radio, CH_HIDDEN, 0);
            }
        } else {
            let radio = match op(h, EE_SCREENS_GAME_CH_PLAYER_TYPE, 0) {
                0 => Some(CH_ALL),
                1 => Some(CH_ALLY),
                2 => Some(CH_ENEMY),
                3 => Some(CH_CUSTOM),
                _ => None,
            };
            if let Some(radio) = radio {
                op(h, EE_SCREENS_OP_CHECK_RADIO, radio);
            }
        }
    }
    op(h, EE_SCREENS_OP_CH_TABBED_TO, CH_EDIT);
    for (radio, event) in [
        (CH_ALL, EE_SCREENS_CH_SEND_TYPE_ALL),
        (CH_ALLY, EE_SCREENS_CH_SEND_TYPE_ALLIES),
        (CH_ENEMY, EE_SCREENS_CH_SEND_TYPE_ENEMIES),
        (CH_CUSTOM, EE_SCREENS_CH_SEND_TYPE_CUSTOM),
    ] {
        h.call(EE_SCREENS_OP_TRIGGER_HERE, radio, CH_TRIGGER_CHECKED, event);
    }
    op(h, EE_SCREENS_OP_CH_KEYBOARD_EVENT, EE_SCREENS_CH_KEYBOARD_FOCUS);
}

/// `EEUIECEChangeUserChatType`: the next recipients (all, enemies, custom, allies) or the previous.
fn cycle<H: ChatHost + ?Sized>(h: &mut H, backwards: bool) {
    // (the original's tests in its order: all, allies, enemies, custom; the first checked one moves)
    let order = [CH_ALL, CH_ALLY, CH_ENEMY, CH_CUSTOM];
    let next = |r: u32| match (r, backwards) {
        (CH_ALL, true) => CH_ALLY,
        (CH_ALLY, true) => CH_CUSTOM,
        (CH_ENEMY, true) => CH_ALL,
        (_, true) => CH_ENEMY,
        (CH_ALL, false) => CH_ENEMY,
        (CH_ALLY, false) => CH_ALL,
        (CH_ENEMY, false) => CH_CUSTOM,
        (_, false) => CH_ALLY,
    };
    for r in order {
        if checked(h, r) {
            op(h, EE_SCREENS_OP_CHECK_RADIO, next(r));
            return;
        }
    }
}

/// `EEUIECLEOpenChatLog`: the game paused, the log's lines ("<name>" ours, "[name]" others', "-" the game's) or a
/// centred "no chat messages", scrolled to the end.
fn open_log<H: ChatHost + ?Sized>(h: &mut H) {
    op(h, EE_SCREENS_GAME_PAUSE, 0);
    op(h, EE_SCREENS_OP_CL_BEGIN, 0);
    let count = op(h, EE_SCREENS_GAME_CL_COUNT, 0);
    for i in 0..count {
        let name = h.text(EE_SCREENS_TEXT_CL_NAME, i);
        let mut line: Vec<u8> = if name.is_empty() {
            b"  - ".to_vec()
        } else if name == h.text(EE_SCREENS_TEXT_CL_PLAYER, 0) {
            [b"  <".as_slice(), &name, b"> "].concat()
        } else {
            [b"  [".as_slice(), &name, b"] "].concat()
        };
        line.extend_from_slice(&h.text(EE_SCREENS_TEXT_CL_TEXT, i));
        line.push(b'\n');
        h.put_text(EE_SCREENS_TEXT_CL_APPEND, 0, &line);
    }
    if count == 0 {
        op(h, EE_SCREENS_OP_CL_CENTER, 0);
        let mut text = h.string(IDS_NO_CHAT_MESSAGES);
        text.push(b'\n');
        h.put_text(EE_SCREENS_TEXT_CL_APPEND, 0, &text);
    }
    op(h, EE_SCREENS_OP_CL_END, 0);
}

/// One of the chat forms' events; `None` for another form's, else its `ProcessEvent` result.
pub fn dispatch<H: ChatHost + ?Sized>(h: &mut H, event: u32) -> Option<c_long> {
    if !is_chat_event(event) {
        return None;
    }
    let mut result: c_long = 0;
    let signed = |v: u32| v as i32 as c_long;
    match event {
        EE_SCREENS_CH_OPEN => open(h),
        EE_SCREENS_CH_CLOSE => {
            op(h, EE_SCREENS_OP_GO_REMOVE_HOT_KEYS, 0);
        }
        EE_SCREENS_CH_CANCEL => {
            if op(h, EE_SCREENS_OP_CH_MOUSE_INSIDE, 0) == 0 {
                result = signed(op(h, EE_SCREENS_GAME_CH_CLOSE, 0));
            }
        }
        EE_SCREENS_CH_SEND => {
            let text = h.text(EE_SCREENS_TEXT_EDIT, CH_EDIT);
            if !text.is_empty() {
                if op(h, EE_SCREENS_GAME_CH_CHEATS, 0) != 0 {
                    h.put_text(EE_SCREENS_TEXT_CH_CHEAT, 0, &text);
                }
                h.put_text(EE_SCREENS_TEXT_CH_CHAT, 0, &text);
            }
            result = signed(op(h, EE_SCREENS_GAME_CH_CLOSE, 0));
        }
        EE_SCREENS_CH_KEYBOARD_FOCUS => {
            if op(h, EE_SCREENS_OP_CH_KEYBOARD_CAPTURED, 0) == 0 {
                op(h, EE_SCREENS_OP_CH_TAB_STOP, 1);
            }
        }
        EE_SCREENS_CH_SEND_TYPE_ALL..=EE_SCREENS_CH_SEND_TYPE_CUSTOM => {
            op(h, EE_SCREENS_GAME_CH_CHAT_TYPE, event - EE_SCREENS_CH_SEND_TYPE_ALL);
        }
        EE_SCREENS_CH_ESCAPE => result = signed(op(h, EE_SCREENS_GAME_CH_CLOSE, 0)),
        EE_SCREENS_CH_NEXT_TYPE => cycle(h, false),
        EE_SCREENS_CH_PREVIOUS_TYPE => cycle(h, true),
        EE_SCREENS_CL_OPEN => open_log(h),
        EE_SCREENS_CL_CLOSE => {
            op(h, EE_SCREENS_GAME_CL_UNPAUSE, 0);
            op(h, EE_SCREENS_OP_POP, 0);
        }
        EE_SCREENS_CL_MOUSE_PRESSED => {
            if op(h, EE_SCREENS_OP_CH_MOUSE_INSIDE, 0) == 0 {
                op(h, EE_SCREENS_OP_POST_EVENT, EE_SCREENS_CL_CLOSE);
            }
        }
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
        assert!(is_chat_event(EE_SCREENS_CH_OPEN) && is_chat_event(EE_SCREENS_CH_PREVIOUS_TYPE));
        assert!(is_chat_event(EE_SCREENS_CL_OPEN) && is_chat_event(EE_SCREENS_CL_MOUSE_PRESSED));
        assert!(!is_chat_event(0x1200) && !is_chat_event(0x120d) && !is_chat_event(0x1d04));
    }
}
