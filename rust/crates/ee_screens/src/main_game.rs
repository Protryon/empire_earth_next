//! The main game form's events (form 11) whole: the original `EEUIEMainGameEvents.cpp` (its event classes but the
//! command buttons' hot key event, which holds a C++ button) over a host ([`MainGameHost`]) whose calls are each one
//! toolkit or game call (`UI Events/EEScreens.cpp`). The events have no state of their own: the original's event
//! objects held only their constructor's argument, which is the event's code here (one per value its creators pass).

use core::ffi::c_long;

use crate::lobby_form::EE_SCREENS_GAME_LE_IS_CLIENT;
use crate::main_game_options::{EE_SCREENS_GAME_GO_USER_PAUSE, EE_SCREENS_GO_OPEN};
use crate::menus::{EE_SCREENS_OP_OPEN_EVENT, EE_SCREENS_OP_SHOW, EE_SCREENS_OP_SWITCH};
use crate::save_load::{EE_SCREENS_GAME_CTS_PROCESSED, EE_SCREENS_GAME_IN_EDITOR, EE_SCREENS_GAME_MULTIPLAYER_SESSION};
use crate::settings_form::EE_SCREENS_GAME_IS_SERVER;

// ---- the events (EEScreenEvent::mEvent): the main game form's control events, hot keys and posted events

pub const EE_SCREENS_MG_CANCEL_UI_MODE: u32 = 0x0b01;
/// The media dump (`EEUIEMGDumpLoadedMedia(replace, leaks)`): the main screen's two hot keys, the scenario editor's.
pub const EE_SCREENS_MG_DUMP_MEDIA: u32 = 0x0b02; // (false, true)
pub const EE_SCREENS_MG_DUMP_MEDIA_ALL: u32 = 0x0b03; // (true, true)
pub const EE_SCREENS_MG_DUMP_MEDIA_EDITOR: u32 = 0x0b04; // (false, false)
pub const EE_SCREENS_MG_DUMP_MEDIA_EDITOR_ALL: u32 = 0x0b05; // (true, false)
pub const EE_SCREENS_MG_END_SCENARIO_DIALOG: u32 = 0x0b06;
pub const EE_SCREENS_MG_DISPLAY_STATISTICS: u32 = 0x0b07;
pub const EE_SCREENS_MG_DISPLAY_CIV_BUILDER: u32 = 0x0b08;
pub const EE_SCREENS_MG_DISPLAY_CHAT: u32 = 0x0b09;
pub const EE_SCREENS_MG_DISPLAY_DIPLOMACY: u32 = 0x0b0a;
pub const EE_SCREENS_MG_DISPLAY_OPTIONS: u32 = 0x0b0b;
pub const EE_SCREENS_MG_ENTER_SHIFT: u32 = 0x0b0c;
pub const EE_SCREENS_MG_EXIT_SHIFT: u32 = 0x0b0d;
pub const EE_SCREENS_MG_FLARE: u32 = 0x0b0e;
/// The airport's rally timers (`EEUIEMGIncrementRallyTimer(1..3)`).
pub const EE_SCREENS_MG_RALLY_FIGHTER: u32 = 0x0b0f;
pub const EE_SCREENS_MG_RALLY_BOMBER: u32 = 0x0b10;
pub const EE_SCREENS_MG_RALLY_ATOMIC: u32 = 0x0b11;
pub const EE_SCREENS_MG_ENTER_EDITOR: u32 = 0x0b12;
/// F4: quick save, Shift+F4 quick load, Ctrl+F4 the auto save's load (`EEUIEMGInGameLoadAndSave(0..2)`).
pub const EE_SCREENS_MG_QUICK_SAVE: u32 = 0x0b13;
pub const EE_SCREENS_MG_QUICK_LOAD: u32 = 0x0b14;
pub const EE_SCREENS_MG_AUTO_LOAD: u32 = 0x0b15;
/// F3, the status button's pause and the server's posted pause (`EEUIEMGPauseGame`).
pub const EE_SCREENS_MG_PAUSE: u32 = 0x0b16;
pub const EE_SCREENS_MG_CANCEL_UPDATE_STATE: u32 = 0x0b17;
pub const EE_SCREENS_MG_TOGGLE_CLOCK: u32 = 0x0b18;
pub const EE_SCREENS_MG_KILL: u32 = 0x0b19;
pub const EE_SCREENS_MG_KILL_ALL: u32 = 0x0b1a;
pub const EE_SCREENS_MG_ATTACK: u32 = 0x0b1b;
pub const EE_SCREENS_MG_ATTACK_ALLIES: u32 = 0x0b1c;
pub const EE_SCREENS_MG_TOGGLE_HIDDEN: u32 = 0x0b1d;
/// The period key: follow the lead unit (`EEUIEMGFollowUnit(false)`, its only creator).
pub const EE_SCREENS_MG_FOLLOW: u32 = 0x0b1e;
pub const EE_SCREENS_MG_PERSPECTIVE: u32 = 0x0b1f;
pub const EE_SCREENS_MG_FASTER: u32 = 0x0b20;
pub const EE_SCREENS_MG_SLOWER: u32 = 0x0b21;
/// The behaviour menu (`EEUIEMGUnitBehavior(0, 1, 2, 5)`).
pub const EE_SCREENS_MG_BEHAVIOR_SCOUT: u32 = 0x0b22;
pub const EE_SCREENS_MG_BEHAVIOR_STAND_GROUND: u32 = 0x0b23;
pub const EE_SCREENS_MG_BEHAVIOR_GUARD: u32 = 0x0b24;
pub const EE_SCREENS_MG_BEHAVIOR_AGGRESSIVE: u32 = 0x0b25;
pub const EE_SCREENS_MG_PREVIOUS_MESSAGES: u32 = 0x0b26;
/// The formation menu's eight buttons (`EEUIEFormationChange(0..7)`): this code plus the formation type.
pub const EE_SCREENS_MG_FORMATION: u32 = 0x0b27;

