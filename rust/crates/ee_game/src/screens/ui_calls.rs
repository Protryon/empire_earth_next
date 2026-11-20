//! The UI thread's calls (`User Interface Thread/EEUserInterface.cpp`) in Rust (UI lane round 7): the members
//! `ChangeRasterizer`, `ResizeGameScreen`, `ShowMessageOfTheDay`, `OpenSelectServer`, `PauseGraphics` and
//! `UpdateInterface` (their C++ bodies forward here), and the multi-statement cases of the UI object's hosts
//! (`ee_screenscpp_ui_game`, `ee_screenscpp_ui_message`, `ee_screenscpp_ui_event`, `ee_screenscpp_action`), over the
//! one-statement shims of [`Cpp`] (`game_cpp.rs`). The hosts' adapters ask these before the C++ host.

use std::sync::Mutex;

use crate::screens::actions::{EE_SCREENS_ACTION_LOCK, EE_SCREENS_ACTION_UNLOCK};
use crate::screens::civ_choice::{EE_SCREENS_PDCS_CLOSE, EE_SCREENS_PDCS_OPEN};
use crate::screens::game_calls::string;
use crate::screens::game_cpp::{Cpp, EeScreensShimOut as Out, EeScreensShimText as Text};
use crate::screens::ui_game::*;
use crate::screens::ui_thread::{
    EE_SCREENS_UI_EV_IME_UPDATE, EE_SCREENS_UI_EV_KEYBOARD, EE_SCREENS_UI_EV_MASTER_VOLUME, EE_SCREENS_UI_EV_MOUSE,
    EE_SCREENS_UI_EV_MUSIC_VOLUME, EE_SCREENS_UI_OP_DIALOG_LOG, EE_SCREENS_UI_OP_HOST_GONE, EE_SCREENS_UI_OP_LOCK,
    EE_SCREENS_UI_OP_PROCESS_EVENT, EE_SCREENS_UI_OP_QUICK_LOAD, EE_SCREENS_UI_OP_UNLOCK,
};

// the strings and forms (ee/EEStringIDs.h, ee/EEUIManager.h)
/// cbindgen:ignore
const STRING_ENGLISH: u32 = 6;
/// cbindgen:ignore
const STRING_SAVE_GAME_ISNT_VALID: u32 = 7153;
/// cbindgen:ignore
const STRING_THERE_WAS_ERROR_TRYING_TO_LOAD: u32 = 10047;
const FORM_CIV_BUILDER: u32 = 7;
const FORM_MULTIPLAYER: u32 = 30;
const FORM_PREDEFINED_CIV: u32 = 35;
const FORM_SELECT_SERVER: u32 = 38;
const MULTIPLAYER_PASSWORD: u32 = 22;
const MULTIPLAYER_REMEMBER_PASSWORD: u32 = 28;
const MULTIPLAYER_MESSAGE_OF_THE_DAY: u32 = 29;
/// The mouse pointer thread's hide and show messages.
const POINTER_HIDE: u32 = 0x402;
const POINTER_SHOW: u32 = 0x404;
const POINTER_SURFACE: i64 = 0x405;
/// `kDisplayModeShown`.
const DISPLAY_SHOWN: u32 = 1;

/// The civilization builder's menu music while the builder runs (`EE_SCREENS_UG_OP_CB_MUSIC`'s static).
static BUILDER_MUSIC: Mutex<Vec<u8>> = Mutex::new(Vec::new());

/// `EEUserInterface::PauseGraphics`: the graphics thread paused, waited for.
pub(crate) fn pause_graphics<C: Cpp + ?Sized>(c: &mut C) {
    c.gfx_post_pause();
    while !c.gfx_paused() {
        c.sleep(1);
    }
}

