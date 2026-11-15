// C:\EEX\Empire Earth\...\EEAIPathSector.h (RECONSTRUCTION; file path inferred). A path sector of
// the AI's continent map (RTTI EEAIPathSector, no bases; vtable 0x43a52c, one slot ??_G 0x2bc2e; sizeof 0x4c: the
// manager's `new` 0x4c). EEAIPathSector.cpp 0x2bbd3..0x2be88. Ctor 0x2bbd3 (init list: the members' ctors, then the
// zero stores, then the vptr), dtor 0x2bc4a (DeleteSubSectors, then the members' dtors). Member and method names
// are provisional. Requires <map>, <vector>, lle/U2DPoint.h, lle/U2DRectangle.h.
#ifndef EE_EEAIPATHSECTOR_H
#define EE_EEAIPATHSECTOR_H

#include <map>
#include <vector>

class EEAIPathArea;
class EEAIPathSubSector;

class EEAIContinent;

class EEAIPathSector {
public:
    EEAIPathSector();                       // 0x2bbd3
    virtual ~EEAIPathSector();              // +0x00 dtor 0x2bc4a (??_G 0x2bc2e)
    void AddNeighbor(EEAIContinent* inContinent);   // 0x2bc72: push_back unless present (std::find; the manager's 0x20b2e)
    void AddPathAreaID(unsigned short inID);   // 0x2bccf: unless present (binary_search), push_back + sort
    EEAIPathArea* GetPathAreaAt(const U2DPoint<long>& inPoint);   // 0x2bd4d: the last listed path area holding it
    void DeleteSubSectors();                // 0x2bda4: deletes every sub-sector, then clears the map
    void RemovePathAreaID(unsigned short inID);   // 0x2bde6: lower_bound + erase when equal

    std::map<unsigned short, EEAIPathSubSector*> mSubSectors;   // +0x04 (the dtor's _Tree dtor 0x1b6d9 names it)
    std::vector<EEAIContinent*> mNeighbors; // +0x10 the continents touching the sector
    std::vector<unsigned short> mPathAreaIDs;  // +0x20 sorted (EEAIContinentManager::mPathAreas ids)
    U2DRectangle<long> mBounds;             // +0x30 (the ctor's zero stores +0x3c, +0x34, +0x30, +0x38: U2DRectangle())
    U2DPoint<long> mCenter;                 // +0x40 (copied as one point: 0x41a8c2, StartGraphPath 0x4a69e2)
    unsigned short mID;                     // +0x48 (EEAIContinentManager::RemoveSectorTiles erases it)
};

#endif
