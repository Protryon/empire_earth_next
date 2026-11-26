// EEUnitClassShots.cpp (new, R5b.4 round 32): the shots' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>

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
    case EE_COMBAT_SHOT_R_CLASS_ROTTING_TIME: return (int32_t)theClass->mRottingTime;
    case EE_COMBAT_SHOT_R_TILE_HEIGHT: return (int32_t)UFloatBits(theUnit->mTile->GetHeight());
    case EE_COMBAT_SHOT_R_SPLASH_EFFECT: return theClass->mSplashEffect != NULL;
    case EE_COMBAT_SHOT_R_DEATH_EFFECT: return theClass->mDeathEffect != NULL;
    case EE_COMBAT_SHOT_R_DEATH_SOUND: return theUnit->mStats->mDeathSound != NULL;
    case EE_COMBAT_SHOT_R_CARGO: return (int32_t)theUnit->GetNumCargoUnits();
    case EE_COMBAT_SHOT_R_LINE_OF_SIGHT: return (int32_t)UFloatBits(theUnit->GetLineOfSight());
    case EE_COMBAT_SHOT_R_COMPLEX_GOAL_KIND: return (int32_t)theUnit->mComplexUnit->mGoalState->mKind;
    case EE_COMBAT_SHOT_R_RANDOM: return (int32_t)UFloatBits(gPhysics->mRandom.Generate());
    case EE_COMBAT_SHOT_R_FLAMING_CHANCE: return (int32_t)UFloatBits((float)gCivPowersManager.GetPowerRecord(kCivPowerFlamingArrows)->mChance);
    case EE_COMBAT_SHOT_R_REMAINS: return theClass->mRemainsClass != NULL;
    case EE_COMBAT_SHOT_R_IS_PROJECTILE: return theClass->mIsProjectile;
    case EE_COMBAT_SHOT_R_WORLD_CLEARING: return gWorld.mClearing;
    case EE_COMBAT_SHOT_R_FREE_CAMERA: return gFreeCamera;
    case EE_COMBAT_SHOT_R_TARGET_HIT_POINTS: return (int32_t)EEShotAttack(theUnit)->GetAttackTarget()->mHitPoints;
    case EE_COMBAT_SHOT_R_POPULATION_COST: return (int32_t)theUnit->mUnitClass->GetPopulationCost();
    case EE_COMBAT_SHOT_R_PHYSICS_MODEL: return theClass->mPhysicsModel != NULL;
    case EE_COMBAT_SHOT_R_TICK: return (int32_t)gPhysics->mTickTimeMs;
    // ---- operations
    case EE_COMBAT_SHOT_SET_ANIMATION: theUnit->SetAnimation(inArg, -1, 0); break;
    case EE_COMBAT_SHOT_SET_ROTTING: theUnit->SetRottingAwayTime(inArg); break;
    case EE_COMBAT_SHOT_UPDATE_ROTTING: theUnit->UpdateRottingAwayTime(); break;
    case EE_COMBAT_SHOT_DEATH_EFFECT: gWorld.mEffectManager->CreateUnitEffect(theUnit, theClass->mDeathEffect, NULL, 0, 0, 0); break;
    case EE_COMBAT_SHOT_PHYSICS_ACTION: theUnit->SetCurrentPhysicsAction((unsigned long)inArg); break;
    case EE_COMBAT_SHOT_DROP_PATH: theUnit->DropPath(); break;
    case EE_COMBAT_SHOT_UNLOAD_CARGO: theUnit->UnloadCargoAtTile((long)UBitsFloat(inArg), (long)UBitsFloat(inArg1), false); break;
    case EE_COMBAT_SHOT_KILL_CARGO: theUnit->KillCargo(); break;
    case EE_COMBAT_SHOT_FIRE_SHOT: theClass->FireShot(theUnit); break;
    case EE_COMBAT_SHOT_SIGHT_OFF: theUnit->SightOffHere(); break;
    case EE_COMBAT_SHOT_SET_HIT_POINTS: theUnit->SetHitPoints(inArg); break;
    case EE_COMBAT_SHOT_SIGHT_ON: theUnit->SightOnHere(); break;
    case EE_COMBAT_SHOT_CREATE_CALAMITY:
        USingleton<EECalamityManager>::GetInstance()->CreateUnitCalamity(
            0x18, -1, static_cast<EEUGRust*>(theUnit->mComplexUnit->mGoalState)->GetAttackTarget()->GetUnitID(),
            theUnit->mPlayer->GetPlayerID());
        break;
    case EE_COMBAT_SHOT_START_ACTION: theUnit->StartAction((unsigned long)inArg); break;
    case EE_COMBAT_SHOT_ACTION_CHANGED: theClass->ActionChanged(theUnit); break;
    case EE_COMBAT_SHOT_CREATE_REMAINS: theClass->CreateRemainsUnit(theUnit); break;
    case EE_COMBAT_SHOT_HANDLE_ACTION_CHANGED: theClass->HandleActionChanged(theUnit); break;
    case EE_COMBAT_SHOT_POPULATION: theUnit->mPlayer->mPopulationCount -= inArg; break;
    case EE_COMBAT_SHOT_MINIMAP_OFF: ee_gamecpp_minimap_move_unit(gMiniMap, theUnit, theUnit->mTileX, theUnit->mTileY, -10000, (long)(-10000)); break;
    case EE_COMBAT_SHOT_STEP_PHYSICS: theClass->mPhysicsModel->Step(theUnit); break;
    case EE_COMBAT_SHOT_ORBITALS: theUnit->UpdateOrbitals(); break;
    case EE_COMBAT_SHOT_EFFECTS: theClass->UpdateEffects(theUnit); break;
    case EE_COMBAT_SHOT_REFRESH_SOUND: theClass->RefreshSound(theUnit); break;
    case EE_COMBAT_SHOT_SET_OPACITY: theUnit->SetOpacityChain(UBitsFloat(inArg)); break;
    case EE_COMBAT_SHOT_SET_FLIGHT_TIME: theUnit->SetFlightTime(inArg); break;
    case EE_COMBAT_SHOT_SET_CLOAKED: theUnit->mCloaked = (unsigned long)(long)inArg; break;
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
    case EE_COMBAT_SHOT_X_UNIT_TYPE_OF: return reinterpret_cast<EEUnit*>(inA)->mUnitClass->mUnitType;
    case EE_COMBAT_SHOT_X_SET_CONTAINS: return reinterpret_cast<EEUnitSet*>(inA)->Contains((unsigned long)inB);
    case EE_COMBAT_SHOT_X_POSITION: theUnit->GetPosition(p); return 0;
    case EE_COMBAT_SHOT_X_LOCATION: p.SetXYZ(theUnit->mPhysicalModel.GetLocation().GetX(), theUnit->mPhysicalModel.GetLocation().GetY(), theUnit->mPhysicalModel.GetLocation().GetZ()); return 0;
    case EE_COMBAT_SHOT_X_SPLASH_AT: gWorld.mEffectManager->CreatePlacedEffect(p, theClass->mSplashEffect, NULL); return 0;
    case EE_COMBAT_SHOT_X_GROUND_POINT: ::gWorld.mMap->GetGroundPoint(GE3DPoint(p), p); return 0;
    case EE_COMBAT_SHOT_X_SET_LOCATION_XYZ: theUnit->mPhysicalModel.SetLocationXYZ(p); return 0;
    case EE_COMBAT_SHOT_X_DEATH_SOUND: return (int64_t)(uintptr_t)theUnit->mStats->mDeathSound;
    case EE_COMBAT_SHOT_X_SOUND_PARAMETERS: theClass->GetSoundParameters(theUnit, reinterpret_cast<EESoundObject*>(inA), &ioF[0], &ioF[1], &ioF[2]); return 0;
    case EE_COMBAT_SHOT_X_PLAY: gSoundManager->Play(reinterpret_cast<EESoundObject*>(inA)->mSampleID, ioF[0], ioF[1], ioF[2], reinterpret_cast<EESoundObject*>(inA)->mLooping, NULL); return 0;
    case EE_COMBAT_SHOT_X_ATTACK_GOAL: return (int64_t)(uintptr_t)EEShotAttack(theUnit);
    case EE_COMBAT_SHOT_X_ATTACK_TARGET: return (int64_t)(uintptr_t)reinterpret_cast<EEUGRust*>(inA)->GetAttackTarget();
    case EE_COMBAT_SHOT_X_COMPLEX_CAST: return (int64_t)(uintptr_t)EEUnitCast<EEComplexUnit>(reinterpret_cast<EEUnit*>(inA));
    case EE_COMBAT_SHOT_X_UNTINT: reinterpret_cast<EEComplexUnit*>(inA)->SetIsTinted(false); return 0;
    case EE_COMBAT_SHOT_X_TILE_X_OF: return reinterpret_cast<EEUnit*>(inA)->mTileX;
    case EE_COMBAT_SHOT_X_TILE_Y_OF: return reinterpret_cast<EEUnit*>(inA)->mTileY;
    case EE_COMBAT_SHOT_X_MAP_TILE: return (int64_t)(uintptr_t)::gWorld.mMap->GetTile((long)inA, (long)inB);
    case EE_COMBAT_SHOT_X_TILE_HEIGHT: ioF[0] = reinterpret_cast<EEMapTile*>(inA)->GetHeight(); return 0;
    case EE_COMBAT_SHOT_X_ATTACK_GROUND: theUnit->SetGoal(EEUGRust::NewAttackGround(p, inB != 0), 0, 0); return 0;
    case EE_COMBAT_SHOT_X_COMPLEX_OF: return (int64_t)(uintptr_t)reinterpret_cast<EEUnit*>(inA)->mComplexUnit;
    case EE_COMBAT_SHOT_X_CONTAINER_OF: return (int64_t)(uintptr_t)reinterpret_cast<EEComplexUnit*>(inA)->mContainer;
    case EE_COMBAT_SHOT_X_SET_ATTACK_TARGET: reinterpret_cast<EEUGRust*>(inA)->SetAttackTarget(reinterpret_cast<EEComplexUnit*>(inB)); return 0;
    case EE_COMBAT_SHOT_X_GOAL_STATE: return (int64_t)(uintptr_t)theUnit->mGoalState;
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
    EeCombatShotHost theHost = {&theContext, EEShotOp, EEShotOp64};
    ee_combat_shot_step(inKind, inUpdate, inFunc, (uint64_t)(uintptr_t)static_cast<EEUnit*>(inUnit), &theHost);
}

