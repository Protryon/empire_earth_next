//! WONGUI's scrolling arithmetic (Scrollbar.cpp, ScrollbarScroller.cpp, ButtonScroller.cpp, ScrollArea.cpp). The
//! C++ keeps the scrollbars, their arrow, track and thumb controls, the scroll areas, their viewports and
//! scrollers, the events and every `SetPos` / `SetSize` / `SetVisible` / `SetEnabled` call; these answer the
//! numbers and the decisions:
//!
//! - a scrollbar's thumb: hidden, its size and its position along the track (`LayoutThumb`), dragged
//!   (`MouseDrag`), and the position a line or page step leaves (`HandleComponentEvent`);
//! - a scroll area's offsets clamped to its content (`AdjustPosition`), its view inside the padding and the
//!   bottom pin (`Virtual08`, `OnBottom`), its desired size and its size to fit the content;
//! - what a scrollbar condition does to a scrollbar's visibility (`EnableScrollbar`) and to a pair of scroll
//!   buttons (`EnableScrollButtons`), and which scroll buttons can still scroll (`ButtonScroller`).
//!
//! Every sum is the 32-bit `int` of the original, wrapping (`-fwrapv`).

/// A scrollbar's thumb along its track (`LayoutThumb`'s inputs, all along the bar's axis).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonThumbIn {
    pub page: i32,
    pub total: i32,
    pub position: i32,
    /// The track's top and height.
    pub track_top: i32,
    pub track_len: i32,
    /// The thumb's top and height now.
    pub thumb_top: i32,
    pub thumb_len: i32,
    pub resize_thumb: bool,
    pub min_thumb: i32,
    pub change_pos: bool,
}

/// `LayoutThumb`'s answer: `hidden` (everything fits: the thumb is hidden and nothing else changes), else the
/// thumb's size and, when `set_pos`, its new top.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonThumbOut {
    pub hidden: bool,
    pub set_pos: bool,
    pub pos: i32,
    pub len: i32,
}

/// `Scrollbar::LayoutThumb`: the thumb's size (the track's share of the page, at least the minimum, when the
/// thumb resizes) and top (the position's share of the free track, when asked), kept inside the track; a thumb
/// pushed back inside is always moved. The total is above the page here, so the divisions are by a positive
/// number unless the total is 0 (a negative page), which the original divided by as well (a crash).
pub fn thumb_layout(t: EeWonThumbIn) -> EeWonThumbOut {
    if t.page >= t.total {
        return EeWonThumbOut { hidden: true, ..EeWonThumbOut::default() };
    }
    let mut len = t.thumb_len;
    if t.resize_thumb {
        len = t.track_len.wrapping_mul(t.page) / t.total;
        if len < t.min_thumb {
            len = t.min_thumb;
        }
    }
    let mut change = t.change_pos;
    let mut pos = t.thumb_top;
    if change {
        pos = t
            .track_top
            .wrapping_add(t.track_len.wrapping_sub(len).wrapping_mul(t.position) / t.total.wrapping_sub(t.page));
    }
    if pos < t.track_top {
        pos = t.track_top;
        change = true;
    }
    let track_bottom = t.track_top.wrapping_add(t.track_len);
    if pos.wrapping_add(len) >= track_bottom {
        pos = track_bottom.wrapping_sub(len);
        change = true;
    }
    EeWonThumbOut { hidden: false, set_pos: change, pos, len }
}

/// `Scrollbar::MouseDrag` on the thumb: (the thumb's new top, the new position). `new_top` is the thumb's top
/// plus the drag along the axis; it is kept inside the track. A track no longer than the thumb gives 0.
pub fn thumb_drag(new_top: i32, track_top: i32, track_len: i32, thumb_len: i32, total: i32, page: i32) -> (i32, i32) {
    let mut y = new_top;
    let track_bottom = track_top.wrapping_add(track_len);
    if y < track_top {
        y = track_top;
    }
    if y.wrapping_add(thumb_len) > track_bottom {
        y = track_bottom.wrapping_sub(thumb_len);
    }
    let diff = track_len.wrapping_sub(thumb_len);
    let position = if diff <= 0 { 0 } else { total.wrapping_sub(page).wrapping_mul(y.wrapping_sub(track_top)) / diff };
    (y, position)
}

