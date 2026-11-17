//! WONGUI's multi-column lists (MultiListArea.cpp, MultiListHeader.cpp: the lobby's game, player and server lists)
//! and the tree rows' rules (TreeItem.cpp). The C++ keeps the lists, their rows and cells, the header's buttons,
//! the column viewports, sorting, events, cursors and the paint; these answer:
//!
//! - the column at an x (`GetColumnAt`), a column's resize (its partner column giving the width, or the total
//!   capped) and every column's new x (`SetColumnWidth`), the dynamic column's change after a resize
//!   (`Virtual08`), the auto-grow test (`UpdateItem`), the partner links (`SetColumnSizeMode`);
//! - the header's buttons over the columns (`NotifyColumnChange`), the sort a header press asks for
//!   (`HandleComponentEvent`), a column's shown sort (`GetSortType`), the resize edge under the mouse
//!   (`GetDragComponent`) and the dragged width (`MouseDrag`);
//! - a tree row's +/- box test (`TreeItem::MouseDown`), a child's indent and insert place (`InsertChild`).
//!
//! Size modes: 0 fixed, 1 free, 2 paired with the next mode-2 column. Sort types: 0 ascending, 1 descending, 2
//! none. Ints wrap (the game builds with -fwrapv).

/// `GetColumnAt`: the first column whose right edge is at or past `x`, the last one when none (-1 without
/// columns).
pub fn column_at(xs: &[i32], widths: &[i32], x: i32) -> i32 {
    (0..xs.len()).find(|&i| xs[i].wrapping_add(widths[i]) >= x).map_or(xs.len() as i32 - 1, |i| i as i32)
}

/// A list's columns as `SetColumnWidth` reads and changes them.
pub struct Columns<'a> {
    pub widths: &'a mut [i32],
    pub modes: &'a [u8],
    pub partners: &'a [i32],
    pub xs: &'a mut [i32],
}

/// `SetColumnWidth`: column `col` to `width` (not negative); a paired column (unless the change comes from a
/// resize) takes the width from its partner (at most all of it), any other grows the total, capped by a
/// positive `max_total`; then every column's x. False (nothing changed) for a column out of range.
pub fn set_column_width(
    c: Columns<'_>,
    total: &mut i32,
    max_total: i32,
    col: i32,
    width: i32,
    from_size: bool,
) -> bool {
    let n = c.widths.len();
    // the original compared the int with the vector's size as unsigned
    if col as isize as usize >= n {
        return false;
    }
    let col = col as usize;
    let width = width.max(0);
    let partner = c.partners[col];
    if !from_size && c.modes[col] == 2 && partner >= 0 && (partner as usize) < n {
        let p = partner as usize;
        let mut delta = width.wrapping_sub(c.widths[col]);
        if delta > c.widths[p] {
            delta = c.widths[p];
        }
        c.widths[col] = c.widths[col].wrapping_add(delta);
        c.widths[p] = c.widths[p].wrapping_sub(delta);
    } else {
        let mut delta = width.wrapping_sub(c.widths[col]);
        if max_total > 0 && delta > 0 && total.wrapping_add(delta) > max_total {
            delta = max_total.wrapping_sub(*total).max(0);
        }
        c.widths[col] = c.widths[col].wrapping_add(delta);
        *total = total.wrapping_add(delta);
    }
    let mut x = 0i32;
    for i in 0..n {
        c.xs[i] = x;
        x = x.wrapping_add(c.widths[i]);
    }
    true
}

/// `Virtual08`: the dynamic column's width change after the view went from `old_width` to `view_width` (kept
/// from overflowing the view); `None` when there is no such column or no change.
pub fn dynamic_delta(column: i32, count: usize, view_width: i32, old_width: i32, total: i32) -> Option<i32> {
    if column < 0 || column >= count as i32 {
        return None;
    }
    let mut d = view_width.wrapping_sub(old_width);
    if total.wrapping_add(d) > view_width {
        d = view_width.wrapping_sub(total).max(0);
    }
    (d != 0).then_some(d)
}

/// `UpdateItem`'s auto-grow: the column's new width for a cell `item_width` wide, `None` when it fits.
pub fn grow_column(item_width: i32, border: i32, column_width: i32) -> Option<i32> {
    let w = item_width.wrapping_add(border);
    (item_width > 0 && w > column_width).then_some(w)
}

/// `SetColumnSizeMode`: whether the partners are linked again (the old or the new mode pairs).
pub fn relink(old_mode: u8, new_mode: u8) -> bool {
    new_mode == 2 || old_mode == 2
}

