// C:\EEX\Empire Earth\RTS Map\EEMap.h (RECONSTRUCTION; file name inferred). The game map (RTTI EEMap, vtables
// 0x441cbc/0x441c9c), reached through gWorld.mMap. Only what the decompiled units read. Requires lle/GETerrainMesh.h.
#ifndef EE_EEMAP_H
#define EE_EEMAP_H

#include <stdio.h>
#include <vector>
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/U2DPoint.h"               // RTSMapTileRun holds two by value
#include "lle/U2DRectangle.h"           // RTSColoredRectangle holds one by value
#include "lle/GEColor.h"               // RTSColoredRectangle holds one by value
#include "ee/EERandomMapIDs.h"

// One map tile (provisional name and layout)
class EEDoppelganger;
class EEPlayer;
template <class T> class U2DPoint;
typedef U2DPoint<long> EETileLocation;   // ee/EETileLocation.h
class GE2DPoint;
class TSCriticalSection;
class GE3DPoint;
class RTSMap;

// A map tile: RTTI EETile > RTSTile (vtables 0x441d0c / 0x441dd0). The map allocates them as
// one array (new EETile[n], 0x28 bytes each, 0x11bb2e); EETile's ctor 0x11e266 runs RTSTile(0.0f) 0x124684 and buys
// a vector. Was EEMapTile (the typedef keeps that spelling; names.txt 42e2 carries EETile now). Names are provisional.
// One entry of a tile's unit list (provisional name and layout; EEGhostUnit 0x203d43)
class EEUnit;
// RTTI RTSTUnitList (vtable 0x441d88, ??_G 0x121c4f); the nodes are EETUnitLists from EETile.cpp's pool
// (were RTSTileUnitNode; the typedef keeps that spelling). The ctor stores both members before the vptr
// (init list); the dtor (inline: EETUnitList's ??_E 0x121b37 expands it) dereferences the unit
class RTSTUnitList {
public:
    EEUnit* GetUnit() const { return this->mUnit; }
    RTSTUnitList* GetNext() const { return this->mNext; }
    RTSTUnitList() : mUnit(NULL), mNext(NULL) {}
    virtual ~RTSTUnitList();                // +0x00 (inline: defined below where ee/EEUnit.h came first)
    EEUnit* mUnit;                          // +0x04 (referenced)
    RTSTUnitList* mNext;                    // +0x08
};
typedef RTSTUnitList RTSTileUnitNode;

// RTTI EETUnitList > RTSTUnitList (vtable 0x441d80, ??_E 0x121b37: an implicit dtor); sizeof 0xc (EETile.cpp's
// UMemoryPool<EETUnitList> 0x5297e0 makes them)
class EETUnitList : public RTSTUnitList {
};

class RTSTile {
public:
    RTSTile(float inHeight);                // 0x124684: +4/+8 = 0, +0x10 = inHeight, +0x14 = false
    virtual ~RTSTile();                     // +0x00 (??_G 0x1246a1; 0x1246bd only stores the vptr)
    // +0x04 0x124719: mTerrainType plus every player's visibility count (provisional name)
    virtual long GetChecksum(unsigned long inPlayerCount) const;
    // +0x08 0x124756: mHeight, then the visibility words (Read(char*, n, outRead) into mVisibility)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                      unsigned long inPlayerCount);
    // +0x0c 0x124793: fprintf of the visibility counts and the terrain type (provisional name)
    virtual void Dump(FILE* inFile, unsigned long inPlayerCount) const;
    // +0x10 0x124895: mHeight, then the visibility words (Write(const char*, n, outWritten))
    virtual long Save(FSFile& outFile, unsigned long inPlayerCount);
    // +0x14 0x1248ec: mTerrainType = inTerrainType (EETile's override also marks the mesh's space tiles)
    virtual void SetTerrainType(long inTerrainType, const EETileLocation& inLocation, RTSMap* inMap);
    // +0x18: inUnit enters the tile: a new node (pool 0x5297e0, inUnit referenced) in front of inNext
    // (EEUCResource::AddToTile stores the result in mUnits) (provisional name)
    virtual RTSTileUnitNode* AddUnit(EEUnit* inUnit, RTSTileUnitNode* inNext) = 0;
    virtual void FreeUnitNode(RTSTileUnitNode* inNode);  // +0x1c 0x124708 (deletes it; EETile returns it to its pool)
    void RemoveAllUnits();                  // 0x12483c (provisional name)
    void RemoveUnit(EEUnit* inUnit);        // 0x12485f (provisional name)
    bool HasUnit(EEUnit* inUnit);           // 0x1248d5 (provisional name)
    void AddSight(EEPlayer* inPlayer);      // 0x1247ed (counts inPlayer in; a first sighting stores 2) (provisional name)
    // inline: out-of-line COMDAT 0x124072 after its first user in RTSMap.cpp (AddSight calls it): stores inValue in
    // inPlayer's 16-bit half of the visibility word (provisional name)
    void SetVisibility(unsigned long inValue, EEPlayer* inPlayer);
    static const UString sElevation;        // "RTSTile-Elevation" (0x529878; key names are provisional)
    static const UString sModelIndex;       // "RTSTile-ModelIndex" (0x529860)
    static const UString sTerrainIndex;     // "RTSTile-TerrainIndex" (0x529850)
    void RemoveSight(EEPlayer* inPlayer);   // 0x1246c4 (EEPlayer::RemoveSharedSight/RemoveOwnSight; provisional name)

