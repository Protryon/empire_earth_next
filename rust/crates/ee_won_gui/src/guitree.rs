//! WONGUI's tree list (TreeArea.cpp, TreeItem.cpp: the room list of the Internet lobby's room dialog). The C++
//! keeps the tree's items (their children vectors, parents and wrapped list items), the list rows, the sort's
//! comparisons, events and the paint; these answer:
//!
//! - the walks over the items: the next item after a subtree (`GetNextNonChild`), the rows a subtree adds when it
//!   opens (`InsertChildren`: children last to first, an open child's own children after it), closing a subtree
//!   (`CloseAll`), through `EeWonTreeOps` (the C++ answers each question about an item and does each step);
//! - the range and last-child tests (`GetChild`, `IsLastChild`), an open or close's work (`SetOpen`) and its
//!   scroll so the opened subtree shows, where an inserted child's row goes (`InsertChild`), what a removal does
//!   (`RemoveChild`), the sort's plan (`TreeArea::Sort`);
//! - the mouse: which wrapped item the mouse leaves and enters (`GetMouseItem`), the selectable and contained
//!   tests right of the indent (`CanSelect`, `ContainsPoint`), the row's width (`CalcDimensions`), the expand
//!   button's event (`TreeArea::HandleComponentEvent`), the tree flags (`SetTreeFlags`, `TreeFlagSet`).
//!
//! The tree has no keys of its own: its keyboard movement is the list's (`guilist`). The +/- box test and a
//! child's indent and place are `guimulti`'s. Ints wrap (the game builds with -fwrapv).

use std::ffi::c_void;

/// `ComponentEvent_ButtonPressed` (WONLobbyGUI.h's event numbers).
const EVENT_BUTTON_PRESSED: i32 = 15;
/// The control id of a tree row's expand button.
const EXPAND_BUTTON_ID: i32 = 100003;

/// The questions the walks ask about the items.
pub trait TreeNodes {
    type Node: Copy;
    fn parent(&self, n: Self::Node) -> Option<Self::Node>;
    fn index(&self, n: Self::Node) -> i32;
    fn count(&self, n: Self::Node) -> usize;
    fn child(&self, n: Self::Node, i: usize) -> Self::Node;
    fn is_open(&self, n: Self::Node) -> bool;
}

/// `IsLastChild`'s test (and `GetNextNonChild`'s): the index, as the `size_t` it is compared as, at or past the
/// last of `count` (so a negative index is past it, and with no children everything but -1 is before it).
pub fn is_last(index: i32, count: usize) -> bool {
    (index as isize as usize) >= count.wrapping_sub(1)
}

/// `GetChild`'s range test.
pub fn child_in_range(pos: i32, count: usize) -> bool {
    !(pos < 0 || pos as usize >= count)
}

/// `GetNextNonChild`: the next sibling, else the parent's next, else none (at the root).
pub fn next_non_child<T: TreeNodes>(t: &T, n: T::Node) -> Option<T::Node> {
    let mut n = n;
    loop {
        let p = t.parent(n)?;
        let i = t.index(n);
        if is_last(i, t.count(p)) {
            n = p;
        } else {
            return Some(t.child(p, i.wrapping_add(1) as isize as usize));
        }
    }
}

/// `InsertChildren`: each child's row at `pos`, last child first; an open child's children at `pos + 1` right
/// after its own row.
pub fn insert_children<T: TreeNodes>(t: &T, n: T::Node, pos: i32, insert: &mut impl FnMut(T::Node, i32)) {
    let mut i = (t.count(n) as i32).wrapping_sub(1);
    while i >= 0 {
        let c = t.child(n, i as usize);
        insert(c, pos);
        if t.is_open(c) {
            insert_children(t, c, pos.wrapping_add(1), insert);
        }
        i -= 1;
    }
}

/// `CloseAll`: the item and then each child's subtree in order.
pub fn close_all<T: TreeNodes>(t: &T, n: T::Node, close: &mut impl FnMut(T::Node)) {
    close(n);
    for i in 0..t.count(n) {
        close_all(t, t.child(n, i), close);
    }
}

/// `SetOpen`'s work: 0 nothing (already so), 1 only the flag (no children: no rows and no event), 2 close (the
/// children's rows deleted), 3 open (their rows inserted, then the scroll).
pub fn set_open_action(open: bool, want: bool, no_children: bool) -> u8 {
    if open == want {
        0
    } else if no_children {
        1
    } else if want {
        3
    } else {
        2
    }
}

