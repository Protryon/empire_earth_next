//! `UIListBox<T>` as a whole Rust object (R6 ui round 15): the template's state (the text rectangles, the colours,
//! the selection record, the scroll bar, the key-repeat polls and the two selection rhombi), every method, and from
//! UI round 15 the lines: `mData` and its drawn copy `mCopyLineArray` are boxed `T`s this object owns, made, copied
//! and freed through the line type's C++ members ([`EeUiLineOps`], `UILineOps<T>` in lle/UIListBox.h); the C++
//! template's members are one call each and `mData`/`mCopyLineArray` are views (`UILines<T>`). The per-`T` host
//! (`UIListBoxRust<T>::Op`) answers only the virtuals (`GetNumLines`, `ChangeSelectionIndex`, `ClearSelection`,
//! `ProcessDown`/`Up`/`Home`/`End`, `ButtonDown`/`ButtonUp`, `AppendLine`, `DeleteAllEntries`: a subclass's
//! override runs); every other op goes to the shared host (`ee_ui_list_host`). The string list's (`UILBString`)
//! draw and type-to-find are here too.
//!
//! The selection and scrolling algorithms are round 4's `list.rs` with their goldens; this object is their host
//! ([`ListHost`]).

use core::cell::{Cell, RefCell};
use core::ffi::{c_char, c_long, c_ulong, c_void, CStr};

use crate::draw;
use crate::list::{List, ListFields, ListHost, ListState};
use crate::obj::{read_buf, write_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::{um_floor, UiPoint, UiRect};
use crate::text::{less_no_case, Platform};

/// `kControlTriggerEnterPressed`.
const TRIGGER_ENTER_PRESSED: isize = 0x3f1;
/// `kErrorFailed`.
const ERROR_FAILED: isize = 9;

/// The inward calls (the C++ template's forwarders).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiListCall {
    /// `CopyData()` (after the per-`T` copy of the lines, op `ListCopyLines`).
    CopyData = 1,
    /// `DrawPolygons()`.
    DrawPolygons = 2,
    /// `DrawTextA()`.
    DrawText = 3,
    /// `DrawScrollBarText()`.
    DrawScrollBarText = 4,
    /// `ChangeSelectionIndex(a, b & 1 fire, b & 2 scroll to)` (the base class's body).
    ChangeSelectionIndex = 5,
    /// `ChangeWidth(a)`.
    ChangeWidth = 6,
    /// `ClearSelection()` (the base class's body).
    ClearSelection = 7,
    /// `DeleteAllEntries()` after `mData.clear()`.
    Cleared = 8,
    /// `DeleteLine(a)` after the erase.
    DeletedLine = 9,
    /// `InsertLine` after the line went in at `a`.
    Inserted = 10,
    /// `CheckScrollBar()`.
    CheckScrollBar = 11,
    /// `SetLocation(f[0], f[1])` (the base class's body).
    SetLocation = 12,
    /// `SetScrollBarBgOpacity(f[0])`.
    SetScrollBarBgOpacity = 13,
    /// `SetScrollBarBgColor(f[0..3])`.
    SetScrollBarBgColor = 14,
    /// `UpdateKeyboard()`.
    UpdateKeyboard = 15,
    /// `ButtonDown()` (the base class's body).
    ButtonDown = 16,
    /// `MouseLButtonDown()`.
    MouseLButtonDown = 17,
    /// `MouseLButtonUp()`.
    MouseLButtonUp = 18,
    /// `MouseLButtonDoublePressed()`.
    MouseDoublePressed = 19,
    /// `MouseMove(f[0], f[1])`.
    MouseMove = 20,
    /// `ProcessDown()` (the base class's body).
    ProcessDown = 21,
    /// `ProcessEnd()`.
    ProcessEnd = 22,
    /// `ProcessHome()`.
    ProcessHome = 23,
    /// `ProcessUp()`.
    ProcessUp = 24,
    /// `TabbedFrom()`.
    TabbedFrom = 25,
    /// `MouseWheelTurned(a)`.
    MouseWheelTurned = 26,
    /// `GetDisplayLine()`.
    DisplayLine = 27,
    /// `SetScrollBarCurrentRange(a)`, `SetStartingDisplayLine(a)`.
    SetScrollValue = 28,
    /// `GetSelectedBackground()`.
    SelectedBackground = 29,
    /// `GetCopiedSelectedBackground()`.
    CopiedSelectedBackground = 30,
    /// `GetNumberLinesDisplayed()`.
    NumberLinesDisplayed = 31,
    /// `GetScrollBar()`.
    ScrollBar = 32,
    /// `GetScrollBarCaptured()`.
    ScrollBarCaptured = 33,
    /// `GetHasSelection()`.
    HasSelection = 34,
    /// `SetAutoSelect(a != 0)`.
    SetAutoSelect = 35,
    /// `SetFocusedRectColor(f[0..3])`.
    SetFocusedRectColor = 36,
    /// `GetLineHeight()`.
    LineHeight = 37,
    /// `GetSelectionIndex()` (`mCurrentSelection`).
    SelectionIndex = 38,
    /// `GetAutoSelect()`.
    AutoSelect = 39,
    /// `GetCopiedHasSelection()`.
    CopiedHasSelection = 40,
    /// `SetCopiedHasSelection(a != 0)`.
    SetCopiedHasSelection = 41,
    /// `SetCopiedHasFocus(a != 0)`.
    SetCopiedHasFocus = 42,
    /// `GetFocusedRectColor()` -> `f[0..3]`.
    FocusedRectColor = 43,
    /// `GetCopiedFocusedRectColor()` -> `f[0..3]`.
    CopiedFocusedRectColor = 44,
    /// `SetCopiedFocusedRectColor(f[0..3])`.
    SetCopiedFocusedRectColor = 45,
    /// `GetCopyTextRectangle()` -> the address of the copied text rectangle (`GERectangle`'s layout; valid while
    /// the object lives).
    CopyTextRectangle = 46,
    /// `GetDrawCount()` (`mDrawLineCount`).
    DrawCount = 47,
}

