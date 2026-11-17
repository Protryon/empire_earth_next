//! The complex unit's whole-record operations (R5b.4, the thirty-first round): `EEComplexUnit::ResetEEComplexUnit`'s
//! defaults (0x5fac37), `CopyFrom`'s copies and scaled fractions (0x5f713d) and `OutOfSynchDump`'s lines (0x5f908d),
//! over the Rust record the class derives from ([`EeUnitsComplexUnitFields`]). The members that are not in the
//! record (pointers, containers, the physical model, the colour) stay the C++'s, which calls these at the point of
//! the original statements they replace; no call between them reads or writes the record.

use std::fmt::Write as _;
use std::os::raw::{c_char, c_long, c_ulong, c_void};

use crate::stats::EeUnitsUnitClassUpgradeFields;
use crate::unit_class_fields::EeUnitsUnitClassFields;
use crate::unit_fields::{bool_byte, EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

/// `ResetEEComplexUnit`'s heading of no initial contact (0x446db4).
pub const EE_UNITS_NO_HEADING: f32 = -10000.0;
/// `ResetEEComplexUnit`'s `mAIState`.
const RESET_AI_STATE: c_long = 300_000;
/// `kTechNone` (ee/EETechTreeTechID.h).
const EE_TECH_NONE: c_long = 100_000;

/// The values `ResetEEComplexUnit` reads off the world and the unit's class and stats (each as the original
/// computed it: the class's for a unit with a class, the stats' for one with a class and a player).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsComplexResetIn {
    /// `mF220`: a random heading (`gPhysics->mRandom.Generate() * 6.2831855f`, drawn by the C++) or 6.2831855f
    pub heading: f32,
    /// `GetStats()->mReloadTime`, or 0
    pub reload_time: c_long,
    /// `GetStats()->mPowerRegeneration`, or 0
    pub power_regeneration: c_long,
    /// `GetUnitClass()->mInitialGarrisonCount`, or 0
    pub garrison_count: c_long,
    /// `GetUnitClass()->mIsCloakable` (false without a class)
    pub cloakable: bool,
    /// `!GetUnitClass()->IsLandBuilding()` (false without a class)
    pub paid_for: bool,
}

/// `ResetEEComplexUnit`'s record members: every one but `mU356`, `mDisabledReason` and `mU35E`, which it leaves.
pub fn reset(cx: &mut EeUnitsComplexUnitFields, input: &EeUnitsComplexResetIn) {
    *cx = EeUnitsComplexUnitFields {
        mU356: cx.mU356,
        mDisabledReason: cx.mDisabledReason,
        mU35E: cx.mU35E,
        mInitialContactHeading: EE_UNITS_NO_HEADING,
        mIsCollidable: 1,
        mF220: input.heading,
        mNextTriggerTime: c_ulong::MAX,
        mExitingUnitID: -1,
        mReloadTimer: input.reload_time,
        mPowerRegenTimer: input.power_regeneration,
        mIsCommandable: true,
        mIsPathDone: true,
        mIsBuilt: true,
        mAnimation: 7,
        mIsCloakable: input.cloakable,
        mFormationSlot: 1,
        mGarrisonCount: input.garrison_count,
        mIsDeletable: true,
        mIsPaidFor: input.paid_for,
        mCurrentResearch: EE_TECH_NONE,
        mCurrentPhysicsAction: 6,
        mCurrentPhysicsOrder: 5,
        mAIState: RESET_AI_STATE,
        mGrowth: 1.0,
        ..EeUnitsComplexUnitFields::default()
    };
}

/// The maxima `CopyFrom` scales by: the replaced unit's (`other_*`) and this unit's (`GetUpgrade(type)->m*`), and
/// whether the camouflage power lets the cloakable flag come over (both classes in its unit set).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsComplexCopyIn {
    pub other_max_hit_points: c_long,
    pub max_hit_points: c_long,
    pub other_power: c_long,
    pub power: c_long,
    pub other_flight_time: c_long,
    pub flight_time: c_long,
    pub other_reload_time: c_long,
    pub reload_time: c_long,
    pub copy_cloakable: bool,
}

