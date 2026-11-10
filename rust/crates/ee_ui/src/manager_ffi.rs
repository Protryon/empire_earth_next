//! `extern "C"` for `UIManager`'s form stack and event queue (`User Interface/UIManager.cpp`): `manager` is the C++
//! `UIManager*`, asked about its lists and forms through `ee_uicpp_manager_*` (UIManager.cpp) on each step, never
//! cached. The forms are opaque handles.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::manager::{self, EventHost, Form, StackHost, Trigger};
use crate::rect::UiRect;

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_ulong, c_void};

    use crate::manager::Trigger;
    use crate::rect::UiRect;

    extern "C" {
        // list: 0 the stack, 1 the visible forms, 2 every form
        pub fn ee_uicpp_manager_len(manager: *mut c_void, list: c_int) -> c_ulong;
        pub fn ee_uicpp_manager_at(manager: *mut c_void, list: c_int, index: c_ulong) -> *mut c_void;
        pub fn ee_uicpp_manager_push(manager: *mut c_void, list: c_int, form: *mut c_void);
        pub fn ee_uicpp_manager_pop(manager: *mut c_void, list: c_int);
        pub fn ee_uicpp_manager_insert(manager: *mut c_void, list: c_int, index: c_ulong, form: *mut c_void);
        pub fn ee_uicpp_manager_erase(manager: *mut c_void, list: c_int, index: c_ulong);
        pub fn ee_uicpp_manager_copied_clear(manager: *mut c_void);
        pub fn ee_uicpp_manager_form_rect(form: *mut c_void) -> UiRect;
        pub fn ee_uicpp_manager_form_id(form: *mut c_void) -> c_ulong;
        // call: 0 Open, 1 Close, 2 Activate, 3 Deactivate, 4 ClearAllClipRectangles, 5 SetAllControlsDirty
        pub fn ee_uicpp_manager_form_call(form: *mut c_void, call: c_int);
        // call: 0 AddDirtyRectangle, 1 RedrawIntersectControls, 2 AddClipRectangle, 3 RelayoutForm
        pub fn ee_uicpp_manager_form_rect_call(form: *mut c_void, call: c_int, rect: *const UiRect);
        pub fn ee_uicpp_manager_event_take(manager: *mut c_void, trigger: *mut Trigger) -> bool;
        pub fn ee_uicpp_manager_event_process(trigger: *const Trigger);
        pub fn ee_uicpp_manager_event_push(manager: *mut c_void, trigger: *const Trigger);
    }
}

const STACK: i32 = 0;
const VISIBLE: i32 = 1;
const FORMS: i32 = 2;

struct Raw(*mut c_void);

impl StackHost for Raw {
    fn stack_len(&self) -> usize {
        unsafe { sys::ee_uicpp_manager_len(self.0, STACK) as usize }
    }
    fn stack(&self, i: usize) -> Form {
        unsafe { sys::ee_uicpp_manager_at(self.0, STACK, i as c_ulong) }
    }
    fn stack_push(&mut self, form: Form) {
        unsafe { sys::ee_uicpp_manager_push(self.0, STACK, form) }
    }
    fn stack_pop(&mut self) {
        unsafe { sys::ee_uicpp_manager_pop(self.0, STACK) }
    }
    fn visible_len(&self) -> usize {
        unsafe { sys::ee_uicpp_manager_len(self.0, VISIBLE) as usize }
    }
    fn visible(&self, i: usize) -> Form {
        unsafe { sys::ee_uicpp_manager_at(self.0, VISIBLE, i as c_ulong) }
    }
    fn visible_insert(&mut self, i: usize, form: Form) {
        unsafe { sys::ee_uicpp_manager_insert(self.0, VISIBLE, i as c_ulong, form) }
    }
    fn visible_erase(&mut self, i: usize) {
        unsafe { sys::ee_uicpp_manager_erase(self.0, VISIBLE, i as c_ulong) }
    }
    fn visible_push(&mut self, form: Form) {
        unsafe { sys::ee_uicpp_manager_push(self.0, VISIBLE, form) }
    }
    fn visible_pop(&mut self) {
        unsafe { sys::ee_uicpp_manager_pop(self.0, VISIBLE) }
    }
    fn forms_len(&self) -> usize {
        unsafe { sys::ee_uicpp_manager_len(self.0, FORMS) as usize }
    }
    fn forms(&self, i: usize) -> Form {
        unsafe { sys::ee_uicpp_manager_at(self.0, FORMS, i as c_ulong) }
    }
    fn copied_clear(&mut self) {
        unsafe { sys::ee_uicpp_manager_copied_clear(self.0) }
    }
    fn rect(&self, form: Form) -> UiRect {
        unsafe { sys::ee_uicpp_manager_form_rect(form) }
    }
    fn form_id(&self, form: Form) -> c_ulong {
        unsafe { sys::ee_uicpp_manager_form_id(form) }
    }
    fn open(&mut self, form: Form) {
        unsafe { sys::ee_uicpp_manager_form_call(form, 0) }
    }
    fn close(&mut self, form: Form) {
        unsafe { sys::ee_uicpp_manager_form_call(form, 1) }
    }
    fn activate(&mut self, form: Form) {
        unsafe { sys::ee_uicpp_manager_form_call(form, 2) }
    }
    fn deactivate(&mut self, form: Form) {
        unsafe { sys::ee_uicpp_manager_form_call(form, 3) }
    }
    fn clear_clips(&mut self, form: Form) {
        unsafe { sys::ee_uicpp_manager_form_call(form, 4) }
    }
    fn set_all_dirty(&mut self, form: Form) {
        unsafe { sys::ee_uicpp_manager_form_call(form, 5) }
    }
    fn add_dirty(&mut self, form: Form, rect: UiRect) {
        unsafe { sys::ee_uicpp_manager_form_rect_call(form, 0, &rect) }
    }
    fn redraw_intersect(&mut self, form: Form, rect: UiRect) {
        unsafe { sys::ee_uicpp_manager_form_rect_call(form, 1, &rect) }
    }
    fn add_clip(&mut self, form: Form, rect: UiRect) {
        unsafe { sys::ee_uicpp_manager_form_rect_call(form, 2, &rect) }
    }
    fn relayout(&mut self, form: Form, rect: UiRect) {
        unsafe { sys::ee_uicpp_manager_form_rect_call(form, 3, &rect) }
    }
}

