//! WONGUI's tree list as whole Rust objects (R6 WON round 24): TreeArea (the Internet lobby's room list) and its
//! rows, TreeItem (a wrapped list item, its parent, indent, index, open state and children). The original's
//! TreeArea.cpp, TreeItem.cpp and TreeItemLifetime.cpp with their state.
//!
//! Both stay C++ classes (the room control holds the tree and its rows; the rows are the list's items), whose
//! virtuals and methods are one-line forwarders: `ee_won_tree_call` (the tree) and `ee_won_tnode_call` (a row, given
//! its tree's Rust object when the call has one). The tree's ListArea base is the Rust list (`listobj`), called
//! directly. A row reaches another row's Rust state through the op `NodeOf` and keeps a reference on its wrapped
//! item and on each child (the original's `SmartPtr`s, through `ItemHold` / `ItemRelease`). The paint is a draw
//! plan (`guidraw`'s operations, the dotted lines' pixel loops the C++'s `EE_WON_DRAW_DOTTED_LINE`).
//!
//! Every method is the original's statements in order with the calls out at the same points (`guitree` and
//! `guimulti` keep the decisions). The states are behind `RefCell`s never borrowed across a call out.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guidraw::{
    EeWonDrawOp, EE_WON_DRAW_DOTTED_LINE, EE_WON_DRAW_LINE, EE_WON_DRAW_RECT, EE_WON_DRAW_SET_SYS_COLOR,
    EE_WON_DRAW_TRANSLATE,
};
use crate::guihost::{EeWonGuiCursor as Cursor, EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guimulti;
use crate::guitree::{self, TreeNodes};
use crate::listobj::{EeWonList, EeWonListCall as L};

/// `ListSortType_None`.
const SORT_NONE: i32 = 2;
/// `StandardColor_3DShadow`, `StandardColor_Text`.
const COLOR_SHADOW: i32 = 3;
const COLOR_TEXT: i32 = 14;

fn call_host(host: EeWonGuiHostFn, comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    match host {
        // SAFETY: the host was given with this object's component; no borrow of a state is live here
        Some(h) => unsafe { h(comp, op, a, b, c, d) },
        None => 0,
    }
}

// ---- the tree

#[derive(Debug)]
struct TreeState {
    /// the root row (a C++ TreeItem, held)
    root: isize,
    flags: i32,
    mouse_col_item: isize,
    sort_type: i32,
}

/// The inward calls of the tree (`TreeArea`'s virtuals and methods; rows are `TreeItem*`s).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonTreeCall {
    /// the constructor's steps (the root row made and opened, the list flags, the tree flags).
    Init = 1,
    /// `HandleComponentEvent(a)`.
    HandleEvent,
    NotifyMouseItemGone,
    /// `SetSortType(a)`.
    SetSortType,
    /// `Sort(a, b != 0)` (`a` a row or NULL: the root).
    SortItem,
    /// `SetTreeFlags(a, b != 0)`.
    SetTreeFlags,
    /// `mTreeFlags`.
    TreeFlags,
    /// `InsertTreeItem(a, b, c)`: the new row.
    InsertTreeItem,
    /// `RemoveTreeItem(a)`.
    RemoveTreeItem,
    /// `RemoveAllItems(a)` (`ClearTree` with NULL).
    RemoveAllItems,
    /// `OpenItem(a, b != 0)`.
    OpenItem,
    /// `CloseItem(a, b != 0, c != 0)`.
    CloseItem,
    /// `ToggleItem(a, b != 0, c != 0)`.
    ToggleItem,
    /// `mRoot`.
    Root,
    /// `mTreeSortType`.
    SortType,
    /// `mMouseColItem`.
    MouseColItem,
}

/// The Rust TreeArea.
pub struct EeWonTree {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    list: *const EeWonList,
    st: RefCell<TreeState>,
}

