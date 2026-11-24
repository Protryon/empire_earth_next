//! `EEUnitClass`'s plain members as a Rust record the C++ class derives from (R5b.4, the twenty-seventh round):
//! `class EEUnitClass : public UReferenceCounter, public EeUnitsUnitClassFields` (ee/EEUnit.h), as the units'
//! records of unit_fields.rs. The fields keep their C++ names and types (`c_long`/`c_ulong` are the C++ `long`/
//! `unsigned long` on every data model) and their order; `mAirTrafficType` and `mCommands` (+0x28, +0x3c in the
//! original, between the names and the research list) come first, then the block from `mCruiseAltitude` (+0x11c)
//! to `mIsParatrooperBomber` (+0x326). The volatile `mIsMediaLoaded` stays a C++ member; the never-read `mB329` and
//! `mS32A` are gone. The constructor's copies from the database row are ee_sim::units::unit_class_fields' `fill`.

#![allow(non_snake_case)]

use crate::units::obj_fields::EeCxxGraphicObject;
use crate::world::unit_fields::{EeCxxUnitBehavior, EeCxxUnitClass};
use std::os::raw::{c_long, c_ulong, c_void};

/// `EEUnitClass`'s plain members (ee/EEUnit.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsUnitClassFields {
    pub mAirTrafficType: c_long,
    /// the command buttons (30: formation)
    pub mCommands: [c_ulong; 9],
    pub mCruiseAltitude: f32,
    pub mMinRange: f32,
    pub mRange: f32,
    pub mAreaEffect: f32,
    pub mAreaEffectScale: f32,
    pub mScale: f32,
    pub mMaxVelocity: f32,
    pub mF138: f32,
    pub mAcceleration: f32,
    pub mAngularAcceleration: f32,
    pub mMaxAngularVelocity: f32,
    pub mCostScale: f32,
    pub mTurretMaxAngularVelocity: f32,
    pub mBuildTimeScale: f32,
    pub mGarrisonBonus: f32,
    pub mMaxRoll: f32,
    pub mFacingOffset: f32,
    pub mFiringArc: f32,
    pub mLineOfSight: f32,
    pub mSelectionSize: f32,
    pub mShadowScale: f32,
    pub mL170: f32,
    pub mOuterCollisionSphere: f32,
    pub mInnerCollisionSphere: f32,
    pub mLaunchPitch: f32,
    pub mL180: c_long,
    pub mConversionResistance: f32,
    pub mConversionRate: f32,
    pub mSlopeSpeedPenalty: f32,
    pub mBobHeight: f32,
    pub mBobPitch: f32,
    pub mFriendlyDamageFactor: f32,
    pub mF19C: f32,
    pub mHealThreshold: f32,
    pub mHealRange: f32,
    pub mHealDelay: f32,
    pub mLoadRange: f32,
    pub mUnloadRange: f32,
    pub mIsComplex: c_long,
    pub mCategory: c_ulong,
    pub mUnitType: c_ulong,
    pub mCombatReaction: c_long,
    pub mWeapon: c_long,
    pub mFamily: c_ulong,
    pub mAmmoType: c_long,
    pub mAIKind: c_ulong,
    pub mCounterKind1: c_ulong,
    pub mCounterKind2: c_ulong,
    pub mCounterKind3: c_ulong,
    pub mKindFlags: c_long,
    pub mTechnology: c_long,
    pub mGarrisonTechnology: c_long,
    pub mWaterKind: c_long,
    /// the base armour per weapon kind
    pub mArmor: [c_long; 6],
    pub mL208: c_long,
    pub mEpoch: c_long,
    pub mReloadTime: c_long,
    pub mAttack: c_long,
    pub mWorkRate: c_long,
    pub mGatherTimes: [c_long; 7],
    pub mFlightTime: c_long,
    pub mLeadership: c_long,
    pub mMaxObjectCarry: c_long,
    pub mInfluence: c_long,
    pub mRottingTime: c_long,
    pub mTransportCapacity: c_long,
    pub mGarrisonCapacity: c_long,
    pub mRepairRate: c_long,
    pub mWeight: c_long,
    pub mHitPoints: c_long,
    pub mNameStringID: c_long,
    pub mTargetSearchInterval: c_long,
    pub mResourceType: c_long,
    pub mResourceAmount: c_long,
    pub mCarryAmount: c_long,
    pub mHealingPoints: c_long,
    pub mRegeneration: c_long,
    pub mPowerRegeneration: c_long,
    pub mPower: c_long,
    pub mL284: c_long,
    pub mMissileDelayTime: c_long,
    pub mTileSize: c_long,
    pub mCloakRadius: c_long,
    pub mL294: c_long,
    pub mOverlapUnitSet: c_long,
    pub mAmmunition: c_long,
    pub mNumUnits: c_long,
    pub mLastUnitGoneTime: c_long,
    pub mMiniMapColor: c_long,
    pub mInitialGarrisonCount: c_long,
    pub mWonderBonus: c_long,
    pub mWonderUnitSet: c_long,
    pub mMinConversionTime: c_long,
    /// the area-effect template ids
    pub mAreaEffectTemplates: [c_long; 4],
    pub mAnimalID: c_long,
    /// the movement domains (land 2, air 1, sea 4, space 8)
    pub mDomains: u8,
    pub mB2D1: bool,
    /// per terrain type, whether the class may stand on it
    pub mTerrainTypes: [bool; 17],
    pub mIsAffectedByGravity: bool,
    pub mUsesAmmunition: bool,
    pub mShootsHigh: bool,
    pub mUpgradesAreaEffect: bool,
    pub mUpgradesRepair: bool,
    pub mUpgradesShockArmor: bool,
    pub mUpgradesPierceArmor: bool,
    pub mUpgradesArrowArmor: bool,
    pub mUpgradesLaserArmor: bool,
    pub mUpgradesGunArmor: bool,
    pub mUpgradesMissileArmor: bool,
    pub mUpgradesRange: bool,
    pub mPitchesWithTerrain: bool,
    pub mRollsWithTerrain: bool,
    pub mFiresBroadside: bool,
    pub mCanAttackGround: bool,
    pub mCanFireOverWalls: bool,
    pub mIsScenarioSelectable: bool,
    pub mIsEnabled: bool,
    pub mFacesCamera: bool,
    pub mIsDecoration: bool,
    pub mFiresWhileMoving: bool,
    pub mCanFly: bool,
    pub mB2FA: bool,
    pub mBuildsOnElevation: bool,
    pub mBlocksTile: bool,
    pub mCanEnterAirTransports: bool,
    pub mCanEnterLandTransports: bool,
    pub mCanEnterSeaTransports: bool,
    pub mCanEnterSpaceTransports: bool,
    pub mCanEnterFortresses: bool,
    pub mCanGarrison: bool,
    pub mIsCollidable: bool,
    pub mCanSetRallyPoint: bool,
    pub mB305: bool,
    pub mIsGaiaPlaceable: bool,
    pub mConvertsBuildings: bool,
    pub mConvertsPriests: bool,
    pub mIsPlayerPlaceable: bool,
    pub mIsVital: bool,
    pub mIsCloakable: bool,
    pub mIsExpansionUnit: bool,
    pub mCanEnterAirports: bool,
    pub mCanEnterCarriers: bool,
    pub mCanEnterNavyYards: bool,
    pub mIsWall: bool,
    /// the contagion functors copy it to the unit's
    pub mIsDeletable: bool,
    pub mIsInactive: bool,
    pub mIsAttackUnit: bool,
    pub mIsDefenseUnit: bool,
    pub mCanCrossForest: bool,
    pub mIsObstacle: bool,
    pub mIsTree: bool,
    pub mIsMelee: bool,
    pub mCanDisband: bool,
    pub mIsHuman: bool,
    pub mIsHorse: bool,
    pub mIsTower: bool,
    pub mHasCalamityPowers: bool,
    pub mIsProjectile: bool,
    pub mCanChangeBehavior: bool,
    pub mIsCyber: bool,
    pub mCanPatrol: bool,
    /// set by `CreateUnitClasses` (unit set 0x81)
    pub mIsMedic: bool,
    pub mSkipsLineOfFireCheck: bool,
    /// set by `CreateUnitClasses` (unit sets 0x8a, 0x97, 0x96)
    pub mIsFlare: bool,
    pub mIsBoobyTrap: bool,
    pub mIsParatrooperBomber: bool,
    pub mHasSpecialAttack: bool,
    /// port (not in the original): the class's `IsLandBuilding()`, set by `CreateUnitClasses` once the class's
    /// vtable is final; the unit store reads it (store.rs)
    pub mIsLandBuildingClass: bool,
}

