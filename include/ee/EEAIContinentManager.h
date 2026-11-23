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
class EEAIPathSubSector;
class EEMap;
class URandom;

class EEAIContinentManager;
extern EEAIContinentManager gContinentManager;

class EEAIContinentManager {
public:
    EEAIContinentManager();                 // 0x1ef6b (Empire Earth.cpp's chain)
    virtual ~EEAIContinentManager();        // +0x00 dtor 0x1f11f (??_G 0x1f103)
    // 0x22b62: the continent id of inUnit's tile (GetContinent(its (+0x1c, +0x20), inKind))
    unsigned short GetContinent(EEUnit* inUnit, long inKind)
        { return ee_astar_cmq_unit_region_id(&this->mRecord, (uint64_t)(uintptr_t)inUnit, inKind); }
    long GetLargestContinent(long inKind) { return ee_astar_cmq_largest_continent(&this->mRecord, inKind); }   // 0x22952 (ret 4; EECPExplorationManager 0x7edf7 with 2 and 4) (provisional name)
    unsigned short GetContinentID(EEUnit* inUnit)
        { return ee_astar_cm_unit_continent_id(&this->mRecord, (uint64_t)(uintptr_t)inUnit); }   // 0x22712 (its tile's GetContinentID; EESelectedGroup 0x20924f)
    // 0x22b86: the id of kind inKind (1..4: the region's +0x86/+0x88/+0x8a/+0x84) of tile inTile's region, 0 if none
    unsigned short GetContinent(const EETileCoordinate& inTile, long inKind)
        { return ee_astar_cmq_region_id(&this->mRecord, inTile.mX, inTile.mY, inKind); }

    // Inline lookups (first emitted by EECPASGUAirBomber.cpp 0x60b4..0x6132; 78/65/5 call sites, never expanded):
    // 0x60ef: the continent id of tile (inX, inY): 0 off the map; mContinentIDs[inX * mWidth + inY], or the slow
    // FindContinentID while the grid is not built
    unsigned short GetContinentID(long inX, long inY) const { return ee_astar_cm_continent_id_at(&this->mRecord, inX, inY); }
    // 0x60c7: the continent of tile (inX, inY), 0 if none
    EEAIContinent* GetContinentAt(long inX, long inY) const
        { return (EEAIContinent*)(uintptr_t)ee_astar_cm_continent_at(&this->mRecord, inX, inY); }
    EEAIContinent* GetContinentAt(const EETileCoordinate& inTile) const { return this->GetContinentAt(inTile.mX, inTile.mY); }