/// `SetOpen`'s scroll after an open: when the next item's top (the total height when none) is below the view, the
/// offset that shows it, no further than the opened item's top.
pub fn open_scroll(
    total_height: i32,
    sibling_top: Option<i32>,
    vert_offset: i32,
    view_height: i32,
    item_top: i32,
) -> Option<i32> {
    let y = sibling_top.unwrap_or(total_height);
    if y > vert_offset.wrapping_add(view_height) {
        let o = y.wrapping_sub(view_height);
        Some(if item_top < o { item_top } else { o })
    } else {
        None
    }
}

/// `InsertChild`'s row for the new child: 0 none (the parent is closed), 1 before the row after the parent's
/// subtree (it went last), 2 before the next child's row.
pub fn insert_row(parent_open: bool, on_end: bool) -> u8 {
    if !parent_open {
        0
    } else if on_end {
        1
    } else {
        2
    }
}

/// `RemoveChild`'s steps: 1 close the child, 2 remove its rows (the parent is open), 4 repaint (no child left).
pub fn remove_plan(child_open: bool, parent_open: bool, count_before: usize) -> u8 {
    let mut s = 0;
    if child_open {
        s |= 1;
    }
    if parent_open {
        s |= 2;
    }
    if count_before.wrapping_sub(1) == 0 {
        s |= 4;
    }
    s
}

/// `TreeArea::Sort`: 0 nothing (an unsorted tree), 1 the root (every row again), 2 an item's children without
/// rows to touch, 3 an open, shown item (closed, sorted, opened).
pub fn sort_plan(sort_none: bool, is_root: bool, open: bool, row: i32) -> u8 {
    if sort_none {
        0
    } else if is_root {
        1
    } else if open && row >= 0 {
        3
    } else {
        2
    }
}

/// `GetMouseItem`: 1 the remembered item gets the exit (it is another item or the mouse left it), 2 the new item
/// gets the enter (none was remembered and the mouse is on it; never in the same call as an exit). `contains`
/// is asked about the remembered item (0) only when it is the new one, about the new item (1) only when none was
/// remembered.
pub fn mouse_item(has_col: bool, same: bool, has_new: bool, contains: &mut impl FnMut(i32) -> bool) -> u8 {
    let mut r = 0;
    if has_col && (!same || !contains(0)) {
        r |= 1;
    }
    if !has_col && has_new && contains(1) {
        r |= 2;
    }
    r
}

/// `CanSelect`: selectable and right of the indent.
pub fn can_select(selectable: bool, x: i32, indent: i32) -> bool {
    selectable && x >= indent
}

/// `ContainsPoint`'s x in the wrapped item, none left of the indent.
pub fn local_x(x: i32, indent: i32) -> Option<i32> {
    if x < indent {
        None
    } else {
        Some(x.wrapping_sub(indent))
    }
}

/// `CalcDimensions`' width: the indent and the wrapped item's.
pub fn row_width(indent: i32, item_width: i32) -> i32 {
    indent.wrapping_add(item_width)
}

/// `TreeArea::HandleComponentEvent`: a press of a row's expand button toggles its row.
pub fn is_expand_press(event_type: i32, control_id: i32) -> bool {
    event_type == EVENT_BUTTON_PRESSED && control_id == EXPAND_BUTTON_ID
}

/// `SetTreeFlags`.
pub fn tree_flags(flags: i32, which: i32, on: bool) -> i32 {
    if on {
        flags | which
    } else {
        flags & !which
    }
}

// ---- the C ABI

/// The C++'s answers and steps over its items (each `node` a `TreeItem*`).
#[repr(C)]
pub struct EeWonTreeOps {
    pub ctx: *mut c_void,
    pub parent: unsafe extern "C" fn(ctx: *mut c_void, node: *mut c_void) -> *mut c_void,
    pub index: unsafe extern "C" fn(ctx: *mut c_void, node: *mut c_void) -> i32,
    pub count: unsafe extern "C" fn(ctx: *mut c_void, node: *mut c_void) -> usize,
    pub child: unsafe extern "C" fn(ctx: *mut c_void, node: *mut c_void, i: usize) -> *mut c_void,
    pub is_open: unsafe extern "C" fn(ctx: *mut c_void, node: *mut c_void) -> bool,
    /// `InsertChildren`'s step: the item's row at a list position.
    pub insert: unsafe extern "C" fn(ctx: *mut c_void, node: *mut c_void, pos: i32),
    /// `CloseAll`'s step: the item marked closed.
    pub close: unsafe extern "C" fn(ctx: *mut c_void, node: *mut c_void),
}

struct Ops<'a>(&'a EeWonTreeOps);

