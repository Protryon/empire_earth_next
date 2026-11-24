//! The game preferences' bodies (game core lane, round 11, lane B; were EEPreferences.cpp's `Load`, `Save`, the
//! game options' load and save, `SetLastGameOptions`, `IsNotKorean`, `EEApplyPortSettings`,
//! `EESetTextureResolution`, `EEInitializeDefaultSettings` and the settings' string callback): config.yaml's rows
//! (ee_game app settings.rs) over the C++ string members, each a one-statement case of EEGameLog.cpp's
//! `EECore11OpB` ([`super::EeGameCpp`]'s `Ep` ops).
//!
//! `EEGamePreferences` and `EEGameSettings` keep their members in place (the UI, the lobby and EEGlobals.cpp's
//! references read them): the plain ones are ee_game app settings.rs's `EeAppSettingsFields`, the base of
//! `EEGameSettings`; the strings (`UWideString`, `UString`) are reached by address. The full-screen line of `Load`
//! stays in C++ (the test profile's windowed overlay patches it: tools/build.py).
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_void, CStr};

use crate::app::game_options::EeAppOptionsFields;
use crate::app::settings::{
    self, EeAppSettingsFields, Records, Strings, EE_APP_SETTINGS_HOST_GAME_NAME, EE_APP_SETTINGS_LAST_AUTO_SAVE,
    EE_APP_SETTINGS_LAST_GAME_FILE, EE_APP_SETTINGS_LAST_QUICK_SAVE, EE_APP_SETTINGS_MAP_TYPE,
    EE_APP_SETTINGS_PLAYER_NAME,
};
use crate::app::settings_ffi::PlatConfig;

use super::global_objects as gobj;
use super::{h, hp, pa, Owned, O, P};

/// The resources' and the system's Korean language id.
const KOREAN: i64 = 0x412;

/// A string member of the settings: its offset and whether it is a `UWideString` (else a `UString`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Member {
    pub off: usize,
    pub wide: bool,
}

/// The string row `id`'s member (ee_game app settings.rs `EE_APP_SETTINGS_*`): the wide ones, else the narrow
/// last game or scenario file.
pub fn member(id: u32) -> Member {
    let wide = |off| Member { off, wide: true };
    match id {
        EE_APP_SETTINGS_PLAYER_NAME => wide(gobj::SET_PLAYER_NAME),
        EE_APP_SETTINGS_HOST_GAME_NAME => wide(gobj::SET_HOST_GAME_NAME),
        EE_APP_SETTINGS_LAST_QUICK_SAVE => wide(gobj::SET_LAST_QUICK_SAVE),
        EE_APP_SETTINGS_LAST_AUTO_SAVE => wide(gobj::SET_LAST_AUTO_SAVE),
        EE_APP_SETTINGS_MAP_TYPE => wide(gobj::SET_MAP_TYPE),
        EE_APP_SETTINGS_LAST_GAME_FILE => Member { off: gobj::SET_LAST_GAME_FILE, wide: false },
        _ => Member { off: gobj::SET_LAST_SCENARIO_FILE, wide: false },
    }
}

/// A string member's text (a `UString` through its `UWideString` conversion, as the callback read it).
pub fn get_text(m: P, wide: bool) -> Vec<u8> {
    let (length, get) = if wide { (O::EpWideLength, O::EpWideGet) } else { (O::EpNarrowLength, O::EpNarrowGet) };
    let n = h(length, m, &[]) as usize;
    let mut buffer = vec![0u8; n + 1];
    h(get, m, &[buffer.as_mut_ptr() as usize as i64, (n + 1) as i64]);
    buffer.truncate(n);
    buffer
}

/// A string member set to a text (no NUL inside: the text ends at the first).
pub fn set_text(m: P, wide: bool, text: &[u8]) {
    let n = text.iter().position(|&b| b == 0).unwrap_or(text.len());
    let mut buffer = text[..n].to_vec();
    buffer.push(0);
    h(if wide { O::EpWideSet } else { O::EpNarrowSet }, m, &[buffer.as_ptr() as usize as i64]);
}

/// A string member copied from another of the same kind.
fn copy(m: P, wide: bool, from: P) {
    h(if wide { O::EpWideCopy } else { O::EpNarrowCopy }, m, &[pa(from)]);
}

