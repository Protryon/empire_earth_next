//! The settings' C ABI (include/rust/ee_app.h): config.yaml through ee_plat_core's `PlatConfig*` (what `UConfig`
//! called); the rows' reads and writes are gamecpp preferences.rs's.

use crate::app::game_options::EeAppOptionsFields;
use crate::app::settings::{self, Config, EeAppSettingsFields};
use ee_base::plat_core::config::{
    PlatConfigGetInt, PlatConfigGetString, PlatConfigSetBool, PlatConfigSetInt, PlatConfigSetString,
};
use std::ffi::CString;
use std::os::raw::{c_char, c_long};

/// `UConfig::GetValue(UWideString)`'s buffer.
const STRING_BUFFER: usize = 1024;

/// config.yaml through ee_plat_core (gamecpp preferences.rs reads and writes the rows over it).
pub(crate) struct PlatConfig;

fn c(text: &str) -> CString {
    CString::new(text).expect("a block or key without NUL")
}

impl Config for PlatConfig {
    fn get_int(&mut self, block: &str, key: &str) -> Option<i32> {
        let mut v = 0;
        // SAFETY: NUL-ended strings and a live out value
        (unsafe { PlatConfigGetInt(c(block).as_ptr(), c(key).as_ptr(), &mut v) } != 0).then_some(v)
    }
    fn get_string(&mut self, block: &str, key: &str) -> Option<Vec<u8>> {
        let mut buffer = [0 as c_char; STRING_BUFFER];
        // SAFETY: as get_int; the buffer's size
        let found = unsafe {
            PlatConfigGetString(c(block).as_ptr(), c(key).as_ptr(), buffer.as_mut_ptr(), STRING_BUFFER as u32)
        } != 0;
        found.then(|| buffer.iter().take_while(|&&b| b != 0).map(|&b| b as u8).collect())
    }
    fn set_int(&mut self, block: &str, key: &str, value: i32) -> bool {
        // SAFETY: NUL-ended strings
        unsafe { PlatConfigSetInt(c(block).as_ptr(), c(key).as_ptr(), value) != 0 }
    }
    fn set_bool(&mut self, block: &str, key: &str, value: bool) -> bool {
        // SAFETY: as set_int
        unsafe { PlatConfigSetBool(c(block).as_ptr(), c(key).as_ptr(), i32::from(value)) != 0 }
    }
    fn set_string(&mut self, block: &str, key: &str, value: &[u8]) -> bool {
        let n = value.iter().position(|&b| b == 0).unwrap_or(value.len());
        let value = CString::new(&value[..n]).expect("no NUL");
        // SAFETY: as set_int
        unsafe { PlatConfigSetString(c(block).as_ptr(), c(key).as_ptr(), value.as_ptr()) != 0 }
    }
}

/// `EEGameSettings()`'s plain members.
///
/// # Safety
/// `out` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_app_settings_default(out: *mut EeAppSettingsFields) {
    unsafe { *out = EeAppSettingsFields::default() };
}

/// The last game options' defaults (`EEDefaultSettings`'s literals, but the map type).
///
/// # Safety
/// `out` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_app_settings_last_options_default(out: *mut EeAppOptionsFields, epochs: c_long) {
    unsafe { settings::last_options_default(&mut *out, epochs) };
}

/// `Load`'s rest after the rows (settings.rs `after_load`); true when the texture resolution was replaced and is to be
/// written back.
///
/// # Safety
/// live records.
#[no_mangle]
pub unsafe extern "C" fn ee_app_settings_after_load(
    settings: *mut EeAppSettingsFields,
    defaults: *const EeAppSettingsFields,
    korean: bool,
) -> bool {
    unsafe { settings::after_load(&mut *settings, &*defaults, korean) }
}
