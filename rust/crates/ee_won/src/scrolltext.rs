//! The credits' scrolling text (R6 WONLobby round 3): `ScrollingTextComponent`, a `Component` whose lines scroll up
//! one pixel per scroll period and fade in and out in the top and bottom thirds. This object holds the lines (each a
//! C++ `GUIString` copy made by the host, with its colour), the last colour, the scroll clock and position and the
//! text's height, and runs AddLine, TimerEvent, Paint, Start, Stop and Reset. The C++ class keeps its vtable (Paint and
//! TimerEvent forward here) and its constructor's clip flag; every call on the component itself or on the paint's
//! `Graphics` is one op of the host (`ScrollTextHost` in `WONLobby/CreditsDialog.cpp`), the original's statements
//! with no decision between them.
//!
//! The original's Paint read the component's `Height()` and `Width()` where each expression named them; the ops are
//! made in the same places and left to right, so the scripted golden sees every read.

use std::cell::{Cell, RefCell};
use std::ffi::c_void;

/// The ops (`a` the `Graphics*` of a paint or a `GUIString*`; `b` a `GUIString*`; `x`, `y`, `w`, `h` as named).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonScrollTextOp {
    /// `Width()` of the component.
    Width = 1,
    /// `Height()` of the component.
    Height,
    /// `Invalidate()`.
    Invalidate,
    /// `RequestTimer(x != 0)`.
    RequestTimer,
    /// `new GUIString(*(const GUIString*)a)`: a line's copy (freed by `LineFree`).
    LineNew,
    /// `delete (GUIString*)a`.
    LineFree,
    /// `Component::Paint(*(Graphics*)a)`.
    BasePaint,
    /// `g.SetFont(GetDefaultFont())`.
    SetDefaultFont,
    /// `g.SetColor((DWORD)x)`.
    SetColor,
    /// `g.SetSysColor(StandardColor_3DHilight)`.
    SysHilight,
    /// `g.SetSysColor(StandardColor_3DShadow)`.
    SysShadow,
    /// `g.FillRect(x, y, w, h)`.
    FillRect,
    /// `g.DrawRect(x, y, w, h)`.
    DrawRect,
    /// `g.mFont->GetHeight()`.
    FontHeight,
    /// `g.mColor`.
    Color,
    /// `g.mFont->GetStringWidth(*(const GUIString*)b)`.
    StringWidth,
    /// `g.DrawString(*(const GUIString*)b, x, y)`.
    DrawString,
}

/// The inward calls.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonScrollTextCall {
    /// `AddLine(*(const GUIString*)a, (unsigned long)b)`.
    AddLine = 1,
    /// `TimerEvent(a)`: 1 (the original always answered true).
    Timer,
    /// `Paint(*(Graphics*)a)`.
    Paint,
    /// `Start()`.
    Start,
    /// `Stop()`.
    Stop,
    /// `Reset()`.
    Reset,
    /// a member: 0 the scroll top, 1 the text's height, 2 the time since the last scroll, 3 the last colour, 4 the
    /// number of lines, 5 the scroll period.
    Field,
}

/// The host: `(component, op, a, b, x, y, w, h) -> result`.
pub type EeWonScrollTextHostFn = Option<
    unsafe extern "C" fn(
        comp: *mut c_void,
        op: EeWonScrollTextOp,
        a: isize,
        b: isize,
        x: i32,
        y: i32,
        w: i32,
        h: i32,
    ) -> isize,
>;

use EeWonScrollTextOp as Op;

/// White: the box's background, and the colour a line is not drawn in.
const WHITE: u32 = 0xff_ffff;

/// `DimColor`: a colour faded towards white (0 percent white, 100 the colour itself).
pub fn dim_color(color: u32, percent: i32) -> u32 {
    if percent <= 0 {
        return WHITE;
    }
    if percent >= 100 {
        return color;
    }
    let fade = |c: i32| 255 - (255 - c) * percent / 100;
    let red = fade(((color >> 16) & 255) as i32);
    let green = fade(((color >> 8) & 255) as i32);
    let blue = fade((color & 255) as i32);
    ((red << 16) | (green << 8) | blue) as u32
}

