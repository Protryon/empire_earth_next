//! The mini map control (`EEUIMiniMap`, `User Interface Thread/EEUIMiniMap.cpp`): `CopyData`'s snapshot (the
//! map and units bitmaps scaled to the window, the camera's outline, the blinking flare and when the screen must
//! be redrawn for it), `DrawPolygons` as a plan the C++ executes (the bitmaps, the outline clipped to the map
//! inside its border), `Update`'s texture coordinates and map bounds, and the left release and drag (a flare sent
//! at the clicked tile, or the camera centred) over the C++ host.
//!
//! The C++ reads the world (`gMiniMap`, the window, the bitmaps) into the records below and applies the results
//! in the original's order; nothing here reads the world itself. f32 arithmetic in the original's order.

use crate::screens::ui_thread::UiHost;

/// `GETransformation::Apply(GE2DPoint)` over the matrix's six 2D entries (m00, m01, m03, m10, m11, m13).
fn apply(m: &[f32; 6], p: [f32; 2]) -> [f32; 2] {
    [p[1] * m[1] + p[0] * m[0] + m[2], p[0] * m[3] + p[1] * m[4] + m[5]]
}

/// `GERectangle::Contains` (edges included) of a (left, top, right, bottom) rectangle.
fn contains(r: &[f32; 4], p: [f32; 2]) -> bool {
    p[0] >= r[0] && p[0] <= r[2] && p[1] >= r[1] && p[1] <= r[3]
}

/// The flare's colour (`EeScreensMiniMapCopy::flare_color`): none set, `sFlareColor` (red), `sFlareBlinkColor`.
pub const EE_SCREENS_MM_COLOR_NONE: u8 = 0;
pub const EE_SCREENS_MM_COLOR_FLARE: u8 = 1;
pub const EE_SCREENS_MM_COLOR_BLINK: u8 = 2;

/// `CopyData`'s record: what the C++ read (in), what it applies (out).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensMiniMapCopy {
    // in: the control's rectangle (left, top, right, bottom), the game window's size, gMiniMap's corners 0..3,
    // the map's origin, gMiniMap's flare flag and point and map-to-pixel matrix (m00 m01 m03 m10 m11 m13),
    // gFreeCamera, the flare texture's height and width (0 without one), the flare bitmap's location and width
    pub rect: [f32; 4],
    pub window_width: u32,
    pub window_height: u32,
    pub corners: [[f32; 2]; 4],
    pub origin: [f32; 2],
    pub flare_point: [f32; 2],
    pub map_to_pixel: [f32; 6],
    pub flare_texture_height: u32,
    pub flare_texture_width: u32,
    pub flare_location: [f32; 2],
    pub flare_width: f32,
    pub flare_on: u8,
    pub free_camera: u8,
    /// in and out: `mShowFlare`, `mFlareBlink`
    pub show_flare: u8,
    pub flare_blink: u8,
    // out: the map and units bitmaps' scales (both at the origin), the flare's colour, the screen's redraw, the
    // flare's new location and scale (when `set_flare`), the outline's four lines (start, end)
    pub flare_color: u8,
    pub redraw_all: u8,
    pub set_flare: u8,
    pub reserved: u8,
    pub map_scale: [f32; 2],
    pub world_scale: [f32; 2],
    pub flare_new_location: [f32; 2],
    pub flare_scale: [f32; 2],
    pub lines: [[[f32; 2]; 2]; 4],
}

/// The flare bitmap's location left the map's inner area (inset by 4/3 of its width): the screen is redrawn.
fn flare_outside(c: &EeScreensMiniMapCopy) -> bool {
    let inset = c.flare_width * 1.333_333_4;
    let r = [c.rect[0] + inset, c.rect[1] + inset, c.rect[2] - inset, c.rect[3] - inset];
    !contains(&r, c.flare_location)
}

