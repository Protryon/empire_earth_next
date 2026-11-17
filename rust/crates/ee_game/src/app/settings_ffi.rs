//! The settings' C ABI (include/rust/ee_app.h): config.yaml through ee_plat_core's `PlatConfig*` (what `UConfig`
//! called), the C++ string members through one callback.

use crate::app::game_options::EeAppOptionsFields;
use crate::app::settings::{self, Config, EeAppSettingsFields, Records, Strings};
use ee_base::plat_core::config::{
    PlatConfigGetInt, PlatConfigGetString, PlatConfigSetBool, PlatConfigSetInt, PlatConfigSetString,
};
use std::ffi::{c_void, CString};
use std::os::raw::{c_char, c_long};

/// The string host: `(ctx, op, id, buffer, size) -> a length` (`EE_APP_SETTINGS_STRING_*`).
pub type EeAppSettingsStringFn = unsafe extern "C" fn(*mut c_void, i32, u32, *mut c_char, u32) -> u32;

/// the member's text's length in bytes
pub const EE_APP_SETTINGS_STRING_LENGTH: i32 = 1;
/// the member's text into the buffer (size: the length + 1, NUL ended)
pub const EE_APP_SETTINGS_STRING_GET: i32 = 2;
/// the member set to the buffer's `size` bytes
pub const EE_APP_SETTINGS_STRING_SET: i32 = 3;
/// the member set to the defaults' member
pub const EE_APP_SETTINGS_STRING_DEFAULT: i32 = 4;

/// `UConfig::GetValue(UWideString)`'s buffer.
const STRING_BUFFER: usize = 1024;

struct PlatConfig;

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

struct Host {
    ctx: *mut c_void,
    call: EeAppSettingsStringFn,
}

impl Strings for Host {
    fn get(&mut self, id: u32) -> Vec<u8> {
        // SAFETY: the C++ passed a live context and its callback for the entry's duration
        let n = unsafe { (self.call)(self.ctx, EE_APP_SETTINGS_STRING_LENGTH, id, std::ptr::null_mut(), 0) } as usize;
        let mut buffer = vec![0 as c_char; n + 1];
        // SAFETY: as above; the buffer holds n + 1 bytes
        unsafe { (self.call)(self.ctx, EE_APP_SETTINGS_STRING_GET, id, buffer.as_mut_ptr(), (n + 1) as u32) };
        buffer[..n].iter().map(|&b| b as u8).collect()
    }
    fn set(&mut self, id: u32, value: &[u8]) {
        let mut buffer: Vec<c_char> = value.iter().map(|&b| b as c_char).collect();
        buffer.push(0);
        // SAFETY: as get
        unsafe { (self.call)(self.ctx, EE_APP_SETTINGS_STRING_SET, id, buffer.as_mut_ptr(), value.len() as u32) };
    }
    fn set_default(&mut self, id: u32) {
        // SAFETY: as get
        unsafe { (self.call)(self.ctx, EE_APP_SETTINGS_STRING_DEFAULT, id, std::ptr::null_mut(), 0) };
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

/// `EEReadSettings`: every row of the settings (or, `game_options`, of the last game options) read from config.yaml,
/// a missing or out-of-range value the defaults' (written back).
///
/// # Safety
/// live records (the defaults' only read); `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_settings_read(
    settings: *mut EeAppSettingsFields,
    options: *mut EeAppOptionsFields,
    default_settings: *mut EeAppSettingsFields,
    default_options: *mut EeAppOptionsFields,
    game_options: bool,
    ctx: *mut c_void,
    call: EeAppSettingsStringFn,
) {
    let rec = Records { settings, options };
    let defaults = Records { settings: default_settings, options: default_options };
    settings::read(&mut PlatConfig, &mut Host { ctx, call }, rec, defaults, game_options);
}

/// `EEWriteSettings`: every row written to config.yaml; the first failure, else 0.
///
/// # Safety
/// as [`ee_app_settings_read`]
#[no_mangle]
pub unsafe extern "C" fn ee_app_settings_write(
    settings: *mut EeAppSettingsFields,
    options: *mut EeAppOptionsFields,
    game_options: bool,
    ctx: *mut c_void,
    call: EeAppSettingsStringFn,
) -> i32 {
    let rec = Records { settings, options };
    settings::write(&mut PlatConfig, &mut Host { ctx, call }, rec, game_options)
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
