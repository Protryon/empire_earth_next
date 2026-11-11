//! `UIEditBox`'s text model (`User Interface/UIEditBox.cpp`): insertion with word wrap, deletion and re-wrap, caret
//! moves, selection, scrolling a single-line box, the caret's place from a mouse location, and the IME composition's
//! bookkeeping. The C++ object keeps every member; its lines stay a `std::vector<UWideString>` that the host reads and
//! writes for Rust ([`EditHost`]), and its scalar members from `mCaretX` on are the pinned record [`EditFields`].
//!
//! Text is read only through [`crate::text`] (character stepping, lengths, line ends, spaces, digits), widths come
//! from the box's own `GEText` through the host. A virtual host call can come back into this module for the same box
//! (`SetRectangle` calls the virtual `SetLocation`), so [`Edit`] writes its state back before every host call with side
//! effects and reads it again after.
//!
//! One divergence: `GetLocationFromIndex` with an index before the first drawn character (a multi-line box can keep a
//! drawing index from `SetCaretLocation`) asked `UWideString::CopyString` for 2^32 - k characters, which padded that
//! many NUL bytes into a small buffer; here it measures nothing (the caret at the text's left edge).

use core::ffi::{c_long, c_short, c_ulong};

use crate::rect::{um_floor, UiPoint, UiRect};
use crate::text::{
    append, char_len, chars_in_bytes, empty, from_bytes, has_line_end, is_digit_at, is_space_at, masked, offset,
    prefix, starts_with, tail, Mbcs,
};

/// `kControlTriggerEnterPressed`.
pub const TRIGGER_ENTER_PRESSED: c_ulong = 0x3f1;
/// `kScrollBarWidth`.
pub const SCROLL_BAR_WIDTH: f32 = 20.0;
/// `kErrorOK`, `kErrorFailed`.
pub const ERROR_OK: c_long = 0;
pub const ERROR_FAILED: c_long = 9;

/// `UIEditBox`'s members from `mCaretX` (+0x17c in the original) to `mSavedAlwaysDirty`, in order (offsets
/// `static_assert`ed in `UIEditBox.cpp`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EditFields {
    pub caret_x: f32,
    pub saved_bg_opacity: f32,
    pub selection_start_line: c_ulong,
    pub copied_selection_start_line: c_ulong,
    pub copied_selection_end_line: c_ulong,
    pub max_chars: c_ulong,
    pub char_scroll: c_ulong,
    pub caret_line: c_ulong,
    pub visible_line_count: c_ulong,
    pub ime_composition_length: c_ulong,
    pub ime_start_index: c_ulong,
    pub ime_start_line: c_ulong,
    pub copied_ime_start_index: c_ulong,
    pub copied_ime_start_line: c_ulong,
    pub copied_ime_end_index: c_ulong,
    pub copied_ime_end_line: c_ulong,
    pub copied_selection_end_index: c_long,
    pub copied_selection_start_index: c_long,
    pub selection_start_index: c_long,
    pub caret_index: c_long,
    pub drawing_index: c_long,
    pub is_text_selected: bool,
    pub key_repeating: bool,
    pub cursor_on: bool,
    pub selecting: bool,
    pub copied_cursor_on: bool,
    pub copied_is_text_selected: bool,
    pub numbers_only: bool,
    pub is_single_line: bool,
    pub scroll_bar_captured: bool,
    pub scroll_bar_visible: bool,
    pub is_password_box: bool,
    pub ime_active: bool,
    pub copied_ime_active: bool,
    pub saved_always_dirty: bool,
}

/// What the model reads of the C++ object: the record, `mTextRectangle`, `mRectangle` and `mBorderThickness`.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EditState {
    pub f: EditFields,
    pub text: UiRect,
    pub rect: UiRect,
    pub border: c_ulong,
}

