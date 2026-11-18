//! `EEComputerPlayer::Init` (0x2f1c8, both constructors) over the record: the difficulty, the default favourite unit
//! classes, the timers and flags, and every tuning value from the computer player behaviour table
//! (`gCPBehaviors`, db\dbcpbehavior.dat; [`EeCpBehavior`] is its record). The managers' objects stay the C++'s.
//!
//! The attack frequency's bounds read record 21 (Attack Frequency Boundary), the attack delay's record 19 (Attack
//! Frequency Boundaries). The record is zeroed first:
//! the members `Init` does not set (the original left them as `new` gave them) start at 0. The arithmetic is the
//! C++'s: `int * 60000` wrapping, `(long)float` truncating, `int * 0.01f` in f32, the drop-off distance's square
//! root in f64 rounded by `UMFloatToLong`.

#![allow(non_snake_case)]
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use ee_base::random::float_to_long;

use super::fields::EeCpFields;

/// One record of the behaviour table (`EECPBehavior`, ee/EECPBehavior.h: 56 bytes).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCpBehavior {
    pub unused1: i32,
    /// the slot (records with index <= 0 are skipped)
    pub index: i32,
    pub i1_min: i32,
    pub i1_max: i32,
    /// `mIntValue1`
    pub i1: i32,
    pub i2_min: i32,
    pub i2_max: i32,
    /// `mIntValue2`
    pub i2: i32,
    pub unused2: i32,
    pub float_min: i32,
    pub float_max: i32,
    pub float_scale: i32,
    /// `mFloatValue`
    pub fv: f32,
    pub settable: bool,
}

/// The table's records (`gCPBehaviors[129]`).
pub const EE_CP_BEHAVIORS: usize = 129;

