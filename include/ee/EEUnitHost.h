// New (port): the C++ the units' Rust calls (src/Empire Earth/Units/EEUnitHost.cpp). The world calls a Rust decision
// makes (ee_units_host_*) are declared by the generated rust/sim/units.h (rust/crates/ee_sim/src/units/host.rs).
#ifndef EE_EEUNITHOST_H
#define EE_EEUNITHOST_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

class EEComplexUnit;

// the farm sorts of EEMBuildUnit (EETileDistanceLess) and EEMSmartFarmers (EEFarmDistanceGreater: inGreater) around
// the tile (inX, inY): the bodies' std::sorts, in EEMessage.cpp with their comparators
void EESortFarms(std::vector<EEComplexUnit*>& ioFarms, long inX, long inY, bool inGreater);

// TEST ONLY: the group command sweep (ee_sim units/group_sweep.rs, called from EEUnitHost.cpp; USGC records; EE-SIM, every five game minutes)
void EEGroupCommandSweep();

#endif
