// C:\EEX\Empire Earth\...\EEAIContinent.h (RECONSTRUCTION; file path inferred). One continent
// of the AI's continent map (RTTI EEAIContinent > EEAIArea @0, AStarGraphableObjectBase @0x68; vtables 0x43a1c8
// (EEAIArea part, 14 slots) and 0x43a1c0 (graph part: ??_G thunk 0x1e602); sizeof 0x120). Ctor 0x1d846 (new site
// 0x156b2), dtor 0x1d94d (??_G 0x1d931 calls ??1). Owned by EEAIContinentManager::mContinents (index = mID).
// Slot signatures from the bodies (ret N, argument reads). Slot and member names are provisional. Ctor 0x1d846 (init
// list mCategory(false), mID(0) + the mCounts loop) and dtor 0x1d94d (empty body) prove the layout and the MI
// (bind_only 100).
// Requires ee/EEAIArea.h's engine headers and ee/EEAreaUtilities.h (EETileCoordinate).
#ifndef EE_EEAICONTINENT_H
#define EE_EEAICONTINENT_H

#include <vector>

#include "ee/EEAIArea.h"
#include "rust/sim/records.h"

class EEAIPathArea;

class EEAIContinent : public EEAIArea, private EeAStarContinentRecord {
public:
    // a Rust object (ee_sim::astar objects.rs): made and freed there, never by a C++ constructor
    static EEAIContinent* New() { return (EEAIContinent*)(uintptr_t)ee_astar_object_new(EeAStarCmTable::Continents); }
    static void Delete(EEAIContinent* inObject) { ee_astar_object_delete(EeAStarCmTable::Continents, (uint64_t)(uintptr_t)inObject); }
    // +0x2c 0x1dc54 (ret 8): reserves outTiles to mShoreTiles's size, then walks mShoreTiles (inID 0: every tile)
    void GetShoreNextTo(unsigned short inID, std::vector<U2DPoint<long> >& outTiles)
        { ee_astar_continent_shore_next_to((uint64_t)(uintptr_t)this, inID, &outTiles, EEAIPushPoint); }
    // +0x30 0x1e45e (ret 0xc): Virtual34 with the one-element id list { inID }
    void GetShoreInRegion(unsigned short inID, std::vector<U2DPoint<long> >& outTiles, long inKind)
        { this->GetShoreInRegions(std::vector<unsigned short>(1, inID), outTiles, inKind); }
    // +0x34 0x1dccf (ret 0xc): outTiles is cleared first (0x288c2c); walks inIDs from the back
    // with inIDs empty or holding a 0, every tile; else the tiles whose path area's id (+0x8a for inArg3 3,
    // else +0x86) is listed, or, without one, a kind-1 continent's tiles next to a listed continent
    void GetShoreInRegions(const std::vector<unsigned short>& inIDs, std::vector<U2DPoint<long> >& outTiles, long inKind)
        { ee_astar_continent_shore_in_regions_into((uint64_t)(uintptr_t)this, inIDs.data(), inIDs.size(), inKind, &outTiles, EEAIClearList<std::vector<U2DPoint<long> > >, EEAIPushPoint); }

    unsigned char GetCategory() const { return this->category; }  // inline (GetRandomContinent 0x422dc8) (provisional name)
    unsigned short GetID() const { return this->id; }
    EeAStarContinentRecord* Record() { return this; }   // the record (ee_sim::astar cm_io.rs's layout)

private:
    EEAIContinent();
    ~EEAIContinent();
    // +0x6c..+0x11c: the shore tiles, the choke point indices (EEAIContinentManager::mChokePoints), the forest, path
    // area and hill ids, the four region id lists (from the path-area unit's 0x17a4b), +0xfc the adjacent continents
    // (the chain machine's graph edges), mCounts (ctor body `for (i = 6; i >= 0; i--) mCounts[i] = 0;`, proven),
    // mCategory (byte; the factory copies its +0x50 byte) and mID (+0x11c, the mContinents index):
    // EeAStarContinentRecord
};

#endif
