//! The UI controls' drawing (`User Interface/UIControl.cpp`, `UIForm.cpp`, `UITextLabel.cpp`, `UILBString.cpp`,
//! `UIListBox.h`, `UICheckBox.cpp`, `UIRadioButton.cpp`): what each draw call is given. The control's bevelled
//! border (`UIControl::DrawBorder`), the form's clipping of a text draw around its clip rectangles, the copied popup
//! or the IME candidate window (`UIForm::DrawClippedText`), where a text label places its line or lines, which
//! lines a label or list box copies for drawing, a list box's selection and focus rectangle and the rows of a
//! string list box, and the check box's and radio button's bitmap scale.
//!
//! The C++ makes every draw call itself, in the original order, from these plans (the line, rhombus, bitmap and
//! text objects and the rasterizer stay C++); the clipped text draw is the one sequence run here, its draws going
//! back to the C++ through [`TextDraw`]. Float operations are the C++'s, in its order and association.

use core::ffi::{c_long, c_ulong};

use crate::form_state::intersects;
use crate::rect::{UiPoint, UiRect};

/// One line of `UIControl::DrawBorder`: from `start` to `end` in the light colour (`UIControl::sLightBorder`) or the
/// dark one (`sDarkBorder`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct BorderLine {
    pub start: UiPoint,
    pub end: UiPoint,
    pub light: bool,
}

/// `DrawBorder`'s line `i` (0 .. thickness) of side `side`: 0 the top rows, 1 the bottom rows, 2 the left columns,
/// 3 the right columns, drawn in that order. The top and left take the top-left colour (dark for border type 0,
/// light for any other type), the bottom and right the other.
pub fn border_line(r: &UiRect, side: u32, i: c_ulong, border_type: c_ulong) -> BorderLine {
    let fi = i as f32;
    let top_left_light = border_type != 0;
    let p = |x: f32, y: f32| UiPoint { x, y };
    match side {
        0 => BorderLine { start: p(r.left, r.top + fi), end: p(r.right + 1.0 - fi, r.top + fi), light: top_left_light },
        1 => BorderLine {
            start: p(r.left + fi, r.bottom - fi),
            end: p(r.right + 1.0, r.bottom - fi),
            light: !top_left_light,
        },
        2 => {
            BorderLine { start: p(r.left + fi, r.top), end: p(r.left + fi, r.bottom + 1.0 - fi), light: top_left_light }
        }
        _ => BorderLine {
            start: p(r.right - fi, r.top + fi),
            end: p(r.right - fi, r.bottom + 1.0),
            light: !top_left_light,
        },
    }
}

/// The text draws of `UIForm::DrawClippedText`: `Draw(text, rectangle, drawing surface)` -> its result.
pub trait TextDraw {
    fn draw(&mut self, r: UiRect) -> c_long;
}

/// What the form clips a text draw against, as `DrawClippedText` tests it in order.
#[derive(Clone, Debug, PartialEq)]
pub enum Clipping {
    /// The form's clip rectangles (at least one).
    Rectangles(Vec<UiRect>),
    /// No clip rectangle, a copied popup: whether the drawing control is the popup (`mCopiedPopup ==
    /// mControls[control->GetIndex()]`), the popup's rectangle and its copied draw rectangle.
    Popup { is_control: bool, rect: UiRect, draw_rect: UiRect },
    /// No clip rectangle, no popup: the IME candidate window's copied rectangle while it is drawn.
    Plain { candidate: Option<UiRect> },
}

/// `RemoveIntersectingRectangle(other, out)` appending to `out` (`existing` = its size before the call).
fn remove_intersecting(r: &UiRect, other: &UiRect, out: &mut Vec<UiRect>) {
    let g = |x: &UiRect| ee_ge::types::GeRectangle::new(x.left, x.top, x.right, x.bottom);
    let existing = out.len();
    g(r).remove_intersecting(&g(other), existing, |p| out.push(UiRect::new(p.left, p.top, p.right, p.bottom)));
}