/// `EEUnitClass`'s pointer members (units round 11; were C++ members of ee/EEUnit.h), the third base of
/// `class EEUnitClass : public UReferenceCounter, public EeUnitsUnitClassFields, public EeUnitsUnitClassLinks`, in
/// the original's member order (+0x060 `mAIScript` .. +0x118 `mBehavior`). A record of its own: `fill` writes the
/// plain record whole after the constructor has set these. The C++ reads them by name, typed `class X*` (the
/// renames in rust/cbindgen.toml). The 29 effects are contiguous from `mAttachedEffect` (`GetEffects`' array).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsUnitClassLinks {
    /// the class's AI script (EEUnitAction::GetScript of its name: a Rust script's handle)
    pub mAIScript: *mut c_void,
    pub mAttachedEffect: *mut EeCxxGraphicEffect,
    pub mDeathEffect: *mut EeCxxGraphicEffect,
    pub mDamageEffect: *mut EeCxxGraphicEffect,
    pub mHealEffect: *mut EeCxxGraphicEffect,
    pub mFireTrailEffect: *mut EeCxxGraphicEffect,
    pub mTrailEffect: *mut EeCxxGraphicEffect,
    pub mMuzzleFlashEffect: *mut EeCxxGraphicEffect,
    pub mRippleEffect: *mut EeCxxGraphicEffect,
    pub mSplashEffect: *mut EeCxxGraphicEffect,
    pub mImpactSparksEffect: *mut EeCxxGraphicEffect,
    pub mImpactBloodEffect: *mut EeCxxGraphicEffect,
    pub mEffect090: *mut EeCxxGraphicEffect,
    pub mCreationEffect: *mut EeCxxGraphicEffect,
    pub mEffect098: *mut EeCxxGraphicEffect,
    pub mEffect09C: *mut EeCxxGraphicEffect,
    pub mEffect0A0: *mut EeCxxGraphicEffect,
    pub mEffect0A4: *mut EeCxxGraphicEffect,
    pub mEffect0A8: *mut EeCxxGraphicEffect,
    pub mSpecialAttackEffect: *mut EeCxxGraphicEffect,
    pub mEffect0B0: *mut EeCxxGraphicEffect,
    pub mShotEffect: *mut EeCxxGraphicEffect,
    pub mLeprosyEffect: *mut EeCxxGraphicEffect,
    pub mPlagueEffect: *mut EeCxxGraphicEffect,
    pub mPulseCannonEffect: *mut EeCxxGraphicEffect,
    pub mMoraleEffect: *mut EeCxxGraphicEffect,
    pub mBattleCryEffect: *mut EeCxxGraphicEffect,
    pub mShellHoleEffect: *mut EeCxxGraphicEffect,
    pub mEffect0D0: *mut EeCxxGraphicEffect,
    pub mBloodStainEffect: *mut EeCxxGraphicEffect,
    /// the class media (LoadClassMedia, UnloadClassMedia)
    pub mModel: *mut EeCxxGraphicObject,
    pub mButton: *mut EeCxxButtonObject,
    pub mCreationSound: *mut EeCxxSoundObject,
    pub mAcknowledgeSounds: [*mut EeCxxSoundObject; 2],
    pub mSelectionSounds: [*mut EeCxxSoundObject; 2],
    pub mDeathSound: *mut EeCxxSoundObject,
    /// the related classes (referenced; the destructor dereferences them)
    pub mProjectileClass: *mut EeCxxUnitClass,
    pub mScaffoldingClass: *mut EeCxxUnitClass,
    /// `VirtualC4` creates a unit of it where the unit dies
    pub mDeathClass: *mut EeCxxUnitClass,
    pub mDeathSpawnClass: *mut EeCxxUnitClass,
    pub mRemainsClass: *mut EeCxxUnitClass,
    /// the class it packs/unpacks into (EEMPackUnpack)
    pub mPackedClass: *mut EeCxxUnitClass,
    /// the class's physics model
    pub mPhysicsModel: *mut EeCxxPhysicsModel,
    /// the record's upgrade table (`EEUCUpgradeTable::sTable`'s entry: its Rust record)
    pub mImprovements: *mut ee_data::tables::EeDataUpgradeFields,
    /// the units' default behaviour (EEUnitBehavior::sTable[record->mBehavior])
    pub mBehavior: *mut EeCxxUnitBehavior,
}

