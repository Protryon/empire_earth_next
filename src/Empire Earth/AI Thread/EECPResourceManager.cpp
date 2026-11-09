// C:\EEX\Empire Earth\AI Thread\EECPResourceManager.cpp (file name inferred: no __FILE__; the class-static key
// "EECPResourceManager-Data"). A computer player's resource manager: 0x8e702..0x9350f (the key chain first, then two
// sort predicates, the members in address order, the sort/find_if COMDATs last).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

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
#include "lle/USingleton.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPUnitTileDistanceLess.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUGLoadContainer.h"
#include "ee/EEUGLoadGarrison.h"
#include "ee/EEUGLoadTransport.h"
#include "ee/EEUGUnloadTransport.h"
#include "ee/UReferenceContainer.h"
#include "ee/EEUCBasicCitizen.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPBuildManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPPersonnel.h"
#include "ee/EEWheel.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EECPResearchManager.h"
#include "ee/EECPTrace.h"
#include "ee/EECPNext.h"

const UString EECPResourceManager::sData("EECPResourceManager-Data");
// SYNTHETIC: EXE 0x48e702   (wrapper)
// SYNTHETIC: EXE 0x48e70c   (ctor)
// SYNTHETIC: EXE 0x48e71d   (registrar)
// SYNTHETIC: EXE 0x48e729   (dtor)

// The forests by the distance of their closest hull point from a tile (RefreshResources's sort; built without /Og like the
// other units' sort predicates) (provisional name)
struct EECPRMForestDistanceLess {
    EECPRMForestDistanceLess(U2DPoint<long> inCenter);
    long DistanceSquared(const EETileCoordinate& inTile) const;   // 0x8a3f (ICF)   // 0x160fba (ICF-folded with EEPatrolTileIs's ctor)
    bool operator()(unsigned short inFirst, unsigned short inSecond) const;

    EETileCoordinate mCenter;
};

// The units by the distance of their tiles from a tile (RefreshResources's second sort) (provisional name)
struct EECPRMUnitDistanceLess {
    EECPRMUnitDistanceLess(U2DPoint<long> inCenter);
    long DistanceSquared(const EETileCoordinate& inTile) const;   // 0x8a3f (ICF)     // 0x160fba (ICF)
    bool operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const;

    EETileCoordinate mCenter;
};

static bool EECPRMFishes(EECPResourceManager* inManager);        // economy.coast_fishing (below GetGatherCapacity)
static void EECPRMRequestBoats(EECPResourceManager* inManager);
static unsigned long EECPRMHunters(EECPResourceManager* inManager);
static const unsigned long kHuntFirst = 6;   // the experiment "hunt6": hunters before the patches

// The units within a distance of a tile (FindFish's find_if: COMDATs 0x921d0/0x921f7) (provisional name)
struct EECPRMUnitWithin : public EETileDistanceLess {
    EECPRMUnitWithin(const EETileCoordinate& inCenter, float inDistance) : EETileDistanceLess(inCenter), mDistance(inDistance) {}
    bool operator()(EEUnit* inUnit) const
    {
        EETileDistanceLess theTile(inUnit->mTileX, inUnit->mTileY);
        return (float)theTile.DistanceSquared(this->mCenter) <= this->mDistance;
    }
    float mDistance;
};

// FUNCTION: EXE 0x48e745
// Orders two forests by the squared distance of their closest hull point from the center tile; false if
// either forest is gone
// Built without /Og (rule 4 evidence: memory-resident locals, `leave`); exact with the pragma, the hash-fitted
// local names (r37_cp), EEAIContinentManager::GetForest and the undone-inline DistanceSquared below (r37_load)
#pragma optimize("g", off)
bool EECPRMForestDistanceLess::operator()(unsigned short inFirst, unsigned short inSecond) const
{
    U2DPoint<long> theFirstPoint;
    U2DPoint<long> theSecondPoint;
    long theLeft;
    long theRight;
    bool theResult = false;
    EEAIForest* theForest = gContinentManager.GetForest(inFirst);
    if (theForest != NULL) {
        theFirstPoint = theForest->GetClosestHullPoint(this->mCenter);
        theForest = gContinentManager.GetForest(inSecond);
        if (theForest != NULL) {
            theSecondPoint = theForest->GetClosestHullPoint(this->mCenter);
            theLeft = this->DistanceSquared(theFirstPoint);
            theRight = this->DistanceSquared(theSecondPoint);
            theResult = theLeft < theRight;
        }
    }
    return theResult;
}
#pragma optimize("g", on)

// FUNCTION: EXE 0x48e85b
// Orders two units by the squared distance of their tiles from the center tile, nearer first
// Built without /Og (rule 4 evidence: memory-resident locals, `leave`); exact with the pragma, the hash-fitted
// local names and the undone-inline DistanceSquared below (r37_load; the r37_cp lane found the operator() shape)
#pragma optimize("g", off)
bool EECPRMUnitDistanceLess::operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const
{
    U2DPoint<long> theFirstPoint(inFirst->GetTileX(), inFirst->GetTileY());
    U2DPoint<long> theSecondPoint(inSecond->GetTileX(), inSecond->GetTileY());
    long theLeft = this->DistanceSquared(theFirstPoint);
    long theRight = this->DistanceSquared(theSecondPoint);
    bool theResult = theLeft < theRight;
    return theResult;
}
#pragma optimize("g", on)

// FUNCTION: EXE 0x48e8f1
// Creates the resource manager of a civilization manager, with no needs, no wanted gatherers and no timers set
EECPResourceManager::EECPResourceManager(EECPCivilizationManager* inManager)
    : mManager(inManager)
{
    for (long i = 0; i < 7; i++) {
        this->mNeeds[i] = 0;
        this->mWantedGatherers[i] = 0;
    }
    for (long j = 0; j < 7; j++)
        this->mB180[j] = false;
    this->mSettlementsEnabled = false;
    this->mInitialized = false;
    this->mNextFishingBoatTime = 0;
    this->mTime170 = 0;
    this->mNextWantedGatherersTime = 0;
    this->mNextAssignGatherersTime = 0;
    this->mNextResourceCheckTime = 0;
}

// SYNTHETIC: EXE 0x48e9cc
// (EECPResourceManager's scalar deleting destructor)

// FUNCTION: EXE 0x48e9e8
// Destroys the manager, releasing every gatherer, mine and fish it holds
EECPResourceManager::~EECPResourceManager()
{
    for (long i = 0; i < 7; i++) {
        this->mDropSiteClasses[i].clear();
        while (this->mGatherers[i].size() > 0) {
            this->mGatherers[i].back()->Dereference();
            this->mGatherers[i].pop_back();
        }
    }
    this->mForests.clear();
    unsigned long j;
    for (j = 0; j < this->mMines.size(); j++)
        this->mMines[j]->Dereference();
    this->mMines.clear();
    unsigned long k;
    for (k = 0; k < this->mFish.size(); k++)
        this->mFish[k]->Dereference();
    this->mFish.clear();
}

// FUNCTION: EXE 0x48eb29
// Drops the units that are not idle, then sends each idle one to gold, else iron, else stone, else wood
void EECPResourceManager::AssignIdleGatherers(std::deque<EEComplexUnit*>& ioUnits)
{
    std::deque<EEComplexUnit*>::iterator it = ioUnits.begin();
    while (it != ioUnits.end()) {
        EEUnitAction* theCommand = (*it)->mCommand;
        if (theCommand != NULL && theCommand->mType == kActionIdle)
            ++it;
        else
            it = ioUnits.erase(it, it + 1);
    }
    // new: the most under-served resource first (wanted minus listed; the original's fixed order, gold, iron, stone,
    // wood, broke ties), so leftover citizens no longer pile up on gold and iron while wood is short
    for (it = ioUnits.begin(); it != ioUnits.end(); ++it) {
        long theOrder[4] = {5, 6, 4, 3};
        for (long i = 1; i < 4; i++)
            for (long k = i; k > 0; k--) {
                long a = theOrder[k - 1], b = theOrder[k];
                long theA = (long)this->mWantedGatherers[a] - (long)this->mGatherers[a].size();
                long theB = (long)this->mWantedGatherers[b] - (long)this->mGatherers[b].size();
                if (theB > theA) {
                    theOrder[k - 1] = b;
                    theOrder[k] = a;
                }
            }
        EEUnit* theResource = NULL;
        for (long i = 0; i < 4 && theResource == NULL; i++)
            theResource = theOrder[i] == 3 ? this->FindForestTarget(1, *it) : this->FindResourceTarget(theOrder[i], *it, false);
        if (theResource != NULL)
            this->SendToGather(*it, theResource, NULL);
    }
}

// FUNCTION: EXE 0x48ec34
// Drops the boats that are not idle, then sends each idle one to fish near its nearest dock
void EECPResourceManager::AssignIdleBoats(std::deque<EEComplexUnit*>& ioUnits)
{
    std::deque<EEComplexUnit*>::iterator it = ioUnits.begin();
    while (it != ioUnits.end()) {
        EEUnitAction* theCommand = (*it)->mCommand;
        if (theCommand != NULL && theCommand->mType == kActionIdle)
            ++it;
        else
            it = ioUnits.erase(it, it + 1);
    }
    for (it = ioUnits.begin(); it != ioUnits.end(); ++it) {
        EEUnit* theResource = this->FindNearestDock(*it);
        if (theResource == NULL)
            continue;
        theResource = this->FindFish(theResource);
        if (theResource != NULL)
            this->SendToGather(*it, theResource, NULL);
    }
}

// FUNCTION: EXE 0x48ecfd
// Takes a transport from the combat personnel to ferry stranded gatherers to a resource; without one, requests a
// transport when fewer than 4 exist and none is requested or in production
void EECPResourceManager::RequestFerry(std::vector<EEComplexUnit*>& inUnits, EEUnit* inResource)
{
    std::vector<long> theClasses;
    std::vector<long> theIDs;
    EEComputerPlayer* thePlayer = this->GetManager()->GetPlayer();
    EECPUnitManager* theUnitManager = this->GetManager()->GetUnitManager();
    EECPCombatManager* theCombatManager = thePlayer->GetCombatManager();
    long theClass = this->GetManager()->ChooseTransportClass();
    bool theFailed = true;
    if (theClass == 0)
        return;
    std::deque<EEComplexUnit*>* theUnits = theUnitManager->GetUnits(theClass);
    theClasses.push_back(theClass);
    EECPPersonnelRequest* theRequest = theCombatManager->GetPersonnel()->NewContinentRequest(theClasses, theIDs, 0);
    if (theRequest->GetNumFilled() == 1) {
        EEComplexUnit* theUnit = theRequest->mUnits.back().mUnit;
        if (theUnit->GetAIState() != 0x46cd7)
            theUnit->mAIState = 0x46cd7;
        theRequest->PopSlot();
        thePlayer->DetachUnitFromAI(theUnit);
        theFailed = false;
        if (this->FerryGatherers(inUnits, theUnit, inResource))
            thePlayer->SetNextIslandExpansionTime();
        else {
            if (theUnit->GetIsDetachedFromAI())
                theUnit->mIsDetachedFromAI = false;
            theCombatManager->UnitAdded(theUnit);
        }
    } else
        theRequest->Cancel();
    theCombatManager->GetPersonnel()->ReleaseRequest(theRequest->GetID());
    theRequest->Dereference();
    if (theFailed && (theUnits == NULL || theUnits->size() < 4) && !theUnitManager->IsRequested(theClass)
        && !theUnitManager->IsInProduction(theClass))
        theUnitManager->RequestUnits(theClass, 1, 0, 0);
}

// FUNCTION: EXE 0x48ee67
// Adds a unit to a resource's gatherer list (referencing it) unless it is already there
void EECPResourceManager::AddGatherer(EEComplexUnit* inUnit, long inResource)
{
    std::deque<EEComplexUnit*>::iterator theIt =
        std::find(this->mGatherers[inResource].begin(), this->mGatherers[inResource].end(), inUnit);
    if (theIt == this->mGatherers[inResource].end()) {
        this->mGatherers[inResource].push_back(inUnit);
        inUnit->Reference();
    }
}

// whether a resource unit takes one more gatherer: the original's test (its users working there below the mining limit);
// the next engine also counts the gatherers sent there and holds a forage patch to kNextPatchPlaces in all
// (EECPNextHasRoom; docs/AI_RESEARCH.md §8.35: 9 were sent to one patch, waiting)
static bool EECPRMHasRoom(EEComputerPlayer* inPlayer, EEUnit* inResource, unsigned long inLimit)
{
    if (EECPNextEngine(inPlayer))
        return EECPNextHasRoom(inPlayer, inResource);
    return gWorld.GetResourceUsers(inResource) < inLimit;
}

// the next engine: a unit of a resource centre's resource with room for one more gatherer (the original takes the
// first unit with some left, so a centre whose first mine or bush is full was passed over)
static EEUnit* EECPRMUnitWithRoom(EEComputerPlayer* inPlayer, EECPResourceCenter* inCenter, unsigned long inResource)
{
    for (unsigned long i = 0; i < inCenter->mUnits.size(); i++) {
        EEUnit* theUnit = inCenter->mUnits[i];
        if (theUnit->mResourceType == inResource && theUnit->mResourceAmount > 0 && EECPNextHasRoom(inPlayer, theUnit))
            return theUnit;
    }
    return NULL;
}

// the next engine (§8.35): the gatherers beyond what a patch (kNextPatchPlaces) or a mine unit (the mining limit)
// holds are taken off it (hunters whose game ran out walk to the nearest bushes; the game moves gatherers to the next
// bush of a patch). They go back to the idle list, and AssignGatherers places them where there is room.
static void EECPRMShedCrowds(EECPResourceManager* inManager)
{
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    long theLimit = thePlayer->mMiningLimit > 0 ? (long)thePlayer->mMiningLimit : 1;
    std::map<EEUnit*, long> theOnUnit;
    std::map<EECPResourceCenter*, long> theOnPatch;
    std::map<EEUnit*, EECPResourceCenter*> thePatchOf;
    for (unsigned long k = 0; k < thePlayer->GetNumResourceCenters(); k++) {
        EECPResourceCenter* theCenter = thePlayer->GetResourceCenter(k);
        for (unsigned long u = 0; u < theCenter->mUnits.size(); u++)
            if (theCenter->mUnits[u]->mUnitClass->mUnitType == kUnitTypeResForageBush)
                thePatchOf[theCenter->mUnits[u]] = theCenter;
    }
    std::vector<EEComplexUnit*> theShed;
    for (long r = 2; r < 7; r++)
        for (unsigned long k = 0; k < inManager->mGatherers[r].size(); k++) {
            EEComplexUnit* theUnit = inManager->mGatherers[r][k];
            if (theUnit->GetUnitClass()->mCategory != kUnitCategoryLandCitizen || theUnit->mIsDisabled
                || theUnit->mIsDetachedFromAI || theUnit->mCitizenBuildQueue.size() != 0)
                continue;
            EEUGGather* theGather = dynamic_cast<EEUGGather*>(theUnit->GetLoadGoal());
            if (theGather == NULL || theGather->mResource == NULL || theGather->mResource->mUnitClass->mIsTree
                || theGather->mResource->mUnitClass->mFamily == kUnitFamilyAnimal
                || theGather->mResource->mUnitClass->mFamily == kUnitFamilyFish)
                continue;
            EEUnit* theResource = theGather->mResource;
            bool theOver = ++theOnUnit[theResource] > theLimit;
            std::map<EEUnit*, EECPResourceCenter*>::iterator thePatch = thePatchOf.find(theResource);
            if (thePatch != thePatchOf.end() && ++theOnPatch[thePatch->second] > kNextPatchPlaces)
                theOver = true;
            if (theOver)
                theShed.push_back(theUnit);
        }
    for (unsigned long k = 0; k < theShed.size(); k++) {
        EEComplexUnit* theUnit = theShed[k];
        theUnit->Reference();
        inManager->AddIdleGatherer(theUnit);
        theUnit->ClearGoalQueue(0);
        theUnit->StartAction(kActionIdle);
        theUnit->GetUnitClass()->ActionChanged(theUnit);
        theUnit->Dereference();
    }
    if (!theShed.empty())
        EE_CP_TRACE(thePlayer, "gather.shed", "n=%lu", (unsigned long)theShed.size());
}

