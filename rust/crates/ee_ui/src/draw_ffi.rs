//! `extern "C"` for the controls' drawing plans (`draw.rs`): pure functions over the C++'s values, and the clipped
//! text draw, whose draws go back to the C++ through a function it passes (no import: the module links into tests).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::draw::{self, BorderLine, Clipping, TextDraw};
use crate::rect::UiRect;

/// `UIControl::DrawBorder`'s line `i` of side `side` (0 top, 1 bottom, 2 left, 3 right).
#[no_mangle]
pub extern "C" fn ee_ui_border_line(r: UiRect, side: u32, i: c_ulong, border_type: c_ulong) -> BorderLine {
    draw::border_line(&r, side, i, border_type)
}

/// What `UIForm::DrawClippedText` clips against (`clips`: the form's `mClipRectangles`, `clip_count` of them, read
/// before the first draw).
#[repr(C)]
pub struct EeUiClipInfo {
    pub clips: *const UiRect,
    pub clip_count: usize,
    pub has_popup: bool,
    pub popup_is_control: bool,
    pub popup_rect: UiRect,
    pub popup_draw_rect: UiRect,
    pub has_candidate: bool,
    pub candidate_rect: UiRect,
}

/// The C++'s text draw: `Draw(text, *r, drawing surface)` -> its result.
pub type EeUiTextDrawFn = unsafe extern "C" fn(ctx: *mut c_void, r: *const UiRect) -> c_long;

struct Draws {
    f: EeUiTextDrawFn,
    ctx: *mut c_void,
}

impl TextDraw for Draws {
    fn draw(&mut self, r: UiRect) -> c_long {
        unsafe { (self.f)(self.ctx, &r) }
    }
}

/// `UIForm::DrawClippedText(r, text, control)` -> the last draw's result.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_draw_clipped_text(
    info: *const EeUiClipInfo,
    r: UiRect,
    draw_text: EeUiTextDrawFn,
    ctx: *mut c_void,
) -> c_long {
    let info = &*info;
    let clipping = if info.clip_count > 0 {
        Clipping::Rectangles(core::slice::from_raw_parts(info.clips, info.clip_count).to_vec())
    } else if info.has_popup {
        Clipping::Popup { is_control: info.popup_is_control, rect: info.popup_rect, draw_rect: info.popup_draw_rect }
    } else {
        Clipping::Plain { candidate: info.has_candidate.then_some(info.candidate_rect) }
    };
    draw::draw_clipped_text(&mut Draws { f: draw_text, ctx }, r, &clipping)
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