    // Out-of-line members (EEAIContinentManager.cpp; ret N / argument reads / the value left at ret)
    // 0x1ef12: Reset() of gContinentManager, delete[] the four grids (absolute; Rust: cm_passes.rs)
    static void Shutdown() { ee_astar_cm_shutdown(&gContinentManager.mRecord); }
    // 0x1f503: deletes every area, path sector and sub sector, empties the tables, zeroes the counters (not the grids)
    void Reset() { ee_astar_cm_reset_all(&this->mRecord); }
    EEAIContinent* GetContinentAt(EEUnit* inUnit)          // 0x22658 (its tile, bounds-checked, then 0x60c7)
        { return (EEAIContinent*)(uintptr_t)ee_astar_cm_unit_continent_at(&this->mRecord, (uint64_t)(uintptr_t)inUnit); }
    // 0x21fff: inUnit can reach tile (inX, inY) (EEComplexUnit::IsOnOtherContinent for class flag 1) (provisional name)
    bool CanGetTo(EEUnit* inUnit, long inX, long inY) { return ee_astar_cm_can_get_to((uint64_t)(uintptr_t)inUnit, inX, inY); }   // cm_unit.rs
    // 0x22ad9: the path area of inTile's sub-sector (its +0x4e; open sub-sectors only), 0 off the map (provisional
    // name)
    // name (EEAIContinent::GetShoreInRegions)
    EEAIPathArea* GetPathAreaAt(const EETileLocation& inTile)
        { return (EEAIPathArea*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 3, inTile.mX, inTile.mY); }
    unsigned short GetPathAreaIDAt(const EETileLocation& inTile);   // 0x208334 (inline COMDAT, defined in EEGroup.cpp) (provisional name)
    unsigned short GetPathAreaIDAt(long inX, long inY);   // inline in EEGroup.cpp (Virtual38: refuses the ctor and the 0x208334 call)
    // 0x2311c (ret 0xc): whether tile (inX, inY) belongs to inID (EEAIContinent::GetShoreNextTo) (provisional name)
    bool IsNextToArea(long inX, long inY, unsigned short inID) { return ee_astar_cmq_is_next_to_area(&this->mRecord, inX, inY, inID); }
    // 0x236b5 takes the map (`ret 4`; it passes it to 0x1f8ae and 0x2033f): EEUserInterface calls
    // Clear(gWorld.mMap); the no-argument declaration above has no users
    void Clear(EEMap* inMap);
    void BuildRegions(EEMap* inMap);        // 0x24a24 (after Clear; resets mClearing; provisional name)
    // 0x24f70: erases inArea's serial from mAreas; true if it was there
    bool RemoveArea(EEAIArea* inArea) { return ee_astar_cm_remove_object_area(&this->mRecord, (uint64_t)(uintptr_t)inArea); }
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
    // 0x22872 (ret 0xc): whether inFirst and inSecond are connected under inFlags (1/2/8: the +0x112/+0x116/+0x10e ids)
    // (provisional name)
    bool AreConnected(EEAIContinent* inFirst, EEAIContinent* inSecond, long inFlags)
        { return ee_astar_cmq_are_connected((uint64_t)(uintptr_t)inFirst, (uint64_t)(uintptr_t)inSecond, inFlags); }
    // area lookups by a tile (bodies read; names and the exact return classes inferred): the continent at the tile
    // (GetContinentAt), then the first of its id list (+0x7c chokes, +0x8c forests, +0xac hills) whose area holds
    // the tile
    EEAIChokePoint* GetChokePointAt(const EETileLocation& inTile)    // 0x221e2 (continent +0x11a must be 1)
        { return (EEAIChokePoint*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 0, inTile.mX, inTile.mY); }
    EEAIChokePoint* GetChokePointAt(EEUnit* inUnit)     // 0x221c1: at inUnit's tile (+0x1c, +0x20)
        { return (EEAIChokePoint*)(uintptr_t)ee_astar_cmq_unit_area_at(&this->mRecord, 0, (uint64_t)(uintptr_t)inUnit); }
    EEAIForest* GetForestAt(const EETileLocation& inTile)
        { return (EEAIForest*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 1, inTile.mX, inTile.mY); }
    // the EEUnit overloads build an EETileLocation from the unit's tile (+0x1c, +0x20)
    EEAIForest* GetForestAt(EEUnit* inUnit)
        { return (EEAIForest*)(uintptr_t)ee_astar_cmq_unit_area_at(&this->mRecord, 1, (uint64_t)(uintptr_t)inUnit); }
    // inline: the forest id at inTile, 0 if none (COMDAT 0x225272; defined in its only user EEUGGather.cpp, where
    // EEAIForest is complete) (provisional name)
    unsigned short GetForestIDAt(const EETileLocation& inTile);
    EEAIPathArea* GetPathAreaAt(EEUnit* inUnit)
        { return (EEAIPathArea*)(uintptr_t)ee_astar_cmq_unit_area_at(&this->mRecord, 3, (uint64_t)(uintptr_t)inUnit); }
    EEAIPathSubSector* GetPathSubSectorAt(EEUnit* inUnit)
        { return (EEAIPathSubSector*)(uintptr_t)ee_astar_cmq_sub_sector_at(&this->mRecord, (uint64_t)(uintptr_t)inUnit); }
    bool CanGetToUnit(EEUnit* inUnit, EEUnit* inTarget)               // 0x21fe7: CanGetTo(inUnit, inTarget's tile)
        { return ee_astar_cm_can_get_to_unit((uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)inTarget); }
    // 0x22635 (const-shaped: lea; mov): the tiles of continent inID (provisional name)
    void GetContinentTiles(unsigned short inID, std::vector<U2DPoint<long> >& outTiles) const
        { ee_astar_cmq_continent_tiles(&this->mRecord, inID, EEAIHullPoints(outTiles), outTiles.size(), &outTiles, EEAIClearList<std::vector<U2DPoint<long> > >, EEAIPushPoint); }
    EEAIHill* GetHillAt(const EETileLocation& inTile)
        { return (EEAIHill*)(uintptr_t)ee_astar_cmq_area_at(&this->mRecord, 2, inTile.mX, inTile.mY); }
    // 0x273c9 (ret 8): id = 0x273f1(inX, inY); id ? mPathSubSectors[id] : 0 (the 0x60c7 shape over +0x5c)
    EEAIPathSubSector* GetPathSubSectorAt(long inX, long inY)   // 0x273c9 (the grid's id, or the found one)
        { return (EEAIPathSubSector*)(uintptr_t)ee_astar_cm_sub_sector_at(&this->mRecord, inX, inY); }
    // 0x245a5 Rebuild (EEPathManager::UpdateRegions: false = not done yet): Rust (pm_host.rs over cm_passes.rs)
    // 0x21e80: like GetConnectedIDs for another id list (EEUCTransport::ContinentsConnect; provisional name)
    void GetAdjacentIDs(std::vector<unsigned short>& outIDs, unsigned short inID)
        { ee_astar_cmq_adjacent_ids_into(&this->mRecord, inID, false, &outIDs, EEAIResizeIDs, EEAIIDsData); }
    bool HasShoreTile(const std::vector<U2DPoint<long> >& inTiles) const
        { return ee_astar_cmq_has_shore_tile(&this->mRecord, EEAIHullPoints(inTiles), inTiles.size()); }
    bool InSameArea(const std::vector<EETileCoordinate>& inTiles, long inKind)
        { return ee_astar_cmq_in_same_area(&this->mRecord, EEAIHullPoints(inTiles), inTiles.size(), inKind); }
    void GetContinentCenter(unsigned short inID, U2DPoint<long>& outPoint)
        { *reinterpret_cast<EeHullPoint*>(&outPoint) = ee_astar_cmq_continent_center(&this->mRecord, inID); }
    EEAIArea* GetRandomArea(long inKind, URandom& inRandom, unsigned short inContinent)
        { return (EEAIArea*)(uintptr_t)ee_astar_cmq_random_area(&this->mRecord, false, inKind, (uint64_t)(uintptr_t)&inRandom, inContinent); }
    void RemoveForestTile(EEUnit* inUnit) { ee_astar_cmq_remove_forest_tile(&this->mRecord, (uint64_t)(uintptr_t)inUnit); }

