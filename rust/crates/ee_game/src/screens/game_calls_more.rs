//! More of the screens' game calls (`ee_screenscpp_call`'s chain, `UI Events/EEScreens.cpp`) in Rust (UI lane
//! round 7), as `game_calls.rs`: the hot keys', the chat form's, the main game form's, the diplomacy form's, the
//! post-game summary's, the loading screen's, the civilization choice's and the campaign editor's cases that were
//! more than one C++ statement, over the one-statement shims of [`Cpp`].

use core::ffi::c_ulong;

use crate::screens::campaign_editor::{EE_SCREENS_GAME_CE_OPEN, EE_SCREENS_OP_CE_BG_NONE};
use crate::screens::chat::EE_SCREENS_OP_CH_SEND_KEYS;
use crate::screens::civ_choice::{EE_SCREENS_GAME_PDCS_CIVS, EE_SCREENS_GAME_PDCS_REQUEST, EE_SCREENS_OP_PDCS_STYLE};
use crate::screens::diplomacy::{EE_SCREENS_OP_DS_FG, EE_SCREENS_OP_DS_UV};
use crate::screens::game_calls::string;
use crate::screens::game_cpp::{Cpp, EeScreensShimOut as Out, EeScreensShimText as Text};
use crate::screens::hot_keys::{EE_SCREENS_GAME_HK_CENTER, EE_SCREENS_GAME_HK_PLAYER_EVENT};
use crate::screens::lobby_form::EE_SCREENS_GAME_LE_RANDOM_MAP_OPTIONS;
use crate::screens::main_game::{
    EE_SCREENS_GAME_MG_CLEAR_GHOSTS, EE_SCREENS_GAME_MG_DUMP_MEDIA, EE_SCREENS_GAME_MG_FOLLOW_LEAD,
    EE_SCREENS_GAME_MG_QUICK_SAVE, EE_SCREENS_GAME_MG_SELECTED_COMMAND, EE_SCREENS_GAME_MG_SET_FORMATION,
    EE_SCREENS_GAME_MG_SHIFT, EE_SCREENS_GAME_MG_TO_EDITOR,
};
use crate::screens::post_game::{
    EE_SCREENS_GAME_PGS_CAMPAIGN_CLEAR, EE_SCREENS_GAME_PGS_CAMPAIGN_SETUP, EE_SCREENS_GAME_PGS_REJOIN,
    EE_SCREENS_GAME_PGS_SCREENSHOT, EE_SCREENS_OP_PGS_NEW_BAR, EE_SCREENS_OP_PGS_NEW_STAR, EE_SCREENS_OP_PGS_REMOVE,
    EE_SCREENS_OP_PGS_TEXT_STYLE,
};
use crate::screens::splash::{EE_SCREENS_GAME_SS_LOCK, EE_SCREENS_OP_SS_BACKGROUND};

fn f32_of(bits: u32) -> f32 {
    f32::from_bits(bits)
}

/// `TMGetTimeOfDay`'s `TMTime` (month 0-based, day, year, hour, minute, second).
#[repr(C)]
#[derive(Default)]
struct Time([c_ulong; 6]);

fn time_of_day<C: Cpp + ?Sized>(c: &mut C) -> Time {
    let mut t = Time::default();
    c.time_of_day(&mut t as *mut Time as usize);
    t
}

/// `kPostGameGoldStar`, the gold star's template control (ee/EEUIManager.h).
const GOLD_STAR: u32 = 790;
/// `IDS_SE_MAP_NONE` ("<NONE>").
const MAP_NONE: u32 = 20009;
/// `DIK_RETURN`.
const KEY_RETURN: u32 = 0x1c;

/// The ghost units cleared under their lock.
fn clear_ghosts<C: Cpp + ?Sized>(c: &mut C) {
    c.ghosts_lock_enter();
    c.world_clear_ghosts();
    c.ghosts_lock_leave();
}

/// The combo box's "none" line, selected (the campaign editor's open and an unknown background's).
fn no_background<C: Cpp + ?Sized>(c: &mut C, form: usize, control: u32) {
    let none = string(MAP_NONE);
    c.combo_create(form, control);
    c.combo_insert_line(form, control, Text::of(&none), 0);
    c.combo_select(form, control, 0);
}

