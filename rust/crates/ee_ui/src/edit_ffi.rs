//! `extern "C"` for `UIEditBox`'s members (`User Interface/UIEditBox.cpp`). `refs` names the C++ object and its
//! members ([`EditRefs`]); the host calls go back through `ee_uicpp_edit_*` (UIEditBox.cpp). Texts cross
//! NUL-terminated; a text the C++ returns is copied before the next host call.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_int, c_long, c_short, c_ulong, c_void, CStr};

use crate::edit::{Edit, EditFields, EditHost, EditState};
use crate::edit_keys::{EditKey, EditKeysHost, EditPoll, PollOp};
use crate::rect::{UiPoint, UiRect};
use crate::text::Platform;

/// One edit box as Rust sees it: the C++ object, its record from `mCaretX` on, `mTextRectangle`, `mRectangle` and
/// `mBorderThickness`.
#[repr(C)]
pub struct EditRefs {
    pub edit: *mut c_void,
    pub fields: *mut EditFields,
    pub text: *mut UiRect,
    pub rect: *const UiRect,
    pub border: *const c_ulong,
}

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_char, c_int, c_long, c_ulong, c_void};

    use crate::rect::UiRect;

    extern "C" {
        pub fn ee_uicpp_edit_line_count(edit: *mut c_void) -> usize;
        pub fn ee_uicpp_edit_line(edit: *mut c_void, i: usize) -> *const c_char;
        pub fn ee_uicpp_edit_set_line(edit: *mut c_void, i: usize, text: *const c_char);
        pub fn ee_uicpp_edit_insert_line(edit: *mut c_void, i: usize, text: *const c_char);
        pub fn ee_uicpp_edit_erase_line(edit: *mut c_void, i: usize);
        pub fn ee_uicpp_edit_keep_first_line(edit: *mut c_void);
        pub fn ee_uicpp_edit_text_width(edit: *mut c_void, text: *const c_char) -> f32;
        pub fn ee_uicpp_edit_font_height(edit: *mut c_void) -> c_long;
        pub fn ee_uicpp_edit_has_scroll_bar(edit: *mut c_void) -> bool;
        pub fn ee_uicpp_edit_scroll_value(edit: *mut c_void) -> c_long;
        pub fn ee_uicpp_edit_scroll_max(edit: *mut c_void) -> c_long;
        pub fn ee_uicpp_edit_set_scroll_value(edit: *mut c_void, value: c_ulong);
        pub fn ee_uicpp_edit_set_scroll_range(edit: *mut c_void, min: c_ulong, max: c_ulong);
        pub fn ee_uicpp_edit_scroll_set_location(edit: *mut c_void, x: f32, y: f32);
        pub fn ee_uicpp_edit_set_dirty(edit: *mut c_void);
        pub fn ee_uicpp_edit_set_rectangle(edit: *mut c_void, r: *const UiRect);
        pub fn ee_uicpp_edit_base_set_rectangle(edit: *mut c_void, r: *const UiRect);
        pub fn ee_uicpp_edit_base_set_location(edit: *mut c_void, x: f32, y: f32);
        pub fn ee_uicpp_edit_fire(edit: *mut c_void, event: c_ulong);
        pub fn ee_uicpp_wide_assign(out: *mut c_void, text: *const c_char);
        pub fn ee_uicpp_edit_key(edit: *mut c_void, key: c_int) -> bool;
        pub fn ee_uicpp_edit_poll(edit: *mut c_void, poll: c_int, op: c_int) -> c_ulong;
        pub fn ee_uicpp_edit_process_enter_key(edit: *mut c_void);
        pub fn ee_uicpp_edit_clipboard_set(text: *const c_char);
        pub fn ee_uicpp_edit_clipboard_get() -> *mut c_char;
        pub fn ee_uicpp_edit_clipboard_free(text: *mut c_char);
        pub fn ee_uicpp_edit_translated_key(edit: *mut c_void, text: *const c_char);
    }
}

struct Raw(EditRefs);

fn copy(p: *const c_char) -> Vec<u8> {
    // SAFETY: the host returns a live NUL-terminated buffer
    unsafe { CStr::from_ptr(p) }.to_bytes_with_nul().to_vec()
}

fn cstr(text: &[u8]) -> *const c_char {
    debug_assert_eq!(text.last(), Some(&0));
    text.as_ptr() as *const c_char
}

