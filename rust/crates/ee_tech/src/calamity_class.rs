//! The calamity classes' own data and the per-class virtuals around their Rust `Start`/`Stop`/`Update`
//! (`src/Empire Earth/Calamities/EECalamityClass.cpp`; the twenty `EECC*` classes kept only their world steps).
//!
//! Every class's numbers are one record, [`EeCalamityClassFields`], that `EECalamityClass` derives from: the base
//! members with their C++ names and types, then the union of the subclasses' numbers (each name is one class's
//! member, or the same member of several classes; the area effects' and the leprosy's radius in tiles is
//! `mRadiusTiles`, the teleport's float damage share `mDamageShare`, the functors' constructor arguments the classes
//! kept in locals `mSpreadTime` and `mFunctorFlag`). The class's effect, sound and functor pointers stay C++ members
//! of the base (`mGraphicEffect1..4`, `mSound`, `mSecondarySound`, `mFunctor`).
//!
//! What each class did in its own overrides is a table by its enum (`mClassEnum`, the `EECalamityEnum`):
//! - [`get_mode`]: how `Initialize` fetched each effect and sound (unconditionally, when the id is not 0, when it is
//!   above 0), [`fill`]: the numbers `Initialize` computed from the db record and the effects;
//! - [`media_step`]: `LoadMedia`'s, `UnloadMedia`'s and the destructor's steps in each class's order;
//! - [`sound_mode`]: `PlaySound`; [`functor_kind`]: the contagion functor `Initialize` made;
//! - [`run`]: the parameters and sounds `Start`, `Stop` and `Update` handed to the Rust bodies;
//! - [`reset`]: `Reset(calamity)`; [`cast_fields`] and [`targets_by_functor`]: `IsValidLocation` and `IsValidTarget`.
//!
//! Arithmetic as the C++ (RUST_PLAN 2.1): the `unsigned long` sums wrap, the conversions to `float` round to nearest,
//! `UMFloatToLong` is the UCRT's `lrintf`.

// the records keep the C++ member names; c_long/c_ulong are 32 bits on Windows: the casts are needed elsewhere
#![allow(non_snake_case, clippy::unnecessary_cast)]

use crate::calamity_start::EE_CALAMITY_POSITIVE;
use crate::calamity_update::{
    kind, EeCalamityParams, EE_CALAMITY_BLOCKED_BY_IMMUNITY, EE_CALAMITY_EFFECT1, EE_CALAMITY_EFFECT2,
    EE_CALAMITY_SECONDARY_SOUND, EE_CALAMITY_SOUND,
};
use std::os::raw::{c_long, c_ulong};

/// One record of `db\dbcalamity.dat` as `EECalamityManager::LoadCalamityClasses` reads it (`EECalamityData`,
/// `ee/EECalamityClass.h`; 236 bytes on every target).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeCalamityData {
    pub mLabel: [u8; 100],
    pub mFloatParam1: f32,
    pub mFloatParam2: f32,
    pub mQuakeMinimumStrength: f32,
    pub mQuakeStrength: f32,
    pub mUnused1: f32,
    pub mRange: f32,
    pub mUnusedDatabaseID: i32,
    pub mID: i32,
    pub mClassEnum: i32,
    pub mUnusedTargetKind: i32,
    pub mLifeTime: i32,
    pub mUnused2: i32,
    pub mGraphicEffectID1: i32,
    pub mGraphicEffectID2: i32,
    pub mGraphicEffectID3: i32,
    pub mGraphicEffectID4: i32,
    pub mUnused3: i32,
    pub mUnused4: i32,
    pub mUnused5: i32,
    pub mUnitClassIndex: i32,
    pub mIntParam1: i32,
    pub mIntParam2: i32,
    pub mIntParam3: i32,
    pub mIntParam4: i32,
    pub mIntParam5: i32,
    pub mIntParam6: i32,
    pub mUnused6: bool,
    pub mBoolParam1: bool,
    pub mUnused7: bool,
    pub mUnused8: bool,
    pub mIsCastByAllSelected: bool,
    pub mIsBlockedByImmunity: bool,
    pub mSoundID: i32,
    pub mSecondarySoundID: i32,
    pub mCost: i32,
    pub mUnitSet: u32,
    pub mNameID: i32,
    pub mUnusedClassFlag: bool,
}

impl Default for EeCalamityData {
    fn default() -> Self {
        // SAFETY: every field is an integer, float, bool (0 is false) or byte array: all zeros is valid
        unsafe { std::mem::zeroed() }
    }
}

