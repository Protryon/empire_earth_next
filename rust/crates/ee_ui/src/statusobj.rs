//! `UIStatusBar` as a whole Rust object (R6 ui round 10): the text area and its copy, the text colour, the two texts
//! (C++ `GEText`s held by address), the value and its range and whether a text was copied, with every method of
//! `UIStatusBar.cpp` and the header's.
//!
//! The C++ `UIStatusBar` stays the type the loading screen and the controls table hold: its methods are one-line
//! forwarders to [`ee_ui_status_bar_call`], its constructor and destructor make and free this object, and every call
//! out is an [`EeUiOp`] through its host. The bar's two rhombi (the bar and its copy for drawing) stay C++ members,
//! reached by the rhombus ops (0 the bar, 1 the copy).
//!
//! The arithmetic is round 7's (`status.rs`, pinned by its golden).

use core::cell::RefCell;
use core::ffi::{c_long, c_ulong, c_void};

use crate::obj::{read_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::UiRect;
use crate::status;

/// The inward calls (the C++ shim's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiStatusBarCall {
    /// `ChangeFont(a)`.
    ChangeFont = 1,
    /// `CopyData()` -> the base's result.
    CopyData = 2,
    /// `DrawPolygons()`.
    DrawPolygons = 3,
    /// `DrawTextA()`.
    DrawText = 4,
    /// `GetCurrentRangeValue()`.
    CurrentRangeValue = 5,
    /// `GetGEText()`.
    Text = 6,
    /// `GetMaxRange()`.
    MaxRange = 7,
    /// `GetMinRange()`.
    MinRange = 8,
    /// `GetRange()`: max - min + 1.
    Range = 9,
    /// `SetCurrentRangeValue(a)`.
    SetCurrentRangeValue = 10,
    /// `SetFocusedTextColor(f[0..3])`.
    SetFocusedTextColor = 11,
    /// `SetLocation(f[0], f[1])`.
    SetLocation = 12,
    /// `SetRange(a, b)`.
    SetRange = 13,
    /// `SetStatusBarOpacity(f[0])`.
    SetStatusBarOpacity = 14,
    /// `SetStatusBarColor(f[0..3])`.
    SetStatusBarColor = 15,
    /// `SetText(*(const UWideString*)a, font b)`.
    SetText = 16,
}

#[derive(Clone, Copy, Debug)]
struct State {
    text_rect: UiRect,
    copied_text_rect: UiRect,
    text_color: [f32; 3],
    text: isize,
    copied_text: isize,
    current: c_long,
    min: c_long,
    max: c_long,
    has_copied_text: bool,
}

/// The Rust UIStatusBar.
pub struct EeUiStatusBar {
    host: Host,
    s: RefCell<State>,
}

const BAR: isize = 0;
const COPY: isize = 1;

impl EeUiStatusBar {
    /// The constructor's part after `UIControl`'s: an empty range, no text, white text, the bar's rhombus half a pixel
    /// outside the control, opaque and black, and the text area inset by the border.
    pub fn new(host: Host, border: c_ulong) -> Self {
        let rect = host.rect();
        let bar = UiRect::new(rect.left - 0.5, rect.top - 0.5, rect.right + 0.5, rect.bottom + 0.5);
        host.op_rect(Op::RhombusSetRect, BAR, 0, bar);
        host.op_f(Op::RhombusSetOpacity, BAR, 0, 0, 0, &mut [1.0, 0.0, 0.0, 0.0]);
        host.op_f(Op::RhombusSetColor, BAR, 0, 0, 0, &mut [0.0; 4]);
        EeUiStatusBar {
            host,
            s: RefCell::new(State {
                text_rect: status::inset(rect, border),
                copied_text_rect: UiRect::default(),
                text_color: [1.0, 1.0, 1.0],
                text: 0,
                copied_text: 0,
                current: 0,
                min: 0,
                max: 0,
                has_copied_text: false,
            }),
        }
    }

    fn st(&self) -> State {
        *self.s.borrow()
    }

    /// The destructor: the two texts, when made.
    pub fn release(&self) {
        let s = self.st();
        if s.text != 0 {
            self.host.op(Op::TextDelete, s.text, 0, 0, 0);
            self.host.op(Op::TextDelete, s.copied_text, 0, 0, 0);
        }
    }

    pub fn copy_data(&self) -> isize {
        let h = &self.host;
        let border = h.op(Op::BorderThickness, 0, 0, 0, 0) as c_ulong;
        let r = h.op(Op::BaseCopyData, 0, 0, 0, 0);
        h.op(Op::RhombusCopy, 0, 0, 0, 0);
        if let Some(fill) = status::copy_fill(h.get_rect(Op::RhombusRect, COPY), border) {
            h.op_rect(Op::RhombusSetRect, COPY, 0, fill);
        }
        let s = self.st();
        if s.text != 0 && h.op(Op::TextLength, s.text, 0, 0, 0) > 0 {
            let e = h.get_point(Op::TextExtent, s.text);
            let t = status::copy_text(s.text_rect, (e.x, e.y));
            self.s.borrow_mut().copied_text_rect = t;
            h.op(Op::TextCopy, s.copied_text, s.text, 0, 0);
            self.s.borrow_mut().has_copied_text = true;
        } else {
            self.s.borrow_mut().has_copied_text = false;
        }
        r
    }

    pub fn draw_polygons(&self) -> isize {
        let mut r = self.host.op(Op::BaseDrawPolygons, 0, 0, 0, 0);
        if self.host.get_rect(Op::RhombusRect, COPY).width() > 0.0 {
            r = self.host.op(Op::RhombusDraw, COPY, 0, 0, 0);
        }
        r
    }

