//! The screens' read view of the game (round 4 of game screens B): the values and texts the screens read from the
//! game settings, the session, the world, the players and the campaign and intro managers, a group at a time.
//!
//! `EEScreens.cpp`'s host had one C++ `case` per read (`return gFreeCamera;`, `theText = gPlayerName;`). A read is
//! now answered here: the op names its group ([`group_of`]), one C++ fill (`ee_screenscpp_game_view`, or
//! `ee_screenscpp_game_texts` for the texts) copies that group's values into a record ([`EeScreensGameView`]) or a
//! 0-joined text block at the moment of the read, and [`value`] picks the op's answer from it. A group holds only
//! the reads of one set of objects, filled only when an op of that group is asked: a fill touches exactly the
//! objects the C++ case touched (the campaign and intro managers are made on first use by their `Get`, the current
//! player is read only by the player ops), and every pointer it follows it checks (an answer of 0 or "" where the
//! C++ case would have dereferenced NULL).
//!
//! The players are read through their accessors (`GetName`, `GetColor`, `GetObserverMode`), never stored: the
//! player lane owns their storage. The sound manager's and the conductor's ops go straight to their Rust objects
//! (`ee_sound`), the same calls `SESoundManager`'s and `SEConductor`'s forwarders make; the chat log's reads to its
//! Rust store; the colour ops to the toolkit's Rust controls (`ee_ui direct.rs`) with the colour table's colour.

// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the ops' `u32` stay
#![allow(clippy::unnecessary_cast)]

use crate::camera::*;
use crate::campaign_editor::EE_SCREENS_TEXT_CE_TRANSLATED;
use crate::chat::{
    EE_SCREENS_GAME_CH_CHEATS, EE_SCREENS_GAME_CH_OBSERVER, EE_SCREENS_GAME_CH_PLAYER_TYPE, EE_SCREENS_GAME_CL_COUNT,
    EE_SCREENS_TEXT_CL_NAME, EE_SCREENS_TEXT_CL_PLAYER, EE_SCREENS_TEXT_CL_TEXT,
};
use crate::custom_campaign::{EE_SCREENS_TEXT_CAMPAIGN_ERROR, EE_SCREENS_TEXT_CAMPAIGN_EXTENSION};
use crate::diplomacy::{
    EE_SCREENS_GAME_DS_CURRENT, EE_SCREENS_GAME_DS_PLAYERS, EE_SCREENS_GAME_DS_TEAMS_LOCKED,
    EE_SCREENS_TEXT_DS_CIVILIZATION, EE_SCREENS_TEXT_DS_NAME,
};
use crate::lobby_form::{
    EE_SCREENS_GAME_LE_HEADER_PLAYERS, EE_SCREENS_GAME_LE_IS_CLIENT, EE_SCREENS_OP_LE_PLAYER_COLOUR,
    EE_SCREENS_TEXT_LE_MAP_TYPE, EE_SCREENS_TEXT_LE_MISSING,
};
use crate::main_game::*;
use crate::main_game_options::{
    EE_SCREENS_GAME_GO_CANNOT_EXIT, EE_SCREENS_GAME_GO_FORM_EXISTS, EE_SCREENS_GAME_GO_HIDE_RESIGN,
    EE_SCREENS_GAME_GO_USER_PAUSED,
};
use crate::menus::*;
use crate::post_game::{
    EE_SCREENS_GAME_PGS_PAGE, EE_SCREENS_GAME_PGS_SCREENSHOTS, EE_SCREENS_TEXT_PGS_CAMPAIGN, EE_SCREENS_TEXT_PGS_NAME,
};
use crate::save_load::{
    EE_SCREENS_GAME_CTS_PROCESSED, EE_SCREENS_GAME_FREE_CAMERA, EE_SCREENS_GAME_MULTIPLAYER_SESSION,
    EE_SCREENS_GAME_SAVED_MAIN_FORM, EE_SCREENS_GAME_SCREEN_ACTIVE, EE_SCREENS_TEXT_LOAD_SAVED_NAME,
    EE_SCREENS_TEXT_LOAD_SCENARIO_NAME, EE_SCREENS_TEXT_SAVED_EXTENSION, EE_SCREENS_TEXT_SCENARIO_EXTENSION,
};
use crate::scenario_screens::{
    EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN_GET, EE_SCREENS_GAME_CAMPAIGN_CHECKSUM, EE_SCREENS_GAME_DIFFICULTY,
    EE_SCREENS_GAME_SI_FLAGS, EE_SCREENS_GAME_SOUND_LATENCY, EE_SCREENS_GAME_SOUND_STOP,
    EE_SCREENS_TEXT_CS_CAMPAIGN_NAME, EE_SCREENS_TEXT_CS_DESCRIPTION, EE_SCREENS_TEXT_SI_BRIEFING,
    EE_SCREENS_TEXT_SI_HINTS, EE_SCREENS_TEXT_SI_HISTORY,
};
use crate::settings_form::*;
use crate::splash::{
    EE_SCREENS_GAME_SS_HUMANS, EE_SCREENS_GAME_SS_PLAYER, EE_SCREENS_OP_SS_BAR_PLAYER_COLOUR, EE_SCREENS_TEXT_SS_NAME,
    EE_SCREENS_TEXT_SS_SUFFIX,
};

