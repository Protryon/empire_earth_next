//! The UI backgrounds' logic (`User Interface/UIBackground.cpp`, `UIBitmapBackground.cpp`, `UITiledBackground.cpp`,
//! `UILRTiledBackground.cpp`, `UITBTiledBackground.cpp`, `UIStretchedTiledBackground.cpp`, `UIFullBackground.cpp`
//! and `UIForm::DrawBackground`'s tiling): which tiles cover a redraw rectangle and the source and destination
//! rectangles of each, the screen rectangle a background is laid out in, and the moves' and visibility changes'
//! redraws.
//!
//! Each tile is handed to the C++ to draw (a host call: the bitmap's stretch, location and the rasterizer's `Draw`
//! stay C++), in the C++'s order; the last draw's result is returned as the C++ did.

use core::ffi::{c_long, c_ulong};

use crate::form_state::and_in_place;
use crate::rect::{um_floor, UiRect};

/// `GERectangle::And(other, out)`: the overlap, `None` when there is none.
pub fn and(r: &UiRect, other: &UiRect) -> Option<UiRect> {
    let mut out = *r;
    and_in_place(&mut out, other).then_some(out)
}

/// The tile kinds' redraws: the tile (its source) and the part to draw (its destination).
pub trait TileHost {
    /// `DrawStrectchedBackground(source, destination)` -> its result.
    fn draw(&mut self, source: UiRect, destination: UiRect) -> c_long;
}

/// `UIBitmapBackground::DrawBackground`: the overlap with the background, stretched from the whole background.
pub fn bitmap_draw(h: &mut impl TileHost, rect: UiRect, redraw: UiRect) -> c_long {
    match and(&redraw, &rect) {
        Some(clip) => h.draw(rect, clip),
        None => 0,
    }
}

/// `UITiledBackground::DrawTiledBackground`: each tile of the rows and columns (`w` x `h` plus one pixel apart from
/// the background's top left), rounded, clipped to `clip` and rounded again.
#[allow(clippy::too_many_arguments)]
pub fn draw_tiled(
    host: &mut impl TileHost,
    rect: UiRect,
    clip: UiRect,
    first_row: c_ulong,
    last_row: c_ulong,
    first_column: c_ulong,
    last_column: c_ulong,
    w: f32,
    h: f32,
) -> c_long {
    let mut result = 0;
    let mut row = first_row;
    while row <= last_row {
        let mut column = first_column;
        while column <= last_column {
            let x = column as f32 * (w + 1.0) + rect.left;
            let y = rect.top + (h + 1.0) * row as f32;
            let mut tile = UiRect::new(x, y, x + w, y + h);
            tile.round_to_nearest();
            if let Some(mut clipped) = and(&tile, &clip) {
                clipped.round_to_nearest();
                result = host.draw(tile, clipped);
            }
            if column == c_ulong::MAX {
                break;
            }
            column += 1;
        }
        if row == c_ulong::MAX {
            break;
        }
        row += 1;
    }
    result
}

/// The tiled kinds (`DrawBackground` of the classes that tile).
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Tiling {
    /// `UITiledBackground`: texture-sized tiles across and down.
    Both,
    /// `UILRTiledBackground`: one row of texture-wide tiles as high as the background.
    LeftRight,
    /// `UITBTiledBackground`: one column of texture-high tiles as wide as the background.
    TopBottom,
    /// `UIStretchedTiledBackground`: a given number of tiles across and down, stretched.
    Stretched { across: f32, down: f32 },
}

/// What a tiling class's `DrawBackground` hands its (virtual) `DrawTiledBackground`: the redraw rectangle's overlap
/// with the background, the rows and columns of tiles under it (longs passed as unsigned longs, as the C++ did) and
/// the tile size.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TilePlan {
    pub clip: UiRect,
    pub first_row: c_ulong,
    pub last_row: c_ulong,
    pub first_column: c_ulong,
    pub last_column: c_ulong,
    pub w: f32,
    pub h: f32,
}

