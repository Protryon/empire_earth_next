//! `EEMiniMap`'s cells and pixels (src/Empire Earth/Map/EEMiniMap.cpp): the 64 x 64 cells' sight counts and
//! building and unit dots, the unit layer's pixel of each cell, the terrain texture's pixel colours, the redraw's
//! diagonal walk, over a host of the C++ parts (the tiles' sight, a tile's doppelgangers and units, the terrain
//! colours) and the terrain mesh ([`crate::minimap_ffi`] reads it directly).
//!
//! The C++ keeps the surfaces, textures, critical sections, the transformations' build (ee_gfx minimap.rs) and the
//! view trapezoid; the classes' dots (`MMPlayerUnit`, `MMMapUnit`) are these records.
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64
// the C++'s literals, kept
#![allow(clippy::excessive_precision)]

use ee_base::math::types::GeTransformation;
use ee_base::math::types::{Ge2DPoint, Ge3DPoint, GeVector};
use ee_base::math::um::lrintf;
use std::os::raw::{c_long, c_ulong};

/// The cells per side and in all.
pub(crate) const SIDE: c_long = 0x40;
pub(crate) const CELLS: usize = 0x1000;
/// The terrain texture's side in pixels (`kMiniMapSize`).
const PIXELS: f32 = 128.0;
/// "Not on the map" (`MoveUnit`'s -10000).
pub(crate) const OFF_MAP: c_long = -10000;
/// `gMiniMapAmbient`: the terrain colours' ambient term.
const AMBIENT: [f32; 3] = [0.102745101, 0.132549018, 0.188235298];
/// `kPixelFormatR5G6B5`.
const R5G6B5: c_ulong = 7;

/// A building's dot (`MMPlayerUnit`: compared by player and colour).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAppMiniMapDot {
    pub player: u16,
    pub color: u16,
    /// 1: a resource the units' dots go after
    pub flag: c_long,
}

/// A unit's dot (`MMMapUnit`: compared by player, colour and tile).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAppMiniMapUnitDot {
    pub player: u16,
    pub color: u16,
    pub flag: c_long,
    pub x: c_long,
    pub y: c_long,
}

/// What the C++ says of a doppelganger or unit: its dot (colour packed, flag), its class's tile size, a wall's
/// facing, and whether it gets a dot at all (a class with a mini map colour; for a unit also not another player's
/// foundation).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAppMiniMapItem {
    pub player: u16,
    pub color: u16,
    pub flag: c_long,
    pub tile_size: c_long,
    pub facing: c_long,
    pub shown: bool,
    /// a wall: three more segments along its facing (`AddDoppelganger`'s `inIsWall`)
    pub wall: bool,
    /// a unit (`MoveUnit`) rather than a building's image
    pub unit: bool,
    /// the item's tile
    pub x: c_long,
    pub y: c_long,
}

/// A tile's sight as `TellTileVisibility` reads it.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct TileSight {
    /// `IsSeenBy(local)`, or the free camera
    pub seen: bool,
    /// `IsVisibleTo(local)`
    pub visible: bool,
    /// the local player sees all, or the free camera
    pub all: bool,
    /// `GetVisibility(local)`
    pub visibility: c_ulong,
}

/// The C++ parts and the mesh.
pub trait MiniMapHost {
    /// whether a unit dot on the tile is drawn: no tile, the local player sees it, or the free camera
    fn unit_shown(&mut self, x: c_long, y: c_long) -> bool;
    /// the tile's sight (None: no tile)
    fn tile_sight(&mut self, x: c_long, y: c_long) -> Option<TileSight>;
    /// `TellTile`: the tile's own doppelgangers of the local player and its living visible units, in its order
    fn tile_items(&mut self, x: c_long, y: c_long, out: &mut Vec<EeAppMiniMapItem>);
    /// `EETerrainTable::Get(id)->mColor`
    fn terrain_color(&mut self, id: c_ulong) -> [f32; 3];
    // the terrain mesh
    fn valid_point(&mut self, x: c_ulong, y: c_ulong) -> bool;
    fn valid_tile(&mut self, x: c_ulong, y: c_ulong) -> bool;
    fn texture_set(&mut self, x: c_ulong, y: c_ulong) -> c_ulong;
    fn vertex_color(&mut self, x: c_ulong, y: c_ulong) -> [f32; 3];
    fn normal(&mut self, x: c_ulong, y: c_ulong) -> GeVector;
}

