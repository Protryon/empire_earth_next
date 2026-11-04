// C:\EEX\Empire Earth\AI Thread\EECPASGAir.cpp (file name inferred: no __FILE__). 0x4192c..0x440cd (unit map
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
#include "ee/EEUnitVectorItem.h"
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
#include "ee/EETown.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPPersonnel.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPASGAir.h"
#include "ee/EECPIntelligence.h"

// Genuine cdecl factory also declared in EECPAttackSubGroup.cpp.
EECPAttackSubGroupUnit* EECPCreateSubGroupUnit(EEComplexUnit*, EECPAttackSubGroup*, bool);
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGLocation.h"

#include "ee/EECPAAirTargetFilters.h"
#include "ee/EECPAreaTargetCollector.h"

#include "ee/EEPathManager.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "ee/EECPCombatManager.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_DEBUG)

// FUNCTION: EXE 0x44192c
// Creates an air attack sub-group belonging to the given attack group
EECPASGAir::EECPASGAir(EECPAttackGroup* inGroup, long inKind) : EECPAttackSubGroup(inGroup, inKind)
{
}

// FUNCTION: EXE 0x441948
// Returns the continent-guard share, fixed at 1.0 for air sub-groups
float EECPASGAir::GetGuardContinentPercentage()
{
    return 1.0f;
}

// SYNTHETIC: EXE 0x44194f
// (the scalar deleting destructor)

// FUNCTION: EXE 0x44196b
// Destroys the air sub-group (base members only)
EECPASGAir::~EECPASGAir()
{
}

// FUNCTION: EXE 0x441976
// Replaces the sub-group's personnel request with a town request (at the town's centre) or a continent request
// for the given classes; during a wonder race the request expires after 300 s
void EECPASGAir::RequestUnits(std::vector<long>& inClasses, std::vector<long>& inUnitIDs, long inContinent, long inTownID)
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

// FUNCTION: EXE 0x441a2e
// Sets the sub-group's task and flags every unit as needing new orders
void EECPASGAir::SetTask(const EECPAGTask& inTask)
{
    EECPAttackSubGroup::SetTask(inTask);
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        this->mUnits[i]->mNeedsOrders = true;
    }
}

// FUNCTION: EXE 0x441a66
// Orders a unit to attack the target (or the unit holding it): a parked plane gets an attack goal; otherwise,
// unless it is taking off or landing, fighters fly to the target's tile and other units get an attack goal
void EECPASGAir::AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile)
{
    GE2DPoint thePoint(0.0f, 0.0f);
    EEComplexUnit* theTarget =
        inTarget->mContainer == NULL ? inTarget : inTarget->mContainer;
    bool theOnMap = gWorld.mMap->GetTile(inUnit->GetUnit()->mTileX, inUnit->GetUnit()->mTileY) != NULL;
    bool theAircraft = inUnit->GetUnit()->mUnitClass->mCategory == kUnitCategoryAirplane;
    bool theCanAct = !(theAircraft && !theOnMap) &&
        (inUnit->GetUnit()->mVelocity != 0.0f ?
          inUnit->GetUnit()->mPhysicalModel.GetLocation().GetZ() == inUnit->GetUnit()->mTile->GetHeight() : true);
    if (inUnit->IsDone())
        return;
    inUnit->SetTarget(theTarget);
    EEUnitGoal* theGoal;
    long theAction;
    if (theAircraft && !theOnMap) {
        inUnit->mNeedsOrders = true;
        inUnit->GetUnit()->StartAction(kActionInitialAttackState);
        theGoal = new EEUGAttackUnit(inUnit->GetUnit(), theTarget, true, false);
        theAction = inUnit->GetUnit()->GetCommandType();
        if (!this->GiveGoal(inUnit->GetUnit(), theGoal, 0, theAction) && theGoal != NULL)
            delete theGoal;
    } else {
        if ((inUnit->GetUnit()->GetCommandType() == kActionTakeoff || inUnit->GetUnit()->GetCommandType() == kActionTaxi ||
             inUnit->GetUnit()->GetCommandType() == kActionContinueToTaxi || inUnit->GetUnit()->GetCommandType() == kActionDescend ||
             inUnit->GetUnit()->GetCommandType() == kActionContinueToDescend || inUnit->GetUnit()->GetCommandType() == kActionBrake ||
             inUnit->GetUnit()->GetCommandType() == kActionContinueToBrake ||
             (inUnit->GetUnit()->mStats->mFlightTime != 0 && inUnit->GetUnit()->mFlightTime == 0)) && !theCanAct)
            return;
        inUnit->mNeedsOrders = false;
        long theKind = inUnit->GetUnit()->mUnitClass->mAIKind;
        if (theKind == kAIKindFighter || theKind == 23) {
            thePoint.SetX(theTarget->mTileX);
            thePoint.SetY(theTarget->mTileY);
            theGoal = new EEUGLocation(thePoint, true, false, false, 0.0f, false, false);
        } else
            theGoal = new EEUGAttackUnit(inUnit->GetUnit(), theTarget, true, false);
        theAction = theAircraft && theCanAct ? 1000 : 8000;
        if (!this->GiveGoal(inUnit->GetUnit(), theGoal, 0, theAction) && theGoal != NULL)
            delete theGoal;
    }
}

