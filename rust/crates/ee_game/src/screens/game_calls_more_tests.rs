//! game_calls_more.rs over the recording fake: each moved case's C++ calls, in the C++ case's order.

use super::*;
use crate::screens::game_cpp::Fake;

const FORM: usize = 0x100;

fn run(f: &mut Fake, op: u32, a: u32, b: u32, c: u32, d: u32) -> Option<u32> {
    call(f, FORM, op, a, b, c, d)
}

#[test]
fn not_here() {
    let mut f = Fake::default();
    assert_eq!(run(&mut f, 0xdead_beef, 0, 0, 0, 0), None);
}

#[test]
fn the_hot_keys_and_chat() {
    let mut f = Fake::default();
    f.answer("hot_key_unit_x", &[0]).answer("hot_key_unit_y", &[2]).answer("hot_key_unit_z", &[3]);
    run(&mut f, EE_SCREENS_GAME_HK_CENTER, 4, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "hot_key_unit_x(4)\nhot_key_unit_y(4)\nhot_key_unit_z(4)\ncamera_set_changed()\n\
         camera_set_location(0.0, 2.0, 3.0, false, true)\nmini_map_refresh()\nset_camera_following(false)"
    );
    let mut f = Fake::default();
    f.answer("hot_key_new", &[7]).answer("screen_event_new", &[8]);
    run(&mut f, EE_SCREENS_OP_CH_SEND_KEYS, 0x33, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "hot_key_new()\nscreen_event_new(51)\nhot_key_set_key(7, 28)\nhot_key_set_just_pressed(7, 8)\n\
         chat_set_enter_trigger(256, 8)\nevent_dereference(8)\nform_add_hot_key(256, 7)\nhot_key_delete(7)"
    );
}

#[test]
fn the_main_game() {
    let mut f = Fake::default();
    f.answer("spec_new", &[9]).answer("spec_fopen_append", &[0]);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_MG_DUMP_MEDIA, 1, 1, 0, 0), Some(0));
    assert_eq!(
        f.calls(),
        "spec_new()\nspec_set_volume_startup(9)\nspec_set_directory_startup(9, \"\")\n\
         spec_set_file(9, \"Media Dump.txt\")\nspec_delete_file(9)\nspec_create(9)\nspec_fopen_append(9)\n\
         add_chat_log(\"Dumped Media Information.\", 0)\nworld_dump_media(9, true)\nspec_close(9)\nspec_delete(9)"
    );
    let mut f = Fake::default();
    f.answer("spec_fopen_append", &[5]);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_MG_DUMP_MEDIA, 0, 0, 0, 0), Some(5));
    assert!(f.calls().ends_with("spec_fopen_append(0)\nspec_delete(0)"));
    let mut f = Fake::default();
    f.answer("post_enter_scenario_editor", &[6]);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_MG_TO_EDITOR, 0, 0, 0, 0), Some(6));
    assert_eq!(
        f.calls(),
        "protocol_clear()\nghosts_lock_enter()\nworld_clear_ghosts()\nghosts_lock_leave()\nprotocol_load()\n\
         world_move_player_to_editor()\nphysics_post(1026)\npost_enter_scenario_editor()\npost_pause_information(true)"
    );
    let mut f = Fake::default();
    f.text("saved_game_extension", b".sav");
    run(&mut f, EE_SCREENS_GAME_MG_QUICK_SAVE, 0, 0, 0, 0);
    assert_eq!(
        f.log[2..],
        [
            "settings_set_last_quick_save(\"QuickSave    0.01.00 - 00h.00m.00s.sav\")",
            "preferences_save()",
            "send_save_to_all(\"QuickSave    0.01.00 - 00h.00m.00s.sav\")"
        ]
    );
    let mut f = Fake::default();
    f.answer("current_player", &[3]).answer("lead_unit_id", &[44]).answer("world_unit", &[55]);
    run(&mut f, EE_SCREENS_GAME_MG_FOLLOW_LEAD, 0, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "current_player()\nlead_unit_id(3)\nworld_unit(44)\ncamera_follow(55, false)\ncamera_set_changed()"
    );
    let mut f = Fake::default();
    f.answer("main_group_selected_unit", &[5]).answer("unit_has_complex", &[1]).answer("unit_has_class", &[1]);
    f.answer("unit_class_command", &[12]);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_MG_SELECTED_COMMAND, 2, 0, 0, 0), Some(12));
    let mut f = Fake::default();
    f.answer("main_group_selected_unit", &[5]);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_MG_SELECTED_COMMAND, 2, 0, 0, 0), Some(0));
    assert_eq!(f.calls(), "main_group_selected_unit()\nunit_has_complex(5)");
    let mut f = Fake::default();
    f.answer("formation_new", &[70]);
    run(&mut f, EE_SCREENS_GAME_MG_SET_FORMATION, 3, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "formation_new(3)\nsend_change_formation(3)\nmain_form_group_disable(8)\n\
         main_form_background_visible(152, false)\nformation_set_b5c(70)\nselected_group_set_formation(70)\n\
         formation_dereference(70)"
    );
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_GAME_MG_SHIFT, 1, 0, 0, 0);
    run(&mut f, EE_SCREENS_GAME_MG_SHIFT, 0, 0, 0, 0);
    assert_eq!(f.calls(), "send_enter_shift()\nsend_exit_shift()");
}