// ---- the groups (`ee_screenscpp_game_view`'s and `ee_screenscpp_game_texts`' first argument)

/// The sound manager and the mixer's conductor (`gSoundManager`, `&gConductor->GetConductor()`).
pub const EE_SCREENS_VIEW_SOUND: u32 = 0;
/// Plain globals: the settings, the display list, the camera's and the UI's flags, the loaded header's player count.
pub const EE_SCREENS_VIEW_SETTINGS: u32 = 1;
/// `gCommunications`' (all 0 without it).
pub const EE_SCREENS_VIEW_SESSION: u32 = 2;
/// `gWorld`'s, the world table's game speeds and the main game form's hot key mode.
pub const EE_SCREENS_VIEW_WORLD: u32 = 3;
/// A player's (the argument: its index, or [`EE_SCREENS_VIEW_CURRENT`]; all 0 for a missing one).
pub const EE_SCREENS_VIEW_PLAYER: u32 = 4;
/// The colour table's colour of the argument (`EEGetPlayerColor`).
pub const EE_SCREENS_VIEW_COLOUR: u32 = 5;
/// The campaign form manager's (`EECampaignFormManager::Get()`).
pub const EE_SCREENS_VIEW_CAMPAIGN: u32 = 6;
/// The scenario intro's form manager's (`EEUISCIFormManager::Get()`).
pub const EE_SCREENS_VIEW_INTRO: u32 = 7;
/// The player argument for the current player (`gWorld.GetCurrentPlayer()`).
pub const EE_SCREENS_VIEW_CURRENT: u32 = 0xffff_ffff;

/// Texts: the settings' and the plain globals' (`EE_SCREENS_VIEW_T_*` in order).
pub const EE_SCREENS_VIEW_TEXTS_SETTINGS: u32 = 0;
/// Texts: the campaign form manager's (loaded description, required civilization, name, description).
pub const EE_SCREENS_VIEW_TEXTS_CAMPAIGN: u32 = 1;
/// Texts: the intro manager's (briefing, hints, history).
pub const EE_SCREENS_VIEW_TEXTS_INTRO: u32 = 2;
/// Texts: a player's (name, civilization name; the argument as [`EE_SCREENS_VIEW_PLAYER`]'s).
pub const EE_SCREENS_VIEW_TEXTS_PLAYER: u32 = 3;
/// Texts: the world's (the statistics' campaign name).
pub const EE_SCREENS_VIEW_TEXTS_WORLD: u32 = 4;
/// Texts: a statistics record's (the argument: its index; its name).
pub const EE_SCREENS_VIEW_TEXTS_RECORD: u32 = 5;
/// Texts: the map type of a lobby options source (the argument: `EE_SCREENS_LE_SOURCE_*`).
pub const EE_SCREENS_VIEW_TEXTS_MAP_TYPE: u32 = 6;

// the settings texts' slots
const T_RASTERIZER: usize = 0; // the settings' rasterizer name, then the defaults'
const T_AUDIO: usize = 2; // the settings' audio device, then the defaults'
const T_VERSION: usize = 4;
const T_PLAYER: usize = 5;
const T_CAMPAIGNS: usize = 6; // four: gCampaignAOCRoman, ..Pacific, ..Asian, gCampaignEELearning
const T_SAVED_EXTENSION: usize = 10;
const T_SCENARIO_EXTENSION: usize = 11;
const T_CAMPAIGN_EXTENSION: usize = 12;
const T_SS_SUFFIX: usize = 13;
const T_CAMPAIGN_ERROR: usize = 14;
const T_LOAD_SAVED_NAME: usize = 15;
const T_LOAD_SCENARIO_NAME: usize = 16;
const T_LE_MISSING: usize = 17;
const T_CE_TRANSLATED: usize = 18;

