//! The units' stats (RUST_PLAN R5b.4): `EEUnitClassUpgrade`'s plain members, a player's upgrade of a unit class
//! (`mStats` of every unit of that class and player), as a Rust record the C++ class derives from (the
//! twenty-eighth round: `class EEUnitClassUpgrade : public EeUnitsUnitClassUpgradeFields`, ee/EEUnit.h), as
//! unit_class_fields.rs did for the class. The fields keep their C++ names and types (`c_long`/`c_ulong` are the
//! C++ `long`/`unsigned long` on every data model) and their order (+0x84 `mAreaEffectScale` .. +0x168
//! `mIsInitialized` in the original); the two volatile busy flags stay C++ members (a Rust field cannot be volatile).
//!
//! The readers (the combat reads through ee_rust's bridge, the update and `ActionChanged` hosts) read the record in
//! place by the stats object's address ([`stats_of`]): the fourteenth round's table of reported copies (its
//! `EEUnitStatsScope` at every writer, its versions, its USCK check) is gone. `Init`'s copies from the class's record
//! ([`init`], [`init_finish`]) and `SetMaxVelocity` ([`set_max_velocity`]) are Rust.

#![allow(non_snake_case)]

use crate::unit_class_fields::EeUnitsUnitClassFields;
use std::os::raw::{c_long, c_ulong};
use std::sync::atomic::{AtomicU64, Ordering};

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

/// The stats the Rust readers take from a stats object (floats as their bits), read in place ([`stats_of`]).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeUnitStatsRecord {
    /// `mScale`, `mRange`, `mMaxVelocity`, `mAttack` (float bits)
    pub scale: u32,
    pub range: u32,
    pub max_velocity: u32,
    pub attack: u32,
    /// `mArmor[6]` (float bits)
    pub armor: [u32; 6],
    /// `mGatherTimes[7]`
    pub gather_times: [i64; 7],
    /// `mFlightTime`, `mReloadTime`, `mMaxHitPoints`, `mPower`, `mHealingPoints`, `mRegeneration` (`long`s)
    pub flight_time: i64,
    pub reload_time: i64,
    pub max_hit_points: i64,
    pub power: i64,
    pub healing_points: i64,
    pub regeneration: i64,
    /// `mCanFireOverWalls` (its byte as 0 or 1)
    pub can_fire_over_walls: u8,
}

/// The record base's offset in `EEUnitClassUpgrade` (the C++ sets it with the store's slots; 0 until then, as in
/// the unit tests, whose objects are records).
static FIELDS_OFFSET: AtomicU64 = AtomicU64::new(0);

/// Where the record sits in a stats object (store.rs's slots).
pub fn set_fields_offset(offset: u64) {
    FIELDS_OFFSET.store(offset, Ordering::Relaxed);
}

/// The record of a stats object (an `EEUnitClassUpgrade*`'s address).
pub fn fields_of(object: u64) -> *const EeUnitsUnitClassUpgradeFields {
    (object as usize + FIELDS_OFFSET.load(Ordering::Relaxed) as usize) as *const EeUnitsUnitClassUpgradeFields
}

/// One member of the record at `$p`, read in place.
macro_rules! read {
    ($p:expr, $f:ident) => {
        // SAFETY: the caller's pointer names a live record
        unsafe { std::ptr::read_volatile(std::ptr::addr_of!((*$p).$f)) }
    };
}

/// The stats of a stats object (a unit record's `stats`), read from its record in place; None for none.
///
/// The object is live while a live unit points to it (its player's upgrades outlive the player's units).
pub fn stats_of(object: u64) -> Option<EeUnitStatsRecord> {
    if object == 0 {
        return None;
    }
    let p = fields_of(object);
    let armor: [f32; 6] = read!(p, mArmor);
    let gather: [c_long; 7] = read!(p, mGatherTimes);
    Some(EeUnitStatsRecord {
        scale: read!(p, mScale).to_bits(),
        range: read!(p, mRange).to_bits(),
        max_velocity: read!(p, mMaxVelocity).to_bits(),
        attack: read!(p, mAttack).to_bits(),
        armor: armor.map(f32::to_bits),
        gather_times: gather.map(i64::from),
        flight_time: read!(p, mFlightTime) as i64,
        reload_time: read!(p, mReloadTime) as i64,
        max_hit_points: read!(p, mMaxHitPoints) as i64,
        power: read!(p, mPower) as i64,
        healing_points: read!(p, mHealingPoints) as i64,
        regeneration: read!(p, mRegeneration) as i64,
        // SAFETY: a live record
        can_fire_over_walls: unsafe { crate::unit_fields::bool_byte(std::ptr::addr_of!((*p).mCanFireOverWalls)) },
    })
}

