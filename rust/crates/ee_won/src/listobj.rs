//! WONGUI's ListArea as a whole Rust object (R6 WON round 23): every list of the lobby (the LAN game list, the
//! user, friend, room and server lists, the combo boxes' drop-downs, the popup menus, the multi-column and tree
//! lists through their C++ subclasses). The original's ListArea.cpp and ListControlAdapters' SetSortType, with
//! their state: the items, the top line, the list flags, the batch depth, the paint width, the sort type, the
//! current, keyboard and mouse items and the selection walk.
//!
//! The C++ `ListArea` stays the type the lobby holds and the base its subclasses (MultiListArea, TreeArea,
//! PopupMenu) derive from: its virtuals and methods are one-line forwarders to `ee_won_list_call`, and every call
//! out is an op of the component op table (`guihost`). The C++ items (`ListItem` and the lobby's classes over it)
//! stay data the lobby makes: Rust holds a reference to each and reads and sets their tops, sizes and selection
//! and calls their virtuals through ops. The sort and the sorted insert's search stay the C++ `std::sort` /
//! `std::lower_bound` over the items with the list's compare function (ops `SortItems`, `LowerBound`, `Less`), so
//! equal items keep the original's order. The C++ keeps the font, the colours, the selection colours, the sort
//! function and the item tool tip.
//!
//! Every method is the original's statements in order with the calls out at the same points (`guilist` and
//! `guidraw` keep the decisions, as before). The state is behind a `RefCell` never borrowed across a call out: the
//! list's own virtuals (AdjustPosition, Sort, NotifyMouseItemGone, ListSelChangedHook) go back through the C++
//! object (a subclass overrides them and calls the base back), and the selection event runs the lobby's handler.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guidraw::{self, EeWonDrawOp, EeWonRowPlace, EE_WON_DRAW_CALL};
use crate::guihost::{EeWonGuiEvent as Event, EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guilist;

/// `ListSortType_None`.
const SORT_NONE: i32 = 2;
/// The list flags the methods test.
const SELECT_ON_HOVER: i32 = 2;
const CLEAR_ON_EXIT: i32 = 4;
const TOGGLE: i32 = 8;
const FULL_LINE: i32 = 0x40;
const PARTIAL: i32 = 0x80;
const WIDTH_CHECK: i32 = 0x200;
const MULTI: i32 = 0x400;
const WRAP: i32 = 0x800;
const FIXED: i32 = 0x1000;
const ITEM_TIPS: i32 = 0x2000;

/// The virtual keys `KeyDown` reads.
const VK_PRIOR: i32 = 0x21;
const VK_NEXT: i32 = 0x22;
const VK_LEFT: i32 = 0x25;
const VK_UP: i32 = 0x26;
const VK_RIGHT: i32 = 0x27;
const VK_DOWN: i32 = 0x28;

/// A click the list delivers (`ListItemClickedEvent`'s arguments besides the item; `DeliverItemClicked`'s `b`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonItemClick {
    pub button: i32,
    pub up: bool,
    pub x: i32,
    pub y: i32,
}

/// The inward calls (the C++ shell's virtuals and methods). Items are `ListItem*`s.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonListCall {
    /// `ListArea::Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `ListArea::AdjustPosition(a != 0)`.
    AdjustPosition,
    /// `ListArea::ScrollPaint(*a)` (`a` the Graphics).
    ScrollPaint,
    /// `ListArea::MouseEnter(a, b)`.
    MouseEnter,
    MouseExit,
    /// `ListArea::MouseDown(a, b, c)`.
    MouseDown,
    /// `ListArea::MouseUp(a, b, c)`.
    MouseUp,
    /// `ListArea::MouseMove(a, b)`.
    MouseMove,
    /// `ListArea::MouseDrag(a, b)`.
    MouseDrag,
    /// `ListArea::KeyDown(a)` (0 or 1).
    KeyDown,
    /// `ListArea::NotifyMouseItemGone()`.
    NotifyMouseItemGone,
    /// `ListArea::Sort()` (0 or 1).
    Sort,
    BeginUpdate,
    EndUpdate,
    /// `Clear(a != 0)`.
    Clear,
    /// `GetItem(a)`.
    GetItem,
    /// `InsertItem(a, b)`: the item.
    InsertItem,
    /// `RefreshItem(a)`.
    RefreshItem,
    /// `DeleteItem(a)`.
    DeleteItem,
    /// `RemoveItem(a)`.
    RemoveItem,
    /// `DeleteRange(a, b)`.
    DeleteRange,
    /// `SetListFlags(a, b != 0)`.
    SetListFlags,
    /// `mListFlags`.
    ListFlags,
    /// `GetNumItems()`.
    NumItems,
    /// `GetItemPos(a)`.
    GetItemPos,
    /// `SetSelItem(int a, b != 0)`.
    SetSelItemAt,
    /// `SetSelItem(ListItem* a, b != 0, c != 0)` (0 or 1).
    SetSelItem,
    /// `EnsureVisible(a)`.
    EnsureVisible,
    /// `ClearSelection(a != 0)`.
    ClearSelection,
    /// `SelectRange(a, b)`.
    SelectRange,
    /// `FindItemAt(a)`.
    FindItemAt,
    /// `IsItemVisible(a)` (0 or 1).
    IsItemVisible,
    EnforceInvariants,
    RewindSelections,
    /// `HasMoreSelections()` (0 or 1).
    HasMoreSelections,
    /// `GetNextSelection()`.
    GetNextSelection,
    /// `GetItemPosAtYPos(a)`.
    GetItemPosAtYPos,
    /// `FindSelectableItem(a, b != 0, c != 0)`.
    FindSelectableItem,
    /// `SetSortType(a)`.
    SetSortType,
    /// `mSortType`.
    SortType,
    /// `mSortType = a` (no sort).
    SetSortTypeOnly,
    /// `mSelItem`.
    SelItem,
    /// `mMouseItem`.
    MouseItem,
    /// `mMouseItem = NULL` (the field only).
    ClearMouseItem,
    /// `mPaintWidth`.
    PaintWidth,
    /// `mPaintWidth = a`.
    SetPaintWidth,
}

