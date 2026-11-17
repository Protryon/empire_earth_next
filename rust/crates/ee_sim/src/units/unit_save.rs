//! The units' saves over their records (R5b.4, the twenty-sixth round): `RTSUnit::Save` (0x63dc3a), `EEUnit::Save`
//! (0x6299e5) and `EEComplexUnit::Save` (0x5f417e) as Rust sequences over the records the classes derive from
//! (unit_fields.rs) and one host callback ([`EeUnitsUnitSaveFn`]), as player_save.rs does for the player.
//!
//! The callback does the file's primitive writes with the `FSFile` overload the original called (`long`, `unsigned
//! long`, `float`, `bool`, `unsigned short`) and the unit's C++ parts by operation: the containers and their loops
//! (each section returns its last result, with the original's own loop shape), the composite members' `Save`, the
//! ids the original computed from pointers, and the stats' and class's maxima. The plain members are read from the
//! records through their raw pointers (a `bool` as its byte), never by a reference held across a host call.
//!
//! Kept quirks: the results the original ignored (the collision tile's x, the open fraction, the holder's id, the
//! foundation flag) and the ones it overwrote (the goal queues' count, the held units' count, the unit variables'
//! count each start a new result whatever came before).

#![allow(non_snake_case)]
// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts to the i64 slot are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;

use crate::units::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

/// One operation's values: a number (`long`/`unsigned long`/`unsigned short`/an id/a count), a float, a bool (0 or 1):
/// the player's saves' io, so the file's primitives are `EESavePrimitive`'s (ee/EEPlayer.h).
pub type EeUnitsUnitSaveIo = crate::units::player_save::EeUnitsPlayerSaveIo;

/// The host: `(ctx, op, index, io) -> the operation's result` (kErrorOK 0; a query's answer in `io`).
pub type EeUnitsUnitSaveFn = unsafe extern "C" fn(*mut c_void, i32, u32, *mut EeUnitsUnitSaveIo) -> i32;

