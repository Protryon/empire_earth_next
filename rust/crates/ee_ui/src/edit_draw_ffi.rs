//! `extern "C"` for the edit box's drawing (`edit_draw.rs`): the lines' lengths come as arrays the C++ fills, the
//! underlines' measures and draws as a function table (no import: the module links into tests).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::edit_draw::{self, EditIme, EditSelection, ImeHost};
use crate::rect::{UiPoint, UiRect};

/// Entry `i` of the C++'s array of `n` lengths (0 past it: the C++ read past its lines there).
unsafe fn len_at(lens: *const c_ulong, n: usize, i: c_ulong) -> c_ulong {
    if (i as usize) < n {
        *lens.add(i as usize)
    } else {
        0
    }
}

/// `UIEditBox::CopyData`'s copied selection (`display_lens`: the copied lines' characters, `count` of them).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_edit_copied_selection(
    caret_line: c_ulong,
    caret_index: c_long,
    anchor_line: c_ulong,
    anchor_index: c_long,
    drawing_index: c_long,
    first: c_ulong,
    last: c_ulong,
    display_lens: *const c_ulong,
    count: usize,
) -> EditSelection {
    edit_draw::copied_selection(caret_line, caret_index, anchor_line, anchor_index, drawing_index, first, last, |i| {
        len_at(display_lens, count, i)
    })
}

/// `UIEditBox::CopyData`'s copied composition of a single-line box.
#[no_mangle]
pub extern "C" fn ee_ui_edit_copied_ime_single(
    start_index: c_ulong,
    length: c_ulong,
    drawing_index: c_long,
) -> EditIme {
    edit_draw::copied_ime_single(start_index, length, drawing_index)
}

/// `UIEditBox::CopyData`'s copied composition of a multi-line box (`line_lens`: every line's characters).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_edit_copied_ime_lines(
    start_line: c_ulong,
    start_index: c_ulong,
    length: c_ulong,
    first: c_ulong,
    last: c_ulong,
    after_start: c_long,
    line_lens: *const c_ulong,
    count: usize,
) -> EditIme {
    edit_draw::copied_ime_lines(start_line, start_index, length, first, last, after_start, |i| {
        len_at(line_lens, count, i)
    })
}

/// A highlighted span of a line: characters `from .. to`.
#[repr(C)]
pub struct EeUiEditSpan {
    pub from: c_ulong,
    pub to: c_ulong,
}

/// `UIEditBox::DrawPolygons`' selection on copied line `i` (`line_len` its characters).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_selection_span(
    s: *const EditSelection,
    i: c_ulong,
    line_len: c_ulong,
) -> EeUiEditSpan {
    let (from, to) = edit_draw::selection_span(&*s, i, line_len);
    EeUiEditSpan { from, to }
}

/// `UIEditBox::GetSelectedTextRectangle` from the two widths the C++ measured.
#[no_mangle]
pub extern "C" fn ee_ui_edit_selection_rect(
    line: c_ulong,
    height: c_long,
    tr: UiRect,
    before: f32,
    span: f32,
) -> UiRect {
    edit_draw::selection_rect(line, height, &tr, before, span)
}

/// `UIEditBox::DrawTextA`'s row y of copied line `i`.
#[no_mangle]
pub extern "C" fn ee_ui_edit_text_row_y(i: c_ulong, height: c_long, top: f32) -> f32 {
    edit_draw::text_row_y(i, height, top)
}

/// The C++'s measures and draw for the IME underlines (`ImeHost`), each given `ctx`.
#[repr(C)]
pub struct EeUiImeHost {
    pub ctx: *mut c_void,
    pub display_len: unsafe extern "C" fn(ctx: *mut c_void, i: c_ulong) -> c_ulong,
    pub prefix_width: unsafe extern "C" fn(ctx: *mut c_void, i: c_ulong, j: c_ulong) -> f32,
    pub char_bytes: unsafe extern "C" fn(ctx: *mut c_void, i: c_ulong, j: c_ulong) -> usize,
    pub char_width: unsafe extern "C" fn(ctx: *mut c_void, i: c_ulong, j: c_ulong) -> f32,
    pub attribute: unsafe extern "C" fn(ctx: *mut c_void, offset: i32) -> u8,
    pub draw_line: unsafe extern "C" fn(ctx: *mut c_void, start: UiPoint, end: UiPoint) -> c_long,
}

struct Table<'a>(&'a EeUiImeHost);

impl ImeHost for Table<'_> {
    fn display_len(&mut self, i: c_ulong) -> c_ulong {
        unsafe { (self.0.display_len)(self.0.ctx, i) }
    }
    fn prefix_width(&mut self, i: c_ulong, j: c_ulong) -> f32 {
        unsafe { (self.0.prefix_width)(self.0.ctx, i, j) }
    }
    fn char_bytes(&mut self, i: c_ulong, j: c_ulong) -> usize {
        unsafe { (self.0.char_bytes)(self.0.ctx, i, j) }
    }
    fn char_width(&mut self, i: c_ulong, j: c_ulong) -> f32 {
        unsafe { (self.0.char_width)(self.0.ctx, i, j) }
    }
    fn attribute(&mut self, offset: i32) -> u8 {
        unsafe { (self.0.attribute)(self.0.ctx, offset) }
    }
    fn draw_line(&mut self, start: UiPoint, end: UiPoint) -> c_long {
        unsafe { (self.0.draw_line)(self.0.ctx, start, end) }
    }
}

/// `UIEditBox::DrawPolygons`' IME underlines -> the last draw's result (`result` when none is drawn).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_ime_underlines(
    m: *const EditIme,
    tr: UiRect,
    font_height: c_ulong,
    result: c_long,
    host: *const EeUiImeHost,
) -> c_long {
    edit_draw::ime_underlines(&mut Table(&*host), &*m, &tr, font_height, result)
}
