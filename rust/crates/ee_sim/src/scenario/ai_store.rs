//! Set AI Variable's stores on a computer player (`EE_SCENARIO_WORLD_AI_STORE`, effect.rs) answered here: the
//! fields written through the record the C++ class derives from (`class EEComputerPlayer : public EEPlayer, public
//! EeCpFields`), by the field numbers of aivar.rs (the original's 34 inline setters, COMDATs 0x1c1ca9..0x1c24c8;
//! `EESMStoreAI` with its tables and ee/EESMAIFields.h's lists, moved here in the scenario editor lane's round 4).
//! The difficulty setters are ee_sim's computer player's (cp player.rs); the next attack time is one C++ statement
//! (`EE_SCENARIO_WK_AI_*`, `SMEClassWorld` in Scenario Manager/EESMEffect.cpp).

// the record keeps the C++ member names; c_long/c_ulong are 32 bits on Windows
#![allow(non_snake_case, clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::cp::fields::EeCpFields;
use crate::scenario::aivar::*;
use crate::scenario::effect::EE_SCENARIO_WORLD_AI_STORE;

/// Player a0 (`SMEPlayer`, as the original's unchecked `EEComputerPlayer*`) as its `EeCpFields*`; its
/// `SetNextAttackTime(gPhysics->mGameTime)`.
pub const EE_SCENARIO_WK_AI_FIELDS: u32 = 764;
pub const EE_SCENARIO_WK_AI_NEXT_ATTACK_NOW: u32 = 765;

