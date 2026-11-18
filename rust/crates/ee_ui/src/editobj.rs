//! `UIEditBox` as a whole Rust object (R6 ui round 14): the box's state (its lines, the display copies, the caret,
//! selection and IME records, the rectangles, colours and flags) and every method of `UIEditBox.cpp` and the
//! header's. The C++ class stays the type the forms, combo boxes and game screens hold (and the scenario editor's
//! `EESETSAEditBox` derives from): its methods are one-line forwarders to [`ee_ui_edit_call`], the constructor makes
//! this object and the destructor frees it. Every call out is an [`EeUiOp`] through the host: `UIControl`'s members,
//! the box's virtuals (`ProcessEnterKey`, `ReleaseCapturedKeyboard`, `ButtonDown`, `SetRectangle`, `SetBgOpacity`,
//! `ReleaseCapturedMouse`, so an override runs), the scroll bar (a part held by address), the two `GEText`s, the
//! caret's and the underlines' `GELine`s, the selection rhombus, the four polls, the keys and the clipboard.
//!
//! **The lines.** `GetText(i)` answers a `const char*`, so the lines are this object's own NUL-terminated bytes and
//! the answer points into them (valid until the text changes, as the `UWideString`'s buffer was). `UWideString`
//! stays only at the boundary: `SetText`, `GetAllText`/`GetSelectedText`'s out strings (assigned by the C++ from the
//! bytes this object answers), the IME strings and attributes (bytes in), the clipboard (bytes both ways).
//!
//! The text model (`edit.rs`), the key dispatch (`edit_keys.rs`) and the drawing's ranges (`edit_draw.rs`) are the
//! earlier rounds' algorithms with their goldens; this object is their host ([`EditHost`], [`EditKeysHost`],
//! [`ImeHost`]).

use core::cell::{Cell, RefCell};
use core::ffi::{c_char, c_long, c_short, c_ulong, c_void, CStr};

use crate::edit::{Edit, EditFields, EditHost, EditState, SCROLL_BAR_WIDTH};
use crate::edit_draw::{self, EditIme, EditSelection, ImeHost};
use crate::edit_keys::{EditKey, EditKeysHost, EditPoll, PollOp};
use crate::obj::{read_buf, write_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::{um_floor, UiPoint, UiRect};
use crate::text::{char_len, empty, from_bytes, has_line_end, prefix, tail, Platform};

/// The inward calls (the C++ shim's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiEditCall {
    /// `AddIMEResultString(a)` (a NUL-terminated bytes).
    AddImeResult = 1,
    /// `CalculateCaretFromMouseLocation(.., (f[0], f[1]))` -> the line; the index written to `*(long*)a`.
    CaretFromMouse = 2,
    /// `ChangeIMECompositionString(a text, c attributes, b cursor)` (NUL-terminated bytes) -> its result.
    ImeChange = 3,
    /// `ChangeFont(a)`.
    ChangeFont = 4,
    /// `ChangeWidth(a)`.
    ChangeWidth = 5,
    /// `CloseIMECompositionString()`.
    ImeClose = 6,
    /// `Copy()`.
    Copy = 7,
    /// `CopyData()`.
    CopyData = 8,
    /// `Cut()`.
    Cut = 9,
    /// `DeleteSelectedText()`.
    DeleteSelectedText = 10,
    /// `DeleteText(a start index, b start line, c end index, d end line)`.
    DeleteRange = 11,
    /// `DeleteText()`.
    Clear = 12,
    /// `DrawPolygons()`.
    DrawPolygons = 13,
    /// `DrawTextA()`.
    DrawText = 14,
    /// `GetAllText` -> the address of its NUL-terminated bytes (valid until the next call).
    AllText = 15,
    /// `GetCaretLocation` -> `f[0]`, `f[1]`.
    CaretLocation = 16,
    /// `GetCursorOn()`.
    CursorOn = 17,
    /// `GetGEText()`.
    Text = 18,
    /// `GetIMEEnabled()`.
    ImeEnabled = 19,
    /// `GetIsPasswordBox()`.
    IsPasswordBox = 20,
    /// `GetIsTextSelected()`.
    IsTextSelected = 21,
    /// `GetLineCount()`.
    LineCount = 22,
    /// `GetSelectedText` -> the address of its NUL-terminated bytes, or 0 without a selection (the out string left
    /// alone).
    SelectedText = 23,
    /// `GetSelectedTextIndices`' start (`mSelectionStartIndex`).
    SelectionStartIndex = 24,
    /// `GetSelectedTextIndices`' end (`mCaretIndex`).
    CaretIndex = 25,
    /// `GetStartingDisplayIndex()`.
    StartingDisplayIndex = 26,
    /// `GetText(a)` -> the address of line `a`'s NUL-terminated bytes (0 past the lines).
    Line = 27,
    /// `Paste()`.
    Paste = 28,
    /// `OpenIMECompositionString()`.
    ImeOpen = 29,
    /// `ReleaseCapturedKeyboard()` (the box's own).
    ReleaseKeyboard = 30,
    /// `SelectText()`.
    SelectAll = 31,
    /// `SelectText(a caret index, b anchor index, c caret line, d anchor line)`.
    Select = 32,
    /// `SetCaretLocation(a index, b line)`.
    SetCaretLocation = 33,
    /// `SetCharScroll(a)`.
    SetCharScroll = 34,
    /// `SetCursorColor(f[0..3])`.
    SetCursorColor = 35,
    /// `SetIMETextColor(f[0..3])`.
    SetImeTextColor = 36,
    /// `SetIsPasswordBox(a != 0)`.
    SetIsPasswordBox = 37,
    /// `SetLocation(f[0], f[1])` (the box's own).
    SetLocation = 38,
    /// `SetSelectedBgColor(f[0..3])`.
    SetSelectedBgColor = 39,
    /// `SetText(a)` (NUL-terminated bytes).
    SetText = 40,
    /// `UpdateKeyboard()`.
    UpdateKeyboard = 41,
    /// `UpdateTranslatedKey(a, b)` (NUL-terminated bytes, the repeat).
    UpdateTranslatedKey = 42,
    /// `MouseLButtonDoublePressed()`.
    MouseDoublePressed = 43,
    /// `MouseLButtonDown()`.
    MouseLButtonDown = 44,
    /// `MouseLButtonUp()`.
    MouseLButtonUp = 45,
    /// `MouseMove(f[0], f[1])`.
    MouseMove = 46,
    /// `ProcessEnterKey()` (the box's own).
    ProcessEnterKey = 47,
    /// `TabbedFrom()`.
    TabbedFrom = 48,
    /// `TabbedTo()`.
    TabbedTo = 49,
}