    long GetTerrainType() const { return this->mTerrainType; }  // inline (EEUCTransport::FilterLoadable 0x61c806 budget) r37_big B2
    long mTerrainType;                      // +0x04 (calamity effects sit at height 0 on 1 and 10)
    RTSTileUnitNode* GetUnits() const { return this->mUnits; }
    RTSTileUnitNode* mUnits;                // +0x08 the units on the tile (EEGhostUnit 0x203d43 walks them; = 0 by the ctor)
    // inline (COMDAT 0x2035cf; expanded in EEDoppelgangerManager 0x20266e): player inPlayerID's visibility of the
    // tile, two 16-bit entries per dword (provisional name)
    unsigned long GetVisibility(unsigned long inPlayerID) const
    {
        return (this->mVisibility[inPlayerID >> 1] >> ((inPlayerID & 1) << 4)) & 0xff;
    }
    // inline accessor (EEAIAStarHillGoal::AcceptTile 0x16db8 loads it before the divisor: an accessor value)
    // (provisional name)
    float GetHeight() const { return this->mHeight; }
    uint32_t* mVisibility;                  // +0x0c (not set by the ctor; 32-bit words: two players per word, saved)
    float mHeight;                          // +0x10 (EECalamityManager::CreateLocationCalamity: the calamity's z)
    signed char GetAIValue() const { return this->mAIValue; }
    signed char mAIValue;                   // +0x14 the AI tile value (GetTileFlag 0x21f5a movsx; RemoveForestTile 0x425325 -= 10 floored at 0)
};

class EETile : public RTSTile {
public:
    // inline (out-of-line COMDAT 0x11e266 after its first user, EEMap::ReserveTiles's new[])
    EETile() : RTSTile(0.0f) {}
    virtual ~EETile();                      // +0x00 (??_G 0x11bba9)
    virtual long GetChecksum(unsigned long inPlayerCount) const;  // +0x04 0x12124e
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                      unsigned long inPlayerCount);           // +0x08 0x12128d
    virtual void Dump(FILE* inFile, unsigned long inPlayerCount) const;   // +0x0c 0x1212ec
    virtual long Save(FSFile& outFile, unsigned long inPlayerCount);   // +0x10 0x12139e
    // +0x14 0x1214bf: the mesh's space flag (type 12); a tile under water becomes type 1, or 10 when fewer than 3 of
    // its corners are above water
    virtual void SetTerrainType(long inTerrainType, const EETileLocation& inLocation, RTSMap* inMap);
    virtual RTSTileUnitNode* AddUnit(EEUnit* inUnit, RTSTileUnitNode* inNext);   // +0x18 0x1211f4
    virtual void FreeUnitNode(RTSTileUnitNode* inNode);  // +0x1c 0x12122a
    void AddDoppelganger(EEDoppelganger* inDoppelganger);  // 0x1211e4 (mU18 takes it; EEDoppelganger::RepairPointers)
    void RemoveDoppelganger(EEDoppelganger* inDoppelganger);  // 0x12133c (mU18 drops it; EEDoppelgangerManager) (provisional name)
    // the node pool (EETile.cpp's UMemoryPool<EETUnitList> 0x5297e0): its stats and release (0x121212, the shutdown
    // 0x1442e0) and its size (0x1216f3, the start-up 0x14318c: 500, 100) (provisional names)
    static void ReleaseUnitListPool();
    static void SetUnitListPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks);

    // inline (COMDAT 0x42e2: reads inPlayer's id and +0x465; body not reconstructed yet, so declared out of line
    // here; EECalamityClass::CanCastAt calls it)
    bool IsVisibleTo(EEPlayer* inPlayer);

    // inline (out-of-line COMDAT 0x431c, 22 callers) (provisional name; EEDoppelgangerManager)
    unsigned long GetNumDoppelgangers() const { return this->mDoppelgangers.size(); }
    EEDoppelganger* GetDoppelganger(unsigned long inIndex) { return this->mDoppelgangers[inIndex]; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    // COMDAT 0x49b21 (IsVisibleTo's twin: the visibility count above 1, or inPlayer's +0x465 flag); EEEffect3D::
    // Virtual54 calls it; declared out of line here (provisional name)
    bool IsSeenBy(EEPlayer* inPlayer);
    std::vector<EEDoppelganger*> mDoppelgangers;  // +0x18 the doppelgangers seen on the tile (EEDoppelgangerManager 0x202222)
};
typedef EETile EEMapTile;

