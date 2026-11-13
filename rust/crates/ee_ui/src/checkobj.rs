//! The check box (`UICheckBox`) and the radio button (`UIRadioButton`) whole in the Rust button (R6 ui round 12):
//! their records ([`CheckFields`]: the label, checked, has-text, scaled-bitmap and disabled flags; [`RadioFields`]:
//! the other check state's two bitmaps, the label and the flags) are the button object's [`Sub`], and every method of
//! `UICheckBox.cpp`, `UIRadioButton.cpp` and their headers is here. The check and radio logic is round 3's
//! (`button.rs` `check_*`, `radio_*`, pinned by its goldens); this module is its [`CheckHost`], [`RadioHost`] and
//! [`LabelHost`] over ops.
//!
//! The radio walk (`RemoveOtherChecks`) reads each sibling's Rust button through [`Op::RadioOf`] (the original's
//! `GetForm()->GetControl(i)` with its `GetType()` test, a pure read as WON's `BtnOf`) and its checked flag directly,
//! as the original read `mIsChecked`; the sibling's `UnCheck` is virtual (the editor's buttons override it) and goes
//! through [`Op::RadioUnCheckAt`]. `ButtonUp` calls the virtual `Check` through [`Op::RadioCheck`].
//!
//! The label is a part (a C++ `UITextLabel` held by address, as the scroll bar holds its slider): made by
//! [`Op::LabelCreate`], its text set by [`Op::LabelSetText`], moved, measured, copied, drawn and deleted through the
//! part ops. The manager's check bitmaps are [`Op::ManagerBitmap`] reads; the box's bitmap scale is read and set
//! through [`Op::BitmapScale`] and [`Op::BitmapSetScale`].

use core::ffi::{c_long, c_ulong, c_void};

use crate::button::{self, CheckFields, CheckHost, LabelHost, RadioFields, RadioHost, SCROLL_BAR_WIDTH};
use crate::buttonobj::{Btn, EeUiButton, EeUiButtonCall, Sub, Which};
use crate::draw;
use crate::obj::EeUiOp as Op;
use crate::rect::{UiPoint, UiRect};

/// The manager's check bitmaps ([`Op::ManagerBitmap`] kinds).
const CHECK_BITMAP_OFF: isize = 8;
const CHECK_BITMAP_ON: isize = 9;
const CHECK_BITMAP_DOWN_OFF: isize = 10;
const CHECK_BITMAP_DOWN_ON: isize = 11;
/// The manager's radio bitmaps of the checked state (the other state's at construction).
const RADIO_BITMAP_ON: isize = 15;
const RADIO_BITMAP_DOWN_ON: isize = 17;

/// A record for a button that has none (a check call on another class: never made by the C++).
const NO_CHECK: CheckFields = CheckFields {
    text_label: core::ptr::null_mut(),
    is_checked: false,
    has_text: false,
    scale_bitmap: false,
    is_disabled: false,
};

/// The radio button's record for a button that has none.
const NO_RADIO: RadioFields = RadioFields {
    other_normal_bitmap: core::ptr::null_mut(),
    other_pressed_bitmap: core::ptr::null_mut(),
    text_label: core::ptr::null_mut(),
    is_checked: false,
    has_text: false,
    is_push_button: false,
    scale_bitmap: false,
};

/// A string's emptiness as the bytes the round-3 logic tests (`text::is_empty`).
fn text_bytes(o: &EeUiButton, text: isize) -> &'static [u8] {
    if o.op(Op::StringLength, text, 0) == 0 {
        &[0]
    } else {
        &[1, 0]
    }
}

impl EeUiButton {
    /// The subclass's label, if it has one.
    pub(crate) fn label(&self) -> *mut c_void {
        match *self.sub.borrow() {
            Sub::Check(c) => c.text_label,
            Sub::Radio(r) => r.text_label,
            Sub::Plain => core::ptr::null_mut(),
        }
    }

