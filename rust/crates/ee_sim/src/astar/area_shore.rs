//! `EEAIContinent`'s shore members whole (was EEAIContinent.cpp): `GetShoreNextTo` (the shore tiles next to an
//! area, each asked through the manager's `IsNextToArea` with its CNTQ record) and `GetShoreInRegions` (the walk of
//! area_geom.rs over the manager's queries, with its TEST ONLY AREA 6 record).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::{c_long, c_void};

use crate::app::capture_record::{
    ee_sim_capture_geom_begin as begin, ee_sim_capture_geom_commit as commit, ee_sim_capture_geom_enter,
    ee_sim_capture_geom_leave, ee_sim_capture_geom_points, ee_sim_capture_geom_value as value,
};
use crate::astar::area_geom::{self, ShoreHost};
use crate::astar::cm::EeAStarCmRecord;
use crate::astar::cm_io;
use crate::astar::cm_queries;
use crate::astar::hull::EeHullPoint;

/// `kGeomCaptureArea`
const AREA: u32 = 0x4145_5241;
const CATEGORY_LAND: u8 = 1;
const PATH_WATER_BODY: c_long = 1;
const PATH_SPACE_REGION: c_long = 3;

fn record() -> &'static EeAStarCmRecord {
    crate::astar::world_view::cm().expect("the continent manager")
}

fn shore(h: u64) -> Vec<EeHullPoint> {
    // SAFETY: a live continent
    unsafe { cm_io::continent(h).lists() }.map_or(Vec::new(), |l| l.shore().to_vec())
}

/// The manager's queries the walk asks (the C++'s `EeShoreCallbacks` over `gContinentManager`).
struct Manager;

impl ShoreHost for Manager {
    fn path_area_region(&mut self, tile: EeHullPoint, space: bool) -> Option<u16> {
        // SAFETY: the registered record
        let a = unsafe { cm_queries::ee_astar_cmq_area_at(record(), 3, tile.x, tile.y) };
        if a == 0 {
            return None;
        }
        // SAFETY: a live path area
        let p = unsafe { cm_io::path_area(a) };
        Some(if space { p.space_region } else { p.water_body })
    }
    fn is_land(&mut self, tile: EeHullPoint) -> bool {
        let c = crate::astar::cm_query::continent_at(record(), tile.x, tile.y);
        // SAFETY: a live continent
        c != 0 && unsafe { cm_io::continent(c).category } == CATEGORY_LAND
    }
    fn region(&mut self, tile: EeHullPoint, space: bool) -> u16 {
        // SAFETY: the registered record
        unsafe {
            cm_queries::ee_astar_cmq_region_id(
                record(),
                tile.x,
                tile.y,
                if space { PATH_SPACE_REGION } else { PATH_WATER_BODY },
            )
        }
    }
    fn map_size(&self) -> (u32, u32) {
        let r = record();
        (r.width as u32, r.height as u32)
    }
}

/// `GetShoreNextTo(id, out)`: `push(ctx, x, y)` for the shore tiles (from the last) next to area `id` (every one for 0).
///
/// # Safety
/// `h` a live continent of the registered manager; `push` accepts `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_shore_next_to(
    h: u64,
    id: u16,
    ctx: *mut c_void,
    push: extern "C" fn(*mut c_void, c_long, c_long),
) {
    for t in shore(h).iter().rev() {
        if id == 0 || cm_queries::ee_astar_cmq_is_next_to_area(record(), t.x, t.y, id) {
            push(ctx, t.x, t.y);
        }
    }
}

/// `GetShoreInRegions(ids, out, kind)`: `push(ctx, x, y)` for each shore tile found (the caller clears its list).
///
/// # Safety
/// `h` a live continent of the registered manager; `ids` holds `n` ids; `push` accepts `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_shore_in_regions(
    h: u64,
    ids: *const u16,
    n: usize,
    kind: c_long,
    ctx: *mut c_void,
    push: extern "C" fn(*mut c_void, c_long, c_long),
) {
    let outer = ee_sim_capture_geom_enter();
    let ids: &[u16] = if n == 0 || ids.is_null() { &[] } else { std::slice::from_raw_parts(ids, n) };
    let own = shore(h);
    let mut tiles = Vec::new();
    area_geom::shore_in_regions(&own, ids, kind == PATH_SPACE_REGION, &mut Manager, &mut tiles);
    for t in &tiles {
        push(ctx, t.x, t.y);
    }
    if outer {
        begin(AREA, 6);
        ee_sim_capture_geom_points(own.as_ptr() as *const c_long, own.len(), false);
        value(ids.len() as i32);
        for &i in ids {
            value(i as i32);
        }
        value(kind as i32);
        ee_sim_capture_geom_points(tiles.as_ptr() as *const c_long, tiles.len(), false);
        commit();
    }
    ee_sim_capture_geom_leave();
}
