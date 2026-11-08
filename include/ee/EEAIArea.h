// C:\EEX\Empire Earth\...\EEAIArea.h (RECONSTRUCTION; file path inferred; for the continent cluster). The base of
// every AI map area (RTTI EEAIArea, no bases; vtable 0x439d6c, 11 slots; sizeof 0x68: EEAIHill 0x6c is the smallest
// kind and its own members start at +0x68). Ctor 0x110cb and dtor 0x11123 inline COMDATs (??_G 0x11107 calls ??1). A
// sparse tile set plus its convex hull. Slot signatures from the EEAIArea bodies (ret N, argument reads, the engine
// calls they forward to). Slot and member names are provisional.
// Requires ee/EEAIAStarStorage.h (EEAIAStarBounds), lle/LowLevelEngine.h, lle/U2DSparseArray.h, <map>, <math.h>, <algorithm>, lle/U2DPolygon.h with the
// U2DPolygon/U2DConvexHull templates (visible in the exe view). Proven: EEAIContinent ctor
// 0x1d846 / dtor 0x1d94d bind_only 100, AddTiles 0xd6e3 exact, EEAIContinentManager::AddArea 0x26f8d bind_only 100.
// Ctor/dtor inline: their COMDATs sit in
// EEAIAStarChokePointGoal.cpp's range (first user EEAIChokePoint's inline ctor); A/B over 49 includers +5 exact, 0 lost.
#ifndef EE_EEAIAREA_H
#define EE_EEAIAREA_H

#include <vector>
#include "ee/EEAIAStarStorage.h"     // EEAIAStarBounds (mBounds)
#include "ee/EEAIKindIDs.h"

class EEInfluenceTable;
class EEUnit;

class EEAIArea {
public:
    EEAIArea() : mSerial(0), mAreaType(kAIAreaNone) {}  // 0x110cb (inline COMDAT; first user EEAIChokePoint's ctor)
    virtual ~EEAIArea() {}                  // +0x00 dtor 0x11123 (inline COMDAT)
    // +0x04 0xd6e3 (ret 8): mTiles.Insert(inTiles); then Virtual24() if inUpdate
    virtual void AddTiles(const std::vector<U2DPoint<long> >& inTiles, bool inUpdate);
    // +0x08 0xd705 (ret 4): mTiles.Create(inTiles); the hull from the extreme edge points; 0xd763(DAT_531860)
    virtual void Create(const std::vector<U2DPoint<long> >& inTiles);
    // +0x0c 0xd859 (ret 4; tiles present: the units on the hull's bounds that Contains() holds, cdecl template
    // 0xdcbe(&mHull bounds, outUnits, &predicate, true)); parameter type from that template's push_back
    virtual void GetUnitsOnTiles(std::vector<EEUnit*>& outUnits);
    // +0x10 0xd87f: the same with 0xde7f (the predicate tests the hull only)
    virtual void GetUnitsInHull(std::vector<EEUnit*>& outUnits);
    // +0x14 0xd8e6 (ret 8, hidden result): mTiles.GetClosestPoint(inPoint)
    virtual U2DPoint<long> GetClosestPoint(const U2DPoint<long>& inPoint);
    // +0x18 0xd8a5 (ret 8, hidden result): inPoint if the hull contains it (0xd16e), else the hull's slot +0x14
    virtual U2DPoint<long> GetClosestHullPoint(const U2DPoint<long>& inPoint);
    // +0x1c 0xd8fe (ret 0xc): the closest pair of points of this and inOther (both cleared first); EEAIContinent 0x1da61
    virtual bool GetClosestPoints(EEAIArea* inOther, U2DPoint<long>& outThis, U2DPoint<long>& outOther);
    // +0x20 0xda77 (ret 4, hidden result): mTiles.GetRandomPoint(gPhysics +0x18), (0, 0) when empty
    virtual U2DPoint<long> GetRandomPoint();
    virtual void UpdateHull();              // +0x24 0xdb32 (the hull from mTiles' extreme edge points; 0xd763)
    // +0x28 0xdb7f (ret 4): mTiles.Remove(inPoint); on success the hull is rebuilt; returns whether it was removed
    virtual bool RemoveTile(const U2DPoint<long>& inPoint);

    // inline COMDAT 0xc6fd (emitted at its first user in EECPASGU*'s range): the hull (bounds first), then the tiles
    // Provisional name
    bool Contains(const U2DPoint<long>& inPoint) { bool theResult = this->mHull.BoundedContains(inPoint); if (theResult) theResult = this->mTiles.Contains(inPoint); return theResult; }
    float GetHullDistanceSquared(EEAIArea* inOther);   // 0xd81e (the hulls' closest points) (provisional name)
    void UpdateBounds(EEInfluenceTable* inTable);       // 0xd763 (mBounds = the hull's bounds in inTable's cells) (provisional name)
    void GetTiles(std::vector<U2DPoint<long> >& outTiles);   // 0xdaae (every tile of the hull's scan lines) (provisional name)

    U2DSparseArrayPointContainer mTiles;    // +0x04 (0x1c; imported ctor, inline dtor: vptr + Clear + _Tidy)
    U2DConvexHull<long> mHull;              // +0x20 (0x30; U2DPolygon<long> ctor 0x11251, then vtable 0x439d9c)
    // +0x50: the tiles' bounds (the ctor's zero stores +0x5c, +0x54, +0x50, +0x58 are U2DRectangle()'s order;
    // EECPIArmy 0x80296 calls U2DRectangle<long>::Contains 0xd190 on this + 0x50, 0x8019d calls 0x123d1 = the shape
    // of U2DRectangle<long>::Extend)
    U2DRectangle<long> mBounds;
    unsigned long GetSerial() const { return this->mSerial; }
    unsigned long mSerial;                  // +0x60 EEAIContinentManager::mLastSerial; the key of its mAreas
    long mAreaType;                         // +0x64 (ctor 0; the continent factory 0x156b2 sets 1)
};

#endif
