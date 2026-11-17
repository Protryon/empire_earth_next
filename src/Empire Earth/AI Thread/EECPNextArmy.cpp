// The next engine's scouting memory (ee/EECPNext.h): what the player sees of the enemies each step (the units, their
// gather spots and towers, where their armies were) for the view and its gather safety, and the losses trace. Stage 6
// (docs/AI_LUA_VERBS.md) deleted the group verbs (attack, raid, defend, retreat, regroup, disband, scout) and their
// step: no Lua AI calls them any more. New in this port: no original address.
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
#include "ee/EEUGRust.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPTrace.h"
#include "ee/EECPNextUnits.h"
#include "ee/EECPNext.h"

static const long kNextTowerAvoid = 9;         // a gather spot this near a known tower is not a raid's (the view's citizen_x)
static const long kNextSpotRange = 8;          // gather spots this near are one spot
static const long kNextSpotsPerPlayer = 8;
static const long kNextTowersPerPlayer = 32;

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

static void EECPNextTraceIncidents(EEComputerPlayer* inPlayer, const EECPNextState* inState, long inNow);

void EECPNextArmyStep(EEComputerPlayer* inPlayer)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    long theNow = (long)gPhysics->mGameTime;
    EECPNextScan(inPlayer, theState, theNow);
    EECPNextTraceIncidents(inPlayer, theState, theNow);
}

// ---------------------------------------------------------------------------------------------------------------
// The view

void EECPNextArmyView(EEComputerPlayer* inPlayer, EECPNextView& ioView)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    std::set<long> theHeld;
    { std::vector<long> theOwn; EECPUnitsHeld(inPlayer, theOwn); theHeld.insert(theOwn.begin(), theOwn.end()); }   // the script's units (ee/EECPNextUnits.h)
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

// ---------------------------------------------------------------------------------------------------------------
// The fishing boats' reflex (EECPResourceManager::FindFish; docs/AI_RESEARCH.md §8.33): the tiles of the armed enemy
// ships the player sees now, read from the world at the call (not the army step's mSeen, which a loaded game has not
// rebuilt yet), so the finder can leave the fish near them alone

void EECPNextSeenWarships(EEComputerPlayer* inPlayer, std::vector<long>& outTiles)
{
    outTiles.clear();
    for (unsigned long p = 1; p < gWorld.mNumPlayers && p < 64; p++) {
        EEPlayer* theOther = gWorld.mPlayers[p];
        if (theOther == NULL || theOther == inPlayer || inPlayer->IsAlly(theOther) || theOther->mObserverMode)
            continue;
        for (EEUnit* theUnit = theOther->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->mTile == NULL || theUnit->mStats == NULL
                || theUnit->mStats->mAttack == 0.0f || theUnit->mUnitClass->IsBuilding()
                || theUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen || !(theUnit->mUnitClass->mDomains & kMovementSea)
                || theUnit->mComplexUnit->mContainer != NULL || !static_cast<EETile*>(theUnit->mTile)->IsSeenBy(inPlayer))
                continue;
            outTiles.push_back(theUnit->GetTileX() << 16 | theUnit->GetTileY());
        }
    }
}

bool EECPNextNearTiles(const std::vector<long>& inTiles, long inX, long inY, long inRange)
{
    for (unsigned long k = 0; k < inTiles.size(); k++)
        if (EECPNextNear(inTiles[k] >> 16, inTiles[k] & 0xffff, inX, inY, inRange))
            return true;
    return false;
}

// ---------------------------------------------------------------------------------------------------------------
// The citizens' losses (the AI trace only, gCPTraceOn: nothing here is read by the game; docs/AI_RESEARCH.md §8.33).
// Every hit on a land citizen of a next-engine player opens or extends an incident (hits within 20 tiles and 30 s of
// each other); every citizen lost writes a next.death line (where, what killed it, what it was doing when first hit,
// the foes, the own soldiers, groups and towers near), and every incident closed a next.raid line (how long it
// lasted, the hits and losses, how far the nearest group was when it began and how long one took to come within 10
// tiles).

