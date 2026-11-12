// C:\EEX\Empire Earth\Units\EEUnitAI.cpp (file name inferred: no __FILE__; it sorts between EEUnitAction.cpp and
// EEUnitClass.cpp). The unit classes' default action handling (EEUnitClass +0x74, run when a unit starts an action of
// its AI script) and default trigger tests (EEUnitClass +0xa0, EEUnitTrigger::Evaluate asks it). 0x22d95f..0x22f272.
#include <windows.h>
#include <stdio.h>

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
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EEUnitUpdateGolden.h"   // TEST ONLY (round 13's golden)
#include "ee/EEUnitKind.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEPathManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEPhysics.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPBehavior.h"
#include "ee/EEAnimal.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUCLandable.h"

// The obstacle wait time scale at 0x4c2374 (a long, 4000; EvaluateCondition reads it at 0x22eca0). Its owning unit
// is not recoverable, so it is only declared here (provisional name).
extern long gUnitAIWaitScale;
bool EEIsLocationGoalQueued(EEComplexUnit* inUnit);


// 0x24485b: the enemy unit near (inX, inY) within inRange that inUnit can attack (EEUCLandable.cpp; provisional name)
EEUnit* EEFindTargetNear(EEComplexUnit* inUnit, long inX, long inY, float inRange);
// 0x243f78: the enemy unit inUnit sees (provisional name)
EEUnit* EEFindNearbyTarget(EEComplexUnit* inUnit);

// The "no heading" marker of a unit's +0x214 (.rdata 0x447ab0, right after EEUnitAction's vtable; provisional name)
const float kUnitAINoHeading = -10000.0f;
const float kUnitAIShortWaitFactor = 0.5f;    // 0x447ab4 (EvaluateCondition 0x62e47a; provisional name)
const float kUnitAILongWaitFactor = 1.5f;     // 0x447ab8 (likewise; provisional name)
const float kUnitAIDiveDepth = -6.0f;         // 0x447abc (likewise; provisional name)

// FUNCTION: EXE 0x62d95f
// Picks a target for inUnit when its search timer has run out or it is attack-moving and gives it an attack goal
// (ahead of a move, patrol or explore goal); returns whether it did, and restarts the search timer
bool EEUnitClass::AttackNearby(EEComplexUnit* inUnit, EEUnitGoal* inGoal)
{
    unsigned char theKind;
    theKind = inGoal != NULL ? inGoal->mKind : 0;
    bool theResult = false;
    bool theIdle = inGoal != NULL;
    if (theIdle) {
        theIdle = theKind == 1;
        if (theIdle)
            theIdle = !static_cast<EEUGLocation*>(inGoal)->mIsForcedMove;
    }
    if (inUnit->mTargetSearchTimer <= 0 || theIdle) {
        EEUnit* theTarget;
        if (inUnit->mInitialContactHeading != kUnitAINoHeading && inUnit->mBehavior->mReturnsToInitialContactLocation
            && (inGoal == NULL || !inGoal->mB1C))
            theTarget = EEFindTargetNear(inUnit, UMFloor(inUnit->mInitialContactLocation.GetX()), UMFloor(inUnit->mInitialContactLocation.GetY()), 1.0f);
        else
            theTarget = EEFindNearbyTarget(inUnit);
        if (theTarget != NULL) {
            EEUnitGoal* theGoal = inUnit->mGoalState;
            if (theGoal == NULL || theGoal->mKind != kGoalAttackUnit || theGoal->mB1C || theTarget->mUnitClass->IsLandBuilding() != 1
                || theTarget->mStats->mAttack != 0.0f) {
                bool theAttack = true;
                if (inUnit->mUnitClass->mCategory == kUnitCategoryBuildingPriest) {
                    EEComplexUnit* theUnit = EEUnitCast<EEComplexUnit>(theTarget);
                    if (theUnit != NULL) {
                        bool theFlag;
                        if (!EECanConvert(inUnit, theUnit, theFlag) || theFlag)
                            theAttack = false;
                    }
                }
                if (theAttack) {
                    EEUGAttackUnit* theAttackGoal = new EEUGAttackUnit(inUnit, theTarget, false, false);
                    if (inGoal != NULL && (theKind == 1 || theKind == 9 || theKind == 10))
                        inUnit->SetGoal(theAttackGoal, 1, 0);
                    else
                        inUnit->SetGoal(theAttackGoal, 0, 0);
                    theResult = true;
                    inUnit->mPlayer->NoticeForeignUnit(theTarget);
                }
            }
        }
        inUnit->SetTargetSearchTimer(inUnit->mUnitClass->mTargetSearchInterval);
    }
    return theResult;
}

