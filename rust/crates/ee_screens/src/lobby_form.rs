//! The game lobby (form 26: the random map setup and the multiplayer staging screen) and its two load screens (form
//! 4) whole: the original `EEUIELobbyEvents.cpp` (its 35 event classes, the row, option and load functions the user
//! interface thread calls) over a host ([`LobbyHost`]) whose methods are each one toolkit or game call
//! (`UI Events/EEScreens.cpp`). The rows' and the options' decisions are [`crate::lobby`]'s and
//! [`crate::lobby_options`]'s (rounds 1-3); the lobby options (`gGameOptions`: the slots and options, under their
//! lock), the save files and headers, the server, the messages stay the game's, behind the host.
//!
//! The state the original kept in its event objects and statics is a [`LobbyForm`] record: each slot's type menu
//! (`sPlayerTypeMenus`), each slot's colour button index (the colour events' `mSlot`: the left and right click events
//! of one button always held the same), the load screen's previous load type (`EEUIELECancelLoad::mLoadType`) and the
//! chat line's prefix (`EEUIELEChat::mPrefix`). It is locked only around its own reads and writes, never across a
//! host call.

// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the host's `u32` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_long, c_ulong};
use std::sync::Mutex;

use crate::lobby::{
    self, EeScreensLobbyContext, EeScreensLobbySlot, EE_SCREENS_LOBBY_LINE_CLOSED, EE_SCREENS_LOBBY_LINE_COMPUTER,
    EE_SCREENS_LOBBY_LINE_NAME, EE_SCREENS_LOBBY_LINE_OPEN, EE_SCREENS_LOBBY_TEXT_COMPUTER, EE_SCREENS_LOBBY_TEXT_NAME,
    EE_SCREENS_LOBBY_TEXT_OPEN,
};
use crate::lobby_options::{
    self, EeScreensLobbyOptionState, EeScreensLobbyOptionsView, EeScreensLobbyStartSlot,
    EE_SCREENS_GAME_TYPE_RANDOM_MAP, EE_SCREENS_GAME_TYPE_SAVED_GAME, EE_SCREENS_GAME_TYPE_SCENARIO,
    EE_SCREENS_LOBBY_FLAGS, EE_SCREENS_LOBBY_NO_STRING, EE_SCREENS_LOBBY_ROW_EDIT, EE_SCREENS_LOBBY_ROW_HIDDEN,
    EE_SCREENS_LOBBY_ROW_KEEP, EE_SCREENS_LOBBY_ROW_LABEL, EE_SCREENS_OPTION_CHEAT_CODES, EE_SCREENS_OPTION_COUNT,
    EE_SCREENS_OPTION_CUSTOM_CIVS, EE_SCREENS_OPTION_DIFFICULTY, EE_SCREENS_OPTION_EPOCH_END,
    EE_SCREENS_OPTION_EPOCH_START, EE_SCREENS_OPTION_LOCK_SPEED, EE_SCREENS_OPTION_LOCK_TEAMS,
    EE_SCREENS_OPTION_MAP_SIZE, EE_SCREENS_OPTION_RECORD_GAMES, EE_SCREENS_OPTION_RESOURCES,
    EE_SCREENS_OPTION_REVEAL_MAP, EE_SCREENS_OPTION_SPEED, EE_SCREENS_OPTION_UNIT_LIMIT, EE_SCREENS_OPTION_VARIANT,
    EE_SCREENS_OPTION_WONDERS,
};
use crate::menus::{
    EE_SCREENS_OP_ENTITY_INDEX, EE_SCREENS_OP_OPEN_EVENT, EE_SCREENS_OP_POP, EE_SCREENS_OP_PUSH, EE_SCREENS_OP_SHOW,
    EE_SCREENS_OP_TRIGGER, EE_SCREENS_TEXT_EDIT, EE_SCREENS_TEXT_LABEL, EE_SCREENS_TEXT_MESSAGE,
};
use crate::save_load::{
    EE_SCREENS_OP_CURRENT_FORM_IS, EE_SCREENS_OP_FORM_LIST_SELECTED, EE_SCREENS_TEXT_FILE_LIST,
    EE_SCREENS_TEXT_LOAD_SAVED_NAME, EE_SCREENS_TEXT_LOAD_SCENARIO_NAME, EE_SCREENS_TEXT_SAVED_EXTENSION,
    EE_SCREENS_TEXT_SCENARIO_EXTENSION,
};
use crate::scenario_screens::EE_SCREENS_OP_TAB_STOP;
use crate::settings_form::{
    EE_SCREENS_GAME_IS_SERVER, EE_SCREENS_OP_BACKGROUND, EE_SCREENS_OP_CHECKBOX, EE_SCREENS_OP_CLOSE_FORM,
    EE_SCREENS_OP_COMBO_CLEAR, EE_SCREENS_OP_COMBO_SELECT, EE_SCREENS_OP_GROUP, EE_SCREENS_OP_RAND,
    EE_SCREENS_OP_SET_CHECKBOX, EE_SCREENS_OP_SRAND_TIME, EE_SCREENS_OP_TRIGGER_HERE, EE_SCREENS_TEXT_COMBO_APPEND,
    EE_SCREENS_TEXT_COMBO_INSERT, EE_SCREENS_TEXT_LIST_DATA,
};

// ---- the events (EEScreenEvent::mEvent): form 26's, form 4's lobby screens, the boot question's answers

pub const EE_SCREENS_LE_CANCEL_LOAD: u32 = 0x1a01;
pub const EE_SCREENS_LE_LOAD_SAVED_GAME: u32 = 0x1a02;
pub const EE_SCREENS_LE_OPEN_LOAD_SAVED_GAME: u32 = 0x1a03;
pub const EE_SCREENS_LE_LOAD_SCENARIO: u32 = 0x1a04;
pub const EE_SCREENS_LE_OPEN_LOAD_SCENARIO: u32 = 0x1a05;
pub const EE_SCREENS_LE_CITIZENS: u32 = 0x1a06;
pub const EE_SCREENS_LE_ENGINE: u32 = 0x1a07;
pub const EE_SCREENS_LE_CHAT: u32 = 0x1a08;
pub const EE_SCREENS_LE_CANCEL: u32 = 0x1a09;
pub const EE_SCREENS_LE_START: u32 = 0x1a0a;
pub const EE_SCREENS_LE_COLOR_UP: u32 = 0x1a0b;
pub const EE_SCREENS_LE_COLOR_DOWN: u32 = 0x1a0c;
pub const EE_SCREENS_LE_TEAM: u32 = 0x1a0d;
pub const EE_SCREENS_LE_PLAYER_TYPE: u32 = 0x1a0e;
pub const EE_SCREENS_LE_READY: u32 = 0x1a0f;
pub const EE_SCREENS_LE_VICTORY: u32 = 0x1a10;
pub const EE_SCREENS_LE_GAME_TYPE: u32 = 0x1a11;
pub const EE_SCREENS_LE_MAP_SIZE: u32 = 0x1a12;
pub const EE_SCREENS_LE_MAP_TYPE: u32 = 0x1a13;
pub const EE_SCREENS_LE_RESOURCES: u32 = 0x1a14;
pub const EE_SCREENS_LE_EPOCH_START: u32 = 0x1a15;
pub const EE_SCREENS_LE_EPOCH_END: u32 = 0x1a16;
pub const EE_SCREENS_LE_UNIT_LIMIT: u32 = 0x1a17;
pub const EE_SCREENS_LE_WONDERS: u32 = 0x1a18;
pub const EE_SCREENS_LE_VARIANT: u32 = 0x1a19;
pub const EE_SCREENS_LE_DIFFICULTY: u32 = 0x1a1a;
pub const EE_SCREENS_LE_SPEED: u32 = 0x1a1b;
pub const EE_SCREENS_LE_RECORD_GAMES: u32 = 0x1a1c;
pub const EE_SCREENS_LE_REVEAL_MAP: u32 = 0x1a1d;
pub const EE_SCREENS_LE_CUSTOM_CIVS: u32 = 0x1a1e;
pub const EE_SCREENS_LE_LOCK_TEAMS: u32 = 0x1a1f;
pub const EE_SCREENS_LE_LOCK_SPEED: u32 = 0x1a20;
pub const EE_SCREENS_LE_CHEAT_CODES: u32 = 0x1a21;
pub const EE_SCREENS_LE_CHAT_FOCUS: u32 = 0x1a22;
/// The lobby's open events: a multiplayer host's (`EEAJoinOrHostAGame`), a client's, single player's (the single
/// player screen's Random Map).
pub const EE_SCREENS_LE_ACTIVATE_SERVER: u32 = 0x1a23;
pub const EE_SCREENS_LE_ACTIVATE_CLIENT: u32 = 0x1a24;
pub const EE_SCREENS_LE_ACTIVATE_SINGLE: u32 = 0x1a25;
/// The boot question's answers (no event object: the host's callbacks, the slot's copy held by the host).
pub const EE_SCREENS_LE_BOOT_YES: u32 = 0x1a26;
pub const EE_SCREENS_LE_BOOT_NO: u32 = 0x1a27;

pub fn is_lobby_event(event: u32) -> bool {
    (EE_SCREENS_LE_CANCEL_LOAD..=EE_SCREENS_LE_BOOT_NO).contains(&event)
}

// ---- the host's ops (ee_screenscpp_call on the named form; the shared ones are the earlier rounds')