/// A line type's C++ members (`UILineOps<T>::kOps`, lle/UIListBox.h): the list's lines are `new`ed `T`s.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeUiLineOps {
    /// `new T` (a slot of the drawn copy).
    pub make: unsafe extern "C" fn() -> *mut c_void,
    /// `new T(*line)`.
    pub clone: unsafe extern "C" fn(*const c_void) -> *mut c_void,
    /// `delete line`.
    pub free: unsafe extern "C" fn(*mut c_void),
    /// `*to = *from`.
    pub assign: unsafe extern "C" fn(*mut c_void, *const c_void),
    /// A `UWideString` line's NUL-terminated bytes (NULL for another line type).
    pub text: unsafe extern "C" fn(*const c_void) -> *const c_char,
    /// `new UWideString(bytes)` (NULL for another line type).
    pub from_text: unsafe extern "C" fn(*const c_char) -> *mut c_void,
}

/// An `InsertLine` predicate: `(*sort)(*a, *b)`.
pub type EeUiLineLess = Option<unsafe extern "C" fn(*mut c_void, *const c_void, *const c_void) -> bool>;
/// `FindString`'s comparison: `*a == *b`.
pub type EeUiLineEq = unsafe extern "C" fn(*const c_void, *const c_void) -> bool;

/// `kErrorNotFound`.
const ERROR_NOT_FOUND: isize = 3;

/// The line array a view names (`UILines<T>`'s `mWhich`): 0 `mData`, 1 `mCopyLineArray`.
const LINES_DATA: u32 = 0;

/// The Rust UIListBox.
pub struct EeUiList {
    pub(crate) host: Host,
    /// The selection record and `mTextRectangle` (`rect` and `border` are read from `UIControl` at each load).
    st: Cell<ListState>,
    /// `mCopyTextRectangle`.
    copy_text: Cell<UiRect>,
    /// `mScrollBarBgColor`, `mFocusedRectColor`, `mCopiedFocusedRectColor`.
    bar_color: Cell<[f32; 3]>,
    focused_color: Cell<[f32; 3]>,
    copied_focused_color: Cell<[f32; 3]>,
    /// `mSelectedBackground`, `mCopiedSelectedBackground` (rhombi by address).
    selected: isize,
    copied_selected: isize,
    /// `mScrollBar` (0 without one).
    bar: Cell<isize>,
    /// `mRepeatPoll` (100 ms), `mRepeatDelayPoll` (500 ms).
    repeat: isize,
    delay: isize,
    /// The line type's members (none in the goldens: their lines were the C++ template's).
    line_ops: Option<EeUiLineOps>,
    /// `mData`, `mCopyLineArray`: the boxed lines.
    lines: RefCell<Vec<*mut c_void>>,
    copy: RefCell<Vec<*mut c_void>>,
}

/// The object as the type-to-find's host: the virtual line count, a line's first byte.
struct Find<'a>(&'a EeUiList);

impl crate::find_key::FindHost for Find<'_> {
    fn lines(&mut self) -> usize {
        self.0.num_lines() as usize
    }
    fn first(&mut self, i: usize) -> Option<u8> {
        let t = self.0.line_text(i);
        // SAFETY: a live line's NUL-terminated bytes
        let b = if t.is_null() { 0 } else { unsafe { *t as u8 } };
        (b != 0).then_some(b)
    }
}

/// The object as the selection algorithms' host.
struct Bridge<'a>(&'a EeUiList);

impl ListHost for Bridge<'_> {
    fn load(&self) -> ListState {
        let o = self.0;
        let mut s = o.st.get();
        s.rect = o.host.rect();
        s.border = o.op(Op::BorderThickness, 0, 0) as c_ulong;
        s
    }
    fn store(&mut self, s: &ListState) {
        self.0.st.set(*s);
    }
    fn num_lines(&self) -> c_ulong {
        self.0.num_lines()
    }
    fn change_selection(&mut self, index: c_ulong, fire: bool, scroll_to: bool) {
        self.0.host.op(Op::ListChangeSelection, index as isize, fire as isize, scroll_to as isize, 0);
    }
    fn clear_selection(&mut self) {
        self.0.op(Op::ListClearSelection, 0, 0);
    }
    fn has_scroll_bar(&self) -> bool {
        self.0.bar.get() != 0
    }
    fn scroll_value(&self) -> c_long {
        self.0.op(Op::BarValue, self.0.bar.get(), 0) as c_long
    }
    fn scroll_max(&self) -> c_long {
        self.0.op(Op::BarMax, self.0.bar.get(), 0) as c_long
    }
    fn set_scroll_value(&mut self, value: c_ulong) {
        self.0.op(Op::BarSetValue, self.0.bar.get(), value as isize);
    }
    fn set_scroll_range(&mut self, _min: c_ulong, max: c_ulong) {
        self.0.op(Op::BarSetRange, self.0.bar.get(), max as isize);
    }
    fn scroll_set_rectangle(&mut self, r: &UiRect) {
        self.0.host.op_rect(Op::PartSetRectangle, self.0.bar.get(), 0, *r);
    }
    fn create_scroll_bar(&mut self, at: UiPoint, length: f32, page: c_ulong, value: c_ulong) {
        let o = self.0;
        let mut f = [at.x, at.y, length, 0.0];
        let bar = o.host.op_f(Op::CreateScrollBar, 0, 0, 0, 0, &mut f);
        o.bar.set(bar);
        o.op(Op::BarSetPageSize, bar, page as isize);
        o.op(Op::BarSetValue, bar, value as isize);
        let mut c = color_buf(o.bar_color.get());
        o.host.op_f(Op::PartSetBgColor, bar, 0, 0, 0, &mut c);
        let mut f = [o.st.get().f.scroll_bar_bg_opacity, 0.0, 0.0, 0.0];
        o.host.op_f(Op::PartSetBgOpacity, bar, 0, 0, 0, &mut f);
    }
    fn set_dirty(&mut self) {
        self.0.op(Op::SetDirty, 1, 0);
    }
    fn fire(&mut self, event: c_ulong) {
        self.0.op(Op::FireTrigger, event as isize, 0);
    }
    fn set_bg_rectangle(&mut self, r: &UiRect) {
        self.0.host.op_rect(Op::SetBgRectangle, 0, 0, *r);
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.0.host.op_point(Op::BaseSetLocation, 0, at);
    }
    fn mouse_location(&self) -> UiPoint {
        self.0.host.mouse()
    }
}

