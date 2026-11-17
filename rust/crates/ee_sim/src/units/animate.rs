//! `EEUnitClass::Animate` (0x6385a7)'s decisions: the animation clock of one unit for one tick. The C++ gathers the
//! unit's and its model's numbers for the current state ([`EeUnitsAnimateIn`]), this decides the frame, the time,
//! the pause, the interpolation fraction, the next frame and the sound slot ([`EeUnitsAnimateOut`]), and the C++
//! applies them in the original's order (pause, frame, time, the sound's stop, the fraction, the next frame) and keeps
//! the world's parts: the
//! idle animation's random pick with the horse's breath, and the sound's play.
//!
//! Determinism: the original's `unsigned long` arithmetic wrapping, its `float` operations in order, `UMFloatToLong`
//! as the UCRT's `lrintf` (ee_ge), no random draw here (the C++ draws for [`EeUnitsAnimateOut::idle_pick`]).

use ee_base::math::um::lrintf;

/// The unit's and its model's numbers for its current animation state (`state`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsAnimateIn {
    /// `mIsAnimationPaused`
    pub paused: bool,
    /// the model's `mLoops[state]`
    pub loops: bool,
    /// `mIsSpeedScaled[state]`
    pub speed_scaled: bool,
    /// `mSharesFrames[state]`
    pub shares_frames: bool,
    /// the unit's stats' `mCanFireOverWalls`
    pub can_fire_over_walls: bool,
    /// `IsFoundation()`
    pub foundation: bool,
    /// `GetAnimation()`
    pub state: u32,
    /// `gPhysics->mTickTimeMs`
    pub tick_ms: u32,
    /// `mAnimationTime`
    pub time: u32,
    /// `mPhysicalModel.GetFrame()`
    pub frame: u32,
    pub hold_time: u32,
    pub start_frame: u32,
    pub last_frame: u32,
    pub frame_count: u32,
    pub frame_time: u32,
    pub sound1_frame: u32,
    pub sound2_frame: u32,
    /// `GetVelocity()`, the class's `GetMaxVelocity()`
    pub velocity: f32,
    pub max_velocity: f32,
    /// `mAngularVelocity.GetYaw()`, the class's `mMaxAngularVelocity`
    pub yaw_velocity: f32,
    pub max_angular_velocity: f32,
}

/// What the tick does: each `set_*` flag with its value, applied in this order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsAnimateOut {
    /// `SetIsAnimationPaused(paused)`
    pub set_paused: bool,
    pub paused: bool,
    /// `SetAnimationFrame(frame)`
    pub set_frame: bool,
    pub frame: u32,
    /// `SetAnimationTime(time)`
    pub set_time: bool,
    pub time: u32,
    /// the unit's sound stopped and released
    pub stop_sound: bool,
    /// the paused loop's idle pick: the C++ draws `GetRandom()` and sets animation 7 or 8 (with the horse's breath)
    pub idle_pick: bool,
    /// the sound to play: 0 none, 1 the state's first, 2 its second
    pub sound: u32,
    /// a running animation only: `mAnimationFraction = fraction` when it differs (the C++ compares, after the
    /// setters), `SetNextFrame(next_frame)`
    pub fraction: f32,
    pub next_frame: u32,
    /// the frames stepped (the return value)
    pub steps: u32,
}

fn sound_at(i: &EeUnitsAnimateIn, frame: u32) -> u32 {
    if frame == i.sound1_frame {
        1
    } else if frame == i.sound2_frame {
        2
    } else {
        0
    }
}

