// The next computer player's per-unit Lua interface (ee/EECPNextUnits.h; docs/AI_LUA_UNITS.md, stage 1). New in this
// port (no original address).
//
// Rules every binding keeps (AI_LUA_UNITS §1):
// - integers only: positions in tiles (x, y) and centi-tiles (px, py), speeds and ranges in centi-tiles;
// - lists sorted by id, or by (distance, id) with `near`;
// - fog-honest: other players' units only while their tile IsSeenBy the player (the reveal cheat lifts it there);
//   resources only on explored tiles (IsVisibleTo);
// - no Lua error while a C++ object with a destructor lives: arguments are read into the player's scratch vectors
//   (members, not locals), bad arguments are refused with a result instead of raised, and only the budget charge
//   (ee_ai lua_host.rs charge) raises, before anything is built;
// - commands run the human's own messages (EEMessage.cpp's Execute) on the computer player's selection, at once
//   (AI_LUA_UNITS §3.2): the selection is filled with the units, the player answers to a borrowed slot id while the
//   message runs, and both are put back.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <functional>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <utility>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/NEMessage.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEMap.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEInfluenceTable.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUGRust.h"
#include "ee/EEPathManager.h"
#include "rust/sim/astar.h"   // the path goals' modifiers (modifier_obj.rs)
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIHill.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EECPEngine.h"
#include "ee/EEMessage.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EETeam.h"
#include "ee/EECPTrace.h"
#include "ee/EEEvent.h"
#include "ee/EECPBehavior.h"
#include "ee/EECPNext.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPNextUnits.h"
#include "ee/EECPASGFlags.h"
#include "platform/PlatSim.h"
#include "rust/sim/ai.h"   // ee_ai_units_*: the save block's format (units_save.rs), the bindings' Lua helpers (units_lua.rs)
#include "rust/sim/units.h"
#include "ee/EEUnitRegions.h"

// ---------------------------------------------------------------------------------------------------------------
// The state

namespace {

const unsigned char kBorrowedSlot = 0xfd;   // the slot id a computer player answers to while its order runs

}  // namespace

// The state is Rust's (ee_ai units_state.rs: the held set, the events, the forwarding, the enemy memory, the reflexes,
// the limits, the danger layers, the bindings' scratch; units_bind.rs, units_more.rs: its hooks and the bindings).
// ai.wall_route's path requests are the state's too (the paths lane's round 2).
struct EECPUnitsState {
    void* mRust = ee_ai_units_state_new();
    ~EECPUnitsState() { ee_ai_units_state_free(mRust); }
};

static uint64_t EECPUnitsHandle(const RTSUnit* inUnit)
{
    return (uint64_t)(uintptr_t)inUnit;
}

static EECPUnitsState* EECPUnitsGet(EEComputerPlayer* inPlayer)
{
    if (inPlayer->mUnitsState == NULL) {
        // the host records as this build's header has them (a stale rust/sim/ai.h is refused)
        if (ee_ai_units_records_size(NULL, NULL, NULL, NULL, NULL, NULL) != sizeof(EeAiUnitsImproveCost) + sizeof(EeAiUnitsPlace)
                + sizeof(EeAiUnitsTuning) + sizeof(EeAiUnitsArea) + sizeof(EeAiUnitsAreaPick) + sizeof(EeAiUnitsTiles))
            abort();
        inPlayer->mUnitsState = new EECPUnitsState();
    }
    return inPlayer->mUnitsState;
}

void EECPUnitsDelete(EEComputerPlayer* inPlayer)
{
    delete inPlayer->mUnitsState;
    inPlayer->mUnitsState = NULL;
}

// ---------------------------------------------------------------------------------------------------------------
// The held set and the reflexes

bool EECPUnitsHoldsID(const EEComputerPlayer* inPlayer, long inID)
{
    return inPlayer != NULL && inPlayer->mUnitsState != NULL && ee_ai_units_holds(inPlayer->mUnitsState->mRust, inID);
}

bool EECPUnitsHolds(const EEComputerPlayer* inPlayer, const EEComplexUnit* inUnit)
{
    return inUnit != NULL && EECPUnitsHoldsID(inPlayer, (long)inUnit->mUnitID);
}

void EECPUnitsHeld(const EEComputerPlayer* inPlayer, std::vector<long>& outIDs)
{
    outIDs.clear();
    if (inPlayer == NULL || inPlayer->mUnitsState == NULL)
        return;
    void* s = inPlayer->mUnitsState->mRust;
    outIDs.resize(ee_ai_units_held(s, NULL, 0));
    ee_ai_units_held(s, outIDs.data(), outIDs.size());
}

