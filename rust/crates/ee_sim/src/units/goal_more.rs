//! More of the goal host's compound operations split into their C++ statements (the units' lane, round 8; were
//! EEUGRust.cpp `GoalOp`'s BOARDING, SURROUNDING_TILES, NEXT_RESOURCE, RESOURCE_OF_KIND and MINING_BEAM cases and
//! the rendezvous' unit list): [`split_more`] answers them after goal_split.rs's `split`, each C++ statement one
//! `EE_UNITS_GOAL_HOST_X_*` case of `GoalOp` (from 800), asked in the original's order. The lists the C++ took as
//! fresh locals are the host's file statics, cleared where the original declared them.

use crate::units::goal_split::GoalSplitHost;
use crate::units::rust_goal::*;
use std::ffi::c_void;

// ---- the statements (`GoalOp`'s cases)
/// container `b`'s transport class's `FindBoardingLocation(a, b, GE3DPoint(f[0..3]), ` the point at `f[3..6])`
/// (800 and 801, the shared point's store and read, are gone: the points travel in `f`, round 14)
pub const EE_UNITS_GOAL_HOST_X_BOARDING_FIND: i32 = 802;
/// the shared unit list's `clear()`
pub const EE_UNITS_GOAL_HOST_X_UNITS_CLEAR: i32 = 803;
/// the shared unit list's `push_back(unit b)`
pub const EE_UNITS_GOAL_HOST_X_UNITS_PUSH: i32 = 804;
/// the shared complex unit list's `clear()`
pub const EE_UNITS_GOAL_HOST_X_COMPLEXES_CLEAR: i32 = 805;
/// the shared complex unit list's `push_back(complex unit b)`
pub const EE_UNITS_GOAL_HOST_X_COMPLEXES_PUSH: i32 = 806;
/// `a->mUnitClass->FindNextResource(a, b, i[0], &` the unit list `)`
pub const EE_UNITS_GOAL_HOST_X_NEXT_RESOURCE: i32 = 807;
/// `a->mUnitClass->FindResourceOfKind(a, b, unit i[3], i[0], i[1], &` the unit list `, i[2] != 0)`
pub const EE_UNITS_GOAL_HOST_X_RESOURCE_OF_KIND: i32 = 808;
/// the shared tile list's `clear()`
pub const EE_UNITS_GOAL_HOST_X_TILES_CLEAR: i32 = 809;
/// `a->mUnitClass->GetSurroundingTiles(a, ` the tile list `, 1, 0)`
pub const EE_UNITS_GOAL_HOST_X_SURROUNDING_FILL: i32 = 810;
/// the tile list's `size()`
pub const EE_UNITS_GOAL_HOST_X_TILES_COUNT: i32 = 811;
/// the tile list's `[i[0]].GetX()`
pub const EE_UNITS_GOAL_HOST_X_TILE_X: i32 = 812;
/// the tile list's `[i[0]].GetY()`
pub const EE_UNITS_GOAL_HOST_X_TILE_Y: i32 = 813;
/// `gWorld.GetEffectManager()->CreateEffect(a, GE3DPoint(f[0..3]), ` the mining beam `, &` the shared effect `, 0)`
pub const EE_UNITS_GOAL_HOST_X_MINING_BEAM_EFFECT: i32 = 814;
/// `a->mFireEffect = ` the shared effect
pub const EE_UNITS_GOAL_HOST_X_SET_FIRE_EFFECT: i32 = 815;
/// the shared effect `= EEEffectHandle()`
pub const EE_UNITS_GOAL_HOST_X_RESET_EFFECT: i32 = 816;

fn op0<H: GoalSplitHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64) -> i64 {
    h.raw(op, a, b, &mut EeUnitsGoalIo::default())
}

/// The shared unit list from the operation's handles (`UnitsIn`).
fn units_in<H: GoalSplitHost + ?Sized>(h: &mut H, io: &EeUnitsGoalIo, clear: i32, push: i32) {
    op0(h, clear, 0, 0);
    let list: Vec<u64> = if io.list.is_null() {
        Vec::new()
    } else {
        // SAFETY: the operation's caller hands `list_len` handles at `list`
        unsafe { std::slice::from_raw_parts(io.list, io.list_len) }.to_vec()
    };
    for unit in list {
        op0(h, push, 0, unit);
    }
}

