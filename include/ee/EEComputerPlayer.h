// C:\EEX\Empire Earth\AI Thread\EEComputerPlayer.h (RECONSTRUCTION; file name inferred). A
// computer player (RTTI EEComputerPlayer > EEPlayer > RTSPlayer; vtable 0x43a57c; sizeof 0xf58; EEComputerPlayer.cpp
// 0x2ce2e..0x412b1, key "EEComputerPlayer-Data"; ctors 0x2cf7e(2 args) and 0x2d03d, ??_G 0x2d021, dtor 0x2d0bb). Only
// what the decompiled units need: its own slots are not declared yet (the EEPlayer ones are inherited); fields by
// offset (provisional names). Requires ee/EEPlayer.h.
#ifndef EE_EECOMPUTERPLAYER_H
#define EE_EECOMPUTERPLAYER_H

#include "ee/EEUnitHandle.h"
#include "rust/sim/cp.h"             // EeCpFields (the plain members), the bodies over them

struct EEPUnitSwap;                         // (ee/EEPhysics.h)
class EECPCombatManager;                    // ee/EECPCombatManager.h
class EECPPlacement;                        // ee/EECPPlacement.h
class EECPResourceCenter;                   // ee/EECPResourceCenter.h
class EEAIFOUnitSelectionScreener;          // ee/EECPUnitSelector.h
class EEAIFOUnitsToKillScoringBase;         // ee/EEAIFOUnitsToKillScoringBase.h

// port (lane B of the AI Thread): the plain members (the tuning values, timers and flags Init sets and Load and Save
// read and write) are the Rust record EeCpFields (ee_sim::cp::fields), their bodies Rust's (cp::init, cp::save,
// cp::player); the containers, the managers and the engines' states stay C++ members
class EEComputerPlayer : public EEPlayer, public EeCpFields {
public:
    static const UString sData;             // 0x4e5c70 "EEComputerPlayer-Data" (chain 0x2ce2e; guard 0x4e5c68 bit 1) (provisional name)
    static const UString sCPUOnlyFlag;      // 0x4e5c58 "EEComputerPlayer-CPU Only Flag" (chain 0x2ce71; bit 2) (provisional name)
    EEComputerPlayer();                     // 0x2d03d (EEPPlayerHelper's CreatePlayer)
    virtual ~EEComputerPlayer();            // +0x00 (??_G 0x2d021, dtor 0x2d0bb)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x18 0x2fd50 (no /Og)
    virtual long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion);   // +0x20 0x3cff8
    // port: RepairPointers' managers part, run by EEWorld::CreatePlayers once every player is repaired (gDeferComputerPlayerRestore)
    long RestoreManagers(unsigned long inMajorVersion, unsigned long inMinorVersion, long inResult);
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x24 0x3d677
    // the computer player's overrides (ret N from the bodies) and members (provisional names)
    EEComputerPlayer(const UWideString& inName, unsigned long inPlayerID);
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
    float GetAttackDelayModifier() { return ee_cp_attack_delay_modifier(this); }
    float GetAttackFrequencyModifier() { return ee_cp_attack_frequency_modifier(this); }
    float GetUnitCostModifier() { return ee_cp_unit_cost_modifier(this); }
    float GetDMAttackFrequencyModifier() { return ee_cp_dm_attack_frequency_modifier(this); }
    float GetDMInitialAttackModifier() { return ee_cp_dm_initial_attack_modifier(this); }
    float GetPopLimitAdjustment() { return ee_cp_pop_limit_adjustment(this); }
    unsigned long GetIslandExpansionTimer() { return ee_cp_island_expansion_timer(this); }   // 0x2ed01 (else 60000)
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
    void EnterEpoch(long inEpoch);          // 0x3c9c5 (GetMaxWidth: a new epoch)
    void UpdateGroups();
    void SetNextAttackTime(unsigned long inTime);  // 0x3d4cd (Update) (provisional name)
    void ChooseCivilization();              // 0x3ca49 (Update, unless gGameOptions.mLoadType) (provisional name)
    void GetObjectsToGroup(std::vector<EEUnit*>& outUnits, long& outCount);
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
    void SetActive(bool inActive);          // 0x3ec42 (sets mActive, telling the unit manager) (provisional name)
    void SetCitizenPercentage(float inValue);  // 0x3ec89 (mCitizenPercentage, mIsCitizenPercentageSet) (provisional name)
    // 0x3ec9d, 0x3ecd6, 0x3ed0f: the difficulty's slot (provisional names)
    void SetDMAttackFrequencyModifier(float inValue) { ee_cp_set_by_difficulty(this, 0, inValue, 0); }
    void SetDMInitialAttackModifier(float inValue) { ee_cp_set_by_difficulty(this, 1, inValue, 0); }
    void SetIslandExpansionTimer(unsigned long inValue) { ee_cp_set_by_difficulty(this, 2, 0.0f, inValue); }
    virtual void RefreshResourceCenters();  // +0xdc 0x3ce56 (the computer player's own slot; EEUserInterface 0x276b97)
    void Update(unsigned long inTime);      // 0x3f175 (gArtificialIntelligence's round robin; provisional name)
    // inline (SetNextEpochAdvanceTime 0x70b43 loads both into registers before its fild spills) (provisional name)
    // inline (its COMDAT 0x68849 sits in EECPBuildManager.cpp's range; the civilization manager calls it out of line)
    // Provisional name
    unsigned long GetNumResourceCenters() { return this->mResourceCenters.size(); }
    EECPResourceCenter* GetResourceCenter(unsigned long inIndex);   // 0x2ef5e (ret 4: mResourceCenters[inIndex]) (provisional name)
    unsigned short GetObjectRegion(EEComplexUnit* inUnit);  // 0x2e9e7 (ret 4): a path area id for inUnit (EECPWallManager::Update) (provisional name)
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
    void WorkOutPopulation();               // 0x2d1c7 (EECPDefenseCommander 0x78148) (provisional name)
    // 0x2e851 (ret 8): deletes inScorer.mCount own units by its scores (EECPDefenseCommander) (provisional name)
    // 0x2db6d (ret 8): fills ioMap with the classes inScreener accepts (EECPUnitSelector::GatherCandidates)
    // (provisional name)
    void GetScreenedTypesByLine(std::map<long, std::vector<long>*>& ioMap, EEAIFOUnitSelectionScreener& inScreener);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    void GetScreenedTypesByLine(std::map<long, std::vector<long>*>& ioMap, const EEAIFOUnitSelectionScreener& inScreener)
        { GetScreenedTypesByLine(ioMap, const_cast<EEAIFOUnitSelectionScreener&>(inScreener)); }