// ---- EEUCBomb.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The bomb's unit class:
// 0x20e2cb..0x20e58d (after EEUCBasicPriest.cpp; EEUCBuilding.cpp follows). Unit flags: the exe's module set. Its ctor
// is inline (EEUnitClass.cpp's factory). Order: VirtualAC (the drop), VirtualB8 with its function-static colour's
// empty atexit dtor, then EEUnitClass::LaunchStoredMissile (the carrier drops its first bomb; EEUnitClass::FireShot calls it).

// Drops the bomb from its carrier: aims it along the carrier's heading, pitched down by the launcher's tilt (0 to
// -90 degrees), with the launch speed's horizontal part; always returns true
// Flies the bomb; an SAS explosive (unit type 0x305) makes its target blink red every half second while it attacks
// Has a carrier drop its first cargo unit (a bomb) at its target and set it moving
// ---- EEUCMissile.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The missile's unit
// class: 0x217337..0x217c6d (after EEUCMiningBuilding.cpp; EEUCNaval.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (vtable 0x448cf8); +0x74/+0x78/+0xac are also EEUCTorpedo's (ICF).

// The pooled words 0x447230/0x447234 (0.3, the height over a target the missile is not on; 2.25, its squared
// proximity to an attack target) are ee_combat's (impact.rs)