/// A calamity class's numbers (`EECalamityClass` derives from it). The base members first, in their order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCalamityClassFields {
    pub mLifeTime: f32,
    pub mClassEnum: c_long,
    /// Each class's calamity flag bit (set on the units it holds).
    pub mClassFlags: c_ulong,
    pub mUnitSet: c_ulong,
    pub mNameID: c_long,
    /// The record's id (`EECalamityKind`).
    pub mID: c_long,
    pub mRange: c_long,
    pub mCost: c_long,
    pub mB24: bool,
    pub mIsBlockedByImmunity: bool,
    pub mIsCastByAllSelected: bool,
    pub mOverWater: bool,
    pub mPositive: bool,
    pub mFunctorFlag: bool,
    /// The earthquake's, the hurricane's, the leprosy's, the plague's and the fire's damage.
    pub mDamage: c_long,
    /// The earthquake's and the hurricane's tick time.
    pub mTickTime: c_long,
    pub mShake: c_long,
    pub mCrackCount: c_long,
    pub mArea: c_long,
    /// The hurricane's, the volcano's, the meteor storm's and the drop object's unit class.
    pub mUnitClassIndex: c_long,
    /// The time warp's and the teleport's `EEFindPlacement` range.
    pub mPlacementRange: c_long,
    pub mDepartTime: c_long,
    pub mAwayTime: c_long,
    pub mHoldTime: c_long,
    /// The morale's effect duration.
    pub mEffectDuration: c_long,
    /// The area effects' and the leprosy's first effect's duration.
    pub mEffect1Duration: c_ulong,
    pub mEffect2Time: c_long,
    pub mEffectTime2: c_ulong,
    pub mEffectTime3: c_ulong,
    /// The area effects' and the leprosy's radius in tiles (their `mRadius`).
    pub mRadiusTiles: c_ulong,
    /// The area effect's damage factor.
    pub mDamageFactor: c_long,
    pub mBurn: c_long,
    /// The functors' spread, update or damage time (a local of the classes' `Initialize`).
    pub mSpreadTime: c_long,
    pub mL3C: c_long,
    pub mL40: c_long,
    pub mL48: c_long,
    /// The teleport's share of the caster's hit points (its `mDamageFactor`).
    pub mDamageShare: f32,
    pub mDuration: f32,
    /// The earthquake's, the hurricane's, the fire's, the meteor storm's (and the plague functor's) radius.
    pub mRadius: f32,
    pub mShakeInterval: f32,
    pub mVolume: f32,
    pub mQuakeVolume: f32,
    pub mMinimumFactor: f32,
    pub mQuakeMinimumFactor: f32,
    pub mSpeed: f32,
    pub mSourceSpeed: f32,
    pub mFalloff: f32,
    /// The fire's (and the plague functor's) chance.
    pub mChance: f32,
    pub mMaximumRadius: f32,
    pub mMinimumRadius: f32,
    pub mMinimumInterval: f32,
    pub mMaximumInterval: f32,
    pub mSightBonus: f32,
}

/// What the C++ knows of the class's effects and sounds when it asks: which are set
/// (`EE_CALAMITY_MEDIA_EFFECT1 << i`, `EE_CALAMITY_MEDIA_SOUND`, `EE_CALAMITY_MEDIA_SECONDARY_SOUND`), each
/// effect's `mDuration`, `GetSize()` and `mTime1`, and the game variant's earthquake damage factor.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCalamityClassMedia {
    pub present: u32,
    pub duration: [u32; 4],
    pub size: [f32; 4],
    pub time1: [u32; 4],
    pub quake_damage_factor: f32,
}

/// [`EeCalamityClassMedia::present`]: effect 1 (effect `i` is this shifted by `i - 1`).
pub const EE_CALAMITY_MEDIA_EFFECT1: u32 = 1;
/// [`EeCalamityClassMedia::present`]: the sound.
pub const EE_CALAMITY_MEDIA_SOUND: u32 = 16;
/// [`EeCalamityClassMedia::present`]: the secondary sound.
pub const EE_CALAMITY_MEDIA_SECONDARY_SOUND: u32 = 32;

impl EeCalamityClassMedia {
    fn has(&self, effect: usize) -> bool {
        self.present & (EE_CALAMITY_MEDIA_EFFECT1 << (effect - 1)) != 0
    }
}

/// How `Initialize` fetched a slot ([`get_mode`]).
pub const EE_CALAMITY_GET_NONE: u32 = 0;
/// `Get(id)` whatever the id (`Get(0)` is the table's first entry).
pub const EE_CALAMITY_GET_ALWAYS: u32 = 1;
/// `if (id != 0)`.
pub const EE_CALAMITY_GET_NONZERO: u32 = 2;
/// `if (id > 0)`.
pub const EE_CALAMITY_GET_POSITIVE: u32 = 3;

/// The slots of [`get_mode`]: effects 1-4 are 0-3, the sound 4, the secondary sound 5.
const NONE: u32 = EE_CALAMITY_GET_NONE;
const ALWAYS: u32 = EE_CALAMITY_GET_ALWAYS;
const NONZERO: u32 = EE_CALAMITY_GET_NONZERO;
const POSITIVE: u32 = EE_CALAMITY_GET_POSITIVE;

