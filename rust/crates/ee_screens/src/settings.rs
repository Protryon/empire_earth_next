//! The game settings form's decisions (form 5, `UI Events/EEUIEGameSettingsEvents.cpp`): the graphics quality presets
//! (which preset the detail controls match, what a preset sets), the video tab's controls from the settings
//! (`ShowGraphics`), and the port's combo boxes, whose lines are values (`PortSelect`, `PortRead`).
//!
//! The C++ keeps the controls: it reads the check boxes and radio buttons into [`EeScreensGraphics`], and makes the
//! `SetChecked`/`Check`/`SetCurrentSelection` calls these plans name, in its own order.

use core::ffi::{c_long, c_ulong};

/// The graphics check boxes in the order `GetGraphicsQuality` tests them (the form's control indices): 82 the
/// trilinear filter, 83 unit lighting, 84 the full-resolution textures, 85 v-sync, 86 shadows, 87 clouds, 110 the
/// battlefield ambients, 88 the weather effects, 105 full-screen anti-aliasing.
#[cfg_attr(not(test), allow(dead_code))]
pub(crate) const CHECK_CONTROLS: [c_ulong; 9] = [82, 83, 84, 85, 86, 87, 110, 88, 105];

/// The presets' detail controls: the model detail's high, medium and low radio buttons, the effects' likewise.
#[cfg_attr(not(test), allow(dead_code))]
pub(crate) const MODEL_RADIOS: [c_ulong; 3] = [75, 76, 77];
#[cfg_attr(not(test), allow(dead_code))]
pub(crate) const EFFECT_RADIOS: [c_ulong; 3] = [79, 80, 81];

/// `GetGraphicsQuality`'s answer when no preset matches: Custom.
pub const EE_SCREENS_QUALITY_CUSTOM: c_ulong = 5;

/// The presets 0 (lowest) to 4 (highest): each check box's state (`kQualityCheck82` ...), in [`CHECK_CONTROLS`]' order.
const QUALITY_CHECKS: [[bool; 9]; 5] = [
    [false, false, false, true, false, false, false, false, false],
    [false, false, false, true, false, false, false, false, false],
    [false, false, false, true, true, true, true, false, false],
    [false, true, true, true, true, true, true, false, false],
    [true, true, true, true, true, true, true, false, false],
];
/// The presets' model detail and effects detail (0 low, 1 medium, 2 high: `kQualityRadios1`, `kQualityRadios2`).
const QUALITY_MODEL: [c_long; 5] = [0, 1, 1, 2, 2];
const QUALITY_EFFECT: [c_long; 5] = [0, 1, 1, 2, 2];

/// The graphics controls' states as `GetGraphicsQuality` reads them.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensGraphics {
    /// The check boxes, in [`CHECK_CONTROLS`]' order.
    pub checks: [bool; 9],
    /// The model detail radio buttons: high, medium, low ([`MODEL_RADIOS`]).
    pub model: [bool; 3],
    /// The effects detail radio buttons: high, medium, low ([`EFFECT_RADIOS`]).
    pub effect: [bool; 3],
}

/// Whether the checked detail radios agree with a preset's level (2 high, 1 medium, 0 low): every checked radio's
/// level must be the preset's (none checked agrees with any).
fn radios_agree(radios: &[bool; 3], level: c_long) -> bool {
    (!radios[0] || level == 2) && (!radios[1] || level == 1) && (!radios[2] || level == 0)
}

/// `GetGraphicsQuality`: the first preset (0..4) the controls match, else [`EE_SCREENS_QUALITY_CUSTOM`].
pub fn graphics_quality(g: &EeScreensGraphics) -> c_ulong {
    for q in 0..5 {
        if g.checks == QUALITY_CHECKS[q]
            && radios_agree(&g.model, QUALITY_MODEL[q])
            && radios_agree(&g.effect, QUALITY_EFFECT[q])
        {
            return q as c_ulong;
        }
    }
    EE_SCREENS_QUALITY_CUSTOM
}

