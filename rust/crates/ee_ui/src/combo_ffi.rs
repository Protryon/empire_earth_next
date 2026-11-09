//! `extern "C"` for `UIComboBox`'s members (`User Interface/UIComboBox.cpp`). `refs` names the C++ object and its
//! members ([`ComboRefs`]); the host calls go back through `ee_uicpp_combo_*` (UIComboBox.cpp). Texts cross
//! NUL-terminated; a text the C++ returns is copied before the next host call.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_int, c_long, c_ulong, c_void, CStr};

use crate::combo::{self, ComboFields, ComboHost, ComboKey, ComboPart, ComboState};
use crate::rect::{UiPoint, UiRect};
use crate::text::Platform;

/// One combo box as Rust sees it: the C++ object, its record from `mTextBox` on, `mRectangle`, `mIndex`, and `ctx`,
/// the C++ forwarder's context for a host call (`UpdateTranslatedKey`'s key; NULL otherwise).
#[repr(C)]
pub struct ComboRefs {
    pub combo: *mut c_void,
    pub fields: *mut ComboFields,
    pub rect: *const UiRect,
    pub index: *const c_ulong,
    pub ctx: *mut c_void,
}

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_char, c_int, c_long, c_ulong, c_void};

    use crate::rect::{UiPoint, UiRect};

    extern "C" {
        pub fn ee_uicpp_combo_has_label(c: *mut c_void) -> bool;
        pub fn ee_uicpp_combo_rect_of(c: *mut c_void, part: c_int) -> UiRect;
        pub fn ee_uicpp_combo_text_box_border(c: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_combo_line_count(c: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_combo_line(c: *mut c_void, i: c_ulong) -> *const c_char;
        pub fn ee_uicpp_combo_line_height(c: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_combo_list_border(c: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_combo_list_scroll_bar_rect(c: *mut c_void, out: *mut UiRect) -> bool;
        pub fn ee_uicpp_combo_list_scroll_bar_captured(c: *mut c_void) -> bool;
        pub fn ee_uicpp_combo_list_selection(c: *mut c_void, out: *mut c_ulong) -> c_long;
        pub fn ee_uicpp_combo_list_change_selection(c: *mut c_void, i: c_ulong, fire: bool, scroll: bool);
        pub fn ee_uicpp_combo_list_insert_line(c: *mut c_void, text: *const c_char, i: c_ulong);
        pub fn ee_uicpp_combo_list_delete_line(c: *mut c_void, i: c_ulong);
        pub fn ee_uicpp_combo_list_delete_all(c: *mut c_void);
        pub fn ee_uicpp_combo_list_change_width(c: *mut c_void, w: c_ulong);
        pub fn ee_uicpp_combo_window_height(c: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_combo_edit_text(c: *mut c_void) -> *const c_char;
        pub fn ee_uicpp_combo_show_line(c: *mut c_void, i: c_ulong);
        pub fn ee_uicpp_combo_clear_text(c: *mut c_void);
        pub fn ee_uicpp_combo_delete_text(c: *mut c_void);
        pub fn ee_uicpp_combo_part_mouse_down(c: *mut c_void, part: c_int) -> c_long;
        pub fn ee_uicpp_combo_part_mouse_up(c: *mut c_void, part: c_int) -> c_long;
        pub fn ee_uicpp_combo_part_mouse_move(c: *mut c_void, part: c_int, x: f32, y: f32) -> c_long;
        pub fn ee_uicpp_combo_list_wheel(c: *mut c_void, delta: c_long) -> c_long;
        pub fn ee_uicpp_combo_part_update_keyboard(c: *mut c_void, part: c_int) -> c_long;
        pub fn ee_uicpp_combo_part_translated_key(c: *mut c_void, part: c_int, ctx: *mut c_void) -> c_long;
        pub fn ee_uicpp_combo_part_tabbed(c: *mut c_void, part: c_int, to: bool);
        pub fn ee_uicpp_combo_part_set_rectangle(c: *mut c_void, part: c_int, r: *const UiRect);
        pub fn ee_uicpp_combo_part_set_location(c: *mut c_void, part: c_int, x: f32, y: f32);
        pub fn ee_uicpp_combo_part_copy_data(c: *mut c_void, part: c_int) -> c_long;
        pub fn ee_uicpp_combo_copy_list_colours(c: *mut c_void);
        pub fn ee_uicpp_combo_set_focus_rect(c: *mut c_void, r: *const UiRect);
        pub fn ee_uicpp_combo_base_copy_data(c: *mut c_void) -> c_long;
        pub fn ee_uicpp_combo_tabbing_on(c: *mut c_void) -> bool;
        pub fn ee_uicpp_combo_tab_stop(c: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_combo_set_popup(c: *mut c_void, this: bool);
        pub fn ee_uicpp_combo_set_dirty(c: *mut c_void);
        pub fn ee_uicpp_combo_set_rectangle(c: *mut c_void, r: *const UiRect);
        pub fn ee_uicpp_combo_base_set_rectangle(c: *mut c_void, r: *const UiRect);
        pub fn ee_uicpp_combo_base_set_location(c: *mut c_void, x: f32, y: f32);
        pub fn ee_uicpp_combo_set_always_dirty(c: *mut c_void, on: bool);
        pub fn ee_uicpp_combo_fire(c: *mut c_void, event: c_ulong);
        pub fn ee_uicpp_combo_capture_mouse(c: *mut c_void);
        pub fn ee_uicpp_combo_capture_keyboard(c: *mut c_void);
        pub fn ee_uicpp_combo_release_mouse(c: *mut c_void);
        pub fn ee_uicpp_combo_release_keyboard(c: *mut c_void);
        pub fn ee_uicpp_combo_has_mouse_capture(c: *mut c_void) -> bool;
        pub fn ee_uicpp_combo_mouse_location(c: *mut c_void) -> UiPoint;
        pub fn ee_uicpp_combo_key(c: *mut c_void, key: c_int) -> bool;
        pub fn ee_uicpp_combo_open_list(c: *mut c_void);
        pub fn ee_uicpp_combo_close_list(c: *mut c_void);
        pub fn ee_uicpp_combo_add_string(c: *mut c_void) -> c_long;
        pub fn ee_uicpp_combo_change_selection(c: *mut c_void);
        pub fn ee_uicpp_combo_process_arrow(c: *mut c_void, down: bool);
    }
}

struct Raw<'a>(&'a ComboRefs);

fn copy(p: *const c_char) -> Vec<u8> {
    // SAFETY: the host returns a live NUL-terminated buffer
    unsafe { CStr::from_ptr(p) }.to_bytes_with_nul().to_vec()
}

fn cstr(text: &[u8]) -> Vec<u8> {
    let mut v = text[..text.iter().position(|&b| b == 0).unwrap_or(text.len())].to_vec();
    v.push(0);
    v
}

impl ComboHost for Raw<'_> {
    fn load(&self) -> ComboState {
        unsafe { ComboState { f: self.0.fields.read(), rect: self.0.rect.read(), index: self.0.index.read() } }
    }
    fn store(&mut self, f: &ComboFields) {
        unsafe { self.0.fields.write(*f) }
    }
    fn has_label(&self) -> bool {
        unsafe { sys::ee_uicpp_combo_has_label(self.0.combo) }
    }
    fn rect_of(&self, part: ComboPart) -> UiRect {
        unsafe { sys::ee_uicpp_combo_rect_of(self.0.combo, part as c_int) }
    }
    fn text_box_border(&self) -> c_ulong {
        unsafe { sys::ee_uicpp_combo_text_box_border(self.0.combo) }
    }
    fn line_count(&self) -> c_ulong {
        unsafe { sys::ee_uicpp_combo_line_count(self.0.combo) }
    }
    fn line(&self, i: c_ulong) -> Vec<u8> {
        copy(unsafe { sys::ee_uicpp_combo_line(self.0.combo, i) })
    }
    fn line_height(&self) -> c_ulong {
        unsafe { sys::ee_uicpp_combo_line_height(self.0.combo) }
    }
    fn list_border(&self) -> c_ulong {
        unsafe { sys::ee_uicpp_combo_list_border(self.0.combo) }
    }
    fn list_scroll_bar_rect(&self) -> Option<UiRect> {
        let mut r = UiRect::default();
        unsafe { sys::ee_uicpp_combo_list_scroll_bar_rect(self.0.combo, &mut r) }.then_some(r)
    }
    fn list_scroll_bar_captured(&self) -> bool {
        unsafe { sys::ee_uicpp_combo_list_scroll_bar_captured(self.0.combo) }
    }
    fn list_selection(&self, out: &mut c_ulong) -> c_long {
        unsafe { sys::ee_uicpp_combo_list_selection(self.0.combo, out) }
    }
    fn list_change_selection(&mut self, i: c_ulong, fire: bool, scroll: bool) {
        unsafe { sys::ee_uicpp_combo_list_change_selection(self.0.combo, i, fire, scroll) }
    }
    fn list_insert_line(&mut self, text: &[u8], i: c_ulong) {
        let t = cstr(text);
        unsafe { sys::ee_uicpp_combo_list_insert_line(self.0.combo, t.as_ptr() as *const c_char, i) }
    }
    fn list_delete_line(&mut self, i: c_ulong) {
        unsafe { sys::ee_uicpp_combo_list_delete_line(self.0.combo, i) }
    }
    fn list_delete_all(&mut self) {
        unsafe { sys::ee_uicpp_combo_list_delete_all(self.0.combo) }
    }
    fn list_change_width(&mut self, w: c_ulong) {
        unsafe { sys::ee_uicpp_combo_list_change_width(self.0.combo, w) }
    }
    fn window_height(&self) -> c_ulong {
        unsafe { sys::ee_uicpp_combo_window_height(self.0.combo) }
    }
    fn edit_text(&self) -> Vec<u8> {
        copy(unsafe { sys::ee_uicpp_combo_edit_text(self.0.combo) })
    }
    fn show_line(&mut self, i: c_ulong) {
        unsafe { sys::ee_uicpp_combo_show_line(self.0.combo, i) }
    }
    fn clear_text(&mut self) {
        unsafe { sys::ee_uicpp_combo_clear_text(self.0.combo) }
    }
    fn delete_text(&mut self) {
        unsafe { sys::ee_uicpp_combo_delete_text(self.0.combo) }
    }
    fn part_mouse_down(&mut self, part: ComboPart) -> c_long {
        unsafe { sys::ee_uicpp_combo_part_mouse_down(self.0.combo, part as c_int) }
    }
    fn part_mouse_up(&mut self, part: ComboPart) -> c_long {
        unsafe { sys::ee_uicpp_combo_part_mouse_up(self.0.combo, part as c_int) }
    }
    fn part_mouse_move(&mut self, part: ComboPart, at: UiPoint) -> c_long {
        unsafe { sys::ee_uicpp_combo_part_mouse_move(self.0.combo, part as c_int, at.x, at.y) }
    }
    fn list_wheel(&mut self, delta: c_long) -> c_long {
        unsafe { sys::ee_uicpp_combo_list_wheel(self.0.combo, delta) }
    }
    fn part_update_keyboard(&mut self, part: ComboPart) -> c_long {
        unsafe { sys::ee_uicpp_combo_part_update_keyboard(self.0.combo, part as c_int) }
    }
    fn part_translated_key(&mut self, part: ComboPart) -> c_long {
        unsafe { sys::ee_uicpp_combo_part_translated_key(self.0.combo, part as c_int, self.0.ctx) }
    }
    fn part_tabbed(&mut self, part: ComboPart, to: bool) {
        unsafe { sys::ee_uicpp_combo_part_tabbed(self.0.combo, part as c_int, to) }
    }
    fn part_set_rectangle(&mut self, part: ComboPart, r: UiRect) {
        unsafe { sys::ee_uicpp_combo_part_set_rectangle(self.0.combo, part as c_int, &r) }
    }
    fn part_set_location(&mut self, part: ComboPart, at: UiPoint) {
        unsafe { sys::ee_uicpp_combo_part_set_location(self.0.combo, part as c_int, at.x, at.y) }
    }
    fn part_copy_data(&mut self, part: ComboPart) -> c_long {
        unsafe { sys::ee_uicpp_combo_part_copy_data(self.0.combo, part as c_int) }
    }
    fn copy_list_colours(&mut self) {
        unsafe { sys::ee_uicpp_combo_copy_list_colours(self.0.combo) }
    }
    fn set_focus_rect(&mut self, r: UiRect) {
        unsafe { sys::ee_uicpp_combo_set_focus_rect(self.0.combo, &r) }
    }
    fn base_copy_data(&mut self) -> c_long {
        unsafe { sys::ee_uicpp_combo_base_copy_data(self.0.combo) }
    }
    fn tabbing_on(&self) -> bool {
        unsafe { sys::ee_uicpp_combo_tabbing_on(self.0.combo) }
    }
    fn tab_stop(&self) -> c_ulong {
        unsafe { sys::ee_uicpp_combo_tab_stop(self.0.combo) }
    }
    fn set_popup(&mut self, this: bool) {
        unsafe { sys::ee_uicpp_combo_set_popup(self.0.combo, this) }
    }
    fn set_dirty(&mut self) {
        unsafe { sys::ee_uicpp_combo_set_dirty(self.0.combo) }
    }
    fn set_rectangle(&mut self, r: UiRect) {
        unsafe { sys::ee_uicpp_combo_set_rectangle(self.0.combo, &r) }
    }
    fn base_set_rectangle(&mut self, r: UiRect) {
        unsafe { sys::ee_uicpp_combo_base_set_rectangle(self.0.combo, &r) }
    }
    fn base_set_location(&mut self, at: UiPoint) {
        unsafe { sys::ee_uicpp_combo_base_set_location(self.0.combo, at.x, at.y) }
    }
    fn set_always_dirty(&mut self, on: bool) {
        unsafe { sys::ee_uicpp_combo_set_always_dirty(self.0.combo, on) }
    }
    fn fire(&mut self, event: c_ulong) {
        unsafe { sys::ee_uicpp_combo_fire(self.0.combo, event) }
    }
    fn capture_mouse(&mut self) {
        unsafe { sys::ee_uicpp_combo_capture_mouse(self.0.combo) }
    }
    fn capture_keyboard(&mut self) {
        unsafe { sys::ee_uicpp_combo_capture_keyboard(self.0.combo) }
    }
    fn release_mouse(&mut self) {
        unsafe { sys::ee_uicpp_combo_release_mouse(self.0.combo) }
    }
    fn release_keyboard(&mut self) {
        unsafe { sys::ee_uicpp_combo_release_keyboard(self.0.combo) }
    }
    fn has_mouse_capture(&self) -> bool {
        unsafe { sys::ee_uicpp_combo_has_mouse_capture(self.0.combo) }
    }
    fn mouse_location(&self) -> UiPoint {
        unsafe { sys::ee_uicpp_combo_mouse_location(self.0.combo) }
    }
    fn key(&self, key: ComboKey) -> bool {
        unsafe { sys::ee_uicpp_combo_key(self.0.combo, key as c_int) }
    }
    fn open_list(&mut self) {
        unsafe { sys::ee_uicpp_combo_open_list(self.0.combo) }
    }
    fn close_list(&mut self) {
        unsafe { sys::ee_uicpp_combo_close_list(self.0.combo) }
    }
    fn add_string(&mut self) -> c_long {
        unsafe { sys::ee_uicpp_combo_add_string(self.0.combo) }
    }
    fn change_selection(&mut self) {
        unsafe { sys::ee_uicpp_combo_change_selection(self.0.combo) }
    }
    fn process_arrow(&mut self, down: bool) {
        unsafe { sys::ee_uicpp_combo_process_arrow(self.0.combo, down) }
    }
}

unsafe fn raw<'a>(refs: *const ComboRefs) -> Raw<'a> {
    Raw(&*refs)
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

/// `UIComboBox::AddString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_add_string(refs: *const ComboRefs) -> c_long {
    combo::add_string(&Platform, &mut raw(refs))
}

/// `UIComboBox::InsertLine` (`text` NUL-terminated).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_insert_line(refs: *const ComboRefs, text: *const c_char, i: c_ulong) {
    combo::insert_line(&mut raw(refs), CStr::from_ptr(text).to_bytes_with_nul(), i);
}

/// `AppendLine` / `AppendStringArray`'s end: an open list reopens.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_reopen(refs: *const ComboRefs) {
    combo::reopen(&mut raw(refs));
}

/// `UIComboBox::ChangeSelection`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_change_selection(refs: *const ComboRefs) {
    combo::change_selection(&mut raw(refs));
}

