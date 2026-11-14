//! `UITextLabel` and `UITLScrolling` as one whole Rust object (R6 ui round 13): the label's state (its lines, the
//! display copies, the rectangles, the scroll bar's colours, the flags) and every method of `UITextLabel.cpp` and
//! the header's; a scrolling label's own state ([`Scroll`]) and methods (`UITLScrolling.cpp`) when its constructor's
//! init call made it one.
//!
//! The C++ classes stay the types the forms, combo boxes, check boxes and game screens hold: their methods are
//! one-line forwarders to [`ee_ui_label_call`], the constructor makes this object and the destructor frees it.
//! Every call out is an [`EeUiOp`] through the host: `UIControl`'s members, the label's own virtuals (so a subclass's
//! override runs: `CheckScrollBar`, `RecomputeDisplayRange`, `SetLocation`, `SetRectangle`), the scroll bar (a part
//! held by address), the two or three `GEText`s, the polls.
//!
//! **The lines.** Each line is a C++ `UWideString` the host made (`StringNew` from the line's bytes) and this object
//! holds by address beside a copy of its bytes: `GetText(i)` still answers a `const UWideString&`, and the re-wrap
//! of `RecomputeDisplayRange` reads the copies instead of the strings. The display copies (`mDisplayLines`) and a
//! scrolling label's draw lines are strings held by address too (assigned with `StringAssign`).
//!
//! The geometry (`label_fit.rs`), the wrapping (`wrap.rs`), the draw places (`draw.rs`) and the scroll
//! (`scrolling.rs`) are the earlier rounds' algorithms with their goldens; this object is their host.

use core::cell::{Cell, OnceCell, RefCell};
use core::ffi::{c_char, c_long, c_ulong, c_void, CStr};

use crate::draw::{label_line, label_line_range, label_one_line_location};
use crate::label_fit::{self, LabelHost, SCROLL_BAR_WIDTH};
use crate::obj::{read_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::{um_floor, UiPoint, UiRect};
use crate::scrolling::{self, PollCall, ScrollField, ScrollHost, ScrollPoll, TRIGGER_SCROLLING_FINISHED};
use crate::text::Platform;
use crate::wrap::split_lines;

/// `kErrorFailed`.
const LABEL_ERROR_FAILED: isize = 9;

/// The inward calls (the C++ shims' methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiLabelCall {
    /// The constructor's text: `SetText(a, true)` (a NUL-terminated bytes).
    Init = 1,
    /// `SetText(a, b != 0)` (the private one; a NUL-terminated bytes).
    SetText = 2,
    /// `ChangeFont(a)`.
    ChangeFont = 3,
    /// `CopyData()` (a scrolling label's own).
    CopyData = 4,
    /// `DeleteArray()`.
    DeleteArray = 5,
    /// `DeleteLine(a)`.
    DeleteLine = 6,
    /// `DrawPolygons()`.
    DrawPolygons = 7,
    /// `DrawTextA()` (a scrolling label's own).
    DrawText = 8,
    /// `MouseLButtonDown()`.
    MouseLButtonDown = 9,
    /// `MouseLButtonUp()`.
    MouseLButtonUp = 10,
    /// `MouseMove(f[0], f[1])`.
    MouseMove = 11,
    /// `MouseWheelTurned(a)`.
    MouseWheelTurned = 12,
    /// `RecomputeDisplayRange()`.
    RecomputeDisplayRange = 13,
    /// `SetFitRectToText(a != 0)`.
    SetFitRectToText = 14,
    /// `SetLocation(f[0], f[1])`.
    SetLocation = 15,
    /// `SetRectangle(f)`.
    SetRectangle = 16,
    /// `CheckScrollBar()` (a scrolling label's does nothing).
    CheckScrollBar = 17,
    /// `GetFitRectToText()`.
    FitRectToText = 20,
    /// `GetGEText()`.
    Text = 21,
    /// `GetLineCount()`.
    LineCount = 22,
    /// `GetNumberLinesDisplayed()`.
    LinesDisplayed = 23,
    /// `GetScrollBar()`: the scroll bar unless it is marked removed, else 0.
    ScrollBar = 24,
    /// `ScrollBarExists()`.
    ScrollBarExists = 25,
    /// `GetText(a)`: the line's `UWideString`.
    Line = 26,
    /// `GetTextRectangle()`: the address of the rectangle (a `GERectangle`).
    TextRectangle = 27,
    /// `GetCopiedTextRectangle()`: its address.
    CopiedTextRectangle = 28,
    /// `GetScrollBarRectangle()`: its address.
    ScrollBarRectangle = 29,
    /// `SetAlignment(a)`.
    SetAlignment = 30,
    /// `SetStartingDisplayLine(a)`.
    SetStartingDisplayLine = 31,
    /// `SetScrollBarBgOpacity(f[0])`.
    SetScrollBarBgOpacity = 32,
    /// `SetScrollBarBgColor(f[0..3])`.
    SetScrollBarBgColor = 33,
    /// `SetTextRectangle(f)`.
    SetTextRectangle = 34,

    // ---- UITLScrolling
    /// The constructor's part after `UITextLabel`'s: the polls, the draw text (font `a`), the offsets.
    ScrollInit = 50,
    /// `PauseScrolling()`.
    PauseScrolling = 51,
    /// `ResumeScrolling()`.
    ResumeScrolling = 52,
    /// `ScrollText(f[0] speed, a start delay, b end delay, f[1] start offset, f[2] end offset)`: the offsets are
    /// `long`s whose bits travel in the floats.
    ScrollText = 53,
    /// `ScrollText(a time, b end delay)`.
    ScrollTextFor = 54,
    /// `StopScrolling()`.
    StopScrolling = 55,
    /// `GetPixelStartingOffset()`.
    PixelStartingOffset = 56,
    /// `GetStartingLine()`.
    StartingLine = 57,
    /// `SetPixelStartingOffset(a)`.
    SetPixelStartingOffset = 58,
    /// `SetStartingLine(a)`.
    SetStartingLine = 59,
}

