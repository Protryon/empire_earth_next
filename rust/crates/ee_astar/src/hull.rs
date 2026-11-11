//! The AI's hulls: `U2DPolygon<long>` and `U2DConvexHull<long>` (`lle/U2DPolygon.h`, the exe's view), the outline of
//! every AI area (continents, path areas, hills, forests, choke points, towns, herds). The C++ class keeps its
//! members (the points, the bounds, the center, the area) and its vtable; these are its algorithms. The convex hull
//! itself and the scan lines are `ee_ge`'s (`ee_ge::hull`, shared with the engine's `float` hull).
//!
//! The exe's arithmetic, RUST_PLAN 2.1: the distances as `long` (`DistanceToSquared`) or as `float`
//! (`DistanceToSquaredF`: the two `long` squares converted and added; `DistanceToF` its square root), the edge and
//! offset vectors as `GEVector`s normalised by the engine's `Normalize`, the projections `(long)` truncations of
//! `float` sums, the center a `float` mean truncated once, the area a `float` sum.

use std::ffi::c_long;

use ee_ge::hull::{self, HullPoint};
use ee_ge::types::{Ge3DLine, Ge3DPoint, GeVector};
use ee_ge::um::sqrt;

/// `U2DPoint<long>` (the C++ class's layout: two `long`s).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeHullPoint {
    pub x: c_long,
    pub y: c_long,
}

impl EeHullPoint {
    pub const fn new(x: c_long, y: c_long) -> EeHullPoint {
        EeHullPoint { x, y }
    }
    fn pair(self) -> HullPoint<c_long> {
        (self.x, self.y)
    }
    fn of(p: HullPoint<c_long>) -> EeHullPoint {
        EeHullPoint { x: p.0, y: p.1 }
    }
}

/// What `SetPoints` leaves besides the points: the input's bounds (`U2DRectangle<long>`'s members in their order),
/// the center and the area.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeHullShape {
    pub bottom: c_long,
    pub left: c_long,
    pub right: c_long,
    pub top: c_long,
    pub center: EeHullPoint,
    pub area: f32,
}

