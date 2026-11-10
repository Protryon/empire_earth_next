//! `GETerrainMesh`'s edits, fog of war and file codec (`GETerrainMesh.cpp`): `ComputeVertexNormal`,
//! `ComputeShorelines`, `UpdateFogOfWar`, `Load` and `Save`.
//!
//! The random map generator, the map's height edits and the scenario editor call the first two while a game is set up
//! and played, so they are simulation code (RUST_PLAN 2.1): the C++'s `f32` operations in its order, and `unsigned
//! long` index arithmetic that wraps as the C++'s does. The mesh stays C++; these read and write its rows in place
//! through the `GeTerrainView` of `terrain.rs`. The file codec goes field by field through the file's own typed reads
//! and writes (`io.rs`), so the bytes and the error behaviour are the C++'s.

#![allow(clippy::unnecessary_cast)] // c_long and c_ulong differ per target

use std::ffi::{c_long, c_ulong};

use crate::io::{FieldIo, OK};
use crate::terrain::{GeTerrainView, GeTmPoint, GeTmRow};
use crate::types::GeVector;

/// `GETerrainMesh::GETMTile` (sizeof 4).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GeTmTile {
    pub reference_count: u16,
    pub texture_set_id: u8,
    pub water_info: u8,
}

/// `kWaterInfoWaterMask`: every bit but space.
const WATER_MASK: u8 = 0x7f;
/// `kWaterInfoSpace`.
const SPACE: u8 = 0x80;

/// The eight neighbours of a tile and the shoreline bits a land (non-space) neighbour sets (`sNeighbours`).
const NEIGHBOURS: [(c_long, c_long, u8); 8] =
    [(-1, 1, 4), (1, 1, 8), (-1, -1, 2), (1, -1, 16), (0, 1, 12), (1, 0, 24), (0, -1, 18), (-1, 0, 6)];

/// The row `y` of `rows` (unchecked).
///
/// # Safety
/// `rows` holds more than `y` rows.
unsafe fn row(rows: *const GeTmRow, y: c_ulong) -> GeTmRow {
    // SAFETY: the function's contract
    unsafe { *rows.add(y as usize) }
}

/// The point at index `i` of a point row (unchecked).
///
/// # Safety
/// `i` is below the row's count.
unsafe fn point_at(r: &GeTmRow, i: c_ulong) -> *mut GeTmPoint {
    // SAFETY: the function's contract
    unsafe { (r.items as *mut GeTmPoint).add(i as usize) }
}

impl GeTerrainView {
    /// `GetTile`: the tile at map column `x` of tile row `y` (unchecked, `x - start` wrapping as the C++'s).
    ///
    /// # Safety
    /// The tile exists.
    pub unsafe fn tile(&self, x: c_ulong, y: c_ulong) -> *mut GeTmTile {
        // SAFETY: the function's contract
        unsafe {
            let r = row(self.tile_rows, y);
            (r.items as *mut GeTmTile).add(x.wrapping_sub(r.start) as usize)
        }
    }

    /// `GetIsSpaceTile`: false off the map.
    ///
    /// # Safety
    /// The view's tile rows are `tile_row_count` long.
    pub unsafe fn is_space_tile(&self, x: c_ulong, y: c_ulong) -> bool {
        // SAFETY: the tile is read only when it is valid
        unsafe { self.is_valid_tile(x, y) && (*self.tile(x, y)).water_info & SPACE != 0 }
    }

    /// `GetIsWaterTile` (unchecked).
    ///
    /// # Safety
    /// The tile exists.
    unsafe fn is_water_tile(&self, x: c_ulong, y: c_ulong) -> bool {
        // SAFETY: the function's contract
        unsafe { (*self.tile(x, y)).water_info & WATER_MASK != 0 }
    }

    /// `ComputeShorelines`: the tile's new shoreline bits for `SetWaterInfo`, or `None` when the C++ leaves the tile
    /// alone (a land tile of a map without space). On a space map a non-space tile gets 0 and a space tile the bits
    /// of its non-space neighbours; otherwise a water tile gets 1 plus the bits of its land neighbours.
    ///
    /// # Safety
    /// The tile (`x`, `y`) exists.
    pub unsafe fn shoreline(&self, space_map: bool, x: c_ulong, y: c_ulong) -> Option<u8> {
        let neighbour = |i: usize| {
            let (dx, dy, _) = NEIGHBOURS[i];
            ((dx as c_ulong).wrapping_add(x), (dy as c_ulong).wrapping_add(y))
        };
        // SAFETY: the function's contract; neighbours are read only when they are valid
        unsafe {
            let mut shoreline: c_ulong;
            if space_map {
                if !self.is_space_tile(x, y) {
                    return Some(0);
                }
                shoreline = 0;
                for (i, &(_, _, bits)) in NEIGHBOURS.iter().enumerate() {
                    let (nx, ny) = neighbour(i);
                    if self.is_valid_tile(nx, ny) && !self.is_space_tile(nx, ny) {
                        shoreline |= bits as c_ulong;
                    }
                }
            } else {
                if !self.is_water_tile(x, y) {
                    return None;
                }
                shoreline = 1;
                for (i, &(_, _, bits)) in NEIGHBOURS.iter().enumerate() {
                    let (nx, ny) = neighbour(i);
                    if self.is_valid_tile(nx, ny) && !self.is_water_tile(nx, ny) {
                        shoreline |= bits as c_ulong;
                    }
                }
            }
            Some(shoreline as u8)
        }
    }
}

