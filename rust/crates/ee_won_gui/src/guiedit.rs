//! WONGUI's single-line text editing (InputBox.cpp): the selection model and the edits' arithmetic. The C++ keeps
//! the box, its `GUIString`s (the text, the password text, the history), the font, the clipboard, the events and
//! the paint; these answer:
//!
//! - where the cursor goes one character or one word left or right (`MoveCursor`);
//! - a selected range (`SelectRange`), the Home, End, Left and Right keys' selection (`KeyDown`), what
//!   Backspace and Delete keep (`KeyDown`);
//! - a pasted or typed text's cut at its first control character and its trim to the box's limit
//!   (`SetSelText`);
//! - the cursor's blink (`TimerEvent`, `SetCursorBlinkRate`);
//! - the character under the mouse from the font's widths (`CheckSel`) and the selection it extends;
//! - the horizontal scroll that keeps the cursor in view (`AdjustPosition`) and the drag scroll
//!   (`TimerEvent`).
//!
//! The text's units are `wchar_t`s (16 bits on Windows, 32 elsewhere); positions are the original's `int`s.

/// `MoveCursor`: one unit left or right, or (`whole_word`) past the word and then the spaces in that direction
/// (left: back to the word's start), clamped to [0, length]. Only the space (U+0020) separates words.
pub fn move_cursor(left: bool, whole_word: bool, text: &[u32], pos: i32) -> i32 {
    let dx = if left { -1 } else { 1 };
    let len = text.len() as i32;
    let mut p = pos.wrapping_add(dx);
    if whole_word {
        while p >= 0 && p < len && text[p as usize] != u32::from(b' ') {
            p += dx;
        }
        while p >= 0 && p < len && text[p as usize] == u32::from(b' ') {
            p += dx;
        }
        if left {
            p += 1;
        }
    }
    p.clamp(0, len.max(0))
}

/// A selection: its start and end (start <= end once normalised) and whether the cursor is at the start.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonSel {
    pub start: i32,
    pub end: i32,
    pub backwards: bool,
}

/// `SelectRange`: a negative or too large end is the text's end; a reversed range selects backwards.
pub fn select_range(start: i32, end: i32, len: i32) -> EeWonSel {
    let mut s = if start < 0 { len } else { start };
    let mut e = if end < 0 { len } else { end };
    if s > len {
        s = len;
    }
    if e > len {
        e = len;
    }
    let backwards = s > e;
    if backwards {
        std::mem::swap(&mut s, &mut e);
    }
    EeWonSel { start: s, end: e, backwards }
}

/// `KeyDown`'s Home (36), End (35), Left (37) and Right (39): the selection after the key (`shift` extends it,
/// `ctrl` moves by words). `None` for another key.
pub fn key_selection(key: i32, shift: bool, ctrl: bool, sel: EeWonSel, text: &[u32]) -> Option<EeWonSel> {
    let len = text.len() as i32;
    let EeWonSel { mut start, mut end, mut backwards } = sel;
    match key {
        36 => {
            if !shift {
                end = 0;
            } else if !backwards {
                end = start;
            }
            start = 0;
            backwards = start != end;
        }
        35 => {
            if !shift {
                start = len;
            } else if backwards {
                start = end;
            }
            backwards = false;
            end = len;
        }
        37 | 39 => {
            let left = key == 37;
            if !shift && start != end {
                if left {
                    end = start;
                } else {
                    start = end;
                }
            } else if backwards {
                start = move_cursor(left, ctrl, text, start);
                if !shift {
                    end = start;
                }
            } else {
                end = move_cursor(left, ctrl, text, end);
                if !shift {
                    start = end;
                }
            }
            if start > end {
                backwards = !backwards;
                std::mem::swap(&mut start, &mut end);
            } else if start == end {
                backwards = false;
            }
        }
        _ => return None,
    }
    Some(EeWonSel { start, end, backwards })
}

/// What Backspace (8) or Delete (46) keeps of the text: the left part `assign(text, 0, left_count)` when
/// `left_count` is not negative (else empty) and the right part `assign(text, right_start)` when
/// `right_start` is not negative (else empty), as the original assigned them. The cursor then sits at the left
/// part's length.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonDeleteParts {
    pub left_count: i32,
    pub right_start: i32,
}

