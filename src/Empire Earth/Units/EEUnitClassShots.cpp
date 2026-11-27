// EEUnitClassShots.cpp (new, R5b.4 round 32): the shots' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEMiniMap.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitClassKinds.h"   // EEObj (the host)
#include "rust/game/gamecpp.h"
#include "rust/sim/tech.h"
#include "rust/sim/units.h"

// ---- EEUCProjectile.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The projectile's unit
// class (EEUCBomb derives from it and shares these slots by ICF): 0x219143..0x219c49 (after EEUCPlanetSpaceship.cpp;
// EEUCResource.cpp's +0x7c 0x219c49 follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0xe0, +0xe4, +0x74, +0x78, +0xac, +0xa0, +0xbc, +0xb8 in source order.

// 0x447338 (3.0, the launch height below which a projectile may hit the ground at once) is ee_combat's (impact.rs)
// 0x447050: the slow-down per unit of depth below the ground

// 0x241a6a (cdecl): the launch angle from inOwner at inLocation for the ammunition (provisional name)
bool EEGetBallisticPitch(EEComplexUnit* inUnit, EEUnitClassUpgrade* inStats, const GE3DPoint* inFrom, const GE3DPoint* inTo, float* outAngle);

// New (port): the host of ee_combat shots.rs's plans (EEUCProjectile/EEUCMissile::ActionChanged and Update,
// EEUCTorpedo::Update): each read the original's expression, each operation its statement, on the unit and its
// class (the original's `this`, cast as the original's member function saw it: virtual calls dispatch as they did).
// The unit's plain fields (command, location, goal kind, type, hit points, tile x, rotting, flight and cloak times) are
// the unit store's (shots.rs StoredShot), never asked here.
namespace {

struct EEShotContext {
    EEComplexUnit* mUnit;
    EEUnitClass* mClass;
    int32_t mKind;                          // EE_COMBAT_SHOT_*
};

EEUGRust* EEShotAttack(EEComplexUnit* inUnit) { return static_cast<EEUGRust*>(inUnit->mGoalState); }

int32_t EEShotOp(void* inContext, int32_t inOp, int32_t inArg, int32_t inArg1)
{
    EEShotContext* theContext = (EEShotContext*)inContext;
    EEComplexUnit* theUnit = theContext->mUnit;
    EEUnitClass* theClass = theContext->mClass;
    switch (inOp) {
    // ---- reads
    case EE_COMBAT_SHOT_R_RANDOM: return (int32_t)UFloatBits(gPhysics->mRandom.Generate());
    case EE_COMBAT_SHOT_R_FLAMING_CHANCE: return (int32_t)UFloatBits((float)(reinterpret_cast<EECivPower*>(ee_tech_civ_power_record((int32_t)kCivPowerFlamingArrows)))->mChance);
    case EE_COMBAT_SHOT_R_WORLD_CLEARING: return gWorld.mClearing;
    case EE_COMBAT_SHOT_R_FREE_CAMERA: return gFreeCamera;
    case EE_COMBAT_SHOT_R_TARGET_HIT_POINTS: return (int32_t)EEShotAttack(theUnit)->GetAttackTarget()->mHitPoints;
    // ---- operations
    case EE_COMBAT_SHOT_UPDATE_ROTTING: (long)ee_units_ucp_call(EE_UNITS_UCP_DO_ROTTING_TIME, NULL, theUnit, 0, 0); break;
    case EE_COMBAT_SHOT_DEATH_EFFECT: gWorld.mEffectManager->CreateUnitEffect(theUnit, theClass->mDeathEffect, NULL, 0, 0, 0); break;
    case EE_COMBAT_SHOT_UNLOAD_CARGO:
        ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_AT_TILE, theUnit, (long)UBitsFloat(inArg), (long)UBitsFloat(inArg1), false);
        break;
    case EE_COMBAT_SHOT_SIGHT_OFF: theUnit->SightOffHere(); break;
    case EE_COMBAT_SHOT_SIGHT_ON: theUnit->SightOnHere(); break;
    case EE_COMBAT_SHOT_CREATE_CALAMITY:
        USingleton<EECalamityManager>::GetInstance()->CreateUnitCalamity(
            0x18, -1, static_cast<EEUGRust*>(theUnit->mComplexUnit->mGoalState)->GetAttackTarget()->GetUnitID(),
            theUnit->mPlayer->GetPlayerID());
        break;
    case EE_COMBAT_SHOT_CREATE_REMAINS: theClass->CreateRemainsUnit(theUnit); break;
    case EE_COMBAT_SHOT_HANDLE_ACTION_CHANGED:
        ee_units_class_action_changed(EE_UNITS_ACTION_BASE, (uint64_t)(uintptr_t)theClass, (uint64_t)(uintptr_t)theUnit);
        break;
    case EE_COMBAT_SHOT_MINIMAP_OFF: ee_gamecpp_minimap_move_unit(gMiniMap, theUnit, theUnit->mTileX, theUnit->mTileY, (long)(-10000), (long)(-10000)); break;
    case EE_COMBAT_SHOT_STEP_PHYSICS: theClass->mPhysicsModel->Step(theUnit); break;
    case EE_COMBAT_SHOT_REFRESH_SOUND: ee_units_uc_body(EE_UNITS_UC_DO_REFRESH_SOUND, theClass, theUnit); break;
    }
    return 0;
}

