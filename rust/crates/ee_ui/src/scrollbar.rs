//! `UIScrollBar` (`User Interface/UIScrollBar.cpp`): a slider between two arrow buttons, the decrement button at the
//! bar's top or left and the increment button at its bottom or right. The bar routes the mouse to the part under it
//! and lays the parts out for its style (1 vertical, 0 horizontal).
//!
//! The C++ object keeps its members ([`ScrollBarFields`] is its record from `mIncrementButton` on) and the parts;
//! the parts' handlers, the capture and `UIControl::SetLocation` are host calls ([`ScrollBarHost`]). The record is
//! written before and read after every host call with side effects.

use core::ffi::{c_long, c_ulong, c_void};

use crate::rect::{UiPoint, UiRect};

/// The arrow buttons' size (`kScrollButtonSize`).
const BUTTON: f32 = 20.0;

/// `UIScrollBar`'s members in their C++ order (from `mIncrementButton` on). The pointers are C++'s.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ScrollBarFields {
    pub increment_button: *mut c_void,
    pub decrement_button: *mut c_void,
    pub slider: *mut c_void,
    pub increment_pressed: bool,
    pub decrement_pressed: bool,
    pub slider_pressed: bool,
}

/// A part of the bar.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(C)]
pub enum ScrollPart {
    Increment = 0,
    Decrement = 1,
    Slider = 2,
}

/// The bar's way back into its C++ object.
pub trait ScrollBarHost {
    /// Reads the record.
    fn load(&self) -> ScrollBarFields;
    /// Writes the record.
    fn store(&mut self, f: &ScrollBarFields);
    /// `mRectangle`.
    fn rectangle(&self) -> UiRect;
    /// `GetMouseLocation()`.
    fn mouse_location(&self) -> UiPoint;
    /// A part's `GetRectangle()`.
    fn part_rect(&self, part: ScrollPart) -> UiRect;
    /// A part's virtual `MouseLButtonDown()`.
    fn part_mouse_down(&mut self, part: ScrollPart) -> c_long;
    /// A part's virtual `MouseLButtonUp()`.
    fn part_mouse_up(&mut self, part: ScrollPart) -> c_long;
    /// The slider's virtual `MouseMove(at)`.
    fn slider_mouse_move(&mut self, at: UiPoint) -> c_long;
    /// An arrow button's virtual `ButtonDown()` (its step and repeat).
    fn button_down(&mut self, part: ScrollPart);
    /// The virtual `CaptureMouse()`.
    fn capture_mouse(&mut self);
    /// The virtual `ReleaseCapturedMouse()`.
    fn release_mouse(&mut self);
    /// `UIControl::SetLocation(at)`.
    fn base_set_location(&mut self, at: UiPoint);
    /// The slider's `GetSliderStyle()`.
    fn slider_style(&self) -> c_ulong;
    /// An arrow button's virtual `SetLocation(at)`.
    fn part_set_location(&mut self, part: ScrollPart, at: UiPoint);
    /// The slider's virtual `SetRectangle(rect)`.
    fn slider_set_rectangle(&mut self, rect: UiRect);
    /// The slider's `GetCurrentRangeValue()`.
    fn slider_value(&self) -> c_long;
    /// The slider's `SetCurrentRangeValue(value)`.
    fn set_slider_value(&mut self, value: c_long);
}

/// The constructor's geometry for a bar at `at`, `length` long: the control's rectangle, the slider's and the
/// increment button's location (the decrement button sits at `at`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ScrollBarLayout {
    pub rect: UiRect,
    pub slider: UiRect,
    pub increment: UiPoint,
}

/// `GERectangle(topLeft, width, height)`: the right and bottom edges are inclusive.
fn sized(at: UiPoint, width: f32, height: f32) -> UiRect {
    let left = at.x;
    let top = at.y;
    UiRect::new(left, top, left + (width - 1.0), top + (height - 1.0))
}

/// The constructor's layout. The original's precedence slip made both styles 19 x (length - 1); the horizontal bar is
/// (length - 1) x 19 since the C++ fix, kept here.
pub fn layout(at: UiPoint, length: f32, style: c_ulong) -> ScrollBarLayout {
    let vertical = style == 1;
    let rect = sized(at, if vertical { 19.0 } else { length - 1.0 }, if vertical { length - 1.0 } else { 19.0 });
    let (mut slider_at, mut increment) = (at, at);
    if vertical {
        slider_at.y = at.y + BUTTON;
        increment.y = at.y + length - BUTTON;
    } else {
        slider_at.x = at.x + BUTTON;
        increment.x = at.x + length - BUTTON;
    }
    let slider = UiRect {
        top: slider_at.y,
        left: slider_at.x,
        right: slider_at.x + if vertical { BUTTON } else { length - 40.0 },
        bottom: slider_at.y + if vertical { length - 40.0 } else { BUTTON },
    };
    ScrollBarLayout { rect, slider, increment }
}

