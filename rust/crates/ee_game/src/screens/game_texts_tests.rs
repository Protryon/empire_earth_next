//! game_texts.rs over the recording fake: each moved text's C++ calls, in the C++ case's order.

use super::*;
use crate::screens::game_cpp::Fake;

const FORM: usize = 0x100;

#[test]
fn the_parts() {
    assert_eq!(parts(b"a\0b"), [&b"a"[..], b"b", b""]);
    assert_eq!(parts(b""), [&b""[..], b"", b""]);
    assert_eq!(parts(b"a\0b\0c\0d").len(), 4);
    assert_eq!(cut(b"ab\0cd"), b"ab");
}

#[test]
fn texts_that_are_not_here_go_on() {
    let mut f = Fake::default();
    assert_eq!(text(&mut f, FORM, 0xffff, 0), None);
    assert_eq!(put(&mut f, FORM, 0xffff, 0, b""), None);
    assert!(f.log.is_empty());
}

#[test]
fn the_reads() {
    let mut f = Fake::default();
    f.answer("pdcs_selection", &[5]);
    assert_eq!(text(&mut f, FORM, EE_SCREENS_TEXT_PDCS_LINE, 0), Some(None));
    let mut f = Fake::default();
    f.out("pdcs_selection", 3).text("pdcs_line", b"Greeks");
    assert_eq!(text(&mut f, FORM, EE_SCREENS_TEXT_PDCS_LINE, 0), Some(Some(b"Greeks".to_vec())));
    assert_eq!(f.calls(), "pdcs_selection(out)\npdcs_line(3, out)");
    // past the lists: ""
    let mut f = Fake::default();
    assert_eq!(text(&mut f, FORM, EE_SCREENS_TEXT_DISPLAY_SCREEN, 0x10002), Some(Some(vec![])));
    assert_eq!(f.calls(), "display_rasterizer_count()");
    let mut f = Fake::default();
    f.answer("display_rasterizer_count", &[2]).answer("display_screen_count", &[3]).text("display_screen_name", b"S");
    assert_eq!(text(&mut f, FORM, EE_SCREENS_TEXT_DISPLAY_SCREEN, 0x10002), Some(Some(b"S".to_vec())));
    assert_eq!(f.calls(), "display_rasterizer_count()\ndisplay_screen_count(1)\ndisplay_screen_name(1, 2, out)");
    let mut f = Fake::default();
    f.answer("rasterizer_count", &[1]).text("rasterizer_name", b"GX");
    assert_eq!(text(&mut f, FORM, EE_SCREENS_TEXT_RASTERIZER_NAME, 0), Some(Some(b"GX".to_vec())));
    // a check box without a label: missing
    let mut f = Fake::default();
    f.answer("form_control", &[40]);
    assert_eq!(text(&mut f, FORM, EE_SCREENS_TEXT_LE_CHECK_LABEL, 7), Some(None));
    let mut f = Fake::default();
    f.answer("form_control", &[40]).answer("check_box_label", &[41, 41]).text("label_text", b"Reveal");
    assert_eq!(text(&mut f, FORM, EE_SCREENS_TEXT_LE_CHECK_LABEL, 7), Some(Some(b"Reveal".to_vec())));
    assert_eq!(f.calls(), "form_control(256, 7)\ncheck_box_label(40)\ncheck_box_label(40)\nlabel_text(41, out)");
}

#[test]
fn the_campaign_archive_and_header() {
    let mut f = Fake::default();
    f.answer("archive_new", &[9]).out("data_find_file", 30);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_CS_ARCHIVE, 1, b"c.eec\0x"), Some(1));
    assert_eq!(
        f.calls(),
        "archive_new()\ndata_find_file(\"c.eec\", out)\narchive_set(9, 30)\ndata_add_archive(9)\nset_data_paths()\n\
         spec_delete(30)\narchive_delete(9)"
    );
    // the start: the media into the intro's manager, the file handed to it
    let mut f = Fake::default();
    f.answer("header_new", &[5]).text("scenario_prefix", b"P_").text("scenario_extension", b".scn");
    f.out("data_find_file", 31);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_CS_START, 0, b"one"), Some(2));
    assert_eq!(
        f.calls(),
        "header_new()\nscenario_prefix(out)\nscenario_extension(out)\ndata_find_file(\"Scenarios\\\\P_one.scn\", out)\n\
         header_load(5, 31)\ncampaign_update_checksum()\nintro_media_from_header(5)\nintro_prefix_media()\n\
         intro_set_file(31)\nintro_set_flag(2, true)\nheader_delete(5)"
    );
    // a header that does not load: 1 for Start, 0 for the header's question
    for (which, r) in [(EE_SCREENS_TEXT_CS_START, 1), (EE_SCREENS_TEXT_CS_SCENARIO_HEADER, 0)] {
        let mut f = Fake::default();
        f.out("data_find_file", 31).answer("header_load", &[3]);
        assert_eq!(put(&mut f, FORM, which, 0, b"one"), Some(r));
        assert!(f.calls().ends_with("header_load(0, 31)\nspec_delete(31)\nheader_delete(0)"));
    }
    let mut f = Fake::default();
    f.out("data_find_file", 31);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_CS_SCENARIO_HEADER, 0, b"one"), Some(1));
    assert!(f.calls().ends_with("spec_delete(31)\nheader_delete(0)"));
}

