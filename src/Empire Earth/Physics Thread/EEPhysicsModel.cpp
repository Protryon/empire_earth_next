// C:\EEX\Empire Earth\Physics Thread\EEPhysicsModel.cpp (file name inferred: no __FILE__; RTTI EEPhysicsModel; one
// unit per physics-model class, alphabetical link order). Range 0x1543b7..0x156df8 (each unit ends with its own
// inline-COMDAT cluster).
// The singletons' creation/destruction (0x1543b7 / 0x1550ba) open the unit: they `new` every subclass.
//
// New (the physics models' lane, round 1): the models are whole in Rust (ee_units pm.rs, pm_ground.rs, pm_air.rs),
// with every EEPM* class (EEPMLand.cpp .. EEPMSubmergedNaval.cpp, deleted). The world lane, round 2: the projectile's
// and missile's steps, the unit's group-aware rates and the TEST ONLY PMST record are Rust too (pm_flight.rs,
// pm_note.rs). This unit keeps the singletons, the forwarders and the C++ the models call (ee_units_pm_host_*,
// pm_host.rs), one call each: the unit's setters, its goals' virtuals, the map's and the mesh's reads, the effects, the
// carrier's tag points.
#include <windows.h>
#include <vector>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/U2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"
#include "lle/GEQuaternion.h"

#include "ee/EEUnitKind.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERules.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUGRust.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUnitGoal.h"
#include "ee/EECombatGoals.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUnitStore.h"
#include "ee/EESimCapture.h"
#include "rust/sim/combat.h"
#include "rust/sim/units.h"

extern EEToHitTable gToHitTable;           // 0x5318e8 (Empire Earth.cpp; as EECPAttackSubGroupUnit.cpp declares it)

// The twelve physics model singletons (land to spaceship; EEUnitClass +0x110 points at one); the original's 0x531d58
// array of `new`s. This unit's .data is 0x4c2368..0x4c2380 (after EEPhysics.cpp's RTTI, before the EEPMProjectile type
// descriptor): 0.5f (the trample minimum: ee_units pm_flight.rs), 0.75f, 1.0f, 4000, 2000, 1.
static EEPhysicsModel sPhysicsModels[12] = {EEPhysicsModel(0), EEPhysicsModel(1), EEPhysicsModel(2), EEPhysicsModel(3), EEPhysicsModel(4), EEPhysicsModel(5), EEPhysicsModel(6), EEPhysicsModel(7), EEPhysicsModel(8), EEPhysicsModel(9), EEPhysicsModel(10), EEPhysicsModel(11)};
EEPhysicsModel* gPhysicsModels[12] = {&sPhysicsModels[0], &sPhysicsModels[1], &sPhysicsModels[2], &sPhysicsModels[3], &sPhysicsModels[4], &sPhysicsModels[5], &sPhysicsModels[6], &sPhysicsModels[7], &sPhysicsModels[8], &sPhysicsModels[9], &sPhysicsModels[10], &sPhysicsModels[11]};
long gUnitAIWaitScale = 4000;               // 0x4c2374 (EEUnitAI 0x22eca0's wait scale; provisional name)
unsigned long gCollisionDetectionUnit = 2000;   // 0x4c2378 (EmpireEarth.h; EEWorld resets it to 2000)
unsigned long gCollisionPeriod = 1;         // 0x4c237c (EEPhysics.cpp's collision period; provisional name)

// Reads world table entry 79's float (the trample's minimum squared distance) for the models
void EEInitPhysicsModels() { ee_units_pm_set_trample_min(gWorldTable.GetFloatValue(kWorldTrampleMinDistanceSquared)); }

// (the singletons are static)
void EEFreePhysicsModels() {}

// The unit's rates (its active group's when the group moves together): ee_units pm_flight.rs rate

// NEAR: register choice only (ecx vs eax for the class pointer)
// Returns whether the unit swims: it has a swim animation, stands on deep water and cruises above the ground
bool EEComplexUnit::IsSwimming()
{
    return this->HasSwimAnimation() && this->mTile && this->mTile->mTerrainType == kTerrainDeepWater
        && this->mUnitClass->GetCruiseAltitude()
            > ::gWorld.mMap->mTerrainMesh.GetMeshZ(this->mPhysicalModel.GetLocation().GetX(), this->mPhysicalModel.GetLocation().GetY());
}

