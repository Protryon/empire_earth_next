//! The game settings form whole (form 5: the video, audio and interface tabs, opened from the main menu or the game):
//! its events, state and every call the original `EEUIEGameSettingsEvents.cpp` made, over a host
//! ([`SettingsHost`]) whose methods are each one toolkit or engine call (`UI Events/EEScreens.cpp`). The decisions
//! are [`settings`](crate::settings)' (the presets, the plans of the tabs, the port's value tables, the values
//! `ApplySettings` takes from the controls).
//!
//! The form's events are codes `0x05nn` (`EE_SCREENS_GS_*`); the control-event table is [`event_for_control`] (the
//! original's `EECreateGameSettingsEvent`). Its state ([`SettingsForm`]: the form's music while it is open, whether it
//! was opened from the game, the output devices once listed, the port's display change and quality guard, the last
//! rasterizer and screen names) is a record locked only around its own reads and writes, never across a host call.

// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the host's `u32` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_long, c_ulong};
use std::sync::Mutex;

use crate::settings::*;

// ---- the events (EEScreenEvent::mEvent)

/// Opening the form from the main menu, from the game (form-open events), the two OK buttons they hang.
pub const EE_SCREENS_GS_OUT_OF_GAME_OPEN: u32 = 0x0501;
pub const EE_SCREENS_GS_IN_GAME_OPEN: u32 = 0x0502;
pub const EE_SCREENS_GS_OUT_OF_GAME_OK: u32 = 0x0503;
pub const EE_SCREENS_GS_IN_GAME_OK: u32 = 0x0504;
pub const EE_SCREENS_GS_CANCEL: u32 = 0x0505;
pub const EE_SCREENS_GS_RESTORE_DEFAULT: u32 = 0x0506;
pub const EE_SCREENS_GS_VIDEO_TAB: u32 = 0x0507;
pub const EE_SCREENS_GS_AUDIO_TAB: u32 = 0x0508;
pub const EE_SCREENS_GS_INTERFACE_TAB: u32 = 0x0509;
pub const EE_SCREENS_GS_VIDEO_CARD: u32 = 0x050a;
pub const EE_SCREENS_GS_RENDERING_SYSTEM: u32 = 0x050b;
pub const EE_SCREENS_GS_MUSIC_VOLUME: u32 = 0x050c;
pub const EE_SCREENS_GS_SOUND_VOLUME: u32 = 0x050d;
pub const EE_SCREENS_GS_MUSIC_QUALITY: u32 = 0x050e;
pub const EE_SCREENS_GS_TEST_SOUND: u32 = 0x050f;
pub const EE_SCREENS_GS_GRAPHICS_QUALITY: u32 = 0x0510;
pub const EE_SCREENS_GS_ADVANCED_CHANGED: u32 = 0x0511;
/// The port's quality combo box's selection.
pub const EE_SCREENS_GS_PORT_QUALITY: u32 = 0x0512;
/// Restore Default's question answered yes (the question's callback, no event object).
pub const EE_SCREENS_GS_RESTORE_YES: u32 = 0x0513;

// ---- the host's ops (ee_screenscpp_call; menus.rs has the shared ones)

pub const EE_SCREENS_OP_GROUP: u32 = 0x120; // (group, enable, form + 1 or 0: the event's form)
pub const EE_SCREENS_OP_BACKGROUND: u32 = 0x121; // (id, visible, form + 1 or 0)
pub const EE_SCREENS_OP_CHECKBOX: u32 = 0x122; // (control) -> UICheckBox::GetIsChecked
pub const EE_SCREENS_OP_SET_CHECKBOX: u32 = 0x123; // (control, checked) UICheckBox::SetChecked
pub const EE_SCREENS_OP_RADIO: u32 = 0x124; // (control) -> UIRadioButton::GetIsChecked
pub const EE_SCREENS_OP_CHECK_RADIO: u32 = 0x125; // (control) UIRadioButton::Check
pub const EE_SCREENS_OP_RANGE: u32 = 0x126; // (control) -> UISlider::GetCurrentRangeValue
pub const EE_SCREENS_OP_SET_RANGE: u32 = 0x127; // (control, value)
pub const EE_SCREENS_OP_ENTITY_RANGE: u32 = 0x128; // -> the event's entity's range value (a slider)
pub const EE_SCREENS_OP_COMBO_SELECT: u32 = 0x129; // (control, line) UIComboBox::SetCurrentSelection
pub const EE_SCREENS_OP_LIST_SELECT: u32 = 0x12a; // (control, line) ChangeSelectionIndex(line, false, true)
pub const EE_SCREENS_OP_COMBO_CLEAR: u32 = 0x12b; // (control) DeleteAllEntries
pub const EE_SCREENS_OP_LIST_CLEAR: u32 = 0x12c; // (control)
pub const EE_SCREENS_OP_RELEASE_MOUSE: u32 = 0x12d; // (control) ReleaseCapturedMouse
pub const EE_SCREENS_OP_LABEL_NUMBER: u32 = 0x12e; // (control, value) UITextLabel::SetText(long)
pub const EE_SCREENS_OP_ENTITY_ID: u32 = 0x12f; // -> the entity's GetId, u32::MAX for none
pub const EE_SCREENS_OP_CONTROL_ID: u32 = 0x130; // (control) -> GetId
pub const EE_SCREENS_OP_TRIGGER_HERE: u32 = 0x131; // (control, trigger, event) on the event's form
pub const EE_SCREENS_OP_SLIDER_LABEL: u32 = 0x132; // (slider, label, off string or 0) a UIESliderChange
pub const EE_SCREENS_OP_CLOSE_FORM: u32 = 0x133; // gUserInterface->CloseForm(NULL)
pub const EE_SCREENS_OP_DISPLAY_CHANGED: u32 = 0x134; // gUserInterface->PostDisplayChanged(false)
pub const EE_SCREENS_OP_RESIZE_MENU: u32 = 0x135; // ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false)
pub const EE_SCREENS_OP_SRAND_TIME: u32 = 0x136; // srand(TMGetTime())
pub const EE_SCREENS_OP_RAND: u32 = 0x137; // -> rand()
pub const EE_SCREENS_GAME_CONDUCTOR_VOLUME: u32 = 0x140; // (bits) -> SetVolume's result
pub const EE_SCREENS_GAME_GET_CONDUCTOR_VOLUME: u32 = 0x141; // -> bits
pub const EE_SCREENS_GAME_REVERB: u32 = 0x142; // (on) SetReverb
pub const EE_SCREENS_GAME_CONDUCTOR_STATE: u32 = 0x143; // (state) SetConductorState
pub const EE_SCREENS_GAME_GET_CONDUCTOR_STATE: u32 = 0x144; // -> GetConductorState
pub const EE_SCREENS_GAME_UI_MUSIC_VOLUME: u32 = 0x145; // (bits) gUIManager->SetMusicVolume
pub const EE_SCREENS_GAME_MASTER_VOLUME: u32 = 0x146; // (bits) -> SetMasterVolume's result
pub const EE_SCREENS_GAME_GET_MASTER_VOLUME: u32 = 0x147; // -> bits
pub const EE_SCREENS_GAME_GET_CHANNELS: u32 = 0x148; // -> GetMaxChannels
pub const EE_SCREENS_GAME_CHANNELS: u32 = 0x149; // (count) SetMaxChannels
pub const EE_SCREENS_GAME_MUSIC_START: u32 = 0x14a; // gMusicManager->Start()
pub const EE_SCREENS_GAME_MUSIC_STOP: u32 = 0x14b;
pub const EE_SCREENS_GAME_MOUSE_SENSITIVITY: u32 = 0x14c; // (bits) gMouse->SetSensitivity
pub const EE_SCREENS_GAME_LEFT_HANDED: u32 = 0x14d; // (left) UIManager::SetMouseLeftHanded
pub const EE_SCREENS_GAME_IS_SERVER: u32 = 0x14e; // -> gCommunications->IsServer()
pub const EE_SCREENS_GAME_PAUSE: u32 = 0x14f; // gWorld.PauseGame()
pub const EE_SCREENS_GAME_CURRENT_RASTERIZER: u32 = 0x150; // -> gRasterizer's index in GERasterizer's list, else 0
pub const EE_SCREENS_GAME_CHANGE_RASTERIZER: u32 = 0x151; // (rasterizer, screen) PostChangeRasterizer
pub const EE_SCREENS_GAME_APPLY_PORT: u32 = 0x152; // EEApplyPortSettings(the settings)
pub const EE_SCREENS_GAME_VIEW: u32 = 0x153; // (left, top, right, bottom bits) EESetGameViewRectangle
pub const EE_SCREENS_GAME_MIP_FILTER: u32 = 0x154; // (mode) gTextureManager->UpdateMIPFilterMode
pub const EE_SCREENS_GAME_TEXTURE_RESOLUTION: u32 = 0x155; // (value) EESetTextureResolution
pub const EE_SCREENS_GAME_AMBIENT: u32 = 0x156; // (on) gAmbientSoundManager.SetEnabled
pub const EE_SCREENS_GAME_SAVE: u32 = 0x157; // -> gGamePreferences.Save()
pub const EE_SCREENS_GAME_NOT_KOREAN: u32 = 0x158; // -> gGamePreferences.IsNotKorean()
pub const EE_SCREENS_GAME_TEST_SOUND: u32 = 0x159; // (sound) EESoundObject::Get: LoadMedia, Play
pub const EE_SCREENS_GAME_WINDOW_MODE: u32 = 0x15a; // the settings' window mode on the game window
pub const EE_SCREENS_GAME_RASTERIZER_COUNT: u32 = 0x15c; // -> gDisplayRasterizers' size
pub const EE_SCREENS_GAME_SCREEN_COUNT: u32 = 0x15d; // (rasterizer) -> its screens (0 past the list)
/// Texts (ee_screenscpp_text / _put_text).
pub const EE_SCREENS_TEXT_MUSIC_FILE: u32 = 0x20; // the event's form's music file name (put: sets it)
pub const EE_SCREENS_TEXT_RASTERIZER_SETTING: u32 = 0x21; // (1: the defaults') "Rasterizer Name" (put: the settings')
pub const EE_SCREENS_TEXT_AUDIO_DEVICE: u32 = 0x22; // (1: the defaults') "Audio Device" (put: the settings', 127 bytes)
pub const EE_SCREENS_TEXT_DISPLAY_RASTERIZER: u32 = 0x23; // (index) gDisplayRasterizers' name
pub const EE_SCREENS_TEXT_DISPLAY_SCREEN: u32 = 0x24; // (rasterizer << 16 | screen) a screen's name
pub const EE_SCREENS_TEXT_RASTERIZER_NAME: u32 = 0x25; // (index) GERasterizer::GetRasterizer(index)->GetName()
pub const EE_SCREENS_TEXT_LIST_DATA: u32 = 0x26; // (control << 16 | line) a list box's line
pub const EE_SCREENS_TEXT_COMBO_DATA: u32 = 0x27; // (control << 16 | line) a combo box's list line
pub const EE_SCREENS_TEXT_ADAPTERS: u32 = 0x28; // the network adapters, each ended by a 0 byte
pub const EE_SCREENS_TEXT_OUTPUT_DEVICES: u32 = 0x29; // the output devices, each ended by a 0 byte
pub const EE_SCREENS_TEXT_COMBO_APPEND: u32 = 0x2a; // put (control) AppendLine
pub const EE_SCREENS_TEXT_COMBO_INSERT: u32 = 0x2b; // put (control << 16 | line) InsertLine
pub const EE_SCREENS_TEXT_LIST_APPEND: u32 = 0x2c; // put (control) a list box's AppendLine
pub const EE_SCREENS_TEXT_QUESTION: u32 = 0x2d; // put: DisplayQuestion(text, true, form, Restore Default's yes, NULL)