#[test]
fn the_cheats_and_buttons() {
    let mut f = Fake::default();
    put(&mut f, FORM, EE_SCREENS_TEXT_CH_CHEAT, 0, b"nothing");
    assert_eq!(f.calls(), "parse_cheat(\"nothing\", out, out)");
    let mut f = Fake::default();
    f.out("parse_cheat", 12).out("parse_cheat", -3);
    put(&mut f, FORM, EE_SCREENS_TEXT_CH_CHEAT, 0, b"code");
    assert_eq!(f.log[1], "send_cheat(12, -3)");
    let mut f = Fake::default();
    f.answer("button_font", &[77]);
    put(&mut f, FORM, EE_SCREENS_TEXT_GO_BUTTON, 4, b"Resign");
    assert_eq!(f.calls(), "button_font(256, 4)\nbutton_set_text(256, 4, \"Resign\", 77)");
}

#[test]
fn the_loads() {
    // the quick save not found: the message
    let mut f = Fake::default();
    f.text("settings_last_save", b"QuickSave 1").answer("spec_new", &[8]).answer("open_save_game_file", &[2]);
    f.out("open_save_game_file", 8);
    put(&mut f, FORM, EE_SCREENS_TEXT_MG_QUICK_LOAD, 0, b"none");
    assert_eq!(
        f.calls(),
        "settings_last_save(false, out)\nspec_new()\nopen_save_game_file(\"QuickSave 1\", \"Saved Games\\\\\", out)\n\
         display_message(\"none\", true)\nspec_delete(8)"
    );
    // found: its file posted
    let mut f = Fake::default();
    f.text("settings_last_save", b"Auto").out("open_save_game_file", 9);
    put(&mut f, FORM, EE_SCREENS_TEXT_MG_QUICK_LOAD, 1, b"none");
    assert_eq!(f.log[3..], ["spec_set_file(9, \"Auto\")", "post_load_saved_game(9)", "spec_delete(9)"]);
    // the single player load
    let mut f = Fake::default();
    f.out("open_save_game_file", 9).answer("validate_save_game", &[1]);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_LOAD_SAVED_GAME, 0, b"g.sav\0Saved Games\\"), Some(1));
    assert_eq!(
        f.calls(),
        "open_save_game_file(\"g.sav\", \"Saved Games\\\\\", out)\nspec_open_read(9)\nshim_message_reset()\n\
         validate_save_game(9)\nlobby_options_write_lock()\nlobby_options_set_defaults()\n\
         lobby_options_set_load_type(2)\nlobby_options_map_type_from_spec(9)\nlobby_options_write_unlock()\n\
         load_saved_game_name_from_spec(9)\nspec_delete(9)\ngame_state_single()\ndelete_communications()\n\
         new_local_server()\nserver_start_threads()\nserver_set_load_responses(false)\nset_game_state_load_type(2)\n\
         server_stop_ping()\nserver_set_load_state(true)\nsend_scenario_start_loading()"
    );
    // an invalid one: its reason shown
    let mut f = Fake::default();
    f.out("open_save_game_file", 9);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_LOAD_SAVED_GAME, 0, b"g.sav\0d"), Some(0));
    assert_eq!(f.log[3..], ["validate_save_game(9)", "display_shim_message()", "spec_delete(9)"]);
    // not opened
    let mut f = Fake::default();
    f.out("open_save_game_file", 9).answer("spec_open_read", &[1]);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_LOAD_SAVED_GAME, 0, b"g.sav\0d"), Some(0));
    assert_eq!(f.log[2], "spec_delete(9)");
}

