//! The saved games' text names (was EESaveLoad.cpp's `GetSaveTextName`): a save file's name with its last three
//! characters replaced by "txt", or the name kept. The C++ forwarder makes its UString of the answer.

use std::cell::RefCell;
use std::ffi::{c_char, c_ulong, CStr};

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
}