// C:\EEX\RTS Engine\Map\RTSMap.h (RECONSTRUCTION; file name inferred). The engine-side
// map (RTTI RTSMap > FSRestorable; vtable 0x441d94; RTSMap.cpp 0x121c78..0x1240b3: the three key chains, ctor 0x121d41,
// its assert names mCriticalSectionForColoredRectangles). sizeof 0x195624. Names are provisional unless stated.
class GE3DLine;
class GEColor;
template <class T> class U2DPoint;
template <class T> class U2DRectangle;

// An element of RTSMap::mColoredRectangles (0x24 bytes: `_Buy` 0xdf542 multiplies by 0x24; trivially destructible:
// the dtor's `_Tidy` is ICF-folded 0xa9c2c). Layout unknown (provisional name and members)
// An element of RTSMap::mColoredRectangles (0x24 bytes): EEROTerrain::RenderScene 0x1131a4 draws each with
// DrawTerrainHighlight(..., mColor, mOpacity) and erases the zero-opacity ones; the vector's erase 0x1141c5 keeps the
// dead `_Destroy` stores of an element with an empty inline dtor (GEColor's). Names are provisional.
struct RTSColoredRectangle {
    U2DRectangle<long> mRect;               // +0x00
    GEColor mColor;                         // +0x10
    float mOpacity;                         // +0x1c (0: erase)
    long mL20;                              // +0x20
};

// An element of RTSMap::mVertexColors (0x14 bytes): a rectangle of colour-table indices for the terrain vertices,
// x in [mLeft, mRight), y in [mBottom, mTop), row-major (0x122b06 applies and clears them; the element's dtor is
// `delete[] mIndices`: _Destroy 0x124064). The copy 0x6c5ce (top, left, bottom, right, indices) is out of line.
// Names are provisional (the offsets are U2DRectangle<long>'s)
struct RTSMapVertexColors {
    ~RTSMapVertexColors() { delete[] this->mIndices; }
    // out of line (0x6c5ce, in an earlier unit's range: copies top, left, bottom, right, then the indices); the
    // vector's copy 0x124040 calls it (provisional name)
    RTSMapVertexColors& operator=(const RTSMapVertexColors& inOther)
    {
        this->mTop = inOther.mTop;
        this->mLeft = inOther.mLeft;
        this->mBottom = inOther.mBottom;
        this->mRight = inOther.mRight;
        this->mIndices = inOther.mIndices;
        return *this;
    }

    long mBottom;                           // +0x00 first y
    long mLeft;                             // +0x04 first x
    long mRight;                            // +0x08 end x
    long mTop;                              // +0x0c end y
    unsigned char* mIndices;                // +0x10
};

