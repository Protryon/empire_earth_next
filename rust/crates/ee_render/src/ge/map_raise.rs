//! The map's terrain edits whole in Rust (the Graphics + LLE cluster, round 1; `RTSMap.cpp` 0x121e7d..0x123389): the
//! scenario editor's raise and smooth of a rectangle (`Raise`) with its ring spreads (`SpreadUp`, `SpreadDown`,
//! `SpreadLeft`, `SpreadRight`), the vertex smoothing (`SmoothVertex`) and the four tiles' upkeep around a changed
//! vertex (`UpdateTiles`), the two tile tests the random map, the ghost units and the wall planner ask
//! (`GetNumRaisedCorners`, `IsTileFlat`), and the selected group's continent outlines (`BuildAreaOutlines`,
//! `AddOutlineEdge`: the tile runs the terrain draws).
//!
//! The terrain mesh is its Rust object (tmobj.rs, through the C entry points the C++ accessors called); what stays
//! the C++'s is the map's tiles (their height member, their units put back on the ground: `PlaceUnitOnGround`) and its
//! water upkeep (`UpdateWater`), both virtuals of `EEMap`, asked through [`EeGeMapHost`]. The arithmetic is the
//! original's in its order (RUST_PLAN 2.1): the editor's actions run in the simulation.

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::um;
use ee_sim::world::map_fields::EeUnitsRtsMapFields;

use crate::ge::tmobj::{ee_ge_tm_is_valid, ee_ge_tm_point, ee_ge_tm_point_edit, ee_ge_tm_tile_edit, EeGeTerrainMesh};

/// `kNegativeHeightEpsilon` (0x441dbc) and `kHeightEpsilon` (EEGlobalConstants): a smoothed height this close to 0
/// is 0.
const NEGATIVE_HEIGHT_EPSILON: f32 = -0.01;
const HEIGHT_EPSILON: f32 = 0.01;
/// `kFlatSlope` (0x441d8c)
const FLAT_SLOPE: f32 = 0.7;

/// The terrain and the map's tiles as the edits use them.
pub trait Terrain {
    fn valid_point(&self, x: c_ulong, y: c_ulong) -> bool;
    fn valid_tile(&self, x: c_ulong, y: c_ulong) -> bool;
    fn z(&self, x: c_ulong, y: c_ulong) -> f32;
    fn set_z(&mut self, x: c_ulong, y: c_ulong, z: f32);
    fn set_water(&mut self, x: c_ulong, y: c_ulong, water: bool);
    fn vertex_normal(&mut self, x: c_ulong, y: c_ulong);
    fn shorelines(&mut self, x: c_ulong, y: c_ulong);
    /// `GetTile(x, y) != NULL`
    fn has_tile(&self, x: c_ulong, y: c_ulong) -> bool;
    /// The tile's `mHeight` set, then each of its units put back on the ground (`PlaceUnitOnGround`)
    fn settle_tile(&mut self, x: c_ulong, y: c_ulong, height: f32);
    /// `UpdateWater(x, y)` (a vertex crossed the sea level)
    fn update_water(&mut self, x: c_long, y: c_long);
}

fn min(a: f32, b: f32) -> f32 {
    if a < b {
        a
    } else {
        b
    }
}

fn max_l(a: c_long, b: c_long) -> c_long {
    if a > b {
        a
    } else {
        b
    }
}

/// `UpdateTiles(x, y)` (0x1225fa): each valid tile of the four around the vertex gets its lowest corner as its height
/// (water below 0), and its units are put back on the ground.
pub fn update_tiles<T: Terrain>(t: &mut T, x: c_long, y: c_long) {
    for ty in y - 1..=y {
        for tx in x - 1..=x {
            let (ux, uy) = (tx as c_ulong, ty as c_ulong);
            if !t.valid_tile(ux, uy) {
                continue;
            }
            let mut height = min(t.z(ux, uy), t.z(ux.wrapping_add(1), uy));
            height = min(height, t.z(ux, uy.wrapping_add(1)));
            height = min(height, t.z(ux.wrapping_add(1), uy.wrapping_add(1)));
            t.set_water(ux, uy, height < 0.0);
            t.settle_tile(ux, uy, height);
        }
    }
}