/// `Animate`'s decisions for one tick.
// the original's forms: `!(x >= 0.5)` takes a NaN to 0.5; the zero frame time is its own branch
#[allow(clippy::neg_cmp_op_on_partial_ord, clippy::manual_checked_ops)]
pub fn animate(i: &EeUnitsAnimateIn) -> EeUnitsAnimateOut {
    let mut o = EeUnitsAnimateOut::default();
    if i.paused {
        let time = i.tick_ms.wrapping_add(i.time);
        if time < i.hold_time {
            o.set_time = true;
            o.time = time;
        } else if i.loops {
            if i.state != 7 && (i.can_fire_over_walls || i.state != 8) {
                o.set_paused = true;
                o.paused = false;
                o.set_frame = true;
                o.frame = i.start_frame;
                o.set_time = true;
                o.time = 0;
                o.sound = sound_at(i, i.start_frame);
            } else {
                o.idle_pick = true;
            }
        } else {
            o.stop_sound = true;
        }
        return o;
    }
    let count = i.frame_count;
    let time = i.tick_ms.wrapping_add(i.time);
    let start = i.start_frame;
    let last = i.last_frame;
    let old_frame = i.frame;
    let mut frame = old_frame;
    let mut steps: u32 = 0;
    let frame_time: u32 = if !i.speed_scaled {
        i.frame_time
    } else {
        let mut speed = i.velocity / i.max_velocity;
        if !(speed >= 0.5) {
            speed = 0.5;
        }
        let turn = i.yaw_velocity;
        let mut turn = (if turn >= 0.0 { turn } else { -turn }) / i.max_angular_velocity;
        if !(speed >= 0.5) {
            speed = 0.5;
        }
        if !(turn >= 0.5) {
            turn = 0.5;
        }
        let scaled = if speed > turn { i.frame_time as f32 / speed } else { i.frame_time as f32 / turn };
        lrintf(scaled) as u32
    };
    let remainder: u32;
    if time < frame_time {
        o.set_time = true;
        o.time = time;
        remainder = time;
    } else {
        if frame_time > 0 {
            steps = time / frame_time;
            remainder = time.wrapping_sub(frame_time.wrapping_mul(steps));
        } else {
            remainder = 0;
            steps = 1;
        }
        if i.loops {
            // a zero frame count divides by zero, as the original's `div` faulted
            frame = (old_frame.wrapping_sub(start).wrapping_add(steps) % count).wrapping_add(start);
            if frame > old_frame || i.hold_time == 0 {
                o.set_frame = true;
                o.frame = frame;
                o.set_time = true;
                o.time = remainder;
                o.sound = sound_at(i, frame);
            } else {
                o.set_paused = true;
                o.paused = true;
                o.set_frame = true;
                o.frame = start;
                o.set_time = true;
                o.time = 0;
                o.stop_sound = true;
            }
        } else if !i.foundation {
            let offset = old_frame.wrapping_sub(start).wrapping_add(steps);
            frame = start.wrapping_add(offset);
            if offset < count {
                o.set_frame = true;
                o.frame = frame;
                o.set_time = true;
                o.time = remainder;
                o.sound = sound_at(i, frame);
            } else {
                o.set_paused = true;
                o.paused = true;
                o.set_frame = true;
                o.frame = last;
                o.set_time = true;
                o.time = 0;
                frame = last;
                o.stop_sound = true;
            }
        }
    }
    let mut new_fraction = 0.0f32;
    if frame_time != 0 {
        new_fraction = remainder as f32 / (frame_time as i32) as f32;
    }
    let mut next = if start == last {
        start
    } else if frame == last {
        if i.loops {
            start
        } else {
            frame
        }
    } else {
        frame.wrapping_add(1)
    };
    if count > 4 && i.shares_frames {
        let even = (!count & 1) != 0;
        if frame == last {
            next = if i.loops { start } else { frame };
        } else {
            next = frame.wrapping_add(1);
            if next != last {
                next = frame.wrapping_add(2);
            } else if even && i.loops {
                next = start;
            }
        }
        if frame != last || even {
            new_fraction *= 0.5;
        }
        if (frame.wrapping_sub(start) & 1) == 1 {
            new_fraction += 0.5;
        }
    }
    o.fraction = new_fraction;
    o.next_frame = next;
    o.steps = steps;
    o
}

/// `EEUnitClass::Animate`'s decisions (`out` filled).
///
/// # Safety
/// `input` and `out` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_units_animate(input: *const EeUnitsAnimateIn, out: *mut EeUnitsAnimateOut) {
    // SAFETY: the caller's contract
    unsafe { *out = animate(&*input) };
}

#[cfg(test)]
#[path = "animate_tests.rs"]
mod tests;
