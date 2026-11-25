// C:\EEX\Empire Earth\AI Thread\EEComputerPlayer.cpp (folder inferred; the class keys "EEComputerPlayer-Data" and
// "EEComputerPlayer-CPU Only Flag"; RTTI EEComputerPlayer, vtable 0x43a57c). A computer player: 0x2ce2e..0x412b1
// (EEArtificialIntelligence.cpp ends at 0x2ce2e; EECPAGOrder.cpp starts at 0x412b1).
// Port (the AI Thread cluster, round 7): the class keeps its vtable and its members, each one call into Rust
// (ee_sim::cp player_game.rs: the events, the tick, the start, the load and save's parts, the favourite screener,
// the hero count); this file is that and ee_cp_player_host, the C++ the Rust bodies ask for (EEPlayer's own
// members, the goals, the placement's file part, the tech tree's epochs), one statement each.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <utility>
#include <algorithm>
#include <math.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPTrace.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EECPEngine.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPPlacement.h"
#include "ee/EEUGRust.h"
#include "ee/EETeam.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EECPBehavior.h"
#include "ee/EECPNextUnits.h"
#include "ee/EECPNext.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECombatGoals.h"
#include "rust/sim/cp.h"
#include "rust/data/ffi.h"   // ee_data_rules_ai_level

// The existing civilization-loader helper declared in EEAResetAllPlayerInfo.h (0x2806e7).
long EEApplyCivilizationFile(UString& ioName, EEPlayer* inPlayer);

#define EECP_THIS ((uint64_t)(uintptr_t)static_cast<EEPlayer*>(this))
#define EECP_UNIT(u) ((uint64_t)(uintptr_t)static_cast<const RTSUnit*>(u))

// chain at 0x42ce2e: the class keys (class static members; provisional names)
const UString EEComputerPlayer::sData("EEComputerPlayer-Data");
const UString EEComputerPlayer::sCPUOnlyFlag("EEComputerPlayer-CPU Only Flag");

static_assert(sizeof(EeCpBehavior) == sizeof(EECPBehavior), "ee_sim::cp::init reads gCPBehaviors as EeCpBehavior");

EEComputerPlayer::EEComputerPlayer(const UWideString& inName, unsigned long inPlayerID) : EEPlayer(inName, inPlayerID)
{
    this->Init();
}

EEComputerPlayer::EEComputerPlayer()
{
    this->Init();
}

EEComputerPlayer::~EEComputerPlayer()
{
    ee_cp_pl_destroy(EECP_THIS);
}

void EEComputerPlayer::Init()
{
    ee_cp_pl_init(EECP_THIS);
}

void EEComputerPlayer::SetCombatState(unsigned long inCombatState)
{
    ee_cp_pl_set_combat_state(EECP_THIS, inCombatState);
}

void EEComputerPlayer::AddResource(long inIndex, long inAmount)
{
    ee_cp_pl_add_resource(EECP_THIS, inIndex, inAmount);
}

void EEComputerPlayer::WorkOutPopulation()
{
    ee_cp_pl_work_out_population(EECP_THIS);
}

void EEComputerPlayer::DetachUnitFromAI(EEComplexUnit* inUnit)
{
    ee_cp_pl_detach_unit(EECP_UNIT(inUnit));
}

bool EEComputerPlayer::CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason)
{
    return ee_cp_pl_can_produce(EECP_THIS, inTechnology, inUnitClass, inCount, inCheckCost, inWonderMode, (uint64_t)(uintptr_t)outReason);
}

void EEComputerPlayer::NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag)
{
    EEPlayer::NoticeTargetPlayer(inPlayer, inFlag);
}

void EEComputerPlayer::Virtual60()
{
    EEPlayer::Virtual60();
}

void EEComputerPlayer::UnitCompleted(EEComplexUnit* inUnit)
{
    ee_cp_pl_unit_completed(EECP_THIS, EECP_UNIT(inUnit));
}

long EEComputerPlayer::GetHeroCount(bool inByClass, bool inRequests)
{
    return ee_cp_pl_hero_count(EECP_THIS, inByClass, inRequests);
}

long EEComputerPlayer::ScaleCost(long inCost)
{
    return ee_cp_scale_cost(this, inCost);
}

bool EEComputerPlayer::IsDeathmatch()
{
    return ee_cp_pl_is_deathmatch(EECP_THIS);
}