/// `SmoothVertex(x, y, from_x, from_y, slope)`: the vertex brought within the slope (times the distance) of its
/// neighbour, copied at slope 0; -1 raised, 1 lowered or copied, 0 unchanged. A change updates the tiles and, when
/// the vertex crosses or stays below the sea level, the water.
pub fn smooth_vertex<T: Terrain>(
    t: &mut T,
    x: c_ulong,
    y: c_ulong,
    from_x: c_ulong,
    from_y: c_ulong,
    slope: f32,
) -> c_long {
    let mut result = 0;
    if !(t.valid_point(x, y) && t.valid_point(from_x, from_y)) {
        return result;
    }
    let mut old_z = 0.0;
    if slope == 0.0 {
        old_z = t.z(x, y);
        let from = t.z(from_x, from_y);
        t.set_z(x, y, from);
        result = 1;
    } else {
        let dy = y.wrapping_sub(from_y) as c_long;
        let dx = x.wrapping_sub(from_x) as c_long;
        let slope = um::sqrt((dy.wrapping_mul(dy)) as f32 + (dx.wrapping_mul(dx)) as f32) * slope;
        let z = t.z(x, y);
        let from = t.z(from_x, from_y);
        if z < from {
            let target = from - slope;
            if z < target {
                old_z = t.z(x, y);
                t.set_z(x, y, if target > NEGATIVE_HEIGHT_EPSILON && target < HEIGHT_EPSILON { 0.0 } else { target });
                result = -1;
            }
        } else {
            let target = from + slope;
            if z > target {
                old_z = t.z(x, y);
                t.set_z(x, y, if target > NEGATIVE_HEIGHT_EPSILON && target < HEIGHT_EPSILON { 0.0 } else { target });
                result = 1;
            }
        }
    }
    if result != 0 {
        update_tiles(t, x as c_long, y as c_long);
        if (old_z < 0.0 && t.z(x, y) >= 0.0) || t.z(x, y) < 0.0 {
            t.update_water(x as c_long, y as c_long);
        }
    }
    result
}

/// The rings' heights: `height` scaled down by the step over `height * slope` steps, the full height at the last.
struct Steps {
    height: f32,
    slope: f32,
    count: u32,
    step: u32,
}

impl Steps {
    fn new(height: f32, slope: f32) -> Steps {
        let steps = height / (height * slope);
        Steps { height, slope, count: um::floor_unsigned(steps), step: 1 }
    }
    fn next(&mut self) -> f32 {
        if self.step == self.count {
            self.height
        } else {
            let h = self.step as f32 * self.height * self.slope;
            self.step += 1;
            h
        }
    }
}

/// `SpreadDown` (0x121e7d): the rows below the raised area smoothed ring by ring until nothing changes; the deepest
/// ring that moved a corner kept in `ring_left` / `ring_right` (`mL195600`, `mL195604`); the lowest row reached.
pub fn spread_down<T: Terrain>(
    t: &mut T,
    f: &mut EeUnitsRtsMapFields,
    x: c_long,
    y: c_long,
    end: c_long,
    height: f32,
    slope: f32,
) -> c_long {
    let mut steps = Steps::new(height, slope);
    let mut ring = 0;
    loop {
        let mut changed = false;
        ring += 1;
        let h = steps.next();
        let left = x - ring;
        let right = y + ring + 1;
        let row = end - ring;
        for column in left..=right {
            let (cx, cy) = (column as c_ulong, row as c_ulong);
            if column == left {
                if smooth_vertex(t, cx, cy, (column + 1) as c_ulong, (row + 1) as c_ulong, h) != 0 {
                    if ring > f.mL195600 {
                        f.mL195600 = ring;
                    }
                    changed = true;
                }
            } else if column == right {
                if smooth_vertex(t, cx, cy, (column - 1) as c_ulong, (row + 1) as c_ulong, h) != 0 {
                    if ring > f.mL195604 {
                        f.mL195604 = ring;
                    }
                    changed = true;
                }
            } else if smooth_vertex(t, cx, cy, cx, (row + 1) as c_ulong, h) != 0 {
                changed = true;
            }
        }
        if !changed {
            break;
        }
    }
    end - ring
}

