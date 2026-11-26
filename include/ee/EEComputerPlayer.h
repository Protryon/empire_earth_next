// C:\EEX\Empire Earth\AI Thread\EEComputerPlayer.h (RECONSTRUCTION; file name inferred). A
// computer player (RTTI EEComputerPlayer > EEPlayer > RTSPlayer; vtable 0x43a57c; sizeof 0xf58; EEComputerPlayer.cpp
// 0x2ce2e..0x412b1, key "EEComputerPlayer-Data"; ctors 0x2cf7e(2 args) and 0x2d03d, ??_G 0x2d021, dtor 0x2d0bb). Only
// what the decompiled units need: its own slots are not declared yet (the EEPlayer ones are inherited); fields by
// offset (provisional names). Requires ee/EEPlayer.h.
#ifndef EE_EECOMPUTERPLAYER_H
#define EE_EECOMPUTERPLAYER_H

#include "ee/EEUnitHandle.h"
#include "rust/sim/cp.h"             // EeCpFields (the plain members), the bodies over them
#include "ee/EEPlayer.h"

struct EEPUnitSwap;                         // (ee/EEPhysics.h)
struct EECPPlacement;                       // ee/EECPPlacement.h
class EEAIFOUnitsToKillScoringBase;         // ee/EEAIFOUnitsToKillScoringBase.h