/// What `SetGraphicsQuality` sets for a preset: the check boxes ([`CHECK_CONTROLS`]' order: the C++ calls
/// `SetChecked` in that order), then the model and the effects detail radio to check (0 low, 1 medium, 2 high).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensPreset {
    pub checks: [bool; 9],
    pub model: c_long,
    pub effect: c_long,
}

/// `SetGraphicsQuality`'s plan: none for a quality past the presets (Custom: the controls stay).
pub fn quality_preset(quality: c_ulong) -> Option<EeScreensPreset> {
    let q = usize::try_from(quality).ok().filter(|&q| q < 5)?;
    Some(EeScreensPreset { checks: QUALITY_CHECKS[q], model: QUALITY_MODEL[q], effect: QUALITY_EFFECT[q] })
}

/// The quality radio button a quality checks (`ShowGraphics`, `AdvancedGraphicsSettingsChanged`): the preset's own,
/// 0..4, or 5, Custom (which those two also show).
pub fn quality_radio(quality: c_ulong) -> c_ulong {
    quality.min(EE_SCREENS_QUALITY_CUSTOM)
}

/// A detail setting's radio (`ShowGraphics`' switches): 1 medium, 0 low, any other value high (2).
pub fn detail_level(value: c_long) -> c_long {
    match value {
        0 => 0,
        1 => 1,
        _ => 2,
    }
}

/// The texture resolution combo box's line for the setting (`ShowGraphics`): 4 the third line, 2 the second, any
/// other value the first (Original).
pub fn texture_resolution_line(value: c_long) -> c_ulong {
    match value {
        4 => 2,
        2 => 1,
        _ => 0,
    }
}

/// The settings `ShowGraphics` puts on the video tab's graphics controls.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensGraphicsSettings {
    pub trilinear_filter: bool,
    pub unit_lighting: bool,
    pub shadows: bool,
    pub clouds: bool,
    pub battlefield_ambients: bool,
    pub weather_effects: bool,
    pub anti_aliasing: bool,
    pub wait_for_vsync: bool,
    pub mip_levels_to_drop: c_long,
    pub terrain_mip_levels_to_drop: c_long,
    pub texture_resolution: c_long,
    pub model_detail: c_long,
    pub effect_detail: c_long,
}

/// `ShowGraphics`' plan: the check boxes' states in [`CHECK_CONTROLS`]' order (the C++ sets 82, 83, 84, the texture
/// resolution line, 86, 87, 110, 88, 105, the detail radios, then 85, the v-sync box, last), the texture resolution
/// line and the detail radios (0 low, 1 medium, 2 high).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensGraphicsShow {
    pub checks: [bool; 9],
    pub texture_line: c_ulong,
    pub model: c_long,
    pub effect: c_long,
}

pub fn graphics_show(s: &EeScreensGraphicsSettings) -> EeScreensGraphicsShow {
    EeScreensGraphicsShow {
        checks: [
            s.trilinear_filter,
            s.unit_lighting,
            s.mip_levels_to_drop == 0 && s.terrain_mip_levels_to_drop == 0,
            s.wait_for_vsync,
            s.shadows,
            s.clouds,
            s.battlefield_ambients,
            s.weather_effects,
            s.anti_aliasing,
        ],
        texture_line: texture_resolution_line(s.texture_resolution),
        model: detail_level(s.model_detail),
        effect: detail_level(s.effect_detail),
    }
}

/// The port's combo boxes whose lines are values (`kWindowModes` ...), by the number the C++ names them with.
pub const EE_SCREENS_TABLE_WINDOW_MODE: u32 = 0;
pub const EE_SCREENS_TABLE_UI_SCALE: u32 = 1;
pub const EE_SCREENS_TABLE_RENDER_RESOLUTION: u32 = 2;
pub const EE_SCREENS_TABLE_FRAME_RATE: u32 = 3;
pub const EE_SCREENS_TABLE_TEXTURE_FILTERING: u32 = 4;
pub const EE_SCREENS_TABLE_BRIGHTNESS: u32 = 5;
pub const EE_SCREENS_TABLE_ZOOM_OUT: u32 = 6;

