//! `extern "C"` for `hud_more.rs` (UI lane round 13): each C++ member's one forward; the objects cross as their
//! addresses (`EEUIFMGAttribute*`, `EEUIFMGStatusButton*`, the font and the colour).

use crate::screens::game_cpp::Ffi;
use crate::screens::hud_more as b;

#[no_mangle]
pub extern "C" fn ee_screens_att_construct(attr: usize, font: usize, color: usize) {
    b::attribute_construct(&mut Ffi, attr, font, color)
}

#[no_mangle]
pub extern "C" fn ee_screens_att_copy_data(attr: usize) -> i64 {
    b::attribute_copy_data(&mut Ffi, attr)
}

#[no_mangle]
pub extern "C" fn ee_screens_att_draw_text(attr: usize) -> i64 {
    b::attribute_draw_text(&mut Ffi, attr)
}

#[no_mangle]
pub extern "C" fn ee_screens_sbm_right_down(button: usize) -> i64 {
    b::status_right_down(&mut Ffi, button)
}

#[no_mangle]
pub extern "C" fn ee_screens_sbm_tabbed_from(button: usize) {
    b::status_tabbed_from(&mut Ffi, button)
}

#[no_mangle]
pub extern "C" fn ee_screens_mff_load(info: usize, button: usize, major: u32, minor: u32, file: usize) -> i64 {
    b::main_load(&mut Ffi, info, button, major, minor, file)
}

#[no_mangle]
pub extern "C" fn ee_screens_mff_save(info: bool, button: bool, file: usize) -> i64 {
    b::main_save(&mut Ffi, info, button, file)
}

#[no_mangle]
pub extern "C" fn ee_screens_mff_set_error(form: usize, index: u32, text: usize) {
    b::main_set_error(&mut Ffi, form, index, text)
}
