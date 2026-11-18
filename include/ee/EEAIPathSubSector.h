// C:\EEX\Empire Earth\...\EEAIPathSubSector.h (RECONSTRUCTION; file path inferred). A path
// sub-sector of the AI's continent map (RTTI EEAIPathSubSector > AStarGraphableObjectBase @0; vtable 0x43a538, one
// slot ??_G 0x2beec). EEAIContinentManager::GetPathSubSectorAt 0x273c9 returns it. Only the members a decompiled unit
// calls are declared; layout from the ctor 0x2be88 (0x1a99b walks the pointer vector at +0x30/+0x34).
// Requires ee/AStarGraphableObjectBase.h, lle/U2DPoint.h, lle/U2DSparseArray.h.
#ifndef EE_EEAIPATHSUBSECTOR_H
#define EE_EEAIPATHSUBSECTOR_H

#include <map>
#include <stdint.h>
#include <vector>
#include "ee/EEAIListView.h"
#include "rust/sim/astar.h"         // EeAStarSubSectorRecord

class EEAIPathSector;
class EEComplexUnit;
struct EECPASGFlags;

// Port: every member but the tiles is the Rust record EeAStarSubSectorRecord (ee_sim::astar sector_rec.rs), the
// private base: +0x04 the links by the other's id (node 0x1c: word key, two pointers; GetLinks/RemoveLinks take
// upper_bound, then lower_bound), +0x2c the neighbours, +0x3c the location, +0x44 the sector (StartGraphPath reads
// its +0x40/+0x44), +0x48 the tile count, +0x4c the continent, +0x4e the path area, +0x50 the id (the manager's
// 0x24dcf indexes +0x5c by it), +0x54 the kind (0x10465 compares it with 0xd), +0x58 the player, +0x5c stale
class EEAIPathSubSector : public AStarGraphableObjectBase, private EeAStarSubSectorRecord {
public:
    // (EEAIPathSubSector.cpp 0x2be88..0x2c79f): ctor 0x2be88, dtor 0x2bf08 (??_G 0x2beec)
    EEAIPathSubSector() { ee_astar_sub_sector_init(this); }
    virtual ~EEAIPathSubSector() { ee_astar_sub_sector_free(this); }
    // 0x2c0b9 (ret 8): appends the links listed under inID; always returns false
    bool GetLinks(unsigned short inID, std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> >& outLinks);
    // 0x2c10f (ret 4): whether the gate (unit class category 0x19) on mTiles lets gWorld's player inPlayer through
    // (setrilo GatePasses; a missing or dead gate passes)
    bool IsPassable(long inPlayer);
    // 0x2c219 (ret 8): inOther is a neighbour and none of its links to this sub-sector is open for inIndex (body
    // not written: the links' +0x54 is a pointer there)
    bool AreLinksBlocked(EEAIPathSubSector* inOther, long inIndex);
    // (EECPAttackSubGroupUnit::ChooseTarget 0x63d50 passes the address of an EECPASGFlags its unit class filled)
    bool AreLinksBlocked(EEAIPathSubSector* inOther, const EECPASGFlags& inFlags);
    // 0x2c2c5 (ret 4): drops every link with inOther and inOther from the neighbours; true if it was a neighbour
    bool RemoveNeighbor(EEAIPathSubSector* inOther)
        { return ee_astar_sub_sector_remove_neighbor(this, (uint64_t)(uintptr_t)inOther, inOther->id); }
    // 0x2c379 (ret 4): erases the links listed under inID
    void RemoveLinks(unsigned short inID) { ee_astar_sub_sector_remove_links(this, inID); }
    void CreateTiles(const std::vector<U2DPoint<long> >& inTiles) { this->mTiles.Create(inTiles); }   // 0x2c3bc

