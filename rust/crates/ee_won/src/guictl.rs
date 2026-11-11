//! WONGUI's compound controls' rules (ComboBox.cpp, TabCtrl.cpp, Splitter.cpp: the lobby's drop lists, its tabbed
//! pages and its split chat and lists). The C++ keeps the controls, their children, popups, events, focus and
//! the paint; these answer the numbers and choices:
//!
//! - ComboBox: the popup list's size (`CalcListBoxSize`), where the input box's selection goes after it takes the
//!   list's text (`SetInputBoxToListSelection`), what a key does (`KeyDown`), the list item matching a text
//!   (`ShowList`, `SetTextAndSel`);
//! - TabBar / TabCtrl: a tab's x (`PositionTab`), the neighbour a disabled tab hands over to (`EnableTab`), the
//!   tab bar's scroll and its arrows' states (`AdjustTabBarScroll`, `KeepTabButtonInView`), the page's and the
//!   bars' places (`PositionTabContainer`, `CalcContainerLayout`, `PositionArrows`, `VertPositionTabBar`,
//!   `HorzPositionTabBar`), the bar height a first tab sets (`AddTab`), the desired size, the tab the arrow
//!   keys select (`KeyDown`) and the tab selected after a removal (`RemoveTab`);
//! - Splitter: the split a maximised quadrant sets (`MaxQuadrant`, `Virtual08`), the clamp and the bars' edges
//!   (`SetSplitterPos`), the remembered percentages, the panes' and bars' rectangles (`CalcQuadrants`,
//!   `CalcSplitterPositions`), the drag handle's cursor (`AddSplitterPrv`), the drag (`MouseDown`,
//!   `MouseDrag`) and the maximise buttons (`RegisterMaxButton`, `CheckMaxButton`).
//!
//! Ints wrap (the game builds with -fwrapv); a double becomes an int as x86's `cvttsd2si` made it.

/// A rectangle (x, y, width, height).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonRect4 {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

/// A double to an int as the original's compiled conversion (x86 `cvttsd2si`): truncation, and the "integer
/// indefinite" 0x80000000 for NaN and values outside the int range.
pub fn trunc_i32(d: f64) -> i32 {
    if d.is_nan() || d >= 2_147_483_648.0 || d <= -2_147_483_649.0 {
        i32::MIN
    } else {
        d as i32
    }
}

// ---- ComboBox

/// `CalcListBoxSize`: the configured size, no wider than the list's content (the box's frame plus the list's
/// padding and items), no narrower than the combo box, as tall as the items and 4 more, at most the configured
/// height.
#[allow(clippy::too_many_arguments)]
pub fn combo_list_size(
    max_width: i32,
    max_height: i32,
    box_width: i32,
    area_width: i32,
    right_pad: i32,
    left_pad: i32,
    total_width: i32,
    combo_width: i32,
    total_height: i32,
) -> (i32, i32) {
    let mut w = max_width;
    let content =
        box_width.wrapping_sub(area_width).wrapping_add(right_pad).wrapping_add(left_pad).wrapping_add(total_width);
    if w > content {
        w = content;
    }
    if w < combo_width {
        w = combo_width;
    }
    let mut h = total_height.wrapping_add(4);
    if h > max_height {
        h = max_height;
    }
    (w, h)
}

/// `SetInputBoxToListSelection`: the cursor goes to the text's end (else the whole text is selected) when the
/// item is wider than the input box less 10.
pub fn combo_select_to_end(item_width: i32, edit_width: i32) -> bool {
    item_width > edit_width.wrapping_sub(10)
}

/// What `ComboBox::KeyDown` does with a key (the Win32 virtual keys): 1 opens the list (Down while closed),
/// 2 moves the open list's selection (Down, Up, Page Up, Page Down), 3 takes the selection and closes (Enter),
/// 4 restores the earlier text and closes (Escape), 0 passes the key to the container.
pub fn combo_key(key: i32, visible: bool) -> i32 {
    if key == 40 && !visible {
        1
    } else if visible && matches!(key, 40 | 38 | 33 | 34) {
        2
    } else if visible && key == 13 {
        3
    } else if key == 27 && visible {
        4
    } else {
        0
    }
}

/// The first of `count` items `matches(i)` accepts (`ShowList`'s and `SetTextAndSel`'s searches).
pub fn first_match(count: usize, matches: impl Fn(usize) -> bool) -> Option<usize> {
    (0..count).find(|&i| matches(i))
}

