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
//   (EECPLuaState::Charge) raises, before anything is built;
// - commands run the human's own messages (EEMessage.cpp's Execute) on the computer player's selection, at once
//   (AI_LUA_UNITS §3.2): the selection is filled with the units, the player answers to a borrowed slot id while the
//   message runs, and both are put back.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
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
#include "lle/NEMessage.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/EEMapDimensions.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEMessage.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EEMessageID.h"
#include "ee/EECPTrace.h"
#include "ee/EEGameLog.h"
#include "ee/EECPLua.h"
#include "ee/EELuaApi.h"
#include "ee/EECPNext.h"
#include "ee/EECPNextUnits.h"
#include "platform/PlatSim.h"

// ---------------------------------------------------------------------------------------------------------------
// The state

namespace {

enum EventKind { kEvAttacked = 1, kEvDied, kEvCreated, kEvBuilt, kEvIdle, kEvSwapped, kEvConverted };
const char* const kEventNames[] = {"", "attacked", "died", "created", "built", "idle", "swapped", "converted"};

struct UnitsEvent {
    long mKind, mUnit, mOther, mType, mAmount, mTime;
};

struct Remembered {                 // the enemy memory (AI_LUA_UNITS §2.5)
    long mPlayer, mType, mX, mY, mHP, mSeen, mBuilding;
};

const long kMaxEvents = 512;
const long kMaxRemembered = 4000;
const long kMaxIds = 4000;           // ids one call takes
const long kMicroMs = 250;           // the micro step's period (game ms)
const int64_t kMicroBudget = 300000;
const long kForwardMs = 600000;      // a swap's forwarding entry lives 10 minutes
const unsigned char kBorrowedSlot = 0xfd;   // the slot id a computer player answers to while its order runs

// the costs in instruction equivalents (AI_LUA_UNITS §2.1)
const int64_t kCostCall = 200, kCostId = 20, kCostScan = 5, kCostRecord = 300, kCostField = 15, kCostTile = 50,
              kCostCommand = 300, kCostOrdered = 100;

}  // namespace

struct EECPUnitsState {
    std::set<long> mHeld;
    std::vector<UnitsEvent> mEvents;
    long mDropped = 0;
    std::map<long, long> mAttackedAt;               // unit -> its index in mEvents (one attacked event per delivery)
    std::map<long, std::pair<long, long> > mForward; // old id -> (new id, game ms)
    std::map<long, Remembered> mMemory;
    long mMemoryMs = 180000;
    long mMemoryNext = 0;
    bool mReflexNavalFlee = true;
    bool mReflexDanger = true;
    long mMicroNext = -1;                           // -1: not started; 0x7fffffff: the script has no micro
    long mOrdersThisStep = 0, mUnitOrdersThisStep = 0;
    long mStepStamp = -1;                           // the game ms of the counters above
    // scratch (members, so that a Lua error never skips a destructor)
    std::vector<long> mIds, mIds2;
    std::vector<std::pair<long long, long> > mSorted;
    std::map<long, std::string> mNames;
};

static EECPUnitsState* EECPUnitsGet(EEComputerPlayer* inPlayer)
{
    if (inPlayer->mUnitsState == NULL)
        inPlayer->mUnitsState = new EECPUnitsState();
    return inPlayer->mUnitsState;
}

void EECPUnitsDelete(EEComputerPlayer* inPlayer)
{
    delete inPlayer->mUnitsState;
    inPlayer->mUnitsState = NULL;
}

// ---------------------------------------------------------------------------------------------------------------
// Units

static long EECPUnitsForward(const EECPUnitsState* s, long inID)
{
    for (int k = 0; k < 8 && s != NULL; k++) {
        std::map<long, std::pair<long, long> >::const_iterator it = s->mForward.find(inID);
        if (it == s->mForward.end())
            break;
        inID = it->second.first;
    }
    return inID;
}

static EEUnit* EECPUnitsAlive(long inID)
{
    if (inID <= 0)
        return NULL;
    EEUnit* theUnit = gWorld.GetUnit((unsigned long)inID);
    if (theUnit == NULL || !theUnit->mIsValid || !(theUnit->mHitPoints > 0) || theUnit->mUnitClass == NULL)
        return NULL;
    return theUnit;
}

static EEComplexUnit* EECPUnitsOwn(EEComputerPlayer* inPlayer, long inID)
{
    EEUnit* theUnit = EECPUnitsAlive(EECPUnitsForward(inPlayer->mUnitsState, inID));
    if (theUnit == NULL || theUnit->mPlayer != inPlayer || theUnit->mComplexUnit == NULL)
        return NULL;
    return theUnit->mComplexUnit;
}

static bool EECPUnitsOnMap(EEUnit* inUnit)
{
    return inUnit->GetTileX() != -10000 && inUnit->mTile != NULL;
}

// whether the player sees the unit now: its own and its allies' always; others while their tile is seen
static bool EECPUnitsSeen(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    if (inUnit->mPlayer == inPlayer || (inUnit->mPlayer != NULL && inUnit->mPlayer != gWorld.mPlayers[0]
        && inPlayer->IsAlly(inUnit->mPlayer)))
        return true;
    if (!EECPUnitsOnMap(inUnit))
        return false;
    return static_cast<EETile*>(inUnit->mTile)->IsSeenBy(inPlayer);
}

static bool EECPUnitsExplored(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    return EECPUnitsOnMap(inUnit) && static_cast<EETile*>(inUnit->mTile)->IsVisibleTo(inPlayer);
}

static bool EECPUnitsIsResource(EEUnit* inUnit)
{
    long r = (long)inUnit->mResourceType - kResourceFood;
    return r >= 0 && r < 5 && inUnit->mResourceAmount > 0
        && (inUnit->mPlayer == NULL || inUnit->mPlayer == gWorld.mPlayers[0]);
}

static const char* EECPUnitsCategory(EEUnit* inUnit)
{
    EEUnitClass* c = inUnit->mUnitClass;
    switch (c->mCategory) {
    case kUnitCategoryLandCitizen: return "citizen";
    case kUnitCategoryNavalCitizen: return "boat";
    case kUnitCategoryResource: return "resource";
    case kUnitCategoryAmbient: return inUnit->mResourceAmount > 0 ? "resource" : "ambient";
    case kUnitCategoryAirplane: case kUnitCategoryHelicopter: return "air";
    case kUnitCategoryNaval: case kUnitCategorySubmergedNaval: return "ship";
    case kUnitCategoryLandTransport: case kUnitCategoryHelicopterTransport: case kUnitCategoryNavalTransport:
    case kUnitCategorySpaceTransport:
        return "transport";
    case kUnitCategoryLandPriest: case kUnitCategoryNavalPriest: case kUnitCategoryHelicopterPriest:
    case kUnitCategoryBuildingPriest:
        return "priest";
    case kUnitCategoryLandMilitaryHero: return "hero";
    case kUnitCategorySeaAnimal: case kUnitCategoryAirAnimal: return "animal";
    default:
        break;
    }
    if (c->IsBuilding())
        return "building";
    if (c->GetFamily() == kUnitFamilyAnimal)
        return "animal";
    return "soldier";
}

static bool EECPUnitsArmed(EEUnit* inUnit)
{
    return inUnit->mStats != NULL && inUnit->mStats->mAttack != 0.0f;
}

static const char* EECPUnitsAction(EEComplexUnit* inUnit)
{
    if (inUnit->mContainer != NULL)
        return "inside";
    if (inUnit->mUnitClass->IsBuilding()) {
        if (!inUnit->mIsBuilt)
            return "unbuilt";
        return inUnit->mCurrentResearch != 100000 ? "produce" : "idle";
    }
    EEUnitGoal* g = inUnit->GetGoalState();
    if (g == NULL)
        return "idle";
    switch (g->GetKind()) {
    case kGoalLocation: return "move";
    case kGoalAttackUnit: return "attack";
    case kGoalAttackGround: return "attack_ground";
    case kGoalBuildRepair: return "build";
    case kGoalGather: return "gather";
    case kGoalLoadTransport: return "load";
    case kGoalUnloadTransport: return "unload";
    case kGoalPatrol: return "patrol";
    case kGoalExplore: return "explore";
    case kGoalCastCalamity: return "cast";
    case kGoalLoadGarrison: return "garrison";
    case kGoalHealUnit: return "heal";
    default: return "other";
    }
}

static bool EECPUnitsIdle(EEComplexUnit* inUnit)
{
    return inUnit->mContainer == NULL && strcmp(EECPUnitsAction(inUnit), "idle") == 0;
}

static long EECPUnitsCenti(float inValue)
{
    return UMFloor(inValue * 100.0f);
}

static long EECPUnitsD2(long inX1, long inY1, long inX2, long inY2)
{
    long dx = inX1 - inX2, dy = inY1 - inY2;
    return dx * dx + dy * dy;
}

static long EECPUnitsStrength(EEUnit* inUnit)
{
    return inUnit->mStats != NULL ? (long)inUnit->mHitPoints * (long)UMFloatToLong(inUnit->mStats->mAttack) : 0;
}

// ---------------------------------------------------------------------------------------------------------------
// The held set and the reflexes

bool EECPUnitsHoldsID(const EEComputerPlayer* inPlayer, long inID)
{
    return inPlayer != NULL && inPlayer->mUnitsState != NULL && inPlayer->mUnitsState->mHeld.count(inID) != 0;
}

bool EECPUnitsHolds(const EEComputerPlayer* inPlayer, const EEComplexUnit* inUnit)
{
    return inUnit != NULL && EECPUnitsHoldsID(inPlayer, (long)inUnit->mUnitID);
}

void EECPUnitsHeld(const EEComputerPlayer* inPlayer, std::vector<long>& outIDs)
{
    outIDs.clear();
    if (inPlayer != NULL && inPlayer->mUnitsState != NULL)
        outIDs.assign(inPlayer->mUnitsState->mHeld.begin(), inPlayer->mUnitsState->mHeld.end());
}

bool EECPUnitsReflexOn(const EEComputerPlayer* inPlayer, const char* inName)
{
    if (inPlayer == NULL || inPlayer->mUnitsState == NULL)
        return true;
    if (strcmp(inName, "naval_flee") == 0)
        return inPlayer->mUnitsState->mReflexNavalFlee;
    if (strcmp(inName, "danger") == 0)
        return inPlayer->mUnitsState->mReflexDanger;
    return true;
}

