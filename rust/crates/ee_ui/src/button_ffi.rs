//! `extern "C"` for the buttons (`User Interface/UIButton.cpp`, `UIDualButton.cpp`, `UIScrollButton.cpp`,
//! `UICheckBox.cpp`, `UIRadioButton.cpp`). `refs` names the C++ object and its members ([`ButtonRefs`]); the host
//! calls go back through `ee_uicpp_button_*` (UIButton.cpp) and the subclasses' `ee_uicpp_check_*`,
//! `ee_uicpp_radio_*` and `ee_uicpp_scroll_button_*`. Only raw pointers cross a host call: the records are read
//! after each.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_long, c_ulong, c_void, CStr};

use crate::button::{
    self, ButtonFields, ButtonHost, ButtonState, CheckFields, CheckHost, Key, LabelHost, RadioFields, RadioHost,
    ScrollButtonHost,
};
use crate::rect::{UiPoint, UiRect};

/// One button as Rust sees it: the C++ object (as a `UIButton*`), its record from `mTextRectangle` on, the
/// `UIControl` members it uses, the subclass's record (`CheckFields`, `RadioFields`, the scroll button's
/// `mDecrement`; NULL for a plain button) and `ctx`, the C++ forwarder's own context for a host call (a `SetText`'s
/// text and font; NULL otherwise).
#[repr(C)]
pub struct ButtonRefs {
    pub button: *mut c_void,
    pub fields: *mut ButtonFields,
    pub border_type: *mut c_ulong,
    pub rect: *const UiRect,
    pub border: *const c_ulong,
    pub index: *const c_ulong,
    pub sub: *mut c_void,
    pub ctx: *mut c_void,
}

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_char, c_int, c_long, c_ulong, c_void};

    use crate::rect::{UiPoint, UiRect};

    extern "C" {
        pub fn ee_uicpp_button_tab_stop(b: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_button_tabbing_on(b: *mut c_void) -> bool;
        pub fn ee_uicpp_button_set_tab_stop(b: *mut c_void, index: c_ulong);
        pub fn ee_uicpp_button_tabable(b: *mut c_void) -> bool;
        pub fn ee_uicpp_button_set_dirty(b: *mut c_void);
        pub fn ee_uicpp_button_play(b: *mut c_void, sound: c_ulong);
        pub fn ee_uicpp_button_capture_mouse(b: *mut c_void);
        pub fn ee_uicpp_button_release_mouse(b: *mut c_void);
        pub fn ee_uicpp_button_capture_keyboard(b: *mut c_void);
        pub fn ee_uicpp_button_release_keyboard(b: *mut c_void);
        pub fn ee_uicpp_button_fire(b: *mut c_void, event: c_ulong);
        pub fn ee_uicpp_button_mouse_location(b: *mut c_void) -> UiPoint;
        pub fn ee_uicpp_button_has_mouse_capture(b: *mut c_void) -> bool;
        pub fn ee_uicpp_button_right_button_down(b: *mut c_void) -> bool;
        pub fn ee_uicpp_button_key(b: *mut c_void, key: c_int) -> bool;
        pub fn ee_uicpp_button_button_down(b: *mut c_void);
        pub fn ee_uicpp_button_button_up(b: *mut c_void);
        pub fn ee_uicpp_button_mouse_over(b: *mut c_void);
        pub fn ee_uicpp_button_mouse_move(b: *mut c_void, x: f32, y: f32);
        pub fn ee_uicpp_button_base_set_location(b: *mut c_void, x: f32, y: f32);
        pub fn ee_uicpp_button_base_copy_data(b: *mut c_void) -> c_long;
        pub fn ee_uicpp_button_set_copied_bitmap(b: *mut c_void, bitmap: *mut c_void);
        pub fn ee_uicpp_button_text(b: *mut c_void) -> *const c_char;
        pub fn ee_uicpp_button_text_extent(b: *mut c_void, w: *mut f32, h: *mut f32);
        pub fn ee_uicpp_button_copy_text(b: *mut c_void, focused: bool);

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

        pub fn ee_uicpp_check_set_bitmaps(b: *mut c_void, on: bool);

        pub fn ee_uicpp_radio_swap_bitmaps(b: *mut c_void);
        pub fn ee_uicpp_radio_control_count(b: *mut c_void) -> c_long;
        pub fn ee_uicpp_radio_is_radio(b: *mut c_void, i: c_long) -> bool;
        pub fn ee_uicpp_radio_checked(b: *mut c_void, i: c_long) -> bool;
        pub fn ee_uicpp_radio_uncheck(b: *mut c_void, i: c_long);
    }
}

