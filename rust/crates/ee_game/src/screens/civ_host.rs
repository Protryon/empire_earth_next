//! The civilization builder's host whole (UI lane round 11): every op of the C++ `ee_screenscpp_cb`
//! (`Civilization Builder/EECivilizationBuilder.cpp`) and of `EECBUIListBox::HostOp` (the list's half,
//! `EE_SCREENS_CBW_L_*`) in Rust over one-statement C++ shims (`ee_screenscpp_cbh_*`, `ee_screenscpp_cbl_*`), with
//! the civilization tables' queries read from `ee_sim::tech::civ` directly. `civ_whole_ffi.rs`' host calls
//! [`op`] with the FFI shims; the tests run it on the recording fake.

use core::ffi::c_ulong;

use ee_sim::tech::civ;

use crate::screens::civ_whole::*;
use crate::screens::game_cpp::{Cpp, EeScreensShimOut, EeScreensShimText, EeScreensShimULongOut};

/// The text up to its first NUL (the C++ cases' `UWideString(theBytes.c_str())`).
fn c_str(text: &[u8]) -> &[u8] {
    text.iter().position(|&b| b == 0).map_or(text, |n| &text[..n])
}

/// A form's control by id (`EECBControl`): 0 without the form.
fn control<C: Cpp + ?Sized>(c: &mut C, form: i64, id: i64) -> usize {
    let form = c.cbh_form(form as u32);
    if form == 0 {
        0
    } else {
        c.cbh_control_by_id(form, id as u32)
    }
}

/// The file list box (`EECBFileList`): on the form `form` (else the form of the id `form_id`), 0 without it.
fn file_list<C: Cpp + ?Sized>(c: &mut C, form: usize, form_id: i64, id: i64) -> usize {
    let form = if form != 0 { form } else { c.cbh_form(form_id as u32) };
    if form == 0 {
        return 0;
    }
    let control = c.cbh_control_by_id(form, id as u32);
    c.cbh_as_file_list(control)
}

/// The main game form's civilization builder button (0) or the large builder alert (else) (`EECBMainButton`).
fn main_button<C: Cpp + ?Sized>(c: &mut C, which: i64) -> usize {
    if which != 0 {
        return c.cbh_main_alert_button();
    }
    let id = c.cbh_main_game_form_id();
    let form = c.cbh_form(id);
    if form == 0 {
        0
    } else {
        c.cbh_main_builder_button(form)
    }
}

