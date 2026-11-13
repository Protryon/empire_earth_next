//! `extern "C"` for the button subclasses' algorithms (`User Interface/UIScrollButton.cpp`, `UICheckBox.cpp`,
//! `UIRadioButton.cpp`) over the whole `UIButton` object (`buttonobj.rs`, R6 ui round 11). `refs` names the C++
//! object, its Rust button and the subclass's record ([`ButtonRefs`]); `UIButton`'s part goes through the object's
//! ops, the subclass's own host calls through `ee_uicpp_check_*`, `ee_uicpp_radio_*`, `ee_uicpp_label_*` and
//! `ee_uicpp_scroll_button_*`. Only raw pointers cross a host call: the records are read after each.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::button::{self, ScrollButtonHost};
use crate::buttonobj::{Btn, EeUiButton};
use crate::rect::UiRect;

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

unsafe fn raw<'a>(refs: *const ButtonRefs) -> Btn<'a> {
    let r = &*refs;
    Btn { o: &*r.obj, sub: r.sub, text: 0, font: 0 }
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