// a unit becomes the script's: out of the classic attack commander (DetachUnitFromAI) and the gatherer lists
static void EECPUnitsTake(EEComputerPlayer* inPlayer, EECPUnitsState* s, EEComplexUnit* inUnit)
{
    long theID = (long)inUnit->mUnitID;
    if (s->mHeld.count(theID))
        return;
    s->mHeld.insert(theID);
    if (!inUnit->mUnitClass->IsBuilding()) {
        inPlayer->DetachUnitFromAI(inUnit);
        if (inPlayer->mCivilizationManager != NULL && inPlayer->mCivilizationManager->mResourceManager != NULL)
            inPlayer->mCivilizationManager->mResourceManager->RemoveGatherer(inUnit);
    }
}

static void EECPUnitsRelease(EEComputerPlayer* inPlayer, EECPUnitsState* s, EEComplexUnit* inUnit)
{
    if (s->mHeld.erase((long)inUnit->mUnitID) == 0 || inUnit->mUnitClass->IsBuilding())
        return;
    if (inUnit->mIsDetachedFromAI)
        inUnit->mIsDetachedFromAI = false;
    if (inPlayer->mCombatManager != NULL)
        inPlayer->mCombatManager->UnitAdded(inUnit);
    long theCategory = inUnit->mUnitClass->mCategory;
    if ((theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen) && EECPUnitsIdle(inUnit)
        && inPlayer->mCivilizationManager != NULL && inPlayer->mCivilizationManager->mResourceManager != NULL)
        inPlayer->mCivilizationManager->mResourceManager->AddIdleGatherer(inUnit);
}

// ---------------------------------------------------------------------------------------------------------------
// Events

static void EECPUnitsEventAdd(EECPUnitsState* s, long inKind, long inUnit, long inOther, long inType, long inAmount)
{
    if ((long)s->mEvents.size() >= kMaxEvents) {
        s->mDropped++;
        return;
    }
    UnitsEvent e = {inKind, inUnit, inOther, inType, inAmount, (long)gPhysics->mGameTime};
    s->mEvents.push_back(e);
}

void EECPUnitsOnDamage(EEComputerPlayer* inPlayer, long inAttackerPlayer, long inAmount, EEUnit* inUnit)
{
    EECPUnitsState* s = inPlayer->mUnitsState;
    if (s == NULL || inUnit == NULL)
        return;
    long theID = (long)inUnit->mUnitID;
    long theAttacker = 0;
    EEComplexUnit* theLast = inUnit->mComplexUnit != NULL ? inUnit->mComplexUnit->mLastAttacker : NULL;
    if (theLast != NULL && theLast->mHitPoints > 0 && EECPUnitsSeen(inPlayer, theLast))
        theAttacker = (long)theLast->mUnitID;
    std::map<long, long>::iterator it = s->mAttackedAt.find(theID);
    if (it != s->mAttackedAt.end() && it->second < (long)s->mEvents.size() && s->mEvents[it->second].mUnit == theID
        && s->mEvents[it->second].mKind == kEvAttacked) {
        UnitsEvent& e = s->mEvents[it->second];
        e.mAmount += inAmount;
        if (theAttacker != 0)
            e.mOther = theAttacker;
        e.mType = inAttackerPlayer;
        e.mTime = (long)gPhysics->mGameTime;
        return;
    }
    s->mAttackedAt[theID] = (long)s->mEvents.size();
    EECPUnitsEventAdd(s, kEvAttacked, theID, theAttacker, inAttackerPlayer, inAmount);
}

void EECPUnitsOnPlaced(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    EECPUnitsState* s = inPlayer->mUnitsState;
    if (s != NULL && inUnit != NULL && inUnit->mUnitClass != NULL)
        EECPUnitsEventAdd(s, kEvCreated, (long)inUnit->mUnitID, 0, inUnit->mUnitClass->mUnitType, 0);
}

void EECPUnitsOnCompleted(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    EECPUnitsState* s = inPlayer->mUnitsState;
    if (s != NULL && inUnit != NULL && inUnit->mUnitClass != NULL && inUnit->mUnitClass->IsBuilding())
        EECPUnitsEventAdd(s, kEvBuilt, (long)inUnit->mUnitID, 0, inUnit->mUnitClass->mUnitType, 0);
}

void EECPUnitsOnTaken(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit, bool inSwapped)
{
    EECPUnitsState* s = inPlayer->mUnitsState;
    if (s == NULL || inUnit == NULL || inSwapped)
        return;
    long theID = (long)inUnit->mUnitID;
    long theKiller = 0;
    if (inUnit->mLastAttacker != NULL && EECPUnitsSeen(inPlayer, inUnit->mLastAttacker))
        theKiller = (long)inUnit->mLastAttacker->mUnitID;
    bool theConverted = inUnit->mHitPoints > 0;   // alive, but no longer the player's
    EECPUnitsEventAdd(s, theConverted ? kEvConverted : kEvDied, theID, theKiller,
        inUnit->mUnitClass != NULL ? inUnit->mUnitClass->mUnitType : -1, 0);
    s->mHeld.erase(theID);
}

void EECPUnitsOnGoalDone(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    EECPUnitsState* s = inPlayer->mUnitsState;
    if (s != NULL && inUnit != NULL && s->mHeld.count((long)inUnit->mUnitID))
        EECPUnitsEventAdd(s, kEvIdle, (long)inUnit->mUnitID, 0, inUnit->mUnitClass->mUnitType, 0);
}

