//! `EEAIArea`'s out-of-line members whole (was EEAIArea.cpp; the game's leaves round 2): the hull's queries on the
//! record, the hull's rebuilds, the closest points of two areas and the hull's tiles, each with its TEST ONLY
//! geometry record (HULL 1, 2, 6; AREA 1, 3, 4, 5: ee/EESimCapture.h, `capture_record`'s scopes). The C++ class keeps
//! its vtable, its tile set and the inline forwarders (ee/EEAIArea.h).
//!
//! A record is written only by the outermost geometry scope of its thread, as the C++ scopes did; the scope opens
//! where the C++ opened it (HullContains: after its bounds test).

use std::ffi::{c_long, c_void};

use crate::app::capture_record::{
    ee_sim_capture_geom_begin as begin, ee_sim_capture_geom_commit as commit, ee_sim_capture_geom_enter,
    ee_sim_capture_geom_float as float, ee_sim_capture_geom_leave, ee_sim_capture_geom_point,
    ee_sim_capture_geom_points, ee_sim_capture_geom_value as value,
};
use crate::astar::area::EeAStarAreaRecord;
use crate::astar::area_geom::{self, EeHullRect};
use crate::astar::continent_grid::EePoint;
use crate::astar::ffi::EeSparseArray;
use crate::astar::hull::{self, EeHullPoint};
use crate::astar::sparse::{PointList, SparseArray};

/// `kGeomCaptureHull`, `kGeomCaptureArea` (lle/U2DPolygon.h)
const HULL: u32 = 0x4c4c_5548;
const AREA: u32 = 0x4145_5241;

/// A geometry scope (`EEGeomCaptureScope`): `outer` when it is its thread's outermost and the capture is on.
struct Scope {
    outer: bool,
}

impl Scope {
    fn new() -> Scope {
        Scope { outer: ee_sim_capture_geom_enter() }
    }
}

impl Drop for Scope {
    fn drop(&mut self) {
        ee_sim_capture_geom_leave();
    }
}

fn point(p: EeHullPoint) {
    ee_sim_capture_geom_point(p.x as i32, p.y as i32);
}

fn points(p: &[EeHullPoint], whole: bool) {
    // SAFETY: an `EeHullPoint` is two C `long`s, the layout the capture reads
    unsafe { ee_sim_capture_geom_points(p.as_ptr() as *const c_long, p.len(), whole) };
}

fn tile_points(set: &SparseArray, list: PointList) -> Vec<EeHullPoint> {
    let mut out = Vec::new();
    set.points(list, &mut out);
    out.iter().map(|p| EeHullPoint::new(c_long::from(p.x), c_long::from(p.y))).collect()
}

/// HULL 1: `U2DConvexHull<long>::SetPoints`' record (unsorted input), the input and the whole hull after it.
fn capture_set_points(r: &EeAStarAreaRecord, input: &[EeHullPoint]) {
    begin(HULL, 1);
    value(0);
    points(input, true);
    points(r.hull_points(), true);
    for v in [r.hull_bounds.bottom, r.hull_bounds.left, r.hull_bounds.right, r.hull_bounds.top] {
        value(v as i32);
    }
    point(r.hull_center);
    float(r.hull_area);
    commit();
}

/// # Safety
/// `points` holds `count` points, or `count` is 0.
unsafe fn slice<'a, T>(points: *const T, count: usize) -> &'a [T] {
    if count == 0 || points.is_null() {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(points, count) }
    }
}

/// `HullContains(point)`: the hull's bounds, then the outline (HULL 6).
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_hull_contains(r: *const EeAStarAreaRecord, p: EeHullPoint) -> bool {
    // SAFETY: the caller's contract
    let r = unsafe { &*r };
    if !r.hull_bounds.contains(p) {
        return false;
    }
    let scope = Scope::new();
    let result = hull::contains(r.hull_points(), p);
    if scope.outer {
        begin(HULL, 6);
        points(r.hull_points(), false);
        point(p);
        value(result as i32);
        commit();
    }
    result
}

/// `GetHullClosestOutlinePoint(point)` (HULL 2).
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_hull_closest_outline_point(
    r: *const EeAStarAreaRecord,
    p: EeHullPoint,
) -> EeHullPoint {
    // SAFETY: the caller's contract
    let r = unsafe { &*r };
    let scope = Scope::new();
    let result = hull::closest_outline_point(r.hull_points(), p);
    if scope.outer {
        begin(HULL, 2);
        points(r.hull_points(), false);
        point(p);
        point(result);
        commit();
    }
    result
}

