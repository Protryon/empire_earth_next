//! The game settings (`EEGameSettings`, `EEGamePreferences`; ee/EEGamePreferences.h, EEPreferences.cpp): the plain
//! members as [`EeAppSettingsFields`], the base of the C++ class, their defaults (the constructor), and config.yaml's
//! rows: each setting's block, key, kind and member, read with its default written back for a missing or out-of-range
//! value, written in the rows' order (the file's order: docs/DATA_FORMATS.md "config.yaml"). The last game's options
//! are rows over the options' record (game_options.rs). `Load`'s settings that are not stored and its port switches
//! are [`after_load`].
//!
//! The file is ee_plat_core's (`PlatConfig*`, through [`Config`]); the string members stay C++ (`UWideString`,
//! `UString`) and are read and written through [`Strings`].

#![allow(non_snake_case)]

use crate::app::game_options::EeAppOptionsFields;
use std::os::raw::{c_char, c_long, c_ulong};

/// `EEGameSettings`' plain members (the game options and the strings stay C++), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeAppSettingsFields {
    pub mMouseSensitivity: f32,
    pub mSideScrollSpeed: f32,
    pub mGrabScrollSpeed: f32,
    pub mSoundVolume: f32,
    pub mMusicVolume: f32,
    pub mMaxNumberOfSounds: f32,
    pub mUnitScaleFactor: f32,
    pub mZoomLock: f32,
    pub mZoomStyle: c_long,
    pub mL53C: c_long,
    pub mL540: c_long,
    pub mScreenMode: c_long,
    pub mButtonMIPLevelsToDrop: c_long,
    pub mL54C: c_long,
    pub mL550: c_long,
    pub mL554: c_long,
    pub mL558: c_long,
    pub mL55C: c_long,
    pub mL560: c_long,
    /// `gDifficultyLevel`
    pub mComputerPlayerDifficultyLevel: c_long,
    pub mGameBitDepth: c_long,
    pub mGameWindowHeight: c_ulong,
    pub mGameWindowWidth: c_ulong,
    pub mCTSendIntervalStepUp: c_long,
    pub mCTSendIntervalStepDown: c_long,
    pub mMaxPTsPerSecond: c_long,
    pub mMinPTsPerSecond: c_long,
    pub mMaxCTSendInterval: c_long,
    pub mMIPLevelsToDrop: c_long,
    pub mModelDetail: c_long,
    pub mEffectDetail: c_long,
    pub mNetworkAdapter: c_long,
    pub mPTRateTuningThreshold: c_ulong,
    pub mScreenID: c_long,
    pub mStartingKeyboardLayout: c_long,
    pub mTerrainMIPLevelsToDrop: c_long,
    pub mTextureBitDepth: c_long,
    pub mNumberOfAutoSaves: c_ulong,
    pub mAutoSaveInMilliseconds: c_ulong,
    pub mLastOpenPostGameStatsPage: c_long,
    pub mL5B8: c_long,
    pub mL5BC: c_long,
    pub mB5C0: bool,
    /// `gZoomEnabled`
    pub mZoomEnabled: bool,
    pub mB5C2: bool,
    pub mAnimationSmoothing: bool,
    pub mB5C4: bool,
    pub mB5C5: bool,
    pub mB5C6: bool,
    pub mDisplayClouds: bool,
    pub mDisplayShadows: bool,
    pub mDisplayBattlefieldAmbients: bool,
    pub mB5CA: bool,
    pub mB5CB: bool,
    pub mB5CC: bool,
    pub mEnableAnimalHerds: bool,
    pub mTrilinearFilter: bool,
    /// the start screen shows when set
    pub mFullScreen: bool,
    pub mReverbEnabled: bool,
    pub mWaitForVSync: bool,
    pub mAmbientSounds: bool,
    pub mShowBlood: bool,
    pub mMusicEnabled: bool,
    pub mRightHandMouseOrientation: bool,
    pub mB5D6: bool,
    pub mUnitLighting: bool,
    pub mUseAreaEffectOverlays: bool,
    pub mUseCandidateWindow: bool,
    pub mDisplayWeatherEffects: bool,
    pub mDebugOOSHandling: bool,
    pub mTakeJPGScreenshots: bool,
    pub mAntiAliasing: bool,
    pub mTakeEndGameScreenshots: bool,
    pub mShowAutoWaypoints: bool,
    /// port: 0 (the original textures), 2 or 4 (Data/Textures-4x)
    pub mTextureResolution: c_long,
    /// port: percent, 0 Auto
    pub mUIScale: c_long,
    /// port: percent of the display's pixels (50..100)
    pub mRenderResolution: c_long,
    /// port: 0 full screen (borderless), 1 windowed
    pub mWindowMode: c_long,
    /// port: 0 v-sync, -1 unlimited, else frames a second
    pub mFrameRateLimit: c_long,
    /// port: 0 bilinear, 1 trilinear, 2 anisotropic
    pub mTextureFiltering: c_long,
    /// port: percent (100 unchanged)
    pub mBrightness: c_long,
    /// port: percent
    pub mMasterVolume: c_long,
    pub mMuteInBackground: bool,
    /// port: percent of the original's farthest zoom (100..300)
    pub mMaximumZoomOut: c_long,
    /// port: 1 writes Logs/game-*.log
    pub mGameLog: c_long,
    /// port: the audio device's name (empty: the system's default)
    pub mAudioDevice: [c_char; 128],
}