// FUNCTION: EXE 0x441ce6
// Moves the rally point to a position found for the sub-group; always returns true
bool EECPASGAir::RegroupHere()
{
    EETileLocation theTile;
    this->FindPosition(theTile, true, false);
    this->mRallyPoint.mTile = theTile;
    return true;
}

// FUNCTION: EXE 0x441d18
// Fills the air route cost weights: all zero except weight 10, which is 1
void EECPASGAir::GetPathCosts(void* inFrom, void* inTo, EECPASGWeights& outWeights)
{
    outWeights.Fill(0.0f);
    outWeights.mValues[10] = 1.0f;
}

// FUNCTION: EXE 0x441d2f
// Clears every unit's goals and all goal queues of the airport or carrier holding it, then disbands the sub-group
void EECPASGAir::Disband()
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        this->mUnits[i]->GetUnit()->ClearGoalQueue(0);
        EEComplexUnit* theHolder = this->mUnits[i]->GetUnit()->mHomeBase;
        if (theHolder == NULL)
            theHolder = this->mUnits[i]->GetUnit()->mContainer;
        if (theHolder != NULL) {
            for (long j = theHolder->GetNumGoalQueues() - 1; j >= 0; j--)
                theHolder->ClearGoalQueue(j);
        }
    }
    EECPAttackSubGroup::Disband();
}

// FUNCTION: EXE 0x441da1
// Returns the number of goal queues the unit has
unsigned long EEComplexUnit::GetNumGoalQueues()
{
    return this->mGoalQueues.size();
}

// FUNCTION: EXE 0x441dbd
// Moves every unit delivered by the personnel request into the sub-group, raising the scan range to the best
// unit's; then releases the request and re-ranks and refreshes the units; always returns true
bool EECPASGAir::TakeRequestedUnits()
{
    EEPlayer* thePlayer = this->mGroup->mTask.mPlayer;
    EECPIntelligence* theIntelligence = this->mGroup->mCommander->mIntelligence;
    EECPIntelligencePlayerFile* theFile = theIntelligence->GetFile(thePlayer);
    float theValue = this->mScanRange;
    while (!this->mRequest->IsEmpty()) {
        EEComplexUnit* theUnit = this->mRequest->mUnits.back().mUnit;
        this->mRequest->PopSlot();
        if (theUnit == NULL)
            continue;
        float theUnitValue = theFile->GetScanRange(theUnit->GetLineOfSight());
        if (theUnitValue > theValue)
            theValue = theUnitValue;
        EECPAttackSubGroupUnit* theSubUnit = EECPCreateSubGroupUnit(theUnit, this, 0);
        theSubUnit->Virtual48();
        this->mUnits.push_back(theSubUnit);
    }
    this->mScanRange = theValue;
    this->RankUnits();
    this->mGroup->mCommander->mPersonnel->ReleaseRequest(this->mRequest->mID);
    this->mRequest->Dereference();
    this->mRequest = NULL;
    this->RefreshUnits();
    return true;
}

// FUNCTION: EXE 0x441eb3
// Returns true if the request holds no unit slots
bool EECPPersonnelRequest::IsEmpty()
{
    return this->mUnits.size() == 0;
}

// FUNCTION: EXE 0x441ed0
// Returns the player's air chokepoint-guard share (0 without a computer player)
float EECPASGAir::GetGuardChokepointPercentage()
{
    return this->mPlayer2 != NULL ? this->mPlayer2->mGuardChokepointAirPercentage : 0;
}

// FUNCTION: EXE 0x441ee4
// Returns the player's town-guard share (0 without a computer player)
float EECPASGAir::GetGuardTownPercentage()
{
    return this->mPlayer2 != NULL ? this->mPlayer2->mGuardTownPercentage : 0;
}

// FUNCTION: EXE 0x441ef8
// Launches the sub-group: every unit retargets now and needs orders, then all fly to the task location if active
void EECPASGAir::Launch(unsigned long inTime)
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        this->mUnits[i]->mNextRetargetTime = inTime;
        this->mUnits[i]->mNeedsOrders = true;
    }
    if (this->mTask.mIsActive)
        this->SetDestination(this->mTask.mLocation);
}

// FUNCTION: EXE 0x441f4a
// Adds every unit in the bounds that passes the air target filter (enemies optional) to the sub-group's targets
void EECPASGAir::CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies)
{
    std::vector<EEComplexUnit*> theUnits;
    EECPAAirTargetFilter theFilter;
    theFilter.mGroup = this;
    theFilter.mPlayer = this->mPlayer2;
    theFilter.mPlayerID = this->mTask.GetPlayerID();
    theFilter.mIncludeEnemies = inIncludeEnemies;
    if (EECollectASGTargets(*inBounds, theUnits, theFilter, true) != 0) {
        this->mTargets.reserve(theUnits.size());
        for (long i = theUnits.size() - 1; i >= 0; --i) {
            EECPASTargetUnit* theTarget = EECPAttackSubGroup::NewTarget(theUnits[i]);
            this->mTargets.push_back(theTarget);
        }
    }
}