pub fn is_main_game_event(event: u32) -> bool {
    (EE_SCREENS_MG_CANCEL_UI_MODE..=EE_SCREENS_MG_FORMATION + 7).contains(&event)
}

// ---- the host's ops (ee_screenscpp_call on the event's form, the main game form; the shared ones are earlier rounds')

pub const EE_SCREENS_GAME_MG_UI_STATE: u32 = 0x210; // -> the form's mUIState
pub const EE_SCREENS_GAME_MG_GHOSTS: u32 = 0x211; // -> gWorld.GetNumGhostUnits()
pub const EE_SCREENS_GAME_MG_CLEAR_GHOSTS: u32 = 0x212; // ClearGhostUnits inside the ghosts' lock
pub const EE_SCREENS_GAME_MG_CLEAR_GATE_GHOSTS: u32 = 0x213; // the form's mBuildGate.ClearGhostUnits()
pub const EE_SCREENS_GAME_MG_SET_UI_STATE: u32 = 0x214; // (state) the form's SetUIState
pub const EE_SCREENS_GAME_MG_POINTER: u32 = 0x215; // (pointer) gMouse->mPointer
pub const EE_SCREENS_GAME_MG_CURSOR_LOCK: u32 = 0x216; // (locked) the form's mIsCursorLocked
pub const EE_SCREENS_GAME_MG_REFRESH_GROUP: u32 = 0x217; // the manager's main game form (if any): mRefreshGroup
pub const EE_SCREENS_GAME_MG_DUMP_MEDIA: u32 = 0x218; // (replace, leaks) Media Dump.txt -> FOpen's result
pub const EE_SCREENS_GAME_MG_SHOW_DIALOG: u32 = 0x219; // -> gPhysics->PostShowDialogMessage()
pub const EE_SCREENS_GAME_MG_CURRENT_GROUP: u32 = 0x21a; // (group) the form's mCurrentGroup
pub const EE_SCREENS_GAME_MG_ENTITY_GROUP: u32 = 0x21b; // (group) the form's SetEntityGroup
pub const EE_SCREENS_GAME_MG_REFRESH_FORM: u32 = 0x21c; // the form's FnE123D
pub const EE_SCREENS_GAME_MG_CIV_BUILDER: u32 = 0x21d; // -> PostDisplayCivBuilder()
pub const EE_SCREENS_GAME_MG_HAS_PLAYER: u32 = 0x21e; // -> gWorld.GetCurrentPlayer() != NULL
pub const EE_SCREENS_GAME_MG_SHIFT: u32 = 0x21f; // (enter) EEMEnterShiftMode / EEMExitShiftMode at the game time
pub const EE_SCREENS_GAME_MG_AIRPORT_SELECTED: u32 = 0x220; // -> an airport among the current player's selection
pub const EE_SCREENS_GAME_MG_RALLY: u32 = 0x221; // (timer) EEMIncrementRallyTimer
pub const EE_SCREENS_GAME_MG_HOT_KEY_MODE: u32 = 0x222; // -> the manager's main game form's mHotKeyMode
pub const EE_SCREENS_GAME_MG_TO_EDITOR: u32 = 0x223; // ghosts cleared, the player to the editor slot, ... -> the post's
pub const EE_SCREENS_GAME_MG_QUICK_SAVE: u32 = 0x224; // the timestamped save to every player
pub const EE_SCREENS_GAME_MG_DIALOG_SHOWN: u32 = 0x226; // -> the dialog message control shown, or a cinematic
pub const EE_SCREENS_GAME_MG_CLOCK: u32 = 0x227; // -> the form's mClockMode
pub const EE_SCREENS_GAME_MG_SET_CLOCK: u32 = 0x228; // (mode)
pub const EE_SCREENS_GAME_MG_KILL: u32 = 0x229; // (all) EEMKillUnit -> the input's result
pub const EE_SCREENS_GAME_MG_ATTACK: u32 = 0x22a; // (allies) EEMAttackPlayers for the current player -> the result
pub const EE_SCREENS_GAME_MG_HIDDEN_MODE: u32 = 0x22b; // -> gHiddenUnitsMode
pub const EE_SCREENS_GAME_MG_SET_HIDDEN_MODE: u32 = 0x22c; // (mode)
pub const EE_SCREENS_GAME_MG_MARK_HIDDEN_MOVED: u32 = 0x22d; // every unit that can hide: its moved flag set
pub const EE_SCREENS_GAME_MG_FOLLOW_LEAD: u32 = 0x22e; // (flag) the camera follows the lead unit; SetChanged unless flag
pub const EE_SCREENS_GAME_MG_CAMERA_MODE: u32 = 0x22f; // -> the camera's mMode
pub const EE_SCREENS_GAME_MG_SET_CAMERA_MODE: u32 = 0x230; // (mode) SetMode
pub const EE_SCREENS_GAME_MG_ZOOM_STYLE: u32 = 0x231; // the preferences' zoom style = the camera's mode
pub const EE_SCREENS_GAME_MG_LOCK_SPEED: u32 = 0x232; // -> the game options' speed lock
pub const EE_SCREENS_GAME_MG_GAME_SPEED: u32 = 0x233; // -> the world's speed (float bits)
pub const EE_SCREENS_GAME_MG_SPEED_TABLE: u32 = 0x234; // (0 step, 1 lowest, 2 highest) -> float bits
pub const EE_SCREENS_GAME_MG_SEND_SPEED: u32 = 0x235; // (bits) EEMGameSpeed
pub const EE_SCREENS_GAME_MG_SELECTED_COMMAND: u32 = 0x236; // (i) the main group's unit's class command i (0: none)
pub const EE_SCREENS_GAME_MG_BEHAVIOR: u32 = 0x237; // (behaviour) EEMSetUnitBehavior
pub const EE_SCREENS_GAME_MG_CLOSE_BEHAVIORS: u32 = 0x238; // the main game form's group 9 off, background 0x97 hidden
pub const EE_SCREENS_GAME_MG_COPY_BITMAPS: u32 = 0x239; // (to, from) the main game form's button bitmaps
/// (type) EEFormation::Create, EEMChangeFormation posted, the formation menu (group 8, background 0x98) closed, the
/// formation the selected group's
pub const EE_SCREENS_GAME_MG_SET_FORMATION: u32 = 0x23a;
/// Texts (ee_screenscpp_put_text).
pub const EE_SCREENS_TEXT_MG_CHAT_LOG: u32 = 0x72; // AddChatLogEntry(text, 0)
/// (auto) the last quick (or auto) save's load; the text is the not-found message, shown when its file is missing.
pub const EE_SCREENS_TEXT_MG_QUICK_LOAD: u32 = 0x73;

