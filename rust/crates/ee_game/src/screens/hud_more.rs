//! More HUD bodies in Rust over one-statement shims (UI lane round 13): `EEUIFMGAttribute`'s constructor body,
//! `CopyData` and `DrawTextA` (the attribute button's level text: the number of queued upgrades in its corner) and
//! `EEUIFMGStatusButton`'s `MouseRButtonDown` and `TabbedFrom`; `EEUIFMainGame`'s `Load`, `Save` (the two blinking
//! flags of a saved game) and `SetErrorString`. Each C++ member is one forward; the calls are the
//! original's, in its order.

// the forwards are the FFI module's (absent from a test binary)
#![cfg_attr(test, allow(dead_code))]

use crate::screens::game_cpp::{Cpp, EeScreensShimText};

/// `kButtonStatePressed` (lle/UIButtonState.h).
const BUTTON_STATE_PRESSED: u32 = 2;

/// The attribute button's constructor body: a transparent level text of the font in the text colour, placed two
/// border widths into the button's top-left corner.
pub(crate) fn attribute_construct<C: Cpp + ?Sized>(c: &mut C, attr: usize, font: usize, color: usize) {
    c.att_new_text(attr, font);
    c.att_text_transparent(attr);
    c.att_text_fg(attr, color);
    let x = c.att_left(attr) + c.att_border(attr) as f32 * 2.0;
    let y = c.att_top(attr) + c.att_border(attr) as f32 * 2.0;
    c.att_text_set_location(attr, x, y);
}

/// `CopyData`: the upgrade button's, then the level text shows the queued count when there is one.
pub(crate) fn attribute_copy_data<C: Cpp + ?Sized>(c: &mut C, attr: usize) -> i64 {
    let result = c.att_base_copy_data(attr);
    let queued = c.att_queued(attr);
    if queued > 0 {
        // PlatIToA(queued, buffer, 10): the decimal digits
        let digits = queued.to_string();
        c.att_level_text(attr, EeScreensShimText::of(digits.as_bytes()));
        c.att_set_show(attr, true);
    } else {
        c.att_set_show(attr, false);
    }
    result
}

/// `DrawTextA`: the upgrade button's, then the level text clipped to the button's size from its location.
pub(crate) fn attribute_draw_text<C: Cpp + ?Sized>(c: &mut C, attr: usize) -> i64 {
    let mut bottom = c.att_text_y(attr);
    let mut right = c.att_text_x(attr);
    bottom += c.att_height(attr) as f32;
    right += c.att_width(attr) as f32;
    let (left, top) = (c.att_text_x(attr), c.att_text_y(attr));
    let mut result = c.att_base_draw_text(attr);
    if c.att_show(attr) {
        result = c.att_draw_level(attr, left, top, right, bottom);
    }
    result
}

/// `EEUIFMGStatusButton::MouseRButtonDown`: a tech button captures the mouse and is pressed (the base's press).
pub(crate) fn status_right_down<C: Cpp + ?Sized>(c: &mut C, button: usize) -> i64 {
    if !c.sbm_is_command(button) {
        c.sbm_capture_mouse(button);
        c.sbm_base_button_down(button);
    }
    0
}

/// `EEUIFMGStatusButton::TabbedFrom`: a pressed button is released as from its hot key, else the base's.
pub(crate) fn status_tabbed_from<C: Cpp + ?Sized>(c: &mut C, button: usize) {
    if c.sbm_state(button) == BUTTON_STATE_PRESSED {
        c.sbm_hot_key_up(button);
    } else {
        c.sbm_base_tabbed_from(button);
    }
}

/// `EEUIFMainGame::Load(major, minor, file)`: the information and civilization builder blinking flags, from saved
/// games of version 1.210 on (`major > 1 || (major == 0 && minor >= 0xd2)`, the original's test), each read only
/// after the last succeeded.
pub(crate) fn main_load<C: Cpp + ?Sized>(
    c: &mut C,
    info: usize,
    button: usize,
    major: u32,
    minor: u32,
    file: usize,
) -> i64 {
    let mut result = 0;
    if major > 1 || (major == 0 && minor >= 0xd2) {
        result = c.mff_read_bool(file, info);
        if result == 0 {
            result = c.mff_read_bool(file, button);
        }
    }
    result
}

/// `EEUIFMainGame::Save(file)`: the two flags, the second only after the first was written.
pub(crate) fn main_save<C: Cpp + ?Sized>(c: &mut C, info: bool, button: bool, file: usize) -> i64 {
    let mut result = c.mff_write_bool(file, info);
    if result == 0 {
        result = c.mff_write_bool(file, button);
    }
    result
}

