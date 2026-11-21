// New (port): the plain reads of the goals' and shots' trigger tests and of the goals' steps (ee/EECombatGoals.h;
// rust/crates/ee_sim/src/combat/goals.rs; docs/RUST_PLAN.md R5b.2, sixth round), the hosts of the Rust tests and steps
// (each lazy read and each operation the original's statement), and their CBTC/CBGS capture records.
// Every read here is a field or a trivial accessor: no side effect, no random draw, nothing the test would not
// survive reading early; a missing unit, target, player or class leaves its fields 0.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitKind.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/RTSDummyHeader.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EECPASGFlags.h"
#include "ee/EERules.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERulesWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUnitAction.h"
#include "ee/EEPathManager.h"
#include "rust/sim/astar.h"   // the path goals' modifiers (modifier_obj.rs)
#include "rust/sim/units.h"   // ee_units_cls_base_condition

extern EEToHitTable gToHitTable;           // 0x5318e8 (Empire Earth.cpp; as EECPAttackSubGroupUnit.cpp declares it)

// (the records' batches, CBTC .. CBGO, are Rust's: ee_sim combat/capture_fold.rs)
void EECombatCaptureCondition(const EeCombatConditionIn& in, const EeCombatConditionOut& inOut)
{
    ee_combat_capture_condition(&in, &inOut);
}

namespace {

thread_local int sHostReadDepth[4] = {0, 0, 0, 0};

}   // namespace

EECombatHostReadScope::EECombatHostReadScope(int inSlot) : mSlot(inSlot), mOuter(sHostReadDepth[inSlot]++ == 0) {}
EECombatHostReadScope::~EECombatHostReadScope() { sHostReadDepth[this->mSlot]--; }

uint32_t EECombatCommandOf(const EEComplexUnit* inUnit)
{
    return inUnit->mCommand != NULL ? (uint32_t)inUnit->mCommand->mType : 0;
}

void EECombatCaptureIgnored(uint32_t inFunc, const EEComplexUnit* inUnit, const EEComplexUnit* inTarget, int32_t inResult,
                            uint32_t inCheckLocation)
{
    if (!EESimCaptureRecord::Active())
        return;
    uint32_t theHash = 0x811c9dc5u;
    for (size_t i = 0; i < inUnit->mIgnoredTargets.size(); i++) {
        const EEComplexUnitAttacker& theEntry = inUnit->mIgnoredTargets[i];
        uint32_t theFields[6] = {(uint32_t)theEntry.mTileX, (uint32_t)theEntry.mTileY, (uint32_t)theEntry.mFromX,
                                 (uint32_t)theEntry.mFromY, (uint32_t)theEntry.mUnitID, (uint32_t)theEntry.mTime};
        const unsigned char* theBytes = (const unsigned char*)theFields;
        for (size_t k = 0; k < sizeof(theFields); k++)
            theHash = (theHash ^ theBytes[k]) * 0x01000193u;
    }
    uint32_t theRecord[7] = {inFunc, (uint32_t)inUnit->mUnitID, inTarget != NULL ? (uint32_t)inTarget->mUnitID : 0,
                             (uint32_t)inResult, inCheckLocation, (uint32_t)inUnit->mIgnoredTargets.size(), theHash};
    ee_combat_capture_fold(EE_COMBAT_FOLD_X_READS, (const uint8_t*)theRecord, sizeof(theRecord));
}

void EECombatCaptureFlush()
{
    ee_combat_capture_fold_flush();
}

void EECombatCaptureStep(const EeCombatStepIn& in)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'G', 'S'));
    theRecord.Bytes(&in, sizeof(in));
    theRecord.Commit();
}

// ---- the hosts of ee_combat's trigger tests and steps

namespace {

struct EEConditionContext {
    int32_t mKind;
    unsigned long mEvent;
    EEComplexUnit* mUnit;
    EEUnitGoal* mGoal;                      // the goals' (attack unit, attack ground, heal)
    EEUnitClass* mClass;                    // the shots'
    EEUnit* mImpact;                        // FindImpactUnit's answer, kept for the targetable read and the action
};

uint32_t EEConditionBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

// the attack goal's target (attack unit), the heal goal's (heal)
EEUnit* EEConditionTarget(EEConditionContext* inContext)
{
    if (inContext->mKind == EE_COMBAT_GOAL_HEAL)
        return static_cast<EEUGRust*>(inContext->mGoal)->GetHealTarget();
    return static_cast<EEUGRust*>(inContext->mGoal)->GetAttackTarget();
}

// The heal goal's fallback, EEUGLocation::EvaluateCondition 0x227da5 of the location goal it derived from (its yaw
// 0, its facing flag false): facing done is FaceYaw 0x22794d at yaw 0 (within a degree, the unit snapped to it);
// facing not required is true; any other event the base goal's false
bool EEHealLocationCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    if (inEvent == kTriggerUnitFacingNotRequired)
        return true;
    if (inEvent != kTriggerCompletedUnitFacing)
        return false;
    const float theYaw0 = 0.0f;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
    theTransformation.Apply(theForward, theDirection);
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    float theDelta = theYaw - theYaw0;
    if (theDelta < -3.1415927f)
        theDelta += 6.2831855f;
    else if (theDelta > 3.1415927f)
        theDelta -= 6.2831855f;
    const float kOneDegree = 0.017453292f;
    bool theResult = theDelta >= -kOneDegree && theDelta <= kOneDegree;
    if (theResult) {
        GETransformation theNewTransformation;
        theNewTransformation = theTransformation;
        theNewTransformation.SetOrientationYPR(theYaw0, 0.0f, 0.0f);
        inUnit->SetTransformation(theNewTransformation, true);
    }
    return theResult;
}

