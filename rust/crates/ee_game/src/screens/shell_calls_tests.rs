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

#[test]
fn the_rest_of_the_shells_round_9() {
    // the listed files, their extensions cut; the numbered lines
    let mut f = Fake::default();
    f.answer("sh_files_count", &[2]);
    run(&mut f, EE_SCREENS_SH_FILE_APPEND_LISTED, 0x10, 0x20, [0.0; 4]);
    assert_eq!(
        f.calls(),
        "sh_files_clear()\nsh_files_list(16, 32)\nsh_files_snapshot()\nsh_files_count()\nsh_text_from_file(0)\n\
         sh_text_trim4()\nsh_combo_append_text(128)\nsh_text_from_file(1)\nsh_text_trim4()\nsh_combo_append_text(128)"
    );
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_SH_APPEND_NUMBERED, 7, 2, [0.0; 4]);
    assert!(f.calls().ends_with("sh_numbered_print(2)\nsh_combo_append_text(128)"));
    // the selections: -1 without one
    let mut f = Fake::default();
    f.answer("sh_form_list_selection", &[1]).answer("sh_list_current", &[1, 1]);
    assert_eq!(run(&mut f, EE_SCREENS_SH_LIST_SELECTION, 3, 0, [0.0; 4]).0, Some(-1));
    assert_eq!(run(&mut f, EE_SCREENS_SH_LIST_CURRENT, 0, 0, [0.0; 4]).0, Some(-1));
    assert_eq!(run(&mut f, EE_SCREENS_SH_LIST_CURRENT, 1, 0, [0.0; 4]).0, Some(0));
    // the tech tree of the editor's player: its selection + 1
    let mut f = Fake::default();
    f.out("sh_editor_player", 2).answer("sh_player_tree", &[0x90]).answer("sh_tree_data_id", &[44]);
    run(&mut f, EE_SCREENS_SH_TECH_DEPENDS, (5i64 << 32) | 6, 1, [0.0; 4]);
    assert_eq!(
        f.calls(),
        "sh_editor_player(out)\nsh_player_tree(2)\nsh_tree_data_id(144, 5)\nsh_tree_depends(144, 6, 44)"
    );
    // the rotation: the turning ghosts only, inside the lock
    let mut f = Fake::default();
    f.answer("sh_ghost_count", &[2]).answer("sh_ghost_turns", &[0, 1]);
    run(&mut f, EE_SCREENS_SH_ROTATE, 1, 0, [0.5, 0.0, 0.0, 0.0]);
    assert_eq!(
        f.calls(),
        "sh_ghosts_enter()\nsh_ghost_count()\nsh_ghost_turns(0)\nsh_ghost_turns(1)\nsh_tr_from_ghost(1)\n\
         sh_tr_orient(0.5)\nsh_tr_to_ghost(1)\nsh_ghosts_leave()"
    );
    let mut f = Fake::default();
    f.answer("sh_editor_selected_count", &[1]).answer("sh_editor_selected", &[0x70]);
    f.answer("sh_unit_has_complex", &[1]).answer("sh_unit_turns", &[1]);
    run(&mut f, EE_SCREENS_SH_ROTATE, 0, 0, [0.25, 0.0, 0.0, 0.0]);
    assert!(f.calls().ends_with("sh_tr_from_unit(112)\nsh_tr_orient(0.25)\nsh_tr_to_unit(112)"));
    // the keys: GetEntity's result << 8 | the press state
    let mut f = Fake::default();
    f.answer("sh_key_load", &[1]).answer("sh_key_just_unpressed", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_SH_KEY, 1, 0, [0.0; 4]).0, Some(0x102));
    let mut f = Fake::default();
    f.answer("sh_key_value", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_SH_SPACE, 0, 0, [0.0; 4]).0, Some(1));
    assert_eq!(f.calls(), "sh_key_load(2)\nsh_key_value()");
    // the list line: 4,499 bytes at most, zero padded
    let mut f = Fake::default();
    f.text("sh_list_data", b"abc");
    let mut line = [0xffu8; 4500];
    run(&mut f, EE_SCREENS_SH_LIST_LINE, 0, line.as_mut_ptr() as i64, [0.0; 4]);
    assert_eq!((&line[..4], line[4499]), (&b"abc\0"[..], 0));
    let mut out = [0.0f32; 6];
    let mut f = Fake::default();
    f.answer("sh_line_direction_z", &[3]);
    run(&mut f, EE_SCREENS_SH_SCREEN_LINE, out.as_mut_ptr() as i64, 0, [1.0, 2.0, 0.0, 0.0]);
    assert_eq!(out[5], 3.0);
    for op in [
        EE_SCREENS_SH_CLEAR_RADIO_BITMAPS,
        EE_SCREENS_SH_EDITOR_PLAYER,
        EE_SCREENS_SH_TECH_TREE_MOVE,
        EE_SCREENS_SH_LIST_OF_COMBO_SELECTION,
        EE_SCREENS_SH_NEW_BITMAP,
        EE_SCREENS_SH_BITMAP_COORDS,
        EE_SCREENS_SH_TEXTURE_SIZE,
        EE_SCREENS_SH_SELECT_TAB,
        EE_SCREENS_SH_FORM_CHECKED,
        EE_SCREENS_SH_TECH_NAME_ID,
        EE_SCREENS_SH_TECH_OBSOLETE,
        EE_SCREENS_SH_TECH_OBSOLETED,
        EE_SCREENS_SH_TECH_MARK_PLAYER,
        EE_SCREENS_SH_UNIT_CLASS,
        EE_SCREENS_SH_LIST_INSERT_BYTES,
        EE_SCREENS_SH_UNIT_NAME_AND_ID,
        EE_SCREENS_SH_MOUSE_BUTTONS,
        EE_SCREENS_SH_SLIDER_OF,
        EE_SCREENS_SH_FILL,
        EE_SCREENS_SH_MAP_CREATE,
    ] {
        let mut f = Fake::default();
        assert!(run(&mut f, op, 1, 2, [0.0; 4]).0.is_some(), "{op}");
    }
    let rect = [0i32; 4];
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_SH_PAINT_AREA, 0x50, rect.as_ptr() as i64, [0.0; 4]);
    assert_eq!(f.calls(), "sh_paint_area(80, 0, 0, 0, 0)");
}
