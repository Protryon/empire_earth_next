//! The next engine host's operations whose bodies moved to Rust in the AI Thread cluster's round 7 (were
//! EECPNext.cpp's ee_ai_next_host cases): the trace line under a scope, the town centre tile, the population
//! figures, the resource centres, a goal's target, a forest's point, the danger zones, the reflexes' orders, a
//! player's units of a type, the units' distance, the micro step's players, the "%.2f" text. What only C++ holds is
//! one case each (EE_AI_NEXT_HOST_* from 63; the computer player's EE_CP_PLAYER_* from ee_sim::cp).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::c_void;
use std::os::raw::c_long;

use super::*;
use crate::cp::lists::{self, EeCpDangerZone};
use crate::cp::place_game::{self as pg, EeCpPlaceIo};
use crate::cp::place_host::EeCpUnitInfo;
use crate::cp::player_game::{self as pl, ctx_of};

/// `EECPTraceOut(player, (const char*)b, "%s", (const char*)c)`
pub const EE_AI_NEXT_HOST_TRACE_OUT: i32 = 63;
/// unit `a`'s `SetGoal(NewAttackUnit(a, b, false, false), 0, 0)`; `SetGoal(NewLocation(GE2DPoint(b, c), true, true,
/// false, 0, false, false), 0, 0)`
pub const EE_AI_NEXT_HOST_SET_ATTACK: i32 = 64;
pub const EE_AI_NEXT_HOST_SET_LOCATION: i32 = 65;

fn cpp(op: i32, me: *mut c_void, a: i64, b: i64, c: i64, out: *mut c_void) -> i64 {
    // SAFETY: the C++ switch over the live game
    unsafe { ee_ai_next_host(op, me, a, b, c, out) }
}

/// One of the computer player's host operations (ee_sim::cp's EE_CP_PLAYER_*, EE_CP_CENTER_*).
fn player_op(me: *mut c_void, op: i32, a: u64, v: &[i64]) -> i64 {
    // the reflexes' orders have no player (`me` null): their unit operations read none
    let ctx = if me.is_null() {
        crate::cp::place_game::EeCpPlaceCtx {
            player: 0,
            player_id: 0,
            fields: std::ptr::null_mut(),
            town: std::ptr::null_mut(),
            placement: std::ptr::null_mut(),
            capture: 0,
        }
    } else {
        // SAFETY: the live computer player
        unsafe { ctx_of(me as u64) }
    };
    let mut io = EeCpPlaceIo {
        v: [0; 6],
        f: [0.0; 3],
        unit: EeCpUnitInfo::default(),
        input: std::ptr::null(),
        input_len: 0,
        list: std::ptr::null_mut(),
        order: std::ptr::null_mut(),
    };
    io.v[..v.len()].copy_from_slice(v);
    // SAFETY: as above
    unsafe { pg::host_call(&ctx, op, a, 0, &mut io) }
}