/// `ComputeVertexNormal` before its quantization: the point's normal from its neighbours' heights (a missing
/// neighbour uses the point's own height). The C++ stores `GetQuantizedVersion()` of it.
///
/// # Safety
/// `point_rows` holds `point_row_count` rows and the point (`x`, `y`) exists.
pub unsafe fn vertex_normal(point_rows: *const GeTmRow, point_row_count: c_ulong, x: c_ulong, y: c_ulong) -> GeVector {
    // SAFETY: the function's contract; every other point is read only inside its row
    unsafe {
        let this_row = row(point_rows, y);
        let local = x.wrapping_sub(this_row.start);
        let z = (*point_at(&this_row, local)).z;
        let (mut nw, mut n, mut ne);
        if y.wrapping_add(1) < point_row_count {
            let next = row(point_rows, y + 1);
            let other = x.wrapping_sub(next.start);
            let at = |i: c_ulong| if i < next.count { (*point_at(&next, i)).z } else { z };
            nw = at(other.wrapping_sub(1));
            n = at(other);
            ne = at(other.wrapping_add(1));
        } else {
            nw = z;
            n = z;
            ne = z;
        }
        // the north heights become the differences in place (NE: SW - NE, N: S - N, NW: NW - SE)
        if y.wrapping_sub(1) < point_row_count {
            let previous = row(point_rows, y.wrapping_sub(1));
            let other = x.wrapping_sub(previous.start);
            let at = |i: c_ulong| if i < previous.count { Some((*point_at(&previous, i)).z) } else { None };
            ne = match at(other.wrapping_sub(1)) {
                Some(p) => p - ne,
                None => z - ne,
            };
            n = match at(other) {
                Some(p) => p - n,
                None => z - n,
            };
            nw = match at(other.wrapping_add(1)) {
                Some(p) => nw - p,
                None => nw - z,
            };
        } else {
            nw = nw - z;
            n = z - n;
            ne = z - ne;
        }
        let w =
            if local.wrapping_sub(1) < this_row.count { (*point_at(&this_row, local.wrapping_sub(1))).z } else { z };
        let d = if local.wrapping_add(1) < this_row.count {
            w - (*point_at(&this_row, local.wrapping_add(1))).z
        } else {
            w - z
        };
        nw = nw * 0.70710677;
        ne = ne * 0.70710677;
        GeVector::new(d + ne + nw, ne + n - nw, 5.0)
    }
}

/// What `UpdateFogOfWar` reads and writes besides the tile rows: the visible area and its point arrays
/// (`[side][side]`, indexed `[x - bottom left x][y - bottom left y]`), the render vertices reached through a pointer
/// to the first one's `mVisibility` and the vertex size.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct GeTerrainFog {
    pub visible_row_count: usize,
    pub bottom_left_x: c_ulong,
    pub bottom_left_y: c_ulong,
    pub tile_width: c_ulong,
    pub tile_height: c_ulong,
    pub num_visible_tiles: c_ulong,
    pub side: usize,
    pub terrain_point_visible: *const u8,
    pub water_point_visible: *const u8,
    pub terrain_visibility: *mut f32,
    pub water_visibility: *mut f32,
    pub vertex_stride: usize,
    pub all_visible: bool,
}

/// One tile's share of a point's fog value: 0.25 for several viewers, 0.125 for one.
fn share(count: u16) -> Option<f32> {
    if count > 1 {
        Some(0.25)
    } else if count == 1 {
        Some(0.125)
    } else {
        None
    }
}

