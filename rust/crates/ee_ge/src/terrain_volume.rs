//! `GETerrainMesh::GetTileRowsInVolume` (`GETerrainMesh.cpp`) and the convex hull it rests on (`U2DConvexHull<float>`,
//! `U2DGrahamsAlgorithm<float>`, `lle/U2DPolygon.h`): the tile rows (first and last column, clipped to the map) under
//! the convex hull of a volume's points, with their bounding box. The renderer asks it for the camera frustum's tiles
//! every frame (`UpdateVisibleTerrain`); nothing in the simulation does.
//!
//! The C++'s arithmetic is kept: `float` points, the side test in `double` from `float` differences, the row walk's
//! `long` truncations and its 2D line crossing. One difference, by design: the C++ sorted the points around the pivot
//! with a comparator that deleted collinear points as a side effect of whichever comparisons `std::sort` made (so the
//! survivors depended on the STL); this sort deletes every collinear point but the farthest from the pivot, which is
//! what the comparator meant, and agrees with it whenever the sort compared them.

#![allow(clippy::unnecessary_cast)] // c_long and c_ulong differ per target

use std::collections::BTreeMap;
use std::ffi::{c_long, c_ulong};

use crate::terrain::GeTmRow;
use crate::types::{Ge3DLine, Ge3DPoint, GeVector};

/// `U2DPoint<float>`.
pub type Point2 = (f32, f32);

/// `U2DPoint::operator<`: lower y first, then larger x (the sort order of the hull's input).
fn point_less(a: &Point2, b: &Point2) -> bool {
    a.1 < b.1 || (a.1 == b.1 && a.0 > b.0)
}

/// `U2DPoint::GetSide(start, end)`: 1 left of start -> end, -1 right, 0 on the line (the cross product in `double` of
/// the `float` differences, each converted on its own).
pub fn side(p: &Point2, start: &Point2, end: &Point2) -> c_long {
    let cross =
        f64::from(end.0 - start.0) * f64::from(p.1 - start.1) - f64::from(p.0 - start.0) * f64::from(end.1 - start.1);
    if cross > 0.0 {
        1
    } else if cross < 0.0 {
        -1
    } else {
        0
    }
}

/// `UMAbs` of a `float`.
fn fabs(v: f32) -> f32 {
    if v < 0.0 {
        -v
    } else {
        v
    }
}

/// A Graham point: the point and its index in the input (`GAPoint`).
#[derive(Clone, Copy, Debug)]
struct GaPoint {
    point: Point2,
    index: c_long,
}

/// `GAAngleCompare::IsNearer` without its deletions: `Some(true)` when `a` is nearer the pivot than `b` (per axis),
/// `Some(false)` when farther, `None` when neither axis decides.
fn nearer(pivot: &Point2, a: &Point2, b: &Point2) -> Option<bool> {
    let dx = fabs(a.0 - pivot.0) - fabs(b.0 - pivot.0);
    let dy = fabs(a.1 - pivot.1) - fabs(b.1 - pivot.1);
    if dx < 0.0 || dy < 0.0 {
        Some(true)
    } else if dx > 0.0 || dy > 0.0 {
        Some(false)
    } else {
        None
    }
}