// FUNCTION: EXE 0x442000
// Returns true if the sub-group is empty, the location is invalid, it has a helicopter, or some plane's fuel range
// (flight seconds x speed x 0.9) from its holder (else its home base; planes with neither are skipped) reaches
// the location
bool EECPASGAir::CanReach(const EECPILocation* inLocation)
{
    EECPILocation theLocation(*inLocation);
    bool theResult = false;
    if (this->mUnits.size() > 0 && theLocation.IsValid()) {
        for (long i = this->mUnits.size() - 1; i >= 0; --i) {
            EEComplexUnit* theUnit = this->mUnits[i]->GetUnit();
            if (theUnit->GetUnitClass()->mCategory == kUnitCategoryHelicopter) {
                theResult = true;
                break;
            }
            EEUnitClassUpgrade* theStats = theUnit->GetStats();
            long theFuel = theStats->mFlightTime;
            EEComplexUnit* theContainer = theUnit->GetContainer();
            if (theFuel <= 0 || !(theStats->GetMaxVelocity() > 0.0f))
                continue;
            // an airborne plane has no container: measure from its home base, as CountUnitsInRange does, instead
            // of reading the position through a NULL container
            if (theContainer == NULL)
                theContainer = theUnit->mHomeBase;
            if (theContainer == NULL)
                continue;
            // The original divides the flight time as an integer (0x4420bf, 0x4420d7); kept
            float theRange = ((theFuel / 1000) * theStats->GetMaxVelocity() * 0.9f) *
                             ((theFuel / 1000) * theStats->GetMaxVelocity() * 0.9f);
            // The original's jbe (0x442103) also accepts an unordered distance/range compare
            if (!(theContainer->DistanceSquaredToTileXY(theLocation.mTile.mX, theLocation.mTile.GetY()) > theRange)) {
                theResult = true;
                break;
            }
        }
    } else
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x44211d
// Returns the squared distance from the unit's position to a tile
float EEComplexUnit::DistanceSquaredToTileXY(long inTileX, long inTileY)
{
    float theDX = this->mPhysicalModel.GetLocation().GetX() - inTileX;
    float theDY = this->mPhysicalModel.GetLocation().GetY() - inTileY;
    return theDY * theDY + theDX * theDX;
}

// FUNCTION: EXE 0x442142
// Counts the units that can reach the location: units without a base or fuel limit always count, others only
// if the location lies within 90% of their fuel range from their base
// NEAR: inlining and register allocation differ from the original
long EECPASGAir::CountUnitsInRange(const EECPILocation& inLocation)
{
    long theCount = this->GetNumUnits();
    for (unsigned long i = 0; i < this->GetNumUnits(); i++) {
        EEComplexUnit* theUnit = this->GetUnitUnit(i);
        EEComplexUnit* theBase = theUnit->mContainer;
        EEUnitClassUpgrade* theStats = theUnit->mStats;
        if (theBase == NULL)
            theBase = theUnit->mHomeBase;
        if (theBase == NULL || theBase->mTileX == -10000)
            continue;
        long theFuel = theStats->mFlightTime;
        if (theFuel <= 0 || theStats->mMaxVelocity <= 0.0f)
            continue;
        // The parentheses keep VC6 from folding 0.001f * 0.9f into 0.0009f: the original multiplies in this order
        // (0x4421ca..0x4421e4), which rounds differently
        float theRange = ((theFuel * 0.001f) * theStats->mMaxVelocity * 0.9f)
                         * ((theFuel * 0.001f) * theStats->mMaxVelocity * 0.9f);
        if (theBase->DistanceSquaredToTileXY(inLocation.mTile.GetX(), inLocation.mTile.GetY()) >= theRange)
            theCount--;
    }
    return theCount;
}

// FUNCTION: EXE 0x442220
// Gives a live, enabled unit that is on the map or held by another the goal and starts the action; false otherwise
bool EECPASGAir::GiveGoal(EEComplexUnit* inUnit, EEUnitGoal* inGoal, bool inKeepCurrent, long inAction)
{
    bool theResult = false;
    if (inUnit != NULL && inUnit->mHitPoints > 0 && !inUnit->mIsDisabled
        && (inUnit->mTileX != -10000 || inUnit->mContainer != NULL)) {
        inUnit->SetGoal(inGoal, inKeepCurrent, 0);
        inUnit->StartAction(inAction);
        inUnit->mUnitClass->ActionChanged(inUnit);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x442273
// Takes any requested units; without an active task, releases all targets, sends paratrooper planes off the map
// and drops them from the sub-group, and zeroes other on-map planes' flight time unless units were just taken
void EECPASGAir::GoIdle()
{
    bool theHadRequest = this->mRequest != NULL;
    if (this->mRequest != NULL)
        this->TakeRequestedUnits();
    if (!this->mTask.mIsActive) {
        std::vector<EEComplexUnit*> theRemoved;
        long i;
        for (i = this->mUnits.size() - 1; i >= 0; i--) {
            EEComplexUnit* theUnit = this->mUnits[i]->GetUnit();
            this->mUnits[i]->ReleaseTarget();
            if (theUnit->mUnitClass->mUnitType == kUnitTypeParatrooperPlane) {
                theUnit->StartAction(kActionSendParatrooperPlaneOffMap);
                theUnit->mUnitClass->ActionChanged(theUnit);
                theRemoved.push_back(theUnit);
            } else {
                this->mUnits[i]->mNeedsOrders = true;
                if (!theHadRequest && gWorld.mMap->GetTile(theUnit->mTileX, theUnit->mTileY) != NULL)
                    theUnit->SetFlightTime(0);
            }
        }
        this->RemoveUnits(theRemoved);
    }
 EE_LOST_DEBUG(3); /* LOST-DEBUG: none: exact only (lostdebug_search, smallest n) */ }

// FUNCTION: EXE 0x4423d4
// Recalls every active unit (zeroes its flight time or clears its holder's goals), clears its goals, asks for removal
void EECPASGAir::RequestRemoval()
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->IsDone())
            continue;
        if (this->mUnits[i]->GetUnit()->mContainer == NULL)
            this->mUnits[i]->GetUnit()->SetFlightTime(0);
        else
            this->mUnits[i]->GetUnit()->mContainer->ClearGoalQueue(0);
        this->mUnits[i]->GetUnit()->ClearGoalQueue(0);
    }
    EECPAttackSubGroup::RequestRemoval();
 EE_LOST_DEBUG(1); /* LOST-DEBUG: none: exact only (lostdebug_search, smallest n) */ }