// the controls (ee/EEUIManager.h), forms and strings (ee/EEStringIDs.h)
const MG_CANCEL_UI_MODE: u32 = 244;
const MG_BEHAVIOR_STAND_GROUND: u32 = 228;
const MG_BEHAVIOR_AGGRESSIVE: u32 = 229;
const MG_BEHAVIOR_SCOUT: u32 = 232;
const MG_BEHAVIOR_GUARD: u32 = 233;
const MG_COMMAND_BUTTONS: u32 = 0x4e;
const MG_COMMAND_BEHAVIOR: u32 = 0x1d;
const MG_COMMAND_FORMATION: u32 = 30;
/// The formation menu's buttons, one per formation type (`kMainGameFormationShortLine` ..).
const MG_FORMATION_BUTTONS: u32 = 236;
const MG_SHOWN: u32 = 1;
const MG_HIDDEN: u32 = 0;
const MG_FORM_CHAT: u32 = 18;
const MG_FORM_DIPLOMACY: u32 = 27;
const MG_FORM_OPTIONS: u32 = 28;
const MG_FORM_CHAT_LOG: u32 = 29;
const MG_UI_STATE_GATE: u32 = 0xb;
const MG_UI_STATE_FLARE: u32 = 0x12;
const IDS_FILE_NAME_NOT_FOUND: u32 = 10019;
const IDS_QUICK_LOAD_IS_FOR_SINGLE_PLAYER: u32 = 30664;
const IDS_QUICK_SAVE_AND_LOAD_ARE_ONLY: u32 = 30665;
const IDS_NORMAL_HIDDEN_UNITS_MODE: u32 = 8618;
const IDS_BUILDINGS_ALWAYS_TRANSPARENT: u32 = 8619;
const IDS_BUILDINGS_ALWAYS_OPAQUE: u32 = 8620;
const IDS_EPIC_ZOOM_MODE: u32 = 8616;
const IDS_ISOMETRIC_ZOOM_MODE: u32 = 8617;
const IDS_NORMAL_ZOOM_MODE: u32 = 8615;

