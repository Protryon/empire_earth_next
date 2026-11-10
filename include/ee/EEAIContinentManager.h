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
#include "ee/EEAIPathSectorRegionInformation.h"

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
class EEAIAStarContinentStorage;
class EEAIAStarContinentGoal;
class EEAIAStarPathAreaStorage;
class EEAIAStarPathSectorStorage;
class EEAIAStarPathSectorGoal;
class EEAIFOContinentScreenContinentsShareAdjacentID;
class EEComplexUnit;
class URandom;
template <class S, class G, class M> class EEAIAStarMachine;

class EEAIContinentManager {
public:
    EEAIContinentManager();                 // 0x1ef6b (Empire Earth.cpp's chain)
    virtual ~EEAIContinentManager();        // +0x00 dtor 0x1f11f (??_G 0x1f103)
    // 0x22b62: the continent id of inUnit's tile (GetContinent(its (+0x1c, +0x20), inKind))
    unsigned short GetContinent(EEUnit* inUnit, long inKind);
    long GetLargestContinent(long inKind);   // 0x22952 (ret 4; EECPExplorationManager 0x7edf7 with 2 and 4) (provisional name)
    unsigned short GetContinentID(EEUnit* inUnit);   // 0x22712 (its tile's GetContinentID; EESelectedGroup 0x20924f)
    // 0x22b86: the id of kind inKind (1..4: the region's +0x86/+0x88/+0x8a/+0x84) of tile inTile's region, 0 if none
    unsigned short GetContinent(const EETileCoordinate& inTile, long inKind);

    // Inline lookups (first emitted by EECPASGUAirBomber.cpp 0x60b4..0x6132; 78/65/5 call sites, never expanded):
    // 0x60ef: the continent id of tile (inX, inY): 0 off the map; mContinentIDs[inX * mWidth + inY], or the slow
    // FindContinentID while the grid is not built
    unsigned short GetContinentID(long inX, long inY) const {
        if (inX < this->mWidth && inY < this->mHeight) {
            if (this->mContinentIDs == NULL) {
                return this->FindContinentID(inX, inY);
            }
            return this->mContinentIDs[inX * this->mWidth + inY];
        }
        return 0;
    }
    // 0x60c7: the continent of tile (inX, inY), 0 if none (const: `lea; mov [eax]` for operator[])
    EEAIContinent* GetContinentAt(long inX, long inY) const {
        unsigned short theID = this->GetContinentID(inX, inY);
        if (theID) {
            return this->mContinents[theID];
        }
        return NULL;
    }
    // 0x60b4
    EEAIContinent* GetContinentAt(const EETileCoordinate& inTile) const {
        long theY = inTile.mY;
        long theX = inTile.mX;
        return this->GetContinentAt(theX, theY);
    }

