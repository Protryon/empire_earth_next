//! The units' render object's geometry (`src/Empire Earth/Graphics Thread/EEROUnits.cpp`): `GetScreenPoint` (a world
//! point to window coordinates in the game view), the bars' sizes from the window (`DrawBars`), a unit bar's origin
//! and rectangle split (`DrawBar`: the filled part and the rest, each a pair of rhombi at least a pixel wide) and an
//! object's health bar (`DrawObjectBars`), with the bar tags' world points. The C++ keeps every GE call: the rhombi,
//! their colours and the rasterizer's draws, in the original's order; it hands these functions plain values.
//!
//! This is the renderer's, not the simulation's: nothing here is simulation state.

use ee_ge::types::{Ge2DPoint, Ge3DPoint, GeRectangle, GeTransformation};
use ee_ge::um::floor;
use ee_ge::viewport::GeViewport;

/// `kBarFraction`: the filled part's share of a bar's own width.
pub const BAR_FRACTION: f32 = 0.75;

/// `GetScreenPoint`: `point` through the camera's inverse, normalised (perspective or orthographic), to the game
/// view's window coordinates.
pub fn screen_point(vp: &GeViewport, orthographic: bool, point: Ge3DPoint) -> Ge2DPoint {
    let c = vp.camera_inverse.apply_point(point);
    let (nx, ny, mut w);
    if !orthographic {
        nx = c.x * vp.perspective_scale;
        ny = c.z * vp.perspective_scale;
        w = c.y;
    } else {
        nx = c.x * vp.orthographic_scale;
        ny = c.z * vp.orthographic_scale;
        w = 1.0;
    }
    if w < 0.0 {
        w = -w;
    }
    if w != 0.0 {
        w = 1.0 / w;
    } else {
        w = 1.0;
    }
    Ge2DPoint {
        x: w * (vp.half_width * nx) + (vp.x_center - vp.view_rectangle.left),
        y: vp.y_center - vp.view_rectangle.top - ny * vp.half_width * w,
    }
}

/// `DrawBar`'s bar tag in the world: the unit's bar offset scaled by its size, through its transformation.
pub fn unit_bar_point(transformation: &GeTransformation, bar: Ge3DPoint, scale: f32) -> Ge3DPoint {
    let offset = Ge3DPoint { x: bar.x * scale, y: bar.y * scale, z: bar.z * scale };
    transformation.apply_point(offset)
}

/// `DrawObjectBars`' bar tag in the world: the object's location plus its bar offset scaled by its model scale.
pub fn object_bar_point(location: Ge3DPoint, bar: Ge3DPoint, scale: f32) -> Ge3DPoint {
    Ge3DPoint { x: location.x + bar.x * scale, y: location.y + bar.y * scale, z: location.z + bar.z * scale }
}

/// The bars' size for a frame (`sBarWidth`, `sBarHeight`, `sBarStep`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxBarSizes {
    pub width: u32,
    pub height: f32,
    pub step: f32,
}

impl Default for EeGfxBarSizes {
    /// The unit's initial values (.data).
    fn default() -> Self {
        EeGfxBarSizes { width: 24, height: 2.0, step: 5.0 }
    }
}

/// `DrawBars`' sizes from the window's size.
pub fn bar_sizes(window_width: u32, window_height: u32) -> EeGfxBarSizes {
    let width = floor(24.0 * (window_width as f32 / 1024.0)).max(24);
    let scale = window_height as f32 * 0.0013020834;
    let height = if 2.0 < scale * 2.0 { scale * 2.0 } else { 2.0 };
    let step = if 5.0 < scale * 5.0 { scale * 5.0 } else { 5.0 };
    EeGfxBarSizes { width: width as u32, height, step }
}

/// A bar's rectangles, in the C++'s order of use. `kind` 0: full (fill, back), 1: partial (fill, its back, the
/// rest, the rest's back), 2: empty (the rest, its back); `count` rectangles are set.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxBarRects {
    pub kind: u32,
    pub count: u32,
    pub rects: [GeRectangle; 4],
}

