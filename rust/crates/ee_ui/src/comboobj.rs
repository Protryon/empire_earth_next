//! `UIComboBox` as a whole Rust object (R6 ui round 15): the combo box's record (the selection, the list's open and
//! copied state, the type and the list's height), its parts (the text label or edit box, the drop button, the
//! string list; C++ objects held by address) and its focus highlight (a `GERhombus` this object makes), with every
//! method. The C++ class keeps its constructor's part making (the parts are C++ controls built from the caller's
//! font and lines) and is otherwise one-line forwarders; the destructor frees this object, which deletes the parts.
//!
//! The logic is round 3's `combo.rs` with its golden; this object is its host ([`ComboHost`]). Each host call is
//! one op: the questions ([`EeUiOp::ComboQuery`], pure: `a` the question in its top byte and its argument below) and
//! the actions ([`EeUiOp::ComboAct`]: `a` the action, `b`, `c`, `d` its arguments), both numbered as the record
//! FFI's `ee_uicpp_combo_*` host calls were, so the C++ host's switch is their bodies.

use core::cell::Cell;
use core::ffi::{c_char, c_long, c_ulong, c_void, CStr};

use crate::combo::{self, ComboFields, ComboHost, ComboKey, ComboPart, ComboState};
use crate::obj::{read_buf, write_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::{UiPoint, UiRect};
use crate::text::Platform;

/// The questions (`ComboQuery`'s `a >> 24`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiComboQuery {
    /// Part `a`'s rectangle -> `f`.
    RectOf = 1,
    /// The text box's border thickness.
    TextBoxBorder = 2,
    /// The list's `GetLineCount()`.
    LineCount = 3,
    /// The list's line `a` -> its NUL-terminated bytes.
    Line = 4,
    /// The list's line height.
    LineHeight = 5,
    /// The list's border thickness.
    ListBorder = 6,
    /// The list's scroll bar rectangle -> whether it has one, the rectangle in `f`.
    ListScrollBarRect = 7,
    /// The list's `GetScrollBarCaptured()`.
    ListScrollBarCaptured = 8,
    /// The list's `GetCurrentSelection(out)` -> its result, `out` in `f[0]`'s bits.
    ListSelection = 9,
    /// The window's height.
    WindowHeight = 10,
    /// The edit box's text -> its NUL-terminated bytes.
    EditText = 11,
    /// The form's `GetTabbingOn()`.
    TabbingOn = 12,
    /// The form's `GetTabStop()`.
    TabStop = 13,
    /// `GetHasMouseCapture()`.
    HasMouseCapture = 14,
    /// `GetMouseLocation()` -> `f[0]`, `f[1]`.
    MouseLocation = 15,
    /// Key `a` ([`ComboKey`]).
    Key = 16,
}

