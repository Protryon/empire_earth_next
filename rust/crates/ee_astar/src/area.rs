//! An AI map area's record (`EEAIArea`, `ee/EEAIArea.h`): the base of every area kind (continents, path areas,
//! hills, forests, choke points, rectangle areas, towns, herds, armies). The C++ class keeps its vtable and its tile
//! set (`U2DSparseArrayPointContainer`, whole in `sparse`); its other members are this record, the C++ class's
//! first base: the hull (the convex outline of the tiles' extreme points, with its bounds, center and area; it was
//! `U2DConvexHull<long>`, whose only user the area was), the hull's bounds in influence-table regions, the serial
//! and the area type. The hull's rebuild (`Create`'s, `AddTiles`', `UpdateHull`'s and `RemoveTile`'s) and
//! `UpdateBounds` are here; the queries on the hull are `hull`'s and `area_geom`'s.
//!
//! RUST_PLAN 2.1: the hull is `hull::set_points` (the C++'s `SetPoints(points, false)`), the region bounds the C++'s
//! `long` divisions (truncating; a zero region size never reaches them: the influence table's is set before any
//! area is made).

use std::ffi::{c_long, c_ulong};

use crate::area_geom::EeHullRect;
use crate::continent_grid::EePoint;
use crate::ffi::EeSparseArray;
use crate::hull::{self, EeHullPoint};
use crate::sparse::{PointList, SparseArray};

/// The hull's points (opaque to C++).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeAStarAreaHull {
    points: Vec<EeHullPoint>,
}

/// `EEAIArea`'s members but the tile set (the C++ class's first base; `ee_astar_area_init` / `ee_astar_area_free`
/// in its ctor and dtor).
#[repr(C)]
#[derive(Debug)]
pub struct EeAStarAreaRecord {
    /// the hull's points (`ee_astar_area_hull_points`); NULL until a hull is set
    pub hull: *mut EeAStarAreaHull,
    /// the hull's input's bounds (`U2DRectangle<long>`'s layout: the C++ hands it out as one)
    pub hull_bounds: EeHullRect,
    /// the hull's center (`U2DPoint<long>`'s layout)
    pub hull_center: EeHullPoint,
    /// the hull's area (`float`)
    pub hull_area: f32,
    /// `mBounds`: the hull's bounds in influence-table regions
    pub bounds: EeHullRect,
    /// `mSerial`: `EEAIContinentManager::mLastSerial` when made, the key of its `mAreas`
    pub serial: c_ulong,
    /// `mAreaType`: the area kind (`kAIArea*`)
    pub area_type: c_long,
}

impl Default for EeAStarAreaRecord {
    fn default() -> EeAStarAreaRecord {
        EeAStarAreaRecord {
            hull: std::ptr::null_mut(),
            hull_bounds: EeHullRect::default(),
            hull_center: EeHullPoint::default(),
            hull_area: 0.0,
            bounds: EeHullRect::default(),
            serial: 0,
            area_type: 0,
        }
    }
}

impl EeAStarAreaRecord {
    /// The hull's points (none before a hull is set).
    pub fn hull_points(&self) -> &[EeHullPoint] {
        if self.hull.is_null() {
            &[]
        } else {
            // SAFETY: `hull` is this record's own box (`set_hull`), alive until `free`
            unsafe { &(*self.hull).points }
        }
    }

    /// `U2DConvexHull<long>::Clear`: no points, the bounds, center and area zero.
    pub fn clear_hull(&mut self) {
        if !self.hull.is_null() {
            // SAFETY: as in `hull_points`
            unsafe { (*self.hull).points.clear() };
        }
        self.hull_bounds = EeHullRect::default();
        self.hull_center = EeHullPoint::default();
        self.hull_area = 0.0;
    }

