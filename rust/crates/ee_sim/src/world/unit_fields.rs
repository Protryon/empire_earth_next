//! The unit classes' plain members as Rust records the C++ classes derive from (R5b.4, the twenty-sixth round):
//! `class RTSUnit : public UReferenceCounter, public EeUnitsRtsUnitFields` (ee/RTSUnit.h), `class EEUnit : public
//! RTSUnit, public EeUnitsUnitFields` and `class EEComplexUnit : public EEUnit, public EeUnitsComplexUnitFields`
//! (ee/EEUnit.h), as `EEPlayer` derives from [`crate::world::player_fields::EeUnitsPlayerSaved`]. The fields keep their C++
//! names, types (`c_long`/`c_ulong` are the C++ `long`/`unsigned long` on every data model) and order, so no reader
//! changed; `RTSUnit` stays the first base at offset 0 of every unit object.
//!
//! The unit store (ee_sim::units::store) and the views (view.rs) read these records directly: a store record holds the addresses of its object's
//! records ([`crate::units::store::EeUnitRecord`]'s `rts_fields`, `unit_fields`, `complex_fields`) and every read of the
//! store refreshes the mirrored values from them ([`crate::units::store::refresh`]), where the C++ used to report each
//! write through a holder.
//!
//! The records' `bool`s are read by the store as bytes (a byte other than 0 or 1, as a hand-edited save can leave
//! through `FSFile::Read(bool&)`, is a valid C++ `bool` read but not a valid Rust one).

#![allow(non_snake_case)]

use std::os::raw::{c_long, c_ulong};

/// `RTSUnit`'s plain members (ee/RTSUnit.h; RTSUnit.cpp's `Reset` sets them).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsRtsUnitFields {
    /// the unit's tile (-10000: off the map)
    pub mTileX: c_long,
    pub mTileY: c_long,
    /// the unit's id (0xffffffff: none; the owner in its top byte)
    pub mUnitID: c_ulong,
    /// saved
    pub mIsQueuedForDelete: bool,
    /// saved as false; Load clears it
    pub mIsEditorSelected: bool,
    /// true after the constructor
    pub mIsNodeFreed: bool,
    /// named by the asserts "this->mIsValid == true"
    pub mIsValid: bool,
}

/// `EEUnit`'s plain members (ee/EEUnit.h), then its class, complex unit and stats pointers (units round 10).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsUnitFields {
    /// saved
    pub mResourceType: c_ulong,
    /// > 0: alive (a load holds the saved fraction's float bits here until `FinishLoad`)
    pub mHitPoints: c_long,
    /// what a resource has left (saved)
    pub mResourceAmount: c_long,
    /// 0x10: `MoveTo` moved it; 0x20 and the low 2 bits: `UpdateHidden`
    pub mHiddenFlags: u8,
    /// +0x2c (the unit store reads it: EEUnitStore.h)
    pub mUnitClass: *mut EeCxxUnitClass,
    /// +0x30: the complex unit this unit belongs to (the calamities read its hit points, transformation and goals
    /// through it); the unit store reads it
    pub mComplexUnit: *mut EeCxxComplexUnit,
    /// +0x34 (provisional name)
    pub mStats: *mut EeCxxUnitClassUpgrade,
}