/// The values of a group (only the group's fields are filled, the rest are 0).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensGameView {
    // EE_SCREENS_VIEW_SOUND
    pub sound_manager: usize,
    pub conductor: usize,
    // EE_SCREENS_VIEW_SETTINGS
    /// `gRasterizer`'s index among `GERasterizer::GetRasterizer` (0 when it is none of them).
    pub current_rasterizer: u32,
    /// `gDisplayRasterizers.size()`.
    pub rasterizer_count: u32,
    pub not_korean: bool,
    /// The settings' `mLastOpenPostGameStatsPage`, `mTakeEndGameScreenshots`, `GetGrabScrollSpeed()`, `mZoomEnabled`.
    pub pgs_page: i32,
    pub pgs_screenshots: bool,
    pub grab_scroll_speed: f32,
    pub settings_zoom_enabled: bool,
    /// `gZoomEnabled`, `gDifficultyLevel`, `gFreeCamera`, `gGameScreenActive`, `gSavedMainGameForm != NULL`,
    /// `gCameraMaxZoom`, `gHiddenUnitsMode`.
    pub zoom_enabled: bool,
    pub difficulty: i32,
    pub free_camera: bool,
    pub screen_active: bool,
    pub saved_main_form: bool,
    pub max_zoom: f32,
    pub hidden_mode: i32,
    /// The loaded header's `mNumPlayers` (0 without one).
    pub header_players: u32,
    // EE_SCREENS_VIEW_SESSION
    pub is_server: bool,
    pub is_client: bool,
    pub multiplayer_session: bool,
    pub cts_processed: u32,
    /// The server migrating, or the client a playback session.
    pub cannot_exit: bool,
    pub humans: u32,
    // EE_SCREENS_VIEW_WORLD
    pub ghosts: u32,
    pub has_player: bool,
    pub camera_mode: i32,
    pub lock_speed: bool,
    pub game_speed: f32,
    pub cinematic: bool,
    pub zoom: f32,
    pub user_paused: bool,
    pub players: u32,
    pub current: i32,
    pub teams_locked: bool,
    pub cheats: bool,
    /// `gWorld.GetGameOptions().GetIsScenarioGame()`.
    pub scenario_game: bool,
    /// `gWorldTable`'s game speed step and range (`kWorldGameSpeedStep`, `kWorldGameSpeedRange` and its second).
    pub speed_step: f32,
    pub speed_low: f32,
    pub speed_high: f32,
    /// The manager's main game form's `mHotKeyMode` (0 without the form).
    pub hot_key_mode: u32,
    // EE_SCREENS_VIEW_PLAYER
    pub present: bool,
    pub observer: bool,
    pub chat_type: u32,
    /// An airport among the player's selected group.
    pub airport_selected: bool,
    // EE_SCREENS_VIEW_PLAYER (the player's `GetColor()`) and EE_SCREENS_VIEW_COLOUR (the table's)
    pub colour: [f32; 3],
    // EE_SCREENS_VIEW_CAMPAIGN
    pub campaign_checksum: u32,
    pub all_scenarios_open: bool,
    // EE_SCREENS_VIEW_INTRO
    /// `EE_SCREENS_SI_FLAG_*` of the intro manager's four flags.
    pub si_flags: u32,
}