/// The actions (`ComboAct`'s `a`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiComboAct {
    /// The list's virtual `ChangeSelectionIndex(b, c & 1, c & 2)`.
    ListChangeSelection = 0,
    /// The list's `InsertLine(UWideString(b bytes), c)`.
    ListInsertLine = 1,
    /// The list's virtual `DeleteLine(b)`.
    ListDeleteLine = 2,
    /// The list's virtual `DeleteAllEntries()`.
    ListDeleteAll = 3,
    /// The list's `ChangeWidth(b)`.
    ListChangeWidth = 4,
    /// The text box shows list line `b`.
    ShowLine = 5,
    /// The text box's text set empty.
    ClearText = 6,
    /// The text box's text deleted (the label's line 0, the edit box's `DeleteText()`).
    DeleteText = 7,
    /// Part `b`'s virtual `MouseLButtonDown()`.
    PartMouseDown = 8,
    /// Part `b`'s virtual `MouseLButtonUp()`.
    PartMouseUp = 9,
    /// Part `b`'s virtual `MouseMove(f[0], f[1])`.
    PartMouseMove = 10,
    /// The list's virtual `MouseWheelTurned(b)`.
    ListWheel = 11,
    /// Part `b`'s virtual `UpdateKeyboard()`.
    PartUpdateKeyboard = 12,
    /// Part `b`'s virtual `UpdateTranslatedKey(c bytes, d)`.
    PartTranslatedKey = 13,
    /// Part `b`'s virtual `TabbedTo()` (`c` != 0) or `TabbedFrom()`.
    PartTabbed = 14,
    /// Part `b`'s virtual `SetRectangle(f)`.
    PartSetRectangle = 15,
    /// Part `b`'s virtual `SetLocation(f[0], f[1])`.
    PartSetLocation = 16,
    /// Part `b`'s virtual `CopyData()`.
    PartCopyData = 17,
    /// The list's text takes the text box's colours and transparency.
    CopyListColours = 18,
    /// `UIControl::CopyData()`.
    BaseCopyData = 20,
    /// The form's `SetPopup(b != 0 ? this : NULL)`.
    SetPopup = 21,
    /// The virtual `SetDirty(true)`.
    SetDirty = 22,
    /// The virtual `SetRectangle(f)`.
    SetRectangle = 23,
    /// `UIControl::SetRectangle(f)`.
    BaseSetRectangle = 24,
    /// `UIControl::SetLocation(f[0], f[1])`.
    BaseSetLocation = 25,
    /// `SetAlwaysDirty(b != 0)`.
    SetAlwaysDirty = 26,
    /// `FireTriggerEvent(b)`.
    Fire = 27,
    /// `CaptureMouse()`.
    CaptureMouse = 28,
    /// `CaptureKeyboard()`.
    CaptureKeyboard = 29,
    /// The virtual `ReleaseCapturedMouse()`.
    ReleaseMouse = 30,
    /// The virtual `ReleaseCapturedKeyboard()`.
    ReleaseKeyboard = 31,
    /// The virtual `OpenListBox()`.
    OpenList = 32,
    /// The virtual `CloseListBox()`.
    CloseList = 33,
    /// The virtual `AddString()`.
    AddString = 34,
    /// The virtual `ChangeSelection()`.
    ChangeSelection = 35,
    /// The virtual `ProcessDown()` (`b` != 0) or `ProcessUp()`.
    ProcessArrow = 36,
}

/// The inward calls (the C++ shim's methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiComboCall {
    /// `AddString()`.
    AddString = 1,
    /// `AppendLine` / `AppendStringArray` after the C++ dirtied the box and appended: an open list reopens.
    Reopen = 2,
    /// `ChangeSelection()`.
    ChangeSelection = 3,
    /// `ChangeWidth(a)`.
    ChangeWidth = 4,
    /// `CloseListBox()`.
    CloseListBox = 5,
    /// `CopyData()`.
    CopyData = 6,
    /// `DeleteAllEntries()`.
    DeleteAllEntries = 7,
    /// `DeleteLine(a)`.
    DeleteLine = 8,
    /// `DrawPolygons()`.
    DrawPolygons = 9,
    /// `DrawTextA()`.
    DrawText = 10,
    /// `HideControl()`.
    HideControl = 11,
    /// `InsertLine(a bytes, b)`.
    InsertLine = 12,
    /// `MouseLButtonDown()`.
    MouseLButtonDown = 13,
    /// `MouseLButtonUp()`.
    MouseLButtonUp = 14,
    /// `MouseMove(f[0], f[1])`.
    MouseMove = 15,
    /// `MouseWheelTurned(a)`.
    MouseWheelTurned = 16,
    /// `OpenListBox()`.
    OpenListBox = 17,
    /// `ProcessDown()` / `ProcessUp()` (the two bodies are one).
    ProcessArrow = 18,
    /// `SetCurrentSelection(a)`.
    SetCurrentSelection = 19,
    /// `SetLocation(f[0], f[1])`.
    SetLocation = 20,
    /// `TabbedTo()` (`a` != 0) or `TabbedFrom()`.
    Tabbed = 21,
    /// `UpdateKeyboard()`.
    UpdateKeyboard = 22,
    /// `UpdateTranslatedKey(a bytes, b)`.
    UpdateTranslatedKey = 23,
    /// `mCurrentSelection`.
    Selection = 24,
    /// `mIsListBoxOpen`.
    IsListBoxOpen = 25,
    /// `mTextBoxType`.
    TextBoxType = 26,
    /// Part `a` ([`ComboPart`]) -> its address (0 for a missing label or edit box).
    Part = 27,
    /// `SetListBoxAlwaysDirty(a != 0)`.
    SetListBoxAlwaysDirty = 28,
    /// The focus highlight's `SetColor(f[0..3])` (`SetListBoxSelectedBgColor`'s second half).
    SetSelectedColor = 29,
}

