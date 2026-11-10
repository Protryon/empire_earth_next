//! `extern "C"` for the tri-state check box, the blinking button and the scalable button (`UITriStateCheckBox.cpp`,
//! `UIBlinkingButton.cpp`, `UIScalableButton.cpp`). The tri-state box's host calls go back through
//! `ee_uicpp_tristate_*` (UITriStateCheckBox.cpp); `refs` points at its record and the `UIControl` members the logic
//! reads. The other two are pure: the C++ applies what they return.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_long, c_ulong, c_void};

use crate::button_variants::{self, BlinkFields, BlinkShow, Slice, TriStateFields, TriStateHost};
use crate::rect::{UiPoint, UiRect};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_ulong, c_void};

    use crate::rect::UiRect;

    extern "C" {
        pub fn ee_uicpp_tristate_label_rect(b: *mut c_void) -> UiRect;
        pub fn ee_uicpp_tristate_label_set_rect(b: *mut c_void, rect: *const UiRect);
        pub fn ee_uicpp_tristate_label_set_location(b: *mut c_void, x: f32, y: f32);
        pub fn ee_uicpp_tristate_label_set_text(b: *mut c_void, ctx: *mut c_void);
        pub fn ee_uicpp_tristate_label_create(b: *mut c_void, ctx: *mut c_void, rect: *const UiRect);
        pub fn ee_uicpp_tristate_label_delete(b: *mut c_void);
        pub fn ee_uicpp_tristate_base_set_rectangle(b: *mut c_void, rect: *const UiRect);
        pub fn ee_uicpp_tristate_base_set_location(b: *mut c_void, x: f32, y: f32);
        pub fn ee_uicpp_tristate_set_dirty(b: *mut c_void);
        pub fn ee_uicpp_tristate_set_check_bitmaps(b: *mut c_void, state: c_ulong);
        pub fn ee_uicpp_tristate_fire(b: *mut c_void, event: c_ulong);
        // 0: UIButton::CopyData, 1: mTextLabel->CopyData
        pub fn ee_uicpp_tristate_copy_data(b: *mut c_void, which: c_int) -> c_long;
    }
}

/// Where a `UITriStateCheckBox` keeps what the Rust side reads and writes.
#[repr(C)]
pub struct TriStateRefs {
    /// The `UITriStateCheckBox*`.
    pub button: *mut c_void,
    /// Its record from `mTextLabel` on.
    pub fields: *mut TriStateFields,
    /// `mRectangle`.
    pub rect: *const UiRect,
    /// `mBorderThickness`.
    pub border: *const c_ulong,
    /// A `SetText`'s text and font for the label host calls (`UIButtonRust::Label`), else NULL.
    pub ctx: *mut c_void,
}

struct Raw(*const TriStateRefs);

impl Raw {
    fn r(&self) -> &TriStateRefs {
        unsafe { &*self.0 }
    }
}

impl TriStateHost for Raw {
    fn load(&self) -> TriStateFields {
        unsafe { self.r().fields.read() }
    }
    fn store(&mut self, f: &TriStateFields) {
        unsafe { self.r().fields.write(*f) }
    }
    fn rect(&self) -> UiRect {
        unsafe { self.r().rect.read() }
    }
    fn border(&self) -> c_ulong {
        unsafe { self.r().border.read() }
    }
    fn label_rect(&self) -> UiRect {
        unsafe { sys::ee_uicpp_tristate_label_rect(self.r().button) }
    }
    fn label_set_rect(&mut self, rect: UiRect) {
        unsafe { sys::ee_uicpp_tristate_label_set_rect(self.r().button, &rect) }
    }
    fn label_set_location(&mut self, at: UiPoint) {
        unsafe { sys::ee_uicpp_tristate_label_set_location(self.r().button, at.x, at.y) }
    }
    fn label_set_text(&mut self) {
        unsafe { sys::ee_uicpp_tristate_label_set_text(self.r().button, self.r().ctx) }
    }
    fn label_create(&mut self, rect: UiRect) {
        unsafe { sys::ee_uicpp_tristate_label_create(self.r().button, self.r().ctx, &rect) }
    }
    fn label_delete(&mut self) {
        unsafe { sys::ee_uicpp_tristate_label_delete(self.r().button) }
    }
    fn base_set_rectangle(&mut self, rect: UiRect) {
        unsafe { sys::ee_uicpp_tristate_base_set_rectangle(self.r().button, &rect) }
    }
    fn base_set_location(&mut self, at: UiPoint) {
        unsafe { sys::ee_uicpp_tristate_base_set_location(self.r().button, at.x, at.y) }
    }
    fn set_dirty(&mut self) {
        unsafe { sys::ee_uicpp_tristate_set_dirty(self.r().button) }
    }
    fn set_check_bitmaps(&mut self, state: c_ulong) {
        unsafe { sys::ee_uicpp_tristate_set_check_bitmaps(self.r().button, state) }
    }
    fn fire(&mut self, event: c_ulong) {
        unsafe { sys::ee_uicpp_tristate_fire(self.r().button, event) }
    }
    fn base_copy_data(&mut self) -> c_long {
        unsafe { sys::ee_uicpp_tristate_copy_data(self.r().button, 0) }
    }
    fn label_copy_data(&mut self) -> c_long {
        unsafe { sys::ee_uicpp_tristate_copy_data(self.r().button, 1) }
    }
}

