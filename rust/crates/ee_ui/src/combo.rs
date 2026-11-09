//! `UIComboBox` (`User Interface/UIComboBox.cpp`): a text box (a text label, type 1, or an edit box) with a drop
//! button and a drop-down list (`UILBString`). The list opens below the text box, or above it when there is more
//! room; while open the combo box is the form's popup and holds the mouse and the keyboard.
//!
//! The C++ object keeps its members ([`ComboFields`] is its record from `mTextBox` on) and its parts. Everything the
//! parts do is a host call ([`ComboHost`]): their handlers, rectangles, texts and the list's data. Every reading of
//! text goes through [`crate::text`]. The record is written before and read after every host call with side effects
//! (a fired trigger can reach the same combo box).

use core::ffi::{c_long, c_ulong, c_void};

use crate::rect::{um_floor, UiPoint, UiRect};
use crate::text::Mbcs;

/// `kErrorOK`.
const OK: c_long = 0;
/// `kErrorNotFound`.
const NOT_FOUND: c_long = 3;
/// `kErrorFailed`.
const FAILED: c_long = 9;
/// `kControlTriggerSelectionChanged`.
const TRIGGER_SELECTION_CHANGED: c_ulong = 0x3eb;
/// `kControlTriggerListOpened`.
const TRIGGER_LIST_OPENED: c_ulong = 0x3ed;
/// `kControlTriggerListClosed`.
const TRIGGER_LIST_CLOSED: c_ulong = 0x3ee;
/// `kScrollBarWidth`.
const SCROLL_BAR_WIDTH: f32 = 20.0;
/// No selection (`mCurrentSelection = -1`).
const NONE: c_ulong = c_ulong::MAX;

/// `UIComboBox`'s members in their C++ order (from `mTextBox` on). The pointer is C++'s.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ComboFields {
    pub text_box: *mut c_void,
    pub text_box_type: c_ulong,
    pub current_selection: c_ulong,
    pub max_list_box_height: c_ulong,
    pub is_list_box_open: bool,
    pub released_once: bool,
    pub copied_list_box_open: bool,
    pub list_box_always_dirty: bool,
    pub copied_has_focus: bool,
}

/// The record and the `UIControl` members the combo box reads.
#[derive(Clone, Copy, Debug)]
pub struct ComboState {
    pub f: ComboFields,
    /// `mRectangle`.
    pub rect: UiRect,
    /// `mIndex`.
    pub index: c_ulong,
}

/// A part of the combo box.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(C)]
pub enum ComboPart {
    /// `mTextBox`: the label or the edit box.
    TextBox = 0,
    /// `mDropButton`.
    Drop = 1,
    /// `mListBox`.
    List = 2,
    /// `mEditBox` (only with one).
    Edit = 3,
    /// `mTextLabel` (only with one).
    Label = 4,
}

/// The keys `UpdateKeyboard` asks about.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(C)]
pub enum ComboKey {
    /// `UIControl::DownKeyPressed()`.
    Down = 0,
    /// `UIControl::UpKeyPressed()`.
    Up = 1,
    /// `UIControl::HomeKeyPressed()`.
    Home = 2,
    /// `UIControl::EndKeyPressed()`.
    End = 3,
    /// `sEnterKey->GetValue() != 0`.
    Enter = 4,
}

