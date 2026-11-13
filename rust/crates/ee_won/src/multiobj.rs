//! WONGUI's multi-column list as whole Rust objects (R6 WON round 24): MultiListArea (the lobby's game, player,
//! server and adapter lists), its rows (MultiListItem: one cell per column) and its header (MultiListHeader: one
//! button per column, the sort and the column drags). The original's MultiListArea.cpp, MultiListItem.cpp,
//! MultiListHeader.cpp and the multi-column list's part of ListControlAdapters.cpp, with their state.
//!
//! Each stays a C++ class (the type the lobby and the configs hold, a row class the adapter list derives from)
//! whose virtuals and methods are one-line forwarders: `ee_won_multi_call` (the list), `ee_won_mrow_call` (a row,
//! given its list's Rust object), `ee_won_mhead_call` (the header). The list's ListArea base is the Rust list
//! (`listobj`): the multi-column list calls it directly, and its own virtuals (AdjustPosition, Sort) go back through
//! the C++ object. Every other call out is an op of the component op table (`guihost`), the list's for the list and
//! its rows (the cells are C++ items, read and called through the item ops), the header's for the header. The
//! header reaches its list's Rust object through `MultiOf`.
//!
//! Every method is the original's statements in order with the calls out at the same points (`guimulti` and
//! `guidraw` keep the decisions). The states are behind `RefCell`s never borrowed across a call out.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guictl::EeWonRect4;
use crate::guidraw::{self, EeWonDrawOp, EeWonMultiRowPaint, EE_WON_DRAW_CALL};
use crate::guihost::{EeWonGuiCursor as Cursor, EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guimulti::{self, EeWonHeaderButton};
use crate::listobj::{EeWonList, EeWonListCall as L};

/// `ListSortType_None`.
const SORT_NONE: i32 = 2;
/// `ComponentFlag_Clip`.
const CLIP: isize = 0x2000;
/// The list flag of full-row selection.
const FULL_ROW: isize = 0x10;

fn call_host(host: EeWonGuiHostFn, comp: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    match host {
        // SAFETY: the host was given with this object's component; no borrow of a state is live here
        Some(h) => unsafe { h(comp, op, a, b, c, d) },
        None => 0,
    }
}

// ---- the list

/// A column (`MultiListArea::ColumnInfo`).
#[derive(Clone, Copy, Debug)]
struct Column {
    /// its `ScrollAreaViewport` (held), or 0
    viewport: isize,
    x: i32,
    width: i32,
    /// its `ListArea::SortFunc`, or 0
    sort_func: isize,
    mode: u8,
    partner: i32,
    halign: u8,
}

impl Default for Column {
    fn default() -> Self {
        Column { viewport: 0, x: 0, width: 0, sort_func: 0, mode: 1, partner: -1, halign: 0 }
    }
}

#[derive(Debug)]
struct MultiState {
    cols: Vec<Column>,
    sort_col_func: isize,
    paint_ctx: i32,
    total: i32,
    sort_col: i32,
    sort_prev: i32,
    border: i32,
    dynamic: i32,
    auto_grow: bool,
    mouse_col_item: isize,
    mouse_col_num: i32,
    max_total: i32,
    header: isize,
}

/// The inward calls of the list (`MultiListArea`'s virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonMultiCall {
    /// `AdjustPosition(a != 0)`.
    AdjustPosition = 1,
    /// `Virtual08` (SizeChanged).
    SizeChanged,
    /// `ScrollPaint(*a)`.
    ScrollPaint,
    NotifyMouseItemGone,
    /// `SetNumColumns(a)`.
    SetNumColumns,
    /// `SetColumnWidth(a, b, c != 0)`.
    SetColumnWidth,
    /// `SetSortColumn(a, b)`.
    SetSortColumn,
    /// `SetSortColumnFunc(a, b)` (`b` a `ListArea::SortFunc`).
    SetSortColumnFunc,
    /// `SetColumnHAlign(a, b)`.
    SetColumnHAlign,
    /// `SetColumnSizeMode(a, b)`.
    SetColumnSizeMode,
    /// `UpdateItem(a)`: the row.
    UpdateItem,
    /// `SetCell(a, b, c)`: the row or 0.
    SetCell,
    /// `GetColumnAt(a)`.
    ColumnAt,
    /// `GetColumnWidth(a)`.
    ColumnWidth,
    /// `GetColumnPos(a)`.
    ColumnPos,
    /// `mColumnPaintContext`.
    PaintContext,
    /// `mSortColumn`.
    SortColumn,
    /// `mSortColumnPrev`.
    SortColumnPrev,
    /// `mDynamicSizeColumn = a`.
    SetDynamicSizeColumn,
    /// `mAutoGrowColumns = a != 0`.
    SetAutoGrowColumns,
    /// `mColumnInfoVector.size()`.
    NumColumns,
    /// `mTotalColumnWidth`.
    TotalColumnWidth,
    /// `mMouseColItem`.
    MouseColItem,
    /// `mListHeader`.
    Header,
}

/// The Rust MultiListArea.
pub struct EeWonMulti {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    list: *const EeWonList,
    st: RefCell<MultiState>,
}

