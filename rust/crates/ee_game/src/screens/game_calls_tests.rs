//! game_calls.rs over the recording fake: each moved case's C++ calls, in the C++ case's order.

use super::*;
use crate::screens::game_cpp::Fake;

const FORM: usize = 0x100;

fn run(fake: &mut Fake, op: u32, a: u32, b: u32, c: u32) -> Option<u32> {
    call(fake, FORM, op, a, b, c, 0)
}

#[test]
fn an_op_that_is_not_here_goes_on() {
    let mut f = Fake::default();
    assert_eq!(run(&mut f, 0xdead_beef, 0, 0, 0), None);
    assert!(f.log.is_empty());
}

#[test]
fn the_slider_label_event() {
    let mut f = Fake::default();
    f.answer("slider_event_new", &[0x55]);
    assert_eq!(run(&mut f, EE_SCREENS_OP_SLIDER_LABEL, 7, 8, 0), Some(0));
    assert_eq!(f.calls(), "slider_event_new(256, 8)\ncontrol_set_range_trigger(256, 7, 85)\nevent_dereference(85)");
    // with a string id: the (missing, so empty) string added at 0.0 before the event is dropped
    let mut f = Fake::default();
    f.answer("slider_event_new", &[0x55]);
    run(&mut f, EE_SCREENS_OP_SLIDER_LABEL, 7, 8, 9);
    assert_eq!(f.log[2], "slider_event_add_string(85, \"\")");
    assert_eq!(f.log[3], "event_dereference(85)");
}

#[test]
fn the_window_mode() {
    // no window: nothing
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_GAME_WINDOW_MODE, 0, 0, 0);
    assert_eq!(f.calls(), "game_window()");
    // full screen
    let mut f = Fake::default();
    f.answer("game_window", &[8]).answer("game_window_os", &[9]);
    run(&mut f, EE_SCREENS_GAME_WINDOW_MODE, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "game_window()\ngame_window_os(8)\nsettings_window_mode()\nwindow_set_fixed_size(9, 0)\nwindow_set_fullscreen(9, 1)"
    );
    // windowed: four fifths of the display, then fixed
    let mut f = Fake::default();
    f.answer("game_window", &[8])
        .answer("game_window_os", &[9])
        .answer("settings_window_mode", &[1])
        .answer("display_logical_size", &[1]);
    f.out("display_logical_size", 1920).out("display_logical_size", 1080);
    run(&mut f, EE_SCREENS_GAME_WINDOW_MODE, 0, 0, 0);
    assert_eq!(
        f.log[3..],
        [
            "window_set_fullscreen(9, 0)",
            "window_set_fixed_size(9, 0)",
            "display_logical_size(out, out)",
            "window_set_size(9, 1536, 864)",
            "window_set_fixed_size(9, 1)"
        ]
    );
}

#[test]
fn the_sounds() {
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_GAME_TEST_SOUND, 3, 0, 0);
    assert_eq!(f.calls(), "sound_object(3)");
    let mut f = Fake::default();
    f.answer("sound_object", &[40]);
    run(&mut f, EE_SCREENS_GAME_TEST_SOUND, 3, 0, 0);
    assert_eq!(f.calls(), "sound_object(3)\nsound_load_media(40)\nsound_play(40)");
    let mut f = Fake::default();
    f.answer("sound_hot_key_alarm", &[41, 41]);
    run(&mut f, crate::screens::hot_keys::EE_SCREENS_GAME_HK_ALARM, 0, 0, 0);
    assert_eq!(f.calls(), "sound_hot_key_alarm()\nsound_hot_key_alarm()\nsound_play(41)");
}

#[test]
fn the_folders() {
    assert!(names_folder(b"Sounds\\intro.mp3", b"sounds\\"));
    assert!(names_folder(b"x\\SOUNDS\\intro", b"sounds\\"));
    assert!(!names_folder(b"intro", b"sounds\\"));
    assert_eq!(in_folder(b"intro".to_vec(), b"Sounds\\"), b"Sounds\\intro");
    assert_eq!(in_folder(b"sounds\\intro".to_vec(), b"Sounds\\"), b"sounds\\intro");
}

#[test]
fn the_briefing_sound() {
    let mut f = Fake::default();
    f.answer("spec_new", &[70]).text("spec_file", b"brief");
    f.out("load_sound", 12).out("load_sound", 77);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_SI_PLAY_SOUND, 5, 0, 0), Some(77));
    assert_eq!(
        f.calls(),
        "spec_new()\nintro_get_sound(70)\nspec_file(70, out)\nspec_delete(70)\nsound_set_latency(500)\n\
         load_sound(\"Sounds\\\\brief\", out, out)\nsound_register_one_shot(12, 77)"
    );
}