/// (kind: [`LE_READ_LOCK`] ..) the lobby options' lock.
pub const EE_SCREENS_GAME_LE_LOCK: u32 = 0x1c0;
pub const EE_SCREENS_GAME_LE_IS_CLIENT: u32 = 0x1c1; // -> gCommunications->IsClient()
pub const EE_SCREENS_GAME_LE_PLAYER_INFO: u32 = 0x1c2; // (field, slot, value) EEMPlayerInformation sent
pub const EE_SCREENS_GAME_LE_SEND_OPTION: u32 = 0x1c3; // (option, value) EEMGameOptions(option, value, "") sent
pub const EE_SCREENS_GAME_LE_SET_OPTION: u32 = 0x1c4; // (option, value) the lobby options' field (0: the load type)
pub const EE_SCREENS_GAME_LE_DIFFICULTY_LEVELS: u32 = 0x1c5; // (level) gDifficultyLevel and gDifficultyLevel2
pub const EE_SCREENS_OP_LE_COMBO_DELETE_LINE: u32 = 0x1c6; // (control, line) a combo box's DeleteLine
pub const EE_SCREENS_OP_LE_COMBO_LINES: u32 = 0x1c7; // (control) -> a combo box's list's GetLineCount
pub const EE_SCREENS_OP_LE_TYPE_SELECT: u32 = 0x1c8; // (control, select, line) the line without the change event
pub const EE_SCREENS_OP_LE_HAS_TRIGGER: u32 = 0x1c9; // (control, trigger) -> GetTriggerEvent != NULL
pub const EE_SCREENS_OP_LE_PLAYER_COLOUR: u32 = 0x1ca; // (control, colour) SetBgColor(EEGetPlayerColor(colour))
pub const EE_SCREENS_OP_LE_OPACITY: u32 = 0x1cb; // (control) SetBgOpacity(1.0)
pub const EE_SCREENS_OP_LE_BITMAP: u32 = 0x1cc; // (control, on) a bitmap's SetBitmap(the check down on or off)
pub const EE_SCREENS_OP_LE_NAME_COLOUR: u32 = 0x1cd; // (control, colour, box) the label's (box: its label's) fg
pub const EE_SCREENS_OP_LE_DIRTY: u32 = 0x1ce; // (control) SetDirty(true)
pub const EE_SCREENS_OP_LE_PING: u32 = 0x1cf; // (control, f32 bits) a ping meter's SetPing
pub const EE_SCREENS_OP_LE_FORM_VISIBLE: u32 = 0x1d0; // (form) -> the manager's IsFormVisible
pub const EE_SCREENS_OP_LE_KEYBOARD_EVENT: u32 = 0x1d1; // (code) SetFormKeyboardCapturedEvent (0: none)
pub const EE_SCREENS_OP_LE_KEYBOARD_CAPTURED: u32 = 0x1d2; // -> GetIsKeyboardCaptured
pub const EE_SCREENS_OP_LE_BLINKING: u32 = 0x1d3; // (control) a blinking button's SetIsBlinking(true)
pub const EE_SCREENS_OP_LE_EDIT_DELETE: u32 = 0x1d4; // (control) an edit box's DeleteText()
pub const EE_SCREENS_GAME_LE_DISCONNECT: u32 = 0x1d5; // NEMClientDisconnectionNotification(false) sent
pub const EE_SCREENS_GAME_LE_REMOVE_HOST: u32 = 0x1d6; // EEARemovePlayer(0xfe) posted
pub const EE_SCREENS_GAME_LE_WON_REJOIN: u32 = 0x1d7; // -> WON's room rejoined (true without WON)
pub const EE_SCREENS_GAME_LE_WON_LEAVE: u32 = 0x1d8; // WON's room left (when there is WON)
pub const EE_SCREENS_GAME_LE_START_SERVER: u32 = 0x1d9; // (humans, players) the server closed for the start
pub const EE_SCREENS_GAME_LE_START_LOADING: u32 = 0x1da; // (humans, seed) EEMStartLoading sent
pub const EE_SCREENS_GAME_LE_SAVE_LAST_OPTIONS: u32 = 0x1db; // the preferences' SetLastGameOptions(gGameOptions)
pub const EE_SCREENS_GAME_LE_START_PING: u32 = 0x1dc; // the server's StartPing()
pub const EE_SCREENS_GAME_LE_FULL_PLAYER: u32 = 0x1dd; // (slot) EEMFullPlayerInformation of the slot sent
pub const EE_SCREENS_GAME_LE_RULE: u32 = 0x1df; // (which: LE_RULE_*, argument) -> a value of the rules' tables
pub const EE_SCREENS_GAME_LE_FIND_GENERATOR: u32 = 0x1e0; // (source) -> its map type's generator's line, bit 31: none
pub const EE_SCREENS_GAME_LE_BOOT_FREE: u32 = 0x1e1; // the boot question's slot copy freed
pub const EE_SCREENS_GAME_LE_BOOT_REFUSE: u32 = 0x1e2; // (type) WON's counts and the server's RefuseClient(copy)
pub const EE_SCREENS_GAME_LE_XFER: u32 = 0x1e3; // (slot, old id) the old transfer aborted, the header's file sent -> id
pub const EE_SCREENS_GAME_LE_HEADER_PLAYERS: u32 = 0x1e4; // -> the loaded header's mNumPlayers
pub const EE_SCREENS_GAME_LE_APPLY_SAVED: u32 = 0x1e5; // the loaded header's options applied, EEMFullGameOptions sent
/// The lobby options copied (their assignment checks each value) with load type 0, and back.
pub const EE_SCREENS_GAME_LE_RANDOM_MAP_OPTIONS: u32 = 0x1e6;
/// Texts (ee_screenscpp_text / _put_text).
pub const EE_SCREENS_TEXT_LE_MAP_TYPE: u32 = 0x60; // (source) the options' mMapType; put: the lobby's
pub const EE_SCREENS_TEXT_LE_SEND_MAP_TYPE: u32 = 0x61; // put (line): EEMGameOptions(2, line, name) sent
pub const EE_SCREENS_TEXT_LE_CHAT: u32 = 0x62; // put: EEMChat(0, text) sent
pub const EE_SCREENS_TEXT_LE_GENERATOR_DISPLAY: u32 = 0x63; // (line) a generator's display name
pub const EE_SCREENS_TEXT_LE_MAP_DISPLAY: u32 = 0x64; // (source) its map type's generator display name
pub const EE_SCREENS_TEXT_LE_GENERATOR_NAME: u32 = 0x65; // (line) a generator's name
pub const EE_SCREENS_TEXT_LE_ENGINE: u32 = 0x66; // (choice) an engine's text (Classic, Random, an AI's name)
pub const EE_SCREENS_TEXT_LE_CHECK_LOAD: u32 = 0x67; // put (file, directory): the start's load check -> LE_LOAD_*
pub const EE_SCREENS_TEXT_LE_OPEN_HEADER: u32 = 0x68; // put (file, directory; load type): the load's -> LE_LOAD_*
pub const EE_SCREENS_TEXT_LE_MISSING: u32 = 0x69; // the last check's missing lines, each ended by a line break
pub const EE_SCREENS_TEXT_LE_BOOT_QUESTION: u32 = 0x6a; // put: the boot question on the slot copy
pub const EE_SCREENS_TEXT_LE_CHECK_LABEL: u32 = 0x6b; // (control) a check box's text label's first line; none: missing
/// The lock's kinds.
pub const EE_SCREENS_LE_READ_LOCK: u32 = 0;
pub const EE_SCREENS_LE_READ_UNLOCK: u32 = 1;
pub const EE_SCREENS_LE_WRITE_LOCK: u32 = 2;
pub const EE_SCREENS_LE_WRITE_UNLOCK: u32 = 3;
/// A load check's or header's outcome.
pub const EE_SCREENS_LE_LOAD_NOT_FOUND: u32 = 0;
pub const EE_SCREENS_LE_LOAD_READ_ERROR: u32 = 1;
pub const EE_SCREENS_LE_LOAD_MISMATCH: u32 = 2;
pub const EE_SCREENS_LE_LOAD_OK: u32 = 3;
/// The slots a record comes from: the lobby options', the boot question's copy, the loaded header's.
pub const EE_SCREENS_LE_SOURCE_LOBBY: u32 = 0;
pub const EE_SCREENS_LE_SOURCE_BOOT: u32 = 1;
pub const EE_SCREENS_LE_SOURCE_HEADER: u32 = 2;
/// The game type's copy of the options (random map: shown as the copy, which keeps a loaded game's map type).
pub const EE_SCREENS_LE_SOURCE_COPY: u32 = 3;
/// [`EE_SCREENS_GAME_LE_RULE`]'s tables.
pub const EE_SCREENS_LE_RULE_EPOCHS: u32 = 0;
pub const EE_SCREENS_LE_RULE_UNIT_MINIMUM: u32 = 1;
pub const EE_SCREENS_LE_RULE_UNIT_MAXIMUM: u32 = 2;
pub const EE_SCREENS_LE_RULE_UNIT_STEP: u32 = 3;
pub const EE_SCREENS_LE_RULE_WONDERS_MAXIMUM: u32 = 4;
pub const EE_SCREENS_LE_RULE_MAP_SIZES: u32 = 5;
pub const EE_SCREENS_LE_RULE_MAP_SIZE_NAME: u32 = 6;
pub const EE_SCREENS_LE_RULE_DIFFICULTIES: u32 = 7;
pub const EE_SCREENS_LE_RULE_DIFFICULTY_NAME: u32 = 8;
pub const EE_SCREENS_LE_RULE_SPEEDS: u32 = 9;
pub const EE_SCREENS_LE_RULE_SPEED_NAME: u32 = 10;
pub const EE_SCREENS_LE_RULE_VARIANTS: u32 = 11;
pub const EE_SCREENS_LE_RULE_VARIANT_SELECTABLE: u32 = 12;
pub const EE_SCREENS_LE_RULE_VARIANT_NAME: u32 = 13;
pub const EE_SCREENS_LE_RULE_RESOURCE_LEVELS: u32 = 14;
pub const EE_SCREENS_LE_RULE_RESOURCE_LEVEL_NAME: u32 = 15;
pub const EE_SCREENS_LE_RULE_STARTING_RESOURCES_NAME: u32 = 16;
pub const EE_SCREENS_LE_RULE_EPOCH_TITLE: u32 = 17;
pub const EE_SCREENS_LE_RULE_GENERATORS: u32 = 18;
pub const EE_SCREENS_LE_RULE_ENGINES: u32 = 19;
pub const EE_SCREENS_LE_RULE_DEFAULT_COLOUR: u32 = 20;
pub const EE_SCREENS_LE_RULE_STARTING_CITIZENS: u32 = 21;

/// The forms a host call names (the event's: [`LE_FORM_EVENT`]).
pub const LE_FORM_LOBBY: u32 = 26;
pub const LE_FORM_LOAD: u32 = 4;
pub const LE_FORM_EVENT: u32 = u32::MAX;

// the controls (ee/EEUIManager.h), triggers (lle/UIControl.h), backgrounds and strings (ee/EEStringIDs.h)
const LE_READY_LABEL: u32 = 1;
const LE_PING_LABEL: u32 = 6;
const LE_CHAT_MESSAGES: u32 = 8;
const LE_CHAT_BOX: u32 = 9;
const LE_START: u32 = 10;
const LE_CANCEL: u32 = 11;
const LE_READY: u32 = 12; // + slot (the check boxes; 233 + slot their bitmaps)
const LE_SCENARIO_INFO_LABEL: u32 = 28;
const LE_SCENARIO_INFO: u32 = 29;
const LE_TYPE: u32 = 30; // + slot
const LE_NAME: u32 = 46;
const LE_TEAM: u32 = 62;
const LE_COLOUR: u32 = 78;
const LE_TEAM_LABEL: u32 = 94;
const LE_COLOUR_LABEL: u32 = 110;
const LE_CITIZENS: u32 = 126;
const LE_CITIZENS_LABEL: u32 = 142;
const LE_PING: u32 = 158;
const LE_READY_BITMAP: u32 = 233;
const LE_TRANSFER: u32 = 256;
const LE_GAME_TYPE: u32 = 175;
const LE_GAME_TYPE_LABEL: u32 = 194;
const LE_MAP_SIZE_LABEL0: u32 = 176;
const LE_MAP_SIZE: u32 = 177;
const LE_MAP_SIZE_LABEL: u32 = 195;
const LE_MAP_TYPE_LABEL0: u32 = 178;
const LE_MAP_TYPE: u32 = 179;
const LE_MAP_TYPE_LABEL: u32 = 196;
const LE_RESOURCES: u32 = 181;
const LE_RESOURCES_LABEL: u32 = 197;
const LE_EPOCH_START: u32 = 183;
const LE_EPOCH_START_LABEL: u32 = 198;
const LE_EPOCH_END: u32 = 185;
const LE_EPOCH_END_LABEL: u32 = 199;
const LE_UNIT_LIMIT: u32 = 187;
const LE_UNIT_LIMIT_LABEL: u32 = 200;
const LE_WONDERS: u32 = 189;
const LE_WONDERS_LABEL: u32 = 201;
const LE_VARIANT: u32 = 205;
const LE_VARIANT_LABEL: u32 = 221;
const LE_DIFFICULTY: u32 = 207;
const LE_DIFFICULTY_LABEL: u32 = 222;
const LE_SPEED: u32 = 209;
const LE_SPEED_LABEL: u32 = 223;
const LE_CHECK_RECORD: u32 = 214;
const LE_CHECK_REVEAL: u32 = 215;
const LE_CHECK_CUSTOM: u32 = 216;
const LE_CHECK_LOCK_TEAMS: u32 = 217;
const LE_CHECK_LOCK_SPEED: u32 = 218;
const LE_CHECK_CHEATS: u32 = 219;
const LE_CHECK_VICTORY: u32 = 220;
const LE_BITMAP_RECORD: u32 = 226;
const LE_BITMAP_REVEAL: u32 = 227;
const LE_BITMAP_CUSTOM: u32 = 228;
const LE_BITMAP_LOCK_TEAMS: u32 = 229;
const LE_BITMAP_LOCK_SPEED: u32 = 230;
const LE_BITMAP_CHEATS: u32 = 231;
const LE_BITMAP_VICTORY: u32 = 232;
const LE_LABEL_RECORD: u32 = 249;
const LE_LABEL_REVEAL: u32 = 250;
const LE_LABEL_CUSTOM: u32 = 251;
const LE_LABEL_LOCK_TEAMS: u32 = 252;
const LE_LABEL_LOCK_SPEED: u32 = 253;
const LE_LABEL_CHEATS: u32 = 254;
const LE_LABEL_VICTORY: u32 = 255;
const LE_LAG: u32 = 273;
const LE_ENGINE_HEADER: u32 = 274;
const LE_ENGINE: u32 = 275; // + slot
const LE_ENGINE_LABEL: u32 = 291;
const LE_LOAD_LOAD: u32 = 1;
const LE_LOAD_CANCEL: u32 = 2;
const LE_LOAD_LIST: u32 = 4;
const LE_LOAD_TITLE: u32 = 6;
const LE_TRIGGER_CHECKED: u32 = 0x3e9;
const LE_TRIGGER_UNCHECKED: u32 = 0x3ea;
const LE_TRIGGER_SELECTION: u32 = 0x3eb;
const LE_TRIGGER_DOUBLE_CLICK: u32 = 0x3ec;
const LE_TRIGGER_BUTTON_UP: u32 = 0x3f0;
const LE_TRIGGER_ENTER: u32 = 0x3f1;
const LE_TRIGGER_RIGHT_UP: u32 = 0x3f6;
const LE_BACKGROUND_MULTI: u32 = 0x89;
const LE_BACKGROUND_SINGLE: u32 = 0xa2;
const LE_SHOWN: u32 = 1;
const LE_HIDDEN: u32 = 0;
const LE_IDS_PLAY_SCENARIO: u32 = 7082;
const LE_IDS_COLOR_TAKEN: u32 = 10003;
const LE_IDS_BOOT: u32 = 10006;
const LE_IDS_NOT_FOUND: u32 = 10019;
const LE_IDS_READ_ERROR: u32 = 10036;
const LE_IDS_OPEN: u32 = 30240;
const LE_IDS_COMPUTER: u32 = 30241;
const LE_IDS_CLOSED: u32 = 30242;
const LE_IDS_OBSERVE: u32 = 30250;
const LE_IDS_OFF: u32 = 30280;
const LE_IDS_ENGINE_HEADER: u32 = 33173;
const LE_IDS_ENGINE_PING_HEADER: u32 = 33174;
/// The game type box's lines (random map, saved game, scenario) and the start check's messages in
/// `EE_SCREENS_START_*`'s order.
const LE_IDS_GAME_TYPES: [u32; 3] = [31258, 31260, 31259];
const LE_IDS_START: [u32; 7] = [10058, 10009, 10063, 10016, 30411, 10041, 10052];
/// `EEMPlayerInformation`'s fields; `LobbyShowPlayer`'s flags.
const LE_INFO_TYPE: u32 = 0;
const LE_INFO_COLOUR: u32 = 2;
const LE_INFO_TEAM: u32 = 3;
const LE_INFO_CITIZENS: u32 = 4;
const LE_INFO_READY: u32 = 5;
const LE_INFO_ENGINE: u32 = 6;
const LE_SHOW_ALL: u32 = 8;
const LE_OPTION_LOAD_TYPE: u32 = 0;
const LE_OPTION_VICTORY: u32 = 17;
const LE_COMPUTER: c_ulong = 0;
const LE_HUMAN: c_ulong = 1;
const LE_OBSERVER: c_ulong = 2;
const LE_OPEN: c_ulong = 3;
const LE_CLOSED: c_ulong = 4;
const LE_LOAD_ERROR: c_long = 9;

/// A lobby slot (`EEGameOptionsPlayer`) but its name.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensLobbyPlayerRecord {
    pub kind: c_ulong,
    pub loaded_type: c_long,
    pub ping: f32,
    pub slot: c_ulong,
    pub color: c_ulong,
    pub team: c_ulong,
    pub citizens: c_ulong,
    pub transfer: c_ulong,
    pub is_local: bool,
    pub is_ready: bool,
    pub b36: bool,
    pub engine: c_ulong,
}

/// The lobby options' (`EEGameOptions`) values the lobby reads, but the map type.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensLobbyOptionsRecord {
    pub load_type: c_ulong,
    pub map_size: c_ulong,
    pub resources: c_ulong,
    pub epoch_start: c_long,
    pub epoch_end: c_long,
    pub unit_limit: c_ulong,
    pub wonders: c_ulong,
    pub variant: c_ulong,
    pub difficulty: c_ulong,
    pub speed: c_ulong,
    pub multiplayer: bool,
    pub record_games: bool,
    pub custom_civs: bool,
    pub reveal_map: bool,
    pub lock_teams: bool,
    pub lock_speed: bool,
    pub cheat_codes: bool,
    pub victory: bool,
}

