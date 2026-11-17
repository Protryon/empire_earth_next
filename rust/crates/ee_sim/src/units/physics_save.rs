//! `EEPhysics`' saved state as a Rust record with its Load and Save (RUST_PLAN R5b.4, the twenty-eighth round): the
//! block `mCurrentTick` (+0x080 in the original) .. `mResourceTimerPending` (+0x0e8), the clocks, tick counters,
//! timing averages and the civilization timer's flags, is [`EeUnitsPhysicsSaved`], which the C++ class derives from
//! (`class EEPhysics : public RTSPhysics, public EeUnitsPhysicsSaved`, ee/EEPhysics.h): the members keep their names,
//! C++ types and order. `EEPhysics::Load` (two era bodies, the 0.206 game-over flag and the 0.250 resource timer) and
//! `Save` are [`load`] and [`save`] over player_save.rs's host and file primitives (`EESavePrimitive`) and two C++
//! parts: `RTSPhysics::Load`/`Save` (the base's seed, speed and counters).

#![allow(non_snake_case)]

use crate::units::player_save::{EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, PlayerSaveHost};
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// `EEPhysics`' plain members (ee/EEPhysics.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPhysicsSaved {
    pub mCurrentTick: c_ulong,
    pub mF84: f32,
    pub mTicksPerGameSecond: f32,
    pub mTickSeconds: f32,
    pub mTimeOfDayStep: f32,
    pub mGameClock: f32,
    pub mTickTime: f32,
    pub mAreaEffectsClock: f32,
    pub mShiftModeClock: f32,
    pub mTickTimeMs: c_long,
    pub mTickStartTime: c_long,
    pub mPreviousTickStartTime: c_long,
    pub mTickRandom: c_long,
    pub mNumTicksDone: c_ulong,
    pub mTicksToProcess: c_ulong,
    pub mTickGapSum: c_ulong,
    pub mTickGapCount: c_ulong,
    pub mTickWorkSum: c_ulong,
    pub mTickWorkCount: c_ulong,
    /// untouched by the constructor, its init and Load/Save
    pub mAutoSaveClock: c_long,
    pub mGameTime: c_long,
    pub mLD4: c_long,
    pub mAveragePhysicsUpdateTime: c_long,
    pub mAverageTimeBetweenPhysicsTicks: c_long,
    pub mTimeOfDayRuns: bool,
    pub mGameStarted: bool,
    pub mBE2: bool,
    pub mCivTimerRuns: bool,
    pub mCivTimerFirstWarning: bool,
    pub mCivTimerSecondWarning: bool,
    pub mGameOver: bool,
    pub mBE7: bool,
    pub mResourceTimerPending: bool,
}

/// `RTSPhysics::Load` (the base's part, first) and `RTSPhysics::Save`.
pub const EE_UNITS_PHYSICS_SAVE_RTS_LOAD: i32 = 101;
pub const EE_UNITS_PHYSICS_SAVE_RTS_SAVE: i32 = 102;

const OK: i32 = 0;

/// `(long)` of a float as the x86 conversion does it (truncation; out of range or NaN: the integer indefinite).
fn truncate_long(value: f32) -> c_long {
    if value.is_nan() || value >= -(c_long::MIN as f32) || value < c_long::MIN as f32 {
        c_long::MIN
    } else {
        value as c_long
    }
}

/// The record behind its raw pointer: one field read or written at a time (no Rust reference to the C++ object
/// across a host call).
struct Rec(*mut EeUnitsPhysicsSaved);

macro_rules! get {
    ($r:expr, $f:ident) => {
        // SAFETY: the record is live for the call (the caller's contract)
        unsafe { std::ptr::addr_of!((*$r.0).$f).read() }
    };
}
macro_rules! set {
    ($r:expr, $f:ident, $v:expr) => {{
        let v = $v;
        // SAFETY: as get!
        unsafe { std::ptr::addr_of_mut!((*$r.0).$f).write(v) }
    }};
}
/// A primitive read into a field (the field's value in, the read value out).
macro_rules! read {
    ($h:expr, $r:expr, $f:ident, $read:ident) => {{
        let mut v = get!($r, $f);
        let result = $h.$read(&mut v);
        set!($r, $f, v);
        result
    }};
}

