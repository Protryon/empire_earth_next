//! `UIStatusBar` (`User Interface/UIStatusBar.cpp`), a progress bar: the value clamped to its range and the fill's
//! width ([`fill`]), the fill and text rectangles a move lays out ([`location`]) and the frame's copies (the fill
//! inset by the border, the text centred in its area: [`copy_fill`], [`copy_text`]). The C++ keeps the rhombi, the texts, every draw
//! and `SetDirty`.

use core::ffi::{c_long, c_ulong};

use crate::rect::UiRect;

/// `SetCurrentRangeValue`: the value clamped to `[min, max]` (below `min` first: a reversed range takes `min`) and
/// the fill rectangle, `fill` with its right edge `left + width * percent` (at least one pixel above 0 %; an empty
/// range is 0 %). `width` is `GetWidth()`, the control's whole pixels.
pub fn fill(value: c_long, min: c_long, max: c_long, width: c_ulong, fill: UiRect) -> (c_long, UiRect) {
    let value = if value < min {
        min
    } else if value > max {
        max
    } else {
        value
    };
    let percent = if max == min { 0.0 } else { (value as f32 - min as f32) / (max as f32 - min as f32) };
    let mut w = width as f32 * percent;
    if percent > 0.0 && w < 1.0 {
        w = 1.0;
    }
    let mut r = fill;
    r.right = r.left + w;
    (value, r)
}

/// `SetLocation`'s rectangles from the moved control rectangle: the fill's (one pixel past the right and bottom
/// edges, before `SetCurrentRangeValue` sizes it) and the text area (inset by the border).
pub fn location(rect: UiRect, border: c_ulong) -> (UiRect, UiRect) {
    let fill = UiRect::new(rect.left, rect.top, rect.right + 1.0, rect.bottom + 1.0);
    (fill, inset(rect, border))
}

/// The rectangle inset by `border` on every side (the constructor's and `SetLocation`'s text area).
pub fn inset(rect: UiRect, border: c_ulong) -> UiRect {
    let b = border as f32;
    UiRect::new(rect.left + b, rect.top + b, rect.right - b, rect.bottom - b)
}

/// `CopyData`'s fill: inset by the border when there is one (`None`: the copy keeps the bar's rectangle).
pub fn copy_fill(fill: UiRect, border: c_ulong) -> Option<UiRect> {
    if border == 0 {
        return None;
    }
    let b = border as f32;
    let mut f = fill;
    f.left += b;
    f.right -= b;
    f.top += b;
    f.bottom -= b;
    Some(f)
}

/// `CopyData`'s text rectangle for a text of `extent` (width, height): the text area, moved right and down to
/// centre a text smaller than it.
pub fn copy_text(text: UiRect, extent: (f32, f32)) -> UiRect {
    let mut t = text;
    if extent.0 < t.width() {
        t.left = t.width() * 0.5 + t.left - extent.0 * 0.5;
    }
    if extent.1 < t.height() {
        t.top = t.height() * 0.5 + t.top - extent.1 * 0.5;
    }
    t
}

#[cfg(test)]
mod tests;