    fn set_label(&self, label: *mut c_void) {
        match &mut *self.sub.borrow_mut() {
            Sub::Check(c) => c.text_label = label,
            Sub::Radio(r) => r.text_label = label,
            Sub::Plain => {}
        }
    }

    fn check(&self) -> CheckFields {
        match *self.sub.borrow() {
            Sub::Check(c) => c,
            _ => NO_CHECK,
        }
    }

    pub(crate) fn radio(&self) -> RadioFields {
        match *self.sub.borrow() {
            Sub::Radio(r) => r,
            _ => NO_RADIO,
        }
    }

    fn store_radio(&self, r: &RadioFields) {
        if let Sub::Radio(x) = &mut *self.sub.borrow_mut() {
            *x = *r;
        }
    }

    /// The subclass's destructor part (it ran before `UIButton`'s): the label deleted; the radio button's other
    /// state's bitmaps released.
    pub(crate) fn release_sub(&self) {
        let label = self.label();
        if !label.is_null() {
            self.op(Op::DeletePart, label as isize, 0);
        }
        if let Sub::Radio(r) = *self.sub.borrow() {
            self.release_bitmap(r.other_normal_bitmap);
            self.release_bitmap(r.other_pressed_bitmap);
        }
    }

    fn reference(&self, bitmap: *mut c_void) {
        if !bitmap.is_null() {
            self.op(Op::BitmapReference, bitmap as isize, 0);
        }
    }

    fn release_bitmap(&self, bitmap: *mut c_void) {
        if !bitmap.is_null() {
            self.op(Op::BitmapRelease, bitmap as isize, 0);
        }
    }

    /// `UIRadioButton`'s constructor after `UIButton`'s: the record, the checked state's bitmaps held as the other
    /// state's (swapped in when checked), the bitmap offset in a box of the constructor's `height`, then `SetText`.
    pub fn radio_init(&self, text: isize, font: isize, height: f32, push: bool, checked: bool, scale_bitmap: bool) {
        let mut r = RadioFields { is_checked: checked, is_push_button: push, scale_bitmap, ..NO_RADIO };
        // the original referenced them without a NULL test
        r.other_normal_bitmap = self.op(Op::ManagerBitmap, RADIO_BITMAP_ON, 0) as *mut c_void;
        self.op(Op::BitmapReference, r.other_normal_bitmap as isize, 0);
        r.other_pressed_bitmap = self.op(Op::ManagerBitmap, RADIO_BITMAP_DOWN_ON, 0) as *mut c_void;
        self.op(Op::BitmapReference, r.other_pressed_bitmap as isize, 0);
        *self.sub.borrow_mut() = Sub::Radio(r);
        if checked {
            let f = self.fields();
            let (normal, pressed) = (f.normal_bitmap, f.pressed_bitmap);
            self.reference(normal);
            self.reference(pressed);
            self.set_bitmap(Which::Normal, r.other_normal_bitmap, true);
            self.set_bitmap(Which::Pressed, r.other_pressed_bitmap, true);
            self.op(Op::BitmapRelease, r.other_normal_bitmap as isize, 0);
            r.other_normal_bitmap = normal;
            self.op(Op::BitmapReference, normal as isize, 0);
            self.op(Op::BitmapRelease, r.other_pressed_bitmap as isize, 0);
            r.other_pressed_bitmap = pressed;
            self.op(Op::BitmapReference, pressed as isize, 0);
            self.store_radio(&r);
            self.release_bitmap(normal);
            self.release_bitmap(pressed);
        }
        let border = self.op(Op::BorderThickness, 0, 0) as c_ulong;
        if let Some(p) = button::label_bitmap_offset(height, border, scale_bitmap, false) {
            self.f.borrow_mut().bitmap_draw_offset = p;
        }
        button::radio_set_text(&mut self.btn_text(text, font), text_bytes(self, text));
    }

