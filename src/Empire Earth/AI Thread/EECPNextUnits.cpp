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
#include <functional>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <utility>
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
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUGRust.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEPathManager.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIArea.h"
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
#include "ee/EECPCombatManager.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEMessageID.h"
#include "ee/EECPTrace.h"
#include "ee/EEGameLog.h"
#include "ee/EECPLua.h"
#include "ee/EEEvent.h"
#include "ee/EEUGRust.h"
#include "ee/EECPBehavior.h"
#include "ee/EERandomMapIDs.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EELuaApi.h"
#include "ee/EECPNext.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPNextUnits.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "platform/PlatSim.h"
#include "rust/sim/ai.h"   // ee_ai_units_*: the save block's format (units_save.rs), the bindings' Lua helpers (units_lua.rs)
#include "rust/sim/units.h"
#include "ee/EEUnitRegions.h"

// ---------------------------------------------------------------------------------------------------------------
// The state

namespace {

const long kMaxIds = 4000;           // ids one call takes
const long kMicroMs = 250;           // the micro step's period (game ms)
const int64_t kMicroBudget = 300000;
const long kDangerCell = 4;          // the danger layers' cell (tiles; ee_ai units_state.rs DANGER_CELL)
const unsigned char kBorrowedSlot = 0xfd;   // the slot id a computer player answers to while its order runs

// the costs in instruction equivalents (AI_LUA_UNITS §2.1)
const int64_t kCostCall = 200, kCostId = 20, kCostScan = 5, kCostRecord = 300, kCostField = 15, kCostTile = 50,
              kCostCommand = 300, kCostOrdered = 100;

}  // namespace

// The state is Rust's (ee_ai units_state.rs: the held set, the events, the forwarding, the enemy memory, the reflexes,
// the limits, the danger layers; units_bind.rs: its hooks and the bindings that read the world). The C++ keeps the
// commands' scratch and ai.wall_route's path requests (C++ objects, not saved).
struct EECPUnitsState {
    void* mRust = ee_ai_units_state_new();
    // scratch (members, so that a Lua error never skips a destructor)
    std::vector<long> mIds, mIds2;
    std::vector<std::pair<long long, long> > mSorted;
    // ai.wall_route's path requests (the path manager's, held until read; not saved: a loaded game forgets them)
    std::map<long, EEAutoPathRequest*> mRoutes;
    long mNextRoute = 1;
    ~EECPUnitsState()
    {
        for (std::map<long, EEAutoPathRequest*>::iterator it = mRoutes.begin(); it != mRoutes.end(); ++it)
            if (it->second != NULL)
                it->second->Dereference();
        ee_ai_units_state_free(mRust);
    }
};

static uint64_t EECPUnitsHandle(const RTSUnit* inUnit)
{
    return (uint64_t)(uintptr_t)inUnit;
}

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
    return s != NULL ? ee_ai_units_forward(s->mRust, inID) : inID;
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

// a foundation the player's builders have not reached yet: in the player's pending list, not in the world's lookup
static EEUnit* EECPUnitsPending(EEComputerPlayer* inPlayer, long inID)
{
    for (unsigned long k = 0; k < inPlayer->GetNumPendingUnits(); k++) {
        RTSUnit* thePending = inPlayer->GetPendingUnit(k);
        if (thePending != NULL && (long)thePending->mUnitID == inID)
            return static_cast<EEUnit*>(thePending);
    }
    return NULL;
}

static EEComplexUnit* EECPUnitsOwn(EEComputerPlayer* inPlayer, long inID)
{
    long theID = EECPUnitsForward(inPlayer->mUnitsState, inID);
    EEUnit* theUnit = EECPUnitsAlive(theID);
    if (theUnit == NULL)
        theUnit = EECPUnitsPending(inPlayer, theID);
    if (theUnit == NULL || theUnit->mPlayer != inPlayer || theUnit->mComplexUnit == NULL || theUnit->mUnitClass == NULL)
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

// a projectile, bomb, missile or torpedo in flight: in the owner's unit list, never a unit of the interface
static bool EECPUnitsIsProjectile(EEUnit* inUnit)
{
    long k = inUnit->mUnitClass->mCategory;
    return k == kUnitCategoryProjectile || k == kUnitCategoryBomb || k == kUnitCategoryMissile || k == kUnitCategoryTorpedo;
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

void EECPUnitsOnDamage(EEComputerPlayer* inPlayer, long inAttackerPlayer, long inAmount, EEUnit* inUnit)
{
    if (inPlayer->mUnitsState != NULL && inUnit != NULL)
        ee_ai_units_on_damage(inPlayer->mUnitsState->mRust, inPlayer, inAttackerPlayer, inAmount, EECPUnitsHandle(inUnit));
}

void EECPUnitsOnPlaced(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    if (inPlayer->mUnitsState != NULL && inUnit != NULL)
        ee_ai_units_on_unit(inPlayer->mUnitsState->mRust, 3, EECPUnitsHandle(inUnit));   // created
}

void EECPUnitsOnAttackPlayer(EEComputerPlayer* inPlayer, long inPlayerID, bool inDefend)
{
    if (inPlayer->mUnitsState != NULL)
        ee_ai_units_on_player(inPlayer->mUnitsState->mRust, inDefend ? 9 : 8, inPlayerID, 0, 0);
}

void EECPUnitsOnFlare(EEComputerPlayer* inPlayer, long inPlayerID, long inX, long inY)
{
    if (inPlayer->mUnitsState != NULL)
        ee_ai_units_on_player(inPlayer->mUnitsState->mRust, 10, inPlayerID, inX, inY);
}

void EECPUnitsOnCompleted(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    if (inPlayer->mUnitsState != NULL && inUnit != NULL)
        ee_ai_units_on_unit(inPlayer->mUnitsState->mRust, 4, EECPUnitsHandle(inUnit));   // built
}

void EECPUnitsOnTaken(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit, bool inSwapped)
{
    if (inPlayer->mUnitsState != NULL && inUnit != NULL && !inSwapped)
        ee_ai_units_on_taken(inPlayer->mUnitsState->mRust, inPlayer, EECPUnitsHandle(inUnit));
}

void EECPUnitsOnGoalDone(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    if (inPlayer->mUnitsState != NULL && inUnit != NULL)
        ee_ai_units_on_unit(inPlayer->mUnitsState->mRust, 5, EECPUnitsHandle(inUnit));   // idle
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
        if (ioSwaps[k].mOld->mPlayer != inPlayer && ioSwaps[k].mNew->mPlayer != inPlayer)
            continue;
        ee_ai_units_on_swap(s->mRust, (long)ioSwaps[k].mOld->mUnitID, (long)ioSwaps[k].mNew->mUnitID,
            ioSwaps[k].mNew->mPlayer == inPlayer, ioSwaps[k].mNew->mUnitClass->mUnitType, theNow);
    }
    ee_ai_units_forward_expire(s->mRust, theNow);
}

// ---------------------------------------------------------------------------------------------------------------
// The enemy memory: every second, what the player sees of its enemies

// ---------------------------------------------------------------------------------------------------------------
// Lua helpers

static EEComputerPlayer* EECPUnitsMe(lua_State* L)
{
    return (EEComputerPlayer*)lua_touserdata(L, lua_upvalueindex(1));
}

static void EECPUnitsCharge(lua_State* L, int64_t inAmount)
{
    ee_ai_units_mark_used(EECPUnitsGet(EECPUnitsMe(L))->mRust);
    EECPLuaState::From(L)->Charge(L, inAmount);
}

bool EECPUnitsUsed(const EEComputerPlayer* inPlayer)
{
    return inPlayer != NULL && inPlayer->mUnitsState != NULL && ee_ai_units_used(inPlayer->mUnitsState->mRust);
}

static const char* EECPUnitsName(EECPUnitsState* s, long inType)
{
    return ee_ai_units_class_name(s->mRust, inType);
}

static long EECPUnitsClassByName(const char* inName)
{
    return EECPNextClassByName(inName);
}

// the helpers below are Rust (ee_ai units_lua.rs): the same Lua calls, the same answers
static long EECPUnitsClassArg(lua_State* L, int inIndex)
{
    return ee_ai_units_class_arg(L, inIndex, EECPUnitsClassByName);
}

// ids at inIndex (an integer or an array of integers) -> outIds (no error: the count of bad entries)
static long EECPUnitsReadIds(lua_State* L, int inIndex, std::vector<long>& outIds)
{
    static_assert(kMaxIds == 4000, "ee_ai units_lua.rs MAX_IDS");
    outIds.resize(kMaxIds);
    size_t theCount = 0;
    long theBad = ee_ai_units_read_ids(L, inIndex, outIds.data(), &theCount);
    outIds.resize(theCount);
    return theBad;
}

static bool EECPUnitsOptInt(lua_State* L, int inTable, const char* inKey, long& outValue)
{
    return ee_ai_units_opt_int(L, inTable, inKey, &outValue);
}

static bool EECPUnitsOptBool(lua_State* L, int inTable, const char* inKey, bool inDefault)
{
    return ee_ai_units_opt_bool(L, inTable, inKey, inDefault);
}

// a string field -> a pointer into the Lua string (valid while the table holds it), or NULL
static const char* EECPUnitsOptString(lua_State* L, int inTable, const char* inKey)
{
    return ee_ai_units_opt_string(L, inTable, inKey);
}

static void EECPUnitsSetInt(lua_State* L, const char* inKey, long inValue)
{
    ee_ai_units_set_int(L, inKey, inValue);
}

static void EECPUnitsSetStr(lua_State* L, const char* inKey, const char* inValue)
{
    ee_ai_units_set_str(L, inKey, inValue);
}

static void EECPUnitsSetBool(lua_State* L, const char* inKey, bool inValue)
{
    ee_ai_units_set_bool(L, inKey, inValue);
}

// ---------------------------------------------------------------------------------------------------------------
// Queries

// ai.can_hurt(attacker_type, target_type) -> whether the attacker's weapon hurts the target's family (the damage
// table, combat.yaml: a human reads it too; EECPAttackSubGroupUnit::CanAttack's test without the ignored targets)
static int LuaCanHurt(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    long theAttacker = EECPUnitsClassArg(L, 1);
    long theTarget = EECPUnitsClassArg(L, 2);
    bool theResult = false;
    if (theAttacker >= 0 && theTarget >= 0 && (unsigned long)theAttacker < EEUnitClassCount()
        && (unsigned long)theTarget < EEUnitClassCount() && gUnitClassTable[theAttacker] != NULL
        && gUnitClassTable[theTarget] != NULL && gDamageTable != NULL) {
        EEUnitClass* theA = gUnitClassTable[theAttacker];
        EEUnitClass* theT = gUnitClassTable[theTarget];
        long theAmmo = theA->mAmmoType;
        long theFamily = theT->mFamily;
        theResult = theAmmo >= 0 && theFamily >= 0 && theT->CanBeAttacked() && gDamageTable[theFamily][theAmmo] != 0.0f;
    }
    lua_pushboolean(L, theResult);
    return 1;
}

// ai.wonders() -> {victory, to_win, players = {{player, team, count}}, list = {{id, player, team, type, built, x, y}}}:
// the wonders (the units whose tech is a wonder, kind 4), as the game announces them and the wonder countdown shows
// them; a wonder's tile (x, y) only where the player has explored it, or with the manifest's "sees_all" (the classic's
// information cheat: its UpdateWonderRace and AttackUnit read every wonder). The classic's wonder race and response
// (EECPCombatManager::RespondToUnit, UpdateWonderRace).
static int LuaWonders(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsCharge(L, kCostCall);
    bool theAll = EECPEngineSeesAll(me);
    lua_createtable(L, 0, 4);
    EECPUnitsSetBool(L, "victory", gWorld.mVictoryEnabled);
    EECPUnitsSetInt(L, "to_win", (long)gWorld.mNumberOfWondersToWin);
    lua_newtable(L);
    lua_newtable(L);
    long theCount = 0;
    long theListed = 0;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (thePlayer == NULL || thePlayer->mObserverMode || thePlayer->mTechnologies == NULL)
            continue;
        long theTeam = (long)thePlayer->GetTeamIndex();
        lua_createtable(L, 0, 3);
        EECPUnitsSetInt(L, "player", (long)thePlayer->mPlayerID);
        EECPUnitsSetInt(L, "team", theTeam);
        EECPUnitsSetInt(L, "count", (long)thePlayer->GetNumWonderUnits());
        lua_rawseti(L, -3, ++theCount);
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* c = theUnit->mComplexUnit;
            if (c == NULL || c->mHitPoints <= 0 || !c->mUnitClass->IsLandBuilding())
                continue;
            EETechTreeNode* theNode = thePlayer->mTechnologies->Get(c->mUnitClass->mTechnology);
            if (theNode == NULL || theNode->mData->mKind != 4)
                continue;
            EECPUnitsCharge(L, kCostId);
            lua_createtable(L, 0, 7);
            EECPUnitsSetInt(L, "id", (long)c->mUnitID);
            EECPUnitsSetInt(L, "player", (long)thePlayer->mPlayerID);
            EECPUnitsSetInt(L, "team", theTeam);
            EECPUnitsSetStr(L, "type", EECPUnitsName(s, c->mUnitClass->mUnitType));
            EECPUnitsSetBool(L, "built", c->mIsBuilt);
            if (thePlayer == me || theAll || (EECPUnitsOnMap(c) && static_cast<EETile*>(c->mTile)->IsVisibleTo(me))) {
                EECPUnitsSetInt(L, "x", (long)c->mTileX);
                EECPUnitsSetInt(L, "y", (long)c->mTileY);
            }
            lua_rawseti(L, -2, ++theListed);
        }
    }
    lua_setfield(L, -3, "list");
    lua_setfield(L, -2, "players");
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
    lua_createtable(L, 0, 16);
    EECPUnitsSetStr(L, "type", EECPUnitsName(s, theType));
    EECPUnitsSetStr(L, "category", ee_ai_units_category_class((uint64_t)(uintptr_t)c, false));
    EECPUnitsSetInt(L, "family", (long)c->GetFamily());
    EECPUnitsSetInt(L, "tech", c->mTechnology);   // the class's own tech (what ai.train orders; ai.techs' id)
    {
        // the cost as the player pays it (the tech's cost scaled; 0 for a class the player's tree has no node for)
        EETechTreeNode* theNode = me->mTechnologies != NULL ? me->mTechnologies->Get(c->mTechnology) : NULL;
        lua_newtable(L);
        static const char* const kNames[5] = {"food", "wood", "stone", "gold", "iron"};
        for (long i = 0; i < 5; i++) {
            lua_pushinteger(L, theNode != NULL ? (lua_Integer)me->ScaleCost(theNode->GetCost(kResourceFood + i, kUnitTypeTownCenter, 1.0f)) : 0);
            lua_setfield(L, -2, kNames[i]);
        }
        lua_setfield(L, -2, "cost");
        lua_newtable(L);
        for (long i = 0; i < 5; i++) {
            lua_pushinteger(L, theNode != NULL ? (lua_Integer)me->ScaleCost(theNode->mData->mCosts[kResourceFood + i]) : 0);
            lua_setfield(L, -2, kNames[i]);
        }
        lua_setfield(L, -2, "data_cost");   // the needs ledger's cost (EECPResourceManager::AddCosts)
    }
    EECPUnitsSetBool(L, "building", c->IsBuilding());
    EECPUnitsSetInt(L, "size", c->GetTileSize());
    EECPUnitsSetInt(L, "garrison", c->mGarrisonCapacity);
    EECPUnitsSetInt(L, "transport", c->mTransportCapacity);
    EECPUnitsSetBool(L, "can_garrison", c->mCanGarrison);
    // the class's military data (the classic unit selector's: its AI kind, the kinds it counters, its roles,
    // its influence and epoch, its domain)
    EECPUnitsSetInt(L, "ai_kind", (long)c->mAIKind);
    lua_createtable(L, 3, 0);
    lua_pushinteger(L, (lua_Integer)c->mCounterKind1);
    lua_rawseti(L, -2, 1);
    lua_pushinteger(L, (lua_Integer)c->mCounterKind2);
    lua_rawseti(L, -2, 2);
    lua_pushinteger(L, (lua_Integer)c->mCounterKind3);
    lua_rawseti(L, -2, 3);
    lua_setfield(L, -2, "counters");
    EECPUnitsSetBool(L, "attack_unit", c->mIsAttackUnit);
    EECPUnitsSetBool(L, "defense_unit", c->mIsDefenseUnit);
    EECPUnitsSetBool(L, "medic", c->mIsMedic);
    EECPUnitsSetBool(L, "converts", c->mHasCalamityPowers || c->mConversionRate != 0.0f);
    EECPUnitsSetBool(L, "is_transport", c->IsTransport());
    // a planet spaceship (category 0x22: the classic's space fighter sub-group, kind 11; the other space units take
    // the spaceship sub-group, kind 9: EECPAttackGroup::GetSubGroupKind)
    EECPUnitsSetBool(L, "planet_spaceship", c->mCategory == kUnitCategoryPlanetSpaceship);
    EECPUnitsSetBool(L, "mine", c->mFamily == kUnitFamilyMines);
    EECPUnitsSetInt(L, "influence", c->mInfluence);
    EECPUnitsSetInt(L, "epoch", c->mEpoch);
    EECPUnitsSetInt(L, "pop", c->GetPopulationCost());
    {
        unsigned char d = c->GetDomains();
        const char* theDomain = ((d & kMovementAir) && !c->FliesOver()) ? "air"
            : (d & kMovementSea) ? "sea" : (d & kMovementSpace) ? "space" : "land";
        EECPUnitsSetStr(L, "domain", theDomain);
    }
    if ((unsigned long)theType < me->mUnitClassUpgrades.size() && me->mUnitClassUpgrades[theType] != NULL) {
        EEUnitClassUpgrade* u = me->mUnitClassUpgrades[theType];
        EECPUnitsSetInt(L, "hp", u->mMaxHitPoints);
        EECPUnitsSetInt(L, "attack", UMFloatToLong(u->mAttack));
        EECPUnitsSetInt(L, "range", EECPUnitsCenti(u->mRange));
        EECPUnitsSetInt(L, "sight", EECPUnitsCenti(u->mLineOfSight));
        EECPUnitsSetInt(L, "speed", EECPUnitsCenti(u->mMaxVelocity));
        EECPUnitsSetInt(L, "reload", u->mReloadTime);
        // an aircraft's fuel (ms in the air; 0: no limit): the classic air sub-group's reach, flight x speed x 0.9
        EECPUnitsSetInt(L, "flight_time", u->mFlightTime);
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

static unsigned short EECPUnitsAreaNear(long inX, long inY, long inKind);

// ---------------------------------------------------------------------------------------------------------------
// Commands

enum CmdReason { kOK, kBadArgument, kNotOwn, kNotSeen, kUnexplored, kCannotPlace, kCannotAfford, kCannotDo, kLimit, kFull, kNotInEpoch };
const char* const kReasonNames[] = {"none", "bad_argument", "not_own", "not_seen", "unexplored", "cannot_place",
                                    "cannot_afford", "cannot_do", "limit", "full", "not_in_epoch"};

// the result: code, reason, n, id
static int EECPUnitsResult(lua_State* L, long inReason, long inCount, long inID)
{
    return ee_ai_units_result(L, inReason, inCount, inID);
}

static bool EECPUnitsWithinLimits(EECPUnitsState* s, long inUnits)
{
    return ee_ai_units_within_limits(s->mRust, inUnits, (long)gPhysics->mGameTime);
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

static bool EECPUnitsTileOK(long x, long y)
{
    return gWorld.mMap != NULL && x >= 0 && y >= 0 && gWorld.mMap->GetTile((unsigned long)x, (unsigned long)y) != NULL;
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
    EEMRecord theMessage(kMessageHackANewUnit, {theType, GE2DPoint((float)lua_tointeger(L, 2), (float)lua_tointeger(L, 3)), (unsigned char)thePlayer});
    unsigned char theSlot = me->mSlot;
    me->mSlot = kBorrowedSlot;
    theMessage.SetOriginatingSlot(kBorrowedSlot);
    theMessage.Execute();
    me->mSlot = theSlot;
    lua_pushboolean(L, 1);
    return 1;
}

// ---------------------------------------------------------------------------------------------------------------
// The danger layers and the route (AI_LUA_UNITS §4.3-§4.4)

// the map's size in tiles (ee/EEMapDimensions.h)
static void EECPUnitsMapSize(long& outW, long& outH)
{
    outW = gWorld.mMap != NULL ? (long)gMapWidth : 0;
    outH = gWorld.mMap != NULL ? (long)gMapHeight : 0;
}

static long EECPUnitsDangerAt(const EECPUnitsState* s, long inX, long inY)
{
    return ee_ai_units_danger_at(s->mRust, inX, inY);
}

// the land mass (or water body) at a tile, else the nearest within 3 tiles: a tree's tile, or a building's, is on
// none (PORT_LEARNINGS "A tree's tile is on no land continent")
static unsigned short EECPUnitsAreaNear(long inX, long inY, long inKind)
{
    for (long r = 0; r <= 3; r++)
        for (long dy = -r; dy <= r; dy++)
            for (long dx = -r; dx <= r; dx++) {
                if (dx > -r && dx < r && dy > -r && dy < r)
                    continue;
                if (inX + dx < 0 || inY + dy < 0)
                    continue;
                unsigned short a = gContinentManager.GetContinent(U2DPoint<long>(inX + dx, inY + dy), inKind);
                if (a != 0)
                    return a;
            }
    return 0;
}

// ai.water_sites(x, y, {max = 64}) -> {{x, y}, ...}: the water tiles by the shore for docks, nearest (x, y) first, at
// most max (up to 512): the largest water body's shore tiles next to the land mass at (x, y) and the water tiles
// round them (EECPBuildManager::CollectSiteTiles(2)), explored ones only; an empty table when that water body does
// not touch the land mass
static int LuaWaterSites(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    lua_newtable(L);
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2) || gWorld.mMap == NULL)
        return 1;
    long x = (long)lua_tointeger(L, 1), y = (long)lua_tointeger(L, 2);
    long theMax = 64;
    EECPUnitsOptInt(L, 3, "max", theMax);
    theMax = theMax < 1 ? 1 : theMax > 512 ? 512 : theMax;
    EECPUnitsCharge(L, kCostCall);
    unsigned short theWaterID = (unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryWater);
    EEAIContinent* theWater = theWaterID != 0 ? gContinentManager.GetContinentFromID(theWaterID) : NULL;
    EEAIContinent* theLand = gContinentManager.GetContinentAt(x, y);
    if (theWater == NULL || theLand == NULL
        || !gContinentManager.mTileFlags[theWaterID * gContinentManager.mFlagsWidth + theLand->GetID()])
        return 1;
    std::vector<U2DPoint<long> > theShore;
    theWater->GetShoreNextTo(theLand->GetID(), theShore);
    EECPUnitsCharge(L, (int64_t)theShore.size() * 9 * kCostScan);
    std::vector<U2DPoint<long> > theTiles;
    theTiles.reserve(theShore.size() * 9);
    for (size_t i = 0; i < theShore.size(); i++)
        for (long dy = -1; dy <= 1; dy++)
            for (long dx = -1; dx <= 1; dx++)
                theTiles.push_back(U2DPoint<long>(theShore[i].mX + dx, theShore[i].mY + dy));
    std::sort(theTiles.begin(), theTiles.end());
    theTiles.erase(std::unique(theTiles.begin(), theTiles.end()), theTiles.end());
    long w, h;
    EECPUnitsMapSize(w, h);
    std::vector<std::pair<long, std::pair<long, long> > > theSites;
    for (size_t i = 0; i < theTiles.size(); i++) {
        long tx = theTiles[i].mX, ty = theTiles[i].mY;
        if (tx < 0 || ty < 0 || tx >= w || ty >= h)
            continue;
        EETile* theTile = static_cast<EETile*>(gWorld.mMap->GetTile((unsigned long)tx, (unsigned long)ty));
        if (theTile == NULL || !theTile->IsVisibleTo(me)
            || (theTile->GetTerrainType() != kTerrainDeepWater && theTile->GetTerrainType() != kTerrainShallowWater))
            continue;
        theSites.push_back(std::make_pair(EECPUnitsD2(tx, ty, x, y), std::make_pair(ty, tx)));
    }
    std::sort(theSites.begin(), theSites.end());   // the distance, then the tile: the same order on every machine
    long n = 0;
    for (size_t i = 0; i < theSites.size() && n < theMax; i++) {
        lua_createtable(L, 2, 0);
        lua_pushinteger(L, (lua_Integer)theSites[i].second.second);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, (lua_Integer)theSites[i].second.first);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, (lua_Integer)++n);
    }
    EECPUnitsCharge(L, n * kCostField * 3);
    return 1;
}