/// The C++ side of an edit box. Texts cross NUL-terminated.
pub trait EditHost {
    /// The members as they are now.
    fn load(&self) -> EditState;
    /// Writes the record and `mTextRectangle` back (the rectangle and the border are the host's).
    fn store(&mut self, s: &EditState);
    /// `mLines.size()`.
    fn line_count(&self) -> usize;
    /// `mLines[i]`.
    fn line(&self, i: usize) -> Vec<u8>;
    /// `mLines[i] = text`.
    fn set_line(&mut self, i: usize, text: &[u8]);
    /// `mLines.insert(begin + i, text)` (`i` may be the size).
    fn insert_line(&mut self, i: usize, text: &[u8]);
    /// `mLines.erase(begin + i)`.
    fn erase_line(&mut self, i: usize);
    /// `mLines.erase(begin + 1, end)`.
    fn keep_first_line(&mut self);
    /// `GetGEText()->SetText(text)`, then its extent's width.
    fn text_width(&mut self, text: &[u8]) -> f32;
    /// `GetGEText()->GetFont()->GetHeight()`.
    fn font_height(&self) -> c_long;
    /// `mScrollBar != NULL`.
    fn has_scroll_bar(&self) -> bool;
    /// The scroll bar slider's `GetCurrentRangeValue`.
    fn scroll_value(&self) -> c_long;
    /// The scroll bar slider's `GetMaxRange`.
    fn scroll_max(&self) -> c_long;
    /// `mScrollBar->SetCurrentRangeValue`.
    fn set_scroll_value(&mut self, value: c_ulong);
    /// `mScrollBar->SetRange`.
    fn set_scroll_range(&mut self, min: c_ulong, max: c_ulong);
    /// `mScrollBar->SetLocation`.
    fn scroll_set_location(&mut self, at: UiPoint);
    /// `SetDirty(true)`.
    fn set_dirty(&mut self);
    /// The virtual `SetRectangle`.
    fn set_rectangle(&mut self, r: &UiRect);
    /// `UIControl::SetRectangle`.
    fn base_set_rectangle(&mut self, r: &UiRect);
    /// `UIControl::SetLocation`.
    fn base_set_location(&mut self, at: UiPoint);
    /// `FireTriggerEvent`.
    fn fire(&mut self, event: c_ulong);
}

/// An index or count as the C runtime's `size_t` takes it (a negative one is huge: the stepping stops at the end).
fn idx(v: c_long) -> usize {
    v as isize as usize
}

/// One call into the model: the members as of its start, written back by [`Edit::finish`].
pub struct Edit<'a, M: Mbcs, H: EditHost> {
    pub(crate) m: &'a M,
    pub h: &'a mut H,
    pub s: EditState,
}

impl<'a, M: Mbcs, H: EditHost> Edit<'a, M, H> {
    pub fn new(m: &'a M, h: &'a mut H) -> Self {
        let s = h.load();
        Edit { m, h, s }
    }

    /// Writes the members back.
    pub fn finish(self) {
        self.h.store(&self.s);
    }

    /// A host call with side effects: the members go out before it and come back after.
    pub(crate) fn call(&mut self, f: impl FnOnce(&mut H)) {
        self.h.store(&self.s);
        f(self.h);
        self.s = self.h.load();
    }

    pub(crate) fn set_dirty(&mut self) {
        self.call(|h| h.set_dirty());
    }

    pub(crate) fn set_scroll_value(&mut self, v: c_ulong) {
        self.call(|h| h.set_scroll_value(v));
    }

    fn line(&self, i: c_ulong) -> Vec<u8> {
        self.h.line(i as usize)
    }

    /// `mLines[i].GetLength()`.
    pub(crate) fn len(&self, i: c_ulong) -> c_ulong {
        char_len(self.m, &self.line(i)) as c_ulong
    }

    pub(crate) fn last_line(&self) -> usize {
        self.h.line_count().wrapping_sub(1)
    }

    pub(crate) fn selection_changed(&mut self) {
        let f = &mut self.s.f;
        f.is_text_selected = f.selection_start_line != f.caret_line || f.selection_start_index != f.caret_index;
    }