    /// The hull as it is (a saved game's): the points, the bounds, the center and the area.
    pub fn set_hull_raw(&mut self, points: &[EeHullPoint], bounds: EeHullRect, center: EeHullPoint, area: f32) {
        if self.hull.is_null() {
            self.hull = Box::into_raw(Box::default());
        }
        // SAFETY: as in `hull_points`
        let hull = unsafe { &mut (*self.hull).points };
        hull.clear();
        hull.extend_from_slice(points);
        self.hull_bounds = bounds;
        self.hull_center = center;
        self.hull_area = area;
    }

    /// `mHull.SetPoints(points, false)`: cleared, then (with points) the hull of the points, their bounds, its
    /// center and area.
    pub fn set_hull(&mut self, points: &[EeHullPoint]) {
        self.clear_hull();
        if points.is_empty() {
            return;
        }
        let (outline, shape) = hull::set_points(points, false);
        let bounds =
            EeHullRect { bottom: shape.bottom, left: shape.left, right: shape.right, top: shape.top };
        self.set_hull_raw(&outline, bounds, shape.center, shape.area);
    }

    /// `UpdateHull`'s and `RemoveTile`'s hull: the hull of the tile set's extreme edge points
    /// (`GetOnlyExtremeEdgePoints`).
    pub fn rebuild_hull(&mut self, tiles: &SparseArray) {
        let mut extremes = Vec::new();
        tiles.points(PointList::Extremes, &mut extremes);
        let points: Vec<EeHullPoint> =
            extremes.iter().map(|p| EeHullPoint::new(c_long::from(p.x), c_long::from(p.y))).collect();
        self.set_hull(&points);
    }

    /// `UpdateBounds(table)`: the hull's bounds divided by the table's region size (`region_size` 0: no table,
    /// the bounds unchanged).
    pub fn update_bounds(&mut self, region_size: c_long) {
        if region_size == 0 {
            return;
        }
        self.bounds.top = self.hull_bounds.top.wrapping_div(region_size);
        self.bounds.left = self.hull_bounds.left.wrapping_div(region_size);
        self.bounds.right = self.hull_bounds.right.wrapping_div(region_size);
        self.bounds.bottom = self.hull_bounds.bottom.wrapping_div(region_size);
    }

    /// `UpdateHull`: the hull rebuilt, then the region bounds.
    pub fn update_hull(&mut self, tiles: &SparseArray, region_size: c_long) {
        self.rebuild_hull(tiles);
        self.update_bounds(region_size);
    }

    /// `RemoveTile(tile)`: the tile removed and, when it was there, the hull rebuilt (the region bounds kept);
    /// whether it was there.
    pub fn remove_tile(&mut self, set: &mut SparseArray, tile: EePoint) -> bool {
        let removed = set.remove(tile);
        if removed {
            self.rebuild_hull(set);
        }
        removed
    }

    /// The record's hull freed (the dtor's); the record is empty afterwards.
    pub fn free(&mut self) {
        if !self.hull.is_null() {
            // SAFETY: `hull` came from `Box::into_raw` in `set_hull_raw` and is freed once (set NULL here)
            drop(unsafe { Box::from_raw(self.hull) });
            self.hull = std::ptr::null_mut();
        }
    }
}

// ---- the C interface (EEAIArea's ctor, dtor and slots) ----

/// The record of a new area: everything zero, no hull.
///
/// # Safety
/// `r` points to writable memory for a record (its old contents are not read).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_init(r: *mut EeAStarAreaRecord) {
    // SAFETY: the caller's contract
    unsafe { r.write(EeAStarAreaRecord::default()) };
}

/// The record's hull freed (the area's dtor).
///
/// # Safety
/// `r` a record from `ee_astar_area_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_free(r: *mut EeAStarAreaRecord) {
    // SAFETY: the caller's contract
    unsafe { (*r).free() };
}

/// The hull's points and their count (NULL and 0 without points); valid until the hull next changes.
///
/// # Safety
/// `r` a live record, `count` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_hull_points(r: *const EeAStarAreaRecord, count: *mut usize) -> *const EeHullPoint {
    // SAFETY: the caller's contract
    let points = unsafe { (*r).hull_points() };
    // SAFETY: the caller's contract
    unsafe { *count = points.len() };
    if points.is_empty() {
        std::ptr::null()
    } else {
        points.as_ptr()
    }
}