    /// `Check`'s and `UnCheck`'s swap: the shown normal and pressed bitmaps trade places with the other state's
    /// (`SetNormalBitmap`/`SetPressedBitmap` dirty the button), then `SetDirty(true)`.
    fn radio_swap(&self) {
        let f = self.fields();
        let (normal, pressed) = (f.normal_bitmap, f.pressed_bitmap);
        self.reference(normal);
        self.reference(pressed);
        let mut r = self.radio();
        self.set_bitmap(Which::Normal, r.other_normal_bitmap, true);
        self.set_bitmap(Which::Pressed, r.other_pressed_bitmap, true);
        self.release_bitmap(r.other_normal_bitmap);
        r.other_normal_bitmap = normal;
        self.reference(normal);
        self.release_bitmap(r.other_pressed_bitmap);
        r.other_pressed_bitmap = pressed;
        self.reference(pressed);
        self.store_radio(&r);
        self.op(Op::SetDirty, 1, 0);
        self.release_bitmap(normal);
        self.release_bitmap(pressed);
    }

    /// The four bitmap setters: the shown bitmap replaced (`ReplaceNormalBitmap`/`ReplacePressedBitmap`, no
    /// `SetDirty`) when the setter's state is the button's, else the other state's.
    fn radio_set_bitmap(&self, pressed: bool, checked_state: bool, bitmap: *mut c_void) {
        let mut r = self.radio();
        if r.is_checked == checked_state {
            self.set_bitmap(if pressed { Which::Pressed } else { Which::Normal }, bitmap, false);
            return;
        }
        self.release_bitmap(if pressed { r.other_pressed_bitmap } else { r.other_normal_bitmap });
        if pressed {
            r.other_pressed_bitmap = bitmap;
        } else {
            r.other_normal_bitmap = bitmap;
        }
        self.store_radio(&r);
        self.reference(bitmap);
    }

    /// The Rust button of the form's control `i`, if it is a radio button.
    fn radio_of(&self, i: c_long) -> Option<&EeUiButton> {
        let p = self.op(Op::RadioOf, i as isize, 0) as *const EeUiButton;
        // SAFETY: a radio button's Rust button lives as long as its C++ object, which its form holds
        (!p.is_null()).then(|| unsafe { &*p })
    }