fn color_buf(c: [f32; 3]) -> [f32; 4] {
    [c[0], c[1], c[2], 0.0]
}

impl EeUiList {
    /// The constructor's part after `UIControl`'s, before the C++ sizes the copy and assigns the lines (then
    /// `CheckScrollBar`): `border` the thickness given.
    pub fn new(host: Host, line_height: c_ulong, auto_select: bool, border: c_ulong) -> Self {
        let op = |o: Op, a: isize| host.op(o, a, 0, 0, 0);
        op(Op::SetBorderType, 0);
        let repeat = op(Op::PollCreate, 100);
        let delay = op(Op::PollCreate, 500);
        let selected = op(Op::RhombusCreate, 0);
        let copied_selected = op(Op::RhombusCreate, 0);
        let r = host.rect();
        let b = border as f32;
        let text = UiRect { left: r.left + b, top: r.top + b, bottom: r.bottom - b, right: r.right - b };
        let mut shown = (um_floor(r.height() - (2 * border) as f32) as c_ulong) / line_height;
        if shown as f32 > r.height() {
            shown = 1;
        }
        let f = ListFields {
            scroll_bar_bg_opacity: 0.0,
            number_lines_displayed: shown,
            current_selection: 0,
            draw_line_count: 0,
            line_height,
            auto_select,
            has_selection: true,
            ..ListFields::default()
        };
        let yellow = [1.0, 1.0, 0.0];
        EeUiList {
            host,
            st: Cell::new(ListState { f, text, rect: UiRect::default(), border: 0 }),
            copy_text: Cell::new(UiRect::default()),
            bar_color: Cell::new([0.0; 3]),
            focused_color: Cell::new(yellow),
            copied_focused_color: Cell::new(yellow),
            selected,
            copied_selected,
            bar: Cell::new(0),
            repeat,
            delay,
            line_ops: None,
            lines: RefCell::new(Vec::new()),
            copy: RefCell::new(Vec::new()),
        }
    }

    /// The lines' part of the constructor: `count` lines at `data` (`stride` bytes apart) copied into `mData`, the
    /// drawn copy made `GetNumberLinesDisplayed()` default lines long.
    ///
    /// # Safety
    /// `data` holds `count` live `T`s of `ops`' type.
    pub unsafe fn set_lines(&mut self, ops: EeUiLineOps, data: *const c_void, count: usize, stride: usize) {
        self.line_ops = Some(ops);
        // SAFETY: the contract
        let lines = (0..count).map(|i| unsafe { (ops.clone)((data as *const u8).add(i * stride) as *const c_void) });
        *self.lines.get_mut() = lines.collect();
        let shown = self.fields().number_lines_displayed as usize;
        // SAFETY: the line type's own constructor
        *self.copy.get_mut() = (0..shown).map(|_| unsafe { (ops.make)() }).collect();
    }

    /// Line `i` of `mData` (`which` 0) or `mCopyLineArray` (1), NULL past the end.
    pub fn line(&self, which: u32, i: usize) -> *mut c_void {
        let v = if which == LINES_DATA { self.lines.borrow() } else { self.copy.borrow() };
        v.get(i).copied().unwrap_or(core::ptr::null_mut())
    }

    /// The lines of `mData` (`which` 0) or `mCopyLineArray` (1).
    pub fn line_count(&self, which: u32) -> usize {
        if which == LINES_DATA {
            self.lines.borrow().len()
        } else {
            self.copy.borrow().len()
        }
    }

    fn ops(&self) -> EeUiLineOps {
        self.line_ops.expect("a list box made with its line type")
    }

    /// `AppendLine(*line)` (the base class's body): the copy pushed, the box dirty, the scroll bar checked.
    ///
    /// # Safety
    /// `line` is a live `T`.
    pub unsafe fn append(&self, line: *const c_void) {
        // SAFETY: the contract
        let l = unsafe { (self.ops().clone)(line) };
        self.lines.borrow_mut().push(l);
        self.op(Op::SetDirty, 1, 0);
        self.list(|l| l.check_scroll_bar());
    }

