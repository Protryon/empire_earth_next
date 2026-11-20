// C:\EEX\Empire Earth\...\EEAIContinentManager.h (RECONSTRUCTION; file path inferred). The AI's continent map (RTTI
// EEAIContinentManager, no bases; vtable 0x43a210, one slot ??_G 0x1f103; sizeof 0x150; EEAIContinentManager.cpp around
// 0x1ef6b..0x2a246 per analysis/exe/unitmap.txt). Ctor 0x1ef6b,
// dtor 0x1f11f (calls 0x1ef12 first, then the members' dtors). The game's instance gContinentManager sits at 0x530bf0
// (324 references; gWorld follows at 0x530d40). It owns every AI area: the seven area vectors hold the objects by id
// (DeleteAll-style template helpers 0x2676e..0x2688e call each element's ??_G), mAreas maps every area's serial
// (EEAIArea +0x60) to it. Element classes from the vtable/ctor of the objects stored (ctor stores of their id fields,
// the dynamic_cast in 0x1f3a0). Member and method names are provisional unless a comment names the evidence.
// Proven (verify --symbol): the inlines 0x60ef/0x60c7/0x60b4 bind_only
// 100 (const members; 0x60b4 copies y then x into locals); ctor 0x1ef6b instruction-identical (init list + the
// mNextIDs zero loop; only ICF aliases _Tree_val/vector<long>::_Buy differ), dtor 0x1f11f the same (ICF _Tidy aliases);
// GetContinentByID 0x26fdd, ClearChecksum 0x1f863, AllocateTileValues 0x1f813 exact; GetContinentAt(EEUnit*) 0x22658,
// GetArea 0x26fb3, NewPathAreaID 0x22a70, AddArea 0x26f8d bind_only 100.
// Includes ee/EEAreaUtilities.h itself (EETileCoordinate: the inline 0x60b4 reads it).
#ifndef EE_EEAICONTINENTMANAGER_H
#define EE_EEAICONTINENTMANAGER_H

#include <map>
#include <vector>

#include "ee/EEAreaUtilities.h"
#include "ee/EEAIListView.h"
#include "rust/sim/astar.h"         // EeAStarCmRecord

class EEUnit;
template <class T> class U2DPoint;
typedef U2DPoint<long> EETileLocation;   // ee/EETileLocation.h (the four area lookups)
class EEAIArea;
class EEAIChokePoint;
class EEAIContinent;
class EEAIForest;
class EEAIHill;
class EEAIPathArea;
class EEAIPathSector;
class EEAIPathSubSector;
class EEAIRectangleArea;
class EEMap;
class EEComplexUnit;
class URandom;

class EEAIContinentManager {
public:
    EEAIContinentManager();                 // 0x1ef6b (Empire Earth.cpp's chain)
    virtual ~EEAIContinentManager();        // +0x00 dtor 0x1f11f (??_G 0x1f103)
    // 0x22b62: the continent id of inUnit's tile (GetContinent(its (+0x1c, +0x20), inKind))
    unsigned short GetContinent(EEUnit* inUnit, long inKind)
        { EeHullPoint t = ee_astar_unit_tile((uint64_t)(uintptr_t)inUnit); return ee_astar_cmq_region_id(&this->mRecord, t.x, t.y, inKind); }
    long GetLargestContinent(long inKind) { return ee_astar_cmq_largest_continent(&this->mRecord, inKind); }   // 0x22952 (ret 4; EECPExplorationManager 0x7edf7 with 2 and 4) (provisional name)
    unsigned short GetContinentID(EEUnit* inUnit)
        { EeHullPoint t = ee_astar_unit_tile((uint64_t)(uintptr_t)inUnit); return this->OnGrid(t) ? this->GetContinentID(t.x, t.y) : 0; }   // 0x22712 (its tile's GetContinentID; EESelectedGroup 0x20924f)
    // 0x22b86: the id of kind inKind (1..4: the region's +0x86/+0x88/+0x8a/+0x84) of tile inTile's region, 0 if none
    unsigned short GetContinent(const EETileCoordinate& inTile, long inKind)
        { return ee_astar_cmq_region_id(&this->mRecord, inTile.mX, inTile.mY, inKind); }

