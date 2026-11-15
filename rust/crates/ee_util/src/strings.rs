//! The game's language strings (data lane round 1): the table `UResourceFile::GetString` reads, generated from
//! Language.rc by `tools/lang_strings.py` into `language_strings.rs` (it replaced the generated
//! `LanguageStrings.cpp`). Each string is UTF-8 (decision D6) and ends in a NUL, so the C++ caller takes the pointer
//! as a C string; an empty rc string is missing, as LoadStringA's 0 was.

use crate::language_strings::{LANGUAGE, STRINGS};

/// The text of string `id` (without its closing NUL), or `None` when the table has no such string.
pub fn resource_string(id: u16) -> Option<&'static str> {
    STRINGS
        .binary_search_by_key(&id, |&(key, _)| key)
        .ok()
        .map(|i| &STRINGS[i].1[..STRINGS[i].1.len() - 1])
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