// ai.enemy_near(x, y) -> whether armed enemy units stand near the tile: the classic build manager's HasEnemyNear.
// The fog-honest default (every AI): the enemies the seat sees or remembers (EECPNextSiteThreat: soldiers seen
// lately within its range, an enemy tower or armed building in range). An AI whose manifest entry declares
// "sees_all" (the classic's information cheat) gets the original: every armed unit not its own in the influence
// regions round the tile (one tile on small island and planet maps), seen or not, idle searchers (target search
// timer above 60 s) left out
static int LuaEnemyNear(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsCharge(L, kCostCall + 20 * kCostScan);
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2) || gWorld.mMap == NULL) {
        lua_pushboolean(L, 0);
        return 1;
    }
    long inX = (long)lua_tointeger(L, 1), inY = (long)lua_tointeger(L, 2);
    if (!EECPEngineSeesAll(me)) {
        lua_pushboolean(L, EECPNextSiteThreat(me, inX, inY, false, NULL) != 0 ? 1 : 0);
        return 1;
    }
    std::vector<EEComplexUnit*> theUnits;
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    EEMap* theMap = gWorld.mMap;
    U2DRectangle<long> theRect;
    long theCount = 0;
    if (theTable != NULL) {
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
        theCount = theTable->SumEnemyValue(me->mPlayerID, theTable->GetRegions(theRect), true);
        if (theCount > 0) {
            EECollectRegionUnits(theMap->mUnitRegions, theRect, theUnits);
            theCount = 0;
            for (size_t i = 0; i < theUnits.size(); i++) {
                EEComplexUnit* u = theUnits[i];
                if (u->mPlayer != me && u->mStats->mAttack != 0.0f && !(u->mTargetSearchTimer > 60000))
                    theCount++;
            }
        }
    }
    lua_pushboolean(L, theCount > 0 ? 1 : 0);
    return 1;
}

