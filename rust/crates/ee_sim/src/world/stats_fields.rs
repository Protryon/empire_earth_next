//! `EEUnitClassUpgrade`'s plain members, a player's upgrade of a unit class (`mStats` of every unit of that class and
//! player), as a Rust record the C++ class derives from (R5b.4, the twenty-eighth round: `class EEUnitClassUpgrade :
//! public EeUnitsUnitClassUpgradeFields`, ee/EEUnit.h). The fields keep their C++ names and types and their order
//! (+0x84 `mAreaEffectScale` .. +0x168 `mIsInitialized` in the original); the two volatile busy flags stay C++
//! members (a Rust field cannot be volatile). `Init` and the readers are ee_sim::units::stats'.

#![allow(non_snake_case)]

use crate::units::obj_fields::EeCxxGraphicObject;
use crate::world::unit_class_fields::{EeCxxButtonObject, EeCxxSoundObject};
use crate::world::unit_fields::EeCxxUnitClass;
use std::os::raw::{c_long, c_ulong};

/// `EEUnitClassUpgrade`'s plain members (ee/EEUnit.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsUnitClassUpgradeFields {
    pub mAreaEffectScale: f32,
    pub mConversionResistance: f32,
    pub mConversionRate: f32,
    pub mCostScale: f32,
    pub mBuildTimeScale: f32,
    pub mScale: f32,
    pub mRange: f32,
    pub mAreaEffect: f32,
    pub mLineOfSight: f32,
    pub mAcceleration: f32,
    pub mMaxVelocity: f32,
    pub mAngularAcceleration: f32,
    pub mMaxAngularVelocity: f32,
    pub mTurretMaxAngularVelocity: f32,
    /// `EEComplexUnit::SetVelocity` keeps `mF21C` at least this (`Init`: the smaller of acceleration and top speed)
    pub mBC: f32,
    /// the projectile's launch speed (`SetRange`)
    pub mLaunchSpeed: f32,
    pub mInverseAcceleration: f32,
    pub mInverseAngularAcceleration: f32,
    pub mInverseMaxAngularVelocity: f32,
    pub mInverseMaxVelocity: f32,
    pub mInverseTurretMaxAngularVelocity: f32,
    pub mAttack: f32,
    pub mLeadership: f32,
    /// per weapon kind
    pub mArmor: [f32; 6],
    pub mCameraFocus: f32,
    pub mAreaEffectTemplates: [c_long; 4],
    pub mWonderUnitSet: c_long,
    pub mGatherTimes: [c_long; 7],
    pub mPowerRegeneration: c_long,
    pub mFlightTime: c_long,
    pub mWorkRate: c_long,
    pub mReloadTime: c_long,
    pub mMaxObjectCarry: c_long,
    pub mRepairRate: c_long,
    pub mMaxHitPoints: c_long,
    pub mPower: c_long,
    pub mCarryAmount: c_long,
    pub mHealingPoints: c_long,
    /// the media loads (`EEPlayer::LoadUnitClassMedia`; signed: the `> 0` tests)
    pub mMediaCount: c_long,
    pub mButtonLoadCount: c_long,
    pub mRegeneration: c_long,
    /// the terrains the unit crosses, a bit per terrain type (0x4000: forest)
    pub mTerrainMask: c_ulong,
    pub mCanFireOverWalls: bool,
    pub mBuildsOnElevation: bool,
    pub mConvertsBuildings: bool,
    pub mConvertsPriests: bool,
    pub mIsInitialized: bool,
}

/// `EEUnitClassUpgrade`'s pointer members (units round 11; were C++ members of ee/EEUnit.h), its first base before
/// [`EeUnitsUnitClassUpgradeFields`]: the class it upgrades, then the eight media members `Media()` walks as one
/// array from `mButton` (button, model, creation and death sounds, two acknowledge and two selection sounds).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsUnitClassUpgradeLinks {
    /// the constructor's argument (`Init` reads the class)
    pub mUnitClass: *mut EeCxxUnitClass,
    /// the unit's portrait button
    pub mButton: *mut EeCxxButtonObject,
    /// `EEUnit::GetModel`'s
    pub mModel: *mut EeCxxGraphicObject,
    pub mCreationSound: *mut EeCxxSoundObject,
    /// `EEUnitClass::UnitDied` plays it where the unit dies
    pub mDeathSound: *mut EeCxxSoundObject,
    pub mAcknowledgeSounds: [*mut EeCxxSoundObject; 2],
    pub mSelectionSounds: [*mut EeCxxSoundObject; 2],
}

impl Default for EeUnitsUnitClassUpgradeLinks {
    fn default() -> Self {
        // SAFETY: every field is a raw pointer: all zero is each one's default
        unsafe { std::mem::zeroed() }
    }
}

// SAFETY: the pointers are the C++ object's members, read and written only on the simulation thread
unsafe impl Send for EeUnitsUnitClassUpgradeLinks {}
unsafe impl Sync for EeUnitsUnitClassUpgradeLinks {}

#[cfg(test)]
mod links_tests {
    use super::*;

    #[test]
    fn the_eight_media_members_are_one_array() {
        let p = std::mem::size_of::<*mut EeCxxSoundObject>();
        assert_eq!(std::mem::offset_of!(EeUnitsUnitClassUpgradeLinks, mSelectionSounds) + 2 * p, 9 * p);
        assert_eq!(std::mem::offset_of!(EeUnitsUnitClassUpgradeLinks, mButton), p);
        assert!(EeUnitsUnitClassUpgradeLinks::default().mModel.is_null());
    }
}
