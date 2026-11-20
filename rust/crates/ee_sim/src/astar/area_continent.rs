//! `EEAIContinent::GetClosestPoints` (was EEAIContinent.cpp) and the slot's dispatch by the area's kind: a
//! continent's shore tile and another continent's hull outline point closest to each other, with its TEST ONLY
//! AREA 2 record; any other pair is `EEAIArea::GetClosestPoints` (area_ops.rs, AREA 1).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::c_long;

use crate::app::capture_record::{
    ee_sim_capture_geom_begin as begin, ee_sim_capture_geom_commit as commit, ee_sim_capture_geom_enter,
    ee_sim_capture_geom_leave, ee_sim_capture_geom_point, ee_sim_capture_geom_points, ee_sim_capture_geom_value as value,
};
use crate::astar::area_geom;
use crate::astar::area_ops;
use crate::astar::cm_io;
use crate::astar::hull::EeHullPoint;

/// `kGeomCaptureArea`
const AREA: u32 = 0x4145_5241;
/// `kAIAreaContinent`
const CONTINENT: c_long = 1;

fn points(p: &[EeHullPoint]) {
    // SAFETY: an `EeHullPoint` is two C `long`s, the layout the capture reads
    unsafe { ee_sim_capture_geom_points(p.as_ptr() as *const c_long, p.len(), false) };
}

fn shore(h: u64) -> Vec<EeHullPoint> {
    // SAFETY: a live continent
    unsafe { cm_io::continent(h).lists() }.map_or(Vec::new(), |l| l.shore().to_vec())
}

/// `EEAIArea::GetClosestPoints` of the two areas (AREA 1).
unsafe fn area_closest(this: u64, other: u64) -> (bool, (EeHullPoint, EeHullPoint)) {
    let (mut a, mut b) = (EeHullPoint::default(), EeHullPoint::default());
    let result = area_ops::ee_astar_area_closest_points(
        cm_io::area(this),
        cm_io::area_tiles(this),
        cm_io::area(other),
        cm_io::area_tiles(other),
        &mut a,
        &mut b,
    );
    (result, (a, b))
}

/// `GetClosestPoints(other, outThis, outOther)` of area `this` by its kind: whether the points were found and the
/// two points ((0, 0) both when not).
///
/// # Safety
/// `this`, `other` live areas.
pub unsafe fn closest_points_of(this: u64, other: u64) -> (bool, (EeHullPoint, EeHullPoint)) {
    if cm_io::area(this).area_type != CONTINENT {
        return area_closest(this, other);
    }
    let outer = ee_sim_capture_geom_enter();
    let other_continent = cm_io::area(other).area_type == CONTINENT;
    let own = shore(this);
    let theirs = if other_continent { shore(other) } else { Vec::new() };
    let mut result = !own.is_empty() && other_continent && !theirs.is_empty();
    let mut out = (EeHullPoint::default(), EeHullPoint::default());
    if other_continent && result {
        if let Some(p) = area_geom::closest_points_to_hull(&own, cm_io::area(other).hull_points()) {
            out = p;
        }
    } else {
        let (r, p) = area_closest(this, other);
        result = r;
        out = p;
    }
    if outer {
        begin(AREA, 2);
        points(&own);
        value(other_continent as i32);
        value(ee_base::sparse::ee_sparse_size(cm_io::area_tiles(other)) as i32);
        points(cm_io::area(other).hull_points());
        value(result as i32);
        ee_sim_capture_geom_point(out.0.x as i32, out.0.y as i32);
        ee_sim_capture_geom_point(out.1.x as i32, out.1.y as i32);
        commit();
    }
    ee_sim_capture_geom_leave();
    (result, out)
}

/// `EEAIArea::GetClosestPoints` / `EEAIContinent::GetClosestPoints` for the C++ (the slot of `this`'s kind).
///
/// # Safety
/// `this`, `other` live areas; the outputs writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_closest_points_of(
    this: u64,
    other: u64,
    out_this: *mut EeHullPoint,
    out_other: *mut EeHullPoint,
) -> bool {
    let (r, (a, b)) = closest_points_of(this, other);
    *out_this = a;
    *out_other = b;
    r
}