/// `(float)value / maximum` as the original divided (a `long` over a `long` maximum converted to float).
#[allow(clippy::cast_precision_loss)]
fn fraction(value: c_long, maximum: c_long) -> f32 {
    value as f32 / maximum as f32
}

/// `UMFloatToLong(maximum * fraction)`.
#[allow(clippy::cast_precision_loss)]
fn scaled(maximum: c_long, fraction: f32) -> c_long {
    c_long::from(ee_render::ge::um::lrintf(maximum as f32 * fraction))
}

/// `CopyFrom`'s first block (0x5f713d, after the sound): the timers and plain state, the hit points, power, flight
/// time and reload timer scaled to this unit's maxima, the flags, and the camouflage timers (the cloakable flag only
/// when `copy_cloakable`), in the original's order.
pub fn copy_first(cx: &mut EeUnitsComplexUnitFields, other: &EeUnitsComplexUnitFields, input: &EeUnitsComplexCopyIn) {
    cx.mVelocity = other.mVelocity;
    cx.mF220 = other.mF220;
    cx.mF21C = other.mF21C;
    cx.mOpacity = other.mOpacity;
    cx.mFacing = other.mFacing;
    cx.mCrewHitPoints = other.mCrewHitPoints;
    cx.mRottingAwayTime = other.mRottingAwayTime;
    cx.mTargetSearchTimer = other.mTargetSearchTimer;
    cx.mWorkTimer = other.mWorkTimer;
    cx.mRegenerationTimer = other.mRegenerationTimer;
    cx.mHealSearchTimer = other.mHealSearchTimer;
    cx.mWakeTimer = other.mWakeTimer;
    cx.mTrailEffectTimer = other.mTrailEffectTimer;
    cx.mGateDirection = other.mGateDirection;
    cx.mOpenFraction = other.mOpenFraction;
    cx.mExperience = other.mExperience;
    cx.mNumAttackers = other.mNumAttackers;
    let f = fraction(other.mLastHitPoints, input.other_max_hit_points);
    cx.mLastHitPoints = scaled(input.max_hit_points, f);
    #[allow(clippy::cast_precision_loss)]
    let maximum = input.other_power as f32;
    let f = if maximum > 0.0 {
        #[allow(clippy::cast_precision_loss)]
        let power = other.mPower as f32;
        power / maximum
    } else {
        0.0
    };
    cx.mPower = scaled(input.power, f);
    cx.mPowerRegenTimer = other.mPowerRegenTimer;
    let f = fraction(other.mFlightTime, input.other_flight_time);
    cx.mFlightTime = scaled(input.flight_time, f);
    let f = fraction(other.mReloadTimer, input.other_reload_time);
    cx.mReloadTimer = scaled(input.reload_time, f);
    cx.mAmmunition = other.mAmmunition;
    cx.mWeaponTagPoint = 0;
    cx.mIsCommandable = other.mIsCommandable;
    cx.mIsClearanceGranted = other.mIsClearanceGranted;
    cx.mIsBuilt = other.mIsBuilt;
    cx.mIsCloaked = other.mIsCloaked;
    cx.mConversionStartTime = other.mConversionStartTime;
    cx.mCloaked = other.mCloaked;
    cx.mLaunchYaw = other.mLaunchYaw;
    cx.mIsTinted = other.mIsTinted;
    cx.mIsLocked = other.mIsLocked;
    cx.mIsPacked = other.mIsPacked;
    cx.mIsSpecialAttack = other.mIsSpecialAttack;
    if input.copy_cloakable {
        cx.mIsCloakable = other.mIsCloakable;
    }
    cx.mCamouflageTimer = other.mCamouflageTimer;
    cx.mU32C = other.mU32C;
}

