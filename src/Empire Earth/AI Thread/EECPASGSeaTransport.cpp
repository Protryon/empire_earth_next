// C:\EEX\Empire Earth\AI Thread\EECPASGSeaTransport.cpp (file name inferred: no __FILE__). 0x4e6e7..0x4fd61 (unit map
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
#include "ee/EECPASGSea.h"
#include "ee/EECPASGLand.h"
#include "ee/EECPASGTransport.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EECPASGSeaTransport.h"
#include "ee/EECPASGSpaceship.h"
#include "ee/EECPAttackFormation.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUGLocation.h"

#include "ee/EECPAAirTargetFilters.h"
#include "ee/EECPAreaTargetCollector.h"

#include "ee/EEPathManager.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "ee/EECPCombatManager.h"
#include "ee/EEUGLoadContainer.h"
#include "ee/EEMathInline.h"
#include "ee/EEUnitUtilities.h"

// FUNCTION: EXE 0x44e6e7
// Creates a naval transport sub-group for the attack group, with no land group to carry yet
EECPASGSeaTransport::EECPASGSeaTransport(EECPAttackGroup* inGroup, long inKind)
    : EECPASGSea(inGroup, inKind), EECPASGTransport()
{
    this->mLand = NULL;
    this->mOwner = this;
}

// SYNTHETIC: EXE 0x44e725
// (the scalar deleting destructor)

// FUNCTION: EXE 0x44e741
// Destroys the naval transport sub-group (base members only)
EECPASGSeaTransport::~EECPASGSeaTransport()
{
}

// FUNCTION: EXE 0x44e76a
// Lets go of the land group it carries, then shuts the sub-group down
void EECPASGSeaTransport::Shutdown()
{
    EECPASGTransport::ReleaseLand();
    EECPAttackSubGroup::Shutdown();
}

// FUNCTION: EXE 0x44e781
// Rebuilds the formation with 2-tile spacing (forced moves allowed) and places it at the location, facing the
// heading from the sub-group's position
void EECPASGSeaTransport::PlaceFormation(const EECPILocation& inLocation)
{
    EETileLocation theTile;
    this->FindPosition(theTile, true, true);
    float theDistance = EEGetSeaHeading(theTile, inLocation.mTile);
    this->mFormation->Build();
    this->mFormation->mNoForcedMove = false;
    for (long i = this->mFormation->GetNumNodes() - 1; i >= 0; i--)
        this->mFormation->mNodes[i]->mSpacing = 2.0f;
    this->mFormation->Place(inLocation, theDistance);
}

// FUNCTION: EXE 0x44e801
// Sends the transports back to the rally point, lets go of any land group, then asks for removal
void EECPASGSeaTransport::RequestRemoval()
{
    this->ReturnToRallyPoint();
    if (this->mLand != NULL) {
        EECPASGTransport::ReleaseLand();
    }
    EECPAttackSubGroup::RequestRemoval();
}

// FUNCTION: EXE 0x44e829
// Switches state: idle gets a task, remove/move/load/scatter run their handlers, carrying puts the land group in
// transit, release drops the land group and makes a return task to a fresh rally point; others go to the sea class
void EECPASGSeaTransport::SetState(long inState, long inTime)
{
    unsigned long theOldState = this->mState;
    this->mState = inState;
    switch (this->mState) {
    case kASGStateIdle:
        if (this->mUnits.size() > 0 && !this->mTask.mIsActive)
            this->mGroup->AssignTask(this);
        break;
    case kASGStateRemove:
        this->RequestRemoval();
        break;
    case kASGStateMove:
        this->mNextWaypoint = this->mTask.mLocation;
        this->MoveToDestination();
        break;
    case kASGStateLoad:
        this->LoadCargo();
        break;
    case kASGStateCarryCargo:
        if (this->mLand != NULL && this->mLand->mState != kASGStateInTransit)
            this->mLand->SetState(kASGStateInTransit, inTime);
        break;
    case kASGStateReleaseCargo:
        if (this->mLand != NULL) {
            if (this->mLand->mTransport == this)
                this->mLand->ReleaseTransport();
            EECPASGTransport::ReleaseLand();
        }
        this->GetRallyPoint(false);
        this->ReturnToRallyPoint();
        this->mTask.mLocation = this->mRallyPoint;
        this->mTask.mType = kAGTaskReturn;
        this->mTask.mIsActive = true;
        break;
    case kASGStateRedirectCargo:
        break;
    case kASGStateScatter:
        this->ScatterDestination();
        break;
    default:
        this->mState = theOldState;
        EECPASGSea::SetState(inState, inTime);
        break;
    }
}