// Returns whether the unit can move on terrain kind 1 and its model has more than one frame of animation 2
bool EEComplexUnit::HasSwimAnimation()
{
    return this->mUnitClass->CanMoveOnTerrain(this, 1) && this->GetModel()->mFrameCounts[2] > 1;
}

// ---- the C++ the Rust models call (ee_units pm_host.rs; each the original statement it replaces)

static EEComplexUnit* PMUnit(uint64_t inObject)
{
    return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inObject));
}

static uint64_t PMObject(const void* inPointer)
{
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(inPointer));
}

// a matrix from Rust as a transformation
static GETransformation PMMatrix(const float* inMatrix) { return GETransformation(reinterpret_cast<const float(*)[4]>(inMatrix)); }

// the collision period, the world's collision detection flags (the tick's: pm_host.rs tick)
extern "C" uint32_t ee_units_pm_host_collision(uint8_t inFlags)
{
    return inFlags ? (uint32_t)gWorld.mCollisionDetectionFlags : (uint32_t)gCollisionPeriod;
}

extern "C" float ee_units_pm_host_random()
{
    return gPhysics->GetRandom();
}

extern "C" void ee_units_pm_host_set_transformation(uint64_t inObject, const float* inMatrix)
{
    PMUnit(inObject)->SetTransformation(PMMatrix(inMatrix), true);
}

extern "C" void ee_units_pm_host_refresh_transformation(uint64_t inObject)
{
    PMUnit(inObject)->SetTransformation(PMUnit(inObject)->mPhysicalModel.GetLocation(), true);
}

extern "C" void ee_units_pm_host_set_location(uint64_t inObject, const float* inMatrix)
{
    PMUnit(inObject)->mPhysicalModel.SetLocation(PMMatrix(inMatrix));
}

extern "C" void ee_units_pm_host_set_location_point(uint64_t inObject, float inX, float inY, float inZ)
{
    PMUnit(inObject)->SetLocation(GE3DPoint(inX, inY, inZ), true);
}

extern "C" void ee_units_pm_host_set_location_xy(uint64_t inObject, float inX, float inY)
{
    PMUnit(inObject)->SetLocationXY(inX, inY, true);
}

extern "C" void ee_units_pm_host_update_tile(uint64_t inObject, float* ioMatrix, long* outTile)
{
    EEUpdateUnitTile(reinterpret_cast<GETransformation*>(ioMatrix), PMUnit(inObject), &outTile[0], &outTile[1]);
}

extern "C" void ee_units_pm_host_set_scale(uint64_t inObject, float inScale)
{
    PMUnit(inObject)->mPhysicalModel.SetScale(inScale);
}

extern "C" int32_t ee_units_pm_host_model_parts(uint64_t inObject)
{
    return (int32_t)PMUnit(inObject)->GetModel()->GetNumParts();
}

extern "C" uint32_t ee_units_pm_host_frame_time(uint64_t inObject, uint32_t inAnimation)
{
    return (uint32_t)PMUnit(inObject)->GetModel()->mFrameTimes[inAnimation];
}

extern "C" uint64_t ee_units_pm_host_tile_of(uint64_t inObject)
{
    return PMObject(static_cast<RTSTile*>(PMUnit(inObject)->mTile));
}

