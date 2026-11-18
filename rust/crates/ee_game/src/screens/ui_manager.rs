//! The game's UI manager (`EEUIManager`, `User Interface Thread/EEUIManager.cpp`): its constructor's work (the
//! event factories registered, the UI database loaded, the port's controls added to it as copies of database
//! controls: the settings' video, interface and audio controls, the lobby's narrower player column and its engine
//! column; the main game form found, the music volume, the save form's activate event), `StartScenarioEditor` (a
//! local server with a human in slot 0 and computers in slots 1..15, then the load), `UpdateKeyboard` (a movie
//! skipped on Escape, Return or Space; F9's screenshot outside text entry) and `UpdateMouse` (a movie skipped on a
//! click).
//!
//! Every call on the manager, the toolkit, the world and the threads is one host op ([`UiHost`],
//! `ee_screenscpp_ui_manager`), asked where and as often as the original asked it. The control and background
//! factories, `CreateForm`, the message boxes and the scenario editor form's lifetime stay C++ (they make C++
//! objects).

use crate::screens::ui_thread::UiHost;

// ---- the host's ops (EE_SCREENS_UM_OP_*)

/// `RegisterControlEventCallback(EECreateGameEvent)`, `(EECreateWonNetServerSelectEvent)`,
/// `RegisterFormEventCallback(EECreateFormEvent)`.
pub const EE_SCREENS_UM_OP_REGISTER: u32 = 1;
/// `LoadDatabase(gDataPath, UString())`.
pub const EE_SCREENS_UM_OP_LOAD_DATABASE: u32 = 2;
/// `UIControlsTable::AddControlLike(this, ...)` of the [`EeScreensManagerControl`] at c.
pub const EE_SCREENS_UM_OP_ADD_LIKE: u32 = 3;
/// `UIControlsTable::MoveControlsHorizontally(...)` of the [`EeScreensManagerMove`] at c.
pub const EE_SCREENS_UM_OP_MOVE: u32 = 4;
/// `EECPEngineChoiceCount()`.
pub const EE_SCREENS_UM_OP_ENGINE_COUNT: u32 = 5;
/// `mMainGameForm = GetFormByID(kFormMainGame)`, `mInScenarioEditor = false`, `mScenarioEditorForm = NULL`,
/// `mB13D = false`.
pub const EE_SCREENS_UM_OP_FORMS: u32 = 6;
/// `SetMusicVolume(the music setting's volume, 0 when the music is off)`.
pub const EE_SCREENS_UM_OP_MUSIC: u32 = 7;
/// `mU138 = 0`; the save form's activate event (`EECreateScreenEvent(EE_SCREENS_SL_ACTIVATE)`, set, dereferenced).
pub const EE_SCREENS_UM_OP_SAVE_EVENT: u32 = 8;
/// The lobby options' lock: a 1 `WriteLock()`, 0 `WriteUnlock()`.
pub const EE_SCREENS_UM_OP_LOBBY_LOCK: u32 = 10;
/// `mInScenarioEditor = true`, `gGameStateIsMultiplayer = false`.
pub const EE_SCREENS_UM_OP_EDITOR_ON: u32 = 11;
/// The old communications deleted; a local `EEServer` ("EEName", the network adapter) made, its threads started,
/// no load responses.
pub const EE_SCREENS_UM_OP_NEW_SERVER: u32 = 12;
/// The lobby options' map size 1, load type 3, the prehistoric epoch, variant 0.
pub const EE_SCREENS_UM_OP_LOBBY_DEFAULTS: u32 = 13;
/// `gResource->GetString(IDS_SE_HUMAN (a 0) or IDS_COMPUTER (a 1), the call's name a)`.
pub const EE_SCREENS_UM_OP_NAME: u32 = 14;
/// `gWorldTable.GetLongValue(kWorldStartingCitizensDefault)`.
pub const EE_SCREENS_UM_OP_CITIZENS: u32 = 15;
/// The lobby options' `mPlayers[slot] =` the [`EeScreensManagerPlayer`] at c.
pub const EE_SCREENS_UM_OP_SET_PLAYER: u32 = 16;
/// The server's `StopPing()`, `mLoadState = 1`, `ProcessLocalUserInput(new EEMStartLoading(1, TMGetTime()))`.
pub const EE_SCREENS_UM_OP_SERVER_LOAD: u32 = 17;
/// `gISKeyboard->GetEntity(key a)->JustPressed()` (a an `EE_SCREENS_UM_KEY_*`).
pub const EE_SCREENS_UM_OP_KEY_PRESSED: u32 = 20;
// (21, 22, 23: the playing movie's test, stop and wait, gone with the movies)
/// `GetCurrentForm() != NULL && !GetCurrentForm()->GetIsKeyboardCaptured() && !gLoadingSavedGame`.
pub const EE_SCREENS_UM_OP_KEYS_FREE: u32 = 24;
/// `EEPostF9Key()`.
pub const EE_SCREENS_UM_OP_POST_F9: u32 = 25;
/// `UIManager::UpdateKeyboard()`: its result.
pub const EE_SCREENS_UM_OP_BASE_KEYBOARD: u32 = 26;
// (27: the mouse press that stopped a movie)
/// `UIManager::UpdateMouse(the call's mouse location)`: its result.
pub const EE_SCREENS_UM_OP_BASE_MOUSE: u32 = 28;