/// A line: the C++ string and a copy of its bytes (no NUL).
struct Line {
    s: isize,
    bytes: Vec<u8>,
}

/// A scrolling label's own state (`UITLScrolling`'s members).
pub struct Scroll {
    /// `mDrawLines`: the visible lines as of the last `CopyData` (strings by address).
    draw_lines: RefCell<Vec<isize>>,
    /// `mDrawRectangle`.
    draw_rect: Cell<UiRect>,
    /// `mDrawText`.
    draw_text: Cell<isize>,
    /// `mScrollPoll`, `mStartPoll`, `mEndPoll`.
    polls: [isize; 3],
    starting_line: Cell<c_ulong>,
    draw_starting_line: Cell<c_ulong>,
    end_offset: Cell<c_ulong>,
    draw_line_count: Cell<c_ulong>,
    pixel_starting_offset: Cell<c_long>,
    draw_pixel_offset: Cell<c_ulong>,
    scrolling: Cell<bool>,
}

/// The Rust UITextLabel (and UITLScrolling).
pub struct EeUiLabel {
    host: Host,
    /// `mLines`.
    lines: RefCell<Vec<Line>>,
    /// `mDisplayLines` (`new UWideString[n]`): the lines `CopyData` hands to `DrawTextA`.
    display: RefCell<Vec<isize>>,
    original: Cell<UiRect>,
    scroll_bar_rect: Cell<UiRect>,
    text_rect: Cell<UiRect>,
    copied_text_rect: Cell<UiRect>,
    bg_color: Cell<[f32; 3]>,
    bg_opacity: Cell<f32>,
    copied_text: Cell<isize>,
    text: Cell<isize>,
    scroll_bar: Cell<isize>,
    displayed: Cell<c_ulong>,
    last_display_line: Cell<c_long>,
    label_type: c_ulong,
    alignment: Cell<c_ulong>,
    fit: Cell<bool>,
    removed: Cell<bool>,
    draw_scroll_bar: Cell<bool>,
    recompute_pending: Cell<bool>,
    /// A scrolling label's part (set by [`EeUiLabelCall::ScrollInit`]).
    scroll: OnceCell<Scroll>,
}

/// NUL-terminated bytes.
fn terminated(bytes: &[u8]) -> Vec<u8> {
    let mut v = Vec::with_capacity(bytes.len() + 1);
    v.extend_from_slice(bytes);
    v.push(0);
    v
}

/// The visible rows of a label of height `height`: `UMFloor((height - 2 * border) / font height)`, at least 1
/// (the C++'s `unsigned long` doubling, its `long` font height as a float).
fn rows(height: f32, border: c_ulong, font_height: c_long) -> c_ulong {
    let n = um_floor((height - border.wrapping_mul(2) as f32) / font_height as f32) as c_ulong;
    if n == 0 {
        1
    } else {
        n
    }
}