impl Default for EeAppSettingsFields {
    /// `EEGameSettings()`'s values (0x1324f8)
    fn default() -> Self {
        EeAppSettingsFields {
            mMouseSensitivity: 0.3125,
            mSideScrollSpeed: 2.0,
            mGrabScrollSpeed: 0.2,
            mSoundVolume: 1.0,
            mMusicVolume: 0.65,
            mMaxNumberOfSounds: 1.0,
            mUnitScaleFactor: 1.0,
            mZoomLock: 1.0,
            mZoomStyle: 0,
            mL53C: 15020,
            mL540: 0,
            mScreenMode: 1,
            mButtonMIPLevelsToDrop: 0,
            mL54C: 0,
            mL550: 0,
            mL554: 0,
            mL558: 0,
            mL55C: 0,
            mL560: 0,
            mComputerPlayerDifficultyLevel: 1,
            mGameBitDepth: 16,
            mGameWindowHeight: 600,
            mGameWindowWidth: 800,
            mCTSendIntervalStepUp: 125,
            mCTSendIntervalStepDown: 10,
            mMaxPTsPerSecond: 125,
            mMinPTsPerSecond: 15,
            mMaxCTSendInterval: 2250,
            mMIPLevelsToDrop: 0,
            mModelDetail: 2,
            mEffectDetail: 2,
            mNetworkAdapter: 0,
            mPTRateTuningThreshold: 30,
            mScreenID: 0,
            mStartingKeyboardLayout: 0,
            mTerrainMIPLevelsToDrop: 0,
            mTextureBitDepth: 16,
            mNumberOfAutoSaves: 10,
            mAutoSaveInMilliseconds: 600000,
            mLastOpenPostGameStatsPage: 1,
            mL5B8: 0,
            mL5BC: 1,
            mB5C0: true,
            mZoomEnabled: true,
            mB5C2: true,
            mAnimationSmoothing: true,
            mB5C4: false,
            mB5C5: true,
            mB5C6: false,
            mDisplayClouds: true,
            mDisplayShadows: true,
            mDisplayBattlefieldAmbients: true,
            mB5CA: true,
            mB5CB: true,
            mB5CC: true,
            mEnableAnimalHerds: true,
            mTrilinearFilter: true,
            mFullScreen: true,
            mReverbEnabled: true,
            mWaitForVSync: true,
            mAmbientSounds: true,
            mShowBlood: true,
            mMusicEnabled: true,
            mRightHandMouseOrientation: true,
            mB5D6: false,
            mUnitLighting: true,
            mUseAreaEffectOverlays: true,
            mUseCandidateWindow: false,
            mDisplayWeatherEffects: false,
            mDebugOOSHandling: true,
            mTakeJPGScreenshots: false,
            mAntiAliasing: false,
            mTakeEndGameScreenshots: false,
            mShowAutoWaypoints: false,
            mTextureResolution: 0,
            mUIScale: 0,
            mRenderResolution: 100,
            mWindowMode: 0,
            mFrameRateLimit: 0,
            mTextureFiltering: 2,
            mBrightness: 100,
            mMasterVolume: 100,
            mMuteInBackground: true,
            mMaximumZoomOut: 200,
            mGameLog: 1,
            mAudioDevice: [0; 128],
        }
    }
}

