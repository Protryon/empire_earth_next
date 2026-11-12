//! The physics models' pure arithmetic of a unit's step (RUST_PLAN R5b.4 leaf 6, "the updated plan" 2), over the
//! velocities in the unit store: `EEPhysicsModel::AfterStep`'s roll decay (0x556710) and `EEPhysicsModel::Move`'s
//! push scale, stop distance and clamp to the unit's speed (0x556213, up to the new point). The C++ keeps the tile
//! tests (`EECanStandOn`), the location's write, the animation and the push's reset; the push velocities, the roll and
//! the location are read from the unit's record (the members handed in for a unit with no record).
//!
//! Determinism (RUST_PLAN 2.1): the same `f32` operations in the C++'s order (no FMA: Rust never contracts), the
//! `max` of `<windows.h>` (`a > b ? a : b`), no random draw.

use crate::step::{record_of, Reads};
use crate::store::Store;

/// `kPMMinimumStep`, `kPMStopDistance`, `kPMMoveDistance` (EEPhysicsModel.cpp 0x44238c, 0x442380, 0x442388).
const PM_MINIMUM_STEP: f32 = 0.005;
const PM_STOP_DISTANCE: f32 = 0.02;
const PM_MOVE_DISTANCE: f32 = 0.03;

/// `AfterStep`'s roll decay (the C++ calls it only for a roll that is not 0): toward 0 by half the tick's seconds,
/// not past it.
pub fn roll_decay(roll: f32, tick_seconds: f32) -> f32 {
    if roll > 0.0 {
        let mut r = roll;
        r -= tick_seconds * 0.5;
        if r <= 0.0 {
            r = 0.0;
        }
        r
    } else {
        let mut r = tick_seconds * 0.5 + roll;
        if r >= 0.0 {
            r = 0.0;
        }
        r
    }
}

/// What `Move`'s arithmetic gives: whether the unit moves (the push is past the stop distance), whether the move is
/// past the move distance (the shuffle animation's test), the push's size per axis after the clamp, the new point.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPush {
    pub moving: u8,
    pub past_move: u8,
    pub dx: f32,
    pub dy: f32,
    pub x: f32,
    pub y: f32,
}

/// `Move`'s push arithmetic: the push velocity times the collision period, its sizes per axis (the signs kept), zero
/// under the stop distance, the larger clamped to the speed (`max(max_velocity, kPMMinimumStep / tick_seconds)`) and
/// the other scaled with it, then the point a tick's seconds along it from the location.
#[allow(clippy::too_many_arguments, clippy::assign_op_pattern)] // the C++'s statements as written
pub fn push(
    push_x: f32,
    push_y: f32,
    period: u32,
    max_velocity: f32,
    tick_seconds: f32,
    loc_x: f32,
    loc_y: f32,
) -> EeUnitsPush {
    let limit = PM_MINIMUM_STEP / tick_seconds;
    let speed = if max_velocity > limit { max_velocity } else { limit };
    let ticks = period as f32;
    let mut dx = push_x;
    dx *= ticks;
    let mut dy = push_y;
    dy *= ticks;
    let sign_x = if dx > 0.0 { 1.0f32 } else { -1.0f32 };
    let sign_y = if dy > 0.0 { 1.0f32 } else { -1.0f32 };
    if dx < 0.0 {
        dx = -dx;
    }
    if dy < 0.0 {
        dy = -dy;
    }
    if dy + dx < PM_STOP_DISTANCE {
        dy = 0.0;
        dx = 0.0;
    }
    let mut out = EeUnitsPush { dx, dy, ..Default::default() };
    if dx != 0.0 || dy != 0.0 {
        if dx > speed || dy > speed {
            if dx > dy {
                dy = speed / dx * dy;
                dx = speed;
            } else {
                dx = speed / dy * dx;
                dy = speed;
            }
        }
        let mut x = tick_seconds * sign_x * dx;
        x += loc_x;
        let mut y = tick_seconds * sign_y * dy;
        y += loc_y;
        out = EeUnitsPush { moving: 1, past_move: u8::from(dy + dx > PM_MOVE_DISTANCE), dx, dy, x, y };
    }
    out
}

/// The roll decay over the store (`member`: the C++ member's roll).
pub fn step_roll(store: &Store, reads: &mut Reads, id: u32, object: u64, member: f32, tick_seconds: f32) -> f32 {
    let roll = match record_of(store, id, object) {
        Some(r) => {
            reads.mismatches += u32::from(r.roll != member.to_bits());
            f32::from_bits(r.roll)
        }
        None => {
            reads.unrecorded += 1;
            member
        }
    };
    roll_decay(roll, tick_seconds)
}

/// The members `step_push` reads from the record (the push velocities, the location's x and y).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPushIn {
    pub push_x: f32,
    pub push_y: f32,
    pub loc_x: f32,
    pub loc_y: f32,
    /// `gCollisionPeriod`
    pub period: u32,
    /// the unit's `GetMaxVelocity()`
    pub max_velocity: f32,
    pub tick_seconds: f32,
}

