//! WONGUI's InputBox as a whole Rust object (R6 WON round 22): the lobby's chat line, the login, password,
//! account and port boxes and every combo box's edit line. The original's InputBox.cpp and
//! `ScreenSelectionState.cpp`'s SetAllowSelection, with their state: the shown and the real (password) text, the
//! selection and its drag, the horizontal scroll, the cursor's blink, the padding and text rectangle, the input
//! history, the limits.
//!
//! The C++ `InputBox` stays the type the lobby holds (a `Component` in the tree, its vtable, the subclasses' frames
//! and paints over it); its virtuals and methods are one-line forwarders to `ee_won_input_call`, and every call out
//! is an op of the component op table (`guihost`, round 21). The C++ keeps what the configs and the lobby set or
//! read: the font, the icon, the input filter, the colours, the selection colours, the backgrounds, and a copy of
//! the shown and the real text (`GetText()` hands out a reference: the Rust object stores both after each change,
//! `TextStore`; the font's measures run over the shown copy).
//!
//! Every method is the original's statements in order with the calls out at the same points (`guiedit` and
//! `guidraw` keep the decisions, as before). The state is behind a `RefCell` never borrowed across a call out: a
//! fired event runs the lobby's handler (the chat line's return clears the box and adds to its history).
//!
//! One deliberate difference: with an input history limit of 0 the original's trim popped an empty deque (UB);
//! here the trim stops when the history is empty.

use std::cell::RefCell;
use std::collections::VecDeque;
use std::ffi::c_void;

use crate::guidraw::{self, EeWonDrawOp, EeWonInputPaint, EeWonTextMeasure};
use crate::guiedit::{self, EeWonSel};
use crate::guihost::{
    EeWonGuiCursor as Cursor, EeWonGuiEvent as Event, EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op,
};

/// The bytes of a `wchar_t` (the units the C++ hands in).
const WCHAR: usize = if cfg!(windows) { 2 } else { 4 };
#[cfg(test)]
pub(crate) const WCHAR_FOR_TESTS: usize = WCHAR;

/// A character stored in a `wchar_t` (`mText[i] = character`).
pub(crate) fn wunit(ch: i32) -> u32 {
    if WCHAR == 2 {
        u32::from(ch as u16)
    } else {
        ch as u32
    }
}

/// The units of a C++ text (`len` `wchar_t`s at `p`).
///
/// # Safety
/// `p` holds `len` `wchar_t`s, or `len` is 0.
pub(crate) unsafe fn units_in(p: isize, len: isize) -> Vec<u32> {
    if p == 0 || len <= 0 {
        return Vec::new();
    }
    let n = len as usize;
    if WCHAR == 2 {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(p as *const u16, n) }.iter().map(|&u| u32::from(u)).collect()
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(p as *const u32, n) }.to_vec()
    }
}

/// `int < size_t` as the original compared them (the int converted).
fn lt_size(i: i32, n: usize) -> bool {
    (i as isize as usize) < n
}

