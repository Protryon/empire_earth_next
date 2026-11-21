//! status_box_calls.rs over the recording fake: each op's C++ calls, in the C++ case's order.

use super::*;
use crate::screens::game_cpp::Fake;

const CALL: usize = 0x50;

fn pack(x: f32, y: f32) -> i64 {
    (x.to_bits() as u64 | (y.to_bits() as u64) << 32) as i64
}

#[test]
fn queued_marks_a_change_dirty() {
    let mut f = Fake::default();
    f.answer("sb_upgrade_queued", &[2, 3]);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_QUEUED, 4, 3, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_QUEUED, 4, 3, 0);
    assert_eq!(
        f.calls(),
        "sb_upgrade_queued(80, 4)\nsb_control_set_dirty(80, 4)\nsb_upgrade_set_queued(80, 4, 3)\n\
         sb_upgrade_queued(80, 4)\nsb_upgrade_set_queued(80, 4, 3)"
    );
}

#[test]
fn the_name_of_a_locked_gate() {
    let mut f = Fake::default();
    f.answer("sb_name_empty_fill", &[1]).answer("sb_is_gate_fill", &[1]).answer("sb_gate_locked", &[1]);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_NAME, 0, 0, 0);
    assert_eq!(
        f.calls(),
        "sb_suffix_clear()\nsb_name_empty_fill(80)\nsb_text_class_name(80)\nsb_is_gate_fill(80)\nsb_gate_locked(80)\n\
         sb_suffix_locked()\nsb_text_append_suffix(80)"
    );
    let mut f = Fake::default();
    call(&mut f, CALL, EE_SCREENS_BOX_OP_NAME, 1, 0, 0);
    assert_eq!(f.calls(), "sb_suffix_clear()\nsb_name_empty(80)\nsb_text_stats_name(80)\nsb_is_gate(80)");
}

#[test]
fn the_choices() {
    let mut f = Fake::default();
    call(&mut f, CALL, EE_SCREENS_BOX_OP_COST_LABEL, 1, -1, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_COST_LABEL, 1, 5, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_BAR_COLOR, 1, 2, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_BAR_COLOR, 1, 7, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_SET_BITMAP, 1, 1, 9);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_PORTRAIT, 0, 0, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_VERIFY, 367, 0, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_VERIFY, 708, 0, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_SET_VALUE, 2, 1, 0);
    assert_eq!(
        f.calls(),
        "sb_upgrade_cost_label_clear(80, 1)\nsb_upgrade_cost_label(80, 1, 5)\nsb_bar_color(80, 1, 2)\n\
         sb_bar_color(80, 1, 0)\nsb_button_set_pressed(80, 1, 9)\nsb_portrait_clear(80)\nsb_verify_armor_count()\n\
         sb_verify_count()\nsb_set_value2(80, 2)"
    );
    let mut f = Fake::default();
    f.answer("sb_main_background", &[0, 6]);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_BACKGROUND, 0x7d, 1, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_BACKGROUND, 0x7d, 1, 0);
    assert_eq!(f.calls(), "sb_main_background(125)\nsb_main_background(125)\nsb_background_set_visible(6, true)");
}

#[test]
fn the_answers() {
    let mut f = Fake::default();
    f.answer("sb_button_normal_is", &[1]).answer("sb_class_armor", &[7]).answer("sb_stats_armor", &[0]);
    f.answer("sb_texture_width", &[256]).answer("sb_texture_height", &[128]);
    assert_eq!(call(&mut f, CALL, EE_SCREENS_BOX_OP_BITMAP_IS, 1, 0, 3), 1);
    assert_eq!(call(&mut f, CALL, EE_SCREENS_BOX_OP_ARMOR, 2, 1, 0), 7);
    assert_eq!(call(&mut f, CALL, EE_SCREENS_BOX_OP_ARMOR, 2, 0, 0), 0);
    assert_eq!(call(&mut f, CALL, EE_SCREENS_BOX_OP_TEXTURE_SIZE, 1, 0, 0), 256 << 32 | 128);
    assert_eq!(call(&mut f, CALL, 0xffff, 0, 0, 0), 0);
}

#[test]
fn the_icons_load() {
    let mut f = Fake::default();
    f.answer("sb_primary_width", &[800]).answer("texture_load", &[44]);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_DESCRIPTION, 0, 0, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_LOAD_TEXTURE, 2, 0, 0);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_SET_RECT, 3, pack(1.0, 2.0), pack(3.0, 4.0));
    call(&mut f, CALL, EE_SCREENS_BOX_OP_BITMAP_TEXTURE, 3 | 2 << 8, pack(0.25, 0.5), pack(0.75, 1.0));
    call(&mut f, CALL, EE_SCREENS_BOX_OP_BITMAP_SCALE, 3, pack(0.5, 2.0), 0);
    assert_eq!(
        f.calls(),
        "sb_primary_width()\nsb_set_description(80, 1)\nsb_description(80)\n\
         texture_load(\"textures\\\\unit improvements_02\", 0)\ntexture_alpha_modulate(44)\ntexture_color_modulate(44)\n\
         texture_min_filter(44, 1)\ntexture_mag_filter(44, 1)\ntexture_mip_point(44)\ntexture_mip_bias(44, -0.75)\n\
         texture_clamp(44)\nsb_set_texture(80, 2, 44)\nsb_set_rect(3, 1.0, 2.0, 3.0, 4.0)\n\
         sb_bitmap_set_texture(3, 80, 2)\nsb_bitmap_set_coordinates(3, 0.25, 0.5, 0.75, 1.0)\nsb_bitmap_scale(3, 0.5, 2.0)"
    );
}

#[test]
fn the_bar_icon_and_the_unload_and_reload() {
    let mut f = Fake::default();
    f.answer("sb_bar_control", &[60]).answer("sb_bitmap", &[61]).answer("sb_icon_rect", &[62]);
    call(&mut f, CALL, EE_SCREENS_BOX_OP_BAR_ICON, 1, 4, 0);
    assert_eq!(
        f.calls(),
        "sb_bar_control(80, 1)\nsb_bitmap(4)\nsb_icon_rect(4)\nsb_bar_bitmap(60, 61)\nsb_bar_stretch(60, 61, 62)\n\
         sb_bar_dirty(60)"
    );
    let mut f = Fake::default();
    let mut present = vec![0i64; 50];
    present[7] = 1;
    f.answer("sb_bitmap", &present);
    unload_icons(&mut f);
    assert_eq!(f.log.len(), 50 + 3);
    assert_eq!(f.log[8], "sb_bitmap_dereference(7)");
    assert_eq!(f.log.last().unwrap(), "sb_icons_set_loaded(false)");
    let mut f = Fake::default();
    f.answer("sb_primary_width", &[1024]).answer("sb_reload", &[0, 5]);
    assert_eq!(reload_icons(&mut f), 5);
    assert_eq!(f.calls(), "sb_primary_width()\nsb_set_reload_description(0)\nsb_reload(1)\nsb_reload(2)");
}
