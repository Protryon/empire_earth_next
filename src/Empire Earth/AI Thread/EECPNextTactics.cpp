// The next engine's verbs (ee/EECPNext.h; docs/AI_LUA_PLAN.md §2, docs/AI_LUA_AUDIT.md "Extraction order"). Each one
// validates its order, hands it to the classic executor (the gatherer assignment, the build queue and its placement,
// the production and research runners, the attack groups) and reports done, queued or refused with a reason in one
// trace line. New in this port: no original address.
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
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPUnitManager.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUnitAction.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include "ee/EECPBuildManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPResearchManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPIntelligence.h"
#include "ee/EECPTrace.h"
#include "ee/EECPNext.h"
#include "ee/EEImprovementIDs.h"

static const long kNextMaxCount = 100;   // a larger count in one order is a bad argument

// one trace line per order: `next.order verb=<verb> <arguments> result=<code> reason=<reason> n=<count>`
static EECPNextResult EECPNextReport(EEComputerPlayer* inPlayer, const char* inVerb, long inCode, long inReason,
    long inCount, const char* inFormat, ...)
{
    EECPNextResult theResult = { inCode, inReason, inCount };
    if (gCPTraceOn) {
        char theArgs[192];
        va_list theList;
        va_start(theList, inFormat);
        vsnprintf(theArgs, sizeof(theArgs), inFormat, theList);
        va_end(theList);
        EECPTraceScope theScope("next");
        EECPTraceOut(inPlayer, "next.order", "verb=%s %s result=%s reason=%s n=%ld", inVerb, theArgs,
            EECPNextCodeName(inCode), EECPNextReasonName(inReason), inCount);
    }
    return theResult;
}

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

// whether the player has a unit (built or not) of one of the classes that make or research a node
static bool EECPNextHasProducer(EEComputerPlayer* inPlayer, EETechTreeNode* inNode)
{
    EECPUnitManager* theUnits = inPlayer->mCivilizationManager->mUnitManager;
    for (unsigned long i = 0; i < inNode->GetNumUnitClasses(); i++)
        if (theUnits->GetCount(inNode->GetUnitClass(i)) > 0)
            return true;
    return false;
}

// whether one of the player's producers of a node has it in its queue (TryResearch started it: it left mEpochIDs)
static bool EECPNextInQueue(EEComputerPlayer* inPlayer, EETechTreeNode* inNode)
{
    EECPUnitManager* theUnits = inPlayer->mCivilizationManager->mUnitManager;
    for (unsigned long i = 0; i < inNode->GetNumUnitClasses(); i++) {
        std::deque<EEComplexUnit*>* theMakers = theUnits->GetUnits(inNode->GetUnitClass(i));
        for (unsigned long m = 0; theMakers != NULL && m < theMakers->size(); m++) {
            EEComplexUnit* theMaker = (*theMakers)[m];
            for (unsigned long q = 0; q < theMaker->GetBuildQueueSize(); q++)
                if ((*(theMaker->mBuildQueue.begin() + q)).mTechnology == inNode->mData->mID)
                    return true;
        }
    }
    return false;
}

static EECPResourceCenter* EECPNextSiteAt(EEComputerPlayer* inPlayer, long inSite)
{
    if (inSite < 0 || (unsigned long)inSite >= inPlayer->GetNumResourceCenters())
        return NULL;
    return inPlayer->GetResourceCenter((unsigned long)inSite);
}

// the tile of a centre's first resource unit with some left (what its site in the view shows; a worked mine's hit
// points are 0)
static bool EECPNextSiteTile(EECPResourceCenter* inCenter, long& outX, long& outY)
{
    for (unsigned long u = 0; u < inCenter->mUnits.size(); u++) {
        EEUnit* theUnit = inCenter->mUnits[u];
        if (theUnit->mResourceAmount > 0 && theUnit->mTile != NULL) {
            outX = theUnit->GetTileX();
            outY = theUnit->GetTileY();
            return true;
        }
    }
    return false;
}

// the first tile in the rings inFirst..inLast round a tile where the class can be built (CanBuildAt, as
// EECPRMPatchSite's rings round a forage patch)
static bool EECPNextRingSite(EEComputerPlayer* inPlayer, long inType, long inX, long inY, long inFirst, long inLast,
    EETileCoordinate& outTile)
{
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    EEUnitClass* theClass = gUnitClassTable[inType];
    unsigned short theContinent = gContinentManager.GetContinent(U2DPoint<long>(inX, inY), kAIPathLandMass);
    std::vector<EETileCoordinate> theTiles;
    for (long theRing = inFirst; theRing <= inLast; theRing++) {
        if (theRing == 0) {
            EETileCoordinate theTile(inX, inY);
            if (theBuild->CanBuildAt(theContinent, inType, theTile, theClass->GetTileSize(), true, true, inX, inY)) {
                outTile = theTile;
                return true;
            }
            continue;
        }
        theTiles.clear();
        EEGetSquareRingTiles(inX, inY, theRing, theTiles);
        for (unsigned long k = 0; k < theTiles.size(); k++)
            if (theBuild->CanBuildAt(theContinent, inType, theTiles[k], theClass->GetTileSize(), true, true, inX, inY)) {
                outTile = theTiles[k];
                return true;
            }
    }
    return false;
}

// a tower's tile in the rings inFirst..inLast round a tile, none of the player's towers within its range of it (the
// rings' order: deterministic; CanBuildAt: only tiles in sight, no armed enemy near)
static bool EECPNextTowerRingSite(EEComputerPlayer* inPlayer, long inType, long inX, long inY, long inFirst, long inLast,
    EETileCoordinate& outTile)
{
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    EEUnitClass* theClass = gUnitClassTable[inType];
    unsigned short theContinent = gContinentManager.GetContinent(U2DPoint<long>(inX, inY), kAIPathLandMass);
    float theSpacing = inPlayer->mUnitClassUpgrades[inType]->mRange;
    std::vector<EETileCoordinate> theTiles;
    for (long theRing = inFirst; theRing <= inLast; theRing++) {
        theTiles.clear();
        EEGetSquareRingTiles(inX, inY, theRing, theTiles);
        for (unsigned long k = 0; k < theTiles.size(); k++)
            if (!theBuild->HasTowerNear(theTiles[k].mX, theTiles[k].mY, theSpacing * theSpacing)
                && theBuild->CanBuildAt(theContinent, inType, theTiles[k], theClass->GetTileSize(), true, true, inX, inY)) {
                outTile = theTiles[k];
                return true;
            }
    }
    return false;
}