/// How the class's `Initialize` fetched slot `slot` (effects 1-4 as 0-3, the sound 4, the secondary sound 5). An
/// effect it fetched it referenced (the C++ references a non-null one; the classes that referenced without a test
/// would have crashed on a null one).
pub fn get_mode(class_enum: i32, slot: u32) -> u32 {
    let modes: [u32; 6] = match class_enum {
        kind::AREA_EFFECT => [NONZERO, NONZERO, NONZERO, NONE, NONZERO, NONE],
        kind::AE_EXTRA_DAMAGE => [POSITIVE, POSITIVE, NONE, NONE, ALWAYS, NONE],
        kind::AE_MIND_CONTROL => [POSITIVE, POSITIVE, POSITIVE, NONE, ALWAYS, NONE],
        kind::CLOAKING => [NONZERO, NONE, NONE, NONE, NONZERO, NONE],
        kind::DROP_OBJECT => [NONZERO, NONZERO, NONZERO, NONE, NONZERO, NONE],
        kind::EARTHQUAKE => [ALWAYS, ALWAYS, NONE, NONE, ALWAYS, NONE],
        kind::FIRE => [POSITIVE, ALWAYS, NONE, NONE, ALWAYS, NONE],
        kind::HURRICANE => [NONZERO, NONZERO, NONZERO, NONE, ALWAYS, NONE],
        kind::LEPROSY => [ALWAYS, NONE, NONE, NONE, ALWAYS, NONE],
        kind::LOGIC_BOMB => [NONZERO, NONZERO, NONZERO, NONE, NONZERO, NONE],
        kind::METEOR_STORM => [NONE, NONE, NONE, NONE, ALWAYS, NONE],
        kind::MORALE => [POSITIVE, NONE, NONE, NONE, ALWAYS, ALWAYS],
        kind::PLAGUE => [ALWAYS, NONE, NONE, NONE, ALWAYS, ALWAYS],
        kind::TELEPORT => [POSITIVE, POSITIVE, NONE, NONE, ALWAYS, ALWAYS],
        kind::TIME_WARP => [NONZERO, NONZERO, NONZERO, NONZERO, ALWAYS, ALWAYS],
        kind::VOLCANO => [ALWAYS, ALWAYS, ALWAYS, NONE, ALWAYS, NONE],
        _ => [NONE; 6],
    };
    modes.get(slot as usize).copied().unwrap_or(NONE)
}

/// Whether a slot of mode `mode` is fetched for the record's id `id`.
pub fn fetches(mode: u32, id: i32) -> bool {
    match mode {
        EE_CALAMITY_GET_ALWAYS => true,
        EE_CALAMITY_GET_NONZERO => id != 0,
        EE_CALAMITY_GET_POSITIVE => id > 0,
        _ => false,
    }
}

/// The calamity flag each class's constructor set (`mClassFlags`; the units its contagion holds carry it).
pub fn class_flags(class_enum: i32) -> c_ulong {
    match class_enum {
        kind::PLAGUE => 1,
        kind::LEPROSY => 2,
        kind::FIRE | kind::SPECIAL_ATTACK => 4,
        kind::AE_EXTRA_DAMAGE => 0x40,
        kind::AE_MIND_CONTROL => 0x80,
        _ => 0,
    }
}

/// `EECalamityClass::Initialize`'s common fields (the base's own `Initialize`), and the class's flag.
pub fn initialize_base(out: &mut EeCalamityClassFields, data: &EeCalamityData) {
    out.mClassEnum = data.mClassEnum as c_long;
    out.mLifeTime = data.mLifeTime as f32;
    out.mUnitSet = data.mUnitSet as c_ulong;
    out.mCost = data.mCost as c_long;
    out.mRange = ee_ge::um::lrintf(data.mRange) as c_long;
    out.mIsBlockedByImmunity = data.mIsBlockedByImmunity;
    out.mIsCastByAllSelected = data.mIsCastByAllSelected;
    out.mB24 = data.mUnusedClassFlag;
    out.mNameID = data.mNameID as c_long;
    out.mID = data.mID as c_long;
    out.mClassFlags = class_flags(data.mClassEnum);
}