/// `EE_SM_AI_LONGS`' field `k` (aivar.rs `long`).
fn long_field(f: &mut EeCpFields, k: u32) -> Option<&mut c_long> {
    Some(match k {
        0 => &mut f.mInitialAttackDelayMax,
        1 => &mut f.mInitialAttackDelayMin,
        2 => &mut f.mAttackDelayModifierEasy,
        3 => &mut f.mAttackDelayModifierHard,
        4 => &mut f.mAssistAlliesDelayEasy,
        5 => &mut f.mAssistAlliesDelayMedium,
        6 => &mut f.mAssistAlliesDelayHard,
        7 => &mut f.mAttackFrequencyMin,
        8 => &mut f.mAttackFrequencyMax,
        9 => &mut f.mAttackFrequencyModifierEasy,
        10 => &mut f.mAttackFrequencyModifierHard,
        11 => &mut f.mNumLandExplorers,
        12 => &mut f.mNumSeaExplorers,
        13 => &mut f.mNumAirExplorers,
        14 => &mut f.mAttackGroupModifierEasy,
        15 => &mut f.mAttackGroupModifierMedium,
        16 => &mut f.mAttackGroupModifierHard,
        17 => &mut f.mAttackGroupSizeMin,
        18 => &mut f.mAttackGroupSizeMax,
        19 => &mut f.mDropOffDistance,
        20 => &mut f.mResearchFrequency,
        21 => &mut f.mNumImprovedUnitTypes,
        22 => &mut f.mNumImprovedAttributes,
        23 => &mut f.mLD7C,
        24 => &mut f.mLD78,
        25 => &mut f.mInitialAttackDelayPerEpoch,
        26 => &mut f.mAttackDelayFloor,
        27 => &mut f.mAttackDelayCeiling,
        28 => &mut f.mAttackFrequencyFloor,
        29 => &mut f.mAttackFrequencyCeiling,
        30 => &mut f.mResearchFrequencyModifierEasy,
        31 => &mut f.mResearchFrequencyModifierHard,
        32 => &mut f.mAttackEnemyChance,
        33 => &mut f.mAttackToClaimResourceChance,
        34 => &mut f.mAttackForRevengeChance,
        35 => &mut f.mEpochFrequencyMin,
        36 => &mut f.mEpochFrequencyMax,
        37 => &mut f.mPriestMaxPerGroup,
        38 => &mut f.mProphetMaxPerGroup,
        39 => &mut f.mHeroMaxPerGroup,
        40 => &mut f.mCarrierMaxPerGroup,
        41 => &mut f.mEnableStockCivilization,
        42 => &mut f.mEnableStockResource,
        43 => &mut f.mEnableStockResearch,
        44 => &mut f.mEnableStockProduction,
        45 => &mut f.mEnableStockBuild,
        46 => &mut f.mLE80,
        47 => &mut f.mEnableStockDefense,
        48 => &mut f.mEnableStockExploration,
        49 => &mut f.mEnableStockCombat,
        50 => &mut f.mManagerEpochAdvanceWait,
        51 => &mut f.mManagerAttackWait,
        52 => &mut f.mMaxEnabledManagers,
        53 => &mut f.mNumChokePointsToGuard,
        54 => &mut f.mDefenseGroupSizeMin,
        55 => &mut f.mDefenseGroupSizeMax,
        56 => &mut f.mDefensiveUnitUpdateEasy,
        57 => &mut f.mDefensiveUnitUpdateMedium,
        58 => &mut f.mDefensiveUnitUpdateHard,
        59 => &mut f.mReplenishDelayMin,
        60 => &mut f.mReplenishDelayMax,
        61 => &mut f.mUnitCostReductionEasy,
        62 => &mut f.mUnitCostReductionHard,
        63 => &mut f.mEpochFrequencyModifierEasy,
        64 => &mut f.mEpochFrequencyModifierHard,
        65 => &mut f.mExpansionTimerEasy,
        66 => &mut f.mExpansionTimerMedium,
        67 => &mut f.mExpansionTimerHard,
        68 => &mut f.mPopLimitAdjustmentEasy,
        69 => &mut f.mPopLimitAdjustmentMedium,
        70 => &mut f.mPopLimitAdjustmentHard,
        71 => &mut f.mTownCenterUpgradeTimer,
        72 => &mut f.mCapitolUpgradeTimer,
        73 => &mut f.mEpochCatchUpMin,
        74 => &mut f.mEpochCatchUpMax,
        75 => &mut f.mMinMilitaryBuildingsEasy,
        76 => &mut f.mMinMilitaryBuildingsHard,
        77 => &mut f.mMinMilitaryBuildingsMedium,
        78 => &mut f.mMaxFlameThrowers,
        79 => &mut f.mTownGuardUpdateEasy,
        80 => &mut f.mTownGuardUpdateMedium,
        81 => &mut f.mTownGuardUpdateHard,
        82 => &mut f.mDMTownGuardUpdateEasy,
        83 => &mut f.mDMTownGuardUpdateMedium,
        84 => &mut f.mDMTownGuardUpdateHard,
        85 => &mut f.mDMDefensiveUnitUpdateEasy,
        86 => &mut f.mDMDefensiveUnitUpdateMedium,
        87 => &mut f.mDMDefensiveUnitUpdateHard,
        88 => &mut f.mShipMaintenanceTimer,
        89 => &mut f.mShipsToMaintainEasy,
        90 => &mut f.mShipsToMaintainMedium,
        91 => &mut f.mShipsToMaintainHard,
        92 => &mut f.mShipsToMaintainDMEasy,
        93 => &mut f.mShipsToMaintainDMMedium,
        94 => &mut f.mShipsToMaintainDMHard,
        95 => &mut f.mBonzaiAttackLevel,
        _ => return None,
    })
}