/// The toolkit's and the game's calls of the main game form's events, on the event's form.
pub trait MainGameHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to three arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32;
    /// A text into the game.
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]);
    /// A resource string.
    fn string(&mut self, id: u32) -> Vec<u8>;
}

fn op<H: MainGameHost + ?Sized>(h: &mut H, op: u32) -> u32 {
    h.call(op, 0, 0)
}
fn chat_log<H: MainGameHost + ?Sized>(h: &mut H, id: u32) {
    let text = h.string(id);
    h.put_text(EE_SCREENS_TEXT_MG_CHAT_LOG, 0, &text);
}
fn f32_of(bits: u32) -> f32 {
    f32::from_bits(bits)
}

/// `EEUIEMGDisplayOptions`: the in-game options menu with its open event.
fn display_options<H: MainGameHost + ?Sized>(h: &mut H) {
    h.call(EE_SCREENS_OP_OPEN_EVENT, MG_FORM_OPTIONS, EE_SCREENS_GO_OPEN);
    h.call(EE_SCREENS_OP_SWITCH, MG_FORM_OPTIONS, 0);
}

/// The ghost units cleared, the mode, the pointer and the cursor lock reset (the cancel's and Escape's common part).
fn reset_mode<H: MainGameHost + ?Sized>(h: &mut H) {
    h.call(EE_SCREENS_GAME_MG_SET_UI_STATE, 0, 0);
    h.call(EE_SCREENS_GAME_MG_POINTER, 0, 0);
    h.call(EE_SCREENS_GAME_MG_CURSOR_LOCK, 0, 0);
}