#[test]
fn the_scenario_backdrop() {
    // the same picture: kept
    let mut f = Fake::default();
    f.answer("form_background", &[30]).answer("spec_new", &[70]).text("spec_file", b"map1");
    f.text("background_texture_file", b"Scenarios\\map1");
    run(&mut f, EE_SCREENS_GAME_SI_MAP_BACKDROP, 0, 0, 0);
    assert_eq!(f.log.last().unwrap(), "background_texture_file(30, out)");
    assert_eq!(f.log[0], "form_background(256, 4294967294)");
    // another one: removed, the new one made when the map opens
    let mut f = Fake::default();
    f.answer("form_background", &[30]).answer("spec_new", &[70]).text("spec_file", b"map2");
    f.text("background_texture_file", b"Scenarios\\map1").answer("intro_open_map", &[1]);
    f.answer("map_background_new", &[31]);
    run(&mut f, EE_SCREENS_GAME_SI_MAP_BACKDROP, 0, 0, 0);
    let tail: Vec<&str> = f.log[6..].iter().map(|s| s.as_str()).collect();
    assert_eq!(tail[0], "form_remove_background(256, 30)");
    assert_eq!(tail[1], "intro_open_map()");
    assert_eq!(tail[2], "form_lock_background(256)");
    assert!(tail[3].starts_with("map_background_new(4294967294, \"Scenarios\\\\map2\", "), "{}", tail[3]);
    assert!(tail[3].ends_with(", 256, 2)"));
    assert_eq!(tail[4..], ["background_open(31)", "form_unlock_background(256)"]);
}

#[test]
fn the_backdrop_description() {
    let d = backdrop_description();
    assert_eq!((d.pixel_depth, d.surface_volatility, d.frame_format, d.num_mip_levels_to_drop), (16, 1, 0, 0));
    assert!(d.has_alpha && d.create_mipmaps && d.use_texture_pack && !d.is_3d_renderable);
}

#[test]
fn the_scenario_start() {
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_GAME_SI_START_GAME, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "options_write_lock()\noptions_set_defaults()\noptions_set_load_type(1)\nload_scenario_name_from_intro()\n\
         options_map_type_from_intro()\noptions_set_multiplayer(false)\noptions_write_unlock()\n\
         delete_communications()\nnew_local_server()\nserver_start_threads()\nserver_set_load_responses(false)\n\
         server_stop_ping()\nserver_set_load_state(true)\nset_local_player_out(false)\nsend_scenario_start_loading()"
    );
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_GAME_SI_START_GAME, 1, 0, 0);
    assert_eq!(
        f.log[2..8],
        [
            "options_set_load_type(4)",
            "options_campaign_difficulty()",
            "options_campaign_name()",
            "options_campaign_checksum()",
            "options_campaign_scenario()",
            "options_campaign_first_map()"
        ]
    );
}

#[test]
fn the_campaign_backdrop() {
    let mut f = Fake::default();
    f.text("campaign_background_image", b"bg.jpg").text("scenario_prefix", b"P\\");
    f.answer("data_find_file", &[1]);
    run(&mut f, EE_SCREENS_GAME_CS_MAP_BACKDROP, 0, 0, 0);
    assert_eq!(f.log[2], "data_find_file(\"Scenarios\\\\P\\\\bg.jpg\", out)");
    assert_eq!(f.log.len(), 3);
    let mut f = Fake::default();
    f.text("campaign_background_image", b"bg.jpg").out("data_find_file", 66).answer("map_background_new", &[31]);
    run(&mut f, EE_SCREENS_GAME_CS_MAP_BACKDROP, 0, 0, 0);
    assert_eq!(f.log[3], "form_lock_background(256)");
    assert_eq!(f.log[5..], ["background_open(31)", "form_unlock_background(256)", "spec_delete(66)"]);
}

#[test]
fn the_lobby() {
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_OP_LE_NAME_COLOUR, 4, 5, 1);
    assert_eq!(f.calls(), "combo_text_colour(256, 4, 1.0, 1.0, 0.0)");
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_OP_LE_NAME_COLOUR, 4, 3, 0);
    assert_eq!(f.calls(), "label_text_colour(256, 4, 0.0, 1.0, 0.0)");
    // WON's rejoin: closes unless the room says no
    let mut f = Fake::default();
    assert_eq!(run(&mut f, EE_SCREENS_GAME_LE_WON_REJOIN, 0, 0, 0), Some(1));
    let mut f = Fake::default();
    f.answer("won_net", &[1]).answer("won_rejoin_room", &[0]);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_LE_WON_REJOIN, 0, 0, 0), Some(0));
    assert_eq!(f.calls(), "won_net()\nwon_clear_game_list()\nwon_rejoin_room(true)");
    // the generator's line, bit 31 for none
    let mut f = Fake::default();
    f.out("find_generator", 0x8000_0005u32 as i64);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_LE_FIND_GENERATOR, 2, 0, 0), Some(5));
    let mut f = Fake::default();
    f.answer("find_generator", &[3]).out("find_generator", 6);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_LE_FIND_GENERATOR, 2, 0, 0), Some(0x8000_0006));
}

