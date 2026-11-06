// C:\EEX\Empire Earth\AI Thread\EEComputerPlayer.h (RECONSTRUCTION; file name inferred). A
// computer player (RTTI EEComputerPlayer > EEPlayer > RTSPlayer; vtable 0x43a57c; sizeof 0xf58; EEComputerPlayer.cpp
// 0x2ce2e..0x412b1, key "EEComputerPlayer-Data"; ctors 0x2cf7e(2 args) and 0x2d03d, ??_G 0x2d021, dtor 0x2d0bb). Only
// what the decompiled units need: its own slots are not declared yet (the EEPlayer ones are inherited); fields by
// offset (provisional names). Requires ee/EEPlayer.h.
#ifndef EE_EECOMPUTERPLAYER_H
#define EE_EECOMPUTERPLAYER_H

struct EEPUnitSwap;                         // (ee/EEPhysics.h)
class EECPCombatManager;                    // ee/EECPCombatManager.h
class EECPCivilizationManager;              // ee/EECPCivilizationManager.h
class EECPPlayerInteractionManager;         // ee/EECPPlayerInteractionManager.h
class EECPDiplomacyManager;                 // ee/EECPDiplomacyManager.h
class EECPExplorationManager;               // ee/EECPExplorationManager.h
class EECPResourceCenter;                   // ee/EECPCivilizationManager.h
class EEAIFOUnitSelectionScreener;          // ee/EECPUnitSelector.h
class EEAIFOUnitsToKillScoringBase;         // ee/EEAIFOUnitsToKillScoringBase.h

