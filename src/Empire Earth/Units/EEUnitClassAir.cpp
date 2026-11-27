// EEUnitClassAir.cpp (new, R5b.4 round 32): the aircraft and spaceships' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitUtilities.h"

// ---- EEUCLandable.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The unit class of the
// aircraft that land (airplanes, bombers, the animals' and planet ships' bases): 0x213c60..0x216c48 (after
// EEUCLand.cpp; EEUCLandCitizen.cpp follows). One unit per EEUC* class in alphabetical order. Unit flags: the exe's
// module set. Its ctor is inline (EEUnitClass.cpp's factory). Order: the landing (ReturnToAirport), the take-off point
// (RouteDockWaypoint), Virtual24, Virtual2C, the approach (FlyToGoal), the ground height (GetGroundHeight), GetNextFlightPoint, Virtual74,
// EEComplexUnit::SetInitialContactLocation, Virtual78, the bomb drop (IsBombOnTarget), the carrier approach (ApproachCarrier, TakeOffFromCarrier), the
// flight state (GetNextFlightState), Virtual88, the runway approaches (ApproachRunway and its four wrappers, TakeOffFromRunway and its four),
// IsAwayFromGoal, VirtualA0, VirtualE0, VirtualB8, EEComplexUnit::SetOpacityChain. EEUCAirAnimal.cpp's bodies are the twins of
// several of these (same sizes).

// 0x447038..0x447050: the unit's named float constants (one .rdata block per unit): uca_air.rs's now

// cdecl helpers (EEUCHelicopter.cpp declares the first the same way) (provisional names)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);

// New (R5b.4, the fourteenth round): EEUCLandable::ActionChanged's operations that are more than a call, each the
// original's statements of its branch (ee_units actions.rs decides which, in which order; EEUnitUpdate.cpp's host
// calls this with the operation code). inoutAirport: FIND_NEAREST_AIRPORT's result, for SET_HOME_BASE_FOUND.
int64_t EELandableActionOperation(int inOperation, EEUnitClass* inClass, EEComplexUnit* inUnit, int64_t inA,
                                  EEComplexUnit** inoutAirport)
{
    return ee_units_uca_call(EE_UNITS_UCA_DO_LANDABLE_OP, inClass, inUnit, EEObj(inoutAirport), inOperation, inA, NULL);   // uca_air.rs
}

// ---- EEUCAirplane.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The airplane's unit
// class: 0x20b192..0x20b9fc (after EEUCAirAnimal.cpp; EEUCAmbient.cpp follows). Unit flags: the exe's module set. Its
// ctor is inline (EEUnitClass.cpp's factory). Order: Virtual74 with EEAirTrafficControl::GetNumSlots's COMDAT after
// it, EEIsTileFree, EECreateCarrierAircraft (the carrier's aircraft), FlyOffMap (off the map edge), VirtualBC, VirtualB8.
// Virtual74 is EEUCPlanetSpaceship's twin (0x218e34).

// New (R5b.4, the fourteenth round): EEUCAirplane::ActionChanged's operations that are more than a call (the original's
// statements; ee_units actions.rs decides when, EEUnitUpdate.cpp's host calls them): circling's side (on space maps
// toward the side it can reach; EE_UNITS_ACT_CIRCLE_SIDE), the holding pattern over its home airport (killed when
// there is none; EE_UNITS_ACT_HOLDING_PATTERN 0)
void EEAirplaneCircleSide(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    (void)inClass;
    ee_units_cr_circle_side(EEObj(inUnit));   // ee_units cls_carrier.rs
}

void EEAirplaneHoldingPattern(EEUnitClass* inClass, EEComplexUnit* inUnit) { ee_units_cr_holding_pattern(EEObj(inClass), EEObj(inUnit)); }  // ee_units cls_carrier.rs

// Returns whether a tile is on the map, not cliff, deep water or space, and not on the map edge
bool EEIsTileFree(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    return ee_units_uc_rest(EE_UNITS_UC_DO_R_TILE_FREE, NULL, inUnit, 0, (int64_t)inX, (int64_t)inY, NULL) != 0;   // uc_rest.rs
}

// Fills a new carrier with aircraft of the best of three fighter techs its player has (else unit class 0x1f)
void EECreateCarrierAircraft(EEComplexUnit* inUnit, long inPlayer) { ee_units_uca_call(EE_UNITS_UCA_DO_CARRIER_AIRCRAFT, NULL, inUnit, 0, inPlayer, 0, NULL); }  // uca_air.rs