const WINDOW_MODES: &[c_long] = &[0, 1];
const UI_SCALES: &[c_long] = &[0, 100, 125, 150, 175, 200, 250, 300];
const RENDER_RESOLUTIONS: &[c_long] = &[100, 75, 67, 50];
const FRAME_RATES: &[c_long] = &[0, 60, 120, -1];
const TEXTURE_FILTERINGS: &[c_long] = &[0, 1, 2];
const BRIGHTNESSES: &[c_long] = &[70, 80, 90, 100, 110, 120, 130];
const ZOOM_OUTS: &[c_long] = &[100, 125, 150, 175, 200, 225, 250, 275, 300];

/// A table's lines' values; none for an unknown table.
pub fn table(kind: u32) -> Option<&'static [c_long]> {
    Some(match kind {
        EE_SCREENS_TABLE_WINDOW_MODE => WINDOW_MODES,
        EE_SCREENS_TABLE_UI_SCALE => UI_SCALES,
        EE_SCREENS_TABLE_RENDER_RESOLUTION => RENDER_RESOLUTIONS,
        EE_SCREENS_TABLE_FRAME_RATE => FRAME_RATES,
        EE_SCREENS_TABLE_TEXTURE_FILTERING => TEXTURE_FILTERINGS,
        EE_SCREENS_TABLE_BRIGHTNESS => BRIGHTNESSES,
        EE_SCREENS_TABLE_ZOOM_OUT => ZOOM_OUTS,
        _ => return None,
    })
}

/// `PortSelect`: the line whose value is nearest the setting, the first of equally near ones (`labs` of the
/// wrapping `long` difference, as the C++ under `-fwrapv`: `labs(LONG_MIN)` stays negative, so nearest).
pub fn nearest_line(values: &[c_long], value: c_long) -> c_ulong {
    let distance = |v: c_long| v.wrapping_sub(value).wrapping_abs();
    let mut best = 0;
    for i in 1..values.len() {
        if distance(values[i]) < distance(values[best]) {
            best = i;
        }
    }
    best as c_ulong
}

/// `PortRead`: the selected line's value; none past the lines (the setting stays).
pub fn line_value(values: &[c_long], selection: c_ulong) -> Option<c_long> {
    usize::try_from(selection).ok().and_then(|s| values.get(s)).copied()
}

/// The C++'s float -> `long` conversion on the Windows build (`cvttss2si`): toward zero, and the "integer
/// indefinite" `LONG_MIN` for NaN and anything past the type (where arm64 would saturate: one answer everywhere).
pub fn float_to_long(v: f32) -> c_long {
    let limit = -(c_long::MIN as f32); // 2^31 (2^63 on a 64-bit long), exact
    if v >= -limit && v < limit {
        v as c_long
    } else {
        c_long::MIN
    }
}

/// The C++'s float -> `unsigned long` conversion (clang on x86-64: `cvttss2si` to 64 bits, the low bits kept): a
/// negative value wraps; NaN and anything past 2^63 give 0x8000000000000000's low bits.
pub fn float_to_ulong(v: f32) -> c_ulong {
    let limit = 9_223_372_036_854_775_808.0f32; // 2^63
    let wide = if v >= -limit && v < limit { v as i64 } else { i64::MIN };
    wide as c_ulong
}

/// A slider's percent as a factor (`GetCurrentRangeValue() * 0.01f`).
pub fn slider_factor(value: c_long) -> f32 {
    value as f32 * 0.01f32
}

/// A factor as a slider's percent (`(long)(inValue * 100.0f)`: the audio and interface tabs).
pub fn slider_percent(value: f32) -> c_long {
    float_to_long(value * 100.0f32)
}

/// `ApplySettings`' floor on the mouse sensitivity.
pub const EE_SCREENS_MINIMUM_MOUSE_SENSITIVITY: f32 = 0.01;
/// `ApplySettings`' floor on the sound channels.
pub const EE_SCREENS_MINIMUM_CHANNELS: c_ulong = 8;