/// A `bool` member's byte copied as the C++ copies a `bool` (a byte other than 0/1 is never a Rust `bool`).
///
/// # Safety
/// Both point to live `bool` members.
unsafe fn copy_bool(to: *mut bool, from: *const bool) {
    // SAFETY: the caller's contract
    unsafe { std::ptr::write(to.cast::<u8>(), crate::unit_fields::bool_byte(from)) }
}

/// `EEUnitClassUpgrade::Init`'s copies from the class before `Reapply` (the original's statements in order; the
/// media setters and `SetRange` stay C++, which write none of these): the gather times, the scale, the speeds, the
/// armour, attack and the other numbers, the flags, the conversions and scales, `mBC` and the inverses.
pub fn init(u: &mut EeUnitsUnitClassUpgradeFields, c: &EeUnitsUnitClassFields) {
    u.mGatherTimes = c.mGatherTimes;
    u.mScale = c.mScale;
    u.mAreaEffect = c.mAreaEffect;
    u.mLineOfSight = c.mLineOfSight;
    u.mAcceleration = c.mAcceleration;
    u.mMaxVelocity = c.mMaxVelocity;
    u.mAngularAcceleration = c.mAngularAcceleration;
    u.mMaxAngularVelocity = c.mMaxAngularVelocity;
    u.mTurretMaxAngularVelocity = c.mTurretMaxAngularVelocity;
    for i in 0..6 {
        u.mArmor[i] = c.mArmor[i] as f32;
    }
    u.mReloadTime = c.mReloadTime;
    u.mAttack = c.mAttack as f32;
    u.mWorkRate = c.mWorkRate;
    u.mFlightTime = c.mFlightTime;
    u.mPowerRegeneration = c.mPowerRegeneration;
    u.mLeadership = c.mLeadership as f32;
    u.mMaxObjectCarry = c.mMaxObjectCarry;
    u.mRepairRate = c.mRepairRate;
    u.mMaxHitPoints = c.mHitPoints;
    u.mCarryAmount = c.mCarryAmount;
    u.mHealingPoints = c.mHealingPoints;
    // SAFETY: members of live records
    unsafe {
        copy_bool(&mut u.mCanFireOverWalls, &c.mCanFireOverWalls);
        copy_bool(&mut u.mBuildsOnElevation, &c.mBuildsOnElevation);
        copy_bool(&mut u.mConvertsBuildings, &c.mConvertsBuildings);
        copy_bool(&mut u.mConvertsPriests, &c.mConvertsPriests);
    }
    u.mConversionResistance = c.mConversionResistance;
    u.mConversionRate = c.mConversionRate;
    u.mCostScale = c.mCostScale;
    u.mBuildTimeScale = c.mBuildTimeScale;
    u.mAreaEffectScale = c.mAreaEffectScale;
    u.mPower = c.mPower;
    u.mRegeneration = c.mRegeneration;
    u.mBC = if u.mAcceleration < u.mMaxVelocity { u.mAcceleration } else { u.mMaxVelocity };
    u.mInverseAcceleration = 1.0f32 / u.mAcceleration;
    u.mInverseAngularAcceleration = 1.0f32 / u.mAngularAcceleration;
    u.mInverseMaxAngularVelocity = 1.0f32 / u.mMaxAngularVelocity;
    u.mInverseMaxVelocity = 1.0f32 / u.mMaxVelocity;
    u.mInverseTurretMaxAngularVelocity = 1.0f32 / u.mTurretMaxAngularVelocity;
}