// FUNCTION: EXE 0x44e9bb
// Runs the transport state machine every 2 s (5 s after a scatter): wait, gather, move, load (scatter if
// blocked), carry until unloaded, release, then remove or idle; task-valid is tested twice when moving
void EECPASGSeaTransport::Update(long inTime)
{
    long theDelay = 2000;
    if (this->mNextScanTime <= inTime && this->mUnits.size() > 0)
        this->ScanTargets(1);
    this->UpdateUnits(inTime);
    bool theFailed = false;
    switch (this->mState) {
    case kASGStateIdle:
        theFailed = !this->TestCondition(kASGConditionHasUnits, inTime);
        if (theFailed)
            this->SetState(kASGStateWaitForUnits, inTime);
        if (!theFailed) {
            if (this->TestCondition(kASGConditionShouldReturn, inTime))
                this->SetState(kASGStateRemove, inTime);
            else if (this->TestCondition(kASGConditionTaskValid, inTime))
                this->SetState(kASGStateMove, inTime);
            else
                this->SetState(kASGStateIdle, inTime);
        }
        break;
    case kASGStateWaitForUnits:
        if (this->TestCondition(kASGConditionRequestEmpty, inTime))
            this->SetState(kASGStateRemove, inTime);
        else if (this->TestCondition(kASGConditionRequestFilled, inTime))
            this->SetState(kASGStateGather, inTime);
        break;
    case kASGStateGather:
        if (!this->TestCondition(kASGConditionHasUnits, inTime))
            this->SetState(kASGStateReturnToRallyPoint, inTime);
        else
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateRemove:
    case kASGStateReturnToRallyPoint:
        break;
    case kASGStateMove:
        theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
        if (theFailed)
            this->SetState(kASGStateRemove, inTime);
        if (!theFailed && this->TestCondition(kASGConditionTaskValid, inTime))
            this->SetState(kASGStateLoad, inTime);
        break;
    case kASGStateLoad:
        theFailed = !this->TestCondition(kASGConditionHasCargo, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed) {
            if (this->TestCondition(kASGConditionAllAboard, inTime))
                this->SetState(kASGStateCarryCargo, inTime);
            else if (this->TestCondition(kASGConditionLoadingBlocked, inTime)) {
                this->SetState(kASGStateScatter, inTime);
                theDelay = 5000;
            }
        }
        break;
    case kASGStateCarryCargo:
    case kASGStateRedirectCargo:
        theFailed = !this->TestCondition(kASGConditionHasCargo, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed && this->TestCondition(kASGConditionAllUnloaded, inTime))
            this->SetState(kASGStateReleaseCargo, inTime);
        break;
    case kASGStateReleaseCargo:
        if (this->TestCondition(kASGConditionShouldReturn, inTime))
            this->SetState(kASGStateRemove, inTime);
        else
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateScatter:
        if (!this->TestCondition(kASGConditionHasCargo, inTime))
            this->SetState(kASGStateIdle, inTime);
        else
            this->SetState(kASGStateLoad, inTime);
        break;
    default:
        this->SetState(kASGStateIdle, inTime);
        break;
    }
    this->mNextUpdateTime = inTime + theDelay;
}

