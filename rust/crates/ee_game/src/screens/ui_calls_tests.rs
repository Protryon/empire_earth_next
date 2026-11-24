//! ui_calls.rs over the recording fake: each moved member's and case's C++ calls, in the C++ order.

use super::*;
use crate::screens::game_cpp::Fake;
use crate::screens::ui_thread::EE_SCREENS_UI_EV_MUSIC_VOLUME;

#[test]
fn pause_graphics_waits() {
    let mut f = Fake::default();
    f.answer("gfx_paused", &[0, 0, 1]);
    pause_graphics(&mut f);
    assert_eq!(f.calls(), "gfx_post_pause()\ngfx_paused()\nsleep(1)\ngfx_paused()\nsleep(1)\ngfx_paused()");
}

#[test]
fn resizes_the_game_screen() {
    // unchanged and not forced: nothing but the reads
    let mut f = Fake::default();
    f.answer("game_window_width", &[800]).answer("game_window_height", &[600]);
    resize_game_screen(&mut f, 800, 600, false, false);
    assert_eq!(f.calls(), "pointer_shown()\ngame_window_width()\ngame_window_height()");
    let mut f = Fake::default();
    f.answer("pointer_shown", &[1, 1, 0, 0]).answer("mini_map", &[5]);
    resize_game_screen(&mut f, 1024, 768, true, true);
    assert_eq!(
        f.calls(),
        "pointer_shown()\ngame_window_width()\npointer_post(1026)\npointer_shown()\nsleep(1)\npointer_shown()\n\
         pointer_shown()\nchange_game_screen(1024, 768, true)\nmanager_relayout_all()\npointer_post(1028)\nmini_map()\n\
         world_write_lock()\nmini_map_redraw_all()\nworld_write_unlock()"
    );
}

#[test]
fn changes_the_rasterizer() {
    let mut f = Fake::default();
    f.answer("rasterizer_screen_count", &[2, 2, 2]).answer("rasterizer_deactivate", &[0]);
    f.answer("menu_width", &[1024]).answer("menu_height", &[768]);
    change_rasterizer(&mut f, 1, 0);
    let calls = f.calls();
    assert!(calls.starts_with(
        "game_window_client_origin()\npointer_post(1026)\npointer_shown()\nrasterizer_screen_count()\n\
         rasterizer_release_screen(0)\nrasterizer_screen_count()\nrasterizer_release_screen(1)\nrasterizer_screen_count()\n\
         delete_effect_batch_buffers()\nrasterizer_deactivate()\nverify_rasterizer_deactivated(0)\nrasterizer_select(1)\n\
         gfx_post_set_rasterizer(0)\nmanager_change_rasterizer()\nmenu_width()\nmenu_height()\npointer_shown()"
    ));
    assert!(calls.ends_with("change_game_screen(1024, 768, false)\nmanager_relayout_all()\nmini_map()"));
}

#[test]
fn the_motd_and_select_server() {
    let mut f = Fake::default();
    f.answer("manager_form_visible", &[1]).answer("manager_form_control", &[9]);
    show_message_of_the_day(&mut f, 100, 200);
    assert_eq!(
        f.calls(),
        "manager_lock_for_copy()\nmanager_form_visible(30)\nmanager_form_control(30, 29)\ncontrol_set_display_mode(9, 1)\n\
         label_delete_array(9)\nlabel_append_wide(9, 100)\nlabel_append_wide(9, 200)\nmanager_unlock_for_copy()"
    );
    let mut f = Fake::default();
    show_message_of_the_day(&mut f, 100, 200);
    assert_eq!(f.calls(), "manager_lock_for_copy()\nmanager_form_visible(30)\nmanager_unlock_for_copy()");
    let mut f = Fake::default();
    f.answer("manager_form_visible", &[1]).answer("select_server_event_new", &[4]);
    f.answer("manager_form_control", &[10, 11]).answer("check_box_checked", &[1]);
    open_select_server(&mut f);
    assert_eq!(
        f.calls(),
        "manager_lock_for_copy()\nmanager_form_visible(30)\nselect_server_event_new()\nmanager_form_set_open_event(38, 4)\n\
         event_dereference(4)\nmanager_form_control(30, 28)\nmanager_form_control(30, 22)\ncheck_box_checked(10)\n\
         won_password_from_edit(11)\npreferences_save()\nmultiplayer_string_from_edit(11)\nmanager_pop_to(30)\n\
         manager_push(38)\nmanager_unlock_for_copy()"
    );
}

