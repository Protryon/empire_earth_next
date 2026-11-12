// New (port; docs/RUST_PLAN.md R5b.4 leaf 6, the twelfth and thirteenth rounds): a unit class's Update as a Rust
// sequence (rust/crates/ee_units/src/update.rs): which of the unit's per-tick leaves run, in which order, decided over
// the unit store, each leaf a call of this host (EEUnitUpdate.cpp). inKind: ee_units' EE_UNITS_UPDATE_*.
#ifndef EE_EEUNITUPDATE_H
#define EE_EEUNITUPDATE_H

class EEUnitClass;
class EEComplexUnit;

void EEUnitClassUpdate(int inKind, EEUnitClass* inClass, EEComplexUnit* inUnit);
// a class's ActionChanged as a Rust decision (rust/crates/ee_units/src/actions.rs; inKind: EE_UNITS_ACTION_*)
void EEUnitClassActionChanged(int inKind, EEUnitClass* inClass, EEComplexUnit* inUnit);

// the fourteenth round's ActionChanged operations that are more than a call, kept beside their classes (their files'
// helpers and constants): each the original's statements of its branch, called by the host (EEUnitUpdate.cpp)
#include <stdint.h>
void EEHelicopterPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit);           // EEUCHelicopter.cpp
void EEHelicopterTransportPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit);  // EEUCHelicopterTransport.cpp
void EEHelicopterPriestPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit);     // EEUCHelicopterPriest.cpp
void EEAirAnimalPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit);            // EEUCAirAnimal.cpp
void EEAirAnimalDeathRoll(EEUnitClass* inClass, EEComplexUnit* inUnit);
void EEAirplaneCircleSide(EEUnitClass* inClass, EEComplexUnit* inUnit);             // EEUCAirplane.cpp
void EEAirplaneHoldingPattern(EEUnitClass* inClass, EEComplexUnit* inUnit);
void EEPlanetSpaceshipHoldingPattern(EEUnitClass* inClass, EEComplexUnit* inUnit);  // EEUCPlanetSpaceship.cpp
// EEUCLandable.cpp: one of the landable's operations (EE_UNITS_ACT_*); inoutAirport the nearest airport found
int64_t EELandableActionOperation(int inOperation, EEUnitClass* inClass, EEComplexUnit* inUnit, int64_t inA,
                                  EEComplexUnit** inoutAirport);

#endif