class EEComputerPlayer : public EEPlayer {
public:
    static const UString sData;             // 0x4e5c70 "EEComputerPlayer-Data" (chain 0x2ce2e; guard 0x4e5c68 bit 1) (provisional name)
    static const UString sCPUOnlyFlag;      // 0x4e5c58 "EEComputerPlayer-CPU Only Flag" (chain 0x2ce71; bit 2) (provisional name)
    bool CanEnableManager(long inKind, long inResource);  // 0x3eeca, signed resource threshold, ret8/AL
    EEComputerPlayer();                     // 0x2d03d (EEPPlayerHelper's CreatePlayer)
    virtual ~EEComputerPlayer();            // +0x00 (??_G 0x2d021, dtor 0x2d0bb)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x18 0x2fd50 (no /Og)
    virtual long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion);   // +0x20 0x3cff8
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x24 0x3d677
    // the computer player's overrides (ret N from the bodies) and members (provisional names)
    EEComputerPlayer(const UWideString& inName, unsigned long inPlayerID);   // 0x2cf7e
    void Init();                            // 0x2f1c8 (both ctors: the tuning members from the AI globals, the managers)
    virtual void AddResource(long inIndex, long inAmount);  // +0x48 0x2d1a5
    virtual void DetachUnitFromAI(EEComplexUnit* inUnit);  // +0x4c 0x2db35
    virtual long GetHeroCount(bool inArg1, bool inArg2);    // +0x50 0x2e248 (the heroes count)
    virtual bool CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason);  // +0x54 0x2dc51
    virtual void NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag);  // +0x58 0x2dca6
    virtual void Virtual60();               // +0x60 0x2e1e4
    virtual void UnitCompleted(EEComplexUnit* inUnit);  // +0x68 0x2e1e9
    virtual void SetCombatState(unsigned long inCombatState);  // +0xa8 0x2d003
    virtual long ScaleCost(long inCost);    // +0x6c 0x2eace (scaled by GetUnitCostModifier when mIsCheating, at least 0)
    virtual long GetExtraPopulation(long inUnitClass);  // +0x78 0x2ef32 (50 for 0x1e0 with a garrison or a fortress time)
    virtual long GetPopulationLimit(long inArg);  // +0x7c 0x2ecca (EEPlayer's when mIgnoreOthers, else mPopulationCap via WorkOutPopulation)
    virtual bool CanAfford(EETechnology* inTechnology, long inUnitType, long inCount);   // +0x80 0x2f033 (epochs: always)
    virtual void DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit);  // +0x84 0x2f16d
    // per-difficulty (mDifficulty 0/1/2) scales: a member in percent times 0.01, else 1 (provisional names)
    float GetAttackDelayModifier();         // 0x2eafc (+0xd40/+0xd44)
    float GetAttackFrequencyModifier();     // 0x2eb26 (+0xd24/+0xd28)
    float GetAttackGroupModifier();         // 0x2eb50 (+0xd68/+0xd70/+0xd6c)
    float GetUnitCostModifier();            // 0x2eb7c (+0xd80/+0xd84/+0xbec)
    float GetDMAttackFrequencyModifier();   // 0x2eba8 (+0xcac/+0xcb0/+0xcb4)
    float GetDMInitialAttackModifier();     // 0x2ebd9 (+0xcb8/+0xcbc/+0xcc0)
    bool Fn2EC67();                         // 0x2ec67 (the resources option is 5)
    float GetPopLimitAdjustment();          // 0x2ec74 (+0xe2c/+0xe34/+0xe30)
    unsigned long GetIslandExpansionTimer();  // 0x2ed01 (+0xf08/+0xf0c/+0xf10, else 60000)
    bool IsAttackDue(unsigned long inTime);  // 0x2ed2f (mCanAttack and inTime >= mNextAttackTime)
    virtual void Clear();                   // +0x1c 0x3cf73 (resets the centers, the exploration and combat managers)
    virtual void EpochMediaLoaded(long inEpoch);  // +0x88 0x3c992
    virtual void FlareReceived(EEPlayer* inFrom, const U2DPoint<long>& inTile);  // same EEPlayer slot, 0x3ce2e
    virtual void UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps);  // +0x9c 0x3ced7 (the EEPlayer slot)
    virtual void PayImprovement(EEUnitClass* inClass, long inImprovement);  // same EEPlayer slot, 0x3edc0
    virtual void RefreshTownGroups();       // +0x90 0x3cec9
    // 0x2dccf/0x2de60/0x2df8c/0x2e0b8: a random favorite unit class per screener flag into mFavoriteAirType/mFavoriteLandType/mFavoriteSeaType/mFavoriteSpaceType,
    // recorded with the epoch in mBA0 (EnterEpoch calls all four) (provisional names)
    void ChooseFavoriteAirType();
    void ChooseFavoriteLandType();
    void ChooseFavoriteSeaType();
    void ChooseFavoriteSpaceType();
    void Regroup();                         // 0x2e2a3 (Attacked when there is no resource center)
    void EnterEpoch(long inEpoch);          // 0x3c9c5 (VirtualA4: a new epoch)
    void UpdateGroups();                    // 0x3f3f1
    void EnableManagers(unsigned long inTime);  // 0x2d334 (Update) (provisional name)
    void SetNextAttackTime(unsigned long inTime);  // 0x3d4cd (Update) (provisional name)
    void ChooseCivilization();              // 0x3ca49 (Update, unless gGameOptions.mLoadType) (provisional name)
    void FlagGroup();                       // 0x2e6ae (Update at mNextExpansionTime) (provisional name)
    void GetObjectsToGroup(std::vector<EEUnit*>& outUnits, long& outCount);  // 0x2edff (ret 8) (provisional name)
    virtual void EpochReached(long inEpoch);  // +0xa4 0x3d45d
    virtual void UnitStranded(EEComplexUnit* inUnit);  // +0xac 0x3ed48
    virtual void Pay(EETechnology* inTechnology, long inUnitType, float inFraction);   // +0xb4 0x3ed98 (epochs: free)
    virtual void TakeResource(long inIndex, long inAmount);  // +0xb8 0x3ed81 (only a positive inArg2)
    virtual void UnitPlaced(EEComplexUnit* inUnit);  // +0xc0 0x3ef5d
    virtual void UnitTaken(RTSUnit* inUnit, bool inSwapped);  // +0xc4 0x3ef91 (the EEPlayer slot: a unit leaves)
    virtual void UnitGoalDone(EEComplexUnit* inUnit); // original ret4, Player slot C8
    void SendToNearestBase(EEComplexUnit* inUnit); // original thiscall ret4
    virtual void EpochMediaUnloaded(long inEpoch);  // +0xcc 0x3f142
    virtual bool AvoidsEnemies(EEComplexUnit* inUnit);  // +0xd0 0x3f73e (a citizen, or a transport holding only citizens)
    virtual void WonderStarted(EEComplexUnit* inUnit);  // +0xd4 0x3f7ba
    void SetEnableTime(long inTime);        // 0x3d492: mNextEnableTime = mEnableInterval seconds after inTime, mIsEnableTimeSet (provisional name)
    void SetNextExpansionTime(unsigned long inTime);  // 0x3d61a: mNextExpansionTime = inTime + the per-difficulty seconds (provisional name)
    void SetActive(bool inActive);          // 0x3ec42 (sets mActive, telling the unit manager) (provisional name)
    void SetCitizenPercentage(float inValue);  // 0x3ec89 (mCitizenPercentage, mIsCitizenPercentageSet) (provisional name)
    void SetDMAttackFrequencyModifier(float inValue);  // 0x3ec9d (mDMAttackFrequencyModifierEasy/mDMAttackFrequencyModifierMedium/mDMAttackFrequencyModifierHard by difficulty) (provisional name)
    void SetDMInitialAttackModifier(float inValue);  // 0x3ecd6 (mDMInitialAttackModifierEasy/mDMInitialAttackModifierMedium/mDMInitialAttackModifierHard) (provisional name)
    void SetIslandExpansionTimer(unsigned long inValue);  // 0x3ed0f (mIslandExpansionTimerEasy/mIslandExpansionTimerMedium/mIslandExpansionTimerHard) (provisional name)
    unsigned long GetBuildableTypes(long inCategory, std::vector<long>& outTypes);  // 0x2ed85 (ret 8): the buildable classes of a category (provisional name)
    virtual void RefreshResourceCenters();  // +0xdc 0x3ce56 (the computer player's own slot; EEUserInterface 0x276b97)
    void CatchUpEpoch(EEPlayer* inPlayer);  // 0x2f064 (EEPlayer::EpochReached calls it for each computer player; provisional name)
    void Update(unsigned long inTime);      // 0x3f175 (gArtificialIntelligence's round robin; provisional name)
    // 0x2eca0: 1.0, or +0xe44/+0xe48 times a constant depending on +0xbd8 (0 or 2) (provisional name)
    float GetResearchFrequencyModifier();
    float GetEpochFrequencyModifier();      // 0x2ec0a (EECPCivilizationManager::SetNextEpochAdvanceTime scales by it) (provisional name)
    // inline (SetNextEpochAdvanceTime 0x70b43 loads both into registers before its fild spills) (provisional name)
    // inline (its COMDAT 0x68849 sits in EECPBuildManager.cpp's range; the civilization manager calls it out of line)
    // Provisional name
    unsigned long GetNumResourceCenters() { return this->mResourceCenters.size(); }
    EECPResourceCenter* GetResourceCenter(unsigned long inIndex);   // 0x2ef5e (ret 4: mResourceCenters[inIndex]) (provisional name)
    unsigned short GetObjectRegion(EEComplexUnit* inUnit);  // 0x2e9e7 (ret 4): a path area id for inUnit (EECPWallManager::Update) (provisional name)
    EECPResourceCenter* GetResourceCenterOf(EEUnit* inUnit);  // 0x2ef6e (ret 4): the resource center holding inUnit (provisional name)
    void SetNextIslandExpansionTime();      // 0x3d652 (EECPResourceManager 0x8ecfd) (provisional name)
    long GetEpochFrequencyMax() const { return this->mEpochFrequencyMax; }
    long GetEpochFrequencyMin() const { return this->mEpochFrequencyMin; }
    // inline (EECPDefenseCommander's ctor 0x77bce loads both into registers before the pushes) (provisional name)
    long GetReplenishDelayMax() const { return this->mReplenishDelayMax; }
    long GetReplenishDelayMin() const { return this->mReplenishDelayMin; }
    // inline (EEComputerPlayer::SetNextAttackTime 0x43d4cd loads each pair into registers before the GetRandom pushes
    // and the clamps, and mAttackFrequencyPerEpoch with fld before the fimul) (provisional names)
    long GetAttackFrequencyCeiling() const { return this->mAttackFrequencyCeiling; }
    long GetAttackFrequencyFloor() const { return this->mAttackFrequencyFloor; }
    long GetAttackFrequencyMax() const { return this->mAttackFrequencyMax; }
    long GetAttackFrequencyMin() const { return this->mAttackFrequencyMin; }
    long GetAttackDelayCeiling() const { return this->mAttackDelayCeiling; }
    long GetAttackDelayFloor() const { return this->mAttackDelayFloor; }
    long GetInitialAttackDelayMax() const { return this->mInitialAttackDelayMax; }
    long GetInitialAttackDelayMin() const { return this->mInitialAttackDelayMin; }
    float GetAttackFrequencyPerEpoch() const { return this->mAttackFrequencyPerEpoch; }
    // inline float getters: the AI compares load them onto the x87 stack (fld [mAidThreatThreshold]; fld st(1); fcompp) instead of
    // comparing against a memory operand (EECPMilitaryAidPackage 0x485892..0x485a2d) (provisional names)
    float GetTownGuardPercentage() const { return this->mTownGuardPercentage; }
    float GetCitizenPercentage() const { return this->mCitizenPercentage; }  // (EECPCombatManager 0x47342e: fld; fsubr [1.0])
    float GetAidThreatThreshold() const { return this->mAidThreatThreshold; }
    float GetWorkerCountWeight() const { return this->mWorkerCountWeight; }
    float GetMilitaryCountWeight() const { return this->mMilitaryCountWeight; }
    void SetCanAttack(bool inValue) { this->mCanAttack = inValue; }  // inline (EESMEffect::SetAIToggle 0x5c26da: 7 free sites set the budget)
    void SetIsCheating(bool inValue) { this->mIsCheating = inValue; }
    void SetCanBuild(bool inValue) { this->mCanBuild = inValue; }
    void SetCanExplore(bool inValue) { this->mCanExplore = inValue; }
    void SetCanGather(bool inValue) { this->mCanGather = inValue; }
    void SetCanDefend(bool inValue) { this->mCanDefend = inValue; }
    void SetIgnoreOthers(bool inValue) { this->mIgnoreOthers = inValue; }
    void SetFavoriteLandType(unsigned long inValue) { this->mFavoriteLandType = inValue; }  // inline (EESMEffect 0x5c2507)
    void SetFavoriteSeaType(unsigned long inValue) { this->mFavoriteSeaType = inValue; }
    void SetFavoriteAirType(unsigned long inValue) { this->mFavoriteAirType = inValue; }
    bool IsDeathmatch();                    // 0x2ec52 (gWorld.mGameOptions.mResources == 4 and a byte clear) (provisional name)
    bool IsIslandExpansionDue();            // 0x2ed4b: the map flag 1/4 and gPhysics time >= mNextIslandExpansionTime (EECPResourceManager::AssignGatherers) (provisional name)
    void WorkOutPopulation();               // 0x2d1c7 (EECPDefenseCommander 0x78148) (provisional name)
    // 0x2e851 (ret 8): deletes inScorer.mCount own units by its scores (EECPDefenseCommander) (provisional name)
    long ChooseUnitsToKill(EEAIFOUnitsToKillScoringBase& inScorer, bool inKill);  // (returns the count: eax)
    // 0x2db6d (ret 8): fills ioMap with the classes inScreener accepts (EECPUnitSelector::GatherCandidates)
    // (provisional name)
    void GetScreenedTypesByLine(std::map<long, std::vector<long>*>& ioMap, EEAIFOUnitSelectionScreener& inScreener);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    void GetScreenedTypesByLine(std::map<long, std::vector<long>*>& ioMap, const EEAIFOUnitSelectionScreener& inScreener)
        { GetScreenedTypesByLine(ioMap, const_cast<EEAIFOUnitSelectionScreener&>(inScreener)); }
