// New (port): EESMEffect::SetAIVariable's computer player fields (ee_scenario aivar.rs: a write's field number is
// its position in its list). The table in EESMEffect.cpp and the golden driver's stand-in both expand these lists;
// the 34 building counts also set the civilization manager's copy under the unit type, as the original's inline
// setters did. Requires ee/EEUnitClassIDs.h.
#ifndef EE_EESMAIFIELDS_H
#define EE_EESMAIFIELDS_H

// the long fields (EE_SCENARIO_AI_KIND_LONG)
#define EE_SM_AI_LONGS(X) \
    X(mInitialAttackDelayMax) X(mInitialAttackDelayMin) X(mAttackDelayModifierEasy) \
    X(mAttackDelayModifierHard) X(mAssistAlliesDelayEasy) X(mAssistAlliesDelayMedium) \
    X(mAssistAlliesDelayHard) X(mAttackFrequencyMin) X(mAttackFrequencyMax) \
    X(mAttackFrequencyModifierEasy) X(mAttackFrequencyModifierHard) X(mNumLandExplorers) \
    X(mNumSeaExplorers) X(mNumAirExplorers) X(mAttackGroupModifierEasy) \
    X(mAttackGroupModifierMedium) X(mAttackGroupModifierHard) X(mAttackGroupSizeMin) \
    X(mAttackGroupSizeMax) X(mDropOffDistance) X(mResearchFrequency) \
    X(mNumImprovedUnitTypes) X(mNumImprovedAttributes) X(mLD7C) \
    X(mLD78) X(mInitialAttackDelayPerEpoch) X(mAttackDelayFloor) \
    X(mAttackDelayCeiling) X(mAttackFrequencyFloor) X(mAttackFrequencyCeiling) \
    X(mResearchFrequencyModifierEasy) X(mResearchFrequencyModifierHard) X(mAttackEnemyChance) \
    X(mAttackToClaimResourceChance) X(mAttackForRevengeChance) X(mEpochFrequencyMin) \
    X(mEpochFrequencyMax) X(mPriestMaxPerGroup) X(mProphetMaxPerGroup) \
    X(mHeroMaxPerGroup) X(mCarrierMaxPerGroup) X(mEnableStockCivilization) \
    X(mEnableStockResource) X(mEnableStockResearch) X(mEnableStockProduction) \
    X(mEnableStockBuild) X(mLE80) X(mEnableStockDefense) \
    X(mEnableStockExploration) X(mEnableStockCombat) X(mManagerEpochAdvanceWait) \
    X(mManagerAttackWait) X(mMaxEnabledManagers) X(mNumChokePointsToGuard) \
    X(mDefenseGroupSizeMin) X(mDefenseGroupSizeMax) X(mDefensiveUnitUpdateEasy) \
    X(mDefensiveUnitUpdateMedium) X(mDefensiveUnitUpdateHard) X(mReplenishDelayMin) \
    X(mReplenishDelayMax) X(mUnitCostReductionEasy) X(mUnitCostReductionHard) \
    X(mEpochFrequencyModifierEasy) X(mEpochFrequencyModifierHard) X(mExpansionTimerEasy) \
    X(mExpansionTimerMedium) X(mExpansionTimerHard) X(mPopLimitAdjustmentEasy) \
    X(mPopLimitAdjustmentMedium) X(mPopLimitAdjustmentHard) X(mTownCenterUpgradeTimer) \
    X(mCapitolUpgradeTimer) X(mEpochCatchUpMin) X(mEpochCatchUpMax) \
    X(mMinMilitaryBuildingsEasy) X(mMinMilitaryBuildingsHard) X(mMinMilitaryBuildingsMedium) \
    X(mMaxFlameThrowers) X(mTownGuardUpdateEasy) X(mTownGuardUpdateMedium) \
    X(mTownGuardUpdateHard) X(mDMTownGuardUpdateEasy) X(mDMTownGuardUpdateMedium) \
    X(mDMTownGuardUpdateHard) X(mDMDefensiveUnitUpdateEasy) X(mDMDefensiveUnitUpdateMedium) \
    X(mDMDefensiveUnitUpdateHard) X(mShipMaintenanceTimer) X(mShipsToMaintainEasy) \
    X(mShipsToMaintainMedium) X(mShipsToMaintainHard) X(mShipsToMaintainDMEasy) \
    X(mShipsToMaintainDMMedium) X(mShipsToMaintainDMHard) X(mBonzaiAttackLevel)

