//! The contagion functors (`EEFireFunctor.cpp`, `EELeprosyFunctor.cpp`, `EEPlagueFunctor.cpp`,
//! `EEAreaEffectFunctor.cpp`, `EEAEExtraDamageFunctor.cpp`, `EEAEMindControlFunctor.cpp`; 0xac7b9..0xacb75 and
//! 0xb6bd1..0xb70cc) as one record, [`EeContagionFunctorFields`], keyed by the functor's kind (the
//! `EE_CALAMITY_FUNCTOR_*` the class's `Initialize` made, [`crate::calamity_class::functor_kind`]).
//!
//! - [`init`]: the numbers each kind's constructor stored, from its class's record (what `Initialize` passed);
//! - [`params`]: the getters the helper's update reads (`GetSpreadTime`, `GetChance`, `GetRadius`, `SkipsSpread`,
//!   `InfectsAlliesOnly`), each kind's override or the base's default;
//! - [`discrete_time`]: `Update`'s discrete update time; [`apply_damage`]: `Apply`'s damage by kind (the arithmetic
//!   is [`crate::contagion`]'s).
//!
//! The functor's effects and its world steps (`Infect`, `Cure`, `Apply`: the units' flags, effects, damage, orders)
//! are the C++ `EEContagionFunctor`'s (`EEContagionManager.cpp`), one switch by kind.

// the record keeps the C++ member names; c_long/c_ulong are 32 bits on Windows
#![allow(non_snake_case, clippy::unnecessary_cast)]

use crate::tech::calamity_class::{
    EeCalamityClassFields, EE_CALAMITY_FUNCTOR_AREA, EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE, EE_CALAMITY_FUNCTOR_FIRE,
    EE_CALAMITY_FUNCTOR_LEPROSY, EE_CALAMITY_FUNCTOR_MIND_CONTROL, EE_CALAMITY_FUNCTOR_PLAGUE,
};
use crate::tech::contagion::{
    area_damage, leprosy_damage, tick_damage, EeContagionParams, EE_CONTAGION_ALLIES_ONLY,
    EE_CONTAGION_BLOCKED_BY_IMMUNITY, EE_CONTAGION_SKIPS_SPREAD,
};
use std::os::raw::{c_long, c_ulong};

/// A functor's numbers (the six classes' members, each under its C++ name; the plague's `mDamageTime` and the area
/// effects' `mUpdateTime` are `mSpreadTime`, the area effects' unit set `mValue` is `mUnitSet`, the extra damage's
/// flag `mFlag`). Its effects are C++ members of `EEContagionFunctor`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeContagionFunctorFields {
    /// The functor's kind (`EE_CALAMITY_FUNCTOR_*`).
    pub mKind: u32,
    /// The spread radius (the fire's, the plague's).
    pub mRadius: f32,
    /// The spread chance (the fire's, the plague's).
    pub mChance: f32,
    /// The fire's burn: hundredths of the unit's maximum hit points burnt at once.
    pub mBurn: c_long,
    /// The damage per discrete update (the area effects': the factor of the unit's attack stat).
    pub mDamage: c_long,
    /// The discrete update time (the fire's and the leprosy's spread time, the plague's damage time, the area
    /// effects' update time).
    pub mSpreadTime: c_ulong,
    /// The unit set it can infect.
    pub mUnitSet: c_ulong,
    /// `TakeHit`'s mode: 2 when the class is blocked by immunity, else 3.
    pub mMode: c_long,
    /// The calamity's discrete updates of this helper update (`Update`).
    pub mUpdates: c_ulong,
    /// The extra damage's flag: the pulse cannon effect, else the battle cry.
    pub mFlag: bool,
}

/// The functor of `kind` for the class (`Initialize`'s `new`, its arguments from the class's record; `mode`: 2 when
/// the class is blocked by immunity, else 3). The mind control and the extra damage pass no damage and no update
/// time to their area effect base.
pub fn init(class: &EeCalamityClassFields, kind: u32) -> EeContagionFunctorFields {
    let mut f = EeContagionFunctorFields {
        mKind: kind,
        mUnitSet: class.mUnitSet,
        mMode: if class.mIsBlockedByImmunity { 2 } else { 3 },
        ..EeContagionFunctorFields::default()
    };
    match kind {
        EE_CALAMITY_FUNCTOR_AREA => {
            f.mDamage = class.mDamageFactor;
            f.mSpreadTime = class.mSpreadTime as c_ulong;
        }
        EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE => f.mFlag = class.mFunctorFlag,
        EE_CALAMITY_FUNCTOR_FIRE => {
            f.mBurn = class.mBurn;
            f.mRadius = class.mRadius;
            f.mChance = class.mChance;
            f.mDamage = class.mDamage;
            f.mSpreadTime = class.mSpreadTime as c_ulong;
        }
        EE_CALAMITY_FUNCTOR_LEPROSY => {
            f.mDamage = class.mDamage;
            f.mSpreadTime = class.mSpreadTime as c_ulong;
        }
        EE_CALAMITY_FUNCTOR_PLAGUE => {
            f.mRadius = class.mRadius;
            f.mChance = class.mChance;
            f.mDamage = class.mDamage;
            f.mSpreadTime = class.mSpreadTime as c_ulong;
        }
        _ => {}
    }
    f
}

