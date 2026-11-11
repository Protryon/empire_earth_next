//! `extern "C"` for the backgrounds' logic (`User Interface/UI*Background.cpp`, `UIForm.cpp`): each tile goes back
//! to the C++ to draw (`ee_uicpp_background_*`), the moves' and visibility changes' calls too.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_int, c_long, c_ulong, c_void};

use crate::background::{self, FormTileHost, FullHost, MoveHost, TileHost, TilePlan, Tiling};
use crate::rect::UiRect;

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_ulong, c_void};

    use crate::rect::UiRect;

    extern "C" {
        pub fn ee_uicpp_background_draw(
            background: *mut c_void,
            source: *const UiRect,
            destination: *const UiRect,
        ) -> c_long;
        pub fn ee_uicpp_background_full_draw(
            background: *mut c_void,
            index: c_ulong,
            tile: *const UiRect,
            clipped: *const UiRect,
        ) -> c_long;
        pub fn ee_uicpp_background_bfull_draw(
            background: *mut c_void,
            index: c_ulong,
            tile: *const UiRect,
            clipped: *const UiRect,
        ) -> c_long;
        pub fn ee_uicpp_form_tile_draw(form: *mut c_void, bitmap: *mut c_void, piece: *const UiRect, x: f32, y: f32);
        // value: 0 mIsVisible, 1 its form is the current form
        pub fn ee_uicpp_background_value(background: *mut c_void, which: c_int) -> bool;
        pub fn ee_uicpp_ustring_assign(out: *mut c_void, text: *const core::ffi::c_char);
        pub fn ee_uicpp_background_set_visible(background: *mut c_void, visible: bool);
        pub fn ee_uicpp_background_rect(background: *mut c_void) -> UiRect;
        pub fn ee_uicpp_background_set_rect(background: *mut c_void, r: *const UiRect);
        // call: 0 the form's LockDrawBackground, 1 UnlockDrawBackground, 2 RedrawIntersectControls(r),
        // 3 the manager's AddDirtyRectangle(r, form)
        pub fn ee_uicpp_background_call(background: *mut c_void, call: c_int, r: *const UiRect);
    }
}

struct Raw(*mut c_void);

impl TileHost for Raw {
    fn draw(&mut self, source: UiRect, destination: UiRect) -> c_long {
        unsafe { sys::ee_uicpp_background_draw(self.0, &source, &destination) }
    }
}

impl FullHost for Raw {
    fn draw(&mut self, index: c_ulong, tile: UiRect, clipped: UiRect) -> c_long {
        unsafe { sys::ee_uicpp_background_full_draw(self.0, index, &tile, &clipped) }
    }
}

/// A `UIBFull` (its tiles go back to `UIBFull.cpp`).
struct BFull(*mut c_void);

impl FullHost for BFull {
    fn draw(&mut self, index: c_ulong, tile: UiRect, clipped: UiRect) -> c_long {
        unsafe { sys::ee_uicpp_background_bfull_draw(self.0, index, &tile, &clipped) }
    }
}

impl MoveHost for Raw {
    fn visible(&self) -> bool {
        unsafe { sys::ee_uicpp_background_value(self.0, 0) }
    }
    fn set_visible(&mut self, v: bool) {
        unsafe { sys::ee_uicpp_background_set_visible(self.0, v) }
    }
    fn rect(&self) -> UiRect {
        unsafe { sys::ee_uicpp_background_rect(self.0) }
    }
    fn set_rect(&mut self, r: UiRect) {
        unsafe { sys::ee_uicpp_background_set_rect(self.0, &r) }
    }
    fn form_is_current(&self) -> bool {
        unsafe { sys::ee_uicpp_background_value(self.0, 1) }
    }
    fn lock(&mut self) {
        unsafe { sys::ee_uicpp_background_call(self.0, 0, core::ptr::null()) }
    }
    fn unlock(&mut self) {
        unsafe { sys::ee_uicpp_background_call(self.0, 1, core::ptr::null()) }
    }
    fn redraw(&mut self, r: UiRect) {
        unsafe { sys::ee_uicpp_background_call(self.0, 2, &r) }
    }
    fn dirty(&mut self, r: UiRect) {
        unsafe { sys::ee_uicpp_background_call(self.0, 3, &r) }
    }
}

struct FormTiles {
    form: *mut c_void,
    bitmap: *mut c_void,
}

impl FormTileHost for FormTiles {
    fn draw(&mut self, piece: UiRect, location: (f32, f32)) {
        unsafe { sys::ee_uicpp_form_tile_draw(self.form, self.bitmap, &piece, location.0, location.1) }
    }
}

