//! `GETerrainMesh`'s queries the simulation makes (`GETerrainMesh.cpp`): the height and normal under a map position,
//! the four map-edge planes, the tile closest to a point or a line, and a line's first hit on the terrain.
//!
//! The mesh stays C++ (its rows, the render arrays, the materials); these functions read its point and tile rows in
//! place through a `TerrainView` the C++ builds per call (`#[repr(C)]` row mirrors, offsets pinned in GERustMath.h).
//! `unsigned long` is `c_ulong` and `long` is `c_long`, so index arithmetic wraps as the C++'s does on each data model.

use std::ffi::{c_long, c_ulong};

use crate::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeVector};
use crate::um::lrintf;

/// `GETerrainMesh::GETMPoint`: height, quantized normal (a pointer into the quantized-vector table), colour index.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct GeTmPoint {
    pub z: f32,
    pub normal: *const GeVector,
    pub color_table_index: u8,
}

/// `GETerrainMesh::GETMPointRow` and `GETMTileRow`: the first map column, the count, the items.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct GeTmRow {
    pub start: c_ulong,
    pub count: c_ulong,
    pub items: *const u8,
}

/// What the queries read of a `GETerrainMesh`: its rows and its four map-edge planes (`mFrontPlane`, `mBackPlane`,
/// `mLeftPlane`, `mRightPlane`, contiguous in the class).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct GeTerrainView {
    pub point_rows: *const GeTmRow,
    pub tile_rows: *const GeTmRow,
    pub tile_row_count: c_ulong,
    pub planes: *const Ge3DPlane,
}

const FRONT: usize = 0;
const BACK: usize = 1;
const LEFT: usize = 2;
const RIGHT: usize = 3;

/// `UMFloorUnsigned` on `unsigned long`: `lrintf` (a `long`), then down when it went up.
fn floor_unsigned(value: f32) -> c_ulong {
    let result = long_lrintf(value) as c_ulong;
    if result as f32 > value {
        result.wrapping_sub(1)
    } else {
        result
    }
}

/// `UMFloor` on `long`.
pub(crate) fn floor(value: f32) -> c_long {
    let result = long_lrintf(value);
    if result as f32 > value {
        result.wrapping_sub(1)
    } else {
        result
    }
}

/// `lrintf` as a `long`: 32 bits on Windows (with the "indefinite" `INT_MIN`), 64 on LP64 (`LONG_MIN`).
fn long_lrintf(value: f32) -> c_long {
    if core::mem::size_of::<c_long>() == 4 {
        lrintf(value) as c_long
    } else {
        let rounded = value.round_ties_even();
        if (-9.223_372e18..9.223_372e18).contains(&rounded) {
            rounded as i64 as c_long
        } else {
            c_long::MIN
        }
    }
}

impl GeTerrainView {
    /// The point at map column `x` of point row `y` (`mPoints[x - mStart]`, unchecked as in the C++).
    ///
    /// # Safety
    /// Row `y` exists and its point `x - start` is in it.
    pub(crate) unsafe fn point(&self, x: c_ulong, y: c_ulong) -> GeTmPoint {
        // SAFETY: the function's contract
        unsafe {
            let row = *self.point_rows.add(y as usize);
            *(row.items as *const GeTmPoint).add(x.wrapping_sub(row.start) as usize)
        }
    }

    unsafe fn plane(&self, index: usize) -> Ge3DPlane {
        // SAFETY: the view's planes are four
        unsafe { *self.planes.add(index) }
    }

    /// `GetIsValidTile`.
    ///
    /// # Safety
    /// The view's tile rows are `tile_row_count` long.
    pub unsafe fn is_valid_tile(&self, x: c_ulong, y: c_ulong) -> bool {
        // SAFETY: the row is read only when y is below the count
        y < self.tile_row_count
            && x.wrapping_sub(unsafe { (*self.tile_rows.add(y as usize)).start })
                < unsafe { (*self.tile_rows.add(y as usize)).count }
    }