/// The combo box's way back into its C++ object.
pub trait ComboHost {
    /// Reads the record.
    fn load(&self) -> ComboState;
    /// Writes the record.
    fn store(&mut self, f: &ComboFields);
    /// `mTextLabel != NULL` (else the edit box).
    fn has_label(&self) -> bool;
    /// A part's `GetRectangle()`.
    fn rect_of(&self, part: ComboPart) -> UiRect;
    /// The text box's `GetBorderThickness()`.
    fn text_box_border(&self) -> c_ulong;
    /// The list's `GetLineCount()`.
    fn line_count(&self) -> c_ulong;
    /// The list's line `i` (NUL-terminated).
    fn line(&self, i: c_ulong) -> Vec<u8>;
    /// The list's `mLineHeight`.
    fn line_height(&self) -> c_ulong;
    /// The list's `GetBorderThickness()`.
    fn list_border(&self) -> c_ulong;
    /// The list's scroll bar's rectangle, if it has one.
    fn list_scroll_bar_rect(&self) -> Option<UiRect>;
    /// The list's `GetScrollBarCaptured()`.
    fn list_scroll_bar_captured(&self) -> bool;
    /// The list's `GetCurrentSelection(out)`: `out` is always written.
    fn list_selection(&self, out: &mut c_ulong) -> c_long;
    /// The list's virtual `ChangeSelectionIndex(i, fire, scroll)`.
    fn list_change_selection(&mut self, i: c_ulong, fire: bool, scroll: bool);
    /// The list's `InsertLine(text, i)`.
    fn list_insert_line(&mut self, text: &[u8], i: c_ulong);
    /// The list's virtual `DeleteLine(i)`.
    fn list_delete_line(&mut self, i: c_ulong);
    /// The list's virtual `DeleteAllEntries()`.
    fn list_delete_all(&mut self);
    /// The list's `ChangeWidth(w)`.
    fn list_change_width(&mut self, w: c_ulong);
    /// The window's `GetHeight()` (the rasterizer's window for the control's screen).
    fn window_height(&self) -> c_ulong;
    /// The edit box's `GetText(0)` (NUL-terminated).
    fn edit_text(&self) -> Vec<u8>;
    /// Shows list line `i` in the text box: the label's `SetText(line, true)`, or the edit box's `SetText(line)`.
    fn show_line(&mut self, i: c_ulong);
    /// Empties the text box: the label's `SetText("", true)` or the edit box's `SetText("")`.
    fn clear_text(&mut self);
    /// `ProcessDown`'s empty text box: the label's `DeleteLine(0)` or the edit box's `DeleteText()`.
    fn delete_text(&mut self);
    /// A part's virtual `MouseLButtonDown()`.
    fn part_mouse_down(&mut self, part: ComboPart) -> c_long;
    /// A part's virtual `MouseLButtonUp()`.
    fn part_mouse_up(&mut self, part: ComboPart) -> c_long;
    /// A part's virtual `MouseMove(at)`.
    fn part_mouse_move(&mut self, part: ComboPart, at: UiPoint) -> c_long;
    /// The list's virtual `MouseWheelTurned(delta)`.
    fn list_wheel(&mut self, delta: c_long) -> c_long;
    /// A part's virtual `UpdateKeyboard()`.
    fn part_update_keyboard(&mut self, part: ComboPart) -> c_long;
    /// A part's virtual `UpdateTranslatedKey(key, repeat)` (the key is the C++ caller's).
    fn part_translated_key(&mut self, part: ComboPart) -> c_long;
    /// A part's virtual `TabbedTo()` / `TabbedFrom()`.
    fn part_tabbed(&mut self, part: ComboPart, to: bool);
    /// A part's virtual `SetRectangle(r)`.
    fn part_set_rectangle(&mut self, part: ComboPart, r: UiRect);
    /// A part's virtual `SetLocation(at)`.
    fn part_set_location(&mut self, part: ComboPart, at: UiPoint);
    /// A part's virtual `CopyData()`.
    fn part_copy_data(&mut self, part: ComboPart) -> c_long;
    /// `CopyData`'s open list: the list's text takes the text box's colours and transparency.
    fn copy_list_colours(&mut self);
    /// `mSelectedBackground.SetRectangle(r)`.
    fn set_focus_rect(&mut self, r: UiRect);
    /// `UIControl::CopyData()`.
    fn base_copy_data(&mut self) -> c_long;
    /// The form's `GetTabbingOn()`.
    fn tabbing_on(&self) -> bool;
    /// The form's `GetTabStop()`.
    fn tab_stop(&self) -> c_ulong;
    /// The form's `SetPopup(this)` or `SetPopup(NULL)`.
    fn set_popup(&mut self, this: bool);
    /// The virtual `SetDirty(true)`.
    fn set_dirty(&mut self);
    /// The virtual `SetRectangle(r)` (this combo box's).
    fn set_rectangle(&mut self, r: UiRect);
    /// `UIControl::SetRectangle(r)`.
    fn base_set_rectangle(&mut self, r: UiRect);
    /// `UIControl::SetLocation(at)`.
    fn base_set_location(&mut self, at: UiPoint);
    /// `SetAlwaysDirty(on)`.
    fn set_always_dirty(&mut self, on: bool);
    /// `FireTriggerEvent(event)`.
    fn fire(&mut self, event: c_ulong);
    /// `CaptureMouse()`.
    fn capture_mouse(&mut self);
    /// `CaptureKeyboard()`.
    fn capture_keyboard(&mut self);
    /// The virtual `ReleaseCapturedMouse()`.
    fn release_mouse(&mut self);
    /// The virtual `ReleaseCapturedKeyboard()`.
    fn release_keyboard(&mut self);
    /// `GetHasMouseCapture()`.
    fn has_mouse_capture(&self) -> bool;
    /// `GetMouseLocation()`.
    fn mouse_location(&self) -> UiPoint;
    /// A key's state.
    fn key(&self, key: ComboKey) -> bool;
    /// The virtual `OpenListBox()`.
    fn open_list(&mut self);
    /// The virtual `CloseListBox()`.
    fn close_list(&mut self);
    /// The virtual `AddString()`.
    fn add_string(&mut self) -> c_long;
    /// The virtual `ChangeSelection()`.
    fn change_selection(&mut self);
    /// The virtual `ProcessDown()` / `ProcessUp()`.
    fn process_arrow(&mut self, down: bool);
}