/// What the view's ops reach outside the crate.
pub(crate) trait ViewHost {
    /// A group's values (`ee_screenscpp_game_view`).
    fn view(&mut self, group: u32, arg: u32) -> EeScreensGameView;
    /// A text group's texts, each ended by a 0 byte (`ee_screenscpp_game_texts`).
    fn texts(&mut self, group: u32, arg: u32) -> Vec<u8>;
    /// `SEConductor`'s, on the conductor `c`: `SetVolume`'s result, `GetVolume`, `SetReverb`, `SetConductorState`,
    /// `GetConductorState`.
    fn conductor(&mut self, c: usize, op: Sound, value: u32) -> u32;
    /// `SESoundManager`'s, on the manager `m`: `SetMasterVolume`'s result, `GetMasterVolume`, `GetMaxChannels`,
    /// `SetMaxChannels`, `Stop(id, false)`, `SetLatency`.
    fn sound(&mut self, m: usize, op: Sound, value: u32) -> u32;
    /// The chat log's entry count, an entry's sender (`which` 0) or text (1).
    fn chat_log_count(&mut self) -> u32;
    fn chat_log_text(&mut self, index: u32, which: u32) -> Vec<u8>;
    /// The manager's form of an id (`gUIManager->GetFormByID`) is there.
    fn form_exists(&mut self, id: u32) -> bool;
    /// A lobby slot (`gWorld.GetLobbyOptions().mPlayers[index]`): its type, local flag, colour and name.
    fn lobby_slot(&mut self, index: u32) -> (u32, bool, u32, Vec<u8>);
    /// `control->SetBgColor(colour)` on the event's form's control.
    fn set_bg_colour(&mut self, control: u32, colour: [f32; 3]);
    /// The status bar's `SetStatusBarColor(colour)` on the event's form's control.
    fn set_bar_colour(&mut self, control: u32, colour: [f32; 3]);
}

/// The sound objects' ops.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Sound {
    SetVolume,
    Volume,
    SetReverb,
    SetState,
    State,
    SetMasterVolume,
    MasterVolume,
    MaxChannels,
    SetMaxChannels,
    Stop,
    SetLatency,
}

/// The group (and its argument) a read op is answered from; `None` for an op the view does not answer.
pub(crate) fn group_of(op: u32) -> Option<(u32, u32)> {
    let g = match op {
        EE_SCREENS_GAME_CURRENT_RASTERIZER
        | EE_SCREENS_GAME_RASTERIZER_COUNT
        | EE_SCREENS_GAME_NOT_KOREAN
        | EE_SCREENS_GAME_PGS_PAGE
        | EE_SCREENS_GAME_PGS_SCREENSHOTS
        | EE_SCREENS_GAME_CAM_GRAB_SPEED
        | EE_SCREENS_GAME_CAM_ZOOM_ENABLED
        | EE_SCREENS_GAME_DIFFICULTY
        | EE_SCREENS_GAME_FREE_CAMERA
        | EE_SCREENS_GAME_CAM_FREE
        | EE_SCREENS_GAME_SCREEN_ACTIVE
        | EE_SCREENS_GAME_SAVED_MAIN_FORM
        | EE_SCREENS_GAME_CAM_MAX_ZOOM
        | EE_SCREENS_GAME_MG_HIDDEN_MODE
        | EE_SCREENS_GAME_LE_HEADER_PLAYERS => EE_SCREENS_VIEW_SETTINGS,
        EE_SCREENS_GAME_IS_SERVER
        | EE_SCREENS_GAME_LE_IS_CLIENT
        | EE_SCREENS_GAME_MULTIPLAYER_SESSION
        | EE_SCREENS_GAME_CTS_PROCESSED
        | EE_SCREENS_GAME_GO_CANNOT_EXIT
        | EE_SCREENS_GAME_SS_HUMANS => EE_SCREENS_VIEW_SESSION,
        EE_SCREENS_GAME_MG_GHOSTS
        | EE_SCREENS_GAME_MG_HAS_PLAYER
        | EE_SCREENS_GAME_MG_CAMERA_MODE
        | EE_SCREENS_GAME_MG_LOCK_SPEED
        | EE_SCREENS_GAME_MG_GAME_SPEED
        | EE_SCREENS_GAME_CAM_CINEMATIC
        | EE_SCREENS_GAME_CAM_ZOOM
        | EE_SCREENS_GAME_GO_USER_PAUSED
        | EE_SCREENS_GAME_DS_PLAYERS
        | EE_SCREENS_GAME_DS_CURRENT
        | EE_SCREENS_GAME_DS_TEAMS_LOCKED
        | EE_SCREENS_GAME_CH_CHEATS
        | EE_SCREENS_GAME_MG_SPEED_TABLE
        | EE_SCREENS_GAME_MG_HOT_KEY_MODE => EE_SCREENS_VIEW_WORLD,
        EE_SCREENS_GAME_CH_OBSERVER | EE_SCREENS_GAME_CH_PLAYER_TYPE | EE_SCREENS_GAME_MG_AIRPORT_SELECTED => {
            return Some((EE_SCREENS_VIEW_PLAYER, EE_SCREENS_VIEW_CURRENT));
        }
        EE_SCREENS_GAME_CAMPAIGN_CHECKSUM | EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN_GET => EE_SCREENS_VIEW_CAMPAIGN,
        EE_SCREENS_GAME_SI_FLAGS => EE_SCREENS_VIEW_INTRO,
        _ => return None,
    };
    Some((g, 0))
}