/// `EEUIEMGChangeGameSpeed`: one step up or down within the world table's range (none past it), unless locked.
fn change_speed<H: MainGameHost + ?Sized>(h: &mut H, faster: bool) {
    if op(h, EE_SCREENS_GAME_MG_LOCK_SPEED) != 0 {
        return;
    }
    let speed = f32_of(op(h, EE_SCREENS_GAME_MG_GAME_SPEED));
    let step = f32_of(h.call(EE_SCREENS_GAME_MG_SPEED_TABLE, 0, 0));
    let mut delta = 0.0f32;
    if faster {
        if speed + step <= f32_of(h.call(EE_SCREENS_GAME_MG_SPEED_TABLE, 2, 0)) {
            delta = step;
        }
    } else if speed - step >= f32_of(h.call(EE_SCREENS_GAME_MG_SPEED_TABLE, 1, 0)) {
        delta = -step;
    }
    h.call(EE_SCREENS_GAME_MG_SEND_SPEED, (speed + delta).to_bits(), 0);
}

/// `EEUIEMGUnitBehavior`: the behaviour sent, the menu closed, the chosen behaviour's bitmaps on the selected unit's
/// first behaviour command button.
fn unit_behavior<H: MainGameHost + ?Sized>(h: &mut H, behavior: u32) {
    // (the unit's commands as the original read them: before the message)
    let mut commands = [0u32; 9];
    for (i, c) in commands.iter_mut().enumerate() {
        *c = h.call(EE_SCREENS_GAME_MG_SELECTED_COMMAND, i as u32, 0);
    }
    h.call(EE_SCREENS_GAME_MG_BEHAVIOR, behavior, 0);
    op(h, EE_SCREENS_GAME_MG_CLOSE_BEHAVIORS);
    let Some(i) = commands.iter().position(|&c| c == MG_COMMAND_BEHAVIOR) else {
        return;
    };
    let from = match behavior {
        0 => MG_BEHAVIOR_SCOUT,
        1 => MG_BEHAVIOR_STAND_GROUND,
        2 => MG_BEHAVIOR_GUARD,
        _ => MG_BEHAVIOR_AGGRESSIVE,
    };
    h.call(EE_SCREENS_GAME_MG_COPY_BITMAPS, MG_COMMAND_BUTTONS + i as u32, from);
}

/// `EEUIEFormationChange`: the formation set and posted, the menu closed, the chosen formation's bitmaps on the
/// selected unit's first formation command button.
fn formation_change<H: MainGameHost + ?Sized>(h: &mut H, formation: u32) {
    // (the unit as the original took it: before the formation; its class's commands do not change)
    let mut commands = [0u32; 9];
    for (i, c) in commands.iter_mut().enumerate() {
        *c = h.call(EE_SCREENS_GAME_MG_SELECTED_COMMAND, i as u32, 0);
    }
    h.call(EE_SCREENS_GAME_MG_SET_FORMATION, formation, 0);
    if let Some(i) = commands.iter().position(|&c| c == MG_COMMAND_FORMATION) {
        h.call(EE_SCREENS_GAME_MG_COPY_BITMAPS, MG_COMMAND_BUTTONS + i as u32, MG_FORMATION_BUTTONS + formation);
    }
}

