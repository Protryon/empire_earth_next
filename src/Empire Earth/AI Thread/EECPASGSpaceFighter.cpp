// C:\EEX\Empire Earth\AI Thread\EECPASGSpaceFighter.cpp (file name inferred: no __FILE__). 0x4fd61..0x5096f (unit map
// edges, not verified yet).
#include <windows.h>
#include <vector>
#include <set>

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
#include "ee/EECPASGSpaceship.h"
#include "ee/EECPASGSpaceFighter.h"

#include "ee/EEUGAttackUnit.h"

#include "ee/EEUGLocation.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_CASE, EE_LOST_DEBUG)

// FUNCTION: EXE 0x44fd61
// Creates a space fighter sub-group belonging to the given attack group
EECPASGSpaceFighter::EECPASGSpaceFighter(EECPAttackGroup* inGroup, long inKind)
    : EECPASGSpaceship(inGroup, inKind)
{
}

// SYNTHETIC: EXE 0x44fd7d
// (the scalar deleting destructor)

// FUNCTION: EXE 0x44fd99
// Destroys the space fighter sub-group (base members only)
EECPASGSpaceFighter::~EECPASGSpaceFighter()
{
}

// FUNCTION: EXE 0x44fda4
// Orders a unit to attack the target (or the unit holding it): off the map it gets an attack goal; on the map,
// unless taking off, landing or out of fuel while airborne, it flies to the target's tile
void EECPASGSpaceFighter::AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile)
{
    if (inUnit->IsDone())
        return;
    GE2DPoint thePoint(0.0f, 0.0f);
    EEComplexUnit* theTarget =
        inTarget->mContainer == NULL ? inTarget : inTarget->mContainer;
    bool theOnMap = gWorld.GetMap()->GetTile(inUnit->GetUnit()->mTileX, inUnit->GetUnit()->mTileY) != NULL;
    bool theCanAct = theOnMap && (inUnit->GetUnit()->mVelocity == 0.0f ||
        inUnit->GetUnit()->mPhysicalModel.GetLocation().GetZ() == inUnit->GetUnit()->mTile->GetHeight());
    inUnit->SetTarget(theTarget);
    EEUnitGoal* theGoal;
    long theAction;
    if (!theOnMap) {
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
        thePoint.SetX(theTarget->mTileX);
        thePoint.SetY(theTarget->mTileY);
        theGoal = new EEUGLocation(thePoint, true, false, false, 0.0f, false, false);
        theAction = theCanAct ? 1000 : 8000;
        if (!this->GiveGoal(inUnit->GetUnit(), theGoal, 0, theAction) && theGoal != NULL)
            delete theGoal;
    }
}

// FUNCTION: EXE 0x44fffb
// Takes any requested units; without an active task, releases every unit's target, flags it for orders and
// zeroes on-map units' flight time unless units were just taken
void EECPASGSpaceFighter::GoIdle()
{
    EE_LOST_DEBUG(9);  // LOST-DEBUG: none: exact only (tools/lostdebug_search.py, smallest n)
    bool theHadRequest = this->mRequest != NULL;
    if (this->mRequest != NULL)
        this->TakeRequestedUnits();
    if (this->mTask.mIsActive)
        return;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = this->mUnits[i]->GetUnit();
        this->mUnits[i]->ReleaseTarget();
        this->mUnits[i]->mNeedsOrders = true;
        if (!theHadRequest && gWorld.GetMap()->GetTile(theUnit->mTileX, theUnit->mTileY) != NULL &&
            theUnit->mFlightTime != 0)
            theUnit->mFlightTime = 0;
    }
}

// FUNCTION: EXE 0x4500d8
// Switches state: idle gets a task and goes idle; remove, launch, hold-at-task and hold-territory (return to the
// task) run their handlers; other states go to the spaceship class
void EECPASGSpaceFighter::SetState(long inState, long inTime)
{
    unsigned long theOldState = this->mState;
    this->mState = inState;
    switch (this->mState) {
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
        EECPASGSpaceship::SetState(inState, inTime);
        break;
    }
}

// FUNCTION: EXE 0x45019d
// Runs the state machine (next update in 3 s): scans or drops targets, updates units, then moves between the idle,
// wait, launch, hold, follow, reposition and hold-territory states as their conditions change
void EECPASGSpaceFighter::Update(long inTime)
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