/// The op `op` (`EE_SCREENS_CBW_*`) on `p` with `a`, `b`, `cc` and `text`; its texts and values in `out`.
#[allow(clippy::too_many_arguments)]
pub(crate) fn op<C: Cpp + ?Sized>(
    c: &mut C,
    op: u32,
    p: usize,
    a: i64,
    b: i64,
    cc: i64,
    text: &[u8],
    out: &mut EeScreensCbOut,
) -> i64 {
    if (EE_SCREENS_CBW_L_COUNT..=EE_SCREENS_CBW_L_DRAW_END).contains(&op) {
        return list_op(c, op, p, a, b, cc, text, out);
    }
    let t = EeScreensShimText::of(c_str(text));
    match op {
        EE_SCREENS_CBW_FORM_EXISTS => (c.cbh_form(a as u32) != 0) as i64,
        EE_SCREENS_CBW_EDIT_TEXT | EE_SCREENS_CBW_SET_EDIT_TEXT | EE_SCREENS_CBW_EDIT_SELECT => {
            let control = control(c, a, b);
            let edit = if control == 0 { 0 } else { c.cbh_as_edit(control) };
            if edit == 0 {
                return 3;
            }
            if op == EE_SCREENS_CBW_EDIT_TEXT {
                c.cbh_edit_text(edit, EeScreensShimOut::of(&mut out.text));
            } else if op == EE_SCREENS_CBW_SET_EDIT_TEXT {
                c.cbh_edit_set_text(edit, t);
            } else {
                c.cbh_edit_select(edit);
                let form = c.cbh_form(a as u32);
                let index = c.cbh_edit_index(edit);
                c.cbh_form_set_tab_stop(form, index);
            }
            0
        }
        EE_SCREENS_CBW_LABEL_TEXT | EE_SCREENS_CBW_SET_LABEL_TEXT | EE_SCREENS_CBW_SET_LABEL_NUMBER => {
            let control = control(c, a, b);
            let label = if control == 0 { 0 } else { c.cbh_as_label(control) };
            if label == 0 {
                return 3;
            }
            if op == EE_SCREENS_CBW_LABEL_TEXT {
                c.cbh_label_text(label, EeScreensShimOut::of(&mut out.text));
            } else if op == EE_SCREENS_CBW_SET_LABEL_TEXT {
                c.cbh_label_set_text(label, t);
            } else {
                c.cbh_label_set_number(label, cc as i32);
            }
            0
        }
        EE_SCREENS_CBW_DISPLAY | EE_SCREENS_CBW_DISPLAY_INDEX => {
            let form = c.cbh_form(a as u32);
            let control = if form == 0 {
                0
            } else if op == EE_SCREENS_CBW_DISPLAY {
                c.cbh_control_by_id(form, b as u32)
            } else {
                c.cbh_control(form, b as u32)
            };
            if control != 0 {
                c.control_set_display_mode(control, (cc != 0) as u32);
            }
            0
        }
        EE_SCREENS_CBW_PUSH => {
            let form = c.cbh_form(a as u32);
            if form == 0 {
                3
            } else {
                c.cbh_push(form) as i64
            }
        }
        EE_SCREENS_CBW_CLOSE_FORM => c.cbh_close_form() as i64,
        EE_SCREENS_CBW_CURRENT_IS => {
            let current = c.cbh_current_form();
            (current == c.cbh_form(a as u32)) as i64
        }
        // Users\default\Civilizations\*.civ
        EE_SCREENS_CBW_FILE_LIST_FILL => {
            let list = file_list(c, 0, a, b);
            c.cbh_path_new();
            c.cbh_spec_volume();
            c.cbh_spec_directory();
            c.cbh_path_add();
            c.cbh_fl_set_paths(list);
            c.cbh_fl_add_extension(list);
            c.cbh_fl_create_entries(list);
            c.cbh_path_release();
            0
        }
        EE_SCREENS_CBW_FILE_SELECTION => {
            let list = file_list(c, p, a, b);
            if list == 0 {
                return -1;
            }
            let mut index: c_ulong = 0;
            let result = c.cbh_fl_selection(list, EeScreensShimULongOut::of(&mut index));
            out.values.push(index as i64);
            result as i64
        }
        EE_SCREENS_CBW_FILE_LINE => {
            let list = file_list(c, p, a, b);
            c.cbh_fl_line(list, cc as u32, EeScreensShimOut::of(&mut out.text));
            0
        }
        EE_SCREENS_CBW_FILE_DELETE => {
            let list = file_list(c, 0, a, b);
            c.cbh_fl_delete(list) as i64
        }
        EE_SCREENS_CBW_IS_FORM => (p == c.cbh_form(a as u32)) as i64,
        EE_SCREENS_CBW_LIST => {
            let control = control(c, a, b);
            if control == 0 {
                0
            } else {
                c.cbh_as_list(control) as i64
            }
        }
        EE_SCREENS_CBW_FORM_EVENTS => {
            let form = c.cbh_form(a as u32);
            let event = c.cbh_screen_event(b as u32);
            c.cbh_form_set_open_event(form, event);
            c.cbh_event_release(event);
            let event = c.cbh_screen_event(cc as u32);
            c.cbh_form_set_close_event(form, event);
            c.cbh_event_release(event);
            0
        }
        EE_SCREENS_CBW_FORM_ID => c.cbh_form_id(p) as i64,
        EE_SCREENS_CBW_STRING => {
            let result = c.cbh_string_load(a as u32);
            c.cbh_string_out(EeScreensShimOut::of(&mut out.text));
            result as i64
        }
        EE_SCREENS_CBW_PERCENT => {
            c.cbh_format_percent();
            c.cbh_print_float(f32::from_bits(a as u32));
            c.cbh_string_out(EeScreensShimOut::of(&mut out.text));
            0
        }
        EE_SCREENS_CBW_RESERVED => {
            let name = c_str(text);
            let reserved = civ::tables().civilizations.iter().any(|entry| {
                let file: Vec<u8> = entry.mFileName.iter().take_while(|&&b| b != 0).map(|&b| b as u8).collect();
                file.eq_ignore_ascii_case(name)
            });
            if !reserved {
                return 0;
            }
            c.cbh_format_reserved();
            c.cbh_print_text(t);
            c.cbh_string_out(EeScreensShimOut::of(&mut out.text));
            1
        }
        EE_SCREENS_CBW_LOOKUP_LEVEL => {
            let upgrade = a as u32;
            let level = civ::tables().category_level(upgrade >> 16, upgrade & 0xffff);
            let (value, bonus) = level.as_ref().map_or((0.0f32, 0.0f32), |l| (l.mValue, l.mBonus));
            out.values.push(value.to_bits() as i64);
            out.values.push(bonus.to_bits() as i64);
            level.is_some() as i64
        }
        EE_SCREENS_CBW_EXTRA_COST => civ::tables().extra_cost(a as u32) as i64,
        EE_SCREENS_CBW_CATEGORY_NAME_ID => {
            let id = civ::tables().category(a as u32).map(|category| category.mNameID);
            out.values.push(id.unwrap_or(0) as i64);
            id.is_some() as i64
        }
        EE_SCREENS_CBW_LEVEL_NAME_ID => civ::ee_tech_civ_level_name_id(a as u32) as i64,
        EE_SCREENS_CBW_IS_POWER_LEVEL => civ::ee_tech_civ_is_power_level(a as u32) as i64,
        EE_SCREENS_CBW_ALL_UPGRADES => {
            let upgrades = civ::tables().upgrades();
            out.values.extend(upgrades.into_iter().map(|u| u as i64));
            0
        }
        EE_SCREENS_CBW_UPGRADE_KEY => {
            let key = civ::tables().upgrade_key(civ::rules(), a as u32);
            out.text.extend_from_slice(key.as_deref().unwrap_or_default());
            key.is_some() as i64
        }
        EE_SCREENS_CBW_QUOTE => {
            c.cbh_quote(EeScreensShimText::of(text), EeScreensShimOut::of(&mut out.text));
            0
        }
        // the civilization's cost members (ee/EECivilization.h: the manager's tables, its purchased upgrades)
        EE_SCREENS_CBW_C_BASE_COST => ee_sim::tech::civilization::ee_tech_civ_base_cost(a as u32) as i32 as i64,
        EE_SCREENS_CBW_C_EXTRA_COST => civ::ee_tech_civ_extra_cost(a as u32) as i64,
        EE_SCREENS_CBW_C_COMPUTE => (a as i32).wrapping_add((b as i32).wrapping_mul(cc as i32)) as i64,
        EE_SCREENS_CBW_C_COUNT => {
            let list: Vec<u32> = (0..c.cbh_civ_purchased_count(p)).map(|i| c.cbh_civ_purchased(p, i)).collect();
            // SAFETY: the copied list
            unsafe { civ::ee_tech_civ_count_in_category(a as u32, list.as_ptr(), list.len()) as i64 }
        }
        EE_SCREENS_CBW_C_AVAILABLE => c.cbh_civ_available(p) as i64,
        EE_SCREENS_CBW_C_UNCHANGED => c.cbh_civ_unchanged(p) as i64,
        EE_SCREENS_CBW_C_MAX => c.cbh_civ_max(p) as i64,
        EE_SCREENS_CBW_C_PURCHASED => {
            let count = c.cbh_civ_purchased_count(p);
            for i in 0..count {
                let upgrade = c.cbh_civ_purchased(p, i);
                out.values.push(upgrade as i64);
            }
            0
        }
        EE_SCREENS_CBW_C_NAME => {
            c.cbh_civ_name(p, EeScreensShimOut::of(&mut out.text));
            0
        }
        // the name's bytes, then the upgrades (little-endian u32s)
        EE_SCREENS_CBW_REQUEST => {
            let split = (b as usize).min(text.len());
            let name = c_str(&text[..split]);
            c.cbh_upgrades_clear();
            let mut at = split;
            while at + 4 <= text.len() {
                c.cbh_upgrades_push(u32::from_le_bytes([text[at], text[at + 1], text[at + 2], text[at + 3]]));
                at += 4;
            }
            let message = c.cbh_request_new(a as u8, EeScreensShimText::of(name));
            if c.cbh_multiplayer() {
                c.cbh_send_server(message);
            } else {
                c.cbh_message_execute(message);
                c.cbh_message_release(message);
            }
            0
        }
        EE_SCREENS_CBW_MULTIPLAYER => c.cbh_multiplayer() as i64,
        EE_SCREENS_CBW_GAME_RESULT => c.cbh_game_result() as i64,
        EE_SCREENS_CBW_PAUSE => c.cbh_pause() as i64,
        EE_SCREENS_CBW_UNPAUSE => c.cbh_unpause() as i64,
        EE_SCREENS_CBW_PLAYER_CIV => {
            let player = c.cbh_current_player();
            if player == 0 {
                0
            } else {
                c.cbh_player_civ(player) as i64
            }
        }
        EE_SCREENS_CBW_PLAYER_ID => c.cbh_player_id() as i64,
        EE_SCREENS_CBW_PLAYER_FLAG => {
            if a == 0 {
                c.cbh_civ_chosen() as i64
            } else {
                c.cbh_civ_locked() as i64
            }
        }
        EE_SCREENS_CBW_SET_PLAYER_FLAG => {
            if a == 0 {
                c.cbh_set_civ_chosen(b != 0);
            } else {
                c.cbh_set_civ_locked(b != 0);
            }
            0
        }
        EE_SCREENS_CBW_FREE_CAMERA => c.cbh_free_camera() as i64,
        EE_SCREENS_CBW_CAN_BUY => c.cbh_can_buy() as i64,
        EE_SCREENS_CBW_CAMPAIGN_NAME => {
            c.cbh_string_clear();
            let found = c.cbh_campaign_name();
            if found {
                c.cbh_string_out(EeScreensShimOut::of(&mut out.text));
            }
            found as i64
        }
        EE_SCREENS_CBW_CUSTOM_CIVS => c.cbh_custom_civs() as i64,
        EE_SCREENS_CBW_SOUND_LOAD => {
            let name: &[u8] = if a != 0 { b"sounds\\button2.wav" } else { b"sounds\\button1.wav" };
            c.cbh_sound_load(EeScreensShimText::of(name)) as i64
        }
        EE_SCREENS_CBW_SOUND_UNLOAD => {
            c.cbh_sound_unload(a as u32);
            0
        }
        EE_SCREENS_CBW_SOUND_PLAY => {
            c.cbh_sound_play(a as u32);
            0
        }
        EE_SCREENS_CBW_MESSAGE => {
            c.cbh_message(t);
            0
        }
        EE_SCREENS_CBW_QUESTION => {
            let answer = match a {
                EE_SCREENS_CBW_ANSWER_WRITE => 0,
                EE_SCREENS_CBW_ANSWER_DELETE => 1,
                _ => 2,
            };
            c.cbh_question(t, p, answer);
            0
        }
        EE_SCREENS_CBW_BLINK_FLAG => {
            c.cbh_set_blink_flag(a != 0);
            0
        }
        EE_SCREENS_CBW_MAIN_BUTTON => (main_button(c, a) != 0) as i64,
        EE_SCREENS_CBW_MAIN_BLINK => {
            let button = main_button(c, a);
            c.cbh_button_blink(button, b != 0);
            0
        }
        EE_SCREENS_CBW_MAIN_DISPLAY => {
            let button = main_button(c, a);
            c.control_set_display_mode(button, (b != 0) as u32);
            0
        }
        EE_SCREENS_CBW_F_DIRECTORY => {
            c.cbh_f_volume();
            c.cbh_f_directory(t);
            0
        }
        EE_SCREENS_CBW_F_NAME => {
            c.cbh_f_name(t);
            0
        }
        EE_SCREENS_CBW_F_OPEN => {
            if a != 0 {
                c.cbh_f_open_write() as i64
            } else {
                c.cbh_f_open_read() as i64
            }
        }
        EE_SCREENS_CBW_F_CLOSE => c.cbh_f_close() as i64,
        EE_SCREENS_CBW_F_DELETE => c.cbh_f_delete() as i64,
        EE_SCREENS_CBW_F_CREATE => c.cbh_f_create() as i64,
        EE_SCREENS_CBW_F_WRITE => c.cbh_f_write(EeScreensShimText::of(text)) as i64,
        EE_SCREENS_CBW_F_FILE => {
            c.cbh_f_file(EeScreensShimOut::of(&mut out.text));
            0
        }
        EE_SCREENS_CBW_STARTUP_DIRECTORY => {
            c.cbh_startup_directory(EeScreensShimOut::of(&mut out.text));
            0
        }
        EE_SCREENS_CBW_RELOAD_PLAYERS => {
            c.cbh_players_clear();
            let mut count = 0i64;
            let players = c.cbh_num_players();
            for i in 1..players {
                if c.cbh_player_civ_file_is(i, t) {
                    c.cbh_players_push(i);
                    count += 1;
                }
            }
            if count > 0 {
                let action = c.cbh_reset_players_new();
                c.cbh_post_action(action);
                c.cbh_action_release(action);
            }
            count
        }
        // the game's file, else (into the same name and upgrades) the user's
        EE_SCREENS_CBW_LOAD_CIVILIZATION => {
            c.cbh_string_clear();
            c.cbh_upgrades_clear();
            let mut result = c.cbh_load_civilization(t, false);
            if result != 0 {
                result = c.cbh_load_civilization(t, true);
            }
            c.cbh_string_out(EeScreensShimOut::of(&mut out.text));
            let count = c.cbh_upgrades_count();
            for i in 0..count {
                let upgrade = c.cbh_upgrades_at(i);
                out.values.push(upgrade as i64);
            }
            result as i64
        }
        _ => 0,
    }
}

