// The next engine's army (ee/EECPNext.h; docs/AI_LUA_PLAN.md §5 "S4 as built"): the group verbs (attack, raid,
// defend, retreat, regroup, disband, scout), the groups' step (targets and moves from what the player sees) and the
// scouting memory (the enemies' gather spots and towers). New in this port: no original address.
//
// A group's soldiers leave the classic attack commander (EEComputerPlayer::DetachUnitFromAI: out of Personnel and
// the area groups, mIsDetachedFromAI) and stay out while the group holds them (UnitGoalDone asks EECPNextHoldsUnit
// before it gives a detached unit back). Every step the army gives them their goals itself: EEUGAttackUnit on a seen
// enemy unit, EEUGLocation towards the group's place. What a group fights is chosen from the enemy units whose tile
// the player sees now (EETile::IsSeenBy), so the groups are as fog-honest as the view. Disbanding (or a script that
// never names the soldiers) leaves them to the classic commander as before.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdarg>
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
#include "ee/EEMap.h"
#include "ee/EEMapDimensions.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPBuildManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPTrace.h"
#include "ee/EECPNext.h"

static const long kNextArmyMaxCount = 100;     // a larger count in one order is a bad argument
static const long kNextEngageRange = 12;       // attack, raid: the seen enemies this near the group's centre
static const long kNextRaidSoldierRange = 6;   // a raid fights soldiers only this near
static const long kNextCitizenRange = 20;      // an attack goes for the citizens it sees this near (before buildings)
static const long kNextBoatRange = 7;          // ranged units shoot at boats and ships this near the centre
static const long kNextTowerAvoid = 9;         // a raid leaves targets this near a known tower alone
static const long kNextArrive = 6;             // a group this near its place has arrived
static const long kNextLeg = 14;               // a move goes this far ahead of the group's centre
static const long kNextFormMs = 20000;         // the longest a group forms
static const long kNextMoveMs = 10000;         // a move is given again after this long
static const long kNextScoutStuckMs = 45000;   // a scout that does not reach its place in this long skips it
static const long kNextScoutGrid = 8;          // the scout's grid of places (tiles)
static const long kNextSpotRange = 8;          // gather spots this near are one spot
static const long kNextSpotsPerPlayer = 8;
static const long kNextTowersPerPlayer = 32;

enum EECPNextSeenKind {
    kSeenSoldier = 0,
    kSeenCitizen,
    kSeenBoat,
    kSeenTower,
    kSeenBuilding,
    kSeenShip,
};

const char* EECPNextTaskName(long inTask)
{
    switch (inTask) {
    case kNextTaskAttack: return "attack";
    case kNextTaskRaid: return "raid";
    case kNextTaskDefend: return "defend";
    case kNextTaskRetreat: return "retreat";
    case kNextTaskScout: return "scout";
    default: return "?";
    }
}

const char* EECPNextGroupStateName(long inState)
{
    switch (inState) {
    case kNextGroupForming: return "forming";
    case kNextGroupMoving: return "moving";
    case kNextGroupFighting: return "fighting";
    case kNextGroupArrived: return "arrived";
    case kNextGroupDone: return "done";
    default: return "?";
    }
}

static long long EECPNextD2(long inX, long inY, long inX2, long inY2)
{
    long long theDX = inX - inX2, theDY = inY - inY2;
    return theDX * theDX + theDY * theDY;
}

static bool EECPNextNear(long inX, long inY, long inX2, long inY2, long inRange)
{
    return EECPNextD2(inX, inY, inX2, inY2) <= (long long)inRange * inRange;
}

// the integer square root (as the view's)
static long EECPNextRoot(long long inValue)
{
    if (inValue <= 0)
        return 0;
    unsigned long long theValue = (unsigned long long)inValue, theRoot = 0, theBit = 1ULL << 62;
    while (theBit > theValue)
        theBit >>= 2;
    while (theBit != 0) {
        if (theValue >= theRoot + theBit) {
            theValue -= theRoot + theBit;
            theRoot = (theRoot >> 1) + theBit;
        } else
            theRoot >>= 1;
        theBit >>= 2;
    }
    return (long)theRoot;
}

static long EECPNextUnitStrength(EEUnit* inUnit)
{
    return (long)inUnit->mHitPoints * (long)UMFloatToLong(inUnit->mStats->mAttack);
}

// a land soldier of the player's that a group may take or keep (as the view counts soldiers)
static bool EECPNextArmySoldier(EEUnit* inUnit)
{
    if (inUnit == NULL || inUnit->mComplexUnit == NULL || !(inUnit->mHitPoints > 0) || inUnit->mStats == NULL
        || inUnit->mStats->mAttack == 0.0f || inUnit->mUnitClass->IsBuilding())
        return false;
    long theCategory = inUnit->mUnitClass->mCategory;
    return theCategory == kUnitCategoryLand || theCategory == kUnitCategoryLandMilitaryHero;
}

static bool EECPNextRanged(EEUnit* inUnit)
{
    return inUnit->mUnitClass->mProjectileClass != NULL;
}

static EEPlayer* EECPNextArmyFirstEnemy(EEComputerPlayer* inPlayer)
{
    for (unsigned long p = 1; p < gWorld.mNumPlayers; p++) {
        EEPlayer* theOther = gWorld.mPlayers[p];
        if (theOther != NULL && theOther != inPlayer && !inPlayer->IsAlly(theOther) && !theOther->mObserverMode
            && theOther->mFirstUnit != NULL)
            return theOther;
    }
    return NULL;
}

static bool EECPNextEnemyPlayer(EEComputerPlayer* inPlayer, long inID)
{
    if (inID < 1 || (unsigned long)inID >= gWorld.mNumPlayers || inID >= 64)
        return false;
    EEPlayer* theOther = gWorld.mPlayers[inID];
    return theOther != NULL && theOther != inPlayer && !inPlayer->IsAlly(theOther) && !theOther->mObserverMode
        && theOther->mFirstUnit != NULL;
}

// a group's unit (NULL: gone, dead or no longer the player's)
static EEComplexUnit* EECPNextGroupUnit(EEComputerPlayer* inPlayer, long inID)
{
    EEUnit* theUnit = gWorld.GetUnit((unsigned long)inID);
    if (theUnit == NULL || theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->mPlayer != inPlayer)
        return NULL;
    return theUnit->mComplexUnit;
}

static bool EECPNextCanOrder(EEComplexUnit* inUnit)
{
    return inUnit->mHitPoints > 0 && inUnit->GetTileX() != -10000 && !inUnit->mIsDisabled && inUnit->mContainer == NULL;
}