/// `EEPhysics::Load`: the base's part, then the members in order while every read succeeds; up to 0.206 (minor
/// 0xce of major 0) the game-over flag is not saved and is cleared, later it is read, and from minor 0xfa the resource
/// timer follows. After the tick time its milliseconds are rounded (`UMFloatToLong`, as `UpdateTiming` does), after
/// the game clock the game time is its truncation; the two averages go through one carried `unsigned long`.
fn load(h: &mut impl PlayerSaveHost, r: &Rec, major: u32, minor: u32) -> i32 {
    let old = major == 0 && minor <= 0xce;
    let mut value: c_ulong = 0;
    let mut result = h.op(EE_UNITS_PHYSICS_SAVE_RTS_LOAD);
    if result != OK {
        return result;
    }
    // the chain: each read only after the one before succeeded
    macro_rules! next {
        ($e:expr) => {
            if result == OK {
                result = $e;
            }
        };
    }
    next!(read!(h, r, mCurrentTick, read_ulong));
    next!(read!(h, r, mF84, read_float));
    next!(read!(h, r, mTicksPerGameSecond, read_float));
    next!(read!(h, r, mTickSeconds, read_float));
    next!(read!(h, r, mTimeOfDayStep, read_float));
    if result == OK {
        result = read!(h, r, mTickTime, read_float);
        set!(r, mTickTimeMs, ee_base::math::um::lrintf(get!(r, mTickTime)) as c_long);
    }
    next!(read!(h, r, mTickStartTime, read_long));
    next!(read!(h, r, mPreviousTickStartTime, read_long));
    next!(read!(h, r, mNumTicksDone, read_ulong));
    next!(read!(h, r, mTicksToProcess, read_ulong));
    next!(read!(h, r, mTickGapSum, read_ulong));
    next!(read!(h, r, mTickGapCount, read_ulong));
    next!(read!(h, r, mTickWorkSum, read_ulong));
    next!(read!(h, r, mTickWorkCount, read_ulong));
    if result == OK {
        result = read!(h, r, mGameClock, read_float);
        set!(r, mGameTime, truncate_long(get!(r, mGameClock)));
    }
    if result == OK {
        result = h.read_ulong(&mut value);
        set!(r, mAveragePhysicsUpdateTime, value as c_long);
    }
    if result == OK {
        result = h.read_ulong(&mut value);
        set!(r, mAverageTimeBetweenPhysicsTicks, value as c_long);
    }
    next!(read!(h, r, mTimeOfDayRuns, read_bool));
    next!(read!(h, r, mAreaEffectsClock, read_float));
    next!(read!(h, r, mCivTimerRuns, read_bool));
    next!(read!(h, r, mCivTimerFirstWarning, read_bool));
    next!(read!(h, r, mCivTimerSecondWarning, read_bool));
    if old {
        set!(r, mGameOver, false);
    } else {
        next!(read!(h, r, mGameOver, read_bool));
        if minor >= 0xfa {
            next!(read!(h, r, mResourceTimerPending, read_bool));
        }
    }
    result
}

