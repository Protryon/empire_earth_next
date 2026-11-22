//! The sections of `EEComplexUnit::Load` that were C++ loops (the units' lane, round 7; were
//! EEComplexDoppelganger.cpp's `EEComplexUnitLoadSection`): the build queue, the citizen queue, the cargo, the held
//! units, the unit variables and the port's live state. The reads go through the saves' host primitives with the
//! overload the original called; each container statement is one `EE_UNITS_UNIT_LOAD_SET` case of the host
//! (`EEUnitSaveOperation`), asked in the original's order. The waypoints (the Paths lane's list) and the goal
//! queues (each queue's own `Load`) stay host sections.
//!
//! The live state's values are kept in a Rust allocation that the host hands to `EEAfterLoad` (a shared pointer
//! whose deleter is [`ee_units_unit_live_free`]); at the loaded game's first tick [`ee_units_unit_live_apply`] puts
//! them back through `ee_units_dop_host` (EEComplexDoppelganger.cpp) while the unit is still the world's of its id.

#![allow(clippy::unnecessary_cast)]

use crate::units::dop_repair::DopHost;
use crate::units::unit_load::*;
use crate::units::unit_save::{EeUnitsUnitSaveIo, SaveHost};

/// a value of the unit being loaded (`index`: `EE_UNITS_UNIT_LOAD_GET_*`; io.i in an argument, the answer in io)
pub const EE_UNITS_UNIT_LOAD_GET: i32 = 114;
/// `mTurretAngles != NULL ? mNumTurretAngles : 0` (io.i)
pub const EE_UNITS_UNIT_LOAD_GET_TURRET_COUNT: u32 = 0;
/// `mTurretAngles[io.i]` (io.f)
pub const EE_UNITS_UNIT_LOAD_GET_TURRET_ANGLE: u32 = 1;
/// `sLoadedContacts.empty()` (io.i)
pub const EE_UNITS_UNIT_LOAD_GET_CONTACTS_EMPTY: u32 = 2;

// ---- the sections' statements (`EE_UNITS_UNIT_LOAD_SET`'s index; io.i the value)
/// `mBuildQueue.clear()`
pub const EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_CLEAR: u32 = 24;
/// the build queue entry's second (`theSecond = io.i`)
pub const EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_SECOND: u32 = 25;
/// `mBuildQueue.push_back({io.i, theSecond})`
pub const EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_PUSH: u32 = 26;
/// `mCitizenBuildQueue.clear()`
pub const EE_UNITS_UNIT_LOAD_SET_CITIZEN_QUEUE_CLEAR: u32 = 27;
/// `mCitizenBuildQueue.push_back(reinterpret_cast<EEUnit*>(io.i))`
pub const EE_UNITS_UNIT_LOAD_SET_CITIZEN_QUEUE_PUSH: u32 = 28;
/// `mCargo.clear()`
pub const EE_UNITS_UNIT_LOAD_SET_CARGO_CLEAR: u32 = 29;
/// `mCargo.push_back(reinterpret_cast<EEComplexUnit*>(io.i))`
pub const EE_UNITS_UNIT_LOAD_SET_CARGO_PUSH: u32 = 30;
/// `mHeldUnits.resize(io.i, NULL)`
pub const EE_UNITS_UNIT_LOAD_SET_HELD_RESIZE: u32 = 31;
/// the slot of the next element set (`theSlot = io.i`)
pub const EE_UNITS_UNIT_LOAD_SET_SLOT: u32 = 32;
/// `mHeldUnits[theSlot] = reinterpret_cast<EEComplexUnit*>(io.i)`
pub const EE_UNITS_UNIT_LOAD_SET_HELD_UNIT: u32 = 33;
/// `mUnitVariables[theSlot] = io.i`
pub const EE_UNITS_UNIT_LOAD_SET_UNIT_VARIABLE: u32 = 34;
/// `EEAfterLoad` of the live state io.i (its apply at the first tick, its free with the fix-up)
pub const EE_UNITS_UNIT_LOAD_SET_LIVE_REGISTER: u32 = 35;
/// `EEAfterLoad(RestoreLoadedContacts)`
pub const EE_UNITS_UNIT_LOAD_SET_CONTACTS_REGISTER: u32 = 36;
/// `sLoadedContacts.push_back(((this, mUnitID), {}))`
pub const EE_UNITS_UNIT_LOAD_SET_CONTACTS_BEGIN: u32 = 37;
/// `sLoadedContacts.back().second.push_back(io.i)`
pub const EE_UNITS_UNIT_LOAD_SET_CONTACT_ID: u32 = 38;