// the goals' reads: pm_host.rs GOAL_* (inGoal the unit's mGoalState, or the cast a case returned)
extern "C" uint64_t ee_units_pm_host_goal(int32_t inWhich, uint64_t inGoal, float* outPoint)
{
    EEUnitGoal* theGoal = reinterpret_cast<EEUnitGoal*>(static_cast<uintptr_t>(inGoal));
    switch (inWhich) {
    case 0: return PMObject(EEAttackUnitCast(theGoal));
    case 1: return PMObject(static_cast<EEUGRust*>(theGoal)->GetAttackTarget());
    case 2: return PMObject(EEHealGoalCast(theGoal));
    case 3: return PMObject(static_cast<EEUGRust*>(theGoal)->GetHealTarget());
    case 4: return PMObject(static_cast<EEUGRust*>(theGoal)->GetGatherResource());
    case 5: return PMObject(static_cast<EEUGRust*>(theGoal)->GetBuildTarget());
    case 6: return PMObject(EECastCalamityGoalCast(theGoal));
    case 7: static_cast<EEUGRust*>(theGoal)->GetCalamityTargetLocation(*reinterpret_cast<GE3DPoint*>(outPoint)); return 0;
    case 8: theGoal->GetGoalLocation(*reinterpret_cast<GE3DPoint*>(outPoint)); return 0;
    case 9: return EEGoalHasLocation(theGoal) ? 1 : 0;
    default: outPoint[0] = EELocationGoalYaw(theGoal); return 0;
    }
}

extern "C" void ee_units_pm_host_path_start(uint64_t inObject, float* outPoint, float* outLength)
{
    PMUnit(inObject)->GetPathStart(*reinterpret_cast<GE3DPoint*>(outPoint), *outLength);
}





// the class flies over (a class method)
extern "C" uint8_t ee_units_pm_host_flies_over(uint64_t inUnit)
{
    return reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inUnit))->mUnitClass->FliesOver() == true ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_has_arrived(uint64_t inObject, float inDistance)
{
    return PMUnit(inObject)->mUnitClass->HasArrived(PMUnit(inObject), inDistance) ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_is_swimming(uint64_t inObject)
{
    return PMUnit(inObject)->IsSwimming() ? 1 : 0;
}

extern "C" void ee_units_pm_host_place_on_ground(uint64_t inObject, float inX, float inY, float inZ)
{
    PMUnit(inObject)->PlaceOnGround(GE3DPoint(inX, inY, inZ));
}

extern "C" float ee_units_pm_host_height_at(uint64_t inObject, float inX, float inY, float inZ)
{
    return PMUnit(inObject)->GetUnitClass()->GetHeightAt(PMUnit(inObject), GE3DPoint(inX, inY, inZ));
}

extern "C" void ee_units_pm_host_set_animation(uint64_t inObject, int32_t inA, int32_t inB, uint8_t inPaused)
{
    PMUnit(inObject)->SetAnimation(inA, inB, inPaused != 0);
}

extern "C" void ee_units_pm_host_set_blocking_unit(uint64_t inObject, uint64_t inBlocker)
{
    // an EEUnit or EEComplexUnit: RTSUnit is the first base of both
    PMUnit(inObject)->SetBlockingUnit(reinterpret_cast<RTSUnit*>(static_cast<uintptr_t>(inBlocker)));
}

extern "C" void ee_units_pm_host_clear_goal_queue(uint64_t inObject)
{
    PMUnit(inObject)->ClearGoalQueue(0);
}

extern "C" void ee_units_pm_host_start_action(uint64_t inObject, int32_t inAction)
{
    PMUnit(inObject)->StartAction(inAction);
}

extern "C" void ee_units_pm_host_action_changed(uint64_t inObject)
{
    PMUnit(inObject)->mUnitClass->ActionChanged(PMUnit(inObject));
}

extern "C" uint8_t ee_units_pm_host_can_stand_on(uint64_t inObject, int32_t inX, int32_t inY)
{
    return ee_units_uu_can_stand_on(inX, inY, (uint64_t)(uintptr_t)(PMUnit(inObject))) ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_friendly(uint64_t inObject, uint64_t inOther)
{
    return PMUnit(inOther)->mPlayer == PMUnit(inObject)->mPlayer
        || PMUnit(inObject)->mPlayer->mEnemies[PMUnit(inOther)->mPlayer->mPlayerID] == 0 ? 1 : 0;
}

// the trample: the shared damage function (ee_combat damage.rs; combat.yaml trample_damage/_threshold) and the hit
extern "C" uint8_t ee_units_pm_host_trample_damage(uint64_t inObject, uint64_t inOther, long* outAmount)
{
    return EECombatComputeDamage(kDamageTrample, PMUnit(inObject), PMUnit(inOther), PMUnit(inObject)->mStats->GetAttack(), PMUnit(inObject)->mUnitClass->mAmmoType,
                                 PMUnit(inObject)->mUnitClass->mWeapon, 0.0f, *outAmount) ? 1 : 0;
}

extern "C" void ee_units_pm_host_take_hit(uint64_t inObject, uint64_t inAttacker, long inAmount)
{
    PMUnit(inObject)->TakeHit(PMUnit(inAttacker), inAmount, PMUnit(inAttacker)->mPlayer->mPlayerID, 1);
}





// the unit's class is a gate class (the contact's gate test: pm_host.rs contact_gate)
extern "C" uint8_t ee_units_pm_host_is_gate_class(uint64_t inObject)
{
    return EEUnitClassOfKind<EE_CLASS_KIND_GATE>(PMUnit(inObject)->mUnitClass) ? 1 : 0;
}

extern "C" uint64_t ee_units_pm_host_tile(int32_t inX, int32_t inY)
{
    return PMObject(static_cast<RTSTile*>(::gWorld.mMap->GetTile(inX, inY)));
}



extern "C" uint8_t ee_units_pm_host_is_edge_tile(int32_t inX, int32_t inY)
{
    return ::gWorld.mMap->IsEdgeTile(inX, inY) ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_can_move_on_terrain(uint64_t inObject, int32_t inTerrain)
{
    return PMUnit(inObject)->mUnitClass->CanMoveOnTerrain(PMUnit(inObject), inTerrain) ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_passable_by(uint64_t inUnit, uint64_t inObject)
{
    return reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inUnit))->IsPassableBy(PMUnit(inObject)) ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_unbuilt_land_building(uint64_t inUnit)
{
    return reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inUnit))->mUnitClass->IsLandBuilding()
        && !reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inUnit))->mComplexUnit->mIsBuilt ? 1 : 0;
}

