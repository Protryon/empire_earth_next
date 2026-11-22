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
#include "rust/sim/units.h"
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
#include "ee/EECombatGoals.h"
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

/// The "no heading" marker (0x447ab0), the wait factors (0x447ab4, 0x447ab8) and the bottom of space (0x447abc) are
// ee_units uai_cond.rs's and cls_condition.rs's constants now.

// New (R5b.4, the thirty-fourth round): the trigger answers are Rust (ee_units cls_condition.rs: every kind's body and
// the common one, golden USST kind 2); this switch is their host, each case the original's expression or statement.
// The units' lane, round 8: the compound cases and EEUnitClass::AttackNearby (0x22d95f) are Rust (uai_cond.rs), each
// statement an EE_UNITS_UAI_* case
static EEUnitGoal* CondGoal(uint64_t inGoal) { return (EEUnitGoal*)(uintptr_t)inGoal; }
static EEComplexUnit* CondUnit(uint64_t inUnit) { return static_cast<EEComplexUnit*>((EEUnit*)(uintptr_t)inUnit); }
static EEUnit* CondSimple(uint64_t inUnit) { return (EEUnit*)(uintptr_t)inUnit; }
static EeUnitsAtc* CondTraffic(EEComplexUnit* inUnit) { return inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1]; }

// the statements' shared locals: EECanConvert's flag, the drop location's and the ground point
static bool sCondFlag;
static GE3DPoint sCondPoint;

static void CondPoint(EeUnitsCondIo* ioIo, const GE3DPoint& inPoint)
{
    ioIo->p[0] = inPoint.GetX();
    ioIo->p[1] = inPoint.GetY();
    ioIo->p[2] = inPoint.GetZ();
}

