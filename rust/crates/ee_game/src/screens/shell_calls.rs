//! The generic shells' host cases (`ee_screenscpp_shell`, `User Interface Thread/EEUIShell.cpp`) in Rust (UI lane
//! round 7): the multi-statement cases of the toolkit's shells, the saved-group buttons, the icon bar, the chat line
//! and the paint areas, over the one-statement shims of [`Cpp`] (`game_cpp.rs`) on the shell's control (`ctl`). The
//! shells' adapter (`shells_ffi.rs`) asks [`call`] before the C++ host. (The editor-only kinds' ops stay the
//! scenario editor lane's.)

use core::ffi::{c_long, c_ulong};

use crate::screens::game_calls::string;
use crate::screens::game_cpp::{Cpp, EeScreensShimLongOut as OutL, EeScreensShimText as Text};
use crate::screens::main_form::{EE_SCREENS_MF_SOUND_BUTTON1, EE_SCREENS_MF_SOUND_BUTTON2};
use crate::screens::shells::{EE_SCREENS_SH_APPEND_STRING, EE_SCREENS_SH_BUTTON_TEXT, EE_SCREENS_SH_TEXT_FG};
use crate::screens::shells_chat::EE_SCREENS_SH_CHAT_FREE;
use crate::screens::shells_files::EE_SCREENS_SH_FILE_FIND_LINE;
use crate::screens::shells_icon_bar::{EE_SCREENS_SH_IB_FREE, EE_SCREENS_SH_SIZE};
use crate::screens::shells_paint::{
    EE_SCREENS_SH_MAP_TILE, EE_SCREENS_SH_MOUSE_AT, EE_SCREENS_SH_PICK, EE_SCREENS_SH_VIEW_RECT,
};
use crate::screens::shells_saved::{
    EE_SCREENS_SH_SG_DRAW, EE_SCREENS_SH_SG_FACE, EE_SCREENS_SH_SG_FIRST_UNIT, EE_SCREENS_SH_SG_SEND,
    EE_SCREENS_SH_SG_SOUND, EE_SCREENS_SH_SG_TEXTURE_REF,
};

/// The button sounds (`kSoundButton1`, `kSoundButton2`).
fn button_sound(second: bool) -> u32 {
    (if second { EE_SCREENS_MF_SOUND_BUTTON2 } else { EE_SCREENS_MF_SOUND_BUTTON1 }) as u32
}