/// The `DrawBackground` of a tiling class up to its `DrawTiledBackground` call; `None` when the redraw rectangle
/// misses the background (the C++ returns 0).
pub fn tiled_plan(kind: Tiling, rect: UiRect, texture: UiRect, redraw: UiRect) -> Option<TilePlan> {
    let clip = and(&redraw, &rect)?;
    let rows = |h: f32| (um_floor((clip.top - rect.top) / h), um_floor((clip.bottom - rect.top) / h));
    let columns = |w: f32| (um_floor((clip.left - rect.left) / w), um_floor((clip.right - rect.left) / w));
    let (w, h, (r0, r1), (c0, c1)) = match kind {
        Tiling::Both => {
            let (w, h) = (texture.width() - 1.0, texture.height() - 1.0);
            (w, h, rows(h), columns(w))
        }
        Tiling::LeftRight => {
            let (w, h) = (texture.width() - 1.0, rect.height());
            (w, h, (0, 0), columns(w))
        }
        Tiling::TopBottom => {
            let (w, h) = (rect.width(), texture.height() - 1.0);
            (w, h, rows(h), (0, 0))
        }
        Tiling::Stretched { across, down } => {
            let (w, h) = (rect.width() / across, rect.height() / down);
            (w, h, rows(h + 1.0), columns(w + 1.0))
        }
    };
    Some(TilePlan {
        clip,
        first_row: r0 as c_ulong,
        last_row: r1 as c_ulong,
        first_column: c0 as c_ulong,
        last_column: c1 as c_ulong,
        w,
        h,
    })
}

/// A full-screen background's tiles: `draw(index, tile, clipped)`.
pub trait FullHost {
    fn draw(&mut self, index: c_ulong, tile: UiRect, clipped: UiRect) -> c_long;
}

/// `UIFullBackground::DrawBackground`: the grid of `columns` x `rows` textures, designed for a screen of
/// `designed`, scaled to the window; each tile under the redraw rectangle stretched onto its overlap.
pub fn full_draw(
    h: &mut impl FullHost,
    redraw: UiRect,
    texture: (c_ulong, c_ulong),
    window: (c_ulong, c_ulong),
    designed: (c_ulong, c_ulong),
    grid: (c_ulong, c_ulong),
) -> c_long {
    let mut result = 0;
    let (columns, rows) = grid;
    let tile_w = window.0 as f32 / designed.0 as f32 * texture.0 as f32;
    let tile_h = window.1 as f32 / designed.1 as f32 * texture.1 as f32;
    let first_row = um_floor(redraw.top / tile_h) as c_ulong;
    let mut last_row = um_floor(redraw.bottom / tile_h) as c_ulong;
    let first_column = um_floor(redraw.left / tile_w) as c_ulong;
    let mut last_column = um_floor(redraw.right / tile_w) as c_ulong;
    if last_row >= rows {
        last_row = rows.wrapping_sub(1);
    }
    if last_column >= columns {
        last_column = columns.wrapping_sub(1);
    }
    let mut row = first_row;
    while row <= last_row {
        let mut column = first_column;
        while column <= last_column {
            let index = columns.wrapping_mul(row).wrapping_add(column);
            let top = row as f32 * tile_h;
            let bottom = top + tile_h - 1.0;
            let left = column as f32 * tile_w;
            let right = left + tile_w - 1.0;
            let tile = UiRect::new(left, top, right, bottom);
            if let Some(clipped) = and(&tile, &redraw) {
                result = h.draw(index, tile, clipped);
            }
            if column == c_ulong::MAX {
                break;
            }
            column += 1;
        }
        if row == c_ulong::MAX {
            break;
        }
        row += 1;
    }
    result
}

/// The rectangle a background is laid out in: the window, inclusive coordinates unless `exclusive`
/// (`UIBackground::Open`; `RelayoutBackground` always inclusive).
pub fn screen_rect(width: c_ulong, height: c_ulong, exclusive: bool) -> UiRect {
    let offset = if exclusive { 0.0 } else { 1.0 };
    UiRect::new(0.0, 0.0, width as f32 - offset, height as f32 - offset)
}