// ---- TabBar / TabCtrl

/// `PositionTab`: the tab's x, after the previous tab overlapped by `overlap` (the first tab at 0).
pub fn tab_x(id: i32, count: usize, prev_right: i32, overlap: i32) -> i32 {
    if id > 0 && id as usize <= count {
        prev_right.wrapping_sub(overlap)
    } else {
        0
    }
}

/// `EnableTab`: the tab selected when the selected tab `old` is disabled (left, or right for the first).
pub fn tab_neighbour(old: i32) -> i32 {
    if old > 0 {
        old - 1
    } else {
        old.wrapping_add(1)
    }
}

/// `KeepTabButtonInView`: the scroll that shows the tab (left edge `left` in the bar's container, `width` wide)
/// in a container `limit` wide, its left edge first when it does not fit; `None` when it is in view.
pub fn tab_keep_in_view(left: i32, width: i32, limit: i32) -> Option<i32> {
    let right = left.wrapping_add(width);
    if left >= 0 && width <= limit {
        return (right > limit).then(|| limit.wrapping_sub(right));
    }
    Some(left.wrapping_neg())
}

/// `AdjustTabBarScroll`: the bar's new x after scrolling `amount` from `old_x` (a bar `width` wide in a
/// container `container_width` wide, kept 5 pixels in from the right), and the arrows' enabled states (left,
/// right) when the tabs overflow (`None`: everything fits, the bar at 0 and the arrows left as they are).
pub fn tab_scroll(old_x: i32, amount: i32, width: i32, container_width: i32) -> (i32, Option<(bool, bool)>) {
    let mut x = old_x.wrapping_add(amount);
    if container_width >= width {
        return (0, None);
    }
    let (mut left, mut right) = (true, true);
    if x.wrapping_add(width) < container_width.wrapping_sub(5) {
        right = false;
        x = container_width.wrapping_sub(width).wrapping_sub(5);
    }
    if x >= 0 {
        left = false;
        x = 0;
    }
    (x, Some((left, right)))
}

/// `PositionTabContainer`: the page inside the padding of the page area (`area_top`, `area_height`).
pub fn tab_page_rect(pads: [i32; 4], area_top: i32, area_height: i32, width: i32) -> EeWonRect4 {
    let [left, right, top, bottom] = pads;
    EeWonRect4 {
        x: left,
        y: area_top.wrapping_add(top),
        w: width.wrapping_sub(right).wrapping_sub(left),
        h: area_height.wrapping_sub(bottom).wrapping_sub(top),
    }
}

/// `CalcContainerLayout`: the page area (top, height): above the bottom tab bar or below the top one, the
/// overlap shared.
pub fn tab_area(on_bottom: bool, height: i32, bar_top: i32, bar_bottom: i32, overlap: i32) -> (i32, i32) {
    let (top, bottom) =
        if on_bottom { (0, bar_top.wrapping_add(overlap)) } else { (bar_bottom.wrapping_sub(overlap), height) };
    (top, bottom.wrapping_sub(top))
}

/// `PositionArrows`: the left arrow's place (the right one follows it): at the left end or the right end of the
/// row, below the page area or just above it.
#[allow(clippy::too_many_arguments)]
pub fn tab_arrows(
    on_left: bool,
    on_bottom: bool,
    width: i32,
    right_width: i32,
    left_width: i32,
    left_height: i32,
    area_top: i32,
    area_bottom: i32,
) -> (i32, i32) {
    let x = if on_left { 0 } else { width.wrapping_sub(right_width).wrapping_sub(left_width) };
    let y = if on_bottom { area_bottom } else { area_top.wrapping_sub(left_height) };
    (x, y)
}

/// `VertPositionTabBar`: the bottom bar's height.
pub fn tab_bottom_height(bar_height: i32, overlap: i32, top_offset: i32) -> i32 {
    bar_height.wrapping_sub(overlap).wrapping_sub(top_offset)
}

/// `VertPositionTabBar`: the top container's y and the bottom container's y, from the bottom container's height
/// (as its `SetSize` left it).
pub fn tab_vert(on_bottom: bool, height: i32, bottom_height: i32, top_offset: i32) -> (i32, i32) {
    let (mut y, mut off) = (0, top_offset);
    if on_bottom {
        y = height.wrapping_sub(bottom_height);
        off = off.wrapping_neg();
    }
    (y, off.wrapping_add(y))
}