impl EditHost for Raw {
    fn load(&self) -> EditState {
        unsafe {
            EditState {
                f: self.0.fields.read(),
                text: self.0.text.read(),
                rect: self.0.rect.read(),
                border: self.0.border.read(),
            }
        }
    }
    fn store(&mut self, s: &EditState) {
        unsafe {
            self.0.fields.write(s.f);
            self.0.text.write(s.text);
        }
    }
    fn line_count(&self) -> usize {
        unsafe { sys::ee_uicpp_edit_line_count(self.0.edit) }
    }
    fn line(&self, i: usize) -> Vec<u8> {
        copy(unsafe { sys::ee_uicpp_edit_line(self.0.edit, i) })
    }
    fn set_line(&mut self, i: usize, text: &[u8]) {
        unsafe { sys::ee_uicpp_edit_set_line(self.0.edit, i, cstr(text)) }
    }
    fn insert_line(&mut self, i: usize, text: &[u8]) {
        unsafe { sys::ee_uicpp_edit_insert_line(self.0.edit, i, cstr(text)) }
    }
    fn erase_line(&mut self, i: usize) {
        unsafe { sys::ee_uicpp_edit_erase_line(self.0.edit, i) }
    }
    fn keep_first_line(&mut self) {
        unsafe { sys::ee_uicpp_edit_keep_first_line(self.0.edit) }
    }
    fn text_width(&mut self, text: &[u8]) -> f32 {
        unsafe { sys::ee_uicpp_edit_text_width(self.0.edit, cstr(text)) }
    }
    fn font_height(&self) -> c_long {
        unsafe { sys::ee_uicpp_edit_font_height(self.0.edit) }
    }
    fn has_scroll_bar(&self) -> bool {
        unsafe { sys::ee_uicpp_edit_has_scroll_bar(self.0.edit) }
    }
    fn scroll_value(&self) -> c_long {
        unsafe { sys::ee_uicpp_edit_scroll_value(self.0.edit) }
    }
    fn scroll_max(&self) -> c_long {
        unsafe { sys::ee_uicpp_edit_scroll_max(self.0.edit) }
    }
    fn set_scroll_value(&mut self, value: c_ulong) {
        unsafe { sys::ee_uicpp_edit_set_scroll_value(self.0.edit, value) }
    }
    fn set_scroll_range(&mut self, min: c_ulong, max: c_ulong) {
        unsafe { sys::ee_uicpp_edit_set_scroll_range(self.0.edit, min, max) }
    }
    fn scroll_set_location(&mut self, at: UiPoint) {
        unsafe { sys::ee_uicpp_edit_scroll_set_location(self.0.edit, at.x, at.y) }
    }
    fn set_dirty(&mut self) {
        unsafe { sys::ee_uicpp_edit_set_dirty(self.0.edit) }
    }
    fn set_rectangle(&mut self, r: &UiRect) {
        unsafe { sys::ee_uicpp_edit_set_rectangle(self.0.edit, r) }
    }
    fn base_set_rectangle(&mut self, r: &UiRect) {
        unsafe { sys::ee_uicpp_edit_base_set_rectangle(self.0.edit, r) }
    }
    fn base_set_location(&mut self, at: UiPoint) {
        unsafe { sys::ee_uicpp_edit_base_set_location(self.0.edit, at.x, at.y) }
    }
    fn fire(&mut self, event: c_ulong) {
        unsafe { sys::ee_uicpp_edit_fire(self.0.edit, event) }
    }
}

impl EditKeysHost for Raw {
    fn key(&mut self, k: EditKey) -> bool {
        unsafe { sys::ee_uicpp_edit_key(self.0.edit, k as c_int) }
    }
    fn poll(&mut self, p: EditPoll, op: PollOp) -> c_ulong {
        unsafe { sys::ee_uicpp_edit_poll(self.0.edit, p as c_int, op as c_int) }
    }
    fn process_enter_key(&mut self) {
        unsafe { sys::ee_uicpp_edit_process_enter_key(self.0.edit) }
    }
    fn clipboard_set(&mut self, text: &[u8]) {
        unsafe { sys::ee_uicpp_edit_clipboard_set(cstr(text)) }
    }
    fn clipboard_get(&mut self) -> Option<Vec<u8>> {
        let p = unsafe { sys::ee_uicpp_edit_clipboard_get() };
        if p.is_null() {
            return None;
        }
        let text = copy(p);
        unsafe { sys::ee_uicpp_edit_clipboard_free(p) };
        Some(text)
    }
    fn translated_key(&mut self, text: &[u8]) {
        unsafe { sys::ee_uicpp_edit_translated_key(self.0.edit, cstr(text)) }
    }
}