/// The record across host calls.
struct Combo<'a, H: ComboHost> {
    h: &'a mut H,
    s: ComboState,
}

impl<'a, H: ComboHost> Combo<'a, H> {
    fn new(h: &'a mut H) -> Self {
        let s = h.load();
        Combo { h, s }
    }
    fn call<R>(&mut self, g: impl FnOnce(&mut H) -> R) -> R {
        self.h.store(&self.s.f);
        let r = g(self.h);
        self.s = self.h.load();
        r
    }
    fn finish(self) {
        self.h.store(&self.s.f);
    }
    /// `mListBox->GetCurrentSelection(mCurrentSelection)`.
    fn take_list_selection(&mut self) -> c_long {
        let mut sel = self.s.f.current_selection;
        let r = self.h.list_selection(&mut sel);
        self.s.f.current_selection = sel;
        r
    }
    /// The open list closes and reopens, so that it resizes (after its lines changed).
    fn reopen(&mut self) {
        if self.s.f.is_list_box_open {
            self.call(|h| {
                h.close_list();
                h.open_list();
            });
        }
    }
}

/// The constructor's drop button: as wide as the control is high, at its right end.
pub fn drop_rect(control: &UiRect, given: &UiRect) -> UiRect {
    UiRect { top: given.top, left: control.right - given.height(), right: given.right, bottom: given.bottom }
}

/// The constructor's text box: from the top-left, as wide as the control less the drop button plus one pixel, as
/// high as the given rectangle (`GERectangle(point, width, height)`: inclusive edges).
pub fn text_rect(control: &UiRect, given: &UiRect, drop_width: c_ulong) -> UiRect {
    let width = control.width() - drop_width as f32 + 1.0;
    let left = given.left;
    let top = given.top;
    UiRect::new(left, top, left + (width - 1.0), top + (given.height() - 1.0))
}

/// The constructor's list: below the text box, as high as the most the list may be.
pub fn list_rect(control: &UiRect, text_bottom: f32, max_height: c_ulong) -> UiRect {
    UiRect::new(control.left, text_bottom, control.right, text_bottom + max_height as f32)
}