impl EeWonMulti {
    /// The constructor's state (the C++ constructor then sets the columns with `SetNumColumns`). `list` is the
    /// list's own Rust ListArea (its base).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, list: *const EeWonList) -> Self {
        EeWonMulti {
            comp,
            host,
            list,
            st: RefCell::new(MultiState {
                cols: Vec::new(),
                sort_col_func: 0,
                paint_ctx: 0,
                total: 0,
                sort_col: 0,
                sort_prev: 0,
                border: 5,
                dynamic: -1,
                auto_grow: false,
                mouse_col_item: 0,
                mouse_col_num: 0,
                max_total: 0,
                header: 0,
            }),
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
    /// The ListArea base (the Rust list).
    fn base(&self, c: L, a: isize, b: isize, cc: isize) -> isize {
        if self.list.is_null() {
            return 0;
        }
        // SAFETY: the list's Rust object lives as long as the C++ list (the base's destructor frees it after this
        // one); a shared reference: the calls out re-enter through it
        unsafe { (*self.list).call(c, a, b, cc) }
    }
    fn ncols(&self) -> usize {
        self.st.borrow().cols.len()
    }
    fn col(&self, i: usize) -> Column {
        self.st.borrow().cols.get(i).copied().unwrap_or_default()
    }

    fn notify_header(&self) {
        let h = self.st.borrow().header;
        if h != 0 {
            self.op1(Op::HeaderNotify, h);
        }
    }

    pub fn set_num_columns(&self, n: i32) {
        let gone: Vec<Column> = {
            let mut s = self.st.borrow_mut();
            let n = n.max(0) as usize;
            if n < s.cols.len() {
                s.cols.split_off(n)
            } else {
                s.cols.resize(n, Column::default());
                Vec::new()
            }
        };
        for c in gone {
            if c.viewport != 0 {
                self.op1(Op::ChildRelease, c.viewport);
            }
        }
        self.st.borrow_mut().total = 0;
        let mut i = 0;
        while i < self.ncols() {
            if self.col(i).viewport == 0 {
                let v = self.op1(Op::NewColumnViewport, 0);
                self.st.borrow_mut().cols[i].viewport = v;
            }
            let mut s = self.st.borrow_mut();
            s.total = s.total.wrapping_add(s.cols[i].width);
            drop(s);
            i += 1;
        }
        self.notify_header();
        self.op1(Op::VirtAdjustPosition, 0);
    }

    pub fn set_column_width(&self, col: i32, width: i32, from_size: bool) {
        let (mut widths, modes, partners, mut total, max_total) = {
            let s = self.st.borrow();
            (
                s.cols.iter().map(|c| c.width).collect::<Vec<_>>(),
                s.cols.iter().map(|c| c.mode).collect::<Vec<_>>(),
                s.cols.iter().map(|c| c.partner).collect::<Vec<_>>(),
                s.total,
                s.max_total,
            )
        };
        let mut xs = vec![0i32; widths.len()];
        let cols = guimulti::Columns { widths: &mut widths, modes: &modes, partners: &partners, xs: &mut xs };
        if !guimulti::set_column_width(cols, &mut total, max_total, col, width, from_size) {
            return;
        }
        self.st.borrow_mut().total = total;
        for i in 0..widths.len() {
            let v = {
                let mut s = self.st.borrow_mut();
                s.cols[i].width = widths[i];
                s.cols[i].viewport
            };
            if v != 0 {
                self.op(Op::ChildSetPos, v, xs[i] as isize, 0, 0);
            }
            self.st.borrow_mut().cols[i].x = xs[i];
        }
        self.notify_header();
        self.op1(Op::VirtAdjustPosition, 0);
    }

    pub fn set_sort_column(&self, col: i32, kind: i32) {
        if col < 0 || col as usize >= self.ncols() {
            return;
        }
        {
            let mut s = self.st.borrow_mut();
            s.sort_prev = s.sort_col;
            s.sort_col = col;
        }
        self.base(L::SetSortTypeOnly, kind as isize, 0, 0);
        let header = {
            let mut s = self.st.borrow_mut();
            s.sort_col_func = s.cols[col as usize].sort_func;
            s.header
        };
        if header != 0 {
            self.op(Op::HeaderSetSortColumn, header, col as isize, kind as isize, 0);
        }
        self.op1(Op::VirtSort, 0);
    }

    pub fn set_sort_column_func(&self, col: i32, func: isize) {
        if col < 0 || col as usize >= self.ncols() {
            return;
        }
        let mut s = self.st.borrow_mut();
        s.cols[col as usize].sort_func = func;
        if s.sort_col == col {
            s.sort_col_func = func;
        }
    }

    pub fn adjust_position(&self, scrollbar: bool) {
        let total = self.st.borrow().total;
        self.op(Op::Set, F::TotalWidth as isize, total as isize, 0, 0);
        self.base(L::AdjustPosition, scrollbar as isize, 0, 0);
        self.notify_header();
    }

    pub fn size_changed(&self) {
        let old = self.get(F::ScrollWidth);
        self.base(L::SizeChanged, 0, 0, 0);
        let (dynamic, n, total) = {
            let s = self.st.borrow();
            (s.dynamic, s.cols.len(), s.total)
        };
        if let Some(d) = guimulti::dynamic_delta(dynamic, n, self.get(F::ScrollWidth), old, total) {
            let w = self.col(dynamic as usize).width.wrapping_add(d);
            self.set_column_width(dynamic, w, true);
        }
    }

    pub fn scroll_paint(&self, g: isize) {
        let (widths, border, total) = {
            let s = self.st.borrow();
            (s.cols.iter().map(|c| c.width).collect::<Vec<_>>(), s.border, s.total)
        };
        let ops = guidraw::multi_list(&widths, border, total, self.get(F::VertOffset), self.get(F::ScrollHeight));
        for o in &ops {
            if o.kind == EE_WON_DRAW_CALL {
                if o.a == 0 {
                    self.st.borrow_mut().paint_ctx = o.b;
                    self.base(L::SetPaintWidth, o.c as isize, 0, 0);
                } else {
                    self.base(L::ScrollPaint, g, 0, 0);
                }
            } else {
                self.op(Op::Draw, g, o as *const EeWonDrawOp as isize, 0, 0);
            }
        }
    }

    pub fn update_item(&self, row: isize) -> isize {
        self.base(L::RefreshItem, row, 0, 0);
        if self.st.borrow().auto_grow {
            let mut i = 0usize;
            while i < self.ncols() {
                let cell = self.op(Op::RowCell, row, i as isize, 0, 0);
                if cell != 0 {
                    let w = self.op1(Op::ItemWidth, cell) as i32;
                    let (border, cw) = {
                        let s = self.st.borrow();
                        (s.border, s.cols[i].width)
                    };
                    if let Some(w) = guimulti::grow_column(w, border, cw) {
                        self.set_column_width(i as i32, w, false);
                    }
                }
                i += 1;
            }
        }
        row
    }

    pub fn set_cell(&self, row: i32, col: i32, cell: isize) -> isize {
        if col >= self.ncols() as i32 {
            return 0;
        }
        let r = self.base(L::GetItem, row as isize, 0, 0);
        if r == 0 {
            return 0;
        }
        self.op(Op::RowSetCell, r, cell, col as isize, 0);
        self.update_item(r);
        r
    }

    pub fn column_at(&self, x: i32) -> i32 {
        let s = self.st.borrow();
        let xs: Vec<i32> = s.cols.iter().map(|c| c.x).collect();
        let ws: Vec<i32> = s.cols.iter().map(|c| c.width).collect();
        guimulti::column_at(&xs, &ws, x)
    }

    pub fn column_width(&self, col: i32) -> i32 {
        if col < 0 || col as usize >= self.ncols() {
            return 0;
        }
        self.col(col as usize).width
    }

    pub fn column_pos(&self, col: i32) -> i32 {
        if col < 0 || col as usize >= self.ncols() {
            return -1;
        }
        self.col(col as usize).x
    }

    pub fn set_column_halign(&self, col: i32, align: i32) {
        if col < 0 || col as usize >= self.ncols() {
            return;
        }
        self.st.borrow_mut().cols[col as usize].halign = align as u8;
    }

    pub fn set_column_size_mode(&self, col: i32, mode: u8) {
        if col < 0 || col as usize >= self.ncols() {
            return;
        }
        let mut s = self.st.borrow_mut();
        let old = s.cols[col as usize].mode;
        s.cols[col as usize].mode = mode;
        if !guimulti::relink(old, mode) {
            return;
        }
        let modes: Vec<u8> = s.cols.iter().map(|c| c.mode).collect();
        let mut partners: Vec<i32> = s.cols.iter().map(|c| c.partner).collect();
        guimulti::link_partners(&modes, &mut partners);
        for (c, p) in s.cols.iter_mut().zip(partners) {
            c.partner = p;
        }
    }

    pub fn notify_mouse_item_gone(&self) {
        self.base(L::NotifyMouseItemGone, 0, 0, 0);
        self.st.borrow_mut().mouse_col_item = 0;
    }

    /// The header's `SetMultiListArea`: `mListHeader = header`.
    pub fn set_header(&self, header: isize) {
        self.st.borrow_mut().header = header;
    }

    /// One inward call.
    pub fn call(&self, call: EeWonMultiCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonMultiCall;
        let (ai, bi) = (a as i32, b as i32);
        match call {
            C::AdjustPosition => self.adjust_position(a != 0),
            C::SizeChanged => self.size_changed(),
            C::ScrollPaint => self.scroll_paint(a),
            C::NotifyMouseItemGone => self.notify_mouse_item_gone(),
            C::SetNumColumns => self.set_num_columns(ai),
            C::SetColumnWidth => self.set_column_width(ai, bi, c != 0),
            C::SetSortColumn => self.set_sort_column(ai, bi),
            C::SetSortColumnFunc => self.set_sort_column_func(ai, b),
            C::SetColumnHAlign => self.set_column_halign(ai, bi),
            C::SetColumnSizeMode => self.set_column_size_mode(ai, b as u8),
            C::UpdateItem => return self.update_item(a),
            C::SetCell => return self.set_cell(ai, bi, c),
            C::ColumnAt => return self.column_at(ai) as isize,
            C::ColumnWidth => return self.column_width(ai) as isize,
            C::ColumnPos => return self.column_pos(ai) as isize,
            C::PaintContext => return self.st.borrow().paint_ctx as isize,
            C::SortColumn => return self.st.borrow().sort_col as isize,
            C::SortColumnPrev => return self.st.borrow().sort_prev as isize,
            C::SetDynamicSizeColumn => self.st.borrow_mut().dynamic = ai,
            C::SetAutoGrowColumns => self.st.borrow_mut().auto_grow = a != 0,
            C::NumColumns => return self.ncols() as isize,
            C::TotalColumnWidth => return self.st.borrow().total as isize,
            C::MouseColItem => return self.st.borrow().mouse_col_item,
            C::Header => return self.st.borrow().header,
        }
        0
    }
}

impl Drop for EeWonMulti {
    fn drop(&mut self) {
        let gone = std::mem::take(&mut self.st.borrow_mut().cols);
        for c in gone {
            if c.viewport != 0 {
                self.op1(Op::ChildRelease, c.viewport);
            }
        }
    }
}

// ---- a row

/// The inward calls of a row (`MultiListItem`'s virtuals and methods; the list's calls need its Rust list).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonMRowCall {
    /// `CalcDimensions(list)`.
    CalcDimensions = 1,
    /// `Compare(a, list)`.
    Compare,
    /// `SetPos(a, b)` (no list needed).
    SetPos,
    /// `Paint(*a, list)`.
    Paint,
    /// `SetViewport(list, a)`.
    SetViewport,
    /// `CanSelect(list, a, b)` (0 or 1).
    CanSelect,
    /// `MouseEnter(list, a, b)`.
    MouseEnter,
    /// `MouseExit(list)`.
    MouseExit,
    /// `MouseMove(list, a, b)`.
    MouseMove,
    /// `MouseDrag(list, a, b)`.
    MouseDrag,
    /// `MouseDown(list, a, b, c)`.
    MouseDown,
    /// `MouseUp(list, a, b, c)`.
    MouseUp,
    /// `SetItem(a, b)` (no list needed).
    SetItem,
    /// `GetItem(a)` (no list needed).
    GetItem,
    /// `InsertItem(a)` (no list needed).
    InsertItem,
}

