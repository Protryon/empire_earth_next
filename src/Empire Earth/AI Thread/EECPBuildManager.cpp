// C:\EEX\Empire Earth\AI Thread\EECPBuildManager.cpp (file name inferred: no __FILE__; RTTI class
// EECPBuildManager, vtable 0x43c050; its key "EECPBuildManager-Data" names the class). A computer player's build
// manager: 0x66af0..0x6e8d4 (the class-static key's guarded chain first, then three distance predicates built without
// /Og, the members in address order; the deque/vector/heap/sort COMDATs close the unit; EECPAttackSubGroupUnit.cpp ends
// at 0x66af0 and EECPCivilizationManager.cpp's key chain starts at 0x6e8d4).
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
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EETown.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EEAIRandom.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEAEManager.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPUnitTileDistanceLess.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUGLocation.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPWallManager.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEUnitCollector.h"

// the class key (0x4e5d00, guard 0x4e5cf8 bit 1)
const UString EECPBuildManager::sData("EECPBuildManager-Data");
// SYNTHETIC: EXE 0x466af0   (wrapper)
// SYNTHETIC: EXE 0x466afa   (ctor)
// SYNTHETIC: EXE 0x466b0b   (registrar)
// SYNTHETIC: EXE 0x466b17   (dtor)

// TEMPLATE: EXE 0x466b33
// EETileDistanceLess::operator()(const EETileCoordinate&, const EETileCoordinate&): the header's inline body, out of
// line and built without /Og (the sorts 0x6cf9e.. call it; EECPWallManager/EEUGBuildRepair copies ICF-fold here)

// FUNCTION: EXE 0x466b6d
// Orders two (continent, town) pairs by the distance of the continent's centre from the reference tile
// Built without /Og (frame locals, `this` spilled to [ebp-0x3c]); the -4 -> -0x20 bool copy is the inline (tile, tile)
// operator's own result. NEAR: #pragma optimize("g", off) alone gives 45 (frame 0x20 vs 0x3c), not exact: removed
bool EETileDistanceLess::operator()(const std::pair<EEAIContinent*, EETown*>& inFirst,
    const std::pair<EEAIContinent*, EETown*>& inSecond) const
{
    const U2DPoint<long>& theFirstCenter = inFirst.first->mHull.GetCenter();
    EETileCoordinate theFirst;
    theFirst.mX = theFirstCenter.mX;
    theFirst.mY = theFirstCenter.mY;
    const U2DPoint<long>& theSecondCenter = inSecond.first->mHull.GetCenter();
    EETileCoordinate theSecond;
    theSecond.mX = theSecondCenter.mX;
    theSecond.mY = theSecondCenter.mY;
    return (*this)(theFirst, theSecond);
}

// FUNCTION: EXE 0x466be7
// Orders two choke points by the distance of their centre from the reference tile
// Built without /Og, as 0x466b6d
bool EETileDistanceLess::operator()(EEAIChokePoint* inFirst, EEAIChokePoint* inSecond) const
{
    const U2DPoint<long>& theFirstCenter = inFirst->mHull.GetCenter();
    EETileCoordinate theFirst;
    theFirst.mX = theFirstCenter.mX;
    theFirst.mY = theFirstCenter.mY;
    const U2DPoint<long>& theSecondCenter = inSecond->mHull.GetCenter();
    EETileCoordinate theSecond;
    theSecond.mX = theSecondCenter.mX;
    theSecond.mY = theSecondCenter.mY;
    return (*this)(theFirst, theSecond);
}

// FUNCTION: EXE 0x466c5d
// Creates the build manager for a civilization manager's player, with empty queues and no town centre tile
EECPBuildManager::EECPBuildManager(EECPCivilizationManager* inManager)
    : mManager(inManager)
{
    this->mPlayer = inManager->mPlayer;
    this->mTownCenterTile = U2DPoint<long>(-10000, -10000);
    this->mNextResourceCenter = 0;
    this->mNextBuilderCheckTime = 0;
    this->mNextUpdateTime = 0;
    this->mSiteTilesCollected = false;
}

// FUNCTION: EXE 0x466d17
// Destroys the build manager (members only)
EECPBuildManager::~EECPBuildManager()
{
}

// FUNCTION: EXE 0x466d50
// Tells whether the player has a unit of the given type within the squared distance of a tile
bool EECPBuildManager::HasUnitNear(long inUnitType, long inX, long inY, float inDistance)
{
    std::vector<EEUnit*> theUnits;
    EEGetPlayerUnitsOfType(inUnitType, this->mPlayer->mPlayerID, theUnits);
    EECPUnitWithinDistance thePredicate(inX, inY, inDistance);
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(), thePredicate);
    return theIt != theUnits.end();
}

// FUNCTION: EXE 0x466de3
// Tells whether one of the player's anti-air buildings covers a tile (within its range)
// NEAR (69): the two mF9C reads and the product order; temporary vs named predicate
bool EECPBuildManager::HasAntiAirNear(long inX, long inY)
{
    std::vector<EEUnit*> theUnits;
    long theType = this->mPlayer->mCivilizationManager->GetAntiAirClass();
    float theFirst = this->mPlayer->mUnitClassUpgrades[theType]->mRange;
    float theSecond = this->mPlayer->mUnitClassUpgrades[theType]->mRange;
    EEGetPlayerUnitsOfType(theType, this->mPlayer->mPlayerID, theUnits);
    EECPUnitWithinDistance thePredicate(inX, inY, theSecond * theFirst);
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(), thePredicate);
    return theIt != theUnits.end();
}

// FUNCTION: EXE 0x466ee8
// Tells whether one of the player's settlements, town centres or capitols is within the distance of a tile
// NEAR (99.5): the predicate temporary's r*r store comes before mCenter.mY here, after it in the original (header
// ctor probes in build/a: body assignment, const-reference x/y: no change)
bool EECPBuildManager::HasTownCenterNear(long inX, long inY, float inDistance)
{
    std::vector<EEUnit*> the2B;
    std::vector<EEUnit*> the23;
    std::vector<EEUnit*> the29;
    std::vector<EEUnit*> theOthers;
    std::vector<EEUnit*> theUnits;
    EEGetPlayerUnitsOfType(kUnitTypeSettlement, this->mPlayer->mPlayerID, the2B);
    EEGetPlayerUnitsOfType(kUnitTypeTownCenter, this->mPlayer->mPlayerID, the23);
    EEGetPlayerUnitsOfType(kUnitTypeCapitol, this->mPlayer->mPlayerID, the29);
    theUnits.insert(theUnits.end(), the2B.begin(), the2B.end());
    theUnits.insert(theUnits.end(), the23.begin(), the23.end());
    theUnits.insert(theUnits.end(), the29.begin(), the29.end());
    theUnits.insert(theUnits.end(), theOthers.begin(), theOthers.end());
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(),
        EECPUnitWithinDistance(inX, inY, inDistance * inDistance));
    return theIt != theUnits.end();
}

// FUNCTION: EXE 0x467122
// Tells whether one of the player's gates is within the radius of a tile
bool EECPBuildManager::HasGateNear(long inX, long inY, float inRadius)
{
    std::vector<EEUnit*> theUnits;
    float theDistance = inRadius * inRadius;
    EEGetPlayerUnitsOfType(this->mManager->GetGateClass(), this->mPlayer->GetPlayerID(), theUnits);
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(),
        EECPUnitWithinDistance(inX, inY, theDistance));
    return theIt != theUnits.end();
}

// FUNCTION: EXE 0x4671c2
// Tells whether one of the player's towers is within a tile's squared distance (inRadius is used as squared)
bool EECPBuildManager::HasTowerNear(long inX, long inY, float inRadius)
{
    std::vector<EEUnit*> theUnits;
    long theType = this->mManager->GetTowerClass();
    EEGetPlayerUnitsOfType(theType, this->mPlayer->mPlayerID, theUnits);
    EECPUnitWithinDistance thePredicate(inX, inY, inRadius);
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(), thePredicate);
    return theIt != theUnits.end();
}

// FUNCTION: EXE 0x46725e
// Tells whether one of the player's walls is within a tile's squared distance (inRadius is used as squared)
bool EECPBuildManager::HasWallNear(long inX, long inY, float inRadius)
{
    std::vector<EEUnit*> theUnits;
    long theType = this->mManager->GetWallClass();
    EEGetPlayerUnitsOfType(theType, this->mPlayer->mPlayerID, theUnits);
    EECPUnitWithinDistance thePredicate(inX, inY, inRadius);
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(), thePredicate);
    return theIt != theUnits.end();
}

// FUNCTION: EXE 0x4672fa
// Tells whether a building class is waiting in any epoch's build queue
bool EECPBuildManager::IsQueued(long inUnitClass)
{
    bool theFound = false;
    for (unsigned long i = 0; i < 15 && !theFound; i++) {
        if (this->mQueues[i].size() > 0) {
            for (EECPBuildQueue::iterator it = this->mQueues[i].begin(); it != this->mQueues[i].end() && !theFound; ++it)
                theFound = (*it).first == inUnitClass;
        }
    }
    return theFound;
}


// The queue a class goes to: its tech tree id (+0x1e4) when it is an epoch (0..14), else id / 1000 - 1 (every user
// expands it) (provisional name)
static inline long EECPBMQueueIndex(long inTechTreeID)
{
    if (inTechTreeID < 0 || inTechTreeID >= kTechEpochAll)
        return inTechTreeID / 1000 - 1;
    return inTechTreeID;
}

// FUNCTION: EXE 0x467356
// Replaces a lost builder of an unfinished building: orders another citizen to build it, or puts the
// building back at the front of its queue when none is free
void EECPBuildManager::BuilderLost(EEComplexUnit* inUnit)
{
    EECPBuildManager* theThis = this;
    EEUGBuildRepair* theGoal = dynamic_cast<EEUGBuildRepair*>(inUnit->GetLoadGoal());
    if (UShutdownInProgress() || theGoal == NULL)
        return;
    EEUnit* theTargetUnit = theGoal->mTarget;
    EEComplexUnit* theTarget = dynamic_cast<EEComplexUnit*>(theTargetUnit);
    if (theTarget == NULL || !theTarget->IsFoundation())
        return;
    EEComplexUnit* theWorker;
    {
        long theUnitType = theTarget->mUnitClass->mUnitType;
        U2DPoint<long> theTile(theTarget->mTileX, theTarget->mTileY);
        EECPUnitManager* theUnitManager = this->mManager->mUnitManager;
        theWorker = theUnitManager->FindBuilderForTile(theTile, theUnitType);
    }
    if (theWorker != NULL) {
        EEUGBuildRepair* theNew = new EEUGBuildRepair(theWorker->GetPlayer(), theTarget, false);
        EEUnitGoal* theOld = theWorker->GetLoadGoal();
        if (theOld != NULL && theOld->mKind == kGoalGather && theTarget->mUnitClass->mUnitType != kUnitTypeFarm)
            theWorker->SetGoal(theNew, 1, 0);
        else {
            theWorker->ClearGoalQueue(0);
            theWorker->SetGoal(theNew, 0, 0);
        }
        theWorker->StartAction(kActionMoveToRepairSite);
        theWorker->mUnitClass->ActionChanged(theWorker);
    } else {
        unsigned long theClass = theGoal->mTargetType;
        long theID = gUnitClassTable[theClass] != NULL ? gUnitClassTable[theClass]->mTechnology : 100000;
        long theIndex = EECPBMQueueIndex(theID);
        EECPBuildRequest theRequest(theClass, U2DPoint<long>(theTarget->mTileX, theTarget->mTileY));
        theThis->mQueues[theIndex].push_front(theRequest);
    }
}