    // Inline lookups (first emitted by EECPASGUAirBomber.cpp 0x60b4..0x6132; 78/65/5 call sites, never expanded):
    // 0x60ef: the continent id of tile (inX, inY): 0 off the map; mContinentIDs[inX * mWidth + inY], or the slow
    // FindContinentID while the grid is not built
    unsigned short GetContinentID(long inX, long inY) const {
        if ((unsigned long)inX < this->mRecord.width && (unsigned long)inY < this->mRecord.height) {
            if (this->mRecord.continent_ids == NULL) {
                return this->FindContinentID(inX, inY);
            }
            return this->mRecord.continent_ids[inX * this->mRecord.width + inY];
        }
        return 0;
    }
    // 0x60c7: the continent of tile (inX, inY), 0 if none (const: `lea; mov [eax]` for operator[])
    EEAIContinent* GetContinentAt(long inX, long inY) const {
        unsigned short theID = this->GetContinentID(inX, inY);
        if (theID) {
            return this->GetContinents()[theID];
        }
        return NULL;
    }
    EEAIContinent* GetContinentAt(const EETileCoordinate& inTile) const {
        long theY = inTile.mY;
        long theX = inTile.mX;
        return this->GetContinentAt(theX, theY);
    }

    // Out-of-line members (EEAIContinentManager.cpp; ret N / argument reads / the value left at ret)
    static void Shutdown();                 // 0x1ef12: Reset() of gContinentManager, delete[] the four grids (absolute)
    void Reset();                           // 0x1f503: clears every container and counter (not the grids)
    // 0x1f813: mTileValues = new long[mHeight * mWidth], zeroed
    void AllocateTileValues() { ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::TileValues, true, this->mRecord.height * this->mRecord.width); }
    void ClearChecksum() { this->mRecord.checksum = 0; }
    EEAIContinent* GetContinentAt(EEUnit* inUnit)          // 0x22658 (its tile, bounds-checked, then 0x60c7)
        { EeHullPoint t = ee_astar_unit_tile((uint64_t)(uintptr_t)inUnit); return this->OnGrid(t) ? this->GetContinentAt(t.x, t.y) : NULL; }
    unsigned short FindContinentID(long inX, long inY) const     // 0x22680: FindContinent(inX, inY)->+0x11c, 0 if none
        { return ee_astar_cm_found_continent_id(&this->mRecord, inX, inY); }
    // 0x21fff: inUnit can reach tile (inX, inY) (EEComplexUnit::IsOnOtherContinent for class flag 1) (provisional name)
    bool CanGetTo(EEUnit* inUnit, long inX, long inY);
    EEAIContinent* FindContinent(long inX, long inY) const       // 0x2269f: via the tile's path sector's sub-sectors
        { return (EEAIContinent*)(uintptr_t)ee_astar_cm_find_continent(&this->mRecord, inX, inY); }
    // 0x22a2f: the last free id of mFreeSubSectorIDs, else ++mLastSubSectorID
    unsigned short NewSubSectorID() { return ee_astar_cm_new_sub_sector_id(&this->mRecord); }
    // 0x22a70: the last free id of mFreePathAreaIDs, else mNextPathAreaID++
    unsigned short NewPathAreaID() { return ee_astar_cm_new_path_area_id(&this->mRecord); }
    // 0x22ad9: the path area of inTile's sub-sector (its +0x4e; open sub-sectors only), 0 off the map (provisional
    // name)
    // name (EEAIContinent::GetShoreInRegions)
    EEAIPathArea* GetPathAreaAt(const EETileLocation& inTile)
        { return (EEAIPathArea*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 3, inTile.mX, inTile.mY); }
    unsigned short GetPathAreaIDAt(const EETileLocation& inTile);   // 0x208334 (inline COMDAT, defined in EEGroup.cpp) (provisional name)
    unsigned short GetPathAreaIDAt(long inX, long inY);   // inline in EEGroup.cpp (Virtual38: refuses the ctor and the 0x208334 call)
    // 0x2311c (ret 0xc): whether tile (inX, inY) belongs to inID (EEAIContinent::GetShoreNextTo) (provisional name)
    bool IsNextToArea(long inX, long inY, unsigned short inID) { return ee_astar_cmq_is_next_to_area(&this->mRecord, inX, inY, inID); }
    EEAIPathSector* GetPathSector(EEUnit* inUnit)
        { EeHullPoint t = ee_astar_unit_tile((uint64_t)(uintptr_t)inUnit); EETileCoordinate c; c.mX = t.x; c.mY = t.y; return this->GetPathSector(c); }
    EEAIPathSector* GetPathSector(const EETileCoordinate& inTile) const    // 0x22bf8: 0 off the map
    {
        ptrdiff_t theIndex = ee_astar_cm_sector_index(&this->mRecord, inTile.mX, inTile.mY);
        return theIndex < 0 ? NULL : this->GetPathSectors()[theIndex];
    }
    // 0x236b5 takes the map (`ret 4`; it passes it to 0x1f8ae and 0x2033f): EEUserInterface calls
    // Clear(gWorld.mMap); the no-argument declaration above has no users
    void Clear(EEMap* inMap);
    void BuildRegions(EEMap* inMap);        // 0x24a24 (after Clear; resets mClearing; provisional name)
    // 0x24f70: erases inArea's serial from mAreas; true if it was there
    bool RemoveArea(EEAIArea* inArea) { return ee_astar_cm_remove_object_area(&this->mRecord, (uint64_t)(uintptr_t)inArea); }
    // 0x24fb5: erases its id (+0x48) from mSectorTiles
    void RemoveSectorTiles(EEAIPathSector* inSector) { ee_astar_cm_remove_sector_tiles_of(&this->mRecord, (uint64_t)(uintptr_t)inSector); }
    // 0x26f8d: mAreas.insert((its serial, inArea))
    void AddArea(EEAIArea* inArea) { ee_astar_cm_add_object_area(&this->mRecord, (uint64_t)(uintptr_t)inArea); }
    // 0x26fb3: 0 if not found
    EEAIArea* GetArea(unsigned long inSerial) const { return (EEAIArea*)(uintptr_t)ee_astar_cm_area(&this->mRecord, inSerial); }
    // 0x22dc8 (ret 8): a random area of kind inKind (EEHerd 0xa4aa2: 5) (provisional name)
    EEAIArea* GetRandomContinent(unsigned char inKind, URandom& inRandom)
        { return (EEAIArea*)(uintptr_t)ee_astar_cmq_random_area(&this->mRecord, true, inKind, (uint64_t)(uintptr_t)&inRandom, 0); }
    // inline twin of GetContinentByID: its undone COMDAT folds with 0x26fdd; at a low share its const operator[] goes out of line
    EEAIContinent* GetContinentFromID(unsigned short inID) const { return this->GetContinents()[inID]; }
    EEAIContinent* GetContinentByID(unsigned short inID) const { return this->GetContinents()[inID]; }
    // inline (EEAIPathSector::GetPathAreaAt 0x2bd4d: the value load, then the const operator[]'s dead lea) (provisional
    // name)
    // name
    EEAIPathArea* GetPathArea(unsigned short inID) const { return this->GetPathAreas()[inID]; }
    // 0x22872 (ret 0xc): whether inFirst and inSecond are connected under inFlags (1/2/8: the +0x112/+0x116/+0x10e ids)
    // (provisional name)
    bool AreConnected(EEAIContinent* inFirst, EEAIContinent* inSecond, long inFlags)
        { return ee_astar_cmq_are_connected((uint64_t)(uintptr_t)inFirst, (uint64_t)(uintptr_t)inSecond, inFlags); }
    // inline (COMDAT 0x934d6 after its first user EECPResourceManager::FindForestTarget; also 0x24f3a1) (provisional
    // name)
    bool AreConnectedByID(unsigned short inFirst, unsigned short inSecond, long inFlags)
    {
        EEAIHandleView<EEAIContinent> theContinents = this->GetContinents();
        EEAIContinent* theSecond = theContinents[inSecond];
        EEAIContinent* theFirst = theContinents[inFirst];
        return this->AreConnected(theFirst, theSecond, inFlags);
    }
    // area lookups by a tile (bodies read; names and the exact return classes inferred): the continent at the tile
    // (GetContinentAt), then the first of its id list (+0x7c chokes, +0x8c forests, +0xac hills) whose area holds
    // the tile
    EEAIChokePoint* GetChokePointAt(const EETileLocation& inTile)    // 0x221e2 (continent +0x11a must be 1)
        { return (EEAIChokePoint*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 0, inTile.mX, inTile.mY); }
    EEAIChokePoint* GetChokePointAt(EEUnit* inUnit)     // 0x221c1: at inUnit's tile (+0x1c, +0x20)
        { EeHullPoint t = ee_astar_unit_tile((uint64_t)(uintptr_t)inUnit); return (EEAIChokePoint*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 0, t.x, t.y); }
    EEAIForest* GetForestAt(const EETileLocation& inTile)
        { return (EEAIForest*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 1, inTile.mX, inTile.mY); }
    // the EEUnit overloads build an EETileLocation from the unit's tile (+0x1c, +0x20)
    EEAIForest* GetForestAt(EEUnit* inUnit)
        { EeHullPoint t = ee_astar_unit_tile((uint64_t)(uintptr_t)inUnit); return (EEAIForest*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 1, t.x, t.y); }
    // inline: the forest id at inTile, 0 if none (COMDAT 0x225272; defined in its only user EEUGGather.cpp, where
    // EEAIForest is complete) (provisional name)
    unsigned short GetForestIDAt(const EETileLocation& inTile);
    EEAIPathArea* GetPathAreaAt(EEUnit* inUnit)
        { EeHullPoint t = ee_astar_unit_tile((uint64_t)(uintptr_t)inUnit); return (EEAIPathArea*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 3, t.x, t.y); }
    EEAIPathSubSector* GetPathSubSectorAt(EEUnit* inUnit)
        { return (EEAIPathSubSector*)(uintptr_t)ee_astar_cmq_sub_sector_at(&this->mRecord, (uint64_t)(uintptr_t)inUnit); }
    // inline (COMDAT 0xabb3f after its first user EEPathManager::RequestUnitPaths): the path sub sector of id inID
    // Provisional name
    EEAIPathSubSector* GetPathSubSector(unsigned short inID) { return this->GetPathSubSectors()[inID]; }
    unsigned short GetPathSubSectorID(EEUnit* inUnit)                 // 0x22cd7 (0 off the grid)
        { EeHullPoint t = ee_astar_unit_tile((uint64_t)(uintptr_t)inUnit); return this->OnGrid(t) ? this->GetPathSubSectorID(t.x, t.y) : 0; }
    EEAIPathSubSector* FindPathSubSector(long inX, long inY)
        { return (EEAIPathSubSector*)(uintptr_t)ee_astar_cm_find_sub_sector(&this->mRecord, inX, inY); }
    unsigned short FindPathSubSectorID(long inX, long inY)            // 0x22cff: FindPathSubSector's id (+0x50), 0 if none
        { return ee_astar_cm_sub_sector_id_found(&this->mRecord, inX, inY); }
    bool InSameContinent(EEUnit* inFirst, EEUnit* inSecond)
        { return ee_astar_cmq_in_same_continent(&this->mRecord, (uint64_t)(uintptr_t)inFirst, (uint64_t)(uintptr_t)inSecond); }
    bool CanGetToUnit(EEUnit* inUnit, EEUnit* inTarget)               // 0x21fe7: CanGetTo(inUnit, inTarget's tile)
        { EeHullPoint t = ee_astar_unit_tile((uint64_t)(uintptr_t)inTarget); return this->CanGetTo(inUnit, t.x, t.y); }
    long GetTileFlag(long inX, long inY) { return ee_astar_tile_flag(inX, inY); }   // 0x21f5a: the map tile's +0x14 (0 off the map)
    // 0x22635 (const-shaped: lea; mov): the tiles of continent inID (provisional name)
    void GetContinentTiles(unsigned short inID, std::vector<U2DPoint<long> >& outTiles) const
    {
        ee_astar_cmq_continent_tiles(&this->mRecord, inID, EEAIHullPoints(outTiles), outTiles.size(), &outTiles,
            [](void* inTiles) { static_cast<std::vector<U2DPoint<long> >*>(inTiles)->clear(); },
            [](void* inTiles, long inX, long inY) { static_cast<std::vector<U2DPoint<long> >*>(inTiles)->push_back(U2DPoint<long>(inX, inY)); });
    }
    EEAIHill* GetHillAt(const EETileLocation& inTile)
        { return (EEAIHill*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 2, inTile.mX, inTile.mY); }
    // 0x273c9 (ret 8): id = 0x273f1(inX, inY); id ? mPathSubSectors[id] : 0 (the 0x60c7 shape over +0x5c)
    EEAIPathSubSector* GetPathSubSectorAt(long inX, long inY);
    bool Rebuild();                         // 0x245a5 (EEPathManager::UpdateRegions: false = not done yet; provisional name)
    // 0x4e6d4 (ret 4; EECPASGSea.cpp's range): GetPathSubSectorAt(inTile's x, y) (provisional name)
    EEAIPathSubSector* GetPathSubSectorAt(const EETileLocation& inTile);
    unsigned short GetPathSubSectorID(long inX, long inY);
    // 0x21e22: outIDs cleared, then the ids listed for inID (EETown::UpdateNearWater 0xabfda; provisional name)
    void GetConnectedIDs(std::vector<unsigned short>& outIDs, unsigned short inID) { this->AdjacentIDs(outIDs, inID, true); }
    // 0x22255 (ret 0xc): the choke points of continent inID into outChokePoints (cleared first when inClear)
    // Provisional name (EECPAttackGroup 0x54cea)
    void GetChokePoints(std::vector<EEAIChokePoint*>& outChokePoints, unsigned short inID, bool inClear)
    {
        if (inClear)
            outChokePoints.clear();
        ee_astar_cmq_choke_points(&this->mRecord, inID, inClear, outChokePoints.empty() ? NULL : (const uint64_t*)&outChokePoints[0],
            outChokePoints.size(), &outChokePoints, [](void* inList, uint64_t inChoke) {
                static_cast<std::vector<EEAIChokePoint*>*>(inList)->push_back((EEAIChokePoint*)(uintptr_t)inChoke); });
    }
    // 0x21e80: like GetConnectedIDs for another id list (EEUCTransport::ContinentsConnect; provisional name)
    void GetAdjacentIDs(std::vector<unsigned short>& outIDs, unsigned short inID) { this->AdjacentIDs(outIDs, inID, false); }
    void AdjacentIDs(std::vector<unsigned short>& outIDs, unsigned short inID, bool inSelf)
    {
        outIDs.resize(this->mRecord.flags_width > 0 ? this->mRecord.flags_width : 0);
        outIDs.resize(ee_astar_cmq_adjacent_ids(&this->mRecord, inID, inSelf, outIDs.empty() ? NULL : &outIDs[0]));
    }
    // inline (COMDAT 0x88ce9 after its first user EECPPersonnel::DraftUnit) (provisional name)
    unsigned long GetNumContinents() const { return this->GetContinents().size(); }
    void FreePathAreaID(unsigned short inID) { ee_astar_cm_add_free_ids(&this->mRecord, EeAStarCmFree::PathAreas, &inID, 1, false); }
    unsigned short GetAdjacentAreaID(long inKind, const EETileLocation& inTile, long inAreaKind)
        { return ee_astar_cm_adjacent_area_id(&this->mRecord, (int32_t)inKind, inTile.mX, inTile.mY, (int32_t)inAreaKind); }
    bool HasShoreTile(const std::vector<U2DPoint<long> >& inTiles) const
        { return ee_astar_cmq_has_shore_tile(&this->mRecord, EEAIHullPoints(inTiles), inTiles.size()); }
    bool InSameArea(const std::vector<EETileCoordinate>& inTiles, long inKind)
    {
        std::vector<U2DPoint<long> > theTiles(inTiles.begin(), inTiles.end());
        return ee_astar_cmq_in_same_area(&this->mRecord, EEAIHullPoints(theTiles), theTiles.size(), inKind);
    }
    void GetContinentCenter(unsigned short inID, U2DPoint<long>& outPoint)
        { EeHullPoint t = ee_astar_cmq_continent_center(&this->mRecord, inID); outPoint.mX = t.x; outPoint.mY = t.y; }
    bool AreConnectedForUnit(EEUnit* inUnit, EEAIContinent* inFirst, EEAIContinent* inSecond);
    EEAIArea* GetRandomArea(long inKind, URandom& inRandom, unsigned short inContinent)
        { return (EEAIArea*)(uintptr_t)ee_astar_cmq_random_area(&this->mRecord, false, inKind, (uint64_t)(uintptr_t)&inRandom, inContinent); }
    bool GetTilesNextTo(long inX, long inY, std::vector<U2DPoint<long> >& outTiles, unsigned short inID)
    {
        EePoint theTiles[8];
        size_t theCount = 0;
        bool theResult = ee_astar_cmq_tiles_next_to(&this->mRecord, inX, inY, inID, theTiles, &theCount);
        outTiles.clear();
        for (size_t k = 0; k < theCount; k++)
            outTiles.push_back(U2DPoint<long>(theTiles[k].x, theTiles[k].y));
        return theResult;
    }
    void UpdateTileValues(bool inWater) { ee_astar_cm_update_tile_values(&this->mRecord, inWater); }   // 0x231d7 (Rust: tile_values.rs)
    // 0x2372b (ret 4): the path areas after inTile changed kind (provisional name)
    void RepairPathArea(const U2DPoint<long>& inTile) { ee_astar_cmq_repair_path_area(&this->mRecord, inTile.mX, inTile.mY); }
    void RemoveSubSectors(EEAIPathSector* inSector) { ee_astar_cm_remove_sub_sectors(&this->mRecord, (uint64_t)(uintptr_t)inSector); }
    void RemoveForestTile(EEUnit* inUnit) { ee_astar_cmq_remove_forest_tile(&this->mRecord, (uint64_t)(uintptr_t)inUnit); }
    void UpdateWallUnit(EEComplexUnit* inUnit, bool inNow);
    // inline (COMDATs in this unit's range; defined inline in EEAIContinentManager.cpp)
    bool IsShoreTile(long inX, long inY) const { return ee_astar_cm_is_shore(&this->mRecord, inX, inY); }
    EEAIPathSector* GetPathSectorByID(unsigned short inID) const { return this->GetPathSectors()[inID]; }

    // New (port): the members are Rust's (ee_sim::astar cm.rs, EeAStarCmRecord): the counters, the sector grid (was
    // EEAIPathSectorRegionInformation, +0xdc) and the grids' pointers are the record, its scalars at fixed addresses
    // (Empire Earth.cpp's aliases); the tables behind it: the serial table (was mAreas +0x04), the objects by id of
    // the eight kinds (mContinents +0x3c, mPathSectors +0x4c, mPathSubSectors +0x5c, mForests +0x6c, mPathAreas
    // +0x7c, mHills +0x8c, mChokePoints +0x9c, mRectangleAreas +0xac), the pending sector tiles (mSectorTiles +0x10),
    // the shore lists (+0x1c, +0x2c) and the free ids (+0xbc, +0xcc)
    EeAStarCmRecord mRecord;
    template <class T> EEAIHandleView<T> Table(EeAStarCmTable inTable) const
    {
        size_t theCount;
        const uint64_t* theData = ee_astar_cm_table(&this->mRecord, inTable, &theCount);
        return EEAIHandleView<T>(theData, theCount);
    }
    void SetTable(EeAStarCmTable inTable, size_t inID, const void* inObject)
        { ee_astar_cm_table_set(&this->mRecord, inTable, inID, (uint64_t)(uintptr_t)inObject); }
    void ResizeTable(EeAStarCmTable inTable, size_t inCount) { ee_astar_cm_table_resize(&this->mRecord, inTable, inCount); }
    EEAIHandleView<EEAIContinent> GetContinents() const { return this->Table<EEAIContinent>(EeAStarCmTable::Continents); }
    EEAIHandleView<EEAIPathSector> GetPathSectors() const { return this->Table<EEAIPathSector>(EeAStarCmTable::PathSectors); }
    EEAIHandleView<EEAIPathSubSector> GetPathSubSectors() const
        { return this->Table<EEAIPathSubSector>(EeAStarCmTable::PathSubSectors); }
    EEAIHandleView<EEAIForest> GetForests() const { return this->Table<EEAIForest>(EeAStarCmTable::Forests); }
    EEAIHandleView<EEAIPathArea> GetPathAreas() const { return this->Table<EEAIPathArea>(EeAStarCmTable::PathAreas); }
    EEAIHandleView<EEAIHill> GetHills() const { return this->Table<EEAIHill>(EeAStarCmTable::Hills); }
    EEAIHandleView<EEAIChokePoint> GetChokePoints() const { return this->Table<EEAIChokePoint>(EeAStarCmTable::ChokePoints); }
    EEAIHandleView<EEAIRectangleArea> GetRectangleAreas() const
        { return this->Table<EEAIRectangleArea>(EeAStarCmTable::RectangleAreas); }
    EEAIContinent* GetContinentsAt(unsigned long inIndex) const { return this->GetContinents()[inIndex]; }
    EEAIPathSector* GetPathSectorsAt(unsigned long inIndex) const { return this->GetPathSectors()[inIndex]; }
    EEAIPathSubSector* GetPathSubSectorsAt(unsigned long inIndex) const { return this->GetPathSubSectors()[inIndex]; }
    EEAIChokePoint* GetChokePointsAt(unsigned long inIndex) const { return this->GetChokePoints()[inIndex]; }
    EEAIHill* GetHill(unsigned short inID) const { return this->GetHills()[inID]; }
    // the serial table, sorted by serial
    EEAIListView<EeAStarCmArea> GetAreas() const
    {
        size_t theCount;
        const EeAStarCmArea* theAreas = ee_astar_cm_areas(&this->mRecord, &theCount);
        return EEAIListView<EeAStarCmArea>(theAreas, theCount);
    }
    // copies of the shore lists, the free ids and the pending sector tiles (in id order)
    std::vector<U2DPoint<long> > GetShoreTiles(EeAStarCmShore inList) const
    {
        size_t theCount;
        const EeHullPoint* theTiles = ee_astar_cm_shore(&this->mRecord, inList, &theCount);
        const U2DPoint<long>* thePoints = reinterpret_cast<const U2DPoint<long>*>(theTiles);
        return std::vector<U2DPoint<long> >(thePoints, thePoints + theCount);
    }
    std::vector<unsigned short> GetFreeIDs(EeAStarCmFree inList) const
    {
        size_t theCount;
        const uint16_t* theIDs = ee_astar_cm_free_ids(&this->mRecord, inList, &theCount);
        return std::vector<unsigned short>(theIDs, theIDs + theCount);
    }
    std::vector<std::pair<unsigned short, std::vector<EETileCoordinate> > > GetSectorTiles() const
    {
        std::vector<std::pair<unsigned short, std::vector<EETileCoordinate> > > theResult(
            ee_astar_cm_sector_tiles_count(&this->mRecord));
        for (size_t i = 0; i < theResult.size(); i++) {
            size_t theCount;
            const EeHullPoint* theTiles = ee_astar_cm_sector_tiles(&this->mRecord, i, &theResult[i].first, &theCount);
            for (size_t k = 0; k < theCount; k++) {
                EETileCoordinate theTile;
                theTile.mX = theTiles[k].x;
                theTile.mY = theTiles[k].y;
                theResult[i].second.push_back(theTile);
            }
        }
        return theResult;
    }
    unsigned long NewSerial() { return ++this->mRecord.last_serial; }   // `++mLastSerial`
    unsigned long GetLastSerial() const { return this->mRecord.last_serial; }
    void SetLastSerial(unsigned long inSerial) { this->mRecord.last_serial = inSerial; }
    // the continents' adjacency matrix (mTileFlags, mFlagsWidth x mFlagsWidth)
    bool AreAdjacent(long inFrom, long inTo) const { return this->mRecord.tile_flags[inFrom * this->mRecord.flags_width + inTo]; }
    long GetFlagsWidth() const { return this->mRecord.flags_width; }
    const long* GetTileValues() const { return this->mRecord.tile_values; }
    unsigned short* GetPathSubSectorIDs() const { return this->mRecord.sub_sector_ids; }
    unsigned short NextChainID(long inKind) { return ++this->mRecord.next_ids[inKind]; }   // `++mNextIDs[kind]`
    long GetState() const { return this->mRecord.state; }
    bool IsClearing() const { return this->mRecord.clearing; }
    long GetNumWaterTiles() const { return this->mRecord.num_water_tiles; }
    long GetNumLandTiles() const { return this->mRecord.num_land_tiles; }
    long GetNumSpaceTiles() const { return this->mRecord.num_space_tiles; }
    unsigned long GetWidth() const { return this->mRecord.width; }
    // the unit lookups' bounds test (`mTileX < mWidth`: unsigned)
    bool OnGrid(const EeHullPoint& inTile) const
        { return (unsigned long)inTile.x < this->mRecord.width && (unsigned long)inTile.y < this->mRecord.height; }
    template <class P> static const EeHullPoint* EEAIHullPoints(const std::vector<P>& inPoints)
        { return inPoints.empty() ? NULL : reinterpret_cast<const EeHullPoint*>(&inPoints[0]); }
    unsigned long GetHeight() const { return this->mRecord.height; }
};