#endif

    // +0xb80 (0x68849 = its size(), 0x2ef5e = operator[]: EECPCivilizationManager 0x6fb5e/0x711ac)
    std::vector<EECPResourceCenter*> mResourceCenters;
    std::vector<UWideString> mB90;          // +0xb90 (20-byte polymorphic elements: _Tidy 0x40ee8; unused here) (provisional name)
    std::vector<std::pair<unsigned long, long> > mFavoriteTypeHistory;  // +0xba0 (Load: resize(n, make_pair(0, 0)), then first/second)
    EEComplexUnit* GetUnitsToKillAt(unsigned long inIndex) const { return this->mUnitsToKill[inIndex]; }
    std::vector<EEUnitHandle<EEComplexUnit> > mUnitsToKill;  // +0xbb0 (ChooseUnitsToKill push_backs the units it kills, referenced; the dtor dereferences each; handles since R5a.4)
    EECPCombatManager* GetCombatManager() const { return this->mCombatManager; }
    EECPCombatManager* mCombatManager;      // +0xbc0 (EEAttackPlayers 0x13f5e9) (provisional name)
    // stage 6 (docs/AI_LUA_VERBS.md): the original civilization manager (+0xbc4) and its unit, build, resource,
    // research and wall managers are gone (the Lua verbs, data/ai/lib/verbs.lua); what the engine and the helper
    // queries still need: the placement searches (with the town centre tile) and the reflexes' danger zones
    EECPPlacement* mPlacement;
    std::vector<std::pair<U2DPoint<long>, unsigned long> > mDangerZones;   // (tile, game ms) EECPReflexes.cpp
    // (+0xbc8..+0xbd0: the diplomacy, exploration and player interaction managers, deleted in stage 5)
    unsigned long GetDifficulty() const { return this->mDifficulty; }
    float GetCarrierChance() const { return this->mCarrierChance; }
    float GetCarrierPopPercentage() const { return this->mCarrierPopPercentage; }
    float GetDMEarlyAttackScale() const { return this->mDMEarlyAttackScale; }
    float GetHeroChance() const { return this->mHeroChance; }
    float GetHeroPopPercentage() const { return this->mHeroPopPercentage; }
    float GetPriestPopPercentage() const { return this->mPriestPopPercentage; }
    float GetPriestChance() const { return this->mPriestChance; }
    float GetProphetPopPercentage() const { return this->mProphetPopPercentage; }
    float GetProphetChance() const { return this->mProphetChance; }
    float GetAirUnitWeight() const { return this->mAirUnitWeight; }
    long GetCarrierMaxPerGroup() const { return this->mCarrierMaxPerGroup; }
    long GetNumAirExplorers() const { return this->mNumAirExplorers; }
    long GetAttackGroupSizeMax() const { return this->mAttackGroupSizeMax; }
    long GetAttackGroupSizeMin() const { return this->mAttackGroupSizeMin; }
    long GetDefenseGroupSizeMax() const { return this->mDefenseGroupSizeMax; }
    long GetDefenseGroupSizeMin() const { return this->mDefenseGroupSizeMin; }
    long GetHeroMaxPerGroup() const { return this->mHeroMaxPerGroup; }
    long GetNumLandExplorers() const { return this->mNumLandExplorers; }
    long GetAntiAirMax() const { return this->mAntiAirMax; }
    long GetAirportMax() const { return this->mAirportMax; }
    long GetArcheryRangeMax() const { return this->mArcheryRangeMax; }
    long GetBarracksMax() const { return this->mBarracksMax; }
    long GetDockMax() const { return this->mDockMax; }
    long GetGranaryMax() const { return this->mGranaryMax; }
    long GetHospitalMax() const { return this->mHospitalMax; }
    long GetCyberFactoryMax() const { return this->mCyberFactoryMax; }
    long GetCyberLabMax() const { return this->mCyberLabMax; }
    long GetNavalYardMax() const { return this->mNavalYardMax; }
    long GetSiegeFactoryMax() const { return this->mSiegeFactoryMax; }
    long GetSpaceDockMax() const { return this->mSpaceDockMax; }
    long GetStableMax() const { return this->mStableMax; }
    long GetTankFactoryMax() const { return this->mTankFactoryMax; }
    long GetTempleMax() const { return this->mTempleMax; }
    long GetTowerMax() const { return this->mTowerMax; }
    long GetUniversityMax() const { return this->mUniversityMax; }
    long GetPriestMaxPerGroup() const { return this->mPriestMaxPerGroup; }
    long GetProphetMaxPerGroup() const { return this->mProphetMaxPerGroup; }
    long GetDropOffDistance() const { return this->mDropOffDistance; }
    long GetNumSeaExplorers() const { return this->mNumSeaExplorers; }
    long GetUnitsPerAntiAirBuilding() const { return this->mUnitsPerAntiAirBuilding; }
    long GetUnitsPerAirport() const { return this->mUnitsPerAirport; }
    long GetUnitsPerArcheryRange() const { return this->mUnitsPerArcheryRange; }
    long GetUnitsPerBarracks() const { return this->mUnitsPerBarracks; }
    long GetUnitsPerDock() const { return this->mUnitsPerDock; }
    long GetUnitsPerGranary() const { return this->mUnitsPerGranary; }
    long GetUnitsPerHospital() const { return this->mUnitsPerHospital; }
    long GetUnitsPerCyberFactory() const { return this->mUnitsPerCyberFactory; }
    long GetUnitsPerCyberLab() const { return this->mUnitsPerCyberLab; }
    long GetUnitsPerNavalYard() const { return this->mUnitsPerNavalYard; }
    long GetUnitsPerSiegeFactory() const { return this->mUnitsPerSiegeFactory; }
    long GetUnitsPerSpaceDock() const { return this->mUnitsPerSpaceDock; }
    long GetUnitsPerStable() const { return this->mUnitsPerStable; }
    long GetUnitsPerTankFactory() const { return this->mUnitsPerTankFactory; }
    long GetUnitsPerTemple() const { return this->mUnitsPerTemple; }
    long GetUnitsPerTower() const { return this->mUnitsPerTower; }
    long GetUnitsPerUniversity() const { return this->mUnitsPerUniversity; }
    unsigned long GetDMEarlyAttackTime() const { return this->mDMEarlyAttackTime; }
    bool GetCanExplore() const { return this->mCanExplore; }
    bool GetAttackStarted() const { return this->mAttackStarted; }
    // port: the engine (ee/EECPEngine.h: always kEECPEngineNext since the C++ classic's decisions were deleted, stage
    // 5; EEGameState::Create sets the AI from the lobby, EEPPlayerHelper saves it), in the padding
    unsigned char mEngine = 1;
    char _gapF57[0xf58 - 0xf57];
    struct EECPNextState* mNextState;       // new: the next engine's state (ee/EECPNext.h; NULL for the classic engine)
    struct EECPUnitsState* mUnitsState;     // new: the per-unit Lua interface's state (ee/EECPNextUnits.h; NULL until used)
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

// the epoch advance one of the player's buildings researches or has queued (kTechNone: none)
long EECPPendingEpoch(EEComputerPlayer* inPlayer);


// port: true while EEWorld::CreatePlayers repairs a loaded game's players (EEComputerPlayer::RestoreManagers)
extern bool gDeferComputerPlayerRestore;
// the managers of the computer players repaired meanwhile, in that order
void EECPRestoreDeferredManagers(unsigned long inMajorVersion, unsigned long inMinorVersion);

#endif
