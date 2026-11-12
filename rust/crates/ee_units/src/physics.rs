//! The physics models' per-action step arithmetic (RUST_PLAN R5b.4 leaf 6, the twelfth round): the turn toward a
//! heading (`EEPhysicsModel::TurnTowardAngle` 0x554738 with `YawMode` 0x554f56), the spin down (`SpinDown`
//! 0x55512b), the steady turn (`SteadyTurn` 0x554ada), the speed (`Accelerate` 0x554bde, `SpeedMode` 0x554cb2,
//! `EEPMHelicopter::SpeedMode` 0x5593ef) and the pitch (`ApproachPitch` 0x55453a and `EEPMNaval::ApproachPitch`
//! 0x55c047, `PitchMode` 0x554e98 and `EEPMNaval::PitchMode` 0x55c146, which `EEPMSpaceship` shares). Every value is
//! handed in by the C++ (the unit's rates, the stats' or the active group's accelerations, the tick): these are the
//! plain arithmetic between the reads and the writes, which stay C++ (the transformations, the terrain, the path).
//!
//! Determinism (RUST_PLAN 2.1): the same `f32` operations in the C++'s order and association (no FMA: Rust never
//! contracts), the mode's `UMFloatToLong` as `lrintf`, the Windows build's 32-bit `long` products wrapping.

// the C++'s statements as written: its negated tests (NaN goes the way they send it), its clamps and compound
// assignments in their order, its constants' spelling
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::manual_clamp, clippy::assign_op_pattern, clippy::approx_constant)]

/// `3.1415927f`, `6.2831855f`, `1.5707964f` as the C++ writes them.
const PI: f32 = 3.141_592_7;
const TWO_PI: f32 = 6.283_185_5;
/// `kPMEdgeOffset`, `kPMRateScale`, `kPMMaximumTilt` (EEPhysicsModel.cpp 0x44237c, 0x44239c, 0x4423a0).
const PM_EDGE_OFFSET: f32 = 0.001;
const PM_RATE_SCALE: f32 = 1.0;
const PM_MAXIMUM_TILT: f32 = 0.349_065_87;
/// `kActionFaceEnemyUnit` (ee/EEUnitAction.h).
const ACTION_FACE_ENEMY_UNIT: u32 = 2012;

/// The modes (`SpeedMode`, `PitchMode`, `YawMode`): hold, accelerate, brake.
pub const EE_UNITS_MODE_HOLD: i32 = 2;
pub const EE_UNITS_MODE_ACCELERATE: i32 = 3;
pub const EE_UNITS_MODE_BRAKE: i32 = 4;

/// `lrintf` as the x86-64 build computes it (round half to even; the "indefinite" value out of range).
fn lrintf(value: f32) -> i32 {
    let rounded = value.round_ties_even();
    if (-2_147_483_648.0..2_147_483_648.0).contains(&rounded) {
        rounded as i32
    } else {
        i32::MIN
    }
}

/// `UMAbs` (`v < 0 ? -v : v`: a negative zero stays).
fn um_abs(v: f32) -> f32 {
    if v < 0.0 {
        -v
    } else {
        v
    }
}

/// The unit's values a turn reads (`TurnTowardAngle`, `YawMode`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsTurnIn {
    /// `gPhysics->mTickSeconds`, `gPhysics->mTicksPerGameSecond`
    pub tick_seconds: f32,
    pub ticks_per_second: f32,
    /// `GetAngularAcceleration()`, `GetMaxAngularVelocity()`, `GetInverseAngularAcceleration()`
    pub accel: f32,
    pub max: f32,
    pub inverse_accel: f32,
    /// `mAngularVelocity.mYaw`
    pub rate: f32,
    /// the heading and the bearing to turn to
    pub current: f32,
    pub target: f32,
    /// `inFlag`: aim the class's facing offset (`GetFacingOffset()`), either side for a broadside ship
    /// (`mFiresBroadside`); both 0 without the flag
    pub use_facing: u8,
    pub broadside: u8,
    pub facing_offset: f32,
}

/// A turn's result: the angle left, the new rate, the new heading (`outA`, `outB`, `outC`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsTurn {
    pub delta: f32,
    pub rate: f32,
    pub heading: f32,
}