/// The key `UpdateKeyboard` tests (EE_SCREENS_UM_OP_KEY_PRESSED's a; 0..2, Escape, Return and Space, stopped a movie).
pub const EE_SCREENS_UM_KEY_F9: i64 = 3;

// ---- the ids the port's controls use (the host `static_assert`s them against the game's names)

/// `kTextureResolutionControlID`: the settings' added controls' first id.
pub const EE_SCREENS_UM_TEXTURE_RESOLUTION_ID: i32 = 90001;
/// `kGameSettingsTextureResolutionComboBox`: the settings' added controls' first form index.
pub const EE_SCREENS_UM_TEXTURE_RESOLUTION_INDEX: i32 = 111;
/// `kTextureResolutionOriginalString`, `kTextureResolutionHelpString`.
pub const EE_SCREENS_UM_TEXTURE_RESOLUTION_STRING: i32 = 33100;
pub const EE_SCREENS_UM_TEXTURE_RESOLUTION_HELP: i32 = 33103;
/// `kFormLobby`.
pub const EE_SCREENS_UM_FORM_LOBBY: i32 = 26;
/// `kLobbyEngineControlID`, `kLobbyEngineHeader`, `kLobbyEngineCombo`, `kLobbyEngineLabel`.
pub const EE_SCREENS_UM_ENGINE_ID: i32 = 90100;
pub const EE_SCREENS_UM_ENGINE_HEADER: i32 = 274;
pub const EE_SCREENS_UM_ENGINE_COMBO: i32 = 275;
pub const EE_SCREENS_UM_ENGINE_LABEL: i32 = 291;
/// `kLobbyEngineClassicString`, `kLobbyEngineRandomString`, `kLobbyEngineHeaderString`, `kLobbyEngineHelpString`.
pub const EE_SCREENS_UM_ENGINE_CLASSIC: i32 = 33170;
pub const EE_SCREENS_UM_ENGINE_RANDOM: i32 = 33171;
pub const EE_SCREENS_UM_ENGINE_HEADER_STRING: i32 = 33173;
pub const EE_SCREENS_UM_ENGINE_HELP: i32 = 33175;

/// One `UIControlsTable::AddControlLike`'s arguments.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensManagerControl {
    pub template_id: i32,
    pub id: i32,
    pub form_index: i32,
    pub group: u32,
    pub left: f32,
    pub right: f32,
    pub top: f32,
    pub bottom: f32,
    pub string_begin: i32,
    pub string_end: i32,
    pub help: u32,
}

/// One `UIControlsTable::MoveControlsHorizontally`'s arguments.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensManagerMove {
    pub form: i32,
    pub first: i32,
    pub last: i32,
    pub left_delta: f32,
    pub right_delta: f32,
}