// the form's controls, groups, triggers and strings (ee/EEUIManager.h, lle/UIControl.h, ee/EEStringIDs.h)
const GS_FORM_GAME_SETTINGS: u32 = 5;
const GS_VIDEO_CARD_LABEL: u32 = 5;
const GS_VIDEO_CARD_BOX: u32 = 6;
const GS_RENDERING_LABEL: u32 = 7;
const GS_RENDERING_LIST: u32 = 8;
const GS_RESOLUTION_LABEL: u32 = 9;
const GS_RESOLUTION_LIST: u32 = 10;
const GS_SHOW_BLOOD: u32 = 13;
const GS_OK_BUTTON: u32 = 18;
const GS_MUSIC_VOLUME: u32 = 25;
const GS_REVERB_ON: u32 = 27;
const GS_REVERB_OFF: u32 = 28;
const GS_MUSIC_OFF: u32 = 29;
const GS_SOUND_VOLUME: u32 = 35;
const GS_VOICES: u32 = 39;
const GS_SIDE_SCROLL: u32 = 49;
const GS_RIGHT_HANDED: u32 = 53;
const GS_LEFT_HANDED: u32 = 54;
const GS_MOUSE_SENSITIVITY: u32 = 56;
const GS_GRAB_SCROLL: u32 = 62;
const GS_AMBIENT_SOUNDS: u32 = 66;
const GS_QUALITY_RADIOS: [u32; 6] = [67, 68, 69, 70, 71, 72];
const GS_CUSTOM_RADIO: u32 = 72;
const GS_AUTOSAVE_MINUTES: u32 = 91;
const GS_AUTOSAVE_MINUTES_LABEL: u32 = 92;
const GS_AUTOSAVE_COUNT: u32 = 94;
const GS_AUTOSAVE_COUNT_LABEL: u32 = 95;
const GS_CANT_CHANGE_WARNING: u32 = 106;
const GS_ADAPTER_BOX: u32 = 109;
const GS_TEXTURE_RESOLUTION_BOX: u32 = 111;
const GS_WINDOW_MODE_BOX: u32 = 112;
const GS_UI_SCALE_BOX: u32 = 113;
const GS_RENDER_RESOLUTION_BOX: u32 = 114;
const GS_FRAME_RATE_BOX: u32 = 115;
const GS_TEXTURE_FILTERING_BOX: u32 = 116;
const GS_BRIGHTNESS_BOX: u32 = 117;
const GS_ZOOM_OUT_BOX: u32 = 118;
const GS_MASTER_VOLUME: u32 = 120;
const GS_OUTPUT_DEVICE_BOX: u32 = 121;
const GS_MUTE_IN_BACKGROUND: u32 = 122;
const GS_QUALITY_BOX: u32 = 123;
const GS_VIDEO_BACKGROUND: u32 = 0x8c;
const GS_TRIGGER_SELECTION_CHANGED: u32 = 0x3eb;
const GS_TRIGGER_BUTTON_UP: u32 = 0x3f0;
const GS_HIDDEN: u32 = 0;
const GS_SHOWN: u32 = 1;
const GS_ERROR_OK: c_long = 0;
const GS_ERROR_NOT_SUPPORTED: c_long = 0xd;
const GS_MIP_FILTER_POINT: u32 = 1;
const GS_MIP_FILTER_LINEAR: u32 = 2;
const GS_IDS_ARE_YOU_SURE_YOU_WANT_TO_RESTORE: u32 = 8534;
const GS_IDS_LOBBY_OFF: u32 = 30280;
const GS_IDS_CIV_POWER_DEFAULT: u32 = 32813;
/// The test sound's four samples (`EEUIEGSETestSoundVolume`).
const GS_TEST_SOUNDS: [u32; 4] = [0x34, 0x47, 0xa0, 0x17a];