/// `UpdateFogOfWar`: sets each visible point's visibility from its four tiles' viewer counts (1 when everything is
/// visible) and returns the visible water and space shares, or `None` when nothing is visible (the C++ returns
/// before it changes anything).
///
/// # Safety
/// The view's tile rows and the fog's arrays are live and large enough for the visible area plus a point each way.
pub unsafe fn fog_of_war(view: &GeTerrainView, fog: &GeTerrainFog) -> Option<(f32, f32)> {
    if fog.visible_row_count == 0 {
        return None;
    }
    let mut water_total = 0.0f32;
    let mut space_total = 0.0f32;
    let end_x = fog.tile_width.wrapping_add(2).wrapping_add(fog.bottom_left_x);
    let end_y = fog.tile_height.wrapping_add(2).wrapping_add(fog.bottom_left_y);
    // SAFETY: the function's contract; a tile is read only when it is valid
    unsafe {
        let count = |x: c_ulong, y: c_ulong| (*view.tile(x, y)).reference_count;
        let mut ty = fog.bottom_left_y;
        let mut j = 0usize;
        while ty < end_y {
            let mut tx = fog.bottom_left_x;
            let mut i = 0usize;
            while tx < end_x {
                let cell = i * fog.side + j;
                if *fog.terrain_point_visible.add(cell) != 0 {
                    let value = if fog.all_visible {
                        1.0
                    } else {
                        let mut value = if view.is_valid_tile(tx.wrapping_sub(1), ty) {
                            share(count(tx.wrapping_sub(1), ty)).unwrap_or(0.0)
                        } else {
                            0.0
                        };
                        for (x, y) in [(tx, ty), (tx.wrapping_sub(1), ty.wrapping_sub(1)), (tx, ty.wrapping_sub(1))] {
                            if view.is_valid_tile(x, y) {
                                if let Some(s) = share(count(x, y)) {
                                    value += s;
                                }
                            }
                        }
                        value
                    };
                    let vertex = cell * fog.vertex_stride;
                    *(fog.terrain_visibility as *mut u8).add(vertex).cast::<f32>() = value;
                    if *fog.water_point_visible.add(cell) != 0 {
                        *(fog.water_visibility as *mut u8).add(vertex).cast::<f32>() = value;
                        water_total += value;
                    }
                    if view.is_space_tile(tx, ty) {
                        space_total += value;
                    }
                }
                tx = tx.wrapping_add(1);
                i += 1;
            }
            ty = ty.wrapping_add(1);
            j += 1;
        }
    }
    Some(if fog.num_visible_tiles != 0 {
        (water_total / fog.num_visible_tiles as f32, space_total / fog.num_visible_tiles as f32)
    } else {
        (0.0, 0.0)
    })
}

/// `Save`: the map size, each tile's texture set and water info, each point's height and colour, the fractal seed;
/// returns the first write error and writes nothing after it.
///
/// # Safety
/// The view's rows and `point_rows` (`point_row_count` rows) are live.
pub unsafe fn save(
    io: &mut impl FieldIo,
    view: &GeTerrainView,
    point_row_count: c_ulong,
    width: c_long,
    height: c_long,
    fractal_seed: c_ulong,
) -> c_long {
    let mut result = io.write_i32(width as i32);
    if result != OK {
        return result;
    }
    result = io.write_i32(height as i32);
    if result != OK {
        return result;
    }
    // SAFETY: the function's contract
    unsafe {
        'tiles: for j in 0..view.tile_row_count {
            let r = row(view.tile_rows, j);
            for i in 0..r.count {
                let tile = *(r.items as *const GeTmTile).add(i as usize);
                result = io.write_u8(tile.texture_set_id);
                if result == OK {
                    result = io.write_u8(tile.water_info);
                }
                if result != OK {
                    break 'tiles;
                }
            }
        }
        let mut j = 0;
        while j < point_row_count && result == OK {
            let r = row(view.point_rows, j);
            for k in 0..r.count {
                let p = *point_at(&r, k);
                result = io.write_f32(p.z);
                if result != OK {
                    break;
                }
                result = io.write_u8(p.color_table_index);
                if result != OK {
                    break;
                }
            }
            j += 1;
        }
    }
    if result == OK {
        result = io.write_u32(fractal_seed as u32);
    }
    result
}

/// `Load`'s first step: the map size, each read through the member's old value (`FSFile::Read(long&)`). The C++
/// builds the flat mesh when both succeed.
pub fn load_size(io: &mut impl FieldIo, width: &mut c_long, height: &mut c_long) -> c_long {
    let mut w = *width as i32;
    let result = io.read_i32(&mut w);
    *width = w as c_long;
    if result != OK {
        return result;
    }
    let mut h = *height as i32;
    let result = io.read_i32(&mut h);
    *height = h as c_long;
    result
}

/// The mesh's members `load_body` updates.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GeTerrainLoad {
    pub space_tile_count: c_ulong,
    pub minimum_z: f32,
    pub maximum_z: f32,
    pub fractal_seed: c_ulong,
    /// The fractal seed was read (successfully or not): the C++ recomputes the fractal from it.
    pub compute_fractal: bool,
}