fn fields<'a>(s: P) -> &'a mut EeAppSettingsFields {
    // SAFETY: the settings' plain members (the base), used before the next C++ call that writes them
    unsafe { &mut *(s as *mut EeAppSettingsFields) }
}

fn records(s: P) -> Records {
    Records { settings: s as *mut EeAppSettingsFields, options: (s + gobj::SET_OPTIONS) as *mut EeAppOptionsFields }
}

/// The settings' strings (`s`) and their defaults' (`d`).
struct Members {
    s: P,
    d: P,
}

impl Strings for Members {
    fn get(&mut self, id: u32) -> Vec<u8> {
        let m = member(id);
        get_text(self.s + m.off, m.wide)
    }
    fn set(&mut self, id: u32, value: &[u8]) {
        let m = member(id);
        set_text(self.s + m.off, m.wide, value);
    }
    fn set_default(&mut self, id: u32) {
        let m = member(id);
        copy(self.s + m.off, m.wide, self.d + m.off);
    }
}

/// `EEDefaultSettings`: a default-constructed settings object's names from `sDefaultSettings` (the ones
/// `EEInitializeDefaultSettings` set) and the last game options' literals (not a copy of `sDefaultSettings`, whose
/// game options' assignment reads the world rules, not loaded yet).
fn default_settings(d: P) {
    let defaults = hp(O::EpDefaultSettings, 0, &[]);
    for off in [gobj::SET_PLAYER_NAME, gobj::SET_HOST_GAME_NAME] {
        copy(d + off, true, defaults + off);
    }
    // SAFETY: the defaults' game options' plain members
    let options = unsafe { &mut *((d + gobj::SET_OPTIONS) as *mut EeAppOptionsFields) };
    settings::last_options_default(options, ee_sim::tech::tech_ids::ee_tech_epoch_count() as c_long);
    set_text(d + gobj::SET_MAP_TYPE, true, b"Continental");
}

/// `EEReadSettings`: one part's rows (the settings, or the last game options) read from config.yaml.
fn read_settings(s: P, game_options: bool) {
    let d = Owned::new(O::EpNewSettings, O::EpDeleteSettings);
    default_settings(d.0);
    settings::read(&mut PlatConfig, &mut Members { s, d: d.0 }, records(s), records(d.0), game_options);
}

/// `EEWriteSettings`: one part's rows written -> the first failure, else 0.
fn write_settings(s: P, game_options: bool) -> c_long {
    settings::write(&mut PlatConfig, &mut Members { s, d: s }, records(s), game_options) as c_long
}

fn config_int(block: &CStr, key: &CStr) -> Option<i32> {
    let mut v = 0;
    // SAFETY: NUL-ended strings and a live out value
    (unsafe { ee_base::plat_core::config::PlatConfigGetInt(block.as_ptr(), key.as_ptr(), &mut v) } != 0).then_some(v)
}

/// `IsNotKorean`: neither the game resources' language nor the system's is Korean.
#[no_mangle]
pub extern "C" fn ee_gamecpp_prefs_is_not_korean() -> bool {
    !(h(O::EpResourceLanguage, 0, &[]) == KOREAN || i64::from(ee_base::util::system::ee_util_os_language()) == KOREAN)
}

/// The texture packs (`EESetTextureResolution`; docs/DATA_FORMATS.md "Texture packs"): resolution 2 draws
/// Data/Textures-4x at half size (one more MIP level dropped), 4 at full size, anything else the original textures.
pub fn texture_pack(resolution: c_long) -> (Option<&'static CStr>, c_long, c_long) {
    match resolution {
        2 => (Some(c"textures-4x"), 2, 1),
        4 => (Some(c"textures-4x"), 2, 0),
        _ => (None, 0, 0),
    }
}

/// `EESetTextureResolution`.
#[no_mangle]
pub extern "C" fn ee_gamecpp_prefs_texture_resolution(resolution: c_long) {
    let (directory, shift, drop) = texture_pack(resolution);
    let directory = directory.map_or(std::ptr::null(), |d| d.as_ptr());
    // SAFETY: NULL or a NUL-ended name
    unsafe { ee_render::ge::manager_ffi::ee_ge_texpack_set(directory, shift, drop) };
}