    pub fn draw_text(&self) -> isize {
        let mut r = self.host.op(Op::BaseDrawText, 0, 0, 0, 0);
        let s = self.st();
        if s.has_copied_text {
            self.host.op_point(Op::TextSetLocation, s.copied_text, s.copied_text_rect.top_left());
            r = self.host.op_rect(Op::DrawClippedText, s.copied_text, 0, s.copied_text_rect);
        }
        r
    }

    pub fn set_current_range_value(&self, value: c_long) {
        let s = self.st();
        let width = self.host.op(Op::Width, 0, 0, 0, 0) as c_ulong;
        let (value, fill) = status::fill(value, s.min, s.max, width, self.host.get_rect(Op::RhombusRect, BAR));
        self.host.op_rect(Op::RhombusSetRect, BAR, 0, fill);
        if self.st().current != value {
            self.s.borrow_mut().current = value;
            self.host.op(Op::SetDirty, 1, 0, 0, 0);
        }
    }

    pub fn set_location(&self, x: f32, y: f32) {
        let h = &self.host;
        h.op_f(Op::BaseSetLocation, 0, 0, 0, 0, &mut [x, y, 0.0, 0.0]);
        let border = h.op(Op::BorderThickness, 0, 0, 0, 0) as c_ulong;
        let (fill, text) = status::location(h.rect(), border);
        h.op_rect(Op::RhombusSetRect, BAR, 0, fill);
        self.set_current_range_value(self.st().current);
        self.s.borrow_mut().text_rect = text;
    }

    /// One inward call; `f` points at four floats (or is NULL for the calls without).
    ///
    /// # Safety
    /// `f` is NULL or four live floats; for `SetText`, `a` is a live `UWideString*` the host reads.
    pub unsafe fn call(&self, call: EeUiStatusBarCall, a: isize, b: isize, f: *mut f32) -> isize {
        type C = EeUiStatusBarCall;
        // SAFETY: the contract
        let buf = unsafe { read_buf(f) };
        let h = &self.host;
        match call {
            C::ChangeFont => {
                let s = self.st();
                if s.text != 0 {
                    h.op(Op::TextSetFont, s.text, a, 0, 0);
                    h.op(Op::TextSetFont, s.copied_text, a, 0, 0);
                }
            }
            C::CopyData => return self.copy_data(),
            C::DrawPolygons => return self.draw_polygons(),
            C::DrawText => return self.draw_text(),
            C::CurrentRangeValue => return self.st().current as isize,
            C::Text => return self.st().text,
            C::MaxRange => return self.st().max as isize,
            C::MinRange => return self.st().min as isize,
            C::Range => {
                let s = self.st();
                return (s.max.wrapping_sub(s.min) as c_ulong).wrapping_add(1) as isize;
            }
            C::SetCurrentRangeValue => self.set_current_range_value(a as c_long),
            C::SetFocusedTextColor => self.s.borrow_mut().text_color = [buf[0], buf[1], buf[2]],
            C::SetLocation => self.set_location(buf[0], buf[1]),
            C::SetRange => {
                {
                    let mut s = self.s.borrow_mut();
                    s.min = a as c_ulong as c_long;
                    s.max = b as c_ulong as c_long;
                }
                self.set_current_range_value(a as c_ulong as c_long);
            }
            C::SetStatusBarOpacity => {
                h.op_f(Op::RhombusSetOpacity, BAR, 0, 0, 0, &mut [buf[0], 0.0, 0.0, 0.0]);
            }
            C::SetStatusBarColor => {
                h.op_f(Op::RhombusSetColor, BAR, 0, 0, 0, &mut [buf[0], buf[1], buf[2], 0.0]);
            }
            C::SetText => {
                if self.st().text == 0 {
                    let t = h.op(Op::TextCreate, b, 0, 0, 0);
                    self.s.borrow_mut().text = t;
                    let c = h.op(Op::TextCreate, b, 0, 0, 0);
                    self.s.borrow_mut().copied_text = c;
                }
                h.op(Op::TextSetText, self.st().text, a, 0, 0);
                h.op(Op::SetDirty, 1, 0, 0, 0);
            }
        }
        0
    }
}

// ---- the C ABI

/// A new Rust status bar for the C++ `bar` (its `UIControl` part constructed with `border`).
#[no_mangle]
pub extern "C" fn ee_ui_status_bar_new(bar: *mut c_void, host: EeUiHostFn, border: c_ulong) -> *mut EeUiStatusBar {
    let obj = Box::into_raw(Box::new(EeUiStatusBar::new(Host { obj: bar, f: host }, border)));
    crate::direct::made(crate::direct::Kind::StatusBar, bar, obj)
}

/// The destructor: deletes the texts and frees the object.
///
/// # Safety
/// `bar` came from `ee_ui_status_bar_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_status_bar_free(bar: *mut EeUiStatusBar) {
    if bar.is_null() {
        return;
    }
    crate::direct::forget(crate::direct::Kind::StatusBar, bar as *const c_void);
    // SAFETY: the contract
    unsafe { (*bar).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(bar) });
}

/// One of UIStatusBar's methods ([`EeUiStatusBarCall`]).
///
/// # Safety
/// `bar` is live; `f` is NULL or four live floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_status_bar_call(
    bar: *const EeUiStatusBar,
    call: EeUiStatusBarCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*bar).call(call, a, b, f) }
}

#[cfg(test)]
#[path = "statusobj_tests.rs"]
mod tests;
