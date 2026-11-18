//! The computer players' experiment timers and the resource state (the port's save blocks 0.258 and 0.265,
//! `EECPSaveExperimentTimers` .. `EECPLoadResourceState`, ee/EECPExperimentTimers.h) whole in Rust: the timers are
//! this module's (they were function statics of the deleted C++ managers, then a C++ global; main's stages 5 and 6
//! left them unused, the blocks keep their layout), the danger zones stay the computer players' (C++ vectors, over
//! the host's operations).
//!
//! Each array is its length (64) and its values; a load reads into a copy (0 where the file has fewer players, the
//! words past 64 read and dropped) that the caller puts in place at the loaded game's first tick
//! ([`ee_cp_timers_apply`](super::ffi::ee_cp_timers_apply)): the timers' block replaces the whole set (its claim
//! ranges zero), then the resource state's block sets the claim ranges. Zero at program start, as the statics were.

#![allow(non_snake_case)]
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};
use std::sync::Mutex;

use super::file::{SaveFile, OK};
use super::save::Prim;
use crate::units::player_save::EeUnitsPlayerSaveIo;

/// The players of every array.
pub const EE_CP_TIMER_PLAYERS: usize = 64;

/// The timers, per player id (the C++ `EECPExperimentTimers`, in its order).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Timers {
    /// "boatrush2": the fishing boats counted at the last check
    pub boats: [c_long; EE_CP_TIMER_PLAYERS],
    /// "boatrush2": no boat is bought before this game ms
    pub boat_hold: [c_ulong; EE_CP_TIMER_PLAYERS],
    /// economy.forage_patches: the last patch settlement queued (game ms)
    pub settle_last: [c_ulong; EE_CP_TIMER_PLAYERS],
    /// the resource sites' guard towers: the last check (game ms)
    pub guard_last: [c_ulong; EE_CP_TIMER_PLAYERS],
    /// the citizens' militia: the last call (game ms)
    pub militia_last: [c_ulong; EE_CP_TIMER_PLAYERS],
    /// the naval flee: the last one (game ms)
    pub naval_flee_last: [c_ulong; EE_CP_TIMER_PLAYERS],
    /// the schedule's epoch last seen
    pub schedule_epoch: [c_long; EE_CP_TIMER_PLAYERS],
    /// when it was first seen (game ms)
    pub schedule_start: [c_ulong; EE_CP_TIMER_PLAYERS],
    /// military.prehistoric_army: the last threat check (game ms)
    pub threat_checked: [c_ulong; EE_CP_TIMER_PLAYERS],
    /// military.prehistoric_army: the cap is lifted until this game ms
    pub threat_until: [c_ulong; EE_CP_TIMER_PLAYERS],
    /// economy.forage_patches' claim range: the steps it grew by (saved in the resource state's block)
    pub claim_steps: [c_long; EE_CP_TIMER_PLAYERS],
    /// the last step (game ms)
    pub claim_last: [c_ulong; EE_CP_TIMER_PLAYERS],
}

impl Timers {
    pub const ZERO: Timers = Timers {
        boats: [0; EE_CP_TIMER_PLAYERS],
        boat_hold: [0; EE_CP_TIMER_PLAYERS],
        settle_last: [0; EE_CP_TIMER_PLAYERS],
        guard_last: [0; EE_CP_TIMER_PLAYERS],
        militia_last: [0; EE_CP_TIMER_PLAYERS],
        naval_flee_last: [0; EE_CP_TIMER_PLAYERS],
        schedule_epoch: [0; EE_CP_TIMER_PLAYERS],
        schedule_start: [0; EE_CP_TIMER_PLAYERS],
        threat_checked: [0; EE_CP_TIMER_PLAYERS],
        threat_until: [0; EE_CP_TIMER_PLAYERS],
        claim_steps: [0; EE_CP_TIMER_PLAYERS],
        claim_last: [0; EE_CP_TIMER_PLAYERS],
    };
}

/// The claim ranges' two arrays (steps, last step).
pub type EeCpClaims = ([c_long; EE_CP_TIMER_PLAYERS], [c_ulong; EE_CP_TIMER_PLAYERS]);

/// The running timers, a timers' load waiting for the first tick, a resource state's claim ranges waiting.
pub struct State {
    pub running: Timers,
    pub loaded: Option<Timers>,
    pub claims: Option<EeCpClaims>,
}

pub static STATE: Mutex<State> = Mutex::new(State { running: Timers::ZERO, loaded: None, claims: None });

/// `EECPSaveTimerArray`: the length, then the values, stopping at the first error.
fn save_array<T: Prim>(f: &mut SaveFile, values: &[T; EE_CP_TIMER_PLAYERS]) -> i32 {
    let mut r = f.write_ulong(EE_CP_TIMER_PLAYERS as u32);
    for &v in values {
        if r != OK {
            break;
        }
        r = T::save(f, v);
    }
    r
}

/// `EECPLoadTimerArray`: the length, then that many values (each read into a 0), the first 64 kept.
fn load_array<T: Prim + Default>(f: &mut SaveFile, out: &mut [T; EE_CP_TIMER_PLAYERS]) -> i32 {
    let (mut r, count) = f.read_ulong(0);
    let mut i = 0u32;
    while i < count && r == OK {
        let (rv, v) = T::load(f, T::default());
        r = rv;
        if (i as usize) < EE_CP_TIMER_PLAYERS {
            out[i as usize] = v;
        }
        i += 1;
    }
    r
}

