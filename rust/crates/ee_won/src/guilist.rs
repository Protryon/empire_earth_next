//! WONGUI's list lines and their selection (ListArea.cpp). The C++ keeps the list, its items (`SmartPtr`s in a
//! `std::vector`), their measuring, painting, sorting and events; these answer, from the items' tops, heights and
//! flags:
//!
//! - the line at a y (`FindLineAt`, the STL's `lower_bound` walk exactly, so an unsorted batch finds what the
//!   original found), the item at a y (`FindItemAt`), whether an item is in view (`IsItemVisible`) and the
//!   scroll that shows it (`EnsureVisible`);
//! - the items stacked top to bottom and the totals (`EnforceInvariants`), the full-line scroll's extra height
//!   (`AdjustPosition`);
//! - the first selectable item from an index (`FindSelectableItem`), a selected range's bounds (`SelectRange`),
//!   the keys' target line and the scroll that follows it (`KeyDown`);
//! - the scroll an insert, a refresh or a delete leaves (`InsertItem`, `RefreshItem`, `DeleteItemPrv`), a
//!   batch insert's provisional top, a refresh's new place, a deleted range's bounds (`DeleteRange`);
//! - whether an item wider than the list shows its tool tip (`MouseMove`).
//!
//! The list flags are the original's: 0x40 full-line scroll, 0x80 partial lines count as visible, 0x800 the
//! selection wraps round, 0x1000 the view never follows inserts and deletes.

use std::ffi::c_void;

/// An item's top (`i` below the count), read from the C++ list.
pub type EeWonTopOf = extern "C" fn(ctx: *const c_void, i: usize) -> i32;
/// Whether item `i` (below the count) is selectable.
pub type EeWonSelectableOf = extern "C" fn(ctx: *const c_void, i: usize) -> bool;

/// `std::lower_bound` over the tops (`top < y`), the walk the STL's implementation makes: the first index whose
/// top is not below `y` when the tops are sorted.
pub fn lower_bound(count: usize, y: i32, top: impl Fn(usize) -> i32) -> usize {
    let mut first = 0usize;
    let mut n = count;
    while n > 0 {
        let step = n / 2;
        let mid = first + step;
        if top(mid) < y {
            first = mid + 1;
            n -= step + 1;
        } else {
            n = step;
        }
    }
    first
}

/// `FindLineAt`: the item whose line covers `y`, the one before when `y` falls between items or past the last
/// (`count` for an empty list).
pub fn find_line_at(count: usize, y: i32, top: impl Fn(usize) -> i32) -> usize {
    let mut i = lower_bound(count, y, &top);
    if (i == count || top(i) > y) && i != 0 {
        i -= 1;
    }
    i
}

/// `FindItemAt`'s test of the line found: `y` is not below the item's bottom.
pub fn covers(top: i32, height: i32, y: i32) -> bool {
    top.wrapping_add(height) >= y
}

/// `IsItemVisible`: not above the view, and its bottom (with `partial`, its top) not below it.
pub fn is_item_visible(top: i32, height: i32, offset: i32, view_height: i32, partial: bool) -> bool {
    if top.wrapping_add(height) < offset {
        return false;
    }
    let last = if partial { top } else { top.wrapping_add(height) };
    last <= offset.wrapping_add(view_height)
}

/// `EnsureVisible`: the offset that shows the item whole (its top wins), `None` when the view stays.
pub fn ensure_visible(top: i32, height: i32, offset: i32, view_height: i32) -> Option<i32> {
    let mut o = offset;
    if top.wrapping_add(height) > offset.wrapping_add(view_height) {
        o = top.wrapping_sub(view_height).wrapping_add(height);
    }
    if o > top {
        o = top;
    }
    if o != offset {
        Some(o)
    } else {
        None
    }
}

/// `EnforceInvariants`: each item's top (stacked from 0 by the heights) into `tops`, and the totals (height,
/// the widest width).
pub fn stack(heights: &[i32], widths: &[i32], tops: &mut [i32]) -> (i32, i32) {
    let (mut h, mut w) = (0i32, 0i32);
    for i in 0..heights.len() {
        tops[i] = h;
        h = h.wrapping_add(heights[i]);
        if widths[i] > w {
            w = widths[i];
        }
    }
    (h, w)
}

