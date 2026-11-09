// The next engine's switch, its per-player state and its tick (ee/EECPNext.h; docs/AI_LUA_PLAN.md §2, stage S1).
// New in this port: no original address.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPBuildManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPResearchManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPTrace.h"
#include "ee/EECPEngine.h"
#include "ee/EECPLua.h"
#include "ee/EECPNext.h"

bool EECPNextEngine(const EEComputerPlayer* inPlayer)
{
    // the seat's engine (ee/EECPEngine.h: the lobby, EE-SIM --engine, EE_AI_ENGINE)
    return inPlayer != NULL && EECPEngine(inPlayer) == kEECPEngineNext;
}

const char* EECPNextCodeName(long inCode)
{
    switch (inCode) {
    case kNextDone: return "done";
    case kNextQueued: return "queued";
    default: return "refused";
    }
}

const char* EECPNextReasonName(long inReason)
{
    switch (inReason) {
    case kNextReasonNoSite: return "no_site";
    case kNextReasonNoProducer: return "no_producer";
    case kNextReasonCannotAfford: return "cannot_afford";
    case kNextReasonNotInEpoch: return "not_in_epoch";
    case kNextReasonUnknownClass: return "unknown_class";
    case kNextReasonBadArgument: return "bad_argument";
    case kNextReasonNoUnits: return "no_units";
    case kNextReasonRequirements: return "requirements";
    case kNextReasonBusy: return "busy";
    case kNextReasonNoTarget: return "no_target";
    default: return "-";
    }
}

EECPNextState::EECPNextState()
    : mClaimRange(kEECPClaimRange), mFoodPrefer(kNextPreferPatch), mFoodLand(0), mEpoch(-1), mEpochStart(0),
      mNextRepairTime(0), mIncomeSteps(0), mNextGroup(1), mLua(NULL), mStarted(false), mFailed(false), mRestore(false), mErrors(0),
      mShadow(NULL)
{
    memset(this->mReserve, 0, sizeof(this->mReserve));
    memset(this->mIncomeTotal, 0, sizeof(this->mIncomeTotal));
    memset(this->mIncomeRing, 0, sizeof(this->mIncomeRing));
    memset(this->mSeenArmy, 0, sizeof(this->mSeenArmy));
    memset(this->mSeenStrength, 0, sizeof(this->mSeenStrength));
    memset(this->mLastSeen, 0, sizeof(this->mLastSeen));
    for (long i = 0; i < 64; i++)
        this->mSeenTownX[i] = this->mSeenTownY[i] = this->mArmyX[i] = this->mArmyY[i] = -1;
    memset(this->mSeenCitizens, 0, sizeof(this->mSeenCitizens));
    memset(&this->mTest, 0, sizeof(this->mTest));
}

EECPNextState::~EECPNextState()
{
    delete this->mLua;
    delete this->mShadow;
}

EECPNextState* EECPNextGetState(EEComputerPlayer* inPlayer)
{
    if (inPlayer->mNextState == NULL)
        inPlayer->mNextState = new EECPNextState();
    return inPlayer->mNextState;
}

void EECPNextDeleteState(EEComputerPlayer* inPlayer)
{
    delete inPlayer->mNextState;
    inPlayer->mNextState = NULL;
}

// EEComputerPlayer::CanAfford for the next engine: the plain check (EEPlayer::CanAfford), then the strategy's reserve,
// which only the epoch advance may spend (no epoch pressure, army cap, boom or recovery rule)
bool EECPNextCanAfford(EEComputerPlayer* inPlayer, EETechTreeNode* inNode, long inUnitType, long inCount)
{
    bool theEpoch = inNode != NULL && inNode->mData != NULL && EEIsEpochID(inNode->mData->mID);
    if (theEpoch && inPlayer->mTechnologies != NULL && EECPCheatOn(inPlayer, kAICheatEpochs))
        return true;
    if (!inPlayer->EEPlayer::CanAfford(inNode, inUnitType, inCount))
        return false;
    if (theEpoch || inPlayer->mNextState == NULL)
        return true;
    for (long i = 0; i < kNextResources; i++) {
        long theReserve = inPlayer->mNextState->mReserve[i];
        if (theReserve <= 0)
            continue;
        long theCost = inPlayer->ScaleCost(inNode->GetCost(kResourceFood + i, inUnitType, 1.0f)) * inCount;
        if (theCost > 0 && inPlayer->mResourceCaps[kResourceFood + i] - theCost < theReserve)
            return false;
    }
    return true;
}