// FUNCTION: EXE 0x44249f
// Switches state: idle gets a new task and goes idle; remove, launch, hold-at-task and hold-territory (return to
// the task) run their handlers; other states go to the base class
void EECPASGAir::SetState(long inState, long inTime)
{
    unsigned long theOldState = this->mState;
    this->mState = inState;
    switch (inState) {
    case kASGStateIdle:
        if (this->mUnits.size() > 0 && !this->mTask.mIsActive)
            this->mGroup->AssignTask(this);
        this->GoIdle();
        break;
    case kASGStateWaitForUnits:
        break;
    case kASGStateRemove:
        this->RequestRemoval();
        break;
    case kASGStateLaunch:
        this->Launch(inTime);
        break;
    case kASGStateHoldAtTask:
        this->HoldAtTask(inTime);
        break;
    case kASGStateHoldTerritory:
        this->ReturnToTask(inTime);
        break;
    default:
        this->mState = theOldState;
        EECPAttackSubGroup::SetState(inState, inTime);
        break;
    }
}

// FUNCTION: EXE 0x442564
// Returns true if any unit can reach the location
bool EECPASGAir::IsInRange(const EECPILocation& inLocation)
{
    return this->CountUnitsInRange(inLocation) ? true : false;
}

// FUNCTION: EXE 0x442578
// Loads the sub-group from a saved game (base data only); returns the base's error code
long EECPASGAir::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EECPILocation theLocation;
    return EECPAttackSubGroup::Load(inMajorVersion, inMinorVersion, inFile);
}

// FUNCTION: EXE 0x44259e
// Counts the units able to attack each target and returns true if any can; with all targets and release set,
// releases the targets no unit can attack
bool EECPASGAir::MatchTargetsToUnits(bool inAllTargets, bool inAllUnits, bool inReleaseUnmatched)
{
    bool theResult = false;
    long i;
    long j;
    for (i = this->mUnits.size() - 1; i >= 0; i--)
        (this->mUnits[i])->mTargetPickResult = 0;
    for (i = this->mTargets.size() - 1; i >= 0 && (!theResult || inAllTargets); i--) {
        (this->mTargets[i])->mNumAttackers = 0;
        for (j = this->mUnits.size() - 1; j >= 0 && (!theResult || !inAllUnits); j--) {
            if ((this->mUnits[j])->CanAttack(this->mTargets[i])) {
                (this->mTargets[i])->mNumAttackers++;
                theResult = true;
            }
        }
    }
    if (!inAllTargets || !inReleaseUnmatched)
        return theResult;
    for (i = this->mTargets.size() - 1; i >= 0; i--) {
        if ((this->mTargets[i])->mNumAttackers != 0)
            continue;
        EECPASTargetUnit* theTarget = this->mTargets[i];
        this->mTargets.erase(this->mTargets.begin() + i);
        EECPAttackSubGroup::ReleaseTarget(theTarget);
    }
    return theResult;
}

