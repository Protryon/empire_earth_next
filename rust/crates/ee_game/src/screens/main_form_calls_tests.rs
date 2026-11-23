//! main_form_calls.rs over the recording fake: each moved case's C++ calls, in the C++ case's order.

use super::*;
use crate::screens::game_cpp::Fake;

const REC: usize = 0x40;

fn run(f: &mut Fake, op: u32, a: i64, b: i64) -> (Option<i64>, EeScreensMainFormIo) {
    let mut io = EeScreensMainFormIo { f: [1.0, 2.0, 3.0, 4.0], n: [7, 8] };
    let r = call(f, REC, op, a, b, &mut io);
    (r, io)
}

#[test]
fn not_here() {
    assert_eq!(run(&mut Fake::default(), 0xffff_fff0, 0, 0).0, None);
}

#[test]
fn the_mouse_modes() {
    let mut f = Fake::default();
    f.answer("mouse_left_down", &[1]).answer("mouse_right_just_unpressed", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_MD_BUTTONS, 0, 0).0, Some(2 | 32));
    assert_eq!(f.log.len(), 6);
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_MF_OP_MD_UI_STATE, 3, 0);
    run(&mut f, EE_SCREENS_MF_OP_MD_UI_STATE, 3, 1);
    assert_eq!(f.calls(), "mf_set_ui_state(64, 3)\nmf_has_wall(64)\nmf_gate_cancel(64)");
    let mut f = Fake::default();
    f.answer("picked_owner_hostile", &[1]).answer("picked_seen_by_current", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_MD_UNIT, EE_SCREENS_MF_UNIT_HOSTILE_SEEN, 0).0, Some(1));
    assert_eq!(run(&mut Fake::default(), EE_SCREENS_MF_OP_MD_UNIT, 0, 0).0, Some(0));
    let mut f = Fake::default();
    f.answer("mf_first_selected", &[1]).answer("mf_has_first", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_MD_FIRST, 0, 0).0, Some(3));
    assert_eq!(f.calls(), "mf_first_clear(64)\nmf_first_selected(64)\nmf_has_first(64)");
    let mut f = Fake::default();
    f.answer("first_distance_squared", &[16]).answer("first_min_range", &[4]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_MD_FIRST_FIELD, EE_SCREENS_MF_FIRST_IN_RANGE, 0).0, Some(1));
    assert_eq!(f.log[0], "first_distance_squared(64, 1.0, 2.0, 3.0)");
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_MF_OP_MD_EVENT, 5, 0);
    assert_eq!(f.calls(), "player_add_event(7, 8, 5)");
    let mut f = Fake::default();
    f.answer("ghost_count", &[3]).answer("ghost_tile_y", &[-2]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_MD_GHOST, -1, 0).0, Some(3));
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_MD_GHOST, 1, EE_SCREENS_MF_GHOST_TILE_Y).0, Some(-2));
    let mut f = Fake::default();
    f.answer("key_exists", &[1]).answer("key_just_pressed", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_MD_KEY, 42, 0).0, Some(5));
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_MD_KEY, 42, 0).0, Some(0));
    let mut f = Fake::default();
    f.answer("end_cinematic_event_new", &[9]);
    run(&mut f, EE_SCREENS_MF_OP_MD_CINEMATIC, EE_SCREENS_MF_CINE_END, 0);
    assert_eq!(f.calls(), "end_cinematic_event_new()\npost_event(9)\nevent_dereference(9)");
    let mut f = Fake::default();
    let (_, io) = {
        f.out("gate_preview", 3);
        run(&mut f, EE_SCREENS_MF_OP_MD_GATE_PREVIEW, 0, 0)
    };
    assert_eq!(f.calls(), "mf_gate_create_ghost(64)\ngate_preview(1.0, 2.0, out)");
    assert_eq!(io.n[0], 3);
    let mut f = Fake::default();
    f.answer("mf_mouse_x", &[5]).answer("mf_mouse_mode", &[4]);
    let (r, io) = run(&mut f, EE_SCREENS_MF_OP_MD_MOUSE_AT, 0, 0);
    assert_eq!((r, io.f[0], io.f[1]), (Some(4), 5.0, 0.0));
}

