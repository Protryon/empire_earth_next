//! backgrounds_calls.rs over the recording fake: each body's C++ calls, in the C++ order.

use super::*;
use crate::screens::game_cpp::Fake;

const BG: usize = 0xa0;

#[test]
fn the_frame_piece_reorders_its_coordinates() {
    let mut f = Fake::default();
    f.answer("gb_bitmap", &[5]).answer("gb_screen", &[0]);
    let p = EeScreensFramePiece {
        texture: [1.0, 2.0, 3.0, 4.0],
        destination: [5.0, 6.0, 7.0, 8.0],
        location: [9.0, 10.0],
        cell: 2,
        orders: [1 | 2 << 2 | 3 << 4, EE_SCREENS_FRAME_KEEP, 3 << 6],
        swap_scale: true,
    };
    piece(&mut f, BG, &p);
    assert_eq!(
        f.calls(),
        "gb_bitmap(160)\ngb_screen(160)\ngb_stretch(5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 2, 0)\n\
         gb_coordinates_load(5)\ngb_coordinates_set(5, 1, 2, 3, 0)\ngb_swap_scale(5)\ngb_coordinates_load(5)\n\
         gb_coordinates_set(5, 0, 0, 0, 3)\ngb_bitmap_at(5, 9.0, 10.0)\ngb_draw(160, 5, 0)"
    );
}

#[test]
fn open_scales_to_the_window() {
    let mut f = Fake::default();
    f.answer("gb_window_width", &[1024]).answer("gb_designed_width", &[512]);
    f.answer("gb_window_height", &[768]).answer("gb_designed_height", &[384]);
    open(&mut f, BG);
    assert!(f.calls().contains("gb_set_scale_x(160, 2.0)"));
    assert!(f.calls().ends_with("gb_set_scale_y(160, 2.0)"));
}

#[test]
fn the_map_cut_into_cells() {
    let mut f = Fake::default();
    f.answer("mb_open_save_game_file", &[0]).answer("mb_is_png", &[1]).answer("mb_width", &[300]);
    f.answer("mb_height", &[256]).answer("mb_make_texture", &[7, 0]).answer("mb_image", &[1]).answer("mb_file", &[1]);
    assert_eq!(load_map(&mut f, BG), 0);
    let calls = f.calls();
    assert!(calls.contains("mb_attributes_png()\nmb_create_image(0)\nmb_load_png()\nmb_width()\nmb_height()"));
    assert!(calls.contains("mb_columns_rows(160, 2, 1)"));
    assert!(calls.contains("mb_texture_rectangle(160, 299.0, 255.0)"));
    assert!(calls.contains(
        "mb_source(256.0, 0.0, 300.0, 256.0)
mb_destination(44.0, 256.0)
mb_copy()"
    ));
    assert!(calls.contains("mb_cell_texture_set(160, 7, 0, 0)"));
    assert!(calls.ends_with("mb_image()\nmb_image_dereference()\nmb_file()\nmb_file_delete()"));
    // a file found on neither path: its error, nothing loaded or released
    let mut f = Fake::default();
    f.answer("mb_open_save_game_file", &[3]).answer("mb_path_find", &[4]);
    assert_eq!(load_map(&mut f, BG), 4);
    assert!(f.calls().ends_with("mb_path_find(0, 160)\nmb_path_dereference(0)"));
    // the attributes' error skips the image
    let mut f = Fake::default();
    f.answer("mb_attributes_jpg", &[2]).answer("mb_file", &[1]);
    assert_eq!(load_map(&mut f, BG), 2);
    assert!(f.calls().ends_with("mb_is_png()\nmb_attributes_jpg()\nmb_file()\nmb_file_delete()"));
}

#[test]
fn construct_and_fill() {
    let mut f = Fake::default();
    construct(&mut f, BG, 0.5, 3, 7, 1.0, 2.0);
    assert_eq!(
        f.calls(),
        "gb_set_opacity(160, 0.5)\ngb_set_flags(160, 3)\ngb_set_color(160, 7)\ngb_set_scale_x(160, 1.0)\ngb_set_scale_y(160, 2.0)"
    );
    let mut f = Fake::default();
    assert_eq!(fill(&mut f, BG, [1.0, 2.0, 3.0, 4.0]), 0);
    assert_eq!(
        f.calls(),
        "gb_rasterizer(160)
gb_screen(160)
gb_color(160)
gb_opacity(160)"
    );
}