fn rect(left: f32, top: f32, right: f32, bottom: f32) -> GeRectangle {
    GeRectangle { bottom, left, right, top }
}

/// `max(width, 1)`: a rectangle under a pixel wide grows to one (`GetWidth() < 1`).
fn at_least_a_pixel(mut r: GeRectangle) -> GeRectangle {
    if r.right - r.left < 1.0 {
        r.right = r.left + 1.0;
    }
    r
}

/// `DrawBar`'s origin: the screen point of the bar tag moved left by half a bar and up by `row` steps, floored.
pub fn bar_origin(screen: Ge2DPoint, row: i32, sizes: &EeGfxBarSizes) -> Ge2DPoint {
    let x = screen.x - (sizes.width / 2) as f32;
    let y = screen.y - row as f32 * sizes.step;
    Ge2DPoint { x: floor(x) as f32, y: floor(y) as f32 }
}

/// `DrawBar`'s rectangles from its origin and fraction.
pub fn unit_bar(origin: Ge2DPoint, fraction: f32, sizes: &EeGfxBarSizes) -> EeGfxBarRects {
    let w = sizes.width as f32;
    let (x, y) = (origin.x, origin.y);
    let mut out = EeGfxBarRects::default();
    if fraction >= 1.0 {
        let bar = rect(x, y, w * BAR_FRACTION + x, y + sizes.height);
        let back = rect(bar.right, bar.top, w + x, bar.bottom);
        out.kind = 0;
        out.count = 2;
        out.rects[0] = bar;
        out.rects[1] = back;
    } else if fraction != 0.0 {
        let bar = at_least_a_pixel(rect(x, y, w * BAR_FRACTION * fraction + x, y + sizes.height));
        let back = at_least_a_pixel(rect(bar.right, bar.top, w * fraction + x, bar.bottom));
        let rest =
            at_least_a_pixel(rect(back.right, back.top, (w - w * fraction) * BAR_FRACTION + back.right, back.bottom));
        let rest_back = rect(rest.right, rest.top, w + x, rest.bottom);
        out.kind = 1;
        out.count = 4;
        out.rects = [bar, back, rest, rest_back];
    } else {
        let bar = rect(x, y, w * BAR_FRACTION + x, y + sizes.height);
        let back = rect(bar.right, bar.top, w + x, bar.bottom);
        out.kind = 2;
        out.count = 2;
        out.rects[0] = bar;
        out.rects[1] = back;
    }
    out
}

/// `DrawObjectBars`' rectangles from the screen point of the object's bar tag (not floored) and its health. `kind`
/// 0: full (the fill), 1: partial (the fill, the back), 2: empty (the back).
pub fn object_bar(screen: Ge2DPoint, health: f32, sizes: &EeGfxBarSizes) -> EeGfxBarRects {
    let w = sizes.width as f32;
    let x = screen.x - (sizes.width / 2) as f32;
    let y = screen.y - sizes.step;
    let mut out = EeGfxBarRects::default();
    if health >= 1.0 {
        out.kind = 0;
        out.count = 1;
        out.rects[0] = rect(x, y, w + x, y + sizes.height);
    } else if health != 0.0 {
        let bar = rect(x, y, w * health + x, y + sizes.height);
        out.kind = 1;
        out.count = 2;
        out.rects[0] = bar;
        out.rects[1] = rect(bar.right, bar.top, w + bar.left, bar.bottom);
    } else {
        out.kind = 2;
        out.count = 1;
        out.rects[0] = rect(x, y, w + x, y + sizes.height);
    }
    out
}

/// `sSelectionSizes`: the largest selection size each ground ring model fits (the last takes the rest).
pub(crate) const SELECTION_SIZES: [f32; 3] = [0.75, 1.5, 100.0];
/// `kSelectionGrow`: a ground ring's growth over its unit's selection size.
pub(crate) const SELECTION_GROW: f32 = 0.3;

/// A ground unit's selection ring: which of the three models (`sSelectionModels`) and its scale.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxSelectionRing {
    pub model: u32,
    pub scale: f32,
}

