//! The editor's Rust objects as the composed hosts (`editor_sefmx.rs`, `editor_tabsx.rs`) call them: the tab groups,
//! the form, the unit, player, trigger, area, effect and map tabs (their `extern "C"` entries, by handle). The unit
//! tests build without the FFI modules: there every call is a no-op answering 0 (the composers' tests pass NULL
//! objects, where the entries answer 0 too).

#![allow(clippy::too_many_arguments)]

pub(crate) use real::*;

#[cfg(not(test))]
mod real {
    use core::ffi::c_void;

    use crate::screens::{
        editor_area_ffi, editor_effects_ffi, editor_form_ffi, editor_groups_ffi, editor_map_ffi, editor_players_ffi,
        editor_tscreen_ffi, editor_unit_ffi, editor_whole_ffi,
    };

    pub use editor_players_ffi::{
        EE_SCREENS_SEPL_FILL_TECH_LISTS, EE_SCREENS_SEPL_PLAYER_COUNT, EE_SCREENS_SEPL_REBUILD_TECH_LISTS,
        EE_SCREENS_SEPL_REFRESH,
    };
    pub use editor_tscreen_ffi::{
        EE_SCREENS_SETS_ALPHA_SORT, EE_SCREENS_SETS_DEC_PRIORITY, EE_SCREENS_SETS_DELETE_ENTRY,
        EE_SCREENS_SETS_DUPLICATE_ENTRY, EE_SCREENS_SETS_INC_PRIORITY, EE_SCREENS_SETS_NEW_ENTRY,
        EE_SCREENS_SETS_SELECT_ENTRY, EE_SCREENS_SETS_SELECT_NAME, EE_SCREENS_SETS_SHOW_TAB,
    };
    pub use editor_unit_ffi::{
        EE_SCREENS_SEUN_REFRESH, EE_SCREENS_SEUN_RESELECT, EE_SCREENS_SEUN_SHOW_CARGO, EE_SCREENS_SEUN_SHOW_UNIT,
    };

    // SAFETY (every entry below): the handles are the live objects (or NULL) the caller names
    pub fn segr_tab(g: usize) -> usize {
        unsafe { editor_groups_ffi::ee_screens_segr_tab(g as *const _) as usize }
    }
    pub fn segr_call(g: usize, slot: i32, x: f32, y: f32, p: usize) {
        unsafe { editor_groups_ffi::ee_screens_segr_call(g as *const _, slot, x, y, p as *mut c_void) }
    }
    pub fn seun_call(tab: usize, what: u32, a: i32, p: usize) {
        unsafe { editor_unit_ffi::ee_screens_seun_call(tab as *const _, what, a, 0.0, 0.0, p as *mut c_void) }
    }
    pub fn sepl_call(tab: usize, what: u32) -> u32 {
        unsafe { editor_players_ffi::ee_screens_sepl_call(tab as *const _, what, 0, 0) }
    }
    pub fn sefm_get(form: usize, slot: u32) -> usize {
        unsafe { editor_form_ffi::ee_screens_sefm_get(form as *const _, slot) }
    }
    pub fn sefm_set(form: usize, slot: u32, value: usize) {
        unsafe { editor_form_ffi::ee_screens_sefm_set(form as *const _, slot, value) }
    }
    pub fn sefm_call(form: usize, what: u32, a: i32, p: usize, r: *const f32) -> i32 {
        unsafe { editor_form_ffi::ee_screens_sefm_call(form as *const _, what, a, p as *mut c_void, r) }
    }
    pub fn sefm_selected(form: usize, index: isize) -> u32 {
        unsafe { editor_form_ffi::ee_screens_sefm_selected(form as *const _, index) }
    }
    pub fn sets_call(tab: usize, what: u32, a: u32) {
        unsafe { editor_tscreen_ffi::ee_screens_sets_call(tab as *const _, what, a, 0.0, 0.0, core::ptr::null_mut()) }
    }
    pub fn sets_sub_op(tab: usize, k: u32, what: u32, a: u32, b: u32) -> u32 {
        unsafe { editor_tscreen_ffi::ee_screens_sets_sub_op(tab as *const _, k, what, a, b) }
    }
    pub fn sets_sub(tab: usize, k: u32) -> usize {
        unsafe {
            editor_tscreen_ffi::ee_screens_sets_sub(tab as *const _, k, core::ptr::null_mut(), core::ptr::null_mut())
                as usize
        }
    }
    pub fn sear_center(screen: usize, out: &mut [f32; 2]) -> bool {
        unsafe { editor_area_ffi::ee_screens_sear_center(screen as *const _, out.as_mut_ptr()) }
    }
    pub fn seet_event(types: usize, controls: usize, what: u32, a: u32, b: u32) {
        unsafe { editor_effects_ffi::ee_screens_seet_event(types as *const _, controls as *mut c_void, what, a, b) }
    }
    pub fn semp_call(tab: usize, what: u32, a: i32) {
        unsafe { editor_map_ffi::ee_screens_semp_call(tab as *const _, what, a) }
    }
    pub fn see_viewport(event: usize, rectangle: &[f32; 4], hidden: bool) {
        unsafe { editor_whole_ffi::ee_screens_see_viewport(event as *mut _, rectangle.as_ptr(), hidden as u8) }
    }
}