// An element of RTSMap::mU1955DC (0x10 bytes): a straight run of tiles from mStart to mEnd (0x123e36 splits a span at
// continent changes). Its two-point ctor is out of line (0xa016f, in the AI range, first used by 0x9d9e9; RTSMap.cpp
// calls it); trivially destructible (the map's dtor `_Tidy` is ICF-folded 0xa9c2c). Name is provisional.
struct RTSMapTileRun {
    RTSMapTileRun(const U2DPoint<long>& inStart, const U2DPoint<long>& inEnd);
    // out of line (COMDAT 0x9fec3 among EECPWallManager.cpp's, called by its find 0x9fe9d; the four fields compared in
    // order) (provisional name)
    bool operator==(const RTSMapTileRun& inOther) const;

    U2DPoint<long> mStart;                  // +0x00
    U2DPoint<long> mEnd;                    // +0x08
};

class RTSMap : public FSRestorable {
public:
    RTSMap(float inF1955F4);                // 0x121d41 (EEMap's ctors push a float constant; the vectors' allocator
                                            // byte is read from the parameter's high byte)
    // +0x00: an out-of-line empty override (vtable 0x441d94 slot 0 = the ICF-folded `ret 4` 0x557269; EEMap::GetChecksum
    // 0x51c531 calls it, not FSRestorable's inline body)
    virtual void GetChecksum(ESChecksum& ioChecksum) const;
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x123259
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x123807
    virtual ~RTSMap();                      // +0x10 (??_G 0x121e20; RTSWorld's dtor deletes the map through it)
    // the pure slots: signatures from RTSMap.cpp's calls and EEMap's overrides' `ret N` (provisional names)
    virtual void UpdateWater(long inX, long inY) = 0;  // +0x14 (a vertex height changed: 0x122761, 0x123389)
    virtual EEMapTile* CreateTile(float inHeight) = 0;  // +0x18 (a new tile: 0x1229b3)
    virtual void ReserveTiles(unsigned long inCount) = 0;  // +0x1c (before the tiles are made: 0x1229b3)
    virtual void PlaceUnitOnGround(EEUnit* inUnit) = 0;  // +0x20 (each unit on a changed tile: 0x1225fa)
    virtual EEMapTile* LoadTile(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                                 long inX, long inY) = 0;   // +0x24 (loads one tile: Load 0x123259)

    // 0x24baf6 (thiscall; out of line right after its first user 0x24ba75, EEFOFindNewTargetNearOldTarget.cpp's range):
    // the same test for a tile (provisional name)
    bool IsEdgeTile(const EETileLocation& inTile);
    // inline (the volcano's Virtual4C 0xb523e; bool return: push 1; pop edx; fourth test reuses +0x195610)
    bool IsEdgeTile(long inX, long inY) const
        { return inX + inY == this->mL195608 || inY - inX == this->mL195610 || inX + inY == this->mL19560C || inX - inY == this->mL195610; }
    // inline: the ctor copies mF1955F4 to gMapTileSize through the FPU (fld/fstp: an accessor value) (provisional name)
    float GetF1955F4() const { return this->mF1955F4; }
    // inline (RTSMap.cpp expands it: 0x1225fa, 0x122c9b, ...; the out-of-line COMDAT is 0x2f85, EEAMiniMap.cpp): 0
    // off the map
    EEMapTile* GetTile(unsigned long inX, unsigned long inY)
    {
        return inX < this->mSize && inY < this->mSize ? this->mTiles[inY * this->mSize + inX] : 0;
    }