void EECPUnitsOnSwapped(EEComputerPlayer* inPlayer, std::vector<EEPUnitSwap>& ioSwaps)
{
    EECPUnitsState* s = inPlayer->mUnitsState;
    if (s == NULL)
        return;
    long theNow = (long)gPhysics->mGameTime;
    for (unsigned long k = 0; k < ioSwaps.size(); k++) {
        if (ioSwaps[k].mOld == NULL || ioSwaps[k].mNew == NULL)
            continue;
        long theOld = (long)ioSwaps[k].mOld->mUnitID, theNew = (long)ioSwaps[k].mNew->mUnitID;
        if (ioSwaps[k].mOld->mPlayer != inPlayer && ioSwaps[k].mNew->mPlayer != inPlayer)
            continue;
        s->mForward[theOld] = std::make_pair(theNew, theNow);
        if (s->mHeld.erase(theOld) && ioSwaps[k].mNew->mPlayer == inPlayer)
            s->mHeld.insert(theNew);
        EECPUnitsEventAdd(s, kEvSwapped, theOld, theNew, ioSwaps[k].mNew->mUnitClass->mUnitType, 0);
    }
    for (std::map<long, std::pair<long, long> >::iterator it = s->mForward.begin(); it != s->mForward.end();) {
        if (theNow - it->second.second > kForwardMs)
            s->mForward.erase(it++);
        else
            ++it;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The enemy memory: every second, what the player sees of its enemies

static void EECPUnitsRemember(EEComputerPlayer* inPlayer, EECPUnitsState* s)
{
    long theNow = (long)gPhysics->mGameTime;
    for (unsigned long p = 1; p < gWorld.mNumPlayers && p < 64; p++) {
        EEPlayer* theOther = gWorld.mPlayers[p];
        if (theOther == NULL || theOther == inPlayer || inPlayer->IsAlly(theOther))
            continue;
        for (EEUnit* theUnit = theOther->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (!(theUnit->mHitPoints > 0) || theUnit->mComplexUnit == NULL || theUnit->mComplexUnit->mContainer != NULL
                || !EECPUnitsOnMap(theUnit) || !static_cast<EETile*>(theUnit->mTile)->IsSeenBy(inPlayer))
                continue;
            long theID = (long)theUnit->mUnitID;
            std::map<long, Remembered>::iterator it = s->mMemory.find(theID);
            if (it == s->mMemory.end() && (long)s->mMemory.size() >= kMaxRemembered)
                continue;
            Remembered& r = s->mMemory[theID];
            r.mPlayer = (long)p;
            r.mType = theUnit->mUnitClass->mUnitType;
            r.mX = theUnit->GetTileX();
            r.mY = theUnit->GetTileY();
            r.mHP = theUnit->mHitPoints;
            r.mSeen = theNow;
            r.mBuilding = theUnit->mUnitClass->IsBuilding() ? 1 : 0;
        }
    }
    // forget: the dead whose place is seen; buildings whose tile is seen without them; mobile units after memory_ms
    for (std::map<long, Remembered>::iterator it = s->mMemory.begin(); it != s->mMemory.end();) {
        Remembered& r = it->second;
        bool theForget = false;
        if (r.mSeen != theNow) {
            EETile* theTile = gWorld.mMap != NULL && r.mX >= 0 && r.mY >= 0
                ? static_cast<EETile*>(gWorld.mMap->GetTile((unsigned long)r.mX, (unsigned long)r.mY)) : NULL;
            bool theTileSeen = theTile != NULL && theTile->IsSeenBy(inPlayer);
            if (r.mBuilding)
                theForget = theTileSeen;   // seen now, it would have been refreshed
            else
                theForget = theNow - r.mSeen > s->mMemoryMs || (theTileSeen && EECPUnitsAlive(it->first) == NULL);
        }
        if (theForget)
            s->mMemory.erase(it++);
        else
            ++it;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Lua helpers

static EEComputerPlayer* EECPUnitsMe(lua_State* L)
{
    return (EEComputerPlayer*)lua_touserdata(L, lua_upvalueindex(1));
}

static void EECPUnitsCharge(lua_State* L, int64_t inAmount)
{
    EECPLuaState::From(L)->Charge(L, inAmount);
}

static const char* EECPUnitsName(EECPUnitsState* s, long inType)
{
    std::map<long, std::string>::iterator it = s->mNames.find(inType);
    if (it == s->mNames.end())
        it = s->mNames.insert(std::make_pair(inType, EECPNextClassName(inType))).first;
    return it->second.c_str();
}

static long EECPUnitsClassArg(lua_State* L, int inIndex)
{
    if (lua_type(L, inIndex) == LUA_TNUMBER && lua_isinteger(L, inIndex))
        return (long)lua_tointeger(L, inIndex);
    if (lua_type(L, inIndex) != LUA_TSTRING)
        return -1;
    const char* theName = lua_tostring(L, inIndex);
    return EECPNextClassByName(theName);
}

// ids at inIndex (an integer or an array of integers) -> outIds (no error: the count of bad entries)
static long EECPUnitsReadIds(lua_State* L, int inIndex, std::vector<long>& outIds)
{
    outIds.clear();
    long theBad = 0;
    if (lua_type(L, inIndex) == LUA_TNUMBER) {
        if (lua_isinteger(L, inIndex))
            outIds.push_back((long)lua_tointeger(L, inIndex));
        else
            theBad++;
        return theBad;
    }
    if (lua_type(L, inIndex) != LUA_TTABLE)
        return 1;
    lua_Integer n = (lua_Integer)lua_rawlen(L, inIndex);
    for (lua_Integer i = 1; i <= n && (long)outIds.size() < kMaxIds; i++) {
        lua_rawgeti(L, inIndex, i);
        if (lua_isinteger(L, -1))
            outIds.push_back((long)lua_tointeger(L, -1));
        else
            theBad++;
        lua_pop(L, 1);
    }
    return theBad;
}

static bool EECPUnitsOptInt(lua_State* L, int inTable, const char* inKey, long& outValue)
{
    if (lua_type(L, inTable) != LUA_TTABLE)
        return false;
    lua_getfield(L, inTable, inKey);
    bool theFound = lua_isinteger(L, -1) != 0;
    if (theFound)
        outValue = (long)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return theFound;
}

static bool EECPUnitsOptBool(lua_State* L, int inTable, const char* inKey, bool inDefault)
{
    if (lua_type(L, inTable) != LUA_TTABLE)
        return inDefault;
    lua_getfield(L, inTable, inKey);
    bool theValue = lua_isnil(L, -1) ? inDefault : lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return theValue;
}

// a string field -> a pointer into the Lua string (valid while the table holds it), or NULL
static const char* EECPUnitsOptString(lua_State* L, int inTable, const char* inKey)
{
    if (lua_type(L, inTable) != LUA_TTABLE)
        return NULL;
    lua_getfield(L, inTable, inKey);
    const char* theValue = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : NULL;
    lua_pop(L, 1);
    return theValue;
}

// `near = {x, y, r}` -> true with the values
static bool EECPUnitsOptNear(lua_State* L, int inTable, long& outX, long& outY, long& outR)
{
    if (lua_type(L, inTable) != LUA_TTABLE)
        return false;
    lua_getfield(L, inTable, "near");
    bool theOK = false;
    if (lua_type(L, -1) == LUA_TTABLE) {
        int t = lua_gettop(L);
        lua_rawgeti(L, t, 1);
        lua_rawgeti(L, t, 2);
        lua_rawgeti(L, t, 3);
        if (lua_isinteger(L, -3) && lua_isinteger(L, -2)) {
            outX = (long)lua_tointeger(L, -3);
            outY = (long)lua_tointeger(L, -2);
            outR = lua_isinteger(L, -1) ? (long)lua_tointeger(L, -1) : 1000000;
            theOK = true;
        }
        lua_pop(L, 3);
    }
    lua_pop(L, 1);
    return theOK;
}

static void EECPUnitsSetInt(lua_State* L, const char* inKey, long inValue)
{
    lua_pushinteger(L, (lua_Integer)inValue);
    lua_setfield(L, -2, inKey);
}

static void EECPUnitsSetStr(lua_State* L, const char* inKey, const char* inValue)
{
    lua_pushstring(L, inValue);
    lua_setfield(L, -2, inKey);
}

static void EECPUnitsSetBool(lua_State* L, const char* inKey, bool inValue)
{
    lua_pushboolean(L, inValue ? 1 : 0);
    lua_setfield(L, -2, inKey);
}

// the ids of s->mSorted (key, id), sorted, up to inLimit, as a Lua array
static void EECPUnitsPushSorted(lua_State* L, EECPUnitsState* s, long inLimit)
{
    std::sort(s->mSorted.begin(), s->mSorted.end());
    long n = (long)s->mSorted.size();
    if (inLimit >= 0 && n > inLimit)
        n = inLimit;
    lua_createtable(L, (int)n, 0);
    for (long k = 0; k < n; k++) {
        lua_pushinteger(L, (lua_Integer)s->mSorted[k].second);
        lua_rawseti(L, -2, (lua_Integer)(k + 1));
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Queries

static int LuaMe(lua_State* L)
{
    lua_pushinteger(L, (lua_Integer)EECPUnitsMe(L)->mPlayerID);
    return 1;
}

static int LuaTime(lua_State* L)
{
    lua_pushinteger(L, (lua_Integer)gPhysics->mGameTime);
    return 1;
}

static int LuaPlayers(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsCharge(L, kCostCall + 16 * kCostRecord / 10);
    lua_newtable(L);
    long n = 0;
    for (unsigned long p = 1; p < gWorld.mNumPlayers && p < 64; p++) {
        EEPlayer* theOther = gWorld.mPlayers[p];
        if (theOther == NULL || theOther->mObserverMode)
            continue;
        lua_newtable(L);
        EECPUnitsSetInt(L, "id", (long)theOther->mPlayerID);
        EECPUnitsSetBool(L, "me", theOther == me);
        EECPUnitsSetBool(L, "ally", theOther != me && me->IsAlly(theOther));
        EECPUnitsSetBool(L, "enemy", !me->IsAlly(theOther));
        EECPUnitsSetBool(L, "alive", theOther->mFirstUnit != NULL);
        EECPUnitsSetBool(L, "human", theOther->IsHuman());
        lua_rawseti(L, -2, (lua_Integer)++n);
    }
    return 1;
}

static int LuaMap(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    lua_newtable(L);
    long w = 0, h = 0;
    if (gWorld.mMap != NULL) {
        while (w < 4096 && gWorld.mMap->GetTile((unsigned long)w, 0) != NULL)
            w++;
        while (h < 4096 && gWorld.mMap->GetTile(0, (unsigned long)h) != NULL)
            h++;
    }
    EECPUnitsSetInt(L, "width", w);
    EECPUnitsSetInt(L, "height", h);
    return 1;
}

// the filters of ai.units / ai.seen / ai.resources
struct EECPUnitsFilter {
    long mType = -1;
    const char* mCategory = NULL;
    bool mHasNear = false;
    long mX = 0, mY = 0, mR = 0;
    int mIdle = -1, mBuilt = -1, mArmed = -1;
    const char* mHeld = NULL;
    const char* mAction = NULL;
    long mPlayer = -1;
    int mEnemy = -1;
    long mKind = -1;
    long mLimit = -1;
};

static void EECPUnitsReadFilter(lua_State* L, int inTable, EECPUnitsFilter& f)
{
    if (lua_type(L, inTable) != LUA_TTABLE)
        return;
    lua_getfield(L, inTable, "type");
    if (!lua_isnil(L, -1))
        f.mType = EECPUnitsClassArg(L, lua_gettop(L));
    if (!lua_isnil(L, -1) && f.mType < 0)
        f.mType = -2;   // an unknown name: matches nothing
    lua_pop(L, 1);
    f.mCategory = EECPUnitsOptString(L, inTable, "category");
    f.mHasNear = EECPUnitsOptNear(L, inTable, f.mX, f.mY, f.mR);
    lua_getfield(L, inTable, "idle");
    if (!lua_isnil(L, -1))
        f.mIdle = lua_toboolean(L, -1) ? 1 : 0;
    lua_pop(L, 1);
    lua_getfield(L, inTable, "built");
    if (!lua_isnil(L, -1))
        f.mBuilt = lua_toboolean(L, -1) ? 1 : 0;
    lua_pop(L, 1);
    lua_getfield(L, inTable, "armed");
    if (!lua_isnil(L, -1))
        f.mArmed = lua_toboolean(L, -1) ? 1 : 0;
    lua_pop(L, 1);
    lua_getfield(L, inTable, "enemy");
    if (!lua_isnil(L, -1))
        f.mEnemy = lua_toboolean(L, -1) ? 1 : 0;
    lua_pop(L, 1);
    f.mHeld = EECPUnitsOptString(L, inTable, "held");
    f.mAction = EECPUnitsOptString(L, inTable, "action");
    EECPUnitsOptInt(L, inTable, "player", f.mPlayer);
    EECPUnitsOptInt(L, inTable, "limit", f.mLimit);
    const char* theKind = EECPUnitsOptString(L, inTable, "kind");
    static const char* const kKinds[5] = {"food", "wood", "stone", "gold", "iron"};
    for (long k = 0; theKind != NULL && k < 5; k++)
        if (strcmp(theKind, kKinds[k]) == 0)
            f.mKind = k;
    if (theKind != NULL && f.mKind < 0)
        f.mKind = 99;
}

// the common tests of a filter; outKey the sort key (the squared distance with near, else 0)
static bool EECPUnitsPasses(EEUnit* inUnit, const EECPUnitsFilter& f, long long& outKey)
{
    if (f.mType != -1 && inUnit->mUnitClass->mUnitType != f.mType)
        return false;
    if (f.mCategory != NULL && strcmp(EECPUnitsCategory(inUnit), f.mCategory) != 0)
        return false;
    if (f.mArmed >= 0 && (EECPUnitsArmed(inUnit) ? 1 : 0) != f.mArmed)
        return false;
    outKey = 0;
    if (f.mHasNear) {
        if (!EECPUnitsOnMap(inUnit))
            return false;
        long d2 = EECPUnitsD2(inUnit->GetTileX(), inUnit->GetTileY(), f.mX, f.mY);
        if (d2 > f.mR * f.mR)
            return false;
        outKey = d2;
    }
    return true;
}

static int LuaUnits(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsFilter f;
    EECPUnitsReadFilter(L, 1, f);
    s->mSorted.clear();
    long theTested = 0;
    for (EEUnit* theUnit = me->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        theTested++;
        EEComplexUnit* c = theUnit->mComplexUnit;
        if (c == NULL || !(theUnit->mHitPoints > 0) || !theUnit->mIsValid)
            continue;
        long long theKey;
        if (!EECPUnitsPasses(theUnit, f, theKey))
            continue;
        if (f.mIdle >= 0 && (EECPUnitsIdle(c) ? 1 : 0) != f.mIdle)
            continue;
        if (f.mBuilt >= 0 && (c->mIsBuilt ? 1 : 0) != f.mBuilt)
            continue;
        if (f.mHeld != NULL && (strcmp(f.mHeld, "lua") == 0) != (s->mHeld.count((long)theUnit->mUnitID) != 0))
            continue;
        if (f.mAction != NULL && strcmp(EECPUnitsAction(c), f.mAction) != 0)
            continue;
        s->mSorted.push_back(std::make_pair(theKey, (long)theUnit->mUnitID));
    }
    EECPUnitsCharge(L, kCostCall + theTested * kCostScan + (int64_t)s->mSorted.size() * kCostId);
    EECPUnitsPushSorted(L, s, f.mLimit);
    return 1;
}

static int LuaSeen(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsFilter f;
    EECPUnitsReadFilter(L, 1, f);
    s->mSorted.clear();
    long theTested = 0;
    for (unsigned long p = 1; p < gWorld.mNumPlayers && p < 64; p++) {
        EEPlayer* theOther = gWorld.mPlayers[p];
        if (theOther == NULL || theOther == me || (f.mPlayer >= 0 && (long)p != f.mPlayer))
            continue;
        if (f.mEnemy >= 0 && (me->IsAlly(theOther) ? 0 : 1) != f.mEnemy)
            continue;
        for (EEUnit* theUnit = theOther->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            theTested++;
            if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->mComplexUnit->mContainer != NULL
                || !EECPUnitsSeen(me, theUnit))
                continue;
            long long theKey;
            if (!EECPUnitsPasses(theUnit, f, theKey))
                continue;
            s->mSorted.push_back(std::make_pair(theKey, (long)theUnit->mUnitID));
        }
    }
    EECPUnitsCharge(L, kCostCall + theTested * kCostScan + (int64_t)s->mSorted.size() * kCostId);
    EECPUnitsPushSorted(L, s, f.mLimit);
    return 1;
}

static int LuaResources(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsFilter f;
    EECPUnitsReadFilter(L, 1, f);
    bool theRoom = EECPUnitsOptBool(L, 1, "has_room", false);
    s->mSorted.clear();
    long theTested = 0;
    EEPlayer* theGaia = gWorld.mPlayers[0];
    for (EEUnit* theUnit = theGaia != NULL ? theGaia->mFirstUnit : NULL; theUnit != NULL;
         theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        theTested++;
        if (!EECPUnitsIsResource(theUnit) || !EECPUnitsExplored(me, theUnit))
            continue;
        if (f.mKind >= 0 && (long)theUnit->mResourceType - kResourceFood != f.mKind)
            continue;
        long long theKey;
        if (!EECPUnitsPasses(theUnit, f, theKey))
            continue;
        if (theRoom && !EECPNextHasRoom(me, theUnit))
            continue;
        s->mSorted.push_back(std::make_pair(theKey, (long)theUnit->mUnitID));
    }
    EECPUnitsCharge(L, kCostCall + theTested * kCostScan + (int64_t)s->mSorted.size() * kCostId);
    EECPUnitsPushSorted(L, s, f.mLimit);
    return 1;
}

// one unit's record on the stack (the fields by what the player may know: AI_LUA_UNITS §2.3)
static void EECPUnitsPushRecord(lua_State* L, EEComputerPlayer* me, EECPUnitsState* s, EEUnit* inUnit)
{
    bool theOwn = inUnit->mPlayer == me;
    bool theAlly = !theOwn && inUnit->mPlayer != NULL && inUnit->mPlayer != gWorld.mPlayers[0] && me->IsAlly(inUnit->mPlayer);
    EEComplexUnit* c = inUnit->mComplexUnit;
    lua_createtable(L, 0, 24);
    EECPUnitsSetInt(L, "id", (long)inUnit->mUnitID);
    EECPUnitsSetStr(L, "type", EECPUnitsName(s, inUnit->mUnitClass->mUnitType));
    EECPUnitsSetStr(L, "category", EECPUnitsCategory(inUnit));
    EECPUnitsSetInt(L, "player", inUnit->mPlayer != NULL ? (long)inUnit->mPlayer->mPlayerID : 0);
    EECPUnitsSetInt(L, "x", inUnit->GetTileX());
    EECPUnitsSetInt(L, "y", inUnit->GetTileY());
    if (c != NULL) {
        const GETransformation& theAt = c->mPhysicalModel.GetLocation();
        EECPUnitsSetInt(L, "px", EECPUnitsCenti(theAt.GetX()));
        EECPUnitsSetInt(L, "py", EECPUnitsCenti(theAt.GetY()));
    }
    if (EECPUnitsIsResource(inUnit)) {
        static const char* const kKinds[5] = {"food", "wood", "stone", "gold", "iron"};
        EECPUnitsSetStr(L, "kind", kKinds[(long)inUnit->mResourceType - kResourceFood]);
        EECPUnitsSetInt(L, "amount", inUnit->mResourceAmount);
        EECPUnitsSetInt(L, "workers", EECPNextAssigned(me, inUnit));
        EECPUnitsSetBool(L, "has_room", EECPNextHasRoom(me, inUnit));
        return;
    }
    EECPUnitsSetInt(L, "hp", inUnit->mHitPoints);
    if (inUnit->mStats != NULL) {
        EECPUnitsSetInt(L, "hp_max", inUnit->mStats->mMaxHitPoints);
        EECPUnitsSetInt(L, "attack", UMFloatToLong(inUnit->mStats->mAttack));
        EECPUnitsSetInt(L, "range", EECPUnitsCenti(inUnit->mStats->mRange));
        EECPUnitsSetInt(L, "sight", EECPUnitsCenti(inUnit->mStats->mLineOfSight));
        EECPUnitsSetInt(L, "speed", EECPUnitsCenti(inUnit->mStats->mMaxVelocity));
    }
    EECPUnitsSetBool(L, "armed", EECPUnitsArmed(inUnit));
    if (c == NULL)
        return;
    EECPUnitsSetBool(L, "built", c->mIsBuilt);
    const char* theAction = EECPUnitsAction(c);
    if (!theOwn && !theAlly) {
        // what a human sees of another player's unit: moving, attacking or standing
        if (strcmp(theAction, "move") != 0 && strcmp(theAction, "attack") != 0 && strcmp(theAction, "idle") != 0)
            theAction = "busy";
        EECPUnitsSetStr(L, "action", theAction);
        return;
    }
    EECPUnitsSetStr(L, "action", theAction);
    EEUnitGoal* g = c->GetGoalState();
    if (g != NULL) {
        GE3DPoint thePlace;
        g->GetGoalLocation(thePlace);
        EECPUnitsSetInt(L, "tx", UMFloor(thePlace.GetX()));
        EECPUnitsSetInt(L, "ty", UMFloor(thePlace.GetY()));
    }
    if (c->mContainer != NULL)
        EECPUnitsSetInt(L, "inside", (long)c->mContainer->mUnitID);
    EECPUnitsSetInt(L, "cargo", (long)c->GetNumCargoUnits());
    if (c->mUnitClass->IsBuilding()) {
        EECPUnitsSetInt(L, "queue", (long)c->mBuildQueue.size() + (c->mCurrentResearch != 100000 ? 1 : 0));
        if (c->mCurrentResearch != 100000)
            EECPUnitsSetInt(L, "producing", c->mCurrentResearch);
    }
    long theStance = -1;
    for (unsigned long k = 0; k < EEUnitBehavior::sTable.size(); k++)
        if (EEUnitBehavior::sTable[k] == c->mBehavior)
            theStance = (long)k;
    EECPUnitsSetInt(L, "stance", theStance);
    if (theOwn) {
        EECPUnitsSetStr(L, "held", s->mHeld.count((long)inUnit->mUnitID) ? "lua" : "engine");
        if (c->mLastAttacker != NULL && c->mLastAttacker->mHitPoints > 0 && EECPUnitsSeen(me, c->mLastAttacker))
            EECPUnitsSetInt(L, "attacker", (long)c->mLastAttacker->mUnitID);
    }
}

// a unit the player may know of -> the unit, else NULL (own, allied, seen now; a resource on an explored tile)
static EEUnit* EECPUnitsKnown(EEComputerPlayer* me, long inID)
{
    EEUnit* theUnit = EECPUnitsAlive(EECPUnitsForward(me->mUnitsState, inID));
    if (theUnit == NULL) {
        theUnit = gWorld.GetUnit((unsigned long)inID);   // a resource is alive with hit points 0 (a worked mine)
        if (theUnit == NULL || !theUnit->mIsValid || !EECPUnitsIsResource(theUnit))
            return NULL;
    }
    if (EECPUnitsIsResource(theUnit))
        return EECPUnitsExplored(me, theUnit) ? theUnit : NULL;
    if (theUnit->mComplexUnit != NULL && theUnit->mComplexUnit->mContainer != NULL && theUnit->mPlayer != me)
        return NULL;
    return EECPUnitsSeen(me, theUnit) ? theUnit : NULL;
}

static int LuaUnit(lua_State* L)
{
    EECPUnitsCharge(L, kCostRecord);
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    if (!lua_isinteger(L, 1)) {
        lua_pushnil(L);
        return 1;
    }
    EEUnit* theUnit = EECPUnitsKnown(me, (long)lua_tointeger(L, 1));
    if (theUnit == NULL)
        lua_pushnil(L);
    else
        EECPUnitsPushRecord(L, me, s, theUnit);
    return 1;
}

// ai.get(ids, fields) -> {field = {values...}}; a unit not known: nil in each (the record's own fields)
static int LuaGet(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsReadIds(L, 1, s->mIds);
    long theFields = lua_type(L, 2) == LUA_TTABLE ? (long)lua_rawlen(L, 2) : 0;
    EECPUnitsCharge(L, kCostCall + (int64_t)s->mIds.size() * (theFields * kCostField + kCostId));
    lua_newtable(L);                      // the result (index 3)
    int theResult = lua_gettop(L);
    for (long f = 1; f <= theFields; f++) {
        lua_rawgeti(L, 2, f);
        lua_newtable(L);
        lua_settable(L, theResult);
    }
    for (unsigned long k = 0; k < s->mIds.size(); k++) {
        EEUnit* theUnit = EECPUnitsKnown(me, s->mIds[k]);
        if (theUnit == NULL)
            continue;
        EECPUnitsPushRecord(L, me, s, theUnit);
        int theRecord = lua_gettop(L);
        for (long f = 1; f <= theFields; f++) {
            lua_rawgeti(L, 2, f);         // the field's name
            lua_pushvalue(L, -1);
            lua_gettable(L, theResult);   // its column
            lua_insert(L, -2);
            lua_gettable(L, theRecord);   // the value
            lua_rawseti(L, -2, (lua_Integer)(k + 1));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    return 1;
}

static int LuaMemory(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsFilter f;
    EECPUnitsReadFilter(L, 1, f);
    long theSince = 0;
    EECPUnitsOptInt(L, 1, "since", theSince);
    int theBuildings = -1;
    if (lua_type(L, 1) == LUA_TTABLE) {
        lua_getfield(L, 1, "buildings");
        if (!lua_isnil(L, -1))
            theBuildings = lua_toboolean(L, -1) ? 1 : 0;
        lua_pop(L, 1);
    }
    s->mSorted.clear();
    for (std::map<long, Remembered>::iterator it = s->mMemory.begin(); it != s->mMemory.end(); ++it) {
        const Remembered& r = it->second;
        if ((f.mPlayer >= 0 && r.mPlayer != f.mPlayer) || (f.mType != -1 && r.mType != f.mType) || r.mSeen < theSince
            || (theBuildings >= 0 && r.mBuilding != theBuildings))
            continue;
        long long theKey = 0;
        if (f.mHasNear) {
            long d2 = EECPUnitsD2(r.mX, r.mY, f.mX, f.mY);
            if (d2 > f.mR * f.mR)
                continue;
            theKey = d2;
        }
        s->mSorted.push_back(std::make_pair(theKey, it->first));
    }
    std::sort(s->mSorted.begin(), s->mSorted.end());
    long n = (long)s->mSorted.size();
    if (f.mLimit >= 0 && n > f.mLimit)
        n = f.mLimit;
    EECPUnitsCharge(L, kCostCall + (int64_t)s->mMemory.size() * kCostScan + n * kCostRecord / 3);
    lua_createtable(L, (int)n, 0);
    for (long k = 0; k < n; k++) {
        const Remembered& r = s->mMemory[s->mSorted[k].second];
        lua_createtable(L, 0, 8);
        EECPUnitsSetInt(L, "id", s->mSorted[k].second);
        EECPUnitsSetInt(L, "player", r.mPlayer);
        EECPUnitsSetStr(L, "type", EECPUnitsName(s, r.mType));
        EECPUnitsSetInt(L, "x", r.mX);
        EECPUnitsSetInt(L, "y", r.mY);
        EECPUnitsSetInt(L, "hp", r.mHP);
        EECPUnitsSetInt(L, "seen", r.mSeen);
        EECPUnitsSetBool(L, "building", r.mBuilding != 0);
        lua_rawseti(L, -2, (lua_Integer)(k + 1));
    }
    return 1;
}

static int LuaTile(lua_State* L)
{
    EECPUnitsCharge(L, kCostTile);
    EEComputerPlayer* me = EECPUnitsMe(L);
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2) || gWorld.mMap == NULL) {
        lua_pushnil(L);
        return 1;
    }
    long x = (long)lua_tointeger(L, 1), y = (long)lua_tointeger(L, 2);
    EETile* theTile = x >= 0 && y >= 0 ? static_cast<EETile*>(gWorld.mMap->GetTile((unsigned long)x, (unsigned long)y)) : NULL;
    if (theTile == NULL) {
        lua_pushnil(L);
        return 1;
    }
    bool theExplored = theTile->IsVisibleTo(me);
    lua_createtable(L, 0, 6);
    EECPUnitsSetBool(L, "explored", theExplored);
    EECPUnitsSetBool(L, "visible", theTile->IsSeenBy(me));
    if (theExplored) {
        U2DPoint<long> theAt(x, y);
        EECPUnitsSetInt(L, "continent", (long)gContinentManager.GetContinent(theAt, kAIPathLandMass));
        EECPUnitsSetInt(L, "sea", (long)gContinentManager.GetContinent(theAt, kAIPathWaterBody));
        EECPUnitsSetInt(L, "terrain", theTile->GetTerrainType());
    }
    return 1;
}

// ai.class(type) -> static data from the class and the player's improvements (what the unit's panel shows)
static int LuaClass(lua_State* L)
{
    EECPUnitsCharge(L, kCostRecord);
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    long theType = EECPUnitsClassArg(L, 1);
    if (theType < 0 || (unsigned long)theType >= EEUnitClassCount() || gUnitClassTable[theType] == NULL) {
        lua_pushnil(L);
        return 1;
    }
    EEUnitClass* c = gUnitClassTable[theType];
    lua_createtable(L, 0, 12);
    EECPUnitsSetStr(L, "type", EECPUnitsName(s, theType));
    EECPUnitsSetBool(L, "building", c->IsBuilding());
    EECPUnitsSetInt(L, "size", c->GetTileSize());
    EECPUnitsSetInt(L, "garrison", c->mGarrisonCapacity);
    EECPUnitsSetInt(L, "transport", c->mTransportCapacity);
    EECPUnitsSetBool(L, "can_garrison", c->mCanGarrison);
    if ((unsigned long)theType < me->mUnitClassUpgrades.size() && me->mUnitClassUpgrades[theType] != NULL) {
        EEUnitClassUpgrade* u = me->mUnitClassUpgrades[theType];
        EECPUnitsSetInt(L, "hp", u->mMaxHitPoints);
        EECPUnitsSetInt(L, "attack", UMFloatToLong(u->mAttack));
        EECPUnitsSetInt(L, "range", EECPUnitsCenti(u->mRange));
        EECPUnitsSetInt(L, "sight", EECPUnitsCenti(u->mLineOfSight));
        EECPUnitsSetInt(L, "speed", EECPUnitsCenti(u->mMaxVelocity));
        EECPUnitsSetInt(L, "reload", u->mReloadTime);
    }
    return 1;
}

static int LuaEvents(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    long n = (long)s->mEvents.size();
    EECPUnitsCharge(L, kCostCall + n * kCostId * 3);
    lua_createtable(L, (int)n, 0);
    for (long k = 0; k < n; k++) {
        const UnitsEvent& e = s->mEvents[k];
        lua_createtable(L, 0, 6);
        EECPUnitsSetStr(L, "kind", kEventNames[e.mKind]);
        EECPUnitsSetInt(L, "unit", e.mUnit);
        EECPUnitsSetInt(L, "time", e.mTime);
        switch (e.mKind) {
        case kEvAttacked:
            if (e.mOther != 0)
                EECPUnitsSetInt(L, "attacker", e.mOther);
            EECPUnitsSetInt(L, "player", e.mType);
            EECPUnitsSetInt(L, "damage", e.mAmount);
            break;
        case kEvDied: case kEvConverted:
            if (e.mOther != 0)
                EECPUnitsSetInt(L, "killer", e.mOther);
            if (e.mType >= 0)
                EECPUnitsSetStr(L, "type", EECPUnitsName(s, e.mType));
            break;
        case kEvSwapped:
            EECPUnitsSetInt(L, "new", e.mOther);
            EECPUnitsSetStr(L, "type", EECPUnitsName(s, e.mType));
            break;
        default:
            if (e.mType >= 0)
                EECPUnitsSetStr(L, "type", EECPUnitsName(s, e.mType));
            break;
        }
        lua_rawseti(L, -2, (lua_Integer)(k + 1));
    }
    s->mEvents.clear();
    s->mAttackedAt.clear();
    if (s->mDropped > 0) {
        EECPTraceScope theScope("next");
        EE_CP_TRACE(me, "next.events", "dropped=%ld (at most %ld between deliveries)", s->mDropped, kMaxEvents);
        s->mDropped = 0;
    }
    return 1;
}

// ---------------------------------------------------------------------------------------------------------------
// Helpers

static int LuaCanPlace(lua_State* L)
{
    EECPUnitsCharge(L, kCostCommand);
    EEComputerPlayer* me = EECPUnitsMe(L);
    long theType = EECPUnitsClassArg(L, 1);
    if (theType < 0 || !lua_isinteger(L, 2) || !lua_isinteger(L, 3)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, EECanPlaceOnTile(me, theType, (long)lua_tointeger(L, 2), (long)lua_tointeger(L, 3), false, 1) ? 1 : 0);
    return 1;
}

// ai.reach(x1, y1, x2, y2) -> whether both tiles are on one land mass
static int LuaReach(lua_State* L)
{
    EECPUnitsCharge(L, kCostTile * 2);
    for (int i = 1; i <= 4; i++)
        if (!lua_isinteger(L, i)) {
            lua_pushboolean(L, 0);
            return 1;
        }
    U2DPoint<long> a((long)lua_tointeger(L, 1), (long)lua_tointeger(L, 2));
    U2DPoint<long> b((long)lua_tointeger(L, 3), (long)lua_tointeger(L, 4));
    unsigned short ca = gContinentManager.GetContinent(a, kAIPathLandMass);
    lua_pushboolean(L, ca != 0 && ca == gContinentManager.GetContinent(b, kAIPathLandMass) ? 1 : 0);
    return 1;
}

// ai.strength(ids) -> the sum of hit points x attack of the units the player may know of
static int LuaStrength(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsReadIds(L, 1, s->mIds);
    EECPUnitsCharge(L, kCostCall + (int64_t)s->mIds.size() * kCostField);
    long long theSum = 0;
    for (unsigned long k = 0; k < s->mIds.size(); k++) {
        EEUnit* theUnit = EECPUnitsKnown(me, s->mIds[k]);
        if (theUnit != NULL)
            theSum += EECPUnitsStrength(theUnit);
    }
    lua_pushinteger(L, (lua_Integer)theSum);
    return 1;
}

// ---------------------------------------------------------------------------------------------------------------
// Commands

enum CmdReason { kOK, kBadArgument, kNotOwn, kNotSeen, kUnexplored, kCannotPlace, kCannotAfford, kCannotDo, kLimit, kFull };
const char* const kReasonNames[] = {"none", "bad_argument", "not_own", "not_seen", "unexplored", "cannot_place",
                                    "cannot_afford", "cannot_do", "limit", "full"};

// the result: code, reason, n, id
static int EECPUnitsResult(lua_State* L, long inReason, long inCount, long inID)
{
    lua_pushstring(L, inReason == kOK ? "ok" : "refused");
    lua_pushstring(L, kReasonNames[inReason]);
    lua_pushinteger(L, (lua_Integer)inCount);
    if (inID != 0)
        lua_pushinteger(L, (lua_Integer)inID);
    else
        lua_pushnil(L);
    return 4;
}

// the per-step limits (AI_LUA_UNITS §3.4)
static bool EECPUnitsWithinLimits(EECPUnitsState* s, long inUnits)
{
    long theNow = (long)gPhysics->mGameTime;
    if (s->mStepStamp != theNow) {
        s->mStepStamp = theNow;
        s->mOrdersThisStep = 0;
        s->mUnitOrdersThisStep = 0;
    }
    if (s->mOrdersThisStep + 1 > 400 || s->mUnitOrdersThisStep + inUnits > 4000)
        return false;
    s->mOrdersThisStep++;
    s->mUnitOrdersThisStep += inUnits;
    return true;
}

// the ids at inIndex -> s->mIds2: the player's own living units on the map (or inside, with inInside) that pass
// inTest; the first reason one failed in outReason
static void EECPUnitsOwnUnits(EEComputerPlayer* me, EECPUnitsState* s, bool inInside, bool (*inTest)(EEComplexUnit*),
    long& outReason)
{
    s->mIds2.clear();
    outReason = kOK;
    for (unsigned long k = 0; k < s->mIds.size(); k++) {
        EEComplexUnit* c = EECPUnitsOwn(me, s->mIds[k]);
        if (c == NULL) {
            if (outReason == kOK)
                outReason = kNotOwn;
            continue;
        }
        if (c->mIsDisabled || (!inInside && (c->mContainer != NULL || c->GetTileX() == -10000))
            || (inTest != NULL && !inTest(c))) {
            if (outReason == kOK)
                outReason = kCannotDo;
            continue;
        }
        if (std::find(s->mIds2.begin(), s->mIds2.end(), (long)c->mUnitID) == s->mIds2.end())
            s->mIds2.push_back((long)c->mUnitID);
    }
}

// runs a human's message for the player on the units of s->mIds2 (the selection), and takes them
static void EECPUnitsRun(EEComputerPlayer* me, EECPUnitsState* s, EEMessage& inMessage, bool inTake)
{
    EESelectedGroup* theSelection = me->mActiveGroup;
    if (theSelection == NULL)
        return;
    theSelection->Clear();
    for (unsigned long k = 0; k < s->mIds2.size(); k++) {
        EEComplexUnit* c = EECPUnitsOwn(me, s->mIds2[k]);
        if (c != NULL) {
            theSelection->Add(c);
            if (inTake)
                EECPUnitsTake(me, s, c);
        }
    }
    unsigned char theSlot = me->mSlot;
    me->mSlot = kBorrowedSlot;
    if (gWorld.GetPlayerBySlot(kBorrowedSlot) == me) {
        inMessage.SetOriginatingSlot(kBorrowedSlot);
        inMessage.Execute();
    }
    me->mSlot = theSlot;
    theSelection->Clear();
}

static void EECPUnitsTraceCmd(EEComputerPlayer* me, EECPUnitsState* s, const char* inVerb, long inReason, long inCount,
    long inArg1, long inArg2)
{
    if (!gCPTraceOn)
        return;
    unsigned long theHash = 2166136261u;
    for (unsigned long k = 0; k < s->mIds2.size(); k++)
        theHash = (theHash ^ (unsigned long)s->mIds2[k]) * 16777619u;
    EECPTraceScope theScope("next");
    EECPTraceOut(me, "next.cmd", "%s n=%ld a=%ld b=%ld ids=%08lx first=%ld result=%s reason=%s", inVerb, inCount, inArg1,
        inArg2, theHash & 0xffffffffu, s->mIds2.empty() ? 0L : s->mIds2[0], inReason == kOK ? "ok" : "refused",
        kReasonNames[inReason]);
}

static bool EECPUnitsMobile(EEComplexUnit* c)
{
    return !c->mUnitClass->IsBuilding() && c->mStats != NULL && c->mStats->mMaxVelocity > 0.0f;
}

static bool EECPUnitsCanAttack(EEComplexUnit* c)
{
    return EECPUnitsMobile(c) || (c->mStats != NULL && c->mStats->mAttack != 0.0f);
}

static bool EECPUnitsCitizen(EEComplexUnit* c)
{
    long k = c->mUnitClass->mCategory;
    return k == kUnitCategoryLandCitizen || k == kUnitCategoryNavalCitizen;
}

static bool EECPUnitsBuilding(EEComplexUnit* c)
{
    return c->mUnitClass->IsBuilding() && c->mIsBuilt;
}

// the shared start of a unit command: the ids at 1, the charge, the own units -> false with the result pushed
static bool EECPUnitsBegin(lua_State* L, EEComputerPlayer* me, EECPUnitsState* s, bool inInside,
    bool (*inTest)(EEComplexUnit*), const char* inVerb, int& outResults)
{
    EECPUnitsReadIds(L, 1, s->mIds);
    EECPUnitsCharge(L, kCostCommand + (int64_t)s->mIds.size() * kCostOrdered);
    long theReason;
    EECPUnitsOwnUnits(me, s, inInside, inTest, theReason);
    if (s->mIds2.empty()) {
        EECPUnitsTraceCmd(me, s, inVerb, theReason == kOK ? kBadArgument : theReason, 0, 0, 0);
        outResults = EECPUnitsResult(L, theReason == kOK ? kBadArgument : theReason, 0, 0);
        return false;
    }
    if (!EECPUnitsWithinLimits(s, (long)s->mIds2.size())) {
        EECPUnitsTraceCmd(me, s, inVerb, kLimit, 0, 0, 0);
        outResults = EECPUnitsResult(L, kLimit, 0, 0);
        return false;
    }
    return true;
}

static bool EECPUnitsTileOK(long x, long y)
{
    return gWorld.mMap != NULL && x >= 0 && y >= 0 && gWorld.mMap->GetTile((unsigned long)x, (unsigned long)y) != NULL;
}

// ai.move(ids, x, y, {queue, forced, formation})
static int LuaMove(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsMobile, "move", r))
        return r;
    if (!lua_isinteger(L, 2) || !lua_isinteger(L, 3) || !EECPUnitsTileOK((long)lua_tointeger(L, 2), (long)lua_tointeger(L, 3)))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    long x = (long)lua_tointeger(L, 2), y = (long)lua_tointeger(L, 3);
    bool theQueue = EECPUnitsOptBool(L, 4, "queue", false);
    bool theForced = EECPUnitsOptBool(L, 4, "forced", false);
    bool theFormation = EECPUnitsOptBool(L, 4, "formation", false);
    EEMPathTo theMessage((float)x + 0.5f, (float)y + 0.5f, 0.0f, theForced, false, theQueue, theFormation);
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "move", kOK, (long)s->mIds2.size(), x, y);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.attack(ids, target, {queue}): the target seen now, or a building the player remembers
static int LuaAttack(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsCanAttack, "attack", r))
        return r;
    long theID = lua_isinteger(L, 2) ? EECPUnitsForward(s, (long)lua_tointeger(L, 2)) : 0;
    EEUnit* theTarget = EECPUnitsAlive(theID);
    if (theTarget == NULL || theTarget->mPlayer == me || (theTarget->mPlayer != NULL && theTarget->mPlayer != gWorld.mPlayers[0]
        && me->IsAlly(theTarget->mPlayer)))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    std::map<long, Remembered>::iterator it = s->mMemory.find(theID);
    bool theKnown = EECPUnitsSeen(me, theTarget) || (it != s->mMemory.end() && it->second.mBuilding);
    if (!theKnown) {
        EECPUnitsTraceCmd(me, s, "attack", kNotSeen, 0, theID, 0);
        return EECPUnitsResult(L, kNotSeen, 0, 0);
    }
    EEMAttackUnit theMessage(theID, EECPUnitsOptBool(L, 3, "queue", false), false);
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "attack", kOK, (long)s->mIds2.size(), theID, 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.attack_ground(ids, x, y)
static int LuaAttackGround(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsCanAttack, "attack_ground", r))
        return r;
    if (!lua_isinteger(L, 2) || !lua_isinteger(L, 3) || !EECPUnitsTileOK((long)lua_tointeger(L, 2), (long)lua_tointeger(L, 3)))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    long x = (long)lua_tointeger(L, 2), y = (long)lua_tointeger(L, 3);
    EEMAttackGround theMessage((float)x + 0.5f, (float)y + 0.5f);
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "attack_ground", kOK, (long)s->mIds2.size(), x, y);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.gather(ids, resource, {queue}): a resource on an explored tile with some left
static int LuaGather(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsCitizen, "gather", r))
        return r;
    long theID = lua_isinteger(L, 2) ? (long)lua_tointeger(L, 2) : 0;
    EEUnit* theTarget = theID > 0 ? gWorld.GetUnit((unsigned long)theID) : NULL;
    if (theTarget == NULL || !theTarget->mIsValid || theTarget->mResourceAmount <= 0)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    if (!EECPUnitsExplored(me, theTarget)) {
        EECPUnitsTraceCmd(me, s, "gather", kUnexplored, 0, theID, 0);
        return EECPUnitsResult(L, kUnexplored, 0, 0);
    }
    EEMGatherResource theMessage(theID, EECPUnitsOptBool(L, 3, "queue", false));
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "gather", kOK, (long)s->mIds2.size(), theID, 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// the newest own building of a class whose tile is (x, y) (the foundation a build order just made), 0 if none
static long EECPUnitsNewBuilding(EEComputerPlayer* me, long inType, long inX, long inY, long inAbove)
{
    long theBest = 0;
    for (EEUnit* theUnit = me->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (theUnit->mUnitClass->mUnitType == inType && theUnit->GetTileX() == inX && theUnit->GetTileY() == inY
            && (long)theUnit->mUnitID > inAbove && (long)theUnit->mUnitID > theBest)
            theBest = (long)theUnit->mUnitID;
    for (unsigned long k = 0; theBest == 0; k++) {
        RTSUnit* thePending = me->GetPendingUnit(k);
        if (thePending == NULL)
            break;
        EEUnit* theUnit = static_cast<EEUnit*>(thePending);
        if (theUnit->mUnitClass != NULL && theUnit->mUnitClass->mUnitType == inType && theUnit->GetTileX() == inX
            && theUnit->GetTileY() == inY)
            theBest = (long)theUnit->mUnitID;
    }
    return theBest;
}

// ai.build(ids, type, x, y, {queue}) -> ..., the foundation's id: the citizens build it (it is the script's)
static int LuaBuild(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsCitizen, "build", r))
        return r;
    long theType = EECPUnitsClassArg(L, 2);
    if (theType < 0 || (unsigned long)theType >= EEUnitClassCount() || gUnitClassTable[theType] == NULL
        || !gUnitClassTable[theType]->IsBuilding() || !lua_isinteger(L, 3) || !lua_isinteger(L, 4))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    long x = (long)lua_tointeger(L, 3), y = (long)lua_tointeger(L, 4);
    long theTech = gUnitClassTable[theType]->mTechnology;
    if (!me->EEPlayer::CanProduce(theTech, 0x20, 1, true, 3, NULL)) {
        EECPUnitsTraceCmd(me, s, "build", kCannotAfford, 0, theType, x);
        return EECPUnitsResult(L, kCannotAfford, 0, 0);
    }
    if (!EECanPlaceOnTile(me, theType, x, y, false, 1)) {
        EECPUnitsTraceCmd(me, s, "build", kCannotPlace, 0, theType, x);
        return EECPUnitsResult(L, kCannotPlace, 0, 0);
    }
    long theBefore = 0;
    for (EEUnit* theUnit = me->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if ((long)theUnit->mUnitID > theBefore)
            theBefore = (long)theUnit->mUnitID;
    EEMBuildUnit theMessage(GE2DPoint((float)x, (float)y), theTech, EECPUnitsOptBool(L, 5, "queue", false) ? 1 : 0);
    EECPUnitsRun(me, s, theMessage, true);
    long theNew = EECPUnitsNewBuilding(me, theType, x, y, theBefore);
    if (theNew != 0) {
        EEComplexUnit* c = EECPUnitsOwn(me, theNew);
        if (c != NULL)
            EECPUnitsTake(me, s, c);
    }
    EECPUnitsTraceCmd(me, s, "build", theNew != 0 ? kOK : kCannotPlace, (long)s->mIds2.size(), theType, theNew);
    return EECPUnitsResult(L, theNew != 0 ? kOK : kCannotPlace, theNew != 0 ? (long)s->mIds2.size() : 0, theNew);
}

