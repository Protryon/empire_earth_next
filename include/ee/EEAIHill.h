// C:\EEX\Empire Earth\...\EEAIHill.h (RECONSTRUCTION; file path inferred). An AI map area
// (RTTI EEAIHill > EEAIArea @0; vtable 0x439f18, 11 slots). Only the base is declared: EECPILocation::Update 0x80a57 reads the
// EEAIArea serial of what EEAIContinentManager's lookup returns. Requires ee/EEAIArea.h.
#ifndef EE_EEAIHILL_H
#define EE_EEAIHILL_H
#include "ee/EEAIArea.h"
#include "rust/sim/records.h"

// Port: the members are the Rust record EeAStarHillRecord (ee_sim::astar area_kinds.rs), the private base: +0x68
// its continent (the hill goal's +0x56), +0x6a its id (EEAIContinent::AddHillID 0x26e13 reads it)
class EEAIHill : public EEAIArea, private EeAStarHillRecord {
public:
    // a Rust object (ee_sim::astar objects.rs): made and freed there, never by a C++ constructor
    EeAStarHillRecord* Record() { return this; }
    const EeAStarHillRecord* Record() const { return this; }

private:
    EEAIHill();
    ~EEAIHill();
};

#endif