/// `UIComboBox::ChangeWidth`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_change_width(refs: *const ComboRefs, width: c_ulong) {
    combo::change_width(&mut raw(refs), width);
}

/// `UIComboBox::CloseListBox`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_close_list_box(refs: *const ComboRefs) {
    combo::close_list_box(&mut raw(refs));
}

/// `UIComboBox::CopyData`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_copy_data(refs: *const ComboRefs) -> c_long {
    combo::copy_data(&mut raw(refs))
}

/// `UIComboBox::DeleteAllEntries`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_delete_all_entries(refs: *const ComboRefs) {
    combo::delete_all_entries(&mut raw(refs));
}

/// `UIComboBox::DeleteLine`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_delete_line(refs: *const ComboRefs, line: c_ulong) {
    combo::delete_line(&mut raw(refs), line);
}

/// `UIComboBox::HideControl`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_hide_control(refs: *const ComboRefs) {
    combo::hide_control(&mut raw(refs));
}

/// `UIComboBox::MouseLButtonDown`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_mouse_l_button_down(refs: *const ComboRefs) -> c_long {
    combo::mouse_l_button_down(&mut raw(refs))
}

/// `UIComboBox::MouseLButtonUp`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_mouse_l_button_up(refs: *const ComboRefs) -> c_long {
    combo::mouse_l_button_up(&mut raw(refs))
}