// The lazy reads, each the original's call at the place the test makes it (ee_combat goals.rs EE_COMBAT_READ_*)
uint32_t EEConditionRead(void* inContext, int32_t inCode)
{
    EEConditionContext* theContext = (EEConditionContext*)inContext;
    EEComplexUnit* theUnit = theContext->mUnit;
    switch (inCode) {
    case EE_COMBAT_READ_LINE_OF_SIGHT:
        return EEConditionBits(theUnit->GetLineOfSight());
    case EE_COMBAT_READ_IN_RANGE_TO_HIT:
        return EECombatWorld(EE_COMBAT_WORLD_IN_RANGE_TO_HIT, theUnit, EEConditionTarget(theContext)) != 0;
    case EE_COMBAT_READ_IN_REACH:
        return theUnit->IsInReach(EEConditionTarget(theContext));
    case EE_COMBAT_READ_SEEN:
        if (theContext->mKind == EE_COMBAT_GOAL_HEAL)
            return EECombatIsSeenBy(EEConditionTarget(theContext), theUnit->GetPlayer(), true);
        return EECombatIsSeenBy(EEConditionTarget(theContext), theUnit->mPlayer, false);
    case EE_COMBAT_READ_FACES_ARCH:
        return theUnit->GetUnitClass()->FacesTarget(theUnit, EEConditionTarget(theContext), true);
    case EE_COMBAT_READ_FACES:
        return theUnit->mUnitClass->FacesTarget(theUnit, EEConditionTarget(theContext), false);
    case EE_COMBAT_READ_FACES_MOVING: {
        EEComplexUnit* theComplexUnit = EEConditionTarget(theContext)->mComplexUnit;
        return theUnit->GetUnitClass()->FacesTarget(theUnit, theComplexUnit, true);
    }
    case EE_COMBAT_READ_DISTANCE_SQUARED:
        return EEConditionBits(theUnit->DistanceSquaredToUnit(EEConditionTarget(theContext)));
    case EE_COMBAT_READ_BORDERING:
        return theUnit->IsOnTileBorderingTarget(EEConditionTarget(theContext));
    case EE_COMBAT_READ_LINE_BLOCKED: {
        EEUnit* theArg3 = NULL;
        long theArg4;
        long theArg5;
        return EEIsLineToUnitBlocked(theUnit, EEConditionTarget(theContext), &theArg3, &theArg4, &theArg5);
    }
    case EE_COMBAT_READ_GATE_CLOSED:
        return EECombatIsGateClosed(theUnit, static_cast<EEUGRust*>(theContext->mGoal));
    case EE_COMBAT_READ_STILL_REACHABLE:
        return EECombatIsStillReachable(theUnit, static_cast<EEUGRust*>(theContext->mGoal));
    case EE_COMBAT_READ_ENEMY_IN_RANGE:
        return EEIsEnemyWithinRange(theUnit);
    case EE_COMBAT_READ_BASE:
        switch (theContext->mKind) {
        case EE_COMBAT_GOAL_HEAL:
            return EEHealLocationCondition(theUnit, theContext->mEvent);
        case EE_COMBAT_GOAL_TORPEDO:
            return (uint32_t)theContext->mClass->MissileEvaluateCondition(theUnit, theContext->mEvent);
        default:
            // EEUnitClass::BaseEvaluateCondition, the Rust's answer called directly (it is EEUnitAI.cpp's one-line
            // forwarder: the combat goals named the units' AI unit only for it; PORT_LEARNINGS "Cycle cuts")
            return (uint32_t)ee_units_cls_base_condition(EEObj(theContext->mClass), EEObj(theUnit),
                                                         (uint32_t)theContext->mEvent);
        }
    case EE_COMBAT_READ_CAN_REACH_TILE: {
        const GE3DPoint& theTarget = static_cast<EEUGRust*>(theContext->mGoal)->GetGroundTarget();
        return theUnit->CanReachTile(UMFloor(theTarget.GetX()), UMFloor(theTarget.GetY()));
    }
    case EE_COMBAT_READ_FACES_POINT:
        return theUnit->GetUnitClass()->FacesPoint(theUnit, static_cast<EEUGRust*>(theContext->mGoal)->GetGroundTarget(), true);
    case EE_COMBAT_READ_HEAL_IN_RANGE:
        return EECombatHealInRange(theUnit, static_cast<EEUGRust*>(theContext->mGoal)->GetHealTarget());
    case EE_COMBAT_READ_REPAIRABLE:
        return static_cast<EEUGRust*>(theContext->mGoal)->GetHealTarget()->IsRepairable();
    case EE_COMBAT_READ_IMPACT_UNIT:
        theContext->mImpact = theContext->mClass->FindImpactUnit(theUnit);
        return theContext->mImpact != NULL;
    case EE_COMBAT_READ_TARGETABLE:
        return ee_units_uu_targetable((uint64_t)(uintptr_t)theUnit, (uint64_t)(uintptr_t)(theContext->mImpact->GetUnitClass()), 0);
    case EE_COMBAT_READ_GROUND_IMPACT:
        return theContext->mClass->CheckGroundImpact(theUnit);
    case EE_COMBAT_READ_WATER_IMPACT:
        return theContext->mClass->CheckWaterImpact(theUnit);
    }
    return 0;
}

struct EEStepContext {
    int32_t mKind;
    EEComplexUnit* mUnit;
    EEUnitGoal* mGoal;
};

// The step's operations, each the original's statement (ee_combat goals.rs EE_COMBAT_STEP_*); reads answer
int32_t EEStepOp(void* inContext, int32_t inOp, int32_t inArg)
{
    EEStepContext* theContext = (EEStepContext*)inContext;
    EEComplexUnit* theUnit = theContext->mUnit;
    switch (inOp) {
    case EE_COMBAT_STEP_PHYSICS_ACTION:
        theUnit->SetCurrentPhysicsAction((unsigned long)inArg);
        break;
    case EE_COMBAT_STEP_FACES:
        if (theContext->mKind == EE_COMBAT_GOAL_HEAL)
            return theUnit->GetUnitClass()->FacesTarget(theUnit, static_cast<EEUGRust*>(theContext->mGoal)->GetHealTarget(), false);
        return theUnit->mUnitClass->FacesTarget(theUnit, static_cast<EEUGRust*>(theContext->mGoal)->GetAttackTarget(), false);
    case EE_COMBAT_STEP_ANIMATION:
        return (int32_t)theUnit->mAnimation;
    case EE_COMBAT_STEP_SET_ANIMATION:
        theUnit->SetAnimation(inArg, -1, 0);
        break;
    case EE_COMBAT_STEP_DROP_PATH:
        theUnit->DropPath();
        break;
    case EE_COMBAT_STEP_RETARGET_GATE:
        static_cast<EEUGRust*>(theContext->mGoal)->RetargetGatePart(theUnit);
        break;
    case EE_COMBAT_STEP_ATTACK_FRAME_SET:
        return (int32_t)EEGetAttackFrameSet(theUnit);
    case EE_COMBAT_STEP_SOUND_FRAME:
        return (int32_t)theUnit->GetModel()->mSound1Frames[inArg];
    case EE_COMBAT_STEP_FRAME:
        return (int32_t)theUnit->mPhysicalModel.GetFrame();
    case EE_COMBAT_STEP_FIRE_SHOT:
        theUnit->mUnitClass->FireShot(theUnit);
        break;
    case EE_COMBAT_STEP_RELOAD_TIMER:
        theUnit->SetRandomReloadTimer(inArg);
        break;
    case EE_COMBAT_STEP_SEARCH_TIMER:
        theUnit->SetTargetSearchTimer(inArg);
        break;
    case EE_COMBAT_STEP_PATH_ATTEMPTS:
        theContext->mGoal->SetPathAttempts(inArg);
        break;
    case EE_COMBAT_STEP_FIRING_HIGH:
        return theUnit->mIsFiringHigh;
    }
    return 0;
}

}   // namespace

