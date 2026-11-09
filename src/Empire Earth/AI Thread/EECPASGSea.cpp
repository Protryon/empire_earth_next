// C:\EEX\Empire Earth\AI Thread\EECPASGSea.cpp (file name inferred: no __FILE__). 0x4dc37..0x4e6e7 (unit map
// edges, not verified yet).
#include <windows.h>
#include <vector>
#include <set>
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
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"

#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EECPASGSea.h"

#include "ee/EECPAttackFormation.h"

#include "ee/EECPAGOrder.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"

#include "ee/EECPAAirTargetFilters.h"
#include "ee/EECPAreaTargetCollector.h"

#include "ee/EEPathManager.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPASGSpaceship.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPPersonnel.h"
#include "ee/EETown.h"

// FUNCTION: EXE 0x44dc37
// Creates a naval attack sub-group belonging to the given attack group
EECPASGSea::EECPASGSea(EECPAttackGroup* inGroup, long inKind) : EECPAttackSubGroup(inGroup, inKind)
{
}

// SYNTHETIC: EXE 0x44dc53
// (the scalar deleting destructor)

// FUNCTION: EXE 0x44dc6f
// Destroys the naval sub-group (base members only)
EECPASGSea::~EECPASGSea()
{
}

// FUNCTION: EXE 0x44dc7a
// Counts the sub-group's units that are submarines
long EECPASGSea::CountSubmarines()
{
    long theCount = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = this->mUnits[i]->mUnit;
        if (theUnit != NULL && theUnit->mUnitClass->mAIKind == kAIKindSubmarine)
            theCount++;
    }
    return theCount;
}

// FUNCTION: EXE 0x44dcbc
// Takes the requested units as the base does; if they sit in more than one body of water, orders the attack group
// to split the sub-group; then refreshes the units and returns the base's result
bool EECPASGSea::TakeRequestedUnits()
{
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*> theUnits;
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theIter;
    EECPAGOrder theOrder;
    bool theResult = EECPAttackSubGroup::TakeRequestedUnits();
    if (theResult && this->mUnits.size() > 1) {
        this->GroupUnitsByWaterArea(theUnits, 1, true);
        if (theUnits.size() > 1) {
            theOrder.mType = kAGOrderSplitSubGroup;
            theOrder.SetSubGroup(this);
            theOrder.mIsActive = true;
            this->mGroup->mOrders.push_back(theOrder);
        }
        for (theIter = theUnits.begin(); theIter != theUnits.end(); theIter++)
            delete theIter->second;
    }
    this->RefreshUnits();
    return theResult;
}

// FUNCTION: EXE 0x44dd9f
// Rebuilds the formation with 2-tile spacing and no forced moves and places it at the location, facing the
// heading from the sub-group's position
void EECPASGSea::PlaceFormation(const EECPILocation& inLocation)
{
    EETileLocation theTile;
    this->FindPosition(theTile, true, true);
    float theDistance = EEGetSeaHeading(theTile, inLocation.mTile);
    this->mFormation->Build();
    this->mFormation->mNoForcedMove = true;
    for (long i = this->mFormation->GetNumNodes() - 1; i >= 0; i--)
        this->mFormation->mNodes[i]->mSpacing = 2.0f;
    this->mFormation->Place(inLocation, theDistance);
}

// FUNCTION: EXE 0x44de1f
// Collects the units in the bounds that pass the naval target filter (enemies optional): walls become siege
// targets; towers, gates and kind-4 tech-tree buildings are both normal and siege targets
void EECPASGSea::CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies)
{
    std::vector<EEComplexUnit*> theUnits;
    EECPASeaTargetFilter theFilter;
    EECPASTargetUnit* theTarget;
    theFilter.mAlternate = false;
    theFilter.mAllowNaval = false;
    theFilter.mPlayer = this->mPlayer2;
    theFilter.mIncludeEnemies = inIncludeEnemies;
    theFilter.mGroup = this;
    theFilter.mAllowShore = this->mCanAttackShore;
    theFilter.mPlayerID = this->mTask.GetPlayerID();
    if (EECollectASGTargets(*inBounds, theUnits, theFilter, true) != 0) {
        this->mTargets.reserve(theUnits.size());
        for (long i = theUnits.size() - 1; i >= 0; --i) {
            theTarget = EECPAttackSubGroup::NewTarget(theUnits[i]);
            EEUnitClass* theClass = theUnits[i]->mUnitClass;
            if (theClass->mIsWall && !theClass->mIsTower) {
                this->mSiegeTargets.push_back(theTarget);
            } else {
                this->mTargets.push_back(theTarget);
                if (theClass->mIsTower || theClass->mCategory == kUnitCategoryGate ||
                    (theTarget->mUnit->GetTechTreeNode() != NULL &&
                     theTarget->mUnit->GetTechTreeNode()->mData->mKind == 4)) {
                    theTarget = EECPAttackSubGroup::NewTarget(theUnits[i]);
                    this->mSiegeTargets.push_back(theTarget);
                }
            }
        }
    }
}

