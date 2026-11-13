//! `UISlider` as a whole Rust object (R6 ui round 10): the thumb's rectangle and its copy, the thumb bitmap (a C++
//! object held by address), the style, the pixel scale, the value range, the page size, the track's pixels and the
//! drag, with every method of `UISlider.cpp` and the header's.
//!
//! The C++ `UISlider` stays the type the forms, the scroll bar and the scenario editor hold (a `UIControl` with its
//! vtable): its methods are one-line forwarders to [`ee_ui_slider_call`], its constructor and destructor make and
//! free this object, and every call out is an [`EeUiOp`] through its host. `ChangeRange`, `ButtonDown`, `ButtonUp`
//! and `SetDirty` are virtuals and go through the C++ object (`EEUIFSEESElevation::ChangeRange` overrides one and
//! reads the slider back), so the state is never borrowed across an op.
//!
//! The arithmetic is round 1's (`slider.rs`, pinned by its golden); this object is its [`SliderHost`].

use core::cell::RefCell;
use core::ffi::{c_long, c_ulong, c_void};

use crate::obj::{read_buf, rect_buf, write_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::{UiPoint, UiRect};
use crate::slider::{self, SliderFields, SliderHost};

/// The inward calls (the C++ shim's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiSliderCall {
    /// `CopyData()` -> its result.
    CopyData = 1,
    /// `DrawPolygons()` -> its result.
    DrawPolygons = 2,
    /// `GetLocFromRange(a, f)`: `f[0]`, `f[1]` the location in and out.
    LocFromRange = 3,
    /// `GetRangeFromLoc(f[0], f[1])`.
    RangeFromLoc = 4,
    /// `GetCurrentRangeValue()`.
    CurrentRangeValue = 5,
    /// `GetIsDragging()`.
    IsDragging = 6,
    /// `GetMaxRange()`.
    MaxRange = 7,
    /// `GetMinRange()`.
    MinRange = 8,
    /// `GetPageSize()`.
    PageSize = 9,
    /// `GetRange()`.
    Range = 10,
    /// `GetSliderStyle()`.
    Style = 11,
    /// `GetThumbRectangle()` -> `f`.
    ThumbRectangle = 12,
    /// `SetCurrentRangeValue(a)`.
    SetCurrentRangeValue = 13,
    /// `SetLocation(f[0], f[1])`.
    SetLocation = 14,
    /// `SetPageSize(a)`.
    SetPageSize = 15,
    /// `SetRange(a, b)`.
    SetRange = 16,
    /// `UISlider::ButtonUp()`.
    ButtonUp = 17,
    /// `UISlider::ChangeRange(a)`.
    ChangeRange = 18,
    /// `DecrementByPageSize()`.
    DecrementByPageSize = 19,
    /// `IncrementByPageSize()`.
    IncrementByPageSize = 20,
    /// `MouseLButtonDown()` -> OK.
    MouseLButtonDown = 21,
    /// `MouseLButtonUp()` -> OK.
    MouseLButtonUp = 22,
    /// `MouseMove(f[0], f[1])` -> OK.
    MouseMove = 23,
    /// `SetIsDragging(a != 0)`.
    SetIsDragging = 24,
    /// `GetCurrentPoint(f[0], f[1])`.
    CurrentPoint = 25,
}

/// The Rust UISlider.
pub struct EeUiSlider {
    host: Host,
    f: RefCell<SliderFields>,
}

/// The slider as round 1's arithmetic sees it: the record copied in and out around every op.
struct H<'a>(&'a EeUiSlider);

impl SliderHost for H<'_> {
    fn load(&self) -> SliderFields {
        *self.0.f.borrow()
    }
    fn store(&mut self, fields: &SliderFields) {
        *self.0.f.borrow_mut() = *fields;
    }
    fn rectangle(&self) -> UiRect {
        self.0.host.rect()
    }
    fn set_dirty(&mut self) {
        self.0.host.op(Op::SetDirty, 1, 0, 0, 0);
    }
    fn change_range(&mut self, value: c_ulong) {
        self.0.host.op(Op::SliderChangeRange, value as isize, 0, 0, 0);
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.0.host.op_point(Op::BaseSetLocation, 0, at);
    }
}

impl EeUiSlider {
    /// The constructor's part after `UIControl`'s (the C++ set `mBorderType` to 0): the style, the track and the
    /// square thumb at the control's top-left, the thumb bitmap scaled to it, an empty range and no page size.
    ///
    /// The original called `SetRange(0, 0)` on members it had not set; whichever way its first test went the result
    /// is this one (range 0, both scales 0, the value 0 inside). The copied thumb starts zeroed (the original's was
    /// unset until the first `CopyData`).
    pub fn new(host: Host, style: c_ulong) -> Self {
        let zero = UiRect::default();
        let mut f = SliderFields {
            thumb: zero,
            copied_thumb: zero,
            thumb_bitmap: core::ptr::null_mut(),
            style,
            pixels_per_unit: 0.0,
            units_per_pixel: 0.0,
            current: 0,
            page_size: 0,
            range: 0,
            min_range: 0,
            max_range: 0,
            min_pixel: 0,
            max_pixel: 0,
            max_thumb_pixel: 0,
            drag_offset: 0,
            is_dragging: false,
        };
        let rect = host.rect();
        slider::init(&mut f, &rect, style);
        let bitmap = host.op_rect(Op::ThumbCreate, 0, 0, f.thumb);
        f.thumb_bitmap = bitmap as *mut c_void;
        EeUiSlider { host, f: RefCell::new(f) }
    }

    fn fields(&self) -> SliderFields {
        *self.f.borrow()
    }