/// The Rust UIEditBox.
pub struct EeUiEdit {
    host: Host,
    /// The model's members: the record (the original's from `mCaretX` on), `mTextRectangle`; `rect` and `border`
    /// are read from `UIControl` at each load.
    st: Cell<EditState>,
    /// `mLines`: NUL-terminated bytes each.
    lines: RefCell<Vec<Vec<u8>>>,
    /// `mDisplayLines`: the visible lines as of the last `CopyData`, NUL-terminated.
    display: RefCell<Vec<Vec<u8>>>,
    /// `mIMEAttributes`, `mCopiedIMEAttributes` (the bytes before the terminator).
    ime_attrs: RefCell<Vec<u8>>,
    copied_ime_attrs: RefCell<Vec<u8>>,
    copied_text_rect: Cell<UiRect>,
    selected_bg: Cell<[f32; 3]>,
    cursor_color: Cell<[f32; 3]>,
    ime_color: Cell<[f32; 3]>,
    copied_ime_color: Cell<[f32; 3]>,
    /// `mScrollBar` (multi-line boxes; 0 otherwise).
    scroll_bar: isize,
    /// `mCursor`; the underlines' line (a `GELine` local of the original's `DrawPolygons`).
    cursor: isize,
    underline: isize,
    /// `mSelectionRhombus`.
    rhombus: isize,
    /// `mText` (measuring), `mCopiedText` (drawing).
    text: isize,
    copied_text: isize,
    /// `mRepeatPoll`, `mRepeatDelayPoll`, `mCursorPoll`, `mScrollPoll` ([`EditPoll`]'s order).
    polls: [isize; 4],
    /// A text answer's bytes (`GetAllText`, `GetSelectedText`), kept until the next.
    out: RefCell<Vec<u8>>,
}

/// An index or count as the C runtime's `size_t` takes it (a negative one is huge: the stepping stops at the end).
fn idx(v: c_long) -> usize {
    v as isize as usize
}

/// NUL-terminated bytes at `p` (with the terminator); the empty text for NULL.
///
/// # Safety
/// `p` is NULL or NUL-terminated bytes.
unsafe fn bytes_at(p: isize) -> Vec<u8> {
    if p == 0 {
        return empty();
    }
    // SAFETY: the contract
    unsafe { CStr::from_ptr(p as *const c_char) }.to_bytes_with_nul().to_vec()
}

fn color_buf(c: [f32; 3]) -> [f32; 4] {
    [c[0], c[1], c[2], 0.0]
}

/// The box as the text model's host.
struct Bridge<'a>(&'a EeUiEdit);

impl EditHost for Bridge<'_> {
    fn load(&self) -> EditState {
        let o = self.0;
        let mut s = o.st.get();
        s.rect = o.host.rect();
        s.border = o.op(Op::BorderThickness, 0, 0) as c_ulong;
        s
    }
    fn store(&mut self, s: &EditState) {
        self.0.st.set(*s);
    }
    fn line_count(&self) -> usize {
        self.0.lines.borrow().len()
    }
    fn line(&self, i: usize) -> Vec<u8> {
        self.0.line(i)
    }
    fn set_line(&mut self, i: usize, text: &[u8]) {
        if let Some(l) = self.0.lines.borrow_mut().get_mut(i) {
            *l = from_bytes(body(text));
        }
    }
    fn insert_line(&mut self, i: usize, text: &[u8]) {
        let mut lines = self.0.lines.borrow_mut();
        let i = i.min(lines.len());
        lines.insert(i, from_bytes(body(text)));
    }
    fn erase_line(&mut self, i: usize) {
        let mut lines = self.0.lines.borrow_mut();
        if i < lines.len() {
            lines.remove(i);
        }
    }
    fn keep_first_line(&mut self) {
        self.0.lines.borrow_mut().truncate(1);
    }
    fn text_width(&mut self, text: &[u8]) -> f32 {
        self.0.extent_of(self.0.text, text)
    }
    fn font_height(&self) -> c_long {
        self.0.op(Op::TextFontHeight, self.0.text, 0) as c_long
    }
    fn has_scroll_bar(&self) -> bool {
        self.0.scroll_bar != 0
    }
    fn scroll_value(&self) -> c_long {
        self.0.op(Op::BarValue, self.0.scroll_bar, 0) as c_long
    }
    fn scroll_max(&self) -> c_long {
        self.0.op(Op::BarMax, self.0.scroll_bar, 0) as c_long
    }
    fn set_scroll_value(&mut self, value: c_ulong) {
        self.0.op(Op::BarSetValue, self.0.scroll_bar, value as isize);
    }
    fn set_scroll_range(&mut self, min: c_ulong, max: c_ulong) {
        // the model asks only for ranges from 0 (`SetRange(0, max)`)
        debug_assert_eq!(min, 0);
        self.0.op(Op::BarSetRange, self.0.scroll_bar, max as isize);
    }
    fn scroll_set_location(&mut self, at: UiPoint) {
        self.0.host.op_point(Op::PartSetLocation, self.0.scroll_bar, at);
    }
    fn set_dirty(&mut self) {
        self.0.op(Op::SetDirty, 1, 0);
    }
    fn set_rectangle(&mut self, r: &UiRect) {
        self.0.host.op_rect(Op::VirtSetRectangle, 0, 0, *r);
    }
    fn base_set_rectangle(&mut self, r: &UiRect) {
        self.0.host.op_rect(Op::BaseSetRectangle, 0, 0, *r);
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.0.host.op_point(Op::BaseSetLocation, 0, at);
    }
    fn fire(&mut self, event: c_ulong) {
        self.0.op(Op::FireTrigger, event as isize, 0);
    }
}

