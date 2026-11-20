//! mini_map_calls.rs over the recording fake: each body's C++ calls, in the C++ order.

use super::*;
use crate::screens::game_cpp::Fake;

const MAP: usize = 0x90;

#[test]
fn construct_and_destroy() {
    let mut f = Fake::default();
    construct(&mut f, MAP, 5, 2);
    destroy(&mut f, MAP);
    assert_eq!(
        f.calls(),
        "mm_action_new(144)\nmm_rect_left(5)\nmm_rect_top(5)\nmm_set_origin(144, 2.0, 2.0)\nmm_set_lines(144, false)\n\
         mm_set_show_flare(144, false)\nmm_set_flare_blink(144, true)\nmm_action(144)"
    );
}

#[test]
fn copy_data_without_a_flare() {
    let mut f = Fake::default();
    f.answer("mm_window_width", &[800]).answer("mm_window_height", &[600]).answer("mm_flare_blink", &[0]);
    f.answer("mm_base_copy_data", &[3]);
    assert_eq!(copy_data(&mut f, MAP), 3);
    let calls = f.calls();
    assert!(calls.starts_with("mm_map_rect(144)\nmm_flare_on()\nmm_update_world_texture()\nmm_corner_x(0)"));
    assert!(calls.contains(
        "mm_bitmap_at_origin(0, 144)\nmm_bitmap_at_origin(1, 144)\nmm_bitmap_scale(0, 1.0, 1.0)\n\
         mm_bitmap_scale(1, 2.0, 2.0)\nmm_set_lines(144, true)\nmm_set_show_flare(144, false)\n\
         mm_set_flare_blink(144, true)\nmm_line_start(144, 0, 0.0, 0.0)"
    ));
    assert!(!calls.contains("mm_flare_color"));
    assert!(calls.ends_with("mm_line_end(144, 3, 0.0, 0.0)\nmm_base_copy_data(144)"));
}

#[test]
fn the_media() {
    let mut f = Fake::default();
    f.answer("texture_load", &[7]);
    load_media(&mut f);
    unload_media(&mut f);
    assert_eq!(
        f.calls(),
        "mm_set_description()\nmm_bitmap_new(0)\nmm_bitmap_new(1)\nmm_bitmap_new(2)\nmm_description()\n\
         texture_load(\"textures\\\\Flare\", 0)\ntexture_alpha_modulate(7)\ntexture_color_modulate(7)\n\
         texture_min_filter(7, 1)\ntexture_mag_filter(7, 1)\ntexture_mip_point(7)\ntexture_mip_bias(7, 0.0)\n\
         texture_wrap(7)\nmm_flare_texture_set(7)\nmm_bitmap_dereference(0)\nmm_bitmap_clear(0)\n\
         mm_bitmap_dereference(1)\nmm_bitmap_clear(1)\nmm_bitmap_dereference(2)\nmm_bitmap_clear(2)"
    );
}

#[test]
fn draw_follows_the_plan() {
    let mut f = Fake::default();
    f.answer("mm_draw_bitmap", &[0, 9]);
    let plan = draw_plan(false, [false; 4], [0.0; 4]);
    let r = draw_polygons(&mut f, MAP);
    assert_eq!(r, draw_result(&plan));
    assert!(f.calls().starts_with("mm_show_flare(144)\nmm_line_drawn(144, 0)"));
}

/// The plan's result under the fake's answers (the map's draw 0, the world's 9).
fn draw_result(plan: &EeScreensMiniMapDraw) -> i64 {
    let mut r = 0;
    for &s in &plan.steps[..plan.count as usize] {
        if s == EE_SCREENS_MM_DRAW_WORLD {
            r = 9;
        }
    }
    r
}

#[test]
fn the_presses_and_the_ops() {
    let mut f = Fake::default();
    f.answer("mm_window_width", &[800]).answer("mm_window_height", &[600]);
    assert_eq!(mouse_r_down(&mut f, MAP), 0);
    assert!(f.calls().ends_with(
        "mm_action_mode(144, 1)\nmm_action_point(144, 0.0, 0.0)\nmm_action_modifiers(144, 0)\nmm_post_action(144)\n\
         mm_capture(144)"
    ));
    let mut f = Fake::default();
    assert_eq!(rust_op(&mut f, MAP, EE_SCREENS_MM_OP_FLARE_DONE, 0, 0, None), Some(0));
    assert_eq!(rust_op(&mut f, MAP, EE_SCREENS_MM_OP_SEND_FLARE, 3, 4, None), Some(0));
    assert_eq!(rust_op(&mut f, MAP, EE_SCREENS_MM_OP_POINTER, 0, 0, None), None);
    assert_eq!(f.calls(), "mm_pointer_clear()\nmm_form_ui_state_clear(144)\nmm_send_flare(3, 4)");
    let mut m = EeScreensMiniMapMouse::default();
    let mut f = Fake::default();
    f.answer("mm_form_mouse_x", &[0]).answer("mm_pixel_to_map", &[1, 2, 3, 4, 5, 6]);
    rust_op(&mut f, MAP, EE_SCREENS_MM_OP_READ_MOUSE, 0, 0, Some(&mut m));
    assert_eq!(m.pixel_to_map, [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]);
    assert!(f.calls().ends_with("mm_pixel_to_map(1, 3)"));
}

#[test]
fn the_update() {
    let mut f = Fake::default();
    update_map(&mut f, 0);
    assert!(f.log.is_empty());
    f.answer("mm_bitmap", &[1, 0, 1]);
    update_map(&mut f, 5);
    let calls = f.calls();
    assert!(calls.starts_with("mm_map_width()\nmm_map_height()\nmm_map_size()\nmm_update_map_to_pixel(5, 0, 0)"));
    assert!(calls.contains("mm_bitmap(1)\nmm_world_coordinates("));
    assert!(calls.ends_with("mm_set_transformation(5)\nmm_bitmap(0)\nmm_bitmap(1)\nmm_world_texture_set(5)"));
}

#[test]
fn set_location_keeps_the_origin_inside_the_border() {
    let mut f = Fake::default();
    f.answer("mm_border", &[2]);
    set_location(&mut f, MAP, 10.0, 20.0);
    assert_eq!(f.calls(), "mm_base_set_location(144, 10.0, 20.0)
mm_border(144)
mm_set_origin(144, 12.0, 22.0)");
}