/// The C++ string members by id ([`Strings`]): the settings' `UWideString`s, then their `UString`s.
pub const EE_APP_SETTINGS_PLAYER_NAME: u32 = 0;
pub const EE_APP_SETTINGS_HOST_GAME_NAME: u32 = 1;
pub const EE_APP_SETTINGS_LAST_QUICK_SAVE: u32 = 2;
pub const EE_APP_SETTINGS_LAST_AUTO_SAVE: u32 = 3;
/// the last game options' map type (`mGameOptions.mMapType`)
pub const EE_APP_SETTINGS_MAP_TYPE: u32 = 4;
/// `UString`s (narrow)
pub const EE_APP_SETTINGS_LAST_GAME_FILE: u32 = 5;
pub const EE_APP_SETTINGS_LAST_SCENARIO_FILE: u32 = 6;

/// config.yaml (`PlatConfig*`): the block's path and the key; `None` / false for a missing value or a failed write.
pub trait Config {
    fn get_int(&mut self, block: &str, key: &str) -> Option<i32>;
    fn get_string(&mut self, block: &str, key: &str) -> Option<Vec<u8>>;
    fn set_int(&mut self, block: &str, key: &str, value: i32) -> bool;
    fn set_bool(&mut self, block: &str, key: &str, value: bool) -> bool;
    fn set_string(&mut self, block: &str, key: &str, value: &[u8]) -> bool;
}

/// The C++ string members of the settings and of their defaults (`defaults` true).
pub trait Strings {
    fn get(&mut self, id: u32) -> Vec<u8>;
    fn set(&mut self, id: u32, value: &[u8]);
    /// the member set to the defaults' (a missing or unreadable value)
    fn set_default(&mut self, id: u32);
}

/// A row's kind.
#[derive(Clone, Copy, Debug, PartialEq)]
enum Kind {
    /// a `long` or `unsigned long`
    Long,
    /// a `float`, stored as a whole percent
    Percent,
    /// a `bool`, stored true/false
    Bool,
    /// a `long` 0 or 1, stored true/false
    LongFlag,
    /// a C++ string member (wide or narrow: the same UTF-8 bytes)
    Str(u32),
    /// `mAudioDevice`
    Chars,
}

/// Where a row's member lives: the settings' record, the options' record (by byte offset) or C++ (a string).
#[derive(Clone, Copy, Debug)]
enum Field {
    Settings(usize),
    Options(usize),
    None,
}

#[derive(Clone, Copy, Debug)]
struct Row {
    block: &'static str,
    key: &'static str,
    kind: Kind,
    field: Field,
    minimum: c_long,
    maximum: c_long,
    clamp: bool,
}

const ANY_MINIMUM: c_long = c_long::MIN;
const ANY_MAXIMUM: c_long = c_long::MAX;

macro_rules! s {
    ($f:ident) => {
        Field::Settings(std::mem::offset_of!(EeAppSettingsFields, $f))
    };
}
macro_rules! o {
    ($f:ident) => {
        Field::Options(std::mem::offset_of!(EeAppOptionsFields, $f))
    };
}

const fn row(
    block: &'static str,
    key: &'static str,
    kind: Kind,
    field: Field,
    minimum: c_long,
    maximum: c_long,
    clamp: bool,
) -> Row {
    Row { block, key, kind, field, minimum, maximum, clamp }
}
const fn int(block: &'static str, key: &'static str, field: Field) -> Row {
    row(block, key, Kind::Long, field, ANY_MINIMUM, ANY_MAXIMUM, false)
}
const fn flag(block: &'static str, key: &'static str, field: Field) -> Row {
    row(block, key, Kind::Bool, field, 0, 0, false)
}
const fn text(block: &'static str, key: &'static str, id: u32) -> Row {
    row(block, key, Kind::Str(id), Field::None, 0, 0, false)
}