/// An angle into (-pi, pi] the way the C++ writes it: one turn added below -pi, else one taken above pi.
fn wrap_low_first(a: f32) -> f32 {
    if a < -PI {
        a + TWO_PI
    } else if a > PI {
        a - TWO_PI
    } else {
        a
    }
}

/// The facing's wrap (above pi first).
fn wrap_high_first(a: f32) -> f32 {
    if a > PI {
        a - TWO_PI
    } else if a < -PI {
        a + TWO_PI
    } else {
        a
    }
}

/// `YawMode`: 4 brake (turning away, or too close to stop), 2 hold (on target and still), else 3 accelerate.
pub fn yaw_mode(i: &EeUnitsTurnIn, rate: f32, angle: f32) -> i32 {
    if (!(rate >= 0.0) && !(angle <= 0.0)) || (!(rate <= 0.0) && !(angle >= 0.0)) {
        return EE_UNITS_MODE_BRAKE;
    }
    if angle == 0.0 {
        if rate == 0.0 {
            return EE_UNITS_MODE_HOLD;
        }
    } else {
        let mut angle = angle;
        if angle < 0.0 {
            angle = -angle;
        }
        let rate = um_abs(rate);
        let step = i.tick_seconds;
        let speed = i.accel * step + rate;
        angle -= i.tick_seconds * speed;
        if angle >= 0.0 {
            let ticks = i.inverse_accel * i.ticks_per_second * speed;
            let mut count = lrintf(ticks);
            if (count as f32) < ticks {
                count = count.wrapping_add(1);
            }
            let pairs = count.wrapping_mul(count.wrapping_sub(1)) / 2 + 1;
            let lhs = count.wrapping_sub(1) as f32 * step * speed - step * step * i.accel * pairs as f32;
            if lhs < angle {
                return EE_UNITS_MODE_ACCELERATE;
            }
        }
    }
    EE_UNITS_MODE_BRAKE
}

/// `TurnTowardAngle`: the yaw rate accelerated or braked toward the target heading, the move not past it.
pub fn turn_toward_angle(i: &EeUnitsTurnIn) -> EeUnitsTurn {
    let step = i.tick_seconds;
    let (accel, max, rate) = (i.accel, i.max, i.rate);
    let delta = if i.use_facing == 0 {
        wrap_low_first(i.target - i.current)
    } else {
        let mut facing = wrap_high_first(i.current + i.facing_offset);
        let mut delta = wrap_low_first(i.target - facing);
        if i.broadside != 0 {
            facing = wrap_high_first(facing + PI);
            let other = wrap_low_first(i.target - facing);
            if um_abs(other) < um_abs(delta) {
                delta = other;
            }
        }
        delta
    };
    let new = match yaw_mode(i, rate, delta) {
        EE_UNITS_MODE_HOLD => rate,
        EE_UNITS_MODE_ACCELERATE => {
            let mut new = accel * step;
            if delta < 0.0 {
                new = rate - new;
                if new < -max {
                    new = -max;
                }
            } else {
                new += rate;
                if max < new {
                    new = max;
                }
            }
            new
        }
        _ => {
            let change = accel * step;
            if rate < 0.0 {
                let mut new = change + rate;
                if delta < 0.0 && new > -change {
                    new = -change;
                } else if delta == 0.0 && new > -PM_EDGE_OFFSET {
                    new = 0.0;
                }
                new
            } else {
                let mut new = rate - change;
                if delta > 0.0 && new < change {
                    new = change;
                } else if delta == 0.0 && new < PM_EDGE_OFFSET {
                    new = 0.0;
                }
                new
            }
        }
    };
    let mut moved = new * step;
    if (delta < 0.0 && moved < delta) || (delta > 0.0 && delta < moved) {
        moved = delta;
    }
    EeUnitsTurn { delta, rate: new, heading: moved + i.current }
}

/// `SpinDown`: the yaw rate braked toward 0 (within the top rate), the heading moved by it.
pub fn spin_down(rate: f32, heading: f32, tick_seconds: f32, accel: f32, max: f32) -> EeUnitsTurn {
    let step = tick_seconds;
    let new = if rate < 0.0 {
        let mut new = accel * step + rate;
        if new > 0.0 {
            new = 0.0;
        }
        if new < -max {
            new = -max;
        }
        new
    } else if rate > 0.0 {
        let mut new = rate - accel * step;
        if new < 0.0 {
            new = 0.0;
        }
        if max < new {
            new = max;
        }
        new
    } else {
        0.0
    };
    EeUnitsTurn { delta: 0.0, rate: new, heading: new * step + heading }
}