struct Raw<'a>(&'a ButtonRefs);

impl ButtonHost for Raw<'_> {
    fn load(&self) -> ButtonState {
        unsafe {
            ButtonState {
                f: self.0.fields.read(),
                border_type: self.0.border_type.read(),
                rect: self.0.rect.read(),
                border: self.0.border.read(),
                index: self.0.index.read(),
            }
        }
    }
    fn store(&mut self, s: &ButtonState) {
        unsafe {
            self.0.fields.write(s.f);
            self.0.border_type.write(s.border_type);
        }
    }
    fn tab_stop(&self) -> c_ulong {
        unsafe { sys::ee_uicpp_button_tab_stop(self.0.button) }
    }
    fn tabbing_on(&self) -> bool {
        unsafe { sys::ee_uicpp_button_tabbing_on(self.0.button) }
    }
    fn set_tab_stop(&mut self, index: c_ulong) {
        unsafe { sys::ee_uicpp_button_set_tab_stop(self.0.button, index) }
    }
    fn tabable(&self) -> bool {
        unsafe { sys::ee_uicpp_button_tabable(self.0.button) }
    }
    fn set_dirty(&mut self) {
        unsafe { sys::ee_uicpp_button_set_dirty(self.0.button) }
    }
    fn play(&mut self, sound: c_ulong) {
        unsafe { sys::ee_uicpp_button_play(self.0.button, sound) }
    }
    fn capture_mouse(&mut self) {
        unsafe { sys::ee_uicpp_button_capture_mouse(self.0.button) }
    }
    fn release_mouse(&mut self) {
        unsafe { sys::ee_uicpp_button_release_mouse(self.0.button) }
    }
    fn capture_keyboard(&mut self) {
        unsafe { sys::ee_uicpp_button_capture_keyboard(self.0.button) }
    }
    fn release_keyboard(&mut self) {
        unsafe { sys::ee_uicpp_button_release_keyboard(self.0.button) }
    }
    fn fire(&mut self, event: c_ulong) {
        unsafe { sys::ee_uicpp_button_fire(self.0.button, event) }
    }
    fn mouse_location(&self) -> UiPoint {
        unsafe { sys::ee_uicpp_button_mouse_location(self.0.button) }
    }
    fn has_mouse_capture(&self) -> bool {
        unsafe { sys::ee_uicpp_button_has_mouse_capture(self.0.button) }
    }
    fn right_button_down(&self) -> bool {
        unsafe { sys::ee_uicpp_button_right_button_down(self.0.button) }
    }
    fn key(&mut self, key: Key) -> bool {
        let k = match key {
            Key::SpaceJustPressed => 0,
            Key::EnterJustPressed => 1,
            Key::SpaceJustUnpressed => 2,
            Key::EnterJustUnpressed => 3,
        };
        unsafe { sys::ee_uicpp_button_key(self.0.button, k) }
    }
    fn button_down(&mut self) {
        unsafe { sys::ee_uicpp_button_button_down(self.0.button) }
    }
    fn button_up(&mut self) {
        unsafe { sys::ee_uicpp_button_button_up(self.0.button) }
    }
    fn mouse_over(&mut self) {
        unsafe { sys::ee_uicpp_button_mouse_over(self.0.button) }
    }
    fn mouse_move(&mut self, at: UiPoint) {
        unsafe { sys::ee_uicpp_button_mouse_move(self.0.button, at.x, at.y) }
    }
    fn base_set_location(&mut self, at: UiPoint) {
        unsafe { sys::ee_uicpp_button_base_set_location(self.0.button, at.x, at.y) }
    }
    fn base_copy_data(&mut self) -> c_long {
        unsafe { sys::ee_uicpp_button_base_copy_data(self.0.button) }
    }
    fn set_copied_bitmap(&mut self, bitmap: *mut c_void) {
        unsafe { sys::ee_uicpp_button_set_copied_bitmap(self.0.button, bitmap) }
    }
    fn text(&self) -> Vec<u8> {
        // SAFETY: the host returns mText's live NUL-terminated buffer, copied before the next host call
        unsafe { CStr::from_ptr(sys::ee_uicpp_button_text(self.0.button)) }.to_bytes_with_nul().to_vec()
    }
    fn text_extent(&self) -> (f32, f32) {
        let (mut w, mut h) = (0.0, 0.0);
        unsafe { sys::ee_uicpp_button_text_extent(self.0.button, &mut w, &mut h) };
        (w, h)
    }
    fn copy_text(&mut self, focused: bool) {
        unsafe { sys::ee_uicpp_button_copy_text(self.0.button, focused) }
    }
}