/// `EEComplexUnit`'s plain members (ee/EEUnit.h), in the class's order, then its pointer members (units round 10: the
/// C++ reads them by name, typed `class X*`; the layout's offsets of them are derived here, layout.rs); its effect
/// handles (`mConversionEffect`, `mHealEffect`, `mFireEffect`), points, containers and parts stay C++ members.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsComplexUnitFields {
    pub mHeight: f32,
    pub mInitialContactHeading: f32,
    pub mOpacity: f32,
    pub mF21C: f32,
    pub mF220: f32,
    pub mLaunchYaw: f32,
    pub mOpenFraction: f32,
    pub mHealSearchTimer: f32,
    pub mPushVelocityX: f32,
    pub mPushVelocityY: f32,
    pub mVelocity: f32,
    pub mAnimationFraction: f32,
    pub mGrowth: f32,
    pub mCollisionRadius: f32,
    pub mCollisionResolutionCount: f32,
    pub mF24C: f32,
    pub mSight: f32,
    pub mCalamityFlags: c_ulong,
    /// unsigned: AIStep compares it with the game time unsigned
    pub mNextTriggerTime: c_ulong,
    pub mCurrentResearch: c_long,
    pub mAnimation: c_ulong,
    pub mCurrentPhysicsAction: c_ulong,
    pub mCurrentPhysicsOrder: c_ulong,
    pub mFacing: c_ulong,
    pub mGateDirection: c_ulong,
    pub mNumBuilders: c_long,
    pub mCrewHitPoints: c_long,
    pub mProductionTime: c_long,
    pub mTrailEffectTimer: c_long,
    /// a load holds the saved fraction's float bits here until `FinishLoad`
    pub mGarrisonCount: c_long,
    pub mGarrisonPopulation: c_long,
    /// a load holds the saved fraction's float bits here until `FinishLoad`
    pub mFlightTime: c_long,
    pub mRegenerationTimer: c_long,
    pub mExitingUnitID: c_long,
    pub mFormationSlot: c_ulong,
    /// a load holds the saved fraction's float bits here until `FinishLoad`
    pub mLastHitPoints: c_long,
    pub mReloadTimer: c_long,
    pub mCargoRoom: c_long,
    pub mRottingAwayTime: c_long,
    pub mTargetSearchTimer: c_long,
    pub mNumAreaEffects: c_ulong,
    pub mWakeTimer: c_long,
    pub mWorkTimer: c_long,
    pub mPositiveMorale: c_long,
    pub mNegativeMorale: c_long,
    pub mPowerRegenTimer: c_long,
    /// a load holds the saved fraction's float bits here until `FinishLoad`
    pub mPower: c_long,
    pub mNumPathRequests: c_long,
    pub mIsCollidable: c_long,
    pub mFormationRank: c_long,
    pub mWaterState: c_long,
    pub mAmmunition: c_ulong,
    pub mWeaponTagPoint: c_ulong,
    pub mNumOrbitals: c_ulong,
    pub mNumWheels: c_ulong,
    pub mNumTurretAngles: c_long,
    pub mU2FC: c_ulong,
    pub mAnimationTime: c_ulong,
    pub mConversionStartTime: c_ulong,
    pub mTimesUpdated: c_ulong,
    pub mNextFrame: c_long,
    pub mGrazeTime: c_ulong,
    pub mGrowTime: c_ulong,
    pub mNumAttackers: c_ulong,
    /// the cloak's timer (the cloaking calamity sets it)
    pub mCloaked: c_ulong,
    /// a game time (compared unsigned)
    pub mPushImmuneUntil: c_ulong,
    pub mCamouflageTimer: c_ulong,
    pub mU32C: c_ulong,
    pub mAIState: c_long,
    pub mL334: c_long,
    pub mL338: c_long,
    pub mCollisionState: u16,
    pub mExperience: u16,
    pub mIsDeletable: bool,
    pub mIsCommandable: bool,
    pub mIsClearanceGranted: bool,
    pub mIsAnimationPaused: bool,
    pub mIsPaidFor: bool,
    pub mIsCloakable: bool,
    pub mIsCloaked: bool,
    pub mIsDisabled: bool,
    pub mIsLocked: bool,
    pub mIsTinted: bool,
    pub mIsPathDone: bool,
    pub mHasArrived: bool,
    pub mIsBuilt: bool,
    pub mIsDetachedFromAI: bool,
    pub mIsFiringHigh: bool,
    pub mB34F: bool,
    pub mIsRegrowing: bool,
    pub mIsFoundation: bool,
    pub mCanBeHidden: bool,
    pub mIsCitizen: bool,
    pub mIsPacked: bool,
    pub mIsSpecialAttack: bool,
    /// untouched by the constructor and Load/Save
    pub mU356: u16,
    pub mDisabledReason: c_ulong,
    pub mPlanetID: u16,
    pub mU35E: u16,
    pub mRoll: f32,
    /// port: a human shift-queued a build or repair (EEMBuildUnit, EEMRepairUnit)
    pub mHasShiftQueuedBuild: bool,
    /// +0x1b8 (+0x1b8..+0x20c: zeroed by 0x1fac37; mAreaEffectTemplate0 copies of the model's orbitals:
    /// Virtual64/Virtual30)
    pub mOrbitals: *mut *mut EeCxxOrbital,
    /// +0x1bc (mAreaEffectTemplate1 copies of the model's wheels: Virtual64/Virtual30)
    pub mWheels: *mut *mut EeCxxWheel,
    /// +0x1c0 provisional name (drop object: the dropped class)
    pub mLauncherClass: *mut EeCxxUnitClass,
    /// +0x1c8 provisional name (0x5e407 and the teleport calamity call into it; the unit store reads it)
    pub mActiveGroup: *mut EeCxxActiveGroup,
    /// +0x1cc: the unit carrying this one (non-zero: not selectable by the aircraft hot key; the fire and plague
    /// functors walk the chain to the outermost carrier)
    pub mPreviousPart: *mut EeCxxComplexUnit,
    /// +0x1d0 the next unit of a linked group (EEDoppelgangerManager 0x20278c walks it)
    pub mNextPart: *mut EeCxxComplexUnit,
    /// +0x1d4
    pub mHomeBase: *mut EeCxxComplexUnit,
    /// +0x1d8 provisional name (aircraft in an airport: its position is the holder's; the unit store reads it)
    pub mContainer: *mut EeCxxComplexUnit,
    /// +0x1dc (released by Virtual04)
    pub mLastAttacker: *mut EeCxxComplexUnit,
    /// +0x1e0 provisional name (SetOwner; referenced)
    pub mOwner: *mut EeCxxComplexUnit,
    /// +0x1e4 (Virtual04: mRegeneration->ReleaseHeld(this))
    pub mHolder: *mut EeCxxComplexUnit,
    /// +0x1e8 the unit's current script action (ee/EEUnitAction.h; the unit store reads it)
    pub mCommand: *mut EeCxxUnitAction,
    /// +0x1ec: a goal (EECPASGUHero::CheckCondition tests its mKind == 0xb; EECPASGUProphet::CanStillCast casts it
    /// to EEUnitGoal)
    pub mActiveGoal: *mut EeCxxUnitGoal,
    /// +0x1f0: the unit's goal (EEMAddPatrolPoint casts it to EEUGPatrol; mKind 0xb: not to be cloaked); the unit
    /// store reads it
    pub mGoalState: *mut EeCxxUnitGoal,
    /// +0x1f4
    pub mLastResource: *mut EeCxxRtsUnit,
    /// +0x1f8 (SetGatheredResource: referenced, its +0x2b tested)
    pub mGatheredResource: *mut EeCxxUnit,
    /// +0x1fc (a unit: the mining building tests its +0x2b/+0x40; provisional name)
    pub mMine: *mut EeCxxUnit,
    /// +0x200 the unit's herd (EEHerd AddUnit 0xa1b45 sets it, RemoveUnit 0xa3749 clears it; the unit store reads
    /// it)
    pub mHerd: *mut EeCxxHerd,
    /// +0x204 (ResizeTurretAngles: delete[], new float[n] zeroed)
    pub mTurretAngles: *mut f32,
    /// +0x208 the unit's behaviour (EEUnitAI 0x22d9b5 reads its mB09; EEUIFMGMainGroup compares it with
    /// EEUnitBehavior::sTable[i])
    pub mBehavior: *mut EeCxxUnitBehavior,
    /// +0x20c
    pub mBlockingUnit: *mut EeCxxRtsUnit,
    /// +0x25c
    pub mSound: *mut EeCxxSoundInstance,
}