// ---- the file's primitives (io holds the value)
pub const EE_UNITS_UNIT_SAVE_WRITE_LONG: i32 = crate::units::player_save::EE_UNITS_PLAYER_SAVE_WRITE_LONG;
pub const EE_UNITS_UNIT_SAVE_WRITE_ULONG: i32 = crate::units::player_save::EE_UNITS_PLAYER_SAVE_WRITE_ULONG;
pub const EE_UNITS_UNIT_SAVE_WRITE_FLOAT: i32 = crate::units::player_save::EE_UNITS_PLAYER_SAVE_WRITE_FLOAT;
pub const EE_UNITS_UNIT_SAVE_WRITE_BOOL: i32 = crate::units::player_save::EE_UNITS_PLAYER_SAVE_WRITE_BOOL;
pub const EE_UNITS_UNIT_SAVE_WRITE_USHORT: i32 = crate::units::player_save::EE_UNITS_PLAYER_SAVE_WRITE_USHORT;
// ---- queries (the answer in io.i)
/// `mStats->mMaxHitPoints`, `mStats->mFlightTime`, `mStats->mPower`, `GetUnitClass()->mGarrisonCapacity`
pub const EE_UNITS_UNIT_SAVE_MAX: i32 = 20;
pub const EE_UNITS_UNIT_SAVE_MAX_HIT_POINTS: u32 = 0;
pub const EE_UNITS_UNIT_SAVE_MAX_FLIGHT_TIME: u32 = 1;
pub const EE_UNITS_UNIT_SAVE_MAX_POWER: u32 = 2;
pub const EE_UNITS_UNIT_SAVE_MAX_GARRISON: u32 = 3;
/// the value the original wrote for a pointer member or a part of one (`index`: `EE_UNITS_UNIT_SAVE_ID_*`), as an
/// `unsigned long` (io.i)
pub const EE_UNITS_UNIT_SAVE_ID: i32 = 21;
pub const EE_UNITS_UNIT_SAVE_ID_FRAME: u32 = 0;
pub const EE_UNITS_UNIT_SAVE_ID_LAUNCHER_CLASS: u32 = 1;
pub const EE_UNITS_UNIT_SAVE_ID_PATROL_GROUP: u32 = 2;
pub const EE_UNITS_UNIT_SAVE_ID_PREVIOUS_PART: u32 = 3;
pub const EE_UNITS_UNIT_SAVE_ID_NEXT_PART: u32 = 4;
pub const EE_UNITS_UNIT_SAVE_ID_HOME_BASE: u32 = 5;
pub const EE_UNITS_UNIT_SAVE_ID_CONTAINER: u32 = 6;
pub const EE_UNITS_UNIT_SAVE_ID_COMMAND: u32 = 7;
pub const EE_UNITS_UNIT_SAVE_ID_GATHERED_RESOURCE: u32 = 8;
pub const EE_UNITS_UNIT_SAVE_ID_LAST_ATTACKER: u32 = 9;
pub const EE_UNITS_UNIT_SAVE_ID_OWNER: u32 = 10;
pub const EE_UNITS_UNIT_SAVE_ID_MINE: u32 = 11;
/// (the herd's id, 0 for none)
pub const EE_UNITS_UNIT_SAVE_ID_HERD: u32 = 12;
pub const EE_UNITS_UNIT_SAVE_ID_HOLDER: u32 = 13;
pub const EE_UNITS_UNIT_SAVE_ID_BEHAVIOR: u32 = 14;
pub const EE_UNITS_UNIT_SAVE_ID_LAST_RESOURCE: u32 = 15;
/// the effect handles' values (`mConversionEffect`, `mHealEffect` as the original's `long`)
pub const EE_UNITS_UNIT_SAVE_ID_CONVERSION_EFFECT: u32 = 16;
pub const EE_UNITS_UNIT_SAVE_ID_HEAL_EFFECT: u32 = 17;
/// `mPhysicalModel.GetScale()` and `mInitialContactLocation`'s x, y, z (io.f; `index` 0..3)
pub const EE_UNITS_UNIT_SAVE_FLOAT: i32 = 22;
pub const EE_UNITS_UNIT_SAVE_FLOAT_SCALE: u32 = 0;
pub const EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_X: u32 = 1;
pub const EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_Y: u32 = 2;
pub const EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_Z: u32 = 3;
/// the collision tile's x and y (`index` 0, 1; io.i, written as `long`)
pub const EE_UNITS_UNIT_SAVE_COLLISION_TILE: i32 = 23;
// ---- the C++ parts that write themselves (the result)
/// a section (`index`: `EE_UNITS_UNIT_SAVE_SECTION_*`): its count and elements with the original's loop
pub const EE_UNITS_UNIT_SAVE_SECTION: i32 = 30;
pub const EE_UNITS_UNIT_SAVE_SECTION_BUILD_QUEUE: u32 = 0;
pub const EE_UNITS_UNIT_SAVE_SECTION_CITIZEN_QUEUE: u32 = 1;
pub const EE_UNITS_UNIT_SAVE_SECTION_CARGO: u32 = 2;
pub const EE_UNITS_UNIT_SAVE_SECTION_WAYPOINTS: u32 = 3;
pub const EE_UNITS_UNIT_SAVE_SECTION_TURRET_ANGLES: u32 = 4;
pub const EE_UNITS_UNIT_SAVE_SECTION_GOAL_QUEUES: u32 = 5;
pub const EE_UNITS_UNIT_SAVE_SECTION_HELD_UNITS: u32 = 6;
pub const EE_UNITS_UNIT_SAVE_SECTION_UNIT_VARIABLES: u32 = 7;
pub const EE_UNITS_UNIT_SAVE_SECTION_IGNORED_TARGETS: u32 = 8;
pub const EE_UNITS_UNIT_SAVE_SECTION_BOUNDED_PATH_TILES: u32 = 9;
pub const EE_UNITS_UNIT_SAVE_SECTION_CONTACTS: u32 = 10;
/// a composite member's `Save` (`index`: `EE_UNITS_UNIT_SAVE_PART_*`)
pub const EE_UNITS_UNIT_SAVE_PART: i32 = 31;
pub const EE_UNITS_UNIT_SAVE_PART_PROJECTILE_START: u32 = 0;
pub const EE_UNITS_UNIT_SAVE_PART_LOCATION: u32 = 1;
pub const EE_UNITS_UNIT_SAVE_PART_ANGULAR_VELOCITY: u32 = 2;
pub const EE_UNITS_UNIT_SAVE_PART_ORIENTATION: u32 = 3;
pub const EE_UNITS_UNIT_SAVE_PART_U0DC: u32 = 4;
pub const EE_UNITS_UNIT_SAVE_PART_COLOR: u32 = 5;
pub const EE_UNITS_UNIT_SAVE_PART_FIRE_EFFECT: u32 = 6;
/// `gPathManager.GetPathGoal`: the bool written, then the goal tile's `Save`
pub const EE_UNITS_UNIT_SAVE_PART_PATH: u32 = 7;
pub const EE_UNITS_UNIT_SAVE_PART_LAST_MAP_POSITION: u32 = 8;
/// `if (mHerd != NULL) mHerd->HasUnit(this)` after the herd's id (no write; 0)
pub const EE_UNITS_UNIT_SAVE_PART_HERD_HAS_UNIT: u32 = 9;

