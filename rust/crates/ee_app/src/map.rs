//! `EEMap`'s sight, cliffs, terrain painting, water colours, out-of-synch checksum and tile count (src/Empire
//! Earth/Map/EEMap.cpp) over a host of the map's C++ parts: the tiles (their terrain types are virtual C++ calls),
//! the terrain tables, the players' sight and the mini map, and the terrain mesh (ee_ge's object, read and edited
//! directly by the C ABI's host, [`crate::map_ffi`]).
//!
//! The C++ keeps the map's lifetime, its tile and visibility arrays, the region tables, the texture loads
//! (`SetTileTerrain`), the save and the pick lines. Every float is computed as the C++ did (RUST_PLAN 2.1).
#![allow(clippy::unnecessary_cast)]
// c_long and c_ulong are 32 bits on Windows, 64 on LP64
// the negated float tests are the C++'s (a NaN takes the same branch)
#![allow(clippy::neg_cmp_op_on_partial_ord)]

use ee_ge::types::GeVector;
use ee_random::URandom;
use std::os::raw::{c_long, c_ulong};

/// `kTerrainCliff`: the terrain type `UpdateCliffs` gives a steep tile.
pub(crate) const TERRAIN_CLIFF: c_long = 9;
/// `kCliffNormalTolerance`.
const CLIFF_NORMAL_TOLERANCE: f32 = 0.075;
/// `kChecksumScale`.
const CHECKSUM_SCALE: f32 = -1000.0;

/// The map's numbers the bodies read (`RTSMap`'s and `EEMap`'s plain members, names kept).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAppMapDims {
    /// `mSize`: the square's side in tiles
    pub size: c_long,
    /// `mL195610` and `mL195614`: the diamond's two side lengths
    pub l195610: c_long,
    pub l195614: c_long,
    /// `mF1955F4`: the tile height (`GetF1955F4`)
    pub f1955f4: f32,
    /// `mU195650`: the base terrain; `mU19565C`: the cliff terrain
    pub base_terrain: c_long,
    pub cliff_terrain: c_long,
    /// `mU19561C`: the player count
    pub players: c_ulong,
}

/// A terrain table row's numbers (`EETerrainTable`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Terrain {
    pub texture_set: c_long,
    pub terrain_type: c_long,
    pub color_type: c_long,
}

/// The map's C++ parts and the mesh. Coordinates are the C++'s: a tile's `GetTile` takes them unsigned (a negative
/// one is off the map), the mesh's calls take them as `unsigned long`.
pub trait MapHost {
    /// `GetTile(x, y) != NULL`
    fn tile(&mut self, x: c_long, y: c_long) -> bool;
    /// the tile's `mTerrainType` (the tile exists)
    fn terrain_type(&mut self, x: c_long, y: c_long) -> c_long;
    /// `GetTile(x, y)->SetTerrainType(t, (x, y), this)` (the tile exists)
    fn set_terrain_type(&mut self, x: c_long, y: c_long, terrain_type: c_long);
    /// `SetTileTerrain(x, y, set, unload)`
    fn set_tile_terrain(&mut self, x: c_long, y: c_long, texture_set: c_long, unload: bool);
    /// `EETerrainTable::Get(id)`'s numbers (the row exists, as the C++ assumed)
    fn terrain(&mut self, id: c_long) -> Terrain;
    /// `EETerrainTypeTable::Get(color_type)->mColorSetID`
    fn color_set(&mut self, color_type: c_long) -> c_ulong;
    /// `EETerrainTable::sCliffTable[id]`'s first and last terrains
    fn cliff(&mut self, id: c_long) -> (c_long, c_long);
    /// the tile's `GetChecksum(players)` (the tile exists)
    fn tile_checksum(&mut self, x: c_long, y: c_long) -> c_long;

    // the terrain mesh
    fn z(&mut self, x: c_ulong, y: c_ulong) -> f32;
    fn normal(&mut self, x: c_ulong, y: c_ulong) -> GeVector;
    fn texture_set(&mut self, x: c_ulong, y: c_ulong) -> c_ulong;
    fn valid_tile(&mut self, x: c_ulong, y: c_ulong) -> bool;
    fn valid_point(&mut self, x: c_ulong, y: c_ulong) -> bool;
    fn water_tile(&mut self, x: c_ulong, y: c_ulong) -> bool;
    fn space_tile(&mut self, x: c_ulong, y: c_ulong) -> bool;
    fn set_color_set(&mut self, x: c_ulong, y: c_ulong, set: c_ulong);
    fn set_water_color_set(&mut self, x: c_ulong, y: c_ulong);
    /// the mesh's `ReferenceTile` (`on`) or `DereferenceTile`
    fn reference_tile(&mut self, x: c_ulong, y: c_ulong, on: bool);