/// The mini map's Rust half.
pub struct EeAppMiniMap {
    /// `mU1015C`: each cell's sight count (0 unexplored)
    pub counts: [u16; CELLS],
    /// `mPlayerUnits`: the buildings' dots per cell, head first
    pub dots: Vec<Vec<EeAppMiniMapDot>>,
    /// `mMapUnits`: the units' dots per cell
    pub units: Vec<Vec<EeAppMiniMapUnitDot>>,
    /// the unit layer's locked pixels and row length (`mPixels`, `mPixelsPerRow`)
    pub pixels: *mut u16,
    pub pitch: c_long,
    pub map_to_pixel: GeTransformation,
    pub pixel_to_map: GeTransformation,
    pub map_to_cell: GeTransformation,
    /// `mL12178`, `mL1217C`: the map points one pixel spans
    pub pixel_width: c_long,
    pub pixel_height: c_long,
    /// the world map's two side lengths (`mL195614`, `mL195610`) at the last redraw's walk
    pub width: c_long,
    pub height: c_long,
}

impl Default for EeAppMiniMap {
    fn default() -> Self {
        EeAppMiniMap {
            counts: [0; CELLS],
            dots: vec![Vec::new(); CELLS],
            units: vec![Vec::new(); CELLS],
            pixels: std::ptr::null_mut(),
            pitch: 0,
            map_to_pixel: GeTransformation::default(),
            pixel_to_map: GeTransformation::default(),
            map_to_cell: GeTransformation::default(),
            pixel_width: 0,
            pixel_height: 0,
            width: 0,
            height: 0,
        }
    }
}

/// `GETransformation::Apply(GE2DPoint)`.
fn apply2(t: &GeTransformation, x: f32, y: f32) -> Ge2DPoint {
    let m = &t.m;
    Ge2DPoint { x: y * m[0][1] + x * m[0][0] + m[0][3], y: x * m[1][0] + y * m[1][1] + m[1][3] }
}

/// `UMCeil`: the rounded value, one up when below.
fn ceil(v: f32) -> c_long {
    let r = lrintf(v) as c_long;
    if (r as f32) < v {
        r + 1
    } else {
        r
    }
}

/// `UMFloor`: the rounded value, one down when above.
fn floor(v: f32) -> c_long {
    let r = lrintf(v) as c_long;
    if (r as f32) > v {
        r - 1
    } else {
        r
    }
}

#[allow(clippy::unnecessary_cast)]
fn u(v: c_long) -> c_ulong {
    v as c_ulong
}

fn index(x: c_long, y: c_long) -> usize {
    (x * SIDE + y) as usize
}

impl EeAppMiniMap {
    /// The cell of a map point (`mMapToCell` applied, each coordinate rounded).
    fn cell(&self, x: f32, y: f32) -> (c_long, c_long) {
        let p = self.map_to_cell.apply_point(Ge3DPoint { x, y, z: 0.0 });
        (lrintf(p.x) as c_long, lrintf(p.y) as c_long)
    }

    /// The cell of an item's tile (its footprint's centre).
    fn item_cell(&self, tile_size: c_long, x: c_long, y: c_long) -> (c_long, c_long) {
        let half = tile_size as f32 * 0.5;
        self.cell(half + x as f32, half + y as f32)
    }

    /// `RedrawCell`: the first shown unit's colour, else the first building's, else black, dark or grey by the count.
    pub fn redraw_cell(&mut self, h: &mut impl MiniMapHost, x: c_long, y: c_long) {
        let i = index(x, y);
        let mut pixel = None;
        for d in &self.units[i] {
            if h.unit_shown(d.x, d.y) {
                pixel = Some(d.color);
                break;
            }
        }
        let pixel = pixel.unwrap_or_else(|| match self.dots[i].first() {
            Some(d) => d.color,
            None => match self.counts[i] {
                0 => 0xf000,
                1 => 0x7000,
                _ => 0x777,
            },
        });
        // SAFETY: the C++ hands the locked unit layer (64 x 64 pixels at least) before any redraw
        unsafe { *self.pixels.offset((self.pitch * y + x) as isize) = pixel };
    }