struct EECPNextLossHit {
    long mFirst, mLast;         // game ms: its first hit of the incident, its last
    long mWhat;                 // what it was doing at the first hit (EECPNextDoing)
    long mIncident;
};

struct EECPNextIncident {
    long mID, mX, mY, mDistance, mStart, mLast, mHits, mLost, mFoes, mFoeStrength, mAttacker;
    long mGuard0, mOwn0, mTower0, mGuardAt;
};

static std::map<long, EECPNextLossHit> sNextHits[64];
static std::vector<EECPNextIncident> sNextIncidents[64];
static long sNextIncidentID[64];
static long sNextLossTime[64];

void EECPNextArmyNewGame()
{
    for (long k = 0; k < 64; k++) {
        sNextHits[k].clear();
        sNextIncidents[k].clear();
        sNextIncidentID[k] = 0;
        sNextLossTime[k] = 0;
    }
}

enum {
    kDoingIdle = 0, kDoingForage, kDoingHunt, kDoingFish, kDoingFarm, kDoingFood, kDoingWood, kDoingStone, kDoingGold,
    kDoingIron, kDoingBuild, kDoingFight, kDoingMove, kDoingOther,
};

static const char* EECPNextDoingName(long inWhat)
{
    static const char* const sNames[] = {"idle", "forage", "hunt", "fish", "farm", "food", "wood", "stone", "gold",
                                         "iron", "build", "fight", "move", "other"};
    return inWhat >= 0 && inWhat <= kDoingOther ? sNames[inWhat] : "?";
}

static long EECPNextDoing(EEComplexUnit* inUnit)
{
    EEUnitGoal* theGoal = inUnit->mActiveGoal;
    if (theGoal == NULL)
        return kDoingIdle;
    switch (theGoal->mKind) {
    case kGoalGather: {
        EEUnit* theResource = static_cast<EEUGRust*>(theGoal)->GetGatherResource();
        if (theResource == NULL)
            return kDoingOther;
        switch (theResource->mResourceType) {
        case kResourceFood: {
            long theClass = theResource->mUnitClass->mUnitType;
            unsigned long theFamily = theResource->mUnitClass->GetFamily();
            return theClass == kUnitTypeResForageBush ? kDoingForage : theFamily == kUnitFamilyAnimal ? kDoingHunt
                : theFamily == kUnitFamilyFish || theClass == kUnitTypeResFish ? kDoingFish
                : theClass == kUnitTypeFarm || theClass == kUnitTypeRoboticFarm ? kDoingFarm : kDoingFood;
        }
        case kResourceWood: return kDoingWood;
        case kResourceStone: return kDoingStone;
        case kResourceGold: return kDoingGold;
        case kResourceIron: return kDoingIron;
        default: return kDoingOther;
        }
    }
    case kGoalAttackUnit: {
        EEUnit* theTarget = static_cast<EEUGRust*>(theGoal)->GetAttackTarget();
        return theTarget != NULL && theTarget->mUnitClass->GetFamily() == kUnitFamilyAnimal ? kDoingHunt : kDoingFight;
    }
    case kGoalBuildRepair: return kDoingBuild;
    case kGoalLocation: return kDoingMove;
    default: return kDoingOther;
    }
}

// the nearest of the player's groups to a tile: none (stage 6: the group verbs are gone; the Lua AIs keep their own)
static long EECPNextNearestGroup(EEComputerPlayer* inPlayer, const EECPNextState* inState, long inX, long inY,
    long& outTask, long& outUnits)
{
    (void)inPlayer;
    (void)inState;
    (void)inX;
    (void)inY;
    outTask = -1;
    outUnits = 0;
    return -1;
}

