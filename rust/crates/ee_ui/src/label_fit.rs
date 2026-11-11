//! A text label's geometry (`User Interface/UITextLabel.cpp`): `ComputeTextLabelRect` (a fit-to-text label's
//! rectangle around its line, or its visible lines), `CheckScrollBar` (whether a multi-line label's scroll bar is
//! made, kept with a new range, or marked for removal) and `SetLocation`'s text and scroll bar rectangles.
//!
//! The text object, the scroll bar and the label stay C++ objects: the lines are measured through the host (it sets
//! the label's text object to the line, as the C++ did, and reads its extent), and the scroll bar's calls are host
//! calls in the C++'s order.

use core::ffi::{c_long, c_ulong};

use crate::rect::UiRect;

/// `kTextLabelMargin` (0x10099b54): the width a fitted label adds to its text's.
pub const TEXT_LABEL_MARGIN: f32 = 6.0;
/// `kScrollBarWidth`.
pub const SCROLL_BAR_WIDTH: f32 = 20.0;
/// `kTextAlignCenter`, `kTextAlignRight`.
pub const ALIGN_CENTER: c_ulong = 1;
pub const ALIGN_RIGHT: c_ulong = 2;

/// The label as its geometry asks about it.
pub trait LabelHost {
    /// `mFitRectToText`, `mLabelType`, `mAlignment`, `mOriginalRectangle`, `mRectangle`.
    fn fit(&self) -> bool;
    fn label_type(&self) -> c_ulong;
    fn alignment(&self) -> c_ulong;
    fn original(&self) -> UiRect;
    fn rect(&self) -> UiRect;
    /// `mLines.size()`, `mNumberLinesDisplayed`.
    fn lines(&self) -> usize;
    fn displayed(&self) -> c_ulong;
    /// `mText->SetText(*mLines[i])`, then its extent's width and height.
    fn measure(&mut self, i: usize) -> (f32, f32);
    /// `mScrollBar != NULL`, `mScrollBarRemoved` (and setting it), the slider's `GetCurrentRangeValue()`.
    fn has_scroll_bar(&self) -> bool;
    fn removed(&self) -> bool;
    fn set_removed(&mut self, removed: bool);
    fn scroll_value(&mut self) -> c_long;
    /// `SetRectangle(r)` (virtual), `SetDirty(true)` (virtual), `SetBgRectangle(r)`.
    fn set_rectangle(&mut self, r: UiRect);
    fn set_dirty(&mut self);
    fn set_bg_rectangle(&mut self, r: UiRect);
    /// The scroll bar's `SetRange(0, max)`.
    fn set_range(&mut self, max: c_ulong);
    /// `new UIScrollBar(mId, mScrollBarRectangle's top left, height, 1, ...)`, then its `SetPageSize(page)`,
    /// `SetBgColor`, `SetBgOpacity` (the C++ sets the range between the creation and the page size: `set_range`)
    fn create_scroll_bar(&mut self, height: f32);
    fn set_page_size(&mut self, page: c_ulong);
    fn set_scroll_bar_colors(&mut self);
    /// `mScrollBarRectangle`, the new scroll bar's rectangle.
    fn scroll_bar_rect(&self) -> UiRect;
    fn scroll_bar_control_rect(&self) -> UiRect;
}

/// `ComputeTextLabelRect`: with fit-to-text on, a one-line label's rectangle is its text's width (plus the margin)
/// placed by its alignment and centred vertically on its original rectangle; a multi-line label's is its widest
/// visible line by the visible lines' heights from the original top left. Rounded; set (and dirty) when it changed.
pub fn compute_rect(h: &mut impl LabelHost) {
    let mut r = h.original();
    if !h.fit() {
        return;
    }
    let mut width;
    let mut height;
    if h.label_type() == 0 && h.lines() > 0 {
        (width, height) = h.measure(0);
        if h.alignment() == ALIGN_CENTER {
            r.left = r.width() * 0.5 + r.left - width * 0.5;
        } else if h.alignment() == ALIGN_RIGHT {
            r.left = r.right - width;
        }
        r.top = r.height() * 0.5 + r.top - height * 0.5;
    } else {
        height = 0.0;
        width = 0.0;
        let (mut i, end): (c_ulong, c_ulong) = if h.has_scroll_bar() && !h.removed() {
            let i = h.scroll_value() as c_ulong;
            (i, h.displayed().wrapping_add(i))
        } else {
            (0, h.lines() as c_ulong)
        };
        while i < end {
            let (w, hh) = h.measure(i as usize);
            height += hh;
            if w > width {
                width = w;
            }
            i += 1;
        }
    }
    r.right = r.left + TEXT_LABEL_MARGIN + width;
    r.bottom = r.top + height;
    r.round_to_nearest();
    let cur = h.rect();
    if r.top != cur.top || r.left != cur.left || r.right != cur.right || r.bottom != cur.bottom {
        h.set_rectangle(r);
        h.set_dirty();
    }
}

/// `CheckScrollBar`: a shown scroll bar the lines now fit without is marked for removal (the next `CopyData`
/// deletes it), otherwise its range follows the lines; when the lines overflow, a missing scroll bar is made
/// (its page a line less than the visible rows, the background narrowed to its left). Then the fit.
pub fn check_scroll_bar(h: &mut impl LabelHost) {
    let lines = h.lines() as c_ulong;
    let displayed = h.displayed();
    if h.has_scroll_bar() && !h.removed() {
        if lines <= displayed {
            h.set_removed(true);
            h.set_dirty();
            let r = h.rect();
            h.set_bg_rectangle(r);
        } else {
            h.set_range(lines.wrapping_sub(displayed));
        }
    } else if lines > displayed {
        if !h.has_scroll_bar() {
            let s = h.scroll_bar_rect();
            h.create_scroll_bar(s.bottom + 1.0 - s.top);
            h.set_range(lines.wrapping_sub(displayed));
            h.set_page_size(if displayed > 1 { displayed - 1 } else { displayed });
            h.set_scroll_bar_colors();
            let r = h.rect();
            let left = h.scroll_bar_control_rect().left;
            h.set_bg_rectangle(UiRect::new(r.left, r.top, left - 1.0, r.bottom));
        } else {
            h.set_range(lines.wrapping_sub(displayed));
        }
        h.set_removed(false);
    }
    compute_rect(h);
}

/// `SetLocation`'s rectangles after `UIControl::SetLocation`: the scroll bar's strip at the right and the text
/// rectangle inset by the border and a pixel, less the strip when there is a scroll bar -> (scroll bar rectangle,
/// text rectangle).
pub fn location_rects(rect: UiRect, border: c_ulong, scroll_bar: bool) -> (UiRect, UiRect) {
    let s = UiRect::new(rect.right - SCROLL_BAR_WIDTH + 1.0, rect.top, rect.right, rect.bottom);
    let b = border as f32;
    let mut t = UiRect::new(rect.left + b + 1.0, rect.top + b + 1.0, rect.right - b - 1.0, rect.bottom - b - 1.0);
    if scroll_bar {
        t.right = t.right - s.width() - 1.0;
    }
    (s, t)
}

#[cfg(test)]
mod tests;