    /// `AppendStringArray(lines)`: each pushed, then the scroll bar checked and the box dirty.
    ///
    /// # Safety
    /// `data` holds `count` live `T`s, `stride` bytes apart.
    pub unsafe fn append_n(&self, data: *const c_void, count: usize, stride: usize) {
        let ops = self.ops();
        for i in 0..count {
            // SAFETY: the contract
            let l = unsafe { (ops.clone)((data as *const u8).add(i * stride) as *const c_void) };
            self.lines.borrow_mut().push(l);
        }
        self.list(|l| l.check_scroll_bar());
        self.op(Op::SetDirty, 1, 0);
    }

    /// `InsertLine(*line, index, sort)`: `std::lower_bound` (MSVC's halving) from `index` with `less(ctx, line i,
    /// line)` (no predicate: never less), the copy inserted there, then `Inserted`.
    ///
    /// # Safety
    /// `line` is a live `T`; `less` is NULL or takes `ctx` and two live `T`s.
    pub unsafe fn insert(&self, line: *const c_void, index: usize, ctx: *mut c_void, less: EeUiLineLess) {
        let at = {
            let v = self.lines.borrow();
            let mut first = index.min(v.len());
            let mut count = v.len() - first;
            if let Some(less) = less {
                while count > 0 {
                    let step = count / 2;
                    // SAFETY: the contract; both lines are live
                    if unsafe { less(ctx, v[first + step], line) } {
                        first += step + 1;
                        count -= step + 1;
                    } else {
                        count = step;
                    }
                }
            }
            first
        };
        // SAFETY: the contract
        let l = unsafe { (self.ops().clone)(line) };
        self.lines.borrow_mut().insert(at, l);
        self.list(|l| l.inserted(at as c_ulong));
    }

    /// `DeleteLine(i)` (the base class's body): the line erased and freed, then `DeletedLine`.
    pub fn delete_line(&self, i: usize) {
        let gone = {
            let mut v = self.lines.borrow_mut();
            (i < v.len()).then(|| v.remove(i))
        };
        if let Some(l) = gone {
            // SAFETY: a line this object made
            unsafe { (self.ops().free)(l) };
        }
        self.list(|l| l.deleted_line(i as c_ulong));
    }

    /// `DeleteAllEntries()` (the base class's body): the lines freed, then `Cleared`.
    pub fn delete_all(&self) {
        self.free_lines(false);
        self.list(|l| l.cleared());
    }

    /// The lines (and with `copy` the drawn copy) freed.
    fn free_lines(&self, copy: bool) {
        let Some(ops) = self.line_ops else { return };
        let gone: Vec<_> = if copy { self.copy.borrow_mut().drain(..).collect() } else { Vec::new() };
        let lines: Vec<_> = self.lines.borrow_mut().drain(..).collect();
        for l in gone.into_iter().chain(lines) {
            // SAFETY: lines this object made
            unsafe { (ops.free)(l) };
        }
    }

    /// `FindString(*line, out)`: the first line `i < GetNumLines()` equal to `line` (`eq`).
    ///
    /// # Safety
    /// `line` is a live `T`; `eq` takes two live `T`s.
    pub unsafe fn find(&self, line: *const c_void, eq: EeUiLineEq) -> Option<usize> {
        let mut i = 0;
        while (i as c_ulong) < self.num_lines() {
            let l = self.line(LINES_DATA, i);
            // SAFETY: the contract; a live line
            if !l.is_null() && unsafe { eq(l, line) } {
                return Some(i);
            }
            i += 1;
        }
        None
    }

    /// `GetCurrentSelection(out)`: the result (OK when there are lines and a selection, else 0xd) and the selection.
    pub fn current_selection(&self) -> (isize, c_ulong) {
        let f = self.fields();
        let ok = self.line_count(LINES_DATA) > 0 && f.has_selection;
        (if ok { 0 } else { 0xd }, f.current_selection)
    }

    /// The visible lines copied: the drawn copy grown to `shown` lines if shorter, then lines `first .. last` copied
    /// to its start.
    fn copy_lines(&self, shown: usize, first: usize, last: usize) {
        let Some(ops) = self.line_ops else { return };
        while self.copy.borrow().len() < shown {
            // SAFETY: the line type's own constructor
            let l = unsafe { (ops.make)() };
            self.copy.borrow_mut().push(l);
        }
        let (lines, copy) = (self.lines.borrow(), self.copy.borrow());
        for (j, i) in (first..last).enumerate() {
            if let (Some(&to), Some(&from)) = (copy.get(j), lines.get(i)) {
                // SAFETY: lines this object made
                unsafe { (ops.assign)(to, from) };
            }
        }
    }

    /// Line `i`'s text (a `UIListBox<UWideString>`'s), NULL for another type or past the end.
    pub fn line_text(&self, i: usize) -> *const c_char {
        let l = self.line(LINES_DATA, i);
        match self.line_ops {
            // SAFETY: a live line of the ops' type
            Some(ops) if !l.is_null() => unsafe { (ops.text)(l) },
            _ => core::ptr::null(),
        }
    }

    /// The direct path's list ops (`direct::list_op`): the lines' count, text and selection answered here, a line
    /// appended from text through the virtual `AppendLine`, every other op passed to the host.
    pub fn direct_op(&self, op: Op, a: isize, b: isize, c: isize, f: &mut [f32; 4]) -> isize {
        match op {
            Op::ListLineCount => self.line_count(LINES_DATA) as isize,
            Op::ListLineText if a >= 0 => self.line_text(a as usize) as isize,
            Op::ListLineText => 0,
            Op::ListSelection => {
                let (r, s) = self.current_selection();
                f[0] = f32::from_bits(s as u32);
                r
            }
            Op::ListAppendText => {
                let Some(ops) = self.line_ops else { return 0 };
                // SAFETY: NUL-terminated bytes (the op's contract)
                let l = unsafe { (ops.from_text)(a as *const c_char) };
                if !l.is_null() {
                    self.host.op(Op::ListAppendLine, l as isize, 0, 0, 0);
                    // SAFETY: made above
                    unsafe { (ops.free)(l) };
                }
                0
            }
            _ => self.host.op_f(op, a, b, c, 0, f),
        }
    }