impl EeWonTree {
    /// The state before the constructor's steps (`Init`). `list` is the tree's own Rust ListArea (its base). The
    /// original left the tree's sort type unset; here it is none until the config or the room control sets it.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, list: *const EeWonList) -> Self {
        EeWonTree {
            comp,
            host,
            list,
            st: RefCell::new(TreeState { root: 0, flags: 0, mouse_col_item: 0, sort_type: SORT_NONE }),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        call_host(self.host, self.comp, op, a, b, c, d)
    }
    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0)
    }
    fn get(&self, f: F) -> i32 {
        self.op(Op::Get, f as isize, 0, 0, 0) as i32
    }
    fn base(&self, c: L, a: isize, b: isize, cc: isize) -> isize {
        if self.list.is_null() {
            return 0;
        }
        // SAFETY: the list's Rust object lives as long as the C++ tree (its base's destructor frees it after this
        // one); a shared reference: the calls out re-enter through it
        unsafe { (*self.list).call(c, a, b, cc) }
    }
    fn flag(&self, f: i32) -> bool {
        self.st.borrow().flags & f != 0
    }
    /// A row's Rust state.
    fn node(&self, item: isize) -> &EeWonTNode {
        let p = self.op1(Op::NodeOf, item) as *const EeWonTNode;
        // SAFETY: a row lives while the tree or a parent holds it; a shared reference (the calls out re-enter)
        unsafe { &*p }
    }

    pub fn init(&self) {
        let root = self.op1(Op::NewTreeItem, 0);
        self.st.borrow_mut().root = root;
        self.node(root).set_open(self, true, false, false);
        self.base(L::SetListFlags, 0x30, 0, 0);
        self.base(L::SetListFlags, 0x200, 1, 0);
        let mut s = self.st.borrow_mut();
        s.flags = 3;
        s.mouse_col_item = 0;
    }

    pub fn set_sort_type(&self, kind: i32) {
        if self.st.borrow().sort_type == kind {
            return;
        }
        self.st.borrow_mut().sort_type = kind;
        self.op1(Op::VirtSort, 0);
    }

    pub fn notify_mouse_item_gone(&self) {
        self.base(L::NotifyMouseItemGone, 0, 0, 0);
        self.st.borrow_mut().mouse_col_item = 0;
    }

    pub fn insert_tree_item(&self, parent: isize, item: isize, pos: i32) -> isize {
        let parent = if parent == 0 { self.st.borrow().root } else { parent };
        let n = self.op1(Op::NewTreeItem, 0);
        self.node(n).set_item(item);
        let sel = self.op1(Op::ItemSelectable, item);
        self.op(Op::ItemSetSelectable, n, sel, 0, 0);
        self.node(parent).insert_child(self, n, pos);
        // the creator's SmartPtr goes: the parent holds the row
        self.op1(Op::ItemRelease, n);
        n
    }

    pub fn remove_all_items(&self, parent: isize) {
        if parent == 0 {
            self.base(L::Clear, 1, 0, 0);
            let root = self.st.borrow().root;
            self.node(root).clear_children();
        } else {
            self.node(parent).remove_all_children(self);
        }
    }

    pub fn handle_event(&self, event: isize) {
        let kind = self.op1(Op::EventType, event) as i32;
        let id = self.op1(Op::EventCompControlId, event) as i32;
        if guitree::is_expand_press(kind, id) {
            let top = self.op1(Op::EventComponentTop, event) as i32;
            let item = self.base(L::FindItemAt, top as isize, 0, 0);
            if item != 0 {
                let all = self.flag(4);
                self.node(item).toggle_open(self, all, true);
            }
        } else {
            self.op1(Op::BaseHandleEvent, event);
        }
    }

    pub fn sort_item(&self, item: isize, recursive: bool) {
        let open = item != 0 && self.node(item).open();
        let row = if open { self.base(L::GetItemPos, item, 0, 0) as i32 } else { -1 };
        let plan = guitree::sort_plan(self.st.borrow().sort_type == SORT_NONE, item == 0, open, row);
        if plan == 0 {
            return;
        }
        if plan == 1 {
            self.base(L::Clear, 0, 0, 0);
            let root = self.st.borrow().root;
            self.node(root).sort(self, recursive);
            self.node(root).insert_children(self, 0);
            return;
        }
        let visible = plan == 3;
        let n = self.node(item);
        if visible {
            n.set_open(self, false, false, false);
        }
        n.sort(self, recursive);
        if visible {
            n.set_open(self, true, false, false);
        }
    }

    /// One inward call.
    pub fn call(&self, call: EeWonTreeCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonTreeCall;
        match call {
            C::Init => self.init(),
            C::HandleEvent => self.handle_event(a),
            C::NotifyMouseItemGone => self.notify_mouse_item_gone(),
            C::SetSortType => self.set_sort_type(a as i32),
            C::SortItem => self.sort_item(a, b != 0),
            C::SetTreeFlags => {
                let mut s = self.st.borrow_mut();
                s.flags = guitree::tree_flags(s.flags, a as i32, b != 0);
            }
            C::TreeFlags => return self.st.borrow().flags as isize,
            C::InsertTreeItem => return self.insert_tree_item(a, b, c as i32),
            C::RemoveTreeItem => self.node(a).remove_from_parent(self),
            C::RemoveAllItems => self.remove_all_items(a),
            C::OpenItem => self.node(a).set_open(self, true, false, b != 0),
            C::CloseItem => self.node(a).set_open(self, false, b != 0, c != 0),
            C::ToggleItem => self.node(a).toggle_open(self, b != 0, c != 0),
            C::Root => return self.st.borrow().root,
            C::SortType => return self.st.borrow().sort_type as isize,
            C::MouseColItem => return self.st.borrow().mouse_col_item,
        }
        0
    }
}