/// `Load` after the flat mesh: each tile's texture set (an id without a registered set, `valid_sets[id] == 0`,
/// becomes 0) and water info, each point's height (widening the height range) and colour, then the fractal seed.
/// Stops at the first failed read and returns it; what was read before stays. The C++ then recomputes the normals.
///
/// # Safety
/// The view's rows and `point_rows` (`point_row_count` rows) are live and writable.
pub unsafe fn load_body(
    io: &mut impl FieldIo,
    view: &GeTerrainView,
    point_row_count: c_ulong,
    valid_sets: &[u8; 256],
    state: &mut GeTerrainLoad,
) -> c_long {
    let mut result = OK;
    // SAFETY: the function's contract
    unsafe {
        let mut j = 0;
        while j < view.tile_row_count && result == OK {
            let r = row(view.tile_rows, j);
            for i in 0..r.count {
                let tile = (r.items as *mut GeTmTile).add(i as usize);
                let mut id = 0u8;
                result = io.read_u8(&mut id);
                if result != OK {
                    break;
                }
                (*tile).texture_set_id = if valid_sets[id as usize] != 0 { id } else { 0 };
                let mut info = (*tile).water_info;
                result = io.read_u8(&mut info);
                (*tile).water_info = info;
                if result != OK {
                    break;
                }
                if info & SPACE != 0 {
                    state.space_tile_count = state.space_tile_count.wrapping_add(1);
                }
            }
            j += 1;
        }
        let mut j = 0;
        while j < point_row_count && result == OK {
            let r = row(view.point_rows, j);
            for k in 0..r.count {
                let p = point_at(&r, k);
                let mut z = 0.0f32;
                result = io.read_f32(&mut z);
                if result != OK {
                    break;
                }
                (*p).z = z;
                // min()/max() are the C++ macros: (a < b ? a : b), (a > b ? a : b)
                state.minimum_z = if state.minimum_z < z { state.minimum_z } else { z };
                state.maximum_z = if state.maximum_z > z { state.maximum_z } else { z };
                let mut colour = (*p).color_table_index;
                result = io.read_u8(&mut colour);
                (*p).color_table_index = colour;
                if result != OK {
                    break;
                }
            }
            j += 1;
        }
    }
    if result == OK {
        let mut seed = state.fractal_seed as u32;
        result = io.read_u32(&mut seed);
        state.fractal_seed = seed as c_ulong;
        state.compute_fractal = true;
    }
    result
}

// ---- CreateFlatMesh's layout and the water state ----

/// `CreateFlatMesh`'s diamond of `width` x `height` tiles: each tile row's and point row's first and last map column
/// (`GETMTileRow::Initialize(start, end)`), and the edge sums (`mLeftEdge` = x + y on the left edge, `mTopEdge` = y - x,
/// `mRightEdge` = x + y, `mBottomEdge` = x - y).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct FlatMesh {
    pub tile_rows: Vec<(c_ulong, c_ulong)>,
    pub point_rows: Vec<(c_ulong, c_ulong)>,
    pub left_edge: c_long,
    pub top_edge: c_long,
    pub right_edge: c_long,
    pub bottom_edge: c_long,
}

/// The rows of a diamond: `count` rows from (`height - 1`, `first_end`); the start falls to 0 and holds there `hold`
/// times before it rises, the end rises to `count - 1` and holds there `hold` times before it falls.
fn diamond_rows(height: c_long, count: c_ulong, first_end: c_ulong, hold: u32) -> Vec<(c_ulong, c_ulong)> {
    let mut out = Vec::with_capacity(count as usize);
    let mut start = (height as c_ulong).wrapping_sub(1);
    let mut end = first_end;
    let (mut falling, mut rising) = (true, true);
    let (mut bottom, mut top) = (0u32, 0u32);
    for _ in 0..count {
        out.push((start, end));
        if falling {
            if start != 0 {
                start = start.wrapping_sub(1);
            } else {
                bottom += 1;
                if bottom == hold {
                    falling = false;
                }
            }
        } else {
            start = start.wrapping_add(1);
        }
        if rising {
            if end != count.wrapping_sub(1) {
                end = end.wrapping_add(1);
            } else {
                top += 1;
                if top == hold {
                    rising = false;
                }
            }
        } else {
            end = end.wrapping_sub(1);
        }
    }
    out
}

/// `CreateFlatMesh(width, height)`'s layout.
pub fn flat_mesh(width: c_long, height: c_long) -> FlatMesh {
    let tile_count = width.wrapping_add(height) as c_ulong;
    let point_count = tile_count.wrapping_add(1);
    FlatMesh {
        tile_rows: diamond_rows(height, tile_count, height as c_ulong, 1),
        point_rows: diamond_rows(height, point_count, (height as c_ulong).wrapping_add(1), 2),
        left_edge: height.wrapping_sub(1),
        top_edge: height,
        right_edge: width.wrapping_add(height).wrapping_sub(1).wrapping_add(width),
        bottom_edge: height,
    }
}

