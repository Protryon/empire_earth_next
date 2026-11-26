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
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERulesWorld.h"
#include "ee/EEUnitBehavior.h"
#include "rust/sim/units.h"

extern EEToHitTable gToHitTable;           // 0x5318e8 (Empire Earth.cpp; as EECPAttackSubGroupUnit.cpp declares it)

// ---- the hosts of ee_combat's trigger tests and steps

// The trigger tests and the steps of the combat goals (EECombatCondition, EECombatStep: ee/EECombatGoals.h): their
// records, lazy reads, heal facing fallback and CBGS record are ee_sim combat cg_goal.rs over ee_combat's goals.rs;
// each C++ statement they ask is one case here

template <class T> static T* EECgObject(int64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

extern "C" int64_t ee_units_cg_host(int32_t inOp, uint64_t inUnit, uint64_t inA, int64_t inB, int64_t inC, float (*ioF)[4])
{
    switch (inOp) {
    case EE_UNITS_CG_TARGET_COMPLEX: return EEObj(EECgObject<EEUnit>((int64_t)inA)->mComplexUnit);
    case EE_UNITS_CG_MODEL_PARTS: return (int32_t)EECgObject<EEComplexUnit>((int64_t)inUnit)->GetModel()->GetNumParts();
    case EE_UNITS_CG_STATS_SCALE: (*ioF)[0] = EECgObject<EEComplexUnit>((int64_t)inUnit)->mStats->GetScale(); return 0;
    case EE_UNITS_CG_HUMAN: return EECgObject<EEComplexUnit>((int64_t)inUnit)->mPlayer != NULL && EECgObject<EEComplexUnit>((int64_t)inUnit)->mPlayer->IsHuman();
    case EE_UNITS_CG_ATTACK_TILE_X: return EECgObject<EEUGRust>((int64_t)inA)->GetAttackTileX();
    case EE_UNITS_CG_ATTACK_TILE_Y: return EECgObject<EEUGRust>((int64_t)inA)->GetAttackTileY();
    case EE_UNITS_CG_CONVERT: return EECgObject<EEUGRust>((int64_t)inA)->IsConvertAttack();
    case EE_UNITS_CG_ATTACK_IMPACT: EECgObject<EEComplexUnit>((int64_t)inUnit)->SetGoal(EEUGRust::NewAttackUnit(EECgObject<EEComplexUnit>((int64_t)inUnit), EECgObject<EEUnit>((int64_t)inA), false, false), 0, 0); return 0;
    case EE_UNITS_CG_LINE_OF_SIGHT: (*ioF)[0] = EECgObject<EEComplexUnit>((int64_t)inUnit)->GetLineOfSight(); return 0;
    case EE_UNITS_CG_HEAL_TARGET: return EEObj(EECgObject<EEUGRust>((int64_t)inA)->GetHealTarget());
    case EE_UNITS_CG_ATTACK_TARGET: return EEObj(EECgObject<EEUGRust>((int64_t)inA)->GetAttackTarget());
    case EE_UNITS_CG_IN_RANGE_TO_HIT: return EECombatWorld(EE_COMBAT_WORLD_IN_RANGE_TO_HIT, EECgObject<EEComplexUnit>((int64_t)inUnit), EECgObject<EEUnit>((int64_t)inA)) != 0;
    case EE_UNITS_CG_IN_REACH: return EECgObject<EEComplexUnit>((int64_t)inUnit)->IsInReach(EECgObject<EEUnit>((int64_t)inA));
    case EE_UNITS_CG_SEEN: return EECombatIsSeenBy(EECgObject<EEUnit>((int64_t)inA), EECgObject<EEComplexUnit>((int64_t)inUnit)->mPlayer, inB != 0);
    case EE_UNITS_CG_FACES: return EECgObject<EEComplexUnit>((int64_t)inUnit)->mUnitClass->FacesTarget(EECgObject<EEComplexUnit>((int64_t)inUnit), EECgObject<EEUnit>((int64_t)inA), inB != 0);
    case EE_UNITS_CG_DISTANCE_SQUARED: (*ioF)[0] = EECgObject<EEComplexUnit>((int64_t)inUnit)->DistanceSquaredToUnit(EECgObject<EEUnit>((int64_t)inA)); return 0;
    case EE_UNITS_CG_BORDERING: return EECgObject<EEComplexUnit>((int64_t)inUnit)->IsOnTileBorderingTarget(EECgObject<EEUnit>((int64_t)inA));
    case EE_UNITS_CG_GATE_CLOSED: return EECombatIsGateClosed(EECgObject<EEComplexUnit>((int64_t)inUnit), EECgObject<EEUGRust>((int64_t)inA));
    case EE_UNITS_CG_STILL_REACHABLE: return EECombatIsStillReachable(EECgObject<EEComplexUnit>((int64_t)inUnit), EECgObject<EEUGRust>((int64_t)inA));
    case EE_UNITS_CG_ENEMY_IN_RANGE: return EEIsEnemyWithinRange(EECgObject<EEComplexUnit>((int64_t)inUnit));
    case EE_UNITS_CG_MISSILE_CONDITION: return (uint32_t)EECgObject<EEUnitClass>((int64_t)inA)->MissileEvaluateCondition(EECgObject<EEComplexUnit>((int64_t)inUnit), (unsigned long)inB);
    case EE_UNITS_CG_GROUND_TARGET: *EECgObject<GE3DPoint>(inB) = EECgObject<EEUGRust>((int64_t)inA)->GetGroundTarget(); return 0;
    case EE_UNITS_CG_CAN_REACH_TILE: return EECgObject<EEComplexUnit>((int64_t)inUnit)->CanReachTile((long)inB, (long)inC);
    case EE_UNITS_CG_FACES_POINT: return EECgObject<EEComplexUnit>((int64_t)inUnit)->GetUnitClass()->FacesPoint(EECgObject<EEComplexUnit>((int64_t)inUnit), EECgObject<EEUGRust>((int64_t)inA)->GetGroundTarget(), true);
    case EE_UNITS_CG_HEAL_IN_RANGE: return EECombatHealInRange(EECgObject<EEComplexUnit>((int64_t)inUnit), EECgObject<EEUnit>((int64_t)inA));
    case EE_UNITS_CG_REPAIRABLE: return EECgObject<EEComplexUnit>((int64_t)inA)->IsRepairable();
    case EE_UNITS_CG_FIND_IMPACT: return EEObj(EECgObject<EEUnitClass>((int64_t)inA)->FindImpactUnit(EECgObject<EEComplexUnit>((int64_t)inUnit)));
    case EE_UNITS_CG_CLASS: return EEObj(EECgObject<EEUnit>((int64_t)inA)->GetUnitClass());
    case EE_UNITS_CG_GROUND_IMPACT: return EECgObject<EEUnitClass>((int64_t)inA)->CheckGroundImpact(EECgObject<EEComplexUnit>((int64_t)inUnit));
    case EE_UNITS_CG_WATER_IMPACT: return EECgObject<EEUnitClass>((int64_t)inA)->CheckWaterImpact(EECgObject<EEComplexUnit>((int64_t)inUnit));
    case EE_UNITS_CG_LOCATION: memcpy(EECgObject<float>(inB), EECgObject<EEComplexUnit>((int64_t)inUnit)->mPhysicalModel.GetLocation().mMatrix, 16 * sizeof(float)); return 0;
    case EE_UNITS_CG_SET_TRANSFORMATION: EECgObject<EEComplexUnit>((int64_t)inUnit)->SetTransformation(*EECgObject<GETransformation>(inB), true); return 0;
    case EE_UNITS_CG_PHYSICS_ACTION: EECgObject<EEComplexUnit>((int64_t)inUnit)->SetCurrentPhysicsAction((unsigned long)inB); return 0;
    case EE_UNITS_CG_ANIMATION: return (int32_t)EECgObject<EEComplexUnit>((int64_t)inUnit)->mAnimation;
    case EE_UNITS_CG_SET_ANIMATION: EECgObject<EEComplexUnit>((int64_t)inUnit)->SetAnimation((long)inB, -1, 0); return 0;
    case EE_UNITS_CG_DROP_PATH: EECgObject<EEComplexUnit>((int64_t)inUnit)->DropPath(); return 0;
    case EE_UNITS_CG_RETARGET_GATE: EECgObject<EEUGRust>((int64_t)inA)->RetargetGatePart(EECgObject<EEComplexUnit>((int64_t)inUnit)); return 0;
    case EE_UNITS_CG_ATTACK_FRAME_SET: return (int32_t)EEGetAttackFrameSet(EECgObject<EEComplexUnit>((int64_t)inUnit));
    case EE_UNITS_CG_SOUND_FRAME: return (int32_t)EECgObject<EEComplexUnit>((int64_t)inUnit)->GetModel()->mSound1Frames[inB];
    case EE_UNITS_CG_FRAME: return (int32_t)EECgObject<EEComplexUnit>((int64_t)inUnit)->mPhysicalModel.GetFrame();
    case EE_UNITS_CG_FIRE_SHOT: EECgObject<EEComplexUnit>((int64_t)inUnit)->mUnitClass->FireShot(EECgObject<EEComplexUnit>((int64_t)inUnit)); return 0;
    case EE_UNITS_CG_RELOAD_TIMER: ee_units_cu_body(EE_UNITS_CU_DO_SET_RANDOM_RELOAD_TIMER, EECgObject<EEComplexUnit>((int64_t)inUnit), (long)inB, 0, 0); return 0;
    case EE_UNITS_CG_SEARCH_TIMER: EECgObject<EEComplexUnit>((int64_t)inUnit)->SetTargetSearchTimer((long)inB); return 0;
    case EE_UNITS_CG_PATH_ATTEMPTS: ee_units_goal_set_path_attempts(EE_UNITS_GOAL_SET_PATH_ATTEMPTS, reinterpret_cast<EeUnitsGoalBase*>(EECgObject<EEUGRust>((int64_t)inA)), (long)inB); return 0;
    case EE_UNITS_CG_FIRING_HIGH: return EECgObject<EEComplexUnit>((int64_t)inUnit)->mIsFiringHigh;
    // FacesPoint's turret part inC of the unit's model (ee_sim combat cg_query.rs faces_point)
    case EE_UNITS_CG_TURRET_POSITION: memcpy(EECgObject<float>(inB), &(*(EECgObject<EEComplexUnit>((int64_t)inUnit)->GetModel()->mTurretParts.begin() + inC))->mPosition, 3 * sizeof(float)); return 0;
    case EE_UNITS_CG_TURRET_TRANSFORMATION: memcpy(EECgObject<float>(inB), EECgObject<EEComplexUnit>((int64_t)inUnit)->GetModel()->GetTurretPartTransformation((long)inC).mMatrix, 16 * sizeof(float)); return 0;
    case EE_UNITS_CG_TURRET_ANGLE: (*ioF)[0] = EECgObject<EEComplexUnit>((int64_t)inUnit)->mTurretAngles[inC]; return 0;
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
    EEUnitClass* mClass = NULL;             // world.rs: the class of Attacked or FireShot
    EEUnitGoal* mAttackedGoal = NULL;       // Attacked's goal (the helpers' clones)
    std::vector<EEUnit*> mNear;             // Attacked's units near the attacked unit
    U2DPoint<long> mTile;                   // Attacked's EEIsAttackReachable tile (one for all helpers, as the original)
    GE3DPoint mPoint;                       // FireShot's drop point
    std::vector<EETileCoordinate> mFootprint;   // EEIsSeenBy's footprint tiles
    EEUnitGoal* mHelpGoal = NULL;           // Attacked's helper's clone of mAttackedGoal
    EEGraphicEffect* mEffect = NULL;        // ApplyHit's unit effect (GetEffect(3))
};

template <class T> T* EEObjectOf(int64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

EEQueryContext* EEQueryContextOf(void* inContext) { return (EEQueryContext*)inContext; }

EEUnit* EEQueryUnit(uint64_t inHandle) { return (EEUnit*)(uintptr_t)inHandle; }

// a handle whose complex members are read (every one the original reads them of is a complex unit)
EEComplexUnit* EEQueryComplex(uint64_t inHandle) { return static_cast<EEComplexUnit*>(EEQueryUnit(inHandle)); }

uint64_t EEQueryHandleOf(const EEUnit* inUnit) { return (uint64_t)(uintptr_t)inUnit; }

// the tag points of a unit's model at its animation (NULL: none)
EETagPointList* EEQueryTags(EEComplexUnit* inUnit) { return inUnit->GetModel()->mTagPoints[inUnit->mAnimation]; }

// what the class of FireShot has: a projectile class, ammunition, a muzzle flash, a shot effect
uint32_t EEQueryShotClass(EEUnitClass* inClass)
{
    return (inClass->mProjectileClass != NULL ? EE_COMBAT_SHOT_CLASS_PROJECTILE : 0) | (inClass->mUsesAmmunition ? EE_COMBAT_SHOT_CLASS_AMMUNITION : 0) | (inClass->mMuzzleFlashEffect != NULL ? EE_COMBAT_SHOT_CLASS_MUZZLE_FLASH : 0) | (inClass->mShotEffect != NULL ? EE_COMBAT_SHOT_CLASS_SHOT_EFFECT : 0);
}

// (the plain member reads are Rust's: ee_combat query_views.rs)
uint32_t EEQueryRead(void* inContext, int32_t inCode, uint64_t inObject, int32_t inArg)
{
    switch (inCode) {
    case EE_COMBAT_Q_MODEL: return EEQueryComplex(inObject)->mPhysicalModel.GetModel() != NULL;
    case EE_COMBAT_Q_TARGETABLE: return ee_units_uu_targetable((uint64_t)(uintptr_t)(inArg ? EEQueryContextOf(inContext)->mUnit : NULL), (uint64_t)(uintptr_t)EEQueryUnit(inObject)->mUnitClass, 0);
    case EE_COMBAT_Q_PASSABLE: return EEQueryComplex(inObject)->IsPassableBy(EEQueryContextOf(inContext)->mUnit);
    case EE_COMBAT_Q_UNIT_SET: return gUnitSetManager.Contains(EEQueryUnit(inObject)->mUnitClass->GetUnitType(), (unsigned long)inArg);
    case EE_COMBAT_Q_SCALE: return UFloatBits(EEQueryComplex(inObject)->mPhysicalModel.GetScale());
    case EE_COMBAT_Q_SIDE_PART: return EEUnitClassOfKind<EE_CLASS_KIND_GATE>(EEQueryUnit(inObject)->GetUnitClass())->IsSidePart(EEQueryUnit(inObject)->mComplexUnit);
    case EE_COMBAT_Q_PATH_ATTEMPTS: return (uint32_t)EEQueryContextOf(inContext)->mGoal->mPathAttempts;
    case EE_COMBAT_Q_CAN_REACH_UNIT: return EEQueryContextOf(inContext)->mUnit->CanReachUnit(EEQueryUnit(inObject));
    case EE_COMBAT_Q_ATTACK_REACHABLE: return ee_units_cgq_attack_reachable_from_zero(EEQueryHandleOf(EEQueryContextOf(inContext)->mUnit), inObject);
    case EE_COMBAT_Q_IGNORED: return EEQueryContextOf(inContext)->mUnit->IsTargetIgnored(EEQueryComplex(inObject), true);
    case EE_COMBAT_Q_LINE_OF_SIGHT: return UFloatBits(EEQueryComplex(inObject)->GetLineOfSight());
    case EE_COMBAT_Q_SEEN: return EECombatIsSeenBy(EEQueryComplex(inObject), EEQueryContextOf(inContext)->mUnit->GetPlayer(), true);
    case EE_COMBAT_Q_DUMMY_HEAD: return EEQueryUnit(inObject) == NULL || RTSDummyHead(EEQueryUnit(inObject)) != NULL;
    case EE_COMBAT_Q_PROJECTILE_CATEGORY:
        return EEQueryUnit(inObject)->GetUnitClass()->mProjectileClass != NULL ? (uint32_t)EEQueryUnit(inObject)->GetUnitClass()->mProjectileClass->mCategory : EE_COMBAT_Q_NONE;
    case EE_COMBAT_Q_HIT_DISTANCE_SQUARED: return UFloatBits(EECombatHitDistanceSquared(EEQueryContextOf(inContext)->mUnit, EEQueryUnit(inObject), -10000.0f, -10000.0f));
    case EE_COMBAT_Q_TAG_COUNT: return EEQueryTags(EEQueryComplex(inObject)) != NULL ? (uint32_t)EEQueryTags(EEQueryComplex(inObject))->GetNumTagPoints() : 0;
    case EE_COMBAT_Q_IN_RANGE_TO_HIT: return EECombatWorld(EE_COMBAT_WORLD_IN_RANGE_TO_HIT, EEQueryContextOf(inContext)->mUnit, EEQueryUnit(inObject)) != 0;
    case EE_COMBAT_Q_BALLISTIC_HIT: return EECanBallisticHit(EEQueryContextOf(inContext)->mUnit, EEQueryUnit(inObject));
    case EE_COMBAT_Q_CONTACT_RANGE: return ee_units_cgq_in_contact_range_bits(EEQueryHandleOf(EEQueryContextOf(inContext)->mUnit), inObject, inArg);
    // (EE_COMBAT_Q_REGION_COUNT and the region's units: ee_combat reads.rs region_units over the Rust table)
    case EE_COMBAT_Q_CONTACT_X: return UFloatBits(EEQueryComplex(inObject)->mInitialContactLocation.GetX());
    case EE_COMBAT_Q_CONTACT_Y: return UFloatBits(EEQueryComplex(inObject)->mInitialContactLocation.GetY());
    case EE_COMBAT_Q_CONTACT_Z: return UFloatBits(EEQueryComplex(inObject)->mInitialContactLocation.GetZ());
    case EE_COMBAT_Q_IGNORED_COUNT: return (uint32_t)EEQueryComplex(inObject)->mIgnoredTargets.size();
    case EE_COMBAT_Q_DISTANCE_SQUARED: return UFloatBits(EEQueryContextOf(inContext)->mUnit->DistanceSquaredTo(EEQueryComplex(inObject)));
    case EE_COMBAT_Q_TARGETS_ENEMIES: return EEQueryComplex(inObject)->mBehavior->mCanTargetEnemies;
    case EE_COMBAT_Q_MOVES_FREELY: return EEQueryComplex(inObject)->mBehavior->CanMoveFreely(EEQueryComplex(inObject));
    case EE_COMBAT_Q_GOAL_B1C: return EEQueryComplex(inObject)->mGoalState->GetB1C();
    case EE_COMBAT_Q_SHOT_CLASS: return EEQueryShotClass(EEQueryContextOf(inContext)->mClass);
    case EE_COMBAT_Q_TURRET_PARTS: return (uint32_t)EEQueryComplex(inObject)->GetModel()->mTurretParts.size();
    case EE_COMBAT_Q_HIGH_TECH_TOOLS:
        return EEGetRules().Epoch(EEQueryUnit(inObject)->mPlayer->mTechnologies->mCurrentEpoch).mHighTechTools;   // epochs.yaml
    case EE_COMBAT_Q_SPARKS_EFFECT: return EEQueryUnit(inObject)->mUnitClass->mImpactSparksEffect != NULL;
    // leaves.rs (R5b.2, thirteenth round)
    case EE_COMBAT_Q_WAYPOINTS: return (uint32_t)EEQueryComplex(inObject)->mWaypoints.size();
    case EE_COMBAT_Q_MAX_VELOCITY: return UFloatBits(EEQueryComplex(inObject)->GetMaxVelocity());
    }
    return 0;
}

uint64_t EEQueryHandle(void* inContext, int32_t inCode, uint64_t inObject, int32_t inArg)
{
    switch (inCode) {
    case EE_COMBAT_H_COMPLEX: return EEQueryUnit(inObject) != NULL ? EEQueryHandleOf(EEQueryUnit(inObject)->GetComplexUnit()) : 0;
    case EE_COMBAT_H_PREVIOUS_PART: return EEQueryHandleOf(EEQueryComplex(inObject)->mPreviousPart);
    case EE_COMBAT_H_OWNER: return EEQueryHandleOf(EEQueryComplex(inObject)->mOwner);
    case EE_COMBAT_H_GOAL_TARGET: return ee_units_cgq_shot_target(inObject);   // its attack-unit goal's target (cg_query.rs)
    case EE_COMBAT_H_TILE_PREV: return EEQueryHandleOf(static_cast<EEUnit*>(EEQueryUnit(inObject)->mTileNode->mPrev));
    case EE_COMBAT_H_TILE_NEXT: return EEQueryHandleOf(static_cast<EEUnit*>(EEQueryUnit(inObject)->mTileNode->mNext));
    case EE_COMBAT_H_TARGET: return EEQueryHandleOf(EEQueryContextOf(inContext)->mGoal->GetAttackTarget());
    case EE_COMBAT_H_ACTIVE_GOAL_TARGET: return EEQueryHandleOf(static_cast<EEUGRust*>(EEQueryComplex(inObject)->mActiveGoal)->GetAttackTarget());
    }
    return 0;
}

void EEQueryAct(void* inContext, int32_t inCode, uint64_t inObject, int32_t inArg)
{
    switch (inCode) {
    case EE_COMBAT_A_FIRING_HIGH: EEQueryComplex(inObject)->mIsFiringHigh = true; break;
    case EE_COMBAT_A_WEAPON_TAG: EEQueryComplex(inObject)->mWeaponTagPoint = (unsigned long)(uint32_t)inArg; break;
    case EE_COMBAT_A_IGNORE: ee_units_cu_small(EE_UNITS_CU_S_DO_IGNORE_TARGET, EEQueryContextOf(inContext)->mUnit, EEQueryComplex(inObject), 0); break;
    }
}

void EEQueryLocation(void* inContext, uint64_t inObject, float (*outRows)[16]) { memcpy(*outRows, EEQueryComplex(inObject)->mPhysicalModel.GetLocation().mMatrix, sizeof(*outRows)); }

int32_t EEQueryIntersects(void* inContext, uint64_t inCandidate, const float (*inLine)[6], int32_t inSegment,
                          uint32_t inLOD, float inScale)
{
    return EEQueryContextOf(inContext)->mUnit->mPhysicalModel.Intersects(&EEQueryComplex(inCandidate)->mPhysicalModel, inSegment ? GE3DLine(GE3DPoint((*inLine)[0], (*inLine)[1], (*inLine)[2]), GEVector((*inLine)[3], (*inLine)[4], (*inLine)[5])) : GE3DLine(), inSegment != 0, inLOD, 1.0f, 1.0f, inScale);
}

// (the footprint tile: ee_sim combat cg_query.rs ee_units_cgq_footprint_tile, which reads the context's first member)
EeCombatQueryHost EEQueryHostOf(EEQueryContext* inContext)
{
    return {inContext, EEQueryRead, EEQueryHandle, EEQueryAct, EEQueryLocation, EEQueryIntersects, ee_units_cgq_footprint_tile};
}

// ---- the operations of ee_combat world.rs (R5b.2, eighth round): each the original's statement on the handles
// (the effects, the one-member stores and the plain reads are Rust's: ee_sim combat/world_views.rs)
uint64_t EEWorldOp(void* inContext, int32_t inCode, uint64_t inObject, uint64_t inOther, EeCombatWorldArgs* ioArgs)
{
    switch (inCode) {
    // (the compound cases are split in Rust, ee_sim combat cg_world.rs: each statement one case here, in order)
    case EE_COMBAT_W_IGN_TILE_X: return EEQueryComplex(inObject)->mIgnoredTargets[(size_t)(*ioArgs)[0]].mTileX;
    case EE_COMBAT_W_IGN_TILE_Y: return EEQueryComplex(inObject)->mIgnoredTargets[(size_t)(*ioArgs)[0]].mTileY;
    case EE_COMBAT_W_IGN_FROM_X: return EEQueryComplex(inObject)->mIgnoredTargets[(size_t)(*ioArgs)[0]].mFromX;
    case EE_COMBAT_W_IGN_FROM_Y: return EEQueryComplex(inObject)->mIgnoredTargets[(size_t)(*ioArgs)[0]].mFromY;
    case EE_COMBAT_W_IGN_UNIT_ID: return EEQueryComplex(inObject)->mIgnoredTargets[(size_t)(*ioArgs)[0]].mUnitID;
    case EE_COMBAT_W_IGN_TIME: return EEQueryComplex(inObject)->mIgnoredTargets[(size_t)(*ioArgs)[0]].mTime;
    case EE_COMBAT_W_IGN_ERASE_TO_END: EEQueryComplex(inObject)->mIgnoredTargets.erase(EEQueryComplex(inObject)->mIgnoredTargets.begin() + (size_t)(*ioArgs)[0], EEQueryComplex(inObject)->mIgnoredTargets.end()); return 0;
    case EE_COMBAT_W_IGN_ERASE_RANGE: EEQueryComplex(inObject)->mIgnoredTargets.erase(EEQueryComplex(inObject)->mIgnoredTargets.begin() + (size_t)(*ioArgs)[0], EEQueryComplex(inObject)->mIgnoredTargets.begin() + (size_t)(*ioArgs)[1]); return 0;
    case EE_COMBAT_W_IGN_PUSH_FRONT: EEQueryComplex(inObject)->mIgnoredTargets.push_front(EEComplexUnitAttacker()); return 0;
    case EE_COMBAT_W_IGN_SET_TILE_X: EEQueryComplex(inObject)->mIgnoredTargets[0].mTileX = (long)(*ioArgs)[0]; return 0;
    case EE_COMBAT_W_IGN_SET_TILE_Y: EEQueryComplex(inObject)->mIgnoredTargets[0].mTileY = (long)(*ioArgs)[0]; return 0;
    case EE_COMBAT_W_IGN_SET_FROM_X: EEQueryComplex(inObject)->mIgnoredTargets[0].mFromX = (long)(*ioArgs)[0]; return 0;
    case EE_COMBAT_W_IGN_SET_FROM_Y: EEQueryComplex(inObject)->mIgnoredTargets[0].mFromY = (long)(*ioArgs)[0]; return 0;
    case EE_COMBAT_W_IGN_SET_UNIT_ID: EEQueryComplex(inObject)->mIgnoredTargets[0].mUnitID = (unsigned long)(*ioArgs)[0]; return 0;
    case EE_COMBAT_W_IGN_SET_TIME: EEQueryComplex(inObject)->mIgnoredTargets[0].mTime = (unsigned long)(*ioArgs)[0]; return 0;
    case EE_COMBAT_W_IGNORED_TIME: EEQueryComplex(inObject)->mIgnoredTargets[(size_t)(*ioArgs)[0]].mTime = (unsigned long)(*ioArgs)[1]; return 0;
    // the reach tests' continent queries: Rust (world_views.rs reach_op)
    case EE_COMBAT_W_OTHER_CONTINENT: return EEQueryComplex(inObject)->IsOnOtherContinentThan(EEQueryComplex(inOther));
    case EE_COMBAT_W_SEEN_BY_OWNER: return EECombatIsSeenBy(EEQueryComplex(inObject), EEQueryUnit(inOther)->GetPlayer(), true);
    case EE_COMBAT_W_AG_NEW_LOCATION: EEQueryContextOf(inContext)->mAttackedGoal = EEUGRust::NewLocation(EEQueryComplex(inOther)->mPhysicalModel.GetLocation().GetX(), EEQueryComplex(inOther)->mPhysicalModel.GetLocation().GetY(), 0.0f, false, false, false, 0.0f, false, 0); return 0;
    case EE_COMBAT_W_AG_NEW_ATTACK: EEQueryContextOf(inContext)->mAttackedGoal = EEUGRust::NewAttackUnit(EEQueryComplex(inObject), EEQueryComplex(inOther), false, false); return 0;
    case EE_COMBAT_W_AG_SET_GOAL: EEQueryComplex(inObject)->SetGoal(EEQueryContextOf(inContext)->mAttackedGoal, 1, 0); return 0;
    case EE_COMBAT_W_NEAR_LIST: return (uint64_t)(uintptr_t)&EEQueryContextOf(inContext)->mNear;
    case EE_COMBAT_W_NEAR_UNIT: return EEQueryHandleOf(EEQueryContextOf(inContext)->mNear[(size_t)(*ioArgs)[0]]);
    case EE_COMBAT_W_CAN_ATTACK_CLASS: return EEQueryComplex(inObject)->CanAttackClass(EEQueryUnit(inOther)->GetUnitClass());
    case EE_COMBAT_W_IS_IGNORED: return EEQueryComplex(inObject)->IsTargetIgnored(EEQueryComplex(inOther), true);
    case EE_COMBAT_W_ATTACK_REACHABLE: return EEIsAttackReachable(EEQueryComplex(inObject), EEQueryComplex(inOther), &EEQueryContextOf(inContext)->mTile);
    case EE_COMBAT_W_HELP_CLONE: EEQueryContextOf(inContext)->mHelpGoal = EEQueryContextOf(inContext)->mAttackedGoal->Clone(); return 0;
    case EE_COMBAT_W_HELP_HAS_ACTION: return EEQueryComplex(inObject)->GetClass()->FindAction(EEQueryContextOf(inContext)->mHelpGoal->GetStartAction()) != NULL;
    case EE_COMBAT_W_HELP_SET_GOAL: EEQueryComplex(inObject)->SetGoal(EEQueryContextOf(inContext)->mHelpGoal, 1, 0); return 0;
    case EE_COMBAT_W_HELP_START_ACTION: EEQueryComplex(inObject)->StartAction(EEQueryContextOf(inContext)->mHelpGoal->GetStartAction()); return 0;
    case EE_COMBAT_W_ACTION_CHANGED: EEQueryComplex(inObject)->GetUnitClass()->ActionChanged(EEQueryComplex(inObject)); return 0;
    case EE_COMBAT_W_IGNORE: ee_units_cu_small(EE_UNITS_CU_S_DO_IGNORE_TARGET, EEQueryComplex(inObject), EEQueryComplex(inOther), 0); return 0;
    case EE_COMBAT_W_HAS_LAST_ATTACKER: return EEQueryComplex(inObject)->mLastAttacker != NULL;
    case EE_COMBAT_W_DEREFERENCE_LAST_ATTACKER: EEQueryComplex(inObject)->mLastAttacker->Dereference(); return 0;
    case EE_COMBAT_W_CLEAR_LAST_ATTACKER: EEQueryComplex(inObject)->mLastAttacker = NULL; return 0;
    case EE_COMBAT_W_LAUNCH_STORED_MISSILE: EEQueryContextOf(inContext)->mClass->LaunchStoredMissile(EEQueryComplex(inObject)); return 0;
    case EE_COMBAT_W_RELOAD: ee_units_cu_body(EE_UNITS_CU_DO_SET_RANDOM_RELOAD_TIMER, EEQueryComplex(inObject), (long)(*ioArgs)[0], 0, 0); return 0;
    case EE_COMBAT_W_FIND_DROP: return EEQueryHandleOf(EECombatFindDropLocation(EEQueryComplex(inObject), NULL, EEQueryContextOf(inContext)->mPoint));
    case EE_COMBAT_W_LINE_BLOCKED_AT: return EEIsLineToUnitBlocked(EEQueryComplex(inObject), EEQueryUnit(inOther), EEObjectOf<EEUnit*>((*ioArgs)[0]), EEObjectOf<long>((*ioArgs)[1]), EEObjectOf<long>((*ioArgs)[2]));
    case EE_COMBAT_W_LAUNCH_SHOT: EEQueryContextOf(inContext)->mClass->LaunchShot(EEQueryComplex(inObject), EEQueryUnit(inOther), EEQueryContextOf(inContext)->mPoint, NULL); return 0;
    case EE_COMBAT_W_LAUNCH_FROM_TURRET: EEQueryContextOf(inContext)->mClass->LaunchFromTurret(EEQueryComplex(inObject), EEQueryUnit(inOther), EEQueryContextOf(inContext)->mPoint); return 0;
    case EE_COMBAT_W_START_ACTION: EEQueryComplex(inObject)->StartAction((unsigned long)(*ioArgs)[0]); return 0;
    case EE_COMBAT_W_SHOT_EFFECT_AT: gWorld.GetEffectManager()->CreateEffect(EEQueryComplex(inObject), EEQueryContextOf(inContext)->mPoint, EEQueryContextOf(inContext)->mClass->mShotEffect, NULL, 0); return 0;
    // damage.rs (R5b.2, tenth round): ApplyHit, ApplyAreaDamage, ComputeDamage
    case EE_COMBAT_W_DI_ATTACK_FACTOR: return UFloatBits(gToHitTable.GetAttackFactor((long)(*ioArgs)[0], EEQueryUnit(inOther)->mUnitClass, EEQueryComplex(inObject)->mPlayer->GetPlayerID(), EEQueryUnit(inOther)->mPlayer->GetPlayerID()));
    case EE_COMBAT_W_DI_WEAPON_FACTOR: return UFloatBits(gWeaponToHitFactors[EEQueryUnit(inOther)->mUnitClass->mWeapon][(*ioArgs)[0]]);
    case EE_COMBAT_W_RULE_MINIMUM_DAMAGE: return EEGetRules().mCombat.mMinimumDamage;
    case EE_COMBAT_W_RULE_FANATIC_FACTOR: return UFloatBits(EERuleFloat(EEGetRules().mCombat.mFanaticFactor));
    case EE_COMBAT_W_RULE_ELEVATION_STEP: return UFloatBits(EEGetRules().mCombat.mElevationStep);
    case EE_COMBAT_W_RULE_ALLY_FACTOR: return UFloatBits(EEGetRules().mCombat.mAllyFactor);
    case EE_COMBAT_W_RULE_HERD_ALLY_FACTOR: return UFloatBits(EEGetRules().mCombat.mHerdAllyFactor);
    case EE_COMBAT_W_RULE_TRAMPLE_DAMAGE: return EEGetRules().mCombat.mTrampleDamage;
    case EE_COMBAT_W_RULE_TRAMPLE_THRESHOLD: return UFloatBits(EEGetRules().mCombat.mTrampleThreshold);
    case EE_COMBAT_W_RULE_DEAD_FOOD_SHARE: return UFloatBits(EEGetRules().mCombat.mDeadFoodShare);
    case EE_COMBAT_W_ELEVATION: return UFloatBits(EEQueryContextOf(inContext)->mClass->GetElevationFactor(EEQueryComplex(inObject), EEQueryComplex(inOther)));
    case EE_COMBAT_W_HC_CATEGORY: return EEQueryComplex(inObject)->mUnitClass->mCategory;
    case EE_COMBAT_W_HC_SPECIAL_ATTACK: return EEQueryComplex(inObject)->mIsSpecialAttack;
    case EE_COMBAT_W_HC_LAUNCHER: return (uint64_t)(uintptr_t)EEQueryComplex(inObject)->mLauncherClass;
    case EE_COMBAT_W_HC_SPECIAL_WEAPON: return (uint64_t)(uintptr_t)gUnitClassTable[kUnitTypeCapitalShipSpecialWeapon];
    case EE_COMBAT_W_HC_UPGRADE: return (uint64_t)(uintptr_t)EEQueryComplex(inObject)->mPlayer->GetUpgrade(EEObjectOf<EEUnitClass>((*ioArgs)[0])->mUnitType);
    case EE_COMBAT_W_HC_UPGRADE_ATTACK: return UFloatBits(EEObjectOf<EEUnitClassUpgrade>((*ioArgs)[0])->GetAttack());
    case EE_COMBAT_W_HC_UPGRADE_AREA: return UFloatBits(EEObjectOf<EEUnitClassUpgrade>((*ioArgs)[0])->mAreaEffect);
    case EE_COMBAT_W_HC_CLASS_ATTACK: return UFloatBits(EEObjectOf<EEUnitClass>((*ioArgs)[0])->mAttack);
    case EE_COMBAT_W_HC_CLASS_AREA: return UFloatBits(EEObjectOf<EEUnitClass>((*ioArgs)[0])->mAreaEffect);
    case EE_COMBAT_W_HC_CLASS_AMMO: return EEObjectOf<EEUnitClass>((*ioArgs)[0])->mAmmoType;
    case EE_COMBAT_W_HC_CLASS_WEAPON: return EEObjectOf<EEUnitClass>((*ioArgs)[0])->mWeapon;
    case EE_COMBAT_W_HC_CLASS_CATEGORY: return EEObjectOf<EEUnitClass>((*ioArgs)[0])->mCategory;
    case EE_COMBAT_W_HC_CLASS_FRIENDLY: return UFloatBits(EEObjectOf<EEUnitClass>((*ioArgs)[0])->mFriendlyDamageFactor);
    case EE_COMBAT_W_HC_SHOT_EFFECT: return EEQueryContextOf(inContext)->mClass->mShotEffect != NULL;
    case EE_COMBAT_W_UE_CLEAR: EEQueryContextOf(inContext)->mEffect = NULL; return 0;
    case EE_COMBAT_W_UE_GET: return EEQueryComplex(inObject)->GetEffect(3, EEQueryContextOf(inContext)->mEffect);
    case EE_COMBAT_W_UE_HAS: return EEQueryContextOf(inContext)->mEffect != NULL;
    case EE_COMBAT_W_UE_CREATE: gWorld.mEffectManager->CreateUnitEffect(EEQueryComplex(inObject), EEQueryContextOf(inContext)->mEffect, NULL, false, 0, 0); return 0;
    case EE_COMBAT_W_TAKE_HIT: EEQueryUnit(inObject)->TakeHit(EEQueryComplex(inOther), (long)(*ioArgs)[0], EEQueryComplex(inOther)->mPlayer->GetPlayerID(), 1); return 0;
    case EE_COMBAT_W_UNIT_DAMAGED: EEQueryUnit(inObject)->mUnitClass->UnitDamaged(EEQueryUnit(inObject)->GetComplexUnit(), EEQueryComplex(inOther), (long)(*ioArgs)[0]); return 0;
    case EE_COMBAT_W_PATH_START_AT: EEQueryComplex(inObject)->GetPathStart(*EEObjectOf<GE3DPoint>((*ioArgs)[0]), *EEObjectOf<float>((*ioArgs)[1])); return 0;
    case EE_COMBAT_W_NEW_TARGET_NEAR_OLD: EEFindNewTargetNearOld(EEQueryComplex(inObject), EEQueryComplex(inOther)); return 0;
    case EE_COMBAT_W_RING_TILE_X: return EEQueryUnit(inOther)->GetTileX();
    case EE_COMBAT_W_RING_TILE_Y: return EEQueryUnit(inOther)->GetTileY();
    case EE_COMBAT_W_RING_TILE_SIZE: return EEQueryUnit(inOther)->GetUnitClass()->GetTileSize();
    case EE_COMBAT_W_FOOTPRINT: ee_units_uu_unit_tiles(0, (uint64_t)(uintptr_t)(EEQueryUnit(inObject)), &EEQueryContextOf(inContext)->mFootprint); return EEQueryContextOf(inContext)->mFootprint.size();
    case EE_COMBAT_W_FOOTPRINT_X: return EEQueryContextOf(inContext)->mFootprint[(size_t)(*ioArgs)[0]].mX;
    case EE_COMBAT_W_FOOTPRINT_Y: return EEQueryContextOf(inContext)->mFootprint[(size_t)(*ioArgs)[0]].mY;
    case EE_COMBAT_W_TS_TILE_AT_LOCATION: return (uint64_t)(uintptr_t)::gWorld.mMap->GetTileAtLocation(EETileLocation((long)(*ioArgs)[0], (long)(*ioArgs)[1]));
    case EE_COMBAT_W_TS_TILE: return (uint64_t)(uintptr_t)::gWorld.mMap->GetTile((unsigned long)(*ioArgs)[0], (unsigned long)(*ioArgs)[1]);
    case EE_COMBAT_W_TS_SEEN: return EEObjectOf<EETile>((*ioArgs)[0])->IsSeenBy(EEObjectOf<EEPlayer>((int64_t)inObject));
    case EE_COMBAT_W_TS_VISIBLE: return EEObjectOf<EETile>((*ioArgs)[0])->GetVisibility(EEObjectOf<EEPlayer>((int64_t)inObject)->GetPlayerID()) > 1;
    case EE_COMBAT_W_GOAL_LOCATION_AT: EEQueryComplex(inObject)->mActiveGoal->GetGoalLocation(*EEObjectOf<GE3DPoint>((*ioArgs)[0])); return 0;
    case EE_COMBAT_W_DM_HAS_MODEL: return EEQueryUnit(inObject)->GetModel()->mModel != NULL;
    case EE_COMBAT_W_DM_DEFAULT_POINT: *EEObjectOf<GE3DPoint>((*ioArgs)[0]) = EEQueryUnit(inObject)->GetModel()->mDefaultPoint; return 0;
    }
    return 0;
}

}   // namespace

// The query entry points: ee_sim combat cg_query.rs (the marshalling and the CBQR records) over one C++ context and
// host per call, made here
extern "C" void ee_units_cg_query(uint64_t inUnit, uint64_t inGoal, uint64_t inClass, void* inData,
                                  void (*inBody)(void*, const EeCombatWorldHost*))
{
    EEQueryContext theContext = {EEQueryComplex(inUnit), EECgObject<EEUGRust>((int64_t)inGoal), EECgObject<EEUnitClass>((int64_t)inClass)};
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp, inClass};
    inBody(inData, &theHost);
}

// (the reads' entry points are inline in ee/EECombatGoals.h)
bool EECombatComputeDamage(int inForm, EEComplexUnit* inUnit, EEUnit* inTarget, float inAttack, long inAmmo, long inWeapon,
                           float inFriendlyFactor, long& outAmount)
{
    return ee_units_cgq_compute_damage(inForm, EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), EEObj(inUnit->mUnitClass),
                                       inAttack, inAmmo, inWeapon, inFriendlyFactor, &outAmount);
}
