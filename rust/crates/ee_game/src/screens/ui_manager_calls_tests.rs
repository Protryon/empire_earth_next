//! ui_manager_calls.rs over the recording fake: each moved case's and member's C++ calls, in the C++ order.

use super::*;
use crate::screens::game_cpp::Fake;

const CALL: usize = 0x30;

#[test]
fn not_moved_ops_go_to_the_host() {
    assert_eq!(call(&mut Fake::default(), CALL, EE_SCREENS_UM_OP_CITIZENS, 0, 0), None);
    assert_eq!(call(&mut Fake::default(), CALL, EE_SCREENS_UM_OP_BASE_MOUSE, 0, 0), None);
}

#[test]
fn register_and_save_event() {
    let mut f = Fake::default();
    f.answer("um_call_manager", &[7, 7]).answer("screen_event_new", &[9]);
    call(&mut f, CALL, EE_SCREENS_UM_OP_REGISTER, 0, 0);
    call(&mut f, CALL, EE_SCREENS_UM_OP_SAVE_EVENT, 0, 0);
    assert_eq!(
        f.calls(),
        "um_call_manager(48)\num_register_game_event(7)\num_register_wns_event(7)\num_register_form_event(7)\n\
         um_call_manager(48)\num_clear_u138(7)\nscreen_event_new(769)\num_save_form_set_activate(7, 9)\n\
         event_dereference(9)"
    );
}

#[test]
fn the_editor_start_cases() {
    let mut f = Fake::default();
    f.answer("comms_present", &[1, 0]);
    call(&mut f, CALL, EE_SCREENS_UM_OP_NEW_SERVER, 0, 0);
    call(&mut f, CALL, EE_SCREENS_UM_OP_NEW_SERVER, 0, 0);
    assert_eq!(
        f.calls(),
        "comms_present()\ncomms_delete()\num_new_server()\nserver_start_threads()\nserver_clear_load_responses()\n\
         comms_present()\num_new_server()\nserver_start_threads()\nserver_clear_load_responses()"
    );
    let mut f = Fake::default();
    call(&mut f, CALL, EE_SCREENS_UM_OP_LOBBY_LOCK, 1, 0);
    call(&mut f, CALL, EE_SCREENS_UM_OP_LOBBY_LOCK, 0, 0);
    call(&mut f, CALL, EE_SCREENS_UM_OP_LOBBY_DEFAULTS, 0, 0);
    call(&mut f, CALL, EE_SCREENS_UM_OP_NAME, 1, 0);
    call(&mut f, CALL, EE_SCREENS_UM_OP_SERVER_LOAD, 0, 0);
    assert_eq!(
        f.calls(),
        "lobby_options_write_lock()\nlobby_options_write_unlock()\nlobby_set_map_size(1)\nlobby_set_load_type(3)\n\
         lobby_set_epoch_prehistoric()\nlobby_set_variant(0)\num_name_computer(48)\nserver_stop_ping()\n\
         server_set_load_state(true)\ncomms_start_loading()"
    );
    let p = EeScreensManagerPlayer {
        slot: 3,
        color: 4,
        kind: 0,
        starting_citizens: 5,
        is_local: 0,
        is_ready: 1,
        name: 1,
        reserved: 0,
    };
    let mut f = Fake::default();
    call(&mut f, CALL, EE_SCREENS_UM_OP_SET_PLAYER, 0, &p as *const EeScreensManagerPlayer as i64);
    assert_eq!(
        f.calls(),
        "lobby_player_reset(3)\nlobby_player_set_type(3, 0)\num_lobby_player_set_name(48, 3, 1)\n\
         lobby_player_set_slot(3, 3)\nlobby_player_set_color(3, 4)\nlobby_player_set_citizens(3, 5)\n\
         lobby_player_set_local(3, false)\nlobby_player_set_ready(3, true)"
    );
}

#[test]
fn the_keys() {
    let mut f = Fake::default();
    f.answer("um_key_just_pressed", &[1]);
    assert_eq!(call(&mut f, CALL, EE_SCREENS_UM_OP_KEY_PRESSED, EE_SCREENS_UM_KEY_F9, 0), Some(1));
    assert_eq!(f.calls(), "um_key_load(3)\num_key_just_pressed()");
    let mut f = Fake::default();
    f.answer("um_current_form", &[5, 5, 0]).answer("form_keyboard_captured", &[0, 1]);
    assert_eq!(call(&mut f, CALL, EE_SCREENS_UM_OP_KEYS_FREE, 0, 0), Some(1));
    assert_eq!(call(&mut f, CALL, EE_SCREENS_UM_OP_KEYS_FREE, 0, 0), Some(0));
    assert_eq!(call(&mut f, CALL, EE_SCREENS_UM_OP_KEYS_FREE, 0, 0), Some(0));
    assert_eq!(f.log.iter().filter(|l| l.starts_with("loading_saved_game")).count(), 1);
}