// ---- the live state's statements at the first tick (`ee_units_dop_host`, after dop_repair.rs's)
/// `static_cast<RTSUnit*>(this) == gWorld.GetUnit(b)`
pub const EE_UNITS_DOP_IS_WORLD_UNIT: i32 = 200;
/// `mAnimationFraction = the float of the bits b`
pub const EE_UNITS_DOP_SET_ANIMATION_FRACTION: i32 = 201;
/// `mNextFrame = b`
pub const EE_UNITS_DOP_SET_NEXT_FRAME: i32 = 202;
/// `mCollisionRadius = the float of the bits b`
pub const EE_UNITS_DOP_SET_COLLISION_RADIUS: i32 = 203;
/// `mPushVelocityX = the float of the bits b`
pub const EE_UNITS_DOP_SET_PUSH_VELOCITY_X: i32 = 204;
/// `mPushVelocityY = the float of the bits b`
pub const EE_UNITS_DOP_SET_PUSH_VELOCITY_Y: i32 = 205;
/// `mF24C = the float of the bits b`
pub const EE_UNITS_DOP_SET_F24C: i32 = 206;
/// `mAIState = b`
pub const EE_UNITS_DOP_SET_AI_STATE: i32 = 207;
/// `mNextTriggerTime = b`
pub const EE_UNITS_DOP_SET_NEXT_TRIGGER_TIME: i32 = 208;
/// `mIgnoredTargets.clear()`
pub const EE_UNITS_DOP_IGNORED_TARGETS_CLEAR: i32 = 209;
/// `mIgnoredTargets.push_back(the attacker of the six values at a)`
pub const EE_UNITS_DOP_IGNORED_TARGETS_PUSH: i32 = 210;
/// `mBoundedPathTiles.clear()`
pub const EE_UNITS_DOP_BOUNDED_PATH_TILES_CLEAR: i32 = 211;
/// `mBoundedPathTiles.push_back(U2DPoint<long>(b, c))`
pub const EE_UNITS_DOP_BOUNDED_PATH_TILES_PUSH: i32 = 212;
/// `mPhysicalModel.SetFrame(b)`
pub const EE_UNITS_DOP_SET_FRAME: i32 = 213;
/// `mFlightTime = b`
pub const EE_UNITS_DOP_SET_FLIGHT_TIME: i32 = 214;
/// `mLastHitPoints = b`
pub const EE_UNITS_DOP_SET_LAST_HIT_POINTS: i32 = 215;
/// `mGarrisonCount = b`
pub const EE_UNITS_DOP_SET_GARRISON_COUNT: i32 = 216;
/// `mPower = b`
pub const EE_UNITS_DOP_SET_POWER: i32 = 217;
/// `GetModel() != NULL ? GetModel()->GetNumParts() : 0`
pub const EE_UNITS_DOP_MODEL_PARTS_OR_ZERO: i32 = 218;
/// `mTurretAngles != NULL`
pub const EE_UNITS_DOP_HAS_TURRET_ANGLES: i32 = 219;
/// `mTurretAngles[b] = the float of the bits c`
pub const EE_UNITS_DOP_SET_TURRET_ANGLE: i32 = 220;

fn read<H: SaveHost + ?Sized>(h: &mut H, op: i32, old: i64) -> (i32, i64) {
    let mut io = EeUnitsUnitSaveIo { i: old, ..Default::default() };
    let r = h.op(op, 0, &mut io);
    (r, io.i)
}

fn read_float<H: SaveHost + ?Sized>(h: &mut H, old: f32) -> (i32, f32) {
    let mut io = EeUnitsUnitSaveIo { f: old, ..Default::default() };
    let r = h.op(EE_UNITS_UNIT_LOAD_READ_FLOAT, 0, &mut io);
    (r, io.f)
}

fn set<H: SaveHost + ?Sized>(h: &mut H, index: u32, i: i64) {
    h.op(EE_UNITS_UNIT_LOAD_SET, index, &mut EeUnitsUnitSaveIo { i, ..Default::default() });
}

fn get<H: SaveHost + ?Sized>(h: &mut H, index: u32, i: i64) -> EeUnitsUnitSaveIo {
    let mut io = EeUnitsUnitSaveIo { i, ..Default::default() };
    h.op(EE_UNITS_UNIT_LOAD_GET, index, &mut io);
    io
}

