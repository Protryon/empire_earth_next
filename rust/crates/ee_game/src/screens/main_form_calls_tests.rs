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
    assert_eq!(run(&mut Fake::default(), EE_SCREENS_MF_OP_MD_UNIT, 0, 0).0, None);
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
    assert_eq!(f.calls(), "mf_group_wants_refresh(64)\nmf_group_select(64)");
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