// FUNCTION: EXE 0x44ec4b
// Gives one unit a move goal to the location's tile (its area if off the map); inFlag forces it; frees the goal
// if the unit refuses it
void EECPASGSeaTransport::SendUnitTo(EECPAttackSubGroupUnit* inUnit, const EECPILocation& inLocation, bool inFlag)
{
    GE2DPoint thePoint(0.0f, 0.0f);
    EECPILocation theLocation(inLocation);
    if (gWorld.mMap->GetTileAtLocation(theLocation.mTile) == NULL)
        theLocation.ResetToArea();
    if (inUnit == NULL || gWorld.mMap->GetTileAtLocation(theLocation.mTile) == NULL)
        return;
    thePoint.SetX(theLocation.mTile.mX);
    thePoint.SetY(theLocation.mTile.mY);
    EEUnitGoal* theGoal = new EEUGLocation(thePoint, true, inFlag, false, 0.0f, false, false);
    if (!this->GiveGoal(inUnit->GetUnit(), theGoal, inFlag, kActionPrepareToMove) && theGoal != NULL)
        delete theGoal;
}

// FUNCTION: EXE 0x44ed08
// Returns true if a ready ship waiting to load has an adjacent passenger that can stand on neither tile one turn
// left or right of the direction towards it
bool EECPASGSeaTransport::IsLoadingBlocked()
{
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    bool theResult = false;
    for (long i = this->mUnits.size() - 1; i >= 0 && !theResult; i--) {
        EEComplexUnit* theUnit = this->mUnits[i]->GetUnit();
        if (this->mUnits[i]->IsDone() || theUnit->mIsDisabled || theUnit->mHitPoints <= 0 || theUnit->mWaypoints.size() != 0)
            continue;
        EEUnitGoal* theGoal = theUnit->mActiveGoal;
        if (theGoal == NULL || theGoal->mKind != kGoalLoadTransport)
            continue;
        EEUGLoadContainer* theLoad = static_cast<EEUGLoadContainer*>(theGoal);
        for (long j = theLoad->mUnits.mItems.size() - 1; j >= 0 && !theResult; j--) {
            EEComplexUnit* theOther = theLoad->mUnits.mItems[j];
            if (theOther == NULL || theOther->mTileX == -10000 || theOther->mIsDisabled || theOther->mHitPoints <= 0)
                continue;
            if (UMAbs(theUnit->mTileX - theOther->mTileX) > 1 || UMAbs(theUnit->mTileY - theOther->mTileY) > 1)
                continue;
            long theDirection = EEGetDirectionTo(theUnit, theOther);
            EEGetNextTile(theUnit->GetTile(), theTile, EETurnDirection(theDirection, 1, true), 1);
            if (EECanStandOn(theTile, theOther))
                continue;
            EEGetNextTile(theUnit->GetTile(), theTile, EETurnDirection(theDirection, 1, false), 1);
            theResult = !EECanStandOn(theTile, theOther);
        }
    }
    return theResult;
}

// TestCondition (ICF-folded into EECPASGSpaceTransport's copy 0x450fd9): request filled; task valid or has cargo
// = the land group has units; all aboard, all unloaded, loading blocked; the rest go to the sea class
bool EECPASGSeaTransport::TestCondition(long inState, long inTime)
{
    bool theResult = false;
    switch (inState) {
    case kASGConditionRequestFilled:
        theResult = this->IsRequestFilled();
        break;
    case kASGConditionTaskValid:
    case kASGConditionHasCargo:
        theResult = this->mLand != NULL && this->mLand->GetNumUnits() > 0;
        break;
    case kASGConditionTaskInvalid:
        theResult = this->mLand == NULL;
        break;
    case kASGConditionShouldReturn:
        theResult = this->mLand == NULL || this->mUnits.size() == 0 || (this->mTask.mIsActive && this->mTask.mType == kAGTaskReturn);
        break;
    case kASGConditionAllAboard:
        theResult = this->mLand != NULL && this->mLand->CountUnitsAboard() == this->mLand->GetNumUnits();
        break;
    case 4001:
    case kASGConditionAnyAboard:
        break;
    case kASGConditionAllUnloaded:
        theResult = this->mLand == NULL || (this->mLand->CountUnitsAboard() == 0 && this->mCargoStage == 2);
        break;
    case kASGConditionLoadingBlocked:
        theResult = this->IsLoadingBlocked();
        break;
    default:
        theResult = EECPASGSea::TestCondition(inState, inTime);
        break;
    }
    return theResult;
}