/// `Init` after `Reapply`: the area-effect templates, the wonder unit set, initialised, the camera focus (0.5) and
/// the terrain mask (a bit per terrain the class crosses; 0x4000 added for a forest crosser).
pub fn init_finish(u: &mut EeUnitsUnitClassUpgradeFields, c: &EeUnitsUnitClassFields) {
    u.mAreaEffectTemplates = c.mAreaEffectTemplates;
    u.mWonderUnitSet = c.mWonderUnitSet;
    u.mIsInitialized = true;
    u.mCameraFocus = 0.5f32;
    u.mTerrainMask = 0;
    for j in 0..0x11u32 {
        // SAFETY: a member of a live record
        if unsafe { crate::unit_fields::bool_byte(&c.mTerrainTypes[j as usize]) } == 0 {
            continue;
        }
        let bit: c_ulong = 1 << j;
        if bit & u.mTerrainMask == 0 {
            u.mTerrainMask |= bit;
        }
    }
    // SAFETY: as above
    if unsafe { crate::unit_fields::bool_byte(&c.mCanCrossForest) } != 0 && (u.mTerrainMask >> 0xe) & 1 == 0 {
        u.mTerrainMask |= 0x4000;
    }
}

/// `EEUnitClassUpgrade::SetMaxVelocity`: the top speed, the acceleration and turn rates (turret too) scaled by its
/// ratio to the class's, and the inverses.
pub fn set_max_velocity(u: &mut EeUnitsUnitClassUpgradeFields, c: &EeUnitsUnitClassFields, value: f32) {
    let ratio = value / c.mMaxVelocity;
    u.mMaxVelocity = value;
    u.mAcceleration = c.mAcceleration * ratio;
    u.mAngularAcceleration = c.mAngularAcceleration * ratio;
    u.mMaxAngularVelocity = c.mMaxAngularVelocity * ratio;
    u.mTurretMaxAngularVelocity = c.mTurretMaxAngularVelocity * ratio;
    u.mInverseAcceleration = 1.0f32 / u.mAcceleration;
    u.mInverseAngularAcceleration = 1.0f32 / u.mAngularAcceleration;
    u.mInverseMaxAngularVelocity = 1.0f32 / u.mMaxAngularVelocity;
    u.mInverseMaxVelocity = 1.0f32 / value;
    u.mInverseTurretMaxAngularVelocity = 1.0f32 / u.mTurretMaxAngularVelocity;
}

/// `Init`'s copies before `Reapply` ([`init`]).
///
/// # Safety
/// `upgrade` and `class` point to live records; no other reference to them is held during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_upgrade_init(
    upgrade: *mut EeUnitsUnitClassUpgradeFields,
    class: *const EeUnitsUnitClassFields,
) {
    // SAFETY: the caller's contract
    unsafe { init(&mut *upgrade, &*class) }
}

/// `Init`'s copies after `Reapply` ([`init_finish`]).
///
/// # Safety
/// As [`ee_units_upgrade_init`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_upgrade_init_finish(
    upgrade: *mut EeUnitsUnitClassUpgradeFields,
    class: *const EeUnitsUnitClassFields,
) {
    // SAFETY: the caller's contract
    unsafe { init_finish(&mut *upgrade, &*class) }
}