/// `EEUIEMGInGameLoadAndSave`: the host's quick save or load of a running game (the loads in single player only); a
/// client is told they are the host's.
fn load_and_save<H: MainGameHost + ?Sized>(h: &mut H, mode: u32) {
    if op(h, EE_SCREENS_GAME_IS_SERVER) != 0 && op(h, EE_SCREENS_GAME_CTS_PROCESSED) > 0 {
        if mode == 0 {
            op(h, EE_SCREENS_GAME_MG_QUICK_SAVE);
        } else if op(h, EE_SCREENS_GAME_MULTIPLAYER_SESSION) == 0 {
            // (the not-found message goes with the load: the host shows it before the file goes)
            let text = h.string(IDS_FILE_NAME_NOT_FOUND);
            h.put_text(EE_SCREENS_TEXT_MG_QUICK_LOAD, (mode == 2) as u32, &text);
        } else {
            chat_log(h, IDS_QUICK_LOAD_IS_FOR_SINGLE_PLAYER);
        }
    } else if op(h, EE_SCREENS_GAME_LE_IS_CLIENT) != 0 && op(h, EE_SCREENS_GAME_CTS_PROCESSED) > 0 {
        chat_log(h, IDS_QUICK_SAVE_AND_LOAD_ARE_ONLY);
    }
}