/// `AdjustPosition`'s full-line extra height: the room that lets the last page start on the item found at
/// `total - view + 1` (its top and height), at least 0.
pub fn full_line_extra(total_height: i32, view_height: i32, item_top: i32, item_height: i32) -> i32 {
    item_top.wrapping_add(item_height).wrapping_add(view_height).wrapping_sub(total_height).max(0)
}

/// `FindSelectableItem`: the first selectable item from `start` up or down; when none, with `wrap` the far end
/// round (the original's two calls look at one item each: going up, the last item, then the first; going down,
/// the first, then the last), else with `opposite` the other way from `start`. `None`: nothing selectable.
///
/// Found: with `wrap` and neither end selectable the original called itself without end (a stack overflow);
/// this answers `None` there.
pub fn find_selectable(
    count: usize,
    start: i32,
    up: bool,
    opposite: bool,
    wrap: bool,
    selectable: impl Fn(usize) -> bool,
) -> Option<usize> {
    let n = count as i64;
    let walk = |from: i64, up: bool| -> Option<usize> {
        let step = if up { -1 } else { 1 };
        let mut i = from;
        while i >= 0 && i < n {
            if selectable(i as usize) {
                return Some(i as usize);
            }
            i += step;
        }
        None
    };
    if let Some(i) = walk(i64::from(start), up) {
        return Some(i);
    }
    if wrap {
        let (a, b) = if up { (n - 1, 0) } else { (0, n - 1) };
        // each of the original's wrap calls walks one item before it wraps again
        for e in [a, b] {
            if e >= 0 && e < n && selectable(e as usize) {
                return Some(e as usize);
            }
        }
        return None;
    }
    if opposite {
        return walk(i64::from(start), !up);
    }
    None
}

/// `SelectRange(start, end)`'s bounds clamped to the list and its step, `None` for an empty list.
pub fn select_range(start: i32, end: i32, size: i32) -> Option<(i32, i32, i32)> {
    let (mut s, mut e) = (start, end);
    if s >= size {
        s = size.wrapping_sub(1);
    }
    if e >= size {
        e = size.wrapping_sub(1);
    }
    if s < 0 {
        s = 0;
    }
    if e < 0 {
        e = 0;
    }
    if s >= size || e >= size {
        return None;
    }
    Some((s, e, if s <= e { 1 } else { -1 }))
}

/// `KeyDown`'s line to look for from the keyboard item (its top and height): down, up, page down, page up
/// (40, 38, 34, 33), and whether to search up; the selection wraps round at the ends with `wrap`. Right and left
/// (39, 37) scroll sideways and look for no line (-1), but with `wrap` the original's -1 wrapped to the end like
/// any line above the first, so they then move the selection too. `None` for another key.
pub fn key_target(
    key: i32,
    top: i32,
    height: i32,
    view_height: i32,
    total_height: i32,
    wrap: bool,
) -> Option<(i32, bool)> {
    let h = height.wrapping_add(1);
    let mut y = match key {
        40 => top.wrapping_add(h),
        38 => top.wrapping_sub(1),
        34 => top.wrapping_add(view_height),
        33 => top.wrapping_add(h).wrapping_sub(view_height),
        37 | 39 => -1,
        _ => return None,
    };
    let mut up = y < top;
    if wrap {
        if y < 0 {
            y = total_height;
            up = true;
        } else if y >= total_height {
            y = 0;
            up = false;
        }
    }
    Some((y, up))
}

/// `KeyDown`'s scroll to the new keyboard item: its bottom into view (with the extra height), its top wins.
pub fn key_scroll(top: i32, height: i32, offset: i32, view_height: i32, extra_height: i32) -> i32 {
    let mut o = offset;
    if top.wrapping_add(height) > o.wrapping_add(view_height) {
        o = top.wrapping_add(height).wrapping_sub(view_height).wrapping_add(extra_height);
    }
    if top < o {
        o = top;
    }
    o
}

/// The scroll an insert leaves: at the end the view follows the bottom; elsewhere it keeps its items when the
/// insert is above the top line (or the view is on the bottom) and the list overflows. `fixed` (0x1000): never.
#[allow(clippy::too_many_arguments)]
pub fn insert_scroll(
    at_end: bool,
    fixed: bool,
    on_bottom: bool,
    top_line: i32,
    position: i32,
    total_height: i32,
    view_height: i32,
    offset: i32,
    height: i32,
) -> i32 {
    if fixed {
        return offset;
    }
    let follow = if at_end { on_bottom } else { (top_line >= position || on_bottom) && total_height > view_height };
    if follow {
        offset.wrapping_add(height)
    } else {
        offset
    }
}

