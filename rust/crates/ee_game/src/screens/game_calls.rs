//! The screens' game calls (`ee_screenscpp_call`'s cases, `UI Events/EEScreens.cpp`) in Rust (UI lane round 7):
//! each case that was more than one C++ statement is here, over the one-statement shims of [`Cpp`]
//! (`game_cpp.rs`). The dispatchers' adapter (`menus_ffi.rs` `Form::op`) asks [`call`] before the C++ host, below
//! the hosts' traits: the screens' replay goldens are unchanged. An op that is not here goes on to the C++ host.
//!
//! Pointers are the C++ objects' addresses (`usize`): the form, a background, an event, a file spec, a server.

// (the casts of the C `long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use core::ffi::c_ulong;

use ee_render::ge::texobj::EeGeTextureDescription;

use crate::screens::game_cpp::{
    Cpp, EeScreensShimI32Out as OutI32, EeScreensShimOut as Out, EeScreensShimPtrOut as OutPtr,
    EeScreensShimText as Text, EeScreensShimULongOut as OutUL,
};
use crate::screens::hot_keys::EE_SCREENS_GAME_HK_ALARM;
use crate::screens::lobby_form::*;
use crate::screens::main_game_options::*;
use crate::screens::menus::*;
use crate::screens::save_load::*;
use crate::screens::scenario_screens::*;
use crate::screens::settings_form::*;

/// `gResource->GetString(id, s)` (the language strings; "" for a missing id).
pub(crate) fn string(id: u32) -> Vec<u8> {
    crate::screens::hud_direct::resource_string(id as i64).unwrap_or(b"").to_vec()
}

/// `UString::Find(dir) != GetLength()` on the two lowered strings: whether `file` names the folder `dir`
/// (lower case already) anywhere.
pub(crate) fn names_folder(file: &[u8], dir: &[u8]) -> bool {
    let lower = file.to_ascii_lowercase();
    dir.is_empty() || lower.windows(dir.len()).any(|w| w == dir)
}

/// `file` below `folder` unless it already names it (the intro's sound and map).
fn in_folder(file: Vec<u8>, folder: &[u8]) -> Vec<u8> {
    if names_folder(&file, &folder.to_ascii_lowercase()) {
        file
    } else {
        [folder, &file].concat()
    }
}

/// The map backdrops' texture description (the scenario intro's, the campaign's, the post-game screenshot's).
pub(crate) fn backdrop_description() -> EeGeTextureDescription {
    EeGeTextureDescription {
        surface_volatility: 1,
        frame_format: 0,
        pixel_depth: 16,
        num_mip_levels_to_drop: 0,
        is_3d_renderable: false,
        is_transparency_enabled: false,
        has_alpha: true,
        create_mipmaps: true,
        use_texture_pack: true,
    }
}

/// A file spec's file name (a new spec filled by `fill`, then deleted).
fn spec_file<C: Cpp + ?Sized>(c: &mut C, fill: impl FnOnce(&mut C, usize)) -> Vec<u8> {
    let spec = c.spec_new();
    fill(c, spec);
    let mut file = Vec::new();
    c.spec_file(spec, Out::of(&mut file));
    c.spec_delete(spec);
    file
}

/// The background id -2 (`GetBackgroundByID(-2)`, `unsigned long`).
const BACKDROP: u32 = 0xffff_fffe;

/// A new map backdrop of `file` on the form at draw priority `priority`, opened between the form's background lock.
pub(crate) fn new_backdrop<C: Cpp + ?Sized>(c: &mut C, form: usize, file: &[u8], priority: u32, load: bool) {
    let description = backdrop_description();
    c.form_lock_background(form);
    let map = c.map_background_new(BACKDROP, Text::of(file), &description as *const _ as usize, form, priority);
    c.background_open(map);
    if load {
        c.background_load(map);
    }
    c.form_unlock_background(form);
}

/// The colours of a lobby slot's name: loaded game, observer, normal, the local player's.
const NAME_COLOURS: [[f32; 3]; 4] = [[1.0, 0.0, 0.0], [1.0, 1.0, 0.0], [1.0, 1.0, 1.0], [0.0, 1.0, 0.0]];