/// The save's inputs: the version, whether it carries the port's live state (0.254) and running state (0.256).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsUnitSaveIn {
    pub major: u32,
    pub minor: u32,
    pub live_state: u8,
    pub running_state: u8,
}

/// The host as a Rust value (the game's callback, a recording mock in the tests).
pub trait SaveHost {
    fn op(&mut self, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32;
}

/// The game's host: the C++ callback and its context.
pub(crate) struct CallbackHost {
    pub(crate) ctx: *mut c_void,
    pub(crate) f: EeUnitsUnitSaveFn,
}

impl SaveHost for CallbackHost {
    fn op(&mut self, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32 {
        // SAFETY: the C++ caller's callback and context
        unsafe { (self.f)(self.ctx, op, index, io) }
    }
}

/// The writer: the host and the primitives.
struct W<'a, H: SaveHost> {
    host: &'a mut H,
}

impl<H: SaveHost> W<'_, H> {
    fn long(&mut self, v: i64) -> i32 {
        self.host.op(EE_UNITS_UNIT_SAVE_WRITE_LONG, 0, &mut EeUnitsUnitSaveIo { i: v, ..Default::default() })
    }
    fn ulong(&mut self, v: i64) -> i32 {
        self.host.op(EE_UNITS_UNIT_SAVE_WRITE_ULONG, 0, &mut EeUnitsUnitSaveIo { i: v, ..Default::default() })
    }
    fn float(&mut self, v: f32) -> i32 {
        self.host.op(EE_UNITS_UNIT_SAVE_WRITE_FLOAT, 0, &mut EeUnitsUnitSaveIo { f: v, ..Default::default() })
    }
    fn bool(&mut self, v: u8) -> i32 {
        self.host.op(EE_UNITS_UNIT_SAVE_WRITE_BOOL, 0, &mut EeUnitsUnitSaveIo { b: v, ..Default::default() })
    }
    fn ushort(&mut self, v: u16) -> i32 {
        self.host.op(
            EE_UNITS_UNIT_SAVE_WRITE_USHORT,
            0,
            &mut EeUnitsUnitSaveIo { i: i64::from(v), ..Default::default() },
        )
    }
    fn query(&mut self, op: i32, index: u32) -> EeUnitsUnitSaveIo {
        let mut io = EeUnitsUnitSaveIo::default();
        self.host.op(op, index, &mut io);
        io
    }
    fn id(&mut self, index: u32) -> i64 {
        self.query(EE_UNITS_UNIT_SAVE_ID, index).i
    }
    fn max(&mut self, index: u32) -> i64 {
        self.query(EE_UNITS_UNIT_SAVE_MAX, index).i
    }
    fn section(&mut self, index: u32) -> i32 {
        self.host.op(EE_UNITS_UNIT_SAVE_SECTION, index, &mut EeUnitsUnitSaveIo::default())
    }
    fn part(&mut self, index: u32) -> i32 {
        self.host.op(EE_UNITS_UNIT_SAVE_PART, index, &mut EeUnitsUnitSaveIo::default())
    }
}