/// `Init`'s values into `p`: `difficulty` is the lobby's AI level, `cheat_cost` whether the cost cheat is on.
///
/// # Safety
/// `p` is a live record; `behaviors` points to the table's [`EE_CP_BEHAVIORS`] records.
pub unsafe fn init(
    p: *mut EeCpFields,
    behaviors: *const EeCpBehavior,
    difficulty: c_long,
    player_id: c_ulong,
    cheat_cost: bool,
) {
    // SAFETY: the caller's contract
    let table = unsafe { std::slice::from_raw_parts(behaviors, EE_CP_BEHAVIORS) };
    let b = |k: usize| table[k];
    // SAFETY: the caller's live record, written field by field
    unsafe {
        p.write(EeCpFields::default());
        (*p).mDifficulty = difficulty as c_ulong; // EEDifficultyAILevel(the lobby's difficulty): gameoptions.yaml ai_level
        (*p).mLBD4 = 0;
        (*p).mFavoriteLandType = 20;
        (*p).mFavoriteSeaType = 242;
        (*p).mFavoriteAirType = 72;
        (*p).mFavoriteSpaceType = 730;
        (*p).mSeed = (player_id % 10) as c_long;
        (*p).mNextAttackTime = 0;
        (*p).mCanAttack = true;
        (*p).mIsStarted = false;
        (*p).mLF24 = 0;
        (*p).mLCC8 = 0;
        (*p).mLCCC = 0;
        (*p).mLCD0 = 0;
        (*p).mLCD4 = 0;
        (*p).mNextEnableTime = 0;
        (*p).mIsEnableTimeSet = false;
        (*p).mLastDamagedTime = 0;
        (*p).mBuildingDamaged = false;
        (*p).mCitizenDamaged = false;
        (*p).mActive = true;
        (*p).mIgnoreOthers = false;
        (*p).mNextPopulationTime = 0;
        (*p).mPopulationCap = 0;
        (*p).mNextExpansionTime = 0;
        (*p).mNextIslandExpansionTime = 0;
        (*p).mUsesFishing = false;
        (*p).mAssistAlliesDelayEasy = b(4).i1.wrapping_mul(60000) as c_long;
        (*p).mAssistAlliesDelayMedium = b(4).i2.wrapping_mul(60000) as c_long;
        (*p).mAssistAlliesDelayHard = (b(4).fv as i32).wrapping_mul(60000) as c_long;
        (*p).mNavalUnitWeight = b(111).fv;
        (*p).mAirUnitWeight = b(112).fv;
        (*p).mMaxFlameThrowers = b(113).i1 as c_long;
        (*p).mUnitCostMedium = b(67).fv;
        (*p).mCarrierMaxPerGroup = b(52).i1 as c_long;
        (*p).mCarrierPopPercentage = b(52).fv;
        (*p).mCarrierChance = (b(52).i2 as f32) * 0.01;
        (*p).mNumAirExplorers = b(10).i1 as c_long;
        (*p).mComputerAggression = b(91).fv;
        (*p).mAttackEnemyChance = b(25).i1 as c_long;
        (*p).mAttackToClaimResourceChance = b(26).i1 as c_long;
        (*p).mAttackForRevengeChance = b(27).i1 as c_long;
        (*p).mAttackGroupSizeMax = b(13).i2 as c_long;
        (*p).mAttackGroupSizeMin = b(13).i1 as c_long;
        (*p).mAttackFrequencyPerEpoch = b(22).fv;
        (*p).mAttackFrequencyModifierEasy = b(7).i1 as c_long;
        (*p).mAttackFrequencyModifierHard = b(7).i2 as c_long;
        (*p).mAttackFrequencyCeiling = b(21).i2 as c_long;
        (*p).mAttackFrequencyFloor = b(21).i1 as c_long;
        (*p).mAttackFrequencyMax = b(6).i2 as c_long;
        (*p).mAttackFrequencyMin = b(6).i1 as c_long;
        (*p).mInitialAttackDelayPerEpoch = b(18).i1 as c_long;
        (*p).mAttackDelayModifierEasy = b(2).i1 as c_long;
        (*p).mAttackDelayModifierHard = b(2).i2 as c_long;
        (*p).mAttackDelayCeiling = b(19).i2 as c_long;
        (*p).mAttackDelayFloor = b(19).i1 as c_long;
        (*p).mInitialAttackDelayMax = b(1).i2 as c_long;
        (*p).mInitialAttackDelayMin = b(1).i1 as c_long;
        (*p).mSpecializedUnitPercentage = b(45).fv;
        (*p).mAttackGroupModifierEasy = b(11).i1 as c_long;
        (*p).mAttackGroupModifierMedium = b(11).i2 as c_long;
        (*p).mAttackGroupModifierHard = (b(11).fv as i32) as c_long;
        (*p).mRPSSelectionNonIslands = b(53).fv;
        (*p).mRPSSelectionIslands = b(110).fv;
        (*p).mSpecializedUnitChance = b(54).fv;
        (*p).mBonzaiAttackLevel = b(120).i1 as c_long;
        (*p).mDMEarlyAttackTime = b(105).i2.wrapping_mul(60000) as c_ulong;
        (*p).mDMEarlyAttackScale = (b(105).i1 as f32) * 0.01;
        (*p).mTownGuardPercentage = b(90).fv;
        (*p).mLD78 = b(17).i1 as c_long;
        (*p).mLD7C = b(16).i1 as c_long;
        (*p).mCitizenPercentage = b(3).fv;
        (*p).mUnitCostReductionEasy = b(96).i1 as c_long;
        (*p).mUnitCostReductionHard = b(96).i2 as c_long;
        (*p).mDefenseGroupSizeMax = b(87).i2 as c_long;
        (*p).mDefenseGroupSizeMin = b(87).i1 as c_long;
        (*p).mEpochFrequencyMax = b(28).i2 as c_long;
        (*p).mEpochFrequencyMin = b(28).i1 as c_long;
        (*p).mEpochCatchUpMax = b(102).i2 as c_long;
        (*p).mEpochCatchUpMin = b(102).i1 as c_long;
        (*p).mEpochCatchUpEasy = (b(107).i1 as f32) * 0.01;
        (*p).mEpochCatchUpHard = (b(107).i2 as f32) * 0.01;
        (*p).mEpochCatchUpDeathmatch = b(107).fv * 0.01;
        (*p).mEpochFrequencyModifierEasy = b(29).i1 as c_long;
        (*p).mEpochFrequencyModifierHard = b(29).i2 as c_long;
        (*p).mExpansionTimerEasy = b(97).i1 as c_long;
        (*p).mExpansionTimerMedium = b(97).i2 as c_long;
        (*p).mExpansionTimerHard = (b(97).fv as i32) as c_long;
        (*p).mGuardContinentPercentage = b(71).fv;
        (*p).mGuardChokepointAirPercentage = b(77).fv;
        (*p).mGuardChokepointPercentage = b(70).fv;
        (*p).mGuardHighGroundPercentage = b(72).fv;
        (*p).mFC38 = b(79).fv;
        (*p).mFC3C = b(83).fv;
        (*p).mFC40 = b(80).fv;
        (*p).mFC44 = b(74).fv;
        (*p).mFC48 = b(84).fv;
        (*p).mFC4C = b(81).fv;
        (*p).mFC50 = b(75).fv;
        (*p).mFC54 = b(85).fv;
        (*p).mFC58 = b(76).fv;
        (*p).mGuardTownPercentage = b(78).fv;
        (*p).mGuardTownLandPercentage = b(73).fv;
        (*p).mGuardWaterwayPercentage = b(82).fv;
        (*p).mHeroMaxPerGroup = b(51).i1 as c_long;
        (*p).mHeroPopPercentage = b(51).fv;
        (*p).mHeroChance = (b(51).i2 as f32) * 0.01;
        (*p).mNumImprovedUnitTypes = b(46).i1 as c_long;
        (*p).mNumImprovedAttributes = b(47).i1 as c_long;
        (*p).mReplenishDelayMax = b(89).i2 as c_long;
        (*p).mReplenishDelayMin = b(89).i1 as c_long;
        (*p).mNumLandExplorers = b(8).i1 as c_long;
        (*p).mNumChokePointsToGuard = b(86).i1 as c_long;
        (*p).mAntiAirMax = (b(109).fv as i32) as c_long;
        (*p).mAirportMax = (b(36).fv as i32) as c_long;
        (*p).mArcheryRangeMax = (b(31).fv as i32) as c_long;
        (*p).mBarracksMax = (b(30).fv as i32) as c_long;
        (*p).mDockMax = (b(33).fv as i32) as c_long;
        (*p).mGranaryMax = (b(41).fv as i32) as c_long;
        (*p).mHospitalMax = (b(40).fv as i32) as c_long;
        (*p).mCyberFactoryMax = (b(37).fv as i32) as c_long;
        (*p).mCyberLabMax = (b(42).fv as i32) as c_long;
        (*p).mNavalYardMax = (b(35).fv as i32) as c_long;
        (*p).mUniversityMax = (b(38).fv as i32) as c_long;
        (*p).mSiegeFactoryMax = (b(34).fv as i32) as c_long;
        (*p).mSpaceDockMax = (b(117).fv as i32) as c_long;
        (*p).mStableMax = (b(32).fv as i32) as c_long;
        (*p).mTankFactoryMax = (b(108).fv as i32) as c_long;
        (*p).mTempleMax = (b(39).fv as i32) as c_long;
        (*p).mTowerMax = b(95).i2 as c_long;
        (*p).mDefensiveUnitCap = b(20).fv;
        (*p).mSiegeWeaponPercentage = b(119).fv * 0.01;
        (*p).mMaxAttackResponseRange = b(48).fv;
        (*p).mMinMilitaryBuildingsEasy = b(103).i1 as c_long;
        (*p).mMinMilitaryBuildingsEasyDM = b(103).i2 as c_long;
        (*p).mMinMilitaryBuildingsMedium = b(106).i1 as c_long;
        (*p).mMinMilitaryBuildingsMediumDM = b(106).i2 as c_long;
        (*p).mMinMilitaryBuildingsHard = b(104).i1 as c_long;
        (*p).mMinMilitaryBuildingsHardDM = b(104).i2 as c_long;
        (*p).mMilitaryPopPercentage = b(43).fv;
        (*p).mAidThreatThreshold = b(69).fv;
        (*p).mPopLimitAdjustmentEasy = b(98).i1 as c_long;
        (*p).mPopLimitAdjustmentMedium = b(98).i2 as c_long;
        (*p).mPopLimitAdjustmentHard = (b(98).fv as i32) as c_long;
        (*p).mPriestMaxPerGroup = b(49).i1 as c_long;
        (*p).mPriestPopPercentage = b(49).fv;
        (*p).mPriestChance = (b(49).i2 as f32) * 0.01;
        (*p).mProphetMaxPerGroup = b(50).i1 as c_long;
        (*p).mProphetPopPercentage = b(50).fv;
        (*p).mProphetChance = (b(50).i2 as f32) * 0.01;
        (*p).mResearchFrequency = b(15).i1 as c_long;
        (*p).mResearchFrequencyModifierEasy = b(23).i1 as c_long;
        (*p).mResearchFrequencyModifierHard = b(23).i2 as c_long;
        (*p).mDropOffDistance = b(14).i1 as c_long;
        (*p).mExtraResources = b(101).i1 as c_long;
        (*p).mNumSeaExplorers = b(9).i1 as c_long;
        (*p).mTownCenterUpgradeTimer = b(99).i1 as c_long;
        (*p).mMaxEnabledManagers = b(66).i1 as c_long;
        (*p).mManagerAttackWait = b(65).i1 as c_long;
        (*p).mManagerEpochAdvanceWait = b(64).i1 as c_long;
        (*p).mEnableStockProduction = b(58).i1 as c_long;
        (*p).mEnableStockResource = b(56).i1 as c_long;
        (*p).mEnableStockCivilization = b(55).i1 as c_long;
        (*p).mEnableStockExploration = b(62).i1 as c_long;
        (*p).mEnableStockDefense = b(61).i1 as c_long;
        (*p).mEnableStockBuild = b(59).i1 as c_long;
        (*p).mLE80 = b(60).i1 as c_long;
        (*p).mEnableStockResearch = b(57).i1 as c_long;
        (*p).mEnableStockCombat = b(63).i1 as c_long;
        (*p).mEnableInterval = b(68).fv;
        (*p).mMilitaryCountWeight = (b(12).i1 as f32) * 0.01;
        (*p).mWorkerCountWeight = (b(12).i2 as f32) * 0.01;
        (*p).mUnitsPerAntiAirBuilding = b(109).i1 as c_long;
        (*p).mUnitsPerAirport = b(36).i1 as c_long;
        (*p).mUnitsPerArcheryRange = b(31).i1 as c_long;
        (*p).mUnitsPerBarracks = b(30).i1 as c_long;
        (*p).mUnitsPerDock = b(33).i1 as c_long;
        (*p).mUnitsPerGranary = b(41).i1 as c_long;
        (*p).mUnitsPerHospital = b(40).i1 as c_long;
        (*p).mUnitsPerCyberFactory = b(37).i1 as c_long;
        (*p).mUnitsPerCyberLab = b(42).i1 as c_long;
        (*p).mUnitsPerNavalYard = b(35).i1 as c_long;
        (*p).mUnitsPerUniversity = b(38).i1 as c_long;
        (*p).mUnitsPerSiegeFactory = b(34).i1 as c_long;
        (*p).mUnitsPerSpaceDock = b(117).i1 as c_long;
        (*p).mUnitsPerStable = b(32).i1 as c_long;
        (*p).mUnitsPerTankFactory = b(108).i1 as c_long;
        (*p).mUnitsPerTemple = b(39).i1 as c_long;
        (*p).mUnitsPerTower = b(95).i1 as c_long;
        (*p).mCapitolUpgradeTimer = b(100).i1 as c_long;
        (*p).mTowerSpacing = b(95).fv;
        (*p).mShipsToMaintainDMEasy = b(44).i1 as c_long;
        (*p).mShipsToMaintainDMMedium = b(44).i2 as c_long;
        (*p).mShipsToMaintainDMHard = (b(44).fv as i32) as c_long;
        (*p).mShipsToMaintainEasy = b(5).i1 as c_long;
        (*p).mShipsToMaintainMedium = b(5).i2 as c_long;
        (*p).mShipsToMaintainHard = (b(5).fv as i32) as c_long;
        (*p).mShipMaintenanceTimer = b(118).i1 as c_long;
        (*p).mDefensiveUnitUpdateEasy = b(88).i1.wrapping_mul(60000) as c_long;
        (*p).mDefensiveUnitUpdateMedium = b(88).i2.wrapping_mul(60000) as c_long;
        (*p).mDefensiveUnitUpdateHard = (b(88).fv as i32).wrapping_mul(60000) as c_long;
        (*p).mDMDefensiveUnitUpdateEasy = b(116).i1.wrapping_mul(60000) as c_long;
        (*p).mDMDefensiveUnitUpdateMedium = b(116).i2.wrapping_mul(60000) as c_long;
        (*p).mDMDefensiveUnitUpdateHard = (b(116).fv as i32).wrapping_mul(60000) as c_long;
        (*p).mTownGuardUpdateEasy = b(114).i1.wrapping_mul(60000) as c_long;
        (*p).mTownGuardUpdateMedium = b(114).i2.wrapping_mul(60000) as c_long;
        (*p).mTownGuardUpdateHard = (b(114).fv as i32).wrapping_mul(60000) as c_long;
        (*p).mDMTownGuardUpdateEasy = b(115).i1.wrapping_mul(60000) as c_long;
        (*p).mDMTownGuardUpdateMedium = b(115).i2.wrapping_mul(60000) as c_long;
        (*p).mDMTownGuardUpdateHard = (b(115).fv as i32).wrapping_mul(60000) as c_long;
        (*p).mIslandExpansionTimerEasy = b(121).i1 as c_long;
        (*p).mIslandExpansionTimerMedium = b(121).i2 as c_long;
        (*p).mIslandExpansionTimerHard = (b(121).fv as i32) as c_long;
        (*p).mDMAttackFrequencyModifierEasy = (b(122).i1 as f32) * 0.01;
        (*p).mDMAttackFrequencyModifierMedium = (b(122).i2 as f32) * 0.01;
        (*p).mDMAttackFrequencyModifierHard = b(122).fv * 0.01;
        (*p).mDMInitialAttackModifierEasy = (b(123).i1 as f32) * 0.01;
        (*p).mDMInitialAttackModifierMedium = (b(123).i2 as f32) * 0.01;
        (*p).mDMInitialAttackModifierHard = b(123).fv * 0.01;
        (*p).mLD50 = 1;
        (*p).mLD54 = 1;
        (*p).mLD48 = 1;
        (*p).mLD4C = 1;
        (*p).mLF14 = -1;
        (*p).mLF18 = -1;
        (*p).mLF1C = -1;
        (*p).mLF20 = -1;
        (*p).mDropOffDistance = float_to_long(((*p).mDropOffDistance as f64).sqrt() as f32) as c_long;
        (*p).mCanBuild = true;
        (*p).mIsCheating = cheat_cost; // EECPCheatOn(kAICheatCost): EE_AI_CHEATS (the original: always true)
        (*p).mCanDefend = true;
        (*p).mBF4E = true;
        (*p).mCanExplore = true;
        (*p).mCanGather = true;
        (*p).mIsCitizenPercentageSet = true;
        (*p).mReplaceOnLoad = false;
        (*p).mRegroupPending = false;
        (*p).mAttackStarted = false;
    }
}

#[cfg(test)]
#[path = "init_tests.rs"]
mod tests;