EEUnit* EECombatShotTarget(EEComplexUnit* inUnit)
{
    EEUnitGoal* theGoal = inUnit->mGoalState;
    return theGoal != NULL && theGoal->mKind == kGoalAttackUnit ? static_cast<EEUGRust*>(theGoal)->GetAttackTarget() : NULL;
}

long EECombatCondition(int32_t inKind, EEComplexUnit* inUnit, unsigned long inEvent, EEUnit* inTarget,
                       EEUnitGoal* inGoal, EEUnitClass* inClass)
{
    // the record: the store's part in Rust (ee_combat store.rs: the plain fields, the stats table's, the players'
    // tables); here the unit's player's IsHuman (a virtual) and the attack goal's own fields
    EeCombatConditionIn in;
    ee_combat_fill_condition(&in, inKind, (uint64_t)(uintptr_t)inUnit, (uint32_t)inEvent, (uint64_t)(uintptr_t)inTarget,
                             inTarget != NULL ? (uint64_t)(uintptr_t)static_cast<EEUnit*>(inTarget->mComplexUnit) : 0);
    in.human = inUnit->mPlayer != NULL && inUnit->mPlayer->IsHuman();
    if (inKind == EE_COMBAT_GOAL_ATTACK_UNIT) {
        EEUGRust* theGoal = static_cast<EEUGRust*>(inGoal);
        in.goal_tile[0] = (int32_t)theGoal->GetAttackTileX();
        in.goal_tile[1] = (int32_t)theGoal->GetAttackTileY();
        in.convert = theGoal->IsConvertAttack();
    }
    EEConditionContext theContext = {in.kind, in.event, inUnit, inGoal, inClass, NULL};
    EeCombatConditionHost theHost = {&theContext, EEConditionRead};
    EeCombatConditionOut theOut = ee_combat_condition(&in, &theHost);
    // the action before the record, as the original's body set the goal before its end
    if (theOut.action == EE_COMBAT_CONDITION_ATTACK_IMPACT_UNIT)
        inUnit->SetGoal(EEUGRust::NewAttackUnit(inUnit, theContext.mImpact, false, false), 0, 0);
    EECombatCaptureCondition(in, theOut);
    return theOut.result;
}

void EECombatStep(int32_t inKind, EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEUnit* inTarget)
{
    EeCombatStepIn in;
    ee_combat_fill_step(&in, inKind, (uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)inTarget);
    EEStepContext theContext = {inKind, inUnit, inGoal};
    EeCombatStepHost theHost = {&theContext, EEStepOp};
    ee_combat_goal_step(&in, &theHost);
    EECombatCaptureStep(in);
}

// ---- the host of ee_combat's reads (reads.rs): FindImpactUnit, FacesPoint, IsInReach, IsStillReachable,
// IsGateClosed, EEIsEnemyWithinRange. Every handle an EEUnit* (a player's an EEPlayer*), 0 for none; each read the
// original's expression on it. The unit's plain fields, its stats and the players' tables are ee_combat store.rs's
// reads (the unit store's records, the stats table), never asked here.