/// Reads a plain member through the record's raw pointer.
macro_rules! get {
    ($p:expr, $f:ident) => {
        // SAFETY: the caller's pointer names a live record
        unsafe { std::ptr::read(std::ptr::addr_of!((*$p).$f)) }
    };
}

/// A `bool` member's byte (0 or 1).
macro_rules! flag {
    ($p:expr, $f:ident) => {
        // SAFETY: the caller's pointer names a live record
        unsafe { crate::units::unit_fields::bool_byte(std::ptr::addr_of!((*$p).$f)) }
    };
}

/// A count over a maximum as the original's `float` fraction: `(float)value / max` (the `long` maximum converted to
/// `float`), 0 when either is under 1.
fn fraction(value: i64, max: i64) -> f32 {
    if value < 1 || max < 1 {
        0.0
    } else {
        value as f32 / max as f32
    }
}

/// `RTSUnit::Save`: the tile, the id and the flags (the editor selection written as false; versions up to 0.6 the id
/// and the tile only). The first write error.
///
/// # Safety
/// `rts` points to a live record.
pub unsafe fn rts_save<H: SaveHost>(host: &mut H, rts: *const EeUnitsRtsUnitFields, inp: &EeUnitsUnitSaveIn) -> i32 {
    let mut w = W { host };
    let (x, y, id) = (get!(rts, mTileX) as i64, get!(rts, mTileY) as i64, get!(rts, mUnitID) as i64);
    if inp.major == 0 && inp.minor <= 6 {
        let mut r = w.ulong(id);
        if r == 0 {
            r = w.long(x);
            if r == 0 {
                r = w.long(y);
            }
        }
        return r;
    }
    let mut r = w.long(x);
    if r == 0 {
        r = w.long(y);
        if r == 0 {
            r = w.ulong(id);
            if r == 0 {
                r = w.bool(flag!(rts, mIsQueuedForDelete));
                if r == 0 {
                    r = w.bool(0);
                    if r == 0 {
                        r = w.bool(flag!(rts, mIsValid));
                    }
                }
            }
        }
    }
    r
}

/// `EEUnit::Save`: the base unit's part, the resource type, the hit points as a fraction of the stats' maximum, the
/// amount, a zero. The first write error.
///
/// # Safety
/// The pointers name the live records of one unit.
pub unsafe fn unit_save<H: SaveHost>(
    host: &mut H,
    rts: *const EeUnitsRtsUnitFields,
    unit: *const EeUnitsUnitFields,
    inp: &EeUnitsUnitSaveIn,
) -> i32 {
    // SAFETY: the caller's contract
    let r = unsafe { rts_save(host, rts, inp) };
    if r != 0 {
        return r;
    }
    let mut w = W { host };
    let r = w.ulong(get!(unit, mResourceType) as i64);
    if r != 0 {
        return r;
    }
    // (float)mHitPoints / mStats->mMaxHitPoints: no guard (a zero maximum divides by zero, as the original)
    let max = w.max(EE_UNITS_UNIT_SAVE_MAX_HIT_POINTS);
    let r = w.float(get!(unit, mHitPoints) as i64 as f32 / max as f32);
    if r != 0 {
        return r;
    }
    let mut r = w.long(get!(unit, mResourceAmount) as i64);
    if r == 0 {
        r = w.long(0);
    }
    r
}