#[test]
fn updates_the_interface() {
    let mut f = Fake::default();
    update_interface(&mut f);
    assert_eq!(
        f.calls(),
        "keyboard_refresh()\nmouse_input_lock_enter()\nmouse_refresh()\nmouse_update()\nmouse_input_lock_leave()\n\
         world_read_lock()\nlobby_shown()\nmanager_update_ui()\nworld_read_unlock()\nactions_process()"
    );
    let mut f = Fake::default();
    f.answer("lobby_shown", &[1]);
    update_interface(&mut f);
    assert!(!f.calls().contains("manager_update_ui"));
}

#[test]
fn the_game_starts() {
    let mut f = Fake::default();
    assert_eq!(ui_game(&mut f, 1, 0xffff_ffff, 0, 0), None);
    let mut f = Fake::default();
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_PLAYER, 2, EE_SCREENS_UG_PLAYER_HERDS);
    assert_eq!(f.calls(), "player_register_herds(2)\nplayer_assign_herds(2)");
    let mut f = Fake::default();
    assert_eq!(ui_game(&mut f, 1, EE_SCREENS_UG_OP_PLAYER, 2, EE_SCREENS_UG_PLAYER_EXISTS), None);
    let mut f = Fake::default();
    f.text("scenario_extension", b".scn");
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_SCENARIO_OPTIONS, 0, 0);
    assert_eq!(
        f.calls(),
        "scenario_extension(out)\nworld_options_set_load_type(1)\nworld_options_set_map_type(\"ScenDefault.scn\")"
    );
    let mut f = Fake::default();
    f.answer("header_new", &[3]);
    assert_eq!(ui_game(&mut f, 1, EE_SCREENS_UG_OP_LOAD_HEADER, 9, 0), Some(1));
    assert_eq!(
        f.calls(),
        "header_new()\nheader_set_major(3)\nheader_set_minor(3)\nheader_load_file(3, 9)\nheader_delete(3)"
    );
    let mut f = Fake::default();
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_FPU, 1, 0);
    assert_eq!(f.calls(), "fpu_precision_is_53()\nfpu_set_precision_53()\nfpu_rounding_mode()");
    let mut f = Fake::default();
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_FPU, 0, 0);
    assert_eq!(f.calls(), "fpu_set_precision_53()");
    let mut f = Fake::default();
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_POINTER_POST, 0x405, 0);
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_POINTER_POST, 0x404, 0);
    assert_eq!(f.calls(), "pointer_post_surface(1029)\npointer_post(1028)");
    let mut f = Fake::default();
    f.answer("communications", &[1]);
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_NEW_CLIENT, 0, 0);
    assert_eq!(f.calls(), "communications()\ndelete_communications()\nnew_client()\nclient_start_threads()");
    let mut f = Fake::default();
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_NEW_CLIENT, 0, 0);
    assert_eq!(f.calls(), "communications()");
    let mut f = Fake::default();
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_SERVER_LOAD, 0, 0);
    assert_eq!(
        f.calls(),
        "server_allow_new_players(false)\nserver_set_load_responses(false)\nserver_set_load_state(true)\n\
         server_set_game_state(2)\nserver_set_humans(1)\nserver_set_multiplayer(false)"
    );
    let mut f = Fake::default();
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_CAMERA, 0.5f32.to_bits() as i64, 0);
    assert_eq!(f.calls(), "camera_update(0.5)");
    let mut f = Fake::default();
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_GRAPHICS, 0, 0);
    assert_eq!(f.calls(), "ui_resume_graphics(1)");
}

