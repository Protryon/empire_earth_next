//! `extern "C"` for the blinking button and the scalable button (`UIBlinkingButton.cpp`, `UIScalableButton.cpp`):
//! pure, the C++ applies what they return. (The tri-state box is whole in the Rust button since R6 ui round 12,
//! `checkobj.rs`.)
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_ulong;

use crate::button_variants::{self, BlinkFields, BlinkShow, Slice};
use crate::rect::UiRect;

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
