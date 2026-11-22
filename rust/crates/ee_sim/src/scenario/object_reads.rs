//! The object evaluation's, the placement's and the conditions' host reads that were C++ statement sequences
//! (Scenario Manager/EESMObject.cpp `EESMObjectRead`, EEScenarioHost.cpp `Read`; the scenario editor lane's round 4):
//! a unit's flags, the selection test, an area's unit test, the player unit chain, a dry tile, the footprint
//! distance, the nearest footprint tile, the chat test, the continent's and a town's centre, the context unit's
//! previous tile; and the side and corner tile lists (ee_sim units). Composed here of single C++ statements
//! (`EE_SCENARIO_WZ_*`, `SMEObjectWorld` in EESMObject.cpp, through the world channel), in the C++'s order.

// c_long/c_ulong are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::scenario::attribute::{EE_SCENARIO_OF_STATS, EE_SCENARIO_READ_UNIT_FIELD, EE_SCENARIO_UNIT_VARIABLE};
use crate::scenario::host::*;
use crate::scenario::objeval::{
    EE_SCENARIO_READ_AREA_CONTAINS_UNIT, EE_SCENARIO_READ_PLAYER_UNIT, EE_SCENARIO_READ_UNIT_FLAGS,
    EE_SCENARIO_READ_UNIT_SELECTED_BY,
};
use crate::scenario::placement::{
    EE_SCENARIO_READ_FOOTPRINT_DISTANCE, EE_SCENARIO_READ_MAP_TILE_DRY, EE_SCENARIO_READ_UNIT_NEAREST_TILE,
};