// the tiles between a building of inSize at a tile (its corner) and a resource unit's footprint (0: touching)
static long EECPNextGap(long inX, long inY, long inSize, EEUnit* inUnit)
{
    long theUX = inUnit->GetTileX(), theUY = inUnit->GetTileY();
    long theUSize = inUnit->mUnitClass->GetTileSize() > 0 ? inUnit->mUnitClass->GetTileSize() : 1;
    long theDX = (std::max)((std::max)(theUX - (inX + inSize), inX - (theUX + theUSize)), 0L);
    long theDY = (std::max)((std::max)(theUY - (inY + inSize), inY - (theUY + theUSize)), 0L);
    return (std::max)(theDX, theDY);
}

static long long EECPNextDistance2Of(long inX, long inY, long inX2, long inY2)
{
    long long theDX = inX - inX2, theDY = inY - inY2;
    return theDX * theDX + theDY * theDY;
}

// a drop site by its resource (§8.35, the owner: "settlements directly adjacent to all resource mines, not just randomly
// 3 tiles away"): of the tiles in the rings 1..inLast round a tile where the class can be built (CanBuildAt keeps a
// one-tile margin free, the gatherers' side), the one nearest the resource units (inUnits; the tile itself when
// empty), then nearest the town centre (the shorter way home), then the first in the rings' order
static bool EECPNextAdjacentSite(EEComputerPlayer* inPlayer, long inType, long inX, long inY, long inLast,
    const std::vector<EEUnit*>& inUnits, EETileCoordinate& outTile, long& outGap)
{
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    EEUnitClass* theClass = gUnitClassTable[inType];
    long theSize = theClass->GetTileSize() > 0 ? theClass->GetTileSize() : 1;
    // the town's land (a tree's tile is forest, on no land continent: every settlement by a forest was refused)
    unsigned short theContinent = gContinentManager.GetContinent(theBuild->mTownCenterTile, kAIPathLandMass);
    long theTownX = theBuild->mTownCenterTile.mX, theTownY = theBuild->mTownCenterTile.mY;
    std::vector<EETileCoordinate> theTiles;
    bool theFound = false;
    long theBestGap = 0;
    long long theBestHome = 0;
    for (long theRing = 1; theRing <= inLast; theRing++) {
        theTiles.clear();
        EEGetSquareRingTiles(inX - theSize / 2, inY - theSize / 2, theRing, theTiles);
        for (unsigned long k = 0; k < theTiles.size(); k++) {
            long theGap = 1000;
            if (inUnits.empty()) {
                long theDX = (std::max)((std::max)(inX - (theTiles[k].mX + theSize - 1), theTiles[k].mX - inX), 0L);
                long theDY = (std::max)((std::max)(inY - (theTiles[k].mY + theSize - 1), theTiles[k].mY - inY), 0L);
                theGap = (std::max)(theDX, theDY);
            }
            for (unsigned long u = 0; u < inUnits.size(); u++)
                theGap = (std::min)(theGap, EECPNextGap(theTiles[k].mX, theTiles[k].mY, theSize, inUnits[u]));
            if (theFound && theGap > theBestGap)
                continue;
            long long theDX = theTiles[k].mX - theTownX, theDY = theTiles[k].mY - theTownY;
            long long theHome = theDX * theDX + theDY * theDY;
            if (theFound && theGap == theBestGap && theHome >= theBestHome)
                continue;
            if (!theBuild->CanBuildAt(theContinent, inType, theTiles[k], theSize, true, true, inX, inY))
                continue;
            theFound = true;
            theBestGap = theGap;
            theBestHome = theHome;
            outTile = theTiles[k];
            outGap = theGap;
        }
        if (theFound && theBestGap <= 1 && theRing >= theBestGap + 2)
            break;   // (nothing nearer lies further out)
    }
    return theFound;
}

// ---------------------------------------------------------------------------------------------------------------
// Gathering and claims (EECPResourceManager: AssignGatherers, RebalanceGatherers, the finders and SendToGather carry
// them out; UpdateWantedGatherers, EECPRMSplitByDemand, EECPRMActivateNeeded, SettlePatches and GuardSites never run)

// the gatherer places by a finished drop site (§8.36): the claimed forage patches (kNextPatchPlaces each) or mines
// (the mining limit a unit with some left) whose drop site stands; wood: kNextMaxCount when a forest edge has one
static long EECPNextReadyPlaces(EEComputerPlayer* inPlayer, long inResource)
{
    if (inResource == 1)
        return EECPNextWoodTarget(inPlayer, 0, NULL) != NULL ? kNextMaxCount : 0;
    long theLimit = inPlayer->mMiningLimit > 0 ? (long)inPlayer->mMiningLimit : 1;
    long thePlaces = 0;
    for (unsigned long k = 0; k < inPlayer->GetNumResourceCenters(); k++) {
        EECPResourceCenter* theCenter = inPlayer->GetResourceCenter(k);
        if (!theCenter->mIsActive)
            continue;
        long theUnits = 0;
        bool theReady = false;
        for (unsigned long u = 0; u < theCenter->mUnits.size(); u++) {
            EEUnit* theUnit = theCenter->mUnits[u];
            if ((long)theUnit->mResourceType != kResourceFood + inResource || !(theUnit->mResourceAmount > 0)
                || theUnit->mTile == NULL || (inResource == 0 && theUnit->mUnitClass->mUnitType != kUnitTypeResForageBush))
                continue;
            theUnits++;
            theReady = theReady || EECPNextUnitDropReady(inPlayer, theUnit);
        }
        if (theReady)
            thePlaces += inResource == 0 ? kNextPatchPlaces : theUnits * theLimit;
    }
    return thePlaces;
}