// FUNCTION: EXE 0x62db0f
// Runs the default handling of the unit's new AI-script action (animation, goals, herd, flight, healing, death,
// rotting), then restarts the current goal's timer
void EEUnitClass::ActionChanged(EEComplexUnit* inUnit)
{
    EEUnitActionScope theGoldenScope(inUnit->mUnitID);   // TEST ONLY (round 13's golden)
    (EEACN(RELEASE_HEAL_EFFECT), inUnit->ReleaseHealEffect());
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionAnimalParentalVengance:
        (EEACN(HERD_ATTACK_BACK), inUnit->mHerd->AttackBack(inUnit, true));
        break;
    case kActionAnimalRetaliation:
        if (inUnit->mHerd != NULL)
            (EEACN(HERD_UNIT_HIT), inUnit->mHerd->HandleUnitHit(inUnit));
        break;
    case kActionCheckRange:
        if (inUnit->mInitialContactHeading == kUnitAINoHeading) {
            EEACN(CONTACT_CHECK_RANGE);
            GEVector theUp(0.0f, 1.0f, 0.0f);
            GEVector theDirection(0.0f, 0.0f, 0.0f);
            const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
            GE3DPoint thePoint(theLocation.GetX(), theLocation.GetY(), 0.0f);
            theLocation.Apply(theUp, theDirection);
            float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
            if (inUnit->mInitialContactLocation != thePoint)
                inUnit->mInitialContactLocation = thePoint;
            if (inUnit->mInitialContactHeading != theYaw)
                inUnit->mInitialContactHeading = theYaw;
        }
        break;
    case kActionRetaliateAgainstAttacker:
        (EEACN(ATTACKED), this->Attacked(inUnit));
        break;
    case kActionRunFromAttacker:
        (EEACN(FLEE_ATTACKER), this->FleeAttacker(inUnit));
        break;
    case kActionShouldIFollowEnemyUnit:
        if (inUnit->mInitialContactHeading == kUnitAINoHeading) {
            EEACN(CONTACT_FOLLOW);
            GEVector theUp(0.0f, 1.0f, 0.0f);
            GEVector theDirection(0.0f, 0.0f, 0.0f);
            const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
            GE3DPoint thePoint(theLocation.GetX(), theLocation.GetY(), 0.0f);
            theLocation.Apply(theUp, theDirection);
            float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
            inUnit->SetInitialContactLocation(thePoint);
            if (inUnit->mInitialContactHeading != theYaw)
                inUnit->mInitialContactHeading = theYaw;
        }
        break;
    case kActionWaitForReload:
        if ((inUnit->mUnitClass->mDomains & kMovementAir) == 0 && inUnit->mUnitClass->mCategory != kUnitCategoryPlanetSpaceship
            && inUnit->GetAnimation() != (unsigned long)EEGetAttackFrameSet(inUnit)) {
            (EEACNA(SET_PHYSICS_ACTION, 3, 0), inUnit->SetCurrentPhysicsAction(3));
            if (inUnit->GetAnimation() != 0)
                (EEACNA(SET_ANIMATION, 0, 1), inUnit->SetAnimation(0, -1, 1));
        }
        break;
    case kActionReturnToInitialContactLocation: {
        EEUnitGoal* theLocation = new EEUGLocation(&inUnit->mInitialContactLocation, false, false, true, inUnit->mInitialContactHeading, false, false);
        EEUnitGoal* theGoal = inUnit->mGoalState;
        if (theGoal != NULL && (theGoal->mKind == kGoalPatrol || theGoal->mKind == kGoalLocation || theGoal->mKind == kGoalExplore))
            (EEACNA(RETURN_TO_CONTACT, 1, 0), inUnit->SetGoal(theLocation, 1, 0));
        else
            (EEACNA(RETURN_TO_CONTACT, 0, 0), inUnit->SetGoal(theLocation, 0, 0));
        (EEACNA(SET_TARGET_SEARCH_TIMER, 0, 0), inUnit->SetTargetSearchTimer(0));
        break;
    }
    case kActionPostCalamityFlee:
        (EEACN(FLEE_AFTER_CALAMITY), inUnit->FleeAfterCalamity(0));
        break;
    case kActionIdle: {
        (EEACNA(SET_PHYSICS_ACTION, 3, 0), inUnit->SetCurrentPhysicsAction(3));
        EEUnitClassUpgrade* theStats = inUnit->mStats;
        (EEACNA(SET_NEXT_TRIGGER_TIME, -1, 0), inUnit->mNextTriggerTime = -1);
        unsigned long theState = inUnit->GetAnimation();
        if (inUnit->IsSwimming())
            (EEACNA(SET_ANIMATION, 2, 0), inUnit->SetAnimation(2, -1, 0));
        else if ((!this->IsLandBuilding() || this->mAttack > 0) && theState != 7 && (inUnit->mStats->mCanFireOverWalls || theState != 8)) {
            if (gPhysics->GetRandom() < 0.5f || inUnit->mStats->mCanFireOverWalls)
                (EEACNA(SET_ANIMATION, 7, 0), inUnit->SetAnimation(7, -1, 0));
            else
                (EEACNA(SET_ANIMATION, 8, 0), inUnit->SetAnimation(8, -1, 0));
            (EEACN(PAUSE_ANIMATION), inUnit->SetIsAnimationPaused(true));
            EEACN(RANDOM_ANIMATION_TIME);
        inUnit->SetAnimationTime(gPhysics->GetRandom(0L, (long)theStats->mModel->mHoldTimes[inUnit->GetAnimation()]));
        }
        (EEACN(DROP_PATH), inUnit->DropPath());
        if (this->mDomains & kMovementSeaOrSpace)
            (EEACN(UPDATE_WATER_STATE), inUnit->UpdateWaterState(false));
        (EEACN(UNIT_GOAL_DONE), inUnit->mPlayer->UnitGoalDone(inUnit));
        if (!((inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen || inUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen) && inUnit->mContainer != NULL
              && inUnit->GetQueueGoals(0).size() > 1))
            (EEACNA(CLEAR_GOALS, 0, 0), inUnit->ClearGoals(0));
        (EEACN(CLEAR_HEADING), inUnit->mInitialContactHeading = kUnitAINoHeading);
        if (inUnit->mGoalState == NULL)
            (EEACN(CANCEL_PATHS), gPathManager.CancelPaths(inUnit, true));
        break;
    }
    case kActionResourcesRotting:
        (EEACNA(SET_PHYSICS_ACTION, 6, 0), inUnit->SetCurrentPhysicsAction(6));
        if (!inUnit->mUnitClass->mIsWall)
            (EEACNA(SET_ANIMATION, 5, 0), inUnit->SetAnimation(5, -1, 0));
        (EEACNA(SET_ROTTING, 0, 0), inUnit->SetRottingAwayTime(0));
        (EEACNA(CLEAR_GOALS, 0, 0), inUnit->ClearGoals(0));
        if (inUnit->mNextPart != NULL) {
            (EEACNA(NEXT_PART_ACTION, kActionResourcesRotting, 0), inUnit->mNextPart->StartAction(kActionResourcesRotting));
            inUnit->mNextPart->mUnitClass->ActionChanged(inUnit->mNextPart);
        }
        break;
    case kActionDying:
        (EEACN(UNIT_DIED), this->UnitDied(inUnit));
        break;
    case kActionReacquireGoal:
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalLoadTransport)
            (EEACN(GOAL_STEP), inUnit->mGoalState->Step(inUnit));
        break;
    case kActionUnitRotting:
        (EEACN(DESELECT), inUnit->mPlayer->DeselectUnit(inUnit));
        (EEACNA(SET_ROTTING, this->mRottingTime, 0), inUnit->SetRottingAwayTime(this->mRottingTime));
        if (inUnit->mNextPart != NULL) {
            (EEACNA(NEXT_PART_ACTION, kActionUnitRotting, 0), inUnit->mNextPart->StartAction(kActionUnitRotting));
            inUnit->mNextPart->mUnitClass->ActionChanged(inUnit->mNextPart);
        }
        break;
    case kActionFadingOutInSpace:
        (EEACN(CLOAK_CHAIN), inUnit->SetCloakedChain(true));
        (EEACN(OPAQUE_CHAIN), inUnit->SetOpacityChain(1.0f));
        break;
    case kActionKillSilently:
        (EEACNA(KILL, 0, 0), inUnit->Kill(false, false, false));
        break;
    case kActionKill:
        (EEACNA(KILL, 1, 0), inUnit->Kill(true, false, false));
        break;
    case kActionResourcesDepleting:
        (EEACNA(SET_PHYSICS_ACTION, 3, 0), inUnit->SetCurrentPhysicsAction(3));
        break;
    case kActionDead: {
        (EEACNA(SET_PHYSICS_ACTION, 6, 0), inUnit->SetCurrentPhysicsAction(6));
        (EEACN(KILL_UNIT_AND_PARTS), inUnit->mPlayer->KillUnit(inUnit, false));
        for (EEComplexUnit* theUnit = inUnit->mNextPart; theUnit != NULL; theUnit = theUnit->mNextPart)
            inUnit->mPlayer->KillUnit(theUnit, false);
        break;
    }
    case kActionPrepareToMove: {
        EEUnitGoal* theGoal = inUnit->mGoalState;
        if ((unsigned long)theGoal->mPathAttempts > (unsigned long)theGoal->GetMaxPathAttempts()) {
            (EEACN(START_IDLE), inUnit->StartAction(kActionIdle));
            inUnit->mUnitClass->ActionChanged(inUnit);
        } else
            (EEACN(PATH_TO_GOAL), this->PathToGoal(inUnit));
        break;
    }
    case kActionMoveToValidMapLocation:
        (EEACN(MOVE_TO_TILE_CENTRE), this->MoveToTileCentre(inUnit));
        break;
    case kActionGetNextAutoWaypoint:
        (EEACNA(CLEAR_GOALS, 0, 0), inUnit->ClearGoals(0));
        break;
    case kActionGetNextPatrolPoint:
        (EEACN(CLEAR_HEADING), inUnit->mInitialContactHeading = kUnitAINoHeading);
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalPatrol)
            (EEACN(GOAL_STEP), inUnit->mGoalState->Step(inUnit));
        break;
    case kActionFleeDanger:
        (EEACN(FLEE), this->Flee(inUnit));
        break;
    case kActionGetNextMoveWaypoint:
        (EEACN(NEXT_WAYPOINT), this->NextWaypoint(inUnit));
        break;
    case kActionFindSafeLocation:
        (EEACN(FIND_SAFE_LOCATION), this->FindSafeLocation(inUnit));
        break;
    case kActionAdvance:
        (EEACNA(SET_PHYSICS_ACTION, 0, 0), inUnit->SetCurrentPhysicsAction(0));
        (EEACN(CLEAR_ARRIVED), inUnit->SetHasArrived(false));
        if (inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen && inUnit->mResourceAmount > 0 && inUnit->mUnitClass->mResourceAmount == 0)
            (EEACNA(SET_ANIMATION, 2, 0), inUnit->SetAnimation(2, -1, 0));
        else if (inUnit->IsSwimming())
            (EEACNA(SET_ANIMATION, 2, 0), inUnit->SetAnimation(2, -1, 0));
        else
            (EEACNA(SET_ANIMATION, 0xd, 0), inUnit->SetAnimation(0xd, -1, 0));
        break;
    case kActionTurnToUnitFacing: {
        EEUnitGoal* theGoal = inUnit->mGoalState;
        if (theGoal != NULL && theGoal->mKind == kGoalLocation && (EEACN(FACE_YAW), static_cast<EEUGLocation*>(theGoal)->FaceYaw(inUnit)))
            break;
        (EEACNA(SET_PHYSICS_ACTION, 5, 0), inUnit->SetCurrentPhysicsAction(5));
        if (inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen && inUnit->mResourceAmount > 0 && inUnit->mUnitClass->mResourceAmount == 0
            && inUnit->GetAnimation() != 2)
            (EEACNA(SET_ANIMATION, 2, 0), inUnit->SetAnimation(2, -1, 0));
        else if (inUnit->IsSwimming())
            (EEACNA(SET_ANIMATION, 2, 0), inUnit->SetAnimation(2, -1, 0));
        else if (inUnit->GetAnimation() != 0xd)
            (EEACNA(SET_ANIMATION, 0xd, 0), inUnit->SetAnimation(0xd, -1, 0));
        break;
    }
    case kActionRepathAroundObstacle:
        (EEACN(FIND_PATH), this->FindPath(inUnit));
        break;
    case kActionHealUnit:
        (EEACN(HEAL), this->Heal(inUnit));
        break;
    case kActionLookForHealee: {
        EEUnit* theTarget = NULL;
        EEUnitGoal* theGoal = inUnit->mGoalState;
        if ((EEACN(FIND_HEALEE), this->FindTargetNear(inUnit, inUnit->mStats->GetLineOfSight(), &theTarget))) {
            EEUnitGoal* theHeal = new EEUGHealUnit(static_cast<EEComplexUnit*>(theTarget), false);
            if (theGoal != NULL && (theGoal->mKind == kGoalPatrol || theGoal->mKind == kGoalLocation || theGoal->mKind == kGoalExplore))
                (EEACNA(SET_HEAL_GOAL, 1, 0), inUnit->SetGoal(theHeal, 1, 0));
            else
                (EEACNA(SET_HEAL_GOAL, 0, 0), inUnit->SetGoal(theHeal, 0, 0));
        }
        (EEACN(RESET_HEAL_SEARCH), inUnit->SetHealSearchTimer(this->mHealDelay));
        break;
    }
    case kActionAttackEnemyUnit:
    case kActionAttackLocation:
    case kActionFaceEnemyUnit:
    case kActionCastCalamity:
    case kActionFaceLocation:
    case kActionPrepareToCastCalamity:
    case kActionCalamityPreCast:
    case kActionChangeTargetForSpecialEnemy:
    case kActionInsufficientResources:
    case kActionChangeTargetForSpecialRepair:
    case kActionReacquireEnemyUnit:
    case kActionFindUnexploredArea:
    case kActionLoadIntoContainer:
    case kActionUnloadTransport:
    case kActionMoveToRendezvous:
    case kActionPickedUpLoadableUnits:
    case kActionGetContainerProgress:
    case kActionGetNextTransportWaypoint:
    case kActionBumpExtraLoadableUnits:
    case kActionGarrisonUpgrade:
    case kActionWaitForGarrisonCost:
    case kActionWaitForPassengers:
    case kActionWaitForTransport:
    case kActionCannotReachContainer:
    case kActionTurnToFaceHealee:
        if (inUnit->mGoalState != NULL)
            (EEACN(GOAL_STEP), inUnit->mGoalState->Step(inUnit));
        break;
    case kActionSpecialLocation:
        break;
    default:
        return;
    }
    EEUnitGoal* theGoal = inUnit->mGoalState;
    if (theGoal != NULL && !gWorldClearing)
        (EEACN(GOAL_START_TIME), theGoal->SetStartTime(gPhysics->mGameTime));
}