// the foes round a tile (enemy soldiers, towers and warships within inRange; armed animals within 4), the own soldiers
// within inRange, and the nearest own built tower and drop site
static void EECPNextAround(EEComputerPlayer* inPlayer, long inX, long inY, long inRange, long& outFoes, long& outStrength,
    long& outAnimals, long& outOwn, long& outTower, long& outDrop)
{
    outFoes = outStrength = outAnimals = outOwn = 0;
    outTower = outDrop = -1;
    for (unsigned long p = 0; p < gWorld.mNumPlayers && p < 64; p++) {
        EEPlayer* theOther = gWorld.mPlayers[p];
        if (theOther == NULL)
            continue;
        bool theOwn = theOther == inPlayer;
        if (!theOwn && p > 0 && inPlayer->IsAlly(theOther))
            continue;
        for (EEUnit* theUnit = theOther->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->mStats == NULL
                || theUnit->mComplexUnit->mContainer != NULL || theUnit->GetTileX() == -10000)
                continue;
            long long theD2 = EECPNextD2(theUnit->GetTileX(), theUnit->GetTileY(), inX, inY);
            EEUnitClass* theClass = theUnit->mUnitClass;
            if (theOwn) {
                if (theClass->IsBuilding() && theUnit->mComplexUnit->mIsBuilt) {
                    long theDistance = EECPNextRoot(theD2);
                    if (theClass->mIsTower && theUnit->mStats->mAttack != 0.0f && (outTower < 0 || theDistance < outTower))
                        outTower = theDistance;
                    if (theClass->IsDropSite() && (outDrop < 0 || theDistance < outDrop))
                        outDrop = theDistance;
                } else if (EECPNextArmySoldier(theUnit) && theD2 <= (long long)inRange * inRange)
                    outOwn++;
                continue;
            }
            if (p == 0) {
                if (theUnit->mStats->mAttack != 0.0f && theClass->GetFamily() == kUnitFamilyAnimal && theD2 <= 16)
                    outAnimals++;
                continue;
            }
            if (theD2 > (long long)inRange * inRange)
                continue;
            if (EECPNextArmySoldier(theUnit) || (theClass->IsBuilding() && theClass->mIsTower && theUnit->mStats->mAttack != 0.0f)
                || ((theClass->mDomains & kMovementSea) && theUnit->mStats->mAttack != 0.0f && !theClass->IsBuilding()
                    && theClass->mCategory != kUnitCategoryNavalCitizen)) {
                outFoes++;
                outStrength += EECPNextUnitStrength(theUnit);
            }
        }
    }
}

static long EECPNextTownDistance(EEComputerPlayer* inPlayer, long inX, long inY)
{
    EECPPlacement* theBuild = inPlayer->mPlacement;
    if (theBuild == NULL || theBuild->mTownCenterTile.mX == -10000)
        return -1;
    return EECPNextRoot(EECPNextD2(inX, inY, theBuild->mTownCenterTile.mX, theBuild->mTownCenterTile.mY));
}

static void EECPNextLossReset(unsigned long inID, long inNow)
{
    if (inNow < sNextLossTime[inID]) {   // a new game in the same process
        sNextHits[inID].clear();
        sNextIncidents[inID].clear();
        sNextIncidentID[inID] = 0;
    }
    sNextLossTime[inID] = inNow;
}