/// The numbers each class's `Initialize` set after the base's, from the record and its fetched effects (`media`).
/// An effect the class read without a test reads as 0 when it is missing (the original would have crashed).
pub fn fill(out: &mut EeCalamityClassFields, data: &EeCalamityData, media: &EeCalamityClassMedia) {
    let duration = |i: usize| media.duration[i - 1];
    match data.mClassEnum {
        kind::AREA_EFFECT => {
            out.mRadiusTiles = data.mIntParam1 as c_ulong;
            out.mDamageFactor = data.mIntParam2 as c_long;
            out.mSpreadTime = data.mIntParam3 as c_long;
            out.mEffect1Duration = if media.has(1) { duration(1) as c_ulong } else { 600 };
        }
        kind::AE_EXTRA_DAMAGE | kind::AE_MIND_CONTROL => {
            out.mRadiusTiles = data.mIntParam1 as c_ulong;
            out.mEffect1Duration = if media.has(1) { duration(1) as c_ulong } else { 0 };
            if media.has(2) {
                out.mEffect2Time = if data.mClassEnum == kind::AE_EXTRA_DAMAGE {
                    (duration(2) as c_ulong).wrapping_add(out.mEffect1Duration) as c_long
                } else {
                    out.mEffect1Duration as c_long
                };
            }
            out.mFunctorFlag = data.mBoolParam1;
        }
        kind::CLOAKING => {
            if media.has(1) {
                out.mLifeTime = duration(1) as c_ulong as f32;
            }
        }
        kind::DROP_OBJECT => out.mUnitClassIndex = data.mUnitClassIndex as c_long,
        kind::EARTHQUAKE => {
            out.mRadius = media.size[0];
            out.mMinimumFactor = data.mFloatParam1;
            out.mVolume = data.mFloatParam2;
            out.mQuakeMinimumFactor = data.mQuakeMinimumStrength;
            out.mQuakeVolume = data.mQuakeStrength;
            out.mSourceSpeed = duration(2) as c_ulong as f32 * 0.01f32;
            out.mFalloff = 1.0f32 / data.mIntParam1 as f32;
            out.mDamage = data.mIntParam2 as c_long;
            out.mShake = data.mIntParam3 as c_long;
            out.mShakeInterval = 1.0f32 / data.mIntParam4 as f32;
            out.mTickTime = data.mIntParam5 as c_long;
            out.mCrackCount = data.mIntParam6 as c_long;
            out.mDuration = duration(1) as c_ulong as f32 * 0.001f32;
            out.mSpeed = out.mRadius / out.mDuration;
        }
        kind::FIRE => {
            out.mBurn = data.mIntParam2 as c_long;
            out.mDamage = data.mIntParam3 as c_long;
            out.mSpreadTime = data.mIntParam4 as c_long;
            out.mRadius = data.mFloatParam1;
            out.mChance = data.mFloatParam2;
        }
        kind::HURRICANE => {
            out.mDamage = data.mIntParam1 as c_long;
            out.mArea = data.mIntParam6 as c_long;
            out.mTickTime = data.mIntParam2 as c_long;
            out.mRadius = data.mFloatParam1;
            out.mUnitClassIndex = data.mUnitClassIndex as c_long;
            out.mOverWater = data.mBoolParam1;
        }
        kind::LEPROSY => {
            out.mRadiusTiles = data.mIntParam1 as c_ulong;
            out.mDamage = data.mIntParam2 as c_long;
            out.mSpreadTime = data.mIntParam3 as c_long;
            out.mEffect1Duration = duration(1) as c_ulong;
        }
        kind::LINE_OF_SIGHT => out.mSightBonus = data.mFloatParam1,
        kind::LOGIC_BOMB => {
            out.mEffectTime2 = if media.has(1) { duration(1) as c_ulong } else { 600 };
            if media.has(2) {
                out.mEffectTime3 = (duration(2) as c_ulong).wrapping_add(out.mEffectTime2);
            }
        }
        kind::METEOR_STORM => {
            out.mLifeTime = data.mLifeTime as f32;
            out.mUnitClassIndex = data.mUnitClassIndex as c_long;
            out.mRadius = data.mFloatParam1;
            out.mMinimumInterval = data.mIntParam2 as f32;
            out.mMaximumInterval = data.mIntParam3 as f32;
        }
        kind::MORALE => {
            if media.has(1) {
                out.mEffectDuration = duration(1) as c_long;
            }
            out.mPositive = data.mBoolParam1;
        }
        kind::PLAGUE => {
            out.mDamage = data.mIntParam1 as c_long;
            out.mRadius = data.mFloatParam1;
            out.mSpreadTime = data.mIntParam2 as c_long;
            out.mChance = data.mFloatParam2;
        }
        kind::TELEPORT => {
            out.mDamageShare = data.mFloatParam1;
            out.mPlacementRange = data.mIntParam1 as c_long;
            out.mHoldTime = if media.has(1) { duration(1) as c_long } else { 2000 };
        }
        kind::TIME_WARP => {
            out.mPlacementRange = data.mIntParam1 as c_long;
            if media.has(1) {
                out.mDepartTime = duration(1) as c_long;
            }
            out.mAwayTime = if media.has(2) { duration(2) as c_long } else { 3000 };
        }
        kind::VOLCANO => {
            out.mLifeTime = (duration(1) as c_ulong).wrapping_add(media.time1[0] as c_ulong) as f32;
            out.mL48 = data.mIntParam6 as c_long;
            out.mUnitClassIndex = data.mUnitClassIndex as c_long;
            out.mL3C = data.mIntParam2 as c_long;
            out.mL40 = data.mIntParam3 as c_long;
            out.mMinimumRadius = data.mIntParam4 as f32;
            out.mMaximumRadius = data.mIntParam5 as f32;
        }
        _ => {}
    }
}

/// The contagion functor `Initialize` made ([`functor_kind`]).
pub const EE_CALAMITY_FUNCTOR_NONE: u32 = 0;
/// `EEAreaEffectFunctor(effect 2, effect 3, mDamageFactor, mSpreadTime, unit set, mode)`.
pub const EE_CALAMITY_FUNCTOR_AREA: u32 = 1;
/// `EEAEExtraDamageFunctor(unit set, mode, mFunctorFlag)`.
pub const EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE: u32 = 2;
/// `EEAEMindControlFunctor(effect 3, unit set, mode)`.
pub const EE_CALAMITY_FUNCTOR_MIND_CONTROL: u32 = 3;
/// `EEFireFunctor(effect 2, mBurn, mRadius, mChance, mDamage, mSpreadTime, unit set, mode)`.
pub const EE_CALAMITY_FUNCTOR_FIRE: u32 = 4;
/// `EELeprosyFunctor(mDamage, mSpreadTime, unit set, mode)`.
pub const EE_CALAMITY_FUNCTOR_LEPROSY: u32 = 5;
/// `EEPlagueFunctor(mRadius, mChance, mDamage, mSpreadTime, unit set, mode)`.
pub const EE_CALAMITY_FUNCTOR_PLAGUE: u32 = 6;