// ai.garrison_fill(id, n) -> ok: an own finished building's garrison count raised by n with no citizen going in, as the
// classic's FillGarrison does when it has too few idle citizens (EEComplexUnit::AddGarrison(count + n, population):
// the original's call, which takes only into an empty garrison). Only for an AI whose manifest entry declares
// "fills_garrison" (the classic's garrison cheat); false for every other AI
static int LuaGarrisonFill(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsCharge(L, kCostCommand);
    if (!EECPEngineFillsGarrison(me) || !lua_isinteger(L, 1) || !lua_isinteger(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    EEComplexUnit* c = EECPUnitsOwn(me, (long)lua_tointeger(L, 1));
    long n = (long)lua_tointeger(L, 2);
    bool theDone = false;
    if (c != NULL && n > 0 && c->mIsBuilt && c->mHitPoints > 0 && c->mUnitClass->mGarrisonCapacity > 0)
        theDone = c->AddGarrison(c->mGarrisonCount + n, c->mGarrisonPopulation);
    if (gCPTraceOn) {
        EECPTraceScope theScope("next");
        EECPTraceOut(me, "next.garrison_fill", "id=%ld n=%ld done=%d", (long)lua_tointeger(L, 1), n, theDone ? 1 : 0);
    }
    lua_pushboolean(L, theDone ? 1 : 0);
    return 1;
}

// ai.area(id) or ai.area(x, y) -> {id, category = 1 land / 2 water / 3 cliff / 4 space / 5 map edge, tiles = n}: an
// area of the continent manager (a land mass, a water body: the classic's continents, as ai.tuning's largest_sea) by
// its id or the one at a tile, its kind and size; nil for none
static int LuaArea(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    EEAIContinent* theArea = NULL;
    if (lua_isinteger(L, 1) && lua_isinteger(L, 2)) {
        long x = (long)lua_tointeger(L, 1), y = (long)lua_tointeger(L, 2);
        long w, h;
        EECPUnitsMapSize(w, h);
        if (x >= 0 && y >= 0 && x < w && y < h)
            theArea = gContinentManager.GetContinentAt(x, y);
    } else if (lua_isinteger(L, 1)) {
        lua_Integer theID = lua_tointeger(L, 1);
        if (theID > 0 && (size_t)theID < gContinentManager.mContinents.size())
            theArea = gContinentManager.GetContinentByID((unsigned short)theID);
    }
    if (theArea == NULL)
        return 0;
    lua_createtable(L, 0, 3);
    EECPUnitsSetInt(L, "id", (long)theArea->GetID());
    EECPUnitsSetInt(L, "category", (long)theArea->GetCategory());
    EECPUnitsSetInt(L, "tiles", (long)theArea->GetNumTiles());
    return 1;
}

// ai.forest_point(x, y, r, pick, seed) -> x, y, n: a random point of one of the forests of the continent manager
// (EEAIForest, the classic's forests) on the land mass at (x, y) whose hull holds the tile or comes within r tiles of
// it - the (pick mod n)-th of the n found, its point drawn from a stream seeded with the script's number
// (EECPDefenseCommander::AssignSubGroupTask's forest); nil when none
static int LuaForestPoint(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    for (int i = 1; i <= 5; i++)
        if (!lua_isinteger(L, i))
            return 0;
    long x = (long)lua_tointeger(L, 1), y = (long)lua_tointeger(L, 2), r = (long)lua_tointeger(L, 3);
    long w, h;
    EECPUnitsMapSize(w, h);
    if (x < 0 || y < 0 || x >= w || y >= h)
        return 0;
    EEAIContinent* theLand = gContinentManager.GetContinentAt(x, y);
    if (theLand == NULL)
        return 0;
    U2DPoint<long> theFrom(x, y);
    std::vector<EEAIForest*> theForests;
    for (size_t i = 0; i < theLand->GetForestIDs().size(); i++) {
        unsigned short theID = theLand->GetForestIDs()[i];
        EEAIForest* theForest = theID < gContinentManager.mForests.size() ? gContinentManager.mForests[theID] : NULL;
        if (theForest != NULL && (theForest->HullContains(theFrom)
                || theForest->GetClosestHullPoint(theFrom).DistanceToSquared(theFrom) <= r * r))
            theForests.push_back(theForest);
    }
    EECPUnitsCharge(L, (int64_t)theLand->GetForestIDs().size() * kCostScan * 4);
    if (theForests.empty())
        return 0;
    long thePick = (long)(lua_tointeger(L, 4) % (lua_Integer)theForests.size());
    if (thePick < 0)
        thePick += (long)theForests.size();
    URandom theRandom((unsigned long)lua_tointeger(L, 5));
    EEAIForest* theForest = theForests[thePick];
    if (!(theForest->GetNumTiles() > 0))
        return 0;
    U2DPoint<long> thePoint = theForest->GetTileSet().GetRandomPoint(theRandom);
    lua_pushinteger(L, (lua_Integer)thePoint.mX);
    lua_pushinteger(L, (lua_Integer)thePoint.mY);
    lua_pushinteger(L, (lua_Integer)theForests.size());
    return 3;
}

// ai.hill_point(x, y, r, pick, seed) -> x, y, n: as ai.forest_point, over the continent manager's hills (EEAIHill) on
// the land mass at (x, y) whose hull holds the tile or comes within r tiles (the classic defence ladder's hill)
static int LuaHillPoint(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    for (int i = 1; i <= 5; i++)
        if (!lua_isinteger(L, i))
            return 0;
    long x = (long)lua_tointeger(L, 1), y = (long)lua_tointeger(L, 2), r = (long)lua_tointeger(L, 3);
    long w, h;
    EECPUnitsMapSize(w, h);
    if (x < 0 || y < 0 || x >= w || y >= h)
        return 0;
    EEAIContinent* theLand = gContinentManager.GetContinentAt(x, y);
    if (theLand == NULL)
        return 0;
    U2DPoint<long> theFrom(x, y);
    std::vector<EEAIHill*> theHills;
    for (size_t i = 0; i < theLand->GetHillIDs().size(); i++) {
        unsigned short theID = theLand->GetHillIDs()[i];
        EEAIHill* theHill = theID < gContinentManager.mHills.size() ? gContinentManager.mHills[theID] : NULL;
        if (theHill != NULL && (theHill->HullContains(theFrom)
                || theHill->GetClosestHullPoint(theFrom).DistanceToSquared(theFrom) <= r * r))
            theHills.push_back(theHill);
    }
    EECPUnitsCharge(L, (int64_t)theLand->GetHillIDs().size() * kCostScan * 4);
    if (theHills.empty())
        return 0;
    long thePick = (long)(lua_tointeger(L, 4) % (lua_Integer)theHills.size());
    if (thePick < 0)
        thePick += (long)theHills.size();
    EEAIHill* theHill = theHills[thePick];
    if (!(theHill->GetNumTiles() > 0))
        return 0;
    URandom theRandom((unsigned long)lua_tointeger(L, 5));
    U2DPoint<long> thePoint = theHill->GetTileSet().GetRandomPoint(theRandom);
    lua_pushinteger(L, (lua_Integer)thePoint.mX);
    lua_pushinteger(L, (lua_Integer)thePoint.mY);
    lua_pushinteger(L, (lua_Integer)theHills.size());
    return 3;
}

// the classic wall manager's area utilities (EECPWallManager.cpp declares them the same way)
void EEGetWallLine(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo, std::vector<U2DPoint<long> >& outTiles);
void EEGetSquareRingTiles(long inX, long inY, long inRadius, std::vector<U2DPoint<long> >& outTiles);
long EEGetUnitsOn(std::vector<U2DPoint<long> >& inTiles, std::vector<EEUnit*>& outUnits);
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles);

// ai.wall_path(unit, x1, y1, x2, y2) -> {{x, y}, ...}: the tile path an own unit would take from (x1, y1) to
// (x2, y2) inside the box of the two tiles, as the classic wall planner refines each step of its way
// (EECPWallManager::PlanWall: EEPathManager::FindPath with the wall pather, EEAIGPGModifierComputerPlayerWallPather)
static int LuaWallPath(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    lua_newtable(L);
    for (int i = 2; i <= 5; i++)
        if (!lua_isinteger(L, i) || gWorld.mMap == NULL)
            return 1;
    EEComplexUnit* theUnit = lua_isinteger(L, 1) ? EECPUnitsOwn(me, (long)lua_tointeger(L, 1)) : NULL;
    if (theUnit == NULL)
        return 1;
    U2DPoint<long> theFrom((long)lua_tointeger(L, 2), (long)lua_tointeger(L, 3));
    U2DPoint<long> theTo((long)lua_tointeger(L, 4), (long)lua_tointeger(L, 5));
    EEAIAStarBounds theBounds;
    theBounds.Set(theFrom);
    theBounds.Extend(theTo);
    std::vector<U2DPoint<long> > theSegment;
    EEAIGPGModifierComputerPlayerWallPather* thePather = new EEAIGPGModifierComputerPlayerWallPather;
    gPathManager.FindPath(theUnit, gWorld.mMap, theFrom, theTo, theBounds, theSegment, 1, 0x32, thePather, NULL);
    thePather->Dereference();
    long theArea = ((theTo.mX > theFrom.mX ? theTo.mX - theFrom.mX : theFrom.mX - theTo.mX) + 1) * ((theTo.mY > theFrom.mY ? theTo.mY - theFrom.mY : theFrom.mY - theTo.mY) + 1);
    EECPUnitsCharge(L, kCostCall + theArea * kCostScan + (int64_t)theSegment.size() * kCostField * 2);
    for (size_t k = 0; k < theSegment.size(); k++) {
        lua_createtable(L, 2, 0);
        lua_pushinteger(L, (lua_Integer)theSegment[k].mX);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, (lua_Integer)theSegment[k].mY);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, (lua_Integer)k + 1);
    }
    return 1;
}

// ai.wall_route(enemy, x1, y1, x2, y2) -> handle: the classic wall planner's way from the enemy's centre (x1, y1) to
// the own town (x2, y2): the path manager's request on its coarse graph with the wall planner modifier (the enemy's
// influence costs) and a citizen's terrain (EECPWallManager::StartNextWall). The request runs over the next game
// ticks; ai.wall_route_result(handle) -> nil while it runs, the tiles {{x, y}, ...} when done (empty: no way), false
// for a handle it does not hold (read already, or a loaded game). One request a player at a time: a new one drops
// the old.
static int LuaWallRoute(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsCharge(L, kCostCommand);
    for (int i = 1; i <= 5; i++)
        if (!lua_isinteger(L, i) || gWorld.mMap == NULL) {
            lua_pushnil(L);
            return 1;
        }
    long theEnemy = (long)lua_tointeger(L, 1);
    if (theEnemy <= 0 || (unsigned long)theEnemy >= gWorld.mNumPlayers || gWorld.mPlayers[theEnemy] == NULL
        || gUnitClassTable[kUnitTypeCitizen] == NULL) {
        lua_pushnil(L);
        return 1;
    }
    for (std::map<long, EEAutoPathRequest*>::iterator it = s->mRoutes.begin(); it != s->mRoutes.end(); ++it)
        if (it->second != NULL)
            it->second->Dereference();
    s->mRoutes.clear();
    EECPASGFlags theFlags;
    gUnitClassTable[kUnitTypeCitizen]->GetTerrainFlags(NULL, theFlags);
    EEAIGPGModifierComputerPlayerWallPlanner* theModifier = new EEAIGPGModifierComputerPlayerWallPlanner(theEnemy, 0);
    EETileCoordinate theFrom;
    theFrom.mX = (long)lua_tointeger(L, 2);
    theFrom.mY = (long)lua_tointeger(L, 3);
    EETileCoordinate theTo;
    theTo.mX = (long)lua_tointeger(L, 4);
    theTo.mY = (long)lua_tointeger(L, 5);
    EEAutoPathRequest* theRequest = gPathManager.RequestPath(theEnemy, theFrom, theTo, &theFlags, 2, -1, theModifier, 1);
    theModifier->Dereference();
    if (theRequest == NULL) {
        lua_pushnil(L);
        return 1;
    }
    long theHandle = s->mNextRoute++;
    s->mRoutes[theHandle] = theRequest;
    lua_pushinteger(L, (lua_Integer)theHandle);
    return 1;
}

static int LuaWallRouteResult(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsCharge(L, kCostCall);
    std::map<long, EEAutoPathRequest*>::iterator it =
        lua_isinteger(L, 1) ? s->mRoutes.find((long)lua_tointeger(L, 1)) : s->mRoutes.end();
    if (it == s->mRoutes.end() || it->second == NULL) {
        lua_pushboolean(L, 0);
        return 1;
    }
    EEAutoPathRequest* theRequest = it->second;
    if (!theRequest->mIsDone) {
        lua_pushnil(L);
        return 1;
    }
    EECPUnitsCharge(L, (int64_t)theRequest->mPath.size() * kCostField * 2);
    lua_createtable(L, (int)theRequest->mPath.size(), 0);
    for (size_t k = 0; k < theRequest->mPath.size(); k++) {
        lua_createtable(L, 2, 0);
        lua_pushinteger(L, (lua_Integer)theRequest->mPath[k].mX);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, (lua_Integer)theRequest->mPath[k].mY);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, (lua_Integer)k + 1);
    }
    theRequest->Dereference();
    s->mRoutes.erase(it);
    return 1;
}

// ai.wall_line(x1, y1, x2, y2) -> {{x, y}, ...}: the tiles a wall from (x1, y1) to (x2, y2) lays (EEGetWallLine), in order
static int LuaWallLine(lua_State* L)
{
    lua_newtable(L);
    for (int i = 1; i <= 4; i++)
        if (!lua_isinteger(L, i))
            return 1;
    U2DPoint<long> theFrom((long)lua_tointeger(L, 1), (long)lua_tointeger(L, 2));
    U2DPoint<long> theTo((long)lua_tointeger(L, 3), (long)lua_tointeger(L, 4));
    std::vector<U2DPoint<long> > theTiles;
    EEGetWallLine(theFrom, theTo, theTiles);
    EECPUnitsCharge(L, kCostCall + (int64_t)theTiles.size() * kCostField * 2);
    for (size_t k = 0; k < theTiles.size(); k++) {
        lua_createtable(L, 2, 0);
        lua_pushinteger(L, (lua_Integer)theTiles[k].mX);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, (lua_Integer)theTiles[k].mY);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, (lua_Integer)k + 1);
    }
    return 1;
}

// ai.wall_stretches(x1, y1, x2, y2) -> blocked, {{ax, ay, bx, by, len, level}, ...}: the classic wall planner's look at
// one wall line (EECPWallManager::PlanWall, IsPathBlocked). blocked: a land building stands on the line or within 2
// tiles of it (the player's own wall pieces on the line excepted). The stretches: the runs of open tiles that follow
// a blocked tile (off the map, a forest, a cliff, water, space), each with its ends, its length and whether five
// level tiles in a row lie on it (EEAreTilesLevel)
static int LuaWallStretches(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    for (int i = 1; i <= 4; i++)
        if (!lua_isinteger(L, i) || gWorld.mMap == NULL) {
            lua_pushboolean(L, 1);
            lua_newtable(L);
            return 2;
        }
    U2DPoint<long> theFrom((long)lua_tointeger(L, 1), (long)lua_tointeger(L, 2));
    U2DPoint<long> theTo((long)lua_tointeger(L, 3), (long)lua_tointeger(L, 4));
    std::vector<U2DPoint<long> > theLine;
    EEGetWallLine(theFrom, theTo, theLine);
    EECPUnitsCharge(L, kCostCall + (int64_t)theLine.size() * 25 * kCostScan);
    // IsPathBlocked
    std::vector<U2DPoint<long> > theAll;
    std::vector<U2DPoint<long> > theAround;
    for (size_t k = 0; k < theLine.size(); k++) {
        theAll.push_back(theLine[k]);
        for (long r = 1; r <= 2; r++) {
            EEGetSquareRingTiles(theLine[k].mX, theLine[k].mY, r, theAround);
            theAll.insert(theAll.end(), theAround.begin(), theAround.end());
        }
    }
    std::sort(theAll.begin(), theAll.end());
    theAll.erase(std::unique(theAll.begin(), theAll.end()), theAll.end());
    std::vector<EEUnit*> theUnits;
    EEGetUnitsOn(theAll, theUnits);
    bool theBlocked = false;
    for (size_t k = 0; k < theUnits.size() && !theBlocked; k++) {
        EEUnit* u = theUnits[k];
        if (!u->mUnitClass->IsLandBuilding())
            continue;
        theBlocked = true;
        if (u->mPlayer == me && u->mUnitClass->mIsWall && u->mUnitClass->mCategory != kUnitCategoryGate
            && std::find(theLine.begin(), theLine.end(), U2DPoint<long>(u->mTileX, u->mTileY)) != theLine.end())
            theBlocked = false;
    }
    lua_pushboolean(L, theBlocked ? 1 : 0);
    lua_newtable(L);
    if (theBlocked)
        return 2;
    // the stretches between the blocks
    std::vector<std::pair<long, long> > theStretches;
    bool theInRun = false, theSeenBlock = false;
    long theRunStart = 0;
    for (size_t j = 0; j < theLine.size(); j++) {
        bool theBlock;
        if (gWorld.mMap->GetTile((unsigned long)theLine[j].mX, (unsigned long)theLine[j].mY) == NULL)
            theBlock = true;
        else if (gContinentManager.GetForestAt(theLine[j]) != NULL)
            theBlock = true;
        else {
            EEAIContinent* theArea = gContinentManager.GetContinentAt(theLine[j].mX, theLine[j].mY);
            unsigned char theKind = theArea != NULL ? theArea->GetCategory() : kContinentCategoryWater;
            theBlock = theKind == kContinentCategoryCliff || theKind == kContinentCategoryWater || theKind == kContinentCategorySpace;
        }
        if (theBlock) {
            if (theInRun) {
                theInRun = false;
                theStretches.push_back(std::make_pair(theRunStart, (long)j - 1));
            }
            theSeenBlock = true;
        } else if (theSeenBlock && !theInRun) {
            theInRun = true;
            theRunStart = (long)j;
        }
    }
    for (size_t k = 0; k < theStretches.size(); k++) {
        long a = theStretches[k].first, b = theStretches[k].second;
        bool theLevel = false;
        std::vector<U2DPoint<long> > theFive;
        for (long m = 0; a + m + 4 <= b && !theLevel; m++) {
            theFive.assign(theLine.begin() + a + m, theLine.begin() + a + m + 5);
            theLevel = EEAreTilesLevel(theFive);
        }
        lua_createtable(L, 0, 6);
        EECPUnitsSetInt(L, "ax", theLine[a].mX);
        EECPUnitsSetInt(L, "ay", theLine[a].mY);
        EECPUnitsSetInt(L, "bx", theLine[b].mX);
        EECPUnitsSetInt(L, "by", theLine[b].mY);
        EECPUnitsSetInt(L, "len", b - a + 1);
        EECPUnitsSetBool(L, "level", theLevel);
        lua_rawseti(L, -2, (lua_Integer)k + 1);
    }
    return 2;
}