namespace {

struct EEQueryContext {
    EEComplexUnit* mUnit;                   // the call's unit
    EEUGRust* mGoal;                  // IsStillReachable's, IsGateClosed's attack goal
    const uint64_t* mRegion;                // FindImpactUnit's region (ee_combat regions.rs), found by its count read
    EEUnitClass* mClass = NULL;             // world.rs: the class of Attacked or FireShot
    EEUnitGoal* mAttackedGoal = NULL;       // Attacked's goal (the helpers' clones)
    std::vector<EEUnit*> mNear;             // Attacked's units near the attacked unit
    U2DPoint<long> mTile;                   // Attacked's EEIsAttackReachable tile (one for all helpers, as the original)
    GE3DPoint mPoint;                       // FireShot's drop point
    std::vector<EETileCoordinate> mFootprint;   // EEIsSeenBy's footprint tiles
    EEUnitGoal* mHelpGoal = NULL;           // Attacked's helper's clone of mAttackedGoal
    EEGraphicEffect* mEffect = NULL;        // ApplyHit's unit effect (GetEffect(3))
};

template <class T> T* EEObjectOf(int64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

EEUnit* EEQueryUnit(uint64_t inHandle)
{
    return (EEUnit*)(uintptr_t)inHandle;
}

// a handle whose complex members are read (every one the original reads them of is a complex unit)
EEComplexUnit* EEQueryComplex(uint64_t inHandle)
{
    return static_cast<EEComplexUnit*>(EEQueryUnit(inHandle));
}

uint64_t EEQueryHandleOf(const EEUnit* inUnit)
{
    return (uint64_t)(uintptr_t)inUnit;
}

// (the plain member reads are Rust's: ee_combat query_views.rs)
uint32_t EEQueryRead(void* inContext, int32_t inCode, uint64_t inObject, int32_t inArg)
{
    EEQueryContext* theContext = (EEQueryContext*)inContext;
    EEComplexUnit* theUnit = theContext->mUnit;
    EEUnit* theObject = EEQueryUnit(inObject);
    switch (inCode) {
    case EE_COMBAT_Q_MODEL:
        return EEQueryComplex(inObject)->mPhysicalModel.GetModel() != NULL;
    case EE_COMBAT_Q_TARGETABLE:
        return ee_units_uu_targetable((uint64_t)(uintptr_t)(inArg ? theUnit : NULL), (uint64_t)(uintptr_t)theObject->mUnitClass, 0);
    case EE_COMBAT_Q_PASSABLE:
        return EEQueryComplex(inObject)->IsPassableBy(theUnit);
    case EE_COMBAT_Q_UNIT_SET:
        return gUnitSetManager.Contains(theObject->mUnitClass->GetUnitType(), (unsigned long)inArg);
    case EE_COMBAT_Q_SCALE:
        return EEConditionBits(EEQueryComplex(inObject)->mPhysicalModel.GetScale());
    case EE_COMBAT_Q_SIDE_PART:
        return EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theObject->GetUnitClass())->IsSidePart(theObject->mComplexUnit);
    case EE_COMBAT_Q_PATH_ATTEMPTS:
        return (uint32_t)theContext->mGoal->mPathAttempts;
    case EE_COMBAT_Q_CAN_REACH_UNIT:
        return theUnit->CanReachUnit(theObject);
    case EE_COMBAT_Q_ATTACK_REACHABLE: {
        U2DPoint<long> theTile;
        return EEIsAttackReachable(theUnit, EEQueryComplex(inObject), &theTile);
    }
    case EE_COMBAT_Q_IGNORED:
        return theUnit->IsTargetIgnored(EEQueryComplex(inObject), true);
    case EE_COMBAT_Q_LINE_OF_SIGHT:
        return EEConditionBits(EEQueryComplex(inObject)->GetLineOfSight());
    case EE_COMBAT_Q_SEEN:
        return EECombatIsSeenBy(EEQueryComplex(inObject), theUnit->GetPlayer(), true);
    case EE_COMBAT_Q_DUMMY_HEAD:
        return theObject == NULL || RTSDummyHead(theObject) != NULL;
    case EE_COMBAT_Q_PROJECTILE_CATEGORY: {
        EEUnitClass* theProjectile = theObject->GetUnitClass()->mProjectileClass;
        return theProjectile != NULL ? (uint32_t)theProjectile->mCategory : EE_COMBAT_Q_NONE;
    }
    case EE_COMBAT_Q_HIT_DISTANCE_SQUARED:
        return EEConditionBits(EECombatHitDistanceSquared(theUnit, theObject, -10000.0f, -10000.0f));
    case EE_COMBAT_Q_TAG_COUNT: {
        EEComplexUnit* theComplex = EEQueryComplex(inObject);
        EETagPointList* theTags = theComplex->GetModel()->mTagPoints[theComplex->mAnimation];
        return theTags != NULL ? (uint32_t)theTags->GetNumTagPoints() : 0;
    }
    case EE_COMBAT_Q_IN_RANGE_TO_HIT:
        return EECombatWorld(EE_COMBAT_WORLD_IN_RANGE_TO_HIT, theUnit, theObject) != 0;
    case EE_COMBAT_Q_BALLISTIC_HIT:
        return EECanBallisticHit(theUnit, theObject);
    case EE_COMBAT_Q_CONTACT_RANGE: {
        float theDistance;
        memcpy(&theDistance, &inArg, 4);
        return EEIsInContactRange(theUnit, theObject, theDistance);
    }
    case EE_COMBAT_Q_REGION_COUNT: {
        EeCombatUnitRegions* theTable = ::gWorld.mMap->mUnitRegions;
        long theSize = ee_combat_regions_size(theTable);
        int32_t theCount;
        theContext->mRegion = ee_combat_regions_units(theTable, (uint32_t)(theObject->GetTileX() / theSize),
                                                      (uint32_t)(theObject->GetTileY() / theSize), &theCount);
        return theContext->mRegion != NULL ? (uint32_t)theCount : EE_COMBAT_Q_NONE;
    }
    case EE_COMBAT_Q_CONTACT_X:
        return EEConditionBits(EEQueryComplex(inObject)->mInitialContactLocation.GetX());
    case EE_COMBAT_Q_CONTACT_Y:
        return EEConditionBits(EEQueryComplex(inObject)->mInitialContactLocation.GetY());
    case EE_COMBAT_Q_CONTACT_Z:
        return EEConditionBits(EEQueryComplex(inObject)->mInitialContactLocation.GetZ());
    case EE_COMBAT_Q_IGNORED_COUNT: return (uint32_t)EEQueryComplex(inObject)->mIgnoredTargets.size();
    case EE_COMBAT_Q_DISTANCE_SQUARED: return EEConditionBits(theUnit->DistanceSquaredTo(EEQueryComplex(inObject)));
    case EE_COMBAT_Q_TARGETS_ENEMIES: return EEQueryComplex(inObject)->mBehavior->mCanTargetEnemies;
    case EE_COMBAT_Q_MOVES_FREELY: return EEQueryComplex(inObject)->mBehavior->CanMoveFreely(EEQueryComplex(inObject));
    case EE_COMBAT_Q_GOAL_B1C: return EEQueryComplex(inObject)->mGoalState->GetB1C();
    case EE_COMBAT_Q_SHOT_CLASS: {
        EEUnitClass* theClass = theContext->mClass;
        return (theClass->mProjectileClass != NULL ? EE_COMBAT_SHOT_CLASS_PROJECTILE : 0)
            | (theClass->mUsesAmmunition ? EE_COMBAT_SHOT_CLASS_AMMUNITION : 0)
            | (theClass->mMuzzleFlashEffect != NULL ? EE_COMBAT_SHOT_CLASS_MUZZLE_FLASH : 0)
            | (theClass->mShotEffect != NULL ? EE_COMBAT_SHOT_CLASS_SHOT_EFFECT : 0);
    }
    case EE_COMBAT_Q_TURRET_PARTS: return (uint32_t)EEQueryComplex(inObject)->GetModel()->mTurretParts.size();
    case EE_COMBAT_Q_HIGH_TECH_TOOLS:
        return EEGetRules().Epoch(theObject->mPlayer->mTechnologies->mCurrentEpoch).mHighTechTools;   // epochs.yaml
    case EE_COMBAT_Q_SPARKS_EFFECT: return theObject->mUnitClass->mImpactSparksEffect != NULL;
    // leaves.rs (R5b.2, thirteenth round)
    case EE_COMBAT_Q_WAYPOINTS: return (uint32_t)EEQueryComplex(inObject)->mWaypoints.size();
    case EE_COMBAT_Q_MAX_VELOCITY: return EEConditionBits(EEQueryComplex(inObject)->GetMaxVelocity());
    }
    return 0;
}

uint64_t EEQueryHandle(void* inContext, int32_t inCode, uint64_t inObject, int32_t inArg)
{
    EEQueryContext* theContext = (EEQueryContext*)inContext;
    EEUnit* theObject = EEQueryUnit(inObject);
    switch (inCode) {
    case EE_COMBAT_H_COMPLEX:
        return theObject != NULL ? EEQueryHandleOf(theObject->GetComplexUnit()) : 0;
    case EE_COMBAT_H_PREVIOUS_PART:
        return EEQueryHandleOf(EEQueryComplex(inObject)->mPreviousPart);
    case EE_COMBAT_H_OWNER:
        return EEQueryHandleOf(EEQueryComplex(inObject)->mOwner);
    case EE_COMBAT_H_GOAL_TARGET: {
        EEUGRust* theGoal = EEAttackUnitCast(EEQueryComplex(inObject)->mGoalState);
        return theGoal != NULL ? EEQueryHandleOf(theGoal->GetAttackTarget()) : 0;
    }
    case EE_COMBAT_H_REGION_UNIT:
        return theContext->mRegion[inArg];
    case EE_COMBAT_H_TILE_PREV:
        return EEQueryHandleOf(static_cast<EEUnit*>(theObject->mTileNode->mPrev));
    case EE_COMBAT_H_TILE_NEXT:
        return EEQueryHandleOf(static_cast<EEUnit*>(theObject->mTileNode->mNext));
    case EE_COMBAT_H_TARGET:
        return EEQueryHandleOf(theContext->mGoal->GetAttackTarget());
    case EE_COMBAT_H_ACTIVE_GOAL_TARGET:
        return EEQueryHandleOf(static_cast<EEUGRust*>(EEQueryComplex(inObject)->mActiveGoal)->GetAttackTarget());
    }
    return 0;
}

void EEQueryAct(void* inContext, int32_t inCode, uint64_t inObject, int32_t inArg)
{
    EEQueryContext* theContext = (EEQueryContext*)inContext;
    switch (inCode) {
    case EE_COMBAT_A_FIRING_HIGH:
        EEQueryComplex(inObject)->mIsFiringHigh = true;
        break;
    case EE_COMBAT_A_WEAPON_TAG:
        EEQueryComplex(inObject)->mWeaponTagPoint = (unsigned long)(uint32_t)inArg;
        break;
    case EE_COMBAT_A_IGNORE:
        theContext->mUnit->IgnoreTarget(EEQueryComplex(inObject));
        break;
    }
}

void EEQueryLocation(void* inContext, uint64_t inObject, float (*outRows)[16])
{
    memcpy(*outRows, EEQueryComplex(inObject)->mPhysicalModel.GetLocation().mMatrix, sizeof(*outRows));
}

int32_t EEQueryIntersects(void* inContext, uint64_t inCandidate, const float (*inLine)[6], int32_t inSegment,
                          uint32_t inLOD, float inScale)
{
    EEComplexUnit* theUnit = ((EEQueryContext*)inContext)->mUnit;
    GE3DLine theLine;
    if (inSegment) {
        theLine.SetStart(GE3DPoint((*inLine)[0], (*inLine)[1], (*inLine)[2]));
        theLine.SetDirection(GEVector((*inLine)[3], (*inLine)[4], (*inLine)[5]));
    }
    return theUnit->mPhysicalModel.Intersects(&EEQueryComplex(inCandidate)->mPhysicalModel, theLine, inSegment != 0,
                                              inLOD, 1.0f, 1.0f, inScale);
}

void EEQueryFootprintTile(void* inContext, uint64_t inObject, int32_t (*outTile)[2])
{
    long theX;
    long theY;
    ((EEQueryContext*)inContext)->mUnit->GetNearestFootprintTile(EEQueryUnit(inObject), &theX, &theY);
    (*outTile)[0] = (int32_t)theX;
    (*outTile)[1] = (int32_t)theY;
}

EeCombatQueryHost EEQueryHostOf(EEQueryContext* inContext)
{
    EeCombatQueryHost theHost = {inContext, EEQueryRead, EEQueryHandle, EEQueryAct, EEQueryLocation, EEQueryIntersects,
                                 EEQueryFootprintTile};
    return theHost;
}

// ---- the operations of ee_combat world.rs (R5b.2, eighth round): each the original's statement on the handles
// (the effects, the one-member stores and the plain reads are Rust's: ee_sim combat/world_views.rs)
uint64_t EEWorldOp(void* inContext, int32_t inCode, uint64_t inObject, uint64_t inOther, EeCombatWorldArgs* ioArgs)
{
    EEQueryContext* theContext = (EEQueryContext*)inContext;
    EEComplexUnit* theObject = EEQueryComplex(inObject);
    EEComplexUnit* theOther = EEQueryComplex(inOther);
    int64_t* a = *ioArgs;
    EEUnitClass* theClass = theContext->mClass;
    switch (inCode) {
    // (the compound cases are split in Rust, ee_sim combat cg_world.rs: each statement one case here, in order)
    case EE_COMBAT_W_IGN_TILE_X: return theObject->mIgnoredTargets[(size_t)a[0]].mTileX;
    case EE_COMBAT_W_IGN_TILE_Y: return theObject->mIgnoredTargets[(size_t)a[0]].mTileY;
    case EE_COMBAT_W_IGN_FROM_X: return theObject->mIgnoredTargets[(size_t)a[0]].mFromX;
    case EE_COMBAT_W_IGN_FROM_Y: return theObject->mIgnoredTargets[(size_t)a[0]].mFromY;
    case EE_COMBAT_W_IGN_UNIT_ID: return theObject->mIgnoredTargets[(size_t)a[0]].mUnitID;
    case EE_COMBAT_W_IGN_TIME: return theObject->mIgnoredTargets[(size_t)a[0]].mTime;
    case EE_COMBAT_W_IGN_ERASE_TO_END: theObject->mIgnoredTargets.erase(theObject->mIgnoredTargets.begin() + (size_t)a[0], theObject->mIgnoredTargets.end()); return 0;
    case EE_COMBAT_W_IGN_ERASE_RANGE: theObject->mIgnoredTargets.erase(theObject->mIgnoredTargets.begin() + (size_t)a[0], theObject->mIgnoredTargets.begin() + (size_t)a[1]); return 0;
    case EE_COMBAT_W_IGN_PUSH_FRONT: theObject->mIgnoredTargets.push_front(EEComplexUnitAttacker()); return 0;
    case EE_COMBAT_W_IGN_SET_TILE_X: theObject->mIgnoredTargets[0].mTileX = (long)a[0]; return 0;
    case EE_COMBAT_W_IGN_SET_TILE_Y: theObject->mIgnoredTargets[0].mTileY = (long)a[0]; return 0;
    case EE_COMBAT_W_IGN_SET_FROM_X: theObject->mIgnoredTargets[0].mFromX = (long)a[0]; return 0;
    case EE_COMBAT_W_IGN_SET_FROM_Y: theObject->mIgnoredTargets[0].mFromY = (long)a[0]; return 0;
    case EE_COMBAT_W_IGN_SET_UNIT_ID: theObject->mIgnoredTargets[0].mUnitID = (unsigned long)a[0]; return 0;
    case EE_COMBAT_W_IGN_SET_TIME: theObject->mIgnoredTargets[0].mTime = (unsigned long)a[0]; return 0;
    case EE_COMBAT_W_IGNORED_TIME: theObject->mIgnoredTargets[(size_t)a[0]].mTime = (unsigned long)a[1]; return 0;
    // the reach tests' continent queries: Rust (world_views.rs reach_op)
    case EE_COMBAT_W_OTHER_CONTINENT: return theObject->IsOnOtherContinentThan(theOther);
    case EE_COMBAT_W_SEEN_BY_OWNER: return EECombatIsSeenBy(theObject, EEQueryUnit(inOther)->GetPlayer(), true);
    case EE_COMBAT_W_AG_NEW_LOCATION: theContext->mAttackedGoal = EEUGRust::NewLocation(theOther->mPhysicalModel.GetLocation().GetX(), theOther->mPhysicalModel.GetLocation().GetY(), 0.0f, false, false, false, 0.0f, false, 0); return 0;
    case EE_COMBAT_W_AG_NEW_ATTACK: theContext->mAttackedGoal = EEUGRust::NewAttackUnit(theObject, theOther, false, false); return 0;
    case EE_COMBAT_W_AG_SET_GOAL: theObject->SetGoal(theContext->mAttackedGoal, 1, 0); return 0;
    case EE_COMBAT_W_NEAR_LIST: return (uint64_t)(uintptr_t)&theContext->mNear;
    case EE_COMBAT_W_NEAR_UNIT: return EEQueryHandleOf(theContext->mNear[(size_t)a[0]]);
    case EE_COMBAT_W_CAN_ATTACK_CLASS: return theObject->CanAttackClass(EEQueryUnit(inOther)->GetUnitClass());
    case EE_COMBAT_W_IS_IGNORED: return theObject->IsTargetIgnored(theOther, true);
    case EE_COMBAT_W_ATTACK_REACHABLE: return EEIsAttackReachable(theObject, theOther, &theContext->mTile);
    case EE_COMBAT_W_HELP_CLONE: theContext->mHelpGoal = theContext->mAttackedGoal->Clone(); return 0;
    case EE_COMBAT_W_HELP_HAS_ACTION: return theObject->GetClass()->FindAction(theContext->mHelpGoal->GetStartAction()) != NULL;
    case EE_COMBAT_W_HELP_SET_GOAL: theObject->SetGoal(theContext->mHelpGoal, 1, 0); return 0;
    case EE_COMBAT_W_HELP_START_ACTION: theObject->StartAction(theContext->mHelpGoal->GetStartAction()); return 0;
    case EE_COMBAT_W_ACTION_CHANGED: theObject->GetUnitClass()->ActionChanged(theObject); return 0;
    case EE_COMBAT_W_IGNORE: theObject->IgnoreTarget(theOther); return 0;
    case EE_COMBAT_W_HAS_LAST_ATTACKER: return theObject->mLastAttacker != NULL;
    case EE_COMBAT_W_DEREFERENCE_LAST_ATTACKER: theObject->mLastAttacker->Dereference(); return 0;
    case EE_COMBAT_W_CLEAR_LAST_ATTACKER: theObject->mLastAttacker = NULL; return 0;
    case EE_COMBAT_W_LAUNCH_STORED_MISSILE: theClass->LaunchStoredMissile(theObject); return 0;
    case EE_COMBAT_W_RELOAD: theObject->SetRandomReloadTimer((long)a[0]); return 0;
    case EE_COMBAT_W_FIND_DROP: return EEQueryHandleOf(EECombatFindDropLocation(theObject, NULL, theContext->mPoint));
    case EE_COMBAT_W_LINE_BLOCKED_AT: return EEIsLineToUnitBlocked(theObject, EEQueryUnit(inOther), EEObjectOf<EEUnit*>(a[0]), EEObjectOf<long>(a[1]), EEObjectOf<long>(a[2]));
    case EE_COMBAT_W_LAUNCH_SHOT: theClass->LaunchShot(theObject, EEQueryUnit(inOther), theContext->mPoint, NULL); return 0;
    case EE_COMBAT_W_LAUNCH_FROM_TURRET: theClass->LaunchFromTurret(theObject, EEQueryUnit(inOther), theContext->mPoint); return 0;
    case EE_COMBAT_W_START_ACTION: theObject->StartAction((unsigned long)a[0]); return 0;
    case EE_COMBAT_W_SHOT_EFFECT_AT: gWorld.GetEffectManager()->CreateEffect(theObject, theContext->mPoint, theClass->mShotEffect, NULL, 0); return 0;
    // damage.rs (R5b.2, tenth round): ApplyHit, ApplyAreaDamage, ComputeDamage
    case EE_COMBAT_W_DI_ATTACK_FACTOR: return EEConditionBits(gToHitTable.GetAttackFactor((long)a[0], EEQueryUnit(inOther)->mUnitClass, theObject->mPlayer->GetPlayerID(), EEQueryUnit(inOther)->mPlayer->GetPlayerID()));
    case EE_COMBAT_W_DI_WEAPON_FACTOR: return EEConditionBits(gWeaponToHitFactors[EEQueryUnit(inOther)->mUnitClass->mWeapon][a[0]]);
    case EE_COMBAT_W_RULE_MINIMUM_DAMAGE: return EEGetRules().mCombat.mMinimumDamage;
    case EE_COMBAT_W_RULE_FANATIC_FACTOR: return EEConditionBits(EERuleFloat(EEGetRules().mCombat.mFanaticFactor));
    case EE_COMBAT_W_RULE_ELEVATION_STEP: return EEConditionBits(EEGetRules().mCombat.mElevationStep);
    case EE_COMBAT_W_RULE_ALLY_FACTOR: return EEConditionBits(EEGetRules().mCombat.mAllyFactor);
    case EE_COMBAT_W_RULE_HERD_ALLY_FACTOR: return EEConditionBits(EEGetRules().mCombat.mHerdAllyFactor);
    case EE_COMBAT_W_RULE_TRAMPLE_DAMAGE: return EEGetRules().mCombat.mTrampleDamage;
    case EE_COMBAT_W_RULE_TRAMPLE_THRESHOLD: return EEConditionBits(EEGetRules().mCombat.mTrampleThreshold);
    case EE_COMBAT_W_RULE_DEAD_FOOD_SHARE: return EEConditionBits(EEGetRules().mCombat.mDeadFoodShare);
    case EE_COMBAT_W_ELEVATION: return EEConditionBits(theClass->GetElevationFactor(theObject, theOther));
    case EE_COMBAT_W_HC_CATEGORY: return theObject->mUnitClass->mCategory;
    case EE_COMBAT_W_HC_SPECIAL_ATTACK: return theObject->mIsSpecialAttack;
    case EE_COMBAT_W_HC_LAUNCHER: return (uint64_t)(uintptr_t)theObject->mLauncherClass;
    case EE_COMBAT_W_HC_SPECIAL_WEAPON: return (uint64_t)(uintptr_t)gUnitClassTable[kUnitTypeCapitalShipSpecialWeapon];
    case EE_COMBAT_W_HC_UPGRADE: return (uint64_t)(uintptr_t)theObject->mPlayer->GetUpgrade(EEObjectOf<EEUnitClass>(a[0])->mUnitType);
    case EE_COMBAT_W_HC_UPGRADE_ATTACK: return EEConditionBits(EEObjectOf<EEUnitClassUpgrade>(a[0])->GetAttack());
    case EE_COMBAT_W_HC_UPGRADE_AREA: return EEConditionBits(EEObjectOf<EEUnitClassUpgrade>(a[0])->mAreaEffect);
    case EE_COMBAT_W_HC_CLASS_ATTACK: return EEConditionBits(EEObjectOf<EEUnitClass>(a[0])->mAttack);
    case EE_COMBAT_W_HC_CLASS_AREA: return EEConditionBits(EEObjectOf<EEUnitClass>(a[0])->mAreaEffect);
    case EE_COMBAT_W_HC_CLASS_AMMO: return EEObjectOf<EEUnitClass>(a[0])->mAmmoType;
    case EE_COMBAT_W_HC_CLASS_WEAPON: return EEObjectOf<EEUnitClass>(a[0])->mWeapon;
    case EE_COMBAT_W_HC_CLASS_CATEGORY: return EEObjectOf<EEUnitClass>(a[0])->mCategory;
    case EE_COMBAT_W_HC_CLASS_FRIENDLY: return EEConditionBits(EEObjectOf<EEUnitClass>(a[0])->mFriendlyDamageFactor);
    case EE_COMBAT_W_HC_SHOT_EFFECT: return theClass->mShotEffect != NULL;
    case EE_COMBAT_W_UE_CLEAR: theContext->mEffect = NULL; return 0;
    case EE_COMBAT_W_UE_GET: return theObject->GetEffect(3, theContext->mEffect);
    case EE_COMBAT_W_UE_HAS: return theContext->mEffect != NULL;
    case EE_COMBAT_W_UE_CREATE: gEffectManager->CreateUnitEffect(theObject, theContext->mEffect, NULL, false, 0, 0); return 0;
    case EE_COMBAT_W_TAKE_HIT: EEQueryUnit(inObject)->TakeHit(theOther, (long)a[0], theOther->mPlayer->GetPlayerID(), 1); return 0;
    case EE_COMBAT_W_UNIT_DAMAGED: EEQueryUnit(inObject)->mUnitClass->UnitDamaged(EEQueryUnit(inObject)->GetComplexUnit(), theOther, (long)a[0]); return 0;
    case EE_COMBAT_W_PATH_START_AT: theObject->GetPathStart(*EEObjectOf<GE3DPoint>(a[0]), *EEObjectOf<float>(a[1])); return 0;
    case EE_COMBAT_W_NEW_TARGET_NEAR_OLD: EEFindNewTargetNearOld(theObject, theOther); return 0;
    case EE_COMBAT_W_RING_TILE_X: return EEQueryUnit(inOther)->GetTileX();
    case EE_COMBAT_W_RING_TILE_Y: return EEQueryUnit(inOther)->GetTileY();
    case EE_COMBAT_W_RING_TILE_SIZE: return EEQueryUnit(inOther)->GetUnitClass()->GetTileSize();
    case EE_COMBAT_W_FOOTPRINT: ee_units_uu_unit_tiles(0, (uint64_t)(uintptr_t)(EEQueryUnit(inObject)), &theContext->mFootprint); return theContext->mFootprint.size();
    case EE_COMBAT_W_FOOTPRINT_X: return theContext->mFootprint[(size_t)a[0]].mX;
    case EE_COMBAT_W_FOOTPRINT_Y: return theContext->mFootprint[(size_t)a[0]].mY;
    case EE_COMBAT_W_TS_TILE_AT_LOCATION: return (uint64_t)(uintptr_t)::gWorld.mMap->GetTileAtLocation(EETileLocation((long)a[0], (long)a[1]));
    case EE_COMBAT_W_TS_TILE: return (uint64_t)(uintptr_t)::gWorld.mMap->GetTile((unsigned long)a[0], (unsigned long)a[1]);
    case EE_COMBAT_W_TS_SEEN: return EEObjectOf<EETile>(a[0])->IsSeenBy(EEObjectOf<EEPlayer>((int64_t)inObject));
    case EE_COMBAT_W_TS_VISIBLE: return EEObjectOf<EETile>(a[0])->GetVisibility(EEObjectOf<EEPlayer>((int64_t)inObject)->GetPlayerID()) > 1;
    case EE_COMBAT_W_GOAL_LOCATION_AT: theObject->mActiveGoal->GetGoalLocation(*EEObjectOf<GE3DPoint>(a[0])); return 0;
    case EE_COMBAT_W_DM_HAS_MODEL: return EEQueryUnit(inObject)->GetModel()->mModel != NULL;
    case EE_COMBAT_W_DM_DEFAULT_POINT: *EEObjectOf<GE3DPoint>(a[0]) = EEQueryUnit(inObject)->GetModel()->mDefaultPoint; return 0;
    }
    return 0;
}

struct EEFacingContext {
    EEComplexUnit* mUnit;
    EEGraphicObject* mModel;
};

void EEFacingTurret(void* inContext, int32_t inIndex, EeCombatTurret* outTurret)
{
    EEFacingContext* theContext = (EEFacingContext*)inContext;
    EETurretPart* thePart = *(theContext->mModel->mTurretParts.begin() + inIndex);
    outTurret->position[0] = thePart->mPosition.GetX();
    outTurret->position[1] = thePart->mPosition.GetY();
    outTurret->position[2] = thePart->mPosition.GetZ();
    memcpy(outTurret->transformation, theContext->mModel->GetTurretPartTransformation(inIndex).mMatrix,
           sizeof(outTurret->transformation));
    outTurret->angle = theContext->mUnit->mTurretAngles[inIndex];
}

}   // namespace