/// `CopyFrom`'s second block (after `UpdateCamouflage`): the path requests, flags, physics action and order, growth
/// and calamities; the area effect counts start again.
pub fn copy_second(cx: &mut EeUnitsComplexUnitFields, other: &EeUnitsComplexUnitFields) {
    cx.mIsFiringHigh = false;
    cx.mNumPathRequests = other.mNumPathRequests;
    cx.mIsDisabled = other.mIsDisabled;
    cx.mIsDeletable = other.mIsDeletable;
    cx.mIsPaidFor = other.mIsPaidFor;
    cx.mCurrentPhysicsAction = other.mCurrentPhysicsAction;
    cx.mCurrentPhysicsOrder = other.mCurrentPhysicsOrder;
    cx.mGrowth = other.mGrowth;
    cx.mB34F = other.mB34F;
    cx.mIsRegrowing = other.mIsRegrowing;
    cx.mCalamityFlags = other.mCalamityFlags;
    cx.mNumAreaEffects = 0;
    cx.mU2FC = 0;
}

/// `CopyFrom`'s last block (after the goals): the planet, production time and the AI's state.
pub fn copy_third(cx: &mut EeUnitsComplexUnitFields, other: &EeUnitsComplexUnitFields) {
    cx.mPlanetID = other.mPlanetID;
    cx.mProductionTime = other.mProductionTime;
    cx.mAIState = other.mAIState;
    cx.mL334 = other.mL334;
    cx.mL338 = other.mL338;
    cx.mNumBuilders = other.mNumBuilders;
    cx.mIsDetachedFromAI = other.mIsDetachedFromAI;
}

/// The values of `OutOfSynchDump` that are not in the records.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsComplexDumpIn {
    pub player_id: c_ulong,
    pub tile_x: c_long,
    pub tile_y: c_long,
    /// `mAngularVelocity`'s yaw, pitch and roll
    pub angular_velocity: [f32; 3],
    pub build_queue: usize,
    pub citizen_build_queue: usize,
    pub collision_tile: [c_long; 2],
    pub frame: c_long,
    pub projectile_start: [f32; 3],
    /// `mPhysicalModel.GetLocation()`'s matrix, by row
    pub location: [[f32; 4]; 4],
}

/// One `%6.6f` (the dump's floats, printed as doubles).
fn f6(value: f32) -> String {
    format!("{:6.6}", f64::from(value))
}