    // Out-of-line members (EEAIContinentManager.cpp; ret N / argument reads / the value left at ret)
    static void Shutdown();                 // 0x1ef12: Reset() of gContinentManager, delete[] the four grids (absolute)
    void AddChokePoint(EEAIChokePoint* inChokePoint);      // 0x1f1b8: mChokePoints[+0x78] = it; its continent's 0x26d84; AddArea
    void AddRectangleArea(EEAIRectangleArea* inArea);// 0x1f3a0: dynamic_cast<EEAIRectangleArea*>; mRectangleAreas[+0x8a] = it
    void Reset();                           // 0x1f503: clears every container and counter (not the grids)
    void DeleteChokePoints();               // 0x1f5cb
    void DeleteContinents();                // 0x1f5dd (also mL134/mUnitUnderMouse/mPlayerID = 0)
    void DeleteForests();                   // 0x1f600
    void DeleteHills();                     // 0x1f60b
    void DeletePathAreas();                 // 0x1f619
    void DeletePathSectors();               // 0x1f624 (also the sub-sector ids and mRegionInformation.Reset())
    void DeleteRectangleAreas();            // 0x1f651
    void AllocateTileValues();              // 0x1f813: mTileValues = new long[mHeight * mWidth], zeroed
    void ClearChecksum();                   // 0x1f863
    EEAIContinent* GetContinentAt(EEUnit* inUnit);         // 0x22658 (its tile, bounds-checked, then 0x60c7)
    unsigned short FindContinentID(long inX, long inY) const;    // 0x22680: FindContinent(inX, inY)->+0x11c, 0 if none
    // 0x21fff: inUnit can reach tile (inX, inY) (EEComplexUnit::IsOnOtherContinent for class flag 1) (provisional name)
    bool CanGetTo(EEUnit* inUnit, long inX, long inY);
    EEAIContinent* FindContinent(long inX, long inY) const;      // 0x2269f: via the tile's path sector's sub-sectors
    unsigned short NewSubSectorID();        // 0x22a2f: the last free id of mFreeSubSectorIDs, else ++mLastSubSectorID
    unsigned short NewPathAreaID();         // 0x22a70: the last free id of mFreePathAreaIDs, else mNextPathAreaID++
    // 0x22ad9: the path area of inTile's sub-sector (its +0x4e; open sub-sectors only), 0 off the map (provisional
    // name)
    // name (EEAIContinent::GetShoreInRegions)
    EEAIPathArea* GetPathAreaAt(const EETileLocation& inTile);
    unsigned short GetPathAreaIDAt(const EETileLocation& inTile);   // 0x208334 (inline COMDAT, defined in EEGroup.cpp) (provisional name)
    unsigned short GetPathAreaIDAt(long inX, long inY);   // inline in EEGroup.cpp (Virtual38: refuses the ctor and the 0x208334 call)
    // 0x2311c (ret 0xc): whether tile (inX, inY) belongs to inID (EEAIContinent::GetShoreNextTo) (provisional name)
    bool IsNextToArea(long inX, long inY, unsigned short inID);
    EEAIPathSector* GetPathSector(EEUnit* inUnit);         // 0x22bd7
    EEAIPathSector* GetPathSector(const EETileCoordinate& inTile) const;   // 0x22bf8: 0 off the map
    // 0x236b5 takes the map (`ret 4`; it passes it to 0x1f8ae and 0x2033f): EEUserInterface calls
    // Clear(gWorld.mMap); the no-argument declaration above has no users
    void Clear(EEMap* inMap);               // 0x236b5
    void BuildRegions(EEMap* inMap);        // 0x24a24 (after Clear; resets mClearing; provisional name)
    bool RemoveArea(EEAIArea* inArea);      // 0x24f70: erases inArea's serial from mAreas; true if it was there
    void RemoveSectorTiles(EEAIPathSector* inSector);      // 0x24fb5: erases its id (+0x48) from mSectorTiles
    void AddArea(EEAIArea* inArea);         // 0x26f8d: mAreas.insert((its serial, inArea))
    EEAIArea* GetArea(unsigned long inSerial) const;             // 0x26fb3: 0 if not found
    EEAIArea* GetRandomContinent(unsigned char inKind, URandom& inRandom);  // 0x22dc8 (ret 8): a random area of kind inKind (EEHerd 0xa4aa2: 5) (provisional name)
    // inline twin of GetContinentByID: its undone COMDAT folds with 0x26fdd; at a low share its const operator[] goes out of line
    EEAIContinent* GetContinentFromID(unsigned short inID) const { return this->mContinents[inID]; }
    EEAIContinent* GetContinentByID(unsigned short inID) const;  // 0x26fdd: mContinents[inID] (lea; mov: const-shaped)
    // inline (out-of-line COMDAT 0xa06ef, EECPWallManager.cpp: lea; mov, const-shaped) (provisional name)
    EEAIPathArea* GetPathAreaByID(unsigned short inID) const { return this->mPathAreas[inID]; }
    // inline (EEAIPathSector::GetPathAreaAt 0x2bd4d: the value load, then the const operator[]'s dead lea) (provisional
    // name)
    // name
    EEAIPathArea* GetPathArea(unsigned short inID) const { return this->mPathAreas[inID]; }
    // 0x22872 (ret 0xc): whether inFirst and inSecond are connected under inFlags (1/2/8: the +0x112/+0x116/+0x10e ids)
    // (provisional name)
    bool AreConnected(EEAIContinent* inFirst, EEAIContinent* inSecond, long inFlags);
    // inline (COMDAT 0x934d6 after its first user EECPResourceManager::FindForestTarget; also 0x24f3a1) (provisional
    // name)
    bool AreConnectedByID(unsigned short inFirst, unsigned short inSecond, long inFlags)
    {
        const std::vector<EEAIContinent*>& theContinents = this->mContinents;
        EEAIContinent* theSecond = theContinents[inSecond];
        EEAIContinent* theFirst = theContinents[inFirst];
        return this->AreConnected(theFirst, theSecond, inFlags);
    }
    // inline: the two connected-by-id arguments; at a low share the second argument's const operator[] is refused and the first
    // accessor undone (SendToGather 0x912be, 0x8f40b, 0x8fea6, 0x9055f, 0x90758, 0x6f5ee)
    bool AreConnectedFromIDs(unsigned short inFirst, unsigned short inSecond, long inFlags)
    {
        return this->AreConnected(this->GetContinentFromID(inFirst), this->GetContinentFromID(inSecond), inFlags);
    }
    // area lookups by a tile (bodies read; names and the exact return classes inferred): the continent at the tile
    // (GetContinentAt), then the first of its id list (+0x7c chokes, +0x8c forests, +0xac hills) whose area holds
    // the tile
    EEAIChokePoint* GetChokePointAt(const EETileLocation& inTile);    // 0x221e2 (continent +0x11a must be 1)
    EEAIChokePoint* GetChokePointAt(EEUnit* inUnit);    // 0x221c1: at inUnit's tile (+0x1c, +0x20)
    EEAIForest* GetForestAt(const EETileLocation& inTile);            // 0x2275b
    // the EEUnit overloads build an EETileLocation from the unit's tile (+0x1c, +0x20)
    EEAIForest* GetForestAt(EEUnit* inUnit);                          // 0x2273a
    // inline: the forest id at inTile, 0 if none (COMDAT 0x225272; defined in its only user EEUGGather.cpp, where
    // EEAIForest is complete) (provisional name)
    unsigned short GetForestIDAt(const EETileLocation& inTile);
    EEAIPathArea* GetPathAreaAt(EEUnit* inUnit);                      // 0x22ab8
    EEAIPathSubSector* GetPathSubSectorAt(EEUnit* inUnit);            // 0x22c51
    // inline (COMDAT 0xabb3f after its first user EEPathManager::RequestUnitPaths): the path sub sector of id inID
    // Provisional name
    EEAIPathSubSector* GetPathSubSector(unsigned short inID) { return this->mPathSubSectors[inID]; }
    unsigned short GetPathSubSectorID(EEUnit* inUnit);                // 0x22cd7 (0 off the grid)
    EEAIPathSubSector* FindPathSubSector(long inX, long inY);         // 0x22c65 (provisional name)
    unsigned short FindPathSubSectorID(long inX, long inY);           // 0x22cff: FindPathSubSector's id (+0x50), 0 if none
    bool InSameContinent(EEUnit* inFirst, EEUnit* inSecond);          // 0x21f13 (provisional name)
    bool CanGetToUnit(EEUnit* inUnit, EEUnit* inTarget);              // 0x21fe7: CanGetTo(inUnit, inTarget's tile)
    long GetTileFlag(long inX, long inY);                             // 0x21f5a: the map tile's +0x14 (0 off the map; movsx: a long)
    long GetResourceWeight(long inX, long inY);                       // 0x1f65f: 10 per unit of class +0x268 == 3 on the tile
    // 0x22635 (const-shaped: lea; mov): the tiles of continent inID (provisional name)
    void GetContinentTiles(unsigned short inID, std::vector<U2DPoint<long> >& outTiles) const;
    EEAIHill* GetHillAt(const EETileLocation& inTile);                // 0x227d0
    // 0x22eb4: the last of mRectangleAreas whose 0xc6fd(inTile) holds (argument type inferred)
    EEAIRectangleArea* GetRectangleAreaAt(const EETileLocation& inTile);
    // 0x273c9 (ret 8): id = 0x273f1(inX, inY); id ? mPathSubSectors[id] : 0 (the 0x60c7 shape over +0x5c)
    EEAIPathSubSector* GetPathSubSectorAt(long inX, long inY);
    bool Rebuild();                         // 0x245a5 (EEPathManager::UpdateRegions: false = not done yet; provisional name)
    // 0x4e6d4 (ret 4; EECPASGSea.cpp's range): GetPathSubSectorAt(inTile's x, y) (provisional name)
    EEAIPathSubSector* GetPathSubSectorAt(const EETileLocation& inTile);
    unsigned short GetPathSubSectorID(long inX, long inY);  // 0x273f1
    // 0x21e22: outIDs cleared, then the ids listed for inID (EETown::UpdateNearWater 0xabfda; provisional name)
    void GetConnectedIDs(std::vector<unsigned short>& outIDs, unsigned short inID);
    // 0x22255 (ret 0xc): the choke points of continent inID into outChokePoints (cleared first when inClear)
    // Provisional name (EECPAttackGroup 0x54cea)
    void GetChokePoints(std::vector<EEAIChokePoint*>& outChokePoints, unsigned short inID, bool inClear);
    // 0x21e80: like GetConnectedIDs for another id list (EEUCTransport::ContinentsConnect; provisional name)
    void GetAdjacentIDs(std::vector<unsigned short>& outIDs, unsigned short inID);
    EETileLocation GetNearestLandTile(const EETileLocation& inTile);  // 0x22320 (a tile by value; EECPASGLand::GetTargetContinent) (provisional name)
    // inline (COMDAT 0x88ce9 after its first user EECPPersonnel::DraftUnit) (provisional name)
    unsigned long GetNumContinents() { return this->mContinents.size(); }
    // (EEAIContinentManager.cpp; provisional names)
    void AddSectorTiles(std::vector<U2DPoint<long> >& ioTiles);       // 0x1f1f3 (ret 4)
    void GetPathAreas(EEMap* inMap, EEAIPathArea**& ioAreas);         // 0x1f45a (ret 8)
    void LinkSubSectors(std::vector<EEAIPathSubSector*>** inSubSectors, EEAIPathSubSector* inSubSector,
        const unsigned short* inID, bool inAdd, const std::pair<EEAIPathSubSector*, EEAIPathSubSector*>& inLink);   // 0x1f6b1 (ret 0x14; this unused)
    void Build(EEMap* inMap);               // 0x1f86b (ret 4)
    void CreateContinents(EEMap* inMap);    // 0x1f8ae (ret 4)
    void CreateContinentGrid(EEAIAStarMachine<EEAIAStarContinentStorage, EEAIAStarContinentGoal, EEMap>& inMachine,
        EEMap* inMap);                      // 0x1fa5c (ret 8; Rust: ee_astar continent_grid.rs; AddShoreTile 0x1f3e0 and AddTileValue 0x24fea are part of it)
    void PruneChokePoints();                // 0x24b31
    void NumberContinentChains(EEMap* inMap, unsigned long inKind);   // 0x201f7 (ret 8; inMap not read)
    void CreateHills(EEMap* inMap);         // 0x2033f (ret 4)
    void CreatePathAreaKind(EEMap* inMap, long inKind);               // 0x204ea (ret 8)
    void CreatePathAreas(EEMap* inMap);     // 0x208cc (ret 4)
    void CreatePathSectors(EEMap* inMap, EEAIAStarPathAreaStorage* inStorage);   // 0x20b48 (ret 8)
    void LinkSubSectorGrid(EEAIAStarMachine<EEAIAStarPathSectorStorage, EEAIAStarPathSectorGoal, EEMap>& inMachine, EEMap* inMap);   // 0x20ee1 (ret 8; inMap unused)
    // inline (COMDAT 0x27604 in this unit's range; defined inline in EEAIContinentManager.cpp): mPathSectors[its id]
    void SetPathSector(EEAIPathSector* inSector);
    void CreateScenarioAreas(EEMap* inMap); // 0x2124c (ret 4; inMap unused)
    void RelinkSubSectors(std::map<unsigned short, std::vector<EETileCoordinate> >& inSectors,
        std::vector<EEAIPathSubSector*>& inSubSectors);   // 0x215dc (ret 8)
    unsigned short GetAdjacentAreaID(long inKind, const EETileLocation& inTile, long inAreaKind);   // 0x21a18 (ret 0xc)
    bool HasShoreTile(const std::vector<U2DPoint<long> >& inTiles) const;   // 0x21ee2 (ret 4)
    bool InSameArea(const std::vector<EETileCoordinate>& inTiles, long inKind);   // 0x21f97 (ret 8)
    void GetAdjacentContinents(unsigned short inFirst, unsigned short inSecond, std::vector<unsigned short>& outIDs,
        EEAIFOContinentScreenContinentsShareAdjacentID* inScreen);   // 0x22443 (ret 0x10)
    void GetContinentCenter(unsigned short inID, U2DPoint<long>& outPoint);   // 0x2260b (ret 8)
    bool AreConnectedForUnit(EEUnit* inUnit, EEAIContinent* inFirst, EEAIContinent* inSecond);  // 0x22848 (ret 0xc)
    EEAIArea* GetRandomArea(long inKind, URandom& inRandom, unsigned short inContinent);   // 0x22d1b (ret 0xc)
    unsigned short GetUnitContinentID(EEUnit* inUnit);   // 0x22f15 (ret 4)
    bool GetTilesNextTo(long inX, long inY, std::vector<U2DPoint<long> >& outTiles, unsigned short inID);   // 0x22f5f (ret 0x10)
    bool AllNeighborsOfKind(unsigned short inID, unsigned char inKind);   // 0x2315c (ret 8)
    void UpdateTileValues(bool inWater);    // 0x231d7 (ret 4)
    void RepairPathArea(const U2DPoint<long>& inTile);  // 0x2372b (ret 4): the path areas after inTile changed kind (provisional name)
    void RemoveSubSectors(EEAIPathSector* inSector);   // 0x24dcf (ret 4)
    void RemoveForestTile(EEUnit* inUnit);  // 0x25325 (ret 4)
    void UpdateWallUnit(EEComplexUnit* inUnit, bool inNow);   // 0x25606 (ret 8)
    // inline (COMDATs in this unit's range; defined inline in EEAIContinentManager.cpp)
    bool IsShoreTile(long inX, long inY) const;   // 0x26fed: binary_search of (inX, inY) in mShoreTiles
    EEAIPathSector* GetPathSectorByID(unsigned short inID) const;     // 0x273b9: mPathSectors[inID]