void EECPNextTraceHit(EEComputerPlayer* inPlayer, long inAttacker, EEUnit* inUnit)
{
    unsigned long theID = inPlayer->mPlayerID;
    if (!gCPTraceOn || inPlayer->mNextState == NULL || theID >= 64 || inUnit == NULL || inUnit->mComplexUnit == NULL
        || inUnit->mUnitClass->mCategory != kUnitCategoryLandCitizen || inUnit->GetTileX() == -10000)
        return;
    long theNow = (long)gPhysics->mGameTime;
    EECPNextLossReset(theID, theNow);
    long theX = inUnit->GetTileX(), theY = inUnit->GetTileY();
    std::vector<EECPNextIncident>& theIncidents = sNextIncidents[theID];
    EECPNextIncident* theIncident = NULL;
    for (unsigned long k = 0; k < theIncidents.size() && theIncident == NULL; k++)
        if (theNow - theIncidents[k].mLast <= 30000 && EECPNextNear(theIncidents[k].mX, theIncidents[k].mY, theX, theY, 20))
            theIncident = &theIncidents[k];
    long theFoes, theStrength, theAnimals, theOwn, theTower, theDrop;
    EECPNextAround(inPlayer, theX, theY, 10, theFoes, theStrength, theAnimals, theOwn, theTower, theDrop);
    if (theIncident == NULL) {
        EECPNextIncident theNew;
        memset(&theNew, 0, sizeof(theNew));
        theNew.mID = ++sNextIncidentID[theID];
        theNew.mX = theX;
        theNew.mY = theY;
        theNew.mDistance = EECPNextTownDistance(inPlayer, theX, theY);
        theNew.mStart = theNow;
        theNew.mAttacker = inAttacker;
        long theTask, theUnits;
        theNew.mGuard0 = EECPNextNearestGroup(inPlayer, inPlayer->mNextState, theX, theY, theTask, theUnits);
        theNew.mOwn0 = theOwn;
        theNew.mTower0 = theTower;
        theNew.mGuardAt = -1;
        theIncidents.push_back(theNew);
        theIncident = &theIncidents.back();
    }
    theIncident->mLast = theNow;
    theIncident->mHits++;
    if (theFoes > theIncident->mFoes)
        theIncident->mFoes = theFoes;
    if (theStrength > theIncident->mFoeStrength)
        theIncident->mFoeStrength = theStrength;
    EECPNextLossHit& theHit = sNextHits[theID][(long)inUnit->mUnitID];
    if (theHit.mLast == 0 || theNow - theHit.mLast > 30000) {
        theHit.mFirst = theNow;
        theHit.mWhat = EECPNextDoing(inUnit->mComplexUnit);
    }
    theHit.mLast = theNow;
    theHit.mIncident = theIncident->mID;
}

void EECPNextTraceDeath(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    unsigned long theID = inPlayer->mPlayerID;
    if (!gCPTraceOn || inPlayer->mNextState == NULL || theID >= 64 || inUnit == NULL || inUnit->mComplexUnit == NULL
        || (inUnit->mUnitClass->mCategory != kUnitCategoryLandCitizen && inUnit->mUnitClass->mCategory != kUnitCategoryNavalCitizen))
        return;
    bool theBoat = inUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen;
    long theNow = (long)gPhysics->mGameTime;
    EECPNextLossReset(theID, theNow);
    long theX = inUnit->mTileX, theY = inUnit->mTileY;
    EEComplexUnit* theKiller = inUnit->mComplexUnit->mLastAttacker;
    long theKillerPlayer = -1, theKillerClass = -1, theKillerDistance = -1;
    char theKind = '-';
    if (theKiller != NULL && theKiller->mUnitClass != NULL) {
        theKillerPlayer = theKiller->mPlayer != NULL ? (long)theKiller->mPlayer->mPlayerID : -1;
        theKillerClass = theKiller->mUnitClass->mUnitType;
        if (theKiller->GetTileX() != -10000)
            theKillerDistance = EECPNextRoot(EECPNextD2(theKiller->GetTileX(), theKiller->GetTileY(), theX, theY));
        EEUnitClass* theClass = theKiller->mUnitClass;
        theKind = theKillerPlayer == 0 ? 'a' : theClass->IsBuilding() ? (theClass->mIsTower ? 't' : 'b')
            : theClass->mCategory == kUnitCategoryLandCitizen ? 'c' : EECPNextArmySoldier(theKiller) ? 's'
            : (theClass->mDomains & kMovementSea) ? 'n' : 'o';
    }
    long theFoes, theStrength, theAnimals, theOwn, theTower, theDrop;
    EECPNextAround(inPlayer, theX, theY, 10, theFoes, theStrength, theAnimals, theOwn, theTower, theDrop);
    long theTask, theUnits;
    long theGuard = EECPNextNearestGroup(inPlayer, inPlayer->mNextState, theX, theY, theTask, theUnits);
    long theNowDoing = EECPNextDoing(inUnit->mComplexUnit);
    long theWhat = theNowDoing, theHitMs = -1, theIncidentID = 0, theIncidentMs = -1;
    std::map<long, EECPNextLossHit>::iterator theHit = sNextHits[theID].find((long)inUnit->mUnitID);
    if (theHit != sNextHits[theID].end()) {
        theWhat = theHit->second.mWhat;
        theHitMs = theNow - theHit->second.mFirst;
        theIncidentID = theHit->second.mIncident;
        sNextHits[theID].erase(theHit);
    }
    for (unsigned long k = 0; k < sNextIncidents[theID].size(); k++)
        if (sNextIncidents[theID][k].mID == theIncidentID) {
            sNextIncidents[theID][k].mLost++;
            theIncidentMs = theNow - sNextIncidents[theID][k].mStart;
        }
    EECPTraceScope theScope("next");
    EE_CP_TRACE(inPlayer, "next.death",
        "unit=%s x=%ld y=%ld d=%ld epoch=%ld killer=%ld:%s:%c kd=%ld doing=%s now=%s hit=%ld inc=%ld inc_ms=%ld foes=%ld/%ld "
        "animals=%ld own=%ld guard=%ld:%s:%ld tower=%ld drop=%ld",
        theBoat ? "boat" : "citizen", theX, theY, EECPNextTownDistance(inPlayer, theX, theY),
        inPlayer->mTechnologies != NULL ? (long)inPlayer->mTechnologies->mCurrentEpoch : -1L,
        theKillerPlayer, theKillerClass >= 0 ? EECPNextClassName(theKillerClass).c_str() : "-", theKind, theKillerDistance,
        EECPNextDoingName(theWhat), EECPNextDoingName(theNowDoing), theHitMs, theIncidentID, theIncidentMs, theFoes, theStrength,
        theAnimals, theOwn, theGuard, "-", theUnits, theTower, theDrop);
}