/// `UIBitmapBackground::DrawBackground(redraw)` (`rect`: the background's) -> the draw's result.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_background_bitmap_draw(background: *mut c_void, rect: UiRect, redraw: UiRect) -> c_long {
    background::bitmap_draw(&mut Raw(background), rect, redraw)
}

/// The tiling classes' `DrawBackground(redraw)` up to `DrawTiledBackground`: `kind` 0 tiled, 1 left-right,
/// 2 top-bottom, 3 stretched (`across` x `down` tiles) -> whether the redraw rectangle meets the background, the
/// plan in `out`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_background_tiled_plan(
    kind: c_int,
    across: f32,
    down: f32,
    rect: UiRect,
    texture: UiRect,
    redraw: UiRect,
    out: *mut TilePlan,
) -> bool {
    let kind = match kind {
        0 => Tiling::Both,
        1 => Tiling::LeftRight,
        2 => Tiling::TopBottom,
        _ => Tiling::Stretched { across, down },
    };
    match background::tiled_plan(kind, rect, texture, redraw) {
        Some(p) => {
            out.write(p);
            true
        }
        None => false,
    }
}

/// `UITiledBackground::DrawTiledBackground(clip, rows, columns, w, h)` (`rect`: the background's).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_background_draw_tiled(
    background: *mut c_void,
    rect: UiRect,
    clip: UiRect,
    first_row: c_ulong,
    last_row: c_ulong,
    first_column: c_ulong,
    last_column: c_ulong,
    w: f32,
    h: f32,
) -> c_long {
    background::draw_tiled(&mut Raw(background), rect, clip, first_row, last_row, first_column, last_column, w, h)
}

/// `UIFullBackground::DrawBackground(redraw)`: the first tile's texture size, the window's, the designed screen's
/// and the grid (columns, rows) -> the last draw's result.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_background_full_draw(
    background: *mut c_void,
    redraw: UiRect,
    texture_width: c_ulong,
    texture_height: c_ulong,
    window_width: c_ulong,
    window_height: c_ulong,
    designed_width: c_ulong,
    designed_height: c_ulong,
    columns: c_ulong,
    rows: c_ulong,
) -> c_long {
    background::full_draw(
        &mut Raw(background),
        redraw,
        (texture_width, texture_height),
        (window_width, window_height),
        (designed_width, designed_height),
        (columns, rows),
    )
}

/// `UIBFull::DrawBackground(redraw)`: the form's rectangle, the first tile's texture size, the texture rectangle
/// and the grid (columns, rows) -> the last draw's result.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_background_bfull_draw(
    background: *mut c_void,
    form: UiRect,
    redraw: UiRect,
    texture_width: c_ulong,
    texture_height: c_ulong,
    texture_rect: UiRect,
    columns: c_ulong,
    rows: c_ulong,
) -> c_long {
    background::bfull_draw(
        &mut BFull(background),
        form,
        redraw,
        (texture_width, texture_height),
        texture_rect,
        (columns, rows),
    )
}

/// The screen rectangle `UIBackground::Open` (`exclusive`: the form's exclusive coordinates) and
/// `RelayoutBackground` (never exclusive) lay a background out in.
#[no_mangle]
pub extern "C" fn ee_ui_background_screen_rect(width: c_ulong, height: c_ulong, exclusive: bool) -> UiRect {
    background::screen_rect(width, height, exclusive)
}

/// `UIBackground::SetRectangle(rect)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_background_set_rectangle(background: *mut c_void, rect: UiRect) {
    background::set_rectangle(&mut Raw(background), rect);
}

/// `UIBackground::SetVisibility(visible)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_background_set_visibility(background: *mut c_void, visible: bool) {
    background::set_visibility(&mut Raw(background), visible);
}

/// `UIForm::DrawBackground(bitmap, rect, draw, tiles)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_draw_tiles(
    form: *mut c_void,
    bitmap: *mut c_void,
    rect: UiRect,
    draw: UiRect,
    tiles: UiRect,
) {
    background::form_tiles(&mut FormTiles { form, bitmap }, rect, draw, tiles);
}

/// `UIFullBackground::LoadBackground`'s tile file name into the `UString` `out`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_background_full_tile_name(
    name: *const core::ffi::c_char,
    row: c_ulong,
    column: c_ulong,
    out: *mut c_void,
) {
    let name = core::ffi::CStr::from_ptr(name).to_bytes_with_nul();
    let tile = background::full_tile_name(name, row, column);
    sys::ee_uicpp_ustring_assign(out, tile.as_ptr().cast());
}