/// A `bool` member's byte, read without making a Rust `bool` of it (any non-zero byte is true, as C++ tests it).
///
/// # Safety
/// `field` must point to a readable byte.
pub(crate) unsafe fn bool_byte(field: *const bool) -> u8 {
    // SAFETY: the caller's contract; a byte read never makes an invalid value
    u8::from(unsafe { std::ptr::read_volatile(field.cast::<u8>()) } != 0)
}

impl Default for EeUnitsUnitFields {
    fn default() -> Self {
        // SAFETY: every field is a number, a bool or a raw pointer: all zero is each one's default
        unsafe { std::mem::zeroed() }
    }
}

// SAFETY: the pointers are the C++ object's members, read and written only on the thread that owns the object
unsafe impl Send for EeUnitsUnitFields {}
unsafe impl Sync for EeUnitsUnitFields {}

impl Default for EeUnitsComplexUnitFields {
    fn default() -> Self {
        // SAFETY: every field is a number, a bool or a raw pointer: all zero is each one's default
        unsafe { std::mem::zeroed() }
    }
}

// SAFETY: the pointers are the C++ object's members, read and written only on the thread that owns the object
unsafe impl Send for EeUnitsComplexUnitFields {}
unsafe impl Sync for EeUnitsComplexUnitFields {}

