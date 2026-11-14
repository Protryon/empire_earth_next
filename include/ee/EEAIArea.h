// C:\EEX\Empire Earth\...\EEAIArea.h (RECONSTRUCTION; file path inferred; for the continent cluster). The base of
// every AI map area (RTTI EEAIArea, no bases; vtable 0x439d6c, 11 slots; sizeof 0x68: EEAIHill 0x6c is the smallest
// kind and its own members start at +0x68). Ctor 0x110cb and dtor 0x11123 inline COMDATs (??_G 0x11107 calls ??1). A
// sparse tile set plus its convex hull. Slot signatures from the EEAIArea bodies (ret N, argument reads, the engine
// calls they forward to). Slot and member names are provisional.
// Port: every member but the tile set is the Rust record EeAStarAreaRecord (ee_astar area.rs), the class's private
// first base: the hull (it was a U2DConvexHull<long>: its points, bounds, center and area), the region bounds, the
// serial and the type. The hull's rebuild and UpdateBounds are Rust; the AI reads the record through the accessors
// below (docs/RUST_PLAN.md R5b.1, branch rust-astar-areas1).
// Requires ee/EEAIAStarStorage.h (EEAIAStarBounds), lle/LowLevelEngine.h, lle/U2DSparseArray.h, <map>, <math.h>,
// <algorithm>, lle/U2DPolygon.h (the seam's point type and capture records). Proven: EEAIContinent ctor
// 0x1d846 / dtor 0x1d94d bind_only 100, AddTiles 0xd6e3 exact, EEAIContinentManager::AddArea 0x26f8d bind_only 100.
// Ctor/dtor inline: their COMDATs sit in
// EEAIAStarChokePointGoal.cpp's range (first user EEAIChokePoint's inline ctor); A/B over 49 includers +5 exact, 0 lost.
#ifndef EE_EEAIAREA_H
#define EE_EEAIAREA_H

#include <stddef.h>
#include <vector>
#include "rust/ee_astar.h"         // EeAStarAreaRecord
#include "ee/EEAIAStarStorage.h"     // EEAIAStarBounds (mBounds)
#include "ee/EEAIKindIDs.h"

class EEInfluenceTable;
class EEUnit;

class EEAIArea : private EeAStarAreaRecord {
public:
    EEAIArea() { ee_astar_area_init(this); this->area_type = kAIAreaNone; }  // 0x110cb (inline COMDAT; first user EEAIChokePoint's ctor)
    virtual ~EEAIArea() { ee_astar_area_free(this); }   // +0x00 dtor 0x11123 (inline COMDAT)
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
    bool Contains(const U2DPoint<long>& inPoint) { bool theResult = this->HullContains(inPoint); if (theResult) theResult = this->mTiles.Contains(inPoint); return theResult; }
    float GetHullDistanceSquared(EEAIArea* inOther);   // 0xd81e (the hulls' closest points) (provisional name)
    void UpdateBounds(EEInfluenceTable* inTable);       // 0xd763 (mBounds = the hull's bounds in inTable's cells) (provisional name)
    void GetTiles(std::vector<U2DPoint<long> >& outTiles);   // 0xdaae (every tile of the hull's scan lines) (provisional name)

