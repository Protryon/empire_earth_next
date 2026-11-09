//! `UIListBox<T>`'s selection and scrolling (`lle/UIListBox.h`, a template: the exported `UIListBox<UWideString>` is
//! emitted by `UIComboBox.cpp`, the game instantiates its own). The C++ template keeps its members and its data
//! (`std::vector<T>`, whatever `T` is); its members forward here with the scalar members from `mScrollBarBgOpacity` on
//! as the pinned record [`ListFields`] and reach the object back through a table of host functions instantiated per
//! `T` ([`ListHost`]). The virtual members the algorithms call (`GetNumLines`, `ChangeSelectionIndex`,
//! `ClearSelection`) stay virtual: a subclass's override runs.

use core::ffi::{c_long, c_ulong};

use crate::rect::{um_floor, UiPoint, UiRect};

/// `kControlTriggerSelectionChanged`, `kControlTriggerDoubleClicked`, `kControlTriggerButtonDown`.
pub const TRIGGER_SELECTION_CHANGED: c_ulong = 0x3eb;
pub const TRIGGER_DOUBLE_CLICKED: c_ulong = 0x3ec;
pub const TRIGGER_BUTTON_DOWN: c_ulong = 0x3ef;
/// `kScrollBarWidth`.
pub const SCROLL_BAR_WIDTH: f32 = 20.0;

/// `UIListBox`'s members from `mScrollBarBgOpacity` to `mHasSelection`, in order (offsets `static_assert`ed in
/// `lle/UIListBox.h`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ListFields {
    pub scroll_bar_bg_opacity: f32,
    pub number_lines_displayed: c_ulong,
    pub current_selection: c_ulong,
    pub draw_line_count: c_ulong,
    pub line_height: c_ulong,
    pub auto_select: bool,
    pub button_down: bool,
    pub scroll_bar_captured: bool,
    pub copied_has_focus: bool,
    pub copied_has_selection: bool,
    pub scroll_bar_removed: bool,
    pub copied_scroll_bar_visible: bool,
    pub has_selection: bool,
}

/// What the algorithms read of the C++ object: the record, `mTextRectangle`, `mRectangle` and `mBorderThickness`.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ListState {
    pub f: ListFields,
    pub text: UiRect,
    pub rect: UiRect,
    pub border: c_ulong,
}

/// The C++ side of a list box.
pub trait ListHost {
    /// The members as they are now.
    fn load(&self) -> ListState;
    /// Writes the record and `mTextRectangle` back.
    fn store(&mut self, s: &ListState);
    /// The virtual `GetNumLines`.
    fn num_lines(&self) -> c_ulong;
    /// The virtual `ChangeSelectionIndex`.
    fn change_selection(&mut self, index: c_ulong, fire: bool, scroll_to: bool);
    /// The virtual `ClearSelection`.
    fn clear_selection(&mut self);
    /// `mScrollBar != NULL`.
    fn has_scroll_bar(&self) -> bool;
    /// The scroll bar slider's `GetCurrentRangeValue`.
    fn scroll_value(&self) -> c_long;
    /// The scroll bar slider's `GetMaxRange`.
    fn scroll_max(&self) -> c_long;
    /// `mScrollBar->SetCurrentRangeValue`.
    fn set_scroll_value(&mut self, value: c_ulong);
    /// `mScrollBar->SetRange`.
    fn set_scroll_range(&mut self, min: c_ulong, max: c_ulong);
    /// `mScrollBar->SetRectangle`.
    fn scroll_set_rectangle(&mut self, r: &UiRect);
    /// `mScrollBar = new UIScrollBar(.., at, length, ..)`, then its page size, value, colour and opacity.
    fn create_scroll_bar(&mut self, at: UiPoint, length: f32, page: c_ulong, value: c_ulong);
    /// `SetDirty(true)`.
    fn set_dirty(&mut self);
    /// `FireTriggerEvent`.
    fn fire(&mut self, event: c_ulong);
    /// `SetBgRectangle`.
    fn set_bg_rectangle(&mut self, r: &UiRect);
    /// `UIControl::SetLocation`.
    fn base_set_location(&mut self, at: UiPoint);
    /// `GetMouseLocation`.
    fn mouse_location(&self) -> UiPoint;
}

/// One call into a list box: the members as of its start, written back by [`List::finish`].
pub struct List<'a, H: ListHost> {
    pub h: &'a mut H,
    pub s: ListState,
}

impl<'a, H: ListHost> List<'a, H> {
    pub fn new(h: &'a mut H) -> Self {
        let s = h.load();
        List { h, s }
    }

    /// Writes the members back.
    pub fn finish(self) {
        self.h.store(&self.s);
    }

    /// A host call with side effects (a virtual one may come back into this module for the same box).
    fn call<R>(&mut self, f: impl FnOnce(&mut H) -> R) -> R {
        self.h.store(&self.s);
        let r = f(self.h);
        self.s = self.h.load();
        r
    }