/// `HorzPositionTabBar`: the containers' (left, right): room for a visible arrow pair when the tabs overflow,
/// else the bar against the right edge when it goes there.
#[allow(clippy::too_many_arguments)]
pub fn tab_horz(
    need_arrows: bool,
    arrows_on_left: bool,
    right_arrow: Option<i32>,
    left_arrow: Option<i32>,
    on_right: bool,
    width: i32,
    bar_width: i32,
    left_offset: i32,
) -> (i32, i32) {
    let (mut left, mut right) = (0, width);
    if need_arrows {
        if arrows_on_left {
            if let Some(r) = right_arrow {
                left = r;
            }
        } else if let Some(l) = left_arrow {
            right = l.wrapping_sub(1);
        }
    } else if on_right {
        left = width.wrapping_sub(bar_width).wrapping_sub(left_offset);
    }
    (left, right)
}

/// `AddTab`: the bar height (the first tab's, 20 when it has none; kept once set).
pub fn tab_bar_height(current: i32, tab_height: i32) -> i32 {
    if current != 0 {
        current
    } else if tab_height > 0 {
        tab_height
    } else {
        20
    }
}

/// `TabCtrl::KeyDown`: the tab Right (39) or Left (37) selects from `old`: the next one, past disabled tabs while
/// inside (`disabled(i)` as `TabBar::Disabled`); `None` for another key.
pub fn tab_key_target(key: i32, old: i32, count: usize, disabled: impl Fn(i32) -> bool) -> Option<i32> {
    let mut step = match key {
        39 => 1,
        37 => -1,
        _ => return None,
    };
    let mut next = old.wrapping_add(step);
    // the original compared with the page count less one as unsigned
    while disabled(next) && next > 0 && (next as u32 as usize) < count.wrapping_sub(1) {
        if key == 39 {
            step = step.wrapping_add(1);
            next = next.wrapping_add(1);
        } else {
            step = step.wrapping_sub(1);
            next = next.wrapping_sub(1);
        }
    }
    Some(old.wrapping_add(step))
}

/// `RemoveTab`: the tab selected after tab `removed` goes (the selected one was `current`).
pub fn tab_after_remove(removed: i32, current: i32) -> i32 {
    if removed < current {
        current - 1
    } else if removed > current {
        current
    } else {
        0
    }
}

// ---- Splitter

pub const POSITION_LEFT: i32 = 1;
pub const POSITION_TOP: i32 = 2;
pub const POSITION_RIGHT: i32 = 4;
pub const POSITION_BOTTOM: i32 = 8;
pub const SPLITTER_LEFT_RIGHT: i32 = 5;
pub const SPLITTER_UP_DOWN: i32 = 10;

/// `MaxQuadrant`: the split (x, y) that maximises quadrant `pos` (bars half off the edge) and whether each axis
/// is maximised.
pub fn split_max(
    pos: i32,
    width: i32,
    height: i32,
    bar_w: i32,
    bar_h: i32,
    sx: i32,
    sy: i32,
) -> (i32, i32, bool, bool) {
    let (mut x, mut y, mut lr, mut ud) = (sx, sy, true, true);
    if pos & POSITION_LEFT != 0 {
        x = width.wrapping_sub(bar_w / 2);
    } else if pos & POSITION_RIGHT != 0 {
        x = bar_w / 2;
    } else {
        lr = false;
    }
    if pos & POSITION_TOP != 0 {
        y = height.wrapping_sub(bar_h / 2);
    } else if pos & POSITION_BOTTOM != 0 {
        y = bar_h / 2;
    } else {
        ud = false;
    }
    (x, y, lr, ud)
}

/// The quadrant a maximise button stands for (its control id), as `MaxQuadrant` and `CheckMaxButton` read it.
pub fn split_button_quadrant(control_id: i32) -> i32 {
    control_id.wrapping_sub(100006).wrapping_add(POSITION_LEFT)
}

/// `CheckMaxButton`'s id range.
pub fn split_is_max_button(control_id: i32) -> bool {
    (100006..=100017).contains(&control_id)
}

/// The split's state (`SetSplitterPos`): the point and the bars' edges.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonSplit {
    pub x: i32,
    pub y: i32,
    pub left: i32,
    pub right: i32,
    pub top: i32,
    pub bottom: i32,
}