    /// `GetMeshZ`: the height over the half of the tile the position is in (split along the 00-11 diagonal).
    ///
    /// # Safety
    /// The tile under the position and the points around it exist (the C++ checks nothing either).
    pub unsafe fn mesh_z(&self, x: f32, y: f32) -> f32 {
        let tx = floor_unsigned(x);
        let ty = floor_unsigned(y);
        let fx = x - tx as f32;
        let fy = y - ty as f32;
        let inverse_x = 1.0 - fx;
        // SAFETY: the function's contract (each point read only where the C++ reads it)
        let (z00, z10, z11) = unsafe {
            (
                self.point(tx, ty).z,
                self.point(tx.wrapping_add(1), ty).z,
                self.point(tx.wrapping_add(1), ty.wrapping_add(1)).z,
            )
        };
        let a = z00 * inverse_x;
        let b = z11 * fx;
        let diagonal = b + a;
        if fy >= fx {
            let t = (fy - fx) / inverse_x;
            // SAFETY: as above
            let z01 = unsafe { self.point(tx, ty.wrapping_add(1)).z };
            return (z01 * inverse_x + b) * t + (1.0 - t) * diagonal;
        }
        let t = fy / fx;
        (z10 * fx + a) * (1.0 - t) + t * diagonal
    }

    /// `GetMeshNormal`: the four surrounding points' normals, interpolated bilinearly.
    ///
    /// # Safety
    /// As `mesh_z`; the points' normals are valid.
    pub unsafe fn mesh_normal(&self, x: f32, y: f32) -> GeVector {
        let tx = floor_unsigned(x);
        let ty = floor_unsigned(y);
        let fx = x - tx as f32;
        let fy = y - ty as f32;
        // SAFETY: the function's contract
        let (n00, n10, n01, n11) = unsafe {
            (
                *self.point(tx, ty).normal,
                *self.point(tx.wrapping_add(1), ty).normal,
                *self.point(tx, ty.wrapping_add(1)).normal,
                *self.point(tx.wrapping_add(1), ty.wrapping_add(1)).normal,
            )
        };
        let ix = 1.0 - fx;
        let bottom = GeVector::new(n00.x * ix + n10.x * fx, n00.y * ix + n10.y * fx, n00.z * ix + n10.z * fx);
        let top = GeVector::new(n01.x * ix + n11.x * fx, n01.y * ix + n11.y * fx, n01.z * ix + n11.z * fx);
        GeVector::new(
            bottom.x * (1.0 - fy) + top.x * fy,
            bottom.y * (1.0 - fy) + top.y * fy,
            bottom.z * (1.0 - fy) + top.z * fy,
        )
    }

    /// `GetClosestTile(point)`: a point outside the map is first moved onto the nearest edge planes, just inside.
    ///
    /// # Safety
    /// The view's planes are valid.
    pub unsafe fn closest_tile_to_point(&self, point: Ge3DPoint) -> (c_long, c_long) {
        let mut p = point;
        // SAFETY: the function's contract
        let (front, back, left, right) =
            unsafe { (self.plane(FRONT), self.plane(BACK), self.plane(LEFT), self.plane(RIGHT)) };
        // the line from the point along the plane's normal, to the plane (a nonzero normal always meets it; the C++
        // would read an uninitialized parameter otherwise, here 0)
        let onto = |p: Ge3DPoint, plane: &Ge3DPlane| {
            let line = Ge3DLine { start: p, direction: plane.normal };
            line.point_at(line.intersects_plane(plane).unwrap_or(0.0))
        };
        if left.is_point_on_or_below(p) {
            p = onto(p, &left);
            p.x += 0.001;
            p.y += 0.001;
        } else if right.is_point_on_or_below(p) {
            p = onto(p, &right);
            p.x -= 0.001;
            p.y -= 0.001;
        }
        if front.is_point_on_or_below(p) {
            p = onto(p, &front);
            p.x += 0.001;
            p.y -= 0.001;
        } else if back.is_point_on_or_below(p) {
            p = onto(p, &back);
            p.x -= 0.001;
            p.y += 0.001;
        }
        (floor(p.x), floor(p.y))
    }

