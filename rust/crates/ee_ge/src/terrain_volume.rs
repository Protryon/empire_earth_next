//! `GETerrainMesh::GetTileRowsInVolume` (`GETerrainMesh.cpp`) and the convex hull it rests on (`U2DConvexHull<float>`,
//! `U2DGrahamsAlgorithm<float>`, `lle/U2DPolygon.h`): the tile rows (first and last column, clipped to the map) under
//! the convex hull of a volume's points, with their bounding box. The renderer asks it for the camera frustum's tiles
//! every frame (`UpdateVisibleTerrain`); nothing in the simulation does.
//!
//! The hull and the row walk are `hull.rs`'s, shared with the AI's `long` hull (`ee_astar` hull.rs). One difference,
//! by design: the C++ sorted the points around the pivot with a comparator that deleted collinear points as a side
//! effect of whichever comparisons `std::sort` made (so with `float` points the survivors depended on the STL); the
//! shared sort deletes every collinear point but the farthest from the pivot, which is what the comparator meant, and
//! agrees with it whenever the sort compared them.

#![allow(clippy::unnecessary_cast)] // c_long and c_ulong differ per target

use std::collections::BTreeMap;
use std::ffi::{c_long, c_ulong};

use crate::hull;
use crate::terrain::GeTmRow;
use crate::types::{Ge3DLine, Ge3DPoint, GeVector};

/// `U2DPoint<float>`.
pub type Point2 = (f32, f32);

/// `U2DPoint::GetSide(start, end)` of `float` points (`hull::side`).
pub fn side(p: &Point2, start: &Point2, end: &Point2) -> c_long {
    hull::side(p, start, end)
}

/// `U2DConvexHull<float>::SetPoints(points, false)`: the points sorted (`operator<`) and made unique, then the hull of
/// three or more (`hull::convex_hull`).
pub fn convex_hull(points: &[Point2]) -> Vec<Point2> {
    hull::convex_hull(points, false)
}

/// `U2DConvexHull<float>::AddScanLine` (`hull::add_scan_line`).
pub fn add_scan_line(edge: &Ge3DLine, rows: &mut BTreeMap<c_long, (c_long, c_long)>) {
    hull::add_scan_line(edge, rows)
}

/// `U2DConvexHull<float>::GetScanLines`: the rows under the outline, row -> (first x, last x); each edge from its
/// point along the `float` difference to the next.
pub fn scan_lines(outline: &[Point2]) -> BTreeMap<c_long, (c_long, c_long)> {
    let count = outline.len();
    hull::scan_lines_by(count, |i| {
        let start = outline[i];
        let end = outline[(i + 1) % count];
        Ge3DLine {
            start: Ge3DPoint::new(start.0, start.1, 0.0),
            direction: GeVector::new(end.0 - start.0, end.1 - start.1, 0.0),
        }
    })
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
        assert_eq!(hull::tile_of(-0.5), -1);
        assert_eq!(hull::tile_of(0.0), -1);
        assert_eq!(hull::tile_of(2.9), 2);
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