/// The Rust UIComboBox.
pub struct EeUiCombo {
    host: Host,
    /// The record (`text_box` is the label or the edit box).
    f: Cell<ComboFields>,
    label: isize,
    drop: isize,
    list: isize,
    edit: isize,
    /// `mSelectedBackground` (a `GERhombus` member of the original).
    rhombus: isize,
    /// `UpdateTranslatedKey`'s key and repeat while it runs.
    key: Cell<(isize, isize)>,
}

impl EeUiCombo {
    /// The drop-down list's C++ object (its `UILBString*`; the direct path's list).
    pub fn list_cpp(&self) -> isize {
        self.list
    }
}

/// NUL-terminated bytes at `p` (with the terminator).
///
/// # Safety
/// `p` is NUL-terminated bytes.
unsafe fn bytes_at(p: isize) -> Vec<u8> {
    // SAFETY: the contract
    unsafe { CStr::from_ptr(p as *const c_char) }.to_bytes_with_nul().to_vec()
}

fn cstr(text: &[u8]) -> Vec<u8> {
    let mut v = text[..text.iter().position(|&b| b == 0).unwrap_or(text.len())].to_vec();
    v.push(0);
    v
}

/// The object as the combo logic's host.
struct Bridge<'a>(&'a EeUiCombo);

impl Bridge<'_> {
    fn q(&self, q: EeUiComboQuery, arg: isize) -> isize {
        self.0.host.op(Op::ComboQuery, ((q as isize) << 24) | arg, 0, 0, 0)
    }
    fn qf(&self, q: EeUiComboQuery, arg: isize) -> (isize, [f32; 4]) {
        let mut f = [0.0f32; 4];
        let r = self.0.host.op_f(Op::ComboQuery, ((q as isize) << 24) | arg, 0, 0, 0, &mut f);
        (r, f)
    }
    fn act(&self, a: EeUiComboAct, b: isize, c: isize) -> isize {
        self.0.host.op(Op::ComboAct, a as isize, b, c, 0)
    }
    fn act_f(&self, a: EeUiComboAct, b: isize, f: [f32; 4]) -> isize {
        let mut f = f;
        self.0.host.op_f(Op::ComboAct, a as isize, b, 0, 0, &mut f)
    }
}

fn rect_f(r: UiRect) -> [f32; 4] {
    crate::obj::rect_buf(r)
}