// the goals (as EECPAttackSubGroup::AttackWith and SendUnitTo give them)
static void EECPNextGiveAttack(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    if (!EECPNextCanOrder(inUnit))
        return;
    EEUGAttackUnit* theGoal = new EEUGAttackUnit(inUnit, inTarget, true, false);
    inUnit->SetGoal(theGoal, false, 0);
    inUnit->StartAction(2016);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

static void EECPNextGiveMove(EEComplexUnit* inUnit, long inX, long inY, bool inForced)
{
    if (!EECPNextCanOrder(inUnit) || gWorld.mMap == NULL || inX < 0 || inY < 0
        || gWorld.mMap->GetTile((unsigned long)inX, (unsigned long)inY) == NULL)
        return;
    EEUGLocation* theGoal = new EEUGLocation(GE2DPoint(inX + 0.5f, inY + 0.5f), true, inForced, false, 0.0f, false, false);
    inUnit->SetGoal(theGoal, false, 0);
    inUnit->StartAction(kActionPrepareToMove);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

static void EECPNextReleaseUnit(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    if (inUnit->mIsDetachedFromAI)
        inUnit->mIsDetachedFromAI = false;
    if (inPlayer->mCombatManager != NULL)
        inPlayer->mCombatManager->UnitAdded(inUnit);
}

bool EECPNextHoldsUnit(const EEComputerPlayer* inPlayer, const EEComplexUnit* inUnit)
{
    const EECPNextState* theState = inPlayer->mNextState;
    if (theState == NULL || inUnit == NULL)
        return false;
    long theID = (long)inUnit->mUnitID;
    for (unsigned long g = 0; g < theState->mGroups.size(); g++)
        if (std::find(theState->mGroups[g].mUnits.begin(), theState->mGroups[g].mUnits.end(), theID) != theState->mGroups[g].mUnits.end())
            return true;
    return false;
}

static std::set<long> EECPNextHeld(const EECPNextState* inState)
{
    std::set<long> theHeld;
    for (unsigned long g = 0; g < inState->mGroups.size(); g++)
        theHeld.insert(inState->mGroups[g].mUnits.begin(), inState->mGroups[g].mUnits.end());
    return theHeld;
}

static EECPNextGroup* EECPNextFindGroup(EECPNextState* inState, long inID)
{
    for (unsigned long g = 0; g < inState->mGroups.size(); g++)
        if (inState->mGroups[g].mID == inID)
            return &inState->mGroups[g];
    return NULL;
}

// the group's centre (the mean tile of its units on the map); false when none is
static bool EECPNextCentre(EEComputerPlayer* inPlayer, const EECPNextGroup& inGroup, long& outX, long& outY)
{
    long long theX = 0, theY = 0;
    long theCount = 0;
    for (unsigned long k = 0; k < inGroup.mUnits.size(); k++) {
        EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, inGroup.mUnits[k]);
        if (theUnit == NULL || theUnit->mContainer != NULL || theUnit->GetTileX() == -10000)
            continue;
        theX += theUnit->GetTileX();
        theY += theUnit->GetTileY();
        theCount++;
    }
    if (theCount == 0)
        return false;
    outX = (long)(theX / theCount);
    outY = (long)(theY / theCount);
    return true;
}

static long EECPNextGroupStrength(EEComputerPlayer* inPlayer, const EECPNextGroup& inGroup)
{
    long theStrength = 0;
    for (unsigned long k = 0; k < inGroup.mUnits.size(); k++) {
        EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, inGroup.mUnits[k]);
        if (theUnit != NULL)
            theStrength += EECPNextUnitStrength(theUnit);
    }
    return theStrength;
}

static bool EECPNextNearTower(const EECPNextState* inState, long inX, long inY)
{
    for (unsigned long t = 0; t < inState->mTowers.size(); t++)
        if (EECPNextNear(inX, inY, inState->mTowers[t].mX, inState->mTowers[t].mY, kNextTowerAvoid))
            return true;
    return false;
}

// the latest gather spot of a player with no known tower near (away from inNotX/Y when it is set); false: none
static bool EECPNextBestSpot(const EECPNextState* inState, long inPlayer, long inNotX, long inNotY, long& outX, long& outY)
{
    long theBest = -1;
    for (unsigned long k = 0; k < inState->mSpots.size(); k++) {
        const EECPNextSpot& theSpot = inState->mSpots[k];
        if (theSpot.mPlayer != inPlayer || EECPNextNearTower(inState, theSpot.mX, theSpot.mY)
            || (inNotX >= 0 && EECPNextNear(theSpot.mX, theSpot.mY, inNotX, inNotY, 10)))
            continue;
        if (theBest < 0 || theSpot.mTime > inState->mSpots[theBest].mTime)
            theBest = (long)k;
    }
    if (theBest < 0)
        return false;
    outX = inState->mSpots[theBest].mX;
    outY = inState->mSpots[theBest].mY;
    return true;
}

static void EECPNextTraceGroup(EEComputerPlayer* inPlayer, const char* inWhat, const EECPNextGroup& inGroup)
{
    EE_CP_TRACE(inPlayer, "next.group", "%s id=%ld task=%s state=%s units=%lu target=%ld tx=%ld ty=%ld lost=%ld", inWhat,
        inGroup.mID, EECPNextTaskName(inGroup.mTask), EECPNextGroupStateName(inGroup.mState),
        (unsigned long)inGroup.mUnits.size(), inGroup.mTarget, inGroup.mX, inGroup.mY, inGroup.mLost);
}

static void EECPNextSetState(EEComputerPlayer* inPlayer, EECPNextGroup& ioGroup, long inState, long inNow)
{
    if (ioGroup.mState == inState)
        return;
    ioGroup.mState = inState;
    ioGroup.mStateTime = inNow;
    EECPNextTraceGroup(inPlayer, "state", ioGroup);
}

// ---------------------------------------------------------------------------------------------------------------
// What the player sees, and remembers