/// `UIComboBox::AddString`: selects the edit box's text in the list (OK), or adds it at the top and selects it (not
/// found); empty text fails.
pub fn add_string(m: &impl Mbcs, h: &mut impl ComboHost) -> c_long {
    let text = h.edit_text();
    let mut i = h.line_count() as c_long;
    if crate::text::is_empty(&text) {
        return FAILED;
    }
    let mut found = false;
    i -= 1;
    while i >= 0 {
        if crate::text::equal(m, &h.line(i as c_ulong), &text) {
            found = true;
            break;
        }
        i -= 1;
    }
    let result;
    if found {
        result = OK;
        h.list_change_selection(i as c_ulong, true, false);
    } else {
        result = NOT_FOUND;
        insert_line(h, &text, 0);
        h.list_change_selection(0, true, false);
    }
    h.set_dirty();
    result
}

/// `UIComboBox::InsertLine`: a list line at an index; an open list reopens to resize.
pub fn insert_line(h: &mut impl ComboHost, text: &[u8], i: c_ulong) {
    h.set_dirty();
    h.list_insert_line(text, i);
    Combo::new(h).reopen();
}

/// After `AppendLine` / `AppendStringArray` changed the list (the C++ dirtied the box and appended): an open list
/// reopens to resize.
pub fn reopen(h: &mut impl ComboHost) {
    Combo::new(h).reopen();
}

/// `UIComboBox::ChangeSelection`: the selected line in the text box, then the selection-changed trigger.
pub fn change_selection(h: &mut impl ComboHost) {
    let sel = h.load().f.current_selection;
    h.show_line(sel);
    h.fire(TRIGGER_SELECTION_CHANGED);
}

/// `UIComboBox::ChangeWidth`: the text box less the scroll bar's width, the list, then the control.
pub fn change_width(h: &mut impl ComboHost, width: c_ulong) {
    let mut r = h.rect_of(ComboPart::TextBox);
    let w = width as f32;
    r.right = r.left + w - SCROLL_BAR_WIDTH;
    h.part_set_rectangle(ComboPart::TextBox, r);
    h.list_change_width(width);
    r.right = r.left + w;
    h.base_set_rectangle(r);
}

/// `UIComboBox::CloseListBox`: the control shrinks back to the text box and the drop button.
pub fn close_list_box(h: &mut impl ComboHost) {
    let mut c = Combo::new(h);
    // only without an edit box (`mEditBox == NULL`): an edit box keeps the keyboard
    if c.h.has_label() {
        c.call(|h| h.release_keyboard());
    }
    c.s.f.is_list_box_open = false;
    c.call(|h| {
        h.set_dirty();
        h.set_popup(false);
    });
    let tb = c.h.rect_of(ComboPart::TextBox);
    let drop = c.h.rect_of(ComboPart::Drop);
    let r = UiRect { top: tb.top, left: tb.left, right: drop.right, bottom: tb.bottom };
    c.call(|h| h.set_rectangle(r));
    if c.s.f.list_box_always_dirty {
        c.call(|h| h.set_always_dirty(false));
    }
    c.call(|h| h.fire(TRIGGER_LIST_CLOSED));
    c.finish();
}

/// `UIComboBox::CopyData`: the parts, then either the open list in the text box's colours or, tabbed to, the focus
/// highlight inside the text box's border.
pub fn copy_data(h: &mut impl ComboHost) -> c_long {
    let mut c = Combo::new(h);
    c.s.f.copied_list_box_open = false;
    c.call(|h| {
        h.part_copy_data(ComboPart::TextBox);
        h.part_copy_data(ComboPart::Drop);
    });
    if c.s.f.is_list_box_open {
        c.s.f.copied_list_box_open = true;
        c.call(|h| {
            h.copy_list_colours();
            h.part_copy_data(ComboPart::List);
        });
    } else if c.h.tabbing_on() && c.h.tab_stop() == c.s.index {
        let tb = c.h.rect_of(ComboPart::TextBox);
        let b = c.h.text_box_border() as f32;
        let r = UiRect { left: tb.left + b, top: tb.top + b, right: tb.right - b, bottom: tb.bottom - b };
        c.call(|h| h.set_focus_rect(r));
        c.s.f.copied_has_focus = true;
    } else {
        c.s.f.copied_has_focus = false;
    }
    let r = c.call(|h| h.base_copy_data());
    c.finish();
    r
}