/// The Rust MultiListItem: its cells (a reference held on each).
pub struct EeWonMRow {
    /// the C++ row (the `a` of the item ops on the row itself)
    me: isize,
    /// the row's own host: the item ops of `SetPos` and the cells' references (`ItemHold` / `ItemRelease`)
    host: EeWonGuiHostFn,
    cells: RefCell<Vec<isize>>,
}

impl EeWonMRow {
    pub fn new(me: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonMRow { me: me as isize, host, cells: RefCell::new(Vec::new()) }
    }

    fn own(&self, op: Op, a: isize, b: isize) {
        call_host(self.host, self.me as *mut c_void, op, a, b, 0, 0);
    }
    fn cells(&self) -> Vec<isize> {
        self.cells.borrow().clone()
    }

    pub fn get_item(&self, col: i32) -> isize {
        let c = self.cells.borrow();
        if col < 0 || col as usize >= c.len() {
            return 0;
        }
        c[col as usize]
    }

    pub fn set_item(&self, cell: isize, col: i32) {
        // the original compared the size with the int as unsigned: a negative column is undefined there
        if col < 0 {
            return;
        }
        let old = {
            let mut c = self.cells.borrow_mut();
            if c.len() <= col as usize {
                c.resize(col as usize + 1, 0);
            }
            c[col as usize]
        };
        if old != cell {
            if old != 0 {
                self.own(Op::ItemRelease, old, 0);
            }
            self.cells.borrow_mut()[col as usize] = cell;
            if cell != 0 {
                self.own(Op::ItemHold, cell, 0);
            }
        }
    }