/// `gWorld.GetUnit((uint32_t)a0)`; unit a0's `GetHitPoints()`, `mUnitClass->mIsTree`, `mComplexUnit != NULL`,
/// `mComplexUnit->mPreviousPart != NULL`.
pub const EE_SCENARIO_WZ_UNIT: u32 = 900;
pub const EE_SCENARIO_WZ_UNIT_HIT_POINTS: u32 = 901;
pub const EE_SCENARIO_WZ_UNIT_IS_TREE: u32 = 902;
pub const EE_SCENARIO_WZ_UNIT_COMPLEX: u32 = 903;
pub const EE_SCENARIO_WZ_UNIT_CARRIED: u32 = 904;
/// `gWorld.mPlayers[a0]`; player a0's `mActiveGroup->Contains(unit a1's mComplexUnit)`, `mViewedUnit == unit a1`,
/// `mFirstUnit`; unit a0's `mNext`.
pub const EE_SCENARIO_WZ_PLAYER_SLOT: u32 = 905;
pub const EE_SCENARIO_WZ_GROUP_CONTAINS: u32 = 906;
pub const EE_SCENARIO_WZ_VIEWED_UNIT: u32 = 907;
pub const EE_SCENARIO_WZ_FIRST_UNIT: u32 = 908;
pub const EE_SCENARIO_WZ_NEXT_UNIT: u32 = 909;
/// The context's `mCursor`, its `mCursor = a0`, `SMChainUnit(a0)`.
pub const EE_SCENARIO_WZ_CURSOR: u32 = 910;
pub const EE_SCENARIO_WZ_SET_CURSOR: u32 = 911;
pub const EE_SCENARIO_WZ_CHAIN_UNIT: u32 = 912;
/// The manager's `GetArea(a0)`; area a0's `ContainsUnit(unit a1)`.
pub const EE_SCENARIO_WZ_AREA: u32 = 913;
pub const EE_SCENARIO_WZ_AREA_CONTAINS_UNIT: u32 = 914;
/// `gWorld.mMap->GetTile(a0, a1)`; tile a0's `mTerrainType`.
pub const EE_SCENARIO_WZ_TILE: u32 = 915;
pub const EE_SCENARIO_WZ_TILE_TERRAIN: u32 = 916;
/// The bits of `EEGetFootprintDistance(unit a0 as complex, a1, a2, the longs at a3, a4)`; unit a0's
/// `GetNearestFootprintTile(unit a1, the longs at a2, a3)`.
pub const EE_SCENARIO_WZ_FOOTPRINT_DISTANCE: u32 = 917;
pub const EE_SCENARIO_WZ_NEAREST_TILE: u32 = 918;
/// The chat test's two texts (the C++'s locals): text a0 set to the context condition's `mChatText` (a0 0) or the
/// manager's `mLastChatText` (1); text a0 lowered (`PlatMbsLwr`); text 1's `Find(text 0, 0) != GetLength()`.
pub const EE_SCENARIO_WZ_CHAT_TEXT: u32 = 919;
pub const EE_SCENARIO_WZ_CHAT_LAST: u32 = 920;
pub const EE_SCENARIO_WZ_TEXT_LOWER: u32 = 921;
pub const EE_SCENARIO_WZ_TEXT_FOUND: u32 = 922;
/// The point (the C++'s local): `gContinentManager.GetContinentCenter((unsigned short)a0, it)`, `= gWorld.GetPlayer(
/// a0)->mTowns[a1]->GetHullCenter()`, its x, its y; the context unit's complex unit's `mU0DC.GetX()`, `GetY()`.
pub const EE_SCENARIO_WZ_CONTINENT_CENTER: u32 = 923;
pub const EE_SCENARIO_WZ_TOWN_CENTER: u32 = 924;
pub const EE_SCENARIO_WZ_POINT_X: u32 = 925;
pub const EE_SCENARIO_WZ_POINT_Y: u32 = 926;
pub const EE_SCENARIO_WZ_PREVIOUS_X: u32 = 927;
pub const EE_SCENARIO_WZ_PREVIOUS_Y: u32 = 928;
/// The context unit's complex unit's `mUnitVariables[a0]`; the bits of its `mPhysicalModel.GetScale()`.
pub const EE_SCENARIO_WZ_UNIT_VARIABLE: u32 = 929;
pub const EE_SCENARIO_WZ_UNIT_SCALE: u32 = 930;
/// The tile list (the C++'s local vector): cleared; filled by area a0's `GetTiles`, `gContinentManager.
/// GetContinentTiles((unsigned short)a0, ..)`, `gWorld.GetPlayer(a0)->mTowns[a1]->GetTiles`,
/// `ee_units_uu_get_tiles_near_point(the point at a0, float a1, ..)`; its size; tile a0's x, y.
pub const EE_SCENARIO_WZ_TILES_CLEAR: u32 = 931;
pub const EE_SCENARIO_WZ_TILES_AREA: u32 = 932;
pub const EE_SCENARIO_WZ_TILES_CONTINENT: u32 = 933;
pub const EE_SCENARIO_WZ_TILES_TOWN: u32 = 934;
pub const EE_SCENARIO_WZ_TILES_NEAR_POINT: u32 = 935;
pub const EE_SCENARIO_WZ_TILES_COUNT: u32 = 936;
pub const EE_SCENARIO_WZ_TILE_X: u32 = 937;
pub const EE_SCENARIO_WZ_TILE_Y: u32 = 938;

/// `kTerrainDeepWater`, `kTerrainShallowWater`.
const TERRAIN_DEEP_WATER: i64 = 1;
const TERRAIN_SHALLOW_WATER: i64 = 10;

/// Two longs as one read: x low, y high (`SMPack`).
fn pack(x: c_long, y: c_long) -> i64 {
    (x as u32 as i64) | ((y as u32 as u64) << 32) as i64
}