long EEComputerPlayer::GetPopulationLimit(long inArg)
{
    return ee_cp_pl_population_limit(EECP_THIS, inArg);
}

// (50 for citizens while the classic's garrison or fortress was planned: gone in stage 6)
long EEComputerPlayer::GetExtraPopulation(long)
{
    return 1;
}

long EECPPendingEpoch(EEComputerPlayer* inPlayer)
{
    return ee_cp_pl_pending_epoch((uint64_t)(uintptr_t)static_cast<EEPlayer*>(inPlayer));
}

bool EEComputerPlayer::CanAfford(EETechnology* inTechnology, long inUnitType, long inCount)
{
    return ee_cp_pl_can_afford(EECP_THIS, (uint64_t)(uintptr_t)inTechnology, inUnitType, inCount);
}

void EEComputerPlayer::DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit)
{
    ee_cp_pl_damage_taken(EECP_THIS, inPlayer, inAmount, EECP_UNIT(inUnit));
}

long EEComputerPlayer::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_cp_pl_load(EECP_THIS, inMajorVersion, inMinorVersion, (uint64_t)(uintptr_t)&inFile);
}

long EEComputerPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_cp_pl_save(EECP_THIS, inMajorVersion, inMinorVersion, (uint64_t)(uintptr_t)&outFile);
}

void EEComputerPlayer::EpochMediaLoaded(long inEpoch)
{
    ee_cp_pl_epoch_media(EECP_THIS, inEpoch, true);
}

void EEComputerPlayer::EpochMediaUnloaded(long inEpoch)
{
    ee_cp_pl_epoch_media(EECP_THIS, inEpoch, false);
}

void EEComputerPlayer::FlareReceived(EEPlayer* inFrom, const U2DPoint<long>& inTile)
{
    ee_cp_pl_flare(EECP_THIS, (uint64_t)(uintptr_t)inFrom, inTile.mX, inTile.mY);
}

void EEComputerPlayer::RefreshResourceCenters()
{
    ee_cp_pl_refresh_resource_centers(EECP_THIS);
}

// Let the combat personnel draft the units it deferred (stage 5: no personnel; the Lua AIs take their units)
void EEComputerPlayer::RefreshTownGroups()
{
}

void EEComputerPlayer::UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps)
{
    ee_cp_pl_units_swapped(EECP_THIS, reinterpret_cast<const EeCpUnitSwap*>(ioSwaps.data()), ioSwaps.size(), (uint64_t)(uintptr_t)&ioSwaps);
}

void EEComputerPlayer::Clear()
{
    ee_cp_pl_clear(EECP_THIS);
}

long EEComputerPlayer::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return ee_cp_pl_repair_pointers(EECP_THIS, inMajorVersion, inMinorVersion);
}

// port: set by EEWorld::CreatePlayers while it repairs the players of a loaded game from save version 0.260 on; the
// computer players' managers are then restored after every player's RepairPointers, not after their own (seed 3's
// 1350 s save: player 3's attack sub group lost its task's town, player 2 having been repaired first)
bool gDeferComputerPlayerRestore = false;

void EECPRestoreDeferredManagers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    ee_cp_pl_restore_deferred(inMajorVersion, inMinorVersion);
}

// Appends a goal to one of the unit's goal queues and refreshes its goals
void EEComplexUnit::QueueGoal(EEUnitGoal* inGoal, long inItem)
{
    EEUnitGoal* theGoal = inGoal;
    EEUnitVectorItem& theItem = this->mGoalQueues[inItem];
    theItem.mGoals.push_back(theGoal);
    theGoal->SetOwner(theItem.GetUnit());
    this->RefreshGoals();
    this->RefreshActiveGoal();
}

void EEComputerPlayer::EpochReached(long inEpoch)
{
    ee_cp_pl_epoch_reached(EECP_THIS, inEpoch);
}

void EEComputerPlayer::SetNextAttackTime(unsigned long inTime)
{
    ee_cp_pl_set_next_attack_time(EECP_THIS, inTime);
}

void EEComputerPlayer::SetActive(bool inActive)
{
    ee_cp_pl_set_active(EECP_THIS, inActive);
}

void EEComputerPlayer::SetCitizenPercentage(float inValue)
{
    ee_cp_pl_set_citizen_percentage(EECP_THIS, inValue);
}