    /// `UILBString::DrawTextA`: the scroll bar's text and the base's draw, then each drawn line in its row (one pixel
    /// wider than the text rectangle) clipped by the form.
    fn string_draw(&self) -> isize {
        self.draw_scroll_bar_text();
        self.op(Op::DrawCountDecrement, 0, 0);
        let ctr = self.copy_text.get();
        let line_height = self.fields().line_height;
        let mut result = 0;
        let mut i: c_ulong = 0;
        while i < self.fields().draw_line_count {
            let row = draw::string_list_row(&ctr, i, line_height);
            self.host.op_point(Op::StringListTextAt, 0, UiPoint { x: row.left, y: row.top });
            self.op(Op::StringListTextLine, i as isize, 0);
            result = self.host.op_rect(Op::StringListDrawClipped, 0, 0, row);
            i += 1;
        }
        result
    }

    /// `UILBString::UpdateTranslatedKey`: the next line starting with the key's letter selected and scrolled to
    /// (`find_key.rs`); kErrorNotFound when none does.
    fn string_key(&self, key: &[u8]) -> isize {
        let Some(i) = crate::find_key::find(key, self.fields().current_selection, &mut Find(self)) else {
            return ERROR_NOT_FOUND;
        };
        let bar = self.bar.get();
        if bar != 0 {
            self.op(Op::BarSetValue, bar, i as isize);
        }
        self.host.op(Op::ListChangeSelection, i as isize, 1, 1, 0);
        self.op(Op::SetDirty, 1, 0);
        0
    }

    fn op(&self, op: Op, a: isize, b: isize) -> isize {
        self.host.op(op, a, b, 0, 0)
    }

    fn fields(&self) -> ListFields {
        self.st.get().f
    }

    fn update(&self, f: impl FnOnce(&mut ListFields)) {
        let mut s = self.st.get();
        f(&mut s.f);
        self.st.set(s);
    }

    fn num_lines(&self) -> c_ulong {
        self.op(Op::ListNumLines, 0, 0) as c_ulong
    }

    /// One call of the selection algorithms (`list.rs`).
    fn list<R>(&self, f: impl FnOnce(&mut List<Bridge>) -> R) -> R {
        let mut b = Bridge(self);
        let mut l = List::new(&mut b);
        let r = f(&mut l);
        l.finish();
        r
    }

    /// The destructor's part after `mData.clear()`: the rest of `DeleteAllEntries`, then the scroll bar, the polls
    /// and the rhombi deleted in the original's order.
    fn release(&self) {
        self.list(|l| l.cleared());
        let bar = self.bar.get();
        if bar != 0 {
            self.op(Op::DeletePart, bar, 0);
            self.bar.set(0);
        }
        self.op(Op::PollDelete, self.repeat, 0);
        self.op(Op::PollDelete, self.delay, 0);
        self.op(Op::RhombusDelete, self.selected, 0);
        self.op(Op::RhombusDelete, self.copied_selected, 0);
        self.free_lines(true);
    }

    fn copy_data(&self) -> isize {
        let mut result = self.op(Op::BaseCopyData, 0, 0);
        self.update(|f| {
            f.copied_has_focus = false;
            f.copied_has_selection = false;
        });
        self.copy_text.set(self.st.get().text);
        if self.fields().scroll_bar_removed {
            self.op(Op::DeletePart, self.bar.get(), 0);
            self.bar.set(0);
            self.update(|f| {
                f.scroll_bar_removed = false;
                f.scroll_bar_captured = false;
            });
        }
        let bar = self.bar.get();
        let value = if bar != 0 { Some(self.op(Op::BarValue, bar, 0) as c_long) } else { None };
        let (first, last, count) = draw::list_line_range(value, self.fields().number_lines_displayed, self.num_lines());
        self.update(|f| f.draw_line_count = count);
        if bar != 0 {
            result = self.op(Op::PartCopyData, bar, 0);
        }
        self.update(|f| f.copied_scroll_bar_visible = bar != 0);
        let shown = self.fields().number_lines_displayed;
        self.copy_lines(shown as usize, first as usize, last as usize);
        let f = self.fields();
        let ctr = self.copy_text.get();
        let lines = self.num_lines();
        if let Some(r) =
            draw::list_selection(lines, f.has_selection, f.current_selection, first, last, &ctr, f.line_height)
        {
            self.update(|f| f.copied_has_selection = true);
            crate::draw2d::rhombus_set_rect(self.selected, r, || {
                self.host.op_rect(Op::RhombusSetRect, self.selected, 0, r)
            });
            crate::draw2d::rhombus_assign(self.copied_selected, self.selected, || {
                self.op(Op::RhombusAssign, self.copied_selected, self.selected)
            });
            if self.op(Op::TabbingOn, 0, 0) != 0 && self.op(Op::TabStop, 0, 0) == self.op(Op::Index, 0, 0) {
                self.update(|f| f.copied_has_focus = true);
                self.copied_focused_color.set(self.focused_color.get());
            }
        }
        result
    }

