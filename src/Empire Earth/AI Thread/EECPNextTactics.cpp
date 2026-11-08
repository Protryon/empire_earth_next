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

static EECPResourceCenter* EECPNextSiteAt(EEComputerPlayer* inPlayer, long inSite)
{
    if (inSite < 0 || (unsigned long)inSite >= inPlayer->GetNumResourceCenters())
        return NULL;
    return inPlayer->GetResourceCenter((unsigned long)inSite);
}

// the tile of a centre's first living resource unit (what its site in the view shows)
static bool EECPNextSiteTile(EECPResourceCenter* inCenter, long& outX, long& outY)
{
    for (unsigned long u = 0; u < inCenter->mUnits.size(); u++) {
        EEUnit* theUnit = inCenter->mUnits[u];
        if (theUnit->mHitPoints > 0 && theUnit->mResourceAmount > 0) {
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
        Fn2492C2(inX, inY, theRing, theTiles);
        for (unsigned long k = 0; k < theTiles.size(); k++)
            if (theBuild->CanBuildAt(theContinent, inType, theTiles[k], theClass->GetTileSize(), true, true, inX, inY)) {
                outTile = theTiles[k];
                return true;
            }
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------
// Gathering and claims (EECPResourceManager: AssignGatherers, RebalanceGatherers, the finders and SendToGather carry
// them out; UpdateWantedGatherers, EECPRMSplitByDemand, EECPRMActivateNeeded, SettlePatches and GuardSites never run)

EECPNextResult EECPNextGather(EEComputerPlayer* inPlayer, long inResource, long inCount, long inPrefer)
{
    if (inResource < 0 || inResource >= kNextResources || inCount < 0 || inCount > 1000
        || (inPrefer != kNextPreferPatch && inPrefer != kNextPreferHunt))
        return EECPNextReport(inPlayer, "gather", kNextRefused, kNextReasonBadArgument, 0, "resource=%ld want=%ld prefer=%ld",
            inResource, inCount, inPrefer);
    EECPNextState* theState = EECPNextGetState(inPlayer);
    EECPResourceManager* theResources = inPlayer->mCivilizationManager->mResourceManager;
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

EECPNextResult EECPNextBuild(EEComputerPlayer* inPlayer, long inType, long inNear, long inArg1, long inArg2)
{
    long theReason;
    EETechTreeNode* theNode = EECPNextClassNode(inPlayer, inType, theReason);
    if (theNode != NULL && (!gUnitClassTable[inType]->IsBuilding() || (gUnitClassTable[inType]->mIsWall && !gUnitClassTable[inType]->mIsTower)))
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
    const char* theNearName = "town";
    if (inNear == kNextNearTown)
        theFound = theBuild->FindSite(inType, theTile);   // the classic placement for the class
    else if (inNear == kNextNearSite) {
        theNearName = "site";
        EECPResourceCenter* theCenter = EECPNextSiteAt(inPlayer, inArg1);
        long theX, theY;
        if (theCenter == NULL || !EECPNextSiteTile(theCenter, theX, theY))
            return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonBadArgument, 0, "cls=%ld near=site arg=%ld",
                inType, inArg1);
        // a settlement in the rings round the resource (EECPRMPatchSite: 2..kEECPPatchDropRange + 2), anything else
        // a little further out so that it does not stand on the gatherers' path
        theFound = inType == kUnitTypeSettlement ? EECPNextRingSite(inPlayer, inType, theX, theY, 2, kEECPPatchDropRange + 2, theTile)
            : EECPNextRingSite(inPlayer, inType, theX, theY, 2, 7, theTile);
    } else {
        theNearName = "tile";
        if (gWorld.mMap == NULL || inArg1 < 0 || inArg2 < 0 || gWorld.mMap->GetTile((unsigned long)inArg1, (unsigned long)inArg2) == NULL)
            return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonBadArgument, 0, "cls=%ld near=tile arg=%ld/%ld",
                inType, inArg1, inArg2);
        theFound = EECPNextRingSite(inPlayer, inType, inArg1, inArg2, 0, 6, theTile);
    }
    if (!theFound || (theTile.mX == 0 && theTile.mY == 0))
        return EECPNextReport(inPlayer, "build", kNextRefused, kNextReasonNoSite, 0, "cls=%ld near=%s arg=%ld/%ld", inType,
            theNearName, inArg1, inArg2);
    bool theAfford = inPlayer->CanAfford(theNode, kUnitTypeCitizen, 1);
    theBuild->QueueBuildingAt(inType, theTile.mX, theTile.mY);
    return EECPNextReport(inPlayer, "build", kNextQueued, kNextReasonNone, 1, "cls=%ld near=%s arg=%ld/%ld x=%ld y=%ld afford=%d",
        inType, theNearName, inArg1, inArg2, (long)theTile.mX, (long)theTile.mY, theAfford ? 1 : 0);
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
        EECPNextResult theDock = EECPNextBuild(inPlayer, kUnitTypeDock, kNextNearTown);
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
    EECPUGatherSpecialUnitsCount theBoats(inPlayer, theClass, 0, false);
    theCivilization->mUnitManager->GatherUnits(theBoats);
    long theHave = (long)(theBoats.mUnits + theBoats.mRequested);
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
    else if (theNode->mIsResearched)
        theReason = kNextReasonBusy;
    else if (!theNode->GetIsAvailable() || theNode->GetIsObsolete())
        theReason = kNextReasonNotInEpoch;
    else {
        for (long e = 0; e < EEEpochCount() && theReason == kNextReasonNone; e++)
            if (std::find(theResearch->mEpochIDs[e].begin(), theResearch->mEpochIDs[e].end(), inTech) != theResearch->mEpochIDs[e].end())
                theReason = kNextReasonBusy;
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

// ---------------------------------------------------------------------------------------------------------------
// Groups (EECPAttackCommander::CreateGroupFromStrategy with an explicit size and the classes there are; the group
// state machines carry them out; CombatManager::Update's attack choice, the attack timers, PlanAid, BuildUp,
// CheckRaids and NoteThreat never run, and a group trains nothing: Personnel::RequestProduction is classic only)

// a strategy of up to inCount of the player's land soldiers, the classes taken in order of type
static long EECPNextGroupStrategy(EEComputerPlayer* inPlayer, long inCount, long inKind, EECPCombatStrategy& outStrategy)
{
    std::map<long, long> theClasses;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->mStats->mAttack == 0.0f
            || theUnit->mUnitClass->IsBuilding())
            continue;
        long theCategory = theUnit->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryLand || theCategory == kUnitCategoryLandMilitaryHero)
            theClasses[theUnit->mUnitClass->mUnitType]++;
    }
    long theLeft = inCount;
    outStrategy.Reset();
    outStrategy.mAddFavorites = false;
    outStrategy.mKind = inKind;
    for (std::map<long, long>::iterator it = theClasses.begin(); it != theClasses.end() && theLeft > 0; ++it) {
        long theTake = it->second < theLeft ? it->second : theLeft;
        EECPUnitRequirement theRequirement;
        theRequirement.mUnitClass = it->first;
        theRequirement.mKind = kRequirementAlreadyDefined;
        theRequirement.mNumLeft = theTake;
        theRequirement.mNumWanted = theTake;
        outStrategy.mRequirements.push_back(theRequirement);
        theLeft -= theTake;
    }
    outStrategy.mNumUnits = inCount - theLeft;
    outStrategy.mNumRemaining = inCount - theLeft;
    return inCount - theLeft;
}

// the units a new group holds (its sub-groups')
static long EECPNextGroupUnits(EECPAttackGroup* inGroup)
{
    long theCount = 0;
    for (unsigned long i = 0; i < inGroup->GetNumSubGroups(); i++)
        theCount += (long)inGroup->mSubGroups[i]->GetNumUnits();
    return theCount;
}

static EEPlayer* EECPNextFirstEnemy(EEComputerPlayer* inPlayer)
{
    for (unsigned long p = 1; p < gWorld.mNumPlayers; p++) {
        EEPlayer* theOther = gWorld.mPlayers[p];
        if (theOther != NULL && theOther != inPlayer && !inPlayer->IsAlly(theOther) && !theOther->mObserverMode
            && theOther->mFirstUnit != NULL)
            return theOther;
    }
    return NULL;
}

EECPNextResult EECPNextDefend(EEComputerPlayer* inPlayer, long inX, long inY, long inCount)
{
    EECPBuildManager* theBuild = inPlayer->mCivilizationManager->mBuildManager;
    if (inX == -1 && inY == -1) {
        inX = theBuild->mTownCenterTile.mX;
        inY = theBuild->mTownCenterTile.mY;
    }
    if (inCount <= 0 || inCount > kNextMaxCount || gWorld.mMap == NULL || inX < 0 || inY < 0
        || gWorld.mMap->GetTile((unsigned long)inX, (unsigned long)inY) == NULL || inPlayer->mCombatManager == NULL)
        return EECPNextReport(inPlayer, "defend", kNextRefused, kNextReasonBadArgument, 0, "x=%ld y=%ld want=%ld", inX, inY, inCount);
    EECPCombatStrategy theStrategy;
    long theSize = EECPNextGroupStrategy(inPlayer, inCount, 1, theStrategy);
    EEPlayer* theEnemy = EECPNextFirstEnemy(inPlayer);
    if (theSize == 0 || theEnemy == NULL)
        return EECPNextReport(inPlayer, "defend", kNextRefused, kNextReasonNoUnits, 0, "x=%ld y=%ld want=%ld", inX, inY, inCount);
    // as EECPDefenseCommander::CreateDefendGroup, at a location
    EECPAGTask theTask;
    theTask.mPlayer = theEnemy;
    theTask.mIsActive = false;
    theTask.mType = kAGTaskLocation;
    theTask.mMode = kAGModeDefense;
    theTask.mLocation = EECPILocation(EETileLocation(inX, inY));
    std::vector<long> theIDs;
    EECPAttackGroup* theGroup = inPlayer->mCombatManager->mAttackCommander->CreateGroupFromStrategy(theTask, theStrategy, theIDs);
    if (theGroup == NULL)
        return EECPNextReport(inPlayer, "defend", kNextRefused, kNextReasonNoUnits, 0, "x=%ld y=%ld want=%ld", inX, inY, inCount);
    for (long i = theGroup->GetNumSubGroups() - 1; i >= 0; i--)
        theGroup->mSubGroups[i]->mMission = 1;
    return EECPNextReport(inPlayer, "defend", kNextDone, kNextReasonNone, EECPNextGroupUnits(theGroup), "x=%ld y=%ld want=%ld",
        inX, inY, inCount);
}

EECPNextResult EECPNextAttack(EEComputerPlayer* inPlayer, long inTarget, long inCount)
{
    EEPlayer* theTarget = inTarget >= 1 && (unsigned long)inTarget < gWorld.mNumPlayers ? gWorld.mPlayers[inTarget] : NULL;
    if (theTarget == NULL || theTarget == inPlayer || inPlayer->IsAlly(theTarget) || theTarget->mObserverMode
        || theTarget->mFirstUnit == NULL || inCount <= 0 || inCount > kNextMaxCount || inPlayer->mCombatManager == NULL)
        return EECPNextReport(inPlayer, "attack", kNextRefused, kNextReasonBadArgument, 0, "target=%ld want=%ld", inTarget, inCount);
    EECPCombatStrategy theStrategy;
    if (EECPNextGroupStrategy(inPlayer, inCount, 0, theStrategy) == 0)
        return EECPNextReport(inPlayer, "attack", kNextRefused, kNextReasonNoUnits, 0, "target=%ld want=%ld", inTarget, inCount);
    // as EECPCombatManager::Update's attack: a task naming the target player
    EECPAGTask theTask;
    theTask.mPlayer = theTarget;
    std::vector<long> theIDs;
    EECPAttackGroup* theGroup = inPlayer->mCombatManager->mAttackCommander->CreateGroupFromStrategy(theTask, theStrategy, theIDs);
    if (theGroup == NULL)
        return EECPNextReport(inPlayer, "attack", kNextRefused, kNextReasonNoUnits, 0, "target=%ld want=%ld", inTarget, inCount);
    return EECPNextReport(inPlayer, "attack", kNextDone, kNextReasonNone, EECPNextGroupUnits(theGroup), "target=%ld want=%ld", inTarget,
        inCount);
}