#[test]
fn the_form() {
    let mut f = Fake::default();
    f.answer("mf_pick", &[5]).answer("mf_picked_is_wall", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_PICK, 0, 0).0, Some(2));
    assert_eq!(run(&mut Fake::default(), EE_SCREENS_MF_OP_PICK, 0, 0).0, Some(0));
    let mut f = Fake::default();
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_WANTS_REFRESH, 0).0, Some(0));
    run(&mut f, EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_SELECT, 0);
    assert_eq!(f.calls(), "mf_current_group(64)\nmf_group_wants_refresh(64)\nmf_current_group(64)");
    // a Rust kind (the statistics group): Refresh and Select are its, Reset and WantsRefresh nothing
    let mut f = Fake::default();
    f.answer("mf_current_group", &[1, 1, 1, 1]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_WANTS_REFRESH, 0).0, Some(0));
    run(&mut f, EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_RESET, 0);
    run(&mut f, EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_REFRESH, 0);
    run(&mut f, EE_SCREENS_MF_OP_GROUP, EE_SCREENS_MF_GROUP_SELECT, 0);
    assert_eq!(
        f.calls(),
        "mf_current_group(64)\nmf_current_group(64)\nmf_current_group(64)\npanel_group(64, 1, 1)\nmf_current_group(64)\npanel_group(64, 1, 2)"
    );
    let mut f = Fake::default();
    f.answer("unit_class_count", &[10]).answer("unit_class_exists", &[1]);
    f.answer("mf_info_unit_type", &[4]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_INFO_HAS_BUTTON, 0, 0).0, Some(0));
    assert_eq!(
        f.calls(),
        "mf_info_unit_type(64)\nunit_class_count()\nunit_class_exists(4)\nplayer_class_has_button(4)"
    );
    let mut f = Fake::default();
    f.answer("mf_control", &[30]).answer("bitmap_new", &[31]);
    run(&mut f, EE_SCREENS_MF_OP_INFO_PORTRAIT, 0, 0);
    assert_eq!(
        f.calls(),
        "mf_control(64, 223)\nbitmap_new()\nmf_info_set_face(64, 31, 30)\nui_bitmap_set_bitmap(30, 31)\n\
         control_set_display_mode(30, 1)\ncontrol_refresh_opacity(30)\nbitmap_dereference(31)"
    );
    let mut f = Fake::default();
    f.answer("screen_event_new", &[50, 51]);
    let mut io = EeScreensMainFormIo { n: [(EE_SCREENS_MF_NO_EVENT as i64) | (3 << 32), 4], ..Default::default() };
    call(&mut f, REC, EE_SCREENS_MF_OP_ADD_HOT_KEY, 0x0004_0003_0002_0001, (1 << 32) | 9, &mut io);
    assert_eq!(
        f.calls(),
        "screen_event_new(3)\nscreen_event_new(4)\nscreen_event_new(0)\nmf_add_hot_key(64, 1, 2, 3, 4, 0, 50, 51, 0, 9, true)"
    );
    let mut f = Fake::default();
    f.answer("mf_unit", &[5]).answer("unit_has_complex", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_UNIT, 3, 0).0, Some(2));
    assert_eq!(f.calls(), "mf_unit_load(64, 3)\nmf_unit(64)\nunit_has_complex(5)");
    let mut f = Fake::default();
    f.answer("selected_count", &[2]).answer("mf_has_caster", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_CASTER, 0, 0).0, Some(1));
    assert_eq!(f.calls(), "selected_count()\nmf_caster_first(64)\nmf_has_caster(64)");
    let mut f = Fake::default();
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_ICON_BAR_UPDATE, 0, 0).0, Some(0));
    let mut f = Fake::default();
    f.answer("mf_control", &[6]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_SAVED_GROUP_REFRESH, 12, 0).0, Some(1));
    assert_eq!(f.calls(), format!("mf_control(64, 12)\nshell_op(6, {})", EE_SCREENS_SKOP_SG_REFRESH));
    let mut f = Fake::default();
    let (_, io) = {
        f.out("team_wonder_countdown", 99).answer("team_wonder_countdown", &[77]);
        run(&mut f, EE_SCREENS_MF_OP_COUNTDOWN, 1, 0)
    };
    assert_eq!(io.n[0], 99);
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_MF_OP_MOUSE_RESET, 0, 0);
    assert_eq!(f.calls(), "mouse_shift_off()\nmouse_set_pointer(0)\nmouse_set_bounds(1.0, 2.0, 3.0, 4.0)");
    let mut f = Fake::default();
    f.answer("graphics", &[1]);
    run(&mut f, EE_SCREENS_MF_OP_GRAPHICS_UI, 0, 0);
    assert_eq!(f.calls(), "graphics()\ngfx_clear_render_objects()\ngfx_push_ui_render_object()");
}

