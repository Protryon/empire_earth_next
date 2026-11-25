// EEUnitClassAir.cpp (new, R5b.4 round 32): the aircraft and spaceships' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EETechTreeTechID.h"
#include "lle/GEModel.h"
#include "rust/sim/units.h"

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

// 0x24485b: the enemy unit near (inX, inY) within inRange that inUnit can attack (provisional name)

// Takes off from the ground: rolls 3 ahead, climbs 7 ahead to 3/4 cruising height (plus the ground height when the
// ground is higher), then flies to the goal; returns true, doing nothing without an air traffic control
// Drops the path and adds a waypoint at the home carrier's take-off point (its tag point 2; its centre without tags)
// Gets the sound parameters, giving a moving plane's sound a Doppler pitch shift against the camera


// Paths a plane from a point to its goal's location (move, patrol, explore, attack ground, unload, attack unit)
// at cruising height; an explore goal without a target picks a new area first
// Gives the ground height under a point (the closest tile's off the map; 0 under water)
// New (R5b.4, the fourteenth round): EEUCLandable::ActionChanged's operations that are more than a call, each the
// original's statements of its branch (ee_units actions.rs decides which, in which order; EEUnitUpdate.cpp's host
// calls this with the operation code). inoutAirport: FIND_NEAREST_AIRPORT's result, for SET_HOME_BASE_FOUND.
int64_t EELandableActionOperation(int inOperation, EEUnitClass* inClass, EEComplexUnit* inUnit, int64_t inA,
                                  EEComplexUnit** inoutAirport)
{
    return ee_units_uca_call(EE_UNITS_UCA_DO_LANDABLE_OP, inClass, inUnit, EEObj(inoutAirport), inOperation, inA, NULL);   // uca_air.rs
}

// Sets the unit's initial contact location
void EEComplexUnit::SetInitialContactLocation(const GE3DPoint& inPoint)
{
    if (this->mInitialContactLocation != inPoint)
        this->mInitialContactLocation = inPoint;
}

// Sets up a new plane: adds its weight to its air traffic control and starts it on the ground
// Returns whether a bomb dropped now lands within reach of the attack target along its ballistic arc (reach:
// half the area effect for the last bomb on a moving target, else 0.25 for the last bomb or none)
// Plans a plane's approach to its home carrier's deck (tag point 0): turn, descent and touch-down (inSteep: the
// full approach whatever the height), or just the deck when braking; makes the deck the move goal
// Plans a take-off from the home carrier: on take-off, sits at the deck start (tag point 0) and heads for the
// take-off point; otherwise flies off the deck end (tag point 2) towards the goal
// Returns 0 (left) or 1 (right) when the next waypoint or goal lies inside the turning circle beside or behind
// the plane, else 5 (ee_units cls_air.rs, R5b.4 round 33)
// Plans a land plane's runway approach in a direction: turn onto the line, descent and touch-down at the airport
// (a navy yard: its pier), then makes the airport the move goal (ee_units cls_air.rs, R5b.4 round 33). The four
// direction wrappers (0x616308..0x616338) and the take-off's (0x616430..0x616460) are the runway direction's table.
// Plans a runway take-off in a direction: rolls 3 tiles low, climbs to 3/4 cruising height 7 tiles out, then
// flies to the goal (ee_units cls_air.rs)
// Returns whether the move goal is more than 1 from the home carrier's deck point (tag point 0): the carrier moved
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

void EEAirplaneHoldingPattern(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    ee_units_cr_holding_pattern(EEObj(inClass), EEObj(inUnit));   // ee_units cls_carrier.rs
}

// Returns whether a tile is on the map, not cliff, deep water or space, and not on the map edge
bool EEIsTileFree(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    return ee_units_uc_rest(EE_UNITS_UC_DO_R_TILE_FREE, NULL, inUnit, 0, (int64_t)inX, (int64_t)inY, NULL) != 0;   // uc_rest.rs
}

// Fills a new carrier with aircraft of the best of three fighter techs its player has (else unit class 0x1f)
void EECreateCarrierAircraft(EEComplexUnit* inUnit, long inPlayer)
{
    ee_units_uca_call(EE_UNITS_UCA_DO_CARRIER_AIRCRAFT, NULL, inUnit, 0, inPlayer, 0, NULL);   // uca_air.rs
}

