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

#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIListView.h"

class EEAIPathArea;

class EEAIContinent : public EEAIArea, public AStarGraphableObjectBase, private EeAStarContinentRecord {
public:
    EEAIContinent();
    virtual ~EEAIContinent();               // +0x00 dtor 0x1d94d
    // +0x1c 0x1da61: the EEAIArea slot (closest points)
    virtual bool GetClosestPoints(EEAIArea* inOther, U2DPoint<long>& outThis, U2DPoint<long>& outOther);
    // +0x2c 0x1dc54 (ret 8): reserves outTiles to mShoreTiles's size, then walks mShoreTiles (inID 0: every tile)
    virtual void GetShoreNextTo(unsigned short inID, std::vector<U2DPoint<long> >& outTiles);
    // +0x30 0x1e45e (ret 0xc): Virtual34 with the one-element id list { inID }
    virtual void GetShoreInRegion(unsigned short inID, std::vector<U2DPoint<long> >& outTiles, long inKind);
    // +0x34 0x1dccf (ret 0xc): outTiles is cleared first (0x288c2c); walks inIDs from the back
    // with inIDs empty or holding a 0, every tile; else the tiles whose path area's id (+0x8a for inArg3 3,
    // else +0x86) is listed, or, without one, a kind-1 continent's tiles next to a listed continent
    virtual void GetShoreInRegions(const std::vector<unsigned short>& inIDs, std::vector<U2DPoint<long> >& outTiles,
                           long inKind);

    // 0x1d9e0: unless present (0x1e5bd), push_back + sort (0x1e6ac) (Rust: ee_astar continent.rs, as the rest)
    void AddPathAreaID(unsigned short inID) { ee_astar_continent_add_id(this, EeAStarContinentList::PathAreas, inID); }
    EEAIPathArea* GetPathAreaAt(const U2DPoint<long>& inPoint);   // 0x1dbf4: as EEAIPathSector's
    // 0x1e4a1: lower_bound + erase when equal
    void RemovePathAreaID(unsigned short inID) { ee_astar_continent_remove_id(this, EeAStarContinentList::PathAreas, inID); }
    // 0x26d84: push_back + sort (EEAIContinentManager::AddChokePoint)
    void AddChokePoint(long inIndex) { ee_astar_continent_add_choke_point(this, inIndex); }
    // 0x26dca: push_back + sort (0x208cc, the forest's +0x82)
    void AddForestID(unsigned short inID) { ee_astar_continent_add_id(this, EeAStarContinentList::Forests, inID); }
    // 0x26e13: push_back + sort (0x2033f, the hill's +0x6a)
    void AddHillID(unsigned short inID) { ee_astar_continent_add_id(this, EeAStarContinentList::Hills, inID); }
    // 0x26ea4 (EEAIContinentManager 0x25325) (provisional name)
    void RemoveForestID(unsigned short inID) { ee_astar_continent_remove_id(this, EeAStarContinentList::Forests, inID); }
    // inline (COMDATs after their first user EEAIAStarPathAreaGoal::AssignKindIDs 0x17a4b; bodies in
    // EEAIAStarPathAreaGoal.cpp): push_back unless std::find finds it (provisional names)
    void AddIDBC(unsigned short inID) { ee_astar_continent_add_id(this, EeAStarContinentList::IdsBc, inID); }
    void AddIDCC(unsigned short inID) { ee_astar_continent_add_id(this, EeAStarContinentList::IdsCc, inID); }
    void AddIDDC(unsigned short inID) { ee_astar_continent_add_id(this, EeAStarContinentList::IdsDc, inID); }
    void AddIDEC(unsigned short inID) { ee_astar_continent_add_id(this, EeAStarContinentList::IdsEc, inID); }
    // inline (COMDAT 0x26e7b in EEAIContinentManager.cpp's range): inContinent is in mNeighbors
    bool HasNeighbor(EEAIContinent* inContinent) const
    {
        EEAIHandleView<EEAIContinent> theNeighbors = this->GetNeighbors();
        for (size_t i = 0; i < theNeighbors.size(); i++)
            if (theNeighbors[i] == inContinent)
                return true;
        return false;
    }
    // the graph machine's view (EEAIAStarGraphMachine<.., EEAIContinent>): out-of-line COMDAT 0x275eb
    // (`jbe`: unsigned) and the hull's center as the node tile (0x28aa7 reads +0x44/+0x48) (provisional names)
    unsigned long GetNumNeighbors() const { return (unsigned long)this->GetNeighbors().size(); }
    const U2DPoint<long>& GetGraphLocation() const { return this->GetHullCenter(); }