/// Runs the writes in order while each returns 0; the first non-zero result (0 when all wrote).
macro_rules! chain {
    ($($e:expr),+ $(,)?) => {{
        let mut r = 0;
        $( if r == 0 { r = $e; } )+
        r
    }};
}

/// `EEComplexUnit::Save` after `EEUnit::Save`: the sections, the members and the port's live and running state, in
/// the original's order with its three error labels (`SaveGoals`, `SaveFinalFields`, the end).
///
/// # Safety
/// `cx` points to the live record of the unit the host saves.
pub unsafe fn complex_save_rest<H: SaveHost>(
    host: &mut H,
    cx: *const EeUnitsComplexUnitFields,
    inp: &EeUnitsUnitSaveIn,
) -> i32 {
    let mut w = W { host };
    // (a failed write goes on at SaveGoals, whose first write starts a new result)
    save_to_goals(&mut w, cx);
    // SaveGoals:
    let mut r = w.section(EE_UNITS_UNIT_SAVE_SECTION_GOAL_QUEUES);
    if r == 0 {
        let v = w.id(EE_UNITS_UNIT_SAVE_ID_CONVERSION_EFFECT);
        r = w.long(v);
        if r == 0 {
            let v = w.id(EE_UNITS_UNIT_SAVE_ID_HEAL_EFFECT);
            r = w.long(v);
        }
    }
    let herd = w.id(EE_UNITS_UNIT_SAVE_ID_HERD);
    if r == 0 {
        r = w.ulong(herd);
    }
    w.part(EE_UNITS_UNIT_SAVE_PART_HERD_HAS_UNIT);
    let holder = w.id(EE_UNITS_UNIT_SAVE_ID_HOLDER);
    if r == 0 {
        w.ulong(holder);
    }
    r = w.section(EE_UNITS_UNIT_SAVE_SECTION_HELD_UNITS);
    if r == 0 {
        save_to_final(&mut w, cx);
    }
    // SaveFinalFields:
    r = w.section(EE_UNITS_UNIT_SAVE_SECTION_UNIT_VARIABLES);
    if r != 0 {
        return r;
    }
    r = chain!(
        w.long(get!(cx, mFormationRank) as i64),
        w.long(get!(cx, mWaterState) as i64),
        w.part(EE_UNITS_UNIT_SAVE_PART_LAST_MAP_POSITION),
        w.ulong(get!(cx, mCloaked) as i64),
        w.ulong(get!(cx, mCamouflageTimer) as i64),
        w.bool(flag!(cx, mIsSpecialAttack)),
        w.ulong(get!(cx, mDisabledReason) as i64),
        w.ushort(get!(cx, mPlanetID)),
        w.float(get!(cx, mSight)),
        w.ulong(get!(cx, mU32C) as i64),
    );
    if r == 0 && inp.live_state != 0 {
        r = chain!(
            w.float(get!(cx, mAnimationFraction)),
            w.long(get!(cx, mNextFrame) as i64),
            w.float(get!(cx, mCollisionRadius)),
            w.float(get!(cx, mPushVelocityX)),
            w.float(get!(cx, mPushVelocityY)),
            w.float(get!(cx, mF24C)),
            w.long(get!(cx, mAIState) as i64),
            w.ulong(get!(cx, mNextTriggerTime) as i64),
            w.section(EE_UNITS_UNIT_SAVE_SECTION_IGNORED_TARGETS),
        );
        if r == 0 {
            r = w.section(EE_UNITS_UNIT_SAVE_SECTION_BOUNDED_PATH_TILES);
        }
        if r == 0 && inp.running_state != 0 {
            let frame = w.id(EE_UNITS_UNIT_SAVE_ID_FRAME);
            r = chain!(
                w.ulong(frame),
                w.long(get!(cx, mFlightTime) as i64),
                w.long(get!(cx, mRottingAwayTime) as i64),
                w.long(get!(cx, mCurrentResearch) as i64),
                w.long(get!(cx, mLastHitPoints) as i64),
                w.long(get!(cx, mGarrisonCount) as i64),
                w.long(get!(cx, mPower) as i64),
                w.section(EE_UNITS_UNIT_SAVE_SECTION_CONTACTS),
            );
        }
    }
    r
}