/// `U2DGrahamsAlgorithm<float>::Compute` over at least three distinct points: the hull, counter-clockwise from the
/// lowest point.
fn graham(points: &[Point2]) -> Vec<Point2> {
    let mut ga: Vec<GaPoint> =
        points.iter().enumerate().map(|(i, p)| GaPoint { point: *p, index: i as c_long }).collect();
    // MoveLowestToFront
    let mut lowest = 0;
    for i in 1..ga.len() {
        if point_less(&ga[i].point, &ga[lowest].point) {
            lowest = i;
        }
    }
    ga.swap(0, lowest);
    let pivot = ga[0].point;
    // the angle sort: `a` before `b` when `b` is left of pivot -> a; collinear points are resolved by distance, the
    // nearer one deleted (equal distances: the one with the lower index). The deletions are decided over every
    // collinear pair first, by the points' input indices
    let mut deleted = vec![false; points.len()];
    let rest = &mut ga[1..];
    for i in 0..rest.len() {
        for j in i + 1..rest.len() {
            if side(&rest[j].point, &pivot, &rest[i].point) == 0 {
                match nearer(&pivot, &rest[i].point, &rest[j].point) {
                    Some(true) => deleted[rest[i].index as usize] = true,
                    Some(false) => deleted[rest[j].index as usize] = true,
                    None => {
                        if rest[i].index > rest[j].index {
                            deleted[rest[j].index as usize] = true;
                        } else if rest[i].index != rest[j].index {
                            deleted[rest[i].index as usize] = true;
                        }
                    }
                }
            }
        }
    }
    rest.sort_by(|a, b| {
        let s = side(&b.point, &pivot, &a.point);
        if s > 0 {
            std::cmp::Ordering::Less
        } else if s < 0 {
            std::cmp::Ordering::Greater
        } else {
            // the deleted one goes first (the comparator said "a before b" when it deleted a); a stable order otherwise
            match nearer(&pivot, &a.point, &b.point) {
                Some(true) => std::cmp::Ordering::Less,
                Some(false) => std::cmp::Ordering::Greater,
                None => b.index.cmp(&a.index),
            }
        }
    });
    // RemoveDeleted (the pivot is never deleted)
    let kept: Vec<GaPoint> = ga.into_iter().filter(|g| !deleted[g.index as usize]).collect();
    // Scan
    let mut stack: Vec<GaPoint> = Vec::new();
    if kept.len() >= 2 {
        stack.push(kept[0]);
        stack.push(kept[1]);
        let mut i = 2;
        while i < kept.len() {
            // the C++ indexed `theStack[size - 2]` without a guard: a stack of one point here was undefined
            if stack.len() < 2 || side(&kept[i].point, &stack[stack.len() - 2].point, &stack[stack.len() - 1].point) > 0
            {
                stack.push(kept[i]);
                i += 1;
            } else {
                stack.pop();
            }
        }
    }
    stack.into_iter().map(|g| g.point).collect()
}

/// `U2DConvexHull<float>::SetPoints(points, false)`: the points sorted (`operator<`) and made unique, then the hull of
/// three or more.
pub fn convex_hull(points: &[Point2]) -> Vec<Point2> {
    let mut sorted = points.to_vec();
    sorted.sort_by(|a, b| {
        if point_less(a, b) {
            std::cmp::Ordering::Less
        } else if point_less(b, a) {
            std::cmp::Ordering::Greater
        } else {
            std::cmp::Ordering::Equal
        }
    });
    sorted.dedup();
    if sorted.len() >= 3 {
        graham(&sorted)
    } else {
        sorted
    }
}

/// `(long)` of a `float`: truncation toward zero (the x86-64 conversion's "indefinite" value out of range).
fn trunc_long(v: f32) -> c_long {
    if v.is_nan() || v >= 2_147_483_648.0 || v < -2_147_483_648.0 {
        c_long::MIN
    } else {
        v as i32 as c_long
    }
}

/// The tile column or row of a coordinate (`AddScanLine`): truncated, and one less at or below 0.
fn tile_of(v: f32) -> c_long {
    if v > 0.0 {
        trunc_long(v)
    } else {
        trunc_long(v).wrapping_sub(1)
    }
}

/// `U2DConvexHull<float>::AddScanLine`: walks the tiles under one edge, widening each row it crosses.
pub fn add_scan_line(edge: &Ge3DLine, rows: &mut BTreeMap<c_long, (c_long, c_long)>) {
    let crossing_direction = GeVector::new(0.0, 1.0, 0.0);
    let start_x = tile_of(edge.start.x);
    let start_y = tile_of(edge.start.y);
    let end = edge.point_at(1.0);
    let mut end_x = tile_of(end.x);
    let mut end_y = tile_of(end.y);
    end_x = end_x.wrapping_sub(start_x);
    if end_x < 0 {
        end_x = end_x.wrapping_neg();
    }
    end_y = end_y.wrapping_sub(start_y);
    if end_y < 0 {
        end_y = end_y.wrapping_neg();
    }
    let (step_x, offset) = if edge.direction.x < 0.0 { (-1, 0.0f32) } else { (1, 1.0f32) };
    let step_y = if edge.direction.y < 0.0 { -1 } else { 1 };
    let mut x = start_x;
    let mut y = start_y;
    loop {
        let mut dx = x.wrapping_sub(start_x);
        if dx < 0 {
            dx = dx.wrapping_neg();
        }
        let mut dy = y.wrapping_sub(start_y);
        if dy < 0 {
            dy = dy.wrapping_neg();
        }
        if dx > end_x || dy > end_y {
            break;
        }
        rows.entry(y).and_modify(|r| *r = (r.0.min(x), r.1.max(x))).or_insert((x, x));
        let crossing =
            Ge3DLine { start: Ge3DPoint::new(x as f32 + offset, y as f32, 0.0), direction: crossing_direction };
        match edge.intersects_in_2d(&crossing) {
            Some((_, edge_t)) if edge_t >= -0.00001 && edge_t <= 1.0001 => x = x.wrapping_add(step_x),
            _ => y = y.wrapping_add(step_y),
        }
    }
}

