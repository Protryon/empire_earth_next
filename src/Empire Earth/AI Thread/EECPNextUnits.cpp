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
#include "ee/EEUGGather.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIForest.h"
#include "ee/EECPEngine.h"
#include "ee/EEMessage.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EETeam.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EEMessageID.h"
#include "ee/EECPTrace.h"
#include "ee/EEGameLog.h"
#include "ee/EECPLua.h"
#include "ee/EEEvent.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUGGather.h"
#include "ee/EECPBehavior.h"
#include "ee/EERandomMapIDs.h"
#include "ee/EETechTreeTechID.h"
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

struct DangerMark {                 // ai.mark: the script's own danger
    long mX, mY, mR, mValue, mUntil;
};

const long kDangerCell = 4;         // the danger layers' and the route's cell (tiles)
const long kLostMs = 1200000;       // the lost layer remembers a bad trade this long (Age of AI's 20 minutes)
const long kMaxTrades = 1024;       // (minute, cell) records of the lost layer
const long kInfluenceSpread = 3;    // cells

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
    bool mReflexMilitia = true;    // the engine's citizen militia (EECPRMMilitia); off when the script fights its own
    bool mUsed = false;                             // a binding was called (the save's shadow step cannot replay commands)
    long mMicroNext = -1;                           // -1: not started; 0x7fffffff: the script has no micro
    long mOrdersThisStep = 0, mUnitOrdersThisStep = 0;
    long mStepStamp = -1;                           // the game ms of the counters above
    // the threat layer (AI_LUA_UNITS §4.4): kDangerCell cells, rebuilt every game second
    std::vector<long> mThreat;
    long mDangerW = 0, mDangerH = 0;
    std::vector<std::pair<long, long> > mHurt;      // own citizens hurt: (x + y * 65536, game ms), 2 minutes
    std::vector<DangerMark> mMarks;
    // the lost layer (§4.4): the hit points own units lost over those they dealt, by cell and minute, 20 minutes
    std::map<std::pair<long, long>, std::pair<long, long> > mTrades;   // (minute, cell) -> (lost, dealt)
    std::vector<long> mLost;
    // the influence layer: own strength less the remembered enemies', spread over kInfluenceSpread cells
    std::vector<long> mInfluence;
    std::vector<long> mG, mFrom;                    // the route's scratch
    std::vector<unsigned char> mOpen;
    std::vector<std::pair<long, long> > mHeap;
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

static const char* EECPUnitsCategoryClass(EEUnitClass* c, bool inHasResource)
{
    long k = c->mCategory;
    if (k == kUnitCategoryProjectile || k == kUnitCategoryBomb || k == kUnitCategoryMissile || k == kUnitCategoryTorpedo)
        return "projectile";
    switch (c->mCategory) {
    case kUnitCategoryLandCitizen: return "citizen";
    case kUnitCategoryNavalCitizen: return "boat";
    case kUnitCategoryResource: return "resource";
    case kUnitCategoryAmbient: return inHasResource ? "resource" : "ambient";
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

static const char* EECPUnitsCategory(EEUnit* inUnit)
{
    return EECPUnitsCategoryClass(inUnit->mUnitClass, inUnit->mResourceAmount > 0);
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
    if (strcmp(inName, "militia") == 0)
        return inPlayer->mUnitsState->mReflexMilitia;
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

// inWork (ai.release(ids, {gather = true})): a citizen or boat that gathers goes onto its resource's gatherer list, so
// the engine's runners count it where it works (else only an idle one rejoins them, on the idle list)
static void EECPUnitsRelease(EEComputerPlayer* inPlayer, EECPUnitsState* s, EEComplexUnit* inUnit, bool inWork)
{
    if (s->mHeld.erase((long)inUnit->mUnitID) == 0 || inUnit->mUnitClass->IsBuilding())
        return;
    if (inUnit->mIsDetachedFromAI)
        inUnit->mIsDetachedFromAI = false;
    if (inPlayer->mCombatManager != NULL)
        inPlayer->mCombatManager->UnitAdded(inUnit);
    long theCategory = inUnit->mUnitClass->mCategory;
    if ((theCategory != kUnitCategoryLandCitizen && theCategory != kUnitCategoryNavalCitizen)
        || inPlayer->mCivilizationManager == NULL || inPlayer->mCivilizationManager->mResourceManager == NULL)
        return;
    EECPResourceManager* theManager = inPlayer->mCivilizationManager->mResourceManager;
    if (EECPUnitsIdle(inUnit)) {
        theManager->AddIdleGatherer(inUnit);
        return;
    }
    EEUGGather* theGoal = inWork ? dynamic_cast<EEUGGather*>(inUnit->GetLoadGoal()) : NULL;
    if (theGoal != NULL && theGoal->mResource != NULL && theGoal->mResource->mHitPoints > 0) {
        long theResource = (long)theGoal->mResource->GetResourceType();
        if (theResource >= 0 && theResource < 7) {
            theManager->RemoveGatherer(inUnit);
            theManager->AddGatherer(inUnit, theResource);
        }
    }
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

// the lost layer's record of a hit where inUnit stands: own hit points lost, or dealt to an enemy (AI_LUA_UNITS §4.4)
static void EECPUnitsTrade(EECPUnitsState* s, EEUnit* inUnit, long inLost, long inDealt)
{
    if (s->mDangerW <= 0 || !EECPUnitsOnMap(inUnit) || (inLost <= 0 && inDealt <= 0))
        return;
    long cx = inUnit->GetTileX() / kDangerCell, cy = inUnit->GetTileY() / kDangerCell;
    if (cx < 0 || cy < 0 || cx >= s->mDangerW || cy >= s->mDangerH)
        return;
    std::pair<long, long> theKey((long)gPhysics->mGameTime / 60000, cy * s->mDangerW + cx);
    std::map<std::pair<long, long>, std::pair<long, long> >::iterator it = s->mTrades.find(theKey);
    if (it == s->mTrades.end()) {
        if ((long)s->mTrades.size() >= kMaxTrades)
            s->mTrades.erase(s->mTrades.begin());   // the oldest minute goes first
        it = s->mTrades.insert(std::make_pair(theKey, std::make_pair(0L, 0L))).first;
    }
    it->second.first += inLost;
    it->second.second += inDealt;
}

void EECPUnitsOnDealt(EEComputerPlayer* inPlayer, long inAmount, EEUnit* inVictim)
{
    EECPUnitsState* s = inPlayer->mUnitsState;
    if (s == NULL || inVictim == NULL || inVictim->mPlayer == NULL || inVictim->mPlayer == inPlayer
        || inVictim->mPlayer == gWorld.mPlayers[0] || inPlayer->IsAlly(inVictim->mPlayer))
        return;
    EECPUnitsTrade(s, inVictim, 0, inAmount);
}

void EECPUnitsOnDamage(EEComputerPlayer* inPlayer, long inAttackerPlayer, long inAmount, EEUnit* inUnit)
{
    EECPUnitsState* s = inPlayer->mUnitsState;
    if (s == NULL || inUnit == NULL)
        return;
    long theLost = inAmount;   // an animal's or an ally's hit is no trade (the event still comes below)
    if (inAttackerPlayer <= 0 || (unsigned long)inAttackerPlayer >= gWorld.mNumPlayers || gWorld.mPlayers[inAttackerPlayer] == NULL
        || inPlayer->IsAlly(gWorld.mPlayers[inAttackerPlayer]))
        theLost = 0;
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
        EECPUnitsTrade(s, inUnit, theLost, 0);
        return;
    }
    s->mAttackedAt[theID] = (long)s->mEvents.size();
    EECPUnitsEventAdd(s, kEvAttacked, theID, theAttacker, inAttackerPlayer, inAmount);
    EECPUnitsTrade(s, inUnit, theLost, 0);
    long theCategory = inUnit->mUnitClass->mCategory;
    if ((theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen) && inAttackerPlayer > 0
        && EECPUnitsOnMap(inUnit) && s->mHurt.size() < 256)
        s->mHurt.push_back(std::make_pair(inUnit->GetTileX() + inUnit->GetTileY() * 65536, (long)gPhysics->mGameTime));
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
    // converted: alive and another player's now. A unit killed still has its hit points here (EEComplexUnit::Kill
    // calls UnitTaken before it zeroes them), so the hit points alone made every kill a "converted" event and no
    // script ever saw "died" (AI_RESEARCH §8.41); a priest's conversion is a swap (EEConvertUnit), skipped above
    bool theConverted = inUnit->mHitPoints > 0 && inUnit->mPlayer != NULL && inUnit->mPlayer != inPlayer;
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
                || EECPUnitsIsProjectile(theUnit) || !EECPUnitsOnMap(theUnit) || !static_cast<EETile*>(theUnit->mTile)->IsSeenBy(inPlayer))
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
    EECPUnitsGet(EECPUnitsMe(L))->mUsed = true;
    EECPLuaState::From(L)->Charge(L, inAmount);
}

bool EECPUnitsUsed(const EEComputerPlayer* inPlayer)
{
    return inPlayer != NULL && inPlayer->mUnitsState != NULL && inPlayer->mUnitsState->mUsed;
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
        EECPUnitsSetBool(L, "hostile", theOther != me && theOther->GetEnemy(me->mPlayerID) != 0);   // their stance towards me
        // the teams the player is in (EEWorld::AddAlliance: both sides allied): the other players of those teams
        lua_newtable(L);
        long na = 0;
        for (unsigned long t = 0; t < gWorld.mTeams.size(); t++) {
            if (!gWorld.mTeams[t].ContainsPlayer(theOther->mPlayerID))
                continue;
            for (unsigned long k = 0; k < gWorld.mTeams[t].GetNumPlayers(); k++) {
                unsigned long q = gWorld.mTeams[t].GetPlayersAt(k);
                if (q != theOther->mPlayerID && q > 0 && q < gWorld.mNumPlayers && gWorld.mPlayers[q] != NULL) {
                    lua_pushinteger(L, (lua_Integer)q);
                    lua_rawseti(L, -2, (lua_Integer)++na);
                }
            }
        }
        lua_setfield(L, -2, "allies");
        EECPUnitsSetBool(L, "alive", theOther->mFirstUnit != NULL);
        EECPUnitsSetBool(L, "human", theOther->IsHuman());
        lua_rawseti(L, -2, (lua_Integer)++n);
    }
    return 1;
}

static void EECPUnitsMapSize(long& outW, long& outH);

static int LuaMap(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    lua_newtable(L);
    long w = 0, h = 0;
    EECPUnitsMapSize(w, h);
    EECPUnitsSetInt(L, "width", w);
    EECPUnitsSetInt(L, "height", h);
    return 1;
}

// the filters of ai.units / ai.seen / ai.resources
static const long kUnitsMaxTypes = 16;   // class names in one filter's `type` list
struct EECPUnitsFilter {
    long mType = -1;   // -1 any class, -2 an unknown name (nothing), -3 the list mTypes
    long mTypes[kUnitsMaxTypes] = {0};
    long mTypeCount = 0;
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
    if (lua_type(L, -1) == LUA_TTABLE) {
        // a list of class names (or ids): any of them; the unknown ones are left out (none known: nothing)
        int theList = lua_gettop(L);
        lua_Integer theLength = (lua_Integer)lua_rawlen(L, theList);
        for (lua_Integer k = 1; k <= theLength && f.mTypeCount < kUnitsMaxTypes; k++) {
            lua_rawgeti(L, theList, k);
            long theType = EECPUnitsClassArg(L, lua_gettop(L));
            lua_pop(L, 1);
            if (theType >= 0)
                f.mTypes[f.mTypeCount++] = theType;
        }
        f.mType = -3;
    } else {
        if (!lua_isnil(L, -1))
            f.mType = EECPUnitsClassArg(L, lua_gettop(L));
        if (!lua_isnil(L, -1) && f.mType < 0)
            f.mType = -2;   // an unknown name: matches nothing
    }
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

// a class against a filter's `type` (one class, a list, or any)
static bool EECPUnitsTypeMatch(const EECPUnitsFilter& f, long inType)
{
    if (f.mType == -1)
        return true;
    if (f.mType != -3)
        return inType == f.mType;
    for (long k = 0; k < f.mTypeCount; k++)
        if (f.mTypes[k] == inType)
            return true;
    return false;
}

// the common tests of a filter; outKey the sort key (the squared distance with near, else 0)
static bool EECPUnitsPasses(EEUnit* inUnit, const EECPUnitsFilter& f, long long& outKey)
{
    if (!EECPUnitsTypeMatch(f, inUnit->mUnitClass->mUnitType))
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
        if (c == NULL || !(theUnit->mHitPoints > 0) || !theUnit->mIsValid || EECPUnitsIsProjectile(theUnit))
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
                || EECPUnitsIsProjectile(theUnit) || !EECPUnitsSeen(me, theUnit))
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
    bool theOwn = inUnit->mPlayer == me || (inUnit->mPlayer == NULL && EECPUnitsPending(me, (long)inUnit->mUnitID) != NULL);
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
    if (!EECPUnitsOnMap(inUnit) && c->mUnitClass->IsBuilding() && !c->mIsBuilt)
        EECPUnitsSetBool(L, "pending", true);   // a foundation the builders have not reached (ai.build's id)
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
        // the goal's target unit (AI_LUA_UNITS §2.3 `target`): the resource gathered, the building built or repaired,
        // the unit attacked; and a gatherer's resource kind
        EEUnit* theTarget = NULL;
        if (EEUGGather* theGather = dynamic_cast<EEUGGather*>(g)) {
            theTarget = theGather->mResource;
            long r = (long)c->mResourceType - kResourceFood;
            static const char* const kKinds[5] = {"food", "wood", "stone", "gold", "iron"};
            if (r >= 0 && r < 5)
                EECPUnitsSetStr(L, "gathers", kKinds[r]);
            // waiting for room at its resource (the classic resource manager counts these gatherers as idle)
            if (theGather->mWaiting && c->mCurrentPhysicsAction != 0)
                EECPUnitsSetBool(L, "waiting", true);
        } else if (EEUGBuildRepair* theBuild = dynamic_cast<EEUGBuildRepair*>(g)) {
            theTarget = theBuild->mTarget;
            // a builder waiting for its foundation's site to clear while another unit holds it (the classic's
            // FindStalledBuilder: CancelStalledBuilds cancels these)
            if ((c->mCommand ? c->mCommand->mType : 0) == kActionWaitForBuildSiteToClear && theTarget != NULL
                && theTarget->mComplexUnit != NULL && !theTarget->mComplexUnit->mIsBuilt && theBuild->IsBuildSiteTaken())
                EECPUnitsSetBool(L, "stalled", true);
        }
        else if (EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(g))
            theTarget = theAttack->mTarget;
        if (theTarget != NULL && theTarget->mIsValid)
            EECPUnitsSetInt(L, "target", (long)theTarget->mUnitID);
    }
    if (c->mContainer != NULL)
        EECPUnitsSetInt(L, "inside", (long)c->mContainer->mUnitID);
    EECPUnitsSetInt(L, "cargo", (long)c->GetNumCargoUnits());
    EECPUnitsSetInt(L, "garrisoned", c->mGarrisonCount);   // citizens inside a town centre, capitol or settlement
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
        theUnit = EECPUnitsPending(me, EECPUnitsForward(me->mUnitsState, inID));   // a foundation not placed yet (ai.build's id)
        if (theUnit != NULL && theUnit->mUnitClass != NULL && theUnit->mComplexUnit != NULL)
            return theUnit;   // the player's own (its owner field may still be unset)
    }
    if (theUnit == NULL) {
        theUnit = gWorld.GetUnit((unsigned long)inID);   // a resource is alive with hit points 0 (a worked mine)
        if (theUnit == NULL || !theUnit->mIsValid || !EECPUnitsIsResource(theUnit))
            return NULL;
    }
    if (theUnit->mUnitClass == NULL)
        return NULL;
    if (EECPUnitsIsResource(theUnit))
        return EECPUnitsExplored(me, theUnit) ? theUnit : NULL;
    if (EECPUnitsIsProjectile(theUnit))
        return NULL;
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
        if ((f.mPlayer >= 0 && r.mPlayer != f.mPlayer) || !EECPUnitsTypeMatch(f, r.mType) || r.mSeen < theSince
            || (theBuildings >= 0 && r.mBuilding != theBuildings))
            continue;
        if (f.mCategory != NULL && (r.mType < 0 || (unsigned long)r.mType >= EEUnitClassCount() || gUnitClassTable[r.mType] == NULL
                || strcmp(EECPUnitsCategoryClass(gUnitClassTable[r.mType], false), f.mCategory) != 0))
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
    lua_createtable(L, 0, 16);
    EECPUnitsSetStr(L, "type", EECPUnitsName(s, theType));
    EECPUnitsSetStr(L, "category", EECPUnitsCategoryClass(c, false));
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