/// A read op's answer from its group's values (as the C++ case returned it: bools 0 or 1, floats as their bits).
pub(crate) fn value(op: u32, a: u32, v: &EeScreensGameView) -> u32 {
    let b = |x: bool| x as u32;
    match op {
        EE_SCREENS_GAME_CURRENT_RASTERIZER => v.current_rasterizer,
        EE_SCREENS_GAME_RASTERIZER_COUNT => v.rasterizer_count,
        EE_SCREENS_GAME_NOT_KOREAN => b(v.not_korean),
        EE_SCREENS_GAME_PGS_PAGE => v.pgs_page as u32,
        EE_SCREENS_GAME_PGS_SCREENSHOTS => b(v.pgs_screenshots),
        EE_SCREENS_GAME_CAM_GRAB_SPEED => v.grab_scroll_speed.to_bits(),
        // (a: the wheel's gZoomEnabled, else the settings')
        EE_SCREENS_GAME_CAM_ZOOM_ENABLED => b(if a != 0 { v.zoom_enabled } else { v.settings_zoom_enabled }),
        EE_SCREENS_GAME_DIFFICULTY => v.difficulty as u32,
        EE_SCREENS_GAME_FREE_CAMERA | EE_SCREENS_GAME_CAM_FREE => b(v.free_camera),
        EE_SCREENS_GAME_SCREEN_ACTIVE => b(v.screen_active),
        EE_SCREENS_GAME_SAVED_MAIN_FORM => b(v.saved_main_form),
        EE_SCREENS_GAME_CAM_MAX_ZOOM => v.max_zoom.to_bits(),
        EE_SCREENS_GAME_MG_HIDDEN_MODE => v.hidden_mode as u32,
        EE_SCREENS_GAME_MG_SPEED_TABLE => match a {
            0 => v.speed_step,
            1 => v.speed_low,
            _ => v.speed_high,
        }
        .to_bits(),
        EE_SCREENS_GAME_LE_HEADER_PLAYERS => v.header_players,
        EE_SCREENS_GAME_MG_HOT_KEY_MODE => v.hot_key_mode,
        EE_SCREENS_GAME_IS_SERVER => b(v.is_server),
        EE_SCREENS_GAME_LE_IS_CLIENT => b(v.is_client),
        EE_SCREENS_GAME_MULTIPLAYER_SESSION => b(v.multiplayer_session),
        EE_SCREENS_GAME_CTS_PROCESSED => v.cts_processed,
        EE_SCREENS_GAME_GO_CANNOT_EXIT => b(v.cannot_exit),
        EE_SCREENS_GAME_SS_HUMANS => v.humans,
        EE_SCREENS_GAME_MG_GHOSTS => v.ghosts,
        EE_SCREENS_GAME_MG_HAS_PLAYER => b(v.has_player),
        EE_SCREENS_GAME_MG_CAMERA_MODE => v.camera_mode as u32,
        EE_SCREENS_GAME_MG_LOCK_SPEED => b(v.lock_speed),
        EE_SCREENS_GAME_MG_GAME_SPEED => v.game_speed.to_bits(),
        EE_SCREENS_GAME_CAM_CINEMATIC => b(v.cinematic),
        EE_SCREENS_GAME_CAM_ZOOM => v.zoom.to_bits(),
        EE_SCREENS_GAME_GO_USER_PAUSED => b(v.user_paused),
        EE_SCREENS_GAME_DS_PLAYERS => v.players,
        EE_SCREENS_GAME_DS_CURRENT => v.current as u32,
        EE_SCREENS_GAME_DS_TEAMS_LOCKED => b(v.teams_locked),
        EE_SCREENS_GAME_CH_CHEATS => b(v.cheats),
        EE_SCREENS_GAME_CH_OBSERVER => b(v.observer),
        EE_SCREENS_GAME_CH_PLAYER_TYPE => v.chat_type,
        EE_SCREENS_GAME_MG_AIRPORT_SELECTED => b(v.airport_selected),
        EE_SCREENS_GAME_CAMPAIGN_CHECKSUM => v.campaign_checksum,
        EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN_GET => b(v.all_scenarios_open),
        EE_SCREENS_GAME_SI_FLAGS => v.si_flags,
        _ => 0,
    }
}