/// `SteadyTurn`: a steady turn one way in physics order 0, the other else, at most 20 degrees a second unless the
/// command faces an enemy (`accel`, `max`: the stats', not the group's).
pub fn steady_turn(
    rate: f32,
    heading: f32,
    tick_seconds: f32,
    accel: f32,
    max: f32,
    order: i32,
    command: u32,
) -> EeUnitsTurn {
    let step = tick_seconds;
    let mut max = max;
    if max > PM_MAXIMUM_TILT && command != ACTION_FACE_ENEMY_UNIT {
        max = PM_MAXIMUM_TILT;
    }
    let new = if order == 0 {
        let mut new = rate - accel * step;
        if new < -max {
            new = -max;
        }
        new
    } else {
        let mut new = accel * step + rate;
        if new > max {
            new = max;
        }
        new
    };
    EeUnitsTurn { delta: 0.0, rate: new, heading: new * step + heading }
}

/// The unit's values a speed reads (`Accelerate`, `SpeedMode`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsSpeedIn {
    pub tick_seconds: f32,
    pub ticks_per_second: f32,
    /// `GetVelocity()`, `GetMaxVelocity()`, `GetAcceleration()`, `GetInverseAcceleration()`
    pub speed: f32,
    pub max_speed: f32,
    pub accel: f32,
    pub inverse_accel: f32,
    /// `mAngularVelocity.GetYaw()`
    pub yaw_rate: f32,
    /// the class's `mSlopeSpeedPenalty` (the helicopter's has none)
    pub slope_penalty: f32,
    /// `mWaypoints.size()`
    pub waypoints: i32,
    /// `GetPathLength` (read when the mode needs it: two waypoints or more), `GetTerrainPitch` of the location
    /// (read when the penalty is under 1)
    pub path_length: f32,
    pub terrain_pitch: f32,
}

/// `SpeedMode` (`helicopter`: `EEPMHelicopter::SpeedMode`): 3 accelerate, 2 hold, 4 brake (a sharp turn, within
/// braking distance of the path's end, or faster than the slope allows).
pub fn speed_mode(i: &EeUnitsSpeedIn, distance: f32, turn: f32, helicopter: bool) -> i32 {
    let step = i.tick_seconds;
    let brake = i.accel;
    let speed = i.speed;
    let (stop, slide) = if helicopter {
        let ticks = i.inverse_accel * i.ticks_per_second * speed;
        let mut stop = step * step * (ticks + 1.0) * ticks * 0.5 * brake;
        if stop < 0.0001 {
            stop = 0.0;
        }
        let mut slide = distance;
        slide *= 0.5 * i.yaw_rate * step;
        (stop, slide)
    } else {
        let ticks = (i.inverse_accel * i.ticks_per_second * speed) / PM_RATE_SCALE;
        let mut stop = step * step * (ticks + 1.0) * ticks * 0.5 * PM_RATE_SCALE * brake;
        if stop < 0.0001 {
            stop = 0.0;
        }
        let limit = if distance > 0.5 { 0.5 } else { distance };
        (stop, limit * 0.5 * i.yaw_rate * step)
    };
    let slide = if slide < 0.0 { -slide } else { slide };
    let mut result = EE_UNITS_MODE_ACCELERATE;
    if turn < -0.785_398_2 || turn > 0.785_398_2 {
        if slide <= speed * step {
            result = EE_UNITS_MODE_BRAKE;
        } else if slide <= (brake * step + speed) * step {
            result = EE_UNITS_MODE_HOLD;
        }
    }
    let remaining = if i.waypoints >= 2 { i.path_length } else { distance };
    if remaining <= stop {
        result = EE_UNITS_MODE_BRAKE;
    }
    if helicopter || !(i.slope_penalty < 1.0) {
        return result;
    }
    let mut slope = i.terrain_pitch * 1.909_859_3;
    if slope < 0.0 {
        slope = 0.0;
    } else if slope > 1.0 {
        slope = 1.0;
    }
    if (1.0 - slope * i.slope_penalty) * i.max_speed < i.speed {
        result = EE_UNITS_MODE_BRAKE;
    }
    result
}