// GetRallyPoint: moves the rally point to where a ship would dock at one of the player's docks (picked after a
// shuffle); returns it. ICF-folded into EECPASGSpaceTransport's 0x450af9 (the same body; both vtables' +0x88
// slot); this unit's copy of the Fn4EEF9<vector<EEUnit*> > COMDAT it instantiates is the one kept
EETileLocation EECPASGSeaTransport::GetRallyPoint(bool inIncludeAllies)
{
    std::vector<EEUnit*> theUnits;
    if (this->mUnits.empty())
        return this->mRallyPoint.mTile;
    EEGetPlayerUnitsOfType(kUnitTypeDock, this->mPlayer2->GetPlayerID(), theUnits);
    if (theUnits.size() > 0) {
        GETransformation theTransformation;
        GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
        Fn4EEF9(theUnits, UMFloatToLong(gPhysics->mRandom.GenerateUnit()));
        if (EEGetDockShipFacing(&theLocation, this->mUnits.front()->GetUnit()->GetUnitClass(),
                      theUnits.front(), false, &theTransformation)) {
            this->mRallyPoint.mTile = EETileLocation(UMFloor(theTransformation.GetX()), UMFloor(theTransformation.GetY()));
        }
    }
    return this->mRallyPoint.mTile;
}

// Fn4EEF9<vector<EEUnit*> > (ee/EEUnitUtilities.h; ICF-shared by AirTransport's EEPlayer*/EETown* calls 0x4432a/0x44392)
// TEMPLATE: EXE 0x44eef9

// SYNTHETIC: EXE 0x44f0bb
// (the scalar deleting destructor)

// SYNTHETIC: EXE 0x44f0c6
// (the this-adjusting thunk into Load (sub ecx, 0x1e8; jmp), compiler-generated for the EECPASGTransport base)

// FUNCTION: EXE 0x44f0d1
// Creates a spaceship attack sub-group belonging to the given attack group
EECPASGSpaceship::EECPASGSpaceship(EECPAttackGroup* inGroup, long inKind) : EECPAttackSubGroup(inGroup, inKind)
{
}

// SYNTHETIC: EXE 0x44f0ed
// (the scalar deleting destructor)

// FUNCTION: EXE 0x44f109
// Destroys the spaceship sub-group (base members only)
EECPASGSpaceship::~EECPASGSpaceship()
{
}

// FUNCTION: EXE 0x44f114
// Finds the sub-group's position the base way
bool EECPASGSeaTransport::FindPosition(EETileLocation& outTile, bool inArg2, bool inForce)
{
    return EECPAttackSubGroup::FindPosition(outTile, inArg2, inForce);
}

// FUNCTION: EXE 0x44f128
// Returns the number of normal plus siege targets
long EECPASGSeaTransport::GetNumTargets()
{
    return this->mTargets.size() + this->mSiegeTargets.size();
}

