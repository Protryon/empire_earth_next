//! The scenario host's unit reads answered from the world views (calamities and scenario lane, round 4): what
//! `EESMUnitField` (EESMObject.cpp) read of a unit, its complex unit, its upgrade and a class for
//! `EESMObject::GetUnitAttribute` ([`EE_SCENARIO_READ_UNIT_FIELD`]). The C++ keeps the two members no record holds
//! (the unit variables' vector, the physical model's scale: [`unit_field`] answers None for them).
//!
//! The answers are the C++'s `int64_t` conversions: a `long` sign-extended, an `unsigned long` zero-extended, a
//! `(long)` cast of an `unsigned long` sign-extended from 32 bits, a `bool` 0 or 1, a `float` its bits.

// the records keep the C++ member names; c_long/c_ulong are 32 bits on Windows
#![allow(non_snake_case, clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::scenario::attribute::*;

/// The body of [`unit_field`] over the records of one source: `$get!(record, member)` reads a member, `$unit` is the
/// unit's record, `$complex`, `$stats` and `$class` (the unit's class, or the upgrade's for a field past
/// `EE_SCENARIO_OF_STATS`) options of theirs, `$cargo` the cargo count.
macro_rules! unit_field_body {
    ($field:expr, $unit:expr, $complex:expr, $stats:expr, $class:expr, $cargo:expr, $get:ident) => {{
        let field: i32 = $field;
        let bits = |v: f32| v.to_bits() as i64;
        let f = if field >= EE_SCENARIO_OF_STATS { field - EE_SCENARIO_OF_STATS } else { field };
        let c = $complex;
        let s = $stats;
        let k = $class;
        if (EE_SCENARIO_UNIT_VARIABLE..EE_SCENARIO_UNIT_VARIABLE + 3).contains(&f) || f == EE_SCENARIO_UNIT_SCALE {
            return None;
        }
        if (EE_SCENARIO_STATS_ARMOR..EE_SCENARIO_STATS_ARMOR + 6).contains(&f) {
            let armor: [f32; 6] = $get!(s?, mArmor);
            return Some(bits(armor[(f - EE_SCENARIO_STATS_ARMOR) as usize]));
        }
        if (EE_SCENARIO_CLASS_ARMOR..EE_SCENARIO_CLASS_ARMOR + 6).contains(&f) {
            let armor: [c_long; 6] = $get!(k?, mArmor);
            return Some(armor[(f - EE_SCENARIO_CLASS_ARMOR) as usize] as i64);
        }
        Some(match f {
            EE_SCENARIO_UNIT_HAS_COMPLEX => c.is_some() as i64,
            EE_SCENARIO_UNIT_HAS_STATS => s.is_some() as i64,
            EE_SCENARIO_UNIT_HAS_CLASS => k.is_some() as i64,
            EE_SCENARIO_UNIT_HIT_POINTS => $get!($unit, mHitPoints) as i64,
            EE_SCENARIO_UNIT_RESOURCE_TYPE => $get!($unit, mResourceType) as i64,
            EE_SCENARIO_UNIT_RESOURCE_AMOUNT => $get!($unit, mResourceAmount) as i64,
            EE_SCENARIO_UNIT_FLIGHT_TIME => $get!(c?, mFlightTime) as i64,
            EE_SCENARIO_UNIT_CARGO => $cargo as c_long as i64,
            EE_SCENARIO_UNIT_GARRISON => $get!(c?, mGarrisonCount) as i64,
            EE_SCENARIO_UNIT_AMMUNITION => $get!(c?, mAmmunition) as c_long as i64,
            EE_SCENARIO_UNIT_PAUSED => $get!(c?, mIsAnimationPaused) as i64,
            EE_SCENARIO_UNIT_RELOAD_TIMER => $get!(c?, mReloadTimer) as i64,
            EE_SCENARIO_UNIT_POWER => $get!(c?, mPower) as i64,
            EE_SCENARIO_UNIT_ROTTING => $get!(c?, mRottingAwayTime) as i64,
            EE_SCENARIO_UNIT_BUILT => $get!(c?, mIsBuilt) as i64,
            EE_SCENARIO_STATS_MAX_HIT_POINTS => $get!(s?, mMaxHitPoints) as i64,
            EE_SCENARIO_STATS_CARRY => $get!(s?, mCarryAmount) as i64,
            EE_SCENARIO_STATS_FLIGHT_TIME => $get!(s?, mFlightTime) as i64,
            EE_SCENARIO_STATS_MAX_CARRY => $get!(s?, mMaxObjectCarry) as i64,
            EE_SCENARIO_STATS_RELOAD => $get!(s?, mReloadTime) as i64,
            EE_SCENARIO_STATS_SCALE => bits($get!(s?, mScale)),
            EE_SCENARIO_STATS_POWER => $get!(s?, mPower) as i64,
            EE_SCENARIO_STATS_ATTACK => bits($get!(s?, mAttack)),
            EE_SCENARIO_STATS_WORK_RATE => $get!(s?, mWorkRate) as i64,
            EE_SCENARIO_STATS_LEADERSHIP => bits($get!(s?, mLeadership)),
            EE_SCENARIO_STATS_REPAIR => $get!(s?, mRepairRate) as i64,
            EE_SCENARIO_STATS_HEALING => $get!(s?, mHealingPoints) as i64,
            EE_SCENARIO_STATS_RANGE => bits($get!(s?, mRange)),
            EE_SCENARIO_STATS_LINE_OF_SIGHT => bits($get!(s?, mLineOfSight)),
            EE_SCENARIO_STATS_VELOCITY => bits($get!(s?, mMaxVelocity)),
            EE_SCENARIO_STATS_INITIALIZED => $get!(s?, mIsInitialized) as i64,
            EE_SCENARIO_STATS_CAMERA_FOCUS => bits($get!(s?, mCameraFocus)),
            EE_SCENARIO_CLASS_GARRISON => $get!(k?, mGarrisonCapacity) as i64,
            EE_SCENARIO_CLASS_AMMUNITION => $get!(k?, mAmmunition) as i64,
            EE_SCENARIO_CLASS_ROTTING => $get!(k?, mRottingTime) as i64,
            EE_SCENARIO_CLASS_POWER => $get!(k?, mPower) as i64,
            EE_SCENARIO_CLASS_RELOAD => $get!(k?, mReloadTime) as i64,
            EE_SCENARIO_CLASS_ATTACK => $get!(k?, mAttack) as i64,
            EE_SCENARIO_CLASS_WORK_RATE => $get!(k?, mWorkRate) as i64,
            EE_SCENARIO_CLASS_FLIGHT_TIME => $get!(k?, mFlightTime) as i64,
            EE_SCENARIO_CLASS_LEADERSHIP => $get!(k?, mLeadership) as i64,
            EE_SCENARIO_CLASS_MAX_CARRY => $get!(k?, mMaxObjectCarry) as i64,
            EE_SCENARIO_CLASS_REPAIR => $get!(k?, mRepairRate) as i64,
            EE_SCENARIO_CLASS_HIT_POINTS => $get!(k?, mHitPoints) as i64,
            EE_SCENARIO_CLASS_CARRY => $get!(k?, mCarryAmount) as i64,
            EE_SCENARIO_CLASS_HEALING => $get!(k?, mHealingPoints) as i64,
            EE_SCENARIO_CLASS_RANGE => bits($get!(k?, mRange)),
            EE_SCENARIO_CLASS_LINE_OF_SIGHT => bits($get!(k?, mLineOfSight)),
            EE_SCENARIO_CLASS_VELOCITY => bits($get!(k?, mMaxVelocity)),
            EE_SCENARIO_CLASS_TRANSPORT => $get!(k?, mTransportCapacity) as i64,
            EE_SCENARIO_CLASS_WEIGHT => $get!(k?, mWeight) as i64,
            EE_SCENARIO_CLASS_TILE_SIZE => $get!(k?, mTileSize) as i64,
            EE_SCENARIO_CLASS_CRUISE => bits($get!(k?, mCruiseAltitude)),
            EE_SCENARIO_CLASS_MIN_RANGE => bits($get!(k?, mMinRange)),
            EE_SCENARIO_CLASS_AIR => $get!(k?, mCanEnterAirTransports) as i64,
            EE_SCENARIO_CLASS_LAND => $get!(k?, mCanEnterLandTransports) as i64,
            EE_SCENARIO_CLASS_SEA => $get!(k?, mCanEnterSeaTransports) as i64,
            EE_SCENARIO_CLASS_FORTRESS => $get!(k?, mCanEnterFortresses) as i64,
            _ => 0,
        })
    }};
}