/// The list's ops (`EE_SCREENS_CBW_L_*`, `EECBUIListBox::HostOp`) on the list `p`.
#[allow(clippy::too_many_arguments)]
fn list_op<C: Cpp + ?Sized>(
    c: &mut C,
    op: u32,
    p: usize,
    a: i64,
    b: i64,
    cc: i64,
    text: &[u8],
    out: &mut EeScreensCbOut,
) -> i64 {
    let t = EeScreensShimText::of(c_str(text));
    match op {
        EE_SCREENS_CBW_L_COUNT => c.cbl_count(p) as i64,
        EE_SCREENS_CBW_L_LINE => {
            c.cbl_line(p, a as u32, EeScreensShimOut::of(&mut out.text));
            0
        }
        EE_SCREENS_CBW_L_SET_LINE => {
            c.cbl_set_line(p, a as u32, t);
            0
        }
        EE_SCREENS_CBW_L_APPEND => {
            c.cbl_append(p, t);
            0
        }
        EE_SCREENS_CBW_L_DELETE => {
            c.cbl_delete(p, a as u32);
            0
        }
        EE_SCREENS_CBW_L_DELETE_ALL => {
            c.cbl_delete_all(p);
            0
        }
        EE_SCREENS_CBW_L_SELECT => {
            c.cbl_select(p, a as u32, b != 0, cc != 0);
            0
        }
        EE_SCREENS_CBW_L_CURRENT => {
            let mut index: c_ulong = 0;
            let result = c.cbl_current(p, EeScreensShimULongOut::of(&mut index));
            out.values.push(index as i64);
            result as i64
        }
        EE_SCREENS_CBW_L_AUTO => c.cbl_auto(p) as i64,
        EE_SCREENS_CBW_L_HAS => c.cbl_has(p) as i64,
        EE_SCREENS_CBW_L_INDEX => c.cbl_index(p) as i64,
        EE_SCREENS_CBW_L_DIRTY => {
            c.cbl_dirty(p);
            0
        }
        EE_SCREENS_CBW_L_FLAG => {
            (match a {
                EE_SCREENS_CBW_FLAG_UPGRADES_ONLY => c.cbl_upgrades_only(p),
                EE_SCREENS_CBW_FLAG_SHOW_POINTS => c.cbl_show_points(p),
                EE_SCREENS_CBW_FLAG_SORT => c.cbl_sort(p),
                _ => c.cbl_copied_points(p),
            }) as i64
        }
        EE_SCREENS_CBW_L_SET_FLAG => {
            let value = b != 0;
            match a {
                EE_SCREENS_CBW_FLAG_UPGRADES_ONLY => c.cbl_set_upgrades_only(p, value),
                EE_SCREENS_CBW_FLAG_SHOW_POINTS => c.cbl_set_show_points(p, value),
                EE_SCREENS_CBW_FLAG_SORT => c.cbl_set_sort(p, value),
                _ => c.cbl_set_copied_points(p, value),
            }
            0
        }
        EE_SCREENS_CBW_L_DRAW_BEGIN => {
            c.cbl_draw_base(p);
            c.cbl_draw_count(p) as i64
        }
        EE_SCREENS_CBW_L_COPY_LINE => {
            c.cbl_copy_line(p, a as u32, EeScreensShimOut::of(&mut out.text));
            0
        }
        // the line's rectangle (the copied text rectangle, 2 in on the left, 1 out on the right), then the text
        EE_SCREENS_CBW_L_DRAW => {
            c.cbl_rect_copy(p);
            let left = c.cbl_rect_left();
            c.cbl_rect_set_left(left + 2.0);
            let right = c.cbl_rect_right();
            c.cbl_rect_set_right(right + 1.0);
            let height = c.cbl_line_height(p);
            let top = c.cbl_copy_top(p) + (a as u32).wrapping_mul(height) as f32;
            c.cbl_rect_set_top(top);
            c.cbl_rect_set_bottom(top + height as f32);
            let ge_text = c.cbl_text(p);
            let bold = b & EE_SCREENS_CBW_DRAW_BOLD != 0;
            if bold {
                c.cbl_font_bold(ge_text);
            }
            let right = b & EE_SCREENS_CBW_DRAW_RIGHT != 0;
            if right {
                c.cbl_align_right(ge_text);
            } else {
                c.cbl_align_left(ge_text);
            }
            c.cbl_text_set(ge_text, t);
            match cc {
                EE_SCREENS_CBW_COLOUR_HEADER => c.cbl_colour_header(ge_text, p),
                EE_SCREENS_CBW_COLOUR_BLACK => c.cbl_colour(ge_text, 0.0, 0.0, 0.0),
                _ => c.cbl_colour(ge_text, 0.0, 0.0, 0.39),
            }
            if right {
                c.cbl_location_top_right(ge_text);
            } else {
                c.cbl_location_top_left(ge_text);
            }
            let result = c.cbl_draw_clipped(p, ge_text);
            if bold {
                c.cbl_font_plain(ge_text);
            }
            result as i64
        }
        EE_SCREENS_CBW_L_DRAW_END => c.cbl_draw_end(p) as i64,
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    fn run(fake: &mut Fake, op_: u32, p: usize, a: i64, b: i64, cc: i64, text: &[u8]) -> (i64, EeScreensCbOut) {
        let mut out = EeScreensCbOut::default();
        let r = op(fake, op_, p, a, b, cc, text, &mut out);
        (r, out)
    }

    #[test]
    fn an_edit_box_missing_answers_3_without_a_call_on_it() {
        let mut fake = Fake::default();
        let (r, _) = run(&mut fake, EE_SCREENS_CBW_EDIT_TEXT, 0, 7, 410, 0, b"");
        assert_eq!(r, 3);
        assert_eq!(fake.calls(), "cbh_form(7)");
    }

    #[test]
    fn the_edit_select_sets_the_forms_tab_stop() {
        let mut fake = Fake::default();
        fake.answer("cbh_form", &[100, 100]).answer("cbh_control_by_id", &[200]).answer("cbh_as_edit", &[300]);
        fake.answer("cbh_edit_index", &[4]);
        let (r, _) = run(&mut fake, EE_SCREENS_CBW_EDIT_SELECT, 0, 15, 410, 0, b"");
        assert_eq!(r, 0);
        assert_eq!(
            fake.calls(),
            "cbh_form(15)\ncbh_control_by_id(100, 410)\ncbh_as_edit(200)\ncbh_edit_select(300)\ncbh_form(15)\n\
             cbh_edit_index(300)\ncbh_form_set_tab_stop(100, 4)"
        );
    }

    #[test]
    fn the_file_list_fill_makes_the_path_and_lists_the_civ_files() {
        let mut fake = Fake::default();
        fake.answer("cbh_form", &[1]).answer("cbh_control_by_id", &[2]).answer("cbh_as_file_list", &[3]);
        run(&mut fake, EE_SCREENS_CBW_FILE_LIST_FILL, 0, 16, 420, 0, b"");
        assert_eq!(
            fake.calls(),
            "cbh_form(16)\ncbh_control_by_id(1, 420)\ncbh_as_file_list(2)\ncbh_path_new()\ncbh_spec_volume()\n\
             cbh_spec_directory()\ncbh_path_add()\ncbh_fl_set_paths(3)\ncbh_fl_add_extension(3)\n\
             cbh_fl_create_entries(3)\ncbh_path_release()"
        );
    }

    #[test]
    fn the_file_selection_on_a_given_form_reads_the_index() {
        let mut fake = Fake::default();
        fake.answer("cbh_control_by_id", &[2]).answer("cbh_as_file_list", &[3]).answer("cbh_fl_selection", &[0]);
        fake.out("cbh_fl_selection", 5);
        let (r, out) = run(&mut fake, EE_SCREENS_CBW_FILE_SELECTION, 9, 0, 425, 0, b"");
        assert_eq!((r, out.values), (0, vec![5]));
        assert!(fake.calls().starts_with("cbh_control_by_id(9, 425)"));
        let mut fake = Fake::default();
        let (r, _) = run(&mut fake, EE_SCREENS_CBW_FILE_SELECTION, 0, 17, 425, 0, b"");
        assert_eq!(r, -1);
    }

    #[test]
    fn the_request_splits_the_name_and_the_upgrades_and_runs_it_alone() {
        let mut fake = Fake::default();
        fake.answer("cbh_request_new", &[77]);
        let mut text = b"Rome".to_vec();
        text.extend_from_slice(&0x0002_0003u32.to_le_bytes());
        text.extend_from_slice(&5u32.to_le_bytes());
        run(&mut fake, EE_SCREENS_CBW_REQUEST, 0, 2, 4, 0, &text);
        assert_eq!(
            fake.calls(),
            "cbh_upgrades_clear()\ncbh_upgrades_push(131075)\ncbh_upgrades_push(5)\ncbh_request_new(2, \"Rome\")\n\
             cbh_multiplayer()\ncbh_message_execute(77)\ncbh_message_release(77)"
        );
        let mut fake = Fake::default();
        fake.answer("cbh_request_new", &[77]).answer("cbh_multiplayer", &[1]);
        run(&mut fake, EE_SCREENS_CBW_REQUEST, 0, 2, 4, 0, b"Rome");
        assert!(fake.calls().ends_with("cbh_multiplayer()\ncbh_send_server(77)"));
    }

    #[test]
    fn the_reload_posts_one_action_for_the_players_of_the_file() {
        let mut fake = Fake::default();
        fake.answer("cbh_num_players", &[4]).answer("cbh_player_civ_file_is", &[1, 0, 1]);
        fake.answer("cbh_reset_players_new", &[50]);
        let (r, _) = run(&mut fake, EE_SCREENS_CBW_RELOAD_PLAYERS, 0, 0, 0, 0, b"a.civ\0junk");
        assert_eq!(r, 2);
        assert_eq!(
            fake.calls(),
            "cbh_players_clear()\ncbh_num_players()\ncbh_player_civ_file_is(1, \"a.civ\")\ncbh_players_push(1)\n\
             cbh_player_civ_file_is(2, \"a.civ\")\ncbh_player_civ_file_is(3, \"a.civ\")\ncbh_players_push(3)\n\
             cbh_reset_players_new()\ncbh_post_action(50)\ncbh_action_release(50)"
        );
    }

    #[test]
    fn the_load_tries_the_users_directory_after_the_games() {
        let mut fake = Fake::default();
        fake.answer("cbh_load_civilization", &[3, 0]).answer("cbh_upgrades_count", &[2]);
        fake.answer("cbh_upgrades_at", &[9, 8]).text("cbh_string_out", b"Name");
        let (r, out) = run(&mut fake, EE_SCREENS_CBW_LOAD_CIVILIZATION, 0, 0, 0, 0, b"x");
        assert_eq!((r, out.text, out.values), (0, b"Name".to_vec(), vec![9, 8]));
        assert!(fake.calls().contains("cbh_load_civilization(\"x\", false)\ncbh_load_civilization(\"x\", true)"));
    }

    #[test]
    fn the_main_buttons_the_builder_on_its_form_and_the_alert() {
        let mut fake = Fake::default();
        fake.answer("cbh_main_game_form_id", &[1]).answer("cbh_form", &[10]);
        fake.answer("cbh_main_builder_button", &[11]);
        run(&mut fake, EE_SCREENS_CBW_MAIN_BLINK, 0, 0, 1, 0, b"");
        assert_eq!(
            fake.calls(),
            "cbh_main_game_form_id()\ncbh_form(1)\ncbh_main_builder_button(10)\ncbh_button_blink(11, true)"
        );
        let mut fake = Fake::default();
        fake.answer("cbh_main_alert_button", &[12]);
        run(&mut fake, EE_SCREENS_CBW_MAIN_DISPLAY, 0, 1, 0, 0, b"");
        assert_eq!(fake.calls(), "cbh_main_alert_button()\ncontrol_set_display_mode(12, 0)");
    }

    #[test]
    fn the_sound_names_and_the_question_answers() {
        let mut fake = Fake::default();
        run(&mut fake, EE_SCREENS_CBW_SOUND_LOAD, 0, 1, 0, 0, b"");
        run(&mut fake, EE_SCREENS_CBW_QUESTION, 5, EE_SCREENS_CBW_ANSWER_DELETE, 0, 0, b"Delete?");
        assert_eq!(fake.calls(), "cbh_sound_load(\"sounds\\\\button2.wav\")\ncbh_question(\"Delete?\", 5, 1)");
    }

    #[test]
    fn a_line_draws_in_its_rectangle_bold_and_right() {
        let mut fake = Fake::default();
        fake.answer("cbl_line_height", &[12]).answer("cbl_copy_top", &[100]).answer("cbl_text", &[40]);
        fake.answer("cbl_rect_left", &[10]).answer("cbl_rect_right", &[90]).answer("cbl_draw_clipped", &[1]);
        let flags = EE_SCREENS_CBW_DRAW_BOLD | EE_SCREENS_CBW_DRAW_RIGHT;
        let (r, _) = run(&mut fake, EE_SCREENS_CBW_L_DRAW, 7, 2, flags, EE_SCREENS_CBW_COLOUR_DARK_BLUE, b"12");
        assert_eq!(r, 1);
        assert_eq!(
            fake.calls(),
            "cbl_rect_copy(7)\ncbl_rect_left()\ncbl_rect_set_left(12.0)\ncbl_rect_right()\ncbl_rect_set_right(91.0)\n\
             cbl_line_height(7)\ncbl_copy_top(7)\ncbl_rect_set_top(124.0)\ncbl_rect_set_bottom(136.0)\ncbl_text(7)\n\
             cbl_font_bold(40)\ncbl_align_right(40)\ncbl_text_set(40, \"12\")\ncbl_colour(40, 0.0, 0.0, 0.39)\n\
             cbl_location_top_right(40)\ncbl_draw_clipped(7, 40)\ncbl_font_plain(40)"
        );
    }

    #[test]
    fn the_list_flags_by_name() {
        let mut fake = Fake::default();
        fake.answer("cbl_sort", &[1]);
        assert_eq!(run(&mut fake, EE_SCREENS_CBW_L_FLAG, 3, EE_SCREENS_CBW_FLAG_SORT, 0, 0, b"").0, 1);
        run(&mut fake, EE_SCREENS_CBW_L_SET_FLAG, 3, EE_SCREENS_CBW_FLAG_COPIED_POINTS, 1, 0, b"");
        assert_eq!(fake.calls(), "cbl_sort(3)\ncbl_set_copied_points(3, true)");
    }

    #[test]
    fn the_percent_prints_the_value_through_the_format() {
        let mut fake = Fake::default();
        fake.text("cbh_string_out", b" 12.5%");
        let (_, out) = run(&mut fake, EE_SCREENS_CBW_PERCENT, 0, 12.5f32.to_bits() as i64, 0, 0, b"");
        assert_eq!(out.text, b" 12.5%");
        assert_eq!(fake.calls(), "cbh_format_percent()\ncbh_print_float(12.5)\ncbh_string_out(out)");
    }
}