    // the graph machine's view (EEAIAStarGraphMachine<.., EEAIPathSubSector>; 0xab6c1 takes the node tile from
    // mSector +0x40/+0x44) (provisional names)
    EEAIHandleView<EEAIPathSubSector> GetNeighbors() const
    {
        size_t theCount;
        const uint64_t* theNeighbors = ee_astar_sub_sector_neighbors(this, &theCount);
        return EEAIHandleView<EEAIPathSubSector>(theNeighbors, theCount);
    }
    unsigned long GetNumNeighbors() const { return (unsigned long)this->GetNeighbors().size(); }
    U2DPoint<long> GetGraphLocation() const;
    // 0x1a99b (ret 4): whether inOther is a neighbour (inline; its COMDAT follows EEAIAStarPathGraphGoal::IsGoalNode)
    bool IsNeighbor(EEAIPathSubSector* inOther) const
    {
        EEAIHandleView<EEAIPathSubSector> theNeighbors = this->GetNeighbors();
        for (size_t i = 0; i < theNeighbors.size(); i++)
            if (theNeighbors[i] == inOther)
                return true;
        return false;
    }
    void AddNeighbor(EEAIPathSubSector* inOther) { ee_astar_sub_sector_add_neighbor(this, (uint64_t)(uintptr_t)inOther); }
    // inline (COMDAT 0x26430 in EEAIContinentManager.cpp's range): links listed under inID
    bool HasLinks(unsigned short inID) const
    {
        size_t theCount;
        ee_astar_sub_sector_links_of(this, inID, &theCount);
        return theCount != 0;
    }
    void AddLink(unsigned short inID, const std::pair<EEAIPathSubSector*, EEAIPathSubSector*>& inLink)
        { ee_astar_sub_sector_insert_link(this, inID, (uint64_t)(uintptr_t)inLink.first, (uint64_t)(uintptr_t)inLink.second); }
    EEAIListView<EeAStarSubSectorLink> GetAllLinks() const
    {
        size_t theCount;
        const EeAStarSubSectorLink* theLinks = ee_astar_sub_sector_links_all(this, &theCount);
        return EEAIListView<EeAStarSubSectorLink>(theLinks, theCount);
    }
    // inline (COMDAT 0x2640c, same range): the location = the tiles' center (provisional name)
    void UpdateLocation() { this->SetLocation(this->mTiles.ComputeCenterPoint(true)); }

    EeAStarSubSectorRecord* Record() { return this; }
    const EeAStarSubSectorRecord* Record() const { return this; }
    const U2DPoint<long>& GetLocation() const { return reinterpret_cast<const U2DPoint<long>&>(this->location); }
    void SetLocation(const U2DPoint<long>& inLocation) { reinterpret_cast<U2DPoint<long>&>(this->location) = inLocation; }
    EEAIPathSector* GetSector() const { return (EEAIPathSector*)(uintptr_t)this->sector; }
    void SetSector(EEAIPathSector* inSector) { this->sector = (uint64_t)(uintptr_t)inSector; }
    long GetNumTiles() const { return this->num_tiles; }
    void SetNumTiles(long inCount) { this->num_tiles = inCount; }
    unsigned short GetContinentID() const { return this->continent_id; }
    void SetContinentID(unsigned short inID) { this->continent_id = inID; }
    unsigned short GetPathAreaID() const { return this->path_area_id; }
    void SetPathAreaID(unsigned short inID) { this->path_area_id = inID; }
    unsigned short GetID() const { return this->id; }
    void SetID(unsigned short inID) { this->id = inID; }
    long GetKind() const { return this->kind; }
    void SetKind(long inKind) { this->kind = inKind; }
    long GetPlayerID() const { return this->player_id; }
    void SetPlayerID(long inID) { this->player_id = inID; }
    bool IsStale() const { return this->is_stale; }
    void SetStale(bool inStale) { this->is_stale = inStale; }

    U2DSparseArrayPointContainer mTiles;    // +0x10 (0x1c)

private:
    EEAIPathSubSector(const EEAIPathSubSector&);
    EEAIPathSubSector& operator=(const EEAIPathSubSector&);
};

#endif