/// `EEUIMiniMap::CopyData` (0x675585) after `gMiniMap->UpdateWorldTexture()`: the scales (the window over
/// 800 x 600), the corners in the control, the flare.
pub fn copy_data(c: &mut EeScreensMiniMapCopy) {
    let (left, top) = (c.rect[0], c.rect[1]);
    let sx = c.window_width as f32 * 0.00125;
    let sy = c.window_height as f32 * 0.001_666_666_7;
    let corner = |p: [f32; 2]| [p[0] * sx + c.origin[0] - left, p[1] * sy + c.origin[1] - top];
    let (c0, c1, c2, c3) = (corner(c.corners[0]), corner(c.corners[1]), corner(c.corners[2]), corner(c.corners[3]));
    c.map_scale = [sx, sy];
    c.world_scale = [2.0 * sx, 2.0 * sy];
    let hide = c.show_flare != 0 && c.flare_on == 0;
    c.show_flare = (c.flare_on != 0 && c.free_camera == 0) as u8;
    c.flare_color = EE_SCREENS_MM_COLOR_NONE;
    c.redraw_all = 0;
    c.set_flare = 0;
    if c.show_flare != 0 {
        let mut flare = apply(&c.map_to_pixel, c.flare_point);
        c.flare_color = if c.flare_blink != 0 { EE_SCREENS_MM_COLOR_FLARE } else { EE_SCREENS_MM_COLOR_BLINK };
        c.flare_blink = (c.flare_blink == 0) as u8;
        let x = c.origin[0];
        flare[0] = flare[0] * sx + x - c.flare_texture_height as f32 * sx * 0.375;
        let y = flare[1] * sy + c.origin[1];
        flare[1] = y - c.flare_texture_width as f32 * sy * 0.375;
        if c.flare_location == flare && flare_outside(c) {
            c.redraw_all = 1;
        }
        c.set_flare = 1;
        c.flare_new_location = flare;
        c.flare_scale = [0.75 * sx, 0.75 * sy];
    } else {
        c.flare_blink = 1;
        if hide && flare_outside(c) {
            c.redraw_all = 1;
        }
    }
    c.lines = [[c0, c2], [c2, c3], [c3, c1], [c1, c0]];
}

// ---- DrawPolygons' plan (EE_SCREENS_MM_DRAW_*): the steps in order

/// `gMiniMap->mTerrainCriticalSection->Enter()`.
pub const EE_SCREENS_MM_DRAW_ENTER: u8 = 1;
/// `UIControl::DrawPolygons()`.
pub const EE_SCREENS_MM_DRAW_BASE: u8 = 2;
/// The rasterizer's `Draw(sMapBitmap)` (its result unused).
pub const EE_SCREENS_MM_DRAW_MAP: u8 = 3;
/// `Draw(sWorldBitmap)`, `Draw(sFlareBitmap)`: the result.
pub const EE_SCREENS_MM_DRAW_WORLD: u8 = 4;
pub const EE_SCREENS_MM_DRAW_FLARE: u8 = 5;
/// The viewport's view rectangle kept, the viewport set to the plan's clip rectangle.
pub const EE_SCREENS_MM_DRAW_CLIP: u8 = 6;
/// `Draw(&mLines[i], 1)` for i = step - LINE: the result.
pub const EE_SCREENS_MM_DRAW_LINE: u8 = 7;
/// The viewport's view rectangle put back.
pub const EE_SCREENS_MM_DRAW_RESTORE: u8 = 11;
/// `mTerrainCriticalSection->Leave()`.
pub const EE_SCREENS_MM_DRAW_LEAVE: u8 = 12;

/// `DrawPolygons`' plan: its steps (`count` of `steps`) and the clip rectangle (left, top, right, bottom).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensMiniMapDraw {
    pub steps: [u8; 16],
    pub count: u32,
    pub clip: [f32; 4],
}

/// `EEUIMiniMap::DrawPolygons` (0x675ba1) of a control whose flare is shown or not, its lines' draw flags and
/// its copied draw rectangle (left, top, right, bottom): under the terrain's lock the base, the map, the units,
/// the flare, then the outline clipped to the rectangle inside its one-pixel border.
pub fn draw_plan(show_flare: bool, draw_lines: [bool; 4], rect: [f32; 4]) -> EeScreensMiniMapDraw {
    let mut d = EeScreensMiniMapDraw::default();
    let mut push = |s: u8| {
        d.steps[d.count as usize] = s;
        d.count += 1;
    };
    push(EE_SCREENS_MM_DRAW_ENTER);
    push(EE_SCREENS_MM_DRAW_BASE);
    push(EE_SCREENS_MM_DRAW_MAP);
    push(EE_SCREENS_MM_DRAW_WORLD);
    if show_flare {
        push(EE_SCREENS_MM_DRAW_FLARE);
    }
    push(EE_SCREENS_MM_DRAW_CLIP);
    for (i, &on) in draw_lines.iter().enumerate() {
        if on {
            push(EE_SCREENS_MM_DRAW_LINE + i as u8);
        }
    }
    push(EE_SCREENS_MM_DRAW_RESTORE);
    push(EE_SCREENS_MM_DRAW_LEAVE);
    d.clip = [rect[0] + 1.0, rect[1] + 1.0, rect[2] - 1.0, rect[3] - 1.0];
    d
}