/// The record across host calls.
struct Bar<'a, H: ScrollBarHost> {
    h: &'a mut H,
    f: ScrollBarFields,
}

impl<'a, H: ScrollBarHost> Bar<'a, H> {
    fn new(h: &'a mut H) -> Self {
        let f = h.load();
        Bar { h, f }
    }
    fn call<R>(&mut self, g: impl FnOnce(&mut H) -> R) -> R {
        self.h.store(&self.f);
        let r = g(self.h);
        self.f = self.h.load();
        r
    }
    fn finish(self) {
        self.h.store(&self.f);
    }
    fn over(&self, part: ScrollPart) -> bool {
        self.h.part_rect(part).contains(self.h.mouse_location())
    }
}

/// `UIScrollBar::MouseLButtonDown`: presses the arrow under the mouse, else the slider; captures the mouse.
pub fn mouse_l_button_down(h: &mut impl ScrollBarHost) -> c_long {
    let mut b = Bar::new(h);
    let r;
    if b.over(ScrollPart::Decrement) {
        r = b.call(|h| h.part_mouse_down(ScrollPart::Decrement));
        b.f.decrement_pressed = true;
    } else if b.over(ScrollPart::Increment) {
        r = b.call(|h| h.part_mouse_down(ScrollPart::Increment));
        b.f.increment_pressed = true;
    } else {
        r = b.call(|h| h.part_mouse_down(ScrollPart::Slider));
        b.f.slider_pressed = true;
    }
    b.call(|h| h.capture_mouse());
    b.finish();
    r
}

/// `UIScrollBar::MouseLButtonUp`: releases the pressed parts (slider, increment, decrement) and the capture.
pub fn mouse_l_button_up(h: &mut impl ScrollBarHost) -> c_long {
    let mut b = Bar::new(h);
    let mut r = 0;
    if b.f.slider_pressed {
        r = b.call(|h| h.part_mouse_up(ScrollPart::Slider));
    }
    if b.f.increment_pressed {
        r = b.call(|h| h.part_mouse_up(ScrollPart::Increment));
    }
    if b.f.decrement_pressed {
        r = b.call(|h| h.part_mouse_up(ScrollPart::Decrement));
    }
    b.f.slider_pressed = false;
    b.f.increment_pressed = false;
    b.f.decrement_pressed = false;
    b.call(|h| h.release_mouse());
    b.finish();
    r
}

/// `UIScrollBar::MouseMove`: drags the slider when held or under the mouse; a held arrow repeats while the mouse is
/// on it.
pub fn mouse_move(h: &mut impl ScrollBarHost, at: UiPoint) -> c_long {
    let mut b = Bar::new(h);
    let mut r = 0;
    if b.f.slider_pressed || b.over(ScrollPart::Slider) {
        r = b.call(|h| h.slider_mouse_move(at));
    }
    if b.f.increment_pressed && b.over(ScrollPart::Increment) {
        b.call(|h| h.button_down(ScrollPart::Increment));
    }
    if b.f.decrement_pressed && b.over(ScrollPart::Decrement) {
        b.call(|h| h.button_down(ScrollPart::Decrement));
    }
    b.finish();
    r
}

/// `UIScrollBar::SetLocation`: the control, then the increment button and the slider for the style, then the
/// decrement button at the new location.
pub fn set_location(h: &mut impl ScrollBarHost, at: UiPoint) {
    h.base_set_location(at);
    let rect = h.rectangle();
    if h.slider_style() == 1 {
        h.part_set_location(ScrollPart::Increment, UiPoint { x: at.x, y: rect.bottom - BUTTON });
        let rect = h.rectangle();
        h.slider_set_rectangle(UiRect::new(at.x, at.y + BUTTON, at.x + BUTTON - 1.0, rect.bottom - BUTTON - 1.0));
    } else {
        h.part_set_location(ScrollPart::Increment, UiPoint { x: rect.right - BUTTON, y: at.y });
        let rect = h.rectangle();
        h.slider_set_rectangle(UiRect::new(at.x + BUTTON, at.y, rect.right - BUTTON - 1.0, at.y + BUTTON - 1.0));
    }
    h.part_set_location(ScrollPart::Decrement, at);
}

/// `UIScrollBar::IncrementBy`: moves the slider's value (a bar without a slider does nothing).
pub fn increment_by(h: &mut impl ScrollBarHost, amount: c_long) {
    if h.load().slider.is_null() {
        return;
    }
    let v = h.slider_value().wrapping_add(amount);
    h.set_slider_value(v);
}

#[cfg(test)]
mod tests;
