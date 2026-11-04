// C:\EEX\Empire Earth\AI Thread\EECPUnitManager.cpp (file name inferred: no __FILE__; the class-static key
// "EECPUnitManager-Data", guarded chain 0x9350f..0x93552). A computer player's unit manager: 0x9350f..0x979ff.
// The deque/map/sort COMDATs 0x95e70.. follow from the source.
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

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUGGather.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPPersonnel.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPBuildManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include "ee/EECPUnitManager.h"
#include "ee/EEAIFOUnitsToKillScoringBase.h"
#include "ee/EEAIFOSubGroupFilter.h"
#include <EEAIFunctionObjects.h>

const UString EECPUnitManager::sData("EECPUnitManager-Data");
// SYNTHETIC: EXE 0x49350f   (wrapper)
// SYNTHETIC: EXE 0x493519   (ctor)
// SYNTHETIC: EXE 0x49352a   (registrar)
// SYNTHETIC: EXE 0x493536   (dtor)

// FUNCTION: EXE 0x493552
// Orders two units by the squared distance of their tiles from the center tile, nearer first
// Built without /Og (rule 4 evidence: memory-resident locals, `leave`); exact with the pragma, hash-fitted names and
// U2DPoint<long>::DistanceSquared (an inline the /Os post-check undoes: its locals keep 8 frame slots; r37_load)
#pragma optimize("g", off)
bool EECPUnitDistanceLess::operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const
{
    U2DPoint<long> theFirstTile(inFirst->GetTileX(), inFirst->GetTileY());
    U2DPoint<long> theSecondTile(inSecond->GetTileX(), inSecond->GetTileY());
    long theLeft = theFirstTile.DistanceSquared(this->mCenter);
    long theRight = theSecondTile.DistanceSquared(this->mCenter);
    bool theResult = theLeft < theRight;
    return theResult;
}
#pragma optimize("g", on)

// FUNCTION: EXE 0x4935e6
// Creates the unit manager of a civilization manager, with nothing waiting or in production and production off
EECPUnitManager::EECPUnitManager(EECPCivilizationManager* inManager)
{
    this->mManager = inManager;
    this->mPlayer = inManager->mPlayer;
    this->mNumWaiting = 0;
    this->mNumInProduction = 0;
    this->mProductionEnabled = false;
}

// FUNCTION: EXE 0x493660
// Destroys the manager, releasing every unit it tracks
EECPUnitManager::~EECPUnitManager()
{
    this->ReleaseUnits();
}

// FUNCTION: EXE 0x493696
// Returns whether any epoch's request list holds a request for a unit class
bool EECPUnitManager::IsRequested(long inUnitClass)
{
    bool theFound = false;
    for (long i = 0; i < 15 && !theFound; i++) {
        std::deque<EECPUMRequest>& theRequests = this->mRequests[i];
        for (std::deque<EECPUMRequest>::iterator it = theRequests.begin(); it != theRequests.end() && !theFound; ++it)
            theFound = (*it).mUnitClass == inUnitClass;
    }
    return theFound;
}