/// A batch insert's provisional top: just below the item before (0 for the first), at most the next one's top.
pub fn batch_top(prev_top: Option<i32>, next_top: i32) -> i32 {
    let y = prev_top.unwrap_or(0).wrapping_add(1);
    if y > next_top {
        next_top
    } else {
        y
    }
}

/// `RefreshItem`'s new place for an item moved from `position`: `insert` (its sorted place found with it
/// replaced by a neighbour) less one past it, clamped to the list.
pub fn refresh_place(position: i32, insert: i32, size: usize) -> i32 {
    let mut i = insert;
    if position < i {
        i -= 1;
    }
    // the original compared the int with the vector's size_t (a negative one is past the end) and then with 0
    if i as i64 as u64 >= size as u64 {
        i = size as i32 - 1;
    }
    if i < 0 {
        i = 0;
    }
    i
}

/// The scroll a refresh leaves: the view keeps its items when the item is above the view's top (or the view is
/// on the bottom) and the list overflows.
#[allow(clippy::too_many_arguments)]
pub fn refresh_scroll(
    fixed: bool,
    on_bottom: bool,
    top: i32,
    offset: i32,
    total_height: i32,
    view_height: i32,
    height: i32,
    old_height: i32,
) -> i32 {
    if !fixed && (offset >= top || on_bottom) && total_height > view_height {
        offset.wrapping_add(height.wrapping_sub(old_height))
    } else {
        offset
    }
}

/// The scroll a delete leaves: up by the item when it is at or above the view's top.
pub fn delete_scroll(fixed: bool, top: i32, offset: i32, height: i32) -> i32 {
    if !fixed && offset >= top {
        offset.wrapping_sub(height)
    } else {
        offset
    }
}

/// `DeleteRange(start, end)`: (start, amount) of the items to remove, `None` for nothing. A negative or too
/// large end is the list's end.
pub fn delete_range(start: i32, end: i32, size: usize) -> Option<(i32, i32)> {
    let mut e = end;
    if e < 0 || e as u32 as usize > size {
        e = size as i32;
    }
    if start < 0 || e <= start {
        return None;
    }
    let mut amount = e - start;
    if (start + amount) as u32 as usize > size {
        amount = size as i32 - start;
    }
    Some((start, amount))
}

/// `MouseMove`'s item tool tip: the item is wider than the view and its middle is inside it.
pub fn item_tip(width: i32, height: i32, top: i32, view_width: i32, offset: i32, view_height: i32) -> bool {
    if width <= view_width {
        return false;
    }
    let mid = top.wrapping_add(height >> 1);
    mid > offset && mid < offset.wrapping_add(view_height)
}

// ---- the C ABI

/// `find_line_at` over `count` items whose tops `top_of(ctx, i)` gives.
#[no_mangle]
pub extern "C" fn ee_won_gui_list_line_at(count: usize, y: i32, top_of: EeWonTopOf, ctx: *const c_void) -> usize {
    find_line_at(count, y, |i| top_of(ctx, i))
}