/// The C++ classes the records' pointer members point to: opaque to Rust, written `class X` in the generated
/// header (rust/cbindgen.toml's renames; they are excluded, so no `struct X;` is emitted: MSVC's mangling follows
/// the tag of a class's last declaration).
pub struct EeCxxUnitClass {
    _opaque: [u8; 0],
}
pub struct EeCxxComplexUnit {
    _opaque: [u8; 0],
}
pub struct EeCxxUnitClassUpgrade {
    _opaque: [u8; 0],
}
pub struct EeCxxOrbital {
    _opaque: [u8; 0],
}
pub struct EeCxxWheel {
    _opaque: [u8; 0],
}
pub struct EeCxxActiveGroup {
    _opaque: [u8; 0],
}
pub struct EeCxxUnitAction {
    _opaque: [u8; 0],
}
pub struct EeCxxUnitGoal {
    _opaque: [u8; 0],
}
pub struct EeCxxRtsUnit {
    _opaque: [u8; 0],
}
pub struct EeCxxUnit {
    _opaque: [u8; 0],
}
pub struct EeCxxHerd {
    _opaque: [u8; 0],
}
pub struct EeCxxUnitBehavior {
    _opaque: [u8; 0],
}
pub struct EeCxxSoundInstance {
    _opaque: [u8; 0],
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_records_have_the_c_layout_of_their_members() {
        let long = std::mem::size_of::<c_long>();
        // RTSUnit: two longs, an unsigned long, four bools
        assert_eq!(std::mem::size_of::<EeUnitsRtsUnitFields>(), (3 * long + 4).next_multiple_of(long));
        let f = EeUnitsRtsUnitFields::default();
        let base = std::ptr::addr_of!(f) as usize;
        assert_eq!(std::ptr::addr_of!(f.mIsValid) as usize - base, 3 * long + 3);
        // EEComplexUnit: the floats first, in order
        let c = EeUnitsComplexUnitFields::default();
        let base = std::ptr::addr_of!(c) as usize;
        assert_eq!(std::ptr::addr_of!(c.mHeight) as usize - base, 0);
        assert_eq!(std::ptr::addr_of!(c.mSight) as usize - base, 16 * 4);
        assert!(std::ptr::addr_of!(c.mRoll) as usize > std::ptr::addr_of!(c.mPlanetID) as usize);
    }

    #[test]
    fn a_bool_member_reads_as_a_byte() {
        let mut bytes = [0u8, 1, 2, 0xff];
        let p = bytes.as_mut_ptr().cast::<bool>();
        // SAFETY: four readable bytes
        let read: Vec<u8> = (0..4).map(|i| unsafe { bool_byte(p.add(i)) }).collect();
        assert_eq!(read, [0, 1, 1, 1]);
    }

    #[test]
    fn the_pointer_members_default_to_null_and_their_layout_offsets_come_from_the_records() {
        let (u, c) = (EeUnitsUnitFields::default(), EeUnitsComplexUnitFields::default());
        assert!(u.mUnitClass.is_null() && u.mStats.is_null() && c.mOwner.is_null() && c.mSound.is_null());
        let mut l =
            crate::world::layout::EeWorldLayout { unit_fields: 0x40, complex_unit_fields: 0x100, ..Default::default() };
        crate::world::layout::derive_record_members(&mut l);
        assert_eq!(l.unit_stats, 0x40 + std::mem::offset_of!(EeUnitsUnitFields, mStats) as u64);
        assert_eq!(l.unit_class, 0x40 + std::mem::offset_of!(EeUnitsUnitFields, mUnitClass) as u64);
        assert_eq!(l.complex_owner, 0x100 + std::mem::offset_of!(EeUnitsComplexUnitFields, mOwner) as u64);
        assert_eq!(l.complex_herd, 0x100 + std::mem::offset_of!(EeUnitsComplexUnitFields, mHerd) as u64);
    }
}