    // the sight
    /// `gSightUpdatesEnabled`
    fn sight_enabled(&mut self) -> bool;
    /// whether the local player sees through the player's sight (its own, or shared: an ally sharing it)
    fn local_sees(&mut self, shared: bool) -> bool;
    /// `EEGetTilesNearPoint((x, y, 0), radius)`
    fn tiles_near(&mut self, x: f32, y: f32, radius: f32, out: &mut Vec<(c_long, c_long)>);
    /// the player's `Add/RemoveSharedSightAt` (`shared`) or `Add/RemoveOwnSightAt`
    fn sight_at(&mut self, x: c_long, y: c_long, add: bool, shared: bool);
    /// `gMiniMap->CountCellOn` (`on`) or `CountCellOff`
    fn count_cell(&mut self, x: c_long, y: c_long, on: bool);
}

#[allow(clippy::unnecessary_cast)]
fn u(v: c_long) -> c_ulong {
    v as c_ulong
}

/// The C cast `(long)f`: truncation; the x86 conversion's "integer indefinite" (the type's minimum) out of range.
pub fn ftol(f: f32) -> c_long {
    let t = f.trunc();
    if t.is_nan() || t < c_long::MIN as f32 || t >= -(c_long::MIN as f32) {
        c_long::MIN
    } else {
        t as c_long
    }
}

/// `UpdateWater(x, y)`: the four tiles around a vertex re-typed from their terrain, then the vertex's colour set:
/// the water's below sea level, else its terrain's.
pub fn update_water(h: &mut impl MapHost, d: &EeAppMapDims, x: c_long, y: c_long) {
    for ty in y - 1..=y {
        for tx in x - 1..=x {
            if h.tile(tx, ty) {
                let set = h.texture_set(u(tx), u(ty)) as c_long;
                let terrain_type = h.terrain(set).terrain_type;
                h.set_terrain_type(tx, ty, terrain_type);
            }
        }
    }
    let (mut vx, mut vy) = (x, y);
    if !h.valid_tile(u(x), u(y)) {
        if y > d.l195614 && x > d.l195614 {
            vx = x - 1;
            vy = y - 1;
        } else if x > d.l195610 && y <= d.l195614 {
            vx = x - 1;
        } else if y > d.l195610 && x <= d.l195614 {
            vy = y - 1;
        }
    }
    if h.z(u(x), u(y)) < 0.0 {
        h.set_water_color_set(u(x), u(y));
    } else {
        let set = if h.valid_tile(u(vx - 1), u(vy - 1)) {
            h.texture_set(u(vx - 1), u(vy - 1))
        } else {
            h.texture_set(u(vx), u(vy))
        };
        let color_type = h.terrain(set as c_long).color_type;
        let color_set = h.color_set(color_type);
        h.set_color_set(u(x), u(y), color_set);
    }
}

/// `CreateMap`'s fill (after `Create` and the region tables): every point the base terrain's colour set, every tile
/// its texture and terrain type.
pub fn fill_base_terrain(h: &mut impl MapHost, d: &EeAppMapDims) {
    let count = u(d.size) + 1;
    let base = h.terrain(d.base_terrain);
    let color_set = h.color_set(base.color_type);
    for x in 0..count {
        for y in 0..count {
            if h.valid_point(x, y) {
                h.set_color_set(x, y, color_set);
            }
            if !h.valid_tile(x, y) {
                continue;
            }
            h.set_tile_terrain(x as c_long, y as c_long, base.texture_set, false);
            h.set_terrain_type(x as c_long, y as c_long, base.terrain_type);
        }
    }
}

/// `AddSight` (`add`) / `RemoveSight`: the player's own or shared sight of the mesh-flagged tiles near a point; the
/// mini map's counts and the mesh's references too when the local player sees through it.
pub fn sight(h: &mut impl MapHost, x: f32, y: f32, radius: f32, shared: bool, add: bool) {
    if !h.sight_enabled() {
        return;
    }
    let visible = h.local_sees(shared);
    let mut tiles = Vec::new();
    h.tiles_near(x, y, radius, &mut tiles);
    for (tx, ty) in tiles {
        if !h.valid_tile(u(tx), u(ty)) || !h.water_tile(u(tx), u(ty)) {
            continue;
        }
        h.sight_at(tx, ty, add, shared);
        if visible {
            h.count_cell(tx, ty, add);
            h.reference_tile(u(tx), u(ty), add);
        }
    }
}