/// The functor the class's `Initialize` made last (its mode: 2 when the class is blocked by immunity, else 3).
pub fn functor_kind(class_enum: i32) -> u32 {
    match class_enum {
        kind::AREA_EFFECT => EE_CALAMITY_FUNCTOR_AREA,
        kind::AE_EXTRA_DAMAGE => EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE,
        kind::AE_MIND_CONTROL => EE_CALAMITY_FUNCTOR_MIND_CONTROL,
        kind::FIRE => EE_CALAMITY_FUNCTOR_FIRE,
        kind::LEPROSY => EE_CALAMITY_FUNCTOR_LEPROSY,
        kind::PLAGUE => EE_CALAMITY_FUNCTOR_PLAGUE,
        _ => EE_CALAMITY_FUNCTOR_NONE,
    }
}

/// The step lists of [`media_step`]: `LoadMedia(player, on demand)`.
pub const EE_CALAMITY_MEDIA_LOAD: u32 = 0;
/// `UnloadMedia(player)`.
pub const EE_CALAMITY_MEDIA_UNLOAD: u32 = 1;
/// The class's destructor.
pub const EE_CALAMITY_MEDIA_DESTROY: u32 = 2;

/// A step ([`media_step`]) after the last one.
pub const EE_CALAMITY_STEP_END: u32 = 0;
/// Effect 1..4 (`EE_CALAMITY_STEP_EFFECT1 + i - 1`): `Load(on demand)`, `Unload()` or `Dereference()` when it is
/// set.
pub const EE_CALAMITY_STEP_EFFECT1: u32 = 1;
/// The sound: `LoadMedia()` or `UnloadMedia()` when it is set.
pub const EE_CALAMITY_STEP_SOUND: u32 = 5;
/// The secondary sound, as the sound.
pub const EE_CALAMITY_STEP_SECONDARY_SOUND: u32 = 6;
/// The unit class `mUnitClassIndex`'s media: loaded when on demand and its media mask set to 0x7f, or unloaded.
pub const EE_CALAMITY_STEP_UNIT_MEDIA: u32 = 7;
/// The unit class `mUnitClassIndex`'s buttons: loaded or unloaded.
pub const EE_CALAMITY_STEP_UNIT_BUTTONS: u32 = 8;
/// The sound manager's `Unload` of the sound's sample (the volcano's destructor).
pub const EE_CALAMITY_STEP_SAMPLE: u32 = 9;
/// `delete mFunctor`.
pub const EE_CALAMITY_STEP_FUNCTOR: u32 = 10;

const E1: u32 = EE_CALAMITY_STEP_EFFECT1;
const E2: u32 = EE_CALAMITY_STEP_EFFECT1 + 1;
const E3: u32 = EE_CALAMITY_STEP_EFFECT1 + 2;
const E4: u32 = EE_CALAMITY_STEP_EFFECT1 + 3;
const S1: u32 = EE_CALAMITY_STEP_SOUND;
const S2: u32 = EE_CALAMITY_STEP_SECONDARY_SOUND;
const UM: u32 = EE_CALAMITY_STEP_UNIT_MEDIA;
const UB: u32 = EE_CALAMITY_STEP_UNIT_BUTTONS;
const SM: u32 = EE_CALAMITY_STEP_SAMPLE;
const FN: u32 = EE_CALAMITY_STEP_FUNCTOR;

fn media_steps(class_enum: i32, list: u32) -> &'static [u32] {
    let (load, unload, destroy): (&'static [u32], &'static [u32], &'static [u32]) = match class_enum {
        kind::AREA_EFFECT | kind::AE_EXTRA_DAMAGE | kind::AE_MIND_CONTROL => {
            (&[E1, E2, E3, S1], &[E1, E2, E3, S1], &[E2, E1, E3, FN])
        }
        kind::CLOAKING => (&[E1, S1], &[E1, S1], &[E1]),
        kind::DROP_OBJECT | kind::LOGIC_BOMB => (&[E1, E2, E3, S1], &[E1, E2, E3, S1], &[E1, E2, E3]),
        kind::EARTHQUAKE => (&[E2, E1, S1], &[E2, E1, S1], &[E2, E1]),
        kind::FIRE => (&[E1, E2, S1], &[E1, E2, S1], &[FN, E1, E2]),
        kind::HURRICANE => (&[E2, E3, S1, UM, UB], &[E2, E3, S1, UM, UB], &[E2, E3, E1]),
        kind::LEPROSY => (&[E1, S1], &[E1, S1], &[E1, FN]),
        kind::METEOR_STORM => (&[S1, UM], &[S1, UM], &[]),
        kind::MORALE => (&[E1, S1, S2], &[E1, S1, S2], &[E1]),
        kind::PLAGUE => (&[E1, S1, S2], &[E1, S1, S2], &[E1, FN]),
        kind::TELEPORT => (&[E2, E1, S1, S2], &[E2, E1, S1, S2], &[E1, E2]),
        kind::TIME_WARP => (&[E1, E2, E3, E4, S1, S2], &[E1, E2, E3, E4, S1, S2], &[E1, E2, E3, E4]),
        kind::VOLCANO => (&[E1, E2, E3, E4, S1, UM, UB], &[E1, E2, E3, E4, S1, UM, UB], &[SM, E1, E2, E3, E4]),
        _ => (&[], &[], &[]),
    };
    match list {
        EE_CALAMITY_MEDIA_LOAD => load,
        EE_CALAMITY_MEDIA_UNLOAD => unload,
        EE_CALAMITY_MEDIA_DESTROY => destroy,
        _ => &[],
    }
}