// FUNCTION: EXE 0x442759
// Sends every active unit to the location (its area if off the map): parked planes prepare to move, others not
// taking off or landing get a move goal, forced for active non-wonder tasks
void EECPASGAir::SetDestination(const EECPILocation& inLocation)
{
    GE2DPoint thePoint(0.0f, 0.0f);
    EECPILocation theLocation(inLocation);
    bool theForced = this->mTask.mIsActive && this->mTask.mMode != kAGModeWonder;
    // (the tile used to be tested twice)
    if (gWorld.mMap->GetTileAtLocation(theLocation.mTile) == NULL)
        theLocation.ResetToArea();
    thePoint.SetXY(theLocation.mTile.mX, theLocation.mTile.GetY());
    if (gWorld.mMap->GetTileAtLocation(theLocation.mTile) == NULL)
        return;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->IsDone())
            continue;
        EECPAttackSubGroupUnit* theUnit = this->mUnits[i];
        bool theOnMap = gWorld.mMap->GetTile(theUnit->GetUnit()->mTileX, theUnit->GetUnit()->mTileY) != NULL;
        bool theAircraft = theUnit->GetUnit()->mUnitClass->mCategory == kUnitCategoryAirplane;
        bool theCanAct = !(theAircraft && !theOnMap) &&
            (theUnit->GetUnit()->mVelocity != 0.0f ?
              theUnit->GetUnit()->mPhysicalModel.GetLocation().GetZ() == theUnit->GetUnit()->mTile->GetHeight() : true);
        EEUnitGoal* theGoal;
        long theAction;
        if (theAircraft && !theOnMap) {
            // (started twice in a row before; the second start changed nothing)
            theUnit->GetUnit()->StartAction(kActionPrepareToMove);
            theGoal = new EEUGLocation(thePoint, true, theForced, false, 0.0f, false, false);
            theAction = theUnit->GetUnit()->GetCommandType();
            if (!this->GiveGoal(theUnit->GetUnit(), theGoal, 0, theAction) && theGoal != NULL)
                delete theGoal;
        } else {
            if (theUnit->GetUnit()->GetCommandType() == kActionTakeoff || theUnit->GetUnit()->GetCommandType() == kActionTaxi ||
                theUnit->GetUnit()->GetCommandType() == kActionContinueToTaxi || theUnit->GetUnit()->GetCommandType() == kActionDescend ||
                theUnit->GetUnit()->GetCommandType() == kActionContinueToDescend || theUnit->GetUnit()->GetCommandType() == kActionBrake ||
                theUnit->GetUnit()->GetCommandType() == kActionContinueToBrake)
                continue;
            theGoal = new EEUGLocation(thePoint, true, theForced, false, 0.0f, false, false);
            theAction = theAircraft && theCanAct ? 1000 : 8000;
            if (!this->GiveGoal(theUnit->GetUnit(), theGoal, 0, theAction) && theGoal != NULL)
                delete theGoal;
        }
    }
}

// FUNCTION: EXE 0x442a47
// Sends one unit to the centre of the location's tile (its area if off the map) like SetDestination; inFlag forces it
void EECPASGAir::SendUnitTo(EECPAttackSubGroupUnit* inUnit, const EECPILocation& inLocation, bool inFlag)
{
    GE2DPoint thePoint(0.0f, 0.0f);
    EECPILocation theLocation(inLocation);
    if (gWorld.mMap->GetTileAtLocation(theLocation.mTile) == NULL)
        theLocation.ResetToArea();
    if (inUnit == NULL || gWorld.mMap->GetTileAtLocation(theLocation.mTile) == NULL)
        return;
    thePoint.SetXY(theLocation.mTile.mX + 0.5f, theLocation.mTile.GetY() + 0.5f);
    bool theOnMap = gWorld.mMap->GetTile(inUnit->GetUnit()->mTileX, inUnit->GetUnit()->mTileY) != NULL;
    bool theAircraft = inUnit->GetUnit()->mUnitClass->mCategory == kUnitCategoryAirplane;
    bool theCanAct = !(theAircraft && !theOnMap) &&
        (inUnit->GetUnit()->mVelocity != 0.0f ?
          inUnit->GetUnit()->mPhysicalModel.GetLocation().GetZ() == inUnit->GetUnit()->mTile->GetHeight() : true);
    EEUnitGoal* theGoal;
    long theAction;
    if (theAircraft && !theOnMap) {
        // (started twice in a row before; the second start changed nothing)
        inUnit->GetUnit()->StartAction(kActionPrepareToMove);
        theGoal = new EEUGLocation(thePoint, true, inFlag, false, 0.0f, false, false);
        theAction = inUnit->GetUnit()->GetCommandType();
        if (!this->GiveGoal(inUnit->GetUnit(), theGoal, 0, theAction) && theGoal != NULL)
            delete theGoal;
    } else {
        if (inUnit->GetUnit()->GetCommandType() == kActionTakeoff || inUnit->GetUnit()->GetCommandType() == kActionTaxi ||
            inUnit->GetUnit()->GetCommandType() == kActionContinueToTaxi || inUnit->GetUnit()->GetCommandType() == kActionDescend ||
            inUnit->GetUnit()->GetCommandType() == kActionContinueToDescend || inUnit->GetUnit()->GetCommandType() == kActionBrake ||
            inUnit->GetUnit()->GetCommandType() == kActionContinueToBrake)
            return;
        theGoal = new EEUGLocation(thePoint, true, inFlag, false, 0.0f, false, false);
        theAction = theAircraft && theCanAct ? 1000 : 8000;
        if (!this->GiveGoal(inUnit->GetUnit(), theGoal, 0, theAction) && theGoal != NULL)
            delete theGoal;
    }
}

