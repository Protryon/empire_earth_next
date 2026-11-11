//! `extern "C"` for `UIForm`'s dirty-rectangle bookkeeping and its open, close and copy sequences
//! (`User Interface/UIForm.cpp`): `form` is the C++ `UIForm*`, asked through `ee_uicpp_form_life_*` (UIForm.cpp)
//! on each step, never cached.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_int, c_long, c_ulong, c_void};

use crate::form_state::{self, DirtyRect, FormEvent, Handle, LifeHost, Lists};
use crate::rect::UiRect;

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_ulong, c_void};

    use crate::form_state::DirtyRect;
    use crate::rect::UiRect;

    extern "C" {
        pub fn ee_uicpp_form_life_copy_len(form: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_form_life_copy_at(form: *mut c_void, i: c_ulong) -> UiRect;
        pub fn ee_uicpp_form_life_copy_erase(form: *mut c_void, i: c_ulong);
        pub fn ee_uicpp_form_life_copy_push(form: *mut c_void, r: *const UiRect);
        pub fn ee_uicpp_form_life_dirty_len(form: *mut c_void) -> c_ulong;
        pub fn ee_uicpp_form_life_dirty_take(form: *mut c_void, out: *mut DirtyRect);
        pub fn ee_uicpp_form_life_dirty_push(form: *mut c_void, d: *const DirtyRect);
        // value: 0 surfaces, 1 window width, 2 window height, 3 TMGetTime, 4 mOpenTime, 5 mPopupDrawCount,
        // 6 mDrawToolTipShadow, 7 GetShouldFadeIn, 8 mControlCount, 9 mControlGroupCount, 10 mBackgrounds.size,
        // 11 mDatabaseControls.size, 12 mEventData.size
        pub fn ee_uicpp_form_life_value(form: *mut c_void, which: c_int) -> c_ulong;
        // set: 0 mOpenTime, 1 mPopupDrawCount
        pub fn ee_uicpp_form_life_set_value(form: *mut c_void, which: c_int, v: c_ulong);
        // pointer: 0 mPopup, 1 mHelpTextLabel, 2 mMouseOverControl
        pub fn ee_uicpp_form_life_ptr(form: *mut c_void, which: c_int) -> *mut c_void;
        // set: 0 mPopup, 1 mHelpTextLabel, 2 mCopiedPopup
        pub fn ee_uicpp_form_life_set_ptr(form: *mut c_void, which: c_int, p: *mut c_void);
        pub fn ee_uicpp_form_life_rect(form: *mut c_void) -> UiRect;
        pub fn ee_uicpp_form_life_layout(form: *mut c_void, screen: *const UiRect);
        pub fn ee_uicpp_form_life_set_fade(form: *mut c_void, v: f32);
        pub fn ee_uicpp_form_life_set_copied_popup_rect(form: *mut c_void, r: *const UiRect);
        // which: 0 update, 1 open, 2 close -> whether the form had it
        pub fn ee_uicpp_form_life_event(form: *mut c_void, which: c_int, result: *mut c_long) -> bool;
        pub fn ee_uicpp_form_life_manager_dirty(form: *mut c_void, r: *const UiRect);
        // call: 0 ProcessAllEventsForForm, 1 UpdateHelpText(i != 0), 2 SetTabbing(mTabbingOn); 3 background i's Open,
        // 4 its Close
        pub fn ee_uicpp_form_life_call(form: *mut c_void, call: c_int, i: c_ulong);
        pub fn ee_uicpp_form_life_control(form: *mut c_void, i: c_long) -> *mut c_void;
        pub fn ee_uicpp_form_life_set_draw(form: *mut c_void, i: c_long, c: *mut c_void);
        pub fn ee_uicpp_form_life_group_enabled(form: *mut c_void, g: c_long) -> bool;
        pub fn ee_uicpp_form_life_entity_count(form: *mut c_void, g: c_long) -> c_ulong;
        pub fn ee_uicpp_form_life_entity(form: *mut c_void, g: c_long, j: c_ulong) -> *mut c_void;
        pub fn ee_uicpp_form_life_db_table(form: *mut c_void, i: c_ulong) -> *mut c_void;
        pub fn ee_uicpp_form_life_db_control(form: *mut c_void, i: c_ulong) -> *mut c_void;
        pub fn ee_uicpp_form_life_set_db_control(form: *mut c_void, i: c_ulong, c: *mut c_void);
        pub fn ee_uicpp_form_life_create_control(form: *mut c_void, table: *mut c_void) -> *mut c_void;
        pub fn ee_uicpp_form_life_table_create_event(form: *mut c_void, table: *mut c_void);
        // value: 0 GetGroup, 1 GetIsVisible, 2 GetControlType, 3 GetStringTableID
        pub fn ee_uicpp_form_life_table_value(table: *mut c_void, which: c_int) -> c_ulong;
        // call: 0 AddControl, 1 RemoveControl, 2 delete
        pub fn ee_uicpp_form_life_control_call(form: *mut c_void, call: c_int, c: *mut c_void, group: c_ulong);
        pub fn ee_uicpp_form_life_event_data(form: *mut c_void, i: c_ulong, kind: *mut c_ulong) -> c_ulong;
        pub fn ee_uicpp_form_life_create_form_event(form: *mut c_void, id: c_ulong) -> *mut c_void;
        pub fn ee_uicpp_form_life_set_form_event(form: *mut c_void, kind: c_ulong, e: *mut c_void);
        // value: 0 UIEntity::GetType, 1 GetDisplayMode, 2 GetIsDirty, 3 GetDrawCount, 4 GetIndex
        pub fn ee_uicpp_form_life_ctl_value(c: *mut c_void, which: c_int) -> c_ulong;
        pub fn ee_uicpp_form_life_ctl_rect(c: *mut c_void) -> UiRect;
        // call: 0 CopyData (-> its result), 1 SetDirty(arg != 0), 2 SetDisplayMode(arg), 3 HideControl,
        // 4 SetStringTableID(arg)
        pub fn ee_uicpp_form_life_ctl_call(c: *mut c_void, call: c_int, arg: c_ulong) -> c_long;
    }
}

