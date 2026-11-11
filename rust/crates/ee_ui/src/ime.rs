//! `UIIMECandidate` (`User Interface/UIIMECandidate.cpp`), the IME candidate window the manager draws over the
//! forms: where `OpenCandidateWindow` puts it ([`open_rect`]), `SetCandidateList`'s page, numbered lines and width
//! ([`candidate_list`]), and the drawing's selection bar, bevelled border ([`frame`]) and line rows ([`row`]).
//!
//! The C++ keeps the members, the candidate strings (the list's multi-byte strings are walked there), the texts,
//! the rhombi, the line and every draw.

use core::ffi::{c_long, c_ulong};

use crate::form_state::{and_in_place, contains_rect};
use crate::rect::{um_floor, UiRect};

/// `OpenCandidateWindow`: below the control, as wide and high as the screen's ratios; when that leaves the screen,
/// moved inside it on the left or the right, above the control when it passes the bottom, and clipped to the
/// screen.
pub fn open_rect(screen: UiRect, control: UiRect, width_ratio: f32, height_ratio: f32) -> UiRect {
    let mut r = UiRect::new(
        control.left,
        control.bottom + 1.0,
        screen.width() * width_ratio + control.left,
        screen.height() * height_ratio + control.top,
    );
    if !contains_rect(&screen, &r) {
        if r.left < screen.left {
            r.left = screen.left;
            r.right = screen.width() * width_ratio + screen.left;
        } else if r.right > screen.right {
            r.right = screen.right;
            r.left = screen.right - screen.width() * width_ratio;
        }
        if r.bottom > screen.bottom {
            r.top = control.top - screen.height() * height_ratio - 1.0;
            r.bottom = control.top - 1.0;
        }
        and_in_place(&mut r, &screen);
    }
    r
}

/// `SetCandidateList`'s strings: the list's walk and the lines.
pub trait CandidateHost {
    /// Steps past one string of the list (one before the page).
    fn skip(&mut self);
    /// The current string as a line `number` + the string: set into the measuring text, kept as a line of the page
    /// and stepped past; the text's width.
    fn line(&mut self, number: &[u8]) -> f32;
}

/// What `SetCandidateList` leaves: the window's left and right edges and the selected line within the page.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CandidateFit {
    pub left: f32,
    pub right: f32,
    pub selection: c_long,
}

/// `SetCandidateList`: the page of the selection (as many lines as the window's `height` holds at `font_height`),
/// each line numbered `"%2ld: "` from 1, the window as wide as the widest line plus 4 (whole pixels; at most the
/// window's `window_width`) from `left`, moved left to stay inside the window. A window lower than a line made the
/// C++ divide by zero; here its page is empty (no line, the selection unchanged).
pub fn candidate_list(
    h: &mut impl CandidateHost,
    height: f32,
    font_height: c_long,
    selection: c_ulong,
    count: c_ulong,
    window_width: c_ulong,
    mut left: f32,
) -> CandidateFit {
    let page_size = um_floor(height / font_height as f32) as c_ulong;
    let page_start = match selection.checked_div(page_size) {
        Some(q) => q.wrapping_mul(page_size),
        None => 0,
    };
    let mut i: c_ulong = 0;
    while i < page_start {
        h.skip();
        i += 1;
    }
    let mut max_width: c_ulong = 0;
    while i < count && i < page_start.wrapping_add(page_size) {
        let number = format!("{:2}: ", i.wrapping_add(1) as c_long);
        let w = h.line(number.as_bytes());
        if (max_width as f32) < w {
            max_width = um_floor(w + 4.0) as c_ulong;
        }
        i += 1;
    }
    if max_width > window_width {
        max_width = window_width;
    }
    let width = max_width as f32;
    if width + left > window_width as f32 {
        left = window_width as f32 - width;
    }
    CandidateFit { left, right: width + left, selection: selection.wrapping_sub(page_start) as c_long }
}

/// One line of the window's border.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CandidateLine {
    pub x0: f32,
    pub y0: f32,
    pub x1: f32,
    pub y1: f32,
    /// The light border colour (else the dark one).
    pub light: bool,
}

/// `DrawPolygons`' plan after the background: the selection bar (when `selection` is not negative: the selected
/// line, two pixels in from each side, clipped to the window) and the two-pixel bevelled border, top (dark),
/// bottom (light), left (dark), right (light), in the C++'s order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CandidateFrame {
    pub has_selection: bool,
    pub selection: UiRect,
    pub lines: [CandidateLine; 8],
}

/// [`CandidateFrame`] for the copied window `r`, selection and the font's line `height`.
pub fn frame(r: UiRect, selection: c_long, height: c_ulong) -> CandidateFrame {
    let mut f = CandidateFrame::default();
    if selection >= 0 {
        let mut s = UiRect::default();
        s.left = r.left + 2.0;
        s.right = r.right - 2.0;
        s.top = (selection as c_ulong).wrapping_mul(height) as f32 + r.top;
        s.bottom = s.top + height as f32;
        and_in_place(&mut s, &r);
        f.has_selection = true;
        f.selection = s;
    }
    let line = |x0, y0, x1, y1, light| CandidateLine { x0, y0, x1, y1, light };
    for i in 0..2u32 {
        let i = i as f32;
        f.lines[i as usize] = line(r.left, r.top + i, r.right + 1.0 - i, r.top + i, false);
        f.lines[2 + i as usize] = line(r.left + i, r.bottom - i, r.right + 1.0, r.bottom - i, true);
        f.lines[4 + i as usize] = line(r.left + i, r.top, r.left + i, r.bottom + 1.0 - i, false);
        f.lines[6 + i as usize] = line(r.right - i, r.top + i, r.right - i, r.bottom + 1.0, true);
    }
    f
}

/// `DrawTextA`'s rectangle of line `index` in the copied window `r` at the font's `height`.
pub fn row(r: UiRect, index: c_ulong, height: c_ulong) -> UiRect {
    let top = r.top + index.wrapping_mul(height) as f32 + 2.0;
    UiRect::new(r.left + 2.0, top, r.right - 2.0, top + height as f32 + 2.0)
}

#[cfg(test)]
mod tests;
