//! The generic shells' host cases (`ee_screenscpp_shell`, `User Interface Thread/EEUIShell.cpp`) in Rust (UI lane
//! round 7): the multi-statement cases of the toolkit's shells, the saved-group buttons, the icon bar, the chat line
//! and the paint areas, over the one-statement shims of [`Cpp`] (`game_cpp.rs`) on the shell's control (`ctl`). The
//! shells' adapter (`shells_ffi.rs`) asks [`call`] before the C++ host. (The editor-only kinds' ops stay the
//! scenario editor lane's.)

// (the casts of the C `long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_long, c_ulong};

use crate::screens::game_calls::string;
use crate::screens::game_cpp::{
    Cpp, EeScreensShimLongOut as OutL, EeScreensShimOut, EeScreensShimText as Text, EeScreensShimULongOut as OutUL,
};
use crate::screens::main_form::{EE_SCREENS_MF_SOUND_BUTTON1, EE_SCREENS_MF_SOUND_BUTTON2};
use crate::screens::shells::EE_SCREENS_SH_APPEND_NUMBERED;
use crate::screens::shells::{EE_SCREENS_SH_APPEND_STRING, EE_SCREENS_SH_BUTTON_TEXT, EE_SCREENS_SH_TEXT_FG};
use crate::screens::shells_chat::EE_SCREENS_SH_CHAT_FREE;
use crate::screens::shells_dialog::EE_SCREENS_SH_FILL;
use crate::screens::shells_editor::{
    EE_SCREENS_SH_BITMAP_COORDS, EE_SCREENS_SH_CLEAR_RADIO_BITMAPS, EE_SCREENS_SH_EDITOR_PLAYER,
    EE_SCREENS_SH_LIST_OF_COMBO_SELECTION, EE_SCREENS_SH_LIST_SELECTION, EE_SCREENS_SH_MAP_CREATE,
    EE_SCREENS_SH_NEW_BITMAP, EE_SCREENS_SH_SELECT_TAB, EE_SCREENS_SH_TECH_TREE_MOVE, EE_SCREENS_SH_TEXTURE_SIZE,
};
use crate::screens::shells_files::{EE_SCREENS_SH_FILE_APPEND_LISTED, EE_SCREENS_SH_FILE_FIND_LINE};
use crate::screens::shells_icon_bar::{EE_SCREENS_SH_IB_FREE, EE_SCREENS_SH_SIZE};
use crate::screens::shells_paint::{
    EE_SCREENS_SH_MAP_TILE, EE_SCREENS_SH_MOUSE_AT, EE_SCREENS_SH_MOUSE_BUTTONS, EE_SCREENS_SH_PAINT_AREA,
    EE_SCREENS_SH_PICK, EE_SCREENS_SH_SCREEN_LINE, EE_SCREENS_SH_SLIDER_OF, EE_SCREENS_SH_SPACE,
    EE_SCREENS_SH_VIEW_RECT,
};
use crate::screens::shells_saved::{
    EE_SCREENS_SH_SG_DRAW, EE_SCREENS_SH_SG_FACE, EE_SCREENS_SH_SG_FIRST_UNIT, EE_SCREENS_SH_SG_SEND,
    EE_SCREENS_SH_SG_SOUND, EE_SCREENS_SH_SG_TEXTURE_REF,
};
use crate::screens::shells_tech::{
    EE_SCREENS_SH_FORM_CHECKED, EE_SCREENS_SH_LIST_CURRENT, EE_SCREENS_SH_TECH_DEPENDS, EE_SCREENS_SH_TECH_MARK_PLAYER,
    EE_SCREENS_SH_TECH_NAME_ID, EE_SCREENS_SH_TECH_OBSOLETE, EE_SCREENS_SH_TECH_OBSOLETED,
};
use crate::screens::shells_unit::{
    EE_SCREENS_SH_KEY, EE_SCREENS_SH_LIST_INSERT_BYTES, EE_SCREENS_SH_LIST_LINE, EE_SCREENS_SH_ROTATE,
    EE_SCREENS_SH_UNIT_CLASS, EE_SCREENS_SH_UNIT_NAME_AND_ID,
};

