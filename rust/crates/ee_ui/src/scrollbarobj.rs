//! `UIScrollBar` as a whole Rust object (R6 ui round 10): its three parts (the slider and the two arrow buttons, C++
//! objects held by address) and which of them the mouse went down on, with every method of `UIScrollBar.cpp` and
//! the header's (the forwards to the slider included).
//!
//! The C++ `UIScrollBar` stays the type the edit boxes, list boxes and text labels hold: its methods are one-line
//! forwarders to [`ee_ui_scroll_bar_call`], its constructor and destructor make and free this object, and every call
//! out is an [`EeUiOp`] through its host: `UIControl`'s members, the parts' virtuals (`new` and `delete` of the parts
//! included), the manager's arrow bitmaps.
//!
//! The routing and layout are round 3's (`scrollbar.rs`, pinned by its golden); this object is its [`ScrollBarHost`].

use core::cell::RefCell;
use core::ffi::{c_long, c_ulong, c_void};

use crate::obj::{read_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::{UiPoint, UiRect};
use crate::scrollbar::{self, ScrollBarFields, ScrollBarHost, ScrollPart};

/// The inward calls (the C++ shim's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiScrollBarCall {
    /// `CopyData()` -> the slider's result.
    CopyData = 1,
    /// `DrawPolygons()` -> the decrement button's result.
    DrawPolygons = 2,
    /// `DrawTextA()` -> the base's result.
    DrawText = 3,
    /// `IncrementBy(a)`.
    IncrementBy = 4,
    /// `MouseLButtonDown()`.
    MouseLButtonDown = 5,
    /// `MouseLButtonUp()`.
    MouseLButtonUp = 6,
    /// `MouseMove(f[0], f[1])`.
    MouseMove = 7,
    /// `SetLocation(f[0], f[1])`.
    SetLocation = 8,
    /// `DecrementByPageSize()` (the slider's).
    DecrementByPageSize = 9,
    /// `IncrementByPageSize()` (the slider's).
    IncrementByPageSize = 10,
    /// `SetCurrentRangeValue(a)` (the slider's).
    SetCurrentRangeValue = 11,
    /// `SetBgColor(f[0..3])` (the slider's).
    SetBgColor = 12,
    /// `SetBgOpacity(f[0])` (the slider's).
    SetBgOpacity = 13,
    /// `SetPageSize(a)` (the slider's).
    SetPageSize = 14,
    /// `SetRange(a, b)` (the slider's).
    SetRange = 15,
    /// `GetDecrementButton()`.
    DecrementButton = 16,
    /// `GetIncrementButton()`.
    IncrementButton = 17,
    /// `GetSlider()`.
    Slider = 18,
}

/// The Rust UIScrollBar.
pub struct EeUiScrollBar {
    host: Host,
    f: RefCell<ScrollBarFields>,
}

/// The bar as round 3's routing sees it.
struct H<'a>(&'a EeUiScrollBar);

impl H<'_> {
    fn part(&self, part: ScrollPart) -> isize {
        let f = self.0.f.borrow();
        (match part {
            ScrollPart::Increment => f.increment_button,
            ScrollPart::Decrement => f.decrement_button,
            ScrollPart::Slider => f.slider,
        }) as isize
    }
    fn on(&self, op: Op, part: ScrollPart) -> c_long {
        self.0.host.op(op, self.part(part), 0, 0, 0) as c_long
    }
}