    /// `GetClosestTile(line)`: the tile where the segment (t in 0..1) crosses a map-edge plane onto the map; else the
    /// tile closest to where the line reaches height 0, or to its start.
    ///
    /// # Safety
    /// The view's planes and tile rows are valid.
    pub unsafe fn closest_tile_to_line(&self, line: &Ge3DLine) -> (c_long, c_long) {
        // SAFETY: the function's contract
        let (front, back, left, right) =
            unsafe { (self.plane(FRONT), self.plane(BACK), self.plane(LEFT), self.plane(RIGHT)) };
        let on_segment = |plane: &Ge3DPlane| line.intersects_plane(plane).filter(|t| *t >= 0.0 && *t <= 1.0);
        let mut point = Ge3DPoint::default();
        let mut found = false;
        // SAFETY (the closure): the function's contract
        let valid = |p: Ge3DPoint| unsafe { self.is_valid_tile(floor_unsigned(p.x), floor_unsigned(p.y)) };
        if let Some(t) = on_segment(&left) {
            point = line.point_at(t);
            found = valid(point);
        } else if let Some(t) = on_segment(&right) {
            point = line.point_at(t);
            found = valid(point);
        }
        let front_t = on_segment(&front).filter(|_| !found);
        if let Some(t) = front_t {
            point = line.point_at(t);
            found = valid(point);
        } else if let Some(t) = on_segment(&back).filter(|_| !found) {
            point = line.point_at(t);
            found = valid(point);
        }
        if found {
            return (floor(point.x), floor(point.y));
        }
        let point = match line.solve_for_z_point(0.0) {
            Some((_, p)) => p,
            None => line.start,
        };
        // SAFETY: the function's contract
        unsafe { self.closest_tile_to_point(point) }
    }

    /// `DoesLineIntersectTile`: whether the line hits either triangle of the tile. `distance` is written whenever the
    /// line meets a triangle's plane, hit or not (the C++ writes through its reference).
    ///
    /// # Safety
    /// The view's rows are valid; a valid tile's points around it exist.
    unsafe fn line_hits_tile(&self, line: &Ge3DLine, x: c_ulong, y: c_ulong, distance: &mut Option<f32>) -> bool {
        // SAFETY: the function's contract
        if !unsafe { self.is_valid_tile(x, y) } {
            return false;
        }
        let fx = x as f32;
        let fy = y as f32;
        let next_y = fy + 1.0;
        let next_x = fx + 1.0;
        // SAFETY: a valid tile's points exist (the fourth is read in the second pass, as the C++ reads it)
        let (z00, z01, z10) =
            unsafe { (self.point(x, y).z, self.point(x, y.wrapping_add(1)).z, self.point(x.wrapping_add(1), y).z) };
        let mut p = Ge3DPoint::new(fx, fy, z00);
        let mut q = Ge3DPoint::new(fx, next_y, z01);
        let r = Ge3DPoint::new(next_x, fy, z10);
        let mut result = false;
        for _ in 0..2 {
            let edge_a = GeVector::new(p.x - q.x, p.y - q.y, p.z - q.z);
            let edge_b = GeVector::new(q.x - r.x, q.y - r.y, q.z - r.z);
            let normal = edge_b.cross(edge_a);
            let plane = Ge3DPlane { point: p, normal };
            match line.intersects_plane(&plane) {
                None => result = false,
                Some(t) => {
                    *distance = Some(t);
                    result = true;
                    let hit = line.point_at(t);
                    let e1 = (r.x - q.x, r.y - q.y);
                    let determinant = e1.1 * (hit.x - p.x) - (hit.y - p.y) * e1.0;
                    if determinant == 0.0 {
                        if p.x != hit.x || p.y != hit.y || p.z != hit.z {
                            result = false;
                        }
                        if result {
                            return result;
                        }
                    } else {
                        let inverse = 1.0 / determinant;
                        let e2 = (p.x - q.x, p.y - q.y);
                        let u = (e2.1 * (hit.x - p.x) - (hit.y - p.y) * e2.0) * inverse;
                        if (e2.1 * e1.0 - e1.1 * e2.0) * inverse >= 0.99999 && u >= -0.00001 && u <= 1.0001 {
                            return true;
                        }
                        result = false;
                    }
                }
            }
            p.y = next_y;
            p.z = q.z;
            // SAFETY: as above
            q = Ge3DPoint::new(next_x, next_y, unsafe { self.point(x.wrapping_add(1), y.wrapping_add(1)).z });
        }
        result
    }

