//! WONGUI's WrappedText as a Rust object (R6 WON round 38): every label's and skinned button's text layout. The
//! original's WrappedText.cpp: the line breaks for the font and maximum width (`guiwrap`'s rule), the text's width
//! and height, the wrap and centre settings, the rewrap rule of `SetMaxWidth`, and the paint's plan (`guidraw`'s
//! `wrapped_text` over the object's own breaks).
//!
//! The C++ `WrappedText` stays the reference-counted type the labels and buttons hold; it keeps its C++-object
//! members (the text and the font) and passes them to `ee_won_wrap_calc` (the text's units and a measure of the
//! font) when the original rewrapped. The setters that may rewrap answer whether to.

use std::ffi::c_void;

use crate::guidraw::{wrapped_text, EeWonDrawOp, EeWonLineBreak, EeWonTextMeasure};
use crate::guiwrap::{max_width_change, wrap};

/// The Rust WrappedText (the original's members but the text and font).
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct EeWonWrapText {
    /// `mBreakList` (each field the original's `unsigned short`).
    pub breaks: Vec<EeWonLineBreak>,
    pub width: i32,
    pub height: i32,
    pub max_width: i32,
    pub wrap: bool,
    pub centered: bool,
}

impl EeWonWrapText {
    /// `Init()`: wrapping, not centred, no size and no maximum width.
    pub fn new() -> Self {
        EeWonWrapText { wrap: true, ..Default::default() }
    }

    /// `CalcWrap()` over the text's units, `measure` the font's (`string_width(0, len)` and `char_width(i)`) and
    /// its height; nothing without a font.
    pub fn calc(&mut self, has_font: bool, units: &[u32], m: &EeWonTextMeasure, font_height: i32) {
        if !has_font {
            return;
        }
        let len = units.len() as i32;
        if !self.wrap {
            self.width = m.string_width_at(0, len);
            self.height = font_height;
            return;
        }
        if self.max_width == 0 {
            return;
        }
        self.breaks.clear();
        // the original passed the length as an unsigned short to the wrap
        let n = usize::from(len as u16);
        let widths: Vec<i32> = (0..len).map(|i| m.char_width_at(i)).collect();
        let (b, width) = wrap(&units[..n], &widths[..n], self.max_width);
        self.breaks.extend(b.iter().map(|x| EeWonLineBreak {
            break_pos: x.pos as u16,
            next_start: x.next as u16,
            width: x.width as u16,
        }));
        self.width = width;
        // (size + 1) * height in the original's size_t, kept as an int
        self.height = ((self.breaks.len() as u64 + 1).wrapping_mul(font_height as i64 as u64)) as i32;
    }

    /// `SetWrap`: whether to rewrap (the setting changed).
    pub fn set_wrap(&mut self, wrap: bool) -> bool {
        if wrap != self.wrap {
            self.wrap = wrap;
            return true;
        }
        false
    }

    /// `SetMaxWidth`: whether to rewrap (`guiwrap`'s rule).
    pub fn set_max_width(&mut self, width: i32) -> bool {
        match max_width_change(width, self.max_width, self.wrap, !self.breaks.is_empty()) {
            Some((w, rewrap)) => {
                self.max_width = w;
                rewrap
            }
            None => false,
        }
    }

    /// `Paint`'s plan (with a font: the C++ returns before it without one).
    pub fn paint(&self, length: i32, font_height: i32, x: i32, y: i32, m: &EeWonTextMeasure) -> Vec<EeWonDrawOp> {
        wrapped_text(true, length, self.centered, self.width, font_height, &self.breaks, x, y, m)
    }
}

/// A new WrappedText's state (`Init()`).
#[no_mangle]
pub extern "C" fn ee_won_wrap_new() -> *mut EeWonWrapText {
    Box::into_raw(Box::new(EeWonWrapText::new()))
}

/// # Safety
/// `o` is null or from `ee_won_wrap_new`, not freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_free(o: *mut EeWonWrapText) {
    if !o.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(o) });
    }
}

/// `CalcWrap()`: `text` holds `len` units of `wchar_size` bytes (or is NULL with 0), `m` measures them in the font.
///
/// # Safety
/// `o` is live; `text` and `m` as described.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_calc(
    o: *mut EeWonWrapText,
    has_font: bool,
    text: *const c_void,
    len: i32,
    wchar_size: u32,
    m: *const EeWonTextMeasure,
    font_height: i32,
) {
    let n = len.max(0) as usize;
    let units: Vec<u32> = if n == 0 || text.is_null() {
        Vec::new()
    } else if wchar_size == 2 {
        // SAFETY: `len` units (the contract)
        unsafe { std::slice::from_raw_parts(text.cast::<u16>(), n) }.iter().map(|&u| u32::from(u)).collect()
    } else {
        // SAFETY: `len` units (the contract)
        unsafe { std::slice::from_raw_parts(text.cast::<u32>(), n) }.to_vec()
    };
    // SAFETY: the contract
    unsafe { (*o).calc(has_font, &units, &*m, font_height) }
}

/// `SetWrap(wrap)`: true when the C++ must rewrap.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_set_wrap(o: *mut EeWonWrapText, wrap: bool) -> bool {
    // SAFETY: the contract
    unsafe { (*o).set_wrap(wrap) }
}

/// `SetMaxWidth(width)`: true when the C++ must rewrap.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_set_max_width(o: *mut EeWonWrapText, width: i32) -> bool {
    // SAFETY: the contract
    unsafe { (*o).set_max_width(width) }
}

/// `SetCentered(centered)`.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_set_centered(o: *mut EeWonWrapText, centered: bool) {
    // SAFETY: the contract
    unsafe { (*o).centered = centered }
}

/// `mWidth`.
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_width(o: *const EeWonWrapText) -> i32 {
    // SAFETY: the contract
    unsafe { (*o).width }
}

/// `mHeight` (the C++ `GetHeight` answers the font's height when it is 0).
///
/// # Safety
/// `o` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_height(o: *const EeWonWrapText) -> i32 {
    // SAFETY: the contract
    unsafe { (*o).height }
}

/// The break list (`mBreakList`): the first `cap` into `out`, their count returned.
///
/// # Safety
/// `o` is live; `out` has room for `cap` breaks (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_breaks(o: *const EeWonWrapText, out: *mut EeWonLineBreak, cap: usize) -> usize {
    // SAFETY: the contract
    let b = unsafe { &(*o).breaks };
    if !out.is_null() {
        for (i, x) in b.iter().take(cap).enumerate() {
            // SAFETY: room for `cap` (the contract)
            unsafe { *out.add(i) = *x };
        }
    }
    b.len()
}

/// `Paint`'s plan for a text of `length` units in a font `font_height` high at (`x`, `y`): the first `cap`
/// operations into `out`, their count returned.
///
/// # Safety
/// `o` and `m` are live; `out` has room for `cap` operations (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_wrap_paint(
    o: *const EeWonWrapText,
    length: i32,
    font_height: i32,
    x: i32,
    y: i32,
    m: *const EeWonTextMeasure,
    out: *mut EeWonDrawOp,
    cap: usize,
) -> usize {
    // SAFETY: the contract
    let ops = unsafe { (*o).paint(length, font_height, x, y, &*m) };
    if !out.is_null() {
        for (i, op) in ops.iter().take(cap).enumerate() {
            // SAFETY: room for `cap` (the contract)
            unsafe { *out.add(i) = *op };
        }
    }
    ops.len()
}

#[cfg(test)]
#[path = "wrapobj_tests.rs"]
mod tests;