// ai.repair(ids, target): an own or allied unit or building
static int LuaRepair(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsCitizen, "repair", r))
        return r;
    long theID = lua_isinteger(L, 2) ? EECPUnitsForward(s, (long)lua_tointeger(L, 2)) : 0;
    EEUnit* theTarget = EECPUnitsAlive(theID);
    if (theTarget == NULL || theTarget->mPlayer == NULL || !me->IsAlly(theTarget->mPlayer))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    EEMRepairUnit theMessage(theID, EECPUnitsOptBool(L, 3, "queue", false));
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "repair", kOK, (long)s->mIds2.size(), theID, 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.garrison(ids, building) / ai.load(ids, transport)
static int EECPUnitsLoadInto(lua_State* L, bool inGarrison)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    const char* theVerb = inGarrison ? "garrison" : "load";
    if (!EECPUnitsBegin(L, me, s, false, NULL, theVerb, r))
        return r;
    long theID = lua_isinteger(L, 2) ? EECPUnitsForward(s, (long)lua_tointeger(L, 2)) : 0;
    EEComplexUnit* theInto = EECPUnitsOwn(me, theID);
    if (theInto == NULL || (inGarrison ? theInto->mUnitClass->mGarrisonCapacity <= 0 : !theInto->mUnitClass->IsTransport()))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    s->mIds2.erase(std::remove(s->mIds2.begin(), s->mIds2.end(), theID), s->mIds2.end());
    long theBefore = (long)theInto->GetNumCargoUnits();
    if (inGarrison) {
        EEMLoadGarrison theMessage(theID, -1, 0);
        EECPUnitsRun(me, s, theMessage, true);
    } else {
        EEMLoadTransport theMessage(theID, -1, 0);
        EECPUnitsRun(me, s, theMessage, true);
    }
    EECPUnitsTake(me, s, theInto);
    EECPUnitsTraceCmd(me, s, theVerb, kOK, (long)s->mIds2.size(), theID, theBefore);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