/// A slot with its name (UTF-8).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct LobbyPlayer {
    pub r: EeScreensLobbyPlayerRecord,
    pub name: Vec<u8>,
}

/// The lobby's state (see the module).
#[derive(Debug)]
pub struct LobbyForm {
    pub menus: [c_long; 16],
    pub colours: [c_ulong; 16],
    pub cancel_load_type: c_ulong,
    pub chat_prefix: Vec<u8>,
}

impl LobbyForm {
    pub const fn new() -> Self {
        LobbyForm { menus: [0; 16], colours: [0; 16], cancel_load_type: 0, chat_prefix: Vec::new() }
    }
}

impl Default for LobbyForm {
    fn default() -> Self {
        Self::new()
    }
}

/// The toolkit's and the game's calls of the lobby. `form` names the form a call acts on ([`LE_FORM_EVENT`]: the
/// event's). Texts are the game's UTF-8 bytes.
pub trait LobbyHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` on `form` with up to four arguments and an answer.
    fn call(&mut self, form: u32, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32;
    fn text(&mut self, form: u32, which: u32, arg: u32) -> Vec<u8>;
    /// A text of the game, `None` when the host says it is missing.
    fn text_checked(&mut self, form: u32, which: u32, arg: u32) -> Option<Vec<u8>>;
    /// A text into the game; the host's answer.
    fn put_text(&mut self, form: u32, which: u32, arg: u32, text: &[u8]) -> u32;
    /// A resource string.
    fn string(&mut self, id: u32) -> Vec<u8>;
    /// A combo box's `GetCurrentSelection`: whether it has one, and the value it wrote either way.
    fn selection(&mut self, form: u32, control: u32) -> (bool, u32);
    fn player(&mut self, source: u32, index: u32) -> LobbyPlayer;
    fn put_player(&mut self, source: u32, index: u32, player: &LobbyPlayer);
    fn options(&mut self, source: u32) -> EeScreensLobbyOptionsRecord;
}

// ---- small helpers over the host

fn op<H: LobbyHost + ?Sized>(h: &mut H, form: u32, op: u32, a: u32, b: u32) -> u32 {
    h.call(form, op, a, b, 0, 0)
}
fn show<H: LobbyHost + ?Sized>(h: &mut H, form: u32, control: u32, mode: u32) {
    op(h, form, EE_SCREENS_OP_SHOW, control, mode);
}
fn label<H: LobbyHost + ?Sized>(h: &mut H, form: u32, control: u32, text: &[u8]) {
    h.put_text(form, EE_SCREENS_TEXT_LABEL, control, text);
}
fn select<H: LobbyHost + ?Sized>(h: &mut H, form: u32, control: u32, line: u32) {
    op(h, form, EE_SCREENS_OP_COMBO_SELECT, control, line);
}
fn append<H: LobbyHost + ?Sized>(h: &mut H, form: u32, control: u32, text: &[u8]) {
    h.put_text(form, EE_SCREENS_TEXT_COMBO_APPEND, control, text);
}
/// A control's trigger event (0: none) on a named form or on the event's.
fn trigger<H: LobbyHost + ?Sized>(h: &mut H, form: u32, control: u32, kind: u32, event: u32) {
    if form == LE_FORM_EVENT {
        h.call(form, EE_SCREENS_OP_TRIGGER_HERE, control, kind, event, 0);
    } else {
        h.call(form, EE_SCREENS_OP_TRIGGER, form, control, kind, event);
    }
}
fn lock<H: LobbyHost + ?Sized>(h: &mut H, kind: u32) {
    op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_LOCK, kind, 0);
}
fn rule<H: LobbyHost + ?Sized>(h: &mut H, which: u32, arg: u32) -> u32 {
    op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_RULE, which, arg)
}
fn is_server<H: LobbyHost + ?Sized>(h: &mut H) -> bool {
    op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_IS_SERVER, 0, 0) != 0
}
fn message<H: LobbyHost + ?Sized>(h: &mut H, text: &[u8]) {
    h.put_text(LE_FORM_LOBBY, EE_SCREENS_TEXT_MESSAGE, 1, text);
}
fn player_info<H: LobbyHost + ?Sized>(h: &mut H, field: u32, slot: c_ulong, value: u32) {
    h.call(LE_FORM_LOBBY, EE_SCREENS_GAME_LE_PLAYER_INFO, field, slot as u32, value, 0);
}
fn lobby_player<H: LobbyHost + ?Sized>(h: &mut H, index: c_ulong) -> LobbyPlayer {
    h.player(EE_SCREENS_LE_SOURCE_LOBBY, index as u32)
}
fn put_lobby_player<H: LobbyHost + ?Sized>(h: &mut H, index: c_ulong, p: &LobbyPlayer) {
    h.put_player(EE_SCREENS_LE_SOURCE_LOBBY, index as u32, p);
}
fn number(value: i64) -> Vec<u8> {
    value.to_string().into_bytes()
}
/// `sprintf("%ld", x)` of an `unsigned long` (a Windows `long`: 32 bits, signed).
fn number_u(value: c_ulong) -> Vec<u8> {
    number(value as u32 as i32 as i64)
}
/// An epoch's title string.
fn epoch_title<H: LobbyHost + ?Sized>(h: &mut H, epoch: u32) -> Vec<u8> {
    let id = rule(h, EE_SCREENS_LE_RULE_EPOCH_TITLE, epoch);
    h.string(id)
}
/// The options' value for each EEMGameOptions option number.
fn current_options(o: &EeScreensLobbyOptionsRecord) -> [c_long; EE_SCREENS_OPTION_COUNT] {
    let mut c = [0 as c_long; EE_SCREENS_OPTION_COUNT];
    c[EE_SCREENS_OPTION_MAP_SIZE as usize] = o.map_size as c_long;
    c[EE_SCREENS_OPTION_RESOURCES as usize] = o.resources as c_long;
    c[EE_SCREENS_OPTION_EPOCH_START as usize] = o.epoch_start;
    c[EE_SCREENS_OPTION_EPOCH_END as usize] = o.epoch_end;
    c[EE_SCREENS_OPTION_UNIT_LIMIT as usize] = o.unit_limit as c_long;
    c[EE_SCREENS_OPTION_WONDERS as usize] = o.wonders as c_long;
    c[EE_SCREENS_OPTION_VARIANT as usize] = o.variant as c_long;
    c[EE_SCREENS_OPTION_DIFFICULTY as usize] = o.difficulty as c_long;
    c[EE_SCREENS_OPTION_SPEED as usize] = o.speed as c_long;
    c[EE_SCREENS_OPTION_RECORD_GAMES as usize] = o.record_games as c_long;
    c[EE_SCREENS_OPTION_CUSTOM_CIVS as usize] = o.custom_civs as c_long;
    c[EE_SCREENS_OPTION_REVEAL_MAP as usize] = o.reveal_map as c_long;
    c[EE_SCREENS_OPTION_LOCK_TEAMS as usize] = o.lock_teams as c_long;
    c[EE_SCREENS_OPTION_LOCK_SPEED as usize] = o.lock_speed as c_long;
    c[EE_SCREENS_OPTION_CHEAT_CODES as usize] = o.cheat_codes as c_long;
    c
}
/// The game variants' selectable flags (the variant box's lines).
fn selectable_variants<H: LobbyHost + ?Sized>(h: &mut H) -> Vec<bool> {
    let n = rule(h, EE_SCREENS_LE_RULE_VARIANTS, 0);
    (0..n).map(|i| rule(h, EE_SCREENS_LE_RULE_VARIANT_SELECTABLE, i) != 0).collect()
}

/// A slot and the lobby's context as the row decisions read them.
fn slot_view(p: &LobbyPlayer) -> EeScreensLobbySlot {
    EeScreensLobbySlot {
        kind: p.r.kind,
        loaded_type: p.r.loaded_type,
        slot: p.r.slot,
        is_local: p.r.is_local,
        is_ready: p.r.is_ready,
        b36: p.r.b36,
    }
}
fn context_view<H: LobbyHost + ?Sized>(h: &mut H, host: &LobbyPlayer) -> EeScreensLobbyContext {
    let server = is_server(h);
    let o = h.options(EE_SCREENS_LE_SOURCE_LOBBY);
    EeScreensLobbyContext { server, multiplayer: o.multiplayer, load_type: o.load_type, host_ready: host.r.is_ready }
}

// ---- the rows

/// `LobbyShowPlayer`: part of a slot's row (`flags`: 0 type and name, 1 ping, 2 colour, 3 team, 4 citizens, 5 ready,
/// 6 the engine, 8 all); the chat prefix the local player's name.
pub fn show_player<H: LobbyHost + ?Sized>(
    h: &mut H,
    s: &Mutex<LobbyForm>,
    p: &LobbyPlayer,
    host: &LobbyPlayer,
    flags: u32,
) {
    const F: u32 = LE_FORM_LOBBY;
    if op(h, F, EE_SCREENS_OP_LE_FORM_VISIBLE, F, 0) == 0 {
        return;
    }
    let c = context_view(h, host);
    let plan = lobby::player_plan(&slot_view(p), &c);
    if p.r.is_local && h.options(EE_SCREENS_LE_SOURCE_LOBBY).multiplayer {
        s.lock().unwrap().chat_prefix = p.name.clone();
    }
    let slot = p.r.slot as u32;
    if flags == 0 || flags == LE_SHOW_ALL {
        if is_server(h) {
            show_row(h, s, true, p, host);
        } else {
            show_row(h, s, false, p, host);
        }
        show_name(h, p, host);
    }
    if flags == LE_INFO_READY || flags == LE_SHOW_ALL {
        show(h, F, slot.wrapping_add(LE_READY), plan.ready[0] as u32);
        show(h, F, slot.wrapping_add(LE_READY_BITMAP), plan.ready[1] as u32);
        op(h, F, EE_SCREENS_OP_SET_CHECKBOX, slot.wrapping_add(LE_READY), p.r.is_ready as u32);
        op(h, F, EE_SCREENS_OP_LE_BITMAP, slot.wrapping_add(LE_READY_BITMAP), p.r.is_ready as u32);
    }
    if flags == LE_INFO_COLOUR || flags == LE_SHOW_ALL {
        let colour = p.r.color as u32;
        show(h, F, slot.wrapping_add(LE_COLOUR), plan.colour[0] as u32);
        show(h, F, slot.wrapping_add(LE_COLOUR_LABEL), plan.colour[1] as u32);
        op(h, F, EE_SCREENS_OP_LE_OPACITY, slot.wrapping_add(LE_COLOUR), 0);
        op(h, F, EE_SCREENS_OP_LE_PLAYER_COLOUR, slot.wrapping_add(LE_COLOUR), colour);
        if op(h, F, EE_SCREENS_OP_LE_HAS_TRIGGER, slot.wrapping_add(LE_COLOUR), LE_TRIGGER_RIGHT_UP) != 0 {
            if let Some(c) = s.lock().unwrap().colours.get_mut(slot as usize) {
                *c = p.r.color;
            }
        }
        op(h, F, EE_SCREENS_OP_LE_OPACITY, slot.wrapping_add(LE_COLOUR_LABEL), 0);
        op(h, F, EE_SCREENS_OP_LE_PLAYER_COLOUR, slot.wrapping_add(LE_COLOUR_LABEL), colour);
    }
    if flags == LE_INFO_TEAM || flags == LE_SHOW_ALL {
        show(h, F, slot.wrapping_add(LE_TEAM), plan.team[0] as u32);
        show(h, F, slot.wrapping_add(LE_TEAM_LABEL), plan.team[1] as u32);
        let text = if p.r.team == 0 { b"-".to_vec() } else { number_u(p.r.team) };
        let team = lobby_player(h, p.r.slot).r.team as u32;
        let (ok, selection) = h.selection(F, slot.wrapping_add(LE_TEAM));
        if !ok || selection != team {
            select(h, F, slot.wrapping_add(LE_TEAM), team);
        }
        label(h, F, slot.wrapping_add(LE_TEAM_LABEL), &text);
    }
    if flags == LE_INFO_CITIZENS || flags == LE_SHOW_ALL {
        show(h, F, slot.wrapping_add(LE_CITIZENS), plan.citizens[0] as u32);
        show(h, F, slot.wrapping_add(LE_CITIZENS_LABEL), plan.citizens[1] as u32);
        select(h, F, slot.wrapping_add(LE_CITIZENS), (p.r.citizens as u32).wrapping_sub(1));
        label(h, F, slot.wrapping_add(LE_CITIZENS_LABEL), &number_u(p.r.citizens));
    }
    if flags == 0 || flags == LE_INFO_ENGINE || flags == LE_SHOW_ALL {
        show_engine(h, p, host);
    }
    if flags != 1 && flags != LE_SHOW_ALL {
        return;
    }
    show(h, F, slot.wrapping_add(LE_PING), plan.ping as u32);
    op(h, F, EE_SCREENS_OP_LE_PING, slot.wrapping_add(LE_PING), p.r.ping.to_bits());
}

/// `LobbyShowHostRow` (`host_view`) or `LobbyShowClientRow`: the type box (refilled when its menu changes), the name
/// label's text, which controls are editable.
fn show_row<H: LobbyHost + ?Sized>(
    h: &mut H,
    s: &Mutex<LobbyForm>,
    host_view: bool,
    p: &LobbyPlayer,
    host: &LobbyPlayer,
) {
    const F: u32 = LE_FORM_LOBBY;
    let c = context_view(h, host);
    let v = slot_view(p);
    let row = if host_view { lobby::host_row(&v, &c) } else { lobby::client_row(&v, &c) };
    let slot = p.r.slot as usize;
    let menu = s.lock().unwrap().menus[slot];
    if menu != row.menu {
        fill_player_types(h, row.menu, p, (slot as u32).wrapping_add(LE_TYPE));
        s.lock().unwrap().menus[slot] = row.menu;
    }
    let name = (slot as u32).wrapping_add(LE_NAME);
    let text = match row.text {
        EE_SCREENS_LOBBY_TEXT_COMPUTER => h.string(LE_IDS_COMPUTER),
        EE_SCREENS_LOBBY_TEXT_NAME => p.name.clone(),
        EE_SCREENS_LOBBY_TEXT_OPEN => h.string(LE_IDS_OPEN),
        _ => h.string(LE_IDS_CLOSED),
    };
    label(h, F, name, &text);
    show_row_controls(h, p.r.slot as u32, row.controls);
}

/// `LobbyFillPlayerTypes`: the type box's lines of a menu, then the slot's type selected.
fn fill_player_types<H: LobbyHost + ?Sized>(h: &mut H, menu: c_long, p: &LobbyPlayer, control: u32) {
    const F: u32 = LE_FORM_LOBBY;
    op(h, F, EE_SCREENS_OP_COMBO_CLEAR, control, 0);
    for &line in lobby::player_type_lines(menu) {
        let text = match line {
            EE_SCREENS_LOBBY_LINE_NAME => p.name.clone(),
            EE_SCREENS_LOBBY_LINE_OPEN => h.string(LE_IDS_OPEN),
            EE_SCREENS_LOBBY_LINE_COMPUTER => h.string(LE_IDS_COMPUTER),
            EE_SCREENS_LOBBY_LINE_CLOSED => h.string(LE_IDS_CLOSED),
            _ => h.string(LE_IDS_OBSERVE),
        };
        append(h, F, control, &text);
    }
    // LobbySelectPlayerType: the line without firing the box's change event
    let line = lobby::player_type_line(menu, p.r.kind);
    h.call(F, EE_SCREENS_OP_LE_TYPE_SELECT, control, line.is_some() as u32, line.unwrap_or(0) as u32, 0);
}

/// `LobbyShowRowControls`: the ready, team, colour and bonus pairs (label, control); an editable team box gets its
/// change event.
fn show_row_controls<H: LobbyHost + ?Sized>(h: &mut H, slot: u32, controls: [c_ulong; 4]) {
    const F: u32 = LE_FORM_LOBBY;
    let pairs = [
        (LE_READY_BITMAP, LE_READY),
        (LE_TEAM_LABEL, LE_TEAM),
        (LE_COLOUR_LABEL, LE_COLOUR),
        (LE_CITIZENS_LABEL, LE_CITIZENS),
    ];
    for (k, (first, second)) in pairs.into_iter().enumerate() {
        let pair = lobby::row_pair(controls[k]);
        show(h, F, slot.wrapping_add(first), pair[0] as u32);
        show(h, F, slot.wrapping_add(second), pair[1] as u32);
        if k == 1 {
            let event = if controls[1] == 1 { EE_SCREENS_LE_TEAM } else { 0 };
            trigger(h, F, slot.wrapping_add(LE_TEAM), LE_TRIGGER_SELECTION, event);
        }
    }
}

/// `LobbyShowName`: the type box or the name label, the name's colour.
fn show_name<H: LobbyHost + ?Sized>(h: &mut H, p: &LobbyPlayer, host: &LobbyPlayer) {
    const F: u32 = LE_FORM_LOBBY;
    let c = context_view(h, host);
    let plan = lobby::name_plan(&slot_view(p), &c);
    let slot = p.r.slot as u32;
    show(h, F, slot.wrapping_add(LE_TYPE), plan.box_mode as u32);
    show(h, F, slot.wrapping_add(LE_NAME), plan.label_mode as u32);
    let colour = plan.colour & 3;
    if plan.box_dirty {
        h.call(F, EE_SCREENS_OP_LE_NAME_COLOUR, slot.wrapping_add(LE_TYPE), colour, 1, 0);
    }
    h.call(F, EE_SCREENS_OP_LE_NAME_COLOUR, slot.wrapping_add(LE_NAME), colour, 0, 0);
    if plan.box_dirty {
        op(h, F, EE_SCREENS_OP_LE_DIRTY, slot.wrapping_add(LE_TYPE), 0);
    }
    op(h, F, EE_SCREENS_OP_LE_DIRTY, slot.wrapping_add(LE_NAME), 0);
}

/// `LobbyFillEngineBox`: the engines past the box's lines appended.
fn fill_engine_box<H: LobbyHost + ?Sized>(h: &mut H, control: u32) {
    let count = rule(h, EE_SCREENS_LE_RULE_ENGINES, 0);
    let mut k = op(h, LE_FORM_LOBBY, EE_SCREENS_OP_LE_COMBO_LINES, control, 0);
    while k < count {
        let text = h.text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_ENGINE, k);
        append(h, LE_FORM_LOBBY, control, &text);
        k += 1;
    }
}

/// `LobbyShowEngine`: a computer row's engine, the host's box or the label.
fn show_engine<H: LobbyHost + ?Sized>(h: &mut H, p: &LobbyPlayer, host: &LobbyPlayer) {
    const F: u32 = LE_FORM_LOBBY;
    if p.r.slot >= 16 {
        return;
    }
    let slot = p.r.slot as u32;
    let count = rule(h, EE_SCREENS_LE_RULE_ENGINES, 0);
    let engine = lobby::engine_choice(p.r.engine, count as c_ulong) as u32;
    fill_engine_box(h, slot + LE_ENGINE);
    let c = context_view(h, host);
    let plan = lobby::player_plan(&slot_view(p), &c);
    show(h, F, slot + LE_ENGINE, plan.engine[0] as u32);
    show(h, F, slot + LE_ENGINE_LABEL, plan.engine[1] as u32);
    let (ok, selection) = h.selection(F, slot + LE_ENGINE);
    if !ok || selection != engine {
        select(h, F, slot + LE_ENGINE, engine);
    }
    let text = h.text(F, EE_SCREENS_TEXT_LE_ENGINE, engine);
    label(h, F, slot + LE_ENGINE_LABEL, &text);
}

/// `LobbyUpdatePlayer`: the given parts of one slot's row from the lobby options.
pub fn update_player<H: LobbyHost + ?Sized>(h: &mut H, s: &Mutex<LobbyForm>, slot: c_long, flags: c_long) {
    lock(h, EE_SCREENS_LE_READ_LOCK);
    let p = lobby_player(h, slot as c_ulong);
    let host = lobby_player(h, 0);
    show_player(h, s, &p, &host, flags as u32);
    lock(h, EE_SCREENS_LE_READ_UNLOCK);
}

// ---- the game options

/// A box shown and its label hidden, or the other way round.
fn show_option_row<H: LobbyHost + ?Sized>(h: &mut H, edit: bool, control: u32, text: u32) {
    show(h, LE_FORM_LOBBY, control, if edit { LE_SHOWN } else { LE_HIDDEN });
    show(h, LE_FORM_LOBBY, text, if edit { LE_HIDDEN } else { LE_SHOWN });
}
/// A map row (the leading label too) hidden, editable or a label.
fn show_map_row<H: LobbyHost + ?Sized>(h: &mut H, row: u32, label0: u32, control: u32, text: u32) {
    if row == EE_SCREENS_LOBBY_ROW_HIDDEN {
        show(h, LE_FORM_LOBBY, label0, LE_HIDDEN);
        show(h, LE_FORM_LOBBY, control, LE_HIDDEN);
        show(h, LE_FORM_LOBBY, text, LE_HIDDEN);
    } else {
        show(h, LE_FORM_LOBBY, label0, LE_SHOWN);
        show_option_row(h, row == EE_SCREENS_LOBBY_ROW_EDIT, control, text);
    }
}

/// `LobbyShowGameOptions`: the options of `source` (the lobby's, or the game type's copy) as editable boxes for the
/// host before ready (random maps only) or labels; record games and victory stay hidden.
pub fn show_game_options<H: LobbyHost + ?Sized>(h: &mut H, source: u32, host: &LobbyPlayer) {
    const F: u32 = LE_FORM_LOBBY;
    let o = &h.options(source);
    let g = h.options(EE_SCREENS_LE_SOURCE_LOBBY);
    let ready = host.r.is_ready;
    let server = is_server(h);
    if op(h, F, EE_SCREENS_OP_CURRENT_FORM_IS, F, 0) == 0 {
        return;
    }
    const FLAGS: [(u32, u32, u32); EE_SCREENS_LOBBY_FLAGS] = [
        (LE_CHECK_RECORD, LE_BITMAP_RECORD, LE_LABEL_RECORD),
        (LE_CHECK_CUSTOM, LE_BITMAP_CUSTOM, LE_LABEL_CUSTOM),
        (LE_CHECK_REVEAL, LE_BITMAP_REVEAL, LE_LABEL_REVEAL),
        (LE_CHECK_LOCK_TEAMS, LE_BITMAP_LOCK_TEAMS, LE_LABEL_LOCK_TEAMS),
        (LE_CHECK_LOCK_SPEED, LE_BITMAP_LOCK_SPEED, LE_LABEL_LOCK_SPEED),
        (LE_CHECK_CHEATS, LE_BITMAP_CHEATS, LE_LABEL_CHEATS),
    ];
    let flags = [o.record_games, o.custom_civs, o.reveal_map, o.lock_teams, o.lock_speed, o.cheat_codes];
    let (type_selected, type_selection) = h.selection(F, LE_GAME_TYPE);
    let found = op(h, F, EE_SCREENS_GAME_LE_FIND_GENERATOR, source, 0);
    let (epoch_selected, _) = h.selection(F, LE_EPOCH_START);
    let mut v = EeScreensLobbyOptionsView {
        load_type: o.load_type as c_long,
        lobby_load_type: g.load_type as c_long,
        server,
        host_ready: ready,
        lobby_multiplayer: g.multiplayer,
        options_multiplayer: o.multiplayer,
        type_selected,
        type_selection: type_selection as c_ulong,
        map_found: found & 0x8000_0000 == 0,
        map_line: (found & 0x7fff_ffff) as c_ulong,
        epoch_selected,
        unit_limit: o.unit_limit as c_long,
        unit_minimum: rule(h, EE_SCREENS_LE_RULE_UNIT_MINIMUM, 0) as c_ulong,
        unit_step: rule(h, EE_SCREENS_LE_RULE_UNIT_STEP, 0) as i32 as c_long,
        wonders: o.wonders,
        flags,
        checked: [false; EE_SCREENS_LOBBY_FLAGS],
    };
    for (i, f) in FLAGS.iter().enumerate() {
        v.checked[i] = op(h, F, EE_SCREENS_OP_CHECKBOX, f.0, 0) != 0;
    }
    let plan = lobby_options::show_options(&v);
    if plan.set_type_line {
        select(h, F, LE_GAME_TYPE, plan.type_line as u32);
    }
    let text = if plan.type_string != EE_SCREENS_LOBBY_NO_STRING {
        h.string(LE_IDS_GAME_TYPES[plan.type_string as usize])
    } else {
        Vec::new()
    };
    label(h, F, LE_GAME_TYPE_LABEL, &text);
    show_option_row(h, plan.type_editable, LE_GAME_TYPE, LE_GAME_TYPE_LABEL);
    select(h, F, LE_MAP_SIZE, o.map_size as u32);
    let (_, size) = h.selection(F, LE_MAP_SIZE);
    let sizes = rule(h, EE_SCREENS_LE_RULE_MAP_SIZES, 0);
    let id = rule(h, EE_SCREENS_LE_RULE_MAP_SIZE_NAME, if size < sizes { size } else { 0 });
    let text = h.string(id);
    label(h, F, LE_MAP_SIZE_LABEL, &text);
    show_map_row(h, plan.map_rows, LE_MAP_SIZE_LABEL0, LE_MAP_SIZE, LE_MAP_SIZE_LABEL);
    if plan.set_map_line {
        select(h, F, LE_MAP_TYPE, plan.map_line as u32);
    }
    let text = h.text(F, EE_SCREENS_TEXT_LE_MAP_DISPLAY, source);
    label(h, F, LE_MAP_TYPE_LABEL, &text);
    show_map_row(h, plan.map_rows, LE_MAP_TYPE_LABEL0, LE_MAP_TYPE, LE_MAP_TYPE_LABEL);
    if plan.scenario_info {
        let text = h.text(F, EE_SCREENS_TEXT_LE_MAP_TYPE, EE_SCREENS_LE_SOURCE_LOBBY);
        label(h, F, LE_SCENARIO_INFO, &text);
        show(h, F, LE_SCENARIO_INFO, LE_SHOWN);
        show(h, F, LE_SCENARIO_INFO_LABEL, LE_SHOWN);
    } else {
        show(h, F, LE_SCENARIO_INFO, LE_HIDDEN);
        show(h, F, LE_SCENARIO_INFO_LABEL, LE_HIDDEN);
    }
    let edit = plan.editable;
    select(h, F, LE_RESOURCES, o.resources as u32);
    let id = rule(h, EE_SCREENS_LE_RULE_STARTING_RESOURCES_NAME, o.resources as u32);
    let text = h.string(id);
    label(h, F, LE_RESOURCES_LABEL, &text);
    show_option_row(h, edit, LE_RESOURCES, LE_RESOURCES_LABEL);
    let text = epoch_title(h, o.epoch_start as u32);
    label(h, F, LE_EPOCH_START_LABEL, &text);
    if plan.set_epoch_start {
        select(h, F, LE_EPOCH_START, o.epoch_start as u32);
    }
    show_option_row(h, edit, LE_EPOCH_START, LE_EPOCH_START_LABEL);
    let text = epoch_title(h, o.epoch_end as u32);
    label(h, F, LE_EPOCH_END_LABEL, &text);
    show_option_row(h, edit, LE_EPOCH_END, LE_EPOCH_END_LABEL);
    select(h, F, LE_UNIT_LIMIT, plan.unit_line as u32);
    label(h, F, LE_UNIT_LIMIT_LABEL, &number_u(o.unit_limit));
    show_option_row(h, edit, LE_UNIT_LIMIT, LE_UNIT_LIMIT_LABEL);
    select(h, F, LE_WONDERS, o.wonders as u32);
    let text = if plan.wonders_number { number_u(o.wonders) } else { h.string(LE_IDS_OFF) };
    label(h, F, LE_WONDERS_LABEL, &text);
    show_option_row(h, edit, LE_WONDERS, LE_WONDERS_LABEL);
    let selectable = selectable_variants(h);
    select(h, F, LE_VARIANT, lobby_options::variant_line(&selectable, o.variant) as u32);
    let id = rule(h, EE_SCREENS_LE_RULE_VARIANT_NAME, o.variant as u32);
    let text = h.string(id);
    label(h, F, LE_VARIANT_LABEL, &text);
    show_option_row(h, edit, LE_VARIANT, LE_VARIANT_LABEL);
    select(h, F, LE_DIFFICULTY, o.difficulty as u32);
    let n = rule(h, EE_SCREENS_LE_RULE_DIFFICULTIES, 0);
    let id =
        rule(h, EE_SCREENS_LE_RULE_DIFFICULTY_NAME, if (o.difficulty as u32) < n { o.difficulty as u32 } else { 0 });
    let text = h.string(id);
    label(h, F, LE_DIFFICULTY_LABEL, &text);
    show_option_row(h, edit, LE_DIFFICULTY, LE_DIFFICULTY_LABEL);
    select(h, F, LE_SPEED, o.speed as u32);
    let n = rule(h, EE_SCREENS_LE_RULE_SPEEDS, 0);
    let id = rule(h, EE_SCREENS_LE_RULE_SPEED_NAME, if (o.speed as u32) < n { o.speed as u32 } else { 0 });
    let text = h.string(id);
    label(h, F, LE_SPEED_LABEL, &text);
    show_option_row(h, edit, LE_SPEED, LE_SPEED_LABEL);
    // the check boxes: the box set when it differs, the bitmap, the label text; record games stay hidden, the cheat
    // codes row follows the plan (a multiplayer game keeps it as it is)
    for (i, f) in FLAGS.iter().enumerate() {
        if plan.set_checked[i] {
            op(h, F, EE_SCREENS_OP_SET_CHECKBOX, f.0, flags[i] as u32);
        }
        op(h, F, EE_SCREENS_OP_LE_BITMAP, f.1, flags[i] as u32);
        let text = h.text_checked(F, EE_SCREENS_TEXT_LE_CHECK_LABEL, f.0).unwrap_or_default();
        label(h, F, f.2, &text);
        let row = if i == 0 {
            EE_SCREENS_LOBBY_ROW_HIDDEN
        } else if i == EE_SCREENS_LOBBY_FLAGS - 1 {
            plan.cheats_row
        } else if edit {
            EE_SCREENS_LOBBY_ROW_EDIT
        } else {
            EE_SCREENS_LOBBY_ROW_LABEL
        };
        if row == EE_SCREENS_LOBBY_ROW_HIDDEN {
            show(h, F, f.0, LE_HIDDEN);
            show(h, F, f.1, LE_HIDDEN);
            show(h, F, f.2, LE_HIDDEN);
        } else if row != EE_SCREENS_LOBBY_ROW_KEEP {
            let edit_row = row == EE_SCREENS_LOBBY_ROW_EDIT;
            show(h, F, f.0, if edit_row { LE_SHOWN } else { LE_HIDDEN });
            show(h, F, f.1, if edit_row { LE_HIDDEN } else { LE_SHOWN });
            show(h, F, f.2, if edit_row { LE_HIDDEN } else { LE_SHOWN });
        }
    }
    op(h, F, EE_SCREENS_OP_SET_CHECKBOX, LE_CHECK_VICTORY, 1);
    show(h, F, LE_CHECK_VICTORY, LE_HIDDEN);
    show(h, F, LE_BITMAP_VICTORY, LE_HIDDEN);
    show(h, F, LE_LABEL_VICTORY, LE_HIDDEN);
}

/// `LobbyUpdateGameOptions`: the options shown from the lobby options.
pub fn update_game_options<H: LobbyHost + ?Sized>(h: &mut H) {
    lock(h, EE_SCREENS_LE_READ_LOCK);
    let host = lobby_player(h, 0);
    show_game_options(h, EE_SCREENS_LE_SOURCE_LOBBY, &host);
    lock(h, EE_SCREENS_LE_READ_UNLOCK);
}

/// `LobbyRefresh`: every slot and the options, then the form.
fn refresh<H: LobbyHost + ?Sized>(h: &mut H, s: &Mutex<LobbyForm>) {
    lock(h, EE_SCREENS_LE_READ_LOCK);
    let host = lobby_player(h, 0);
    for i in 0..16 {
        let p = lobby_player(h, i);
        show_player(h, s, &p, &host, LE_SHOW_ALL);
    }
    show_game_options(h, EE_SCREENS_LE_SOURCE_LOBBY, &host);
    lock(h, EE_SCREENS_LE_READ_UNLOCK);
    update_form(h, s);
}

/// `LobbyUpdateEpochEnd`: the end epoch box trimmed or refilled to the epochs from the chosen start on.
fn update_epoch_end<H: LobbyHost + ?Sized>(h: &mut H) {
    const F: u32 = LE_FORM_LOBBY;
    let (ok, selection) = h.selection(F, LE_EPOCH_START);
    if !ok {
        return;
    }
    let epochs = rule(h, EE_SCREENS_LE_RULE_EPOCHS, 0) as i32;
    let first = if selection == epochs as u32 { 0 } else { selection as i32 }; // random: every epoch
    let limit = epochs.wrapping_sub(op(h, F, EE_SCREENS_OP_LE_COMBO_LINES, LE_EPOCH_END, 0) as i32);
    if first > limit {
        for _ in limit..first {
            op(h, F, EE_SCREENS_OP_LE_COMBO_DELETE_LINE, LE_EPOCH_END, 0);
        }
    } else if first < limit {
        let mut i = limit - 1;
        while i >= first {
            let text = epoch_title(h, i as u32);
            h.put_text(F, EE_SCREENS_TEXT_COMBO_INSERT, LE_EPOCH_END << 16, &text);
            i -= 1;
        }
    }
}

/// `LobbyUpdateForm`: the end epoch, the local slots' (and the host's option) events, the form enabled.
pub fn update_form<H: LobbyHost + ?Sized>(h: &mut H, s: &Mutex<LobbyForm>) {
    const F: u32 = LE_FORM_LOBBY;
    if op(h, F, EE_SCREENS_OP_LE_FORM_VISIBLE, F, 0) == 0 {
        return;
    }
    lock(h, EE_SCREENS_LE_READ_LOCK);
    update_epoch_end(h);
    let o = h.options(EE_SCREENS_LE_SOURCE_LOBBY);
    let mut end = o.epoch_end;
    if o.epoch_start != rule(h, EE_SCREENS_LE_RULE_EPOCHS, 0) as c_long {
        end = end.wrapping_sub(o.epoch_start);
    }
    select(h, F, LE_EPOCH_END, end as u32);
    for i in 0..16 {
        if lobby_player(h, i).r.is_local {
            set_player_events(h, s, i as u32);
        }
    }
    if is_server(h) {
        set_game_option_events(h);
        set_host_events(h);
    }
    h.call(F, EE_SCREENS_OP_GROUP, 1, 1, F + 1, 0);
    lock(h, EE_SCREENS_LE_READ_UNLOCK);
}

/// `LobbySetPlayerEvents`: a slot's type, colour (both clicks: the slot's default colour), team and ready events.
fn set_player_events<H: LobbyHost + ?Sized>(h: &mut H, s: &Mutex<LobbyForm>, slot: u32) {
    const F: u32 = LE_FORM_LOBBY;
    trigger(h, F, slot + LE_TYPE, LE_TRIGGER_SELECTION, EE_SCREENS_LE_PLAYER_TYPE);
    let colour = rule(h, EE_SCREENS_LE_RULE_DEFAULT_COLOUR, slot);
    s.lock().unwrap().colours[slot as usize] = colour as c_ulong;
    trigger(h, F, slot + LE_COLOUR, LE_TRIGGER_BUTTON_UP, EE_SCREENS_LE_COLOR_UP);
    trigger(h, F, slot + LE_COLOUR, LE_TRIGGER_RIGHT_UP, EE_SCREENS_LE_COLOR_DOWN);
    trigger(h, F, slot + LE_TEAM, LE_TRIGGER_SELECTION, EE_SCREENS_LE_TEAM);
    trigger(h, F, slot + LE_READY, LE_TRIGGER_BUTTON_UP, EE_SCREENS_LE_READY);
}

/// `LobbySetHostEvents`: the type boxes of slots 1 to 15, every slot's citizens box and engine box (filled).
fn set_host_events<H: LobbyHost + ?Sized>(h: &mut H) {
    const F: u32 = LE_FORM_LOBBY;
    for i in 31..46 {
        trigger(h, F, i, LE_TRIGGER_SELECTION, EE_SCREENS_LE_PLAYER_TYPE);
    }
    for i in LE_CITIZENS..LE_CITIZENS + 16 {
        trigger(h, F, i, LE_TRIGGER_SELECTION, EE_SCREENS_LE_CITIZENS);
    }
    for i in LE_ENGINE..LE_ENGINE + 16 {
        fill_engine_box(h, i);
        trigger(h, F, i, LE_TRIGGER_SELECTION, EE_SCREENS_LE_ENGINE);
    }
}

/// `LobbySetGameOptionEvents`: the host's option boxes and check boxes.
fn set_game_option_events<H: LobbyHost + ?Sized>(h: &mut H) {
    const F: u32 = LE_FORM_LOBBY;
    let boxes = [
        (LE_GAME_TYPE, EE_SCREENS_LE_GAME_TYPE),
        (LE_MAP_SIZE, EE_SCREENS_LE_MAP_SIZE),
        (LE_MAP_TYPE, EE_SCREENS_LE_MAP_TYPE),
        (LE_RESOURCES, EE_SCREENS_LE_RESOURCES),
        (LE_EPOCH_START, EE_SCREENS_LE_EPOCH_START),
        (LE_EPOCH_END, EE_SCREENS_LE_EPOCH_END),
        (LE_UNIT_LIMIT, EE_SCREENS_LE_UNIT_LIMIT),
        (LE_WONDERS, EE_SCREENS_LE_WONDERS),
        (LE_VARIANT, EE_SCREENS_LE_VARIANT),
        (LE_DIFFICULTY, EE_SCREENS_LE_DIFFICULTY),
        (LE_SPEED, EE_SCREENS_LE_SPEED),
    ];
    for (control, event) in boxes {
        trigger(h, F, control, LE_TRIGGER_SELECTION, event);
    }
    let checks = [
        (LE_CHECK_RECORD, EE_SCREENS_LE_RECORD_GAMES),
        (LE_CHECK_REVEAL, EE_SCREENS_LE_REVEAL_MAP),
        (LE_CHECK_CUSTOM, EE_SCREENS_LE_CUSTOM_CIVS),
        (LE_CHECK_LOCK_TEAMS, EE_SCREENS_LE_LOCK_TEAMS),
        (LE_CHECK_LOCK_SPEED, EE_SCREENS_LE_LOCK_SPEED),
        (LE_CHECK_CHEATS, EE_SCREENS_LE_CHEAT_CODES),
        (LE_CHECK_VICTORY, EE_SCREENS_LE_VICTORY),
    ];
    for (control, event) in checks {
        trigger(h, F, control, LE_TRIGGER_CHECKED, event);
        trigger(h, F, control, LE_TRIGGER_UNCHECKED, event);
    }
}

/// `LobbyShowTransferProgress`: a client's file transfer percentage on its row; 0 or 100 hide it, and at 100 the
/// host clears the slot's transfer.
pub fn show_transfer_progress<H: LobbyHost + ?Sized>(h: &mut H, slot: u8, progress: u8) {
    const F: u32 = LE_FORM_LOBBY;
    if op(h, F, EE_SCREENS_OP_LE_FORM_VISIBLE, F, 0) == 0 || slot == 0 || slot >= 16 {
        return;
    }
    let control = slot as u32 + LE_TRANSFER;
    if progress != 100 && progress != 0 {
        if op(h, F, EE_SCREENS_OP_CURRENT_FORM_IS, F, 0) != 0 {
            label(h, F, control, &number(progress as i64));
            show(h, F, control, LE_SHOWN);
        }
    } else {
        show(h, F, control, LE_HIDDEN);
        if progress == 100 && is_server(h) {
            lock(h, EE_SCREENS_LE_WRITE_LOCK);
            let mut p = lobby_player(h, slot as c_ulong);
            p.r.transfer = 0;
            put_lobby_player(h, slot as c_ulong, &p);
            lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
        }
    }
}

/// `LobbyOpen`: the boxes filled (citizens, teams, map sizes and types, resources, epochs, unit limits, wonders,
/// variants, difficulties, speeds, game types), the type menus forgotten, the engine column's header.
fn lobby_open<H: LobbyHost + ?Sized>(h: &mut H, s: &Mutex<LobbyForm>) {
    const F: u32 = LE_FORM_EVENT;
    for i in LE_CITIZENS..LE_CITIZENS + 16 {
        for j in 1..=20 {
            append(h, F, i, &number(j));
        }
    }
    for i in LE_TEAM..LE_TEAM + 16 {
        append(h, F, i, b"-");
        for j in 1..=4 {
            append(h, F, i, &number(j));
        }
    }
    let sizes = rule(h, EE_SCREENS_LE_RULE_MAP_SIZES, 0);
    for k in 0..sizes {
        let id = rule(h, EE_SCREENS_LE_RULE_MAP_SIZE_NAME, k);
        let text = h.string(id);
        append(h, F, LE_MAP_SIZE, &text);
    }
    let generators = rule(h, EE_SCREENS_LE_RULE_GENERATORS, 0);
    for j in 0..generators {
        let text = h.text(F, EE_SCREENS_TEXT_LE_GENERATOR_DISPLAY, j);
        append(h, F, LE_MAP_TYPE, &text);
    }
    let levels = rule(h, EE_SCREENS_LE_RULE_RESOURCE_LEVELS, 0);
    for j in 0..levels {
        let id = rule(h, EE_SCREENS_LE_RULE_RESOURCE_LEVEL_NAME, j);
        let text = h.string(id);
        append(h, F, LE_RESOURCES, &text);
    }
    let epochs = rule(h, EE_SCREENS_LE_RULE_EPOCHS, 0);
    for j in 0..=epochs {
        let text = epoch_title(h, j); // every epoch, then "Random Epoch"
        append(h, F, LE_EPOCH_START, &text);
    }
    for j in 0..epochs {
        let text = epoch_title(h, j);
        append(h, F, LE_EPOCH_END, &text);
    }
    let minimum = rule(h, EE_SCREENS_LE_RULE_UNIT_MINIMUM, 0);
    let maximum = rule(h, EE_SCREENS_LE_RULE_UNIT_MAXIMUM, 0);
    let step = rule(h, EE_SCREENS_LE_RULE_UNIT_STEP, 0);
    let mut j = minimum;
    while j <= maximum {
        append(h, F, LE_UNIT_LIMIT, &number(j as i32 as i64));
        j = j.wrapping_add(step);
    }
    let text = h.string(LE_IDS_OFF);
    append(h, F, LE_WONDERS, &text);
    let wonders = rule(h, EE_SCREENS_LE_RULE_WONDERS_MAXIMUM, 0);
    for j in 1..=wonders {
        append(h, F, LE_WONDERS, &number(j as i64));
    }
    let variants = rule(h, EE_SCREENS_LE_RULE_VARIANTS, 0);
    for j in 0..variants {
        if rule(h, EE_SCREENS_LE_RULE_VARIANT_SELECTABLE, j) != 0 {
            let id = rule(h, EE_SCREENS_LE_RULE_VARIANT_NAME, j);
            let text = h.string(id);
            append(h, F, LE_VARIANT, &text);
        }
    }
    let n = rule(h, EE_SCREENS_LE_RULE_DIFFICULTIES, 0);
    for k in 0..n {
        let id = rule(h, EE_SCREENS_LE_RULE_DIFFICULTY_NAME, k);
        let text = h.string(id);
        append(h, F, LE_DIFFICULTY, &text);
    }
    let n = rule(h, EE_SCREENS_LE_RULE_SPEEDS, 0);
    for k in 0..n {
        let id = rule(h, EE_SCREENS_LE_RULE_SPEED_NAME, k);
        let text = h.string(id);
        append(h, F, LE_SPEED, &text);
    }
    for id in LE_IDS_GAME_TYPES {
        let text = h.string(id);
        append(h, F, LE_GAME_TYPE, &text);
    }
    s.lock().unwrap().menus = [0; 16];
    // port: the engine column (EEUIManager's ctor adds it): the rows show their engine; the header replaces the ping
    // label ("Ping / AI"; single player: "AI")
    for i in 0..16 {
        show(h, F, LE_ENGINE + i, LE_HIDDEN);
        show(h, F, LE_ENGINE_LABEL + i, LE_HIDDEN);
    }
    let text = h.string(LE_IDS_ENGINE_PING_HEADER);
    label(h, F, LE_ENGINE_HEADER, &text);
    show(h, F, LE_ENGINE_HEADER, LE_SHOWN);
    show(h, F, LE_PING_LABEL, LE_HIDDEN);
}

/// The lobby opened: the multiplayer host's (`server`), a client's or single player's.
fn activate<H: LobbyHost + ?Sized>(h: &mut H, s: &Mutex<LobbyForm>, event: u32) {
    const F: u32 = LE_FORM_EVENT;
    h.call(F, EE_SCREENS_OP_GROUP, 1, 0, 0, 0);
    lobby_open(h, s);
    trigger(h, F, LE_CANCEL, LE_TRIGGER_BUTTON_UP, EE_SCREENS_LE_CANCEL);
    if event != EE_SCREENS_LE_ACTIVATE_CLIENT {
        trigger(h, F, LE_START, LE_TRIGGER_BUTTON_UP, EE_SCREENS_LE_START);
    }
    if event == EE_SCREENS_LE_ACTIVATE_SINGLE {
        show(h, F, LE_CHAT_BOX, LE_HIDDEN);
        show(h, F, LE_CHAT_MESSAGES, LE_HIDDEN);
        show(h, F, LE_READY_LABEL, LE_HIDDEN);
        show(h, F, LE_PING_LABEL, LE_HIDDEN);
        let text = h.string(LE_IDS_ENGINE_HEADER); // port: the engine column's header without the ping
        label(h, F, LE_ENGINE_HEADER, &text);
        op(h, F, EE_SCREENS_OP_LE_KEYBOARD_EVENT, 0, 0);
        show(h, F, LE_START, LE_SHOWN);
        h.call(F, EE_SCREENS_OP_BACKGROUND, LE_BACKGROUND_MULTI, 0, 0, 0);
        h.call(F, EE_SCREENS_OP_BACKGROUND, LE_BACKGROUND_SINGLE, 1, 0, 0);
        op(h, F, EE_SCREENS_OP_LE_BLINKING, LE_LAG, 0);
        show(h, F, LE_LAG, LE_HIDDEN);
        return;
    }
    s.lock().unwrap().chat_prefix.clear(); // a new chat event's empty prefix
    trigger(h, F, LE_CHAT_BOX, LE_TRIGGER_ENTER, EE_SCREENS_LE_CHAT);
    op(h, F, EE_SCREENS_OP_LE_KEYBOARD_EVENT, EE_SCREENS_LE_CHAT_FOCUS, 0);
    let server = event == EE_SCREENS_LE_ACTIVATE_SERVER;
    show(h, F, LE_START, if server { LE_SHOWN } else { LE_HIDDEN });
    if server {
        op(h, F, EE_SCREENS_GAME_LE_START_PING, 0, 0);
    }
    op(h, F, EE_SCREENS_OP_TAB_STOP, LE_CHAT_BOX, 0);
    h.call(F, EE_SCREENS_OP_BACKGROUND, LE_BACKGROUND_MULTI, 1, 0, 0);
    h.call(F, EE_SCREENS_OP_BACKGROUND, LE_BACKGROUND_SINGLE, 0, 0, 0);
    show(h, F, LE_CHECK_CHEATS, LE_HIDDEN);
    show(h, F, LE_LABEL_CHEATS, LE_HIDDEN);
    show(h, F, LE_BITMAP_CHEATS, LE_HIDDEN);
    op(h, F, EE_SCREENS_OP_LE_BLINKING, LE_LAG, 0);
    show(h, F, LE_LAG, LE_HIDDEN);
    if !server {
        op(h, F, EE_SCREENS_GAME_LE_WON_LEAVE, 0, 0);
    }
}

// ---- the start check, the loads

/// `LobbyCheckStart`: whether the game may start (a loaded game's file opens and matches; the players, colours,
/// teams, ready boxes and transfers), each reason shown.
fn check_start<H: LobbyHost + ?Sized>(h: &mut H) -> bool {
    let o = h.options(EE_SCREENS_LE_SOURCE_LOBBY);
    let mut load_ok = true;
    if o.load_type != 0 {
        let directory: &[u8] = if o.load_type == 2 { b"Saved Games\\" } else { b"Scenarios\\" };
        let file = h.text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_MAP_TYPE, EE_SCREENS_LE_SOURCE_LOBBY);
        let outcome =
            h.put_text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_CHECK_LOAD, 0, &[&file[..], b"\0", directory].concat());
        load_problem(h, outcome);
        load_ok = outcome == EE_SCREENS_LE_LOAD_OK;
    }
    let mut slots = [EeScreensLobbyStartSlot::default(); 16];
    for (i, slot) in slots.iter_mut().enumerate() {
        let p = lobby_player(h, i as c_ulong);
        *slot = EeScreensLobbyStartSlot {
            kind: p.r.kind,
            color: p.r.color,
            team: p.r.team,
            ready: p.r.is_ready,
            transfer: p.r.transfer,
        };
    }
    let start = lobby_options::start_check(o.load_type, load_ok, o.multiplayer, &slots);
    for &m in &start.messages[..start.count as usize] {
        let text = h.string(LE_IDS_START[m as usize]);
        message(h, &text);
    }
    if start.hide_rows {
        for i in 257..272 {
            show(h, LE_FORM_LOBBY, i, LE_HIDDEN);
        }
    }
    start.result
}

/// A load check's or header's problem shown: the file not found, a read error, the options' missing lines.
fn load_problem<H: LobbyHost + ?Sized>(h: &mut H, outcome: u32) {
    let text = match outcome {
        EE_SCREENS_LE_LOAD_NOT_FOUND => h.string(LE_IDS_NOT_FOUND),
        EE_SCREENS_LE_LOAD_READ_ERROR => h.string(LE_IDS_READ_ERROR),
        EE_SCREENS_LE_LOAD_MISMATCH => h.text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_MISSING, 0),
        _ => return,
    };
    message(h, &text);
}

/// `LobbyFindFile`: the load screen's Load: the picked file's header checked against the lobby's options, the file
/// sent to the joined humans, its options and players applied; the load screen closed. `kErrorOK` (0) with its name.
fn find_file<H: LobbyHost + ?Sized>(
    h: &mut H,
    s: &Mutex<LobbyForm>,
    directory: &[u8],
    extension: &[u8],
    load_type: u32,
) -> Option<Vec<u8>> {
    let mut done = false;
    let mut result: c_long = 1;
    let mut name = Vec::new();
    lock(h, EE_SCREENS_LE_WRITE_LOCK);
    let selection = op(h, LE_FORM_LOAD, EE_SCREENS_OP_FORM_LIST_SELECTED, LE_FORM_LOAD + 1, LE_LOAD_LIST);
    if selection != u32::MAX {
        let picked = h.text(LE_FORM_LOAD, EE_SCREENS_TEXT_LIST_DATA, LE_LOAD_LIST << 16 | selection);
        let file = [&picked[..], extension].concat();
        let outcome = h.put_text(
            LE_FORM_LOBBY,
            EE_SCREENS_TEXT_LE_OPEN_HEADER,
            load_type,
            &[&file[..], b"\0", directory].concat(),
        );
        match outcome {
            EE_SCREENS_LE_LOAD_OK => {
                name = picked;
                for i in 1..16 {
                    let mut p = lobby_player(h, i);
                    if p.r.kind != LE_HUMAN && p.r.kind != LE_OBSERVER {
                        continue;
                    }
                    p.r.transfer =
                        h.call(LE_FORM_LOBBY, EE_SCREENS_GAME_LE_XFER, i as u32, p.r.transfer as u32, 0, 0) as c_ulong;
                    put_lobby_player(h, i, &p);
                }
                op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_SET_OPTION, LE_OPTION_LOAD_TYPE, load_type);
                h.put_text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_MAP_TYPE, EE_SCREENS_LE_SOURCE_LOBBY, &file);
                apply_saved_game_options(h);
                result = rebuild_players(h, s);
                done = true;
            }
            EE_SCREENS_LE_LOAD_MISMATCH => {
                load_problem(h, outcome);
                result = LE_LOAD_ERROR;
            }
            _ => load_problem(h, outcome),
        }
    }
    lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
    if done {
        op(h, LE_FORM_LOAD, EE_SCREENS_OP_OPEN_EVENT, LE_FORM_LOAD, 0);
        op(h, LE_FORM_LOAD, EE_SCREENS_OP_POP, 0, 0);
        refresh(h, s);
    }
    (result == 0).then_some(name)
}

/// `LobbyApplySavedGameOptions`: the loaded header's options (a copy: `EEGameOptions`' assignment checks each value)
/// into the lobby options, sent.
fn apply_saved_game_options<H: LobbyHost + ?Sized>(h: &mut H) {
    op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_APPLY_SAVED, 0, 0);
}

/// One saved slot's fields into a slot taking it.
fn take_saved(p: &mut LobbyPlayer, saved: &LobbyPlayer, kind: c_ulong, with_engine: bool) {
    p.r.kind = kind;
    p.name = saved.name.clone();
    p.r.citizens = saved.r.citizens;
    p.r.loaded_type = saved.r.kind as c_long;
    p.r.color = saved.r.color;
    p.r.team = saved.r.team;
    p.r.b36 = saved.r.b36;
    if with_engine {
        p.r.engine = saved.r.engine; // port: the engine the saved game played (shown)
    }
}
/// A joined human taking a saved slot's citizens, colour, team and flag.
fn keep_human(p: &mut LobbyPlayer, saved: &LobbyPlayer) {
    p.r.citizens = saved.r.citizens;
    p.r.color = saved.r.color;
    p.r.team = saved.r.team;
    p.r.b36 = saved.r.b36;
}

/// `LobbyRebuildPlayers`: the slots rebuilt from the loaded header and sent: joined humans keep their slots (matched
/// by name in saved games), the others become computer, open or closed; 9 when the humans don't fit. (The fix of
/// the original's: running out of free slots stops the rebuild, which then fails.)
fn rebuild_players<H: LobbyHost + ?Sized>(h: &mut H, s: &Mutex<LobbyForm>) -> c_long {
    let mut others: Vec<LobbyPlayer> = Vec::new();
    let mut humans: Vec<LobbyPlayer> = Vec::new();
    for i in 0..16 {
        let mut p = lobby_player(h, 15 - i);
        if p.r.kind == LE_HUMAN || p.r.kind == LE_OBSERVER {
            humans.push(p);
        } else {
            p.r.kind = LE_CLOSED;
            others.push(p);
        }
    }
    let mut out_of_slots = false;
    let saved_game = h.options(EE_SCREENS_LE_SOURCE_LOBBY).load_type == 2;
    if !humans.is_empty() {
        if saved_game {
            for j in 0..16 {
                let saved = h.player(EE_SCREENS_LE_SOURCE_HEADER, j);
                let human = saved.r.kind == LE_HUMAN || saved.r.kind == LE_OBSERVER;
                if human {
                    if let Some(k) = humans.iter().position(|p| name_eq(&p.name, &saved.name)) {
                        let p = &mut humans[k];
                        p.r.kind = saved.r.kind;
                        p.r.loaded_type = saved.r.loaded_type;
                        keep_human(p, &saved);
                        let p = humans.remove(k);
                        put_lobby_player(h, p.r.slot, &p);
                        continue;
                    }
                }
                let Some(mut p) = others.pop() else {
                    out_of_slots = true;
                    break;
                };
                let kind = if human {
                    LE_OPEN
                } else if saved.r.kind != LE_COMPUTER {
                    LE_CLOSED
                } else {
                    LE_COMPUTER
                };
                take_saved(&mut p, &saved, kind, true);
                put_lobby_player(h, p.r.slot, &p);
            }
        } else {
            let saved = h.player(EE_SCREENS_LE_SOURCE_HEADER, 0);
            let mut p = humans.pop().unwrap();
            keep_human(&mut p, &saved);
            put_lobby_player(h, p.r.slot, &p);
            let count = op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_HEADER_PLAYERS, 0, 0) as i32;
            for j in 1..16i32 {
                if j < count.wrapping_sub(1) {
                    let saved = h.player(EE_SCREENS_LE_SOURCE_HEADER, j as u32);
                    if saved.r.b36 {
                        let Some(mut p) = others.pop() else {
                            out_of_slots = true;
                            break;
                        };
                        take_saved(&mut p, &saved, LE_COMPUTER, true);
                        put_lobby_player(h, p.r.slot, &p);
                    } else if let Some(mut p) = humans.pop() {
                        keep_human(&mut p, &saved);
                        put_lobby_player(h, p.r.slot, &p);
                    } else {
                        let Some(mut p) = others.pop() else {
                            out_of_slots = true;
                            break;
                        };
                        take_saved(&mut p, &saved, LE_OPEN, false);
                        put_lobby_player(h, p.r.slot, &p);
                    }
                } else {
                    let Some(mut p) = others.pop() else {
                        out_of_slots = true;
                        break;
                    };
                    p.r.kind = LE_CLOSED;
                    p.r.loaded_type = LE_CLOSED as c_long;
                    p.r.b36 = false;
                    put_lobby_player(h, p.r.slot, &p);
                }
            }
        }
    }
    let result = if !humans.is_empty() || out_of_slots { LE_LOAD_ERROR } else { 0 };
    if result == 0 {
        for j in 0..16 {
            op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_FULL_PLAYER, j, 0);
            let p = lobby_player(h, j as c_ulong);
            let host = lobby_player(h, 0);
            show_player(h, s, &p, &host, LE_SHOW_ALL);
        }
    }
    result
}

/// `operator==` of two `UWideString`s (a multibyte compare of two C strings: their bytes up to a 0).
fn name_eq(a: &[u8], b: &[u8]) -> bool {
    let end = |s: &[u8]| s.iter().position(|&c| c == 0).unwrap_or(s.len());
    a[..end(a)] == b[..end(b)]
}

/// `LobbyIsColorUnique`: at most one human uses the colour (always for a loaded game).
fn is_color_unique<H: LobbyHost + ?Sized>(h: &mut H, colour: c_ulong) -> bool {
    let mut count = 0;
    if h.options(EE_SCREENS_LE_SOURCE_LOBBY).load_type == 0 {
        for i in 0..16 {
            let p = lobby_player(h, i);
            if p.r.color == colour && p.r.kind == LE_HUMAN {
                count += 1;
            }
        }
    }
    count <= 1
}

// ---- the option events

/// An option event's change (ee_screens lobby_options.rs option_change): each message sent and its value stored,
/// under the options' write lock (the caller's).
fn change_option<H: LobbyHost + ?Sized>(h: &mut H, option: c_long, selection: u32) {
    let o = h.options(EE_SCREENS_LE_SOURCE_LOBBY);
    let state = EeScreensLobbyOptionState {
        current: current_options(&o),
        epoch_count: rule(h, EE_SCREENS_LE_RULE_EPOCHS, 0) as c_long,
        unit_minimum: rule(h, EE_SCREENS_LE_RULE_UNIT_MINIMUM, 0) as c_ulong,
        unit_step: rule(h, EE_SCREENS_LE_RULE_UNIT_STEP, 0) as i32 as c_long,
    };
    let selectable = if option == EE_SCREENS_OPTION_VARIANT { selectable_variants(h) } else { Vec::new() };
    let change = lobby_options::option_change(option, selection as c_ulong, &state, &selectable);
    for (i, m) in change.messages[..change.count as usize].iter().enumerate() {
        if i == 0 && change.update_epoch_end {
            update_epoch_end(h);
        }
        if change.set_difficulty {
            op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_DIFFICULTY_LEVELS, m.value as u32, 0);
        }
        op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_SEND_OPTION, m.option as u32, m.value as u32);
        op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_SET_OPTION, m.option as u32, m.value as u32);
    }
}
/// A check box option's event: its state, under the write lock.
fn change_flag<H: LobbyHost + ?Sized>(h: &mut H, option: c_long, control: u32) {
    lock(h, EE_SCREENS_LE_WRITE_LOCK);
    let checked = op(h, LE_FORM_EVENT, EE_SCREENS_OP_CHECKBOX, control, 0);
    change_option(h, option, checked);
    lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
}
/// A combo box option's event: its selection (none: nothing, not even the lock), under the write lock.
fn change_selection<H: LobbyHost + ?Sized>(h: &mut H, option: c_long, control: u32) {
    let (ok, selection) = h.selection(LE_FORM_EVENT, control);
    if !ok {
        return;
    }
    lock(h, EE_SCREENS_LE_WRITE_LOCK);
    change_option(h, option, selection);
    lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
}

/// A colour button's step (up or down, wrapping in 16): shown on the button, sent.
fn change_colour<H: LobbyHost + ?Sized>(h: &mut H, s: &Mutex<LobbyForm>, control: u32, up: bool) {
    lock(h, EE_SCREENS_LE_WRITE_LOCK);
    let mut p = lobby_player(h, control.wrapping_sub(LE_COLOUR) as c_ulong);
    let colour = {
        let mut st = s.lock().unwrap();
        let c = &mut st.colours[control.wrapping_sub(LE_COLOUR) as usize];
        *c = if up {
            if c.wrapping_add(1) >= 16 {
                0
            } else {
                *c + 1
            }
        } else if *c == 0 {
            15
        } else {
            *c - 1
        };
        *c
    };
    op(h, LE_FORM_EVENT, EE_SCREENS_OP_LE_PLAYER_COLOUR, control, colour as u32);
    p.r.color = colour;
    player_info(h, LE_INFO_COLOUR, p.r.slot, p.r.color as u32);
    put_lobby_player(h, p.r.slot, &p);
    lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
}

/// `EEUIELEChangePlayerType`: the host asks before booting a human from a slot, other changes apply and are sent at
/// once; a client can only switch its own slot between player and observer.
fn change_player_type<H: LobbyHost + ?Sized>(h: &mut H, s: &Mutex<LobbyForm>, control: u32) {
    lock(h, EE_SCREENS_LE_WRITE_LOCK);
    let mut p = lobby_player(h, control.wrapping_sub(LE_TYPE) as c_ulong);
    let (ok, selection) = h.selection(LE_FORM_EVENT, control);
    if ok {
        let mut selection = selection as c_ulong;
        let mut ask = false;
        let mut changed = false;
        if is_server(h) && !p.r.is_local {
            let mut menu = s.lock().unwrap().menus[p.r.slot as usize];
            changed = p.r.kind != lobby::player_type_from_menu(selection, menu) as c_ulong;
            if changed {
                if p.r.kind == LE_HUMAN || p.r.kind == LE_OBSERVER {
                    ask = true;
                    // a joined human's box lists its name first (menu 2, or 5 in scenarios): skip that entry and map
                    // the rest through the same menu without the name (1 or 4); picking the name changes nothing
                    if menu == 2 || menu == 5 {
                        if selection == 0 {
                            changed = false;
                        }
                        selection = selection.wrapping_sub(1);
                        menu = if menu == 2 { 1 } else { 4 };
                    }
                }
                if changed {
                    p.r.kind = lobby::player_type_from_menu(selection, menu) as c_ulong;
                }
            }
        } else {
            match p.r.kind {
                1 => changed = selection != 0,
                2 => changed = selection != 1,
                _ => {}
            }
            if changed {
                match selection {
                    0 => p.r.kind = LE_HUMAN,
                    1 => p.r.kind = LE_OBSERVER,
                    _ => {}
                }
            }
        }
        if changed {
            if ask {
                h.put_player(EE_SCREENS_LE_SOURCE_BOOT, 0, &p);
                let text = h.string(LE_IDS_BOOT);
                h.put_text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_BOOT_QUESTION, 0, &text);
            } else {
                put_lobby_player(h, p.r.slot, &p);
                let host = lobby_player(h, 0);
                show_player(h, s, &p, &host, 0);
                player_info(h, LE_INFO_TYPE, p.r.slot, p.r.kind as u32);
            }
        }
    }
    lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
}

/// `EEUIELEChangeReady`: the local player ready or not (the host also its computer slots), refused on a taken colour;
/// sent, the rows redrawn, Cancel hidden while ready.
fn change_ready<H: LobbyHost + ?Sized>(h: &mut H, control: u32, s: &Mutex<LobbyForm>) {
    lock(h, EE_SCREENS_LE_WRITE_LOCK);
    let mut p = lobby_player(h, control.wrapping_sub(LE_READY) as c_ulong);
    let checked = |h: &mut H| op(h, LE_FORM_EVENT, EE_SCREENS_OP_CHECKBOX, control, 0) != 0;
    if checked(h) != p.r.is_ready {
        if checked(h) && !is_color_unique(h, p.r.color) {
            let text = h.string(LE_IDS_COLOR_TAKEN);
            message(h, &text);
            let now = checked(h);
            op(h, LE_FORM_EVENT, EE_SCREENS_OP_SET_CHECKBOX, control, !now as u32);
        } else if is_server(h) {
            for i in 0..16 {
                p = lobby_player(h, i);
                if !p.r.is_local && p.r.kind != LE_COMPUTER {
                    continue;
                }
                p.r.is_ready = checked(h);
                put_lobby_player(h, p.r.slot, &p);
                player_info(h, LE_INFO_READY, p.r.slot, p.r.is_ready as u32);
            }
        } else {
            p.r.is_ready = checked(h);
            put_lobby_player(h, p.r.slot, &p);
            player_info(h, LE_INFO_READY, p.r.slot, p.r.is_ready as u32);
        }
        if is_server(h) {
            p = lobby_player(h, 0);
            p.r.is_ready = checked(h);
            for i in 1..16 {
                let mut other = lobby_player(h, i);
                if other.r.kind == LE_COMPUTER {
                    other.r.is_ready = p.r.is_ready;
                }
                show_player(h, s, &other, &p, LE_SHOW_ALL);
            }
            show_player(h, s, &p, &p, LE_SHOW_ALL);
            show_game_options(h, EE_SCREENS_LE_SOURCE_LOBBY, &p);
        } else {
            let other = lobby_player(h, 0);
            show_player(h, s, &p, &other, LE_SHOW_ALL);
        }
        let mode = if checked(h) { LE_HIDDEN } else { LE_SHOWN };
        show(h, LE_FORM_EVENT, LE_CANCEL, mode);
    }
    lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
}

/// The boot question answered yes: the slot's client booted, the slot reset (not ready, default citizens, no name or
/// team, the classic engine) to the chosen type.
fn boot_yes<H: LobbyHost + ?Sized>(h: &mut H) {
    let mut p = h.player(EE_SCREENS_LE_SOURCE_BOOT, 0);
    lock(h, EE_SCREENS_LE_WRITE_LOCK);
    p.r.is_ready = false;
    p.r.citizens = rule(h, EE_SCREENS_LE_RULE_STARTING_CITIZENS, 0) as c_ulong;
    p.name.clear();
    p.r.ping = 0.0;
    p.r.team = 0;
    p.r.engine = 0; // port: kEECPChoiceClassic
    h.put_player(EE_SCREENS_LE_SOURCE_BOOT, 0, &p);
    op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_BOOT_REFUSE, p.r.kind as u32, 0);
    put_lobby_player(h, p.r.slot, &p);
    op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_BOOT_FREE, 0, 0);
    lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
}

/// The boot question answered no: the slot's type box back on its first line.
fn boot_no<H: LobbyHost + ?Sized>(h: &mut H) {
    let p = h.player(EE_SCREENS_LE_SOURCE_BOOT, 0);
    select(h, LE_FORM_LOBBY, (p.r.slot as u32).wrapping_add(LE_TYPE), 0);
    op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_BOOT_FREE, 0, 0);
}

/// `EEUIELEStart`: the game started when the checks pass: the server closed to new players, start loading sent with
/// a random seed, the options saved as the last game's, Start hidden.
fn start<H: LobbyHost + ?Sized>(h: &mut H) {
    lock(h, EE_SCREENS_LE_READ_LOCK);
    if check_start(h) {
        let (mut humans, mut players) = (0u8, 0u8);
        for i in 0..16 {
            let k = lobby_player(h, i).r.kind;
            if k == LE_HUMAN || k == LE_OBSERVER {
                humans = humans.wrapping_add(1);
            }
            if k == LE_OPEN || k == LE_HUMAN || k == LE_OBSERVER {
                players = players.wrapping_add(1);
            }
        }
        op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_START_SERVER, humans as u32, players as u32);
        op(h, LE_FORM_LOBBY, EE_SCREENS_OP_SRAND_TIME, 0, 0);
        let seed = op(h, LE_FORM_LOBBY, EE_SCREENS_OP_RAND, 0, 0);
        op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_START_LOADING, humans as u32, seed);
        op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_SAVE_LAST_OPTIONS, 0, 0);
        show(h, LE_FORM_EVENT, LE_START, LE_HIDDEN);
    }
    lock(h, EE_SCREENS_LE_READ_UNLOCK);
}

/// A load screen's open event: the file list on the folder (the last name preselected), Cancel and Load hooked up
/// (and the scenario's title).
fn open_load<H: LobbyHost + ?Sized>(h: &mut H, scenario: bool) {
    const F: u32 = LE_FORM_EVENT;
    let (extension, directory, name, load) = if scenario {
        (
            h.text(F, EE_SCREENS_TEXT_SCENARIO_EXTENSION, 0),
            &b"data\\Scenarios\\"[..],
            h.text(F, EE_SCREENS_TEXT_LOAD_SCENARIO_NAME, 0),
            EE_SCREENS_LE_LOAD_SCENARIO,
        )
    } else {
        (
            h.text(F, EE_SCREENS_TEXT_SAVED_EXTENSION, 0),
            &b"data\\Saved Games\\"[..],
            h.text(F, EE_SCREENS_TEXT_LOAD_SAVED_NAME, 0),
            EE_SCREENS_LE_LOAD_SAVED_GAME,
        )
    };
    let parts = [&extension[..], b"\0", directory, b"\0", &name[..]].concat();
    h.put_text(F, EE_SCREENS_TEXT_FILE_LIST, LE_FORM_LOAD << 16 | LE_LOAD_LIST, &parts);
    trigger(h, F, LE_LOAD_CANCEL, LE_TRIGGER_BUTTON_UP, EE_SCREENS_LE_CANCEL_LOAD);
    trigger(h, F, LE_LOAD_LOAD, LE_TRIGGER_BUTTON_UP, load);
    trigger(h, F, LE_LOAD_LIST, LE_TRIGGER_DOUBLE_CLICK, load);
    if scenario {
        let text = h.string(LE_IDS_PLAY_SCENARIO);
        label(h, F, LE_LOAD_TITLE, &text);
    }
}

/// The lobby's events (form 26's, form 4's lobby screens, the boot question's answers); false for another form's.
/// The entity of a control event is the event form's control (its index is the host's `EE_SCREENS_OP_ENTITY_INDEX`).
pub fn dispatch<H: LobbyHost + ?Sized>(h: &mut H, s: &Mutex<LobbyForm>, event: u32) -> bool {
    if !is_lobby_event(event) {
        return false;
    }
    let entity = |h: &mut H| op(h, LE_FORM_EVENT, EE_SCREENS_OP_ENTITY_INDEX, 0, 0);
    match event {
        EE_SCREENS_LE_CANCEL_LOAD => {
            op(h, LE_FORM_EVENT, EE_SCREENS_OP_OPEN_EVENT, LE_FORM_LOAD, 0);
            op(h, LE_FORM_EVENT, EE_SCREENS_OP_POP, 0, 0);
            lock(h, EE_SCREENS_LE_WRITE_LOCK);
            let load_type = s.lock().unwrap().cancel_load_type;
            op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_SET_OPTION, LE_OPTION_LOAD_TYPE, load_type as u32);
            lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
            refresh(h, s);
        }
        EE_SCREENS_LE_LOAD_SAVED_GAME | EE_SCREENS_LE_LOAD_SCENARIO => {
            let saved = event == EE_SCREENS_LE_LOAD_SAVED_GAME;
            let extension = h.text(
                LE_FORM_EVENT,
                if saved { EE_SCREENS_TEXT_SAVED_EXTENSION } else { EE_SCREENS_TEXT_SCENARIO_EXTENSION },
                0,
            );
            let directory: &[u8] = if saved { b"Saved Games\\" } else { b"Scenarios\\" };
            if let Some(name) = find_file(h, s, directory, &extension, if saved { 2 } else { 1 }) {
                let which = if saved { EE_SCREENS_TEXT_LOAD_SAVED_NAME } else { EE_SCREENS_TEXT_LOAD_SCENARIO_NAME };
                h.put_text(LE_FORM_EVENT, which, 0, &name);
            }
        }
        EE_SCREENS_LE_OPEN_LOAD_SAVED_GAME => open_load(h, false),
        EE_SCREENS_LE_OPEN_LOAD_SCENARIO => open_load(h, true),
        EE_SCREENS_LE_CITIZENS => {
            lock(h, EE_SCREENS_LE_WRITE_LOCK);
            let control = entity(h);
            let (ok, selection) = h.selection(LE_FORM_EVENT, control);
            if ok {
                let mut p = lobby_player(h, control.wrapping_sub(LE_CITIZENS) as c_ulong);
                if (p.r.citizens as u32).wrapping_sub(1) != selection {
                    p.r.citizens = selection.wrapping_add(1) as c_ulong;
                    player_info(h, LE_INFO_CITIZENS, p.r.slot, p.r.citizens as u32);
                    put_lobby_player(h, p.r.slot, &p);
                }
            }
            lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
        }
        EE_SCREENS_LE_ENGINE => {
            lock(h, EE_SCREENS_LE_WRITE_LOCK);
            let control = entity(h);
            if is_server(h) {
                let (ok, selection) = h.selection(LE_FORM_EVENT, control);
                if ok && selection < rule(h, EE_SCREENS_LE_RULE_ENGINES, 0) {
                    let mut p = lobby_player(h, control.wrapping_sub(LE_ENGINE) as c_ulong);
                    if p.r.kind == LE_COMPUTER && p.r.engine != selection as c_ulong {
                        p.r.engine = selection as c_ulong;
                        player_info(h, LE_INFO_ENGINE, p.r.slot, p.r.engine as u32);
                        put_lobby_player(h, p.r.slot, &p);
                    }
                }
            }
            lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
        }
        EE_SCREENS_LE_CHAT => {
            let control = entity(h);
            let prefix = s.lock().unwrap().chat_prefix.clone();
            let line = h.text(LE_FORM_EVENT, EE_SCREENS_TEXT_EDIT, control);
            if !line.is_empty() {
                let text = [&prefix[..], b": ", &line[..]].concat();
                h.put_text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_CHAT, 0, &text);
            }
            op(h, LE_FORM_EVENT, EE_SCREENS_OP_LE_EDIT_DELETE, control, 0);
        }
        EE_SCREENS_LE_CANCEL => {
            let close = op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_WON_REJOIN, 0, 0) != 0;
            if op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_IS_CLIENT, 0, 0) != 0 {
                op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_DISCONNECT, 0, 0);
            } else {
                op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_REMOVE_HOST, 0, 0);
            }
            if close {
                op(h, LE_FORM_LOBBY, EE_SCREENS_OP_CLOSE_FORM, 0, 0);
            }
        }
        EE_SCREENS_LE_START => start(h),
        EE_SCREENS_LE_COLOR_UP | EE_SCREENS_LE_COLOR_DOWN => {
            let control = entity(h);
            change_colour(h, s, control, event == EE_SCREENS_LE_COLOR_UP);
        }
        EE_SCREENS_LE_TEAM => {
            lock(h, EE_SCREENS_LE_WRITE_LOCK);
            let control = entity(h);
            let mut p = lobby_player(h, control.wrapping_sub(LE_TEAM) as c_ulong);
            let (ok, selection) = h.selection(LE_FORM_EVENT, control);
            if ok {
                p.r.team = selection as c_ulong;
                player_info(h, LE_INFO_TEAM, p.r.slot, p.r.team as u32);
                put_lobby_player(h, p.r.slot, &p);
            }
            lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
        }
        EE_SCREENS_LE_PLAYER_TYPE => {
            let control = entity(h);
            change_player_type(h, s, control);
        }
        EE_SCREENS_LE_READY => {
            let control = entity(h);
            change_ready(h, control, s);
        }
        EE_SCREENS_LE_VICTORY => {
            lock(h, EE_SCREENS_LE_WRITE_LOCK);
            let control = entity(h);
            let checked = op(h, LE_FORM_EVENT, EE_SCREENS_OP_CHECKBOX, control, 0) != 0;
            if checked != h.options(EE_SCREENS_LE_SOURCE_LOBBY).victory {
                op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_SEND_OPTION, LE_OPTION_VICTORY, checked as u32);
                op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_SET_OPTION, LE_OPTION_VICTORY, checked as u32);
            }
            lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
        }
        EE_SCREENS_LE_GAME_TYPE => {
            let control = entity(h);
            let (ok, selection) = h.selection(LE_FORM_EVENT, control);
            if ok {
                lock(h, EE_SCREENS_LE_WRITE_LOCK);
                let load_type = h.options(EE_SCREENS_LE_SOURCE_LOBBY).load_type;
                match lobby_options::game_type_action(selection as c_ulong, load_type) {
                    a @ (EE_SCREENS_GAME_TYPE_SCENARIO | EE_SCREENS_GAME_TYPE_SAVED_GAME) => {
                        let open = if a == EE_SCREENS_GAME_TYPE_SCENARIO {
                            EE_SCREENS_LE_OPEN_LOAD_SCENARIO
                        } else {
                            EE_SCREENS_LE_OPEN_LOAD_SAVED_GAME
                        };
                        op(h, LE_FORM_EVENT, EE_SCREENS_OP_OPEN_EVENT, LE_FORM_LOAD, open);
                        op(h, LE_FORM_EVENT, EE_SCREENS_OP_PUSH, LE_FORM_LOAD, 0);
                        let load_type = h.options(EE_SCREENS_LE_SOURCE_LOBBY).load_type;
                        s.lock().unwrap().cancel_load_type = load_type;
                    }
                    EE_SCREENS_GAME_TYPE_RANDOM_MAP => {
                        op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_SEND_OPTION, 0, 0);
                        op(h, LE_FORM_LOBBY, EE_SCREENS_GAME_LE_RANDOM_MAP_OPTIONS, 0, 0);
                        let host = lobby_player(h, 0);
                        show_game_options(h, EE_SCREENS_LE_SOURCE_COPY, &host);
                        for i in 0..16 {
                            let p = lobby_player(h, i);
                            show_player(h, s, &p, &host, LE_SHOW_ALL);
                        }
                    }
                    _ => {}
                }
                lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
            }
        }
        EE_SCREENS_LE_MAP_TYPE => {
            let control = entity(h);
            let (ok, selection) = h.selection(LE_FORM_EVENT, control);
            if ok {
                lock(h, EE_SCREENS_LE_WRITE_LOCK);
                let name = h.text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_GENERATOR_NAME, selection);
                let current = h.text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_MAP_TYPE, EE_SCREENS_LE_SOURCE_LOBBY);
                if lobby_options::map_type_changed(&name, &current) {
                    h.put_text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_SEND_MAP_TYPE, selection, &name);
                    h.put_text(LE_FORM_LOBBY, EE_SCREENS_TEXT_LE_MAP_TYPE, EE_SCREENS_LE_SOURCE_LOBBY, &name);
                }
                lock(h, EE_SCREENS_LE_WRITE_UNLOCK);
            }
        }
        EE_SCREENS_LE_MAP_SIZE..=EE_SCREENS_LE_SPEED => {
            let option = match event {
                EE_SCREENS_LE_MAP_SIZE => EE_SCREENS_OPTION_MAP_SIZE,
                EE_SCREENS_LE_RESOURCES => EE_SCREENS_OPTION_RESOURCES,
                EE_SCREENS_LE_EPOCH_START => EE_SCREENS_OPTION_EPOCH_START,
                EE_SCREENS_LE_EPOCH_END => EE_SCREENS_OPTION_EPOCH_END,
                EE_SCREENS_LE_UNIT_LIMIT => EE_SCREENS_OPTION_UNIT_LIMIT,
                EE_SCREENS_LE_WONDERS => EE_SCREENS_OPTION_WONDERS,
                EE_SCREENS_LE_VARIANT => EE_SCREENS_OPTION_VARIANT,
                EE_SCREENS_LE_DIFFICULTY => EE_SCREENS_OPTION_DIFFICULTY,
                _ => EE_SCREENS_OPTION_SPEED,
            };
            let control = entity(h);
            change_selection(h, option, control);
        }
        EE_SCREENS_LE_RECORD_GAMES..=EE_SCREENS_LE_CHEAT_CODES => {
            let option = match event {
                EE_SCREENS_LE_RECORD_GAMES => EE_SCREENS_OPTION_RECORD_GAMES,
                EE_SCREENS_LE_REVEAL_MAP => EE_SCREENS_OPTION_REVEAL_MAP,
                EE_SCREENS_LE_CUSTOM_CIVS => EE_SCREENS_OPTION_CUSTOM_CIVS,
                EE_SCREENS_LE_LOCK_TEAMS => EE_SCREENS_OPTION_LOCK_TEAMS,
                EE_SCREENS_LE_LOCK_SPEED => EE_SCREENS_OPTION_LOCK_SPEED,
                _ => EE_SCREENS_OPTION_CHEAT_CODES,
            };
            let control = entity(h);
            change_flag(h, option, control);
        }
        EE_SCREENS_LE_CHAT_FOCUS => {
            if op(h, LE_FORM_EVENT, EE_SCREENS_OP_LE_KEYBOARD_CAPTURED, 0, 0) == 0 {
                op(h, LE_FORM_EVENT, EE_SCREENS_OP_TAB_STOP, LE_CHAT_BOX, 0);
            }
        }
        EE_SCREENS_LE_ACTIVATE_SERVER | EE_SCREENS_LE_ACTIVATE_CLIENT | EE_SCREENS_LE_ACTIVATE_SINGLE => {
            activate(h, s, event)
        }
        EE_SCREENS_LE_BOOT_YES => boot_yes(h),
        EE_SCREENS_LE_BOOT_NO => boot_no(h),
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
    fn events() {
        assert!(is_lobby_event(EE_SCREENS_LE_CANCEL_LOAD) && is_lobby_event(EE_SCREENS_LE_BOOT_NO));
        assert!(!is_lobby_event(0x1a00) && !is_lobby_event(0x1a28));
    }

    #[test]
    fn names() {
        assert!(name_eq(b"Max", b"Max"));
        assert!(name_eq(b"Max\0x", b"Max"));
        assert!(!name_eq(b"Max", b"max"));
        assert!(name_eq(b"", b""));
    }

    #[test]
    fn numbers() {
        assert_eq!(number_u(20), b"20");
        assert_eq!(number_u(u32::MAX as c_ulong), b"-1");
    }
}
