//! shell_calls.rs over the recording fake: each moved case's C++ calls, in the C++ case's order.

use super::*;
use crate::screens::game_cpp::Fake;

const CTL: usize = 0x80;

fn run(f: &mut Fake, op: u32, a: i64, b: i64, fl: [f32; 4]) -> (Option<i64>, [f32; 4]) {
    let mut fl = fl;
    let r = call(f, CTL, op, a, b, &mut fl);
    (r, fl)
}

#[test]
fn not_here() {
    assert_eq!(run(&mut Fake::default(), 0xffff, 0, 0, [0.0; 4]).0, None);
}

#[test]
fn the_toolkit_shells() {
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_SH_TEXT_FG, 1, 0, [0.5, 0.25, 1.0, 0.0]);
    assert_eq!(f.calls(), "shell_text_fg(128, 1, 0.5, 0.25, 1.0)\nshell_text_transparent(128, 1)");
    let mut f = Fake::default();
    f.answer("shell_combo_line_count", &[3, 3, 3]).answer("shell_combo_line_is", &[0, 1]);
    assert_eq!(run(&mut f, EE_SCREENS_SH_FILE_FIND_LINE, 0x99, 0, [0.0; 4]).0, Some(1));
    assert_eq!(
        f.calls(),
        "shell_combo_line_count(128)\nshell_combo_line_is(128, 0, 153)\nshell_combo_line_count(128)\n\
         shell_combo_line_is(128, 1, 153)"
    );
    let mut f = Fake::default();
    assert_eq!(run(&mut f, EE_SCREENS_SH_FILE_FIND_LINE, 0x99, 0, [0.0; 4]).0, Some(-1));
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_SH_BUTTON_TEXT, 9, 0x70, [0.0; 4]);
    assert!(f.calls().starts_with("shell_button_set_text(128, "));
    assert!(f.calls().ends_with(", 112)"));
}

#[test]
fn the_saved_groups() {
    let mut f = Fake::default();
    f.answer("sound_object", &[5]).answer("sound_sample_id", &[44]);
    assert_eq!(run(&mut f, EE_SCREENS_SH_SG_SOUND, 1, 0, [0.0; 4]).0, Some(44));
    assert_eq!(f.calls(), "sound_object(520)\nsound_load_media(5)\nsound_sample_id(5)");
    let mut f = Fake::default();
    f.answer("sound_object", &[5]);
    assert_eq!(run(&mut f, EE_SCREENS_SH_SG_SOUND, 0, 1, [0.0; 4]).0, Some(0));
    assert_eq!(f.calls(), "sound_object(519)\nsound_unload_media(5)");
    let mut f = Fake::default();
    f.answer("player_group", &[6]).answer("group_count", &[2]).answer("group_first_unit", &[7]);
    assert_eq!(run(&mut f, EE_SCREENS_SH_SG_FIRST_UNIT, 3, 1, [0.0; 4]).0, Some(7));
    assert_eq!(f.calls(), "player_group(3)\ngroup_count(6)\ngroup_first_unit(6)");
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_SH_SG_SEND, 0, 0x104, [0.0; 4]);
    run(&mut f, EE_SCREENS_SH_SG_SEND, 3, 0x2, [0.0; 4]);
    assert_eq!(f.calls(), "send_saved_group_select(4, true)\nsend_saved_group_clear(2)");
    let mut f = Fake::default();
    f.answer("unit_stats_button", &[9]).answer("button_cell_u", &[1]).answer("button_texture", &[11]);
    let (r, fl) = run(&mut f, EE_SCREENS_SH_SG_FACE, 3, 4, [0.0; 4]);
    assert_eq!((r, fl[0], fl[1]), (Some(11), 1.0, 0.0));
    assert_eq!(
        f.calls(),
        "unit_stats_button(3)\nbutton_set_face(9, 4, 128)\nbutton_cell_u(9)\nbutton_cell_v(9)\nbutton_texture(9)"
    );
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_SH_SG_TEXTURE_REF, 0, 1, [0.0; 4]);
    run(&mut f, EE_SCREENS_SH_SG_TEXTURE_REF, 5, 1, [0.0; 4]);
    run(&mut f, EE_SCREENS_SH_SG_TEXTURE_REF, 5, 0, [0.0; 4]);
    assert_eq!(f.calls(), "texture_reference(5)\ntexture_dereference(5)");
}

#[test]
fn the_icon_bar_and_chat_line() {
    let mut f = Fake::default();
    f.answer("sound_object", &[5, 0]).answer("shell_slot", &[0, 8]);
    run(&mut f, EE_SCREENS_SH_IB_FREE, 0, 0, [0.0; 4]);
    assert_eq!(
        f.calls(),
        "sound_object(519)\nsound_unload_media(5)\nsound_object(520)\nshell_slot(128, 0)\nshell_slot(128, 1)\n\
         bitmap_dereference(8)\nshell_clear_slot(128, 0)\nshell_clear_slot(128, 1)"
    );
    let mut f = Fake::default();
    f.answer("shell_slot", &[1, 2, 3, 4]);
    run(&mut f, EE_SCREENS_SH_CHAT_FREE, 0, 0, [0.0; 4]);
    assert_eq!(f.log.iter().filter(|l| l.starts_with("text_delete")).count(), 4);
}

#[test]
fn the_paint_areas() {
    let mut f = Fake::default();
    f.answer("pick_tile_xy", &[1]).out("pick_tile_xy", 12).out("pick_tile_xy", 34).answer("shim_tile_exists", &[1]);
    f.answer("shim_tile_height", &[2]);
    let (r, fl) = run(&mut f, EE_SCREENS_SH_PICK, 1, 0, [5.0, 6.0, 0.0, 0.0]);
    assert_eq!((r, fl), (Some(1), [12.0, 34.0, 2.0, 0.0]));
    assert_eq!(f.log[2], "pick_tile_xy(5.0, 6.0, out, out)");
    let mut f = Fake::default();
    f.answer("shim_point_x", &[1]).answer("shim_point_y", &[2]).answer("shim_point_z", &[3]);
    let (r, fl) = run(&mut f, EE_SCREENS_SH_PICK, 2, 0, [5.0, 6.0, 0.0, 0.0]);
    assert_eq!((r, fl), (Some(0), [1.0, 2.0, 3.0, 0.0]));
    assert_eq!(f.log[2], "pick_map_location(5.0, 6.0)");
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_SH_PICK, 0, 0, [5.0, 6.0, 0.0, 0.0]);
    assert_eq!(f.log[2], "pick_unit_or_tile(5.0, 6.0)");
    let mut f = Fake::default();
    f.answer("shim_map_tile_exists", &[1]).answer("shim_map_tile_height", &[7]);
    let (r, fl) = run(&mut f, EE_SCREENS_SH_MAP_TILE, 3, 4, [0.0; 4]);
    assert_eq!((r, fl[0]), (Some(1), 7.0));
    let mut f = Fake::default();
    f.answer("view_left", &[1]).answer("view_bottom", &[4]);
    assert_eq!(run(&mut f, EE_SCREENS_SH_VIEW_RECT, 0, 0, [0.0; 4]).1, [1.0, 0.0, 0.0, 4.0]);
    let mut f = Fake::default();
    f.answer("mouse_x", &[9]);
    assert_eq!(run(&mut f, EE_SCREENS_SH_MOUSE_AT, 1, 0, [0.0; 4]).1[0], 9.0);
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_SH_MOUSE_AT, 0, 0, [0.0; 4]);
    assert_eq!(f.calls(), "shell_mouse_x(128)\nshell_mouse_y(128)");
}