/// The read `op` composed of the world channel's statements `w`, or `None` for the C++'s.
pub fn compose(w: &mut dyn FnMut(u32, [i64; 6]) -> i64, op: u32, a: i32, b: i32, c: i32) -> Option<i64> {
    let mut x = |op: u32, args: &[i64]| -> i64 {
        let mut v = [0i64; 6];
        v[..args.len()].copy_from_slice(args);
        w(op, v)
    };
    let (a, b, c) = (a as i64, b as i64, c as i64);
    Some(match op {
        EE_SCENARIO_READ_UNIT_FLAGS => {
            let unit = x(EE_SCENARIO_WZ_UNIT, &[a]);
            if unit == 0 {
                return Some(0);
            }
            let alive = x(EE_SCENARIO_WZ_UNIT_HIT_POINTS, &[unit]) as c_long > 0;
            let tree = x(EE_SCENARIO_WZ_UNIT_IS_TREE, &[unit]) != 0;
            let complex = x(EE_SCENARIO_WZ_UNIT_COMPLEX, &[unit]) != 0;
            let carried = complex && x(EE_SCENARIO_WZ_UNIT_CARRIED, &[unit]) != 0;
            1 | (alive as i64) << 1 | (tree as i64) << 2 | (complex as i64) << 3 | (carried as i64) << 4
        }
        EE_SCENARIO_READ_UNIT_SELECTED_BY => {
            let player = x(EE_SCENARIO_WZ_PLAYER_SLOT, &[a]);
            let unit = x(EE_SCENARIO_WZ_UNIT, &[b]);
            (x(EE_SCENARIO_WZ_GROUP_CONTAINS, &[player, unit]) != 0
                || x(EE_SCENARIO_WZ_VIEWED_UNIT, &[player, unit]) != 0) as i64
        }
        EE_SCENARIO_READ_AREA_CONTAINS_UNIT => {
            let area = x(EE_SCENARIO_WZ_AREA, &[a]);
            if area == 0 {
                -1
            } else {
                let unit = x(EE_SCENARIO_WZ_UNIT, &[b]);
                x(EE_SCENARIO_WZ_AREA_CONTAINS_UNIT, &[area, unit])
            }
        }
        EE_SCENARIO_READ_PLAYER_UNIT => {
            let next = if b == 0 {
                let player = x(EE_SCENARIO_WZ_PLAYER_SLOT, &[a]);
                if player != 0 {
                    x(EE_SCENARIO_WZ_FIRST_UNIT, &[player])
                } else {
                    0
                }
            } else {
                let cursor = x(EE_SCENARIO_WZ_CURSOR, &[]);
                x(EE_SCENARIO_WZ_NEXT_UNIT, &[cursor])
            };
            x(EE_SCENARIO_WZ_SET_CURSOR, &[next]);
            let cursor = x(EE_SCENARIO_WZ_CURSOR, &[]);
            x(EE_SCENARIO_WZ_CHAIN_UNIT, &[cursor])
        }
        EE_SCENARIO_READ_MAP_TILE_DRY => {
            let tile = x(EE_SCENARIO_WZ_TILE, &[a, b]);
            (tile != 0 && {
                let t = x(EE_SCENARIO_WZ_TILE_TERRAIN, &[tile]);
                t != TERRAIN_DEEP_WATER && t != TERRAIN_SHALLOW_WATER
            }) as i64
        }
        EE_SCENARIO_READ_FOOTPRINT_DISTANCE => {
            let (mut tx, mut ty): (c_long, c_long) = (0, 0);
            let unit = x(EE_SCENARIO_WZ_UNIT, &[a]);
            let (px, py) = (&mut tx as *mut c_long as usize as i64, &mut ty as *mut c_long as usize as i64);
            x(EE_SCENARIO_WZ_FOOTPRINT_DISTANCE, &[unit, b, c, px, py])
        }
        EE_SCENARIO_READ_UNIT_NEAREST_TILE => {
            let (mut tx, mut ty): (c_long, c_long) = (0, 0);
            let unit = x(EE_SCENARIO_WZ_UNIT, &[a]);
            let other = x(EE_SCENARIO_WZ_UNIT, &[b]);
            let (px, py) = (&mut tx as *mut c_long as usize as i64, &mut ty as *mut c_long as usize as i64);
            x(EE_SCENARIO_WZ_NEAREST_TILE, &[unit, other, px, py]);
            pack(tx, ty)
        }
        EE_SCENARIO_READ_CHAT_CONTAINS => {
            x(EE_SCENARIO_WZ_CHAT_TEXT, &[]);
            x(EE_SCENARIO_WZ_CHAT_LAST, &[]);
            x(EE_SCENARIO_WZ_TEXT_LOWER, &[0]);
            x(EE_SCENARIO_WZ_TEXT_LOWER, &[1]);
            (x(EE_SCENARIO_WZ_TEXT_FOUND, &[]) != 0) as i64
        }
        EE_SCENARIO_READ_CONTINENT_CENTER | EE_SCENARIO_READ_TOWN_CENTER => {
            if op == EE_SCENARIO_READ_CONTINENT_CENTER {
                x(EE_SCENARIO_WZ_CONTINENT_CENTER, &[a]);
            } else {
                x(EE_SCENARIO_WZ_TOWN_CENTER, &[a, b]);
            }
            pack(x(EE_SCENARIO_WZ_POINT_X, &[]) as c_long, x(EE_SCENARIO_WZ_POINT_Y, &[]) as c_long)
        }
        EE_SCENARIO_READ_UNIT_FIELD => {
            // the members no record holds (unit_views.rs answers the rest): the unit variables, else the scale
            let field = if a >= EE_SCENARIO_OF_STATS as i64 { a - EE_SCENARIO_OF_STATS as i64 } else { a };
            let variable = EE_SCENARIO_UNIT_VARIABLE as i64;
            if (variable..variable + 3).contains(&field) {
                x(EE_SCENARIO_WZ_UNIT_VARIABLE, &[field - variable])
            } else {
                x(EE_SCENARIO_WZ_UNIT_SCALE, &[])
            }
        }
        EE_SCENARIO_READ_UNIT_PREVIOUS_TILE => {
            pack(x(EE_SCENARIO_WZ_PREVIOUS_X, &[]) as c_long, x(EE_SCENARIO_WZ_PREVIOUS_Y, &[]) as c_long)
        }
        _ => return None,
    })
}