impl ComboHost for Bridge<'_> {
    fn load(&self) -> ComboState {
        ComboState {
            f: self.0.f.get(),
            rect: self.0.host.rect(),
            index: self.0.host.op(Op::Index, 0, 0, 0, 0) as c_ulong,
        }
    }
    fn store(&mut self, f: &ComboFields) {
        self.0.f.set(*f);
    }
    fn has_label(&self) -> bool {
        self.0.label != 0
    }
    fn rect_of(&self, part: ComboPart) -> UiRect {
        crate::obj::buf_rect(&self.qf(EeUiComboQuery::RectOf, part as isize).1)
    }
    fn text_box_border(&self) -> c_ulong {
        self.q(EeUiComboQuery::TextBoxBorder, 0) as c_ulong
    }
    fn line_count(&self) -> c_ulong {
        self.q(EeUiComboQuery::LineCount, 0) as c_ulong
    }
    fn line(&self, i: c_ulong) -> Vec<u8> {
        // SAFETY: the host answers the line's NUL-terminated bytes
        unsafe { bytes_at(self.q(EeUiComboQuery::Line, i as isize)) }
    }
    fn line_height(&self) -> c_ulong {
        self.q(EeUiComboQuery::LineHeight, 0) as c_ulong
    }
    fn list_border(&self) -> c_ulong {
        self.q(EeUiComboQuery::ListBorder, 0) as c_ulong
    }
    fn list_scroll_bar_rect(&self) -> Option<UiRect> {
        let (r, f) = self.qf(EeUiComboQuery::ListScrollBarRect, 0);
        (r != 0).then_some(crate::obj::buf_rect(&f))
    }
    fn list_scroll_bar_captured(&self) -> bool {
        self.q(EeUiComboQuery::ListScrollBarCaptured, 0) != 0
    }
    fn list_selection(&self, out: &mut c_ulong) -> c_long {
        let (r, f) = self.qf(EeUiComboQuery::ListSelection, 0);
        *out = f[0].to_bits() as c_ulong;
        r as c_long
    }
    fn list_change_selection(&mut self, i: c_ulong, fire: bool, scroll: bool) {
        self.act(EeUiComboAct::ListChangeSelection, i as isize, (fire as isize) | ((scroll as isize) << 1));
    }
    fn list_insert_line(&mut self, text: &[u8], i: c_ulong) {
        let t = cstr(text);
        self.act(EeUiComboAct::ListInsertLine, t.as_ptr() as isize, i as isize);
    }
    fn list_delete_line(&mut self, i: c_ulong) {
        self.act(EeUiComboAct::ListDeleteLine, i as isize, 0);
    }
    fn list_delete_all(&mut self) {
        self.act(EeUiComboAct::ListDeleteAll, 0, 0);
    }
    fn list_change_width(&mut self, w: c_ulong) {
        self.act(EeUiComboAct::ListChangeWidth, w as isize, 0);
    }
    fn window_height(&self) -> c_ulong {
        self.q(EeUiComboQuery::WindowHeight, 0) as c_ulong
    }
    fn edit_text(&self) -> Vec<u8> {
        // SAFETY: the host answers the edit box's NUL-terminated bytes
        unsafe { bytes_at(self.q(EeUiComboQuery::EditText, 0)) }
    }
    fn show_line(&mut self, i: c_ulong) {
        self.act(EeUiComboAct::ShowLine, i as isize, 0);
    }
    fn clear_text(&mut self) {
        self.act(EeUiComboAct::ClearText, 0, 0);
    }
    fn delete_text(&mut self) {
        self.act(EeUiComboAct::DeleteText, 0, 0);
    }
    fn part_mouse_down(&mut self, part: ComboPart) -> c_long {
        self.act(EeUiComboAct::PartMouseDown, part as isize, 0) as c_long
    }
    fn part_mouse_up(&mut self, part: ComboPart) -> c_long {
        self.act(EeUiComboAct::PartMouseUp, part as isize, 0) as c_long
    }
    fn part_mouse_move(&mut self, part: ComboPart, at: UiPoint) -> c_long {
        self.act_f(EeUiComboAct::PartMouseMove, part as isize, [at.x, at.y, 0.0, 0.0]) as c_long
    }
    fn list_wheel(&mut self, delta: c_long) -> c_long {
        self.act(EeUiComboAct::ListWheel, delta as isize, 0) as c_long
    }
    fn part_update_keyboard(&mut self, part: ComboPart) -> c_long {
        self.act(EeUiComboAct::PartUpdateKeyboard, part as isize, 0) as c_long
    }
    fn part_translated_key(&mut self, part: ComboPart) -> c_long {
        let (key, repeat) = self.0.key.get();
        self.0.host.op(Op::ComboAct, EeUiComboAct::PartTranslatedKey as isize, part as isize, key, repeat) as c_long
    }
    fn part_tabbed(&mut self, part: ComboPart, to: bool) {
        self.act(EeUiComboAct::PartTabbed, part as isize, to as isize);
    }
    fn part_set_rectangle(&mut self, part: ComboPart, r: UiRect) {
        self.act_f(EeUiComboAct::PartSetRectangle, part as isize, rect_f(r));
    }
    fn part_set_location(&mut self, part: ComboPart, at: UiPoint) {
        self.act_f(EeUiComboAct::PartSetLocation, part as isize, [at.x, at.y, 0.0, 0.0]);
    }
    fn part_copy_data(&mut self, part: ComboPart) -> c_long {
        self.act(EeUiComboAct::PartCopyData, part as isize, 0) as c_long
    }
    fn copy_list_colours(&mut self) {
        self.act(EeUiComboAct::CopyListColours, 0, 0);
    }
    fn set_focus_rect(&mut self, r: UiRect) {
        self.0.host.op_rect(Op::RhombusSetRect, self.0.rhombus, 0, r);
    }
    fn base_copy_data(&mut self) -> c_long {
        self.act(EeUiComboAct::BaseCopyData, 0, 0) as c_long
    }
    fn tabbing_on(&self) -> bool {
        self.q(EeUiComboQuery::TabbingOn, 0) != 0
    }
    fn tab_stop(&self) -> c_ulong {
        self.q(EeUiComboQuery::TabStop, 0) as c_ulong
    }
    fn set_popup(&mut self, this: bool) {
        self.act(EeUiComboAct::SetPopup, this as isize, 0);
    }
    fn set_dirty(&mut self) {
        self.act(EeUiComboAct::SetDirty, 0, 0);
    }
    fn set_rectangle(&mut self, r: UiRect) {
        self.act_f(EeUiComboAct::SetRectangle, 0, rect_f(r));
    }
    fn base_set_rectangle(&mut self, r: UiRect) {
        self.act_f(EeUiComboAct::BaseSetRectangle, 0, rect_f(r));
    }
    fn base_set_location(&mut self, at: UiPoint) {
        self.act_f(EeUiComboAct::BaseSetLocation, 0, [at.x, at.y, 0.0, 0.0]);
    }
    fn set_always_dirty(&mut self, on: bool) {
        self.act(EeUiComboAct::SetAlwaysDirty, on as isize, 0);
    }
    fn fire(&mut self, event: c_ulong) {
        self.act(EeUiComboAct::Fire, event as isize, 0);
    }
    fn capture_mouse(&mut self) {
        self.act(EeUiComboAct::CaptureMouse, 0, 0);
    }
    fn capture_keyboard(&mut self) {
        self.act(EeUiComboAct::CaptureKeyboard, 0, 0);
    }
    fn release_mouse(&mut self) {
        self.act(EeUiComboAct::ReleaseMouse, 0, 0);
    }
    fn release_keyboard(&mut self) {
        self.act(EeUiComboAct::ReleaseKeyboard, 0, 0);
    }
    fn has_mouse_capture(&self) -> bool {
        self.q(EeUiComboQuery::HasMouseCapture, 0) != 0
    }
    fn mouse_location(&self) -> UiPoint {
        let (_, f) = self.qf(EeUiComboQuery::MouseLocation, 0);
        UiPoint { x: f[0], y: f[1] }
    }
    fn key(&self, key: ComboKey) -> bool {
        self.q(EeUiComboQuery::Key, key as isize) != 0
    }
    fn open_list(&mut self) {
        self.act(EeUiComboAct::OpenList, 0, 0);
    }
    fn close_list(&mut self) {
        self.act(EeUiComboAct::CloseList, 0, 0);
    }
    fn add_string(&mut self) -> c_long {
        self.act(EeUiComboAct::AddString, 0, 0) as c_long
    }
    fn change_selection(&mut self) {
        self.act(EeUiComboAct::ChangeSelection, 0, 0);
    }
    fn process_arrow(&mut self, down: bool) {
        self.act(EeUiComboAct::ProcessArrow, down as isize, 0);
    }
}

