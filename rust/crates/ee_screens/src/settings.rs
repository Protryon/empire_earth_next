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

#[cfg(test)]
mod golden;
#[cfg(test)]
mod tests;
