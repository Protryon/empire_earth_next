//! `extern "C"` for the game settings form's decisions (`settings.rs`; `UI Events/EEUIEGameSettingsEvents.cpp`):
//! plain values in and out, no host calls.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong};

use crate::settings::{self, EeScreensGraphics, EeScreensGraphicsSettings, EeScreensGraphicsShow, EeScreensPreset};

/// `GetGraphicsQuality`: the preset (0..4) the controls match, else 5 (Custom).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_graphics_quality(controls: *const EeScreensGraphics) -> c_ulong {
    settings::graphics_quality(&*controls)
}

/// `SetGraphicsQuality`: whether `quality` is a preset (`out` then its plan).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_quality_preset(quality: c_ulong, out: *mut EeScreensPreset) -> bool {
    match settings::quality_preset(quality) {
        Some(p) => {
            out.write(p);
            true
        }
        None => false,
    }
}

/// The quality radio button (0..4, 5 Custom) a quality checks.
#[no_mangle]
pub extern "C" fn ee_screens_quality_radio(quality: c_ulong) -> c_ulong {
    settings::quality_radio(quality)
}

/// `ShowGraphics`' plan for the settings.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_graphics_show(
    settings: *const EeScreensGraphicsSettings,
    out: *mut EeScreensGraphicsShow,
) {
    out.write(settings::graphics_show(&*settings));
}

/// `PortSelect`: the line of value table `table` nearest `value` (0 for an unknown table).
#[no_mangle]
pub extern "C" fn ee_screens_settings_nearest(table: u32, value: c_long) -> c_ulong {
    settings::table(table).map_or(0, |t| settings::nearest_line(t, value))
}

/// `PortRead`: whether line `selection` of value table `table` exists (`out` then its value).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_settings_value(table: u32, selection: c_ulong, out: *mut c_long) -> bool {
    match settings::table(table).and_then(|t| settings::line_value(t, selection)) {
        Some(v) => {
            out.write(v);
            true
        }
        None => false,
    }
}