// FUNCTION: EXE 0x450433
// Sends every active unit to the location (its area if off the map): units off the map prepare to move, others
// not taking off or landing get a move goal, forced for active non-wonder tasks
void EECPASGSpaceFighter::SetDestination(const EECPILocation& inLocation)
{
    EECPILocation theLocation(inLocation);
    GE2DPoint thePoint(0.0f, 0.0f);
    // (the tile used to be tested twice)
    if (gWorld.GetMap()->GetTileAtLocation(theLocation.mTile) == NULL)
        theLocation.ResetToArea();
    thePoint.SetXY(theLocation.mTile.mX, theLocation.mTile.GetY());
    if (gWorld.GetMap()->GetTileAtLocation(theLocation.mTile) == NULL)
        return;
    bool theFlag = this->mTask.mIsActive && this->mTask.mMode != kAGModeWonder;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i]->IsDone())
            continue;
        EECPAttackSubGroupUnit* theUnit = this->mUnits[i];
        EEUnitGoal* theGoal = NULL;
        bool theOnMap = gWorld.GetMap()->GetTile(theUnit->GetUnit()->mTileX, theUnit->GetUnit()->mTileY) != NULL;
        bool theCanAct = theOnMap && (theUnit->GetUnit()->mVelocity == 0.0f ||
            theUnit->GetUnit()->mPhysicalModel.GetLocation().GetZ() == theUnit->GetUnit()->mTile->GetHeight());
        if (!theOnMap) {
            theUnit->GetUnit()->StartAction(kActionPrepareToMove);
            theGoal = new EEUGLocation(thePoint, true, theFlag, false, 0.0f, false, false);
            if (!this->GiveGoal(theUnit->GetUnit(), theGoal, 0, theUnit->GetUnit()->GetCommandType()) && theGoal != NULL)
                delete theGoal;
        } else {
            if (theUnit->GetUnit()->GetCommandType() == kActionTakeoff || theUnit->GetUnit()->GetCommandType() == kActionTaxi ||
                theUnit->GetUnit()->GetCommandType() == kActionContinueToTaxi || theUnit->GetUnit()->GetCommandType() == kActionDescend ||
                theUnit->GetUnit()->GetCommandType() == kActionContinueToDescend || theUnit->GetUnit()->GetCommandType() == kActionBrake ||
                theUnit->GetUnit()->GetCommandType() == kActionContinueToBrake)
                continue;
            theGoal = new EEUGLocation(thePoint, true, theFlag, false, 0.0f, false, false);
            if (!this->GiveGoal(theUnit->GetUnit(), theGoal, 0, theCanAct ? 1000 : 8000) && theGoal != NULL)
                delete theGoal;
        }
    }
}

// FUNCTION: EXE 0x45072c
// Sends one unit to the centre of the location's tile (its area if off the map) like SetDestination; inFlag forces it
void EECPASGSpaceFighter::SendUnitTo(EECPAttackSubGroupUnit* inUnit, const EECPILocation& inLocation, bool inFlag)
{
    EECPILocation theLocation(inLocation);
    if (gWorld.GetMap()->GetTileAtLocation(theLocation.mTile) == NULL)
        theLocation.ResetToArea();
    if (inUnit == NULL || gWorld.GetMap()->GetTileAtLocation(theLocation.mTile) == NULL)
        return;
    GE2DPoint thePoint(theLocation.mTile.mX + 0.5f, theLocation.mTile.GetY() + 0.5f);
    EEUnitGoal* theGoal = NULL;
    bool theOnMap = gWorld.GetMap()->GetTile(inUnit->GetUnit()->mTileX, inUnit->GetUnit()->mTileY) != NULL;
    bool theCanAct = theOnMap && (inUnit->GetUnit()->mVelocity == 0.0f ||
        inUnit->GetUnit()->mPhysicalModel.GetLocation().GetZ() == inUnit->GetUnit()->mTile->GetHeight());
    if (!theOnMap) {
        inUnit->GetUnit()->StartAction(kActionPrepareToMove);
        theGoal = new EEUGLocation(thePoint, true, inFlag, false, 0.0f, false, false);
        if (!this->GiveGoal(inUnit->GetUnit(), theGoal, 0, inUnit->GetUnit()->GetCommandType()) && theGoal != NULL)
            delete theGoal;
    } else {
        if (inUnit->GetUnit()->GetCommandType() == kActionTakeoff || inUnit->GetUnit()->GetCommandType() == kActionTaxi ||
            inUnit->GetUnit()->GetCommandType() == kActionContinueToTaxi || inUnit->GetUnit()->GetCommandType() == kActionDescend ||
            inUnit->GetUnit()->GetCommandType() == kActionContinueToDescend || inUnit->GetUnit()->GetCommandType() == kActionBrake ||
            inUnit->GetUnit()->GetCommandType() == kActionContinueToBrake)
            return;
        theGoal = new EEUGLocation(thePoint, true, inFlag, false, 0.0f, false, false);
        if (!this->GiveGoal(inUnit->GetUnit(), theGoal, 0, theCanAct ? 1000 : 8000) && theGoal != NULL)
            delete theGoal;
    }
}

// ICF-FOLDED: EXE 0x442220
// Gives a live, enabled unit that is on the map or held by another the goal and starts the action; false otherwise
// (no address of its own: the same body as EECPASGAir::GiveGoal, which ICF kept; [sib 5])
bool EECPASGSpaceFighter::GiveGoal(EEComplexUnit* inUnit, EEUnitGoal* inGoal, bool inKeepCurrent, long inAction)
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

// ICF-FOLDED: EXE 0x441d2f
// Clears every unit's goals and all goal queues of the base or carrier holding it, then disbands the sub-group
// (no address of its own: the same body as EECPASGAir::Disband, which ICF kept; [sib 56])
void EECPASGSpaceFighter::Disband()
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