EECPNextResult EECPNextGather(EEComputerPlayer* inPlayer, long inResource, long inCount, long inPrefer, long inExact)
{
    if (inResource < 0 || inResource >= kNextResources || inCount < 0 || inCount > 1000
        || (inPrefer != kNextPreferPatch && inPrefer != kNextPreferHunt))
        return EECPNextReport(inPlayer, "gather", kNextRefused, kNextReasonBadArgument, 0, "resource=%ld want=%ld prefer=%ld",
            inResource, inCount, inPrefer);
    EECPNextState* theState = EECPNextGetState(inPlayer);
    EECPResourceManager* theResources = inPlayer->mCivilizationManager->mResourceManager;
    if (inExact >= 0)
        theState->mExact = inExact != 0 ? 1 : 0;
    unsigned long theWanted = (unsigned long)inCount;
    if (inResource == 0) {
        // the boats fishing count as food gatherers in the assignment: they come on top
        unsigned long theBoats = 0;
        for (long r = 0; r < 7; r++)
            for (unsigned long k = 0; k < theResources->mGatherers[r].size(); k++)
                if (theResources->mGatherers[r][k]->GetUnitClass()->mCategory == kUnitCategoryNavalCitizen)
                    theBoats++;
        theWanted += theBoats;
        theState->mFoodLand = inCount;
        theState->mFoodPrefer = inPrefer;
    }
    theResources->mWantedGatherers[kResourceFood + inResource] = theWanted;
    if (inCount > 0 && !(inResource == 0 && inPrefer == kNextPreferHunt)) {
        long thePlaces = EECPNextReadyPlaces(inPlayer, inResource);
        if (thePlaces < inCount)   // (the game for hunting has no drop site rule: the patches' places only)
            return EECPNextReport(inPlayer, "gather", kNextQueued, kNextReasonNoDropoff, inCount,
                "resource=%ld want=%ld prefer=%ld ready=%ld", inResource, inCount, inPrefer, thePlaces);
    }
    return EECPNextReport(inPlayer, "gather", kNextDone, kNextReasonNone, inCount, "resource=%ld want=%ld prefer=%ld",
        inResource, inCount, inPrefer);
}

static EECPNextResult EECPNextSetActive(EEComputerPlayer* inPlayer, long inSite, bool inActive)
{
    const char* theVerb = inActive ? "claim" : "release";
    EECPResourceCenter* theCenter = EECPNextSiteAt(inPlayer, inSite);
    if (theCenter == NULL)
        return EECPNextReport(inPlayer, theVerb, kNextRefused, kNextReasonBadArgument, 0, "site=%ld", inSite);
    long theX = -1, theY = -1;
    EECPNextSiteTile(theCenter, theX, theY);
    if (theCenter->mIsActive == inActive)
        return EECPNextReport(inPlayer, theVerb, kNextDone, kNextReasonNone, 0, "site=%ld x=%ld y=%ld", inSite, theX, theY);
    theCenter->SetActive(inActive, 0);
    return EECPNextReport(inPlayer, theVerb, kNextDone, kNextReasonNone, 1, "site=%ld x=%ld y=%ld", inSite, theX, theY);
}

EECPNextResult EECPNextClaim(EEComputerPlayer* inPlayer, long inSite)
{
    return EECPNextSetActive(inPlayer, inSite, true);
}

EECPNextResult EECPNextRelease(EEComputerPlayer* inPlayer, long inSite)
{
    return EECPNextSetActive(inPlayer, inSite, false);
}

EECPNextResult EECPNextClaimRange(EEComputerPlayer* inPlayer, long inTiles)
{
    if (inTiles < kEECPClaimRange || inTiles > kEECPClaimRangeMax)
        return EECPNextReport(inPlayer, "claim_range", kNextRefused, kNextReasonBadArgument, 0, "range=%ld", inTiles);
    EECPNextGetState(inPlayer)->mClaimRange = inTiles;
    return EECPNextReport(inPlayer, "claim_range", kNextDone, kNextReasonNone, inTiles, "range=%ld", inTiles);
}

// ---------------------------------------------------------------------------------------------------------------
// Building (EECPBuildManager::QueueBuilding / QueueBuildingAt and the build runner; the QueueBuilding vetoes, the
// ClaimExpand of StartBuilding, CheckBuildingCounts and QueueMoraleBuilding are classic only)

static EECPNextResult EECPNextDockByFish(EEComputerPlayer* inPlayer);