/// The side of `SetGlobalWaterState`'s water texture grid.
pub const WATER_GRID: usize = 33;

/// `SetGlobalWaterState`'s 33 x 33 grid of water texture indices over `texture_count` textures: a fresh `URandom`
/// (seed 1) draws one in 0..texture_count per cell, `UMFloatToLong` of `Generate() * count`, the count itself clamped
/// to the last.
pub fn water_texture_indices(texture_count: c_long) -> Vec<Vec<c_ulong>> {
    let mut random = ee_random::URandom::new(0);
    let last = texture_count.wrapping_sub(1);
    let mut grid = vec![vec![0 as c_ulong; WATER_GRID]; WATER_GRID];
    for row in grid.iter_mut() {
        for cell in row.iter_mut() {
            let mut index = lrintf(random.generate() * (last.wrapping_add(1)) as f32) as c_long;
            if index == last.wrapping_add(1) {
                index = last;
            }
            *cell = index as c_ulong;
        }
    }
    grid
}

/// `SetWaterColorSet`'s colour set index of a point of height `z`: its scaled depth times the set's last index,
/// truncated (a negative product wraps as the x86-64 conversion does) and clamped to the last entry.
pub fn water_color_index(texture_scale: f32, z: f32, set_size: c_ulong) -> c_ulong {
    let height = texture_scale * z;
    let scaled = height * set_size.wrapping_sub(1) as f32;
    let mut index = if scaled.is_nan() { 0 } else { scaled as i64 as c_ulong };
    if index >= set_size {
        index = set_size.wrapping_sub(1);
    }
    index
}

/// `UpdateWaterAnimation`'s step at clock `now`: the whole periods elapsed since `last` advance the frame (modulo the
/// frame count) and the last time; `None` before a period has passed.
pub fn water_animation_step(
    now: c_ulong,
    last: c_ulong,
    period: c_ulong,
    frame: c_ulong,
    frame_count: c_ulong,
) -> Option<(c_ulong, c_ulong)> {
    let elapsed = now.wrapping_sub(last);
    if elapsed < period {
        return None;
    }
    let frames = elapsed / period;
    Some((frame.wrapping_add(frames) % frame_count, last.wrapping_add(period.wrapping_mul(frames))))
}

use crate::um::lrintf;

#[cfg(test)]
mod tests {
    use super::*;
    use crate::io::{BufferIo, END_OF_FILE, FAILED};
    use crate::types::Ge3DPlane;

    #[test]
    fn the_flat_mesh_is_a_diamond() {
        let m = flat_mesh(2, 3);
        // 5 tile rows: the start falls 2, 1, 0, holds once, then rises; the end rises 3, 4, holds once, then falls
        assert_eq!(m.tile_rows, vec![(2, 3), (1, 4), (0, 4), (0, 3), (1, 2)]);
        // 6 point rows: the start holds at 0 twice, the end at 5 twice
        assert_eq!(m.point_rows, vec![(2, 4), (1, 5), (0, 5), (0, 5), (0, 4), (1, 3)]);
        assert_eq!((m.left_edge, m.top_edge, m.right_edge, m.bottom_edge), (2, 3, 6, 3));
        let m = flat_mesh(1, 1);
        assert_eq!(m.tile_rows, vec![(0, 1), (0, 1)]);
        assert_eq!(m.point_rows, vec![(0, 2), (0, 2), (0, 2)]);
        // the rows agree with the test corpus's diamonds (which the goldens built the way CreateFlatMesh does)
        let d = diamond(5, 3, 1);
        let m = flat_mesh(5, 3);
        for (row, &(start, end)) in d.tile_rows.iter().zip(&m.tile_rows) {
            assert_eq!((row.start, row.count), (start, end - start + 1));
        }
        for (row, &(start, end)) in d.point_rows.iter().zip(&m.point_rows) {
            assert_eq!((row.start, row.count), (start, end - start + 1));
        }
    }

    #[test]
    fn the_water_grid_draws_from_a_fresh_generator_in_range() {
        let g = water_texture_indices(6);
        assert_eq!(g.len(), WATER_GRID);
        assert!(g.iter().all(|r| r.len() == WATER_GRID && r.iter().all(|&i| i < 6)));
        // URandom(1): the first draw is 0x343fd + 0x269ec3 over 2^31; its index at 6 textures
        let mut r = ee_random::URandom::new(1);
        let first = lrintf(r.generate() * 6.0) as c_ulong;
        assert_eq!(g[0][0], first);
        assert_eq!(water_texture_indices(6), g); // the same every time
        assert!(water_texture_indices(1).iter().all(|r| r.iter().all(|&i| i == 0)));
    }