impl TreeNodes for Ops<'_> {
    type Node = *mut c_void;
    fn parent(&self, n: *mut c_void) -> Option<*mut c_void> {
        // SAFETY: the C++'s callbacks over its live items (the contract)
        let p = unsafe { (self.0.parent)(self.0.ctx, n) };
        if p.is_null() {
            None
        } else {
            Some(p)
        }
    }
    fn index(&self, n: *mut c_void) -> i32 {
        unsafe { (self.0.index)(self.0.ctx, n) }
    }
    fn count(&self, n: *mut c_void) -> usize {
        unsafe { (self.0.count)(self.0.ctx, n) }
    }
    fn child(&self, n: *mut c_void, i: usize) -> *mut c_void {
        unsafe { (self.0.child)(self.0.ctx, n, i) }
    }
    fn is_open(&self, n: *mut c_void) -> bool {
        unsafe { (self.0.is_open)(self.0.ctx, n) }
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_is_last(index: i32, count: usize) -> bool {
    is_last(index, count)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_child_in_range(pos: i32, count: usize) -> bool {
    child_in_range(pos, count)
}

/// `next_non_child`: the item, or NULL.
///
/// # Safety
/// `ops` is valid and its callbacks answer for live items, `node` among them.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tree_next_non_child(ops: *const EeWonTreeOps, node: *mut c_void) -> *mut c_void {
    // SAFETY: valid (the contract)
    next_non_child(&Ops(unsafe { &*ops }), node).unwrap_or(std::ptr::null_mut())
}

/// `insert_children` through `ops.insert`.
///
/// # Safety
/// As `ee_won_gui_tree_next_non_child`; the steps leave the children vectors as they are.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tree_insert_children(ops: *const EeWonTreeOps, node: *mut c_void, pos: i32) {
    // SAFETY: valid (the contract)
    let o = unsafe { &*ops };
    insert_children(&Ops(o), node, pos, &mut |c, p| unsafe { (o.insert)(o.ctx, c, p) });
}

/// `close_all` through `ops.close`.
///
/// # Safety
/// As `ee_won_gui_tree_insert_children`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tree_close_all(ops: *const EeWonTreeOps, node: *mut c_void) {
    // SAFETY: valid (the contract)
    let o = unsafe { &*ops };
    close_all(&Ops(o), node, &mut |c| unsafe { (o.close)(o.ctx, c) });
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_set_open_action(open: bool, want: bool, no_children: bool) -> u8 {
    set_open_action(open, want, no_children)
}