impl Drop for EeWonTree {
    fn drop(&mut self) {
        let root = std::mem::take(&mut self.st.borrow_mut().root);
        if root != 0 {
            self.op1(Op::ItemRelease, root);
        }
    }
}

// ---- a row

#[derive(Debug)]
struct NodeState {
    /// the parent row (borrowed), or 0
    parent: isize,
    x: i32,
    index: i32,
    open: bool,
    /// the wrapped item (held), or 0
    item: isize,
    /// the children (held)
    children: Vec<isize>,
}

/// The inward calls of a row (`TreeItem`'s virtuals and methods; `tree` is given for the ones that have one).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonTNodeCall {
    /// `Paint(*a, tree)`.
    Paint = 1,
    /// `CalcDimensions(tree)`.
    CalcDimensions,
    /// `SetViewport(tree, a)`.
    SetViewport,
    /// `Compare(a, tree)`.
    Compare,
    /// `CanSelect(tree, a, b)` (0 or 1).
    CanSelect,
    /// `SetPos(a, b)` (no tree).
    SetPos,
    /// `MouseEnter(tree, a, b)`.
    MouseEnter,
    /// `MouseExit(tree)`.
    MouseExit,
    /// `MouseMove(tree, a, b)`.
    MouseMove,
    /// `MouseDrag(tree, a, b)`.
    MouseDrag,
    /// `MouseDown(tree, a, b, c)`.
    MouseDown,
    /// `MouseUp(tree, a, b, c)`.
    MouseUp,
    /// `GetChild(a)` (no tree).
    GetChild,
    /// `GetNextNonChild()` (no tree).
    GetNextNonChild,
    /// `GetNextNonChildPos(tree)`.
    GetNextNonChildPos,
    /// `InsertChild(tree, a, b)`.
    InsertChild,
    /// `RemoveAllChildren(tree)`.
    RemoveAllChildren,
    /// `RemoveChild(tree, a)`.
    RemoveChild,
    /// `RemoveFromParent(tree)`.
    RemoveFromParent,
    /// `CloseAll()` (no tree).
    CloseAll,
    /// `InsertChildren(tree, a)`.
    InsertChildren,
    /// `SetOpen(tree, a != 0, b != 0, c != 0)`.
    SetOpen,
    /// `ToggleOpen(tree, a != 0, b != 0)`.
    ToggleOpen,
    /// `IsLastChild()` (no tree; 0 or 1).
    IsLastChild,
    /// `ContainsPoint(a, b)` (no tree; 0 or 1).
    ContainsPoint,
    /// `Sort(tree, a != 0)`.
    Sort,
    /// `mItem` (no tree).
    Item,
    /// `mChildren.size()` (no tree).
    NumChildren,
    /// `mParent` (no tree).
    Parent,
    /// `mOpen` (no tree; 0 or 1).
    IsOpen,
    /// `mX` (no tree).
    Indent,
    /// `mIndex` (no tree).
    Index,
}

