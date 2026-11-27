// C:\EEX\Empire Earth\...\EEAIPathSector.h (RECONSTRUCTION; file path inferred). A path sector of
// the AI's continent map (RTTI EEAIPathSector, no bases; vtable 0x43a52c, one slot ??_G 0x2bc2e; sizeof 0x4c: the
// manager's `new` 0x4c). EEAIPathSector.cpp 0x2bbd3..0x2be88. Ctor 0x2bbd3 (init list: the members' ctors, then the
// zero stores, then the vptr), dtor 0x2bc4a (DeleteSubSectors, then the members' dtors). Member and method names
// are provisional. Requires <map>, <vector>, lle/U2DPoint.h, lle/U2DRectangle.h.
#ifndef EE_EEAIPATHSECTOR_H
#define EE_EEAIPATHSECTOR_H

#include <map>
#include <stdint.h>
#include <vector>
#include "rust/sim/astar.h"         // EeAStarSectorRecord

class EEAIPathArea;
class EEAIPathSubSector;

class EEAIContinent;

// Port: the members are the Rust record EeAStarSectorRecord (ee_sim::astar sector_rec.rs), the private base: +0x04
// the sub sectors by id (the dtor's _Tree dtor 0x1b6d9), +0x10 the continents touching it, +0x20 the sorted path
// area ids, +0x30 the bounds, +0x40 the center (StartGraphPath 0x4a69e2), +0x48 the id (RemoveSectorTiles erases it)
class EEAIPathSector : private EeAStarSectorRecord {
public:
    // a Rust object (ee_sim::astar objects.rs): made and freed there, never by a C++ constructor
    // (dtor 0x2bc4a: DeleteSubSectors, then the record: objects.rs's delete)

    EeAStarSectorRecord* Record() { return this; }
    const EeAStarSectorRecord* Record() const { return this; }

private:
    EEAIPathSector();
    ~EEAIPathSector();
    EEAIPathSector(const EEAIPathSector&);
    EEAIPathSector& operator=(const EEAIPathSector&);
};

#endif