static void EECPNextScan(EEComputerPlayer* inPlayer, EECPNextState* inState, long inNow)
{
    inState->mSeen.clear();
    long long theArmyX[64], theArmyY[64];
    long theArmyCount[64];
    for (long p = 0; p < 64; p++) {
        theArmyX[p] = theArmyY[p] = 0;
        theArmyCount[p] = 0;
        inState->mSeenCitizens[p] = 0;
    }
    for (unsigned long p = 1; p < gWorld.mNumPlayers && p < 64; p++) {
        EEPlayer* theOther = gWorld.mPlayers[p];
        if (theOther == NULL || theOther == inPlayer || inPlayer->IsAlly(theOther) || theOther->mObserverMode)
            continue;
        for (EEUnit* theUnit = theOther->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->mTile == NULL || theUnit->mStats == NULL
                || theUnit->mComplexUnit->mContainer != NULL || !static_cast<EETile*>(theUnit->mTile)->IsSeenBy(inPlayer))
                continue;
            EEUnitClass* theClass = theUnit->mUnitClass;
            long theKind = -1;
            if (theClass->IsBuilding()) {
                if (theClass->mIsTower)
                    theKind = kSeenTower;
                else if (!theClass->mIsWall)
                    theKind = kSeenBuilding;
            } else if (theClass->mCategory == kUnitCategoryLandCitizen)
                theKind = kSeenCitizen;
            else if (theClass->mCategory == kUnitCategoryNavalCitizen)
                theKind = kSeenBoat;
            else if (EECPNextArmySoldier(theUnit))
                theKind = kSeenSoldier;
            else if ((theClass->mDomains & kMovementSea) && theUnit->mStats->mAttack != 0.0f)
                theKind = kSeenShip;
            if (theKind < 0)
                continue;
            EECPNextSeen theSeen;
            theSeen.mUnit = (long)theUnit->mUnitID;
            theSeen.mPlayer = (long)p;
            theSeen.mX = theUnit->GetTileX();
            theSeen.mY = theUnit->GetTileY();
            theSeen.mKind = theKind;
            theSeen.mHitPoints = (long)theUnit->mHitPoints;
            theSeen.mStrength = theKind == kSeenSoldier || theKind == kSeenTower || theKind == kSeenShip ? EECPNextUnitStrength(theUnit) : 0;
            theSeen.mContinent = theKind == kSeenSoldier || theKind == kSeenCitizen
                ? gContinentManager.GetContinent(theUnit, kAIPathLandMass) : 0;
            inState->mSeen.push_back(theSeen);
            if (theKind == kSeenSoldier) {
                theArmyX[p] += theSeen.mX;
                theArmyY[p] += theSeen.mY;
                theArmyCount[p]++;
            } else if (theKind == kSeenCitizen || theKind == kSeenBoat) {
                inState->mSeenCitizens[p]++;
                // a gather spot: one within kNextSpotRange is the same spot (its time renewed)
                bool theKnown = false;
                for (unsigned long k = 0; k < inState->mSpots.size() && !theKnown; k++)
                    if (inState->mSpots[k].mPlayer == (long)p
                        && EECPNextNear(inState->mSpots[k].mX, inState->mSpots[k].mY, theSeen.mX, theSeen.mY, kNextSpotRange)) {
                        inState->mSpots[k].mTime = inNow;
                        theKnown = true;
                    }
                if (!theKnown) {
                    long theCount = 0, theOldest = -1;
                    for (unsigned long k = 0; k < inState->mSpots.size(); k++)
                        if (inState->mSpots[k].mPlayer == (long)p) {
                            theCount++;
                            if (theOldest < 0 || inState->mSpots[k].mTime < inState->mSpots[theOldest].mTime)
                                theOldest = (long)k;
                        }
                    EECPNextSpot theSpot = { (long)p, theSeen.mX, theSeen.mY, inNow, 0 };
                    if (theCount < kNextSpotsPerPlayer)
                        inState->mSpots.push_back(theSpot);
                    else
                        inState->mSpots[theOldest] = theSpot;
                }
            } else if (theKind == kSeenTower) {
                bool theKnown = false;
                long theCount = 0;
                for (unsigned long t = 0; t < inState->mTowers.size(); t++) {
                    if (inState->mTowers[t].mPlayer == (long)p)
                        theCount++;
                    if (inState->mTowers[t].mUnit == theSeen.mUnit) {
                        inState->mTowers[t].mTime = inNow;
                        theKnown = true;
                    }
                }
                EECPNextSpot theTower = { (long)p, theSeen.mX, theSeen.mY, inNow, theSeen.mUnit };
                if (!theKnown && theCount < kNextTowersPerPlayer)
                    inState->mTowers.push_back(theTower);
            }
        }
    }
    for (long p = 0; p < 64; p++)
        if (theArmyCount[p] > 0) {
            inState->mArmyX[p] = (long)(theArmyX[p] / theArmyCount[p]);
            inState->mArmyY[p] = (long)(theArmyY[p] / theArmyCount[p]);
        }
    // forgotten: a tower whose tile is in sight with the tower gone; a gather spot in sight with no gatherer near for
    // a minute
    for (long t = (long)inState->mTowers.size() - 1; t >= 0; t--) {
        const EECPNextSpot& theTower = inState->mTowers[t];
        if (theTower.mTime == inNow)
            continue;
        EETile* theTile = gWorld.mMap != NULL ? gWorld.mMap->GetTile((unsigned long)theTower.mX, (unsigned long)theTower.mY) : NULL;
        if (theTile == NULL || theTile->IsSeenBy(inPlayer))
            inState->mTowers.erase(inState->mTowers.begin() + t);
    }
    for (long k = (long)inState->mSpots.size() - 1; k >= 0; k--) {
        const EECPNextSpot& theSpot = inState->mSpots[k];
        if (inNow - theSpot.mTime < 60000)
            continue;
        EETile* theTile = gWorld.mMap != NULL ? gWorld.mMap->GetTile((unsigned long)theSpot.mX, (unsigned long)theSpot.mY) : NULL;
        if (theTile == NULL || theTile->IsSeenBy(inPlayer))
            inState->mSpots.erase(inState->mSpots.begin() + k);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The groups' step

// the priority of a seen unit for a task (0 first; -1 not a target)
static long EECPNextPriority(long inTask, long inKind)
{
    switch (inTask) {
    case kNextTaskAttack:
        switch (inKind) {
        case kSeenSoldier: return 0;
        case kSeenCitizen: return 1;
        case kSeenTower: return 2;
        case kSeenBuilding: return 3;
        default: return 4;      // boats and ships: the ranged units
        }
    case kNextTaskRaid:
        switch (inKind) {
        case kSeenCitizen: return 0;
        case kSeenBoat: return 1;
        case kSeenSoldier: return 2;
        default: return -1;
        }
    case kNextTaskDefend:
        switch (inKind) {
        case kSeenSoldier: return 0;
        case kSeenCitizen: return 1;
        case kSeenTower: return 1;
        case kSeenBuilding: return 2;
        default: return 3;
        }
    default:
        return -1;
    }
}

// the seen enemies the group would fight now (indexes into mSeen)
static void EECPNextCandidates(EEComputerPlayer* inPlayer, const EECPNextState* inState, const EECPNextGroup& inGroup,
    long inX, long inY, std::vector<long>& outCandidates)
{
    outCandidates.clear();
    for (unsigned long k = 0; k < inState->mSeen.size(); k++) {
        const EECPNextSeen& theSeen = inState->mSeen[k];
        if (EECPNextPriority(inGroup.mTask, theSeen.mKind) < 0)
            continue;
        if (inGroup.mTask == kNextTaskDefend) {
            if (!EECPNextNear(theSeen.mX, theSeen.mY, inGroup.mX, inGroup.mY, inGroup.mRadius)
                && !EECPNextNear(theSeen.mX, theSeen.mY, inX, inY, 6))
                continue;
        } else {
            long theRange = inGroup.mTask == kNextTaskRaid && theSeen.mKind == kSeenSoldier ? kNextRaidSoldierRange
                : inGroup.mTask == kNextTaskAttack && theSeen.mKind == kSeenCitizen ? kNextCitizenRange : kNextEngageRange;
            if (theSeen.mKind == kSeenBoat || theSeen.mKind == kSeenShip)
                theRange = kNextBoatRange;
            if (!EECPNextNear(theSeen.mX, theSeen.mY, inX, inY, theRange))
                continue;
            if (inGroup.mTask == kNextTaskRaid && EECPNextNearTower(inState, theSeen.mX, theSeen.mY))
                continue;
        }
        outCandidates.push_back((long)k);
    }
}

// each unit's target: it keeps a target still in the candidates at the best priority it can reach, else takes the
// nearest one (a target already taken by others counts as a little further); returns the units fighting
static long EECPNextFight(EEComputerPlayer* inPlayer, EECPNextState* inState, EECPNextGroup& ioGroup,
    const std::vector<long>& inCandidates)
{
    std::map<long, long> theTaken;   // unit id -> attackers
    for (unsigned long k = 0; k < ioGroup.mUnits.size(); k++)
        if (ioGroup.mAims[k] != 0)
            theTaken[ioGroup.mAims[k]]++;
    long theFighting = 0;
    for (unsigned long k = 0; k < ioGroup.mUnits.size(); k++) {
        EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, ioGroup.mUnits[k]);
        if (theUnit == NULL || !EECPNextCanOrder(theUnit)) {
            ioGroup.mAims[k] = 0;
            continue;
        }
        bool theRanged = EECPNextRanged(theUnit);
        unsigned short theContinent = gContinentManager.GetContinent(theUnit, kAIPathLandMass);
        long theBestPriority = 100, theCurrent = -1;
        for (unsigned long c = 0; c < inCandidates.size(); c++) {
            const EECPNextSeen& theSeen = inState->mSeen[inCandidates[c]];
            if ((theSeen.mKind == kSeenBoat || theSeen.mKind == kSeenShip) && !theRanged)
                continue;
            if (theSeen.mContinent != 0 && theContinent != 0 && theSeen.mContinent != theContinent)
                continue;
            long thePriority = EECPNextPriority(ioGroup.mTask, theSeen.mKind);
            if (thePriority < theBestPriority)
                theBestPriority = thePriority;
            if (theSeen.mUnit == ioGroup.mAims[k])
                theCurrent = (long)c;
        }
        if (theBestPriority == 100) {
            if (ioGroup.mAims[k] != 0)
                theTaken[ioGroup.mAims[k]]--;
            ioGroup.mAims[k] = 0;
            continue;
        }
        if (theCurrent >= 0 && EECPNextPriority(ioGroup.mTask, inState->mSeen[inCandidates[theCurrent]].mKind) == theBestPriority) {
            theFighting++;
            continue;   // it keeps its target
        }
        long theChoice = -1;
        long long theBestScore = 0;
        for (unsigned long c = 0; c < inCandidates.size(); c++) {
            const EECPNextSeen& theSeen = inState->mSeen[inCandidates[c]];
            if ((theSeen.mKind == kSeenBoat || theSeen.mKind == kSeenShip) && !theRanged)
                continue;
            if (theSeen.mContinent != 0 && theContinent != 0 && theSeen.mContinent != theContinent)
                continue;
            if (EECPNextPriority(ioGroup.mTask, theSeen.mKind) != theBestPriority)
                continue;
            std::map<long, long>::iterator theCount = theTaken.find(theSeen.mUnit);
            long long theScore = EECPNextD2(theUnit->GetTileX(), theUnit->GetTileY(), theSeen.mX, theSeen.mY)
                + (theCount != theTaken.end() ? 16LL * theCount->second : 0)
                + (theSeen.mKind == kSeenBuilding || theSeen.mKind == kSeenTower ? (long long)theSeen.mHitPoints : 0);   // the weakest building first
            if (theChoice < 0 || theScore < theBestScore) {
                theChoice = (long)c;
                theBestScore = theScore;
            }
        }
        if (theChoice < 0)
            continue;
        const EECPNextSeen& theTarget = inState->mSeen[inCandidates[theChoice]];
        EEUnit* theEnemy = gWorld.GetUnit((unsigned long)theTarget.mUnit);
        if (theEnemy == NULL)
            continue;
        if (ioGroup.mAims[k] != 0)
            theTaken[ioGroup.mAims[k]]--;
        ioGroup.mAims[k] = theTarget.mUnit;
        theTaken[theTarget.mUnit]++;
        EECPNextGiveAttack(theUnit, theEnemy);
        theFighting++;
    }
    return theFighting;
}

