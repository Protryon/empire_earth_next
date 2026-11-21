//! The continent manager's members that change the world (AI Thread cluster round 8; were EEAIContinentManager.cpp's):
//! UpdateWallUnit (a finished wall or gate's footprint queued for a sector rebuild, the path manager told) with
//! its CNTQ record, and a forest's RegrowTrees (new trees at random forest tiles round a felled one). What only C++
//! holds is one host case each (EE_ASTAR_CM_* from 46).

#![cfg(not(test))]
#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::os::raw::c_long;

use crate::astar::cm::EeAStarCmRecord;
use crate::astar::cm_io::{self, EeAStarCmIo};
use crate::astar::hull::EeHullPoint;
use crate::world::{UnitView, WorldRead, WorldView};

/// `gPhysics->GetRandom(i0, i1)` (the longs')
pub const EE_ASTAR_CM_RANDOM_RANGE: i32 = 46;
/// `gUnitClassTable[kUnitTypeResEagle]->CreateUnit(i0, i1, 0, false, NULL)->mComplexUnit`; its `mIsRegrowing = true`
pub const EE_ASTAR_CM_REGROW_UNIT: i32 = 47;
pub const EE_ASTAR_CM_SET_REGROWING: i32 = 48;
/// unit `h0`'s `IsFoundation()`; `gPathManager.SetRegionUpdates(true, i0)`
pub const EE_ASTAR_CM_IS_FOUNDATION: i32 = 49;
pub const EE_ASTAR_CM_REGION_UPDATES: i32 = 50;

/// `kUnitCategoryGate`
const CATEGORY_GATE: u64 = 0x19;
/// CNTQ's kind of UpdateWallUnit
const QUERY_UPDATE_WALL_UNIT: u32 = 24;

fn host(op: i32, h: u64, i: [i64; 4]) -> i64 {
    let mut io = EeAStarCmIo { h: [h, 0], i };
    cm_io::call_host(op, &mut io)
}

/// `EEAIContinentManager::UpdateWallUnit`: the footprint of a finished blocking unit or gate queued for a sector
/// rebuild and the path manager told (`now`: at once); skipped while clearing or before regions exist.
///
/// # Safety
/// `r` is the manager's live record; `unit` a live complex unit.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_update_wall_unit(r: *mut EeAStarCmRecord, unit: u64, now: bool) {
    let on = crate::astar::cm_capture::ee_astar_cntq_enter(QUERY_UPDATE_WALL_UNIT);
    let (id, blocks) = {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: the caller's live unit
        let v = unsafe { UnitView::of(&t, unit) };
        let id = v.as_ref().map_or(-1, |u| u.id() as i64);
        let blocks = v
            .and_then(|u| u.class())
            .map(|c| crate::world_field!(c.fields(), mBlocksTile) || c.category() as u64 == CATEGORY_GATE);
        let has_map = WorldView::global(&t).and_then(|w| w.map()).is_some();
        (id, blocks.filter(|_| has_map))
    };
    if on {
        crate::astar::cm_capture::ee_astar_cntq_i(id as i32);
        crate::astar::cm_capture::ee_astar_cntq_i(i32::from(now));
    }
    // SAFETY: the caller's contract
    let record = unsafe { &mut *r };
    let regions = !record.tables().areas.is_empty() && !record.clearing;
    if regions && blocks.is_some() && host(EE_ASTAR_CM_IS_FOUNDATION, unit, [0; 4]) == 0 && blocks == Some(true) {
        let mut tiles = Vec::new();
        crate::units::uu_ffi::unit_tiles(0, unit, &mut tiles);
        let points: Vec<EeHullPoint> =
            tiles.iter().map(|p| EeHullPoint { x: p[0] as c_long, y: p[1] as c_long }).collect();
        // SAFETY: the live record; the points above
        unsafe {
            crate::astar::cm_passes::ee_astar_cm_add_sector_tiles(
                r,
                if points.is_empty() { std::ptr::null() } else { points.as_ptr() },
                points.len(),
            )
        };
        host(EE_ASTAR_CM_REGION_UPDATES, 0, [i64::from(now), 0, 0, 0]);
    }
    if on {
        // SAFETY: the live record
        unsafe { crate::astar::cm_queries::ee_astar_cntq_state(r) };
    }
    crate::astar::cm_capture::ee_astar_cntq_leave(on);
}

/// `EEAIForest::RegrowTrees`: when the tile is in the forest, `count` trees (unit type 0x147) at random forest
/// tiles within `radius` of it (11 tries each, else the tile), flagged as regrowing -> whether it is in the forest.
///
/// # Safety
/// `forest` is a live forest.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_forest_regrow(
    forest: u64,
    x: c_long,
    y: c_long,
    radius: c_long,
    count: c_long,
) -> bool {
    if !cm_io::contains(forest, x, y) {
        return false;
    }
    // U2DRectangle: Set((x, y)), then Grow(radius, true)
    let (left, top) = ((x - radius).max(0), (y - radius).max(0));
    let (right, bottom) = (x + radius, y + radius);
    for _ in 0..count.max(0) {
        let mut point = (x, y);
        let mut found = false;
        let mut tries = 10;
        while tries >= 0 && !found {
            let ty = host(EE_ASTAR_CM_RANDOM_RANGE, 0, [top as i64, bottom as i64, 0, 0]) as c_long;
            let tx = host(EE_ASTAR_CM_RANDOM_RANGE, 0, [left as i64, right as i64, 0, 0]) as c_long;
            point = (tx, ty);
            found = cm_io::contains(forest, tx, ty);
            tries -= 1;
        }
        if !found {
            point = (x, y);
        }
        let unit = host(EE_ASTAR_CM_REGROW_UNIT, 0, [point.0 as i64, point.1 as i64, 0, 0]) as u64;
        host(EE_ASTAR_CM_SET_REGROWING, unit, [0; 4]);
    }
    true
}