    #[test]
    fn the_water_colour_index_is_the_scaled_depth_clamped() {
        assert_eq!(water_color_index(-0.71428573, -1.0, 8), 5); // 0.714 * 7 = 5.0
        assert_eq!(water_color_index(-0.71428573, -0.1, 8), 0);
        assert_eq!(water_color_index(-0.71428573, -3.0, 8), 7); // past the end: the last
        assert_eq!(water_color_index(-0.71428573, 1.0, 8), 7); // a negative product wraps, then clamps
        assert_eq!(water_color_index(-0.71428573, -1.0, 1), 0);
    }

    #[test]
    fn the_water_animation_advances_by_whole_periods() {
        assert_eq!(water_animation_step(100, 50, 80, 0, 4), None);
        assert_eq!(water_animation_step(130, 50, 80, 0, 4), Some((1, 130)));
        assert_eq!(water_animation_step(300, 50, 80, 3, 4), Some((2, 290))); // 3 periods: frame (3 + 3) % 4
        assert_eq!(water_animation_step(10, 0xffff_fff0, 80, 0, 4), None); // the clock wrapped: 26 elapsed
    }

    /// A diamond mesh as `CreateFlatMesh` builds it, heights and tiles from a small LCG.
    pub(crate) struct Diamond {
        pub points: Vec<Vec<GeTmPoint>>,
        pub tiles: Vec<Vec<GeTmTile>>,
        pub point_rows: Vec<GeTmRow>,
        pub tile_rows: Vec<GeTmRow>,
        pub planes: [Ge3DPlane; 4],
    }

    pub(crate) fn diamond(width: c_ulong, height: c_ulong, seed: u32) -> Diamond {
        let mut state = seed;
        let mut next = || {
            state = state.wrapping_mul(1664525).wrapping_add(1013904223);
            state >> 8
        };
        let rows = |count: c_ulong, first_end: c_ulong, bottom_hold: u32| {
            let mut out = Vec::new();
            let (mut start, mut end) = (height - 1, first_end);
            let (mut falling, mut rising) = (true, true);
            let (mut bottom, mut top) = (0, 0);
            for _ in 0..count {
                out.push((start, end - start + 1));
                if falling {
                    if start != 0 {
                        start -= 1;
                    } else {
                        bottom += 1;
                        if bottom == bottom_hold {
                            falling = false;
                        }
                    }
                } else {
                    start += 1;
                }
                if rising {
                    if end != count - 1 {
                        end += 1;
                    } else {
                        top += 1;
                        if top == bottom_hold {
                            rising = false;
                        }
                    }
                } else {
                    end -= 1;
                }
            }
            out
        };
        let tile_spans = rows(width + height, height, 1);
        let point_spans = rows(width + height + 1, height + 1, 2);
        let tiles: Vec<Vec<GeTmTile>> = tile_spans
            .iter()
            .map(|&(_, n)| {
                (0..n)
                    .map(|_| {
                        let r = next();
                        GeTmTile {
                            reference_count: (r % 4) as u16,
                            texture_set_id: (r >> 4) as u8 % 6,
                            water_info: if r & 0x300 == 0 { 0x80 } else { ((r >> 10) & 1) as u8 },
                        }
                    })
                    .collect()
            })
            .collect();
        let points: Vec<Vec<GeTmPoint>> = point_spans
            .iter()
            .map(|&(_, n)| {
                (0..n)
                    .map(|_| GeTmPoint {
                        z: (next() % 2000) as f32 / 100.0 - 5.0,
                        normal: std::ptr::null(),
                        color_table_index: next() as u8,
                    })
                    .collect()
            })
            .collect();
        let tile_rows: Vec<GeTmRow> = tile_spans
            .iter()
            .zip(&tiles)
            .map(|(&(start, count), t)| GeTmRow { start, count, items: t.as_ptr() as *const u8 })
            .collect();
        let point_rows = point_spans
            .iter()
            .zip(&points)
            .map(|(&(start, count), p)| GeTmRow { start, count, items: p.as_ptr() as *const u8 })
            .collect();
        // SAFETY: at least one tile row
        let planes = unsafe { crate::terrain::edge_planes(tile_rows.as_ptr(), tile_rows.len() as c_ulong) };
        Diamond { points, tiles, point_rows, tile_rows, planes }
    }

    impl Diamond {
        pub(crate) fn view(&self) -> GeTerrainView {
            GeTerrainView {
                point_rows: self.point_rows.as_ptr(),
                tile_rows: self.tile_rows.as_ptr(),
                tile_row_count: self.tile_rows.len() as c_ulong,
                planes: self.planes.as_ptr(),
            }
        }
    }