    std::map<unsigned long, EEAIArea*> mAreas;             // +0x04 (node 0x18; key = EEAIArea +0x60, unsigned `jb`)
    // +0x10: tiles per path sector id (node 0x24: key unsigned short, `jb` word compare in 0x207724; the node's
    // vector<EETileCoordinate>::_Tidy in _Erase 0x26366)
    std::map<unsigned short, std::vector<EETileCoordinate> > mSectorTiles;
    std::vector<U2DPoint<long> > mShoreTiles; // +0x1c (sorted with U2DPoint<long>::operator< 0x1308c; Reset clears it)
    std::vector<U2DPoint<long> > mIceShoreTiles; // +0x2c
    EEAIContinent* GetContinentsAt(unsigned long inIndex) const { return this->mContinents[inIndex]; }
    std::vector<EEAIContinent*> mContinents;               // +0x3c by id (EEAIContinent +0x11c; 0x60c7)
    EEAIPathSector* GetPathSectorsAt(unsigned long inIndex) const { return this->mPathSectors[inIndex]; }
    std::vector<EEAIPathSector*> mPathSectors;             // +0x4c by id (new 0x4c + ctor 0x2bbd3, vtable 0x43a52c; id +0x48)
    EEAIPathSubSector* GetPathSubSectorsAt(unsigned long inIndex) const { return this->mPathSubSectors[inIndex]; }
    std::vector<EEAIPathSubSector*> mPathSubSectors;       // +0x5c by id (not owned: Reset only clear()s it)
    // inline: the no-/Og forest sort predicate 0x48e745 reads both forests through it (its nested
    // begin/_Ptrit::operator+ budget: both operator+ out of line) (provisional name)
    EEAIForest* GetForest(unsigned short inID) { return this->mForests[inID]; }
    EEAIForest* GetForestsAt(unsigned long inIndex) const { return this->mForests[inIndex]; }
    std::vector<EEAIForest*> mForests;      // +0x6c by id (+0x82; continent +0x80: ctor 0x29bcd, vtable 0x43a42c)
    std::vector<EEAIPathArea*> mPathAreas;  // +0x7c by id (+0x7a = NewPathAreaID(); ctor 0x18a03, vtable 0x439fac)
    std::vector<EEAIHill*> mHills;          // +0x8c by id (+0x6a; continent +0x68)
    EEAIChokePoint* GetChokePointsAt(unsigned long inIndex) const { return this->mChokePoints[inIndex]; }
    std::vector<EEAIChokePoint*> mChokePoints;             // +0x9c by +0x78 (ctor 0x1109a, vtable 0x439d3c)
    std::vector<EEAIRectangleArea*> mRectangleAreas;       // +0xac by +0x8a (0x1f3a0's dynamic_cast)
    std::vector<unsigned short> mFreeSubSectorIDs;         // +0xbc (pop_back in NewSubSectorID)
    std::vector<unsigned short> mFreePathAreaIDs;          // +0xcc (pop_back in NewPathAreaID; 0x2372b push_backs)
    EEAIPathSectorRegionInformation mRegionInformation;    // +0xdc (0x14)
    long* mTileValues;                      // +0xf0 per tile, new long[] (0x1f813; 0x231d7 stores values >= -0x78)
    unsigned short* mContinentIDs;          // +0xf4 per tile (0x60ef reads it; new unsigned short[] in 0x1fa5c)
    unsigned short* GetPathSubSectorIDs() const { return this->mPathSubSectorIDs; }
    unsigned short* mPathSubSectorIDs;      // +0xf8 per tile (0x20ee1 allocates mHeight * mWidth * 2)
    bool* mTileFlags;                       // +0xfc (mFlagsWidth + 1) * n bytes set to 1 by 0x1fa5c
    unsigned short mNextIDs[7];             // +0x100 per kind < 7 (0x15dd5: `++mNextIDs[kind]`); memset 14 in ctor/Reset
    long mState;                            // +0x110 (0 / 2; 0x22ad9 tests == 2)
    unsigned long mLastSerial;              // +0x114 area serials (`++`, then stored into EEAIArea +0x60)
    long mL118;                             // +0x118 (only zeroed: ctor, Reset)
    unsigned short mNextPathAreaID;         // +0x11c (post-increment in NewPathAreaID; 0x208cc sets it)
    unsigned short mLastSubSectorID;        // +0x11e (pre-increment in NewSubSectorID; 0x20b48 resizes +0x5c to it + 1)
    long mChecksum;                         // +0x120 (ClearChecksum; 0x1520cf adds to it)
    long mNextID124;                        // +0x124 (`(unsigned short)mNextID124++` in 0x2372b, as are the next three)
    long mNextID128;                        // +0x128
    long mNextID12C;                        // +0x12c
    long mNextID130;                        // +0x130
    long GetNumWaterTiles() const { return this->mNumWaterTiles; }
    long mNumWaterTiles;                    // +0x134 per-kind continent counts (DeleteContinents zeroes them; 0x1f8ae adds)
    long GetNumLandTiles() const { return this->mNumLandTiles; }
    long mNumLandTiles;                     // +0x138
    long GetNumSpaceTiles() const { return this->mNumSpaceTiles; }
    long mNumSpaceTiles;                    // +0x13c
    long mFlagsWidth;                       // +0x140 (mTileFlags row stride - 1; 0x1fa5c sets it)
    unsigned long GetWidth() const { return this->mWidth; }
    unsigned long mWidth;                   // +0x144 tiles (unsigned `jae` against long x in 0x60ef)
    unsigned long GetHeight() const { return this->mHeight; }
    unsigned long mHeight;                  // +0x148
    bool mClearing;                         // +0x14c (Clear sets it, 0x24a24 resets it, 0x25606 tests it)
};

