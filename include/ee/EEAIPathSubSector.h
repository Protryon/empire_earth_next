// C:\EEX\Empire Earth\...\EEAIPathSubSector.h (RECONSTRUCTION; file path inferred). A path
// sub-sector of the AI's continent map (RTTI EEAIPathSubSector > AStarGraphableObjectBase @0; vtable 0x43a538, one
// slot ??_G 0x2beec). EEAIContinentManager::GetPathSubSectorAt 0x273c9 returns it. Only the members a decompiled unit
// calls are declared; layout from the ctor 0x2be88 (0x1a99b walks the pointer vector at +0x30/+0x34).
// Requires lle/U2DPoint.h, lle/U2DSparseArray.h.
#ifndef EE_EEAIPATHSUBSECTOR_H
#define EE_EEAIPATHSUBSECTOR_H

#include <map>
#include <stdint.h>
#include <vector>
#include "ee/EEAIListView.h"
#include "rust/sim/astar.h"         // EeAStarSubSectorRecord
#include "ee/EECPASGFlags.h"

class EEAIPathSector;
class EEComplexUnit;
struct EECPASGFlags;

// Port: every member but the tiles is the Rust record EeAStarSubSectorRecord (ee_sim::astar sector_rec.rs), the
// private base: +0x04 the links by the other's id (node 0x1c: word key, two pointers; GetLinks/RemoveLinks take
// upper_bound, then lower_bound), +0x2c the neighbours, +0x3c the location, +0x44 the sector (StartGraphPath reads
// its +0x40/+0x44), +0x48 the tile count, +0x4c the continent, +0x4e the path area, +0x50 the id (the manager's
// 0x24dcf indexes +0x5c by it), +0x54 the kind (0x10465 compares it with 0xd), +0x58 the player, +0x5c stale
class EEAIPathSubSector : private EeAStarSubSectorRecord {
public:
    // a Rust object (ee_sim::astar objects.rs): made and freed there, never by a C++ constructor

    EeAStarSubSectorRecord* Record() { return this; }
    const EeAStarSubSectorRecord* Record() const { return this; }
    const U2DPoint<long>& GetLocation() const { return reinterpret_cast<const U2DPoint<long>&>(this->location); }
    void SetLocation(const U2DPoint<long>& inLocation) { reinterpret_cast<U2DPoint<long>&>(this->location) = inLocation; }

    U2DSparseArrayPointContainer mTiles;    // +0x10 (0x1c)

private:
    EEAIPathSubSector();
    ~EEAIPathSubSector();
    EEAIPathSubSector(const EEAIPathSubSector&);
    EEAIPathSubSector& operator=(const EEAIPathSubSector&);
};

#endif