/// `EECPSaveExperimentTimers`: the ten arrays.
pub fn save_timers(f: &mut SaveFile, t: &Timers) -> i32 {
    let mut r = save_array(f, &t.boats);
    for a in [&t.boat_hold, &t.settle_last, &t.guard_last, &t.militia_last, &t.naval_flee_last] {
        if r == OK {
            r = save_array(f, a);
        }
    }
    if r == OK {
        r = save_array(f, &t.schedule_epoch);
    }
    for a in [&t.schedule_start, &t.threat_checked, &t.threat_until] {
        if r == OK {
            r = save_array(f, a);
        }
    }
    r
}

/// `EECPLoadExperimentTimers`: the ten arrays into a zeroed copy (`None` on an error).
pub fn load_timers(f: &mut SaveFile) -> (i32, Option<Timers>) {
    let mut t = Timers::ZERO;
    let mut r = load_array(f, &mut t.boats);
    for a in [&mut t.boat_hold, &mut t.settle_last, &mut t.guard_last, &mut t.militia_last, &mut t.naval_flee_last] {
        if r == OK {
            r = load_array(f, a);
        }
    }
    if r == OK {
        r = load_array(f, &mut t.schedule_epoch);
    }
    for a in [&mut t.schedule_start, &mut t.threat_checked, &mut t.threat_until] {
        if r == OK {
            r = load_array(f, a);
        }
    }
    (r, (r == OK).then_some(t))
}

// ---- the resource state's host operations (the danger zones of player `index`'s computer player)
/// The zones of player `index` (0 without a computer player) into `io.i`.
pub const EE_CP_STATE_ZONES: i32 = 420;
/// Zone `io.i` of player `index`: its tile x, tile y and game ms into `io.i`, `io.f` unused; three calls, `io.b`
/// selecting 0 x, 1 y, 2 the time.
pub const EE_CP_STATE_ZONE: i32 = 421;
/// A loaded zone of player `index`: `io.i` the x, then the y, then the time (`io.b` 0, 1, 2; the time closes it).
pub const EE_CP_STATE_PUSH_ZONE: i32 = 422;
/// A loaded player `index` with no zones yet (the original's `push_back` of an empty list).
pub const EE_CP_STATE_PUSH_PLAYER: i32 = 423;

fn op(f: &mut SaveFile, op: i32, index: u32, i: i64, which: u8) -> i64 {
    let mut io = EeUnitsPlayerSaveIo { i, b: which, ..Default::default() };
    f.call_at(op, index, &mut io);
    io.i
}

/// `EECPSaveResourceState`: the claim ranges' two arrays, then per player id (`players` of them) the danger zones
/// of its computer player: the count, then each zone's tile and game ms (a player with none writes 0).
pub fn save_resource_state(f: &mut SaveFile, t: &Timers, players: u32) -> i32 {
    let mut r = save_array(f, &t.claim_steps);
    if r == OK {
        r = save_array(f, &t.claim_last);
    }
    if r == OK {
        r = f.write_ulong(players);
    }
    let mut p = 0;
    while p < players && r == OK {
        let count = op(f, EE_CP_STATE_ZONES, p, 0, 0) as u32;
        r = f.write_ulong(count);
        let mut z = 0;
        while z < count && r == OK {
            let x = op(f, EE_CP_STATE_ZONE, p, i64::from(z), 0);
            let y = op(f, EE_CP_STATE_ZONE, p, i64::from(z), 1);
            let time = op(f, EE_CP_STATE_ZONE, p, i64::from(z), 2);
            r = f.write_long(x as i32);
            if r == OK {
                r = f.write_long(y as i32);
            }
            if r == OK {
                r = f.write_ulong(time as u32);
            }
            z += 1;
        }
        p += 1;
    }
    r
}

/// `EECPLoadResourceState`: the claim ranges into a copy (returned on success), the zones pushed through the host
/// (each player's list, then its zones; a zone cut short is pushed with what was read).
pub fn load_resource_state(f: &mut SaveFile) -> (i32, Option<EeCpClaims>) {
    let mut steps = [0 as c_long; EE_CP_TIMER_PLAYERS];
    let mut last = [0 as c_ulong; EE_CP_TIMER_PLAYERS];
    let mut r = load_array(f, &mut steps);
    if r == OK {
        r = load_array(f, &mut last);
    }
    let mut players = 0u32;
    if r == OK {
        (r, players) = f.read_ulong(0);
    }
    let mut p = 0;
    while p < players && r == OK {
        let count;
        (r, count) = f.read_ulong(0);
        op(f, EE_CP_STATE_PUSH_PLAYER, p, 0, 0);
        let mut z = 0;
        while z < count && r == OK {
            let (mut x, mut y, mut time) = (0i32, 0i32, 0u32);
            (r, x) = f.read_long(x);
            if r == OK {
                (r, y) = f.read_long(y);
            }
            if r == OK {
                (r, time) = f.read_ulong(time);
            }
            op(f, EE_CP_STATE_PUSH_ZONE, p, i64::from(x), 0);
            op(f, EE_CP_STATE_PUSH_ZONE, p, i64::from(y), 1);
            op(f, EE_CP_STATE_PUSH_ZONE, p, i64::from(time), 2);
            z += 1;
        }
        p += 1;
    }
    (r, (r == OK).then_some((steps, last)))
}

#[cfg(test)]
#[path = "timers_tests.rs"]
mod tests;