// a move of every unit not fighting to the next leg towards (inX, inY): given again when the leg moved or is stale
static void EECPNextMove(EEComputerPlayer* inPlayer, EECPNextGroup& ioGroup, long inCX, long inCY, long inX, long inY,
    bool inForced, long inNow)
{
    long theX = inX, theY = inY;
    long long theD2 = EECPNextD2(inCX, inCY, inX, inY);
    if (theD2 > (long long)kNextLeg * kNextLeg) {
        long theDistance = EECPNextRoot(theD2);
        theX = inCX + (inX - inCX) * kNextLeg / theDistance;
        theY = inCY + (inY - inCY) * kNextLeg / theDistance;
    }
    if (ioGroup.mWayTime != 0 && inNow - ioGroup.mWayTime < kNextMoveMs && EECPNextNear(theX, theY, ioGroup.mWayX, ioGroup.mWayY, 4))
        return;
    ioGroup.mWayX = theX;
    ioGroup.mWayY = theY;
    ioGroup.mWayTime = inNow;
    for (unsigned long k = 0; k < ioGroup.mUnits.size(); k++) {
        if (ioGroup.mAims[k] != 0)
            continue;
        EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, ioGroup.mUnits[k]);
        if (theUnit != NULL)
            EECPNextGiveMove(theUnit, theX, theY, inForced);
    }
}

// the scout's next place: an unexplored grid point on its land, nearer the scout and further from the town first
static bool EECPNextScoutPlace(EEComputerPlayer* inPlayer, const EECPNextGroup& inGroup, long inX, long inY,
    unsigned short inContinent, long& outX, long& outY)
{
    if (gWorld.mMap == NULL)
        return false;
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    bool theTown = theBuild->mTownCenterTile.mX != -10000;
    long theTownX = theTown ? theBuild->mTownCenterTile.mX : inX, theTownY = theTown ? theBuild->mTownCenterTile.mY : inY;
    unsigned long theID = inPlayer->mPlayerID;
    bool theFound = false;
    long theBest = 0;
    for (long y = kNextScoutGrid / 2; y < (long)gMapHeight; y += kNextScoutGrid)
        for (long x = kNextScoutGrid / 2; x < (long)gMapWidth; x += kNextScoutGrid) {
            EETile* theTile = gWorld.mMap->GetTile((unsigned long)x, (unsigned long)y);
            if (theTile == NULL || theTile->GetVisibility(theID) > 0)
                continue;
            bool theSkip = false;
            for (unsigned long s = 0; s < inGroup.mSkips.size() && !theSkip; s++)
                theSkip = EECPNextNear(x, y, inGroup.mSkips[s] >> 16, inGroup.mSkips[s] & 0xffff, kNextScoutGrid);
            if (theSkip || gContinentManager.GetContinent(U2DPoint<long>(x, y), kAIPathLandMass) != inContinent)
                continue;
            long theScore = EECPNextRoot(EECPNextD2(x, y, inX, inY)) - EECPNextRoot(EECPNextD2(x, y, theTownX, theTownY)) / 2;
            if (!theFound || theScore < theBest) {
                theFound = true;
                theBest = theScore;
                outX = x;
                outY = y;
            }
        }
    return theFound;
}

static void EECPNextScoutStep(EEComputerPlayer* inPlayer, EECPNextGroup& ioGroup, long inCX, long inCY, long inNow)
{
    if (ioGroup.mState == kNextGroupDone)
        return;
    EEComplexUnit* theLead = NULL;
    for (unsigned long k = 0; k < ioGroup.mUnits.size() && theLead == NULL; k++) {
        theLead = EECPNextGroupUnit(inPlayer, ioGroup.mUnits[k]);
        if (theLead != NULL && !EECPNextCanOrder(theLead))
            theLead = NULL;
    }
    if (theLead == NULL)
        return;
    bool theNeed = ioGroup.mX < 0;
    if (!theNeed) {
        EETile* theTile = gWorld.mMap != NULL ? gWorld.mMap->GetTile((unsigned long)ioGroup.mX, (unsigned long)ioGroup.mY) : NULL;
        theNeed = theTile == NULL || theTile->GetVisibility(inPlayer->mPlayerID) > 0
            || EECPNextNear(inCX, inCY, ioGroup.mX, ioGroup.mY, 3);
        if (!theNeed && inNow - ioGroup.mStateTime > kNextScoutStuckMs) {
            if (ioGroup.mSkips.size() < 64)
                ioGroup.mSkips.push_back(ioGroup.mX << 16 | ioGroup.mY);
            theNeed = true;
        }
    }
    if (!theNeed)
        return;
    long theX, theY;
    if (!EECPNextScoutPlace(inPlayer, ioGroup, inCX, inCY, gContinentManager.GetContinent(theLead, kAIPathLandMass), theX, theY)) {
        EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
        ioGroup.mX = theBuild->mTownCenterTile.mX != -10000 ? theBuild->mTownCenterTile.mX : inCX;
        ioGroup.mY = theBuild->mTownCenterTile.mX != -10000 ? theBuild->mTownCenterTile.mY : inCY;
        for (unsigned long k = 0; k < ioGroup.mUnits.size(); k++) {
            EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, ioGroup.mUnits[k]);
            if (theUnit != NULL)
                EECPNextGiveMove(theUnit, ioGroup.mX, ioGroup.mY, true);
        }
        EECPNextSetState(inPlayer, ioGroup, kNextGroupDone, inNow);
        return;
    }
    ioGroup.mX = theX;
    ioGroup.mY = theY;
    ioGroup.mStateTime = inNow;
    ioGroup.mState = kNextGroupMoving;
    for (unsigned long k = 0; k < ioGroup.mUnits.size(); k++) {
        EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, ioGroup.mUnits[k]);
        if (theUnit != NULL)
            EECPNextGiveMove(theUnit, theX, theY, true);
    }
}