/// `Accelerate` with its mode: the new speed (accelerated, not past the top speed nor the last waypoint; held; or
/// braked to a stop).
pub fn accelerate(i: &EeUnitsSpeedIn, distance: f32, mode: i32) -> f32 {
    let step = i.tick_seconds;
    match mode {
        EE_UNITS_MODE_HOLD => i.speed,
        EE_UNITS_MODE_ACCELERATE => {
            let mut r = step * i.accel + i.speed;
            if r > i.max_speed {
                r = i.max_speed;
            } else if i.waypoints == 1 && r > distance {
                r = distance;
            }
            r
        }
        EE_UNITS_MODE_BRAKE => {
            let mut r = i.speed - PM_RATE_SCALE * step * i.accel;
            if r < 0.005 {
                r = 0.0;
            }
            r
        }
        _ => 0.0,
    }
}

/// The unit's values a pitch reads (`ApproachPitch`, `PitchMode`): the base model's are the unit's (its active
/// group's), the naval model's the class's.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPitchIn {
    pub tick_seconds: f32,
    pub ticks_per_second: f32,
    pub accel: f32,
    pub max: f32,
    /// the base model's `GetInverseAngularAcceleration()` (the naval model divides by `accel`)
    pub inverse_accel: f32,
    /// `mAngularVelocity.mPitch`
    pub rate: f32,
    /// 1: `EEPMNaval`'s (and `EEPMSpaceship`'s) mode, 0: the base model's
    pub naval: u8,
}

/// `PitchMode` (the base model's or, with `naval`, `EEPMNaval`'s): 4 brake when stopping needs the whole angle left,
/// else 3.
pub fn pitch_mode(i: &EeUnitsPitchIn, rate: f32, angle: f32) -> i32 {
    let angle = if angle < 0.0 { -angle } else { angle };
    let rate = if rate < 0.0 { -rate } else { rate };
    if i.naval != 0 {
        let ticks = i.ticks_per_second * rate / i.accel;
        let step = i.tick_seconds;
        let mut stop = (ticks + 1.0) * ticks * 0.5;
        stop = stop * (step * step) * i.accel;
        return if angle <= stop { EE_UNITS_MODE_BRAKE } else { EE_UNITS_MODE_ACCELERATE };
    }
    let mut ticks = i.inverse_accel;
    ticks *= i.ticks_per_second * rate;
    let turn = i.tick_seconds;
    if (ticks + 1.0) * ticks * 0.5 * (turn * i.tick_seconds * i.accel) >= angle {
        EE_UNITS_MODE_BRAKE
    } else {
        EE_UNITS_MODE_ACCELERATE
    }
}

/// `ApproachPitch` with its mode (the model's `PitchMode` of the rate and the angle left): the pitch rate accelerated
/// or braked toward the target, the new pitch. The naval model's brake gives 0 for a NaN rate where the base keeps it.
pub fn approach_pitch(i: &EeUnitsPitchIn, current: f32, target: f32, mode: i32) -> EeUnitsTurn {
    let step = i.tick_seconds;
    let delta = target - current;
    let (accel, max) = (i.accel, i.max);
    let mut rate = i.rate;
    match mode {
        EE_UNITS_MODE_HOLD => {}
        EE_UNITS_MODE_ACCELERATE => {
            if delta < 0.0 {
                rate -= accel * step;
                if rate < -max {
                    rate = -max;
                }
            } else {
                rate = accel * step + rate;
                if max < rate {
                    rate = max;
                }
            }
        }
        EE_UNITS_MODE_BRAKE => {
            if delta < 0.0 {
                rate = PM_RATE_SCALE * accel * step + rate;
                let keep = if i.naval != 0 { rate <= 0.0 } else { !(rate > 0.0) };
                if !keep {
                    rate = 0.0;
                }
            } else {
                rate -= accel * step;
                let keep = if i.naval != 0 { rate >= 0.0 } else { !(rate < 0.0) };
                if !keep {
                    rate = 0.0;
                }
            }
        }
        _ => rate = 0.0,
    }
    EeUnitsTurn { delta, rate, heading: rate * step + current }
}

/// `TurnTowardAngle` over the C++'s values.
///
/// # Safety
/// `input` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_pm_turn(input: *const EeUnitsTurnIn) -> EeUnitsTurn {
    // SAFETY: the caller's contract
    turn_toward_angle(unsafe { &*input })
}

