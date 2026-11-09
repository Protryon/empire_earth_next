//! `extern "C"` for `UIListBox<T>`'s members (`lle/UIListBox.h`). The template is instantiated for several `T`, so
//! the host calls come as a table of functions instantiated per `T` (`UIListBoxRust<T>`), named in [`ListRefs`] with
//! the object and its members. `ee_ui_text_less_no_case` is `UWSNoCaseAlphaSort`'s order.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_long, c_ulong, c_void, CStr};

use crate::list::{List, ListFields, ListHost, ListState};
use crate::rect::{UiPoint, UiRect};
use crate::text::{less_no_case, Platform};

/// The host functions of one `UIListBox<T>` instantiation.
#[repr(C)]
pub struct ListHostTable {
    pub num_lines: unsafe extern "C" fn(list: *mut c_void) -> c_ulong,
    pub change_selection: unsafe extern "C" fn(list: *mut c_void, index: c_ulong, fire: bool, scroll_to: bool),
    pub clear_selection: unsafe extern "C" fn(list: *mut c_void),
    pub has_scroll_bar: unsafe extern "C" fn(list: *mut c_void) -> bool,
    pub scroll_value: unsafe extern "C" fn(list: *mut c_void) -> c_long,
    pub scroll_max: unsafe extern "C" fn(list: *mut c_void) -> c_long,
    pub set_scroll_value: unsafe extern "C" fn(list: *mut c_void, value: c_ulong),
    pub set_scroll_range: unsafe extern "C" fn(list: *mut c_void, min: c_ulong, max: c_ulong),
    pub scroll_set_rectangle: unsafe extern "C" fn(list: *mut c_void, r: *const UiRect),
    pub create_scroll_bar:
        unsafe extern "C" fn(list: *mut c_void, x: f32, y: f32, length: f32, page: c_ulong, value: c_ulong),
    pub set_dirty: unsafe extern "C" fn(list: *mut c_void),
    pub fire: unsafe extern "C" fn(list: *mut c_void, event: c_ulong),
    pub set_bg_rectangle: unsafe extern "C" fn(list: *mut c_void, r: *const UiRect),
    pub base_set_location: unsafe extern "C" fn(list: *mut c_void, x: f32, y: f32),
    pub mouse_location: unsafe extern "C" fn(list: *mut c_void, out: *mut UiPoint),
}

/// One list box as Rust sees it: the object, its host table, its record from `mScrollBarBgOpacity` on,
/// `mTextRectangle`, `mRectangle` and `mBorderThickness`.
#[repr(C)]
pub struct ListRefs {
    pub list: *mut c_void,
    pub host: *const ListHostTable,
    pub fields: *mut ListFields,
    pub text: *mut UiRect,
    pub rect: *const UiRect,
    pub border: *const c_ulong,
}

struct Raw(ListRefs);

impl Raw {
    fn t(&self) -> &ListHostTable {
        // SAFETY: the C++ passes its instantiation's static table
        unsafe { &*self.0.host }
    }
}