    /// `AddChar`: inserts `input` at the caret. A single-line box takes characters up to `mMaxChars` (a numbers-only
    /// box drops non-digits but counts them); a multi-line box re-wraps the rest of the text at word breaks. Returns
    /// how far into `input` a single-line box read (the C++ returned that pointer).
    pub fn add_char(&mut self, input: &[u8]) -> usize {
        let m = self.m;
        let mut at = 0;
        let length = char_len(m, input);
        let mut spaces: c_ulong = 0;
        let mut new_line = false;
        if self.s.f.is_text_selected {
            self.delete_selected_text();
        }
        let line = self.line(self.s.f.caret_line);
        let line_length = char_len(m, &line) as c_ulong;
        if length == 0 {
            return at;
        }
        if self.s.f.is_single_line {
            let mut work = empty();
            let mut count = line_length;
            while count < self.s.f.max_chars && input[at] != 0 {
                if !self.s.f.numbers_only || is_digit_at(m, input, at) {
                    append(&mut work, &prefix(m, &input[at..], 1));
                }
                count += 1;
                at = m.inc(input, at);
            }
            let cut = offset(m, &line, idx(self.s.f.caret_index));
            append(&mut work, &line[cut..]);
            let mut line = from_bytes(&line[..cut]);
            append(&mut line, &work);
            self.h.set_line(self.s.f.caret_line as usize, &line);
            return at;
        }
        let mut line_index = self.s.f.caret_line;
        let mut work = prefix(m, &line, idx(self.s.f.caret_index));
        append(&mut work, input);
        append(&mut work, tail(m, &line, idx(self.s.f.caret_index)));
        let mut cursor = 0;
        while work[cursor] != 0 {
            let mut line = empty();
            let mut width = 0.0f32;
            let mut count: c_ulong = 0;
            let mut last_space: c_ulong = 0;
            let start = cursor;
            let mut wrap = false;
            loop {
                let mut ch;
                if spaces == 0 {
                    ch = prefix(m, &work[cursor..], 1);
                    if starts_with(&work, cursor, b'\n') {
                        new_line = true;
                        cursor = m.inc(&work, cursor);
                        append(&mut line, b"\n\0");
                    } else if is_space_at(m, &work, cursor) {
                        last_space = count + 1;
                    } else if starts_with(&work, cursor, b'\r') {
                        ch = empty();
                    } else if starts_with(&work, cursor, b'\t') {
                        spaces = 4;
                        ch = empty();
                    }
                } else {
                    ch = from_bytes(b" ");
                    spaces -= 1;
                }
                width += self.h.text_width(&ch);
                if width > self.s.text.width() || new_line {
                    wrap = true;
                    break;
                }
                append(&mut line, &ch);
                if spaces == 0 {
                    cursor = m.inc(&work, cursor);
                }
                count += 1;
                if !(work[cursor] != 0 && cursor != start) {
                    break;
                }
            }
            if !wrap {
                self.h.set_line(line_index as usize, &line);
                continue;
            }
            if last_space > 0 && !new_line && last_space < count + 1 {
                cursor = offset(m, &work, last_space as usize);
                self.h.set_line(line_index as usize, &prefix(m, &line, last_space as usize));
                if line_index == self.s.f.caret_line && self.s.f.caret_index > self.len(line_index) as c_long {
                    self.s.f.caret_index = self.s.f.caret_index.wrapping_sub(last_space as c_long);
                    self.s.f.caret_line += 1;
                }
            } else {
                self.h.set_line(line_index as usize, &line);
            }
            if line_index as usize == self.last_line() {
                self.h.insert_line(self.h.line_count(), &empty());
            }
            new_line = false;
            line_index += 1;
            let mut next = work[cursor..].to_vec();
            append(&mut next, &self.line(line_index));
            work = next;
            cursor = 0;
        }
        at
    }

    /// `BackSpaceChar`.
    pub fn back_space_char(&mut self) {
        if !self.s.f.is_single_line && self.s.f.caret_line != 0 {
            if self.s.f.caret_index == 0 {
                self.decrement_caret_pos();
                self.delete_char();
                self.compute_caret_location(true);
            } else {
                self.decrement_caret_pos();
                self.delete_char();
            }
        } else if self.s.f.caret_index > 0 {
            self.decrement_caret_pos();
            self.delete_char();
        }
    }

    /// `CalculateCaretFromMouseLocation` -> (line, index).
    pub fn caret_from_mouse(&mut self, at: UiPoint) -> (c_ulong, c_long) {
        let m = self.m;
        let text = self.s.text;
        let mut x = um_floor(text.left);
        let mut index = self.s.f.drawing_index;
        let mut out_line: c_ulong;
        if self.s.f.is_single_line {
            out_line = 0;
        } else {
            let value = self.h.scroll_value();
            out_line = if at.y < text.top {
                value as c_ulong
            } else if at.y > text.bottom {
                (value as c_ulong).wrapping_add(self.s.f.visible_line_count).wrapping_sub(1)
            } else {
                value.wrapping_add(um_floor((at.y - text.top) / self.h.font_height() as f32)) as c_ulong
            };
            if out_line as usize >= self.h.line_count() {
                out_line = self.last_line() as c_ulong;
            }
        }
        if at.x > text.left {
            let line = self.line(out_line);
            let mut cursor = offset(m, &line, idx(self.s.f.drawing_index));
            let length = if !self.s.f.is_single_line && has_line_end(m, &line[cursor..]) {
                (char_len(m, &line) as c_ulong).wrapping_sub(1) as c_long
            } else {
                char_len(m, &line) as c_long
            };
            loop {
                index = index.wrapping_add(1);
                if index > length {
                    break;
                }
                let ch = prefix(m, &line[cursor..], 1);
                x = x.wrapping_add(um_floor(self.h.text_width(&ch)));
                let right = x as f32;
                if right > text.right || right > at.x {
                    break;
                }
                cursor = m.inc(&line, cursor);
            }
            index = index.wrapping_sub(1);
        }
        (out_line, index)
    }