EECPNextResult EECPNextBuild(EEComputerPlayer* inPlayer, long inType, long inNear, long inArg1, long inArg2)
{
    long theReason;
    EETechTreeNode* theNode = EECPNextClassNode(inPlayer, inType, theReason);
    // (a fortress is a transport class by its category, a land building by its placement: IsLandBuilding)
    if (theNode != NULL && ((!gUnitClassTable[inType]->IsBuilding() && !gUnitClassTable[inType]->IsLandBuilding())
                            || (gUnitClassTable[inType]->mIsWall && !gUnitClassTable[inType]->mIsTower)))
        theReason = kNextReasonBadArgument;   // a unit (train it) or a wall (wall plans come later)
    if (theReason == kNextReasonNone && inNear != kNextNearTown && inNear != kNextNearSite && inNear != kNextNearTile)
        theReason = kNextReasonBadArgument;
    if (theReason != kNextReasonNone)
        return EECPNextReport(inPlayer, "build", kNextRefused, theReason, 0, "cls=%ld near=%ld arg=%ld/%ld", inType, inNear,
            inArg1, inArg2);
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    if (theBuild->mTownCenterTile.mX == -10000)
        return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonNoSite, 0, "cls=%ld near=%ld arg=%ld/%ld (no town)",
            inType, inNear, inArg1, inArg2);
    EETileCoordinate theTile(0, 0);
    bool theFound = false;
    long theGap = -1;   // a drop site's tiles from its resource (EECPNextAdjacentSite; -1: not placed that way)
    const char* theNearName = "town";
    if (inNear == kNextNearTown && inType == kUnitTypeDock)
        return EECPNextDockByFish(inPlayer);   // (§8.35: by the fish near home, or none)
    if (inNear == kNextNearTown)
        theFound = theBuild->FindSite(inType, theTile);   // the classic placement for the class
    else if (inNear == kNextNearSite) {
        theNearName = "site";
        EECPResourceCenter* theCenter = EECPNextSiteAt(inPlayer, inArg1);
        long theX, theY;
        if (theCenter == NULL || !EECPNextSiteTile(theCenter, theX, theY))
            return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonBadArgument, 0, "cls=%ld near=site arg=%ld",
                inType, inArg1);
        // a settlement next to the resource (EECPNextAdjacentSite: the nearest free tile to the centre's units, §8.35),
        // anything else a little further out so that it does not stand on the gatherers' path
        if (inType == kUnitTypeSettlement || inType == kUnitTypeGranary) {
            std::vector<EEUnit*> theUnits, theNoDrop;
            for (unsigned long u = 0; u < theCenter->mUnits.size(); u++)
                if (theCenter->mUnits[u]->mResourceAmount > 0 && theCenter->mUnits[u]->mTile != NULL) {
                    theUnits.push_back(theCenter->mUnits[u]);
                    if (!EECPNextUnitDropReady(inPlayer, theCenter->mUnits[u]))
                        theNoDrop.push_back(theCenter->mUnits[u]);
                }
            if (!theNoDrop.empty() && theNoDrop.size() < theUnits.size()) {
                // (§8.36: by the units with no drop site yet: a centre's iron 6 tiles from its gold's settlement)
                theUnits.swap(theNoDrop);
                theX = theUnits[0]->GetTileX();
                theY = theUnits[0]->GetTileY();
            }
            theFound = EECPNextAdjacentSite(inPlayer, inType, theX, theY, kEECPPatchDropRange + 4, theUnits, theTile, theGap);
            // §8.36: a site not explored yet has only far tiles known to be free (the settlement went 5-6 tiles off and
            // its patch never had a drop at hand): the nearest citizen not building walks there first, and the order
            // comes again once the site is seen
            EETile* theSiteTile = gWorld.mMap != NULL
                ? static_cast<EETile*>(gWorld.mMap->GetTile((unsigned long)theX, (unsigned long)theY)) : NULL;
            // (a building goes only on tiles in sight now, EECanPlaceUnitOnTile: the walker stands there until the
            // order comes again; once it is there the rings' result stands)
            bool theArrived = false;
            {
                EECPNextState* theState = EECPNextGetState(inPlayer);
                for (unsigned long e = 0; e < theState->mExplore.size(); e++) {
                    EEUnit* theUnit = gWorld.GetUnit((unsigned long)theState->mExplore[e].mUnit);
                    if (theState->mExplore[e].mX == theX && theState->mExplore[e].mY == theY && theUnit != NULL
                        && theUnit->mHitPoints > 0 && theUnit->mTile != NULL
                        && EECPNextDistance2Of(theUnit->GetTileX(), theUnit->GetTileY(), theX, theY) <= 9)
                        theArrived = true;
                }
            }
            if ((!theFound || theGap > 2) && !theArrived && theSiteTile != NULL) {
                if (EECPNextBuildUnsafe(inPlayer, theX, theY) != kNextBlockedNone)
                    return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonUnsafe, 0,
                        "cls=%ld near=site arg=%ld x=%ld y=%ld (unexplored)", inType, inArg1, theX, theY);
                // one walker a site: one sent here lately and alive (walking or standing there) is enough
                EECPNextState* theState = EECPNextGetState(inPlayer);
                long theNow = (long)gPhysics->mGameTime;
                EEComplexUnit* theWalker = NULL;
                for (long e = (long)theState->mExplore.size() - 1; e >= 0; e--) {
                    EECPNextSpot& theSpot = theState->mExplore[e];
                    EEUnit* theUnit = gWorld.GetUnit((unsigned long)theSpot.mUnit);
                    if (theNow - theSpot.mTime > kNextExploreMs || theUnit == NULL || !(theUnit->mHitPoints > 0)
                        || theUnit->mComplexUnit == NULL)
                        theState->mExplore.erase(theState->mExplore.begin() + e);
                    else if (theSpot.mX == theX && theSpot.mY == theY)
                        theWalker = theUnit->mComplexUnit;
                }
                bool theNew = theWalker == NULL;
                std::vector<EEComplexUnit*> theCitizens;
                if (theNew)
                    inPlayer->mCivilizationManager->mUnitManager->GetCitizens(theCitizens);
                long long theBest = 0;
                for (unsigned long c = 0; c < theCitizens.size(); c++) {
                    EEComplexUnit* theUnit = theCitizens[c];
                    if (!(theUnit->GetHitPoints() > 0) || theUnit->GetTileX() == -10000 || theUnit->mIsDetachedFromAI
                        || dynamic_cast<EEUGBuildRepair*>(theUnit->GetLoadGoal()) != NULL
                        || dynamic_cast<EEUGLocation*>(theUnit->GetLoadGoal()) != NULL)
                        continue;
                    long long theDX = theUnit->GetTileX() - theX, theDY = theUnit->GetTileY() - theY;
                    if (theWalker == NULL || theDX * theDX + theDY * theDY < theBest) {
                        theWalker = theUnit;
                        theBest = theDX * theDX + theDY * theDY;
                    }
                }
                if (theNew && theWalker != NULL) {
                    theWalker->ClearGoalQueue(0);
                    theWalker->SetGoal(new EEUGLocation((float)theX, (float)theY, 0.0f, false, true, false, 0.0f, false, false), 0, 0);
                    theWalker->StartAction(kActionPrepareToMove);
                    theWalker->mUnitClass->ActionChanged(theWalker);
                    EECPNextSpot theSpot = { (long)inPlayer->mPlayerID, theX, theY, theNow, (long)theWalker->mUnitID };
                    theState->mExplore.push_back(theSpot);
                }
                return EECPNextReport(inPlayer, "build", kNextQueued, kNextReasonUnexplored, theWalker != NULL ? 1 : 0,
                    "cls=%ld near=site arg=%ld x=%ld y=%ld gap=%ld (explore)", inType, inArg1, theX, theY, theFound ? theGap : -1L);
            }
        } else
            theFound = EECPNextRingSite(inPlayer, inType, theX, theY, 2, 7, theTile);
        // a settlement with no room in those rings (trees, cliffs, other buildings round a mine): the rings a little
        // further out, then the centre's own placement (as BuildSettlementFor's fallback), still near the resource
        if (!theFound && inType == kUnitTypeSettlement) {
            theFound = EECPNextRingSite(inPlayer, inType, theX, theY, kEECPPatchDropRange + 3, kEECPPatchDropRange + 5,
                theTile);
            if (!theFound && theCenter->FindBuildSite(inType, theTile)) {
                long theDX = theTile.mX - theX, theDY = theTile.mY - theY;
                theFound = theDX * theDX + theDY * theDY <= (kEECPPatchDropRange + 6) * (kEECPPatchDropRange + 6);
            }
        }
        // a tower with no room round its site (the rings' tiles not in sight, under trees or the mine, or an armed
        // enemy near: a far or contested mine) goes to the town instead (below), rather than being refused for as
        // long as that site stays unguarded
        if (!theFound && gUnitClassTable[inType]->mIsTower)
            theNearName = "site>town";
    } else {
        theNearName = "tile";
        if (gWorld.mMap == NULL || inArg1 < 0 || inArg2 < 0 || gWorld.mMap->GetTile((unsigned long)inArg1, (unsigned long)inArg2) == NULL)
            return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonBadArgument, 0, "cls=%ld near=tile arg=%ld/%ld",
                inType, inArg1, inArg2);
        if (inType == kUnitTypeSettlement) {   // (§8.35: by the forest edge the wood gatherers work)
            std::vector<EEUnit*> theNone;
            theFound = EECPNextAdjacentSite(inPlayer, inType, inArg1, inArg2, 7, theNone, theTile, theGap);
        } else
            theFound = EECPNextRingSite(inPlayer, inType, inArg1, inArg2, 0, 6, theTile);
    }
    // a tower at the town: the classic placement (FindTowerSite: the gold, iron and worked food centres, gates, choke
    // points), else the rings 3..12 round the town centre away from the other towers (FindTowerSite finds nothing once
    // those centres are guarded or out of sight)
    if (!theFound && gUnitClassTable[inType]->mIsTower && inNear != kNextNearTile) {
        if (inNear == kNextNearSite)
            theFound = theBuild->FindSite(inType, theTile);
        if (!theFound) {
            long theHalf = gUnitClassTable[kUnitTypeTownCenter] != NULL ? gUnitClassTable[kUnitTypeTownCenter]->GetTileSize() / 2 : 0;
            theFound = EECPNextTowerRingSite(inPlayer, inType, theBuild->mTownCenterTile.mX + theHalf,
                theBuild->mTownCenterTile.mY + theHalf, 3, 12, theTile);
            theNearName = inNear == kNextNearSite ? "site>ring" : "town>ring";
        }
    }
    if (!theFound || (theTile.mX == 0 && theTile.mY == 0)) {
        if (gCPTraceOn && inType == kUnitTypeSettlement && inNear == kNextNearSite) {
            // TEST ONLY (§8.36): why the rings round the site took no settlement: of the tiles 1..8 out, how many the
            // footprint check refuses (units, terrain), how many lie off the town's land, on a choke point or a hill
            EECPResourceCenter* theCenter = EECPNextSiteAt(inPlayer, inArg1);
            long theX = -1, theY = -1;
            if (theCenter != NULL && EECPNextSiteTile(theCenter, theX, theY)) {
                long theSize = gUnitClassTable[inType]->GetTileSize() > 0 ? gUnitClassTable[inType]->GetTileSize() : 1;
                unsigned short theContinent = gContinentManager.GetContinent(theBuild->mTownCenterTile, kAIPathLandMass);
                long theTiles = 0, theBlocked = 0, theLand = 0, theChoke = 0, theHill = 0;
                std::vector<EETileCoordinate> theRing;
                for (long r = 1; r <= 8; r++) {
                    theRing.clear();
                    EEGetSquareRingTiles(theX - theSize / 2, theY - theSize / 2, r, theRing);
                    for (unsigned long k = 0; k < theRing.size(); k++) {
                        theTiles++;
                        if (!EEPlayerBlockTakes(inPlayer, inType, theRing[k].mX - 1, theRing[k].mY - 1, theSize + 2, theSize + 2, 0))
                            theBlocked++;
                        else if (gContinentManager.GetContinent(theRing[k], kAIPathLandMass) != theContinent)
                            theLand++;
                        else if (gContinentManager.GetChokePointAt(theRing[k]) != NULL)
                            theChoke++;
                        else if (gContinentManager.GetHillAt(theRing[k]) != NULL)
                            theHill++;
                    }
                }
                EE_CP_TRACE(inPlayer, "build.nosite_why", "x=%ld y=%ld tiles=%ld blocked=%ld land=%ld choke=%ld hill=%ld", theX,
                    theY, theTiles, theBlocked, theLand, theChoke, theHill);
            }
        }
        return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonNoSite, 0, "cls=%ld near=%s arg=%ld/%ld", inType,
            theNearName, inArg1, inArg2);
    }
    // §8.35: never where the builders would walk under a seen enemy tower or into enemy soldiers seen there
    long theUnsafe = EECPNextBuildUnsafe(inPlayer, theTile.mX, theTile.mY);
    if (theUnsafe != kNextBlockedNone)
        return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonUnsafe, 0, "cls=%ld near=%s arg=%ld/%ld x=%ld y=%ld why=%ld",
            inType, theNearName, inArg1, inArg2, (long)theTile.mX, (long)theTile.mY, theUnsafe);
    bool theAfford = inPlayer->CanAfford(theNode, kUnitTypeCitizen, 1);
    if (inNear == kNextNearSite) {   // (§8.36: the walker sent to see the site is free again; it is the nearest builder)
        EECPNextState* theState = EECPNextGetState(inPlayer);
        for (long e = (long)theState->mExplore.size() - 1; e >= 0; e--) {
            long theDX = theState->mExplore[e].mX - theTile.mX, theDY = theState->mExplore[e].mY - theTile.mY;
            if (theDX * theDX + theDY * theDY <= 100)
                theState->mExplore.erase(theState->mExplore.begin() + e);
        }
    }
    theBuild->QueueBuildingAt(inType, theTile.mX, theTile.mY);
    return EECPNextReport(inPlayer, "build", kNextQueued, kNextReasonNone, 1, "cls=%ld near=%s arg=%ld/%ld x=%ld y=%ld gap=%ld afford=%d",
        inType, theNearName, inArg1, inArg2, (long)theTile.mX, (long)theTile.mY, theGap, theAfford ? 1 : 0);
}