    fn set_dirty(&mut self) {
        self.call(|h| h.set_dirty());
    }

    fn first_line(&self) -> c_long {
        if self.h.has_scroll_bar() {
            self.h.scroll_value()
        } else {
            0
        }
    }

    /// Keeps the selection on screen (`ChangeSelectionIndex`'s and the arrow keys' scroll; `ProcessDown` compared
    /// the selection above the first line signed).
    fn scroll_to_selection(&mut self, signed: bool) {
        let value = self.h.scroll_value();
        let (selection, shown) = (self.s.f.current_selection, self.s.f.number_lines_displayed);
        let above = if signed { (selection as c_long) < value } else { selection < value as c_ulong };
        if above {
            self.call(|h| h.set_scroll_value(selection));
        } else if selection >= (value as c_ulong).wrapping_add(shown) {
            self.call(|h| h.set_scroll_value(selection.wrapping_sub(shown).wrapping_add(1)));
        }
    }

    /// `ChangeSelectionIndex` (the base class's body).
    pub fn change_selection_index(&mut self, index: c_ulong, fire: bool, scroll_to: bool) {
        let mut index = index;
        let count = self.h.num_lines();
        if count > 0 && index >= self.h.num_lines() {
            index = count - 1;
        }
        if !self.s.f.has_selection || self.s.f.current_selection != index {
            self.s.f.current_selection = index;
            self.set_dirty();
            if fire {
                self.call(|h| h.fire(TRIGGER_SELECTION_CHANGED));
            }
            self.s.f.has_selection = true;
        }
        if scroll_to && self.h.has_scroll_bar() {
            self.check_scroll_bar();
            self.scroll_to_selection(false);
        }
    }

    /// `CheckScrollBar`: a scroll bar while the lines do not fit; one no longer needed is marked for the next
    /// `CopyData` to delete.
    pub fn check_scroll_bar(&mut self) {
        let lines = self.h.num_lines();
        let shown = self.s.f.number_lines_displayed;
        if self.h.has_scroll_bar() && !self.s.f.scroll_bar_removed {
            if lines <= shown {
                self.s.f.scroll_bar_removed = true;
                self.set_dirty();
                self.s.text.right = self.s.rect.right - self.s.border as f32;
                let text = self.s.text;
                self.call(|h| h.set_bg_rectangle(&text));
            } else {
                self.call(|h| h.set_scroll_range(0, lines.wrapping_sub(shown)));
            }
        } else if lines > shown {
            if !self.h.has_scroll_bar() {
                let at = UiPoint { x: self.s.rect.right - SCROLL_BAR_WIDTH, y: self.s.text.top };
                let length = self.s.text.bottom - self.s.text.top;
                let page = if shown > 1 { shown - 1 } else { shown };
                let value = self.s.f.current_selection;
                self.call(|h| h.create_scroll_bar(at, length, page, value));
            }
            self.s.text.right = self.s.rect.right - SCROLL_BAR_WIDTH;
            let range = self.h.num_lines().wrapping_sub(self.s.f.number_lines_displayed);
            self.call(|h| h.set_scroll_range(0, range));
            let text = self.s.text;
            self.call(|h| h.set_bg_rectangle(&text));
            self.s.f.scroll_bar_removed = false;
        }
    }

    /// `DeleteAllEntries` after the data is cleared.
    pub fn cleared(&mut self) {
        self.s.f.current_selection = 0;
        self.call(|h| h.clear_selection());
        self.set_dirty();
        self.check_scroll_bar();
    }

    /// `DeleteLine` after the line is erased.
    pub fn deleted_line(&mut self, line: c_ulong) {
        self.set_dirty();
        let f = &self.s.f;
        if f.has_selection
            && (f.current_selection > line || self.h.num_lines() > 0 && f.current_selection == self.h.num_lines())
        {
            self.s.f.current_selection -= 1;
        }
        self.check_scroll_bar();
    }

    /// `InsertLine` after the line went in at `index`.
    pub fn inserted(&mut self, index: c_ulong) {
        self.set_dirty();
        if self.s.f.current_selection >= index && self.h.num_lines() > 1 {
            self.s.f.current_selection += 1;
        }
        self.check_scroll_bar();
    }

    /// The line under the mouse (`ButtonDown`, `MouseLButtonDoublePressed`).
    fn line_at_mouse(&self) -> c_ulong {
        let first = self.first_line();
        let y = self.h.mouse_location().y;
        um_floor((y - self.s.text.top) / self.s.f.line_height as f32 + first as f32) as c_ulong
    }