/// `SetColumnSizeMode`'s links: each paired column's partner becomes the next paired one (the last keeps its own).
pub fn link_partners(modes: &[u8], partners: &mut [i32]) {
    let mut prev: Option<usize> = None;
    for (i, &m) in modes.iter().enumerate() {
        if m != 2 {
            continue;
        }
        if let Some(p) = prev {
            partners[p] = i as i32;
        }
        prev = Some(i);
    }
}

/// A header button's place (`NotifyColumnChange`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonHeaderCol {
    pub x: i32,
    pub width: i32,
    pub enabled: bool,
}

/// `NotifyColumnChange`: each header column's x and width over the list's columns (scrolled by `horz`), enabled
/// when the list has the column; a header column past the list's columns is 0 wide when it has a button, and
/// moves the next ones one pixel left when it has none (the original added its -1).
pub fn header_layout(has_button: &[bool], widths: &[i32], horz: i32) -> Vec<EeWonHeaderCol> {
    let mut x = horz.wrapping_neg();
    let mut out = Vec::with_capacity(has_button.len());
    for (i, &has) in has_button.iter().enumerate() {
        let mut w = widths.get(i).copied().unwrap_or(-1);
        let enabled = w != -1;
        if has && !enabled {
            w = 0;
        }
        out.push(EeWonHeaderCol { x, width: w, enabled });
        x = x.wrapping_add(w);
    }
    out
}

/// `HandleComponentEvent`: the sort a press on a column asks for: the other direction of the sorted column (or
/// of a column with no direction yet), else the column's own.
pub fn header_toggle(sort_type: i32, is_current: bool) -> i32 {
    if is_current || sort_type == 2 {
        if sort_type == 0 {
            1
        } else {
            0
        }
    } else {
        sort_type
    }
}

/// `GetSortType`: the column's direction when it is the sorted column, else none (2).
pub fn header_sort_type(column: i32, count: usize, current: i32, sort_type: i32) -> i32 {
    if column < 0 || column >= count as i32 || column != current {
        2
    } else {
        sort_type
    }
}

/// A header button as `GetDragComponent` reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonHeaderButton {
    pub present: bool,
    pub left: i32,
    pub right: i32,
    pub width: i32,
}

/// `GetDragComponent`: the column whose resize edge (a quarter of the button, at most 5 pixels) is under `x` and
/// the drag's grab x (`x` less the column's width); `None` when the first button under `x` has no resizable
/// edge there (fixed columns, unpaired paired ones).
pub fn header_drag(
    x: i32,
    buttons: &[EeWonHeaderButton],
    widths: &[i32],
    modes: &[u8],
    partners: &[i32],
) -> Option<(i32, i32)> {
    let n = widths.len();
    for (i, b) in buttons.iter().enumerate() {
        if !b.present || !(x >= b.left) || !(x <= b.right) {
            continue;
        }
        let edge = (b.width / 4).min(5);
        let col = if x.wrapping_sub(b.left) < edge && i != 0 {
            i - 1
        } else if b.right.wrapping_sub(x) < edge {
            i
        } else {
            return None;
        };
        if col >= n {
            return None;
        }
        let p = partners[col];
        if modes[col] == 0 || (modes[col] == 2 && (p < 0 || p as usize >= n)) {
            return None;
        }
        return Some((col as i32, x.wrapping_sub(widths[col])));
    }
    None
}

/// `MouseDrag`: the dragged column's width (not negative).
pub fn header_drag_width(x: i32, drag_x: i32) -> i32 {
    x.wrapping_sub(drag_x).max(0)
}

/// `TreeItem::MouseDown`: a left press in the 13 pixels left of the row's indent toggles it (as does an
/// accepted double click on a tree that allows it).
pub fn tree_box_hit(x: i32, indent: i32) -> bool {
    x < indent && x > indent.wrapping_sub(14)
}

/// `InsertChild`: the child's indent (top-level rows of a tree without root lines or buttons at 0, the others 15
/// in from their parent's) and its place among `count` children (an out-of-range place appends).
pub fn tree_child(parent_is_root: bool, tree_flags: i32, parent_indent: i32, pos: i32, count: usize) -> (i32, usize) {
    let indent = if parent_is_root && tree_flags & 3 == 0 { 0 } else { parent_indent.wrapping_add(15) };
    let p = if pos < 0 || pos as usize > count { count } else { pos as usize };
    (indent, p)
}

// ---- the C ABI

/// A slice of `n` from a pointer that may be NULL when `n` is 0.
unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        // SAFETY: `n` entries (the callers' contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

unsafe fn slice_mut<'a, T>(p: *mut T, n: usize) -> &'a mut [T] {
    if n == 0 || p.is_null() {
        &mut []
    } else {
        // SAFETY: `n` entries (the callers' contract)
        unsafe { std::slice::from_raw_parts_mut(p, n) }
    }
}