impl ListHost for Raw {
    fn load(&self) -> ListState {
        unsafe {
            ListState {
                f: self.0.fields.read(),
                text: self.0.text.read(),
                rect: self.0.rect.read(),
                border: self.0.border.read(),
            }
        }
    }
    fn store(&mut self, s: &ListState) {
        unsafe {
            self.0.fields.write(s.f);
            self.0.text.write(s.text);
        }
    }
    fn num_lines(&self) -> c_ulong {
        unsafe { (self.t().num_lines)(self.0.list) }
    }
    fn change_selection(&mut self, index: c_ulong, fire: bool, scroll_to: bool) {
        unsafe { (self.t().change_selection)(self.0.list, index, fire, scroll_to) }
    }
    fn clear_selection(&mut self) {
        unsafe { (self.t().clear_selection)(self.0.list) }
    }
    fn has_scroll_bar(&self) -> bool {
        unsafe { (self.t().has_scroll_bar)(self.0.list) }
    }
    fn scroll_value(&self) -> c_long {
        unsafe { (self.t().scroll_value)(self.0.list) }
    }
    fn scroll_max(&self) -> c_long {
        unsafe { (self.t().scroll_max)(self.0.list) }
    }
    fn set_scroll_value(&mut self, value: c_ulong) {
        unsafe { (self.t().set_scroll_value)(self.0.list, value) }
    }
    fn set_scroll_range(&mut self, min: c_ulong, max: c_ulong) {
        unsafe { (self.t().set_scroll_range)(self.0.list, min, max) }
    }
    fn scroll_set_rectangle(&mut self, r: &UiRect) {
        unsafe { (self.t().scroll_set_rectangle)(self.0.list, r) }
    }
    fn create_scroll_bar(&mut self, at: UiPoint, length: f32, page: c_ulong, value: c_ulong) {
        unsafe { (self.t().create_scroll_bar)(self.0.list, at.x, at.y, length, page, value) }
    }
    fn set_dirty(&mut self) {
        unsafe { (self.t().set_dirty)(self.0.list) }
    }
    fn fire(&mut self, event: c_ulong) {
        unsafe { (self.t().fire)(self.0.list, event) }
    }
    fn set_bg_rectangle(&mut self, r: &UiRect) {
        unsafe { (self.t().set_bg_rectangle)(self.0.list, r) }
    }
    fn base_set_location(&mut self, at: UiPoint) {
        unsafe { (self.t().base_set_location)(self.0.list, at.x, at.y) }
    }
    fn mouse_location(&self) -> UiPoint {
        let mut at = UiPoint { x: 0.0, y: 0.0 };
        unsafe { (self.t().mouse_location)(self.0.list, &mut at) };
        at
    }
}

unsafe fn run<R>(refs: *const ListRefs, f: impl FnOnce(&mut List<Raw>) -> R) -> R {
    let mut raw = Raw(refs.read());
    let mut l = List::new(&mut raw);
    let r = f(&mut l);
    l.finish();
    r
}

/// `UIListBox::ChangeSelectionIndex`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_change_selection_index(
    refs: *const ListRefs,
    index: c_ulong,
    fire: bool,
    scroll_to: bool,
) {
    run(refs, |l| l.change_selection_index(index, fire, scroll_to))
}

/// `UIListBox::CheckScrollBar`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_check_scroll_bar(refs: *const ListRefs) {
    run(refs, |l| l.check_scroll_bar())
}

/// `UIListBox::DeleteAllEntries` after `mData.clear()`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_cleared(refs: *const ListRefs) {
    run(refs, |l| l.cleared())
}

/// `UIListBox::DeleteLine` after the erase.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_deleted_line(refs: *const ListRefs, line: c_ulong) {
    run(refs, |l| l.deleted_line(line))
}

/// `UIListBox::InsertLine` after the insert at `index`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_inserted(refs: *const ListRefs, index: c_ulong) {
    run(refs, |l| l.inserted(index))
}

/// `UIListBox::ButtonDown`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_button_down(refs: *const ListRefs) {
    run(refs, |l| l.button_down())
}

/// `UIListBox::MouseLButtonDoublePressed` inside the text (false: the C++ hands the click to `UIControl`).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_double_pressed(refs: *const ListRefs) -> bool {
    run(refs, |l| l.double_pressed())
}

/// `UIListBox::MouseLButtonUp`'s test: the button went up on the selected line.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_up_on_selection(refs: *const ListRefs) -> bool {
    run(refs, |l| l.up_on_selection())
}

/// `UIListBox::MouseMove` off the scroll bar.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_mouse_move(refs: *const ListRefs, at: UiPoint) {
    run(refs, |l| l.mouse_move(at))
}

/// `UIListBox::ProcessDown`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_down(refs: *const ListRefs) {
    run(refs, |l| l.down())
}

/// `UIListBox::ProcessUp`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_up(refs: *const ListRefs) {
    run(refs, |l| l.up())
}

/// `UIListBox::ProcessHome`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_home(refs: *const ListRefs) {
    run(refs, |l| l.home())
}

/// `UIListBox::ProcessEnd`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_end(refs: *const ListRefs) {
    run(refs, |l| l.end())
}

/// `UIListBox::SetLocation`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_set_location(refs: *const ListRefs, at: UiPoint) {
    run(refs, |l| l.set_location(at))
}

/// `UWSNoCaseAlphaSort::operator()`: `left` sorts before `right` with the case folded.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_text_less_no_case(left: *const c_char, right: *const c_char) -> bool {
    less_no_case(&Platform, CStr::from_ptr(left).to_bytes_with_nul(), CStr::from_ptr(right).to_bytes_with_nul())
}