    pub fn insert_item(&self, cell: isize) {
        self.cells.borrow_mut().push(cell);
        if cell != 0 {
            self.own(Op::ItemHold, cell, 0);
        }
    }

    pub fn calc_dimensions(&self, m: &EeWonMulti) {
        m.op(Op::ItemSetHeight, self.me, 0, 0, 0);
        for cell in self.cells() {
            if cell == 0 {
                continue;
            }
            m.op1(Op::ItemCalcDimensions, cell);
            let h = m.op1(Op::ItemHeight, cell);
            if h > m.op1(Op::ItemHeight, self.me) {
                m.op(Op::ItemSetHeight, self.me, h, 0, 0);
            }
        }
        let total = m.st.borrow().total;
        m.op(Op::ItemSetWidth, self.me, total as isize, 0, 0);
    }

    pub fn compare(&self, m: &EeWonMulti, other: isize) -> i32 {
        let col = m.st.borrow().sort_col;
        let a = self.get_item(col);
        let b = m.op(Op::RowCell, other, col as isize, 0, 0);
        if a == b {
            return 0;
        }
        if a == 0 {
            return -1;
        }
        if b == 0 {
            return 1;
        }
        let f = m.st.borrow().sort_col_func;
        if f != 0 {
            return m.op(Op::CallSortFunc, f, a, b, 0) as i32;
        }
        m.op(Op::ItemCompare, a, b, 0, 0) as i32
    }