// FUNCTION: EXE 0x62e47a
// Tests one AI-script trigger for inUnit (its active goal may answer first); returns 0 for false, 1 for true, 2 for
// true from an active goal that is not the current one
long EEUnitClass::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theFlag = false;
    EEUnitGoal* theGoal = inUnit->mActiveGoal;
    if (theGoal != NULL && theGoal->EvaluateCondition(inUnit, inEvent))
        return (theGoal != inUnit->mGoalState) + 1;
    switch (inEvent) {
    case kTriggerCanITargetEnemies:
        theFlag = inUnit->mBehavior->mCanTargetEnemies || EEIsLocationGoalQueued(inUnit);
        break;
    case kTriggerAmIUnderAttack:
        theFlag = (inUnit->mLastAttacker == NULL ? 0 : inUnit->GetConditionTarget()) != 0;
        break;
    case kTriggerAttackMoveEnabled: {
        unsigned char theKind = theGoal != NULL ? theGoal->mKind : 0;
        theFlag = theKind == kGoalLocation && this->CanAttackMove() && !static_cast<EEUGLocation*>(theGoal)->mIsForcedMove;
        break;
    }
    case kTriggerAmmoExhausted:
        theFlag = inUnit->mAmmunition == 0 && inUnit->mUnitClass->mUsesAmmunition;
        break;
    case kTriggerReloaded:
        theFlag = inUnit->mReloadTimer == 0;
        break;
    case kTriggerAttackerIsNotBuilding:
        theFlag = inUnit->mLastAttacker != NULL && inUnit->mLastAttacker->mUnitClass->mFamily != kUnitFamilyBuilding;
        break;
    case kTriggerAttackerIsReachable:
        theFlag = inUnit->mLastAttacker != NULL && inUnit->CanReachUnit(inUnit->mLastAttacker);
        break;
    case kTriggerCanDamageAttacker: {
        if (inUnit->mLastAttacker == NULL)
            break;
        EEUnitClass* theClass = inUnit->mLastAttacker->mUnitClass;
        long theColumn = inUnit->mUnitClass->mAmmoType;
        unsigned long theRow = theClass->mFamily;
        theFlag = theClass->CanBeAttacked() && EEDamageFactor(theRow, theColumn) != 0.0f;
        break;
    }
    case kTriggerCanFlee:
        theFlag = inUnit->mBehavior->mCanFlee && !EEIsLocationGoalQueued(inUnit);
        break;
    case kTriggerCanIMoveFreely:
        theFlag = inUnit->mBehavior->CanMoveFreely(inUnit) || EEIsLocationGoalQueued(inUnit);
        // port: a guard (a stance that returns to its initial contact location) closes on the unit that hit it,
        // then goes back (CanIPursuePastInitialContactLOS and ReturnsToInitialContactLocation still apply); the
        // original's ranged guards (dbunitbehavior guard mRangedCanMoveFreely 0) stood and took hits from beyond
        // their weapon range
        if (!theFlag && inUnit->mBehavior->mReturnsToInitialContactLocation && inUnit->mBehavior->mCanTargetEnemies
            && theGoal != NULL && theGoal->mKind == kGoalAttackUnit && inUnit->mLastAttacker != NULL
            && static_cast<EEUGAttackUnit*>(theGoal)->mTarget == inUnit->mLastAttacker && inUnit->mLastAttacker->mHitPoints > 0)
            theFlag = true;
        break;
    case kTriggerCanIPursuePastInitialContactLOS:
        theFlag = inUnit->mBehavior->mCanPursuePastInitialContactLOS || EEIsLocationGoalQueued(inUnit);
        break;
    case kTriggerEnemyInsideInitialContactLOS: {
        GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
        unsigned char theKind = theGoal != NULL ? theGoal->mKind : 0;
        if (theKind != kGoalAttackUnit && theKind != kGoalAttackGround)
            break;
        EEUnit* theTarget;
        EEFindDropLocation(inUnit, &theTarget, &theLocation, NULL);
        theLocation.SetZ(0.0f);
        float theRange = inUnit->GetLineOfSight();
        theFlag = inUnit->mInitialContactLocation.DistanceToSquared(theLocation) <= theRange * theRange;
        break;
    }
    case kTriggerEnemyWithinMinimumRange: {
        GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
        unsigned char theKind = theGoal != NULL ? theGoal->mKind : 0;
        if (theKind != kGoalAttackUnit && theKind != kGoalAttackGround)
            break;
        EEUnit* theTarget;
        EEFindDropLocation(inUnit, &theTarget, &theLocation, NULL);
        float theRange = this->mMinRange;
        float theDistance = theKind == kGoalAttackUnit
            ? EEGetHitDistanceSquared(inUnit, theTarget, -10000.0f, -10000.0f)
            : inUnit->DistanceSquared3D(theLocation);
        theFlag = theDistance < theRange * theRange;
        break;
    }
    case kTriggerHasValidInitialContactLocation:
        theFlag = inUnit->mInitialContactHeading != kUnitAINoHeading;
        break;
    case kTriggerIsAttackerInRetaliationRange: {
        if (inUnit->mLastAttacker == NULL)
            break;
        float theRange = inUnit->mPlayer->mComputerPlayer != NULL
            ? inUnit->mPlayer->mComputerPlayer->mMaxAttackResponseRange : gCPBehaviors[kCPBehaviorMaximumAttackResponseRange].mFloatValue;
        theFlag = inUnit->DistanceSquaredTo(inUnit->mLastAttacker) < theRange;
        break;
    }
    case kTriggerReturnsToInitialContactLocation:
        theFlag = inUnit->mBehavior->mReturnsToInitialContactLocation && !EEIsLocationGoalQueued(inUnit);
        break;
    case kTriggerUnitIdleOrAttackingDefenselessBuilding: {
        if (theGoal != NULL) {
            if (theGoal->mKind != kGoalAttackUnit)
                break;
            EEUnit* theTarget = static_cast<EEUGAttackUnit*>(theGoal)->mTarget;
            if (!theTarget->mUnitClass->IsLandBuilding()) {
                // port: a unit still on its way to its target (not yet in reach of it) fights back against another
                // unit that hits it (Generic Attack.tai UnderAttack retaliates only when this holds); the original
                // kept walking and took the hits
                theFlag = inUnit->mLastAttacker != NULL && inUnit->mLastAttacker != theTarget && !inUnit->IsInReach(theTarget);
                break;
            }
            if (theTarget->mStats->mAttack != 0.0f) {
                EEUnitClass* theClass = inUnit->mUnitClass;
                long theColumn = theTarget->mUnitClass->mAmmoType;
                unsigned long theRow = theClass->mFamily;
                if (theClass->CanBeAttacked()) {
                    theFlag = EEDamageFactor(theRow, theColumn) == 0.0f;
                    break;
                }
            }
        }
        theFlag = true;
        break;
    }
    case kTriggerAttackerInWeaponRange:
        theFlag = inUnit->mLastAttacker != NULL && inUnit->IsInReach(inUnit->mLastAttacker) && inUnit->mIsBuilt;
        break;
    case kTriggerGoalIsLocation:
    case kTriggerSafeLocationFound:
        theFlag = theGoal != NULL && theGoal->mKind == kGoalLocation;
        break;
    case kTriggerTakingLightDamage:
        theFlag = inUnit->mLastHitPoints > inUnit->mHitPoints;
        break;
    case kTriggerGoalIsUnit:
        theFlag = theGoal != NULL && theGoal->mKind == kGoalAttackUnit;
        break;
    case kTriggerEnemyUnitInLOS:
        theFlag = EEIsEnemyWithinRange(inUnit);
        break;
    case kTriggerOwnerIsComputer:
        theFlag = inUnit->mPlayer->mComputerPlayer != NULL;
        break;
    case kTriggerOwnerIsHuman:
        theFlag = inUnit->mPlayer->mHumanPlayer != NULL;
        break;
    case kTriggerGoalIsPlayerInitiated:
        theFlag = theGoal != NULL && theGoal->mB1C;
        break;
    case kTriggerOneSecondElapsed:
        theFlag = theGoal != NULL && (unsigned long)(gPhysics->mGameTime - theGoal->GetStartTime()) >= 1000;
        break;
    case kTriggerGoalIsAttackLocation:
        theFlag = theGoal != NULL && theGoal->mKind == kGoalAttackGround;
        break;
    case kTriggerUnitHasGoal:
        theFlag = theGoal != NULL;
        break;
    case kTriggerGoalIsCalamity:
        theFlag = theGoal != NULL && theGoal->mKind == kGoalCastCalamity;
        break;
    case kTriggerGoalIsExplore:
        theFlag = theGoal != NULL && theGoal->mKind == kGoalExplore;
        break;
    case kTriggerTwoSecondsElapsed:
        theFlag = theGoal != NULL && (unsigned long)(gPhysics->mGameTime - theGoal->GetStartTime()) >= 2000;
        break;
    case kTriggerLocationOffMap: {
        theFlag = !::gWorld.mMap->mTerrainMesh.GetIsValidTile(
            UMFloorUnsigned(inUnit->mPhysicalModel.GetLocation().GetX()),
            UMFloorUnsigned(inUnit->mPhysicalModel.GetLocation().GetY()));
        break;
    }
    case kTriggerLocationInSpace: {
        EEMapTile* theTile = ::gWorld.mMap->GetTile(
            UMFloorUnsigned(inUnit->mPhysicalModel.GetLocation().GetX()),
            UMFloorUnsigned(inUnit->mPhysicalModel.GetLocation().GetY()));
        theFlag = theTile != NULL && theTile->mTerrainType == kTerrainSpace;
        break;
    }
    case kTriggerGoalIsNotCalamity:
        theFlag = theGoal == NULL || theGoal->mKind != kGoalCastCalamity;
        break;
    case kTriggerLocationWayOffMap: {
        GE3DPoint theLocation((float)inUnit->mTileX + 0.5f, (float)inUnit->mTileY + 0.5f, inUnit->mHeight);
        theFlag = inUnit->mPhysicalModel.GetLocation().DistanceTo(theLocation) > gWorldTable.GetFloatValue(kWorldTileStrayDistance);
        break;
    }
    case kTriggerOneAndAHalfSecondsElapsed:
        theFlag = theGoal != NULL && (unsigned long)(gPhysics->mGameTime - theGoal->GetStartTime()) >= 1500;
        break;
    case kTriggerThreeSecondsElapsed:
        theFlag = theGoal != NULL && (unsigned long)(gPhysics->mGameTime - theGoal->GetStartTime()) >= 3000;
        break;
    case kTriggerCargoIsEmpty:
        theFlag = inUnit->GetNumCargoUnits() == 0;
        break;
    case kTriggerUnitHitSpace: {
        GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
        inUnit->GetPosition(theLocation);
        theFlag = inUnit->mTile->mTerrainType == kTerrainSpace && theLocation.GetZ() <= 0.0f;
        break;
    }
    case kTriggerReachedBottomOfSpace: {
        GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
        inUnit->GetPosition(theLocation);
        theFlag = theLocation.GetZ() <= kUnitAIDiveDepth;
        break;
    }
    case kTriggerDyingAnimationFinished:
        theFlag = inUnit->mIsAnimationPaused || inUnit->GetModel()->mFrameCounts[inUnit->GetAnimation()] == 1;
        break;
    case kTriggerCanIMove:
        theFlag = inUnit->GetMaxVelocity() > 0.1f;
        break;
    case kTriggerMovePreparationComplete:
        theFlag = inUnit->mIsPathDone && (long)inUnit->mWaypoints.size() > 0;
        break;
    case kTriggerUnitRottedAway:
        theFlag = inUnit->mRottingAwayTime == 0;
        break;
    case kTriggerResourceDepleted:
    case kTriggerResourcesRottedAway:
        theFlag = inUnit->mResourceAmount == 0;
        break;
    case kTriggerObstacleDetected: {
        if (inUnit->mBlockingUnit == NULL) {
            unsigned long theWait = UMFloatToLong(gPhysics->GetRandom(0.75f, 1.25f) * (float)gUnitAIWaitScale);
            if (inUnit->mUnitClass->mIsMelee && inUnit->mVelocity > 0.0f && theGoal != NULL && theGoal->mKind == kGoalAttackUnit)
                theWait = UMFloatToLong((float)theWait * kUnitAIShortWaitFactor);
            else if ((inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen || inUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen)
                     && inUnit->mVelocity > 0.0f && theGoal != NULL && theGoal->mKind == kGoalBuildRepair)
                theWait = UMFloatToLong((float)theWait * kUnitAILongWaitFactor);
            if (inUnit->GetCollisionResolutionCount() <= (float)theWait)
                break;
            inUnit->mCollisionResolutionCount = 0.0f;
        }
        theFlag = true;
        break;
    }
    case kTriggerShouldMoveToNextAutoWaypoint:
        theFlag = theGoal != NULL && inUnit->ShouldMoveToNextAutoWaypoint();
        break;
    case kTriggerGoalIsPatrol:
        theFlag = theGoal != NULL && theGoal->mKind == kGoalPatrol;
        break;
    case kTriggerNoMoreWaypoints:
        theFlag = inUnit->mWaypoints.size() == 0;
        if (theFlag && (!gPathManager.HasAutoPath(inUnit) || !gPathManager.IsPathDone(inUnit))) {
            inUnit->RefreshPath();
            theFlag = inUnit->mWaypoints.size() == 0;
        }
        if (theFlag && inUnit->mCollisionState != 0)
            inUnit->mCollisionState = 0;
        break;
    case kTriggerNextWaypointRetrieved:
        if (inUnit->mWaypoints.size() == 0) {
            inUnit->RefreshPath();
            theFlag = (long)inUnit->mWaypoints.size() > 0;
            break;
        }
        if (inUnit->mWaypoints.size() == 1)
            inUnit->RefreshPath();
        theFlag = true;
        break;
    case kTriggerNextPatrolPointRetrieved:
        theFlag = (long)inUnit->mWaypoints.size() > 0;
        break;
    case kTriggerArrivedAtMoveWaypoint:
        theFlag = inUnit->mHasArrived;
        if (theFlag && inUnit->mCollisionState != 0)
            inUnit->mCollisionState = 0;
        break;
    case kTriggerOneWaypointRemaining:
        theFlag = inUnit->mWaypoints.size() == 1 && gPathManager.IsPathDone(inUnit);
        break;
    case kTriggerIsAdult:
        theFlag = inUnit->mGrowth >= 1.0f;
        break;
    case kTriggerIsBaby:
        theFlag = inUnit->mGrowth < 1.0f;
        break;
    case kTriggerHealSearchTimeElapsed:
        theFlag = this->mHealDelay != 0.0f && inUnit->mHealSearchTimer == 0.0f;
        break;
    case kTriggerGoalIsNotAttack: {
        unsigned char theKind = theGoal != NULL ? theGoal->mKind : 0;
        theFlag = theKind != kGoalAttackUnit && theKind != kGoalAttackGround;
        break;
    }
    case kTriggerHasGroupAttack:
        theFlag = inUnit->mHerd != NULL && inUnit->mHerd->mAnimal->mHasGroupAttack;
        break;
    case kTriggerCanISeeEnemy:
        if (!inUnit->mBehavior->mCanTargetEnemies && !EEIsLocationGoalQueued(inUnit))
            break;
        theFlag = this->AttackNearby(inUnit, theGoal);
        break;
    case kTriggerEnemyUnitSpotted:
        theFlag = this->AttackNearby(inUnit, theGoal);
        break;
    case kTriggerShouldIRetaliate: {
        if (inUnit->mLastAttacker == NULL)
            break;
        EEComplexUnit* theAttacker = inUnit->GetConditionTarget();
        if (theAttacker == NULL || (!inUnit->mBehavior->mCanTargetEnemies && !EEIsLocationGoalQueued(inUnit))
            || (theGoal != NULL && theGoal->mB1C))
            break;
        EEUnitClass* theAttackerClass = theAttacker->mUnitClass;
        long theColumn = inUnit->mUnitClass->mAmmoType;
        unsigned long theRow = theAttackerClass->mFamily;
        if (!theAttackerClass->CanBeAttacked() || EEDamageFactor(theRow, theColumn) == 0.0f)
            break;
        if (theGoal != NULL) {
            if (theGoal->mKind != kGoalAttackUnit)
                break;
            EEUnit* theTarget = static_cast<EEUGAttackUnit*>(theGoal)->mTarget;
            if (!theTarget->mUnitClass->IsLandBuilding())
                break;
            if (theTarget->mStats->mAttack != 0.0f) {
                EEUnitClass* theClass = inUnit->mUnitClass;
                long theTargetColumn = theTarget->mUnitClass->mAmmoType;
                unsigned long theOwnRow = theClass->mFamily;
                if (theClass->CanBeAttacked() && EEDamageFactor(theOwnRow, theTargetColumn) != 0.0f)
                    break;
            }
        }
        float theRange = inUnit->mPlayer->mComputerPlayer != NULL
            ? inUnit->mPlayer->mComputerPlayer->mMaxAttackResponseRange : gCPBehaviors[kCPBehaviorMaximumAttackResponseRange].mFloatValue;
        theFlag = inUnit->DistanceSquaredTo(theAttacker) < theRange && inUnit->CanReachUnit(theAttacker);
        break;
    }
    case kTriggerShouldIRun:
        theFlag = inUnit->mBehavior->mCanFlee && !EEIsLocationGoalQueued(inUnit) && inUnit->mLastAttacker != NULL
                && inUnit->GetConditionTarget() != NULL && (theGoal == NULL || !theGoal->mB1C);
        break;
    case kTriggerGoalIsNotPlayerInitiated:
        theFlag = theGoal == NULL || !theGoal->mB1C;
        break;
    case kTriggerHasParentalVengance:
        theFlag = inUnit->mHerd != NULL && inUnit->mHerd->mAnimal->mHasParentalVengeance && inUnit->mUnitClass->mAttack > 0;
        break;
    case kTriggerHasGroupFlee:
        theFlag = inUnit->mHerd != NULL && inUnit->mHerd->mAnimal->mHasGroupFlee;
        break;
    case kTriggerAlwaysTrue:
        theFlag = true;
        break;
    default:
        break;
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x62f20d
// Returns whether inUnit's first goal queue holds a move goal that is not a forced move (EvaluateCondition's helper)
bool EEIsLocationGoalQueued(EEComplexUnit* inUnit)
{
    std::deque<EEUnitGoal*>::iterator it = inUnit->GetQueueGoals(0).begin();
    std::deque<EEUnitGoal*>::iterator theEnd = inUnit->GetQueueGoals(0).end();
    bool theResult = false;
    for (; it != theEnd && !theResult; ++it) {
        EEUnitGoal* theGoal = *it;
        if (theGoal != NULL && theGoal->mKind == kGoalLocation && !static_cast<EEUGLocation*>(theGoal)->mIsForcedMove)
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}