/// What pressed a scrollbar's button (`Scrollbar::HandleComponentEvent`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonScrollPress {
    LineUp = 0,
    LineDown = 1,
    /// The track, with the mouse along the axis.
    Track = 2,
}

/// The position after a press: a line up or down, or a page towards the mouse on the track (none on the thumb),
/// clamped to [0, total - page] (0 wins when that is negative).
#[allow(clippy::too_many_arguments)]
pub fn scroll_step(
    press: EeWonScrollPress,
    mouse: i32,
    thumb_top: i32,
    thumb_len: i32,
    position: i32,
    line_up: i32,
    line_down: i32,
    page_step: i32,
    total: i32,
    page: i32,
) -> i32 {
    let mut p = position;
    match press {
        EeWonScrollPress::LineUp => p = p.wrapping_sub(line_up),
        EeWonScrollPress::LineDown => p = p.wrapping_add(line_down),
        EeWonScrollPress::Track => {
            if mouse < thumb_top {
                p = p.wrapping_sub(page_step);
            } else if mouse >= thumb_top.wrapping_add(thumb_len) {
                p = p.wrapping_add(page_step);
            }
        }
    }
    let max = total.wrapping_sub(page);
    if p > max {
        p = max;
    }
    if p < 0 {
        p = 0;
    }
    p
}

/// A scroll area's numbers (`ScrollArea`): the offsets, the content, the view and the padding.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonScrollArea {
    pub vert_offset: i32,
    pub horz_offset: i32,
    pub total_width: i32,
    pub total_height: i32,
    pub extra_height: i32,
    /// The view (`mScrollArea`).
    pub view_x: i32,
    pub view_y: i32,
    pub view_width: i32,
    pub view_height: i32,
}

/// `ScrollArea::AdjustPosition`'s clamp: (vertical, horizontal) offsets inside the content (the vertical one
/// with the extra height), 0 when the content is smaller than the view.
pub fn clamp_offsets(a: &EeWonScrollArea) -> (i32, i32) {
    let mut v = a.vert_offset;
    let content = a.total_height.wrapping_add(a.extra_height);
    if v.wrapping_add(a.view_height) > content {
        v = content.wrapping_sub(a.view_height);
    }
    if v < 0 {
        v = 0;
    }
    let mut h = a.horz_offset;
    if h.wrapping_add(a.view_width) > a.total_width {
        h = a.total_width.wrapping_sub(a.view_width);
    }
    if h < 0 {
        h = 0;
    }
    (v, h)
}

/// `ScrollArea::OnBottom`: scrolled down (offset above 0) to the content's end (without the extra height).
pub fn on_bottom(a: &EeWonScrollArea) -> bool {
    a.vert_offset > 0 && a.vert_offset.wrapping_add(a.view_height) >= a.total_height
}

/// `ScrollArea::Virtual08`: the view inside the padding (sizes at least 0) and, when the area was on the bottom
/// before, the vertical offset that keeps it there (before `AdjustPosition`'s clamp).
#[allow(clippy::too_many_arguments)]
pub fn view_layout(
    a: &EeWonScrollArea,
    width: i32,
    height: i32,
    left_pad: i32,
    top_pad: i32,
    right_pad: i32,
    bottom_pad: i32,
) -> EeWonScrollArea {
    let bottom = on_bottom(a);
    let mut r = *a;
    r.view_x = left_pad;
    r.view_y = top_pad;
    r.view_width = width.wrapping_sub(right_pad).wrapping_sub(left_pad).max(0);
    r.view_height = height.wrapping_sub(bottom_pad).wrapping_sub(top_pad).max(0);
    if bottom {
        r.vert_offset = a.total_height.wrapping_add(a.extra_height).wrapping_sub(r.view_height);
    }
    r
}

/// `ScrollArea::GetDesiredSize`: the content, at most 300 x 300.
pub fn desired_size(total_width: i32, total_height: i32) -> (i32, i32) {
    (if total_width < 300 { total_width } else { 300 }, if total_height < 300 { total_height } else { 300 })
}

