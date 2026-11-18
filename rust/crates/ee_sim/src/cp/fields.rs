//! The computer player's plain members as a Rust record the C++ class derives from
//! (`class EEComputerPlayer : public EEPlayer, public EeCpFields`, ee/EEComputerPlayer.h): the tuning values
//! `Init` takes from the behaviour table, the timers and the flags, every one `Load` and `Save` read and write. The
//! fields keep their C++ names, types and order (the original's offsets in the comments), so every reader is
//! unchanged. The containers, the managers' pointers and the engines' states stay C++ members.

#![allow(non_snake_case)]

use std::os::raw::{c_long, c_ulong};

/// `EEComputerPlayer`'s plain members, `mLBD4` (+0xbd4) to `mAttackStarted` (+0xf56), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCpFields {
    /// (unsigned: Load)
    pub mLBD4: c_ulong,
    /// +0xbd8 0 or 2 (GetResearchFrequencyModifier; CheckBuildingCounts picks +0xe14/+0xe24/+0xe1c by it) (provisional name; unsigned: Load)
    pub mDifficulty: c_ulong,
    pub mFavoriteLandType: c_ulong,
    pub mFavoriteSeaType: c_ulong,
    pub mFavoriteAirType: c_ulong,
    pub mFavoriteSpaceType: c_ulong,
    pub mUnitCostMedium: f32,
    /// +0xbf0 (fld at 0x47346f)
    pub mComputerAggression: f32,
    pub mCarrierChance: f32,
    pub mCarrierPopPercentage: f32,
    pub mAttackFrequencyPerEpoch: f32,
    pub mSpecializedUnitPercentage: f32,
    pub mRPSSelectionNonIslands: f32,
    pub mRPSSelectionIslands: f32,
    pub mSpecializedUnitChance: f32,
    /// +0xc10 (EECPDefenseCommander 0x783e7: the aid chance)
    pub mTownGuardPercentage: f32,
    /// +0xc14 (fld at 0x4734d2)
    pub mCitizenPercentage: f32,
    pub mDMEarlyAttackScale: f32,
    pub mEpochCatchUpEasy: f32,
    pub mEpochCatchUpHard: f32,
    pub mEpochCatchUpDeathmatch: f32,
    /// +0xc28
    pub mGuardContinentPercentage: f32,
    /// +0xc2c
    pub mGuardChokepointAirPercentage: f32,
    /// +0xc30
    pub mGuardChokepointPercentage: f32,
    /// +0xc34
    pub mGuardHighGroundPercentage: f32,
    pub mFC38: f32,
    pub mFC3C: f32,
    pub mFC40: f32,
    pub mFC44: f32,
    pub mFC48: f32,
    pub mFC4C: f32,
    pub mFC50: f32,
    pub mFC54: f32,
    pub mFC58: f32,
    /// +0xc5c
    pub mGuardTownPercentage: f32,
    /// +0xc60
    pub mGuardTownLandPercentage: f32,
    pub mGuardWaterwayPercentage: f32,
    pub mHeroChance: f32,
    pub mHeroPopPercentage: f32,
    /// +0xc70 (EECPDefenseCommander 0x799fa: the defense share)
    pub mDefensiveUnitCap: f32,
    /// (float: Load/Init)
    pub mSiegeWeaponPercentage: f32,
    /// (float: Load/Init)
    pub mMaxAttackResponseRange: f32,
    /// (float: Load/Init)
    pub mMilitaryPopPercentage: f32,
    /// +0xc80 (EECPDefenseCommander 0x785cb: the allies' threat limit)
    pub mAidThreatThreshold: f32,
    /// (float: Load/Init)
    pub mPriestPopPercentage: f32,
    /// (float: Load/Init)
    pub mPriestChance: f32,
    /// (float: Load/Init)
    pub mProphetPopPercentage: f32,
    /// (float: Load/Init)
    pub mProphetChance: f32,
    /// (float: Load/Init)
    pub mEnableInterval: f32,
    /// +0xc98 (EECPIntelligencePlayerFile::Update: the worker-count weight) (provisional name)
    pub mWorkerCountWeight: f32,
    /// +0xc9c (Update: the military-count weight) (provisional name)
    pub mMilitaryCountWeight: f32,
    /// (float: Load/Init)
    pub mNavalUnitWeight: f32,
    /// (float: Load/Init)
    pub mAirUnitWeight: f32,
    /// +0xca8 (float: EECPResourceCenter::FindBuildSite 0x48bee3 flds it)
    pub mTowerSpacing: f32,
    /// (float: Load/Init)
    pub mDMAttackFrequencyModifierEasy: f32,
    /// (float: Load/Init)
    pub mDMAttackFrequencyModifierMedium: f32,
    /// (float: Load/Init)
    pub mDMAttackFrequencyModifierHard: f32,
    /// (float: Load/Init)
    pub mDMInitialAttackModifierEasy: f32,
    /// (float: Load/Init)
    pub mDMInitialAttackModifierMedium: f32,
    /// (float: Load/Init)
    pub mDMInitialAttackModifierHard: f32,
    pub mLCC4: c_long,
    pub mLCC8: c_long,
    pub mLCCC: c_long,
    pub mLCD0: c_long,
    pub mLCD4: c_long,
    /// +0xcd8 (EECPPlayerInteractionManager's ctor 0x895de copies it)
    pub mSeed: c_long,
    pub mShipsToMaintainDMEasy: c_long,
    pub mShipsToMaintainDMMedium: c_long,
    pub mShipsToMaintainDMHard: c_long,
    pub mShipsToMaintainEasy: c_long,
    pub mShipsToMaintainMedium: c_long,
    pub mShipsToMaintainHard: c_long,
    pub mShipMaintenanceTimer: c_long,
    /// +0xcf8 (EECPMilitaryAidPackage 0x84e42: the aid delay against a human, by mDifficulty) (provisional name)
    pub mAssistAlliesDelayEasy: c_long,
    /// +0xcfc
    pub mAssistAlliesDelayMedium: c_long,
    /// +0xd00
    pub mAssistAlliesDelayHard: c_long,
    pub mPopulationCap: c_long,
    pub mCarrierMaxPerGroup: c_long,
    /// +0xd0c (EECPExplorationManager::Update: the wanted explorers of kind 2 when +0xf55)
    pub mNumAirExplorers: c_long,
    pub mAttackEnemyChance: c_long,
    pub mAttackToClaimResourceChance: c_long,
    pub mAttackForRevengeChance: c_long,
    pub mAttackGroupSizeMax: c_long,
    pub mAttackGroupSizeMin: c_long,
    pub mAttackFrequencyModifierEasy: c_long,
    pub mAttackFrequencyModifierHard: c_long,
    pub mAttackFrequencyCeiling: c_long,
    pub mAttackFrequencyFloor: c_long,
    pub mAttackFrequencyMax: c_long,
    pub mAttackFrequencyMin: c_long,
    pub mInitialAttackDelayPerEpoch: c_long,
    pub mAttackDelayModifierEasy: c_long,
    pub mAttackDelayModifierHard: c_long,
    pub mLD48: c_long,
    pub mLD4C: c_long,
    pub mLD50: c_long,
    pub mLD54: c_long,
    pub mAttackDelayCeiling: c_long,
    pub mAttackDelayFloor: c_long,
    /// +0xd60 (EECPDefenseCommander 0x78866: minutes before the citizens' raid)
    pub mInitialAttackDelayMax: c_long,
    pub mInitialAttackDelayMin: c_long,
    pub mAttackGroupModifierEasy: c_long,
    pub mAttackGroupModifierMedium: c_long,
    pub mAttackGroupModifierHard: c_long,
    /// +0xd74 (EECPDefenseCommander 0x78866: the building count limit)
    pub mBonzaiAttackLevel: c_long,
    pub mLD78: c_long,
    pub mLD7C: c_long,
    pub mUnitCostReductionEasy: c_long,
    pub mUnitCostReductionHard: c_long,
    pub mDefenseGroupSizeMax: c_long,
    pub mDefenseGroupSizeMin: c_long,
    pub mEpochFrequencyMax: c_long,
    pub mEpochFrequencyMin: c_long,
    pub mEpochFrequencyModifierEasy: c_long,
    pub mEpochFrequencyModifierHard: c_long,
    pub mEpochCatchUpMin: c_long,
    pub mEpochCatchUpMax: c_long,
    pub mExpansionTimerEasy: c_long,
    pub mExpansionTimerMedium: c_long,
    pub mExpansionTimerHard: c_long,
    pub mHeroMaxPerGroup: c_long,
    pub mNumImprovedAttributes: c_long,
    pub mNumImprovedUnitTypes: c_long,
    pub mReplenishDelayMax: c_long,
    pub mReplenishDelayMin: c_long,
    pub mNumLandExplorers: c_long,
    pub mNumChokePointsToGuard: c_long,
    pub mAntiAirMax: c_long,
    pub mAirportMax: c_long,
    pub mArcheryRangeMax: c_long,
    pub mBarracksMax: c_long,
    pub mDockMax: c_long,
    pub mGranaryMax: c_long,
    pub mHospitalMax: c_long,
    pub mCyberFactoryMax: c_long,
    pub mCyberLabMax: c_long,
    pub mNavalYardMax: c_long,
    pub mSiegeFactoryMax: c_long,
    pub mSpaceDockMax: c_long,
    pub mStableMax: c_long,
    pub mTankFactoryMax: c_long,
    pub mTempleMax: c_long,
    pub mTowerMax: c_long,
    pub mUniversityMax: c_long,
    pub mMinMilitaryBuildingsEasy: c_long,
    pub mMinMilitaryBuildingsEasyDM: c_long,
    pub mMinMilitaryBuildingsMedium: c_long,
    pub mMinMilitaryBuildingsMediumDM: c_long,
    pub mMinMilitaryBuildingsHard: c_long,
    pub mMinMilitaryBuildingsHardDM: c_long,
    pub mPopLimitAdjustmentEasy: c_long,
    pub mPopLimitAdjustmentMedium: c_long,
    pub mPopLimitAdjustmentHard: c_long,
    pub mPriestMaxPerGroup: c_long,
    pub mProphetMaxPerGroup: c_long,
    /// +0xe40 (EECPResearchManager::SetNextTime 0x8b1fb scales it by GetResearchFrequencyModifier)
    pub mResearchFrequency: c_long,
    pub mResearchFrequencyModifierEasy: c_long,
    pub mResearchFrequencyModifierHard: c_long,
    pub mDropOffDistance: c_long,
    pub mExtraResources: c_long,
    pub mNumSeaExplorers: c_long,
    pub mTownCenterUpgradeTimer: c_long,
    pub mMaxEnabledManagers: c_long,
    pub mManagerAttackWait: c_long,
    pub mManagerEpochAdvanceWait: c_long,
    pub mEnableStockProduction: c_long,
    pub mEnableStockResource: c_long,
    pub mEnableStockCivilization: c_long,
    pub mEnableStockExploration: c_long,
    pub mEnableStockDefense: c_long,
    pub mEnableStockBuild: c_long,
    pub mLE80: c_long,
    pub mEnableStockResearch: c_long,
    pub mEnableStockCombat: c_long,
    pub mUnitsPerAntiAirBuilding: c_long,
    pub mUnitsPerAirport: c_long,
    pub mUnitsPerArcheryRange: c_long,
    pub mUnitsPerBarracks: c_long,
    pub mUnitsPerDock: c_long,
    pub mUnitsPerGranary: c_long,
    pub mUnitsPerHospital: c_long,
    pub mUnitsPerCyberFactory: c_long,
    pub mUnitsPerCyberLab: c_long,
    pub mUnitsPerNavalYard: c_long,
    pub mUnitsPerSiegeFactory: c_long,
    pub mUnitsPerSpaceDock: c_long,
    pub mUnitsPerStable: c_long,
    pub mUnitsPerTankFactory: c_long,
    pub mUnitsPerTemple: c_long,
    pub mUnitsPerTower: c_long,
    pub mUnitsPerUniversity: c_long,
    pub mCapitolUpgradeTimer: c_long,
    pub mMaxFlameThrowers: c_long,
    /// +0xed8
    pub mTownGuardUpdateEasy: c_long,
    /// +0xedc
    pub mTownGuardUpdateMedium: c_long,
    /// +0xee0
    pub mTownGuardUpdateHard: c_long,
    /// +0xee4
    pub mDMTownGuardUpdateEasy: c_long,
    /// +0xee8
    pub mDMTownGuardUpdateMedium: c_long,
    /// +0xeec
    pub mDMTownGuardUpdateHard: c_long,
    pub mDefensiveUnitUpdateEasy: c_long,
    pub mDefensiveUnitUpdateMedium: c_long,
    pub mDefensiveUnitUpdateHard: c_long,
    pub mDMDefensiveUnitUpdateEasy: c_long,
    pub mDMDefensiveUnitUpdateMedium: c_long,
    pub mDMDefensiveUnitUpdateHard: c_long,
    pub mIslandExpansionTimerEasy: c_long,
    pub mIslandExpansionTimerMedium: c_long,
    pub mIslandExpansionTimerHard: c_long,
    pub mLF14: c_long,
    pub mLF18: c_long,
    pub mLF1C: c_long,
    pub mLF20: c_long,
    pub mLF24: c_long,
    /// (unsigned: Load)
    pub mNextAttackTime: c_ulong,
    /// (unsigned: Load)
    pub mNextEnableTime: c_ulong,
    pub mLastDamagedTime: c_long,
    pub mNextPopulationTime: c_long,
    /// (unsigned: Load)
    pub mNextExpansionTime: c_ulong,
    /// (unsigned: Load)
    pub mDMEarlyAttackTime: c_ulong,
    /// (unsigned: Load)
    pub mNextIslandExpansionTime: c_ulong,
    pub mCanAttack: bool,
    /// +0xf45 (set: Update skips the managers)
    pub mIsStarted: bool,
    pub mIsEnableTimeSet: bool,
    pub mCitizenDamaged: bool,
    pub mBuildingDamaged: bool,
    /// +0xf49 (Update is only called while set; provisional name)
    pub mActive: bool,
    pub mUsesFishing: bool,
    pub mIsCheating: bool,
    pub mCanBuild: bool,
    pub mCanExplore: bool,
    pub mBF4E: bool,
    pub mCanGather: bool,
    pub mCanDefend: bool,
    pub mIsCitizenPercentageSet: bool,
    pub mIgnoreOthers: bool,
    pub mReplaceOnLoad: bool,
    pub mRegroupPending: bool,
    pub mAttackStarted: bool,
}