/// The last game options' rows (config.yaml's game_options block).
fn option_rows() -> Vec<Row> {
    vec![
        int("game_options.map", "size", o!(mMapSize)),
        text("game_options.map", "type", EE_APP_SETTINGS_MAP_TYPE),
        int("game_options", "starting_resources", o!(mResources)),
        int("game_options.epochs", "start", o!(mEpochStart)),
        int("game_options.epochs", "end", o!(mEpochEnd)),
        int("game_options", "unit_limit", o!(mUnitLimit)),
        int("game_options", "wonders_for_victory", o!(mWondersForVictory)),
        int("game_options", "variant", o!(mVariant)),
        int("game_options", "difficulty", o!(mDifficulty)),
        int("game_options", "speed", o!(mGameSpeed)),
        flag("game_options", "record_game", o!(mRecordGames)),
        flag("game_options", "reveal_map", o!(mRevealMap)),
        flag("game_options", "allow_custom_civs", o!(mAllowCustomCivs)),
        flag("game_options", "lock_teams", o!(mLockTeams)),
        flag("game_options", "lock_speed", o!(mLockSpeed)),
        flag("game_options", "cheat_codes", o!(mCheatCodes)),
        flag("game_options", "victory_allowed", o!(mVictory)),
    ]
}

/// The settings' rows. Settings the port does not use any more are not stored: the rasterizer name (one renderer),
/// the bit depths (32), "Wait for VSync" (the frame rate limit decides it), the WON and Sierra.com accounts, the IME
/// candidate window and the values nothing reads (unit scale factor, animal herds, area effect overlays, button MIP
/// levels, auto waypoints, keyboard layout, load game name).
fn setting_rows() -> Vec<Row> {
    use Kind::*;
    vec![
        text("player", "name", EE_APP_SETTINGS_PLAYER_NAME),
        int("player", "computer_difficulty", s!(mComputerPlayerDifficultyLevel)),
        row("display", "screen", Long, s!(mScreenID), 0, ANY_MAXIMUM, false),
        row("display", "window_mode", Long, s!(mWindowMode), 0, 1, false), // 0 full screen, 1 windowed
        row("display.resolution", "width", Long, s!(mGameWindowWidth), 1, ANY_MAXIMUM, false),
        row("display.resolution", "height", Long, s!(mGameWindowHeight), 1, ANY_MAXIMUM, false),
        row("display", "ui_scale", Long, s!(mUIScale), 0, 300, false), // percent; 0 Auto
        row("display", "render_resolution", Long, s!(mRenderResolution), 50, 100, false),
        row("display", "frame_rate_limit", Long, s!(mFrameRateLimit), -1, 1000, false), // 0 v-sync, -1 none
        row("display", "brightness", Long, s!(mBrightness), 50, 200, false),
        row("graphics", "texture_resolution", Long, s!(mTextureResolution), 0, 4, false), // 0, 2 or 4
        row("graphics", "texture_filtering", Long, s!(mTextureFiltering), 0, 2, false),
        flag("graphics", "trilinear_filter", s!(mTrilinearFilter)),
        row("graphics.mip_levels_to_drop", "textures", Long, s!(mMIPLevelsToDrop), 0, 8, true),
        row("graphics.mip_levels_to_drop", "terrain", Long, s!(mTerrainMIPLevelsToDrop), 0, 8, true),
        row("graphics", "model_detail", Long, s!(mModelDetail), ANY_MINIMUM, 2, true),
        row("graphics", "effect_detail", Long, s!(mEffectDetail), ANY_MINIMUM, 2, true),
        flag("graphics", "unit_lighting", s!(mUnitLighting)),
        flag("graphics", "animation_smoothing", s!(mAnimationSmoothing)),
        flag("graphics", "shadows", s!(mDisplayShadows)),
        flag("graphics", "clouds", s!(mDisplayClouds)),
        flag("graphics", "battlefield_ambients", s!(mDisplayBattlefieldAmbients)),
        flag("graphics", "show_blood", s!(mShowBlood)),
        row("audio", "device", Chars, Field::None, 0, 0, false), // empty: the system's default
        row("audio", "master_volume", Long, s!(mMasterVolume), 0, 100, false),
        row("audio", "sound_volume", Percent, s!(mSoundVolume), 0, 100, true),
        flag("audio.music", "enabled", s!(mMusicEnabled)),
        row("audio.music", "volume", Percent, s!(mMusicVolume), 0, 100, true),
        flag("audio", "reverb", s!(mReverbEnabled)),
        flag("audio", "ambient_sounds", s!(mAmbientSounds)),
        row("audio", "max_sounds", Percent, s!(mMaxNumberOfSounds), 0, ANY_MAXIMUM, true),
        flag("audio", "mute_in_background", s!(mMuteInBackground)),
        row("input", "mouse_sensitivity", Percent, s!(mMouseSensitivity), 1, ANY_MAXIMUM, true),
        flag("input", "right_hand_mouse", s!(mRightHandMouseOrientation)),
        row("input.scroll", "side_speed", Percent, s!(mSideScrollSpeed), 1, ANY_MAXIMUM, true),
        row("input.scroll", "grab_speed", Percent, s!(mGrabScrollSpeed), 1, ANY_MAXIMUM, true),
        row("camera", "maximum_zoom_out", Long, s!(mMaximumZoomOut), 100, 300, false),
        int("camera", "zoom_style", s!(mZoomStyle)),
        row("camera", "zoom_lock", Percent, s!(mZoomLock), ANY_MINIMUM, ANY_MAXIMUM, false),
        row("autosave", "interval_ms", Long, s!(mAutoSaveInMilliseconds), 0, ANY_MAXIMUM, false),
        row("autosave", "count", Long, s!(mNumberOfAutoSaves), 0, ANY_MAXIMUM, false),
        text("network", "host_game_name", EE_APP_SETTINGS_HOST_GAME_NAME),
        row("network", "adapter", Long, s!(mNetworkAdapter), 0, ANY_MAXIMUM, false),
        int("network.pt_rate", "min", s!(mMinPTsPerSecond)),
        int("network.pt_rate", "max", s!(mMaxPTsPerSecond)),
        int("network.pt_rate", "tuning_threshold", s!(mPTRateTuningThreshold)), // percent
        int("network.ct_send_interval", "max", s!(mMaxCTSendInterval)),
        int("network.ct_send_interval", "step_down", s!(mCTSendIntervalStepDown)),
        int("network.ct_send_interval", "step_up", s!(mCTSendIntervalStepUp)),
        flag("screenshots", "jpg", s!(mTakeJPGScreenshots)),
        flag("screenshots", "end_game", s!(mTakeEndGameScreenshots)),
        row("debug", "game_log", LongFlag, s!(mGameLog), 0, 0, false), // Logs/game-*.log (ee/EEGameLog.h)
        flag("debug", "oos_handling", s!(mDebugOOSHandling)),
        text("recent", "game_file", EE_APP_SETTINGS_LAST_GAME_FILE),
        text("recent", "scenario_file", EE_APP_SETTINGS_LAST_SCENARIO_FILE),
        text("recent", "quick_save", EE_APP_SETTINGS_LAST_QUICK_SAVE),
        text("recent", "auto_save", EE_APP_SETTINGS_LAST_AUTO_SAVE),
        int("recent", "post_game_stats_page", s!(mLastOpenPostGameStatsPage)),
    ]
}