// FUNCTION: EXE 0x442cac
// Requests an air route between two tiles from the path manager, all terrain allowed, with the airplane route planner
void EECPASGAir::RequestPath(void* inFrom, void* inTo)
{
    EECPASGWeights theWeights;
    theWeights.Fill(0.0f);
    // The original's prologue (0x442cac) initialises both flag records
    EECPASGFlags theUnusedFlags;
    EECPASGFlags theFlags;
    memset(theFlags.mFlags, 1, sizeof(theFlags.mFlags));
    this->mPath.clear();
    this->CancelPath();
    this->GetPathCosts(inFrom, inTo, theWeights);
    EEAIGPGModifierAirplaneAttackSubGroupRoutePlanner* theModifier = new EEAIGPGModifierAirplaneAttackSubGroupRoutePlanner(this, theWeights);
    // The arguments point at tile coordinates; the graph modifier is passed as an opaque handle
    this->mPathRequest = gPathManager.RequestPath(this->mPlayer->GetPlayerID(),
        *static_cast<EETileCoordinate*>(inFrom), *static_cast<EETileCoordinate*>(inTo),
        &theFlags, 2, -1, theModifier, 0);
    ++this->mPlayer->mCombatManager->mAttackCommander->mUpdatesThisTick;
    if (theModifier != NULL)
        theModifier->Dereference();
}

// FUNCTION: EXE 0x442d81
// Copies the locations that have influence for the task's player and that some unit can reach
void EECPASGAir::FilterReachableLocations(const std::vector<EECPILocation>& inLocations,
                          std::vector<EECPILocation>& outLocations)
{
    EEInfluenceTable* theInfluence = gWorld.mInfluenceTable;
    outLocations.clear();
    outLocations.reserve(inLocations.size());
    for (long i = inLocations.size() - 1; i >= 0; --i) {
        if (theInfluence->GetTileValue(gWorld.mPlayers[this->mTask.GetPlayerID()]->GetPlayerID(), inLocations[i].mTile) &&
            this->IsInRange(inLocations[i]))
            outLocations.push_back(inLocations[i]);
    }
}

// FUNCTION: EXE 0x442e47
// Returns the player's influence value for the region holding the tile
long EEInfluenceTable::GetTileValue(unsigned long inPlayer, const U2DPoint<long>& inTile)
{
    U2DPoint<long> theRegion(inTile.mX / this->mRegionSize, inTile.GetY() / this->mRegionSize);
    return this->GetValue(inPlayer, theRegion);
}

// FUNCTION: EXE 0x442e77
// Re-rolls target preferences: bombers may prefer citizens (10%) or anti-air units (15-40%), fighters may prefer
// bombers (35-70%), each only if their ammo can hurt that kind; transports get none
void EECPASGAir::AssignPrejudices()
{
    std::vector<EECPAttackSubGroupUnit*> theFighters;
    std::vector<EECPAttackSubGroupUnit*> theBombers;
    EECPAttackSubGroupUnitPrejudice theAntiAir;
    theAntiAir.mUnitType = 0;
    theAntiAir.mUnitLine = kAIKindNone;
    theAntiAir.mUnitFamily = kUnitFamilyNoFamily;
    EECPAttackSubGroupUnitPrejudice theFighter;
    theFighter.mUnitType = 0;
    theFighter.mUnitLine = kAIKindNone;
    theFighter.mUnitFamily = kUnitFamilyNoFamily;
    EECPAttackSubGroupUnitPrejudice theBomber;
    theBomber.mUnitType = 0;
    theBomber.mUnitLine = kAIKindNone;
    theBomber.mUnitFamily = kUnitFamilyNoFamily;
    float theFighterChance = gPhysics->GetRandom(0.15f, 0.4f);
    float theBomberChance = gPhysics->GetRandom(0.35f, 0.7f);
    theAntiAir.mUnitFamily = kUnitFamilyCitizen;
    theFighter.mUnitLine = kAIKindAntiAir;
    theBomber.mUnitLine = kAIKindBomber;
    long i;
    for (i = this->mUnits.size() - 1; i >= 0; i--) {
        this->mUnits[i]->mPrejudices.clear();
        if (this->mUnits[i]->GetUnit()->GetUnitClass()->IsTransport())
            continue;
        if (this->mUnits[i]->mType == kASGUKindAirBomber)
            theFighters.push_back(this->mUnits[i]);
        else if (this->mUnits[i]->mType == kASGUKindAirFighter)
            theBombers.push_back(this->mUnits[i]);
    }
    for (i = theFighters.size() - 1; i >= 0; i--) {
        if (gU4E8708[36][theFighters[i]->GetUnit()->GetUnitClass()->mAmmoType] > 0.0f &&
            gPhysics->GetRandom() <= 0.1f)
            theFighters[i]->mPrejudices.push_back(theAntiAir);
        if (gU4E8708[32][theFighters[i]->GetUnit()->GetUnitClass()->mAmmoType] > 0.0f &&
            gPhysics->GetRandom() <= theFighterChance)
            theFighters[i]->mPrejudices.push_back(theFighter);
    }
    for (i = theBombers.size() - 1; i >= 0; i--) {
        if (gU4E8708[20][theBombers[i]->GetUnit()->GetUnitClass()->mAmmoType] > 0.0f &&
            gPhysics->GetRandom() <= theBomberChance)
            theBombers[i]->mPrejudices.push_back(theBomber);
    }
}