bool EECPUnitsReflexOn(const EEComputerPlayer* inPlayer, const char* inName)
{
    if (inPlayer == NULL || inPlayer->mUnitsState == NULL)
        return strcmp(inName, "held") != 0;
    return ee_ai_units_reflex_on(inPlayer->mUnitsState->mRust, inName);
}

// ---------------------------------------------------------------------------------------------------------------
// Events

void EECPUnitsOnDealt(EEComputerPlayer* inPlayer, long inAmount, EEUnit* inVictim)
{
    if (inPlayer->mUnitsState != NULL && inVictim != NULL)
        ee_ai_units_on_dealt(inPlayer->mUnitsState->mRust, inPlayer, inAmount, EECPUnitsHandle(inVictim));
}

void EECPUnitsOnAttackPlayer(EEComputerPlayer* inPlayer, long inPlayerID, bool inDefend)
{
    if (inPlayer->mUnitsState != NULL)
        ee_ai_units_on_player(inPlayer->mUnitsState->mRust, inDefend ? 9 : 8, inPlayerID, 0, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// The bindings (ee_ai units_bind.rs, units_more.rs)

bool EECPUnitsUsed(const EEComputerPlayer* inPlayer)
{
    return inPlayer != NULL && inPlayer->mUnitsState != NULL && ee_ai_units_used(inPlayer->mUnitsState->mRust);
}

// ---------------------------------------------------------------------------------------------------------------
// The host's helpers for the Rust bindings (ee_ai units_more.rs)

// the classic research manager's citizen test (EECPResearchManager::ChooseResearch): a tech whose researched event
// improves a citizen's gathering (effect kind 2 on the citizen classes, attributes 0x1a-0x1d, 0x1f-0x21)
static bool EECPUnitsCitizenUpgrade(EETechTreeNode* inNode)
{
    EEEvent* theEvent = EEEvent::Get(inNode->mData->mResearchedEvent);
    if (theEvent == NULL)
        return false;
    for (unsigned long e = 0; e < theEvent->GetNumEffects(); e++) {
        EEEventEffect* theEffect = EEEvent::GetEventEffect(theEvent->GetEffect(e));
        if (theEffect != NULL && theEffect->mKind == 2 && (theEffect->mUnitClass == 0x20 || theEffect->mUnitClass == 0x1e0)
            && (theEffect->mAttribute == 0x1a || theEffect->mAttribute == 0x1b || theEffect->mAttribute == 0x1c
                || theEffect->mAttribute == 0x1d || theEffect->mAttribute == 0x20 || theEffect->mAttribute == 0x21
                || theEffect->mAttribute == 0x1f))
            return true;
    }
    return false;
}

bool EEDoesWallGateFitOnUnit(EEComplexUnit* inUnit, long inSide, long inUnitType, std::vector<EEUnit*>& outUnits);   // EECPWallManager.cpp

// the next level's cost of a class's improvement, as the improve verb pays it (ai.improve_cost): false for no node
// now or no such improvement
static bool EECPUnitsImproveCost(EEComputerPlayer* inPlayer, long inType, long inImprovement, EeAiUnitsImproveCost& outCost)
{
    if (inType < 0 || (unsigned long)inType >= EEUnitClassCount() || gUnitClassTable[inType] == NULL
        || inPlayer->mTechnologies == NULL)
        return false;
    EETechTreeNode* theNode = inPlayer->mTechnologies->Get(gUnitClassTable[inType]->mTechnology);
    if (theNode == NULL || theNode->mData == NULL || !theNode->GetIsAvailable() || theNode->GetIsObsolete())
        return false;
    EEUnitClass* theClass = gUnitClassTable[inType];
    if (inImprovement < 0 || inImprovement >= 16 || theClass->mImprovements == NULL)
        return false;
    EEUnitClassUpgrade* theUpgrade = inPlayer->mUnitClassUpgrades[inType];
    outCost.level = inPlayer->GetImprovementLevel(inType, inImprovement);
    outCost.max = theClass->mImprovements->GetMaxLevel();
    float theFactor = theClass->mImprovements->mEntries[inImprovement].mCostFactor * (float)outCost.level;
    for (long i = 0; i < 5; i++)
        outCost.cost[i] = inPlayer->ScaleCost(UMFloatToLong((float)theNode->mData->mCosts[kResourceFood + i] * theUpgrade->mCostScale * theFactor));
    return true;
}

// the classic build manager's HasEnemyNear through the fog (the manifest's "sees_all"): every armed unit not the
// player's in the influence regions round the tile (one tile on small island and planet maps), idle searchers left
// out -> their count
static long EECPUnitsEnemyNearAll(EEComputerPlayer* me, long inX, long inY)
{
    std::vector<EEComplexUnit*> theUnits;
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    EEMap* theMap = gWorld.mMap;
    U2DRectangle<long> theRect;
    if (theTable == NULL)
        return 0;
    if (((theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap
            || (theMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap)
        && theMap->mL195610 <= 0x2a) {
        theRect.mLeft = inX - 1;
        theRect.mRight = inX + 1;
        theRect.mTop = inY - 1;
        theRect.mBottom = inY + 1;
    } else {
        long theSize = theTable->mRegionSize;
        theRect.mLeft = inX - theSize;
        theRect.mRight = inX + theSize;
        theRect.mTop = inY - theSize;
        theRect.mBottom = theSize + inY;
    }
    long theCount = theTable->SumEnemyValue(me->mPlayerID, theTable->GetRegions(theRect), true);
    if (theCount <= 0)
        return theCount;
    EECollectRegionUnits(theMap->mUnitRegions, theRect, theUnits);
    theCount = 0;
    for (size_t i = 0; i < theUnits.size(); i++) {
        EEComplexUnit* u = theUnits[i];
        if (u->mPlayer != me && u->mStats->mAttack != 0.0f && !(u->mTargetSearchTimer > 60000))
            theCount++;
    }
    return theCount;
}

// ---------------------------------------------------------------------------------------------------------------
// The registration and the micro step are Rust (ee_ai units_bind.rs ee_ai_units_open, next_game.rs
// ee_ai_next_micro_tick)

void* EECPUnitsRust(const EEComputerPlayer* inPlayer)
{
    return inPlayer != NULL && inPlayer->mUnitsState != NULL ? inPlayer->mUnitsState->mRust : NULL;
}

void EECPUnitsMicroTick(unsigned long inTime)
{
    ee_ai_next_micro_tick(inTime);
}

// ---------------------------------------------------------------------------------------------------------------
// The save: "EENU" (version 4: the militia and held reflexes; 3: the lost layer's trades; 2: the hurt spots and the
// marks). The state, its format and the save (with its TEST ONLY check) are Rust's (ee_ai units_state.rs,
// units_save.rs, units_bind.rs ee_ai_units_save).

void EECPUnitsSave(const EEComputerPlayer* inPlayer, std::string& outBytes)
{
    outBytes.clear();
    ee_ai_units_save(const_cast<EEComputerPlayer*>(inPlayer), EECPUnitsRust(inPlayer), &outBytes);
}

bool EECPUnitsLoad(EEComputerPlayer* inPlayer, const std::string& inBytes)
{
    return ee_ai_units_load(inPlayer, (const uint8_t*)inBytes.data(), inBytes.size());
}

// ---------------------------------------------------------------------------------------------------------------
// The host of the Rust bindings (ee_ai units_bind.rs EE_AI_UNITS_HOST_*): what the world views do not hold

// a command's message (ee_ai units_bind.rs EE_AI_UNITS_MSG_*), as the human's: on the selection under the borrowed
// slot (mode 0, EECPUnitsRun's), or run as it is (mode 1)
static const unsigned char kUnitsMessages[] = {0, kMessagePathTo, kMessageAttackUnit, kMessageAttackGround,
    kMessageGatherResource, kMessageBuildUnit, kMessageRepairUnit, kMessageLoadGarrison, kMessageLoadTransport,
    kMessageUnloadTransport, kMessageResearchTechnology, kMessageSetUnitBehavior, kMessageStopActiveGroup, kMessageExplore,
    kMessageKillUnit, kMessageHealUnit, kMessageSetRallyGoal, kMessageRemoveTechnology, kMessageChangeFormation,
    kMessagePackUnpack, kMessageDropoffResource, kMessageReplantFarms, kMessageSmartFarmers, kMessageCastCalamity,
    kMessageAddPatrolPoint, kMessageLockUnlock, kMessageBuildWall, kMessageImproveUnit, kMessageHackANewUnit, kMessageBuildGate,
    kMessageExchangeResources};

static EEMRecordValue EECPUnitsValue(const EeAiUnitsMsgValue& inValue)
{
    if (inValue.kind == 2)
        return EEMRecordValue(GE2DPoint(inValue.x, inValue.y));
    if (inValue.kind == 1)
        return EEMRecordValue((double)inValue.x);
    return EEMRecordValue((long long)inValue.i);
}

static void EECPUnitsExecute(EEComputerPlayer* me, EEMessage& inMessage, long inMode)
{
    if (inMode == 1) {
        inMessage.Execute();
        return;
    }
    unsigned char theSlot = me->mSlot;
    me->mSlot = kBorrowedSlot;
    if (inMode == 2 || gWorld.GetPlayerBySlot(kBorrowedSlot) == me) {   // 2: the borrowed slot, unchecked
        inMessage.SetOriginatingSlot(kBorrowedSlot);
        inMessage.Execute();
    }
    me->mSlot = theSlot;
    if (inMode == 0)   // 2, 3: the selection is not the order's
        me->mActiveGroup->Clear();
}

static void EECPUnitsExec(EEComputerPlayer* me, const EeAiUnitsMessage& m)
{
    static_assert(sizeof(kUnitsMessages) == EE_AI_UNITS_MSG_EXCHANGE + 1, "ee_ai units_bind.rs EE_AI_UNITS_MSG_*");
    unsigned char theType = kUnitsMessages[m.message];
    const EeAiUnitsMsgValue* v = m.values;
    switch (m.count) {
    case 0: { EEMRecord theMessage(theType); EECPUnitsExecute(me, theMessage, m.mode); break; }
    case 1: { EEMRecord theMessage(theType, {EECPUnitsValue(v[0])}); EECPUnitsExecute(me, theMessage, m.mode); break; }
    case 2: { EEMRecord theMessage(theType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1])}); EECPUnitsExecute(me, theMessage, m.mode); break; }
    case 3: {
        EEMRecord theMessage(theType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2])});
        EECPUnitsExecute(me, theMessage, m.mode);
        break;
    }
    case 4: {
        EEMRecord theMessage(theType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2]), EECPUnitsValue(v[3])});
        EECPUnitsExecute(me, theMessage, m.mode);
        break;
    }
    case 5: {
        EEMRecord theMessage(theType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2]), EECPUnitsValue(v[3]),
            EECPUnitsValue(v[4])});
        EECPUnitsExecute(me, theMessage, m.mode);
        break;
    }
    case 7: {
        EEMRecord theMessage(theType, {EECPUnitsValue(v[0]), EECPUnitsValue(v[1]), EECPUnitsValue(v[2]), EECPUnitsValue(v[3]),
            EECPUnitsValue(v[4]), EECPUnitsValue(v[5]), EECPUnitsValue(v[6])});
        EECPUnitsExecute(me, theMessage, m.mode);
        break;
    }
    default:
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The continent manager and the walls for the Rust bindings (ee_ai units_more.rs): the only reads of the
// continent manager's objects the bindings make (lane C re-points these when the continents move)