extern EEAIContinentManager gContinentManager;

// (cdecl; provisional names)
// (0x1ed2c EEAIGetTileKind, 0x1eea9 EEAIGetTileWallID, 0x1f65f GetResourceWeight: Rust, ee_sim::astar cm_host.rs)
// (0x1c949 EEAIBuildChokePoints, the terrain pass before PruneChokePoints: Rust, cm_passes.rs)
long EEGetRandomInt(long inMin, long inMax, URandom& inRandom);  // 0x2491c9 (a random index in [inMin, inMax]; body elsewhere) (provisional name)
extern long& gEEAIRegionSize;                            // 0x52fd80 (0x20b48 sets it by map size: 8..0x12) (provisional name)

// 0x273c9/0x273f1 (COMDATs; inline: every caller needs the definition)
inline EEAIPathSubSector* EEAIContinentManager::GetPathSubSectorAt(long inX, long inY)
{
    EEAIHandleView<EEAIPathSubSector> theSubSectors = this->GetPathSubSectors();
    unsigned short theID = this->GetPathSubSectorID(inX, inY);
    return theID ? theSubSectors[theID] : NULL;
}

inline unsigned short EEAIContinentManager::GetPathSubSectorID(long inX, long inY)
{
    if (!((unsigned long)inX < this->mRecord.width) || !((unsigned long)inY < this->mRecord.height))
        return 0;
    if (this->mRecord.sub_sector_ids)
        return this->mRecord.sub_sector_ids[this->mRecord.width * inX + inY];
    return this->FindPathSubSectorID(inX, inY);
    return 0;
}

// 0x29ced: the resource kind (1..3, by the tree type) of the first wood unit on inTile, not in deep water; 0 if none
// (Rust: ee_sim::astar world_view.rs)
class EETile;
inline long EEAIHasResourceUnit(EETile* inTile) { return ee_astar_tile_resource_kind((uint64_t)(uintptr_t)inTile); }

#endif