    /// `SetPos(x, y)` (the row's own host: the original's SetPos gets no list).
    pub fn set_pos(&self, y: i32) {
        self.own(Op::ItemSetTop, self.me, y as isize);
        for cell in self.cells() {
            if cell != 0 {
                self.own(Op::ItemSetPos, cell, y as isize);
            }
        }
    }

    /// `GetMouseItem(list, x)`: the cell in the column under `x`, `x` made relative to it.
    fn mouse_cell(&self, m: &EeWonMulti, x: &mut i32) -> isize {
        let col = m.column_at(*x);
        *x = x.wrapping_sub(m.column_pos(col));
        self.get_item(col)
    }

    /// `GetMouseItem(list, x, y)`: the hovered cell updated and returned.
    fn mouse_item(&self, m: &EeWonMulti, x: &mut i32, y: &mut i32) -> isize {
        let col = m.column_at(*x);
        *x = x.wrapping_sub(m.column_pos(col));
        let item = self.get_item(col);
        let old = m.st.borrow().mouse_col_item;
        if old != 0 {
            if old != item || {
                let yy = y.wrapping_add(m.op1(Op::ItemTop, self.me) as i32);
                m.op(Op::ItemContains, old, *x as isize, yy as isize, 0) == 0
            } {
                m.op1(Op::ItemMouseExit, old);
                m.op1(Op::SetCursor, Cursor::None as isize);
                m.st.borrow_mut().mouse_col_item = 0;
            }
        } else if item != 0 && {
            let yy = y.wrapping_add(m.op1(Op::ItemTop, self.me) as i32);
            m.op(Op::ItemContains, item, *x as isize, yy as isize, 0) != 0
        } {
            {
                let mut s = m.st.borrow_mut();
                s.mouse_col_item = item;
                s.mouse_col_num = col;
            }
            m.op(Op::ItemMouseEnter, item, *x as isize, *y as isize, 0);
        }
        m.st.borrow().mouse_col_item
    }

    pub fn paint(&self, m: &EeWonMulti, g: isize) {
        let col = m.st.borrow().paint_ctx;
        let cells = self.cells();
        let item = if (col as u32 as usize) < cells.len() { cells[col as usize] } else { 0 };
        let selected = m.op1(Op::ItemSelected, self.me) != 0;
        let full_row_select = m.base(L::ListFlags, 0, 0, 0) & FULL_ROW != 0;
        let selection_color = m.op1(Op::SelBackground, g) as u32;
        let total = m.st.borrow().total;
        let height = m.op1(Op::ItemHeight, self.me) as i32;
        // the original read the column of a cell past the columns out of its vector: none here
        let align = if item != 0 { m.col(col as usize).halign } else { 0 };
        let paint_width = m.base(L::PaintWidth, 0, 0, 0) as i32;
        let cell_width = if item != 0 { m.op1(Op::ItemWidth, item) as i32 } else { 0 };
        let r = EeWonMultiRowPaint {
            column: col,
            selected,
            full_row_select,
            has_cell: item != 0,
            cell_count: cells.len() as u32,
            selection_color,
            total_width: total,
            height,
            align,
            paint_width,
            cell_width,
        };
        for o in &guidraw::multi_row(&r) {
            if o.kind == EE_WON_DRAW_CALL {
                if o.a == 0 {
                    m.op(Op::ItemSetHeight, item, o.b as isize, 0, 0);
                    m.op(Op::ItemSetSelected, item, (o.c != 0) as isize, 0, 0);
                } else {
                    m.op(Op::ItemPaint, item, g, 0, 0);
                }
            } else {
                m.op(Op::Draw, g, o as *const EeWonDrawOp as isize, 0, 0);
            }
        }
    }

    pub fn set_viewport(&self, m: &EeWonMulti, viewport: isize) {
        for (i, cell) in self.cells().into_iter().enumerate() {
            let col = {
                let s = m.st.borrow();
                (i < s.cols.len()).then(|| s.cols[i].viewport)
            };
            if let (true, Some(v)) = (cell != 0, col) {
                let v = if viewport == 0 { 0 } else { v };
                m.op(Op::ItemSetViewport, cell, v, 0, 0);
            }
        }
    }

    pub fn can_select(&self, m: &EeWonMulti, mut x: i32, y: i32) -> bool {
        if m.op1(Op::ItemSelectable, self.me) == 0 {
            return false;
        }
        let item = self.mouse_cell(m, &mut x);
        if item != 0 {
            return m.op(Op::ItemCanSelect, item, x as isize, y as isize, 0) != 0;
        }
        true
    }

    pub fn mouse_exit(&self, m: &EeWonMulti) {
        let it = m.st.borrow().mouse_col_item;
        if it != 0 {
            m.op1(Op::ItemMouseExit, it);
            m.base(L::ClearMouseItem, 0, 0, 0);
        }
    }

    pub fn mouse_drag(&self, m: &EeWonMulti, x: i32, y: i32) {
        let (it, num) = {
            let s = m.st.borrow();
            (s.mouse_col_item, s.mouse_col_num)
        };
        if it != 0 {
            let x = x.wrapping_sub(m.column_pos(num));
            m.op(Op::ItemMouseDrag, it, x as isize, y as isize, 0);
        }
    }