/// One member call: the members in, the model, the members out.
unsafe fn run<R>(refs: *const EditRefs, f: impl FnOnce(&mut Edit<Platform, Raw>) -> R) -> R {
    let mut raw = Raw(refs.read());
    let mut e = Edit::new(&Platform, &mut raw);
    let r = f(&mut e);
    e.finish();
    r
}

unsafe fn text_of<'a>(p: *const c_char) -> &'a [u8] {
    CStr::from_ptr(p).to_bytes_with_nul()
}

/// `UIEditBox::AddChar` -> how many bytes of `text` a single-line box read.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_add_char(refs: *const EditRefs, text: *const c_char) -> usize {
    run(refs, |e| e.add_char(text_of(text)))
}

/// `UIEditBox::BackSpaceChar`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_back_space_char(refs: *const EditRefs) {
    run(refs, |e| e.back_space_char())
}

/// `UIEditBox::CalculateCaretFromMouseLocation` (the outputs are written after the model ran: a caller may pass the
/// box's own caret members).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_caret_from_mouse(
    refs: *const EditRefs,
    at: UiPoint,
    out_line: *mut c_ulong,
    out_index: *mut c_long,
) {
    let (line, index) = run(refs, |e| e.caret_from_mouse(at));
    out_line.write(line);
    out_index.write(index);
}

/// `UIEditBox::ChangeWidth`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_change_width(refs: *const EditRefs, width: c_ulong) {
    run(refs, |e| e.change_width(width))
}

/// `UIEditBox::CheckScrollBar`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_check_scroll_bar(refs: *const EditRefs) {
    run(refs, |e| e.check_scroll_bar())
}

/// `UIEditBox::ComputeCaretLocation`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_compute_caret_location(refs: *const EditRefs, update_x: bool) {
    run(refs, |e| e.compute_caret_location(update_x))
}

/// `UIEditBox::ComputeDrawingIndex`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_compute_drawing_index(refs: *const EditRefs) {
    run(refs, |e| e.compute_drawing_index())
}

/// `UIEditBox::DecrementCaretPos`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_decrement_caret_pos(refs: *const EditRefs) {
    run(refs, |e| e.decrement_caret_pos())
}

/// `UIEditBox::DeleteChar`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_delete_char(refs: *const EditRefs) {
    run(refs, |e| e.delete_char())
}

/// `UIEditBox::DeleteLine`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_delete_line(refs: *const EditRefs, line: c_ulong) {
    run(refs, |e| e.delete_line(line))
}

/// `UIEditBox::DeleteSelectedText`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_delete_selected_text(refs: *const EditRefs) {
    run(refs, |e| e.delete_selected_text())
}

/// `UIEditBox::DeleteText(start index, start line, end index, end line)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_delete_range(
    refs: *const EditRefs,
    start_index: c_ulong,
    start_line: c_ulong,
    end_index: c_ulong,
    end_line: c_ulong,
) {
    run(refs, |e| e.delete_range(start_index, start_line, end_index, end_line))
}

/// `UIEditBox::DeleteText()`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_clear(refs: *const EditRefs) {
    run(refs, |e| e.clear())
}

/// `UIEditBox::GetLocationFromIndex`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_location_from_index(
    refs: *const EditRefs,
    index: c_ulong,
    line: c_ulong,
    out: *mut UiPoint,
) {
    let at = run(refs, |e| e.location_from_index(index, line));
    out.write(at);
}

/// `UIEditBox::GetSelectedText` into the `UWideString` `out` (left alone without a selection).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_selected_text(refs: *const EditRefs, out: *mut c_void) {
    if let Some(text) = run(refs, |e| e.selected_text()) {
        sys::ee_uicpp_wide_assign(out, cstr(&text));
    }
}

/// `UIEditBox::GetText(start line, end line, start index, end index)` into the `UWideString` `out`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_text(
    refs: *const EditRefs,
    start_line: c_ulong,
    end_line: c_ulong,
    start_index: c_ulong,
    end_index: c_ulong,
    out: *mut c_void,
) {
    let text = run(refs, |e| e.text(start_line, end_line, start_index, end_index));
    sys::ee_uicpp_wide_assign(out, cstr(&text));
}