/// The operations whose bodies are Rust (None: the C++ host's).
///
/// # Safety
/// As `ee_ai_next_host`: the operation's objects are live; `out` its record.
pub(super) unsafe fn op(op: i32, me: *mut c_void, a: i64, b: i64, c: i64, out: *mut c_void) -> Option<i64> {
    let out64 = out as *mut i64;
    Some(match op {
        EE_AI_NEXT_HOST_TRACE => {
            if a != 0 {
                let saved = cpp(EE_AI_NEXT_HOST_SCOPE, me, a, 0, 0, std::ptr::null_mut());
                cpp(EE_AI_NEXT_HOST_TRACE_OUT, me, 0, b, c, std::ptr::null_mut());
                cpp(EE_AI_NEXT_HOST_SCOPE, me, saved, 0, 0, std::ptr::null_mut());
            } else {
                cpp(EE_AI_NEXT_HOST_TRACE_OUT, me, 0, b, c, std::ptr::null_mut());
            }
            0
        }
        EE_AI_NEXT_HOST_TOWN => {
            // SAFETY: the live computer player
            let ctx = unsafe { ctx_of(me as u64) };
            if ctx.town.is_null() {
                return Some(0);
            }
            // SAFETY: the placement's tile; the caller's record
            unsafe {
                *out64 = *ctx.town as i64;
                *out64.add(1) = *ctx.town.add(1) as i64;
            }
            1
        }
        EE_AI_NEXT_HOST_POPULATION => {
            // SAFETY: as above
            let ctx = unsafe { ctx_of(me as u64) };
            match a {
                0 => player_op(me, pl::EE_CP_PLAYER_TOTAL_POPULATION, 0, &[1, 0]),
                // SAFETY: the live computer player (its own GetPopulationLimit)
                1 => (unsafe { pl::ee_cp_pl_population_limit(me as u64, 0) }) as i64,
                2 => player_op(me, pl::EE_CP_PLAYER_TOTAL_POPULATION, 0, &[1, 1]),
                3 => player_op(me, pg::EE_CP_PLAYER_BASE_POP_LIMIT, 0, &[]),
                // SAFETY: the player's record
                4 => unsafe { (*ctx.fields).mPopulationCap as i64 },
                // SAFETY: as above
                _ => unsafe { (*ctx.fields).mDropOffDistance as i64 },
            }
        }
        EE_AI_NEXT_HOST_RC => {
            // SAFETY: as above; the caller's record
            let ctx = unsafe { ctx_of(me as u64) };
            unsafe { pg::ee_cp_center_info(&ctx, a as usize, out64) };
            0
        }
        EE_AI_NEXT_HOST_RC_UNIT => {
            // SAFETY: as above
            let ctx = unsafe { ctx_of(me as u64) };
            unsafe { pg::ee_cp_center_unit(&ctx, a as usize, b as usize) as i64 }
        }
        EE_AI_NEXT_HOST_RC_CENTER => {
            // SAFETY: as above
            let ctx = unsafe { ctx_of(me as u64) };
            let mut tile = [0i32; 2];
            unsafe {
                pg::ee_cp_center_get_center_at(&ctx, a as usize, tile.as_mut_ptr());
                *out64 = tile[0] as i64;
                *out64.add(1) = tile[1] as i64;
            }
            0
        }
        EE_AI_NEXT_HOST_GOAL_UNIT => {
            let g = a as u64;
            if b == 1 {
                let mut r: crate::combat::goal_attack::EeCombatAttackGoal = unsafe { std::mem::zeroed() };
                // SAFETY: the live attack goal (`Attack().target`)
                unsafe { crate::combat::goal_attack::ee_combat_attack_get(g, &mut r) };
                r.target as i64
            } else {
                let what = if b == 0 {
                    crate::units::rust_goal::EE_UNITS_GOAL_GATHER_FIELDS
                } else {
                    crate::units::rust_goal::EE_UNITS_GOAL_BUILD_FIELDS
                };
                let mut io: crate::units::rust_goal::EeUnitsGoalIo = unsafe { std::mem::zeroed() };
                // SAFETY: the live goal (GetGatherResource, GetBuildTarget)
                unsafe {
                    crate::units::goal_obj::ee_units_goal_call(
                        what,
                        g as usize as *const crate::units::goal_obj::EeUnitsGoalBase,
                        0,
                        0,
                        &mut io,
                    )
                }
            }
        }
        EE_AI_NEXT_HOST_DANGER_COUNT => {
            // SAFETY: the player's record
            unsafe { lists::ee_cp_zone_count(ctx_of(me as u64).fields) as i64 }
        }
        EE_AI_NEXT_HOST_DANGER => {
            // SAFETY: as above; the caller's record
            unsafe {
                let z = lists::ee_cp_zone_at(ctx_of(me as u64).fields, a as usize);
                *out64 = z.x as i64;
                *out64.add(1) = z.y as i64;
                *out64.add(2) = z.time as i64;
            }
            0
        }
        EE_AI_NEXT_HOST_DANGER_ADD => {
            let z = EeCpDangerZone { x: a as c_long, y: b as c_long, time: c as _ };
            // SAFETY: the player's record
            unsafe { lists::ee_cp_zone_push(ctx_of(me as u64).fields, z) };
            0
        }
        EE_AI_NEXT_HOST_DANGER_ERASE => {
            // SAFETY: as above
            unsafe { lists::ee_cp_zone_erase(ctx_of(me as u64).fields, a as usize) };
            0
        }
        EE_AI_NEXT_HOST_ORDER => {
            // the reflexes' orders (next_reflex.rs): attack a unit, or go to a tile
            // SAFETY: the caller's record: out[0] the kind, [1] the target, [2], [3] the place, [4] the reference
            let o = unsafe { [*out64, *out64.add(1), *out64.add(2), *out64.add(3), *out64.add(4)] };
            let unit = a as u64;
            if o[4] != 0 {
                player_op(me, pg::EE_CP_CENTER_UNIT_REFERENCE, unit, &[]);
            }
            player_op(me, pl::EE_CP_PLAYER_CLEAR_GOALS, unit, &[]);
            if o[0] == 0 {
                cpp(EE_AI_NEXT_HOST_SET_ATTACK, me, a, o[1], 0, std::ptr::null_mut());
            } else {
                cpp(EE_AI_NEXT_HOST_SET_LOCATION, me, a, o[2], o[3], std::ptr::null_mut());
            }
            player_op(me, pl::EE_CP_PLAYER_START_ACTION, unit, &[ACTION_PREPARE_TO_MOVE]);
            player_op(me, pl::EE_CP_PLAYER_ACTION_CHANGED, unit, &[]);
            if o[4] != 0 {
                player_op(me, pg::EE_CP_CENTER_UNIT_DEREFERENCE, unit, &[]);
            }
            0
        }
        EE_AI_NEXT_HOST_UNITS_OF_TYPE => {
            let mut units = Vec::new();
            crate::units::uu_ffi::player_units_of_type(a as i32, b as i32, &mut units);
            for u in units {
                // SAFETY: the caller's list
                unsafe { ee_ai_next_push_handle(out, u) };
            }
            0
        }
        EE_AI_NEXT_HOST_DISTANCE2 => {
            // SAFETY: the live units
            let d = unsafe { crate::cp::place_world::ee_cp_distance_squared_to_unit(a as u64, b as u64) };
            d.to_bits() as i64
        }
        EE_AI_NEXT_HOST_MICRO_PLAYER => {
            // SAFETY: the world's players
            let cp = unsafe { crate::cp::player_game::computer_at(a as u32) };
            if cp == 0 {
                return Some(0);
            }
            let p = cp as usize as *mut c_void;
            // SAFETY: the live computer player's record
            let active = unsafe { (*ctx_of(cp).fields).mActive };
            if player_op(p, pl::EE_CP_PLAYER_UNITS_STATE, 0, &[]) == 0 || player_op(p, pl::EE_CP_PLAYER_NEXT_STATE, 0, &[]) == 0 || !active
            {
                return Some(0);
            }
            cp as i64
        }
        EE_AI_NEXT_HOST_FORMAT_F2 => {
            let text = format!("{:.2}", f64::from_bits(a as u64));
            // SAFETY: the caller's byte list
            unsafe { ee_ai_next_append(out, text.as_ptr(), text.len()) };
            0
        }
        _ => return None,
    })
}

/// `kActionPrepareToMove`
const ACTION_PREPARE_TO_MOVE: i64 = 8000;
