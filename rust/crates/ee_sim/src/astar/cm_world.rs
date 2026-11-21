//! A forest's RegrowTrees (AI Thread cluster round 8; was EEAIContinentManager.cpp's): new trees at random forest tiles
//! round a felled one. What only C++ holds is one host case each (EE_ASTAR_CM_* 46..48). (UpdateWallUnit is the
//! Paths lane's: cm_unit.rs.)

#![cfg(not(test))]
#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::os::raw::c_long;

use crate::astar::cm_io::{self, EeAStarCmIo};

/// `gPhysics->GetRandom(i0, i1)` (the longs')
pub const EE_ASTAR_CM_RANDOM_RANGE: i32 = 46;
/// `gUnitClassTable[kUnitTypeResEagle]->CreateUnit(i0, i1, 0, false, NULL)->mComplexUnit`; its `mIsRegrowing = true`
pub const EE_ASTAR_CM_REGROW_UNIT: i32 = 47;
pub const EE_ASTAR_CM_SET_REGROWING: i32 = 48;

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
