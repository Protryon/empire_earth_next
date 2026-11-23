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

// (the depth is ee_sim combat cg_query.rs's)
EECombatHostReadScope::EECombatHostReadScope(int inSlot) : mSlot(inSlot), mOuter(ee_combat_host_read_enter(inSlot)) {}
EECombatHostReadScope::~EECombatHostReadScope() { ee_combat_host_read_leave(this->mSlot); }

uint32_t EECombatCommandOf(const EEComplexUnit* inUnit)
{
    return inUnit->mCommand != NULL ? (uint32_t)inUnit->mCommand->mType : 0;
}

void EECombatCaptureIgnored(uint32_t inFunc, const EEComplexUnit* inUnit, const EEComplexUnit* inTarget, int32_t inResult,
                            uint32_t inCheckLocation)
{
    ee_units_cgq_capture_ignored(inFunc, (uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)inTarget, inResult, inCheckLocation);   // cg_query.rs
}

void EECombatCaptureFlush()
{
    ee_combat_capture_fold_flush();
}

// ---- the hosts of ee_combat's trigger tests and steps

namespace {

uint32_t EEConditionBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

}   // namespace

EEUnit* EECombatShotTarget(EEComplexUnit* inUnit)
{
    return (EEUnit*)(uintptr_t)ee_units_cgq_shot_target(EEObj(inUnit));   // cg_query.rs
}

// The trigger tests and the steps of the combat goals: their records, lazy reads, heal facing fallback and CBGS
// record are ee_sim combat cg_goal.rs over ee_combat's goals.rs; each C++ statement they ask is one case here
long EECombatCondition(int32_t inKind, EEComplexUnit* inUnit, unsigned long inEvent, EEUnit* inTarget,
                       EEUnitGoal* inGoal, EEUnitClass* inClass)
{
    return ee_units_cg_condition(inKind, EEObj(inUnit), (uint32_t)inEvent, EEObj(inTarget), EEObj(inGoal), EEObj(inClass));
}

void EECombatStep(int32_t inKind, EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEUnit* inTarget)
{
    ee_units_cg_step(inKind, EEObj(inUnit), EEObj(inGoal), EEObj(inTarget));
}

