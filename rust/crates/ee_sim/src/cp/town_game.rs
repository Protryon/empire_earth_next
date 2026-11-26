//! The town's game host (`ee_cp_town_host`, `ee_cp_area_host`: EETown.cpp, EEHerd.cpp) and the entry points
//! `EETown`'s members forward to. Absent from a test binary.

#![cfg(not(test))]

use std::ffi::c_void;

use super::herd::{EeHerdIo, EeHerdPoint};
use super::town::{EeTownFields, Town, TownHost, TownIo};

extern "C" {
    /// The town's world (EETown.cpp): `town` the `EETown*`.
    pub(crate) fn ee_cp_town_host(town: *mut c_void, op: i32, a: u64, b: u64, io: *mut TownIo) -> i64;
    /// An area's operations the random point asks (EEHerd.cpp): `area` the `EEAIArea*`.
    fn ee_cp_area_host(area: *mut c_void, op: i32, io: *mut EeHerdIo) -> i64;
}

struct Game {
    town: *mut c_void,
    area: *mut c_void,
}

impl TownHost for Game {
    fn call(&mut self, op: i32, a: u64, b: u64, io: &mut TownIo) -> i64 {
        // SAFETY: the town and its area are live for the call
        if let Some(r) = unsafe { area_op(self.area as u64, op, a, io) } {
            return r;
        }
        // SAFETY: as above
        if let Some(r) = unsafe { super::town_world::town_op(self.town as u64, op, a, io) } {
            return r;
        }
        // SAFETY: the town is live for the call
        unsafe { ee_cp_town_host(self.town, op, a, b, io) }
    }
    fn area(&mut self, op: i32, v: &[i64]) -> (i64, EeHerdIo) {
        let mut io = EeHerdIo::default();
        io.v[..v.len()].copy_from_slice(v);
        // SAFETY: as above
        if let Some(r) = unsafe { super::herd_world::area_op(self.area as u64, op, &mut io) } {
            return (r, io);
        }
        // SAFETY: as above
        let r = unsafe { ee_cp_area_host(self.area, op, &mut io) };
        (r, io)
    }
}