// the buildings of a type: built, going up and waiting in the build queues
static long EECPNextBuildingCount(EEComputerPlayer* inPlayer, long inType, long* outFinished)
{
    long theCount = 0, theFinished = 0;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (theUnit->mUnitClass->mUnitType == inType && theUnit->mComplexUnit != NULL && theUnit->mHitPoints > 0) {
            theCount++;
            if (theUnit->mComplexUnit->mIsBuilt)
                theFinished++;
        }
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    for (long e = 0; e < EEEpochCount(); e++)
        for (unsigned long k = 0; k < theBuild->mQueues[e].size(); k++)
            if ((long)theBuild->mQueues[e][k].first == inType)
                theCount++;
    if (outFinished != NULL)
        *outFinished = theFinished;
    return theCount;
}

static const long kNextDockSpacing = 12;   // tiles: fish this near one of the player's docks are served by it

// a dock queued at the shore by the nearest fish within kNextDockReach of the town centre with none of the player's
// docks within kNextDockSpacing of it: the rings 1..6 round the fish, the land tile nearest the town first
static EECPNextResult EECPNextDockByFish(EEComputerPlayer* inPlayer)
{
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    EECPResourceManager* theResources = inPlayer->mCivilizationManager->mResourceManager;
    long theReason;
    EETechTreeNode* theNode = EECPNextClassNode(inPlayer, kUnitTypeDock, theReason);
    if (theNode == NULL)
        return EECPNextReport(inPlayer, "build", kNextRefused, theReason, 0, "cls=%ld near=fish", (long)kUnitTypeDock);
    if (theBuild->mTownCenterTile.mX == -10000)
        return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonNoSite, 0, "cls=%ld near=fish (no town)", (long)kUnitTypeDock);
    long theTownX = theBuild->mTownCenterTile.mX, theTownY = theBuild->mTownCenterTile.mY;
    std::vector<EEUnit*> theDocks;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (theUnit->mUnitClass->mUnitType == kUnitTypeDock && theUnit->mHitPoints > 0)
            theDocks.push_back(theUnit);
    for (long e = 0; e < EEEpochCount(); e++)
        for (unsigned long k = 0; k < theBuild->mQueues[e].size(); k++)
            if ((long)theBuild->mQueues[e][k].first == kUnitTypeDock)
                return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonBusy, 0, "cls=%ld near=fish (queued)", (long)kUnitTypeDock);
    EEUnitClass* theClass = gUnitClassTable[kUnitTypeDock];
    long theSize = theClass->GetTileSize() > 0 ? theClass->GetTileSize() : 1;
    unsigned short theContinent = gContinentManager.GetContinent(theBuild->mTownCenterTile, kAIPathLandMass);
    EEUnit* theBestFish = NULL;
    long long theBestD2 = 0;
    long theNear = 0, theSeen = 0;   // (the trace: fish within reach, and of those seen)
    for (unsigned long f = 0; f < theResources->mFish.size(); f++) {
        EEComplexUnit* theFish = theResources->mFish[f];
        if (theFish == NULL || !(theFish->mResourceAmount > 0) || theFish->mTile == NULL)
            continue;
        {
            long long theDX = theFish->GetTileX() - theTownX, theDY = theFish->GetTileY() - theTownY;
            if (theDX * theDX + theDY * theDY <= (long long)kNextDockReach * kNextDockReach)
                theNear++;
        }
        // (seen or not: the fish by the town are the start's food, as the home mines are, §8.34; the AI never explores
        // the water, and the playtest's town had fish at home but its dock 59 tiles out)
        long long theDX = theFish->GetTileX() - theTownX, theDY = theFish->GetTileY() - theTownY;
        long long theD2 = theDX * theDX + theDY * theDY;
        if (theD2 > (long long)kNextDockReach * kNextDockReach || (theBestFish != NULL && theD2 >= theBestD2))
            continue;
        theSeen++;
        bool theServed = false;
        for (unsigned long d = 0; d < theDocks.size() && !theServed; d++) {
            long long theX = theDocks[d]->GetTileX() - theFish->GetTileX(), theY = theDocks[d]->GetTileY() - theFish->GetTileY();
            theServed = theX * theX + theY * theY <= (long long)kNextDockSpacing * kNextDockSpacing;
        }
        if (theServed)
            continue;
        // a shore tile for it
        bool theShore = false;
        std::vector<EETileCoordinate> theTiles;
        for (long theRing = 1; theRing <= 6 && !theShore; theRing++) {
            theTiles.clear();
            EEGetSquareRingTiles(theFish->GetTileX(), theFish->GetTileY(), theRing, theTiles);
            for (unsigned long k = 0; k < theTiles.size() && !theShore; k++)
                theShore = theBuild->CanBuildAt(theContinent, kUnitTypeDock, theTiles[k], theSize, false, true, theTownX, theTownY);
        }
        if (theShore) {
            theBestFish = theFish;
            theBestD2 = theD2;
        }
    }
    if (theBestFish == NULL)
        return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonNoSite, 0, "cls=%ld near=fish fish=%ld seen=%ld (no fish near home)",
            (long)kUnitTypeDock, theNear, theSeen);
    EETileCoordinate theTile(0, 0);
    bool theFound = false;
    long long theBestHome = 0;
    std::vector<EETileCoordinate> theTiles;
    for (long theRing = 1; theRing <= 6 && !theFound; theRing++) {
        theTiles.clear();
        EEGetSquareRingTiles(theBestFish->GetTileX(), theBestFish->GetTileY(), theRing, theTiles);
        for (unsigned long k = 0; k < theTiles.size(); k++) {
            if (!theBuild->CanBuildAt(theContinent, kUnitTypeDock, theTiles[k], theSize, false, true, theTownX, theTownY))
                continue;
            long long theDX = theTiles[k].mX - theTownX, theDY = theTiles[k].mY - theTownY;
            if (!theFound || theDX * theDX + theDY * theDY < theBestHome) {
                theFound = true;
                theBestHome = theDX * theDX + theDY * theDY;
                theTile = theTiles[k];
            }
        }
    }
    if (!theFound)
        return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonNoSite, 0, "cls=%ld near=fish", (long)kUnitTypeDock);
    long theUnsafe = EECPNextBuildUnsafe(inPlayer, theTile.mX, theTile.mY);
    if (theUnsafe != kNextBlockedNone)
        return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonUnsafe, 0, "cls=%ld near=fish x=%ld y=%ld why=%ld",
            (long)kUnitTypeDock, (long)theTile.mX, (long)theTile.mY, theUnsafe);
    bool theAfford = inPlayer->CanAfford(theNode, kUnitTypeCitizen, 1);
    theBuild->QueueBuildingAt(kUnitTypeDock, theTile.mX, theTile.mY);
    return EECPNextReport(inPlayer, "build", kNextQueued, kNextReasonNone, 1, "cls=%ld near=fish fx=%ld fy=%ld x=%ld y=%ld afford=%d",
        (long)kUnitTypeDock, (long)theBestFish->GetTileX(), (long)theBestFish->GetTileY(), (long)theTile.mX, (long)theTile.mY,
        theAfford ? 1 : 0);
}