#[test]
fn the_event_factories() {
    let mut f = Fake::default();
    assert_eq!(wns_event(&mut f, EE_SCREENS_UMC_WNS_CANCEL + 1), 0);
    f.answer("um_new_wns_cancel", &[4]);
    assert_eq!(wns_event(&mut f, EE_SCREENS_UMC_WNS_CANCEL), 4);
    // an id no factory knows: the screen event factory's answer for code 0
    let mut f = Fake::default();
    f.answer("form_manager", &[2]).answer("screen_event_new", &[0]);
    assert_eq!(game_event(&mut f, 1, 0xffff_ff00), 0);
    assert_eq!(f.log[0], "form_manager(1)");
    assert!(f.log.last().unwrap().starts_with("screen_event_new("));
    let mut f = Fake::default();
    form_event(&mut f, 0xffff_ff00);
    assert!(f.calls().starts_with("screen_event_new("));
}

#[test]
fn the_editor_form_and_forms() {
    let mut f = Fake::default();
    f.answer("um_editor_form", &[0, 8]);
    create_editor(&mut f, 1);
    create_editor(&mut f, 1);
    delete_editor(&mut f, 1);
    assert_eq!(f.calls(), "um_editor_form(1)\num_new_editor(1)\num_editor_form(1)\num_delete_editor(1)\num_clear_editor(1)");
    let mut f = Fake::default();
    f.answer("um_form_data_id", &[EE_SCREENS_UMC_FORM_MAIN_GAME as i64, 3]);
    create_form(&mut f, 1, 2);
    create_form(&mut f, 1, 2);
    assert_eq!(f.calls(), "um_form_data_id(2)\num_new_main_game(1, 2)\num_form_data_id(2)\num_base_create_form(1, 2)");
}

#[test]
fn the_generic_background() {
    let mut f = Fake::default();
    f.answer("bt_type", &[5]);
    assert_eq!(create_background(&mut f, 1, 2), 0);
    let mut f = Fake::default();
    f.answer("bt_type", &[EE_SCREENS_UMC_BACKGROUND_GENERIC as i64; 2]).answer("um_form_by_id", &[0, 6]);
    f.answer("bt_attribute1", &[1]).answer("bt_attribute3", &[1]).answer("um_new_generic_background", &[11]);
    assert_eq!(create_background(&mut f, 1, 2), 0);
    assert_eq!(create_background(&mut f, 1, 2), 11);
    assert_eq!(f.log.last().unwrap(), "um_new_generic_background(1, 2, 6, 5)");
}

#[test]
fn the_user_defined_controls() {
    let mut f = Fake::default();
    f.answer("um_form_by_id", &[6]).answer("um_window_width", &[800]).answer("um_window_height", &[600]);
    f.answer("ct_type", &[EE_SCREENS_UMC_SAVED_GROUP as i64]).answer("ct_new_saved_group", &[20]);
    f.answer("ct_normal_bitmap", &[21]).answer("ct_pressed_bitmap", &[22]).answer("um_button_control", &[23]);
    assert_eq!(create_control(&mut f, 1, 2), 23);
    assert_eq!(
        f.calls(),
        "ct_form_id(2)\num_form_by_id(1, 0)\num_window_width(1)\num_window_height(1)\nform_exclusive_coords(6)\n\
         um_parent_set(0.0, 0.0, 799.0, 599.0)\nct_layout_rectangle(2, 6)\nct_strings(1, 2)\nct_type(2)\n\
         ct_new_saved_group(2, 6)\nct_normal_bitmap(2, 6)\num_button_set_normal(20, 21)\num_bitmap_dereference(21)\n\
         ct_pressed_bitmap(2, 6)\num_button_set_pressed(20, 22)\num_bitmap_dereference(22)\num_button_control(20)\n\
         um_strings_clear()"
    );
    let mut f = Fake::default();
    f.answer("form_exclusive_coords", &[1]).answer("ct_type", &[EE_SCREENS_UMC_PGS_LINE_GRAPH as i64]);
    f.answer("ct_new_pgs_graph", &[30]);
    assert_eq!(create_control(&mut f, 1, 2), 30);
    assert!(f.calls().contains("um_parent_set(0.0, 0.0, 0.0, 0.0)\n"));
    assert!(f.calls().contains("ct_new_pgs_graph(2, 0)\nct_graph_font(30, 2)\nct_graph_font_color(30, 2)\num_strings_clear()"));
    let mut f = Fake::default();
    f.answer("ct_type", &[EE_SCREENS_UMC_CB_LIST_BOX as i64]);
    create_control(&mut f, 1, 2);
    assert!(f.calls().contains("ct_setup_font(1, 2)\nct_new_cb_list_box(2, 0)"));
    let mut f = Fake::default();
    f.answer("ct_type", &[0]);
    assert_eq!(create_control(&mut f, 1, 2), 0);
}