EEUnit* EECombatFindImpactUnit(EEComplexUnit* inUnit)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return EEQueryUnit(ee_combat_impact_unit(EEQueryHandleOf(inUnit), &theHost));
}

int32_t EECombatWorld(int32_t inFunction, EEComplexUnit* inUnit, EEUnit* inTarget, EEUnitClass* inClass, long* ioX,
                      long* ioY)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    theContext.mClass = inClass;
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp, (uint64_t)(uintptr_t)theContext.mClass};
    int32_t theTile[2] = {ioX != NULL ? (int32_t)*ioX : 0, ioY != NULL ? (int32_t)*ioY : 0};
    int32_t theResult = ee_combat_world(inFunction, EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), &theHost, &theTile);
    if (ioX != NULL) {
        *ioX = theTile[0];
        *ioY = theTile[1];
    }
    return theResult;
}

bool EECombatComputeDamage(int inForm, EEComplexUnit* inUnit, EEUnit* inTarget, float inAttack, long inAmmo, long inWeapon,
                           float inFriendlyFactor, long& outAmount)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    theContext.mClass = inUnit->mUnitClass;
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp, (uint64_t)(uintptr_t)theContext.mClass};
    EeCombatDamageCall theCall = {inForm, inAttack, (int32_t)inAmmo, (int32_t)inWeapon, inFriendlyFactor, 0};
    if (!ee_combat_compute_damage(&theCall, EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), &theHost))
        return false;
    outAmount = theCall.amount;
    return true;
}

