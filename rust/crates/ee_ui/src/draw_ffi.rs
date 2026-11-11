//! `extern "C"` for the controls' drawing plans (`draw.rs`): pure functions over the C++'s values, and the clipped
//! text draw, whose draws go back to the C++ through a function it passes (no import: the module links into tests).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::draw::{self, BorderLine, Clipping, LineRange, TextDraw};
use crate::rect::{UiPoint, UiRect};

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

/// `UITextLabel::CopyData`'s one-line text location.
#[no_mangle]
pub extern "C" fn ee_ui_label_one_line_location(alignment: c_ulong, fit: bool, tr: UiRect, w: f32, h: f32) -> UiPoint {
    draw::label_one_line_location(alignment, fit, &tr, w, h)
}

/// `UITextLabel::CopyData`'s copied lines of a multi-line label (`has_scroll`: it has a scroll bar at `value`).
#[no_mangle]
pub extern "C" fn ee_ui_label_line_range(has_scroll: bool, value: c_long, shown: c_ulong, lines: c_ulong) -> LineRange {
    draw::label_line_range(has_scroll.then_some(value), shown, lines)
}

/// A line of `UITextLabel::DrawTextA`: the text's location and clip rectangle.
#[repr(C)]
pub struct EeUiLabelLine {
    pub at: UiPoint,
    pub rect: UiRect,
}

#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_ui_label_line(
    alignment: c_ulong,
    fit: bool,
    rect: UiRect,
    ctr: UiRect,
    w: f32,
    has_scroll: bool,
    scroll_width: f32,
    offset: c_ulong,
    line_height: c_ulong,
) -> EeUiLabelLine {
    let (at, rect) =
        draw::label_line(alignment, fit, &rect, &ctr, w, has_scroll.then_some(scroll_width), offset, line_height);
    EeUiLabelLine { at, rect }
}

/// `UIListBox::CopyData`'s copied lines `first .. end` and the count drawn.
#[repr(C)]
pub struct EeUiListRange {
    pub first: c_ulong,
    pub end: c_ulong,
    pub count: c_ulong,
}

#[no_mangle]
pub extern "C" fn ee_ui_list_line_range(
    has_scroll: bool,
    value: c_long,
    shown: c_ulong,
    lines: c_ulong,
) -> EeUiListRange {
    let (first, end, count) = draw::list_line_range(has_scroll.then_some(value), shown, lines);
    EeUiListRange { first, end, count }
}

/// `UIListBox::CopyData`'s selection rectangle -> whether the selection is drawn (`out` set then).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_list_selection(
    lines: c_ulong,
    has_selection: bool,
    selection: c_ulong,
    first: c_ulong,
    end: c_ulong,
    tr: UiRect,
    line_height: c_ulong,
    out: *mut UiRect,
) -> bool {
    match draw::list_selection(lines, has_selection, selection, first, end, &tr, line_height) {
        Some(r) => {
            *out = r;
            true
        }
        None => false,
    }
}

/// `UIListBox::DrawPolygons`'s focus rectangle: `out[2k]` and `out[2k + 1]` are line k's start and end.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_focus_lines(r: UiRect, out: *mut UiPoint) {
    for (k, (s, e)) in draw::list_focus_lines(&r).iter().enumerate() {
        *out.add(2 * k) = *s;
        *out.add(2 * k + 1) = *e;
    }
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