impl Default for EeUnitsUnitClassLinks {
    fn default() -> Self {
        // SAFETY: every field is a raw pointer: all zero is each one's default
        unsafe { std::mem::zeroed() }
    }
}

// SAFETY: the pointers are the C++ class's members, read and written only on the simulation thread
unsafe impl Send for EeUnitsUnitClassLinks {}
unsafe impl Sync for EeUnitsUnitClassLinks {}

/// The C++ classes the links point to (opaque; written `class X` / `struct X` in the generated header).
pub struct EeCxxGraphicEffect {
    _opaque: [u8; 0],
}
pub struct EeCxxButtonObject {
    _opaque: [u8; 0],
}
pub struct EeCxxSoundObject {
    _opaque: [u8; 0],
}
pub struct EeCxxPhysicsModel {
    _opaque: [u8; 0],
}

/// The class's 29 effects and 6 related classes in the constructor's order (the record's: EEUnitClass.cpp's member
/// pointer tables until units round 11).
pub const EE_UNIT_CLASS_EFFECTS: usize = 29;
pub const EE_UNIT_CLASS_RELATED: usize = 6;

impl EeUnitsUnitClassLinks {
    /// Effect `i` of the constructor's order: death, damage, heal, fire trail, trail, muzzle flash, ripple, splash,
    /// impact sparks, impact blood, attached, then the members from `mEffect090` on in their own order.
    pub fn effect_slot(&mut self, i: usize) -> &mut *mut EeCxxGraphicEffect {
        let member = match i {
            0..=9 => i + 1,
            10 => 0,
            _ => i,
        };
        assert!(member < EE_UNIT_CLASS_EFFECTS, "a class has 29 effects");
        // SAFETY: the 29 effect pointers are contiguous from mAttachedEffect (repr(C), the test below)
        unsafe { &mut *(&raw mut self.mAttachedEffect).add(member) }
    }