static int LuaGarrison(lua_State* L)
{
    return EECPUnitsLoadInto(L, true);
}

static int LuaLoad(lua_State* L)
{
    return EECPUnitsLoadInto(L, false);
}

// ai.unload(containers, x, y)
static int LuaUnload(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, NULL, "unload", r))
        return r;
    if (!lua_isinteger(L, 2) || !lua_isinteger(L, 3) || !EECPUnitsTileOK((long)lua_tointeger(L, 2), (long)lua_tointeger(L, 3)))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    long x = (long)lua_tointeger(L, 2), y = (long)lua_tointeger(L, 3);
    EEMUnloadTransport theMessage(s->mIds2[0], x, y);
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "unload", kOK, (long)s->mIds2.size(), x, y);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.train(buildings, type, n) / ai.research(buildings, tech): the queue of the least busy of the buildings
static int EECPUnitsQueue(lua_State* L, bool inTrain)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    const char* theVerb = inTrain ? "train" : "research";
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsBuilding, theVerb, r))
        return r;
    long theTech = -1;
    if (inTrain) {
        long theType = EECPUnitsClassArg(L, 2);
        if (theType >= 0 && (unsigned long)theType < EEUnitClassCount() && gUnitClassTable[theType] != NULL)
            theTech = gUnitClassTable[theType]->mTechnology;
    } else if (lua_type(L, 2) == LUA_TSTRING) {
        theTech = EECPNextTechByName(lua_tostring(L, 2));
    } else if (lua_isinteger(L, 2))
        theTech = (long)lua_tointeger(L, 2);
    long n = lua_isinteger(L, 3) ? (long)lua_tointeger(L, 3) : 1;
    if (theTech < 0 || n < 1 || n > 5)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    // one building of the same type at a time: the message queues at the selected buildings of the clicked one's type
    EEComplexUnit* theFirst = EECPUnitsOwn(me, s->mIds2[0]);
    long theBefore = 0;
    for (unsigned long k = 0; k < s->mIds2.size(); k++) {
        EEComplexUnit* c = EECPUnitsOwn(me, s->mIds2[k]);
        if (c != NULL)
            theBefore += (long)c->mBuildQueue.size() + (c->mCurrentResearch != 100000 ? 1 : 0);
    }
    for (long k = 0; k < n; k++) {
        EEMResearchTechnology theMessage((long)theFirst->mUnitID, theTech, false);
        EECPUnitsRun(me, s, theMessage, false);
    }
    long theAfter = 0;
    for (unsigned long k = 0; k < s->mIds2.size(); k++) {
        EEComplexUnit* c = EECPUnitsOwn(me, s->mIds2[k]);
        if (c != NULL)
            theAfter += (long)c->mBuildQueue.size() + (c->mCurrentResearch != 100000 ? 1 : 0);
    }
    long theQueued = theAfter - theBefore;
    long theReason = theQueued > 0 ? kOK : kCannotAfford;
    EECPUnitsTraceCmd(me, s, theVerb, theReason, theQueued, theTech, n);
    return EECPUnitsResult(L, theReason, theQueued > 0 ? theQueued : 0, 0);
}