/// The inward calls (the C++ shell's virtuals and methods). Texts are `wchar_t` units: a pointer and a length.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonInputCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `GetDesiredSize(*a, *b)` (`a`, `b` point at i32s).
    DesiredSize,
    GotFocus,
    LostFocus,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseUp(a, b, c)`.
    MouseUp,
    MouseExit,
    /// `MouseDrag(a, b)`.
    MouseDrag,
    /// `TimerEvent(a)` (0 or 1).
    TimerEvent,
    /// `KeyChar(a)` (0 or 1).
    KeyChar,
    /// `KeyDown(a)` (0 or 1).
    KeyDown,
    /// `Paint(*a)` (`a` the Graphics).
    Paint,
    /// `SetText(text a, b, c != 0)`.
    SetText,
    /// `SetPasswordChar(a)` (a `char`, sign-extended).
    SetPasswordChar,
    /// `SetAllowSelection(a != 0)`.
    SetAllowSelection,
    /// `SelectRange(a, b)`.
    SelectRange,
    /// `SetIcon`'s steps after the C++ set the icon.
    IconChanged,
    /// `SetCharWidth(a)`.
    SetCharWidth,
    /// `SetCursorBlinkRate(a, b)`.
    SetCursorBlinkRate,
    /// `AdjustPosition()`.
    AdjustPosition,
    /// `CopySelection()`.
    CopySelection,
    /// `SetSelText(text a, b, c != 0)`.
    SetSelText,
    /// `AddInputHistory(text a, b, c != 0)`.
    AddInputHistory,
    /// `SetInputHistory(text a, b, c != 0)`.
    SetInputHistory,
    /// `mMaxChars = a`.
    SetMaxChars,
    /// `mSelectOnFocus = a != 0`.
    SetSelectOnFocus,
    /// `mMaxInputHistory = a`.
    SetMaxInputHistory,
    /// `mLeftPad = a`.
    SetLeftPad,
    /// `mSelStartChar` (the numeric filter reads it).
    SelStart,
}

#[derive(Debug)]
struct State {
    text: Vec<u32>,
    password_text: Vec<u32>,
    password: i32,
    history: VecDeque<Vec<u32>>,
    history_pos: i32,
    max_history: i32,
    traversed: bool,
    backwards: bool,
    sel_start: i32,
    sel_end: i32,
    horz_offset: i32,
    selecting: bool,
    left_pad: i32,
    top_pad: i32,
    right_pad: i32,
    bottom_pad: i32,
    cursor_delay: i32,
    cursor_on_time: i32,
    cursor_off_time: i32,
    cursor_on: bool,
    underline_cursor: bool,
    allow_selection: bool,
    select_on_focus: bool,
    max_chars: i32,
    /// `mTextRect`: x, y, width, height
    rect: [i32; 4],
}

impl State {
    fn get_text(&self) -> &Vec<u32> {
        if self.password == 0 {
            &self.text
        } else {
            &self.password_text
        }
    }
    fn sel(&self) -> EeWonSel {
        EeWonSel { start: self.sel_start, end: self.sel_end, backwards: self.backwards }
    }
    fn put_sel(&mut self, s: EeWonSel) {
        self.sel_start = s.start;
        self.sel_end = s.end;
        self.backwards = s.backwards;
    }
    fn cursor(&self) -> i32 {
        if self.backwards {
            self.sel_start
        } else {
            self.sel_end
        }
    }
}

/// The Rust InputBox.
pub struct EeWonInput {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonInput {
    /// The constructor's state (the C++ constructor sets the font, the colours, the flags and the cursor).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonInput {
            comp,
            host,
            st: RefCell::new(State {
                text: Vec::new(),
                password_text: Vec::new(),
                password: 0,
                history: VecDeque::new(),
                history_pos: 0,
                max_history: 100,
                traversed: false,
                backwards: false,
                sel_start: 0,
                sel_end: 0,
                horz_offset: 0,
                selecting: false,
                left_pad: 4,
                top_pad: 3,
                right_pad: 4,
                bottom_pad: 3,
                cursor_delay: 0,
                cursor_on_time: 500,
                cursor_off_time: 500,
                cursor_on: false,
                underline_cursor: false,
                allow_selection: true,
                select_on_focus: false,
                max_chars: 0,
                rect: [0; 4],
            }),
        }
    }

    // ---- calls out

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, 0) },
            None => 0,
        }
    }
    fn op0(&self, op: Op) -> isize {
        self.op(op, 0, 0, 0)
    }
    fn width(&self) -> i32 {
        self.op(Op::Get, F::Width as isize, 0, 0) as i32
    }
    fn height(&self) -> i32 {
        self.op(Op::Get, F::Height as isize, 0, 0) as i32
    }
    fn invalidate(&self) {
        self.op0(Op::Invalidate);
    }
    fn text_width(&self, offset: i32, length: i32) -> i32 {
        self.op(Op::TextWidth, offset as isize, length as isize, 0) as i32
    }
    fn font_char_width(&self, ch: i32) -> i32 {
        self.op(Op::FontCharWidth, ch as isize, 0, 0) as i32
    }
    fn fire(&self, e: Event) {
        self.op(Op::FireEvent, e as isize, 0, 0);
    }
    fn store(&self, which: isize, text: &[u32]) {
        self.op(Op::TextStore, which, text.as_ptr() as isize, text.len() as isize);
    }

    // ---- the methods

    fn adjust_text_rect(&self) {
        let (mut l, r, t, b) = {
            let s = self.st.borrow();
            (s.left_pad, s.right_pad, s.top_pad, s.bottom_pad)
        };
        let icon = self.op0(Op::IconWidth) as i32;
        if icon >= 0 {
            l = l.wrapping_add(icon.wrapping_add(2));
        }
        let w = self.width().wrapping_sub(l).wrapping_sub(r);
        let h = self.height().wrapping_sub(t).wrapping_sub(b);
        self.st.borrow_mut().rect = [l, t, w, h];
    }

    pub fn icon_changed(&self) {
        self.adjust_text_rect();
        self.adjust_position();
        self.invalidate();
    }

    pub fn size_changed(&self) {
        self.op0(Op::BaseSizeChanged);
        self.adjust_text_rect();
    }

    pub fn desired_size(&self) -> (i32, i32) {
        (100, (self.op0(Op::FontHeight) as i32).wrapping_add(6))
    }

    pub fn set_char_width(&self, em: i32) {
        let w = self.font_char_width('M' as i32).wrapping_mul(em).wrapping_add(8);
        let h = self.height();
        self.op(Op::SetSize, w as isize, h as isize, 0);
    }

    pub fn got_focus(&self) {
        self.op0(Op::BaseGotFocus);
        {
            let mut s = self.st.borrow_mut();
            s.cursor_delay = 0;
            s.cursor_on = true;
        }
        self.op(Op::RequestTimer, 1, 0, 0);
        if self.st.borrow().select_on_focus {
            self.select_range(0, -1);
        }
        self.invalidate();
    }

    pub fn lost_focus(&self) {
        self.op0(Op::BaseLostFocus);
        self.st.borrow_mut().cursor_on = false;
        self.op(Op::RequestTimer, 0, 0, 0);
        self.invalidate();
    }

    pub fn check_sel(&self, x: i32, y: i32, new_start: bool) {
        let _ = y;
        let (allow, horz, left, sel_char, len) = {
            let s = self.st.borrow();
            (s.allow_selection, s.horz_offset, s.rect[0], s.cursor(), s.text.len())
        };
        if !allow {
            return;
        }
        let cx = guiedit::sel_x(x, self.width(), horz, left);
        let w = self.text_width(0, sel_char);
        let mut widths = vec![0i32; len];
        if w >= cx {
            let mut i = 0;
            while i < sel_char && (i as usize) < len {
                widths[i as usize] = self.op(Op::TextCharWidth, i as isize, 0, 0) as i32;
                i += 1;
            }
        } else {
            let mut i = sel_char.max(0) as usize;
            while i < len {
                widths[i] = self.op(Op::TextCharWidth, i as isize, 0, 0) as i32;
                i += 1;
            }
        }
        let c = guiedit::char_at(cx, w, sel_char, &widths);
        {
            let mut s = self.st.borrow_mut();
            let n = guiedit::extend_selection(c, new_start, s.sel());
            s.put_sel(n);
        }
        self.invalidate();
    }

    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        if button != 0 {
            return;
        }
        let shift = self.op0(Op::KeyMod) & 1 != 0;
        self.check_sel(x, y, !shift);
        {
            let mut s = self.st.borrow_mut();
            if s.allow_selection {
                s.selecting = true;
            }
        }
        self.op0(Op::RequestFocus);
    }

    pub fn mouse_up(&self, button: i32) {
        if button == 0 {
            self.st.borrow_mut().selecting = false;
        }
    }

    pub fn mouse_exit(&self) {
        self.op0(Op::BaseMouseExit);
        self.st.borrow_mut().selecting = false;
    }

    pub fn set_cursor_blink_rate(&self, on: i32, off: i32) {
        let mut s = self.st.borrow_mut();
        s.cursor_on_time = on;
        s.cursor_off_time = guiedit::blink_rate(on, off).1;
    }

    pub fn timer_event(&self, delta: i32) -> bool {
        let blink = {
            let mut s = self.st.borrow_mut();
            let b = guiedit::blink(s.cursor_delay, delta, s.cursor_on, s.cursor_on_time, s.cursor_off_time);
            s.cursor_delay = b.delay;
            s.cursor_on = b.on;
            b
        };
        if blink.changed {
            self.invalidate();
        }
        if !self.st.borrow().selecting {
            return true;
        }
        let (mut x, mut y) = (0i32, 0i32);
        self.op(Op::GetMousePos, &mut x as *mut i32 as isize, &mut y as *mut i32 as isize, 0);
        let (char_pos, len, horz) = {
            let s = self.st.borrow();
            (s.cursor(), s.text.len() as i32, s.horz_offset)
        };
        let (w, h) = (self.width(), self.height());
        let Some(o) = guiedit::drag_scroll(x, y, w, h, char_pos, len, horz) else {
            return true;
        };
        self.st.borrow_mut().horz_offset = o;
        self.check_sel(x, y, false);
        let (char_pos, len, rect_w) = {
            let s = self.st.borrow();
            (s.cursor(), s.text.len() as i32, s.rect[2])
        };
        let (mut tw, mut m) = (0, 0);
        if char_pos == len {
            tw = self.op0(Op::TextWidthAll) as i32;
            if tw.wrapping_add(self.st.borrow().horz_offset) > rect_w {
                m = self.op0(Op::FontStringM) as i32;
            }
        }
        let mut s = self.st.borrow_mut();
        s.horz_offset = guiedit::drag_scroll_end(char_pos, len, tw, m, rect_w, s.horz_offset);
        true
    }

    pub fn adjust_position(&self) {
        let cursor = self.st.borrow().cursor();
        let pre = self.text_width(0, cursor);
        let m = self.font_char_width('M' as i32);
        let mut s = self.st.borrow_mut();
        s.horz_offset = guiedit::cursor_scroll(pre, m, s.horz_offset, s.rect[2]);
    }

    pub fn select_range(&self, start: i32, end: i32) {
        {
            let mut s = self.st.borrow_mut();
            let n = guiedit::select_range(start, end, s.text.len() as i32);
            s.put_sel(n);
        }
        self.adjust_position();
        self.invalidate();
    }

    pub fn set_sel_text(&self, text: &[u32], send: bool) {
        let mut paste = text.to_vec();
        let cut = guiedit::control_cut(&paste);
        if cut < paste.len() {
            paste.truncate(cut);
        }
        let mut filtered: Vec<u32> = Vec::new();
        let r =
            self.op(Op::Filter, paste.as_ptr() as isize, paste.len() as isize, &mut filtered as *mut Vec<u32> as isize);
        if r != 2 {
            paste = filtered;
            if r == 0 {
                return;
            }
        }
        let new_text = {
            let mut s = self.st.borrow_mut();
            let cur = s.get_text().clone();
            if let Some(n) =
                guiedit::paste_allowed(cur.len() as i32, paste.len() as i32, s.sel_start, s.sel_end, s.max_chars)
            {
                paste.resize(n.max(0) as usize, 0);
            }
            if !paste.is_empty() || s.sel_start != s.sel_end {
                let (a, b) = (s.sel_start, s.sel_end);
                s.sel_start = s.sel_start.wrapping_add(paste.len() as i32);
                s.sel_end = s.sel_start;
                let mut n: Vec<u32> = Vec::new();
                n.extend_from_slice(&cur[..(a.max(0) as usize).min(cur.len())]);
                n.extend_from_slice(&paste);
                n.extend_from_slice(&cur[(b.max(0) as usize).min(cur.len())..]);
                Some(n)
            } else {
                None
            }
        };
        if let Some(n) = new_text {
            self.set_text_prv(&n, send);
        }
        {
            let mut s = self.st.borrow_mut();
            s.cursor_delay = 0;
            s.cursor_on = true;
        }
        self.adjust_position();
        self.invalidate();
    }

    pub fn add_input_history(&self, text: &[u32], clear_traversal: bool) {
        if text.is_empty() {
            return;
        }
        let mut s = self.st.borrow_mut();
        let mut pos = s.history_pos;
        while s.history.len() >= s.max_history as isize as usize {
            if s.history.is_empty() {
                break; // the original popped an empty deque here (a limit of 0)
            }
            s.history.pop_front();
            pos = pos.wrapping_sub(1);
        }
        if s.max_history > 0 && s.history.back().is_none_or(|b| b.as_slice() != text) {
            s.history.push_back(text.to_vec());
        }
        if pos >= 0 && lt_size(pos, s.history.len()) && s.history[pos as usize].as_slice() == text {
            s.history_pos = pos;
        } else {
            s.history_pos = s.history.len() as i32;
        }
        if clear_traversal {
            s.traversed = false;
        }
    }

    pub fn set_input_history(&self, text: &[u32], clear_traversal: bool) {
        if text.is_empty() {
            return;
        }
        let pos = self.st.borrow().history_pos;
        let inside = pos >= 0 && lt_size(pos, self.st.borrow().history.len());
        if inside {
            self.st.borrow_mut().history[pos as usize] = text.to_vec();
        } else {
            self.add_input_history(text, clear_traversal);
            self.st.borrow_mut().history_pos = pos;
        }
        if clear_traversal {
            self.st.borrow_mut().traversed = false;
        }
    }

    pub fn key_down(&self, key: i32) -> bool {
        let mods = self.op0(Op::KeyMod);
        let mut shift = mods & 1 != 0;
        let ctrl = mods & 4 != 0;
        if !self.st.borrow().allow_selection {
            shift = false;
        }
        match key {
            13 => {
                if !self.st.borrow().get_text().is_empty() {
                    self.fire(Event::InputReturn);
                }
                return true;
            }
            38 | 40 => {
                let size = {
                    let s = self.st.borrow();
                    s.max_history.min(s.history.len() as i32)
                };
                if size > 0 {
                    let first = {
                        let s = self.st.borrow();
                        !s.traversed && lt_size(s.history_pos, s.history.len())
                    };
                    if first {
                        self.st.borrow_mut().traversed = true;
                    } else {
                        self.st.borrow_mut().traversed = true;
                        let t = self.st.borrow().get_text().clone();
                        self.set_input_history(&t, false);
                        let mut s = self.st.borrow_mut();
                        if key == 38 {
                            s.history_pos = s.history_pos.wrapping_sub(1);
                        } else {
                            s.history_pos = s.history_pos.wrapping_add(1);
                        }
                    }
                    {
                        let mut s = self.st.borrow_mut();
                        if s.history_pos < 0 {
                            s.history_pos = 0;
                        }
                    }
                    let pos = self.st.borrow().history_pos;
                    if pos >= size {
                        self.set_text(&[], true);
                        let mut s = self.st.borrow_mut();
                        s.history_pos = s.history.len() as i32;
                    } else {
                        let t = self.st.borrow().history.get(pos as usize).cloned().unwrap_or_default();
                        self.set_text(&t, true);
                        let l = self.st.borrow().get_text().len() as i32;
                        self.select_range(l, -1);
                    }
                }
            }
            8 | 46 => {
                let total = {
                    let mut s = self.st.borrow_mut();
                    let t = s.get_text().clone();
                    let p = guiedit::delete_parts(key == 8, s.sel_start, s.sel_end, t.len() as i32);
                    let left: &[u32] = if p.left_count >= 0 { &t[..(p.left_count as usize).min(t.len())] } else { &[] };
                    let right: &[u32] =
                        if p.right_start >= 0 { &t[(p.right_start as usize).min(t.len())..] } else { &[] };
                    s.sel_start = left.len() as i32;
                    s.sel_end = s.sel_start;
                    let mut total = left.to_vec();
                    total.extend_from_slice(right);
                    total
                };
                self.set_text_prv(&total, true);
            }
            36 | 35 | 37 | 39 => {
                let mut s = self.st.borrow_mut();
                let walk = if key == 36 || key == 35 { s.get_text().clone() } else { s.text.clone() };
                if let Some(n) = guiedit::key_selection(key, shift, ctrl, s.sel(), &walk) {
                    s.put_sel(n);
                }
            }
            _ => return false,
        }
        {
            let mut s = self.st.borrow_mut();
            s.cursor_delay = 0;
            s.cursor_on = true;
        }
        self.adjust_position();
        self.invalidate();
        true
    }

    pub fn set_password_char(&self, ch: i32) {
        let t = {
            let mut s = self.st.borrow_mut();
            s.password = ch;
            s.text.clone()
        };
        self.set_text(&t, false);
    }

    pub fn set_text(&self, text: &[u32], send: bool) {
        self.set_text_prv(text, send);
        {
            let mut s = self.st.borrow_mut();
            s.sel_start = 0;
            s.sel_end = 0;
        }
        self.adjust_position();
        self.invalidate();
    }

    fn set_text_prv(&self, text: &[u32], send: bool) {
        let (shown, real) = {
            let mut s = self.st.borrow_mut();
            if s.password == 0 {
                s.text = text.to_vec();
            } else {
                s.password_text = text.to_vec();
                s.text = vec![wunit(s.password); text.len()];
            }
            (s.text.clone(), s.get_text().clone())
        };
        self.store(0, &shown);
        self.store(1, &real);
        if send {
            self.fire(Event::InputTextChanged);
        }
    }

    pub fn copy_selection(&self) {
        let part = {
            let s = self.st.borrow();
            let a = (s.sel_start.max(0) as usize).min(s.text.len());
            let b = (s.sel_end.max(0) as usize).min(s.text.len()).max(a);
            s.text[a..b].to_vec()
        };
        self.op(Op::ClipboardSet, part.as_ptr() as isize, part.len() as isize, 0);
    }

    pub fn key_char(&self, key: i32) -> bool {
        let mut paste: Vec<u32> = Vec::new();
        if key == 22 {
            self.op(Op::ClipboardTake, &mut paste as *mut Vec<u32> as isize, 0, 0);
        } else if key == 3 {
            self.copy_selection();
            return true;
        } else if key == 24 {
            self.copy_selection();
        } else if key > 31 {
            paste.push(wunit(key));
        } else {
            return true;
        }
        self.set_sel_text(&paste, true);
        true
    }

    pub fn set_allow_selection(&self, allow: bool) {
        if allow == self.st.borrow().allow_selection {
            return;
        }
        self.st.borrow_mut().allow_selection = allow;
        let cursor = if allow { Cursor::IBeam } else { Cursor::None };
        self.op(Op::SetDefaultCursor, cursor as isize, 0, 0);
        let clear = {
            let mut s = self.st.borrow_mut();
            s.selecting = false;
            if !s.allow_selection && s.sel_start != s.sel_end {
                s.sel_start = 0;
                s.sel_end = 0;
                true
            } else {
                false
            }
        };
        if clear {
            self.invalidate();
        }
    }

    pub fn paint(&self, g: isize) {
        self.op(Op::BasePaint, g, 0, 0);
        let mut b = EeWonInputPaint::default();
        self.op(Op::PaintInputs, &mut b as *mut EeWonInputPaint as isize, 0, 0);
        b.width = self.width();
        b.height = self.height();
        {
            let s = self.st.borrow();
            b.cursor_on = s.cursor_on;
            b.underline_cursor = s.underline_cursor;
            b.backwards_sel = s.backwards;
            b.horz_offset = s.horz_offset;
            b.text_x = s.rect[0];
            b.text_y = s.rect[1];
            b.text_width = s.rect[2];
            b.text_height = s.rect[3];
            b.sel_start = s.sel_start;
            b.sel_end = s.sel_end;
            b.length = s.text.len() as i32;
        }
        let m = EeWonTextMeasure {
            ctx: self as *const EeWonInput as *mut c_void,
            string_width: Some(measure_string),
            char_width: Some(measure_char),
        };
        let ops: Vec<EeWonDrawOp> = guidraw::input_box(&b, &m);
        for o in &ops {
            self.op(Op::Draw, g, o as *const EeWonDrawOp as isize, 0);
        }
    }

    /// One inward call.
    pub fn call(&self, call: EeWonInputCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonInputCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        // SAFETY (the text calls): the C++ passes `b` wchar_t units at `a`
        let text = || unsafe { units_in(a, b) };
        match call {
            C::SizeChanged => self.size_changed(),
            C::DesiredSize => {
                let (w, h) = self.desired_size();
                // SAFETY: the C++ passes two live ints
                unsafe {
                    *(a as *mut i32) = w;
                    *(b as *mut i32) = h;
                }
            }
            C::GotFocus => self.got_focus(),
            C::LostFocus => self.lost_focus(),
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseUp => self.mouse_up(ci),
            C::MouseExit => self.mouse_exit(),
            C::MouseDrag => self.check_sel(ai, bi, false),
            C::TimerEvent => return self.timer_event(ai) as isize,
            C::KeyChar => return self.key_char(ai) as isize,
            C::KeyDown => return self.key_down(ai) as isize,
            C::Paint => self.paint(a),
            C::SetText => self.set_text(&text(), c != 0),
            C::SetPasswordChar => self.set_password_char(ai),
            C::SetAllowSelection => self.set_allow_selection(a != 0),
            C::SelectRange => self.select_range(ai, bi),
            C::IconChanged => self.icon_changed(),
            C::SetCharWidth => self.set_char_width(ai),
            C::SetCursorBlinkRate => self.set_cursor_blink_rate(ai, bi),
            C::AdjustPosition => self.adjust_position(),
            C::CopySelection => self.copy_selection(),
            C::SetSelText => self.set_sel_text(&text(), c != 0),
            C::AddInputHistory => self.add_input_history(&text(), c != 0),
            C::SetInputHistory => self.set_input_history(&text(), c != 0),
            C::SetMaxChars => self.st.borrow_mut().max_chars = ai,
            C::SetSelectOnFocus => self.st.borrow_mut().select_on_focus = a != 0,
            C::SetMaxInputHistory => self.st.borrow_mut().max_history = ai,
            C::SetLeftPad => self.st.borrow_mut().left_pad = ai,
            C::SelStart => return self.st.borrow().sel_start as isize,
        }
        0
    }
}

unsafe extern "C" fn measure_string(ctx: *mut c_void, offset: i32, length: i32) -> i32 {
    // SAFETY: the ctx is the box whose paint runs
    let b = unsafe { &*(ctx as *const EeWonInput) };
    b.text_width(offset, length)
}

unsafe extern "C" fn measure_char(ctx: *mut c_void, index: i32) -> i32 {
    // SAFETY: as above
    let b = unsafe { &*(ctx as *const EeWonInput) };
    if index < 0 {
        b.font_char_width('M' as i32)
    } else {
        b.op(Op::TextCharWidth, index as isize, 0, 0) as i32
    }
}

// ---- the C ABI

/// A new Rust InputBox for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_input_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonInput {
    Box::into_raw(Box::new(EeWonInput::new(component, host)))
}

/// # Safety
/// `input` came from `ee_won_input_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_input_free(input: *mut EeWonInput) {
    if !input.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(input) });
    }
}

/// One of InputBox's methods (`EeWonInputCall`).
///
/// # Safety
/// `input` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_input_call(
    input: *const EeWonInput,
    call: EeWonInputCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*input).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "inputobj_tests.rs"]
mod tests;