/// `ComputeChecksum`: the tiles' and corner heights' sums of the next 64 x 64 window; `cursor` advances (past the
/// right edge to the next band's start).
pub fn compute_checksum(h: &mut impl MapHost, d: &EeAppMapDims, cursor: &mut c_ulong) -> c_long {
    let size = u(d.size);
    let start_x = *cursor % size;
    let start_y = *cursor / size;
    let mut sum: c_long = 0;
    let end_x = start_x.wrapping_add(0x40);
    let end_y = start_y.wrapping_add(0x40);
    for y in start_y..end_y {
        for x in start_x..end_x {
            if !h.tile(x as c_long, y as c_long) {
                continue;
            }
            sum = sum.wrapping_add(h.tile_checksum(x as c_long, y as c_long));
            let a = ftol(h.z(x, y + 1) * 1000.0);
            let b = ftol(h.z(x + 1, y + 1) * CHECKSUM_SCALE);
            let c = ftol(h.z(x, y) * CHECKSUM_SCALE);
            let e = ftol(h.z(x + 1, y) * CHECKSUM_SCALE);
            sum = sum.wrapping_add(a.wrapping_sub(b).wrapping_sub(c).wrapping_sub(e));
        }
    }
    if end_x < size {
        *cursor = cursor.wrapping_add(0x40);
    } else {
        *cursor = end_y.wrapping_mul(size);
        if *cursor >= u(d.size.wrapping_mul(d.size)) {
            *cursor = 0;
        }
    }
    sum
}

/// `GetTileCount`: the square minus its two cut-off corner triangles.
pub fn tile_count(d: &EeAppMapDims) -> c_ulong {
    let width = d.l195614 - 1;
    let height = d.l195610 - 1;
    let a = ftol((height + 1).wrapping_mul(height) as f32 * 0.5);
    let b = ftol((width + 1).wrapping_mul(width) as f32 * 0.5);
    u(d.size.wrapping_mul(d.size).wrapping_sub(2_i32.wrapping_mul(1) as c_long * a.wrapping_add(b)))
}

/// `PaintTerrainRect(left, right, top, bottom, terrain, cliff)`: the rectangle's tiles painted, then its vertices
/// recoloured with the colour type `cliff`.
#[allow(clippy::too_many_arguments)]
pub fn paint_terrain_rect(
    h: &mut impl MapHost,
    d: &EeAppMapDims,
    left: c_long,
    right: c_long,
    top: c_long,
    bottom: c_long,
    terrain: c_long,
    cliff: c_long,
) {
    let t = h.terrain(terrain);
    let mut y = top;
    while y >= bottom {
        for x in left..=right {
            if h.tile(x, y) {
                h.set_tile_terrain(x, y, t.texture_set, true);
                h.set_terrain_type(x, y, t.terrain_type);
            }
        }
        y -= 1;
    }
    set_cliff_color_sets(h, d, left, right, top, bottom, cliff);
}

/// `PaintTerrain(x, y, terrain)`: the tile's terrain type, its and its lower-left neighbours' textures, their
/// vertices' colours.
pub fn paint_terrain(h: &mut impl MapHost, d: &EeAppMapDims, x: c_long, y: c_long, terrain: c_long) {
    let t = h.terrain(terrain);
    if !h.tile(x, y) {
        return;
    }
    h.set_terrain_type(x, y, t.terrain_type);
    for tx in x - 1..x + 1 {
        for ty in y - 1..y + 1 {
            if h.tile(tx, ty) {
                h.set_tile_terrain(tx, ty, t.texture_set, true);
            }
        }
    }
    set_cliff_color_sets(h, d, x - 1, x, y, y - 1, t.color_type);
}

/// `SetCliffColorSets(left, right, top, bottom, cliff)`: the rectangle's vertices above water take the colour
/// type's colour set (the bottom row and left column only their inner corners).
pub fn set_cliff_color_sets(
    h: &mut impl MapHost,
    d: &EeAppMapDims,
    left: c_long,
    right: c_long,
    top: c_long,
    bottom: c_long,
    cliff: c_long,
) {
    let set = h.color_set(cliff);
    let mut y = top;
    while y >= bottom {
        for x in left..=right {
            if !h.tile(x, y) {
                continue;
            }
            for i in 0..4 {
                if y == bottom {
                    if x == left {
                        if i != 1 {
                            continue;
                        }
                    } else if i == 2 || i == 3 {
                        continue;
                    }
                } else if x == left && (i == 0 || i == 2) {
                    continue;
                }
                let (px, py) = match i {
                    0 => (x, y + 1),
                    1 => (x + 1, y + 1),
                    2 => (x, y),
                    _ => (x + 1, y),
                };
                if !(-d.f1955f4 > h.z(u(px), u(py))) {
                    h.set_color_set(u(px), u(py), set);
                }
            }
        }
        y -= 1;
    }
}

/// The lowest and highest of a tile's four corners, compared as the C++ did.
fn corners(h: &mut impl MapHost, x: c_ulong, y: c_ulong) -> (f32, f32) {
    let mut min = h.z(x, y);
    let mut max = min;
    for (cx, cy) in [(x, y + 1), (x + 1, y), (x + 1, y + 1)] {
        let z = h.z(cx, cy);
        if z < min {
            min = z;
        }
        if z > max {
            max = z;
        }
    }
    (min, max)
}