/// `U2DConvexHull<float>::GetScanLines`: the rows under the outline, row -> (first x, last x).
pub fn scan_lines(hull: &[Point2]) -> BTreeMap<c_long, (c_long, c_long)> {
    let mut rows = BTreeMap::new();
    let count = hull.len();
    for i in 0..count {
        let start = hull[i];
        let end = hull[(i + 1) % count];
        let edge = Ge3DLine {
            start: Ge3DPoint::new(start.0, start.1, 0.0),
            direction: GeVector::new(end.0 - start.0, end.1 - start.1, 0.0),
        };
        add_scan_line(&edge, &mut rows);
    }
    rows
}

/// A visible tile row: y, first x, last x.
pub type TileRow = (c_long, c_long, c_long);

/// `GetTileRowsInVolume` over a mesh's tile rows (start, count): the rows the hull covers, clipped to the map, and
/// their bounding box as (min x, min y), (max x, max y) (`0x7fffffff` and `0x80000000` when there is none).
pub fn tile_rows_in_volume(
    volume: &[Ge3DPoint],
    tile_rows: &[(c_ulong, c_ulong)],
) -> (Vec<TileRow>, (c_long, c_long), (c_long, c_long)) {
    let points: Vec<Point2> = volume.iter().map(|p| (p.x, p.y)).collect();
    let rows = scan_lines(&convex_hull(&points));
    let row_count = tile_rows.len() as c_long;
    let mut out = Vec::new();
    let (mut min_x, mut min_y) = (0x7fff_ffff as c_long, 0x7fff_ffff as c_long);
    let (mut max_x, mut max_y) = (i32::MIN as c_long, i32::MIN as c_long);
    for (&y, &(first, last)) in &rows {
        if y >= row_count {
            break;
        }
        if y < 0 {
            continue;
        }
        let (start, count) = tile_rows[y as usize];
        let row_start = start as c_long;
        if last < row_start {
            continue;
        }
        let row_end = row_start.wrapping_add(count as c_long).wrapping_sub(1);
        if first > row_end {
            continue;
        }
        let first = first.max(row_start);
        let last = last.min(row_end);
        out.push((y, first, last));
        min_x = min_x.min(first);
        max_x = max_x.max(last);
        min_y = min_y.min(y);
        max_y = max_y.max(y);
    }
    (out, (min_x, min_y), (max_x, max_y))
}