/// The ops the view answers: the reads, the sound objects', the chat log's count, the form question, the loading
/// screen's slot and the two colour ops. `None` for the others (the C++ host's).
pub(crate) fn op<H: ViewHost + ?Sized>(h: &mut H, op: u32, a: u32, b: u32) -> Option<u32> {
    if let Some((group, arg)) = group_of(op) {
        return Some(value(op, a, &h.view(group, arg)));
    }
    let sound = |h: &mut H, s: Sound, value: u32, conductor: bool| {
        let v = h.view(EE_SCREENS_VIEW_SOUND, 0);
        let target = if conductor { v.conductor } else { v.sound_manager };
        // (the C++ called through a NULL manager; nothing is done)
        if target == 0 {
            return 0;
        }
        if conductor {
            h.conductor(target, s, value)
        } else {
            h.sound(target, s, value)
        }
    };
    let r = match op {
        EE_SCREENS_GAME_GO_HIDE_RESIGN => {
            // the current player observes, or the game is a scenario (the second read only when it does not)
            let observer = h.view(EE_SCREENS_VIEW_PLAYER, EE_SCREENS_VIEW_CURRENT).observer;
            (observer || h.view(EE_SCREENS_VIEW_WORLD, 0).scenario_game) as u32
        }
        EE_SCREENS_GAME_CONDUCTOR_VOLUME => sound(h, Sound::SetVolume, a, true),
        EE_SCREENS_GAME_GET_CONDUCTOR_VOLUME => sound(h, Sound::Volume, 0, true),
        EE_SCREENS_GAME_REVERB => {
            sound(h, Sound::SetReverb, a, true);
            0
        }
        EE_SCREENS_GAME_CONDUCTOR_STATE => {
            sound(h, Sound::SetState, a, true);
            0
        }
        EE_SCREENS_GAME_GET_CONDUCTOR_STATE => sound(h, Sound::State, 0, true),
        EE_SCREENS_GAME_MASTER_VOLUME => sound(h, Sound::SetMasterVolume, a, false),
        EE_SCREENS_GAME_GET_MASTER_VOLUME => sound(h, Sound::MasterVolume, 0, false),
        EE_SCREENS_GAME_GET_CHANNELS => sound(h, Sound::MaxChannels, 0, false),
        EE_SCREENS_GAME_CHANNELS => {
            sound(h, Sound::SetMaxChannels, a, false);
            0
        }
        EE_SCREENS_GAME_SOUND_STOP => {
            sound(h, Sound::Stop, a, false);
            0
        }
        EE_SCREENS_GAME_SOUND_LATENCY => {
            sound(h, Sound::SetLatency, a, false);
            0
        }
        EE_SCREENS_GAME_CL_COUNT => h.chat_log_count(),
        EE_SCREENS_GAME_GO_FORM_EXISTS => h.form_exists(a) as u32,
        EE_SCREENS_GAME_SS_PLAYER => {
            let (kind, local, _, _) = h.lobby_slot(a);
            (kind & 0xff) | if local { 1 << 8 } else { 0 }
        }
        EE_SCREENS_OP_LE_PLAYER_COLOUR => {
            let colour = h.view(EE_SCREENS_VIEW_COLOUR, b).colour;
            h.set_bg_colour(a, colour);
            0
        }
        EE_SCREENS_OP_SS_BAR_PLAYER_COLOUR => {
            let (_, _, slot_colour, _) = h.lobby_slot(b);
            let colour = h.view(EE_SCREENS_VIEW_COLOUR, slot_colour).colour;
            h.set_bar_colour(a, colour);
            0
        }
        _ => return None,
    };
    Some(r)
}

/// The `n`th 0-ended text of a block (empty past its end).
fn nth(block: &[u8], n: usize) -> Vec<u8> {
    block.split(|&c| c == 0).nth(n).map(<[u8]>::to_vec).unwrap_or_default()
}