// Sends a leaving plane off the map: hands it to player 0, fuels it for 1000 s and flies it out 1000 tiles along
// its direction from the map origin
// ---- EEUCAirAnimal.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The flying animal's
// unit class: 0x20a279..0x20b192 (after EESelectedGroup.cpp's sort COMDATs; EEUCAirplane.cpp follows). One unit per
// EEUC* class in alphabetical order (AirAnimal, Airplane, Ambient, BasicCitizen, BasicPriest, Bomb, Building, ...).
// Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); ReturnToAirport (the landing on an
// airport, used by Virtual74) comes first, then the slots +0x24, +0x2c, +0x74, +0x78, +0x88, +0xa0, (+0xc0 folded)
// +0xb8 in source order. Bodies are the EEUCLandable twins' (0x213ed2.., same sizes) with the animal's states.


// Lands the animal like a plane: two waypoints ahead (on the ground, then at 3/4 of cruise altitude) and a flight there
// Gets the base sound parameters, then Doppler-shifts the pitch of a moving animal by its speed toward the camera


// New (R5b.4, the fourteenth round): EEUCAirAnimal::ActionChanged's operations that are more than a call (the
// original's statements; ee_units actions.rs decides when, EEUnitUpdate.cpp's host calls them): the path to the goal
// when preparing to move (EE_UNITS_ACT_AIR_ANIMAL_PATH), the dying spin (EE_UNITS_ACT_DEATH_ROLL 0)
void EEAirAnimalPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    ee_units_uca_call(EE_UNITS_UCA_DO_AIR_ANIMAL_PATH, inClass, inUnit, 0, 0, 0, NULL);   // uca_air.rs
}

void EEAirAnimalDeathRoll(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    ee_units_uca_call(EE_UNITS_UCA_DO_AIR_ANIMAL_DEATH_ROLL, inClass, inUnit, 0, 0, 0, NULL);   // uca_air.rs
}

// Sets up a new animal on the ground (height 0)
// ---- EEUCPlanetSpaceship.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The planet
// spaceship's unit class: 0x218e34..0x219143 (after EEUCNavalTransport.cpp; EEUCProjectile.cpp's +0xe0 0x219143
// follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); one own slot, +0x74.


// New (R5b.4, the fourteenth round): EEUCPlanetSpaceship::ActionChanged's holding pattern over its home airport (dies
// without one; the original's statements; EEUnitUpdate.cpp's EE_UNITS_ACT_HOLDING_PATTERN 1 calls it)
void EEPlanetSpaceshipHoldingPattern(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    ee_units_uca_call(EE_UNITS_UCA_DO_SPACESHIP_HOLDING, inClass, inUnit, 0, 0, 0, NULL);   // uca_air.rs
}

// ---- EEUCHelicopter.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The helicopter's unit
// class: 0x212a6f..0x212edf (after EEUCGate.cpp; EEUCHelicopterPriest.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x24,
// +0x74, +0xb8 in source order (the others are ICF-folded into twins).

// 0x249560 (cdecl): the point to fly to for the attack on inTarget (EEMap.cpp's range) (provisional name)

// New (R5b.4, the fourteenth round): EEUCHelicopter::ActionChanged's path to the goal when preparing to move (the
// original's statements; ee_units actions.rs decides when, EEUnitUpdate.cpp's EE_UNITS_ACT_HELICOPTER_PATH calls it)
void EEHelicopterPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    ee_units_uca_call(EE_UNITS_UCA_DO_HELICOPTER_PATH, inClass, inUnit, 0, 0, 0, NULL);   // uca_air.rs
}

// ---- EEUCSpaceship.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The spaceship's unit
// class: 0x21a219..0x21a6e0 (after EEUCSpaceBuilding.cpp; EEUCSpaceTransport.cpp's +0xe4 0x21a6e0 follows). Unit
// flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x24, +0x2c, +0x74, +0x78,
// +0xa0, +0xb8 in source order.

// Returns a sound's volume, pan and pitch, with a doppler shift
// The air animal's and the landable's sound parameters: the spaceship's Doppler pitch, but (as their own inline
// bodies were) without the unit utilities' UUTL note of EEGetDopplerPitch
void EEUnitClass::AirGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                                        float* outPitch)
{
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    float theCamera[3] = {::gWorld.mTransformation.GetX(), ::gWorld.mTransformation.GetY(), ::gWorld.mTransformation.GetZ()};
    ee_units_uu_doppler_pitch_unnoted(*outVolume, inUnit->mComplexUnit->mVelocity, (const float(*)[4][4])&inUnit->mComplexUnit->mPhysicalModel.GetLocation().mMatrix[0][0], &theCamera, outPitch);   // ee_units uu_world.rs doppler_pitch
}