/// The Rust TreeItem.
pub struct EeWonTNode {
    /// the C++ row (the `a` of the item ops on the row itself)
    me: isize,
    /// the row's own host: `NodeOf`, `ItemSetPosXY` and the references (`ItemHold` / `ItemRelease`)
    host: EeWonGuiHostFn,
    st: RefCell<NodeState>,
}

/// The walks' view of the rows (`guitree`'s questions through `NodeOf`).
struct Walk<'a> {
    from: &'a EeWonTNode,
}

impl TreeNodes for Walk<'_> {
    type Node = isize;
    fn parent(&self, n: isize) -> Option<isize> {
        let p = self.from.of(n).st.borrow().parent;
        (p != 0).then_some(p)
    }
    fn index(&self, n: isize) -> i32 {
        self.from.of(n).st.borrow().index
    }
    fn count(&self, n: isize) -> usize {
        self.from.of(n).st.borrow().children.len()
    }
    fn child(&self, n: isize, i: usize) -> isize {
        self.from.of(n).st.borrow().children.get(i).copied().unwrap_or(0)
    }
    fn is_open(&self, n: isize) -> bool {
        self.from.of(n).st.borrow().open
    }
}

impl EeWonTNode {
    /// A closed row at indent 0 with no parent and no index (the C++ constructor's state).
    pub fn new(me: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonTNode {
            me: me as isize,
            host,
            st: RefCell::new(NodeState { parent: 0, x: 0, index: -1, open: false, item: 0, children: Vec::new() }),
        }
    }

    fn own(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        call_host(self.host, self.me as *mut c_void, op, a, b, c, 0)
    }
    /// Another row's Rust state (this one's own when `item` is it).
    fn of(&self, item: isize) -> &EeWonTNode {
        if item == self.me {
            return self;
        }
        let p = self.own(Op::NodeOf, item, 0, 0) as *const EeWonTNode;
        // SAFETY: a row lives while the tree or a parent holds it; a shared reference (the calls out re-enter)
        unsafe { &*p }
    }
    fn open(&self) -> bool {
        self.st.borrow().open
    }
    fn x(&self) -> i32 {
        self.st.borrow().x
    }
    fn item(&self) -> isize {
        self.st.borrow().item
    }
    fn count(&self) -> usize {
        self.st.borrow().children.len()
    }
    fn child_at(&self, i: usize) -> isize {
        self.st.borrow().children.get(i).copied().unwrap_or(0)
    }

    /// `mItem = item` (a `SmartPtr` assignment).
    pub fn set_item(&self, item: isize) {
        let old = self.item();
        if old != item {
            if old != 0 {
                self.own(Op::ItemRelease, old, 0, 0);
            }
            self.st.borrow_mut().item = item;
            if item != 0 {
                self.own(Op::ItemHold, item, 0, 0);
            }
        }
    }

    /// `mChildren.clear()`.
    fn clear_children(&self) {
        let gone = std::mem::take(&mut self.st.borrow_mut().children);
        for c in gone {
            self.own(Op::ItemRelease, c, 0, 0);
        }
    }

    /// `GetMouseItem(tree, x, y)`.
    fn mouse_item(&self, t: &EeWonTree, x: &mut i32, y: i32) -> isize {
        let new = self.item();
        let col = t.st.borrow().mouse_col_item;
        let yy = y.wrapping_add(t.op1(Op::ItemTop, self.me) as i32);
        let items = [col, new];
        let xx = *x;
        let steps = guitree::mouse_item(col != 0, col == new, new != 0, &mut |which| {
            t.op(Op::ItemContains, items[which as usize], xx as isize, yy as isize, 0) != 0
        });
        if steps & 1 != 0 {
            t.op1(Op::ItemMouseExit, col);
            t.op1(Op::SetCursor, Cursor::None as isize);
            t.st.borrow_mut().mouse_col_item = 0;
        }
        if steps & 2 != 0 {
            t.st.borrow_mut().mouse_col_item = new;
            t.op(Op::ItemMouseEnter, new, *x as isize, y as isize, 0);
        }
        *x = x.wrapping_sub(self.x());
        t.st.borrow().mouse_col_item
    }

