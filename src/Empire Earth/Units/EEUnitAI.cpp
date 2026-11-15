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
#include "ee/EEUGRust.h"
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
#include "ee/EEUnitClassKinds.h"
#include "ee/EEAirTrafficControl.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEModel.h"

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
// 0x447abc (-6.0, the bottom of space) is ee_units cls_condition.rs DIVE_DEPTH

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
            theIdle = !static_cast<EEUGRust*>(inGoal)->GetIsForcedMove();
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
                    EEUGRust* theAttackGoal = EEUGRust::NewAttackUnit(inUnit, theTarget, false, false);
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

// FUNCTION: EXE 0x62e47a
// Tests one AI-script trigger for inUnit (its active goal may answer first); returns 0 for false, 1 for true, 2 for
// true from an active goal that is not the current one
// New (R5b.4, the thirty-fourth round): the answers are Rust (ee_units cls_condition.rs base, over the host below)
long EEUnitClass::BaseEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    return ee_units_cls_base_condition(EEObj(this), EEObj(inUnit), (uint32_t)inEvent);
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
        if (theGoal != NULL && theGoal->mKind == kGoalLocation && !static_cast<EEUGRust*>(theGoal)->GetIsForcedMove())
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}

// New (R5b.4, the thirty-fourth round): the trigger answers are Rust (ee_units cls_condition.rs: every kind's body and
// the common one, golden USST kind 2); this switch is their host, each case the original's expression or statement
static EEUnitGoal* CondGoal(uint64_t inGoal) { return (EEUnitGoal*)(uintptr_t)inGoal; }
static EEComplexUnit* CondUnit(uint64_t inUnit) { return static_cast<EEComplexUnit*>((EEUnit*)(uintptr_t)inUnit); }

static void CondPoint(EeUnitsCondIo* ioIo, const GE3DPoint& inPoint)
{
    ioIo->p[0] = inPoint.GetX();
    ioIo->p[1] = inPoint.GetY();
    ioIo->p[2] = inPoint.GetZ();
}

static int64_t CondTimeGone(EEUnitGoal* inGoal, int64_t inMs)
{
    return (unsigned long)(gPhysics->mGameTime - inGoal->GetStartTime()) >= (unsigned long)inMs;
}

