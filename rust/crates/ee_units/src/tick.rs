//! The physics tick's unit loop (RUST_PLAN R5b.4 leaf 6, "the updated plan" 3, started): `EEPhysics::UpdateUnits`
//! (0x55401e) over the stepped units' Rust list (`order`): the collision period (the stepped units spread over
//! `gCollisionDetectionUnit` ticks, at least 1), every player's update, then each stepped unit's `Step` from the last
//! to the first. The players' updates and the steps are host calls (the step is `AIStep` then the class's `Update`:
//! `ai`, `step`, `motion` and the C++); the list is read again before each step, by index, as the C++ indexes its
//! deque (nothing changes the list inside a step: the activate and deactivate queues run at the tick's start).
//!
//! Determinism (RUST_PLAN 2.1): the period's `f32` division and `lrintf` (round half to even, as `fld`/`fistp`) as the
//! C++'s `UMCeilUnsigned`; the steps in the list's order, so the one random stream is drawn in the same order.

use std::ffi::c_void;

/// `lrintf` as the x86-64 build computes it: round half to even, the conversion's "indefinite" value for NaN or out
/// of range.
fn lrintf(value: f32) -> i32 {
    let rounded = value.round_ties_even();
    if (-2_147_483_648.0..2_147_483_648.0).contains(&rounded) {
        rounded as i32
    } else {
        i32::MIN
    }
}

/// `UMCeilUnsigned((float)count / unit)`, at least 1: the collision period. The Windows build's 32-bit unsigned longs.
pub fn collision_period(count: u32, unit: u32) -> u32 {
    let value = count as f32 / unit as f32;
    let mut period = lrintf(value) as u32;
    if (period as f32) < value {
        period = period.wrapping_add(1);
    }
    if period == 0 {
        1
    } else {
        period
    }
}

/// The C++ calls of the unit loop (EEPhysics.cpp). `ctx` is the host's.
#[repr(C)]
pub struct EeUnitsTickHost {
    pub ctx: *mut c_void,
    /// `gWorld.mNumPlayers`
    pub num_players: u32,
    /// `gCollisionDetectionUnit`
    pub detection_unit: u32,
    /// `gCollisionPeriod = value`
    pub set_collision_period: extern "C" fn(ctx: *mut c_void, value: u32),
    /// `gWorld.mPlayers[index]->UpdatePlayer()` when the slot has a player
    pub update_player: extern "C" fn(ctx: *mut c_void, index: u32),
    /// `gWorld.mUpdateUnits[index]->Step()` (the list's id at the index handed in: the host checks it)
    pub step: extern "C" fn(ctx: *mut c_void, index: u32, id: u32),
}

/// The loop's calls as a trait (the FFI table, the tests' mock).
pub trait TickHost {
    fn set_collision_period(&mut self, value: u32);
    fn update_player(&mut self, index: u32);
    fn step(&mut self, index: u32, id: u32);
}

impl TickHost for &EeUnitsTickHost {
    fn set_collision_period(&mut self, value: u32) {
        (self.set_collision_period)(self.ctx, value)
    }
    fn update_player(&mut self, index: u32) {
        (self.update_player)(self.ctx, index)
    }
    fn step(&mut self, index: u32, id: u32) {
        (self.step)(self.ctx, index, id)
    }
}

/// `EEPhysics::UpdateUnits` over a list read through `ids` (the count, or the id at an index; the lock is not held
/// across a host call): returns the stepped units' count at the start, as the C++ does.
pub fn update_units(
    host: &mut impl TickHost,
    num_players: u32,
    detection_unit: u32,
    len: &mut dyn FnMut() -> usize,
    id_at: &mut dyn FnMut(usize) -> Option<u32>,
) -> u32 {
    let count = len() as u32;
    host.set_collision_period(collision_period(count, detection_unit));
    for i in 0..num_players {
        host.update_player(i);
    }
    let n = len();
    for j in (0..n).rev() {
        // the C++ indexes its deque: an index past the list's end is a bug there too (none: the list does not change
        // inside a step); the id is the host's check
        let id = id_at(j).unwrap_or(u32::MAX);
        host.step(j as u32, id);
    }
    count
}

/// `EEPhysics::UpdateUnits` over the stepped units' list (`order::ORDER`).
///
/// # Safety
/// `host` points to a valid table whose calls are valid for its `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_update_units(host: *const EeUnitsTickHost) -> u32 {
    // SAFETY: the caller's contract
    let mut host = unsafe { &*host };
    let (players, unit) = (host.num_players, host.detection_unit);
    update_units(&mut host, players, unit, &mut || crate::order::with_order(|o| o.len()), &mut |j| {
        crate::order::with_order(|o| o.ids().get(j).copied())
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Mock {
        log: Vec<String>,
    }

    impl TickHost for Mock {
        fn set_collision_period(&mut self, value: u32) {
            self.log.push(format!("period {value}"));
        }
        fn update_player(&mut self, index: u32) {
            self.log.push(format!("player {index}"));
        }
        fn step(&mut self, index: u32, id: u32) {
            self.log.push(format!("step {index} {id}"));
        }
    }

    #[test]
    fn the_period_is_the_ceiling_at_least_one() {
        assert_eq!(collision_period(0, 2000), 1);
        assert_eq!(collision_period(1, 2000), 1);
        assert_eq!(collision_period(2000, 2000), 1);
        assert_eq!(collision_period(2001, 2000), 2);
        assert_eq!(collision_period(3000, 2000), 2);
        assert_eq!(collision_period(5000, 2000), 3);
        assert_eq!(collision_period(10, 4), 3);
        // a unit of 0 (the toggle's off): an infinite quotient, the conversion's indefinite value, then 0 + 1
        assert_eq!(collision_period(10, 0), 0x8000_0001);
    }

    #[test]
    fn players_then_the_units_last_first() {
        let ids = [11u32, 12, 13];
        let mut m = Mock::default();
        let n = update_units(&mut m, 2, 2000, &mut || ids.len(), &mut |j| ids.get(j).copied());
        assert_eq!(n, 3);
        assert_eq!(m.log, ["period 1", "player 0", "player 1", "step 2 13", "step 1 12", "step 0 11"]);
    }
}