impl EeUiLabel {
    /// The constructor's part after `UIControl`'s (`ctor` the rectangle given, `border` its thickness): the
    /// rectangles, the two texts, the visible rows and the display copies. The text is set by the first inward call
    /// ([`EeUiLabelCall::Init`]) once the C++ object holds this one.
    pub fn new(host: Host, ctor: UiRect, font: isize, label_type: c_ulong, border: c_ulong) -> Self {
        let r = host.rect();
        let b = border as f32;
        let mut text_rect = UiRect::new(r.left + b + 1.0, r.top + b + 1.0, r.right - b - 1.0, r.bottom - b - 1.0);
        let text = host.op(Op::TextCreate, font, 0, 0, 0);
        let copied = host.op(Op::TextCreate, font, 0, 0, 0);
        let border_now = host.op(Op::BorderThickness, 0, 0, 0, 0) as c_ulong;
        let displayed = rows(ctor.height(), border_now, host.op(Op::TextFontHeight, text, 0, 0, 0) as c_long);
        let mut scroll_bar_rect = UiRect::default();
        let n = if label_type == 1 {
            text_rect.right = r.right - SCROLL_BAR_WIDTH - 1.0;
            scroll_bar_rect = UiRect::new(r.right - SCROLL_BAR_WIDTH + 1.0, r.top, r.right, r.bottom);
            displayed
        } else {
            1
        };
        let display = (0..n).map(|_| host.op(Op::StringNew, c"".as_ptr() as isize, 0, 0, 0)).collect();
        EeUiLabel {
            host,
            lines: RefCell::new(Vec::new()),
            display: RefCell::new(display),
            original: Cell::new(r),
            scroll_bar_rect: Cell::new(scroll_bar_rect),
            text_rect: Cell::new(text_rect),
            copied_text_rect: Cell::new(UiRect::default()),
            bg_color: Cell::new([0.0; 3]),
            bg_opacity: Cell::new(0.0),
            copied_text: Cell::new(copied),
            text: Cell::new(text),
            scroll_bar: Cell::new(0),
            displayed: Cell::new(displayed),
            last_display_line: Cell::new(0),
            label_type,
            alignment: Cell::new(0),
            fit: Cell::new(false),
            removed: Cell::new(false),
            draw_scroll_bar: Cell::new(false),
            recompute_pending: Cell::new(false),
            scroll: OnceCell::new(),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize) -> isize {
        self.host.op(op, a, b, 0, 0)
    }

    fn op1(&self, op: Op, a: isize, f0: f32) -> isize {
        let mut f = [f0, 0.0, 0.0, 0.0];
        self.host.op_f(op, a, 0, 0, 0, &mut f)
    }

    fn string_new(&self, bytes: &[u8]) -> isize {
        let t = terminated(bytes);
        self.op(Op::StringNew, t.as_ptr() as isize, 0)
    }

    fn text_set_bytes(&self, text: isize, bytes: &[u8]) {
        let t = terminated(bytes);
        self.op(Op::TextSetBytes, text, t.as_ptr() as isize);
    }

    fn extent(&self, text: isize) -> (f32, f32) {
        let p = self.host.get_point(Op::TextExtent, text);
        (p.x, p.y)
    }

    fn font_height(&self, text: isize) -> c_long {
        self.op(Op::TextFontHeight, text, 0) as c_long
    }

    fn bar_value(&self) -> c_long {
        self.op(Op::BarValue, self.scroll_bar.get(), 0) as c_long
    }

    fn line(&self, i: usize) -> Option<isize> {
        self.lines.borrow().get(i).map(|l| l.s)
    }

    // ---- the label's virtuals, through the C++ object (a subclass's override runs)
    fn virt_check_scroll_bar(&self) {
        self.op(Op::VirtCheckScrollBar, 0, 0);
    }
    fn virt_set_rectangle(&self, r: UiRect) {
        self.host.op_rect(Op::VirtSetRectangle, 0, 0, r);
    }

    /// The destructor: a scrolling label's part first (its polls, draw text and draw lines), then the lines, the
    /// scroll bar, the texts and the display copies.
    pub fn release(&self) {
        if let Some(s) = self.scroll.get() {
            for p in s.polls {
                self.op(Op::PollDelete, p, 0);
            }
            if s.draw_text.get() != 0 {
                self.op(Op::TextDelete, s.draw_text.get(), 0);
            }
            let lines = core::mem::take(&mut *s.draw_lines.borrow_mut());
            for l in lines {
                self.op(Op::StringDelete, l, 0);
            }
        }
        let lines = core::mem::take(&mut *self.lines.borrow_mut());
        for l in lines {
            self.op(Op::StringDelete, l.s, 0);
        }
        if self.scroll_bar.get() != 0 {
            self.op(Op::DeletePart, self.scroll_bar.get(), 0);
        }
        self.op(Op::TextDelete, self.text.get(), 0);
        self.op(Op::TextDelete, self.copied_text.get(), 0);
        let display = core::mem::take(&mut *self.display.borrow_mut());
        for s in display {
            self.op(Op::StringDelete, s, 0);
        }
    }

    // ---- UITextLabel.cpp

    /// `SetText(text, replace)`: one line as is, or word-wrapped lines for a multi-line label; dirty.
    fn set_text(&self, text: &[u8], replace: bool) {
        if replace {
            self.delete_array();
        }
        if self.fit.get() {
            let o = self.original.get();
            self.virt_set_rectangle(o);
            self.host.op_rect(Op::RedrawIntersect, 0, 0, o);
        }
        if self.label_type == 0 {
            let s = self.string_new(text);
            self.lines.borrow_mut().push(Line { s, bytes: text.to_vec() });
        } else {
            let measured = self.text.get();
            let width = self.text_rect.get().width();
            let mut made = Vec::new();
            split_lines(
                &Platform,
                &mut |word: &[u8]| {
                    self.text_set_bytes(measured, word);
                    self.extent(measured).0
                },
                &terminated(text),
                width,
                &mut |line: &[u8]| {
                    let s = self.string_new(line);
                    made.push(Line { s, bytes: line.to_vec() });
                },
            );
            self.lines.borrow_mut().extend(made);
        }
        self.op(Op::SetDirty, 1, 0);
        self.virt_check_scroll_bar();
    }

    /// `DeleteArray`: every line deleted; dirty; the scroll bar checked.
    fn delete_array(&self) {
        let lines = core::mem::take(&mut *self.lines.borrow_mut());
        for l in lines {
            self.op(Op::StringDelete, l.s, 0);
        }
        self.op(Op::SetDirty, 1, 0);
        self.virt_check_scroll_bar();
    }

    /// `DeleteLine(i)` (a line past the end: nothing, where the C++ read past its vector).
    fn delete_line(&self, i: usize) {
        let s = {
            let mut lines = self.lines.borrow_mut();
            if i >= lines.len() {
                return;
            }
            lines.remove(i).s
        };
        self.op(Op::StringDelete, s, 0);
        self.virt_check_scroll_bar();
    }

    fn change_font(&self, font: isize) {
        if self.text.get() == 0 {
            return;
        }
        self.op(Op::TextSetFont, self.copied_text.get(), font);
        self.op(Op::TextSetFont, self.text.get(), font);
        label_fit::compute_rect(&mut Fit(self));
    }

    fn check_scroll_bar(&self) {
        if self.scroll.get().is_some() {
            return;
        }
        label_fit::check_scroll_bar(&mut Fit(self));
    }

    fn copy_data(&self) -> isize {
        if let Some(s) = self.scroll.get() {
            self.op(Op::TextCopy, s.draw_text.get(), self.text.get());
            return scrolling::copy_data(&mut Scrolling(self, s)) as isize;
        }
        let copied = self.copied_text.get();
        self.op(Op::TextCopy, copied, self.text.get());
        self.copied_text_rect.set(self.text_rect.get());
        if self.label_type == 0 {
            match self.line(0) {
                Some(s) => self.op(Op::TextSetText, copied, s),
                None => {
                    self.text_set_bytes(copied, b"");
                    0
                }
            };
            let (w, h) = self.extent(copied);
            let at = label_one_line_location(self.alignment.get(), self.fit.get(), &self.text_rect.get(), w, h);
            self.host.op_point(Op::TextSetLocation, copied, at);
        } else {
            if self.removed.get() {
                self.op(Op::DeletePart, self.scroll_bar.get(), 0);
                self.removed.set(false);
                self.scroll_bar.set(0);
            }
            if self.recompute_pending.get() {
                self.op(Op::VirtRecompute, 0, 0);
            }
            let bar = self.scroll_bar.get();
            let value = (bar != 0).then(|| self.bar_value());
            let lines = self.lines.borrow().len() as c_ulong;
            let range = label_line_range(value, self.displayed.get(), lines);
            self.last_display_line.set(range.last);
            self.draw_scroll_bar.set(bar != 0);
            if bar != 0 {
                self.op(Op::PartCopyData, bar, 0);
            }
            let mut i = range.first;
            let mut j = 0usize;
            while i < range.end {
                let (Some(dst), Some(src)) = (self.display.borrow().get(j).copied(), self.line(i as usize)) else {
                    break;
                };
                self.op(Op::StringAssign, dst, src);
                i += 1;
                j += 1;
            }
        }
        self.op(Op::BaseCopyData, 0, 0)
    }

    fn draw_polygons(&self) -> isize {
        let mut r = self.op(Op::BaseDrawPolygons, 0, 0);
        if self.draw_scroll_bar.get() {
            r = self.op(Op::PartDrawPolygons, self.scroll_bar.get(), 0);
        }
        r
    }

    fn draw_text(&self) -> isize {
        if let Some(s) = self.scroll.get() {
            return self.scroll_draw_text(s);
        }
        let copied = self.copied_text.get();
        let line_height = self.font_height(copied) as c_ulong;
        self.op(Op::DrawCountDecrement, 0, 0);
        let mut r;
        if self.label_type == 0 {
            let d = self.host.get_rect(Op::CopiedDrawRect, 0);
            r = self.host.op_rect(Op::DrawClippedText, copied, 0, d);
        } else {
            r = 0;
            let mut offset: c_ulong = 0;
            let mut i: c_long = 0;
            while i <= self.last_display_line.get() {
                let Some(s) = self.display.borrow().get(i as usize).copied() else {
                    break;
                };
                self.op(Op::TextSetText, copied, s);
                let w = self.extent(copied).0;
                let bar = self.scroll_bar.get();
                let scroll_width = (bar != 0).then(|| self.host.get_rect(Op::PartCopiedDrawRect, bar).width());
                let (at, clip) = label_line(
                    self.alignment.get(),
                    self.fit.get(),
                    &self.host.rect(),
                    &self.copied_text_rect.get(),
                    w,
                    scroll_width,
                    offset,
                    line_height,
                );
                self.host.op_point(Op::TextSetLocation, copied, at);
                r = self.host.op_rect(Op::DrawClippedText, copied, 0, clip);
                i += 1;
                offset = offset.wrapping_add(line_height);
            }
        }
        if self.draw_scroll_bar.get() {
            r = self.op(Op::PartDrawText, self.scroll_bar.get(), 0);
        }
        r
    }

    /// The three mouse handlers: forwarded to the scroll bar (the button down only inside its strip, capturing the
    /// mouse; the button up releasing it); a scroll refits the label.
    fn mouse(&self, call: EeUiLabelCall, at: UiPoint) -> isize {
        let bar = self.scroll_bar.get();
        if bar == 0 {
            return 0;
        }
        if call == EeUiLabelCall::MouseLButtonDown && !self.scroll_bar_rect.get().contains(self.host.mouse()) {
            return 0;
        }
        let old = self.bar_value();
        let r = match call {
            EeUiLabelCall::MouseLButtonDown => {
                let r = self.op(Op::PartMouseDown, bar, 0);
                self.op(Op::CaptureMouse, 0, 0);
                r
            }
            EeUiLabelCall::MouseLButtonUp => {
                let r = self.op(Op::PartMouseUp, bar, 0);
                self.op(Op::ReleaseMouse, 0, 0);
                r
            }
            _ => self.host.op_point(Op::PartMouseMove, bar, at),
        };
        if old != self.bar_value() {
            label_fit::compute_rect(&mut Fit(self));
        }
        r
    }

    fn mouse_wheel(&self, delta: isize) -> isize {
        let bar = self.scroll_bar.get();
        if bar == 0 {
            return LABEL_ERROR_FAILED;
        }
        self.op(Op::BarIncrementBy, bar, delta as c_long as isize);
        0
    }

    /// `RecomputeDisplayRange`: the visible rows from the rectangle, new display copies, every line re-wrapped, the
    /// scroll bar rebuilt.
    fn recompute(&self) {
        let r = self.host.rect();
        let border = self.op(Op::BorderThickness, 0, 0) as c_ulong;
        let displayed = rows(r.height(), border, self.font_height(self.text.get()));
        self.displayed.set(displayed);
        let old = core::mem::take(&mut *self.display.borrow_mut());
        for s in old {
            self.op(Op::StringDelete, s, 0);
        }
        let display: Vec<isize> = (0..displayed).map(|_| self.string_new(b"")).collect();
        *self.display.borrow_mut() = display;
        let mut text = Vec::new();
        for l in self.lines.borrow().iter() {
            text.extend_from_slice(&l.bytes);
            text.push(b'\n');
        }
        self.set_text(&text, true);
        self.removed.set(false);
        self.recompute_pending.set(false);
        let at = self.host.rect().top_left();
        self.host.op_point(Op::VirtSetLocation, 0, at);
        self.virt_check_scroll_bar();
        self.op(Op::TextCopy, self.copied_text.get(), self.text.get());
        self.copied_text_rect.set(self.text_rect.get());
    }

    fn set_location(&self, at: UiPoint) {
        self.host.op_point(Op::BaseSetLocation, 0, at);
        let border = self.op(Op::BorderThickness, 0, 0) as c_ulong;
        let bar = self.scroll_bar.get();
        let (s, t) = label_fit::location_rects(self.host.rect(), border, bar != 0);
        self.scroll_bar_rect.set(s);
        self.text_rect.set(t);
        if bar != 0 {
            self.host.op_point(Op::PartSetLocation, bar, s.top_left());
        }
    }

    fn set_rectangle(&self, r: UiRect) {
        self.host.op_rect(Op::BaseSetRectangle, 0, 0, r);
        if !self.fit.get() {
            self.removed.set(true);
            self.recompute_pending.set(true);
        } else {
            self.recompute_pending.set(false);
        }
    }

    // ---- UITLScrolling.cpp

    fn scroll_init(&self, font: isize) {
        let polls = [0; 3].map(|_| self.op(Op::PollCreate, 1000, 0));
        let draw_text = self.op(Op::TextCreate, font, 0);
        let r = self.host.rect();
        let b = self.op(Op::BorderThickness, 0, 0) as c_ulong as f32;
        let s = Scroll {
            draw_lines: RefCell::new(Vec::new()),
            draw_rect: Cell::new(UiRect::default()),
            draw_text: Cell::new(draw_text),
            polls,
            starting_line: Cell::new(0),
            draw_starting_line: Cell::new(0),
            end_offset: Cell::new(0),
            draw_line_count: Cell::new(0),
            pixel_starting_offset: Cell::new(um_floor(r.top + b + 1.0)),
            draw_pixel_offset: Cell::new(0),
            scrolling: Cell::new(false),
        };
        // a second init (never made) is ignored
        let _ = self.scroll.set(s);
        self.text_rect.set(UiRect::new(r.left + b + 1.0, r.top + b + 1.0, r.right - b - 1.0, r.bottom - b - 1.0));
    }

    fn scroll_draw_text(&self, s: &Scroll) -> isize {
        let mut r = self.op(Op::BaseDrawText, 0, 0);
        let offset = s.draw_pixel_offset.get();
        let text = s.draw_text.get();
        let mut i: c_ulong = 0;
        while i < s.draw_line_count.get() {
            let Some(l) = s.draw_lines.borrow().get(i as usize).copied() else {
                break;
            };
            self.op(Op::TextSetText, text, l);
            let rect = s.draw_rect.get();
            let (x, y) = scrolling::draw_line_at(rect.left, self.font_height(text) as c_ulong, i, offset);
            self.host.op_point(Op::TextSetLocation, text, UiPoint { x, y });
            r = self.host.op_rect(Op::DrawClippedText, text, 0, rect);
            i += 1;
        }
        r
    }

    fn scrolling(&self) -> Option<&Scroll> {
        self.scroll.get()
    }

    /// One inward call; `f` points at four floats (or is NULL for the calls without).
    ///
    /// # Safety
    /// `f` is NULL or four live floats; a text argument is NUL-terminated bytes.
    pub unsafe fn call(&self, call: EeUiLabelCall, a: isize, b: isize, f: *mut f32) -> isize {
        type C = EeUiLabelCall;
        // SAFETY: the contract
        let buf = unsafe { read_buf(f) };
        let at = UiPoint { x: buf[0], y: buf[1] };
        let rect = crate::obj::buf_rect(&buf);
        // SAFETY: the contract (the bytes are copied before any call out)
        let bytes = || unsafe { CStr::from_ptr(a as *const c_char) }.to_bytes().to_vec();
        match call {
            C::Init => self.set_text(&bytes(), true),
            C::SetText => self.set_text(&bytes(), b != 0),
            C::ChangeFont => self.change_font(a),
            C::CopyData => return self.copy_data(),
            C::DeleteArray => self.delete_array(),
            C::DeleteLine => self.delete_line(a as c_ulong as usize),
            C::DrawPolygons => return self.draw_polygons(),
            C::DrawText => return self.draw_text(),
            C::MouseLButtonDown | C::MouseLButtonUp | C::MouseMove => return self.mouse(call, at),
            C::MouseWheelTurned => return self.mouse_wheel(a),
            C::RecomputeDisplayRange => self.recompute(),
            C::SetFitRectToText => {
                self.fit.set(a != 0);
                label_fit::compute_rect(&mut Fit(self));
            }
            C::SetLocation => self.set_location(at),
            C::SetRectangle => self.set_rectangle(rect),
            C::CheckScrollBar => self.check_scroll_bar(),
            C::FitRectToText => return self.fit.get() as isize,
            C::Text => return self.text.get(),
            C::LineCount => return self.lines.borrow().len() as c_ulong as isize,
            C::LinesDisplayed => return self.displayed.get() as isize,
            C::ScrollBar => return if self.removed.get() { 0 } else { self.scroll_bar.get() },
            C::ScrollBarExists => return (self.scroll_bar.get() != 0) as isize,
            C::Line => return self.line(a as c_ulong as usize).unwrap_or(0),
            C::TextRectangle => return self.text_rect.as_ptr() as isize,
            C::CopiedTextRectangle => return self.copied_text_rect.as_ptr() as isize,
            C::ScrollBarRectangle => return self.scroll_bar_rect.as_ptr() as isize,
            C::SetAlignment => self.alignment.set(a as c_ulong),
            C::SetStartingDisplayLine => {
                let bar = self.scroll_bar.get();
                if bar != 0 {
                    self.op(Op::BarSetValue, bar, a as c_ulong as isize);
                }
            }
            C::SetScrollBarBgOpacity => {
                let bar = self.scroll_bar.get();
                if bar != 0 {
                    self.op1(Op::PartSetBgOpacity, bar, buf[0]);
                }
                self.bg_opacity.set(buf[0]);
            }
            C::SetScrollBarBgColor => {
                let bar = self.scroll_bar.get();
                if bar != 0 {
                    let mut c = [buf[0], buf[1], buf[2], 0.0];
                    self.host.op_f(Op::PartSetBgColor, bar, 0, 0, 0, &mut c);
                }
                self.bg_color.set([buf[0], buf[1], buf[2]]);
            }
            C::SetTextRectangle => self.text_rect.set(rect),
            C::ScrollInit => self.scroll_init(a),
            C::PauseScrolling | C::ResumeScrolling => {
                if let Some(s) = self.scrolling() {
                    scrolling::pause_or_resume(&mut Scrolling(self, s), call == C::ResumeScrolling);
                }
            }
            C::ScrollText => {
                if let Some(s) = self.scrolling() {
                    let (start, end) = (buf[1].to_bits() as i32 as c_long, buf[2].to_bits() as i32 as c_long);
                    scrolling::scroll_text(&mut Scrolling(self, s), buf[0], a as c_long, b as c_long, start, end);
                }
            }
            C::ScrollTextFor => {
                if let Some(s) = self.scrolling() {
                    scrolling::scroll_text_for(&mut Scrolling(self, s), a as c_ulong, b as c_ulong);
                }
            }
            C::StopScrolling => {
                if let Some(s) = self.scrolling() {
                    scrolling::stop_scrolling(&mut Scrolling(self, s));
                }
            }
            C::PixelStartingOffset => {
                return self.scrolling().map_or(0, |s| s.pixel_starting_offset.get() as c_ulong as isize)
            }
            C::StartingLine => return self.scrolling().map_or(0, |s| s.starting_line.get() as isize),
            C::SetPixelStartingOffset => {
                if let Some(s) = self.scrolling() {
                    s.pixel_starting_offset.set(a as c_ulong as c_long);
                }
            }
            C::SetStartingLine => {
                if let Some(s) = self.scrolling() {
                    s.starting_line.set(a as c_ulong);
                }
            }
        }
        0
    }
}

/// The label as its geometry (`label_fit.rs`) sees it.
struct Fit<'a>(&'a EeUiLabel);