    /// A `Btn` carrying a `SetText`'s text and font.
    fn btn_text(&self, text: isize, font: isize) -> Btn<'_> {
        let mut h = Btn::new(self);
        h.text = text;
        h.font = font;
        h
    }

    /// `UICheckBox`'s constructor after `UIButton`'s: the record, the bitmap offset in a box of the constructor's
    /// `height`, then `SetText`.
    pub fn check_init(&self, text: isize, font: isize, height: f32, scale_bitmap: bool) {
        *self.sub.borrow_mut() = Sub::Check(CheckFields { scale_bitmap, ..NO_CHECK });
        let border = self.op(Op::BorderThickness, 0, 0) as c_ulong;
        if let Some(p) = button::label_bitmap_offset(height, border, scale_bitmap, true) {
            self.f.borrow_mut().bitmap_draw_offset = p;
        }
        self.check_set_text(text, font);
    }

    /// `SetText`: the label's text, or a new label beside the box; empty text hides the label.
    pub fn check_set_text(&self, text: isize, font: isize) {
        button::check_set_text(&mut self.btn_text(text, font), text_bytes(self, text));
    }

    /// `ChangeWidth` (the check box's and the tri-state box's, one body in the original): the label shrunk to fit
    /// beside the box, then the box's own width.
    pub fn change_width(&self, width: c_ulong) {
        let label = self.label();
        if !label.is_null() {
            let mut r = self.host.get_rect(Op::PartRect, label as isize);
            r.right = (r.left + width as f32) - SCROLL_BAR_WIDTH;
            self.host.op_rect(Op::PartSetRectangle, label as isize, 0, r);
        }
        let mut r = self.host.rect();
        r.right = (r.left + width as f32) - 1.0;
        self.host.op_rect(Op::BaseSetRectangle, 0, 0, r);
    }

    /// The label's part of `CopyData` after the button's (`result`): its copy, or its deletion once the text was
    /// cleared.
    fn label_copy_data(&self, result: isize, has_text: bool) -> isize {
        let label = self.label();
        if has_text {
            self.op(Op::PartCopyData, label as isize, 0)
        } else {
            if !label.is_null() {
                self.op(Op::DeletePart, label as isize, 0);
                self.set_label(core::ptr::null_mut());
            }
            result
        }
    }

    /// `DrawPolygons` of the check box and radio button: the box bitmap scaled to the copied height (less twice the
    /// border: `border`) while drawn, then the label.
    fn box_draw_polygons(&self, scale_bitmap: bool, border: Option<c_ulong>) -> isize {
        let bitmap = self.fields().copied_bitmap;
        let r = if scale_bitmap && !bitmap.is_null() {
            let full = self.host.get_point(Op::BitmapScale, bitmap as isize);
            let height = self.host.get_rect(Op::CopiedDrawRect, 0).height();
            let (x, y) = draw::box_bitmap_scale(full.x, full.y, height, border);
            self.host.op_f(Op::BitmapSetScale, bitmap as isize, 0, 0, 0, &mut [x, y, 0.0, 0.0]);
            let r = self.draw_polygons();
            self.host.op_f(Op::BitmapSetScale, bitmap as isize, 0, 0, 0, &mut [full.x, full.y, 0.0, 0.0]);
            r
        } else {
            self.draw_polygons()
        };
        let label = self.label();
        if label.is_null() {
            r
        } else {
            self.op(Op::PartDrawPolygons, label as isize, 0)
        }
    }

    /// `DrawTextA` of the check box (`base`: the control's text) and radio button (the button's), then the label.
    fn label_draw_text(&self, base: isize) -> isize {
        let label = self.label();
        if label.is_null() {
            base
        } else {
            self.op(Op::PartDrawText, label as isize, 0)
        }
    }

    /// The subclasses' inward calls ([`EeUiButtonCall`] from 40 on).
    pub(crate) fn call_sub(&self, call: EeUiButtonCall, a: isize, b: isize, buf: [f32; 4]) -> isize {
        type C = EeUiButtonCall;
        let at = UiPoint { x: buf[0], y: buf[1] };
        let mut h = Btn::new(self);
        match call {
            C::CheckInit => self.check_init(a, b, buf[0], buf[1] != 0.0),
            C::CheckChangeWidth => self.change_width(a as c_ulong),
            C::CheckCopyData => {
                let r = button::copy_data(&mut h) as isize;
                return self.label_copy_data(r, self.check().has_text);
            }
            C::CheckDrawPolygons => {
                let border = self.op(Op::BorderThickness, 0, 0) as c_ulong;
                return self.box_draw_polygons(self.check().scale_bitmap, Some(border));
            }
            C::CheckDrawText => {
                let r = self.op(Op::BaseDrawText, 0, 0);
                return self.label_draw_text(r);
            }
            C::CheckSetChecked => button::check_set_checked(&mut h, a != 0),
            C::CheckSetDisabled => button::check_set_disabled(&mut h, a != 0),
            C::CheckSetLocation => button::check_set_location(&mut h, at),
            C::CheckSetText => self.check_set_text(a, b),
            C::CheckToggle => button::check_toggle(&mut h),
            C::CheckButtonUp => {
                button::check_toggle(&mut h);
                button::button_up(&mut h);
            }
            C::CheckButtonDown => {
                if button::check_enabled(&h) {
                    button::button_down(&mut h);
                }
            }
            C::CheckMouseLButtonDown => {
                if button::check_enabled(&h) {
                    return button::mouse_l_button_down(&mut h) as isize;
                }
            }
            C::CheckMouseLButtonUp => {
                if button::check_enabled(&h) {
                    return button::mouse_l_button_up(&mut h) as isize;
                }
            }
            C::CheckMouseMove => {
                if button::check_enabled(&h) {
                    return button::mouse_move(&mut h, at) as isize;
                }
            }
            C::CheckIsChecked => return self.check().is_checked as isize,
            C::TextLabel => return self.label() as isize,
            C::RadioInit => self.radio_init(a, b, buf[0], buf[1] != 0.0, buf[2] != 0.0, buf[3] != 0.0),
            C::RadioCheck => button::radio_check(&mut h),
            C::RadioUnCheck => button::radio_uncheck(&mut h),
            C::RadioCopyData => {
                let r = button::radio_copy_data(&mut h) as isize;
                return self.label_copy_data(r, self.radio().has_text);
            }
            C::RadioDrawPolygons => return self.box_draw_polygons(self.radio().scale_bitmap, None),
            C::RadioDrawText => {
                let r = self.draw_text();
                return self.label_draw_text(r);
            }
            C::RadioIsChecked => return self.radio().is_checked as isize,
            C::RadioTabable => return !self.radio().is_push_button as isize,
            C::RadioSetDownChecked => self.radio_set_bitmap(true, true, a as *mut c_void),
            C::RadioSetDownUnchecked => self.radio_set_bitmap(true, false, a as *mut c_void),
            C::RadioSetUpChecked => self.radio_set_bitmap(false, true, a as *mut c_void),
            C::RadioSetUpUnchecked => self.radio_set_bitmap(false, false, a as *mut c_void),
            C::RadioSetLocation => button::radio_set_location(&mut h, at),
            C::RadioSetText => button::radio_set_text(&mut self.btn_text(a, b), text_bytes(self, a)),
            C::RadioButtonUp => {
                self.op(Op::RadioCheck, 0, 0);
                button::button_up(&mut h);
            }
            C::RadioMouseLButtonDown => return button::radio_mouse_l_button_down(&mut h) as isize,
            _ => {}
        }
        0
    }
}