template <class T> static T* EECgObject(int64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

extern "C" int64_t ee_units_cg_host(int32_t inOp, uint64_t inUnit, uint64_t inA, int64_t inB, int64_t inC, float (*ioF)[4])
{
    EEComplexUnit* theUnit = EECgObject<EEComplexUnit>((int64_t)inUnit);
    EEUnit* theObject = EECgObject<EEUnit>((int64_t)inA);
    EEUGRust* theGoal = EECgObject<EEUGRust>((int64_t)inA);
    EEUnitClass* theClass = EECgObject<EEUnitClass>((int64_t)inA);
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_CG_TARGET_COMPLEX: return EEObj(theObject->mComplexUnit);
    case EE_UNITS_CG_MODEL_PARTS: return (int32_t)theUnit->GetModel()->GetNumParts();
    case EE_UNITS_CG_STATS_SCALE: f[0] = theUnit->mStats->GetScale(); return 0;
    case EE_UNITS_CG_HUMAN: return theUnit->mPlayer != NULL && theUnit->mPlayer->IsHuman();
    case EE_UNITS_CG_ATTACK_TILE_X: return theGoal->GetAttackTileX();
    case EE_UNITS_CG_ATTACK_TILE_Y: return theGoal->GetAttackTileY();
    case EE_UNITS_CG_CONVERT: return theGoal->IsConvertAttack();
    case EE_UNITS_CG_ATTACK_IMPACT: theUnit->SetGoal(EEUGRust::NewAttackUnit(theUnit, theObject, false, false), 0, 0); return 0;
    case EE_UNITS_CG_LINE_OF_SIGHT: f[0] = theUnit->GetLineOfSight(); return 0;
    case EE_UNITS_CG_HEAL_TARGET: return EEObj(theGoal->GetHealTarget());
    case EE_UNITS_CG_ATTACK_TARGET: return EEObj(theGoal->GetAttackTarget());
    case EE_UNITS_CG_IN_RANGE_TO_HIT: return EECombatWorld(EE_COMBAT_WORLD_IN_RANGE_TO_HIT, theUnit, theObject) != 0;
    case EE_UNITS_CG_IN_REACH: return theUnit->IsInReach(theObject);
    case EE_UNITS_CG_SEEN: return EECombatIsSeenBy(theObject, theUnit->mPlayer, inB != 0);
    case EE_UNITS_CG_FACES: return theUnit->mUnitClass->FacesTarget(theUnit, theObject, inB != 0);
    case EE_UNITS_CG_DISTANCE_SQUARED: f[0] = theUnit->DistanceSquaredToUnit(theObject); return 0;
    case EE_UNITS_CG_BORDERING: return theUnit->IsOnTileBorderingTarget(theObject);
    case EE_UNITS_CG_GATE_CLOSED: return EECombatIsGateClosed(theUnit, theGoal);
    case EE_UNITS_CG_STILL_REACHABLE: return EECombatIsStillReachable(theUnit, theGoal);
    case EE_UNITS_CG_ENEMY_IN_RANGE: return EEIsEnemyWithinRange(theUnit);
    case EE_UNITS_CG_MISSILE_CONDITION: return (uint32_t)theClass->MissileEvaluateCondition(theUnit, (unsigned long)inB);
    case EE_UNITS_CG_GROUND_TARGET: *EECgObject<GE3DPoint>(inB) = theGoal->GetGroundTarget(); return 0;
    case EE_UNITS_CG_CAN_REACH_TILE: return theUnit->CanReachTile((long)inB, (long)inC);
    case EE_UNITS_CG_FACES_POINT: return theUnit->GetUnitClass()->FacesPoint(theUnit, theGoal->GetGroundTarget(), true);
    case EE_UNITS_CG_HEAL_IN_RANGE: return EECombatHealInRange(theUnit, theObject);
    case EE_UNITS_CG_REPAIRABLE: return EECgObject<EEComplexUnit>((int64_t)inA)->IsRepairable();
    case EE_UNITS_CG_FIND_IMPACT: return EEObj(theClass->FindImpactUnit(theUnit));
    case EE_UNITS_CG_CLASS: return EEObj(theObject->GetUnitClass());
    case EE_UNITS_CG_GROUND_IMPACT: return theClass->CheckGroundImpact(theUnit);
    case EE_UNITS_CG_WATER_IMPACT: return theClass->CheckWaterImpact(theUnit);
    case EE_UNITS_CG_LOCATION: memcpy(EECgObject<float>(inB), theUnit->mPhysicalModel.GetLocation().mMatrix, 16 * sizeof(float)); return 0;
    case EE_UNITS_CG_SET_TRANSFORMATION: theUnit->SetTransformation(*EECgObject<GETransformation>(inB), true); return 0;
    case EE_UNITS_CG_PHYSICS_ACTION: theUnit->SetCurrentPhysicsAction((unsigned long)inB); return 0;
    case EE_UNITS_CG_ANIMATION: return (int32_t)theUnit->mAnimation;
    case EE_UNITS_CG_SET_ANIMATION: theUnit->SetAnimation((long)inB, -1, 0); return 0;
    case EE_UNITS_CG_DROP_PATH: theUnit->DropPath(); return 0;
    case EE_UNITS_CG_RETARGET_GATE: theGoal->RetargetGatePart(theUnit); return 0;
    case EE_UNITS_CG_ATTACK_FRAME_SET: return (int32_t)EEGetAttackFrameSet(theUnit);
    case EE_UNITS_CG_SOUND_FRAME: return (int32_t)theUnit->GetModel()->mSound1Frames[inB];
    case EE_UNITS_CG_FRAME: return (int32_t)theUnit->mPhysicalModel.GetFrame();
    case EE_UNITS_CG_FIRE_SHOT: theUnit->mUnitClass->FireShot(theUnit); return 0;
    case EE_UNITS_CG_RELOAD_TIMER: theUnit->SetRandomReloadTimer((long)inB); return 0;
    case EE_UNITS_CG_SEARCH_TIMER: theUnit->SetTargetSearchTimer((long)inB); return 0;
    case EE_UNITS_CG_PATH_ATTEMPTS: theGoal->SetPathAttempts((long)inB); return 0;
    case EE_UNITS_CG_FIRING_HIGH: return theUnit->mIsFiringHigh;
    }
    return 0;
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
    case EE_COMBAT_Q_ATTACK_REACHABLE: return ee_units_cgq_attack_reachable_from_zero(EEQueryHandleOf(theUnit), inObject);
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
    case EE_COMBAT_Q_CONTACT_RANGE: return ee_units_cgq_in_contact_range_bits(EEQueryHandleOf(theUnit), inObject, inArg);
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
    case EE_COMBAT_H_GOAL_TARGET: return ee_units_cgq_shot_target(inObject);   // its attack-unit goal's target (cg_query.rs)
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

}   // namespace