// the last list handed out, for a second read into a larger buffer (EE_AI_UNITS_HOST_TILES_AGAIN): a list is never
// computed twice (the shore's GetTilesNextTo and FindPath are captured)
static std::vector<U2DPoint<long> > sUnitsLastTiles;

static void EECPUnitsTilesOut(EeAiUnitsTiles& ioTiles, const std::vector<U2DPoint<long> >& inTiles)
{
    for (size_t k = 0; k < inTiles.size() && k < ioTiles.cap; k++) {
        ioTiles.xy[2 * k] = inTiles[k].mX;
        ioTiles.xy[2 * k + 1] = inTiles[k].mY;
    }
    if (inTiles.size() > ioTiles.cap)
        sUnitsLastTiles = inTiles;
}

static void EECPUnitsTilesIn(const EeAiUnitsTiles& inTiles, std::vector<U2DPoint<long> >& outTiles)
{
    outTiles.clear();
    for (size_t k = 0; k < inTiles.len; k++)
        outTiles.push_back(U2DPoint<long>(inTiles.xy[2 * k], inTiles.xy[2 * k + 1]));
}

// the classic wall manager's area utilities (EECPWallManager.cpp declares them the same way)
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles);

static int64_t EECPUnitsWallsHost(EEComputerPlayer* me, int32_t inOp, int64_t a, int64_t b, void* outRecord)
{
    switch (inOp) {
    case EE_AI_UNITS_HOST_TILES_AGAIN: {
        std::vector<U2DPoint<long> > theTiles;
        theTiles.swap(sUnitsLastTiles);
        EECPUnitsTilesOut(*(EeAiUnitsTiles*)outRecord, theTiles);
        return (int64_t)theTiles.size();
    }
    case EE_AI_UNITS_HOST_TILES_LEVEL: {
        std::vector<U2DPoint<long> > theFive;
        EECPUnitsTilesIn(*(EeAiUnitsTiles*)outRecord, theFive);
        return EEAreTilesLevel(theFive) ? 1 : 0;
    }
    // the wall path and the wall routes: Rust (units_more.rs over ee_sim::astar pm_entry.rs, request.rs)
    default:
        return 0;
    }
}

