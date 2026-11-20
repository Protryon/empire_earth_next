//! The game's two background classes' C++ bodies in Rust over one-statement C++ shims (UI lane round 8; the shims
//! are `ee_screenscpp_gb_*` and `_mb_*` in `User Interface Thread/EEUIBGenericBackground.cpp`): the framed
//! background's constructor body, its frame host (`frame.rs`' fill and pieces) and `Open`; the campaign map
//! background's `LoadBackground` (the image cut into 256 x 256 textures). `bg` is the background.

use core::ffi::c_long;

use crate::screens::frame::{EeScreensFramePiece, EE_SCREENS_FRAME_KEEP};
use crate::screens::game_cpp::Cpp;

/// The framed background's constructor body (`color` its `GEColor*`).
pub(crate) fn construct(c: &mut impl Cpp, bg: usize, opacity: f32, flags: u32, color: usize, sx: f32, sy: f32) {
    c.gb_set_opacity(bg, opacity);
    c.gb_set_flags(bg, flags);
    c.gb_set_color(bg, color);
    c.gb_set_scale_x(bg, sx);
    c.gb_set_scale_y(bg, sy);
}

/// The frame host's fill: the tinted panel over `inner` (left, top, right, bottom); the draw's result.
pub(crate) fn fill(c: &mut impl Cpp, bg: usize, inner: [f32; 4]) -> c_long {
    let (rasterizer, screen) = (c.gb_rasterizer(bg), c.gb_screen(bg));
    let rect = ee_base::math::types::GeRectangle { left: inner[0], top: inner[1], right: inner[2], bottom: inner[3] };
    let (color, opacity) = (c.gb_color(bg), c.gb_opacity(bg));
    #[cfg(not(test))]
    {
        // SAFETY: the background's live rasterizer and colour (three f32s)
        unsafe {
            ee_render::ge::list2d::ee_ge_draw2d_fill(rasterizer as *mut _, screen as _, &rect, color as *const f32, opacity)
        }
    }
    #[cfg(test)]
    {
        let _ = (rasterizer, screen, rect, color, opacity);
        0
    }
}

/// The frame host's piece: the bitmap stretched from the piece's rectangle onto its visible part with the cell,
/// its texture coordinates reordered (the scales swapped after a quarter turn), placed and drawn.
pub(crate) fn piece(c: &mut impl Cpp, bg: usize, p: &EeScreensFramePiece) {
    let bitmap = c.gb_bitmap(bg);
    let screen = c.gb_screen(bg);
    let (t, d) = (p.texture, p.destination);
    c.gb_stretch(bitmap, t[0], t[1], t[2], t[3], d[0], d[1], d[2], d[3], p.cell, screen);
    for (i, &order) in p.orders.iter().enumerate() {
        if order == EE_SCREENS_FRAME_KEEP {
            continue;
        }
        c.gb_coordinates_load(bitmap);
        c.gb_coordinates_set(bitmap, order & 3, order >> 2 & 3, order >> 4 & 3, order >> 6 & 3);
        if i == 0 && p.swap_scale {
            c.gb_swap_scale(bitmap);
        }
    }
    c.gb_bitmap_at(bitmap, p.location[0], p.location[1]);
    c.gb_draw(bg, bitmap, screen);
}

/// `EEUIBGenericBackground::Open`: the base's open, the frame scaled to the window against the form's designed size.
pub(crate) fn open(c: &mut impl Cpp, bg: usize) {
    c.gb_base_open(bg);
    let sx = c.gb_window_width() / c.gb_designed_width(bg);
    c.gb_set_scale_x(bg, sx);
    let sy = c.gb_window_height() / c.gb_designed_height(bg);
    c.gb_set_scale_y(bg, sy);
}