/// `EE_SM_AI_FLOATS`' field `k` (aivar.rs `float`).
fn float_field(f: &mut EeCpFields, k: u32) -> Option<&mut f32> {
    Some(match k {
        0 => &mut f.mDefensiveUnitCap,
        1 => &mut f.mAttackFrequencyPerEpoch,
        2 => &mut f.mMilitaryCountWeight,
        3 => &mut f.mWorkerCountWeight,
        4 => &mut f.mMilitaryPopPercentage,
        5 => &mut f.mSpecializedUnitPercentage,
        6 => &mut f.mMaxAttackResponseRange,
        7 => &mut f.mPriestChance,
        8 => &mut f.mPriestPopPercentage,
        9 => &mut f.mProphetChance,
        10 => &mut f.mProphetPopPercentage,
        11 => &mut f.mHeroChance,
        12 => &mut f.mHeroPopPercentage,
        13 => &mut f.mCarrierChance,
        14 => &mut f.mCarrierPopPercentage,
        15 => &mut f.mRPSSelectionNonIslands,
        16 => &mut f.mUnitCostMedium,
        17 => &mut f.mEnableInterval,
        18 => &mut f.mAidThreatThreshold,
        19 => &mut f.mGuardChokepointPercentage,
        20 => &mut f.mGuardContinentPercentage,
        21 => &mut f.mGuardHighGroundPercentage,
        22 => &mut f.mGuardTownLandPercentage,
        23 => &mut f.mFC44,
        24 => &mut f.mFC50,
        25 => &mut f.mFC58,
        26 => &mut f.mGuardChokepointAirPercentage,
        27 => &mut f.mGuardTownPercentage,
        28 => &mut f.mFC38,
        29 => &mut f.mFC40,
        30 => &mut f.mFC4C,
        31 => &mut f.mGuardWaterwayPercentage,
        32 => &mut f.mFC3C,
        33 => &mut f.mFC48,
        34 => &mut f.mFC54,
        35 => &mut f.mTownGuardPercentage,
        36 => &mut f.mComputerAggression,
        37 => &mut f.mDMEarlyAttackScale,
        38 => &mut f.mSpecializedUnitChance,
        39 => &mut f.mRPSSelectionIslands,
        40 => &mut f.mNavalUnitWeight,
        41 => &mut f.mAirUnitWeight,
        42 => &mut f.mSiegeWeaponPercentage,
        _ => return None,
    })
}

/// `EE_SM_AI_BUILDINGS`' building `k`: its maximum (`max`) or its units per building (aivar.rs `building`).
fn building_field(f: &mut EeCpFields, k: u32, max: bool) -> Option<&mut c_long> {
    Some(match (k, max) {
        (0, true) => &mut f.mBarracksMax,
        (0, false) => &mut f.mUnitsPerBarracks,
        (1, true) => &mut f.mArcheryRangeMax,
        (1, false) => &mut f.mUnitsPerArcheryRange,
        (2, true) => &mut f.mStableMax,
        (2, false) => &mut f.mUnitsPerStable,
        (3, true) => &mut f.mDockMax,
        (3, false) => &mut f.mUnitsPerDock,
        (4, true) => &mut f.mSiegeFactoryMax,
        (4, false) => &mut f.mUnitsPerSiegeFactory,
        (5, true) => &mut f.mNavalYardMax,
        (5, false) => &mut f.mUnitsPerNavalYard,
        (6, true) => &mut f.mAirportMax,
        (6, false) => &mut f.mUnitsPerAirport,
        (7, true) => &mut f.mCyberFactoryMax,
        (7, false) => &mut f.mUnitsPerCyberFactory,
        (8, true) => &mut f.mUniversityMax,
        (8, false) => &mut f.mUnitsPerUniversity,
        (9, true) => &mut f.mTempleMax,
        (9, false) => &mut f.mUnitsPerTemple,
        (10, true) => &mut f.mHospitalMax,
        (10, false) => &mut f.mUnitsPerHospital,
        (11, true) => &mut f.mGranaryMax,
        (11, false) => &mut f.mUnitsPerGranary,
        (12, true) => &mut f.mCyberLabMax,
        (12, false) => &mut f.mUnitsPerCyberLab,
        (13, true) => &mut f.mSpaceDockMax,
        (13, false) => &mut f.mUnitsPerSpaceDock,
        (14, true) => &mut f.mTowerMax,
        (14, false) => &mut f.mUnitsPerTower,
        (15, true) => &mut f.mTankFactoryMax,
        (15, false) => &mut f.mUnitsPerTankFactory,
        (16, true) => &mut f.mAntiAirMax,
        (16, false) => &mut f.mUnitsPerAntiAirBuilding,
        _ => return None,
    })
}

