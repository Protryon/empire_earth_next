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

#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"
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
    // New (R5b.4, the thirteenth round): the decision is Rust (ee_units actions.rs), its operations
    // EEUnitUpdate.cpp's host calls
    EEUnitClassActionChanged(EE_UNITS_ACTION_BASE, this, inUnit);
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
    EEGoalDeque::const_iterator it = inUnit->GetQueueGoals(0).begin();
    EEGoalDeque::const_iterator theEnd = inUnit->GetQueueGoals(0).end();
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