static unsigned short EECPUnitsAreaNear(long inX, long inY, long inKind);

// ai.reach(x1, y1, x2, y2) -> whether both tiles are on one land mass (a tile on none takes its nearest within 3)
static int LuaReach(lua_State* L)
{
    EECPUnitsCharge(L, kCostTile * 2);
    for (int i = 1; i <= 4; i++)
        if (!lua_isinteger(L, i)) {
            lua_pushboolean(L, 0);
            return 1;
        }
    unsigned short ca = EECPUnitsAreaNear((long)lua_tointeger(L, 1), (long)lua_tointeger(L, 2), kAIPathLandMass);
    lua_pushboolean(L, ca != 0 && ca == EECPUnitsAreaNear((long)lua_tointeger(L, 3), (long)lua_tointeger(L, 4), kAIPathLandMass) ? 1 : 0);
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

enum CmdReason { kOK, kBadArgument, kNotOwn, kNotSeen, kUnexplored, kCannotPlace, kCannotAfford, kCannotDo, kLimit, kFull, kNotInEpoch };
const char* const kReasonNames[] = {"none", "bad_argument", "not_own", "not_seen", "unexplored", "cannot_place",
                                    "cannot_afford", "cannot_do", "limit", "full", "not_in_epoch"};

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
        if (c == NULL || EECPUnitsIsProjectile(c)) {
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
        || (!gUnitClassTable[theType]->IsBuilding() && !gUnitClassTable[theType]->IsLandBuilding())   // a fortress: a land building of a transport class
        || !lua_isinteger(L, 3) || !lua_isinteger(L, 4))
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
    bool theWork = !inTake && EECPUnitsOptBool(L, 2, "gather", false);
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
            EECPUnitsRelease(me, s, c, theWork);
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

// ai.reflex{naval_flee = false, danger = false, militia = false, memory_ms = 180000}
static int LuaReflex(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    s->mReflexNavalFlee = EECPUnitsOptBool(L, 1, "naval_flee", s->mReflexNavalFlee);
    s->mReflexDanger = EECPUnitsOptBool(L, 1, "danger", s->mReflexDanger);
    s->mReflexMilitia = EECPUnitsOptBool(L, 1, "militia", s->mReflexMilitia);
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
// The danger layers and the route (AI_LUA_UNITS §4.3-§4.4)

// the map's size in tiles (ee/EEMapDimensions.h)
static void EECPUnitsMapSize(long& outW, long& outH)
{
    outW = gWorld.mMap != NULL ? (long)gMapWidth : 0;
    outH = gWorld.mMap != NULL ? (long)gMapHeight : 0;
}

// adds inValue to every cell whose centre is within inRange tiles of (inX, inY)
static void EECPUnitsDangerAdd(EECPUnitsState* s, long inX, long inY, long inRange, long inValue)
{
    long c = kDangerCell;
    long x0 = (inX - inRange) / c, x1 = (inX + inRange) / c, y0 = (inY - inRange) / c, y1 = (inY + inRange) / c;
    for (long cy = y0 < 0 ? 0 : y0; cy <= y1 && cy < s->mDangerH; cy++)
        for (long cx = x0 < 0 ? 0 : x0; cx <= x1 && cx < s->mDangerW; cx++) {
            long dx = cx * c + c / 2 - inX, dy = cy * c + c / 2 - inY;
            long r = inRange + c / 2;
            if (dx * dx + dy * dy > r * r)
                continue;
            long& v = s->mThreat[cy * s->mDangerW + cx];
            v = v + inValue > 30000 ? 30000 : v + inValue;
        }
}

// adds inValue to the influence cell at (inX, inY) and, falling off linearly, to the cells within kInfluenceSpread
static void EECPUnitsInfluenceAdd(EECPUnitsState* s, long inX, long inY, long inValue)
{
    long cx0 = inX / kDangerCell, cy0 = inY / kDangerCell, r = kInfluenceSpread;
    for (long cy = cy0 - r; cy <= cy0 + r; cy++)
        for (long cx = cx0 - r; cx <= cx0 + r; cx++) {
            if (cx < 0 || cy < 0 || cx >= s->mDangerW || cy >= s->mDangerH)
                continue;
            long dx = cx > cx0 ? cx - cx0 : cx0 - cx, dy = cy > cy0 ? cy - cy0 : cy0 - cy;
            long d = dx > dy ? dx : dy;
            long long v = (long long)inValue * (r + 1 - d) / (r + 1);
            long long sum = (long long)s->mInfluence[cy * s->mDangerW + cx] + v;
            s->mInfluence[cy * s->mDangerW + cx] = sum > 1000000000LL ? 1000000000L : sum < -1000000000LL ? -1000000000L : (long)sum;
        }
}

// the threat layer, rebuilt from what the player sees and remembers, and where its citizens were hurt
static void EECPUnitsDangerBuild(EEComputerPlayer* inPlayer, EECPUnitsState* s)
{
    long w, h;
    EECPUnitsMapSize(w, h);
    s->mDangerW = (w + kDangerCell - 1) / kDangerCell;
    s->mDangerH = (h + kDangerCell - 1) / kDangerCell;
    s->mThreat.assign((size_t)(s->mDangerW * s->mDangerH), 0);
    long theNow = (long)gPhysics->mGameTime;
    for (std::map<long, Remembered>::iterator it = s->mMemory.begin(); it != s->mMemory.end(); ++it) {
        const Remembered& r = it->second;
        if (r.mType < 0 || (unsigned long)r.mType >= EEUnitClassCount() || gUnitClassTable[r.mType] == NULL
            || r.mPlayer <= 0 || (unsigned long)r.mPlayer >= gWorld.mNumPlayers || gWorld.mPlayers[r.mPlayer] == NULL)
            continue;
        EEPlayer* theOwner = gWorld.mPlayers[r.mPlayer];
        if ((unsigned long)r.mType >= theOwner->mUnitClassUpgrades.size() || theOwner->mUnitClassUpgrades[r.mType] == NULL)
            continue;
        EEUnitClassUpgrade* u = theOwner->mUnitClassUpgrades[r.mType];
        long theAttack = UMFloatToLong(u->mAttack);
        if (theAttack <= 0)
            continue;
        long theRange = UMFloor(u->mRange) + (r.mBuilding ? 2 : 1);
        long theAge = theNow - r.mSeen;
        long theValue = theAttack;
        if (!r.mBuilding && theAge > 0)   // a soldier seen a while ago counts less (none after memory_ms)
            theValue = theAttack * (s->mMemoryMs - (theAge < s->mMemoryMs ? theAge : s->mMemoryMs)) / (s->mMemoryMs > 0 ? s->mMemoryMs : 1);
        if (theValue > 0)
            EECPUnitsDangerAdd(s, r.mX, r.mY, theRange < 2 ? 2 : theRange, theValue);
    }
    for (unsigned long k = 0; k < s->mHurt.size();) {
        if (theNow - s->mHurt[k].second > 120000) {
            s->mHurt.erase(s->mHurt.begin() + k);
            continue;
        }
        EECPUnitsDangerAdd(s, s->mHurt[k].first % 65536, s->mHurt[k].first / 65536, 6, 5);
        k++;
    }
    for (unsigned long k = 0; k < s->mMarks.size();) {
        if (theNow >= s->mMarks[k].mUntil) {
            s->mMarks.erase(s->mMarks.begin() + k);
            continue;
        }
        EECPUnitsDangerAdd(s, s->mMarks[k].mX, s->mMarks[k].mY, s->mMarks[k].mR, s->mMarks[k].mValue);
        k++;
    }
    // the lost layer: where own units lost more hit points than they dealt in the last kLostMs, the difference over
    // 10 on the cell and half of it on the cells round it (Age of AI's DANGER_LOST, cover.c)
    s->mLost.assign((size_t)(s->mDangerW * s->mDangerH), 0);
    long theMinute = theNow / 60000 - kLostMs / 60000;
    while (!s->mTrades.empty() && s->mTrades.begin()->first.first < theMinute)
        s->mTrades.erase(s->mTrades.begin());
    for (std::map<std::pair<long, long>, std::pair<long, long> >::iterator it = s->mTrades.begin(); it != s->mTrades.end(); ++it) {
        long theNet = it->second.first - it->second.second;
        long c = it->first.second;
        if (theNet <= 0 || c < 0 || c >= s->mDangerW * s->mDangerH)
            continue;
        long v = theNet / 10;
        v = v > 30000 ? 30000 : v;
        long cx = c % s->mDangerW, cy = c / s->mDangerW;
        for (long dy = -1; dy <= 1; dy++)
            for (long dx = -1; dx <= 1; dx++) {
                long nx = cx + dx, ny = cy + dy;
                if (nx < 0 || ny < 0 || nx >= s->mDangerW || ny >= s->mDangerH)
                    continue;
                long& cell = s->mLost[ny * s->mDangerW + nx];
                long add = (dx == 0 && dy == 0) ? v : v / 2;
                cell = cell + add > 30000 ? 30000 : cell + add;
            }
    }
    // the influence layer: own strength less the remembered enemies' (weighed down with age as the threat layer
    // does), each spread over kInfluenceSpread cells with a linear fall-off
    s->mInfluence.assign((size_t)(s->mDangerW * s->mDangerH), 0);
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (!(theUnit->mHitPoints > 0) || !EECPUnitsOnMap(theUnit) || theUnit->mStats == NULL || EECPUnitsIsProjectile(theUnit))
            continue;
        long v = EECPUnitsStrength(theUnit);
        if (v > 0)
            EECPUnitsInfluenceAdd(s, theUnit->GetTileX(), theUnit->GetTileY(), v);
    }
    for (std::map<long, Remembered>::iterator it = s->mMemory.begin(); it != s->mMemory.end(); ++it) {
        const Remembered& r = it->second;
        if (r.mType < 0 || (unsigned long)r.mType >= EEUnitClassCount() || gUnitClassTable[r.mType] == NULL
            || r.mPlayer <= 0 || (unsigned long)r.mPlayer >= gWorld.mNumPlayers || gWorld.mPlayers[r.mPlayer] == NULL)
            continue;
        EEPlayer* theOwner = gWorld.mPlayers[r.mPlayer];
        if ((unsigned long)r.mType >= theOwner->mUnitClassUpgrades.size() || theOwner->mUnitClassUpgrades[r.mType] == NULL)
            continue;
        long theAttack = UMFloatToLong(theOwner->mUnitClassUpgrades[r.mType]->mAttack);
        if (theAttack <= 0)
            continue;
        long theAge = theNow - r.mSeen;
        long v = r.mHP * theAttack;
        if (!r.mBuilding && theAge > 0)
            v = (long)((long long)v * (s->mMemoryMs - (theAge < s->mMemoryMs ? theAge : s->mMemoryMs)) / (s->mMemoryMs > 0 ? s->mMemoryMs : 1));
        if (v > 0)
            EECPUnitsInfluenceAdd(s, r.mX, r.mY, -v);
    }
}

static long EECPUnitsLayerAt(const std::vector<long>& inLayer, const EECPUnitsState* s, long inX, long inY)
{
    if (inX < 0 || inY < 0 || inLayer.empty())
        return 0;
    long cx = inX / kDangerCell, cy = inY / kDangerCell;
    if (cx >= s->mDangerW || cy >= s->mDangerH)
        return 0;
    return inLayer[cy * s->mDangerW + cx];
}

static long EECPUnitsDangerAt(const EECPUnitsState* s, long inX, long inY)
{
    return EECPUnitsLayerAt(s->mThreat, s, inX, inY);
}

// the layer an optional string argument names: "threat" (the default), "lost" or "influence"; NULL: unknown
static const std::vector<long>* EECPUnitsLayerArg(lua_State* L, int inIndex, const EECPUnitsState* s)
{
    if (lua_type(L, inIndex) != LUA_TSTRING)
        return &s->mThreat;
    const char* theName = lua_tostring(L, inIndex);
    if (strcmp(theName, "threat") == 0)
        return &s->mThreat;
    if (strcmp(theName, "lost") == 0)
        return &s->mLost;
    if (strcmp(theName, "influence") == 0)
        return &s->mInfluence;
    return NULL;
}

// ai.danger(x, y[, layer]) -> the layer's value at the tile (0: none); layer "threat" (the default), "lost" or
// "influence" (own less enemy strength: negative where the enemy is stronger)
static int LuaDanger(lua_State* L)
{
    EECPUnitsCharge(L, kCostTile);
    EECPUnitsState* s = EECPUnitsGet(EECPUnitsMe(L));
    const std::vector<long>* theLayer = EECPUnitsLayerArg(L, 3, s);
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2) || theLayer == NULL) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, (lua_Integer)EECPUnitsLayerAt(*theLayer, s, (long)lua_tointeger(L, 1), (long)lua_tointeger(L, 2)));
    return 1;
}