// fish(boats, docks) (from EECPRMRequestBoats): a dock at the shore while fewer than inDocks stand or are queued, then
// fishing boats up to inBoats (made, in production or requested)
EECPNextResult EECPNextFish(EEComputerPlayer* inPlayer, long inBoats, long inDocks)
{
    if (inBoats < 0 || inBoats > kNextMaxCount || inDocks < 0 || inDocks > 10)
        return EECPNextReport(inPlayer, "fish", kNextRefused, kNextReasonBadArgument, 0, "boats=%ld docks=%ld", inBoats, inDocks);
    EECPCivilizationManager* theCivilization = inPlayer->mCivilizationManager;
    long theBuilt = 0;
    long theDocks = EECPNextBuildingCount(inPlayer, kUnitTypeDock, &theBuilt);
    long theCode = kNextDone, theReason = kNextReasonNone, theCount = 0;
    if (theDocks < inDocks) {
        // §8.35 (the owner: "they have fish at home! no dock at home", "a dock really, really far away from home"):
        // the dock goes to the shore by the nearest fish within kNextDockReach of the town that no dock of the
        // player's serves yet (kNextDockSpacing); with no such fish, none (the classic shore site was the largest
        // water's shore nearest the town, 59 tiles out in the playtest)
        EECPNextResult theDock = EECPNextDockByFish(inPlayer);
        if (theDock.mCode == kNextRefused && theBuilt == 0)
            return EECPNextReport(inPlayer, "fish", kNextRefused, theDock.mReason, 0, "boats=%ld docks=%ld", inBoats, inDocks);
        theCode = kNextQueued;
    }
    if (theBuilt == 0)
        return EECPNextReport(inPlayer, "fish", theCode, theCode == kNextQueued ? kNextReasonNone : kNextReasonNoProducer, 0,
            "boats=%ld docks=%ld (no dock yet)", inBoats, inDocks);
    long theClass = theCivilization->GetFishingBoatClass();
    long theNodeReason;
    if (EECPNextClassNode(inPlayer, theClass, theNodeReason) == NULL)
        return EECPNextReport(inPlayer, "fish", kNextRefused, theNodeReason, 0, "boats=%ld docks=%ld cls=%ld", inBoats,
            inDocks, theClass);
    // the boats there are (every naval citizen), in production at the docks and requested
    long theHave = 0;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0))
            continue;
        if (theUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen)
            theHave++;
        else if (theUnit->mUnitClass->IsBuilding())
            for (unsigned long q = 0; q < theUnit->mComplexUnit->GetBuildQueueSize(); q++) {
                long theID = (*(theUnit->mComplexUnit->mBuildQueue.begin() + q)).mTechnology;
                EETechTreeNode* theMade = EEIsEpochID(theID) ? NULL : inPlayer->mTechnologies->Get(theID);
                if (theMade != NULL && theMade->mData->mMakesUnit && theMade->mData->mUnitClass == theClass)
                    theHave++;
            }
    }
    std::map<unsigned long, unsigned long>::iterator theWaiting = theCivilization->mUnitManager->mWaitingCounts.find((unsigned long)theClass);
    if (theWaiting != theCivilization->mUnitManager->mWaitingCounts.end())
        theHave += (long)theWaiting->second;
    if (theHave < inBoats) {
        theCount = inBoats - theHave;
        theCivilization->mUnitManager->RequestUnits(theClass, theCount, 0, 0);
        theCode = kNextQueued;
    }
    return EECPNextReport(inPlayer, "fish", theCode, theReason, theCount, "boats=%ld docks=%ld cls=%ld have=%ld", inBoats,
        inDocks, theClass, theHave);
}