/// From the build queue to the open fraction (the part a failed write leaves for `SaveGoals`).
fn save_to_goals<H: SaveHost>(w: &mut W<'_, H>, cx: *const EeUnitsComplexUnitFields) -> i32 {
    // each section's count write starts a new result: the build queue's, the citizen queue's and the cargo's errors
    // are not tested
    w.section(EE_UNITS_UNIT_SAVE_SECTION_BUILD_QUEUE);
    w.section(EE_UNITS_UNIT_SAVE_SECTION_CITIZEN_QUEUE);
    w.section(EE_UNITS_UNIT_SAVE_SECTION_CARGO);
    let mut r = w.section(EE_UNITS_UNIT_SAVE_SECTION_WAYPOINTS);
    if r != 0 {
        return r;
    }
    r = chain!(
        w.part(EE_UNITS_UNIT_SAVE_PART_PROJECTILE_START),
        w.part(EE_UNITS_UNIT_SAVE_PART_LOCATION),
        w.bool(flag!(cx, mIsFiringHigh)),
    );
    if r != 0 {
        return r;
    }
    let scale = w.query(EE_UNITS_UNIT_SAVE_FLOAT, EE_UNITS_UNIT_SAVE_FLOAT_SCALE).f;
    r = w.float(scale);
    if r != 0 {
        return r;
    }
    let frame = w.id(EE_UNITS_UNIT_SAVE_ID_FRAME);
    r = chain!(
        w.ulong(frame),
        w.part(EE_UNITS_UNIT_SAVE_PART_ANGULAR_VELOCITY),
        w.part(EE_UNITS_UNIT_SAVE_PART_ORIENTATION),
    );
    if r != 0 {
        return r;
    }
    let tile_x = w.query(EE_UNITS_UNIT_SAVE_COLLISION_TILE, 0).i;
    w.long(tile_x);
    let tile_y = w.query(EE_UNITS_UNIT_SAVE_COLLISION_TILE, 1).i;
    r = w.long(tile_y);
    if r != 0 {
        return r;
    }
    for id in [
        EE_UNITS_UNIT_SAVE_ID_LAUNCHER_CLASS,
        EE_UNITS_UNIT_SAVE_ID_PATROL_GROUP,
        EE_UNITS_UNIT_SAVE_ID_PREVIOUS_PART,
        EE_UNITS_UNIT_SAVE_ID_NEXT_PART,
        EE_UNITS_UNIT_SAVE_ID_HOME_BASE,
        EE_UNITS_UNIT_SAVE_ID_CONTAINER,
        EE_UNITS_UNIT_SAVE_ID_COMMAND,
    ] {
        let v = w.id(id);
        r = w.ulong(v);
        if r != 0 {
            return r;
        }
    }
    r = w.section(EE_UNITS_UNIT_SAVE_SECTION_TURRET_ANGLES);
    r = chain!(
        r,
        w.float(get!(cx, mHeight)),
        w.float(get!(cx, mVelocity)),
        w.float(get!(cx, mF220)),
        w.float(get!(cx, mF21C)),
        w.long(get!(cx, mCurrentResearch) as i64),
        w.ulong(get!(cx, mAnimation) as i64),
        w.ulong(get!(cx, mCurrentPhysicsAction) as i64),
        w.ulong(get!(cx, mCurrentPhysicsOrder) as i64),
        w.ulong(get!(cx, mFacing) as i64),
        w.float(get!(cx, mLaunchYaw)),
    );
    if r != 0 {
        return r;
    }
    let max_hp = w.max(EE_UNITS_UNIT_SAVE_MAX_HIT_POINTS);
    r = chain!(
        w.float(fraction(get!(cx, mLastHitPoints) as i64, max_hp)),
        w.long(get!(cx, mExitingUnitID) as i64),
        w.long(get!(cx, mCrewHitPoints) as i64),
        w.long(get!(cx, mProductionTime) as i64),
    );
    if r != 0 {
        return r;
    }
    let max_flight = w.max(EE_UNITS_UNIT_SAVE_MAX_FLIGHT_TIME);
    r = chain!(
        w.float(fraction(get!(cx, mFlightTime) as i64, max_flight)),
        w.long(get!(cx, mReloadTimer) as i64),
        w.long(get!(cx, mCargoRoom) as i64),
        w.long(get!(cx, mRottingAwayTime) as i64),
        w.long(get!(cx, mTargetSearchTimer) as i64),
        w.long(get!(cx, mWorkTimer) as i64),
        w.long(get!(cx, mRegenerationTimer) as i64),
        w.float(get!(cx, mHealSearchTimer)),
        w.float(get!(cx, mCollisionResolutionCount)),
        w.long(get!(cx, mWakeTimer) as i64),
        w.long(get!(cx, mTrailEffectTimer) as i64),
        w.ulong(get!(cx, mAmmunition) as i64),
        w.ulong(get!(cx, mAnimationTime) as i64),
        w.ulong(0),
        w.ulong(get!(cx, mWeaponTagPoint) as i64),
        w.ulong(get!(cx, mTimesUpdated) as i64),
        w.ulong(get!(cx, mConversionStartTime) as i64),
        w.ushort(get!(cx, mCollisionState)),
        w.bool(flag!(cx, mIsDisabled)),
        w.bool(flag!(cx, mIsDeletable)),
        w.bool(flag!(cx, mIsCommandable)),
        w.bool(flag!(cx, mIsAnimationPaused)),
        w.bool(flag!(cx, mHasArrived)),
        w.bool(flag!(cx, mIsPathDone)),
        w.bool(flag!(cx, mIsClearanceGranted)),
        w.bool(flag!(cx, mIsBuilt)),
        w.float(get!(cx, mOpacity)),
        w.bool(flag!(cx, mIsCloaked)),
    );
    if r != 0 {
        return r;
    }
    let v = w.id(EE_UNITS_UNIT_SAVE_ID_GATHERED_RESOURCE);
    r = w.ulong(v);
    if r != 0 {
        return r;
    }
    // theState = mNumAreaEffects: the unsigned long written as a long
    r = chain!(w.long(get!(cx, mNumAreaEffects) as i64), w.ulong(get!(cx, mU2FC) as i64));
    if r != 0 {
        return r;
    }
    for id in [EE_UNITS_UNIT_SAVE_ID_LAST_ATTACKER, EE_UNITS_UNIT_SAVE_ID_OWNER] {
        let v = w.id(id);
        r = w.ulong(v);
        if r != 0 {
            return r;
        }
    }
    let max_garrison = w.max(EE_UNITS_UNIT_SAVE_MAX_GARRISON);
    r = chain!(
        w.float(fraction(get!(cx, mGarrisonCount) as i64, max_garrison)),
        w.long(get!(cx, mGarrisonPopulation) as i64),
        w.bool(flag!(cx, mIsPaidFor)),
        w.long(get!(cx, mNumBuilders) as i64),
    );
    if r != 0 {
        return r;
    }
    let mine = w.id(EE_UNITS_UNIT_SAVE_ID_MINE);
    r = chain!(
        w.ulong(mine),
        w.part(EE_UNITS_UNIT_SAVE_PART_U0DC),
        w.bool(flag!(cx, mIsDetachedFromAI)),
        w.part(EE_UNITS_UNIT_SAVE_PART_COLOR),
        w.bool(flag!(cx, mIsTinted)),
        w.bool(flag!(cx, mIsLocked)),
        w.ulong(get!(cx, mGateDirection) as i64),
    );
    if r != 0 {
        return r;
    }
    // (the open fraction's result was not kept)
    w.float(get!(cx, mOpenFraction));
    0
}