    /// `Intersects(line, distance)`: walks the tiles under the line from its start toward its end; true when it hits
    /// the terrain within its length. `distance` is the last value the C++ wrote (None: never written).
    ///
    /// # Safety
    /// The view's rows are valid.
    pub unsafe fn intersects(&self, line: &Ge3DLine) -> (bool, Option<f32>) {
        let mut distance = None;
        let start = line.start;
        let start_x = floor(start.x);
        let start_y = floor(start.y);
        let end = line.point_at(1.0);
        let end_x = floor(end.x).wrapping_sub(start_x).wrapping_abs();
        let end_y = floor(end.y).wrapping_sub(start_y).wrapping_abs();
        let (step_x, offset) = if line.direction.x >= 0.0 { (1, 1.0f32) } else { (-1, 0.0f32) };
        let step_y: c_long = if line.direction.y >= 0.0 { 1 } else { -1 };
        let mut x = start_x;
        let mut y = start_y;
        let mut edge = Ge3DLine { start: Ge3DPoint::default(), direction: GeVector::new(0.0, 1.0, 0.0) };
        // SAFETY: the function's contract
        while !unsafe { self.line_hits_tile(line, x as c_ulong, y as c_ulong, &mut distance) } {
            let dx = x.wrapping_sub(start_x).wrapping_abs();
            let dy = y.wrapping_sub(start_y).wrapping_abs();
            if dx > end_x || dy > end_y {
                return (false, distance);
            }
            edge.start = Ge3DPoint::new(x as f32 + offset, y as f32, 0.0);
            match line.intersects_in_2d(&edge) {
                Some((_, edge_t)) if edge_t >= -0.00001 && edge_t <= 1.0001 => x = x.wrapping_add(step_x),
                _ => y = y.wrapping_add(step_y),
            }
        }
        // the C++ reads its reference: written by the hit just found
        (distance.is_some_and(|d| d <= 1.0), distance)
    }
}

