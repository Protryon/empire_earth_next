//! `UIListBox<T>` as a whole Rust object (R6 ui round 15): the template's state that does not depend on `T` (the
//! text rectangles, the colours, the selection record, the scroll bar, the key-repeat polls and the two selection
//! rhombi) and every method that does not touch the lines. The C++ template keeps only what depends on `T`: the
//! lines (`mData`) and their drawn copy (`mCopyLineArray`), the data methods (`AppendLine`, `InsertLine`,
//! `DeleteLine`'s erase, `FindString`, `Sort`, `ReplaceLine`), and the per-`T` host (`UIListBoxRust<T>::Op`), which
//! answers the ops that need the instantiation: the virtual `GetNumLines`, `ChangeSelectionIndex`,
//! `ClearSelection`, `ProcessDown`/`Up`/`Home`/`End`, `ButtonDown`/`ButtonUp` (a subclass's override runs) and the
//! copy of the visible lines; every other op goes to the shared host (`UIListBoxHostOp`, UILBString.cpp).
//!
//! The selection and scrolling algorithms are round 4's `list.rs` with their goldens; this object is their host
//! ([`ListHost`]).

use core::cell::Cell;
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
        }
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
        self.host.op(Op::ListCopyLines, shown as isize, first as isize, last as isize, 0);
        let f = self.fields();
        let ctr = self.copy_text.get();
        let lines = self.num_lines();
        if let Some(r) =
            draw::list_selection(lines, f.has_selection, f.current_selection, first, last, &ctr, f.line_height)
        {
            self.update(|f| f.copied_has_selection = true);
            self.host.op_rect(Op::RhombusSetRect, self.selected, 0, r);
            self.op(Op::RhombusAssign, self.copied_selected, self.selected);
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
        self.host.op_f(Op::LineSetColor, line, 0, 0, 0, &mut c);
        let mut result = self.op(Op::BaseDrawPolygons, 0, 0);
        let f = self.fields();
        if f.copied_scroll_bar_visible {
            result = self.op(Op::PartDrawPolygons, self.bar.get(), 0);
        }
        if f.copied_has_selection {
            result = self.op(Op::RhombusDraw, self.copied_selected, 0);
            if f.copied_has_focus {
                let r = self.host.get_rect(Op::RhombusRect, self.copied_selected);
                for (s, e) in draw::list_focus_lines(&r) {
                    let mut p = [s.x, s.y, e.x, e.y];
                    self.host.op_f(Op::LineSetPoints, line, 0, 0, 0, &mut p);
                    result = self.op(Op::LineDraw, line, 0);
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
                self.op(Op::ListProcess, 0, 0);
            }
        }
        if key(8) {
            handled = true;
            if repeat {
                self.op(Op::ListProcess, 1, 0);
            }
        }
        if key(6) {
            handled = true;
            self.op(Op::ListProcess, 2, 0);
        }
        if key(7) {
            handled = true;
            self.op(Op::ListProcess, 3, 0);
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

/// A new Rust list box for the C++ `list` (its `UIControl` part constructed); the C++ then sizes the copy, assigns
/// the lines and calls `CheckScrollBar`.
///
/// # Safety
/// `host` answers the ops for `list`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_new(
    list: *mut c_void,
    host: EeUiHostFn,
    line_height: c_ulong,
    auto_select: bool,
    border: c_ulong,
) -> *mut EeUiList {
    let obj = Box::into_raw(Box::new(EeUiList::new(Host { obj: list, f: host }, line_height, auto_select, border)));
    crate::direct::made(crate::direct::Kind::List, list, obj)
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