// FUNCTION: EXE 0x4674c4
// Starts a queued building: walls go to the wall manager; else finds a site and builder and orders the
// build; returns whether the request is done (also for classes that may be skipped when no builder exists)
// NEAR (92): the tile local types (P1/P5)
bool EECPBuildManager::StartBuilding(EECPBuildRequest inRequest)
{
    std::vector<EEComplexUnit*> theWorkers;
    std::vector<EEUnit*> theUnused1;
    std::vector<EEUnit*> theUnused2;
    EETileCoordinate theTile(0, 0);
    std::vector<EEUnit*> theUnused3;
    EECPBuildManager* theThis = this;
    EEUnitClass* theClass = gUnitClassTable[inRequest.first];
    EECPUnitManager* theUnitManager = this->mManager->mUnitManager;
    long theClassID = inRequest.first;
    long theID = theClass != NULL ? theClass->mTechnology : 100000;
    long theIndex = EECPBMQueueIndex(theID);
    bool theResult = false;
    theTile.mX = inRequest.second.mX;
    theTile.mY = inRequest.second.mY;
    bool theHasTile = theTile.mX != 0 && theTile.mY != 0;
    if (theClass != NULL && theClass->mIsWall && !theClass->mIsTower) {
        if (theHasTile)
            this->mManager->mWallManager->BuildWall(inRequest.second, inRequest.second, theClassID, false);
        theResult = true;
    } else {
        this->mManager->mUnitManager->GetIdleCitizens(theWorkers);
        if (theWorkers.size() > 0) {
            EEComplexUnit* theWorker = NULL;
            if (theHasTile || this->FindSite(theClassID, theTile)) {
                EECPUnitManager* theManager = theThis->mManager->mUnitManager;
                theWorker = theManager->FindBuilderForTile(theTile, theClassID);
            }
            if (theWorker != NULL) {
                EEUnit* theTarget;
                if (theClassID == kUnitTypeFarm || theClassID == kUnitTypeRoboticFarm) {
                    RTSTileUnitNode* theNode = gWorld.mMap->GetTileAtLocation(theTile)->mUnits;
                    theTarget = NULL;
                    for (; theNode != NULL; theNode = theNode->mNext) {
                        if (theNode->mUnit != NULL && theNode->mUnit->mUnitClass->mUnitType == theClassID)
                            theTarget = theNode->mUnit;
                    }
                    if (theTarget == NULL) {
                        inRequest.second.mX = 0;
                        inRequest.second.mY = 0;
                        theThis->mQueues[theIndex].push_front(inRequest);
                    }
                } else
                    theTarget = EEMakeBuilding(theThis->mPlayer, theClassID, theTile.mX, theTile.mY);
                if (theTarget != NULL) {   // a requeued farm request orders no one (was a build goal on NULL)
                    EEUGBuildRepair* theGoal = new EEUGBuildRepair(theWorker->GetPlayer(), theTarget, false);
                    theWorker->ClearGoalQueue(0);
                    theWorker->SetGoal(theGoal, 0, 0);
                    theWorker->StartAction(kActionMoveToRepairSite);
                    theWorker->mUnitClass->ActionChanged(theWorker);
                }
                theResult = true;
            } else {
                bool theSpace = (gWorld.mMap->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap;
                theResult = theClassID == kUnitTypeTemple || theClassID == kUnitTypeUniversity || theClassID == kUnitTypeHospital || theClassID == kUnitTypeHouse
                    || theClassID == kUnitTypeFarm || theClassID == kUnitTypeRoboticFarm || theClassID == kUnitTypeGranary || theClassID == kUnitTypePriestTower
                    || (theClassID == kUnitTypeDock && theUnitManager->GetCount(0x24) > 0)
                    || (theClassID == kUnitTypeNavyYard && theUnitManager->GetCount(0x41) > 0)
                    || (theClassID == kUnitTypeDock && theSpace) || (theClassID == kUnitTypeNavyYard && theSpace)
                    || (theClassID == kUnitTypeAirport && theUnitManager->GetCount(0x42) > 0)
                    || (theClassID == kUnitTypeSpaceDock && theUnitManager->GetCount(0x2d8) > 0);
            }
        }
    }
    return theResult;
}

// AssignBuilders's two worker tests (find_if 0x6bdbe/0x6be1f; their operator() 0x6bde5/0x6be46 follow among the COMDATs)
// (provisional names)
struct EECPBMBuildsTarget {
    EECPBMBuildsTarget(EEUnit* inTarget) : mTarget(inTarget) {}
    bool operator()(EEComplexUnit* inWorker);

    EEUnit* mTarget;                        // +0x00
};


bool EECPBMBuildsTarget::operator()(EEComplexUnit* inWorker)
{
    EEUGBuildRepair* theGoal = dynamic_cast<EEUGBuildRepair*>(inWorker->GetLoadGoal());
    return theGoal != NULL && theGoal->mTarget == this->mTarget;
}

struct EECPBMQueuedTarget {
    EECPBMQueuedTarget(EEUnit* inTarget) : mTarget(inTarget) {}
    bool operator()(EEComplexUnit* inWorker)
    {
        bool theFound = false;
        for (unsigned long i = 0; i < inWorker->mCitizenBuildQueue.size() && !theFound; i++)
            theFound = inWorker->mCitizenBuildQueue[i] == this->mTarget;
        return theFound;
    }

    EEUnit* mTarget;                        // +0x00
};

// FUNCTION: EXE 0x4677c6
// Makes sure every unfinished building has a builder: idles a citizen that only has it queued, else orders
// the best free citizen to build it
// GetLoadGoal() (+0x1ec) and a named iterator for both find_if results give the original's code
void EECPBuildManager::AssignBuilders()
{
    std::vector<EEUnit*> theBuildings;
    std::vector<EEComplexUnit*> theWorkers;
    EECPBuildManager* theThis = this;
    std::vector<EEComplexUnit*>::iterator theFound;
    EEUnit* theUnit = this->mPlayer->mFirstUnit;
    EECPUnitManager* theUnitManager = this->mManager->mUnitManager;
    for (; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (!theUnit->mUnitClass->IsLandBuilding())
            continue;
        if (theUnit->mComplexUnit->IsFoundation()
            || (theUnit->mComplexUnit->IsRepairable() && !theUnit->mComplexUnit->mIsBuilt))
            theBuildings.push_back(theUnit);
    }
    this->mManager->mUnitManager->GetCitizens(theWorkers);
    for (std::vector<EEUnit*>::iterator it = theBuildings.begin(); it != theBuildings.end(); it++) {
        theFound = std::find_if(theWorkers.begin(), theWorkers.end(), EECPBMBuildsTarget(*it));
        if (theFound != theWorkers.end())
            continue;
        theFound = std::find_if(theWorkers.begin(), theWorkers.end(), EECPBMQueuedTarget(*it));
        if (theFound != theWorkers.end()) {
            if (dynamic_cast<EEUGBuildRepair*>((*theFound)->GetLoadGoal()) == NULL) {
                (*theFound)->ClearGoalQueue(0);
                (*theFound)->StartAction(kActionIdle);
                (*theFound)->mUnitClass->ActionChanged(*theFound);
            }
        } else {
            EEComplexUnit* theWorker = theUnitManager->FindBuilderForTarget(*it, (*it)->mUnitClass->mUnitType);
            if (theWorker != NULL) {
                EEUGBuildRepair* theGoal = new EEUGBuildRepair(theThis->mPlayer, *it, false);
                theWorker->ClearGoalQueue(0);
                theWorker->SetGoal(theGoal, 0, 0);
                theWorker->StartAction(theGoal->GetStartAction());
                theWorker->mUnitClass->ActionChanged(theWorker);
            }
        }
    }
}

// FUNCTION: EXE 0x4679d7
// Cancels every stalled build: requeues it in front, destroys the foundation, refunds its cost and sends
// the builder back to the town centre (or idles it)
// NEAR (60): the original refuses the request's U2DPoint copy ctor (0x44d28b) and loads the player's vptr first
void EECPBuildManager::CancelStalledBuilds()
{
    EEComplexUnit* theUnit = this->mManager->mUnitManager->FindStalledBuilder();
    while (theUnit != NULL) {
        EEUGBuildRepair* theGoal = dynamic_cast<EEUGBuildRepair*>(theUnit->GetLoadGoal());
        long theClassID = theGoal->mTargetType;
        long theID = gUnitClassTable[theClassID] != NULL ? gUnitClassTable[theClassID]->mTechnology
            : 100000;
        long theIndex = EECPBMQueueIndex(theID);
        this->mQueues[theIndex].push_front(EECPBuildRequest(theClassID,
            U2DPoint<long>(theGoal->mTarget->mTileX, theGoal->mTarget->mTileY)));
        theGoal->mTarget->Kill(0, 0, 0);
        for (unsigned long i = 2; i < 7; i++)
            this->mPlayer->AddResource(i, this->mPlayer->ScaleCost(theGoal->mTargetNode->GetCost(i, 0x20, 1.0f)));
        if (this->mTownCenterTile.mX != -10000 && this->mTownCenterTile.mY != -10000) {
            theUnit->ClearGoalQueue(0);
            theUnit->SetGoal(new EEUGLocation((float)this->mTownCenterTile.GetX(), (float)this->mTownCenterTile.GetY(), 0.0f, false, true,
                false, 0.0f, false, false), 0, 0);
            theUnit->StartAction(kActionPrepareToMove);
        } else
            theUnit->StartAction(kActionIdle);
        theUnit->mUnitClass->ActionChanged(theUnit);
        theUnit = this->mManager->mUnitManager->FindStalledBuilder();
    }
}




// FUNCTION: EXE 0x467b87
// Collects candidate dock tiles (kind 2: water) or space dock tiles (kind 4: space) on the largest such area
// next to the town centre's continent, nearest the town first; then marks the site tiles collected
void EECPBuildManager::CollectSiteTiles(char inKind)
{
    std::vector<U2DPoint<long> > theRegion;
    std::vector<U2DPoint<long> > theRing;
    U2DPoint<long> theCenter;
    EEMap* theMap = gWorld.mMap;
    std::vector<U2DPoint<long> >& theTiles = inKind == 2 ? this->mWaterSites : this->mSpaceSites;
    theCenter = this->mManager->mBuildManager->mTownCenterTile;
    if (theMap == NULL || theCenter.mX == -10000 || theCenter.mY == -10000)
        return;
    unsigned short theKindID = (unsigned short)gContinentManager.GetLargestContinent(inKind);
    EEAIContinent* theKindContinent = gContinentManager.mContinents[theKindID];
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(theCenter.mX, theCenter.mY);
    if (theContinent == NULL)
        return;
    if (gContinentManager.mTileFlags[theKindID * gContinentManager.mFlagsWidth + theContinent->mID]) {
        theKindContinent->GetShoreNextTo(theContinent->mID, theTiles);
        theRegion = theTiles;
        for (std::vector<U2DPoint<long> >::iterator it = theRegion.begin(); it != theRegion.end(); it++) {
            Fn2492C2(it->mX, it->mY, 1, theRing);
            theTiles.insert(theTiles.end(), theRing.begin(), theRing.end());
        }
        std::sort(theTiles.begin(), theTiles.end());
        theTiles.erase(std::unique(theTiles.begin(), theTiles.end()), theTiles.end());
        for (std::vector<U2DPoint<long> >::iterator theIt = theTiles.begin(); theIt != theTiles.end();) {
            EETile* theTile = theMap->GetTileAtLocation(*theIt);
            if (inKind == 2 ? theTile != NULL && (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                : theTile != NULL && theTile->mTerrainType == kTerrainSpace)
                theIt++;
            else
                theIt = theTiles.erase(theIt);
        }
        std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(theCenter));
    }
    this->mSiteTilesCollected = true;
}

// FUNCTION: EXE 0x467e10
// Tells whether armed enemy units stand near a tile (influence regions around it, one tile on small island
// or planet maps), ignoring idle searchers (timer above 60 s)
bool EECPBuildManager::HasEnemyNear(long inX, long inY)
{
    std::vector<EEComplexUnit*> theUnits;
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    EEMap* theMap = gWorld.mMap;
    U2DRectangle<long> theRect;
    long theCount = 0;
    unsigned long thePlayerID = this->mPlayer->mPlayerID;
    if (theTable != NULL) {
        unsigned long theHeight;
        if (theMap != NULL && ((theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (theMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap)
            && (theHeight = theMap->mL195610) <= 0x2a) {
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
        theCount = theTable->SumEnemyValue(thePlayerID, theTable->GetRegions(theRect), true);
        if (theCount > 0) {
            gWorld.mMap->mUnitRegions->CollectValues(theRect, theUnits, std::less<EEComplexUnit*>());
            std::vector<EEComplexUnit*>::iterator it;
            for (it = theUnits.begin(); it != theUnits.end();) {
                if ((*it)->mPlayer == this->mPlayer)
                    it = theUnits.erase(it);
                else
                    it++;
            }
            for (it = theUnits.begin(); it != theUnits.end();) {
                if ((*it)->mStats->mAttack == 0.0f || (*it)->mTargetSearchTimer > 60000)
                    it = theUnits.erase(it);
                else
                    it++;
            }
            theCount = theUnits.size();
        }
    }
    return theCount > 0;
}

// FUNCTION: EXE 0x467f97
// Finds an airport site with room for 4 (else 3) runway pieces facing away from the town centre, rings 5..99
// around it, reachable and with no enemies near
bool EECPBuildManager::FindAirportSite(EETileCoordinate& outTile)
{
    std::vector<U2DPoint<long> > theTiles;
    bool theResult = false;
    U2DPoint<long>& theTownTile = this->mManager->mBuildManager->mTownCenterTile;
    EETileCoordinate theCenter;
    theCenter.mX = theTownTile.mX;
    theCenter.mY = theTownTile.mY;
    if (theCenter.mX != -10000 && theCenter.mY != -10000) {
        long theSize = gUnitClassTable[kUnitTypeAirport]->mTileSize;
        long theWallSize = gUnitClassTable[kUnitTypeAirportRunway]->mTileSize;
        long theLength = theSize + theWallSize * 4 + 2;
        long theOffset = -2 - theSize;
        for (long k = 4; k >= 3 && !theResult; k--) {
            for (long r = 5; r < 100 && !theResult; r++) {
                Fn2492C2(theCenter.mX, theCenter.mY, r, theTiles);
                for (std::vector<U2DPoint<long> >::iterator it = theTiles.begin(); it != theTiles.end() && !theResult;
                    it++) {
                    long theX = it->mX;
                    long theY = it->mY;
                    if (gWorld.mMap->GetTileAtLocation(*it) == NULL
                        || !gContinentManager.AreConnected(gContinentManager.GetContinentAt(theCenter.GetX(), theCenter.GetY()),
                            gContinentManager.GetContinentAt(*it), this->mPlayer->GetCitizenPassability()))
                        continue;
                    switch (EEGetFacingQuarter(theX, theY)) {
                    case 0:
                        theResult = EEPlayerBlockTakes(this->mPlayer, 0x42, theX - 1, theY - 1, theSize + 2, theLength, 0);
                        break;
                    case 1:
                        theResult = EEPlayerBlockTakes(this->mPlayer, 0x42, theX - 1, theY - theOffset - theLength - 1, theSize + 2,
                            theLength, 0);
                        break;
                    case 2:
                        theResult = EEPlayerBlockTakes(this->mPlayer, 0x42, theX - 1, theY - 1, theLength, theSize + 2, 0);
                        break;
                    case 3:
                        theResult = EEPlayerBlockTakes(this->mPlayer, 0x42, theX - theOffset - theLength - 1, theY - 1, theLength,
                            theSize + 2, 0);
                        break;
                    }
                    if (!theResult)
                        continue;
                    theResult = !this->HasEnemyNear(theX, theY);
                    if (theResult) {
                        outTile.mX = it->mX;
                        outTile.mY = it->mY;
                    }
                }
            }
            theLength -= theWallSize;
        }
    }
    return theResult;
}
// FUNCTION: EXE 0x4681b3
// Finds an anti-air site around a building (FindAntiAirSiteAround its tile and size)
bool EECPBuildManager::FindAntiAirSiteNear(EEUnit* inUnit, EETileCoordinate& outTile)
{
    long theSize = inUnit->mUnitClass->mTileSize;
    EETileCoordinate theTile = inUnit->GetTile();
    return this->FindAntiAirSiteAround(theTile, theSize, outTile);
}

// FUNCTION: EXE 0x4681e3
// Finds an anti-air site in square rings growing out from a tile up to the anti-air range, on the tile's
// continent
bool EECPBuildManager::FindAntiAirSiteAround(EETileCoordinate& inTile, long inSize, EETileCoordinate& outTile)
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<EETileCoordinate> theUnused;
    unsigned short theContinent = gContinentManager.GetContinent(inTile, kAIPathLandMass);
    long theType = this->mManager->GetAntiAirClass();
    bool theResult = false;
    EEUnitClass* theClass = gUnitClassTable[theType];
    long theRange = UMFloatToLong(this->mPlayer->GetUpgrade(theType)->mRange);
    long theX = inTile.mX;
    long theSize = theClass->mTileSize;
    long theY = inTile.mY;
    long theRing = inSize;
    for (long i = 0; i < theRange && !theResult; i++) {
        EEGetSquareBlockRing(1, theX, theY, theRing, theTiles, 0);
        theRing += 2;
        theX--;
        theY--;
        for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end() && !theResult;
            it++) {
            theResult = this->CanBuildAt(theContinent, theType, *it, theSize + 1, false, true, inTile.mX, inTile.mY);
            if (theResult) {
                outTile.mX = it->mX;
                outTile.mY = it->mY;
            }
        }
    }
    return theResult;
}

// FindAntiAirSite's building clusters: a box around a building and the number of buildings in it (20-byte elements; the
// pair's (const&, const&) ctor 0x6c60f and operator= 0x6c5ce are out of line). The original's find_if (0x6c027)
// tests the box with U2DRectangle<long>::Contains (0xd190) and grows it with 0x19f9d: one same-layout box type
// (provisional names)
typedef std::pair<EEAIAStarBounds, unsigned long> EECPBMCluster;

struct EECPBMClusterHolds {
    EECPBMClusterHolds(EEUnit* inUnit) : mUnit(inUnit) {}
    bool operator()(const EECPBMCluster& inCluster) const
        { return inCluster.first.Contains(U2DPoint<long>(this->mUnit->mTileX, this->mUnit->mTileY)); }

    EEUnit* mUnit;                          // +0x00
};

struct EECPBMClusterGreater {
    bool operator()(const EECPBMCluster& inFirst, const EECPBMCluster& inSecond) const
        { return inFirst.second > inSecond.second; }
};

// FUNCTION: EXE 0x46832b
// Finds an anti-air site: by a capitol not yet covered, then by a wonder, then in the biggest clusters of
// the player's buildings
bool EECPBuildManager::FindAntiAirSite(EETileCoordinate& outTile)
{
    std::vector<EECPBMCluster> theClusters;
    EEAIAStarBounds theBounds;
    std::vector<EEUnit*> theUnits;
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    EETechTree* theTechTree = this->mPlayer->GetTechTree();
    long theType = this->mManager->GetAntiAirClass();
    bool theResult = false;
    std::vector<EEUnit*>::iterator it;
    EEUnit* theUnit;
    EEGetPlayerUnitsOfType(kUnitTypeCapitol, this->mPlayer->mPlayerID, theUnits);
    for (it = theUnits.begin(); it != theUnits.end() && !theResult; it++) {
        theResult = !this->HasAntiAirNear((*it)->GetTileX(), (*it)->GetTileY());
        if (theResult)
            theResult = this->FindAntiAirSiteNear(*it, outTile);
    }
    if (theResult)
        return theResult;
    theUnits.clear();
    for (theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EETechTreeNode* theNode = theTechTree->Get(theUnit->mUnitClass->mTechnology);
        if (theNode != NULL && theNode->mData->mKind == 4)
            theUnits.push_back(theUnit);
    }
    for (it = theUnits.begin(); it != theUnits.end() && !theResult; it++) {
        theResult = !this->HasAntiAirNear((*it)->GetTileX(), (*it)->GetTileY());
        if (theResult)
            theResult = this->FindAntiAirSiteNear(*it, outTile);
    }
    if (theResult)
        return theResult;
    long theRadius = UMFloatToLong(this->mPlayer->mUnitClassUpgrades[theType]->mRange * 0.5f);
    for (theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (!theUnit->mUnitClass->IsLandBuilding() || theUnit->mUnitClass->mUnitType == kUnitTypeAA10Gun88mm
            || theUnit->mUnitClass->mUnitType == kUnitTypeAA13MissileTower || theUnit->mUnitClass->mIsWall)
            continue;
        std::vector<EECPBMCluster>::iterator theFound = std::find_if(theClusters.begin(),
            theClusters.end(), EECPBMClusterHolds(theUnit));
        if (theFound != theClusters.end())
            theFound->second++;
        else {
            theBounds.mLeft = theUnit->GetTileX();
            theBounds.mTop = theUnit->GetTileY();
            theBounds.mRight = theUnit->GetTileX();
            theBounds.mBottom = theUnit->GetTileY();
            theBounds.Grow(theRadius, true);
            theClusters.push_back(EECPBMCluster(theBounds, 1));
        }
    }
    std::sort(theClusters.begin(), theClusters.end(), EECPBMClusterGreater());
    for (std::vector<EECPBMCluster>::iterator theIt = theClusters.begin();
        theIt != theClusters.end() && !theResult; theIt++) {
        theBounds = theIt->first;
        theTile.mX = theBounds.mLeft + theRadius;
        theTile.mY = theBounds.mTop + theRadius;
        theResult = !this->HasAntiAirNear(theTile.mX, theTile.mY);
        if (theResult)
            theResult = this->FindAntiAirSiteAround(theTile, 1, outTile);
    }
    return theResult;
}

// FindCenterSite's sort of the resource centres (8 bytes by value to std::sort 0x6c120; its operator() sits among the sort
// COMDATs) (provisional name)
struct EECPBMCenterLess {
    EECPBMCenterLess(long inResource, long inPlayerID) : mResource(inResource), mPlayerID(inPlayerID) {}
    // inline (COMDAT 0x6d510 among the sort COMDATs): the centre whose units stand in fewer area effects of kind
    // mResource for mPlayerID comes first
    bool operator()(EECPResourceCenter* inFirst, EECPResourceCenter* inSecond) const
    {
        std::vector<EEUnit*> theUnits;
        unsigned long theFirst = 0;
        unsigned long theSecond = 0;
        unsigned long theCount;
        unsigned long i;
        theUnits = inFirst->mUnits;
        theCount = theUnits.size();
        for (i = 0; i < theCount; i++) {
            if (gAEManager.HasAreaEffect(this->mResource, theUnits[i]->GetTileX(), (*(theUnits.begin() + i))->GetTileY(),
                this->mPlayerID))
                theFirst++;
        }
        theUnits.clear();
        theUnits = inSecond->mUnits;
        theCount = theUnits.size();
        for (i = 0; i < theCount; i++) {
            if (gAEManager.HasAreaEffect(this->mResource, theUnits[i]->GetTileX(), theUnits[i]->GetTileY(), this->mPlayerID))
                theSecond++;
        }
        return theFirst < theSecond;
    }

    long mResource;                         // +0x00 (3, 4, 1 by the class)
    long mPlayerID;                         // +0x04
};

// FUNCTION: EXE 0x46865a
// Finds a site at a resource centre: a settlement at an unclaimed one, else a temple, university or hospital
// at a claimed one, preferring centres whose units stand in fewer matching area effects
bool EECPBuildManager::FindCenterSite(long inUnitClass, EETileCoordinate& outTile)
{
    std::vector<EECPResourceCenter*> theCenters;
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    long theResource = 0;
    bool theResult = false;
    unsigned long i;
    EECPResourceCenter* theCenter;
    if (inUnitClass == 0x2b) {
        for (i = 0; i < this->mPlayer->GetNumResourceCenters(); i++) {
            theCenter = this->mPlayer->GetResourceCenter(i);
            if (theCenter != NULL && theCenter->mUnit == NULL)
                theCenters.push_back(theCenter);
        }
        i = 0;
        unsigned long theCount = theCenters.size();
        for (; i < theCount && !theResult; i++) {
            theCenter = theCenters[i];
            theResult = theCenter->FindBuildSite(kUnitTypeSettlement, theTile);
            if (theResult) {
                outTile.mX = theTile.mX;
                outTile.mY = theTile.mY;
            }
        }
    } else {
        switch (inUnitClass) {
        case 0x27:
            theResource = 3;
            break;
        case 0x3c:
            theResource = 4;
            break;
        case 0x3e:
            theResource = 1;
            break;
        }
        for (i = 0; i < this->mPlayer->GetNumResourceCenters(); i++) {
            theCenter = this->mPlayer->GetResourceCenter(i);
            if (theCenter != NULL && theCenter->mUnit != NULL)
                theCenters.push_back(theCenter);
        }
        unsigned long theCount = theCenters.size();
        if (theCount != 0) {
            std::sort(theCenters.begin(), theCenters.end(), EECPBMCenterLess(theResource, this->mPlayer->mPlayerID));
            for (unsigned long j = 0; j < theCount && !theResult; j++) {
                theCenter = theCenters[j];
                theResult = theCenter->FindBuildSite(inUnitClass, theTile);
                if (theResult) {
                    outTile.mX = theTile.mX;
                    outTile.mY = theTile.mY;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x468862
// Finds a granary site just beyond a resource centre's drop-off distance with room for farms around it
bool EECPBuildManager::FindGranarySite(EETileCoordinate& outTile)
{
    std::vector<EETileCoordinate> theTiles;
    bool theResult = false;
    EETileCoordinate theCenter;
    theCenter.mX = 0;
    theCenter.mY = 0;
    EETileCoordinate theSite;
    theSite.mX = 0;
    theSite.mY = 0;
    long theRadius = this->mPlayer->mDropOffDistance;
    long theSize = gUnitClassTable[kUnitTypeGranary]->mTileSize;
    for (unsigned long i = 0; i < this->mPlayer->GetNumResourceCenters() && !theResult; i++) {
        EECPResourceCenter* theResourceCenter = this->mPlayer->GetResourceCenter(i);
        theResourceCenter->GetCenter(theCenter);
        unsigned short theContinent = gContinentManager.GetContinent(theCenter, kAIPathLandMass);
        for (long r = theRadius + 1; r < theRadius + 0x1e && !theResult; r++) {
            Fn2492C2(theCenter.mX, theCenter.mY, r, theTiles);
            for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end() && !theResult;
                it++) {
                theResult = this->CanBuildAt(theContinent, kUnitTypeFarm, *it, theSize * 3, false, true, theCenter.mX, theCenter.mY);
                if (!theResult)
                    continue;
                theSite.mX = it->mX + theSize;
                theSite.mY = it->mY + theSize;
                theResult = this->CanBuildAt(theContinent, kUnitTypeGranary, theSite, theSize, false, true, theCenter.mX,
                    theCenter.mY);
                if (theResult) {
                    outTile.mX = theSite.mX;
                    outTile.mY = theSite.mY;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4689d0
// Finds a site for a production building at the active resource centres, round robin from the last used one
// Common found result and loop exit preserve the original 117-byte body.
bool EECPBuildManager::FindProductionSite(long inUnitClass, EETileCoordinate& outTile)
{
    unsigned long theIndex = this->mNextResourceCenter;
    unsigned long theCount = this->mPlayer->GetNumResourceCenters();
    bool theFound = false;
    for (unsigned long i = 0; i < theCount; i++) {
        EECPResourceCenter* theCenter = this->mPlayer->GetResourceCenter(theIndex);
        if (theCenter != NULL && theCenter->mIsActive && theCenter->FindBuildSite(inUnitClass, outTile))
            theFound = true;
        else
            theFound = false;
        theIndex++;
        if (theIndex == theCount)
            theIndex = 0;
        if (theFound) {
            this->mNextResourceCenter = theIndex;
            break;
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x468a45
// Finds a dock or navy yard site among the water site tiles away from existing ones; sites without a
// builder are skipped
bool EECPBuildManager::FindShoreSite(long inUnitClass, long inSize, EETileCoordinate& outTile)
{
    std::vector<EEUnit*> the24;
    std::vector<EEUnit*> the41;
    std::vector<EEUnit*> theUnits;
    unsigned short theContinent = gContinentManager.GetContinent(this->mTownCenterTile, kAIPathLandMass);
    bool theResult = false;
    std::vector<U2DPoint<long> >& theSites = this->mWaterSites;
    EECPUnitManager* theUnitManager = this->mManager->mUnitManager;
    unsigned long thePlayerID = this->mPlayer->mPlayerID;
    if (theSites.size() > 0) {
        EEGetPlayerUnitsOfType(kUnitTypeDock, thePlayerID, the24);
        EEGetPlayerUnitsOfType(kUnitTypeNavyYard, thePlayerID, the41);
        theUnits.insert(theUnits.end(), the24.begin(), the24.end());
        theUnits.insert(theUnits.end(), the41.begin(), the41.end());
        for (unsigned long i = 0, theCount = theSites.size(); i < theCount && !theResult; i++) {
            if (std::find_if(theUnits.begin(), theUnits.end(), EECPUnitWithinDistance(theSites[i], 75.0f))
                != theUnits.end())
                continue;
            theResult = this->CanBuildAt(theContinent, inUnitClass, theSites[i], inSize, false, true, this->mTownCenterTile.GetX(),
                this->mTownCenterTile.GetY());
            if (theResult && theUnitManager->FindBuilderForTile(theSites[i], inUnitClass) != NULL) {
                outTile.mX = theSites[i].GetX();
                outTile.mY = theSites[i].GetY();
            } else
                theResult = false;   // no builder: keep looking
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x468c90
// Finds a space dock, turret or station site among space tiles away from units of its class; sites
// without a builder are skipped
// Class 0x302 searches the shore tiles of the town continents (nearest first) on planet maps, else the town continent's
// shore tiles near own units; others use mSpaceSites. 0x2d8 keeps 75 from its kind and uses EEIsSpaceDockSiteOpen
// (its tile moved by -1, -1)
bool EECPBuildManager::FindSpaceSite(long inUnitClass, long inSize, EETileCoordinate& outTile)
{
    bool theResult = false;
    std::vector<U2DPoint<long> >& theWater = this->mSpaceSites;
    if (theWater.size() == 0)
        return false;
    std::vector<U2DPoint<long> > theSites;
    if (inUnitClass == 0x302) {
        std::vector<U2DPoint<long> > theTiles;
        U2DPoint<long> theCenter;
        if ((gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) {
            std::vector<std::pair<EEAIContinent*, EETown*> > theContinents;
            for (unsigned long i = 0; i < this->mPlayer->mTowns.size(); i++) {
                EETown* theTown = this->mPlayer->mTowns[i];
                if (theTown != NULL && theTown->mContinent != 0) {
                    EEAIContinent* theContinent = gContinentManager.mContinents[theTown->mContinent];
                    theContinents.push_back(std::pair<EEAIContinent*, EETown*>(theContinent, theTown));
                }
            }
            std::sort(theContinents.begin(), theContinents.end(),
                EETileDistanceLess(this->mTownCenterTile.mX, this->mTownCenterTile.GetY()));
            for (std::vector<std::pair<EEAIContinent*, EETown*> >::iterator it = theContinents.begin();
                it != theContinents.end(); it++) {
                theTiles = it->first->mShoreTiles;
                theCenter = it->second->mHull.GetCenter();
                std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(theCenter.mX, theCenter.GetY()));
                theSites.insert(theSites.end(), theTiles.begin(), theTiles.end());
            }
        } else {
            theTiles = gContinentManager.GetContinentAt(this->mTownCenterTile.GetX(), this->mTownCenterTile.GetY())->mShoreTiles;
            std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(this->mTownCenterTile.mX, this->mTownCenterTile.GetY()));
            std::vector<U2DPoint<long> > theOwn;
            for (EEUnit* theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                if (theUnit->GetUnitClass()->mUnitType != kUnitTypeSpaceTurret)
                    theOwn.push_back(U2DPoint<long>(theUnit->GetTileX(), theUnit->GetTileY()));
            }
            for (unsigned long i = 0; i < theTiles.size(); i++) {
                theCenter = theTiles[i];
                std::sort(theOwn.begin(), theOwn.end(), EETileDistanceLess(theCenter.mX, theCenter.GetY()));
                if ((unsigned long)theCenter.DistanceToSquared(theOwn[0]) < 25)
                    theSites.push_back(theCenter);
            }
        }
    } else
        theSites = theWater;
    if (theSites.size() > 0) {
        std::vector<EEUnit*> theUnits;
        unsigned short theContinent = gContinentManager.GetContinent(this->mTownCenterTile, kAIPathLandMass);
        EECPUnitManager* theUnitManager = this->mManager->mUnitManager;
        unsigned long thePlayerID = this->mPlayer->mPlayerID;
        float theDistance;
        if (inUnitClass == 0x2d8)
            theDistance = 75.0f;
        else
            theDistance = (this->mPlayer->mUnitClassUpgrades[inUnitClass])->mRange
                * (this->mPlayer->mUnitClassUpgrades[inUnitClass])->mRange;
        EEGetPlayerUnitsOfType(inUnitClass, thePlayerID, theUnits);
        for (unsigned long i = 0, theCount = theSites.size(); i < theCount && !theResult; i++) {
            if (std::find_if(theUnits.begin(), theUnits.end(), EECPUnitWithinDistance(theSites[i], theDistance))
                != theUnits.end())
                continue;
            if (inUnitClass != 0x2d8)
                theResult = this->CanBuildAt(theContinent, inUnitClass, theSites[i], inSize, false, true,
                    this->mTownCenterTile.mX, this->mTownCenterTile.GetY());
            else
                theResult = EEIsSpaceDockSiteOpen(theSites[i].mX, theSites[i].GetY(), this->mPlayer);
            if (theResult && theUnitManager->FindBuilderForTile(theSites[i], inUnitClass) == NULL)
                theResult = false;   // no builder: keep looking
            if (!theResult)
                continue;
            outTile.mX = theSites[i].mX;
            outTile.mY = theSites[i].GetY();
            if (inUnitClass == 0x2d8) {
                outTile.mX--;
                outTile.mY--;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x469245
// Finds a tower site: along town continent shores on island maps, then at a resource centre, then near an
// open gate, then at a choke point near a town not yet guarded by that class
bool EECPBuildManager::FindTowerSite(long inUnitClass, long inSize, EETileCoordinate& outTile)
{
    std::vector<EEUnit*> theGates;
    std::vector<EEAIChokePoint*> theChokePoints;
    std::vector<EEComplexUnit*> theUnits;
    std::vector<std::pair<EEAIContinent*, EETown*> > theContinents;
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theRing;
    std::vector<EETileCoordinate> theRadiusTiles;
    U2DPoint<long> theCenter;
    EETileCoordinate theSite;
    EETileArea theArea;
    EEMap* theMap = gWorld.mMap;
    EEUnitClass* theClass = gUnitClassTable[inUnitClass];
    long theGateType = this->mManager->GetGateClass();
    unsigned long thePlayerID = this->mPlayer->mPlayerID;
    bool theResult = false;
    unsigned long i;
    unsigned long j;
    unsigned long theCount;
    if (this->mTownCenterTile.mX != -10000 && this->mTownCenterTile.mY != -10000) {
        float theRange = (this->mPlayer->mUnitClassUpgrades[inUnitClass]->mRange * 1.5f) * (this->mPlayer->mUnitClassUpgrades[inUnitClass]->mRange * 1.5f);
        if (theMap != NULL && (theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap) {
            for (i = 0; i < this->mPlayer->mTowns.size(); i++) {
                EETown* theTown = this->mPlayer->mTowns[i];
                if (theTown != NULL && theTown->mContinent != 0) {
                    EEAIContinent* theContinent = gContinentManager.mContinents[theTown->mContinent];
                    theContinents.push_back(std::pair<EEAIContinent*, EETown*>(theContinent, theTown));
                }
            }
            std::sort(theContinents.begin(), theContinents.end(),
                EETileDistanceLess(this->mTownCenterTile.mX, this->mTownCenterTile.mY));
            for (std::vector<std::pair<EEAIContinent*, EETown*> >::iterator it = theContinents.begin();
                it != theContinents.end() && !theResult; it++) {
                theTiles = it->first->mShoreTiles;
                theCenter = it->second->mHull.GetCenter();
                std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(theCenter.mX, theCenter.mY));
                for (i = 0, theCount = theTiles.size(); i < theCount && !theResult; i++) {
                    theCenter = theTiles[i];
                    bool theNear;
                    if (inUnitClass == 0x2f0)
                        theNear = this->HasUnitNear(0x2f0, theCenter.mX, theCenter.mY, theRange);
                    else
                        theNear = this->HasTowerNear(theCenter.mX, theCenter.mY, theRange);
                    if (theNear)
                        continue;
                    for (unsigned long r = 1; r <= 2 && !theResult; r++) {
                        Fn2492C2(theCenter.mX, theCenter.mY, r, theRing);
                        for (unsigned long k = 0, n = theRing.size(); k < n && !theResult; k++) {
                            EETile* theTile = theMap->GetTile((*(theRing.begin() + k)).mX,
                                theRing[k].mY);
                            if (theClass != NULL && theTile != NULL && theClass->mTerrainTypes[theTile->mTerrainType]
                                && EEPlayerBlockTakes(this->mPlayer, inUnitClass, (*(theRing.begin() + k)).mX,
                                    (*(theRing.begin() + k)).mY, inSize, inSize, 0)) {
                                theResult = true;
                                outTile.mX = theRing[k].mX;
                                outTile.mY = (*(theRing.begin() + k)).mY;
                            } else
                                theResult = false;
                        }
                    }
                }
            }
        }
        if (!theResult) {
            for (i = 0; i < this->mPlayer->mResourceCenters.size() && !theResult; i++) {
                EECPResourceCenter* theResourceCenter = this->mPlayer->GetResourceCenter(i);
                if (theResourceCenter == NULL)
                    continue;
                theResult = theResourceCenter->FindBuildSite(inUnitClass, theSite);
                if (theResult) {
                    outTile.mX = theSite.mX;
                    outTile.mY = theSite.mY;
                }
            }
        }
        if (!theResult) {
            EEUCGate* theGateClass = dynamic_cast<EEUCGate*>(gUnitClassTable[theGateType]);
            EEGetPlayerUnitsOfType(theGateType, thePlayerID, theGates);
            std::vector<EEUnit*>::iterator theGate;
            for (theGate = theGates.begin(); theGate != theGates.end();) {
                EEComplexUnit* theComplex = (*theGate)->GetComplexUnit();
                if (theComplex->mPreviousPart == NULL && theComplex->mNextPart != NULL)
                    theGate++;
                else
                    theGate = theGates.erase(theGate);
            }
            std::sort(theGates.begin(), theGates.end(), EECPUnitTileDistanceLess(this->mTownCenterTile));
            for (theGate = theGates.begin(); theGate != theGates.end() && !theResult; theGate++) {
                bool theNear;
                if (inUnitClass == 0x2f0)
                    theNear = this->HasUnitNear(0x2f0, (*theGate)->GetTileX(), (*theGate)->GetTileY(), theRange);
                else
                    theNear = this->HasTowerNear((*theGate)->GetTileX(), (*theGate)->GetTileY(), theRange);
                if (!theNear) {
                    EEGetTilesInRadius((*theGate)->GetTileX(), (*theGate)->GetTileY(), 5, true, theRadiusTiles);
                    std::vector<EETileCoordinate>::iterator theIt;
                    for (theIt = theRadiusTiles.begin(); theIt != theRadiusTiles.end();) {
                        if (this->HasGateNear(theIt->mX, theIt->mY, 2.0f) || this->HasWallNear(theIt->mX, theIt->mY, 4.0f)   // 2 tiles squared, like the gate test (was 2: 1.4 tiles)
                            || (inUnitClass == 0x2f0 ? this->HasUnitNear(0x2f0, theIt->mX, theIt->mY, theRange)
                                : this->HasTowerNear(theIt->mX, theIt->mY, theRange)))
                            theIt = theRadiusTiles.erase(theIt);
                        else
                            theIt++;
                    }
                    EEAIRandomShuffle(theRadiusTiles.begin(), theRadiusTiles.end());
                    for (theIt = theRadiusTiles.begin(); theIt != theRadiusTiles.end() && !theResult; theIt++) {
                        theResult = EEPlayerBlockTakes(this->mPlayer, inUnitClass, theIt->mX, theIt->mY, inSize, inSize, 0);
                        if (theResult) {
                            outTile.mX = theIt->mX;
                            outTile.mY = theIt->mY;
                        }
                    }
                }
            }
        }
        if (!theResult) {
            gContinentManager.GetChokePoints(theChokePoints,
                gContinentManager.GetContinentAt(this->mTownCenterTile.mX, this->mTownCenterTile.mY)->mID, true);
            if (theChokePoints.size() > 0) {
                std::sort(theChokePoints.begin(), theChokePoints.end(),
                    EETileDistanceLess(this->mTownCenterTile.mX, this->mTownCenterTile.mY));
                for (i = 0, theCount = theChokePoints.size(); i < theCount && !theResult; i++) {
                    bool theNearTown = false;
                    for (j = 0; j < this->mPlayer->GetNumTowns() && !theNearTown; j++) {
                        EETown* theTown = this->mPlayer->mTowns[j];
                        theNearTown = theTown->mHull.GetCenter().DistanceToSquared(
                            (*(theChokePoints.begin() + i))->mHull.GetCenter()) < 50;
                    }
                    if (theNearTown) {
                        theArea.mTop = theChokePoints[i]->mHull.GetBounds().mTop;
                        theArea.mLeft = (*(theChokePoints.begin() + i))->mHull.GetBounds().mLeft;
                        theArea.mBottom = (*(theChokePoints.begin() + i))->mHull.GetBounds().mBottom;
                        theArea.mRight = (*(theChokePoints.begin() + i))->mHull.GetBounds().mRight;
                        theArea.Grow(UMFloatToLong((*(this->mPlayer->mUnitClassUpgrades.begin() + inUnitClass))->mRange * 2.0f), true);
                        EEAnyUnit theFilter;
                        EEGetUnitsInRectangle(theArea, theUnits, theFilter, true);
                        bool theTaken = false;
                        for (unsigned long k = 0, n = theUnits.size(); k < n && !theTaken; k++) {
                            EEComplexUnit* theUnit = theUnits[k];
                            theTaken = theUnit->GetHitPoints() > 0 && theUnit->mUnitClass->mUnitType == inUnitClass;
                        }
                        if (!theTaken) {
                            theChokePoints[i]->mTiles.BuildPointList(theRing, 1);
                            std::sort(theRing.begin(), theRing.end(),
                                EETileDistanceLess(this->mTownCenterTile.mX, this->mTownCenterTile.mY));
                            unsigned long m;
                            for (m = 0, n = theRing.size(); m < n && !theResult; m++)
                                theResult = EEPlayerBlockTakes(this->mPlayer, inUnitClass, theRing[m].mX, theRing[m].mY, inSize,
                                    inSize, 0);
                            if (theResult) {
                                outTile.mX = theRing[m - 1].mX;
                                outTile.mY = theRing[m - 1].mY;
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x469d6c
// Finds a wonder site with a margin of inSize in rings around the town centre (only once a wall gate exists)
// NEAR (38): the outTile store through EETileCoordinate(theTiles[i].GetX() + h, h + theTiles[i].GetY()) scores 62.5
// but the equivalence check calls it DIFF (the mX read through the out-of-line operator+ return)
bool EECPBuildManager::FindWonderSite(long inUnitClass, unsigned long inSize, EETileCoordinate& outTile)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitClass];
    std::vector<EETileCoordinate> theTiles;
    bool theResult = false;
    EETileCoordinate theCenter(0, 0);
    long theClassSize = theClass->mTileSize;
    theCenter = this->mManager->mBuildManager->mTownCenterTile;
    if (theCenter.mX != -10000 && theCenter.mY != -10000) {
        unsigned short theContinent = gContinentManager.GetContinent(theCenter, kAIPathLandMass);
        EECPWallManager* theWalls = this->mManager->mWallManager;
        if (theWalls->mGateTile.mX != 0 && theWalls->mGateTile.mY != 0) {
            for (unsigned long r = 1; r < 200; r++) {
                Fn2492C2(theCenter.mX, theCenter.mY, r, theTiles);
                unsigned long theCount = theTiles.size();
                if (theCount == 0)
                    break;
                for (unsigned long i = 0; i < theCount && !theResult; i++) {
                    theResult = this->CanBuildAt(theContinent, inUnitClass, theTiles[i], theClassSize + inSize, false, true,
                        theCenter.mX, theCenter.mY);
                    if (theResult) {
                        outTile.mX = theTiles[i].mX + (inSize >> 1);
                        outTile.mY = (inSize >> 1) + theTiles[i].mY;
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x469ee0
// Finds a spot for another building of a unit's class in rings 3..1 around a random town centre that has
// none of that class within squared distance 16
bool EECPBuildManager::FindSpotNearTownCenter(EEUnit* inUnit, EETileCoordinate& outTile)
{
    bool theResult = false;
    U2DPoint<long> theTile;
    std::vector<EEUnit*> theCenters;
    EEGetPlayerUnitsOfType(kUnitTypeSettlement, this->mPlayer->mPlayerID, theCenters);
    EEGetPlayerUnitsOfType(kUnitTypeTownCenter, this->mPlayer->mPlayerID, theCenters);
    EEGetPlayerUnitsOfType(kUnitTypeCapitol, this->mPlayer->mPlayerID, theCenters);
    EEAIRandomShuffle(theCenters.begin(), theCenters.end());
    std::vector<EEUnit*> theSame;
    EEGetPlayerUnitsOfType(inUnit->GetUnitClass()->mUnitType, this->mPlayer->mPlayerID, theSame);
    theSame.erase(std::find(theSame.begin(), theSame.end(), inUnit));
    for (unsigned long i = 0; i < theCenters.size() && !theResult; i++) {
        theTile = U2DPoint<long>((theCenters[i])->GetTileX(), (theCenters[i])->GetTileY());
        if (std::find_if(theSame.begin(), theSame.end(), EECPUnitWithinDistance(theTile, 16.0f)) != theSame.end())
            continue;
        std::vector<U2DPoint<long> > theRing;
        EEMap* theMap = gWorld.mMap;
        for (unsigned short r = 3; r > 0 && !theResult; r--) {
            Fn2492C2(theTile.GetX(), theTile.mY, r, theRing);
            for (unsigned short k = 0; k < theRing.size() && !theResult; k++) {
                EETile* theMapTile = theMap->GetTile((theRing[k]).GetX(), (theRing[k]).mY);
                if (theMapTile != NULL && inUnit->GetUnitClass()->mTerrainTypes[theMapTile->mTerrainType]) {
                    long theSize = inUnit->GetUnitClass()->mTileSize;
                    if (EEPlayerBlockTakes(this->mPlayer, inUnit->GetUnitClass()->mUnitType, (theRing[k]).GetX(),
                        (theRing[k]).mY, theSize, theSize, 0)) {
                        outTile.mX = theRing[k].GetX();
                        outTile.mY = theRing[k].mY;
                        theResult = true;
                    } else
                        theResult = false;
                } else
                    theResult = false;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x46a228
// Finds a building site for a class by its kind (wonder, granary, dock, centre, airport, tower, production,
// anti-air, space) or, for farms, an unbuilt farm foundation; returns whether one was found
// NEAR (55): the switch layout, which depends on the tile parameter: the shared declaration takes a long*
// (EECPCivilizationManager.cpp), this definition a tile reference
bool EECPBuildManager::FindSite(long inUnitClass, EETileCoordinate& theTile)
{
    long theID = gUnitClassTable[inUnitClass] != NULL ? gUnitClassTable[inUnitClass]->GetTechnology() : 100000;
    EETechTreeNode* theNode = this->mPlayer->GetTechTree()->Get(theID);
    EEUnitClass* theClass = gUnitClassTable[inUnitClass];
    bool theResult = false;
    if (theClass == NULL)
        return theResult;
    if (theNode != NULL && theNode->mData->mKind == 4) {
        for (long theSize = 10; theSize != 0 && !theResult; theSize -= 2)
            theResult = this->FindWonderSite(inUnitClass, theSize, theTile);
        return theResult;
    }
    switch (inUnitClass) {
    case kUnitTypeGranary:
        return this->FindGranarySite(theTile);
    case kUnitTypeDock:
    case kUnitTypeNavyYard:
        return this->FindShoreSite(inUnitClass, theClass->GetTileSize(), theTile);
    case kUnitTypeTemple:
    case kUnitTypeSettlement:
    case kUnitTypeUniversity:
    case kUnitTypeHospital:
        return this->FindCenterSite(inUnitClass, theTile);
    case kUnitTypeAirport:
        return this->FindAirportSite(theTile);
    case kUnitTypeGuardTowerPaleo:
    case kUnitTypeTankObstacle:
    case kUnitTypeGuardTowerCopper:
    case kUnitTypeGuardTowerBronze:
    case kUnitTypeGuardTowerMiddle:
    case kUnitTypeGuardTowerImperial:
    case kUnitTypeGuardTowerWW1:
    case kUnitTypeGuardTowerDigital:
    case kUnitTypePriestTower:
    case kUnitTypeGuardTowerSpace:
        return this->FindTowerSite(inUnitClass, theClass->GetTileSize(), theTile);
    case kUnitTypeBarracks:
    case kUnitTypeArcheryRange:
    case kUnitTypeSiegeFactory:
    case kUnitTypeStable:
    case kUnitTypeMissileBase:
    case kUnitTypeTankFactory:
    case kUnitTypeCyberFactory:
    case kUnitTypeCyberLaboratory:
    case kUnitTypeCpMissileBase:
        return this->FindProductionSite(inUnitClass, theTile);
    case kUnitTypeAA10Gun88mm:
    case kUnitTypeAA13MissileTower:
    case kUnitTypeAA15MissileTower:
        return this->FindAntiAirSite(theTile);
    case kUnitTypeSpaceDock:
    case kUnitTypeSpaceTurret:
    case kUnitTypeOrbitalSpaceStation:
        return this->FindSpaceSite(inUnitClass, theClass->GetTileSize(), theTile);
    case kUnitTypeFarm:
    case kUnitTypeRoboticFarm:
        {
            for (EEUnit* theUnit = this->mPlayer->mFirstUnit; theUnit != NULL && !theResult;
                theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                theResult = false;
                if (theUnit->mUnitClass->GetUnitType() != inUnitClass || theUnit->mComplexUnit->mIsBuilt
                    || theUnit->mComplexUnit->mHitPoints != 1)
                    continue;
                theTile.mX = theUnit->mComplexUnit->mTileX;
                theTile.mY = theUnit->mComplexUnit->mTileY;
                theResult = true;
            }
        }
        return theResult;
    }
    return theResult;
}



// FUNCTION: EXE 0x46a473
// Finds a spot for another building of a unit's class in ring 4 around a random dock (space dock for space
// buildings) that has none of that class within squared distance 25
bool EECPBuildManager::FindSpotNearDock(EEUnit* inUnit, EETileCoordinate& outTile)
{
    U2DPoint<long> theTile;
    bool theResult = false;
    std::vector<EEUnit*> theSame;
    EEGetPlayerUnitsOfType(inUnit->mUnitClass->mUnitType, this->mPlayer->mPlayerID, theSame);
    theSame.erase(std::find(theSame.begin(), theSame.end(), inUnit));
    long theCategory = inUnit->mUnitClass->mCategory;
    std::vector<EEUnit*> theBuildings;
    EEGetPlayerUnitsOfType(theCategory == kUnitCategorySpaceBuilding ? 0x2d8 : 0x24, this->mPlayer->mPlayerID, theBuildings);
    EEAIRandomShuffle(theBuildings.begin(), theBuildings.end());
    for (std::vector<EEUnit*>::iterator it = theBuildings.begin(); it != theBuildings.end() && !theResult; it++) {
        theTile = U2DPoint<long>((*it)->GetTileX(), (*it)->GetTileY());
        if (std::find_if(theSame.begin(), theSame.end(), EECPUnitWithinDistance(theTile, 25.0f)) != theSame.end())
            continue;
        std::vector<U2DPoint<long> > theRing;
        EEMap* theMap = gWorld.mMap;
        Fn2492C2(theTile.GetX(), theTile.mY, 4, theRing);
        for (unsigned short k = 0; k < theRing.size() && !theResult; k++) {
            EETile* theMapTile = theMap->GetTile((theRing[k]).GetX(), (theRing[k]).mY);
            if (theMapTile != NULL && inUnit->mUnitClass->mTerrainTypes[theMapTile->mTerrainType]) {
                long theSize = inUnit->mUnitClass->mTileSize;
                if (EEPlayerBlockTakes(this->mPlayer, inUnit->mUnitClass->mUnitType, (theRing[k]).GetX(),
                    (theRing[k]).mY, theSize, theSize, 0)) {
                    outTile.mX = theRing[k].GetX();
                    outTile.mY = theRing[k].mY;
                    theResult = true;
                } else
                    theResult = false;
            } else
                theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x46a73b
// Reads the build queues, town centre tile and next update time from a saved game (older saves lack
// the last queue or the time); returns the first read error
long EECPBuildManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = 0;
    U2DPoint<long> theTile;
    for (unsigned long i = 0; i < 15; i++) {
        if (theResult != 0)
            break;
        this->mQueues[i].clear();
        if (inMajorVersion == 0 && inMinorVersion < 0xea && i == 14)
            continue;
        unsigned long theCount;
        theResult = inFile.Read(theCount);
        if (theResult != 0)
            continue;
        for (unsigned long j = 0; j < theCount && theResult == 0; j++) {
            unsigned long theClass;
            theResult = inFile.Read(theClass);
            if (theResult != 0)
                continue;
            theResult = theTile.Load(inFile);
            if (theResult == 0)
                this->mQueues[i].push_back(EECPBuildRequest(theClass, theTile));
        }
    }
    if (theResult == 0)
        theResult = this->mTownCenterTile.Load(inFile);
    if ((inMajorVersion != 0 || inMinorVersion >= 0xdc) && theResult == 0)
        theResult = inFile.Read(this->mNextUpdateTime);
    return theResult;
}

// FUNCTION: EXE 0x46a833
// Tells whether a square of inSize at a tile lies within the distance of the far end of an airport runway
bool EECPBuildManager::IsNearAirportEnd(long inX, long inY, long inSize, float inDistance)
{
    std::vector<EEUnit*> theUnits;
    U2DPoint<long> theCenter(inX + inSize / 2, inSize / 2 + inY);
    U2DPoint<long> theGate;
    long theGateSize = gUnitClassTable[kUnitTypeAirportRunway]->mTileSize;
    long theHalf = UMFloatToLong(theGateSize * 0.5f);
    bool theResult = false;
    float theRange = ((float)theHalf + inDistance) * ((float)theHalf + inDistance);
    EEGetPlayerUnitsOfType(kUnitTypeAirport, this->mPlayer->mPlayerID, theUnits);
    unsigned long i = 0;
    unsigned long theCount = theUnits.size();
    for (; i < theCount && !theResult; i++) {
        EEComplexUnit* theWall = theUnits[i]->mComplexUnit;
        EEComplexUnit* theLast = theWall->mNextPart;
        while (theLast->mNextPart != NULL)
            theLast = theLast->mNextPart;
        EETileCoordinate theEnd = theLast->GetTile();
        long theX = theEnd.mX;
        long theY = theEnd.mY;
        switch (theWall->mFacing) {
        case 0:
            theGate.mX = theX + theHalf;
            theGate.mY = theY + theGateSize;
            break;
        case 1:
            theGate.mX = theX + theHalf;
            theGate.mY = theY;
            break;
        case 2:
            theX += theGateSize;
        case 3:
            theGate.mX = theX;
            theGate.mY = theY + theHalf;
            break;
        }
        if ((float)theCenter.DistanceToSquared(theGate) > theRange)
            theResult = false;
        else
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x46a989
// Tells whether any queue of a later epoch than the current one holds requests
bool EECPBuildManager::HasLaterEpochRequests()
{
    long theEpoch = this->mPlayer->GetTechTree()->mCurrentEpoch;
    bool theFound = false;
    for (long i = theEpoch + 1; i < 15 && !theFound; i++) {
        if (this->mQueues[i].size() > 0)
            theFound = true;
    }
    return theFound;
}

// FUNCTION: EXE 0x46a9c1
// Queues a building class in its epoch's queue (appended, or once only and not while one is unfinished,
// at the front or back) and reserves its cost
// NEAR (85.5): bools (byte tests) and make_pair (the three pair ctors 0x46c5f3 out of line one level down, the
// temporary straight into push_back: first is the element's unsigned long); open: inUnitClass kept in memory, cmp al, bl
void EECPBuildManager::QueueBuilding(unsigned long inUnitClass, bool inAppend, bool inFront)
{
    U2DPoint<long> theTile;
    EECPBuildQueue::iterator it;
    EECPBuildRequest theRequest;
    EETechTree* theTechTree = this->mPlayer->GetTechTree();
    long theID = gUnitClassTable[inUnitClass] != NULL ? gUnitClassTable[inUnitClass]->mTechnology : 100000;
    long theIndex;
    if (theID >= kTechEpochPrehistoric && theID < kTechEpochAll)
        theIndex = theID;
    else
        theIndex = theID / 1000 - 1;
    if (inAppend)
        this->mQueues[theIndex].push_back(std::make_pair(inUnitClass, theTile));
    else {
        EECPBuildQueue& theQueue = this->mQueues[theIndex];
        for (it = theQueue.begin(); it != theQueue.end(); ++it) {
            theRequest = *it;
            if (inUnitClass == theRequest.first)
                break;
        }
        if (it == theQueue.end()) {
            if (inUnitClass != 0x2a && inUnitClass != 0x33a && this->mManager->mUnitManager->HasUnfinished(inUnitClass))
                return;
            if (inFront)
                theQueue.push_front(std::make_pair(inUnitClass, theTile));
            else
                theQueue.push_back(std::make_pair(inUnitClass, theTile));
        } else
            return;
    }
    this->mManager->GetResourceManager()->AddCosts(theTechTree->Get(theID), 1);
}

// FUNCTION: EXE 0x46ab26
// Queues a building class at a given tile in its epoch's queue and reserves its cost
void EECPBuildManager::QueueBuildingAt(long inUnitClass, long inX, long inY)
{
    EETechTree* theTechTree = this->mPlayer->GetTechTree();
    long theID = gUnitClassTable[inUnitClass] != NULL ? gUnitClassTable[inUnitClass]->mTechnology : 100000;
    long theIndex;
    if (theID >= kTechEpochPrehistoric && theID < kTechEpochAll)
        theIndex = theID;
    else
        theIndex = theID / 1000 - 1;
    this->mQueues[theIndex].push_back(EECPBuildRequest(inUnitClass, U2DPoint<long>(inX, inY)));
    this->mManager->GetResourceManager()->AddCosts(theTechTree->Get(theID), 1);
}

// FUNCTION: EXE 0x46abb5
// Handles an unfinished building left without builders: orders a citizen to repair it, else (not for town
// centres) destroys it, refunds its cost and queues it again
// (the repair goes to the citizen FindBuilderForTile returns, not to inUnit; an earlier draft had it reversed)
void EECPBuildManager::BuildingAbandoned(EEComplexUnit* inUnit)
{
    long theType = inUnit->mUnitClass->mUnitType;
    if (!(inUnit->mHitPoints > 0) || inUnit->mNumBuilders != 0)
        return;
    U2DPoint<long> theTile(inUnit->mTileX, inUnit->mTileY);
    EECPUnitManager* theUnitManager = this->mManager->mUnitManager;
    EEComplexUnit* theCitizen = theUnitManager->FindBuilderForTile(theTile, theType);
    if (theCitizen != NULL)
        theCitizen->OrderRepair(inUnit);
    else if (theType != kUnitTypeSettlement && theType != kUnitTypeTownCenter && theType != kUnitTypeCapitol) {
        long theID = gUnitClassTable[theType] != NULL ? gUnitClassTable[theType]->mTechnology : 100000;
        EETechTreeNode* theNode = this->mPlayer->GetTechTree()->Get(theID);
        inUnit->Kill(0, 0, 0);
        for (unsigned long i = 2; i < 7; i++) {
            long theAmount = this->mPlayer->ScaleCost(theNode->GetCost(i, 0x20, 1.0f));
            this->mPlayer->AddResource(i, theAmount);
        }
        this->QueueBuilding(theType, 1, 0);
    }
}

// FUNCTION: EXE 0x46acb8
// Forgets every queued request's site, the town centre tile and the update time
void EECPBuildManager::Reset()
{
    for (unsigned long i = 0; i < 15; i++) {
        for (EECPBuildQueue::iterator it = this->mQueues[i].begin(); it != this->mQueues[i].end(); ++it) {
            (*it).second = U2DPoint<long>(0, 0);
        }
    }
    this->mTownCenterTile = U2DPoint<long>(-10000, -10000);
    this->mNextUpdateTime = 0;
}

// FUNCTION: EXE 0x46ad23
// Removes requests from the queues before an epoch whose building is obsolete by then
// (mQueues[i] re-indexed at each use, no reference local)
void EECPBuildManager::DropObsoleteRequests(long inEpoch)
{
    EECPBuildQueue::iterator it;
    EETechTree* theTechTree = this->mPlayer->GetTechTree();
    for (long i = 0; i < inEpoch; i++) {
        for (it = this->mQueues[i].begin(); it != this->mQueues[i].end();) {
            EEUnitClass* theClass = gUnitClassTable[(*it).first];
            long theID = theClass != NULL ? theClass->mTechnology : 100000;
            if (theTechTree->Get(theID)->mObsoleteEpoch < inEpoch)
                it = this->mQueues[i].erase(it, it + 1);
            else
                ++it;
        }
    }
}

// FUNCTION: EXE 0x46add5
// Recollects the dock and space dock site tiles after loading and rebases the update time; returns 0
long EECPBuildManager::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    this->mWaterSites.clear();
    this->CollectSiteTiles(2);
    this->mSpaceSites.clear();
    this->CollectSiteTiles(4);
    this->mNextUpdateTime += gPhysics->mGameTime;
    return 0;
}

// FUNCTION: EXE 0x46ae17
// Writes the build queues, town centre tile and time to the next update to the saved game; returns the
// first write error
// NEAR (91): loop/test order
long EECPBuildManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EECPBuildQueue::iterator it;
    unsigned long theCount;
    long theResult = 0;
    for (unsigned long i = 0; i < 15 && theResult == 0; i++) {
        theCount = this->mQueues[i].size();
        theResult = outFile.Write(theCount);
        if (theResult != 0)
            continue;
        for (it = this->mQueues[i].begin(); it != this->mQueues[i].end() && theResult == 0; ++it) {
            theResult = outFile.Write((*it).first);
            if (theResult == 0)
                theResult = (*it).second.Save(outFile);
        }
    }
    if (theResult == 0 && (theResult = this->mTownCenterTile.Save(outFile)) == 0) {
        theResult = outFile.Write(this->mNextUpdateTime - gPhysics->mGameTime);
    }
    return theResult;
}

// FUNCTION: EXE 0x46aef3
// Records a unit's tile as the town centre tile
// SetXY: VC6 evaluates the arguments right to left (mTileY first)
void EECPBuildManager::SetTownCenter(EEUnit* inUnit)
{
    this->mTownCenterTile.SetXY(inUnit->mTileX, inUnit->mTileY);
}
// FUNCTION: EXE 0x46af0d
// Every 3 seconds starts the first buildable request of each epoch up to the current one (rotating ones
// not yet producible, dropping obsolete ones), cancels stalled builds and every 2 minutes reassigns builders
// Explicit zero request and staged tech-node lookup preserve the original 443-byte body.
void EECPBuildManager::Update(unsigned long inTime)
{
    EECPBuildRequest theRequest(0UL, U2DPoint<long>(0, 0));
    EETechTree* theTechTree = this->mPlayer->GetTechTree();
    long theEpoch = theTechTree->mCurrentEpoch;
    if (!this->mPlayer->mCanBuild || !(inTime > this->mNextUpdateTime))
        return;
    if (!this->mSiteTilesCollected) {
        this->CollectSiteTiles(2);
        this->CollectSiteTiles(4);
    }
    this->CancelStalledBuilds();
    for (long i = 0; i <= theEpoch; i++) {
        if (this->mQueues[i].size() <= 0)
            continue;
        theRequest = this->mQueues[i].front();
        long theID = gUnitClassTable[theRequest.first] != NULL ? gUnitClassTable[theRequest.first]->mTechnology
            : 100000;
        EETechTreeNode* theNode = theTechTree->Get(theID);
        if (theNode == NULL || !theNode->GetIsObsolete()) {
            if (this->mPlayer->CanProduceUnitType(theRequest.first, 0x20, NULL)) {
                this->mQueues[i].pop_front();
                if (this->StartBuilding(theRequest)) {
                    long theID2 = gUnitClassTable[theRequest.first] != NULL
                        ? gUnitClassTable[theRequest.first]->mTechnology : 100000;
                    EETechTreeNode* theNode2 = theTechTree->Get(theID2);
                    this->mManager->mResourceManager->RemoveCosts(theNode2, 1);
                } else
                    this->mQueues[i].push_back(theRequest);
                break;
            }
            this->mQueues[i].pop_front();
            this->mQueues[i].push_back(theRequest);
        } else
            this->mQueues[i].pop_front();
    }
    if (inTime > this->mNextBuilderCheckTime) {
        if (this->mNextBuilderCheckTime > 0)
            this->AssignBuilders();
        this->mNextBuilderCheckTime = inTime + 120000;
    }
    this->mNextUpdateTime = inTime + 3000;
}
// (static: only CanBuildAt calls it; emitted before it) (provisional name)
// FUNCTION: EXE 0x46b0c8
// Tells whether a unit class cannot stand on a tile (off the map or wrong terrain)
static bool EECPBMCannotStandOn(const EETileLocation& inTile, EEUnitClass* inClass)
{
    EETile* theTile = gWorld.mMap != NULL ? gWorld.mMap->GetTileAtLocation(inTile) : NULL;
    return !(theTile != NULL && inClass->mTerrainTypes[theTile->mTerrainType]);
}

// CanBuildAt: the test of a site class's tiles (find_if 0x6c4bb, its operator() expanded): the tile's kind-2 continent
// is mContinent (provisional name)
struct EECPBMOnContinent {
    EECPBMOnContinent(unsigned short inContinent) : mContinent(inContinent) {}
    bool operator()(const U2DPoint<long>& inTile) const
        { return gContinentManager.GetContinent(inTile, kAIPathLandMass) == this->mContinent; }

    unsigned short mContinent;              // +0x00
};

// FUNCTION: EXE 0x46b0fb
// Tells whether a class can be built at a tile: free ground on the right (or reachable) continent, no enemies;
// strictly also no choke point, hill, bush, runway end, town centre, gate near and walkable ground around it
// inMargin adds a one-tile margin; the reachability test (civ power 0x30) starts at (inX, inY); site classes need
// a side or corner tile on inContinent. The border must be passable except one run (at least 3 tiles for site classes)
bool EECPBuildManager::CanBuildAt(unsigned short inContinent, long inUnitClass, EETileCoordinate& inTile, long inSize,
    bool inMargin, bool inStrict, long inX, long inY)
{
    std::vector<U2DPoint<long> > theTiles;
    U2DPoint<long> theCorner;
    unsigned long theCount = 0;
    long theX = inTile.mX;
    long theY = inTile.mY;
    bool theResult = EEPlayerBlockTakes(this->mPlayer, inUnitClass, inMargin ? theX - 1 : theX, inMargin ? theY - 1 : theY,
        inMargin ? inSize + 2 : inSize, inMargin ? inSize + 2 : inSize, 0);
    if (theResult) {
        if (!EECPIsSiteClass(inUnitClass) && inUnitClass != kUnitTypeAA10Gun88mm && inUnitClass != kUnitTypeAA13MissileTower && inUnitClass != kUnitTypeAA15MissileTower) {
            if (gCivPowersManager.HasPower(this->mPlayer, kCivPowerPathfinding) && (inX != -1 || inY != -1)) {
                unsigned short theID = gContinentManager.GetContinentID(inX, inY);
                EEAIContinent* theFrom = theID != 0 ? gContinentManager.GetContinentByID(theID) : NULL;
                if (theFrom != NULL)
                    theResult = gContinentManager.AreConnected(theFrom, gContinentManager.GetContinentAt(inTile.mX, inTile.mY),
                        this->mPlayer->GetCitizenPassability());
                else
                    theResult = gContinentManager.GetContinent(inTile, kAIPathLandMass) == inContinent;
            } else
                theResult = gContinentManager.GetContinent(inTile, kAIPathLandMass) == inContinent;
        }
    }
    if (theResult) {
        if (EECPIsSiteClass(inUnitClass)) {
            EEGetSideTiles(theX, theY, inSize, theTiles);
            theResult = std::find_if(theTiles.begin(), theTiles.end(), EECPBMOnContinent(inContinent)) != theTiles.end();
            if (!theResult) {
                EEGetCornerTiles(1, theX, theY, inSize, theTiles);
                theResult = std::find_if(theTiles.begin(), theTiles.end(), EECPBMOnContinent(inContinent))
                    != theTiles.end();
            }
        }
    }
    if (theResult) {
        theResult = !this->HasEnemyNear(theX, theY);
        if (theResult && inStrict) {
            bool theOpen;
            if (gWorld.mMap != NULL && ((gWorld.mMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap))
                theOpen = true;
            else
                theOpen = false;
            if (!theOpen)
                theResult = gContinentManager.GetChokePointAt(inTile) == NULL;
            if (theResult) {
                theResult = gContinentManager.GetHillAt(inTile) == NULL;
                if (theResult) {
                    if (inUnitClass == kUnitTypeFarm || inUnitClass == kUnitTypeRoboticFarm)
                        theResult = EEGetTileUnitOfType(inTile, kUnitTypeResForageBush) == NULL;
                    if (theResult) {
                        if (this->mManager->mUnitManager->GetCount(0x42) > 0)
                            theResult = !this->IsNearAirportEnd(theX, theY, inSize, 2.0f);
                        if (theResult) {
                            if (!EECPIsSiteClass(inUnitClass))
                                theResult = !this->HasTownCenterNear(theX, theY, 3.0f);
                            if (theResult) {
                                if (!theOpen)
                                    theResult = !this->HasGateNear(theX, theY, 4.0f);
                                if (theResult && inUnitClass != kUnitTypeGranary && inUnitClass != kUnitTypeFarm && inUnitClass != kUnitTypeRoboticFarm
                                    && inUnitClass != kUnitTypeAA10Gun88mm && inUnitClass != kUnitTypeAA13MissileTower && inUnitClass != kUnitTypeAA15MissileTower) {
                                    long i;
                                    theTiles.clear();
                                    theCorner.mY = inTile.mY - 1;
                                    theCorner.mX = inTile.mX - 1;
                                    theTiles.push_back(theCorner);
                                    for (i = 0; i < inSize + 1; i++) {
                                        theCorner.mY++;
                                        theTiles.push_back(theCorner);
                                    }
                                    for (i = 0; i < inSize + 1; i++) {
                                        theCorner.mX++;
                                        theTiles.push_back(theCorner);
                                    }
                                    for (i = 0; i < inSize + 1; i++) {
                                        theCorner.mY--;
                                        theTiles.push_back(theCorner);
                                    }
                                    for (i = 0; i < inSize; i++) {
                                        theCorner.mX--;
                                        theTiles.push_back(theCorner);
                                    }
                                    EEUnitClass* theClass;
                                    if (inUnitClass == kUnitTypeDock || inUnitClass == kUnitTypeNavyYard)
                                        theClass = gUnitClassTable[kUnitTypeS02FishingBoatStone];
                                    else if (inUnitClass == kUnitTypeSpaceDock || inUnitClass == kUnitTypeSpaceTurret || inUnitClass == kUnitTypeOrbitalSpaceStation)
                                        theClass = gUnitClassTable[kUnitTypeSp15SpaceTransport];
                                    else
                                        theClass = gUnitClassTable[kUnitTypeCitizen];
                                    unsigned long j;
                                    for (j = 0; EECPBMCannotStandOn(*theTiles.begin(), theClass) && j < theTiles.size();
                                        j++) {
                                        theCorner.mX = theTiles.begin()->mX;
                                        theCorner.mY = theTiles.begin()->mY;
                                        theTiles.erase(theTiles.begin());
                                        theTiles.push_back(theCorner);
                                    }
                                    for (j = 0; !EECPBMCannotStandOn(*theTiles.begin(), theClass) && j < theTiles.size();
                                        j++) {
                                        theCorner.mX = theTiles.begin()->mX;
                                        theCorner.mY = theTiles.begin()->mY;
                                        theTiles.erase(theTiles.begin());
                                        theTiles.push_back(theCorner);
                                    }
                                    std::vector<U2DPoint<long> >::iterator it;
                                    for (it = theTiles.begin(); it != theTiles.end();) {
                                        if (!EECPBMCannotStandOn(*it, theClass))
                                            break;
                                        it = theTiles.erase(it);
                                        if (EECPIsSiteClass(inUnitClass))
                                            theCount++;
                                    }
                                    if (EECPIsSiteClass(inUnitClass))
                                        theResult = theCount >= 3;
                                    for (it = theTiles.begin(); it != theTiles.end() && theResult; it++)
                                        theResult = !EECPBMCannotStandOn(*it, theClass);
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

// FUNCTION: EXE 0x46b6b2
// Tells whether a class is built on a shore or space site (dock, navy yard, space dock and two space classes)
bool EECPIsSiteClass(long inUnitClass)
{
    return inUnitClass == 0x24 || inUnitClass == 0x41 || inUnitClass == 0x2d8 || inUnitClass == 0x302
        || inUnitClass == 0x31f;
}

// (EECPResourceManager.cpp's range: 0x4921f7)
bool EECPUnitWithinDistance::operator()(EEUnit* inUnit) const
{
    U2DPoint<long> theTile(inUnit->mTileX, inUnit->mTileY);
    return (float)theTile.DistanceToSquared(this->mCenter) <= this->mDistanceSquared;
}

// The deque/vector/heap/sort COMDATs (named through call sites)
// TEMPLATE: EXE 0x466cfb
// TEMPLATE: EXE 0x468849
// TEMPLATE: EXE 0x46b6dc
// TEMPLATE: EXE 0x46b76c
// TEMPLATE: EXE 0x46b798
// TEMPLATE: EXE 0x46b82b
// TEMPLATE: EXE 0x46b857
// TEMPLATE: EXE 0x46b883
// TEMPLATE: EXE 0x46b92c
// TEMPLATE: EXE 0x46ba3a
// TEMPLATE: EXE 0x46ba94
// TEMPLATE: EXE 0x46bd85
// TEMPLATE: EXE 0x46bdbe
// TEMPLATE: EXE 0x46bde5
// TEMPLATE: EXE 0x46be1f
// TEMPLATE: EXE 0x46be46
// TEMPLATE: EXE 0x46be8e
// TEMPLATE: EXE 0x46bf74
// TEMPLATE: EXE 0x46c027
// TEMPLATE: EXE 0x46c065
// TEMPLATE: EXE 0x46c120
// TEMPLATE: EXE 0x46c1e6
// TEMPLATE: EXE 0x46c2cc
// TEMPLATE: EXE 0x46c3b0
// TEMPLATE: EXE 0x46c494
// TEMPLATE: EXE 0x46c4bb
// TEMPLATE: EXE 0x46c4e7
// TEMPLATE: EXE 0x46c536
// TEMPLATE: EXE 0x46c5ce
// TEMPLATE: EXE 0x46c5f3
// TEMPLATE: EXE 0x46c62b
// TEMPLATE: EXE 0x46c65e
// TEMPLATE: EXE 0x46c76b
// TEMPLATE: EXE 0x46c7fa
// TEMPLATE: EXE 0x46c854
// TEMPLATE: EXE 0x46c87d
// TEMPLATE: EXE 0x46c956
// TEMPLATE: EXE 0x46c993
// TEMPLATE: EXE 0x46c9c0
// TEMPLATE: EXE 0x46ca9c
// TEMPLATE: EXE 0x46cad0
// TEMPLATE: EXE 0x46cb03
// TEMPLATE: EXE 0x46cc10
// TEMPLATE: EXE 0x46cc55
// TEMPLATE: EXE 0x46cc88
// TEMPLATE: EXE 0x46cd90
// TEMPLATE: EXE 0x46cdc3
// TEMPLATE: EXE 0x46cecb
// TEMPLATE: EXE 0x46cf0d
// TEMPLATE: EXE 0x46cf51
// TEMPLATE: EXE 0x46cf9e
// TEMPLATE: EXE 0x46d0b5
// TEMPLATE: EXE 0x46d1cd
// TEMPLATE: EXE 0x46d21a
// TEMPLATE: EXE 0x46d2d1
// TEMPLATE: EXE 0x46d30e
// TEMPLATE: EXE 0x46d34d
// TEMPLATE: EXE 0x46d446
// TEMPLATE: EXE 0x46d470
// TEMPLATE: EXE 0x46d510
// TEMPLATE: EXE 0x46d62f
// TEMPLATE: EXE 0x46d66c
// TEMPLATE: EXE 0x46d6b0
// TEMPLATE: EXE 0x46d7d0
// TEMPLATE: EXE 0x46d803
// TEMPLATE: EXE 0x46d8c1
// TEMPLATE: EXE 0x46d905
// TEMPLATE: EXE 0x46d952
// TEMPLATE: EXE 0x46da69
// TEMPLATE: EXE 0x46daa5
// TEMPLATE: EXE 0x46dae9
// TEMPLATE: EXE 0x46db36
// TEMPLATE: EXE 0x46db69
// TEMPLATE: EXE 0x46dc1a
// TEMPLATE: EXE 0x46dc5e
// TEMPLATE: EXE 0x46dcab
// TEMPLATE: EXE 0x46ddcb
// TEMPLATE: EXE 0x46ddfe
// TEMPLATE: EXE 0x46deaa
// TEMPLATE: EXE 0x46ded6
// TEMPLATE: EXE 0x46df09
// TEMPLATE: EXE 0x46df70
// TEMPLATE: EXE 0x46df99
// TEMPLATE: EXE 0x46dfbe
// TEMPLATE: EXE 0x46e005
// TEMPLATE: EXE 0x46e032
// TEMPLATE: EXE 0x46e092
// TEMPLATE: EXE 0x46e0c5
// TEMPLATE: EXE 0x46e122
// TEMPLATE: EXE 0x46e155
// TEMPLATE: EXE 0x46e1b2
// TEMPLATE: EXE 0x46e1e5
// TEMPLATE: EXE 0x46e26d
// TEMPLATE: EXE 0x46e2ce
// TEMPLATE: EXE 0x46e2fc
// TEMPLATE: EXE 0x46e36d
// TEMPLATE: EXE 0x46e3c3
// TEMPLATE: EXE 0x46e457
// TEMPLATE: EXE 0x46e4d0
// TEMPLATE: EXE 0x46e540
// TEMPLATE: EXE 0x46e5ba
// TEMPLATE: EXE 0x46e683
// TEMPLATE: EXE 0x46e6ca
// TEMPLATE: EXE 0x46e719
// TEMPLATE: EXE 0x46e750
// TEMPLATE: EXE 0x46e7b0
// TEMPLATE: EXE 0x46e7fd
// TEMPLATE: EXE 0x46e841
// TEMPLATE: EXE 0x46e890

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was compared
// against the original by name)
// TEMPLATE: EXE 0x46a20e

// Template and inline COMDATs this unit already emits (identified by name)
// TEMPLATE: EXE 0x46bace
// TEMPLATE: EXE 0x46c585
// TEMPLATE: EXE 0x46c5a2
// TEMPLATE: EXE 0x46e62a