/// `DrawSelection`'s size choice: the first model whose size the unit's does not exceed (of the first two; else the
/// third), grown by 0.3 of the size.
pub fn selection_ring(size: f32) -> EeGfxSelectionRing {
    let mut model = 0;
    while model < 2 {
        if size <= SELECTION_SIZES[model] {
            break;
        }
        model += 1;
    }
    let mut scale = SELECTION_GROW * size;
    scale += size;
    EeGfxSelectionRing { model: model as u32, scale }
}

/// The area-effect ring's segments: `ring` in the ring colour (one per strength point), then `highlight` in the
/// highlight colour up to seven in all; three vertices each.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxRingSegments {
    pub ring: u32,
    pub highlight: u32,
}

/// `DrawRings`' colours. A strength past 7 colours more than the ring's seven segments, as the original did (its
/// first loop runs to the strength).
pub fn ring_segments(strength: u32) -> EeGfxRingSegments {
    EeGfxRingSegments { ring: strength, highlight: 7_u32.saturating_sub(strength) }
}

/// `EERenderUnitRecord`'s footprint grid (`mHeights`, `mCells`: 12 x 12 corners and cells).
pub(crate) const PLACEMENT_GRID: usize = 12;

/// One edge of a placed building's footprint, in window coordinates: the outline is `gBarColor` (`back` 0), the
/// edges between usable and unusable cells `gBarBackColor` (`back` 1).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxPlacementLine {
    pub back: u32,
    pub start: Ge2DPoint,
    pub end: Ge2DPoint,
}