#[no_mangle]
pub extern "C" fn ee_won_gui_list_covers(top: i32, height: i32, y: i32) -> bool {
    covers(top, height, y)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_list_visible(top: i32, height: i32, offset: i32, view_height: i32, partial: bool) -> bool {
    is_item_visible(top, height, offset, view_height, partial)
}

/// `ensure_visible`: the new offset, or `offset` when the view stays.
#[no_mangle]
pub extern "C" fn ee_won_gui_list_ensure_visible(top: i32, height: i32, offset: i32, view_height: i32) -> i32 {
    ensure_visible(top, height, offset, view_height).unwrap_or(offset)
}

/// `stack` over `count` items; the totals into the outs.
///
/// # Safety
/// `heights` and `widths` hold, and `tops` has room for, `count` ints (or all are NULL with 0); the outs are
/// writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_list_stack(
    heights: *const i32,
    widths: *const i32,
    tops: *mut i32,
    count: usize,
    out_height: *mut i32,
    out_width: *mut i32,
) {
    let (h, w) = if count == 0 {
        (0, 0)
    } else {
        // SAFETY: `count` entries each (the contract)
        unsafe {
            stack(
                std::slice::from_raw_parts(heights, count),
                std::slice::from_raw_parts(widths, count),
                std::slice::from_raw_parts_mut(tops, count),
            )
        }
    };
    // SAFETY: writable (the contract)
    unsafe {
        *out_height = h;
        *out_width = w;
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_list_full_line_extra(
    total_height: i32,
    view_height: i32,
    item_top: i32,
    item_height: i32,
) -> i32 {
    full_line_extra(total_height, view_height, item_top, item_height)
}

/// `find_selectable`: the index, or -1.
#[no_mangle]
pub extern "C" fn ee_won_gui_list_find_selectable(
    count: usize,
    start: i32,
    up: bool,
    opposite: bool,
    wrap: bool,
    selectable_of: EeWonSelectableOf,
    ctx: *const c_void,
) -> i64 {
    find_selectable(count, start, up, opposite, wrap, |i| selectable_of(ctx, i)).map_or(-1, |i| i as i64)
}

/// `select_range`: true with the bounds and step in the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_list_select_range(
    start: i32,
    end: i32,
    size: i32,
    out_start: *mut i32,
    out_end: *mut i32,
    out_step: *mut i32,
) -> bool {
    match select_range(start, end, size) {
        Some((s, e, st)) => {
            // SAFETY: writable (the contract)
            unsafe {
                *out_start = s;
                *out_end = e;
                *out_step = st;
            }
            true
        }
        None => false,
    }
}

/// `key_target`: true with the line and direction in the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_list_key_target(
    key: i32,
    top: i32,
    height: i32,
    view_height: i32,
    total_height: i32,
    wrap: bool,
    out_y: *mut i32,
    out_up: *mut bool,
) -> bool {
    match key_target(key, top, height, view_height, total_height, wrap) {
        Some((y, up)) => {
            // SAFETY: writable (the contract)
            unsafe {
                *out_y = y;
                *out_up = up;
            }
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_list_key_scroll(
    top: i32,
    height: i32,
    offset: i32,
    view_height: i32,
    extra_height: i32,
) -> i32 {
    key_scroll(top, height, offset, view_height, extra_height)
}

#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_won_gui_list_insert_scroll(
    at_end: bool,
    fixed: bool,
    on_bottom: bool,
    top_line: i32,
    position: i32,
    total_height: i32,
    view_height: i32,
    offset: i32,
    height: i32,
) -> i32 {
    insert_scroll(at_end, fixed, on_bottom, top_line, position, total_height, view_height, offset, height)
}

/// `batch_top`; `has_prev` false for the first item.
#[no_mangle]
pub extern "C" fn ee_won_gui_list_batch_top(has_prev: bool, prev_top: i32, next_top: i32) -> i32 {
    batch_top(has_prev.then_some(prev_top), next_top)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_list_refresh_place(position: i32, insert: i32, size: usize) -> i32 {
    refresh_place(position, insert, size)
}

#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_won_gui_list_refresh_scroll(
    fixed: bool,
    on_bottom: bool,
    top: i32,
    offset: i32,
    total_height: i32,
    view_height: i32,
    height: i32,
    old_height: i32,
) -> i32 {
    refresh_scroll(fixed, on_bottom, top, offset, total_height, view_height, height, old_height)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_list_delete_scroll(fixed: bool, top: i32, offset: i32, height: i32) -> i32 {
    delete_scroll(fixed, top, offset, height)
}

/// `delete_range`: true with (start, amount) in the outs.
///
/// # Safety
/// The outs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_list_delete_range(
    start: i32,
    end: i32,
    size: usize,
    out_start: *mut i32,
    out_amount: *mut i32,
) -> bool {
    match delete_range(start, end, size) {
        Some((s, a)) => {
            // SAFETY: writable (the contract)
            unsafe {
                *out_start = s;
                *out_amount = a;
            }
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_list_item_tip(
    width: i32,
    height: i32,
    top: i32,
    view_width: i32,
    offset: i32,
    view_height: i32,
) -> bool {
    item_tip(width, height, top, view_width, offset, view_height)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lines() {
        let tops = [0, 10, 20, 35];
        let t = |i: usize| tops[i];
        assert_eq!(find_line_at(4, -5, t), 0);
        assert_eq!(find_line_at(4, 0, t), 0);
        assert_eq!(find_line_at(4, 9, t), 0);
        assert_eq!(find_line_at(4, 10, t), 1);
        assert_eq!(find_line_at(4, 34, t), 2);
        assert_eq!(find_line_at(4, 1000, t), 3);
        assert_eq!(find_line_at(0, 5, t), 0);
        assert!(covers(35, 5, 40) && !covers(35, 5, 41));
        assert!(is_item_visible(10, 10, 0, 20, false) && !is_item_visible(15, 10, 0, 20, false));
        assert!(is_item_visible(15, 10, 0, 20, true) && !is_item_visible(0, 5, 6, 20, true));
        assert_eq!(ensure_visible(30, 10, 0, 20), Some(20));
        assert_eq!(ensure_visible(5, 10, 8, 20), Some(5));
        assert_eq!(ensure_visible(10, 10, 5, 20), None);
        let mut out = [0; 3];
        assert_eq!(stack(&[3, 4, 5], &[9, 2, 11], &mut out), (12, 11));
        assert_eq!(out, [0, 3, 7]);
        assert_eq!(full_line_extra(100, 30, 70, 10), 10);
        assert_eq!(full_line_extra(100, 30, 40, 10), 0);
    }

    #[test]
    fn selection() {
        let s = [false, true, false, false, true, false];
        let f = |i: usize| s[i];
        assert_eq!(find_selectable(6, 2, false, false, false, f), Some(4));
        assert_eq!(find_selectable(6, 2, true, false, false, f), Some(1));
        assert_eq!(find_selectable(6, 5, false, false, false, f), None);
        assert_eq!(find_selectable(6, 5, false, true, false, f), Some(4));
        let e = [true, false, false, false];
        assert_eq!(find_selectable(4, 2, false, false, true, |i| e[i]), Some(0));
        let l = [false, false, false, true];
        assert_eq!(find_selectable(4, 1, true, false, true, |i| l[i]), Some(3));
        let m = [false, true, false];
        assert_eq!(find_selectable(3, 2, false, false, true, |i| m[i]), None);
        assert_eq!(select_range(5, -2, 3), Some((2, 0, -1)));
        assert_eq!(select_range(0, 0, 0), None);
        assert_eq!(key_target(40, 10, 9, 50, 100, false), Some((20, false)));
        assert_eq!(key_target(38, 0, 9, 50, 100, true), Some((100, true)));
        assert_eq!(key_target(34, 90, 9, 50, 100, true), Some((0, false)));
        assert_eq!(key_target(33, 60, 9, 50, 100, false), Some((20, true)));
        assert_eq!(key_target(37, 60, 9, 50, 100, false), Some((-1, true)));
        assert_eq!(key_target(39, 60, 9, 50, 100, true), Some((100, true)));
        assert_eq!(key_target(13, 60, 9, 50, 100, false), None);
        assert_eq!(key_scroll(90, 10, 0, 50, 5), 55);
        assert_eq!(key_scroll(3, 10, 5, 50, 5), 3);
    }

    #[test]
    fn edits() {
        assert_eq!(insert_scroll(true, false, true, 0, 0, 0, 0, 7, 3), 10);
        assert_eq!(insert_scroll(true, true, true, 0, 0, 0, 0, 7, 3), 7);
        assert_eq!(insert_scroll(false, false, false, 4, 2, 100, 50, 7, 3), 10);
        assert_eq!(insert_scroll(false, false, false, 1, 2, 100, 50, 7, 3), 7);
        assert_eq!(insert_scroll(false, false, true, 1, 2, 40, 50, 7, 3), 7);
        assert_eq!((batch_top(None, 5), batch_top(Some(3), 9), batch_top(Some(9), 9)), (1, 4, 9));
        assert_eq!(refresh_place(1, 4, 5), 3);
        assert_eq!(refresh_place(4, 9, 5), 4);
        assert_eq!(refresh_place(3, -1, 5), 4);
        assert_eq!(refresh_scroll(false, false, 10, 20, 100, 50, 7, 5), 22);
        assert_eq!(refresh_scroll(false, false, 30, 20, 100, 50, 7, 5), 20);
        assert_eq!(
            (delete_scroll(false, 10, 20, 4), delete_scroll(false, 30, 20, 4), delete_scroll(true, 10, 20, 4)),
            (16, 20, 20)
        );
        assert_eq!(delete_range(1, -1, 4), Some((1, 3)));
        assert_eq!(delete_range(1, 9, 4), Some((1, 3)));
        assert_eq!(delete_range(3, 3, 4), None);
        assert_eq!(delete_range(-1, 2, 4), None);
        assert!(
            item_tip(200, 10, 20, 100, 0, 50)
                && !item_tip(100, 10, 20, 100, 0, 50)
                && !item_tip(200, 10, 48, 100, 0, 50)
        );
    }
}