/// `SetHullPoints(points)`: `SetPoints(points, false)` (HULL 1).
///
/// # Safety
/// `r` a live record, `p` holds `count` points (NULL when 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_set_hull_points(r: *mut EeAStarAreaRecord, p: *const EeHullPoint, count: usize) {
    // SAFETY: the caller's contract
    let (r, input) = unsafe { (&mut *r, slice(p, count)) };
    let scope = Scope::new();
    r.set_hull(input);
    if scope.outer {
        capture_set_points(r, input);
    }
}

/// `UpdateHull`: the hull of the tiles' extreme edge points, then the region bounds (HULL 1 of those points).
///
/// # Safety
/// `r` a live record, `tiles` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_update_hull_captured(
    r: *mut EeAStarAreaRecord,
    tiles: *const EeSparseArray,
    region_size: c_long,
) {
    // SAFETY: the caller's contract
    let (r, set) = unsafe { (&mut *r, &(*tiles).set) };
    let scope = Scope::new();
    r.update_hull(set, region_size);
    if scope.outer {
        capture_set_points(r, &tile_points(set, PointList::Extremes));
    }
}

/// `RemoveTile(tile)`: whether it was there (then the hull is rebuilt: HULL 1 of the extreme points).
///
/// # Safety
/// `r` a live record, `tiles` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_remove_tile_captured(
    r: *mut EeAStarAreaRecord,
    tiles: *mut EeSparseArray,
    p: EeHullPoint,
) -> bool {
    // SAFETY: the caller's contract
    let (r, set) = unsafe { (&mut *r, &mut (*tiles).set) };
    let scope = Scope::new();
    let removed = r.remove_tile(set, EePoint { x: p.x as i32, y: p.y as i32 });
    if removed && scope.outer {
        capture_set_points(r, &tile_points(set, PointList::Extremes));
    }
    removed
}

/// `GetHullDistanceSquared(other)`: between the hulls' closest points (AREA 4).
///
/// # Safety
/// `r`, `other` live records.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_hull_distance_squared(
    r: *const EeAStarAreaRecord,
    other: *const EeAStarAreaRecord,
) -> f32 {
    // SAFETY: the caller's contract
    let (r, other) = unsafe { (&*r, &*other) };
    let scope = Scope::new();
    let result = area_geom::hull_distance_squared(r.hull_points(), other.hull_points());
    if scope.outer {
        begin(AREA, 4);
        points(r.hull_points(), false);
        points(other.hull_points(), false);
        float(result);
        commit();
    }
    result
}

/// `GetClosestHullPoint(point)`: the point inside the hull, else the closest outline point (AREA 3).
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_closest_hull_point(r: *const EeAStarAreaRecord, p: EeHullPoint) -> EeHullPoint {
    // SAFETY: the caller's contract
    let r = unsafe { &*r };
    let scope = Scope::new();
    let bounds: EeHullRect = r.hull_bounds;
    let result = area_geom::closest_hull_point(r.hull_points(), &bounds, p);
    if scope.outer {
        begin(AREA, 3);
        points(r.hull_points(), false);
        point(p);
        point(result);
        commit();
    }
    result
}

/// `GetClosestPoints(other, outThis, outOther)`: this area's tile and the other's hull outline point closest to each
/// other; false (both (0, 0)) when either area has no tiles (AREA 1).
///
/// # Safety
/// `r`, `other` live records; `tiles`, `other_tiles` their live sets; the outputs writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_closest_points(
    r: *const EeAStarAreaRecord,
    tiles: *const EeSparseArray,
    other: *const EeAStarAreaRecord,
    other_tiles: *const EeSparseArray,
    out_this: *mut EeHullPoint,
    out_other: *mut EeHullPoint,
) -> bool {
    // SAFETY: the caller's contract
    let (_r, set, other, other_set) = unsafe { (&*r, &(*tiles).set, &*other, &(*other_tiles).set) };
    let scope = Scope::new();
    let result = set.size() != 0 && other_set.size() != 0;
    let mut this_point = EeHullPoint::default();
    let mut other_point = EeHullPoint::default();
    let mut list = Vec::new();
    if result {
        list = tile_points(set, PointList::All(1));
        if let Some((a, b)) = area_geom::closest_points_to_hull(&list, other.hull_points()) {
            this_point = a;
            other_point = b;
        }
    }
    // SAFETY: the caller's contract
    unsafe {
        *out_this = this_point;
        *out_other = other_point;
    }
    if scope.outer {
        begin(AREA, 1);
        value(set.size());
        points(&list, false);
        points(other.hull_points(), false);
        value(result as i32);
        point(this_point);
        point(other_point);
        commit();
    }
    result
}