impl EventHost for Raw {
    fn take_front(&mut self) -> Option<Trigger> {
        let mut t =
            Trigger { event: core::ptr::null_mut(), form: core::ptr::null_mut(), entity: core::ptr::null_mut() };
        unsafe { sys::ee_uicpp_manager_event_take(self.0, &mut t) }.then_some(t)
    }
    fn process(&mut self, trigger: Trigger) {
        unsafe { sys::ee_uicpp_manager_event_process(&trigger) }
    }
    fn push_back(&mut self, trigger: Trigger) {
        unsafe { sys::ee_uicpp_manager_event_push(self.0, &trigger) }
    }
}

/// `UIManager::AddDirtyRectangle(rect, form)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_add_dirty_rectangle(
    manager: *mut c_void,
    rect: *const UiRect,
    form: *mut c_void,
) {
    manager::add_dirty_rectangle(&mut Raw(manager), rect.read(), form);
}

/// `UIManager::Pop`'s body (inside the locks).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_pop(manager: *mut c_void) {
    manager::pop(&mut Raw(manager));
}

/// `UIManager::PopTo`'s body (inside the locks).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_pop_to(manager: *mut c_void, form: *mut c_void) {
    manager::pop_to(&mut Raw(manager), form);
}

/// `UIManager::Push`'s body (inside the locks).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_push(manager: *mut c_void, form: *mut c_void) {
    manager::push(&mut Raw(manager), form);
}

/// `UIManager::SwapForm`'s body (inside the locks).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_swap_form(manager: *mut c_void, form: *mut c_void) {
    manager::swap_form(&mut Raw(manager), form);
}

/// `UIManager::RedrawAll`'s body (inside the lock).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_redraw_all(manager: *mut c_void) {
    manager::redraw_all(&mut Raw(manager));
}

/// `UIManager::RelayoutAllForms`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_relayout_all_forms(
    manager: *mut c_void,
    window_width: c_ulong,
    window_height: c_ulong,
) {
    manager::relayout_all_forms(&mut Raw(manager), window_width, window_height);
}

/// `UIManager::ResetClippedForms`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_reset_clipped_forms(manager: *mut c_void) {
    manager::reset_clipped_forms(&mut Raw(manager));
}

/// `UIManager::GetFormByID` -> the form, or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_form_by_id(manager: *mut c_void, id: c_ulong) -> *mut c_void {
    manager::form_by_id(&Raw(manager), id).unwrap_or(core::ptr::null_mut())
}

/// `UIManager::GetMouseWheelScrollAmount`'s step from the wheel's normalized value.
#[no_mangle]
pub extern "C" fn ee_ui_manager_wheel_scroll_amount(value: c_long) -> c_long {
    manager::wheel_scroll_amount(value)
}

/// `UpdateUI`'s event loop.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_process_all_events(manager: *mut c_void) {
    manager::process_all_events(&mut Raw(manager));
}

/// `UIManager::ProcessAllEventsForForm(form)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_process_all_events_for_form(manager: *mut c_void, form: *mut c_void) {
    manager::process_all_events_for_form(&mut Raw(manager), form);
}