// ai.danger_box(x1, y1, x2, y2[, layer]) -> the highest and the summed value of the cells over the box
static int LuaDangerBox(lua_State* L)
{
    EECPUnitsState* s = EECPUnitsGet(EECPUnitsMe(L));
    const std::vector<long>* theLayer = EECPUnitsLayerArg(L, 5, s);
    for (int i = 1; i <= 4; i++)
        if (!lua_isinteger(L, i) || s->mDangerW <= 0 || theLayer == NULL || theLayer->empty()) {
            lua_pushinteger(L, 0);
            lua_pushinteger(L, 0);
            return 2;
        }
    long x1 = (long)lua_tointeger(L, 1) / kDangerCell, y1 = (long)lua_tointeger(L, 2) / kDangerCell;
    long x2 = (long)lua_tointeger(L, 3) / kDangerCell, y2 = (long)lua_tointeger(L, 4) / kDangerCell;
    if (x1 > x2)
        std::swap(x1, x2);
    if (y1 > y2)
        std::swap(y1, y2);
    x1 = x1 < 0 ? 0 : x1;
    y1 = y1 < 0 ? 0 : y1;
    x2 = x2 >= s->mDangerW ? s->mDangerW - 1 : x2;
    y2 = y2 >= s->mDangerH ? s->mDangerH - 1 : y2;
    long theMax = 0, theSum = 0, theCells = 0;
    for (long cy = y1; cy <= y2; cy++)
        for (long cx = x1; cx <= x2; cx++) {
            long v = (*theLayer)[cy * s->mDangerW + cx];
            theMax = v > theMax ? v : theMax;
            theSum += v;
            theCells++;
        }
    EECPUnitsCharge(L, kCostCall + theCells * kCostScan);
    lua_pushinteger(L, (lua_Integer)theMax);
    lua_pushinteger(L, (lua_Integer)theSum);
    return 2;
}