/// `Move`'s arithmetic over the store (`member`: the members, used for a unit with no record).
pub fn step_push(store: &Store, reads: &mut Reads, id: u32, object: u64, member: &EeUnitsPushIn) -> EeUnitsPush {
    let mut v = *member;
    match record_of(store, id, object) {
        Some(r) => {
            let same = r.push_velocity_x == member.push_x.to_bits()
                && r.push_velocity_y == member.push_y.to_bits()
                && r.location[0] == member.loc_x.to_bits()
                && r.location[1] == member.loc_y.to_bits();
            reads.mismatches += u32::from(!same);
            v.push_x = f32::from_bits(r.push_velocity_x);
            v.push_y = f32::from_bits(r.push_velocity_y);
            v.loc_x = f32::from_bits(r.location[0]);
            v.loc_y = f32::from_bits(r.location[1]);
        }
        None => reads.unrecorded += 1,
    }
    push(v.push_x, v.push_y, v.period, v.max_velocity, v.tick_seconds, v.loc_x, v.loc_y)
}

/// `AfterStep`'s roll decay over the store: the new roll.
#[no_mangle]
pub extern "C" fn ee_units_step_roll(id: u32, object: u64, member: f32, tick_seconds: f32) -> f32 {
    crate::store::with_store(|s| crate::step::with_reads(|reads| step_roll(s, reads, id, object, member, tick_seconds)))
}

/// `Move`'s push arithmetic over the store.
///
/// # Safety
/// `member` points to a valid input.
#[no_mangle]
pub unsafe extern "C" fn ee_units_step_push(id: u32, object: u64, member: *const EeUnitsPushIn) -> EeUnitsPush {
    // SAFETY: the caller's contract
    let member = unsafe { *member };
    crate::store::with_store(|s| crate::step::with_reads(|reads| step_push(s, reads, id, object, &member)))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::store::EeUnitRecord;

    #[test]
    fn the_roll_levels_toward_zero() {
        let want = 0.3f32 - 0.125f32 * 0.5;
        assert_eq!(roll_decay(0.3, 0.125).to_bits(), want.to_bits());
        assert_eq!(roll_decay(0.05, 0.125), 0.0);
        let want = 0.125f32 * 0.5 + -0.3f32;
        assert_eq!(roll_decay(-0.3, 0.125).to_bits(), want.to_bits());
        assert_eq!(roll_decay(-0.05, 0.125), 0.0);
        assert!(roll_decay(f32::NAN, 0.125).is_nan());
    }

    #[test]
    fn a_small_push_stops() {
        let p = push(0.001, -0.001, 3, 2.0, 0.125, 10.5, 20.5);
        assert_eq!(p.moving, 0);
        assert_eq!((p.dx, p.dy), (0.0, 0.0));
    }

    /// the y sign of a push down (`theSignY`)
    const SIGN_DOWN: f32 = -1.0;

    #[test]
    fn a_push_moves_in_the_cs_order() {
        let p = push(0.5, -0.25, 2, 2.0, 0.125, 10.5, 20.5);
        assert_eq!(p.moving, 1);
        assert_eq!(p.past_move, 1);
        assert_eq!((p.dx, p.dy), (1.0, 0.5));
        assert_eq!(p.x.to_bits(), (0.125f32 * 1.0 * 1.0 + 10.5).to_bits());
        assert_eq!(p.y.to_bits(), (0.125f32 * SIGN_DOWN * 0.5 + 20.5).to_bits());
    }

    #[test]
    fn a_push_past_the_speed_is_clamped_on_its_larger_axis() {
        let p = push(3.0, 1.0, 2, 2.0, 0.125, 0.0, 0.0);
        assert_eq!(p.dx, 2.0);
        assert_eq!(p.dy.to_bits(), (2.0f32 / 6.0 * 2.0).to_bits());
        let p = push(-1.0, -3.0, 2, 2.0, 0.125, 0.0, 0.0);
        assert_eq!(p.dy, 2.0);
        assert_eq!(p.dx.to_bits(), (2.0f32 / 6.0 * 2.0).to_bits());
        assert!(p.x < 0.0 && p.y < 0.0);
        // a slow unit's speed is at least the minimum step per tick
        let p = push(1.0, 0.0, 1, 0.0, 0.125, 0.0, 0.0);
        assert_eq!(p.dx, 0.005f32 / 0.125);
    }

    #[test]
    fn the_store_leaves_read_the_record() {
        let mut s = Store::new();
        s.insert(EeUnitRecord {
            id: 2,
            object: 0x20,
            roll: 0.3f32.to_bits(),
            push_velocity_x: 0.5f32.to_bits(),
            push_velocity_y: (-0.25f32).to_bits(),
            location: [10.5f32.to_bits(), 20.5f32.to_bits(), 0],
            ..Default::default()
        });
        let mut reads = Reads::default();
        assert_eq!(step_roll(&s, &mut reads, 2, 0x20, 0.3, 0.125), roll_decay(0.3, 0.125));
        let m = EeUnitsPushIn {
            push_x: 0.5,
            push_y: -0.25,
            loc_x: 10.5,
            loc_y: 20.5,
            period: 2,
            max_velocity: 2.0,
            tick_seconds: 0.125,
        };
        assert_eq!(step_push(&s, &mut reads, 2, 0x20, &m), push(0.5, -0.25, 2, 2.0, 0.125, 10.5, 20.5));
        assert_eq!((reads.mismatches, reads.unrecorded), (0, 0));
        let other = EeUnitsPushIn { push_x: 0.0, ..m };
        assert_eq!(step_push(&s, &mut reads, 2, 0x20, &other).moving, 1);
        assert_eq!(reads.mismatches, 1);
        assert_eq!(step_push(&s, &mut reads, 9, 0x90, &other).moving, 1);
        assert_eq!(reads.unrecorded, 1);
    }
}