/// `DrawPlacement`'s footprint edges for a `size` x `size` grid round tile (`tile_x`, `tile_y`), in the order the
/// original drew them: the outline (bar colour), then every edge with an unusable cell (`cells[x][y] == 0`) on
/// exactly one side (back colour). `heights[x][y]` are the grid's corner heights. Writes at most `out.len()` lines
/// and returns their count. A size past 11 would index past the 12 x 12 grid (the original's stack overrun): it
/// draws no edges.
#[allow(clippy::too_many_arguments)]
pub fn placement_lines(
    vp: &GeViewport,
    orthographic: bool,
    size: i32,
    tile_x: i32,
    tile_y: i32,
    heights: &[[f32; PLACEMENT_GRID]; PLACEMENT_GRID],
    cells: &[[u8; PLACEMENT_GRID]; PLACEMENT_GRID],
    out: &mut [EeGfxPlacementLine],
) -> usize {
    if size >= PLACEMENT_GRID as i32 {
        return 0;
    }
    let half = if size % 2 == 0 { size as f32 * 0.5 } else { (size - 1) as f32 * 0.5 };
    let left = tile_x as f32 - half;
    let top = tile_y as f32 - half;
    let mut n = 0usize;
    // the edge from corner (j, x) one tile along x (`along_x`) or along y
    let mut push = |back: u32, j: i32, x: i32, along_x: bool| {
        if n >= out.len() {
            return;
        }
        let (ju, xu) = (j as usize, x as usize);
        let line_x = j as f32 + left;
        let line_y = x as f32 + top;
        let start = screen_point(vp, orthographic, Ge3DPoint { x: line_x, y: line_y, z: heights[ju][xu] });
        let end = if along_x {
            screen_point(vp, orthographic, Ge3DPoint { x: line_x + 1.0, y: line_y, z: heights[ju + 1][xu] })
        } else {
            screen_point(vp, orthographic, Ge3DPoint { x: line_x, y: line_y + 1.0, z: heights[ju][xu + 1] })
        };
        out[n] = EeGfxPlacementLine { back, start, end };
        n += 1;
    };
    for x in 0..=size {
        for j in 0..=size {
            if (x == 0 || x == size) && j != size {
                push(0, j, x, true);
            }
            if (j != 0 && j != size) || x == size {
                continue;
            }
            push(0, j, x, false);
        }
    }
    let mut rows = [[false; PLACEMENT_GRID]; PLACEMENT_GRID];
    let mut columns = [[false; PLACEMENT_GRID]; PLACEMENT_GRID];
    let cells_side = size.max(0) as usize;
    for x in 0..cells_side {
        for j in 0..cells_side {
            if cells[j][x] != 0 {
                continue;
            }
            rows[j][x] ^= true;
            rows[j][x + 1] ^= true;
            columns[j][x] ^= true;
            columns[j + 1][x] ^= true;
        }
    }
    for x in 0..=size {
        for j in 0..=size {
            let (ju, xu) = (j as usize, x as usize);
            if rows[ju][xu] && j != size {
                push(1, j, x, true);
            }
            if !columns[ju][xu] || x == size {
                continue;
            }
            push(1, j, x, false);
        }
    }
    n
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bars_size_with_the_window() {
        assert_eq!(bar_sizes(1024, 768), EeGfxBarSizes { width: 24, height: 2.0, step: 5.0 });
        let big = bar_sizes(2048, 1536);
        assert_eq!(big.width, 48);
        assert!((big.height - 4.0).abs() < 1e-5 && (big.step - 10.0).abs() < 1e-5);
        assert_eq!(bar_sizes(640, 480).width, 24);
    }

    #[test]
    fn a_partial_bar_splits_in_four() {
        let s = EeGfxBarSizes::default();
        let o = bar_origin(Ge2DPoint { x: 100.7, y: 50.2 }, 1, &s);
        assert_eq!(o, Ge2DPoint { x: 88.0, y: 45.0 });
        let b = unit_bar(o, 0.5, &s);
        assert_eq!((b.kind, b.count), (1, 4));
        assert_eq!(b.rects[0].right, 88.0 + 9.0);
        assert_eq!(b.rects[1].right, 88.0 + 12.0);
        assert_eq!(b.rects[2].right, 100.0 + 9.0);
        assert_eq!(b.rects[3].right, 88.0 + 24.0);
        // a sliver still draws a pixel
        let thin = unit_bar(o, 0.001, &s);
        assert_eq!(thin.rects[0].right - thin.rects[0].left, 1.0);
        assert_eq!(unit_bar(o, 1.0, &s).kind, 0);
        assert_eq!(unit_bar(o, 0.0, &s).kind, 2);
    }

    #[test]
    fn object_bars_split_in_two() {
        let s = EeGfxBarSizes::default();
        let b = object_bar(Ge2DPoint { x: 20.0, y: 20.0 }, 0.25, &s);
        assert_eq!((b.kind, b.count), (1, 2));
        assert_eq!(b.rects[0], rect(8.0, 15.0, 14.0, 17.0));
        assert_eq!(b.rects[1], rect(14.0, 15.0, 32.0, 17.0));
    }

    #[test]
    fn a_footprint_draws_its_outline_and_the_blocked_cells() {
        let vp = GeViewport::default();
        let heights = [[0.0f32; PLACEMENT_GRID]; PLACEMENT_GRID];
        let mut cells = [[1u8; PLACEMENT_GRID]; PLACEMENT_GRID];
        let mut out = [EeGfxPlacementLine::default(); 320];
        assert_eq!(placement_lines(&vp, false, 3, 10, 10, &heights, &cells, &mut out), 12);
        assert!(out[..12].iter().all(|l| l.back == 0));
        // one blocked cell inside: its four edges in the back colour
        cells[1][1] = 0;
        assert_eq!(placement_lines(&vp, false, 3, 10, 10, &heights, &cells, &mut out), 16);
        assert!(out[12..16].iter().all(|l| l.back == 1));
        // two blocked neighbours: the edge between them is not drawn
        cells[2][1] = 0;
        assert_eq!(placement_lines(&vp, false, 3, 10, 10, &heights, &cells, &mut out), 12 + 6);
        assert_eq!(placement_lines(&vp, false, 0, 10, 10, &heights, &cells, &mut out), 0);
        assert_eq!(placement_lines(&vp, false, 12, 10, 10, &heights, &cells, &mut out), 0);
        let all = [[0u8; PLACEMENT_GRID]; PLACEMENT_GRID];
        assert_eq!(placement_lines(&vp, false, 11, 10, 10, &heights, &all, &mut out), 44 + 44);
    }
}