/// `Update`'s record: the map's width (`mL195614`), height (`mL195610`) and size, the mini map's map-to-pixel
/// matrix (in); the units texture's coordinate extent and the map's bounds in mini-map units (left, top, right,
/// bottom) (out).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensMiniMapUpdate {
    pub width: i32,
    pub height: i32,
    pub size: i32,
    pub map_to_pixel: [f32; 6],
    pub extent: f32,
    pub bounds: [f32; 4],
}

/// `EEUIMiniMap::Update` (0x676236)'s numbers: the units texture covers the map's smaller side over 64 (whole
/// from 64 up); the bounds are the map's corner tiles' centres.
pub fn update(u: &mut EeScreensMiniMapUpdate) {
    let mut size = u.width.min(u.height) as f32;
    size = if size < 64.0 { size * 0.015625 } else { 1.0 };
    u.extent = size;
    let (w, h, s) = (u.width as f32, u.height as f32, u.size as f32);
    let m = &u.map_to_pixel;
    u.bounds[0] = apply(m, [0.5, h - 1.0 + 0.5])[0];
    u.bounds[2] = apply(m, [s - 1.0 + 0.5, w + 0.5])[0];
    u.bounds[1] = apply(m, [0.5, h + 0.5])[1];
    u.bounds[3] = apply(m, [s - 1.0 + 0.5, w - 1.0 + 0.5])[1];
}

// ---- the left release and drag's host (EE_SCREENS_MM_OP_*)

/// `GetHasMouseCapture()`.
pub const EE_SCREENS_MM_OP_HAS_CAPTURE: u32 = 1;
/// `GetLeftMouseButtonDown()`.
pub const EE_SCREENS_MM_OP_LEFT_DOWN: u32 = 2;
/// `gMouse->mPointer`.
pub const EE_SCREENS_MM_OP_POINTER: u32 = 3;
/// `CenterCamera()`.
pub const EE_SCREENS_MM_OP_CENTER: u32 = 4;
/// `SetDirty(true)`.
pub const EE_SCREENS_MM_OP_DIRTY: u32 = 5;
/// The [`EeScreensMiniMapMouse`] at c filled: the form's mouse location, the rectangle, gMiniMap's
/// pixel-to-map matrix.
pub const EE_SCREENS_MM_OP_READ_MOUSE: u32 = 6;
/// `gWorld.mMap->GetTileAtLocation((a, b)) != NULL`.
pub const EE_SCREENS_MM_OP_TILE_EXISTS: u32 = 7;
/// `gCommunications->ProcessLocalUserInput(new EEMSendFlare((a, b)), kSlotNone)`.
pub const EE_SCREENS_MM_OP_SEND_FLARE: u32 = 8;
/// `gMouse->mPointer = 0`, the form's `SetUIState(0)`.
pub const EE_SCREENS_MM_OP_FLARE_DONE: u32 = 9;
/// `ReleaseCapturedMouse()`.
pub const EE_SCREENS_MM_OP_RELEASE: u32 = 10;

/// The flare cursor (`gMouse->mPointer`).
const FLARE_POINTER: i64 = 0x16;

/// What the left release reads of the form's mouse: its location, the control's rectangle (left, top, right,
/// bottom) and gMiniMap's pixel-to-map matrix (m00 m01 m03 m10 m11 m13).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensMiniMapMouse {
    pub mouse: [f32; 2],
    pub rect: [f32; 4],
    pub pixel_to_map: [f32; 6],
}