/// The host's tile lists (`Host::tiles`): the side and corner lists ee_sim units' (`EEGetSideTiles(a, b, c)`,
/// `EEGetCornerTiles(0, a, b, c)`), the area's, continent's, town's and near point's (radius `c`'s bits) through the
/// world channel's statements `w`; `None` for an op no list has.
pub fn tiles(w: &mut dyn FnMut(u32, [i64; 6]) -> i64, op: u32, a: i32, b: i32, c: i32) -> Option<Vec<(i32, i32)>> {
    use crate::scenario::placement::*;
    let list = match op {
        EE_SCENARIO_TILES_SIDE => crate::units::uu_direct::square_ring(a, b, c, c, true),
        EE_SCENARIO_TILES_CORNER => crate::units::uu_direct::ring_corners(0, a, b, c, c, true),
        _ => return cpp_tiles(w, op, a, b, c),
    };
    Some(list.into_iter().map(|t| (t[0], t[1])).collect())
}

/// The lists the C++ fills: cleared, filled, read back.
fn cpp_tiles(w: &mut dyn FnMut(u32, [i64; 6]) -> i64, op: u32, a: i32, b: i32, c: i32) -> Option<Vec<(i32, i32)>> {
    use crate::scenario::placement::*;
    let point = [a as f32, b as f32, 0.0f32];
    let (a, b) = (a as i64, b as i64);
    let fill = match op {
        EE_SCENARIO_TILES_AREA => [EE_SCENARIO_WZ_TILES_AREA as i64, a, 0],
        EE_SCENARIO_TILES_CONTINENT => [EE_SCENARIO_WZ_TILES_CONTINENT as i64, a, 0],
        EE_SCENARIO_TILES_TOWN => [EE_SCENARIO_WZ_TILES_TOWN as i64, a, b],
        EE_SCENARIO_TILES_NEAR_POINT => {
            [EE_SCENARIO_WZ_TILES_NEAR_POINT as i64, point.as_ptr() as usize as i64, c as u32 as i64]
        }
        _ => return None,
    };
    w(EE_SCENARIO_WZ_TILES_CLEAR, [0; 6]);
    w(fill[0] as u32, [fill[1], fill[2], 0, 0, 0, 0]);
    let n = w(EE_SCENARIO_WZ_TILES_COUNT, [0; 6]);
    Some(
        (0..n)
            .map(|i| {
                (
                    w(EE_SCENARIO_WZ_TILE_X, [i, 0, 0, 0, 0, 0]) as i32,
                    w(EE_SCENARIO_WZ_TILE_Y, [i, 0, 0, 0, 0, 0]) as i32,
                )
            })
            .collect(),
    )
}