// port (lane B of the AI Thread): the plain members (the tuning values, timers and flags Init sets and Load and Save
// read and write) are the Rust record EeCpFields (ee_sim::cp::fields), their bodies Rust's (cp::init, cp::save,
// cp::player); the containers, the managers and the engines' states stay C++ members
class EEComputerPlayer : public EEPlayer, public EeCpFields {
public:
    static const UString sData;             // 0x4e5c70 "EEComputerPlayer-Data" (chain 0x2ce2e; guard 0x4e5c68 bit 1) (provisional name)
    static const UString sCPUOnlyFlag;      // 0x4e5c58 "EEComputerPlayer-CPU Only Flag" (chain 0x2ce71; bit 2) (provisional name)
    // 0x2d03d (EEPPlayerHelper's CreatePlayer)
    EEComputerPlayer() { this->Init(); }
    // +0x00 (??_G 0x2d021, dtor 0x2d0bb)
    virtual ~EEComputerPlayer() { ee_cp_pl_destroy(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this))); }
    // +0x18 0x2fd50 (no /Og)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_cp_pl_load(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inMajorVersion, inMinorVersion, (uint64_t)(uintptr_t)&inFile); }
    // +0x20 0x3cff8
    virtual long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion) { return ee_cp_pl_repair_pointers(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inMajorVersion, inMinorVersion); }
    // +0x24 0x3d677
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_cp_pl_save(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inMajorVersion, inMinorVersion, (uint64_t)(uintptr_t)&outFile); }
    // the computer player's overrides (ret N from the bodies) and members (provisional names)
    EEComputerPlayer(const UWideString& inName, unsigned long inPlayerID) : EEPlayer(inName, inPlayerID) { this->Init(); }
    // 0x2f1c8 (both ctors: the tuning members from the AI globals, the managers)
    void Init() { ee_cp_pl_init(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this))); }
    // +0x48 0x2d1a5
    virtual void AddResource(long inIndex, long inAmount) { ee_cp_pl_add_resource(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inIndex, inAmount); }
    virtual void DetachUnitFromAI(EEComplexUnit* inUnit);  // +0x4c 0x2db35
    // +0x50 0x2e248 (the heroes count)
    virtual long GetHeroCount(bool inByClass, bool inRequests) { return ee_cp_pl_hero_count(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inByClass, inRequests); }
    // +0x54 0x2dc51
    virtual bool CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason) { return ee_cp_pl_can_produce(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inTechnology, inUnitClass, inCount, inCheckCost, inWonderMode, (uint64_t)(uintptr_t)outReason); }
    // +0x58 0x2dca6
    virtual void NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag) { EEPlayer::NoticeTargetPlayer(inPlayer, inFlag); }
    // +0x60 0x2e1e4
    virtual void Virtual60() { EEPlayer::Virtual60(); }
    virtual void UnitCompleted(EEComplexUnit* inUnit);  // +0x68 0x2e1e9
    // +0xa8 0x2d003
    virtual void SetCombatState(unsigned long inCombatState) { ee_cp_pl_set_combat_state(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inCombatState); }
    // +0x6c 0x2eace (scaled by GetUnitCostModifier when mIsCheating, at least 0)
    virtual long ScaleCost(long inCost) { return ee_cp_scale_cost(this, inCost); }
    // +0x78 0x2ef32 (50 for 0x1e0 with a garrison or a fortress time)
    virtual long GetExtraPopulation(long) { return 1; }
    // +0x7c 0x2ecca (EEPlayer's when mIgnoreOthers, else mPopulationCap via WorkOutPopulation)
    virtual long GetPopulationLimit(long inArg) { return ee_cp_pl_population_limit(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inArg); }
    // +0x80 0x2f033 (epochs: always)
    virtual bool CanAfford(EETechnology* inTechnology, long inUnitType, long inCount) { return ee_cp_pl_can_afford(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), (uint64_t)(uintptr_t)inTechnology, inUnitType, inCount); }
    virtual void DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit);  // +0x84 0x2f16d
    // +0x1c 0x3cf73 (resets the centers, the exploration and combat managers)
    virtual void Clear() { ee_cp_pl_clear(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this))); }
    // +0x88 0x3c992
    virtual void EpochMediaLoaded(long inEpoch) { ee_cp_pl_epoch_media(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inEpoch, true); }
    virtual void FlareReceived(EEPlayer* inFrom, const U2DPoint<long>& inTile);  // same EEPlayer slot, 0x3ce2e
    virtual void UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps);  // +0x9c 0x3ced7 (the EEPlayer slot)
    // same EEPlayer slot, 0x3edc0
    virtual void PayImprovement(EEUnitClass* inClass, long inImprovement) { ee_cp_pl_pay_improvement(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), (uint64_t)(uintptr_t)inClass, inImprovement); }
    // +0x90 0x3cec9
    virtual void RefreshTownGroups() {  }
    // 0x3d4cd (Update) (provisional name)
    void SetNextAttackTime(unsigned long inTime) { ee_cp_pl_set_next_attack_time(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inTime); }
    // +0xa4 0x3d45d
    virtual void EpochReached(long inEpoch) { ee_cp_pl_epoch_reached(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inEpoch); }
    virtual void UnitStranded(EEComplexUnit* inUnit);  // +0xac 0x3ed48
    // +0xb4 0x3ed98 (epochs: free)
    virtual void Pay(EETechnology* inTechnology, long inUnitType, float inFraction) { ee_cp_pl_pay(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), (uint64_t)(uintptr_t)inTechnology, inUnitType, inFraction); }
    // +0xb8 0x3ed81 (only a positive inArg2)
    virtual void TakeResource(long inIndex, long inAmount) { ee_cp_pl_take_resource(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inIndex, inAmount); }
    virtual void UnitPlaced(EEComplexUnit* inUnit);  // +0xc0 0x3ef5d
    virtual void UnitTaken(RTSUnit* inUnit, bool inSwapped);  // +0xc4 0x3ef91 (the EEPlayer slot: a unit leaves)
    virtual void UnitGoalDone(EEComplexUnit* inUnit); // original ret4, Player slot C8
    // +0xcc 0x3f142
    virtual void EpochMediaUnloaded(long inEpoch) { ee_cp_pl_epoch_media(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inEpoch, false); }
    virtual bool AvoidsEnemies(EEComplexUnit* inUnit);  // +0xd0 0x3f73e (a citizen, or a transport holding only citizens)
    // +0xd4 0x3f7ba
    virtual void WonderStarted(EEComplexUnit*) {  }
    // 0x3ec42 (sets mActive, telling the unit manager) (provisional name)
    void SetActive(bool inActive) { ee_cp_pl_set_active(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inActive); }
    // 0x3ec89 (mCitizenPercentage, mIsCitizenPercentageSet) (provisional name)
    void SetCitizenPercentage(float inValue) { ee_cp_pl_set_citizen_percentage(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inValue); }
    // +0xdc 0x3ce56 (the computer player's own slot; EEUserInterface 0x276b97)
    virtual void RefreshResourceCenters() { ee_cp_pl_refresh_resource_centers(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this))); }
    // 0x3f175 (gArtificialIntelligence's round robin; provisional name)
    void Update(unsigned long inTime) { ee_cp_pl_update(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this)), inTime); }
    // inline float getters: the AI compares load them onto the x87 stack (fld [mAidThreatThreshold]; fld st(1); fcompp) instead of
    // comparing against a memory operand (EECPMilitaryAidPackage 0x485892..0x485a2d) (provisional names)
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
    // 0x2ec52 (gWorld.mGameOptions.mResources == 4 and a byte clear) (provisional name)
    bool IsDeathmatch() { return ee_cp_pl_is_deathmatch(((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this))); }
    // 0x2e851 (ret 8): deletes inScorer.mCount own units by its scores (EECPDefenseCommander) (provisional name)

    // +0xb80 (0x68849 = its size(), 0x2ef5e = operator[]: EECPCivilizationManager 0x6fb5e/0x711ac)
    // port (the AI Thread cluster, round 2): the resource centres (+0xb80, EECPResourceCenter whole), the favourite
    // types' history (+0xba0), the units to kill (+0xbb0) and the danger zones are Rust lists (ee_sim::cp lists.rs),
    // the record's mLists (made by Init); the unused +0xb90 vector is gone
    // (+0xbc0: the combat manager, empty since stage 6 and deleted in the AI Thread cluster's round 2)
    // stage 6 (docs/AI_LUA_VERBS.md): the original civilization manager (+0xbc4) and its unit, build, resource,
    // research and wall managers are gone (the Lua verbs, data/ai/lib/verbs.lua); what the engine and the helper
    // queries still need: the placement searches (with the town centre tile)
    EECPPlacement* mPlacement = NULL;
    // (+0xbc8..+0xbd0: the diplomacy, exploration and player interaction managers, deleted in stage 5)
    // port: the engine (ee/EECPEngine.h: always kEECPEngineNext since the C++ classic's decisions were deleted, stage
    // 5; EEGameState::Create sets the AI from the lobby, EEPPlayerHelper saves it), in the padding
    unsigned char mEngine = 1;
    char _gapF57[0xf58 - 0xf57];
    struct EECPNextState* mNextState = NULL;       // new: the next engine's state (ee/EECPNext.h; NULL for the classic engine)
    void* mUnitsState = NULL;     // new: the per-unit Lua interface's state (ee_ai units_state.rs; NULL until used)
};

// the epoch advance one of the player's buildings researches or has queued (kTechNone: none)
inline long EECPPendingEpoch(EEComputerPlayer* inPlayer) { return ee_cp_pl_pending_epoch((uint64_t)(uintptr_t)static_cast<EEPlayer*>(inPlayer)); }

// port: true while EEWorld::CreatePlayers repairs a loaded game's players (EEComputerPlayer::RestoreManagers)
extern bool gDeferComputerPlayerRestore;

#endif