// the float fields (EE_SCENARIO_AI_KIND_FLOAT)
#define EE_SM_AI_FLOATS(X) \
    X(mDefensiveUnitCap) X(mAttackFrequencyPerEpoch) X(mMilitaryCountWeight) \
    X(mWorkerCountWeight) X(mMilitaryPopPercentage) X(mSpecializedUnitPercentage) \
    X(mMaxAttackResponseRange) X(mPriestChance) X(mPriestPopPercentage) \
    X(mProphetChance) X(mProphetPopPercentage) X(mHeroChance) \
    X(mHeroPopPercentage) X(mCarrierChance) X(mCarrierPopPercentage) \
    X(mRPSSelectionNonIslands) X(mUnitCostMedium) X(mEnableInterval) \
    X(mAidThreatThreshold) X(mGuardChokepointPercentage) X(mGuardContinentPercentage) \
    X(mGuardHighGroundPercentage) X(mGuardTownLandPercentage) X(mFC44) \
    X(mFC50) X(mFC58) X(mGuardChokepointAirPercentage) \
    X(mGuardTownPercentage) X(mFC38) X(mFC40) \
    X(mFC4C) X(mGuardWaterwayPercentage) X(mFC3C) \
    X(mFC48) X(mFC54) X(mTownGuardPercentage) \
    X(mComputerAggression) X(mDMEarlyAttackScale) X(mSpecializedUnitChance) \
    X(mRPSSelectionIslands) X(mNavalUnitWeight) X(mAirUnitWeight) \
    X(mSiegeWeaponPercentage)

// the building counts (EE_SCENARIO_AI_KIND_MAX_COUNT: the maximum, EE_SCENARIO_AI_KIND_UNITS_PER: the units per
// building): the two fields and the unit type
#define EE_SM_AI_BUILDINGS(X) \
    X(mBarracksMax, mUnitsPerBarracks, kUnitTypeBarracks) \
    X(mArcheryRangeMax, mUnitsPerArcheryRange, kUnitTypeArcheryRange) \
    X(mStableMax, mUnitsPerStable, kUnitTypeStable) \
    X(mDockMax, mUnitsPerDock, kUnitTypeDock) \
    X(mSiegeFactoryMax, mUnitsPerSiegeFactory, kUnitTypeSiegeFactory) \
    X(mNavalYardMax, mUnitsPerNavalYard, kUnitTypeNavyYard) \
    X(mAirportMax, mUnitsPerAirport, kUnitTypeAirport) \
    X(mCyberFactoryMax, mUnitsPerCyberFactory, kUnitTypeCyberFactory) \
    X(mUniversityMax, mUnitsPerUniversity, kUnitTypeUniversity) \
    X(mTempleMax, mUnitsPerTemple, kUnitTypeTemple) \
    X(mHospitalMax, mUnitsPerHospital, kUnitTypeHospital) \
    X(mGranaryMax, mUnitsPerGranary, kUnitTypeFarm) \
    X(mCyberLabMax, mUnitsPerCyberLab, kUnitTypeCyberLaboratory) \
    X(mSpaceDockMax, mUnitsPerSpaceDock, kUnitTypeSpaceDock) \
    X(mTowerMax, mUnitsPerTower, kUnitTypeGuardTowerPaleo) \
    X(mTankFactoryMax, mUnitsPerTankFactory, kUnitTypeTankFactory) \
    X(mAntiAirMax, mUnitsPerAntiAirBuilding, kUnitTypeAA10Gun88mm)

#endif