struct Raw(*mut c_void);

impl Raw {
    fn value(&self, which: c_int) -> c_ulong {
        unsafe { sys::ee_uicpp_form_life_value(self.0, which) }
    }
    fn ptr(&self, which: c_int) -> Handle {
        unsafe { sys::ee_uicpp_form_life_ptr(self.0, which) }
    }
    fn set_ptr(&mut self, which: c_int, p: Handle) {
        unsafe { sys::ee_uicpp_form_life_set_ptr(self.0, which, p) }
    }
    fn call(&mut self, call: c_int, i: usize) {
        unsafe { sys::ee_uicpp_form_life_call(self.0, call, i as c_ulong) }
    }
    fn control_call(&mut self, call: c_int, c: Handle, group: c_ulong) {
        unsafe { sys::ee_uicpp_form_life_control_call(self.0, call, c, group) }
    }
}

fn ctl_value(c: Handle, which: c_int) -> c_ulong {
    unsafe { sys::ee_uicpp_form_life_ctl_value(c, which) }
}
fn ctl_call(c: Handle, call: c_int, arg: c_ulong) -> c_long {
    unsafe { sys::ee_uicpp_form_life_ctl_call(c, call, arg) }
}
fn table_value(t: Handle, which: c_int) -> c_ulong {
    unsafe { sys::ee_uicpp_form_life_table_value(t, which) }
}

impl Lists for Raw {
    fn copy_len(&self) -> usize {
        unsafe { sys::ee_uicpp_form_life_copy_len(self.0) as usize }
    }
    fn copy_at(&self, i: usize) -> UiRect {
        unsafe { sys::ee_uicpp_form_life_copy_at(self.0, i as c_ulong) }
    }
    fn copy_erase(&mut self, i: usize) {
        unsafe { sys::ee_uicpp_form_life_copy_erase(self.0, i as c_ulong) }
    }
    fn copy_push(&mut self, r: UiRect) {
        unsafe { sys::ee_uicpp_form_life_copy_push(self.0, &r) }
    }
    fn dirty_len(&self) -> usize {
        unsafe { sys::ee_uicpp_form_life_dirty_len(self.0) as usize }
    }
    fn dirty_take_front(&mut self) -> DirtyRect {
        let mut d = DirtyRect::default();
        unsafe { sys::ee_uicpp_form_life_dirty_take(self.0, &mut d) };
        d
    }
    fn dirty_push_back(&mut self, d: DirtyRect) {
        unsafe { sys::ee_uicpp_form_life_dirty_push(self.0, &d) }
    }
}

