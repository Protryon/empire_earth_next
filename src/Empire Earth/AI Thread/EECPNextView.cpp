// The next engine's view (ee/EECPNext.h EECPNextView; docs/AI_LUA_PLAN.md §2 "The view"): a snapshot of what the
// strategy may know, built fresh each step, integers only. Fog-honest: other players' units count only while the
// player sees their tile (EETile::IsSeenBy, as EECPCombatManager::Update; the reveal cheat sees all), never through
// EECPArmyThreat or EECPIntelligence, which read every unit. New in this port: no original address.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include <cstring>

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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIArea.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPBuildManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPResearchManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPTrace.h"
#include "ee/EECPNext.h"

static const long kNextSiteDangerRange = 12;   // tiles: the seen enemy soldiers that count as a site's danger
static const long kNextTowerRange = 8;          // tiles: a tower this near guards a site (EECPRMGuardSites)

static long EECPNextSqrt(long long inValue)     // the integer square root (no float in the view)
{
    if (inValue <= 0)
        return 0;
    long long theRoot = 0, theBit = 1LL << 62;
    while (theBit > inValue)
        theBit >>= 2;
    while (theBit != 0) {
        if (inValue >= theRoot + theBit) {
            inValue -= theRoot + theBit;
            theRoot = (theRoot >> 1) + theBit;
        } else
            theRoot >>= 1;
        theBit >>= 2;
    }
    return (long)theRoot;
}

static long long EECPNextDistance2(long inX, long inY, long inX2, long inY2)
{
    long long theDX = inX - inX2, theDY = inY - inY2;
    return theDX * theDX + theDY * theDY;
}

// hit points x attack, as EECPArmyThreat weighs an army
static long EECPNextStrength(EEUnit* inUnit)
{
    return (long)inUnit->mHitPoints * (long)UMFloatToLong(inUnit->mStats->mAttack);
}

static bool EECPNextIsSoldier(EEUnit* inUnit)
{
    if (inUnit->mComplexUnit == NULL || !(inUnit->mHitPoints > 0) || inUnit->mStats->mAttack == 0.0f
        || inUnit->mUnitClass->IsBuilding())
        return false;
    long theCategory = inUnit->mUnitClass->mCategory;
    return theCategory == kUnitCategoryLand || theCategory == kUnitCategoryLandMilitaryHero;
}

// a resource unit the view and the finders count: some left (a mine's or a patch's hit points drop to 0 at its first
// gather, EEUnit::TakeHit, and it stays gatherable until its amount is gone; the view's old hit point test lost every
// worked mine)
static bool EECPNextResourceLeft(EEUnit* inUnit)
{
    return inUnit->mResourceAmount > 0 && inUnit->mTile != NULL && inUnit->GetTileX() != -10000;
}

// the range in tiles of a building that shoots (a tower, an armed building); 0: it does not
static long EECPNextShootRange(EEUnit* inUnit)
{
    if (inUnit == NULL || !(inUnit->mHitPoints > 0) || inUnit->mStats == NULL || inUnit->mStats->mAttack == 0.0f)
        return 0;
    long theRange = (long)UMFloatToLong(inUnit->mStats->mRange);
    return theRange > 0 ? theRange : 1;
}

// the enemy soldiers in sight this step go into the memory (mThreats, by unit id); the ones not seen for
// kNextThreatMs are forgotten
static void EECPNextUpdateThreats(EECPNextState* inState, long inNow)
{
    for (unsigned long k = 0; k < inState->mSeen.size(); k++) {
        const EECPNextSeen& theSeen = inState->mSeen[k];
        if (theSeen.mKind != kSeenSoldier)
            continue;
        bool theKnown = false;
        for (unsigned long t = 0; t < inState->mThreats.size() && !theKnown; t++)
            if (inState->mThreats[t].mUnit == theSeen.mUnit) {
                inState->mThreats[t].mX = theSeen.mX;
                inState->mThreats[t].mY = theSeen.mY;
                inState->mThreats[t].mTime = inNow;
                theKnown = true;
            }
        if (!theKnown && inState->mThreats.size() < 512) {
            EECPNextSpot theSpot = { theSeen.mPlayer, theSeen.mX, theSeen.mY, inNow, theSeen.mUnit };
            inState->mThreats.push_back(theSpot);
        }
    }
    for (long t = (long)inState->mThreats.size() - 1; t >= 0; t--)
        if (inNow - inState->mThreats[t].mTime > kNextThreatMs)
            inState->mThreats.erase(inState->mThreats.begin() + t);
    inState->mThreatsTime = inNow;
}

