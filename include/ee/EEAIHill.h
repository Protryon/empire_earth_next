// C:\EEX\Empire Earth\...\EEAIHill.h (RECONSTRUCTION; file path inferred). An AI map area
// (RTTI EEAIHill > EEAIArea @0; vtable 0x439f18, 11 slots). Only the base is declared: EECPILocation::Update 0x80a57 reads the
// EEAIArea serial of what EEAIContinentManager's lookup returns. Requires ee/EEAIArea.h.
#ifndef EE_EEAIHILL_H
#define EE_EEAIHILL_H
#include "rust/sim/astar.h"

// Port: the members are the Rust record EeAStarHillRecord (ee_sim::astar area_kinds.rs), the private base: +0x68
// its continent (the hill goal's +0x56), +0x6a its id (EEAIContinent::AddHillID 0x26e13 reads it)
class EEAIHill : public EEAIArea, private EeAStarHillRecord {
public:
    // inline (EEAIAStarHillGoal::CreateHill 0x16d04 expands it: EEAIArea(), +0x68/+0x6a = 0, the vptr)
    EEAIHill() : EeAStarHillRecord() {}
    virtual ~EEAIHill() {}
    EeAStarHillRecord* Record() { return this; }
    const EeAStarHillRecord* Record() const { return this; }
    unsigned short GetContinentID() const { return this->continent_id; }
    void SetContinentID(unsigned short inID) { this->continent_id = inID; }
    unsigned short GetID() const { return this->id; }
    void SetID(unsigned short inID) { this->id = inID; }
    // the members in a saved game (EEAIContinentSave.cpp's writer and reader), in this order
    template <class W> void WriteFields(W& ioWriter) const { ioWriter.U16(this->continent_id); ioWriter.U16(this->id); }
    template <class R> bool ReadFields(R& ioReader) { return ioReader.U16(this->continent_id) && ioReader.U16(this->id); }
};

#endif