/// `column_at` over `count` columns.
///
/// # Safety
/// `xs` and `widths` hold `count` ints (or are NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_ml_column_at(xs: *const i32, widths: *const i32, count: usize, x: i32) -> i32 {
    // SAFETY: `count` each (the contract)
    unsafe { column_at(slice(xs, count), slice(widths, count), x) }
}

/// `set_column_width` over `count` columns in place.
///
/// # Safety
/// The arrays hold `count` entries (or are NULL with 0); `total` is writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_ml_set_width(
    widths: *mut i32,
    modes: *const u8,
    partners: *const i32,
    xs: *mut i32,
    count: usize,
    total: *mut i32,
    max_total: i32,
    col: i32,
    width: i32,
    from_size: bool,
) -> bool {
    // SAFETY: the counts and a writable total (the contract)
    unsafe {
        set_column_width(
            Columns {
                widths: slice_mut(widths, count),
                modes: slice(modes, count),
                partners: slice(partners, count),
                xs: slice_mut(xs, count),
            },
            &mut *total,
            max_total,
            col,
            width,
            from_size,
        )
    }
}

/// `dynamic_delta`: 0 when nothing changes.
#[no_mangle]
pub extern "C" fn ee_won_gui_ml_dynamic_delta(
    column: i32,
    count: usize,
    view_width: i32,
    old_width: i32,
    total: i32,
) -> i32 {
    dynamic_delta(column, count, view_width, old_width, total).unwrap_or(0)
}

/// `grow_column`: -1 when the cell fits.
#[no_mangle]
pub extern "C" fn ee_won_gui_ml_grow(item_width: i32, border: i32, column_width: i32) -> i32 {
    grow_column(item_width, border, column_width).unwrap_or(-1)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_ml_relink(old_mode: u8, new_mode: u8) -> bool {
    relink(old_mode, new_mode)
}

/// `link_partners` over `count` columns in place.
///
/// # Safety
/// `modes` and `partners` hold `count` entries (or are NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_ml_link_partners(modes: *const u8, partners: *mut i32, count: usize) {
    // SAFETY: `count` each (the contract)
    unsafe { link_partners(slice(modes, count), slice_mut(partners, count)) }
}