    /// `CountCellOn` (`on`) / `CountCellOff`: one sight source more or less in the cell of a tile (an unexplored
    /// cell starts at 2), the cell redrawn.
    pub fn count_cell(&mut self, h: &mut impl MiniMapHost, x: c_long, y: c_long, on: bool) {
        let (cx, cy) = self.cell(x as f32 + 0.5, y as f32 + 0.5);
        let count = &mut self.counts[index(cx, cy)];
        *count = if !on {
            count.wrapping_sub(1)
        } else if *count != 0 {
            count.wrapping_add(1)
        } else {
            2
        };
        self.redraw_cell(h, cx, cy);
    }

    /// The wall's three further segments of an item at (x, y).
    fn wall_segments(item: &EeAppMiniMapItem, x: c_long, y: c_long) -> [(c_long, c_long); 3] {
        let s = item.tile_size;
        match item.facing {
            0 => [(x, y + s), (x, y + s * 2), (x, y + s * 3)],
            1 => [(x, y - s), (x, y - s * 2), (x, y - s * 3)],
            2 => [(x + s, y), (x + s * 2, y), (x + s * 3, y)],
            _ => [(x - s, y), (x - s * 2, y), (x - s * 3, y)],
        }
    }

    /// `AddDoppelganger` (`add`) / `RemoveDoppelganger`: a building image's dot into (or out of) its cell, a wall's
    /// next three segments too.
    pub fn building(&mut self, h: &mut impl MiniMapHost, item: &EeAppMiniMapItem, x: c_long, y: c_long, add: bool) {
        if item.shown {
            let (cx, cy) = self.item_cell(item.tile_size, x, y);
            let dot = EeAppMiniMapDot { player: item.player, color: item.color, flag: item.flag };
            let list = &mut self.dots[index(cx, cy)];
            if add {
                if list.first().is_some_and(|d| d.flag != 0) {
                    list.insert(1, dot);
                } else {
                    list.insert(0, dot);
                }
            } else if let Some(at) = list.iter().position(|d| d.player == dot.player && d.color == dot.color) {
                list.remove(at);
            }
            self.redraw_cell(h, cx, cy);
        }
        if !item.wall {
            return;
        }
        let one = EeAppMiniMapItem { wall: false, ..*item };
        for (sx, sy) in Self::wall_segments(item, x, y) {
            self.building(h, &one, sx, sy, add);
        }
    }

    /// `MoveUnit(unit, old x, old y, x, y)`: the unit's dot between cells (or its tile within one); -10000 is off
    /// the map.
    pub fn move_unit(
        &mut self,
        h: &mut impl MiniMapHost,
        item: &EeAppMiniMapItem,
        old_x: c_long,
        old_y: c_long,
        x: c_long,
        y: c_long,
    ) {
        if !item.shown || (old_x == x && old_y == y) {
            return;
        }
        let mut dot = EeAppMiniMapUnitDot { player: item.player, color: item.color, flag: item.flag, x: 0, y: 0 };
        let (mut ocx, mut ocy) = (OFF_MAP, OFF_MAP);
        let (mut cx, mut cy) = (OFF_MAP, OFF_MAP);
        if old_x != OFF_MAP {
            (ocx, ocy) = self.item_cell(item.tile_size, old_x, old_y);
        }
        if x != OFF_MAP {
            (cx, cy) = self.item_cell(item.tile_size, x, y);
        }
        let same = |a: &EeAppMiniMapUnitDot, b: &EeAppMiniMapUnitDot| {
            a.player == b.player && a.color == b.color && a.x == b.x && a.y == b.y
        };
        if cx == ocx && cy == ocy {
            dot.x = old_x;
            dot.y = old_y;
            if let Some(d) = self.units[index(ocx, ocy)].iter_mut().find(|d| same(d, &dot)) {
                d.x = x;
                d.y = y;
            }
            return;
        }
        if old_x != OFF_MAP {
            dot.x = old_x;
            dot.y = old_y;
            let list = &mut self.units[index(ocx, ocy)];
            if let Some(at) = list.iter().position(|d| same(d, &dot)) {
                list.remove(at);
            }
            self.redraw_cell(h, ocx, ocy);
        }
        if x != OFF_MAP {
            dot.x = x;
            dot.y = y;
            let list = &mut self.units[index(cx, cy)];
            if list.first().is_some_and(|d| d.flag != 0) {
                list.insert(1, dot);
            } else {
                list.insert(0, dot);
            }
            self.redraw_cell(h, cx, cy);
        }
    }