/// `SpreadUp`: the rows above, ring by ring; the last ring that moved a corner kept (overwritten, not the deepest);
/// the highest row reached.
pub fn spread_up<T: Terrain>(
    t: &mut T,
    f: &mut EeUnitsRtsMapFields,
    x: c_long,
    y: c_long,
    end: c_long,
    height: f32,
    slope: f32,
) -> c_long {
    let mut steps = Steps::new(height, slope);
    let mut ring = 0;
    loop {
        let mut changed = false;
        ring += 1;
        let h = steps.next();
        let left = x - ring;
        let right = y + ring + 1;
        let row = end + ring + 1;
        for column in left..=right {
            let (cx, cy) = (column as c_ulong, row as c_ulong);
            if column == left {
                if smooth_vertex(t, cx, cy, (column + 1) as c_ulong, (row - 1) as c_ulong, h) != 0 {
                    f.mL195600 = ring;
                    changed = true;
                }
            } else if column == right {
                if smooth_vertex(t, cx, cy, (column - 1) as c_ulong, (row - 1) as c_ulong, h) != 0 {
                    f.mL195604 = ring;
                    changed = true;
                }
            } else if smooth_vertex(t, cx, cy, cx, (row - 1) as c_ulong, h) != 0 {
                changed = true;
            }
        }
        if !changed {
            break;
        }
    }
    ring + end
}

/// `SpreadLeft`: the columns left of the area until nothing changes and the corner rings are covered; the leftmost
/// column reached.
pub fn spread_left<T: Terrain>(
    t: &mut T,
    f: &EeUnitsRtsMapFields,
    x: c_long,
    y: c_long,
    end: c_long,
    height: f32,
    slope: f32,
) -> c_long {
    let mut steps = Steps::new(height, slope);
    let mut ring = 0;
    let mut changed = true;
    while changed || f.mL195600 > ring {
        changed = false;
        ring += 1;
        let h = steps.next();
        let start = ring + y + 1;
        let bottom = end - ring;
        let column = x - ring;
        let cx = column as c_ulong;
        let mut row = start;
        while row >= bottom {
            let cy = row as c_ulong;
            let from = if row == start {
                (row - 1) as c_ulong
            } else if row == bottom {
                (row + 1) as c_ulong
            } else {
                cy
            };
            if smooth_vertex(t, cx, cy, (column + 1) as c_ulong, from, h) != 0 {
                changed = true;
            }
            row -= 1;
        }
    }
    x - max_l(f.mL195600, ring)
}

/// `SpreadRight`: the columns right of the area, likewise; the rightmost column reached.
pub fn spread_right<T: Terrain>(
    t: &mut T,
    f: &EeUnitsRtsMapFields,
    x: c_long,
    y: c_long,
    end: c_long,
    height: f32,
    slope: f32,
) -> c_long {
    let mut steps = Steps::new(height, slope);
    let mut ring = 0;
    let mut changed = true;
    let mut bottom = end;
    while changed || f.mL195604 > ring {
        changed = false;
        ring += 1;
        bottom -= 1;
        let h = steps.next();
        let start = ring + y + 1;
        let column = ring + x + 1;
        let cx = column as c_ulong;
        let mut row = start;
        while row >= bottom {
            let cy = row as c_ulong;
            let from = if row == start {
                (row - 1) as c_ulong
            } else if row == bottom {
                (row + 1) as c_ulong
            } else {
                cy
            };
            if smooth_vertex(t, cx, cy, (column - 1) as c_ulong, from, h) != 0 {
                changed = true;
            }
            row -= 1;
        }
    }
    if f.mL195604 > ring {
        return f.mL195604 + x;
    }
    x + ring
}