// ---------------------------------------------------------------------------------------------------------------
// Production (EECPUnitManager::RequestUnits and the production runner, StartProduction for the producer; no
// automatic citizens, no producer queued on its own, no standing army: the strategy trains)

EECPNextResult EECPNextTrain(EEComputerPlayer* inPlayer, long inType, long inCount, long inTown)
{
    long theReason;
    EETechTreeNode* theNode = EECPNextClassNode(inPlayer, inType, theReason);
    if (theNode != NULL && (gUnitClassTable[inType]->IsBuilding() || !theNode->mData->mMakesUnit))
        theReason = kNextReasonBadArgument;   // a building: build it
    if (theReason == kNextReasonNone && (inCount <= 0 || inCount > kNextMaxCount || inTown < 0))
        theReason = kNextReasonBadArgument;
    if (theReason == kNextReasonNone && !EECPNextHasProducer(inPlayer, theNode))
        theReason = kNextReasonNoProducer;
    if (theReason != kNextReasonNone)
        return EECPNextReport(inPlayer, "train", kNextRefused, theReason, 0, "cls=%ld want=%ld town=%ld", inType, inCount, inTown);
    bool theAfford = inPlayer->CanAfford(theNode, inType, 1);
    inPlayer->mCivilizationManager->mUnitManager->RequestUnits(inType, inCount, inTown, 0);
    return EECPNextReport(inPlayer, "train", kNextQueued, kNextReasonNone, inCount, "cls=%ld want=%ld town=%ld afford=%d",
        inType, inCount, inTown, theAfford ? 1 : 0);
}

EECPNextResult EECPNextReserve(EEComputerPlayer* inPlayer, long inFood, long inWood, long inStone, long inGold, long inIron)
{
    long theAmounts[kNextResources] = {inFood, inWood, inStone, inGold, inIron};
    for (long i = 0; i < kNextResources; i++)
        if (theAmounts[i] < 0 || theAmounts[i] > 1000000)
            return EECPNextReport(inPlayer, "reserve", kNextRefused, kNextReasonBadArgument, 0, "f=%ld w=%ld s=%ld g=%ld i=%ld",
                inFood, inWood, inStone, inGold, inIron);
    EECPNextState* theState = EECPNextGetState(inPlayer);
    for (long i = 0; i < kNextResources; i++)
        theState->mReserve[i] = theAmounts[i];
    return EECPNextReport(inPlayer, "reserve", kNextDone, kNextReasonNone, 0, "f=%ld w=%ld s=%ld g=%ld i=%ld", inFood, inWood,
        inStone, inGold, inIron);
}

// ---------------------------------------------------------------------------------------------------------------
// Improvements (§8.35, the owner: "upgrading tower range is worth it for AI")