extern "C" int64_t ee_units_cls_cond_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, EeUnitsCondIo* ioIo)
{
    EEComplexUnit* theUnit = CondUnit(inA);
    switch (inOp) {
    // (the goals and their queues, the last attacker, the line of sight, the cargo count, the position, the distance
    // to a unit, the initial contact, the tile, the crusader test and the mine's depletion: ee_units cls_host.rs)
    case EE_UNITS_COND_MOVE_FREELY: return theUnit->mBehavior->CanMoveFreely(theUnit);
    case EE_UNITS_COND_CONDITION_TARGET: return EEObj(static_cast<EEUnit*>(theUnit->GetConditionTarget()));
    case EE_UNITS_COND_CAN_REACH: return theUnit->CanReachUnit(CondSimple(inB));
    case EE_UNITS_COND_IN_REACH: return theUnit->IsInReach(CondSimple(inB));
    case EE_UNITS_COND_CAN_BE_ATTACKED: return ((EEUnitClass*)(uintptr_t)inB)->CanBeAttacked();
    case EE_UNITS_COND_DAMAGE_FACTOR: ioIo->f = EEDamageFactor((long)inC, (long)inD); return 0;
    case EE_UNITS_COND_HIT_DISTANCE: ioIo->f = EECombatHitDistanceSquared(theUnit, CondSimple(inB), -10000.0f, -10000.0f); return 0;
    case EE_UNITS_COND_DISTANCE_3D: ioIo->f = theUnit->DistanceSquared3D(GE3DPoint(ioIo->p[0], ioIo->p[1], ioIo->p[2])); return 0;
    case EE_UNITS_COND_ENEMY_IN_RANGE: return EEIsEnemyWithinRange(theUnit);
    case EE_UNITS_COND_FRAME_COUNT: return theUnit->GetModel()->mFrameCounts[theUnit->GetAnimation()];
    case EE_UNITS_COND_MAX_VELOCITY: ioIo->f = theUnit->GetMaxVelocity(); return 0;
    // the waypoints' cases: Rust (cls_host.rs over waypoints.rs)
    case EE_UNITS_COND_CAN_CONVERT: return EECanConvert(theUnit, CondUnit(inB), sCondFlag);
    case EE_UNITS_COND_CONVERSION_COST: return EEGetConversionCost(theUnit);
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
    // ---- uai_cond.rs's statements (the compound answers and AttackNearby)
    case EE_UNITS_UAI_GOAL_KIND: return CondGoal(inB)->mKind;
    case EE_UNITS_UAI_GOAL_FORCED_MOVE: return static_cast<EEUGRust*>(CondGoal(inB))->GetIsForcedMove();
    case EE_UNITS_UAI_GOAL_B1C: return CondGoal(inB)->mB1C;
    case EE_UNITS_UAI_SEARCH_TIMER: return theUnit->mTargetSearchTimer;
    case EE_UNITS_UAI_CONTACT_HEADING: ioIo->f = theUnit->mInitialContactHeading; return 0;
    case EE_UNITS_UAI_RETURNS_TO_CONTACT: return theUnit->mBehavior->mReturnsToInitialContactLocation;
    case EE_UNITS_UAI_CONTACT_LOCATION: CondPoint(ioIo, theUnit->mInitialContactLocation); return 0;
    case EE_UNITS_UAI_FIND_TARGET_NEAR: return EEObj(EEFindTargetNear(theUnit, (long)inC, (long)inD, ioIo->f));
    case EE_UNITS_UAI_FIND_NEARBY_TARGET: return EEObj(EEFindNearbyTarget(theUnit));
    case EE_UNITS_UAI_GOAL_STATE: return EEObj(theUnit->mGoalState);
    case EE_UNITS_UAI_IS_LAND_BUILDING: return CondSimple(inB)->mUnitClass->IsLandBuilding();
    case EE_UNITS_UAI_ATTACK_OF: ioIo->f = CondSimple(inB)->mStats->mAttack; return 0;
    case EE_UNITS_UAI_CATEGORY: return theUnit->mUnitClass->mCategory;
    case EE_UNITS_UAI_AS_COMPLEX: return EEObj(EEUnitCast<EEComplexUnit>(CondSimple(inB)));
    case EE_UNITS_UAI_CAN_CONVERT: return EECanConvert(theUnit, CondUnit(inB), sCondFlag);
    case EE_UNITS_UAI_CONVERT_FLAG: return sCondFlag;
    case EE_UNITS_UAI_NEW_ATTACK: return EEObj(static_cast<EEUnitGoal*>(EEUGRust::NewAttackUnit(theUnit, CondSimple(inB), false, false)));
    case EE_UNITS_UAI_SET_GOAL: theUnit->SetGoal(CondGoal(inB), inC != 0, 0); return 0;
    case EE_UNITS_UAI_NOTICE_FOREIGN: theUnit->mPlayer->NoticeForeignUnit(CondSimple(inB)); return 0;
    case EE_UNITS_UAI_RESTART_SEARCH: theUnit->SetTargetSearchTimer(theUnit->mUnitClass->mTargetSearchInterval); return 0;
    case EE_UNITS_UAI_CAN_TARGET_ENEMIES: return theUnit->mBehavior->mCanTargetEnemies;
    case EE_UNITS_UAI_CAN_FLEE: return theUnit->mBehavior->mCanFlee;
    case EE_UNITS_UAI_CAN_PURSUE: return theUnit->mBehavior->mCanPursuePastInitialContactLOS;
    case EE_UNITS_UAI_FIND_DROP: return EEObj(EECombatFindDropLocation(theUnit, NULL, sCondPoint));
    case EE_UNITS_UAI_POINT: CondPoint(ioIo, sCondPoint); return 0;
    case EE_UNITS_UAI_CLEAR_POINT: sCondPoint = GE3DPoint(0.0f, 0.0f, 0.0f); return 0;
    case EE_UNITS_UAI_HAS_COMPUTER: return theUnit->mPlayer->mComputerPlayer != NULL;
    case EE_UNITS_UAI_CP_RESPONSE_RANGE: ioIo->f = theUnit->mPlayer->mComputerPlayer->mMaxAttackResponseRange; return 0;
    case EE_UNITS_UAI_DEFAULT_RESPONSE_RANGE: ioIo->f = gCPBehaviors[kCPBehaviorMaximumAttackResponseRange].mFloatValue; return 0;
    case EE_UNITS_UAI_HAS_HUMAN: return theUnit->mPlayer->mHumanPlayer != NULL;
    case EE_UNITS_UAI_VALID_TILE: return ::gWorld.mMap->mTerrainMesh.GetIsValidTile((unsigned long)inC, (unsigned long)inD);
    case EE_UNITS_UAI_SPACE_TILE: return ::gWorld.mMap->GetTile((unsigned long)inC, (unsigned long)inD) != NULL && ::gWorld.mMap->GetTile((unsigned long)inC, (unsigned long)inD)->mTerrainType == kTerrainSpace;
    case EE_UNITS_UAI_STRAY: return theUnit->mPhysicalModel.GetLocation().DistanceTo(GE3DPoint((float)theUnit->mTileX + 0.5f, (float)theUnit->mTileY + 0.5f, theUnit->mHeight)) > gWorldTable.GetFloatValue(kWorldTileStrayDistance);
    case EE_UNITS_UAI_HAS_BLOCKING: return theUnit->mBlockingUnit != NULL;
    case EE_UNITS_UAI_RANDOM: ioIo->f = gPhysics->GetRandom(ioIo->p[0], ioIo->p[1]); return 0;
    case EE_UNITS_UAI_WAIT_SCALE: return gUnitAIWaitScale;
    case EE_UNITS_UAI_ACTIVE_GOAL: return EEObj(theUnit->mActiveGoal);
    case EE_UNITS_UAI_IS_MELEE: return theUnit->mUnitClass->mIsMelee;
    case EE_UNITS_UAI_VELOCITY: ioIo->f = theUnit->mVelocity; return 0;
    case EE_UNITS_UAI_COLLISION_COUNT: ioIo->f = theUnit->GetCollisionResolutionCount(); return 0;
    case EE_UNITS_UAI_CLEAR_COLLISION_COUNT: theUnit->mCollisionResolutionCount = 0.0f; return 0;
    case EE_UNITS_UAI_HERD: return EEObj(theUnit->mHerd);
    case EE_UNITS_UAI_HERD_GROUP_ATTACK: return ((EEHerd*)(uintptr_t)inB)->GetAnimal()->mHasGroupAttack;
    case EE_UNITS_UAI_HERD_PARENTAL: return ((EEHerd*)(uintptr_t)inB)->GetAnimal()->mHasParentalVengeance;
    case EE_UNITS_UAI_HERD_GROUP_FLEE: return ((EEHerd*)(uintptr_t)inB)->GetAnimal()->mHasGroupFlee;
    case EE_UNITS_UAI_GROUND_POINT: ::gWorld.mMap->GetGroundPoint(GE3DPoint(ioIo->p[0], ioIo->p[1], ioIo->p[2]), sCondPoint); return 0;
    case EE_UNITS_UAI_POINT_Z: ioIo->f = sCondPoint.GetZ(); return 0;
    case EE_UNITS_UAI_CITIZEN_QUEUE_SIZE: return (int64_t)theUnit->mCitizenBuildQueue.size();
    case EE_UNITS_UAI_POP_BUILD_TARGET: return EEObj(theUnit->PopBuildTarget());
    case EE_UNITS_UAI_NEW_BUILD_REPAIR: return EEObj(static_cast<EEUnitGoal*>(EEUGRust::NewBuildRepair(theUnit->GetPlayer(), CondSimple(inB), true)));
    case EE_UNITS_UAI_START_ACTION: theUnit->StartAction((long)inC); return 0;
    case EE_UNITS_UAI_ATTACK_GOAL: return EEObj(static_cast<EEUnitGoal*>(EEAttackUnitCast(theUnit->GetGoal())));
    case EE_UNITS_UAI_ATTACK_TARGET: return EEObj(static_cast<EEUGRust*>(CondGoal(inB))->GetAttackTarget());
    case EE_UNITS_UAI_ON_MAP: return CondSimple(inB)->mTileNode != NULL;
    case EE_UNITS_UAI_DISABLED: return CondSimple(inB)->GetComplexUnit()->mIsDisabled;
    case EE_UNITS_UAI_PROTECTED: return CondSimple(inB)->GetComplexUnit()->IsProtected();
    case EE_UNITS_UAI_SAME_PLAYER: return CondSimple(inB)->GetPlayer() == theUnit->GetPlayer();
    case EE_UNITS_UAI_STANCE: return theUnit->GetPlayer()->mEnemies[CondSimple(inB)->GetPlayer()->GetPlayerID()];
    case EE_UNITS_UAI_CONVERT_ATTACK: return static_cast<EEUGRust*>(CondGoal(inB))->IsConvertAttack();
    case EE_UNITS_UAI_SELECT_TO_RELEASE: return ((EEUnitClass*)(uintptr_t)inB)->SelectUnitToRelease(theUnit);
    case EE_UNITS_UAI_ERUPT_VOLCANO: return ((EEUnitClass*)(uintptr_t)inB)->EruptVolcano(theUnit);
    case EE_UNITS_UAI_HAS_SUNK: return ((EEUnitClass*)(uintptr_t)inB)->HasSunk(theUnit);
    case EE_UNITS_UAI_ENEMIES_NEAR: return ((EEUnitClass*)(uintptr_t)inB)->AreEnemiesNear(theUnit);
    case EE_UNITS_UAI_PASSER_NEAR: return ((EEUnitClass*)(uintptr_t)inB)->IsPasserNear(theUnit);
    case EE_UNITS_UAI_TRAFFIC_QUEUE_LENGTH: return ee_units_atc_queue_length(CondTraffic(theUnit), theUnit);
    case EE_UNITS_UAI_EXITING_ID: return theUnit->mExitingUnitID;
    case EE_UNITS_UAI_FIND_PASSENGER: return EEObj(theUnit->FindPassenger((unsigned long)inC));
    case EE_UNITS_UAI_CLEARANCE: return CondUnit(inB)->mIsClearanceGranted;
    case EE_UNITS_UAI_SET_EXITING: theUnit->SetExitingUnitID((long)inC); return 0;
    case EE_UNITS_UAI_WORLD_UNIT: return EEObj(::gWorld.GetUnit((unsigned long)inC));
    case EE_UNITS_UAI_COMPLEX_OF: return EEObj(CondSimple(inB)->mComplexUnit);
    case EE_UNITS_UAI_REMOVE_PLANE: ee_units_atc_remove_plane(CondTraffic(theUnit), theUnit, CondUnit(inB)); return 0;
    }
    return 0;
}