impl EditKeysHost for Bridge<'_> {
    fn key(&mut self, k: EditKey) -> bool {
        self.0.op(Op::EditKey, k as isize, 0) != 0
    }
    fn poll(&mut self, p: EditPoll, op: PollOp) -> c_ulong {
        self.0.poll(p, op)
    }
    fn process_enter_key(&mut self) {
        self.0.op(Op::VirtProcessEnterKey, 0, 0);
    }
    fn clipboard_set(&mut self, text: &[u8]) {
        let t = from_bytes(body(text));
        self.0.op(Op::ClipboardSet, t.as_ptr() as isize, 0);
    }
    fn clipboard_get(&mut self) -> Option<Vec<u8>> {
        let p = self.0.op(Op::ClipboardGet, 0, 0);
        if p == 0 {
            return None;
        }
        // SAFETY: the host answers NUL-terminated bytes it keeps until the next ClipboardGet
        Some(unsafe { bytes_at(p) })
    }
    fn translated_key(&mut self, text: &[u8]) {
        let t = from_bytes(body(text));
        self.0.op(Op::TranslatedKey, t.as_ptr() as isize, 0);
    }
}

/// The bytes before the terminator.
fn body(s: &[u8]) -> &[u8] {
    let end = s.iter().position(|&c| c == 0).unwrap_or(s.len());
    &s[..end]
}

/// `DrawPolygons`' IME underlines over the copied lines and the copied text.
struct Underlines<'a> {
    o: &'a EeUiEdit,
    /// The character `char_bytes` copied (the original's work string).
    ch: Vec<u8>,
}

impl ImeHost for Underlines<'_> {
    fn display_len(&mut self, i: c_ulong) -> c_ulong {
        char_len(&Platform, &self.o.display_line(i as usize)) as c_ulong
    }
    fn prefix_width(&mut self, i: c_ulong, j: c_ulong) -> f32 {
        self.ch = prefix(&Platform, &self.o.display_line(i as usize), j as usize);
        self.o.extent_of(self.o.copied_text, &self.ch)
    }
    fn char_bytes(&mut self, i: c_ulong, j: c_ulong) -> usize {
        let line = self.o.display_line(i as usize);
        self.ch = prefix(&Platform, tail(&Platform, &line, j as usize), 1);
        body(&self.ch).len()
    }
    fn char_width(&mut self, _i: c_ulong, _j: c_ulong) -> f32 {
        self.o.extent_of(self.o.copied_text, &self.ch)
    }
    fn attribute(&mut self, offset: i32) -> u8 {
        if offset < 0 {
            return 0;
        }
        self.o.copied_ime_attrs.borrow().get(offset as usize).copied().unwrap_or(0)
    }
    fn draw_line(&mut self, start: UiPoint, end: UiPoint) -> c_long {
        let o = self.o;
        let mut f = [start.x, start.y, end.x, end.y];
        o.host.op_f(Op::LineSetPoints, o.underline, 0, 0, 0, &mut f);
        let mut c = color_buf(o.copied_ime_color.get());
        o.host.op_f(Op::LineSetColor, o.underline, 0, 0, 0, &mut c);
        crate::draw2d::line(o.underline, || o.op(Op::LineDraw, o.underline, 0)) as c_long
    }
}