long EECPNextSiteThreat(EEComputerPlayer* inPlayer, long inX, long inY, bool inNow, long* outKind)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    long theUnit = 0, theKind = kNextBlockedNone;
    long long theBest = 0;
    for (unsigned long t = 0; t < theState->mThreats.size(); t++) {
        const EECPNextSpot& theSpot = theState->mThreats[t];
        if (inNow && theSpot.mTime != theState->mThreatsTime)
            continue;
        long long theDistance = EECPNextDistance2(inX, inY, theSpot.mX, theSpot.mY);
        if (theDistance <= (long long)kNextThreatRange * kNextThreatRange && (theUnit == 0 || theDistance < theBest)) {
            theUnit = theSpot.mUnit;
            theBest = theDistance;
            theKind = kNextBlockedEnemyNear;
        }
    }
    if (theUnit == 0 && !inNow) {
        // the towers remembered (seen and not seen destroyed), then the armed buildings in sight; the reach is from
        // the building's centre
        for (unsigned long t = 0; t < theState->mTowers.size() && theUnit == 0; t++) {
            const EECPNextSpot& theSpot = theState->mTowers[t];
            EEUnit* theTower = gWorld.GetUnit((unsigned long)theSpot.mUnit);
            long theRange = EECPNextShootRange(theTower);
            long theHalf = theTower != NULL ? theTower->mUnitClass->GetTileSize() / 2 : 1;
            if (theRange == 0)
                theRange = 6;   // (gone from the world unseen: the memory keeps it, at a guard tower's reach)
            long theReach = theRange + theHalf + kNextTowerMargin;
            if (EECPNextDistance2(inX, inY, theSpot.mX + theHalf, theSpot.mY + theHalf) <= (long long)theReach * theReach) {
                theUnit = theSpot.mUnit;
                theKind = kNextBlockedTower;
            }
        }
        for (unsigned long k = 0; k < theState->mSeen.size() && theUnit == 0; k++) {
            const EECPNextSeen& theSeen = theState->mSeen[k];
            if (theSeen.mKind != kSeenBuilding)
                continue;
            EEUnit* theBuilding = gWorld.GetUnit((unsigned long)theSeen.mUnit);
            long theRange = EECPNextShootRange(theBuilding);
            if (theRange == 0)
                continue;
            long theHalf = theBuilding->mUnitClass->GetTileSize() / 2;
            long theReach = theRange + theHalf + kNextTowerMargin;
            if (EECPNextDistance2(inX, inY, theSeen.mX + theHalf, theSeen.mY + theHalf) <= (long long)theReach * theReach) {
                theUnit = theSeen.mUnit;
                theKind = kNextBlockedTower;
            }
        }
    }
    if (outKind != NULL)
        *outKind = theKind;
    return theUnit;
}

