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
#include "ee/EEAIListView.h"
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
    static EEAIPathSector* New() { return (EEAIPathSector*)(uintptr_t)ee_astar_object_new(EeAStarCmTable::PathSectors); }
    static void Delete(EEAIPathSector* inObject) { ee_astar_object_delete(EeAStarCmTable::PathSectors, (uint64_t)(uintptr_t)inObject); }
    // (dtor 0x2bc4a: DeleteSubSectors, then the record: objects.rs's delete)
    // 0x2bc72: push_back unless present (std::find; the manager's 0x20b2e)
    void AddNeighbor(EEAIContinent* inContinent) { ee_astar_sector_add_neighbor(this, (uint64_t)(uintptr_t)inContinent, true); }
    // 0x2bccf: unless present (binary_search), push_back + sort
    void AddPathAreaID(unsigned short inID) { ee_astar_sector_add_path_area_id(this, inID); }
    // 0x2bd4d: the last listed path area holding it (Rust: cm_query.rs)
    EEAIPathArea* GetPathAreaAt(const U2DPoint<long>& inPoint)
        { return (EEAIPathArea*)(uintptr_t)ee_astar_object_path_area_at((uint64_t)(uintptr_t)this, true, inPoint.mX, inPoint.mY); }
    // 0x2bde6: lower_bound + erase when equal
    void RemovePathAreaID(unsigned short inID) { ee_astar_sector_remove_path_area_id(this, inID); }

    EeAStarSectorRecord* Record() { return this; }
    const EeAStarSectorRecord* Record() const { return this; }
    EEAIHandleView<EEAIContinent> GetNeighbors() const
        { return EEAIHandleView<EEAIContinent>(ee_astar_sector_neighbors, static_cast<const EeAStarSectorRecord*>(this)); }
    EEAIListView<unsigned short> GetPathAreaIDs() const
        { return EEAIListView<unsigned short>(ee_astar_sector_path_area_ids, static_cast<const EeAStarSectorRecord*>(this)); }
    const U2DRectangle<long>& GetBounds() const { return reinterpret_cast<const U2DRectangle<long>&>(this->bounds); }
    void SetBounds(const U2DRectangle<long>& inBounds) { reinterpret_cast<U2DRectangle<long>&>(this->bounds) = inBounds; }
    const U2DPoint<long>& GetCenter() const { return reinterpret_cast<const U2DPoint<long>&>(this->center); }
    void SetCenter(const U2DPoint<long>& inCenter) { reinterpret_cast<U2DPoint<long>&>(this->center) = inCenter; }
    unsigned short GetID() const { return this->id; }
    void SetID(unsigned short inID) { this->id = inID; }

private:
    EEAIPathSector();
    ~EEAIPathSector();
    EEAIPathSector(const EEAIPathSector&);
    EEAIPathSector& operator=(const EEAIPathSector&);
};

#endif