/// `ScrollArea::SetSizeEqualToTotal`: the content (at least the minimum) plus the padding.
pub fn size_equal_to_total(a: &EeWonScrollArea, min_width: i32, min_height: i32, pads: [i32; 4]) -> (i32, i32) {
    let w = if min_width > a.total_width { min_width } else { a.total_width };
    let h = if min_height > a.total_height { min_height } else { a.total_height };
    (w.wrapping_add(pads[0]).wrapping_add(pads[2]), h.wrapping_add(pads[1]).wrapping_add(pads[3]))
}

/// `ScrollbarScroller::EnableScrollbar`: the visibility to set (condition 0 disable, 1 enable, 2 conditional:
/// the content overflows the view on the bar's axis; another condition sets nothing), or `None` when a visible
/// conditional bar is left alone while conditions are being applied. The caller reports a change when the
/// visibility differs afterwards.
pub fn scrollbar_condition(
    condition: i32,
    setting_conditions: bool,
    visible: bool,
    vertical: bool,
    a: &EeWonScrollArea,
) -> Option<Option<bool>> {
    if setting_conditions && condition == 2 && visible {
        return None;
    }
    Some(match condition {
        0 => Some(false),
        1 => Some(true),
        2 => Some(if vertical { a.total_height > a.view_height } else { a.total_width > a.view_width }),
        _ => None,
    })
}

/// `ScrollbarScroller::NotifyScrollChange` for one bar: (total, page, enabled). The vertical bar's content has
/// the extra height.
pub fn scrollbar_sync(a: &EeWonScrollArea, vertical: bool) -> (i32, i32, bool) {
    if vertical {
        let total = a.total_height.wrapping_add(a.extra_height);
        (total, a.view_height, total > a.view_height)
    } else {
        (a.total_width, a.view_width, a.total_width > a.view_width)
    }
}

/// `ButtonScroller::NotifyScrollChange`: whether the left, up, right and down buttons can still scroll.
pub fn scroll_buttons_enabled(a: &EeWonScrollArea) -> [bool; 4] {
    [
        a.horz_offset > 0,
        a.vert_offset > 0,
        a.horz_offset.wrapping_add(a.view_width) < a.total_width,
        a.vert_offset.wrapping_add(a.view_height) < a.total_height.wrapping_add(a.extra_height),
    ]
}

/// `ButtonScroller::EnableScrollButtons` for a pair: the (first, second) visibility to set, or `None` when the
/// pair is left alone (hiding a visible one while conditions are being applied). Conditions: 0 disable, 1
/// enable, 2 conditional (both shown unless both are disabled), 3 partial (each shown when enabled); another
/// keeps their visibility.
pub fn scroll_button_condition(
    condition: i32,
    setting_conditions: bool,
    visible: [bool; 2],
    disabled: [bool; 2],
) -> Option<[bool; 2]> {
    let show = match condition {
        0 => [false, false],
        1 => [true, true],
        2 => {
            let s = !(disabled[0] && disabled[1]);
            [s, s]
        }
        3 => [!disabled[0], !disabled[1]],
        _ => visible,
    };
    if setting_conditions && ((!show[0] && visible[0]) || (!show[1] && visible[1])) {
        return None;
    }
    Some(show)
}

// ---- the C ABI

#[no_mangle]
pub extern "C" fn ee_won_gui_thumb_layout(t: EeWonThumbIn) -> EeWonThumbOut {
    thumb_layout(t)
}

/// `thumb_drag`: the new top into `out_top`, the position returned.
///
/// # Safety
/// `out_top` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_thumb_drag(
    new_top: i32,
    track_top: i32,
    track_len: i32,
    thumb_len: i32,
    total: i32,
    page: i32,
    out_top: *mut i32,
) -> i32 {
    let (y, p) = thumb_drag(new_top, track_top, track_len, thumb_len, total, page);
    // SAFETY: writable (the contract)
    unsafe { *out_top = y };
    p
}

#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_won_gui_scroll_step(
    press: EeWonScrollPress,
    mouse: i32,
    thumb_top: i32,
    thumb_len: i32,
    position: i32,
    line_up: i32,
    line_down: i32,
    page_step: i32,
    total: i32,
    page: i32,
) -> i32 {
    scroll_step(press, mouse, thumb_top, thumb_len, position, line_up, line_down, page_step, total, page)
}