/// The build queue: a `long` count, then (`long` technology, `unsigned long` flag from 0xf8 on, else 0); each
/// entry pushed when its reads succeed. The result.
pub fn build_queue<H: SaveHost + ?Sized>(h: &mut H, minor: u32) -> i32 {
    set(h, EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_CLEAR, 0);
    let (mut r, count) = read(h, EE_UNITS_UNIT_LOAD_READ_LONG, 0);
    let (mut first, mut second) = (0i64, 0i64);
    let mut i = 0i64;
    while i < count && r == 0 {
        (r, first) = read(h, EE_UNITS_UNIT_LOAD_READ_LONG, first);
        if r == 0 {
            if minor < 0xf8 {
                second = 0;
            } else {
                (r, second) = read(h, EE_UNITS_UNIT_LOAD_READ_ULONG, second);
            }
            if r == 0 {
                set(h, EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_SECOND, second);
                set(h, EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_PUSH, first);
            }
        }
        i += 1;
    }
    r
}

/// The citizen queue or the cargo (`clear`, `push`): a `long` count, then the `unsigned long` ids read into the
/// original's `theValue` (`value`, carried in and out). The result.
pub fn ids<H: SaveHost + ?Sized>(h: &mut H, clear: u32, push: u32, value: &mut i64) -> i32 {
    set(h, clear, 0);
    let (mut r, count) = read(h, EE_UNITS_UNIT_LOAD_READ_LONG, 0);
    let mut j = 0i64;
    while j < count && r == 0 {
        (r, *value) = read(h, EE_UNITS_UNIT_LOAD_READ_ULONG, *value);
        if r == 0 {
            set(h, push, *value);
        }
        j += 1;
    }
    r
}

/// The held units: a `long` count (the vector resized to it), then the ids into `theValue`; a failed read with
/// elements left leaves for `LoadLateVersions` (the bool). The result.
pub fn held_units<H: SaveHost + ?Sized>(h: &mut H, value: &mut i64) -> (i32, bool) {
    let (mut r, count) = read(h, EE_UNITS_UNIT_LOAD_READ_LONG, 0);
    set(h, EE_UNITS_UNIT_LOAD_SET_HELD_RESIZE, count);
    for n in 0..count.max(0) {
        if r != 0 {
            return (r, true);
        }
        (r, *value) = read(h, EE_UNITS_UNIT_LOAD_READ_ULONG, *value);
        set(h, EE_UNITS_UNIT_LOAD_SET_SLOT, n);
        set(h, EE_UNITS_UNIT_LOAD_SET_HELD_UNIT, *value);
    }
    (r, false)
}

/// The unit variables: a `long` count, then `long`s read into the original's `theState` (`state`). The result.
pub fn unit_variables<H: SaveHost + ?Sized>(h: &mut H, state: &mut i64) -> i32 {
    let (mut r, count) = read(h, EE_UNITS_UNIT_LOAD_READ_LONG, 0);
    let mut k = 0i64;
    while k < count && r == 0 {
        (r, *state) = read(h, EE_UNITS_UNIT_LOAD_READ_LONG, *state);
        if r == 0 {
            set(h, EE_UNITS_UNIT_LOAD_SET_SLOT, k);
            set(h, EE_UNITS_UNIT_LOAD_SET_UNIT_VARIABLE, *state);
        }
        k += 1;
    }
    r
}

/// The live state read at the load, put back at the first tick.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeUnitsLiveState {
    animation_fraction: f32,
    next_frame: i64,
    collision_radius: f32,
    push_velocity_x: f32,
    push_velocity_y: f32,
    f24c: f32,
    ai_state: i64,
    next_trigger_time: i64,
    /// (tile x, tile y, from x, from y, unit id, time)
    ignored_targets: Vec<[i64; 6]>,
    bounded_path_tiles: Vec<(i64, i64)>,
    running: bool,
    frame: i64,
    flight_time: i64,
    rotting_away_time: i64,
    current_research: i64,
    last_hit_points: i64,
    garrison_count: i64,
    power: i64,
    /// the turret angles as they were before the reads (the load's LoadMedia zeroes them after Load)
    turret_angles: Vec<f32>,
}