/// The case `op` on the shell's control `ctl` (its arguments a, b and the four floats `f`), `None` for one that is
/// not here.
pub(crate) fn call<C: Cpp + ?Sized>(c: &mut C, ctl: usize, op: u32, a: i64, b: i64, f: &mut [f32; 4]) -> Option<i64> {
    Some(match op {
        EE_SCREENS_SH_TEXT_FG => {
            c.shell_text_fg(ctl, a as u32, f[0], f[1], f[2]);
            c.shell_text_transparent(ctl, a as u32);
            0
        }
        EE_SCREENS_SH_BUTTON_TEXT => {
            let s = string(a as u32);
            c.shell_button_set_text(ctl, Text::of(&s), b as usize);
            0
        }
        EE_SCREENS_SH_APPEND_STRING => {
            let s = string(a as u32);
            c.shell_combo_append(ctl, Text::of(&s));
            0
        }
        EE_SCREENS_SH_FILE_FIND_LINE => {
            let mut i = 0;
            while i < c.shell_combo_line_count(ctl) {
                if c.shell_combo_line_is(ctl, i, a as usize) {
                    return Some(i as i64);
                }
                i += 1;
            }
            -1
        }
        // the saved-group buttons' (shells_saved.rs)
        EE_SCREENS_SH_SG_SOUND => {
            let sound = c.sound_object(button_sound(a != 0));
            if sound == 0 {
                return Some(0);
            }
            if b != 0 {
                c.sound_unload_media(sound);
                return Some(0);
            }
            c.sound_load_media(sound);
            c.sound_sample_id(sound) as i64
        }
        EE_SCREENS_SH_SG_FIRST_UNIT => {
            if b == 0 {
                return Some(c.selected_first_unit() as i64);
            }
            let group = c.player_group(a as u32);
            if group != 0 && c.group_count(group) > 0 {
                c.group_first_unit(group) as i64
            } else {
                0
            }
        }
        EE_SCREENS_SH_SG_SEND => {
            let group = (b & 0xff) as u32;
            match a {
                0 => c.send_saved_group_select(group, (b >> 8) != 0),
                1 => c.send_saved_group_add_selection(group),
                2 => c.send_saved_group_create(group),
                _ => c.send_saved_group_clear(group),
            }
            0
        }
        EE_SCREENS_SH_SG_FACE => {
            let button = c.unit_stats_button(a as usize);
            c.button_set_face(button, b as usize, ctl);
            f[0] = c.button_cell_u(button);
            f[1] = c.button_cell_v(button);
            c.button_texture(button) as i64
        }
        EE_SCREENS_SH_SG_TEXTURE_REF => {
            if a != 0 {
                if b != 0 {
                    c.texture_reference(a as usize);
                } else {
                    c.texture_dereference(a as usize);
                }
            }
            0
        }
        EE_SCREENS_SH_SG_DRAW => {
            c.bitmap_at_button(a as usize, ctl);
            c.shell_draw_bitmap(ctl, a as usize);
            0
        }
        // the icon bar's (shells_icon_bar.rs): the slots let go as its destructor did
        EE_SCREENS_SH_IB_FREE => {
            for second in [false, true] {
                let sound = c.sound_object(button_sound(second));
                if sound != 0 {
                    c.sound_unload_media(sound);
                }
            }
            for slot in 0..2 {
                let bitmap = c.shell_slot(ctl, slot);
                if bitmap != 0 {
                    c.bitmap_dereference(bitmap);
                }
            }
            c.shell_clear_slot(ctl, 0);
            c.shell_clear_slot(ctl, 1);
            0
        }
        // the chat line's (shells_chat.rs): its texts deleted, as its destructor did
        EE_SCREENS_SH_CHAT_FREE => {
            for slot in 0..4 {
                let text = c.shell_slot(ctl, slot);
                c.text_delete(text);
            }
            0
        }
        EE_SCREENS_SH_SIZE => {
            f[0] = c.control_width(ctl);
            f[1] = c.control_height(ctl);
            0
        }
        // the paint areas' (shells_paint.rs)
        EE_SCREENS_SH_PICK => {
            c.shim_tile_clear();
            c.shim_point_clear();
            if a == 1 {
                let (mut x, mut y): (c_long, c_long) = (0, 0);
                let found = c.pick_tile_xy(f[0], f[1], OutL::of(&mut x), OutL::of(&mut y));
                f[0] = x as f32;
                f[1] = y as f32;
                f[2] = if c.shim_tile_exists() { c.shim_tile_height() } else { 0.0 };
                return Some(found as i64);
            }
            if a == 2 {
                c.pick_map_location(f[0], f[1]);
            } else {
                c.pick_unit_or_tile(f[0], f[1]);
            }
            f[0] = c.shim_point_x();
            f[1] = c.shim_point_y();
            f[2] = c.shim_point_z();
            let found = c.shim_tile_exists();
            f[3] = if found { c.shim_tile_height() } else { 0.0 };
            found as i64
        }
        EE_SCREENS_SH_MAP_TILE => {
            c.map_tile_load(a as c_ulong as u32, b as c_ulong as u32);
            let found = c.shim_map_tile_exists();
            f[0] = if found { c.shim_map_tile_height() } else { 0.0 };
            found as i64
        }
        EE_SCREENS_SH_MOUSE_AT => {
            if a != 0 {
                f[0] = c.mouse_x();
                f[1] = c.mouse_y();
            } else {
                f[0] = c.shell_mouse_x(ctl);
                f[1] = c.shell_mouse_y(ctl);
            }
            0
        }
        EE_SCREENS_SH_VIEW_RECT => {
            *f = [c.view_left(), c.view_top(), c.view_right(), c.view_bottom()];
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "shell_calls_tests.rs"]
mod tests;