// ai.mark(x, y, r, value, ms): the script's own danger, added to the threat layer until it runs out (at most 64)
static int LuaMark(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    EECPUnitsState* s = EECPUnitsGet(EECPUnitsMe(L));
    for (int i = 1; i <= 5; i++)
        if (!lua_isinteger(L, i)) {
            lua_pushboolean(L, 0);
            return 1;
        }
    if ((long)s->mMarks.size() >= 64 || lua_tointeger(L, 3) < 0 || lua_tointeger(L, 3) > 40 || lua_tointeger(L, 5) <= 0
        || lua_tointeger(L, 5) > 3600000) {
        lua_pushboolean(L, 0);
        return 1;
    }
    DangerMark m;
    m.mX = (long)lua_tointeger(L, 1);
    m.mY = (long)lua_tointeger(L, 2);
    m.mR = (long)lua_tointeger(L, 3);
    long v = (long)lua_tointeger(L, 4);
    m.mValue = v < 0 ? 0 : v > 30000 ? 30000 : v;
    m.mUntil = (long)gPhysics->mGameTime + (long)lua_tointeger(L, 5);
    s->mMarks.push_back(m);
    if (s->mDangerW > 0)
        EECPUnitsDangerAdd(s, m.mX, m.mY, m.mR, m.mValue);   // at once, not only from the next rebuild
    lua_pushboolean(L, 1);
    return 1;
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

// whether a route cell is open for the kind (a land mass or a water body): one of five tiles of the cell is on it
static bool EECPUnitsCellOpen(long inCX, long inCY, long inKind, unsigned short inArea)
{
    static const long kAt[5][2] = {{2, 2}, {0, 0}, {3, 0}, {0, 3}, {3, 3}};
    for (int k = 0; k < 5; k++) {
        U2DPoint<long> theAt(inCX * kDangerCell + kAt[k][0], inCY * kDangerCell + kAt[k][1]);
        if (gContinentManager.GetContinent(theAt, inKind) == inArea)
            return true;
    }
    return false;
}

// the A* over the kDangerCell cells of the start's land mass (or water body), from (x1, y1) to (x2, y2): 10 a
// straight step, 14 a diagonal, plus inAvoid x the threat and inAvoidLost x the lost layer of the cell entered. The
// cells of the way end in s->mIds (start to goal); outExpanded counts the cells closed. false: no way
static bool EECPUnitsCellRoute(EECPUnitsState* s, long inKind, long x1, long y1, long x2, long y2, long inAvoid,
    long inAvoidLost, long& outExpanded)
{
    outExpanded = 0;
    unsigned short theArea = EECPUnitsAreaNear(x1, y1, inKind);
    if (theArea == 0 || EECPUnitsAreaNear(x2, y2, inKind) != theArea)
        return false;
    long w = s->mDangerW, h = s->mDangerH, n = w * h;
    long theStart = (y1 / kDangerCell) * w + x1 / kDangerCell, theGoal = (y2 / kDangerCell) * w + x2 / kDangerCell;
    if (theStart < 0 || theStart >= n || theGoal < 0 || theGoal >= n)
        return false;
    if ((long)s->mLost.size() != n)
        s->mLost.assign((size_t)n, 0);
    s->mG.assign((size_t)n, -1);
    s->mFrom.assign((size_t)n, -1);
    s->mOpen.assign((size_t)n, 0);   // 2 closed, 3 blocked
    s->mHeap.clear();
    s->mG[theStart] = 0;
    long gx = theGoal % w, gy = theGoal / w;
    s->mHeap.push_back(std::make_pair(0L, theStart));
    static const long kStep[8][3] = {{1, 0, 10}, {-1, 0, 10}, {0, 1, 10}, {0, -1, 10},
                                     {1, 1, 14}, {1, -1, 14}, {-1, 1, 14}, {-1, -1, 14}};
    bool theFound = false;
    while (!s->mHeap.empty()) {
        std::pop_heap(s->mHeap.begin(), s->mHeap.end(), std::greater<std::pair<long, long> >());
        long c = s->mHeap.back().second;
        s->mHeap.pop_back();
        if (s->mOpen[c] == 2)
            continue;
        s->mOpen[c] = 2;
        outExpanded++;
        if (c == theGoal) {
            theFound = true;
            break;
        }
        long cx = c % w, cy = c / w;
        for (int k = 0; k < 8; k++) {
            long nx = cx + kStep[k][0], ny = cy + kStep[k][1];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                continue;
            long d = ny * w + nx;
            if (s->mOpen[d] >= 2)
                continue;
            if (d != theGoal && !EECPUnitsCellOpen(nx, ny, inKind, theArea)) {
                s->mOpen[d] = 3;
                continue;
            }
            long g = s->mG[c] + kStep[k][2] + inAvoid * s->mThreat[d] + inAvoidLost * s->mLost[d];
            if (s->mG[d] >= 0 && s->mG[d] <= g)
                continue;
            s->mG[d] = g;
            s->mFrom[d] = c;
            long dx = nx > gx ? nx - gx : gx - nx, dy = ny > gy ? ny - gy : gy - ny;
            long theH = 10 * (dx > dy ? dx : dy) + 4 * (dx < dy ? dx : dy);
            s->mHeap.push_back(std::make_pair(g + theH, d));
            std::push_heap(s->mHeap.begin(), s->mHeap.end(), std::greater<std::pair<long, long> >());
        }
    }
    if (!theFound)
        return false;
    s->mIds.clear();
    for (long c = theGoal; c >= 0; c = s->mFrom[c])
        s->mIds.push_back(c);
    std::reverse(s->mIds.begin(), s->mIds.end());
    return true;
}

// the shared argument reading of ai.route and ai.path_len: the tiles at 1-4 and the options at inOpts
static bool EECPUnitsRouteArgs(lua_State* L, int inOpts, EECPUnitsState* s, EEComputerPlayer* me, long& x1, long& y1,
    long& x2, long& y2, long& outAvoid, long& outAvoidLost, long& outKind)
{
    for (int i = 1; i <= 4; i++)
        if (!lua_isinteger(L, i))
            return false;
    x1 = (long)lua_tointeger(L, 1);
    y1 = (long)lua_tointeger(L, 2);
    x2 = (long)lua_tointeger(L, 3);
    y2 = (long)lua_tointeger(L, 4);
    outAvoid = 4;
    outAvoidLost = 0;
    EECPUnitsOptInt(L, inOpts, "avoid", outAvoid);
    EECPUnitsOptInt(L, inOpts, "avoid_lost", outAvoidLost);
    outAvoid = outAvoid < 0 ? 0 : outAvoid > 100 ? 100 : outAvoid;
    outAvoidLost = outAvoidLost < 0 ? 0 : outAvoidLost > 100 ? 100 : outAvoidLost;
    outKind = EECPUnitsOptBool(L, inOpts, "sea", false) ? kAIPathWaterBody : kAIPathLandMass;
    if (!EECPUnitsTileOK(x1, y1) || !EECPUnitsTileOK(x2, y2))
        return false;
    if (s->mDangerW <= 0)
        EECPUnitsDangerBuild(me, s);
    return true;
}

// ai.route(x1, y1, x2, y2, {avoid = 4, avoid_lost = 0, sea = false}) -> waypoints {{x, y}, ...} (the last is the
// goal) and the cost; nil when the goal is not on the start's land mass (or water body). An A* over 4-tile cells:
// 10 a straight step, 14 a diagonal, plus avoid x the threat layer (and avoid_lost x the lost layer) of the cell
// entered (AI_LUA_UNITS §4.3, step 1)
static int LuaRoute(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    long x1, y1, x2, y2, theAvoid, theAvoidLost, theKind;
    if (!EECPUnitsRouteArgs(L, 5, s, me, x1, y1, x2, y2, theAvoid, theAvoidLost, theKind))
        return 0;
    EECPUnitsCharge(L, 5000);
    long theExpanded = 0;
    bool theFound = EECPUnitsCellRoute(s, theKind, x1, y1, x2, y2, theAvoid, theAvoidLost, theExpanded);
    EECPUnitsCharge(L, theExpanded * 10);
    if (!theFound)
        return 0;
    long w = s->mDangerW;
    long theGoal = s->mIds.back();
    // a waypoint where the direction changes
    s->mIds2.clear();
    for (unsigned long k = 1; k + 1 < s->mIds.size(); k++) {
        long a = s->mIds[k - 1], b = s->mIds[k], c = s->mIds[k + 1];
        if (b - a != c - b)
            s->mIds2.push_back(b);
    }
    while (s->mIds2.size() > 15) {   // at most 16 waypoints with the goal: every second corner goes
        for (unsigned long k = 1; k < s->mIds2.size(); k++)
            s->mIds2.erase(s->mIds2.begin() + k);
    }
    lua_createtable(L, (int)s->mIds2.size() + 1, 0);
    for (unsigned long k = 0; k <= s->mIds2.size(); k++) {
        lua_createtable(L, 2, 0);
        long x = k < s->mIds2.size() ? (s->mIds2[k] % w) * kDangerCell + kDangerCell / 2 : x2;
        long y = k < s->mIds2.size() ? (s->mIds2[k] / w) * kDangerCell + kDangerCell / 2 : y2;
        lua_pushinteger(L, (lua_Integer)x);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, (lua_Integer)y);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, (lua_Integer)(k + 1));
    }
    lua_pushinteger(L, (lua_Integer)s->mG[theGoal]);
    return 2;
}