    fn draw_polygons(&self) -> isize {
        // the focus rectangle's line (a GELine local of the original's DrawPolygons, made in its colour first)
        let line = self.op(Op::LineCreate, 1, 0);
        let mut c = color_buf(self.copied_focused_color.get());
        crate::draw2d::line_set_color(line, [c[0], c[1], c[2]], || {
            self.host.op_f(Op::LineSetColor, line, 0, 0, 0, &mut c)
        });
        let mut result = self.op(Op::BaseDrawPolygons, 0, 0);
        let f = self.fields();
        if f.copied_scroll_bar_visible {
            result = self.op(Op::PartDrawPolygons, self.bar.get(), 0);
        }
        if f.copied_has_selection {
            result = crate::draw2d::rhombus(self.copied_selected, || self.op(Op::RhombusDraw, self.copied_selected, 0));
            if f.copied_has_focus {
                let r = crate::draw2d::rhombus_rect(self.copied_selected, || {
                    self.host.get_rect(Op::RhombusRect, self.copied_selected)
                });
                for (s, e) in draw::list_focus_lines(&r) {
                    let mut p = [s.x, s.y, e.x, e.y];
                    crate::draw2d::line_set_points(line, p, || {
                        self.host.op_f(Op::LineSetPoints, line, 0, 0, 0, &mut p)
                    });
                    result = crate::draw2d::line(line, || self.op(Op::LineDraw, line, 0));
                }
            }
        }
        self.op(Op::LineDelete, line, 0);
        result
    }

    fn draw_scroll_bar_text(&self) -> isize {
        if self.fields().copied_scroll_bar_visible {
            self.op(Op::PartDrawText, self.bar.get(), 0)
        } else {
            0
        }
    }

    fn update_keyboard(&self) -> isize {
        let mut handled = false;
        let mut repeat = false;
        if self.op(Op::PollRunning, self.delay, 0) != 0 {
            if self.op(Op::PollPoll, self.delay, 0) != 0 {
                self.op(Op::PollStop, self.delay, 0);
                repeat = true;
                self.op(Op::PollStart, self.repeat, 0);
            }
        } else if self.op(Op::PollRunning, self.repeat, 0) != 0 {
            if self.op(Op::PollPoll, self.repeat, 0) != 0 {
                repeat = true;
            }
        } else {
            self.op(Op::PollStart, self.delay, 0);
            repeat = true;
        }
        // the key questions (edit_keys' numbering): 9 Down, 8 Up, 6 Home, 7 End, 10 Enter
        let key = |k: isize| self.op(Op::EditKey, k, 0) != 0;
        if key(9) {
            handled = true;
            if repeat {
                self.op(Op::ListProcessDown, 0, 0);
            }
        }
        if key(8) {
            handled = true;
            if repeat {
                self.op(Op::ListProcessUp, 0, 0);
            }
        }
        if key(6) {
            handled = true;
            self.op(Op::ListProcessHome, 0, 0);
        }
        if key(7) {
            handled = true;
            self.op(Op::ListProcessEnd, 0, 0);
        }
        if key(10) {
            handled = true;
            self.op(Op::FireTrigger, TRIGGER_ENTER_PRESSED, 0);
        }
        if !handled {
            self.op(Op::PollStop, self.delay, 0);
            self.op(Op::PollStop, self.repeat, 0);
        }
        0
    }

    fn mouse_down(&self) -> isize {
        let mut result = 0;
        let index = self.op(Op::Index, 0, 0);
        if self.op(Op::TabStop, 0, 0) != index && self.op(Op::Tabable, 0, 0) != 0 {
            self.op(Op::SetTabStop, index, 0);
        }
        let bar = self.bar.get();
        if bar != 0 && self.host.get_rect(Op::PartRect, bar).contains(self.host.mouse()) {
            result = self.op(Op::PartMouseDown, bar, 0);
            self.op(Op::CaptureMouse, 0, 0);
            self.update(|f| f.scroll_bar_captured = true);
        } else if self.st.get().text.contains(self.host.mouse()) {
            self.op(Op::ButtonDown, 0, 0);
            self.op(Op::CaptureMouse, 0, 0);
        }
        result
    }

    fn mouse_up(&self) -> isize {
        let mut result = 0;
        if self.fields().scroll_bar_captured {
            result = self.op(Op::PartMouseUp, self.bar.get(), 0);
        } else if self.list(|l| l.up_on_selection()) {
            self.op(Op::ButtonUp, 0, 0);
        }
        self.op(Op::ReleaseMouse, 0, 0);
        self.update(|f| {
            f.scroll_bar_captured = false;
            f.button_down = false;
        });
        result
    }

    fn mouse_move(&self, at: UiPoint) -> isize {
        if self.fields().scroll_bar_captured {
            self.host.op_point(Op::PartMouseMove, self.bar.get(), at)
        } else {
            self.list(|l| l.mouse_move(at));
            0
        }
    }

    fn tabbed_from(&self) {
        self.op(Op::ReleaseMouse, 0, 0);
        self.op(Op::ReleaseKeyboard, 0, 0);
        self.update(|f| {
            f.button_down = false;
            f.scroll_bar_captured = false;
        });
        self.op(Op::SetDirty, 1, 0);
    }

    fn change_width(&self, width: c_ulong) {
        let mut r = self.host.rect();
        r.right = r.left + width as f32 - 1.0;
        self.host.op_rect(Op::BaseSetRectangle, 0, 0, r);
    }