/// The button sounds (`kSoundButton1`, `kSoundButton2`).
fn button_sound(second: bool) -> u32 {
    (if second { EE_SCREENS_MF_SOUND_BUTTON2 } else { EE_SCREENS_MF_SOUND_BUTTON1 }) as u32
}

/// The scenario editor's player combo box's selection (`GetCurrentSelection`'s; 0 when it has none).
fn editor_player<C: Cpp + ?Sized>(c: &mut C) -> c_ulong {
    let mut s: c_ulong = 0;
    c.sh_editor_player(OutUL::of(&mut s));
    s
}

/// The tech tree of the editor's player (`EEShellTree`: the combo box's selection + 1 indexes the players).
fn editor_tree<C: Cpp + ?Sized>(c: &mut C) -> usize {
    let s = editor_player(c);
    c.sh_player_tree(s as u32)
}

/// `EEShellRotate`: the ghosts (`ghosts`) or the editor's selected units that can turn, turned to `angle`.
fn rotate<C: Cpp + ?Sized>(c: &mut C, ctl: usize, ghosts: bool, angle: f32) {
    if ghosts {
        c.sh_ghosts_enter();
        for i in 0..c.sh_ghost_count() {
            if !c.sh_ghost_turns(i) {
                continue;
            }
            c.sh_tr_from_ghost(i);
            c.sh_tr_orient(angle);
            c.sh_tr_to_ghost(i);
        }
        c.sh_ghosts_leave();
        return;
    }
    for i in 0..c.sh_editor_selected_count(ctl) {
        let unit = c.sh_editor_selected(ctl, i);
        if !c.sh_unit_has_complex(unit) || !c.sh_unit_turns(unit) {
            continue;
        }
        c.sh_tr_from_unit(unit);
        c.sh_tr_orient(angle);
        c.sh_tr_to_unit(unit);
    }
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
        // ---- the rest of the multi-statement cases (UI lane round 9)
        EE_SCREENS_SH_FILE_APPEND_LISTED => {
            // the files path a lists with pattern b (bytes), their last four characters (the extension) cut
            c.sh_files_clear();
            c.sh_files_list(a as usize, b as usize);
            c.sh_files_snapshot();
            for i in 0..c.sh_files_count() {
                c.sh_text_from_file(i);
                c.sh_text_trim4();
                c.sh_combo_append_text(ctl);
            }
            0
        }
        EE_SCREENS_SH_APPEND_NUMBERED => {
            // string a printed with 1..=b
            c.sh_numbered_format(a);
            for i in 1..=(b as c_ulong) {
                c.sh_numbered_print(i as u32);
                c.sh_combo_append_text(ctl);
            }
            0
        }
        EE_SCREENS_SH_CLEAR_RADIO_BITMAPS => {
            c.sh_radio_clear_down_checked(ctl);
            c.sh_radio_clear_up_checked(ctl);
            c.sh_radio_clear_down_unchecked(ctl);
            c.sh_radio_clear_up_unchecked(ctl);
            0
        }
        EE_SCREENS_SH_LIST_SELECTION => {
            let mut s: c_ulong = 0;
            if c.sh_form_list_selection(ctl, a, OutUL::of(&mut s)) != 0 {
                -1
            } else {
                s as i64
            }
        }
        EE_SCREENS_SH_EDITOR_PLAYER => editor_player(c) as i64,
        EE_SCREENS_SH_TECH_TREE_MOVE => {
            c.sh_tech_tree_move(a as u32 as i32, (a >> 32) as u32 as i32, b as u32 as i32, (b >> 32) != 0);
            0
        }
        EE_SCREENS_SH_LIST_OF_COMBO_SELECTION => {
            let mut s: c_ulong = 0;
            c.sh_combo_list_selection(ctl, OutUL::of(&mut s));
            s as i64
        }
        EE_SCREENS_SH_MAP_CREATE => {
            // a: the map tab's group
            #[cfg(not(test))]
            // SAFETY: the editor's live group (its tab is the map tab)
            unsafe {
                let tab = crate::screens::editor_groups_ffi::ee_screens_segr_tab(a as usize as *const _);
                crate::screens::editor_map_ffi::ee_screens_semp_call(
                    tab as *const _,
                    crate::screens::editor_map::EE_SCREENS_SEMP_CREATE_MAP,
                    0,
                );
            }
            0
        }
        EE_SCREENS_SH_NEW_BITMAP => {
            // texture a's picture on the form's screen (b) or the game's
            let bitmap = c.sh_bitmap_new();
            if b != 0 {
                c.sh_bitmap_texture_form(bitmap, a as usize, ctl);
            } else {
                c.sh_bitmap_texture_global(bitmap, a as usize);
            }
            bitmap as i64
        }
        EE_SCREENS_SH_BITMAP_COORDS => {
            c.sh_bitmap_coords(a as usize, f[0], f[1], f[2], f[3]);
            0
        }
        EE_SCREENS_SH_TEXTURE_SIZE => {
            f[0] = c.sh_texture_width(a as usize);
            f[1] = c.sh_texture_height(a as usize);
            0
        }
        EE_SCREENS_SH_SELECT_TAB => {
            // the editor's tab (b's high word) and page: posted to the UI thread (a == 0, when there is one)
            let (tab, page) = ((b >> 32) as u8, b as i32);
            if a == 0 && c.sh_has_user_interface() {
                c.sh_post_select_tab(tab, page);
            } else {
                c.sh_select_tab(ctl, tab, page);
            }
            0
        }
        EE_SCREENS_SH_LIST_CURRENT => {
            let mut s: c_ulong = 0;
            let r = c.sh_list_current(ctl, OutUL::of(&mut s));
            if a != 0 || r == 0 {
                s as i64
            } else {
                -1
            }
        }
        EE_SCREENS_SH_FORM_CHECKED => {
            let control = c.sh_form_control(ctl, a);
            if c.sh_is_radio(control) {
                c.sh_radio_checked(control) as i64
            } else {
                c.sh_check_checked(control) as i64
            }
        }
        EE_SCREENS_SH_TECH_NAME_ID => {
            if b != 0 {
                c.sh_tech_name_id(a)
            } else {
                c.sh_tech_class_name_id(a)
            }
        }
        EE_SCREENS_SH_TECH_DEPENDS => {
            // technology a's low word on a's high word (b: the high word is an index, its data's id)
            let tree = editor_tree(c);
            let mut on = (a >> 32) as i32 as i64;
            if b != 0 {
                on = c.sh_tree_data_id(tree, on);
            }
            c.sh_tree_depends(tree, a as u32 as i32 as i64, on)
        }
        EE_SCREENS_SH_TECH_OBSOLETE => {
            let tree = editor_tree(c);
            c.sh_tree_set_obsoleted(tree, a, b != 0);
            c.sh_tree_epoch_reached(tree, a) as i64
        }
        EE_SCREENS_SH_TECH_OBSOLETED => {
            let tree = editor_tree(c);
            c.sh_tree_obsoleted(tree, a)
        }
        EE_SCREENS_SH_TECH_MARK_PLAYER => {
            let tree = editor_tree(c);
            c.sh_tree_mark_player(tree);
            0
        }
        EE_SCREENS_SH_UNIT_CLASS => match b {
            0 => c.sh_class_name_id(a),
            1 => c.sh_class_epoch(a),
            _ => c.sh_class_family(a),
        },
        EE_SCREENS_SH_LIST_INSERT_BYTES => {
            c.sh_list_insert(ctl, a as usize, b);
            0
        }
        EE_SCREENS_SH_LIST_LINE => {
            // line a into b's 4,500 bytes (strncpy's 4,499, zero padded, and a last 0)
            let mut line = Vec::new();
            c.sh_list_data(ctl, a, EeScreensShimOut::of(&mut line));
            line.truncate(4499);
            line.resize(4500, 0);
            // SAFETY: the 4,500-byte buffer shells.rs hands with the op
            unsafe { core::ptr::copy_nonoverlapping(line.as_ptr(), b as usize as *mut u8, line.len()) };
            0
        }
        EE_SCREENS_SH_UNIT_NAME_AND_ID => {
            let unit = a as usize;
            c.sh_unit_name_id(unit) << 32 | c.sh_unit_id(unit) as i64
        }
        EE_SCREENS_SH_ROTATE => {
            // a == 2: whether there are ghosts; else the ghosts (a) or the editor's selected units turned to f[0]
            if a == 2 {
                return Some((c.sh_ghost_count() > 0) as i64);
            }
            rotate(c, ctl, a != 0, f[0]);
            0
        }
        EE_SCREENS_SH_KEY => {
            // the comma's (a == 0) or the period's: GetEntity's result << 8 | 1 just pressed, 2 just released
            let r = c.sh_key_load((a != 0) as u32);
            let state = if c.sh_key_just_pressed() {
                1
            } else if c.sh_key_just_unpressed() {
                2
            } else {
                0
            };
            r << 8 | state
        }
        EE_SCREENS_SH_SCREEN_LINE => {
            // the screen line under (f[0], f[1]) into a's six floats (its start and direction)
            c.sh_screen_line(f[0], f[1]);
            let line = [
                c.sh_line_start_x(),
                c.sh_line_start_y(),
                c.sh_line_start_z(),
                c.sh_line_direction_x(),
                c.sh_line_direction_y(),
                c.sh_line_direction_z(),
            ];
            // SAFETY: the six floats shells_paint.rs hands with the op
            unsafe { core::ptr::copy_nonoverlapping(line.as_ptr(), a as usize as *mut f32, 6) };
            0
        }
        EE_SCREENS_SH_MOUSE_BUTTONS => {
            (c.sh_right_down(ctl) as i64) | (c.sh_left_down(ctl) as i64) << 1 | (c.sh_has_capture(ctl) as i64) << 2
        }
        EE_SCREENS_SH_PAINT_AREA => {
            // SAFETY: the four ints shells_paint.rs hands with the op
            let r = unsafe { *(b as usize as *const [i32; 4]) };
            c.sh_paint_area(a as usize, r[0], r[1], r[2], r[3])
        }
        EE_SCREENS_SH_SPACE => {
            let found = c.sh_key_load(2) == 0;
            (found && c.sh_key_value() > 0) as i64
        }
        EE_SCREENS_SH_SLIDER_OF => c.sh_slider_max(ctl, a) << 32 | c.sh_slider_current(ctl, a) as i64,
        EE_SCREENS_SH_FILL => {
            // the control's copied draw rectangle filled with f's colour at f[3]
            let (rasterizer, screen) = (c.sh_rasterizer(ctl), c.sh_screen_index(ctl));
            let rect = c.sh_copied_draw_rectangle(ctl);
            #[cfg(not(test))]
            {
                // SAFETY: the control's live rasterizer and rectangle; f holds the colour
                (unsafe {
                    ee_render::ge::list2d::ee_ge_draw2d_fill(
                        rasterizer as *mut _,
                        screen as _,
                        rect as *const _,
                        f.as_ptr(),
                        f[3],
                    )
                }) as i64
            }
            #[cfg(test)]
            {
                let _ = (rasterizer, screen, rect);
                0
            }
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "shell_calls_tests.rs"]
mod tests;
