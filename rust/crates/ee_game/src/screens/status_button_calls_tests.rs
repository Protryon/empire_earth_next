//! status_button_calls.rs over the recording fake: each op's and member's C++ calls, in the C++ order.

use super::*;
use crate::screens::game_cpp::Fake;

const REC: usize = 0x60;
const BUTTON: usize = 0x70;

#[test]
fn the_node_ops_skip_a_command_button() {
    let mut f = Fake::default();
    f.answer("sbt_is_command", &[1, 0]).answer("sbt_node", &[9]).answer("sbt_node_kind", &[0, 4]);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_NODE_KIND, 0, 0, 0), 0);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_NODE_KIND, 0, 0, 0), 4);
    assert_eq!(
        f.calls(),
        "sbt_is_command(96)\nsbt_node_kind(0)\nsbt_is_command(96)\nsbt_node(96)\nsbt_node_kind(9)"
    );
    let mut f = Fake::default();
    f.answer("sbt_is_command", &[1]);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_NODE_PRESENT, 0, 0, 0), 0);
}

#[test]
fn the_sends_and_the_calamity_cost() {
    let mut f = Fake::default();
    call(&mut f, REC, EE_SCREENS_SB_OP_SEND, EE_SCREENS_SB_SEND_UNLOAD, 0, 0);
    call(&mut f, REC, EE_SCREENS_SB_OP_SEND, EE_SCREENS_SB_SEND_RESEARCH, 1, 0);
    call(&mut f, REC, EE_SCREENS_SB_OP_SEND, EE_SCREENS_SB_SEND_EXCHANGE, 0, 0);
    assert_eq!(f.calls(), "sbt_send_unload(96)\nsbt_send_research(96, true)\nsbt_send_remove(96, false)");
    let mut f = Fake::default();
    f.answer("sbt_node", &[9]).answer("sbt_node_calamity_class", &[0, 3]).answer("sbt_calamity_cost", &[250]);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_CALAMITY_COST, 0, 0, 0), 0);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_CALAMITY_COST, 0, 0, 0), 250);
}

#[test]
fn the_texts() {
    let mut f = Fake::default();
    call(&mut f, REC, EE_SCREENS_SB_OP_CHAT_STRING, 33, 0, 0);
    call(&mut f, REC, EE_SCREENS_SB_OP_COUNT_TEXT, -5, 0, 0);
    assert_eq!(
        f.calls(),
        "sbt_string_clear()\nsbt_string_load(33)\nsbt_chat_add_string()\nsbt_count_text(96, \"-5\")"
    );
    let mut f = Fake::default();
    f.answer("sbt_button_object_of", &[0, 4]).answer("sbt_object_hot_key", &[7]).answer("sbt_text_length", &[2]);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_HELP_TEXT, EE_SCREENS_SB_TEXT_OBJECT_HOT_KEY, 0, 0), -1);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_HELP_TEXT, EE_SCREENS_SB_TEXT_OBJECT_HOT_KEY, 0, 0), 2);
    assert_eq!(
        f.calls(),
        "sbt_button_object_of(96)\nsbt_button_object_of(96)\nsbt_object_hot_key(4)\nsbt_hot_key_name(96, 7)\n\
         sbt_text_length(96)"
    );
}

#[test]
fn the_icon_and_the_units() {
    let mut f = Fake::default();
    call(&mut f, REC, EE_SCREENS_SB_OP_ICON, EE_SCREENS_BUTTON_ICON_NONE as i64, 0, 0);
    call(&mut f, REC, EE_SCREENS_SB_OP_ICON, EE_SCREENS_BUTTON_ICON_PRESSED as i64, 0, 0);
    assert_eq!(
        f.calls(),
        "sbt_set_draw_icon(96, false)\nsbt_object_pressed_icon(96)\nsbt_set_draw_icon(96, true)"
    );
    let mut f = Fake::default();
    f.answer("sbt_unit", &[0, 5]).answer("sbt_unit_is_priest", &[1]);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_UNIT_PRIEST, 2, 0, 0), 0);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_UNIT_PRIEST, 2, 0, 0), 1);
    let mut f = Fake::default();
    f.answer("sbt_class_is_land_building", &[1, 1]).answer("sbt_class_naval_building", &[1, 0]);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_CLASS_LAND_BUILDING, 1, 0, 0), 0);
    assert_eq!(call(&mut f, REC, EE_SCREENS_SB_OP_CLASS_LAND_BUILDING, 1, 0, 0), 1);
    let mut f = Fake::default();
    call(&mut f, REC, EE_SCREENS_SB_OP_MAIN_GROUP, 0, 0, 0);
    assert_eq!(f.calls(), "sbt_main_group_l1c(1)\nsbt_main_group_reset()");
}