// each step (trace on): a group that comes within 10 tiles of an open incident marks it answered; an incident with no
// hit for 30 s is closed (a next.raid line)
static void EECPNextTraceIncidents(EEComputerPlayer* inPlayer, const EECPNextState* inState, long inNow)
{
    unsigned long theID = inPlayer->mPlayerID;
    if (!gCPTraceOn || theID >= 64)
        return;
    EECPNextLossReset(theID, inNow);
    std::vector<EECPNextIncident>& theIncidents = sNextIncidents[theID];
    for (long k = (long)theIncidents.size() - 1; k >= 0; k--) {
        EECPNextIncident& theIncident = theIncidents[k];
        if (theIncident.mGuardAt < 0) {
            long theTask, theUnits;
            long theGuard = EECPNextNearestGroup(inPlayer, inState, theIncident.mX, theIncident.mY, theTask, theUnits);
            if (theGuard >= 0 && theGuard <= 10)
                theIncident.mGuardAt = inNow;
        }
        if (inNow - theIncident.mLast <= 30000)
            continue;
        EE_CP_TRACE(inPlayer, "next.raid",
            "id=%ld x=%ld y=%ld d=%ld start=%ld ms=%ld hits=%ld lost=%ld foes=%ld/%ld attacker=%ld guard0=%ld own0=%ld "
            "tower0=%ld guard_ms=%ld",
            theIncident.mID, theIncident.mX, theIncident.mY, theIncident.mDistance, theIncident.mStart,
            theIncident.mLast - theIncident.mStart, theIncident.mHits, theIncident.mLost, theIncident.mFoes,
            theIncident.mFoeStrength, theIncident.mAttacker, theIncident.mGuard0, theIncident.mOwn0, theIncident.mTower0,
            theIncident.mGuardAt >= 0 ? theIncident.mGuardAt - theIncident.mStart : -1L);
        theIncidents.erase(theIncidents.begin() + k);
    }
    for (std::map<long, EECPNextLossHit>::iterator it = sNextHits[theID].begin(); it != sNextHits[theID].end();)
        if (inNow - it->second.mLast > 120000)
            sNextHits[theID].erase(it++);
        else
            ++it;
}