EECPNextResult EECPNextImprove(EEComputerPlayer* inPlayer, long inType, long inImprovement)
{
    long theReason;
    EETechTreeNode* theNode = EECPNextClassNode(inPlayer, inType, theReason);
    if (theNode == NULL)
        return EECPNextReport(inPlayer, "improve", kNextRefused, theReason, 0, "cls=%ld what=%ld", inType, inImprovement);
    EEUnitClass* theClass = gUnitClassTable[inType];
    if (inImprovement < 0 || inImprovement >= 16 || theClass->mImprovements == NULL)
        return EECPNextReport(inPlayer, "improve", kNextRefused, kNextReasonBadArgument, 0, "cls=%ld what=%ld", inType, inImprovement);
    long theLevel = inPlayer->GetImprovementLevel(inType, inImprovement);
    if (!inPlayer->CanBuy(inType, inImprovement, NULL)) {
        // the level's cost not in stock, or no level left (the class's maximum or the improvement points)
        bool theMax = theLevel > theClass->mImprovements->GetMaxLevel();
        return EECPNextReport(inPlayer, "improve", kNextRefused, theMax ? kNextReasonRequirements : kNextReasonCannotAfford, 0,
            "cls=%ld what=%ld level=%ld max=%ld step=%ld", inType, inImprovement, theLevel, theClass->mImprovements->GetMaxLevel(),
            (long)UMFloatToLong(theClass->mImprovements->mEntries[inImprovement].mStepFactor * 100.0f));
    }
    // the reserve: PayImprovement's cost (the class's cost x its cost scale x the level's factor) out of the stock above it
    EECPNextState* theState = EECPNextGetState(inPlayer);
    EEUnitClassUpgrade* theUpgrade = inPlayer->mUnitClassUpgrades[inType];
    float theFactor = theClass->mImprovements->mEntries[inImprovement].mCostFactor * (float)theLevel;
    for (long i = 0; i < kNextResources; i++) {
        long theCost = inPlayer->ScaleCost(UMFloatToLong((float)theNode->mData->mCosts[kResourceFood + i] * theUpgrade->mCostScale * theFactor));
        if (theCost > 0 && inPlayer->mResourceCaps[kResourceFood + i] - theCost < theState->mReserve[i])
            return EECPNextReport(inPlayer, "improve", kNextRefused, kNextReasonCannotAfford, 0, "cls=%ld what=%ld level=%ld (reserve)",
                inType, inImprovement, theLevel);
    }
    inPlayer->PayImprovement(theClass, inImprovement);
    inPlayer->ApplyImprovement(inType, inImprovement, false);
    return EECPNextReport(inPlayer, "improve", kNextDone, kNextReasonNone, 1, "cls=%ld what=%ld level=%ld", inType, inImprovement,
        inPlayer->GetImprovementLevel(inType, inImprovement));
}

// ---------------------------------------------------------------------------------------------------------------
// Research and the advance (EECPResearchManager::Research into mEpochIDs, TryResearch starts it; AttemptEpochAdvance,
// SetNextEpochAdvanceTime, ChooseResearch and ResearchCheapest never run)

EECPNextResult EECPNextResearch(EEComputerPlayer* inPlayer, long inTech)
{
    EETechTree* theTree = inPlayer->mTechnologies;
    EECPResearchManager* theResearch = inPlayer->mCivilizationManager->mResearchManager;
    EETechTreeNode* theNode = theTree != NULL && inTech >= 0 && inTech < kTechNone ? theTree->Get(inTech) : NULL;
    long theReason = kNextReasonNone;
    if (theNode == NULL || theNode->mData == NULL)
        theReason = kNextReasonUnknownClass;
    else if (EEIsEpochID(inTech) || !theResearch->IsWanted(inTech))
        theReason = kNextReasonBadArgument;   // an epoch (advance) or a unit (train, build)
    else if (theNode->mIsResearched)   // nothing to do (e.g. an epoch's kind-7 tech: EEComputerPlayer::EnterEpoch's)
        return EECPNextReport(inPlayer, "research", kNextDone, kNextReasonNone, 0, "tech=%ld (researched)", inTech);
    else if (!theNode->GetIsAvailable() || theNode->GetIsObsolete())
        theReason = kNextReasonNotInEpoch;
    else {
        for (long e = 0; e < EEEpochCount() && theReason == kNextReasonNone; e++)
            if (std::find(theResearch->mEpochIDs[e].begin(), theResearch->mEpochIDs[e].end(), inTech) != theResearch->mEpochIDs[e].end())
                theReason = kNextReasonBusy;
        if (theReason == kNextReasonNone && EECPNextInQueue(inPlayer, theNode))
            theReason = kNextReasonBusy;   // under way at a producer
        if (theReason == kNextReasonNone && !EECPNextHasProducer(inPlayer, theNode))
            theReason = kNextReasonNoProducer;
    }
    if (theReason != kNextReasonNone)
        return EECPNextReport(inPlayer, "research", kNextRefused, theReason, 0, "tech=%ld", inTech);
    theResearch->Research(inTech);
    return EECPNextReport(inPlayer, "research", kNextQueued, kNextReasonNone, 1, "tech=%ld", inTech);
}

EECPNextResult EECPNextAdvance(EEComputerPlayer* inPlayer)
{
    EETechTree* theTree = inPlayer->mTechnologies;
    EECPCivilizationManager* theCivilization = inPlayer->mCivilizationManager;
    long theEpoch = theTree->mCurrentEpoch;
    long theNext = theEpoch + 1;
    if (theNext >= EEEpochCount() || theNext >= (long)theTree->mEpochs.size() || !(theEpoch < gWorld.mGameOptions.mEpochEnd))
        return EECPNextReport(inPlayer, "advance", kNextRefused, kNextReasonRequirements, 0, "from=%ld (last epoch)", theEpoch);
    EETechTreeEpoch* theNode = &theTree->mEpochs[theNext];
    long theMissing = theNode->GetRequiredSOL() - theNode->GetCurrentSOL();
    if (theMissing > 0)
        return EECPNextReport(inPlayer, "advance", kNextRefused, kNextReasonRequirements, 0, "from=%ld sol_missing=%ld",
            theEpoch, theMissing);
    if (EECPPendingEpoch(inPlayer) != kTechNone || theCivilization->mAdvancingEpoch)
        return EECPNextReport(inPlayer, "advance", kNextRefused, kNextReasonBusy, 0, "from=%ld", theEpoch);
    EETechTreeNode* theTech = theTree->Get(theNode->mData->mID);
    if (theTech == NULL || !EECPNextCanAfford(inPlayer, theTech, kUnitTypeTownCenter, 1))
        return EECPNextReport(inPlayer, "advance", kNextRefused, kNextReasonCannotAfford, 0, "from=%ld", theEpoch);
    // as AttemptEpochAdvance does before it asks: the requests that the next epoch makes obsolete go
    theCivilization->mUnitManager->DropObsoleteRequests(theNext);
    theCivilization->mBuildManager->DropObsoleteRequests(theNext);
    theCivilization->mResearchManager->Research(theNode->mData->mID);
    if (EECPPendingEpoch(inPlayer) == kTechNone)
        return EECPNextReport(inPlayer, "advance", kNextRefused, kNextReasonNoProducer, 0, "from=%ld", theEpoch);
    theCivilization->mAdvancingEpoch = true;
    return EECPNextReport(inPlayer, "advance", kNextQueued, kNextReasonNone, 1, "from=%ld to=%ld", theEpoch, theNext);
}

// (the groups, defend and attack: EECPNextArmy.cpp)