    /// `ChangeWidth`.
    pub fn change_width(&mut self, width: c_ulong) {
        let mut r = self.s.rect;
        r.right = r.left + width as f32 - 1.0;
        self.call(|h| h.base_set_rectangle(&r));
        let (rect, border) = (self.s.rect, self.s.border as f32);
        self.s.text.top = rect.top + border;
        self.s.text.left = rect.left + border;
        self.s.text.bottom = rect.bottom - border;
        self.s.text.right =
            if self.s.f.scroll_bar_visible { rect.right - border - SCROLL_BAR_WIDTH } else { rect.right - border };
    }

    /// `CheckScrollBar`: shows or hides the scroll bar as the line count crosses the visible line count.
    pub fn check_scroll_bar(&mut self) {
        if !self.h.has_scroll_bar() {
            return;
        }
        let lines = self.h.line_count();
        let visible = self.s.f.visible_line_count;
        if lines <= visible as usize {
            if self.h.scroll_max() > 0 {
                self.call(|h| h.set_scroll_range(0, 0));
                self.compute_caret_location(true);
            }
            if self.s.f.scroll_bar_visible {
                self.s.f.scroll_bar_visible = false;
                let r = self.s.rect;
                let narrow = UiRect::new(r.left, r.top, r.right - SCROLL_BAR_WIDTH, r.bottom);
                self.call(|h| h.set_rectangle(&narrow));
                self.s.text.right = self.s.rect.right - self.s.border as f32;
            }
        } else {
            let range = lines.wrapping_sub(visible as usize);
            if self.h.scroll_max() as isize as usize != range {
                self.call(|h| h.set_scroll_range(0, range as c_ulong));
                self.compute_caret_location(true);
            }
            if !self.s.f.scroll_bar_visible {
                self.s.f.scroll_bar_visible = true;
                let r = self.s.rect;
                let wide = UiRect::new(r.left, r.top, r.right + SCROLL_BAR_WIDTH, r.bottom);
                self.call(|h| h.set_rectangle(&wide));
            }
        }
    }

    /// `ComputeCaretLocation`: the caret's x (optionally remembered for up/down moves); scrolls to the caret line.
    pub fn compute_caret_location(&mut self, update_x: bool) {
        let at = self.location_from_index(self.s.f.caret_index as c_ulong, self.s.f.caret_line);
        if update_x {
            self.s.f.caret_x = at.x - self.s.text.left;
        }
        if !self.h.has_scroll_bar() {
            return;
        }
        let value = self.h.scroll_value() as c_ulong;
        let line = self.s.f.caret_line;
        if line < value {
            self.set_scroll_value(line);
        } else if line >= value.wrapping_add(self.s.f.visible_line_count) {
            self.set_scroll_value(line.wrapping_sub(self.s.f.visible_line_count).wrapping_add(1));
        }
    }

    /// `ComputeDrawingIndex`: the first character drawn, so that the text before the caret fits.
    pub fn compute_drawing_index(&mut self) {
        let m = self.m;
        let work = prefix(m, &self.line(self.s.f.caret_line), idx(self.s.f.caret_index));
        let mut cursor = 0;
        self.s.f.drawing_index = 0;
        while self.s.f.drawing_index < self.s.f.caret_index {
            if self.h.text_width(&work[cursor..]) < self.s.text.width() {
                break;
            }
            self.s.f.drawing_index += 1;
            cursor = m.inc(&work, cursor);
        }
    }

    /// `DecrementCaretPos`.
    pub fn decrement_caret_pos(&mut self) {
        self.s.f.caret_index = self.s.f.caret_index.wrapping_sub(1);
        if self.s.f.caret_index >= 0 {
            if self.s.f.drawing_index != 0 && self.s.f.caret_index <= self.s.f.drawing_index {
                self.scroll_left();
            } else {
                self.compute_caret_location(true);
            }
            self.set_dirty();
        } else if self.s.f.caret_line > 0 {
            self.s.f.caret_line -= 1;
            self.s.f.caret_index = self.len(self.s.f.caret_line).wrapping_sub(1) as c_long;
            self.compute_caret_location(true);
            self.set_dirty();
        } else {
            self.s.f.caret_index = 0;
        }
    }