/// `EEUserInterface::ResizeGameScreen`: the game screen resized (unless the size is unchanged and not forced), the
/// forms laid out again and the mini map redrawn.
pub(crate) fn resize_game_screen<C: Cpp + ?Sized>(c: &mut C, width: u32, height: u32, force: bool, flag: bool) {
    let was_shown = c.pointer_shown();
    if width == c.game_window_width() && height == c.game_window_height() && !force {
        return;
    }
    c.pointer_post(POINTER_HIDE);
    while c.pointer_shown() {
        c.sleep(1);
    }
    while c.pointer_shown() {
        c.sleep(1);
    }
    c.change_game_screen(width, height, flag);
    c.manager_relayout_all();
    if was_shown {
        c.pointer_post(POINTER_SHOW);
    }
    if c.mini_map() == 0 {
        return;
    }
    c.world_write_lock();
    c.mini_map_redraw_all();
    c.world_write_unlock();
}

/// `EEUserInterface::ChangeRasterizer`: the screens released, the old rasterizer deactivated, the new one set and
/// the game screen at the menus' size.
pub(crate) fn change_rasterizer<C: Cpp + ?Sized>(c: &mut C, rasterizer: i32, screen: i32) {
    c.game_window_client_origin();
    c.pointer_post(POINTER_HIDE);
    while c.pointer_shown() {
        c.sleep(1);
    }
    let mut i = 0;
    while i < c.rasterizer_screen_count() {
        c.rasterizer_release_screen(i);
        i += 1;
    }
    c.delete_effect_batch_buffers();
    let r = c.rasterizer_deactivate();
    c.verify_rasterizer_deactivated(r);
    c.rasterizer_select(rasterizer);
    c.gfx_post_set_rasterizer(screen);
    c.manager_change_rasterizer();
    let (w, h) = (c.menu_width(), c.menu_height());
    resize_game_screen(c, w, h, true, false);
}

/// `EEUserInterface::ShowMessageOfTheDay`: WON's game and system messages on the multiplayer form when visible.
pub(crate) fn show_message_of_the_day<C: Cpp + ?Sized>(c: &mut C, game: usize, system: usize) {
    c.manager_lock_for_copy();
    if c.manager_form_visible(FORM_MULTIPLAYER) {
        let label = c.manager_form_control(FORM_MULTIPLAYER, MULTIPLAYER_MESSAGE_OF_THE_DAY);
        c.control_set_display_mode(label, DISPLAY_SHOWN);
        c.label_delete_array(label);
        c.label_append_wide(label, game);
        c.label_append_wide(label, system);
    }
    c.manager_unlock_for_copy();
}

/// `EEUserInterface::OpenSelectServer`: WON's server selection (the password remembered when asked).
pub(crate) fn open_select_server<C: Cpp + ?Sized>(c: &mut C) {
    c.manager_lock_for_copy();
    if c.manager_form_visible(FORM_MULTIPLAYER) {
        let event = c.select_server_event_new();
        c.manager_form_set_open_event(FORM_SELECT_SERVER, event);
        c.event_dereference(event);
        let remember = c.manager_form_control(FORM_MULTIPLAYER, MULTIPLAYER_REMEMBER_PASSWORD);
        let edit = c.manager_form_control(FORM_MULTIPLAYER, MULTIPLAYER_PASSWORD);
        if c.check_box_checked(remember) {
            c.won_password_from_edit(edit);
        } else {
            c.won_password_clear();
        }
        c.preferences_save();
        c.multiplayer_string_from_edit(edit);
        c.manager_pop_to(FORM_MULTIPLAYER);
        c.manager_push(FORM_SELECT_SERVER);
    }
    c.manager_unlock_for_copy();
}

/// `EEUserInterface::UpdateInterface`: the keyboard and mouse input, the UI under the world's read lock, then the
/// queued actions.
pub(crate) fn update_interface<C: Cpp + ?Sized>(c: &mut C) {
    c.keyboard_refresh();
    c.mouse_input_lock_enter();
    c.mouse_refresh();
    c.mouse_update();
    c.mouse_input_lock_leave();
    c.world_read_lock();
    if !c.lobby_shown() {
        c.manager_update_ui();
    }
    c.world_read_unlock();
    c.actions_process();
}

/// A message's text from the language strings (the original's `GetString` then `DisplayMessage`).
fn message<C: Cpp + ?Sized>(c: &mut C, id: u32) {
    let s = string(id);
    c.display_message(Text::of(&s), true);
}