#[test]
fn the_diplomacy_form() {
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_OP_DS_UV, 2, 0.25f32.to_bits(), 0.5f32.to_bits(), 1.0f32.to_bits());
    assert_eq!(f.calls(), "bitmap_set_corners(256, 2, 0.25, 0.0, 0.5, 0.0, 0.25, 1.0, 0.5, 1.0)");
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_OP_DS_FG, 2, 3, 1, 0);
    run(&mut f, EE_SCREENS_OP_DS_FG, 2, 3, 0, 0);
    assert_eq!(f.calls(), "button_text_player_colour(256, 2, 3)\nlabel_text_player_colour(256, 2, 3)");
}

#[test]
fn the_post_game() {
    let mut f = Fake::default();
    f.answer("form_control", &[10]).answer("control_id", &[100]).answer("control_index", &[20, 41]);
    f.answer("control_border", &[2]).answer("pgs_status_bar_new", &[11]);
    assert_eq!(run(&mut f, EE_SCREENS_OP_PGS_NEW_BAR, 5, 3, 1, 0), Some(41));
    assert_eq!(
        f.calls(),
        "form_control(256, 5)\ncontrol_id(10)\ncontrol_index(10)\ncontrol_border(10)\npgs_status_bar_new(102, 256, 22, 2)\n\
         status_bar_clear_text(11)\nform_add_control(256, 11, 1)\ncontrol_index(11)"
    );
    let mut f = Fake::default();
    f.answer("form_ui_bit_depth", &[32]).answer("texture_load", &[20]).answer("bitmap_new", &[21]);
    f.answer("form_screen", &[0, 0]).answer("ui_bitmap_new", &[22]);
    run(&mut f, EE_SCREENS_OP_PGS_NEW_STAR, 800, 9, 2, 0);
    let calls = f.calls();
    assert!(calls.starts_with("form_ui_bit_depth(256)\ntexture_load(\"textures\\\\gold star\", "), "{calls}");
    assert!(calls.contains(
        "texture_alpha_modulate(20)\ntexture_color_modulate(20)\ntexture_min_filter(20, 1)\ntexture_mag_filter(20, 1)\n\
         texture_mip_point(20)\ntexture_mip_bias(20, 0.0)\ntexture_clamp(20)\nbitmap_new()\nform_screen(256)\n\
         bitmap_set_texture(21, 20, 0)\nform_screen(256)\nbitmap_stretch_to_control(21, 256, 790, 32.0, 32.0, 0)\n\
         ui_bitmap_new(800, 21, 256, 790, 9)\nform_add_control(256, 22, 2)\nbitmap_dereference(21)"
    ));
    let mut f = Fake::default();
    f.answer("spec_new", &[1, 2]).answer("spec_next_file", &[0, 0, 4]).text("spec_file", b"EEPostGame-1.jpg");
    f.answer("map_background_new", &[30]);
    run(&mut f, EE_SCREENS_GAME_PGS_SCREENSHOT, 0, 0, 0, 0);
    let calls = f.calls();
    assert!(calls.contains("spec_first_file(1, \"EEPostGame-1-0-0-???.jpg\", 2)\nspec_next_file(1, 2)\nspec_next_file(1, 2)\nspec_next_file(1, 2)\nspec_file(2, out)\nform_lock_background(256)"), "{calls}");
    assert!(calls.contains(", 256, 3)\nbackground_open(30)\nbackground_load(30)\nform_unlock_background(256)\nspec_delete(2)\nspec_delete(1)"));
    let mut f = Fake::default();
    f.answer("pgs_campaign_is_demo", &[1]);
    run(&mut f, EE_SCREENS_GAME_PGS_CAMPAIGN_SETUP, 0, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "pgs_campaign_first_from_civ()\npgs_campaign_description()\npgs_campaign_is_demo()\npgs_campaign_set_all_open(true)"
    );
    let mut f = Fake::default();
    f.answer("form_control", &[10]);
    run(&mut f, EE_SCREENS_OP_PGS_REMOVE, 5, 1, 0, 0);
    assert_eq!(f.calls(), "form_control(256, 5)\nform_remove_control(256, 10, 1)\ncontrol_delete(10)");
}