    /// One inward call; `m` is the row's list (NULL for the calls that need none).
    pub fn call(&self, m: Option<&EeWonMulti>, call: EeWonMRowCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonMRowCall;
        let (mut ai, mut bi) = (a as i32, b as i32);
        match call {
            C::SetItem => {
                self.set_item(a, bi);
                return 0;
            }
            C::GetItem => return self.get_item(ai),
            C::InsertItem => {
                self.insert_item(a);
                return 0;
            }
            C::SetPos => {
                self.set_pos(bi);
                return 0;
            }
            _ => {}
        }
        let Some(m) = m else { return 0 };
        match call {
            C::CalcDimensions => self.calc_dimensions(m),
            C::Compare => return self.compare(m, a) as isize,
            C::Paint => self.paint(m, a),
            C::SetViewport => self.set_viewport(m, a),
            C::CanSelect => return self.can_select(m, ai, bi) as isize,
            C::MouseEnter => {
                self.mouse_item(m, &mut ai, &mut bi);
            }
            C::MouseExit => self.mouse_exit(m),
            C::MouseMove => {
                let it = self.mouse_item(m, &mut ai, &mut bi);
                if it != 0 {
                    m.op(Op::ItemMouseMove, it, ai as isize, bi as isize, 0);
                }
            }
            C::MouseDrag => self.mouse_drag(m, ai, bi),
            C::MouseDown | C::MouseUp => {
                let it = self.mouse_item(m, &mut ai, &mut bi);
                if it != 0 {
                    let op = if call == C::MouseDown { Op::ItemMouseDown } else { Op::ItemMouseUp };
                    m.op(op, it, ai as isize, bi as isize, c);
                }
            }
            C::SetItem | C::GetItem | C::InsertItem | C::SetPos => {}
        }
        0
    }
}

impl Drop for EeWonMRow {
    fn drop(&mut self) {
        let gone = std::mem::take(&mut *self.cells.borrow_mut());
        for cell in gone {
            if cell != 0 {
                self.own(Op::ItemRelease, cell, 0);
            }
        }
    }
}

// ---- the header

#[derive(Debug)]
struct HeaderState {
    /// each column's button (borrowed: the container holds it) and sort type
    cols: Vec<(isize, i32)>,
    /// the C++ `MultiListArea*`, or 0
    list: isize,
    drag_col: i32,
    drag_comp: isize,
    drag_x: i32,
    cur_sort_col: i32,
    draw_sort_arrow: bool,
}

/// The inward calls of the header (`MultiListHeader`'s virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonMHeadCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    MouseExit,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseUp(a, b, c)`.
    MouseUp,
    /// `MouseMove(a, b)`.
    MouseMove,
    /// `MouseDrag(a, b)`.
    MouseDrag,
    /// `HandleComponentEvent(a)`.
    HandleEvent,
    /// `SetColumnItem(a, b)`.
    SetColumnItem,
    /// `SetMultiListArea(a)`.
    SetMultiListArea,
    NotifyColumnChange,
    /// `SetSortColumn(a, b)`.
    SetSortColumn,
    /// `GetColumnIndex(a)`.
    ColumnIndex,
    /// `GetSortType(a)`.
    SortType,
    /// `mDrawSortArrow` (0 or 1).
    DrawSortArrow,
    /// `mListArea`.
    ListArea,
    /// `mDragColumn`.
    DragColumn,
}

/// The Rust MultiListHeader.
pub struct EeWonMHead {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<HeaderState>,
}

