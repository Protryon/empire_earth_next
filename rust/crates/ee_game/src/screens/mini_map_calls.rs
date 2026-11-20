//! The mini map control's C++ bodies in Rust over one-statement C++ shims (UI lane round 8; the shims are
//! `ee_screenscpp_mm_*` in `User Interface Thread/EEUIMiniMap.cpp`): the constructor's body, the destructor,
//! `CopyData`, `LoadMedia`, `UnloadMedia`, `DrawPolygons`, `MouseRButtonDown`, `CenterCamera`, `SetLocation`,
//! `Update` and the `RustOp` cases. The numbers stay `mini_map.rs`' and ee_render's `gfx::minimap`. `map` is an
//! `EEUIMiniMap*`.

use ee_base::math::types::{Ge2DPoint, GeRectangle};

use crate::screens::game_cpp::{Cpp, EeScreensShimText};
use crate::screens::mini_map::*;

/// The three bitmaps (`kMMBitmaps`): the terrain, the units (twice the scale), the flare.
const MM_MAP: u32 = 0;
const MM_WORLD: u32 = 1;
const MM_FLARE: u32 = 2;

/// A transformation's 2D entries (m00 m01 m03 m10 m11 m13), as `EEUIMiniMapMatrix` took them.
const MM_ENTRIES: [(u32, u32); 6] = [(0, 0), (0, 1), (0, 3), (1, 0), (1, 1), (1, 3)];

/// The constructor's body (`rect` its `GERectangle*`, `border` its border thickness): the click action, the map's
/// origin inside the border, no lines, no flare.
pub(crate) fn construct(c: &mut impl Cpp, map: usize, rect: usize, border: u32) {
    c.mm_action_new(map);
    let border = border as f32;
    let (x, y) = (c.mm_rect_left(rect) + border, c.mm_rect_top(rect) + border);
    c.mm_set_origin(map, x, y);
    c.mm_set_lines(map, false);
    c.mm_set_show_flare(map, false);
    c.mm_set_flare_blink(map, true);
}

/// The destructor's body: the click action released.
pub(crate) fn destroy(c: &mut impl Cpp, map: usize) {
    if c.mm_action(map) != 0 {
        c.mm_action_dereference(map);
    }
}

/// `CopyData`: the units texture refreshed, the map scaled to the window, the camera outline and the blinking flare
/// placed (`mini_map.rs` copy_data's numbers), then the base's copy.
pub(crate) fn copy_data(c: &mut impl Cpp, map: usize) -> i64 {
    let mut copy = EeScreensMiniMapCopy::default();
    let rect = c.mm_map_rect(map);
    copy.flare_on = c.mm_flare_on() as u8;
    c.mm_update_world_texture();
    for i in 0..4 {
        copy.corners[i as usize] = [c.mm_corner_x(i), c.mm_corner_y(i)];
    }
    copy.rect = [c.mm_rect_left(rect), c.mm_rect_top(rect), c.mm_rect_right(rect), c.mm_rect_bottom(rect)];
    copy.window_width = c.mm_window_width();
    copy.window_height = c.mm_window_height();
    copy.origin = [c.mm_origin_x(map), c.mm_origin_y(map)];
    copy.flare_point = [c.mm_flare_point_x(), c.mm_flare_point_y()];
    for (k, (row, column)) in MM_ENTRIES.into_iter().enumerate() {
        copy.map_to_pixel[k] = c.mm_map_to_pixel(row, column);
    }
    let texture = c.mm_flare_texture();
    if texture != 0 {
        copy.flare_texture_height = c.mm_texture_height(texture);
        copy.flare_texture_width = c.mm_texture_width(texture);
    }
    copy.flare_location = [c.mm_flare_location_x(), c.mm_flare_location_y()];
    copy.flare_width = c.mm_flare_width();
    copy.free_camera = c.mm_free_camera() as u8;
    copy.show_flare = c.mm_show_flare(map) as u8;
    copy.flare_blink = c.mm_flare_blink(map) as u8;
    crate::screens::mini_map::copy_data(&mut copy);
    c.mm_bitmap_at_origin(MM_MAP, map);
    c.mm_bitmap_at_origin(MM_WORLD, map);
    c.mm_bitmap_scale(MM_MAP, copy.map_scale[0], copy.map_scale[1]);
    c.mm_bitmap_scale(MM_WORLD, copy.world_scale[0], copy.world_scale[1]);
    c.mm_set_lines(map, true);
    c.mm_set_show_flare(map, copy.show_flare != 0);
    c.mm_set_flare_blink(map, copy.flare_blink != 0);
    if copy.flare_color != EE_SCREENS_MM_COLOR_NONE {
        c.mm_flare_color(copy.flare_color != EE_SCREENS_MM_COLOR_FLARE);
    }
    if copy.redraw_all != 0 {
        c.mm_redraw_all();
    }
    if copy.set_flare != 0 {
        c.mm_flare_at(copy.flare_new_location[0], copy.flare_new_location[1]);
        c.mm_bitmap_scale(MM_FLARE, copy.flare_scale[0], copy.flare_scale[1]);
    }
    for (i, [start, end]) in copy.lines.into_iter().enumerate() {
        c.mm_line_start(map, i as u32, start[0], start[1]);
        c.mm_line_end(map, i as u32, end[0], end[1]);
    }
    c.mm_base_copy_data(map)
}