/// The port's live state (0.254; 0.256's running state when `running`): the floats and counters, the ignored
/// targets, the bounded path tiles, then the running counters and the contacts' ids; on success the contacts go to
/// `sLoadedContacts` and the state to `EEAfterLoad`. The result.
pub fn live_state<H: SaveHost + ?Sized>(h: &mut H, running: bool) -> i32 {
    let mut s = EeUnitsLiveState::default();
    let turrets = get(h, EE_UNITS_UNIT_LOAD_GET_TURRET_COUNT, 0).i;
    for q in 0..turrets.max(0) {
        let angle = get(h, EE_UNITS_UNIT_LOAD_GET_TURRET_ANGLE, q).f;
        s.turret_angles.push(angle);
    }
    let mut live_count = 0i64;
    let mut r;
    macro_rules! long {
        ($op:expr, $f:expr) => {{
            let (res, v) = read(h, $op, $f);
            $f = v;
            res
        }};
    }
    macro_rules! float {
        ($f:expr) => {{
            let (res, v) = read_float(h, $f);
            $f = v;
            res
        }};
    }
    r = float!(s.animation_fraction);
    if r == 0 {
        r = long!(EE_UNITS_UNIT_LOAD_READ_LONG, s.next_frame);
    }
    if r == 0 {
        r = float!(s.collision_radius);
    }
    if r == 0 {
        r = float!(s.push_velocity_x);
    }
    if r == 0 {
        r = float!(s.push_velocity_y);
    }
    if r == 0 {
        r = float!(s.f24c);
    }
    if r == 0 {
        r = long!(EE_UNITS_UNIT_LOAD_READ_LONG, s.ai_state);
    }
    if r == 0 {
        r = long!(EE_UNITS_UNIT_LOAD_READ_ULONG, s.next_trigger_time);
    }
    if r == 0 {
        r = long!(EE_UNITS_UNIT_LOAD_READ_ULONG, live_count);
    }
    let mut k = 0i64;
    while k < live_count && r == 0 {
        let mut a = [0i64; 6];
        for (n, v) in a.iter_mut().enumerate() {
            let op = if n < 4 { EE_UNITS_UNIT_LOAD_READ_LONG } else { EE_UNITS_UNIT_LOAD_READ_ULONG };
            r = long!(op, *v);
            if r != 0 {
                break;
            }
        }
        if r == 0 {
            s.ignored_targets.push(a);
        }
        k += 1;
    }
    live_count = 0;
    if r == 0 {
        r = long!(EE_UNITS_UNIT_LOAD_READ_ULONG, live_count);
    }
    let mut k = 0i64;
    while k < live_count && r == 0 {
        let (mut x, mut y) = (0i64, 0i64);
        r = long!(EE_UNITS_UNIT_LOAD_READ_LONG, x);
        if r == 0 {
            r = long!(EE_UNITS_UNIT_LOAD_READ_LONG, y);
        }
        if r == 0 {
            s.bounded_path_tiles.push((x, y));
        }
        k += 1;
    }
    if r == 0 && running {
        s.running = true;
        r = long!(EE_UNITS_UNIT_LOAD_READ_ULONG, s.frame);
        for f in [
            &mut s.flight_time,
            &mut s.rotting_away_time,
            &mut s.current_research,
            &mut s.last_hit_points,
            &mut s.garrison_count,
            &mut s.power,
        ] {
            if r == 0 {
                let (res, v) = read(h, EE_UNITS_UNIT_LOAD_READ_LONG, *f);
                *f = v;
                r = res;
            }
        }
        if r == 0 {
            r = long!(EE_UNITS_UNIT_LOAD_READ_ULONG, live_count);
        }
        let mut contacts = Vec::new();
        let mut k = 0i64;
        while k < live_count && r == 0 {
            let mut id = 0i64;
            r = long!(EE_UNITS_UNIT_LOAD_READ_ULONG, id);
            if r == 0 {
                contacts.push(id);
            }
            k += 1;
        }
        if r == 0 {
            // once per load (the first unit read registers it; it runs at the first tick)
            if get(h, EE_UNITS_UNIT_LOAD_GET_CONTACTS_EMPTY, 0).i != 0 {
                set(h, EE_UNITS_UNIT_LOAD_SET_CONTACTS_REGISTER, 0);
            }
            set(h, EE_UNITS_UNIT_LOAD_SET_CONTACTS_BEGIN, 0);
            for id in contacts {
                set(h, EE_UNITS_UNIT_LOAD_SET_CONTACT_ID, id);
            }
        }
    }
    if r == 0 {
        let p = Box::into_raw(Box::new(s));
        set(h, EE_UNITS_UNIT_LOAD_SET_LIVE_REGISTER, p as usize as i64);
    }
    r
}

fn bits(v: f32) -> i64 {
    i64::from(v.to_bits())
}