/// `UIComboBox::MouseMove`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_mouse_move(refs: *const ComboRefs, at: UiPoint) -> c_long {
    combo::mouse_move(&mut raw(refs), at)
}

/// `UIComboBox::MouseWheelTurned`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_mouse_wheel_turned(refs: *const ComboRefs, delta: c_long) -> c_long {
    combo::mouse_wheel_turned(&mut raw(refs), delta)
}

/// `UIComboBox::OpenListBox`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_open_list_box(refs: *const ComboRefs) {
    combo::open_list_box(&mut raw(refs));
}

/// `UIComboBox::ProcessDown` and `ProcessUp`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_process_arrow(refs: *const ComboRefs) {
    combo::process_arrow(&mut raw(refs));
}

/// `UIComboBox::SetCurrentSelection`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_set_current_selection(refs: *const ComboRefs, i: c_ulong) {
    combo::set_current_selection(&mut raw(refs), i);
}

/// `UIComboBox::SetLocation`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_set_location(refs: *const ComboRefs, at: UiPoint) {
    combo::set_location(&mut raw(refs), at);
}

/// `UIComboBox::TabbedTo` (`to`) or `TabbedFrom`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_tabbed(refs: *const ComboRefs, to: bool) {
    if to {
        combo::tabbed_to(&mut raw(refs));
    } else {
        combo::tabbed_from(&mut raw(refs));
    }
}

/// `UIComboBox::UpdateKeyboard`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_update_keyboard(refs: *const ComboRefs) -> c_long {
    combo::update_keyboard(&mut raw(refs))
}

/// `UIComboBox::UpdateTranslatedKey` (`ctx`: the C++ key and repeat count).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_combo_update_translated_key(refs: *const ComboRefs) -> c_long {
    combo::update_translated_key(&mut raw(refs))
}
