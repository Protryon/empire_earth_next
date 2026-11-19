// EEAirTrafficControl.h (RECONSTRUCTION; file name inferred). One per player and air traffic type: the airports
// (carrying capacity and landing / takeoff queues) the player's aircraft are routed to.
// port (the units' state lane, round 1): the class is gone. Its state and bodies are Rust's (ee_units atc.rs,
// atc_save.rs; an EEPlayer holds four EeUnitsAtc* in mAirTraffic); the holders call the ee_units_atc_* entry points.
// EEAirTrafficControl.cpp is the host the controls call (the units' world calls) and the saved game's adapters.
// Requires lle/LowLevelEngine.h.
#ifndef EE_EEAIRTRAFFICCONTROL_H
#define EE_EEAIRTRAFFICCONTROL_H

#include <vector>

#include "rust/sim/units.h"

class FSFile;
struct EEPUnitSwap;   // ee/EEPhysics.h

// a new control over the host (EEPlayer::Reset); ee_units_atc_delete releases its planes and airports
EeUnitsAtc* EEATCNew();
// the saved game's sections and EEPlayer::UnitsSwapped's replacement (0x14f88d)
long EEATCLoad(EeUnitsAtc* inATC, FSFile* inFile);
long EEATCSave(EeUnitsAtc* inATC, FSFile* inFile);
void EEATCReplaceUnits(EeUnitsAtc* inATC, std::vector<EEPUnitSwap>& inSwaps);

#endif