    pub fn get_child(&self, pos: i32) -> isize {
        if !guitree::child_in_range(pos, self.count()) {
            return 0;
        }
        self.child_at(pos as usize)
    }

    pub fn next_non_child(&self) -> isize {
        guitree::next_non_child(&Walk { from: self }, self.me).unwrap_or(0)
    }

    pub fn next_non_child_pos(&self, t: &EeWonTree) -> i32 {
        let n = self.next_non_child();
        if n != 0 {
            t.base(L::GetItemPos, n, 0, 0) as i32
        } else {
            -1
        }
    }

    fn insert_pos(&self, t: &EeWonTree, item: isize, pos: i32) -> i32 {
        let kind = t.st.borrow().sort_type;
        if kind == SORT_NONE {
            return pos;
        }
        let v = self.st.borrow().children.clone();
        t.op(Op::LowerBound, v.as_ptr() as isize, v.len() as isize, item, kind as isize) as i32
    }

    pub fn insert_child(&self, t: &EeWonTree, item: isize, pos: i32) {
        let pos = self.insert_pos(t, item, pos);
        let (is_root, x, n) = {
            let s = self.st.borrow();
            (s.parent == 0, s.x, s.children.len())
        };
        let (indent, place) = guimulti::tree_child(is_root, t.flag(2 | 1) as i32, x, pos, n);
        let child = self.of(item);
        child.st.borrow_mut().x = indent;
        let on_end = place == n;
        self.st.borrow_mut().children.insert(place, item);
        self.own(Op::ItemHold, item, 0, 0);
        child.st.borrow_mut().parent = self.me;
        let mut i = place;
        while i < self.count() {
            let c = self.child_at(i);
            self.of(c).st.borrow_mut().index = i as i32;
            i += 1;
        }
        let row = guitree::insert_row(self.open(), on_end);
        if row == 0 {
            return;
        }
        let at = if row == 1 {
            self.next_non_child_pos(t)
        } else {
            t.base(L::GetItemPos, self.child_at(place + 1), 0, 0) as i32
        };
        t.base(L::InsertItem, item, at as isize, 0);
    }

    pub fn remove_all_children(&self, t: &EeWonTree) {
        self.set_open(t, false, false, false);
        self.clear_children();
        t.op1(Op::Invalidate, 0);
    }

    pub fn remove_child(&self, t: &EeWonTree, item: isize) {
        t.base(L::BeginUpdate, 0, 0, 0);
        let child = self.of(item);
        let plan = guitree::remove_plan(child.open(), self.open(), self.count());
        if plan & 1 != 0 {
            child.set_open(t, false, false, false);
        }
        if plan & 2 != 0 {
            t.base(L::RemoveItem, item, 0, 0);
        }
        let index = child.st.borrow().index;
        let gone = {
            let mut s = self.st.borrow_mut();
            let i = index as isize as usize;
            if i < s.children.len() {
                Some(s.children.remove(i))
            } else {
                None
            }
        };
        if let Some(g) = gone {
            self.own(Op::ItemRelease, g, 0, 0);
        }
        for i in 0..self.count() {
            let c = self.child_at(i);
            self.of(c).st.borrow_mut().index = i as i32;
        }
        if plan & 4 != 0 {
            t.op1(Op::Invalidate, 0);
        }
        t.base(L::EndUpdate, 0, 0, 0);
    }

    pub fn remove_from_parent(&self, t: &EeWonTree) {
        let p = self.st.borrow().parent;
        if p == 0 {
            return;
        }
        self.of(p).remove_child(t, self.me);
    }

    pub fn close_all(&self) {
        guitree::close_all(&Walk { from: self }, self.me, &mut |n| self.of(n).st.borrow_mut().open = false);
    }

    pub fn insert_children(&self, t: &EeWonTree, pos: i32) {
        guitree::insert_children(&Walk { from: self }, self.me, pos, &mut |n, p| {
            t.base(L::InsertItem, n, p as isize, 0);
        });
    }