    /// One inward call ([`EeUiListCall`]): `f` the four floats in and out.
    ///
    /// # Safety
    /// `f` is NULL or four live floats.
    pub unsafe fn call(&self, call: EeUiListCall, a: isize, b: isize, f: *mut f32) -> isize {
        use EeUiListCall as C;
        // SAFETY: the contract
        let fin = unsafe { read_buf(f) };
        let at = UiPoint { x: fin[0], y: fin[1] };
        let rgb = [fin[0], fin[1], fin[2]];
        let out = |v: [f32; 3]| {
            // SAFETY: the contract
            unsafe { write_buf(f, color_buf(v)) };
            0
        };
        let set = |g: &dyn Fn(&mut ListFields)| {
            self.update(g);
            0
        };
        let bar = self.bar.get();
        match call {
            C::CopyData => self.copy_data(),
            C::DrawPolygons => self.draw_polygons(),
            C::DrawText => {
                self.draw_scroll_bar_text();
                self.op(Op::DrawCountDecrement, 0, 0);
                0
            }
            C::DrawScrollBarText => self.draw_scroll_bar_text(),
            C::ChangeSelectionIndex => {
                self.list(|l| l.change_selection_index(a as c_ulong, b & 1 != 0, b & 2 != 0));
                0
            }
            C::ChangeWidth => {
                self.change_width(a as c_ulong);
                0
            }
            C::ClearSelection => set(&|f| f.has_selection = false),
            C::Cleared => {
                self.list(|l| l.cleared());
                0
            }
            C::DeletedLine => {
                self.list(|l| l.deleted_line(a as c_ulong));
                0
            }
            C::Inserted => {
                self.list(|l| l.inserted(a as c_ulong));
                0
            }
            C::CheckScrollBar => {
                self.list(|l| l.check_scroll_bar());
                0
            }
            C::SetLocation => {
                self.list(|l| l.set_location(at));
                0
            }
            C::SetScrollBarBgOpacity => {
                if bar != 0 {
                    let mut o = [fin[0], 0.0, 0.0, 0.0];
                    self.host.op_f(Op::PartSetBgOpacity, bar, 0, 0, 0, &mut o);
                }
                set(&|f| f.scroll_bar_bg_opacity = fin[0])
            }
            C::SetScrollBarBgColor => {
                if bar != 0 {
                    let mut c = color_buf(rgb);
                    self.host.op_f(Op::PartSetBgColor, bar, 0, 0, 0, &mut c);
                }
                self.bar_color.set(rgb);
                0
            }
            C::UpdateKeyboard => self.update_keyboard(),
            C::ButtonDown => {
                self.list(|l| l.button_down());
                0
            }
            C::MouseLButtonDown => self.mouse_down(),
            C::MouseLButtonUp => self.mouse_up(),
            C::MouseDoublePressed => {
                if self.list(|l| l.double_pressed()) {
                    0
                } else {
                    self.op(Op::BaseMouseDoublePressed, 0, 0)
                }
            }
            C::MouseMove => self.mouse_move(at),
            C::ProcessDown => {
                self.list(|l| l.down());
                0
            }
            C::ProcessEnd => {
                self.list(|l| l.end());
                0
            }
            C::ProcessHome => {
                self.list(|l| l.home());
                0
            }
            C::ProcessUp => {
                self.list(|l| l.up());
                0
            }
            C::TabbedFrom => {
                self.tabbed_from();
                0
            }
            C::MouseWheelTurned => {
                if bar != 0 {
                    self.op(Op::BarIncrementBy, bar, a);
                    0
                } else {
                    ERROR_FAILED
                }
            }
            C::DisplayLine => {
                if bar != 0 {
                    self.op(Op::BarValue, bar, 0)
                } else {
                    0
                }
            }
            C::SetScrollValue => {
                if bar != 0 {
                    self.op(Op::BarSetValue, bar, a);
                }
                0
            }
            C::SelectedBackground => self.selected,
            C::CopiedSelectedBackground => self.copied_selected,
            C::NumberLinesDisplayed => self.fields().number_lines_displayed as isize,
            C::ScrollBar => bar,
            C::ScrollBarCaptured => self.fields().scroll_bar_captured as isize,
            C::HasSelection => self.fields().has_selection as isize,
            C::SetAutoSelect => set(&|f| f.auto_select = a != 0),
            C::SetFocusedRectColor => {
                self.focused_color.set(rgb);
                0
            }
            C::LineHeight => self.fields().line_height as isize,
            C::SelectionIndex => self.fields().current_selection as isize,
            C::AutoSelect => self.fields().auto_select as isize,
            C::CopiedHasSelection => self.fields().copied_has_selection as isize,
            C::SetCopiedHasSelection => set(&|f| f.copied_has_selection = a != 0),
            C::SetCopiedHasFocus => set(&|f| f.copied_has_focus = a != 0),
            C::FocusedRectColor => out(self.focused_color.get()),
            C::CopiedFocusedRectColor => out(self.copied_focused_color.get()),
            C::SetCopiedFocusedRectColor => {
                self.copied_focused_color.set(rgb);
                0
            }
            C::CopyTextRectangle => self.copy_text.as_ptr() as isize,
            C::DrawCount => self.fields().draw_line_count as isize,
        }
    }
}

// ---- the C ABI