/// `SetSplitterPos`: the point clamped to the minimum panes (not on a maximised axis) and the bars' edges;
/// `None` when the split is valid and unchanged. `mins` is (left, right, top, bottom).
#[allow(clippy::too_many_arguments)]
pub fn split_set(
    x: i32,
    y: i32,
    lr_max: bool,
    ud_max: bool,
    mins: [i32; 4],
    width: i32,
    height: i32,
    valid: bool,
    sx: i32,
    sy: i32,
    bar_w: i32,
    bar_h: i32,
) -> Option<EeWonSplit> {
    let (mut x, mut y) = (x, y);
    if !lr_max {
        if x < mins[0] {
            x = mins[0];
        }
        if x > width.wrapping_sub(mins[1]) {
            x = width.wrapping_sub(mins[1]);
        }
    }
    if !ud_max {
        if y < mins[2] {
            y = mins[2];
        }
        if y > height.wrapping_sub(mins[3]) {
            y = height.wrapping_sub(mins[3]);
        }
    }
    if valid && x == sx && y == sy {
        return None;
    }
    Some(EeWonSplit {
        x,
        y,
        left: x.wrapping_sub(bar_w / 2),
        right: x.wrapping_add(bar_w / 2),
        top: y.wrapping_sub(bar_h / 2),
        bottom: y.wrapping_add(bar_h / 2),
    })
}

/// `SetSplitterPos`'s remembered percentage of `v` in `size` (none when the size is not positive).
pub fn split_percent(v: i32, size: i32) -> Option<f64> {
    (size > 0).then(|| f64::from(v) * 100.0 / f64::from(size))
}

/// The split for a size and percentage (`Virtual08`, `UnMaxQuadrant`): `size * percent / 100 + offset` in
/// doubles, then to an int.
pub fn split_at(size: i32, percent: f64, offset: i32) -> i32 {
    trunc_i32(f64::from(size) * percent / 100.0 + f64::from(offset))
}

/// `Virtual08`'s split: the stored percentages, or the edge of the maximised quadrant (bars half off it; the
/// original negated the bar's size before halving it).
#[allow(clippy::too_many_arguments)]
pub fn split_layout(
    have_max: bool,
    quadrant: i32,
    left_pct: f64,
    top_pct: f64,
    bar_w: i32,
    bar_h: i32,
    width: i32,
    height: i32,
) -> (i32, i32) {
    if !have_max {
        return (split_at(width, left_pct, 0), split_at(height, top_pct, 0));
    }
    let (mut lp, mut tp, mut xo, mut yo) = (left_pct, top_pct, 0, 0);
    if quadrant & POSITION_LEFT != 0 {
        lp = 100.0;
        xo = bar_w.wrapping_neg() / 2;
    } else if quadrant & POSITION_RIGHT != 0 {
        lp = 0.0;
        xo = bar_w / 2;
    }
    if quadrant & POSITION_TOP != 0 {
        tp = 100.0;
        yo = bar_h.wrapping_neg() / 2;
    } else if quadrant & POSITION_BOTTOM != 0 {
        tp = 0.0;
        yo = bar_h / 2;
    }
    (split_at(width, lp, xo), split_at(height, tp, yo))
}

/// `CalcQuadrants`: a pane's rectangle on its side of the split.
pub fn split_pane(pos: i32, width: i32, height: i32, s: &EeWonSplit) -> EeWonRect4 {
    let (mut l, mut r, mut t, mut b) = (0, width, 0, height);
    if pos & POSITION_LEFT != 0 {
        r = s.left;
    }
    if pos & POSITION_RIGHT != 0 {
        l = s.right;
    }
    if pos & POSITION_TOP != 0 {
        b = s.top;
    }
    if pos & POSITION_BOTTOM != 0 {
        t = s.bottom;
    }
    EeWonRect4 { x: l, y: t, w: r.wrapping_sub(l), h: b.wrapping_sub(t) }
}

/// `CalcSplitterPositions`: a bar's rectangle along the split (half bars only on their side).
pub fn split_bar(kind: i32, width: i32, height: i32, s: &EeWonSplit) -> EeWonRect4 {
    let (mut l, mut t, mut r, mut b) = (0, 0, width, height);
    if kind & SPLITTER_LEFT_RIGHT != 0 {
        l = s.left;
        r = s.right;
    }
    if kind & SPLITTER_UP_DOWN != 0 {
        t = s.top;
        b = s.bottom;
    }
    match kind {
        0x15 => b = s.top,
        0x25 => t = s.bottom,
        0x4a => r = s.left,
        0x8a => l = s.right,
        _ => {}
    }
    EeWonRect4 { x: l, y: t, w: r.wrapping_sub(l), h: b.wrapping_sub(t) }
}