/// Step `index` of the class's list `list` (`EE_CALAMITY_MEDIA_*`), in the class's own order;
/// [`EE_CALAMITY_STEP_END`] past the last.
pub fn media_step(class_enum: i32, list: u32, index: u32) -> u32 {
    media_steps(class_enum, list).get(index as usize).copied().unwrap_or(EE_CALAMITY_STEP_END)
}

/// `PlaySound(calamity)`: nothing.
pub const EE_CALAMITY_PLAY_NONE: u32 = 0;
/// The class's sound as the calamity's referenced main sound, from the start (`SetSound(PlayAndReturnReferencedNode
/// (sample, volume, 0, 1, looping))`).
pub const EE_CALAMITY_PLAY_NODE: u32 = 1;
/// The class's sound once at the target location (`EEComputeSoundParameters` with the calamities' falloff).
pub const EE_CALAMITY_PLAY_AT_TARGET: u32 = 2;

/// What the class's `PlaySound` did.
pub fn sound_mode(class_enum: i32) -> u32 {
    match class_enum {
        kind::EARTHQUAKE | kind::HURRICANE | kind::VOLCANO => EE_CALAMITY_PLAY_NODE,
        kind::METEOR_STORM => EE_CALAMITY_PLAY_AT_TARGET,
        _ => EE_CALAMITY_PLAY_NONE,
    }
}

/// [`run`]'s `which`: `Start`.
pub const EE_CALAMITY_RUN_START: u32 = 0;
/// `Stop`.
pub const EE_CALAMITY_RUN_STOP: u32 = 1;
/// `Update`.
pub const EE_CALAMITY_RUN_UPDATE: u32 = 2;

/// [`EeCalamityClassRun::sounds`]: the run gets the class's sound.
pub const EE_CALAMITY_RUN_SOUND: u32 = 1;
/// [`EeCalamityClassRun::sounds`]: the run gets the class's secondary sound.
pub const EE_CALAMITY_RUN_SECONDARY_SOUND: u32 = 2;

/// What one of the class's `Start`, `Stop` or `Update` hands its Rust body: the parameters, which of the class's
/// sounds (`EE_CALAMITY_RUN_*`), and whether it runs at all (the classes whose `Stop` was empty).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCalamityClassRun {
    pub params: EeCalamityParams,
    pub sounds: u32,
    pub runs: bool,
}

fn run_sounds(class_enum: i32) -> u32 {
    match class_enum {
        kind::MORALE | kind::TELEPORT | kind::TIME_WARP => EE_CALAMITY_RUN_SOUND | EE_CALAMITY_RUN_SECONDARY_SOUND,
        kind::CONSCRIPTION
        | kind::LINE_OF_SIGHT
        | kind::METEOR_STORM
        | kind::PLAGUE
        | kind::SPECIAL_ATTACK
        | kind::SPECIAL_LOCATION => 0,
        _ => EE_CALAMITY_RUN_SOUND,
    }
}