    /// `DeleteChar`: the character after the caret.
    pub fn delete_char(&mut self) {
        let (index, line) = (self.s.f.caret_index, self.s.f.caret_line);
        self.increment_caret_pos(1);
        let (end_index, end_line) = (self.s.f.caret_index, self.s.f.caret_line);
        self.s.f.caret_index = index;
        self.s.f.caret_line = line;
        self.delete_range(index as c_ulong, line, end_index as c_ulong, end_line);
    }

    /// `DeleteLine`.
    pub fn delete_line(&mut self, line: c_ulong) {
        if self.s.f.caret_line > line {
            self.s.f.caret_line -= 1;
        } else if self.s.f.caret_line == line {
            if self.s.f.caret_line as usize == self.last_line() {
                self.s.f.caret_line = self.s.f.caret_line.wrapping_sub(1);
            }
            self.s.f.caret_index = self.len(self.s.f.caret_line) as c_long;
        }
        self.h.erase_line(line as usize);
        if !self.h.has_scroll_bar()
            || self.h.line_count() < self.s.f.visible_line_count as usize
            || self.h.scroll_value() <= 0
        {
            return;
        }
        let value = self.h.scroll_value().wrapping_sub(1);
        self.set_scroll_value(value as c_ulong);
        self.compute_caret_location(true);
        self.set_dirty();
    }

    /// The selection as (start index, start line, end index, end line), caret and anchor in text order.
    fn selection(&self) -> (c_ulong, c_ulong, c_ulong, c_ulong) {
        let f = &self.s.f;
        if f.caret_line == f.selection_start_line {
            if f.caret_index < f.selection_start_index {
                (f.caret_index as c_ulong, f.caret_line, f.selection_start_index as c_ulong, f.caret_line)
            } else {
                (f.selection_start_index as c_ulong, f.caret_line, f.caret_index as c_ulong, f.caret_line)
            }
        } else if f.caret_line < f.selection_start_line {
            (f.caret_index as c_ulong, f.caret_line, f.selection_start_index as c_ulong, f.selection_start_line)
        } else {
            (f.selection_start_index as c_ulong, f.selection_start_line, f.caret_index as c_ulong, f.caret_line)
        }
    }

    /// `DeleteSelectedText` (not while an IME composition is open).
    pub fn delete_selected_text(&mut self) {
        if !self.s.f.is_text_selected || self.s.f.ime_active {
            return;
        }
        self.s.f.is_text_selected = false;
        self.s.f.selecting = false;
        let (si, sl, ei, el) = self.selection();
        self.delete_range(si, sl, ei, el);
    }

    /// `DeleteText(start index, start line, end index, end line)`: deletes, re-wraps the following lines and puts the
    /// caret at the start of the gap.
    pub fn delete_range(&mut self, start_index: c_ulong, start_line: c_ulong, end_index: c_ulong, end_line: c_ulong) {
        let m = self.m;
        let (mut si, mut sl) = (start_index, start_line);
        let first = self.line(sl);
        if sl == end_line {
            let mut s = prefix(m, &first, si as usize);
            append(&mut s, tail(m, &first, end_index as usize));
            self.h.set_line(sl as usize, &s);
        } else {
            self.h.set_line(sl as usize, &prefix(m, &first, si as usize));
            if end_index == self.len(end_line) {
                self.delete_line(end_line);
            } else {
                let last = self.line(end_line);
                self.h.set_line(end_line as usize, tail(m, &last, end_index as usize));
                if self.s.f.caret_index > self.len(end_line) as c_long {
                    self.s.f.caret_index = self.len(end_line) as c_long;
                }
            }
        }
        if end_line > 0 {
            for _ in sl + 1..end_line {
                self.delete_line(sl + 1);
            }
        }
        if sl > 0 && !has_line_end(m, &self.line(sl - 1)) {
            sl -= 1;
            si = si.wrapping_add(self.len(sl));
        }
        let mut line = sl;
        while (line as usize + 1) < self.h.line_count() {
            if has_line_end(m, &self.line(line)) {
                break;
            }
            self.s.f.caret_line = line;
            self.s.f.caret_index = self.len(line) as c_long;
            let next = self.line(line + 1);
            self.delete_line(line + 1);
            self.add_char(&next);
            line += 1;
            if line as usize >= self.h.line_count() || next == self.line(line) {
                break;
            }
        }
        self.s.f.caret_index = 0;
        self.s.f.drawing_index = 0;
        self.s.f.caret_line = sl;
        self.increment_caret_pos(si as c_long);
        self.set_dirty();
        self.compute_caret_location(true);
    }