    // New (port): the members behind accessors (R5b.1 blocker (b): the AI and the units read an area only
    // through these, so the record can live elsewhere)
    // the tile set (U2DSparseArrayPointContainer: ee_astar sparse.rs)
    U2DSparseArrayPointContainer& GetTileSet() { return this->mTiles; }
    const U2DSparseArrayPointContainer& GetTileSet() const { return this->mTiles; }
    long GetNumTiles() const { return this->mTiles.Size(); }
    // the hull (the record's; U2DConvexHull<long>'s calls, HULL capture records and all)
    const U2DPoint<long>& GetHullCenter() const { return reinterpret_cast<const U2DPoint<long>&>(this->hull_center); }
    const U2DRectangle<long>& GetHullBounds() const
        { return reinterpret_cast<const U2DRectangle<long>&>(this->hull_bounds); }
    bool HullContains(const U2DPoint<long>& inPoint);   // BoundedContains: the bounds, then the outline (HULL 6)
    unsigned long GetHullPointCount() const { size_t theCount; ee_astar_area_hull_points(this, &theCount); return (unsigned long)theCount; }
    const U2DPoint<long>* GetHullPointData() const
        { size_t theCount; return reinterpret_cast<const U2DPoint<long>*>(ee_astar_area_hull_points(this, &theCount)); }
    const U2DPoint<long>& GetHullPoint(long inIndex) const { return this->GetHullPointData()[inIndex]; }
    std::vector<U2DPoint<long> > GetHullPoints() const;   // a copy
    U2DPoint<long> GetHullClosestPoint(const U2DPoint<long>& inPoint);   // the nearest vertex (HULL 4)
    U2DPoint<long> GetHullClosestOutlinePoint(const U2DPoint<long>& inPoint);   // (HULL 2)
    void SetHullPoints(const std::vector<U2DPoint<long> >& inPoints);   // SetPoints(points, false) (HULL 1)
    void ClearHull() { ee_astar_area_clear_hull(this); }
    // the hull in a saved game (save 0.263): the points, the bounds, the center and the area, as they are
    template <class W> void WriteHull(W& ioWriter) const
    {
        ioWriter.Points(this->GetHullPoints());
        ioWriter.Rect(this->GetHullBounds());
        ioWriter.Point(this->GetHullCenter());
        ioWriter.Bytes(&this->hull_area, sizeof(this->hull_area));
    }
    template <class R> bool ReadHull(R& ioReader)
    {
        std::vector<U2DPoint<long> > thePoints;
        U2DRectangle<long> theBounds;
        U2DPoint<long> theCenter;
        float theArea = 0;
        if (!ioReader.Points(thePoints) || !ioReader.Rect(theBounds) || !ioReader.Point(theCenter)
            || !ioReader.Bytes(&theArea, sizeof(theArea)))
            return false;
        ee_astar_area_set_hull_raw(this, U2DHullPoints(thePoints), thePoints.size(),
                                   reinterpret_cast<const EeHullRect&>(theBounds), U2DHullPointOf(theCenter), theArea);
        return true;
    }
    // the tiles' bounds in influence-table regions
    U2DRectangle<long>& GetBounds() { return reinterpret_cast<U2DRectangle<long>&>(this->bounds); }
    const U2DRectangle<long>& GetBounds() const { return reinterpret_cast<const U2DRectangle<long>&>(this->bounds); }
    unsigned long GetSerial() const { return this->serial; }
    void SetSerial(unsigned long inSerial) { this->serial = inSerial; }
    long GetAreaType() const { return this->area_type; }
    void SetAreaType(long inType) { this->area_type = inType; }

private:
    EEAIArea(const EEAIArea&);              // the record owns its hull: no copies
    EEAIArea& operator=(const EEAIArea&);
    void CaptureSetPoints(const std::vector<U2DPoint<long> >& inPoints);   // TEST ONLY: HULL 1

    U2DSparseArrayPointContainer mTiles;    // +0x04 (0x1c; imported ctor, inline dtor: vptr + Clear + _Tidy)
};

// the record's rectangles and points are U2DRectangle<long>'s and U2DPoint<long>'s layouts (handed out as them)
static_assert(sizeof(EeHullRect) == sizeof(U2DRectangle<long>) && offsetof(EeHullRect, bottom) == 0
              && offsetof(EeHullRect, left) == sizeof(long) && offsetof(EeHullRect, right) == 2 * sizeof(long)
              && offsetof(EeHullRect, top) == 3 * sizeof(long), "EeHullRect");
static_assert(sizeof(EeHullPoint) == sizeof(U2DPoint<long>) && offsetof(EeHullPoint, y) == sizeof(long), "EeHullPoint");

#endif