/// `Raise(left, right, top, bottom, height, slope, edge height, edge slope, raise)` (0x123389): with `raise`, the
/// rectangle's first vertex set to the height; every vertex smoothed against its upper and left neighbours, then the
/// edges spread; the normals and shorelines around recomputed; the changed rectangle written back.
#[allow(clippy::too_many_arguments)]
pub fn raise<T: Terrain>(
    t: &mut T,
    f: &mut EeUnitsRtsMapFields,
    bounds: &mut [c_long; 4],
    height: f32,
    slope: f32,
    edge_height: f32,
    edge_slope: f32,
    raise: bool,
) {
    let [left, right, top, bottom] = *bounds;
    let mut done = !raise;
    f.mL195600 = 0;
    f.mL195604 = 0;
    let mut y = top + 1;
    while y >= bottom {
        for x in left..=right + 1 {
            let (ux, uy) = (x as c_ulong, y as c_ulong);
            if !t.valid_point(ux, uy) {
                continue;
            }
            if !done {
                let old_z = t.z(ux, uy);
                t.set_z(ux, uy, height);
                update_tiles(t, x, y);
                if (old_z < 0.0 && t.z(ux, uy) >= 0.0) || t.z(ux, uy) < 0.0 {
                    t.update_water(x, y);
                }
                done = true;
            }
            if x == left && y == top + 1 {
            } else if x == left {
                smooth_vertex(t, ux, uy, ux, (y + 1) as c_ulong, slope);
                smooth_vertex(t, ux, uy, (x + 1) as c_ulong, (y + 1) as c_ulong, slope);
            } else if y == top + 1 {
                smooth_vertex(t, ux, uy, (x - 1) as c_ulong, uy, slope);
            } else {
                smooth_vertex(t, ux, uy, (x - 1) as c_ulong, (y + 1) as c_ulong, slope);
                smooth_vertex(t, ux, uy, (x - 1) as c_ulong, uy, slope);
                smooth_vertex(t, ux, uy, ux, (y + 1) as c_ulong, slope);
            }
        }
        y -= 1;
    }
    let new_top = spread_up(t, f, left, right, top, edge_height, edge_slope);
    let new_bottom = spread_down(t, f, left, right, bottom, edge_height, edge_slope);
    let new_left = spread_left(t, f, left, top, bottom, edge_height, edge_slope);
    let new_right = spread_right(t, f, right, top, bottom, edge_height, edge_slope);
    for ny in new_bottom - 1..=new_top + 1 {
        for nx in new_left - 1..=new_right + 1 {
            if t.valid_point(nx as c_ulong, ny as c_ulong) {
                t.vertex_normal(nx as c_ulong, ny as c_ulong);
            }
        }
    }
    for sy in new_bottom - 1..=new_top + 1 {
        for sx in new_left - 1..=new_right + 1 {
            if t.valid_tile(sx as c_ulong, sy as c_ulong) {
                t.shorelines(sx as c_ulong, sy as c_ulong);
            }
        }
    }
    *bounds = [new_left, new_right, new_top, new_bottom];
}

/// `GetNumRaisedCorners(x, y)` (0x1222ed): the tile's corners at least 0.4 above its lowest (0 off the map).
pub fn raised_corners<T: Terrain>(t: &T, x: c_ulong, y: c_ulong) -> c_long {
    if !t.has_tile(x, y) {
        return 0;
    }
    let z00 = t.z(x, y);
    let z10 = t.z(x.wrapping_add(1), y);
    let z01 = t.z(x, y.wrapping_add(1));
    let z11 = t.z(x.wrapping_add(1), y.wrapping_add(1));
    let lowest = min(z00, min(z10, min(z01, z11)));
    [z00, z10, z01, z11].iter().filter(|&&z| z - lowest >= 0.4).count() as c_long
}

/// `IsTileFlat(x, y)`: the tile's corners differ by at most 0.7 times the map's tile size (false off the map).
pub fn tile_flat<T: Terrain>(t: &T, tile_size: f32, x: c_ulong, y: c_ulong) -> bool {
    if !t.has_tile(x, y) {
        return false;
    }
    let mut low = t.z(x, y);
    let mut high = low;
    for z in [t.z(x.wrapping_add(1), y), t.z(x, y.wrapping_add(1)), t.z(x.wrapping_add(1), y.wrapping_add(1))] {
        if z < low {
            low = z;
        }
        if z > high {
            high = z;
        }
    }
    high - low <= FLAT_SLOPE * tile_size
}

/// A `U2DRectangle<long>` in the C++'s member order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGeAreaRect {
    pub bottom: c_long,
    pub left: c_long,
    pub right: c_long,
    pub top: c_long,
}

impl EeGeAreaRect {
    /// `Contains(point)`: inside, the edges included
    fn contains(&self, x: c_long, y: c_long) -> bool {
        x >= self.left && x <= self.right && y >= self.top && y <= self.bottom
    }
}