static void EECPNextGroupStep(EEComputerPlayer* inPlayer, EECPNextState* inState, EECPNextGroup& ioGroup, long inNow)
{
    long theCX, theCY;
    if (!EECPNextCentre(inPlayer, ioGroup, theCX, theCY))
        return;   // every unit is carried or garrisoned
    if (ioGroup.mTask == kNextTaskScout) {
        EECPNextScoutStep(inPlayer, ioGroup, theCX, theCY, inNow);
        return;
    }
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    bool theTown = theBuild->mTownCenterTile.mX != -10000;
    if (ioGroup.mTask == kNextTaskRetreat) {
        if (EECPNextNear(theCX, theCY, ioGroup.mX, ioGroup.mY, kNextArrive + 2)) {
            // home: it defends the town (or the place it retreated to)
            ioGroup.mTask = kNextTaskDefend;
            ioGroup.mRadius = theTown && ioGroup.mX == theBuild->mTownCenterTile.mX && ioGroup.mY == theBuild->mTownCenterTile.mY
                ? EECPClaimRange(inPlayer) : 12;
            ioGroup.mWayTime = 0;
            EECPNextSetState(inPlayer, ioGroup, kNextGroupArrived, inNow);
            return;
        }
        EECPNextMove(inPlayer, ioGroup, theCX, theCY, ioGroup.mX, ioGroup.mY, true, inNow);
        return;
    }
    // a raid that meets more than it can fight goes home
    if (ioGroup.mTask == kNextTaskRaid) {
        long theEnemy = 0;
        for (unsigned long k = 0; k < inState->mSeen.size(); k++)
            if ((inState->mSeen[k].mKind == kSeenSoldier || inState->mSeen[k].mKind == kSeenTower)
                && EECPNextNear(inState->mSeen[k].mX, inState->mSeen[k].mY, theCX, theCY, 10))
                theEnemy += inState->mSeen[k].mStrength;
        long theOwn = EECPNextGroupStrength(inPlayer, ioGroup);
        if ((long long)theEnemy * 2 > (long long)theOwn * 3 && theTown) {
            ioGroup.mTask = kNextTaskRetreat;
            ioGroup.mX = theBuild->mTownCenterTile.mX;
            ioGroup.mY = theBuild->mTownCenterTile.mY;
            ioGroup.mWayTime = 0;
            for (unsigned long k = 0; k < ioGroup.mAims.size(); k++)
                ioGroup.mAims[k] = 0;
            EECPNextSetState(inPlayer, ioGroup, kNextGroupMoving, inNow);
            EE_CP_TRACE(inPlayer, "next.group", "flee id=%ld enemy=%ld own=%ld", ioGroup.mID, theEnemy, theOwn);
            EECPNextMove(inPlayer, ioGroup, theCX, theCY, ioGroup.mX, ioGroup.mY, true, inNow);
            return;
        }
    }
    std::vector<long> theCandidates;
    EECPNextCandidates(inPlayer, inState, ioGroup, theCX, theCY, theCandidates);
    long theFighting = theCandidates.empty() ? 0 : EECPNextFight(inPlayer, inState, ioGroup, theCandidates);
    if (theFighting > 0) {
        EECPNextSetState(inPlayer, ioGroup, kNextGroupFighting, inNow);
        // the others close in on the fight
        bool theOthers = false;
        for (unsigned long k = 0; k < ioGroup.mAims.size(); k++)
            theOthers = theOthers || ioGroup.mAims[k] == 0;
        if (theOthers) {
            const EECPNextSeen& theFirst = inState->mSeen[theCandidates[0]];
            EECPNextMove(inPlayer, ioGroup, theCX, theCY, theFirst.mX, theFirst.mY, false, inNow);
        }
        return;
    }
    for (unsigned long k = 0; k < ioGroup.mAims.size(); k++)
        ioGroup.mAims[k] = 0;
    if (ioGroup.mState == kNextGroupFighting)
        ioGroup.mWayTime = 0;   // the fight is over: on to its place
    if (ioGroup.mState == kNextGroupForming) {
        long theNear = 0, theCount = 0;
        for (unsigned long k = 0; k < ioGroup.mUnits.size(); k++) {
            EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, ioGroup.mUnits[k]);
            if (theUnit == NULL || theUnit->mContainer != NULL)
                continue;
            theCount++;
            if (EECPNextNear(theUnit->GetTileX(), theUnit->GetTileY(), ioGroup.mWayX, ioGroup.mWayY, 6))
                theNear++;
        }
        if (inNow - ioGroup.mStateTime < kNextFormMs && theNear * 5 < theCount * 4)
            return;
        ioGroup.mWayTime = 0;
        EECPNextSetState(inPlayer, ioGroup, kNextGroupMoving, inNow);
    }
    if (ioGroup.mTask == kNextTaskDefend) {
        if (EECPNextNear(theCX, theCY, ioGroup.mX, ioGroup.mY, kNextArrive)) {
            EECPNextSetState(inPlayer, ioGroup, kNextGroupArrived, inNow);
            // strays come back
            for (unsigned long k = 0; k < ioGroup.mUnits.size(); k++) {
                EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, ioGroup.mUnits[k]);
                if (theUnit != NULL && !EECPNextNear(theUnit->GetTileX(), theUnit->GetTileY(), ioGroup.mX, ioGroup.mY, 10)
                    && (theUnit->mWaypoints.size() == 0))
                    EECPNextGiveMove(theUnit, ioGroup.mX, ioGroup.mY, false);
            }
            return;
        }
        if (ioGroup.mState == kNextGroupArrived)
            EECPNextSetState(inPlayer, ioGroup, kNextGroupMoving, inNow);
        EECPNextMove(inPlayer, ioGroup, theCX, theCY, ioGroup.mX, ioGroup.mY, false, inNow);
        return;
    }
    // attack and raid
    if (EECPNextNear(theCX, theCY, ioGroup.mX, ioGroup.mY, kNextArrive)) {
        long theX = -1, theY = -1;
        if (ioGroup.mTask == kNextTaskRaid)
            EECPNextBestSpot(inState, ioGroup.mTarget, ioGroup.mX, ioGroup.mY, theX, theY);
        else {
            // the target's nearest unit seen within 30 tiles; else its town as last seen
            long long theBest = 0;
            for (unsigned long k = 0; k < inState->mSeen.size(); k++) {
                const EECPNextSeen& theSeen = inState->mSeen[k];
                if (theSeen.mPlayer != ioGroup.mTarget || theSeen.mKind == kSeenBoat || theSeen.mKind == kSeenShip)
                    continue;
                long long theD2 = EECPNextD2(theSeen.mX, theSeen.mY, theCX, theCY);
                if (theD2 <= 30LL * 30 && (theX < 0 || theD2 < theBest)) {
                    theBest = theD2;
                    theX = theSeen.mX;
                    theY = theSeen.mY;
                }
            }
            long p = ioGroup.mTarget;
            if (theX < 0 && p > 0 && p < 64 && inState->mSeenTownX[p] >= 0
                && !EECPNextNear(inState->mSeenTownX[p], inState->mSeenTownY[p], ioGroup.mX, ioGroup.mY, kNextArrive)) {
                theX = inState->mSeenTownX[p];
                theY = inState->mSeenTownY[p];
            }
        }
        if (theX < 0) {
            EECPNextSetState(inPlayer, ioGroup, kNextGroupArrived, inNow);
            return;
        }
        ioGroup.mX = theX;
        ioGroup.mY = theY;
        ioGroup.mWayTime = 0;
        EECPNextTraceGroup(inPlayer, "next", ioGroup);
    }
    EECPNextSetState(inPlayer, ioGroup, kNextGroupMoving, inNow);
    EECPNextMove(inPlayer, ioGroup, theCX, theCY, ioGroup.mX, ioGroup.mY, false, inNow);
}

