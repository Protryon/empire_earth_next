//! `extern "C"` for `UIScrollBar`'s members (`User Interface/UIScrollBar.cpp`): `bar` is the C++ `UIScrollBar*`,
//! `fields` its record from `mIncrementButton` on ([`ScrollBarFields`]), `rect` its `mRectangle`. The host calls go
//! back through `ee_uicpp_scroll_bar_*` (UIScrollBar.cpp). Only raw pointers cross a host call.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::rect::{UiPoint, UiRect};
use crate::scrollbar::{self, ScrollBarFields, ScrollBarHost, ScrollBarLayout, ScrollPart};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_ulong, c_void};

    use crate::rect::{UiPoint, UiRect};

    extern "C" {
        pub fn ee_uicpp_scroll_bar_mouse_location(bar: *mut c_void) -> UiPoint;
        pub fn ee_uicpp_scroll_bar_part_rect(bar: *mut c_void, part: c_int) -> UiRect;
        pub fn ee_uicpp_scroll_bar_part_mouse_down(bar: *mut c_void, part: c_int) -> c_long;
        pub fn ee_uicpp_scroll_bar_part_mouse_up(bar: *mut c_void, part: c_int) -> c_long;
        pub fn ee_uicpp_scroll_bar_slider_mouse_move(bar: *mut c_void, x: f32, y: f32) -> c_long;
        pub fn ee_uicpp_scroll_bar_button_down(bar: *mut c_void, part: c_int);
        pub fn ee_uicpp_scroll_bar_capture_mouse(bar: *mut c_void);
        pub fn ee_uicpp_scroll_bar_release_mouse(bar: *mut c_void);
        pub fn ee_uicpp_scroll_bar_base_set_location(bar: *mut c_void, x: f32, y: f32);
        pub fn ee_uicpp_scroll_bar_slider_style(bar: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_scroll_bar_part_set_location(bar: *mut c_void, part: c_int, x: f32, y: f32);
        pub fn ee_uicpp_scroll_bar_slider_set_rectangle(bar: *mut c_void, rect: *const UiRect);
        pub fn ee_uicpp_scroll_bar_slider_value(bar: *mut c_void) -> c_long;
        pub fn ee_uicpp_scroll_bar_set_slider_value(bar: *mut c_void, value: c_long);
    }
}

struct Raw {
    bar: *mut c_void,
    fields: *mut ScrollBarFields,
    rect: *const UiRect,
}

impl ScrollBarHost for Raw {
    fn load(&self) -> ScrollBarFields {
        unsafe { self.fields.read() }
    }
    fn store(&mut self, f: &ScrollBarFields) {
        unsafe { self.fields.write(*f) }
    }
    fn rectangle(&self) -> UiRect {
        unsafe { self.rect.read() }
    }
    fn mouse_location(&self) -> UiPoint {
        unsafe { sys::ee_uicpp_scroll_bar_mouse_location(self.bar) }
    }
    fn part_rect(&self, part: ScrollPart) -> UiRect {
        unsafe { sys::ee_uicpp_scroll_bar_part_rect(self.bar, part as i32) }
    }
    fn part_mouse_down(&mut self, part: ScrollPart) -> c_long {
        unsafe { sys::ee_uicpp_scroll_bar_part_mouse_down(self.bar, part as i32) }
    }
    fn part_mouse_up(&mut self, part: ScrollPart) -> c_long {
        unsafe { sys::ee_uicpp_scroll_bar_part_mouse_up(self.bar, part as i32) }
    }
    fn slider_mouse_move(&mut self, at: UiPoint) -> c_long {
        unsafe { sys::ee_uicpp_scroll_bar_slider_mouse_move(self.bar, at.x, at.y) }
    }
    fn button_down(&mut self, part: ScrollPart) {
        unsafe { sys::ee_uicpp_scroll_bar_button_down(self.bar, part as i32) }
    }
    fn capture_mouse(&mut self) {
        unsafe { sys::ee_uicpp_scroll_bar_capture_mouse(self.bar) }
    }
    fn release_mouse(&mut self) {
        unsafe { sys::ee_uicpp_scroll_bar_release_mouse(self.bar) }
    }
    fn base_set_location(&mut self, at: UiPoint) {
        unsafe { sys::ee_uicpp_scroll_bar_base_set_location(self.bar, at.x, at.y) }
    }
    fn slider_style(&self) -> c_ulong {
        unsafe { sys::ee_uicpp_scroll_bar_slider_style(self.bar) }
    }
    fn part_set_location(&mut self, part: ScrollPart, at: UiPoint) {
        unsafe { sys::ee_uicpp_scroll_bar_part_set_location(self.bar, part as i32, at.x, at.y) }
    }
    fn slider_set_rectangle(&mut self, rect: UiRect) {
        unsafe { sys::ee_uicpp_scroll_bar_slider_set_rectangle(self.bar, &rect) }
    }
    fn slider_value(&self) -> c_long {
        unsafe { sys::ee_uicpp_scroll_bar_slider_value(self.bar) }
    }
    fn set_slider_value(&mut self, value: c_long) {
        unsafe { sys::ee_uicpp_scroll_bar_set_slider_value(self.bar, value) }
    }
}

fn raw(bar: *mut c_void, fields: *mut ScrollBarFields, rect: *const UiRect) -> Raw {
    Raw { bar, fields, rect }
}

/// The constructor's geometry ([`scrollbar::layout`]).
#[no_mangle]
pub extern "C" fn ee_ui_scroll_bar_layout(at: UiPoint, length: f32, style: c_ulong) -> ScrollBarLayout {
    scrollbar::layout(at, length, style)
}

/// `UIScrollBar::MouseLButtonDown`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scroll_bar_mouse_l_button_down(
    bar: *mut c_void,
    fields: *mut ScrollBarFields,
    rect: *const UiRect,
) -> c_long {
    scrollbar::mouse_l_button_down(&mut raw(bar, fields, rect))
}

/// `UIScrollBar::MouseLButtonUp`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scroll_bar_mouse_l_button_up(
    bar: *mut c_void,
    fields: *mut ScrollBarFields,
    rect: *const UiRect,
) -> c_long {
    scrollbar::mouse_l_button_up(&mut raw(bar, fields, rect))
}

/// `UIScrollBar::MouseMove`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scroll_bar_mouse_move(
    bar: *mut c_void,
    fields: *mut ScrollBarFields,
    rect: *const UiRect,
    at: UiPoint,
) -> c_long {
    scrollbar::mouse_move(&mut raw(bar, fields, rect), at)
}

/// `UIScrollBar::SetLocation`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scroll_bar_set_location(
    bar: *mut c_void,
    fields: *mut ScrollBarFields,
    rect: *const UiRect,
    at: UiPoint,
) {
    scrollbar::set_location(&mut raw(bar, fields, rect), at);
}

/// `UIScrollBar::IncrementBy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scroll_bar_increment_by(
    bar: *mut c_void,
    fields: *mut ScrollBarFields,
    rect: *const UiRect,
    amount: c_long,
) {
    scrollbar::increment_by(&mut raw(bar, fields, rect), amount);
}
