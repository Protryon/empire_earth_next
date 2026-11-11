//! The edit box's drawing (`User Interface/UIEditBox.cpp`): `CopyData`'s copied selection and IME composition
//! ranges, clipped to the copied lines, and `DrawPolygons`' selection rectangles and IME underlines, `DrawTextA`'s
//! line rows.
//!
//! The C++ keeps the lines, the copies of the text and every draw call; the ranges are pure functions over its
//! values, and the underline loop measures and draws through [`ImeHost`] in the C++'s order. Integer arithmetic is
//! the C++'s (`unsigned long` and `long` wrapping, the signed and unsigned compares where it made them).

use core::ffi::{c_long, c_ulong};

use crate::form_state::and_in_place;
use crate::rect::{UiPoint, UiRect};

/// The copied selection (`mCopiedSelection*`, `mCopiedIsTextSelected`): lines relative to the first copied line.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EditSelection {
    pub start_line: c_ulong,
    pub end_line: c_ulong,
    pub start_index: c_long,
    pub end_index: c_long,
    pub selected: bool,
}

/// `UIEditBox::CopyData`'s selection: the caret and the anchor ordered, the indices made relative to the drawing
/// index (single-line boxes), the lines to the copied window `first .. last` (a start above it starts the copy, one
/// below it or an end above it hides the selection; an end below it ends at the last copied line's end,
/// `display_len` giving the copied lines' lengths).
#[allow(clippy::too_many_arguments)]
pub fn copied_selection(
    caret_line: c_ulong,
    caret_index: c_long,
    anchor_line: c_ulong,
    anchor_index: c_long,
    drawing_index: c_long,
    first: c_ulong,
    last: c_ulong,
    display_len: impl Fn(c_ulong) -> c_ulong,
) -> EditSelection {
    let mut s = EditSelection { selected: true, ..Default::default() };
    if caret_line < anchor_line {
        s.end_line = anchor_line;
        s.start_line = caret_line;
        s.end_index = anchor_index;
        s.start_index = caret_index;
    } else {
        s.start_line = anchor_line;
        s.end_line = caret_line;
        s.start_index = anchor_index;
        s.end_index = caret_index;
    }
    if s.start_line == s.end_line && caret_index < anchor_index {
        s.end_index = anchor_index;
        s.start_index = caret_index;
    }
    s.start_index = if s.start_index <= drawing_index { 0 } else { s.start_index.wrapping_sub(drawing_index) };
    s.end_index = if s.end_index <= drawing_index { 0 } else { s.end_index.wrapping_sub(drawing_index) };
    if s.start_line < first {
        s.start_line = 0;
        s.start_index = 0;
    } else if s.start_line >= last {
        s.selected = false;
    } else {
        s.start_line -= first;
    }
    if s.end_line >= last {
        s.end_line = last.wrapping_sub(first).wrapping_sub(1);
        s.end_index = display_len(s.end_line) as c_long;
    } else if (s.end_line.wrapping_sub(first) as c_long) >= 0 {
        s.end_line = s.end_line.wrapping_sub(first);
    } else {
        s.selected = false;
    }
    s
}

/// The copied IME composition (`mCopiedIME*`): lines relative to the first copied line.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EditIme {
    pub start_line: c_ulong,
    pub start_index: c_ulong,
    pub end_line: c_ulong,
    pub end_index: c_ulong,
    pub active: bool,
}

/// `UIEditBox::CopyData`'s composition in a single-line box: from its start (relative to the drawing index, or the
/// box's start when it began before it) for its length, on line 0.
pub fn copied_ime_single(start_index: c_ulong, length: c_ulong, drawing_index: c_long) -> EditIme {
    let start =
        if start_index < drawing_index as c_ulong { 0 } else { start_index.wrapping_sub(drawing_index as c_ulong) };
    EditIme { start_line: 0, start_index: start, end_line: 0, end_index: length.wrapping_add(start), active: true }
}

/// `UIEditBox::CopyData`'s composition in a multi-line box: its start clipped to the copied window `first .. last`
/// (above it: the copy's start; below it: hidden) and its end found by walking the composition's length over the
/// lines after the start (`after_start`: the characters of the start line from the start; `line_len`: a line's
/// characters), not clipped (the box scrolls to it).
pub fn copied_ime_lines(
    start_line: c_ulong,
    start_index: c_ulong,
    length: c_ulong,
    first: c_ulong,
    last: c_ulong,
    after_start: c_long,
    line_len: impl Fn(c_ulong) -> c_ulong,
) -> EditIme {
    let mut m = EditIme { start_line, start_index, active: true, ..Default::default() };
    if start_line < first {
        m.start_line = 0;
        m.start_index = 0;
    } else if start_line >= last {
        m.active = false;
    } else {
        m.start_line = start_line - first;
    }
    if after_start < length as c_long {
        let mut count = (length as c_long).wrapping_sub(after_start);
        let mut line = start_line.wrapping_add(1);
        while count > 0 {
            count = count.wrapping_sub(line_len(line) as c_long);
            line = line.wrapping_add(1);
        }
        m.end_line = line.wrapping_sub(first).wrapping_sub(1);
        m.end_index = line_len(line.wrapping_sub(1)).wrapping_add(count as c_ulong);
    } else {
        m.end_index = length.wrapping_add(start_index);
        m.end_line = start_line.wrapping_sub(first);
    }
    m
}