pub fn delete_parts(backspace: bool, start: i32, end: i32, len: i32) -> EeWonDeleteParts {
    if start != end {
        EeWonDeleteParts { left_count: start, right_start: end }
    } else if backspace {
        EeWonDeleteParts { left_count: if start > 0 { start.wrapping_sub(1) } else { -1 }, right_start: end }
    } else {
        EeWonDeleteParts { left_count: start, right_start: if end < len { end.wrapping_add(1) } else { -1 } }
    }
}

/// `SetSelText`'s cut: the length up to the first unit below 32 (as the original's `int` of a `wchar_t`).
pub fn control_cut(text: &[u32]) -> usize {
    text.iter().position(|&u| (u as i32) < 32).unwrap_or(text.len())
}

/// `SetSelText`'s trim: the number of pasted units the limit allows (`None`: no trim), from the box's text
/// length, the paste's and the selection.
pub fn paste_allowed(text_len: i32, paste_len: i32, start: i32, end: i32, max_chars: i32) -> Option<i32> {
    let sel = end.wrapping_sub(start);
    if max_chars > 0 && text_len.wrapping_add(paste_len).wrapping_sub(sel) > max_chars {
        Some(max_chars.wrapping_sub(text_len).wrapping_add(sel).max(0))
    } else {
        None
    }
}

/// The cursor's blink state.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonBlink {
    pub delay: i32,
    pub on: bool,
    /// The cursor changed (repaint).
    pub changed: bool,
}

/// `TimerEvent`'s blink: the delay grows by the time passed; past the on (off) time the cursor turns off (on)
/// and the delay keeps the remainder. A zero time with a positive delay divides by zero, as the original did.
pub fn blink(delay: i32, delta: i32, on: bool, on_time: i32, off_time: i32) -> EeWonBlink {
    let mut d = delay.wrapping_add(delta);
    if on && d > on_time {
        d = d.wrapping_rem(on_time);
        EeWonBlink { delay: d, on: false, changed: true }
    } else if !on && d > off_time {
        d = d.wrapping_rem(off_time);
        EeWonBlink { delay: d, on: true, changed: true }
    } else {
        EeWonBlink { delay: d, on, changed: false }
    }
}

/// `SetCursorBlinkRate`: (on, off); a non-positive off time is the on time.
pub fn blink_rate(on_time: i32, off_time: i32) -> (i32, i32) {
    (on_time, if off_time <= 0 { on_time } else { off_time })
}

/// `CheckSel`'s character: from `sel_char` (the cursor's character) whose prefix is `prefix_width` wide, back
/// while the prefix reaches `x`, or forward while it does not, by the characters' widths (`widths[i]` the width
/// of character i); clamped to [0, length]. `x` is already the content's x.
pub fn char_at(x: i32, prefix_width: i32, sel_char: i32, widths: &[i32]) -> i32 {
    let len = widths.len() as i32;
    let mut c = sel_char;
    let mut w = prefix_width;
    // a character outside the text has no width the original could read; the C++ passes widths for every
    // character, and this treats a position outside them as the end of the walk
    let width_of = |i: i32| if i >= 0 && i < len { widths[i as usize] } else { 0 };
    if w >= x {
        c = c.wrapping_sub(1);
        while c >= 0 && w >= x {
            w = w.wrapping_sub(width_of(c));
            if w >= x {
                c -= 1;
            }
        }
        c = c.wrapping_add(1);
    } else {
        while c < len && w < x {
            w = w.wrapping_add(width_of(c));
            c = c.wrapping_add(1);
        }
    }
    c.clamp(0, len.max(0))
}

/// `CheckSel`'s mouse x in the content: clamped to [0, width], plus the scroll, minus the text's left.
pub fn sel_x(x: i32, width: i32, horz_offset: i32, text_left: i32) -> i32 {
    let x = if x < 0 {
        0
    } else if x > width {
        width
    } else {
        x
    };
    x.wrapping_add(horz_offset.wrapping_sub(text_left))
}

/// `CheckSel`'s selection with the character found: a new start collapses it there, else the cursor's end moves
/// and a crossed selection flips.
pub fn extend_selection(c: i32, new_start: bool, sel: EeWonSel) -> EeWonSel {
    if new_start {
        return EeWonSel { start: c, end: c, backwards: false };
    }
    let EeWonSel { mut start, mut end, mut backwards } = sel;
    if backwards {
        start = c;
    } else {
        end = c;
    }
    if start > end {
        backwards = !backwards;
        std::mem::swap(&mut start, &mut end);
    }
    EeWonSel { start, end, backwards }
}