void EECPNextIncome(EEComputerPlayer* inPlayer, long inResource, long inAmount)
{
    long i = inResource - kResourceFood;
    if (inPlayer->mNextState != NULL && i >= 0 && i < kNextResources && inAmount > 0)
        inPlayer->mNextState->mIncomeTotal[i] += inAmount;
}

void EECPNextEventAdd(const EEComputerPlayer* inPlayer, long inKind, long inType, long inCount)
{
    EECPNextState* theState = inPlayer->mNextState;
    if (theState == NULL)
        return;
    for (unsigned long k = 0; k < theState->mEvents.size(); k++)
        if (theState->mEvents[k].kind == inKind && theState->mEvents[k].type == inType) {
            theState->mEvents[k].count += inCount;
            return;
        }
    EECPNextEvent theEvent = { inKind, inType, inCount };
    if (theState->mEvents.size() < 256)
        theState->mEvents.push_back(theEvent);
}

long EECPNextFoodPreference(const EEComputerPlayer* inPlayer)
{
    return inPlayer->mNextState != NULL ? inPlayer->mNextState->mFoodPrefer : (long)kNextPreferPatch;
}

long EECPNextClaimRangeOf(const EEComputerPlayer* inPlayer)
{
    return inPlayer->mNextState != NULL ? inPlayer->mNextState->mClaimRange : kEECPClaimRange;
}

static const char* EECPNextEventName(long inKind)
{
    switch (inKind) {
    case kNextEventUnitLost: return "unit_lost";
    case kNextEventProductionLost: return "production_lost";
    case kNextEventAdvanceLost: return "advance_lost";
    case kNextEventNoSite: return "no_site";
    case kNextEventGroupWants: return "group_wants";
    default: return "?";
    }
}