#[test]
fn the_rest_of_the_host_round_9() {
    // a field read and set; the gate's wall search keeps its direction in io.n[0]
    let mut f = Fake::default();
    f.answer("mf_wall_state", &[6]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_SET_FIELD, EE_SCREENS_MF_FIELD_WALL_STATE, 6).0, Some(6));
    assert_eq!(f.calls(), "mf_set_wall_state(64, 6)\nmf_wall_state(64)");
    let mut f = Fake::default();
    f.answer("mf_find_gate_wall", &[1]).out("mf_find_gate_wall", 3);
    let (r, io) = run(&mut f, EE_SCREENS_MF_OP_FIND_GATE_WALL, 10, 11);
    assert_eq!((r, io.n[0]), (Some(1), 3));
    assert_eq!(f.calls(), "mf_tile_x(10)\nmf_tile_y(11)\nmf_find_gate_wall(out)");
    // the wall's preview: each tile, then the running placement's wall or the form's
    let tiles: [[i32; 2]; 2] = [[1, 2], [3, 4]];
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_MF_OP_PREVIEW_WALL, 2, tiles.as_ptr() as i64);
    assert_eq!(
        f.calls(),
        "mf_tiles_clear()\nmf_tile_x(1)\nmf_tile_y(2)\nmf_tiles_add()\nmf_tile_x(3)\nmf_tile_y(4)\nmf_tiles_add()\n\
         mf_has_wall(64)\nmf_preview_wall_form(64)"
    );
    // a node, the produce check, the media
    let mut f = Fake::default();
    f.answer("mf_has_node", &[1, 1]).answer("mf_node_class_exists", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_NODE, 5, 0).0, Some(1));
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_NODE_CLASS, 0, 0).0, Some(1));
    assert_eq!(f.calls(), "mf_node_load(64, 5)\nmf_has_node(64)\nmf_has_node(64)\nmf_node_class_exists(64)");
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_MF_OP_BUILD_TERMINATE, 1, 0);
    run(&mut f, EE_SCREENS_MF_OP_TEXT_WONDER, 9, 30);
    assert_eq!(f.calls(), "mf_gate_terminate(64)\nmf_wonder_format()\nmf_wonder_print(64, 9, 30)");
}

#[test]
fn the_mouse_modes_round_9() {
    // the point: the map point into io.f, or the tile into io.n (from -10000)
    let mut f = Fake::default();
    f.answer("md_point", &[1]).answer("md_point_x", &[5]).answer("md_point_z", &[2]);
    let (r, io) = run(&mut f, EE_SCREENS_MF_OP_MD_POINT, 0, 0);
    assert_eq!((r, io.f[0], io.f[1], io.f[2]), (Some(1), 5.0, 0.0, 2.0));
    assert!(f.calls().starts_with("md_point_reset()\nmd_point(64, 1.0, 2.0)"));
    let mut f = Fake::default();
    let (_, io) = run(&mut f, EE_SCREENS_MF_OP_MD_POINT, 1, 0);
    assert_eq!(io.n, [0, 0]);
    assert_eq!(f.calls(), "md_point_xy(64, 1.0, 2.0, out, out)");
    // the convert check's two bits, the sends, the order at the cursor
    let mut f = Fake::default();
    f.answer("md_can_convert", &[1]).answer("md_in_range", &[1]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_MD_CAN_CONVERT, 2, 1).0, Some(3));
    let mut f = Fake::default();
    run(&mut f, EE_SCREENS_MF_OP_MD_SEND, EE_SCREENS_MF_SEND_UNLOAD, 0);
    run(&mut f, EE_SCREENS_MF_OP_MD_SEND, 99, 0);
    run(&mut f, EE_SCREENS_MF_OP_MD_ORDER_AT, 2, 0);
    run(&mut f, EE_SCREENS_MF_OP_MD_CALAMITY_EFFECT, 0, 0);
    assert_eq!(
        f.calls(),
        "md_send_unload(64, 7, 8)\nmd_send_rally(64, 1.0, 2.0, 7, 8)\nmd_has_picked(64)\nmd_order_at_none(1.0, 2.0)\n\
         md_calamity_effect(64, 1.0, 2.0, 3.0)"
    );
    let mut f = Fake::default();
    f.answer("md_damage", &[2]);
    assert_eq!(run(&mut f, EE_SCREENS_MF_OP_MD_DAMAGE, 1, 2).1.f[0], 2.0);
    for op in [
        EE_SCREENS_MF_OP_MD_CAPTURED,
        EE_SCREENS_MF_OP_MD_POINTER,
        EE_SCREENS_MF_OP_MD_TILE_SEEN,
        EE_SCREENS_MF_OP_MD_PICK,
        EE_SCREENS_MF_OP_MD_PREVIEW,
        EE_SCREENS_MF_OP_MD_CALAMITY_CHECK,
        EE_SCREENS_MF_OP_MD_IN_GAME_VIEW,
        EE_SCREENS_MF_OP_MD_ENTITY_ENABLED,
        EE_SCREENS_MF_OP_MD_SET_ENTITY_GROUP,
        EE_SCREENS_MF_OP_MD_GATE_CLEAR,
        EE_SCREENS_MF_OP_POLL,
        EE_SCREENS_MF_OP_LOG_LINE,
        EE_SCREENS_MF_OP_SCROLL,
        EE_SCREENS_MF_OP_POPULATION,
        EE_SCREENS_MF_OP_LAG_COLOR,
        EE_SCREENS_MF_OP_PLAY_SOUND,
        EE_SCREENS_MF_OP_MEDIA,
        EE_SCREENS_MF_OP_MEDIA_OLD,
        EE_SCREENS_MF_OP_BG_TEXTURE,
        EE_SCREENS_MF_OP_CALAMITY_CLASS,
        EE_SCREENS_MF_OP_POST_SHOW_DIALOG,
    ] {
        let mut f = Fake::default();
        assert!(run(&mut f, op, 1, 2).0.is_some(), "{op}");
        assert!(!f.log.is_empty(), "{op}");
    }
}