    pub fn set_open(&self, t: &EeWonTree, want: bool, close_all: bool, send_event: bool) {
        let action = {
            let s = self.st.borrow();
            guitree::set_open_action(s.open, want, s.children.is_empty())
        };
        if action == 0 {
            return;
        }
        self.st.borrow_mut().open = want;
        if action == 1 {
            return;
        }
        if action == 2 {
            let p1 = t.base(L::GetItemPos, self.child_at(0), 0, 0);
            let p2 = self.next_non_child_pos(t);
            t.base(L::DeleteRange, p1, p2 as isize, 0);
            if close_all {
                self.close_all();
            }
        } else {
            t.base(L::BeginUpdate, 0, 0, 0);
            let at = (t.base(L::GetItemPos, self.me, 0, 0) as i32).wrapping_add(1);
            self.insert_children(t, at);
            t.base(L::EndUpdate, 0, 0, 0);
            let n = self.next_non_child();
            let total = t.get(F::TotalHeight);
            let sibling = if n != 0 { Some(t.op1(Op::ItemTop, n) as i32) } else { None };
            let vo = t.get(F::VertOffset);
            let vh = t.get(F::ScrollHeight);
            let top = t.op1(Op::ItemTop, self.me) as i32;
            if let Some(o) = guitree::open_scroll(total, sibling, vo, vh, top) {
                t.op1(Op::SetVertOffset, o as isize);
            }
        }
        if send_event {
            t.op1(Op::DeliverTreeOpened, self.me);
        }
    }

    pub fn toggle_open(&self, t: &EeWonTree, close_all: bool, send_event: bool) {
        let open = self.open();
        self.set_open(t, !open, close_all, send_event);
    }

    pub fn is_last_child(&self) -> bool {
        let (p, index) = {
            let s = self.st.borrow();
            (s.parent, s.index)
        };
        p != 0 && guitree::is_last(index, self.of(p).count())
    }

    pub fn set_pos(&self, y: i32) {
        self.own(Op::ItemSetTop, self.me, y as isize, 0);
        let (item, x) = (self.item(), self.x());
        self.own(Op::ItemSetPosXY, item, x as isize, y as isize);
    }

    pub fn paint(&self, t: &EeWonTree, g: isize) {
        let draw = |kind: i32, a: i32, b: i32, c: i32, d: i32| {
            let o = EeWonDrawOp { kind, a, b, c, d, ..EeWonDrawOp::default() };
            t.op(Op::Draw, g, &o as *const EeWonDrawOp as isize, 0, 0);
        };
        let height = || t.op1(Op::ItemHeight, self.me) as i32;
        let draw_button = t.flag(2) && self.count() != 0;
        let x = self.x();
        let square_left = x.wrapping_sub(14);
        let square_right = square_left.wrapping_add(8);
        let square_top = height().wrapping_sub(8) / 2;
        let square_bottom = square_top.wrapping_add(8);
        let (mut line_top, mut line_bottom, mut line_left) = (square_top, square_bottom, square_right);
        if !draw_button {
            line_top = square_top.wrapping_add(4);
            line_bottom = line_top;
            line_left = square_left.wrapping_add(6);
        }
        if t.flag(1) {
            draw(EE_WON_DRAW_SET_SYS_COLOR, COLOR_SHADOW, 0, 0, 0);
            if t.op1(Op::ItemTop, self.me) != 0 {
                draw(EE_WON_DRAW_DOTTED_LINE, square_left.wrapping_add(4), 0, line_top.wrapping_add(1), 1);
            }
            if !self.is_last_child() {
                let h = height().wrapping_sub(line_bottom);
                draw(EE_WON_DRAW_DOTTED_LINE, square_left.wrapping_add(4), line_bottom, h, 1);
            }
            draw(EE_WON_DRAW_DOTTED_LINE, line_left, square_top.wrapping_add(4), x.wrapping_sub(line_left), 0);
            let mut p = self.st.borrow().parent;
            while p != 0 {
                let n = self.of(p);
                if n.x() <= 0 {
                    break;
                }
                if !n.is_last_child() {
                    draw(EE_WON_DRAW_DOTTED_LINE, n.x().wrapping_sub(10), 0, height(), 1);
                }
                p = n.st.borrow().parent;
            }
        }
        if draw_button {
            draw(EE_WON_DRAW_SET_SYS_COLOR, COLOR_SHADOW, 0, 0, 0);
            draw(EE_WON_DRAW_RECT, square_left, square_top, 9, 9);
            draw(EE_WON_DRAW_SET_SYS_COLOR, COLOR_TEXT, 0, 0, 0);
            let mid = square_top.wrapping_add(4);
            draw(EE_WON_DRAW_LINE, square_left.wrapping_add(2), mid, square_right.wrapping_sub(2), mid);
            if !self.open() {
                let cx = square_left.wrapping_add(4);
                draw(EE_WON_DRAW_LINE, cx, square_top.wrapping_add(2), cx, square_bottom.wrapping_sub(2));
            }
        }
        draw(EE_WON_DRAW_TRANSLATE, x, 0, 0, 0);
        let item = self.item();
        let sel = t.op1(Op::ItemSelected, self.me);
        t.op(Op::ItemSetSelected, item, sel, 0, 0);
        t.op(Op::ItemPaint, item, g, 0, 0);
        draw(EE_WON_DRAW_TRANSLATE, x.wrapping_neg(), 0, 0, 0);
    }