/// From the growth to the foundation flag (the part a failed write leaves for `SaveFinalFields`).
fn save_to_final<H: SaveHost>(w: &mut W<'_, H>, cx: *const EeUnitsComplexUnitFields) {
    let r = chain!(
        w.float(get!(cx, mGrowth)),
        w.bool(flag!(cx, mB34F)),
        w.bool(flag!(cx, mIsRegrowing)),
        w.ulong(get!(cx, mNumAttackers) as i64),
        w.ulong(get!(cx, mGrowTime) as i64),
        w.ulong(get!(cx, mGrazeTime) as i64),
        w.part(EE_UNITS_UNIT_SAVE_PART_FIRE_EFFECT),
        w.part(EE_UNITS_UNIT_SAVE_PART_PATH),
        w.long(get!(cx, mPositiveMorale) as i64),
        w.long(get!(cx, mNegativeMorale) as i64),
    );
    if r != 0 {
        return;
    }
    // (float)mPower / mStats->mPower, 0 for a maximum under 1 (the power itself is not tested)
    let max_power = w.max(EE_UNITS_UNIT_SAVE_MAX_POWER);
    let power = if max_power < 1 { 0.0 } else { get!(cx, mPower) as i64 as f32 / max_power as f32 };
    let r = chain!(w.float(power), w.long(get!(cx, mPowerRegenTimer) as i64), w.bool(flag!(cx, mIsCloakable)));
    if r != 0 {
        return;
    }
    for index in [EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_X, EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_Y] {
        let v = w.query(EE_UNITS_UNIT_SAVE_FLOAT, index).f;
        if w.float(v) != 0 {
            return;
        }
    }
    let z = w.query(EE_UNITS_UNIT_SAVE_FLOAT, EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_Z).f;
    if chain!(w.float(z), w.float(get!(cx, mInitialContactHeading))) != 0 {
        return;
    }
    let behavior = w.id(EE_UNITS_UNIT_SAVE_ID_BEHAVIOR);
    if chain!(w.ulong(behavior), w.ushort(get!(cx, mExperience))) != 0 {
        return;
    }
    let last = w.id(EE_UNITS_UNIT_SAVE_ID_LAST_RESOURCE);
    if w.ulong(last) == 0 {
        w.bool(flag!(cx, mIsFoundation));
    }
}

/// The C ABI: `RTSUnit::Save` (`unit`, `complex` null), `EEUnit::Save` (`complex` null) or `EEComplexUnit::Save`
/// (all three) of one unit over the host callback.
///
/// # Safety
/// The record pointers name the live records of the unit the callback saves (null where noted); `inp` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_units_unit_save(
    ctx: *mut c_void,
    f: EeUnitsUnitSaveFn,
    rts: *const EeUnitsRtsUnitFields,
    unit: *const EeUnitsUnitFields,
    complex: *const EeUnitsComplexUnitFields,
    inp: *const EeUnitsUnitSaveIn,
) -> i32 {
    let mut host = CallbackHost { ctx, f };
    // SAFETY: the caller's contract
    let inp = unsafe { *inp };
    if unit.is_null() {
        // SAFETY: the caller's contract
        return unsafe { rts_save(&mut host, rts, &inp) };
    }
    // SAFETY: the caller's contract
    let r = unsafe { unit_save(&mut host, rts, unit, &inp) };
    if r != 0 || complex.is_null() {
        return r;
    }
    // SAFETY: the caller's contract
    unsafe { complex_save_rest(&mut host, complex, &inp) }
}

#[cfg(test)]
#[path = "unit_save_tests.rs"]
mod tests;