/// `LoadMedia`: the terrain, units and flare bitmaps; the flare texture loaded.
pub(crate) fn load_media(c: &mut impl Cpp) {
    c.mm_set_description();
    for bitmap in [MM_MAP, MM_WORLD, MM_FLARE] {
        c.mm_bitmap_new(bitmap);
    }
    let description = c.mm_description();
    let texture = c.texture_load(EeScreensShimText::of(b"textures\\Flare"), description);
    c.texture_alpha_modulate(texture);
    c.texture_color_modulate(texture);
    c.texture_min_filter(texture, 1);
    c.texture_mag_filter(texture, 1);
    c.texture_mip_point(texture);
    c.texture_mip_bias(texture, 0.0);
    c.texture_wrap(texture);
    c.mm_flare_texture_set(texture);
}

/// `UnloadMedia`: the three bitmaps released.
pub(crate) fn unload_media(c: &mut impl Cpp) {
    for bitmap in [MM_MAP, MM_WORLD, MM_FLARE] {
        c.mm_bitmap_dereference(bitmap);
        c.mm_bitmap_clear(bitmap);
    }
}

/// `DrawPolygons`: `mini_map.rs`' plan executed in its order; the result is the last bitmap's or line's draw.
pub(crate) fn draw_polygons(c: &mut impl Cpp, map: usize) -> i64 {
    let show_flare = c.mm_show_flare(map);
    let lines = [0, 1, 2, 3].map(|i| c.mm_line_drawn(map, i));
    let drawn = c.mm_drawn_rect(map);
    let rect = [c.mm_rect_left(drawn), c.mm_rect_top(drawn), c.mm_rect_right(drawn), c.mm_rect_bottom(drawn)];
    let plan = draw_plan(show_flare, lines, rect);
    let mut result = 0;
    for &step in &plan.steps[..plan.count as usize] {
        match step {
            EE_SCREENS_MM_DRAW_ENTER => c.mm_terrain_enter(),
            EE_SCREENS_MM_DRAW_BASE => {
                c.mm_base_draw(map);
            }
            EE_SCREENS_MM_DRAW_MAP => {
                c.mm_draw_bitmap(map, MM_MAP);
            }
            EE_SCREENS_MM_DRAW_WORLD => result = c.mm_draw_bitmap(map, MM_WORLD),
            EE_SCREENS_MM_DRAW_FLARE => result = c.mm_draw_bitmap(map, MM_FLARE),
            EE_SCREENS_MM_DRAW_CLIP | EE_SCREENS_MM_DRAW_RESTORE => {
                if step == EE_SCREENS_MM_DRAW_CLIP {
                    c.mm_save_view_rect(map);
                }
                c.mm_viewport_load(map);
                if step == EE_SCREENS_MM_DRAW_CLIP {
                    c.mm_viewport_clip(plan.clip[0], plan.clip[1], plan.clip[2], plan.clip[3]);
                } else {
                    c.mm_viewport_saved();
                }
                c.mm_viewport_apply(map);
            }
            EE_SCREENS_MM_DRAW_LEAVE => c.mm_terrain_leave(),
            _ => result = c.mm_draw_line(map, (step - EE_SCREENS_MM_DRAW_LINE) as u32),
        }
    }
    result
}

