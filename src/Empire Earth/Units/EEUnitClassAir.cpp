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

// (EEUCAirplane::ActionChanged's circling side: ee_units cls_carrier.rs ee_units_cr_circle_side, which EEUnitUpdate.cpp's
// host calls)

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