/// `EEPhysics::Save`: the base's part, then the members in the load's order while every write succeeds (the two
/// averages as `unsigned long`s, the game-over flag and the resource timer always).
fn save(h: &mut impl PlayerSaveHost, r: &Rec) -> i32 {
    let mut result = h.op(EE_UNITS_PHYSICS_SAVE_RTS_SAVE);
    macro_rules! next {
        ($e:expr) => {
            if result == OK {
                result = $e;
            }
        };
    }
    next!(h.write_ulong(get!(r, mCurrentTick)));
    next!(h.write_float(get!(r, mF84)));
    next!(h.write_float(get!(r, mTicksPerGameSecond)));
    next!(h.write_float(get!(r, mTickSeconds)));
    next!(h.write_float(get!(r, mTimeOfDayStep)));
    next!(h.write_float(get!(r, mTickTime)));
    next!(h.write_long(get!(r, mTickStartTime)));
    next!(h.write_long(get!(r, mPreviousTickStartTime)));
    next!(h.write_ulong(get!(r, mNumTicksDone)));
    next!(h.write_ulong(get!(r, mTicksToProcess)));
    next!(h.write_ulong(get!(r, mTickGapSum)));
    next!(h.write_ulong(get!(r, mTickGapCount)));
    next!(h.write_ulong(get!(r, mTickWorkSum)));
    next!(h.write_ulong(get!(r, mTickWorkCount)));
    next!(h.write_float(get!(r, mGameClock)));
    next!(h.write_ulong(get!(r, mAveragePhysicsUpdateTime) as c_ulong));
    next!(h.write_ulong(get!(r, mAverageTimeBetweenPhysicsTicks) as c_ulong));
    next!(h.write_bool(get!(r, mTimeOfDayRuns)));
    next!(h.write_float(get!(r, mAreaEffectsClock)));
    next!(h.write_bool(get!(r, mCivTimerRuns)));
    next!(h.write_bool(get!(r, mCivTimerFirstWarning)));
    next!(h.write_bool(get!(r, mCivTimerSecondWarning)));
    next!(h.write_bool(get!(r, mGameOver)));
    next!(h.write_bool(get!(r, mResourceTimerPending)));
    result
}

struct FfiHost {
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
}

impl PlayerSaveHost for FfiHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the C++ callback with its own context; io lives for the call
        unsafe { (self.f)(self.ctx, op, index, io) }
    }
}

/// `EEPhysics::Load` over the physics' record `rec`: the file's primitives and the base's part through `f`.
///
/// # Safety
/// `rec` points to the physics' live record; `f` is valid with `ctx` for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_physics_load(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    rec: *mut EeUnitsPhysicsSaved,
    major: u32,
    minor: u32,
) -> i32 {
    load(&mut FfiHost { ctx, f }, &Rec(rec), major, minor)
}