/// One of the main game form's events; `None` for another form's, else its `ProcessEvent` result.
pub fn dispatch<H: MainGameHost + ?Sized>(h: &mut H, event: u32) -> Option<c_long> {
    if !is_main_game_event(event) {
        return None;
    }
    let mut result: c_long = 0;
    let signed = |v: u32| v as i32 as c_long;
    match event {
        EE_SCREENS_MG_CANCEL_UI_MODE => {
            if op(h, EE_SCREENS_GAME_MG_UI_STATE) != MG_UI_STATE_GATE {
                if op(h, EE_SCREENS_GAME_MG_GHOSTS) > 0 {
                    op(h, EE_SCREENS_GAME_MG_CLEAR_GHOSTS);
                }
            } else {
                op(h, EE_SCREENS_GAME_MG_CLEAR_GATE_GHOSTS);
            }
            reset_mode(h);
            h.call(EE_SCREENS_OP_SHOW, MG_CANCEL_UI_MODE, MG_HIDDEN);
            op(h, EE_SCREENS_GAME_MG_REFRESH_GROUP);
        }
        EE_SCREENS_MG_DUMP_MEDIA..=EE_SCREENS_MG_DUMP_MEDIA_EDITOR_ALL => {
            let n = event - EE_SCREENS_MG_DUMP_MEDIA;
            result = signed(h.call(EE_SCREENS_GAME_MG_DUMP_MEDIA, n & 1, (n < 2) as u32));
        }
        EE_SCREENS_MG_END_SCENARIO_DIALOG => result = signed(op(h, EE_SCREENS_GAME_MG_SHOW_DIALOG)),
        EE_SCREENS_MG_DISPLAY_STATISTICS => {
            h.call(EE_SCREENS_GAME_MG_CURRENT_GROUP, 1, 0);
            h.call(EE_SCREENS_GAME_MG_ENTITY_GROUP, 2, 0);
            op(h, EE_SCREENS_GAME_MG_REFRESH_FORM);
        }
        EE_SCREENS_MG_DISPLAY_CIV_BUILDER => result = signed(op(h, EE_SCREENS_GAME_MG_CIV_BUILDER)),
        EE_SCREENS_MG_DISPLAY_CHAT | EE_SCREENS_MG_DISPLAY_DIPLOMACY => {
            if op(h, EE_SCREENS_GAME_MG_HAS_PLAYER) != 0 {
                let form = if event == EE_SCREENS_MG_DISPLAY_CHAT { MG_FORM_CHAT } else { MG_FORM_DIPLOMACY };
                h.call(EE_SCREENS_OP_SWITCH, form, 0);
            }
        }
        EE_SCREENS_MG_DISPLAY_OPTIONS => display_options(h),
        EE_SCREENS_MG_ENTER_SHIFT | EE_SCREENS_MG_EXIT_SHIFT => {
            h.call(EE_SCREENS_GAME_MG_SHIFT, (event == EE_SCREENS_MG_ENTER_SHIFT) as u32, 0);
        }
        EE_SCREENS_MG_FLARE => {
            h.call(EE_SCREENS_GAME_MG_SET_UI_STATE, MG_UI_STATE_FLARE, 0);
            h.call(EE_SCREENS_OP_SHOW, MG_CANCEL_UI_MODE, MG_SHOWN);
            op(h, EE_SCREENS_GAME_MG_REFRESH_GROUP);
        }
        EE_SCREENS_MG_RALLY_FIGHTER..=EE_SCREENS_MG_RALLY_ATOMIC => {
            if op(h, EE_SCREENS_GAME_MG_AIRPORT_SELECTED) != 0 {
                h.call(EE_SCREENS_GAME_MG_RALLY, event - EE_SCREENS_MG_RALLY_FIGHTER + 1, 0);
            }
        }
        EE_SCREENS_MG_ENTER_EDITOR => {
            if op(h, EE_SCREENS_GAME_IS_SERVER) != 0
                && op(h, EE_SCREENS_GAME_CTS_PROCESSED) > 0
                && op(h, EE_SCREENS_GAME_MULTIPLAYER_SESSION) == 0
                && (op(h, EE_SCREENS_GAME_IN_EDITOR) != 0 || op(h, EE_SCREENS_GAME_MG_HOT_KEY_MODE) > 0)
            {
                result = signed(op(h, EE_SCREENS_GAME_MG_TO_EDITOR));
            }
        }
        EE_SCREENS_MG_QUICK_SAVE..=EE_SCREENS_MG_AUTO_LOAD => load_and_save(h, event - EE_SCREENS_MG_QUICK_SAVE),
        EE_SCREENS_MG_PAUSE => {
            if op(h, EE_SCREENS_GAME_CTS_PROCESSED) > 0 && op(h, EE_SCREENS_GAME_IS_SERVER) != 0 {
                // (1: the action's reference dropped, as this event's original did)
                h.call(EE_SCREENS_GAME_GO_USER_PAUSE, 1, 0);
            }
        }
        EE_SCREENS_MG_CANCEL_UPDATE_STATE => {
            if op(h, EE_SCREENS_GAME_MG_UI_STATE) != 0 {
                op(h, EE_SCREENS_GAME_MG_CLEAR_GHOSTS);
                reset_mode(h);
            } else if op(h, EE_SCREENS_GAME_MG_DIALOG_SHOWN) != 0 {
                result = signed(op(h, EE_SCREENS_GAME_MG_SHOW_DIALOG));
            } else {
                // divergence (the original's): Escape with no mode and no message opens the in-game menu
                display_options(h);
            }
        }
        EE_SCREENS_MG_TOGGLE_CLOCK => {
            let next = match op(h, EE_SCREENS_GAME_MG_CLOCK) {
                1 => 2,
                2 => 0,
                _ => 1,
            };
            h.call(EE_SCREENS_GAME_MG_SET_CLOCK, next, 0);
        }
        EE_SCREENS_MG_KILL | EE_SCREENS_MG_KILL_ALL => {
            result = signed(h.call(EE_SCREENS_GAME_MG_KILL, (event == EE_SCREENS_MG_KILL_ALL) as u32, 0));
        }
        EE_SCREENS_MG_ATTACK | EE_SCREENS_MG_ATTACK_ALLIES => {
            if op(h, EE_SCREENS_GAME_MULTIPLAYER_SESSION) == 0 {
                result = signed(h.call(EE_SCREENS_GAME_MG_ATTACK, (event == EE_SCREENS_MG_ATTACK_ALLIES) as u32, 0));
            }
        }
        EE_SCREENS_MG_TOGGLE_HIDDEN => {
            let (next, id) = match op(h, EE_SCREENS_GAME_MG_HIDDEN_MODE) {
                0 => (1, IDS_NORMAL_HIDDEN_UNITS_MODE),
                1 => (2, IDS_BUILDINGS_ALWAYS_TRANSPARENT),
                _ => (0, IDS_BUILDINGS_ALWAYS_OPAQUE),
            };
            h.call(EE_SCREENS_GAME_MG_SET_HIDDEN_MODE, next, 0);
            let text = h.string(id);
            if next == 1 {
                op(h, EE_SCREENS_GAME_MG_MARK_HIDDEN_MOVED);
            }
            h.put_text(EE_SCREENS_TEXT_MG_CHAT_LOG, 0, &text);
        }
        EE_SCREENS_MG_FOLLOW => {
            h.call(EE_SCREENS_GAME_MG_FOLLOW_LEAD, 0, 0);
        }
        EE_SCREENS_MG_PERSPECTIVE => {
            let (next, id) = match op(h, EE_SCREENS_GAME_MG_CAMERA_MODE) {
                0 => (1, IDS_EPIC_ZOOM_MODE),
                1 => (2, IDS_ISOMETRIC_ZOOM_MODE),
                _ => (0, IDS_NORMAL_ZOOM_MODE),
            };
            h.call(EE_SCREENS_GAME_MG_SET_CAMERA_MODE, next, 0);
            let text = h.string(id);
            op(h, EE_SCREENS_GAME_MG_ZOOM_STYLE);
            h.put_text(EE_SCREENS_TEXT_MG_CHAT_LOG, 0, &text);
        }
        EE_SCREENS_MG_FASTER | EE_SCREENS_MG_SLOWER => change_speed(h, event == EE_SCREENS_MG_FASTER),
        EE_SCREENS_MG_BEHAVIOR_SCOUT => unit_behavior(h, 0),
        EE_SCREENS_MG_BEHAVIOR_STAND_GROUND => unit_behavior(h, 1),
        EE_SCREENS_MG_BEHAVIOR_GUARD => unit_behavior(h, 2),
        EE_SCREENS_MG_BEHAVIOR_AGGRESSIVE => unit_behavior(h, 5),
        EE_SCREENS_MG_PREVIOUS_MESSAGES => {
            h.call(EE_SCREENS_OP_SWITCH, MG_FORM_CHAT_LOG, 0);
        }
        _ if (EE_SCREENS_MG_FORMATION..EE_SCREENS_MG_FORMATION + 8).contains(&event) => {
            formation_change(h, event - EE_SCREENS_MG_FORMATION)
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
        assert!(
            is_main_game_event(EE_SCREENS_MG_CANCEL_UI_MODE) && is_main_game_event(EE_SCREENS_MG_PREVIOUS_MESSAGES)
        );
        assert!(is_main_game_event(EE_SCREENS_MG_FORMATION) && is_main_game_event(EE_SCREENS_MG_FORMATION + 7));
        assert!(!is_main_game_event(0x0b00) && !is_main_game_event(0x0b2f));
    }

    #[test]
    fn speed_steps_stay_in_range() {
        struct H(Vec<(u32, u32)>, [f32; 4], bool);
        impl MainGameHost for H {
            fn call(&mut self, op: u32, a: u32, _b: u32) -> u32 {
                match op {
                    EE_SCREENS_GAME_MG_LOCK_SPEED => self.2 as u32,
                    EE_SCREENS_GAME_MG_GAME_SPEED => self.1[0].to_bits(),
                    EE_SCREENS_GAME_MG_SPEED_TABLE => self.1[1 + a as usize].to_bits(),
                    _ => {
                        self.0.push((op, a));
                        0
                    }
                }
            }
            fn put_text(&mut self, _: u32, _: u32, _: &[u8]) {}
            fn string(&mut self, _: u32) -> Vec<u8> {
                Vec::new()
            }
        }
        let mut h = H(Vec::new(), [1.0, 0.5, 0.5, 1.5], false);
        dispatch(&mut h, EE_SCREENS_MG_FASTER);
        dispatch(&mut h, EE_SCREENS_MG_SLOWER);
        h.1[0] = 1.25;
        dispatch(&mut h, EE_SCREENS_MG_FASTER);
        h.2 = true;
        dispatch(&mut h, EE_SCREENS_MG_FASTER);
        let sent: Vec<f32> = h.0.iter().map(|&(_, a)| f32::from_bits(a)).collect();
        assert_eq!(sent, vec![1.5, 0.5, 1.25]);
    }
}