// FUNCTION: EXE 0x44df9a
// Returns true if the sub-group is empty or its first unit lies in the same body of water as the location
bool EECPASGSea::CanReach(const EECPILocation* inLocation)
{
    bool theResult = true;
    if (this->mUnits.size() > 0) {
        EEComplexUnit* theUnit = (*this->mUnits.begin())->mUnit;
        theResult = gContinentManager.GetContinent(inLocation->mTile, kAIPathWaterBody) == gContinentManager.GetContinent(theUnit, kAIPathWaterBody);
    }
    return theResult;
}

// FUNCTION: EXE 0x44dfe6
// Checks whether the sub-group is at the location the base way and discards the answer
void EECPASGSea::IsAtLocation(const EECPILocation& inLocation)
{
    EECPAttackSubGroup::IsAt(inLocation);
}

// FUNCTION: EXE 0x44dff2
// Returns true if the sub-group is in the location's path sub-sector or within 8 tiles of it; else, in a neighbour
// sector, once all units settle, or within 30 tiles when the target sector's terrain kind is not allowed
bool EECPASGSea::IsAt(const EECPILocation& inLocation)
{
    EEAIPathSubSector* theTarget = inLocation.GetPathSubSector();
    EETileLocation theTile;
    bool theResult = this->FindPosition(theTile, true, false);
    if (!theResult || theTile.mX == -10000)
        return theResult;
    EEAIPathSubSector* theCurrent = gContinentManager.GetPathSubSectorAt(theTile);
    if (theCurrent == NULL)
        return theResult;
    if (theCurrent != theTarget && (unsigned long)theTile.DistanceToSquared(inLocation.mTile) > 64) {
        theResult = false;
        if (theCurrent->IsNeighbor(theTarget))
            theResult = this->AreAllUnitsSettled();
        else if (theTarget == NULL)
            theResult = true;
        else if (!this->mFlags.mFlags[theTarget->mKind])
            theResult = (unsigned long)theTile.DistanceToSquared(inLocation.mTile) <= 900;
    } else
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x44e0b1
// Returns the path sub-sector holding the location's tile
EEAIPathSubSector* EECPILocation::GetPathSubSector() const
{
    long theY = this->mTile.GetY();
    long theX = this->mTile.mX;
    return gContinentManager.GetPathSubSectorAt(theX, theY);
}

// FUNCTION: EXE 0x44e0c4
// Returns true if the sub-group mixes submarines with other ships
bool EECPASGSea::DecideSplit()
{
    long theCount = this->CountSubmarines();
    if (theCount != 0 && theCount != this->GetNumUnits())
        return true;
    return false;
}

// FUNCTION: EXE 0x44e0ea
// Returns true if the task is active; a location task must also lie on a water continent
bool EECPASGSea::IsTaskActive(const EECPAGTask& inTask)
{
    bool theResult = inTask.mIsActive;
    if (theResult == true && inTask.mType == kAGTaskLocation) {
        long theY = inTask.mLocation.mTile.GetY();
        long theX = inTask.mLocation.mTile.mX;
        theResult = false;
        EEAIContinent* theContinent = gContinentManager.GetContinentAt(theX, theY);
        if (theContinent != NULL)
            theResult = theContinent->mCategory == kContinentCategoryWater;
    }
    return theResult;
}

// FUNCTION: EXE 0x44e125
// Loads the sub-group from a saved game (base data only); returns the base's error code
long EECPASGSea::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return EECPAttackSubGroup::Load(inMajorVersion, inMinorVersion, inFile);
}

// FUNCTION: EXE 0x44e139
// Sets the destination the base way
void EECPASGSea::SetDestination(const EECPILocation& inLocation)
{
    EECPAttackSubGroup::SetDestination(inLocation);
}