/// `tile_rows_in_volume` over the C++ rows in place.
///
/// # Safety
/// `rows` holds `count` rows.
pub unsafe fn tile_rows_in_volume_raw(
    volume: &[Ge3DPoint],
    rows: *const GeTmRow,
    count: c_ulong,
) -> (Vec<TileRow>, (c_long, c_long), (c_long, c_long)) {
    let mut starts = Vec::with_capacity(count as usize);
    for i in 0..count as usize {
        // SAFETY: the caller's contract
        let row = unsafe { *rows.add(i) };
        starts.push((row.start, row.count));
    }
    tile_rows_in_volume(volume, &starts)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn side_is_the_sign_of_the_cross_product() {
        assert_eq!(side(&(0.0, 1.0), &(0.0, 0.0), &(1.0, 0.0)), 1);
        assert_eq!(side(&(0.0, -1.0), &(0.0, 0.0), &(1.0, 0.0)), -1);
        assert_eq!(side(&(2.0, 0.0), &(0.0, 0.0), &(1.0, 0.0)), 0);
    }

    #[test]
    fn fewer_than_three_points_come_back_sorted_and_unique() {
        assert_eq!(convex_hull(&[]), Vec::<Point2>::new());
        assert_eq!(convex_hull(&[(1.0, 2.0), (1.0, 2.0)]), vec![(1.0, 2.0)]);
        assert_eq!(convex_hull(&[(1.0, 2.0), (3.0, 0.0)]), vec![(3.0, 0.0), (1.0, 2.0)]);
        // the same y: the larger x first
        assert_eq!(convex_hull(&[(1.0, 0.0), (3.0, 0.0)]), vec![(3.0, 0.0), (1.0, 0.0)]);
    }

    #[test]
    fn the_hull_of_a_square_with_an_inner_point_is_the_square_counter_clockwise() {
        let hull = convex_hull(&[(0.0, 0.0), (2.0, 0.0), (1.0, 1.0), (2.0, 2.0), (0.0, 2.0), (1.0, 1.0)]);
        // the lowest point is (2, 0) (lower y, then larger x), then counter-clockwise
        assert_eq!(hull, vec![(2.0, 0.0), (2.0, 2.0), (0.0, 2.0), (0.0, 0.0)]);
    }

    #[test]
    fn collinear_points_keep_only_the_farthest() {
        let hull = convex_hull(&[(0.0, 0.0), (1.0, 0.0), (2.0, 0.0), (3.0, 0.0), (0.0, 3.0), (1.5, 0.0)]);
        assert_eq!(hull, vec![(3.0, 0.0), (0.0, 3.0), (0.0, 0.0)]);
        // collinear along the hull's side away from the pivot: (1, 1) lies on (2, 0) -> (0, 2) and is not a left turn
        let hull = convex_hull(&[(0.0, 0.0), (2.0, 0.0), (1.0, 1.0), (0.0, 2.0)]);
        assert_eq!(hull, vec![(2.0, 0.0), (0.0, 2.0), (0.0, 0.0)]);
    }

    #[test]
    fn the_scan_of_an_axis_aligned_square_covers_its_rows() {
        let rows = scan_lines(&[(2.0, 0.0), (2.0, 2.0), (0.0, 2.0), (0.0, 0.0)]);
        // the edges walk the tiles they touch, and a coordinate at 0 lies in tile -1 (`x > 0 ? x : x - 1`): the
        // edges on x = 0 and y = 0 cover column and row -1 as the C++'s do
        let expected: BTreeMap<c_long, (c_long, c_long)> =
            [(-1, (-1, 2)), (0, (-1, 2)), (1, (-1, 2)), (2, (-1, 2))].into_iter().collect();
        assert_eq!(rows, expected);
    }

    #[test]
    fn a_diagonal_edge_steps_through_the_tiles_it_crosses() {
        let mut rows = BTreeMap::new();
        let edge = Ge3DLine { start: Ge3DPoint::new(0.5, 0.5, 0.0), direction: GeVector::new(3.0, 3.0, 0.0) };
        add_scan_line(&edge, &mut rows);
        let expected: BTreeMap<c_long, (c_long, c_long)> =
            [(0, (0, 1)), (1, (1, 2)), (2, (2, 3)), (3, (3, 3))].into_iter().collect();
        assert_eq!(rows, expected);
        // a negative coordinate's tile is one less than its truncation
        assert_eq!(tile_of(-0.5), -1);
        assert_eq!(tile_of(0.0), -1);
        assert_eq!(tile_of(2.9), 2);
    }

    #[test]
    fn rows_are_clipped_to_the_map_and_boxed() {
        // a 2 x 2 diamond's tile rows (CreateFlatMesh): y 0: x 1..2, 1: 0..3, 2: 0..3, 3: 1..2
        let tiles = [(1, 2), (0, 4), (0, 4), (1, 2)];
        let volume = [
            Ge3DPoint::new(-1.0, -1.0, 0.0),
            Ge3DPoint::new(5.0, -1.0, 0.0),
            Ge3DPoint::new(5.0, 2.5, 0.0),
            Ge3DPoint::new(-1.0, 2.5, 0.0),
        ];
        let (rows, min, max) = tile_rows_in_volume(&volume, &tiles);
        assert_eq!(rows, vec![(0, 1, 2), (1, 0, 3), (2, 0, 3)]);
        assert_eq!(min, (0, 0));
        assert_eq!(max, (3, 2));
        // nothing on the map
        let far = [Ge3DPoint::new(10.0, 10.0, 0.0), Ge3DPoint::new(12.0, 10.0, 0.0), Ge3DPoint::new(11.0, 12.0, 0.0)];
        let (rows, min, max) = tile_rows_in_volume(&far, &tiles);
        assert!(rows.is_empty());
        assert_eq!(min, (0x7fff_ffff, 0x7fff_ffff));
        assert_eq!(max, (i32::MIN as c_long, i32::MIN as c_long));
    }
}