    pub fn calc_dimensions(&self, t: &EeWonTree) {
        let item = self.item();
        t.op1(Op::ItemCalcDimensions, item);
        let w = guitree::row_width(self.x(), t.op1(Op::ItemWidth, item) as i32);
        t.op(Op::ItemSetWidth, self.me, w as isize, 0, 0);
        let h = t.op1(Op::ItemHeight, item);
        t.op(Op::ItemSetHeight, self.me, h, 0, 0);
    }

    pub fn can_select(&self, t: &EeWonTree, x: i32) -> bool {
        let selectable = t.op1(Op::ItemSelectable, self.me) != 0;
        guitree::can_select(selectable, x, self.x())
    }

    pub fn contains_point(&self, x: i32, y: i32) -> bool {
        let Some(ax) = guitree::local_x(x, self.x()) else { return false };
        let yy = y.wrapping_add(self.own(Op::ItemTop, self.me, 0, 0) as i32);
        self.own(Op::ItemContains, self.item(), ax as isize, yy as isize) != 0
    }

    pub fn sort(&self, t: &EeWonTree, recursive: bool) {
        if self.count() == 0 {
            return;
        }
        let kind = t.st.borrow().sort_type;
        let mut v = self.st.borrow().children.clone();
        t.op(Op::SortItems, v.as_mut_ptr() as isize, v.len() as isize, kind as isize, 0);
        self.st.borrow_mut().children = v.clone();
        for (i, c) in v.into_iter().enumerate() {
            let n = self.of(c);
            n.st.borrow_mut().index = i as i32;
            if recursive {
                n.sort(t, recursive);
            }
        }
    }