    // New (port): the members behind accessors (R5b.1 blocker (b)); the lists, counts, category and id are the
    // record's (EeAStarContinentRecord, ee_astar continent.rs), the neighbours stay here
    EEAIListView<U2DPoint<long> > GetShoreTiles() const
    {
        size_t theCount;
        const EeHullPoint* theTiles = ee_astar_continent_shore(this, &theCount);
        return EEAIListView<U2DPoint<long> >(reinterpret_cast<const U2DPoint<long>*>(theTiles), theCount);
    }
    void AddShoreTile(const U2DPoint<long>& inTile) { ee_astar_continent_add_shore_tile(this, U2DHullPointOf(inTile)); }
    void ClearShoreTiles() { ee_astar_continent_clear_shore(this); }
    EEAIListView<long> GetChokePoints() const
    {
        size_t theCount;
        const long* theIndices = ee_astar_continent_choke_points(this, &theCount);
        return EEAIListView<long>(theIndices, theCount);
    }
    long GetChokePointsAt(unsigned long inIndex) const { return this->GetChokePoints()[inIndex]; }
    EEAIListView<unsigned short> GetIDs(EeAStarContinentList inList) const
    {
        size_t theCount;
        const uint16_t* theIDs = ee_astar_continent_ids(this, inList, &theCount);
        return EEAIListView<unsigned short>(theIDs, theCount);
    }
    EEAIListView<unsigned short> GetForestIDs() const { return this->GetIDs(EeAStarContinentList::Forests); }
    EEAIListView<unsigned short> GetPathAreaIDs() const { return this->GetIDs(EeAStarContinentList::PathAreas); }
    EEAIListView<unsigned short> GetHillIDs() const { return this->GetIDs(EeAStarContinentList::Hills); }
    EEAIListView<unsigned short> GetIDsBC() const { return this->GetIDs(EeAStarContinentList::IdsBc); }
    unsigned short GetIDsBCAt(unsigned long inIndex) const { return this->GetIDsBC()[inIndex]; }
    EEAIListView<unsigned short> GetIDsCC() const { return this->GetIDs(EeAStarContinentList::IdsCc); }
    EEAIListView<unsigned short> GetIDsDC() const { return this->GetIDs(EeAStarContinentList::IdsDc); }
    unsigned short GetIDsDCAt(unsigned long inIndex) const { return this->GetIDsDC()[inIndex]; }
    EEAIListView<unsigned short> GetIDsEC() const { return this->GetIDs(EeAStarContinentList::IdsEc); }
    EEAIHandleView<EEAIContinent> GetNeighbors() const
    {
        size_t theCount;
        const uint64_t* theNeighbors = ee_astar_continent_neighbors(this, &theCount);
        return EEAIHandleView<EEAIContinent>(theNeighbors, theCount);
    }
    void AddNeighbor(EEAIContinent* inContinent) { ee_astar_continent_add_neighbor(this, (uint64_t)(uintptr_t)inContinent); }
    void ClearNeighbors() { ee_astar_continent_clear_neighbors(this); }
    unsigned short GetCount(long inKind) const { return this->counts[inKind]; }
    void SetCount(long inKind, unsigned short inCount) { this->counts[inKind] = inCount; }
    unsigned char GetCategory() const { return this->category; }  // inline (GetRandomContinent 0x422dc8) (provisional name)
    void SetCategory(unsigned char inCategory) { this->category = inCategory; }
    unsigned short GetID() const { return this->id; }
    EeAStarContinentRecord* Record() { return this; }   // the record (ee_sim::astar cm_io.rs's layout)
    void SetID(unsigned short inID) { this->id = inID; }
    // the lists in a saved game (EEAIContinentSave.cpp's writer and reader: Points, Longs, Shorts), in this order
    template <class W> void WriteLists(W& ioWriter) const
    {
        ioWriter.Points(std::vector<U2DPoint<long> >(this->GetShoreTiles()));
        ioWriter.Longs(std::vector<long>(this->GetChokePoints()));
        for (int i = 0; i < 7; i++)
            ioWriter.Shorts(std::vector<unsigned short>(this->GetIDs((EeAStarContinentList)i)));
    }
    template <class R> bool ReadLists(R& ioReader)
    {
        std::vector<U2DPoint<long> > theShore;
        std::vector<long> theChokePoints;
        std::vector<unsigned short> theIDs[7];
        if (!ioReader.Points(theShore) || !ioReader.Longs(theChokePoints))
            return false;
        const uint16_t* theLists[7];
        size_t theCounts[7];
        for (int i = 0; i < 7; i++) {
            if (!ioReader.Shorts(theIDs[i]))
                return false;
            theLists[i] = theIDs[i].empty() ? NULL : &theIDs[i][0];
            theCounts[i] = theIDs[i].size();
        }
        ee_astar_continent_set_lists(this, U2DHullPoints(theShore), theShore.size(),
                                     theChokePoints.empty() ? NULL : &theChokePoints[0], theChokePoints.size(), theLists,
                                     theCounts);
        return true;
    }

private:
    // +0x6c..+0x11c: the shore tiles, the choke point indices (EEAIContinentManager::mChokePoints), the forest, path
    // area and hill ids, the four region id lists (from the path-area unit's 0x17a4b), +0xfc the adjacent continents
    // (the chain machine's graph edges), mCounts (ctor body `for (i = 6; i >= 0; i--) mCounts[i] = 0;`, proven),
    // mCategory (byte; the factory copies its +0x50 byte) and mID (+0x11c, the mContinents index):
    // EeAStarContinentRecord
};

#endif
