//! `extern "C"` for a multi-line `UITextLabel`'s wrapping (`User Interface/UITextLabel.cpp`, `SetText`): `ctx` is the
//! C++ side's state (its text object and the label's new lines), handed back to `ee_uicpp_label_measure` (a word's
//! width) and `ee_uicpp_label_add_line` (one wrapped line) with NUL-terminated copies.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_void, CStr};

use crate::wrap::{split_lines, Platform};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_char, c_void};

    extern "C" {
        pub fn ee_uicpp_label_measure(ctx: *mut c_void, word: *const c_char) -> f32;
        pub fn ee_uicpp_label_add_line(ctx: *mut c_void, line: *const c_char);
    }
}

fn terminated(bytes: &[u8]) -> Vec<u8> {
    let mut v = Vec::with_capacity(bytes.len() + 1);
    v.extend_from_slice(bytes);
    v.push(0);
    v
}

/// `UITextLabel::SetText`'s line loop: `text` (NUL-terminated) wrapped to `width`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_label_wrap(ctx: *mut c_void, text: *const c_char, width: f32) {
    // an owned copy: the callbacks run C++ that must not see a Rust borrow of its memory
    let text = CStr::from_ptr(text).to_bytes_with_nul().to_vec();
    split_lines(
        &Platform,
        &mut |word: &[u8]| {
            let word = terminated(word);
            sys::ee_uicpp_label_measure(ctx, word.as_ptr().cast())
        },
        &text,
        width,
        &mut |line: &[u8]| {
            let line = terminated(line);
            sys::ee_uicpp_label_add_line(ctx, line.as_ptr().cast());
        },
    );
}
