//! More HUD bodies in Rust over one-statement shims (UI lane round 13): `EEUIFMGAttribute`'s constructor body,
//! `CopyData` and `DrawTextA` (the attribute button's level text: the number of queued upgrades in its corner) and
//! `EEUIFMGStatusButton`'s `MouseRButtonDown` and `TabbedFrom`. Each C++ member is one forward; the calls are the
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
        assert_eq!(
            fake.calls(),
            "sbm_is_command(3)\nsbm_capture_mouse(3)\nsbm_base_button_down(3)\nsbm_is_command(3)"
        );
        let mut fake = Fake::default();
        fake.answer("sbm_state", &[2, 1]);
        status_tabbed_from(&mut fake, 3);
        status_tabbed_from(&mut fake, 3);
        assert_eq!(fake.calls(), "sbm_state(3)\nsbm_hot_key_up(3)\nsbm_state(3)\nsbm_base_tabbed_from(3)");
    }
}