// ai.path_len(x1, y1, x2, y2, {sea = false}) -> the length in tiles of the way over the cells (no danger), or nil
// when there is none: coarse (4-tile cells), about the walking distance round water and cliffs
static int LuaPathLen(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    long x1, y1, x2, y2, theAvoid, theAvoidLost, theKind;
    if (!EECPUnitsRouteArgs(L, 5, s, me, x1, y1, x2, y2, theAvoid, theAvoidLost, theKind))
        return 0;
    EECPUnitsCharge(L, 2000);
    long theExpanded = 0;
    bool theFound = EECPUnitsCellRoute(s, theKind, x1, y1, x2, y2, 0, 0, theExpanded);
    EECPUnitsCharge(L, theExpanded * 10);
    if (!theFound)
        return 0;
    long theCost = s->mG[s->mIds.back()];
    long theLen = theCost * kDangerCell / 10;
    long dx = x1 > x2 ? x1 - x2 : x2 - x1, dy = y1 > y2 ? y1 - y2 : y2 - y1;
    long theStraight = dx > dy ? dx : dy;
    lua_pushinteger(L, (lua_Integer)(theLen < theStraight ? theStraight : theLen));
    return 1;
}

// ai.find_site(type, x, y, {min = 0, max = 12, safe = false}) -> x, y: the nearest explored tile where the class can be
// placed (EECanPlaceOnTile), ring by ring from (x, y); with safe, only where the threat layer is 0. nil: none
static int LuaFindSite(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    long theType = EECPUnitsClassArg(L, 1);
    if (theType < 0 || (unsigned long)theType >= EEUnitClassCount() || gUnitClassTable[theType] == NULL
        || !lua_isinteger(L, 2) || !lua_isinteger(L, 3) || gWorld.mMap == NULL)
        return 0;
    long x = (long)lua_tointeger(L, 2), y = (long)lua_tointeger(L, 3);
    long theMin = 0, theMax = 12;
    EECPUnitsOptInt(L, 4, "min", theMin);
    EECPUnitsOptInt(L, 4, "max", theMax);
    theMin = theMin < 0 ? 0 : theMin;
    theMax = theMax > 30 ? 30 : theMax;
    bool theSafe = EECPUnitsOptBool(L, 4, "safe", false);
    // margin: the footprint grown by a tile on every side is free as well (the classic build manager's CanBuildAt
    // inMargin, EEPlayerBlockTakes); far: the rings from max inward (FindBuildSite's production order)
    bool theMargin = EECPUnitsOptBool(L, 4, "margin", false);
    bool theFar = EECPUnitsOptBool(L, 4, "far", false);
    long theSize = gUnitClassTable[theType]->GetTileSize();
    EECPUnitsCharge(L, kCostCall + (2 * theMax + 1) * (2 * theMax + 1) * (theMargin ? 6 : 4));   // the most it can test, before it does
    long theX = -1, theY = -1;
    for (long k = theMin; k <= theMax && theX < 0; k++) {
        long r = theFar ? theMax - (k - theMin) : k;
        for (long dy = -r; dy <= r && theX < 0; dy++)
            for (long dx = -r; dx <= r; dx++) {
                if (dx > -r && dx < r && dy > -r && dy < r)
                    continue;   // the ring only
                long tx = x + dx, ty = y + dy;
                EETile* theTile = tx >= 0 && ty >= 0
                    ? static_cast<EETile*>(gWorld.mMap->GetTile((unsigned long)tx, (unsigned long)ty)) : NULL;
                if (theTile == NULL || !theTile->IsVisibleTo(me))
                    continue;
                if (theSafe && EECPUnitsDangerAt(s, tx, ty) > 0)
                    continue;
                if (EECanPlaceOnTile(me, theType, tx, ty, false, 1)
                    && (!theMargin || EEPlayerBlockTakes(me, theType, tx - 1, ty - 1, theSize + 2, theSize + 2, 0))) {
                    theX = tx;
                    theY = ty;
                    break;
                }
            }
    }
    if (theX < 0)
        return 0;
    lua_pushinteger(L, (lua_Integer)theX);
    lua_pushinteger(L, (lua_Integer)theY);
    return 2;
}

// ai.drop_site(x, y[, kind]) -> the id of the player's nearest finished drop site for the resource kind ("food":
// a granary too; the default any), its tile distance, and its centre x, y; nil when it has none
static int LuaDropSite(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsCharge(L, 500);
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2))
        return 0;
    long x = (long)lua_tointeger(L, 1), y = (long)lua_tointeger(L, 2);
    bool theFood = lua_type(L, 3) == LUA_TSTRING && strcmp(lua_tostring(L, 3), "food") == 0;
    EEUnit* theBest = NULL;
    long theBestD2 = 0;
    for (EEUnit* theUnit = me->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        long theType = theUnit->mUnitClass->mUnitType;
        if (theType != kUnitTypeTownCenter && theType != kUnitTypeCapitol && theType != kUnitTypeSettlement
            && !(theFood && theType == kUnitTypeGranary))
            continue;
        if (theUnit->mComplexUnit == NULL || !theUnit->mComplexUnit->mIsBuilt || !(theUnit->mHitPoints > 0) || !EECPUnitsOnMap(theUnit))
            continue;
        long theHalf = theUnit->mUnitClass->GetTileSize() / 2;
        long d2 = EECPUnitsD2(theUnit->GetTileX() + theHalf, theUnit->GetTileY() + theHalf, x, y);
        if (theBest == NULL || d2 < theBestD2 || (d2 == theBestD2 && theUnit->mUnitID < theBest->mUnitID)) {
            theBest = theUnit;
            theBestD2 = d2;
        }
    }
    if (theBest == NULL)
        return 0;
    long theHalf = theBest->mUnitClass->GetTileSize() / 2;
    long d = 0;
    while ((d + 1) * (d + 1) <= theBestD2)
        d++;
    lua_pushinteger(L, (lua_Integer)theBest->mUnitID);
    lua_pushinteger(L, (lua_Integer)d);
    lua_pushinteger(L, (lua_Integer)(theBest->GetTileX() + theHalf));
    lua_pushinteger(L, (lua_Integer)(theBest->GetTileY() + theHalf));
    return 4;
}