void EECombatAttackPosition(bool inFiring, EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& ioLocation)
{
    EEQueryContext theContext = {inAttacker, NULL, NULL};
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp, (uint64_t)(uintptr_t)theContext.mClass};
    float thePoint[3] = {ioLocation.GetX(), ioLocation.GetY(), ioLocation.GetZ()};
    ee_combat_attack_position(inFiring, EEQueryHandleOf(inAttacker), EEQueryHandleOf(inTarget), &theHost, &thePoint);
    ioLocation = GE3DPoint(thePoint[0], thePoint[1], thePoint[2]);
}

EEUnit* EECombatFindDropLocation(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& outLocation)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp, (uint64_t)(uintptr_t)theContext.mClass};
    float thePoint[3];
    EEUnit* theTarget = EEQueryUnit(ee_combat_find_drop_location(EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget),
                                                                 &theHost, &thePoint));
    outLocation = GE3DPoint(thePoint[0], thePoint[1], thePoint[2]);
    return theTarget;
}

float EECombatHitDistanceSquared(EEComplexUnit* inUnit, EEUnit* inTarget, float inX, float inY)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp, (uint64_t)(uintptr_t)theContext.mClass};
    return ee_combat_hit_distance_squared(EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), inX, inY, &theHost);
}

bool EECombatIsSeenBy(EEUnit* inUnit, EEPlayer* inPlayer, bool inExplored)
{
    EEQueryContext theContext = {NULL, NULL, NULL};
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp, (uint64_t)(uintptr_t)theContext.mClass};
    return ee_combat_is_seen_by(EEQueryHandleOf(inUnit), (uint64_t)(uintptr_t)inPlayer, inExplored, &theHost);
}