/// The texture's stages as the UI's pictures set them (modulate, the filters, point MIP filter, `bias`, clamped).
pub(crate) fn ui_texture_stages<C: Cpp + ?Sized>(c: &mut C, texture: usize, bias: f32) {
    c.texture_alpha_modulate(texture);
    c.texture_color_modulate(texture);
    c.texture_min_filter(texture, 1);
    c.texture_mag_filter(texture, 1);
    c.texture_mip_point(texture);
    c.texture_mip_bias(texture, bias);
    c.texture_clamp(texture);
}

/// The case of `op`, `None` for an op that is not here.
pub(crate) fn call<C: Cpp + ?Sized>(c: &mut C, form: usize, op: u32, a: u32, b: u32, cc: u32, d: u32) -> Option<u32> {
    Some(match op {
        // the hot keys' (hot_keys.rs)
        EE_SCREENS_GAME_HK_PLAYER_EVENT => {
            let action = c.player_event_new();
            c.post_action(action);
            c.action_dereference(action);
            0
        }
        EE_SCREENS_GAME_HK_CENTER => {
            // the camera on the unit, the mini map refreshed, the camera no longer following
            let (x, y, z) = (c.hot_key_unit_x(a), c.hot_key_unit_y(a), c.hot_key_unit_z(a));
            c.camera_set_changed();
            c.camera_set_location(x, y, z, false, true);
            c.mini_map_refresh();
            c.set_camera_following(false);
            0
        }
        // the chat form's (chat.rs): Enter, one event for the hot key and the edit box's Enter
        EE_SCREENS_OP_CH_SEND_KEYS => {
            let key = c.hot_key_new();
            let event = c.screen_event_new(a);
            c.hot_key_set_key(key, KEY_RETURN);
            c.hot_key_set_just_pressed(key, event);
            c.chat_set_enter_trigger(form, event);
            c.event_dereference(event);
            c.form_add_hot_key(form, key);
            c.hot_key_delete(key);
            0
        }
        // the main game form's (main_game.rs)
        EE_SCREENS_GAME_MG_CLEAR_GHOSTS => {
            clear_ghosts(c);
            0
        }
        EE_SCREENS_GAME_MG_DUMP_MEDIA => {
            // Media Dump.txt in the startup folder (replaced when a), the loaded media (and leaks when b) appended
            let file = c.spec_new();
            c.spec_set_volume_startup(file);
            c.spec_set_directory_startup(file, Text::of(b""));
            c.spec_set_file(file, Text::of(b"Media Dump.txt"));
            if a != 0 {
                c.spec_delete_file(file);
            }
            c.spec_create(file);
            let r = c.spec_fopen_append(file);
            if r == 0 {
                c.add_chat_log(Text::of(b"Dumped Media Information."), r);
                c.world_dump_media(file, b != 0);
                c.spec_close(file);
            }
            c.spec_delete(file);
            r as u32
        }
        EE_SCREENS_GAME_MG_SHIFT => {
            if a != 0 {
                c.send_enter_shift();
            } else {
                c.send_exit_shift();
            }
            0
        }
        EE_SCREENS_GAME_MG_TO_EDITOR => {
            // back from a test game to the scenario editor: the ghosts cleared, the player to the editor slot, the
            // simulation stopped, the editor switch and the pause posted
            c.protocol_clear();
            clear_ghosts(c);
            c.protocol_load();
            c.world_move_player_to_editor();
            c.physics_post(0x402);
            let r = c.post_enter_scenario_editor();
            c.post_pause_information(true);
            r as u32
        }
        EE_SCREENS_GAME_MG_QUICK_SAVE => {
            // the timestamped quick save (remembered in the settings) to every player
            let t = time_of_day(c);
            let extension = {
                let mut e = Vec::new();
                c.saved_game_extension(Out::of(&mut e));
                e
            };
            let [month, day, year, hour, minute, second] = t.0;
            let mut name = format!(
                "QuickSave {:4}.{:02}.{:02} - {:02}h.{:02}m.{:02}s",
                year,
                month.wrapping_add(1),
                day,
                hour,
                minute,
                second
            )
            .into_bytes();
            name.extend_from_slice(&extension);
            c.settings_set_last_quick_save(Text::of(&name));
            c.preferences_save();
            c.send_save_to_all(Text::of(&name));
            0
        }
        EE_SCREENS_GAME_MG_FOLLOW_LEAD => {
            let player = c.current_player();
            let lead = c.lead_unit_id(player as u64);
            let unit = c.world_unit(lead);
            c.camera_follow(unit, a != 0);
            if a == 0 {
                c.camera_set_changed();
            }
            0
        }
        EE_SCREENS_GAME_MG_SELECTED_COMMAND => {
            let unit = c.main_group_selected_unit();
            if unit == 0 || !c.unit_has_complex(unit) || !c.unit_has_class(unit) {
                return Some(0);
            }
            c.unit_class_command(unit, a)
        }
        EE_SCREENS_GAME_MG_SET_FORMATION => {
            // the selected group's formation, posted; the formation menu closed
            let formation = c.formation_new(a as i32);
            c.send_change_formation(a as i32);
            c.main_form_group_disable(8);
            c.main_form_background_visible(0x98, false);
            c.formation_set_b5c(formation);
            c.selected_group_set_formation(formation);
            c.formation_dereference(formation);
            0
        }
        // the diplomacy form's (diplomacy.rs)
        EE_SCREENS_OP_DS_UV => {
            // the bitmap's corners: top left (b, 0), top right (cc, 0), bottom right (b, d), bottom left (cc, d)
            let (u0, u1, v) = (f32_of(b), f32_of(cc), f32_of(d));
            c.bitmap_set_corners(form, a, u0, 0.0, u1, 0.0, u0, v, u1, v);
            0
        }
        EE_SCREENS_OP_DS_FG => {
            // a player's colour on a button's (cc) or a label's text
            if cc != 0 {
                c.button_text_player_colour(form, a, b);
            } else {
                c.label_text_player_colour(form, a, b);
            }
            0
        }
        // the post-game summary's (post_game.rs)
        EE_SCREENS_OP_PGS_NEW_BAR => {
            let template = c.form_control(form, a);
            let id = c.control_id(template).wrapping_add(b).wrapping_sub(1);
            let index = c.control_index(template).wrapping_add(b).wrapping_sub(1);
            let border = c.control_border(template);
            let bar = c.pgs_status_bar_new(id, form, index, border);
            c.status_bar_clear_text(bar);
            c.form_add_control(form, bar, cc);
            c.control_index(bar)
        }
        EE_SCREENS_OP_PGS_TEXT_STYLE => {
            let id = c.control_string_id(form, a);
            c.control_set_string_id(form, b, id);
            c.bar_copy_text_style(form, a, b);
            0
        }
        EE_SCREENS_OP_PGS_NEW_STAR => {
            // the gold star: the template's rectangle, the texture "textures\gold star"
            let mut description = crate::screens::game_calls::backdrop_description();
            description.pixel_depth = c.form_ui_bit_depth(form) as c_ulong;
            let texture = c.texture_load(Text::of(b"textures\\gold star"), &description as *const _ as usize);
            ui_texture_stages(c, texture, 0.0);
            let bitmap = c.bitmap_new();
            let screen = c.form_screen(form);
            c.bitmap_set_texture(bitmap, texture, screen);
            let screen = c.form_screen(form);
            c.bitmap_stretch_to_control(bitmap, form, GOLD_STAR, 32.0, 32.0, screen);
            let star = c.ui_bitmap_new(a, bitmap, form, GOLD_STAR, b);
            c.form_add_control(form, star, cc);
            c.bitmap_dereference(bitmap);
            0
        }
        EE_SCREENS_GAME_PGS_SCREENSHOT => {
            // the last end-game screenshot of today as the backdrop
            let t = time_of_day(c);
            let [month, day, year, ..] = t.0;
            let pattern = format!("EEPostGame-{}-{}-{}-???.jpg", month.wrapping_add(1), day, year).into_bytes();
            let search = c.spec_new();
            let found = c.spec_new();
            c.spec_set_volume_startup(search);
            c.spec_set_directory_startup(search, Text::of(b""));
            if c.spec_first_file(search, Text::of(&pattern), found) == 0 {
                while c.spec_next_file(search, found) == 0 {}
                let mut file = Vec::new();
                c.spec_file(found, Out::of(&mut file));
                crate::screens::game_calls::new_backdrop(c, form, &file, 3, true);
            }
            c.spec_delete(found);
            c.spec_delete(search);
            0
        }
        EE_SCREENS_OP_PGS_REMOVE => {
            let control = c.form_control(form, a);
            c.form_remove_control(form, control, b);
            if control != 0 {
                c.control_delete(control);
            }
            0
        }
        EE_SCREENS_GAME_PGS_REJOIN => {
            if c.won_net() != 0 {
                c.won_rejoin_room(false);
            }
            0
        }
        EE_SCREENS_GAME_PGS_CAMPAIGN_SETUP => {
            c.pgs_campaign_first_from_civ();
            c.pgs_campaign_description();
            let open = c.pgs_campaign_is_demo() || c.pgs_campaign_is_learning();
            c.pgs_campaign_set_all_open(open);
            0
        }
        EE_SCREENS_GAME_PGS_CAMPAIGN_CLEAR => {
            c.pgs_campaign_set_name(Text::of(b""));
            c.pgs_campaign_set_first(Text::of(b""));
            c.pgs_campaign_set_description(Text::of(b""));
            0
        }
        // the loading screen's (splash.rs)
        EE_SCREENS_GAME_SS_LOCK => {
            if a != 0 {
                c.lobby_options_read_lock();
            } else {
                c.lobby_options_read_unlock();
            }
            0
        }
        EE_SCREENS_OP_SS_BACKGROUND => {
            // the progress background's bottom at a, shown
            let background = c.form_background(form, 0x84);
            let (left, top, right) =
                (c.background_left(background), c.background_top(background), c.background_right(background));
            c.background_set_rect(background, left, top, right, f32_of(a));
            c.background_set_visible(background, true);
            0
        }
        // the civilization choice form's (civ_choice.rs)
        EE_SCREENS_OP_PDCS_STYLE => {
            let list = c.form_control(form, a);
            c.cb_list_set_auto_select(list, false);
            if b & 2 != 0 {
                c.cb_list_set_show_points(list, false);
                c.cb_list_set_select_upgrades_only(list, false);
            }
            c.cb_list_set_sort(list, b & 1 != 0);
            c.cb_list_set_colour(list, f32_of(cc), f32_of(cc), f32_of(d));
            0
        }
        EE_SCREENS_GAME_PDCS_CIVS => {
            c.pdcs_civs_clear();
            c.pdcs_civs_get();
            c.pdcs_civs_count()
        }
        EE_SCREENS_GAME_PDCS_REQUEST => {
            let request = c.civ_request_new();
            if c.comms_multiplayer() {
                c.send_to_server(request);
            } else {
                c.message_execute(request);
                c.message_dereference(request);
            }
            0
        }
        // the campaign editor's (campaign_editor.rs)
        EE_SCREENS_GAME_CE_OPEN => {
            // the scenarios' .jpg files (in either case) after the "none" line
            let directory = c.spec_new();
            let path = c.path_new();
            c.spec_set_volume_startup(directory);
            c.spec_set_directory_startup(directory, Text::of(b"data\\Scenarios\\"));
            c.path_add_directory(path, directory);
            let none = string(MAP_NONE);
            c.combo_add_extension(form, a, Text::of(b".jpg"));
            c.combo_add_extension(form, a, Text::of(b".JPG"));
            c.combo_set_paths(form, a, path);
            c.combo_create(form, a);
            c.combo_insert_line(form, a, Text::of(&none), 0);
            c.combo_select(form, a, 0);
            c.path_dereference(path);
            c.spec_delete(directory);
            0
        }
        EE_SCREENS_OP_CE_BG_NONE => {
            no_background(c, form, a);
            0
        }
        // the lobby's game type: the options copied (the assignment checks each value) with load type 0, and back
        EE_SCREENS_GAME_LE_RANDOM_MAP_OPTIONS => {
            if !c.lobby_copy_exists() {
                c.lobby_copy_new();
            }
            c.lobby_copy_from_options();
            c.lobby_copy_clear_load_type();
            c.options_from_lobby_copy();
            0
        }
        _ => return crate::screens::screens_calls::call(c, form, op, a, b, cc, d),
    })
}

#[cfg(test)]
#[path = "game_calls_more_tests.rs"]
mod tests;