    // RTSMap.cpp's non-virtual members in address order (provisional names unless stated)
    long SpreadDown(long inX, long inY, long inEnd, float inHeight, float inSlope);  // 0x121e7d (raise-terrain edges)
    long SpreadUp(long inX, long inY, long inEnd, float inHeight, float inSlope);   // 0x121fa8
    long SpreadLeft(long inX, long inY, long inEnd, float inHeight, float inSlope);  // 0x1220d6
    long SpreadRight(long inX, long inY, long inEnd, float inHeight, float inSlope);  // 0x1221e6
    long GetNumRaisedCorners(unsigned long inX, unsigned long inY);   // 0x1222ed (EEPlanWall; built without /Og)
    void ClearU1955DC();                    // 0x1225ef (EESelectedGroup::ShowSelectionAreas)
    void UpdateTiles(long inX, long inY);   // 0x1225fa (the four tiles around vertex (inX, inY))
    long SmoothVertex(unsigned long inX, unsigned long inY, unsigned long inFromX, unsigned long inFromY,
                      float inSlope);       // 0x122761
    void Create(long inWidth, long inHeight, unsigned long inPlayerCount, bool inCreateTiles);   // 0x1229b3
    void ApplyVertexColors();               // 0x122b06
    bool IsTileFlat(unsigned long inX, unsigned long inY);   // 0x122ce1
    EEMapTile* GetTileOnLine(const GE3DLine& inLine, long& outX, long& outY, bool inHitWaterSurface);  // 0x122ee8 (EEMap::PickScreenTile)
    EEMapTile* GetLocationOnLine(const GE3DLine& inLine, GE3DPoint& outLocation, bool inHitWaterSurface);  // 0x12307d (EEMap::GetLocationAt)
    const GEColor& GetVertexColor(long inX, long inY, long inCorner);   // 0x12320d
    void Raise(long* ioLeft, long* ioRight, long* ioTop, long* ioBottom, float inHeight, float inSlope,
               float inEdgeHeight, float inEdgeSlope, bool inRaise);  // 0x123389 (x in [left, right + 1], y in [bottom, top + 1])
    void DeleteTiles();                     // 0x1236e6
    void FogSeen(EEPlayer* inPlayer);       // 0x1236f3 (seen tiles become explored)
    void ExploreUnseen(EEPlayer* inPlayer);  // 0x12377f (unseen tiles become explored)
    void BuildAreaOutlines(std::vector<U2DRectangle<long> >& inAreas, std::vector<unsigned short>& inContinents);  // 0x1238bd
    // inline (refused in EEROTerrain::RenderScene's erase; its copy is ICF-folded with the `lea eax, [ecx+4]` 0x1d99fb;
    // provisional name)
    std::vector<RTSColoredRectangle>& GetColoredRectangles() { return this->mColoredRectangles; }
    void AddOutlineEdge(U2DPoint<long> inStart, U2DPoint<long> inEnd, unsigned short inContinent);  // 0x123e36

    static const UString sMapWidth;         // "RTSMap-MapWidth" (0x529840; key names are provisional)
    static const UString sMapHeight;        // "RTSMap-MapHeight" (0x529828)
    static const UString sVertexColorIndex; // "RTSMap-VertexColorIndex" (0x529818)

    std::vector<RTSColoredRectangle> mColoredRectangles;   // +0x04 (0x24-byte elements; provisional name)
    TSCriticalSection* mCriticalSectionForColoredRectangles;   // +0x14 (named by the ctor's assert)
    long mL18;                              // +0x18 (untouched by the ctor)
    GETerrainMesh& GetTerrainMesh() { return this->mTerrainMesh; }  // inline (EEAMouseClick::Execute 0x403350: the original refuses its early message ctors)
    GETerrainMesh mTerrainMesh;             // +0x1c (GetMeshZ)
    std::vector<RTSMapVertexColors> mVertexColors;   // +0x1955cc (0x14-byte elements: `_Buy` 0x6ba94)
    // +0x1955dc (0x10-byte elements: `_Buy` 0x88ba6; 0x123e36 pushes runs built by 0xa016f)
    std::vector<RTSMapTileRun> mU1955DC;
    long mL1955EC;                          // +0x1955ec
    EEMapTile** mTiles;                     // +0x1955f0 row-major, mSize x mSize
    float mF1955F4;                         // +0x1955f4 (the ctor's argument; copied to gMapTileSize)
    float mF1955F8;                         // +0x1955f8 (84 after Create)
    float mF1955FC;                         // +0x1955fc (42 after Create)
    long mL195600;                          // +0x195600
    long mL195604;                          // +0x195604
    long GetL195608() const { return this->mL195608; }
    long mL195608;                          // +0x195608
    long GetL19560C() const { return this->mL19560C; }
    long mL19560C;                          // +0x19560c
    long GetL195610() const { return this->mL195610; }
    long mL195610;                          // +0x195610 (saved: Create's height)
    long mL195614;                          // +0x195614 (saved: Create's width)
    long GetSize() const { return this->mSize; }
    long mSize;                             // +0x195618 tiles per side (signed: the loops compare it with jl)
    unsigned long mU19561C;                 // +0x19561c (saved: the player count)
    bool mB195620;                          // +0x195620 (true once the tiles exist)
};