// the start resources by the town: the 2 gold, 2 iron, 1 stone and 2 forage patches every player is given (the random
// maps' per-player resources, e.g. Continental Small: kGoldPerPlayer 1 and PlaceResourceInRing (Gold, 14, 16)), taken
// as the nearest of each within kNextHomeRange tiles of the town centre among the player's resource centres (nearest
// first, then the lowest unit id). Mines are never used up, so the home mines stay the same all game.
static void EECPNextUpdateHome(EEComputerPlayer* inPlayer, EECPNextState* inState)
{
    inState->mHome.clear();
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    if (theBuild->mTownCenterTile.mX == -10000)
        return;
    long theTownX = theBuild->mTownCenterTile.mX, theTownY = theBuild->mTownCenterTile.mY;
    static const long kWanted[kNextResources] = {2, 0, 1, 2, 2};
    std::vector<std::pair<long long, long> > theFound[kNextResources];
    for (unsigned long k = 0; k < inPlayer->GetNumResourceCenters(); k++) {
        EECPResourceCenter* theCenter = inPlayer->GetResourceCenter(k);
        for (unsigned long u = 0; u < theCenter->mUnits.size(); u++) {
            EEUnit* theUnit = theCenter->mUnits[u];
            long r = (long)theUnit->mResourceType - kResourceFood;
            if (r < 0 || r >= kNextResources || kWanted[r] == 0 || !EECPNextResourceLeft(theUnit)
                || (r == 0 && theUnit->mUnitClass->mUnitType != kUnitTypeResForageBush))
                continue;
            long long theDistance = EECPNextDistance2(theUnit->GetTileX(), theUnit->GetTileY(), theTownX, theTownY);
            if (theDistance <= (long long)kNextHomeRange * kNextHomeRange)
                theFound[r].push_back(std::make_pair(theDistance, (long)theUnit->mUnitID));
        }
    }
    for (long r = 0; r < kNextResources; r++) {
        std::sort(theFound[r].begin(), theFound[r].end());
        for (unsigned long k = 0; k < theFound[r].size() && (long)k < kWanted[r]; k++)
            inState->mHome.push_back(theFound[r][k].second);
    }
    std::sort(inState->mHome.begin(), inState->mHome.end());
}

bool EECPNextIsHome(EEComputerPlayer* inPlayer, EEUnit* inUnit)
{
    const std::vector<long>& theHome = EECPNextGetState(inPlayer)->mHome;
    return std::binary_search(theHome.begin(), theHome.end(), (long)inUnit->mUnitID);
}

// why the gatherer finders (EECPResourceManager::FindResourceTarget, FindFarm: their next-engine branch) pass a
// resource over: never seen; enemy soldiers seen near it now or lately; an enemy tower or armed building whose range
// covers it; a danger zone (its gatherers attacked there lately); within 10 tiles of the area of a player who is not
// its enemy (gaia, an ally). A home site is passed over only while enemy soldiers are seen on it. kNextBlockedNone
// when they would send gatherers to it.
long EECPNextSiteBlocked(EEComputerPlayer* inPlayer, EEUnit* inUnit, long* outCause)
{
    if (outCause != NULL)
        *outCause = 0;
    if (inUnit->mTile == NULL || !static_cast<EETile*>(inUnit->mTile)->IsVisibleTo(inPlayer))
        return kNextBlockedUnexplored;
    long theX = inUnit->GetTileX(), theY = inUnit->GetTileY();
    bool theHome = EECPNextIsHome(inPlayer, inUnit);
    long theKind = kNextBlockedNone;
    long theCause = EECPNextSiteThreat(inPlayer, theX, theY, theHome, &theKind);
    if (outCause != NULL)
        *outCause = theCause;
    if (theCause != 0)
        return theKind;
    if (theHome)
        return kNextBlockedNone;
    EECPCivilizationManager* theCivilization = inPlayer->mCivilizationManager;
    if (EECPRMResourceInDanger(theCivilization->mResourceManager, theX, theY))
        return kNextBlockedDanger;
    for (unsigned long j = 0; j < gWorld.mNumPlayers; j++) {
        EEPlayer* theOther = gWorld.mPlayers[j];
        if (theOther == NULL || theOther == inPlayer || theOther->mEnemies[inPlayer->mPlayerID] != 0)
            continue;
        EETileCoordinate theTile;
        theTile.mX = theX;
        theTile.mY = theY;
        EEAIArea* theArea = theOther->GetNearestArea(theTile, true);
        if (theArea == NULL)
            continue;
        U2DPoint<long> theClosest = theArea->GetClosestHullPoint(U2DPoint<long>(theX, theY));
        long long theDX = theClosest.mX - theX, theDY = theClosest.mY - theY;
        if (theDX * theDX + theDY * theDY <= 100)
            return kNextBlockedArea;
    }
    return kNextBlockedNone;
}

