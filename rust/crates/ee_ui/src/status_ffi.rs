//! `extern "C"` for `UIStatusBar`'s arithmetic (`User Interface/UIStatusBar.cpp`): plain values in and out, no host
//! calls.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong};

use crate::rect::UiRect;
use crate::status;

/// `SetCurrentRangeValue`: the clamped value; `out` the fill rectangle.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_status_fill(
    value: c_long,
    min: c_long,
    max: c_long,
    width: c_ulong,
    fill: UiRect,
    out: *mut UiRect,
) -> c_long {
    let (value, r) = status::fill(value, min, max, width, fill);
    out.write(r);
    value
}

/// `SetLocation`'s fill and text rectangles from the moved control rectangle.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_status_location(rect: UiRect, border: c_ulong, fill: *mut UiRect, text: *mut UiRect) {
    let (f, t) = status::location(rect, border);
    fill.write(f);
    text.write(t);
}

/// `CopyData`'s fill: whether the border insets it (`out` then the inset rectangle).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_status_copy_fill(fill: UiRect, border: c_ulong, out: *mut UiRect) -> bool {
    match status::copy_fill(fill, border) {
        Some(r) => {
            out.write(r);
            true
        }
        None => false,
    }
}

/// `CopyData`'s text rectangle for a text of `width` x `height`.
#[no_mangle]
pub extern "C" fn ee_ui_status_copy_text(text: UiRect, width: f32, height: f32) -> UiRect {
    status::copy_text(text, (width, height))
}