impl EeUiEdit {
    /// The constructor's part after `UIControl`'s: `font` the text font, `flags` bit 0 numbers only, bit 1
    /// multi-line, bit 2 a password box; `border` the thickness given.
    pub fn new(host: Host, font: isize, max_chars: c_ulong, flags: u32, border: c_ulong) -> Self {
        let numbers_only = flags & 1 != 0;
        let multi = flags & 2 != 0;
        let password = flags & 4 != 0;
        let op = |o: Op, a: isize, b: isize| host.op(o, a, b, 0, 0);
        let rhombus = op(Op::RhombusCreate, 0, 0);
        let f = EditFields {
            char_scroll: 1,
            max_chars,
            is_single_line: !multi,
            is_password_box: !multi && password,
            numbers_only,
            ..EditFields::default()
        };
        let ime_color = [0.0, 1.0, 0.0];
        op(Op::SetBorderType, 0, 0);
        let cursor = op(Op::LineCreate, 1, 0);
        let underline = op(Op::LineCreate, 0, 0);
        let polls = [
            op(Op::PollCreate, 50, 0),
            op(Op::PollCreate, 500, 0),
            op(Op::PollCreate, 500, 0),
            op(Op::PollCreate, 25, 0),
        ];
        let text = op(Op::TextCreate, font, 0);
        let copied_text = op(Op::TextCreate, font, 0);
        let mut c = [1.0f32, 1.0, 1.0, 0.0];
        host.op_f(Op::TextSetFgColor, text, 0, 0, 0, &mut c);
        let mut c = [0.0f32; 4];
        host.op_f(Op::TextSetBgColor, text, 0, 0, 0, &mut c);
        op(Op::TextSetTransparent, text, 1);
        let r = host.rect();
        let b = border as f32;
        let mut tr = UiRect::new(r.left + b, r.top + b, r.right - b, r.bottom - b);
        if tr.top > tr.bottom {
            tr.top = r.height() * 0.5 + r.top;
            tr.bottom = tr.top + 1.0;
        }
        if tr.left > tr.right {
            tr.left = r.width() * 0.5 + r.left;
            tr.right = tr.left + 1.0;
        }
        let font_height = op(Op::TextFontHeight, text, 0) as c_long;
        let visible = um_floor(tr.height() / font_height as f32) as c_ulong;
        let mut f = f;
        f.visible_line_count = visible;
        let (scroll_bar, display) = if multi {
            f.scroll_bar_visible = true;
            tr.right = host.rect().right - SCROLL_BAR_WIDTH;
            let mut at = [tr.right, tr.top, tr.height(), 0.0];
            let bar = host.op_f(Op::CreateScrollBar, 0, 0, 0, 0, &mut at);
            op(Op::BarSetRange, bar, 0);
            op(Op::BarSetPageSize, bar, if visible > 0 { (visible - 1) as isize } else { 0 });
            (bar, visible as usize)
        } else {
            (0, 1)
        };
        EeUiEdit {
            host,
            st: Cell::new(EditState { f, text: tr, rect: UiRect::default(), border: 0 }),
            lines: RefCell::new(vec![empty()]),
            display: RefCell::new(vec![empty(); display]),
            ime_attrs: RefCell::new(Vec::new()),
            copied_ime_attrs: RefCell::new(Vec::new()),
            copied_text_rect: Cell::new(UiRect::default()),
            selected_bg: Cell::new([0.0; 3]),
            cursor_color: Cell::new([0.0; 3]),
            ime_color: Cell::new(ime_color),
            copied_ime_color: Cell::new(ime_color),
            scroll_bar,
            cursor,
            underline,
            rhombus,
            text,
            copied_text,
            polls,
            out: RefCell::new(empty()),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize) -> isize {
        self.host.op(op, a, b, 0, 0)
    }

    fn fields(&self) -> EditFields {
        self.st.get().f
    }

    fn update(&self, f: impl FnOnce(&mut EditFields)) {
        let mut s = self.st.get();
        f(&mut s.f);
        self.st.set(s);
    }

    fn line(&self, i: usize) -> Vec<u8> {
        self.lines.borrow().get(i).cloned().unwrap_or_else(empty)
    }

    fn display_line(&self, i: usize) -> Vec<u8> {
        self.display.borrow().get(i).cloned().unwrap_or_else(empty)
    }

    fn set_display(&self, i: usize, text: Vec<u8>) {
        if let Some(d) = self.display.borrow_mut().get_mut(i) {
            *d = text;
        }
    }

    /// `text->SetText(bytes)`, then its extent's width.
    fn extent_of(&self, text: isize, bytes: &[u8]) -> f32 {
        let t = from_bytes(body(bytes));
        self.op(Op::TextSetBytes, text, t.as_ptr() as isize);
        let mut f = [0.0f32; 4];
        self.host.op_f(Op::TextExtent, text, 0, 0, 0, &mut f);
        f[0]
    }

    fn poll(&self, p: EditPoll, op: PollOp) -> c_ulong {
        let h = self.polls[p as usize];
        match op {
            PollOp::Running => self.op(Op::PollRunning, h, 0) as c_ulong,
            PollOp::Poll => self.op(Op::PollPoll, h, 0) as c_ulong,
            PollOp::Start => {
                self.op(Op::PollStart, h, 0);
                0
            }
            PollOp::Stop => {
                self.op(Op::PollStop, h, 0);
                0
            }
        }
    }

    /// One run of the text model over the box's members.
    fn run<R>(&self, f: impl FnOnce(&mut Edit<Platform, Bridge>) -> R) -> R {
        let mut b = Bridge(self);
        let mut e = Edit::new(&Platform, &mut b);
        let r = f(&mut e);
        e.finish();
        r
    }

    /// The destructor's deletes (the original's order, then the rhombus and underline line this object made).
    pub fn release(&self) {
        self.op(Op::PollDelete, self.polls[0], 0);
        self.op(Op::PollDelete, self.polls[1], 0);
        self.op(Op::PollDelete, self.polls[2], 0);
        self.op(Op::LineDelete, self.cursor, 0);
        self.op(Op::TextDelete, self.text, 0);
        self.op(Op::TextDelete, self.copied_text, 0);
        if self.scroll_bar != 0 {
            self.op(Op::DeletePart, self.scroll_bar, 0);
        }
        self.op(Op::PollDelete, self.polls[3], 0);
        self.op(Op::LineDelete, self.underline, 0);
        self.op(Op::RhombusDelete, self.rhombus, 0);
    }

    fn copy_data(&self) -> isize {
        self.run(|e| e.check_scroll_bar());
        let mut result = self.op(Op::BaseCopyData, 0, 0);
        let cursor_poll = self.polls[EditPoll::Cursor as usize];
        if self.op(Op::PollRunning, cursor_poll, 0) != 0 && self.op(Op::PollPoll, cursor_poll, 0) as c_ulong > 0 {
            self.update(|f| f.cursor_on = !f.cursor_on);
        }
        if self.fields().cursor_on {
            let f = self.fields();
            let hidden = self.scroll_bar != 0 && {
                let value = self.op(Op::BarValue, self.scroll_bar, 0) as c_long as c_ulong;
                f.caret_line < value || f.caret_line >= value.wrapping_add(f.visible_line_count)
            };
            self.update(|f| f.copied_cursor_on = !hidden);
            if !hidden {
                let at = self.run(|e| e.location_from_index(e.s.f.caret_index as c_ulong, e.s.f.caret_line));
                let height = self.op(Op::TextFontHeight, self.text, 0) as c_long;
                let mut p = [at.x, at.y, at.x, at.y + height as f32];
                self.host.op_f(Op::LineSetPoints, self.cursor, 0, 0, 0, &mut p);
                let mut c = color_buf(self.cursor_color.get());
                self.host.op_f(Op::LineSetColor, self.cursor, 0, 0, 0, &mut c);
            }
        } else {
            self.update(|f| f.copied_cursor_on = false);
        }
        let f = self.fields();
        let (first, last): (c_ulong, c_ulong);
        if f.is_single_line {
            let line = self.line(f.caret_line as usize);
            let shown = if !f.is_password_box {
                tail(&Platform, &line, idx(f.drawing_index)).to_vec()
            } else {
                let mut stars = vec![b'*'; char_len(&Platform, &line)];
                stars.push(0);
                tail(&Platform, &stars, idx(f.drawing_index)).to_vec()
            };
            let tr = self.st.get().text;
            self.host.op_point(Op::TextSetLocation, self.text, UiPoint { x: tr.left, y: tr.top });
            self.op(Op::TextSetBytes, self.text, shown.as_ptr() as isize);
            self.set_display(0, shown);
            if f.ime_active {
                let m = edit_draw::copied_ime_single(f.ime_start_index, f.ime_composition_length, f.drawing_index);
                self.set_copied_ime(&m);
                self.copied_ime_color.set(self.ime_color.get());
                *self.copied_ime_attrs.borrow_mut() = self.ime_attrs.borrow().clone();
            } else {
                self.update(|f| f.copied_ime_active = false);
            }
            first = 0;
            last = 1;
        } else {
            first = self.op(Op::BarValue, self.scroll_bar, 0) as c_long as c_ulong;
            last = first.wrapping_add(f.visible_line_count);
            let mut i = first;
            while i < last {
                let slot = i.wrapping_sub(first) as usize;
                let n = self.lines.borrow().len();
                let shown = if (i as usize) < n {
                    let line = self.line(i as usize);
                    if has_line_end(&Platform, &line) {
                        prefix(&Platform, &line, char_len(&Platform, &line).wrapping_sub(1))
                    } else {
                        line
                    }
                } else {
                    empty()
                };
                self.set_display(slot, shown);
                i += 1;
            }
            result = self.op(Op::PartCopyData, self.scroll_bar, 0);
            if f.ime_active {
                self.copied_ime_color.set(self.ime_color.get());
                *self.copied_ime_attrs.borrow_mut() = self.ime_attrs.borrow().clone();
                let start = self.line(f.ime_start_line as usize);
                let after = char_len(&Platform, tail(&Platform, &start, f.ime_start_index as usize)) as c_long;
                let lens: Vec<c_ulong> =
                    self.lines.borrow().iter().map(|l| char_len(&Platform, l) as c_ulong).collect();
                let m = edit_draw::copied_ime_lines(
                    f.ime_start_line,
                    f.ime_start_index,
                    f.ime_composition_length,
                    first,
                    last,
                    after,
                    |i| lens.get(i as usize).copied().unwrap_or(0),
                );
                self.set_copied_ime(&m);
            } else {
                self.update(|f| f.copied_ime_active = false);
            }
        }
        self.copied_text_rect.set(self.st.get().text);
        self.op(Op::TextCopy, self.copied_text, self.text);
        if f.is_text_selected {
            let mut c = color_buf(self.selected_bg.get());
            self.host.op_f(Op::RhombusSetColor, self.rhombus, 0, 0, 0, &mut c);
            let lens: Vec<c_ulong> = self.display.borrow().iter().map(|l| char_len(&Platform, l) as c_ulong).collect();
            let f = self.fields();
            let s = edit_draw::copied_selection(
                f.caret_line,
                f.caret_index,
                f.selection_start_line,
                f.selection_start_index,
                f.drawing_index,
                first,
                last,
                |i| lens.get(i as usize).copied().unwrap_or(0),
            );
            self.update(|f| {
                f.copied_is_text_selected = s.selected;
                f.copied_selection_start_line = s.start_line;
                f.copied_selection_end_line = s.end_line;
                f.copied_selection_start_index = s.start_index;
                f.copied_selection_end_index = s.end_index;
            });
        } else {
            self.update(|f| f.copied_is_text_selected = false);
        }
        result
    }

    fn set_copied_ime(&self, m: &EditIme) {
        self.update(|f| {
            f.copied_ime_active = m.active;
            f.copied_ime_start_line = m.start_line;
            f.copied_ime_start_index = m.start_index;
            f.copied_ime_end_line = m.end_line;
            f.copied_ime_end_index = m.end_index;
        });
    }

    /// `GetSelectedTextRectangle`: the row of copied line `line` from character `from` to `to`.
    fn selected_text_rect(&self, line: c_ulong, from: c_ulong, to: c_ulong) -> UiRect {
        let height = self.op(Op::TextFontHeight, self.copied_text, 0) as c_long;
        let shown = self.display_line(line as usize);
        let before = self.extent_of(self.copied_text, &prefix(&Platform, &shown, from as usize));
        let span = prefix(&Platform, tail(&Platform, &shown, from as usize), to.wrapping_sub(from) as usize);
        let width = self.extent_of(self.copied_text, &span);
        edit_draw::selection_rect(line, height, &self.st.get().text, before, width)
    }

    fn draw_polygons(&self) -> isize {
        let mut result = self.op(Op::BaseDrawPolygons, 0, 0);
        let f = self.fields();
        if f.scroll_bar_visible {
            result = self.op(Op::PartDrawPolygons, self.scroll_bar, 0);
        }
        if f.copied_is_text_selected {
            let s = EditSelection {
                start_line: f.copied_selection_start_line,
                end_line: f.copied_selection_end_line,
                start_index: f.copied_selection_start_index,
                end_index: f.copied_selection_end_index,
                selected: true,
            };
            let mut i = s.start_line;
            while i <= s.end_line {
                let len = if (i as usize) < self.display.borrow().len() {
                    char_len(&Platform, &self.display_line(i as usize)) as c_ulong
                } else {
                    0
                };
                let (from, to) = edit_draw::selection_span(&s, i, len);
                let r = self.selected_text_rect(i, from, to);
                self.host.op_rect(Op::RhombusSetRect, self.rhombus, 0, r);
                result = crate::draw2d::rhombus(self.rhombus, || self.op(Op::RhombusDraw, self.rhombus, 0));
                if i == c_ulong::MAX {
                    break;
                }
                i += 1;
            }
        }
        if f.copied_ime_active {
            let m = EditIme {
                start_line: f.copied_ime_start_line,
                start_index: f.copied_ime_start_index,
                end_line: f.copied_ime_end_line,
                end_index: f.copied_ime_end_index,
                active: true,
            };
            let height = self.op(Op::TextFontHeight, self.copied_text, 0) as c_long as c_ulong;
            let mut u = Underlines { o: self, ch: empty() };
            let tr = self.copied_text_rect.get();
            result = edit_draw::ime_underlines(&mut u, &m, &tr, height, result as c_long) as isize;
        }
        if f.copied_cursor_on {
            result = crate::draw2d::line(self.cursor, || self.op(Op::LineDraw, self.cursor, 0));
        }
        result
    }

    fn draw_text(&self) -> isize {
        self.op(Op::DrawCountDecrement, 0, 0);
        let mut result = 0;
        let rows = self.display.borrow().clone();
        let tr = self.copied_text_rect.get();
        for (i, row) in rows.iter().enumerate() {
            let i = i as c_ulong;
            self.op(Op::TextSetBytes, self.copied_text, row.as_ptr() as isize);
            let height = self.op(Op::TextFontHeight, self.copied_text, 0) as c_long;
            let y = edit_draw::text_row_y(i, height, tr.top);
            self.host.op_point(Op::TextSetLocation, self.copied_text, UiPoint { x: tr.left, y });
            result = self.host.op_rect(Op::DrawClippedText, self.copied_text, 0, tr);
            let f = self.fields();
            if !(f.copied_ime_active && i >= f.copied_ime_start_line && i <= f.copied_ime_end_line) {
                continue;
            }
            if f.copied_ime_start_line == i {
                let before = prefix(&Platform, row, f.copied_ime_start_index as usize);
                let w = self.extent_of(self.copied_text, &before);
                self.host.op_point(Op::TextSetLocation, self.copied_text, UiPoint { x: w + tr.left, y });
            }
            let mut s = row.clone();
            if f.copied_ime_end_line == i {
                s = prefix(&Platform, &s, f.copied_ime_end_index as usize);
            }
            let shown =
                if f.copied_ime_start_line == i { tail(&Platform, &s, f.copied_ime_start_index as usize) } else { &s };
            let mut color = [0.0f32; 4];
            self.host.op_f(Op::TextFgColor, self.copied_text, 0, 0, 0, &mut color);
            let mut c = color_buf(self.copied_ime_color.get());
            self.host.op_f(Op::TextSetFgColor, self.copied_text, 0, 0, 0, &mut c);
            let shown = shown.to_vec();
            self.op(Op::TextSetBytes, self.copied_text, shown.as_ptr() as isize);
            result = self.host.op_rect(Op::DrawClippedText, self.copied_text, 0, tr);
            self.host.op_f(Op::TextSetFgColor, self.copied_text, 0, 0, 0, &mut color);
        }
        if self.fields().scroll_bar_visible {
            result = self.op(Op::PartDrawText, self.scroll_bar, 0);
        }
        result
    }

    /// `SetupKeyboardForCapture`.
    fn setup_keyboard(&self) {
        self.update(|f| f.cursor_on = true);
        self.op(Op::CaptureKeyboard, 0, 0);
        let dirty = self.op(Op::AlwaysDirty, 0, 0) != 0;
        self.update(|f| f.saved_always_dirty = dirty);
        self.op(Op::SetAlwaysDirty, 1, 0);
    }

    fn mouse_down(&self) -> isize {
        let mut result = 0;
        let bar_hit = self.scroll_bar != 0 && {
            let r = self.host.get_rect(Op::PartRect, self.scroll_bar);
            r.contains(self.host.mouse())
        };
        if bar_hit {
            result = self.op(Op::PartMouseDown, self.scroll_bar, 0);
            self.update(|f| f.scroll_bar_captured = true);
            self.op(Op::CaptureMouse, 0, 0);
        } else if !self.fields().ime_active {
            if self.op(Op::Tabable, 0, 0) != 0 {
                let index = self.op(Op::Index, 0, 0);
                if self.op(Op::TabStop, 0, 0) as c_ulong != index as c_ulong {
                    self.op(Op::SetTabStop, index, 0);
                }
            }
            let at = self.host.mouse();
            self.run(|e| {
                let (line, index) = e.caret_from_mouse(at);
                e.s.f.caret_line = line;
                e.s.f.caret_index = index;
                e.compute_caret_location(true);
            });
            self.update(|f| {
                f.selection_start_index = f.caret_index;
                f.selecting = true;
                f.selection_start_line = f.caret_line;
            });
            self.setup_keyboard();
            self.op(Op::ButtonDown, 0, 0);
            self.op(Op::CaptureMouse, 0, 0);
        }
        self.op(Op::PollStop, self.polls[EditPoll::Cursor as usize], 0);
        result
    }

    fn mouse_up(&self) -> isize {
        let result;
        if self.fields().scroll_bar_captured {
            result = self.op(Op::PartMouseUp, self.scroll_bar, 0);
            self.update(|f| f.scroll_bar_captured = false);
        } else {
            result = 0;
            self.update(|f| f.selecting = false);
        }
        self.op(Op::PollStart, self.polls[EditPoll::Cursor as usize], 0);
        self.op(Op::ReleaseMouse, 0, 0);
        result
    }

    fn release_keyboard(&self) {
        if self.fields().ime_active {
            self.run(|e| e.ime_remove());
            self.op(Op::TextInputCancel, 0, 0);
            self.update(|f| f.ime_active = false);
        }
        self.op(Op::SetAlwaysDirty, self.fields().saved_always_dirty as isize, 0);
        self.op(Op::BaseReleaseKeyboard, 0, 0);
    }

    fn tabbed_from(&self) {
        self.op(Op::PollStop, self.polls[EditPoll::Cursor as usize], 0);
        self.update(|f| f.cursor_on = false);
        let mut o = [self.fields().saved_bg_opacity, 0.0, 0.0, 0.0];
        self.host.op_f(Op::SetBgOpacity, 0, 0, 0, 0, &mut o);
        self.update(|f| {
            f.is_text_selected = false;
            f.selecting = false;
            f.scroll_bar_captured = false;
        });
        self.op(Op::ReleaseKeyboard, 0, 0);
        self.op(Op::ReleaseMouse, 0, 0);
        self.op(Op::TextInputCancel, 0, 0);
        self.op(Op::SetDirty, 1, 0);
    }

    fn tabbed_to(&self) {
        self.op(Op::PollStart, self.polls[EditPoll::Cursor as usize], 0);
        let mut o = [0.0f32; 4];
        self.host.op_f(Op::BgOpacity, 0, 0, 0, 0, &mut o);
        self.update(|f| f.saved_bg_opacity = o[0]);
        let mut one = [1.0f32, 0.0, 0.0, 0.0];
        self.host.op_f(Op::SetBgOpacity, 0, 0, 0, 0, &mut one);
        self.setup_keyboard();
        self.op(Op::TextInputCancel, 0, 0);
        self.op(Op::SetDirty, 1, 0);
    }

    /// An answer's bytes kept for the C++ to copy.
    fn answer(&self, bytes: Vec<u8>) -> isize {
        let mut out = self.out.borrow_mut();
        *out = bytes;
        out.as_ptr() as isize
    }

    /// One of UIEditBox's methods ([`EeUiEditCall`]).
    ///
    /// # Safety
    /// `f` is NULL or four live floats; a text argument is NUL-terminated bytes; `CaretFromMouse`'s `a` is NULL or a
    /// live `long`.
    pub unsafe fn call(&self, call: EeUiEditCall, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
        use EeUiEditCall as C;
        // SAFETY: the contract
        let buf = unsafe { read_buf(f) };
        let point = UiPoint { x: buf[0], y: buf[1] };
        let color = [buf[0], buf[1], buf[2]];
        let fl = self.fields();
        match call {
            C::AddImeResult => {
                // SAFETY: the contract (copied first: typing it comes back into the box)
                let text = unsafe { bytes_at(a) };
                self.run(|e| e.add_ime_result(&text));
            }
            C::CaretFromMouse => {
                let (line, index) = self.run(|e| e.caret_from_mouse(point));
                if a != 0 {
                    // SAFETY: the contract
                    unsafe { (a as *mut c_long).write(index) };
                }
                return line as isize;
            }
            C::ImeChange => {
                // SAFETY: the contract
                let (text, attrs) = unsafe { (bytes_at(a), bytes_at(c)) };
                let active = fl.ime_active;
                let r = self.run(|e| e.ime_change(&text, b as c_ulong));
                if active {
                    *self.ime_attrs.borrow_mut() = body(&attrs).to_vec();
                }
                return r as isize;
            }
            C::ChangeFont => {
                if self.text != 0 {
                    self.op(Op::TextSetFont, self.text, a);
                    self.op(Op::TextSetFont, self.copied_text, a);
                }
            }
            C::ChangeWidth => self.run(|e| e.change_width(a as c_ulong)),
            C::ImeClose => return self.run(|e| e.ime_close()) as isize,
            C::Copy => self.run(|e| e.copy()),
            C::CopyData => return self.copy_data(),
            C::Cut => self.run(|e| e.cut()),
            C::DeleteSelectedText => self.run(|e| e.delete_selected_text()),
            C::DeleteRange => {
                self.run(|e| e.delete_range(a as c_ulong, b as c_ulong, c as c_ulong, d as c_ulong));
            }
            C::Clear => self.run(|e| e.clear()),
            C::DrawPolygons => return self.draw_polygons(),
            C::DrawText => return self.draw_text(),
            C::AllText => {
                let text = self.run(|e| {
                    let last = e.h.line_count().wrapping_sub(1) as c_ulong;
                    let end = e.len(last);
                    e.text(0, last, 0, end)
                });
                return self.answer(text);
            }
            C::CaretLocation => {
                let at = self.run(|e| e.location_from_index(e.s.f.caret_index as c_ulong, e.s.f.caret_line));
                // SAFETY: the contract
                unsafe { write_buf(f, [at.x, at.y, 0.0, 0.0]) };
            }
            C::CursorOn => return fl.cursor_on as isize,
            C::Text => return self.text,
            C::ImeEnabled => return fl.ime_active as isize,
            C::IsPasswordBox => return fl.is_password_box as isize,
            C::IsTextSelected => return fl.is_text_selected as isize,
            C::LineCount => return self.lines.borrow().len() as isize,
            C::SelectedText => {
                return match self.run(|e| e.selected_text()) {
                    Some(text) => self.answer(text),
                    None => 0,
                };
            }
            C::SelectionStartIndex => return fl.selection_start_index as isize,
            C::CaretIndex => return fl.caret_index as isize,
            C::StartingDisplayIndex => return fl.drawing_index as isize,
            C::Line => {
                return match self.lines.borrow().get(a as usize) {
                    Some(l) if a >= 0 => l.as_ptr() as isize,
                    _ => 0,
                };
            }
            C::Paste => self.run(|e| e.paste()),
            C::ImeOpen => return self.run(|e| e.ime_open()) as isize,
            C::ReleaseKeyboard => self.release_keyboard(),
            C::SelectAll => self.run(|e| e.select_all()),
            C::Select => self.run(|e| e.select(a as c_ulong, b as c_ulong, c as c_ulong, d as c_ulong)),
            C::SetCaretLocation => self.run(|e| e.set_caret_location(a as c_ulong, b as c_ulong)),
            C::SetCharScroll => self.update(|f| f.char_scroll = a as c_ulong),
            C::SetCursorColor => self.cursor_color.set(color),
            C::SetImeTextColor => self.ime_color.set(color),
            C::SetIsPasswordBox => self.update(|f| f.is_password_box = f.is_single_line && a != 0),
            C::SetLocation => self.run(|e| e.set_location(point)),
            C::SetSelectedBgColor => self.selected_bg.set(color),
            C::SetText => {
                // SAFETY: the contract
                let text = unsafe { bytes_at(a) };
                self.run(|e| e.set_text(&text));
            }
            C::UpdateKeyboard => return self.run(|e| e.update_keyboard()) as isize,
            C::UpdateTranslatedKey => {
                // SAFETY: the contract
                let text = unsafe { bytes_at(a) };
                self.run(|e| e.type_key(&text, b as c_short));
                let cursor = self.polls[EditPoll::Cursor as usize];
                self.op(Op::PollStop, cursor, 0);
                self.op(Op::PollStart, cursor, 0);
                self.update(|f| f.cursor_on = true);
            }
            C::MouseDoublePressed => {
                let mut m = [0.0f32; 4];
                self.host.op_f(Op::FormMouseLocation, 0, 0, 0, 0, &mut m);
                if self.st.get().text.contains(UiPoint { x: m[0], y: m[1] }) {
                    self.run(|e| e.select_all());
                } else {
                    return self.op(Op::BaseMouseDoublePressed, 0, 0);
                }
            }
            C::MouseLButtonDown => return self.mouse_down(),
            C::MouseLButtonUp => return self.mouse_up(),
            C::MouseMove => {
                if fl.scroll_bar_captured {
                    return self.host.op_point(Op::PartMouseMove, self.scroll_bar, point);
                } else if fl.selecting {
                    self.run(|e| e.drag_to(point));
                }
            }
            C::ProcessEnterKey => self.run(|e| e.enter_key()),
            C::TabbedFrom => self.tabbed_from(),
            C::TabbedTo => self.tabbed_to(),
        }
        0
    }
}

// ---- the C ABI

/// A new Rust edit box for the C++ `edit` (its `UIControl` part constructed): `flags` bit 0 numbers only, bit 1
/// multi-line, bit 2 a password box.
///
/// # Safety
/// `host` answers the ops for `edit`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_new(
    edit: *mut c_void,
    host: EeUiHostFn,
    font: *mut c_void,
    max_chars: c_ulong,
    flags: u32,
    border: c_ulong,
) -> *mut EeUiEdit {
    crate::direct::made(
        crate::direct::Kind::Edit,
        edit,
        Box::into_raw(Box::new(EeUiEdit::new(Host { obj: edit, f: host }, font as isize, max_chars, flags, border))),
    )
}

/// The destructor: deletes the polls, lines, texts and scroll bar and frees the object.
///
/// # Safety
/// `edit` came from `ee_ui_edit_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_free(edit: *mut EeUiEdit) {
    crate::direct::forget(crate::direct::Kind::Edit, edit as *const core::ffi::c_void);
    if edit.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*edit).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(edit) });
}

/// One of UIEditBox's methods ([`EeUiEditCall`]).
///
/// # Safety
/// `edit` is live; see [`EeUiEdit::call`].
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_call(
    edit: *const EeUiEdit,
    call: EeUiEditCall,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*edit).call(call, a, b, c, d, f) }
}

#[cfg(test)]
#[path = "editobj_tests.rs"]
mod tests;