/// The mouse at the map (`form`: the form's mouse, else the control's), its origin, the window and the bounds.
fn mm_point(c: &mut impl Cpp, map: usize, press: bool) -> Ge2DPoint {
    let mouse = Ge2DPoint { x: c.mm_mouse_x(map), y: c.mm_mouse_y(map) };
    let origin = Ge2DPoint { x: c.mm_origin_x(map), y: c.mm_origin_y(map) };
    let (width, height) = (c.mm_window_width(), c.mm_window_height());
    let b = c.mm_bounds();
    let bounds = GeRectangle {
        left: c.mm_rect_left(b),
        top: c.mm_rect_top(b),
        right: c.mm_rect_right(b),
        bottom: c.mm_rect_bottom(b),
    };
    if press {
        ee_render::gfx::minimap::press_point(mouse, origin, width, height, &bounds)
    } else {
        ee_render::gfx::minimap::center_point(mouse, origin, width, height, &bounds)
    }
}

/// `MouseRButtonDown`: a mini map action (mode 1, the keyboard's modifiers) posted at the clicked map point, the
/// mouse captured.
pub(crate) fn mouse_r_down(c: &mut impl Cpp, map: usize) -> i64 {
    let modifiers = c.mm_modifiers();
    let point = mm_point(c, map, true);
    c.mm_action_mode(map, 1);
    c.mm_action_point(map, point.x, point.y);
    c.mm_action_modifiers(map, modifiers);
    c.mm_post_action(map);
    c.mm_capture(map);
    0
}

/// `CenterCamera`: a mini map action (mode 0) posted at the map point under the mouse.
pub(crate) fn center_camera(c: &mut impl Cpp, map: usize) {
    let point = mm_point(c, map, false);
    c.mm_action_mode(map, 0);
    c.mm_action_point(map, point.x, point.y);
    c.mm_post_action(map);
}

/// `SetLocation`: the control and its map origin (inside the border) moved.
pub(crate) fn set_location(c: &mut impl Cpp, map: usize, x: f32, y: f32) {
    c.mm_base_set_location(map, x, y);
    let border = c.mm_border(map) as f32;
    c.mm_set_origin(map, x + border, y + border);
}

/// `Update`: the bitmaps pointed at a new map's terrain and units textures, the bounds recomputed (`mini_map.rs`
/// update's numbers). `minimap` is the `EEMiniMap*` (none: nothing).
pub(crate) fn update_map(c: &mut impl Cpp, minimap: usize) {
    if minimap == 0 {
        return;
    }
    let mut u = EeScreensMiniMapUpdate {
        width: c.mm_map_width(),
        height: c.mm_map_height(),
        size: c.mm_map_size(),
        ..Default::default()
    };
    for (k, (row, column)) in MM_ENTRIES.into_iter().enumerate() {
        u.map_to_pixel[k] = c.mm_update_map_to_pixel(minimap, row, column);
    }
    update(&mut u);
    if c.mm_bitmap(MM_WORLD) != 0 {
        c.mm_world_coordinates(u.extent);
    }
    c.mm_set_bounds(u.bounds[0], u.bounds[1], u.bounds[2], u.bounds[3]);
    c.mm_set_transformation(minimap);
    if c.mm_bitmap(MM_MAP) != 0 {
        c.mm_terrain_texture_set(minimap);
    }
    if c.mm_bitmap(MM_WORLD) != 0 {
        c.mm_world_texture_set(minimap);
    }
}