/// The class's `Start` (`which` 0), `Stop` (1) or `Update` (2): `EECalamityBaseParams` over the sounds it passed,
/// then its own parameters.
pub fn run(fields: &EeCalamityClassFields, media: &EeCalamityClassMedia, which: u32) -> EeCalamityClassRun {
    let class_enum = fields.mClassEnum as i32;
    let mut sounds = run_sounds(class_enum);
    if media.present & EE_CALAMITY_MEDIA_SOUND == 0 {
        sounds &= !EE_CALAMITY_RUN_SOUND;
    }
    if media.present & EE_CALAMITY_MEDIA_SECONDARY_SOUND == 0 {
        sounds &= !EE_CALAMITY_RUN_SECONDARY_SOUND;
    }
    let mut p = EeCalamityParams {
        class_enum,
        lifetime: fields.mLifeTime,
        flags: (if fields.mIsBlockedByImmunity { EE_CALAMITY_BLOCKED_BY_IMMUNITY } else { 0 })
            | (if sounds & EE_CALAMITY_RUN_SOUND != 0 { EE_CALAMITY_SOUND } else { 0 })
            | (if sounds & EE_CALAMITY_RUN_SECONDARY_SOUND != 0 { EE_CALAMITY_SECONDARY_SOUND } else { 0 }),
        ..EeCalamityParams::default()
    };
    let effect1 = if media.has(1) { EE_CALAMITY_EFFECT1 } else { 0 };
    let effect2 = if media.has(2) { EE_CALAMITY_EFFECT2 } else { 0 };
    let mut runs = true;
    if which == EE_CALAMITY_RUN_UPDATE {
        match class_enum {
            kind::AREA_EFFECT | kind::LEPROSY => p.area_delay = fields.mEffect1Duration as u32,
            kind::AE_EXTRA_DAMAGE | kind::AE_MIND_CONTROL => {
                p.flags |= effect2;
                p.area_delay = fields.mEffect1Duration as u32;
                p.area_time2 = fields.mEffect2Time as i32;
            }
            kind::EARTHQUAKE => {
                p.quake_damage = fields.mDamage as i32;
                p.quake_tick_time = fields.mTickTime as i32;
                p.quake_shake = fields.mShake as i32;
                p.quake_duration = fields.mDuration;
                p.quake_radius = fields.mRadius;
                p.quake_shake_interval = fields.mShakeInterval;
                p.quake_volume = fields.mVolume;
                p.quake_quake_volume = fields.mQuakeVolume;
                p.quake_minimum_factor = fields.mMinimumFactor;
                p.quake_quake_minimum_factor = fields.mQuakeMinimumFactor;
                p.quake_speed = fields.mSpeed;
                p.quake_falloff = fields.mFalloff;
                p.quake_damage_factor = media.quake_damage_factor;
            }
            kind::HURRICANE => {
                if media.has(1) {
                    p.flags |= EE_CALAMITY_EFFECT1;
                    p.storm_delay = media.duration[0];
                }
                p.storm_damage = fields.mDamage as i32;
                p.storm_tick_time = fields.mTickTime as u32;
                p.storm_radius = fields.mRadius;
            }
            kind::LOGIC_BOMB => {
                p.flags |= effect2;
                p.bomb_time2 = fields.mEffectTime2 as u32;
                p.bomb_time3 = fields.mEffectTime3 as u32;
            }
            kind::METEOR_STORM => {
                p.meteor_minimum_interval = fields.mMinimumInterval;
                p.meteor_maximum_interval = fields.mMaximumInterval;
            }
            kind::MORALE => p.morale_delay = fields.mEffectDuration as i32,
            kind::TELEPORT => p.teleport_hold_time = fields.mHoldTime as i32,
            kind::TIME_WARP => {
                p.warp_depart_time = fields.mDepartTime as i32;
                p.warp_away_time = fields.mAwayTime as i32;
            }
            kind::VOLCANO => p.volcano_delay = media.time1[0],
            _ => {}
        }
    } else {
        match class_enum {
            kind::AREA_EFFECT
            | kind::AE_EXTRA_DAMAGE
            | kind::AE_MIND_CONTROL
            | kind::DROP_OBJECT
            | kind::FIRE
            | kind::LOGIC_BOMB
            | kind::TIME_WARP => p.flags |= effect1,
            kind::EARTHQUAKE => p.quake_source_speed = fields.mSourceSpeed,
            kind::HURRICANE => {
                p.start_size = media.size[1];
                p.flags |= effect1;
            }
            kind::LINE_OF_SIGHT => p.sight_bonus = fields.mSightBonus,
            kind::MORALE => {
                p.flags |= effect1;
                if fields.mPositive {
                    p.flags |= EE_CALAMITY_POSITIVE;
                }
            }
            kind::VOLCANO => p.start_size = media.size[0],
            _ => {}
        }
        if which == EE_CALAMITY_RUN_STOP {
            runs = !matches!(
                class_enum,
                kind::CONSCRIPTION | kind::METEOR_STORM | kind::SPECIAL_ATTACK | kind::SPECIAL_LOCATION
            );
        }
    }
    EeCalamityClassRun { params: p, sounds, runs }
}

/// The calamity's members a class's `Reset(calamity)` reads and sets, in and out, and the target unit's view.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCalamityReset {
    /// In: the calamity has a target unit; its tile (x -10000 off the map) and hit points.
    pub has_target: bool,
    pub target_tile_x: c_long,
    pub target_tile_y: c_long,
    pub target_hit_points: c_long,
    /// In and out: the calamity's placeholders and remembered tile.
    pub bool1: bool,
    pub bool2: bool,
    pub float1: f32,
    pub int1: c_long,
    pub int2: c_long,
    pub tile_x: c_long,
    pub tile_y: c_long,
    /// Out: `int1` is to be a new contagion helper of the class's functor for the calamity and its target.
    pub create_helper: bool,
}

/// `Reset(calamity)` of the class.
pub fn reset(class_enum: i32, r: &mut EeCalamityReset) {
    match class_enum {
        kind::AE_EXTRA_DAMAGE | kind::AE_MIND_CONTROL | kind::TELEPORT => {
            r.bool1 = false;
            r.bool2 = false;
        }
        kind::EARTHQUAKE => r.float1 = 0.0,
        kind::FIRE | kind::PLAGUE => r.create_helper = true,
        kind::HURRICANE => r.int1 = 0,
        kind::LEPROSY | kind::MORALE => r.bool1 = false,
        kind::TIME_WARP => {
            if r.has_target {
                if r.target_tile_x != -10000 && r.target_hit_points > 0 {
                    r.tile_x = r.target_tile_x;
                    r.tile_y = r.target_tile_y;
                    r.bool1 = false;
                    r.bool2 = false;
                } else {
                    r.int2 = -1;
                    r.bool1 = true;
                    r.bool2 = true;
                }
            }
            r.float1 = 0.0;
        }
        _ => {}
    }
}