    // New (port): the members are Rust's (ee_sim::astar cm.rs, EeAStarCmRecord): the counters, the sector grid (was
    // EEAIPathSectorRegionInformation, +0xdc) and the grids' pointers are the record, its scalars at fixed addresses
    // (Empire Earth.cpp's aliases); the tables behind it: the serial table (was mAreas +0x04), the objects by id of
    // the eight kinds (mContinents +0x3c, mPathSectors +0x4c, mPathSubSectors +0x5c, mForests +0x6c, mPathAreas
    // +0x7c, mHills +0x8c, mChokePoints +0x9c, mRectangleAreas +0xac), the pending sector tiles (mSectorTiles +0x10),
    // the shore lists (+0x1c, +0x2c) and the free ids (+0xbc, +0xcc)
    EeAStarCmRecord mRecord;
    template <class T> EEAIHandleView<T> Table(EeAStarCmTable inTable) const
        { return EEAIHandleView<T>(ee_astar_cm_table, &this->mRecord, inTable); }
    EEAIHandleView<EEAIContinent> GetContinents() const { return this->Table<EEAIContinent>(EeAStarCmTable::Continents); }
    EEAIHandleView<EEAIPathSubSector> GetPathSubSectors() const
        { return this->Table<EEAIPathSubSector>(EeAStarCmTable::PathSubSectors); }
    EEAIHandleView<EEAIForest> GetForests() const { return this->Table<EEAIForest>(EeAStarCmTable::Forests); }
    unsigned long NewSerial() { return ++this->mRecord.last_serial; }   // `++mLastSerial`
    // the continents' adjacency matrix (mTileFlags, mFlagsWidth x mFlagsWidth)
    bool AreAdjacent(long inFrom, long inTo) const { return this->mRecord.tile_flags[inFrom * this->mRecord.flags_width + inTo]; }
    template <class P> static const EeHullPoint* EEAIHullPoints(const std::vector<P>& inPoints)
        { return inPoints.empty() ? NULL : reinterpret_cast<const EeHullPoint*>(&inPoints[0]); }
};


// (cdecl; provisional names)
// (0x1ed2c EEAIGetTileKind, 0x1eea9 EEAIGetTileWallID, 0x1f65f GetResourceWeight: Rust, ee_sim::astar cm_host.rs)
// (0x1c949 EEAIBuildChokePoints, the terrain pass before PruneChokePoints: Rust, cm_passes.rs)
long EEGetRandomInt(long inMin, long inMax, URandom& inRandom);  // 0x2491c9 (a random index in [inMin, inMax]; body elsewhere) (provisional name)
extern long& gEEAIRegionSize;                            // 0x52fd80 (0x20b48 sets it by map size: 8..0x12) (provisional name)

#endif