#endif
    // The scenario effect's AI tuning setters: inline, defined in EESMEffect.cpp (their only user, SetAIVariable, calls
    // them out of line: COMDATs 0x1c1ca9..0x1c24c8, 63 B each). Names are provisional (by field).
    void SetAntiAirMax(long inValue);  // 0x1c1ca9
    void SetAirportMax(long inValue);  // 0x1c1ce8
    void SetArcheryRangeMax(long inValue);  // 0x1c1d27
    void SetBarracksMax(long inValue);  // 0x1c1d66
    void SetDockMax(long inValue);  // 0x1c1da5
    void SetGranaryMax(long inValue);  // 0x1c1de4
    void SetHospitalMax(long inValue);  // 0x1c1e23
    void SetCyberFactoryMax(long inValue);  // 0x1c1e62
    void SetCyberLabMax(long inValue);  // 0x1c1ea1
    void SetNavalYardMax(long inValue);  // 0x1c1ee0
    void SetSiegeFactoryMax(long inValue);  // 0x1c1f1f
    void SetSpaceDockMax(long inValue);  // 0x1c1f5e
    void SetStableMax(long inValue);  // 0x1c1f9d
    void SetTankFactoryMax(long inValue);  // 0x1c1fdc
    void SetTempleMax(long inValue);  // 0x1c201b
    void SetTowerMax(long inValue);  // 0x1c205a
    void SetUniversityMax(long inValue);  // 0x1c2099
    void SetUnitsPerAntiAirBuilding(long inValue);  // 0x1c20d8
    void SetUnitsPerAirport(long inValue);  // 0x1c2117
    void SetUnitsPerArcheryRange(long inValue);  // 0x1c2156
    void SetUnitsPerBarracks(long inValue);  // 0x1c2195
    void SetUnitsPerDock(long inValue);  // 0x1c21d4
    void SetUnitsPerGranary(long inValue);  // 0x1c2213
    void SetUnitsPerHospital(long inValue);  // 0x1c2252
    void SetUnitsPerCyberFactory(long inValue);  // 0x1c2291
    void SetUnitsPerCyberLab(long inValue);  // 0x1c22d0
    void SetUnitsPerNavalYard(long inValue);  // 0x1c230f
    void SetUnitsPerSiegeFactory(long inValue);  // 0x1c234e
    void SetUnitsPerSpaceDock(long inValue);  // 0x1c238d
    void SetUnitsPerStable(long inValue);  // 0x1c23cc
    void SetUnitsPerTankFactory(long inValue);  // 0x1c240b
    void SetUnitsPerTemple(long inValue);  // 0x1c244a
    void SetUnitsPerTower(long inValue);  // 0x1c2489
    void SetUnitsPerUniversity(long inValue);  // 0x1c24c8

    // +0xb80 (0x68849 = its size(), 0x2ef5e = operator[]: EECPCivilizationManager 0x6fb5e/0x711ac)
    std::vector<EECPResourceCenter*> mResourceCenters;
    std::vector<UWideString> mB90;          // +0xb90 (20-byte polymorphic elements: _Tidy 0x40ee8; unused here) (provisional name)
    std::vector<std::pair<unsigned long, long> > mFavoriteTypeHistory;  // +0xba0 (Load: resize(n, make_pair(0, 0)), then first/second)
    std::vector<EEComplexUnit*> mUnitsToKill;  // +0xbb0 (ChooseUnitsToKill push_backs the units it kills; the dtor dereferences each)
    EECPCombatManager* mCombatManager;      // +0xbc0 (EEAttackPlayers 0x13f5e9) (provisional name)
    EECPCivilizationManager* mCivilizationManager;   // +0xbc4 (EECPUnitSelector 0x97a34: its +0x3c build manager) (provisional name)
    EECPDiplomacyManager* mDiplomacyManager;   // +0xbc8 (new 8: Init 0x2f1c8; deleted in the dtor) (provisional name)
    EECPExplorationManager* mExplorationManager;   // +0xbcc (new 0x30: Init; Load calls its +0x08) (provisional name)
    EECPPlayerInteractionManager* mInteractionManager;   // +0xbd0 (new 0x2c; EECPCombatManager 0x472f39 calls SendChat on it) (provisional name)
    unsigned long mLBD4;   // (unsigned: Load)
    unsigned long mDifficulty;                       // +0xbd8 0 or 2 (GetResearchFrequencyModifier; CheckBuildingCounts picks +0xe14/+0xe24/+0xe1c by it) (provisional name; unsigned: Load)
    unsigned long mFavoriteLandType;
    unsigned long mFavoriteSeaType;
    unsigned long mFavoriteAirType;
    unsigned long mFavoriteSpaceType;
    float mUnitCostMedium;
    float mComputerAggression;              // +0xbf0 (fld at 0x47346f)
    float mCarrierChance;
    float mCarrierPopPercentage;
    float mAttackFrequencyPerEpoch;
    float mSpecializedUnitPercentage;
    float mRPSSelectionNonIslands;
    float mRPSSelectionIslands;
    float mSpecializedUnitChance;
    float mTownGuardPercentage;             // +0xc10 (EECPDefenseCommander 0x783e7: the aid chance)
    float mCitizenPercentage;               // +0xc14 (fld at 0x4734d2)
    float mDMEarlyAttackScale;
    float mEpochCatchUpEasy;
    float mEpochCatchUpHard;
    float mEpochCatchUpDeathmatch;
    float mGuardContinentPercentage;        // +0xc28
    float mGuardChokepointAirPercentage;    // +0xc2c
    float mGuardChokepointPercentage;       // +0xc30
    float mGuardHighGroundPercentage;       // +0xc34
    float mFC38;
    float mFC3C;
    float mFC40;
    float mFC44;
    float mFC48;
    float mFC4C;
    float mFC50;
    float mFC54;
    float mFC58;
    float mGuardTownPercentage;             // +0xc5c
    float mGuardTownLandPercentage;         // +0xc60
    float mGuardWaterwayPercentage;
    float mHeroChance;
    float mHeroPopPercentage;
    float mDefensiveUnitCap;                // +0xc70 (EECPDefenseCommander 0x799fa: the defense share)
    float mSiegeWeaponPercentage;  // (float: Load/Init)
    float mMaxAttackResponseRange;  // (float: Load/Init)
    float mMilitaryPopPercentage;  // (float: Load/Init)
    float mAidThreatThreshold;              // +0xc80 (EECPDefenseCommander 0x785cb: the allies' threat limit)
    float mPriestPopPercentage;  // (float: Load/Init)
    float mPriestChance;  // (float: Load/Init)
    float mProphetPopPercentage;  // (float: Load/Init)
    float mProphetChance;  // (float: Load/Init)
    float mEnableInterval;  // (float: Load/Init)
    float mWorkerCountWeight;               // +0xc98 (EECPIntelligencePlayerFile::Update: the worker-count weight) (provisional name)
    float mMilitaryCountWeight;             // +0xc9c (Update: the military-count weight) (provisional name)
    float mNavalUnitWeight;  // (float: Load/Init)
    float mAirUnitWeight;  // (float: Load/Init)
    float mTowerSpacing;                    // +0xca8 (float: EECPResourceCenter::FindBuildSite 0x48bee3 flds it)
    float mDMAttackFrequencyModifierEasy;  // (float: Load/Init)
    float mDMAttackFrequencyModifierMedium;  // (float: Load/Init)
    float mDMAttackFrequencyModifierHard;  // (float: Load/Init)
    float mDMInitialAttackModifierEasy;  // (float: Load/Init)
    float mDMInitialAttackModifierMedium;  // (float: Load/Init)
    float mDMInitialAttackModifierHard;  // (float: Load/Init)
    long mLCC4;
    long mLCC8;
    long mLCCC;
    long mLCD0;
    long mLCD4;
    long mSeed;                             // +0xcd8 (EECPPlayerInteractionManager's ctor 0x895de copies it)
    long mShipsToMaintainDMEasy;
    long mShipsToMaintainDMMedium;
    long mShipsToMaintainDMHard;
    long mShipsToMaintainEasy;
    long mShipsToMaintainMedium;
    long mShipsToMaintainHard;
    long mShipMaintenanceTimer;
    long mAssistAlliesDelayEasy;            // +0xcf8 (EECPMilitaryAidPackage 0x84e42: the aid delay against a human, by mDifficulty) (provisional name)
    long mAssistAlliesDelayMedium;          // +0xcfc
    long mAssistAlliesDelayHard;            // +0xd00
    long mPopulationCap;
    long mCarrierMaxPerGroup;
    long mNumAirExplorers;                  // +0xd0c (EECPExplorationManager::Update: the wanted explorers of kind 2 when +0xf55)
    long mAttackEnemyChance;
    long mAttackToClaimResourceChance;
    long mAttackForRevengeChance;
    long mAttackGroupSizeMax;
    long mAttackGroupSizeMin;
    long mAttackFrequencyModifierEasy;
    long mAttackFrequencyModifierHard;
    long mAttackFrequencyCeiling;
    long mAttackFrequencyFloor;
    long mAttackFrequencyMax;
    long mAttackFrequencyMin;
    long mInitialAttackDelayPerEpoch;
    long mAttackDelayModifierEasy;
    long mAttackDelayModifierHard;
    long mLD48;
    long mLD4C;
    long mLD50;
    long mLD54;
    long mAttackDelayCeiling;
    long mAttackDelayFloor;
    long mInitialAttackDelayMax;            // +0xd60 (EECPDefenseCommander 0x78866: minutes before the citizens' raid)
    long mInitialAttackDelayMin;
    long mAttackGroupModifierEasy;
    long mAttackGroupModifierMedium;
    long mAttackGroupModifierHard;
    long mBonzaiAttackLevel;                // +0xd74 (EECPDefenseCommander 0x78866: the building count limit)
    long mLD78;
    long mLD7C;
    long mUnitCostReductionEasy;
    long mUnitCostReductionHard;
    long mDefenseGroupSizeMax;
    long mDefenseGroupSizeMin;
    long mEpochFrequencyMax;
    long mEpochFrequencyMin;
    long mEpochFrequencyModifierEasy;
    long mEpochFrequencyModifierHard;
    long mEpochCatchUpMin;
    long mEpochCatchUpMax;
    long mExpansionTimerEasy;
    long mExpansionTimerMedium;
    long mExpansionTimerHard;
    long mHeroMaxPerGroup;
    long mNumImprovedAttributes;
    long mNumImprovedUnitTypes;
    long mReplenishDelayMax;
    long mReplenishDelayMin;
    long mNumLandExplorers;
    long mNumChokePointsToGuard;
    long mAntiAirMax;
    long mAirportMax;
    long mArcheryRangeMax;
    long mBarracksMax;
    long mDockMax;
    long mGranaryMax;
    long mHospitalMax;
    long mCyberFactoryMax;
    long mCyberLabMax;
    long mNavalYardMax;
    long mSiegeFactoryMax;
    long mSpaceDockMax;
    long mStableMax;
    long mTankFactoryMax;
    long mTempleMax;
    long mTowerMax;
    long mUniversityMax;
    long mMinMilitaryBuildingsEasy;
    long mMinMilitaryBuildingsEasyDM;
    long mMinMilitaryBuildingsMedium;
    long mMinMilitaryBuildingsMediumDM;
    long mMinMilitaryBuildingsHard;
    long mMinMilitaryBuildingsHardDM;
    long mPopLimitAdjustmentEasy;
    long mPopLimitAdjustmentMedium;
    long mPopLimitAdjustmentHard;
    long mPriestMaxPerGroup;
    long mProphetMaxPerGroup;
    long mResearchFrequency;                // +0xe40 (EECPResearchManager::SetNextTime 0x8b1fb scales it by GetResearchFrequencyModifier)
    long mResearchFrequencyModifierEasy;
    long mResearchFrequencyModifierHard;
    long mDropOffDistance;
    long mExtraResources;
    long mNumSeaExplorers;
    long mTownCenterUpgradeTimer;
    long mMaxEnabledManagers;
    long mManagerAttackWait;
    long mManagerEpochAdvanceWait;
    long mEnableStockProduction;
    long mEnableStockResource;
    long mEnableStockCivilization;
    long mEnableStockExploration;
    long mEnableStockDefense;
    long mEnableStockBuild;
    long mLE80;
    long mEnableStockResearch;
    long mEnableStockCombat;
    long mUnitsPerAntiAirBuilding;
    long mUnitsPerAirport;
    long mUnitsPerArcheryRange;
    long mUnitsPerBarracks;
    long mUnitsPerDock;
    long mUnitsPerGranary;
    long mUnitsPerHospital;
    long mUnitsPerCyberFactory;
    long mUnitsPerCyberLab;
    long mUnitsPerNavalYard;
    long mUnitsPerSiegeFactory;
    long mUnitsPerSpaceDock;
    long mUnitsPerStable;
    long mUnitsPerTankFactory;
    long mUnitsPerTemple;
    long mUnitsPerTower;
    long mUnitsPerUniversity;
    long mCapitolUpgradeTimer;
    long mMaxFlameThrowers;
    long mTownGuardUpdateEasy;          // +0xed8
    long mTownGuardUpdateMedium;        // +0xedc
    long mTownGuardUpdateHard;          // +0xee0
    long mDMTownGuardUpdateEasy;        // +0xee4
    long mDMTownGuardUpdateMedium;      // +0xee8
    long mDMTownGuardUpdateHard;        // +0xeec
    long mDefensiveUnitUpdateEasy;
    long mDefensiveUnitUpdateMedium;
    long mDefensiveUnitUpdateHard;
    long mDMDefensiveUnitUpdateEasy;
    long mDMDefensiveUnitUpdateMedium;
    long mDMDefensiveUnitUpdateHard;
    long mIslandExpansionTimerEasy;
    long mIslandExpansionTimerMedium;
    long mIslandExpansionTimerHard;
    long mLF14;
    long mLF18;
    long mLF1C;
    long mLF20;
    long mLF24;
    unsigned long mNextAttackTime;  // (unsigned: Load)
    unsigned long mNextEnableTime;  // (unsigned: Load)
    long mLastDamagedTime;
    long mNextPopulationTime;
    unsigned long mNextExpansionTime;  // (unsigned: Load)
    unsigned long mDMEarlyAttackTime;  // (unsigned: Load)
    unsigned long mNextIslandExpansionTime;  // (unsigned: Load)
    bool mCanAttack;
    bool mIsStarted;                        // +0xf45 (set: Update skips the managers)
    bool mIsEnableTimeSet;
    bool mCitizenDamaged;
    bool mBuildingDamaged;
    bool mActive;                           // +0xf49 (Update is only called while set; provisional name)
    bool mUsesFishing;
    bool mIsCheating;
    bool mCanBuild;
    bool mCanExplore;
    bool mBF4E;
    bool mCanGather;
    bool mCanDefend;
    bool mIsCitizenPercentageSet;
    bool mIgnoreOthers;
    bool mReplaceOnLoad;
    bool mRegroupPending;
    bool mAttackStarted;
    char _gapF56[0xf58 - 0xf56];
};