/// `AddOutlineEdge(start, end, continent)`: one run; on a space map the runs of the edge's tiles on the continent.
fn outline_edge(
    start: (c_long, c_long),
    end: (c_long, c_long),
    continent: u16,
    space: bool,
    continent_at: &mut dyn FnMut(c_long, c_long) -> u16,
    run: &mut dyn FnMut([c_long; 4]),
) {
    if !space {
        run([start.0, start.1, end.0, end.1]);
        return;
    }
    let (mut x, mut y) = start;
    let (mut run_x, mut run_y) = (x, y);
    let (dx, dy, count) = if start.0 == end.0 { (0, 1, end.1 - start.1) } else { (1, 0, end.0 - start.0) };
    let mut in_run = false;
    for _ in 0..=count {
        let here = continent_at(x, y);
        if !in_run {
            if here == continent {
                run_x = x;
                run_y = y;
                in_run = true;
            }
        } else if here != continent {
            run([run_x, run_y, x, y]);
            in_run = false;
        }
        x += dx;
        y += dy;
    }
    if in_run {
        run([run_x, run_y, x - dx, y - dy]);
    }
}

/// `BuildAreaOutlines(areas, continents)`: every unit edge of each area not inside another area of its continent
/// that does not share the edge's line, as runs (the top edges, the bottom, the left, the right, area by area).
pub fn area_outlines(
    areas: &[EeGeAreaRect],
    continents: &[u16],
    space: bool,
    continent_at: &mut dyn FnMut(c_long, c_long) -> u16,
    run: &mut dyn FnMut([c_long; 4]),
) {
    let n = areas.len();
    for i in 0..n {
        let a = areas[i];
        let c = continents[i];
        // the edge from `s` to `e` is covered by another area of the continent whose `side` differs
        let covered = |s: (c_long, c_long), e: (c_long, c_long), other: &dyn Fn(&EeGeAreaRect) -> bool| {
            (0..n).any(|j| {
                j != i
                    && continents[j] == c
                    && areas[j].contains(s.0, s.1)
                    && areas[j].contains(e.0, e.1)
                    && other(&areas[j])
            })
        };
        for x in a.left..a.right {
            let (s, e) = ((x, a.top), (x + 1, a.top));
            if !covered(s, e, &|o| o.top != a.top) {
                outline_edge(s, e, c, space, continent_at, run);
            }
        }
        for x in a.left..a.right {
            let (s, e) = ((x, a.bottom), (x + 1, a.bottom));
            if !covered(s, e, &|o| o.bottom != a.bottom) {
                outline_edge(s, e, c, space, continent_at, run);
            }
        }
        for y in a.top..a.bottom {
            let (s, e) = ((a.left, y), (a.left, y + 1));
            if !covered(s, e, &|o| o.left != a.left) {
                outline_edge(s, e, c, space, continent_at, run);
            }
        }
        for y in a.top..a.bottom {
            let (s, e) = ((a.right, y), (a.right, y + 1));
            if !covered(s, e, &|o| o.right != a.right) {
                outline_edge(s, e, c, space, continent_at, run);
            }
        }
    }
}

// ---- the C interface (RTSMap.cpp)

/// The map's tiles and water for the edits: the map (`EEMap*`) and its mesh's Rust object, then the C++'s calls.
#[repr(C)]
pub struct EeGeMapHost {
    pub map: *mut c_void,
    pub mesh: *mut EeGeTerrainMesh,
    /// `GetTile(x, y) != NULL`
    pub has_tile: extern "C" fn(map: *mut c_void, x: c_ulong, y: c_ulong) -> bool,
    /// the tile's `mHeight`, then `PlaceUnitOnGround` of each unit on it
    pub settle_tile: extern "C" fn(map: *mut c_void, x: c_ulong, y: c_ulong, height: f32),
    /// `UpdateWater(x, y)`
    pub update_water: extern "C" fn(map: *mut c_void, x: c_long, y: c_long),
}

struct Cpp<'a>(&'a EeGeMapHost);

