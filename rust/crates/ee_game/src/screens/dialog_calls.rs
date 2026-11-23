//! The message box's host's cases of more than one call (`ee_screenscpp_dialog`, `EEUIFError.cpp`) in Rust over
//! one-statement shims (UI lane round 10): the buttons' set-up, the layout's rectangle, the font, the controls'
//! making, the edit box's colours, the background, the controls' deletion, the closed post. `slots` are the box's
//! controls (the Rust kind's: 0 the message, 1 OK/Yes, 2 No, 3 Cancel, 4 the edit box, 5 the background);
//! `shells_dialog.rs`' host asks here before the C++.

use crate::screens::dialog::{
    EE_SCREENS_DLG_BACKGROUND, EE_SCREENS_DLG_BUTTON, EE_SCREENS_DLG_CALC, EE_SCREENS_DLG_EDIT_COLORS,
    EE_SCREENS_DLG_FONT, EE_SCREENS_DLG_MESSAGE, EE_SCREENS_DLG_NEW, EE_SCREENS_DLG_RECT,
};
use crate::screens::game_cpp::Cpp;
use crate::screens::shells_dialog::{
    EE_SCREENS_DLG_DELETE, EE_SCREENS_DLG_POST_CLOSED, EE_SCREENS_DLG_SCALE_BACKGROUND, EE_SCREENS_SK_DIALOG_CANCEL,
    EE_SCREENS_SK_DIALOG_OK,
};