#[derive(Debug)]
struct State {
    items: Vec<isize>,
    top_line: i32,
    flags: i32,
    multi_change: i32,
    paint_width: i32,
    sort_type: i32,
    sel: isize,
    key: isize,
    mouse: isize,
    sel_itr: i32,
}

/// The Rust ListArea.
pub struct EeWonList {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonList {
    /// The constructor's state (the C++ constructor sets the font, the text colour, the focus flag and the
    /// padding).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonList {
            comp,
            host,
            st: RefCell::new(State {
                items: Vec::new(),
                top_line: 0,
                flags: 0xb0,
                multi_change: 0,
                paint_width: -1,
                sort_type: SORT_NONE,
                sel: 0,
                key: 0,
                mouse: 0,
                sel_itr: 0,
            }),
        }
    }

    // ---- calls out

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, d) },
            None => 0,
        }
    }
    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0)
    }
    fn get(&self, f: F) -> i32 {
        self.op(Op::Get, f as isize, 0, 0, 0) as i32
    }
    fn set(&self, f: F, v: i32) {
        self.op(Op::Set, f as isize, v as isize, 0, 0);
    }
    fn flag(&self, f: i32) -> bool {
        self.st.borrow().flags & f != 0
    }
    fn invalidate(&self) {
        self.op1(Op::Invalidate, 0);
    }
    fn top(&self, item: isize) -> i32 {
        self.op1(Op::ItemTop, item) as i32
    }
    fn height(&self, item: isize) -> i32 {
        self.op1(Op::ItemHeight, item) as i32
    }
    fn width(&self, item: isize) -> i32 {
        self.op1(Op::ItemWidth, item) as i32
    }
    fn selected(&self, item: isize) -> bool {
        self.op1(Op::ItemSelected, item) != 0
    }
    fn set_selected(&self, item: isize, on: bool) {
        self.op(Op::ItemSetSelected, item, on as isize, 0, 0);
    }
    fn items(&self) -> Vec<isize> {
        self.st.borrow().items.clone()
    }
    fn count(&self) -> usize {
        self.st.borrow().items.len()
    }
    fn item_at(&self, i: usize) -> isize {
        self.st.borrow().items.get(i).copied().unwrap_or(0)
    }
    fn virt_adjust(&self, b: bool) {
        self.op1(Op::VirtAdjustPosition, b as isize);
    }
    fn virt_mouse_gone(&self) {
        self.op1(Op::VirtNotifyMouseItemGone, 0);
    }

    // ---- the methods

    pub fn begin_update(&self) {
        self.st.borrow_mut().multi_change += 1;
    }

    pub fn end_update(&self) {
        let done = {
            let mut s = self.st.borrow_mut();
            s.multi_change -= 1;
            if s.multi_change <= 0 {
                s.multi_change = 0;
                true
            } else {
                false
            }
        };
        if done && self.op1(Op::VirtSort, 0) == 0 {
            self.enforce_invariants();
        }
    }

    fn find_line_at(&self, y: i32) -> usize {
        let items = self.items();
        guilist::find_line_at(items.len(), y, |i| self.top(items[i]))
    }

    pub fn find_item_at(&self, y: i32) -> isize {
        let i = self.find_line_at(y);
        if i == self.count() {
            return 0;
        }
        let item = self.item_at(i);
        let (t, h) = (self.top(item), self.height(item));
        if guilist::covers(t, h, y) {
            item
        } else {
            0
        }
    }

    fn find_top_line(&self) {
        let i = self.find_line_at(self.get(F::VertOffset));
        self.st.borrow_mut().top_line = i as i32;
    }

    pub fn is_item_visible(&self, item: isize) -> bool {
        let (t, h) = (self.top(item), self.height(item));
        guilist::is_item_visible(t, h, self.get(F::VertOffset), self.get(F::ScrollHeight), self.flag(PARTIAL))
    }

    pub fn ensure_visible(&self, item: isize) {
        let (t, h) = (self.top(item), self.height(item));
        let vo = self.get(F::VertOffset);
        let o = guilist::ensure_visible(t, h, vo, self.get(F::ScrollHeight)).unwrap_or(vo);
        if o != vo {
            self.op1(Op::SetVertOffset, o as isize);
        }
    }

    pub fn set_sel_item(&self, item: isize, add: bool, fire: bool) -> bool {
        if item == 0 {
            return self.clear_selection_prv(fire) > 0;
        }
        let old = self.st.borrow().sel;
        if item != old {
            if old != 0 && (!self.flag(MULTI) || !add) {
                let was = self.selected(item);
                self.clear_selection_prv(false);
                self.set_selected(item, was);
            }
            self.st.borrow_mut().sel = item;
        }
        let sel = self.st.borrow().sel;
        if sel != 0 && !self.selected(sel) {
            self.st.borrow_mut().sel = 0;
        }
        if item != old {
            self.invalidate();
        }
        if self.st.borrow().sel != old {
            self.invalidate();
            self.op1(Op::FocusChildLost, 0);
            if fire {
                self.op1(Op::FireEvent, Event::ListSelChanged as isize);
            }
            self.op1(Op::VirtListSelChangedHook, 0);
            return true;
        }
        false
    }

    pub fn select_range(&self, start: i32, end: i32) {
        self.clear_selection_prv(false);
        let Some((s, e, step)) = guilist::select_range(start, end, self.count() as i32) else {
            return;
        };
        let mut i = s;
        while i != e.wrapping_add(step) {
            self.set_selected(self.get_item(i), true);
            i = i.wrapping_add(step);
        }
        self.set_sel_item(self.get_item(s), false, true);
    }

    pub fn set_sel_item_at(&self, index: i32, fire: bool) {
        if index >= 0 && (index as usize) < self.count() {
            let item = self.item_at(index as usize);
            self.set_selected(item, true);
            self.st.borrow_mut().key = item;
            self.set_sel_item(item, false, fire);
        } else {
            self.set_sel_item(0, false, fire);
            self.st.borrow_mut().key = 0;
        }
    }

    pub fn rewind_selections(&self) {
        self.st.borrow_mut().sel_itr = -1;
        self.find_next_selection();
    }

    pub fn has_more_selections(&self) -> bool {
        let s = self.st.borrow();
        (s.sel_itr as u32 as usize) < s.items.len()
    }

    fn find_next_selection(&self) {
        self.st.borrow_mut().sel_itr += 1;
        loop {
            let (i, n) = {
                let s = self.st.borrow();
                (s.sel_itr as u32 as usize, s.items.len())
            };
            if i >= n || self.selected(self.item_at(i)) {
                break;
            }
            self.st.borrow_mut().sel_itr += 1;
        }
    }

    pub fn get_next_selection(&self) -> isize {
        if self.has_more_selections() {
            let item = self.item_at(self.st.borrow().sel_itr as usize);
            self.find_next_selection();
            return item;
        }
        0
    }

    pub fn clear_selection_prv(&self, fire: bool) -> i32 {
        let sel = self.st.borrow().sel;
        if sel == 0 {
            return 0;
        }
        let mut count = 0;
        if self.flag(MULTI) {
            for item in self.items() {
                if self.selected(item) {
                    self.set_selected(item, false);
                    count += 1;
                }
            }
        } else {
            count = 1;
            self.set_selected(sel, false);
        }
        self.st.borrow_mut().sel = 0;
        self.invalidate();
        if fire {
            self.op1(Op::FireEvent, Event::ListSelChanged as isize);
            self.op1(Op::VirtListSelChangedHook, 0);
        }
        count
    }

    pub fn get_item_pos(&self, item: isize) -> i32 {
        if item == 0 {
            return -1;
        }
        let mut i = self.find_line_at(self.top(item));
        while i < self.count() {
            let it = self.item_at(i);
            if it == item {
                return i as i32;
            }
            if self.top(it) > self.top(item) {
                break;
            }
            i += 1;
        }
        -1
    }

    pub fn get_item(&self, index: i32) -> isize {
        if index < 0 || index as u32 as usize >= self.count() {
            return 0;
        }
        self.item_at(index as usize)
    }

    pub fn clear(&self, fire: bool) {
        self.set(F::TotalHeight, 0);
        self.set(F::TotalWidth, 0);
        self.set(F::HorzOffset, 0);
        self.set(F::VertOffset, 0);
        self.clear_selection_prv(fire);
        let mouse = {
            let mut s = self.st.borrow_mut();
            s.sel = 0;
            s.key = 0;
            s.mouse
        };
        if mouse != 0 {
            self.virt_mouse_gone();
        }
        let gone = std::mem::take(&mut self.st.borrow_mut().items);
        for item in gone {
            self.op1(Op::ItemRelease, item);
        }
        self.virt_adjust(false);
    }

    pub fn enforce_invariants(&self) {
        if self.st.borrow().multi_change != 0 {
            return;
        }
        let items = self.items();
        let mut heights = Vec::with_capacity(items.len());
        let mut widths = Vec::with_capacity(items.len());
        for &item in &items {
            heights.push(self.height(item));
            widths.push(self.width(item));
        }
        let mut tops = vec![0i32; items.len()];
        let (th, tw) = guilist::stack(&heights, &widths, &mut tops);
        self.set(F::TotalHeight, th);
        self.set(F::TotalWidth, tw);
        for (n, &item) in items.iter().enumerate() {
            self.op(Op::ItemSetPos, item, tops[n] as isize, 0, 0);
        }
        self.virt_adjust(false);
    }

    pub fn sort_base(&self) -> bool {
        let kind = self.st.borrow().sort_type;
        if kind == SORT_NONE {
            return false;
        }
        let mut v = self.items();
        self.op(Op::SortItems, v.as_mut_ptr() as isize, v.len() as isize, kind as isize, 0);
        self.st.borrow_mut().items = v;
        self.enforce_invariants();
        true
    }

    fn find_insert_pos(&self, item: isize, position: i32) -> i32 {
        let (multi, kind, n) = {
            let s = self.st.borrow();
            (s.multi_change, s.sort_type, s.items.len())
        };
        if multi == 0 && kind != SORT_NONE {
            let v = self.items();
            return self.op(Op::LowerBound, v.as_ptr() as isize, v.len() as isize, item, kind as isize) as i32;
        }
        if position < 0 || position as u32 as usize > n {
            return n as i32;
        }
        position
    }

    pub fn adjust_position(&self, scrollbar: bool) {
        self.set(F::ExtraHeight, 0);
        let (th, sh) = (self.get(F::TotalHeight), self.get(F::ScrollHeight));
        if self.flag(FULL_LINE) && th > sh {
            let i = self.find_line_at(th.wrapping_sub(sh).wrapping_add(1));
            if i != self.count() {
                let item = self.item_at(i);
                let (t, h) = (self.top(item), self.height(item));
                self.set(F::ExtraHeight, guilist::full_line_extra(th, sh, t, h));
            }
        }
        self.op1(Op::BaseAdjustPosition, scrollbar as isize);
        self.find_top_line();
        let top = self.st.borrow().top_line;
        if !self.flag(FULL_LINE) || top as u32 as usize >= self.count() {
            return;
        }
        let item = self.item_at(top as usize);
        self.set(F::VertOffset, self.top(item));
        self.set(F::DownScrollAmount, self.height(item));
        if top > 0 {
            let prev = self.item_at(top as usize - 1);
            self.set(F::UpScrollAmount, self.height(prev));
        }
        self.op1(Op::BaseAdjustPosition, 1);
    }

    pub fn size_changed(&self) {
        self.op1(Op::BaseSizeChanged, 0);
        self.virt_adjust(false);
    }

    pub fn scroll_paint(&self, g: isize) {
        let items = self.items();
        let (paint_width, top) = {
            let s = self.st.borrow();
            (s.paint_width, s.top_line)
        };
        let (tw, sw) = (self.get(F::TotalWidth), self.get(F::ScrollWidth));
        let (vo, sh) = (self.get(F::VertOffset), self.get(F::ScrollHeight));
        let row = |i: usize| EeWonRowPlace { x: 0, top: self.top(items[i]), height: self.height(items[i]) };
        let (pw, ops) =
            guidraw::list_paint(tw, sw, paint_width, top as usize, items.len(), vo, sh, self.flag(PARTIAL), &row);
        self.st.borrow_mut().paint_width = pw;
        for o in &ops {
            if o.kind == EE_WON_DRAW_CALL {
                self.op(Op::ItemPaint, items.get(o.b as usize).copied().unwrap_or(0), g, 0, 0);
            } else {
                self.op(Op::Draw, g, o as *const EeWonDrawOp as isize, 0, 0);
            }
        }
        self.st.borrow_mut().paint_width = -1;
    }

    pub fn check_mouse_sel(&self, item: isize, x: i32, y: i32, down: bool) {
        if item == 0 {
            if self.flag(CLEAR_ON_EXIT) {
                self.clear_selection_prv(true);
            }
            return;
        }
        if !self.is_item_visible(item) {
            return;
        }
        let ry = y.wrapping_sub(self.top(item));
        if self.op(Op::ItemCanSelect, item, x as isize, ry as isize, 0) == 0 {
            if self.flag(CLEAR_ON_EXIT) {
                self.clear_selection_prv(true);
            }
            return;
        }
        self.st.borrow_mut().key = item;
        let mut add = false;
        if down && self.flag(MULTI) {
            let mods = self.op1(Op::RootKeyMod, 0);
            let sel = self.st.borrow().sel;
            if mods & 4 != 0 {
                add = true;
                let was = self.selected(item);
                self.set_selected(item, !was);
                self.invalidate();
            } else if mods & 1 != 0 && sel != 0 {
                let a = self.get_item_pos(sel);
                let b = self.get_item_pos(item);
                self.select_range(a, b);
                return;
            } else {
                let no_change = sel == item && self.clear_selection_prv(false) == 1;
                self.set_selected(item, true);
                if no_change {
                    self.st.borrow_mut().sel = item;
                }
            }
        } else if self.flag(SELECT_ON_HOVER) || !self.flag(TOGGLE) {
            self.set_selected(item, true);
        } else {
            let was = self.selected(item);
            self.set_selected(item, !was);
        }
        self.set_sel_item(item, add, true);
    }

    pub fn find_selectable_item(&self, start: i32, up: bool, opposite: bool) -> isize {
        let items = self.items();
        let wrap = self.flag(WRAP);
        match guilist::find_selectable(items.len(), start, up, opposite, wrap, |i| {
            self.op1(Op::ItemSelectable, items[i]) != 0
        }) {
            Some(i) => items[i],
            None => 0,
        }
    }

    pub fn insert_item(&self, item: isize, position: i32) -> isize {
        let position = self.find_insert_pos(item, position);
        let viewport = self.op1(Op::GetViewport, 0);
        self.op(Op::ItemSetViewport, item, viewport, 0, 0);
        self.op1(Op::ItemCalcDimensions, item);
        if self.height(item) <= 0 {
            self.op(Op::ItemSetHeight, item, 1, 0, 0);
        }
        self.set_selected(item, false);
        let end = position as u32 as usize == self.count();
        self.op1(Op::ItemHold, item);
        {
            let mut s = self.st.borrow_mut();
            let at = (position.max(0) as usize).min(s.items.len());
            s.items.insert(at, item);
            if s.sel_itr >= position {
                s.sel_itr += 1;
            }
        }
        let fixed = self.flag(FIXED);
        if end {
            self.op(Op::ItemSetPos, item, self.get(F::TotalHeight) as isize, 0, 0);
            let th = self.get(F::TotalHeight).wrapping_add(self.height(item));
            self.set(F::TotalHeight, th);
            let w = self.width(item);
            if w > self.get(F::TotalWidth) {
                self.set(F::TotalWidth, w);
            }
            let on_bottom = self.op1(Op::OnBottom, 0) != 0;
            let top = self.st.borrow().top_line;
            let h = self.height(item);
            let vo = guilist::insert_scroll(
                true,
                fixed,
                on_bottom,
                top,
                position,
                self.get(F::TotalHeight),
                self.get(F::ScrollHeight),
                self.get(F::VertOffset),
                h,
            );
            self.set(F::VertOffset, vo);
            self.virt_adjust(false);
        } else {
            let on_bottom = self.op1(Op::OnBottom, 0) != 0;
            let top = self.st.borrow().top_line;
            let h = self.height(item);
            let vo = guilist::insert_scroll(
                false,
                fixed,
                on_bottom,
                top,
                position,
                self.get(F::TotalHeight),
                self.get(F::ScrollHeight),
                self.get(F::VertOffset),
                h,
            );
            self.set(F::VertOffset, vo);
            if self.st.borrow().multi_change != 0 {
                let prev = if position != 0 { Some(self.top(self.item_at(position as usize - 1))) } else { None };
                let next = self.top(self.item_at(position as usize + 1));
                self.op(Op::ItemSetTop, item, guilist::batch_top(prev, next) as isize, 0, 0);
            } else {
                self.enforce_invariants();
            }
        }
        item
    }

    fn less(&self, a: isize, b: isize) -> bool {
        let kind = self.st.borrow().sort_type;
        self.op(Op::Less, a, b, kind as isize, 0) != 0
    }

    pub fn refresh_item(&self, item: isize) {
        let old = self.height(item);
        let viewport = self.op1(Op::GetViewport, 0);
        self.op(Op::ItemSetViewport, item, viewport, 0, 0);
        self.op1(Op::ItemCalcDimensions, item);
        if self.height(item) <= 0 {
            self.op(Op::ItemSetHeight, item, 1, 0, 0);
        }
        if self.st.borrow().multi_change != 0 {
            return;
        }
        let position = self.get_item_pos(item);
        if position >= 0 && self.st.borrow().sort_type != SORT_NONE {
            let prev = self.get_item(position - 1);
            let next = self.get_item(position + 1);
            // the original's `if (prev && less(item, prev)) ... else if (next && less(next, item))`
            let sort = (prev != 0 && self.less(item, prev)) || (next != 0 && self.less(next, item));
            if sort {
                let p = position as usize;
                self.st.borrow_mut().items[p] = if prev != 0 { prev } else { next };
                let found = self.find_insert_pos(item, 0);
                let n = self.count();
                let insert = guilist::refresh_place(position, found, n);
                let mut s = self.st.borrow_mut();
                if position > insert {
                    let mut i = p;
                    while i > insert as usize {
                        s.items[i] = s.items[i - 1];
                        i -= 1;
                    }
                } else if position < insert {
                    let mut i = p;
                    while i < insert as usize {
                        s.items[i] = s.items[i + 1];
                        i += 1;
                    }
                }
                s.items[insert as usize] = item;
            }
        }
        if position < 0 {
            return;
        }
        let on_bottom = self.op1(Op::OnBottom, 0) != 0;
        let t = self.top(item);
        let h = self.height(item);
        let vo = guilist::refresh_scroll(
            self.flag(FIXED),
            on_bottom,
            t,
            self.get(F::VertOffset),
            self.get(F::TotalHeight),
            self.get(F::ScrollHeight),
            h,
            old,
        );
        self.set(F::VertOffset, vo);
        self.enforce_invariants();
    }

    fn delete_item_prv(&self, position: usize) {
        let item = self.item_at(position);
        self.op(Op::ItemSetViewport, item, 0, 0, 0);
        let (t, h) = (self.top(item), self.height(item));
        let vo = guilist::delete_scroll(self.flag(FIXED), t, self.get(F::VertOffset), h);
        self.set(F::VertOffset, vo);
        if item == self.st.borrow().sel {
            self.set_selected(item, false);
            self.set_sel_item_at(-1, true);
        }
        if item == self.st.borrow().key {
            self.st.borrow_mut().key = 0;
        }
        if item == self.st.borrow().mouse {
            self.virt_mouse_gone();
        }
    }

    pub fn remove_item(&self, item: isize) {
        let position = self.get_item_pos(item);
        if position >= 0 {
            self.delete_range(position, position.wrapping_add(1));
        }
    }

    pub fn delete_range(&self, start: i32, end: i32) {
        let Some((start, amount)) = guilist::delete_range(start, end, self.count()) else {
            return;
        };
        for i in 0..amount {
            self.delete_item_prv((start + i) as usize);
        }
        let gone: Vec<isize> = {
            let mut s = self.st.borrow_mut();
            if s.sel_itr >= start {
                s.sel_itr -= amount;
            }
            let a = (start as usize).min(s.items.len());
            let b = ((start + amount) as usize).min(s.items.len());
            s.items.drain(a..b).collect()
        };
        for item in gone {
            self.op1(Op::ItemRelease, item);
        }
        self.enforce_invariants();
    }

    fn mouse_contains(&self, item: isize, x: i32, y: i32, width_check: bool) -> bool {
        if self.op(Op::ItemContains, item, x as isize, y as isize, 0) == 0 {
            return false;
        }
        if width_check && x > self.width(item) {
            return false;
        }
        true
    }

    /// `ItemMouseAt(x, y)`: the point to list coordinates, the item under it updated.
    pub fn item_mouse_at(&self, x: &mut i32, y: &mut i32) {
        self.op(Op::ScrollTranslate, x as *mut i32 as isize, y as *mut i32 as isize, 0, 0);
        let check = self.flag(WIDTH_CHECK);
        let mouse = self.st.borrow().mouse;
        if mouse != 0 && !self.mouse_contains(mouse, *x, *y, check) {
            self.op1(Op::ItemMouseExit, mouse);
            self.op1(Op::SetCursorDefault, 0);
            self.virt_mouse_gone();
        }
        if self.st.borrow().mouse != 0 {
            return;
        }
        let found = self.find_item_at(*y);
        self.st.borrow_mut().mouse = found;
        if found == 0 {
            return;
        }
        if self.mouse_contains(found, *x, *y, check) {
            let ry = y.wrapping_sub(self.top(found));
            self.op(Op::ItemMouseEnter, found, *x as isize, ry as isize, 0);
        } else {
            self.st.borrow_mut().mouse = 0;
        }
    }

    fn mouse(&self) -> isize {
        self.st.borrow().mouse
    }

    pub fn mouse_drag(&self, mut x: i32, mut y: i32) {
        self.op(Op::BaseMouseDrag, x as isize, y as isize, 0, 0);
        if self.flag(SELECT_ON_HOVER) {
            self.item_mouse_at(&mut x, &mut y);
            self.check_mouse_sel(self.mouse(), x, y, false);
        } else {
            self.op(Op::ScrollTranslate, &mut x as *mut i32 as isize, &mut y as *mut i32 as isize, 0, 0);
        }
        let m = self.mouse();
        if m != 0 {
            let ry = y.wrapping_sub(self.top(m));
            self.op(Op::ItemMouseDrag, m, x as isize, ry as isize, 0);
        }
    }

    pub fn key_down(&self, key: i32) -> bool {
        if self.op1(Op::BaseKeyDown, key as isize) != 0 {
            return true;
        }
        let key_item = self.st.borrow().key;
        if key_item == 0 {
            if key == VK_DOWN || key == VK_UP || key == VK_NEXT || key == VK_PRIOR {
                let item = self.find_item_at(self.get(F::VertOffset));
                if item == 0 {
                    return false;
                }
                let pos = self.get_item_pos(item);
                let item = self.find_selectable_item(pos, key == VK_UP || key == VK_PRIOR, true);
                if item != 0 {
                    self.st.borrow_mut().key = item;
                    self.set_selected(item, true);
                    self.set_sel_item(item, false, true);
                }
                return true;
            }
            return false;
        }
        let y = self.top(key_item);
        if key == VK_RIGHT {
            self.set(F::HorzOffset, self.get(F::HorzOffset).wrapping_add(10));
        } else if key == VK_LEFT {
            self.set(F::HorzOffset, self.get(F::HorzOffset).wrapping_sub(10));
        }
        let h = self.height(key_item);
        let Some((target, up)) =
            guilist::key_target(key, y, h, self.get(F::ScrollHeight), self.get(F::TotalHeight), self.flag(WRAP))
        else {
            return false;
        };
        if target != -1 {
            let i = self.find_line_at(target);
            if i == self.count() {
                return false;
            }
            let item = self.item_at(i);
            let pos = self.get_item_pos(item);
            let item = self.find_selectable_item(pos, up, true);
            if item != 0 {
                let sel = self.st.borrow().sel;
                if sel != 0 && self.flag(MULTI) && self.op1(Op::RootKeyMod, 0) & 1 != 0 {
                    let a = self.get_item_pos(sel);
                    let b = self.get_item_pos(item);
                    self.select_range(a, b);
                } else {
                    self.set_selected(item, true);
                    self.set_sel_item(item, false, true);
                }
                self.st.borrow_mut().key = item;
                let (t, h) = (self.top(item), self.height(item));
                let vo = guilist::key_scroll(
                    t,
                    h,
                    self.get(F::VertOffset),
                    self.get(F::ScrollHeight),
                    self.get(F::ExtraHeight),
                );
                self.set(F::VertOffset, vo);
            }
        }
        self.virt_adjust(false);
        true
    }

    pub fn set_list_flags(&self, flags: i32, on: bool) {
        let over = self.flag(CLEAR_ON_EXIT);
        {
            let mut s = self.st.borrow_mut();
            if on {
                s.flags |= flags;
            } else {
                s.flags &= !flags;
            }
        }
        if over || !self.flag(CLEAR_ON_EXIT) {
            return;
        }
        if self.op1(Op::PointerOutside, 0) != 0 {
            self.clear_selection_prv(true);
        }
    }

    pub fn mouse_move(&self, mut x: i32, mut y: i32) {
        self.op(Op::BaseMouseMove, x as isize, y as isize, 0, 0);
        self.item_mouse_at(&mut x, &mut y);
        if self.flag(SELECT_ON_HOVER) {
            self.check_mouse_sel(self.mouse(), x, y, false);
        }
        let m = self.mouse();
        if m != 0 {
            let ry = y.wrapping_sub(self.top(m));
            self.op(Op::ItemMouseMove, m, x as isize, ry as isize, 0);
        }
        if !self.flag(ITEM_TIPS) {
            return;
        }
        let m = self.mouse();
        let mut show = false;
        if m != 0 {
            let (w, h, t) = (self.width(m), self.height(m), self.top(m));
            show = guilist::item_tip(
                w,
                h,
                t,
                self.get(F::ScrollWidth),
                self.get(F::VertOffset),
                self.get(F::ScrollHeight),
            );
        }
        if show {
            self.op1(Op::TipShow, m);
        } else {
            self.op1(Op::TipHide, 0);
        }
    }

    fn deliver(&self, item: isize, button: i32, up: bool, x: i32, y: i32) {
        let ry = y.wrapping_sub(self.top(item));
        if up {
            self.op(Op::ItemMouseUp, item, x as isize, ry as isize, button as isize);
        } else {
            self.op(Op::ItemMouseDown, item, x as isize, ry as isize, button as isize);
        }
        let ry = y.wrapping_sub(self.top(item));
        let click = EeWonItemClick { button, up, x, y: ry };
        self.op(Op::DeliverItemClicked, item, &click as *const EeWonItemClick as isize, 0, 0);
    }

    pub fn mouse_down(&self, mut x: i32, mut y: i32, button: i32) {
        self.op(Op::BaseMouseDown, x as isize, y as isize, button as isize, 0);
        self.item_mouse_at(&mut x, &mut y);
        if button != 1 {
            self.op1(Op::RequestFocus, 0);
            self.check_mouse_sel(self.mouse(), x, y, true);
        }
        let m = self.mouse();
        if m != 0 {
            self.deliver(m, button, false, x, y);
        }
    }

    pub fn mouse_up(&self, mut x: i32, mut y: i32, button: i32) {
        self.op(Op::BaseMouseUp, x as isize, y as isize, button as isize, 0);
        self.item_mouse_at(&mut x, &mut y);
        let m = self.mouse();
        if m != 0 {
            self.deliver(m, button, true, x, y);
        }
    }

    pub fn mouse_enter(&self, mut x: i32, mut y: i32) {
        self.op(Op::BaseMouseEnter, x as isize, y as isize, 0, 0);
        self.item_mouse_at(&mut x, &mut y);
    }

    pub fn mouse_exit(&self) {
        self.op1(Op::BaseMouseExit, 0);
        let m = self.mouse();
        if m != 0 {
            self.op1(Op::ItemMouseExit, m);
            self.virt_mouse_gone();
        }
        if self.flag(CLEAR_ON_EXIT) {
            self.clear_selection_prv(true);
        }
        if self.flag(ITEM_TIPS) {
            self.op1(Op::TipHide, 0);
        }
    }

    pub fn set_sort_type(&self, kind: i32) {
        if self.st.borrow().sort_type == kind {
            return;
        }
        self.st.borrow_mut().sort_type = kind;
        self.op1(Op::VirtSort, 0);
    }

    /// One inward call.
    pub fn call(&self, call: EeWonListCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonListCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::SizeChanged => self.size_changed(),
            C::AdjustPosition => self.adjust_position(a != 0),
            C::ScrollPaint => self.scroll_paint(a),
            C::MouseEnter => self.mouse_enter(ai, bi),
            C::MouseExit => self.mouse_exit(),
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseUp => self.mouse_up(ai, bi, ci),
            C::MouseMove => self.mouse_move(ai, bi),
            C::MouseDrag => self.mouse_drag(ai, bi),
            C::KeyDown => return self.key_down(ai) as isize,
            C::NotifyMouseItemGone => self.st.borrow_mut().mouse = 0,
            C::Sort => return self.sort_base() as isize,
            C::BeginUpdate => self.begin_update(),
            C::EndUpdate => self.end_update(),
            C::Clear => self.clear(a != 0),
            C::GetItem => return self.get_item(ai),
            C::InsertItem => return self.insert_item(a, bi),
            C::RefreshItem => self.refresh_item(a),
            C::DeleteItem => self.delete_range(ai, ai.wrapping_add(1)),
            C::RemoveItem => self.remove_item(a),
            C::DeleteRange => self.delete_range(ai, bi),
            C::SetListFlags => self.set_list_flags(ai, b != 0),
            C::ListFlags => return self.st.borrow().flags as isize,
            C::NumItems => return self.count() as isize,
            C::GetItemPos => return self.get_item_pos(a) as isize,
            C::SetSelItemAt => self.set_sel_item_at(ai, b != 0),
            C::SetSelItem => return self.set_sel_item(a, b != 0, c != 0) as isize,
            C::EnsureVisible => self.ensure_visible(a),
            C::ClearSelection => {
                self.clear_selection_prv(a != 0);
            }
            C::SelectRange => self.select_range(ai, bi),
            C::FindItemAt => return self.find_item_at(ai),
            C::IsItemVisible => return self.is_item_visible(a) as isize,
            C::EnforceInvariants => self.enforce_invariants(),
            C::RewindSelections => self.rewind_selections(),
            C::HasMoreSelections => return self.has_more_selections() as isize,
            C::GetNextSelection => return self.get_next_selection(),
            C::GetItemPosAtYPos => return self.find_line_at(ai) as isize,
            C::FindSelectableItem => return self.find_selectable_item(ai, b != 0, c != 0),
            C::SetSortType => self.set_sort_type(ai),
            C::SortType => return self.st.borrow().sort_type as isize,
            C::SetSortTypeOnly => self.st.borrow_mut().sort_type = ai,
            C::SelItem => return self.st.borrow().sel,
            C::MouseItem => return self.st.borrow().mouse,
            C::ClearMouseItem => self.st.borrow_mut().mouse = 0,
            C::PaintWidth => return self.st.borrow().paint_width as isize,
            C::SetPaintWidth => self.st.borrow_mut().paint_width = ai,
        }
        0
    }

    /// The items' references go back (the C++ destructor's vector).
    fn release_all(&self) {
        let gone = std::mem::take(&mut self.st.borrow_mut().items);
        for item in gone {
            self.op1(Op::ItemRelease, item);
        }
    }
}

impl Drop for EeWonList {
    fn drop(&mut self) {
        self.release_all();
    }
}

// ---- the C ABI

/// A new Rust ListArea for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_list_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonList {
    Box::into_raw(Box::new(EeWonList::new(component, host)))
}

/// # Safety
/// `list` came from `ee_won_list_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_list_free(list: *mut EeWonList) {
    if !list.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(list) });
    }
}

/// One of ListArea's methods (`EeWonListCall`).
///
/// # Safety
/// `list` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_list_call(
    list: *const EeWonList,
    call: EeWonListCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*list).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "listobj_tests.rs"]
mod tests;