/// One scrolling text's Rust half.
pub struct EeWonScrollText {
    comp: *mut c_void,
    host: EeWonScrollTextHostFn,
    /// `mLines`: each line's `GUIString` copy and colour, in order.
    lines: RefCell<Vec<(isize, u32)>>,
    last_color: Cell<u32>,
    time_since_last_scroll: Cell<i32>,
    scroll_speed: Cell<i32>,
    scroll_top: Cell<i32>,
    total_text_height: Cell<i32>,
}

impl EeWonScrollText {
    /// The constructor's members: no line, colour 0, one pixel every 60 ms, at the top.
    pub fn new(comp: *mut c_void, host: EeWonScrollTextHostFn) -> Self {
        Self {
            comp,
            host,
            lines: RefCell::new(Vec::new()),
            last_color: Cell::new(0),
            time_since_last_scroll: Cell::new(0),
            scroll_speed: Cell::new(60),
            scroll_top: Cell::new(0),
            total_text_height: Cell::new(0),
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn op(&self, op: Op, a: isize, b: isize, x: i32, y: i32, w: i32, h: i32) -> isize {
        match self.host {
            // SAFETY: the host is the C++ component's (or a test's), called with the component it was given for
            Some(f) => unsafe { f(self.comp, op, a, b, x, y, w, h) },
            None => 0,
        }
    }

    fn me(&self, op: Op) -> i32 {
        self.op(op, 0, 0, 0, 0, 0, 0) as i32
    }

    fn g(&self, op: Op, g: isize, x: i32) -> isize {
        self.op(op, g, 0, x, 0, 0, 0)
    }

    fn rect(&self, op: Op, g: isize, x: i32, y: i32, w: i32, h: i32) {
        self.op(op, g, 0, x, y, w, h);
    }

    fn add_line(&self, text: isize, color: u32) {
        if color != u32::MAX {
            self.last_color.set(color);
        }
        let line = self.op(Op::LineNew, text, 0, 0, 0, 0, 0);
        self.lines.borrow_mut().push((line, self.last_color.get()));
    }

    fn timer(&self, delta: i32) {
        let time = self.time_since_last_scroll.get().wrapping_add(delta);
        self.time_since_last_scroll.set(time);
        if time < self.scroll_speed.get() {
            return;
        }
        let top = self.scroll_top.get().wrapping_sub(1);
        self.scroll_top.set(top);
        if top < self.total_text_height.get().wrapping_neg() {
            self.scroll_top.set(self.me(Op::Height));
        }
        self.me(Op::Invalidate);
        self.time_since_last_scroll.set(0);
    }

    fn reset(&self) {
        self.scroll_top.set(self.me(Op::Height));
    }

    fn paint(&self, g: isize) {
        self.g(Op::BasePaint, g, 0);
        self.g(Op::SetDefaultFont, g, 0);
        self.g(Op::SetColor, g, WHITE as i32);
        let w = self.me(Op::Width).wrapping_sub(4);
        let h = self.me(Op::Height).wrapping_sub(4);
        self.rect(Op::FillRect, g, 2, 2, w, h);
        self.g(Op::SysHilight, g, 0);
        let (w, h) = (self.me(Op::Width), self.me(Op::Height));
        self.rect(Op::DrawRect, g, 0, 0, w, h);
        let w = self.me(Op::Width).wrapping_sub(1);
        let h = self.me(Op::Height).wrapping_sub(1);
        self.rect(Op::DrawRect, g, 1, 1, w, h);
        self.g(Op::SysShadow, g, 0);
        let w = self.me(Op::Width).wrapping_sub(1);
        let h = self.me(Op::Height).wrapping_sub(1);
        self.rect(Op::DrawRect, g, 0, 0, w, h);
        let mut top = self.scroll_top.get();
        // a copy: no borrow across the ops
        let lines = self.lines.borrow().clone();
        for (line, color) in lines {
            let height = self.g(Op::FontHeight, g, 0) as i32;
            // a height under 3 divides by zero, as the original did (it faulted; this panics)
            let shade = if top < self.me(Op::Height) / 3 {
                dim_color(color, top.wrapping_mul(100) / (self.me(Op::Height) / 3))
            } else if self.me(Op::Height).wrapping_sub(top) <= height {
                WHITE
            } else if top > self.me(Op::Height).wrapping_mul(2) / 3 - height {
                let below = self.me(Op::Height).wrapping_sub(top.wrapping_add(height));
                dim_color(color, below.wrapping_mul(100) / (self.me(Op::Height) / 3))
            } else {
                color
            };
            self.g(Op::SetColor, g, shade as i32);
            if self.g(Op::Color, g, 0) as u32 != WHITE
                && top > 2
                && top < self.me(Op::Height).wrapping_sub(height).wrapping_sub(2)
            {
                let width = self.op(Op::StringWidth, g, line, 0, 0, 0, 0) as i32;
                let x = self.me(Op::Width).wrapping_sub(width) / 2;
                self.op(Op::DrawString, g, line, x, top, 0, 0);
            }
            top = top.wrapping_add(height);
        }
        self.total_text_height.set(top.wrapping_sub(self.scroll_top.get()));
    }

    /// One inward call.
    pub fn call(&self, call: EeWonScrollTextCall, a: isize, b: isize) -> isize {
        use EeWonScrollTextCall as C;
        match call {
            C::AddLine => self.add_line(a, b as u32),
            C::Timer => {
                self.timer(a as i32);
                return 1;
            }
            C::Paint => self.paint(a),
            C::Start => {
                self.op(Op::RequestTimer, 0, 0, 1, 0, 0, 0);
                self.reset();
            }
            C::Stop => {
                self.op(Op::RequestTimer, 0, 0, 0, 0, 0, 0);
            }
            C::Reset => self.reset(),
            C::Field => {
                return match a {
                    0 => self.scroll_top.get() as isize,
                    1 => self.total_text_height.get() as isize,
                    2 => self.time_since_last_scroll.get() as isize,
                    3 => self.last_color.get() as isize,
                    4 => self.lines.borrow().len() as isize,
                    5 => self.scroll_speed.get() as isize,
                    _ => 0,
                }
            }
        }
        0
    }

    /// The destructor: the lines freed front to back (the list's destruction).
    fn free_lines(&self) {
        let lines = std::mem::take(&mut *self.lines.borrow_mut());
        for (line, _) in lines {
            self.op(Op::LineFree, line, 0, 0, 0, 0, 0);
        }
    }
}

/// A new scrolling text for the C++ component `comp`.
#[no_mangle]
pub extern "C" fn ee_won_scrolltext_new(comp: *mut c_void, host: EeWonScrollTextHostFn) -> *mut EeWonScrollText {
    Box::into_raw(Box::new(EeWonScrollText::new(comp, host)))
}

/// Frees the lines (front to back) and the object.
///
/// # Safety
/// `st` came from `ee_won_scrolltext_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_scrolltext_free(st: *mut EeWonScrollText) {
    if !st.is_null() {
        // SAFETY: the contract
        unsafe { (*st).free_lines() };
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(st) });
    }
}

/// One inward call (`EeWonScrollTextCall`).
///
/// # Safety
/// `st` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_scrolltext_call(
    st: *const EeWonScrollText,
    call: EeWonScrollTextCall,
    a: isize,
    b: isize,
) -> isize {
    // SAFETY: live (the contract)
    unsafe { (*st).call(call, a, b) }
}

#[cfg(test)]
#[path = "scrolltext_tests.rs"]
mod tests;