/// The `RustOp` cases moved (`op` without `EE_SCREENS_HC_CLASS`; `mouse` the read's record): `None` for the others.
pub(crate) fn rust_op(
    c: &mut impl Cpp,
    map: usize,
    op: u32,
    a: i64,
    b: i64,
    mouse: Option<&mut EeScreensMiniMapMouse>,
) -> Option<i64> {
    match op {
        EE_SCREENS_MM_OP_CENTER => center_camera(c, map),
        EE_SCREENS_MM_OP_READ_MOUSE => {
            let m = mouse?;
            let rect = c.mm_map_rect(map);
            m.mouse = [c.mm_form_mouse_x(map), c.mm_form_mouse_y(map)];
            m.rect = [c.mm_rect_left(rect), c.mm_rect_top(rect), c.mm_rect_right(rect), c.mm_rect_bottom(rect)];
            for (k, (row, column)) in MM_ENTRIES.into_iter().enumerate() {
                m.pixel_to_map[k] = c.mm_pixel_to_map(row, column);
            }
        }
        EE_SCREENS_MM_OP_TILE_EXISTS => return Some(c.mm_tile_exists(a, b) as i64),
        EE_SCREENS_MM_OP_SEND_FLARE => c.mm_send_flare(a, b),
        EE_SCREENS_MM_OP_FLARE_DONE => {
            c.mm_pointer_clear();
            c.mm_form_ui_state_clear(map);
        }
        _ => return None,
    }
    Some(0)
}

#[cfg(not(test))]
mod ffi {
    use crate::screens::game_cpp::Ffi;

    /// `EEUIMiniMap`'s constructor's body (`rect` the `GERectangle*`).
    #[no_mangle]
    pub extern "C" fn ee_screens_mm_construct(map: usize, rect: usize, border: u32) {
        super::construct(&mut Ffi, map, rect, border)
    }
    /// `~EEUIMiniMap`'s body.
    #[no_mangle]
    pub extern "C" fn ee_screens_mm_destroy(map: usize) {
        super::destroy(&mut Ffi, map)
    }
    /// `EEUIMiniMap::CopyData`.
    #[no_mangle]
    pub extern "C" fn ee_screens_mm_copy_data(map: usize) -> i64 {
        super::copy_data(&mut Ffi, map)
    }
    /// `EEUIMiniMap::LoadMedia`.
    #[no_mangle]
    pub extern "C" fn ee_screens_mm_load_media() {
        super::load_media(&mut Ffi)
    }
    /// `EEUIMiniMap::UnloadMedia`.
    #[no_mangle]
    pub extern "C" fn ee_screens_mm_unload_media() {
        super::unload_media(&mut Ffi)
    }
    /// `EEUIMiniMap::DrawPolygons`.
    #[no_mangle]
    pub extern "C" fn ee_screens_mm_draw_polygons(map: usize) -> i64 {
        super::draw_polygons(&mut Ffi, map)
    }
    /// `EEUIMiniMap::MouseRButtonDown`.
    #[no_mangle]
    pub extern "C" fn ee_screens_mm_mouse_r_down(map: usize) -> i64 {
        super::mouse_r_down(&mut Ffi, map)
    }
    /// `EEUIMiniMap::CenterCamera`.
    #[no_mangle]
    pub extern "C" fn ee_screens_mm_center_camera(map: usize) {
        super::center_camera(&mut Ffi, map)
    }
    /// `EEUIMiniMap::SetLocation`.
    #[no_mangle]
    pub extern "C" fn ee_screens_mm_set_location(map: usize, x: f32, y: f32) {
        super::set_location(&mut Ffi, map, x, y)
    }
    /// `EEUIMiniMap::Update` (`minimap` the `EEMiniMap*`).
    #[no_mangle]
    pub extern "C" fn ee_screens_mm_update(minimap: usize) {
        super::update_map(&mut Ffi, minimap)
    }
}

#[cfg(test)]
#[path = "mini_map_calls_tests.rs"]
mod tests;