/// Whether the unit class's `IsValidTarget` asked its functor (`CanInfect`) instead of the shared rule.
pub fn targets_by_functor(class_enum: i32) -> bool {
    matches!(class_enum, kind::FIRE | kind::PLAGUE)
}

/// The location class's own fields of its `IsValidLocation` question (`EeCalamityCast`): the radius in tiles, the
/// hurricane's water flag and map area, the meteor storm's space terrain (`space_terrain`, the C++'s
/// `kTerrainSpace`); and the unit class it places (the hurricane's and the volcano's, else 0).
pub fn cast_fields(
    fields: &EeCalamityClassFields,
    cast: &mut crate::calamity_cast::EeCalamityCast,
    space_terrain: i32,
) -> c_long {
    match fields.mClassEnum as i32 {
        kind::AREA_EFFECT | kind::AE_EXTRA_DAMAGE | kind::AE_MIND_CONTROL | kind::LEPROSY => {
            cast.radius = fields.mRadiusTiles as u32;
            0
        }
        kind::HURRICANE => {
            cast.over_water = fields.mOverWater;
            cast.area = fields.mArea as i32;
            fields.mUnitClassIndex
        }
        kind::METEOR_STORM => {
            cast.space_terrain = space_terrain;
            0
        }
        kind::VOLCANO => fields.mUnitClassIndex,
        _ => 0,
    }
}

// ---- the C ABI (`EECalamityClass.cpp`) -----------------------------------------------------------------------------
// SAFETY (every function below): the caller passes valid, aligned pointers for the documented reads and writes; the
// records are copied in and out, so no reference into C++ memory lives past the call.

/// [`get_mode`].
#[no_mangle]
pub extern "C" fn ee_calamity_class_get_mode(class_enum: i32, slot: u32) -> u32 {
    get_mode(class_enum, slot)
}

/// [`fetches`].
#[no_mangle]
pub extern "C" fn ee_calamity_class_fetches(mode: u32, id: i32) -> bool {
    fetches(mode, id)
}

/// [`initialize_base`] into `*fields`.
///
/// # Safety
/// `fields` valid for reads and writes, `data` for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_calamity_class_initialize_base(
    fields: *mut EeCalamityClassFields,
    data: *const EeCalamityData,
) {
    // SAFETY: the caller's contract
    let (mut f, d) = unsafe { (*fields, *data) };
    initialize_base(&mut f, &d);
    // SAFETY: as above
    unsafe { *fields = f };
}

/// [`fill`] into `*fields`.
///
/// # Safety
/// `fields` valid for reads and writes, `data` and `media` for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_calamity_class_fill(
    fields: *mut EeCalamityClassFields,
    data: *const EeCalamityData,
    media: *const EeCalamityClassMedia,
) {
    // SAFETY: the caller's contract
    let (mut f, d, m) = unsafe { (*fields, *data, *media) };
    fill(&mut f, &d, &m);
    // SAFETY: as above
    unsafe { *fields = f };
}

/// [`functor_kind`].
#[no_mangle]
pub extern "C" fn ee_calamity_class_functor_kind(class_enum: i32) -> u32 {
    functor_kind(class_enum)
}

/// [`media_step`].
#[no_mangle]
pub extern "C" fn ee_calamity_class_media_step(class_enum: i32, list: u32, index: u32) -> u32 {
    media_step(class_enum, list, index)
}

/// [`sound_mode`].
#[no_mangle]
pub extern "C" fn ee_calamity_class_sound_mode(class_enum: i32) -> u32 {
    sound_mode(class_enum)
}

/// [`run`].
///
/// # Safety
/// `fields` and `media` valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_calamity_class_run(
    fields: *const EeCalamityClassFields,
    media: *const EeCalamityClassMedia,
    which: u32,
) -> EeCalamityClassRun {
    // SAFETY: the caller's contract
    let (f, m) = unsafe { (*fields, *media) };
    run(&f, &m, which)
}

/// [`reset`] over `*state`.
///
/// # Safety
/// `state` valid for reads and writes.
#[no_mangle]
pub unsafe extern "C" fn ee_calamity_class_reset(class_enum: i32, state: *mut EeCalamityReset) {
    // SAFETY: the caller's contract
    let mut r = unsafe { *state };
    reset(class_enum, &mut r);
    // SAFETY: as above
    unsafe { *state = r };
}

/// [`targets_by_functor`].
#[no_mangle]
pub extern "C" fn ee_calamity_class_targets_by_functor(class_enum: i32) -> bool {
    targets_by_functor(class_enum)
}

/// [`cast_fields`] into `*cast`; the unit class it places.
///
/// # Safety
/// `fields` valid for reads, `cast` for reads and writes.
#[no_mangle]
pub unsafe extern "C" fn ee_calamity_class_cast_fields(
    fields: *const EeCalamityClassFields,
    cast: *mut crate::calamity_cast::EeCalamityCast,
    space_terrain: i32,
) -> c_long {
    // SAFETY: the caller's contract
    let (f, mut c) = unsafe { (*fields, *cast) };
    let unit_class = cast_fields(&f, &mut c, space_terrain);
    // SAFETY: as above
    unsafe { *cast = c };
    unit_class
}

#[cfg(test)]
#[path = "calamity_class_tests.rs"]
mod tests;
