//! `EEUIShell.cpp`'s factory and draw bodies in Rust over one-statement shims (UI lane round 12): the selected-units
//! icon bar's and the chat line's controls as the user-defined control factory makes them (the shells' slots: the
//! icon bitmap, the frame, the button sounds; the line's four texts), and the tech list's blue bars
//! (`EESEUITechListBox::DrawPolygons`' `DrawBars`).

use crate::screens::game_cpp::{Cpp, EeScreensShimF32Out};
use crate::screens::main_form::{EE_SCREENS_MF_SOUND_BUTTON1, EE_SCREENS_MF_SOUND_BUTTON2};
use crate::screens::shells_chat::EE_SCREENS_SKOP_CHAT_PARTS;
use crate::screens::shells_icon_bar::EE_SCREENS_SKOP_IB_ICON_RECT;

/// `EEUIShellNewIconBar(table, rect, form)` (EEUICUnitIconBar): the icon bitmap, the table's normal bitmap at one
/// icon's rectangle as its frame, the two button sounds loaded (their sample ids in slots 2 and 3).
pub(crate) fn icon_bar<C: Cpp + ?Sized>(c: &mut C, table: usize, form: usize) -> usize {
    let bar = c.sf_icon_bar_new(table, form);
    let mut icon = [0.0f32; 4];
    c.sf_shell_op(bar, EE_SCREENS_SKOP_IB_ICON_RECT, 0, 0, EeScreensShimF32Out(icon.as_mut_ptr()));
    let bitmap = c.sf_bitmap_new();
    c.sf_slot_set(bar, 0, bitmap);
    let frame = c.sf_normal_bitmap(table, form, icon[0], icon[1], icon[2], icon[3]);
    c.sf_slot_set(bar, 1, frame);
    for (i, id) in [EE_SCREENS_MF_SOUND_BUTTON1, EE_SCREENS_MF_SOUND_BUTTON2].into_iter().enumerate() {
        let sound = c.sf_sound(id as u32);
        if sound != 0 {
            c.sf_sound_load(sound);
            let sample = c.sf_sound_sample(sound);
            c.sf_slot_set(bar, 2 + i as u32, sample);
        }
    }
    bar
}

/// `EEUIShellNewChatLine(table, rect, form)` (EEUICChatLine): the table's background, then the bold name, its
/// drawn copy (both at the line's top left), the message and its copy, handed to the kind.
pub(crate) fn chat_line<C: Cpp + ?Sized>(c: &mut C, table: usize, form: usize) -> usize {
    let opacity = c.sf_table_bg_opacity(table);
    let line = c.sf_chat_line_new(table, form, opacity.to_bits() as i64);
    let dirty = c.sf_table_always_dirty(table);
    c.sf_set_always_dirty(line, dirty);
    c.sf_bg_color_of_table(line, table);
    c.sf_bg_opacity(line, opacity);
    let font = c.sf_table_font(table);
    for i in 0..4u32 {
        let f = if i >= 2 { font } else { c.sf_bold_font(font) };
        let text = c.sf_text_new(f);
        c.sf_table_setup_text(table, text);
        if i < 2 {
            c.sf_text_at_top_left(text, line);
        }
        c.sf_slot_set(line, i, text);
    }
    let slots = c.sf_slots(line);
    let mut unused = [0.0f32; 4];
    c.sf_shell_op(line, EE_SCREENS_SKOP_CHAT_PARTS, slots as i64, 0, EeScreensShimF32Out(unused.as_mut_ptr()));
    line
}

/// `EEUIShellList::DrawBars(lines)`: a blue bar across the list's text at each line -> the last draw's result.
pub(crate) fn draw_bars<C: Cpp + ?Sized>(c: &mut C, list: usize, lines: &[u32]) -> i64 {
    let rhombus = c.bars_rhombus_new();
    c.bars_rhombus_color(rhombus, 0.0, 0.0, 1.0);
    let mut r = [0.0f32; 4]; // bottom, left, right, top (UIObjHost::PutRect's)
    c.bars_copy_text_rect(list, EeScreensShimF32Out(r.as_mut_ptr()));
    let (left, right, first) = (r[1], r[2], r[3]);
    let mut result = 0;
    for &line in lines {
        let height = c.bars_line_height(list) as u32;
        let top = first + line.wrapping_mul(height) as f32;
        let height = c.bars_line_height(list) as u32;
        c.bars_rhombus_rect(rhombus, left, top, right, top + height as f32);
        result = c.bars_draw(list, rhombus);
    }
    c.bars_rhombus_free(rhombus);
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_icon_bar_skips_a_missing_sound() {
        let mut fake = Fake::default();
        fake.answer("sf_icon_bar_new", &[10]).answer("sf_bitmap_new", &[20]).answer("sf_normal_bitmap", &[21]);
        fake.answer("sf_sound", &[0, 30]).answer("sf_sound_sample", &[7]);
        assert_eq!(icon_bar(&mut fake, 1, 2), 10);
        let calls = fake.calls();
        assert!(calls.starts_with("sf_icon_bar_new(1, 2)\nsf_shell_op(10, 137, 0, 0, out)\nsf_bitmap_new()\nsf_slot_set(10, 0, 20)\n"));
        assert!(calls.ends_with(
            "sf_slot_set(10, 1, 21)\nsf_sound(519)\nsf_sound(520)\nsf_sound_load(30)\nsf_sound_sample(30)\nsf_slot_set(10, 3, 7)"
        ));
    }

    #[test]
    fn the_chat_line_makes_two_bold_texts_at_its_corner() {
        let mut fake = Fake::default();
        fake.answer("sf_chat_line_new", &[10]).answer("sf_table_font", &[5]).answer("sf_bold_font", &[6, 6]);
        fake.answer("sf_text_new", &[40, 41, 42, 43]).answer("sf_slots", &[99]);
        assert_eq!(chat_line(&mut fake, 1, 2), 10);
        let calls = fake.calls();
        assert!(calls.contains("sf_bold_font(5)\nsf_text_new(6)\nsf_table_setup_text(1, 40)\nsf_text_at_top_left(40, 10)\nsf_slot_set(10, 0, 40)"));
        assert!(calls.contains("sf_text_new(5)\nsf_table_setup_text(1, 43)\nsf_slot_set(10, 3, 43)\nsf_slots(10)\nsf_shell_op(10, 132, 99, 0, out)"));
    }

    #[test]
    fn the_bars_follow_the_lines() {
        let mut fake = Fake::default();
        fake.answer("bars_rhombus_new", &[50]).answer("bars_line_height", &[12, 12, 12, 12]).answer("bars_draw", &[0, 3]);
        assert_eq!(draw_bars(&mut fake, 1, &[0, 2]), 3);
        assert!(fake.calls().contains("bars_rhombus_rect(50, 0.0, 24.0, 0.0, 36.0)"));
        let mut fake = Fake::default();
        assert_eq!(draw_bars(&mut fake, 1, &[]), 0);
        assert!(fake.calls().ends_with("bars_rhombus_free(0)"));
    }
}