/// The rendezvous' units (`EEComplexUnit*`s of the operation's handles) into the shared complex unit list.
pub fn complexes_in<H: GoalSplitHost + ?Sized>(h: &mut H, io: &EeUnitsGoalIo) {
    units_in(h, io, EE_UNITS_GOAL_HOST_X_COMPLEXES_CLEAR, EE_UNITS_GOAL_HOST_X_COMPLEXES_PUSH);
}

/// Answers the compound operation `op` (`Some`) from its statements, or `None`.
pub fn split_more<H: GoalSplitHost + ?Sized>(
    h: &mut H,
    op: i32,
    a: u64,
    b: u64,
    io: &mut EeUnitsGoalIo,
) -> Option<i64> {
    Some(match op {
        EE_UNITS_GOAL_HOST_BOARDING => {
            // FindBoardingLocation(unit, container, f[0..3], out f[3..6]): the out point starts as the one handed
            let mut q = EeUnitsGoalIo::default();
            q.f[..6].copy_from_slice(&io.f[..6]);
            h.raw(EE_UNITS_GOAL_HOST_X_BOARDING_FIND, a, b, &mut q);
            io.f[3..6].copy_from_slice(&q.f[3..6]);
            0
        }
        EE_UNITS_GOAL_HOST_SURROUNDING_TILES => {
            op0(h, EE_UNITS_GOAL_HOST_X_TILES_CLEAR, 0, 0);
            op0(h, EE_UNITS_GOAL_HOST_X_SURROUNDING_FILL, a, 0);
            let count = op0(h, EE_UNITS_GOAL_HOST_X_TILES_COUNT, 0, 0);
            let mut tiles = Vec::new();
            for k in 0..count {
                let mut q = EeUnitsGoalIo::default();
                q.i[0] = k;
                let x = h.raw(EE_UNITS_GOAL_HOST_X_TILE_X, 0, 0, &mut q) as i32;
                let y = h.raw(EE_UNITS_GOAL_HOST_X_TILE_Y, 0, 0, &mut q) as i32;
                tiles.push((x, y));
            }
            push_tiles(io.sink, &tiles);
            0
        }
        EE_UNITS_GOAL_HOST_NEXT_RESOURCE => {
            units_in(h, io, EE_UNITS_GOAL_HOST_X_UNITS_CLEAR, EE_UNITS_GOAL_HOST_X_UNITS_PUSH);
            let mut q = EeUnitsGoalIo::default();
            q.i[0] = io.i[0];
            h.raw(EE_UNITS_GOAL_HOST_X_NEXT_RESOURCE, a, b, &mut q)
        }
        EE_UNITS_GOAL_HOST_RESOURCE_OF_KIND => {
            units_in(h, io, EE_UNITS_GOAL_HOST_X_UNITS_CLEAR, EE_UNITS_GOAL_HOST_X_UNITS_PUSH);
            let mut q = EeUnitsGoalIo::default();
            q.i[..4].copy_from_slice(&io.i[..4]);
            h.raw(EE_UNITS_GOAL_HOST_X_RESOURCE_OF_KIND, a, b, &mut q)
        }
        EE_UNITS_GOAL_HOST_MINING_BEAM => {
            // the beam from the unit to the resource's position, kept as the unit's fire effect
            let at = h.position(b);
            op0(h, EE_UNITS_GOAL_HOST_X_RESET_EFFECT, 0, 0);
            let mut q = EeUnitsGoalIo::default();
            q.f[..3].copy_from_slice(&at);
            h.raw(EE_UNITS_GOAL_HOST_X_MINING_BEAM_EFFECT, a, 0, &mut q);
            op0(h, EE_UNITS_GOAL_HOST_X_SET_FIRE_EFFECT, a, 0);
            0
        }
        _ => return None,
    })
}

/// The tiles onto the operation's sink (`ee_units_goal_push_tile`'s `Vec`); none without one.
fn push_tiles(sink: *mut c_void, tiles: &[(i32, i32)]) {
    if sink.is_null() {
        return;
    }
    // SAFETY: the sink is the `Vec<(i32, i32)>` the operation's caller made (rust_goal.rs `surrounding_tiles`)
    let v = unsafe { &mut *sink.cast::<Vec<(i32, i32)>>() };
    v.extend_from_slice(tiles);
}

#[cfg(test)]
#[path = "goal_more_tests.rs"]
mod tests;