// ai.choke_point(x, y, nearest, pick, seed) -> x, y, n: a random point of one of the continent manager's choke points
// (EEAIChokePoint) of the land mass at (x, y) - of the `nearest` ones to the tile (by their closest point) when
// nearest > 0, else of all n of them; the (pick mod count)-th (the classic defence ladder's choke point and
// FindTowerSite's); nil when the land has none
static int LuaChokePoint(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    for (int i = 1; i <= 5; i++)
        if (!lua_isinteger(L, i))
            return 0;
    long x = (long)lua_tointeger(L, 1), y = (long)lua_tointeger(L, 2), theNearest = (long)lua_tointeger(L, 3);
    long w, h;
    EECPUnitsMapSize(w, h);
    if (x < 0 || y < 0 || x >= w || y >= h)
        return 0;
    EEAIContinent* theLand = gContinentManager.GetContinentAt(x, y);
    if (theLand == NULL)
        return 0;
    std::vector<EEAIChokePoint*> theChokePoints;
    gContinentManager.GetChokePoints(theChokePoints, theLand->GetID(), false);
    EECPUnitsCharge(L, (int64_t)theChokePoints.size() * kCostScan * 4);
    long theTotal = (long)theChokePoints.size();
    if (theChokePoints.empty())
        return 0;
    if (theNearest > 0) {
        U2DPoint<long> theFrom(x, y);
        std::multimap<long, EEAIChokePoint*> theMap;
        while (!theChokePoints.empty()) {
            U2DPoint<long> theClosest = theChokePoints.back()->GetClosestPoint(theFrom);
            theMap.insert(std::make_pair((long)theClosest.DistanceToSquared(theFrom), theChokePoints.back()));
            theChokePoints.pop_back();
        }
        long theCount = 0;
        for (std::multimap<long, EEAIChokePoint*>::iterator it = theMap.begin(); it != theMap.end() && theCount < theNearest;
             ++it, ++theCount)
            theChokePoints.push_back(it->second);
    }
    long thePick = (long)(lua_tointeger(L, 4) % (lua_Integer)theChokePoints.size());
    if (thePick < 0)
        thePick += (long)theChokePoints.size();
    EEAIChokePoint* theChoke = theChokePoints[thePick];
    if (!(theChoke->GetNumTiles() > 0))
        return 0;
    URandom theRandom((unsigned long)lua_tointeger(L, 5));
    U2DPoint<long> thePoint = theChoke->GetTileSet().GetRandomPoint(theRandom);
    lua_pushinteger(L, (lua_Integer)thePoint.mX);
    lua_pushinteger(L, (lua_Integer)thePoint.mY);
    lua_pushinteger(L, (lua_Integer)theTotal);
    return 3;
}

// ai.area_point(id, seed) -> x, y: a random tile of the area (EEAIArea::GetRandomPoint), drawn from a stream seeded
// with the script's number (the game's random stream is left alone); nil for no such area or an empty one
static int LuaAreaPoint(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2))
        return 0;
    lua_Integer theID = lua_tointeger(L, 1);
    if (theID <= 0 || (size_t)theID >= gContinentManager.mContinents.size())
        return 0;
    EEAIContinent* theArea = gContinentManager.GetContinentByID((unsigned short)theID);
    if (theArea == NULL || !(theArea->GetNumTiles() > 0))
        return 0;
    URandom theRandom((unsigned long)lua_tointeger(L, 2));
    U2DPoint<long> thePoint = theArea->GetTileSet().GetRandomPoint(theRandom);
    lua_pushinteger(L, (lua_Integer)thePoint.mX);
    lua_pushinteger(L, (lua_Integer)thePoint.mY);
    return 2;
}