/// `AdjustPosition`: the scroll that keeps the cursor (its prefix `pre_width` wide) in the text rectangle
/// (`rect_width`): back by ten 'M's when it is left of the view, or to the cursor plus an 'M' past the right.
pub fn cursor_scroll(pre_width: i32, m_width: i32, horz_offset: i32, rect_width: i32) -> i32 {
    let mut o = horz_offset;
    if pre_width.wrapping_sub(o) < 0 {
        o = pre_width.wrapping_sub(m_width.wrapping_mul(10));
        if o < 0 {
            o = 0;
        }
    }
    if pre_width.wrapping_sub(o) >= rect_width && rect_width != 0 && pre_width.wrapping_sub(o) != 0 {
        o = pre_width.wrapping_sub(rect_width).wrapping_add(m_width);
    }
    o
}

/// `TimerEvent`'s drag scroll before the selection moves: `None` while the mouse is inside the box (nothing
/// happens), else the scroll towards the mouse (right only while the cursor is before the end, left only while
/// after the start).
pub fn drag_scroll(x: i32, y: i32, width: i32, height: i32, char_pos: i32, len: i32, horz_offset: i32) -> Option<i32> {
    if x >= 0 && x <= width && y >= 0 && y <= height {
        return None;
    }
    let mut o = horz_offset;
    if x > width && char_pos != len {
        o = o.wrapping_add(x.wrapping_sub(width));
        if o < 0 {
            o = 0;
        }
    } else if x < 0 && char_pos != 0 {
        o = o.wrapping_add(x);
    }
    Some(o)
}

/// `TimerEvent`'s drag scroll after the selection moved: at the end, the whole text shown with an 'M' to spare
/// (`text_width` the text's width, `m_width` the string "M"'s; read only at the end); at the start, 0; never
/// negative.
pub fn drag_scroll_end(
    char_pos: i32,
    len: i32,
    text_width: i32,
    m_width: i32,
    rect_width: i32,
    horz_offset: i32,
) -> i32 {
    let mut o = horz_offset;
    if char_pos == len && text_width.wrapping_add(o) > rect_width {
        o = text_width.wrapping_sub(rect_width).wrapping_add(m_width);
    }
    if char_pos == 0 {
        o = 0;
    }
    if o < 0 {
        o = 0;
    }
    o
}

// ---- the C ABI