/// `SetPoints(points, false)` on the hull.
///
/// # Safety
/// `r` a live record, `points` holds `count` points (NULL when 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_set_hull(r: *mut EeAStarAreaRecord, points: *const EeHullPoint, count: usize) {
    // SAFETY: the caller's contract
    let points = unsafe { slice(points, count) };
    // SAFETY: the caller's contract
    unsafe { (*r).set_hull(points) };
}

/// The hull as a saved game has it (points, bounds, center, area).
///
/// # Safety
/// `r` a live record, `points` holds `count` points (NULL when 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_set_hull_raw(
    r: *mut EeAStarAreaRecord,
    points: *const EeHullPoint,
    count: usize,
    bounds: EeHullRect,
    center: EeHullPoint,
    area: f32,
) {
    // SAFETY: the caller's contract
    let points = unsafe { slice(points, count) };
    // SAFETY: the caller's contract
    unsafe { (*r).set_hull_raw(points, bounds, center, area) };
}

/// `U2DConvexHull<long>::Clear` on the hull.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_clear_hull(r: *mut EeAStarAreaRecord) {
    // SAFETY: the caller's contract
    unsafe { (*r).clear_hull() };
}

/// `UpdateBounds(table)` (`region_size` 0: no table).
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_update_bounds(r: *mut EeAStarAreaRecord, region_size: c_long) {
    // SAFETY: the caller's contract
    unsafe { (*r).update_bounds(region_size) };
}

/// `UpdateHull`: the hull of `tiles`' extreme edge points, then the region bounds.
///
/// # Safety
/// `r` a live record, `tiles` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_update_hull(r: *mut EeAStarAreaRecord, tiles: *const EeSparseArray, region_size: c_long) {
    // SAFETY: the caller's contract
    unsafe { (*r).update_hull(&(*tiles).set, region_size) };
}

/// `RemoveTile(tile)`: whether the tile was there (then the hull is rebuilt).
///
/// # Safety
/// `r` a live record, `tiles` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_remove_tile(r: *mut EeAStarAreaRecord, tiles: *mut EeSparseArray, x: i32, y: i32) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*r).remove_tile(&mut (*tiles).set, EePoint { x, y }) }
}