/// One lobby slot `StartScenarioEditor` sets: the player's fields, its name the call's name `name` (0 the human's,
/// 1 the computer's).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensManagerPlayer {
    pub slot: u32,
    pub color: u32,
    pub kind: u32,
    pub starting_citizens: u32,
    pub is_local: u8,
    pub is_ready: u8,
    pub name: u8,
    /// 0 (the record's last byte, so that it has no padding)
    pub reserved: u8,
}

/// The port's settings controls after the texture resolution box: (template, form index, group, left, right, top,
/// lines (-1 for a one-line control), first string, last string, help string). Combo boxes are copies of the video
/// card's (151): their lines are their strings, the rectangles include the open list.
type PortControl = (i32, i32, u32, f32, f32, f32, f32, i32, i32, u32);
const PORT_CONTROLS: [PortControl; 12] = [
    (151, 112, 1, 0.2473, 0.4614, 0.160, 2.0, 33110, 33111, 33150),
    (151, 113, 1, 0.2473, 0.4614, 0.215, 8.0, 33112, 33119, 33151),
    (151, 114, 1, 0.2473, 0.4614, 0.270, 4.0, 33120, 33123, 33152),
    (151, 115, 1, 0.5400, 0.7540, 0.160, 4.0, 33124, 33127, 33153),
    (151, 116, 1, 0.5400, 0.7540, 0.215, 3.0, 33128, 33130, 33154),
    (151, 117, 1, 0.5400, 0.7540, 0.270, 7.0, 33131, 33137, 33155),
    (151, 118, 3, 0.2473, 0.4614, 0.340, 9.0, 33138, 33146, 33156),
    (182, 119, 2, 0.2473, 0.4614, 0.560, -1.0, 33147, 0, 33157),
    (183, 120, 2, 0.2473, 0.4614, 0.587, -1.0, 0, 0, 33157),
    (151, 121, 2, 0.5400, 0.7540, 0.587, 6.0, 33148, 33148, 33158),
    (804, 122, 2, 0.5400, 0.7540, 0.535, -1.0, 33149, 0, 33159),
    (151, 123, 1, 0.3130, 0.5270, 0.490, 6.0, 33160, 33165, 33166),
];

/// The lobby's player column narrowed by this much, the columns right of it moved left by as much.
const LOBBY_PLAYER_SHRINK: f32 = 0.022;
/// The engine column's open list: at most this many lines, each this tall.
const ENGINE_LIST_LINES_MAX: u32 = 8;
const ENGINE_LIST_LINE: f32 = 0.034;
/// The lobby's slot rows (the database's: 0.1043 + slot * 0.0391) and their height.
const ENGINE_ROW_TOP: f32 = 0.104_302_48;
const ENGINE_ROW_STEP: f32 = 0.039_113_43;
const ENGINE_ROW_HEIGHT: f32 = 0.024_771_84;

fn add_like(h: &mut impl UiHost, c: EeScreensManagerControl) {
    h.op(EE_SCREENS_UM_OP_ADD_LIKE, 0, 0, &c as *const EeScreensManagerControl as i64);
}

fn move_h(h: &mut impl UiHost, first: i32, last: i32, left_delta: f32, right_delta: f32) {
    let m = EeScreensManagerMove { form: EE_SCREENS_UM_FORM_LOBBY, first, last, left_delta, right_delta };
    h.op(EE_SCREENS_UM_OP_MOVE, 0, 0, &m as *const EeScreensManagerMove as i64);
}