/// `UIComboBox::DeleteAllEntries`: no lines, no text, no selection.
pub fn delete_all_entries(h: &mut impl ComboHost) {
    h.list_delete_all();
    h.clear_text();
    h.set_dirty();
    let mut c = Combo::new(h);
    c.reopen();
    c.s.f.current_selection = NONE;
    c.finish();
}

/// `UIComboBox::DeleteLine`: shows the line now selected, or clears the text and the selection.
pub fn delete_line(h: &mut impl ComboHost, line: c_ulong) {
    h.list_delete_line(line);
    let mut sel = line;
    if h.list_selection(&mut sel) == OK {
        h.show_line(sel);
    } else {
        let mut s = h.load().f;
        s.current_selection = NONE;
        h.store(&s);
        h.clear_text();
    }
    h.set_dirty();
    Combo::new(h).reopen();
}

/// `UIComboBox::HideControl`: an open list closes and gives up the mouse and the keyboard.
pub fn hide_control(h: &mut impl ComboHost) {
    if !h.load().f.is_list_box_open {
        return;
    }
    h.close_list();
    h.release_mouse();
    h.release_keyboard();
}

/// `UIComboBox::MouseLButtonDown`: opens the list (or, on an edit box's text, starts editing), or picks in / closes
/// the open list.
pub fn mouse_l_button_down(h: &mut impl ComboHost) -> c_long {
    let mut c = Combo::new(h);
    let mut r = OK;
    if !c.s.f.is_list_box_open && !c.h.has_mouse_capture() {
        if c.h.has_label() {
            c.call(|h| h.part_mouse_down(ComboPart::Drop));
            r = c.call(|h| h.part_mouse_down(ComboPart::Label));
            c.take_list_selection();
            c.call(|h| h.open_list());
        } else if c.h.rect_of(ComboPart::TextBox).contains(c.h.mouse_location()) {
            r = c.call(|h| h.part_mouse_down(ComboPart::Edit));
            c.call(|h| h.capture_mouse());
        } else {
            r = c.call(|h| h.part_mouse_down(ComboPart::Drop));
            c.take_list_selection();
            c.call(|h| h.open_list());
        }
    } else {
        if c.h.rect_of(ComboPart::List).contains(c.h.mouse_location()) {
            r = c.call(|h| h.part_mouse_down(ComboPart::List));
            if !c.h.list_scroll_bar_captured() {
                c.take_list_selection();
            }
        } else {
            c.call(|h| h.close_list());
        }
        c.call(|h| h.capture_mouse());
    }
    c.finish();
    r
}

/// `UIComboBox::MouseLButtonUp`: a release on a line (or the second release anywhere) commits the line and closes the
/// list; the first release after opening keeps it open.
pub fn mouse_l_button_up(h: &mut impl ComboHost) -> c_long {
    let mut c = Combo::new(h);
    let mut list = c.h.rect_of(ComboPart::List);
    if c.s.f.is_list_box_open {
        if !c.h.list_scroll_bar_captured() {
            if let Some(bar) = c.h.list_scroll_bar_rect() {
                list.right = bar.left;
            }
            let commit = if list.contains(c.h.mouse_location()) {
                true
            } else {
                c.s.f.released_once && c.s.f.current_selection < c.h.line_count()
            };
            if commit {
                if c.take_list_selection() == OK {
                    c.call(|h| h.change_selection());
                }
                c.call(|h| {
                    h.close_list();
                    h.release_mouse();
                });
            } else {
                c.s.f.released_once = true;
            }
        }
        c.call(|h| h.part_mouse_up(ComboPart::List));
    } else {
        if c.s.f.current_selection < c.h.line_count() {
            let sel = c.s.f.current_selection;
            c.call(|h| h.list_change_selection(sel, false, false));
        }
        c.call(|h| h.release_mouse());
    }
    c.call(|h| h.part_mouse_up(ComboPart::TextBox));
    let r = c.call(|h| h.part_mouse_up(ComboPart::Drop));
    c.call(|h| h.set_dirty());
    c.finish();
    r
}