// 0x52988c: set from RTSMap::mF1955F4 by RTSMap's ctor, read by EETile::Virtual14 0x1214bf (in RTSTile.cpp's .bss
// range, after its three keys). An extern here (a static data member would cost every includer a `$E` number);
// (provisional name and owner)
extern float gMapTileSize;

// The game map: RTTI EEMap > RTSMap, with an FSPersistent base at +0x195624 (secondary vtable 0x441c9c, tag 4).
// Ctors 0x11b844 / 0x11b8d9 (both out of line), sizeof 0x195664 (new sites).
template <class T> class RTSRegionTable;   // ee/RTSRegionTable.h
class EEComplexUnit;
class EEEffect;
class EEMap : public RTSMap, public FSPersistent {
public:
    EEMap();                                // 0x11b844 (EEPTPRestorable<EEMap>::CreateDefault)
    // 0x11b8d9: a new map of inWidth x inHeight for inPlayerCount players in inTerrain (provisional name)
    EEMap(long inWidth, long inHeight, unsigned long inPlayerCount, long inTerrain);
    // the overrides of both bases' FSRestorable slots (vtable 0x441cbc: +0x00 0x11c531, +0x04 0x11ca0d, +0x0c 0x11df3f;
    // the persistent templates call them through EEMap*)
    virtual void GetChecksum(ESChecksum& ioChecksum) const;
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    virtual ~EEMap();                       // +0x10 (??_G 0x11b8bd)
    // RTSMap's pure slots (vtable 0x441cbc +0x14..+0x24: 0x11bdaa, 0x11baa0, 0x11bae6, 0x11bd55, 0x11ceb0)
    virtual void UpdateWater(long inX, long inY);
    virtual EEMapTile* CreateTile(float inHeight);
    virtual void ReserveTiles(unsigned long inCount);
    virtual void PlaceUnitOnGround(EEUnit* inUnit);
    virtual EEMapTile* LoadTile(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                                 long inX, long inY);
    virtual FSPersistent* CreateDefault() const;   // the FSPersistent base's +0x14 (secondary vtable 0x441c9c)
    // inline over RTSMap's inline GetTile (the out-of-line COMDAT 0x2f85 from ~190 sites; small functions expand both
    // levels)
    EEMapTile* GetTile(unsigned long inX, unsigned long inY) { return RTSMap::GetTile(inX, inY); }
    // inline: the ground height under inPoint (EEMAttackGround reloads the point's y through the reference)
    float GetHeight(const GE3DPoint& inPoint) { return this->mTerrainMesh.GetMeshZ(inPoint.GetX(), inPoint.GetY()); }
    // 0x11c5fb: the centre of tile (inX, inY) on the ground; false off the map (the cast-calamity goal) (provisional
    // name)
    bool GetTileLocation(long inX, long inY, GE3DPoint& outLocation);
    // 0xc0894 (out of line, first emitted in the AI range; EEUCTorpedo::CheckGroundImpact): inPoint's x/y with the height of
    // the ground at its tile (provisional name)
    void GetGroundPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint);
    // 0x11c53d: the tile under the screen point inPoint (provisional name; aircraft hot key)
    EETile* GetTileAt(const GE2DPoint& inPoint, long& outX, long& outY);
    // 0x11c96d: the map location under the screen point inPoint; 0 when there is none (provisional name and result)
    EETile* GetLocationAt(const GE2DPoint& inPoint, GE3DPoint& outLocation, bool inHitWaterSurface);
    // 0x11c934 (ret 0x10): the tile under the screen point inPoint through the pick line (GetTileOnLine)
    // (provisional name)
    EETile* PickScreenTile(const GE2DPoint& inPoint, long& outX, long& outY, bool inHitWaterSurface);
    // 0x11c665 (ret 8): the pick line through the screen point inPoint (EESETSArea 0x17d0eb solves it for the
    // tile height; provisional name)
    void GetScreenLine(const GE2DPoint& inPoint, GE3DLine& outLine);
    // 0x11c75a (ret 4): a float from the ground under inLocation (the slope: EEPhysicsModel::SpeedMode /
    // EEPMSeaAnimal::SpeedMode scale it by 1.909859 = 6/pi) (provisional name)
    float GetTerrainPitch(const GETransformation& inLocation);
    float GetTerrainRoll(const GETransformation& inLocation);  // 0x11c847 (EEComplexUnit::PlaceOnGround: the roll, with mRollsWithTerrain) (provisional name)
    // 0xc5bb: the tile at inLocation (EEPlayer::RemoveSharedSightAt/RemoveOwnSightAt; provisional name)
    EETile* GetTileAtLocation(const EETileLocation& inLocation);
    // inline: the highest / lowest corner of tile (inX, inY), -1 off the map (COMDATs 0x212753 / 0x2128e1 after their
    // first user EEUCGate.cpp's 0x211d00) (provisional names)
    float GetMaximumCornerZ(unsigned long inX, unsigned long inY)
    {
        // the nested max (windows.h macro: re-evaluated arms) and RTSMap::GetTile expanded in place, -1 loaded
        // directly (EEUCGate 0x612753)
        if (RTSMap::GetTile(inX, inY) != NULL)
            return max(this->mTerrainMesh.GetZ(inX, inY), max(this->mTerrainMesh.GetZ(inX + 1, inY),
                            max(this->mTerrainMesh.GetZ(inX, inY + 1), this->mTerrainMesh.GetZ(inX + 1, inY + 1))));
        return -1.0f;
    }
    float GetMinimumCornerZ(unsigned long inX, unsigned long inY)
    {
        // as GetMaximumCornerZ (EEUCGate 0x6128e1)
        if (RTSMap::GetTile(inX, inY) != NULL)
            return min(this->mTerrainMesh.GetZ(inX, inY), min(this->mTerrainMesh.GetZ(inX + 1, inY),
                            min(this->mTerrainMesh.GetZ(inX, inY + 1), this->mTerrainMesh.GetZ(inX + 1, inY + 1))));
        return -1.0f;
    }
    // defined in RTSMap.cpp's range, so presumably RTSMap's in the original; kept EEMap's here because names.txt and
    // their callers bind the EEMap names (provisional names)
    void PrepareSave();                     // 0x122517 (EESEASaveScenario before saving a scenario)
    // 0x122c9b: the nearest tile on the map to inPoint; its coordinates in ioX/ioY (EEMPathTo; provisional name)
    EEMapTile* GetClosestTile(GE3DPoint* inPoint, long* ioX, long* ioY);
    bool IsAreaOnMap(unsigned long inX, unsigned long inY, unsigned long inRadius);   // 0x122dec
    // EEMap.cpp members called by the random map (EERandomMap.cpp; provisional names)
    void PaintTerrain(long inX, long inY, long inTerrain);  // 0x11d34b (the tile's terrain texture)
    void UpdateCliffs(long inLeft, long inRight, long inBottom, long inTop);  // 0x11d5e8
    void SetCliffTexture(long inCliffTexture);  // 0x11df99 (+0x19565c = it, then UpdateCliffs over the whole map)
    unsigned long GetTileCount();           // 0x11e12f
    long ComputeChecksum();                 // 0x11c3ca (EEPhysics::GetOutOfSynchChecksum: the out-of-synch checksum) (provisional name)
    // EEMap.cpp's other members in address order (provisional names)
    void SetPlayerCount(unsigned long inPlayerCount);  // 0x11bc03 (the visibility words for a new player count)
    void CreateMap(long inWidth, long inHeight, unsigned long inPlayerCount);  // 0x11bf4a (Create + region tables + colours)
    // 0x11c13f / 0x11d006: inPlayer stops / starts seeing the tiles within inRadius of (inX, inY)
    void RemoveSight(EEPlayer* inPlayer, float inX, float inY, float inRadius, bool inShared);
    void Initialize();                      // 0x11c9a3 (the ctors' common part)
    void AddSight(EEPlayer* inPlayer, float inX, float inY, float inRadius, bool inShared);
    void PaintTerrainRect(long inLeft, long inRight, long inTop, long inBottom, long inTerrain, long inCliff);  // 0x11d2a2
    void SetCliffColorSets(long inLeft, long inRight, long inTop, long inBottom, long inCliff);  // 0x11d433 (cliff colours)
    void RemoveFlatCliffs(unsigned long inLeft, unsigned long inRight, unsigned long inBottom, unsigned long inTop);  // 0x11dbf1
    void RegisterTerrainSets();             // 0x11dd95 (registers the terrain texture and colour sets with the mesh)
    void SetTileTerrain(long inX, long inY, long inTextureSet, bool inUnload);  // 0x11dfb7 (the tile's texture set)

    virtual void DumpOutOfSynch(FILE* inFile);  // +0x28 0x11cf60 (ret 4: the out-of-synch dump's file, EEPhysics 0x152311) (provisional name)

    URandom mRandom;                        // +0x19562c (seed 1; provisional name)
    RTSRegionTable<EEEffect*>* mEffectRegions;   // +0x19563c (the effects per region: EEEffect 0x102e61 etc.; +0x19563c..+0x195660 zeroed by 0x11c9a3) (provisional name)
    RTSRegionTable<EEComplexUnit*>* mUnitRegions;   // +0x195640 (ctor 0x11e05e; the units per region; EEAIArea 0xdcbe/0xde7f) (provisional name)
    EETile* mTileArray;                     // +0x195644 (new EETile[n]: Virtual1C; Virtual18/24 hand them out) (provisional name)
    uint32_t* mVisibilityWords;             // +0x195648 (the tiles' visibility words, (players + 1) / 2 per tile) (provisional name)
    unsigned long GetMapCharacteristics() const { return this->mMapCharacteristics; }
    unsigned long mMapCharacteristics;      // +0x19564c (saved)
    unsigned long mU195650;                 // +0x195650 (saved; a ctor result)
    unsigned long mNextTile;                // +0x195654 (the next free entry of mTileArray) (provisional name)
    unsigned long mTileArrayCount;          // +0x195658 (provisional name)
    unsigned long mU19565C;                 // +0x19565c (saved; the second ctor stores 0x11bf4a's result)
    unsigned long mChecksumCursor;          // +0x195660 (ComputeChecksum checks 64 x 64 tiles from here) (provisional name)
};