/// `YawMode` over the C++'s values (`rate`, `angle`: the call's).
///
/// # Safety
/// `input` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_pm_yaw_mode(input: *const EeUnitsTurnIn, rate: f32, angle: f32) -> i32 {
    // SAFETY: the caller's contract
    yaw_mode(unsafe { &*input }, rate, angle)
}

/// `SpinDown` (the heading is the caller's).
#[no_mangle]
pub extern "C" fn ee_units_pm_spin_down(
    rate: f32,
    heading: f32,
    tick_seconds: f32,
    accel: f32,
    max: f32,
) -> EeUnitsTurn {
    spin_down(rate, heading, tick_seconds, accel, max)
}

/// `SteadyTurn` (the heading is the caller's).
#[no_mangle]
pub extern "C" fn ee_units_pm_steady_turn(
    rate: f32,
    heading: f32,
    tick_seconds: f32,
    accel: f32,
    max: f32,
    order: i32,
    command: u32,
) -> EeUnitsTurn {
    steady_turn(rate, heading, tick_seconds, accel, max, order, command)
}

/// `SpeedMode` (`helicopter`: `EEPMHelicopter::SpeedMode`).
///
/// # Safety
/// `input` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_pm_speed_mode(
    input: *const EeUnitsSpeedIn,
    distance: f32,
    turn: f32,
    helicopter: bool,
) -> i32 {
    // SAFETY: the caller's contract
    speed_mode(unsafe { &*input }, distance, turn, helicopter)
}

/// `Accelerate`'s speed after the mode.
///
/// # Safety
/// `input` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_pm_accelerate(input: *const EeUnitsSpeedIn, distance: f32, mode: i32) -> f32 {
    // SAFETY: the caller's contract
    accelerate(unsafe { &*input }, distance, mode)
}

/// `PitchMode` (the base model's or the naval one's).
///
/// # Safety
/// `input` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_pm_pitch_mode(input: *const EeUnitsPitchIn, rate: f32, angle: f32) -> i32 {
    // SAFETY: the caller's contract
    pitch_mode(unsafe { &*input }, rate, angle)
}