/// The town's operations on its area and the continent manager, over their Rust records (AI Thread cluster round
/// 6; once `ee_cp_town_host`'s): the tiles, the hull, the continents. `None` for the C++ host's.
///
/// # Safety
/// `area` the town's live `EEAIArea*`; `a` 0 or another live town (an `EETown*` is its `EEAIArea*`).
unsafe fn area_op(area: u64, op: i32, a: u64, io: &mut TownIo) -> Option<i64> {
    use super::town::*;
    use crate::astar::cm_io;
    use crate::astar::continent_grid::EePoint;
    use crate::astar::hull::EeHullPoint;
    let (x, y) = (io.v[0], io.v[1]);
    let p = EeHullPoint::new(x as std::ffi::c_long, y as std::ffi::c_long);
    let input = || -> Vec<(i64, i64)> {
        (0..io.input_len)
            // SAFETY: the operation's `input_len` points
            .map(|i| unsafe { (*io.input.add(2 * i), *io.input.add(2 * i + 1)) })
            .collect()
    };
    let rec = cm_io::area(area);
    let tiles = cm_io::area_tiles(area) as *mut ee_base::sparse::EeSparseArray;
    let cm = crate::astar::world_view::cm();
    Some(match op {
        EE_CP_TOWN_NUM_TILES => i64::from(ee_base::sparse::ee_sparse_size(tiles)),
        EE_CP_TOWN_HULL_CONTAINS => i64::from(crate::astar::area_ops::ee_astar_area_hull_contains(rec, p)),
        EE_CP_TOWN_HULL_POINTS => {
            let mut n = 0;
            let points = crate::astar::area::ee_astar_area_hull_points(rec, &mut n);
            for k in 0..n {
                let q = *points.add(k);
                super::herd::ee_cp_herd_push(io.list, q.x as i64, q.y as i64, 0, 0);
            }
            0
        }
        EE_CP_TOWN_TILE_INSERT => {
            ee_base::sparse::ee_sparse_insert(tiles, x as i32, y as i32);
            0
        }
        EE_CP_TOWN_TILE_CONTAINS => i64::from(ee_base::sparse::ee_sparse_contains(tiles, x as i32, y as i32)),
        EE_CP_TOWN_TILE_REMOVE => {
            ee_base::sparse::ee_sparse_remove(tiles, x as i32, y as i32);
            0
        }
        EE_CP_TOWN_TILES => {
            let set = if a != 0 { cm_io::area_tiles(a) as *mut ee_base::sparse::EeSparseArray } else { tiles };
            let mut n = 0;
            let points = ee_base::sparse::ee_sparse_points(set, false, 1, &mut n);
            let points: Vec<EePoint> = (0..n).map(|k| *points.add(k)).collect();
            for q in points {
                super::herd::ee_cp_herd_push(io.list, i64::from(q.x), i64::from(q.y), 0, 0);
            }
            0
        }
        EE_CP_TOWN_CREATE => {
            let points: Vec<EePoint> = input().iter().map(|&(x, y)| EePoint { x: x as i32, y: y as i32 }).collect();
            crate::astar::cm_host::area_create(area, &points);
            0
        }
        EE_CP_TOWN_SET_HULL => {
            let points: Vec<EeHullPoint> =
                input().iter().map(|&(x, y)| EeHullPoint::new(x as std::ffi::c_long, y as std::ffi::c_long)).collect();
            crate::astar::area_ops::ee_astar_area_set_hull_points(rec, points.as_ptr(), points.len());
            0
        }
        EE_CP_TOWN_CLEAR_HULL => {
            crate::astar::area::ee_astar_area_clear_hull(rec);
            0
        }
        EE_CP_TOWN_CLOSEST_HULL => {
            let q = crate::astar::area_ops::ee_astar_area_closest_hull_point(rec, p);
            io.v[0] = q.x as i64;
            io.v[1] = q.y as i64;
            0
        }
        EE_CP_TOWN_CONTINENT_AT => {
            cm.map_or(0, |r| i64::from(cm_io::continent_id_at(r, x as std::ffi::c_long, y as std::ffi::c_long)))
        }
        EE_CP_TOWN_CONNECTED => {
            let Some(r) = cm else { return Some(0) };
            let mut ids = vec![0u16; r.flags_width.max(0) as usize];
            let n = crate::astar::cm_queries::ee_astar_cmq_adjacent_ids(r, x as u16, true, ids.as_mut_ptr());
            for &id in &ids[..n] {
                super::herd::ee_cp_herd_push(io.list, i64::from(id), 0, 0, 0);
            }
            0
        }
        EE_CP_TOWN_CONTINENT => {
            let c = cm.and_then(|r| {
                r.tables().tables[crate::astar::cm::EeAStarCmTable::Continents as usize].get(x as u16 as usize).copied()
            });
            let c = c.unwrap_or(0);
            if c == 0 {
                return Some(-1);
            }
            if y != 0 {
                let (mut this, mut other) = (EeHullPoint::new(0, 0), EeHullPoint::new(0, 0));
                io.v[2] = i64::from(crate::astar::area_continent::ee_astar_area_closest_points_of(
                    c, area, &mut this, &mut other,
                ));
                io.v[3] = this.x as i64;
                io.v[4] = this.y as i64;
                io.v[5] = other.x as i64;
                io.v[6] = other.y as i64;
            }
            i64::from(cm_io::continent(c).category)
        }
        _ => return None,
    })
}

/// # Safety
/// `town` is the live `EETown*` whose area is `area` and record `f`.
unsafe fn with<R>(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    body: impl FnOnce(&mut Town<'_, Game>) -> R,
) -> R {
    let mut game = Game { town, area };
    // SAFETY: the caller's contract
    let mut t = unsafe { Town::new(&mut game, f) };
    body(&mut t)
}

/// `EETown::AddUnit`.
///
/// # Safety
/// As `with`; `unit` a live `EEUnit*`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_add_unit(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    unit: u64,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.add_unit(unit)) }
}

/// `EETown::RemoveUnit`.
///
/// # Safety
/// As [`ee_cp_town_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_remove_unit(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    unit: u64,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.remove_unit(unit)) }
}

/// `EETown::CanTake`.
///
/// # Safety
/// As [`ee_cp_town_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_can_take(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    unit: u64,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.can_take(unit)) }
}

/// `EETown::Merge` of the town `other` (its record `o`).
///
/// # Safety
/// As `with`; `other` a live `EETown*` with its record `o`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_merge(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
    other: u64,
    o: *const EeTownFields,
) {
    // SAFETY: the caller's contract (the other's record copied first)
    let o = unsafe { *o };
    // SAFETY: as above
    unsafe { with(town, area, f, |t| t.merge(other, &o)) }
}

/// `EETown::Reset`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_reset(town: *mut c_void, area: *mut c_void, f: *mut EeTownFields) {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.reset()) }
}

/// `EETown::UpdateNearWater`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_update_near_water(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.update_near_water()) }
}

/// `EETown::RefreshPatrolGroup`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_refresh_patrol_group(town: *mut c_void, area: *mut c_void, f: *mut EeTownFields) {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.refresh_patrol_group()) }
}

/// `EETown::GetRandomPoint`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_town_random_point(
    town: *mut c_void,
    area: *mut c_void,
    f: *mut EeTownFields,
) -> EeHerdPoint {
    // SAFETY: the caller's contract
    unsafe { with(town, area, f, |t| t.random_point()) }
}