// The statements of the operations shots.rs composes (EE_COMBAT_SHOT_X_*: the original compound cases' calls, one
// each; inA an object, inB a value, ioF floats)
int64_t EEShotOp64(void* inContext, int32_t inOp, uint64_t inA, int64_t inB, float* ioF)
{
    EEShotContext* theContext = (EEShotContext*)inContext;
    EEComplexUnit* theUnit = theContext->mUnit;
    EEUnitClass* theClass = theContext->mClass;
    GE3DPoint& p = *reinterpret_cast<GE3DPoint*>(ioF);
    switch (inOp) {
    case EE_COMBAT_SHOT_X_FLAMING_CLASS: return USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(0x17, *reinterpret_cast<EECalamityClass**>(inA));
    case EE_COMBAT_SHOT_X_UNIT_SET: return (int64_t)(uintptr_t)gUnitSetManager.GetSet(reinterpret_cast<EECalamityClass*>(inA)->GetUnitSet());
    case EE_COMBAT_SHOT_X_COMPLEX_GOAL_TARGET: return (int64_t)(uintptr_t)static_cast<EEUGRust*>(theUnit->mComplexUnit->mGoalState)->GetAttackTarget();
    case EE_COMBAT_SHOT_X_SET_CONTAINS: return reinterpret_cast<EEUnitSet*>(inA)->Contains((unsigned long)inB);
    case EE_COMBAT_SHOT_X_POSITION: theUnit->GetPosition(p); return 0;
    case EE_COMBAT_SHOT_X_SPLASH_AT: gWorld.mEffectManager->CreatePlacedEffect(p, theClass->mSplashEffect, NULL); return 0;
    case EE_COMBAT_SHOT_X_GROUND_POINT: ::gWorld.mMap->GetGroundPoint(GE3DPoint(p), p); return 0;
    case EE_COMBAT_SHOT_X_SET_LOCATION_XYZ: theUnit->mPhysicalModel.SetLocationXYZ(p); return 0;
    case EE_COMBAT_SHOT_X_SOUND_PARAMETERS: ee_units_uck_sound(theClass, static_cast<EEUnit*>(theUnit), reinterpret_cast<EESoundObject*>(inA), &ioF[0], &ioF[1], &ioF[2]); return 0;
    case EE_COMBAT_SHOT_X_PLAY: gSoundManager->Play(reinterpret_cast<EESoundObject*>(inA)->mSampleID, ioF[0], ioF[1], ioF[2], reinterpret_cast<EESoundObject*>(inA)->mLooping, NULL); return 0;
    case EE_COMBAT_SHOT_X_ATTACK_GOAL: return (int64_t)(uintptr_t)EEShotAttack(theUnit);
    case EE_COMBAT_SHOT_X_ATTACK_TARGET: return (int64_t)(uintptr_t)reinterpret_cast<EEUGRust*>(inA)->GetAttackTarget();
    case EE_COMBAT_SHOT_X_COMPLEX_CAST: return (int64_t)(uintptr_t)EEUnitCast<EEComplexUnit>(reinterpret_cast<EEUnit*>(inA));
    case EE_COMBAT_SHOT_X_UNTINT: reinterpret_cast<EEComplexUnit*>(inA)->SetIsTinted(false); return 0;
    case EE_COMBAT_SHOT_X_MAP_TILE: return (int64_t)(uintptr_t)::gWorld.mMap->GetTile((long)inA, (long)inB);
    case EE_COMBAT_SHOT_X_ATTACK_GROUND: theUnit->SetGoal(EEUGRust::NewAttackGround(p, inB != 0), 0, 0); return 0;
    case EE_COMBAT_SHOT_X_SET_ATTACK_TARGET: reinterpret_cast<EEUGRust*>(inA)->SetAttackTarget(reinterpret_cast<EEComplexUnit*>(inB)); return 0;
    case EE_COMBAT_SHOT_X_GOAL_LOCATION: reinterpret_cast<EEUnitGoal*>(inA)->GetGoalLocation(p); return 0;
    case EE_COMBAT_SHOT_X_GOAL_B1C: return reinterpret_cast<EEUnitGoal*>(inA)->GetB1C();
    case EE_COMBAT_SHOT_X_LOCATION_MATRIX: *reinterpret_cast<GETransformation*>(inA) = theUnit->mPhysicalModel.GetLocation(); return 0;
    case EE_COMBAT_SHOT_X_SET_TRANSFORMATION: theUnit->SetTransformation(*reinterpret_cast<GETransformation*>(inA), true); return 0;
    }
    return 0;
}

}   // namespace