/// `open_scroll`: whether to scroll, the offset into `out`.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tree_open_scroll(
    total_height: i32,
    has_sibling: bool,
    sibling_top: i32,
    vert_offset: i32,
    view_height: i32,
    item_top: i32,
    out: *mut i32,
) -> bool {
    match open_scroll(total_height, has_sibling.then_some(sibling_top), vert_offset, view_height, item_top) {
        Some(o) => {
            // SAFETY: writable (the contract)
            unsafe { *out = o };
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_insert_row(parent_open: bool, on_end: bool) -> u8 {
    insert_row(parent_open, on_end)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_remove_plan(child_open: bool, parent_open: bool, count_before: usize) -> u8 {
    remove_plan(child_open, parent_open, count_before)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_sort_plan(sort_none: bool, is_root: bool, open: bool, row: i32) -> u8 {
    sort_plan(sort_none, is_root, open, row)
}

/// The C++'s `Contains` of the remembered (0) or the new (1) wrapped item at the mouse.
pub type EeWonTreeContains = unsafe extern "C" fn(ctx: *mut c_void, which: i32) -> bool;

/// `mouse_item`.
///
/// # Safety
/// `contains` is callable with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tree_mouse_item(
    has_col: bool,
    same: bool,
    has_new: bool,
    contains: EeWonTreeContains,
    ctx: *mut c_void,
) -> u8 {
    // SAFETY: callable (the contract)
    mouse_item(has_col, same, has_new, &mut |w| unsafe { contains(ctx, w) })
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_can_select(selectable: bool, x: i32, indent: i32) -> bool {
    can_select(selectable, x, indent)
}

/// `local_x`: whether right of the indent, the x into `out`.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_tree_local_x(x: i32, indent: i32, out: *mut i32) -> bool {
    match local_x(x, indent) {
        Some(v) => {
            // SAFETY: writable (the contract)
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_row_width(indent: i32, item_width: i32) -> i32 {
    row_width(indent, item_width)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_is_expand_press(event_type: i32, control_id: i32) -> bool {
    is_expand_press(event_type, control_id)
}

#[no_mangle]
pub extern "C" fn ee_won_gui_tree_flags(flags: i32, which: i32, on: bool) -> i32 {
    tree_flags(flags, which, on)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Items 0..n: parent, index in it, children, open.
    struct T {
        parent: Vec<Option<usize>>,
        index: Vec<i32>,
        kids: Vec<Vec<usize>>,
        open: Vec<bool>,
    }

    impl TreeNodes for T {
        type Node = usize;
        fn parent(&self, n: usize) -> Option<usize> {
            self.parent[n]
        }
        fn index(&self, n: usize) -> i32 {
            self.index[n]
        }
        fn count(&self, n: usize) -> usize {
            self.kids[n].len()
        }
        fn child(&self, n: usize, i: usize) -> usize {
            self.kids[n][i]
        }
        fn is_open(&self, n: usize) -> bool {
            self.open[n]
        }
    }

    // root 0: children 1, 2; 1: children 3, 4; 4: child 5
    fn tree() -> T {
        T {
            parent: vec![None, Some(0), Some(0), Some(1), Some(1), Some(4)],
            index: vec![-1, 0, 1, 0, 1, 0],
            kids: vec![vec![1, 2], vec![3, 4], vec![], vec![], vec![5], vec![]],
            open: vec![true, true, false, false, true, false],
        }
    }

    #[test]
    fn walks() {
        let t = tree();
        assert_eq!(next_non_child(&t, 3), Some(4));
        assert_eq!(next_non_child(&t, 5), Some(2));
        assert_eq!(next_non_child(&t, 4), Some(2));
        assert_eq!(next_non_child(&t, 2), None);
        assert_eq!(next_non_child(&t, 0), None);
        let mut rows = vec![];
        insert_children(&t, 0, 1, &mut |n, p| rows.push((n, p)));
        assert_eq!(rows, [(2, 1), (1, 1), (4, 2), (5, 3), (3, 2)]);
        let mut closed = vec![];
        close_all(&t, 1, &mut |n| closed.push(n));
        assert_eq!(closed, [1, 3, 4, 5]);
    }

    #[test]
    fn tests_and_plans() {
        assert!(is_last(2, 3));
        assert!(!is_last(1, 3));
        assert!(is_last(-1, 3));
        assert!(is_last(-1, 0));
        assert!(!is_last(5, 0));
        assert!(child_in_range(0, 1) && !child_in_range(1, 1) && !child_in_range(-1, 5));
        assert_eq!(set_open_action(true, true, false), 0);
        assert_eq!(set_open_action(false, true, true), 1);
        assert_eq!(set_open_action(false, true, false), 3);
        assert_eq!(set_open_action(true, false, false), 2);
        assert_eq!(open_scroll(500, Some(300), 0, 200, 50), Some(50));
        assert_eq!(open_scroll(500, Some(300), 0, 200, 150), Some(100));
        assert_eq!(open_scroll(500, None, 400, 200, 0), None);
        assert_eq!(insert_row(false, true), 0);
        assert_eq!(insert_row(true, true), 1);
        assert_eq!(insert_row(true, false), 2);
        assert_eq!(remove_plan(true, true, 1), 7);
        assert_eq!(remove_plan(false, false, 2), 0);
        assert_eq!(sort_plan(true, true, true, 0), 0);
        assert_eq!(sort_plan(false, true, false, -1), 1);
        assert_eq!(sort_plan(false, false, true, 3), 3);
        assert_eq!(sort_plan(false, false, true, -1), 2);
    }

    #[test]
    fn mouse() {
        let mut asked = vec![];
        assert_eq!(
            mouse_item(true, true, true, &mut |w| {
                asked.push(w);
                true
            }),
            0
        );
        assert_eq!(
            mouse_item(true, true, true, &mut |w| {
                asked.push(w);
                false
            }),
            1
        );
        assert_eq!(
            mouse_item(true, false, true, &mut |w| {
                asked.push(w);
                true
            }),
            1
        );
        assert_eq!(
            mouse_item(false, false, true, &mut |w| {
                asked.push(w);
                true
            }),
            2
        );
        assert_eq!(
            mouse_item(false, false, false, &mut |w| {
                asked.push(w);
                true
            }),
            0
        );
        assert_eq!(asked, [0, 0, 1]);
        assert!(can_select(true, 15, 15) && !can_select(true, 14, 15) && !can_select(false, 20, 15));
        assert_eq!(local_x(20, 15), Some(5));
        assert_eq!(local_x(14, 15), None);
        assert_eq!(row_width(i32::MAX, 1), i32::MIN);
        assert!(is_expand_press(15, 100003) && !is_expand_press(16, 100003) && !is_expand_press(15, 3));
        assert_eq!(tree_flags(3, 1, false), 2);
        assert_eq!(tree_flags(2, 8, true), 10);
    }
}