impl LabelHost for Fit<'_> {
    fn fit(&self) -> bool {
        self.0.fit.get()
    }
    fn label_type(&self) -> c_ulong {
        self.0.label_type
    }
    fn alignment(&self) -> c_ulong {
        self.0.alignment.get()
    }
    fn original(&self) -> UiRect {
        self.0.original.get()
    }
    fn rect(&self) -> UiRect {
        self.0.host.rect()
    }
    fn lines(&self) -> usize {
        self.0.lines.borrow().len()
    }
    fn displayed(&self) -> c_ulong {
        self.0.displayed.get()
    }
    fn measure(&mut self, i: usize) -> (f32, f32) {
        let text = self.0.text.get();
        // a line past the end (the scroll bar's window past the lines) measures nothing, where the C++ read past
        // its vector
        match self.0.line(i) {
            Some(s) => {
                self.0.op(Op::TextSetText, text, s);
                self.0.extent(text)
            }
            None => (0.0, 0.0),
        }
    }
    fn has_scroll_bar(&self) -> bool {
        self.0.scroll_bar.get() != 0
    }
    fn removed(&self) -> bool {
        self.0.removed.get()
    }
    fn set_removed(&mut self, removed: bool) {
        self.0.removed.set(removed);
    }
    fn scroll_value(&mut self) -> c_long {
        self.0.bar_value()
    }
    fn set_rectangle(&mut self, r: UiRect) {
        self.0.virt_set_rectangle(r);
    }
    fn set_dirty(&mut self) {
        self.0.op(Op::SetDirty, 1, 0);
    }
    fn set_bg_rectangle(&mut self, r: UiRect) {
        self.0.host.op_rect(Op::SetBgRectangle, 0, 0, r);
    }
    fn set_range(&mut self, max: c_ulong) {
        self.0.op(Op::BarSetRange, self.0.scroll_bar.get(), max as isize);
    }
    fn create_scroll_bar(&mut self, height: f32) {
        let s = self.0.scroll_bar_rect.get();
        let mut f = [s.left, s.top, height, 0.0];
        let bar = self.0.host.op_f(Op::CreateScrollBar, 0, 0, 0, 0, &mut f);
        self.0.scroll_bar.set(bar);
    }
    fn set_page_size(&mut self, page: c_ulong) {
        self.0.op(Op::BarSetPageSize, self.0.scroll_bar.get(), page as isize);
    }
    fn set_scroll_bar_colors(&mut self) {
        let bar = self.0.scroll_bar.get();
        let c = self.0.bg_color.get();
        let mut f = [c[0], c[1], c[2], 0.0];
        self.0.host.op_f(Op::PartSetBgColor, bar, 0, 0, 0, &mut f);
        self.0.op1(Op::PartSetBgOpacity, bar, self.0.bg_opacity.get());
    }
    fn scroll_bar_rect(&self) -> UiRect {
        self.0.scroll_bar_rect.get()
    }
    fn scroll_bar_control_rect(&self) -> UiRect {
        self.0.host.get_rect(Op::PartRect, self.0.scroll_bar.get())
    }
}

