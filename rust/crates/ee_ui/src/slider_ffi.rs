//! `extern "C"` for `UISlider`'s members (`User Interface/UISlider.cpp`): `slider` is the C++ `UISlider*`, `fields`
//! its record from `mThumbRectangle` on ([`SliderFields`]), `rectangle` its `mRectangle`. The host calls go back
//! through `ee_uicpp_slider_*` (UISlider.cpp). Only raw pointers cross a host call: the record is read after each.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::rect::{UiPoint, UiRect};
use crate::slider::{self, SliderFields, SliderHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_ulong, c_void};

    extern "C" {
        pub fn ee_uicpp_slider_set_dirty(slider: *mut c_void);
        pub fn ee_uicpp_slider_change_range(slider: *mut c_void, value: c_ulong);
        pub fn ee_uicpp_slider_base_set_location(slider: *mut c_void, x: f32, y: f32);
    }
}

struct Raw {
    slider: *mut c_void,
    fields: *mut SliderFields,
    rectangle: *const UiRect,
}

impl SliderHost for Raw {
    fn load(&self) -> SliderFields {
        unsafe { self.fields.read() }
    }
    fn store(&mut self, fields: &SliderFields) {
        unsafe { self.fields.write(*fields) }
    }
    fn rectangle(&self) -> UiRect {
        unsafe { self.rectangle.read() }
    }
    fn set_dirty(&mut self) {
        unsafe { sys::ee_uicpp_slider_set_dirty(self.slider) }
    }
    fn change_range(&mut self, value: c_ulong) {
        unsafe { sys::ee_uicpp_slider_change_range(self.slider, value) }
    }
    fn base_set_location(&mut self, at: UiPoint) {
        unsafe { sys::ee_uicpp_slider_base_set_location(self.slider, at.x, at.y) }
    }
}

fn raw(slider: *mut c_void, fields: *mut SliderFields, rectangle: *const UiRect) -> Raw {
    Raw { slider, fields, rectangle }
}

/// The constructor's track and thumb ([`slider::init`]).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_init(fields: *mut SliderFields, rectangle: *const UiRect, style: c_ulong) {
    let mut f = fields.read();
    slider::init(&mut f, &rectangle.read(), style);
    fields.write(f);
}

/// `UISlider::ButtonUp`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_button_up(
    slider: *mut c_void,
    fields: *mut SliderFields,
    rectangle: *const UiRect,
    mouse: UiPoint,
) {
    slider::button_up(&mut raw(slider, fields, rectangle), mouse);
}

/// `UISlider::DecrementByPageSize`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_decrement_by_page_size(
    slider: *mut c_void,
    fields: *mut SliderFields,
    rectangle: *const UiRect,
) {
    slider::decrement_by_page_size(&mut raw(slider, fields, rectangle));
}

/// `UISlider::IncrementByPageSize`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_increment_by_page_size(
    slider: *mut c_void,
    fields: *mut SliderFields,
    rectangle: *const UiRect,
) {
    slider::increment_by_page_size(&mut raw(slider, fields, rectangle));
}

/// `UISlider::GetLocFromRange` (`out` keeps nothing: both coordinates are written).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_loc_from_range(
    fields: *const SliderFields,
    rectangle: *const UiRect,
    value: c_long,
    out: *mut UiPoint,
) {
    let mut p = out.read();
    fields.read().loc_from_range(&rectangle.read(), value, &mut p);
    out.write(p);
}

/// `UISlider::GetRangeFromLoc`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_range_from_loc(fields: *const SliderFields, at: UiPoint) -> c_ulong {
    fields.read().range_from_loc(at)
}

/// The middle of `UISlider::MouseLButtonDown`: a drag starts on the thumb.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_press(fields: *mut SliderFields, mouse: UiPoint) {
    let mut f = fields.read();
    slider::press(&mut f, mouse);
    fields.write(f);
}

/// `UISlider::MouseMove`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_mouse_move(
    slider: *mut c_void,
    fields: *mut SliderFields,
    rectangle: *const UiRect,
    at: UiPoint,
) {
    slider::mouse_move(&mut raw(slider, fields, rectangle), at);
}

/// `UISlider::SetCurrentRangeValue`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_set_current_range_value(
    slider: *mut c_void,
    fields: *mut SliderFields,
    rectangle: *const UiRect,
    value: c_long,
) {
    slider::set_current_range_value(&mut raw(slider, fields, rectangle), value);
}

/// `UISlider::SetLocation` -> the thumb's thickness (the C++ rescales the thumb bitmap with it).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_set_location(
    slider: *mut c_void,
    fields: *mut SliderFields,
    rectangle: *const UiRect,
    at: UiPoint,
) -> f32 {
    slider::set_location(&mut raw(slider, fields, rectangle), at)
}

/// `UISlider::SetRange`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_set_range(
    slider: *mut c_void,
    fields: *mut SliderFields,
    rectangle: *const UiRect,
    min: c_ulong,
    max: c_ulong,
) {
    slider::set_range(&mut raw(slider, fields, rectangle), min, max);
}