extern "C" void ee_units_pm_host_ground_point(float inX, float inY, float inZ, float* outPoint)
{
    ::gWorld.mMap->GetGroundPoint(GE3DPoint(inX, inY, inZ), *reinterpret_cast<GE3DPoint*>(outPoint));
}

extern "C" float ee_units_pm_host_terrain_tilt(const float* inMatrix, uint8_t inRoll)
{
    return inRoll ? ::gWorld.mMap->GetTerrainRoll(PMMatrix(inMatrix)) : ::gWorld.mMap->GetTerrainPitch(PMMatrix(inMatrix));
}

// the terrain mesh's left, right, front and back planes (each its point and normal: six floats)
extern "C" const float* ee_units_pm_host_edge_plane(int32_t inWhich)
{
    switch (inWhich) {
    case 0: return &::gWorld.mMap->mTerrainMesh.GetLeftPlane().GetPoint().mX;
    case 1: return &::gWorld.mMap->mTerrainMesh.GetRightPlane().GetPoint().mX;
    case 2: return &::gWorld.mMap->mTerrainMesh.GetFrontPlane().GetPoint().mX;
    default: return &::gWorld.mMap->mTerrainMesh.GetBackPlane().GetPoint().mX;
    }
}

// the terrain mesh: its tile at the point's floors is valid (1.0f), its height at the point (pm_host.rs mesh_ground)
extern "C" float ee_units_pm_host_mesh(int32_t inWhich, float inX, float inY)
{
    switch (inWhich) {
    case 0: return ::gWorld.GetMap()->mTerrainMesh.GetIsValidTile(UMFloorUnsigned(inX), UMFloorUnsigned(inY)) ? 1.0f : 0.0f;
    default: return ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(inX, inY);
    }
}