template <class T> static T* EEUcaObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

// GetGroundHeight's closest tile (its x and y are not read)
static long sUcaX;
static long sUcaY;

// the carrier's fighters, best first (EE_UNITS_UCA_FIGHTER_*)
static const long kUcaFighters[3] = {kTechInf15Watchman, kTechInf14Guardian, kTechInf13Sentinel};

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
    case EE_UNITS_UCA_LOCATION: p = GE3DPoint(theUnit->mPhysicalModel.GetLocation().GetX(), theUnit->mPhysicalModel.GetLocation().GetY(), theUnit->mPhysicalModel.GetLocation().GetZ()); return 0;
    case EE_UNITS_UCA_GOAL_STATE: return EEObj(theUnit->mGoalState);
    case EE_UNITS_UCA_HAS_LOCATION: return EEGoalHasLocation(theGoal);
    case EE_UNITS_UCA_GOAL_CAST: return EEObj(EERustGoalCast(theGoal, (EEUnitGoalKind)inB));
    case EE_UNITS_UCA_LOCATION_POINT: p = EELocationGoalPoint(theGoal); return 0;
    case EE_UNITS_UCA_PATROL_TILE_X: return theGoal->GetPatrolTarget()->GetTileX();
    case EE_UNITS_UCA_PATROL_TILE_Y: return theGoal->GetPatrolTarget()->GetTileY();
    case EE_UNITS_UCA_EXPLORE_CENTER: theGoal->GetExploreTileCenter(p); return 0;
    case EE_UNITS_UCA_START_ACTION: theUnit->StartAction((long)inB); return 0;
    case EE_UNITS_UCA_GOAL_STEP: theGoal->Step(theUnit); return 0;
    case EE_UNITS_UCA_AIR_TRAFFIC_TYPE: return theUnit->mUnitClass->mAirTrafficType;
    case EE_UNITS_UCA_GROUND_TARGET: p = theGoal->GetGroundTarget(); return 0;
    case EE_UNITS_UCA_GOAL_LOCATION: theGoal->GetGoalLocation(p); return 0;
    case EE_UNITS_UCA_ATTACK_TARGET: return EEObj(theGoal->GetAttackTarget());
    case EE_UNITS_UCA_COMPLEX_OF: return EEObj(theSimple->mComplexUnit);
    case EE_UNITS_UCA_HAS_PROJECTILE: return theUnit->mUnitClass->mProjectileClass != NULL;
    case EE_UNITS_UCA_FIND_DROP: return EEObj(EECombatFindDropLocation(theUnit, NULL, p));
    case EE_UNITS_UCA_LOCATION_OF: p = GE3DPoint(theOther->mPhysicalModel.GetLocation().GetX(), theOther->mPhysicalModel.GetLocation().GetY(), theOther->mPhysicalModel.GetLocation().GetZ()); return 0;
    case EE_UNITS_UCA_TILE_SIZE_OF: return theSimple->mUnitClass->GetTileSize();
    case EE_UNITS_UCA_TILE_X_OF: return theSimple->GetTileX();
    case EE_UNITS_UCA_TILE_Y_OF: return theSimple->GetTileY();
    case EE_UNITS_UCA_CRUISE: f[0] = theClass->GetCruiseAltitude(); return 0;
    case EE_UNITS_UCA_LANDING_POINT: EEFindLandingPoint(theUnit, &p); return 0;
    case EE_UNITS_UCA_GET_TILE: return EEObj(::gWorld.mMap->GetTile((long)inB, (long)inC));
    case EE_UNITS_UCA_CLOSEST_TILE: return EEObj(::gWorld.mMap->GetClosestTile(&p, &sUcaX, &sUcaY));
    case EE_UNITS_UCA_TILE_HEIGHT_OF: f[0] = EEUcaObject<EEMapTile>(inA)->GetHeight(); return 0;
    case EE_UNITS_UCA_APPROACH_CARRIER: theClass->ApproachCarrier(theUnit, inB != 0); return 0;
    case EE_UNITS_UCA_ATC: return EEObj(theUnit->mPlayer->mAirTraffic[theUnit->mUnitClass->mAirTrafficType - 1]);
    case EE_UNITS_UCA_ATC_REMOVE_PLANE: ee_units_atc_remove_plane(theAtc, theUnit->mHomeBase, theUnit); return 0;
    case EE_UNITS_UCA_HOME_LOAD: theUnit->mHomeBase->LoadCargo(theUnit, 1, 0); return 0;
    case EE_UNITS_UCA_SET_HOME_BASE: theUnit->SetHomeBase(theOther); return 0;
    case EE_UNITS_UCA_SET_COMMANDABLE: theUnit->SetIsCommandable(inB != 0); return 0;
    case EE_UNITS_UCA_SET_HEIGHT: theUnit->SetHeight(f[0]); return 0;
    case EE_UNITS_UCA_TAKE_OFF_CARRIER: theClass->TakeOffFromCarrier(theUnit); return 0;
    case EE_UNITS_UCA_FIND_NEAREST_AIRPORT: return EEObj(ee_units_atc_find_nearest_airport(theAtc, theUnit));
    case EE_UNITS_UCA_HOME_BASE: return EEObj(theUnit->mHomeBase);
    case EE_UNITS_UCA_HOME_FACING: return (uint32_t)theUnit->mHomeBase->mFacing;
    case EE_UNITS_UCA_APPROACH_RUNWAY: theClass->ApproachRunway(theUnit, (long)inB, (long)inC); return 0;
    case EE_UNITS_UCA_TARGETS_ENEMIES: return theUnit->mBehavior->mCanTargetEnemies;
    case EE_UNITS_UCA_FIND_TARGET_NEAR: return EEObj(EEFindTargetNear(theUnit, (long)inB, (long)inC, f[0]));
    case EE_UNITS_UCA_SET_ATTACK_TARGET: theGoal->SetAttackTarget(EEUcaObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UCA_CLEAR_GOALS: theUnit->ClearGoals(0); return 0;
    case EE_UNITS_UCA_GOAL_B1C: return theGoal->GetB1C();
    case EE_UNITS_UCA_NEW_LOCATION: return EEObj(EEUGRust::NewLocation(f[0], f[1], f[2], inB != 0, true, false, 0.0f, false, false));
    case EE_UNITS_UCA_SET_GOAL: theUnit->SetGoal(theGoal, 0, 0); return 0;
    case EE_UNITS_UCA_GOAL_START_ACTION: return theGoal->GetStartAction();
    case EE_UNITS_UCA_ACTION_CHANGED: theClass->ActionChanged(theUnit); return 0;
    case EE_UNITS_UCA_ATC_SLOT_COUNT: return ee_units_atc_slot_count(theAtc);
    case EE_UNITS_UCA_KILL: theUnit->Kill(true, false, false); return 0;
    case EE_UNITS_UCA_CONTAINER: return EEObj(theUnit->mContainer);
    case EE_UNITS_UCA_ATC_SLOT_AIRPORT: return EEObj(ee_units_atc_slot_airport(theAtc, (unsigned long)inB));
    case EE_UNITS_UCA_RETURN_TO_AIRPORT: theClass->LandableReturnToAirport(theUnit); return 0;
    case EE_UNITS_UCA_TAKE_OFF_RUNWAY: theClass->TakeOffFromRunway(theUnit, (long)inB, (long)inC); return 0;
    case EE_UNITS_UCA_SET_STATS_FLIGHT_TIME: theUnit->SetFlightTime(theUnit->GetStats()->mFlightTime); return 0;
    case EE_UNITS_UCA_PARATROOPER_BOMBER: return theClass->mIsParatrooperBomber;
    case EE_UNITS_UCA_SET_AMMUNITION: theUnit->SetAmmunition(theClass->mAmmunition); return 0;
    case EE_UNITS_UCA_SET_ANIMATION: theUnit->SetAnimation((long)inB, -1, 0); return 0;
    case EE_UNITS_UCA_NEXT_WAYPOINT: return theUnit->GetNextWaypointPoint(p);
    case EE_UNITS_UCA_HEIGHT_AT: f[0] = theClass->GetHeightAt(theUnit, GE3DPoint(f[0], f[1], f[2])); return 0;
    case EE_UNITS_UCA_ATC_QUEUE_LANDING: return (int64_t)ee_units_atc_queue_request(theAtc, theUnit->mHomeBase, theUnit, 1);
    case EE_UNITS_UCA_ATTACK_GROUND_GOAL: theUnit->SetGoal(EEUGRust::NewAttackGround(p, theGoal->GetB1C()), 0, 0); return 0;
    case EE_UNITS_UCA_HEIGHT: f[0] = theUnit->mHeight; return 0;
    case EE_UNITS_UCA_SET_HEIGHT_FIELD: theUnit->mHeight = f[0]; return 0;
    case EE_UNITS_UCA_SET_CONTACT: theUnit->SetInitialContactLocation(p); return 0;
    case EE_UNITS_UCA_CONTACT_DIFFERS: return theUnit->mInitialContactLocation != p;
    case EE_UNITS_UCA_SET_CONTACT_FIELD: theUnit->mInitialContactLocation = p; return 0;
    case EE_UNITS_UCA_CONTACT_HEADING: f[0] = theUnit->mInitialContactHeading; return 0;
    case EE_UNITS_UCA_SET_CONTACT_HEADING: theUnit->mInitialContactHeading = f[0]; return 0;
    case EE_UNITS_UCA_GOAL_KIND: return theGoal->mKind;
    case EE_UNITS_UCA_VELOCITY_OF: f[0] = theOther->mVelocity; return 0;
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
    case EE_UNITS_UCA_GRAVITY: f[0] = theClass->mProjectileClass->mAcceleration; return 0;
    case EE_UNITS_UCA_LAUNCH_SPEED: f[0] = theUnit->GetStats()->mLaunchSpeed; return 0;
    case EE_UNITS_UCA_AREA_EFFECT: f[0] = theUnit->GetStats()->mAreaEffect; return 0;
    case EE_UNITS_UCA_AMMUNITION: return theUnit->mAmmunition;
    case EE_UNITS_UCA_PLAYER_ID: return theUnit->mPlayer->mPlayerID;
    case EE_UNITS_UCA_FIGHTER_USABLE: return ::gWorld.mPlayers[inB]->mTechnologies->Get(kUcaFighters[inC])->GetIsAvailable() && ::gWorld.mPlayers[inB]->mTechnologies->Get(kUcaFighters[inC])->mIsAllowed;
    case EE_UNITS_UCA_FIGHTER_CLASS: return ::gWorld.mPlayers[inB]->mTechnologies->Get(kUcaFighters[inC])->mData->mUnitClass;
    case EE_UNITS_UCA_CLASS_AT: return EEObj(gUnitClassTable[(size_t)inB]);
    case EE_UNITS_UCA_CARGO_COUNT: return theUnit->GetNumCargoUnits();
    case EE_UNITS_UCA_CAPACITY: return theUnit->mUnitClass->mTransportCapacity;
    case EE_UNITS_UCA_CREATE_CARRIED: return EEObj(EEUcaObject<EEUnitClass>(inA)->CreateUnitAt(*EEUcaObject<GETransformation>((uint64_t)inB), (long)inC, 0, theUnit)->mComplexUnit);
    case EE_UNITS_UCA_LOAD_CARGO: theUnit->LoadCargo(theOther, 1, 0); return 0;
    case EE_UNITS_UCA_POSITION: theUnit->GetPosition(p); return 0;
    case EE_UNITS_UCA_CONVERT: return EEObj(EEConvertUnit(theUnit, 0, 0));
    case EE_UNITS_UCA_SET_FLIGHT_TIME_OF: theOther->SetFlightTime((long)inB); return 0;
    case EE_UNITS_UCA_DROP_PATH_OF: theOther->DropPath(); return 0;
    case EE_UNITS_UCA_DROP_PATH: theUnit->DropPath(); return 0;
    case EE_UNITS_UCA_TILE_HEIGHT: f[0] = theUnit->mTile->GetHeight(); return 0;
    case EE_UNITS_UCA_SET_ORIENTATION: theUnit->mOrientation = GEOrientation(f[0], f[1], f[2]); return 0;
    case EE_UNITS_UCA_CRUISE_FIELD: f[0] = theClass->mCruiseAltitude; return 0;
    case EE_UNITS_UCA_OPACITY_OF: f[0] = theOther->mOpacity; return 0;
    case EE_UNITS_UCA_SET_OPACITY_OF: theOther->mOpacity = f[0]; return 0;
    case EE_UNITS_UCA_NEXT_PART_OF: return EEObj(theOther->mNextPart);
    case EE_UNITS_UCA_HOME_TILE_X: return theUnit->mHomeBase->GetTileX();
    case EE_UNITS_UCA_HOME_TILE_Y: return theUnit->mHomeBase->GetTileY();
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