/// `UIComboBox::MouseMove`: the text box, the drop button and the open list.
pub fn mouse_move(h: &mut impl ComboHost, at: UiPoint) -> c_long {
    h.part_mouse_move(ComboPart::TextBox, at);
    let mut r = h.part_mouse_move(ComboPart::Drop, at);
    if h.load().f.is_list_box_open {
        r = h.part_mouse_move(ComboPart::List, at);
    }
    r
}

/// `UIComboBox::MouseWheelTurned`: scrolls the open list; fails while it is closed.
pub fn mouse_wheel_turned(h: &mut impl ComboHost, delta: c_long) -> c_long {
    if h.load().f.is_list_box_open {
        h.list_wheel(delta)
    } else {
        FAILED
    }
}

/// `UIComboBox::OpenListBox`'s rectangles: the list's and the control's. Below the text box when the list fits there
/// or there is more room below, else above it; never taller than the room on that side.
pub fn open_rects(
    tb: &UiRect,
    drop: &UiRect,
    lines: c_ulong,
    line_height: c_ulong,
    border: c_ulong,
    max_height: c_ulong,
    window_height: c_ulong,
) -> (UiRect, UiRect) {
    let mut height = lines.wrapping_mul(line_height).wrapping_add(border.wrapping_mul(2)).wrapping_add(1) as c_long;
    if height > max_height as c_long {
        height = max_height as c_long;
    }
    let mut list = UiRect::default();
    let mut rect = UiRect::default();
    list.left = tb.left;
    list.right = drop.right;
    rect.left = list.left;
    rect.right = list.right;
    let below = (window_height as c_long).wrapping_sub(um_floor(tb.bottom)).wrapping_sub(1);
    let above = um_floor(tb.top).wrapping_sub(1);
    if height > below && below < above {
        if above < height {
            height = above;
        }
        list.bottom = tb.top - 1.0;
        list.top = list.bottom - height as f32;
        rect.top = list.top;
        rect.bottom = tb.bottom;
    } else {
        if height > below {
            height = below;
        }
        list.top = tb.bottom + 1.0;
        list.bottom = list.top + height as f32;
        rect.top = tb.top;
        rect.bottom = list.bottom;
    }
    (list, rect)
}

/// `UIComboBox::OpenListBox`: lays the list out ([`open_rects`]), becomes the form's popup, captures the mouse and
/// the keyboard and fires the list-opened trigger.
pub fn open_list_box(h: &mut impl ComboHost) {
    let mut c = Combo::new(h);
    let window = c.h.window_height();
    let (list, rect) = open_rects(
        &c.h.rect_of(ComboPart::TextBox),
        &c.h.rect_of(ComboPart::Drop),
        c.h.line_count(),
        c.h.line_height(),
        c.h.list_border(),
        c.s.f.max_list_box_height,
        window,
    );
    c.s.f.released_once = false;
    c.s.f.is_list_box_open = true;
    c.call(|h| {
        h.part_set_rectangle(ComboPart::List, list);
        h.set_rectangle(rect);
        h.set_popup(true);
        h.capture_mouse();
        h.capture_keyboard();
        h.set_dirty();
    });
    if c.s.f.list_box_always_dirty {
        c.call(|h| h.set_always_dirty(true));
    }
    c.call(|h| h.fire(TRIGGER_LIST_OPENED));
    c.finish();
}

/// `UIComboBox::ProcessDown` / `ProcessUp`: the list's selection in the text box after an arrow key, or no text.
pub fn process_arrow(h: &mut impl ComboHost) {
    let mut c = Combo::new(h);
    if c.take_list_selection() == OK {
        let sel = c.s.f.current_selection;
        c.call(|h| h.show_line(sel));
    } else {
        c.call(|h| h.delete_text());
    }
    c.finish();
}