/// A new client for the communications (its threads started).
fn new_client<C: Cpp + ?Sized>(c: &mut C) {
    c.new_client();
    c.client_start_threads();
}

/// `ee_screenscpp_ui_game`'s case `op` on the UI object `ui`, `None` for one that is not here.
pub(crate) fn ui_game<C: Cpp + ?Sized>(c: &mut C, ui: usize, op: u32, a: i64, b: i64) -> Option<i64> {
    Some(match op {
        EE_SCREENS_UG_OP_GRAPHICS => {
            if a != 0 {
                pause_graphics(c);
            } else {
                c.ui_resume_graphics(ui);
            }
            0
        }
        EE_SCREENS_UG_OP_WORLD_LOCK => {
            if a != 0 {
                c.world_write_lock();
            } else {
                c.world_write_unlock();
            }
            0
        }
        EE_SCREENS_UG_OP_OPTIONS_LOCK => {
            if a != 0 {
                c.options_write_lock();
            } else {
                c.options_write_unlock();
            }
            0
        }
        EE_SCREENS_UG_OP_CONTINENTS => {
            c.continents_clear();
            c.continents_build_regions();
            0
        }
        EE_SCREENS_UG_OP_PLAYER => {
            let p = a as u32;
            match b {
                EE_SCREENS_UG_PLAYER_HERDS => {
                    c.player_register_herds(p);
                    c.player_assign_herds(p);
                }
                EE_SCREENS_UG_PLAYER_TOWNS => {
                    c.player_register_towns(p);
                    c.player_assign_towns(p);
                }
                EE_SCREENS_UG_PLAYER_CENTRES => c.player_refresh_resource_centres(p),
                EE_SCREENS_UG_PLAYER_TOWN_GROUPS => c.player_refresh_town_groups(p),
                _ => return None,
            }
            0
        }
        EE_SCREENS_UG_OP_MINI_MAP => {
            c.mini_map_redraw();
            c.mini_map_refresh();
            0
        }
        EE_SCREENS_UG_OP_GAME_VIEW => {
            c.set_game_view(0.0, 35.0, 1600.0, 1085.0);
            0
        }
        EE_SCREENS_UG_OP_AMBIENT => {
            c.ambient_enable_from_settings();
            c.ambient_restart();
            0
        }
        EE_SCREENS_UG_OP_SCENARIO_OPTIONS => {
            let mut extension = Vec::new();
            c.scenario_extension(Out::of(&mut extension));
            let name = [&b"ScenDefault"[..], &extension].concat();
            c.world_options_set_load_type(1);
            c.world_options_set_map_type(Text::of(&name));
            0
        }
        EE_SCREENS_UG_OP_LOAD_HEADER => {
            let header = c.header_new();
            c.header_set_major(header);
            c.header_set_minor(header);
            let ok = c.header_load_file(header, a as usize) == 0;
            c.header_delete(header);
            ok as i64
        }
        EE_SCREENS_UG_OP_NEW_SERVER => {
            c.delete_communications();
            c.new_local_server();
            c.server_start_threads();
            0
        }
        EE_SCREENS_UG_OP_SERVER_LOAD => {
            c.server_allow_new_players(false);
            c.server_set_load_responses(false);
            c.server_set_load_state(true);
            c.server_set_game_state(2);
            c.server_set_humans(1);
            c.server_set_multiplayer(false);
            0
        }
        EE_SCREENS_UG_OP_LOAD_OPTIONS => {
            c.options_set_load_type(b as i32);
            c.options_map_type_from_file(a as usize);
            0
        }
        EE_SCREENS_UG_OP_INVALID_SAVE => {
            message(c, STRING_SAVE_GAME_ISNT_VALID);
            0
        }
        EE_SCREENS_UG_OP_LOAD_ERROR => {
            message(c, STRING_THERE_WAS_ERROR_TRYING_TO_LOAD);
            0
        }
        EE_SCREENS_UG_OP_INPUT_RESUME => {
            c.mouse_resume();
            c.keyboard_resume();
            0
        }
        EE_SCREENS_UG_OP_LOCALE => {
            // port (decision D6): game text is UTF-8, so the C runtime's character classes stay the "C" locale's
            let locale = string(STRING_ENGLISH);
            c.set_locale_all(Text::of(&locale));
            c.set_locale_numeric_english();
            c.set_locale_ctype_c();
            c.query_locale();
            0
        }
        EE_SCREENS_UG_OP_CREATE_RENDER_OBJECT => {
            // the UI's render object to the graphics thread
            c.ui_render_object_new();
            c.gfx_push_ui_render_object();
            0
        }
        EE_SCREENS_UG_OP_POINTER_POST => {
            if a == POINTER_SURFACE {
                c.pointer_post_surface(a as u32);
            } else {
                c.pointer_post(a as u32);
            }
            0
        }
        EE_SCREENS_UG_OP_GRAPHICS_START => {
            c.gfx_set_flag54(false);
            c.gfx_post_resume();
            0
        }
        EE_SCREENS_UG_OP_FPU => {
            if a == 0 {
                c.fpu_set_precision_53();
                return Some(0);
            }
            if !c.fpu_precision_is_53() {
                c.fpu_set_precision_53();
            }
            if c.fpu_rounding_mode() != 0 {
                c.fpu_set_rounding_mode(0);
            }
            0
        }
        EE_SCREENS_UG_OP_CAMERA => {
            c.camera_update(f32::from_bits(a as u32));
            0
        }
        EE_SCREENS_UG_OP_RELEASE_SCENE => {
            c.gfx_release_scene();
            c.gfx_wait_50a();
            0
        }
        EE_SCREENS_UG_OP_PHYSICS_INIT => {
            if a == 0 {
                c.physics_post_init();
            } else {
                c.physics_run_owed_ticks();
                c.physics_init();
            }
            0
        }
        EE_SCREENS_UG_OP_NEW_CLIENT => {
            if c.communications() != 0 {
                c.delete_communications();
                new_client(c);
            }
            0
        }
        EE_SCREENS_UG_OP_BOOT_CLIENT => {
            let action = c.boot_client_new(a as i32);
            c.post_action(action);
            c.action_dereference(action);
            0
        }
        EE_SCREENS_UG_OP_EDITOR_PAUSES => {
            let server = c.server_dynamic();
            c.server_pause(server);
            c.music_pause();
            c.ambient_stop_sounds();
            c.ambient_pause();
            c.chat_log_clear();
            0
        }
        EE_SCREENS_UG_OP_CB_MUSIC => {
            // (the UI thread's: the menus' music kept across the builder's run)
            let mut music = BUILDER_MUSIC.lock().unwrap_or_else(|e| e.into_inner());
            c.manager_form(FORM_CIV_BUILDER);
            if a == 0 {
                music.clear();
                c.form_music(FORM_CIV_BUILDER, Out::of(&mut music));
            } else if a == 1 {
                c.form_set_music(FORM_CIV_BUILDER, Text::of(b""));
            } else {
                c.form_set_music(FORM_CIV_BUILDER, Text::of(&music));
            }
            0
        }
        EE_SCREENS_UG_OP_CIV_CHOOSE => {
            c.manager_form(FORM_PREDEFINED_CIV);
            c.civ_builder_button_blinking(false);
            c.player_civilization_chosen();
            c.player_civilization_locked();
            let event = c.screen_event_new(EE_SCREENS_PDCS_OPEN);
            c.manager_form_set_open_event(FORM_PREDEFINED_CIV, event);
            c.event_dereference(event);
            let event = c.screen_event_new(EE_SCREENS_PDCS_CLOSE);
            c.manager_form_set_close_event(FORM_PREDEFINED_CIV, event);
            c.event_dereference(event);
            c.manager_push(FORM_PREDEFINED_CIV);
            0
        }
        _ => return None,
    })
}