// ai.shore(x, y, {sea = id, land = id, max = 20}) -> x, y, sea, land: the nearest explored land tile, ring by ring
// from (x, y), that touches water (of the water body `sea` when given) and lies on the land mass `land` when given;
// nil: none within max rings
static int LuaShore(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2) || gWorld.mMap == NULL)
        return 0;
    long x = (long)lua_tointeger(L, 1), y = (long)lua_tointeger(L, 2);
    long theSea = 0, theLand = 0, theMax = 20;
    EECPUnitsOptInt(L, 3, "sea", theSea);
    EECPUnitsOptInt(L, 3, "land", theLand);
    EECPUnitsOptInt(L, 3, "max", theMax);
    theMax = theMax < 0 ? 0 : theMax > 40 ? 40 : theMax;
    EECPUnitsCharge(L, kCostCall + (2 * theMax + 1) * (2 * theMax + 1) * 6);
    long w, h;
    EECPUnitsMapSize(w, h);
    static const long kSide[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (long r = 0; r <= theMax; r++)
        for (long dy = -r; dy <= r; dy++)
            for (long dx = -r; dx <= r; dx++) {
                if (dx > -r && dx < r && dy > -r && dy < r)
                    continue;
                long tx = x + dx, ty = y + dy;
                if (tx < 0 || ty < 0 || tx >= w || ty >= h)
                    continue;
                EETile* theTile = static_cast<EETile*>(gWorld.mMap->GetTile((unsigned long)tx, (unsigned long)ty));
                if (theTile == NULL || !theTile->IsVisibleTo(me))
                    continue;
                unsigned short theMass = gContinentManager.GetContinent(U2DPoint<long>(tx, ty), kAIPathLandMass);
                if (theMass == 0 || (theLand != 0 && theMass != theLand))
                    continue;
                unsigned short theWater = 0;
                for (int k = 0; k < 4 && theWater == 0; k++) {
                    long nx = tx + kSide[k][0], ny = ty + kSide[k][1];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        continue;
                    unsigned short b = gContinentManager.GetContinent(U2DPoint<long>(nx, ny), kAIPathWaterBody);
                    if (b != 0 && (theSea == 0 || b == theSea))
                        theWater = b;
                }
                if (theWater == 0)
                    continue;
                lua_pushinteger(L, (lua_Integer)tx);
                lua_pushinteger(L, (lua_Integer)ty);
                lua_pushinteger(L, (lua_Integer)theWater);
                lua_pushinteger(L, (lua_Integer)theMass);
                return 4;
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
        || !gContinentManager.mTileFlags[theWaterID * gContinentManager.mFlagsWidth + theLand->mID])
        return 1;
    std::vector<U2DPoint<long> > theShore;
    theWater->GetShoreNextTo(theLand->mID, theShore);
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
            theMap->mUnitRegions->CollectValues(theRect, theUnits, std::less<EEComplexUnit*>());
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

// ai.enemy_force(player, x, y, r) -> n, hp, attack: the player's land soldiers (an attack, a land unit or hero, not a
// building) within r tiles of the tile, counted and summed - every one, seen or not, as the classic's militia counts
// the raiders (EECPRMMilitia). Only for an AI whose manifest entry declares "sees_all" (the classic's information
// cheat); nothing for every other AI (ai.seen is the fog-honest count)
static int LuaEnemyForce(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsCharge(L, kCostCall);
    if (!EECPEngineSeesAll(me) || !lua_isinteger(L, 1) || !lua_isinteger(L, 2) || !lua_isinteger(L, 3)
        || !lua_isinteger(L, 4))
        return 0;
    lua_Integer thePlayer = lua_tointeger(L, 1);
    if (thePlayer < 0 || thePlayer >= (lua_Integer)gWorld.mNumPlayers || gWorld.mPlayers[thePlayer] == NULL)
        return 0;
    long x = (long)lua_tointeger(L, 2), y = (long)lua_tointeger(L, 3), r = (long)lua_tointeger(L, 4);
    long n = 0, theHP = 0, theAttack = 0, theScanned = 0;
    for (EEUnit* u = gWorld.mPlayers[thePlayer]->mFirstUnit; u != NULL; u = static_cast<EEUnit*>(u->mNext)) {
        theScanned++;
        if (u->mComplexUnit == NULL || !(u->mHitPoints > 0) || u->mStats == NULL || u->mStats->mAttack == 0.0f
            || u->mUnitClass->IsLandBuilding())
            continue;
        long theCategory = u->mUnitClass->mCategory;
        if (theCategory != kUnitCategoryLand && theCategory != kUnitCategoryLandMilitaryHero)
            continue;
        long dx = u->GetTileX() - x, dy = u->GetTileY() - y;
        if (dx * dx + dy * dy > r * r)
            continue;
        n++;
        theHP += (long)u->mHitPoints;
        theAttack += UMFloatToLong(u->mStats->mAttack);
    }
    EECPUnitsCharge(L, theScanned * kCostScan);
    lua_pushinteger(L, (lua_Integer)n);
    lua_pushinteger(L, (lua_Integer)theHP);
    lua_pushinteger(L, (lua_Integer)theAttack);
    return 3;
}

// ai.player_file(player) -> {gatherers, types = {[class] = n}, military = {land, sea, air, space} (counts),
// strength = {land, sea, air, space} (the classes' influence)}: the classic's intelligence file of a player
// (EECPIntelligencePlayerFile::Update / AddUnitClass / AddUnitType: every unit of the player alive, seen or not - its
// citizens as gatherers, its non-building units by class, the ones with an attack, calamity power, heal or conversion
// in their domain's strength). Only for an AI whose manifest entry declares "sees_all" (the classic's information
// cheat); nothing for every other AI
static int LuaPlayerFile(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* st = EECPUnitsGet(me);
    EECPUnitsCharge(L, kCostCall);
    if (!EECPEngineSeesAll(me) || !lua_isinteger(L, 1))
        return 0;
    lua_Integer thePlayer = lua_tointeger(L, 1);
    if (thePlayer < 0 || thePlayer >= (lua_Integer)gWorld.mNumPlayers || gWorld.mPlayers[thePlayer] == NULL)
        return 0;
    long theGatherers = 0, theScanned = 0;
    long theCount[4] = {0, 0, 0, 0}, theStrength[4] = {0, 0, 0, 0};   // land, sea, air, space
    std::map<long, long> theTypes;
    for (EEUnit* u = gWorld.mPlayers[thePlayer]->mFirstUnit; u != NULL; u = static_cast<EEUnit*>(u->mNext)) {
        theScanned++;
        if (!(u->mHitPoints > 0) || u->mUnitClass == NULL)
            continue;
        EEUnitClass* c = u->mUnitClass;
        if (c->mCategory == kUnitCategoryLandCitizen || c->mCategory == kUnitCategoryNavalCitizen) {
            theGatherers++;
            continue;
        }
        if (c->mFamily == kUnitFamilyBuilding || c->mFamily == kUnitFamilyAnimal)
            continue;
        theTypes[c->GetUnitType()]++;
        if (!(c->GetAttack() > 0) && !c->mHasCalamityPowers && !c->GetIsMedic() && c->GetConversionRate() == 0.0f)
            continue;
        long d = (c->GetDomains() & kMovementAir) ? 2 : (c->GetDomains() & kMovementSea) ? 1 : (c->GetDomains() & kMovementSpace) ? 3 : 0;
        theCount[d]++;
        theStrength[d] += (long)c->mInfluence;
    }
    EECPUnitsCharge(L, theScanned * kCostScan + (int64_t)theTypes.size() * kCostField);
    static const char* const kDomains[4] = {"land", "sea", "air", "space"};
    lua_createtable(L, 0, 4);
    EECPUnitsSetInt(L, "gatherers", theGatherers);
    lua_createtable(L, 0, (int)theTypes.size());
    for (std::map<long, long>::iterator it = theTypes.begin(); it != theTypes.end(); ++it) {
        lua_pushinteger(L, (lua_Integer)it->second);
        lua_setfield(L, -2, EECPUnitsName(st, it->first));
    }
    lua_setfield(L, -2, "types");
    lua_createtable(L, 0, 4);
    for (int d = 0; d < 4; d++)
        EECPUnitsSetInt(L, kDomains[d], theCount[d]);
    lua_setfield(L, -2, "military");
    lua_createtable(L, 0, 4);
    for (int d = 0; d < 4; d++)
        EECPUnitsSetInt(L, kDomains[d], theStrength[d]);
    lua_setfield(L, -2, "strength");
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
    EECPUnitsSetInt(L, "id", (long)theArea->mID);
    EECPUnitsSetInt(L, "category", (long)theArea->mCategory);
    EECPUnitsSetInt(L, "tiles", (long)theArea->mTiles.Size());
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
    for (size_t i = 0; i < theLand->mForestIDs.size(); i++) {
        unsigned short theID = theLand->mForestIDs[i];
        EEAIForest* theForest = theID < gContinentManager.mForests.size() ? gContinentManager.mForests[theID] : NULL;
        if (theForest != NULL && (theForest->mHull.BoundedContains(theFrom)
                || theForest->GetClosestHullPoint(theFrom).DistanceToSquared(theFrom) <= r * r))
            theForests.push_back(theForest);
    }
    EECPUnitsCharge(L, (int64_t)theLand->mForestIDs.size() * kCostScan * 4);
    if (theForests.empty())
        return 0;
    long thePick = (long)(lua_tointeger(L, 4) % (lua_Integer)theForests.size());
    if (thePick < 0)
        thePick += (long)theForests.size();
    URandom theRandom((unsigned long)lua_tointeger(L, 5));
    EEAIForest* theForest = theForests[thePick];
    if (!(theForest->mTiles.Size() > 0))
        return 0;
    U2DPoint<long> thePoint = theForest->mTiles.GetRandomPoint(theRandom);
    lua_pushinteger(L, (lua_Integer)thePoint.mX);
    lua_pushinteger(L, (lua_Integer)thePoint.mY);
    lua_pushinteger(L, (lua_Integer)theForests.size());
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
    if (theArea == NULL || !(theArea->mTiles.Size() > 0))
        return 0;
    URandom theRandom((unsigned long)lua_tointeger(L, 2));
    U2DPoint<long> thePoint = theArea->mTiles.GetRandomPoint(theRandom);
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

// ai.config{memory_ms = n} -> memory_ms: the enemy memory's life for mobile units (30 s to 30 minutes; the default
// 3 minutes, Age of AI's fight_memory)
static int LuaConfig(lua_State* L)
{
    EECPUnitsCharge(L, kCostCall);
    EECPUnitsState* s = EECPUnitsGet(EECPUnitsMe(L));
    long v;
    if (EECPUnitsOptInt(L, 1, "memory_ms", v))
        s->mMemoryMs = v < 30000 ? 30000 : v > 1800000 ? 1800000 : v;
    lua_pushinteger(L, (lua_Integer)s->mMemoryMs);
    return 1;
}

// ai.used() -> the instructions and charges this call has used so far (a multiple of the hook step, plus the helpers'
// charges), against the budget (ai.used's second value): a script's own profiling; it changes nothing
static int LuaUsed(lua_State* L)
{
    EECPLuaState* s = EECPLuaState::From(L);
    lua_pushinteger(L, (lua_Integer)s->LastInstructions());
    lua_pushinteger(L, (lua_Integer)s->Budget());
    return 2;
}

// ---------------------------------------------------------------------------------------------------------------
// More commands (batch 2)

// ai.heal(ids, target)
static int LuaHeal(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, NULL, "heal", r))
        return r;
    long theID = lua_isinteger(L, 2) ? EECPUnitsForward(s, (long)lua_tointeger(L, 2)) : 0;
    EEUnit* theTarget = EECPUnitsAlive(theID);
    if (theTarget == NULL || theTarget->mPlayer == NULL || !me->IsAlly(theTarget->mPlayer))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    EEMHealUnit theMessage(theID);
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "heal", kOK, (long)s->mIds2.size(), theID, 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.rally(buildings, x, y) or ai.rally(buildings, target): each building's rally goal
static int LuaRally(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsBuilding, "rally", r))
        return r;
    long x = 0, y = 0, theTarget = -1;
    if (lua_isinteger(L, 2) && lua_isinteger(L, 3)) {
        x = (long)lua_tointeger(L, 2);
        y = (long)lua_tointeger(L, 3);
        if (!EECPUnitsTileOK(x, y))
            return EECPUnitsResult(L, kBadArgument, 0, 0);
    } else if (lua_isinteger(L, 2)) {
        EEUnit* theUnit = EECPUnitsKnown(me, (long)lua_tointeger(L, 2));
        if (theUnit == NULL)
            return EECPUnitsResult(L, kNotSeen, 0, 0);
        theTarget = (long)theUnit->mUnitID;
        x = theUnit->GetTileX();
        y = theUnit->GetTileY();
    } else
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    // the message sets the rally of the selected buildings of the named one's type
    long n = (long)s->mIds2.size();
    EEMSetRallyGoal theMessage(s->mIds2[0], GE2DPoint((float)x + 0.5f, (float)y + 0.5f), theTarget, 0);
    EECPUnitsRun(me, s, theMessage, false);
    EECPUnitsTraceCmd(me, s, "rally", kOK, n, x, y);
    return EECPUnitsResult(L, kOK, n, 0);
}