/// `UIComboBox::SetCurrentSelection`: selects and shows a line; an index past the lines is ignored.
pub fn set_current_selection(h: &mut impl ComboHost, i: c_ulong) {
    let mut sel: c_ulong = 0;
    h.list_selection(&mut sel);
    if i >= h.line_count() {
        return;
    }
    if i != sel {
        h.list_change_selection(i, true, false);
    }
    let mut f = h.load().f;
    f.current_selection = i;
    h.store(&f);
    change_selection(h);
}

/// `UIComboBox::SetLocation`: the parts move with the control; a list open above the text box stays above it.
pub fn set_location(h: &mut impl ComboHost, at: UiPoint) {
    let list = h.rect_of(ComboPart::List);
    if h.load().f.is_list_box_open && list.top < h.rect_of(ComboPart::TextBox).top {
        h.part_set_location(ComboPart::List, at);
        let y = h.rect_of(ComboPart::List).bottom + 1.0;
        h.part_set_location(ComboPart::TextBox, UiPoint { x: at.x, y });
        let tb = h.rect_of(ComboPart::TextBox);
        h.part_set_location(ComboPart::Drop, UiPoint { x: tb.right + 1.0, y: tb.top });
    } else {
        h.part_set_location(ComboPart::TextBox, at);
        let x = h.rect_of(ComboPart::TextBox).right + 1.0;
        h.part_set_location(ComboPart::Drop, UiPoint { x, y: at.y });
        let tb = h.rect_of(ComboPart::TextBox);
        h.part_set_location(ComboPart::List, UiPoint { x: tb.left, y: tb.bottom + 1.0 });
    }
    h.base_set_location(at);
}

/// `UIComboBox::TabbedTo`: the keyboard focus (the edit box's too).
pub fn tabbed_to(h: &mut impl ComboHost) {
    h.set_dirty();
    if !h.has_label() {
        h.part_tabbed(ComboPart::Edit, true);
        h.capture_keyboard();
    } else if h.tabbing_on() {
        h.capture_keyboard();
    }
}

/// `UIComboBox::TabbedFrom`: closes the list and gives up the mouse and the keyboard.
pub fn tabbed_from(h: &mut impl ComboHost) {
    if h.load().f.is_list_box_open {
        h.close_list();
    } else {
        h.set_dirty();
    }
    h.release_mouse();
    if !h.has_label() {
        h.part_tabbed(ComboPart::Edit, false);
    }
    h.release_keyboard();
}

/// `UIComboBox::UpdateKeyboard`: Down opens the list; in the open list Enter commits (or adds the edit box's text)
/// and the arrows move; the edit box gets the other keys.
pub fn update_keyboard(h: &mut impl ComboHost) -> c_long {
    let mut r = OK;
    if !h.load().f.is_list_box_open {
        if h.key(ComboKey::Down) {
            h.open_list();
        } else if !h.has_label() {
            if h.key(ComboKey::Enter) {
                h.add_string();
            }
            r = h.part_update_keyboard(ComboPart::Edit);
        }
    } else {
        r = h.part_update_keyboard(ComboPart::List);
        if h.key(ComboKey::Enter) {
            if !h.has_label() {
                h.add_string();
            } else {
                let mut c = Combo::new(h);
                if c.take_list_selection() == OK {
                    c.call(|h| h.change_selection());
                }
                c.call(|h| {
                    h.close_list();
                    h.release_mouse();
                });
                c.finish();
            }
        }
        if h.key(ComboKey::Down) {
            h.process_arrow(true);
        }
        if h.key(ComboKey::Up) {
            h.process_arrow(false);
        }
        if !h.has_label()
            && !h.key(ComboKey::Down)
            && !h.key(ComboKey::Up)
            && !h.key(ComboKey::Home)
            && !h.key(ComboKey::End)
        {
            r = h.part_update_keyboard(ComboPart::Edit);
        }
    }
    r
}

/// `UIComboBox::UpdateTranslatedKey`: a typed character goes to the open list, else to the text box.
pub fn update_translated_key(h: &mut impl ComboHost) -> c_long {
    let part = if h.load().f.is_list_box_open { ComboPart::List } else { ComboPart::TextBox };
    h.part_translated_key(part)
}

#[cfg(test)]
mod tests;
