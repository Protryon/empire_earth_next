// C:\EEX\Empire Earth\...\EEAIChokePoint.h (RECONSTRUCTION; file path inferred). An AI map area
// (RTTI EEAIChokePoint > EEAIArea @0; vtable 0x439d3c, 11 slots). Only the base is declared: EECPILocation::Update 0x80a57 reads the
// EEAIArea serial of what EEAIContinentManager's lookup returns. Requires ee/EEAIArea.h.
#ifndef EE_EEAICHOKEPOINT_H
#define EE_EEAICHOKEPOINT_H
#include <stdint.h>
#include "ee/EEAIArea.h"

// Port: the members are the Rust record EeAStarChokeRecord (ee_sim::astar area_kinds.rs), the private base: +0x68 the
// serials of the areas it joins (EEAIContinentManager 0x24b31), +0x78 its mChokePoints index (also
// EEAIContinent::AddChokePoint's argument), +0x7c its continent
class EEAIChokePoint : public EEAIArea, private EeAStarChokeRecord {
public:
    // a Rust object (ee_sim::astar objects.rs): made and freed there, never by a C++ constructor
    EeAStarChokeRecord* Record() { return this; }
    const EeAStarChokeRecord* Record() const { return this; }

private:
    EEAIChokePoint();
    ~EEAIChokePoint();
    EEAIChokePoint(const EEAIChokePoint&);
    EEAIChokePoint& operator=(const EEAIChokePoint&);
};

// (the terrain pass's per-tile grids, .bss 0x4e5c44..0x4e5c54 in the original, are Rust's: ee_sim::astar choke.rs)

#endif