void EEComputerPlayer::UnitStranded(EEComplexUnit* inUnit)
{
    ee_cp_pl_unit_stranded(EECP_THIS, EECP_UNIT(inUnit));
}

void EEComputerPlayer::TakeResource(long inIndex, long inAmount)
{
    ee_cp_pl_take_resource(EECP_THIS, inIndex, inAmount);
}

void EEComputerPlayer::Pay(EETechnology* inTechnology, long inUnitType, float inFraction)
{
    ee_cp_pl_pay(EECP_THIS, (uint64_t)(uintptr_t)inTechnology, inUnitType, inFraction);
}

void EEComputerPlayer::PayImprovement(EEUnitClass* inClass, long inImprovement)
{
    ee_cp_pl_pay_improvement(EECP_THIS, (uint64_t)(uintptr_t)inClass, inImprovement);
}

void EEComputerPlayer::UnitPlaced(EEComplexUnit* inUnit)
{
    ee_cp_pl_unit_placed(EECP_THIS, EECP_UNIT(inUnit));
}

void EEComputerPlayer::UnitTaken(RTSUnit* inUnit, bool inSwapped)
{
    ee_cp_pl_unit_taken(EECP_THIS, EECP_UNIT(inUnit), inSwapped);
}

void EEComputerPlayer::UnitGoalDone(EEComplexUnit* inUnit)
{
    ee_cp_pl_unit_goal_done(EECP_THIS, EECP_UNIT(inUnit));
}

void EEComputerPlayer::Update(unsigned long inTime)
{
    ee_cp_pl_update(EECP_THIS, inTime);
}

void EEComputerPlayer::UpdateGroups()
{
    ee_cp_pl_regroup(EECP_THIS, 1);
}

bool EEComputerPlayer::AvoidsEnemies(EEComplexUnit* inUnit)
{
    return ee_cp_pl_avoids_enemies(EECP_THIS, EECP_UNIT(inUnit));
}

// port: the seat's Lua AI answers from ai.wonders (classic_lua's update_wonders; the original's combat manager
// responded here)
void EEComputerPlayer::WonderStarted(EEComplexUnit*)
{
}

// Returns the tech node's unit class at the index, or its default unit class past the end (ee_sim::cp player_game.rs)
long EETechTreeNode::GetUnitClass(unsigned long inIndex)
{
    return ee_cp_node_unit_class((uint64_t)(uintptr_t)this, inIndex);
}

// Returns the number of teams in the world
unsigned long EEWorld::GetNumTeams()
{
    return this->mTeams.size();
}

// Returns whether the given player is a member of the team
bool EETeam::ContainsPlayer(unsigned long inPlayerID)
{
    return std::find(this->mPlayers.begin(), this->mPlayers.end(), inPlayerID) != this->mPlayers.end();
}

// port (ee/EECPExperimentTimers.h): the experiments' timers and the resource state, saved from 0.258 and 0.265
// (ee_sim::cp timers.rs, player_game.rs ee_cp_pl_state_block)
long EECPSaveExperimentTimers(FSFile& outFile)
{
    return ee_cp_pl_state_block((uint64_t)(uintptr_t)&outFile, 0, &EESaveFilePrimitive);
}

long EECPLoadExperimentTimers(FSFile& inFile)
{
    return ee_cp_pl_state_block((uint64_t)(uintptr_t)&inFile, 1, &EESaveFilePrimitive);
}

long EECPSaveResourceState(FSFile& outFile)
{
    return ee_cp_pl_state_block((uint64_t)(uintptr_t)&outFile, 2, &EESaveFilePrimitive);
}

long EECPLoadResourceState(FSFile& inFile)
{
    return ee_cp_pl_state_block((uint64_t)(uintptr_t)&inFile, 3, &EESaveFilePrimitive);
}

// the after-load fixups of player_game.rs: inCall(inBox) at the loaded game's first tick
extern "C" void ee_cp_after_load(void (*inCall)(uint64_t), uint64_t inBox)
{
    EEAfterLoad([inCall, inBox]() { inCall(inBox); });
}

extern "C" uint64_t ee_cp_computer_at(uint32_t inID)
{
    return inID < gWorld.mNumPlayers && gWorld.mPlayers[inID] != NULL ? (uint64_t)(uintptr_t)static_cast<EEPlayer*>(gWorld.mPlayers[inID]->mComputerPlayer) : 0;
}