/// The lines of `OutOfSynchDump` from the unit's id to its hit points (part 0) or from the physics action to the
/// planet (part 1), each as the original's `fprintf`.
#[allow(clippy::too_many_lines)]
pub fn dump(
    part: u32,
    rts: &EeUnitsRtsUnitFields,
    unit: &EeUnitsUnitFields,
    cx: &EeUnitsComplexUnitFields,
    class: &EeUnitsUnitClassFields,
    stats: &EeUnitsUnitClassUpgradeFields,
    d: &EeUnitsComplexDumpIn,
) -> String {
    const P: &str = "OUT OF SYNCH DUMP:  ";
    let mut s = String::new();
    // SAFETY (the bools): the records are live; a member read as a byte
    let b = |field: &bool| unsafe { bool_byte(field) };
    if part == 0 {
        s.push('\n');
        let _ = writeln!(s, "{P}**************  Unit ID: {:08x}  *************", rts.mUnitID);
        let _ = writeln!(s, "{P}Times Updated:      {}", cx.mTimesUpdated);
        let _ = writeln!(s, "{P}Owner:              {:2}", d.player_id);
        let _ = writeln!(s, "{P}Object Type:        {:4}", class.mUnitType);
        let _ = writeln!(s, "{P}Tile (x):           {:4}", d.tile_x);
        let _ = writeln!(s, "{P}Tile (y):           {:4}", d.tile_y);
        let _ = writeln!(s, "{P}Hit Points:         {:6}", unit.mHitPoints);
        return s;
    }
    let lines: [(&str, String); 22] = [
        ("CurrentPhysicsAction: ", format!("{:2}", cx.mCurrentPhysicsAction)),
        ("CurrentPhysicsOrder:  ", format!("{:2}", cx.mCurrentPhysicsOrder)),
        ("Queued for delete:  ", format!("{:2}", b(&rts.mIsQueuedForDelete))),
        ("Ammunition:         ", format!("{:6}", cx.mAmmunition)),
        ("Animation:          ", format!("{:6}", cx.mAnimation)),
        ("Yaw Velocity:       ", f6(d.angular_velocity[0])),
        ("Pitch Velocity:     ", f6(d.angular_velocity[1])),
        ("Roll Velocity:      ", f6(d.angular_velocity[2])),
        ("Blg Queue Count:    ", format!("{:6}", d.build_queue)),
        ("Cit Build Que Size: ", format!("{:6}", d.citizen_build_queue)),
        ("Clearance granted:  ", format!("{:2}", b(&cx.mIsClearanceGranted))),
        ("Coll Resolut Cnt:   ", f6(cx.mCollisionResolutionCount)),
        ("Collision State:    ", format!("{:6}", cx.mCollisionState)),
        ("Collision Tile (x): ", format!("{:6}", d.collision_tile[0])),
        ("Collision Tile (y): ", format!("{:6}", d.collision_tile[1])),
        ("Crew Hit Points:    ", format!("{:6}", cx.mCrewHitPoints)),
        ("Current Frame:      ", format!("{:6}", d.frame)),
        ("Flight Time:        ", format!("{:6}", cx.mFlightTime)),
        ("ProjectileStart X:  ", f6(d.projectile_start[0])),
        ("ProjectileStart Y:  ", f6(d.projectile_start[1])),
        ("ProjectileStart Z:  ", f6(d.projectile_start[2])),
        ("Rotting Away Time:  ", format!("{:6}", cx.mRottingAwayTime)),
    ];
    for (label, value) in &lines {
        let _ = writeln!(s, "{P}{label}{value}");
    }
    let lines: [(&str, String); 36] = [
        ("Velocity:           ", f6(cx.mVelocity)),
        ("Affected by Grav:   ", format!("{:2}", b(&class.mIsAffectedByGravity))),
        ("Ammunition:         ", format!("{:6}", class.mAmmunition)),
        ("Ammo Type:          ", format!("{:6}", class.mAmmoType)),
        ("Base Accel:         ", f6(class.mAcceleration)),
        ("Ang Accel:\t\t\t", f6(stats.mAngularAcceleration)),
        ("Base Area Effect:   ", f6(class.mAreaEffect)),
        ("Base Attack         ", format!("{:6}", class.mAttack)),
        ("Base Build On Elev: ", format!("{:2}", b(&class.mBuildsOnElevation))),
        ("Base Convert Blg:   ", format!("{:2}", b(&class.mConvertsBuildings))),
        ("Base Conver Priest: ", format!("{:2}", b(&class.mConvertsPriests))),
        ("Base Carry Ammt:    ", format!("{:6}", class.mCarryAmount)),
        ("Base Flight Time:   ", format!("{:6}", class.mFlightTime)),
        ("Base Healing Pts:   ", format!("{:6}", class.mHealingPoints)),
        ("Base Hit Points:    ", format!("{:6}", class.mHitPoints)),
        ("LOS:\t\t\t\t", f6(stats.mLineOfSight)),
        ("Max Ang Vel:\t\t", f6(stats.mMaxAngularVelocity)),
        ("Base Max Obj Carry: ", format!("{:6}", class.mMaxObjectCarry)),
        ("Max Vel:\t\t\t", f6(stats.mMaxVelocity)),
        ("Base Range:         ", f6(class.mRange)),
        ("Base Reload Time:   ", format!("{:6}", class.mReloadTime)),
        ("Base Repair Rate:   ", format!("{:6}", class.mRepairRate)),
        ("Base Turret MaxAV:  ", f6(class.mTurretMaxAngularVelocity)),
        ("Base Work Rate:     ", format!("{:6}", class.mWorkRate)),
        ("Fire While Moving:  ", format!("{:2}", b(&class.mFiresWhileMoving))),
        ("Combat Reaction:    ", format!("{:6}", class.mCombatReaction)),
        ("Cnvert % per sec:   ", f6(stats.mConversionRate)),
        ("Cruise Altitude:    ", f6(cx.mHeight)),
        ("Inner Coll Sphere:  ", f6(class.mInnerCollisionSphere)),
        ("Min Range:          ", f6(class.mMinRange)),
        ("Missle Delay Time:  ", format!("{:6}", class.mMissileDelayTime)),
        ("Outer Coll Sphere:  ", f6(class.mOuterCollisionSphere)),
        ("Shoots High:        ", format!("{:2}", b(&class.mShootsHigh))),
        ("Model Location (x): ", f6(d.location[0][3])),
        ("Model Location (y): ", f6(d.location[1][3])),
        ("Model Location (z): ", f6(d.location[2][3])),
    ];
    for (label, value) in &lines {
        let _ = writeln!(s, "{P}{label}{value}");
    }
    for (r, row) in d.location.iter().enumerate() {
        for (c, value) in row.iter().enumerate() {
            let _ = writeln!(s, "{P}Trans ({r},{c}):        {}", f6(*value));
        }
    }
    let _ = writeln!(s, "{P}Is Cloaked?:\t\t{:2}", b(&cx.mIsCloaked));
    let _ = writeln!(s, "OUT OF SYNCH DUMP:\tPlanet ID:          {:6}", cx.mPlanetID);
    s
}