// ---- EEUCAirAnimal.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The flying animal's
// unit class: 0x20a279..0x20b192 (after EESelectedGroup.cpp's sort COMDATs; EEUCAirplane.cpp follows). One unit per
// EEUC* class in alphabetical order (AirAnimal, Airplane, Ambient, BasicCitizen, BasicPriest, Bomb, Building, ...).
// Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); ReturnToAirport (the landing on an
// airport, used by Virtual74) comes first, then the slots +0x24, +0x2c, +0x74, +0x78, +0x88, +0xa0, (+0xc0 folded)
// +0xb8 in source order. Bodies are the EEUCLandable twins' (0x213ed2.., same sizes) with the animal's states.

// New (R5b.4, the fourteenth round): EEUCAirAnimal::ActionChanged's operations that are more than a call (the
// original's statements; ee_units actions.rs decides when, EEUnitUpdate.cpp's host calls them): the path to the goal
// when preparing to move (EE_UNITS_ACT_AIR_ANIMAL_PATH), the dying spin (EE_UNITS_ACT_DEATH_ROLL 0)
void EEAirAnimalPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit) { ee_units_uca_call(EE_UNITS_UCA_DO_AIR_ANIMAL_PATH, inClass, inUnit, 0, 0, 0, NULL); }  // uca_air.rs

void EEAirAnimalDeathRoll(EEUnitClass* inClass, EEComplexUnit* inUnit) { ee_units_uca_call(EE_UNITS_UCA_DO_AIR_ANIMAL_DEATH_ROLL, inClass, inUnit, 0, 0, 0, NULL); }  // uca_air.rs

// ---- EEUCPlanetSpaceship.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The planet
// spaceship's unit class: 0x218e34..0x219143 (after EEUCNavalTransport.cpp; EEUCProjectile.cpp's +0xe0 0x219143
// follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); one own slot, +0x74.

// New (R5b.4, the fourteenth round): EEUCPlanetSpaceship::ActionChanged's holding pattern over its home airport (dies
// without one; the original's statements; EEUnitUpdate.cpp's EE_UNITS_ACT_HOLDING_PATTERN 1 calls it)
void EEPlanetSpaceshipHoldingPattern(EEUnitClass* inClass, EEComplexUnit* inUnit) { ee_units_uca_call(EE_UNITS_UCA_DO_SPACESHIP_HOLDING, inClass, inUnit, 0, 0, 0, NULL); }  // uca_air.rs

// ---- EEUCHelicopter.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The helicopter's unit
// class: 0x212a6f..0x212edf (after EEUCGate.cpp; EEUCHelicopterPriest.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x24,
// +0x74, +0xb8 in source order (the others are ICF-folded into twins).

// New (R5b.4, the fourteenth round): EEUCHelicopter::ActionChanged's path to the goal when preparing to move (the
// original's statements; ee_units actions.rs decides when, EEUnitUpdate.cpp's EE_UNITS_ACT_HELICOPTER_PATH calls it)
void EEHelicopterPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit) { ee_units_uca_call(EE_UNITS_UCA_DO_HELICOPTER_PATH, inClass, inUnit, 0, 0, 0, NULL); }  // uca_air.rs

// ---- EEUCSpaceship.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The spaceship's unit
// class: 0x21a219..0x21a6e0 (after EEUCSpaceBuilding.cpp; EEUCSpaceTransport.cpp's +0xe4 0x21a6e0 follows). Unit
// flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x24, +0x2c, +0x74, +0x78,
// +0xa0, +0xb8 in source order.

// The air animal's and the landable's sound parameters: the spaceship's Doppler pitch, but (as their own inline
// bodies were) without the unit utilities' UUTL note of EEGetDopplerPitch
void EEUnitClass::AirGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                                        float* outPitch)
{
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    float theCamera[3] = {::gWorld.mTransformation.GetX(), ::gWorld.mTransformation.GetY(), ::gWorld.mTransformation.GetZ()};
    ee_units_uu_doppler_pitch_unnoted(*outVolume, inUnit->mComplexUnit->mVelocity, (const float(*)[4][4])&inUnit->mComplexUnit->mPhysicalModel.GetLocation().mMatrix[0][0], &theCamera, outPitch);   // ee_units uu_world.rs doppler_pitch
}

