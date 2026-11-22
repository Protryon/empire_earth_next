//! `extern "C"` for the HUD classes' bodies (`hud_bodies.rs`, UI lane round 12): each C++ member's one forward. The
//! objects cross as addresses (`EEUIFMainGame*`, `EEUIBUpgrade*`, `EECBUIListBox*` as `UIControl*`'s address, the
//! table, the lines' `std::vector<UWideString>*`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;

use crate::screens::civ_whole_ffi::{ee_screens_cbl_add_line, ee_screens_cbl_change_selection};
use crate::screens::game_cpp::Ffi;
use crate::screens::hud_bodies as b;

#[no_mangle]
pub extern "C" fn ee_screens_mfb_create_group(form: usize, index: u32) {
    b::main_create_group(&mut Ffi, form, index)
}

#[no_mangle]
pub extern "C" fn ee_screens_mfb_in_game_view(form: usize, x: f32, y: f32) -> bool {
    b::main_in_game_view(&mut Ffi, form, x, y)
}

#[no_mangle]
pub extern "C" fn ee_screens_mfb_destroy(form: usize) {
    b::main_destroy(&mut Ffi, form)
}

#[no_mangle]
pub extern "C" fn ee_screens_upb_construct(button: usize) {
    b::upgrade_construct(&mut Ffi, button)
}

#[no_mangle]
pub extern "C" fn ee_screens_upb_button_down(button: usize) {
    b::upgrade_button_down(&mut Ffi, button)
}

#[no_mangle]
pub extern "C" fn ee_screens_upb_copy_data(button: usize) -> i64 {
    b::upgrade_copy_data(&mut Ffi, button)
}

#[no_mangle]
pub extern "C" fn ee_screens_upb_draw_text(button: usize) -> i64 {
    b::upgrade_draw_text(&mut Ffi, button)
}

#[no_mangle]
pub extern "C" fn ee_screens_upb_mouse_over(button: usize) {
    b::upgrade_mouse_over(&mut Ffi, button)
}

#[no_mangle]
pub extern "C" fn ee_screens_upb_right_down(button: usize) -> i64 {
    b::upgrade_right_down(&mut Ffi, button)
}

#[no_mangle]
pub extern "C" fn ee_screens_upb_set_dirty(button: usize, dirty: bool) {
    b::upgrade_set_dirty(&mut Ffi, button, dirty)
}

#[no_mangle]
pub extern "C" fn ee_screens_cbb_construct(table: usize, list: usize, tabable: bool) {
    b::cb_list_construct(&mut Ffi, table, list, tabable, |l| {
        ee_screens_cbl_change_selection(l as *mut c_void, 0, false, false)
    })
}

#[no_mangle]
pub extern "C" fn ee_screens_cbb_append(list: usize, lines: usize) {
    // SAFETY: the line's bytes live for the call
    b::cb_list_append(&mut Ffi, list, lines, |l, t| unsafe {
        ee_screens_cbl_add_line(l as *mut c_void, t.as_ptr(), t.len())
    })
}

#[no_mangle]
pub extern "C" fn ee_screens_cbb_copy_data(list: usize) -> i64 {
    b::cb_list_copy_data(&mut Ffi, list)
}