// the scratch name of ChooseCivilization (EE_CP_PLAYER_CIV_*)
static UString sCivName;

// The computer player's world (ee_sim::cp events.rs and player_game.rs over EE_CP_PLAYER_*; `inA`, `inB` units, players
// or C++ objects): one statement each
extern "C" int64_t ee_cp_player_host(const EeCpPlaceCtx* inContext, int32_t inOp, uint64_t inA, uint64_t inB,
    EeCpPlaceIo* io)
{
    EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(reinterpret_cast<EEPlayer*>((uintptr_t)inContext->player));
    EEUnit* theUnit = reinterpret_cast<EEUnit*>((uintptr_t)inA);
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);
    EEUGRust* theGoal = reinterpret_cast<EEUGRust*>((uintptr_t)inB);
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_PLAYER_BEHAVIOR_FLOAT: io->f[0] = gCPBehaviors[(long)v[0]].mFloatValue;
        return 0;
    case EE_CP_PLAYER_BASE_POP_LIMIT:
        return thePlayer->EEPlayer::GetPopulationLimit(0);
    case EE_CP_PLAYER_NUM_TEAMS:
        return (int64_t)gWorld.GetNumTeams();
    case EE_CP_PLAYER_TEAM_WONDERS:
        return gWorld.mTeams[(size_t)v[0]].mVictoryCondition->HasEnoughWonders() ? 1 : 0;
    case EE_CP_PLAYER_TEAM_CONTAINS:
        return gWorld.mTeams[(size_t)v[0]].ContainsPlayer((unsigned long)v[1]) ? 1 : 0;
    case EE_CP_PLAYER_NUM_PLAYERS:
        return (int64_t)gWorld.mNumPlayers;
    case EE_CP_PLAYER_HUMAN:
        return gWorld.mPlayers[(size_t)v[0]] == NULL ? -1 : gWorld.mPlayers[(size_t)v[0]]->IsHuman() ? 1 : 0;
    case EE_CP_PLAYER_POPULATION:
        return gWorld.mPlayers[(size_t)v[0]]->GetTotalPopulation(true, true);
    case EE_CP_PLAYER_HANDLE:
        return (int64_t)(uintptr_t)gWorld.GetPlayer((unsigned long)v[0]);
    case EE_CP_PLAYER_UNIT_CONTINENT_AT:
        return (int64_t)(uintptr_t)gContinentManager.GetContinentAt(theUnit);
    case EE_CP_PLAYER_IS_EPOCH:
        return EEIsEpochID((long)v[0]) ? 1 : 0;
    case EE_CP_PLAYER_UNIT_IS_BUILDING:
        return theComplex->mUnitClass->IsBuilding() ? 1 : 0;
    case EE_CP_PLAYER_UNIT_OBSOLETE:
        return theComplex->GetTechTreeNode() != NULL && theComplex->GetTechTreeNode()->GetIsObsolete() ? 1 : 0;
    case EE_CP_PLAYER_SEEN_BY:
        return EECombatIsSeenBy(theComplex, reinterpret_cast<EEPlayer*>((uintptr_t)inB), true) ? 1 : 0;
    case EE_CP_PLAYER_KILL: theComplex->Kill(true, false, false);
        return 0;
    case EE_CP_PLAYER_IS_TRANSPORT:
        return theUnit->mUnitClass->IsTransport() ? 1 : 0;
    case EE_CP_PLAYER_BASE_ADD_RESOURCE: thePlayer->EEPlayer::AddResource((long)v[0], (long)v[1]);
        return 0;
    case EE_CP_PLAYER_NEXT_STATE:
        return (int64_t)(uintptr_t)thePlayer->mNextState;
    case EE_CP_PLAYER_NEXT_STATE_GET:
        return (int64_t)(uintptr_t)EECPNextGetState(thePlayer);
    case EE_CP_PLAYER_UNITS_STATE:
        return (int64_t)(uintptr_t)EECPUnitsRust(thePlayer);
    case EE_CP_PLAYER_BASE_CAN_PRODUCE:
        return thePlayer->EEPlayer::CanProduce((long)v[0], (long)v[1], (long)v[2], v[3] != 0, (long)v[4], reinterpret_cast<UWideString*>((uintptr_t)inA)) ? 1 : 0;
    case EE_CP_PLAYER_BASE_UNIT_COMPLETED: thePlayer->EEPlayer::UnitCompleted(theComplex);
        return 0;
    case EE_CP_PLAYER_BASE_UNIT_PLACED: thePlayer->EEPlayer::UnitPlaced(theComplex);
        return 0;
    case EE_CP_PLAYER_BASE_UNIT_TAKEN: thePlayer->EEPlayer::UnitTaken(static_cast<RTSUnit*>(theUnit), v[0] != 0);
        return 0;
    case EE_CP_PLAYER_BASE_DAMAGE_TAKEN: thePlayer->EEPlayer::DamageTaken((long)v[0], (long)v[1], theUnit);
        return 0;
    case EE_CP_PLAYER_TRACE_ON:
        return gCPTraceOn ? 1 : 0;
    case EE_CP_PLAYER_IS_ALLY:
        return thePlayer->IsAlly(reinterpret_cast<EEPlayer*>((uintptr_t)inA)) ? 1 : 0;
    case EE_CP_PLAYER_EPOCH_MEDIA: v[1] != 0 ? static_cast<EETechTreeEpoch*>(thePlayer->mTechnologies->Get((long)v[0]))->LoadMediaOfKind(7) : static_cast<EETechTreeEpoch*>(thePlayer->mTechnologies->Get((long)v[0]))->UnloadMediaOfKind(7);
        return 0;
    case EE_CP_PLAYER_BASE_EPOCH_MEDIA_LOADED: thePlayer->EEPlayer::EpochMediaLoaded((long)v[0]);
        return 0;
    case EE_CP_PLAYER_BASE_EPOCH_MEDIA_UNLOADED: thePlayer->EEPlayer::EpochMediaUnloaded((long)v[0]);
        return 0;
    case EE_CP_PLAYER_EPOCH_RESEARCH: if (EETechTreeEpoch* theEpoch = dynamic_cast<EETechTreeEpoch*>(thePlayer->mTechnologies->Get((long)v[0]))) theEpoch->ResearchKind(7, true);
        return 0;
    case EE_CP_PLAYER_CIV_NAME:
        return (int64_t)(uintptr_t)(const char*)thePlayer->mCivilizationFile;
    case EE_CP_PLAYER_CIV_SET_NAME: sCivName = UString((const char*)(uintptr_t)inA);
        return 0;
    case EE_CP_PLAYER_CIV_APPLY:
        return EEApplyCivilizationFile(sCivName, thePlayer);
    case EE_CP_PLAYER_CIV_STORE: thePlayer->mCivilizationFile = sCivName;
        return 0;
    case EE_CP_PLAYER_SPACE_MAP:
        return gWorld.mMap->mTerrainMesh.GetIsSpaceMap() ? 1 : 0;
    case EE_CP_PLAYER_BASE_UNITS_SWAPPED: thePlayer->EEPlayer::UnitsSwapped(*reinterpret_cast<std::vector<EEPUnitSwap>*>((uintptr_t)inA));
        return 0;
    case EE_CP_PLAYER_BASE_CLEAR: thePlayer->EEPlayer::Clear();
        return 0;
    case EE_CP_PLAYER_BASE_REPAIR:
        return thePlayer->EEPlayer::RepairPointers((unsigned long)v[0], (unsigned long)v[1]);
    case EE_CP_PLAYER_DEFER_RESTORE:
        return gDeferComputerPlayerRestore ? 1 : 0;
    case EE_CP_PLAYER_CAN_MOVE_ON:
        return theUnit->GetUnitClass()->CanMoveOnTerrain(theComplex, (long)v[0]) ? 1 : 0;
    case EE_CP_PLAYER_ADD_STRANDED: thePlayer->AddStrandedUnit(theUnit);
        return 0;
    case EE_CP_PLAYER_QUEUE_LOCATION: theComplex->QueueGoal(EEUGRust::NewLocation(io->f[0], io->f[1], io->f[2], false, true, false, 0.0f, false, false), 0);
        return 0;
    case EE_CP_PLAYER_DETACH: thePlayer->DetachUnitFromAI(theComplex);
        return 0;
    case EE_CP_PLAYER_RESET_SOL: thePlayer->mTechnologies->mEpochs[thePlayer->mTechnologies->mCurrentEpoch].mCurrentSOL = 0;
        return 0;
    case EE_CP_PLAYER_BASE_EPOCH_REACHED: thePlayer->EEPlayer::EpochReached((long)v[0]);
        return 0;
    case EE_CP_PLAYER_EPOCH_SPAN:
        return gWorld.mGameOptions.mEpochEnd - gWorld.mGameOptions.mEpochStart + 1;
    case EE_CP_PLAYER_RANDOM_RANGE:
        return gPhysics->GetRandom((long)v[0], (long)v[1]);
    case EE_CP_PLAYER_BASE_LOAD:
        return thePlayer->EEPlayer::Load((unsigned long)v[0], (unsigned long)v[1], *reinterpret_cast<FSFile*>((uintptr_t)inA));
    case EE_CP_PLAYER_BASE_SAVE:
        return thePlayer->EEPlayer::Save((unsigned long)v[0], (unsigned long)v[1], *reinterpret_cast<FSFile*>((uintptr_t)inA));
    case EE_CP_PLAYER_FILE_OPEN:
        return reinterpret_cast<FSFile*>((uintptr_t)inA)->GetIsOpen() ? 1 : 0;
    case EE_CP_PLAYER_SAVE_PRIMITIVE:
        return (int64_t)(uintptr_t)&EESaveFilePrimitive;
    case EE_CP_PLAYER_EPOCH_COUNT:
        return EEEpochCount();
    case EE_CP_PLAYER_BASE_TAKE_RESOURCE: thePlayer->EEPlayer::TakeResource((long)v[0], (long)v[1]);
        return 0;
    case EE_CP_PLAYER_BASE_PAY: thePlayer->EEPlayer::Pay(reinterpret_cast<EETechnology*>((uintptr_t)inA), (long)v[0], io->f[0]);
        return 0;
    case EE_CP_PLAYER_BASE_PAY_IMPROVEMENT: thePlayer->EEPlayer::PayImprovement(reinterpret_cast<EEUnitClass*>((uintptr_t)inA), (long)v[0]);
        return 0;
    case EE_CP_PLAYER_TECH_ID:
        return reinterpret_cast<EETechTreeNode*>((uintptr_t)inA)->mData != NULL ? reinterpret_cast<EETechTreeNode*>((uintptr_t)inA)->mData->mID : -1;
    case EE_CP_PLAYER_TECH_UNIT_CLASS:
        return (long)reinterpret_cast<EETechTreeNode*>((uintptr_t)inA)->mData->mUnitClass;
    case EE_CP_PLAYER_CLASS_IS_BUILDING:
        return reinterpret_cast<EEUnitClass*>((uintptr_t)inA)->IsBuilding() ? 1 : 0;
    case EE_CP_PLAYER_CLASS_IS_TRANSPORT:
        return reinterpret_cast<EEUnitClass*>((uintptr_t)inA)->IsTransport() ? 1 : 0;
    case EE_CP_PLAYER_CLASS_IS_DROP_SITE:
        return reinterpret_cast<EEUnitClass*>((uintptr_t)inA)->IsDropSite() ? 1 : 0;
    case EE_CP_PLAYER_CLASS_IS_PRIEST:
        return reinterpret_cast<EEUnitClass*>((uintptr_t)inA)->IsPriest() ? 1 : 0;
    case EE_CP_PLAYER_CLASS_FLIES_OVER:
        return reinterpret_cast<EEUnitClass*>((uintptr_t)inA)->FliesOver() ? 1 : 0;
    case EE_CP_PLAYER_BASE_POP_LIMIT_OF:
        return thePlayer->EEPlayer::GetPopulationLimit((long)v[0]);
    case EE_CP_PLAYER_BASE_CAN_AFFORD:
        return thePlayer->EEPlayer::CanAfford(reinterpret_cast<EETechTreeNode*>((uintptr_t)inA), (long)v[0], (long)v[1]) ? 1 : 0;
    case EE_CP_PLAYER_GAME_RESOURCES:
        return gWorld.mGameOptions.mResources;
    case EE_CP_PLAYER_SCENARIO_GAME:
        return gWorld.mGameOptions.mIsScenarioGame ? 1 : 0;
    case EE_CP_PLAYER_LOAD_TYPE:
        return gGameOptions.mLoadType;
    case EE_CP_PLAYER_EXPLORE_UNSEEN: gWorld.mMap->ExploreUnseen(thePlayer);
        return 0;
    case EE_CP_PLAYER_ENGINE_NAME: EECPNextAppend((void*)(uintptr_t)inA, EECPEngineName(thePlayer));
        return 0;
    case EE_CP_PLAYER_ENGINE_SEES_ALL:
        return EECPEngineSeesAll(thePlayer) ? 1 : 0;
    case EE_CP_PLAYER_ENGINE_FILLS_GARRISON:
        return EECPEngineFillsGarrison(thePlayer) ? 1 : 0;
    case EE_CP_PLAYER_SET_SELF: thePlayer->mComputerPlayer = thePlayer;
        return 0;
    case EE_CP_PLAYER_BEHAVIORS:
        return (int64_t)(uintptr_t)gCPBehaviors;
    case EE_CP_PLAYER_AI_LEVEL:
        return ee_data_rules_ai_level(EEGetRules().RustRules(), gWorld.mLobbyOptions.mDifficulty);   // ee_data ffi.rs
    case EE_CP_PLAYER_NEXT_DELETE: EECPNextDeleteState(thePlayer);
        return 0;
    case EE_CP_PLAYER_UNITS_DELETE: EECPUnitsDelete(thePlayer);
        return 0;
    case EE_CP_PLAYER_GOAL_COUNT:
        return (int64_t)theComplex->mGoalQueues[0].mGoals.size();
    case EE_CP_PLAYER_GOAL_STATE_KIND:
        return theComplex->GetGoalState() != NULL ? (int64_t)theComplex->GetGoalState()->GetKind() : -1;
    case EE_CP_PLAYER_REPAIRABLE:
        return theComplex->IsRepairable() ? 1 : 0;
    case EE_CP_PLAYER_CLEAR_GOALS: theComplex->ClearGoalQueue(0);
        return 0;
    case EE_CP_PLAYER_START_ACTION: theComplex->StartAction((long)v[0]);
        return 0;
    case EE_CP_PLAYER_ACTION_CHANGED: theComplex->mUnitClass->ActionChanged(theComplex);
        return 0;
    case EE_CP_PLAYER_GROUP_REMOVE: theComplex->GetActiveGroup()->RemoveUnit(theComplex);
        return 0;
    case EE_CP_PLAYER_LOAD_UNITS:
        return (int64_t)theGoal->GetNumLoadUnits();
    case EE_CP_PLAYER_HAS_LOAD_UNIT:
        return theGoal->HasLoadUnit(theComplex) ? 1 : 0;
    case EE_CP_PLAYER_GOAL_CONDITION:
        return theGoal->EvaluateCondition(theComplex, (unsigned long)v[0]) ? 1 : 0;
    case EE_CP_PLAYER_LOAD_TILE_X:
        return theGoal->GetLoadContainerTile().GetX();
    case EE_CP_PLAYER_LOAD_TILE_Y:
        return theGoal->GetLoadContainerTile().GetY();
    case EE_CP_PLAYER_UNLOAD_AT: theComplex->UnloadCargoAtTile((long)v[0], (long)v[1], true);
        return 0;
    case EE_CP_PLAYER_NEW_UNLOAD:
        return (int64_t)(uintptr_t)EEUGRust::NewUnloadTransport(theComplex, (long)v[0], (long)v[1], true);
    case EE_CP_PLAYER_CAN_UNLOAD:
        return theGoal->CanUnloadAtDestination() ? 1 : 0;
    case EE_CP_PLAYER_QUEUE_GOAL: theComplex->QueueGoal(theGoal, 0);
        return 0;
    case EE_CP_PLAYER_SET_EMPTY:
        return gUnitSetManager.GetSet((long)v[0])->IsEmpty() ? 1 : 0;
    case EE_CP_PLAYER_SET_HAS:
        return gUnitSetManager.GetSet((long)v[0])->Contains((long)v[1]) ? 1 : 0;
    case EE_CP_PLAYER_NODE_OBSOLETE:
        return thePlayer->GetTechTree()->Get((long)v[0])->GetIsObsolete() ? 1 : 0;
    case EE_CP_PLAYER_TOTAL_POPULATION:
        return thePlayer->GetTotalPopulation(v[0] != 0, v[1] != 0);
    case EE_CP_PLAYER_GAME_TIME:
        return (int64_t)gPhysics->mGameTime;
    }
    return 0;
}