// FUNCTION: EXE 0x44f153
// Takes the requested units as the base does; if they sit in more than one space region, orders the attack group
// to split the sub-group; then refreshes the units and returns the base's result
bool EECPASGSpaceship::TakeRequestedUnits()
{
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*> theUnits;
    std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>::iterator theIter;
    EECPAGOrder theOrder;
    bool theResult = EECPAttackSubGroup::TakeRequestedUnits();
    if (theResult && this->mUnits.size() > 1) {
        this->GroupUnitsByWaterArea(theUnits, 3, true);
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

// FUNCTION: EXE 0x44f236
// Picks units to split off: the largest group when units sit in several space regions, else the first half
void EECPASGSpaceship::PickSplitUnits(std::vector<EEComplexUnit*>& outUnits)
{
    typedef std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*> ContinentMap;
    ContinentMap theContinents;
    ContinentMap::iterator theIter;
    unsigned long theHalf = this->GetNumUnits() >> 1;
    if (this->mUnits.size() > 1) {
        this->GroupUnitsByWaterArea(theContinents, 3, true);
        if (theContinents.size() > 1) {
            std::vector<EECPAttackSubGroupUnit*>* theLargest = NULL;
            for (theIter = theContinents.begin(); theIter != theContinents.end(); theIter++) {
                if (theLargest == NULL || theLargest->size() < theIter->second->size())
                    theLargest = theIter->second;
            }
            if (theLargest != NULL) {
                for (long i = theLargest->size() - 1; i >= 0; i--) {
                    EEComplexUnit* theUnit = ((*theLargest)[i])->GetUnit();
                    outUnits.push_back(theUnit);
                }
                goto DeleteContinents;
            }
        }
    }
    if (this->GetNumUnits() > 1) {
        // (the units used to be ranked by AI kind here into a map that was never read)
        if (outUnits.size() == 0 && this->GetNumUnits() > theHalf) {
            for (long i = theHalf - 1; i >= 0; i--) {
                EEComplexUnit* theUnit = this->GetUnitUnit(i);
                if (theUnit != NULL)
                    outUnits.push_back(theUnit);
            }
        }
    }
DeleteContinents:
    for (theIter = theContinents.begin(); theIter != theContinents.end(); theIter++)
        delete theIter->second;
}

// FUNCTION: EXE 0x44f444
// Collects the units in the bounds that pass the spaceship target filter (enemies optional): walls become siege
// targets; towers, gates and kind-4 tech-tree buildings are both normal and siege targets
void EECPASGSpaceship::CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies)
{
    std::vector<EEComplexUnit*> theUnits;
    EECPASpaceshipTargetFilter theFilter;
    theFilter.mGroup = this;
    theFilter.mPlayer = this->mPlayer2;
    theFilter.mPlayerID = this->mTask.GetPlayerID();
    theFilter.mIncludeEnemies = inIncludeEnemies;
    theFilter.mAllowShore = this->mCanAttackShore;
    if (EECollectASGTargets(*inBounds, theUnits, theFilter, true) != 0) {
        this->mTargets.reserve(theUnits.size());
        for (long i = theUnits.size() - 1; i >= 0; --i) {
            EECPASTargetUnit* theTarget = EECPAttackSubGroup::NewTarget(theUnits[i]);
            EEUnitClass* theClass = theUnits[i]->GetUnitClass();
            if (theClass->mIsWall && !theClass->mIsTower) {
                this->mSiegeTargets.push_back(theTarget);
            } else {
                this->mTargets.push_back(theTarget);
                if (theClass->mIsTower || theClass->GetCategory() == kUnitCategoryGate ||
                    (theTarget->mUnit->GetTechTreeNode() != NULL &&
                     theTarget->mUnit->GetTechTreeNode()->mData->mKind == 4)) {
                    theTarget = EECPAttackSubGroup::NewTarget(theUnits[i]);
                    this->mSiegeTargets.push_back(theTarget);
                }
            }
        }
    }
}

// FUNCTION: EXE 0x44f5b7
// Returns true if the sub-group is empty or the location's water body id equals the first unit's space region id
// (an original bug: the two ids come from different maps)
bool EECPASGSpaceship::CanReach(const EECPILocation* inLocation)
{
    bool theResult = true;
    if (this->mUnits.size() > 0) {
        EEComplexUnit* theUnit = (*this->mUnits.begin())->GetUnit();
        theResult = gContinentManager.GetContinent(inLocation->mTile, kAIPathWaterBody) == gContinentManager.GetContinent(theUnit, kAIPathSpaceRegion);
    }
    return theResult;
}

// FUNCTION: EXE 0x44f603
// Switches state: idle gets a new task, split asks to split, move heads for the task, hold/move-to-task/follow run
// their handlers; other states go to the base class
void EECPASGSpaceship::SetState(long inState, long inTime)
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

// FUNCTION: EXE 0x44f70e
// Runs the state machine each tick: scans or drops targets, updates units, then moves between the idle, wait,
// gather, split, attack, hold, follow, return and hold-territory states as their conditions change
void EECPASGSpaceship::Update(long inTime)
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

// FUNCTION: EXE 0x44fabf
// Returns true if the task is active; a location task must also lie in a space continent
bool EECPASGSpaceship::IsTaskActive(const EECPAGTask& inTask)
{
    bool theResult = inTask.mIsActive;
    if (theResult == true && inTask.mType == kAGTaskLocation) {
        long theY = inTask.mLocation.mTile.mY;
        long theX = inTask.mLocation.mTile.mX;
        theResult = false;
        EEAIContinent* theContinent = gContinentManager.GetContinentAt(theX, theY);
        if (theContinent != NULL)
            theResult = theContinent->mCategory == kContinentCategorySpace;
    }
    return theResult;
}

// FUNCTION: EXE 0x44fafa
// Requests a route between two tiles from the path manager, walls and gates allowed, with the space route planner
void EECPASGSpaceship::RequestPath(void* inFrom, void* inTo)
{
    EECPASGWeights theWeights;
    theWeights.Fill(0.0f);
    this->GetPathCosts(inFrom, inTo, theWeights);
    this->mFlags.mFlags[kAITileWall] = true;
    this->mFlags.mFlags[kAITileGate] = true;
    this->mPath.clear();
    this->CancelPath();
    EEAIGPGModifierSpaceRoutePlanner* theModifier = new EEAIGPGModifierSpaceRoutePlanner(this->mPlayer->GetPlayerID(), this, theWeights);
    // The arguments point at tile coordinates; the graph modifier is passed as an opaque handle
    this->mPathRequest = gPathManager.RequestPath(this->mPlayer->GetPlayerID(),
        *static_cast<EETileCoordinate*>(inFrom), *static_cast<EETileCoordinate*>(inTo),
        &this->mFlags, 2, -1, theModifier, 0);
    ++this->mPlayer->mCombatManager->mAttackCommander->mUpdatesThisTick;
    theModifier->Dereference();
}

// FUNCTION: EXE 0x44fbb8
// Collects the units in the bounds that pass the spaceship target filter, dropping duplicates when a
// multi-tile unit was accepted and uniqueness was asked for; returns the count
// EECollectASGTargets<EECPASpaceshipTargetFilter>, emitted by EECPASGSpaceship::CollectTargets above.

// ICF-FOLDED: EXE 0x67c0b4
// Returns false
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGSpaceship::IsAwayFromTowns()
{
    return false;
}

// ICF-FOLDED: EXE 0x67c0b4
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGSeaTransport::DecideSplit()
{
    return false;
}

// ICF-FOLDED: EXE 0x67c0b4
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGSpaceship::DecideSplit()
{
    return false;
}

// ICF-FOLDED: EXE 0x4e123d
// Does nothing for spaceship sub-groups
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EECPASGSpaceship::MoveToNearestTown()
{
}

// ICF-FOLDED: EXE 0x4ac98e
// Does nothing: transports never attack
// (no address of its own: folded into the shared `ret 0xc` body the vtable slot points at)
void EECPASGSeaTransport::AttackWith(EECPAttackSubGroupUnit*,EEComplexUnit*,EETileCoordinate*)
{
}

// ICF-FOLDED: EXE 0x44e125
// Loads the sub-group from a saved game (base data only); returns the base's error code
// (no address of its own: the same body as EECPASGSea::Load, which ICF kept; [sib 1])
long EECPASGSpaceship::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return EECPAttackSubGroup::Load(inMajorVersion, inMinorVersion, inFile);
}