    /// `Recolor`: every other player's dots take its current colours (buildings `colors`, units `unit_colors`).
    pub fn recolor(&mut self, unit_colors: &[u16; 17], colors: &[u16; 17]) {
        for list in &mut self.dots {
            for d in list.iter_mut().filter(|d| d.player != 0) {
                d.color = colors[d.player as usize];
            }
        }
        for list in &mut self.units {
            for d in list.iter_mut().filter(|d| d.player != 0) {
                d.color = unit_colors[d.player as usize];
            }
        }
    }

    /// `TellTileVisibility`: a tile's sight into its cell, explored-only tiles once, seen ones once per viewer.
    fn tell_visibility(&mut self, h: &mut impl MiniMapHost, x: c_long, y: c_long) {
        let Some(s) = h.tile_sight(x, y) else { return };
        if !s.seen {
            if s.visible {
                self.count_cell(h, x, y, true);
                self.count_cell(h, x, y, false);
            }
        } else {
            if s.all {
                self.count_cell(h, x, y, true);
            }
            let mut i = s.visibility as c_long - 1;
            while i > 0 {
                self.count_cell(h, x, y, true);
                i -= 1;
            }
        }
    }

    /// `Redraw`'s walk (the C++ cleared the unit layer first): every list and count cleared, then each tile's sight,
    /// then each tile's buildings and units, diagonal by diagonal.
    pub fn redraw(&mut self, h: &mut impl MiniMapHost) {
        for i in 0..CELLS {
            self.dots[i].clear();
            self.units[i].clear();
            self.counts[i] = 0;
        }
        let order = diagonal_order(self.width, self.height);
        for &(x, y) in &order {
            self.tell_visibility(h, x, y);
        }
        let mut items = Vec::new();
        for &(x, y) in &order {
            items.clear();
            h.tile_items(x, y, &mut items);
            for item in &items {
                if item.unit {
                    self.move_unit(h, item, OFF_MAP, OFF_MAP, item.x, item.y);
                } else {
                    self.building(h, item, x, y, true);
                }
            }
        }
    }

    /// `GetPixelColor(x, y)`: the lit average of the map vertices under a terrain pixel plus the ambient, or the
    /// nearest vertex's when none falls inside; clamped (black when the nearest is off the map or unlit).
    pub fn pixel_color(&self, h: &mut impl MiniMapHost, light: GeVector, x: c_long, y: c_long) -> [f32; 3] {
        let (fx, fy) = (x as f32, y as f32);
        let mut out = [0.0f32; 3];
        let mut count = 0.0f32;
        let (left, top, right, bottom) = (fx - 0.5, fy - 0.5, fx + 0.5, fy + 0.5);
        let start_x = ceil(apply2(&self.pixel_to_map, fx - 0.5, fy - 0.5).x);
        let mut ty = ceil(apply2(&self.pixel_to_map, fx - 0.5, fy + 0.5).y);
        let end_x = self.pixel_width + start_x;
        let end_y = self.pixel_height + ty;
        while ty < end_y {
            for tx in start_x..end_x {
                if !h.valid_point(u(tx), u(ty)) {
                    continue;
                }
                let p = apply2(&self.map_to_pixel, tx as f32, ty as f32);
                if !(p.x >= left && p.x <= right && p.y >= top && p.y <= bottom) {
                    continue;
                }
                let (vc, t, n) = vertex(h, tx, ty);
                let lit = light.x * n.x + light.y * n.y + light.z * n.z;
                if lit > 0.0 {
                    out[0] += t[0] * vc[0] * lit;
                    out[1] += vc[1] * t[1] * lit;
                    out[2] += vc[2] * t[2] * lit;
                }
                count += 1.0;
            }
            ty += 1;
        }
        if count != 0.0 {
            let scale = 1.0 / count;
            out[0] = out[0] * scale + AMBIENT[0];
            out[1] = AMBIENT[1] + out[1] * scale;
            out[2] = AMBIENT[2] + out[2] * scale;
        } else {
            let p = apply2(&self.pixel_to_map, fx, fy);
            let tx = lrintf(p.x) as c_long;
            let ty = lrintf(p.y) as c_long;
            if !h.valid_point(u(tx), u(ty)) {
                return out;
            }
            let (vc, t, n) = vertex(h, tx, ty);
            let lit = light.x * n.x + light.y * n.y + light.z * n.z;
            if lit <= 0.0 {
                return out;
            }
            out[0] = t[0] * vc[0] * lit + AMBIENT[0];
            out[1] = AMBIENT[1] + vc[1] * t[1] * lit;
            out[2] = AMBIENT[2] + vc[2] * t[2] * lit;
        }
        // GEColor::Normalize (f32::clamp tests below then above, as it: a NaN or -0 kept)
        out.map(|c| c.clamp(0.0, 1.0))
    }