impl EeWonMHead {
    /// The constructor's state (no drag, no sort column, sort arrows on); the C++ constructor then attaches the
    /// list (`SetMultiListArea`).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonMHead {
            comp,
            host,
            st: RefCell::new(HeaderState {
                cols: Vec::new(),
                list: 0,
                drag_col: -1,
                drag_comp: 0,
                drag_x: 0,
                cur_sort_col: -1,
                draw_sort_arrow: true,
            }),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        call_host(self.host, self.comp, op, a, b, c, 0)
    }
    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0)
    }
    fn get(&self, f: F) -> i32 {
        self.op(Op::Get, f as isize, 0, 0) as i32
    }
    /// The list's Rust object (the list is set).
    fn multi(&self, list: isize) -> &EeWonMulti {
        let m = self.op1(Op::MultiOf, list) as *const EeWonMulti;
        // SAFETY: the header's list is live while it is attached; a shared reference (the calls out re-enter)
        unsafe { &*m }
    }

    pub fn set_multi_list_area(&self, list: isize) {
        self.st.borrow_mut().list = list;
        if list != 0 {
            self.multi(list).set_header(self.comp as isize);
        }
    }

    pub fn set_sort_column(&self, col: i32, kind: i32) {
        let mut s = self.st.borrow_mut();
        if col < 0 || col as usize >= s.cols.len() {
            return;
        }
        s.cols[col as usize].1 = kind;
        s.cur_sort_col = col;
    }

    pub fn set_column_item(&self, col: i32, item: isize) {
        // the original compared the size with the int as unsigned: a negative column is undefined there
        if col < 0 {
            return;
        }
        let c = col as usize;
        let old = {
            let mut s = self.st.borrow_mut();
            s.drag_col = -1;
            if s.cols.len() <= c {
                s.cols.resize(c + 1, (0, SORT_NONE));
            }
            s.cols[c].0
        };
        if old != 0 {
            self.op1(Op::RemoveChild, old);
        }
        self.op(Op::ChildSetControlId, item, col as isize, 0);
        self.op(Op::ChildSetFlags, item, CLIP, 1);
        self.st.borrow_mut().cols[c].0 = item;
        self.op1(Op::AddChild, item);
        let list = self.st.borrow().list;
        if list != 0 {
            let m = self.multi(list);
            if c < m.ncols() && m.column_width(col) <= 0 {
                let mut wh = [0i32; 2];
                self.op(Op::ChildDesiredSize, item, wh.as_mut_ptr() as isize, 0);
                m.set_column_width(col, wh[0], false);
                if m.st.borrow().sort_col == col {
                    let kind = m.base(L::SortType, 0, 0, 0) as i32;
                    let mut s = self.st.borrow_mut();
                    s.cur_sort_col = col;
                    s.cols[c].1 = kind;
                }
                return;
            }
        }
        self.notify_column_change();
    }

    pub fn notify_column_change(&self) {
        let list = self.st.borrow().list;
        if list == 0 {
            return;
        }
        let m = self.multi(list);
        let comps: Vec<isize> = self.st.borrow().cols.iter().map(|c| c.0).collect();
        let has: Vec<bool> = comps.iter().map(|&c| c != 0).collect();
        let widths: Vec<i32> = m.st.borrow().cols.iter().map(|c| c.width).collect();
        let places = guimulti::header_layout(&has, &widths, m.get(F::HorzOffset));
        for (i, &c) in comps.iter().enumerate() {
            if c != 0 {
                let p = places[i];
                self.op(Op::ChildSetEnabled, c, p.enabled as isize, 0);
                self.op(Op::ChildSetVisible, c, (p.width != 0) as isize, 0);
                let r = EeWonRect4 { x: p.x, y: 0, w: p.width, h: self.get(F::Height) };
                self.op(Op::ChildSetPosSize, c, &r as *const EeWonRect4 as isize, 0);
            }
        }
        if let Some(&c) = comps.last() {
            // the original assumed the last column has a button
            if c != 0 {
                self.op(Op::ChildSetVisible, c, 1, 0);
                let left = self.op1(Op::ChildLeft, c) as i32;
                let r = EeWonRect4 { x: left, y: 0, w: self.get(F::Width), h: self.get(F::Height) };
                self.op(Op::ChildSetPosSize, c, &r as *const EeWonRect4 as isize, 0);
            }
        }
        self.op1(Op::Invalidate, 0);
    }

    pub fn size_changed(&self) {
        self.op1(Op::BaseSizeChanged, 0);
        self.notify_column_change();
    }

    pub fn column_index(&self, comp: isize) -> i32 {
        self.st.borrow().cols.iter().position(|c| c.0 == comp).map_or(-1, |i| i as i32)
    }

    pub fn sort_type(&self, col: i32) -> i32 {
        let s = self.st.borrow();
        let mut kind = SORT_NONE;
        if col >= 0 && (col as usize) < s.cols.len() {
            kind = s.cols[col as usize].1;
        }
        guimulti::header_sort_type(col, s.cols.len(), s.cur_sort_col, kind)
    }

    pub fn handle_event(&self, event: isize) {
        let list = self.st.borrow().list;
        if self.op1(Op::EventIsButtonPress, event) != 0 && list != 0 {
            let child = self.op1(Op::EventChildUnder, event);
            if child == 0 {
                return;
            }
            let col = self.op1(Op::ChildControlId, child) as i32;
            let m = self.multi(list);
            let n = self.st.borrow().cols.len();
            if col >= 0 && (col as usize) < m.ncols() && (col as usize) < n {
                let (kind, cur) = {
                    let s = self.st.borrow();
                    (s.cols[col as usize].1, s.cur_sort_col)
                };
                m.set_sort_column(col, guimulti::header_toggle(kind, col == cur));
                return;
            }
        }
        self.op1(Op::BaseHandleEvent, event);
    }

    fn drag_component(&self, x: i32) -> isize {
        let (list, comps) = {
            let mut s = self.st.borrow_mut();
            s.drag_col = -1;
            s.drag_comp = 0;
            (s.list, s.cols.iter().map(|c| c.0).collect::<Vec<_>>())
        };
        // the original read the list without a test: a header without one is undefined there
        if list == 0 {
            return 0;
        }
        let m = self.multi(list);
        let mut buttons = vec![EeWonHeaderButton::default(); comps.len()];
        for (i, &c) in comps.iter().enumerate() {
            buttons[i].present = c != 0;
            if c != 0 {
                buttons[i].left = self.op1(Op::ChildLeft, c) as i32;
                buttons[i].right = self.op1(Op::ChildRight, c) as i32;
                buttons[i].width = self.op1(Op::ChildWidth, c) as i32;
            }
        }
        let (widths, modes, partners) = {
            let s = m.st.borrow();
            (
                s.cols.iter().map(|c| c.width).collect::<Vec<_>>(),
                s.cols.iter().map(|c| c.mode).collect::<Vec<_>>(),
                s.cols.iter().map(|c| c.partner).collect::<Vec<_>>(),
            )
        };
        let Some((col, drag_x)) = guimulti::header_drag(x, &buttons, &widths, &modes, &partners) else {
            return 0;
        };
        let mut s = self.st.borrow_mut();
        s.drag_col = col;
        s.drag_x = drag_x;
        s.drag_comp = s.cols[col as usize].0;
        s.drag_comp
    }

    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        if button == 0 && self.drag_component(x) != 0 {
            return;
        }
        self.op(Op::BaseMouseDown, x as isize, y as isize, button as isize);
    }

    pub fn mouse_move(&self, x: i32, y: i32) {
        self.op(Op::BaseMouseMove, x as isize, y as isize, 0);
        if self.drag_component(x) != 0 {
            self.op1(Op::SetCursor, Cursor::EastWest as isize);
        } else if self.op1(Op::CursorIs, Cursor::EastWest as isize) != 0 {
            self.op1(Op::SetCursor, Cursor::None as isize);
        }
    }

    pub fn mouse_drag(&self, x: i32, y: i32) {
        let (col, drag_x, list) = {
            let s = self.st.borrow();
            (s.drag_col, s.drag_x, s.list)
        };
        if col >= 0 {
            self.multi(list).set_column_width(col, guimulti::header_drag_width(x, drag_x), false);
            self.op1(Op::SetCursor, Cursor::EastWest as isize);
        } else {
            self.op(Op::BaseMouseDrag, x as isize, y as isize, 0);
        }
    }

    pub fn mouse_up(&self, x: i32, y: i32, button: i32) {
        if button == 0 && self.st.borrow().drag_col != -1 {
            self.st.borrow_mut().drag_col = -1;
        } else {
            self.op(Op::BaseMouseUp, x as isize, y as isize, button as isize);
        }
    }

    pub fn mouse_exit(&self) {
        self.st.borrow_mut().drag_col = -1;
        self.op1(Op::BaseMouseExit, 0);
    }

    /// One inward call.
    pub fn call(&self, call: EeWonMHeadCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonMHeadCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::SizeChanged => self.size_changed(),
            C::MouseExit => self.mouse_exit(),
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseUp => self.mouse_up(ai, bi, ci),
            C::MouseMove => self.mouse_move(ai, bi),
            C::MouseDrag => self.mouse_drag(ai, bi),
            C::HandleEvent => self.handle_event(a),
            C::SetColumnItem => self.set_column_item(ai, b),
            C::SetMultiListArea => self.set_multi_list_area(a),
            C::NotifyColumnChange => self.notify_column_change(),
            C::SetSortColumn => self.set_sort_column(ai, bi),
            C::ColumnIndex => return self.column_index(a) as isize,
            C::SortType => return self.sort_type(ai) as isize,
            C::DrawSortArrow => return self.st.borrow().draw_sort_arrow as isize,
            C::ListArea => return self.st.borrow().list,
            C::DragColumn => return self.st.borrow().drag_col as isize,
        }
        0
    }
}