impl ScrollButtonHost for Raw<'_> {
    fn decrement(&self) -> c_ulong {
        unsafe { (self.0.sub as *const c_ulong).read() }
    }
    fn delay_running(&self) -> bool {
        unsafe { sys::ee_uicpp_scroll_button_delay_running(self.0.button) }
    }
    fn delay_poll(&mut self) -> bool {
        unsafe { sys::ee_uicpp_scroll_button_delay_poll(self.0.button) }
    }
    fn delay_start(&mut self) {
        unsafe { sys::ee_uicpp_scroll_button_delay_start(self.0.button) }
    }
    fn delay_stop(&mut self) {
        unsafe { sys::ee_uicpp_scroll_button_delay_stop(self.0.button) }
    }
    fn repeat_running(&self) -> bool {
        unsafe { sys::ee_uicpp_scroll_button_repeat_running(self.0.button) }
    }
    fn repeat_poll(&mut self) -> bool {
        unsafe { sys::ee_uicpp_scroll_button_repeat_poll(self.0.button) }
    }
    fn repeat_start(&mut self) {
        unsafe { sys::ee_uicpp_scroll_button_repeat_start(self.0.button) }
    }
    fn slider_value(&self) -> c_long {
        unsafe { sys::ee_uicpp_scroll_button_slider_value(self.0.button) }
    }
    fn set_slider_value(&mut self, value: c_long) {
        unsafe { sys::ee_uicpp_scroll_button_set_slider_value(self.0.button, value) }
    }
}

impl LabelHost for Raw<'_> {
    fn label_set_text(&mut self) {
        unsafe { sys::ee_uicpp_label_set_text(self.0.button, self.0.ctx) }
    }
    fn label_create(&mut self, rect: UiRect) {
        unsafe { sys::ee_uicpp_label_create(self.0.button, self.0.ctx, &rect) }
    }
    fn label_set_location(&mut self, at: UiPoint) {
        unsafe { sys::ee_uicpp_label_set_location(self.0.button, self.0.ctx, at.x, at.y) }
    }
}

impl CheckHost for Raw<'_> {
    fn load_check(&self) -> CheckFields {
        unsafe { (self.0.sub as *const CheckFields).read() }
    }
    fn store_check(&mut self, c: &CheckFields) {
        unsafe { (self.0.sub as *mut CheckFields).write(*c) }
    }
    fn set_check_bitmaps(&mut self, on: bool) {
        unsafe { sys::ee_uicpp_check_set_bitmaps(self.0.button, on) }
    }
}

impl RadioHost for Raw<'_> {
    fn load_radio(&self) -> RadioFields {
        unsafe { (self.0.sub as *const RadioFields).read() }
    }
    fn store_radio(&mut self, r: &RadioFields) {
        unsafe { (self.0.sub as *mut RadioFields).write(*r) }
    }
    fn swap_bitmaps(&mut self) {
        unsafe { sys::ee_uicpp_radio_swap_bitmaps(self.0.button) }
    }
    fn control_count(&self) -> c_long {
        unsafe { sys::ee_uicpp_radio_control_count(self.0.button) }
    }
    fn is_radio(&self, i: c_long) -> bool {
        unsafe { sys::ee_uicpp_radio_is_radio(self.0.button, i) }
    }
    fn radio_checked(&self, i: c_long) -> bool {
        unsafe { sys::ee_uicpp_radio_checked(self.0.button, i) }
    }
    fn uncheck_radio(&mut self, i: c_long) {
        unsafe { sys::ee_uicpp_radio_uncheck(self.0.button, i) }
    }
}

