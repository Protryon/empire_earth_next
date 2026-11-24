//! WONGUI's Clipboard over the platform's (R6 WON GUI round 35).
//!
//! The original read and wrote CF_UNICODETEXT; the port's PlatClipboard.cpp went through the platform layer's UTF-8
//! clipboard text (decision D6) with the conversions in C++. Both methods are here; the platform's calls
//! (`PlatClipboardGetText`, `PlatClipboardSetText` and the C runtime's `free` for the text the first returns) come as
//! function pointers.
use ee_base::plat_core::string::{PlatUtf8ToWide, PlatWideToUtf8, WChar, PLAT_TEXT_TERMINATED};
use std::cell::RefCell;
use std::ffi::{c_char, c_void};

/// The platform's clipboard calls.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonClipFns {
    /// `PlatClipboardGetText()`: the text (UTF-8, terminated, the caller's to free), NULL when the clipboard holds
    /// none.
    pub get: unsafe extern "C" fn() -> *mut c_char,
    /// `PlatClipboardSetText(text)`.
    pub set: unsafe extern "C" fn(text: *const c_char) -> i32,
    /// `free(text)`.
    pub free: unsafe extern "C" fn(text: *mut c_void),
}

/// `Clipboard::Get()`'s text as wide units: empty when the clipboard holds none; the platform's text freed.
pub fn get(fns: &EeWonClipFns) -> Vec<WChar> {
    // SAFETY: the platform's calls (the struct's contract)
    unsafe {
        let text = (fns.get)();
        if text.is_null() {
            return Vec::new();
        }
        let count = PlatUtf8ToWide(text, PLAT_TEXT_TERMINATED, std::ptr::null_mut(), 0);
        let mut wide = vec![0 as WChar; count as usize];
        if count != 0 {
            PlatUtf8ToWide(text, PLAT_TEXT_TERMINATED, wide.as_mut_ptr(), count);
        }
        (fns.free)(text as *mut c_void);
        wide
    }
}

/// `Clipboard::Set(text)`: the units as UTF-8, handed over terminated (so a text with a zero unit is cut there, as
/// the original's `c_str()` was).
pub fn set(fns: &EeWonClipFns, text: &[WChar]) {
    // SAFETY: the slice; the platform's call
    unsafe {
        let count = PlatWideToUtf8(text.as_ptr(), text.len() as u32, std::ptr::null_mut(), 0);
        let mut utf8 = vec![0u8; count as usize + 1];
        if count != 0 {
            PlatWideToUtf8(text.as_ptr(), text.len() as u32, utf8.as_mut_ptr() as *mut c_char, count);
        }
        (fns.set)(utf8.as_ptr() as *const c_char);
    }
}

thread_local! {
    /// The text `ee_won_clip_get` last answered with (terminated).
    static TEXT: RefCell<Vec<WChar>> = const { RefCell::new(Vec::new()) };
}

/// `Clipboard::Get()`: the clipboard's text as terminated wide units, empty when it holds none. The answer is this
/// thread's until its next call.
///
/// # Safety
/// `fns` is live and its functions are the platform's.
#[no_mangle]
pub unsafe extern "C" fn ee_won_clip_get(fns: *const EeWonClipFns) -> *const c_void {
    // SAFETY: the contract
    let mut wide = get(unsafe { &*fns });
    wide.push(0);
    TEXT.with(|t| {
        *t.borrow_mut() = wide;
        t.borrow().as_ptr() as *const c_void
    })
}

/// `Clipboard::Set(text)`: replaces the clipboard's contents with the `len` units at `text`.
///
/// # Safety
/// `fns` as `ee_won_clip_get`; `text` holds `len` units (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_clip_set(fns: *const EeWonClipFns, text: *const c_void, len: usize) {
    // SAFETY: the contract
    let t: &[WChar] =
        if text.is_null() || len == 0 { &[] } else { unsafe { std::slice::from_raw_parts(text as *const WChar, len) } };
    // SAFETY: the contract
    set(unsafe { &*fns }, t);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CStr;

    thread_local! {
        static HAVE: RefCell<Option<Vec<u8>>> = const { RefCell::new(None) };
        static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    }
    unsafe extern "C" fn get_text() -> *mut c_char {
        HAVE.with(|h| match h.borrow().as_ref() {
            None => std::ptr::null_mut(),
            Some(t) => {
                let mut v = t.clone();
                v.push(0);
                Box::into_raw(v.into_boxed_slice()) as *mut c_char
            }
        })
    }
    unsafe extern "C" fn set_text(text: *const c_char) -> i32 {
        // SAFETY: terminated (the module's contract with the platform)
        let s = unsafe { CStr::from_ptr(text) }.to_bytes().to_vec();
        LOG.with(|l| l.borrow_mut().push(format!("set {s:02x?}")));
        1
    }
    unsafe extern "C" fn free_text(text: *mut c_void) {
        let n = HAVE.with(|h| h.borrow().as_ref().map_or(0, |t| t.len() + 1));
        // SAFETY: get_text's box of that length
        drop(unsafe { Box::from_raw(std::ptr::slice_from_raw_parts_mut(text as *mut u8, n)) });
        LOG.with(|l| l.borrow_mut().push("free".to_string()));
    }
    const FNS: EeWonClipFns = EeWonClipFns { get: get_text, set: set_text, free: free_text };
    fn log() -> Vec<String> {
        LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
    }
    fn wide(s: &str) -> Vec<WChar> {
        s.encode_utf16().map(|c| c as WChar).collect()
    }

    #[test]
    fn get_converts_and_frees_and_an_empty_clipboard_is_an_empty_text() {
        assert!(get(&FNS).is_empty());
        assert!(log().is_empty());
        HAVE.with(|h| *h.borrow_mut() = Some("a\u{e9}\u{20ac}".as_bytes().to_vec()));
        if std::mem::size_of::<WChar>() == 2 {
            assert_eq!(get(&FNS), wide("a\u{e9}\u{20ac}"));
        } else {
            assert_eq!(get(&FNS).len(), 3);
        }
        assert_eq!(log(), ["free"]);
        HAVE.with(|h| *h.borrow_mut() = Some(Vec::new()));
        assert!(get(&FNS).is_empty());
        assert_eq!(log(), ["free"]);
        // the FFI's answer is terminated and stays until the next call
        HAVE.with(|h| *h.borrow_mut() = Some(b"xy".to_vec()));
        // SAFETY: the test's functions
        let p = unsafe { ee_won_clip_get(&FNS) } as *const WChar;
        // SAFETY: three units
        assert_eq!(unsafe { std::slice::from_raw_parts(p, 3) }, &[b'x' as WChar, b'y' as WChar, 0]);
        log();
    }

    #[test]
    fn set_hands_over_terminated_utf8() {
        if std::mem::size_of::<WChar>() != 2 {
            return;
        }
        set(&FNS, &wide("a\u{e9}"));
        assert_eq!(log(), ["set [61, c3, a9]"]);
        set(&FNS, &[]);
        assert_eq!(log(), ["set []"]);
        // a zero unit cuts the text for the platform (the original's c_str())
        set(&FNS, &[b'a' as WChar, 0, b'b' as WChar]);
        assert_eq!(log(), ["set [61]"]);
        // SAFETY: NULL with no units
        unsafe { ee_won_clip_set(&FNS, std::ptr::null(), 0) };
        assert_eq!(log(), ["set []"]);
    }
}