// ai.cancel(buildings, type or tech): one of it out of their queues
static int LuaCancel(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsBuilding, "cancel", r))
        return r;
    long theTech = -1;
    if (lua_type(L, 2) == LUA_TSTRING) {
        long theType = EECPUnitsClassArg(L, 2);
        if (theType >= 0 && (unsigned long)theType < EEUnitClassCount() && gUnitClassTable[theType] != NULL)
            theTech = gUnitClassTable[theType]->mTechnology;
        else
            theTech = EECPNextTechByName(lua_tostring(L, 2));
    } else if (lua_isinteger(L, 2))
        theTech = (long)lua_tointeger(L, 2);
    if (theTech < 0)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    EEMRemoveTechnology theMessage(s->mIds2[0], theTech, false, false);
    EECPUnitsRun(me, s, theMessage, false);
    EECPUnitsTraceCmd(me, s, "cancel", kOK, (long)s->mIds2.size(), theTech, 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
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
    EEMImproveUnit theMessage((long)theUnit->mUnitID, theImprovement);
    unsigned char theSlot = me->mSlot;
    me->mSlot = kBorrowedSlot;
    theMessage.SetOriginatingSlot(kBorrowedSlot);
    theMessage.Execute();
    me->mSlot = theSlot;
    return EECPUnitsResult(L, kOK, 1, 0);
}

// ai.formation(ids, kind)
static int LuaFormation(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsMobile, "formation", r))
        return r;
    if (!lua_isinteger(L, 2) || lua_tointeger(L, 2) < 0 || lua_tointeger(L, 2) > 16)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    EEMChangeFormation theMessage((long)lua_tointeger(L, 2));
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "formation", kOK, (long)s->mIds2.size(), (long)lua_tointeger(L, 2), 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.pack(ids, true/false)
static int LuaPack(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, NULL, "pack", r))
        return r;
    EEMPackUnpack theMessage(lua_toboolean(L, 2) != 0);
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "pack", kOK, (long)s->mIds2.size(), lua_toboolean(L, 2), 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.dropoff(ids, building)
static int LuaDropoff(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsCitizen, "dropoff", r))
        return r;
    long theID = lua_isinteger(L, 2) ? EECPUnitsForward(s, (long)lua_tointeger(L, 2)) : 0;
    EEComplexUnit* theDrop = EECPUnitsOwn(me, theID);
    if (theDrop == NULL || !theDrop->mIsBuilt)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    EEMDropoffResource theMessage(theID);
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "dropoff", kOK, (long)s->mIds2.size(), theID, 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.replant(granary) -> ok: the granary's missing field foundations are laid again (the human's replant command;
// a granary placed by a human's build order comes without its fields)
static int LuaReplant(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    EECPUnitsCharge(L, kCostCommand);
    long theID = lua_isinteger(L, 1) ? EECPUnitsForward(s, (long)lua_tointeger(L, 1)) : 0;
    EEComplexUnit* theGranary = EECPUnitsOwn(me, theID);
    if (theGranary == NULL || !theGranary->mIsBuilt || theGranary->mTileNode == NULL
        || (theGranary->mUnitClass->mUnitType != kUnitTypeGranary && theGranary->mUnitClass->mUnitType != kUnitTypeGranaryImproved))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    s->mIds2.clear();
    s->mIds2.push_back(theID);
    if (!EECPUnitsWithinLimits(s, 1))
        return EECPUnitsResult(L, kLimit, 0, 0);
    EEMReplantFarms theMessage(theID);
    theMessage.Execute();
    EECPUnitsTraceCmd(me, s, "replant", kOK, 1, theID, 0);
    return EECPUnitsResult(L, kOK, 1, 0);
}

// ai.farm(ids, granary or farm, {queue}): the citizens farm by it (the free farms nearest first)
static int LuaFarm(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsCitizen, "farm", r))
        return r;
    long theID = lua_isinteger(L, 2) ? EECPUnitsForward(s, (long)lua_tointeger(L, 2)) : 0;
    if (EECPUnitsOwn(me, theID) == NULL)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    EEMSmartFarmers theMessage(theID, EECPUnitsOptBool(L, 3, "queue", false) ? 1 : 0);
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "farm", kOK, (long)s->mIds2.size(), theID, 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.cast(ids, tech, target) / ai.cast(ids, tech, x, y): a power or calamity by the first caster that can
static int LuaCast(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, NULL, "cast", r))
        return r;
    long theTech = lua_type(L, 2) == LUA_TSTRING ? EECPNextTechByName(lua_tostring(L, 2))
                   : lua_isinteger(L, 2)        ? (long)lua_tointeger(L, 2) : -1;
    if (theTech < 0 || me->mTechnologies->Get(theTech) == NULL)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    long theTarget = -1, x = 0, y = 0;
    if (lua_isinteger(L, 3) && lua_isinteger(L, 4)) {
        x = (long)lua_tointeger(L, 3);
        y = (long)lua_tointeger(L, 4);
    } else if (lua_isinteger(L, 3)) {
        EEUnit* theUnit = EECPUnitsKnown(me, (long)lua_tointeger(L, 3));
        if (theUnit == NULL)
            return EECPUnitsResult(L, kNotSeen, 0, 0);
        theTarget = (long)theUnit->mUnitID;
        x = theUnit->GetTileX();
        y = theUnit->GetTileY();
    } else
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    EEMCastCalamity theMessage(s->mIds2[0], theTarget, x, y, theTech);
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "cast", kOK, (long)s->mIds2.size(), theTech, theTarget);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ---------------------------------------------------------------------------------------------------------------
// Walls, gates, diplomacy (Age of AI's defence.c walls, diplo.c): the human's messages, as the orders above

bool EEDoesWallGateFitOnUnit(EEComplexUnit* inUnit, long inSide, long inUnitType, std::vector<EEUnit*>& outUnits);   // EECPWallManager.cpp

static bool EECPUnitsGatePiece(EEComplexUnit* c)
{
    return dynamic_cast<EEUCGate*>(c->GetUnitClass()) != NULL;
}

// ai.wall(ids, type, x1, y1, x2, y2): the citizens lay a straight wall of the class between the two tiles, as a
// human's drag does (EEPlanWall: along the tiles or exactly diagonal); n = the citizens sent
static int LuaWall(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsCitizen, "wall", r))
        return r;
    // the class: "wall" (or nil) is the epoch's wall as the classic wall manager takes it (GetWallClass)
    long theType = (lua_type(L, 2) == LUA_TNIL || (lua_type(L, 2) == LUA_TSTRING && strcmp(lua_tostring(L, 2), "wall") == 0))
        && me->mCivilizationManager != NULL ? me->mCivilizationManager->GetWallClass() : EECPUnitsClassArg(L, 2);
    if (theType < 0 || (unsigned long)theType >= EEUnitClassCount() || gUnitClassTable[theType] == NULL
        || !gUnitClassTable[theType]->mIsWall || !lua_isinteger(L, 3) || !lua_isinteger(L, 4) || !lua_isinteger(L, 5)
        || !lua_isinteger(L, 6))
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    long x1 = (long)lua_tointeger(L, 3), y1 = (long)lua_tointeger(L, 4), x2 = (long)lua_tointeger(L, 5), y2 = (long)lua_tointeger(L, 6);
    if (gWorld.mMap == NULL || gWorld.mMap->GetTile((unsigned long)x1, (unsigned long)y1) == NULL
        || gWorld.mMap->GetTile((unsigned long)x2, (unsigned long)y2) == NULL)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    // (no CanProduce: the classic wall manager sends the message as it is, EEPlanWall and EEBuildWall check the
    // placement and pay the pieces; a wall tech is never "available" the production way for a computer player)
    long theTech = gUnitClassTable[theType]->mTechnology;
    EETechTreeNode* theNode = me->mTechnologies->Get(theTech);
    if (theNode == NULL) {   // (the wall tech is never "available" the production way: the classic manager never asks)
        EECPUnitsTraceCmd(me, s, "wall", kNotInEpoch, 0, theType, 0);
        return EECPUnitsResult(L, kNotInEpoch, 0, 0);
    }
    if (!me->CanAfford(theNode, 0x20, 1)) {
        EECPUnitsTraceCmd(me, s, "wall", kCannotAfford, 0, theType, x1);
        return EECPUnitsResult(L, kCannotAfford, 0, 0);
    }
    EEMBuildWall theMessage(GE2DPoint((float)x1, (float)y1), GE2DPoint((float)x2, (float)y2), theTech, (long)me->mPlayerID);
    EECPUnitsRun(me, s, theMessage, true);
    EECPUnitsTraceCmd(me, s, "wall", kOK, (long)s->mIds2.size(), theType, x1 * 10000 + y1);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.gate(type, x, y): a gate of the class over the finished wall pieces at the tile, there at once as the human's
// gate button lays it (EEBuildGate: the five pieces turned into the gate's parts); the side is the first the gate
// fits on, as the classic wall manager picks it. Refused cannot_place where no wall of five finished pieces lies
static int LuaGate(lua_State* L)
{
    EECPUnitsCharge(L, kCostCommand);
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    long theType = (lua_type(L, 1) == LUA_TNIL || (lua_type(L, 1) == LUA_TSTRING && strcmp(lua_tostring(L, 1), "gate") == 0))
        && me->mCivilizationManager != NULL ? me->mCivilizationManager->GetGateClass() : EECPUnitsClassArg(L, 1);
    if (theType < 0 || (unsigned long)theType >= EEUnitClassCount() || gUnitClassTable[theType] == NULL
        || dynamic_cast<EEUCGate*>(gUnitClassTable[theType]) == NULL || !lua_isinteger(L, 2) || !lua_isinteger(L, 3))
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
    EEMBuildGate theMessage(GE2DPoint((float)x, (float)y), theTech, theSide, (long)me->mPlayerID);
    theMessage.Execute();
    EECPUnitsTraceCmd(me, s, "gate", kOK, 1, theType, x * 10000 + y);
    return EECPUnitsResult(L, kOK, 1, 0);
}