impl LifeHost for Raw {
    fn surfaces(&self) -> c_ulong {
        self.value(0)
    }
    fn rect(&self) -> UiRect {
        unsafe { sys::ee_uicpp_form_life_rect(self.0) }
    }
    fn layout(&mut self, screen: UiRect) {
        unsafe { sys::ee_uicpp_form_life_layout(self.0, &screen) }
    }
    fn window_size(&self) -> (c_ulong, c_ulong) {
        (self.value(1), self.value(2))
    }
    fn now(&self) -> c_ulong {
        self.value(3)
    }
    fn open_time(&self) -> c_ulong {
        self.value(4)
    }
    fn set_open_time(&mut self, t: c_ulong) {
        unsafe { sys::ee_uicpp_form_life_set_value(self.0, 0, t) }
    }
    fn should_fade_in(&mut self) -> bool {
        self.value(7) != 0
    }
    fn set_fade_opacity(&mut self, v: f32) {
        unsafe { sys::ee_uicpp_form_life_set_fade(self.0, v) }
    }
    fn process_event(&mut self, which: FormEvent) -> Option<c_long> {
        let mut result = 0;
        unsafe { sys::ee_uicpp_form_life_event(self.0, which as c_int, &mut result) }.then_some(result)
    }
    fn manager_add_dirty(&mut self, r: UiRect) {
        unsafe { sys::ee_uicpp_form_life_manager_dirty(self.0, &r) }
    }
    fn manager_process_events(&mut self) {
        self.call(0, 0)
    }
    fn update_help_text(&mut self, force: bool) {
        self.call(1, force as usize)
    }
    fn reset_tabbing(&mut self) {
        self.call(2, 0)
    }
    fn control_count(&self) -> c_long {
        self.value(8) as c_long
    }
    fn control(&self, i: c_long) -> Handle {
        unsafe { sys::ee_uicpp_form_life_control(self.0, i) }
    }
    fn set_draw_control(&mut self, i: c_long, c: Handle) {
        unsafe { sys::ee_uicpp_form_life_set_draw(self.0, i, c) }
    }
    fn group_count(&self) -> c_long {
        self.value(9) as c_long
    }
    fn group_enabled(&self, g: c_long) -> bool {
        unsafe { sys::ee_uicpp_form_life_group_enabled(self.0, g) }
    }
    fn entity_count(&self, g: c_long) -> c_ulong {
        unsafe { sys::ee_uicpp_form_life_entity_count(self.0, g) }
    }
    fn entity(&self, g: c_long, j: c_ulong) -> Handle {
        unsafe { sys::ee_uicpp_form_life_entity(self.0, g, j) }
    }
    fn popup(&self) -> Handle {
        self.ptr(0)
    }
    fn set_popup_field(&mut self, c: Handle) {
        self.set_ptr(0, c)
    }
    fn set_copied_popup(&mut self, c: Handle) {
        self.set_ptr(2, c)
    }
    fn set_copied_popup_rect(&mut self, r: UiRect) {
        unsafe { sys::ee_uicpp_form_life_set_copied_popup_rect(self.0, &r) }
    }
    fn popup_draw_count(&self) -> c_ulong {
        self.value(5)
    }
    fn set_popup_draw_count(&mut self, n: c_ulong) {
        unsafe { sys::ee_uicpp_form_life_set_value(self.0, 1, n) }
    }
    fn help_label(&self) -> Handle {
        self.ptr(1)
    }
    fn set_help_label(&mut self, c: Handle) {
        self.set_ptr(1, c)
    }
    fn mouse_over(&self) -> Handle {
        self.ptr(2)
    }
    fn draw_shadow(&self) -> bool {
        self.value(6) != 0
    }
    fn background_count(&self) -> usize {
        self.value(10) as usize
    }
    fn background_open(&mut self, i: usize) {
        self.call(3, i)
    }
    fn background_close(&mut self, i: usize) {
        self.call(4, i)
    }
    fn db_count(&self) -> usize {
        self.value(11) as usize
    }
    fn db_table(&self, i: usize) -> Handle {
        unsafe { sys::ee_uicpp_form_life_db_table(self.0, i as c_ulong) }
    }
    fn db_control(&self, i: usize) -> Handle {
        unsafe { sys::ee_uicpp_form_life_db_control(self.0, i as c_ulong) }
    }
    fn set_db_control(&mut self, i: usize, c: Handle) {
        unsafe { sys::ee_uicpp_form_life_set_db_control(self.0, i as c_ulong, c) }
    }
    fn table_create_control(&mut self, t: Handle) -> Handle {
        unsafe { sys::ee_uicpp_form_life_create_control(self.0, t) }
    }
    fn table_group(&self, t: Handle) -> c_ulong {
        table_value(t, 0)
    }
    fn table_visible(&self, t: Handle) -> bool {
        table_value(t, 1) != 0
    }
    fn table_control_type(&self, t: Handle) -> c_ulong {
        table_value(t, 2)
    }
    fn table_string_id(&self, t: Handle) -> c_ulong {
        table_value(t, 3)
    }
    fn table_create_event(&mut self, t: Handle) {
        unsafe { sys::ee_uicpp_form_life_table_create_event(self.0, t) }
    }
    fn add_control(&mut self, c: Handle, group: c_ulong) {
        self.control_call(0, c, group)
    }
    fn remove_control(&mut self, c: Handle, group: c_ulong) {
        self.control_call(1, c, group)
    }
    fn delete_control(&mut self, c: Handle) {
        self.control_call(2, c, 0)
    }
    fn event_data_count(&self) -> usize {
        self.value(12) as usize
    }
    fn event_data(&self, i: usize) -> (c_ulong, c_ulong) {
        let mut kind = 0;
        let id = unsafe { sys::ee_uicpp_form_life_event_data(self.0, i as c_ulong, &mut kind) };
        (id, kind)
    }
    fn create_form_event(&mut self, id: c_ulong) -> Handle {
        unsafe { sys::ee_uicpp_form_life_create_form_event(self.0, id) }
    }
    fn set_form_event(&mut self, kind: c_ulong, e: Handle) {
        unsafe { sys::ee_uicpp_form_life_set_form_event(self.0, kind, e) }
    }
    fn ctl_entity_type(&self, c: Handle) -> c_ulong {
        ctl_value(c, 0)
    }
    fn ctl_mode(&self, c: Handle) -> c_ulong {
        ctl_value(c, 1)
    }
    fn ctl_dirty(&self, c: Handle) -> bool {
        ctl_value(c, 2) != 0
    }
    fn ctl_rect(&self, c: Handle) -> UiRect {
        unsafe { sys::ee_uicpp_form_life_ctl_rect(c) }
    }
    fn ctl_draw_count(&self, c: Handle) -> c_ulong {
        ctl_value(c, 3)
    }
    fn ctl_index(&self, c: Handle) -> c_ulong {
        ctl_value(c, 4)
    }
    fn ctl_copy_data(&mut self, c: Handle) -> c_long {
        ctl_call(c, 0, 0)
    }
    fn ctl_set_dirty(&mut self, c: Handle, dirty: bool) {
        ctl_call(c, 1, dirty as c_ulong);
    }
    fn ctl_set_display_mode(&mut self, c: Handle, mode: c_ulong) {
        ctl_call(c, 2, mode);
    }
    fn ctl_hide(&mut self, c: Handle) {
        ctl_call(c, 3, 0);
    }
    fn ctl_set_string_id(&mut self, c: Handle, id: c_ulong) {
        ctl_call(c, 4, id);
    }
}

