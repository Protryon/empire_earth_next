//! `extern "C"` for the scalable button (`UIScalableButton.cpp`): pure, the C++ applies what they return. (The
//! tri-state box and the blinking button are whole in the Rust button since R6 ui round 12.)
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_ulong;

use crate::button_variants::{self, Slice};
use crate::rect::UiRect;

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
