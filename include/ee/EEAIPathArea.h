// C:\EEX\Empire Earth\...\EEAIPathArea.h (RECONSTRUCTION, STUB; file path inferred). A path area
// of the AI's continent map (RTTI EEAIPathArea > EEAIArea @0; ctor 0x18a03, vtable 0x439fac; its id +0x7a =
// EEAIContinentManager::NewPathAreaID()). EEAIPathSector::GetPathAreaAt 0x2bd4d calls the EEAIArea inline Contains
// (0xc6fd) on it; members only where a decompiled body reads them (gaps elsewhere). Requires ee/EEAIArea.h.
#ifndef EE_EEAIPATHAREA_H
#define EE_EEAIPATHAREA_H
#include "rust/sim/astar.h"

// Port: the members are the Rust record EeAStarPathAreaRecord (ee_sim::astar area_kinds.rs), the private base:
// +0x68 the sector ids, +0x78 the continent, +0x7a the id (NewPathAreaID), +0x7c the kind (EEAIGetTileKind), +0x80
// the obstacle's unit (-1), +0x84..+0x8a the region ids by kind (GetContinent 0x22b86: kind 4, 1, 2, 3)
class EEAIPathArea : public EEAIArea, private EeAStarPathAreaRecord {
public:
    // a Rust object (ee_sim::astar objects.rs): made and freed there, never by a C++ constructor
    static EEAIPathArea* New() { return (EEAIPathArea*)(uintptr_t)ee_astar_object_new(EeAStarCmTable::PathAreas); }
    static void Delete(EEAIPathArea* inObject) { ee_astar_object_delete(EeAStarCmTable::PathAreas, (uint64_t)(uintptr_t)inObject); }

    EeAStarPathAreaRecord* Record() { return this; }
    const EeAStarPathAreaRecord* Record() const { return this; }
    unsigned short GetContinentID() const { return this->continent_id; }
    void SetContinentID(unsigned short inID) { this->continent_id = inID; }
    unsigned short GetID() const { return this->id; }
    void SetID(unsigned short inID) { this->id = inID; }
    long GetKind() const { return this->kind; }
    void SetKind(long inKind) { this->kind = inKind; }
    long GetUnitID() const { return this->unit_id; }

private:
    EEAIPathArea();
    ~EEAIPathArea();
    EEAIPathArea(const EEAIPathArea&);
    EEAIPathArea& operator=(const EEAIPathArea&);
};

#endif