/// A new Rust list box for the C++ `list` (its `UIControl` part constructed) with a copy of the `count` lines at
/// `data` (`stride` bytes apart, of the type `ops` makes); the C++ then calls `CheckScrollBar`.
///
/// # Safety
/// `host` answers the ops for `list`; `ops` is live; `data` holds `count` live lines.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_list_new(
    list: *mut c_void,
    host: EeUiHostFn,
    ops: *const EeUiLineOps,
    data: *const c_void,
    count: usize,
    stride: usize,
    line_height: c_ulong,
    auto_select: bool,
    border: c_ulong,
) -> *mut EeUiList {
    let mut o = EeUiList::new(Host { obj: list, f: host }, line_height, auto_select, border);
    // SAFETY: the contract
    unsafe { o.set_lines(*ops, data, count, stride) };
    crate::direct::made(crate::direct::Kind::List, list, Box::into_raw(Box::new(o)))
}

/// The lines of `mData` (`which` 0) or `mCopyLineArray` (1).
///
/// # Safety
/// `list` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_lines(list: *const EeUiList, which: u32) -> usize {
    // SAFETY: the contract
    unsafe { (*list).line_count(which) }
}

/// Line `i` of `mData` (`which` 0) or `mCopyLineArray` (1): the `T` (NULL past the end).
///
/// # Safety
/// `list` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_line(list: *const EeUiList, which: u32, i: usize) -> *mut c_void {
    // SAFETY: the contract
    unsafe { (*list).line(which, i) }
}

/// `AppendLine(*line)` (the base class's).
///
/// # Safety
/// `list` is live; `line` is a live line.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_append(list: *const EeUiList, line: *const c_void) {
    // SAFETY: the contract
    unsafe { (*list).append(line) }
}

/// `AppendStringArray`: the `count` lines at `data`, `stride` bytes apart.
///
/// # Safety
/// `list` is live; `data` holds `count` live lines.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_append_n(list: *const EeUiList, data: *const c_void, count: usize, stride: usize) {
    // SAFETY: the contract
    unsafe { (*list).append_n(data, count, stride) }
}

/// `InsertLine(*line, index, sort)`: `less(sort, a, b)` the predicate (NULL: the line goes in at `index`).
///
/// # Safety
/// `list` is live; `line` is a live line; `less` is NULL or takes `sort` and two live lines.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_insert(
    list: *const EeUiList,
    line: *const c_void,
    index: c_ulong,
    sort: *mut c_void,
    less: EeUiLineLess,
) {
    // SAFETY: the contract
    unsafe { (*list).insert(line, index as usize, sort, less) }
}

/// `DeleteLine(i)` (the base class's).
///
/// # Safety
/// `list` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_delete_line(list: *const EeUiList, i: c_ulong) {
    // SAFETY: the contract
    unsafe { (*list).delete_line(i as usize) }
}

/// `DeleteAllEntries()` (the base class's).
///
/// # Safety
/// `list` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_delete_all(list: *const EeUiList) {
    // SAFETY: the contract
    unsafe { (*list).delete_all() }
}

/// `FindString(*line, *out)`: true with the index in `out` when a line equals `line` (`eq`).
///
/// # Safety
/// `list` is live; `line` is a live line; `eq` takes two live lines; `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_find(
    list: *const EeUiList,
    line: *const c_void,
    eq: EeUiLineEq,
    out: *mut c_ulong,
) -> bool {
    // SAFETY: the contract
    match unsafe { (*list).find(line, eq) } {
        Some(i) => {
            // SAFETY: the contract
            unsafe { *out = i as c_ulong };
            true
        }
        None => false,
    }
}

/// `GetCurrentSelection(*out)`: the selection in `out`; OK when there are lines and a selection, else 0xd.
///
/// # Safety
/// `list` is live; `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_current_selection(list: *const EeUiList, out: *mut c_ulong) -> c_long {
    // SAFETY: the contract
    let (r, s) = unsafe { (*list).current_selection() };
    // SAFETY: the contract
    unsafe { *out = s };
    r as c_long
}

/// `UILBString::DrawTextA`.
///
/// # Safety
/// `list` is a live string list's.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_string_list_draw(list: *const EeUiList) -> c_long {
    // SAFETY: the contract
    unsafe { (*list).string_draw() as c_long }
}

/// `UILBString::UpdateTranslatedKey(key)`: OK, or kErrorNotFound when no line starts with the key's letter.
///
/// # Safety
/// `list` is a live string list's; `key` is NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_string_list_key(list: *const EeUiList, key: *const c_char) -> c_long {
    // SAFETY: the contract
    let key = if key.is_null() { &[][..] } else { unsafe { CStr::from_ptr(key) }.to_bytes() };
    // SAFETY: the contract
    unsafe { (*list).string_key(key) as c_long }
}

/// The destructor (after `mData.clear()`): the rest of `DeleteAllEntries`, then the parts deleted; frees the object.
///
/// # Safety
/// `list` came from `ee_ui_list_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_free(list: *mut EeUiList) {
    if list.is_null() {
        return;
    }
    crate::direct::forget(crate::direct::Kind::List, list as *const c_void);
    // SAFETY: the contract
    unsafe { (*list).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(list) });
}

/// One of UIListBox's methods ([`EeUiListCall`]).
///
/// # Safety
/// `list` is live; `f` is NULL or four live floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_call(
    list: *const EeUiList,
    call: EeUiListCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*list).call(call, a, b, f) }
}

/// `UWSNoCaseAlphaSort::operator()`: `left` sorts before `right` with the case folded.
///
/// # Safety
/// Both are NUL-terminated bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_text_less_no_case(left: *const c_char, right: *const c_char) -> bool {
    // SAFETY: the contract
    let (l, r) = unsafe { (CStr::from_ptr(left), CStr::from_ptr(right)) };
    less_no_case(&Platform, l.to_bytes_with_nul(), r.to_bytes_with_nul())
}

#[cfg(test)]
#[path = "listobj_tests.rs"]
mod tests;