/// `UIEditBox::DrawPolygons`' selection on copied line `i` (`start_line ..= end_line`): the characters `from .. to`
/// highlighted, the whole line (`line_len` characters) between the first and the last.
pub fn selection_span(s: &EditSelection, i: c_ulong, line_len: c_ulong) -> (c_ulong, c_ulong) {
    if i == s.start_line {
        if i == s.end_line {
            (s.start_index as c_ulong, s.end_index as c_ulong)
        } else {
            (s.start_index as c_ulong, line_len)
        }
    } else if i == s.end_line {
        (0, s.end_index as c_ulong)
    } else {
        (0, line_len)
    }
}

/// `UIEditBox::GetSelectedTextRectangle`: the row of line `line` (`height` pixels a row) from the end of the
/// `before`-wide text to `span` further, clipped to the text rectangle `tr` (left as it is when outside it).
pub fn selection_rect(line: c_ulong, height: c_long, tr: &UiRect, before: f32, span: f32) -> UiRect {
    let top = line.wrapping_mul(height as c_ulong) as f32 + tr.top;
    let bottom = height as f32 + top;
    let left = before + tr.left;
    let mut r = UiRect { top, bottom, left, right: span + left };
    and_in_place(&mut r, tr);
    r
}

/// `UIEditBox::DrawTextA`'s row of copied line `i`: its y.
pub fn text_row_y(i: c_ulong, height: c_long, top: f32) -> f32 {
    i.wrapping_mul(height as c_ulong) as f32 + top
}

/// What `DrawPolygons`' IME underlines measure and draw (the copied text and the copied lines are the C++'s).
pub trait ImeHost {
    /// The copied line `i`'s characters.
    fn display_len(&mut self, i: c_ulong) -> c_ulong;
    /// The width of the first `j` characters of copied line `i` (the copied text set to them).
    fn prefix_width(&mut self, i: c_ulong, j: c_ulong) -> f32;
    /// The bytes of the character at `j` of copied line `i` (0 past its end).
    fn char_bytes(&mut self, i: c_ulong, j: c_ulong) -> usize;
    /// The width of the character at `j` of copied line `i` (the copied text set to it).
    fn char_width(&mut self, i: c_ulong, j: c_ulong) -> f32;
    /// The copied IME attribute byte at `offset` (0 out of range).
    fn attribute(&mut self, offset: i32) -> u8;
    /// Draws an underline in the IME colour -> the draw's result.
    fn draw_line(&mut self, start: UiPoint, end: UiPoint) -> c_long;
}

/// `UIEditBox::DrawPolygons`' IME underlines: under each composed character whose attribute is 1 or 3 (the target
/// clauses) and that starts left of the text rectangle's right edge, one pixel above the row's bottom, cut at the
/// right edge; `result` is returned unchanged when nothing is drawn.
pub fn ime_underlines(
    h: &mut impl ImeHost,
    m: &EditIme,
    tr: &UiRect,
    font_height: c_ulong,
    mut result: c_long,
) -> c_long {
    let mut offset: i32 = 0;
    let mut i = m.start_line;
    while i <= m.end_line {
        let mut j = if i == m.start_line { m.start_index } else { 0 };
        loop {
            let end = if i == m.end_line { m.end_index } else { h.display_len(i) };
            if j > end {
                break;
            }
            let x = h.prefix_width(i, j) + tr.left;
            let bytes = h.char_bytes(i, j);
            let a = h.attribute(offset);
            if (a == 1 || a == 3) && tr.right > x {
                let y = i.wrapping_add(1).wrapping_mul(font_height) as f32 + tr.top - 1.0;
                let w = h.char_width(i, j);
                let end_x = if w + x < tr.right { w + x } else { tr.right };
                result = h.draw_line(UiPoint { x, y }, UiPoint { x: end_x, y });
            }
            offset = offset.wrapping_add(bytes as i32);
            if j == c_ulong::MAX {
                break;
            }
            j += 1;
        }
        if i == c_ulong::MAX {
            break;
        }
        i += 1;
    }
    result
}

#[cfg(test)]
mod tests;