// RTSTile::SetVisibility: inline (the out-of-line COMDAT 0x124072 follows its first user in RTSMap.cpp, PrepareSave
// 0x122517). Its body needs the player's id, so it is defined only where ee/EEPlayer.h came first (RTSMap.cpp,
// RTSTile.cpp); the real RTSTile.h presumably included the player header. Placement inferred.
#ifdef EE_EEUNIT_H
inline RTSTUnitList::~RTSTUnitList()
{
    if (this->mUnit)
        this->mUnit->Dereference();
}
#endif

#ifdef EE_EEPLAYER_H
inline void RTSTile::SetVisibility(unsigned long inValue, EEPlayer* inPlayer)
{
    unsigned long thePlayerID = inPlayer->GetPlayerID();
    this->mVisibility[thePlayerID >> 1] = (this->mVisibility[thePlayerID >> 1] & (0xffff << (((thePlayerID + 1) & 1) << 4)))
        | (inValue << ((thePlayerID & 1) << 4));
}
#endif

// Tile terrain types (RTSTile::mTerrainType = dbterrain.dat record +0x78, plus the water kinds EETile::SetTerrainType derives from the height)
enum EETerrainType {
    kTerrainDeepWater = 1,    // EETile::SetTerrainType 0x5214bf: a tile below -0.75 elevation unit becomes 1; setrilo ai_astar.h '2 deep water for type 1'
    kTerrainSnow = 5,         // dbterrain.dat type 5 (+0x78) = Ice, Snow, Snow - Tufts/Grass Patches/Dirt Patches, Mars glaciers/polar cap; EEUnitClass 0x638ce7 shows the horse breath effect at night or on type 5 (provisional name)
    kTerrainCliff = 9,        // dbterrain.dat: the records of type 9 (+0x78) are exactly the cliff textures (Cliff, Cliffwinter, CliffRock); EEMap::UpdateCliffs sets 9 (setrilo rts_map.h)
    kTerrainShallowWater = 10,  // EETile::SetTerrainType 0x5214bf: a tile under water with fewer than 3 dry corners becomes 10 (3+ dry corners keep the land type); setrilo ai_continents.h 'a water tile (types 1, 10)', shallows join land continents (provisional name)
    kTerrainSpace = 12,       // dbterrain.dat: 'Space' is the only record of type 12 (+0x78); EETile::SetTerrainType 0x5214bf SetIsSpace(type == 12)
};

#endif