/// The records a row's member lives in (the settings' or their defaults').
#[derive(Clone, Copy)]
pub struct Records {
    pub settings: *mut EeAppSettingsFields,
    pub options: *mut EeAppOptionsFields,
}

impl Records {
    fn at(self, field: Field) -> *mut u8 {
        match field {
            Field::Settings(o) => unsafe { (self.settings as *mut u8).add(o) },
            Field::Options(o) => unsafe { (self.options as *mut u8).add(o) },
            Field::None => std::ptr::null_mut(),
        }
    }
    // SAFETY (the accessors): the records are live for the call (the callers' contract) and the offset is a member of
    // the row's type: c_long / c_ulong (the same size), f32, bool
    fn long(self, f: Field) -> c_long {
        unsafe { (self.at(f) as *const c_long).read_unaligned() }
    }
    fn set_long(self, f: Field, v: c_long) {
        unsafe { (self.at(f) as *mut c_long).write_unaligned(v) }
    }
    fn float(self, f: Field) -> f32 {
        unsafe { (self.at(f) as *const f32).read_unaligned() }
    }
    fn set_float(self, f: Field, v: f32) {
        unsafe { (self.at(f) as *mut f32).write_unaligned(v) }
    }
    fn flag(self, f: Field) -> bool {
        unsafe { *self.at(f) != 0 }
    }
    fn set_flag(self, f: Field, v: bool) {
        unsafe { *self.at(f) = u8::from(v) }
    }
    fn chars(self) -> &'static mut [c_char; 128] {
        unsafe { &mut (*self.settings).mAudioDevice }
    }
}