/// `ResetEEComplexUnit`'s record members.
///
/// # Safety
/// `cx` is the unit's live record; `input` readable.
#[no_mangle]
pub unsafe extern "C" fn ee_units_complex_reset(
    cx: *mut EeUnitsComplexUnitFields,
    input: *const EeUnitsComplexResetIn,
) {
    // SAFETY: the caller's contract; no host call while the references live
    unsafe { reset(&mut *cx, &*input) };
}

/// `CopyFrom`'s record copies: `block` 0 [`copy_first`] (with `input`), 1 [`copy_second`], 2 [`copy_third`].
///
/// # Safety
/// `cx` and `other` are two live units' records (not the same); `input` readable for block 0.
#[no_mangle]
pub unsafe extern "C" fn ee_units_complex_copy(
    block: u32,
    cx: *mut EeUnitsComplexUnitFields,
    other: *const EeUnitsComplexUnitFields,
    input: *const EeUnitsComplexCopyIn,
) {
    // SAFETY: the caller's contract; the copies are made from a value of the other record
    let other = unsafe { *other };
    let cx = unsafe { &mut *cx };
    match block {
        0 => copy_first(cx, &other, unsafe { &*input }),
        1 => copy_second(cx, &other),
        _ => copy_third(cx, &other),
    }
}

/// The dump's write: `ctx` and the text's bytes (no terminator).
pub type EeUnitsDumpWriteFn = unsafe extern "C" fn(ctx: *mut c_void, text: *const c_char, len: usize);

/// `OutOfSynchDump`'s lines of `part` (see [`dump`]) written through `write`.
///
/// # Safety
/// The records are the unit's, its class's and its stats' live records; `d` readable; `write` callable with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_complex_dump(
    part: u32,
    rts: *const EeUnitsRtsUnitFields,
    unit: *const EeUnitsUnitFields,
    cx: *const EeUnitsComplexUnitFields,
    class: *const EeUnitsUnitClassFields,
    stats: *const EeUnitsUnitClassUpgradeFields,
    d: *const EeUnitsComplexDumpIn,
    write: EeUnitsDumpWriteFn,
    ctx: *mut c_void,
) {
    // SAFETY: the caller's contract; the text is made before the write is called
    let text = unsafe { dump(part, &*rts, &*unit, &*cx, &*class, &*stats, &*d) };
    // SAFETY: the caller's contract
    unsafe { write(ctx, text.as_ptr().cast(), text.len()) };
}

#[cfg(test)]
#[path = "unit_bulk_tests.rs"]
mod tests;