/// The live state put back at the loaded game's first tick, while `unit` is still the world's unit of `id` (not a
/// doppelganger, not freed).
pub fn live_apply<H: DopHost + ?Sized>(h: &mut H, unit: u64, id: i64, s: &EeUnitsLiveState) {
    let mut q = |op: i32, a: u64, b: i64, c: i64| h.dop(op, unit, a, b, c);
    if q(EE_UNITS_DOP_IS_WORLD_UNIT, 0, id, 0) == 0 {
        return;
    }
    q(EE_UNITS_DOP_SET_ANIMATION_FRACTION, 0, bits(s.animation_fraction), 0);
    q(EE_UNITS_DOP_SET_NEXT_FRAME, 0, s.next_frame, 0);
    q(EE_UNITS_DOP_SET_COLLISION_RADIUS, 0, bits(s.collision_radius), 0);
    q(EE_UNITS_DOP_SET_PUSH_VELOCITY_X, 0, bits(s.push_velocity_x), 0);
    q(EE_UNITS_DOP_SET_PUSH_VELOCITY_Y, 0, bits(s.push_velocity_y), 0);
    q(EE_UNITS_DOP_SET_F24C, 0, bits(s.f24c), 0);
    q(EE_UNITS_DOP_SET_AI_STATE, 0, s.ai_state, 0);
    q(EE_UNITS_DOP_SET_NEXT_TRIGGER_TIME, 0, s.next_trigger_time, 0);
    q(EE_UNITS_DOP_IGNORED_TARGETS_CLEAR, 0, 0, 0);
    for a in &s.ignored_targets {
        q(EE_UNITS_DOP_IGNORED_TARGETS_PUSH, a.as_ptr() as usize as u64, 0, 0);
    }
    q(EE_UNITS_DOP_BOUNDED_PATH_TILES_CLEAR, 0, 0, 0);
    for &(x, y) in &s.bounded_path_tiles {
        q(EE_UNITS_DOP_BOUNDED_PATH_TILES_PUSH, 0, x, y);
    }
    if s.running {
        q(EE_UNITS_DOP_SET_FRAME, 0, s.frame, 0);
        q(EE_UNITS_DOP_SET_FLIGHT_TIME, 0, s.flight_time, 0);
        q(crate::units::dop_repair::EE_UNITS_DOP_SET_ROTTING_AWAY_TIME, 0, s.rotting_away_time, 0);
        q(crate::units::dop_repair::EE_UNITS_DOP_SET_CURRENT_RESEARCH, 0, s.current_research, 0);
        q(EE_UNITS_DOP_SET_LAST_HIT_POINTS, 0, s.last_hit_points, 0);
        q(EE_UNITS_DOP_SET_GARRISON_COUNT, 0, s.garrison_count, 0);
        q(EE_UNITS_DOP_SET_POWER, 0, s.power, 0);
    }
    let parts = q(EE_UNITS_DOP_MODEL_PARTS_OR_ZERO, 0, 0, 0);
    let mut k = 0usize;
    while k < s.turret_angles.len() && (k as i64) < parts && q(EE_UNITS_DOP_HAS_TURRET_ANGLES, 0, 0, 0) != 0 {
        q(EE_UNITS_DOP_SET_TURRET_ANGLE, 0, k as i64, bits(s.turret_angles[k]));
        k += 1;
    }
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::dop_repair::game::Game;
    use std::os::raw::c_void;

    /// The live state `live` (from `EE_UNITS_UNIT_LOAD_SET_LIVE_REGISTER`) put back on `unit` if it is still the
    /// world's unit of `id`.
    ///
    /// # Safety
    /// `live` is the registered state, alive (its shared pointer held); `unit` may be stale (the first statement
    /// compares it with the world's unit of `id` before any other use).
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_unit_live_apply(unit: *mut c_void, id: u32, live: *const c_void) {
        // SAFETY: the caller's live state
        let s = unsafe { &*(live as *const EeUnitsLiveState) };
        live_apply(&mut Game, unit as usize as u64, i64::from(id), s);
    }
}

/// Frees a live state (the deleter of the fix-up's shared pointer).
///
/// # Safety
/// `live` came from `EE_UNITS_UNIT_LOAD_SET_LIVE_REGISTER` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_units_unit_live_free(live: *mut std::ffi::c_void) {
    if !live.is_null() {
        // SAFETY: the caller's contract (a Box this module made)
        drop(unsafe { Box::from_raw(live as *mut EeUnitsLiveState) });
    }
}

#[cfg(test)]
#[path = "dop_load_tests.rs"]
mod tests;