/// `EEUIBMapBackground::LoadBackground`: the campaign map image (from the saved game's resources, else the startup
/// path; PNG, else JPG) cut into 256 x 256 textures, one per cell; the last step's result.
pub(crate) fn load_map(c: &mut impl Cpp, bg: usize) -> i64 {
    c.mb_file_clear();
    c.mb_image_clear();
    c.mb_directory_reset();
    let secondary = c.mb_secondary_surface();
    let screen = c.mb_screen();
    c.mb_desc_reset();
    c.mb_desc_pixel_format(secondary);
    c.mb_desc_pixel_depth(secondary);
    c.mb_desc_system_memory();
    c.mb_desc_volatility(1);
    c.mb_desc_m14(0);
    c.mb_desc_mip_levels(0);
    c.mb_desc_3d(false);
    c.mb_desc_transparency(false);
    let mut result = c.mb_open_save_game_file(bg);
    if result != 0 {
        c.mb_directory_volume();
        c.mb_directory_directory();
        let path = c.mb_path_new();
        c.mb_path_add(path);
        result = c.mb_path_find(path, bg);
        c.mb_path_dereference(path);
        if result != 0 {
            return result;
        }
    }
    let png = c.mb_is_png();
    result = if png { c.mb_attributes_png() } else { c.mb_attributes_jpg() };
    if result == 0 {
        c.mb_create_image(screen);
        result = if png { c.mb_load_png() } else { c.mb_load_jpg() };
        if result == 0 {
            result = cut_cells(c, bg, screen);
        }
        // the image surface released also when its load failed
        if c.mb_image() {
            c.mb_image_dereference();
        }
    }
    if c.mb_file() {
        c.mb_file_delete();
    }
    result
}

/// The loaded image's 256 x 256 cells as the background's textures; the last cell's result.
fn cut_cells(c: &mut impl Cpp, bg: usize, screen: usize) -> i64 {
    let (width, height) = (c.mb_width(), c.mb_height());
    let columns = (width >> 8) + (width & 0xff != 0) as u32;
    let rows = (height >> 8) + (height & 0xff != 0) as u32;
    c.mb_cell_desc();
    c.mb_cell_texture();
    c.mb_cell_width(256);
    c.mb_cell_height(256);
    c.mb_columns_rows(bg, columns, rows);
    c.mb_texture_description(bg);
    c.mb_texture_rectangle(bg, width.wrapping_sub(1) as f32, height.wrapping_sub(1) as f32);
    let mut result = 0;
    for column in 0..columns {
        let x = column * 256;
        for row in 0..rows {
            let y = row * 256;
            let right = (x + 256).min(width);
            let bottom = (y + 256).min(height);
            result = c.mb_create_cell(screen);
            if result == 0 {
                result = c.mb_copy(x as f32, y as f32, right as f32, bottom as f32, (right - x) as f32, (bottom - y) as f32);
                if result == 0 {
                    let texture = c.mb_make_texture(screen);
                    if texture != 0 {
                        c.mb_cell_texture_set(bg, texture, column, row);
                    }
                }
                c.mb_cell_dereference();
            }
        }
    }
    result
}

#[cfg(not(test))]
mod ffi {
    use crate::screens::frame::EeScreensFramePiece;
    use crate::screens::game_cpp::Ffi;

    /// `EEUIBGenericBackground`'s constructor's body (`color` the `GEColor*`).
    #[no_mangle]
    pub extern "C" fn ee_screens_gb_construct(bg: usize, opacity: f32, flags: u32, color: usize, sx: f32, sy: f32) {
        super::construct(&mut Ffi, bg, opacity, flags, color, sx, sy)
    }
    /// `EEUIBGenericBackground::Open`.
    #[no_mangle]
    pub extern "C" fn ee_screens_gb_open(bg: usize) {
        super::open(&mut Ffi, bg)
    }
    /// `EEUIBMapBackground::LoadBackground`.
    #[no_mangle]
    pub extern "C" fn ee_screens_mb_load(bg: usize) -> i64 {
        super::load_map(&mut Ffi, bg)
    }
    /// The frame host's fill (frame_ffi.rs).
    pub(crate) fn fill(bg: usize, inner: [f32; 4]) -> core::ffi::c_long {
        super::fill(&mut Ffi, bg, inner)
    }
    /// The frame host's piece (frame_ffi.rs).
    pub(crate) fn piece(bg: usize, p: &EeScreensFramePiece) {
        super::piece(&mut Ffi, bg, p)
    }
}
#[cfg(not(test))]
pub(crate) use ffi::{fill as frame_fill, piece as frame_piece};

#[cfg(test)]
#[path = "backgrounds_calls_tests.rs"]
mod tests;