/// The texts the view answers; `None` for the others (the C++ host's).
pub(crate) fn text<H: ViewHost + ?Sized>(h: &mut H, which: u32, arg: u32) -> Option<Vec<u8>> {
    let (group, garg, slot) = match which {
        EE_SCREENS_TEXT_RASTERIZER_SETTING => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_RASTERIZER + (arg != 0) as usize),
        EE_SCREENS_TEXT_AUDIO_DEVICE => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_AUDIO + (arg != 0) as usize),
        EE_SCREENS_TEXT_VERSION => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_VERSION),
        EE_SCREENS_TEXT_PLAYER => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_PLAYER),
        EE_SCREENS_TEXT_CAMPAIGN => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_CAMPAIGNS + (arg & 3) as usize),
        EE_SCREENS_TEXT_SAVED_EXTENSION => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_SAVED_EXTENSION),
        EE_SCREENS_TEXT_SCENARIO_EXTENSION => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_SCENARIO_EXTENSION),
        EE_SCREENS_TEXT_CAMPAIGN_EXTENSION => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_CAMPAIGN_EXTENSION),
        EE_SCREENS_TEXT_SS_SUFFIX => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_SS_SUFFIX),
        EE_SCREENS_TEXT_CAMPAIGN_ERROR => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_CAMPAIGN_ERROR),
        EE_SCREENS_TEXT_LOAD_SAVED_NAME => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_LOAD_SAVED_NAME),
        EE_SCREENS_TEXT_LOAD_SCENARIO_NAME => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_LOAD_SCENARIO_NAME),
        EE_SCREENS_TEXT_LE_MISSING => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_LE_MISSING),
        EE_SCREENS_TEXT_CE_TRANSLATED => (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, T_CE_TRANSLATED),
        EE_SCREENS_TEXT_CAMPAIGN_LOADED => (EE_SCREENS_VIEW_TEXTS_CAMPAIGN, 0, 0),
        EE_SCREENS_TEXT_CAMPAIGN_CIV => (EE_SCREENS_VIEW_TEXTS_CAMPAIGN, 0, 1),
        EE_SCREENS_TEXT_CS_CAMPAIGN_NAME => (EE_SCREENS_VIEW_TEXTS_CAMPAIGN, 0, 2),
        EE_SCREENS_TEXT_CS_DESCRIPTION => (EE_SCREENS_VIEW_TEXTS_CAMPAIGN, 0, 3),
        EE_SCREENS_TEXT_SI_BRIEFING => (EE_SCREENS_VIEW_TEXTS_INTRO, 0, 0),
        EE_SCREENS_TEXT_SI_HINTS => (EE_SCREENS_VIEW_TEXTS_INTRO, 0, 1),
        EE_SCREENS_TEXT_SI_HISTORY => (EE_SCREENS_VIEW_TEXTS_INTRO, 0, 2),
        EE_SCREENS_TEXT_CL_PLAYER => (EE_SCREENS_VIEW_TEXTS_PLAYER, EE_SCREENS_VIEW_CURRENT, 0),
        EE_SCREENS_TEXT_DS_NAME => (EE_SCREENS_VIEW_TEXTS_PLAYER, arg, 0),
        EE_SCREENS_TEXT_DS_CIVILIZATION => (EE_SCREENS_VIEW_TEXTS_PLAYER, arg, 1),
        EE_SCREENS_TEXT_PGS_CAMPAIGN => (EE_SCREENS_VIEW_TEXTS_WORLD, 0, 0),
        EE_SCREENS_TEXT_PGS_NAME => (EE_SCREENS_VIEW_TEXTS_RECORD, arg, 0),
        EE_SCREENS_TEXT_LE_MAP_TYPE => (EE_SCREENS_VIEW_TEXTS_MAP_TYPE, arg, 0),
        EE_SCREENS_TEXT_SS_NAME => return Some(h.lobby_slot(arg).3),
        EE_SCREENS_TEXT_CL_NAME => return Some(h.chat_log_text(arg, 0)),
        EE_SCREENS_TEXT_CL_TEXT => return Some(h.chat_log_text(arg, 1)),
        _ => return None,
    };
    Some(nth(&h.texts(group, garg), slot))
}

#[cfg(test)]
#[path = "game_view_golden.rs"]
mod golden;
#[cfg(test)]
#[path = "game_view_tests.rs"]
mod tests;
