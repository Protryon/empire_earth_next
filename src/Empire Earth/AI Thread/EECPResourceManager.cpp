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
#include "ee/EECPCombatManager.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPPersonnel.h"
#include "ee/EEWheel.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EECPResearchManager.h"
#include "ee/EECPTrace.h"

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
    EEComputerPlayer* thePlayer = this->mManager->mPlayer;
    EECPUnitManager* theUnitManager = this->mManager->mUnitManager;
    EECPCombatManager* theCombatManager = thePlayer->mCombatManager;
    long theClass = this->mManager->ChooseTransportClass();
    bool theFailed = true;
    if (theClass == 0)
        return;
    std::deque<EEComplexUnit*>* theUnits = theUnitManager->GetUnits(theClass);
    theClasses.push_back(theClass);
    EECPPersonnelRequest* theRequest = theCombatManager->mPersonnel->NewContinentRequest(theClasses, theIDs, 0);
    if (theRequest->mNumFilled == 1) {
        EEComplexUnit* theUnit = theRequest->mUnits.back().mUnit;
        if (theUnit->mAIState != 0x46cd7)
            theUnit->mAIState = 0x46cd7;
        theRequest->PopSlot();
        thePlayer->DetachUnitFromAI(theUnit);
        theFailed = false;
        if (this->FerryGatherers(inUnits, theUnit, inResource))
            thePlayer->SetNextIslandExpansionTime();
        else {
            if (theUnit->mIsDetachedFromAI)
                theUnit->mIsDetachedFromAI = false;
            theCombatManager->UnitAdded(theUnit);
        }
    } else
        theRequest->Cancel();
    theCombatManager->mPersonnel->ReleaseRequest(theRequest->mID);
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
    EEMap* theMap = gWorld.mMap;
    if (theMap != NULL && ((theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (theMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap)) {
        U2DPoint<long> theTown = this->mManager->mBuildManager->mTownCenterTile;
        theAcross = true;
        if (theTown.mX != -10000)
            theTownContinent = gContinentManager.GetContinentID(theTown.mX, theTown.mY);
    } else
        theAcross = false;
    this->RebalanceGatherers();   // new

    for (it = this->mGatherers[0].begin(); it != this->mGatherers[0].end(); ++it) {
        theUnit = *it;
        EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(theUnit->mActiveGoal);
        EEUGGather* theGather = dynamic_cast<EEUGGather*>(theUnit->mActiveGoal);
        if ((theGather != NULL && theGather->mResource != NULL)
            || (theAttack != NULL && theAttack->mTarget->GetUnitClass()->mFamily == kUnitFamilyAnimal))
            theMoved.push_back(theUnit);
    }
    unsigned long i;
    for (i = 0; i < theMoved.size(); i++) {
        this->RemoveGatherer(theMoved[i]);
        this->AddGatherer(theMoved[i], theMoved[i]->mResourceType);
    }

    unsigned long j;
    for (j = 0; j < 7; j++) {
        for (it = this->mGatherers[j].begin(); it != this->mGatherers[j].end(); ++it) {
            theUnit = *it;
            EEUnitGoal* theGoal = theUnit->mActiveGoal;
            EEUGGather* theGather = dynamic_cast<EEUGGather*>(theGoal);
            if (theUnit->mIsDisabled || theUnit->mIsDetachedFromAI)
                continue;
            if (theUnit->GetUnitClass()->mCategory == kUnitCategoryNavalCitizen) {
                if (j == 0)
                    theBoats.push_back(theUnit);
            } else if (theUnit->mCitizenBuildQueue.size() == 0 && theUnit->mHitPoints > 0 && theUnit->GetTileX() != -10000
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
            if (theResource == 2 && theWanted > 0 && thePlayer->mUsesFishing && theBoats.size() < 8
                && gPhysics->mGameTime >= this->mNextFishingBoatTime) {
                this->mManager->mUnitManager->RequestFishingBoats(1);
                this->mNextFishingBoatTime = gPhysics->mGameTime + 30000;
            }
            if (theWanted >= theIdle.size() + theBoats.size())
                theWanted = theIdle.size() + theBoats.size();
            if (theWanted > 0 || (thePlayer->IsIslandExpansionDue() && (theResource == 4 || theResource == 5 || theResource == 6))) {
                switch (theResource) {
                case 2: {
                    EEUnit* theTarget = this->FindFarm(true, true, theIdle.size() > 0 ? theIdle.front() : NULL);
                    if (theTarget != NULL) {
                        unsigned long theCount = theWanted < theIdle.size() ? theWanted : theIdle.size();
                        while (theCount > 0) {
                            if (gWorld.GetResourceUsers(theTarget) < theLimit) {
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
                                        if (gWorld.GetResourceUsers(theTarget) < theLimit) {
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
                                            if (gWorld.GetResourceUsers(theTarget) < theLimit) {
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
                                            if (theWanted > 0 && thePlayer->mUsesFishing) {
                                                if (this->mWantedGatherers[theResource] - this->mGatherers[theResource].size() > theBoats.size()
                                                    && theBoats.size() < 8 && gPhysics->mGameTime >= this->mNextFishingBoatTime) {
                                                    this->mManager->mUnitManager->RequestFishingBoats(1);
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
                                if (gWorld.GetResourceUsers(theTarget) < theLimit) {
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
                                    if (gContinentManager.AreConnected(gContinentManager.GetContinentByID(theTownContinent),
                                            gContinentManager.mContinents[gContinentManager.GetContinentID(theUnit)],
                                            this->mManager->mPlayer->GetCitizenPassability())
                                        && !gContinentManager.AreConnected(gContinentManager.GetContinentAt(theFar),
                                            gContinentManager.GetContinentAt(theUnit), this->mManager->mPlayer->GetCitizenPassability()))
                                        theStranded.push_back(theUnit);
                                }
                                if (theStranded.size() > 0)
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
    if (thePlayer->mCanBuild && this->mSettlementsEnabled && thePlayer->CanProduceUnitType(kUnitTypeSettlement, 0x20, NULL)) {
        EEUnit* theResource = theGoal->mResource;
        EEUnit* theStorage = theGoal->mStorage;
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
                                if (theCenter == NULL || !theCenter->FindBuildSite(kUnitTypeSettlement, theSite.mCenter))
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
    if (theCenter.mX != -10000 && theCenter.mY != -10000)
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
        if (!(theUnit->mResourceAmount > 0) || theUnit->mResourceType != kResourceFood || !(theUnit->mHitPoints > 0) || theUnit->mStats->mAttack != 0.0f)
            continue;
        long theKind = theUnit->GetUnitClass()->mFamily;
        if (theKind == kUnitFamilyAnimal) {
            theUnit->Reference();
            EEComplexUnit* theMine = theUnit->mComplexUnit;
            this->mMines.push_back(theMine);
        } else if (theKind == kUnitFamilyFish) {
            theUnit->Reference();
            this->mFish.push_back(static_cast<EEComplexUnit*>(theUnit));
        }
    }
    if (theCenter.mX != -10000 && theCenter.mY != -10000)
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
    long theRadius = this->mManager->mPlayer->mDropOffDistance;
    outUnits.clear();
    long theRange = inNear ? theRadius * theRadius * 4 : theRadius * theRadius * 36;
    EETileCoordinate theCenter;
    theCenter.mX = this->mManager->mBuildManager->mTownCenterTile.mX;
    theCenter.mY = this->mManager->mBuildManager->mTownCenterTile.mY;
    bool theHaveCenter;
    if (theCenter.mX != -10000 && theCenter.mY != -10000)
        theHaveCenter = true;
    else
        theHaveCenter = false;
    unsigned long theFound = 0;
    for (unsigned long i = 0; theFound < inCount && i < this->mMines.size(); i++) {
        EEComplexUnit* theMine = this->mMines[i];
        bool theSame;
        if (theHaveCenter) {
            EEComputerPlayer* thePlayer = this->mManager->mPlayer;
            long theY = theMine->GetTileY();
            long theX = theMine->GetTileX();
            unsigned short theCenterID = gContinentManager.GetContinentID(theCenter.mX, theCenter.mY);
            long theFlags = thePlayer->GetCitizenPassability();
            theSame = gContinentManager.AreConnected(gContinentManager.GetContinentByID(theCenterID),
                                                gContinentManager.mContinents[gContinentManager.GetContinentID(theX, theY)], theFlags);
        } else
            theSame = true;
        if (!theSame || !theMine->mIsValid || !(theMine->mResourceAmount > 0) || !(theMine->GetHitPoints() > 0))
            continue;
        bool theFree = !this->mManager->mBuildManager->HasEnemyNear(theMine->GetTileX(), theMine->GetTileY());
        if (!theFree)
            continue;
        EEComplexUnit* theCenterUnit = this->FindNearestTownCenter(theMine);
        if (theCenterUnit != NULL && theCenterUnit->DistanceSquaredTo(theMine) < (float)theRange) {
            outUnits.push_back(this->mMines[i]);
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
        if (theUnit == NULL || theUnit->GetUnitClass()->mUnitType != kUnitTypeResForageBush)
            continue;
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
            if (!gContinentManager.AreConnected(gContinentManager.GetContinentByID(theCenterID),
                                           gContinentManager.mContinents[gContinentManager.GetContinentID(theX, theY)], theFlags))
                continue;
        }
        if (!static_cast<EETile*>(theUnit->mTile)->IsVisibleTo(thePlayer))
            continue;   // skip it (was break: ended the whole search)
        if (gWorld.GetResourceUsers(theUnit) >= theLimit)
            continue;
        theTile.mCenter.mX = theUnit->GetTileX();
        theTile.mCenter.mY = theUnit->GetTileY();
        if (theUnit->mResourceAmount <= 0 || theUnit->mTile->mTerrainType == kTerrainDeepWater)
            continue;
        if (inNeedsDropSite && theCenterObject->mUnit == NULL)
            continue;
        if (this->mManager->mBuildManager->HasEnemyNear(theUnit->GetTileX(), theUnit->GetTileY()))
            continue;
        if (theTile.DistanceSquared(theCenter.mCenter) < theRange) {
            theResult = theUnit;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x490819
// Returns the nearest fish to a unit that is not crowded, preferring fish more than 8 tiles from every own dock
EEUnit* EECPResourceManager::FindFish(EEUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    unsigned long theLimit = this->mManager->mPlayer->mMiningLimit;
    EEUnit* theResult = NULL;
    std::sort(this->mFish.begin(), this->mFish.end(), EECPUnitTileDistanceLess(inUnit->GetTile()));
    EEGetPlayerUnitsOfType(kUnitTypeDock, this->mManager->mPlayer->mPlayerID, theUnits);
    unsigned long i;
    for (i = 0; i < this->mFish.size() && theResult == NULL; i++) {
        EEComplexUnit* theUnit = this->mFish[i];
        if (theUnit->mResourceAmount > 0 && gWorld.GetResourceUsers(theUnit) < theLimit) {
            if (std::find_if(theUnits.begin(), theUnits.end(), EECPRMUnitWithin(theUnit->GetTile(), 64.0f)) == theUnits.end())
                theResult = theUnit;
        }
    }
    if (theResult != NULL)
        return theResult;
    unsigned long j;
    for (j = 0; j < this->mFish.size() && theResult == NULL; j++) {
        EEComplexUnit* theUnit = this->mFish[j];
        if (theUnit->mResourceAmount > 0 && gWorld.GetResourceUsers(theUnit) < theLimit)
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
EEUnit* EECPResourceManager::FindResourceTarget(long inResource, EEComplexUnit* inUnit, bool inBackwards)
{
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
            if (theUnit != NULL && static_cast<EETile*>(theUnit->mTile)->IsVisibleTo(thePlayer) && gWorld.GetResourceUsers(theUnit) < theLimit
                && theUnit->mResourceAmount > 0 && theUnit->mTile->mTerrainType != kTerrainDeepWater) {
                theOK = !theBuildManager->HasEnemyNear(theUnit->GetTileX(), theUnit->GetTileY());
                if (theOK) {
                    EEPlayer** thePlayers = gWorld.mPlayers;
                    for (unsigned long j = 0; j < gWorld.mNumPlayers && theOK; j++, thePlayers++) {
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
    bool theOK = gContinentManager.AreConnected(gContinentManager.GetContinentByID(theUnitContinent),
                                           gContinentManager.mContinents[theResourceContinent], theFlags);
    if (!theOK)
        return;
    EEUnitAction* theCommand = inUnit->mCommand;
    if (theCommand != NULL && theCommand->mType == kActionIdle) {
        if (inResource->mResourceType != inUnit->mResourceType && inUnit->mResourceAmount != 0)
            inUnit->mResourceAmount = 0;
        EEUnitGoal* theGoal = new EEUGGather(inResource, inStorage, true);
        inUnit->ClearGoalQueue(0);
        inUnit->SetGoal(theGoal, 0, 0);
    } else if (inUnit->IsGathering()) {
        EEUGGather* theGoal = static_cast<EEUGGather*>(inUnit->GetLoadGoal());
        if (inUnit->mResourceAmount != 0)
            inUnit->mResourceAmount = 0;
        inUnit->SetGatheredResource(NULL);
        theGoal->SetResource(inResource);
        theGoal->SetStorage(inStorage);
    } else {
        EEUGGather* theGoal = dynamic_cast<EEUGGather*>(inUnit->GetLoadGoal());
        if (theGoal != NULL) {
            if (inResource->mResourceType != inUnit->mResourceType && inUnit->mResourceAmount != 0)
                inUnit->mResourceAmount = 0;
            inUnit->SetGatheredResource(NULL);
            theGoal->SetResource(inResource);
            theGoal->SetStorage(inStorage);
        } else
            theOK = false;
    }
    if (!theOK)
        return;
    if (inUnit->mResourceType != inResource->mResourceType)
        inUnit->mResourceType = inResource->mResourceType;
    inUnit->StartAction(kActionMoveToWaitingArea);
    inUnit->GetUnitClass()->ActionChanged(inUnit);
    this->RemoveGatherer(inUnit);
    this->AddGatherer(inUnit, inResource->mResourceType);
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
        if (r == 2)
            theCapacity += this->mGatherers[2].size();   // farms and fish are not center units: what works there now
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
        long theShort = theTarget - theUnits->CountMilitaryUnits(false, false);
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
    if (theEpoch != kTechNone) {
        const EEAIPersonalityRule& thePersonality = EECPPersonality(thePlayer);
        float theShare = (EECPEpochPressure(thePlayer, gPhysics->mGameTime) - thePersonality.mEpochPressureStart)
            / (thePersonality.mEpochPressureFull - thePersonality.mEpochPressureStart);
        if (theShare > 1.0f)
            theShare = 1.0f;
        if (theShare > 0.0f)
            EECPRMAddCost(thePlayer, theTree->Get(theEpoch), kUnitTypeTownCenter, theShare, outDemand.mTier[1]);
    }
}

static bool EECPRMSplitByDemand(EECPResourceManager* inManager)
{
    EEComputerPlayer* thePlayer = inManager->mManager->mPlayer;
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
        if (this->mNextWantedGatherersTime > 0)
            this->UpdateWantedGatherers();
        this->mNextWantedGatherersTime = inTime + 5000;
    }
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