#[test]
fn the_constructor_loads_the_dither() {
    let mut f = Fake::default();
    f.answer("sbt_normal_bitmap", &[1]).answer("sbt_pressed_bitmap", &[2]).answer("sbt_left", &[0]);
    f.answer("sbt_load_dither", &[8]);
    construct(&mut f, BUTTON, 3, 4, 5);
    let calls = f.calls();
    assert!(calls.starts_with(
        "sbt_object_clear(112)\nsbt_normal_bitmap(112, 3, 5)\nsbt_pressed_bitmap(112, 3, 5)\n\
         sbt_set_pressed_bitmap(112, 2)\nsbt_set_normal_bitmap(112, 1)\num_bitmap_dereference(1)\n\
         um_bitmap_dereference(2)\nsbt_set_f13x(112, 40.0)\nsbt_icon_new(112)\nsbt_left(112)\nsbt_top(112)\n\
         sbt_icon_at(112, 0.0, 0.0)\nsbt_queue_new(112, 5)\nsbt_queue_verify(112)\nsbt_queue_at(112, 3.0, 1.0)"
    ));
    assert!(calls.ends_with(
        "sbt_dither_new(112)\nsbt_dither_texture(112, 8, 3)\nsbt_texture_height(8)\nsbt_texture_width(8)\n\
         sbt_dither_stretch(112, 4, 0.0, 0.0, 3)"
    ));
    let mut f = Fake::default();
    construct(&mut f, BUTTON, 3, 4, 5);
    assert!(f.calls().ends_with("sbt_load_dither(3)"));
}

#[test]
fn the_members() {
    let mut f = Fake::default();
    f.answer("sbt_object", &[1]);
    destroy(&mut f, BUTTON);
    assert_eq!(
        f.calls(),
        "sbt_icon_dereference(112)\nsbt_queue_delete(112)\nsbt_dither_dereference(112)\nsbt_object(112)\n\
         sbt_object_release(112)\nsbt_object_clear(112)"
    );
    let mut f = Fake::default();
    f.answer("sbt_draw_icon", &[1]).answer("sbt_base_draw_polygons", &[6]);
    assert_eq!(draw_polygons(&mut f, BUTTON), 6);
    assert_eq!(
        f.calls(),
        "sbt_draw_icon(112)\nsbt_icon_white(112)\nsbt_draw_icon_bitmap(112)\nsbt_available(112)\n\
         sbt_dither_at_drawn(112)\nsbt_draw_dither(112)\nsbt_base_draw_polygons(112)"
    );
    let mut f = Fake::default();
    draw_text(&mut f, BUTTON);
    hot_key_up(&mut f, BUTTON);
    set_location(&mut f, BUTTON, 10.0, 20.0);
    assert_eq!(
        f.calls(),
        "sbt_show_count(112)\nsbt_base_draw_text(112)\nsbt_pressed(112)\nsbt_icon_at(112, 10.0, 20.0)\n\
         sbt_queue_at(112, 13.0, 21.0)\nsbt_base_set_location(112, 10.0, 20.0)"
    );
    let mut f = Fake::default();
    set_command(&mut f, BUTTON, 4);
    set_tech(&mut f, BUTTON, 7, 2, 9);
    assert_eq!(
        f.calls(),
        "sbt_object(112)\nsbt_set_count(112, 0)\nsbt_set_command(112, true)\nsbt_set_button(112, 4)\n\
         sbt_set_object(112, 4)\nsbt_object_acquire(112)\nsbt_object(112)\nsbt_set_command(112, false)\n\
         sbt_set_count(112, 0)\nsbt_set_technology(112, 7)\nsbt_set_unit_type(112, 2)\nsbt_set_unit_id(112, 9)\n\
         sbt_set_node_object(112, 7)\nsbt_object_acquire(112)"
    );
}
