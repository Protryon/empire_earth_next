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

/// `UConfig::GetValue(block, key, UWideString&)`: the key's text (at most 1023 bytes) handed to `assign` (the C++
/// string's assignment) and kErrorOK, or kErrorNotFound (the string left) when the key holds none.
///
/// # Safety
/// As `PlatConfigGetString`; `assign` takes `target` and a NUL-terminated text.
#[no_mangle]
pub unsafe extern "C" fn ee_uconfig_get_text(
    block: *const c_char,
    key: *const c_char,
    assign: unsafe extern "C" fn(*mut std::ffi::c_void, *const c_char),
    target: *mut std::ffi::c_void,
) -> std::ffi::c_long {
    let mut data = [0 as c_char; 1024];
    if crate::plat_core::config::PlatConfigGetString(block, key, data.as_mut_ptr(), data.len() as u32) == 0 {
        return ESError::kErrorNotFound as std::ffi::c_long;
    }
    assign(target, data.as_ptr());
    ESError::kErrorOK as std::ffi::c_long
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn assign(target: *mut std::ffi::c_void, text: *const c_char) {
        *target.cast::<Vec<u8>>() = std::ffi::CStr::from_ptr(text).to_bytes().to_vec();
    }

    #[test]
    fn a_missing_text_leaves_the_string() {
        let mut out = b"before".to_vec();
        let target = (&raw mut out).cast();
        unsafe {
            let block = c"lleui1-uconfig-test".as_ptr();
            assert_eq!(ee_uconfig_get_text(block, c"absent".as_ptr(), assign, target), ESError::kErrorNotFound as _);
            assert_eq!(out, b"before");
            assert_eq!(ee_uconfig_get_text(block, std::ptr::null(), assign, target), ESError::kErrorNotFound as _);
        }
    }
}
