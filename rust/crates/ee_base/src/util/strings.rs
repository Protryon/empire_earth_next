//! The game's language strings (data lane round 1): the table `UResourceFile::GetString` reads, generated from
//! Language.rc by `tools/lang_strings.py` into `language_strings.rs` (it replaced the generated
//! `LanguageStrings.cpp`). Each string is UTF-8 (decision D6) and ends in a NUL, so the C++ caller takes the pointer
//! as a C string; an empty rc string is missing, as LoadStringA's 0 was.

use crate::util::language_strings::{LANGUAGE, STRINGS};

/// The text of string `id` (without its closing NUL), or `None` when the table has no such string.
pub fn resource_string(id: u16) -> Option<&'static str> {
    STRINGS.binary_search_by_key(&id, |&(key, _)| key).ok().map(|i| &STRINGS[i].1[..STRINGS[i].1.len() - 1])
}

/// The strings' Windows LANGID, or 0 when the table is empty (the caller then asks the system).
#[no_mangle]
pub extern "C" fn ee_util_resource_strings_language() -> u16 {
    if STRINGS.is_empty() {
        0
    } else {
        LANGUAGE
    }
}

/// String `id` as a NUL-terminated UTF-8 C string that lives for the whole run, or null when it is missing.
#[no_mangle]
pub extern "C" fn ee_util_resource_string(id: u16) -> *const u8 {
    match STRINGS.binary_search_by_key(&id, |&(key, _)| key) {
        Ok(i) => STRINGS[i].1.as_ptr(),
        Err(_) => core::ptr::null(),
    }
}

/// `UResourceFile::GetString`'s text of string `id`: at most 4,499 bytes (its 4,500-byte buffer), `None` when missing.
pub fn resource_string_for_get(id: u16) -> Option<Vec<u8>> {
    let text = resource_string(id)?.as_bytes();
    let mut out = text[..text.len().min(4499)].to_vec();
    out.push(0);
    Some(out)
}

/// `UResourceFile::GetString`: string `id` handed to `assign` (the C++ `UWideString` assignment) and kErrorOK, or
/// kErrorNativeError with native error 1814 (ERROR_RESOURCE_NAME_NOT_FOUND, as LoadStringA left) when it is missing.
///
/// # Safety
/// `assign` takes `target` and a NUL-terminated text.
#[no_mangle]
pub unsafe extern "C" fn ee_util_resource_string_get(
    id: u16,
    assign: unsafe extern "C" fn(*mut core::ffi::c_void, *const core::ffi::c_char),
    target: *mut core::ffi::c_void,
) -> core::ffi::c_long {
    match resource_string_for_get(id) {
        Some(text) => {
            // SAFETY: the function's contract
            unsafe { assign(target, text.as_ptr().cast()) };
            crate::lle_enums::ESError::kErrorOK as core::ffi::c_long
        }
        None => {
            crate::error::ee_error_record_native(1814);
            crate::lle_enums::ESError::kErrorNativeError as core::ffi::c_long
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// FNV-1a 64 over every row (the id little-endian, the bytes, the NUL), as the generated C++ table gave it
    /// (`LanguageStrings.cpp` at 307aea23, decoded from its C literals: 3,773 strings).
    #[test]
    fn table_equals_the_cpp_table() {
        let mut h: u64 = 0xcbf2_9ce4_8422_2325;
        for &(id, text) in STRINGS.iter() {
            for &b in id.to_le_bytes().iter().chain(text.as_bytes()) {
                h = (h ^ u64::from(b)).wrapping_mul(0x0100_0000_01b3);
            }
        }
        assert_eq!(STRINGS.len(), 3773);
        assert_eq!(h, 0x69d3_7468_17dd_a2ff);
    }

    #[test]
    fn get_string_text_is_cut_at_4499_bytes() {
        let &(id, text) = STRINGS.iter().max_by_key(|(_, text)| text.len()).unwrap();
        let got = resource_string_for_get(id).unwrap();
        assert_eq!(got.len(), (text.len() - 1).min(4499) + 1);
        assert_eq!(got.last(), Some(&0));
        assert_eq!(&got[..got.len() - 1], &text.as_bytes()[..got.len() - 1]);
        let missing = (0..=u16::MAX).find(|&i| resource_string(i).is_none()).unwrap();
        assert!(resource_string_for_get(missing).is_none());
    }

    #[test]
    fn sorted_terminated_and_nonempty() {
        for w in STRINGS.windows(2) {
            assert!(w[0].0 < w[1].0);
        }
        for &(_, text) in STRINGS.iter() {
            assert!(text.len() > 1 && text.ends_with('\0') && !text[..text.len() - 1].contains('\0'));
        }
    }

    #[test]
    fn lookups() {
        assert_eq!(resource_string(0), Some("FONTS"));
        assert_eq!(resource_string(2), Some("Times New Roman"));
        assert_eq!(resource_string(8556), Some("The player\u{2019}s current game status"));
        assert_eq!(resource_string(7), None);
        assert_eq!(resource_string(u16::MAX), None);
        assert!(ee_util_resource_string(7).is_null());
        let p = ee_util_resource_string(1);
        assert_eq!(unsafe { core::ffi::CStr::from_ptr(p.cast()) }.to_bytes(), b"Arial");
        assert_eq!(ee_util_resource_strings_language(), 0x409);
    }
}