/// What the helper's update reads of the functor: the fire's spread time, chance and radius; the plague's 1000 ms,
/// chance and radius; 0 for the others (the base's defaults). The leprosy and the area effects skip the spread; the
/// plague infects enemies too.
pub fn params(f: &EeContagionFunctorFields, blocked_by_immunity: bool) -> EeContagionParams {
    let spreads = matches!(f.mKind, EE_CALAMITY_FUNCTOR_FIRE | EE_CALAMITY_FUNCTOR_PLAGUE);
    let skips = matches!(
        f.mKind,
        EE_CALAMITY_FUNCTOR_LEPROSY
            | EE_CALAMITY_FUNCTOR_AREA
            | EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE
            | EE_CALAMITY_FUNCTOR_MIND_CONTROL
    );
    EeContagionParams {
        spread_time: match f.mKind {
            EE_CALAMITY_FUNCTOR_FIRE => f.mSpreadTime as u32,
            EE_CALAMITY_FUNCTOR_PLAGUE => 1000,
            _ => 0,
        },
        chance: if spreads { f.mChance } else { 0.0 },
        radius: if spreads { f.mRadius } else { 0.0 },
        flags: (if skips { EE_CONTAGION_SKIPS_SPREAD } else { 0 })
            | (if f.mKind != EE_CALAMITY_FUNCTOR_PLAGUE { EE_CONTAGION_ALLIES_ONLY } else { 0 })
            | (if blocked_by_immunity { EE_CONTAGION_BLOCKED_BY_IMMUNITY } else { 0 }),
    }
}

/// `Update`'s discrete update time for the calamity (`mDiscreteUpdateTime = ...`): the stored time converted as its
/// C++ member's type (the area effects' `long mUpdateTime`, the others' `unsigned long`).
pub fn discrete_time(f: &EeContagionFunctorFields) -> f32 {
    match f.mKind {
        EE_CALAMITY_FUNCTOR_AREA | EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE | EE_CALAMITY_FUNCTOR_MIND_CONTROL => {
            f.mSpreadTime as c_long as f32
        }
        _ => f.mSpreadTime as f32,
    }
}

/// What `Apply` does to a unit ([`apply_damage`]): whether it hits and the damage.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeContagionFunctorHit {
    /// The unit takes a hit.
    pub hits: bool,
    /// Its damage.
    pub damage: i32,
}

/// `Apply`'s hit on a unit with `hit_points` and attack stat `attack`, only after a discrete update: the fire's and
/// the plague's tick (whatever it is), the leprosy's (never below 1 hit point left) and the area effect's (times the
/// attack stat) when above 1; none for the mind control and the extra damage.
pub fn apply_damage(f: &EeContagionFunctorFields, hit_points: i32, attack: f32) -> EeContagionFunctorHit {
    if f.mUpdates == 0 {
        return EeContagionFunctorHit::default();
    }
    let (damage, updates) = (f.mDamage as i32, f.mUpdates as u32);
    let damage = match f.mKind {
        EE_CALAMITY_FUNCTOR_FIRE | EE_CALAMITY_FUNCTOR_PLAGUE => {
            return EeContagionFunctorHit { hits: true, damage: tick_damage(damage, updates) }
        }
        EE_CALAMITY_FUNCTOR_LEPROSY => leprosy_damage(damage, updates, hit_points),
        EE_CALAMITY_FUNCTOR_AREA => area_damage(damage, updates, attack),
        _ => 0,
    };
    EeContagionFunctorHit { hits: damage != 0, damage }
}

/// [`init`] into `*out`.
///
/// # Safety
/// `class` valid for reads, `out` for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_functor_init(
    out: *mut EeContagionFunctorFields,
    class: *const EeCalamityClassFields,
    kind: u32,
) {
    // SAFETY: the caller's contract
    unsafe { *out = init(&*class, kind) };
}

/// [`params`].
///
/// # Safety
/// `f` valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_functor_params(
    f: *const EeContagionFunctorFields,
    blocked_by_immunity: bool,
) -> EeContagionParams {
    // SAFETY: the caller's contract
    params(unsafe { &*f }, blocked_by_immunity)
}

/// [`discrete_time`].
///
/// # Safety
/// `f` valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_functor_discrete_time(f: *const EeContagionFunctorFields) -> f32 {
    // SAFETY: the caller's contract
    discrete_time(unsafe { &*f })
}

/// [`apply_damage`].
///
/// # Safety
/// `f` valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_functor_apply_damage(
    f: *const EeContagionFunctorFields,
    hit_points: i32,
    attack: f32,
) -> EeContagionFunctorHit {
    // SAFETY: the caller's contract
    apply_damage(unsafe { &*f }, hit_points, attack)
}

#[cfg(test)]
#[path = "contagion_functor_tests.rs"]
mod tests;
