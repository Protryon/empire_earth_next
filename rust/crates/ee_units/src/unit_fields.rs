//! The unit classes' plain members as Rust records the C++ classes derive from (R5b.4, the twenty-sixth round):
//! `class RTSUnit : public UReferenceCounter, public EeUnitsRtsUnitFields` (ee/RTSUnit.h), `class EEUnit : public
//! RTSUnit, public EeUnitsUnitFields` and `class EEComplexUnit : public EEUnit, public EeUnitsComplexUnitFields`
//! (ee/EEUnit.h), as `EEPlayer` derives from [`crate::player_save::EeUnitsPlayerSaved`]. The fields keep their C++
//! names, types (`c_long`/`c_ulong` are the C++ `long`/`unsigned long` on every data model) and order, so no reader
//! changed; `RTSUnit` stays the first base at offset 0 of every unit object.
//!
//! The unit store (store.rs) reads these records directly: a store record holds the addresses of its object's
//! records ([`crate::store::EeUnitRecord`]'s `rts_fields`, `unit_fields`, `complex_fields`) and every read of the
//! store refreshes the mirrored values from them ([`crate::store::refresh`]), where the C++ used to report each
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

/// `EEUnit`'s plain members (ee/EEUnit.h).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsUnitFields {
    /// saved
    pub mResourceType: c_ulong,
    /// > 0: alive (a load holds the saved fraction's float bits here until `FinishLoad`)
    pub mHitPoints: c_long,
    /// what a resource has left (saved)
    pub mResourceAmount: c_long,
    /// 0x10: `MoveTo` moved it; 0x20 and the low 2 bits: `UpdateHidden`
    pub mHiddenFlags: u8,
}

/// `EEComplexUnit`'s plain members (ee/EEUnit.h), in the class's order; its pointers (`mSound`), effect handles
/// (`mConversionEffect`, `mHealEffect`, `mFireEffect`), points, containers and parts stay C++ members.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
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
}

/// A `bool` member's byte, read without making a Rust `bool` of it (any non-zero byte is true, as C++ tests it).
///
/// # Safety
/// `field` must point to a readable byte.
pub(crate) unsafe fn bool_byte(field: *const bool) -> u8 {
    // SAFETY: the caller's contract; a byte read never makes an invalid value
    u8::from(unsafe { std::ptr::read_volatile(field.cast::<u8>()) } != 0)
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
}