void EECPNextArmyStep(EEComputerPlayer* inPlayer)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    long theNow = (long)gPhysics->mGameTime;
    EECPNextScan(inPlayer, theState, theNow);
    for (unsigned long g = 0; g < theState->mGroups.size();) {
        EECPNextGroup& theGroup = theState->mGroups[g];
        for (long k = (long)theGroup.mUnits.size() - 1; k >= 0; k--) {
            if (EECPNextGroupUnit(inPlayer, theGroup.mUnits[k]) != NULL)
                continue;
            theGroup.mUnits.erase(theGroup.mUnits.begin() + k);
            theGroup.mAims.erase(theGroup.mAims.begin() + k);
            theGroup.mLost++;
        }
        if (theGroup.mUnits.empty()) {
            EECPNextTraceGroup(inPlayer, "gone", theGroup);
            theState->mGroups.erase(theState->mGroups.begin() + g);
            continue;
        }
        EECPNextGroupStep(inPlayer, theState, theGroup, theNow);
        g++;
    }
    if (gCPTraceOn && theNow % 30000 < 1000)
        for (unsigned long g = 0; g < theState->mGroups.size(); g++) {
            const EECPNextGroup& theGroup = theState->mGroups[g];
            long theX = -1, theY = -1;
            EECPNextCentre(inPlayer, theGroup, theX, theY);
            EE_CP_TRACE(inPlayer, "next.group", "id=%ld task=%s state=%s units=%lu str=%ld x=%ld y=%ld tx=%ld ty=%ld lost=%ld",
                theGroup.mID, EECPNextTaskName(theGroup.mTask), EECPNextGroupStateName(theGroup.mState),
                (unsigned long)theGroup.mUnits.size(), EECPNextGroupStrength(inPlayer, theGroup), theX, theY, theGroup.mX,
                theGroup.mY, theGroup.mLost);
        }
}

// ---------------------------------------------------------------------------------------------------------------
// The verbs

static EECPNextResult EECPNextArmyReport(EEComputerPlayer* inPlayer, const char* inVerb, long inCode, long inReason,
    long inCount, long inGroup, const char* inFormat, ...)
{
    EECPNextResult theResult = { inCode, inReason, inCount, inGroup };
    if (gCPTraceOn) {
        char theArgs[192];
        va_list theList;
        va_start(theList, inFormat);
        vsnprintf(theArgs, sizeof(theArgs), inFormat, theList);
        va_end(theList);
        EECPTraceScope theScope("next");
        EECPTraceOut(inPlayer, "next.order", "verb=%s group=%ld %s result=%s reason=%s n=%ld", inVerb, inGroup, theArgs,
            EECPNextCodeName(inCode), EECPNextReasonName(inReason), inCount);
    }
    return theResult;
}

struct EECPNextCandidate {
    long mKey1, mKey2, mID;
    bool operator<(const EECPNextCandidate& inOther) const
    {
        if (this->mKey1 != inOther.mKey1)
            return this->mKey1 < inOther.mKey1;
        if (this->mKey2 != inOther.mKey2)
            return this->mKey2 < inOther.mKey2;
        return this->mID < inOther.mID;
    }
};

// up to inCount free soldiers of the classes (any when empty), or of group inFrom's when it is set: nearest to (inX,
// inY) first (inFast: the fastest first)
static void EECPNextFreeSoldiers(EEComputerPlayer* inPlayer, EECPNextState* inState, long inCount,
    const std::vector<long>& inClasses, long inX, long inY, bool inFast, const EECPNextGroup* inFrom, std::vector<long>& outUnits)
{
    outUnits.clear();
    std::set<long> theHeld = EECPNextHeld(inState);
    std::vector<EECPNextCandidate> theFree;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (!EECPNextArmySoldier(theUnit) || theUnit->mComplexUnit->mContainer != NULL || theUnit->mComplexUnit->mIsDisabled
            || theUnit->GetTileX() == -10000)
            continue;
        bool theTaken = theHeld.count((long)theUnit->mUnitID) != 0;
        if (inFrom == NULL ? theTaken
                : std::find(inFrom->mUnits.begin(), inFrom->mUnits.end(), (long)theUnit->mUnitID) == inFrom->mUnits.end())
            continue;
        if (!inClasses.empty() && std::find(inClasses.begin(), inClasses.end(), (long)theUnit->mUnitClass->mUnitType) == inClasses.end())
            continue;
        EECPNextCandidate theEntry;
        long theDistance = inX >= 0 ? EECPNextRoot(EECPNextD2(theUnit->GetTileX(), theUnit->GetTileY(), inX, inY)) : 0;
        long theSpeed = (long)UMFloatToLong(theUnit->mStats->mMaxVelocity * 100.0f);
        theEntry.mKey1 = inFast ? -theSpeed : theDistance;
        theEntry.mKey2 = inFast ? theDistance : 0;
        theEntry.mID = (long)theUnit->mUnitID;
        theFree.push_back(theEntry);
    }
    std::sort(theFree.begin(), theFree.end());
    for (unsigned long k = 0; k < theFree.size() && (long)outUnits.size() < inCount; k++)
        outUnits.push_back(theFree[k].mID);
}