// ICF-FOLDED: EXE 0x44e203
// Tests a condition: has-location-task is true for an active location, type-10 or wonder-defence task; others go
// to the base
// (no address of its own: the same body as EECPASGSea::TestCondition, which ICF kept; [sib 3])
bool EECPASGSpaceship::TestCondition(long inState, long inTime)
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

// ICF-FOLDED: EXE 0x450fa8
// Writes the sub-group and then its transport data to the saved game; returns the first error
// (no address of its own: the same body as EECPASGSpaceTransport::Save, which ICF kept; [sib 10])
long EECPASGSeaTransport::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EECPAttackSubGroup::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = EECPASGTransport::Save(inMajorVersion, inMinorVersion, outFile);
    return theResult;
}

// ICF-FOLDED: EXE 0x444b05
// Sets the land group the transports carry
// (no address of its own: the same body as EECPASGAirTransport::SetLand, which ICF kept; [sib 11])
void EECPASGSeaTransport::SetLand(EECPASGLand* inLand)
{
    EECPASGTransport::SetLand(inLand);
}

// ICF-FOLDED: EXE 0x444188
// Counts the units aboard all the sub-group's ships
// (no address of its own: the same body as EECPASGAirTransport::Virtual04, which ICF kept; [sib 13])
long EECPASGSeaTransport::CountCargo()
{
    long theResult = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        theResult += this->mUnits[i]->mUnit->GetNumCargoUnits();
    }
    return theResult;
}