/// `EESMUnitField(unit, field)` from the world views (None for the unit variables and the model's scale, the C++'s).
///
/// # Safety
/// `unit` is a live unit of the running game, read on the simulation thread inside a call from C++.
#[cfg(not(test))]
pub unsafe fn unit_field(unit: u64, field: i32) -> Option<i64> {
    use crate::world::{UnitView, WorldRead};
    macro_rules! get {
        ($record:expr, $member:ident) => {
            crate::world_field!($record, $member)
        };
    }
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    let u = unsafe { UnitView::of(&t, unit) }?;
    let stats = u.stats();
    let class = if field >= EE_SCENARIO_OF_STATS { stats.and_then(|s| s.class()) } else { u.class() };
    let complex = u.complex_unit();
    unit_field_body!(
        field,
        u.fields(),
        complex.map(|c| c.complex()),
        stats.map(|s| s.fields()),
        class.map(|k| k.fields()),
        complex.map_or(0, |c| c.cargo_len()),
        get
    )
}

/// The read ops the world views answer (the rest are the C++ host's): `cpp(op)` asks the C++ for the context's unit
/// (`EE_SCENARIO_READ_CONTEXT_UNIT`) and the world (`EE_SCENARIO_READ_WORLD`); None when the C++ must answer (a
/// member no record holds, an index the arrays do not have: the C++ reads what it read).
///
/// # Safety
/// As [`unit_field`]: inside a call from C++ on the simulation thread, the context's unit live.
#[cfg(not(test))]
pub unsafe fn read(cpp: &mut dyn FnMut(u32) -> i64, op: u32, a: i32, b: i32) -> Option<i64> {
    use crate::scenario::host::*;
    use crate::world::{ClassView, UnitView, WorldRead, WorldView};
    let pack = |x: i64, y: i64| (x as u32 as i64) | (((y as u32) as u64) << 32) as i64;
    match op {
        // SAFETY: the caller's contract
        EE_SCENARIO_READ_UNIT_FIELD => return unsafe { unit_field(cpp(EE_SCENARIO_READ_CONTEXT_UNIT) as u64, a) },
        EE_SCENARIO_READ_UNIT_TILE | EE_SCENARIO_READ_UNIT_HAS_COMPLEX => {
            // SAFETY: as above
            let t = unsafe { WorldRead::begin() };
            let u = unsafe { UnitView::of(&t, cpp(EE_SCENARIO_READ_CONTEXT_UNIT) as u64) }?;
            return Some(if op == EE_SCENARIO_READ_UNIT_TILE {
                let (x, y) = u.tile_xy();
                pack(x, y)
            } else {
                (u.complex_unit_handle() != 0) as i64
            });
        }
        _ => {}
    }
    // SAFETY: as above; `&gWorld`'s players and classes are live
    let t = unsafe { WorldRead::begin() };
    let class = || if a < 0 { None } else { ClassView::by_index(&t, a as usize) };
    match op {
        EE_SCENARIO_READ_CLASS_COUNT => return Some(ClassView::count(&t) as i64),
        EE_SCENARIO_READ_CLASS_EXISTS => return Some(class().is_some() as i64),
        EE_SCENARIO_READ_CLASS_SELECTABLE => {
            return Some(class().is_some_and(|k| crate::world_field!(k.fields(), mIsScenarioSelectable)) as i64)
        }
        EE_SCENARIO_READ_CLASS_CATEGORY => return Some(class()?.category() as i64),
        EE_SCENARIO_READ_CLASS_EPOCH => return Some(crate::world_field!(class()?.fields(), mEpoch) as i64),
        EE_SCENARIO_READ_CLASS_FAMILY => return Some(class()?.family() as i64),
        EE_SCENARIO_READ_CLASS_AI_KIND => return Some(class()?.ai_kind() as i64),
        EE_SCENARIO_READ_CLASS_TYPE => return Some(class()?.unit_type() as c_long as i64),
        _ => {}
    }
    // SAFETY: as above
    let w = unsafe { WorldView::of(&t, cpp(EE_SCENARIO_READ_WORLD) as u64) }?;
    if op == EE_SCENARIO_READ_NUM_PLAYERS {
        return Some(w.num_players() as i64);
    }
    // the world's player slot a (none outside the array, as no original index reached)
    let player = if a < 0 { None } else { w.player(a as usize) };
    if op == EE_SCENARIO_READ_PLAYER_EXISTS {
        return Some(player.is_some() as i64);
    }
    let p = player?;
    let at = |len: usize| (b >= 0 && (b as usize) < len).then_some(b as usize);
    Some(match op {
        EE_SCENARIO_READ_ENEMY => p.enemy(at(17)?) as i64,
        EE_SCENARIO_READ_RESOURCE => crate::world_field!(p.saved(), mResourceCaps)[at(7)?] as i64,
        EE_SCENARIO_READ_UNIT_COUNT => crate::world_field!(p.rts(), mUnitCount) as i64,
        EE_SCENARIO_READ_VARIABLE => crate::world_field!(p.saved(), mScenarioVariables)[at(10)?] as i64,
        EE_SCENARIO_READ_TRIBUTE => crate::world_field!(p.saved(), mTributes)[at(10)?] as i64,
        EE_SCENARIO_READ_MINING_LIMIT => crate::world_field!(p.fields(), mMiningLimit) as i64,
        EE_SCENARIO_READ_SHARE_BONUS => crate::world_field!(p.fields(), mShareBonus) as i64,
        EE_SCENARIO_READ_PLAYER_ID => crate::world_field!(p.rts(), mPlayerID) as i64,
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
    use crate::world::unit_class_fields::EeUnitsUnitClassFields;
    use crate::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsUnitFields};

    macro_rules! get {
        ($record:expr, $member:ident) => {
            $record.$member
        };
    }

    fn field(
        f: i32,
        u: &EeUnitsUnitFields,
        c: Option<&EeUnitsComplexUnitFields>,
        s: Option<&EeUnitsUnitClassUpgradeFields>,
        k: Option<&EeUnitsUnitClassFields>,
    ) -> Option<i64> {
        unit_field_body!(f, u, c, s, k, 3usize, get)
    }

    #[test]
    fn the_fields_convert_as_the_cpp_returned_them() {
        let u = EeUnitsUnitFields { mHitPoints: -5, mResourceType: 0xffff_fff0, ..Default::default() };
        let c = EeUnitsComplexUnitFields { mAmmunition: 0xffff_ffff, mIsBuilt: true, ..Default::default() };
        let mut s = EeUnitsUnitClassUpgradeFields { mRange: 2.5, ..Default::default() };
        s.mArmor[2] = 1.5;
        let mut k = EeUnitsUnitClassFields { mCanEnterFortresses: true, ..Default::default() };
        k.mArmor[5] = -7;
        let all = |f| field(f, &u, Some(&c), Some(&s), Some(&k));
        assert_eq!(all(EE_SCENARIO_UNIT_HIT_POINTS), Some(-5));
        assert_eq!(all(EE_SCENARIO_UNIT_RESOURCE_TYPE), Some(0xffff_fff0));
        assert_eq!(all(EE_SCENARIO_UNIT_AMMUNITION), Some(0xffff_ffffu32 as c_long as i64));
        assert_eq!(all(EE_SCENARIO_UNIT_BUILT), Some(1));
        assert_eq!(all(EE_SCENARIO_UNIT_CARGO), Some(3));
        assert_eq!(all(EE_SCENARIO_STATS_RANGE), Some(2.5f32.to_bits() as i64));
        assert_eq!(all(EE_SCENARIO_STATS_ARMOR + 2), Some(1.5f32.to_bits() as i64));
        assert_eq!(all(EE_SCENARIO_CLASS_ARMOR + 5), Some(-7));
        assert_eq!(all(EE_SCENARIO_CLASS_FORTRESS + EE_SCENARIO_OF_STATS), Some(1));
        assert_eq!(all(EE_SCENARIO_UNIT_HAS_COMPLEX), Some(1));
        assert_eq!(all(99), Some(0));
    }

    #[test]
    fn the_cpp_keeps_the_variables_and_the_scale_and_a_missing_part_has_no_answer() {
        let u = EeUnitsUnitFields::default();
        let c = EeUnitsComplexUnitFields::default();
        assert_eq!(field(EE_SCENARIO_UNIT_VARIABLE + 1, &u, Some(&c), None, None), None);
        assert_eq!(field(EE_SCENARIO_UNIT_SCALE, &u, Some(&c), None, None), None);
        assert_eq!(field(EE_SCENARIO_UNIT_HAS_STATS, &u, None, None, None), Some(0));
        assert_eq!(field(EE_SCENARIO_STATS_POWER, &u, None, None, None), None);
    }
}