impl ScrollBarHost for H<'_> {
    fn load(&self) -> ScrollBarFields {
        *self.0.f.borrow()
    }
    fn store(&mut self, f: &ScrollBarFields) {
        *self.0.f.borrow_mut() = *f;
    }
    fn rectangle(&self) -> UiRect {
        self.0.host.rect()
    }
    fn mouse_location(&self) -> UiPoint {
        self.0.host.mouse()
    }
    fn part_rect(&self, part: ScrollPart) -> UiRect {
        self.0.host.get_rect(Op::PartRect, self.part(part))
    }
    fn part_mouse_down(&mut self, part: ScrollPart) -> c_long {
        self.on(Op::PartMouseDown, part)
    }
    fn part_mouse_up(&mut self, part: ScrollPart) -> c_long {
        self.on(Op::PartMouseUp, part)
    }
    fn slider_mouse_move(&mut self, at: UiPoint) -> c_long {
        self.0.host.op_point(Op::PartMouseMove, self.part(ScrollPart::Slider), at) as c_long
    }
    fn button_down(&mut self, part: ScrollPart) {
        self.on(Op::PartButtonDown, part);
    }
    fn capture_mouse(&mut self) {
        self.0.host.op(Op::CaptureMouse, 0, 0, 0, 0);
    }
    fn release_mouse(&mut self) {
        self.0.host.op(Op::ReleaseMouse, 0, 0, 0, 0);
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.0.host.op_point(Op::BaseSetLocation, 0, at);
    }
    fn slider_style(&self) -> c_ulong {
        self.on(Op::SliderStyle, ScrollPart::Slider) as c_ulong
    }
    fn part_set_location(&mut self, part: ScrollPart, at: UiPoint) {
        self.0.host.op_point(Op::PartSetLocation, self.part(part), at);
    }
    fn slider_set_rectangle(&mut self, rect: UiRect) {
        self.0.host.op_rect(Op::PartSetRectangle, self.part(ScrollPart::Slider), 0, rect);
    }
    fn slider_value(&self) -> c_long {
        self.on(Op::SliderValue, ScrollPart::Slider)
    }
    fn set_slider_value(&mut self, value: c_long) {
        self.0.host.op(Op::SliderSetValue, self.part(ScrollPart::Slider), value as isize, 0, 0);
    }
}

/// The manager's arrow bitmaps by style: (increment normal, decrement normal, increment pressed, decrement pressed)
/// as [`Op::ManagerBitmap`] kinds.
fn arrow_bitmaps(style: c_ulong) -> [isize; 4] {
    if style == 1 {
        [0, 1, 2, 3]
    } else {
        [4, 5, 6, 7]
    }
}

impl EeUiScrollBar {
    /// The constructor's part after `UIControl`'s (made with [`scrollbar::layout`]'s rectangle and border 0): the
    /// arrow bitmaps for the style, then the slider, the decrement button at `at` and the increment button.
    pub fn new(host: Host, at: UiPoint, length: f32, style: c_ulong, border: c_ulong) -> Self {
        let layout = scrollbar::layout(at, length, style);
        let b = arrow_bitmaps(style).map(|k| host.op(Op::ManagerBitmap, k, 0, 0, 0));
        let slider = host.op_rect(Op::CreateSlider, style as isize, border as isize, layout.slider);
        let mut f = [at.x, at.y, 0.0, 0.0];
        let decrement = host.op_f(Op::CreateButton, b[1], b[3], slider, 1, &mut f);
        let mut f = [layout.increment.x, layout.increment.y, 0.0, 0.0];
        let increment = host.op_f(Op::CreateButton, b[0], b[2], slider, 0, &mut f);
        EeUiScrollBar {
            host,
            f: RefCell::new(ScrollBarFields {
                increment_button: increment as *mut c_void,
                decrement_button: decrement as *mut c_void,
                slider: slider as *mut c_void,
                increment_pressed: false,
                decrement_pressed: false,
                slider_pressed: false,
            }),
        }
    }

    fn parts(&self) -> (isize, isize, isize) {
        let f = self.f.borrow();
        (f.increment_button as isize, f.decrement_button as isize, f.slider as isize)
    }

    /// The destructor: the slider, then the increment and decrement buttons.
    pub fn release(&self) {
        let (inc, dec, slider) = self.parts();
        for p in [slider, inc, dec] {
            self.host.op(Op::DeletePart, p, 0, 0, 0);
        }
    }