impl LabelHost for Btn<'_> {
    fn label_set_text(&mut self) {
        self.o.op(Op::LabelSetText, self.o.label() as isize, self.text);
    }
    fn label_create(&mut self, rect: UiRect) {
        let label = self.o.host.op_rect(Op::LabelCreate, self.text, self.font, rect) as *mut c_void;
        self.o.set_label(label);
    }
    fn label_set_location(&mut self, at: UiPoint) {
        self.o.host.op_point(Op::PartSetLocation, self.o.label() as isize, at);
    }
}

impl CheckHost for Btn<'_> {
    fn load_check(&self) -> CheckFields {
        self.o.check()
    }
    fn store_check(&mut self, c: &CheckFields) {
        if let Sub::Check(r) = &mut *self.o.sub.borrow_mut() {
            *r = *c;
        }
    }
    fn set_check_bitmaps(&mut self, on: bool) {
        let normal = if on { CHECK_BITMAP_ON } else { CHECK_BITMAP_OFF };
        let pressed = if on { CHECK_BITMAP_DOWN_ON } else { CHECK_BITMAP_DOWN_OFF };
        let normal = self.o.op(Op::ManagerBitmap, normal, 0) as *mut c_void;
        self.o.set_bitmap(Which::Normal, normal, true);
        let pressed = self.o.op(Op::ManagerBitmap, pressed, 0) as *mut c_void;
        self.o.set_bitmap(Which::Pressed, pressed, true);
    }
}

impl RadioHost for Btn<'_> {
    fn load_radio(&self) -> RadioFields {
        self.o.radio()
    }
    fn store_radio(&mut self, r: &RadioFields) {
        self.o.store_radio(r);
    }
    fn swap_bitmaps(&mut self) {
        self.o.radio_swap();
    }
    fn control_count(&self) -> c_long {
        self.o.op(Op::ControlCount, 0, 0) as c_long
    }
    fn is_radio(&self, i: c_long) -> bool {
        self.o.radio_of(i).is_some()
    }
    fn radio_checked(&self, i: c_long) -> bool {
        self.o.radio_of(i).is_some_and(|b| b.radio().is_checked)
    }
    fn uncheck_radio(&mut self, i: c_long) {
        self.o.op(Op::RadioUnCheckAt, i as isize, 0);
    }
}

#[cfg(test)]
#[path = "checkobj_tests.rs"]
mod tests;
