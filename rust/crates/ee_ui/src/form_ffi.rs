//! `extern "C"` for `UIForm`'s focus walk and hit test (`User Interface/UIForm.cpp`): `form` is the C++ `UIForm*`,
//! asked about its controls and groups through `ee_uicpp_form_*` (UIForm.cpp) on each step, never cached.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::form::{self, FormHost, HitHost};
use crate::rect::{UiPoint, UiRect};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_long, c_ulong, c_void};

    use crate::rect::UiRect;

    extern "C" {
        pub fn ee_uicpp_form_count(form: *mut c_void) -> c_long;
        pub fn ee_uicpp_form_exists(form: *mut c_void, index: c_long) -> bool;
        pub fn ee_uicpp_form_kind(form: *mut c_void, index: c_long) -> c_ulong;
        pub fn ee_uicpp_form_checked(form: *mut c_void, index: c_long) -> bool;
        pub fn ee_uicpp_form_shown_enabled(form: *mut c_void, index: c_long) -> bool;
        pub fn ee_uicpp_form_tabable(form: *mut c_void, index: c_long) -> bool;
        pub fn ee_uicpp_form_set_tab_stop(form: *mut c_void, index: c_ulong) -> bool;
        pub fn ee_uicpp_form_group_count(form: *mut c_void) -> c_long;
        pub fn ee_uicpp_form_group_enabled(form: *mut c_void, group: c_long) -> bool;
        pub fn ee_uicpp_form_entity_count(form: *mut c_void, group: c_long) -> c_ulong;
        pub fn ee_uicpp_form_entity_hidden(form: *mut c_void, group: c_long, entity: c_ulong) -> bool;
        pub fn ee_uicpp_form_entity_rect(form: *mut c_void, group: c_long, entity: c_ulong) -> UiRect;
    }
}

struct Raw(*mut c_void);

impl FormHost for Raw {
    fn count(&self) -> c_long {
        unsafe { sys::ee_uicpp_form_count(self.0) }
    }
    fn exists(&self, i: c_long) -> bool {
        unsafe { sys::ee_uicpp_form_exists(self.0, i) }
    }
    fn kind(&self, i: c_long) -> c_ulong {
        unsafe { sys::ee_uicpp_form_kind(self.0, i) }
    }
    fn checked(&self, i: c_long) -> bool {
        unsafe { sys::ee_uicpp_form_checked(self.0, i) }
    }
    fn shown_enabled(&self, i: c_long) -> bool {
        unsafe { sys::ee_uicpp_form_shown_enabled(self.0, i) }
    }
    fn tabable(&self, i: c_long) -> bool {
        unsafe { sys::ee_uicpp_form_tabable(self.0, i) }
    }
    fn set_tab_stop(&mut self, i: c_ulong) -> bool {
        unsafe { sys::ee_uicpp_form_set_tab_stop(self.0, i) }
    }
}

impl HitHost for Raw {
    fn group_count(&self) -> c_long {
        unsafe { sys::ee_uicpp_form_group_count(self.0) }
    }
    fn group_enabled(&self, g: c_long) -> bool {
        unsafe { sys::ee_uicpp_form_group_enabled(self.0, g) }
    }
    fn entity_count(&self, g: c_long) -> c_ulong {
        unsafe { sys::ee_uicpp_form_entity_count(self.0, g) }
    }
    fn entity_hidden(&self, g: c_long, j: c_ulong) -> bool {
        unsafe { sys::ee_uicpp_form_entity_hidden(self.0, g, j) }
    }
    fn entity_rect(&self, g: c_long, j: c_ulong) -> UiRect {
        unsafe { sys::ee_uicpp_form_entity_rect(self.0, g, j) }
    }
}

/// `UIForm::CheckedRadioButton`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_checked_radio_button(form: *mut c_void, index: *mut c_long) {
    let mut i = index.read();
    form::checked_radio_button(&Raw(form), &mut i);
    index.write(i);
}

/// `UIForm::Tab`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_tab(form: *mut c_void, tabbing: bool, tab_stop: c_ulong) {
    form::tab(&mut Raw(form), tabbing, tab_stop);
}

/// `UIForm::ShiftTab`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_shift_tab(form: *mut c_void, tabbing: bool, tab_stop: c_ulong) {
    form::shift_tab(&mut Raw(form), tabbing, tab_stop);
}

/// `UIForm::UpdateMouseLocation`'s search -> whether a control was found, at `(*group, *entity)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_hit(
    form: *mut c_void,
    at: UiPoint,
    group: *mut c_long,
    entity: *mut c_ulong,
) -> bool {
    match form::hit(&Raw(form), at) {
        Some((g, j)) => {
            group.write(g);
            entity.write(j);
            true
        }
        None => false,
    }
}