// the terrain a unit crosses (EEUnitClass::GetTerrainFlags, the classic's sub-group common move flags): forest bit 0,
// cliff bit 1
static int64_t EECPUnitsCrosses(EEComplexUnit* inUnit)
{
    EECPASGFlags theFlags;
    memset(theFlags.mFlags, 0, sizeof(theFlags.mFlags));
    theFlags.mPlayerID = 0;
    inUnit->mUnitClass->GetTerrainFlags(inUnit, theFlags);
    return (theFlags.mFlags[kAITileForest] != 0 ? 1 : 0) | (theFlags.mFlags[kAITileCliff] != 0 ? 2 : 0);
}

// ai.tuning's switches (ee_ai units_host.rs EE_AI_UNITS_SWITCH_*)
static int64_t EECPUnitsSwitch(EEComputerPlayer* me, int64_t inSwitch)
{
    switch (inSwitch) {
    case EE_AI_UNITS_SWITCH_DEMAND_SPLIT: return EECPDemandSplitOn(me);
    case EE_AI_UNITS_SWITCH_COAST_FISHING: return EECPCoastFishingOn(me);
    case EE_AI_UNITS_SWITCH_FORAGE_PATCHES: return EECPForagePatchesOn(me);
    case EE_AI_UNITS_SWITCH_CLAIM_RANGE: return EECPClaimRangeOn(me);
    case EE_AI_UNITS_SWITCH_CLAIM_NEEDED: return EECPClaimNeededOn(me);
    case EE_AI_UNITS_SWITCH_EARLY_TOWN_CENTERS: return EECPEarlyTownCenters(me);
    case EE_AI_UNITS_SWITCH_EARLY_TRAINERS: return EECPEarlyTrainers(me);
    case EE_AI_UNITS_SWITCH_PATCH_EXPAND: return EECPPatchExpandOn(me);
    case EE_AI_UNITS_SWITCH_MILITIA: return EECPMilitiaOn(me);
    case EE_AI_UNITS_SWITCH_MILITIA_TOWERS: return EECPMilitiaTowersOn(me);
    case EE_AI_UNITS_SWITCH_PREHISTORIC_ARMY: return EECPPrehistoricArmy(me);
    case EE_AI_UNITS_SWITCH_PREHISTORIC_PEACE: return EECPPrehistoricPeaceOn(me);
    case EE_AI_UNITS_SWITCH_STONE_PEACE: return EECPStonePeaceOn(me);
    case EE_AI_UNITS_SWITCH_NAVAL_FLEE: return EECPNavalFleeOn(me);
    }
    return 0;
}