/// `header_layout` for `count` header columns over `columns` list columns into `out` (room for `count`).
///
/// # Safety
/// `has_button` holds and `out` has room for `count` entries, `widths` holds `columns` ints (each may be NULL
/// with a 0 count).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_mh_layout(
    has_button: *const bool,
    count: usize,
    widths: *const i32,
    columns: usize,
    horz: i32,
    out: *mut EeWonHeaderCol,
) {
    // SAFETY: the counts (the contract)
    unsafe {
        let l = header_layout(slice(has_button, count), slice(widths, columns), horz);
        for (i, c) in l.iter().enumerate() {
            *out.add(i) = *c;
        }
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_mh_toggle(sort_type: i32, is_current: bool) -> i32 {
    header_toggle(sort_type, is_current)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_mh_sort_type(column: i32, count: usize, current: i32, sort_type: i32) -> i32 {
    header_sort_type(column, count, current, sort_type)
}

/// `header_drag` over `count` buttons and `columns` list columns: the column (and the grab x into `out_drag_x`),
/// or -1.
///
/// # Safety
/// `buttons` holds `count` entries and the column arrays `columns` (each may be NULL with a 0 count);
/// `out_drag_x` is writable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_gui_mh_drag(
    x: i32,
    buttons: *const EeWonHeaderButton,
    count: usize,
    widths: *const i32,
    modes: *const u8,
    partners: *const i32,
    columns: usize,
    out_drag_x: *mut i32,
) -> i32 {
    // SAFETY: the counts and a writable out (the contract)
    unsafe {
        match header_drag(
            x,
            slice(buttons, count),
            slice(widths, columns),
            slice(modes, columns),
            slice(partners, columns),
        ) {
            Some((c, d)) => {
                *out_drag_x = d;
                c
            }
            None => -1,
        }
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_mh_drag_width(x: i32, drag_x: i32) -> i32 {
    header_drag_width(x, drag_x)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_box_hit(x: i32, indent: i32) -> bool {
    tree_box_hit(x, indent)
}

/// `tree_child`: the indent, the place into `out_pos`.
///
/// # Safety
/// `out_pos` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tree_child(
    parent_is_root: bool,
    tree_flags: i32,
    parent_indent: i32,
    pos: i32,
    count: usize,
    out_pos: *mut usize,
) -> i32 {
    let (i, p) = tree_child(parent_is_root, tree_flags, parent_indent, pos, count);
    // SAFETY: writable (the contract)
    unsafe { *out_pos = p };
    i
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn columns() {
        assert_eq!(column_at(&[0, 50, 80], &[50, 30, 40], 60), 1);
        assert_eq!(column_at(&[0, 50, 80], &[50, 30, 40], 500), 2);
        assert_eq!(column_at(&[], &[], 5), -1);
        let (mut w, mut xs, mut total) = ([50, 30, 40], [0; 3], 120);
        // a free column capped by the maximum total
        assert!(set_column_width(
            Columns { widths: &mut w, modes: &[1, 1, 1], partners: &[-1; 3], xs: &mut xs },
            &mut total,
            130,
            1,
            60,
            false
        ));
        assert_eq!((w, xs, total), ([50, 40, 40], [0, 50, 90], 130));
        // a paired column takes from its partner, at most all of it
        assert!(set_column_width(
            Columns { widths: &mut w, modes: &[2, 2, 1], partners: &[1, -1, -1], xs: &mut xs },
            &mut total,
            0,
            0,
            200,
            false
        ));
        assert_eq!((w, xs, total), ([90, 0, 40], [0, 90, 90], 130));
        assert!(!set_column_width(
            Columns { widths: &mut w, modes: &[1; 3], partners: &[-1; 3], xs: &mut xs },
            &mut total,
            0,
            -1,
            5,
            false
        ));
        assert!(!set_column_width(
            Columns { widths: &mut w, modes: &[1; 3], partners: &[-1; 3], xs: &mut xs },
            &mut total,
            0,
            3,
            5,
            false
        ));
        assert_eq!(dynamic_delta(0, 3, 300, 250, 200), Some(50));
        assert_eq!(dynamic_delta(0, 3, 300, 250, 280), Some(20));
        assert_eq!(dynamic_delta(0, 3, 300, 250, 310), None);
        assert_eq!(dynamic_delta(3, 3, 300, 250, 200), None);
        assert_eq!(grow_column(40, 2, 41), Some(42));
        assert_eq!(grow_column(40, 2, 42), None);
        assert_eq!(grow_column(0, 2, -5), None);
        assert!(relink(2, 1) && relink(1, 2) && !relink(1, 0));
        let mut p = [-1, -1, -1, -1];
        link_partners(&[2, 1, 2, 2], &mut p);
        assert_eq!(p, [2, -1, 3, -1]);
    }

    #[test]
    fn header() {
        let c = |x, width, enabled| EeWonHeaderCol { x, width, enabled };
        assert_eq!(
            header_layout(&[true, true, true], &[50, 30], 10),
            vec![c(-10, 50, true), c(40, 30, true), c(70, 0, false)]
        );
        assert_eq!(
            header_layout(&[true, false, true], &[50], 0),
            vec![c(0, 50, true), c(50, -1, false), c(49, 0, false)]
        );
        assert_eq!(
            (header_toggle(0, true), header_toggle(1, true), header_toggle(2, false), header_toggle(1, false)),
            (1, 0, 0, 1)
        );
        assert_eq!(header_sort_type(1, 3, 1, 0), 0);
        assert_eq!(header_sort_type(2, 3, 1, 0), 2);
        assert_eq!(header_sort_type(-1, 3, -1, 0), 2);
        let b = |left, right| EeWonHeaderButton { present: true, left, right, width: right - left };
        let buttons = [b(0, 50), b(50, 80), b(80, 200)];
        let (w, m, p) = ([50, 30, 120], [1u8, 0, 1], [-1, -1, -1]);
        assert_eq!(header_drag(48, &buttons, &w, &m, &p), Some((0, -2)));
        // the left edge of the second button is the first column's
        assert_eq!(header_drag(51, &buttons, &w, &m, &p), Some((0, 1)));
        // the middle of a button, a fixed column's edge
        assert_eq!(header_drag(30, &buttons, &w, &m, &p), None);
        assert_eq!(header_drag(78, &buttons, &w, &m, &p), None);
        assert_eq!(header_drag(500, &buttons, &w, &m, &p), None);
        assert_eq!(header_drag_width(10, 20), 0);
    }

    #[test]
    fn tree() {
        assert!(tree_box_hit(14, 15) && tree_box_hit(2, 15) && !tree_box_hit(1, 15) && !tree_box_hit(15, 15));
        assert_eq!(tree_child(true, 0, 30, 1, 3), (0, 1));
        assert_eq!(tree_child(true, 1, 30, 9, 3), (45, 3));
        assert_eq!(tree_child(false, 0, 30, -1, 3), (45, 3));
    }
}