#[test]
fn the_scenario_intro() {
    let mut f = Fake::default();
    f.answer("header_new", &[5]).out("open_save_game_file", 9);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_SCENARIO_INTRO, 0, b"s.scn\0Scenarios\\"), Some(2));
    assert_eq!(
        f.calls(),
        "header_new()\nopen_save_game_file(\"s.scn\", \"Scenarios\\\\\", out)\nheader_set_major(5)\nheader_set_minor(5)\n\
         header_load(5, 9)\nintro_media_from_header(5)\nintro_set_file(9)\nintro_set_flag(2, false)\n\
         load_scenario_name_from_spec(9)\nheader_delete(5)"
    );
    let mut f = Fake::default();
    f.answer("open_save_game_file", &[1]);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_SCENARIO_INTRO, 0, b"s"), Some(0));
}

#[test]
fn the_file_list_and_checks() {
    let mut f = Fake::default();
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_FILE_LIST, 0x0005_0003, b".sav\0Saved Games\\\0g"), Some(0));
    assert_eq!(f.calls(), "manager_form_control(5, 3)");
    let mut f = Fake::default();
    f.answer("manager_form_control", &[60]).answer("spec_new", &[61]).answer("path_new", &[62]);
    f.answer("file_list_find", &[1]).out("file_list_find", 4);
    put(&mut f, FORM, EE_SCREENS_TEXT_FILE_LIST, 0x0005_0003, b".sav\0Saved Games\\\0g");
    assert_eq!(
        f.calls(),
        "manager_form_control(5, 3)\nspec_new()\npath_new()\nfile_list_add_extension(60, \".sav\")\n\
         spec_set_volume_startup(61)\nspec_set_directory_startup(61, \"Saved Games\\\\\")\npath_add_directory(62, 61)\n\
         file_list_set_paths(60, 62)\nfile_list_create(60)\nfile_list_find(60, \"g\", out)\nlist_select(60, 4)\n\
         list_scroll_to(60, 4)\npath_dereference(62)\nspec_delete(61)"
    );
    let mut f = Fake::default();
    f.answer("spec_new", &[61]);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_FILE_EXISTS, 0, b"a\0b"), Some(1));
    assert_eq!(
        f.calls(),
        "spec_new()\nspec_set_file(61, \"a\")\nspec_set_directory_startup(61, \"b\")\nspec_open_read(61)\n\
         spec_close(61)\nspec_delete(61)"
    );
    let mut f = Fake::default();
    f.answer("open_save_game_file", &[1]);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_LOAD_POST, 0, b"a\0b"), Some(0));
}

#[test]
fn the_rest() {
    let mut f = Fake::default();
    f.text("campaign_extension", b".eec").answer("campaign_load", &[1]);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_CAMPAIGN_LOAD, 0, b"Mine"), Some(1));
    assert_eq!(f.log[1..], ["campaign_error_reset()", "campaign_load(\"Campaigns\\\\Mine.eec\")"]);
    let mut f = Fake::default();
    f.answer("manager_form_control", &[3]);
    put(&mut f, FORM, EE_SCREENS_TEXT_LABEL_LINES, 0x0001_0002, b"L");
    assert_eq!(f.calls(), "manager_form_control(1, 2)\nlabel_delete_array(3)\nlabel_append_text(3, \"L\")");
    let mut f = Fake::default();
    put(&mut f, FORM, EE_SCREENS_TEXT_PGS_SEND, 80, b"host\0pay\0load");
    assert_eq!(f.calls(), "pgs_send(\"host\", 80, \"pay\\0load\")");
    let mut f = Fake::default();
    put(&mut f, FORM, EE_SCREENS_TEXT_PGS_SEND, 80, b"host");
    assert_eq!(f.calls(), "pgs_send(\"host\", 80, \"\")");
    let mut f = Fake::default();
    f.answer("pdcs_load", &[4]);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_PDCS_LOAD, 0, b"civ"), Some(4));
    assert_eq!(f.calls(), "pdcs_name_reset()\npdcs_upgrades_clear()\npdcs_load(\"civ\")");
    let mut f = Fake::default();
    put(&mut f, FORM, EE_SCREENS_TEXT_DSU_LABEL, 3, b"x");
    assert_eq!(f.calls(), "debug_sound_label(3)");
    let mut f = Fake::default();
    f.answer("lobby_load", &[2]);
    assert_eq!(put(&mut f, FORM, EE_SCREENS_TEXT_LE_OPEN_HEADER, 4, b"a\0b"), Some(2));
    assert_eq!(f.calls(), "lobby_header_delete()\nlobby_header_new()\nlobby_file_clear()\nlobby_load(\"a\", \"b\", 4)");
    let mut f = Fake::default();
    put(&mut f, FORM, EE_SCREENS_TEXT_SERVER, 0, b"Me");
    assert_eq!(f.log[4], "send_connection_request(\"Me\")");
}