/// `EEApplyPortSettings`: the port's settings to the camera, the renderer and the mixer.
#[no_mangle]
pub extern "C" fn ee_gamecpp_prefs_apply_port(settings: *mut c_void) {
    let s = *fields(settings as P);
    crate::gamecpp::globals::set!(gCameraMaxZoom, s.mMaximumZoomOut as f32 / 100.0f32);
    sys::render_percent(s.mRenderResolution as _);
    sys::frame_limit(s.mFrameRateLimit);
    sys::texture_filtering(s.mTextureFiltering as _);
    sys::brightness(s.mBrightness as f32 / 100.0f32);
    sys::master_volume(s.mMasterVolume as f32 / 100.0f32);
    sys::output_device(s.mAudioDevice.as_ptr());
}

/// `EEGamePreferences::Load` but its full-screen line: every setting read from config.yaml (the default written
/// for each missing value; no blood in Korean), the settings that are not stored, the port's switches, then the
/// game options -> their result.
#[no_mangle]
pub extern "C" fn ee_gamecpp_prefs_load(prefs: *mut c_void) -> c_long {
    let s = prefs as P + gobj::PREFS_SETTINGS;
    let defaults = hp(O::EpDefaultSettings, 0, &[]);
    read_settings(s, false);
    let korean = !ee_gamecpp_prefs_is_not_korean();
    let d = *fields(defaults);
    if settings::after_load(fields(s), &d, korean) {
        let resolution = fields(s).mTextureResolution;
        // SAFETY: NUL-ended strings
        unsafe {
            ee_base::plat_core::config::PlatConfigSetInt(
                c"graphics".as_ptr(),
                c"texture_resolution".as_ptr(),
                resolution as i32,
            )
        };
    }
    let kept = [
        (gobj::SET_RASTERIZER_NAME, false),
        (gobj::SET_SIERRA_USER_ID, true),
        (gobj::SET_SIERRA_GAME_NAME, true),
        (gobj::SET_WON_NET_PASSWORD, false),
    ];
    for (off, wide) in kept {
        copy(s + off, wide, defaults + off);
    }
    // the game's own location is where it runs from (the data root), never a stored path: a play folder can move
    for (from, to) in
        [(O::EpStartupVolume, gobj::SET_INSTALLED_VOLUME), (O::EpStartupDirectory, gobj::SET_INSTALLED_DIRECTORY)]
    {
        let m = s + to;
        h(from, m, &[]);
        h(O::EpNarrowUpper, m, &[]);
    }
    ee_gamecpp_prefs_apply_port(s as *mut c_void);
    ee_gamecpp_prefs_texture_resolution(fields(s).mTextureResolution);
    ee_gamecpp_prefs_load_options(prefs)
}

/// `LoadGameOptions`: the last game options from config.yaml's game_options block -> 0.
#[no_mangle]
pub extern "C" fn ee_gamecpp_prefs_load_options(prefs: *mut c_void) -> c_long {
    read_settings(prefs as P + gobj::PREFS_SETTINGS, true);
    0
}

/// `Save`: every setting, then the game options -> the first failure (0 when every value was written).
#[no_mangle]
pub extern "C" fn ee_gamecpp_prefs_save(prefs: *mut c_void) -> c_long {
    let result = write_settings(prefs as P + gobj::PREFS_SETTINGS, false);
    let options = ee_gamecpp_prefs_save_options(prefs);
    if result != 0 {
        result
    } else {
        options
    }
}

/// `SaveGameOptions` -> the first failure, else 0.
#[no_mangle]
pub extern "C" fn ee_gamecpp_prefs_save_options(prefs: *mut c_void) -> c_long {
    write_settings(prefs as P + gobj::PREFS_SETTINGS, true)
}

/// `SetLastGameOptions`: a new random-map game's options (load type 0) kept as the last ones, then the save.
#[no_mangle]
pub extern "C" fn ee_gamecpp_prefs_set_last_options(prefs: *mut c_void, options: *const c_void) {
    // SAFETY: the options' plain members (the base)
    let load_type = unsafe { (*(options as P as *const EeAppOptionsFields)).mLoadType };
    if load_type == 0 {
        h(O::EpAssignLastOptions, prefs as P + gobj::PREFS_SETTINGS, &[options as usize as i64]);
    }
    ee_gamecpp_prefs_save(prefs);
}