// the next engine's tick: the executors and the reflexes the classic engine runs too, then one strategy step
void EECPNextUpdate(EEComputerPlayer* inPlayer, unsigned long inTime)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    EECPCivilizationManager* theCivilization = inPlayer->mCivilizationManager;
    if (theCivilization == NULL || inPlayer->mTechnologies == NULL)
        return;
    // the bookkeeping the classic tick does first (EEComputerPlayer::Update): new units into resource centres, the
    // combat state's decay and the population cap (WorkOutPopulation; GetPopulationLimit reads it). EnableManagers
    // does not run: its flags gate only classic decisions (the next engine's improvements stay off)
    if (inPlayer->mRegroupPending) {
        inPlayer->UpdateGroups();
        inPlayer->mRegroupPending = false;
    }
    if (inPlayer->mCombatState == 2 && inPlayer->mNumAttacks == 0 && inTime - inPlayer->mLastDamagedTime > 60000) {
        inPlayer->SetCombatState(1);
        inPlayer->mBuildingDamaged = false;
        inPlayer->mCitizenDamaged = false;
    }
    if (inTime >= (unsigned long)inPlayer->mNextPopulationTime) {
        inPlayer->WorkOutPopulation();
        inPlayer->mNextPopulationTime = inTime + 60000;
    }
    if (theState->mEpoch != inPlayer->mTechnologies->mCurrentEpoch) {
        if (theState->mEpoch >= 0)
            theState->mEpochStart = inTime;
        theState->mEpoch = inPlayer->mTechnologies->mCurrentEpoch;
    }
    // the runners (EECPCivilizationManager::Update's executors; its decisions stay classic-only)
    {
        EECPTraceScope theScope("unit");
        theCivilization->mUnitManager->Update(inTime);
    }
    {
        EECPTraceScope theScope("resource");
        theCivilization->mResourceManager->Update(inTime);
    }
    {
        EECPTraceScope theScope("build");
        theCivilization->mBuildManager->Update(inTime);
    }
    {
        EECPTraceScope theScope("research");
        theCivilization->mResearchManager->Update(inTime);
    }
    // the reflexes: repairs, and the attack groups' state machines (the militia, flee and naval flee run on the
    // damage events)
    if (inPlayer->mCanBuild && inTime >= theState->mNextRepairTime) {
        EECPTraceScope theScope("civ");
        theCivilization->RepairBuildings();
        theState->mNextRepairTime = inTime + 31000;
    }
    if (inPlayer->mCombatManager != NULL) {
        EECPTraceScope theScope("combat");
        inPlayer->mCombatManager->mAttackCommander->Update(inTime);
    }
    {
        EECPTraceScope theScope("next");
        EECPNextArmyStep(inPlayer);   // the next engine's groups (EECPNextArmy.cpp)
    }
    // one strategy step on a fresh view
    EECPTraceScope theScope("next");
    EECPNextView theView = EECPNextView();   // (value-initialized: every count starts at 0)
    EECPNextBuildView(inPlayer, theView);
    for (unsigned long k = 0; k < theView.events.size(); k++)
        EE_CP_TRACE(inPlayer, "next.event", "kind=%s cls=%ld n=%ld", EECPNextEventName(theView.events[k].kind),
            theView.events[k].type, theView.events[k].count);
    theState->mEvents.clear();
    long theSlot = theState->mIncomeSteps % 60;
    for (long i = 0; i < kNextResources; i++)
        theState->mIncomeRing[theSlot][i] = theState->mIncomeTotal[i];
    theState->mIncomeSteps++;
    if (inTime % 30000 < 1000)
        EE_CP_TRACE(inPlayer, "next.view",
            "epoch=%ld sol=%ld adv=%ld cit=%ld idle=%ld boats=%ld army=%ld/%ld threat=%ld sites=%lu claim=%ld gat=%ld/%ld/%ld/%ld/%ld "
            "want=%ld/%ld/%ld/%ld/%ld inc=%ld/%ld/%ld/%ld/%ld res=%ld/%ld/%ld/%ld/%ld",
            theView.epoch, theView.sol_missing, theView.advancing, theView.citizens, theView.idle, theView.boats, theView.army_count, theView.army_strength,
            theView.threat_strength, (unsigned long)theView.sites.size(), theView.claim_range, theView.gatherers[0],
            theView.gatherers[1], theView.gatherers[2], theView.gatherers[3], theView.gatherers[4], theView.wanted[0],
            theView.wanted[1], theView.wanted[2], theView.wanted[3], theView.wanted[4], theView.income[0],
            theView.income[1], theView.income[2], theView.income[3], theView.income[4], theView.reserve[0],
            theView.reserve[1], theView.reserve[2], theView.reserve[3], theView.reserve[4]);
    if (gCPTraceOn && inTime % 60000 < 1000) {
        // the claimed sites: index:resource:units:room:workers:distance:drop:tower:danger
        char theSites[200];
        long theLength = 0, theCount = 0;
        theSites[0] = 0;
        for (unsigned long k = 0; k < theView.sites.size() && theLength < 160; k++) {
            const EECPNextSite& theSite = theView.sites[k];
            if (!theSite.claimed || theSite.units == 0)
                continue;
            theLength += snprintf(theSites + theLength, sizeof(theSites) - theLength, " %ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld:%ld",
                theSite.index, theSite.resource, theSite.units, theSite.room, theSite.workers, theSite.distance,
                theSite.has_drop, theSite.has_tower, theSite.danger);
            theCount++;
        }
        EE_CP_TRACE(inPlayer, "next.sites", "claimed=%ld%s", theCount, theSites);
    }
    EECPNextStrategyStep(inPlayer, theView);   // the script (or TEST ONLY the C++ test opening)
    EECPTraceStats(inPlayer, inTime);
}