/// `InitializePlanes`: the four vertical map-edge planes (front, back, left, right) from the first and last tile rows.
///
/// # Safety
/// `tile_rows` holds `tile_row_count` (at least one) rows.
pub unsafe fn edge_planes(tile_rows: *const GeTmRow, tile_row_count: c_ulong) -> [Ge3DPlane; 4] {
    // SAFETY: the function's contract
    let (first, last) = unsafe { (*tile_rows, *tile_rows.add(tile_row_count as usize - 1)) };
    let plane = |x: f32, y: f32, nx: f32, ny: f32| {
        let mut normal = GeVector::new(nx, ny, 0.0);
        normal.normalize();
        Ge3DPlane { point: Ge3DPoint::new(x, y, 0.0), normal }
    };
    let last_x = last.start as f32 + 1.0;
    let first_x = first.start as f32 + 1.0;
    let count = tile_row_count as f32;
    [
        plane(last_x, count, 1.0, -1.0),
        plane(first_x, 0.0, -1.0, 1.0),
        plane(first_x, 0.0, 1.0, 1.0),
        plane(last_x, count, -1.0, -1.0),
    ]
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A 2 x 2 point grid at map columns 0..1 (one tile), heights 0, 1, 2, 3.
    struct Mesh {
        points: Vec<Vec<GeTmPoint>>,
        point_rows: Vec<GeTmRow>,
        tile_rows: Vec<GeTmRow>,
        planes: [Ge3DPlane; 4],
        up: Box<GeVector>,
    }

    fn mesh(heights: [[f32; 2]; 2]) -> Mesh {
        let up = Box::new(GeVector::new(0.0, 0.0, 1.0));
        let points: Vec<Vec<GeTmPoint>> = heights
            .iter()
            .map(|row| row.iter().map(|&z| GeTmPoint { z, normal: &*up, color_table_index: 0 }).collect())
            .collect();
        let point_rows =
            points.iter().map(|r| GeTmRow { start: 0, count: 2, items: r.as_ptr() as *const u8 }).collect();
        let tile_rows = vec![GeTmRow { start: 0, count: 1, items: std::ptr::null() }];
        // SAFETY: one tile row
        let planes = unsafe { edge_planes(tile_rows.as_ptr(), 1) };
        Mesh { points, point_rows, tile_rows, planes, up }
    }

    fn view(m: &Mesh) -> GeTerrainView {
        GeTerrainView {
            point_rows: m.point_rows.as_ptr(),
            tile_rows: m.tile_rows.as_ptr(),
            tile_row_count: 1,
            planes: m.planes.as_ptr(),
        }
    }

    #[test]
    fn height_interpolates_over_the_two_triangles() {
        let m = mesh([[0.0, 1.0], [2.0, 3.0]]);
        let v = view(&m);
        assert_eq!(m.points.len(), 2);
        assert_eq!(*m.up, GeVector::new(0.0, 0.0, 1.0));
        // SAFETY: positions inside the one tile
        unsafe {
            assert_eq!(v.mesh_z(0.0, 0.0), 0.0);
            assert_eq!(v.mesh_z(0.5, 0.5), 1.5); // on the diagonal
            assert_eq!(v.mesh_z(0.5, 0.0), 0.5); // the lower triangle
            assert_eq!(v.mesh_z(0.0, 0.5), 1.0); // the upper triangle
            assert_eq!(v.mesh_normal(0.3, 0.6), GeVector::new(0.0, 0.0, 1.0));
        }
    }

    #[test]
    fn valid_tiles_and_edge_planes() {
        let m = mesh([[0.0; 2]; 2]);
        let v = view(&m);
        // SAFETY: the view's rows
        unsafe {
            assert!(v.is_valid_tile(0, 0));
            assert!(!v.is_valid_tile(1, 0));
            assert!(!v.is_valid_tile(0, 1));
            assert!(!v.is_valid_tile(c_ulong::MAX, 0)); // x - start wraps: invalid
        }
        assert_eq!(m.planes[FRONT].point, Ge3DPoint::new(1.0, 1.0, 0.0));
        assert_eq!(m.planes[BACK].point, Ge3DPoint::new(1.0, 0.0, 0.0));
    }

    #[test]
    fn a_line_straight_down_hits_its_tile() {
        let m = mesh([[1.0, 1.0], [1.0, 1.0]]);
        let v = view(&m);
        let line = Ge3DLine { start: Ge3DPoint::new(0.25, 0.5, 11.0), direction: GeVector::new(0.0, 0.0, -20.0) };
        // SAFETY: the view's rows and planes
        unsafe {
            assert_eq!(v.intersects(&line), (true, Some(0.5)));
            assert_eq!(v.closest_tile_to_line(&line), (0, 0));
            let short = Ge3DLine { direction: GeVector::new(0.0, 0.0, -5.0), ..line };
            assert_eq!(v.intersects(&short), (false, Some(2.0)));
        }
    }

    #[test]
    fn floors_follow_the_data_model() {
        assert_eq!(floor(-0.5), -1);
        assert_eq!(floor(2.7), 2);
        assert_eq!(floor_unsigned(2.7), 2);
        // lrintf(-0.5) is 0 (ties to even), above -0.5: one less, which wraps
        assert_eq!(floor_unsigned(-0.5), c_ulong::MAX);
    }
}