// NEAR: block order and register choice (the resource loop: the original tests 2, 3, then >3 and lays the bodies out in reverse)
// FUNCTION: EXE 0x48eecf
// Sends idle citizens and boats to food, wood, stone, gold and iron up to each resource's wanted gatherer
// count, requesting fishing boats and ferries as needed; the rest go to AssignIdleGatherers/AssignIdleBoats
void EECPResourceManager::AssignGatherers()
{
    std::vector<EEComplexUnit*> theFound;
    std::vector<EEComplexUnit*> theMoved;
    std::vector<EEComplexUnit*> theStranded;
    std::deque<EEComplexUnit*> theIdle;
    std::deque<EEComplexUnit*> theBoats;
    std::deque<EEComplexUnit*>::iterator it;
    EEComplexUnit* theUnit;
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    unsigned long theLimit = thePlayer->mMiningLimit;
    unsigned short theTownContinent = 0;
    bool theAcross;
    EEMap* theMap = gWorld.GetMap();
    if (theMap != NULL && ((theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (theMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap)) {
        U2DPoint<long> theTown = this->mManager->mBuildManager->mTownCenterTile;
        theAcross = true;
        if (theTown.GetX() != -10000)
            theTownContinent = gContinentManager.GetContinentID(theTown.GetX(), theTown.GetY());
    } else
        theAcross = false;
    bool theNext = EECPNextEngine(thePlayer);   // the next engine: no boat requests (fish), the food order of gather
    this->RebalanceGatherers();   // new
    if (theNext)
        EECPRMShedCrowds(this);   // the next engine: no more than a patch or mine holds (§8.35)

    for (it = this->mGatherers[0].begin(); it != this->mGatherers[0].end(); ++it) {
        theUnit = *it;
        EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(theUnit->GetLoadGoal());
        EEUGGather* theGather = dynamic_cast<EEUGGather*>(theUnit->GetLoadGoal());
        if ((theGather != NULL && theGather->mResource != NULL)
            || (theAttack != NULL && theAttack->mTarget->GetUnitClass()->GetFamily() == kUnitFamilyAnimal))
            theMoved.push_back(theUnit);
    }
    unsigned long i;
    for (i = 0; i < theMoved.size(); i++) {
        this->RemoveGatherer(theMoved[i]);
        this->AddGatherer(theMoved[i], theMoved[i]->GetResourceType());
    }

    unsigned long j;
    for (j = 0; j < 7; j++) {
        for (it = this->mGatherers[j].begin(); it != this->mGatherers[j].end(); ++it) {
            theUnit = *it;
            EEUnitGoal* theGoal = theUnit->GetLoadGoal();
            EEUGGather* theGather = dynamic_cast<EEUGGather*>(theGoal);
            if (theUnit->GetIsDisabled() || theUnit->mIsDetachedFromAI)
                continue;
            if (theUnit->GetUnitClass()->GetCategory() == kUnitCategoryNavalCitizen) {
                if (j == 0)
                    theBoats.push_back(theUnit);
            } else if (theUnit->mCitizenBuildQueue.size() == 0 && theUnit->GetHitPoints() > 0 && theUnit->GetTileX() != -10000
                       && theUnit->GetTileY() != -10000
                       && (theGoal == NULL || (theGather != NULL && theGather->mWaiting && theUnit->mCurrentPhysicsAction != 0)))
                theIdle.push_back(theUnit);
        }
    }

    if (theIdle.size() > 0) {
        unsigned long theResource;
        for (theResource = 2; theResource < 7; theResource++) {
            if (this->mWantedGatherers[theResource] <= this->mGatherers[theResource].size())
                continue;
            unsigned long theWanted = this->mWantedGatherers[theResource] - this->mGatherers[theResource].size();
            if (theResource == 2 && theWanted > 0 && !theNext && EECPRMFishes(this) && theBoats.size() < 8
                && gPhysics->mGameTime >= this->mNextFishingBoatTime) {
                EECPRMRequestBoats(this);
                this->mNextFishingBoatTime = gPhysics->mGameTime + 30000;
            }
            if (theWanted >= theIdle.size() + theBoats.size())
                theWanted = theIdle.size() + theBoats.size();
            if (theWanted > 0 || (thePlayer->IsIslandExpansionDue() && (theResource == 4 || theResource == 5 || theResource == 6))) {
                switch (theResource) {
                case 2: {
                    // the experiment "hunt6" (docs/AI_RESEARCH.md §8.29; the owner hunts with 6 beside the patches, the AI
                    // with 1-4): the game near home before the patches while fewer than kHuntFirst hunt
                    // (the next engine's gather(food, n, hunt): all n hunt first)
                    bool theHuntFirst = theNext ? EECPNextFoodPreference(thePlayer) == kNextPreferHunt
                        : EECPExperiment(thePlayer, "hunt6");
                    if (theHuntFirst) {
                        unsigned long theHunters = EECPRMHunters(this);
                        unsigned long theFirst = theNext ? this->mWantedGatherers[2] : kHuntFirst;
                        if (theHunters < theFirst) {
                            unsigned long theMore = theFirst - theHunters;
                            if (theMore > theWanted)
                                theMore = theWanted;
                            if (theMore > theIdle.size())
                                theMore = theIdle.size();
                            this->FindMines(theMore, true, theFound);
                            for (unsigned long h = 0; h < theFound.size() && theIdle.size() > 0 && theWanted > 0; h++) {
                                theUnit = theIdle.front();
                                theIdle.pop_front();
                                this->SendToGather(theUnit, theFound[h], NULL);
                                theWanted--;
                            }
                        }
                    }
                    EEUnit* theTarget = this->FindFarm(true, true, theIdle.size() > 0 ? theIdle.front() : NULL);
                    if (theTarget != NULL) {
                        unsigned long theCount = theWanted < theIdle.size() ? theWanted : theIdle.size();
                        while (theCount > 0) {
                            if (EECPRMHasRoom(thePlayer, theTarget, theLimit)) {
                                theUnit = theIdle.front();
                                theIdle.pop_front();
                                this->SendToGather(theUnit, theTarget, NULL);
                                theWanted--;
                                theCount--;
                            } else {
                                theTarget = this->FindFarm(true, true, theIdle.size() > 0 ? theIdle.front() : NULL);
                                if (theTarget == NULL)
                                    break;
                            }
                        }
                    }
                    if (theWanted > 0) {
                        this->GetIdleFarms(theWanted < theIdle.size() ? theWanted : theIdle.size(), theFound);
                        unsigned long k;
                        for (k = 0; k < theFound.size(); k++) {
                            theUnit = theIdle.front();
                            theIdle.pop_front();
                            this->SendToGather(theUnit, theFound[k], NULL);
                            theWanted--;
                        }
                        if (theWanted > 0) {
                            this->FindMines(theWanted < theIdle.size() ? theWanted : theIdle.size(), true, theFound);
                            unsigned long theFoundCount;
                            for (k = 0, theFoundCount = theFound.size(); k < theFoundCount; k++) {
                                theUnit = theIdle.front();
                                EETileCoordinate theUnitTile;
                                theUnitTile.mX = theUnit->GetTileX();
                                theUnitTile.mY = theUnit->GetTileY();
                                EETileCoordinate theFoundTile;
                                theFoundTile.mX = theFound[k]->GetTileX();
                                theFoundTile.mY = theFound[k]->GetTileY();
                                unsigned short theFoundContinent = gContinentManager.GetContinent(theFoundTile, kAIPathLandMass);
                                if (gContinentManager.GetContinent(theUnitTile, kAIPathLandMass) != theFoundContinent)
                                    continue;
                                theIdle.pop_front();
                                this->SendToGather(theUnit, theFound[k], NULL);
                                theWanted--;
                            }
                            if (theWanted > 0) {
                                theTarget = this->FindFarm(false, true, theIdle.size() > 0 ? theIdle.front() : NULL);
                                if (theTarget != NULL) {
                                    unsigned long theCount = theWanted < theIdle.size() ? theWanted : theIdle.size();
                                    while (theCount > 0) {
                                        if (EECPRMHasRoom(thePlayer, theTarget, theLimit)) {
                                            theUnit = theIdle.front();
                                            theIdle.pop_front();
                                            this->SendToGather(theUnit, theTarget, NULL);
                                            theWanted--;
                                            theCount--;
                                        } else {
                                            theTarget = this->FindFarm(false, true, theIdle.size() > 0 ? theIdle.front() : NULL);
                                            if (theTarget == NULL)
                                                break;
                                        }
                                    }
                                }
                                if (theWanted > 0) {
                                    theTarget = this->FindFarm(false, false, theIdle.size() > 0 ? theIdle.front() : NULL);
                                    if (theTarget != NULL) {
                                        unsigned long theCount = theWanted < theIdle.size() ? theWanted : theIdle.size();
                                        while (theCount > 0) {
                                            if (EECPRMHasRoom(thePlayer, theTarget, theLimit)) {
                                                theUnit = theIdle.front();
                                                theIdle.pop_front();
                                                this->SendToGather(theUnit, theTarget, NULL);
                                                theWanted--;
                                                theCount--;
                                            } else {
                                                theTarget = this->FindFarm(false, false, theIdle.size() > 0 ? theIdle.front() : NULL);
                                                if (theTarget == NULL)
                                                    break;
                                            }
                                        }
                                    }
                                    if (theWanted > 0) {
                                        this->FindMines(theWanted < theIdle.size() ? theWanted : theIdle.size(), false, theFound);
                                        for (k = 0, theFoundCount = theFound.size(); k < theFoundCount; k++) {
                                            theUnit = theIdle.front();
                                            EETileCoordinate theUnitTile;
                                            theUnitTile.mX = theUnit->GetTileX();
                                            theUnitTile.mY = theUnit->GetTileY();
                                            EETileCoordinate theFoundTile;
                                            theFoundTile.mX = theFound[k]->GetTileX();
                                            theFoundTile.mY = theFound[k]->GetTileY();
                                            unsigned short theFoundContinent = gContinentManager.GetContinent(theFoundTile, kAIPathLandMass);
                                            if (gContinentManager.GetContinent(theUnitTile, kAIPathLandMass) == theFoundContinent) {
                                                theIdle.pop_front();
                                                this->SendToGather(theUnit, theFound[k], NULL);
                                                theWanted--;
                                            }
                                        }
                                        if (theWanted > 0) {
                                            if (!thePlayer->mUsesFishing && thePlayer->CanProduceUnitType(kUnitTypeFarm, 0x20, NULL)) {
                                                this->GetUnfinishedFarms(theWanted < theIdle.size() ? theWanted : theIdle.size(), theFound);
                                                for (k = 0; k < theFound.size(); k++) {
                                                    theUnit = theIdle.front();
                                                    theIdle.pop_front();
                                                    theUnit->SetGoal(new EEUGBuildRepair(thePlayer, theFound[k], true), 0, 0);
                                                    theUnit->StartAction(kActionMoveToRepairSite);
                                                    theUnit->GetUnitClass()->ActionChanged(theUnit);
                                                    theWanted--;
                                                }
                                            }
                                            if (theWanted > 0 && EECPRMFishes(this)) {
                                                if (this->mWantedGatherers[theResource] - this->mGatherers[theResource].size() > theBoats.size()
                                                    && !theNext && theBoats.size() < 8 && gPhysics->mGameTime >= this->mNextFishingBoatTime) {
                                                    EECPRMRequestBoats(this);
                                                    this->mNextFishingBoatTime = gPhysics->mGameTime + 30000;
                                                }
                                                unsigned long theBoatCount = theBoats.size();
                                                for (k = 0; k < theBoatCount; k++) {
                                                    theUnit = theBoats.front();
                                                    EEUnit* theShore = this->FindNearestDock(theUnit);
                                                    if (theShore == NULL)
                                                        break;
                                                    EEUnit* theFish = this->FindFish(theShore);
                                                    if (theFish == NULL)
                                                        break;
                                                    theBoats.pop_front();
                                                    this->SendToGather(theUnit, theFish, NULL);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                case 3: {
                    EEUnit* theTarget = this->FindForestTarget(theWanted, theIdle.size() > 0 ? theIdle.front() : NULL);
                    if (theTarget != NULL) {
                        unsigned long theCount = theWanted < theIdle.size() ? theWanted : theIdle.size();
                        while (theCount > 0 && theIdle.size() > 0) {
                            theUnit = theIdle.front();
                            theIdle.pop_front();
                            this->SendToGather(theUnit, theTarget, NULL);
                            theCount--;
                        }
                    }
                    break;
                }
                default:
                    if (theResource > 3 && theResource < 7) {
                        EEUnit* theTarget = this->FindResourceTarget(theResource, theIdle.size() > 0 ? theIdle.front() : NULL, true);
                        if (theTarget != NULL) {
                            unsigned long theCount = theWanted < theIdle.size() ? theWanted : theIdle.size();
                            while (theCount > 0) {
                                if (EECPRMHasRoom(thePlayer, theTarget, theLimit)) {
                                    theUnit = theIdle.front();
                                    theIdle.pop_front();
                                    this->SendToGather(theUnit, theTarget, NULL);
                                    theWanted--;
                                    theCount--;
                                } else {
                                    theTarget = this->FindResourceTarget(theResource, theIdle.size() > 0 ? theIdle.front() : NULL, true);
                                    if (theTarget == NULL)
                                        break;
                                }
                            }
                        }
                        if ((theWanted > 0 || thePlayer->IsIslandExpansionDue()) && theAcross) {
                            EEUnit* theFar = this->FindResourceTarget(theResource, NULL, true);
                            if (theFar != NULL) {
                                while (theIdle.size() > 0) {
                                    theUnit = theIdle.front();
                                    theIdle.pop_front();
                                    if (gContinentManager.AreConnectedFromIDs(theTownContinent, gContinentManager.GetContinentID(theUnit),
                                            this->mManager->mPlayer->GetCitizenPassability())
                                        && !gContinentManager.AreConnected(gContinentManager.GetContinentAt(theFar),
                                            gContinentManager.GetContinentAt(theUnit), this->mManager->mPlayer->GetCitizenPassability()))
                                        theStranded.push_back(theUnit);
                                }
                                if (theStranded.size() != 0)
                                    this->RequestFerry(theStranded, theFar);
                            }
                        }
                    }
                    break;
                }
            }
        }
        if (theIdle.size() > 0)
            this->AssignIdleGatherers(theIdle);
    }
    if (theBoats.size() > 0)
        this->AssignIdleBoats(theBoats);
}

// FUNCTION: EXE 0x48fca6
// Removes a lost citizen from the gatherer lists and, unless it was garrisoned, adds a citizen's cost to the needs
void EECPResourceManager::CitizenLost(EEComplexUnit* inUnit)
{
    EETechTree* theTechTree = this->mManager->mPlayer->mTechnologies;
    EETechTreeNode* theNode = theTechTree->Get(gUnitClassTable[kUnitTypeCitizen] ? gUnitClassTable[kUnitTypeCitizen]->mTechnology : 100000);
    this->RemoveGatherer(inUnit);
    if (dynamic_cast<EEUGLoadGarrison*>(inUnit->GetLoadGoal()) == NULL)
        this->AddCosts(theNode, 1);
}

// FUNCTION: EXE 0x48fd15
// Moves a unit to the idle gatherer list unless it has buildings queued
void EECPResourceManager::AddIdleGatherer(EEComplexUnit* inUnit)
{
    this->RemoveGatherer(inUnit);
    if (inUnit->mCitizenBuildQueue.size() == 0)
        this->AddGatherer(inUnit, 0);
}

// the experiment "foodeco": the settlement site nearest a forage patch, in the rings round it (BuildSettlementFor)
static const long kPatchDropRange = kEECPPatchDropRange;

// economy.forage_patches: whether the player has a drop site for food (town centre, capitol, settlement, granary;
// built or going up) within inRange tiles of a tile (measured to the drop site's centre tile, so a large town centre
// counts from its middle)
static bool EECPRMDropNear(EEComputerPlayer* inPlayer, long inX, long inY, long inRange, bool inFood)
{
    static const long kTypes[4] = {kUnitTypeTownCenter, kUnitTypeCapitol, kUnitTypeSettlement, kUnitTypeGranary};
    for (int t = 0; t < (inFood ? 4 : 3); t++) {
        std::vector<EEUnit*> theUnits;
        EEGetPlayerUnitsOfType(kTypes[t], inPlayer->mPlayerID, theUnits);
        for (unsigned long k = 0; k < theUnits.size(); k++) {
            if (!(theUnits[k]->mHitPoints > 0))
                continue;
            long theHalf = theUnits[k]->mUnitClass->GetTileSize() / 2;
            long theDX = theUnits[k]->GetTileX() + theHalf - inX, theDY = theUnits[k]->GetTileY() + theHalf - inY;
            long theReach = inRange + theHalf;
            if (theDX * theDX + theDY * theDY <= theReach * theReach)
                return true;
        }
    }
    return false;
}

static bool EECPRMFoodDropNear(EEComputerPlayer* inPlayer, long inX, long inY, long inRange)
{
    return EECPRMDropNear(inPlayer, inX, inY, inRange, true);
}

static bool EECPRMInClaimRange(EECPResourceManager* inManager, long inX, long inY)
{
    EECPBuildManager* theBuild = inManager->mManager->mBuildManager;
    if (theBuild == NULL || theBuild->mTownCenterTile.mX == -10000)
        return true;
    long theDX = inX - theBuild->mTownCenterTile.mX, theDY = inY - theBuild->mTownCenterTile.mY;
    long theRange = EECPClaimRange(inManager->mManager->mPlayer);
    return theDX * theDX + theDY * theDY <= theRange * theRange;
}

static bool EECPRMPatchSite(EECPResourceManager* inManager, EEUnit* inPatch, EETileCoordinate& outTile)
{
    EECPBuildManager* theBuild = inManager->mManager->mBuildManager;
    EEUnitClass* theClass = gUnitClassTable[kUnitTypeSettlement];
    if (theBuild == NULL || theClass == NULL)
        return false;
    long theX = inPatch->GetTileX(), theY = inPatch->GetTileY();
    unsigned short theContinent = gContinentManager.GetContinent(U2DPoint<long>(theX, theY), kAIPathLandMass);
    std::vector<EETileCoordinate> theTiles;
    for (long theRing = 2; theRing <= kPatchDropRange + 2; theRing++) {
        EEGetSquareRingTiles(theX, theY, theRing, theTiles);
        for (unsigned long k = 0; k < theTiles.size(); k++)
            if (theBuild->CanBuildAt(theContinent, kUnitTypeSettlement, theTiles[k], theClass->mTileSize, true, true, theX, theY)) {
                outTile = theTiles[k];
                return true;
            }
    }
    return false;
}

// FUNCTION: EXE 0x48fd38
// Orders a gatherer far from its drop site to build a settlement near its resource when allowed and no other
// settlement or town center is queued or near; returns true if it did
bool EECPResourceManager::BuildSettlementFor(EEComplexUnit* inUnit)
{
    U2DPoint<float> theSpot(0.0f, 0.0f);
    EETileDistanceLess theSite(0, 0);
    EEUGGather* theGoal = dynamic_cast<EEUGGather*>(inUnit->GetLoadGoal());
    EEUCBasicCitizen* theClass = dynamic_cast<EEUCBasicCitizen*>(inUnit->GetUnitClass());
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    long theRadius = thePlayer->mDropOffDistance;
    long theRange = theRadius * theRadius;
    bool theResult = false;
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    if (theGroup != NULL && theGroup->GetNumUnits() > 0)
        theGroup->Clear();
    // (the next engine's strategy builds its settlements: build(settlement, site))
    if (thePlayer->mCanBuild && this->mSettlementsEnabled && !EECPNextEngine(thePlayer)
        && thePlayer->CanProduceUnitType(kUnitTypeSettlement, 0x20, NULL)) {
        EEUnit* theResource = theGoal->mResource;
        EEUnit* theStorage = theGoal->mStorage;
        // the experiment "foodeco" (docs/AI_RESEARCH.md §8.27; the owner builds on the patch "so citizens move as little
        // as possible"): a forage patch with no drop site within kPatchDropRange tiles gets a settlement in the rings
        // round the patch itself (the original waited for the full drop-off distance and searched round the centre)
        bool thePatch = theResource != NULL && EECPForagePatchesOn(thePlayer) && theResource->mResourceType == kResourceFood
            && theResource->GetUnitClass()->mFamily != kUnitFamilyAnimal && theResource->GetUnitClass()->mFamily != kUnitFamilyFish
            && theResource->GetUnitClass()->GetUnitType() != EEGetFarmType(thePlayer);
        if (thePatch)
            theRange = kPatchDropRange * kPatchDropRange;
        if (theResource != NULL
            && (theStorage == NULL || static_cast<EEComplexUnit*>(theStorage)->DistanceSquaredToUnit(theResource) > (float)theRange)) {
            theResult = theGoal->FindUnfinishedFarm(inUnit) == NULL;
            if (theResult) {
                EEUnit* theNearest = theClass->FindStorage(inUnit, true);
                if (theNearest != NULL
                    && static_cast<EEComplexUnit*>(theNearest)->DistanceSquaredToUnit(theResource) <= (float)theRange) {
                    if (inUnit->GetUnitClass()->mCategory != kUnitCategoryLandCitizen)
                        return false;
                    long theFlags = this->mManager->mPlayer->GetCitizenPassability();
                    unsigned short theFirst = gContinentManager.GetContinentID(theNearest);
                    unsigned short theSecond = gContinentManager.GetContinentID(inUnit);
                    if (gContinentManager.AreConnected(gContinentManager.GetContinentByID(theSecond),
                                                  gContinentManager.mContinents[theFirst], theFlags))
                        return false;
                }
                theResult = !this->IsSettlementQueued(theResource->mResourceType);
                if (theResult) {
                    if (theResource->mResourceType == kResourceWood) {
                        EEAIForest* theForest = gContinentManager.GetForestAt(theResource);
                        if (theForest == NULL || theForest->mTiles.Size() < 5)
                            theResult = false;
                    }
                    if (theResult) {
                        long theKind = theResource->GetUnitClass()->mFamily;
                        theResult = theKind != kUnitFamilyAnimal;
                        if (theResult) {
                            if (theResource->mResourceType != kResourceWood && theKind != kUnitFamilyAnimal) {
                                EECPResourceCenter* theCenter = thePlayer->GetResourceCenterOf(theResource);
                                if (thePatch) {
                                    if (!EECPRMPatchSite(this, theResource, theSite.mCenter))
                                        return false;
                                    theRange = theRadius * theRadius;   // the site check below: within the drop-off distance
                                } else if (theCenter == NULL || !theCenter->FindBuildSite(kUnitTypeSettlement, theSite.mCenter))
                                    return false;
                                EETileCoordinate theTile;
                                theTile.mX = theResource->mTileX;
                                theTile.mY = theResource->mTileY;
                                theResult = theSite.DistanceSquared(theTile) <= theRange;
                            } else {
                                theResult = theClass->FindWoodSite(inUnit, 0x2b, theSpot);
                                if (!theResult)
                                    return theResult;
                                long theY = UMFloor(theSpot.GetY());
                                long theX = UMFloor(theSpot.mX);
                                theSite.mCenter.mX = theX;
                                theSite.mCenter.mY = theY;
                                EETileCoordinate theTile;
                                theTile.mX = theResource->mTileX;
                                theTile.mY = theResource->mTileY;
                                theResult = theSite.DistanceSquared(theTile) <= theRange;
                            }
                            if (theResult) {
                                theResult = !this->mManager->mBuildManager->HasTownCenterNear(theSite.mCenter.mX, theSite.mCenter.GetY(),
                                                                               (float)theRadius * 0.75f);
                                if (theResult) {
                                    EEUnit* theTarget = EEMakeBuilding(thePlayer, 0x2b, theSite.mCenter.mX, theSite.mCenter.GetY());
                                    inUnit->SetGoal(new EEUGBuildRepair(thePlayer, theTarget, false), 1, 0);
                                    inUnit->StartAction(kActionMoveToRepairSite);
                                    EE_CP_TRACE(thePlayer, "settlement.order", "resource=%lu x=%ld y=%ld", (unsigned long)theResource->mResourceType, (long)theSite.mCenter.mX, (long)theSite.mCenter.GetY());
                                    inUnit->GetUnitClass()->ActionChanged(inUnit);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x49007f
// Removes a unit from every resource's gatherer list, dropping each reference
void EECPResourceManager::RemoveGatherer(EEComplexUnit* inUnit)
{
    for (long i = 0; i < 7; i++) {
        std::deque<EEComplexUnit*>::iterator it = std::find(this->mGatherers[i].begin(), this->mGatherers[i].end(), inUnit);
        if (it != this->mGatherers[i].end()) {
            this->mGatherers[i].erase(it, it + 1);
            inUnit->Dereference();
        }
    }
}

// FUNCTION: EXE 0x490105
// Lowers the need for a resource by an amount (no change for 0)
void EECPResourceManager::Spend(long inResource, unsigned long inAmount)
{
    if (inAmount > 0)
        this->mNeeds[inResource] -= inAmount;
}

// FUNCTION: EXE 0x490122
// Moves every idle citizen with no buildings queued back to the idle gatherer list
void EECPResourceManager::RequeueIdleGatherers()
{
    std::vector<EEComplexUnit*> theUnits;
    this->mManager->mUnitManager->GetCitizens(theUnits);
    for (std::vector<EEComplexUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
        EEComplexUnit* theUnit = *it;
        EEUnitAction* theCommand = theUnit->mCommand;
        if (theCommand != NULL && theCommand->mType == kActionIdle && theUnit->mCitizenBuildQueue.size() == 0) {
            this->RemoveGatherer(theUnit);
            this->AddGatherer(*it, 0);
        }
    }
}

// FUNCTION: EXE 0x490195
// Rebuilds the forest list (nearest to the town center first) and the lists of huntable animals (sorted by
// distance) and fish from nature's units
void EECPResourceManager::RefreshResources()
{
    U2DPoint<long> theCenter;
    UString theName;
    EEPlayer* theNature = gWorld.mPlayers[0];
    this->mForests.clear();
    for (unsigned long i = 0, n = gContinentManager.mForests.size(); i < n; i++) {
        EEAIForest* theForest = gContinentManager.mForests[(unsigned short)i];
        if (theForest != NULL) {
            unsigned short theID = theForest->mID;
            this->mForests.push_back(theID);
        }
    }
    theCenter = U2DPoint<long>(this->mManager->mBuildManager->mTownCenterTile.mX, this->mManager->mBuildManager->mTownCenterTile.mY);
    if (theCenter.GetX() != -10000 && theCenter.GetY() != -10000)
        std::sort(this->mForests.begin(), this->mForests.end(), EECPRMForestDistanceLess(theCenter));
    unsigned long j;
    for (j = 0; j < this->mMines.size(); j++)
        this->mMines[j]->Dereference();
    this->mMines.clear();
    unsigned long k;
    for (k = 0; k < this->mFish.size(); k++)
        this->mFish[k]->Dereference();
    this->mFish.clear();
    for (EEUnit* theUnit = theNature->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (!(theUnit->GetResourceAmount() > 0) || theUnit->GetResourceType() != kResourceFood || !(theUnit->GetHitPoints() > 0) || theUnit->GetStats()->GetAttack() != 0.0f)
            continue;
        long theKind = theUnit->GetUnitClass()->GetFamily();
        if (theKind == kUnitFamilyAnimal) {
            theUnit->Reference();
            EEComplexUnit* theMine = theUnit->GetComplexUnit();
            this->mMines.push_back(theMine);
        } else if (theKind == kUnitFamilyFish) {
            theUnit->Reference();
            this->mFish.push_back(static_cast<EEComplexUnit*>(theUnit));
        }
    }
    if (theCenter.GetX() != -10000 && theCenter.GetY() != -10000)
        std::sort(this->mMines.begin(), this->mMines.end(), EECPRMUnitDistanceLess(theCenter));
}

// FUNCTION: EXE 0x490406
// Adds a tech tree node's scaled cost, times a count, to the needs for food through iron
void EECPResourceManager::AddCosts(EETechTreeNode* inNode, long inCount)
{
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    for (long i = 2; i < 7; i++) {
        long theValue = inNode->mData->mCosts[i];
        long theCost = thePlayer->ScaleCost(theValue);
        if (theCost > 0)
            this->Earn(i, theCost * inCount);
    }
}

// FUNCTION: EXE 0x490458
// Collects up to a count of live, unthreatened animals on the town's continent within near or far range of a
// town center
void EECPResourceManager::FindMines(unsigned long inCount, bool inNear, std::vector<EEComplexUnit*>& outUnits)
{
    long theRadius = this->GetManager()->GetPlayer()->GetDropOffDistance();
    outUnits.clear();
    long theRange = inNear ? theRadius * theRadius * 4 : theRadius * theRadius * 36;
    EETileCoordinate theCenter;
    theCenter.SetX(this->GetManager()->GetBuildManager()->mTownCenterTile.GetX());
    theCenter.SetY(this->GetManager()->GetBuildManager()->mTownCenterTile.GetY());
    bool theHaveCenter;
    if (theCenter.GetX() != -10000 && theCenter.GetY() != -10000)
        theHaveCenter = true;
    else
        theHaveCenter = false;
    unsigned long theFound = 0;
    for (unsigned long i = 0; theFound < inCount && i < this->mMines.size(); i++) {
        EEComplexUnit* theMine = this->GetMinesAt(i);
        bool theSame;
        if (theHaveCenter) {
            EEComputerPlayer* thePlayer = this->GetManager()->GetPlayer();
            long theY = theMine->GetTileY();
            long theX = theMine->GetTileX();
            unsigned short theCenterID = gContinentManager.GetContinentID(theCenter.GetX(), theCenter.GetY());
            long theFlags = thePlayer->GetCitizenPassability();
            theSame = gContinentManager.AreConnectedFromIDs(theCenterID, gContinentManager.GetContinentID(theX, theY), theFlags);
        } else
            theSame = true;
        if (!theSame || !theMine->GetIsValid() || !(theMine->GetResourceAmount() > 0) || !(theMine->GetHitPoints() > 0))
            continue;
        bool theFree = !this->GetManager()->GetBuildManager()->HasEnemyNear(theMine->GetTileX(), theMine->GetTileY());
        if (!theFree)
            continue;
        if (EECPNextEngine(this->GetManager()->GetPlayer())) {
            // the next engine (§8.35, the owner: "hunting really really far away from home"): the game within the
            // claim range of the town centre (EECPNextHuntReach), not more than kNextHuntersPerAnimal hunters an animal
            long theDX = theMine->GetTileX() - theCenter.GetX(), theDY = theMine->GetTileY() - theCenter.GetY();
            long theReach = EECPNextHuntReach(this->GetManager()->GetPlayer());
            if (theHaveCenter && theDX * theDX + theDY * theDY <= theReach * theReach
                && EECPNextAssigned(this->GetManager()->GetPlayer(), theMine) < kNextHuntersPerAnimal) {
                outUnits.push_back(this->GetMinesAt(i));
                theFound++;
            }
            continue;
        }
        EEComplexUnit* theCenterUnit = this->FindNearestTownCenter(theMine);
        if (theCenterUnit != NULL && theCenterUnit->DistanceSquaredTo(theMine) < (float)theRange) {
            outUnits.push_back(this->GetMinesAt(i));
            theFound++;
        }
    }
}

// FUNCTION: EXE 0x490602
// Returns the first forage bush of a resource center that is reachable, not crowded, unthreatened and in range
// of the town center; bushes not visible are skipped
EEUnit* EECPResourceManager::FindFarm(bool inNeedsDropSite, bool inNear, EEComplexUnit* inUnit)
{
    EETileDistanceLess theTile(0, 0);
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    long theRadius = thePlayer->mDropOffDistance;
    unsigned long theLimit = thePlayer->mMiningLimit;
    EETileDistanceLess theCenter(this->mManager->mBuildManager->mTownCenterTile);
    EEUnit* theResult = NULL;
    long theRange = inNear ? theRadius * theRadius * 4 : theRadius * theRadius * 36;
    if (inNear && (theCenter.mCenter.mX == -10000 || theCenter.mCenter.mY == -10000))
        return theResult;
    for (unsigned long i = 0; i < thePlayer->GetNumResourceCenters(); i++) {
        EECPResourceCenter* theCenterObject = thePlayer->GetResourceCenter(i);
        EEUnit* theUnit = theCenterObject->FindResourceUnit(2);
        if (theUnit == NULL || theUnit->GetUnitClass()->GetUnitType() != kUnitTypeResForageBush)
            continue;
        if (EECPNextEngine(thePlayer)) {   // a bush of the patch with room (§8.35), else the next patch
            theUnit = EECPRMUnitWithRoom(thePlayer, theCenterObject, 2);
            if (theUnit == NULL)
                continue;
        }
        if (inUnit != NULL) {
            EETileCoordinate theUnitTile;
            theUnitTile.mX = theUnit->GetTileX();
            theUnitTile.mY = theUnit->GetTileY();
            EETileCoordinate theOtherTile;
            theOtherTile.mX = inUnit->GetTileX();
            theOtherTile.mY = inUnit->GetTileY();
            if (gContinentManager.GetContinent(theOtherTile, kAIPathLandMass) != gContinentManager.GetContinent(theUnitTile, kAIPathLandMass))
                continue;
        } else {
            EEComputerPlayer* theOwner = this->mManager->mPlayer;
            long theY = theUnit->GetTileY();
            long theX = theUnit->GetTileX();
            unsigned short theCenterID = gContinentManager.GetContinentID(theCenter.mCenter.mX, theCenter.mCenter.mY);
            long theFlags = theOwner->GetCitizenPassability();
            if (!gContinentManager.AreConnectedFromIDs(theCenterID, gContinentManager.GetContinentID(theX, theY), theFlags))
                continue;
        }
        if (!static_cast<EETile*>(theUnit->GetRTSTile())->IsVisibleTo(thePlayer))
            continue;   // skip it (was break: ended the whole search)
        if (!EECPRMHasRoom(thePlayer, theUnit, theLimit))
            continue;
        theTile.mCenter.mX = theUnit->GetTileX();
        theTile.mCenter.mY = theUnit->GetTileY();
        if (theUnit->GetResourceAmount() <= 0 || theUnit->GetRTSTile()->GetTerrainType() == kTerrainDeepWater)
            continue;
        if (inNeedsDropSite && theCenterObject->mUnit == NULL)
            continue;
        if (EECPNextEngine(thePlayer) ? EECPNextSiteBlocked(thePlayer, theUnit, NULL) > kNextBlockedUnexplored
                                      : this->mManager->mBuildManager->HasEnemyNear(theUnit->GetTileX(), theUnit->GetTileY()))
            continue;   // (the next engine: its gather safety, as FindResourceTarget)
        // economy.forage_patches: only the patches within the claim range (the playtest AI sent 17 of 27 food gatherers
        // on far trips); their settlements come from EECPRMSettlePatches. TEST ONLY: the experiment "droprule" also
        // skips a patch with no drop site within kEECPPatchDropRange tiles (lost 4-17 in the sim: the patches stood
        // unworked until their settlements were up; docs/AI_RESEARCH.md §8.28)
        if (EECPClaimRangeOn(thePlayer)
            && (!EECPRMInClaimRange(this, theUnit->GetTileX(), theUnit->GetTileY())
                || (EECPExperiment(thePlayer, "droprule")
                    && !EECPRMFoodDropNear(thePlayer, theUnit->GetTileX(), theUnit->GetTileY(), kEECPPatchDropRange))))
            continue;
        if (theTile.DistanceSquared(theCenter.mCenter) < theRange) {
            theResult = theUnit;
            break;
        }
    }
    return theResult;
}

// the experiment "navalflee": fish in a danger zone (a warship's attack, EECPRMNavalFlee) are skipped
static bool EECPRMFishInDanger(EECPResourceManager* inManager, EEUnit* inFish)
{
    return EECPNavalFleeOn(inManager->mManager->mPlayer)
        && EECPRMResourceInDanger(inManager, inFish->GetTileX(), inFish->GetTileY());
}

// the next engine (docs/AI_RESEARCH.md §8.33): the fishing boats also leave alone the fish within this many tiles of an
// armed enemy ship the player sees now (frigates sank about a third of the boom's losses, 36-40 boats a game, at fish
// the 30-second danger zones had already forgotten)
static const long kNextFishWarshipRange = 12;

// FUNCTION: EXE 0x490819
// Returns the nearest fish to a unit that is not crowded, preferring fish more than 8 tiles from every own dock
EEUnit* EECPResourceManager::FindFish(EEUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    unsigned long theLimit = this->GetManager()->GetPlayer()->mMiningLimit;
    EEUnit* theResult = NULL;
    std::sort(this->mFish.begin(), this->mFish.end(), EECPUnitTileDistanceLess(inUnit->GetTile()));
    EEGetPlayerUnitsOfType(kUnitTypeDock, this->GetManager()->GetPlayer()->GetPlayerID(), theUnits);
    std::vector<long> theWarships;
    bool theNext = EECPNextEngine(this->GetManager()->GetPlayer());
    if (theNext)
        EECPNextSeenWarships(this->GetManager()->GetPlayer(), theWarships);
    unsigned long i;
    for (i = 0; i < this->mFish.size() && theResult == NULL; i++) {
        EEComplexUnit* theUnit = this->mFish[i];
        if (theNext && EECPNextNearTiles(theWarships, theUnit->GetTileX(), theUnit->GetTileY(), kNextFishWarshipRange))
            continue;
        if (theUnit->GetResourceAmount() > 0 && gWorld.GetResourceUsers(theUnit) < theLimit && !EECPRMFishInDanger(this, theUnit)) {
            if (std::find_if(theUnits.begin(), theUnits.end(), EECPRMUnitWithin(theUnit->GetTile(), 64.0f)) == theUnits.end())
                theResult = theUnit;
        }
    }
    if (theResult != NULL)
        return theResult;
    unsigned long j;
    for (j = 0; j < this->mFish.size() && theResult == NULL; j++) {
        EEComplexUnit* theUnit = this->mFish[j];
        if (theNext && EECPNextNearTiles(theWarships, theUnit->GetTileX(), theUnit->GetTileY(), kNextFishWarshipRange))
            continue;
        if (theUnit->GetResourceAmount() > 0 && gWorld.GetResourceUsers(theUnit) < theLimit && !EECPRMFishInDanger(this, theUnit))
            theResult = theUnit;
    }
    return theResult;
}

// FUNCTION: EXE 0x4909d7
// Collects up to a count of the player's farms that are not yet built
void EECPResourceManager::GetUnfinishedFarms(long inMax, std::vector<EEComplexUnit*>& outUnits)
{
    std::deque<EEComplexUnit*>::iterator it;
    long theCount = 0;
    outUnits.clear();
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    EECPUnitManager* theUnitManager = this->mManager->mUnitManager;
    std::deque<EEComplexUnit*>* theUnits = theUnitManager->GetUnits(EEGetFarmType(thePlayer));
    if (theUnits == NULL)
        return;
    for (it = theUnits->begin(); it != theUnits->end() && theCount < inMax; ++it) {
        EEComplexUnit* theUnit = *it;
        if (!theUnit->mComplexUnit->mIsBuilt) {
            outUnits.push_back(theUnit);
            theCount++;
        }
    }
}

// FUNCTION: EXE 0x490a63
// Collects up to a count of the player's built farms that nobody works
void EECPResourceManager::GetIdleFarms(long inMax, std::vector<EEComplexUnit*>& outUnits)
{
    std::deque<EEComplexUnit*>::iterator it;
    long theCount = 0;
    outUnits.clear();
    std::deque<EEComplexUnit*>* theUnits = this->mManager->mUnitManager->GetUnits(0x2a);
    if (theUnits == NULL)
        return;
    for (it = theUnits->begin(); it != theUnits->end() && theCount < inMax; ++it) {
        EEComplexUnit* theUnit = *it;
        if (theUnit->mComplexUnit->mIsBuilt && gWorld.GetResourceUsers(theUnit) == 0) {
            outUnits.push_back(theUnit);
            theCount++;
        }
    }
}

// FUNCTION: EXE 0x490af3
// Returns a tree in the nearest reachable forest to the town center (single-tree forests only for fewer than 8
// gatherers); NULL without a town center
EEUnit* EECPResourceManager::FindForestTarget(long inCount, EEComplexUnit* inUnit)
{
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    U2DPoint<long> theCenter;
    theCenter = U2DPoint<long>(this->mManager->mBuildManager->mTownCenterTile.mX, this->mManager->mBuildManager->mTownCenterTile.mY);
    EEUnit* theResult = NULL;
    if (theCenter.mX == -10000 || theCenter.mY == -10000)
        return theResult;
    for (unsigned long i = 0; i < this->mForests.size() && theResult == NULL; i++) {
        EEAIForest* theForest = gContinentManager.mForests[this->mForests[i]];
        if (theForest == NULL)
            continue;
        if (inUnit != NULL) {
            unsigned short theContinent = theForest->mContinentID;
            if (!gContinentManager.AreConnectedByID(gContinentManager.GetContinentID(inUnit), theContinent,
                                           this->mManager->mPlayer->GetCitizenPassability()))
                continue;
        }
        if (theForest->mTiles.Size() == 1) {
            if (inCount < 8) {
                U2DPoint<long> thePoint = theForest->GetClosestPoint(theCenter);
                theTile.mX = thePoint.mX;
                theTile.mY = thePoint.mY;
                EEMapTile* theMapTile = gWorld.mMap->GetTile(theTile.mX, theTile.mY);
                if (theMapTile != NULL) {
                    for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode != NULL && theResult == NULL; theNode = theNode->mNext) {
                        EEUnit* theUnit = theNode->mUnit;
                        if (theUnit != NULL && theUnit->GetUnitClass()->mResourceType == kResourceWood)
                            theResult = theUnit;
                    }
                    if (theResult != NULL)
                        continue;
                }
            }
        }
        if (!(theForest->mTiles.Size() > 1))
            continue;
        U2DPoint<long> thePoint = theForest->GetClosestPoint(theCenter);
        theTile.mX = thePoint.mX;
        theTile.mY = thePoint.mY;
        EETile* theMapTile = gWorld.mMap->GetTileAtLocation(*(EETileLocation*)&theTile);
        if (theMapTile == NULL)
            continue;
        for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode != NULL && theResult == NULL; theNode = theNode->mNext) {
            EEUnit* theUnit = theNode->mUnit;
            if (theUnit != NULL && theUnit->GetUnitClass()->mResourceType == kResourceWood)
                theResult = theUnit;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x490cb5
// Returns a resource unit of a type from the resource centers (forwards or backwards) that is visible, not
// crowded, unthreatened, away from other players' areas and reachable from the unit
// economy.forage_patches: FindResourceTarget's first pass takes only the mines within the claim range (the playtest AI
// sent 6 citizens to an iron mine far out with no settlement); a second pass, the original's, when none is left
static bool sClaimPass = false;

EEUnit* EECPResourceManager::FindResourceTarget(long inResource, EEComplexUnit* inUnit, bool inBackwards)
{
    if (!sClaimPass && EECPClaimRangeOn(this->mManager->mPlayer)) {
        sClaimPass = true;
        EEUnit* theNear = this->FindResourceTarget(inResource, inUnit, inBackwards);
        sClaimPass = false;
        if (theNear != NULL)
            return theNear;
    }
    EETileDistanceLess theTile(0, 0);
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    EECPBuildManager* theBuildManager = this->mManager->mBuildManager;
    unsigned long theLimit = thePlayer->mMiningLimit;
    EEUnit* theResult = NULL;
    unsigned long theCount = thePlayer->GetNumResourceCenters();
    if (theCount <= 0)
        return theResult;
    unsigned long i = inBackwards ? theCount - 1 : 0;
    while (true) {
        EECPResourceCenter* theCenter = thePlayer->GetResourceCenter(i);
        bool theOK = false;
        if (theCenter != NULL && theCenter->mIsActive) {
            EEUnit* theUnit = theCenter->FindResourceUnit(inResource);
            if (theUnit != NULL && EECPNextEngine(thePlayer)) {   // a unit with room (§8.35)
                EEUnit* theRoom = EECPRMUnitWithRoom(thePlayer, theCenter, (unsigned long)inResource);
                if (theRoom != NULL)
                    theUnit = theRoom;
            }
            if (theUnit != NULL && static_cast<EETile*>(theUnit->mTile)->IsVisibleTo(thePlayer) && EECPRMHasRoom(thePlayer, theUnit, theLimit)
                && theUnit->mResourceAmount > 0 && theUnit->mTile->mTerrainType != kTerrainDeepWater) {
                // the next engine: its gather safety (EECPNextSiteBlocked: seen soldiers, towers in range, a danger
                // zone, another player's land; a home site only soldiers on it; docs/AI_RESEARCH.md §8.34)
                bool theNextCheck = EECPNextEngine(thePlayer);
                if (theNextCheck)
                    theOK = EECPNextSiteBlocked(thePlayer, theUnit, NULL) <= kNextBlockedUnexplored;
                else
                    theOK = !theBuildManager->HasEnemyNear(theUnit->GetTileX(), theUnit->GetTileY());
                if (theOK && !theNextCheck)
                    theOK = !EECPRMResourceInDanger(this, theUnit->GetTileX(), theUnit->GetTileY());
                if (theOK && sClaimPass)
                    theOK = EECPRMInClaimRange(this, theUnit->GetTileX(), theUnit->GetTileY());
                if (theOK) {
                    EEPlayer** thePlayers = gWorld.mPlayers;
                    for (unsigned long j = 0; j < gWorld.mNumPlayers && theOK && !theNextCheck; j++, thePlayers++) {
                        EEPlayer* theOther = *thePlayers;
                        if (theOther == thePlayer || theOther->mEnemies[thePlayer->mPlayerID] != 0)
                            continue;
                        EETileCoordinate theUnitTile;
                        theUnitTile.mX = theUnit->GetTileX();
                        theUnitTile.mY = theUnit->GetTileY();
                        EEAIArea* theArea = theOther->GetNearestArea(theUnitTile, true);
                        if (theArea == NULL)
                            continue;
                        U2DPoint<long> theFrom(theUnit->GetTileX(), theUnit->GetTileY());
                        U2DPoint<long> theClosest = theArea->GetClosestHullPoint(theFrom);
                        theTile.mCenter.mX = theClosest.mX;
                        theTile.mCenter.mY = theClosest.mY;
                        EETileCoordinate theTo;
                        theTo.mX = theUnit->GetTileX();
                        theTo.mY = theUnit->GetTileY();
                        long theDistance = theTile.DistanceSquared(theTo);
                        if ((float)theDistance > 100.0f)
                            theOK = true;
                        else
                            theOK = false;
                    }
                    if (theOK) {
                        if (inUnit == NULL)
                            return theUnit;
                        if (gContinentManager.AreConnected(gContinentManager.GetContinentAt(theUnit),
                                                      gContinentManager.GetContinentAt(inUnit),
                                                      this->mManager->mPlayer->GetCitizenPassability()))
                            return theUnit;
                    }
                }
            }
        }
        if (inBackwards) {
            if (i == 0)
                break;
            i--;
        } else {
            i++;
            if (i == theCount)
                break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x490ec6
// Returns the player's dock nearest to a unit, or NULL
EEUnit* EECPResourceManager::FindNearestDock(EEComplexUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    EEUnit* theResult = NULL;
    EEGetPlayerUnitsOfType(kUnitTypeDock, this->mManager->mPlayer->mPlayerID, theUnits);
    if (theUnits.size() > 0) {
        std::sort(theUnits.begin(), theUnits.end(), EECPUnitTileDistanceLess(inUnit->GetTile()));
        theResult = theUnits[0]->mComplexUnit;
    }
    return theResult;
}

// FUNCTION: EXE 0x490f72
// Returns the player's settlement, town center or capitol nearest to a unit, or NULL
EEComplexUnit* EECPResourceManager::FindNearestTownCenter(EEComplexUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    EEComplexUnit* theResult = NULL;
    long thePlayerID = this->mManager->mPlayer->mPlayerID;
    EEGetPlayerUnitsOfType(kUnitTypeSettlement, thePlayerID, theUnits);
    EEGetPlayerUnitsOfType(kUnitTypeTownCenter, thePlayerID, theUnits);
    EEGetPlayerUnitsOfType(kUnitTypeCapitol, thePlayerID, theUnits);
    if (theUnits.size() > 0) {
        std::sort(theUnits.begin(), theUnits.end(), EECPUnitTileDistanceLess(inUnit->GetTile()));
        theResult = theUnits[0]->mComplexUnit;
    }
    return theResult;
}

// FUNCTION: EXE 0x491037
// Raises the need for a resource by an amount (no change for 0)
void EECPResourceManager::Earn(long inResource, unsigned long inAmount)
{
    if (inAmount > 0)
        this->mNeeds[inResource] += inAmount;
}

// FUNCTION: EXE 0x491054
// Reads the drop site classes, needs, wanted gatherers, flags and timers from the saved game; returns the first
// read error
long EECPResourceManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = 0;
    unsigned long i;
    for (i = 0; i < 7 && theResult == 0; i++) {
        unsigned long theCount;
        theResult = inFile.Read(theCount);
        if (theResult != 0)
            continue;
        this->mDropSiteClasses[i].clear();
        unsigned long j;
        for (j = 0; j < theCount && theResult == 0; j++) {
            unsigned long theID;
            theResult = inFile.Read(theID);
            if (theResult == 0)
                this->mDropSiteClasses[i].push_back(theID);
        }
    }
    if (theResult != 0)
        return theResult;
    for (i = 0; i < 7 && theResult == 0; i++)
        theResult = inFile.Read(this->mNeeds[i]);
    if (theResult != 0)
        return theResult;
    for (i = 0; i < 7 && theResult == 0; i++)
        theResult = inFile.Read(this->mWantedGatherers[i]);
    if (theResult != 0)
        return theResult;
    for (i = 0; i < 7 && theResult == 0; i++)
        theResult = inFile.Read(this->mB180[i]);
    if (theResult == 0)
        theResult = inFile.Read(this->mInitialized);
    if (theResult == 0)
        theResult = inFile.Read(this->mSettlementsEnabled);
    if (theResult == 0)
        theResult = inFile.Read(this->mTime170);
    if (theResult == 0)
        theResult = inFile.Read(this->mNextWantedGatherersTime);
    if (theResult == 0)
        theResult = inFile.Read(this->mNextAssignGatherersTime);
    if (theResult == 0)
        theResult = inFile.Read(this->mNextResourceCheckTime);
    return theResult;
}

// FUNCTION: EXE 0x4911ea
// Returns whether the build manager has a settlement queued (the argument is unused)
bool EECPResourceManager::IsSettlementQueued(long inArg)
{
    return this->mManager->mBuildManager->IsQueued(0x2b);
}

// FUNCTION: EXE 0x4911fd
// Rebuilds the forest, animal and fish lists
void EECPResourceManager::Refresh()
{
    this->RefreshResources();
}

// FUNCTION: EXE 0x491202
// Moves swapped citizens between the gatherer lists, but tests the OLD unit's owner before adding the new one
void EECPResourceManager::UnitsSwapped(std::vector<EEPUnitSwap>& inSwaps)
{
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    for (std::vector<EEPUnitSwap>::iterator it = inSwaps.begin(); it != inSwaps.end(); it++) {
        EEComplexUnit* theUnit = it->mOld;
        EEComplexUnit* theNew = it->mNew;
        if (theUnit != NULL) {
            long theCategory = theUnit->GetUnitClass()->mCategory;
            if ((theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen) && theUnit->mPlayer == thePlayer)
                this->RemoveGatherer(theUnit);
        }
        if (theNew == NULL)
            continue;
        long theCategory = theNew->GetUnitClass()->mCategory;
        if ((theCategory != kUnitCategoryLandCitizen && theCategory != kUnitCategoryNavalCitizen) || theNew->mPlayer != thePlayer)
            continue;
        EEUGGather* theGoal = dynamic_cast<EEUGGather*>(theNew->GetLoadGoal());
        if (theGoal != NULL && theGoal->mResource != NULL)
            this->AddGatherer(theNew, theGoal->mResource->mResourceType);
    }
}


// FUNCTION: EXE 0x4912be
// Sends a unit on the same land mass to gather a resource (new or retargeted gather goal) and files it under
// that resource
void EECPResourceManager::SendToGather(EEComplexUnit* inUnit, EEUnit* inResource, EEUnit* inStorage)
{
    long theFlags = this->mManager->mPlayer->GetCitizenPassability();
    unsigned short theResourceContinent = gContinentManager.GetContinentID(inResource);
    unsigned short theUnitContinent = gContinentManager.GetContinentID(inUnit);
    bool theOK = gContinentManager.AreConnectedFromIDs(theUnitContinent, theResourceContinent, theFlags);
    if (!theOK)
        return;
    EEUnitAction* theCommand = inUnit->mCommand;
    if (theCommand != NULL && theCommand->mType == kActionIdle) {
        if (inResource->GetResourceType() != inUnit->GetResourceType())
            inUnit->SetResourceAmount(0);
        EEUnitGoal* theGoal = new EEUGGather(inResource, inStorage, true);
        inUnit->ClearGoalQueue(0);
        inUnit->SetGoal(theGoal, 0, 0);
    } else if (inUnit->IsGathering()) {
        EEUGGather* theGoal = static_cast<EEUGGather*>(inUnit->GetLoadGoal());
        inUnit->SetResourceAmount(0);
        inUnit->SetGatheredResource(NULL);
        theGoal->SetResource(inResource);
        theGoal->SetStorage(inStorage);
    } else {
        EEUGGather* theGoal = dynamic_cast<EEUGGather*>(inUnit->GetLoadGoal());
        if (theGoal != NULL) {
            if (inResource->GetResourceType() != inUnit->GetResourceType())
                inUnit->SetResourceAmount(0);
            inUnit->SetGatheredResource(NULL);
            theGoal->SetResource(inResource);
            theGoal->SetStorage(inStorage);
        } else
            theOK = false;
    }
    if (!theOK)
        return;
    inUnit->SetResourceType(inResource->GetResourceType());
    inUnit->StartAction(kActionMoveToWaitingArea);
    inUnit->GetUnitClass()->ActionChanged(inUnit);
    this->RemoveGatherer(inUnit);
    this->AddGatherer(inUnit, inResource->GetResourceType());
}

// FUNCTION: EXE 0x491430
// Clears the per-resource flags and the wanted gatherer counts
void EECPResourceManager::Reset()
{
    for (unsigned long i = 0; i < 7; i++) {
        this->mB180[i] = false;
        this->mWantedGatherers[i] = 0;
    }
}

// FUNCTION: EXE 0x49144d
// Refiles the citizens with a gather goal after loading and turns the saved relative timers absolute; returns 0
long EECPResourceManager::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEComplexUnit*> theUnused;
    unsigned long theTime = gPhysics->mGameTime;
    this->mManager->mUnitManager->GetCitizens(theUnits);
    unsigned long i, n;
    for (i = 0, n = theUnits.size(); i < n; i++) {
        EEComplexUnit* theUnit = theUnits[i];
        EEUGGather* theGoal = dynamic_cast<EEUGGather*>(theUnit->GetLoadGoal());
        if (theGoal != NULL && theGoal->mResource != NULL)
            this->AddGatherer(theUnit, theGoal->mResource->mResourceType);
    }
    this->mInitialized = false;
    this->mTime170 += theTime;
    this->mNextWantedGatherersTime += theTime;
    this->mNextAssignGatherersTime += theTime;
    this->mNextResourceCheckTime += theTime;
    return 0;
}

// FUNCTION: EXE 0x49152a
// Writes the drop site classes, needs, wanted gatherers, flags and timers (relative to now) to the saved game;
// returns the first write error
long EECPResourceManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = 0;
    unsigned long theTime = gPhysics->mGameTime;
    unsigned long i;
    for (i = 0; i < 7 && theResult == 0; i++) {
        unsigned long theCount = this->mDropSiteClasses[i].size();
        theResult = outFile.Write(theCount);
        if (theResult != 0)
            continue;
        unsigned long j;
        for (j = 0; j < theCount && theResult == 0; j++)
            theResult = outFile.Write(this->mDropSiteClasses[i][j]);
    }
    if (theResult != 0)
        return theResult;
    for (i = 0; i < 7 && theResult == 0; i++)
        theResult = outFile.Write(this->mNeeds[i]);
    if (theResult != 0)
        return theResult;
    for (i = 0; i < 7 && theResult == 0; i++)
        theResult = outFile.Write(this->mWantedGatherers[i]);
    if (theResult != 0)
        return theResult;
    unsigned long k;
    for (k = 0; k < 7 && theResult == 0; k++)
        theResult = outFile.Write(this->mB180[k]);
    if (theResult == 0)
        theResult = outFile.Write(this->mInitialized);
    if (theResult == 0)
        theResult = outFile.Write(this->mSettlementsEnabled);
    if (theResult == 0) {
        unsigned long theValue = this->mTime170 - theTime;
        theResult = outFile.Write(theValue);
        if (theResult == 0) {
            theValue = this->mNextWantedGatherersTime - theTime;
            theResult = outFile.Write(theValue);
            if (theResult == 0) {
                theValue = this->mNextAssignGatherersTime - theTime;
                theResult = outFile.Write(theValue);
                if (theResult == 0) {
                    theValue = this->mNextResourceCheckTime - theTime;
                    theResult = outFile.Write(theValue);
                }
            }
        }
    }
    return theResult;
}

// NEAR: the 0x92230 assign shape is a guess from the decompiler outline; the original returns true on every
// path (0x49195e)
// FUNCTION: EXE 0x4916e6
// Loads as many gatherers as fit on a transport and ships them to a resource to gather it; returns false
// when the transport cannot unload there
bool EECPResourceManager::FerryGatherers(std::vector<EEComplexUnit*>& inUnits, EEComplexUnit* inTransport, EEUnit* inResource)
{
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EETileCoordinate> theTiles;
    long theX = inResource->GetTileX();
    long theY = inResource->GetTileY();
    EEUGUnloadTransport* theUnload = new EEUGUnloadTransport(inTransport, theX, theY, true);
    if (theUnload != NULL) {
        if (theUnload->CanUnloadAtDestination()) {
            inTransport->ClearGoalQueue(0);
            inTransport->SetGoal(theUnload, 0, 0);
            long theCount = inTransport->GetUnitClass()->mTransportCapacity / inUnits[0]->GetUnitClass()->mWeight;
            std::vector<EEComplexUnit*>::iterator theEnd = theCount > inUnits.size() ? inUnits.end() : inUnits.begin() + theCount;
            theUnits.assign(inUnits.begin(), theEnd);
            EEUGLoadTransport* theLoad = new EEUGLoadTransport(inTransport, theUnits, true);
            inTransport->SetGoal(theLoad, 1, 0);
            inTransport->StartAction(kActionPrepareToMove);
            inTransport->GetUnitClass()->ActionChanged(inTransport);
            EEUnitGoal* theGather = new EEUGGather(inResource, NULL, true);
            unsigned long i, n;
            for (i = 0, n = theUnits.size(); i < n; i++) {
                theUnits[i]->ClearGoalQueue(0);
                theUnits[i]->SetGoal(theGather->Clone(), 0, 0);
                theUnits[i]->SetGoal(theLoad->Clone(), 1, 0);
                theUnits[i]->StartAction(kActionPrepareToMove);
                theUnits[i]->GetUnitClass()->ActionChanged(theUnits[i]);
            }
            if (theGather != NULL)
                delete theGather;
        } else {
            delete theUnload;
            return false;   // cannot unload there: RequestFerry's failure branch runs (was true)
        }
    }
    return true;
}

// The container tile is copied by value before forwarding the coordinates (original 449-byte body).
// FUNCTION: EXE 0x491965
// Handles an idle transport: idles its loading citizens and unloads its cargo at the rendezvous, or re-queues a
// detached transport's unload
void EECPResourceManager::TransportIdle(EEComplexUnit* inUnit)
{
    std::vector<EEComplexUnit*> theUnits;
    EEUnitGoal* theGoal = inUnit->mActiveGoal;
    EEUGLoadTransport* theLoad = dynamic_cast<EEUGLoadTransport*>(theGoal);
    EEUGUnloadTransport* theUnload = dynamic_cast<EEUGUnloadTransport*>(theGoal);
    bool theChanged = false;
    if (theLoad == NULL && theUnload == NULL) {
        theGoal = inUnit->mActiveGoal;
        theLoad = dynamic_cast<EEUGLoadTransport*>(theGoal);
        theUnload = dynamic_cast<EEUGUnloadTransport*>(theGoal);
    }
    if (theLoad != NULL) {
        UReferenceContainer<EEComplexUnit*>& theCargo = theLoad->mUnits;
        long theCount = theCargo.GetSize();
        if (theCount > 0) {
            this->mManager->mUnitManager->GetCitizens(theUnits);
            for (std::vector<EEComplexUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); ++it) {
                EEComplexUnit* theUnit = *it;
                std::vector<EEComplexUnit*>::iterator theWhere;
                if (!theCargo.Contains(theUnit, theWhere))
                    continue;
                (*it)->ClearGoalQueue(0);
                (*it)->StartAction(kActionIdle);
                (*it)->mUnitClass->ActionChanged(*it);
                theChanged = true;
            }
            if (theChanged) {
                if (inUnit->GetNumCargoUnits() > 0 && theLoad->EvaluateCondition(inUnit, kTriggerHaveReachedRendezvousPoint)) {
                    U2DPoint<long> theTile = theLoad->mContainerTile;
                    inUnit->UnloadCargoAtTile(theTile.GetX(), theTile.GetY(), true);
                }
                inUnit->ClearGoalQueue(0);
            }
        }
    } else if (theUnload != NULL) {
        if (inUnit->GetNumCargoUnits() > 0 && inUnit->mIsDetachedFromAI) {
            EEUGUnloadTransport* theGoal = new EEUGUnloadTransport(inUnit, theUnload->mDestination.GetX(),
                                                                   theUnload->mDestination.GetY(), true);
            if (theGoal->CanUnloadAtDestination())
                inUnit->QueueGoal(theGoal, 0);
        }
    }
}
// FUNCTION: EXE 0x491b26
// Removes a tech tree node's scaled cost, times a count, from the needs for food through iron
void EECPResourceManager::RemoveCosts(EETechTreeNode* inNode, long inCount)
{
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    for (long i = 2; i < 7; i++) {
        long theValue = inNode->mData->mCosts[i];
        long theCost = thePlayer->ScaleCost(theValue);
        if (theCost > 0)
            this->Spend(i, theCost * inCount);
    }
}

// New (docs/AI_RESEARCH.md §8.8-8.11): the economy's tuning is the personality's (aipersonalities.yaml economy).
// UpdateWantedGatherers keeps the original's split by mNeeds (a ledger of queued costs minus what was gathered or
// started; food-heavy, which suits the first epochs: food is citizens) and then corrects it by the stock: a resource
// with more than surplus in stock gets surplus_cut of its share; one with less than low_stock at least its floor of
// the gatherers. (Two replacements of the split itself grew the honest economy more slowly: §8.10.)

// What the computer player has asked for and not yet paid, per resource (scaled costs)
[[maybe_unused]] static void EECPRMPendingCosts(EECPCivilizationManager* inManager, long* outCosts)
{
    EEComputerPlayer* thePlayer = inManager->mPlayer;
    EETechTree* theTree = thePlayer->mTechnologies;
    for (long r = 0; r < 7; r++)
        outCosts[r] = 0;
    if (theTree == NULL)
        return;
    struct Local {
        static void Add(EEComputerPlayer* inPlayer, EETechTreeNode* inNode, long inCount, long* ioCosts)
        {
            if (inNode == NULL || inCount <= 0)
                return;
            for (long r = 2; r < 7; r++)
                ioCosts[r] += inPlayer->ScaleCost(inNode->GetCost(r, kUnitTypeTownCenter, 1.0f)) * inCount;
        }
        static EETechTreeNode* ClassNode(EETechTree* inTree, unsigned long inClass)
        {
            EEUnitClass* theClass = inClass < EEUnitClassCount() ? gUnitClassTable[inClass] : NULL;
            return theClass != NULL ? inTree->Get(theClass->mTechnology) : NULL;
        }
    };
    for (long e = 0; e < EEEpochCount(); e++) {
        if (inManager->mBuildManager != NULL)
            for (unsigned long k = 0; k < inManager->mBuildManager->mQueues[e].size(); k++)
                Local::Add(thePlayer, Local::ClassNode(theTree, inManager->mBuildManager->mQueues[e][k].first), 1, outCosts);
        if (inManager->mUnitManager != NULL)
            for (unsigned long k = 0; k < inManager->mUnitManager->mRequests[e].size(); k++) {
                const EECPUMRequest& theRequest = inManager->mUnitManager->mRequests[e][k];
                Local::Add(thePlayer, Local::ClassNode(theTree, theRequest.mUnitClass), (long)theRequest.mCount, outCosts);
            }
        if (inManager->mResearchManager != NULL)
            for (unsigned long k = 0; k < inManager->mResearchManager->mEpochIDs[e].size(); k++)
                Local::Add(thePlayer, theTree->Get(inManager->mResearchManager->mEpochIDs[e][k]), 1, outCosts);
    }
    if (inManager->mUnitManager != NULL) {
        long theMissing = inManager->mUnitManager->GetWantedCitizens() - (long)inManager->mUnitManager->GetNumWorkers();
        if (theMissing > 10)
            theMissing = 10;   // the next ten citizens
        Local::Add(thePlayer, Local::ClassNode(theTree, kUnitTypeCitizen), theMissing, outCosts);
    }
}

// New (docs/AI_RESEARCH.md §8.26; aipersonalities.yaml economy.coast_fishing): fishing off any coast. The original fished only on island
// maps (mUsesFishing, where it also gives up farms) and with one boat at a time, so on a continent its food before
// the Copper Age farms was the bushes and the first animals. From the Stone Age (the first fishing boat) the fish
// within kCoastFishRange tiles of the town centre take up to kCoastBoatsPerFish boats each, kCoastBoats at most,
// worked from the player's built dock (one is queued while there is none); farms stay as they are.
static const long kCoastFishRange = 30;
static const unsigned long kCoastBoats = 8;
static const unsigned long kCoastBoatsLater = 16;   // the experiment "foodeco": "then lots of fishing boats" (the owner)
static const unsigned long kCoastBoatsPerFish = 2;

static unsigned long EECPRMCoastFish(EECPResourceManager* inManager)
{
    EECPBuildManager* theBuild = inManager->mManager->mBuildManager;
    if (theBuild == NULL || theBuild->mTownCenterTile.mX == -10000)
        return 0;
    long theX = theBuild->mTownCenterTile.mX;
    long theY = theBuild->mTownCenterTile.mY;
    unsigned long theCount = 0;
    for (unsigned long k = 0; k < inManager->mFish.size(); k++) {
        EEComplexUnit* theFish = inManager->mFish[k];
        if (!(theFish->mResourceAmount > 0) || !(theFish->mHitPoints > 0))
            continue;
        long theDX = theFish->GetTileX() - theX;
        long theDY = theFish->GetTileY() - theY;
        if (theDX * theDX + theDY * theDY <= kCoastFishRange * kCoastFishRange)
            theCount++;
    }
    return theCount;
}

static unsigned long EECPRMCoastBoatsWanted(EECPResourceManager* inManager)
{
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    if (thePlayer->mUsesFishing || !EECPCoastFishingOn(thePlayer) || thePlayer->mTechnologies == NULL
        || thePlayer->mTechnologies->mCurrentEpoch < kTechEpochStone)
        return 0;
    unsigned long theBoats = EECPRMCoastFish(inManager) * kCoastBoatsPerFish;
    unsigned long theMax = EECPForagePatchesOn(thePlayer) ? kCoastBoatsLater : kCoastBoats;
    return theBoats < theMax ? theBoats : theMax;
}

static bool EECPRMFishes(EECPResourceManager* inManager)
{
    return inManager->mManager->mPlayer->mUsesFishing || EECPRMCoastBoatsWanted(inManager) > 0;
}

// the coast's boats: a dock first, then one boat request at a time up to the wanted count; the island rule keeps
// the original's single request
static const unsigned long kBoatRushBatch = 4;   // the experiment "boatrush": boats requested at once
static const unsigned long kBoatHoldMs = 120000;   // the experiment "boatrush2": no boat bought this long after a loss

static bool EECPRMHasBuildManager(EECPCivilizationManager* inManager)
{
    return inManager->mBuildManager != NULL;
}

static void EECPRMRequestBoats(EECPResourceManager* inManager)
{
    EECPCivilizationManager* theManager = inManager->mManager;
    unsigned long theWanted = EECPRMCoastBoatsWanted(inManager);
    if (theWanted == 0) {
        theManager->mUnitManager->RequestFishingBoats(1);
        return;
    }
    std::vector<EEUnit*> theDocks;
    EEGetPlayerUnitsOfType(kUnitTypeDock, theManager->mPlayer->mPlayerID, theDocks);
    unsigned long theBuilt = 0;
    for (unsigned long k = 0; k < theDocks.size(); k++)
        if (theDocks[k]->mComplexUnit != NULL && theDocks[k]->mComplexUnit->mIsBuilt && theDocks[k]->mHitPoints > 0)
            theBuilt++;
    bool theRush = EECPExperiment(theManager->mPlayer, "boatrush") || EECPExperiment(theManager->mPlayer, "boatrush2");
    // the experiment "boatrush": a second dock once the first stands, so two train boats at once
    if (EECPRMHasBuildManager(theManager) && (theBuilt == 0 || (theRush && theBuilt == 1 && theDocks.size() < 2 && theWanted > kBoatRushBatch)))
        theManager->mBuildManager->QueueBuilding(kUnitTypeDock, false, true);
    if (theBuilt == 0)
        return;
    long theClass = theManager->GetFishingBoatClass();
    EECPUGatherSpecialUnitsCount theCount(theManager->mPlayer, theClass, 0, false);
    theManager->mUnitManager->GatherUnits(theCount);
    if (theRush) {
        // the experiment "boatrush" (the owner: "I spam boats ASAP at stone"): every boat short of the wanted count is
        // requested, kBoatRushBatch at a time
        unsigned long theHave = (unsigned long)(theCount.mUnits + theCount.mRequested);
        // the experiment "boatrush2" (docs/AI_RESEARCH.md §8.29): boatrush lost 29-45 boats a game to warships by 60
        // minutes and spent its wood rebuying them; after a boat is lost none is bought for kBoatHoldMs
        if (EECPExperiment(theManager->mPlayer, "boatrush2")) {
            static long sBoats[64];
            static unsigned long sHold[64];
            unsigned long theID = theManager->mPlayer->mPlayerID < 64 ? theManager->mPlayer->mPlayerID : 63;
            unsigned long theNow = gPhysics->mGameTime;
            if (sHold[theID] > theNow + kBoatHoldMs)
                sHold[theID] = 0;   // a new game
            if ((long)theCount.mUnits < sBoats[theID])
                sHold[theID] = theNow + kBoatHoldMs;
            sBoats[theID] = (long)theCount.mUnits;
            if (theNow < sHold[theID])
                return;
        }
        if (theHave < theWanted) {
            unsigned long theMore = theWanted - theHave;
            theManager->mUnitManager->RequestUnits(theClass, (long)(theMore < kBoatRushBatch ? theMore : kBoatRushBatch), 0, 0);
        }
    } else if (theCount.mRequested == 0 && (unsigned long)theCount.mUnits < theWanted)
        theManager->mUnitManager->RequestUnits(theClass, 1, 0, 0);
}

// The experiment "foodeco" (docs/AI_RESEARCH.md §8.27; the owner: a settlement on the patch, 6 per patch, the rest
// hunting, then many fishing boats). The food capacity past the centres' bushes: the gatherers working farms, fish
// or game now, and 1 more per living animal within kHuntRange tiles of the town centre (kHuntSlots at most). The
// original added every food gatherer, so the capacity grew with each one sent: 25 citizens queued at one patch.
static const long kHuntRange = 45;
static const unsigned long kHuntSlots = 12;
static const unsigned long kPatchesWanted = 3;   // the forage patches kept in the active centres (Prehistoric) ...
static const unsigned long kPatchesWantedLater = 5;   // ... and from the Stone Age (more gatherers later)

// the food gatherers whose goal is an animal
static unsigned long EECPRMHunters(EECPResourceManager* inManager)
{
    unsigned long theCount = 0;
    for (unsigned long k = 0; k < inManager->mGatherers[2].size(); k++) {
        EEUGGather* theGoal = dynamic_cast<EEUGGather*>(inManager->mGatherers[2][k]->GetLoadGoal());
        EEUnit* theResource = theGoal != NULL ? theGoal->mResource : NULL;
        if (theResource != NULL && theResource->mUnitClass->mFamily == kUnitFamilyAnimal)
            theCount++;
    }
    return theCount;
}

static unsigned long EECPRMFoodOutsideCenters(EECPResourceManager* inManager)
{
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    long theFarm = EEGetFarmType(thePlayer);
    unsigned long theCount = 0, theHunters = 0;
    for (unsigned long k = 0; k < inManager->mGatherers[2].size(); k++) {
        EEComplexUnit* theUnit = inManager->mGatherers[2][k];
        if (theUnit->GetUnitClass()->mCategory == kUnitCategoryNavalCitizen) {
            theCount++;
            continue;
        }
        EEUGGather* theGoal = dynamic_cast<EEUGGather*>(theUnit->GetLoadGoal());
        EEUnit* theResource = theGoal != NULL ? theGoal->mResource : NULL;
        if (theResource == NULL)
            continue;
        if (theResource->mUnitClass->GetUnitType() == theFarm)
            theCount++;
        else if (theResource->mUnitClass->mFamily == kUnitFamilyAnimal)
            theHunters++;
    }
    unsigned long theGame = 0;
    EECPBuildManager* theBuild = inManager->mManager->mBuildManager;
    if (theBuild != NULL && theBuild->mTownCenterTile.mX != -10000)
        for (unsigned long k = 0; k < inManager->mMines.size(); k++) {
            EEComplexUnit* theAnimal = inManager->mMines[k];
            if (!(theAnimal->mHitPoints > 0) || !(theAnimal->mResourceAmount > 0)
                || theAnimal->mUnitClass->mFamily != kUnitFamilyAnimal)
                continue;
            long theDX = theAnimal->GetTileX() - theBuild->mTownCenterTile.mX;
            long theDY = theAnimal->GetTileY() - theBuild->mTownCenterTile.mY;
            if (theDX * theDX + theDY * theDY <= kHuntRange * kHuntRange)
                theGame++;
        }
    if (theGame > kHuntSlots)
        theGame = kHuntSlots;
    return theCount + (theHunters > theGame ? theHunters : theGame);
}

static const unsigned long kEarlyWoodSlots = 10;   // the experiment "woodcap"

unsigned long EECPResourceManager::GetGatherCapacity(long inResource)
{
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    unsigned long theSites[7] = {0, 0, 0, 0, 0, 0, 0};
    unsigned long theActive = 0;
    for (int thePass = 0; thePass < 2 && theActive == 0; thePass++) {   // the active centers; all when none is
        for (unsigned long k = 0; k < thePlayer->GetNumResourceCenters(); k++) {
            EECPResourceCenter* theCenter = thePlayer->GetResourceCenter(k);
            if (thePass == 0 && !theCenter->mIsActive)
                continue;
            theActive++;
            for (unsigned long u = 0; u < theCenter->mUnits.size(); u++) {
                unsigned long theType = theCenter->mUnits[u]->mResourceType;
                if (theType >= 2 && theType < 7 && theCenter->mUnits[u]->mHitPoints > 0)
                    theSites[theType]++;
            }
        }
    }
    unsigned long theLimit = thePlayer->mMiningLimit > 0 ? (unsigned long)thePlayer->mMiningLimit : 1;
    unsigned long theTotal = 0;
    for (long r = 2; r < 7; r++) {
        unsigned long theCapacity = r == 3 ? (unsigned long)EECPPersonality(thePlayer).mWoodSlots : theSites[r] * theLimit;
        // the experiment "woodcap" (docs/AI_RESEARCH.md §8.29; the owner keeps 6-13 on wood until the Copper Age, the
        // playtest AI 15-42, while the Copper Age needs no wood): at most kEarlyWoodSlots woodcutters before Copper
        if (r == 3 && theCapacity > kEarlyWoodSlots && EECPExperiment(thePlayer, "woodcap") && thePlayer->mTechnologies != NULL
            && thePlayer->mTechnologies->mCurrentEpoch < kTechEpochCopper)
            theCapacity = kEarlyWoodSlots;
        if (r == 2 && EECPForagePatchesOn(thePlayer))
            theCapacity += EECPRMFoodOutsideCenters(this);   // farms, fish and the hunt: what works there, and the game near home
        else if (r == 2)
            theCapacity += this->mGatherers[2].size();   // farms and fish are not center units: what works there now
        if (r == 2) {
            unsigned long theBoats = EECPRMCoastBoatsWanted(this);   // and the coast's boats still to come
            if (theBoats > 0) {
                unsigned long theHave = (unsigned long)this->mManager->mUnitManager->GetCount(this->mManager->GetFishingBoatClass());
                if (theBoats > theHave)
                    theCapacity += theBoats - theHave;
            }
        }
        if (r == inResource)
            return theCapacity;
        theTotal += theCapacity;
    }
    return theTotal;
}

// New (docs/AI_RESEARCH.md §8.21; the personality's economy.demand_split, EE_AI_DEMAND overrides it): the demand
// split. The AI's wants in tiers, each a cost per resource:
//   1  growth: the next citizens (up to 15 of the missing ones), the standing army's shortfall (up to 15 of its
//      favourite land unit), and the first two buildings of every epoch's build queue;
//   2  the pending epoch advance, by its pressure (as the reserve in EEComputerPlayer::CanAfford);
//   3  the rest: the other unit requests, the queued research, the rest of the build queues.
// The demand is tier 1 + tier 2 + kDemandTier3 x tier 3; a resource's shortfall is its demand less its stock. Every
// resource first gets kDemandBase of the personality's floor share of the gatherers (economy.floor: the base income that unqueued
// buildings, upkeep and the next wants need); the rest go by shortfall, plus kDemandFlow of the demand so that a
// resource in demand keeps a flow while its stock covers it. A share past what the claimed resource units hold
// (GetGatherCapacity) goes to the others that have room.
static const float kDemandTier3 = 0.5f;
static const float kDemandFlow = 0.25f;
static const long kDemandNext = 15;
static const float kDemandBase = 0.5f;   // of the personality's floor shares

struct EECPRMDemand {
    long mTier[3][7];
};

static EETechTreeNode* EECPRMClassNode(EETechTree* inTree, unsigned long inClass)
{
    EEUnitClass* theClass = inClass < EEUnitClassCount() ? gUnitClassTable[inClass] : NULL;
    return theClass != NULL ? inTree->Get(theClass->mTechnology) : NULL;
}

static void EECPRMAddCost(EEComputerPlayer* inPlayer, EETechTreeNode* inNode, long inUnitType, float inCount, long* ioCosts)
{
    if (inNode == NULL || !(inCount > 0.0f))
        return;
    for (long r = 2; r < 7; r++)
        ioCosts[r] += UMFloatToLong((float)inPlayer->ScaleCost(inNode->GetCost(r, inUnitType, 1.0f)) * inCount);
}

static void EECPRMGetDemand(EECPResourceManager* inManager, EECPRMDemand& outDemand)
{
    EECPCivilizationManager* theManager = inManager->mManager;
    EEComputerPlayer* thePlayer = theManager->mPlayer;
    EETechTree* theTree = thePlayer->mTechnologies;
    for (long t = 0; t < 3; t++)
        for (long r = 0; r < 7; r++)
            outDemand.mTier[t][r] = 0;
    if (theTree == NULL)
        return;
    EECPUnitManager* theUnits = theManager->mUnitManager;
    if (theUnits != NULL) {
        long theMissing = theUnits->GetWantedCitizens() - (long)theUnits->GetNumWorkers();
        if (theMissing > kDemandNext)
            theMissing = kDemandNext;
        EECPRMAddCost(thePlayer, EECPRMClassNode(theTree, kUnitTypeCitizen), kUnitTypeCitizen, (float)theMissing, outDemand.mTier[0]);
        long theFavorite = thePlayer->mFavoriteLandType;
        long theTarget = UMFloatToLong(EECPPersonality(thePlayer).mStandingArmy * (float)theUnits->GetNumWorkers());
        long theShort = EECPBooming(thePlayer) ? 0 : theTarget - theUnits->CountMilitaryUnits(false, false);
        if (theShort > kDemandNext)
            theShort = kDemandNext;
        if (theFavorite > 0)
            EECPRMAddCost(thePlayer, EECPRMClassNode(theTree, (unsigned long)theFavorite), theFavorite, (float)theShort, outDemand.mTier[0]);
        for (long e = 0; e < EEEpochCount(); e++)
            for (unsigned long k = 0; k < theUnits->mRequests[e].size(); k++) {
                const EECPUMRequest& theRequest = theUnits->mRequests[e][k];
                if (theRequest.mUnitClass != kUnitTypeCitizen)
                    EECPRMAddCost(thePlayer, EECPRMClassNode(theTree, theRequest.mUnitClass), (long)theRequest.mUnitClass,
                                  (float)theRequest.mCount, outDemand.mTier[2]);
            }
    }
    if (theManager->mBuildManager != NULL)
        for (long e = 0; e < EEEpochCount(); e++)
            for (unsigned long k = 0; k < theManager->mBuildManager->mQueues[e].size(); k++) {
                unsigned long theClass = theManager->mBuildManager->mQueues[e][k].first;
                EECPRMAddCost(thePlayer, EECPRMClassNode(theTree, theClass), kUnitTypeTownCenter, 1.0f, outDemand.mTier[k < 2 ? 0 : 2]);
            }
    if (theManager->mResearchManager != NULL)
        for (long e = 0; e < EEEpochCount(); e++)
            for (unsigned long k = 0; k < theManager->mResearchManager->mEpochIDs[e].size(); k++) {
                long theID = theManager->mResearchManager->mEpochIDs[e][k];
                if (!EEIsEpochID(theID))
                    EECPRMAddCost(thePlayer, theTree->Get(theID), kUnitTypeTownCenter, 1.0f, outDemand.mTier[2]);
            }
    long theEpoch = EECPCheatOn(thePlayer, kAICheatEpochs) ? kTechNone : EECPPendingEpoch(thePlayer);
    // the experiment "boatrush": with the Stone Age pending, the wood of two docks and four boats is banked for it
    if (theEpoch != kTechNone && EECPExperiment(thePlayer, "boatrush") && theTree->mCurrentEpoch <= kTechEpochPrehistoric
        && EECPCoastFishingOn(thePlayer))
        outDemand.mTier[1][kResourceWood] += 2 * 175 + 4 * 50;
    if (theEpoch != kTechNone) {
        const EEAIPersonalityRule& thePersonality = EECPPersonality(thePlayer);
        float theShare = (EECPEpochPressure(thePlayer, gPhysics->mGameTime) - thePersonality.mEpochPressureStart)
            / (thePersonality.mEpochPressureFull - thePersonality.mEpochPressureStart);
        if (theShare > 1.0f)
            theShare = 1.0f;
        if (theShare > 0.0f)
            EECPRMAddCost(thePlayer, theTree->Get(theEpoch), kUnitTypeTownCenter, theShare, outDemand.mTier[1]);
    }
    // the experiment "goldbank" (EEComputerPlayer::CanAfford): the next epoch's gold counts whole from the Stone Age
    if (EECPExperiment(thePlayer, "goldbank") && !EECPCheatOn(thePlayer, kAICheatEpochs)
        && theTree->mCurrentEpoch >= kTechEpochStone && theTree->mCurrentEpoch + 1 < EEEpochCount()) {
        long theCosts[7] = {0, 0, 0, 0, 0, 0, 0};
        EECPRMAddCost(thePlayer, theTree->Get(theTree->mCurrentEpoch + 1), kUnitTypeTownCenter, 1.0f, theCosts);
        if (theCosts[kResourceGold] > outDemand.mTier[1][kResourceGold])
            outDemand.mTier[1][kResourceGold] = theCosts[kResourceGold];
    }
    // the experiment "epochplan": the next epoch's wood, stone, gold and iron count whole from the start of an epoch
    // (queued or not, whatever the pressure: those stocks take long to fill with few gatherers, and only food
    // competes with the citizens); the pressure share above keeps the food
    if (EECPExperiment(thePlayer, "epochplan") && !EECPCheatOn(thePlayer, kAICheatEpochs)) {
        long theNext = theTree->mCurrentEpoch + 1;
        EETechTreeNode* theNode = theNext < EEEpochCount() ? theTree->Get(theNext) : NULL;
        if (theNode != NULL) {
            long theCosts[7] = {0, 0, 0, 0, 0, 0, 0};
            EECPRMAddCost(thePlayer, theNode, kUnitTypeTownCenter, 1.0f, theCosts);
            for (long r = 3; r < 7; r++)
                if (theCosts[r] > outDemand.mTier[1][r])
                    outDemand.mTier[1][r] = theCosts[r];
        }
    }
}

// the experiment "goldfix" (docs/AI_RESEARCH.md §8.27): when the next epoch costs stone, gold or iron that the stock
// does not cover and no active resource centre holds any, the nearest centre to the town centre that does is
// activated (its gatherers then order their settlement); the original activated centres only by distance at the
// start, so an AI whose near gold ran out never had gold again
// economy.forage_patches: a settlement right on each forage patch of the active centres within the claim range that has
// no drop site at hand, one at a time (every kSettleMs), before anyone gathers there
static const unsigned long kSettleMs = 20000;

static void EECPRMSettlePatches(EECPResourceManager* inManager)
{
    static unsigned long sLast[64];
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    EECPBuildManager* theBuild = inManager->mManager->mBuildManager;
    unsigned long theID = thePlayer->mPlayerID < 64 ? thePlayer->mPlayerID : 63;
    if (theBuild == NULL || !thePlayer->mCanBuild || !inManager->mSettlementsEnabled
        || !thePlayer->CanProduceUnitType(kUnitTypeSettlement, 0x20, NULL))
        return;
    if (sLast[theID] != 0 && gPhysics->mGameTime >= sLast[theID] && gPhysics->mGameTime - sLast[theID] < kSettleMs)
        return;
    if (inManager->IsSettlementQueued(kResourceFood) || inManager->mManager->mUnitManager->HasUnfinished(kUnitTypeSettlement))
        return;
    EEUnit* theBest = NULL;
    long long theBestDistance = 0;
    for (unsigned long k = 0; k < thePlayer->GetNumResourceCenters(); k++) {
        EECPResourceCenter* theCenter = thePlayer->GetResourceCenter(k);
        if (theCenter == NULL || !theCenter->mIsActive)
            continue;
        // the forage patch, and the worked stone, gold and iron mines (a settlement at each, as at the patches)
        for (long r = kResourceFood; r <= kResourceIron; r++) {
            if (r == kResourceWood)
                continue;
            EEUnit* theUnit = theCenter->FindResourceUnit(r);
            if (theUnit == NULL || !(theUnit->mHitPoints > 0) || !(theUnit->mResourceAmount > 0)
                || (r == kResourceFood && theUnit->GetUnitClass()->mUnitType != kUnitTypeResForageBush)
                || (r != kResourceFood && gWorld.GetResourceUsers(theUnit) == 0)
                || !EECPRMInClaimRange(inManager, theUnit->GetTileX(), theUnit->GetTileY())
                || EECPRMDropNear(thePlayer, theUnit->GetTileX(), theUnit->GetTileY(), kEECPPatchDropRange, r == kResourceFood)
                || theBuild->HasEnemyNear(theUnit->GetTileX(), theUnit->GetTileY())
                || EECPRMResourceInDanger(inManager, theUnit->GetTileX(), theUnit->GetTileY()))
                continue;
            long long theDX = theUnit->GetTileX() - theBuild->mTownCenterTile.mX;
            long long theDY = theUnit->GetTileY() - theBuild->mTownCenterTile.mY;
            if (theBest == NULL || theDX * theDX + theDY * theDY < theBestDistance) {
                theBest = theUnit;
                theBestDistance = theDX * theDX + theDY * theDY;
            }
        }
    }
    if (theBest == NULL)
        return;
    EETileCoordinate theSite;
    if (!EECPRMPatchSite(inManager, theBest, theSite)) {
        EECPClaimExpand(thePlayer);   // no room by the patch: claim more land
        return;
    }
    sLast[theID] = gPhysics->mGameTime;
    theBuild->QueueBuildingAt(kUnitTypeSettlement, theSite.mX, theSite.mY);
    EE_CP_TRACE(thePlayer, "settlement.patch", "x=%ld y=%ld patch=%ld/%ld distance=%.0f", (long)theSite.mX, (long)theSite.mY,
                (long)theBest->GetTileX(), (long)theBest->GetTileY(), sqrt((double)theBestDistance));
}

// economy.forage_patches: a tower for each worked site (a patch or mine with gatherers) within the claim range that has
// none within kGuardRange tiles, one at a time (every kGuardMs); FindTowerSite puts it at the nearest unguarded centre
// (the owner: "undefended resources, no tower"; towers are strong until the Bronze Age)
static const unsigned long kGuardMs = 30000;
static const float kGuardRange = 8.0f;

static void EECPRMGuardSites(EECPResourceManager* inManager)
{
    static unsigned long sLast[64];
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    EECPBuildManager* theBuild = inManager->mManager->mBuildManager;
    unsigned long theID = thePlayer->mPlayerID < 64 ? thePlayer->mPlayerID : 63;
    if (theBuild == NULL || !thePlayer->mCanBuild)
        return;
    // the experiment "towerdef" (docs/AI_RESEARCH.md §8.29; towers cost stone, which nothing else in the Stone Age
    // needs): from the Stone Age every active site in range is guarded, worked or not, and twice as often
    bool theMore = EECPExperiment(thePlayer, "towerdef") && thePlayer->mTechnologies != NULL
        && thePlayer->mTechnologies->mCurrentEpoch >= kTechEpochStone;
    if (sLast[theID] != 0 && gPhysics->mGameTime >= sLast[theID]
        && gPhysics->mGameTime - sLast[theID] < (theMore ? kGuardMs / 2 : kGuardMs))
        return;
    long theTower = inManager->mManager->GetTowerClass();
    if (theTower <= 0 || !thePlayer->CanProduceUnitType(theTower, 0x20, NULL) || theBuild->IsQueued(theTower)
        || inManager->mManager->mUnitManager->HasUnfinished(theTower))
        return;
    for (unsigned long k = 0; k < thePlayer->GetNumResourceCenters(); k++) {
        EECPResourceCenter* theCenter = thePlayer->GetResourceCenter(k);
        if (theCenter == NULL || !theCenter->mIsActive)
            continue;
        for (long r = kResourceFood; r <= kResourceIron; r++) {
            EEUnit* theUnit = theCenter->FindResourceUnit(r);
            if (theUnit == NULL || !(theUnit->mHitPoints > 0) || (gWorld.GetResourceUsers(theUnit) == 0 && !theMore)
                || !EECPRMInClaimRange(inManager, theUnit->GetTileX(), theUnit->GetTileY())
                || theBuild->HasTowerNear(theUnit->GetTileX(), theUnit->GetTileY(), kGuardRange * kGuardRange))
                continue;
            sLast[theID] = gPhysics->mGameTime;
            theBuild->QueueBuilding(theTower, false, true);
            EE_CP_TRACE(thePlayer, "tower.guard", "resource=%ld x=%ld y=%ld", r, (long)theUnit->GetTileX(), (long)theUnit->GetTileY());
            return;
        }
    }
}

static void EECPRMActivateNeeded(EECPResourceManager* inManager)
{
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    EECPBuildManager* theBuild = inManager->mManager->mBuildManager;
    if (thePlayer->mTechnologies == NULL || theBuild == NULL || theBuild->mTownCenterTile.mX == -10000)
        return;
    // the experiment "foodeco": at least kPatchesWanted forage patches in the active centres (the owner works 2 with
    // 6 each and hunts with the rest); the original's active patches ran out over the game (5.4 held at 5-10 min,
    // 2.4 at 75-90 in the sim) and none was claimed again
    if (EECPForagePatchesOn(thePlayer)) {
        unsigned long thePatches = 0;
        for (unsigned long k = 0; k < thePlayer->GetNumResourceCenters(); k++) {
            EECPResourceCenter* theCenter = thePlayer->GetResourceCenter(k);
            if (theCenter == NULL || !theCenter->mIsActive)
                continue;
            for (unsigned long u = 0; u < theCenter->mUnits.size(); u++)
                if (theCenter->mUnits[u]->mResourceType == kResourceFood && theCenter->mUnits[u]->mHitPoints > 0
                    && theCenter->mUnits[u]->mResourceAmount > 0
                    && theCenter->mUnits[u]->GetUnitClass()->mUnitType == kUnitTypeResForageBush)
                    thePatches++;
        }
        unsigned long theWantedPatches = thePlayer->mTechnologies->mCurrentEpoch <= kTechEpochPrehistoric ? kPatchesWanted : kPatchesWantedLater;
        if (EECPClaimRangeOn(thePlayer)) {
            if (!EECPExperiment(thePlayer, "nosettle"))   // TEST ONLY: the patch settlements off
                EECPRMSettlePatches(inManager);
            if (!EECPExperiment(thePlayer, "noguard"))    // TEST ONLY: the site towers off
                EECPRMGuardSites(inManager);
        }
        if (thePatches < theWantedPatches) {
            EECPResourceCenter* theBest = NULL;
            long long theBestDistance = 0;
            for (unsigned long k = 0; k < thePlayer->GetNumResourceCenters(); k++) {
                EECPResourceCenter* theCenter = thePlayer->GetResourceCenter(k);
                if (theCenter == NULL || theCenter->mIsActive)
                    continue;
                EEUnit* theUnit = theCenter->FindResourceUnit(kResourceFood);
                if (theUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->GetUnitClass()->mUnitType != kUnitTypeResForageBush
                    || EECPRMResourceInDanger(inManager, theUnit->GetTileX(), theUnit->GetTileY())
                    || (EECPClaimRangeOn(thePlayer) && !EECPRMInClaimRange(inManager, theUnit->GetTileX(), theUnit->GetTileY())))
                    continue;
                long long theDX = theUnit->GetTileX() - theBuild->mTownCenterTile.mX;
                long long theDY = theUnit->GetTileY() - theBuild->mTownCenterTile.mY;
                if (theBest == NULL || theDX * theDX + theDY * theDY < theBestDistance) {
                    theBest = theCenter;
                    theBestDistance = theDX * theDX + theDY * theDY;
                }
            }
            if (theBest != NULL) {
                theBest->SetActive(true, 0);
                EE_CP_TRACE(thePlayer, "center.activate", "resource=%ld distance=%.0f patches=%lu", (long)kResourceFood,
                            sqrt((double)theBestDistance), thePatches);
            } else if (EECPPatchExpandOn(thePlayer) && EECPClaimRangeOn(thePlayer) && thePatches == 0
                       && thePlayer->mTechnologies->mCurrentEpoch < kTechEpochCopper)
                // economy.patch_expand (§8.29): before the farms (the Copper Age) the patches are the food; with
                // no patch worked and none left in the claim range it grows (growing whenever fewer than the wanted
                // patches were held sent the citizens far out: 14-33 with it against 19-26 without) (the playtest AI had no food gatherer for 45 minutes, 1399 of
                // the Copper Age's 1584 food, and the claim never grew: only a farm or settlement without room grew it)
                EECPClaimExpand(thePlayer);
        }
    }
    if (!EECPClaimNeededOn(thePlayer))
        return;
    long theNext = thePlayer->mTechnologies->mCurrentEpoch + 1;
    EETechTreeNode* theNode = theNext < EEEpochCount() ? thePlayer->mTechnologies->Get(theNext) : NULL;
    if (theNode == NULL)
        return;
    for (long r = kResourceStone; r <= kResourceIron; r++) {
        if (thePlayer->ScaleCost(theNode->GetCost(r, kUnitTypeTownCenter, 1.0f)) <= thePlayer->mResourceCaps[r]
            || inManager->GetGatherCapacity(r) > 0)
            continue;
        EECPResourceCenter* theBest = NULL;
        long long theBestDistance = 0;
        for (unsigned long k = 0; k < thePlayer->GetNumResourceCenters(); k++) {
            EECPResourceCenter* theCenter = thePlayer->GetResourceCenter(k);
            if (theCenter == NULL || theCenter->mIsActive)
                continue;
            EEUnit* theUnit = theCenter->FindResourceUnit(r);
            if (theUnit == NULL || !(theUnit->mHitPoints > 0) || EECPRMResourceInDanger(inManager, theUnit->GetTileX(), theUnit->GetTileY())
                || (EECPClaimRangeOn(thePlayer) && !EECPRMInClaimRange(inManager, theUnit->GetTileX(), theUnit->GetTileY())))
                continue;
            long long theDX = theUnit->GetTileX() - theBuild->mTownCenterTile.mX;
            long long theDY = theUnit->GetTileY() - theBuild->mTownCenterTile.mY;
            if (theBest == NULL || theDX * theDX + theDY * theDY < theBestDistance) {
                theBest = theCenter;
                theBestDistance = theDX * theDX + theDY * theDY;
            }
        }
        if (theBest != NULL) {
            theBest->SetActive(true, 0);
            EE_CP_TRACE(thePlayer, "center.activate", "resource=%ld distance=%.0f", r, sqrt((double)theBestDistance));
        }
    }
}

static bool EECPRMSplitByDemand(EECPResourceManager* inManager)
{
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    EECPRMActivateNeeded(inManager);
    // the experiment "boatrush": the boats are asked for on every split, not only while a citizen stands idle
    if ((EECPExperiment(thePlayer, "boatrush") || EECPExperiment(thePlayer, "boatrush2"))
        && EECPRMCoastBoatsWanted(inManager) > 0)
        EECPRMRequestBoats(inManager);
    long theGatherers = (long)inManager->mGatherers[0].size();
    for (long r = 2; r < 7; r++)
        theGatherers += (long)inManager->mGatherers[r].size();
    EECPRMDemand theDemand;
    EECPRMGetDemand(inManager, theDemand);
    bool theNoFood = thePlayer->mTechnologies->mCurrentEpoch > kTechEpochNano;
    float theWeight[7] = {0, 0, 0, 0, 0, 0, 0};
    float theTotal = 0.0f;
    long theShortfall[7] = {0, 0, 0, 0, 0, 0, 0};
    for (long r = 2; r < 7; r++) {
        if (r == 2 && theNoFood)
            continue;
        float theWant = (float)(theDemand.mTier[0][r] + theDemand.mTier[1][r]) + kDemandTier3 * (float)theDemand.mTier[2][r];
        float theShort = theWant - (float)thePlayer->mResourceCaps[r];
        theShortfall[r] = theShort > 0.0f ? UMFloatToLong(theShort) : 0;
        theWeight[r] = (theShort > 0.0f ? theShort : 0.0f) + kDemandFlow * theWant;
        theTotal += theWeight[r];
    }
    unsigned long theWanted[7] = {0, 0, 0, 0, 0, 0, 0};
    if (theGatherers > 0) {
        unsigned long theRoom[7] = {0, 0, 0, 0, 0, 0, 0};
        long theLeft = theGatherers;
        const EEAIPersonalityRule& theTuning = EECPPersonality(thePlayer);
        float theFloorSum = 0.0f;
        for (long r = 2; r < 7; r++)
            if (!(r == 2 && theNoFood))
                theFloorSum += kDemandBase * theTuning.mFloor[r - 2];
        if (theFloorSum > 1.0f)
            theFloorSum = 1.0f;
        for (long r = 2; r < 7; r++) {
            theRoom[r] = inManager->GetGatherCapacity(r);
            float theBase = r == 2 && theNoFood ? 0.0f : kDemandBase * theTuning.mFloor[r - 2];
            unsigned long theShare = (unsigned long)UMFloatToLong((theBase + (theTotal > 0.0f ? (1.0f - theFloorSum) * theWeight[r] / theTotal : 0.0f)) * (float)theGatherers);
            if (theShare > theRoom[r])
                theShare = theRoom[r];
            if (theShare == 0 && theWeight[r] > 0.0f && theRoom[r] > 0)
                theShare = 1;
            theWanted[r] = theShare;
            theLeft -= (long)theShare;
        }
        // what the capacity caps left over: to the resources in demand with room, the heaviest first, one at a time
        while (theLeft > 0) {
            long theBest = -1;
            for (long r = 2; r < 7; r++)
                if (theWeight[r] > 0.0f && theWanted[r] < theRoom[r]
                    && (theBest < 0 || theWeight[r] / (float)(theWanted[r] + 1) > theWeight[theBest] / (float)(theWanted[theBest] + 1)))
                    theBest = r;
            if (theBest < 0)
                break;
            theWanted[theBest]++;
            theLeft--;
        }
    }
    bool theChanged = false;
    for (long r = 2; r < 7; r++)
        if (theWanted[r] != inManager->mWantedGatherers[r]) {
            inManager->mWantedGatherers[r] = theWanted[r];
            theChanged = true;
        }
    if (gCPTraceOn && gPhysics->mGameTime % 30000 < 5000)
        EE_CP_TRACE(thePlayer, "demand", "t1=%ld/%ld/%ld/%ld/%ld t2=%ld/%ld/%ld/%ld/%ld t3=%ld/%ld/%ld/%ld/%ld short=%ld/%ld/%ld/%ld/%ld "
            "wanted=%lu/%lu/%lu/%lu/%lu of=%ld",
            theDemand.mTier[0][2], theDemand.mTier[0][3], theDemand.mTier[0][4], theDemand.mTier[0][5], theDemand.mTier[0][6],
            theDemand.mTier[1][2], theDemand.mTier[1][3], theDemand.mTier[1][4], theDemand.mTier[1][5], theDemand.mTier[1][6],
            theDemand.mTier[2][2], theDemand.mTier[2][3], theDemand.mTier[2][4], theDemand.mTier[2][5], theDemand.mTier[2][6],
            theShortfall[2], theShortfall[3], theShortfall[4], theShortfall[5], theShortfall[6],
            theWanted[2], theWanted[3], theWanted[4], theWanted[5], theWanted[6], theGatherers);
    return theChanged;
}

// FUNCTION: EXE 0x491b78
// Splits the gatherers over the resources by their needs, capped by the resource units available; returns true
// if any wanted count changed
bool EECPResourceManager::UpdateWantedGatherers()
{
    if (EECPDemandSplitOn(this->mManager->mPlayer))   // economy.demand_split (EE_AI_DEMAND): the demand split above
        return EECPRMSplitByDemand(this);
    long theTotal = 0;
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    unsigned long theLimit = thePlayer->mMiningLimit;
    long theGatherers = 0;
    long theLowest = 100000;
    unsigned long theFound = 0;
    bool theChanged = false;
    long i;
    for (i = 2; i < 7; i++)
        if (this->mNeeds[i] <= theLowest)
            theLowest = this->mNeeds[i];
    if (theLowest < 0)
        for (i = 2; i < 7; i++)
            if (this->mNeeds[i] != 0)
                this->mNeeds[i] -= theLowest;
    for (i = 2; i < 7; i++) {
        theGatherers += this->mGatherers[i].size();
        theTotal += this->mNeeds[i];
    }
    theGatherers += this->mGatherers[0].size();
    if (theGatherers > 0 && theTotal > 0) {
        for (unsigned long j = 2; j < 7; j++) {
            long theNeed = this->mNeeds[j];
            if (theNeed >= 0) {
                unsigned long theWanted = (unsigned long)UMFloatToLong((float)theNeed / (float)theTotal * (float)theGatherers);
                if (j == 2) {
                    if (thePlayer->mTechnologies->mCurrentEpoch > kTechEpochNano) {
                        theWanted = 0;
                        theNeed = 0;
                    }
                } else if (j != 3) {
                    theFound = 0;   // this resource's units only (was carried over from the earlier resources)
                    for (unsigned long k = 0; k < thePlayer->GetNumResourceCenters(); k++) {
                        std::vector<EEUnit*> theUnits = thePlayer->GetResourceCenter(k)->mUnits;
                        for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++)
                            if ((*it)->mResourceType == j)
                                theFound++;
                    }
                    if (theFound * theLimit < theWanted)
                        theWanted = theFound * theLimit;
                }
                if (theNeed > 0 && theWanted <= 1)
                    theWanted = 1;
                if (theWanted != this->mWantedGatherers[j]) {
                    this->mWantedGatherers[j] = theWanted;
                    theChanged = true;
                }
            } else {
                this->mWantedGatherers[j] = 0;
                if (this->mGatherers[j].size() != 0)
                    theChanged = true;
            }
        }
    }
    // new: the stock correction (the personality's economy; see above). A standing army short of its target
    // (EEComputerPlayer::KeepStandingArmy) raises the floor of what its unit costs by up to the personality's
    // army_floor of the gatherers, split by the unit's cost (docs/AI_RESEARCH.md §8.15-8.16: the army waited on iron
    // while food took 70%)
    const float kArmyFloor = EECPPersonality(thePlayer).mArmyFloor;
    float theArmyFloor[7] = {0, 0, 0, 0, 0, 0, 0};
    if (theGatherers > 0 && this->mManager->mUnitManager != NULL) {
        long theFavorite = thePlayer->mFavoriteLandType;
        EEUnitClass* theClass = theFavorite > 0 && (unsigned long)theFavorite < EEUnitClassCount() ? gUnitClassTable[theFavorite] : NULL;
        EETechTreeNode* theNode = theClass != NULL ? thePlayer->mTechnologies->Get(theClass->mTechnology) : NULL;
        long theTarget = UMFloatToLong(EECPPersonality(thePlayer).mStandingArmy * (float)this->mManager->mUnitManager->GetNumWorkers());
        long theArmy = this->mManager->mUnitManager->CountMilitaryUnits(false, false);
        if (theNode != NULL && theTarget > 0 && theArmy < theTarget) {
            float theShort = 1.0f - (float)theArmy / (float)theTarget;
            float theCost[7] = {0, 0, 0, 0, 0, 0, 0};
            float theSum = 0.0f;
            for (long r = 2; r < 7; r++) {
                theCost[r] = (float)thePlayer->ScaleCost(theNode->GetCost(r, theFavorite, 1.0f));
                theSum += theCost[r];
            }
            if (theSum > 0.0f)
                for (long r = 2; r < 7; r++)
                    theArmyFloor[r] = kArmyFloor * theShort * theCost[r] / theSum;
        }
    }
    if (theGatherers > 0) {
        for (unsigned long r = 2; r < 7; r++) {
            unsigned long theWanted = this->mWantedGatherers[r];
            long theStock = thePlayer->mResourceCaps[r];
            const EEAIPersonalityRule& theTuning = EECPPersonality(thePlayer);
            if (theStock > theTuning.mSurplus)
                theWanted = (unsigned long)UMFloatToLong(theWanted * theTuning.mSurplusCut);
            else if (theStock < theTuning.mLowStock) {
                unsigned long theFloor = (unsigned long)UMFloatToLong((theTuning.mFloor[r - 2] + theArmyFloor[r]) * (float)theGatherers);
                if (r == 2 && thePlayer->mTechnologies->mCurrentEpoch > kTechEpochNano)
                    theFloor = 0;
                if (theWanted < theFloor)
                    theWanted = theFloor;
            }
            if (theWanted != this->mWantedGatherers[r]) {
                this->mWantedGatherers[r] = theWanted;
                theChanged = true;
            }
        }
    }
    return theChanged;
}

// New: frees up to the personality's moves_per_cycle gatherers of resources with more than one over their wanted count while
// another resource is short (the original only placed idle citizens, so a split once made never changed); AssignGatherers
// places them on its next pass
void EECPResourceManager::RebalanceGatherers()
{
    // New (docs/AI_RESEARCH.md §8.11): a citizen that finished its building keeps the build goal and waits in the
    // citizen script's TaskSmartCitizen state for good; the AI takes only goal-less or gathering citizens, so these
    // were lost (11 of 30 citizens at 20 minutes). They are freed for this pass.
    std::vector<EEComplexUnit*> theCitizens;
    if (this->mManager->mUnitManager != NULL)
        this->mManager->mUnitManager->GetCitizens(theCitizens);
    unsigned long theFreed = 0;
    for (unsigned long i = 0; i < theCitizens.size(); i++) {
        EEComplexUnit* theUnit = theCitizens[i];
        EEUGBuildRepair* theGoal = dynamic_cast<EEUGBuildRepair*>(theUnit->mActiveGoal);
        if (theGoal == NULL || theUnit->mIsDetachedFromAI || theUnit->mIsDisabled || !(theUnit->mHitPoints > 0)
            || (theUnit->mCommand ? theUnit->mCommand->mType : 0) != kActionTaskSmartCitizen)
            continue;
        EEComplexUnit* theTarget = theGoal->mTarget != NULL ? theGoal->mTarget->mComplexUnit : NULL;
        if (theTarget != NULL && !theTarget->mIsBuilt)
            continue;
        theUnit->ClearGoalQueue(0);
        theUnit->StartAction(kActionIdle);
        theUnit->GetUnitClass()->ActionChanged(theUnit);
        this->AddIdleGatherer(theUnit);
        theFreed++;
    }
    if (theFreed > 0)
        EE_CP_TRACE(this->mManager->mPlayer, "citizen.freed", "n=%lu", theFreed);

    bool theShort = false;
    for (long r = 2; r < 7; r++)
        if (this->mGatherers[r].size() < this->mWantedGatherers[r])
            theShort = true;
    if (!theShort)
        return;
    unsigned long theMoves = 0;
    unsigned long theMaxMoves = (unsigned long)EECPPersonality(this->mManager->mPlayer).mMovesPerCycle;
    for (long r = 2; r < 7 && theMoves < theMaxMoves; r++) {
        while (theMoves < theMaxMoves && this->mGatherers[r].size() > this->mWantedGatherers[r] + 1) {
            EEComplexUnit* theUnit = this->mGatherers[r].back();
            if (theUnit->mIsDisabled || theUnit->mIsDetachedFromAI || theUnit->mCitizenBuildQueue.size() != 0
                || theUnit->GetUnitClass()->mCategory == kUnitCategoryNavalCitizen)
                break;
            theUnit->Reference();
            this->AddIdleGatherer(theUnit);   // off this resource's list, onto the idle list
            theUnit->ClearGoalQueue(0);
            theUnit->StartAction(kActionIdle);
            theUnit->GetUnitClass()->ActionChanged(theUnit);
            EE_CP_TRACE(this->mManager->mPlayer, "gather.move", "from=%ld unit=%lu", r, theUnit->GetUnitID());
            theUnit->Dereference();
            theMoves++;
        }
    }
}

// New (docs/AI_RESEARCH.md §8.22): gatherers run from a harassing raid. Where a human player's unit (a computer
// player's only with the experiment "flee") hits a citizen (EEComputerPlayer::DamageTaken -> EECPRMNoteDanger) is a danger zone for
// kDangerMs: the gatherers within kFleeRange tiles of it go back towards the town centre and onto the idle list, and
// FindResourceTarget sends nobody to a resource within kDangerRange tiles of it until it expires.
static const long kFleeRange = 8;
static const long kDangerRange = 10;
static const unsigned long kDangerMs = 30000;
static const long kHomeRange = 12;
static const long kMaxRaiders = 2;
static const long kDefendRange = 30;
static const unsigned long kDefenders = 4;

static bool EECPRMInDanger(EECPResourceManager* inManager, long inX, long inY, long inRange);

// New (docs/AI_RESEARCH.md §8.25; aipersonalities.yaml military.militia, EE_AI_EXP "nomilitia" turns it off): the town's answer to an attack group on its citizens
// when no army does it. The citizens within kMilitiaRange tiles of the hit one, with the soldiers within
// kDefendRange that are not attacking, fight when their strength (summed hit points x summed attack) is at least
// kMilitiaOdds times the attackers' within kMilitiaRange; else the citizens run to the town centre, or when the
// attack is there to the drop site farthest from it. At most once every kMilitiaMs per player.
static const long kMilitiaRange = 10;
static const long kMilitiaTowerRange = 12;   // military.militia_towers
static const float kMilitiaOdds = 1.5f;
static const unsigned long kMilitiaMs = 4000;

static bool EECPRMIsSoldier(EEUnit* inUnit)
{
    if (inUnit->mComplexUnit == NULL || !(inUnit->mHitPoints > 0) || inUnit->mStats->mAttack == 0.0f
        || inUnit->mUnitClass->IsLandBuilding())
        return false;
    long theCategory = inUnit->mUnitClass->mCategory;
    return theCategory == kUnitCategoryLand || theCategory == kUnitCategoryLandMilitaryHero;
}

static bool EECPRMNear(EEUnit* inUnit, long inX, long inY, long inRange)
{
    long theDX = inUnit->GetTileX() - inX, theDY = inUnit->GetTileY() - inY;
    return theDX * theDX + theDY * theDY <= inRange * inRange;
}

static void EECPRMMilitia(EECPResourceManager* inManager, EEUnit* inCitizen, unsigned long inAttacker)
{
    static unsigned long sLast[64];
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    unsigned long theID = thePlayer->mPlayerID < 64 ? thePlayer->mPlayerID : 63;
    if (sLast[theID] != 0 && gPhysics->mGameTime >= sLast[theID] && gPhysics->mGameTime - sLast[theID] < kMilitiaMs)
        return;
    sLast[theID] = gPhysics->mGameTime;
    long theX = inCitizen->GetTileX(), theY = inCitizen->GetTileY();
    std::vector<EEComplexUnit*> theRaiders;
    float theRaiderHP = 0.0f, theRaiderAttack = 0.0f;
    for (EEUnit* theUnit = gWorld.mPlayers[inAttacker]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (EECPRMIsSoldier(theUnit) && EECPRMNear(theUnit, theX, theY, kMilitiaRange)) {
            theRaiders.push_back(theUnit->mComplexUnit);
            theRaiderHP += (float)theUnit->mHitPoints;
            theRaiderAttack += theUnit->mStats->mAttack;
        }
    if (theRaiders.empty())
        return;
    std::vector<EEComplexUnit*> theCitizens, theSoldiers;
    float theHP = 0.0f, theAttack = 0.0f;
    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (theComplex == NULL || !(theUnit->mHitPoints > 0) || theComplex->mIsDisabled || theComplex->mIsDetachedFromAI
            || theComplex->mContainer != NULL)
            continue;
        if (theUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen && EECPRMNear(theUnit, theX, theY, kMilitiaRange)
            && theComplex->mCitizenBuildQueue.size() == 0) {
            theCitizens.push_back(theComplex);
            theHP += (float)theUnit->mHitPoints;
            theAttack += theUnit->mStats->mAttack;
        } else if (EECPRMIsSoldier(theUnit) && EECPRMNear(theUnit, theX, theY, kDefendRange)
                   && (theComplex->mActiveGoal == NULL || theComplex->mActiveGoal->mKind != kGoalAttackUnit)) {
            theSoldiers.push_back(theComplex);
            theHP += (float)theUnit->mHitPoints;
            theAttack += theUnit->mStats->mAttack;
        }
    }
    // military.militia_towers (docs/AI_RESEARCH.md §8.26; the owner: towers are strong until the Bronze Age, a boom
    // lives behind them): the own built towers within kMilitiaTowerRange tiles count in the defence, and a raid the
    // town cannot fight sends the citizens to the nearest tower within kDefendRange instead of the far drop site
    bool theTowerRule = EECPMilitiaTowersOn(thePlayer);
    EEUnit* theTower = NULL;
    if (theTowerRule) {
        long theBest = -1;
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mComplexUnit == NULL || !theUnit->mUnitClass->mIsTower || !(theUnit->mHitPoints > 0)
                || !theUnit->mComplexUnit->mIsBuilt || theUnit->mStats->mAttack == 0.0f)
                continue;
            if (EECPRMNear(theUnit, theX, theY, kMilitiaTowerRange)) {
                theHP += (float)theUnit->mHitPoints;
                theAttack += theUnit->mStats->mAttack;
            }
            long theDX = theUnit->GetTileX() - inCitizen->GetTileX(), theDY = theUnit->GetTileY() - inCitizen->GetTileY();
            long theDistance = theDX * theDX + theDY * theDY;
            if (theDistance <= kDefendRange * kDefendRange && (theBest < 0 || theDistance < theBest)) {
                theBest = theDistance;
                theTower = theUnit;
            }
        }
    }
    bool theFight = theHP * theAttack >= kMilitiaOdds * theRaiderHP * theRaiderAttack;
    if (theFight) {
        // the soldiers first, then only as many of the nearest citizens as the odds need
        std::vector<EEComplexUnit*> theAll(theSoldiers);
        float theNeedHP = 0.0f, theNeedAttack = 0.0f;
        for (unsigned long i = 0; i < theSoldiers.size(); i++) {
            theNeedHP += (float)theSoldiers[i]->mHitPoints;
            theNeedAttack += theSoldiers[i]->mStats->mAttack;
        }
        std::vector<std::pair<long, EEComplexUnit*> > theByDistance;
        for (unsigned long i = 0; i < theCitizens.size(); i++) {
            long theDX = theCitizens[i]->GetTileX() - theX, theDY = theCitizens[i]->GetTileY() - theY;
            theByDistance.push_back(std::pair<long, EEComplexUnit*>(theDX * theDX + theDY * theDY, theCitizens[i]));
        }
        std::stable_sort(theByDistance.begin(), theByDistance.end(),
            [](const std::pair<long, EEComplexUnit*>& x, const std::pair<long, EEComplexUnit*>& y) { return x.first < y.first; });
        theCitizens.clear();
        for (unsigned long i = 0; i < theByDistance.size()
             && theNeedHP * theNeedAttack < kMilitiaOdds * theRaiderHP * theRaiderAttack; i++) {
            theCitizens.push_back(theByDistance[i].second);
            theAll.push_back(theByDistance[i].second);
            theNeedHP += (float)theByDistance[i].second->mHitPoints;
            theNeedAttack += theByDistance[i].second->mStats->mAttack;
        }
        for (unsigned long i = 0; i < theAll.size(); i++) {
            EEComplexUnit* theUnit = theAll[i];
            EEComplexUnit* theTarget = NULL;
            long theBest = 0;
            for (unsigned long r = 0; r < theRaiders.size(); r++) {
                long theDX = theRaiders[r]->GetTileX() - theUnit->GetTileX(), theDY = theRaiders[r]->GetTileY() - theUnit->GetTileY();
                long theDistance = theDX * theDX + theDY * theDY;
                if (theTarget == NULL || theDistance < theBest) {
                    theTarget = theRaiders[r];
                    theBest = theDistance;
                }
            }
            theUnit->Reference();
            if (theUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen)
                inManager->AddIdleGatherer(theUnit);   // off its resource's list: reassigned once the fight is over
            theUnit->ClearGoalQueue(0);
            theUnit->SetGoal(new EEUGAttackUnit(theUnit, theTarget, false, false), 0, 0);
            theUnit->StartAction(kActionPrepareToMove);
            theUnit->GetUnitClass()->ActionChanged(theUnit);
            theUnit->Dereference();
        }
    } else {
        EECPBuildManager* theBuildManager = inManager->mManager->mBuildManager;
        bool theHaveHome = theBuildManager != NULL && theBuildManager->mTownCenterTile.mX != -10000;
        GE2DPoint theHome(0.0f, 0.0f);
        bool theRun = false;
        if (theTower != NULL && !EECPRMNear(theTower, theX, theY, 3)) {
            theHome = GE2DPoint((float)theTower->GetTileX(), (float)theTower->GetTileY());
            theRun = true;
        } else if (theHaveHome && !EECPRMNear(inCitizen, theBuildManager->mTownCenterTile.mX, theBuildManager->mTownCenterTile.mY, kHomeRange)) {
            theHome = GE2DPoint((float)theBuildManager->mTownCenterTile.mX, (float)theBuildManager->mTownCenterTile.mY);
            theRun = true;
        } else {
            // the attack is at the town centre: to the drop site farthest from it
            EEComplexUnit* theRefuge = NULL;
            long theFarthest = -1;
            for (unsigned long c = 0; c < thePlayer->GetNumResourceCenters(); c++) {
                EECPResourceCenter* theCenter = thePlayer->GetResourceCenter(c);
                if (theCenter == NULL || theCenter->mUnit == NULL || !theCenter->mUnit->mUnitClass->IsLandBuilding()
                    || !(theCenter->mUnit->mHitPoints > 0))
                    continue;
                long theDX = theCenter->mUnit->GetTileX() - theX, theDY = theCenter->mUnit->GetTileY() - theY;
                long theDistance = theDX * theDX + theDY * theDY;
                if (theDistance > theFarthest) {
                    theFarthest = theDistance;
                    theRefuge = theCenter->mUnit;
                }
            }
            if (theRefuge != NULL && theFarthest > kMilitiaRange * kMilitiaRange) {
                theHome = GE2DPoint((float)theRefuge->GetTileX(), (float)theRefuge->GetTileY());
                theRun = true;
            }
        }
        if (!theRun)
            theCitizens.clear();   // nowhere to run: they keep working
        for (unsigned long i = 0; i < theCitizens.size(); i++) {
            EEComplexUnit* theUnit = theCitizens[i];
            theUnit->Reference();
            inManager->AddIdleGatherer(theUnit);
            theUnit->ClearGoalQueue(0);
            theUnit->SetGoal(new EEUGLocation(theHome, true, true, false, 0.0f, false, false), 0, 0);
            theUnit->StartAction(kActionPrepareToMove);
            theUnit->GetUnitClass()->ActionChanged(theUnit);
            theUnit->Dereference();
        }
        if (!theCitizens.empty())
            inManager->mDangerZones.push_back(std::pair<U2DPoint<long>, unsigned long>(U2DPoint<long>(theX, theY), gPhysics->mGameTime));
    }
    EE_CP_TRACE(thePlayer, "militia", "%s citizens=%lu soldiers=%lu raiders=%lu odds=%.2f tower=%d", theFight ? "fight" : "run",
                (unsigned long)theCitizens.size(), (unsigned long)theSoldiers.size(), (unsigned long)theRaiders.size(),
                theRaiderHP * theRaiderAttack > 0.0f ? theHP * theAttack / (theRaiderHP * theRaiderAttack) : 0.0f, theTower != NULL ? 1 : 0);
}

static const long kNavalFleeRange = 12;      // the own units round the victim that leave with it
static const unsigned long kNavalFleeMs = 4000;

void EECPRMNavalFlee(EECPResourceManager* inManager, EEUnit* inVictim, EEComplexUnit* inAttacker)
{
    static unsigned long sLast[64];
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    if (inAttacker == NULL || !(inAttacker->mHitPoints > 0))
        return;
    long theAttackerCategory = inAttacker->mUnitClass->mCategory;
    if (theAttackerCategory != kUnitCategoryNaval)
        return;   // warships only (not boats, not land units on the shore)
    unsigned long theID = thePlayer->mPlayerID < 64 ? thePlayer->mPlayerID : 63;
    if (sLast[theID] != 0 && gPhysics->mGameTime >= sLast[theID] && gPhysics->mGameTime - sLast[theID] < kNavalFleeMs)
        return;
    sLast[theID] = gPhysics->mGameTime;
    long theX = inVictim->GetTileX(), theY = inVictim->GetTileY();
    bool theBoat = inVictim->mUnitClass->mCategory == kUnitCategoryNavalCitizen;
    GE2DPoint theRefuge(0.0f, 0.0f);
    if (theBoat) {
        // the own built dock farthest from the attacker (the nearest one is often where it shoots)
        std::vector<EEUnit*> theDocks;
        EEGetPlayerUnitsOfType(kUnitTypeDock, thePlayer->mPlayerID, theDocks);
        long theBest = -1;
        for (unsigned long k = 0; k < theDocks.size(); k++) {
            if (theDocks[k]->mComplexUnit == NULL || !theDocks[k]->mComplexUnit->mIsBuilt || !(theDocks[k]->mHitPoints > 0))
                continue;
            long theDX = theDocks[k]->GetTileX() - inAttacker->GetTileX(), theDY = theDocks[k]->GetTileY() - inAttacker->GetTileY();
            if (theDX * theDX + theDY * theDY > theBest) {
                theBest = theDX * theDX + theDY * theDY;
                theRefuge = GE2DPoint((float)theDocks[k]->GetTileX(), (float)theDocks[k]->GetTileY());
            }
        }
        if (theBest < 0)
            return;
    } else {
        EECPBuildManager* theBuild = inManager->mManager->mBuildManager;
        if (theBuild == NULL || theBuild->mTownCenterTile.mX == -10000)
            return;
        theRefuge = GE2DPoint((float)theBuild->mTownCenterTile.mX, (float)theBuild->mTownCenterTile.mY);
    }
    unsigned long theCount = 0;
    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (theComplex == NULL || !(theUnit->mHitPoints > 0) || theComplex->mIsDisabled || theComplex->mIsDetachedFromAI
            || theComplex->mContainer != NULL || !EECPRMNear(theUnit, theX, theY, kNavalFleeRange))
            continue;
        long theCategory = theUnit->mUnitClass->mCategory;
        bool theLeaves;
        if (theBoat)
            theLeaves = theCategory == kUnitCategoryNavalCitizen;
        else   // the land units that cannot answer a ship: citizens and melee soldiers
            theLeaves = theCategory == kUnitCategoryLandCitizen
                || ((theCategory == kUnitCategoryLand || theCategory == kUnitCategoryLandMilitaryHero) && theUnit->mStats->mRange < 3.0f);
        if (!theLeaves)
            continue;
        theComplex->Reference();
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen)
            inManager->AddIdleGatherer(theComplex);
        theComplex->ClearGoalQueue(0);
        theComplex->SetGoal(new EEUGLocation(theRefuge, true, true, false, 0.0f, false, false), 0, 0);
        theComplex->StartAction(kActionPrepareToMove);
        theComplex->GetUnitClass()->ActionChanged(theComplex);
        theComplex->Dereference();
        theCount++;
    }
    if (theCount > 0)
        inManager->mDangerZones.push_back(std::pair<U2DPoint<long>, unsigned long>(U2DPoint<long>(theX, theY), gPhysics->mGameTime));
    EE_CP_TRACE(thePlayer, "navalflee", "%s units=%lu", theBoat ? "boats" : "land", theCount);
}

void EECPRMNoteDanger(EECPResourceManager* inManager, EEUnit* inCitizen, unsigned long inAttacker)
{
    // a harassing raid only (kMaxRaiders soldiers of the attacker's within kDangerRange): against an attack group
    // running costs more work than it saves lives (docs/AI_RESEARCH.md §8.22)
    if (inAttacker == 0 || inAttacker >= gWorld.mNumPlayers || gWorld.mPlayers[inAttacker] == NULL)
        return;
    if (EECPMilitiaOn(inManager->mManager->mPlayer)) {
        EECPRMMilitia(inManager, inCitizen, inAttacker);
        return;
    }
    if (!gWorld.mPlayers[inAttacker]->IsHuman() && !EECPExperiment(inManager->mManager->mPlayer, "flee"))
        return;
    long theRaiders = 0;
    for (EEUnit* theUnit = gWorld.mPlayers[inAttacker]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->mStats->mAttack == 0.0f
            || theUnit->mUnitClass->IsLandBuilding())
            continue;
        long theCategory = theUnit->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen || theCategory == kUnitCategoryAmbient
            || theCategory == kUnitCategoryResource || theCategory == kUnitCategoryProjectile)
            continue;
        long theDX = theUnit->GetTileX() - inCitizen->GetTileX(), theDY = theUnit->GetTileY() - inCitizen->GetTileY();
        if (theDX * theDX + theDY * theDY <= kDangerRange * kDangerRange && ++theRaiders > kMaxRaiders)
            return;
    }
    EECPBuildManager* theBuildManager = inManager->mManager->mBuildManager;
    if (theBuildManager != NULL && theBuildManager->mTownCenterTile.mX != -10000) {
        long theDX = inCitizen->GetTileX() - theBuildManager->mTownCenterTile.mX;
        long theDY = inCitizen->GetTileY() - theBuildManager->mTownCenterTile.mY;
        if (theDX * theDX + theDY * theDY < kHomeRange * kHomeRange)
            return;   // at home: the town's defence answers, the gatherers keep working
    }
    bool theNew = !EECPRMInDanger(inManager, inCitizen->GetTileX(), inCitizen->GetTileY(), kDangerRange);
    inManager->mDangerZones.push_back(std::pair<U2DPoint<long>, unsigned long>(
        U2DPoint<long>(inCitizen->GetTileX(), inCitizen->GetTileY()), gPhysics->mGameTime));
    // a new zone: up to kDefenders of the nearest soldiers within kDefendRange tiles that are not attacking already
    // go for the raider (the town defence plans its aid by town threat, on a timer: a raider on an outer patch went
    // unanswered for minutes)
    EEComplexUnit* theRaider = inCitizen->mComplexUnit != NULL ? inCitizen->mComplexUnit->mLastAttacker : NULL;
    if (!theNew || theRaider == NULL || !(theRaider->mHitPoints > 0))
        return;
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    std::vector<std::pair<long, EEComplexUnit*> > theNear;
    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (theComplex == NULL || !(theUnit->mHitPoints > 0) || theUnit->mStats->mAttack == 0.0f
            || theUnit->mUnitClass->IsLandBuilding() || theComplex->mIsDisabled || theComplex->mIsDetachedFromAI)
            continue;
        long theCategory = theUnit->mUnitClass->mCategory;
        if (theCategory != kUnitCategoryLand && theCategory != kUnitCategoryLandMilitaryHero)
            continue;
        if (theComplex->mActiveGoal != NULL && theComplex->mActiveGoal->mKind == kGoalAttackUnit)
            continue;
        long theDX = theUnit->GetTileX() - theRaider->GetTileX(), theDY = theUnit->GetTileY() - theRaider->GetTileY();
        long theDistance = theDX * theDX + theDY * theDY;
        if (theDistance <= kDefendRange * kDefendRange)
            theNear.push_back(std::pair<long, EEComplexUnit*>(theDistance, theComplex));
    }
    std::stable_sort(theNear.begin(), theNear.end(),
                     [](const std::pair<long, EEComplexUnit*>& a, const std::pair<long, EEComplexUnit*>& b) { return a.first < b.first; });
    unsigned long theSent = 0;
    for (unsigned long k = 0; k < theNear.size() && theSent < kDefenders; k++, theSent++) {
        EEComplexUnit* theUnit = theNear[k].second;
        theUnit->ClearGoalQueue(0);
        theUnit->SetGoal(new EEUGAttackUnit(theUnit, theRaider, false, false), 0, 0);
        theUnit->StartAction(kActionPrepareToMove);
        theUnit->GetUnitClass()->ActionChanged(theUnit);
    }
    EE_CP_TRACE(thePlayer, "raid.defend", "sent=%lu near=%lu raider=%lu", theSent, (unsigned long)theNear.size(),
                (unsigned long)theRaider->GetUnitID());
}

static bool EECPRMInDanger(EECPResourceManager* inManager, long inX, long inY, long inRange)
{
    for (unsigned long z = 0; z < inManager->mDangerZones.size(); z++) {
        long theDX = inManager->mDangerZones[z].first.mX - inX, theDY = inManager->mDangerZones[z].first.mY - inY;
        if (theDX * theDX + theDY * theDY <= inRange * inRange)
            return true;
    }
    return false;
}

bool EECPRMResourceInDanger(EECPResourceManager* inManager, long inX, long inY)
{
    return EECPRMInDanger(inManager, inX, inY, kDangerRange);
}

static void EECPRMFleeRaids(EECPResourceManager* inManager)
{
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
    EECPBuildManager* theBuildManager = inManager->mManager->mBuildManager;
    std::vector<std::pair<U2DPoint<long>, unsigned long> >& theZones = inManager->mDangerZones;
    for (unsigned long z = 0; z < theZones.size();)
        if (gPhysics->mGameTime - theZones[z].second > kDangerMs)
            theZones.erase(theZones.begin() + z);
        else
            z++;
    if (theZones.empty() || theBuildManager == NULL || theBuildManager->mTownCenterTile.mX == -10000)
        return;
    GE2DPoint theHome((float)theBuildManager->mTownCenterTile.mX, (float)theBuildManager->mTownCenterTile.mY);
    unsigned long theFled = 0;
    for (long r = 2; r < 7; r++) {
        std::vector<EEComplexUnit*> theRun;
        for (unsigned long k = 0; k < inManager->mGatherers[r].size(); k++) {
            EEComplexUnit* theUnit = inManager->mGatherers[r][k];
            if (theUnit->mIsDisabled || theUnit->mIsDetachedFromAI || !(theUnit->mHitPoints > 0)
                || theUnit->GetUnitClass()->mCategory != kUnitCategoryLandCitizen)
                continue;
            if (EECPRMInDanger(inManager, theUnit->GetTileX(), theUnit->GetTileY(), kFleeRange))
                theRun.push_back(theUnit);
        }
        for (unsigned long k = 0; k < theRun.size(); k++) {
            EEComplexUnit* theUnit = theRun[k];
            theUnit->Reference();
            inManager->AddIdleGatherer(theUnit);   // off this resource's list, onto the idle list
            theUnit->ClearGoalQueue(0);
            theUnit->SetGoal(new EEUGLocation(theHome, true, true, false, 0.0f, false, false), 0, 0);
            theUnit->StartAction(kActionPrepareToMove);
            theUnit->GetUnitClass()->ActionChanged(theUnit);
            theUnit->Dereference();
            theFled++;
        }
    }
    if (theFled > 0)
        EE_CP_TRACE(thePlayer, "flee", "n=%lu zones=%lu", theFled, (unsigned long)theZones.size());
}

// FUNCTION: EXE 0x491d34
// Periodically recomputes the wanted gatherers, reassigns gatherers and prunes depleted animals and fish;
// initialises the drop site classes and timers on the first call
void EECPResourceManager::Update(unsigned long inTime)
{
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    unsigned long i;
    if (!this->mInitialized) {
        for (i = 0; i < EEUnitClassCount(); i++) {
            EEUnitClass* theClass = gUnitClassTable[i];
            if (theClass != NULL && (theClass->IsDropSite() || theClass->mUnitType == kUnitTypeDock || theClass->mUnitType == kUnitTypeNavyYard))
                this->mDropSiteClasses[theClass->mResourceType].push_back(i);
        }
        this->RefreshResources();
        this->mNextFishingBoatTime = inTime + 30000;
        this->mTime170 = inTime + 20000;
        this->mNextWantedGatherersTime = inTime + 5000;
        this->mNextAssignGatherersTime = inTime + 10000;
        this->mNextResourceCheckTime = inTime + 15000;
        this->mInitialized = true;
    }
    if (!thePlayer->mCanGather)
        return;
    if (inTime >= this->mNextWantedGatherersTime) {
        if (this->mNextWantedGatherersTime > 0 && !EECPNextEngine(thePlayer))   // the next engine: gather() sets them
            this->UpdateWantedGatherers();
        this->mNextWantedGatherersTime = inTime + 5000;
    }
    EECPRMFleeRaids(this);   // a no-op without danger zones
    if (inTime >= this->mNextAssignGatherersTime) {
        if (this->mNextAssignGatherersTime > 0)
            this->AssignGatherers();
        this->mNextAssignGatherersTime = inTime + 10000;
    }
    if (thePlayer->mSeesAll || !thePlayer->mCanExplore || !(inTime >= this->mNextResourceCheckTime))
        return;
    for (long j = 2; j < 7; j++)
        this->mB180[j] = false;
    std::vector<EEComplexUnit*>::iterator it = this->mMines.begin();
    while (it != this->mMines.end()) {
        if (!(*it)->mIsValid || (*it)->mResourceAmount == 0) {
            (*it)->Dereference();
            it = this->mMines.erase(it);
        } else
            it++;
    }
    it = this->mFish.begin();
    while (it != this->mFish.end()) {
        if (!(*it)->mIsValid || (*it)->mResourceAmount == 0) {
            (*it)->Dereference();
            it = this->mFish.erase(it);
        } else
            it++;
    }
    this->RequeueIdleGatherers();
    this->mNextResourceCheckTime = inTime + 15000;
}

// The two predicates' ctor (ICF-folded into EEPatrolTileIs's 0x160fba)
EECPRMForestDistanceLess::EECPRMForestDistanceLess(U2DPoint<long> inCenter)
    : mCenter(inCenter.mX, inCenter.mY)
{
}

// ICF-folds into 0x8a3f (EETileDistanceLess::DistanceSquared)
inline long EECPRMForestDistanceLess::DistanceSquared(const EETileCoordinate& inTile) const
{
    long theDY = inTile.GetY();
    theDY = this->mCenter.mY - theDY;
    long theDX = inTile.GetX();
    theDX = this->mCenter.mX - theDX;
    theDX *= theDX;
    theDY *= theDY;
    return theDX + theDY;
}


EECPRMUnitDistanceLess::EECPRMUnitDistanceLess(U2DPoint<long> inCenter)
    : mCenter(inCenter.mX, inCenter.mY)
{
}

// ICF-folds into 0x8a3f (EETileDistanceLess::DistanceSquared). Inline: VC6's /Os post-check undoes it at the no-/Og
// sites (calls stay out of line) while its two locals keep their caller frame slots
inline long EECPRMUnitDistanceLess::DistanceSquared(const EETileCoordinate& inTile) const
{
    long theDY = inTile.GetY();
    theDY = this->mCenter.mY - theDY;
    long theDX = inTile.GetX();
    theDX = this->mCenter.mX - theDX;
    theDX *= theDX;
    theDY *= theDY;
    return theDX + theDY;
}


// The sort/find_if COMDATs (0x91f11..0x9350f; paired by build/scratch_r12-ai-3c/mtg.py, best effort)
// TEMPLATE: EXE 0x491f11
// TEMPLATE: EXE 0x491f24
// TEMPLATE: EXE 0x492008
// TEMPLATE: EXE 0x4920ec
// TEMPLATE: EXE 0x4921d0
// TEMPLATE: EXE 0x4921f7
// TEMPLATE: EXE 0x492230
// TEMPLATE: EXE 0x49225d
// TEMPLATE: EXE 0x492290
// TEMPLATE: EXE 0x492398
// TEMPLATE: EXE 0x4923dc
// TEMPLATE: EXE 0x49240f
// TEMPLATE: EXE 0x492517
// TEMPLATE: EXE 0x492559
// TEMPLATE: EXE 0x49258c
// TEMPLATE: EXE 0x492694
// TEMPLATE: EXE 0x4926d6
// TEMPLATE: EXE 0x492742
// TEMPLATE: EXE 0x4927f6
// TEMPLATE: EXE 0x49283a
// TEMPLATE: EXE 0x492886
// TEMPLATE: EXE 0x4929b6
// TEMPLATE: EXE 0x4929f1
// TEMPLATE: EXE 0x492aa2
// TEMPLATE: EXE 0x492ae6
// TEMPLATE: EXE 0x492b33
// TEMPLATE: EXE 0x492c53
// TEMPLATE: EXE 0x492c86
// TEMPLATE: EXE 0x492d37
// TEMPLATE: EXE 0x492d7b
// TEMPLATE: EXE 0x492dc8
// TEMPLATE: EXE 0x492ee8
// TEMPLATE: EXE 0x492f46
// TEMPLATE: EXE 0x492f79
// TEMPLATE: EXE 0x492fd6
// TEMPLATE: EXE 0x493009
// TEMPLATE: EXE 0x493066
// TEMPLATE: EXE 0x493099
// TEMPLATE: EXE 0x49311c
// TEMPLATE: EXE 0x49318e
// TEMPLATE: EXE 0x493208
// TEMPLATE: EXE 0x493278
// TEMPLATE: EXE 0x4932f2
// TEMPLATE: EXE 0x493362
// TEMPLATE: EXE 0x4933b9
// TEMPLATE: EXE 0x4933ff
// TEMPLATE: EXE 0x49344e
// TEMPLATE: EXE 0x493492
// TEMPLATE: EXE 0x4934d6
// TEMPLATE: EXE 0x4934fd
// TEMPLATE: EXE 0x493502
