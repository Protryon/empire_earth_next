//! The saved games' texts (were EESaveLoad.cpp's `GetSaveTextName` and `GetDateText`): a save file's name with its
//! last three characters replaced by "txt", or the name kept; the date and time of a save's header (the C runtime's
//! asctime of now). The C++ forwarders make their UStrings of the answers. Also EEGameStateLoader.cpp's
//! `EEIsScenarioFile` (a file name's lowered ending against the scenario extension, over ee_base's UString records).

use std::cell::RefCell;
use std::ffi::{c_char, c_ulong, c_void, CStr};
use std::mem::MaybeUninit;

use ee_base::ustring::{
    ee_ustring_change_case, ee_ustring_compare, ee_ustring_free, ee_ustring_init_copy, ee_ustring_init_text,
    ee_ustring_substring, EeUString,
};

use super::{hp, O};

thread_local! {
    /// The last answer (valid until the thread's next call).
    static TEXT: RefCell<Vec<u8>> = const { RefCell::new(Vec::new()) };
}

/// The name (`length` its UString length, `text` its bytes up to the terminator) with its last three characters
/// replaced by "txt" (UString::Copy's count of `length - 3`, clamped to the text), or kept.
pub fn save_text_name(text: &[u8], length: c_ulong, keep: bool) -> Vec<u8> {
    if keep {
        return text.to_vec();
    }
    let count = length.wrapping_sub(3) as u64;
    let n = (count.min(text.len() as u64)) as usize;
    let mut out = text[..n].to_vec();
    out.extend_from_slice(b"txt");
    out
}

/// GetSaveTextName: the answer as a C string (the thread's buffer, valid until its next call).
///
/// # Safety
/// `name` is a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_save_text_name(name: *const c_char, length: c_ulong, keep: bool) -> *const c_char {
    let text = CStr::from_ptr(name).to_bytes();
    let mut out = save_text_name(text, length, keep);
    out.push(0);
    TEXT.with(|t| {
        *t.borrow_mut() = out;
        t.borrow().as_ptr() as *const c_char
    })
}

/// The C runtime's clock and calendar (time_t is 64-bit on every target; UCRT's `time` and `localtime` are inline
/// wrappers of `_time64` and `_localtime64`).
/// cbindgen:ignore
mod crt {
    use std::ffi::{c_char, c_void};

    extern "C" {
        #[cfg_attr(windows, link_name = "_time64")]
        pub fn time(t: *mut i64) -> i64;
        #[cfg_attr(windows, link_name = "_localtime64")]
        pub fn localtime(t: *const i64) -> *mut c_void;
        pub fn asctime(tm: *const c_void) -> *const c_char;
    }
}

/// The text of a save's date: asctime of now (its trailing newline kept), or "Date/time failed." when the clock
/// fails (or the calendar cannot convert it).
pub fn date_text() -> Vec<u8> {
    let mut now: i64 = 0;
    // SAFETY: the C runtime's calls on a local; asctime's static buffer copied at once
    unsafe {
        if crt::time(&mut now) != -1 {
            let tm: *mut c_void = crt::localtime(&now);
            if !tm.is_null() {
                return CStr::from_ptr(crt::asctime(tm)).to_bytes().to_vec();
            }
        }
    }
    b"Date/time failed.".to_vec()
}

/// GetDateText: the answer as a C string (the thread's buffer, valid until its next call).
#[no_mangle]
pub extern "C" fn ee_gamecpp_date_text() -> *const c_char {
    let mut out = date_text();
    out.push(0);
    TEXT.with(|t| {
        *t.borrow_mut() = out;
        t.borrow().as_ptr() as *const c_char
    })
}

/// Whether a file name, lowered, ends in the scenario extension (as the original: the name copied and lowered, its
/// last extension-length bytes taken and lowered, then compared with the extension as it is).
///
/// # Safety
/// `file` and `extension` are live UString records.
pub unsafe fn ends_in_extension(file: *const EeUString, extension: *const EeUString) -> bool {
    let mut lowered = MaybeUninit::<EeUString>::uninit();
    ee_ustring_init_copy(lowered.as_mut_ptr(), file);
    ee_ustring_change_case(lowered.as_mut_ptr(), false);
    let n = (*extension).length;
    let mut ending = MaybeUninit::<EeUString>::uninit();
    ee_ustring_substring(ending.as_mut_ptr(), lowered.as_ptr(), (*lowered.as_ptr()).length.wrapping_sub(n), n);
    ee_ustring_change_case(ending.as_mut_ptr(), false);
    let same = ee_ustring_compare((*ending.as_ptr()).string, (*extension).string) == 0;
    ee_ustring_free(ending.as_mut_ptr());
    ee_ustring_free(lowered.as_mut_ptr());
    same
}

/// A UString record of a C string (the copy the original made: its length the text's).
///
/// # Safety
/// `text` is a C string; the record is freed by the caller.
unsafe fn record_of(text: *const c_char) -> EeUString {
    let mut s = MaybeUninit::<EeUString>::uninit();
    ee_ustring_init_text(s.as_mut_ptr(), text);
    s.assume_init()
}

/// EEIsScenarioFile: whether a file name ends in the scenario extension (gScenarioExtension), case-insensitively.
///
/// # Safety
/// `file` is a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_is_scenario_file(file: *const c_char) -> bool {
    let mut f = record_of(file);
    let mut e = record_of(hp(O::C11ScenarioExtension, 0, &[]) as *const c_char);
    let same = ends_in_extension(&f, &e);
    ee_ustring_free(&mut f);
    ee_ustring_free(&mut e);
    same
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_extension_becomes_txt() {
        assert_eq!(save_text_name(b"game.sav", 8, false), b"game.txt");
        assert_eq!(save_text_name(b"EEQuickSave.sav", 15, true), b"EEQuickSave.sav");
        // a name under three characters: the copy's count wraps and is clamped to the text
        assert_eq!(save_text_name(b"ab", 2, false), b"abtxt");
        assert_eq!(save_text_name(b"", 0, false), b"txt");
        let p = unsafe { ee_gamecpp_save_text_name(c"x.scn".as_ptr(), 5, false) };
        assert_eq!(unsafe { CStr::from_ptr(p) }.to_bytes(), b"x.txt");
    }

    #[test]
    fn the_date_is_asctime_s_form() {
        let d = date_text();
        // "Sun Nov 22 17:00:00 2026\n": 24 characters and the newline
        assert_eq!(d.len(), 25, "{:?}", String::from_utf8_lossy(&d));
        assert_eq!(d[24], b'\n');
        assert_eq!(d[3], b' ');
    }

    fn record(text: &str) -> EeUString {
        let c = std::ffi::CString::new(text).unwrap();
        unsafe { record_of(c.as_ptr()) }
    }

    #[test]
    fn a_scenario_ends_in_the_extension_whatever_its_case() {
        let mut ext = record(".scn");
        for (name, want) in [("Map.SCN", true), ("a.scn", true), ("a.sav", false), ("sc", false), ("", false)] {
            let mut f = record(name);
            assert_eq!(unsafe { ends_in_extension(&f, &ext) }, want, "{name}");
            unsafe { ee_ustring_free(&mut f) };
        }
        // the extension is compared as it is: an upper-case one never matches
        let mut upper = record(".SCN");
        let mut f = record("a.scn");
        assert!(!unsafe { ends_in_extension(&f, &upper) });
        unsafe {
            ee_ustring_free(&mut f);
            ee_ustring_free(&mut upper);
            ee_ustring_free(&mut ext);
        }
    }
}