    /// Related class `k`: projectile, scaffolding, death, death spawn, packed, remains.
    pub fn related_slot(&mut self, k: usize) -> &mut *mut EeCxxUnitClass {
        match k {
            0 => &mut self.mProjectileClass,
            1 => &mut self.mScaffoldingClass,
            2 => &mut self.mDeathClass,
            3 => &mut self.mDeathSpawnClass,
            4 => &mut self.mPackedClass,
            5 => &mut self.mRemainsClass,
            _ => panic!("a class has 6 related classes"),
        }
    }
}

/// The record's effect id `i` (the constructor's order).
pub fn record_effect(r: &ee_data::tables::DbUnitClass, i: usize) -> i32 {
    [
        r.mDeathEffect,
        r.mDamageEffect,
        r.mHealEffect,
        r.mUnusedFireTrailEffect,
        r.mTrailEffect,
        r.mMuzzleFlashEffect,
        r.mRippleEffect,
        r.mSplashEffect,
        r.mImpactSparksEffect,
        r.mImpactBloodEffect,
        r.mAttachedEffect,
        r.mUnusedEffect1,
        r.mCreationEffect,
        r.mUnusedEffect2,
        r.mUnusedEffect3,
        r.mUnusedEffect4,
        r.mUnusedEffect5,
        r.mUnusedEffect6,
        r.mSpecialAttackEffect,
        r.mUnusedEffect7,
        r.mShotEffect,
        r.mLeprosyEffect,
        r.mPlagueEffect,
        r.mPulseCannonEffect,
        r.mMoraleEffect,
        r.mBattleCryEffect,
        r.mShellHoleEffect,
        r.mUnusedEffect8,
        r.mBloodStainEffect,
    ][i]
}

/// The record's related unit type `k` ([`EeUnitsUnitClassLinks::related_slot`]'s order).
pub fn record_related(r: &ee_data::tables::DbUnitClass, k: usize) -> i32 {
    [r.mProjectileType, r.mScaffoldingType, r.mDeathType, r.mDeathSpawnType, r.mPackedType, r.mRemainsType][k]
}

/// `&class->` effect `i` (the constructor's order; EEUnitClass.cpp's host).
///
/// # Safety
/// `links` is a live class's record; `i` below 29.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uc_effect_slot(
    links: *mut EeUnitsUnitClassLinks,
    i: i32,
) -> *mut *mut EeCxxGraphicEffect {
    // SAFETY: the caller's contract
    unsafe { (*links).effect_slot(i as usize) }
}

