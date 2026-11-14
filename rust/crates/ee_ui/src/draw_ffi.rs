//! `extern "C"` for the controls' drawing plans (`draw.rs`): pure functions over the C++'s values (the form's clipped
//! text is the form object's, `formobj.rs`; no import: the module links into tests).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_ulong;

use crate::draw::{self, BorderLine};
use crate::rect::UiRect;

/// `UIControl::DrawBorder`'s line `i` of side `side` (0 top, 1 bottom, 2 left, 3 right).
#[no_mangle]
pub extern "C" fn ee_ui_border_line(r: UiRect, side: u32, i: c_ulong, border_type: c_ulong) -> BorderLine {
    draw::border_line(&r, side, i, border_type)
}

/// `UILBString::DrawTextA`'s row `i`.
#[no_mangle]
pub extern "C" fn ee_ui_string_list_row(ctr: UiRect, i: c_ulong, line_height: c_ulong) -> UiRect {
    draw::string_list_row(&ctr, i, line_height)
}

/// The check box's (`has_border`) and radio button's (not) scaled bitmap: `out[0]`, `out[1]` the x and y scale.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_box_bitmap_scale(
    x_scale: f32,
    y_scale: f32,
    height: f32,
    has_border: bool,
    border: c_ulong,
    out: *mut f32,
) {
    let (x, y) = draw::box_bitmap_scale(x_scale, y_scale, height, has_border.then_some(border));
    *out = x;
    *out.add(1) = y;
}