extern "C" int64_t ee_units_cls_cond_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, EeUnitsCondIo* ioIo)
{
    EEComplexUnit* theUnit = CondUnit(inA);
    switch (inOp) {
    case EE_UNITS_COND_GOAL:
        return EEObj(inC != 0 ? theUnit->mGoalState : theUnit->mActiveGoal);
    case EE_UNITS_COND_GOAL_INFO: {
        EEUnitGoal* theGoal = CondGoal(inB);
        bool theForced = theGoal->mKind == kGoalLocation && static_cast<EEUGRust*>(theGoal)->GetIsForcedMove();
        return (int64_t)theGoal->mKind | (theGoal->mB1C ? 0x100 : 0) | (theForced ? 0x200 : 0);
    }
    case EE_UNITS_COND_GOAL_CONDITION:
        return CondGoal(inB)->EvaluateCondition(theUnit, (unsigned long)inC);
    case EE_UNITS_COND_GOAL_ELAPSED:
        return CondTimeGone(CondGoal(inB), inC);
    case EE_UNITS_COND_GOAL_TARGET:
        return EEObj(static_cast<EEUGRust*>(CondGoal(inB))->GetAttackTarget());
    case EE_UNITS_COND_BEHAVIOR: {
        EEUnitBehavior* theBehavior = theUnit->mBehavior;
        return (theBehavior->mCanTargetEnemies ? 1 : 0) | (theBehavior->mCanFlee ? 2 : 0) |
               (theBehavior->mCanPursuePastInitialContactLOS ? 4 : 0) | (theBehavior->mReturnsToInitialContactLocation ? 8 : 0);
    }
    case EE_UNITS_COND_MOVE_FREELY:
        return theUnit->mBehavior->CanMoveFreely(theUnit);
    case EE_UNITS_COND_LOCATION_QUEUED:
        return EEIsLocationGoalQueued(theUnit);
    case EE_UNITS_COND_LAST_ATTACKER:
        return EEObj(static_cast<EEUnit*>(theUnit->mLastAttacker));
    case EE_UNITS_COND_CONDITION_TARGET:
        return EEObj(static_cast<EEUnit*>(theUnit->GetConditionTarget()));
    case EE_UNITS_COND_CAN_REACH:
        return theUnit->CanReachUnit((EEUnit*)(uintptr_t)inB);
    case EE_UNITS_COND_IN_REACH:
        return theUnit->IsInReach((EEUnit*)(uintptr_t)inB);
    case EE_UNITS_COND_CAN_BE_ATTACKED:
        return ((EEUnitClass*)(uintptr_t)inB)->CanBeAttacked();
    case EE_UNITS_COND_DAMAGE_FACTOR:
        ioIo->f = EEDamageFactor((long)inC, (long)inD);
        return 0;
    case EE_UNITS_COND_DROP_LOCATION: {
        GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
        EEUnit* theTarget;
        EEFindDropLocation(theUnit, &theTarget, &theLocation, NULL);
        CondPoint(ioIo, theLocation);
        return EEObj(theTarget);
    }
    case EE_UNITS_COND_LINE_OF_SIGHT:
        ioIo->f = theUnit->GetLineOfSight();
        return 0;
    case EE_UNITS_COND_HIT_DISTANCE:
        ioIo->f = EEGetHitDistanceSquared(theUnit, (EEUnit*)(uintptr_t)inB, -10000.0f, -10000.0f);
        return 0;
    case EE_UNITS_COND_DISTANCE_3D:
        ioIo->f = theUnit->DistanceSquared3D(GE3DPoint(ioIo->p[0], ioIo->p[1], ioIo->p[2]));
        return 0;
    case EE_UNITS_COND_RESPONSE_RANGE:
        ioIo->f = theUnit->mPlayer->mComputerPlayer != NULL ? theUnit->mPlayer->mComputerPlayer->mMaxAttackResponseRange
                                                            : gCPBehaviors[kCPBehaviorMaximumAttackResponseRange].mFloatValue;
        return 0;
    case EE_UNITS_COND_DISTANCE_TO:
        ioIo->f = theUnit->DistanceSquaredTo(CondUnit(inB));
        return 0;
    case EE_UNITS_COND_ENEMY_IN_RANGE:
        return EEIsEnemyWithinRange(theUnit);
    case EE_UNITS_COND_OWNER:
        return (theUnit->mPlayer->mComputerPlayer != NULL ? 1 : 0) | (theUnit->mPlayer->mHumanPlayer != NULL ? 2 : 0);
    case EE_UNITS_COND_OFF_MAP: {
        const GETransformation& theLocation = theUnit->mPhysicalModel.GetLocation();
        if (inC == 0)
            return !::gWorld.mMap->mTerrainMesh.GetIsValidTile(UMFloorUnsigned(theLocation.GetX()), UMFloorUnsigned(theLocation.GetY()));
        if (inC == 1) {
            EEMapTile* theTile = ::gWorld.mMap->GetTile(UMFloorUnsigned(theLocation.GetX()), UMFloorUnsigned(theLocation.GetY()));
            return theTile != NULL && theTile->mTerrainType == kTerrainSpace;
        }
        GE3DPoint thePoint((float)theUnit->mTileX + 0.5f, (float)theUnit->mTileY + 0.5f, theUnit->mHeight);
        return theLocation.DistanceTo(thePoint) > gWorldTable.GetFloatValue(kWorldTileStrayDistance);
    }
    case EE_UNITS_COND_CARGO:
        return theUnit->GetNumCargoUnits();
    case EE_UNITS_COND_FRAME_COUNT:
        return theUnit->GetModel()->mFrameCounts[theUnit->GetAnimation()];
    case EE_UNITS_COND_MAX_VELOCITY:
        ioIo->f = theUnit->GetMaxVelocity();
        return 0;
    case EE_UNITS_COND_OBSTACLE: {
        if (theUnit->mBlockingUnit != NULL)
            return 1;
        EEUnitGoal* theGoal = theUnit->mActiveGoal;
        unsigned long theWait = UMFloatToLong(gPhysics->GetRandom(0.75f, 1.25f) * (float)gUnitAIWaitScale);
        if (theUnit->mUnitClass->mIsMelee && theUnit->mVelocity > 0.0f && theGoal != NULL && theGoal->mKind == kGoalAttackUnit)
            theWait = UMFloatToLong((float)theWait * kUnitAIShortWaitFactor);
        else if ((theUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen || theUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen)
                 && theUnit->mVelocity > 0.0f && theGoal != NULL && theGoal->mKind == kGoalBuildRepair)
            theWait = UMFloatToLong((float)theWait * kUnitAILongWaitFactor);
        if (theUnit->GetCollisionResolutionCount() <= (float)theWait)
            return 0;
        theUnit->mCollisionResolutionCount = 0.0f;
        return 1;
    }
    case EE_UNITS_COND_AUTO_WAYPOINT:
        return theUnit->ShouldMoveToNextAutoWaypoint();
    case EE_UNITS_COND_WAYPOINT_STEP: {
        bool theFlag;
        if (inC == 0) {
            theFlag = theUnit->mWaypoints.size() == 0;
            if (theFlag && (!gPathManager.HasAutoPath(theUnit) || !gPathManager.IsPathDone(theUnit))) {
                theUnit->RefreshPath();
                theFlag = theUnit->mWaypoints.size() == 0;
            }
        } else if (inC == 1) {
            if (theUnit->mWaypoints.size() == 0) {
                theUnit->RefreshPath();
                return (long)theUnit->mWaypoints.size() > 0;
            }
            if (theUnit->mWaypoints.size() == 1)
                theUnit->RefreshPath();
            return 1;
        } else if (inC == 2)
            theFlag = theUnit->mHasArrived;
        else
            return theUnit->mWaypoints.size() == 1 && gPathManager.IsPathDone(theUnit);
        if (theFlag && theUnit->mCollisionState != 0)
            theUnit->mCollisionState = 0;
        return theFlag;
    }
    case EE_UNITS_COND_HERD: {
        EEHerd* theHerd = theUnit->mHerd;
        if (theHerd == NULL)
            return 0;
        return 1 | (theHerd->mAnimal->mHasGroupAttack ? 2 : 0) | (theHerd->mAnimal->mHasParentalVengeance ? 4 : 0) |
               (theHerd->mAnimal->mHasGroupFlee ? 8 : 0);
    }
    case EE_UNITS_COND_ATTACK_NEARBY:
        return ((EEUnitClass*)(uintptr_t)inB)->AttackNearby(theUnit, CondGoal((uint64_t)inC));
    case EE_UNITS_COND_INITIAL_CONTACT:
        CondPoint(ioIo, theUnit->mInitialContactLocation);
        return 0;
    case EE_UNITS_COND_WAYPOINTS:
        return (int64_t)theUnit->mWaypoints.size();
    case EE_UNITS_COND_POSITION: {
        GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
        theUnit->GetPosition(thePosition);
        CondPoint(ioIo, thePosition);
        return 0;
    }
    case EE_UNITS_COND_GROUND_Z: {
        GE3DPoint theGround(0.0f, 0.0f, 0.0f);
        ::gWorld.mMap->GetGroundPoint(GE3DPoint(ioIo->p[0], ioIo->p[1], ioIo->p[2]), theGround);
        ioIo->f = theGround.GetZ();
        return 0;
    }
    case EE_UNITS_COND_TILE:
        ioIo->f = theUnit->mTile->GetHeight();
        return theUnit->mTile->mTerrainType;
    case EE_UNITS_COND_BUILD_QUEUE: {
        if (inC == 0)
            return (int64_t)theUnit->mCitizenBuildQueue.size();
        EEUnit* theSite = theUnit->PopBuildTarget();
        if (theSite == NULL)
            return 0;
        theUnit->SetGoal(EEUGRust::NewBuildRepair(theUnit->GetPlayer(), theSite, true), 0, 0);
        theUnit->StartAction(kActionMoveToRepairSite);
        return 1;
    }
    case EE_UNITS_COND_CAN_CONVERT: {
        bool theFlag;
        return EECanConvert(theUnit, CondUnit(inB), theFlag);
    }
    case EE_UNITS_COND_CONVERSION_COST:
        return EEGetConversionCost(theUnit);
    case EE_UNITS_COND_ATTACK_GOAL: {
        EEUGRust* theGoal = EEAttackUnitCast(theUnit->GetGoal());
        if (theGoal == NULL)
            return 0;
        EEUnit* theTarget = theGoal->GetAttackTarget();
        bool theValid = theTarget != NULL && (theTarget->mTileNode == NULL || !theTarget->GetComplexUnit()->mIsDisabled ||
                                              theTarget->GetComplexUnit()->IsProtected());
        bool theFriendly = theTarget != NULL && (theTarget->GetPlayer() == theUnit->GetPlayer() ||
                                                 theUnit->GetPlayer()->mEnemies[theTarget->GetPlayer()->GetPlayerID()] == 0);
        return 1 | (theValid ? 2 : 0) | (theGoal->IsConvertAttack() ? 4 : 0) | (theFriendly ? 8 : 0);
    }
    case EE_UNITS_COND_CRUSADER:
        return theUnit->GetUnitClass()->IsCrusader();
    case EE_UNITS_COND_SUNK: {
        GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
        const GEPhysicalModel& theModel = theUnit->mPhysicalModel;
        GEModel* theGEModel = theModel.GetModel();
        GEBoundingBox theTransformedBox;
        theUnit->GetPosition(thePosition);
        GETransformation theTransformation(false);
        theModel.GetLocation().Apply(theGEModel->GetInternalTransform(theModel.GetFrame()), theTransformation);
        float theScale = theModel.GetScale();
        theTransformation.Scale(theScale, theScale, theScale);
        theTransformation.SetX(thePosition.GetX());
        theTransformation.SetY(thePosition.GetY());
        theTransformation.SetZ(thePosition.GetZ());
        GEBoundingBox theBox = theGEModel->GetBoundingBox(theModel.GetFrame());
        theBox.Transform(theTransformation, theTransformedBox);
        return theTransformedBox.GetTopRightBack().GetZ() < 0.0f;
    }
    case EE_UNITS_COND_MEMBER: {
        EEUnitClass* theClass = (EEUnitClass*)(uintptr_t)inB;
        switch (inC) {
        case 0: return theClass->SelectUnitToRelease(theUnit);
        case 1: return theClass->EruptVolcano(theUnit);
        case 2: return theClass->HasSunk(theUnit);
        case 3: return theClass->AreEnemiesNear(theUnit);
        default: return theClass->IsPasserNear(theUnit);
        }
    }
    case EE_UNITS_COND_LAUNCH: {
        EEAirTrafficControl* theTraffic = theUnit->mPlayer->mAirTraffic[theUnit->mUnitClass->mAirTrafficType - 1];
        if (inC == 0)
            return theTraffic->GetQueueLength(theUnit) == 0 || theUnit->mExitingUnitID == -1;
        long theID = theUnit->mExitingUnitID;
        if (theID == -1)
            return 0;
        EEComplexUnit* theCargo = theUnit->FindPassenger(theID);
        if (theCargo != NULL)
            return theCargo->mIsClearanceGranted;
        theUnit->SetExitingUnitID(-1);
        EEUnit* theOther = ::gWorld.GetUnit(theID);
        if (theOther != NULL && theOther->mComplexUnit != NULL)
            theTraffic->RemovePlane(theUnit, theOther->mComplexUnit);
        return 0;
    }
    case EE_UNITS_COND_QUEUED_GOAL:
        return theUnit->GetQueuedGoal((unsigned long)inC) != NULL;
    case EE_UNITS_COND_MINE_DEPLETED: {
        EEUnit* theMine = theUnit->mMine;
        return theMine == NULL || !theMine->mIsValid || theMine->mResourceAmount <= 0;
    }
    case EE_UNITS_COND_SECONDARY_TARGET: {
        EEUGRust* theGoal = EEAttackUnitCast(theUnit->GetGoal());
        return theGoal != NULL && theGoal->GetAttackTarget()->mHitPoints > 0;
    }
    }
    return 0;
}