bool EECombatIsInReach(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return ee_combat_in_reach(EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), &theHost) != 0;
}

// EEUGAttackUnit::IsStillReachable (CBQR: the path attempts and the ignored targets' count after)
bool EECombatIsStillReachable(EEComplexUnit* inUnit, EEUGRust* inGoal)
{
    EECombatHostReadScope theScope;
    EEUnit* theTarget = inGoal->GetAttackTarget();
    EEQueryContext theContext = {inUnit, inGoal, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    bool theReachable = ee_combat_still_reachable(EEQueryHandleOf(inUnit), &theHost) != 0;
    if (theScope.mOuter)
        EECombatCaptureHostRead(EE_COMBAT_HR_STILL_REACHABLE, inUnit, theTarget, theReachable, (uint32_t)inGoal->mPathAttempts,
                                (uint32_t)inUnit->mIgnoredTargets.size());
    return theReachable;
}

// EEUGAttackUnit::IsGateClosed (CBQR)
bool EECombatIsGateClosed(EEComplexUnit* inUnit, EEUGRust* inGoal)
{
    EECombatHostReadScope theScope;
    EEQueryContext theContext = {inUnit, inGoal, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    bool theClosed = ee_combat_gate_closed(EEQueryHandleOf(inUnit), &theHost) != 0;
    if (theScope.mOuter)
        EECombatCaptureHostRead(EE_COMBAT_HR_GATE_CLOSED, inUnit, inGoal->GetAttackTarget(), theClosed);
    return theClosed;
}

bool EECombatHealInRange(EEComplexUnit* inHealer, EEUnit* inTarget)
{
    EEQueryContext theContext = {inHealer, NULL, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return ee_combat_heal_in_range(EEQueryHandleOf(inHealer), EEQueryHandleOf(inTarget), &theHost) != 0;
}

bool EECombatIsEnemyWithinRange(EEComplexUnit* inUnit)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return ee_combat_enemy_in_range(EEQueryHandleOf(inUnit), &theHost) != 0;
}

bool EECombatFacesPoint(EEUnitClass* inClass, EEComplexUnit* inUnit, const GE3DPoint& inLocation, bool inUseTolerance)
{
    EeCombatFacingIn in;
    memset(&in, 0, sizeof(in));
    memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    in.point[0] = inLocation.GetX();
    in.point[1] = inLocation.GetY();
    in.use_tolerance = inUseTolerance;
    in.firing_arc = inClass->mFiringArc;
    in.facing_offset = inClass->mFacingOffset;
    in.fires_broadside = inClass->mFiresBroadside;
    in.unit_type = (int32_t)inUnit->mUnitClass->GetUnitType();
    in.special_attack = inUnit->mIsSpecialAttack;
    EEGraphicObject* theModel = inUnit->GetModel();
    in.turrets = (int32_t)theModel->GetNumParts();
    if (in.turrets > 0)
        in.scale = inUnit->mStats->GetScale();
    EEFacingContext theContext = {inUnit, theModel};
    return ee_combat_faces_point(&in, &theContext, EEFacingTurret) != 0;
}