// the units a gate's fit asks about (EE_AI_UNITS_HOST_GATE_FITS: emptied for each side)
static std::vector<EEUnit*> sGateUnits;

// (the world's and the continent manager's reads are Rust's: units_bind.rs world_op)
extern "C" int64_t ee_ai_units_host_cpp(int32_t inOp, void* inPlayer, int64_t a, int64_t b, int64_t c, void* outRecord)
{
    EEComputerPlayer* me = (EEComputerPlayer*)inPlayer;
    switch (inOp) {
    case EE_AI_UNITS_HOST_STATE:
        return (int64_t)(uintptr_t)EECPUnitsGet(me)->mRust;
    case EE_AI_UNITS_HOST_UNIT:
        return a > 0 ? (int64_t)EECPUnitsHandle(gWorld.GetUnit((unsigned long)a)) : 0;
    case EE_AI_UNITS_HOST_MAP_SIZE: {
        // the map's size in tiles (ee/EEMapDimensions.h)
        long w = gWorld.mMap != NULL ? (long)gMapWidth : 0, h = gWorld.mMap != NULL ? (long)gMapHeight : 0;
        return (int64_t)(((uint64_t)(uint32_t)h << 32) | (uint32_t)w);
    }
    case EE_AI_UNITS_HOST_CLASS_BY_NAME:
        return EECPNextClassByName((const char*)(uintptr_t)a);
    case EE_AI_UNITS_HOST_CLASS_NAME: {
        std::string theName = EECPNextClassName((long)a);
        size_t n = theName.size() < 1023 ? theName.size() : 1023;
        memcpy(outRecord, theName.data(), n);
        ((char*)outRecord)[n] = 0;
        return (int64_t)n;
    }
    case EE_AI_UNITS_HOST_DETACH:
        me->DetachUnitFromAI((EEComplexUnit*)(uintptr_t)a);
        return 0;
    case EE_AI_UNITS_HOST_ATTACH: {
        EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)a;
        if (theUnit->mIsDetachedFromAI)
            theUnit->mIsDetachedFromAI = false;
        return 0;
    }
    case EE_AI_UNITS_HOST_UPGRADE: {
        EEPlayer* theOwner = (EEPlayer*)(uintptr_t)a;
        if (b < 0 || (unsigned long)b >= theOwner->mUnitClassUpgrades.size())
            return 0;
        return (int64_t)(uintptr_t)theOwner->mUnitClassUpgrades[(unsigned long)b];
    }
    case EE_AI_UNITS_HOST_TRACE:
        if (gCPTraceOn) {
            EECPTraceScope theScope("next");
            EECPTraceOut(me, (const char*)(uintptr_t)a, "%s", (const char*)(uintptr_t)b);
        }
        return 0;
    case EE_AI_UNITS_HOST_TRACE_ON:
        return gCPTraceOn ? 1 : 0;
    case EE_AI_UNITS_HOST_ASSIGNED:
        return EECPNextAssigned(me, (EEUnit*)(uintptr_t)a);
    case EE_AI_UNITS_HOST_HAS_ROOM:
        return EECPNextHasRoom(me, (EEUnit*)(uintptr_t)a) ? 1 : 0;
    case EE_AI_UNITS_HOST_IS_HUMAN:
        return ((EEPlayer*)(uintptr_t)a)->IsHuman() ? 1 : 0;
    case EE_AI_UNITS_HOST_TEAMS:
        return (int64_t)gWorld.mTeams.size();
    case EE_AI_UNITS_HOST_TEAM_HAS:
        return gWorld.mTeams[(size_t)a].ContainsPlayer((unsigned long)b) ? 1 : 0;
    case EE_AI_UNITS_HOST_TEAM_SIZE:
        return (int64_t)gWorld.mTeams[(size_t)a].GetNumPlayers();
    case EE_AI_UNITS_HOST_TEAM_PLAYER:
        return (int64_t)gWorld.mTeams[(size_t)a].GetPlayersAt((unsigned long)b);
    case EE_AI_UNITS_HOST_GATHER_TARGET: {
        EEUGRust* theGather = EEGatherGoalCast(((EEComplexUnit*)(uintptr_t)a)->GetGoalState());
        if (theGather != NULL && theGather->GetGatherResource() != NULL && theGather->GetGatherResource()->mIsValid)
            return (int64_t)theGather->GetGatherResource()->mUnitID;
        return 0;
    }
    case EE_AI_UNITS_HOST_SELECT_BEGIN:
        if (me->mActiveGroup == NULL)
            return 0;
        me->mActiveGroup->Clear();
        return 1;
    case EE_AI_UNITS_HOST_SELECT_ADD:
        me->mActiveGroup->Add((EEComplexUnit*)(uintptr_t)a);
        return 0;
    case EE_AI_UNITS_HOST_EXEC:
        EECPUnitsExec(me, *(const EeAiUnitsMessage*)outRecord);
        return 0;
    case EE_AI_UNITS_HOST_TECH_BY_NAME:
        return EECPNextTechByName((const char*)(uintptr_t)a);
    case EE_AI_UNITS_HOST_CAN_PRODUCE:
        return me->EEPlayer::CanProduce((long)a, 0x20, 1, true, 3, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_CAN_PLACE:
        return ee_units_uu_can_place_unit_on_tile((uint64_t)(uintptr_t)me, 0, (long)a, (long)b, (long)c, false, 1, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_LAND_BUILDING:
        return ((EEUnitClass*)(uintptr_t)a)->IsLandBuilding() ? 1 : 0;
    case EE_AI_UNITS_HOST_BEHAVIOR_OK:
        return a >= 0 && (unsigned long)a < EEUnitBehavior::sTable.size() && EEUnitBehavior::sTable[(unsigned long)a] != NULL;
    case EE_AI_UNITS_HOST_HAS_NODE:
        return me->mTechnologies->Get((long)a) != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_SEES_ALL:
        return EECPEngineSeesAll(me) ? 1 : 0;
    case EE_AI_UNITS_HOST_BLOCK_TAKES: {
        long theSize = *(const long*)outRecord;
        return ee_units_uu_block_takes((uint64_t)(uintptr_t)me, (long)a, (long)b - 1, (long)c - 1, theSize + 2, theSize + 2, 0) ? 1 : 0;
    }
    case EE_AI_UNITS_HOST_HAS_TREE:
        return me->mTechnologies != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_WALL_CLASS:
        return EECPWallClass(me);
    case EE_AI_UNITS_HOST_NODE_AFFORD: {
        EETechTreeNode* theNode = me->mTechnologies->Get((long)a);
        if (theNode == NULL)
            return -1;
        return me->CanAfford(theNode, 0x20, 1) ? 1 : 0;
    }
    case EE_AI_UNITS_HOST_FLIES_OVER:
        return ((EEUnitClass*)(uintptr_t)a)->FliesOver() ? 1 : 0;
    case EE_AI_UNITS_HOST_POP_COST:
        return ((EEUnitClass*)(uintptr_t)a)->GetPopulationCost();
    case EE_AI_UNITS_HOST_DAMAGES:
        return EEDamageFactor((long)a, (long)b) != 0.0f ? 1 : 0;
    case EE_AI_UNITS_HOST_TEAM_INDEX:
        return (int64_t)((EEPlayer*)(uintptr_t)a)->GetTeamIndex();
    case EE_AI_UNITS_HOST_WONDER_COUNT:
        return (int64_t)((EEPlayer*)(uintptr_t)a)->GetNumWonderUnits();
    case EE_AI_UNITS_HOST_NODE_COST:
        return me->ScaleCost(((EETechTreeNode*)(uintptr_t)a)->GetCost((long)b, (long)c, 1.0f));
    case EE_AI_UNITS_HOST_SCALE_COST:
        return me->ScaleCost((long)a);
    case EE_AI_UNITS_HOST_NODE_AVAILABLE:
        return ((EETechTreeNode*)(uintptr_t)a)->GetIsAvailable() ? 1 : 0;
    case EE_AI_UNITS_HOST_NODE_OBSOLETE:
        return ((EETechTreeNode*)(uintptr_t)a)->GetIsObsolete() ? 1 : 0;
    case EE_AI_UNITS_HOST_TECH_NAME: {
        std::string theName = EECPNextTechName((long)a);
        size_t n = theName.size() < 1023 ? theName.size() : 1023;
        memcpy(outRecord, theName.data(), n);
        ((char*)outRecord)[n] = 0;
        return (int64_t)n;
    }
    case EE_AI_UNITS_HOST_CAN_PRODUCE_AS:
        return me->CanProduce((long)a, (long)b, 1, c != 0, 3, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_CITIZEN_UPGRADE:
        return EECPUnitsCitizenUpgrade((EETechTreeNode*)(uintptr_t)a) ? 1 : 0;
    case EE_AI_UNITS_HOST_IS_EPOCH_ID:
        return EEIsEpochID((long)a) ? 1 : 0;
    case EE_AI_UNITS_HOST_EPOCH_COUNT:
        return EEEpochCount();
    case EE_AI_UNITS_HOST_IMPROVE_COST:
        return EECPUnitsImproveCost(me, (long)a, (long)b, *(EeAiUnitsImproveCost*)outRecord) ? 1 : 0;
    case EE_AI_UNITS_HOST_CAN_BUY:
        return me->CanBuy((long)a, (long)b, NULL) ? 1 : 0;
    case EE_AI_UNITS_HOST_GATE_CLASS:
        return EECPGateClass(me);
    case EE_AI_UNITS_HOST_IS_GATE_CLASS:
        return EEUnitClassOfKind<EE_CLASS_KIND_GATE>((EEUnitClass*)(uintptr_t)a) != NULL ? 1 : 0;
    case EE_AI_UNITS_HOST_TRIBUTE: {
        EEPlayer* theTo = gWorld.mPlayers[(long)a];
        me->TakeResource((long)b, (long)c);
        me->AddTribute(0, (long)b, (long)c);
        theTo->AddResource((long)b, (long)c);
        theTo->AddTribute(1, (long)b, (long)c);
        return 0;
    }
    case EE_AI_UNITS_HOST_STANCE:
        gWorld.SetDiplomaticStance(me->mPlayerID, (long)a, (long)b);
        return 0;
    case EE_AI_UNITS_HOST_GARRISON_FILL: {
        EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)a;
        return theUnit->AddGarrison(theUnit->mGarrisonCount + (long)b, theUnit->mGarrisonPopulation) ? 1 : 0;
    }
    case EE_AI_UNITS_HOST_SITE_BLOCKED:
        return EECPNextSiteBlocked(me, (EEUnit*)(uintptr_t)a, NULL);
    case EE_AI_UNITS_HOST_BUILD_UNSAFE:
        return EECPNextBuildUnsafe(me, (long)a, (long)b);
    case EE_AI_UNITS_HOST_SITE_THREAT:
        return EECPNextSiteThreat(me, (long)a, (long)b, false, NULL);
    case EE_AI_UNITS_HOST_ENEMY_NEAR_ALL:
        return EECPUnitsEnemyNearAll(me, (long)a, (long)b);
    case EE_AI_UNITS_HOST_PLACE: {
        EeAiUnitsPlace& p = *(EeAiUnitsPlace*)outRecord;
        std::vector<long> theUnits(p.units, p.units + p.units_len);
        return EECPNextPlaceSearch(me, p.unit_type, p.mode, p.x, p.y, p.first, p.last, theUnits, p.site, p.out_x, p.out_y,
            p.out_gap) ? 1 : 0;
    }
    case EE_AI_UNITS_HOST_FILLS_GARRISON:
        return EECPEngineFillsGarrison(me) ? 1 : 0;
    case EE_AI_UNITS_HOST_GOAL_LOCATION: reinterpret_cast<EEUGRust*>((uintptr_t)a)->GetGoalLocation(*(GE3DPoint*)outRecord);
        return 0;
    case EE_AI_UNITS_HOST_MOVES:
        return !((EEComplexUnit*)(uintptr_t)a)->mUnitClass->IsBuilding() && ((EEComplexUnit*)(uintptr_t)a)->mUnitClass->GetMaxVelocity() > 0.0f ? 1 : 0;
    case EE_AI_UNITS_HOST_CROSSES:
        return EECPUnitsCrosses((EEComplexUnit*)(uintptr_t)a);
    case EE_AI_UNITS_HOST_BEHAVIORS:
        return (int64_t)EEUnitBehavior::sTable.size();
    case EE_AI_UNITS_HOST_BEHAVIOR_AT:
        return (int64_t)(uintptr_t)EEUnitBehavior::sTable[(size_t)a];
    case EE_AI_UNITS_HOST_GATE_FITS:
        return EEDoesWallGateFitOnUnit((EEComplexUnit*)(uintptr_t)a, (long)b, (long)c, sGateUnits = std::vector<EEUnit*>()) ? 1 : 0;
    case EE_AI_UNITS_HOST_GATE_WALL:
        return ((EEUnit*)(uintptr_t)a)->GetUnitClass()->GetIsWall() && ((EEUnit*)(uintptr_t)a)->GetHitPoints() > 0 && ((EEUnit*)(uintptr_t)a)->mPlayer == me && ((EEUnit*)(uintptr_t)a)->GetComplexUnit() != NULL && ((EEUnit*)(uintptr_t)a)->GetComplexUnit()->GetIsBuilt() ? 1 : 0;
    case EE_AI_UNITS_HOST_T_DEATHMATCH:
        return me->IsDeathmatch() ? 1 : 0;
    case EE_AI_UNITS_HOST_T_RESOURCES:
        return gWorld.mGameOptions.mResources;
    case EE_AI_UNITS_HOST_T_EPOCH_START:
        return gWorld.mGameOptions.mEpochStart;
    case EE_AI_UNITS_HOST_T_EPOCH_END:
        return gWorld.mGameOptions.mEpochEnd;
    case EE_AI_UNITS_HOST_T_SPACE:
        return gWorld.mMap != NULL && gWorld.mMap->mTerrainMesh.GetIsSpaceMap() ? 1 : 0;
    case EE_AI_UNITS_HOST_T_POP_LIMIT:
        return me->EEPlayer::GetPopulationLimit(0);
    case EE_AI_UNITS_HOST_T_CHEAT:
        return EECPCheatOn(me, (long)a) ? 1 : 0;
    case EE_AI_UNITS_HOST_T_ID: snprintf((char*)outRecord, 128, "%s", EECPPersonality(me).mID.c_str());
        return 0;
    case EE_AI_UNITS_HOST_T_PRESSURE_START: *(float*)outRecord = EECPPersonality(me).mEpochPressureStart;
        return 0;
    case EE_AI_UNITS_HOST_T_PRESSURE_FULL: *(float*)outRecord = EECPPersonality(me).mEpochPressureFull;
        return 0;
    case EE_AI_UNITS_HOST_T_SURPLUS_CUT: *(float*)outRecord = EECPPersonality(me).mSurplusCut;
        return 0;
    case EE_AI_UNITS_HOST_T_MAX_CITIZEN_SHARE: *(float*)outRecord = EECPPersonality(me).mMaxCitizenShare;
        return 0;
    case EE_AI_UNITS_HOST_T_STANDING_ARMY: *(float*)outRecord = EECPPersonality(me).mStandingArmy;
        return 0;
    case EE_AI_UNITS_HOST_T_ARMY_FLOOR: *(float*)outRecord = EECPPersonality(me).mArmyFloor;
        return 0;
    case EE_AI_UNITS_HOST_T_FLOOR: *(float*)outRecord = EECPPersonality(me).mFloor[(size_t)a];
        return 0;
    case EE_AI_UNITS_HOST_T_STONE_SPAN: *(float*)outRecord = EECPStoneSpan(me);
        return 0;
    case EE_AI_UNITS_HOST_T_SAFE_BONUS: *(float*)outRecord = EECPSafeBonus(me);
        return 0;
    case EE_AI_UNITS_HOST_T_SURPLUS:
        return EECPPersonality(me).mSurplus;
    case EE_AI_UNITS_HOST_T_LOW_STOCK:
        return EECPPersonality(me).mLowStock;
    case EE_AI_UNITS_HOST_T_WOOD_SLOTS:
        return EECPPersonality(me).mWoodSlots;
    case EE_AI_UNITS_HOST_T_MOVES_PER_CYCLE:
        return EECPPersonality(me).mMovesPerCycle;
    case EE_AI_UNITS_HOST_T_SWITCH:
        return EECPUnitsSwitch(me, a);
    case EE_AI_UNITS_HOST_T_BEHAVIORS:
        return (int64_t)(uintptr_t)gCPBehaviors;
    case EE_AI_UNITS_HOST_TILES_LEVEL: case EE_AI_UNITS_HOST_TILES_AGAIN:
        return EECPUnitsWallsHost(me, inOp, a, b, outRecord);
    default:
        return 0;
    }
}