/// # Safety
/// `points` holds `count` elements, or `count` is 0.
unsafe fn slice<'a, T>(points: *const T, count: usize) -> &'a [T] {
    if count == 0 || points.is_null() {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(points, count) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(x: c_long, y: c_long) -> EeHullPoint {
        EeHullPoint::new(x, y)
    }

    fn set_of(points: &[(i32, i32)]) -> SparseArray {
        let mut set = SparseArray::new();
        let points: Vec<EePoint> = points.iter().map(|&(x, y)| EePoint { x, y }).collect();
        set.create(&points);
        set
    }

    #[test]
    fn a_new_record_is_empty() {
        let mut r = std::mem::MaybeUninit::<EeAStarAreaRecord>::uninit();
        // SAFETY: init writes the whole record
        let mut r = unsafe {
            ee_astar_area_init(r.as_mut_ptr());
            r.assume_init()
        };
        assert!(r.hull.is_null() && r.serial == 0 && r.area_type == 0 && r.bounds == EeHullRect::default());
        let mut count = 7;
        // SAFETY: a live record
        assert!(unsafe { ee_astar_area_hull_points(&r, &mut count) }.is_null());
        assert_eq!(count, 0);
        r.free();
        r.free();
        assert!(r.hull.is_null());
    }

    #[test]
    fn the_hull_is_set_points_and_clear_empties_it() {
        let input = [p(0, 0), p(4, 0), p(4, 4), p(0, 4), p(2, 2)];
        let mut r = EeAStarAreaRecord::default();
        r.set_hull(&input);
        let (outline, shape) = hull::set_points(&input, false);
        assert_eq!(r.hull_points(), &outline[..]);
        assert_eq!(r.hull_bounds, EeHullRect { bottom: shape.bottom, left: shape.left, right: shape.right, top: shape.top });
        assert_eq!((r.hull_center, r.hull_area), (shape.center, shape.area));
        assert_eq!(r.hull_bounds, EeHullRect { bottom: 4, left: 0, right: 4, top: 0 });
        // no points: the C++'s Clear (bounds, center and area zero), then nothing
        r.set_hull(&[]);
        assert!(r.hull_points().is_empty());
        assert_eq!((r.hull_bounds, r.hull_center, r.hull_area), (EeHullRect::default(), p(0, 0), 0.0));
        r.set_hull(&input);
        r.clear_hull();
        assert!(r.hull_points().is_empty() && r.hull_area == 0.0);
        r.free();
    }

    #[test]
    fn the_region_bounds_truncate_and_need_a_table() {
        let mut r = EeAStarAreaRecord::default();
        r.set_hull(&[p(-5, 3), p(9, 3), p(9, 17), p(-5, 17)]);
        r.update_bounds(0);
        assert_eq!(r.bounds, EeHullRect::default());
        r.update_bounds(4);
        // C's `/`: toward zero (-5 / 4 is -1)
        assert_eq!(r.bounds, EeHullRect { bottom: 4, left: -1, right: 2, top: 0 });
        r.free();
    }

    #[test]
    fn the_hull_is_rebuilt_from_the_extreme_edge_points() {
        let tiles = [(1, 1), (1, 2), (1, 3), (2, 2), (3, 0), (3, 1), (3, 5), (6, 4)];
        let set = set_of(&tiles);
        let mut r = EeAStarAreaRecord::default();
        r.update_hull(&set, 2);
        let mut extremes = Vec::new();
        set.points(PointList::Extremes, &mut extremes);
        let expected: Vec<EeHullPoint> = extremes.iter().map(|q| p(q.x.into(), q.y.into())).collect();
        let mut by_hand = EeAStarAreaRecord::default();
        by_hand.set_hull(&expected);
        by_hand.update_bounds(2);
        assert_eq!(r.hull_points(), by_hand.hull_points());
        assert_eq!((r.hull_bounds, r.bounds, r.hull_center), (by_hand.hull_bounds, by_hand.bounds, by_hand.hull_center));
        assert_eq!(r.hull_area.to_bits(), by_hand.hull_area.to_bits());
        r.free();
        by_hand.free();
    }

    #[test]
    fn remove_tile_rebuilds_only_when_the_tile_was_there() {
        let mut set = set_of(&[(0, 0), (0, 4), (4, 0), (4, 4), (2, 2)]);
        let mut r = EeAStarAreaRecord::default();
        r.update_hull(&set, 1);
        let before = r.hull_points().to_vec();
        assert!(!r.remove_tile(&mut set, EePoint { x: 9, y: 9 }));
        assert_eq!(r.hull_points(), &before[..]);
        assert!(r.remove_tile(&mut set, EePoint { x: 4, y: 4 }));
        assert!(!set.contains(EePoint { x: 4, y: 4 }));
        assert!(!r.hull_points().contains(&p(4, 4)));
        // the region bounds are not updated by RemoveTile
        assert_eq!(r.bounds, EeHullRect { bottom: 4, left: 0, right: 4, top: 0 });
        r.free();
    }

    #[test]
    fn a_saved_hull_comes_back_as_it_was() {
        let mut r = EeAStarAreaRecord::default();
        let bounds = EeHullRect { bottom: 9, left: 1, right: 8, top: 2 };
        // SAFETY: a live record and three points
        unsafe {
            ee_astar_area_set_hull_raw(&mut r, [p(1, 2), p(8, 2), p(5, 9)].as_ptr(), 3, bounds, p(4, 4), 24.5)
        };
        assert_eq!(r.hull_points(), &[p(1, 2), p(8, 2), p(5, 9)]);
        assert_eq!((r.hull_bounds, r.hull_center, r.hull_area), (bounds, p(4, 4), 24.5));
        r.free();
    }
}