/// The text's bytes with the terminator.
unsafe fn text(s: *const c_char) -> &'static [u8] {
    let n = core::ffi::CStr::from_ptr(s).to_bytes().len();
    core::slice::from_raw_parts(s as *const u8, n + 1)
}

/// The constructor's `UIButton` rectangle.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_tristate_rect(rect: *const UiRect) -> UiRect {
    button_variants::tri_state_rect(&*rect)
}

/// The constructor's bitmap offset: true and `out` written, or false to keep it.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_tristate_bitmap_offset(height: f32, border: c_ulong, out: *mut UiPoint) -> bool {
    match button_variants::tri_state_bitmap_offset(height, border) {
        Some(p) => {
            out.write(p);
            true
        }
        None => false,
    }
}

/// `UITriStateCheckBox::ChangeWidth`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_tristate_change_width(refs: *const TriStateRefs, width: c_ulong) {
    button_variants::tri_state_change_width(&mut Raw(refs), width);
}

/// `UITriStateCheckBox::CopyData`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_tristate_copy_data(refs: *const TriStateRefs) -> c_long {
    button_variants::tri_state_copy_data(&mut Raw(refs))
}

/// `UITriStateCheckBox::SetCheckState`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_tristate_set_check_state(refs: *const TriStateRefs, state: c_ulong) {
    button_variants::tri_state_set_check_state(&mut Raw(refs), state);
}

/// `UITriStateCheckBox::SetLocation`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_tristate_set_location(refs: *const TriStateRefs, at: UiPoint) {
    button_variants::tri_state_set_location(&mut Raw(refs), at);
}

/// `UITriStateCheckBox::SetText` (`ctx` in `refs`: the C++ text and font for the label host calls).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_tristate_set_text(refs: *const TriStateRefs, s: *const c_char) {
    button_variants::tri_state_set_text(&mut Raw(refs), text(s));
}

/// `UITriStateCheckBox::CycleCheck`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_tristate_cycle(refs: *const TriStateRefs) {
    button_variants::tri_state_cycle(&mut Raw(refs));
}

/// `UIBlinkingButton`'s constructor: the record after `UIButton`'s (written whole).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_blink_init(fields: *mut BlinkFields, rate: c_ulong, always_dirty: bool) {
    fields.write(button_variants::blink_init(rate, always_dirty));
}

/// `UIBlinkingButton::CopyData`'s fade: the opacities to set (the record updated).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_blink_copy_data(
    fields: *mut BlinkFields,
    polled: bool,
    time_left: c_ulong,
    state: c_ulong,
    bg: f32,
    border: f32,
) -> BlinkShow {
    let mut f = fields.read();
    let show = button_variants::blink_copy_data(&mut f, polled, time_left, state, bg, border);
    fields.write(f);
    show
}

/// `UIScalableButton::CopyData`'s source rectangle for the state.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scalable_copied_rect(
    state: c_ulong,
    has_focused_bitmap: bool,
    normal: *const UiRect,
    focused: *const UiRect,
    pressed: *const UiRect,
    current: *const UiRect,
) -> UiRect {
    button_variants::scalable_copied_rect(state, has_focused_bitmap, &*normal, &*focused, &*pressed, &*current)
}

/// `UIScalableButton::DrawPolygons`' three slices, written to `out[0..3]`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scalable_slices(
    copied: *const UiRect,
    draw: *const UiRect,
    texture_width: c_ulong,
    out: *mut Slice,
) {
    let slices = button_variants::scalable_slices(&*copied, &*draw, texture_width);
    for (i, s) in slices.iter().enumerate() {
        out.add(i).write(*s);
    }
}