/// The C cast `(long)f`: truncation; the x86 conversion's "integer indefinite" (the type's minimum) out of range.
fn ftol(f: f32) -> c_long {
    let t = f.trunc();
    if t.is_nan() || t < c_long::MIN as f32 || t >= -(c_long::MIN as f32) {
        c_long::MIN
    } else {
        t as c_long
    }
}

/// `(int32_t)value`: UConfig's write of a `long` (64 bits on LP64).
#[allow(clippy::unnecessary_cast)]
fn to_i32(v: c_long) -> i32 {
    v as i32
}

fn clamp(v: c_long, r: &Row) -> c_long {
    if v < r.minimum {
        r.minimum
    } else if v > r.maximum {
        r.maximum
    } else {
        v
    }
}

/// The C string of `mAudioDevice` (up to its NUL).
fn chars_text(c: &[c_char; 128]) -> Vec<u8> {
    c.iter().take_while(|&&b| b != 0).map(|&b| b as u8).collect()
}

/// `snprintf(mAudioDevice, 128, "%s", text)`.
fn set_chars(c: &mut [c_char; 128], text: &[u8]) {
    let n = text.iter().position(|&b| b == 0).unwrap_or(text.len()).min(127);
    for (d, &s) in c.iter_mut().zip(text[..n].iter()) {
        *d = s as c_char;
    }
    c[n] = 0;
}

/// `EEWriteSetting`: one row's member written; whether it was.
fn write_row(cfg: &mut dyn Config, strings: &mut dyn Strings, rec: Records, r: &Row) -> bool {
    match r.kind {
        Kind::Long => cfg.set_int(r.block, r.key, to_i32(rec.long(r.field))),
        Kind::Percent => {
            let mut v = ftol(rec.float(r.field) * 100.0f32);
            if r.clamp {
                v = clamp(v, r);
            }
            cfg.set_int(r.block, r.key, to_i32(v))
        }
        Kind::Bool => cfg.set_bool(r.block, r.key, rec.flag(r.field)),
        Kind::LongFlag => cfg.set_bool(r.block, r.key, rec.long(r.field) != 0),
        Kind::Str(id) => {
            let v = strings.get(id);
            cfg.set_string(r.block, r.key, &v)
        }
        Kind::Chars => {
            let v = chars_text(rec.chars());
            cfg.set_string(r.block, r.key, &v)
        }
    }
}

/// `EEReadSetting`: one row read into its member; a missing value (or one out of range) is the default's, written
/// to the file.
fn read_row(cfg: &mut dyn Config, strings: &mut dyn Strings, rec: Records, defaults: Records, r: &Row) {
    let read = match r.kind {
        Kind::Long | Kind::Percent => {
            let mut read = false;
            if let Some(v) = cfg.get_int(r.block, r.key) {
                let mut v = v as c_long;
                if r.clamp {
                    v = clamp(v, r);
                }
                if v >= r.minimum && v <= r.maximum {
                    read = true;
                    if r.kind == Kind::Long {
                        rec.set_long(r.field, v);
                    } else {
                        rec.set_float(r.field, v as f32 * 0.01f32);
                    }
                }
            }
            if !read {
                if r.kind == Kind::Long {
                    rec.set_long(r.field, defaults.long(r.field));
                } else {
                    rec.set_float(r.field, defaults.float(r.field));
                }
            }
            read
        }
        Kind::Bool | Kind::LongFlag => {
            let v = cfg.get_int(r.block, r.key).map(|v| v != 0);
            if r.kind == Kind::Bool {
                rec.set_flag(r.field, v.unwrap_or_else(|| defaults.flag(r.field)));
            } else {
                rec.set_long(
                    r.field,
                    match v {
                        Some(f) => c_long::from(f),
                        None => c_long::from(defaults.long(r.field) != 0),
                    },
                );
            }
            v.is_some()
        }
        Kind::Str(id) => match cfg.get_string(r.block, r.key) {
            Some(v) => {
                strings.set(id, &v);
                true
            }
            None => {
                strings.set_default(id);
                false
            }
        },
        Kind::Chars => {
            let v = cfg.get_string(r.block, r.key);
            let text = match &v {
                Some(t) => t.clone(),
                None => chars_text(defaults.chars()),
            };
            set_chars(rec.chars(), &text);
            v.is_some()
        }
    };
    if !read {
        write_row(cfg, strings, rec, r);
    }
}