/// The settings the form reads and writes (`EEGameSettings`' fields; the texts are the host's).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensSettingsRecord {
    pub mouse_sensitivity: f32,
    pub side_scroll_speed: f32,
    pub grab_scroll_speed: f32,
    pub sound_volume: f32,
    pub music_volume: f32,
    pub max_number_of_sounds: f32,
    pub game_bit_depth: c_long,
    pub game_window_height: c_ulong,
    pub game_window_width: c_ulong,
    pub mip_levels_to_drop: c_long,
    pub model_detail: c_long,
    pub effect_detail: c_long,
    pub network_adapter: c_long,
    pub screen_id: c_long,
    pub terrain_mip_levels_to_drop: c_long,
    pub texture_bit_depth: c_long,
    pub number_of_auto_saves: c_ulong,
    pub auto_save_in_milliseconds: c_ulong,
    pub texture_resolution: c_long,
    pub ui_scale: c_long,
    pub render_resolution: c_long,
    pub window_mode: c_long,
    pub frame_rate_limit: c_long,
    pub texture_filtering: c_long,
    pub brightness: c_long,
    pub master_volume: c_long,
    pub maximum_zoom_out: c_long,
    pub zoom_enabled: bool,
    pub display_clouds: bool,
    pub display_shadows: bool,
    pub display_battlefield_ambients: bool,
    pub trilinear_filter: bool,
    pub full_screen: bool,
    pub reverb_enabled: bool,
    pub wait_for_vsync: bool,
    pub ambient_sounds: bool,
    pub show_blood: bool,
    pub music_enabled: bool,
    pub right_hand_mouse_orientation: bool,
    pub unit_lighting: bool,
    pub display_weather_effects: bool,
    pub anti_aliasing: bool,
    pub mute_in_background: bool,
}