    #[test]
    fn a_flat_point_points_straight_up() {
        let mut d = diamond(3, 2, 1);
        for r in &mut d.points {
            for p in r {
                p.z = 2.0;
            }
        }
        let v = d.view();
        // SAFETY: every point of the diamond exists
        unsafe {
            for (y, r) in d.point_rows.iter().enumerate() {
                for x in r.start..r.start + r.count {
                    let n = vertex_normal(v.point_rows, d.point_rows.len() as c_ulong, x, y as c_ulong);
                    assert_eq!(n, GeVector::new(0.0, 0.0, 5.0));
                }
            }
        }
    }

    #[test]
    fn a_slope_tilts_the_normal_against_it() {
        let mut d = diamond(4, 4, 1);
        for r in &mut d.point_rows.iter().zip(&mut d.points) {
            for (i, p) in r.1.iter_mut().enumerate() {
                p.z = (r.0.start + i as c_ulong) as f32; // rises with x
            }
        }
        let v = d.view();
        // an inner point: W is one lower, E one higher
        let (x, y) = (4, 4);
        // SAFETY: an inner point
        let n = unsafe { vertex_normal(v.point_rows, d.point_rows.len() as c_ulong, x, y) };
        assert!(n.x < 0.0, "{n:?}");
        assert_eq!(n.z, 5.0);
    }

    #[test]
    fn shorelines_on_water_and_space_maps() {
        let mut d = diamond(3, 3, 5);
        for r in &mut d.tiles {
            for t in r {
                t.water_info = 0;
            }
        }
        // one water tile in the middle row
        let (wy, wi) = (2usize, 1usize);
        d.tiles[wy][wi].water_info = 1;
        let wx = d.tile_rows[wy].start + wi as c_ulong;
        let v = d.view();
        // SAFETY: tiles of the diamond
        unsafe {
            let s = v.shoreline(false, wx, wy as c_ulong).unwrap();
            assert_eq!(s & 1, 1);
            assert_ne!(s & 0x1e, 0, "land around it");
            // a land tile is left alone
            assert_eq!(v.shoreline(false, d.tile_rows[0].start, 0), None);
            // a space map: a non-space tile gets 0
            assert_eq!(v.shoreline(true, wx, wy as c_ulong), Some(0));
        }
        d.tiles[wy][wi].water_info = 0x80;
        let v = d.view();
        // SAFETY: as above
        unsafe {
            let s = v.shoreline(true, wx, wy as c_ulong).unwrap();
            assert_eq!(s & 1, 0);
            assert_ne!(s, 0);
        }
    }

    #[test]
    fn save_then_load_restores_every_field() {
        let d = diamond(5, 4, 9);
        let v = d.view();
        let n = d.point_rows.len() as c_ulong;
        let mut out = BufferIo::writer();
        // SAFETY: the diamond's rows
        assert_eq!(unsafe { save(&mut out, &v, n, 5, 4, 0xdead_beef) }, OK);
        let tiles: usize = d.tiles.iter().map(Vec::len).sum();
        let points: usize = d.points.iter().map(Vec::len).sum();
        assert_eq!(out.bytes.len(), 8 + tiles * 2 + points * 5 + 4);
        assert_eq!(&out.bytes[..8], &[5, 0, 0, 0, 4, 0, 0, 0]);

        let mut back = diamond(5, 4, 1234);
        let bv = back.view();
        let mut io = BufferIo::reader(out.bytes.clone());
        let (mut w, mut h) = (0, 0);
        assert_eq!(load_size(&mut io, &mut w, &mut h), OK);
        assert_eq!((w, h), (5, 4));
        let mut valid = [1u8; 256];
        valid[3] = 0; // an unregistered set becomes 0
        let mut state = GeTerrainLoad::default();
        // SAFETY: the second diamond's rows, writable
        assert_eq!(unsafe { load_body(&mut io, &bv, n, &valid, &mut state) }, OK);
        assert!(state.compute_fractal);
        assert_eq!(state.fractal_seed, 0xdead_beef);
        let spaces = d.tiles.iter().flatten().filter(|t| t.water_info & 0x80 != 0).count();
        assert_eq!(state.space_tile_count, spaces as c_ulong);
        for (a, b) in d.tiles.iter().flatten().zip(back.tiles.iter_mut().flatten()) {
            assert_eq!(b.water_info, a.water_info);
            assert_eq!(b.texture_set_id, if a.texture_set_id == 3 { 0 } else { a.texture_set_id });
        }
        for (a, b) in d.points.iter().flatten().zip(back.points.iter().flatten()) {
            assert_eq!((a.z.to_bits(), a.color_table_index), (b.z.to_bits(), b.color_table_index));
        }
        let lo = d.points.iter().flatten().map(|p| p.z).fold(0.0f32, f32::min);
        assert_eq!(state.minimum_z, lo);
    }