/// `DistanceToSquared`: the `long` squared distance.
pub fn distance_squared(a: EeHullPoint, b: EeHullPoint) -> c_long {
    let dx = a.x.wrapping_sub(b.x);
    let dy = a.y.wrapping_sub(b.y);
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

/// `DistanceToSquaredF`: `(float)(dx * dx) + dy * dy`, each square a `long` converted on its own.
pub fn distance_squared_f(a: EeHullPoint, b: EeHullPoint) -> f32 {
    let dx = a.x.wrapping_sub(b.x);
    let dy = a.y.wrapping_sub(b.y);
    dx.wrapping_mul(dx) as f32 + dy.wrapping_mul(dy) as f32
}

/// `DistanceToF`: the square root of `DistanceToSquaredF`.
pub fn distance_f(a: EeHullPoint, b: EeHullPoint) -> f32 {
    sqrt(distance_squared_f(a, b))
}

/// A `GEVector` (x, y, 0), normalised.
fn unit(x: f32, y: f32) -> GeVector {
    let mut v = GeVector::new(x, y, 0.0);
    v.normalize();
    v
}

/// `U2DPolygon<long>::ProjectPoint` (slot 4, the exe's): `point` moved to `scale` times its distance from `start`
/// along `start -> end`; unchanged when two of the three are equal. (The C++ also normalised the offset to `point`
/// and never read it.)
pub fn project_point(point: EeHullPoint, scale: f32, start: EeHullPoint, end: EeHullPoint) -> EeHullPoint {
    let mut result = point;
    if start != end && point != start && point != end {
        let direction = unit(end.x as f32 - start.x as f32, end.y as f32 - start.y as f32);
        let distance = distance_f(start, point) * scale;
        result.x = hull::trunc_long(direction.x * distance + start.x as f32);
        result.y = hull::trunc_long(direction.y * distance + start.y as f32);
    }
    result
}

/// `GetClosestPointIndex` (slot 8): the vertex nearest `point` by `long` distance, the first from the end on ties;
/// -1 for no points.
pub fn closest_point_index(points: &[EeHullPoint], point: EeHullPoint) -> c_long {
    let Some(last) = points.last() else { return -1 };
    let mut best = distance_squared(point, *last);
    let mut result = points.len() - 1;
    for i in (0..points.len() - 1).rev() {
        let distance = distance_squared(point, points[i]);
        if distance < best {
            best = distance;
            result = i;
        }
    }
    result as c_long
}

/// `GetClosestPoint` (slot 7): the nearest vertex, (0, 0) for no points.
pub fn closest_point(points: &[EeHullPoint], point: EeHullPoint) -> EeHullPoint {
    match closest_point_index(points, point) {
        -1 => EeHullPoint::default(),
        i => points[i as usize],
    }
}

/// `GetClosestOutlinePoint` (slot 5, the exe's: the dot products and the best distance are `float`s): the point of
/// the outline nearest `point`, by the nearest vertex and the projections onto its two edges.
pub fn closest_outline_point(points: &[EeHullPoint], point: EeHullPoint) -> EeHullPoint {
    let count = points.len() as c_long;
    let index = closest_point_index(points, point);
    let previous = if index > 0 { index - 1 } else { count - 1 };
    let next = if index < count - 1 { index + 1 } else { 0 };
    if !(index >= 0 && index < count && points[index as usize] != point) {
        return point;
    }
    let vertex = points[index as usize];
    if count <= 1 {
        return points[0];
    }
    if count == 2 {
        let to_point = unit(point.x as f32 - vertex.x as f32, point.y as f32 - vertex.y as f32);
        let other = points[previous as usize];
        let edge = unit(other.x as f32 - vertex.x as f32, other.y as f32 - vertex.y as f32);
        let dot = edge.dot(to_point);
        return if dot >= 0.0 { project_point(point, dot, vertex, other) } else { vertex };
    }
    if next == previous {
        return vertex;
    }
    // the offsets as `long` differences converted (the two-point case converts each coordinate first)
    let to_point = unit(point.x.wrapping_sub(vertex.x) as f32, point.y.wrapping_sub(vertex.y) as f32);
    let mut result = EeHullPoint::default();
    let mut best = 0.0f32;
    let mut found = false;
    let before = points[previous as usize];
    let edge = unit(before.x.wrapping_sub(vertex.x) as f32, before.y.wrapping_sub(vertex.y) as f32);
    let dot = edge.dot(to_point);
    if dot >= 0.0 {
        result = project_point(point, dot, vertex, before);
        best = distance_squared_f(point, result);
        found = true;
    }
    let after = points[next as usize];
    let edge = unit(after.x.wrapping_sub(vertex.x) as f32, after.y.wrapping_sub(vertex.y) as f32);
    let dot = edge.dot(to_point);
    if dot >= 0.0 {
        let candidate = project_point(point, dot, vertex, after);
        if best == 0.0 || distance_squared_f(point, candidate) < best {
            result = candidate;
            found = true;
        }
    }
    if !found {
        result = vertex;
    }
    result
}

/// `GetClosestPoints` (slot 6): the closest pair between this outline (`points`) and another's (`other`). Each of
/// this outline's vertices, from the last, keyed by the `float` distance to the other's nearest vertex; the first
/// smallest key (a multimap keeps equal keys in insertion order) gives the pair, refined by the outline points both
/// ways. (0, 0) twice for no points.
///
/// The C++ read `other`'s point -1 when `other` had none (undefined); this returns (0, 0) twice there.
pub fn closest_points(points: &[EeHullPoint], other: &[EeHullPoint]) -> (EeHullPoint, EeHullPoint) {
    let zero = EeHullPoint::default();
    if points.is_empty() || other.is_empty() {
        return (zero, zero);
    }
    let mut best: Option<(f32, usize, usize)> = None;
    for i in (0..points.len()).rev() {
        // `other` has points: the index is one of them
        let j = closest_point_index(other, points[i]) as usize;
        let distance = distance_squared_f(other[j], points[i]);
        if best.is_none_or(|(d, _, _)| distance < d) {
            best = Some((distance, i, j));
        }
    }
    let (_, i, j) = best.expect("points is not empty");
    let other_point = closest_outline_point(other, points[i]);
    let this_point = closest_outline_point(points, other[j]);
    let distance = distance_squared_f(points[i], other_point);
    if distance_squared_f(other[j], this_point) < distance {
        (this_point, other[j])
    } else {
        (points[i], other_point)
    }
}

/// `U2DConvexHull<long>::Contains` (slot 11): `point` is on no outer side of any edge (each edge from point `i - 1`
/// to point `i`, walked from the first); false for no points.
pub fn contains(points: &[EeHullPoint], point: EeHullPoint) -> bool {
    let count = points.len();
    let mut result = count != 0;
    let mut i = 0;
    while result && i < count {
        let end = points[(i + count - 1) % count];
        let start = points[i];
        result = hull::side(&point.pair(), &start.pair(), &end.pair()) <= 0;
        i += 1;
    }
    result
}

/// `U2DConvexHull<long>::SetPoints(points, is_sorted)`: the hull (`Compute`: sorted and made unique unless
/// `is_sorted`, Graham's scan of three or more), the input's bounds (from its last point, extended by the others
/// from the end), the hull's center (`ComputeCenter`: `float` sums of `x / count` from the last point, truncated)
/// and its area (`ComputeArea`: the `float` fan from the first point, from the last triangle; the point count below
/// three points).
pub fn set_points(points: &[EeHullPoint], is_sorted: bool) -> (Vec<EeHullPoint>, EeHullShape) {
    let mut shape = EeHullShape::default();
    if let Some(last) = points.last() {
        shape.top = last.y;
        shape.left = last.x;
        shape.bottom = last.y;
        shape.right = last.x;
        for p in points[..points.len() - 1].iter().rev() {
            shape.left = shape.left.min(p.x);
            shape.right = shape.right.max(p.x);
            shape.top = shape.top.min(p.y);
            shape.bottom = shape.bottom.max(p.y);
        }
    }
    let pairs: Vec<HullPoint<c_long>> = points.iter().map(|p| p.pair()).collect();
    let outline: Vec<EeHullPoint> = hull::convex_hull(&pairs, is_sorted).into_iter().map(EeHullPoint::of).collect();
    shape.center = center(&outline);
    shape.area = area(&outline);
    (outline, shape)
}

/// `U2DPolygon<long>::Sort` (slot 13): sorted (`operator<`, duplicates kept) and Graham's scan of three or more.
pub fn sort(points: &[EeHullPoint]) -> Vec<EeHullPoint> {
    let mut pairs: Vec<HullPoint<c_long>> = points.iter().map(|p| p.pair()).collect();
    hull::sort_points(&mut pairs);
    hull::graham(&pairs).into_iter().map(EeHullPoint::of).collect()
}

/// `U2DPolygon<long>::ComputeCenter` (slot 12, the exe's): the `float` mean, summed from the last point, truncated.
pub fn center(points: &[EeHullPoint]) -> EeHullPoint {
    let count = points.len() as f32;
    let mut x = 0.0f32;
    let mut y = 0.0f32;
    for p in points.iter().rev() {
        x += p.x as f32 / count;
        y += p.y as f32 / count;
    }
    EeHullPoint::new(hull::trunc_long(x), hull::trunc_long(y))
}

/// `U2DConvexHull<long>::ComputeArea` (slot 1, the exe's `float`): the fan of triangles from the first point, from
/// the last; the point count below three points.
pub fn area(points: &[EeHullPoint]) -> f32 {
    if points.len() < 3 {
        return points.len() as f32;
    }
    let first_x = points[0].x as f32;
    let first_y = points[0].y as f32;
    let mut area = 0.0f32;
    for i in (2..points.len()).rev() {
        let j = i - 1;
        // the C++'s `mArea = (...) * 0.5f + mArea`: the term first
        #[allow(clippy::assign_op_pattern)]
        {
            area = ((points[i].x as f32 - first_x) * (points[j].y as f32 - first_y)
                - (points[j].x as f32 - first_x) * (points[i].y as f32 - first_y))
                * 0.5
                + area;
        }
    }
    area
}

/// `U2DConvexHull<long>::GetScanLines` (slot 14): the tile rows under the outline as (y, first x, last x), by y.
/// Each edge starts at its point as `float`s and runs along the `long` difference to the next point.
pub fn scan_lines(points: &[EeHullPoint]) -> Vec<(c_long, c_long, c_long)> {
    let count = points.len();
    let rows = hull::scan_lines_by(count, |i| {
        let start = points[i];
        let end = points[(i + 1) % count];
        Ge3DLine {
            start: Ge3DPoint::new(start.x as f32, start.y as f32, 0.0),
            direction: GeVector::new(end.x.wrapping_sub(start.x) as f32, end.y.wrapping_sub(start.y) as f32, 0.0),
        }
    });
    rows.into_iter().map(|(y, (first, last))| (y, first, last)).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(x: c_long, y: c_long) -> EeHullPoint {
        EeHullPoint::new(x, y)
    }

    fn square() -> Vec<EeHullPoint> {
        // a square with an inner point, a duplicate and a point on its bottom edge
        vec![p(0, 0), p(4, 0), p(4, 4), p(0, 4), p(2, 2), p(2, 2), p(1, 0)]
    }

    #[test]
    fn set_points_keeps_the_outline_counter_clockwise_from_the_lowest_point() {
        let (outline, shape) = set_points(&square(), false);
        // the lowest point is (4, 0) (lower y, then larger x); (1, 0) lies on the ray to (0, 0) and is nearer
        assert_eq!(outline, vec![p(4, 0), p(4, 4), p(0, 4), p(0, 0)]);
        assert_eq!((shape.left, shape.top, shape.right, shape.bottom), (0, 0, 4, 4));
        assert_eq!(shape.center, p(2, 2));
        // the fan from (4, 0) is clockwise in the C++'s sign: negative
        assert_eq!(shape.area, -16.0);
    }

    #[test]
    fn the_bounds_come_from_the_input_and_the_center_is_truncated() {
        let (outline, shape) = set_points(&[p(0, 0), p(1, 0), p(0, 1), p(-3, 7)], false);
        assert_eq!(outline.len(), 3);
        assert_eq!((shape.left, shape.top, shape.right, shape.bottom), (-3, 0, 1, 7));
        // (1 - 3 + 0) / 3 and (0 + 7 + 0) / 3 as float sums, truncated toward zero
        assert_eq!(shape.center, p(0, 2));
        let (outline, shape) = set_points(&[p(5, 5), p(5, 5)], false);
        assert_eq!(outline, vec![p(5, 5)]);
        assert_eq!(shape.area, 1.0);
        assert_eq!(shape.center, p(5, 5));
        // sorted input: no unique pass, fewer than three points stay as they are
        let (outline, shape) = set_points(&[p(5, 5), p(5, 5)], true);
        assert_eq!(outline, vec![p(5, 5), p(5, 5)]);
        assert_eq!(shape.area, 2.0);
        let (outline, shape) = set_points(&[], false);
        assert!(outline.is_empty());
        assert_eq!(shape, EeHullShape::default());
    }

    #[test]
    fn contains_includes_the_edges() {
        let (outline, _) = set_points(&square(), false);
        assert!(contains(&outline, p(2, 2)));
        assert!(contains(&outline, p(4, 2)));
        assert!(contains(&outline, p(0, 0)));
        assert!(!contains(&outline, p(5, 2)));
        assert!(!contains(&outline, p(2, -1)));
        assert!(!contains(&[], p(0, 0)));
    }

    #[test]
    fn the_nearest_vertex_keeps_the_first_from_the_end_on_ties() {
        let points = [p(0, 0), p(2, 0)];
        assert_eq!(closest_point_index(&points, p(1, 0)), 1);
        assert_eq!(closest_point_index(&points, p(0, 1)), 0);
        assert_eq!(closest_point_index(&[], p(0, 1)), -1);
        assert_eq!(closest_point(&points, p(-5, 0)), p(0, 0));
        assert_eq!(closest_point(&[], p(-5, 0)), p(0, 0));
    }

    #[test]
    fn the_outline_point_is_projected_onto_the_nearer_edge() {
        let (outline, _) = set_points(&[p(0, 0), p(10, 0), p(0, 10)], false);
        assert_eq!(outline, vec![p(10, 0), p(0, 10), p(0, 0)]);
        // nearest vertex (0, 0); the offset (3, -4) has dot 0.6 with the edge to (10, 0): 3 tiles along it
        assert_eq!(closest_outline_point(&outline, p(3, -4)), p(3, 0));
        // a vertex itself, and a point beyond a corner (both dots negative: the vertex)
        assert_eq!(closest_outline_point(&outline, p(0, 0)), p(0, 0));
        assert_eq!(closest_outline_point(&outline, p(-2, -2)), p(0, 0));
        // no points: the point itself; one point: that point
        assert_eq!(closest_outline_point(&[], p(7, 7)), p(7, 7));
        assert_eq!(closest_outline_point(&[p(1, 1)], p(7, 7)), p(1, 1));
        // two points: the projection onto the segment, else the nearer end
        assert_eq!(closest_outline_point(&[p(0, 0), p(10, 0)], p(4, 3)), p(4, 0));
        assert_eq!(closest_outline_point(&[p(0, 0), p(10, 0)], p(-4, 3)), p(0, 0));
    }

    #[test]
    fn project_point_leaves_degenerate_cases_alone() {
        assert_eq!(project_point(p(3, 3), 0.5, p(0, 0), p(0, 0)), p(3, 3));
        assert_eq!(project_point(p(0, 0), 0.5, p(0, 0), p(4, 0)), p(0, 0));
        // distance 5 times 0.8 along +x
        assert_eq!(project_point(p(3, 4), 0.8, p(0, 0), p(4, 0)), p(4, 0));
        assert_eq!(distance_f(p(0, 0), p(3, 4)), 5.0);
        assert_eq!(distance_squared(p(1, 1), p(4, 5)), 25);
        assert_eq!(distance_squared_f(p(1, 1), p(4, 5)), 25.0);
    }

    #[test]
    fn closest_points_between_two_squares() {
        let (a, _) = set_points(&[p(0, 0), p(4, 0), p(4, 4), p(0, 4)], false);
        let (b, _) = set_points(&[p(8, 0), p(12, 0), p(12, 4), p(8, 4)], false);
        // (4, 4) and (4, 0) tie at 16 from b; the first inserted, from the last vertex, wins
        assert_eq!(closest_points(&a, &b), (p(4, 4), p(8, 4)));
        assert_eq!(closest_points(&b, &a), (p(8, 0), p(4, 0)));
        assert_eq!(closest_points(&[], &b), (p(0, 0), p(0, 0)));
        assert_eq!(closest_points(&a, &[]), (p(0, 0), p(0, 0)));
    }

    #[test]
    fn the_scan_lines_of_a_square_cover_its_rows() {
        let (outline, _) = set_points(&[p(1, 1), p(3, 1), p(3, 3), p(1, 3)], false);
        let rows = scan_lines(&outline);
        assert_eq!(rows, vec![(1, 1, 3), (2, 1, 3), (3, 1, 3)]);
        assert!(scan_lines(&[]).is_empty());
    }

    #[test]
    fn sort_keeps_duplicates_for_the_scan() {
        // (0, 0) twice: the scan's tie-break deletes the copy with the lower index
        assert_eq!(sort(&[p(0, 1), p(0, 0), p(0, 0)]), vec![p(0, 0), p(0, 1)]);
        assert_eq!(sort(&[p(0, 1), p(0, 0)]), vec![p(0, 0), p(0, 1)]);
    }

    /// The C++'s Graham angle sort for at most 32 points (MSVC's `std::sort` is its insertion sort there), with the
    /// comparator's deletions as side effects on the elements it compares, the value being inserted included.
    fn cpp_graham(points: &[HullPoint<c_long>]) -> Vec<HullPoint<c_long>> {
        #[derive(Clone, Copy)]
        struct Ga {
            point: HullPoint<c_long>,
            index: usize,
            deleted: bool,
        }
        fn less(pivot: HullPoint<c_long>, a: &mut Ga, b: &mut Ga) -> bool {
            let side = hull::side(&b.point, &pivot, &a.point);
            if side > 0 {
                return true;
            }
            if side < 0 {
                return false;
            }
            let dx = (a.point.0 - pivot.0).abs() - (b.point.0 - pivot.0).abs();
            let dy = (a.point.1 - pivot.1).abs() - (b.point.1 - pivot.1).abs();
            if dx < 0 || dy < 0 {
                a.deleted = true;
                return true;
            }
            if dx > 0 || dy > 0 {
                b.deleted = true;
                return false;
            }
            if a.index > b.index {
                b.deleted = true;
            } else if a.index != b.index {
                a.deleted = true;
            }
            false
        }
        let mut v: Vec<Ga> =
            points.iter().enumerate().map(|(i, &point)| Ga { point, index: i, deleted: false }).collect();
        let mut lowest = 0;
        for i in 1..v.len() {
            if hull::point_less(&v[i].point, &v[lowest].point) {
                lowest = i;
            }
        }
        v.swap(0, lowest);
        let pivot = v[0].point;
        // _Insertion_sort_unchecked over [1, n)
        let first = 1;
        let mut mid = first;
        loop {
            mid += 1;
            if mid >= v.len() {
                break;
            }
            let mut val = v[mid];
            let mut front = v[first];
            if less(pivot, &mut val, &mut front) {
                v[first] = front;
                v.copy_within(first..mid, first + 1);
                v[first] = val;
            } else {
                v[first] = front;
                let mut hole = mid;
                loop {
                    let mut prev = v[hole - 1];
                    let goes_before = less(pivot, &mut val, &mut prev);
                    v[hole - 1] = prev;
                    if !goes_before {
                        break;
                    }
                    v[hole] = v[hole - 1];
                    hole -= 1;
                }
                v[hole] = val;
            }
        }
        let kept: Vec<Ga> = v.into_iter().filter(|g| !g.deleted).collect();
        let mut stack: Vec<Ga> = Vec::new();
        if kept.len() >= 2 {
            stack.push(kept[0]);
            stack.push(kept[1]);
            let mut i = 2;
            while i < kept.len() {
                assert!(stack.len() >= 2, "the C++ would index its stack at -1");
                if hull::side(&kept[i].point, &stack[stack.len() - 2].point, &stack[stack.len() - 1].point) > 0 {
                    stack.push(kept[i]);
                    i += 1;
                } else {
                    stack.pop();
                }
            }
        }
        stack.into_iter().map(|g| g.point).collect()
    }

    #[test]
    fn the_hull_equals_the_cpp_comparators_on_crowded_grids() {
        // small grids: many collinear points, every ray from the pivot crowded
        let mut seed: u32 = 12345;
        let mut next = |n: u32| {
            seed = seed.wrapping_mul(1_103_515_245).wrapping_add(12345);
            (seed >> 16) % n
        };
        for case in 0..3000 {
            let count = 3 + next(28) as usize;
            let side = 2 + next(if case % 2 == 0 { 4 } else { 12 });
            let mut points: Vec<HullPoint<c_long>> =
                (0..count).map(|_| (next(side) as c_long - 3, next(side) as c_long + 2)).collect();
            hull::sort_points(&mut points);
            points.dedup();
            if points.len() < 3 {
                continue;
            }
            assert_eq!(hull::graham(&points), cpp_graham(&points), "{points:?}");
        }
    }
}