// ICF-FOLDED: EXE 0x450ab8
// Returns the total transport capacity of the sub-group's ships
// (no address of its own: the same body as EECPASGSpaceTransport::Virtual08, which ICF kept; [sib 17])
long EECPASGSeaTransport::GetCargoCapacity()
{
    long theSum = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--)
        theSum += this->mUnits[i]->GetUnit()->GetUnitClass()->mTransportCapacity;
    return theSum;
}

// ICF-FOLDED: EXE 0x45110d
// Removes the unit the base way; while loading, restarts loading once for every ship still in service (not while
// the world clears or a game loads); returns whether it was removed
// (no address of its own: the same body as EECPASGSpaceTransport::RemoveUnit, which ICF kept; [sib 26])
bool EECPASGSeaTransport::RemoveUnit(EEComplexUnit* inUnit)
{
    bool theResult = EECPAttackSubGroup::RemoveUnit(inUnit);
    if (theResult && this->mCargoStage == 1 && gPhysics != NULL) {
        for (unsigned long i = 0; i < this->mUnits.size(); i++) {
            if (!this->mUnits[i]->IsDone() && !gWorldClearing && !gLoadingSavedGame) {
                this->StartLoading();   // once, not once per remaining unit
                break;
            }
        }
    }
    return theResult;
}

// ICF-FOLDED: EXE 0x441a2e
// Sets the sub-group's task and flags every unit as needing new orders
// (no address of its own: the same body as EECPASGAir::Virtual14, which ICF kept; [sib 27])
void EECPASGSeaTransport::SetTask(const EECPAGTask& inTask)
{
    EECPAttackSubGroup::SetTask(inTask);
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        this->mUnits[i]->mNeedsOrders = true;
    }
}

// ICF-FOLDED: EXE 0x444a58
// Switches to the load state if there are ships and the land group still has units not aboard
// (no address of its own: the same body as EECPASGAirTransport::Virtual14, which ICF kept; [sib 28])
void EECPASGSeaTransport::StartLoading()
{
    std::vector<EEComplexUnit*> theUnits;
    if (this->mLand == NULL || this->mUnits.size() <= 0)
        return;
    this->mLand->GetUnitsNotAboard(theUnits);
    if (theUnits.size() > 0)
        this->SetState(kASGStateLoad, gPhysics->mGameTime);
}

// ICF-FOLDED: EXE 0x44dd9f
// Rebuilds the formation with 2-tile spacing and no forced moves and places it at the location, facing the
// heading from the sub-group's position
// (no address of its own: the same body as EECPASGSea::PlaceFormation, which ICF kept; [sib 33])
void EECPASGSpaceship::PlaceFormation(const EECPILocation& inLocation)
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