/// `clamp_offsets`: the area's offsets clamped in place.
///
/// # Safety
/// `a` is a valid area.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_scroll_clamp(a: *mut EeWonScrollArea) {
    // SAFETY: valid (the contract)
    let a = unsafe { &mut *a };
    let (v, h) = clamp_offsets(a);
    a.vert_offset = v;
    a.horz_offset = h;
}

#[no_mangle]
pub extern "C" fn ee_won_gui_scroll_on_bottom(a: EeWonScrollArea) -> bool {
    on_bottom(&a)
}

/// `view_layout`: the view and the pinned offset in place.
///
/// # Safety
/// `a` is a valid area.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_scroll_view(
    a: *mut EeWonScrollArea,
    width: i32,
    height: i32,
    left_pad: i32,
    top_pad: i32,
    right_pad: i32,
    bottom_pad: i32,
) {
    // SAFETY: valid (the contract)
    let a = unsafe { &mut *a };
    *a = view_layout(a, width, height, left_pad, top_pad, right_pad, bottom_pad);
}

/// `desired_size` (`which` 0) or `size_equal_to_total` (1, with the minimum and the left, top, right, bottom
/// padding) into the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_scroll_size(
    which: i32,
    a: EeWonScrollArea,
    min_width: i32,
    min_height: i32,
    left_pad: i32,
    top_pad: i32,
    right_pad: i32,
    bottom_pad: i32,
    out_w: *mut i32,
    out_h: *mut i32,
) {
    let (w, h) = if which == 0 {
        desired_size(a.total_width, a.total_height)
    } else {
        size_equal_to_total(&a, min_width, min_height, [left_pad, top_pad, right_pad, bottom_pad])
    };
    // SAFETY: writable (the contract)
    unsafe {
        *out_w = w;
        *out_h = h;
    }
}

/// `scrollbar_condition`: -1 leave the bar alone (no change), 0 / 1 the visibility to set, 2 set nothing.
#[no_mangle]
pub extern "C" fn ee_won_gui_scrollbar_condition(
    condition: i32,
    setting_conditions: bool,
    visible: bool,
    vertical: bool,
    a: EeWonScrollArea,
) -> i32 {
    match scrollbar_condition(condition, setting_conditions, visible, vertical, &a) {
        None => -1,
        Some(Some(v)) => i32::from(v),
        Some(None) => 2,
    }
}

/// `scrollbar_sync`: the total and page into the outs, enabled returned.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_scrollbar_sync(
    a: EeWonScrollArea,
    vertical: bool,
    out_total: *mut i32,
    out_page: *mut i32,
) -> bool {
    let (t, p, e) = scrollbar_sync(&a, vertical);
    // SAFETY: writable (the contract)
    unsafe {
        *out_total = t;
        *out_page = p;
    }
    e
}

/// `scroll_buttons_enabled`: bit 0 left, 1 up, 2 right, 3 down.
#[no_mangle]
pub extern "C" fn ee_won_gui_scroll_buttons(a: EeWonScrollArea) -> u32 {
    scroll_buttons_enabled(&a).iter().enumerate().fold(0, |m, (i, &b)| if b { m | (1 << i) } else { m })
}