/// `ee_screenscpp_ui_message`'s case `op`, `None` for one that is not here.
pub(crate) fn ui_message<C: Cpp + ?Sized>(c: &mut C, op: u32, a: i64, b: i64) -> Option<i64> {
    Some(match op {
        // (the copy lock, 0, is hud_views.rs')
        EE_SCREENS_UI_OP_LOCK => {
            if a == 1 {
                c.world_read_lock();
            } else {
                c.world_write_lock();
            }
            0
        }
        EE_SCREENS_UI_OP_UNLOCK => {
            if a == 1 {
                c.world_read_unlock();
            } else {
                c.world_write_unlock();
            }
            0
        }
        EE_SCREENS_UI_OP_PROCESS_EVENT => {
            c.event_process(a as usize, b as usize);
            c.event_dereference(a as usize);
            0
        }
        EE_SCREENS_UI_OP_QUICK_LOAD => {
            c.quick_save_set_load(a as usize);
            c.post_action(a as usize);
            c.action_dereference(a as usize);
            0
        }
        EE_SCREENS_UI_OP_DIALOG_LOG => {
            let entry = c.chat_entry_new();
            c.chat_entry_text_from_dialog(entry, a as usize);
            c.chat_entry_set_type(entry, 1);
            c.chat_log_add(entry);
            c.chat_entry_delete(entry);
            0
        }
        EE_SCREENS_UI_OP_HOST_GONE => {
            c.set_quit_flag();
            let action = c.boot_client_host_gone_new();
            c.post_action(action);
            c.action_dereference(action);
            c.client_disconnect();
            if c.communications() != 0 {
                c.delete_communications();
            }
            new_client(c);
            0
        }
        _ => return None,
    })
}