// ai.unexplored(x, y, {land = id, sea = false, grid = 8, away = {ax, ay}, skip = {{x, y}, ...}}) -> x, y: the unexplored grid
// point (every `grid` tiles) on the land mass nearest (x, y) (EECPNextArmy's scout), scored by its distance less
// half its distance from `away` (further from the town first); points within grid of a skip entry are left out.
// nil: nothing unexplored on that land
static int LuaUnexplored(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2) || gWorld.mMap == NULL)
        return 0;
    long x = (long)lua_tointeger(L, 1), y = (long)lua_tointeger(L, 2);
    long theLand = 0, theGrid = 8, ax = x, ay = y;
    EECPUnitsOptInt(L, 3, "land", theLand);
    EECPUnitsOptInt(L, 3, "grid", theGrid);
    long theKind = EECPUnitsOptBool(L, 3, "sea", false) ? kAIPathWaterBody : kAIPathLandMass;   // sea: over a water body
    theGrid = theGrid < 4 ? 4 : theGrid > 32 ? 32 : theGrid;
    s->mIds2.clear();
    if (lua_type(L, 3) == LUA_TTABLE) {
        lua_getfield(L, 3, "away");
        if (lua_type(L, -1) == LUA_TTABLE) {
            lua_rawgeti(L, -1, 1);
            lua_rawgeti(L, -2, 2);
            if (lua_isinteger(L, -2) && lua_isinteger(L, -1)) {
                ax = (long)lua_tointeger(L, -2);
                ay = (long)lua_tointeger(L, -1);
            }
            lua_pop(L, 2);
        }
        lua_pop(L, 1);
        lua_getfield(L, 3, "skip");
        if (lua_type(L, -1) == LUA_TTABLE) {
            lua_Integer n = (lua_Integer)lua_rawlen(L, -1);
            for (lua_Integer i = 1; i <= n && i <= 128; i++) {
                lua_rawgeti(L, -1, i);
                if (lua_type(L, -1) == LUA_TTABLE) {
                    lua_rawgeti(L, -1, 1);
                    lua_rawgeti(L, -2, 2);
                    if (lua_isinteger(L, -2) && lua_isinteger(L, -1)) {
                        s->mIds2.push_back((long)lua_tointeger(L, -2));
                        s->mIds2.push_back((long)lua_tointeger(L, -1));
                    }
                    lua_pop(L, 2);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
    if (theLand == 0)
        theLand = EECPUnitsAreaNear(x, y, theKind);
    long w, h;
    EECPUnitsMapSize(w, h);
    EECPUnitsCharge(L, kCostCall + (w / theGrid + 1) * (h / theGrid + 1) * 3);
    bool theFound = false;
    long theBest = 0, bx = -1, by = -1;
    for (long gy = theGrid / 2; gy < h; gy += theGrid)
        for (long gx = theGrid / 2; gx < w; gx += theGrid) {
            EETile* theTile = static_cast<EETile*>(gWorld.mMap->GetTile((unsigned long)gx, (unsigned long)gy));
            if (theTile == NULL || theTile->GetVisibility(me->mPlayerID) > 0)
                continue;
            if (gContinentManager.GetContinent(U2DPoint<long>(gx, gy), theKind) != theLand)
                continue;
            bool theSkip = false;
            for (unsigned long k = 0; k + 1 < s->mIds2.size() && !theSkip; k += 2)
                theSkip = EECPUnitsD2(gx, gy, s->mIds2[k], s->mIds2[k + 1]) <= theGrid * theGrid;
            if (theSkip)
                continue;
            long d1 = 0, d2 = 0;
            long q1 = EECPUnitsD2(gx, gy, x, y), q2 = EECPUnitsD2(gx, gy, ax, ay);
            while ((d1 + 1) * (d1 + 1) <= q1)
                d1++;
            while ((d2 + 1) * (d2 + 1) <= q2)
                d2++;
            long theScore = d1 - d2 / 2;
            if (!theFound || theScore < theBest) {
                theFound = true;
                theBest = theScore;
                bx = gx;
                by = gy;
            }
        }
    if (!theFound)
        return 0;
    lua_pushinteger(L, (lua_Integer)bx);
    lua_pushinteger(L, (lua_Integer)by);
    return 2;
}

// ai.used() -> the instructions and charges this call has used so far (a multiple of the hook step, plus the helpers'
// charges), against the budget (ai.used's second value): a script's own profiling; it changes nothing
static int LuaUsed(lua_State* L)
{
    return ee_ai_lua_used_binding(L);   // ee_ai lua_host.rs
}

// ---------------------------------------------------------------------------------------------------------------
// More commands (batch 2)

// the tech node of a unit class the player may make now; else the refusal's reason
static EETechTreeNode* EECPNextClassNode(EEComputerPlayer* inPlayer, long inType, long& outReason)
{
    outReason = kNextReasonUnknownClass;
    if (inType < 0 || (unsigned long)inType >= EEUnitClassCount() || gUnitClassTable[inType] == NULL
        || inPlayer->mTechnologies == NULL)
        return NULL;
    EETechTreeNode* theNode = inPlayer->mTechnologies->Get(gUnitClassTable[inType]->mTechnology);
    if (theNode == NULL || theNode->mData == NULL)
        return NULL;
    outReason = kNextReasonNotInEpoch;
    if (!theNode->GetIsAvailable() || theNode->GetIsObsolete())
        return NULL;
    outReason = kNextReasonNone;
    return theNode;
}


// node now or no such improvement (ai.improve_cost, docs/AI_LUA_UNITS.md §11)
bool EECPNextImproveCost(EEComputerPlayer* inPlayer, long inType, long inImprovement, long outCost[5], long& outLevel,
                         long& outMax)
{
    long theReason;
    EETechTreeNode* theNode = EECPNextClassNode(inPlayer, inType, theReason);
    if (theNode == NULL)
        return false;
    EEUnitClass* theClass = gUnitClassTable[inType];
    if (inImprovement < 0 || inImprovement >= 16 || theClass->mImprovements == NULL)
        return false;
    EEUnitClassUpgrade* theUpgrade = inPlayer->mUnitClassUpgrades[inType];
    outLevel = inPlayer->GetImprovementLevel(inType, inImprovement);
    outMax = theClass->mImprovements->GetMaxLevel();
    float theFactor = theClass->mImprovements->mEntries[inImprovement].mCostFactor * (float)outLevel;
    for (long i = 0; i < kNextResources; i++)
        outCost[i] = inPlayer->ScaleCost(UMFloatToLong((float)theNode->mData->mCosts[kResourceFood + i] * theUpgrade->mCostScale * theFactor));
    return true;
}


// ai.improve(unit, n): one level of improvement n (dbupgrade's order: the unit panel's buttons) for the unit's class
static int LuaImprove(lua_State* L)
{
    EECPUnitsCharge(L, kCostCommand);
    EEComputerPlayer* me = EECPUnitsMe(L);
    long theID = lua_isinteger(L, 1) ? (long)lua_tointeger(L, 1) : 0;
    EEComplexUnit* theUnit = EECPUnitsOwn(me, theID);
    if (theUnit == NULL || !lua_isinteger(L, 2) || lua_tointeger(L, 2) < 0 || lua_tointeger(L, 2) > 15)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    long theImprovement = (long)lua_tointeger(L, 2);
    if (!me->CanBuy(theUnit->mUnitClass->GetUnitType(), theImprovement, NULL))
        return EECPUnitsResult(L, kCannotAfford, 0, 0);
    EEMRecord theMessage(kMessageImproveUnit, {(long)theUnit->mUnitID, theImprovement});
    unsigned char theSlot = me->mSlot;
    me->mSlot = kBorrowedSlot;
    theMessage.SetOriginatingSlot(kBorrowedSlot);
    theMessage.Execute();
    me->mSlot = theSlot;
    return EECPUnitsResult(L, kOK, 1, 0);
}

// ai.improve_cost(class, what) -> {food=, wood=, stone=, gold=, iron=, level=, max=}: the next level's cost of a class's
// improvement (what: "attack", "hit_points", "range", "speed" or the improvement's number) as the improve verb pays
// it, or nil (no node now, no such improvement). A human reads it off the improvement's button.
bool EECPNextImproveCost(EEComputerPlayer* inPlayer, long inType, long inImprovement, long outCost[5], long& outLevel,
                         long& outMax);

static int LuaImproveCost(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    EEComputerPlayer* me = EECPUnitsMe(L);
    long theType = EECPUnitsClassArg(L, 1);
    long theWhat = -1;
    if (lua_isinteger(L, 2))
        theWhat = (long)lua_tointeger(L, 2);
    else if (lua_type(L, 2) == LUA_TSTRING) {
        const char* theName = lua_tostring(L, 2);
        theWhat = strcmp(theName, "range") == 0 ? 11 : strcmp(theName, "attack") == 0 ? 1
            : strcmp(theName, "hit_points") == 0 ? 9 : strcmp(theName, "speed") == 0 ? 14 : -1;
    }
    long theCost[5] = {0, 0, 0, 0, 0}, theLevel = 0, theMax = 0;
    if (theType < 0 || theWhat < 0 || !EECPNextImproveCost(me, theType, theWhat, theCost, theLevel, theMax))
        return 0;
    static const char* const kNames[5] = {"food", "wood", "stone", "gold", "iron"};
    lua_createtable(L, 0, 7);
    for (long i = 0; i < 5; i++)
        EECPUnitsSetInt(L, kNames[i], theCost[i]);
    EECPUnitsSetInt(L, "level", theLevel);
    EECPUnitsSetInt(L, "max", theMax);
    return 1;
}

// ---------------------------------------------------------------------------------------------------------------
// Walls, gates, diplomacy (Age of AI's defence.c walls, diplo.c): the human's messages, as the orders above

bool EEDoesWallGateFitOnUnit(EEComplexUnit* inUnit, long inSide, long inUnitType, std::vector<EEUnit*>& outUnits);   // EECPWallManager.cpp

// ai.gate(type, x, y): a gate of the class over the finished wall pieces at the tile, there at once as the human's
// gate button lays it (EEBuildGate: the five pieces turned into the gate's parts); the side is the first the gate
// fits on, as the classic wall manager picks it. Refused cannot_place where no wall of five finished pieces lies
static int LuaGate(lua_State* L)
{
    EECPUnitsCharge(L, kCostCommand);
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    long theType = (lua_type(L, 1) == LUA_TNIL || (lua_type(L, 1) == LUA_TSTRING && strcmp(lua_tostring(L, 1), "gate") == 0))
        && me->mTechnologies != NULL ? EECPGateClass(me) : EECPUnitsClassArg(L, 1);
    if (theType < 0 || (unsigned long)theType >= EEUnitClassCount() || gUnitClassTable[theType] == NULL
        || EEUnitClassOfKind<EE_CLASS_KIND_GATE>(gUnitClassTable[theType]) == NULL || !lua_isinteger(L, 2) || !lua_isinteger(L, 3))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    long x = (long)lua_tointeger(L, 2), y = (long)lua_tointeger(L, 3);
    if (gWorld.mMap == NULL || gWorld.mMap->GetTile((unsigned long)x, (unsigned long)y) == NULL)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    long theTech = gUnitClassTable[theType]->mTechnology;
    s->mIds2.clear();
    EETechTreeNode* theNode = me->mTechnologies->Get(theTech);
    if (theNode == NULL) {
        EECPUnitsTraceCmd(me, s, "gate", kNotInEpoch, 0, theType, 0);
        return EECPUnitsResult(L, kNotInEpoch, 0, 0);
    }
    std::vector<EEUnit*> theUnits;
    EEGetUnitsOnTile(x, y, theUnits);
    EEUnit* theWall = NULL;
    for (unsigned long k = 0; k < theUnits.size() && theWall == NULL; k++)
        if (theUnits[k]->GetUnitClass()->GetIsWall() && theUnits[k]->GetHitPoints() > 0 && theUnits[k]->mPlayer == me
            && theUnits[k]->GetComplexUnit() != NULL && theUnits[k]->GetComplexUnit()->GetIsBuilt())
            theWall = theUnits[k];
    long theSide = 0;
    if (theWall != NULL) {
        long theWallType = theWall->GetUnitClass()->GetUnitType();
        for (long side = 1; side <= 4 && theSide == 0; side++) {
            theUnits.clear();
            if (EEDoesWallGateFitOnUnit(static_cast<EEComplexUnit*>(theWall), side, theWallType, theUnits))
                theSide = side;
        }
    }
    if (theSide == 0) {
        EECPUnitsTraceCmd(me, s, "gate", kCannotPlace, 0, theType, x * 10000 + y);
        return EECPUnitsResult(L, kCannotPlace, 0, 0);
    }
    EEMRecord theMessage(kMessageBuildGate, {GE2DPoint((float)x, (float)y), theTech, theSide, (long)me->mPlayerID});
    theMessage.Execute();
    EECPUnitsTraceCmd(me, s, "gate", kOK, 1, theType, x * 10000 + y);
    return EECPUnitsResult(L, kOK, 1, 0);
}

static long EECPUnitsOptAmount(lua_State* L, int inTable, const char* inKey)
{
    return ee_ai_units_opt_amount(L, inTable, inKey);
}

// ai.tribute(player, {food=, wood=, stone=, gold=, iron=}): the resources given to another player as a human's
// tribute (EEMPayTribute's moves and ledger, without its chat line); refused cannot_afford when any amount is
// beyond the stock, bad_argument for a player not in the game
static int LuaTribute(lua_State* L)
{
    EECPUnitsCharge(L, kCostCommand);
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    s->mIds2.clear();
    long theOther = lua_isinteger(L, 1) ? (long)lua_tointeger(L, 1) : -1;
    if (theOther <= 0 || (unsigned long)theOther >= gWorld.mNumPlayers || gWorld.mPlayers[theOther] == NULL
        || gWorld.mPlayers[theOther] == me || lua_type(L, 2) != LUA_TTABLE)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    EEPlayer* theTo = gWorld.mPlayers[theOther];
    static const char* const kKeys[5] = {"food", "wood", "stone", "gold", "iron"};
    static const long kIndex[5] = {kResourceFood, kResourceWood, kResourceStone, kResourceGold, kResourceIron};
    long theAmount[5], theTotal = 0;
    for (int k = 0; k < 5; k++) {
        theAmount[k] = EECPUnitsOptAmount(L, 2, kKeys[k]);
        if (theAmount[k] > me->GetResource(kIndex[k])) {
            EECPUnitsTraceCmd(me, s, "tribute", kCannotAfford, 0, theOther, theAmount[k]);
            return EECPUnitsResult(L, kCannotAfford, 0, 0);
        }
        theTotal += theAmount[k];
    }
    if (theTotal <= 0)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    for (int k = 0; k < 5; k++)
        if (theAmount[k] > 0) {
            me->TakeResource(kIndex[k], theAmount[k]);
            me->AddTribute(0, kIndex[k], theAmount[k]);
            theTo->AddResource(kIndex[k], theAmount[k]);
            theTo->AddTribute(1, kIndex[k], theAmount[k]);
        }
    EECPUnitsTraceCmd(me, s, "tribute", kOK, 1, theOther, theTotal);
    return EECPUnitsResult(L, kOK, 1, 0);
}

// ai.trade(resource, "buy" or "sell", n): n lots of 100 of the resource bought with gold or sold for it at the
// market's price (EEMExchangeResources: 1 or 5 lots a call)
static int LuaTrade(lua_State* L)
{
    EECPUnitsCharge(L, kCostCommand);
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    s->mIds2.clear();
    long theResource = -1;
    if (lua_type(L, 1) == LUA_TSTRING) {
        static const char* const kKeys[4] = {"food", "wood", "stone", "iron"};
        static const long kIndex[4] = {kResourceFood, kResourceWood, kResourceStone, kResourceIron};
        for (int k = 0; k < 4; k++)
            if (strcmp(lua_tostring(L, 1), kKeys[k]) == 0)
                theResource = kIndex[k];
    }
    const char* theWay = lua_type(L, 2) == LUA_TSTRING ? lua_tostring(L, 2) : "";
    bool theSell = strcmp(theWay, "sell") == 0;
    long n = lua_isinteger(L, 3) ? (long)lua_tointeger(L, 3) : 1;
    if (theResource < 0 || (!theSell && strcmp(theWay, "buy") != 0) || (n != 1 && n != 5))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    if (theSell ? me->GetResource(theResource) <= 0 : me->GetResource(kResourceGold) <= 0) {
        EECPUnitsTraceCmd(me, s, "trade", kCannotAfford, 0, theResource, n);
        return EECPUnitsResult(L, kCannotAfford, 0, 0);
    }
    EEMRecord theMessage(kMessageExchangeResources, {theSell ? 1 : 0, theResource, n == 5 ? 1 : 0});
    unsigned char theSlot = me->mSlot;
    me->mSlot = kBorrowedSlot;
    if (gWorld.GetPlayerBySlot(kBorrowedSlot) == me) {
        theMessage.SetOriginatingSlot(kBorrowedSlot);
        theMessage.Execute();
    }
    me->mSlot = theSlot;
    EECPUnitsTraceCmd(me, s, "trade", kOK, n, theResource, theSell ? 1 : 0);
    return EECPUnitsResult(L, kOK, n, 0);
}

// ai.diplomacy(player, "ally" or "enemy"): the stance towards another player (EEWorld::SetDiplomaticStance, 0 the
// ally: an alliance once both sides stand allied, broken when one turns)
static int LuaDiplomacy(lua_State* L)
{
    EECPUnitsCharge(L, kCostCommand);
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    s->mIds2.clear();
    long theOther = lua_isinteger(L, 1) ? (long)lua_tointeger(L, 1) : -1;
    const char* theStance = lua_type(L, 2) == LUA_TSTRING ? lua_tostring(L, 2) : "";
    bool theAlly = strcmp(theStance, "ally") == 0;
    if (theOther <= 0 || (unsigned long)theOther >= gWorld.mNumPlayers || gWorld.mPlayers[theOther] == NULL
        || gWorld.mPlayers[theOther] == me || (!theAlly && strcmp(theStance, "enemy") != 0))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    gWorld.SetDiplomaticStance(me->mPlayerID, theOther, theAlly ? 0 : 1);
    EECPUnitsTraceCmd(me, s, "diplomacy", kOK, 1, theOther, theAlly ? 0 : 1);
    return EECPUnitsResult(L, kOK, 1, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// The seat's tuning and the tech tree (the classic AI's data; docs/AI_LUA_UNITS.md §11, the classic port)

// a float as an integer in thousandths (the interface carries integers only)
static long EECPUnitsMilli(float inValue)
{
    return UMFloatToLong(inValue * 1000.0f);
}

// ai.tuning() -> what the classic computer player reads from data for this seat: the difficulty, whether it is a
// deathmatch, the cheats on (gameoptions.yaml ai_cheats, EE_AI_CHEATS), the personality (aipersonalities.yaml, the
// EE_AI_PERSONALITY override), the map's kind, the game's epoch span and the behaviour record values the computer
// player copied in EEComputerPlayer::Init (dbcpbehavior). Floats are thousandths (keys ending in _k)
static int LuaTuning(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall + 40 * kCostRecord / 10);
    EEComputerPlayer* me = EECPUnitsMe(L);
    lua_createtable(L, 0, 16);
    EECPUnitsSetInt(L, "difficulty", (long)me->mDifficulty);
    EECPUnitsSetBool(L, "deathmatch", me->IsDeathmatch());
    EECPUnitsSetBool(L, "all_managers", gWorld.mGameOptions.mResources == 5);
    EECPUnitsSetInt(L, "epoch_start", gWorld.mGameOptions.mEpochStart);
    EECPUnitsSetInt(L, "epoch_end", gWorld.mGameOptions.mEpochEnd);
    EEMap* theMap = gWorld.mMap;
    unsigned long theKind = theMap != NULL ? theMap->mMapCharacteristics : 0;
    EECPUnitsSetBool(L, "island", (theKind & kRMMapIslandMap) != 0);
    EECPUnitsSetBool(L, "planets", (theKind & kRMMapPlanetsMap) != 0);
    EECPUnitsSetBool(L, "waterless", (theKind & kRMMapWaterlessMap) != 0);
    EECPUnitsSetBool(L, "space", theMap != NULL && theMap->mTerrainMesh.GetIsSpaceMap());
    // the map's tiles by kind (the continent manager's counts: the classic's domain shares weigh the sea by them)
    EECPUnitsSetInt(L, "water_tiles", gContinentManager.GetNumWaterTiles());
    EECPUnitsSetInt(L, "land_tiles", gContinentManager.GetNumLandTiles());
    EECPUnitsSetInt(L, "space_tiles", gContinentManager.GetNumSpaceTiles());
    EECPUnitsSetInt(L, "largest_sea", gContinentManager.GetLargestContinent(kContinentCategoryWater));
    EECPUnitsSetBool(L, "fills_garrison", EECPEngineFillsGarrison(me));   // the manifest's "fills_garrison" (ai.garrison_fill)
    EECPUnitsSetBool(L, "sees_all", EECPEngineSeesAll(me));   // the manifest's "sees_all" (ai.enemy_near reads through the fog)
    EECPUnitsSetInt(L, "population_limit", me->EEPlayer::GetPopulationLimit(0));   // the game's (the AI's cap: the view)
    // the cheats
    lua_createtable(L, 0, 7);
    static const char* const kCheats[7] = {"cost", "reveal", "epochs", "extra", "popband", "catchup", "retire"};
    for (long k = 0; k < 7; k++)
        EECPUnitsSetBool(L, kCheats[k], EECPCheatOn(me, 1L << k));
    lua_setfield(L, -2, "cheats");
    // the personality (with the TEST ONLY experiments' switches the C++ classic honours)
    const EEAIPersonalityRule& p = EECPPersonality(me);
    lua_createtable(L, 0, 32);
    EECPUnitsSetStr(L, "id", p.mID.c_str());
    EECPUnitsSetInt(L, "epoch_pressure_start_k", EECPUnitsMilli(p.mEpochPressureStart));
    EECPUnitsSetInt(L, "epoch_pressure_full_k", EECPUnitsMilli(p.mEpochPressureFull));
    EECPUnitsSetInt(L, "stone_span_k", EECPUnitsMilli(EECPStoneSpan(me)));
    EECPUnitsSetInt(L, "safe_bonus_k", EECPUnitsMilli(EECPSafeBonus(me)));
    EECPUnitsSetInt(L, "surplus", p.mSurplus);
    EECPUnitsSetInt(L, "surplus_cut_k", EECPUnitsMilli(p.mSurplusCut));
    EECPUnitsSetInt(L, "low_stock", p.mLowStock);
    lua_createtable(L, 0, 5);
    static const char* const kNames[5] = {"food", "wood", "stone", "gold", "iron"};
    for (long k = 0; k < 5; k++)
        EECPUnitsSetInt(L, kNames[k], EECPUnitsMilli(p.mFloor[k]));
    lua_setfield(L, -2, "floor_k");
    EECPUnitsSetInt(L, "max_citizen_share_k", EECPUnitsMilli(p.mMaxCitizenShare));
    EECPUnitsSetInt(L, "wood_slots", p.mWoodSlots);
    EECPUnitsSetInt(L, "moves_per_cycle", p.mMovesPerCycle);
    EECPUnitsSetBool(L, "demand_split", EECPDemandSplitOn(me));
    EECPUnitsSetBool(L, "coast_fishing", EECPCoastFishingOn(me));
    EECPUnitsSetBool(L, "forage_patches", EECPForagePatchesOn(me));
    EECPUnitsSetBool(L, "claim_range", EECPClaimRangeOn(me));
    EECPUnitsSetBool(L, "claim_needed", EECPClaimNeededOn(me));
    EECPUnitsSetInt(L, "early_town_centers", EECPEarlyTownCenters(me));
    EECPUnitsSetInt(L, "early_trainers", EECPEarlyTrainers(me));
    EECPUnitsSetBool(L, "patch_expand", EECPPatchExpandOn(me));
    EECPUnitsSetInt(L, "standing_army_k", EECPUnitsMilli(p.mStandingArmy));
    EECPUnitsSetInt(L, "army_floor_k", EECPUnitsMilli(p.mArmyFloor));
    EECPUnitsSetBool(L, "militia", EECPMilitiaOn(me));
    EECPUnitsSetBool(L, "militia_towers", EECPMilitiaTowersOn(me));
    EECPUnitsSetInt(L, "prehistoric_army", EECPPrehistoricArmy(me));
    EECPUnitsSetBool(L, "prehistoric_attacks", !EECPPrehistoricPeaceOn(me));
    EECPUnitsSetBool(L, "stone_attacks", !EECPStonePeaceOn(me));
    EECPUnitsSetBool(L, "naval_flee", EECPNavalFleeOn(me));
    lua_setfield(L, -2, "personality");
    // the behaviour record values (EEComputerPlayer::Init; a scenario's SetAIVariable may have changed them)
    lua_createtable(L, 0, 180);
#define EE_TUNE_INT(name, field) EECPUnitsSetInt(L, name, (long)me->field)
#define EE_TUNE_K(name, field) EECPUnitsSetInt(L, name, EECPUnitsMilli((float)me->field))
    EE_TUNE_INT("assist_allies_delay_easy", mAssistAlliesDelayEasy);
    EE_TUNE_INT("assist_allies_delay_medium", mAssistAlliesDelayMedium);
    EE_TUNE_INT("assist_allies_delay_hard", mAssistAlliesDelayHard);
    EE_TUNE_K("naval_unit_weight_k", mNavalUnitWeight);
    EE_TUNE_K("air_unit_weight_k", mAirUnitWeight);
    EE_TUNE_INT("max_flame_throwers", mMaxFlameThrowers);
    EE_TUNE_K("unit_cost_medium_k", mUnitCostMedium);
    EE_TUNE_INT("carrier_max_per_group", mCarrierMaxPerGroup);
    EE_TUNE_K("carrier_pop_percentage_k", mCarrierPopPercentage);
    EE_TUNE_K("carrier_chance_k", mCarrierChance);
    EE_TUNE_INT("num_air_explorers", mNumAirExplorers);
    EE_TUNE_K("computer_aggression_k", mComputerAggression);
    EE_TUNE_INT("attack_enemy_chance", mAttackEnemyChance);
    EE_TUNE_INT("attack_to_claim_resource_chance", mAttackToClaimResourceChance);
    EE_TUNE_INT("attack_for_revenge_chance", mAttackForRevengeChance);
    EE_TUNE_INT("attack_group_size_min", mAttackGroupSizeMin);
    EE_TUNE_INT("attack_group_size_max", mAttackGroupSizeMax);
    EE_TUNE_K("attack_frequency_per_epoch_k", mAttackFrequencyPerEpoch);
    EE_TUNE_INT("attack_frequency_modifier_easy", mAttackFrequencyModifierEasy);
    EE_TUNE_INT("attack_frequency_modifier_hard", mAttackFrequencyModifierHard);
    EE_TUNE_INT("attack_frequency_floor", mAttackFrequencyFloor);
    EE_TUNE_INT("attack_frequency_ceiling", mAttackFrequencyCeiling);
    EE_TUNE_INT("attack_frequency_min", mAttackFrequencyMin);
    EE_TUNE_INT("attack_frequency_max", mAttackFrequencyMax);
    EE_TUNE_INT("initial_attack_delay_per_epoch", mInitialAttackDelayPerEpoch);
    EE_TUNE_INT("attack_delay_modifier_easy", mAttackDelayModifierEasy);
    EE_TUNE_INT("attack_delay_modifier_hard", mAttackDelayModifierHard);
    EE_TUNE_INT("attack_delay_floor", mAttackDelayFloor);
    EE_TUNE_INT("attack_delay_ceiling", mAttackDelayCeiling);
    EE_TUNE_INT("initial_attack_delay_min", mInitialAttackDelayMin);
    EE_TUNE_INT("initial_attack_delay_max", mInitialAttackDelayMax);
    EE_TUNE_K("specialized_unit_percentage_k", mSpecializedUnitPercentage);
    EE_TUNE_INT("attack_group_modifier_easy", mAttackGroupModifierEasy);
    EE_TUNE_INT("attack_group_modifier_medium", mAttackGroupModifierMedium);
    EE_TUNE_INT("attack_group_modifier_hard", mAttackGroupModifierHard);
    EE_TUNE_K("rps_selection_non_islands_k", mRPSSelectionNonIslands);
    EE_TUNE_K("rps_selection_islands_k", mRPSSelectionIslands);
    EE_TUNE_K("specialized_unit_chance_k", mSpecializedUnitChance);
    EE_TUNE_INT("bonzai_attack_level", mBonzaiAttackLevel);
    EE_TUNE_INT("dm_early_attack_time", mDMEarlyAttackTime);
    EE_TUNE_K("dm_early_attack_scale_k", mDMEarlyAttackScale);
    EE_TUNE_K("town_guard_percentage_k", mTownGuardPercentage);
    EE_TUNE_K("citizen_percentage_k", mCitizenPercentage);
    EE_TUNE_INT("unit_cost_reduction_easy", mUnitCostReductionEasy);
    EE_TUNE_INT("unit_cost_reduction_hard", mUnitCostReductionHard);
    EE_TUNE_INT("defense_group_size_min", mDefenseGroupSizeMin);
    EE_TUNE_INT("defense_group_size_max", mDefenseGroupSizeMax);
    EE_TUNE_INT("epoch_frequency_min", mEpochFrequencyMin);
    EE_TUNE_INT("epoch_frequency_max", mEpochFrequencyMax);
    EE_TUNE_INT("epoch_catch_up_min", mEpochCatchUpMin);
    EE_TUNE_INT("epoch_catch_up_max", mEpochCatchUpMax);
    EE_TUNE_K("epoch_catch_up_easy_k", mEpochCatchUpEasy);
    EE_TUNE_K("epoch_catch_up_hard_k", mEpochCatchUpHard);
    EE_TUNE_K("epoch_catch_up_deathmatch_k", mEpochCatchUpDeathmatch);
    EE_TUNE_INT("epoch_frequency_modifier_easy", mEpochFrequencyModifierEasy);
    EE_TUNE_INT("epoch_frequency_modifier_hard", mEpochFrequencyModifierHard);
    EE_TUNE_INT("expansion_timer_easy", mExpansionTimerEasy);
    EE_TUNE_INT("expansion_timer_medium", mExpansionTimerMedium);
    EE_TUNE_INT("expansion_timer_hard", mExpansionTimerHard);
    EE_TUNE_K("guard_continent_percentage_k", mGuardContinentPercentage);
    EE_TUNE_K("guard_chokepoint_air_percentage_k", mGuardChokepointAirPercentage);
    EE_TUNE_K("guard_chokepoint_percentage_k", mGuardChokepointPercentage);
    EE_TUNE_K("guard_high_ground_percentage_k", mGuardHighGroundPercentage);
    EE_TUNE_K("guard_town_percentage_k", mGuardTownPercentage);
    EE_TUNE_K("guard_town_land_percentage_k", mGuardTownLandPercentage);
    EE_TUNE_K("guard_waterway_percentage_k", mGuardWaterwayPercentage);
    EE_TUNE_INT("hero_max_per_group", mHeroMaxPerGroup);
    EE_TUNE_K("hero_pop_percentage_k", mHeroPopPercentage);
    EE_TUNE_K("hero_chance_k", mHeroChance);
    EE_TUNE_INT("num_improved_unit_types", mNumImprovedUnitTypes);
    EE_TUNE_INT("num_improved_attributes", mNumImprovedAttributes);
    EE_TUNE_INT("replenish_delay_min", mReplenishDelayMin);
    EE_TUNE_INT("replenish_delay_max", mReplenishDelayMax);
    EE_TUNE_INT("num_land_explorers", mNumLandExplorers);
    EE_TUNE_INT("num_sea_explorers", mNumSeaExplorers);
    EE_TUNE_INT("num_choke_points_to_guard", mNumChokePointsToGuard);
    EE_TUNE_INT("anti_air_max", mAntiAirMax);
    EE_TUNE_INT("airport_max", mAirportMax);
    EE_TUNE_INT("archery_range_max", mArcheryRangeMax);
    EE_TUNE_INT("barracks_max", mBarracksMax);
    EE_TUNE_INT("dock_max", mDockMax);
    EE_TUNE_INT("granary_max", mGranaryMax);
    EE_TUNE_INT("hospital_max", mHospitalMax);
    EE_TUNE_INT("cyber_factory_max", mCyberFactoryMax);
    EE_TUNE_INT("cyber_lab_max", mCyberLabMax);
    EE_TUNE_INT("naval_yard_max", mNavalYardMax);
    EE_TUNE_INT("university_max", mUniversityMax);
    EE_TUNE_INT("siege_factory_max", mSiegeFactoryMax);
    EE_TUNE_INT("space_dock_max", mSpaceDockMax);
    EE_TUNE_INT("stable_max", mStableMax);
    EE_TUNE_INT("tank_factory_max", mTankFactoryMax);
    EE_TUNE_INT("temple_max", mTempleMax);
    EE_TUNE_INT("tower_max", mTowerMax);
    EE_TUNE_K("defensive_unit_cap_k", mDefensiveUnitCap);
    EE_TUNE_K("siege_weapon_percentage_k", mSiegeWeaponPercentage);
    EE_TUNE_K("max_attack_response_range_k", mMaxAttackResponseRange);
    EE_TUNE_INT("min_military_buildings_easy", mMinMilitaryBuildingsEasy);
    EE_TUNE_INT("min_military_buildings_easy_dm", mMinMilitaryBuildingsEasyDM);
    EE_TUNE_INT("min_military_buildings_medium", mMinMilitaryBuildingsMedium);
    EE_TUNE_INT("min_military_buildings_medium_dm", mMinMilitaryBuildingsMediumDM);
    EE_TUNE_INT("min_military_buildings_hard", mMinMilitaryBuildingsHard);
    EE_TUNE_INT("min_military_buildings_hard_dm", mMinMilitaryBuildingsHardDM);
    EE_TUNE_K("military_pop_percentage_k", mMilitaryPopPercentage);
    EE_TUNE_K("aid_threat_threshold_k", mAidThreatThreshold);
    EE_TUNE_INT("pop_limit_adjustment_easy", mPopLimitAdjustmentEasy);
    EE_TUNE_INT("pop_limit_adjustment_medium", mPopLimitAdjustmentMedium);
    EE_TUNE_INT("pop_limit_adjustment_hard", mPopLimitAdjustmentHard);
    EE_TUNE_INT("priest_max_per_group", mPriestMaxPerGroup);
    EE_TUNE_K("priest_pop_percentage_k", mPriestPopPercentage);
    EE_TUNE_K("priest_chance_k", mPriestChance);
    EE_TUNE_INT("prophet_max_per_group", mProphetMaxPerGroup);
    EE_TUNE_K("prophet_pop_percentage_k", mProphetPopPercentage);
    EE_TUNE_K("prophet_chance_k", mProphetChance);
    EE_TUNE_INT("research_frequency", mResearchFrequency);
    EE_TUNE_INT("research_frequency_modifier_easy", mResearchFrequencyModifierEasy);
    EE_TUNE_INT("research_frequency_modifier_hard", mResearchFrequencyModifierHard);
    EE_TUNE_INT("drop_off_distance", mDropOffDistance);
    EE_TUNE_INT("extra_resources", mExtraResources);
    EE_TUNE_INT("town_center_upgrade_timer", mTownCenterUpgradeTimer);
    EE_TUNE_INT("capitol_upgrade_timer", mCapitolUpgradeTimer);
    EE_TUNE_INT("max_enabled_managers", mMaxEnabledManagers);
    EE_TUNE_INT("manager_attack_wait", mManagerAttackWait);
    EE_TUNE_INT("manager_epoch_advance_wait", mManagerEpochAdvanceWait);
    EE_TUNE_INT("enable_stock_production", mEnableStockProduction);
    EE_TUNE_INT("enable_stock_resource", mEnableStockResource);
    EE_TUNE_INT("enable_stock_civilization", mEnableStockCivilization);
    EE_TUNE_INT("enable_stock_exploration", mEnableStockExploration);
    EE_TUNE_INT("enable_stock_defense", mEnableStockDefense);
    EE_TUNE_INT("enable_stock_build", mEnableStockBuild);
    EE_TUNE_INT("enable_stock_5", mLE80);
    EE_TUNE_INT("enable_stock_research", mEnableStockResearch);
    EE_TUNE_INT("enable_stock_combat", mEnableStockCombat);
    EE_TUNE_K("enable_interval_k", mEnableInterval);
    EE_TUNE_K("military_count_weight_k", mMilitaryCountWeight);
    EE_TUNE_K("worker_count_weight_k", mWorkerCountWeight);
    EE_TUNE_INT("units_per_anti_air", mUnitsPerAntiAirBuilding);
    EE_TUNE_INT("units_per_airport", mUnitsPerAirport);
    EE_TUNE_INT("units_per_archery_range", mUnitsPerArcheryRange);
    EE_TUNE_INT("units_per_barracks", mUnitsPerBarracks);
    EE_TUNE_INT("units_per_dock", mUnitsPerDock);
    EE_TUNE_INT("units_per_granary", mUnitsPerGranary);
    EE_TUNE_INT("units_per_hospital", mUnitsPerHospital);
    EE_TUNE_INT("units_per_cyber_factory", mUnitsPerCyberFactory);
    EE_TUNE_INT("units_per_cyber_lab", mUnitsPerCyberLab);
    EE_TUNE_INT("units_per_naval_yard", mUnitsPerNavalYard);
    EE_TUNE_INT("units_per_university", mUnitsPerUniversity);
    EE_TUNE_INT("units_per_siege_factory", mUnitsPerSiegeFactory);
    EE_TUNE_INT("units_per_space_dock", mUnitsPerSpaceDock);
    EE_TUNE_INT("units_per_stable", mUnitsPerStable);
    EE_TUNE_INT("units_per_tank_factory", mUnitsPerTankFactory);
    EE_TUNE_INT("units_per_temple", mUnitsPerTemple);
    EE_TUNE_INT("units_per_tower", mUnitsPerTower);
    EE_TUNE_K("tower_spacing_k", mTowerSpacing);
    EE_TUNE_INT("ships_to_maintain_dm_easy", mShipsToMaintainDMEasy);
    EE_TUNE_INT("ships_to_maintain_dm_medium", mShipsToMaintainDMMedium);
    EE_TUNE_INT("ships_to_maintain_dm_hard", mShipsToMaintainDMHard);
    EE_TUNE_INT("ships_to_maintain_easy", mShipsToMaintainEasy);
    EE_TUNE_INT("ships_to_maintain_medium", mShipsToMaintainMedium);
    EE_TUNE_INT("ships_to_maintain_hard", mShipsToMaintainHard);
    EE_TUNE_INT("ship_maintenance_timer", mShipMaintenanceTimer);
    EE_TUNE_INT("defensive_unit_update_easy", mDefensiveUnitUpdateEasy);
    EE_TUNE_INT("defensive_unit_update_medium", mDefensiveUnitUpdateMedium);
    EE_TUNE_INT("defensive_unit_update_hard", mDefensiveUnitUpdateHard);
    EE_TUNE_INT("dm_defensive_unit_update_easy", mDMDefensiveUnitUpdateEasy);
    EE_TUNE_INT("dm_defensive_unit_update_medium", mDMDefensiveUnitUpdateMedium);
    EE_TUNE_INT("dm_defensive_unit_update_hard", mDMDefensiveUnitUpdateHard);
    EE_TUNE_INT("town_guard_update_easy", mTownGuardUpdateEasy);
    EE_TUNE_INT("town_guard_update_medium", mTownGuardUpdateMedium);
    EE_TUNE_INT("town_guard_update_hard", mTownGuardUpdateHard);
    EE_TUNE_INT("dm_town_guard_update_easy", mDMTownGuardUpdateEasy);
    EE_TUNE_INT("dm_town_guard_update_medium", mDMTownGuardUpdateMedium);
    EE_TUNE_INT("dm_town_guard_update_hard", mDMTownGuardUpdateHard);
    EE_TUNE_INT("island_expansion_timer_easy", mIslandExpansionTimerEasy);
    EE_TUNE_INT("island_expansion_timer_medium", mIslandExpansionTimerMedium);
    EE_TUNE_INT("island_expansion_timer_hard", mIslandExpansionTimerHard);
    EE_TUNE_K("dm_attack_frequency_modifier_easy_k", mDMAttackFrequencyModifierEasy);
    EE_TUNE_K("dm_attack_frequency_modifier_medium_k", mDMAttackFrequencyModifierMedium);
    EE_TUNE_K("dm_attack_frequency_modifier_hard_k", mDMAttackFrequencyModifierHard);
    EE_TUNE_K("dm_initial_attack_modifier_easy_k", mDMInitialAttackModifierEasy);
    EE_TUNE_K("dm_initial_attack_modifier_medium_k", mDMInitialAttackModifierMedium);
    EE_TUNE_K("dm_initial_attack_modifier_hard_k", mDMInitialAttackModifierHard);
    EE_TUNE_INT("mining_limit", mMiningLimit);
#undef EE_TUNE_INT
#undef EE_TUNE_K
    EECPUnitsSetInt(L, "epoch_advance_time_scale_k", EECPUnitsMilli(gCPBehaviors[kCPBehaviorEpochAdvanceTimeScale].mFloatValue));
    // the carrier fighters to keep (CheckNaval from WW2): easy, medium, hard; and in a deathmatch
    EECPUnitsSetInt(L, "carrier_fighters_easy", gCPBehaviors[kCPBehaviorCarrierFightersToMaintain].mIntValue1);
    EECPUnitsSetInt(L, "carrier_fighters_medium", gCPBehaviors[kCPBehaviorCarrierFightersToMaintain].mIntValue2);
    EECPUnitsSetInt(L, "carrier_fighters_hard", (long)gCPBehaviors[kCPBehaviorCarrierFightersToMaintain].mFloatValue);
    EECPUnitsSetInt(L, "carrier_fighters_dm_easy", gCPBehaviors[kCPBehaviorCarrierFightersToMaintainDM].mIntValue1);
    EECPUnitsSetInt(L, "carrier_fighters_dm_medium", gCPBehaviors[kCPBehaviorCarrierFightersToMaintainDM].mIntValue2);
    EECPUnitsSetInt(L, "carrier_fighters_dm_hard", (long)gCPBehaviors[kCPBehaviorCarrierFightersToMaintainDM].mFloatValue);
    EECPUnitsSetInt(L, "population_cap_own_weight_k", EECPUnitsMilli(gCPBehaviors[kCPBehaviorPopulationCapOwnWeight].mFloatValue));
    EECPUnitsSetInt(L, "population_cap_other_weight_k", EECPUnitsMilli(gCPBehaviors[kCPBehaviorPopulationCapOtherWeight].mFloatValue));
    lua_setfield(L, -2, "behavior");
    return 1;
}

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

static void EECPUnitsPushCost(lua_State* L, EEComputerPlayer* me, EETechTreeNode* inNode, long inUnitType)
{
    lua_createtable(L, 0, 5);
    static const char* const kNames[5] = {"food", "wood", "stone", "gold", "iron"};
    for (long i = 0; i < 5; i++) {
        lua_pushinteger(L, (lua_Integer)me->ScaleCost(inNode->GetCost(kResourceFood + i, inUnitType, 1.0f)));
        lua_setfield(L, -2, kNames[i]);
    }
    lua_setfield(L, -2, "cost");
    // the tech's listed cost as the classic's needs ledger counts it (EECPResourceManager::AddCosts: the data's
    // mCosts, ScaleCost'ed, without GetCost's epoch and upgrade factors)
    lua_createtable(L, 0, 5);
    for (long i = 0; i < 5; i++) {
        lua_pushinteger(L, (lua_Integer)me->ScaleCost(inNode->mData->mCosts[kResourceFood + i]));
        lua_setfield(L, -2, kNames[i]);
    }
    lua_setfield(L, -2, "data_cost");
}

// ai.techs{epoch = n, id = tech, name = data id} -> the player's tech tree nodes of epochs n..0 (default: the current epoch) in the
// tree's order (EETechTree::GetEpochNodeIDs, the latest epoch first, as the classic research manager collects them):
// id, name, epoch, kind, makes (the class made, by name), counts_toward_epoch, researched, available, obsolete,
// obsolete_epoch, producers (the classes that research or train it), plain_producer (a producer is no citizen and
// its own tech no epoch: the classic research manager's IsWanted),
// cost (as the player pays it), producible (EEComputerPlayer::CanProduce without the cost), affordable (with it),
// citizen_upgrade. With id: that node only. Fog does not apply (a human reads the tech tree)
static int LuaTechs(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EETechTree* theTree = me->mTechnologies;
    if (theTree == NULL) {
        lua_newtable(L);
        return 1;
    }
    long theEpoch = theTree->mCurrentEpoch, theOnly = -1;
    EECPUnitsOptInt(L, 1, "epoch", theEpoch);
    EECPUnitsOptInt(L, 1, "id", theOnly);
    const char* theName = EECPUnitsOptString(L, 1, "name");   // a tech by its data id (ai.techs{name = "t_..."})
    if (theName != NULL)
        theOnly = EECPNextTechByName(theName);
    if (theName != NULL && theOnly < 0) {
        lua_newtable(L);
        return 1;
    }
    std::vector<long> theIDs;
    if (theOnly >= 0)
        theIDs.push_back(theOnly);
    else
        for (long i = theEpoch < (long)theTree->mEpochs.size() ? theEpoch : (long)theTree->mEpochs.size() - 1; i >= 0; i--)
            theTree->GetEpochNodeIDs(i, theIDs);
    EECPUnitsCharge(L, kCostCall + (int64_t)theIDs.size() * kCostRecord);
    lua_createtable(L, (int)theIDs.size(), 0);
    long n = 0;
    for (unsigned long k = 0; k < theIDs.size(); k++) {
        EETechTreeNode* theNode = theTree->Get(theIDs[k]);
        if (theNode == NULL || theNode->mData == NULL)
            continue;
        lua_createtable(L, 0, 18);
        EECPUnitsSetInt(L, "id", theNode->mData->mID);
        EECPUnitsSetStr(L, "name", EECPNextTechName(theNode->mData->mID).c_str());
        EECPUnitsSetInt(L, "epoch", theNode->mEpoch);
        EECPUnitsSetInt(L, "kind", theNode->mData->mKind);
        long theClass = theNode->mData->mMakesUnit ? theNode->mData->mUnitClass : -1;
        if (theClass >= 0 && (unsigned long)theClass < EEUnitClassCount() && gUnitClassTable[theClass] != NULL)
            EECPUnitsSetStr(L, "makes", EECPUnitsName(s, theClass));
        else
            theClass = -1;
        EECPUnitsSetBool(L, "counts_toward_epoch", theNode->mData->mCountsTowardEpoch);
        EECPUnitsSetBool(L, "researched", theNode->mIsResearched);
        EECPUnitsSetBool(L, "available", theNode->GetIsAvailable());
        EECPUnitsSetBool(L, "obsolete", theNode->GetIsObsolete());
        EECPUnitsSetInt(L, "obsolete_epoch", theNode->mObsoleteEpoch);
        lua_createtable(L, (int)theNode->GetNumUnitClasses(), 0);
        bool thePlain = false;
        long np = 0;
        for (unsigned long i = 0; i < theNode->GetNumUnitClasses(); i++) {
            long theProducer = theNode->GetUnitClass(i);
            if (theProducer < 0 || (unsigned long)theProducer >= EEUnitClassCount() || gUnitClassTable[theProducer] == NULL)
                continue;
            lua_pushstring(L, EECPUnitsName(s, theProducer));
            lua_rawseti(L, -2, (lua_Integer)++np);
            EETechTreeNode* theOther = theTree->Get(gUnitClassTable[theProducer]->mTechnology);
            if (theOther != NULL && theOther->mData != NULL && !EEIsEpochID(theOther->mData->mID)
                && theProducer != 0x20 && theProducer != 0x1e0)
                thePlain = true;
        }
        lua_setfield(L, -2, "producers");
        EECPUnitsSetBool(L, "plain_producer", thePlain);
        long theFirst = theNode->GetNumUnitClasses() > 0 ? theNode->GetUnitClass(0) : 0x20;
        EECPUnitsPushCost(L, me, theNode, theClass >= 0 ? theClass : 0x20);
        EECPUnitsSetBool(L, "producible", me->CanProduce(theNode->mData->mID, theFirst, 1, false, 3, NULL));
        EECPUnitsSetBool(L, "affordable", me->CanProduce(theNode->mData->mID, theFirst, 1, true, 3, NULL));
        EECPUnitsSetBool(L, "citizen_upgrade", EECPUnitsCitizenUpgrade(theNode));
        lua_rawseti(L, -2, (lua_Integer)++n);
    }
    return 1;
}

// ---------------------------------------------------------------------------------------------------------------
// The helper queries of the Lua verbs (data/ai/lib/verbs.lua, docs/AI_LUA_VERBS.md): searches and the view's safety
// tests, one call each; the policy is the script's

static const char* const kUnitsBlocked[6] = {NULL, "unexplored", "enemy_near", "danger", "area", "tower"};

// ai.place(type, mode, {x =, y =, first =, last =, units = {ids}, site = index}) -> x, y, gap: a building site by one
// of build()'s rules (mode "ring", "tower", "adjacent", "centre", "classic", "dock": ee/EECPNext.h EECPNextPlaceSearch);
// nil: none
static int LuaPlace(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    long theType = EECPUnitsClassArg(L, 1);
    const char* theMode = lua_type(L, 2) == LUA_TSTRING ? lua_tostring(L, 2) : "";
    static const char* const kModes[6] = {"ring", "tower", "adjacent", "centre", "classic", "dock"};
    long theKind = -1;
    for (long k = 0; k < 6; k++)
        if (strcmp(theMode, kModes[k]) == 0)
            theKind = k;
    long x = -1, y = -1, theFirst = 0, theLast = 6, theSite = 0;
    EECPUnitsOptInt(L, 3, "x", x);
    EECPUnitsOptInt(L, 3, "y", y);
    EECPUnitsOptInt(L, 3, "first", theFirst);
    EECPUnitsOptInt(L, 3, "last", theLast);
    EECPUnitsOptInt(L, 3, "site", theSite);
    std::vector<long> theUnits;
    if (lua_type(L, 3) == LUA_TTABLE) {
        lua_getfield(L, 3, "units");
        if (lua_type(L, -1) == LUA_TTABLE)
            EECPUnitsReadIds(L, lua_gettop(L), theUnits);
        lua_pop(L, 1);
    }
    theFirst = theFirst < 0 ? 0 : theFirst;
    theLast = theLast > 30 ? 30 : theLast;
    if (theKind < 0 || theType < 0) {
        EECPUnitsCharge(L, kCostCall);
        return 0;
    }
    long theRings = theKind == kNextPlaceClassic || theKind == kNextPlaceCentre || theKind == kNextPlaceDock ? 30 : theLast;
    EECPUnitsCharge(L, kCostCall + (2 * theRings + 1) * (2 * theRings + 1) * 6 + (int64_t)theUnits.size() * 50);
    long theX, theY, theGap;
    if (!EECPNextPlaceSearch(me, theType, theKind, x, y, theFirst, theLast, theUnits, theSite - 1, theX, theY, theGap))
        return 0;
    lua_pushinteger(L, (lua_Integer)theX);
    lua_pushinteger(L, (lua_Integer)theY);
    lua_pushinteger(L, (lua_Integer)theGap);
    return 3;
}

// ai.site_blocked(resource) -> nil when the next engine's gather safety lets gatherers go to the resource unit, else
// why not: "unexplored", "enemy_near", "danger", "area", "tower" (EECPNextSiteBlocked: the view's threat memory)
static int LuaSiteBlocked(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsCharge(L, kCostCall + 400);
    EEUnit* theUnit = lua_isinteger(L, 1) ? gWorld.GetUnit((unsigned long)lua_tointeger(L, 1)) : NULL;
    if (theUnit == NULL || !theUnit->mIsValid || theUnit->mTile == NULL) {
        lua_pushstring(L, "unexplored");
        return 1;
    }
    long theKind = EECPNextSiteBlocked(me, theUnit, NULL);
    if (theKind <= 0 || theKind >= 6)
        return 0;
    lua_pushstring(L, kUnitsBlocked[theKind]);
    return 1;
}

// ai.build_unsafe(x, y) -> nil when builders may go to the tile, else why not ("enemy_near", "danger", "tower":
// EECPNextBuildUnsafe)
static int LuaBuildUnsafe(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsCharge(L, kCostCall + 400);
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2))
        return 0;
    long theKind = EECPNextBuildUnsafe(me, (long)lua_tointeger(L, 1), (long)lua_tointeger(L, 2));
    if (theKind <= 0 || theKind >= 6)
        return 0;
    lua_pushstring(L, kUnitsBlocked[theKind]);
    return 1;
}

// ai.epoch(n) -> epoch n's node: id, name, required_sol, current_sol, cost (as the player pays it), available,
// researched; nil off the tree
static int LuaEpoch(lua_State* L)
{
    EECPUnitsCharge(L, kCostRecord);
    EEComputerPlayer* me = EECPUnitsMe(L);
    EETechTree* theTree = me->mTechnologies;
    long theIndex = lua_isinteger(L, 1) ? (long)lua_tointeger(L, 1) : -1;
    if (theTree == NULL || theIndex < 0 || theIndex >= (long)theTree->mEpochs.size() || theIndex >= EEEpochCount()) {
        lua_pushnil(L);
        return 1;
    }
    EETechTreeEpoch* theEpoch = &theTree->mEpochs[theIndex];
    EETechTreeNode* theNode = theEpoch->mData != NULL ? theTree->Get(theEpoch->mData->mID) : NULL;
    if (theNode == NULL) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 8);
    EECPUnitsSetInt(L, "id", theNode->mData->mID);
    EECPUnitsSetStr(L, "name", EECPNextTechName(theNode->mData->mID).c_str());
    EECPUnitsSetInt(L, "required_sol", theEpoch->GetRequiredSOL());
    EECPUnitsSetInt(L, "current_sol", theEpoch->GetCurrentSOL());
    EECPUnitsPushCost(L, me, theNode, kUnitTypeTownCenter);
    EECPUnitsSetBool(L, "available", theNode->GetIsAvailable());
    EECPUnitsSetBool(L, "researched", theNode->mIsResearched);
    return 1;
}

// ---------------------------------------------------------------------------------------------------------------
// Opening, the micro step

static const luaL_Reg kUnitsFunctions[] = {
    {"me", (lua_CFunction)ee_ai_units_lua_me}, {"time", (lua_CFunction)ee_ai_units_lua_time}, {"players", (lua_CFunction)ee_ai_units_lua_players}, {"map", (lua_CFunction)ee_ai_units_lua_map},
    {"units", (lua_CFunction)ee_ai_units_lua_units}, {"unit", (lua_CFunction)ee_ai_units_lua_unit}, {"get", (lua_CFunction)ee_ai_units_lua_get}, {"seen", (lua_CFunction)ee_ai_units_lua_seen}, {"memory", (lua_CFunction)ee_ai_units_lua_memory},
    {"resources", (lua_CFunction)ee_ai_units_lua_resources}, {"tile", (lua_CFunction)ee_ai_units_lua_tile}, {"class", LuaClass}, {"can_hurt", LuaCanHurt}, {"wonders", LuaWonders}, {"events", (lua_CFunction)ee_ai_units_lua_events},
    {"can_place", LuaCanPlace}, {"reach", (lua_CFunction)ee_ai_units_lua_reach}, {"strength", (lua_CFunction)ee_ai_units_lua_strength},
    {"move", (lua_CFunction)ee_ai_units_lua_move}, {"attack", (lua_CFunction)ee_ai_units_lua_attack}, {"attack_ground", (lua_CFunction)ee_ai_units_lua_attack_ground}, {"gather", (lua_CFunction)ee_ai_units_lua_gather},
    {"build", (lua_CFunction)ee_ai_units_lua_build}, {"repair", (lua_CFunction)ee_ai_units_lua_repair}, {"garrison", (lua_CFunction)ee_ai_units_lua_garrison}, {"load", (lua_CFunction)ee_ai_units_lua_load}, {"unload", (lua_CFunction)ee_ai_units_lua_unload},
    {"train", (lua_CFunction)ee_ai_units_lua_train}, {"research", (lua_CFunction)ee_ai_units_lua_research}, {"stance", (lua_CFunction)ee_ai_units_lua_stance}, {"stop", (lua_CFunction)ee_ai_units_lua_stop},
    {"explore", (lua_CFunction)ee_ai_units_lua_explore}, {"delete", (lua_CFunction)ee_ai_units_lua_delete}, {"take", (lua_CFunction)ee_ai_units_lua_take}, {"release", (lua_CFunction)ee_ai_units_lua_release},
    {"reflex", (lua_CFunction)ee_ai_units_lua_reflex}, {"test_spawn", LuaTestSpawn}, {"test_town", (lua_CFunction)ee_ai_units_lua_test_town},
    {"danger", (lua_CFunction)ee_ai_units_lua_danger}, {"danger_box", (lua_CFunction)ee_ai_units_lua_danger_box}, {"mark", (lua_CFunction)ee_ai_units_lua_mark}, {"route", (lua_CFunction)ee_ai_units_lua_route},
    {"find_site", (lua_CFunction)ee_ai_units_lua_find_site}, {"heal", (lua_CFunction)ee_ai_units_lua_heal}, {"rally", (lua_CFunction)ee_ai_units_lua_rally}, {"cancel", (lua_CFunction)ee_ai_units_lua_cancel},
    {"improve", LuaImprove}, {"improve_cost", LuaImproveCost}, {"formation", (lua_CFunction)ee_ai_units_lua_formation}, {"pack", (lua_CFunction)ee_ai_units_lua_pack}, {"dropoff", (lua_CFunction)ee_ai_units_lua_dropoff},
    {"farm", (lua_CFunction)ee_ai_units_lua_farm}, {"cast", (lua_CFunction)ee_ai_units_lua_cast}, {"replant", (lua_CFunction)ee_ai_units_lua_replant},
    {"path_len", (lua_CFunction)ee_ai_units_lua_path_len}, {"drop_site", (lua_CFunction)ee_ai_units_lua_drop_site}, {"shore", (lua_CFunction)ee_ai_units_lua_shore}, {"water_sites", LuaWaterSites}, {"area", LuaArea}, {"garrison_fill", LuaGarrisonFill}, {"enemy_force", (lua_CFunction)ee_ai_units_lua_enemy_force}, {"player_file", (lua_CFunction)ee_ai_units_lua_player_file}, {"area_point", LuaAreaPoint}, {"forest_point", LuaForestPoint}, {"hill_point", LuaHillPoint}, {"choke_point", LuaChokePoint}, {"wall_path", LuaWallPath}, {"wall_route", LuaWallRoute}, {"wall_route_result", LuaWallRouteResult}, {"wall_line", LuaWallLine}, {"wall_stretches", LuaWallStretches}, {"enemy_near", LuaEnemyNear}, {"unexplored", LuaUnexplored},
    {"config", (lua_CFunction)ee_ai_units_lua_config}, {"used", LuaUsed},
    {"wall", (lua_CFunction)ee_ai_units_lua_wall}, {"gate", LuaGate}, {"lock", (lua_CFunction)ee_ai_units_lua_lock}, {"tribute", LuaTribute}, {"trade", LuaTrade},
    {"diplomacy", LuaDiplomacy}, {"patrol", (lua_CFunction)ee_ai_units_lua_patrol},
    {"tuning", LuaTuning}, {"techs", LuaTechs}, {"epoch", LuaEpoch}, {"gatherers", (lua_CFunction)ee_ai_units_lua_gatherers},
    {"place", LuaPlace}, {"site_blocked", LuaSiteBlocked}, {"build_unsafe", LuaBuildUnsafe},
    {NULL, NULL}};
static const size_t kUnitsFunctionCount = sizeof(kUnitsFunctions) / sizeof(kUnitsFunctions[0]) - 1;

// TEST ONLY (EE_AI_BINDING_TRACE, ee_ai binding_trace.rs): each binding's arguments and results, the port's golden
template <size_t K>
static int LuaTraced(lua_State* L)
{
    ee_ai_binding_trace_enter(L, kUnitsFunctions[K].name);
    int theResults = kUnitsFunctions[K].func(L);
    ee_ai_binding_trace_leave(L, kUnitsFunctions[K].name, theResults);
    return theResults;
}

template <size_t... K>
static void EECPUnitsTracedTable(lua_CFunction* outTable, std::index_sequence<K...>)
{
    ((outTable[K] = LuaTraced<K>), ...);
}

static void EECPUnitsTraceOut(const char* inText)
{
    ESDebugOut("%s", inText);
}

void EECPUnitsOpen(lua_State* L, void* inPlayer)
{
    EEComputerPlayer* thePlayer = (EEComputerPlayer*)inPlayer;
    EECPUnitsGet(thePlayer);
    static lua_CFunction sTraced[kUnitsFunctionCount] = {};
    bool theTrace = ee_ai_binding_trace_on();
    if (theTrace && sTraced[0] == NULL) {
        ee_ai_binding_trace_set_out(EECPUnitsTraceOut);
        EECPUnitsTracedTable(sTraced, std::make_index_sequence<kUnitsFunctionCount>());
    }
    for (size_t k = 0; k < kUnitsFunctionCount; k++) {
        lua_pushlightuserdata(L, inPlayer);
        lua_pushcclosure(L, theTrace ? sTraced[k] : kUnitsFunctions[k].func, 1);
        lua_setfield(L, -2, kUnitsFunctions[k].name);
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
        void* s = theCP->mUnitsState->mRust;
        ee_ai_units_second(s, theCP, (long)inTime);   // the enemy memory and the danger layers, once a second
        EECPNextState* theNext = theCP->mNextState;
        long theMicro = ee_ai_units_micro_next(s);
        if (theNext->mLua == NULL || !theNext->mStarted || theNext->mFailed || theMicro == 0x7fffffff)
            continue;
        if (theMicro < 0)
            theMicro = (long)inTime + kMicroMs;
        if ((long)inTime < theMicro) {
            ee_ai_units_set_micro_next(s, theMicro);
            continue;
        }
        while (theMicro <= (long)inTime)
            theMicro += kMicroMs;
        ee_ai_units_set_micro_next(s, theMicro);
        EECPLuaValue theArg = EECPLuaValue::NewMap(), theResult;
        theArg.Set("time", EECPLuaValue::Int((int64_t)inTime));
        bool theFound = false;
        if (!theNext->mLua->CallOptional("micro", theArg, theResult, kMicroBudget, theFound))
            EECPUnitsMicroError(theCP, theNext->mLua);
        else if (!theFound)
            ee_ai_units_set_micro_next(s, 0x7fffffff);   // the script has no micro
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The save: "EENU" (version 4: the militia and held reflexes; 3: the lost layer's trades; 2: the hurt spots and the
// marks). The state and its format are Rust's (ee_ai units_state.rs, units_save.rs).

// TEST ONLY (EE_AI_SAVE_CHECK=1, as EECPNextSaveCheck): the bytes decode into a fresh state that encodes to the same
// bytes; `next.savecheck units=same|DIFFERENT` with the counts
static void EECPUnitsSaveCheck(const EEComputerPlayer* inPlayer, const EECPUnitsState* s, const std::string& inBytes)
{
    static int sOn = -1;
    if (sOn < 0) {
        const char* theValue = getenv("EE_AI_SAVE_CHECK");
        sOn = theValue != NULL && theValue[0] == '1' ? 1 : 0;
    }
    if (!sOn)
        return;
    size_t theCounts[4];
    bool theSame = ee_ai_units_state_save_check(s->mRust, (const uint8_t*)inBytes.data(), inBytes.size(), theCounts);
    EECPTraceScope theScope("next");
    EE_CP_TRACE(const_cast<EEComputerPlayer*>(inPlayer), "next.savecheck", "units=%s bytes=%lu held=%lu events=%lu memory=%lu trades=%lu",
        theSame ? "same" : "DIFFERENT", (unsigned long)inBytes.size(), (unsigned long)theCounts[0],
        (unsigned long)theCounts[1], (unsigned long)theCounts[2], (unsigned long)theCounts[3]);
}

void EECPUnitsSave(const EEComputerPlayer* inPlayer, std::string& outBytes)
{
    outBytes.clear();
    const EECPUnitsState* s = inPlayer->mUnitsState;
    if (s == NULL)
        return;
    outBytes.resize(ee_ai_units_state_save(s->mRust, NULL, 0));
    ee_ai_units_state_save(s->mRust, (uint8_t*)&outBytes[0], outBytes.size());
    EECPUnitsSaveCheck(inPlayer, s, outBytes);
}

bool EECPUnitsLoad(EEComputerPlayer* inPlayer, const std::string& inBytes)
{
    if (inBytes.empty())
        return true;
    return ee_ai_units_state_load(EECPUnitsGet(inPlayer)->mRust, (const uint8_t*)inBytes.data(), inBytes.size());
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
    kMessageAddPatrolPoint, kMessageLockUnlock, kMessageBuildWall};

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
    if (gWorld.GetPlayerBySlot(kBorrowedSlot) == me) {
        inMessage.SetOriginatingSlot(kBorrowedSlot);
        inMessage.Execute();
    }
    me->mSlot = theSlot;
    me->mActiveGroup->Clear();
}

static void EECPUnitsExec(EEComputerPlayer* me, const EeAiUnitsMessage& m)
{
    static_assert(sizeof(kUnitsMessages) == EE_AI_UNITS_MSG_BUILD_WALL + 1, "ee_ai units_bind.rs EE_AI_UNITS_MSG_*");
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

extern "C" int64_t ee_ai_units_host(int32_t inOp, void* inPlayer, int64_t a, int64_t b, int64_t c, void* outRecord)
{
    EEComputerPlayer* me = (EEComputerPlayer*)inPlayer;
    switch (inOp) {
    case EE_AI_UNITS_HOST_STATE:
        return (int64_t)(uintptr_t)EECPUnitsGet(me)->mRust;
    case EE_AI_UNITS_HOST_NOW:
        return (int64_t)gPhysics->mGameTime;
    case EE_AI_UNITS_HOST_UNIT:
        return a > 0 ? (int64_t)EECPUnitsHandle(gWorld.GetUnit((unsigned long)a)) : 0;
    case EE_AI_UNITS_HOST_MAP_SIZE: {
        long w, h;
        EECPUnitsMapSize(w, h);
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
    case EE_AI_UNITS_HOST_LAST_ATTACKER:
        return (int64_t)EECPUnitsHandle(((EEComplexUnit*)(uintptr_t)a)->mLastAttacker);
    case EE_AI_UNITS_HOST_UPGRADE: {
        EEPlayer* theOwner = (EEPlayer*)(uintptr_t)a;
        if (b < 0 || (unsigned long)b >= theOwner->mUnitClassUpgrades.size())
            return 0;
        return (int64_t)(uintptr_t)theOwner->mUnitClassUpgrades[(unsigned long)b];
    }
    case EE_AI_UNITS_HOST_CONTINENT:
        return (int64_t)gContinentManager.GetContinent(U2DPoint<long>((long)a, (long)b), c != 0 ? kAIPathWaterBody : kAIPathLandMass);
    case EE_AI_UNITS_HOST_TRACE:
        if (gCPTraceOn) {
            EECPTraceScope theScope("next");
            EECPTraceOut(me, (const char*)(uintptr_t)a, "%s", (const char*)(uintptr_t)b);
        }
        return 0;
    case EE_AI_UNITS_HOST_TRACE_ON:
        return gCPTraceOn ? 1 : 0;
    case EE_AI_UNITS_HOST_FACTS: {
        EEComplexUnit* c = (EEComplexUnit*)(uintptr_t)a;
        EeAiUnitsFacts& f = *(EeAiUnitsFacts*)outRecord;
        ee_ai_units_facts_reset(&f);
        EEUnitGoal* g = c->GetGoalState();
        if (g != NULL) {
            f.has_goal = true;
            GE3DPoint thePlace;
            g->GetGoalLocation(thePlace);
            f.tx = UMFloor(thePlace.GetX());
            f.ty = UMFloor(thePlace.GetY());
            // the goal's target unit (AI_LUA_UNITS §2.3 `target`): the resource gathered, the building built or
            // repaired, the unit attacked; and a gatherer's resource kind
            EEUnit* theTarget = NULL;
            if (EEUGRust* theGather = EEGatherGoalCast(g)) {
                theTarget = theGather->GetGatherResource();
                long r = (long)c->mResourceType - kResourceFood;
                if (r >= 0 && r < 5)
                    f.gathers = r;
                // waiting for room at its resource (the classic resource manager counts these gatherers as idle)
                f.waiting = theGather->IsGatherWaiting() && c->mCurrentPhysicsAction != 0;
            } else if (EEUGRust* theBuild = EEBuildRepairGoalCast(g)) {
                theTarget = theBuild->GetBuildTarget();
                // a builder waiting for its foundation's site to clear while another unit holds it (the classic's
                // FindStalledBuilder: CancelStalledBuilds cancels these)
                f.stalled = (c->mCommand ? c->mCommand->mType : 0) == kActionWaitForBuildSiteToClear && theTarget != NULL
                    && theTarget->mComplexUnit != NULL && !theTarget->mComplexUnit->mIsBuilt && theBuild->IsBuildSiteTaken();
            } else if (EEUGRust* theAttack = EEAttackUnitCast(g))
                theTarget = theAttack->GetAttackTarget();
            if (theTarget != NULL && theTarget->mIsValid)
                f.target = (long)theTarget->mUnitID;
        }
        // an own aircraft's home base (the airport or carrier it returns to; the classic air sub-group's reach)
        if (c->mHomeBase != NULL && c->mHomeBase->mIsValid)
            f.home_base = (long)c->mHomeBase->mUnitID;
        if (!c->mUnitClass->IsBuilding() && c->mUnitClass->GetMaxVelocity() > 0.0f) {
            // the terrain the unit can cross (EEUnitClass::GetTerrainFlags: the classic's sub-group common move flags)
            f.moves = true;
            EECPASGFlags theFlags;
            memset(theFlags.mFlags, 0, sizeof(theFlags.mFlags));
            theFlags.mPlayerID = 0;
            c->mUnitClass->GetTerrainFlags(c, theFlags);
            f.crosses_forest = theFlags.mFlags[kAITileForest] != 0;
            f.crosses_cliff = theFlags.mFlags[kAITileCliff] != 0;
        }
        f.garrisoned = c->mGarrisonCount;   // citizens inside a town centre, capitol or settlement
        for (unsigned long k = 0; k < EEUnitBehavior::sTable.size(); k++)
            if (EEUnitBehavior::sTable[k] == c->mBehavior)
                f.stance = (long)k;
        return 0;
    }
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
        return EECanPlaceOnTile(me, (long)a, (long)b, (long)c, false, 1) ? 1 : 0;
    case EE_AI_UNITS_HOST_IS_TRANSPORT:
        return ((EEUnitClass*)(uintptr_t)a)->IsTransport() ? 1 : 0;
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
        return EEPlayerBlockTakes(me, (long)a, (long)b - 1, (long)c - 1, theSize + 2, theSize + 2, 0) ? 1 : 0;
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
    default:
        return 0;
    }
}