/// `EEUIManager::EEUIManager` (0x671319) after its base: the factories, the database, the port's controls, the
/// forms, the music volume, the save form's activate event.
pub fn construct(h: &mut impl UiHost) {
    h.op(EE_SCREENS_UM_OP_REGISTER, 0, 0, 0);
    h.op(EE_SCREENS_UM_OP_LOAD_DATABASE, 0, 0, 0);
    // the settings' texture resolution combo box, in the video tab's right column below the weather effects check
    // box: a copy of the video card combo box (151) with the strings 33100..33102
    add_like(
        h,
        EeScreensManagerControl {
            template_id: 151,
            id: EE_SCREENS_UM_TEXTURE_RESOLUTION_ID,
            form_index: EE_SCREENS_UM_TEXTURE_RESOLUTION_INDEX,
            group: 1,
            left: 0.2473,
            right: 0.4614,
            top: 0.325,
            bottom: 0.325 + 0.025 + 3.0 * 0.022,
            string_begin: EE_SCREENS_UM_TEXTURE_RESOLUTION_STRING,
            string_end: EE_SCREENS_UM_TEXTURE_RESOLUTION_STRING + 2,
            help: EE_SCREENS_UM_TEXTURE_RESOLUTION_HELP as u32,
        },
    );
    // the modern display settings, the zoom-out, the master volume, the output device, the background mute
    for (i, &(template_id, form_index, group, left, right, top, lines, begin, end, help)) in
        PORT_CONTROLS.iter().enumerate()
    {
        let bottom = if lines >= 0.0 { top + 0.025 + lines * 0.022 } else { top + 0.0245 };
        add_like(
            h,
            EeScreensManagerControl {
                template_id,
                id: EE_SCREENS_UM_TEXTURE_RESOLUTION_ID + 1 + i as i32,
                form_index,
                group,
                left,
                right,
                top,
                bottom,
                string_begin: begin,
                string_end: end,
                help,
            },
        );
    }
    // the lobby's player column narrower (the name combo boxes 30..45 and labels 46..61), the columns right of it
    // (the headers 3..6, the slots' controls 62..173) moved left by as much
    move_h(h, 30, 61, 0.0, -LOBBY_PLAYER_SHRINK);
    move_h(h, 3, 6, -LOBBY_PLAYER_SHRINK, -LOBBY_PLAYER_SHRINK);
    move_h(h, 62, 173, -LOBBY_PLAYER_SHRINK, -LOBBY_PLAYER_SHRINK);
    // the lobby's engine column where the ping column is: the header a copy of the ping label (2507), per slot a
    // combo box like the citizens combo (2629) and a label like the citizens label (2645)
    let left = 0.4282 - LOBBY_PLAYER_SHRINK;
    let right = 0.5200 - LOBBY_PLAYER_SHRINK;
    let lines = (h.op(EE_SCREENS_UM_OP_ENGINE_COUNT, 0, 0, 0) as u32).min(ENGINE_LIST_LINES_MAX);
    add_like(
        h,
        EeScreensManagerControl {
            template_id: 2507,
            id: EE_SCREENS_UM_ENGINE_ID,
            form_index: EE_SCREENS_UM_ENGINE_HEADER,
            group: 0,
            left,
            right,
            top: 0.058_670_145,
            bottom: 0.102_998_696,
            string_begin: EE_SCREENS_UM_ENGINE_HEADER_STRING,
            string_end: 0,
            help: EE_SCREENS_UM_ENGINE_HELP as u32,
        },
    );
    for slot in 0..16i32 {
        let top = ENGINE_ROW_TOP + slot as f32 * ENGINE_ROW_STEP;
        add_like(
            h,
            EeScreensManagerControl {
                template_id: 2629,
                id: EE_SCREENS_UM_ENGINE_ID + 1 + slot,
                form_index: EE_SCREENS_UM_ENGINE_COMBO + slot,
                group: 1,
                left,
                right,
                top,
                bottom: top + 0.025 + lines as f32 * ENGINE_LIST_LINE,
                string_begin: EE_SCREENS_UM_ENGINE_CLASSIC,
                string_end: EE_SCREENS_UM_ENGINE_RANDOM,
                help: EE_SCREENS_UM_ENGINE_HELP as u32,
            },
        );
        add_like(
            h,
            EeScreensManagerControl {
                template_id: 2645,
                id: EE_SCREENS_UM_ENGINE_ID + 17 + slot,
                form_index: EE_SCREENS_UM_ENGINE_LABEL + slot,
                group: 1,
                left,
                right,
                top,
                bottom: top + ENGINE_ROW_HEIGHT,
                string_begin: EE_SCREENS_UM_ENGINE_CLASSIC,
                string_end: 0,
                help: EE_SCREENS_UM_ENGINE_HELP as u32,
            },
        );
    }
    h.op(EE_SCREENS_UM_OP_FORMS, 0, 0, 0);
    h.op(EE_SCREENS_UM_OP_MUSIC, 0, 0, 0);
    h.op(EE_SCREENS_UM_OP_SAVE_EVENT, 0, 0, 0);
}

