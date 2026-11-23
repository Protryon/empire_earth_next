//! A forest's RegrowTrees (AI Thread cluster round 8; was EEAIContinentManager.cpp's): new trees at random forest tiles
//! round a felled one; the areas' random points (AI Thread cluster round 9; were EEAIRectangleArea.cpp's
//! AreaRandomPoint and EEAIRectangleArea::GetRandomPoint). What only C++ holds is one host case each
//! (EE_ASTAR_CM_* 46..49). (UpdateWallUnit is the Paths lane's: cm_unit.rs.)

#![cfg(not(test))]
#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::os::raw::c_long;

use crate::astar::cm_io::{self, EeAStarCmIo};

/// `gPhysics->GetRandom(i0, i1)` (the longs')
pub const EE_ASTAR_CM_RANDOM_RANGE: i32 = 46;
/// `gUnitClassTable[kUnitTypeResEagle]->CreateUnit(i0, i1, 0, false, NULL)->mComplexUnit`; its `mIsRegrowing = true`
pub const EE_ASTAR_CM_REGROW_UNIT: i32 = 47;
pub const EE_ASTAR_CM_SET_REGROWING: i32 = 48;
/// `&gPhysics->mRandom` (the physics' `URandom`, an `EeURandom`)
pub const EE_ASTAR_CM_PHYSICS_RANDOM: i32 = 49;

fn host(op: i32, h: u64, i: [i64; 4]) -> i64 {
    let mut io = EeAStarCmIo { h: [h, 0], i };
    cm_io::call_host(op, &mut io)
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

/// `EEAIArea::AreaRandomPoint`: a random tile of the area from the physics' stream (`URandom::Generate` draws, as
/// the tile set's `GetRandomPoint(gPhysics->mRandom)`), (0, 0) when it is empty.
///
/// # Safety
/// `area` a live area.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_random_point(area: u64) -> crate::astar::hull::EeHullPoint {
    let tiles = cm_io::area_tiles(area) as *const ee_base::sparse::EeSparseArray;
    if ee_base::sparse::ee_sparse_size(tiles) <= 0 {
        return crate::astar::hull::EeHullPoint::new(0, 0);
    }
    let random = host(EE_ASTAR_CM_PHYSICS_RANDOM, 0, [0; 4]) as usize as *mut ee_base::random::EeURandom;
    let p = (*tiles).set.random_point(&mut || ee_base::random::ee_urandom_generate(random));
    crate::astar::hull::EeHullPoint::new(p.x as c_long, p.y as c_long)
}

/// `EEAIRectangleArea::GetRandomPoint`: the area's random tile; when the map has no tile there, a random on-map tile
/// of the area instead (`cm_inline::rect_on_map_point`; unchanged when there is none).
///
/// # Safety
/// `area` a live rectangle area.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_rect_random_point(area: u64) -> crate::astar::hull::EeHullPoint {
    let result = ee_astar_area_random_point(area);
    let t = crate::world::WorldRead::begin();
    let Some(map) = crate::world::WorldView::global(&t).and_then(|w| w.map()) else { return result };
    if map.tile(result.x as i64, result.y as i64).is_some() {
        return result;
    }
    let mut n = 0usize;
    let points = ee_base::sparse::ee_sparse_points(cm_io::area_tiles(area), false, 1, &mut n);
    let points = if n == 0 { &[][..] } else { std::slice::from_raw_parts(points, n) };
    let pick = crate::astar::cm_inline::rect_on_map_point(
        points,
        |p| map.tile(p.x as i64, p.y as i64).is_some(),
        |a, b| host(EE_ASTAR_CM_RANDOM_RANGE, 0, [a as i64, b as i64, 0, 0]) as i32,
    );
    pick.map_or(result, |p| crate::astar::hull::EeHullPoint::new(p.x as c_long, p.y as c_long))
}