// A shot's ActionChanged or Update (inUpdate) within its host-read scope and its CBSO record (inFunc): ee_combat
// shots.rs over this file's host
void EECombatShotStep(int32_t inKind, bool inUpdate, uint32_t inFunc, EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEShotContext theContext = {inUnit, inClass, inKind};
    EeCombatShotHost theHost = {&theContext, EEShotOp, EEShotOp64, EEObj(inUnit), EEObj(inClass)};
    ee_combat_shot_step(inKind, inUpdate, inFunc, (uint64_t)(uintptr_t)static_cast<EEUnit*>(inUnit), &theHost);
}

// ---- EEUCMissile.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The missile's unit
// class: 0x217337..0x217c6d (after EEUCMiningBuilding.cpp; EEUCNaval.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (vtable 0x448cf8); +0x74/+0x78/+0xac are also EEUCTorpedo's (ICF).

// The pooled words 0x447230/0x447234 (0.3, the height over a target the missile is not on; 2.25, its squared
// proximity to an attack target) are ee_combat's (impact.rs)

// Returns whether a unit stands on one of the target's tiles (every part of a multi-part target, its whole footprint)
bool EEIsOnTargetTiles(EEUnit* inUnit, EEUnit* inTarget) { return ee_units_ucs_call(EE_UNITS_UCS_DO_ON_TARGET_TILES, NULL, inUnit, EEObj(inTarget), NULL) != 0; }  // ucs_shots.rs

// Answers the missile triggers: hit unit (attacks it; also true when its lifetime ran out), ground and water impact,
// rotted or dying done (always with an attack-unit goal), target dead or in a container, new target
long EEUnitClass::MissileEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    // ee_combat goals.rs condition (CBTC): the target is the attack-unit goal's; a hit unit's attack goal is set here
    return EECombatCondition(EE_COMBAT_GOAL_MISSILE, inUnit, inEvent, EECombatShotTarget(inUnit), NULL, this);
}