// The original first argument reads only the base tile. Its sole ComplexUnit caller does not uniquely
// recover narrower pointer specificity; this declaration records the minimum proven contract.
bool EEIsOnTargetTiles(EEUnit* inUnit, EEUnit* inTarget);

// Returns whether the missile hit the ground after its flight time (0.3 higher over its target): sets it down there
// and, for an attack-ground goal, leaves a shell hole and returns true (never for a missile base's missiles)
// Returns whether a unit stands on one of the target's tiles (every part of a multi-part target, its whole footprint)
bool EEIsOnTargetTiles(EEUnit* inUnit, EEUnit* inTarget) { return ee_units_ucs_call(EE_UNITS_UCS_DO_ON_TARGET_TILES, NULL, inUnit, EEObj(inTarget), NULL) != 0; }  // ucs_shots.rs

// Returns whether the missile hit water (z 0 or below on terrain it cannot cross); if so, it attacks the ground there

// Sets up a new missile: records its start point and stops it; all but the ICBM start the flight animation and advance
// Launches the missile: tilts it to its launcher's launch pitch and starts the launch delay and a 300 s lifetime; true
// Answers the missile triggers: hit unit (attacks it; also true when its lifetime ran out), ground and water impact,
// rotted or dying done (always with an attack-unit goal), target dead or in a container, new target
long EEUnitClass::MissileEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    // ee_combat goals.rs condition (CBTC): the target is the attack-unit goal's; a hit unit's attack goal is set here
    return EECombatCondition(EE_COMBAT_GOAL_MISSILE, inUnit, inEvent, EECombatShotTarget(inUnit), NULL, this);
}