    /// `DeleteText()`: one empty line, the caret at the start, no selection.
    pub fn clear(&mut self) {
        self.s.f.is_text_selected = false;
        self.s.f.caret_index = 0;
        self.s.f.drawing_index = 0;
        self.s.f.caret_line = 0;
        self.h.keep_first_line();
        self.h.set_line(0, &empty());
        self.set_dirty();
    }

    /// `GetLocationFromIndex`: x from the caret line's text (stars for a password box) from the first drawn character,
    /// y from the line.
    pub fn location_from_index(&mut self, index: c_ulong, line: c_ulong) -> UiPoint {
        let m = self.m;
        let caret_line = self.line(self.s.f.caret_line);
        let base = if self.s.f.is_password_box { masked(char_len(m, &caret_line)) } else { caret_line };
        let text = tail(m, &base, idx(self.s.f.drawing_index));
        // the original's unsigned count wrapped for an index before the drawing index (module comment)
        let count = if (index as c_long) < self.s.f.drawing_index {
            0
        } else {
            index.wrapping_sub(self.s.f.drawing_index as c_ulong)
        };
        let shown = prefix(m, text, count as usize);
        let width = self.h.text_width(&shown);
        let mut at = UiPoint { x: width + self.s.text.left, y: self.s.text.top };
        if self.h.has_scroll_bar() {
            let rows =
                line.wrapping_sub(self.h.scroll_value() as c_ulong).wrapping_mul(self.h.font_height() as c_ulong);
            at.y = rows as f32 + self.s.text.top;
        }
        at
    }

    /// `GetSelectedText` (`None` without a selection: the C++ left its output alone).
    pub fn selected_text(&self) -> Option<Vec<u8>> {
        if !self.s.f.is_text_selected {
            return None;
        }
        let (si, sl, ei, el) = self.selection();
        Some(self.text(sl, el, si, ei))
    }

    /// `GetText(start line, end line, start index, end index)`.
    pub fn text(&self, start_line: c_ulong, end_line: c_ulong, start_index: c_ulong, end_index: c_ulong) -> Vec<u8> {
        let m = self.m;
        let first = self.line(start_line);
        if start_line == end_line {
            return prefix(m, tail(m, &first, start_index as usize), end_index.wrapping_sub(start_index) as usize);
        }
        let first_len = self.len(start_line);
        let mut out = if start_index == first_len {
            empty()
        } else {
            prefix(m, tail(m, &first, start_index as usize), first_len.wrapping_sub(start_index) as usize)
        };
        for i in start_line + 1..end_line {
            append(&mut out, &self.line(i));
        }
        let last = self.line(end_line);
        if end_index == self.len(end_line) {
            append(&mut out, &last);
        } else {
            append(&mut out, &prefix(m, &last, end_index as usize));
        }
        out
    }

    /// `IncrementCaretPos`: right by `count` characters across lines; scrolls right past the edge.
    pub fn increment_caret_pos(&mut self, count: c_long) {
        let mut count = count;
        while count > 0 {
            let index = self.s.f.caret_index.wrapping_add(count);
            let length = self.len(self.s.f.caret_line);
            let more_lines = (self.s.f.caret_line as usize) < self.last_line();
            if (index as c_ulong) < length {
                self.s.f.caret_index = index;
                break;
            }
            if index as c_ulong == length {
                if more_lines {
                    self.s.f.caret_line += 1;
                    self.s.f.caret_index = 0;
                } else {
                    self.s.f.caret_index = index;
                }
                break;
            }
            if more_lines {
                count = count.wrapping_add((self.s.f.caret_index as c_ulong).wrapping_sub(length) as c_long);
                self.s.f.caret_index = 0;
                self.s.f.caret_line += 1;
            } else {
                self.s.f.caret_index = length as c_long;
                break;
            }
        }
        let at = self.location_from_index(self.s.f.caret_index as c_ulong, self.s.f.caret_line);
        if at.x > self.s.text.right {
            self.scroll_right(count);
        }
        self.compute_caret_location(true);
        self.set_dirty();
    }

    /// `ProcessDownKey`.
    pub fn down_key(&mut self) {
        if (self.s.f.caret_line as usize) < self.last_line() {
            self.s.f.caret_line += 1;
            self.index_from_up_down_offset();
        }
        self.compute_caret_location(false);
    }