// FacesPoint's turret part inIndex of the unit's model (ee_sim combat cg_query.rs faces_point)
extern "C" void ee_units_cg_facing_turret(uint64_t inUnit, int32_t inIndex, EeCombatTurret* outTurret)
{
    EEComplexUnit* theUnit = EEQueryComplex(inUnit);
    EEGraphicObject* theModel = theUnit->GetModel();
    EETurretPart* thePart = *(theModel->mTurretParts.begin() + inIndex);
    outTurret->position[0] = thePart->mPosition.GetX();
    outTurret->position[1] = thePart->mPosition.GetY();
    outTurret->position[2] = thePart->mPosition.GetZ();
    memcpy(outTurret->transformation, theModel->GetTurretPartTransformation(inIndex).mMatrix, sizeof(outTurret->transformation));
    outTurret->angle = theUnit->mTurretAngles[inIndex];
}

// The query entry points: ee_sim combat cg_query.rs (the marshalling and the CBQR records) over one C++ context and
// host per call, made here
extern "C" void ee_units_cg_query(uint64_t inUnit, uint64_t inGoal, uint64_t inClass, void* inData,
                                  void (*inBody)(void*, const EeCombatWorldHost*))
{
    EEQueryContext theContext = {EEQueryComplex(inUnit), EECgObject<EEUGRust>((int64_t)inGoal), NULL};
    theContext.mClass = EECgObject<EEUnitClass>((int64_t)inClass);
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp, inClass};
    inBody(inData, &theHost);
}

EEUnit* EECombatFindImpactUnit(EEComplexUnit* inUnit)
{
    return EEQueryUnit(ee_units_cgq_impact_unit(EEQueryHandleOf(inUnit)));
}

int32_t EECombatWorld(int32_t inFunction, EEComplexUnit* inUnit, EEUnit* inTarget, EEUnitClass* inClass, long* ioX,
                      long* ioY)
{
    return ee_units_cgq_world(inFunction, EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), EEObj(inClass), ioX, ioY);
}

bool EECombatComputeDamage(int inForm, EEComplexUnit* inUnit, EEUnit* inTarget, float inAttack, long inAmmo, long inWeapon,
                           float inFriendlyFactor, long& outAmount)
{
    return ee_units_cgq_compute_damage(inForm, EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), EEObj(inUnit->mUnitClass),
                                       inAttack, inAmmo, inWeapon, inFriendlyFactor, &outAmount);
}

void EECombatAttackPosition(bool inFiring, EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& ioLocation)
{
    ee_units_cgq_attack_position(inFiring, EEQueryHandleOf(inAttacker), EEQueryHandleOf(inTarget), (float(*)[3])&ioLocation.mX);
}

EEUnit* EECombatFindDropLocation(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& outLocation)
{
    return EEQueryUnit(ee_units_cgq_find_drop_location(EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), (float(*)[3])&outLocation.mX));
}

float EECombatHitDistanceSquared(EEComplexUnit* inUnit, EEUnit* inTarget, float inX, float inY)
{
    return ee_units_cgq_hit_distance_squared(EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), inX, inY);
}

bool EECombatIsSeenBy(EEUnit* inUnit, EEPlayer* inPlayer, bool inExplored)
{
    return ee_units_cgq_is_seen_by(EEQueryHandleOf(inUnit), (uint64_t)(uintptr_t)inPlayer, inExplored);
}

bool EECombatIsInReach(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    return ee_units_cgq_in_reach(EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget));
}

// (with its CBQR record)
bool EECombatIsStillReachable(EEComplexUnit* inUnit, EEUGRust* inGoal)
{
    return ee_units_cgq_still_reachable(EEQueryHandleOf(inUnit), EEObj(inGoal));
}

// (with its CBQR record)
bool EECombatIsGateClosed(EEComplexUnit* inUnit, EEUGRust* inGoal)
{
    return ee_units_cgq_gate_closed(EEQueryHandleOf(inUnit), EEObj(inGoal));
}

bool EECombatHealInRange(EEComplexUnit* inHealer, EEUnit* inTarget)
{
    return ee_units_cgq_heal_in_range(EEQueryHandleOf(inHealer), EEQueryHandleOf(inTarget));
}

bool EECombatIsEnemyWithinRange(EEComplexUnit* inUnit)
{
    return ee_units_cgq_enemy_in_range(EEQueryHandleOf(inUnit));
}

bool EECombatFacesPoint(EEUnitClass* inClass, EEComplexUnit* inUnit, const GE3DPoint& inLocation, bool inUseTolerance)
{
    return ee_units_cgq_faces_point(EEObj(inClass), EEQueryHandleOf(inUnit), inLocation.GetX(), inLocation.GetY(), inUseTolerance);
}