/// A scrolling label as its scroll (`scrolling.rs`) sees it.
struct Scrolling<'a>(&'a EeUiLabel, &'a Scroll);

impl Scrolling<'_> {
    fn poll_of(&self, p: ScrollPoll) -> isize {
        self.1.polls[p as usize]
    }
}

impl ScrollHost for Scrolling<'_> {
    fn font_height(&mut self) -> c_ulong {
        self.0.font_height(self.0.text.get()) as c_ulong
    }
    fn rect(&self) -> UiRect {
        self.0.host.rect()
    }
    fn border(&self) -> c_ulong {
        self.0.op(Op::BorderThickness, 0, 0) as c_ulong
    }
    fn text_rect(&self) -> UiRect {
        self.0.text_rect.get()
    }
    fn get(&self, f: ScrollField) -> c_ulong {
        let s = self.1;
        match f {
            ScrollField::StartingLine => s.starting_line.get(),
            ScrollField::DrawStartingLine => s.draw_starting_line.get(),
            ScrollField::EndOffset => s.end_offset.get(),
            ScrollField::DrawLineCount => s.draw_line_count.get(),
            ScrollField::PixelStartingOffset => s.pixel_starting_offset.get() as c_ulong,
            ScrollField::DrawPixelOffset => s.draw_pixel_offset.get(),
            ScrollField::Scrolling => s.scrolling.get() as c_ulong,
        }
    }
    fn set(&mut self, f: ScrollField, v: c_ulong) {
        let s = self.1;
        match f {
            ScrollField::StartingLine => s.starting_line.set(v),
            ScrollField::DrawStartingLine => s.draw_starting_line.set(v),
            ScrollField::EndOffset => s.end_offset.set(v),
            ScrollField::DrawLineCount => s.draw_line_count.set(v),
            ScrollField::PixelStartingOffset => s.pixel_starting_offset.set(v as c_long),
            ScrollField::DrawPixelOffset => s.draw_pixel_offset.set(v),
            ScrollField::Scrolling => s.scrolling.set(v != 0),
        }
    }
    fn poll(&mut self, p: ScrollPoll, c: PollCall, arg: c_ulong) -> c_ulong {
        let h = self.poll_of(p);
        let op = match c {
            PollCall::Running => Op::PollRunning,
            PollCall::Poll => Op::PollPoll,
            PollCall::Start => Op::PollStart,
            PollCall::Stop => Op::PollStop,
            PollCall::Pause => Op::PollPause,
            PollCall::Resume => Op::PollResume,
            PollCall::SetTimeDelay => Op::PollSetDelay,
        };
        let r = self.0.op(op, h, arg as isize);
        match c {
            PollCall::Running | PollCall::Poll => r as c_ulong,
            _ => 0,
        }
    }
    fn line_count(&self) -> usize {
        self.0.lines.borrow().len()
    }
    fn displayed(&self) -> c_ulong {
        self.0.displayed.get()
    }
    fn resize_draw_lines(&mut self, n: usize) {
        let len = self.1.draw_lines.borrow().len();
        if len < n {
            for _ in len..n {
                let s = self.0.string_new(b"");
                self.1.draw_lines.borrow_mut().push(s);
            }
        } else if len > n {
            let tail = self.1.draw_lines.borrow_mut().split_off(n);
            for s in tail {
                self.0.op(Op::StringDelete, s, 0);
            }
        }
    }
    fn copy_draw_rect(&mut self) {
        self.1.draw_rect.set(self.0.text_rect.get());
    }
    fn copy_line(&mut self, i: usize, line: usize) {
        let dst = self.1.draw_lines.borrow().get(i).copied();
        if let (Some(dst), Some(src)) = (dst, self.0.line(line)) {
            self.0.op(Op::StringAssign, dst, src);
        }
    }
    fn control_copy_data(&mut self) -> c_long {
        self.0.op(Op::BaseCopyData, 0, 0) as c_long
    }
    fn fire_finished(&mut self) {
        self.0.op(Op::FireTrigger, TRIGGER_SCROLLING_FINISHED as isize, 0);
    }
    fn set_dirty(&mut self) {
        self.0.op(Op::SetDirty, 1, 0);
    }
}