// FUNCTION: EXE 0x44e145
// Requests a sea route between two tiles from the path manager, walls and gates allowed, with the sea route planner
void EECPASGSea::RequestPath(void* inFrom, void* inTo)
{
    EECPASGWeights theWeights;
    theWeights.Fill(0.0f);
    this->GetPathCosts(inFrom, inTo, theWeights);
    this->mFlags.mFlags[kAITileWall] = true;
    this->mFlags.mFlags[kAITileGate] = true;
    this->mPath.clear();
    this->CancelPath();
    EEAIGPGModifierSeaRoutePlanner* theModifier = new EEAIGPGModifierSeaRoutePlanner(this->mPlayer->GetPlayerID(), this, theWeights);
    // The arguments point at tile coordinates; the graph modifier is passed as an opaque handle
    this->mPathRequest = gPathManager.RequestPath(this->mPlayer->GetPlayerID(),
        *static_cast<EETileCoordinate*>(inFrom), *static_cast<EETileCoordinate*>(inTo),
        &this->mFlags, 2, -1, theModifier, 0);
    ++this->mPlayer->mCombatManager->mAttackCommander->mUpdatesThisTick;
    theModifier->Dereference();
}

// FUNCTION: EXE 0x44e203
// Tests a condition: has-location-task is true for an active location, type-10 or wonder-defence task; others go
// to the base
bool EECPASGSea::TestCondition(long inState, long inTime)
{
    bool theResult = false;
    if (inState != kASGConditionHasLocationTask)
        return EECPAttackSubGroup::TestCondition(inState, inTime);
    if (this->mTask.mIsActive) {
        long theKind = this->mTask.mType;
        if (theKind == kAGTaskLocation || (theKind > 9 && theKind <= kAGTaskDefendWonder))
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x44e240
// Returns true while the task location still has influence for the task's player
bool EECPASGSea::IsTerritoryTaskValid(long inTime)
{
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    return theTable->GetTileValue(this->mTask.GetPlayerID(), this->mTask.mLocation.mTile) ? true : false;
}

// FUNCTION: EXE 0x44e26a
// Re-checks the task at random 5-35 s intervals: a location attack stays valid while enemies have influence within
// 2 regions, a wonder defence while the wonder lives; defence or modeless tasks lapse with a 65% chance
bool EECPASGSea::IsLocationTaskValid(long inTime)
{
    U2DRectangle<long> theBounds;
    bool theResult = true;
    if (this->mNextTaskCheckTime < (unsigned long)inTime) {
        switch (this->mTask.mType) {
        case kAGTaskAttackLocation:
            theResult = false;
            if (gWorld.mMap->GetTileAtLocation(this->mTask.mLocation.mTile) != NULL) {
                long theRegionSize = gWorld.mInfluenceTable->mRegionSize;
                U2DPoint<long> theRegion(this->mTask.mLocation.mTile.mX / theRegionSize,
                                         this->mTask.mLocation.mTile.GetY() / theRegionSize);
                theBounds.mLeft = theRegion.mX;
                theBounds.mTop = theRegion.GetY();
                U2DPoint<long> theBottomRight(theBounds.mLeft, theBounds.mTop);
                theBounds.mRight = theBottomRight.mX;
                theBounds.mBottom = theBottomRight.GetY();
                U2DPoint<long> theTopLeft(theBounds.mLeft, theBounds.mTop);
                theBounds.mLeft = theTopLeft.mX - 2;
                theBounds.mTop = theTopLeft.GetY() - 2;
                U2DPoint<long> theEnd(theBounds.mRight, theBounds.mBottom);
                theBounds.mRight = theEnd.mX + 2;
                theBounds.mBottom = theEnd.GetY() + 2;
                theResult = gWorld.mInfluenceTable->SumEnemyValue(this->mPlayer->GetPlayerID(), theBounds, false) != 0;
                if (theResult)
                    this->mNextTaskCheckTime = gPhysics->GetRandom(5000L, 35000L) + inTime;
            }
            break;
        case kAGTaskDefendWonder:
            this->mNextTaskCheckTime = gPhysics->GetRandom(5000L, 35000L) + inTime;
            if (this->mTask.mUnit == NULL || this->mTask.mUnit->mHitPoints <= 0)
                theResult = false;
            break;
        default:
            if (gPhysics->mRandom.Generate() > 0.35f && (this->mTask.mMode == kAGModeDefense || this->mTask.mMode == kAGModeNone))
                theResult = false;
            else
                this->mNextTaskCheckTime = gPhysics->GetRandom(5000L, 35000L) + inTime;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x44e42f
// Returns true while the task unit lives and either the task is a wonder task or the next follow time has passed
bool EECPASGSea::IsUnitTaskValid(long inTime)
{
    bool theResult = false;
    EEComplexUnit* theUnit = this->mTask.mUnit;
    if (theUnit != NULL && theUnit->mHitPoints > 0) {
        if (this->mTask.mMode != kAGModeWonder)
            theResult = this->mNextFollowTime < inTime;
        else
            theResult = true;
    }
    return theResult;
}

// TEMPLATE: EXE 0x44e45d
// TEMPLATE: EXE 0x44e488
// TEMPLATE: EXE 0x44e4f9
// (the multimap<long, EECPAttackSubGroupUnit*> dtor and erase; this build emits them for Update's copy
// at the end of the unit, evidence that the original unit held that body too)

// FUNCTION: EXE 0x44e52b
// Collects the units in the bounds that pass the naval target filter, dropping duplicates when a multi-tile
// unit was accepted and uniqueness was asked for; returns the count
// EECollectASGTargets<EECPASeaTargetFilter> (ee/EECPAreaTargetCollector.h), emitted by CollectTargets above

// FUNCTION: EXE 0x44e6d4
// Returns the path sub-sector holding the tile
EEAIPathSubSector* EEAIContinentManager::GetPathSubSectorAt(const EETileLocation& inTile)
{
    long theY = inTile.GetY();
    long theX = inTile.mX;
    return this->GetPathSubSectorAt(theX, theY);
}

// ICF-FOLDED: EXE 0x67c0b4
// Returns false
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGSea::IsAwayFromTowns()
{
    return false;
}

// ICF-FOLDED: EXE 0x4e123d
// Does nothing for naval sub-groups
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EECPASGSea::MoveToNearestTown()
{
}

// ICF-FOLDED: EXE 0x44f603
// Switches state: idle gets a new task, split asks to split, move heads for the task, hold/move-to-task/follow run
// their handlers; other states go to the base class
// (no address of its own: the same body as EECPASGSpaceship::SetState, which ICF kept; [sib 6])
void EECPASGSea::SetState(long inState, long inTime)
{
    unsigned long theOldState = this->mState;
    this->mState = inState;
    switch (this->mState) {
    case kASGStateIdle:
        if (this->mUnits.size() > 0 && !this->mTask.mIsActive)
            this->mGroup->AssignTask(this);
        break;
    case kASGStateSplit:
        this->RequestSplit(true);
        break;
    case kASGStateMove:
        this->mNextWaypoint = this->mTask.mLocation;
        this->MoveToDestination();
        break;
    case 5000:
        break;
    case 6000:
        this->Virtual1D8(inTime);
        break;
    case kASGStateHoldAtTask:
        this->HoldAtTask(inTime);
        break;
    case 6003:
    case 6004:
        this->MoveToTask(false);
        break;
    case kASGStateFollowTaskUnit:
        this->FollowTaskUnit(inTime);
        break;
    default:
        this->mState = theOldState;
        EECPAttackSubGroup::SetState(inState, inTime);
        break;
    }
}

// ICF-FOLDED: EXE 0x44f70e
// Runs the state machine each tick: scans or drops targets, updates units, then moves between the idle, wait,
// gather, split, attack, hold, follow, return and hold-territory states as their conditions change
// (no address of its own: the same body as EECPASGSpaceship::Update, which ICF kept; [sib 36])
void EECPASGSea::Update(long inTime)
{
    if (this->mNextScanTime <= inTime && this->mUnits.size() > 0)
        this->ScanTargets(1);
    else
        this->DropLostTargets(inTime);
    this->UpdateUnits(inTime);
    bool theFailed = false;
    switch (this->mState) {
    case kASGStateIdle:
        theFailed = !this->TestCondition(kASGConditionHasUnits, inTime);
        if (theFailed)
            this->SetState(kASGStateWaitForUnits, inTime);
        if (!theFailed) {
        if (this->TestCondition(kASGConditionShouldSplit, inTime))
            this->SetState(kASGStateSplit, inTime);
        else if (!this->CheckTaskArrival(inTime)) {
            bool theHandled = false;
            if (this->mTask.mIsActive) {
                theHandled = true;
                switch (this->mTask.mType) {
                case kAGTaskLocation:
                case kAGTaskAttackLocation:
                case kAGTaskDefendWonder:
                    this->SetState(kASGStateHoldAtTask, inTime);
                    break;
                case kAGTaskTerritory:
                    this->SetState(kASGStateHoldTerritory, inTime);
                    break;
                case 4:
                    this->SetState(6003, inTime);
                    break;
                case 5:
                    this->SetState(6005, inTime);
                    break;
                case kAGTaskUnit:
                case kAGTaskAttackWonder:
                    this->SetState(kASGStateFollowTaskUnit, inTime);
                    break;
                case kAGTaskReturn:
                    this->SetState(kASGStateReturnToRallyPoint, inTime);
                    break;
                default:
                    theHandled = false;
                    break;
                }
            }
            if (!theHandled) {
                if (this->TestCondition(kASGConditionHasTargets, inTime))
                    this->SetState(kASGStateAttack, inTime);
                else if (this->TestCondition(kASGConditionShouldReturn, inTime))
                    this->SetState(kASGStateReturnToRallyPoint, inTime);
                else if (!this->TestCondition(kASGConditionTaskValid, inTime))
                    this->SetState(kASGStateIdle, inTime);
            }
        }
        }
        break;
    case kASGStateWaitForUnits:
        if (this->TestCondition(kASGConditionRequestEmpty, inTime))
            this->SetState(kASGStateRemove, inTime);
        else if (this->TestCondition(kASGConditionRequestFilled, inTime))
            this->SetState(kASGStateGather, inTime);
        break;
    case kASGStateGather:
        theFailed = !this->TestCondition(kASGConditionHasUnits, inTime);
        if (theFailed)
            this->SetState(kASGStateReturnToRallyPoint, inTime);
        if (!theFailed && this->TestCondition(kASGConditionGathered, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateSplit:
        if (this->TestCondition(kASGConditionAlways, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateMove:  // NEAR: the original's compare chain keeps 2001 (0x44f789: -> the end); an empty case drops it
        break;
    case kASGStateAttack:
        theFailed = !this->TestCondition(kASGConditionHasTargets, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed)
            this->CheckTaskArrival(inTime);
        break;
    case kASGStateHoldAtTask:
        if (this->TestCondition(kASGConditionHasTargets, inTime))
            this->SetState(kASGStateAttack, inTime);
        else {
            theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
            if (theFailed)
                this->SetState(kASGStateIdle, inTime);
            if (!theFailed && this->TestCondition(kASGConditionShouldRefreshTask, inTime))
                this->SetState(kASGStateRepositionAtTask, inTime);
        }
        break;
    case kASGStateFollowTaskUnit:
        if (this->TestCondition(kASGConditionHasTargets, inTime))
            this->SetState(kASGStateAttack, inTime);
        else if (this->TestCondition(3004, inTime))
            this->SetState(kASGStateFollowTaskUnit, inTime);
        else if (!this->TestCondition(kASGConditionTaskValid, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateReturnToRallyPoint:
        if (this->TestCondition(kASGConditionAtRallyPoint, inTime))
            this->SetState(kASGStateRemove, inTime);
        else if (this->TestCondition(kASGConditionHasTargets, inTime))
            this->SetState(kASGStateAttack, inTime);
        break;
    case kASGStateHoldTerritory:
        if (this->TestCondition(kASGConditionHasTargets, inTime))
            this->SetState(kASGStateAttack, inTime);
        else if (this->TestCondition(kASGConditionHasLocationTask, inTime))
            this->SetState(kASGStateHoldAtTask, inTime);
        else if (this->TestCondition(kASGConditionHasUnitTask, inTime))
            this->SetState(kASGStateFollowTaskUnit, inTime);
        else if (this->TestCondition(kASGConditionTerritoryTaskInvalid, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    default:
        this->SetState(kASGStateIdle, inTime);
        break;
    }
    this->mNextUpdateTime = inTime;
}

// ICF-FOLDED: EXE 0x44f114
// Finds the sub-group's position the base way
// (no address of its own: the same body as EECPASGSeaTransport::FindPosition, which ICF kept; [sib 49])
bool EECPASGSea::FindPosition(EETileLocation& outTile, bool inArg2, bool inForce)
{
    return EECPAttackSubGroup::FindPosition(outTile, inArg2, inForce);
}

// ICF-FOLDED: EXE 0x44f128
// Returns the number of normal plus siege targets
// (no address of its own: the same body as EECPASGSeaTransport::GetNumTargets, which ICF kept; [sib 54])
long EECPASGSea::GetNumTargets()
{
    return this->mTargets.size() + this->mSiegeTargets.size();
}

// ICF-FOLDED: EXE 0x451479
// Picks units to split off: the largest group when units sit in several bodies of water, else all kind-0x15
// units (never all of them), else the first half
// (no address of its own: the same body as EECPASGSubmarine::PickSplitUnits, which ICF kept; [sib 59])
void EECPASGSea::PickSplitUnits(std::vector<EEComplexUnit*>& outUnits)
{
    std::multimap<long, EECPAttackSubGroupUnit*> theRanked;
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*> theGroups;
    std::vector<EECPAttackSubGroupUnit*>* theLargest = NULL;
    long theHalf = this->GetNumUnits() / 2;
    if (this->GetNumUnits() > 1) {
        this->GroupUnitsByWaterArea(theGroups, 1, true);
        if (theGroups.size() > 1) {
            std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theGroup;
            for (theGroup = theGroups.begin(); theGroup != theGroups.end(); ++theGroup)
                if (theLargest == NULL || theGroup->second->size() > theLargest->size())
                    theLargest = theGroup->second;
            if (theLargest != NULL) {
                for (long i = theLargest->size() - 1; i >= 0; i--)
                    outUnits.push_back(((*theLargest)[i])->GetUnit());
            }
        }
    }
    if (theLargest == NULL && this->GetNumUnits() > 1) {
        for (long i = this->GetNumUnits() - 1; i >= 0; i--) {
            EECPAttackSubGroupUnit* theUnit = i < this->GetNumUnits() ? this->mUnits[i] : NULL;
            long theKind = this->GetUnitUnit(i)->GetUnitClass()->mAIKind;
            theRanked.insert(std::multimap<long, EECPAttackSubGroupUnit*>::value_type(theKind, theUnit));
        }
        std::multimap<long, EECPAttackSubGroupUnit*>::iterator theEntry;
        for (theEntry = theRanked.begin(); theEntry != theRanked.end(); ++theEntry)
            if (theEntry->first == 0x15)
                outUnits.push_back(theEntry->second->GetUnit());
        if (outUnits.size() == theRanked.size() && outUnits.size() > 1)
            outUnits.pop_back();
        if (outUnits.size() == 0 && this->GetNumUnits() - theHalf > 0) {
            for (long i = theHalf - 1; i >= 0; i--) {
                EEComplexUnit* theUnit = this->GetUnitUnit(i);
                if (theUnit != NULL)
                    outUnits.push_back(theUnit);
            }
        }
    }
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theGroup;
    for (theGroup = theGroups.begin(); theGroup != theGroups.end(); ++theGroup)
        delete theGroup->second;
}

// ICF-FOLDED: EXE 0x441976
// Replaces the sub-group's personnel request with a town request (at the town's centre) or a continent request
// for the given classes; during a wonder race the request expires after 300 s
// (no address of its own: the same body as EECPASGAir::RequestUnits, which ICF kept)
void EECPASGSea::RequestUnits(std::vector<long>& inClasses, std::vector<long>& inUnitIDs, long inContinent, long inTownID)
{
    if (this->mRequest != NULL) {
        this->mGroup->mCommander->mPersonnel->ReleaseRequest(this->mRequest->mID);
        this->mRequest->Dereference();
    }
    EETown* theTown = NULL;
    if (inTownID != 0)
        theTown = this->mPlayer->GetTownByID(inTownID);
    if (theTown == NULL)
        this->mRequest = this->mGroup->mCommander->mPersonnel->NewContinentRequest(inClasses, inUnitIDs, inContinent);
    else {
        EECPPersonnel* thePersonnel = this->mGroup->mCommander->mPersonnel;
        this->mRequest = thePersonnel->NewTownRequest(inClasses, inUnitIDs, inTownID, theTown->mHull.GetCenter());
    }
    if (this->mRequest != NULL && this->mPlayer2->mCombatManager->mIsWonderRace)
        this->mRequest->mDeadline = gPhysics->mGameTime + 300000;
}

// ICF-FOLDED: EXE 0x44dfe6
// Tests whether the sub-group is at the location (the result is dropped)
// (no address of its own: the same body as EECPASGSea::IsAtLocation, which ICF kept)
void EECPASGSpaceship::IsAtLocation(const EECPILocation& inLocation)
{
    EECPAttackSubGroup::IsAt(inLocation);
}