/// The toolkit's and the engine's calls the settings form makes, each one call (controls are the event's form's,
/// by index; texts are UTF-8 bytes; a missing control answers 0 and takes nothing).
pub trait SettingsHost {
    fn group(&mut self, form: Option<u32>, group: u32, enable: bool);
    fn background(&mut self, form: Option<u32>, id: u32, visible: bool);
    fn checkbox(&mut self, control: u32) -> bool;
    fn set_checkbox(&mut self, control: u32, checked: bool);
    fn radio(&mut self, control: u32) -> bool;
    fn check_radio(&mut self, control: u32);
    fn range(&mut self, control: u32) -> c_long;
    fn set_range(&mut self, control: u32, value: c_long);
    fn entity_range(&mut self) -> c_long;
    /// `GetCurrentSelection`: its result and the selection it wrote (a combo box's, a list box's).
    fn combo_selection(&mut self, control: u32) -> (c_long, c_ulong);
    fn list_selection(&mut self, control: u32) -> (c_long, c_ulong);
    fn combo_select(&mut self, control: u32, line: c_ulong);
    fn list_select(&mut self, control: u32, line: c_ulong);
    fn combo_clear(&mut self, control: u32);
    fn list_clear(&mut self, control: u32);
    fn combo_append(&mut self, control: u32, text: &[u8]);
    fn combo_insert(&mut self, control: u32, line: u32, text: &[u8]);
    fn list_append(&mut self, control: u32, text: &[u8]);
    fn combo_data(&mut self, control: u32, line: c_ulong) -> Vec<u8>;
    fn list_data(&mut self, control: u32, line: c_ulong) -> Vec<u8>;
    fn release_mouse(&mut self, control: u32);
    fn show(&mut self, control: u32, mode: u32);
    fn label_number(&mut self, control: u32, value: c_long);
    /// The event's entity's id, none without one; a control's id.
    fn entity_id(&mut self) -> Option<c_ulong>;
    fn control_id(&mut self, control: u32) -> c_ulong;
    /// The event's form's control `control`: `SetTriggerEvent(trigger, event)`.
    fn trigger(&mut self, control: u32, trigger: u32, event: u32);
    /// A `UIESliderChange` on label `label` (modifier 1, offset 0) as slider `slider`'s range event, with resource
    /// string `off` at 0 when there is one.
    fn slider_label(&mut self, slider: u32, label: u32, off: Option<u32>);
    fn music_file(&mut self) -> Vec<u8>;
    fn set_music_file(&mut self, text: &[u8]);
    fn string(&mut self, id: u32) -> Vec<u8>;
    /// Restore Default's question (yes: [`EE_SCREENS_GS_RESTORE_YES`]).
    fn question(&mut self, text: &[u8]);
    fn pop(&mut self);
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` without a value (close form, display changed, resize, srand, music
    /// start and stop, pause, apply port, window mode).
    fn call(&mut self, op: u32);
    /// `EE_SCREENS_GAME_*` / `EE_SCREENS_OP_*` with arguments and an answer (floats as bits).
    fn call_with(&mut self, op: u32, a: u32, b: u32) -> u32;
    fn settings(&mut self, defaults: bool) -> EeScreensSettingsRecord;
    fn put_settings(&mut self, settings: &EeScreensSettingsRecord);
    /// `EE_SCREENS_TEXT_*` (`arg` per text).
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8>;
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]);
    /// `FillModes`' modes (unsorted): those of the first screen of index `screen` in the first rasterizer of index
    /// `rasterizer` that has it.
    fn modes(&mut self, rasterizer: c_ulong, screen: c_ulong) -> Vec<EeScreensVideoMode>;
    /// The current mode of `gRasterizer`'s screen `screen`.
    fn current_mode(&mut self, screen: c_ulong) -> EeScreensVideoMode;
    /// `GXGameSize(percent, windowed)`: the game's width and height.
    fn game_size(&mut self, percent: c_ulong, windowed: bool) -> (c_ulong, c_ulong);
}

/// The form's state (the original's file statics).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SettingsForm {
    /// The form's music while it is open (`sMusicFileName`).
    pub music_file: Vec<u8>,
    /// Opened from the game (`sInGame`).
    pub in_game: bool,
    /// The output device combo's lines after "System Default", listed once (`sOutputDevices`).
    pub output_devices: Vec<Vec<u8>>,
    /// The last OK changed the window mode, the UI scale or the render resolution (`sPortDisplayChanged`).
    pub display_changed: bool,
    /// The quality combo box and the detail controls set each other (`sPortQualityGuard`).
    pub quality_guard: bool,
    /// The rendering system and video card last seen by their events (`sLastRasterizer`, `sLastScreen`).
    pub last_rasterizer: Vec<u8>,
    pub last_screen: Vec<u8>,
}

impl SettingsForm {
    pub const fn new() -> SettingsForm {
        SettingsForm {
            music_file: Vec::new(),
            in_game: false,
            output_devices: Vec::new(),
            display_changed: false,
            quality_guard: false,
            last_rasterizer: Vec::new(),
            last_screen: Vec::new(),
        }
    }
}

/// The form's event for a control event id (`dbuicontrolevents`), 0 for another form's (`EECreateGameSettingsEvent`:
/// ids 45..0xfb; the OK button's is the open events').
pub fn event_for_control(id: c_ulong) -> u32 {
    match id {
        46 => EE_SCREENS_GS_CANCEL,
        47 => EE_SCREENS_GS_RESTORE_DEFAULT,
        48 => EE_SCREENS_GS_VIDEO_TAB,
        49 => EE_SCREENS_GS_AUDIO_TAB,
        50 => EE_SCREENS_GS_INTERFACE_TAB,
        72 => EE_SCREENS_GS_VIDEO_CARD,
        73 => EE_SCREENS_GS_RENDERING_SYSTEM,
        74 => EE_SCREENS_GS_MUSIC_VOLUME,
        75 => EE_SCREENS_GS_SOUND_VOLUME,
        76..=78 => EE_SCREENS_GS_MUSIC_QUALITY,
        84 => EE_SCREENS_GS_TEST_SOUND,
        214..=218 => EE_SCREENS_GS_GRAPHICS_QUALITY,
        220..=239 | 250 | 251 => EE_SCREENS_GS_ADVANCED_CHANGED,
        _ => 0,
    }
}

/// Whether `event` is one of the form's.
pub fn is_settings_event(event: u32) -> bool {
    (EE_SCREENS_GS_OUT_OF_GAME_OPEN..=EE_SCREENS_GS_RESTORE_YES).contains(&event)
}

fn lock(state: &Mutex<SettingsForm>) -> std::sync::MutexGuard<'_, SettingsForm> {
    state.lock().unwrap_or_else(|e| e.into_inner())
}

fn bits(v: f32) -> u32 {
    v.to_bits()
}

/// The video modes `FillModes` collects, sorted largest first (`EEVideoModeGreater`; modes equal by it are equal in
/// everything the form reads).
fn fill_modes<H: SettingsHost>(host: &mut H, rasterizer: c_ulong, screen: c_ulong) -> Vec<EeScreensVideoMode> {
    let mut modes = host.modes(rasterizer, screen);
    modes.sort_by(|a, b| {
        if video_mode_greater(a, b) {
            core::cmp::Ordering::Less
        } else if video_mode_greater(b, a) {
            core::cmp::Ordering::Greater
        } else {
            core::cmp::Ordering::Equal
        }
    });
    modes
}

/// The settings' size as a mode (`EEScreensSettingsMode`).
fn settings_mode(s: &EeScreensSettingsRecord) -> EeScreensVideoMode {
    EeScreensVideoMode {
        width: s.game_window_width,
        height: s.game_window_height,
        refresh_rate: 0,
        bit_count: s.game_bit_depth as c_ulong,
    }
}

/// A list of texts each ended by a 0 byte.
fn split_list(list: &[u8]) -> Vec<Vec<u8>> {
    let mut out = Vec::new();
    let mut start = 0;
    for (i, &b) in list.iter().enumerate() {
        if b == 0 {
            out.push(list[start..i].to_vec());
            start = i + 1;
        }
    }
    out
}

/// The texts of a C string field (up to its first 0).
fn c_text(text: &[u8]) -> &[u8] {
    &text[..text.iter().position(|&b| b == 0).unwrap_or(text.len())]
}

fn music_off<H: SettingsHost>(host: &mut H) {
    host.call_with(EE_SCREENS_GAME_CONDUCTOR_STATE, 0, 0);
    host.call_with(EE_SCREENS_GAME_UI_MUSIC_VOLUME, bits(0.0), 0);
    host.call(EE_SCREENS_GAME_MUSIC_STOP);
}

fn music_on<H: SettingsHost>(host: &mut H, state: &Mutex<SettingsForm>) {
    host.call_with(EE_SCREENS_GAME_CONDUCTOR_STATE, 1, 0);
    if lock(state).in_game {
        host.call(EE_SCREENS_GAME_MUSIC_START);
    }
    let volume = host.call_with(EE_SCREENS_GAME_GET_CONDUCTOR_VOLUME, 0, 0);
    host.call_with(EE_SCREENS_GAME_UI_MUSIC_VOLUME, volume, 0);
}

/// The detail controls as `GetGraphicsQuality` reads them.
fn read_graphics<H: SettingsHost>(host: &mut H) -> EeScreensGraphics {
    let mut g = EeScreensGraphics::default();
    for (i, &c) in CHECK_CONTROLS.iter().enumerate() {
        g.checks[i] = host.checkbox(c as u32);
    }
    for i in 0..3 {
        g.model[i] = host.radio(MODEL_RADIOS[i] as u32);
        g.effect[i] = host.radio(EFFECT_RADIOS[i] as u32);
    }
    g
}

fn graphics_quality_of<H: SettingsHost>(host: &mut H) -> c_ulong {
    graphics_quality(&read_graphics(host))
}

/// Checks a detail radio of a group (high, medium, low) by its level: 0 low, 1 medium, 2 high.
fn check_detail<H: SettingsHost>(host: &mut H, radios: &[c_ulong; 3], level: c_long) {
    if (0..=2).contains(&level) {
        host.check_radio(radios[(2 - level) as usize] as u32);
    }
}

fn set_graphics_quality<H: SettingsHost>(host: &mut H, quality: c_ulong) {
    let Some(preset) = quality_preset(quality) else { return };
    for (i, &c) in CHECK_CONTROLS.iter().enumerate() {
        host.set_checkbox(c as u32, preset.checks[i]);
    }
    check_detail(host, &MODEL_RADIOS, preset.model);
    check_detail(host, &EFFECT_RADIOS, preset.effect);
}

/// The quality combo box shows the preset the detail controls match (5: Custom); only a different selection is
/// set (the controls' events are queued: the two would set each other forever).
fn port_show_quality<H: SettingsHost>(host: &mut H, state: &Mutex<SettingsForm>) {
    if lock(state).quality_guard {
        return;
    }
    let quality = graphics_quality_of(host);
    let (result, selection) = host.combo_selection(GS_QUALITY_BOX);
    if result == GS_ERROR_OK && selection == quality {
        return;
    }
    lock(state).quality_guard = true;
    host.combo_select(GS_QUALITY_BOX, quality);
    lock(state).quality_guard = false;
}

fn port_select<H: SettingsHost>(host: &mut H, control: u32, table_kind: u32, value: c_long) {
    let line = table(table_kind).map_or(0, |t| nearest_line(t, value));
    host.combo_select(control, line);
}

fn port_read<H: SettingsHost>(host: &mut H, control: u32, table_kind: u32, value: &mut c_long) {
    let (result, selection) = host.combo_selection(control);
    if result == GS_ERROR_OK {
        if let Some(v) = table(table_kind).and_then(|t| line_value(t, selection)) {
            *value = v;
        }
    }
}

fn port_show_video<H: SettingsHost>(host: &mut H, state: &Mutex<SettingsForm>, s: &EeScreensSettingsRecord) {
    // the controls the port's replace, the quality radios, slider and labels (the quality combo box drives them)
    for control in [
        GS_VIDEO_CARD_LABEL,
        GS_VIDEO_CARD_BOX,
        GS_RENDERING_LABEL,
        GS_RENDERING_LIST,
        GS_RESOLUTION_LABEL,
        GS_RESOLUTION_LIST,
        GS_CANT_CHANGE_WARNING,
        82,
        83,
        84,
        85,
    ] {
        host.show(control, GS_HIDDEN);
    }
    for control in (14..=22).filter(|&i| i != 18 && i != 19) {
        host.show(control, GS_HIDDEN);
    }
    for control in GS_QUALITY_RADIOS[0]..=GS_CUSTOM_RADIO + 1 {
        host.show(control, GS_HIDDEN);
    }
    port_show_quality(host, state);
    port_select(host, GS_WINDOW_MODE_BOX, EE_SCREENS_TABLE_WINDOW_MODE, s.window_mode);
    port_select(host, GS_UI_SCALE_BOX, EE_SCREENS_TABLE_UI_SCALE, s.ui_scale);
    port_select(host, GS_RENDER_RESOLUTION_BOX, EE_SCREENS_TABLE_RENDER_RESOLUTION, s.render_resolution);
    port_select(host, GS_FRAME_RATE_BOX, EE_SCREENS_TABLE_FRAME_RATE, s.frame_rate_limit);
    port_select(host, GS_TEXTURE_FILTERING_BOX, EE_SCREENS_TABLE_TEXTURE_FILTERING, s.texture_filtering);
    port_select(host, GS_BRIGHTNESS_BOX, EE_SCREENS_TABLE_BRIGHTNESS, s.brightness);
}

fn port_show_audio<H: SettingsHost>(
    host: &mut H,
    state: &Mutex<SettingsForm>,
    s: &EeScreensSettingsRecord,
    defaults: bool,
) {
    host.set_range(GS_MASTER_VOLUME, s.master_volume);
    host.set_checkbox(GS_MUTE_IN_BACKGROUND, s.mute_in_background);
    if lock(state).output_devices.is_empty() {
        // once a run: the devices there are, after "System Default"
        let devices = split_list(&host.text(EE_SCREENS_TEXT_OUTPUT_DEVICES, 0));
        for (i, name) in devices.iter().enumerate() {
            lock(state).output_devices.push(name.clone());
            host.combo_insert(GS_OUTPUT_DEVICE_BOX, i as u32 + 1, name);
        }
    }
    let device = host.text(EE_SCREENS_TEXT_AUDIO_DEVICE, defaults as u32);
    let device = c_text(&device);
    let selection = {
        let s = lock(state);
        s.output_devices.iter().rposition(|d| d.as_slice() == device).map_or(0, |i| i as c_ulong + 1)
    };
    host.combo_select(GS_OUTPUT_DEVICE_BOX, selection);
}

fn port_show_interface<H: SettingsHost>(host: &mut H, s: &EeScreensSettingsRecord) {
    // the network adapter section (its labels and box): not a choice any more
    for control in 107..=GS_ADAPTER_BOX {
        host.show(control, GS_HIDDEN);
    }
    port_select(host, GS_ZOOM_OUT_BOX, EE_SCREENS_TABLE_ZOOM_OUT, s.maximum_zoom_out);
}

/// Reads the port's controls into the settings and puts them into effect; whether the window mode, the UI scale or
/// the render resolution changed.
fn port_apply<H: SettingsHost>(host: &mut H, state: &Mutex<SettingsForm>, s: &mut EeScreensSettingsRecord) -> bool {
    let (window_mode, ui_scale, render_resolution) = (s.window_mode, s.ui_scale, s.render_resolution);
    port_read(host, GS_WINDOW_MODE_BOX, EE_SCREENS_TABLE_WINDOW_MODE, &mut s.window_mode);
    port_read(host, GS_UI_SCALE_BOX, EE_SCREENS_TABLE_UI_SCALE, &mut s.ui_scale);
    port_read(host, GS_RENDER_RESOLUTION_BOX, EE_SCREENS_TABLE_RENDER_RESOLUTION, &mut s.render_resolution);
    port_read(host, GS_FRAME_RATE_BOX, EE_SCREENS_TABLE_FRAME_RATE, &mut s.frame_rate_limit);
    port_read(host, GS_TEXTURE_FILTERING_BOX, EE_SCREENS_TABLE_TEXTURE_FILTERING, &mut s.texture_filtering);
    port_read(host, GS_BRIGHTNESS_BOX, EE_SCREENS_TABLE_BRIGHTNESS, &mut s.brightness);
    port_read(host, GS_ZOOM_OUT_BOX, EE_SCREENS_TABLE_ZOOM_OUT, &mut s.maximum_zoom_out);
    s.master_volume = host.range(GS_MASTER_VOLUME);
    s.mute_in_background = host.checkbox(GS_MUTE_IN_BACKGROUND);
    let (result, device) = host.combo_selection(GS_OUTPUT_DEVICE_BOX);
    if result == GS_ERROR_OK {
        let name = {
            let st = lock(state);
            let n = st.output_devices.len() as c_ulong;
            if device > 0 && device <= n {
                st.output_devices[device as usize - 1].clone()
            } else {
                Vec::new()
            }
        };
        host.put_text(EE_SCREENS_TEXT_AUDIO_DEVICE, 0, &name);
    }
    s.full_screen = s.window_mode == 0;
    s.wait_for_vsync = s.frame_rate_limit == 0;
    // the game's size follows the UI scale (32 bits: the bit depth is no setting any more)
    (s.game_window_width, s.game_window_height) = host.game_size(s.ui_scale as c_ulong, s.window_mode != 0);
    s.game_bit_depth = 32;
    s.texture_bit_depth = 32;
    host.put_settings(s);
    host.call(EE_SCREENS_GAME_APPLY_PORT);
    window_mode != s.window_mode || ui_scale != s.ui_scale || render_resolution != s.render_resolution
}

/// `ApplySettings`: the form's choices into the settings and the engine, then the save; its result and the mode
/// index (the last mode of the settings' size).
fn apply_settings<H: SettingsHost>(host: &mut H, state: &Mutex<SettingsForm>) -> (c_long, c_ulong) {
    let mut s = host.settings(false);
    s.anti_aliasing = false;
    let (result, mut rasterizer) = host.list_selection(GS_RENDERING_LIST);
    if result == GS_ERROR_NOT_SUPPORTED {
        rasterizer = 0;
    }
    let (result, mut screen) = host.combo_selection(GS_VIDEO_CARD_BOX);
    if result != GS_ERROR_OK {
        screen = 0;
    }
    let (_, adapter) = host.combo_selection(GS_ADAPTER_BOX);
    s.network_adapter = adapter as c_long;
    let name = host.text(EE_SCREENS_TEXT_RASTERIZER_NAME, rasterizer);
    if host.text(EE_SCREENS_TEXT_RASTERIZER_SETTING, 0) != name || s.screen_id as c_ulong != screen {
        host.call_with(EE_SCREENS_GAME_CHANGE_RASTERIZER, rasterizer, screen);
        host.put_text(EE_SCREENS_TEXT_RASTERIZER_SETTING, 0, &name);
        s.screen_id = screen as c_long;
    }
    // port: the game's size is the UI scale's, not the resolution list's (hidden); the mode index is the matching
    // mode's (the in-game OK compares it with the current mode)
    let changed = port_apply(host, state, &mut s);
    lock(state).display_changed = changed;
    let modes = fill_modes(host, rasterizer, screen);
    let mode = mode_index(&modes, &settings_mode(&s));
    host.call_with(EE_SCREENS_GAME_VIEW, 0, 0);
    let c = EeScreensApplyControls {
        show_blood: host.checkbox(GS_SHOW_BLOOD),
        trilinear: host.checkbox(82),
        unit_lighting: host.checkbox(83),
        full_textures: host.checkbox(84),
        texture_selected: false,
        texture_line: 0,
        shadows: host.checkbox(86),
        clouds: host.checkbox(87),
        battlefield_ambients: host.checkbox(110),
        weather_effects: host.checkbox(88),
        model: [host.radio(75), host.radio(76), host.radio(77)],
        effect: [host.radio(79), host.radio(80), host.radio(81)],
        ambient_sounds: host.checkbox(GS_AMBIENT_SOUNDS),
        voices: host.range(GS_VOICES),
        max_channels: host.call_with(EE_SCREENS_GAME_GET_CHANNELS, 0, 0) as c_ulong,
        music_off: host.radio(GS_MUSIC_OFF),
        reverb_on: host.radio(GS_REVERB_ON),
        reverb_off: host.radio(GS_REVERB_OFF),
        mouse_sensitivity: host.range(GS_MOUSE_SENSITIVITY),
        side_scroll: host.range(GS_SIDE_SCROLL),
        grab_scroll: host.range(GS_GRAB_SCROLL),
        right_handed: host.radio(GS_RIGHT_HANDED),
        autosave_minutes: host.range(GS_AUTOSAVE_MINUTES),
        autosave_count: host.range(GS_AUTOSAVE_COUNT),
    };
    let (texture_result, texture_line) = host.combo_selection(GS_TEXTURE_RESOLUTION_BOX);
    let c = EeScreensApplyControls { texture_selected: texture_result == GS_ERROR_OK, texture_line, ..c };
    let mut a = EeScreensApplied {
        texture_resolution: s.texture_resolution,
        model_detail: s.model_detail,
        effect_detail: s.effect_detail,
        reverb: s.reverb_enabled,
        ..Default::default()
    };
    apply(&c, &mut a);
    s.show_blood = a.show_blood;
    s.zoom_enabled = true;
    s.trilinear_filter = a.trilinear;
    host.call_with(EE_SCREENS_GAME_MIP_FILTER, if a.trilinear { GS_MIP_FILTER_LINEAR } else { GS_MIP_FILTER_POINT }, 0);
    s.unit_lighting = a.unit_lighting;
    s.mip_levels_to_drop = a.mip_levels_to_drop;
    s.terrain_mip_levels_to_drop = a.terrain_mip_levels_to_drop;
    if a.set_texture_resolution {
        s.texture_resolution = a.texture_resolution;
        host.call_with(EE_SCREENS_GAME_TEXTURE_RESOLUTION, a.texture_resolution as u32, 0);
    }
    s.display_shadows = a.shadows;
    s.display_clouds = a.clouds;
    s.display_battlefield_ambients = a.battlefield_ambients;
    s.display_weather_effects = a.weather_effects;
    s.model_detail = a.model_detail;
    s.effect_detail = a.effect_detail;
    s.ambient_sounds = a.ambient_sounds;
    host.call_with(EE_SCREENS_GAME_AMBIENT, a.ambient_sounds as u32, 0);
    s.sound_volume = f32::from_bits(host.call_with(EE_SCREENS_GAME_GET_MASTER_VOLUME, 0, 0));
    s.max_number_of_sounds = a.max_number_of_sounds;
    host.call_with(EE_SCREENS_GAME_CHANNELS, a.channels as u32, 0);
    s.music_enabled = a.music_enabled;
    s.reverb_enabled = a.reverb;
    s.music_volume = f32::from_bits(host.call_with(EE_SCREENS_GAME_GET_CONDUCTOR_VOLUME, 0, 0));
    host.call_with(EE_SCREENS_GAME_MOUSE_SENSITIVITY, bits(a.mouse_sensitivity), 0);
    s.mouse_sensitivity = a.mouse_sensitivity;
    s.side_scroll_speed = a.side_scroll;
    s.grab_scroll_speed = a.grab_scroll;
    host.call_with(EE_SCREENS_GAME_LEFT_HANDED, !a.right_handed as u32, 0);
    s.right_hand_mouse_orientation = a.right_handed;
    s.auto_save_in_milliseconds = a.autosave_ms;
    s.number_of_auto_saves = a.autosaves;
    host.put_settings(&s);
    (host.call_with(EE_SCREENS_GAME_SAVE, 0, 0) as c_long, mode)
}

fn show_rasterizers<H: SettingsHost>(host: &mut H, defaults: bool) {
    host.list_clear(GS_RENDERING_LIST);
    let name = host.text(EE_SCREENS_TEXT_RASTERIZER_SETTING, defaults as u32);
    let count = host.call_with(EE_SCREENS_GAME_RASTERIZER_COUNT, 0, 0);
    for i in 0..count {
        let line = host.text(EE_SCREENS_TEXT_DISPLAY_RASTERIZER, i);
        host.list_append(GS_RENDERING_LIST, &line);
        if line == name {
            host.list_select(GS_RENDERING_LIST, i as c_ulong);
        }
    }
}

fn show_screens<H: SettingsHost>(host: &mut H, s: &EeScreensSettingsRecord) {
    host.release_mouse(GS_VIDEO_CARD_BOX);
    host.combo_clear(GS_VIDEO_CARD_BOX);
    let (_, rasterizer) = host.list_selection(GS_RENDERING_LIST);
    let count = host.call_with(EE_SCREENS_GAME_SCREEN_COUNT, rasterizer, 0);
    let line = screen_line(count as c_ulong, s.screen_id as c_ulong);
    for i in 0..count {
        let name = host.text(EE_SCREENS_TEXT_DISPLAY_SCREEN, (rasterizer << 16) | i);
        host.combo_append(GS_VIDEO_CARD_BOX, &name);
        if line == i as c_ulong {
            host.combo_select(GS_VIDEO_CARD_BOX, i as c_ulong);
        }
    }
}

fn show_resolutions<H: SettingsHost>(host: &mut H, s: &EeScreensSettingsRecord) {
    let defaults = host.settings(true);
    let (_, rasterizer) = host.list_selection(GS_RENDERING_LIST);
    let (_, screen) = host.combo_selection(GS_VIDEO_CARD_BOX);
    let modes = fill_modes(host, rasterizer, screen);
    let line = resolution_line(&modes, &settings_mode(s), &settings_mode(&defaults));
    host.list_clear(GS_RESOLUTION_LIST);
    for m in &modes {
        host.list_append(GS_RESOLUTION_LIST, format!("{} x {} x {} Bit", m.width, m.height, m.bit_count).as_bytes());
    }
    host.list_select(GS_RESOLUTION_LIST, line);
}

fn show_graphics<H: SettingsHost>(host: &mut H, s: &EeScreensSettingsRecord) {
    let plan = graphics_show(&EeScreensGraphicsSettings {
        trilinear_filter: s.trilinear_filter,
        unit_lighting: s.unit_lighting,
        shadows: s.display_shadows,
        clouds: s.display_clouds,
        battlefield_ambients: s.display_battlefield_ambients,
        weather_effects: s.display_weather_effects,
        anti_aliasing: s.anti_aliasing,
        wait_for_vsync: s.wait_for_vsync,
        mip_levels_to_drop: s.mip_levels_to_drop,
        terrain_mip_levels_to_drop: s.terrain_mip_levels_to_drop,
        texture_resolution: s.texture_resolution,
        model_detail: s.model_detail,
        effect_detail: s.effect_detail,
    });
    // the original's order: 82, 83, 84, the texture resolution, 86, 87, 110, 88, 105, the detail radios, 85 (v-sync)
    for (&control, &checked) in CHECK_CONTROLS.iter().zip(&plan.checks).take(3) {
        host.set_checkbox(control as u32, checked);
    }
    host.combo_select(GS_TEXTURE_RESOLUTION_BOX, plan.texture_line);
    for (&control, &checked) in CHECK_CONTROLS.iter().zip(&plan.checks).skip(4) {
        host.set_checkbox(control as u32, checked);
    }
    check_detail(host, &MODEL_RADIOS, plan.model);
    check_detail(host, &EFFECT_RADIOS, plan.effect);
    host.set_checkbox(CHECK_CONTROLS[3] as u32, plan.checks[3]);
    let radio = quality_radio(graphics_quality_of(host));
    host.show(GS_CUSTOM_RADIO, GS_HIDDEN);
    host.check_radio(GS_QUALITY_RADIOS[radio as usize]);
    if radio == EE_SCREENS_QUALITY_CUSTOM {
        host.show(GS_CUSTOM_RADIO, GS_SHOWN);
    }
}

fn show_video<H: SettingsHost>(host: &mut H, state: &Mutex<SettingsForm>, defaults: bool) {
    let s = host.settings(defaults);
    show_rasterizers(host, defaults);
    show_screens(host, &s);
    show_resolutions(host, &s);
    host.set_checkbox(GS_SHOW_BLOOD, s.show_blood);
    let shown = host.call_with(EE_SCREENS_GAME_NOT_KOREAN, 0, 0) != 0;
    host.show(GS_SHOW_BLOOD, shown as u32);
    show_graphics(host, &s);
    port_show_video(host, state, &s);
}

fn show_audio<H: SettingsHost>(host: &mut H, state: &Mutex<SettingsForm>, defaults: bool) {
    let s = host.settings(defaults);
    let d = host.settings(true);
    let plan = audio_show(&EeScreensAudioSettings {
        music_volume: s.music_volume,
        default_music_volume: d.music_volume,
        sound_volume: s.sound_volume,
        default_sound_volume: d.sound_volume,
        max_number_of_sounds: s.max_number_of_sounds,
        reverb: s.reverb_enabled,
        music_enabled: s.music_enabled,
    });
    if plan.set_music_volume {
        host.call_with(EE_SCREENS_GAME_CONDUCTOR_VOLUME, bits(s.music_volume), 0);
        host.call_with(EE_SCREENS_GAME_UI_MUSIC_VOLUME, bits(s.music_volume), 0);
    }
    let volume = f32::from_bits(host.call_with(EE_SCREENS_GAME_GET_CONDUCTOR_VOLUME, 0, 0));
    host.set_range(GS_MUSIC_VOLUME, slider_percent(volume));
    if plan.set_sound_volume {
        host.call_with(EE_SCREENS_GAME_MASTER_VOLUME, bits(s.sound_volume), 0);
    }
    let volume = f32::from_bits(host.call_with(EE_SCREENS_GAME_GET_MASTER_VOLUME, 0, 0));
    host.set_range(GS_SOUND_VOLUME, slider_percent(volume));
    host.set_range(GS_VOICES, plan.voices);
    host.check_radio(if plan.reverb_on { GS_REVERB_ON } else { GS_REVERB_OFF });
    if plan.music_off {
        host.check_radio(GS_MUSIC_OFF);
    }
    host.set_checkbox(GS_AMBIENT_SOUNDS, s.ambient_sounds);
    port_show_audio(host, state, &s, defaults);
}

fn show_interface<H: SettingsHost>(host: &mut H, defaults: bool) {
    let s = host.settings(defaults);
    // the adapters are listed first (a query: the original listed them after the sliders)
    let adapters = split_list(&host.text(EE_SCREENS_TEXT_ADAPTERS, 0));
    let plan = interface_show(&EeScreensInterfaceSettings {
        mouse_sensitivity: s.mouse_sensitivity,
        side_scroll: s.side_scroll_speed,
        grab_scroll: s.grab_scroll_speed,
        network_adapter: s.network_adapter,
        adapters: adapters.len() as c_ulong,
        right_handed: s.right_hand_mouse_orientation,
        autosave_ms: s.auto_save_in_milliseconds,
        autosaves: s.number_of_auto_saves,
    });
    host.set_range(GS_MOUSE_SENSITIVITY, plan.mouse_sensitivity);
    host.call_with(EE_SCREENS_GAME_MOUSE_SENSITIVITY, bits(s.mouse_sensitivity), 0);
    host.set_range(GS_SIDE_SCROLL, plan.side_scroll);
    host.set_range(GS_GRAB_SCROLL, plan.grab_scroll);
    host.combo_clear(GS_ADAPTER_BOX);
    let default = host.string(GS_IDS_CIV_POWER_DEFAULT);
    host.combo_append(GS_ADAPTER_BOX, &default);
    for a in &adapters {
        host.combo_append(GS_ADAPTER_BOX, a);
    }
    host.combo_select(GS_ADAPTER_BOX, plan.adapter_line);
    host.call_with(EE_SCREENS_GAME_LEFT_HANDED, plan.left_handed as u32, 0);
    host.check_radio(if plan.left_handed { GS_LEFT_HANDED } else { GS_RIGHT_HANDED });
    host.set_range(GS_AUTOSAVE_MINUTES, plan.autosave_minutes);
    host.set_range(GS_AUTOSAVE_COUNT, plan.autosaves);
    host.label_number(GS_AUTOSAVE_MINUTES_LABEL, plan.autosave_minutes);
    host.label_number(GS_AUTOSAVE_COUNT_LABEL, plan.autosaves);
    port_show_interface(host, &s);
}

fn tab<H: SettingsHost>(host: &mut H, form: Option<u32>, shown: u32, video: bool) {
    for group in [1, 2, 3].into_iter().filter(|&g| g != shown) {
        host.group(form, group, false);
    }
    host.group(form, shown, true);
    host.background(form, GS_VIDEO_BACKGROUND, video);
}

/// The form's preparation (`OpenSettingsForm`): the autosave sliders' labels, the port's quality event and display
/// boxes, every tab from the settings, the video tab shown.
fn open_settings_form<H: SettingsHost>(host: &mut H, state: &Mutex<SettingsForm>) {
    for group in 1..=3 {
        host.group(None, group, false);
    }
    host.slider_label(GS_AUTOSAVE_MINUTES, GS_AUTOSAVE_MINUTES_LABEL, Some(GS_IDS_LOBBY_OFF));
    host.slider_label(GS_AUTOSAVE_COUNT, GS_AUTOSAVE_COUNT_LABEL, None);
    host.trigger(GS_QUALITY_BOX, GS_TRIGGER_SELECTION_CHANGED, EE_SCREENS_GS_PORT_QUALITY);
    for control in [GS_WINDOW_MODE_BOX, GS_UI_SCALE_BOX, GS_RENDER_RESOLUTION_BOX] {
        host.show(control, GS_SHOWN);
    }
    show_audio(host, state, false);
    show_interface(host, false);
    show_video(host, state, false);
    host.call(EE_SCREENS_OP_SRAND_TIME);
    tab(host, Some(GS_FORM_GAME_SETTINGS), 1, true);
}

/// One of the form's events over its state: the event's result; none for another code (nothing called).
pub fn dispatch<H: SettingsHost>(host: &mut H, state: &Mutex<SettingsForm>, event: u32) -> Option<c_long> {
    match event {
        EE_SCREENS_GS_AUDIO_TAB => tab(host, None, 2, false),
        EE_SCREENS_GS_INTERFACE_TAB => tab(host, None, 3, false),
        EE_SCREENS_GS_VIDEO_TAB => tab(host, Some(GS_FORM_GAME_SETTINGS), 1, true),
        EE_SCREENS_GS_CANCEL => {
            // undoes the live music, sound and mouse changes from the saved preferences
            let s = host.settings(false);
            host.call_with(EE_SCREENS_GAME_CONDUCTOR_VOLUME, bits(s.music_volume), 0);
            host.call_with(EE_SCREENS_GAME_UI_MUSIC_VOLUME, bits(s.music_volume), 0);
            host.call_with(EE_SCREENS_GAME_MASTER_VOLUME, bits(s.sound_volume), 0);
            host.call_with(EE_SCREENS_GAME_REVERB, s.reverb_enabled as u32, 0);
            if s.music_enabled {
                music_on(host, state);
            } else {
                music_off(host);
            }
            host.call_with(EE_SCREENS_GAME_MOUSE_SENSITIVITY, bits(s.mouse_sensitivity), 0);
            host.call_with(EE_SCREENS_GAME_LEFT_HANDED, !s.right_hand_mouse_orientation as u32, 0);
            host.call(EE_SCREENS_OP_CLOSE_FORM);
            let music = lock(state).music_file.clone();
            host.set_music_file(&music);
        }
        EE_SCREENS_GS_IN_GAME_OK => {
            // closes the form, or asks for a display change when the rasterizer, screen, mode or anti-aliasing
            // changed
            let before = host.settings(false);
            let old_rasterizer = host.call_with(EE_SCREENS_GAME_CURRENT_RASTERIZER, 0, 0) as c_ulong;
            let (result, mode_index) = apply_settings(host, state);
            let (code, mut rasterizer) = host.list_selection(GS_RENDERING_LIST);
            if code == GS_ERROR_NOT_SUPPORTED {
                rasterizer = 0;
            }
            let (code, mut screen) = host.combo_selection(GS_VIDEO_CARD_BOX);
            if code != GS_ERROR_OK {
                screen = 0;
            }
            let modes = fill_modes(host, rasterizer, screen);
            let old = host.current_mode(before.screen_id as c_ulong);
            // (the original read past an empty list; none here: a zero mode)
            let new = modes.get(mode_index as usize).copied().unwrap_or_default();
            let after = host.settings(false);
            if old_rasterizer == rasterizer
                && before.screen_id as c_ulong == screen
                && old.bit_count == new.bit_count
                && old.height == new.height
                && old.width == new.width
                && before.anti_aliasing == after.anti_aliasing
            {
                host.call(EE_SCREENS_OP_CLOSE_FORM);
            } else {
                host.call(EE_SCREENS_OP_DISPLAY_CHANGED);
            }
            let music = lock(state).music_file.clone();
            host.set_music_file(&music);
            return Some(result);
        }
        EE_SCREENS_GS_IN_GAME_OPEN => {
            // silences the form's music, pauses (the host only), hides the display choices
            let music = host.music_file();
            lock(state).music_file = music;
            host.set_music_file(b"");
            if host.call_with(EE_SCREENS_GAME_IS_SERVER, 0, 0) != 0 {
                host.call(EE_SCREENS_GAME_PAUSE);
            }
            open_settings_form(host, state);
            host.trigger(GS_OK_BUTTON, GS_TRIGGER_BUTTON_UP, EE_SCREENS_GS_IN_GAME_OK);
            for control in [
                GS_VIDEO_CARD_BOX,
                GS_VIDEO_CARD_LABEL,
                GS_RENDERING_LIST,
                GS_RENDERING_LABEL,
                GS_RESOLUTION_LIST,
                GS_RESOLUTION_LABEL,
                // port: in a game the display combo boxes stay hidden, as the resolution list was
                GS_WINDOW_MODE_BOX,
                GS_UI_SCALE_BOX,
                GS_RENDER_RESOLUTION_BOX,
            ] {
                host.show(control, GS_HIDDEN);
            }
            lock(state).in_game = true;
        }
        EE_SCREENS_GS_MUSIC_QUALITY => {
            if host.radio(GS_MUSIC_OFF) {
                music_off(host);
            } else {
                music_on(host, state);
            }
            if host.radio(GS_REVERB_ON) {
                host.call_with(EE_SCREENS_GAME_REVERB, 1, 0);
            } else if host.radio(GS_REVERB_OFF) {
                host.call_with(EE_SCREENS_GAME_REVERB, 0, 0);
            }
        }
        EE_SCREENS_GS_MUSIC_VOLUME => {
            // the slider's value as an unsigned long times 0.01 (a negative value wraps)
            let volume = host.range(GS_MUSIC_VOLUME) as c_ulong as f32 * 0.01f32;
            let result = host.call_with(EE_SCREENS_GAME_CONDUCTOR_VOLUME, bits(volume), 0) as c_long;
            if host.call_with(EE_SCREENS_GAME_GET_CONDUCTOR_STATE, 0, 0) == 1 {
                host.call_with(EE_SCREENS_GAME_UI_MUSIC_VOLUME, bits(volume), 0);
            }
            return Some(result);
        }
        EE_SCREENS_GS_OUT_OF_GAME_OK => {
            let (result, _) = apply_settings(host, state);
            host.pop();
            if lock(state).display_changed {
                // port: the window mode, the UI scale or the render resolution at once
                host.call(EE_SCREENS_GAME_WINDOW_MODE);
                host.call(EE_SCREENS_OP_RESIZE_MENU);
            }
            return Some(result);
        }
        EE_SCREENS_GS_OUT_OF_GAME_OPEN => {
            let music = host.music_file();
            lock(state).music_file = music;
            open_settings_form(host, state);
            host.trigger(GS_OK_BUTTON, GS_TRIGGER_BUTTON_UP, EE_SCREENS_GS_OUT_OF_GAME_OK);
            lock(state).in_game = false;
        }
        EE_SCREENS_GS_RENDERING_SYSTEM => {
            // refills the screen list when another rendering system is selected
            let (_, selection) = host.list_selection(GS_RENDERING_LIST);
            let name = host.list_data(GS_RENDERING_LIST, selection);
            if lock(state).last_rasterizer != name {
                let s = host.settings(false);
                show_screens(host, &s);
            }
            lock(state).last_rasterizer = name;
        }
        EE_SCREENS_GS_RESTORE_DEFAULT => {
            let text = host.string(GS_IDS_ARE_YOU_SURE_YOU_WANT_TO_RESTORE);
            host.question(&text);
        }
        EE_SCREENS_GS_RESTORE_YES => {
            show_audio(host, state, true);
            show_interface(host, true);
            show_video(host, state, true);
        }
        EE_SCREENS_GS_SOUND_VOLUME => {
            let volume = host.entity_range() as c_ulong as f32 * 0.01f32;
            return Some(host.call_with(EE_SCREENS_GAME_MASTER_VOLUME, bits(volume), 0) as c_long);
        }
        EE_SCREENS_GS_TEST_SOUND => {
            let r = host.call_with(EE_SCREENS_OP_RAND, 0, 0) as c_long;
            host.call_with(EE_SCREENS_GAME_TEST_SOUND, GS_TEST_SOUNDS[(r % 4) as usize], 0);
        }
        EE_SCREENS_GS_VIDEO_CARD => {
            // refills the resolution list when another video card is selected
            let (_, selection) = host.combo_selection(GS_VIDEO_CARD_BOX);
            let name = host.combo_data(GS_VIDEO_CARD_BOX, selection);
            if lock(state).last_screen != name {
                let s = host.settings(false);
                show_resolutions(host, &s);
            }
            lock(state).last_screen = name;
        }
        EE_SCREENS_GS_GRAPHICS_QUALITY => {
            // the clicked quality radio's preset
            let Some(id) = host.entity_id() else { return Some(0) };
            let Some(quality) = (0..5).find(|&q| host.control_id(GS_QUALITY_RADIOS[q]) == id) else { return Some(0) };
            set_graphics_quality(host, quality as c_ulong);
        }
        EE_SCREENS_GS_ADVANCED_CHANGED => {
            // checks the quality radio the detail controls match, or shows and checks Custom
            let radio = quality_radio(graphics_quality_of(host));
            host.show(GS_CUSTOM_RADIO, GS_HIDDEN);
            let button = GS_QUALITY_RADIOS[radio as usize];
            if radio == EE_SCREENS_QUALITY_CUSTOM {
                host.show(GS_CUSTOM_RADIO, GS_SHOWN);
            }
            if !host.radio(button) {
                host.check_radio(button);
            }
            port_show_quality(host, state);
        }
        EE_SCREENS_GS_PORT_QUALITY => {
            // a preset sets the detail controls (Custom leaves them; the preset already shown does nothing)
            if lock(state).quality_guard {
                return Some(0);
            }
            let (result, quality) = host.combo_selection(GS_QUALITY_BOX);
            if result != GS_ERROR_OK || quality >= 5 || graphics_quality_of(host) == quality {
                return Some(0);
            }
            lock(state).quality_guard = true;
            set_graphics_quality(host, quality);
            lock(state).quality_guard = false;
        }
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod tests;