/// `SetMaxVelocity` ([`set_max_velocity`]).
///
/// # Safety
/// As [`ee_units_upgrade_init`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_upgrade_set_max_velocity(
    upgrade: *mut EeUnitsUnitClassUpgradeFields,
    class: *const EeUnitsUnitClassFields,
    value: f32,
) {
    // SAFETY: the caller's contract
    unsafe { set_max_velocity(&mut *upgrade, &*class, value) }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn class() -> Box<EeUnitsUnitClassFields> {
        let mut c = Box::<EeUnitsUnitClassFields>::default();
        c.mGatherTimes = [1, 2, 3, 4, 5, 6, 7];
        c.mScale = 1.25;
        c.mAreaEffect = 0.5;
        c.mLineOfSight = 9.0;
        c.mAcceleration = 3.0;
        c.mMaxVelocity = 2.0;
        c.mAngularAcceleration = 4.0;
        c.mMaxAngularVelocity = 8.0;
        c.mTurretMaxAngularVelocity = 0.0;
        c.mArmor = [1, -2, 3, 16_777_217, 5, 6];
        c.mReloadTime = 2000;
        c.mAttack = 7;
        c.mLeadership = 3;
        c.mHitPoints = 120;
        c.mHealingPoints = 4;
        c.mPower = 50;
        c.mRegeneration = 2;
        c.mCanFireOverWalls = true;
        c.mConvertsPriests = true;
        c.mAreaEffectTemplates = [9, 8, 7, 6];
        c.mWonderUnitSet = 11;
        c.mTerrainTypes[0] = true;
        c.mTerrainTypes[3] = true;
        c.mTerrainTypes[16] = true;
        c.mCanCrossForest = true;
        c
    }

    #[test]
    fn init_copies_the_class_as_the_original() {
        let c = class();
        let mut u = EeUnitsUnitClassUpgradeFields { mRange: 6.0, mLaunchSpeed: 1.5, ..Default::default() };
        init(&mut u, &c);
        assert_eq!(u.mGatherTimes, [1, 2, 3, 4, 5, 6, 7]);
        assert_eq!((u.mScale, u.mMaxVelocity, u.mAcceleration), (1.25, 2.0, 3.0));
        // (float)long: round to nearest even past 2^24
        assert_eq!(u.mArmor, [1.0, -2.0, 3.0, 16_777_216.0, 5.0, 6.0]);
        assert_eq!((u.mAttack, u.mLeadership, u.mMaxHitPoints, u.mReloadTime), (7.0, 3.0, 120, 2000));
        assert!(u.mCanFireOverWalls && u.mConvertsPriests && !u.mBuildsOnElevation && !u.mConvertsBuildings);
        // the smaller of acceleration and top speed; the inverses, a zero turret rate's infinite
        assert_eq!(u.mBC, 2.0);
        assert_eq!((u.mInverseAcceleration, u.mInverseMaxVelocity), (1.0 / 3.0, 0.5));
        assert_eq!(u.mInverseTurretMaxAngularVelocity, f32::INFINITY);
        // SetRange's two are not Init's copies
        assert_eq!((u.mRange, u.mLaunchSpeed), (6.0, 1.5));
        assert!(!u.mIsInitialized);
        init_finish(&mut u, &c);
        assert_eq!((u.mAreaEffectTemplates, u.mWonderUnitSet), ([9, 8, 7, 6], 11));
        assert!(u.mIsInitialized);
        assert_eq!(u.mCameraFocus, 0.5);
        assert_eq!(u.mTerrainMask, 1 | 8 | 0x1_0000 | 0x4000);
        // a forest terrain already set: no second bit
        let mut c2 = class();
        c2.mTerrainTypes[14] = true;
        init_finish(&mut u, &c2);
        assert_eq!(u.mTerrainMask, 1 | 8 | 0x1_0000 | 0x4000);
        c2.mCanCrossForest = false;
        c2.mTerrainTypes[14] = false;
        init_finish(&mut u, &c2);
        assert_eq!(u.mTerrainMask, 1 | 8 | 0x1_0000);
    }

    #[test]
    fn set_max_velocity_scales_by_the_ratio() {
        let c = class();
        let mut u = EeUnitsUnitClassUpgradeFields::default();
        init(&mut u, &c);
        set_max_velocity(&mut u, &c, 3.0);
        let ratio = 3.0f32 / 2.0;
        assert_eq!(u.mMaxVelocity, 3.0);
        assert_eq!(u.mAcceleration, 3.0 * ratio);
        assert_eq!(u.mMaxAngularVelocity, 8.0 * ratio);
        assert_eq!(u.mInverseMaxVelocity, 1.0 / 3.0);
        assert_eq!(u.mInverseAngularAcceleration, 1.0 / (4.0 * ratio));
        // mBC is Init's only
        assert_eq!(u.mBC, 2.0);
    }

    #[test]
    fn stats_are_read_in_place() {
        let c = class();
        let mut u = Box::<EeUnitsUnitClassUpgradeFields>::default();
        init(&mut u, &c);
        u.mRange = 4.5;
        let object = &*u as *const EeUnitsUnitClassUpgradeFields as u64;
        let s = stats_of(object).unwrap();
        assert_eq!(f32::from_bits(s.range), 4.5);
        assert_eq!(f32::from_bits(s.attack), 7.0);
        assert_eq!(f32::from_bits(s.armor[1]), -2.0);
        assert_eq!((s.reload_time, s.max_hit_points, s.power, s.regeneration), (2000, 120, 50, 2));
        assert_eq!((s.gather_times[6], s.can_fire_over_walls), (7, 1));
        // a write is seen at the next read (no report)
        u.mMaxHitPoints = 150;
        assert_eq!(stats_of(object).unwrap().max_hit_points, 150);
        assert_eq!(stats_of(0), None);
    }
}