/// `EESMStoreAI`: one store of Set AI Variable on a computer player's record, in place; the next attack time
/// through `next_attack_now`.
pub fn store(fields: &mut EeCpFields, write: &EeScenarioAiWrite, next_attack_now: &mut dyn FnMut()) {
    match write.kind {
        EE_SCENARIO_AI_KIND_LONG => {
            if let Some(v) = long_field(fields, write.field) {
                *v = write.long_value;
            }
        }
        EE_SCENARIO_AI_KIND_FLOAT => {
            if let Some(v) = float_field(fields, write.field) {
                *v = write.float_value;
            }
        }
        // (the original also mirrored it into the civilization manager's building counts, gone in main's stage 6)
        EE_SCENARIO_AI_KIND_MAX_COUNT | EE_SCENARIO_AI_KIND_UNITS_PER => {
            if let Some(v) = building_field(fields, write.field, write.kind == EE_SCENARIO_AI_KIND_MAX_COUNT) {
                *v = write.long_value;
            }
        }
        EE_SCENARIO_AI_KIND_CALL => match write.field {
            EE_SCENARIO_AI_CALL_NEXT_ATTACK_TIME_NOW => next_attack_now(),
            EE_SCENARIO_AI_CALL_ISLAND_EXPANSION_TIMER => {
                fields.set_island_expansion_timer(write.long_value as c_ulong)
            }
            EE_SCENARIO_AI_CALL_DM_ATTACK_FREQUENCY_MODIFIER => {
                fields.set_dm_attack_frequency_modifier(write.float_value)
            }
            EE_SCENARIO_AI_CALL_DM_INITIAL_ATTACK_MODIFIER => fields.set_dm_initial_attack_modifier(write.float_value),
            _ => {}
        },
        _ => {}
    }
}