/// `GetTiles(outTiles)`: appends every tile of the hull's scan lines through `push` (AREA 5: the tiles appended).
///
/// # Safety
/// `r` a live record; `push` accepts `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_get_tiles(
    r: *const EeAStarAreaRecord,
    first: usize,
    ctx: *mut c_void,
    push: extern "C" fn(*mut c_void, c_long, c_long),
) {
    // SAFETY: the caller's contract
    let r = unsafe { &*r };
    let scope = Scope::new();
    let mut tiles = Vec::new();
    area_geom::tiles(r.hull_points(), &mut tiles);
    for t in &tiles {
        push(ctx, t.x, t.y);
    }
    if scope.outer {
        begin(AREA, 5);
        points(r.hull_points(), false);
        value(first as i32);
        points(&tiles, false);
        commit();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn set_of(points: &[(i32, i32)]) -> EeSparseArray {
        let mut a = EeSparseArray::default();
        a.set.create(&points.iter().map(|&(x, y)| EePoint { x, y }).collect::<Vec<_>>());
        a
    }

    #[test]
    fn the_queries_match_the_geometry_they_wrap() {
        let tiles = set_of(&[(0, 0), (0, 4), (4, 0), (4, 4), (2, 2)]);
        let mut r = EeAStarAreaRecord::default();
        // SAFETY: a live record and set
        unsafe { ee_astar_area_update_hull_captured(&mut r, &tiles, 1) };
        let inside = EeHullPoint::new(2, 2);
        let outside = EeHullPoint::new(9, 9);
        // SAFETY: as above
        unsafe {
            assert!(ee_astar_area_hull_contains(&r, inside));
            assert!(!ee_astar_area_hull_contains(&r, outside));
            assert_eq!(ee_astar_area_closest_hull_point(&r, inside), inside);
            assert_eq!(
                ee_astar_area_hull_closest_outline_point(&r, outside),
                hull::closest_outline_point(r.hull_points(), outside)
            );
            assert_eq!(ee_astar_area_hull_distance_squared(&r, &r), 0.0);
        }
        let empty = EeSparseArray::default();
        let e = EeAStarAreaRecord::default();
        let (mut a, mut b) = (EeHullPoint::new(7, 7), EeHullPoint::new(7, 7));
        // SAFETY: as above
        assert!(!unsafe { ee_astar_area_closest_points(&r, &tiles, &e, &empty, &mut a, &mut b) });
        assert_eq!((a, b), (EeHullPoint::default(), EeHullPoint::default()), "no tiles: both (0, 0)");
        // SAFETY: as above
        assert!(unsafe { ee_astar_area_closest_points(&r, &tiles, &r, &tiles, &mut a, &mut b) });
        let mut out: Vec<(c_long, c_long)> = Vec::new();
        extern "C" fn push(ctx: *mut c_void, x: c_long, y: c_long) {
            // SAFETY: the test's vector
            unsafe { (*(ctx as *mut Vec<(c_long, c_long)>)).push((x, y)) };
        }
        // SAFETY: as above
        unsafe { ee_astar_area_get_tiles(&r, 0, &mut out as *mut _ as *mut c_void, push) };
        assert!(out.contains(&(2, 2)) && out.contains(&(0, 0)));
        let mut tiles = tiles;
        // SAFETY: as above
        unsafe {
            assert!(ee_astar_area_remove_tile_captured(&mut r, &mut tiles, EeHullPoint::new(4, 4)));
            assert!(!ee_astar_area_remove_tile_captured(&mut r, &mut tiles, EeHullPoint::new(4, 4)));
        }
        let pts = [EeHullPoint::new(0, 0), EeHullPoint::new(3, 0), EeHullPoint::new(0, 3)];
        // SAFETY: as above
        unsafe { ee_astar_area_set_hull_points(&mut r, pts.as_ptr(), pts.len()) };
        assert_eq!(r.hull_points().len(), 3);
        r.free();
    }
}