unsafe fn units<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: `n` entries (the callers' contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// A `GUIString`'s units (`wchar_size` bytes each) widened.
unsafe fn wide(text: *const std::ffi::c_void, len: u16, wchar_size: u32) -> Vec<u32> {
    let n = usize::from(len);
    if wchar_size == 2 {
        // SAFETY: `len` units (the callers' contract)
        unsafe { units(text.cast::<u16>(), n) }.iter().map(|&u| u32::from(u)).collect()
    } else {
        // SAFETY: `len` units (the callers' contract)
        unsafe { units(text.cast::<u32>(), n) }.to_vec()
    }
}

/// `move_cursor` over a `GUIString`'s units.
///
/// # Safety
/// `text` holds `len` units of `wchar_size` bytes (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_move_cursor(
    left: bool,
    whole_word: bool,
    text: *const std::ffi::c_void,
    len: u16,
    wchar_size: u32,
    pos: i32,
) -> i32 {
    // SAFETY: the contract
    move_cursor(left, whole_word, &unsafe { wide(text, len, wchar_size) }, pos)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_select_range(start: i32, end: i32, len: i32) -> EeWonSel {
    select_range(start, end, len)
}

/// `key_selection` in place: false (the selection unchanged) for a key it does not handle.
///
/// # Safety
/// `sel` is valid; `text` holds `len` units of `wchar_size` bytes (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_key_selection(
    key: i32,
    shift: bool,
    ctrl: bool,
    sel: *mut EeWonSel,
    text: *const std::ffi::c_void,
    len: u16,
    wchar_size: u32,
) -> bool {
    // SAFETY: the contract
    let t = unsafe { wide(text, len, wchar_size) };
    // SAFETY: valid (the contract)
    let s = unsafe { &mut *sel };
    match key_selection(key, shift, ctrl, *s, &t) {
        Some(n) => {
            *s = n;
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_delete_parts(backspace: bool, start: i32, end: i32, len: i32) -> EeWonDeleteParts {
    delete_parts(backspace, start, end, len)
}

/// `control_cut` over a `GUIString`'s units.
///
/// # Safety
/// `text` holds `len` units of `wchar_size` bytes (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_control_cut(text: *const std::ffi::c_void, len: u16, wchar_size: u32) -> i32 {
    // SAFETY: the contract
    control_cut(&unsafe { wide(text, len, wchar_size) }) as i32
}

/// `paste_allowed`: the allowed count, or -1 for no trim.
#[no_mangle]
pub extern "C" fn ee_won_gui_paste_allowed(text_len: i32, paste_len: i32, start: i32, end: i32, max_chars: i32) -> i32 {
    paste_allowed(text_len, paste_len, start, end, max_chars).unwrap_or(-1)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_blink(delay: i32, delta: i32, on: bool, on_time: i32, off_time: i32) -> EeWonBlink {
    blink(delay, delta, on, on_time, off_time)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_blink_off_time(on_time: i32, off_time: i32) -> i32 {
    blink_rate(on_time, off_time).1
}

/// `sel_x`, then `char_at` with `count` widths, then `extend_selection` into `sel`.
///
/// # Safety
/// `widths` holds `count` ints (or is NULL with 0); `sel` is valid.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_check_sel(
    x: i32,
    width: i32,
    horz_offset: i32,
    text_left: i32,
    prefix_width: i32,
    sel_char: i32,
    widths: *const i32,
    count: usize,
    new_start: bool,
    sel: *mut EeWonSel,
) {
    let cx = sel_x(x, width, horz_offset, text_left);
    // SAFETY: the contract
    let c = char_at(cx, prefix_width, sel_char, unsafe { units(widths, count) });
    // SAFETY: valid (the contract)
    let s = unsafe { &mut *sel };
    *s = extend_selection(c, new_start, *s);
}

/// `sel_x`: the content x `CheckSel` measures against.
#[no_mangle]
pub extern "C" fn ee_won_gui_sel_x(x: i32, width: i32, horz_offset: i32, text_left: i32) -> i32 {
    sel_x(x, width, horz_offset, text_left)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_cursor_scroll(pre_width: i32, m_width: i32, horz_offset: i32, rect_width: i32) -> i32 {
    cursor_scroll(pre_width, m_width, horz_offset, rect_width)
}

/// `drag_scroll`: true with the offset in `out`, false while the mouse is inside the box.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_drag_scroll(
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    char_pos: i32,
    len: i32,
    horz_offset: i32,
    out: *mut i32,
) -> bool {
    match drag_scroll(x, y, width, height, char_pos, len, horz_offset) {
        Some(o) => {
            // SAFETY: writable (the contract)
            unsafe { *out = o };
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_drag_scroll_end(
    char_pos: i32,
    len: i32,
    text_width: i32,
    m_width: i32,
    rect_width: i32,
    horz_offset: i32,
) -> i32 {
    drag_scroll_end(char_pos, len, text_width, m_width, rect_width, horz_offset)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn t(s: &str) -> Vec<u32> {
        s.chars().map(u32::from).collect()
    }

    #[test]
    fn cursor() {
        let s = t("ab  cd ef");
        assert_eq!(move_cursor(false, false, &s, 9), 9);
        assert_eq!(move_cursor(true, false, &s, 0), 0);
        assert_eq!(move_cursor(false, true, &s, 0), 4);
        assert_eq!(move_cursor(false, true, &s, 4), 7);
        assert_eq!(move_cursor(false, true, &s, 7), 9);
        // the original's word left stops after the space before the word it crossed
        assert_eq!(move_cursor(true, true, &s, 9), 6);
        assert_eq!(move_cursor(true, true, &s, 7), 6);
        assert_eq!(move_cursor(true, true, &s, 4), 2);
        assert_eq!(move_cursor(true, true, &s, 3), 2);
        assert_eq!(move_cursor(true, true, &s, 1), 0);
        assert_eq!(select_range(-1, 2, 5), EeWonSel { start: 2, end: 5, backwards: true });
        assert_eq!(select_range(0, 99, 5), EeWonSel { start: 0, end: 5, backwards: false });
    }

    #[test]
    fn keys() {
        let s = t("hello world");
        let sel = |start, end, backwards| EeWonSel { start, end, backwards };
        assert_eq!(key_selection(36, false, false, sel(3, 5, false), &s), Some(sel(0, 0, false)));
        assert_eq!(key_selection(36, true, false, sel(3, 5, false), &s), Some(sel(0, 3, true)));
        assert_eq!(key_selection(35, true, false, sel(3, 5, true), &s), Some(sel(5, 11, false)));
        assert_eq!(key_selection(37, false, false, sel(3, 5, false), &s), Some(sel(3, 3, false)));
        assert_eq!(key_selection(39, true, false, sel(3, 3, false), &s), Some(sel(3, 4, false)));
        assert_eq!(key_selection(37, true, false, sel(3, 3, false), &s), Some(sel(2, 3, true)));
        assert_eq!(key_selection(39, false, true, sel(0, 0, false), &s), Some(sel(6, 6, false)));
        assert_eq!(key_selection(40, false, false, sel(0, 0, false), &s), None);
        assert_eq!(delete_parts(true, 3, 3, 5), EeWonDeleteParts { left_count: 2, right_start: 3 });
        assert_eq!(delete_parts(true, 0, 0, 5), EeWonDeleteParts { left_count: -1, right_start: 0 });
        assert_eq!(delete_parts(false, 5, 5, 5), EeWonDeleteParts { left_count: 5, right_start: -1 });
        assert_eq!(delete_parts(false, 1, 4, 5), EeWonDeleteParts { left_count: 1, right_start: 4 });
    }

    #[test]
    fn edits() {
        assert_eq!(control_cut(&t("ab\tc")), 2);
        assert_eq!(control_cut(&t("abc")), 3);
        assert_eq!(control_cut(&[0x41, 0x8000_0000]), 1);
        assert_eq!(paste_allowed(5, 3, 1, 1, 6), Some(1));
        assert_eq!(paste_allowed(5, 3, 0, 5, 6), None);
        assert_eq!(paste_allowed(9, 3, 1, 1, 6), Some(0));
        assert_eq!(paste_allowed(9, 3, 1, 1, 0), None);
        assert_eq!(blink(100, 450, true, 500, 300), EeWonBlink { delay: 50, on: false, changed: true });
        assert_eq!(blink(0, 300, false, 500, 300), EeWonBlink { delay: 300, on: false, changed: false });
        assert_eq!(blink_rate(500, 0), (500, 500));
    }

    #[test]
    fn mouse_and_scroll() {
        let w = [5, 5, 5, 5];
        assert_eq!(char_at(12, 20, 4, &w), 3);
        assert_eq!(char_at(0, 20, 4, &w), 0);
        assert_eq!(char_at(12, 0, 0, &w), 3);
        assert_eq!(char_at(100, 0, 0, &w), 4);
        assert_eq!(sel_x(-5, 100, 10, 3), 7);
        assert_eq!(sel_x(500, 100, 10, 3), 107);
        let sel = |start, end, backwards| EeWonSel { start, end, backwards };
        assert_eq!(extend_selection(1, false, sel(3, 5, false)), sel(1, 3, true));
        assert_eq!(extend_selection(4, true, sel(3, 5, false)), sel(4, 4, false));
        assert_eq!(cursor_scroll(20, 8, 30, 100), 0);
        assert_eq!(cursor_scroll(100, 8, 0, 90), 18);
        assert_eq!(cursor_scroll(100, 8, 0, 0), 0);
        assert_eq!(drag_scroll(5, 5, 10, 10, 2, 4, 0), None);
        assert_eq!(drag_scroll(15, 5, 10, 10, 2, 4, 3), Some(8));
        assert_eq!(drag_scroll(-4, 5, 10, 10, 2, 4, 3), Some(-1));
        assert_eq!(drag_scroll(15, 5, 10, 10, 4, 4, 3), Some(3));
        assert_eq!(drag_scroll_end(4, 4, 120, 9, 100, 0), 29);
        assert_eq!(drag_scroll_end(0, 4, 120, 9, 100, -3), 0);
        assert_eq!(drag_scroll_end(2, 4, 120, 9, 100, -3), 0);
    }
}
