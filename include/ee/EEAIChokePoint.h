// C:\EEX\Empire Earth\...\EEAIChokePoint.h (RECONSTRUCTION; file path inferred). An AI map area
// (RTTI EEAIChokePoint > EEAIArea @0; vtable 0x439d3c, 11 slots). Only the base is declared: EECPILocation::Update 0x80a57 reads the
// EEAIArea serial of what EEAIContinentManager's lookup returns. Requires ee/EEAIArea.h.
#ifndef EE_EEAICHOKEPOINT_H
#define EE_EEAICHOKEPOINT_H
#include <stdint.h>
#include "rust/sim/astar.h"

// Port: the members are the Rust record EeAStarChokeRecord (ee_sim::astar area_kinds.rs), the private base: +0x68 the
// serials of the areas it joins (EEAIContinentManager 0x24b31), +0x78 its mChokePoints index (also
// EEAIContinent::AddChokePoint's argument), +0x7c its continent
class EEAIChokePoint : public EEAIArea, private EeAStarChokeRecord {
public:
    // a Rust object (ee_sim::astar objects.rs): made and freed there, never by a C++ constructor
    static EEAIChokePoint* New() { return (EEAIChokePoint*)(uintptr_t)ee_astar_object_new(EeAStarCmTable::ChokePoints); }
    static void Delete(EEAIChokePoint* inObject) { ee_astar_object_delete(EeAStarCmTable::ChokePoints, (uint64_t)(uintptr_t)inObject); }
    // 0x1c75a: appended to the area serials
    void AddAreaSerials(const std::vector<unsigned long>& inSerials)
        { ee_astar_choke_add_area_serials(this, inSerials.empty() ? NULL : &inSerials[0], inSerials.size(), false); }
    EeAStarChokeRecord* Record() { return this; }
    const EeAStarChokeRecord* Record() const { return this; }
    EEAIListView<unsigned long> GetAreaSerials() const
    {
        size_t theCount;
        const unsigned long* theSerials = ee_astar_choke_area_serials(this, &theCount);
        return EEAIListView<unsigned long>(theSerials, theCount);
    }
    long GetIndex() const { return this->index; }
    void SetIndex(long inIndex) { this->index = inIndex; }
    unsigned short GetContinentID() const { return this->continent_id; }
    void SetContinentID(unsigned short inID) { this->continent_id = inID; }
    // the members in a saved game (EEAIContinentSave.cpp's writer and reader), in this order
    template <class W> void WriteFields(W& ioWriter) const
    {
        ioWriter.ULongs(std::vector<unsigned long>(this->GetAreaSerials()));
        ioWriter.I32(this->index);
        ioWriter.U16(this->continent_id);
    }
    template <class R> bool ReadFields(R& ioReader)
    {
        std::vector<unsigned long> theSerials;
        if (!ioReader.ULongs(theSerials))
            return false;
        ee_astar_choke_add_area_serials(this, theSerials.empty() ? NULL : &theSerials[0], theSerials.size(), true);
        return ioReader.I32(this->index) && ioReader.U16(this->continent_id);
    }

private:
    EEAIChokePoint();
    ~EEAIChokePoint();
    EEAIChokePoint(const EEAIChokePoint&);
    EEAIChokePoint& operator=(const EEAIChokePoint&);
};

// (the terrain pass's per-tile grids, .bss 0x4e5c44..0x4e5c54 in the original, are Rust's: ee_sim::astar choke.rs)

#endif