template <class T> static T* EEUcaObject(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

// GetGroundHeight's closest tile (its x and y are not read)
static long sUcaTile[2];   // x, y

// The C++ of the air classes' Rust bodies (ee_units uca_air.rs; EE_UNITS_UCA_*): one statement of the original
// bodies each, in their order; the class host's statements (EE_UNITS_UC_*) pass through to EEUnitClass.cpp's
extern "C" int64_t ee_units_uca_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                     float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EEUcaObject<EEComplexUnit>(inA);
    EEUnit* theSimple = EEUcaObject<EEUnit>(inA);
    EEUGRust* theGoal = EEUcaObject<EEUGRust>(inA);
    EeUnitsAtc* theAtc = EEUcaObject<EeUnitsAtc>(inA);
    float* f = *ioF;
    GE3DPoint& p = *reinterpret_cast<GE3DPoint*>(f);
    switch (inOp) {
    case EE_UNITS_UCA_HAS_LOCATION: return EEGoalHasLocation(theGoal);
    case EE_UNITS_UCA_GOAL_CAST: return EEObj(EERustGoalCast(theGoal, (EEUnitGoalKind)inB));
    case EE_UNITS_UCA_LOCATION_POINT: p = EELocationGoalPoint(theGoal); return 0;
    case EE_UNITS_UCA_PATROL_TILE_X: return theGoal->GetPatrolTarget()->GetTileX();
    case EE_UNITS_UCA_PATROL_TILE_Y: return theGoal->GetPatrolTarget()->GetTileY();
    case EE_UNITS_UCA_EXPLORE_CENTER: theGoal->GetExploreTileCenter(p); return 0;
    case EE_UNITS_UCA_GOAL_STEP: theGoal->Step(theUnit); return 0;
    case EE_UNITS_UCA_GROUND_TARGET: p = theGoal->GetGroundTarget(); return 0;
    case EE_UNITS_UCA_GOAL_LOCATION: theGoal->GetGoalLocation(p); return 0;
    case EE_UNITS_UCA_ATTACK_TARGET: return EEObj(theGoal->GetAttackTarget());
    case EE_UNITS_UCA_HAS_PROJECTILE: return theUnit->mUnitClass->mProjectileClass != NULL;
    case EE_UNITS_UCA_FIND_DROP: return EEObj(EECombatFindDropLocation(theUnit, NULL, p));
    case EE_UNITS_UCA_LOCATION_OF: p = GE3DPoint(theOther->mPhysicalModel.GetLocation().GetX(), theOther->mPhysicalModel.GetLocation().GetY(), theOther->mPhysicalModel.GetLocation().GetZ()); return 0;
    case EE_UNITS_UCA_CRUISE: f[0] = theClass->GetCruiseAltitude(); return 0;
    case EE_UNITS_UCA_LANDING_POINT: EEFindLandingPoint(theUnit, &p); return 0;
    case EE_UNITS_UCA_GET_TILE: return EEObj(::gWorld.mMap->GetTile((long)inB, (long)inC));
    case EE_UNITS_UCA_CLOSEST_TILE: return EEObj(::gWorld.mMap->GetClosestTile(&p, &sUcaTile[0], &sUcaTile[1]));
    case EE_UNITS_UCA_TILE_HEIGHT_OF: f[0] = EEUcaObject<EEMapTile>(inA)->GetHeight(); return 0;
    case EE_UNITS_UCA_ATC_REMOVE_PLANE: ee_units_atc_remove_plane(theAtc, theUnit->mHomeBase, theUnit); return 0;
    case EE_UNITS_UCA_HOME_LOAD: theUnit->mHomeBase->LoadCargo(theUnit, 1, 0); return 0;
    case EE_UNITS_UCA_SET_COMMANDABLE: theUnit->SetIsCommandable(inB != 0); return 0;
    case EE_UNITS_UCA_SET_HEIGHT: theUnit->SetHeight(f[0]); return 0;
    case EE_UNITS_UCA_FIND_NEAREST_AIRPORT: return EEObj(ee_units_atc_find_nearest_airport(theAtc, theUnit));
    case EE_UNITS_UCA_HOME_FACING: return (uint32_t)theUnit->mHomeBase->mFacing;
    case EE_UNITS_UCA_TARGETS_ENEMIES: return theUnit->mBehavior->mCanTargetEnemies;
    case EE_UNITS_UCA_FIND_TARGET_NEAR: return EEObj(EEFindTargetNear(theUnit, (long)inB, (long)inC, f[0]));
    case EE_UNITS_UCA_SET_ATTACK_TARGET: theGoal->SetAttackTarget(EEUcaObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UCA_GOAL_B1C: return theGoal->GetB1C();
    case EE_UNITS_UCA_NEW_LOCATION: return EEObj(EEUGRust::NewLocation(f[0], f[1], f[2], inB != 0, true, false, 0.0f, false, false));
    case EE_UNITS_UCA_SET_GOAL: theUnit->SetGoal(theGoal, 0, 0); return 0;
    case EE_UNITS_UCA_GOAL_START_ACTION: return theGoal->GetStartAction();
    case EE_UNITS_UCA_ATC_SLOT_COUNT: return ee_units_atc_slot_count(theAtc);
    case EE_UNITS_UCA_ATC_SLOT_AIRPORT: return EEObj(ee_units_atc_slot_airport(theAtc, (unsigned long)inB));
    case EE_UNITS_UCA_SET_STATS_FLIGHT_TIME: theUnit->SetFlightTime(theUnit->GetStats()->mFlightTime); return 0;
    case EE_UNITS_UCA_SET_AMMUNITION: theUnit->SetAmmunition(theClass->mAmmunition); return 0;
    case EE_UNITS_UCA_NEXT_WAYPOINT: return theUnit->GetNextWaypointPoint(p);
    case EE_UNITS_UCA_HEIGHT_AT: f[0] = theClass->GetHeightAt(theUnit, GE3DPoint(f[0], f[1], f[2])); return 0;
    case EE_UNITS_UCA_ATC_QUEUE_LANDING: return (int64_t)ee_units_atc_queue_request(theAtc, theUnit->mHomeBase, theUnit, 1);
    case EE_UNITS_UCA_ATTACK_GROUND_GOAL: theUnit->SetGoal(EEUGRust::NewAttackGround(p, theGoal->GetB1C()), 0, 0); return 0;
    case EE_UNITS_UCA_SET_CONTACT: theUnit->SetInitialContactLocation(p); return 0;
    case EE_UNITS_UCA_CONTACT_DIFFERS: return theUnit->mInitialContactLocation != p;
    case EE_UNITS_UCA_RANGE: f[0] = theUnit->GetStats()->mRange; return 0;
    case EE_UNITS_UCA_HIT_DISTANCE: f[0] = EECombatHitDistanceSquared(theUnit, theSimple, -10000.0f, -10000.0f); return 0;
    case EE_UNITS_UCA_DESTINATION: theGoal->GetDestination(theUnit, p); return 0;
    case EE_UNITS_UCA_AIRPLANE_MODEL: return theClass->mPhysicsModel != NULL && theClass->mPhysicsModel->IsAirplane();
    case EE_UNITS_UCA_COMPUTE_ROLL: theUnit->SetF220(theClass->mPhysicsModel->ComputeRoll(theUnit, f[0])); return 0;
    case EE_UNITS_UCA_ANGULAR_VELOCITY: p = GE3DPoint(theUnit->mAngularVelocity.GetYaw(), theUnit->mAngularVelocity.GetPitch(), theUnit->mAngularVelocity.GetRoll()); return 0;
    case EE_UNITS_UCA_SET_ANGULAR_VELOCITY: theUnit->mAngularVelocity = GEOrientation(f[0], f[1], f[2]); return 0;
    case EE_UNITS_UCA_ATC_ADD_WEIGHT: ee_units_atc_add_cargo_weight(theUnit->GetPlayer()->mAirTraffic[theUnit->GetUnitClass()->mAirTrafficType - 1], theUnit); return 0;
    case EE_UNITS_UCA_LOCATION_MATRIX: *EEUcaObject<GETransformation>(inA) = theUnit->mPhysicalModel.GetLocation(); return 0;
    case EE_UNITS_UCA_STATS_SCALE: f[0] = theUnit->GetStats()->GetScale(); return 0;
    case EE_UNITS_UCA_TAG_LIST: return EEObj(theUnit->GetModel()->mTagPoints[theUnit->mAnimation]);
    case EE_UNITS_UCA_TAG_COUNT: return EEUcaObject<EETagPointList>(inA)->mPoints.size();
    case EE_UNITS_UCA_TAG_OFFSET: EEUcaObject<EETagPointList>(inA)->mPoints[theUnit->mWeaponTagPoint]->GetOffset(theUnit->mPhysicalModel.GetFrame(), p); return 0;
    case EE_UNITS_UCA_LAUNCH_SPEED: f[0] = theUnit->GetStats()->mLaunchSpeed; return 0;
    case EE_UNITS_UCA_AREA_EFFECT: f[0] = theUnit->GetStats()->mAreaEffect; return 0;
    case EE_UNITS_UCA_CLASS_AT: return EEObj(gUnitClassTable[(size_t)inB]);
    case EE_UNITS_UCA_CREATE_CARRIED: return EEObj(EEUcaObject<EEUnitClass>(inA)->CreateUnitAt(*EEUcaObject<GETransformation>((uint64_t)inB), (long)inC, 0, theUnit)->mComplexUnit);
    case EE_UNITS_UCA_POSITION: theUnit->GetPosition(p); return 0;
    case EE_UNITS_UCA_CONVERT: return EEObj(EEConvertUnit(theUnit, 0, 0));
    case EE_UNITS_UCA_SET_FLIGHT_TIME_OF: theOther->SetFlightTime((long)inB); return 0;
    case EE_UNITS_UCA_TILE_HEIGHT: f[0] = theUnit->mTile->GetHeight(); return 0;
    case EE_UNITS_UCA_SET_ORIENTATION: theUnit->mOrientation = GEOrientation(f[0], f[1], f[2]); return 0;
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
