//! The convex hull of `lle/U2DPolygon.h` (`U2DConvexHull<T>::Compute`, `U2DGrahamsAlgorithm<T>`, `GetScanLines`),
//! shared by its two instantiations: the engine's `float` one (the terrain volume's, `terrain_volume.rs`; only the
//! renderer asks it) and the AI's `long` one (`ee_astar` hull.rs: every AI area's hull, simulation code).
//!
//! The C++'s arithmetic is kept: the points in `T`, the side test in `double` from `T` differences, the row walk's
//! `long` truncations and its 2D line crossing. The angle sort: the C++ sorted the points around the pivot with a
//! comparator that deleted the nearer of two collinear points as a side effect of the comparisons `std::sort` made.
//! Here every collinear pair is decided first and the sort is a plain one. For `long` points the side test is exact,
//! so the comparator is a strict weak order (the angle around the lowest point, then the distance from it) and a
//! sort compares every two neighbours of its result: the C++ deleted exactly the points this deletes (all but the
//! farthest of each ray) and its order is the only one. For `float` points a rounded side test can break that order;
//! there the C++'s survivors depended on the STL (renderer only).

use std::collections::BTreeMap;
use std::ffi::c_long;

use crate::types::{Ge3DLine, Ge3DPoint, GeVector};

/// A coordinate type of `U2DPoint<T>`: `f32` or `c_long`.
pub trait HullCoord: Copy + PartialEq + PartialOrd {
    const ZERO: Self;
    /// `a - b` in `T` (wrapping for `long`).
    fn minus(self, other: Self) -> Self;
    /// `(double)` of a value.
    fn wide(self) -> f64;
    /// `UMAbs`.
    fn magnitude(self) -> Self;
}

impl HullCoord for f32 {
    const ZERO: f32 = 0.0;
    fn minus(self, other: f32) -> f32 {
        self - other
    }
    fn wide(self) -> f64 {
        f64::from(self)
    }
    fn magnitude(self) -> f32 {
        if self < 0.0 {
            -self
        } else {
            self
        }
    }
}

impl HullCoord for c_long {
    const ZERO: c_long = 0;
    fn minus(self, other: c_long) -> c_long {
        self.wrapping_sub(other)
    }
    fn wide(self) -> f64 {
        self as f64
    }
    fn magnitude(self) -> c_long {
        if self < 0 {
            self.wrapping_neg()
        } else {
            self
        }
    }
}

/// `U2DPoint<T>` as (x, y).
pub type HullPoint<T> = (T, T);

/// `U2DPoint::operator<`: lower y first, then larger x (the sort order of the hull's input).
pub fn point_less<T: HullCoord>(a: &HullPoint<T>, b: &HullPoint<T>) -> bool {
    a.1 < b.1 || (a.1 == b.1 && a.0 > b.0)
}

/// `U2DPoint::GetSide(start, end)`: 1 left of start -> end, -1 right, 0 on the line (the cross product in `double` of
/// the `T` differences, each converted on its own).
pub fn side<T: HullCoord>(p: &HullPoint<T>, start: &HullPoint<T>, end: &HullPoint<T>) -> c_long {
    let cross = end.0.minus(start.0).wide() * p.1.minus(start.1).wide()
        - p.0.minus(start.0).wide() * end.1.minus(start.1).wide();
    if cross > 0.0 {
        1
    } else if cross < 0.0 {
        -1
    } else {
        0
    }
}

/// A Graham point: the point and its index in the input (`GAPoint`).
#[derive(Clone, Copy, Debug)]
struct GaPoint<T> {
    point: HullPoint<T>,
    index: usize,
}

/// `GAAngleCompare::IsNearer` without its deletions: `Some(true)` when `a` is nearer the pivot than `b` (per axis),
/// `Some(false)` when farther, `None` when neither axis decides.
fn nearer<T: HullCoord>(pivot: &HullPoint<T>, a: &HullPoint<T>, b: &HullPoint<T>) -> Option<bool> {
    let dx = a.0.minus(pivot.0).magnitude().minus(b.0.minus(pivot.0).magnitude());
    let dy = a.1.minus(pivot.1).magnitude().minus(b.1.minus(pivot.1).magnitude());
    if dx < T::ZERO || dy < T::ZERO {
        Some(true)
    } else if dx > T::ZERO || dy > T::ZERO {
        Some(false)
    } else {
        None
    }
}