/// The pieces of `r` drawn while `around` covers part of it, stopping at the first draw that fails.
fn draw_pieces(h: &mut impl TextDraw, pieces: &[UiRect]) -> c_long {
    let mut result = 0;
    for p in pieces {
        if result != 0 {
            break;
        }
        result = h.draw(*p);
    }
    result
}

/// `UIForm::DrawClippedText(r, text, control)`: the text drawn into `r` whole, or into the pieces of `r` outside
/// the rectangles that cover it (none when they cover it all). The whole draw's result is returned as it is; a
/// pieced draw stops at its first failing piece.
pub fn draw_clipped_text(h: &mut impl TextDraw, r: UiRect, clipping: &Clipping) -> c_long {
    let mut pieces = Vec::new();
    match clipping {
        Clipping::Plain { candidate: None } => h.draw(r),
        Clipping::Plain { candidate: Some(c) } => {
            if !intersects(&r, c) {
                return h.draw(r);
            }
            remove_intersecting(&r, c, &mut pieces);
            draw_pieces(h, &pieces)
        }
        Clipping::Popup { is_control, rect, draw_rect } => {
            if *is_control || !intersects(&r, rect) {
                return h.draw(r);
            }
            remove_intersecting(&r, draw_rect, &mut pieces);
            draw_pieces(h, &pieces)
        }
        Clipping::Rectangles(clips) => {
            let mut clipped = false;
            for clip in clips {
                if intersects(&r, clip) {
                    if !clipped {
                        pieces.push(r);
                        clipped = true;
                    }
                    let mut remaining = Vec::new();
                    for p in &pieces {
                        remove_intersecting(p, clip, &mut remaining);
                    }
                    pieces = remaining;
                }
            }
            if clipped {
                draw_pieces(h, &pieces)
            } else {
                h.draw(r)
            }
        }
    }
}

/// `UITextLabel`'s margin (`kTextLabelMargin`), half of it kept from an edge of a label fitted to its text.
pub const TEXT_LABEL_MARGIN: f32 = 6.0;

/// `UITextLabel::CopyData`, a one-line label: where its text goes in the text rectangle `tr` (alignment 0 left,
/// 1 centred, any other right; `fit` keeps half the margin from the left or right edge), the text being `w` x `h`.
pub fn label_one_line_location(alignment: c_ulong, fit: bool, tr: &UiRect, w: f32, h: f32) -> UiPoint {
    let x = match alignment {
        0 => {
            let mut x = tr.left;
            if fit {
                x += TEXT_LABEL_MARGIN * 0.5;
            }
            x
        }
        1 => tr.width() * 0.5 + tr.left - w * 0.5,
        _ => {
            let mut x = tr.right - w;
            if fit {
                x -= TEXT_LABEL_MARGIN * 0.5;
            }
            x
        }
    };
    UiPoint { x, y: tr.height() * 0.5 + tr.top - h * 0.5 }
}

/// `UITextLabel::CopyData`, a multi-line label: the lines copied for drawing, `first .. end`, and the last line
/// `DrawTextA` draws (`mLastDisplayLine`): the scroll bar's window of `shown` lines from its value, or every line.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct LineRange {
    pub first: c_ulong,
    pub end: c_ulong,
    pub last: c_long,
}

pub fn label_line_range(scroll_value: Option<c_long>, shown: c_ulong, lines: c_ulong) -> LineRange {
    match scroll_value {
        Some(v) => {
            let first = v as c_ulong;
            LineRange { first, end: shown.wrapping_add(first), last: shown.wrapping_sub(1) as c_long }
        }
        None => LineRange { first: 0, end: lines, last: lines.wrapping_sub(1) as c_long },
    }
}

