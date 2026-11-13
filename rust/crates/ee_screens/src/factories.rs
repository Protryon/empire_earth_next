//! The game's UI manager's event factories (`EEUIManager.cpp`): the control-event factories it registers, as one
//! table in their registration order (the first that knows an id makes its event), and the form-event factory. Each
//! answer is a screen event's code (`EEScreenEvent`), an `EE_SCREENS_CPP_*` code for the few events that are still C++
//! classes, or 0 for an id no factory knows. WON's server select factory stays C++ (the WON lane's).

use core::ffi::c_ulong;

use crate::campaign_editor::{EE_SCREENS_CE_CLOSE, EE_SCREENS_CE_OPEN};
use crate::chat::{
    EE_SCREENS_CH_CANCEL, EE_SCREENS_CH_CLOSE, EE_SCREENS_CH_OPEN, EE_SCREENS_CL_CLOSE, EE_SCREENS_CL_MOUSE_PRESSED,
    EE_SCREENS_CL_OPEN,
};
use crate::civ_builder::*;
use crate::diplomacy::EE_SCREENS_DS_OPEN;
use crate::hot_keys::ee_screens_hot_key_unit_selection;
use crate::main_game::*;
use crate::main_game_options::EE_SCREENS_GO_DISPLAY_INTRO;
use crate::menus::EE_SCREENS_MAIN_OPEN;
use crate::post_game::{EE_SCREENS_PGS_CLOSE, EE_SCREENS_PGS_OPEN};
use crate::save_load::{EE_SCREENS_SCL_OPEN, EE_SCREENS_SPL_OPEN};
use crate::scenario_screens::{EE_SCREENS_SI_CLOSE, EE_SCREENS_SI_OPEN};
use crate::splash::EE_SCREENS_SS_OPEN;

/// The save and load forms' Cancel (`EEUIECancel`: opens the main game form, or the scenario editor's out of a game).
pub const EE_SCREENS_CPP_CANCEL_LOAD: u32 = 0x0001_0001;
/// The main game form's debug unit reload (`EEUIELoadUnit`).
pub const EE_SCREENS_CPP_LOAD_UNIT: u32 = 0x0001_0002;
/// WON's server select form's open event (`EEUIEWNSOpen`).
pub const EE_SCREENS_CPP_WNS_OPEN: u32 = 0x0001_0003;

/// `kUnitTypeCitizen` (the idle citizen button's selection).
const FACTORY_UNIT_TYPE_CITIZEN: i32 = 0x20;

/// The chat log form's (`EECreateChatLogEvent`): its Close.
fn chat_log_event_for(id: u32) -> u32 {
    match id {
        213 => EE_SCREENS_CL_CLOSE,
        _ => 0,
    }
}

/// The civilization builder's (`EECreateCivilizationBuilderEvent`).
fn civ_builder_event_for(id: u32) -> u32 {
    match id {
        52 => EE_SCREENS_CB_SAVE_NAME_SELECTED,
        62 => EE_SCREENS_CB_CANCEL_NAME,
        65 => EE_SCREENS_CB_SHOW_LOAD,
        66 => EE_SCREENS_CB_CANCEL_LOAD,
        63 => EE_SCREENS_CB_SHOW_SAVE,
        64 => EE_SCREENS_CB_CANCEL_SAVE,
        55 => EE_SCREENS_CB_PURCHASED_SELECTED,
        56 => EE_SCREENS_CB_POTENTIAL_SELECTED,
        57 => EE_SCREENS_CB_PURCHASE,
        58 => EE_SCREENS_CB_RETURNED,
        59 => EE_SCREENS_CB_UPDATE_CONTROLS,
        61 => EE_SCREENS_CB_CANCEL_CHANGES,
        60 => EE_SCREENS_CB_ACCEPT_CHANGES,
        123 => EE_SCREENS_CB_SHOW_CHANGE_NAME,
        54 | 121 => EE_SCREENS_CB_ACCEPT_SAVE,
        53 | 120 => EE_SCREENS_CB_ACCEPT_LOAD,
        51 | 119 => EE_SCREENS_CB_ACCEPT_CHANGE_NAME,
        105 | 118 => EE_SCREENS_CB_DELETE,
        _ => 0,
    }
}