/// `EEPhysics::Save`: as [`ee_units_physics_load`].
///
/// # Safety
/// As [`ee_units_physics_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_physics_save(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    rec: *mut EeUnitsPhysicsSaved,
) -> i32 {
    save(&mut FfiHost { ctx, f }, &Rec(rec))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::units::player_save::{EeUnitsPlayerSaveIo, PlayerSaveHost, EE_UNITS_PLAYER_SAVE_WRITE_ULONG};

    /// A file of primitive values (each op reads the next or appends), failing at the `fail_at`th primitive.
    #[derive(Default)]
    struct File {
        values: Vec<(i32, EeUnitsPlayerSaveIo)>,
        at: usize,
        ops: Vec<i32>,
        fail_at: Option<usize>,
        primitives: usize,
    }

    impl PlayerSaveHost for File {
        fn call(&mut self, op: i32, _index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
            self.ops.push(op);
            if op > 100 {
                return 0;
            }
            self.primitives += 1;
            if self.fail_at == Some(self.primitives) {
                return 7;
            }
            if op >= EE_UNITS_PLAYER_SAVE_WRITE_ULONG {
                // a write: its read twin and the value
                self.values.push((op - 4, *io));
            } else {
                let (kind, v) = self.values[self.at];
                assert_eq!(kind, op, "read {} as op {op}", self.at);
                self.at += 1;
                *io = v;
            }
            0
        }
    }

    fn sample() -> EeUnitsPhysicsSaved {
        EeUnitsPhysicsSaved {
            mCurrentTick: 1234,
            mF84: 0.25,
            mTicksPerGameSecond: 5.0,
            mTickSeconds: 0.2,
            mTimeOfDayStep: 0.01,
            mGameClock: 1999.75,
            mTickTime: 200.5,
            mAreaEffectsClock: 3.5,
            mTickStartTime: -5,
            mPreviousTickStartTime: 77,
            mNumTicksDone: 9,
            mTicksToProcess: 2,
            mTickGapSum: 1,
            mTickGapCount: 2,
            mTickWorkSum: 3,
            mTickWorkCount: 4,
            mAveragePhysicsUpdateTime: -1,
            mAverageTimeBetweenPhysicsTicks: 40,
            mTimeOfDayRuns: true,
            mCivTimerFirstWarning: true,
            mGameOver: true,
            mResourceTimerPending: true,
            ..Default::default()
        }
    }

    #[test]
    fn save_then_load_gives_the_saved_members() {
        let mut saved = sample();
        let mut f = File::default();
        assert_eq!(save(&mut f, &Rec(&mut saved)), 0);
        assert_eq!(f.ops[0], EE_UNITS_PHYSICS_SAVE_RTS_SAVE);
        assert_eq!(f.values.len(), 24);
        let mut back = EeUnitsPhysicsSaved { mShiftModeClock: 8.0, mAutoSaveClock: 3, ..Default::default() };
        assert_eq!(load(&mut f, &Rec(&mut back), 0, 0x110), 0);
        assert_eq!(f.at, 24);
        let mut want = saved;
        // the derived members; the unsaved ones keep their values
        want.mTickTimeMs = 200; // lrintf(200.5): to even
        want.mGameTime = 1999;
        want.mShiftModeClock = 8.0;
        want.mAutoSaveClock = 3;
        assert_eq!(back, want);
    }

    #[test]
    fn the_eras_and_failures() {
        let mut saved = sample();
        let mut f = File::default();
        save(&mut f, &Rec(&mut saved));
        // before 0.250 (minor 0xfa) no resource timer: one value left
        let mut back = EeUnitsPhysicsSaved::default();
        assert_eq!(load(&mut f, &Rec(&mut back), 0, 0xf9), 0);
        assert_eq!(f.at, 23);
        assert!(back.mGameOver && !back.mResourceTimerPending);
        // up to 0.206 the game-over flag is not read and is cleared (here the next value is read as a bool anyway)
        let mut f2 = File { values: f.values[..22].to_vec(), ..Default::default() };
        let mut back = EeUnitsPhysicsSaved { mGameOver: true, ..Default::default() };
        assert_eq!(load(&mut f2, &Rec(&mut back), 0, 0xce), 0);
        assert_eq!(f2.at, 22);
        assert!(!back.mGameOver);
        // a failed read stops the chain and is the result; the game-over flag still cleared in the old era
        let mut f3 = File { values: f.values.clone(), fail_at: Some(3), ..Default::default() };
        let mut back = EeUnitsPhysicsSaved { mGameOver: true, mTickSeconds: 9.0, ..Default::default() };
        assert_eq!(load(&mut f3, &Rec(&mut back), 0, 0xce), 7);
        assert_eq!((f3.primitives, back.mF84, back.mTicksPerGameSecond, back.mTickSeconds), (3, 0.25, 0.0, 9.0));
        assert!(!back.mGameOver);
        // a failed base load returns before anything
        struct Fails;
        impl PlayerSaveHost for Fails {
            fn call(&mut self, _: i32, _: u32, _: &mut EeUnitsPlayerSaveIo) -> i32 {
                5
            }
        }
        let mut back = EeUnitsPhysicsSaved { mGameOver: true, ..Default::default() };
        assert_eq!(load(&mut Fails, &Rec(&mut back), 0, 0xce), 5);
        assert!(back.mGameOver);
        // a failed write: the rest are not written
        let mut f4 = File { fail_at: Some(2), ..Default::default() };
        assert_eq!(save(&mut f4, &Rec(&mut saved)), 7);
        assert_eq!(f4.primitives, 2);
    }

    #[test]
    fn the_game_time_truncates_as_x86() {
        assert_eq!(truncate_long(1999.75), 1999);
        assert_eq!(truncate_long(-2.5), -2);
        assert_eq!(truncate_long(f32::NAN), c_long::MIN);
        assert_eq!(truncate_long(1e20), c_long::MIN);
    }
}