impl Terrain for Cpp<'_> {
    fn valid_point(&self, x: c_ulong, y: c_ulong) -> bool {
        // SAFETY: the map's live mesh
        unsafe { ee_ge_tm_is_valid(self.0.mesh, x, y, false) }
    }
    fn valid_tile(&self, x: c_ulong, y: c_ulong) -> bool {
        // SAFETY: as above
        unsafe { ee_ge_tm_is_valid(self.0.mesh, x, y, true) }
    }
    fn z(&self, x: c_ulong, y: c_ulong) -> f32 {
        // SAFETY: as above; the point exists (the C++ read it alike)
        unsafe { (*ee_ge_tm_point(self.0.mesh, x, y)).z }
    }
    fn set_z(&mut self, x: c_ulong, y: c_ulong, z: f32) {
        // SAFETY: as above
        unsafe { ee_ge_tm_point_edit(self.0.mesh, x, y, 0, 0, z) }
    }
    fn set_water(&mut self, x: c_ulong, y: c_ulong, water: bool) {
        // SAFETY: as above
        unsafe { ee_ge_tm_tile_edit(self.0.mesh, x, y, 5, water as c_ulong) }
    }
    fn vertex_normal(&mut self, x: c_ulong, y: c_ulong) {
        // SAFETY: as above
        unsafe { ee_ge_tm_point_edit(self.0.mesh, x, y, 4, 0, 0.0) }
    }
    fn shorelines(&mut self, x: c_ulong, y: c_ulong) {
        // SAFETY: as above
        unsafe { ee_ge_tm_tile_edit(self.0.mesh, x, y, 7, 0) }
    }
    fn has_tile(&self, x: c_ulong, y: c_ulong) -> bool {
        (self.0.has_tile)(self.0.map, x, y)
    }
    fn settle_tile(&mut self, x: c_ulong, y: c_ulong, height: f32) {
        (self.0.settle_tile)(self.0.map, x, y, height)
    }
    fn update_water(&mut self, x: c_long, y: c_long) {
        (self.0.update_water)(self.0.map, x, y)
    }
}

/// `RTSMap::Raise`; `bounds` is left, right, top, bottom (read and written), `fields` the map's record.
///
/// # Safety
/// The host's map and mesh are live; the pointers valid.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ge_map_raise(
    host: *const EeGeMapHost,
    fields: *mut EeUnitsRtsMapFields,
    bounds: *mut [c_long; 4],
    height: f32,
    slope: f32,
    edge_height: f32,
    edge_slope: f32,
    raise_first: bool,
) {
    // SAFETY: the caller's contract
    unsafe { raise(&mut Cpp(&*host), &mut *fields, &mut *bounds, height, slope, edge_height, edge_slope, raise_first) }
}

/// `RTSMap::GetNumRaisedCorners`.
///
/// # Safety
/// As [`ee_ge_map_raise`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_map_raised_corners(host: *const EeGeMapHost, x: c_ulong, y: c_ulong) -> c_long {
    // SAFETY: the caller's contract
    raised_corners(&Cpp(unsafe { &*host }), x, y)
}

/// `RTSMap::IsTileFlat` (`tile_size` is `mF1955F4`).
///
/// # Safety
/// As [`ee_ge_map_raise`].
#[no_mangle]
pub unsafe extern "C" fn ee_ge_map_tile_flat(host: *const EeGeMapHost, tile_size: f32, x: c_ulong, y: c_ulong) -> bool {
    // SAFETY: the caller's contract
    tile_flat(&Cpp(unsafe { &*host }), tile_size, x, y)
}

/// `BuildAreaOutlines`: `count` areas and their continents; `space` is `GetIsSpaceMap()`; each run (start x, y, end
/// x, y) handed to `run`, a tile's continent asked of `continent` (`gContinentManager.GetContinent`, non-space).
///
/// # Safety
/// The arrays hold `count` entries.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_map_area_outlines(
    context: *mut c_void,
    areas: *const EeGeAreaRect,
    continents: *const u16,
    count: usize,
    space: bool,
    continent: extern "C" fn(context: *mut c_void, x: c_long, y: c_long) -> u16,
    run: extern "C" fn(context: *mut c_void, run: *const c_long),
) {
    if count == 0 {
        return;
    }
    // SAFETY: the caller's contract
    let (areas, continents) =
        unsafe { (std::slice::from_raw_parts(areas, count), std::slice::from_raw_parts(continents, count)) };
    area_outlines(areas, continents, space, &mut |x, y| continent(context, x, y), &mut |r| run(context, r.as_ptr()));
}

#[cfg(test)]
#[path = "map_raise_tests.rs"]
mod tests;