    /// One inward call; `t` is the row's tree (NULL for the calls that need none).
    pub fn call(&self, t: Option<&EeWonTree>, call: EeWonTNodeCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonTNodeCall;
        let (mut ai, bi) = (a as i32, b as i32);
        match call {
            C::SetPos => {
                self.set_pos(bi);
                return 0;
            }
            C::GetChild => return self.get_child(ai),
            C::GetNextNonChild => return self.next_non_child(),
            C::CloseAll => {
                self.close_all();
                return 0;
            }
            C::IsLastChild => return self.is_last_child() as isize,
            C::ContainsPoint => return self.contains_point(ai, bi) as isize,
            C::Item => return self.item(),
            C::NumChildren => return self.count() as isize,
            C::Parent => return self.st.borrow().parent,
            C::IsOpen => return self.open() as isize,
            C::Indent => return self.x() as isize,
            C::Index => return self.st.borrow().index as isize,
            _ => {}
        }
        let Some(t) = t else { return 0 };
        match call {
            C::Paint => self.paint(t, a),
            C::CalcDimensions => self.calc_dimensions(t),
            C::SetViewport => {
                t.op(Op::ItemSetViewport, self.item(), a, 0, 0);
            }
            C::Compare => {
                let other = self.of(a).item();
                return t.op(Op::ItemCompare, self.item(), other, 0, 0);
            }
            C::CanSelect => return self.can_select(t, ai) as isize,
            C::MouseEnter => {
                self.mouse_item(t, &mut ai, bi);
            }
            C::MouseExit => {
                let col = t.st.borrow().mouse_col_item;
                if col != 0 {
                    t.op1(Op::ItemMouseExit, col);
                    t.base(L::ClearMouseItem, 0, 0, 0);
                }
            }
            C::MouseMove => {
                let it = self.mouse_item(t, &mut ai, bi);
                if it != 0 {
                    t.op(Op::ItemMouseMove, it, ai as isize, b, 0);
                }
            }
            C::MouseDrag => {
                let col = t.st.borrow().mouse_col_item;
                if col != 0 {
                    t.op(Op::ItemMouseDrag, col, ai.wrapping_sub(self.x()) as isize, b, 0);
                }
            }
            C::MouseDown => {
                if c == 0
                    && (guimulti::tree_box_hit(ai, self.x()) || (t.op1(Op::CheckDoubleClick, c) != 0 && t.flag(8)))
                {
                    self.toggle_open(t, t.flag(4), true);
                    return 0;
                }
                let it = self.mouse_item(t, &mut ai, bi);
                if it != 0 {
                    t.op(Op::ItemMouseDown, it, ai as isize, b, c);
                }
            }
            C::MouseUp => {
                let it = self.mouse_item(t, &mut ai, bi);
                if it != 0 {
                    t.op(Op::ItemMouseUp, it, ai as isize, b, c);
                }
            }
            C::GetNextNonChildPos => return self.next_non_child_pos(t) as isize,
            C::InsertChild => self.insert_child(t, a, bi),
            C::RemoveAllChildren => self.remove_all_children(t),
            C::RemoveChild => self.remove_child(t, a),
            C::RemoveFromParent => self.remove_from_parent(t),
            C::InsertChildren => self.insert_children(t, ai),
            C::SetOpen => self.set_open(t, a != 0, b != 0, c != 0),
            C::ToggleOpen => self.toggle_open(t, a != 0, b != 0),
            C::Sort => self.sort(t, a != 0),
            _ => {}
        }
        0
    }
}

impl Drop for EeWonTNode {
    fn drop(&mut self) {
        // the original's members go in reverse order: the children, then the wrapped item
        self.clear_children();
        let item = std::mem::take(&mut self.st.borrow_mut().item);
        if item != 0 {
            self.own(Op::ItemRelease, item, 0, 0);
        }
    }
}

// ---- the C ABI

/// A new Rust TreeArea for the C++ `component`, its calls out through `host`, its base the Rust list `list`.
#[no_mangle]
pub extern "C" fn ee_won_tree_new(
    component: *mut c_void,
    host: EeWonGuiHostFn,
    list: *const EeWonList,
) -> *mut EeWonTree {
    Box::into_raw(Box::new(EeWonTree::new(component, host, list)))
}

/// # Safety
/// `tree` came from `ee_won_tree_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tree_free(tree: *mut EeWonTree) {
    if !tree.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(tree) });
    }
}

/// One of TreeArea's methods (`EeWonTreeCall`).
///
/// # Safety
/// `tree` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tree_call(
    tree: *const EeWonTree,
    call: EeWonTreeCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*tree).call(call, a, b, c) }
}

/// A new Rust TreeItem for the C++ row `component`; `host` answers `NodeOf`, `ItemSetTop`, `ItemSetPosXY`,
/// `ItemTop`, `ItemContains` and the references.
#[no_mangle]
pub extern "C" fn ee_won_tnode_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonTNode {
    Box::into_raw(Box::new(EeWonTNode::new(component, host)))
}

/// # Safety
/// `node` came from `ee_won_tnode_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tnode_free(node: *mut EeWonTNode) {
    if !node.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(node) });
    }
}

/// One of TreeItem's methods (`EeWonTNodeCall`); `tree` is the row's tree's Rust object (NULL for the calls that
/// need none).
///
/// # Safety
/// `node` is live; `tree` is live or NULL; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_tnode_call(
    node: *const EeWonTNode,
    tree: *const EeWonTree,
    call: EeWonTNodeCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract)
    unsafe { (*node).call(tree.as_ref(), call, a, b, c) }
}

#[cfg(test)]
#[path = "treeobj_tests.rs"]
mod tests;