    /// `ProcessEndKey`: the end of the line, before its line break.
    pub fn end_key(&mut self) {
        let line = self.line(self.s.f.caret_line);
        self.s.f.caret_index = char_len(self.m, &line) as c_long;
        if has_line_end(self.m, &line) {
            self.s.f.caret_index -= 1;
        }
        if self.s.f.is_single_line {
            self.compute_drawing_index();
        }
        self.compute_caret_location(true);
        self.set_dirty();
    }

    /// `ProcessEnterKey`: a multi-line box breaks the line at the caret; a single-line box fires Enter.
    pub fn enter_key(&mut self) {
        let m = self.m;
        if self.s.f.is_single_line {
            self.call(|h| h.fire(TRIGGER_ENTER_PRESSED));
            return;
        }
        if self.s.f.is_text_selected {
            self.delete_selected_text();
        }
        let caret_line = self.s.f.caret_line;
        if caret_line as usize == self.last_line() {
            self.h.insert_line(caret_line as usize + 1, &empty());
        }
        let line = self.line(caret_line);
        let rest = if char_len(m, &line) > 0 {
            let at = idx(self.s.f.caret_index);
            let mut broken = prefix(m, &line, at);
            append(&mut broken, b"\n\0");
            self.h.set_line(caret_line as usize, &broken);
            tail(m, &line, at).to_vec()
        } else {
            self.h.set_line(caret_line as usize, b"\n\0");
            empty()
        };
        self.s.f.caret_line += 1;
        self.s.f.caret_index = 0;
        self.add_char(&rest);
        self.compute_caret_location(true);
    }

    /// `ProcessHomeKey`.
    pub fn home_key(&mut self) {
        self.s.f.drawing_index = 0;
        self.s.f.caret_index = 0;
        self.compute_caret_location(true);
        self.set_dirty();
    }

    /// `ProcessUpKey`.
    pub fn up_key(&mut self) {
        if self.s.f.caret_line > 0 {
            self.s.f.caret_line -= 1;
            self.index_from_up_down_offset();
        }
        self.compute_caret_location(false);
    }

    /// `ScrollLeft`: a single-line box shows `mCharScroll` characters more before the caret.
    pub fn scroll_left(&mut self) {
        self.s.f.drawing_index = self.s.f.caret_index;
        let mut i = self.s.f.char_scroll.wrapping_sub(1);
        while i > 0 {
            self.s.f.drawing_index = self.s.f.drawing_index.wrapping_sub(1);
            let at = self.location_from_index(self.s.f.caret_index as c_ulong, self.s.f.caret_line);
            if self.s.f.drawing_index < 0 || self.s.text.right == at.x {
                self.s.f.drawing_index += 1;
                break;
            }
            i -= 1;
        }
    }

    /// `ScrollRight` (it moves the caret to line 0 on the way, as the C++ did).
    pub fn scroll_right(&mut self, count: c_long) {
        let index = self.s.f.caret_index;
        self.s.f.caret_index = self.s.f.caret_index.wrapping_add(count);
        let length = self.len(self.s.f.caret_line) as c_long;
        if self.s.f.caret_index > length {
            self.s.f.caret_index = length;
        }
        self.set_caret_location(self.s.f.caret_index as c_ulong, 0);
        self.s.f.caret_index = index;
        if self.s.f.caret_index < self.s.f.drawing_index {
            self.s.f.caret_index = self.s.f.drawing_index;
        }
    }

    /// `SelectText()`: all of it.
    pub fn select_all(&mut self) {
        if !self.s.f.ime_active && self.h.line_count() > 0 {
            let last = self.last_line() as c_ulong;
            self.select(0, self.len(last), 0, last);
        }
    }

    /// `SelectText(caret index, anchor index, caret line, anchor line)`.
    pub fn select(&mut self, caret_index: c_ulong, start_index: c_ulong, caret_line: c_ulong, start_line: c_ulong) {
        if self.s.f.ime_active {
            return;
        }
        self.s.f.caret_index = caret_index as c_long;
        self.s.f.caret_line = caret_line;
        self.s.f.selection_start_index = start_index as c_long;
        self.s.f.selection_start_line = start_line;
        if self.s.f.is_single_line {
            self.compute_drawing_index();
        }
        self.compute_caret_location(true);
        self.selection_changed();
        self.set_dirty();
    }

    /// `SetCaretLocation`.
    pub fn set_caret_location(&mut self, index: c_ulong, line: c_ulong) {
        self.s.f.caret_line = line;
        self.s.f.caret_index = index as c_long;
        self.compute_drawing_index();
        self.compute_caret_location(true);
        self.set_dirty();
    }