    /// `ButtonDown`: a click selects the line under it (not in an auto-select box).
    pub fn button_down(&mut self) {
        if self.s.f.button_down || self.s.f.auto_select {
            return;
        }
        let line = self.line_at_mouse();
        if line < self.h.num_lines() {
            self.call(|h| h.change_selection(line, true, false));
        }
        self.call(|h| h.fire(TRIGGER_BUTTON_DOWN));
        self.s.f.button_down = true;
    }

    /// `MouseLButtonDoublePressed` inside the text: selects the line and fires; false outside it (the C++ hands the
    /// click to `UIControl`).
    pub fn double_pressed(&mut self) -> bool {
        if !self.s.text.contains(self.h.mouse_location()) {
            return false;
        }
        let line = self.line_at_mouse();
        if line < self.h.num_lines() {
            self.call(|h| h.change_selection(line, true, false));
        }
        self.call(|h| h.fire(TRIGGER_DOUBLE_CLICKED));
        true
    }

    /// `MouseLButtonUp` (not on the scroll bar): whether the button went up on the selected line.
    pub fn up_on_selection(&self) -> bool {
        let mouse = self.h.mouse_location();
        if !self.s.text.contains(mouse) {
            return false;
        }
        let mut r = self.s.text;
        let row = self.s.f.current_selection.wrapping_sub(self.first_line() as c_ulong);
        r.top += row as f32 * self.s.f.line_height as f32; // an addition commutes: the C++ added r.top last
        r.bottom = r.top + self.s.f.line_height as f32;
        r.contains(mouse)
    }

    /// `MouseMove` (not on the scroll bar): an auto-select box selects the line under the mouse.
    pub fn mouse_move(&mut self, at: UiPoint) {
        if !self.s.f.auto_select || !self.s.text.contains(at) {
            return;
        }
        let first = self.first_line() as c_ulong;
        let y = self.h.mouse_location().y;
        let mut line =
            ((um_floor(y - self.s.text.top) as c_ulong) / self.s.f.line_height).wrapping_add(first) as c_long;
        let last = self.s.f.number_lines_displayed.wrapping_add(first) as c_long;
        if line >= last {
            line = last.wrapping_sub(1);
        }
        if line < self.h.num_lines() as c_long {
            self.call(|h| h.change_selection(line as c_ulong, true, false));
        }
    }

    /// `ProcessDown`.
    pub fn down(&mut self) {
        let selection = self.s.f.current_selection;
        if selection + 1 < self.h.num_lines() {
            self.call(|h| h.change_selection(selection + 1, true, false));
            if self.h.has_scroll_bar() {
                self.scroll_to_selection(true);
            }
            self.set_dirty();
        }
    }

    /// `ProcessUp`.
    pub fn up(&mut self) {
        let selection = self.s.f.current_selection;
        if selection != 0 {
            self.call(|h| h.change_selection(selection - 1, true, false));
            if self.h.has_scroll_bar() {
                self.scroll_to_selection(false);
            }
            self.set_dirty();
        }
    }

    /// `ProcessHome`.
    pub fn home(&mut self) {
        if self.s.f.current_selection > 0 {
            if self.h.has_scroll_bar() {
                self.call(|h| h.set_scroll_value(0));
            }
            self.call(|h| h.change_selection(0, true, false));
            self.set_dirty();
        }
    }

    /// `ProcessEnd`.
    pub fn end(&mut self) {
        if self.s.f.current_selection < self.h.num_lines() {
            if self.h.has_scroll_bar() {
                let max = self.h.scroll_max() as c_ulong;
                self.call(|h| h.set_scroll_value(max));
            }
            let last = self.h.num_lines().wrapping_sub(1);
            self.call(|h| h.change_selection(last, true, false));
            self.set_dirty();
        }
    }

    /// `SetLocation`: the text area, the scroll bar beside it and the lines shown follow the control.
    pub fn set_location(&mut self, at: UiPoint) {
        self.call(|h| h.base_set_location(at));
        let (rect, border) = (self.s.rect, self.s.border as f32);
        self.s.text.left = rect.left + border;
        self.s.text.top = rect.top + border;
        self.s.text.bottom = rect.bottom - border;
        self.s.text.right = rect.right - border;
        if self.h.has_scroll_bar() && !self.s.f.scroll_bar_removed {
            self.s.text.right = rect.right - SCROLL_BAR_WIDTH;
            let corner = UiPoint { x: self.s.text.right + 1.0, y: self.s.text.top };
            let bar = UiRect::new(corner.x, corner.y, self.s.rect.right, self.s.text.bottom);
            self.call(|h| h.scroll_set_rectangle(&bar));
        }
        let rect = self.s.rect;
        self.s.f.number_lines_displayed =
            (um_floor(rect.bottom - rect.top - (2 * self.s.border) as f32) as c_ulong) / self.s.f.line_height;
        let text = self.s.text;
        self.call(|h| h.set_bg_rectangle(&text));
        self.check_scroll_bar();
    }
}

#[cfg(test)]
mod tests;