    #[test]
    fn a_short_file_stops_at_the_failed_read() {
        let d = diamond(3, 3, 2);
        let v = d.view();
        let n = d.point_rows.len() as c_ulong;
        let mut out = BufferIo::writer();
        // SAFETY: the diamond's rows
        assert_eq!(unsafe { save(&mut out, &v, n, 3, 3, 7) }, OK);
        // cut inside the first point's height
        let tiles: usize = d.tiles.iter().map(Vec::len).sum();
        let mut bytes = out.bytes[8..].to_vec();
        bytes.truncate(tiles * 2 + 2);
        let back = diamond(3, 3, 99);
        let before = back.points[0][0];
        let mut io = BufferIo::reader(bytes);
        let mut state = GeTerrainLoad { fractal_seed: 5, ..Default::default() };
        // SAFETY: as above
        assert_eq!(unsafe { load_body(&mut io, &back.view(), n, &[1; 256], &mut state) }, END_OF_FILE);
        assert!(!state.compute_fractal);
        assert_eq!(state.fractal_seed, 5);
        assert_eq!(back.points[0][0].z.to_bits(), before.z.to_bits());
        let saved = |t: &GeTmTile| (t.texture_set_id, t.water_info);
        assert!(back.tiles.iter().flatten().map(saved).eq(d.tiles.iter().flatten().map(saved)));
    }

    #[test]
    fn a_failed_write_ends_the_save() {
        let d = diamond(3, 3, 2);
        let mut out = BufferIo { write_limit: 9, ..BufferIo::writer() };
        // SAFETY: the diamond's rows
        assert_eq!(unsafe { save(&mut out, &d.view(), d.point_rows.len() as c_ulong, 3, 3, 7) }, FAILED);
        assert_eq!(out.bytes.len(), 9);
        let mut out = BufferIo { write_limit: 3, ..BufferIo::writer() };
        // SAFETY: as above
        assert_eq!(unsafe { save(&mut out, &d.view(), d.point_rows.len() as c_ulong, 3, 3, 7) }, FAILED);
        assert!(out.bytes.is_empty());
    }

    #[test]
    fn a_failed_size_read_keeps_the_old_value_where_nothing_was_read() {
        let mut io = BufferIo::reader(vec![2, 0, 0, 0]);
        let (mut w, mut h) = (9, 9);
        assert_eq!(load_size(&mut io, &mut w, &mut h), END_OF_FILE);
        assert_eq!((w, h), (2, 9));
    }

    #[test]
    fn fog_counts_the_viewers_of_the_four_tiles() {
        let mut d = diamond(4, 4, 3);
        for t in d.tiles.iter_mut().flatten() {
            t.reference_count = 1;
            t.water_info = 0;
        }
        let v = d.view();
        const SIDE: usize = 16;
        let mut terrain_visible = [[0u8; SIDE]; SIDE];
        let mut water_visible = [[0u8; SIDE]; SIDE];
        let mut terrain = [[[0.0f32; 2]; SIDE]; SIDE];
        let mut water = [[[0.0f32; 2]; SIDE]; SIDE];
        // the point at map (4, 4), the bottom-left of the view at (3, 3): cell [1][1]
        terrain_visible[1][1] = 1;
        water_visible[1][1] = 1;
        let mut fog = GeTerrainFog {
            visible_row_count: 1,
            bottom_left_x: 3,
            bottom_left_y: 3,
            tile_width: 2,
            tile_height: 2,
            num_visible_tiles: 4,
            side: SIDE,
            terrain_point_visible: terrain_visible.as_ptr() as *const u8,
            water_point_visible: water_visible.as_ptr() as *const u8,
            terrain_visibility: terrain.as_mut_ptr() as *mut f32,
            water_visibility: water.as_mut_ptr() as *mut f32,
            vertex_stride: 8,
            all_visible: false,
        };
        // SAFETY: the arrays cover the view
        let shares = unsafe { fog_of_war(&v, &fog) }.unwrap();
        assert_eq!(terrain[1][1][0], 0.5); // four tiles, one viewer each
        assert_eq!(water[1][1][0], 0.5);
        assert_eq!(shares, (0.125, 0.0));
        fog.all_visible = true;
        // SAFETY: as above
        let shares = unsafe { fog_of_war(&v, &fog) }.unwrap();
        assert_eq!(terrain[1][1][0], 1.0);
        assert_eq!(shares, (0.25, 0.0));
        fog.visible_row_count = 0;
        // SAFETY: as above
        assert_eq!(unsafe { fog_of_war(&v, &fog) }, None);
    }
}
