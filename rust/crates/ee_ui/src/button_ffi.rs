//! `extern "C"` for the button subclasses' algorithms (`User Interface/UIScrollButton.cpp`, `UICheckBox.cpp`,
//! `UIRadioButton.cpp`) over the whole `UIButton` object (`buttonobj.rs`, R6 ui round 11). `refs` names the C++
//! object, its Rust button and the subclass's record ([`ButtonRefs`]); `UIButton`'s part goes through the object's
//! ops, the subclass's own host calls through `ee_uicpp_check_*`, `ee_uicpp_radio_*`, `ee_uicpp_label_*` and
//! `ee_uicpp_scroll_button_*`. Only raw pointers cross a host call: the records are read after each.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_long, c_ulong, c_void, CStr};

use crate::button::{self, RadioFields, RadioHost, ScrollButtonHost};
use crate::buttonobj::{Btn, EeUiButton};
use crate::rect::{UiPoint, UiRect};

/// One button subclass as Rust sees it: the C++ object (as a `UIButton*`), its Rust button, the subclass's record
/// (`CheckFields`, `RadioFields`, the scroll button's `mDecrement`) and `ctx`, the C++ forwarder's own context for a
/// host call (a `SetText`'s text and font; NULL otherwise).
#[repr(C)]
pub struct ButtonRefs {
    pub button: *mut c_void,
    pub obj: *const EeUiButton,
    pub sub: *mut c_void,
    pub ctx: *mut c_void,
}

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_long, c_void};

    use crate::rect::UiRect;

    extern "C" {
        pub fn ee_uicpp_scroll_button_delay_running(b: *mut c_void) -> bool;
        pub fn ee_uicpp_scroll_button_delay_poll(b: *mut c_void) -> bool;
        pub fn ee_uicpp_scroll_button_delay_start(b: *mut c_void);
        pub fn ee_uicpp_scroll_button_delay_stop(b: *mut c_void);
        pub fn ee_uicpp_scroll_button_repeat_running(b: *mut c_void) -> bool;
        pub fn ee_uicpp_scroll_button_repeat_poll(b: *mut c_void) -> bool;
        pub fn ee_uicpp_scroll_button_repeat_start(b: *mut c_void);
        pub fn ee_uicpp_scroll_button_slider_value(b: *mut c_void) -> c_long;
        pub fn ee_uicpp_scroll_button_set_slider_value(b: *mut c_void, value: c_long);

        pub fn ee_uicpp_label_set_text(b: *mut c_void, ctx: *mut c_void);
        pub fn ee_uicpp_label_create(b: *mut c_void, ctx: *mut c_void, rect: *const UiRect);
        pub fn ee_uicpp_label_set_location(b: *mut c_void, ctx: *mut c_void, x: f32, y: f32);

        pub fn ee_uicpp_radio_swap_bitmaps(b: *mut c_void);
        pub fn ee_uicpp_radio_control_count(b: *mut c_void) -> c_long;
        pub fn ee_uicpp_radio_is_radio(b: *mut c_void, i: c_long) -> bool;
        pub fn ee_uicpp_radio_checked(b: *mut c_void, i: c_long) -> bool;
        pub fn ee_uicpp_radio_uncheck(b: *mut c_void, i: c_long);
    }
}

/// The C++ object behind a subclass's button.
fn cpp(b: &Btn) -> *mut c_void {
    b.o.host.obj
}

impl ScrollButtonHost for Btn<'_> {
    fn decrement(&self) -> c_ulong {
        unsafe { (self.sub as *const c_ulong).read() }
    }
    fn delay_running(&self) -> bool {
        unsafe { sys::ee_uicpp_scroll_button_delay_running(cpp(self)) }
    }
    fn delay_poll(&mut self) -> bool {
        unsafe { sys::ee_uicpp_scroll_button_delay_poll(cpp(self)) }
    }
    fn delay_start(&mut self) {
        unsafe { sys::ee_uicpp_scroll_button_delay_start(cpp(self)) }
    }
    fn delay_stop(&mut self) {
        unsafe { sys::ee_uicpp_scroll_button_delay_stop(cpp(self)) }
    }
    fn repeat_running(&self) -> bool {
        unsafe { sys::ee_uicpp_scroll_button_repeat_running(cpp(self)) }
    }
    fn repeat_poll(&mut self) -> bool {
        unsafe { sys::ee_uicpp_scroll_button_repeat_poll(cpp(self)) }
    }
    fn repeat_start(&mut self) {
        unsafe { sys::ee_uicpp_scroll_button_repeat_start(cpp(self)) }
    }
    fn slider_value(&self) -> c_long {
        unsafe { sys::ee_uicpp_scroll_button_slider_value(cpp(self)) }
    }
    fn set_slider_value(&mut self, value: c_long) {
        unsafe { sys::ee_uicpp_scroll_button_set_slider_value(cpp(self), value) }
    }
}