/// `ApproachPitch` with its mode: the new rate (`rate`) and pitch (`heading`).
///
/// # Safety
/// `input` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_pm_approach_pitch(
    input: *const EeUnitsPitchIn,
    current: f32,
    target: f32,
    mode: i32,
) -> EeUnitsTurn {
    // SAFETY: the caller's contract
    approach_pitch(unsafe { &*input }, current, target, mode)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn turn_in(rate: f32, current: f32, target: f32) -> EeUnitsTurnIn {
        EeUnitsTurnIn {
            tick_seconds: 0.125,
            ticks_per_second: 8.0,
            accel: 2.0,
            max: 1.5,
            inverse_accel: 0.5,
            rate,
            current,
            target,
            ..Default::default()
        }
    }

    #[test]
    fn the_yaw_mode_brakes_holds_and_accelerates() {
        let i = turn_in(0.0, 0.0, 0.0);
        // turning away from the angle left
        assert_eq!(yaw_mode(&i, 0.5, -0.2), EE_UNITS_MODE_BRAKE);
        assert_eq!(yaw_mode(&i, -0.5, 0.2), EE_UNITS_MODE_BRAKE);
        // on target and still
        assert_eq!(yaw_mode(&i, 0.0, 0.0), EE_UNITS_MODE_HOLD);
        // on target but turning: brake
        assert_eq!(yaw_mode(&i, 0.3, 0.0), EE_UNITS_MODE_BRAKE);
        // far: accelerate; less than one tick's turn left: brake
        assert_eq!(yaw_mode(&i, 0.0, 2.0), EE_UNITS_MODE_ACCELERATE);
        assert_eq!(yaw_mode(&i, 0.0, 0.01), EE_UNITS_MODE_BRAKE);
        // the C++ statements for a middle case
        let speed = 2.0f32 * 0.125 + 0.5;
        let left = 1.0f32 - 0.125 * speed;
        let ticks = 0.5f32 * 8.0 * speed;
        let mut count = lrintf(ticks);
        if (count as f32) < ticks {
            count += 1;
        }
        let lhs = (count - 1) as f32 * 0.125 * speed - 0.125 * 0.125 * 2.0 * ((count * (count - 1)) / 2 + 1) as f32;
        let want = if lhs < left { EE_UNITS_MODE_ACCELERATE } else { EE_UNITS_MODE_BRAKE };
        assert_eq!(yaw_mode(&i, 0.5, 1.0), want);
    }

    #[test]
    fn a_turn_accelerates_toward_the_target_and_stops_on_it() {
        // from rest toward +1: the rate is accel * step, the heading moves rate * step
        let t = turn_toward_angle(&turn_in(0.0, 0.0, 1.0));
        assert_eq!(t.delta, 1.0);
        assert_eq!(t.rate, 0.25);
        assert_eq!(t.heading.to_bits(), (0.25f32 * 0.125 + 0.0).to_bits());
        // the rate is capped at the maximum
        let t = turn_toward_angle(&turn_in(1.45, 0.0, 3.0));
        assert_eq!(t.rate, 1.5);
        // the move never passes the target
        let t = turn_toward_angle(&turn_in(1.4, 0.0, 0.01));
        assert!(t.heading <= 0.01);
        // across pi the short way
        let t = turn_toward_angle(&turn_in(0.0, 3.0, -3.0));
        assert_eq!(t.delta.to_bits(), (-3.0f32 - 3.0 + TWO_PI).to_bits());
        // the facing offset, and a broadside ship's other side
        let mut i = turn_in(0.0, 0.0, 2.0);
        i.use_facing = 1;
        i.facing_offset = 0.5;
        assert_eq!(turn_toward_angle(&i).delta, 1.5);
        i.broadside = 1;
        let other = wrap_low_first(2.0 - wrap_high_first(0.5 + PI));
        assert_eq!(turn_toward_angle(&i).delta, if um_abs(other) < 1.5 { other } else { 1.5 });
    }

    #[test]
    fn the_brake_of_a_turn_keeps_the_edge_offset() {
        // braking on target: a small rate goes to 0
        let t = turn_toward_angle(&turn_in(0.0005, 0.0, 0.0));
        assert_eq!(t.rate, 0.0);
        // braking against the angle left: the rate goes down by accel * step
        let t = turn_toward_angle(&turn_in(1.0, 0.0, -0.5));
        assert_eq!(t.rate, 1.0 - 0.25);
    }

    #[test]
    fn the_spin_down_and_the_steady_turn() {
        let t = spin_down(-1.0, 0.5, 0.125, 2.0, 1.5);
        assert_eq!(t.rate, -0.75);
        assert_eq!(t.heading.to_bits(), (-0.75f32 * 0.125 + 0.5).to_bits());
        assert_eq!(spin_down(0.1, 0.0, 0.125, 2.0, 1.5).rate, 0.0);
        assert_eq!(spin_down(2.0, 0.0, 0.125, 0.0, 1.5).rate, 1.5);
        assert_eq!(spin_down(0.0, 0.25, 0.125, 2.0, 1.5), EeUnitsTurn { delta: 0.0, rate: 0.0, heading: 0.25 });
        // the steady turn: at most 20 degrees a second unless the command faces an enemy
        assert_eq!(steady_turn(0.0, 0.0, 0.125, 8.0, 2.0, 1, 0).rate, PM_MAXIMUM_TILT);
        assert_eq!(steady_turn(0.0, 0.0, 0.125, 8.0, 2.0, 1, ACTION_FACE_ENEMY_UNIT).rate, 1.0);
        assert_eq!(steady_turn(0.0, 0.0, 0.125, 8.0, 2.0, 0, 0).rate, -PM_MAXIMUM_TILT);
    }

    fn speed_in() -> EeUnitsSpeedIn {
        EeUnitsSpeedIn {
            tick_seconds: 0.125,
            ticks_per_second: 8.0,
            speed: 1.0,
            max_speed: 2.0,
            accel: 1.0,
            inverse_accel: 1.0,
            yaw_rate: 0.0,
            slope_penalty: 1.0,
            waypoints: 1,
            path_length: 0.0,
            terrain_pitch: 0.0,
        }
    }

    #[test]
    fn the_speed_mode_and_the_speed() {
        let i = speed_in();
        // far from the end: accelerate; within the stop distance: brake
        assert_eq!(speed_mode(&i, 10.0, 0.0, false), EE_UNITS_MODE_ACCELERATE);
        let ticks = 1.0f32 * 8.0 * 1.0;
        let stop = 0.125f32 * 0.125 * (ticks + 1.0) * ticks * 0.5 * 1.0;
        assert_eq!(speed_mode(&i, stop, 0.0, false), EE_UNITS_MODE_BRAKE);
        // two waypoints or more: the path's length decides
        let far = EeUnitsSpeedIn { waypoints: 3, path_length: 10.0, ..i };
        assert_eq!(speed_mode(&far, stop, 0.0, false), EE_UNITS_MODE_ACCELERATE);
        // a sharp turn with no yaw rate: the slide 0 is within a tick's move: brake
        assert_eq!(speed_mode(&i, 10.0, 1.0, false), EE_UNITS_MODE_BRAKE);
        // a slope it cannot climb at this speed; the helicopter has no slope
        let steep = EeUnitsSpeedIn { slope_penalty: 0.5, terrain_pitch: 1.0, speed: 1.5, ..i };
        assert_eq!(speed_mode(&steep, 10.0, 0.0, false), EE_UNITS_MODE_BRAKE);
        assert_eq!(speed_mode(&steep, 10.0, 0.0, true), EE_UNITS_MODE_ACCELERATE);
        // the speed after the mode
        assert_eq!(accelerate(&i, 10.0, EE_UNITS_MODE_ACCELERATE), 0.125 * 1.0 + 1.0);
        assert_eq!(accelerate(&i, 1.05, EE_UNITS_MODE_ACCELERATE), 1.05);
        assert_eq!(accelerate(&EeUnitsSpeedIn { speed: 1.95, ..i }, 10.0, EE_UNITS_MODE_ACCELERATE), 2.0);
        assert_eq!(accelerate(&i, 10.0, EE_UNITS_MODE_HOLD), 1.0);
        assert_eq!(accelerate(&i, 10.0, EE_UNITS_MODE_BRAKE), 1.0 - 0.125);
        assert_eq!(accelerate(&EeUnitsSpeedIn { speed: 0.12, ..i }, 10.0, EE_UNITS_MODE_BRAKE), 0.0);
        assert_eq!(accelerate(&i, 10.0, 7), 0.0);
    }

    #[test]
    fn the_pitch_modes_and_the_approach() {
        let base = EeUnitsPitchIn {
            tick_seconds: 0.125,
            ticks_per_second: 8.0,
            accel: 2.0,
            max: 1.0,
            inverse_accel: 0.5,
            rate: 0.0,
            naval: 0,
        };
        assert_eq!(pitch_mode(&base, 0.0, 1.0), EE_UNITS_MODE_ACCELERATE);
        assert_eq!(pitch_mode(&base, 0.0, 0.0), EE_UNITS_MODE_BRAKE);
        let naval = EeUnitsPitchIn { naval: 1, ..base };
        let ticks = 8.0f32 * 1.0 / 2.0;
        let stop = (ticks + 1.0) * ticks * 0.5 * (0.125 * 0.125) * 2.0;
        assert_eq!(pitch_mode(&naval, -1.0, stop), EE_UNITS_MODE_BRAKE);
        assert_eq!(pitch_mode(&naval, -1.0, stop * 1.01), EE_UNITS_MODE_ACCELERATE);
        let p = approach_pitch(&base, 0.0, 1.0, EE_UNITS_MODE_ACCELERATE);
        assert_eq!((p.rate, p.heading), (0.25, 0.25 * 0.125));
        let p = approach_pitch(&EeUnitsPitchIn { rate: 0.2, ..base }, 0.0, 1.0, EE_UNITS_MODE_BRAKE);
        assert_eq!(p.rate, 0.0);
        let p = approach_pitch(&EeUnitsPitchIn { rate: 0.9, ..base }, 0.0, -1.0, EE_UNITS_MODE_ACCELERATE);
        assert_eq!(p.rate, 0.9 - 0.25);
        // a NaN rate braking: the base keeps it, the naval model zeroes it
        let p = approach_pitch(&EeUnitsPitchIn { rate: f32::NAN, ..base }, 0.0, -1.0, EE_UNITS_MODE_BRAKE);
        assert!(p.rate.is_nan());
        let p = approach_pitch(&EeUnitsPitchIn { rate: f32::NAN, ..naval }, 0.0, -1.0, EE_UNITS_MODE_BRAKE);
        assert_eq!(p.rate, 0.0);
        assert_eq!(approach_pitch(&base, 0.5, 1.0, EE_UNITS_MODE_HOLD).heading, 0.5);
    }
}