// ---- EEUCTorpedo.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The torpedo's unit class:
// 0x21b163..0x21b27a (after EEUCSubmergedNaval.cpp; EEUCTransport.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline; +0xe4 is ICF-folded (0x29059d: false).

// BombUpdate's blink colour (its function static until round 7)
static GEColor sBombBlinkColor(0.9f, 0.0f, 0.2f);

template <class T> static T* EEUcsObject(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

// The C++ of the shots' Rust bodies (ee_units ucs_shots.rs; EE_UNITS_UCS_*): one statement of the original bodies
// each, in their order; the class host's statements (EE_UNITS_UC_*) pass through to EEUnitClass.cpp's
extern "C" int64_t ee_units_ucs_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                     float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EEUcsObject<EEComplexUnit>(inA);
    float* f = *ioF;
    switch (inOp) {
    // the impact tests' (ucs_impact.rs)
    case EE_UNITS_UCS_GOAL_STATE: return EEObj(theUnit->mGoalState);
    case EE_UNITS_UCS_IS_BOMB: return theClass->mCategory == kUnitCategoryBomb;
    case EE_UNITS_UCS_IMPACT_FLIGHT_TIME: return theUnit->mFlightTime;
    case EE_UNITS_UCS_START_Z: f[0] = theUnit->mProjectileStart.GetZ(); return 0;
    case EE_UNITS_UCS_POSITION: theUnit->GetPosition(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCS_TILE_PRESENT: return ::gWorld.mMap->GetTile((long)inB, (long)inC) != NULL;
    case EE_UNITS_UCS_GOAL_KIND: return EEUcsObject<EEUnitGoal>(inA)->mKind;
    case EE_UNITS_UCS_SHELL_HOLE: return theUnit->GetUnitClass()->mShellHoleEffect != NULL;
    case EE_UNITS_UCS_CAPTURE_IMPACT:
        EECombatCapture(inB != 0 ? EE_COMBAT_TAG('C', 'B', 'W', 'I') : EE_COMBAT_TAG('C', 'B', 'G', 'I'), *EEUcsObject<EeCombatImpactIn>(inA), *EEUcsObject<EeCombatImpactOut>((uint64_t)inC));
        return 0;
    case EE_UNITS_UCS_ATTACK_GROUND: theUnit->SetGoal(EEUGRust::NewAttackGround(GE3DPoint(f[0], f[1], f[2]), false), 0, 0); return 0;
    case EE_UNITS_UCS_DECAL: EEPlaceImpactDecal(theUnit, *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCS_LOCATION: *reinterpret_cast<GE3DPoint*>(f) = GE3DPoint(theUnit->mPhysicalModel.GetLocation()); return 0;
    case EE_UNITS_UCS_LAUNCHER_CP_BASE: return theUnit->mLauncherClass->mUnitType == kUnitTypeCpMissileBase;
    case EE_UNITS_UCS_LAUNCHER_MISSILE_BASE: return theUnit->mLauncherClass->mUnitType == kUnitTypeMissileBase;
    case EE_UNITS_UCS_PLACE_ON_GROUND: theUnit->PlaceOnGround(GE3DPoint(f[0], f[1], f[2])); return 0;
    case EE_UNITS_UCS_GROUND_POINT: ::gWorld.mMap->GetGroundPoint(GE3DPoint(f[0], f[1], f[2]), *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCS_TILE_HEIGHT: f[0] = theUnit->mTile->GetHeight(); return 0;
    case EE_UNITS_UCS_ATTACK_TARGET: return EEObj(static_cast<EEUGRust*>(theUnit->mGoalState)->GetAttackTarget());
    case EE_UNITS_UCS_TARGET_BUILDING: return EEUcsObject<EEUnit>(inA)->GetUnitClass()->IsLandBuilding() || EEUcsObject<EEUnit>(inA)->GetUnitClass()->mIsWall;
    case EE_UNITS_UCS_IMPACT_TILE_X: return EEUcsObject<EEUnit>(inA)->GetTileX();
    case EE_UNITS_UCS_IMPACT_TILE_Y: return EEUcsObject<EEUnit>(inA)->GetTileY();
    case EE_UNITS_UCS_IMPACT_TILE_SIZE: return EEUcsObject<EEUnit>(inA)->GetUnitClass()->mTileSize;
    case EE_UNITS_UCS_GOAL_LOCATION: theUnit->mGoalState->GetGoalLocation(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCS_ON_TARGET_TILES: return EEIsOnTargetTiles(theUnit, static_cast<EEUGRust*>(theUnit->GetGoal())->GetAttackTarget());
    case EE_UNITS_UCS_SAME_TILE: return ::gWorld.mMap->GetTile(UMFloatToLong(f[0]), UMFloatToLong(f[1])) == theUnit->mTile;
    case EE_UNITS_UCS_CAN_MOVE: return theClass->CanMoveOnTerrain(theUnit, theUnit->mTile->mTerrainType);
    case EE_UNITS_UCS_SET_CONTACT_LOCATION: theUnit->mInitialContactLocation = GE3DPoint(f[0], f[1], f[2]); return 0;
    case EE_UNITS_UCS_FULL_SPEED: theUnit->SetVelocity(theUnit->mStats->GetMaxVelocity()); return 0;
    case EE_UNITS_UCS_UNIT_TYPE: return theUnit->mUnitClass->mUnitType;
    case EE_UNITS_UCS_BALLISTIC_PITCH: return EEGetBallisticPitch(theUnit->mOwner, theUnit->mPlayer->mUnitClassUpgrades[theUnit->mLauncherClass->mUnitType], EEUcsObject<GE3DPoint>(inA), EEUcsObject<GE3DPoint>((uint64_t)inB), &f[0]);
    case EE_UNITS_UCS_SET_FLIGHT_TIME: theUnit->SetFlightTime((unsigned long)inB); return 0;
    case EE_UNITS_UCS_LAUNCH_SPEED: f[0] = theUnit->mPlayer->mUnitClassUpgrades[theUnit->mLauncherClass->mUnitType]->mLaunchSpeed; return 0;
    case EE_UNITS_UCS_UPGRADE_LAUNCH_SPEED: f[0] = theUnit->mPlayer->GetUpgrade(theUnit->mLauncherClass->mUnitType)->mLaunchSpeed; return 0;
    case EE_UNITS_UCS_LAUNCH_PITCH: f[0] = theUnit->mLauncherClass->mLaunchPitch; return 0;
    case EE_UNITS_UCS_HAS_TRAIL: return theClass->mTrailEffect != NULL;
    case EE_UNITS_UCS_TRAIL_DETAILED: return theClass->mTrailEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCS_HAS_RIPPLE: return theClass->mRippleEffect != NULL;
    case EE_UNITS_UCS_RIPPLE_DETAILED: return theClass->mRippleEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCS_COMPLEX_CAST: return EEObj(EEUnitCast<EEComplexUnit>(EEUcsObject<EEUnit>(inA)));
    case EE_UNITS_UCS_FLIGHT_TIME: return theUnit->mFlightTime;
    case EE_UNITS_UCS_SET_TINTED: theOther->SetIsTinted(inB != 0); return 0;
    case EE_UNITS_UCS_SET_BLINK_COLOR: theOther->SetColor(sBombBlinkColor); return 0;
    case EE_UNITS_UCS_PROJECTILE_UPDATE: theClass->ProjectileUpdate(theUnit); return 0;
    case EE_UNITS_UCS_FIND_DROP: return EEObj(EECombatFindDropLocation(theUnit, NULL, *reinterpret_cast<GE3DPoint*>(f)));
    case EE_UNITS_UCS_FIRST_CARGO: return EEObj(theUnit->mCargo[0]);
    case EE_UNITS_UCS_UNLOAD_NOW: theUnit->UnloadCargoNow(theOther); return 0;
    case EE_UNITS_UCS_LAUNCH_SHOT: theClass->LaunchShot(theUnit, EEUcsObject<EEUnit>(inA), *reinterpret_cast<GE3DPoint*>(f), EEUcsObject<EEComplexUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UCS_COMPLEX_FIELD: return EEObj(EEUcsObject<EEUnit>(inA)->mComplexUnit);
    case EE_UNITS_UCS_TILE_OF: return EEObj(EEUcsObject<EEUnit>(inA)->mTile);
    case EE_UNITS_UCS_PREVIOUS_PART: return EEObj(theOther->mPreviousPart);
    case EE_UNITS_UCS_NEXT_PART: return EEObj(theOther->mNextPart);
    case EE_UNITS_UCS_TILE_SIZE_OF: return theOther->mUnitClass->mTileSize;
    case EE_UNITS_UCS_TILE_X_OF: return theOther->mTileX;
    case EE_UNITS_UCS_TILE_Y_OF: return theOther->mTileY;
    case EE_UNITS_UCS_SET_LIFETIME: theUnit->mCloaked = 300000; return 0;
    case EE_UNITS_UCS_SET_LAUNCH_DELAY: theUnit->SetFlightTime(theUnit->mLauncherClass->mMissileDelayTime); return 0;
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