/// `EEUIMiniMap::MouseLButtonUp` (0x675f72): with the mouse captured, the camera centred, or with the flare
/// cursor a flare sent at the tile under a mouse inside the control (the cursor and the form's mode reset); the
/// mouse released. Its result is 0.
pub fn mouse_l_up(h: &mut impl UiHost) -> i64 {
    if h.op(EE_SCREENS_MM_OP_HAS_CAPTURE, 0, 0, 0) == 0 {
        return 0;
    }
    if h.op(EE_SCREENS_MM_OP_POINTER, 0, 0, 0) != FLARE_POINTER {
        h.op(EE_SCREENS_MM_OP_CENTER, 0, 0, 0);
    } else {
        let mut m = EeScreensMiniMapMouse::default();
        h.op(EE_SCREENS_MM_OP_READ_MOUSE, 0, 0, &mut m as *mut EeScreensMiniMapMouse as i64);
        if contains(&m.rect, m.mouse) {
            let local = [m.mouse[0] - m.rect[0], m.mouse[1] - m.rect[1]];
            let world = apply(&m.pixel_to_map, local);
            // (the tile and the flare's point: the floats truncated to longs)
            let (x, y) = (world[0] as i32 as i64, world[1] as i32 as i64);
            if h.op(EE_SCREENS_MM_OP_TILE_EXISTS, x, y, 0) != 0 {
                h.op(EE_SCREENS_MM_OP_SEND_FLARE, x, y, 0);
                h.op(EE_SCREENS_MM_OP_FLARE_DONE, 0, 0, 0);
            }
        }
    }
    h.op(EE_SCREENS_MM_OP_RELEASE, 0, 0, 0);
    0
}

/// `EEUIMiniMap::MouseMove` (0x6760af): a left drag with the mouse captured keeps centring the camera (not with
/// the flare cursor). Its result is 0.
pub fn mouse_move(h: &mut impl UiHost) -> i64 {
    if h.op(EE_SCREENS_MM_OP_HAS_CAPTURE, 0, 0, 0) != 0
        && h.op(EE_SCREENS_MM_OP_LEFT_DOWN, 0, 0, 0) != 0
        && h.op(EE_SCREENS_MM_OP_POINTER, 0, 0, 0) != FLARE_POINTER
    {
        h.op(EE_SCREENS_MM_OP_CENTER, 0, 0, 0);
        h.op(EE_SCREENS_MM_OP_DIRTY, 0, 0, 0);
    }
    0
}

/// The op as the shared control host's (`EEUIRustControl`, `ee_screenscpp_hud_ctl`): dirty and the mouse's release
/// as its ops, the others as the class's own (`EE_SCREENS_HC_CLASS + op`, `RustOp`; the mouse's read into the op's
/// floats, an [`EeScreensMiniMapMouse`]); with its a.
pub fn hud_op(op: u32, a: i64) -> (u32, i64) {
    use crate::screens::hud_ctl::{EE_SCREENS_HC_CLASS, EE_SCREENS_HC_RELEASE_MOUSE, EE_SCREENS_HC_SET_DIRTY};
    match op {
        EE_SCREENS_MM_OP_DIRTY => (EE_SCREENS_HC_SET_DIRTY, 1),
        EE_SCREENS_MM_OP_RELEASE => (EE_SCREENS_HC_RELEASE_MOUSE, a),
        _ => (EE_SCREENS_HC_CLASS + op, a),
    }
}

#[cfg(test)]
mod hud_op_tests {
    use super::*;
    use crate::screens::hud_ctl::*;

    #[test]
    fn dirty_and_the_release_are_the_hosts() {
        assert_eq!(hud_op(EE_SCREENS_MM_OP_DIRTY, 0), (EE_SCREENS_HC_SET_DIRTY, 1));
        assert_eq!(hud_op(EE_SCREENS_MM_OP_RELEASE, 0), (EE_SCREENS_HC_RELEASE_MOUSE, 0));
        for op in [1, 2, 3, 4, 6, 7, 8, 9] {
            assert_eq!(hud_op(op, 5), (EE_SCREENS_HC_CLASS + op, 5));
        }
        // the mouse's read is twelve floats (the host's floats)
        assert_eq!(core::mem::size_of::<EeScreensMiniMapMouse>(), 12 * 4);
    }
}

#[cfg(test)]
mod tests;