/// `ee_screenscpp_ui_event`'s case `op`, `None` for one that is not here.
pub(crate) fn ui_event<C: Cpp + ?Sized>(c: &mut C, op: u32, a: i64, b: i64, cc: i64) -> Option<i64> {
    Some(match op {
        EE_SCREENS_UI_EV_MUSIC_VOLUME => {
            let v = if c.conductor() != 0 { c.conductor_volume() } else { 0.0 };
            v.to_bits() as i64
        }
        EE_SCREENS_UI_EV_MASTER_VOLUME => {
            let v = if c.sound_manager() != 0 { c.master_volume() } else { 0.0 };
            v.to_bits() as i64
        }
        EE_SCREENS_UI_EV_KEYBOARD => {
            if a != 0 {
                c.keyboard_resume();
            } else {
                c.keyboard_suspend();
            }
            0
        }
        EE_SCREENS_UI_EV_MOUSE => {
            if a != 0 {
                c.mouse_resume();
            } else {
                c.mouse_suspend();
            }
            0
        }
        EE_SCREENS_UI_EV_IME_UPDATE => {
            // the input method's composition (its text at a), the edited part b..b + cc as the target clause, the
            // rest as input (IMM's ATTR_TARGET_CONVERTED 1 / ATTR_CONVERTED 2), and the cursor
            // SAFETY: the event's text, a NUL-terminated string for the call
            let text = unsafe { ee_ui::direct::c_bytes(a as *const core::ffi::c_void) };
            let attributes = ime_attributes(text.len(), b, cc);
            c.ime_composition(Text::of(&text), Text::of(&attributes), b.max(0) as u32);
            0
        }
        _ => return None,
    })
}

/// The composition's attributes: 1 for the characters in `start..start + length`, else 2.
pub(crate) fn ime_attributes(len: usize, start: i64, length: i64) -> Vec<u8> {
    (0..len as i64).map(|i| if i >= start && i < start + length { 1 } else { 2 }).collect()
}

/// `ee_screenscpp_action`'s case `op`, `None` for one that is not here.
pub(crate) fn action<C: Cpp + ?Sized>(c: &mut C, op: u32) -> Option<u32> {
    Some(match op {
        EE_SCREENS_ACTION_LOCK => {
            c.manager_lock_for_copy();
            c.world_write_lock();
            0
        }
        EE_SCREENS_ACTION_UNLOCK => {
            c.world_write_unlock();
            c.manager_unlock_for_copy();
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "ui_calls_tests.rs"]
mod tests;