fn set_player(h: &mut impl UiHost, p: EeScreensManagerPlayer) {
    h.op(EE_SCREENS_UM_OP_SET_PLAYER, 0, 0, &p as *const EeScreensManagerPlayer as i64);
}

/// `EEUIManager::StartScenarioEditor` (0x674e69): under the lobby options' lock, a local server, the options'
/// defaults, a ready local human in slot 0 (colour 0) and ready computers in slots 1..15 (colour = slot), each
/// with the default starting citizens; then the load.
pub fn start_scenario_editor(h: &mut impl UiHost) {
    h.op(EE_SCREENS_UM_OP_LOBBY_LOCK, 1, 0, 0);
    h.op(EE_SCREENS_UM_OP_EDITOR_ON, 0, 0, 0);
    h.op(EE_SCREENS_UM_OP_NEW_SERVER, 0, 0, 0);
    h.op(EE_SCREENS_UM_OP_LOBBY_DEFAULTS, 0, 0, 0);
    h.op(EE_SCREENS_UM_OP_NAME, 0, 0, 0);
    let citizens = h.op(EE_SCREENS_UM_OP_CITIZENS, 0, 0, 0) as u32;
    set_player(
        h,
        EeScreensManagerPlayer {
            slot: 0,
            color: 0,
            kind: 1,
            starting_citizens: citizens,
            is_local: 1,
            is_ready: 1,
            name: 0,
            reserved: 0,
        },
    );
    h.op(EE_SCREENS_UM_OP_NAME, 1, 0, 0);
    for slot in 1..16u32 {
        let citizens = h.op(EE_SCREENS_UM_OP_CITIZENS, 0, 0, 0) as u32;
        set_player(
            h,
            EeScreensManagerPlayer {
                slot,
                color: slot,
                kind: 0,
                starting_citizens: citizens,
                is_local: 0,
                is_ready: 1,
                name: 1,
                reserved: 0,
            },
        );
    }
    h.op(EE_SCREENS_UM_OP_SERVER_LOAD, 0, 0, 0);
    h.op(EE_SCREENS_UM_OP_LOBBY_LOCK, 0, 0, 0);
}

/// `EEUIManager::UpdateKeyboard` (0x6750a9): F9 just pressed outside text entry (and not while a saved game loads)
/// posts the screenshot and ends the update (0); otherwise the base manager's keyboard update. (The original first
/// stopped a playing movie on Escape, Return or Space; no movie plays.)
pub fn update_keyboard(h: &mut impl UiHost) -> i64 {
    if h.op(EE_SCREENS_UM_OP_KEYS_FREE, 0, 0, 0) != 0
        && h.op(EE_SCREENS_UM_OP_KEY_PRESSED, EE_SCREENS_UM_KEY_F9, 0, 0) != 0
    {
        h.op(EE_SCREENS_UM_OP_POST_F9, 0, 0, 0);
        return 0;
    }
    h.op(EE_SCREENS_UM_OP_BASE_KEYBOARD, 0, 0, 0)
}

/// `EEUIManager::UpdateMouse` (0x6751ab): the base manager's mouse update. (The original first stopped a playing
/// movie on a left or right press; no movie plays.)
pub fn update_mouse(h: &mut impl UiHost) -> i64 {
    h.op(EE_SCREENS_UM_OP_BASE_MOUSE, 0, 0, 0)
}

#[cfg(test)]
mod tests;
