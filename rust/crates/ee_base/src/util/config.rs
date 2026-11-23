//! `UConfig`'s reads (lle/UConfig.h) over the settings file (`PlatConfig*`, plat_core config.rs).

use std::ffi::c_char;

use crate::lle_enums::ESError;
use crate::plat_core::config::PlatConfigGetInt;

/// `UConfig::GetValue(block, key, long&)`: kErrorOK with the value, kErrorNotFound (the value left) when the key
/// holds no integer.
///
/// # Safety
/// As `PlatConfigGetInt`; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_uconfig_get_long(
    block: *const c_char,
    key: *const c_char,
    out: *mut std::ffi::c_long,
) -> std::ffi::c_long {
    let mut value = 0;
    if PlatConfigGetInt(block, key, &mut value) == 0 {
        return ESError::kErrorNotFound as std::ffi::c_long;
    }
    *out = value as std::ffi::c_long;
    ESError::kErrorOK as std::ffi::c_long
}

/// `UConfig::GetFlag`: kErrorOK with whether the integer is nonzero, kErrorNotFound (the flag left) otherwise.
///
/// # Safety
/// As `PlatConfigGetInt`; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_uconfig_get_flag(
    block: *const c_char,
    key: *const c_char,
    out: *mut bool,
) -> std::ffi::c_long {
    let mut value = 0;
    if PlatConfigGetInt(block, key, &mut value) == 0 {
        return ESError::kErrorNotFound as std::ffi::c_long;
    }
    *out = value != 0;
    ESError::kErrorOK as std::ffi::c_long
}
