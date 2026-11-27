// Low-Level Engine U2DPoint header, as seen by its includers. Inferred: only what an includer's codegen
// proves. Requires lle/LowLevelEngine.h.
#ifndef LLE_U2DPOINT_H
#define LLE_U2DPOINT_H

// LLE exports use U2DPoint<long> (?$U2DPoint@J@@) and U2DPoint<float>; no member is exported (all inline).
// Default.dll EEClumps keeps a vector<U2DPoint<long> > (8-byte elements, x then y, trivial dtor: 0x8e6d).
#include "rust/base/math.h"   // GetLineTiles, the file forms
#include <type_traits>

template <class T> class U2DPoint {
public:
    U2DPoint() : mX(0), mY(0) {}           // zero-filled: GETerrainMesh.cpp static arrays 0x1002c1ba (and dword, 0),
                                            // AddDenormalizedPointToList 0x1002ab5d (U2DPoint<float>: fldz)
#ifdef LLE_EXE_HEADERS   // the exe's U2DPoint.h takes the coordinates by const reference (AddGraphSeries 0x2764e0
                         // loads the first argument first through both element addresses). The engine and
                         // Default.dll views keep by-value (each loses a row)
    U2DPoint(const T& inX, const T& inY) : mX(inX), mY(inY) {}
#else
    U2DPoint(T inX, T inY) : mX(inX), mY(inY) {}
#endif
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_EXE_HEADERS)
    // exe view too (owner decision, net +32 exact with 18 known regressions: the returned U2DPoint is then built in
    // place, U2DRectangle<T>::GetClosestPoint 0x80347/0x1f056a).
    // Inferred engine-only: GETerrainMesh.cpp's static U2DPoint<long>[2] arrays register an (empty) atexit dtor
    // (0x1002c1d0/0x1002c1dc), so the engine's header declares one; Default.dll's units match only without it
    // (775 -> 772 exact), i.e. the plug-in was built against a header without the dtor.
    ~U2DPoint() {}
#endif
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_EXE_HEADERS)
    // Inferred engine + exe: user-declared copy ctor and operator=. The engine copies U2DPoint elements and by-value
    // arguments field by field through references (GETerrainMesh's sort/heap COMDATs 0x100311c5.., _Adjust_heap
    // 0x1003252e; the out-of-line operator= is 0x1002fe8b, ICF-folded with GE2DPoint's), while Default.dll copies
    // U2DPoint<float> with rep movsd (0x131bd): the plug-ins were probably built against an older U2DPoint.h.
    // The exe calls an out-of-line copy ctor 0x4d28b (479 callers); with the pair in the exe view allscores gained
    // 3 exact, 0 lost (EEAIArea 0xda77, EEUGLoadGarrison 0x226ba4, EEUGUnloadTransport 0x228883).
    // (The engine's empty dtor is in the exe view too: EEAIArea 0xe98e is one of its known regressions.)
#endif
    // Engine template members (instantiated by GETerrainMesh.cpp's U2DPolygon<float> code). (provisional names)
    bool operator<(const U2DPoint& inPoint) const   // 0x10031459: lower y first, then larger x (through the accessors:
                                                    // refused inline at direct sites, fld;fld;fcompp loads)
        { return GetY() < inPoint.GetY() || (GetY() == inPoint.GetY() && GetX() > inPoint.GetX()); }
    bool operator==(const U2DPoint& inPoint) const { return this->mX == inPoint.mX && this->mY == inPoint.mY; }
    bool operator!=(const U2DPoint& inPoint) const { return this->mX != inPoint.mX || this->mY != inPoint.mY; }
    T GetX() const { return this->mX; }
    T GetY() const { return this->mY; }
    void SetX(T inX) { this->mX = inX; }
    void SetY(T inY) { this->mY = inY; }
#ifdef LLE_EXE_HEADERS
    // exe: the squared distance (out-of-line COMDAT ICF-identical to EETileDistanceLess::DistanceSquared 0x8a3f; the no-/Og
    // header sort functor 0x408fd7 calls it on GetTile()'s result). Inline: VC6's /Os post-check undoes it at every
    // site; at a no-/Og site its locals and the getter temps keep 4 caller frame slots (r37_load family note)
    float DistanceToSquaredF(const U2DPoint& inPoint) const
    {
        return (float)((this->mX - inPoint.GetX()) * (this->mX - inPoint.GetX())) +
               (this->mY - inPoint.GetY()) * (this->mY - inPoint.GetY());
    }
#endif
#ifdef LLE_EXE_HEADERS
    // exe view only: the tiles a line from this tile to inTo crosses, one step of inStep tiles at a time, inTo last
    // (U2DPoint<long> only: ee_base math u2d.rs)
    void GetLineTiles(const U2DPoint& inTo, float inStep, std::vector<U2DPoint>& outTiles) const
        { ee_u2d_line_tiles(reinterpret_cast<const long(*)[2]>(&this->mX), reinterpret_cast<const long(*)[2]>(&inTo.mX), inStep, &outTiles, AssignTiles); }
    // GetLineTiles' vector: the Rust tiles replace its contents
    static void AssignTiles(void* outTiles, const long (*inTiles)[2], size_t inCount)
        { static_cast<std::vector<U2DPoint>*>(outTiles)->assign(reinterpret_cast<const U2DPoint*>(inTiles), reinterpret_cast<const U2DPoint*>(inTiles) + inCount); }
#endif
    // each difference written out twice (a squaring macro's shape): the one form that gives both the float copy
    // (GETerrainMesh.cpp 0x1002fe6c: y difference first, the squares from st0/st2) and the long copy
    // (U2DSparseArray.cpp 0x10055899: x and y kept in eax/edx, inPoint in esi)
    T DistanceToSquared(const U2DPoint& inPoint) const
        { return (this->mX - inPoint.GetX()) * (this->mX - inPoint.GetX()) + (this->mY - inPoint.GetY()) * (this->mY - inPoint.GetY()); }
    // x and y from (to) an open file, each through its typed call; kErrorNotOpen when it is not open, else the first
    // error (ee_base math u2d.rs; T is long or float)
    long Load(FSFile& inFile) { return ee_u2d_point_io(&inFile, &this->mX, std::is_floating_point<T>::value, false); }
    long Save(FSFile& inFile) const { return ee_u2d_point_io(&inFile, const_cast<T*>(&this->mX), std::is_floating_point<T>::value, true); }

    T mX;                                   // +0x00
    T mY;                                   // +0x04
};

#endif