/// A background's form as its moves and visibility changes ask about it.
pub trait MoveHost {
    /// `mIsVisible`, `mRectangle`.
    fn visible(&self) -> bool;
    fn set_visible(&mut self, v: bool);
    fn rect(&self) -> UiRect;
    fn set_rect(&mut self, r: UiRect);
    /// `mForm == mForm->GetManager()->GetCurrentForm()`.
    fn form_is_current(&self) -> bool;
    /// The form's `LockDrawBackground` / `UnlockDrawBackground`.
    fn lock(&mut self);
    fn unlock(&mut self);
    /// The form's `RedrawIntersectControls(r)` and the manager's `AddDirtyRectangle(r, form)`.
    fn redraw(&mut self, r: UiRect);
    fn dirty(&mut self, r: UiRect);
}

/// `UIBackground::SetRectangle`: moved under the form's lock; on the current form, the old and the new area redrawn.
pub fn set_rectangle(h: &mut impl MoveHost, r: UiRect) {
    let current = h.visible() && h.form_is_current();
    h.lock();
    if current {
        let old = h.rect();
        h.redraw(old);
        let old = h.rect();
        h.dirty(old);
    }
    h.set_rect(r);
    if current {
        let new = h.rect();
        h.redraw(new);
        let new = h.rect();
        h.dirty(new);
    }
    h.unlock();
}

/// `UIBackground::SetVisibility`: a change shown or hidden under the form's lock, its area redrawn.
pub fn set_visibility(h: &mut impl MoveHost, visible: bool) {
    if visible == h.visible() {
        return;
    }
    h.lock();
    h.set_visible(visible);
    let r = h.rect();
    h.redraw(r);
    let r = h.rect();
    h.dirty(r);
    h.unlock();
}

/// `UIForm::DrawBackground(bitmap, rect, draw, tile)`: a bitmap tiled over the tile area from its top left, one piece
/// per tile under the rectangle; `draw(piece, location)` gets the piece of the bitmap's draw rectangle and where it
/// goes. The tile size is the draw rectangle's, floored to whole pixels.
pub trait FormTileHost {
    fn draw(&mut self, piece: UiRect, location: (f32, f32));
}

/// See [`FormTileHost`].
pub fn form_tiles(h: &mut impl FormTileHost, rect: UiRect, draw: UiRect, tiles: UiRect) {
    let Some(clipped) = and(&rect, &tiles) else {
        return;
    };
    let tile_w = um_floor(draw.width()) as c_ulong;
    let tile_h = um_floor(draw.height()) as c_ulong;
    let first_row = um_floor((clipped.top - tiles.top) / tile_h as f32) as c_ulong;
    let last_row = um_floor((clipped.bottom - tiles.top) / tile_h as f32) as c_ulong;
    let first_column = um_floor((clipped.left - tiles.left) / tile_w as f32) as c_ulong;
    let last_column = um_floor((clipped.right - tiles.left) / tile_w as f32) as c_ulong;
    let mut row = first_row;
    while row <= last_row {
        let mut column = first_column;
        while column <= last_column {
            let px = tiles.left + column.wrapping_mul(tile_w) as f32;
            let py = tiles.top + row.wrapping_mul(tile_h) as f32;
            let mut tile = UiRect::new(px, py, px + tile_w as f32, py + tile_h as f32);
            if and_in_place(&mut tile, &clipped) && tile.width() > 0.0 && tile.height() > 0.0 {
                let location = (tile.left, tile.top);
                let (ox, oy) = (draw.left, draw.top);
                tile.left = tile.left + ox - px;
                tile.right = tile.right + ox - px;
                tile.bottom = oy + tile.bottom - py;
                tile.top = tile.top + oy - py;
                h.draw(tile, location);
            }
            if column == c_ulong::MAX {
                break;
            }
            column += 1;
        }
        if row == c_ulong::MAX {
            break;
        }
        row += 1;
    }
}

#[cfg(test)]
mod tests;