static int LuaTrain(lua_State* L)
{
    return EECPUnitsQueue(L, true);
}

static int LuaResearch(lua_State* L)
{
    return EECPUnitsQueue(L, false);
}

// ai.stance(ids, n): the behaviour table's index (the unit panel's buttons)
static int LuaStance(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, NULL, "stance", r))
        return r;
    if (!lua_isinteger(L, 2) || lua_tointeger(L, 2) < 0 || (unsigned long)lua_tointeger(L, 2) >= EEUnitBehavior::sTable.size()
        || EEUnitBehavior::sTable[(unsigned long)lua_tointeger(L, 2)] == NULL)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    EEMSetUnitBehavior theMessage((long)lua_tointeger(L, 2));
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "stance", kOK, (long)s->mIds2.size(), (long)lua_tointeger(L, 2), 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

static int LuaStop(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, NULL, "stop", r))
        return r;
    EEMStopActiveGroup theMessage;
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "stop", kOK, (long)s->mIds2.size(), 0, 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

static int LuaExplore(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsMobile, "explore", r))
        return r;
    EEMExplore theMessage;
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "explore", kOK, (long)s->mIds2.size(), 0, 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

static int LuaDelete(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, true, NULL, "delete", r))
        return r;
    EEMKillUnit theMessage(1);
    EECPUnitsRun(me, s, theMessage, false);
    EECPUnitsTraceCmd(me, s, "delete", kOK, (long)s->mIds2.size(), 0, 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.take(ids) / ai.release(ids): the held set
static int EECPUnitsHold(lua_State* L, bool inTake)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsReadIds(L, 1, s->mIds);
    EECPUnitsCharge(L, kCostCall + (int64_t)s->mIds.size() * kCostId);
    long n = 0;
    for (unsigned long k = 0; k < s->mIds.size(); k++) {
        EEComplexUnit* c = EECPUnitsOwn(me, s->mIds[k]);
        if (c == NULL)
            continue;
        if (inTake)
            EECPUnitsTake(me, s, c);
        else
            EECPUnitsRelease(me, s, c);
        n++;
    }
    lua_pushinteger(L, (lua_Integer)n);
    return 1;
}