// SYNTHETIC: EXE 0x4431fe
// (the scalar deleting destructor)

// FUNCTION: EXE 0x44321b
// Tests a condition: request filled, task valid or invalid (through CheckTask), no targets; others go to the base
bool EECPASGAir::TestCondition(long inState, long inTime)
{
    bool theResult;
    switch (inState) {
    case kASGConditionRequestFilled:
        theResult = this->IsRequestFilled();
        break;
    case kASGConditionTaskValid:
    case 2000:
    case 2003:
        theResult = this->CheckTask(inTime);
        break;
    case kASGConditionTaskInvalid:
        theResult = this->CheckTask(inTime) == false;
        break;
    case kASGConditionNoTargets:
        theResult = this->GetNumTargets() == 0;
        break;
    default:
        theResult = EECPAttackSubGroup::TestCondition(inState, inTime);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x44327d
// Re-checks the task at random 15-180 s intervals: a location attack stays valid while enemies have influence within
// 2 regions, a wonder defence while the wonder lives; other defence tasks lapse with a 35% chance
bool EECPASGAir::IsLocationTaskValid(long inTime)
{
    U2DRectangle<long> theBounds;
    bool theResult = true;
    if (!(this->mNextTaskCheckTime < (unsigned long)inTime))
        return theResult;
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
                this->mNextTaskCheckTime = gPhysics->GetRandom(15000L, 180000L) + inTime;
        }
        break;
    case kAGTaskDefendWonder:
        this->mNextTaskCheckTime = gPhysics->GetRandom(15000L, 180000L) + inTime;
        if (this->mTask.mUnit == NULL || this->mTask.mUnit->mHitPoints <= 0)
            theResult = false;
        break;
    default:
        if (gPhysics->mRandom.Generate() > 0.65f && this->mTask.mMode == kAGModeDefense)
            theResult = false;
        else
            this->mNextTaskCheckTime = gPhysics->GetRandom(15000L, 180000L) + inTime;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x44343c
// Returns true while the task's unit is alive
bool EECPAttackSubGroup::IsUnitTaskValid(long inTime)
{
    if (this->mTask.mUnit != NULL && this->mTask.mUnit->mHitPoints > 0)
        return true;
    return false;
}

// FUNCTION: EXE 0x443455
// Appends a unit to a list of sub-group units (std::vector push_back)

// FUNCTION: EXE 0x443481
// Appends a target preference to a list (std::vector push_back)

// FUNCTION: EXE 0x4434ad
// Empties a list of target preferences (std::vector clear)

// FUNCTION: EXE 0x4434e8
// Reserves room in a list of AI locations (std::vector reserve)

// FUNCTION: EXE 0x4435c4
// Appends an AI location to a list (std::vector push_back)

// FUNCTION: EXE 0x4435f0
// Empties a list of AI locations (std::vector clear)

// FUNCTION: EXE 0x44362b
// Inserts one target preference before a position (std::vector insert)

// FUNCTION: EXE 0x44367b
// Erases a range of target preferences (std::vector erase)

// FUNCTION: EXE 0x4436e6
// Inserts one AI location before a position (std::vector insert)

// FUNCTION: EXE 0x443740
// Erases a range of AI locations (std::vector erase)

// FUNCTION: EXE 0x4437ab
// Returns a random float from the physics random generator (the body RTSPhysics::GetRandom() shares through ICF)
// No source here: 0x4437ab is the ICF representative of an AI function identical to RTSPhysics::GetRandom(),
// not RTSPhysics' own body

// FUNCTION: EXE 0x4437b4
// Inserts n copies of a target preference (std::vector insert)

// FUNCTION: EXE 0x443a68
// Inserts n copies of an AI location (std::vector insert)

// FUNCTION: EXE 0x443d3a
// Copy-constructs n target preferences into raw storage (std::vector _Ufill)

// FUNCTION: EXE 0x443d61
// Copy-constructs n AI locations into raw storage (std::vector _Ufill)

// FUNCTION: EXE 0x443d8c
// Returns the iterator moved back n elements (std::_Ptrit operator-, a shared body)

// FUNCTION: EXE 0x443da3
// Collects the units in the bounds that pass the air target filter, dropping duplicates when a multi-tile unit
// was accepted and uniqueness was asked for; returns the count
// EECollectASGTargets<EECPAAirTargetFilter> (ee/EECPAreaTargetCollector.h), emitted by CollectTargets above

// FUNCTION: EXE 0x443f4c
// Copy-constructs a range of AI locations into raw storage (std::vector _Ucopy)

// FUNCTION: EXE 0x443f78
// Copies a range of target preferences (std::copy)

// FUNCTION: EXE 0x443fa5
// Copies a range of AI locations (std::copy)

// FUNCTION: EXE 0x443fc9
// Copy-constructs a range of target preferences into raw storage (std::vector _Ucopy)

// FUNCTION: EXE 0x443ff1
// Assigns one target preference to every element of a range (std::fill)

// FUNCTION: EXE 0x44401b
// Copies a range of target preferences back to front (std::copy_backward)

// FUNCTION: EXE 0x44404d
// Assigns one AI location to every element of a range (std::fill)

// FUNCTION: EXE 0x44406a
// Copies a range of AI locations back to front (std::copy_backward)

// FUNCTION: EXE 0x444096
// Copy-constructs one target preference in place (std::_Construct)

// FUNCTION: EXE 0x4440bb
// Copy-constructs one AI location in place (std::_Construct)

// ICF-FOLDED: EXE 0x67c0b4
// Returns false
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGAir::IsAwayFromTowns()
{
    return false;
}

// ICF-FOLDED: EXE 0x67c0b4
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPASGAir::DecideSplit()
{
    return false;
}

// ICF-FOLDED: EXE 0x4e123d
// Does nothing for air sub-groups
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EECPASGAir::MoveToNearestTown()
{
}

// ICF-FOLDED: EXE 0x45019d
// Runs the state machine (next update in 3 s): scans or drops targets, updates units, then moves between the idle,
// wait, launch, hold, follow, reposition and hold-territory states as their conditions change
// (no address of its own: the same body as EECPASGSpaceFighter::Update, which ICF kept; [sib 4])
void EECPASGAir::Update(long inTime)
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
        if (!theFailed && !this->CheckTaskArrival(inTime)) {
            if (this->TestCondition(kASGConditionShouldReturn, inTime))
                this->SetState(kASGStateRemove, inTime);
            else if (this->TestCondition(kASGConditionTaskValid, inTime))
                this->SetState(kASGStateLaunch, inTime);
            else
                this->SetState(kASGStateIdle, inTime);
        }
        break;
    case kASGStateWaitForUnits:
        if (this->TestCondition(kASGConditionRequestEmpty, inTime))
            this->SetState(kASGStateRemove, inTime);
        else if (this->TestCondition(kASGConditionRequestFilled, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateRemove:  // LOST-DEBUG: the compare chain keeps 1002 and 6007 (0x450210, 0x45037a: -> the end), so their bodies compiled away
        EE_LOST_CASE(theFailed); break;
    case kASGStateSplit:
        if (this->TestCondition(kASGConditionAlways, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateLaunch:
        if (!this->CheckTaskArrival(inTime) && !this->TestCondition(kASGConditionTaskValid, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateHoldAtTask:
        if (this->TestCondition(kASGConditionHasTargets, inTime))
            this->SetState(kASGStateAttack, inTime);
        else {
            theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
            if (theFailed)
                this->SetState(kASGStateIdle, inTime);
            if (!theFailed && !this->TestCondition(kASGConditionHasTargets, inTime) && this->TestCondition(kASGConditionShouldRefreshTask, inTime))
                this->SetState(kASGStateRepositionAtTask, inTime);
        }
        break;
    case kASGStateFollowTaskUnit:
        if (this->TestCondition(3004, inTime))
            this->SetState(kASGStateFollowTaskUnit, inTime);
        else {
            theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
            if (theFailed)
                this->SetState(kASGStateIdle, inTime);
            if (!theFailed && this->TestCondition(kASGConditionTaskInOtherSubSector, inTime))
                this->SetState(kASGStateLaunch, inTime);
        }
        break;
    case kASGStateReturnToRallyPoint:
        EE_LOST_CASE(theFailed); break;
    case kASGStateRepositionAtTask:
        if (!this->TestCondition(kASGConditionTaskValid, inTime))
            this->SetState(kASGStateIdle, inTime);
        else
            this->SetState(kASGStateHoldAtTask, inTime);
        break;
    case kASGStateHoldTerritory:
        if (this->TestCondition(kASGConditionTerritoryTaskInvalid, inTime))
            this->SetState(kASGStateIdle, inTime);
        break;
    }
    this->mNextUpdateTime = inTime + 3000;
}

// ICF-FOLDED: EXE 0x44e240
// Returns true while the task location still has influence for the task's player
// (no address of its own: the same body as EECPASGSea::IsTerritoryTaskValid, which ICF kept; [sib 40])
bool EECPASGAir::IsTerritoryTaskValid(long inTime)
{
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    return theTable->GetTileValue(this->mTask.GetPlayerID(), this->mTask.mLocation.mTile) ? true : false;
}