    /// `RedrawPixels(left, right, bottom, top)`: the terrain texture's pixels of the rectangle, packed R5G6B5 into
    /// `bits` (rows of `pitch` pixels).
    #[allow(clippy::too_many_arguments)]
    pub fn redraw_pixels(
        &self,
        h: &mut impl MiniMapHost,
        light: GeVector,
        bits: &mut [u16],
        pitch: usize,
        left: c_long,
        right: c_long,
        bottom: c_long,
        top: c_long,
    ) {
        for y in bottom..top {
            for x in left..right {
                let c = self.pixel_color(h, light, x, y);
                let packed = ee_render::ge::pixel::pack_float(R5G6B5, c[0], c[1], c[2], 1.0);
                bits[pitch * y as usize + x as usize] = packed as u16;
            }
        }
    }

    /// `RedrawRect`'s pixel rectangle of a map rectangle: (left, right, bottom, top) clamped to the texture.
    pub fn pixel_rect(&self, left: c_long, right: c_long, bottom: c_long, top: c_long) -> [c_long; 4] {
        let clamp = |v: f32, low_first: bool| {
            if low_first {
                if v < 0.0 {
                    0
                } else if v > PIXELS {
                    0x80
                } else {
                    floor(v)
                }
            } else if v > PIXELS {
                0x80
            } else if v < 0.0 {
                0
            } else {
                floor(v)
            }
        };
        let min_x = clamp(apply2(&self.map_to_pixel, left as f32, top as f32).x, true);
        let min_y = clamp(apply2(&self.map_to_pixel, left as f32, bottom as f32).y, true);
        let max_x = clamp(apply2(&self.map_to_pixel, right as f32, bottom as f32).x, false);
        let max_y = clamp(apply2(&self.map_to_pixel, right as f32, top as f32).y, false);
        [min_x, max_x, min_y, max_y]
    }
}

/// A vertex's colour, its terrain's colour and its normal.
fn vertex(h: &mut impl MiniMapHost, x: c_long, y: c_long) -> ([f32; 3], [f32; 3], GeVector) {
    let vc = h.vertex_color(u(x), u(y));
    let id = terrain_at(h, u(x), u(y));
    let t = h.terrain_color(id);
    let n = h.normal(u(x), u(y));
    (vc, t, n)
}

/// The tile's terrain at a vertex: the tile above and right of it, else the one to its left, below, below left.
fn terrain_at(h: &mut impl MiniMapHost, x: c_ulong, y: c_ulong) -> c_ulong {
    if h.valid_tile(x, y) {
        return h.texture_set(x, y);
    }
    if h.valid_tile(x.wrapping_sub(1), y) {
        return h.texture_set(x.wrapping_sub(1), y);
    }
    if h.valid_tile(x, y.wrapping_sub(1)) {
        return h.texture_set(x, y.wrapping_sub(1));
    }
    h.texture_set(x.wrapping_sub(1), y.wrapping_sub(1))
}

/// `Redraw`'s tile order on a map of the two side lengths: the diagonals' pairs, then the two edges.
pub fn diagonal_order(width: c_long, height: c_long) -> Vec<(c_long, c_long)> {
    let mut out = Vec::new();
    for i in 0..width {
        let (mut x, mut y) = (i, height + i);
        for _ in 0..height.max(0) {
            out.push((x, y));
            x += 1;
            out.push((x, y));
            y -= 1;
        }
    }
    let (mut x, mut y) = (0, height - 1);
    while x < height {
        out.push((x, y));
        x += 1;
        y -= 1;
    }
    for i in 0..width {
        out.push((height + i, i));
    }
    out
}