#[test]
fn the_lobby_start_and_boot() {
    let mut f = Fake::default();
    f.answer("won_net", &[1]).answer("won_info_flags", &[7]);
    run(&mut f, EE_SCREENS_GAME_LE_START_SERVER, 3, 5, 0);
    assert_eq!(
        f.calls(),
        "won_info_reset()\nlobby_start_time_now()\nserver_allow_new_players(false)\nserver_set_game_state(2)\n\
         server_set_load_responses(false)\nwon_net()\nwon_info_load()\nwon_info_flags()\nwon_info_set_flags(6)\n\
         won_info_set_players(5)\nwon_info_set_humans(3)\nwon_info_store()\nserver_stop_ping()\nserver_reset_xfer()\n\
         server_set_load_state(true)\nserver_allow_new_players(false)"
    );
    // a computer slot's boot (4): both counts down (wrapping, as the unsigned chars)
    let mut f = Fake::default();
    f.answer("server_dynamic", &[50]).answer("won_info_humans", &[0]).answer("won_info_players", &[2]);
    f.answer("lobby_boot", &[60]);
    run(&mut f, EE_SCREENS_GAME_LE_BOOT_REFUSE, 4, 0, 0);
    assert_eq!(
        f.calls(),
        "server_dynamic()\nwon_info_reset()\nwon_net()\nwon_info_humans()\nwon_info_set_humans(255)\n\
         won_info_players()\nwon_info_set_players(1)\nlobby_boot()\nserver_refuse_boot(50)\nwon_net()"
    );
}

#[test]
fn the_lobby_transfer() {
    let mut f = Fake::default();
    assert_eq!(run(&mut f, EE_SCREENS_GAME_LE_XFER, 2, 9, 0), Some(0));
    assert_eq!(f.calls(), "lobby_has_file()");
    let mut f = Fake::default();
    f.answer("lobby_has_file", &[1]).out("server_xfer_lobby_file", 44);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_LE_XFER, 2, 9, 0), Some(44));
    assert_eq!(f.calls(), "lobby_has_file()\nserver_abort_xfer(9)\nserver_xfer_lobby_file(2, out)");
}

#[test]
fn the_options_menu() {
    let mut f = Fake::default();
    f.text("form_music", b"menu.mp3").answer("server_dynamic", &[50]);
    run(&mut f, EE_SCREENS_GAME_GO_TO_SCENARIO_LOAD, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "form_music(20, out)\nform_set_music(20, \"\")\nmanager_pop()\nmanager_swap(20)\n\
         form_set_music(20, \"menu.mp3\")\nserver_dynamic()\nserver_pause(50)\npost_pause_information(true)\n\
         intro_set_flag(0, false)\nintro_set_flag(1, true)\nsave_main_game_form()"
    );
    let mut f = Fake::default();
    f.answer("user_pause_new", &[8]);
    run(&mut f, EE_SCREENS_GAME_GO_USER_PAUSE, 0, 0, 0);
    assert_eq!(f.calls(), "user_pause_new()\npost_action(8)");
    let mut f = Fake::default();
    f.answer("user_pause_new", &[8]);
    run(&mut f, EE_SCREENS_GAME_GO_USER_PAUSE, 1, 0, 0);
    assert_eq!(f.calls(), "user_pause_new()\npost_action(8)\naction_dereference(8)");
}

#[test]
fn the_small_ones() {
    let mut f = Fake::default();
    f.answer("server_dynamic", &[50, 50]).answer("server_paused", &[1]);
    run(&mut f, EE_SCREENS_GAME_PAUSE_SERVER, 0, 0, 0);
    assert_eq!(f.calls(), "server_dynamic()\nserver_paused(50)");
    run(&mut f, EE_SCREENS_GAME_PAUSE_SERVER, 0, 0, 0);
    assert_eq!(f.log[4..], ["server_pause(50)", "post_pause_information(true)"]);
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_GAME_SI_SET_FLAG, 4, 1, 0);
    assert_eq!(f.calls(), "intro_set_flag(2, true)");
    let mut f = Fake::default();
    f.answer("civ_builder_points", &[600]);
    run(&mut f, EE_SCREENS_GAME_CIV_BUILDER, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "civ_clear_upgrades()\nciv_reset_points()\nciv_builder_points()\nciv_set_points_and_max(600)\n\
         open_civ_builder()"
    );
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_GAME_CS_SET_DIFFICULTY, 2, 0, 0);
    assert_eq!(
        f.calls(),
        "campaign_set_difficulty(2)\nset_difficulty_level(2)\nset_difficulty_level2(2)\noptions_write_lock()\n\
         options_set_difficulty(2)\noptions_write_unlock()"
    );
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_GAME_LE_LOCK, EE_SCREENS_LE_WRITE_UNLOCK, 0, 0);
    assert_eq!(f.calls(), "options_write_unlock()");
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_GAME_VIEW, 0, 0, 0);
    assert_eq!(f.calls(), "set_game_view(0.0, 35.0, 1600.0, 1085.0)");
}