#[test]
fn the_builder_music_and_choice() {
    let mut f = Fake::default();
    f.text("form_music", b"menu.mp3");
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_CB_MUSIC, 0, 0);
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_CB_MUSIC, 1, 0);
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_CB_MUSIC, 2, 0);
    assert_eq!(
        f.calls(),
        "manager_form(7)\nform_music(7, out)\nmanager_form(7)\nform_set_music(7, \"\")\nmanager_form(7)\n\
         form_set_music(7, \"menu.mp3\")"
    );
    let mut f = Fake::default();
    f.answer("screen_event_new", &[5, 6]);
    ui_game(&mut f, 1, EE_SCREENS_UG_OP_CIV_CHOOSE, 0, 0);
    let calls = f.calls();
    assert!(calls.starts_with(
        "manager_form(35)\nciv_builder_button_blinking(false)\nplayer_civilization_chosen()\nplayer_civilization_locked()\n"
    ));
    assert!(
        calls.ends_with(
            "manager_form_set_open_event(35, 5)\nevent_dereference(5)\nscreen_event_new(6915)\n\
         manager_form_set_close_event(35, 6)\nevent_dereference(6)\nmanager_push(35)"
        ) || calls.contains("manager_form_set_close_event(35, 6)\nevent_dereference(6)\nmanager_push(35)")
    );
}

#[test]
fn the_messages_events_and_actions() {
    let mut f = Fake::default();
    ui_message(&mut f, EE_SCREENS_UI_OP_LOCK, 1, 0);
    ui_message(&mut f, EE_SCREENS_UI_OP_LOCK, 2, 0);
    ui_message(&mut f, EE_SCREENS_UI_OP_UNLOCK, 1, 0);
    assert_eq!(f.calls(), "world_read_lock()\nworld_write_lock()\nworld_read_unlock()");
    let mut f = Fake::default();
    ui_message(&mut f, EE_SCREENS_UI_OP_HOST_GONE, 0, 0);
    assert_eq!(f.calls(), "set_quit_flag()\nboot_client_host_gone_new()\npost_action(0)\naction_dereference(0)\nclient_disconnect()\ncommunications()\nnew_client()\nclient_start_threads()");
    let mut f = Fake::default();
    f.answer("chat_entry_new", &[3]);
    ui_message(&mut f, EE_SCREENS_UI_OP_DIALOG_LOG, 8, 0);
    assert_eq!(
        f.calls(),
        "chat_entry_new()\nchat_entry_text_from_dialog(3, 8)\nchat_entry_set_type(3, 1)\nchat_log_add(3)\nchat_entry_delete(3)"
    );
    let mut f = Fake::default();
    assert_eq!(ui_event(&mut f, EE_SCREENS_UI_EV_MUSIC_VOLUME, 0, 0, 0), Some(0));
    assert_eq!(f.calls(), "conductor()");
    let mut f = Fake::default();
    f.answer("conductor", &[1]).answer("conductor_volume", &[1]);
    assert_eq!(ui_event(&mut f, EE_SCREENS_UI_EV_MUSIC_VOLUME, 0, 0, 0), Some(1.0f32.to_bits() as i64));
    assert_eq!(ime_attributes(5, 1, 2), [2, 1, 1, 2, 2]);
    assert_eq!(ime_attributes(3, -1, 0), [2, 2, 2]);
    let mut f = Fake::default();
    action(&mut f, EE_SCREENS_ACTION_LOCK);
    action(&mut f, EE_SCREENS_ACTION_UNLOCK);
    assert_eq!(
        f.calls(),
        "manager_lock_for_copy()\nworld_write_lock()\nworld_write_unlock()\nmanager_unlock_for_copy()"
    );
    assert_eq!(action(&mut Fake::default(), 0xffff), None);
}

#[test]
fn window_events_one_call_ops() {
    use crate::screens::ui_thread::{
        EE_SCREENS_UI_EV_HAS, EE_SCREENS_UI_EV_SET_MASTER_VOLUME, EE_SCREENS_UI_GLOBAL_CONDUCTOR,
        EE_SCREENS_UI_GLOBAL_WINDOW,
    };
    let mut f = Fake::default();
    f.answer("conductor", &[5]).answer("ui_has_window", &[1]);
    assert_eq!(ui_event(&mut f, EE_SCREENS_UI_EV_HAS, EE_SCREENS_UI_GLOBAL_CONDUCTOR, 0, 0), Some(1));
    assert_eq!(ui_event(&mut f, EE_SCREENS_UI_EV_HAS, EE_SCREENS_UI_GLOBAL_WINDOW, 0, 0), Some(1));
    let bits = 0.5f32.to_bits() as i64;
    assert_eq!(ui_event(&mut f, EE_SCREENS_UI_EV_SET_MASTER_VOLUME, bits, 0, 0), Some(0));
    assert_eq!(f.calls(), "conductor()\nui_has_window()\nui_set_master_volume(0.5)");
}