// TEST ONLY (EE_AI_EXP sitetrace; docs/AI_RESEARCH.md §8.34): one line a minute for each site the gatherers would pass
// over but for being out of sight: its blocked reason, whether it is home, and for enemy_near and tower the unit that
// caused it (its class, owner, category, distance in tiles, and whether the player sees its tile now)
static void EECPNextTraceSites(EEComputerPlayer* inPlayer, const EECPNextView& inView)
{
    static const char* const kReasons[6] = {"ok", "unexplored", "enemy_near", "danger", "area", "tower"};
    static const char* const kNames[kNextResources] = {"food", "wood", "stone", "gold", "iron"};
    for (unsigned long k = 0; k < inView.sites.size(); k++) {
        const EECPNextSite& theSite = inView.sites[k];
        if (theSite.resource < 0 || theSite.blocked < kNextBlockedEnemyNear || theSite.blocked > kNextBlockedTower)
            continue;
        EEUnit* theCause = theSite.cause != 0 ? gWorld.GetUnit((unsigned long)theSite.cause) : NULL;
        if (theCause == NULL || theCause->mUnitClass == NULL || theCause->mPlayer == NULL) {
            EE_CP_TRACE(inPlayer, "next.sitecheck", "site=%ld res=%s d=%ld claimed=%ld home=%ld blocked=%s cause=-",
                theSite.index + 1, kNames[theSite.resource], theSite.distance, theSite.claimed, theSite.home,
                kReasons[theSite.blocked]);
            continue;
        }
        EETile* theTile = static_cast<EETile*>(theCause->mTile);
        EE_CP_TRACE(inPlayer, "next.sitecheck",
            "site=%ld res=%s d=%ld claimed=%ld home=%ld blocked=%s cause=%s owner=p%lu cat=%ld cd=%ld seen=%d",
            theSite.index + 1, kNames[theSite.resource], theSite.distance, theSite.claimed, theSite.home,
            kReasons[theSite.blocked], EECPNextClassName((long)theCause->mUnitClass->mUnitType).c_str(),
            theCause->mPlayer->mPlayerID, (long)theCause->mUnitClass->mCategory,
            EECPNextSqrt(EECPNextDistance2(theSite.x, theSite.y, theCause->GetTileX(), theCause->GetTileY())),
            theTile != NULL && theTile->IsSeenBy(inPlayer) ? 1 : 0);
    }
}

struct EECPNextPlace {
    long mX, mY, mReach, mStrength;
};

const EECPNextBuildings* EECPNextFindBuildings(const EECPNextView& inView, long inType)
{
    for (unsigned long k = 0; k < inView.buildings.size(); k++)
        if (inView.buildings[k].type == inType)
            return &inView.buildings[k];
    return NULL;
}

const EECPNextClassCost* EECPNextFindCost(const EECPNextView& inView, long inType)
{
    for (unsigned long k = 0; k < inView.costs.size(); k++)
        if (inView.costs[k].type == inType)
            return &inView.costs[k];
    return NULL;
}