    /// One inward call; `f` points at four floats (or is NULL for the calls without).
    ///
    /// # Safety
    /// `f` is NULL or four live floats.
    pub unsafe fn call(&self, call: EeUiScrollBarCall, a: isize, b: isize, f: *mut f32) -> isize {
        type C = EeUiScrollBarCall;
        // SAFETY: the contract
        let buf = unsafe { read_buf(f) };
        let at = UiPoint { x: buf[0], y: buf[1] };
        let (inc, dec, slider) = self.parts();
        let h = &self.host;
        match call {
            C::CopyData => {
                h.op(Op::BaseCopyData, 0, 0, 0, 0);
                h.op(Op::PartCopyData, inc, 0, 0, 0);
                h.op(Op::PartCopyData, dec, 0, 0, 0);
                return h.op(Op::PartCopyData, slider, 0, 0, 0);
            }
            C::DrawPolygons => {
                h.op(Op::BaseDrawPolygons, 0, 0, 0, 0);
                h.op(Op::PartDrawPolygons, slider, 0, 0, 0);
                h.op(Op::PartDrawPolygons, inc, 0, 0, 0);
                return h.op(Op::PartDrawPolygons, dec, 0, 0, 0);
            }
            C::DrawText => {
                h.op(Op::PartDrawText, dec, 0, 0, 0);
                h.op(Op::PartDrawText, inc, 0, 0, 0);
                h.op(Op::PartDrawText, slider, 0, 0, 0);
                return h.op(Op::BaseDrawText, 0, 0, 0, 0);
            }
            C::IncrementBy => scrollbar::increment_by(&mut H(self), a as c_long),
            C::MouseLButtonDown => return scrollbar::mouse_l_button_down(&mut H(self)) as isize,
            C::MouseLButtonUp => return scrollbar::mouse_l_button_up(&mut H(self)) as isize,
            C::MouseMove => return scrollbar::mouse_move(&mut H(self), at) as isize,
            C::SetLocation => scrollbar::set_location(&mut H(self), at),
            C::DecrementByPageSize => {
                h.op(Op::SliderDecrementPage, slider, 0, 0, 0);
            }
            C::IncrementByPageSize => {
                h.op(Op::SliderIncrementPage, slider, 0, 0, 0);
            }
            // the header's `unsigned long` value, the slider's `long` parameter
            C::SetCurrentRangeValue => {
                h.op(Op::SliderSetValue, slider, a as c_ulong as c_long as isize, 0, 0);
            }
            C::SetBgColor => {
                let mut c = [buf[0], buf[1], buf[2], 0.0];
                h.op_f(Op::PartSetBgColor, slider, 0, 0, 0, &mut c);
            }
            C::SetBgOpacity => {
                let mut c = [buf[0], 0.0, 0.0, 0.0];
                h.op_f(Op::PartSetBgOpacity, slider, 0, 0, 0, &mut c);
            }
            C::SetPageSize => {
                h.op(Op::SliderSetPageSize, slider, a, 0, 0);
            }
            C::SetRange => {
                h.op(Op::SliderSetRange, slider, a, b, 0);
            }
            C::DecrementButton => return dec,
            C::IncrementButton => return inc,
            C::Slider => return slider,
        }
        0
    }
}

// ---- the C ABI

/// The constructor's geometry ([`scrollbar::layout`]): the C++ needs the rectangle for `UIControl`'s constructor,
/// before this object is made.
#[no_mangle]
pub extern "C" fn ee_ui_scroll_bar_layout(at: UiPoint, length: f32, style: c_ulong) -> scrollbar::ScrollBarLayout {
    scrollbar::layout(at, length, style)
}

/// A new Rust scroll bar for the C++ `bar` (its `UIControl` part constructed): `f` the location and length.
///
/// # Safety
/// `f` points at four live floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scroll_bar_new(
    bar: *mut c_void,
    host: EeUiHostFn,
    f: *const f32,
    style: c_ulong,
    border: c_ulong,
) -> *mut EeUiScrollBar {
    // SAFETY: the contract
    let b = unsafe { read_buf(f) };
    let at = UiPoint { x: b[0], y: b[1] };
    Box::into_raw(Box::new(EeUiScrollBar::new(Host { obj: bar, f: host }, at, b[2], style, border)))
}

/// The destructor: deletes the parts and frees the object.
///
/// # Safety
/// `bar` came from `ee_ui_scroll_bar_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scroll_bar_free(bar: *mut EeUiScrollBar) {
    if bar.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*bar).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(bar) });
}

/// One of UIScrollBar's methods ([`EeUiScrollBarCall`]).
///
/// # Safety
/// `bar` is live; `f` is NULL or four live floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scroll_bar_call(
    bar: *const EeUiScrollBar,
    call: EeUiScrollBarCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*bar).call(call, a, b, f) }
}

#[cfg(test)]
#[path = "scrollbarobj_tests.rs"]
mod tests;