/// The main game form's (`EECreateMainGameEvent`).
fn main_game_event_for(id: u32) -> u32 {
    match id {
        22 => EE_SCREENS_MG_DISPLAY_STATISTICS,
        23 => EE_SCREENS_MG_DISPLAY_DIPLOMACY,
        25 => EE_SCREENS_MG_DISPLAY_CHAT,
        26 => EE_SCREENS_MG_DISPLAY_OPTIONS,
        211 | 24 => EE_SCREENS_MG_DISPLAY_CIV_BUILDER,
        212 | 122 => EE_SCREENS_GO_DISPLAY_INTRO,
        27 => EE_SCREENS_CPP_LOAD_UNIT,
        138 => ee_screens_hot_key_unit_selection(FACTORY_UNIT_TYPE_CITIZEN, true, false, false),
        139 => ee_screens_hot_key_unit_selection(0, false, true, false),
        140 => EE_SCREENS_MG_RALLY_FIGHTER,
        141 => EE_SCREENS_MG_RALLY_BOMBER,
        142 => EE_SCREENS_MG_RALLY_ATOMIC,
        143 => EE_SCREENS_MG_END_SCENARIO_DIALOG,
        185 => EE_SCREENS_MG_TOGGLE_HIDDEN,
        204 => EE_SCREENS_MG_FLARE,
        205 => EE_SCREENS_MG_BEHAVIOR_SCOUT,
        206 => EE_SCREENS_MG_BEHAVIOR_STAND_GROUND,
        207 => EE_SCREENS_MG_BEHAVIOR_GUARD,
        209 => EE_SCREENS_MG_BEHAVIOR_AGGRESSIVE,
        241..=248 => EE_SCREENS_MG_FORMATION + (id - 241),
        249 => EE_SCREENS_MG_PREVIOUS_MESSAGES,
        _ => 0,
    }
}

/// The save and load forms' (`EECreateSaveLoadEvent`): their Cancel (`kControlEventCancelLoad`) is C++'s, the rest
/// as the screen and the editor decide (save_load.rs).
fn save_load_event_for(id: u32, screen_active: bool, in_editor: bool) -> u32 {
    if id == 15 {
        return EE_SCREENS_CPP_CANCEL_LOAD;
    }
    crate::save_load::save_load_event_for(id, screen_active, in_editor)
}

/// The control-event factories in `EEUIManager`'s registration order (the campaign editor's, the chat log's, the
/// civilization builder's, the game settings', the save and load forms', the main game form's, the menus', the
/// post-game summary's, the scenario intro's, the scenario load form's, the single player load form's, the game
/// settings' again, then, after WON's server select (C++), the debug sound panel's): the first answer that is not 0.
pub fn control_event_for(id: c_ulong, screen_active: bool, in_editor: bool) -> u32 {
    // (the database's ids are small; the tables of the original's 32-bit ids take them as u32)
    #[allow(clippy::unnecessary_cast)] // (c_ulong is u64 on LP64)
    let small = id as u32;
    let tables: [&dyn Fn() -> u32; 12] = [
        &|| crate::campaign_editor::event_for_control(id),
        &|| chat_log_event_for(small),
        &|| civ_builder_event_for(small),
        &|| crate::settings_form::event_for_control(id),
        &|| save_load_event_for(small, screen_active, in_editor),
        &|| main_game_event_for(small),
        &|| crate::menus::event_for_control(id),
        &|| crate::post_game::event_for_control(id),
        &|| crate::scenario_screens::scenario_intro_event_for(small),
        &|| crate::save_load::scenario_load_event_for(small),
        &|| crate::save_load::sp_load_event_for(small),
        &|| crate::debug_sound::event_for_control(id),
    ];
    tables.iter().map(|table| table()).find(|&code| code != 0).unwrap_or(0)
}

/// The form-event factory (`EECreateFormEvent`).
pub fn form_event_for(id: c_ulong) -> u32 {
    match id {
        1 => EE_SCREENS_MAIN_OPEN,
        3 => EE_SCREENS_CH_OPEN,
        4 => EE_SCREENS_CH_CLOSE,
        5 | 6 => EE_SCREENS_CH_CANCEL,
        8 => EE_SCREENS_CPP_WNS_OPEN,
        7 => EE_SCREENS_SI_OPEN,
        10 => EE_SCREENS_SI_CLOSE,
        12 => EE_SCREENS_SCL_OPEN,
        11 => EE_SCREENS_SPL_OPEN,
        9 => EE_SCREENS_SS_OPEN,
        13 => EE_SCREENS_PGS_OPEN,
        14 => EE_SCREENS_PGS_CLOSE,
        15 => EE_SCREENS_DS_OPEN,
        16 => EE_SCREENS_CE_OPEN,
        17 => EE_SCREENS_CE_CLOSE,
        18 => EE_SCREENS_CL_OPEN,
        19 | 20 => EE_SCREENS_CL_MOUSE_PRESSED,
        _ => 0,
    }
}

/// `EEUIManager`'s control-event factory: the event code for a control event id (`EE_SCREENS_CPP_*` for a C++
/// class), 0 when no factory of the game's knows it. `screen_active` is `gGameScreenActive`, `in_editor` the
/// manager's scenario editor flag (the save and load forms' kind).
#[no_mangle]
pub extern "C" fn ee_screens_control_event_for(id: c_ulong, screen_active: bool, in_editor: bool) -> u32 {
    control_event_for(id, screen_active, in_editor)
}

/// `EEUIManager`'s form-event factory: the event code for a form event id, 0 for none.
#[no_mangle]
pub extern "C" fn ee_screens_form_event_for(id: c_ulong) -> u32 {
    form_event_for(id)
}

#[cfg(test)]
mod golden;