// ai.patrol(ids, {{x, y}, ...}): the mobile units patrol through 2..8 points (EEMAddPatrolPoint for each, as the
// human's patrol clicks: the first makes the patrol goal, the others add points)
static int LuaPatrol(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsMobile, "patrol", r))
        return r;
    if (lua_type(L, 2) != LUA_TTABLE)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    long n = (long)lua_rawlen(L, 2);
    if (n < 2 || n > 8)
        return EECPUnitsResult(L, kBadArgument, 0, 0);
    std::vector<GE2DPoint> thePoints;
    for (long k = 1; k <= n; k++) {
        lua_rawgeti(L, 2, k);
        if (lua_type(L, -1) != LUA_TTABLE) {
            lua_pop(L, 1);
            return EECPUnitsResult(L, kBadArgument, 0, 0);
        }
        lua_rawgeti(L, -1, 1);
        lua_rawgeti(L, -2, 2);
        bool ok = lua_isinteger(L, -2) && lua_isinteger(L, -1);
        long x = ok ? (long)lua_tointeger(L, -2) : -1, y = ok ? (long)lua_tointeger(L, -1) : -1;
        lua_pop(L, 3);
        if (!ok || gWorld.mMap == NULL || gWorld.mMap->GetTile((unsigned long)x, (unsigned long)y) == NULL)
            return EECPUnitsResult(L, kBadArgument, 0, 0);
        thePoints.push_back(GE2DPoint((float)x, (float)y));
    }
    for (unsigned long k = 0; k < thePoints.size(); k++) {
        EEMAddPatrolPoint theMessage(thePoints[k]);
        EECPUnitsRun(me, s, theMessage, true);
    }
    EECPUnitsTraceCmd(me, s, "patrol", kOK, (long)s->mIds2.size(), n, (long)thePoints[0].GetX() * 10000 + (long)thePoints[0].GetY());
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

// ai.lock(ids, on): the gates locked or unlocked (their three moving parts)
static int LuaLock(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    EECPUnitsState* s = EECPUnitsGet(me);
    int r;
    if (!EECPUnitsBegin(L, me, s, false, EECPUnitsGatePiece, "lock", r))
        return r;
    EEMLockUnlock theMessage(lua_toboolean(L, 2) != 0);
    EECPUnitsRun(me, s, theMessage, false);
    EECPUnitsTraceCmd(me, s, "lock", kOK, (long)s->mIds2.size(), lua_toboolean(L, 2), 0);
    return EECPUnitsResult(L, kOK, (long)s->mIds2.size(), 0);
}

static long EECPUnitsOptAmount(lua_State* L, int inTable, const char* inKey)
{
    if (lua_type(L, inTable) != LUA_TTABLE)
        return 0;
    lua_getfield(L, inTable, inKey);
    long v = lua_isinteger(L, -1) ? (long)lua_tointeger(L, -1) : 0;
    lua_pop(L, 1);
    return v < 0 ? 0 : v;
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
    EEMExchangeResources theMessage(theSell ? 1 : 0, theResource, n == 5 ? 1 : 0);
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

// ai.techs{epoch = n, id = tech} -> the player's tech tree nodes of epochs n..0 (default: the current epoch) in the
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

// ai.gatherers() -> {[resource id] = n}: the player's own living citizens and boats whose gather goal is that resource
// (walking there, waiting or working; whoever holds them: the runners' lists, which ai.resource's `workers` counts, do
// not hold a script's units). One scan of the own units
static int LuaGatherers(lua_State* L)
{
    EEComputerPlayer* me = EECPUnitsMe(L);
    std::map<long, long> theCounts;
    long theTested = 0;
    for (EEUnit* theUnit = me->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        theTested++;
        EEComplexUnit* c = theUnit->mComplexUnit;
        if (c == NULL || !(theUnit->mHitPoints > 0) || !EECPUnitsCitizen(c))
            continue;
        EEUGGather* theGather = dynamic_cast<EEUGGather*>(c->GetGoalState());
        if (theGather != NULL && theGather->mResource != NULL && theGather->mResource->mIsValid)
            theCounts[(long)theGather->mResource->mUnitID]++;
    }
    EECPUnitsCharge(L, kCostCall + theTested * kCostScan + (int64_t)theCounts.size() * kCostId);
    lua_createtable(L, 0, (int)theCounts.size());
    for (std::map<long, long>::iterator it = theCounts.begin(); it != theCounts.end(); ++it) {
        lua_pushinteger(L, (lua_Integer)it->second);
        lua_rawseti(L, -2, (lua_Integer)it->first);
    }
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
        {"danger", LuaDanger}, {"danger_box", LuaDangerBox}, {"mark", LuaMark}, {"route", LuaRoute},
        {"find_site", LuaFindSite}, {"heal", LuaHeal}, {"rally", LuaRally}, {"cancel", LuaCancel},
        {"improve", LuaImprove}, {"formation", LuaFormation}, {"pack", LuaPack}, {"dropoff", LuaDropoff},
        {"farm", LuaFarm}, {"cast", LuaCast}, {"replant", LuaReplant},
        {"path_len", LuaPathLen}, {"drop_site", LuaDropSite}, {"shore", LuaShore}, {"water_sites", LuaWaterSites}, {"area", LuaArea}, {"garrison_fill", LuaGarrisonFill}, {"enemy_force", LuaEnemyForce}, {"player_file", LuaPlayerFile}, {"area_point", LuaAreaPoint}, {"forest_point", LuaForestPoint}, {"enemy_near", LuaEnemyNear}, {"unexplored", LuaUnexplored},
        {"config", LuaConfig}, {"used", LuaUsed},
        {"wall", LuaWall}, {"gate", LuaGate}, {"lock", LuaLock}, {"tribute", LuaTribute}, {"trade", LuaTrade},
        {"diplomacy", LuaDiplomacy}, {"patrol", LuaPatrol},
        {"tuning", LuaTuning}, {"techs", LuaTechs}, {"epoch", LuaEpoch}, {"gatherers", LuaGatherers},
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
            EECPUnitsDangerBuild(theCP, s);
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

static void EECPUnitsSaveState(const EECPUnitsState* s, std::string& outBytes)
{
    outBytes.append("EENU", 4);
    EECPUnitsPut(outBytes, 3);
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
    // version 2: the hurt spots and the marks
    EECPUnitsPut(outBytes, (long)s->mHurt.size());
    for (unsigned long k = 0; k < s->mHurt.size(); k++) {
        EECPUnitsPut(outBytes, s->mHurt[k].first);
        EECPUnitsPut(outBytes, s->mHurt[k].second);
    }
    EECPUnitsPut(outBytes, (long)s->mMarks.size());
    for (unsigned long k = 0; k < s->mMarks.size(); k++) {
        EECPUnitsPut(outBytes, s->mMarks[k].mX);
        EECPUnitsPut(outBytes, s->mMarks[k].mY);
        EECPUnitsPut(outBytes, s->mMarks[k].mR);
        EECPUnitsPut(outBytes, s->mMarks[k].mValue);
        EECPUnitsPut(outBytes, s->mMarks[k].mUntil);
    }
    // version 3: the lost layer's trades
    EECPUnitsPut(outBytes, (long)s->mTrades.size());
    for (std::map<std::pair<long, long>, std::pair<long, long> >::const_iterator it = s->mTrades.begin(); it != s->mTrades.end(); ++it) {
        EECPUnitsPut(outBytes, it->first.first);
        EECPUnitsPut(outBytes, it->first.second);
        EECPUnitsPut(outBytes, it->second.first);
        EECPUnitsPut(outBytes, it->second.second);
    }
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

static bool EECPUnitsLoadState(EECPUnitsState* s, const std::string& inBytes)
{
    if (inBytes.size() < 8 || inBytes.compare(0, 4, "EENU") != 0)
        return false;
    Getter g(inBytes);
    g.mOffset = 4;
    long theVersion = g.Get();
    if (theVersion < 1 || theVersion > 3)
        return false;
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
    s->mHurt.clear();
    s->mMarks.clear();
    if (theVersion >= 2) {
        n = g.Count(256);
        for (long k = 0; k < n && g.mOK; k++) {
            long a = g.Get(), t = g.Get();
            s->mHurt.push_back(std::make_pair(a, t));
        }
        n = g.Count(64);
        for (long k = 0; k < n && g.mOK; k++) {
            DangerMark m;
            m.mX = g.Get();
            m.mY = g.Get();
            m.mR = g.Get();
            m.mValue = g.Get();
            m.mUntil = g.Get();
            s->mMarks.push_back(m);
        }
    }
    s->mTrades.clear();
    if (theVersion >= 3) {
        n = g.Count(kMaxTrades);
        for (long k = 0; k < n && g.mOK; k++) {
            long a = g.Get(), b = g.Get(), c = g.Get(), d = g.Get();
            s->mTrades[std::make_pair(a, b)] = std::make_pair(c, d);
        }
    }
    s->mLost.clear();
    s->mInfluence.clear();
    s->mThreat.clear();
    s->mDangerW = s->mDangerH = 0;   // rebuilt at the next game second
    return g.mOK && g.mOffset == inBytes.size();
}

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
    EECPUnitsState theCopy;
    std::string theAgain;
    bool theDecoded = EECPUnitsLoadState(&theCopy, inBytes);
    if (theDecoded)
        EECPUnitsSaveState(&theCopy, theAgain);
    EECPTraceScope theScope("next");
    EE_CP_TRACE(const_cast<EEComputerPlayer*>(inPlayer), "next.savecheck", "units=%s bytes=%lu held=%lu events=%lu memory=%lu trades=%lu",
        theDecoded && theAgain == inBytes ? "same" : "DIFFERENT", (unsigned long)inBytes.size(), (unsigned long)s->mHeld.size(),
        (unsigned long)s->mEvents.size(), (unsigned long)s->mMemory.size(), (unsigned long)s->mTrades.size());
}

void EECPUnitsSave(const EEComputerPlayer* inPlayer, std::string& outBytes)
{
    outBytes.clear();
    const EECPUnitsState* s = inPlayer->mUnitsState;
    if (s == NULL)
        return;
    EECPUnitsSaveState(s, outBytes);
    EECPUnitsSaveCheck(inPlayer, s, outBytes);
}

bool EECPUnitsLoad(EEComputerPlayer* inPlayer, const std::string& inBytes)
{
    if (inBytes.empty())
        return true;
    return EECPUnitsLoadState(EECPUnitsGet(inPlayer), inBytes);
}