/// The case of `op` (its C++ arguments a..d), `None` for an op that is not here.
pub(crate) fn call<C: Cpp + ?Sized>(c: &mut C, form: usize, op: u32, a: u32, b: u32, cc: u32, d: u32) -> Option<u32> {
    Some(match op {
        // the lobby's (lobby_calls.rs)
        EE_SCREENS_GAME_LE_APPLY_SAVED => {
            crate::screens::lobby_calls::apply_saved(c);
            0
        }
        EE_SCREENS_GAME_LE_BOOT_FREE => {
            crate::screens::lobby_calls::boot_free(c);
            0
        }
        EE_SCREENS_OP_SLIDER_LABEL => {
            // the slider's range event on its label b (with string cc at 0.0)
            let event = c.slider_event_new(form, b);
            c.control_set_range_trigger(form, a, event);
            if cc != 0 {
                let s = string(cc);
                c.slider_event_add_string(event, Text::of(&s));
            }
            c.event_dereference(event);
            0
        }
        EE_SCREENS_GAME_VIEW => {
            // the game view rectangle the settings reset (left, top, right, bottom)
            c.set_game_view(0.0, 35.0, 1600.0, 1085.0);
            0
        }
        EE_SCREENS_GAME_TEST_SOUND => {
            let sound = c.sound_object(a);
            if sound != 0 {
                c.sound_load_media(sound);
                c.sound_play(sound);
            }
            0
        }
        EE_SCREENS_GAME_HK_ALARM => {
            if c.sound_hot_key_alarm() != 0 {
                let sound = c.sound_hot_key_alarm();
                c.sound_play(sound);
            }
            0
        }
        EE_SCREENS_GAME_WINDOW_MODE => {
            // the settings' window mode on the game window (the caller resizes the game)
            let game_window = c.game_window();
            let window = if game_window != 0 { c.game_window_os(game_window) } else { 0 };
            if window == 0 {
                return Some(0);
            }
            if c.settings_window_mode() == 0 {
                c.window_set_fixed_size(window, 0);
                c.window_set_fullscreen(window, 1);
            } else {
                let (mut width, mut height) = (0i32, 0i32);
                c.window_set_fullscreen(window, 0);
                c.window_set_fixed_size(window, 0);
                if c.display_logical_size(OutI32::of(&mut width), OutI32::of(&mut height)) != 0 && width > 0 {
                    c.window_set_size(window, width * 4 / 5, height * 4 / 5);
                }
                c.window_set_fixed_size(window, 1);
            }
            0
        }
        EE_SCREENS_GAME_CIV_BUILDER => {
            c.civ_clear_upgrades();
            c.civ_reset_points();
            let points = c.civ_builder_points();
            c.civ_set_points_and_max(points);
            c.open_civ_builder();
            0
        }
        EE_SCREENS_GAME_LOBBY_BEGIN => {
            c.lobby_options_write_lock();
            c.options_from_last_game();
            0
        }
        EE_SCREENS_OP_POST_EVENT => {
            let event = c.screen_event_new(a);
            c.post_event(event);
            c.event_dereference(event);
            0
        }
        EE_SCREENS_GAME_PAUSE_SERVER => {
            // (the save screen's original did not check the cast: a client crashed there)
            let server = c.server_dynamic();
            if server != 0 && !c.server_paused(server) {
                c.server_pause(server);
                c.post_pause_information(true);
            }
            0
        }
        EE_SCREENS_GAME_SI_SET_FLAG => {
            for i in 0..4 {
                if a == 1 << i {
                    c.intro_set_flag(i, b != 0);
                }
            }
            0
        }
        EE_SCREENS_GAME_SI_PLAY_SOUND => {
            // the briefing sound once (below Sounds\ unless its file names the folder)
            let file = spec_file(c, |c, spec| {
                c.intro_get_sound(spec);
            });
            let file = in_folder(file, b"Sounds\\");
            c.sound_set_latency(500);
            let (mut sample, mut id): (c_ulong, c_ulong) = (0, a as c_ulong);
            c.load_sound(Text::of(&file), OutUL::of(&mut sample), OutUL::of(&mut id));
            c.sound_register_one_shot(sample as u32, id as u32);
            id as u32
        }
        EE_SCREENS_GAME_SI_MAP_BACKDROP => {
            // the scenario's map picture as the form's backdrop (-2), unless it already is
            let background = c.form_background(form, BACKDROP);
            let file = spec_file(c, |c, spec| {
                c.intro_get_map(spec);
            });
            let file = in_folder(file, b"Scenarios\\");
            let mut current = Vec::new();
            if background != 0 {
                c.background_texture_file(background, Out::of(&mut current));
            }
            if background == 0 || current != file {
                if background != 0 {
                    c.form_remove_background(form, background);
                }
                if c.intro_open_map() {
                    new_backdrop(c, form, &file, 2, false);
                }
            }
            0
        }
        EE_SCREENS_GAME_SI_START_GAME => {
            // a single player game of the scenario (a: the campaign's), a new local server, start loading
            c.options_write_lock();
            c.options_set_defaults();
            if a != 0 {
                c.options_set_load_type(4);
                c.options_campaign_difficulty();
                c.options_campaign_name();
                c.options_campaign_checksum();
                c.options_campaign_scenario();
                c.options_campaign_first_map();
            } else {
                c.options_set_load_type(1);
                c.load_scenario_name_from_intro();
            }
            c.options_map_type_from_intro();
            c.options_set_multiplayer(false);
            c.options_write_unlock();
            new_local_server(c);
            c.server_stop_ping();
            c.server_set_load_state(true);
            c.set_local_player_out(false);
            c.send_scenario_start_loading();
            0
        }
        EE_SCREENS_GAME_CS_MAP_BACKDROP => {
            // the campaign's background image (a file of Scenarios) as the form's backdrop (-2)
            let mut name = Vec::new();
            c.campaign_background_image(Out::of(&mut name));
            let mut prefix = Vec::new();
            c.scenario_prefix(Out::of(&mut prefix));
            let file = [&b"Scenarios\\"[..], &prefix, &name].concat();
            let mut spec = 0usize;
            if c.data_find_file(Text::of(&file), OutPtr::of(&mut spec)) == 0 {
                new_backdrop(c, form, &file, 2, false);
                if spec != 0 {
                    c.spec_delete(spec);
                }
            }
            0
        }
        EE_SCREENS_GAME_CS_SET_DIFFICULTY => {
            c.campaign_set_difficulty(a as i32);
            c.set_difficulty_level(a as i32);
            c.set_difficulty_level2(a as i32);
            c.options_write_lock();
            c.options_set_difficulty(a as i32);
            c.options_write_unlock();
            0
        }
        EE_SCREENS_GAME_LE_LOCK => {
            match a {
                EE_SCREENS_LE_READ_LOCK => c.options_read_lock(),
                EE_SCREENS_LE_READ_UNLOCK => c.options_read_unlock(),
                EE_SCREENS_LE_WRITE_LOCK => c.options_write_lock(),
                _ => c.options_write_unlock(),
            }
            0
        }
        EE_SCREENS_GAME_LE_DIFFICULTY_LEVELS => {
            c.set_difficulty_level(a as i32);
            c.set_difficulty_level2(a as i32);
            0
        }
        EE_SCREENS_OP_LE_NAME_COLOUR => {
            // a slot's name colour (the type box's label too)
            let [r, g, bl] = NAME_COLOURS[(b & 3) as usize];
            if cc != 0 {
                c.combo_text_colour(form, a, r, g, bl);
            } else {
                c.label_text_colour(form, a, r, g, bl);
            }
            0
        }
        EE_SCREENS_GAME_LE_REMOVE_HOST => {
            let action = c.remove_player_new(0xfe);
            c.post_action(action);
            c.action_dereference(action);
            0
        }
        EE_SCREENS_GAME_LE_WON_REJOIN => {
            // the lobby's Cancel: back to WON's room (the form closes unless that failed)
            let mut close = true;
            if c.won_net() != 0 {
                c.won_clear_game_list();
                close = c.won_rejoin_room(true);
            }
            close as u32
        }
        EE_SCREENS_GAME_LE_WON_LEAVE => {
            if c.won_net() != 0 {
                c.won_leave_room();
            }
            0
        }
        EE_SCREENS_GAME_LE_START_SERVER => {
            // Start: the server closed to new players (WON's game told the humans and players), its transfers reset
            c.won_info_reset();
            c.lobby_start_time_now();
            c.server_allow_new_players(false);
            c.server_set_game_state(2);
            c.server_set_load_responses(false);
            if c.won_net() != 0 {
                c.won_info_load();
                let flags = c.won_info_flags();
                c.won_info_set_flags(flags & !1);
                c.won_info_set_players(b as u8);
                c.won_info_set_humans(a as u8);
                c.won_info_store();
            }
            c.server_stop_ping();
            c.server_reset_xfer();
            c.server_set_load_state(true);
            c.server_allow_new_players(false);
            0
        }
        EE_SCREENS_GAME_LE_FULL_PLAYER => {
            if a < 16 {
                c.send_full_player(a);
            }
            0
        }
        EE_SCREENS_GAME_LE_FIND_GENERATOR => {
            let mut line: c_ulong = 0;
            let options = crate::screens::lobby_calls::options_of(c, a);
            let found = c.find_generator(options, OutUL::of(&mut line)) == 0;
            (line as u32 & 0x7fff_ffff) | if found { 0 } else { 0x8000_0000 }
        }
        EE_SCREENS_GAME_LE_BOOT_REFUSE => {
            // the boot answered yes: WON's counts, the slot's client refused (the copy, reset)
            let server = c.server_dynamic();
            c.won_info_reset();
            if c.won_net() != 0 {
                c.won_info_load();
            }
            if a != 0 {
                let humans = c.won_info_humans();
                c.won_info_set_humans(humans.wrapping_sub(1));
            }
            if a == 4 {
                let players = c.won_info_players();
                c.won_info_set_players(players.wrapping_sub(1));
            }
            if server != 0 && c.lobby_boot() != 0 {
                c.server_refuse_boot(server);
            }
            if c.won_net() != 0 {
                c.won_info_store();
            }
            0
        }
        EE_SCREENS_GAME_LE_XFER => {
            // a joined human's transfer of the loaded file: the old one aborted, the new one's id
            if !c.lobby_has_file() {
                return Some(0);
            }
            c.server_abort_xfer(b);
            let mut id: c_ulong = 0;
            c.server_xfer_lobby_file(a as u8, OutUL::of(&mut id));
            id as u32
        }
        EE_SCREENS_GAME_GO_TO_EDITOR => {
            // back to the scenario editor from its single player test game: the simulation stopped, the editor
            // posted
            c.physics_post(0x402);
            c.post_enter_scenario_editor();
            c.post_pause_information(true);
            0
        }
        EE_SCREENS_GAME_GO_USER_PAUSE => {
            // (1: our reference dropped, as the main game form's pause did; the options menu's original kept it)
            let action = c.user_pause_new();
            c.post_action(action);
            if a != 0 {
                c.action_dereference(action);
            }
            0
        }
        EE_SCREENS_GAME_GO_TO_SCENARIO_LOAD => {
            // Play Scenario: the scenario load form swapped in for the menu (keeping its music), the game paused, the
            // main game form remembered
            const SCENARIO_LOAD: u32 = 20; // kFormScenarioLoad
            let mut music = Vec::new();
            c.form_music(SCENARIO_LOAD, Out::of(&mut music));
            c.form_set_music(SCENARIO_LOAD, Text::of(b""));
            c.manager_pop();
            c.manager_swap(SCENARIO_LOAD);
            c.form_set_music(SCENARIO_LOAD, Text::of(&music));
            let server = c.server_dynamic();
            c.server_pause(server);
            c.post_pause_information(true);
            c.intro_set_flag(0, false);
            c.intro_set_flag(1, true);
            c.save_main_game_form();
            0
        }
        _ => return crate::screens::game_calls_more::call(c, form, op, a, b, cc, d),
    })
}

/// `gCommunications` replaced by a new local server (the old one deleted), its threads started, no load
/// responses (the scenario start's and the random map's).
pub(crate) fn new_local_server<C: Cpp + ?Sized>(c: &mut C) {
    c.delete_communications();
    c.new_local_server();
    c.server_start_threads();
    c.server_set_load_responses(false);
}

#[cfg(test)]
#[path = "game_calls_tests.rs"]
mod tests;