// the effects: pm_host.rs EFFECT_* (the class's splash placed at the point, its death effect on the unit, the impact
// decal, the land ripple)
extern "C" void ee_units_pm_host_effect(int32_t inWhich, uint64_t inObject, float* inPoint)
{
    GE3DPoint& thePoint = *reinterpret_cast<GE3DPoint*>(inPoint);
    switch (inWhich) {
    case 0: gEffectManager->CreatePlacedEffect(thePoint, PMUnit(inObject)->mUnitClass->mSplashEffect, NULL); break;
    case 1: gEffectManager->CreateUnitEffect(PMUnit(inObject), PMUnit(inObject)->mUnitClass->mDeathEffect, NULL, 0, 0, 0); break;
    case 2: EEPlaceImpactDecal(PMUnit(inObject), thePoint); break;
    default: gWorld.GetEffectManager()->CreatePlacedEffect(thePoint, EEGraphicEffect::Get(kGraphicEffectLandRipple), NULL); break;
    }
}

// the carrier's tag points (pm_host.rs CARRIER_*: the animation's list, its count, the location scaled by the stats, a
// point at the frame)
extern "C" uint64_t ee_units_pm_host_carrier(int32_t inWhich, uint64_t inOf, int32_t inIndex, float* ioMatrix, float* outPoint)
{
    switch (inWhich) {
    case 0: return PMObject(PMUnit(inOf)->GetModel()->mTagPoints[PMUnit(inOf)->mAnimation]);
    case 1: return (uint64_t)reinterpret_cast<EETagPointList*>(static_cast<uintptr_t>(inOf))->mPoints.size();
    case 2: EETagPointList::Scale(*reinterpret_cast<GETransformation*>(ioMatrix), PMUnit(inOf)->mStats->GetScale()); return 0;
    default: PMUnit(inOf)->GetModel()->mTagPoints[PMUnit(inOf)->mAnimation]->mPoints[inIndex]->GetPoint(PMUnit(inOf)->mPhysicalModel.GetFrame(), *reinterpret_cast<GETransformation*>(ioMatrix), *reinterpret_cast<GE3DPoint*>(outPoint)); return 0;
    }
}

// OrientOnCarrier's last call: the class's RouteDockWaypoint(unit)
extern "C" void ee_units_pm_host_route_dock_waypoint(uint64_t inObject)
{
    PMUnit(inObject)->mUnitClass->RouteDockWaypoint(PMUnit(inObject));
}

// a gate's last linked carrier and last linked part (pm_host.rs gate_links)
extern "C" uint64_t ee_units_pm_host_gate_link(int32_t inWhich, uint64_t inObject)
{
    return PMObject(inWhich == 0 ? EEUnitClassOfKind<EE_CLASS_KIND_GATE>(PMUnit(inObject)->GetUnitClass())->GetLastLinkedCarrier(PMUnit(inObject))
                                 : EEUnitClassOfKind<EE_CLASS_KIND_GATE>(PMUnit(inObject)->GetUnitClass())->GetLastLinked(PMUnit(inObject)));
}

// the projectile's and the missile's steps' reads and writes (pm_flight.rs: the steps are Rust)
extern "C" float ee_units_pm_host_launch_speed(uint64_t inObject, uint8_t inLauncher)
{
    return PMUnit(inObject)->mPlayer->mUnitClassUpgrades[(inLauncher ? PMUnit(inObject)->mLauncherClass : PMUnit(inObject)->mUnitClass)->mUnitType]->mLaunchSpeed;
}

extern "C" const float* ee_units_pm_host_projectile_start(uint64_t inObject)
{
    return &PMUnit(inObject)->mProjectileStart.mX;
}

extern "C" void ee_units_pm_host_set_velocity(uint64_t inObject, float inVelocity)
{
    PMUnit(inObject)->SetVelocity(inVelocity);
}

extern "C" void ee_units_pm_host_drop_target(uint64_t inObject, float* outPoint)
{
    EECombatFindDropLocation(PMUnit(inObject), NULL, *reinterpret_cast<GE3DPoint*>(outPoint));
}

// TEST ONLY (PMST): the blocking unit's id, 0xfffffffe for none
extern "C" uint32_t ee_units_pm_host_blocking_id(uint64_t inObject)
{
    return PMUnit(inObject)->mBlockingUnit ? (uint32_t)PMUnit(inObject)->mBlockingUnit->mUnitID : 0xfffffffeu;
}