impl EeUiCombo {
    /// The constructor's end, after the C++ made the parts and set them up: `label` or `edit` the text box (the
    /// other 0), `selection` the first line's (0) or none (-1), `color` the list's selection colour for the focus
    /// highlight.
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        host: Host,
        label: isize,
        drop: isize,
        list: isize,
        edit: isize,
        text_box_type: c_ulong,
        max_list_box_height: c_ulong,
        selection: c_ulong,
        color: [f32; 3],
    ) -> Self {
        let rhombus = host.op(Op::RhombusCreate, 0, 0, 0, 0);
        let mut c = [color[0], color[1], color[2], 0.0];
        host.op_f(Op::RhombusSetColor, rhombus, 0, 0, 0, &mut c);
        let f = ComboFields {
            text_box: (if label != 0 { label } else { edit }) as *mut c_void,
            text_box_type,
            current_selection: selection,
            max_list_box_height,
            is_list_box_open: false,
            released_once: false,
            copied_list_box_open: false,
            list_box_always_dirty: false,
            copied_has_focus: false,
        };
        EeUiCombo { host, f: Cell::new(f), label, drop, list, edit, rhombus, key: Cell::new((0, 0)) }
    }

    fn text_box(&self) -> isize {
        self.f.get().text_box as isize
    }

    /// The destructor: the text box, the list and the drop button deleted (the original's order), then the focus
    /// highlight.
    fn release(&self) {
        self.host.op(Op::DeletePart, self.text_box(), 0, 0, 0);
        self.host.op(Op::DeletePart, self.list, 0, 0, 0);
        self.host.op(Op::DeletePart, self.drop, 0, 0, 0);
        self.host.op(Op::RhombusDelete, self.rhombus, 0, 0, 0);
    }

    fn draw_polygons(&self) -> isize {
        let op = |o: Op, a: isize| self.host.op(o, a, 0, 0, 0);
        op(Op::BaseDrawPolygons, 0);
        op(Op::PartDrawPolygons, self.text_box());
        if self.f.get().copied_has_focus {
            op(Op::RhombusDraw, self.rhombus);
        }
        let mut r = op(Op::PartDrawPolygons, self.drop);
        if self.f.get().copied_list_box_open {
            r = op(Op::PartDrawPolygons, self.list);
        }
        r
    }

    fn draw_text(&self) -> isize {
        let op = |o: Op, a: isize| self.host.op(o, a, 0, 0, 0);
        op(Op::DrawCountDecrement, 0);
        op(Op::PartDrawText, self.text_box());
        let mut r = op(Op::PartDrawText, self.drop);
        if self.f.get().copied_list_box_open {
            r = op(Op::PartDrawText, self.list);
        }
        r
    }

    /// One inward call ([`EeUiComboCall`]).
    ///
    /// # Safety
    /// `f` is NULL or four live floats; the byte arguments are NUL-terminated.
    pub unsafe fn call(&self, call: EeUiComboCall, a: isize, b: isize, f: *mut f32) -> isize {
        use EeUiComboCall as C;
        // SAFETY: the contract
        let fin = unsafe { read_buf(f) };
        let at = UiPoint { x: fin[0], y: fin[1] };
        let h = &mut Bridge(self);
        match call {
            C::AddString => combo::add_string(&Platform, h) as isize,
            C::Reopen => {
                combo::reopen(h);
                0
            }
            C::ChangeSelection => {
                combo::change_selection(h);
                0
            }
            C::ChangeWidth => {
                combo::change_width(h, a as c_ulong);
                0
            }
            C::CloseListBox => {
                combo::close_list_box(h);
                0
            }
            C::CopyData => combo::copy_data(h) as isize,
            C::DeleteAllEntries => {
                combo::delete_all_entries(h);
                0
            }
            C::DeleteLine => {
                combo::delete_line(h, a as c_ulong);
                0
            }
            C::DrawPolygons => self.draw_polygons(),
            C::DrawText => self.draw_text(),
            C::HideControl => {
                combo::hide_control(h);
                0
            }
            C::InsertLine => {
                // SAFETY: the contract
                let text = unsafe { bytes_at(a) };
                combo::insert_line(h, &text, b as c_ulong);
                0
            }
            C::MouseLButtonDown => combo::mouse_l_button_down(h) as isize,
            C::MouseLButtonUp => combo::mouse_l_button_up(h) as isize,
            C::MouseMove => combo::mouse_move(h, at) as isize,
            C::MouseWheelTurned => combo::mouse_wheel_turned(h, a as c_long) as isize,
            C::OpenListBox => {
                combo::open_list_box(h);
                0
            }
            C::ProcessArrow => {
                combo::process_arrow(h);
                0
            }
            C::SetCurrentSelection => {
                combo::set_current_selection(h, a as c_ulong);
                0
            }
            C::SetLocation => {
                combo::set_location(h, at);
                0
            }
            C::Tabbed => {
                if a != 0 {
                    combo::tabbed_to(h);
                } else {
                    combo::tabbed_from(h);
                }
                0
            }
            C::UpdateKeyboard => combo::update_keyboard(h) as isize,
            C::UpdateTranslatedKey => {
                let saved = self.key.replace((a, b));
                let r = combo::update_translated_key(h) as isize;
                self.key.set(saved);
                r
            }
            C::Selection => self.f.get().current_selection as isize,
            C::IsListBoxOpen => self.f.get().is_list_box_open as isize,
            C::TextBoxType => self.f.get().text_box_type as isize,
            C::Part => match a {
                0 => self.text_box(),
                1 => self.drop,
                2 => self.list,
                3 => self.edit,
                _ => self.label,
            },
            C::SetListBoxAlwaysDirty => {
                let mut v = self.f.get();
                v.list_box_always_dirty = a != 0;
                self.f.set(v);
                0
            }
            C::SetSelectedColor => {
                let mut c = [fin[0], fin[1], fin[2], 0.0];
                self.host.op_f(Op::RhombusSetColor, self.rhombus, 0, 0, 0, &mut c);
                // SAFETY: the contract
                unsafe { write_buf(f, fin) };
                0
            }
        }
    }
}