void EECPNextBuildView(EEComputerPlayer* inPlayer, EECPNextView& outView)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    EECPCivilizationManager* theCivilization = inPlayer->mCivilizationManager;
    EECPResourceManager* theResources = theCivilization->mResourceManager;
    EECPBuildManager* theBuild = theCivilization->mBuildManager;
    EECPUnitManager* theUnits = theCivilization->mUnitManager;
    EETechTree* theTree = inPlayer->mTechnologies;
    unsigned long theNow = gPhysics->mGameTime;
    // the gather safety's memory (the army step's scan just ran) and the home resources
    EECPNextUpdateThreats(theState, (long)theNow);
    EECPNextUpdateHome(inPlayer, theState);

    outView.player = (long)inPlayer->mPlayerID;
    outView.time = (long)theNow;
    outView.epoch = theTree->mCurrentEpoch;
    outView.epoch_start = (long)theState->mEpochStart;
    outView.last_epoch = gWorld.mGameOptions.mEpochEnd < EEEpochCount() - 1 ? (long)gWorld.mGameOptions.mEpochEnd : EEEpochCount() - 1;
    outView.advancing = EECPPendingEpoch(inPlayer) != kTechNone || theCivilization->mAdvancingEpoch ? 1 : 0;
    long theSlot = theState->mIncomeSteps % 60;
    for (long i = 0; i < kNextResources; i++) {
        outView.stock[i] = inPlayer->mResourceCaps[kResourceFood + i];
        outView.income[i] = theState->mIncomeTotal[i] - theState->mIncomeRing[theSlot][i];
        outView.reserve[i] = theState->mReserve[i];
        outView.gatherers[i] = 0;
        outView.wanted[i] = i == 0 ? theState->mFoodLand : (long)theResources->mWantedGatherers[kResourceFood + i];
        outView.next_epoch_cost[i] = 0;
    }
    outView.idle = 0;
    for (long r = 0; r < 7; r++)
        for (unsigned long k = 0; k < theResources->mGatherers[r].size(); k++) {
            if (theResources->mGatherers[r][k]->GetUnitClass()->mCategory == kUnitCategoryNavalCitizen)
                continue;
            if (r == 0)
                outView.idle++;
            else if (r >= kResourceFood && r < kResourceFood + kNextResources)
                outView.gatherers[r - kResourceFood]++;
        }
    outView.population = inPlayer->GetTotalPopulation(true, false);
    outView.population_cap = inPlayer->GetPopulationLimit(0);
    bool theTown = theBuild->mTownCenterTile.mX != -10000;
    long theTownX = theTown ? theBuild->mTownCenterTile.mX : -1, theTownY = theTown ? theBuild->mTownCenterTile.mY : -1;
    outView.town_x = theTownX;
    outView.town_y = theTownY;
    outView.claim_range = EECPClaimRange(inPlayer);

    // the player's own units: citizens, boats, buildings, soldiers, drop sites and towers
    std::map<long, EECPNextBuildings> theBuildings;
    std::map<long, EECPNextSoldiers> theSoldiers;
    std::map<long, long> theInProduction;   // by the class made
    std::vector<EECPNextPlace> theDrops, theFoodDrops, theTowers;
    outView.citizens = 0;
    outView.boats = 0;
    outView.army_count = 0;
    outView.army_strength = 0;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (theComplex == NULL || !(theUnit->mHitPoints > 0))
            continue;
        EEUnitClass* theClass = theUnit->mUnitClass;
        long theType = theClass->mUnitType;
        long theCategory = theClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen)
            outView.citizens++;
        else if (theCategory == kUnitCategoryNavalCitizen)
            outView.boats++;
        if (theClass->IsBuilding()) {
            EECPNextBuildings& theEntry = theBuildings[theType];
            theEntry.type = theType;
            if (theComplex->mIsBuilt) {
                theEntry.finished++;
                long theQueue = (long)theComplex->GetBuildQueueSize();
                if (theQueue > 0)
                    theEntry.busy++;
                theEntry.queue += theQueue;
                for (long q = 0; q < theQueue; q++) {
                    long theID = (*(theComplex->mBuildQueue.begin() + q)).mTechnology;
                    EETechTreeNode* theMade = EEIsEpochID(theID) ? NULL : theTree->Get(theID);
                    if (theMade != NULL && theMade->mData->mMakesUnit)
                        theInProduction[theMade->mData->mUnitClass]++;
                }
            } else
                theEntry.unfinished++;
            long theHalf = theClass->GetTileSize() / 2;
            EECPNextPlace thePlace = { theUnit->GetTileX() + theHalf, theUnit->GetTileY() + theHalf, theHalf, 0 };
            if (theType == kUnitTypeTownCenter || theType == kUnitTypeCapitol || theType == kUnitTypeSettlement) {
                theDrops.push_back(thePlace);
                theFoodDrops.push_back(thePlace);
            } else if (theType == kUnitTypeGranary)
                theFoodDrops.push_back(thePlace);
            if (theClass->mIsTower)
                theTowers.push_back(thePlace);
        } else if (EECPNextIsSoldier(theUnit)) {
            EECPNextSoldiers& theEntry = theSoldiers[theType];
            theEntry.type = theType;
            theEntry.count++;
            theEntry.strength += EECPNextStrength(theUnit);
            outView.army_count++;
            outView.army_strength += EECPNextStrength(theUnit);
        }
    }
    for (long e = 0; e < EEEpochCount(); e++)
        for (unsigned long k = 0; k < theBuild->mQueues[e].size(); k++) {
            unsigned long theClass = theBuild->mQueues[e][k].first;
            if (theClass >= EEUnitClassCount() || gUnitClassTable[theClass] == NULL)
                continue;
            long theType = gUnitClassTable[theClass]->mUnitType;
            theBuildings[theType].type = theType;
            theBuildings[theType].queued++;
        }
    for (std::map<long, EECPNextBuildings>::iterator it = theBuildings.begin(); it != theBuildings.end(); ++it)
        outView.buildings.push_back(it->second);
    for (std::map<long, EECPNextSoldiers>::iterator it = theSoldiers.begin(); it != theSoldiers.end(); ++it)
        outView.soldiers.push_back(it->second);

    // other players' units the player sees now: the threats near the town, the scouting
    std::map<long, EECPNextThreat> theThreats;
    std::vector<EECPNextPlace> theEnemySoldiers;
    long theRange = outView.claim_range;
    for (unsigned long p = 1; p < gWorld.mNumPlayers && p < 64; p++) {
        EEPlayer* theOther = gWorld.mPlayers[p];
        if (theOther == NULL || theOther == inPlayer || inPlayer->IsAlly(theOther) || theOther->mObserverMode)
            continue;
        long theSeen = 0, theSeenStrength = 0;
        bool theAny = false;
        for (EEUnit* theUnit = theOther->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->mTile == NULL
                || !static_cast<EETile*>(theUnit->mTile)->IsSeenBy(inPlayer))
                continue;
            theAny = true;
            long theType = theUnit->mUnitClass->mUnitType;
            if (theType == kUnitTypeTownCenter || theType == kUnitTypeCapitol) {
                theState->mSeenTownX[p] = theUnit->GetTileX();
                theState->mSeenTownY[p] = theUnit->GetTileY();
            }
            if (!EECPNextIsSoldier(theUnit))
                continue;
            long theStrength = EECPNextStrength(theUnit);
            theSeen++;
            theSeenStrength += theStrength;
            EECPNextPlace thePlace = { theUnit->GetTileX(), theUnit->GetTileY(), 0, theStrength };
            theEnemySoldiers.push_back(thePlace);
            if (!theTown)
                continue;
            long long theDistance2 = EECPNextDistance2(theUnit->GetTileX(), theUnit->GetTileY(), theTownX, theTownY);
            if (theDistance2 > (long long)theRange * theRange)
                continue;
            EECPNextThreat& theThreat = theThreats[(long)p];
            long theDistance = EECPNextSqrt(theDistance2);
            if (theThreat.count == 0 || theDistance < theThreat.distance) {
                theThreat.x = theUnit->GetTileX();
                theThreat.y = theUnit->GetTileY();
                theThreat.distance = theDistance;
            }
            theThreat.player = (long)p;
            theThreat.count++;
            theThreat.strength += theStrength;
        }
        if (theAny)
            theState->mLastSeen[p] = theNow;
        if (theSeen > 0) {
            theState->mSeenArmy[p] = theSeen;
            theState->mSeenStrength[p] = theSeenStrength;
        }
        EECPNextEnemy theEnemy;
        theEnemy.player = (long)p;
        theEnemy.epoch = theOther->mTechnologies != NULL ? theOther->mTechnologies->mCurrentEpoch : 0;
        theEnemy.seen_army = theState->mSeenArmy[p];
        theEnemy.seen_strength = theState->mSeenStrength[p];
        theEnemy.last_seen = (long)theState->mLastSeen[p];
        theEnemy.town_x = theState->mSeenTownX[p];
        theEnemy.town_y = theState->mSeenTownY[p];
        outView.enemies.push_back(theEnemy);
    }
    outView.threat_strength = 0;
    for (std::map<long, EECPNextThreat>::iterator it = theThreats.begin(); it != theThreats.end(); ++it) {
        outView.threats.push_back(it->second);
        outView.threat_strength += it->second.strength;
    }

    // the resource centres
    long theLimit = inPlayer->mMiningLimit > 0 ? (long)inPlayer->mMiningLimit : 1;
    for (unsigned long k = 0; k < inPlayer->GetNumResourceCenters(); k++) {
        EECPResourceCenter* theCenter = inPlayer->GetResourceCenter(k);
        EECPNextSite theSite;
        memset(&theSite, 0, sizeof(theSite));
        theSite.index = (long)k;
        theSite.resource = -1;
        theSite.x = theSite.y = -1;
        long theCounts[kNextResources] = {0, 0, 0, 0, 0};
        for (unsigned long u = 0; u < theCenter->mUnits.size(); u++) {
            EEUnit* theUnit = theCenter->mUnits[u];
            long r = (long)theUnit->mResourceType - kResourceFood;
            if (EECPNextResourceLeft(theUnit) && r >= 0 && r < kNextResources) {
                theCounts[r]++;
                if (EECPNextIsHome(inPlayer, theUnit))
                    theSite.home = 1;
            }
        }
        for (long r = 0; r < kNextResources; r++)
            theSite.holds[r] = theCounts[r];
        for (long r = 0; r < kNextResources; r++)
            if (theCounts[r] > 0 && (theSite.resource < 0 || theCounts[r] > theCounts[theSite.resource]))
                theSite.resource = r;
        theSite.claimed = theCenter->mIsActive ? 1 : 0;
        if (theSite.resource >= 0) {
            for (unsigned long u = 0; u < theCenter->mUnits.size(); u++) {
                EEUnit* theUnit = theCenter->mUnits[u];
                if (!EECPNextResourceLeft(theUnit) || (long)theUnit->mResourceType != kResourceFood + theSite.resource)
                    continue;
                if (theSite.units == 0) {
                    theSite.x = theUnit->GetTileX();
                    theSite.y = theUnit->GetTileY();
                    theSite.patch = theUnit->mUnitClass->mUnitType == kUnitTypeResForageBush ? 1 : 0;
                    theSite.blocked = EECPNextSiteBlocked(inPlayer, theUnit, &theSite.cause);
                }
                theSite.units++;
                theSite.amount += theUnit->mResourceAmount;
                theSite.workers += (long)gWorld.GetResourceUsers(theUnit);
            }
            theSite.room = theSite.units * theLimit - theSite.workers;
            if (theSite.room < 0)
                theSite.room = 0;
        } else {
            EETileCoordinate theTile;
            theCenter->GetCenter(theTile);
            theSite.x = theTile.mX;
            theSite.y = theTile.mY;
        }
        if (theSite.x >= 0 && theSite.y >= 0 && gWorld.mMap != NULL) {
            EETile* theTile = static_cast<EETile*>(gWorld.mMap->GetTile((unsigned long)theSite.x,
                (unsigned long)theSite.y));
            theSite.explored = theTile != NULL && theTile->IsVisibleTo(inPlayer) ? 1 : 0;
        }
        theSite.distance = theTown ? EECPNextSqrt(EECPNextDistance2(theSite.x, theSite.y, theTownX, theTownY)) : 0;
        const std::vector<EECPNextPlace>& theDropList = theSite.resource == 0 ? theFoodDrops : theDrops;
        for (unsigned long d = 0; d < theDropList.size() && !theSite.has_drop; d++) {
            long theReach = kEECPPatchDropRange + 2 + theDropList[d].mReach;
            theSite.has_drop = EECPNextDistance2(theSite.x, theSite.y, theDropList[d].mX, theDropList[d].mY)
                <= (long long)theReach * theReach ? 1 : 0;
        }
        for (unsigned long t = 0; t < theTowers.size() && !theSite.has_tower; t++)
            theSite.has_tower = EECPNextDistance2(theSite.x, theSite.y, theTowers[t].mX, theTowers[t].mY)
                <= (long long)kNextTowerRange * kNextTowerRange ? 1 : 0;
        for (unsigned long e = 0; e < theEnemySoldiers.size(); e++)
            if (EECPNextDistance2(theSite.x, theSite.y, theEnemySoldiers[e].mX, theEnemySoldiers[e].mY)
                <= (long long)kNextSiteDangerRange * kNextSiteDangerRange)
                theSite.danger += theEnemySoldiers[e].mStrength;
        if (EECPRMResourceInDanger(theResources, theSite.x, theSite.y))
            theSite.danger++;
        outView.sites.push_back(theSite);
    }

    // the next epoch
    long theNext = theTree->mCurrentEpoch + 1;
    outView.sol_missing = 0;
    if (theNext < EEEpochCount() && theNext < (long)theTree->mEpochs.size() && theTree->mCurrentEpoch < gWorld.mGameOptions.mEpochEnd) {
        EETechTreeEpoch* theEpoch = &theTree->mEpochs[theNext];
        EETechTreeNode* theNode = theTree->Get(theEpoch->mData->mID);
        for (long i = 0; theNode != NULL && i < kNextResources; i++)
            outView.next_epoch_cost[i] = inPlayer->ScaleCost(theNode->GetCost(kResourceFood + i, kUnitTypeTownCenter, 1.0f));
        long theMissing = theEpoch->GetRequiredSOL() - theEpoch->GetCurrentSOL();
        outView.sol_missing = theMissing > 0 ? theMissing : 0;
    }

    // what the player can make now
    for (unsigned long c = 0; c < EEUnitClassCount(); c++) {
        EEUnitClass* theClass = gUnitClassTable[c];
        if (theClass == NULL)
            continue;
        EETechTreeNode* theNode = theTree->Get(theClass->mTechnology);
        if (theNode == NULL || theNode->mData == NULL || !theNode->mData->mMakesUnit || theNode->mData->mUnitClass != (long)c
            || !theNode->GetIsAvailable() || theNode->GetIsObsolete())
            continue;
        EECPNextClassCost theCost;
        memset(&theCost, 0, sizeof(theCost));
        theCost.type = (long)c;
        for (long i = 0; i < kNextResources; i++)
            theCost.cost[i] = inPlayer->ScaleCost(theNode->GetCost(kResourceFood + i, (long)c, 1.0f));
        theCost.building = theClass->IsBuilding() ? 1 : 0;
        theCost.military = inPlayer->mCombatManager != NULL && inPlayer->mCombatManager->IsMilitaryClass(theClass) ? 1 : 0;
        theCost.producer = theNode->GetNumUnitClasses() > 0 ? theNode->GetUnitClass(0) : -1;
        for (unsigned long i = 0; i < theNode->GetNumUnitClasses(); i++) {
            std::deque<EEComplexUnit*>* theMakers = theUnits->GetUnits(theNode->GetUnitClass(i));
            for (unsigned long m = 0; theMakers != NULL && m < theMakers->size(); m++)
                if ((*theMakers)[m]->mIsBuilt)
                    theCost.producers++;
        }
        std::map<unsigned long, unsigned long>::iterator theWaiting = theUnits->mWaitingCounts.find(c);
        theCost.requested = theWaiting != theUnits->mWaitingCounts.end() ? (long)theWaiting->second : 0;
        std::map<long, long>::iterator theMaking = theInProduction.find((long)c);
        theCost.in_production = theMaking != theInProduction.end() ? theMaking->second : 0;
        theCost.counts_toward_epoch = theNode->mData->mCountsTowardEpoch ? 1 : 0;
        outView.costs.push_back(theCost);
    }

    outView.events = theState->mEvents;
    EECPNextArmyView(inPlayer, outView);   // the groups, the free soldiers, the enemies' army fields (EECPNextArmy.cpp)
    if (gCPTraceOn && theNow % 60000 < 1000 && EECPExperiment(inPlayer, "sitetrace"))
        EECPNextTraceSites(inPlayer, outView);
}