/// `U2DGrahamsAlgorithm<T>::Compute`: the hull of three or more points, counter-clockwise from the lowest point;
/// fewer come back unchanged (the C++ left its output alone).
pub fn graham<T: HullCoord>(points: &[HullPoint<T>]) -> Vec<HullPoint<T>> {
    if points.len() < 3 {
        return points.to_vec();
    }
    let mut ga: Vec<GaPoint<T>> = points.iter().enumerate().map(|(i, p)| GaPoint { point: *p, index: i }).collect();
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
                    Some(true) => deleted[rest[i].index] = true,
                    Some(false) => deleted[rest[j].index] = true,
                    None => {
                        if rest[i].index > rest[j].index {
                            deleted[rest[j].index] = true;
                        } else if rest[i].index != rest[j].index {
                            deleted[rest[i].index] = true;
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
    let kept: Vec<GaPoint<T>> = ga.into_iter().filter(|g| !deleted[g.index]).collect();
    // Scan
    let mut stack: Vec<GaPoint<T>> = Vec::new();
    if kept.len() >= 2 {
        stack.push(kept[0]);
        stack.push(kept[1]);
        let mut i = 2;
        while i < kept.len() {
            // the C++ indexed `theStack[size - 2]` without a guard: a stack of one point here was undefined (with an
            // exact side test it cannot happen: the second point is the first by angle with no collinear one left)
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

/// `std::sort` of the points by `operator<` (a total order on distinct points: every sort gives the same result).
pub fn sort_points<T: HullCoord>(points: &mut [HullPoint<T>]) {
    points.sort_by(|a, b| {
        if point_less(a, b) {
            std::cmp::Ordering::Less
        } else if point_less(b, a) {
            std::cmp::Ordering::Greater
        } else {
            std::cmp::Ordering::Equal
        }
    });
}

/// `U2DConvexHull<T>::Compute(inIsSorted)`'s points: unless `is_sorted`, sorted (`operator<`) and made unique
/// (`std::unique`, `operator==`), then the hull of three or more.
pub fn convex_hull<T: HullCoord>(points: &[HullPoint<T>], is_sorted: bool) -> Vec<HullPoint<T>> {
    let mut sorted = points.to_vec();
    if !is_sorted {
        sort_points(&mut sorted);
        sorted.dedup();
    }
    graham(&sorted)
}

/// `(long)` of a `float`: truncation toward zero (the x86-64 conversion's "indefinite" value, `LONG_MIN`, out of
/// range).
pub fn trunc_long(v: f32) -> c_long {
    let limit = -(c_long::MIN as f32);
    if v.is_nan() || v >= limit || v < -limit {
        c_long::MIN
    } else {
        v as c_long
    }
}

/// The tile column or row of a coordinate (`AddScanLine`): truncated, and one less at or below 0.
pub fn tile_of(v: f32) -> c_long {
    if v > 0.0 {
        trunc_long(v)
    } else {
        trunc_long(v).wrapping_sub(1)
    }
}

/// `U2DConvexHull<T>::AddScanLine`: walks the tiles under one edge, widening each row it crosses.
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

/// `U2DConvexHull<T>::GetScanLines` over an outline of `count` points whose edge `i` (point `i` to point
/// `(i + 1) % count`) is `edge(i)`: the rows under it, row -> (first x, last x).
pub fn scan_lines_by(count: usize, edge: impl Fn(usize) -> Ge3DLine) -> BTreeMap<c_long, (c_long, c_long)> {
    let mut rows = BTreeMap::new();
    for i in 0..count {
        add_scan_line(&edge(i), &mut rows);
    }
    rows
}