#[test]
fn the_splash_and_civ_choice() {
    let mut f = Fake::default();
    f.answer("form_background", &[6]).answer("background_left", &[1]).answer("background_top", &[2]);
    f.answer("background_right", &[3]);
    run(&mut f, EE_SCREENS_OP_SS_BACKGROUND, 9.0f32.to_bits(), 0, 0, 0);
    assert_eq!(
        f.calls(),
        "form_background(256, 132)\nbackground_left(6)\nbackground_top(6)\nbackground_right(6)\n\
         background_set_rect(6, 1.0, 2.0, 3.0, 9.0)\nbackground_set_visible(6, true)"
    );
    let mut f = Fake::default();
    f.answer("form_control", &[4]);
    run(&mut f, EE_SCREENS_OP_PDCS_STYLE, 3, 3, 0.5f32.to_bits(), 1.0f32.to_bits());
    assert_eq!(
        f.calls(),
        "form_control(256, 3)\ncb_list_set_auto_select(4, false)\ncb_list_set_show_points(4, false)\n\
         cb_list_set_select_upgrades_only(4, false)\ncb_list_set_sort(4, true)\ncb_list_set_colour(4, 0.5, 0.5, 1.0)"
    );
    let mut f = Fake::default();
    f.answer("civ_request_new", &[5]);
    run(&mut f, EE_SCREENS_GAME_PDCS_REQUEST, 0, 0, 0, 0);
    assert_eq!(f.calls(), "civ_request_new()\ncomms_multiplayer()\nmessage_execute(5)\nmessage_dereference(5)");
    let mut f = Fake::default();
    f.answer("civ_request_new", &[5]).answer("comms_multiplayer", &[1]);
    run(&mut f, EE_SCREENS_GAME_PDCS_REQUEST, 0, 0, 0, 0);
    assert_eq!(f.calls(), "civ_request_new()\ncomms_multiplayer()\nsend_to_server(5)");
    let mut f = Fake::default();
    f.answer("pdcs_civs_count", &[7]);
    assert_eq!(run(&mut f, EE_SCREENS_GAME_PDCS_CIVS, 0, 0, 0, 0), Some(7));
}

#[test]
fn the_campaign_editor_and_lobby_copy() {
    let none = format!("{:?}", String::from_utf8_lossy(&string(20009)));
    let mut f = Fake::default();
    f.answer("spec_new", &[1]).answer("path_new", &[2]);
    run(&mut f, EE_SCREENS_GAME_CE_OPEN, 6, 0, 0, 0);
    assert_eq!(
        f.calls().replace(&none, "NONE"),
        "spec_new()\npath_new()\nspec_set_volume_startup(1)\nspec_set_directory_startup(1, \"data\\\\Scenarios\\\\\")\n\
         path_add_directory(2, 1)\ncombo_add_extension(256, 6, \".jpg\")\ncombo_add_extension(256, 6, \".JPG\")\n\
         combo_set_paths(256, 6, 2)\ncombo_create(256, 6)\ncombo_insert_line(256, 6, NONE, 0)\ncombo_select(256, 6, 0)\n\
         path_dereference(2)\nspec_delete(1)"
    );
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_OP_CE_BG_NONE, 6, 0, 0, 0);
    assert_eq!(
        f.calls().replace(&none, "NONE"),
        "combo_create(256, 6)\ncombo_insert_line(256, 6, NONE, 0)\ncombo_select(256, 6, 0)"
    );
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_GAME_LE_RANDOM_MAP_OPTIONS, 0, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "lobby_copy_exists()\nlobby_copy_new()\nlobby_copy_from_options()\nlobby_copy_clear_load_type()\n\
         options_from_lobby_copy()"
    );
}