// ---- the C ABI

/// A new Rust combo box for the C++ `combo` (its parts made; see [`EeUiCombo::new`]).
///
/// # Safety
/// `host` answers the ops for `combo`; `color` is three floats.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_combo_new(
    combo: *mut c_void,
    host: EeUiHostFn,
    label: *mut c_void,
    drop: *mut c_void,
    list: *mut c_void,
    edit: *mut c_void,
    text_box_type: c_ulong,
    max_list_box_height: c_ulong,
    selection: c_ulong,
    color: *const f32,
) -> *mut EeUiCombo {
    // SAFETY: the contract
    let c = unsafe { [*color, *color.add(1), *color.add(2)] };
    crate::direct::made(
        crate::direct::Kind::Combo,
        combo,
        Box::into_raw(Box::new(EeUiCombo::new(
            Host { obj: combo, f: host },
            label as isize,
            drop as isize,
            list as isize,
            edit as isize,
            text_box_type,
            max_list_box_height,
            selection,
            c,
        ))),
    )
}

/// The destructor: deletes the parts and the focus highlight and frees the object.
///
/// # Safety
/// `combo` came from `ee_ui_combo_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_free(combo: *mut EeUiCombo) {
    crate::direct::forget(crate::direct::Kind::Combo, combo as *const core::ffi::c_void);
    if combo.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*combo).release() };
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(combo) });
}

/// One of UIComboBox's methods ([`EeUiComboCall`]).
///
/// # Safety
/// `combo` is live; see [`EeUiCombo::call`].
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_call(
    combo: *const EeUiCombo,
    call: EeUiComboCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*combo).call(call, a, b, f) }
}

/// The constructor's drop button ([`combo::drop_rect`]).
#[no_mangle]
pub extern "C" fn ee_ui_combo_drop_rect(control: UiRect, given: UiRect) -> UiRect {
    combo::drop_rect(&control, &given)
}

/// The constructor's text box ([`combo::text_rect`]).
#[no_mangle]
pub extern "C" fn ee_ui_combo_text_rect(control: UiRect, given: UiRect, drop_width: c_ulong) -> UiRect {
    combo::text_rect(&control, &given, drop_width)
}

/// The constructor's list ([`combo::list_rect`]).
#[no_mangle]
pub extern "C" fn ee_ui_combo_list_rect(control: UiRect, text_bottom: f32, max_height: c_ulong) -> UiRect {
    combo::list_rect(&control, text_bottom, max_height)
}

#[cfg(test)]
#[path = "comboobj_tests.rs"]
mod tests;