/// `UITextLabel::DrawTextA`, a multi-line label's line: its text location (`w` wide, `offset` rows of pixels down)
/// and the rectangle it is clipped to. The centring uses the control's rectangle `rect` and the copied text
/// rectangle `ctr` and moves left by half the scroll bar's copied width when the label has one; the right
/// alignment ends at the control's right edge.
#[allow(clippy::too_many_arguments)]
pub fn label_line(
    alignment: c_ulong,
    fit: bool,
    rect: &UiRect,
    ctr: &UiRect,
    w: f32,
    scroll_width: Option<f32>,
    offset: c_ulong,
    line_height: c_ulong,
) -> (UiPoint, UiRect) {
    let x = match alignment {
        0 => {
            let mut x = ctr.left;
            if fit {
                x += TEXT_LABEL_MARGIN * 0.5;
            }
            x
        }
        1 => {
            let mut x = rect.width() * 0.5 + ctr.left - w * 0.5;
            if let Some(s) = scroll_width {
                x -= s * 0.5;
            }
            x
        }
        _ => {
            let mut x = rect.right - w;
            if fit {
                x -= TEXT_LABEL_MARGIN * 0.5;
            }
            x
        }
    };
    let at = UiPoint { x, y: offset as f32 + ctr.top };
    let height = line_height as f32;
    (at, UiRect::new(at.x, at.y, ctr.right, at.y + height))
}

/// `UIListBox::CopyData`: the lines copied (`first .. end`) and drawn (`mDrawLineCount`): the scroll bar's window of
/// `shown` lines from its value, or every line.
pub fn list_line_range(scroll_value: Option<c_long>, shown: c_ulong, lines: c_ulong) -> (c_ulong, c_ulong, c_ulong) {
    match scroll_value {
        Some(v) => {
            let first = v as c_ulong;
            (first, shown.wrapping_add(first), shown)
        }
        None => (0, lines, lines),
    }
}

/// `UIListBox::CopyData`: the selected line's rectangle when it is among the copied lines `first .. end`, across the
/// text rectangle `tr`, `line_height` high less one.
pub fn list_selection(
    lines: c_ulong,
    has_selection: bool,
    selection: c_ulong,
    first: c_ulong,
    end: c_ulong,
    tr: &UiRect,
    line_height: c_ulong,
) -> Option<UiRect> {
    if !(lines > 0 && has_selection && selection >= first && selection < end) {
        return None;
    }
    let top = selection.wrapping_sub(first).wrapping_mul(line_height) as f32 + tr.top;
    let bottom = top + line_height as f32 - 1.0;
    Some(UiRect { left: tr.left, right: tr.right, top, bottom })
}

/// `UIListBox::DrawPolygons`: the focus rectangle around the selection, four lines in the C++'s order (top, left,
/// bottom, right; each keeps the previous line's other end).
pub fn list_focus_lines(r: &UiRect) -> [(UiPoint, UiPoint); 4] {
    let p = |x: f32, y: f32| UiPoint { x, y };
    let (tl, tr, bl, br) = (p(r.left, r.top), p(r.right, r.top), p(r.left, r.bottom), p(r.right, r.bottom));
    [(tl, tr), (tl, bl), (br, bl), (br, tr)]
}

/// `UILBString::DrawTextA`: row `i`'s rectangle (one pixel wider than the copied text rectangle `ctr`; the row's
/// pixel offset an unsigned 32-bit product, as the C++'s).
pub fn string_list_row(ctr: &UiRect, i: c_ulong, line_height: c_ulong) -> UiRect {
    let top = i.wrapping_mul(line_height) as f32 + ctr.top;
    UiRect { left: ctr.left, right: ctr.right + 1.0, top, bottom: line_height as f32 + top }
}

/// The scaled bitmap of `UICheckBox::DrawPolygons` (`border` = the border thickness: the box's inner height) and
/// `UIRadioButton::DrawPolygons` (`border` = `None`: its whole height): `scale * (height * 0.05)`, a box of 20
/// pixels being the bitmap's own size.
pub fn box_bitmap_scale(x_scale: f32, y_scale: f32, height: f32, border: Option<c_ulong>) -> (f32, f32) {
    match border {
        Some(b) => {
            let border = (2 as c_ulong).wrapping_mul(b) as f32;
            (x_scale * ((height - border) * 0.05), y_scale * ((height - border) * 0.05))
        }
        None => (x_scale * (height * 0.05), y_scale * (height * 0.05)),
    }
}

#[cfg(test)]
mod tests;