/// `EEInitializeDefaultSettings`: the default player, host and Sierra names the "unknown" string, the WON password "".
#[no_mangle]
pub extern "C" fn ee_gamecpp_prefs_init_defaults() {
    let d = hp(O::EpDefaultSettings, 0, &[]);
    for off in [gobj::SET_HOST_GAME_NAME, gobj::SET_PLAYER_NAME, gobj::SET_SIERRA_USER_ID, gobj::SET_SIERRA_GAME_NAME] {
        h(O::EpUnknownName, d + off, &[]);
    }
    set_text(d + gobj::SET_WON_NET_PASSWORD, false, b"");
}

// The renderer's and the mixer's settings (ee_render gx, ee_audio mixer: their C ABI).
/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use std::ffi::{c_char, c_long, c_ulong};

    extern "C" {
        fn gx_set_render_percent(percent: c_ulong);
        fn gx_set_frame_limit(frames_per_second: c_long);
        fn gx_set_texture_filtering(mode: c_ulong);
        fn gx_set_brightness(gamma: f32);
    }
    pub fn render_percent(percent: c_ulong) {
        // SAFETY: plain values
        unsafe { gx_set_render_percent(percent) }
    }
    pub fn frame_limit(frames_per_second: c_long) {
        // SAFETY: as above
        unsafe { gx_set_frame_limit(frames_per_second) }
    }
    pub fn texture_filtering(mode: c_ulong) {
        // SAFETY: as above
        unsafe { gx_set_texture_filtering(mode) }
    }
    pub fn brightness(gamma: f32) {
        // SAFETY: as above
        unsafe { gx_set_brightness(gamma) }
    }
    pub fn master_volume(volume: f32) {
        ee_audio::mixer::ffi::SPIMSetMasterVolume(volume)
    }
    pub fn output_device(name: *const c_char) {
        // SAFETY: the settings' NUL-ended device name
        unsafe { ee_audio::mixer::ffi::SPIMSetOutputDevice(name) }
    }
}

/// The tests set nothing outside Rust: a call is a bug of the test.
#[cfg(test)]
mod sys {
    use std::ffi::{c_char, c_long, c_ulong};

    pub fn render_percent(_: c_ulong) {
        panic!("no renderer in the tests")
    }
    pub fn frame_limit(_: c_long) {
        panic!("no renderer in the tests")
    }
    pub fn texture_filtering(_: c_ulong) {
        panic!("no renderer in the tests")
    }
    pub fn brightness(_: f32) {
        panic!("no renderer in the tests")
    }
    pub fn master_volume(_: f32) {
        panic!("no mixer in the tests")
    }
    pub fn output_device(_: *const c_char) {
        panic!("no mixer in the tests")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::app::settings::EE_APP_SETTINGS_LAST_SCENARIO_FILE;

    #[test]
    fn the_string_rows_name_their_members() {
        assert_eq!(member(EE_APP_SETTINGS_PLAYER_NAME), Member { off: gobj::SET_PLAYER_NAME, wide: true });
        assert_eq!(member(EE_APP_SETTINGS_MAP_TYPE), Member { off: gobj::SET_MAP_TYPE, wide: true });
        assert_eq!(member(EE_APP_SETTINGS_LAST_GAME_FILE), Member { off: gobj::SET_LAST_GAME_FILE, wide: false });
        assert_eq!(
            member(EE_APP_SETTINGS_LAST_SCENARIO_FILE),
            Member { off: gobj::SET_LAST_SCENARIO_FILE, wide: false }
        );
        assert!(member(EE_APP_SETTINGS_LAST_AUTO_SAVE).wide && member(EE_APP_SETTINGS_LAST_QUICK_SAVE).wide);
        assert!(member(EE_APP_SETTINGS_HOST_GAME_NAME).wide);
    }

    #[test]
    fn the_texture_resolutions_pick_the_pack() {
        assert_eq!(texture_pack(2), (Some(c"textures-4x"), 2, 1));
        assert_eq!(texture_pack(4), (Some(c"textures-4x"), 2, 0));
        assert_eq!(texture_pack(0), (None, 0, 0));
        assert_eq!(texture_pack(3), (None, 0, 0));
    }
}