fn rows(game_options: bool) -> Vec<Row> {
    if game_options {
        option_rows()
    } else {
        setting_rows()
    }
}

/// `EEReadSettings`: every row of the settings (or, `game_options`, of the last game options) read.
pub fn read(cfg: &mut dyn Config, strings: &mut dyn Strings, rec: Records, defaults: Records, game_options: bool) {
    for r in rows(game_options) {
        read_row(cfg, strings, rec, defaults, &r);
    }
}

/// `EEWriteSettings`: every row written; the first failure (`kErrorNotFound`), else 0.
pub fn write(cfg: &mut dyn Config, strings: &mut dyn Strings, rec: Records, game_options: bool) -> i32 {
    let mut result = 0;
    for r in rows(game_options) {
        let ok = write_row(cfg, strings, rec, &r);
        if result == 0 && !ok {
            result = EE_APP_SETTINGS_ERROR_NOT_FOUND;
        }
    }
    result
}

/// `kErrorNotFound`: UConfig's failed read or write.
pub const EE_APP_SETTINGS_ERROR_NOT_FOUND: i32 = 3;

/// The last game options' defaults (`EEDefaultSettings`, the original's literals; the map type "Continental" is the
/// C++'s).
pub fn last_options_default(o: &mut EeAppOptionsFields, epochs: c_long) {
    o.mMapSize = 1;
    o.mResources = 0;
    o.mEpochStart = 0;
    o.mEpochEnd = epochs.wrapping_sub(1);
    o.mUnitLimit = 500;
    o.mWondersForVictory = 3;
    o.mVariant = 2;
    o.mDifficulty = 1;
    o.mGameSpeed = 1;
    o.mRecordGames = false;
    o.mRevealMap = false;
    o.mAllowCustomCivs = true;
    o.mLockTeams = false;
    o.mLockSpeed = false;
    o.mCheatCodes = true;
    o.mVictory = true;
}

/// `Load`'s rest after the rows: a texture resolution other than 0, 2 or 4 is the default's (written back: returns
/// true), no blood when `korean`, the settings that are not stored (the defaults' or fixed), and the port's switches
/// (full screen is window mode 0, v-sync is frame rate limit 0).
pub fn after_load(s: &mut EeAppSettingsFields, d: &EeAppSettingsFields, korean: bool) -> bool {
    let mut rewrite = false;
    if s.mTextureResolution != 0 && s.mTextureResolution != 2 && s.mTextureResolution != 4 {
        s.mTextureResolution = d.mTextureResolution;
        rewrite = true;
    }
    if korean {
        s.mShowBlood = false;
    }
    s.mGameBitDepth = 32;
    s.mTextureBitDepth = 32;
    s.mUseCandidateWindow = d.mUseCandidateWindow;
    s.mStartingKeyboardLayout = d.mStartingKeyboardLayout;
    s.mButtonMIPLevelsToDrop = d.mButtonMIPLevelsToDrop;
    s.mUnitScaleFactor = d.mUnitScaleFactor;
    s.mEnableAnimalHerds = d.mEnableAnimalHerds;
    s.mUseAreaEffectOverlays = d.mUseAreaEffectOverlays;
    s.mScreenMode = d.mScreenMode;
    s.mL5BC = d.mL5BC;
    s.mL5B8 = 0;
    s.mShowAutoWaypoints = false;
    s.mB5C0 = true;
    s.mL53C = 15020;
    s.mL540 = 0;
    s.mB5C5 = true;
    s.mB5C2 = true;
    s.mB5CA = true;
    s.mB5CB = true;
    s.mB5CC = true;
    s.mDisplayWeatherEffects = false;
    s.mZoomEnabled = true;
    s.mL54C = 0;
    s.mL550 = 0;
    s.mL554 = 0;
    s.mL558 = 0;
    s.mL55C = 0;
    s.mL560 = 0;
    s.mB5C4 = false;
    s.mB5C6 = false;
    s.mAntiAliasing = false;
    s.mFullScreen = s.mWindowMode == 0;
    s.mWaitForVSync = s.mFrameRateLimit == 0;
    rewrite
}