EECPNextResult EECPNextGroupTask(EEComputerPlayer* inPlayer, long inTask, const EECPNextGroupOrder& inOrder)
{
    const char* theVerb = EECPNextTaskName(inTask);
    if ((inTask != kNextTaskAttack && inTask != kNextTaskRaid && inTask != kNextTaskDefend && inTask != kNextTaskScout)
        || inOrder.mCount < 0 || inOrder.mCount > kNextArmyMaxCount || inOrder.mRadius < 0 || inOrder.mRadius > 60
        || inPlayer->mCombatManager == NULL || gWorld.mMap == NULL
        || ((inOrder.mX >= 0 || inOrder.mY >= 0)
            && (inOrder.mX < 0 || inOrder.mY < 0 || gWorld.mMap->GetTile((unsigned long)inOrder.mX, (unsigned long)inOrder.mY) == NULL)))
        return EECPNextArmyReport(inPlayer, theVerb, kNextRefused, kNextReasonBadArgument, 0, inOrder.mGroup,
            "want=%ld x=%ld y=%ld", inOrder.mCount, inOrder.mX, inOrder.mY);
    EECPNextState* theState = EECPNextGetState(inPlayer);
    EECPNextGroup* theGroup = NULL;
    if (inOrder.mGroup != 0) {
        theGroup = EECPNextFindGroup(theState, inOrder.mGroup);
        if (theGroup == NULL)
            return EECPNextArmyReport(inPlayer, theVerb, kNextRefused, kNextReasonBadArgument, 0, inOrder.mGroup,
                "want=%ld (no such group)", inOrder.mCount);
    } else if (inOrder.mCount == 0)
        return EECPNextArmyReport(inPlayer, theVerb, kNextRefused, kNextReasonBadArgument, 0, 0, "want=0");
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    bool theTown = theBuild->mTownCenterTile.mX != -10000;
    // the target and the place
    long theTarget = 0, theX = inOrder.mX, theY = inOrder.mY, theRadius = inOrder.mRadius;
    if (inTask == kNextTaskAttack || inTask == kNextTaskRaid) {
        if (inOrder.mPlayer != 0) {
            if (!EECPNextEnemyPlayer(inPlayer, inOrder.mPlayer))
                return EECPNextArmyReport(inPlayer, theVerb, kNextRefused, kNextReasonBadArgument, 0, inOrder.mGroup,
                    "target=%ld want=%ld", inOrder.mPlayer, inOrder.mCount);
            theTarget = inOrder.mPlayer;
        } else {
            EEPlayer* theEnemy = EECPNextArmyFirstEnemy(inPlayer);
            theTarget = theEnemy != NULL ? (long)theEnemy->mPlayerID : 0;
        }
        if (theTarget <= 0 || theTarget >= 64)
            return EECPNextArmyReport(inPlayer, theVerb, kNextRefused, kNextReasonNoTarget, 0, inOrder.mGroup, "want=%ld (no enemy)",
                inOrder.mCount);
        if (theX < 0 && inTask == kNextTaskRaid)
            EECPNextBestSpot(theState, theTarget, -1, -1, theX, theY);
        if (theX < 0 && theState->mSeenTownX[theTarget] >= 0) {
            theX = theState->mSeenTownX[theTarget];
            theY = theState->mSeenTownY[theTarget];
        }
        if (theX < 0)
            return EECPNextArmyReport(inPlayer, theVerb, kNextRefused, kNextReasonNoTarget, 0, inOrder.mGroup,
                "target=%ld want=%ld (no place seen)", theTarget, inOrder.mCount);
    } else if (inTask == kNextTaskDefend) {
        if (theX < 0) {
            if (!theTown)
                return EECPNextArmyReport(inPlayer, theVerb, kNextRefused, kNextReasonBadArgument, 0, inOrder.mGroup, "want=%ld (no town)",
                    inOrder.mCount);
            theX = theBuild->mTownCenterTile.mX;
            theY = theBuild->mTownCenterTile.mY;
            if (theRadius == 0)
                theRadius = EECPClaimRange(inPlayer);
        }
        if (theRadius == 0)
            theRadius = 12;
    } else {
        theX = theY = -1;   // a scout finds its places
    }
    // the soldiers
    std::vector<long> theUnits;
    long theFromX = theX >= 0 ? theX : (theTown ? (long)theBuild->mTownCenterTile.mX : -1);
    long theFromY = theX >= 0 ? theY : (theTown ? (long)theBuild->mTownCenterTile.mY : -1);
    if (inTask == kNextTaskAttack || inTask == kNextTaskRaid || inTask == kNextTaskScout) {
        theFromX = theTown ? (long)theBuild->mTownCenterTile.mX : theFromX;   // the soldiers at home go
        theFromY = theTown ? (long)theBuild->mTownCenterTile.mY : theFromY;
    }
    EECPNextGroup* theSource = NULL;
    if (inOrder.mFrom != 0) {
        theSource = EECPNextFindGroup(theState, inOrder.mFrom);
        if (theSource == NULL || theSource == theGroup)
            return EECPNextArmyReport(inPlayer, theVerb, kNextRefused, kNextReasonBadArgument, 0, inOrder.mGroup,
                "want=%ld from=%ld (no such group)", inOrder.mCount, inOrder.mFrom);
    }
    EECPNextFreeSoldiers(inPlayer, theState, inOrder.mCount, inOrder.mClasses, theFromX, theFromY,
        inTask == kNextTaskRaid || inTask == kNextTaskScout, theSource, theUnits);
    if (theGroup == NULL && theUnits.empty())
        return EECPNextArmyReport(inPlayer, theVerb, kNextRefused, kNextReasonNoUnits, 0, 0, "want=%ld from=%ld", inOrder.mCount,
            inOrder.mFrom);
    // out of the group they come from (an emptied group is dropped at the next step)
    for (unsigned long k = 0; theSource != NULL && k < theUnits.size(); k++)
        for (unsigned long u = 0; u < theSource->mUnits.size(); u++)
            if (theSource->mUnits[u] == theUnits[k]) {
                theSource->mUnits.erase(theSource->mUnits.begin() + u);
                theSource->mAims.erase(theSource->mAims.begin() + u);
                break;
            }
    if (theGroup == NULL && theUnits.empty())
        return EECPNextArmyReport(inPlayer, theVerb, kNextRefused, kNextReasonNoUnits, 0, 0, "want=%ld", inOrder.mCount);
    long theNow = (long)gPhysics->mGameTime;
    bool theNew = theGroup == NULL;
    long theOldX = -1, theOldY = -1;
    if (theNew) {
        EECPNextGroup theEntry;
        theEntry.mID = theState->mNextGroup++;
        theEntry.mState = kNextGroupForming;
        theEntry.mCreated = theNow;
        theEntry.mStarted = 0;
        theEntry.mLost = 0;
        theState->mGroups.push_back(theEntry);
        theGroup = &theState->mGroups.back();
    } else
        EECPNextCentre(inPlayer, *theGroup, theOldX, theOldY);
    // the same task at the same place only adds soldiers: the group goes on as it was
    bool theSame = !theNew && theGroup->mTask == inTask && theGroup->mTarget == theTarget
        && (inTask == kNextTaskScout || (theGroup->mX == theX && theGroup->mY == theY && theGroup->mRadius == theRadius)
            || (inOrder.mX < 0 && inTask != kNextTaskDefend));
    if (!theSame) {
        theGroup->mTask = inTask;
        theGroup->mTarget = theTarget;
        theGroup->mX = theX;
        theGroup->mY = theY;
        theGroup->mRadius = theRadius;
        theGroup->mWayTime = 0;
        theGroup->mWayX = theGroup->mWayY = -1;
        theGroup->mStateTime = theNow;
        if (!theNew || inTask == kNextTaskScout)
            theGroup->mState = kNextGroupMoving;
        for (unsigned long k = 0; k < theGroup->mAims.size(); k++)
            theGroup->mAims[k] = 0;
    }
    for (unsigned long k = 0; k < theUnits.size(); k++) {
        EEUnit* theUnit = gWorld.GetUnit((unsigned long)theUnits[k]);
        inPlayer->DetachUnitFromAI(theUnit->mComplexUnit);
        theGroup->mUnits.push_back(theUnits[k]);
        theGroup->mAims.push_back(0);
    }
    if ((long)theGroup->mUnits.size() > theGroup->mStarted)
        theGroup->mStarted = (long)theGroup->mUnits.size();
    // the first moves: a new group forms at the soldier nearest its centre; soldiers added go to the group
    long theCX = -1, theCY = -1;
    if (theNew && inTask != kNextTaskScout && EECPNextCentre(inPlayer, *theGroup, theCX, theCY)) {
        long long theBest = -1;
        long theMX = theCX, theMY = theCY;
        for (unsigned long k = 0; k < theGroup->mUnits.size(); k++) {
            EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, theGroup->mUnits[k]);
            if (theUnit == NULL || theUnit->mContainer != NULL)
                continue;
            long long theD2 = EECPNextD2(theUnit->GetTileX(), theUnit->GetTileY(), theCX, theCY);
            if (theBest < 0 || theD2 < theBest) {
                theBest = theD2;
                theMX = theUnit->GetTileX();
                theMY = theUnit->GetTileY();
            }
        }
        theGroup->mWayX = theMX;
        theGroup->mWayY = theMY;
        theGroup->mWayTime = theNow;
        for (unsigned long k = 0; k < theGroup->mUnits.size(); k++) {
            EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, theGroup->mUnits[k]);
            if (theUnit != NULL)
                EECPNextGiveMove(theUnit, theMX, theMY, false);
        }
    } else if (!theNew && theOldX >= 0) {
        for (unsigned long k = 0; k < theUnits.size(); k++) {
            EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, theUnits[k]);
            if (theUnit != NULL)
                EECPNextGiveMove(theUnit, theOldX, theOldY, false);
        }
    }
    EECPNextResult theResult = EECPNextArmyReport(inPlayer, theVerb, kNextDone, kNextReasonNone, (long)theGroup->mUnits.size(),
        theGroup->mID, "target=%ld x=%ld y=%ld radius=%ld want=%ld took=%lu", theTarget, theX, theY, theRadius, inOrder.mCount,
        (unsigned long)theUnits.size());
    return theResult;
}