    /// The destructor: the thumb bitmap's reference.
    pub fn release(&self) {
        let bitmap = self.fields().thumb_bitmap;
        self.host.op(Op::ThumbRelease, bitmap as isize, 0, 0, 0);
    }

    pub fn copy_data(&self) -> isize {
        let r = self.host.op(Op::BaseCopyData, 0, 0, 0, 0);
        let mut f = self.f.borrow_mut();
        f.copied_thumb = f.thumb;
        r
    }

    pub fn draw_polygons(&self) -> isize {
        self.host.op(Op::BaseDrawPolygons, 0, 0, 0, 0);
        let f = self.fields();
        let bitmap = f.thumb_bitmap as isize;
        self.host.op_point(Op::ThumbSetLocation, bitmap, f.copied_thumb.top_left());
        self.host.op(Op::ThumbDraw, bitmap, 0, 0, 0)
    }

    pub fn set_location(&self, at: UiPoint) {
        let thickness = slider::set_location(&mut H(self), at);
        let scale = self.host.get_point(Op::DragBarScale, 0);
        let bitmap = self.fields().thumb_bitmap as isize;
        let mut b = [scale.x * thickness * 0.05, scale.y * thickness * 0.05, 0.0, 0.0];
        self.host.op_f(Op::ThumbSetScale, bitmap, 0, 0, 0, &mut b);
    }

    pub fn change_range(&self, value: c_ulong) {
        self.f.borrow_mut().current = value as c_long;
        self.host.op(Op::FireTrigger, crate::obj::TRIGGER_RANGE_CHANGED, 0, 0, 0);
    }

    pub fn mouse_l_button_down(&self) -> isize {
        self.host.op(Op::CaptureMouse, 0, 0, 0, 0);
        let mouse = self.host.mouse();
        slider::press(&mut self.f.borrow_mut(), mouse);
        self.host.op(Op::SliderButtonDown, 0, 0, 0, 0);
        0
    }

    pub fn mouse_l_button_up(&self) -> isize {
        self.host.op(Op::ReleaseMouse, 0, 0, 0, 0);
        self.host.op(Op::SliderButtonUp, 0, 0, 0, 0);
        0
    }

    /// One inward call; `f` points at four floats (or is NULL for the calls without).
    ///
    /// # Safety
    /// `f` is NULL or four live floats.
    pub unsafe fn call(&self, call: EeUiSliderCall, a: isize, b: isize, f: *mut f32) -> isize {
        type C = EeUiSliderCall;
        // SAFETY: the contract
        let buf = unsafe { read_buf(f) };
        let at = UiPoint { x: buf[0], y: buf[1] };
        match call {
            C::CopyData => return self.copy_data(),
            C::DrawPolygons => return self.draw_polygons(),
            C::LocFromRange => {
                let rect = self.host.rect();
                let mut p = at;
                self.fields().loc_from_range(&rect, a as c_long, &mut p);
                // SAFETY: the contract
                unsafe { write_buf(f, [p.x, p.y, buf[2], buf[3]]) };
            }
            C::RangeFromLoc => return self.fields().range_from_loc(at) as isize,
            C::CurrentRangeValue => return self.fields().current as isize,
            C::IsDragging => return self.fields().is_dragging as isize,
            C::MaxRange => return self.fields().max_range as isize,
            C::MinRange => return self.fields().min_range as isize,
            C::PageSize => return self.fields().page_size as isize,
            C::Range => return self.fields().range as isize,
            C::Style => return self.fields().style as isize,
            C::ThumbRectangle => {
                // SAFETY: the contract
                unsafe { write_buf(f, rect_buf(self.fields().thumb)) };
            }
            C::SetCurrentRangeValue => slider::set_current_range_value(&mut H(self), a as c_long),
            C::SetLocation => self.set_location(at),
            C::SetPageSize => self.f.borrow_mut().page_size = a as c_ulong,
            C::SetRange => slider::set_range(&mut H(self), a as c_ulong, b as c_ulong),
            C::ButtonUp => {
                let mouse = self.host.mouse();
                slider::button_up(&mut H(self), mouse);
            }
            C::ChangeRange => self.change_range(a as c_ulong),
            C::DecrementByPageSize => slider::decrement_by_page_size(&mut H(self)),
            C::IncrementByPageSize => slider::increment_by_page_size(&mut H(self)),
            C::MouseLButtonDown => return self.mouse_l_button_down(),
            C::MouseLButtonUp => return self.mouse_l_button_up(),
            C::MouseMove => slider::mouse_move(&mut H(self), at),
            C::SetIsDragging => self.f.borrow_mut().is_dragging = a != 0,
            C::CurrentPoint => return self.fields().current_point(at) as isize,
        }
        0
    }
}

// ---- the C ABI

/// A new Rust slider for the C++ `slider` (its `UIControl` part constructed), its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_ui_slider_new(slider: *mut c_void, host: EeUiHostFn, style: c_ulong) -> *mut EeUiSlider {
    Box::into_raw(Box::new(EeUiSlider::new(Host { obj: slider, f: host }, style)))
}

/// The destructor: releases the thumb bitmap and frees the object.
///
/// # Safety
/// `slider` came from `ee_ui_slider_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_free(slider: *mut EeUiSlider) {
    if slider.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*slider).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(slider) });
}

/// One of UISlider's methods ([`EeUiSliderCall`]).
///
/// # Safety
/// `slider` is live; `f` is NULL or four live floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_call(
    slider: *const EeUiSlider,
    call: EeUiSliderCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*slider).call(call, a, b, f) }
}

#[cfg(test)]
#[path = "sliderobj_tests.rs"]
mod tests;