unsafe fn raw<'a>(refs: *const ButtonRefs) -> Raw<'a> {
    Raw(&*refs)
}

unsafe fn text<'a>(s: *const c_char) -> &'a [u8] {
    CStr::from_ptr(s).to_bytes_with_nul()
}

/// The constructor's part after `UIControl`'s ([`button::init`]).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_init(fields: *mut ButtonFields, rect: *const UiRect, border: c_ulong) {
    // written whole: the members are not initialised yet
    fields.write(button::init(&rect.read(), border));
}

/// `UIButton::ButtonDown`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_button_down(refs: *const ButtonRefs) {
    button::button_down(&mut raw(refs));
}

/// `UIButton::ButtonUp`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_button_up(refs: *const ButtonRefs) {
    button::button_up(&mut raw(refs));
}

/// `UIButton::MouseOver`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_mouse_over(refs: *const ButtonRefs) {
    button::mouse_over(&mut raw(refs));
}

/// `UIButton::MouseLButtonDown`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_mouse_l_button_down(refs: *const ButtonRefs) -> c_long {
    button::mouse_l_button_down(&mut raw(refs))
}

/// `UIButton::MouseLButtonUp`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_mouse_l_button_up(refs: *const ButtonRefs) -> c_long {
    button::mouse_l_button_up(&mut raw(refs))
}

/// `UIButton::MouseMove`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_mouse_move(refs: *const ButtonRefs, at: UiPoint) -> c_long {
    button::mouse_move(&mut raw(refs), at)
}

/// `UIButton::TabbedTo`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_tabbed_to(refs: *const ButtonRefs) {
    button::tabbed_to(&mut raw(refs));
}

/// `UIButton::TabbedFrom`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_tabbed_from(refs: *const ButtonRefs) {
    button::tabbed_from(&mut raw(refs));
}

/// `UIButton::UpdateKeyboard`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_update_keyboard(refs: *const ButtonRefs) -> c_long {
    button::update_keyboard(&mut raw(refs))
}

/// `UIButton::SetLocation`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_set_location(refs: *const ButtonRefs, at: UiPoint) {
    button::set_location(&mut raw(refs), at);
}

/// `UIButton::CopyData`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_copy_data(refs: *const ButtonRefs) -> c_long {
    button::copy_data(&mut raw(refs))
}

/// `UIDualButton::MouseRButtonDown`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_dual_button_mouse_r_button_down(refs: *const ButtonRefs) -> c_long {
    button::dual_mouse_r_button_down(&mut raw(refs))
}

/// `UIDualButton::MouseRButtonUp`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_dual_button_mouse_r_button_up(refs: *const ButtonRefs) -> c_long {
    button::dual_mouse_r_button_up(&mut raw(refs))
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

/// `UICheckBox::SetChecked` (`sub`: the check box's record).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_check_set_checked(refs: *const ButtonRefs, checked: bool) {
    button::check_set_checked(&mut raw(refs), checked);
}

/// `UICheckBox::ToggleCheck`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_check_toggle(refs: *const ButtonRefs) {
    button::check_toggle(&mut raw(refs));
}

/// `UICheckBox::SetDisabled`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_check_set_disabled(refs: *const ButtonRefs, disabled: bool) {
    button::check_set_disabled(&mut raw(refs), disabled);
}

/// `UICheckBox::SetLocation` (`ctx`: the label, as for `SetText`).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_check_set_location(refs: *const ButtonRefs, at: UiPoint) {
    button::check_set_location(&mut raw(refs), at);
}

/// `UICheckBox::SetText` (`ctx`: the C++ text and font for the label host calls).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_check_set_text(refs: *const ButtonRefs, s: *const c_char) {
    button::check_set_text(&mut raw(refs), text(s));
}

/// `UICheckBox::ButtonDown` and the mouse handlers: whether the enabled box goes on to `UIButton`'s.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_check_enabled(refs: *const ButtonRefs) -> bool {
    button::check_enabled(&raw(refs))
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