EECPNextResult EECPNextRetreat(EEComputerPlayer* inPlayer, long inGroup, long inX, long inY)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    if (inX < 0 || inY < 0) {
        inX = theBuild->mTownCenterTile.mX != -10000 ? (long)theBuild->mTownCenterTile.mX : -1;
        inY = theBuild->mTownCenterTile.mX != -10000 ? (long)theBuild->mTownCenterTile.mY : -1;
    }
    if (inX < 0 || gWorld.mMap == NULL || gWorld.mMap->GetTile((unsigned long)inX, (unsigned long)inY) == NULL
        || (inGroup != 0 && EECPNextFindGroup(theState, inGroup) == NULL))
        return EECPNextArmyReport(inPlayer, "retreat", kNextRefused, kNextReasonBadArgument, 0, inGroup, "x=%ld y=%ld", inX, inY);
    long theNow = (long)gPhysics->mGameTime, theUnits = 0;
    for (unsigned long g = 0; g < theState->mGroups.size(); g++) {
        EECPNextGroup& theGroup = theState->mGroups[g];
        if ((inGroup != 0 && theGroup.mID != inGroup) || (inGroup == 0 && theGroup.mTask == kNextTaskScout))
            continue;
        theGroup.mTask = kNextTaskRetreat;
        theGroup.mX = inX;
        theGroup.mY = inY;
        theGroup.mWayTime = 0;
        for (unsigned long k = 0; k < theGroup.mAims.size(); k++)
            theGroup.mAims[k] = 0;
        EECPNextSetState(inPlayer, theGroup, kNextGroupMoving, theNow);
        long theCX, theCY;
        if (EECPNextCentre(inPlayer, theGroup, theCX, theCY))
            EECPNextMove(inPlayer, theGroup, theCX, theCY, inX, inY, true, theNow);
        theUnits += (long)theGroup.mUnits.size();
    }
    return EECPNextArmyReport(inPlayer, "retreat", kNextDone, kNextReasonNone, theUnits, inGroup, "x=%ld y=%ld", inX, inY);
}

EECPNextResult EECPNextRegroup(EEComputerPlayer* inPlayer, long inGroup, long inX, long inY)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    EECPNextGroup* theGroup = EECPNextFindGroup(theState, inGroup);
    long theCX = -1, theCY = -1;
    if (theGroup != NULL && (inX < 0 || inY < 0) && EECPNextCentre(inPlayer, *theGroup, theCX, theCY)) {
        inX = theCX;
        inY = theCY;
    }
    if (theGroup == NULL || inX < 0 || inY < 0 || gWorld.mMap == NULL || gWorld.mMap->GetTile((unsigned long)inX, (unsigned long)inY) == NULL)
        return EECPNextArmyReport(inPlayer, "regroup", kNextRefused, kNextReasonBadArgument, 0, inGroup, "x=%ld y=%ld", inX, inY);
    long theNow = (long)gPhysics->mGameTime;
    theGroup->mTask = kNextTaskDefend;
    theGroup->mX = inX;
    theGroup->mY = inY;
    theGroup->mRadius = 8;
    theGroup->mWayTime = 0;
    for (unsigned long k = 0; k < theGroup->mAims.size(); k++)
        theGroup->mAims[k] = 0;
    EECPNextSetState(inPlayer, *theGroup, kNextGroupMoving, theNow);
    return EECPNextArmyReport(inPlayer, "regroup", kNextDone, kNextReasonNone, (long)theGroup->mUnits.size(), inGroup, "x=%ld y=%ld",
        inX, inY);
}

EECPNextResult EECPNextDisband(EEComputerPlayer* inPlayer, long inGroup)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    for (unsigned long g = 0; g < theState->mGroups.size(); g++) {
        if (theState->mGroups[g].mID != inGroup)
            continue;
        EECPNextGroup theGroup = theState->mGroups[g];
        theState->mGroups.erase(theState->mGroups.begin() + g);   // first, so that UnitAdded's draft may take them
        long theCount = 0;
        for (unsigned long k = 0; k < theGroup.mUnits.size(); k++) {
            EEComplexUnit* theUnit = EECPNextGroupUnit(inPlayer, theGroup.mUnits[k]);
            if (theUnit == NULL)
                continue;
            EECPNextReleaseUnit(inPlayer, theUnit);
            theCount++;
        }
        return EECPNextArmyReport(inPlayer, "disband", kNextDone, kNextReasonNone, theCount, inGroup, "lost=%ld", theGroup.mLost);
    }
    return EECPNextArmyReport(inPlayer, "disband", kNextRefused, kNextReasonBadArgument, 0, inGroup, "(no such group)");
}

EECPNextResult EECPNextDefend(EEComputerPlayer* inPlayer, long inX, long inY, long inCount)
{
    EECPNextGroupOrder theOrder;
    theOrder.mGroup = 0;
    theOrder.mCount = inCount;
    theOrder.mFrom = 0;
    theOrder.mPlayer = 0;
    theOrder.mX = inX;
    theOrder.mY = inY;
    theOrder.mRadius = 0;
    return EECPNextGroupTask(inPlayer, kNextTaskDefend, theOrder);
}

EECPNextResult EECPNextAttack(EEComputerPlayer* inPlayer, long inTarget, long inCount)
{
    EECPNextGroupOrder theOrder;
    theOrder.mGroup = 0;
    theOrder.mCount = inCount;
    theOrder.mFrom = 0;
    theOrder.mPlayer = inTarget;
    theOrder.mX = theOrder.mY = -1;
    theOrder.mRadius = 0;
    return EECPNextGroupTask(inPlayer, kNextTaskAttack, theOrder);
}

// ---------------------------------------------------------------------------------------------------------------
// The view

void EECPNextArmyView(EEComputerPlayer* inPlayer, EECPNextView& ioView)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    for (unsigned long g = 0; g < theState->mGroups.size(); g++) {
        const EECPNextGroup& theGroup = theState->mGroups[g];
        EECPNextGroupView theEntry;
        memset(&theEntry, 0, sizeof(theEntry));
        theEntry.id = theGroup.mID;
        theEntry.task = theGroup.mTask;
        theEntry.state = theGroup.mState;
        theEntry.units = (long)theGroup.mUnits.size();
        theEntry.strength = EECPNextGroupStrength(inPlayer, theGroup);
        theEntry.x = theEntry.y = -1;
        EECPNextCentre(inPlayer, theGroup, theEntry.x, theEntry.y);
        theEntry.target = theGroup.mTarget;
        theEntry.tx = theGroup.mX;
        theEntry.ty = theGroup.mY;
        theEntry.started = theGroup.mStarted;
        theEntry.lost = theGroup.mLost;
        theEntry.created = theGroup.mCreated;
        if (theEntry.x >= 0) {
            for (unsigned long k = 0; k < theState->mSeen.size(); k++) {
                const EECPNextSeen& theSeen = theState->mSeen[k];
                if (theSeen.mKind == kSeenSoldier && EECPNextNear(theSeen.mX, theSeen.mY, theEntry.x, theEntry.y, 12)) {
                    theEntry.enemy_count++;
                    theEntry.enemy_strength += theSeen.mStrength;
                }
            }
            std::vector<long> theCandidates;
            EECPNextCandidates(inPlayer, theState, theGroup, theEntry.x, theEntry.y, theCandidates);
            theEntry.targets = (long)theCandidates.size();
        }
        ioView.groups.push_back(theEntry);
    }
    std::set<long> theHeld = EECPNextHeld(theState);
    ioView.free_soldiers = 0;
    ioView.free_strength = 0;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (EECPNextArmySoldier(theUnit) && theHeld.count((long)theUnit->mUnitID) == 0) {
            ioView.free_soldiers++;
            ioView.free_strength += EECPNextUnitStrength(theUnit);
        }
    for (unsigned long e = 0; e < ioView.enemies.size(); e++) {
        EECPNextEnemy& theEnemy = ioView.enemies[e];
        long p = theEnemy.player;
        theEnemy.towers = 0;
        for (unsigned long t = 0; t < theState->mTowers.size(); t++)
            if (theState->mTowers[t].mPlayer == p)
                theEnemy.towers++;
        theEnemy.citizens_seen = p > 0 && p < 64 ? theState->mSeenCitizens[p] : 0;
        theEnemy.citizen_x = theEnemy.citizen_y = -1;
        EECPNextBestSpot(theState, p, -1, -1, theEnemy.citizen_x, theEnemy.citizen_y);
        theEnemy.army_x = p > 0 && p < 64 ? theState->mArmyX[p] : -1;
        theEnemy.army_y = p > 0 && p < 64 ? theState->mArmyY[p] : -1;
    }
}