// ---- the C ABI

/// A new Rust label for the C++ `label` (its `UIControl` part constructed): `f` the rectangle given (a
/// `GERectangle`), `font` the text font. Its text is set by the first [`ee_ui_label_call`] (`Init`).
///
/// # Safety
/// `f` points at four live floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_label_new(
    label: *mut c_void,
    host: EeUiHostFn,
    f: *const f32,
    font: *mut c_void,
    label_type: c_ulong,
    border: c_ulong,
) -> *mut EeUiLabel {
    // SAFETY: the contract
    let r = crate::obj::buf_rect(&unsafe { read_buf(f) });
    Box::into_raw(Box::new(EeUiLabel::new(Host { obj: label, f: host }, r, font as isize, label_type, border)))
}

/// The destructor: deletes the lines, the scroll bar, the texts (and a scrolling label's polls) and frees the
/// object.
///
/// # Safety
/// `label` came from `ee_ui_label_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_label_free(label: *mut EeUiLabel) {
    if label.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*label).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(label) });
}

/// One of UITextLabel's or UITLScrolling's methods ([`EeUiLabelCall`]).
///
/// # Safety
/// `label` is live; `f` is NULL or four live floats; a text argument is NUL-terminated bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_label_call(
    label: *const EeUiLabel,
    call: EeUiLabelCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*label).call(call, a, b, f) }
}

#[cfg(test)]
#[path = "labelobj_tests.rs"]
mod tests;