/// The controls `ApplySettings` reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensApplyControls {
    /// 13: show blood.
    pub show_blood: bool,
    /// 82, 83, 84: the trilinear filter, unit lighting, full-resolution textures.
    pub trilinear: bool,
    pub unit_lighting: bool,
    pub full_textures: bool,
    /// 111, the port's texture resolution combo box: whether it is there with a selection, and the selection.
    pub texture_selected: bool,
    pub texture_line: c_ulong,
    /// 86, 87, 110, 88: shadows, clouds, battlefield ambients, weather effects.
    pub shadows: bool,
    pub clouds: bool,
    pub battlefield_ambients: bool,
    pub weather_effects: bool,
    /// The model and the effects detail radios: high, medium, low (75..77, 79..81).
    pub model: [bool; 3],
    pub effect: [bool; 3],
    /// 66: the ambient sounds.
    pub ambient_sounds: bool,
    /// 39: the voices slider (percent of the mixer's channels).
    pub voices: c_long,
    /// The mixer's channels (`GetMaxChannels`).
    pub max_channels: c_ulong,
    /// 29 music off, 27 reverb on, 28 reverb off.
    pub music_off: bool,
    pub reverb_on: bool,
    pub reverb_off: bool,
    /// 56, 49, 62: the mouse sensitivity and the side and grab scroll speeds (percent).
    pub mouse_sensitivity: c_long,
    pub side_scroll: c_long,
    pub grab_scroll: c_long,
    /// 53: the right-handed mouse.
    pub right_handed: bool,
    /// 91, 94: the autosave minutes and count.
    pub autosave_minutes: c_long,
    pub autosave_count: c_long,
}

/// The settings `ApplySettings` writes, in and out: a detail level, the reverb and the texture resolution stay when
/// no control decides them.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensApplied {
    pub show_blood: bool,
    pub trilinear: bool,
    pub unit_lighting: bool,
    pub mip_levels_to_drop: c_long,
    pub terrain_mip_levels_to_drop: c_long,
    /// Whether the texture resolution is set (and `EESetTextureResolution` called), and its value.
    pub set_texture_resolution: bool,
    pub texture_resolution: c_long,
    pub shadows: bool,
    pub clouds: bool,
    pub battlefield_ambients: bool,
    pub weather_effects: bool,
    pub model_detail: c_long,
    pub effect_detail: c_long,
    pub ambient_sounds: bool,
    /// The voices factor (`mMaxNumberOfSounds`) and the channels it leaves (`SetMaxChannels`).
    pub max_number_of_sounds: f32,
    pub channels: c_ulong,
    pub music_enabled: bool,
    pub reverb: bool,
    pub mouse_sensitivity: f32,
    pub side_scroll: f32,
    pub grab_scroll: f32,
    pub right_handed: bool,
    pub autosave_ms: c_ulong,
    pub autosaves: c_ulong,
}

/// A detail radio group (high, medium, low) as `ApplySettings` reads it: low first, then medium, then high; none
/// checked keeps the setting.
fn applied_detail(radios: &[bool; 3], current: c_long) -> c_long {
    if radios[2] {
        0
    } else if radios[1] {
        1
    } else if radios[0] {
        2
    } else {
        current
    }
}

/// The texture resolution combo box's line as a setting (0 Original, 1 2x, 2 4x); none past the lines.
pub fn texture_resolution_of_line(line: c_ulong) -> Option<c_long> {
    match line {
        0 => Some(0),
        1 => Some(2),
        2 => Some(4),
        _ => None,
    }
}