#[cfg(test)]
mod real {
    // (the FFI modules' numbers, which the tests do not build)
    pub(crate) const EE_SCREENS_SEPL_REFRESH: u32 = 5;
    pub(crate) const EE_SCREENS_SEPL_FILL_TECH_LISTS: u32 = 8;
    pub(crate) const EE_SCREENS_SEPL_REBUILD_TECH_LISTS: u32 = 9;
    pub(crate) const EE_SCREENS_SEPL_PLAYER_COUNT: u32 = 11;
    pub(crate) const EE_SCREENS_SETS_ALPHA_SORT: u32 = 1;
    pub(crate) const EE_SCREENS_SETS_SELECT_ENTRY: u32 = 2;
    pub(crate) const EE_SCREENS_SETS_DEC_PRIORITY: u32 = 5;
    pub(crate) const EE_SCREENS_SETS_DELETE_ENTRY: u32 = 6;
    pub(crate) const EE_SCREENS_SETS_DUPLICATE_ENTRY: u32 = 7;
    pub(crate) const EE_SCREENS_SETS_SHOW_TAB: u32 = 8;
    pub(crate) const EE_SCREENS_SETS_INC_PRIORITY: u32 = 16;
    pub(crate) const EE_SCREENS_SETS_NEW_ENTRY: u32 = 18;
    pub(crate) const EE_SCREENS_SETS_SELECT_NAME: u32 = 20;
    pub(crate) const EE_SCREENS_SEUN_RESELECT: u32 = 9;
    pub(crate) const EE_SCREENS_SEUN_REFRESH: u32 = 12;
    pub(crate) const EE_SCREENS_SEUN_SHOW_CARGO: u32 = 13;
    pub(crate) const EE_SCREENS_SEUN_SHOW_UNIT: u32 = 15;

    pub(crate) fn segr_tab(_: usize) -> usize {
        0
    }
    pub(crate) fn segr_call(_: usize, _: i32, _: f32, _: f32, _: usize) {}
    pub(crate) fn seun_call(_: usize, _: u32, _: i32, _: usize) {}
    pub(crate) fn sepl_call(_: usize, _: u32) -> u32 {
        0
    }
    pub(crate) fn sefm_get(_: usize, _: u32) -> usize {
        0
    }
    pub(crate) fn sefm_set(_: usize, _: u32, _: usize) {}
    pub(crate) fn sefm_call(_: usize, _: u32, _: i32, _: usize, _: *const f32) -> i32 {
        0
    }
    pub(crate) fn sefm_selected(_: usize, _: isize) -> u32 {
        0
    }
    pub(crate) fn sets_call(_: usize, _: u32, _: u32) {}
    pub(crate) fn sets_sub_op(_: usize, _: u32, _: u32, _: u32, _: u32) -> u32 {
        0
    }
    pub(crate) fn sets_sub(_: usize, _: u32) -> usize {
        0
    }
    pub(crate) fn sear_center(_: usize, _: &mut [f32; 2]) -> bool {
        false
    }
    pub(crate) fn seet_event(_: usize, _: usize, _: u32, _: u32, _: u32) {}
    pub(crate) fn semp_call(_: usize, _: u32, _: i32) {}
    pub(crate) fn see_viewport(_: usize, _: &[f32; 4], _: bool) {}
}