extern EEAIContinentManager gContinentManager;   // 0x530bf0 (provisional name)

// (cdecl; provisional names)
long EEAIGetTileKind(const EETileLocation& inTile);     // 0x1ed2c (EEAIContinentManager.cpp; the caller 0x2380c)
long EEAIGetTileWallID(const EETileLocation& inTile);   // 0x1eea9 (EEAIContinentManager.cpp)
void EEAIBuildChokePoints(EEMap* inMap, long inKind);   // 0x1c949 (the terrain pass before PruneChokePoints; body elsewhere)
long EEGetRandomInt(long inMin, long inMax, URandom& inRandom);  // 0x2491c9 (a random index in [inMin, inMax]; body elsewhere) (provisional name)
extern long& gEEAIRegionSize;                            // 0x52fd80 (0x20b48 sets it by map size: 8..0x12) (provisional name)

// 0x273c9/0x273f1 (COMDATs; inline: every caller needs the definition)
inline EEAIPathSubSector* EEAIContinentManager::GetPathSubSectorAt(long inX, long inY)
{
    const std::vector<EEAIPathSubSector*>& theSubSectors = this->mPathSubSectors;
    unsigned short theID = this->GetPathSubSectorID(inX, inY);
    return theID ? theSubSectors[theID] : NULL;
}

inline unsigned short EEAIContinentManager::GetPathSubSectorID(long inX, long inY)
{
    if (!((unsigned long)inX < this->mWidth) || !((unsigned long)inY < this->mHeight))
        return 0;
    if (this->mPathSubSectorIDs)
        return this->mPathSubSectorIDs[this->mWidth * inX + inY];
    return this->FindPathSubSectorID(inX, inY);
    return 0;
}

#endif