/// `scroll_button_condition`: -1 leave the pair alone, else bit 0 show the first, bit 1 show the second.
#[no_mangle]
pub extern "C" fn ee_won_gui_scroll_button_condition(
    condition: i32,
    setting_conditions: bool,
    visible_first: bool,
    visible_second: bool,
    disabled_first: bool,
    disabled_second: bool,
) -> i32 {
    match scroll_button_condition(
        condition,
        setting_conditions,
        [visible_first, visible_second],
        [disabled_first, disabled_second],
    ) {
        None => -1,
        Some(s) => i32::from(s[0]) | (i32::from(s[1]) << 1),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn thumb(page: i32, total: i32, position: i32, change_pos: bool) -> EeWonThumbIn {
        EeWonThumbIn {
            page,
            total,
            position,
            track_top: 16,
            track_len: 100,
            thumb_top: 20,
            thumb_len: 10,
            resize_thumb: true,
            min_thumb: 8,
            change_pos,
        }
    }

    #[test]
    fn thumbs() {
        assert!(thumb_layout(thumb(10, 10, 0, true)).hidden);
        assert_eq!(
            thumb_layout(thumb(50, 100, 25, true)),
            EeWonThumbOut { hidden: false, set_pos: true, pos: 41, len: 50 }
        );
        assert_eq!(
            thumb_layout(thumb(1, 1000, 0, false)),
            EeWonThumbOut { hidden: false, set_pos: false, pos: 20, len: 8 }
        );
        // pushed back inside the track: moved even when not asked
        let mut t = thumb(50, 100, 0, false);
        t.thumb_top = 90;
        assert_eq!(thumb_layout(t), EeWonThumbOut { hidden: false, set_pos: true, pos: 66, len: 50 });
        assert_eq!(thumb_drag(0, 16, 100, 20, 200, 50), (16, 0));
        assert_eq!(thumb_drag(56, 16, 100, 20, 200, 50), (56, 75));
        assert_eq!(thumb_drag(500, 16, 100, 20, 200, 50), (96, 150));
        assert_eq!(thumb_drag(30, 16, 10, 20, 200, 50), (6, 0));
        let s = |press, mouse, position| scroll_step(press, mouse, 40, 10, position, 3, 4, 25, 100, 30);
        assert_eq!(s(EeWonScrollPress::LineUp, 0, 2), 0);
        assert_eq!(s(EeWonScrollPress::LineDown, 0, 68), 70);
        assert_eq!(s(EeWonScrollPress::Track, 39, 30), 5);
        assert_eq!(s(EeWonScrollPress::Track, 45, 30), 30);
        assert_eq!(s(EeWonScrollPress::Track, 50, 30), 55);
        assert_eq!(scroll_step(EeWonScrollPress::LineDown, 0, 0, 0, 5, 1, 1, 1, 10, 30), 0);
    }

    #[test]
    fn areas() {
        let a = EeWonScrollArea {
            vert_offset: 90,
            horz_offset: -4,
            total_width: 50,
            total_height: 100,
            extra_height: 5,
            view_x: 0,
            view_y: 0,
            view_width: 40,
            view_height: 30,
        };
        assert_eq!(clamp_offsets(&a), (75, 0));
        assert!(on_bottom(&a));
        let v = view_layout(&a, 60, 50, 2, 3, 4, 5);
        assert_eq!((v.view_x, v.view_y, v.view_width, v.view_height, v.vert_offset), (2, 3, 54, 42, 63));
        let v = view_layout(&EeWonScrollArea { vert_offset: 0, ..a }, 1, 1, 2, 3, 4, 5);
        assert_eq!((v.view_width, v.view_height, v.vert_offset), (0, 0, 0));
        assert_eq!(desired_size(299, 300), (299, 300));
        assert_eq!(desired_size(1000, -3), (300, -3));
        assert_eq!(size_equal_to_total(&a, 60, 10, [1, 2, 3, 4]), (64, 106));
        assert_eq!(scrollbar_condition(2, true, true, true, &a), None);
        assert_eq!(scrollbar_condition(2, false, true, true, &a), Some(Some(true)));
        assert_eq!(scrollbar_condition(2, true, false, false, &a), Some(Some(true)));
        assert_eq!(scrollbar_condition(0, true, true, false, &a), Some(Some(false)));
        assert_eq!(scrollbar_condition(7, false, true, false, &a), Some(None));
        assert_eq!(scrollbar_sync(&a, true), (105, 30, true));
        assert_eq!(scrollbar_sync(&a, false), (50, 40, true));
        assert_eq!(scroll_buttons_enabled(&a), [false, true, true, false]);
        assert_eq!(scroll_button_condition(2, false, [false, false], [true, false]), Some([true, true]));
        assert_eq!(scroll_button_condition(3, false, [false, false], [true, false]), Some([false, true]));
        assert_eq!(scroll_button_condition(3, true, [true, false], [true, false]), None);
        assert_eq!(scroll_button_condition(9, true, [true, false], [true, true]), Some([true, false]));
        assert_eq!(ee_won_gui_scroll_button_condition(1, false, false, false, false, false), 3);
        assert_eq!(ee_won_gui_scrollbar_condition(2, true, true, true, a), -1);
    }
}