/// `&class->` related class `k`.
///
/// # Safety
/// `links` is a live class's record; `k` below 6.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uc_related_slot(links: *mut EeUnitsUnitClassLinks, k: i32) -> *mut *mut EeCxxUnitClass {
    // SAFETY: the caller's contract
    unsafe { (*links).related_slot(k as usize) }
}

/// The record's effect id `i`.
///
/// # Safety
/// `record` is a live record; `i` below 29.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uc_record_effect(record: *const ee_data::tables::DbUnitClass, i: i32) -> i32 {
    // SAFETY: the caller's contract
    record_effect(unsafe { &*record }, i as usize)
}

/// The record's related unit type `k`.
///
/// # Safety
/// `record` is a live record; `k` below 6.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uc_record_related(record: *const ee_data::tables::DbUnitClass, k: i32) -> i32 {
    // SAFETY: the caller's contract
    record_related(unsafe { &*record }, k as usize)
}

#[cfg(test)]
mod links_tests {
    use super::*;
    use std::mem::{offset_of, size_of};

    #[test]
    fn the_effects_are_one_array_and_the_media_follow() {
        let p = size_of::<*mut c_void>();
        assert_eq!(offset_of!(EeUnitsUnitClassLinks, mBloodStainEffect) - offset_of!(EeUnitsUnitClassLinks, mAttachedEffect), 28 * p);
        assert_eq!(offset_of!(EeUnitsUnitClassLinks, mModel), 30 * p);
        assert_eq!(offset_of!(EeUnitsUnitClassLinks, mBehavior), 46 * p);
        assert_eq!(size_of::<EeUnitsUnitClassLinks>(), 47 * p);
        assert!(EeUnitsUnitClassLinks::default().mBehavior.is_null());
    }

    #[test]
    fn the_slots_follow_the_constructors_order() {
        let mut l = EeUnitsUnitClassLinks::default();
        let base = &raw const l as usize;
        let at = |p: &mut *mut EeCxxGraphicEffect| p as *mut _ as usize - base;
        assert_eq!(at(l.effect_slot(0)), offset_of!(EeUnitsUnitClassLinks, mDeathEffect));
        assert_eq!(at(l.effect_slot(9)), offset_of!(EeUnitsUnitClassLinks, mImpactBloodEffect));
        assert_eq!(at(l.effect_slot(10)), offset_of!(EeUnitsUnitClassLinks, mAttachedEffect));
        assert_eq!(at(l.effect_slot(11)), offset_of!(EeUnitsUnitClassLinks, mEffect090));
        assert_eq!(at(l.effect_slot(12)), offset_of!(EeUnitsUnitClassLinks, mCreationEffect));
        assert_eq!(at(l.effect_slot(18)), offset_of!(EeUnitsUnitClassLinks, mSpecialAttackEffect));
        assert_eq!(at(l.effect_slot(24)), offset_of!(EeUnitsUnitClassLinks, mMoraleEffect));
        assert_eq!(at(l.effect_slot(28)), offset_of!(EeUnitsUnitClassLinks, mBloodStainEffect));
        let rel = |p: &mut *mut EeCxxUnitClass| p as *mut _ as usize - base;
        assert_eq!(rel(l.related_slot(4)), offset_of!(EeUnitsUnitClassLinks, mPackedClass));
        assert_eq!(rel(l.related_slot(5)), offset_of!(EeUnitsUnitClassLinks, mRemainsClass));
    }

    #[test]
    fn the_records_ids_follow_the_constructors_order() {
        // SAFETY: the record is plain numbers and bytes: all zero is a value
        let mut r: ee_data::tables::DbUnitClass = unsafe { std::mem::zeroed() };
        r.mHealEffect = 3;
        r.mUnusedFireTrailEffect = 4;
        r.mAttachedEffect = 11;
        r.mPulseCannonEffect = 24;
        r.mMoraleEffect = 25;
        r.mPackedType = 50;
        r.mRemainsType = 60;
        assert_eq!([2, 3, 10, 23, 24].map(|i| record_effect(&r, i)), [3, 4, 11, 24, 25]);
        assert_eq!([record_related(&r, 4), record_related(&r, 5)], [50, 60]);
    }
}