/// The host's moved op on the box `dialog` and its slots; `None` for the C++ host's.
pub(crate) fn call<C: Cpp + ?Sized>(
    c: &mut C,
    dialog: usize,
    slots: &mut [usize; 6],
    op: u32,
    a: u32,
    b: u32,
) -> Option<u32> {
    match op {
        // the message label (slot 0): its alignment, then the box's message
        EE_SCREENS_DLG_MESSAGE => {
            c.dlg_message_align(slots[0], a);
            c.dlg_message_text(slots[0]);
        }
        // the layout rectangle's side `a` (left, top, right, bottom) as the float's bits
        EE_SCREENS_DLG_RECT => return Some(c.dlg_rect_side(a).to_bits()),
        EE_SCREENS_DLG_BUTTON => {
            let button = slots[a as usize];
            c.dlg_text_load(b >> 2);
            c.dlg_button_set_text(button);
            if b & 3 != 0 {
                let callback = c.dlg_callback(if b & 3 == 2 { 1 } else { 0 });
                let data = if b & 3 == 3 { c.dlg_edit_text(slots[4]) } else { c.dlg_data() };
                c.dlg_set_callback(button, callback, data);
            }
            c.dlg_show_button(button);
            if a == 3 && b & 3 != 0 {
                c.dlg_set_close(slots[3]);
            }
            c.dlg_button_text_color(button);
            c.dlg_button_text_transparent(button);
        }
        EE_SCREENS_DLG_CALC => {
            let height = c.dlg_window_height();
            let width = c.dlg_window_width();
            c.dlg_layout_reset();
            c.dlg_layout_left((a & 0xffff) as f32 / 1024.0);
            c.dlg_layout_right((a >> 16) as f32 / 1024.0);
            c.dlg_layout_top((b & 0xffff) as f32 / 768.0);
            c.dlg_layout_bottom((b >> 16) as f32 / 768.0);
            c.dlg_layout_calculate(width, height);
        }
        EE_SCREENS_DLG_FONT => {
            if a == 0 {
                let height = c.dlg_window_height() - 0.0;
                let size = ee_ui::float_to_long(c.dlg_font16_height() * (1.0 / 768.0) * height);
                c.dlg_font_create(size as i32);
            } else if a == 4 {
                c.dlg_font_release();
            } else {
                c.dlg_button_font(slots[a as usize]);
            }
        }
        // (the original's ESVERIFYs after each new: new never returns NULL)
        EE_SCREENS_DLG_NEW => {
            slots[a as usize] = match a {
                0 => c.dlg_new_label(dialog),
                4 => c.dlg_new_edit(dialog),
                // OK/Yes (EEUIFEBOK), No and Cancel (EEUIFEBCancel): 50x20, a one-pixel border, the index the id
                _ => c.dlg_new_button(
                    dialog,
                    if a == 1 { EE_SCREENS_SK_DIALOG_OK } else { EE_SCREENS_SK_DIALOG_CANCEL },
                    a,
                ),
            };
        }
        EE_SCREENS_DLG_EDIT_COLORS => {
            c.dlg_edit_selected_color(slots[4]);
            c.dlg_edit_bg_color(slots[4]);
            c.dlg_edit_cursor_color(slots[4]);
        }
        EE_SCREENS_DLG_BACKGROUND => {
            c.dlg_layout_reset();
            let left = c.dlg_float(0);
            c.dlg_layout_left(left);
            let right = c.dlg_float(1);
            c.dlg_layout_right(right);
            let top = c.dlg_float(2);
            c.dlg_layout_top(top);
            let bottom = c.dlg_float(3);
            c.dlg_layout_bottom(bottom);
            c.dlg_box_set_layout(dialog);
            slots[5] = c.dlg_new_background(dialog);
        }
        EE_SCREENS_DLG_DELETE => {
            for &control in &slots[..5] {
                c.dlg_delete(control);
            }
        }
        EE_SCREENS_DLG_SCALE_BACKGROUND => c.dlg_scale_background(slots[5]),
        EE_SCREENS_DLG_POST_CLOSED => {
            let (mut callback, mut data) = (0, 0);
            for &button in &slots[1..=3] {
                if button != 0 && c.dlg_pressed(button) {
                    callback = c.dlg_pressed_callback(button);
                    data = c.dlg_pressed_data(button);
                    break;
                }
            }
            c.dlg_post_closed(dialog, callback, data);
        }
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn button_with_the_edit_boxs_callback() {
        let mut fake = Fake::default();
        fake.answer("dlg_callback", &[0x70]).answer("dlg_edit_text", &[0x71]);
        let mut slots = [10, 11, 12, 13, 14, 15];
        call(&mut fake, 1, &mut slots, EE_SCREENS_DLG_BUTTON, 3, 40 << 2 | 3);
        assert_eq!(
            fake.calls(),
            "dlg_text_load(40)\ndlg_button_set_text(13)\ndlg_callback(0)\ndlg_edit_text(14)\ndlg_set_callback(13, 112, 113)\n\
             dlg_show_button(13)\ndlg_set_close(13)\ndlg_button_text_color(13)\ndlg_button_text_transparent(13)"
        );
        let mut fake = Fake::default();
        call(&mut fake, 1, &mut slots, EE_SCREENS_DLG_BUTTON, 2, 41 << 2 | 2);
        assert!(fake.calls().contains("dlg_callback(1)\ndlg_data()\n"));
    }

    #[test]
    fn layout_font_and_controls() {
        let mut fake = Fake::default();
        fake.answer("dlg_window_height", &[768, 768])
            .answer("dlg_window_width", &[1024])
            .answer("dlg_font16_height", &[16]);
        fake.answer("dlg_new_label", &[0x20]).answer("dlg_new_button", &[0x21]);
        let mut slots = [0; 6];
        call(&mut fake, 1, &mut slots, EE_SCREENS_DLG_CALC, 512 | 1024 << 16, 384 | 768 << 16);
        call(&mut fake, 1, &mut slots, EE_SCREENS_DLG_FONT, 0, 0);
        call(&mut fake, 1, &mut slots, EE_SCREENS_DLG_NEW, 0, 0);
        call(&mut fake, 1, &mut slots, EE_SCREENS_DLG_NEW, 2, 0);
        assert_eq!(slots[..3], [0x20, 0, 0x21]);
        assert_eq!(
            fake.calls(),
            "dlg_window_height()\ndlg_window_width()\ndlg_layout_reset()\ndlg_layout_left(0.5)\ndlg_layout_right(1.0)\n\
             dlg_layout_top(0.5)\ndlg_layout_bottom(1.0)\ndlg_layout_calculate(1024.0, 768.0)\ndlg_window_height()\n\
             dlg_font16_height()\ndlg_font_create(16)\ndlg_new_label(1)\ndlg_new_button(1, 106, 2)"
        );
    }

    #[test]
    fn closed_post_names_the_pressed_button() {
        let mut fake = Fake::default();
        fake.answer("dlg_pressed", &[0, 1]).answer("dlg_pressed_callback", &[5]).answer("dlg_pressed_data", &[6]);
        let mut slots = [10, 11, 0, 13, 14, 15];
        call(&mut fake, 1, &mut slots, EE_SCREENS_DLG_POST_CLOSED, 0, 0);
        assert_eq!(
            fake.calls(),
            "dlg_pressed(11)\ndlg_pressed(13)\ndlg_pressed_callback(13)\ndlg_pressed_data(13)\ndlg_post_closed(1, 5, 6)"
        );
        assert_eq!(call(&mut fake, 1, &mut slots, 1, 0, 0), None);
    }
}