fn normalized(x: f32, y: f32, z: f32) -> GeVector {
    let mut v = GeVector { x, y, z };
    v.normalize();
    v
}

/// `UpdateCliffs(left, right, bottom, top)`: steep tiles of the rectangle become cliffs textured from the cliff
/// terrain by slope direction; then tiles with three or more cliff neighbours become cliffs too.
pub fn update_cliffs(h: &mut impl MapHost, d: &EeAppMapDims, left: c_long, right: c_long, bottom: c_long, top: c_long) {
    let facing_y = normalized(0.0, -1.0, 1.0);
    let facing_x = normalized(1.0, 0.0, 1.0);
    let cliff_height = d.f1955f4 * 1.5 - 0.0001;
    let mut random = URandom { seed: 0 };
    let (first, last) = h.cliff(d.cliff_terrain);
    let mut table = [[0 as c_long; 32]; 32];
    for row in table.iter_mut() {
        for entry in row.iter_mut() {
            *entry = random.generate_int(0, (last - first) as i32) as c_long + first;
        }
    }
    for x in u(left)..u(right) {
        for y in u(bottom)..u(top) {
            if !h.tile(x as c_long, y as c_long) {
                continue;
            }
            let (min, max) = corners(h, x, y);
            if !(max >= d.f1955f4 && max - min >= cliff_height) {
                continue;
            }
            let (x, y) = (x as c_long, y as c_long);
            h.set_terrain_type(x, y, TERRAIN_CLIFF);
            for tx in x - 1..x + 1 {
                for ty in y - 1..y + 1 {
                    if !h.tile(tx, ty) {
                        continue;
                    }
                    let mut n = h.normal(u(tx), u(ty));
                    for (nx, ny) in [(tx + 1, ty), (tx, ty + 1), (tx + 1, ty + 1)] {
                        let m = h.normal(u(nx), u(ny));
                        n.x += m.x;
                        n.y += m.y;
                        n.z += m.z;
                    }
                    n = GeVector { x: 0.25 * n.x, y: 0.25 * n.y, z: 0.25 * n.z };
                    let terrain = if 1.0 - facing_y.dot(n) < CLIFF_NORMAL_TOLERANCE {
                        first
                    } else if 1.0 - facing_x.dot(n) < CLIFF_NORMAL_TOLERANCE {
                        last
                    } else {
                        table[(tx % 32) as usize][(ty % 32) as usize]
                    };
                    let set = h.terrain(terrain).texture_set;
                    h.set_tile_terrain(tx, ty, set, true);
                }
            }
            let color_type = h.terrain(first).color_type;
            set_cliff_color_sets(h, d, x - 1, x, y, y - 1, color_type);
        }
    }
    for x in u(left)..u(right) {
        for y in u(bottom)..u(top) {
            if !h.tile(x as c_long, y as c_long) || h.terrain_type(x as c_long, y as c_long) == TERRAIN_CLIFF {
                continue;
            }
            let mut count = 0;
            let space = h.space_tile(x, y);
            for n in 0..4 {
                let nx = x.wrapping_sub(n & 1);
                let ny = y.wrapping_sub(n >> 1);
                if h.tile(nx as c_long, ny as c_long) {
                    let id = h.texture_set(nx, ny);
                    for k in first..=last {
                        if h.terrain(k).texture_set == id as c_long {
                            count += 1;
                            break;
                        }
                    }
                } else if !space {
                    count += 1;
                }
            }
            if count >= 3 {
                h.set_terrain_type(x as c_long, y as c_long, TERRAIN_CLIFF);
            }
        }
    }
}

/// `RemoveFlatCliffs(left, right, bottom, top)`: cliff tiles that are no longer steep go back to the base terrain.
pub fn remove_flat_cliffs(
    h: &mut impl MapHost,
    d: &EeAppMapDims,
    left: c_ulong,
    right: c_ulong,
    bottom: c_ulong,
    top: c_ulong,
) {
    let flat = d.f1955f4 * 1.5;
    for x in left..right {
        for y in bottom..top {
            if !h.tile(x as c_long, y as c_long) {
                continue;
            }
            let (min, max) = corners(h, x, y);
            if !(max - min < flat) || h.terrain_type(x as c_long, y as c_long) != TERRAIN_CLIFF {
                continue;
            }
            paint_terrain(h, d, x as c_long, y as c_long, d.base_terrain);
            let terrain_type = h.terrain(d.base_terrain).terrain_type;
            h.set_terrain_type(x as c_long, y as c_long, terrain_type);
        }
    }
}