/// `UIForm::AddCopyDirtyRectangle(rect)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_add_copy_dirty_rectangle(form: *mut c_void, rect: *const UiRect) {
    form_state::add_copy_dirty_rectangle(&mut Raw(form), rect.read());
}

/// `UIForm::AddDirtyRectangle(rect)` after its popup test (`surfaces`: the manager's surface count).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_add_dirty_rectangle(form: *mut c_void, rect: *const UiRect, surfaces: c_ulong) {
    form_state::add_dirty_rectangle(&mut Raw(form), rect.read(), surfaces);
}

/// `UIForm::CopyData` -> its result.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_copy_data(form: *mut c_void) -> c_long {
    form_state::copy_data(&mut Raw(form))
}

/// `UIForm::Open`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_open(form: *mut c_void) {
    form_state::open(&mut Raw(form));
}

/// `UIForm::Close`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_close(form: *mut c_void) {
    form_state::close(&mut Raw(form));
}

/// `UIForm::SetPopup(control)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_set_popup(form: *mut c_void, control: *mut c_void) {
    form_state::set_popup(&mut Raw(form), control);
}

/// `UIForm::RedrawIntersectControls(rect)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_redraw_intersect_controls(form: *mut c_void, rect: *const UiRect) {
    form_state::redraw_intersect_controls(&mut Raw(form), rect.read());
}

/// `UIForm::SetAllControlsDirty`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_set_all_controls_dirty(form: *mut c_void) {
    form_state::set_all_controls_dirty(&mut Raw(form));
}

/// `UIForm::GetToolTipShadowRectangle(rect, out)`.
#[no_mangle]
pub extern "C" fn ee_ui_form_tool_tip_shadow(rect: UiRect) -> UiRect {
    form_state::tool_tip_shadow(&rect)
}

/// `UIForm::SetHelpTextPopupLocation`'s point for the mouse, the tool tip label's rectangle and the form's.
#[no_mangle]
pub extern "C" fn ee_ui_form_help_location(
    mouse: crate::rect::UiPoint,
    label: UiRect,
    form: UiRect,
) -> crate::rect::UiPoint {
    form_state::help_popup_location(mouse, label, form)
}