    /// `SetIndexFromUpDownOffset`: the character on the new line nearest the remembered x.
    pub fn index_from_up_down_offset(&mut self) {
        let m = self.m;
        let line = self.line(self.s.f.caret_line);
        let mut cursor = offset(m, &line, idx(self.s.f.drawing_index));
        let mut width = 0.0f32;
        let mut length = char_len(m, &line) as c_long;
        if has_line_end(m, &line[cursor..]) {
            length -= 1;
        }
        self.s.f.caret_index = self.s.f.drawing_index;
        while self.s.f.caret_index < length {
            let ch = prefix(m, &line[cursor..], 1);
            width += self.h.text_width(&ch);
            if width > self.s.f.caret_x {
                break;
            }
            cursor = m.inc(&line, cursor);
            self.s.f.caret_index += 1;
        }
        self.set_dirty();
    }

    /// `SetLocation`: moves the box, keeping the text area's size, and the scroll bar with it.
    pub fn set_location(&mut self, at: UiPoint) {
        let width = um_floor(self.s.text.width()) as c_ulong;
        let height = um_floor(self.s.text.height()) as c_ulong;
        self.call(|h| h.base_set_location(at));
        let (rect, border) = (self.s.rect, self.s.border as f32);
        self.s.text.top = rect.top + border;
        self.s.text.left = rect.left + border;
        self.s.text.right = self.s.text.left + width as f32;
        self.s.text.bottom = self.s.text.top + height as f32;
        if self.h.has_scroll_bar() {
            let corner = UiPoint { x: self.s.text.right, y: self.s.text.top };
            self.call(|h| h.scroll_set_location(corner));
        }
    }

    /// `SetText`: the whole text, the caret at the start.
    pub fn set_text(&mut self, text: &[u8]) {
        self.clear();
        self.add_char(text);
        self.s.f.caret_line = 0;
        self.set_caret_location(0, 0);
        self.set_dirty();
    }

    /// `OpenIMECompositionString`: starts a composition at the caret (deleting a selection); a numbers-only box
    /// refuses.
    pub fn ime_open(&mut self) -> c_long {
        if self.s.f.numbers_only {
            return ERROR_FAILED;
        }
        if !self.s.f.ime_active {
            self.delete_selected_text();
            self.s.f.ime_start_index = self.s.f.caret_index as c_ulong;
            self.s.f.ime_active = true;
            self.s.f.ime_start_line = self.s.f.caret_line;
        }
        ERROR_OK
    }

    /// `CloseIMECompositionString`.
    pub fn ime_close(&mut self) -> c_long {
        if self.s.f.ime_active {
            self.ime_remove();
            self.s.f.ime_active = false;
        }
        ERROR_OK
    }

    /// `ChangeIMECompositionString` without the attributes (the C++ keeps `mIMEAttributes`): replaces the
    /// composition and puts the caret `cursor` bytes into it.
    pub fn ime_change(&mut self, text: &[u8], cursor: c_ulong) -> c_long {
        if self.s.f.ime_active {
            self.ime_remove();
            self.add_char(text);
            self.s.f.ime_start_index = self.s.f.caret_index as c_ulong;
            self.s.f.ime_start_line = self.s.f.caret_line;
            self.s.f.ime_composition_length = char_len(self.m, text) as c_ulong;
            self.increment_caret_pos(chars_in_bytes(self.m, text, cursor as usize) as c_long);
        }
        ERROR_OK
    }

    /// `RemoveIMECompositionString`.
    pub fn ime_remove(&mut self) {
        self.set_caret_location(self.s.f.ime_start_index, self.s.f.ime_start_line);
        while self.s.f.ime_composition_length > 0 {
            self.delete_char();
            self.s.f.ime_composition_length -= 1;
        }
    }

    /// `UpdateTranslatedKey`'s typing: `key` `repeat` times at the caret, or into the composition.
    pub fn type_key(&mut self, key: &[u8], repeat: c_short) {
        for _ in 0..repeat.max(0) {
            if !self.s.f.ime_active {
                self.add_char(key);
                self.increment_caret_pos(1);
            } else {
                self.set_caret_location(self.s.f.ime_start_index, self.s.f.ime_start_line);
                self.add_char(key);
                self.increment_caret_pos(1);
                self.s.f.ime_start_index = self.s.f.caret_index as c_ulong;
                self.s.f.ime_start_line = self.s.f.caret_line;
            }
        }
    }
}

#[cfg(test)]
mod tests;