/// `ApplySettings`' values from the form's controls (the rasterizer, the port's combo boxes and the volumes the
/// engine holds stay the C++'s).
pub fn apply(c: &EeScreensApplyControls, io: &mut EeScreensApplied) {
    io.show_blood = c.show_blood;
    io.trilinear = c.trilinear;
    io.unit_lighting = c.unit_lighting;
    let drop = if c.full_textures { 0 } else { 1 };
    io.mip_levels_to_drop = drop;
    io.terrain_mip_levels_to_drop = drop;
    io.set_texture_resolution = false;
    if c.texture_selected {
        if let Some(r) = texture_resolution_of_line(c.texture_line) {
            io.set_texture_resolution = true;
            io.texture_resolution = r;
        }
    }
    io.shadows = c.shadows;
    io.clouds = c.clouds;
    io.battlefield_ambients = c.battlefield_ambients;
    io.weather_effects = c.weather_effects;
    io.model_detail = applied_detail(&c.model, io.model_detail);
    io.effect_detail = applied_detail(&c.effect, io.effect_detail);
    io.ambient_sounds = c.ambient_sounds;
    io.max_number_of_sounds = slider_factor(c.voices);
    io.channels = float_to_ulong(c.max_channels as f32 * io.max_number_of_sounds).max(EE_SCREENS_MINIMUM_CHANNELS);
    io.music_enabled = !c.music_off;
    if c.reverb_on {
        io.reverb = true;
    } else if c.reverb_off {
        io.reverb = false;
    }
    let sensitivity = slider_factor(c.mouse_sensitivity);
    io.mouse_sensitivity = if sensitivity < EE_SCREENS_MINIMUM_MOUSE_SENSITIVITY {
        EE_SCREENS_MINIMUM_MOUSE_SENSITIVITY
    } else {
        sensitivity
    };
    io.side_scroll = slider_factor(c.side_scroll);
    io.grab_scroll = slider_factor(c.grab_scroll);
    io.right_handed = c.right_handed;
    io.autosave_ms = c.autosave_minutes.wrapping_mul(60_000) as c_ulong;
    io.autosaves = c.autosave_count as c_ulong;
}

/// The audio tab's settings as `ShowAudio` reads them (with the defaults' volumes: a volume equal to its default is
/// put into effect, as Restore Default does).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensAudioSettings {
    pub music_volume: f32,
    pub default_music_volume: f32,
    pub sound_volume: f32,
    pub default_sound_volume: f32,
    pub max_number_of_sounds: f32,
    pub reverb: bool,
    pub music_enabled: bool,
}

/// `ShowAudio`'s plan: whether to put the music and the sound volume into effect, the voices slider, the reverb
/// radio to check (27 on, else 28 off) and whether to check music off (29). The music and sound sliders show the
/// engine's volumes afterwards ([`slider_percent`]).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensAudioShow {
    pub set_music_volume: bool,
    pub set_sound_volume: bool,
    pub voices: c_long,
    pub reverb_on: bool,
    pub music_off: bool,
}

pub fn audio_show(s: &EeScreensAudioSettings) -> EeScreensAudioShow {
    EeScreensAudioShow {
        set_music_volume: s.music_volume == s.default_music_volume,
        set_sound_volume: s.sound_volume == s.default_sound_volume,
        voices: slider_percent(s.max_number_of_sounds),
        reverb_on: s.reverb,
        music_off: !s.music_enabled,
    }
}

/// The interface tab's settings as `ShowInterface` reads them.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensInterfaceSettings {
    pub mouse_sensitivity: f32,
    pub side_scroll: f32,
    pub grab_scroll: f32,
    pub network_adapter: c_long,
    /// The adapters the protocol lists (the box holds Default, then them).
    pub adapters: c_ulong,
    pub right_handed: bool,
    pub autosave_ms: c_ulong,
    pub autosaves: c_ulong,
}

/// `ShowInterface`'s plan: the three sliders, the adapter box's line (an adapter past the list: Default), the mouse
/// hand (`SetMouseLeftHanded`; 53 or 54 checked) and the autosave sliders' and labels' values.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensInterfaceShow {
    pub mouse_sensitivity: c_long,
    pub side_scroll: c_long,
    pub grab_scroll: c_long,
    pub adapter_line: c_ulong,
    pub left_handed: bool,
    pub autosave_minutes: c_long,
    pub autosaves: c_long,
}

pub fn interface_show(s: &EeScreensInterfaceSettings) -> EeScreensInterfaceShow {
    let adapter = s.network_adapter as c_ulong;
    EeScreensInterfaceShow {
        mouse_sensitivity: slider_percent(s.mouse_sensitivity),
        side_scroll: slider_percent(s.side_scroll),
        grab_scroll: slider_percent(s.grab_scroll),
        adapter_line: if adapter > s.adapters { 0 } else { adapter },
        left_handed: !s.right_handed,
        autosave_minutes: (s.autosave_ms / 60_000) as c_long,
        autosaves: s.autosaves as c_long,
    }
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod golden2;
#[cfg(test)]
mod tests;