/// `AddSplitterPrv`'s default cursor for a drag handle: 1 the four-way one (the centre), 2 east-west, 3
/// north-south, 0 none.
pub fn split_cursor(kind: i32) -> i32 {
    if kind == 0x10f {
        1
    } else if kind & SPLITTER_LEFT_RIGHT != 0 {
        2
    } else if kind & SPLITTER_UP_DOWN != 0 {
        3
    } else {
        0
    }
}

/// `MouseDrag`'s new split point from the dragged bar's (`child_x`, `child_y`) and the grab offset.
#[allow(clippy::too_many_arguments)]
pub fn split_drag(
    x: i32,
    y: i32,
    child_x: i32,
    child_y: i32,
    drag_x: i32,
    drag_y: i32,
    lr: bool,
    ud: bool,
    sx: i32,
    sy: i32,
) -> (i32, i32) {
    let dx = if lr { x.wrapping_sub(child_x).wrapping_sub(drag_x) } else { 0 };
    let dy = if ud { y.wrapping_sub(child_y).wrapping_sub(drag_y) } else { 0 };
    (sx.wrapping_add(dx), sy.wrapping_add(dy))
}

// ---- the C ABI

/// `combo_list_size` into the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_combo_list_size(
    max_width: i32,
    max_height: i32,
    box_width: i32,
    area_width: i32,
    right_pad: i32,
    left_pad: i32,
    total_width: i32,
    combo_width: i32,
    total_height: i32,
    out_width: *mut i32,
    out_height: *mut i32,
) {
    let (w, h) = combo_list_size(
        max_width,
        max_height,
        box_width,
        area_width,
        right_pad,
        left_pad,
        total_width,
        combo_width,
        total_height,
    );
    // SAFETY: writable (the contract)
    unsafe {
        *out_width = w;
        *out_height = h;
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_combo_select_to_end(item_width: i32, edit_width: i32) -> bool {
    combo_select_to_end(item_width, edit_width)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_combo_key(key: i32, visible: bool) -> i32 {
    combo_key(key, visible)
}

/// Whether item `i` (below the count) matches, read from the C++ list.
pub type EeWonMatchOf = extern "C" fn(ctx: *const std::ffi::c_void, i: usize) -> bool;

/// `first_match`: the index, or -1.
#[no_mangle]
pub extern "C" fn ee_won_gui_first_match(count: usize, matches: EeWonMatchOf, ctx: *const std::ffi::c_void) -> i64 {
    first_match(count, |i| matches(ctx, i)).map_or(-1, |i| i as i64)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tab_x(id: i32, count: usize, prev_right: i32, overlap: i32) -> i32 {
    tab_x(id, count, prev_right, overlap)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tab_neighbour(old: i32) -> i32 {
    tab_neighbour(old)
}

/// `tab_keep_in_view`: false when in view, else true with the scroll in `out`.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tab_keep_in_view(left: i32, width: i32, limit: i32, out: *mut i32) -> bool {
    match tab_keep_in_view(left, width, limit) {
        Some(a) => {
            // SAFETY: writable (the contract)
            unsafe { *out = a };
            true
        }
        None => false,
    }
}

/// `tab_scroll`: the new x; true with the arrows' states in the outs when the tabs overflow.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tab_scroll(
    old_x: i32,
    amount: i32,
    width: i32,
    container_width: i32,
    out_x: *mut i32,
    out_left: *mut bool,
    out_right: *mut bool,
) -> bool {
    let (x, arrows) = tab_scroll(old_x, amount, width, container_width);
    // SAFETY: writable (the contract)
    unsafe {
        *out_x = x;
        if let Some((l, r)) = arrows {
            *out_left = l;
            *out_right = r;
            return true;
        }
    }
    false
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tab_page_rect(
    left_pad: i32,
    right_pad: i32,
    top_pad: i32,
    bottom_pad: i32,
    area_top: i32,
    area_height: i32,
    width: i32,
) -> EeWonRect4 {
    tab_page_rect([left_pad, right_pad, top_pad, bottom_pad], area_top, area_height, width)
}

/// `tab_area`: the top, the height into `out_height`.
///
/// # Safety
/// `out_height` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tab_area(
    on_bottom: bool,
    height: i32,
    bar_top: i32,
    bar_bottom: i32,
    overlap: i32,
    out_height: *mut i32,
) -> i32 {
    let (t, h) = tab_area(on_bottom, height, bar_top, bar_bottom, overlap);
    // SAFETY: writable (the contract)
    unsafe { *out_height = h };
    t
}

/// `tab_arrows`: the x, the y into `out_y`.
///
/// # Safety
/// `out_y` is writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_tab_arrows(
    on_left: bool,
    on_bottom: bool,
    width: i32,
    right_width: i32,
    left_width: i32,
    left_height: i32,
    area_top: i32,
    area_bottom: i32,
    out_y: *mut i32,
) -> i32 {
    let (x, y) = tab_arrows(on_left, on_bottom, width, right_width, left_width, left_height, area_top, area_bottom);
    // SAFETY: writable (the contract)
    unsafe { *out_y = y };
    x
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tab_bottom_height(bar_height: i32, overlap: i32, top_offset: i32) -> i32 {
    tab_bottom_height(bar_height, overlap, top_offset)
}

/// `tab_vert`: the top container's y, the bottom container's into `out_bottom_y`.
///
/// # Safety
/// `out_bottom_y` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tab_vert(
    on_bottom: bool,
    height: i32,
    bottom_height: i32,
    top_offset: i32,
    out_bottom_y: *mut i32,
) -> i32 {
    let (a, b) = tab_vert(on_bottom, height, bottom_height, top_offset);
    // SAFETY: writable (the contract)
    unsafe { *out_bottom_y = b };
    a
}

/// `tab_horz` (an arrow's edge is read only when the arrow is there and visible): the left, the right into
/// `out_right`.
///
/// # Safety
/// `out_right` is writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_tab_horz(
    need_arrows: bool,
    arrows_on_left: bool,
    right_arrow_shown: bool,
    right_arrow_right: i32,
    left_arrow_shown: bool,
    left_arrow_left: i32,
    on_right: bool,
    width: i32,
    bar_width: i32,
    left_offset: i32,
    out_right: *mut i32,
) -> i32 {
    let (l, r) = tab_horz(
        need_arrows,
        arrows_on_left,
        right_arrow_shown.then_some(right_arrow_right),
        left_arrow_shown.then_some(left_arrow_left),
        on_right,
        width,
        bar_width,
        left_offset,
    );
    // SAFETY: writable (the contract)
    unsafe { *out_right = r };
    l
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tab_bar_height(current: i32, tab_height: i32) -> i32 {
    tab_bar_height(current, tab_height)
}

/// Whether tab `i` (any int) is disabled, as `TabBar::Disabled`.
pub type EeWonDisabledOf = extern "C" fn(ctx: *const std::ffi::c_void, i: i32) -> bool;

/// `tab_key_target`: false for another key, else true with the tab in `out`.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tab_key_target(
    key: i32,
    old: i32,
    count: usize,
    disabled: EeWonDisabledOf,
    ctx: *const std::ffi::c_void,
    out: *mut i32,
) -> bool {
    match tab_key_target(key, old, count, |i| disabled(ctx, i)) {
        Some(t) => {
            // SAFETY: writable (the contract)
            unsafe { *out = t };
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tab_after_remove(removed: i32, current: i32) -> i32 {
    tab_after_remove(removed, current)
}

/// `split_max` into the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_split_max(
    pos: i32,
    width: i32,
    height: i32,
    bar_w: i32,
    bar_h: i32,
    sx: i32,
    sy: i32,
    out_x: *mut i32,
    out_y: *mut i32,
    out_lr: *mut bool,
    out_ud: *mut bool,
) {
    let (x, y, lr, ud) = split_max(pos, width, height, bar_w, bar_h, sx, sy);
    // SAFETY: writable (the contract)
    unsafe {
        *out_x = x;
        *out_y = y;
        *out_lr = lr;
        *out_ud = ud;
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_split_button_quadrant(control_id: i32) -> i32 {
    split_button_quadrant(control_id)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_split_is_max_button(control_id: i32) -> bool {
    split_is_max_button(control_id)
}

/// `split_set`: false when unchanged, else true with the split in `out`.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_split_set(
    x: i32,
    y: i32,
    lr_max: bool,
    ud_max: bool,
    min_left: i32,
    min_right: i32,
    min_top: i32,
    min_bottom: i32,
    width: i32,
    height: i32,
    valid: bool,
    sx: i32,
    sy: i32,
    bar_w: i32,
    bar_h: i32,
    out: *mut EeWonSplit,
) -> bool {
    match split_set(
        x,
        y,
        lr_max,
        ud_max,
        [min_left, min_right, min_top, min_bottom],
        width,
        height,
        valid,
        sx,
        sy,
        bar_w,
        bar_h,
    ) {
        Some(s) => {
            // SAFETY: writable (the contract)
            unsafe { *out = s };
            true
        }
        None => false,
    }
}

/// `split_percent`: false when the size is not positive, else true with the percentage in `out`.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_split_percent(v: i32, size: i32, out: *mut f64) -> bool {
    match split_percent(v, size) {
        Some(p) => {
            // SAFETY: writable (the contract)
            unsafe { *out = p };
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_split_at(size: i32, percent: f64, offset: i32) -> i32 {
    split_at(size, percent, offset)
}

/// `split_layout`: the x, the y into `out_y`.
///
/// # Safety
/// `out_y` is writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_split_layout(
    have_max: bool,
    quadrant: i32,
    left_pct: f64,
    top_pct: f64,
    bar_w: i32,
    bar_h: i32,
    width: i32,
    height: i32,
    out_y: *mut i32,
) -> i32 {
    let (x, y) = split_layout(have_max, quadrant, left_pct, top_pct, bar_w, bar_h, width, height);
    // SAFETY: writable (the contract)
    unsafe { *out_y = y };
    x
}

#[no_mangle]
pub extern "C" fn ee_won_gui_split_pane(pos: i32, width: i32, height: i32, s: EeWonSplit) -> EeWonRect4 {
    split_pane(pos, width, height, &s)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_split_bar(kind: i32, width: i32, height: i32, s: EeWonSplit) -> EeWonRect4 {
    split_bar(kind, width, height, &s)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_split_cursor(kind: i32) -> i32 {
    split_cursor(kind)
}

/// `split_drag`: the x, the y into `out_y`.
///
/// # Safety
/// `out_y` is writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_split_drag(
    x: i32,
    y: i32,
    child_x: i32,
    child_y: i32,
    drag_x: i32,
    drag_y: i32,
    lr: bool,
    ud: bool,
    sx: i32,
    sy: i32,
    out_y: *mut i32,
) -> i32 {
    let (a, b) = split_drag(x, y, child_x, child_y, drag_x, drag_y, lr, ud, sx, sy);
    // SAFETY: writable (the contract)
    unsafe { *out_y = b };
    a
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn combo() {
        // the content (60 - 50 + 2 + 2 + 120 = 134) caps the configured 200, the combo's 150 floors it
        assert_eq!(combo_list_size(200, 150, 60, 50, 2, 2, 120, 100, 300), (134, 150));
        assert_eq!(combo_list_size(200, 150, 60, 50, 2, 2, 120, 150, 20), (150, 24));
        assert!(combo_select_to_end(95, 100) && !combo_select_to_end(90, 100));
        assert_eq!(combo_key(40, false), 1);
        assert_eq!(combo_key(40, true), 2);
        assert_eq!(combo_key(33, true), 2);
        assert_eq!(combo_key(33, false), 0);
        assert_eq!(combo_key(13, true), 3);
        assert_eq!(combo_key(27, true), 4);
        assert_eq!(combo_key(27, false), 0);
        assert_eq!(first_match(4, |i| i == 2), Some(2));
        assert_eq!(first_match(4, |_| false), None);
    }

    #[test]
    fn tabs() {
        assert_eq!(tab_x(0, 3, 50, 4), 0);
        assert_eq!(tab_x(2, 3, 50, 4), 46);
        assert_eq!(tab_x(4, 3, 50, 4), 0);
        assert_eq!((tab_neighbour(0), tab_neighbour(3)), (1, 2));
        assert_eq!(tab_keep_in_view(10, 20, 100), None);
        assert_eq!(tab_keep_in_view(90, 20, 100), Some(-10));
        assert_eq!(tab_keep_in_view(-5, 20, 100), Some(5));
        assert_eq!(tab_keep_in_view(10, 200, 100), Some(-10));
        assert_eq!(tab_scroll(-30, 10, 100, 200), (0, None));
        assert_eq!(tab_scroll(-30, 10, 300, 200), (-20, Some((true, true))));
        assert_eq!(tab_scroll(-30, 40, 300, 200), (0, Some((false, true))));
        assert_eq!(tab_scroll(-30, -100, 300, 200), (-105, Some((true, false))));
        assert_eq!(tab_page_rect([5, 5, 5, 5], 20, 100, 200), EeWonRect4 { x: 5, y: 25, w: 190, h: 90 });
        assert_eq!(tab_area(false, 300, 0, 20, 1), (19, 281));
        assert_eq!(tab_area(true, 300, 280, 300, 1), (0, 281));
        assert_eq!(tab_arrows(false, false, 300, 10, 12, 15, 19, 300), (278, 4));
        assert_eq!(tab_bottom_height(20, 1, 3), 16);
        assert_eq!(tab_vert(false, 300, 16, 3), (0, 3));
        assert_eq!(tab_vert(true, 300, 16, 3), (284, 281));
        assert_eq!(tab_horz(true, true, Some(30), None, false, 300, 0, 2), (30, 300));
        assert_eq!(tab_horz(true, false, None, Some(270), false, 300, 0, 2), (0, 269));
        assert_eq!(tab_horz(true, false, None, None, true, 300, 100, 2), (0, 300));
        assert_eq!(tab_horz(false, false, None, None, true, 300, 100, 2), (198, 300));
        assert_eq!((tab_bar_height(0, 0), tab_bar_height(0, 24), tab_bar_height(18, 24)), (20, 24, 18));
        let off = [false, true, true, false];
        let dis = |i: i32| !(0..4).contains(&i) || off[i as usize];
        assert_eq!(tab_key_target(39, 0, 4, dis), Some(3));
        assert_eq!(tab_key_target(37, 3, 4, dis), Some(0));
        assert_eq!(tab_key_target(37, 0, 4, dis), Some(-1));
        assert_eq!(tab_key_target(13, 0, 4, dis), None);
        assert_eq!((tab_after_remove(1, 3), tab_after_remove(4, 3), tab_after_remove(3, 3)), (2, 3, 0));
    }

    #[test]
    fn splitter() {
        assert_eq!(split_max(1, 300, 200, 6, 4, 10, 10), (297, 10, true, false));
        assert_eq!(split_max(12, 300, 200, 6, 4, 10, 10), (3, 2, true, true));
        assert_eq!(split_max(0, 300, 200, 6, 4, 10, 11), (10, 11, false, false));
        assert_eq!(split_button_quadrant(100008), 3);
        assert!(split_is_max_button(100006) && split_is_max_button(100017) && !split_is_max_button(100018));
        let s = split_set(5, 150, false, false, [20, 20, 0, 60], 300, 200, false, 0, 0, 6, 4).unwrap();
        assert_eq!(s, EeWonSplit { x: 20, y: 140, left: 17, right: 23, top: 138, bottom: 142 });
        assert_eq!(split_set(20, 140, false, false, [20, 20, 0, 60], 300, 200, true, 20, 140, 6, 4), None);
        assert_eq!(split_set(-5, 150, true, true, [20, 20, 0, 60], 300, 200, true, 20, 140, 6, 4).unwrap().x, -5);
        assert_eq!(split_percent(150, 300), Some(50.0));
        assert_eq!(split_percent(150, 0), None);
        assert_eq!(split_at(301, 50.0, 0), 150);
        assert_eq!(split_at(300, f64::NAN, 0), i32::MIN);
        assert_eq!(split_at(i32::MAX, 300.0, 0), i32::MIN);
        assert_eq!(split_layout(false, 0, 50.0, 25.0, 6, 4, 300, 200), (150, 50));
        assert_eq!(split_layout(true, 3, 50.0, 25.0, 6, 4, 300, 200), (297, 198));
        assert_eq!(split_layout(true, 12, 50.0, 25.0, 6, 4, 300, 200), (3, 2));
        assert_eq!(split_pane(3, 300, 200, &s), EeWonRect4 { x: 0, y: 0, w: 17, h: 138 });
        assert_eq!(split_pane(12, 300, 200, &s), EeWonRect4 { x: 23, y: 142, w: 277, h: 58 });
        assert_eq!(split_bar(5, 300, 200, &s), EeWonRect4 { x: 17, y: 0, w: 6, h: 200 });
        assert_eq!(split_bar(0x15, 300, 200, &s), EeWonRect4 { x: 17, y: 0, w: 6, h: 138 });
        assert_eq!(split_bar(0x10f, 300, 200, &s), EeWonRect4 { x: 17, y: 138, w: 6, h: 4 });
        assert_eq!((split_cursor(0x10f), split_cursor(5), split_cursor(10), split_cursor(0)), (1, 2, 3, 0));
        assert_eq!(split_drag(50, 60, 40, 30, 2, 3, true, false, 100, 100), (108, 100));
    }
}