/// The radio button's label (its C++ record's, `ctx` naming it): `checkobj.rs`'s `LabelHost` for a C++ record.
pub(crate) fn cpp_label_set_text(b: &mut Btn) {
    unsafe { sys::ee_uicpp_label_set_text(cpp(b), b.ctx) }
}
pub(crate) fn cpp_label_create(b: &mut Btn, rect: UiRect) {
    unsafe { sys::ee_uicpp_label_create(cpp(b), b.ctx, &rect) }
}
pub(crate) fn cpp_label_set_location(b: &mut Btn, at: UiPoint) {
    unsafe { sys::ee_uicpp_label_set_location(cpp(b), b.ctx, at.x, at.y) }
}

impl RadioHost for Btn<'_> {
    fn load_radio(&self) -> RadioFields {
        unsafe { (self.sub as *const RadioFields).read() }
    }
    fn store_radio(&mut self, r: &RadioFields) {
        unsafe { (self.sub as *mut RadioFields).write(*r) }
    }
    fn swap_bitmaps(&mut self) {
        unsafe { sys::ee_uicpp_radio_swap_bitmaps(cpp(self)) }
    }
    fn control_count(&self) -> c_long {
        unsafe { sys::ee_uicpp_radio_control_count(cpp(self)) }
    }
    fn is_radio(&self, i: c_long) -> bool {
        unsafe { sys::ee_uicpp_radio_is_radio(cpp(self), i) }
    }
    fn radio_checked(&self, i: c_long) -> bool {
        unsafe { sys::ee_uicpp_radio_checked(cpp(self), i) }
    }
    fn uncheck_radio(&mut self, i: c_long) {
        unsafe { sys::ee_uicpp_radio_uncheck(cpp(self), i) }
    }
}

unsafe fn raw<'a>(refs: *const ButtonRefs) -> Btn<'a> {
    let r = &*refs;
    Btn { o: &*r.obj, sub: r.sub, ctx: r.ctx, text: 0, font: 0 }
}

unsafe fn text<'a>(s: *const c_char) -> &'a [u8] {
    CStr::from_ptr(s).to_bytes_with_nul()
}

/// `UIScrollButton::ButtonDown` (`sub`: `mDecrement`).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scroll_button_button_down(refs: *const ButtonRefs) {
    button::scroll_button_down(&mut raw(refs));
}

/// The check box's and radio button's bitmap rectangle ([`button::box_rect`]).
#[no_mangle]
pub extern "C" fn ee_ui_button_box_rect(rect: UiRect, scale_bitmap: bool) -> UiRect {
    button::box_rect(&rect, scale_bitmap)
}

/// The check box's (`float_border`) and radio button's bitmap offset: true and `out` written, or false to keep it
/// ([`button::label_bitmap_offset`]).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_label_bitmap_offset(
    height: f32,
    border: c_ulong,
    scale_bitmap: bool,
    float_border: bool,
    out: *mut UiPoint,
) -> bool {
    match button::label_bitmap_offset(height, border, scale_bitmap, float_border) {
        Some(p) => {
            out.write(p);
            true
        }
        None => false,
    }
}

/// `UIRadioButton::Check`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_radio_check(refs: *const ButtonRefs) {
    button::radio_check(&mut raw(refs));
}

/// `UIRadioButton::UnCheck`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_radio_uncheck(refs: *const ButtonRefs) {
    button::radio_uncheck(&mut raw(refs));
}

/// `UIRadioButton::RemoveOtherChecks`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_radio_remove_other_checks(refs: *const ButtonRefs) {
    button::radio_remove_other_checks(&mut raw(refs));
}

/// `UIRadioButton::CopyData`'s button part (the label's stays C++).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_radio_copy_data(refs: *const ButtonRefs) -> c_long {
    button::radio_copy_data(&mut raw(refs))
}

/// `UIRadioButton::MouseLButtonDown`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_radio_mouse_l_button_down(refs: *const ButtonRefs) -> c_long {
    button::radio_mouse_l_button_down(&mut raw(refs))
}

/// `UIRadioButton::SetLocation` (`ctx`: the label, as for `SetText`).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_radio_set_location(refs: *const ButtonRefs, at: UiPoint) {
    button::radio_set_location(&mut raw(refs), at);
}

/// `UIRadioButton::SetText` (`ctx` as for the check box).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_radio_set_text(refs: *const ButtonRefs, s: *const c_char) {
    button::radio_set_text(&mut raw(refs), text(s));
}