/// The world operation `op` composed (`EE_SCENARIO_WORLD_AI_STORE`: player a0, the write at a1), or `None` for the
/// others.
pub fn compose(cpp: &mut dyn FnMut(u32, [i64; 6]) -> i64, op: u32, a: [i64; 6]) -> Option<i64> {
    if op != EE_SCENARIO_WORLD_AI_STORE {
        return None;
    }
    // SAFETY: a1 is the caller's live write (effect.rs)
    let write = unsafe { *(a[1] as usize as *const EeScenarioAiWrite) };
    let fields = cpp(EE_SCENARIO_WK_AI_FIELDS, [a[0], 0, 0, 0, 0, 0]) as usize as *mut EeCpFields;
    if write.kind == EE_SCENARIO_AI_KIND_CALL && write.field == EE_SCENARIO_AI_CALL_NEXT_ATTACK_TIME_NOW {
        cpp(EE_SCENARIO_WK_AI_NEXT_ATTACK_NOW, [a[0], 0, 0, 0, 0, 0]);
        return Some(0);
    }
    // SAFETY: the live computer player's record; no C++ call runs while the reference lives
    store(unsafe { &mut *fields }, &write, &mut || {});
    Some(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn write(kind: u32, field: u32, long_value: c_long, float_value: f32) -> EeScenarioAiWrite {
        EeScenarioAiWrite { kind, field, long_value, float_value }
    }

    #[test]
    fn stores_by_kind_and_field_number() {
        let mut f = EeCpFields::default();
        let mut calls = 0;
        let mut now = || calls += 1;
        store(&mut f, &write(EE_SCENARIO_AI_KIND_LONG, long::INITIAL_ATTACK_DELAY_MAX, 9, 0.0), &mut now);
        store(&mut f, &write(EE_SCENARIO_AI_KIND_LONG, long::BONZAI_ATTACK_LEVEL, -4, 0.0), &mut now);
        store(&mut f, &write(EE_SCENARIO_AI_KIND_LONG, long::LD7C, 7, 0.0), &mut now);
        store(&mut f, &write(EE_SCENARIO_AI_KIND_FLOAT, float::SIEGE_WEAPON_PERCENTAGE, 0, 0.5), &mut now);
        store(&mut f, &write(EE_SCENARIO_AI_KIND_FLOAT, float::FC44, 0, 1.25), &mut now);
        store(&mut f, &write(EE_SCENARIO_AI_KIND_MAX_COUNT, building::BARRACKS, 3, 0.0), &mut now);
        store(&mut f, &write(EE_SCENARIO_AI_KIND_UNITS_PER, building::ANTI_AIR, 6, 0.0), &mut now);
        store(&mut f, &write(EE_SCENARIO_AI_KIND_CALL, EE_SCENARIO_AI_CALL_NEXT_ATTACK_TIME_NOW, 0, 0.0), &mut now);
        // out of range: nothing
        store(&mut f, &write(EE_SCENARIO_AI_KIND_LONG, 96, 1, 0.0), &mut now);
        store(&mut f, &write(EE_SCENARIO_AI_KIND_MAX_COUNT, 17, 1, 0.0), &mut now);
        assert_eq!(calls, 1);
        assert_eq!((f.mInitialAttackDelayMax, f.mBonzaiAttackLevel, f.mLD7C), (9, -4, 7));
        assert_eq!((f.mSiegeWeaponPercentage, f.mFC44), (0.5, 1.25));
        assert_eq!((f.mBarracksMax, f.mUnitsPerBarracks, f.mAntiAirMax, f.mUnitsPerAntiAirBuilding), (3, 0, 0, 6));
    }

    #[test]
    fn every_field_number_names_a_distinct_field() {
        let mut f = EeCpFields::default();
        for k in 0..96 {
            *long_field(&mut f, k).unwrap() = k as c_long + 1;
        }
        for k in 0..43 {
            *float_field(&mut f, k).unwrap() = k as f32 + 1.0;
        }
        for k in 0..17 {
            *building_field(&mut f, k, true).unwrap() = 1000 + k as c_long;
            *building_field(&mut f, k, false).unwrap() = 2000 + k as c_long;
        }
        for k in 0..96 {
            assert_eq!(*long_field(&mut f, k).unwrap(), k as c_long + 1);
        }
        for k in 0..43 {
            assert_eq!(*float_field(&mut f, k).unwrap(), k as f32 + 1.0);
        }
        for k in 0..17 {
            assert_eq!(*building_field(&mut f, k, true).unwrap(), 1000 + k as c_long);
            assert_eq!(*building_field(&mut f, k, false).unwrap(), 2000 + k as c_long);
        }
    }

    #[test]
    fn the_world_op_reads_the_record_then_stores() {
        let mut f = EeCpFields::default();
        let fp = &mut f as *mut EeCpFields as usize as i64;
        let w = write(EE_SCENARIO_AI_KIND_LONG, long::MAX_FLAME_THROWERS, 12, 0.0);
        let now = write(EE_SCENARIO_AI_KIND_CALL, EE_SCENARIO_AI_CALL_NEXT_ATTACK_TIME_NOW, 0, 0.0);
        let mut ops = Vec::new();
        {
            let mut cpp = |op: u32, a: [i64; 6]| -> i64 {
                ops.push((op, a[0]));
                if op == EE_SCENARIO_WK_AI_FIELDS {
                    fp
                } else {
                    0
                }
            };
            let at = |w: &EeScenarioAiWrite| w as *const EeScenarioAiWrite as usize as i64;
            assert_eq!(compose(&mut cpp, EE_SCENARIO_WORLD_AI_STORE, [4, at(&w), 0, 0, 0, 0]), Some(0));
            assert_eq!(compose(&mut cpp, EE_SCENARIO_WORLD_AI_STORE + 1, [0; 6]), None);
            compose(&mut cpp, EE_SCENARIO_WORLD_AI_STORE, [4, at(&now), 0, 0, 0, 0]);
        }
        assert_eq!(
            ops,
            vec![(EE_SCENARIO_WK_AI_FIELDS, 4), (EE_SCENARIO_WK_AI_FIELDS, 4), (EE_SCENARIO_WK_AI_NEXT_ATTACK_NOW, 4)]
        );
        assert_eq!(f.mMaxFlameThrowers, 12);
    }
}