/// `EEUIFMainGame::SetErrorString(text, index)`: the error line stored under the strings' lock, marked changed.
pub(crate) fn main_set_error<C: Cpp + ?Sized>(c: &mut C, form: usize, index: u32, text: usize) {
    c.mff_error_enter(form);
    c.mff_error_set(form, index, text);
    c.mff_strings_changed(form);
    c.mff_error_leave(form);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_constructor_places_the_level_text() {
        let mut fake = Fake::default();
        fake.answer("att_left", &[10]).answer("att_top", &[20]).answer("att_border", &[3, 3]);
        attribute_construct(&mut fake, 7, 8, 9);
        assert_eq!(
            fake.calls(),
            "att_new_text(7, 8)\natt_text_transparent(7)\natt_text_fg(7, 9)\natt_left(7)\natt_border(7)\natt_top(7)\n\
             att_border(7)\natt_text_set_location(7, 16.0, 26.0)"
        );
    }

    #[test]
    fn copy_data_shows_the_queued_count() {
        let mut fake = Fake::default();
        fake.answer("att_base_copy_data", &[5, 6]).answer("att_queued", &[12, 0]);
        assert_eq!(attribute_copy_data(&mut fake, 7), 5);
        assert_eq!(attribute_copy_data(&mut fake, 7), 6);
        let calls = fake.calls();
        assert!(calls.contains("att_level_text(7, \"12\")\natt_set_show(7, true)"), "{calls}");
        assert!(calls.ends_with("att_queued(7)\natt_set_show(7, false)"), "{calls}");
    }

    #[test]
    fn draw_text_draws_the_level_only_when_shown() {
        let mut fake = Fake::default();
        fake.answer("att_text_y", &[2, 2]).answer("att_text_x", &[1, 1]).answer("att_height", &[30]);
        fake.answer("att_width", &[40]).answer("att_base_draw_text", &[4]).answer("att_show", &[1]);
        fake.answer("att_draw_level", &[9]);
        assert_eq!(attribute_draw_text(&mut fake, 7), 9);
        assert!(fake.calls().ends_with("att_draw_level(7, 1.0, 2.0, 41.0, 32.0)"), "{}", fake.calls());
        let mut fake = Fake::default();
        fake.answer("att_base_draw_text", &[4]);
        assert_eq!(attribute_draw_text(&mut fake, 7), 4);
        assert!(!fake.calls().contains("att_draw_level"));
    }

    #[test]
    fn the_status_button_right_press_and_tab() {
        let mut fake = Fake::default();
        fake.answer("sbm_is_command", &[0, 1]);
        assert_eq!(status_right_down(&mut fake, 3), 0);
        assert_eq!(status_right_down(&mut fake, 3), 0);
        assert_eq!(fake.calls(), "sbm_is_command(3)\nsbm_capture_mouse(3)\nsbm_base_button_down(3)\nsbm_is_command(3)");
        let mut fake = Fake::default();
        fake.answer("sbm_state", &[2, 1]);
        status_tabbed_from(&mut fake, 3);
        status_tabbed_from(&mut fake, 3);
        assert_eq!(fake.calls(), "sbm_state(3)\nsbm_hot_key_up(3)\nsbm_state(3)\nsbm_base_tabbed_from(3)");
    }

    #[test]
    fn the_main_form_flags_load_and_save() {
        let mut fake = Fake::default();
        assert_eq!(main_load(&mut fake, 1, 2, 0, 0xd1, 9), 0);
        assert_eq!(fake.calls(), "");
        fake.answer("mff_read_bool", &[0, 0, 5]);
        assert_eq!(main_load(&mut fake, 1, 2, 0, 0xd2, 9), 0);
        assert_eq!(main_load(&mut fake, 1, 2, 2, 0, 9), 5);
        assert_eq!(fake.calls(), "mff_read_bool(9, 1)\nmff_read_bool(9, 2)\nmff_read_bool(9, 1)");
        let mut fake = Fake::default();
        fake.answer("mff_write_bool", &[3]);
        assert_eq!(main_save(&mut fake, true, false, 9), 3);
        assert_eq!(main_save(&mut fake, true, false, 9), 0);
        assert_eq!(fake.calls(), "mff_write_bool(9, true)\nmff_write_bool(9, true)\nmff_write_bool(9, false)");
    }

    #[test]
    fn the_error_string_is_set_under_the_lock() {
        let mut fake = Fake::default();
        main_set_error(&mut fake, 4, 7, 8);
        assert_eq!(
            fake.calls(),
            "mff_error_enter(4)\nmff_error_set(4, 7, 8)\nmff_strings_changed(4)\nmff_error_leave(4)"
        );
    }
}