/// TEST ONLY: the SMAR record's hash of an area's tiles (`EESMArea::GetTiles`): FNV-1a 64 over each tile's x and y as
/// 32-bit little-endian ints.
pub fn tiles_hash(tiles: impl IntoIterator<Item = (i32, i32)>) -> u64 {
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for (x, y) in tiles {
        for byte in x.to_le_bytes().into_iter().chain(y.to_le_bytes()) {
            h = (h ^ byte as u64).wrapping_mul(0x0100_0000_01b3);
        }
    }
    h
}

/// [`tiles_hash`] of `count` tiles (`U2DPoint<long>`s: x, y) at `xy`.
///
/// # Safety
/// `xy` must point to `2 * count` longs (or be anything when `count` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_tiles_hash(xy: *const c_long, count: usize) -> u64 {
    if count == 0 {
        return tiles_hash(std::iter::empty());
    }
    // SAFETY: the caller's contract
    let s = unsafe { std::slice::from_raw_parts(xy, 2 * count) };
    tiles_hash(s.chunks(2).map(|p| (p[0] as i32, p[1] as i32)))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_cpp_tile_lists_are_cleared_filled_and_read_back() {
        use crate::scenario::placement::*;
        let mut ops = Vec::new();
        let got = {
            let mut w = |op: u32, v: [i64; 6]| -> i64 {
                ops.push((op, v[0], v[1]));
                match op {
                    EE_SCENARIO_WZ_TILES_COUNT => 2,
                    EE_SCENARIO_WZ_TILE_X => 10 + v[0],
                    EE_SCENARIO_WZ_TILE_Y => -v[0],
                    EE_SCENARIO_WZ_TILES_NEAR_POINT => {
                        // SAFETY: the composer's point, alive through the call
                        let p = unsafe { *(v[0] as usize as *const [f32; 3]) };
                        assert_eq!((p, f32::from_bits(v[1] as u32)), ([3.0, 4.0, 0.0], 2.5));
                        0
                    }
                    _ => 0,
                }
            };
            let town = tiles(&mut w, EE_SCENARIO_TILES_TOWN, 1, 2, 0);
            let near = tiles(&mut w, EE_SCENARIO_TILES_NEAR_POINT, 3, 4, 2.5f32.to_bits() as i32);
            let other = tiles(&mut w, 99, 0, 0, 0);
            (town, near, other)
        };
        assert_eq!(got.0, Some(vec![(10, 0), (11, -1)]));
        assert!(got.1.is_some() && got.2.is_none());
        assert_eq!(ops[..2], [(EE_SCENARIO_WZ_TILES_CLEAR, 0, 0), (EE_SCENARIO_WZ_TILES_TOWN, 1, 2)]);
    }

    #[test]
    fn the_tiles_hash_is_fnv_over_the_ints() {
        assert_eq!(tiles_hash(std::iter::empty()), 0xcbf2_9ce4_8422_2325);
        let xy: [c_long; 4] = [1, -2, 300, 4];
        // SAFETY: two tiles
        let h = unsafe { ee_scenario_tiles_hash(xy.as_ptr(), 2) };
        let mut want: u64 = 0xcbf2_9ce4_8422_2325;
        for v in [1i32, -2, 300, 4] {
            for byte in v.to_le_bytes() {
                want = (want ^ byte as u64).wrapping_mul(0x100000001b3);
            }
        }
        assert_eq!(h, want);
    }

    /// Units 1.. are handles 100 + id; player slot a is 50 + a.
    fn run(
        op: u32,
        a: i32,
        b: i32,
        answer: &dyn Fn(u32, &[i64; 6]) -> Option<i64>,
    ) -> (Option<i64>, Vec<(u32, [i64; 6])>) {
        let mut ops = Vec::new();
        let r = {
            let mut w = |op: u32, v: [i64; 6]| -> i64 {
                ops.push((op, v));
                answer(op, &v).unwrap_or(match op {
                    EE_SCENARIO_WZ_UNIT => 100 + v[0],
                    EE_SCENARIO_WZ_PLAYER_SLOT => 50 + v[0],
                    _ => 0,
                })
            };
            compose(&mut w, op, a, b, 0)
        };
        (r, ops)
    }

    fn none(_: u32, _: &[i64; 6]) -> Option<i64> {
        None
    }

    #[test]
    fn unit_flags() {
        let all = |op: u32, _: &[i64; 6]| match op {
            EE_SCENARIO_WZ_UNIT_HIT_POINTS => Some(5),
            EE_SCENARIO_WZ_UNIT_IS_TREE | EE_SCENARIO_WZ_UNIT_COMPLEX | EE_SCENARIO_WZ_UNIT_CARRIED => Some(1),
            _ => None,
        };
        assert_eq!(run(EE_SCENARIO_READ_UNIT_FLAGS, 3, 0, &all).0, Some(31));
        // not complex: the carried test is not read
        let (r, ops) = run(EE_SCENARIO_READ_UNIT_FLAGS, 3, 0, &none);
        assert_eq!(r, Some(1));
        assert!(!ops.iter().any(|o| o.0 == EE_SCENARIO_WZ_UNIT_CARRIED));
        assert_eq!(
            run(EE_SCENARIO_READ_UNIT_FLAGS, 3, 0, &|op, _| (op == EE_SCENARIO_WZ_UNIT).then_some(0)).0,
            Some(0)
        );
    }

    #[test]
    fn selection_area_and_tiles() {
        assert_eq!(run(EE_SCENARIO_READ_UNIT_SELECTED_BY, 2, 7, &none).0, Some(0));
        let viewed = |op: u32, v: &[i64; 6]| (op == EE_SCENARIO_WZ_VIEWED_UNIT).then_some((v[1] == 107) as i64);
        assert_eq!(run(EE_SCENARIO_READ_UNIT_SELECTED_BY, 2, 7, &viewed).0, Some(1));
        let (r, ops) =
            run(EE_SCENARIO_READ_UNIT_SELECTED_BY, 2, 7, &|op, _| (op == EE_SCENARIO_WZ_GROUP_CONTAINS).then_some(1));
        assert_eq!(r, Some(1));
        assert!(!ops.iter().any(|o| o.0 == EE_SCENARIO_WZ_VIEWED_UNIT));
        assert_eq!(run(EE_SCENARIO_READ_AREA_CONTAINS_UNIT, 2, 7, &none).0, Some(-1));
        let (r, ops) = run(EE_SCENARIO_READ_AREA_CONTAINS_UNIT, 2, 7, &|op, _| match op {
            EE_SCENARIO_WZ_AREA => Some(9),
            EE_SCENARIO_WZ_AREA_CONTAINS_UNIT => Some(1),
            _ => None,
        });
        assert_eq!(r, Some(1));
        assert_eq!(ops.last().unwrap().1[..2], [9, 107]);
        let dry = |t: i64| {
            move |op: u32, _: &[i64; 6]| match op {
                EE_SCENARIO_WZ_TILE => Some(1),
                EE_SCENARIO_WZ_TILE_TERRAIN => Some(t),
                _ => None,
            }
        };
        assert_eq!(run(EE_SCENARIO_READ_MAP_TILE_DRY, 1, 1, &dry(4)).0, Some(1));
        assert_eq!(run(EE_SCENARIO_READ_MAP_TILE_DRY, 1, 1, &dry(TERRAIN_DEEP_WATER)).0, Some(0));
        assert_eq!(run(EE_SCENARIO_READ_MAP_TILE_DRY, 1, 1, &dry(TERRAIN_SHALLOW_WATER)).0, Some(0));
        assert_eq!(run(EE_SCENARIO_READ_MAP_TILE_DRY, 1, 1, &none).0, Some(0));
    }

    #[test]
    fn the_player_unit_chain_moves_the_cursor() {
        let (r, ops) = run(EE_SCENARIO_READ_PLAYER_UNIT, 2, 0, &|op, v| match op {
            EE_SCENARIO_WZ_FIRST_UNIT => Some(v[0] * 2),
            EE_SCENARIO_WZ_CURSOR => Some(104),
            EE_SCENARIO_WZ_CHAIN_UNIT => Some(v[0] + 1),
            _ => None,
        });
        assert_eq!(r, Some(105));
        assert!(ops.contains(&(EE_SCENARIO_WZ_SET_CURSOR, [104, 0, 0, 0, 0, 0])));
        let (_, ops) = run(EE_SCENARIO_READ_PLAYER_UNIT, 2, 1, &|op, _| match op {
            EE_SCENARIO_WZ_CURSOR => Some(104),
            EE_SCENARIO_WZ_NEXT_UNIT => Some(106),
            _ => None,
        });
        assert!(ops.contains(&(EE_SCENARIO_WZ_SET_CURSOR, [106, 0, 0, 0, 0, 0])));
        let (_, ops) =
            run(EE_SCENARIO_READ_PLAYER_UNIT, 2, 0, &|op, _| (op == EE_SCENARIO_WZ_PLAYER_SLOT).then_some(0));
        assert!(ops.contains(&(EE_SCENARIO_WZ_SET_CURSOR, [0; 6])));
    }

    #[test]
    fn points_and_the_chat_test() {
        let at = |op: u32, v: &[i64; 6]| match op {
            EE_SCENARIO_WZ_POINT_X | EE_SCENARIO_WZ_PREVIOUS_X => Some(-3),
            EE_SCENARIO_WZ_POINT_Y | EE_SCENARIO_WZ_PREVIOUS_Y => Some(4),
            EE_SCENARIO_WZ_NEAREST_TILE => {
                // SAFETY: the composer's two longs, alive through the call
                unsafe {
                    *(v[2] as usize as *mut c_long) = 6;
                    *(v[3] as usize as *mut c_long) = 7;
                }
                Some(0)
            }
            _ => None,
        };
        assert_eq!(run(EE_SCENARIO_READ_CONTINENT_CENTER, 1, 0, &at).0, Some(pack(-3, 4)));
        assert_eq!(run(EE_SCENARIO_READ_TOWN_CENTER, 1, 0, &at).0, Some(pack(-3, 4)));
        assert_eq!(run(EE_SCENARIO_READ_UNIT_PREVIOUS_TILE, 0, 0, &at).0, Some(pack(-3, 4)));
        assert_eq!(run(EE_SCENARIO_READ_UNIT_NEAREST_TILE, 1, 2, &at).0, Some(pack(6, 7)));
        let (r, ops) =
            run(EE_SCENARIO_READ_CHAT_CONTAINS, 0, 0, &|op, _| (op == EE_SCENARIO_WZ_TEXT_FOUND).then_some(1));
        assert_eq!(r, Some(1));
        let order: Vec<u32> = ops.iter().map(|o| o.0).collect();
        assert_eq!(
            order,
            vec![
                EE_SCENARIO_WZ_CHAT_TEXT,
                EE_SCENARIO_WZ_CHAT_LAST,
                EE_SCENARIO_WZ_TEXT_LOWER,
                EE_SCENARIO_WZ_TEXT_LOWER,
                EE_SCENARIO_WZ_TEXT_FOUND
            ]
        );
        assert_eq!(run(EE_SCENARIO_READ_CHAT_CONTAINS + 1000, 0, 0, &none).0, None);
        let field = |op: u32, v: &[i64; 6]| match op {
            EE_SCENARIO_WZ_UNIT_VARIABLE => Some(10 + v[0]),
            EE_SCENARIO_WZ_UNIT_SCALE => Some(99),
            _ => None,
        };
        assert_eq!(run(EE_SCENARIO_READ_UNIT_FIELD, 17, 0, &field).0, Some(11));
        assert_eq!(run(EE_SCENARIO_READ_UNIT_FIELD, 118, 0, &field).0, Some(12));
        assert_eq!(run(EE_SCENARIO_READ_UNIT_FIELD, 19, 0, &field).0, Some(99));
    }
}