// FUNCTION: EXE 0x4936ec
// Returns whether a producing building of a unit class is researching or has queued that class's technology
bool EECPUnitManager::IsInProduction(long inUnitClass)
{
    unsigned long i = 0;
    bool theFound = false;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    if (!theTree)
        return theFound;
    inUnitClass = gUnitClassTable[inUnitClass] != NULL ? gUnitClassTable[inUnitClass]->mTechnology : 100000;
    EETechTreeNode* theNode = theTree->Get(inUnitClass);
    if (!theNode || !theNode->GetNumUnitClasses())
        return theFound;
    for (; !theFound && i < theNode->GetNumUnitClasses(); i++) {
        std::deque<EEComplexUnit*>* theUnits = this->GetUnits(theNode->GetUnitClass(i));
        if (!theUnits)
            continue;
        for (std::deque<EEComplexUnit*>::iterator it = theUnits->begin(); it != theUnits->end() && !theFound; ++it) {
            if ((*it)->mCurrentResearch == inUnitClass || (*it)->IsResearchQueued(inUnitClass))
                theFound = true;
            else
                theFound = false;
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x4937dd
// Returns whether a technology is in the unit's build queue
bool EEComplexUnit::IsResearchQueued(long inTechnology)
{
    return std::find(this->mBuildQueue.begin(), this->mBuildQueue.end(), inTechnology) != this->mBuildQueue.end();
}

// FUNCTION: EXE 0x493832
// Returns whether any tracked unit of a class is not yet built
bool EECPUnitManager::HasUnfinished(long inUnitClass)
{
    std::deque<EEComplexUnit*>::iterator it;
    bool theFound = false;
    std::deque<EEComplexUnit*>* theUnits = this->GetUnits(inUnitClass);
    if (theUnits)
        for (it = theUnits->begin(); it != theUnits->end() && !theFound; ++it)
            theFound = !(*it)->mIsBuilt;
    return theFound;
}

// FUNCTION: EXE 0x493893
// Cancels a count of requested units of a class for a town and continent, from the requests and then from the
// build queues; returns true if the whole count was cancelled
bool EECPUnitManager::CancelRequest(long inUnitClass, unsigned long inCount, long inTownID, unsigned short inContinent)
{
    std::deque<EECPUMRequest>::iterator it;
    std::deque<EEComplexUnit*>::iterator theUnit;
    unsigned long theLeft = inCount;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    long theID = gUnitClassTable[inUnitClass] != NULL ? gUnitClassTable[inUnitClass]->mTechnology : 100000;
    EETechTreeNode* theNode = theTree->Get(theID);
    for (unsigned long i = 0; i < 15; i++) {
        if (theLeft == 0)
            goto done;
        std::deque<EECPUMRequest>& theRequests = this->mRequests[i];
        for (it = theRequests.begin(); it != theRequests.end() && theLeft != 0;) {
            if ((*it).mUnitClass == inUnitClass && (*it).mContinent == inContinent && (*it).mTownID == inTownID) {
                if (theLeft < (*it).mCount) {   // only the count still left (was inCount)
                    (*it).mCount -= theLeft;
                    this->mNumInProduction -= theLeft;
                    theLeft = 0;
                } else {
                    theLeft -= (*it).mCount;
                    this->mNumInProduction -= (*it).mCount;
                    it = theRequests.erase(it, it + 1);   // continue at the next request
                    continue;
                }
            }
            ++it;
        }
    }
    if (theLeft != 0) {
        RTSTechTreeNodeData* theData = theNode->mData;
        for (unsigned long j = 0; j < theData->mUnitClasses.size() && theLeft != 0; j++) {
            std::deque<EEComplexUnit*>* theUnits = this->GetUnits(theData->mUnitClasses[j]);
            if (theUnits) {
                for (theUnit = theUnits->begin(); !(theUnit == theUnits->end()) && theLeft != 0; ++theUnit) {
                    EEComplexUnit* theComplex = *theUnit;
                    do {
                        if (!theComplex->IsResearchQueued(theID))
                            break;
                        theComplex->RemoveResearch(theID, 0);
                        theLeft--;
                        this->mNumInProduction--;
                    } while (theLeft != 0);
                }
            }
        }
        this->mManager->GetResourceManager()->RemoveCosts(theNode, theLeft);
    }
done:
    if (this->mNumInProduction < 0)
        this->mNumInProduction = 0;
    return theLeft == 0;
}

// NEAR: the original multiplies with fimul (float loaded first, the long counted from memory); ours fild + fmul (90.5)
// FUNCTION: EXE 0x493aee
// Returns the citizen target: the population limit times the player's citizen percentage
long EECPUnitManager::GetWantedCitizens()
{
    long theCount = this->mPlayer->GetPopulationLimit(0);
    return UMFloatToLong(theCount * this->mPlayer->GetCitizenPercentage());
}

// FUNCTION: EXE 0x493b20
// Releases every tracked unit and frees the per-class lists
void EECPUnitManager::ReleaseUnits()
{
    std::map<long, std::deque<EEComplexUnit*>*>::iterator it;
    for (it = this->mUnits.begin(); it != this->mUnits.end(); ++it) {
        std::deque<EEComplexUnit*>* theUnits = it->second;
        while (theUnits->size() > 0) {
            EEComplexUnit* theUnit = theUnits->front();
            theUnits->pop_front();
            theUnit->Dereference();
        }
        delete theUnits;
    }
    this->mUnits.clear();
}

// FUNCTION: EXE 0x493b97
// Spreads the queued production of a building's class over its buildings on the same continent; the remainder
// goes to the given building
void EECPUnitManager::BalanceProduction(EEComplexUnit* inUnit)
{
    std::vector<long> theTechnologies;
    std::vector<EEComplexUnit*> theUnits;
    bool theFound = false;
    EEUnitClass* theClass = inUnit->mUnitClass;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    std::deque<EEComplexUnit*>* theClassUnits = this->GetUnits(theClass->mUnitType);
    if (theClassUnits == NULL || theClassUnits->size() <= 1)
        return;
    long i;
    for (i = 0; i < (long)theClass->mResearchTechnologies.size(); i++) {
        if (theFound)
            break;
        EETechTreeNode* theNode = theTree->Get(theClass->mResearchTechnologies[i]);
        if (theNode && theNode->mData->mMakesUnit)
            theFound = true;
        else
            theFound = false;
    }
    if (!theFound)
        return;
    for (std::deque<EEComplexUnit*>::iterator it = theClassUnits->begin(); !(it == theClassUnits->end()); ++it)
        if (gContinentManager.GetContinentID(*it) == gContinentManager.GetContinentID(inUnit))
            theUnits.push_back(*it);
    std::vector<EEComplexUnit*>::iterator theUnit;
    for (theUnit = theUnits.begin(); theUnit != theUnits.end(); theUnit++) {
        while ((*theUnit)->GetBuildQueueSize() > 0) {
            long theTechnology = (*theUnit)->mBuildQueue[0].mTechnology;
            (*theUnit)->PopProductionQueue();
            theTechnologies.push_back(theTechnology);
        }
    }
    unsigned long theShare = theTechnologies.size() / theClassUnits->size();
    for (theUnit = theUnits.begin(); theUnit != theUnits.end(); theUnit++) {
        for (unsigned long k = theShare; k > 0; k--) {
            long theTechnology = theTechnologies.back();
            theTechnologies.pop_back();
            (*theUnit)->QueueResearch(theTechnology, false);
        }
    }
    while (theTechnologies.size() > 0) {
        long theTechnology = theTechnologies.back();
        theTechnologies.pop_back();
        inUnit->QueueResearch(theTechnology, false);
    }
}

// FUNCTION: EXE 0x493dbb
// Removes the first entry of the unit's build queue
void EEComplexUnit::PopProductionQueue()
{
    this->mBuildQueue.erase(this->mBuildQueue.begin());
}

// FUNCTION: EXE 0x493de4
// Returns the first attached citizen building an unfinished building whose site is taken while it waits for the
// site or is disabled
EEComplexUnit* EECPUnitManager::FindStalledBuilder()
{
    std::vector<EEComplexUnit*> theUnits;
    EEComplexUnit* theResult = NULL;
    this->GetCitizens(theUnits);
    for (unsigned long i = 0, theCount = theUnits.size(); i < theCount && theResult == NULL; i++) {
        EEComplexUnit* theUnit = theUnits[i];
        EEUGBuildRepair* theGoal = dynamic_cast<EEUGBuildRepair*>(theUnit->GetLoadGoal());
        if (!theGoal || theUnit->mIsDetachedFromAI)
            continue;
        if ((theUnit->mCommand ? theUnit->mCommand->mType : 0) != kActionWaitForBuildSiteToClear && !theUnit->mIsDisabled)
            continue;
        EEUnit* theTargetUnit = theGoal->mTarget;
        EEComplexUnit* theTarget = dynamic_cast<EEComplexUnit*>(theTargetUnit);
        if (!theTarget->mIsBuilt && theGoal->IsBuildSiteTaken())
            theResult = theUnit;
    }
    return theResult;
}

// FUNCTION: EXE 0x493ed4
// Feeds a counting screen the tracked units, the open requests of the tech tree's epochs and the units being
// produced at land buildings
void EECPUnitManager::GatherUnits(EECPUMGatherUnitsScreenBase& ioGatherer)
{
    std::map<long, std::deque<EEComplexUnit*>*>::iterator it;
    long i;
    if (ioGatherer.mByClass) {
        for (it = this->mUnits.begin(); it != this->mUnits.end(); ++it) {
            std::deque<EEComplexUnit*>* theUnits = it->second;
            if (!ioGatherer.mByUnit)
                ioGatherer.CountUnits(gUnitClassTable[it->first], theUnits);
            else
                for (i = theUnits->size() - 1; i >= 0; i--)
                    ioGatherer.CountUnit((*theUnits)[i]);
        }
    }
    if (!ioGatherer.mRequests)
        return;
    EETechTree* theTree = this->mPlayer->GetTechTree();
    for (long theEpoch = theTree->mStartEpoch; theEpoch <= this->mPlayer->GetTechTree()->mEndEpoch; theEpoch++) {
        std::deque<EECPUMRequest>& theRequests = this->mRequests[theEpoch];
        for (long j = theRequests.size() - 1; j >= 0; j--) {
            EEUnitClass* theClass = gUnitClassTable[theRequests[j].mUnitClass];
            ioGatherer.CountRequest(theClass, &theRequests[j]);
        }
    }
    EECPUMRequest theRequest;
    theRequest.mContinent = 0;
    theRequest.mTownID = 0;
    theRequest.mUnitClass = 0;
    theRequest.mCount = 0;
    for (EEUnit* theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (!theComplex || !theComplex->mUnitClass->IsLandBuilding())
            continue;
        EETechTreeNode* theNode;
        long theClass;
        if (theComplex->mCurrentResearch != kTechNone && (theNode = this->mPlayer->GetTechTree()->Get(theComplex->mCurrentResearch)) != NULL &&
            (theClass = theNode->mData->mUnitClass) > 0 && gUnitClassTable[theClass]) {
            theRequest.mCount = 1;
            theRequest.mUnitClass = theClass;
            ioGatherer.CountRequest(gUnitClassTable[theClass], &theRequest);
        }
        for (long k = theComplex->GetBuildQueueSize() - 1; k >= 0; k--) {
            theNode = this->mPlayer->GetTechTree()->Get(theComplex->mBuildQueue[k].mTechnology);
            if (!theNode || !((theClass = theNode->mData->mUnitClass) > 0) || !gUnitClassTable[theClass])
                continue;
            theRequest.mCount = 1;
            theRequest.mUnitClass = theClass;
            ioGatherer.CountRequest(gUnitClassTable[theClass], &theRequest);
        }
    }
}

// FUNCTION: EXE 0x4940eb
// Returns the length of a unit's build queue (EEComplexUnit::GetBuildQueueSize, inline in ee/EEUnit.h)

// FUNCTION: EXE 0x4940fe
// Collects the citizens that are available for a new job
void EECPUnitManager::GetIdleCitizens(std::vector<EEComplexUnit*>& outUnits)
{
    unsigned long i = 0;
    std::vector<EEComplexUnit*> theUnits;
    outUnits.clear();
    this->GetCitizens(theUnits);
    unsigned long theCount = theUnits.size();
    for (; i < theCount; i++)
        if (this->IsCitizenAvailable(theUnits[i]))
            outUnits.push_back(theUnits[i]);
}

// FUNCTION: EXE 0x494174
// Returns the best available citizen to build a unit class at a tile
EEComplexUnit* EECPUnitManager::FindBuilderForTile(U2DPoint<long>& inTile, long inUnitClass)
{
    GE2DPoint theLocation((float)inTile.mX, (float)inTile.mY);
    return this->FindBuilder(theLocation, inUnitClass);
}

// FUNCTION: EXE 0x494197
// Returns the best available citizen to build a unit class at a unit's tile
EEComplexUnit* EECPUnitManager::FindBuilderForTarget(EEUnit* inTarget, long inUnitClass)
{
    GE2DPoint theLocation((float)inTarget->mTileX, (float)inTarget->mTileY);
    return this->FindBuilder(theLocation, inUnitClass);
}

// FUNCTION: EXE 0x4941bb
// Returns the nearest available citizen in the site's path area (or on a shore touching it, for site classes and
// towers); farm builders skip citizens already farming
// NEAR: the original copies the sort predicate from the tile local
EEComplexUnit* EECPUnitManager::FindBuilder(GE2DPoint& inLocation, long inUnitClass)
{
    U2DPoint<long> theTile(UMFloor(inLocation.GetX()), UMFloor(inLocation.GetY()));
    std::vector<EEComplexUnit*> theUnits;
    std::vector<U2DPoint<long> > theTiles;
    EEComplexUnit* theResult = NULL;
    EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(theTile);
    bool theEdge;
    if (gWorld.mMap != NULL && gWorld.mMap->IsEdgeTile(theTile.mX, theTile.GetY()))
        theEdge = true;
    else
        theEdge = false;
    this->GetIdleCitizens(theUnits);
    if (theUnits.size() > 0) {
        std::sort(theUnits.begin(), theUnits.end(), EECPUnitDistanceLess(theTile.mX, theTile.GetY()));
        for (unsigned long i = 0, n = theUnits.size(); i < n; i++) {
            if (theResult)
                break;
            EEComplexUnit* theUnit = theUnits[i];
            if (!theEdge) {
                bool theSame = gContinentManager.GetPathAreaAt(theUnit) == theArea;
                if (!theSame) {
                    if (EECPIsSiteClass(inUnitClass) || inUnitClass == this->mManager->GetTowerClass()) {
                        long theSize = gUnitClassTable[inUnitClass]->mTileSize;
                        theTiles = gContinentManager.GetContinentAt(theUnit->GetTileX(), theUnit->GetTileY())->mShoreTiles;
                        for (unsigned long x = theTile.mX; x < theTile.mX + theSize && !theSame; x++)
                            for (unsigned long y = theTile.GetY(); y < theTile.GetY() + theSize && !theSame; y++)
                                theSame = std::find(theTiles.begin(), theTiles.end(), U2DPoint<long>(x, y)) != theTiles.end();
                    }
                }
                if (!theSame)
                    continue;
                EEUGGather* theGoal;
                if (inUnitClass == 0x2a && (theGoal = dynamic_cast<EEUGGather*>(theUnit->GetLoadGoal())) != NULL &&
                    theGoal->mResource != NULL && theGoal->mResource->GetUnitClass()->mUnitType == kUnitTypeFarm)
                    continue;
            }
            theResult = theUnit;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x49447e
// Collects every tracked citizen of both citizen classes (class 0x1e0 first, then 0x20)
// each deque goes in with vector::insert(begin(), F, L) over deque iterators (COMDAT 0x96506); the deque reference
// local lets begin() and end() share the _Myoff load
void EECPUnitManager::GetCitizens(std::vector<EEComplexUnit*>& outUnits)
{
    std::map<long, std::deque<EEComplexUnit*>*>::iterator it;
    outUnits.clear();
    it = this->mUnits.find(0x20);
    if (it != this->mUnits.end()) {
        std::deque<EEComplexUnit*>& theUnits = *it->second;
        outUnits.insert(outUnits.begin(), theUnits.begin(), theUnits.end());
    }
    it = this->mUnits.find(0x1e0);
    if (it != this->mUnits.end()) {
        std::deque<EEComplexUnit*>& theUnits = *it->second;
        outUnits.insert(outUnits.begin(), theUnits.begin(), theUnits.end());
    }
}

// FUNCTION: EXE 0x494524
// Returns whether a citizen is alive, on the map, attached to the AI, has no buildings queued and is idle or
// gathering
bool EECPUnitManager::IsCitizenAvailable(EEComplexUnit* inUnit)
{
    return inUnit->mHitPoints > 0 && !inUnit->mIsQueuedForDelete && !inUnit->mIsDisabled && !inUnit->mIsDetachedFromAI && inUnit->mTileX != -10000 &&
           inUnit->mTileY != -10000 && inUnit->mCitizenBuildQueue.size() == 0 && (inUnit->mActiveGoal == NULL || inUnit->mActiveGoal->mKind == kGoalGather);
}

// FUNCTION: EXE 0x494572
// Returns the number of citizens and fishing boats
unsigned long EECPUnitManager::GetNumWorkers()
{
    return this->GetCount(this->mManager->GetFishingBoatClass()) + this->GetCount(0x1e0) + this->GetCount(0x20);
}

// FUNCTION: EXE 0x4945a7
// Counts the military units, optionally per class and including requests
long EECPUnitManager::CountMilitaryUnits(bool inByClass, bool inRequests)
{
    EECPUGatherMilitaryUnitsCount theGatherer(this->mPlayer, inByClass, inRequests, true);
    this->GatherUnits(theGatherer);
    return theGatherer.mRequested + theGatherer.mUnits;
}

// FUNCTION: EXE 0x4945de
// Returns the number of tracked units of a class
unsigned long EECPUnitManager::GetCount(long inUnitClass)
{
    std::map<long, std::deque<EEComplexUnit*>*>::iterator theEntry = this->mUnits.find(inUnitClass);
    if (theEntry != this->mUnits.end())
        return theEntry->second->size();
    return 0;
}

// FUNCTION: EXE 0x494608
// Returns the list of tracked units of a class, or NULL
std::deque<EEComplexUnit*>* EECPUnitManager::GetUnits(long inUnitClass)
{
    std::map<long, std::deque<EEComplexUnit*>*>::iterator theEntry = this->mUnits.find(inUnitClass);
    if (theEntry != this->mUnits.end())
        return theEntry->second;
    return NULL;
}

// FUNCTION: EXE 0x49462f
// Reads the requests per epoch (epoch 14 only from version 0.234 on), waiting counts and flags, dropping invalid
// requests; returns the first read error
long EECPUnitManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = 0;
    unsigned long theCount;
    unsigned long i;
    unsigned long j;
    EECPUMRequest theRequest;
    unsigned long theClass;
    unsigned long theValue;
    for (i = 0; i < 15; i++) {
        if (theResult != 0)
            return theResult;
        if (inMajorVersion == 0 && inMinorVersion < 0xea && i == 14)
            continue;
        theResult = inFile.Read(theCount);
        if (theResult != 0)
            continue;
        this->mRequests[i].clear();
        for (j = 0; j < theCount; j++) {
            if (theResult != 0)
                break;
            theResult = inFile.Read(theRequest.mUnitClass);
            if (theResult != 0 || (theResult = inFile.Read(theRequest.mCount)) != 0 ||
                (theResult = inFile.Read(theRequest.mContinent)) != 0 ||
                (theResult = inFile.Read(theRequest.mTownID)) != 0)
                continue;
            EEUnitClass* theUnitClass = gUnitClassTable[theRequest.mUnitClass];
            long theID = theUnitClass != NULL ? theUnitClass->mTechnology : 100000;
            if (theID != kTechNone && theUnitClass != NULL && theRequest.mCount < 300)
                this->mRequests[i].push_back(theRequest);
        }
    }
    if (theResult != 0 || (theResult = inFile.Read(theCount)) != 0)
        return theResult;
    this->mWaitingCounts.clear();
    for (i = 0; i < theCount; i++) {
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(theClass);
        if (theResult == 0 && (theResult = inFile.Read(theValue)) == 0)
            this->mWaitingCounts[theClass] = theValue;
    }
    if (theResult == 0 && (theResult = inFile.Read(this->mNumWaiting)) == 0)
        theResult = inFile.Read(this->mProductionEnabled);
    return theResult;
}

// FUNCTION: EXE 0x4947f3
// Returns whether any request is filed under an epoch later than the current one
bool EECPUnitManager::HasLaterEpochRequests()
{
    bool theFound = false;
    for (long i = this->mPlayer->mTechnologies->mCurrentEpoch + 1; i < 15; i++) {
        if (theFound)
            break;
        if (this->mRequests[i].size() > 0)
            theFound = true;
    }
    return theFound;
}

// FUNCTION: EXE 0x49482b
// Tracks every unit of the player and files its living citizens as idle gatherers
void EECPUnitManager::AddAllUnits()
{
    for (EEUnit* theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        this->UnitAdded(theUnit, 0);
        long theCategory = theUnit->mUnitClass->mCategory;
        if ((theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen) && theUnit->mHitPoints > 0)
            this->mManager->GetResourceManager()->AddIdleGatherer(theUnit->mComplexUnit);
    }
}

// FUNCTION: EXE 0x49487b
// Requests fishing boats unless the player already has or has requested one
void EECPUnitManager::RequestFishingBoats(long inCount)
{
    long theClass = this->mManager->GetFishingBoatClass();
    EECPUGatherSpecialUnitsCount theGatherer(this->mPlayer, theClass, 0, false);
    this->GatherUnits(theGatherer);
    if (theGatherer.mUnits + theGatherer.mRequested == 0)
        this->RequestUnits(theClass, inCount, 0, 0);
}

// The sort predicate of RequestUnits (COMDAT 0x97008, called out of line by the sort COMDATs): the classes by the summed
// costs +0x24..+0x34 of their tech tree nodes, the cheaper first (provisional name)
struct EECPUMCostLess {
    EECPUMCostLess(EETechTree* inTree) : mTree(inTree) {}
    bool operator()(long inFirst, long inSecond) const
    {
        long theFirstCost = 0;
        long theSecondCost = 0;
        EETechTreeNode* theFirst = this->mTree->Get(gUnitClassTable[inFirst] != NULL ? gUnitClassTable[inFirst]->mTechnology : 100000);
        EETechTreeNode* theSecond = this->mTree->Get(gUnitClassTable[inSecond] != NULL ? gUnitClassTable[inSecond]->mTechnology : 100000);
        for (long i = 2; i < 7; i++) {
            theFirstCost += theFirst->mData->mCosts[i];
            theSecondCost += theSecond->mData->mCosts[i];
        }
        return theFirstCost < theSecondCost;
    }
    EETechTree* mTree;
};

// The sort predicate of StartProduction (inlined into the sort COMDATs 0x96bf1..): the units by their queue length
// (provisional name)
struct EECPUMQueueLess {
    bool operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const
        { return inFirst->GetBuildQueueSize() < inSecond->GetBuildQueueSize(); }
};

// FUNCTION: EXE 0x4948e6
// Files a request for units of a class under its epoch (or adds to an existing one) and adds their cost to the
// needs; a new request first needs a producing building, else it requests the cheapest available one
void EECPUnitManager::RequestUnits(long inUnitClass, long inCount, long inTownID, unsigned short inContinent)
{
    long theClass;
    EEUnitClass* theUnitClass;
    bool theFront;
    std::vector<long> theClasses;
    bool theDone = false;
    EECPUMRequest theRequest;
    EEMap* theMap = gWorld.mMap;
    theRequest.mUnitClass = inUnitClass;
    theRequest.mCount = inCount;
    theRequest.mContinent = inContinent;
    theRequest.mTownID = inTownID;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    long theID = gUnitClassTable[inUnitClass] != NULL ? gUnitClassTable[inUnitClass]->mTechnology : 100000;
    long theIndex;
    if (theID >= kTechEpochPrehistoric && theID < kTechEpochAll)
        theIndex = theID;
    else
        theIndex = theID / 1000 - 1;
    EETechTreeNode* theNode = theTree->Get(theID);
    unsigned long i;
    bool theFound = false;
    std::deque<EECPUMRequest>& theRequests = this->mRequests[theIndex];
    unsigned long theSize = theRequests.size();
    for (i = 0; i < theSize; i++) {
        if (theFound)
            return;
        if (theRequests[i].mUnitClass == inUnitClass) {
            this->mManager->GetResourceManager()->AddCosts(theNode, inCount);
            theRequests[i].mCount += inCount;
            this->mWaitingCounts[inUnitClass] += inCount;
            this->mNumWaiting += inCount;
            theFound = true;
        }
    }
    if (theFound)
        return;
    for (i = 0; i < theNode->GetNumUnitClasses(); i++) {
        theClass = theNode->GetUnitClass(i);
        if (this->GetCount(theClass)) {
            theUnitClass = gUnitClassTable[theClass];
            if (theUnitClass == NULL || (theUnitClass->mCategory != kUnitCategoryNavalBuilding && theUnitClass->mCategory != kUnitCategorySpaceBuilding) ||
                theUnitClass->mAirTrafficType == 0)
                theDone = true;
            else
                theDone = EEAIFindUpgradableUnit(theClass, this->mPlayer) != NULL;
            if (theDone)
                goto add;
            break;
        }
        if (theClass != 0x23 && theClass != 0x29) {
            if (theTree->Get(gUnitClassTable[theClass] != NULL ? gUnitClassTable[theClass]->mTechnology : 100000))
                theClasses.push_back(theClass);
        }
    }
    if (theClasses.size() > 0) {
        std::sort(theClasses.begin(), theClasses.end(), EECPUMCostLess(theTree));
        for (i = 0; i < theClasses.size(); i++) {
            theClass = *(theClasses.begin() + i);
            EETechTreeNode* theOther =
                theTree->Get(gUnitClassTable[theClass] != NULL ? gUnitClassTable[theClass]->mTechnology : 100000);
            if (theOther && theOther->GetIsAvailable()) {
                if (theMap != NULL && ((theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (theMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) &&
                    theClass == this->mManager->ChooseTransportClass())
                    theFront = true;
                else
                    theFront = false;
                if (!gUnitClassTable[theClass]->IsLandBuilding())
                    this->RequestUnits(theClass, 1, 0, 0);
                else
                    this->mManager->GetBuildManager()->QueueBuilding(theClass, 0, theFront);
                theDone = true;
            }
        }
        if (theDone) {
        add:
            this->mManager->GetResourceManager()->AddCosts(theNode, inCount);
            theRequests.push_back(theRequest);
            this->mWaitingCounts[inUnitClass] += inCount;
            this->mNumWaiting += inCount;
        }
    }
}

// FUNCTION: EXE 0x494c80
// Releases every tracked unit and clears the waiting counts and every epoch's requests
void EECPUnitManager::Reset()
{
    this->ReleaseUnits();
    for (long i = 0; i < 15; i++)   // every epoch's requests (was the first 7)
        this->mRequests[i].clear();
    this->mWaitingCounts.clear();
}

// FUNCTION: EXE 0x494cac
// Drops the requests filed before an epoch whose class went obsolete before it, releasing their personnel
void EECPUnitManager::DropObsoleteRequests(long inEpoch)
{
    std::deque<EECPUMRequest>::iterator it;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    for (long i = 0; i < inEpoch; i++) {
        std::deque<EECPUMRequest>& theRequests = this->mRequests[i];
        for (it = theRequests.begin(); it != theRequests.end();) {
            long theClass = (*it).mUnitClass;
            long theCount = (*it).mCount;
            long theID = gUnitClassTable[theClass] != NULL ? gUnitClassTable[theClass]->mTechnology : 100000;
            if (theTree->Get(theID)->mObsoleteEpoch < inEpoch) {
                it = theRequests.erase(it, it + 1);
                this->mPlayer->mCombatManager->mPersonnel->ReleaseOfType(theClass, theCount);
            } else
                ++it;
        }
    }
}

// FUNCTION: EXE 0x494d9b
// Re-tracks the player's units after loading when the player is active; returns 0
long EECPUnitManager::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    if (this->mPlayer->mActive)
        this->AddAllUnits();
    return 0;
}

// FUNCTION: EXE 0x494db4
// Writes the requests per epoch, the waiting counts and flags to the saved game; returns the first write error
long EECPUnitManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = 0;
    std::deque<EECPUMRequest>::iterator it;
    std::map<unsigned long, unsigned long>::iterator theEntry;
    unsigned long theCount;
    for (unsigned long i = 0; i < 15; i++) {
        if (theResult != 0)
            return theResult;
        std::deque<EECPUMRequest>& theRequests = this->mRequests[i];
        theCount = theRequests.size();
        theResult = outFile.Write(theCount);
        if (theResult == 0) {
            for (it = theRequests.begin(); it != theRequests.end() && theResult == 0; ++it) {
                EECPUMRequest theRequest = *it;
                theResult = outFile.Write(theRequest.mUnitClass);
                if (theResult != 0)
                    continue;
                theResult = outFile.Write(theRequest.mCount);
                if (theResult != 0)
                    continue;
                theResult = outFile.Write(theRequest.mContinent);
                if (theResult == 0)
                    theResult = outFile.Write(theRequest.mTownID);
            }
        }
    }
    if (theResult == 0) {
        theCount = this->mWaitingCounts.size();
        theResult = outFile.Write(theCount);
        if (theResult == 0) {
            for (theEntry = this->mWaitingCounts.begin(); theEntry != this->mWaitingCounts.end(); ++theEntry) {
                if (theResult != 0)
                    return theResult;
                theResult = outFile.Write(theEntry->first);
                if (theResult == 0)
                    theResult = outFile.Write(theEntry->second);
            }
            if (theResult == 0) {
                theResult = outFile.Write(this->mNumWaiting);
                if (theResult == 0)
                    theResult = outFile.Write(this->mProductionEnabled);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x494f42
// Queues a request's units round-robin at the least busy built producers in its town, on its continent or else on
// the continent with the most; returns true and the count started
bool EECPUnitManager::StartProduction(EECPUMRequest& inRequest, long& outMade)
{
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEComplexUnit*> theCandidates;
    std::map<unsigned short, unsigned long> theCounts;
    std::deque<EEComplexUnit*>::iterator it;
    long theClass = inRequest.mUnitClass;
    long theID = gUnitClassTable[theClass] != NULL ? gUnitClassTable[theClass]->mTechnology : 100000;
    unsigned long theCount = inRequest.mCount;
    bool theResult = false;
    unsigned long theMade = 0;
    EETechTreeNode* theNode = this->mPlayer->mTechnologies->Get(theID);
    unsigned long i;
    for (i = 0; i < theNode->mData->mUnitClasses.size(); i++) {
        long theUnitClass = theNode->GetUnitClass(i);
        std::deque<EEComplexUnit*>* theClassUnits;
        if (!gUnitClassTable[theUnitClass]->IsBuilding() || (theClassUnits = this->GetUnits(theUnitClass)) == NULL)
            continue;
        for (it = theClassUnits->begin(); !(it == theClassUnits->end()); ++it) {
            EEComplexUnit* theUnit = *it;
            if (theUnit->mIsBuilt)
                theUnits.push_back(theUnit);
        }
    }
    if (inRequest.mTownID == 0) {
        if (inRequest.mContinent != 0) {
            for (i = 0; i < theUnits.size(); i++) {
                EEAIContinent* theContinent = gContinentManager.GetContinentAt(theUnits[i]);
                if (theContinent && theContinent->mID == inRequest.mContinent)
                    theCandidates.push_back(theUnits[i]);
            }
        }
    } else {
        for (i = 0; i < theUnits.size(); i++) {
            EETown* theSite = (theUnits[i])->FindNearestTown();
            if (theSite && theSite->mTownID == inRequest.mTownID)
                theCandidates.push_back(theUnits[i]);
        }
    }
    if (theCandidates.size() == 0) {
        for (i = 0; i < theUnits.size(); i++) {
            EEAIContinent* theContinent = gContinentManager.GetContinentAt(theUnits[i]);
            if (theContinent)
                theCounts[theContinent->mID] = theCounts[theContinent->mID] + 1;
        }
        unsigned short theBest = 0;
        unsigned long theMost = 0;
        for (std::map<unsigned short, unsigned long>::iterator theEntry = theCounts.begin(); theEntry != theCounts.end();
             ++theEntry) {
            if (theBest == 0 || theMost < theEntry->second) {
                theBest = theEntry->first;
                theMost = theEntry->second;
            }
        }
        for (i = 0; i < theUnits.size(); i++) {
            EEAIContinent* theContinent = gContinentManager.GetContinentAt(theUnits[i]);
            if (theContinent && theContinent->mID == theBest)
                theCandidates.push_back(theUnits[i]);
        }
    }
    if (theCandidates.size() > 0) {
        if (!(*theCandidates.begin())->GetUnitClass()->IsLandBuilding()) {
            std::vector<EEComplexUnit*> theFree;
            std::vector<EEComplexUnit*>::iterator p;
            for (p = theCandidates.begin(); p != theCandidates.end(); p++)
                if ((*p)->mCargoRoom != 0)
                    theFree.push_back(*p);
            theCandidates.clear();
            for (p = theFree.begin(); p != theFree.end(); p++)
                theCandidates.push_back(*p);
        }
        std::sort(theCandidates.begin(), theCandidates.end(), EECPUMQueueLess());
        long j = 0;
        theMade = theCount;
        for (; theCount != 0; theCount--) {
            theCandidates[j]->QueueResearch(theID, false);
            j++;
            if (j == (long)theCandidates.size())
                j = 0;
        }
        theResult = true;
        if (theMade <= this->mWaitingCounts[theClass])
            this->mWaitingCounts[theClass] -= theMade;
        if ((long)theMade <= this->mNumWaiting)
            this->mNumWaiting -= theMade;
        this->mNumInProduction += theMade;
    }
    outMade = theMade;
    return theResult;
}

// FUNCTION: EXE 0x495440
// Tracks a new unit by class (buildings ordered by distance from the town center) and counts one unit out of
// production
// NEAR: shape so far: std::find for the membership test, the existing-queue branch first, *it read once
void EECPUnitManager::UnitAdded(EEUnit* inUnit, long inArg)
{
    GE2DPoint theLocation((float)inUnit->mTileX, (float)inUnit->mTileY);
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    long theType = inUnit->mUnitClass->mUnitType;
    long theCategory = theUnit != NULL ? theUnit->mUnitClass->mCategory : 9;
    std::deque<EEComplexUnit*>::iterator it;
    EETileDistanceLess theCenter(this->mManager->GetBuildManager()->mTownCenterTile);
    if (theUnit == NULL || theCategory == kUnitCategoryProjectile || theCategory == kUnitCategoryBomb || theCategory == kUnitCategoryMissile || theType == kUnitTypePathPoint)
        return;
    std::deque<EEComplexUnit*>* theUnits = this->GetUnits(theType);
    if (theUnits != NULL) {
        it = std::find(theUnits->begin(), theUnits->end(), theUnit);
        if (it == theUnits->end()) {
            theUnit->Reference();
            if (!inUnit->mUnitClass->IsLandBuilding() || theCenter.mCenter.mX == -10000 || theCenter.mCenter.mY == -10000)
                theUnits->push_back(theUnit);
            else {
                for (it = theUnits->begin(); it != theUnits->end(); ++it) {
                    EEComplexUnit* theOtherUnit = *it;
                    if (theOtherUnit == theUnit)
                        continue;
                    GE2DPoint theOther((float)theOtherUnit->mTileX, (float)theOtherUnit->mTileY);
                    EETileCoordinate theFirst;
                    theFirst.mX = (long)theLocation.GetX();
                    theFirst.mY = (long)theLocation.GetY();
                    EETileCoordinate theSecond;
                    theSecond.mX = (long)theOther.GetX();
                    theSecond.mY = (long)theOther.GetY();
                    if (theCenter.DistanceSquared(theFirst) < theCenter.DistanceSquared(theSecond))
                        break;
                }
                theUnits->insert(it, theUnit);
            }
        }
    } else {
        theUnits = new std::deque<EEComplexUnit*>;
        theUnit->Reference();
        theUnits->push_front(theUnit);
        this->mUnits.insert(std::map<long, std::deque<EEComplexUnit*>*>::value_type(theType, theUnits));
        if (theCenter.mCenter.mX == -10000 && theCenter.mCenter.mY == -10000 && !theUnit->mUnitClass->IsLandBuilding())
            this->mManager->GetBuildManager()->SetTownCenter(theUnit);
    }
    if (this->mNumInProduction > 0 && !inUnit->mUnitClass->IsLandBuilding())
        this->mNumInProduction--;
}

// FUNCTION: EXE 0x4956b9
// Stops tracking a lost unit; for a lost building, re-requests the units it was producing or reschedules an
// interrupted epoch advance
void EECPUnitManager::UnitLost(EEUnit* inUnit)
{
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    long theType = inUnit->mUnitClass->mUnitType;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    long theCategory = theUnit != NULL ? theUnit->mUnitClass->mCategory : 9;
    long theSite;
    unsigned short theContinent;
    if (this->mUnits.size() > 0 && theUnit != NULL && theCategory != kUnitCategoryProjectile && theCategory != kUnitCategoryBomb && theCategory != kUnitCategoryMissile &&
        theType != kUnitTypePathPoint) {
        std::deque<EEComplexUnit*>* theUnits = this->GetUnits(theUnit->mUnitClass->mUnitType);
        std::deque<EEComplexUnit*>::iterator it;
        it = std::find(theUnits->begin(), theUnits->end(), theUnit);
        if (it != theUnits->end()) {
            theUnits->erase(it, it + 1);
            theUnit->Dereference();
            if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen)
                this->mManager->GetBuildManager()->BuilderLost(theUnit);
        }
    }
    if (UShutdownInProgress() || theUnit == NULL || !theUnit->mUnitClass->IsLandBuilding() || !theUnit->mIsBuilt)
        return;
    theSite = theUnit->FindNearestTown() != NULL ? theUnit->FindNearestTown()->mTownID : 0;
    theContinent = gContinentManager.GetContinentID(theUnit);
    for (unsigned long i = 0; i < theUnit->GetBuildQueueSize(); i++) {
        long theID = (*(theUnit->mBuildQueue.begin() + i)).mTechnology;
        if (!(theID < kTechEpochPrehistoric) && !(theID >= kTechEpochAll))
            continue;
        EETechTreeNode* theNode = theTree->Get(theID);
        if (!theNode || !theNode->mData->mMakesUnit)
            continue;
        theNode = theTree->Get(theID);
        this->RequestUnits(theNode->mData->mUnitClass, 1, theSite, theContinent);
        this->mNumInProduction--;
    }
    long theID = theUnit->mCurrentResearch;
    if (theID == kTechNone)
        return;
    if (theID >= kTechEpochPrehistoric && theID < kTechEpochAll) {
        this->mManager->mAdvancingEpoch = false;
        this->mManager->mScheduleEpochAdvance = true;
        this->mManager->SetNextEpochAdvanceTime(gPhysics->mGameTime, true);
    } else {
        EETechTreeNode* theNode = theTree->Get(theID);
        if (theNode && theNode->mData->mMakesUnit) {
            theNode = theTree->Get(theID);
            this->RequestUnits(theNode->mData->mUnitClass, 1, theSite, theContinent);
            this->mNumInProduction--;
        }
    }
}

// FUNCTION: EXE 0x495929
// Requests citizens when below target, starts one transport, then starts the requests of each epoch up to the
// current one while population room remains
void EECPUnitManager::Update(unsigned long inTime)
{
    EEMap* theMap = gWorld.mMap;
    std::deque<EECPUMRequest>::iterator it;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    long theWorker = this->mManager->ChooseTransportClass();
    unsigned long theEpoch = theTree->mCurrentEpoch;
    bool theSmall;
    if (theMap != NULL && (theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap)
        theSmall = true;
    else
        theSmall = false;
    bool theDone = false;
    long theRoom = this->mPlayer->GetPopulationLimit(0) - this->mNumInProduction - this->mPlayer->GetTotalPopulation(true, false);
    bool theRequested;
    if (this->IsRequested(0x20) || this->IsRequested(0x1e0))
        theRequested = true;
    else
        theRequested = false;
    bool theQueued = this->IsInProduction(0x20) || this->IsInProduction(0x1e0);
    if (theRoom > 0 && this->mProductionEnabled && !theRequested && !theQueued) {
        long theMissing = this->GetWantedCitizens() - this->GetNumWorkers();
        if (theMissing > 0) {
            if (!theSmall) {
                if (theMissing > 9)
                    theMissing = 10;
            } else if (theMissing > 4)
                theMissing = 5;
            this->RequestUnits(0x20, theMissing, 0, 0);
        }
    }
    unsigned long i;
    long theMade;
    for (i = 0; i < 15; i++) {
        if (theDone)
            break;
        std::deque<EECPUMRequest>& theRequests = this->mRequests[i];
        for (it = theRequests.begin(); it != theRequests.end() && !theDone; ++it) {
            if ((*it).mUnitClass != theWorker)
                continue;
            EETechTreeNode* theNode = theTree->Get(gUnitClassTable[(*it).mUnitClass]->mTechnology);
            if (this->mPlayer->CanAfford(theNode, (*it).mUnitClass, 1)) {
                long theCount = (*it).mCount;
                (*it).mCount = 1;
                if (!this->StartProduction(*it, theMade))
                    (*it).mCount = theCount;
                else {
                    (*it).mCount = theCount - theMade;
                    if ((*it).mCount == 0)
                        theRequests.erase(it, it + 1);
                    theDone = true;
                }
            }
        }
    }
    if (theRoom <= 0)
        return;
    for (i = 0; ; ) {
        if (theRoom < 1)
            return;
        std::deque<EECPUMRequest>& theRequests = this->mRequests[i];
        for (it = theRequests.begin(); it != theRequests.end() && theRoom > 0;) {
            EEUnitClass* theClass = gUnitClassTable[(*it).mUnitClass];
            EETechTreeNode* theNode = theTree->Get(theClass->mTechnology);
            long theSize = theClass->GetPopulationCost();
            bool theAll = !((unsigned long)theRoom < (*it).mCount * theSize);
            if (theNode->GetIsObsolete() || (*it).mCount == 0) {
                it = theRequests.erase(it, it + 1);
                continue;
            }
            bool theAfford = this->mPlayer->CanAfford(theNode, (*it).mUnitClass, (*it).mCount);
            if (!theAll) {
                if (theSize < theRoom && this->mPlayer->CanAfford(theNode, (*it).mUnitClass, 1)) {
                    long theCount = (*it).mCount;
                    (*it).mCount = 1;
                    if (!this->StartProduction(*it, theMade))
                        (*it).mCount = theCount;
                    else {
                        theRoom -= theSize * theMade;
                        (*it).mCount = theCount - theMade;
                    }
                }
                it++;
            } else if (!theAfford) {
                if (this->mPlayer->CanAfford(theNode, (*it).mUnitClass, 1)) {
                    if ((*it).mUnitClass != 0x20 && (*it).mUnitClass != 0x1e0) {
                        unsigned long theWorkers = this->GetNumWorkers();
                        if (theWorkers < this->mPlayer->GetTotalPopulation(false, false) - theWorkers && theRequested) {
                            ++it;
                            continue;
                        }
                    }
                    long theCount = (*it).mCount;
                    (*it).mCount = 1;
                    if (!this->StartProduction(*it, theMade))
                        (*it).mCount = theCount;
                    else {
                        theRoom -= theSize * theMade;
                        (*it).mCount = theCount - theMade;
                    }
                }
                ++it;
            } else {
                if (!this->StartProduction(*it, theMade)) {
                    ++it;
                    continue;
                }
                theRoom -= theSize * theMade;
                (*it).mCount -= theMade;
                if ((*it).mCount == 0)
                    it = theRequests.erase(it, it + 1);
            }
        }
        i++;
        if (theEpoch < i)
            return;
    }
}

// TEMPLATE: EXE 0x495eaf
// TEMPLATE: EXE 0x496506
// TEMPLATE: EXE 0x496854

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was compared
// against the original by name)
// TEMPLATE: EXE 0x495e70
// TEMPLATE: EXE 0x495fed
// TEMPLATE: EXE 0x495fff
// TEMPLATE: EXE 0x49602a
// TEMPLATE: EXE 0x49603e
// TEMPLATE: EXE 0x496060
// TEMPLATE: EXE 0x4961a3
// TEMPLATE: EXE 0x496214
// TEMPLATE: EXE 0x496372
// TEMPLATE: EXE 0x4963a4
// TEMPLATE: EXE 0x4963d6
// TEMPLATE: EXE 0x496422
// TEMPLATE: EXE 0x496533
// TEMPLATE: EXE 0x4965f0
// TEMPLATE: EXE 0x4966ad
// TEMPLATE: EXE 0x496812
// TEMPLATE: EXE 0x496ac4
// TEMPLATE: EXE 0x496aed
// TEMPLATE: EXE 0x496bc0
// TEMPLATE: EXE 0x496bf1
// TEMPLATE: EXE 0x496c1a
// TEMPLATE: EXE 0x496ced
// TEMPLATE: EXE 0x496daf
// TEMPLATE: EXE 0x496ecf
// TEMPLATE: EXE 0x496f02
// TEMPLATE: EXE 0x496f38
// TEMPLATE: EXE 0x496f6b
// TEMPLATE: EXE 0x497089
// TEMPLATE: EXE 0x4970c3
// TEMPLATE: EXE 0x497104
// TEMPLATE: EXE 0x497224
// TEMPLATE: EXE 0x497257
// TEMPLATE: EXE 0x4972ff
// TEMPLATE: EXE 0x497339
// TEMPLATE: EXE 0x49750c
// TEMPLATE: EXE 0x4975d1
// TEMPLATE: EXE 0x497615
// TEMPLATE: EXE 0x49763e
// TEMPLATE: EXE 0x497682
// TEMPLATE: EXE 0x49771b
// TEMPLATE: EXE 0x497789
// TEMPLATE: EXE 0x4977dc
// TEMPLATE: EXE 0x49785b
// TEMPLATE: EXE 0x4978f2
// TEMPLATE: EXE 0x497941
// TEMPLATE: EXE 0x497975
// TEMPLATE: EXE 0x4979cb
// The std::sort helpers of EECPUnitDistanceLess (exact body pairs, names in names.txt)
// TEMPLATE: EXE 0x4966d7
// TEMPLATE: EXE 0x497541
// TEMPLATE: EXE 0x4978ae
// TEMPLATE: EXE 0x495e9b
// TEMPLATE: EXE 0x4960f3
// TEMPLATE: EXE 0x496356
// TEMPLATE: EXE 0x497008
// TEMPLATE: EXE 0x49737a

// Template COMDATs named by their mangled names
// TEMPLATE: EXE 0x49670a
// ?_Sort@std@@YAXV?$_Ptrit@PAVEEComplexUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0HUEECPUnitDistanceLess@@@Z
// TEMPLATE: EXE 0x496d1e
// ?make_heap@std@@YAXV?$_Ptrit@PAVEEComplexUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPUnitDistanceLess@@@Z
// TEMPLATE: EXE 0x496d62
// ?sort_heap@std@@YAXV?$_Ptrit@PAVEEComplexUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPUnitDistanceLess@@@Z
// TEMPLATE: EXE 0x49759e
// ?pop_heap@std@@YAXV?$_Ptrit@PAVEEComplexUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPUnitDistanceLess@@@Z
// TEMPLATE: EXE 0x4976ab
// ?_Pop_heap_0@std@@YAXV?$_Ptrit@PAVEEComplexUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPUnitDistanceLess@@PAPAVEEComplexUnit@@@Z

// Template COMDATs named by their mangled names
// TEMPLATE: EXE 0x4963ff
// ?find@std@@YA?AV?$_Ptrit@UEEProductionEntry@@HPAU1@AAU1@PAU1@AAU1@@1@V21@0ABJ@Z