// ---- the C ABI

/// A new Rust MultiListArea for the C++ `component`, its calls out through `host`, its base the Rust list `list`.
#[no_mangle]
pub extern "C" fn ee_won_multi_new(
    component: *mut c_void,
    host: EeWonGuiHostFn,
    list: *const EeWonList,
) -> *mut EeWonMulti {
    Box::into_raw(Box::new(EeWonMulti::new(component, host, list)))
}

/// # Safety
/// `multi` came from `ee_won_multi_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_multi_free(multi: *mut EeWonMulti) {
    if !multi.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(multi) });
    }
}

/// One of MultiListArea's methods (`EeWonMultiCall`).
///
/// # Safety
/// `multi` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_multi_call(
    multi: *const EeWonMulti,
    call: EeWonMultiCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*multi).call(call, a, b, c) }
}

/// A new Rust MultiListItem for the C++ row `component`; `host` gives the cells' references.
#[no_mangle]
pub extern "C" fn ee_won_mrow_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonMRow {
    Box::into_raw(Box::new(EeWonMRow::new(component, host)))
}

/// # Safety
/// `row` came from `ee_won_mrow_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_mrow_free(row: *mut EeWonMRow) {
    if !row.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(row) });
    }
}

/// One of MultiListItem's methods (`EeWonMRowCall`); `multi` is the row's list's Rust object (NULL for the calls
/// that need no list).
///
/// # Safety
/// `row` is live; `multi` is live or NULL; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_mrow_call(
    row: *const EeWonMRow,
    multi: *const EeWonMulti,
    call: EeWonMRowCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract)
    unsafe { (*row).call(multi.as_ref(), call, a, b, c) }
}

/// A new Rust MultiListHeader for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_mhead_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonMHead {
    Box::into_raw(Box::new(EeWonMHead::new(component, host)))
}

/// # Safety
/// `head` came from `ee_won_mhead_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_mhead_free(head: *mut EeWonMHead) {
    if !head.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(head) });
    }
}

/// One of MultiListHeader's methods (`EeWonMHeadCall`).
///
/// # Safety
/// `head` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_mhead_call(
    head: *const EeWonMHead,
    call: EeWonMHeadCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract)
    unsafe { (*head).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "multiobj_tests.rs"]
mod tests;