/// `UIEditBox::IncrementCaretPos`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_increment_caret_pos(refs: *const EditRefs, count: c_long) {
    run(refs, |e| e.increment_caret_pos(count))
}

/// `UIEditBox::ProcessDownKey`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_down_key(refs: *const EditRefs) {
    run(refs, |e| e.down_key())
}

/// `UIEditBox::ProcessEndKey`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_end_key(refs: *const EditRefs) {
    run(refs, |e| e.end_key())
}

/// `UIEditBox::ProcessEnterKey`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_enter_key(refs: *const EditRefs) {
    run(refs, |e| e.enter_key())
}

/// `UIEditBox::ProcessHomeKey`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_home_key(refs: *const EditRefs) {
    run(refs, |e| e.home_key())
}

/// `UIEditBox::ProcessUpKey`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_up_key(refs: *const EditRefs) {
    run(refs, |e| e.up_key())
}

/// `UIEditBox::ScrollLeft`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_scroll_left(refs: *const EditRefs) {
    run(refs, |e| e.scroll_left())
}

/// `UIEditBox::ScrollRight`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_scroll_right(refs: *const EditRefs, count: c_long) {
    run(refs, |e| e.scroll_right(count))
}

/// `UIEditBox::SelectText()`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_select_all(refs: *const EditRefs) {
    run(refs, |e| e.select_all())
}

/// `UIEditBox::SelectText(caret index, anchor index, caret line, anchor line)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_select(
    refs: *const EditRefs,
    caret_index: c_ulong,
    start_index: c_ulong,
    caret_line: c_ulong,
    start_line: c_ulong,
) {
    run(refs, |e| e.select(caret_index, start_index, caret_line, start_line))
}

/// `UIEditBox::SetCaretLocation`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_set_caret_location(refs: *const EditRefs, index: c_ulong, line: c_ulong) {
    run(refs, |e| e.set_caret_location(index, line))
}

/// `UIEditBox::SetIndexFromUpDownOffset`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_index_from_up_down_offset(refs: *const EditRefs) {
    run(refs, |e| e.index_from_up_down_offset())
}

/// `UIEditBox::SetLocation`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_set_location(refs: *const EditRefs, at: UiPoint) {
    run(refs, |e| e.set_location(at))
}

/// `UIEditBox::SetText`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_set_text(refs: *const EditRefs, text: *const c_char) {
    run(refs, |e| e.set_text(text_of(text)))
}

/// `UIEditBox::OpenIMECompositionString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_ime_open(refs: *const EditRefs) -> c_long {
    run(refs, |e| e.ime_open())
}

/// `UIEditBox::CloseIMECompositionString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_ime_close(refs: *const EditRefs) -> c_long {
    run(refs, |e| e.ime_close())
}

/// `UIEditBox::ChangeIMECompositionString` (the C++ sets `mIMEAttributes`).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_ime_change(refs: *const EditRefs, text: *const c_char, cursor: c_ulong) -> c_long {
    run(refs, |e| e.ime_change(text_of(text), cursor))
}

/// `UIEditBox::RemoveIMECompositionString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_ime_remove(refs: *const EditRefs) {
    run(refs, |e| e.ime_remove())
}

/// `UIEditBox::UpdateTranslatedKey`'s typing (the C++ restarts the caret blink).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_type_key(refs: *const EditRefs, key: *const c_char, repeat: c_short) {
    run(refs, |e| e.type_key(text_of(key), repeat))
}

/// `UIEditBox::UpdateKeyboard`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_update_keyboard(refs: *const EditRefs) -> c_long {
    run(refs, |e| e.update_keyboard())
}

/// `UIEditBox::Copy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_copy(refs: *const EditRefs) {
    run(refs, |e| e.copy())
}

/// `UIEditBox::Cut`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_cut(refs: *const EditRefs) {
    run(refs, |e| e.cut())
}

/// `UIEditBox::Paste`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_paste(refs: *const EditRefs) {
    run(refs, |e| e.paste())
}

/// `UIEditBox::AddIMEResultString` (the text copied first: typing it comes back into the box).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_add_ime_result(refs: *const EditRefs, text: *const c_char) {
    let text = text_of(text).to_vec();
    run(refs, |e| e.add_ime_result(&text))
}

/// `UIEditBox::MouseMove` while selecting (the C++ forwards a drag of its scroll bar).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_edit_drag(refs: *const EditRefs, at: UiPoint) {
    run(refs, |e| e.drag_to(at))
}