// ICF-FOLDED: EXE 0x450e95
// Places the formation at the next waypoint (the task location if unset, the rally point if off the map),
// snapped to the closest map tile
// (no address of its own: the same body as EECPASGSpaceTransport::MoveToDestination, which ICF kept; [sib 38])
void EECPASGSeaTransport::MoveToDestination()
{
    EECPILocation theLocation;
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    if (this->mNextWaypoint.mTile.mX == 0 && this->mNextWaypoint.mTile.mY == 0)
        this->mNextWaypoint = this->mTask.mLocation;
    if (gWorld.mMap->GetTileAtLocation(this->mNextWaypoint.mTile) == NULL)
        this->mNextWaypoint.mTile = this->mRallyPoint.mTile;
    thePoint.mX = this->mNextWaypoint.mTile.mX;
    thePoint.mY = this->mNextWaypoint.mTile.mY;
    if (gWorld.mMap->GetTile(this->mNextWaypoint.mTile.mX, this->mNextWaypoint.mTile.mY) == NULL) {
        long theX;
        long theY;
        gWorld.mMap->GetClosestTile(&thePoint, &theX, &theY);
        if (gWorld.mMap->GetTile(theX, theY) != NULL) {
            thePoint.mX = theX;
            thePoint.mY = theY;
        }
    }
    theLocation.mTile.mX = (long)thePoint.mX;
    theLocation.mTile.mY = (long)thePoint.mY;
    this->PlaceFormation(theLocation);
}

// ICF-FOLDED: EXE 0x44e240
// Returns true while the task location still has influence for the task's player
// (no address of its own: the same body as EECPASGSea::IsTerritoryTaskValid, which ICF kept; [sib 41])
bool EECPASGSpaceship::IsTerritoryTaskValid(long inTime)
{
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    return theTable->GetTileValue(this->mTask.GetPlayerID(), this->mTask.mLocation.mTile) ? true : false;
}

// ICF-FOLDED: EXE 0x44e26a
// Re-checks the task at random 5-35 s intervals: a location attack stays valid while enemies have influence within
// 2 regions, a wonder defence while the wonder lives; defence or modeless tasks lapse with a 65% chance
// (no address of its own: the same body as EECPASGSea::IsLocationTaskValid, which ICF kept; [sib 42])
bool EECPASGSpaceship::IsLocationTaskValid(long inTime)
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

// ICF-FOLDED: EXE 0x44e42f
// Returns true while the task unit lives and either the task is a wonder task or the next follow time has passed
// (no address of its own: the same body as EECPASGSea::IsUnitTaskValid, which ICF kept; [sib 43])
bool EECPASGSpaceship::IsUnitTaskValid(long inTime)
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

// ICF-FOLDED: EXE 0x44f114
// (no address of its own: the same body as EECPASGSeaTransport::FindPosition, which ICF kept; [sib 50])
bool EECPASGSpaceship::FindPosition(EETileLocation& outTile, bool inArg2, bool inForce)
{
    return EECPAttackSubGroup::FindPosition(outTile, inArg2, inForce);
}

// ICF-FOLDED: EXE 0x4509f2
// Returns the share of ships in danger, at least 0.75 if an endangered ship carries cargo; 0 while redirecting
// (no address of its own: the same body as EECPASGSpaceTransport::GetDangerRatio, which ICF kept; [sib 52])
float EECPASGSeaTransport::GetDangerRatio(long inArg1)
{
    float theResult = 0.0f;
    long theCount = 0;
    long theCargo = 0;
    long theTotalCargo = 0;
    if (this->mState != kASGStateRedirectCargo) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            // The original (0x450a3f, 0x450a44) also sums the total cargo, never used, before testing each unit
            theTotalCargo += this->mUnits[i]->GetUnit()->GetNumCargoUnits();
            if (this->mUnits[i]->IsInDanger()) {
                theCount++;
                theCargo += this->mUnits[i]->GetUnit()->GetNumCargoUnits();
            }
        }
        if (theCount > 0) {
            theResult = (float)theCount / this->mUnits.size();
            if (theCargo > 0 && 0.75f > theResult)
                theResult = 0.75f;
        }
    }
    return theResult;
}

// ICF-FOLDED: EXE 0x44f128
// (no address of its own: the same body as EECPASGSeaTransport::GetNumTargets, which ICF kept; [sib 55])
long EECPASGSpaceship::GetNumTargets()
{
    return this->mTargets.size() + this->mSiegeTargets.size();
}

// ICF-FOLDED: EXE 0x44e139
// Sets the destination the base way
// (no address of its own: the same body as EECPASGSea::SetDestination, which ICF kept; [sib 62])
void EECPASGSpaceship::SetDestination(const EECPILocation& inLocation)
{
    EECPAttackSubGroup::SetDestination(inLocation);
}
