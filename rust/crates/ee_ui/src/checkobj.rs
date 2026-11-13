//! The check box (`UICheckBox`) whole in the Rust button (R6 ui round 12): its record ([`CheckFields`]: the label,
//! checked, has-text, scaled-bitmap and disabled flags) is the button object's [`Sub`], and every method of
//! `UICheckBox.cpp` and the header is here. The check logic is round 3's (`button.rs` `check_*`, pinned by its
//! goldens); this module is its [`CheckHost`] and [`LabelHost`] over ops.
//!
//! The label is a part (a C++ `UITextLabel` held by address, as the scroll bar holds its slider): made by
//! [`Op::LabelCreate`], its text set by [`Op::LabelSetText`], moved, measured, copied, drawn and deleted through the
//! part ops. The manager's check bitmaps are [`Op::ManagerBitmap`] reads; the box's bitmap scale is read and set
//! through [`Op::BitmapScale`] and [`Op::BitmapSetScale`].

use core::ffi::{c_ulong, c_void};

use crate::button::{self, CheckFields, CheckHost, LabelHost, SCROLL_BAR_WIDTH};
use crate::buttonobj::{Btn, EeUiButton, EeUiButtonCall, Sub, Which};
use crate::draw;
use crate::obj::EeUiOp as Op;
use crate::rect::{UiPoint, UiRect};

/// The manager's check bitmaps ([`Op::ManagerBitmap`] kinds).
const CHECK_BITMAP_OFF: isize = 8;
const CHECK_BITMAP_ON: isize = 9;
const CHECK_BITMAP_DOWN_OFF: isize = 10;
const CHECK_BITMAP_DOWN_ON: isize = 11;

/// A record for a button that has none (a check call on another class: never made by the C++).
const NO_CHECK: CheckFields = CheckFields {
    text_label: core::ptr::null_mut(),
    is_checked: false,
    has_text: false,
    scale_bitmap: false,
    is_disabled: false,
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
            Sub::Plain => core::ptr::null_mut(),
        }
    }

    fn set_label(&self, label: *mut c_void) {
        if let Sub::Check(c) = &mut *self.sub.borrow_mut() {
            c.text_label = label;
        }
    }

    fn check(&self) -> CheckFields {
        match *self.sub.borrow() {
            Sub::Check(c) => c,
            _ => NO_CHECK,
        }
    }

    /// The subclass's destructor part (it ran before `UIButton`'s): the check box deletes its label.
    pub(crate) fn release_sub(&self) {
        let label = self.label();
        if !label.is_null() {
            self.op(Op::DeletePart, label as isize, 0);
        }
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
            _ => {}
        }
        0
    }
}

impl LabelHost for Btn<'_> {
    fn label_set_text(&mut self) {
        #[cfg(not(test))]
        if !self.sub.is_null() {
            return crate::button_ffi::cpp_label_set_text(self);
        }
        self.o.op(Op::LabelSetText, self.o.label() as isize, self.text);
    }
    fn label_create(&mut self, rect: UiRect) {
        #[cfg(not(test))]
        if !self.sub.is_null() {
            return crate::button_ffi::cpp_label_create(self, rect);
        }
        let label = self.o.host.op_rect(Op::LabelCreate, self.text, self.font, rect) as *mut c_void;
        self.o.set_label(label);
    }
    fn label_set_location(&mut self, at: UiPoint) {
        #[cfg(not(test))]
        if !self.sub.is_null() {
            return crate::button_ffi::cpp_label_set_location(self, at);
        }
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

#[cfg(test)]
#[path = "checkobj_tests.rs"]
mod tests;