// Computer player difficulty (EEComputerPlayer::mDifficulty, from the game options).
enum EECPDifficulty {
    kDifficultyEasy = 0,  // Language.rc 28504 "The values are Easy, Medium, and Hard Difficulty, respectively" (record 4 value1/value2/float -> mAssistAlliesDelayEasy/Medium/Hard, used for mDifficulty 0/1/2); 29665..29667 "Easy"/"Medium"/"Hard"; GetX getters 0x42eafc.. test 0/1/2
    kDifficultyMedium = 1,  // Language.rc 28504 "The values are Easy, Medium, and Hard Difficulty, respectively" (record 4 value1/value2/float -> mAssistAlliesDelayEasy/Medium/Hard, used for mDifficulty 0/1/2); 29665..29667 "Easy"/"Medium"/"Hard"; GetX getters 0x42eafc.. test 0/1/2
    kDifficultyHard = 2,  // Language.rc 28504 "The values are Easy, Medium, and Hard Difficulty, respectively" (record 4 value1/value2/float -> mAssistAlliesDelayEasy/Medium/Hard, used for mDifficulty 0/1/2); 29665..29667 "Easy"/"Medium"/"Hard"; GetX getters 0x42eafc.. test 0/1/2
};

// Computer player manager kinds of EnableManagers 0x42d334 / CanEnableManager 0x43eeca.
enum EECPManagerKind {
    kCPManagerCivilization = 0,   // setrilo ComputerCanAfford 0x43f66f case 0 enableStockCivilization_; EnableManagers case 0 sets EECPCivilizationManager::mEpochAdvanceEnabled
    kCPManagerResource = 1,       // setrilo case 1 enableStockResource_; EnableManagers case 1 sets EECPResourceManager::mSettlementsEnabled
    kCPManagerResearch = 2,       // setrilo case 2 enableStockResearch_; EnableManagers case 2 sets EECPResearchManager::mEnabled
    kCPManagerProduction = 3,     // setrilo case 3 enableStockProduction_; EnableManagers case 3 sets EECPUnitManager::mProductionEnabled
    kCPManagerBuild = 4,          // setrilo case 4 enableStockBuild_; EnableManagers case 4 sets EECPCivilizationManager::mBuildingCountsEnabled
    kCPManagerDefense = 6,        // setrilo case 6 enableStockDefense_; EnableManagers case 6 sets EECPDefenseCommander::mEnabled
    kCPManagerExploration = 7,    // setrilo case 7 enableStockExploration_; EnableManagers case 7 sets EECPExplorationManager::mEnabled
    kCPManagerCombat = 8,         // setrilo case 8 enableStockCombat_; EnableManagers case 8 sets EECPCombatManager::mImprovementsEnabled
    kCPManagerMoraleBuildings = 9,  // CanEnableManager case 9 uses mEnableStockBuild; EnableManagers case 9 sets EECPCivilizationManager::mMoraleBuildingsEnabled (QueueMoraleBuilding timer) (provisional name)
};

// New (docs/AI_RESEARCH.md §8.7): the epoch advance the player's research manager has queued and not yet started
// (kTechNone: none); without the free-epochs cheat its cost is reserved
long EECPPendingEpoch(EEComputerPlayer* inPlayer);

// New (docs/AI_RESEARCH.md §8.8): the epoch pressure, the elapsed share of the epoch timer's span (dbcpbehavior rows
// 28/29, the mean of the range times the difficulty's modifier): 0 when the timer starts, 1 when it is due (the
// original's only advance time); a paid advance is asked for from the personality's epoch_pressure start, and its
// reserve grows from nothing there to the whole cost at its full (aipersonalities.yaml; tall low, wide high).
float EECPEpochPressure(EEComputerPlayer* inPlayer, unsigned long inTime);
unsigned long EECPEpochTimerSpan(EEComputerPlayer* inPlayer);   // ms

#endif