static int LuaTake(lua_State* L)
{
    return EECPUnitsHold(L, true);
}

static int LuaRelease(lua_State* L)
{
    return EECPUnitsHold(L, false);
}

// ai.reflex{naval_flee = false, danger = false, memory_ms = 180000}
static int LuaReflex(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    s->mReflexNavalFlee = EECPUnitsOptBool(L, 1, "naval_flee", s->mReflexNavalFlee);
    s->mReflexDanger = EECPUnitsOptBool(L, 1, "danger", s->mReflexDanger);
    long theMs;
    if (EECPUnitsOptInt(L, 1, "memory_ms", theMs) && theMs >= 0 && theMs <= 3600000)
        s->mMemoryMs = theMs;
    return 0;
}

static bool EECPUnitsTestOn()
{
    static int sOn = -1;
    if (sOn < 0) {
        const char* theValue = getenv("EE_AI_TEST_SPAWN");
        sOn = theValue != NULL && theValue[0] == '1' ? 1 : 0;
    }
    return sOn == 1;
}

// TEST ONLY (EE_AI_TEST_SPAWN=1): ai.test_town(player) -> x, y of the player's first town centre or capitol (fog ignored): the
// micro test scripts place their fights with it. Off: nil
static int LuaTestTown(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    if (!EECPUnitsTestOn() || !lua_isinteger(L, 1) || lua_tointeger(L, 1) < 0
        || (unsigned long)lua_tointeger(L, 1) >= gWorld.mNumPlayers || gWorld.mPlayers[lua_tointeger(L, 1)] == NULL)
        return 0;
    for (EEUnit* theUnit = gWorld.mPlayers[lua_tointeger(L, 1)]->mFirstUnit; theUnit != NULL;
         theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if ((theUnit->mUnitClass->mUnitType == kUnitTypeTownCenter || theUnit->mUnitClass->mUnitType == kUnitTypeCapitol)
            && theUnit->mHitPoints > 0) {
            long theHalf = theUnit->mUnitClass->GetTileSize() / 2;
            lua_pushinteger(L, (lua_Integer)(theUnit->GetTileX() + theHalf));
            lua_pushinteger(L, (lua_Integer)(theUnit->GetTileY() + theHalf));
            return 2;
        }
    return 0;
}

// TEST ONLY (EE_AI_TEST_SPAWN=1): ai.test_spawn(type, x, y, player) -> true: the cheat's unit (EEMHackANewUnit) is
// made (it is the player's from the next tick: find it with ai.units). The micro test scripts set their armies up
// with it. Off: false
static int LuaTestSpawn(lua_State* L)
{
    bool sOn = EECPUnitsTestOn();
    EECPUnitsCharge(L, kCostCommand);
    EEComputerPlayer* me = EECPUnitsMe(L);
    long theType = EECPUnitsClassArg(L, 1);
    if (!sOn || theType < 0 || (unsigned long)theType >= EEUnitClassCount() || !lua_isinteger(L, 2) || !lua_isinteger(L, 3)
        || !EECPUnitsTileOK((long)lua_tointeger(L, 2), (long)lua_tointeger(L, 3))) {
        lua_pushboolean(L, 0);
        return 1;
    }
    long thePlayer = lua_isinteger(L, 4) ? (long)lua_tointeger(L, 4) : (long)me->mPlayerID;
    if (thePlayer < 0 || (unsigned long)thePlayer >= gWorld.mNumPlayers) {
        lua_pushboolean(L, 0);
        return 1;
    }
    EEMHackANewUnit theMessage(theType, GE2DPoint((float)lua_tointeger(L, 2), (float)lua_tointeger(L, 3)), (unsigned char)thePlayer);
    unsigned char theSlot = me->mSlot;
    me->mSlot = kBorrowedSlot;
    theMessage.SetOriginatingSlot(kBorrowedSlot);
    theMessage.Execute();
    me->mSlot = theSlot;
    lua_pushboolean(L, 1);
    return 1;
}

// ---------------------------------------------------------------------------------------------------------------
// Opening, the micro step

void EECPUnitsOpen(lua_State* L, void* inPlayer)
{
    EEComputerPlayer* thePlayer = (EEComputerPlayer*)inPlayer;
    EECPUnitsGet(thePlayer);
    static const luaL_Reg kFunctions[] = {
        {"me", LuaMe}, {"time", LuaTime}, {"players", LuaPlayers}, {"map", LuaMap},
        {"units", LuaUnits}, {"unit", LuaUnit}, {"get", LuaGet}, {"seen", LuaSeen}, {"memory", LuaMemory},
        {"resources", LuaResources}, {"tile", LuaTile}, {"class", LuaClass}, {"events", LuaEvents},
        {"can_place", LuaCanPlace}, {"reach", LuaReach}, {"strength", LuaStrength},
        {"move", LuaMove}, {"attack", LuaAttack}, {"attack_ground", LuaAttackGround}, {"gather", LuaGather},
        {"build", LuaBuild}, {"repair", LuaRepair}, {"garrison", LuaGarrison}, {"load", LuaLoad}, {"unload", LuaUnload},
        {"train", LuaTrain}, {"research", LuaResearch}, {"stance", LuaStance}, {"stop", LuaStop},
        {"explore", LuaExplore}, {"delete", LuaDelete}, {"take", LuaTake}, {"release", LuaRelease},
        {"reflex", LuaReflex}, {"test_spawn", LuaTestSpawn}, {"test_town", LuaTestTown},
        {NULL, NULL}};
    for (const luaL_Reg* f = kFunctions; f->name != NULL; f++) {
        lua_pushlightuserdata(L, inPlayer);
        lua_pushcclosure(L, f->func, 1);
        lua_setfield(L, -2, f->name);
    }
}

static void EECPUnitsMicroError(EEComputerPlayer* inPlayer, EECPLuaState* inLua)
{
    std::string theStatus = EECPLuaStatusName(inLua->LastStatus());
    std::replace(theStatus.begin(), theStatus.end(), ' ', '_');
    std::string theMessage = inLua->LastError();
    std::replace(theMessage.begin(), theMessage.end(), '\n', '|');
    if (gCPTraceOn) {
        EECPTraceScope theScope("next");
        EECPTraceOut(inPlayer, "next.error", "in=micro status=%s msg=%s", theStatus.c_str(), theMessage.c_str());
    }
    if (gPlatSimMode) {
        fprintf(stderr, "SIM next.error p%lu t=%lu in=micro status=%s msg=%s\n", inPlayer->mPlayerID, gPhysics->mGameTime,
            theStatus.c_str(), theMessage.c_str());
        fflush(stderr);
    }
}

void EECPUnitsMicroTick(unsigned long inTime)
{
    for (unsigned long p = 1; p < gWorld.mNumPlayers && p < 64; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        EEComputerPlayer* theCP = thePlayer != NULL ? thePlayer->mComputerPlayer : NULL;
        if (theCP == NULL || theCP->mUnitsState == NULL || theCP->mNextState == NULL || !theCP->mActive)
            continue;
        EECPUnitsState* s = theCP->mUnitsState;
        // the enemy memory, once a second
        if ((long)inTime >= s->mMemoryNext) {
            s->mMemoryNext = (long)inTime + 1000;
            EECPUnitsRemember(theCP, s);
        }
        EECPNextState* theNext = theCP->mNextState;
        if (theNext->mLua == NULL || !theNext->mStarted || theNext->mFailed || s->mMicroNext == 0x7fffffff)
            continue;
        if (s->mMicroNext < 0)
            s->mMicroNext = (long)inTime + kMicroMs;
        if ((long)inTime < s->mMicroNext)
            continue;
        while (s->mMicroNext <= (long)inTime)
            s->mMicroNext += kMicroMs;
        EECPLuaValue theArg = EECPLuaValue::NewMap(), theResult;
        theArg.Set("time", EECPLuaValue::Int((int64_t)inTime));
        bool theFound = false;
        if (!theNext->mLua->CallOptional("micro", theArg, theResult, kMicroBudget, theFound))
            EECPUnitsMicroError(theCP, theNext->mLua);
        else if (!theFound)
            s->mMicroNext = 0x7fffffff;   // the script has no micro
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The save: "EENU", version 1, then int32 LE fields

static void EECPUnitsPut(std::string& ioBytes, long inValue)
{
    uint32_t v = (uint32_t)inValue;
    for (int i = 0; i < 4; i++)
        ioBytes += (char)((v >> (8 * i)) & 0xff);
}

void EECPUnitsSave(const EEComputerPlayer* inPlayer, std::string& outBytes)
{
    outBytes.clear();
    const EECPUnitsState* s = inPlayer->mUnitsState;
    if (s == NULL)
        return;
    outBytes.append("EENU", 4);
    EECPUnitsPut(outBytes, 1);
    EECPUnitsPut(outBytes, (long)s->mHeld.size());
    for (std::set<long>::const_iterator it = s->mHeld.begin(); it != s->mHeld.end(); ++it)
        EECPUnitsPut(outBytes, *it);
    EECPUnitsPut(outBytes, (long)s->mEvents.size());
    for (unsigned long k = 0; k < s->mEvents.size(); k++) {
        const UnitsEvent& e = s->mEvents[k];
        EECPUnitsPut(outBytes, e.mKind);
        EECPUnitsPut(outBytes, e.mUnit);
        EECPUnitsPut(outBytes, e.mOther);
        EECPUnitsPut(outBytes, e.mType);
        EECPUnitsPut(outBytes, e.mAmount);
        EECPUnitsPut(outBytes, e.mTime);
    }
    EECPUnitsPut(outBytes, (long)s->mForward.size());
    for (std::map<long, std::pair<long, long> >::const_iterator it = s->mForward.begin(); it != s->mForward.end(); ++it) {
        EECPUnitsPut(outBytes, it->first);
        EECPUnitsPut(outBytes, it->second.first);
        EECPUnitsPut(outBytes, it->second.second);
    }
    EECPUnitsPut(outBytes, (long)s->mMemory.size());
    for (std::map<long, Remembered>::const_iterator it = s->mMemory.begin(); it != s->mMemory.end(); ++it) {
        const Remembered& r = it->second;
        EECPUnitsPut(outBytes, it->first);
        EECPUnitsPut(outBytes, r.mPlayer);
        EECPUnitsPut(outBytes, r.mType);
        EECPUnitsPut(outBytes, r.mX);
        EECPUnitsPut(outBytes, r.mY);
        EECPUnitsPut(outBytes, r.mHP);
        EECPUnitsPut(outBytes, r.mSeen);
        EECPUnitsPut(outBytes, r.mBuilding);
    }
    EECPUnitsPut(outBytes, s->mMemoryMs);
    EECPUnitsPut(outBytes, s->mMemoryNext);
    EECPUnitsPut(outBytes, s->mReflexNavalFlee ? 1 : 0);
    EECPUnitsPut(outBytes, s->mReflexDanger ? 1 : 0);
    EECPUnitsPut(outBytes, s->mMicroNext);
}

namespace {
struct Getter {
    const std::string& mBytes;
    size_t mOffset;
    bool mOK;
    explicit Getter(const std::string& inBytes) : mBytes(inBytes), mOffset(0), mOK(true) {}
    long Get()
    {
        if (this->mOffset + 4 > this->mBytes.size()) {
            this->mOK = false;
            return 0;
        }
        uint32_t v = 0;
        for (int i = 0; i < 4; i++)
            v |= (uint32_t)(unsigned char)this->mBytes[this->mOffset + i] << (8 * i);
        this->mOffset += 4;
        return (long)(int32_t)v;
    }
    long Count(long inMax)
    {
        long n = this->Get();
        if (n < 0 || n > inMax)
            this->mOK = false;
        return this->mOK ? n : 0;
    }
};
}  // namespace

bool EECPUnitsLoad(EEComputerPlayer* inPlayer, const std::string& inBytes)
{
    if (inBytes.empty())
        return true;
    if (inBytes.size() < 8 || inBytes.compare(0, 4, "EENU") != 0)
        return false;
    Getter g(inBytes);
    g.mOffset = 4;
    if (g.Get() != 1)
        return false;
    EECPUnitsState* s = EECPUnitsGet(inPlayer);
    s->mHeld.clear();
    long n = g.Count(1 << 20);
    for (long k = 0; k < n && g.mOK; k++)
        s->mHeld.insert(g.Get());
    s->mEvents.clear();
    s->mAttackedAt.clear();
    n = g.Count(kMaxEvents);
    for (long k = 0; k < n && g.mOK; k++) {
        UnitsEvent e;
        e.mKind = g.Get();
        e.mUnit = g.Get();
        e.mOther = g.Get();
        e.mType = g.Get();
        e.mAmount = g.Get();
        e.mTime = g.Get();
        if (e.mKind < kEvAttacked || e.mKind > kEvConverted)
            g.mOK = false;
        else
            s->mEvents.push_back(e);
    }
    s->mForward.clear();
    n = g.Count(1 << 20);
    for (long k = 0; k < n && g.mOK; k++) {
        long a = g.Get(), b = g.Get(), t = g.Get();
        s->mForward[a] = std::make_pair(b, t);
    }
    s->mMemory.clear();
    n = g.Count(kMaxRemembered);
    for (long k = 0; k < n && g.mOK; k++) {
        long theID = g.Get();
        Remembered r;
        r.mPlayer = g.Get();
        r.mType = g.Get();
        r.mX = g.Get();
        r.mY = g.Get();
        r.mHP = g.Get();
        r.mSeen = g.Get();
        r.mBuilding = g.Get();
        s->mMemory[theID] = r;
    }
    s->mMemoryMs = g.Get();
    s->mMemoryNext = g.Get();
    s->mReflexNavalFlee = g.Get() != 0;
    s->mReflexDanger = g.Get() != 0;
    s->mMicroNext = g.Get();
    return g.mOK && g.mOffset == inBytes.size();
}
