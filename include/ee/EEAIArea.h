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
#include "rust/sim/astar.h"         // EeAStarAreaRecord
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIListView.h"

class EEInfluenceTable;
class EEUnit;

class EEAIArea : private EeAStarAreaRecord {
public:
    // the towns' and herds' base (their objects are C++'s); the continent manager's kinds are Rust objects
    // (ee_sim::astar objects.rs) that no C++ constructor makes: the class has no vtable, its slots dispatch by kind
    EEAIArea() { ee_astar_area_init(this); this->area_type = kAIAreaNone; }  // 0x110cb (inline COMDAT; first user EEAIChokePoint's ctor)
    ~EEAIArea() { ee_astar_area_free(this); }   // +0x00 dtor 0x11123 (inline COMDAT)
    // The bodies are ee_sim::astar area_ops.rs (the hull's queries and rebuilds with their TEST ONLY HULL and AREA
    // records) since the game's leaves round 2; GetRandomPoint and UpdateHull read the physics' random stream and the
    // world's influence table (EEAIRectangleArea.cpp).
    // +0x04 0xd6e3 (ret 8): the tiles added; the hull rebuilt (UpdateHull) if inUpdate
    void AddTiles(const std::vector<U2DPoint<long> >& inTiles, bool inUpdate)
    {
        this->mTiles.Insert(inTiles);
        if (inUpdate)
            this->UpdateHull();
    }
    // +0x08 0xd705 (ret 4): the area made of the tiles; its hull and influence-region bounds built (a herd: its own)
    void Create(const std::vector<U2DPoint<long> >& inTiles);
    void AreaCreate(const std::vector<U2DPoint<long> >& inTiles)
    {
        this->mTiles.Create(inTiles);
        this->UpdateHull();
    }
    // +0x14 0xd8e6 (ret 8, hidden result): the area tile closest to inPoint
    U2DPoint<long> GetClosestPoint(const U2DPoint<long>& inPoint) { return this->mTiles.GetClosestPoint(inPoint); }
    // +0x18 0xd8a5 (ret 8, hidden result): inPoint if the hull contains it, else the hull outline's closest point
    U2DPoint<long> GetClosestHullPoint(const U2DPoint<long>& inPoint)
        { return EEAIArea::PointOf(ee_astar_area_closest_hull_point(this, EEAIArea::HullPoint(inPoint))); }
    // +0x1c 0xd8fe (ret 0xc): this area's tile and inOther's hull outline point closest to each other; false (both
    // (0, 0)) when either area is empty (a continent: its own)
    bool GetClosestPoints(EEAIArea* inOther, U2DPoint<long>& outThis, U2DPoint<long>& outOther);
    bool AreaClosestPoints(EEAIArea* inOther, U2DPoint<long>& outThis, U2DPoint<long>& outOther)
    {
        return ee_astar_area_closest_points(this, this->mTiles.GetHandle(), inOther, inOther->mTiles.GetHandle(),
                                            reinterpret_cast<EeHullPoint*>(&outThis), reinterpret_cast<EeHullPoint*>(&outOther));
    }
    // +0x20 0xda77 (ret 4, hidden result): a random tile from the game's random stream, (0, 0) when empty (a town,
    // herd or rectangle area: its own)
    U2DPoint<long> GetRandomPoint();
    U2DPoint<long> AreaRandomPoint();
    void UpdateHull();                      // +0x24 0xdb32 (the hull from the tiles' extreme edge points, the bounds)
    // +0x28 0xdb7f (ret 4): the tile removed and, on success, the hull rebuilt; whether it was removed (a forest:
    // its own)
    bool RemoveTile(const U2DPoint<long>& inPoint);
    bool AreaRemoveTile(const U2DPoint<long>& inPoint)
        { return ee_astar_area_remove_tile_captured(this, this->mTiles.GetHandle(), EEAIArea::HullPoint(inPoint)); }

    // inline COMDAT 0xc6fd (emitted at its first user in EECPASGU*'s range): the hull (bounds first), then the tiles
    // Provisional name
    bool Contains(const U2DPoint<long>& inPoint) { bool theResult = this->HullContains(inPoint); if (theResult) theResult = this->mTiles.Contains(inPoint); return theResult; }
    // 0xd81e: the squared distance of the hulls' closest points (provisional name)
    float GetHullDistanceSquared(EEAIArea* inOther) { return ee_astar_area_hull_distance_squared(this, inOther); }
    void UpdateBounds(EEInfluenceTable* inTable);   // 0xd763: the hull's bounds in the table's regions (EEAIRectangleArea.cpp)
    // 0xdaae: appends every tile of the hull's scan lines (provisional name)
    void GetTiles(std::vector<U2DPoint<long> >& outTiles)
    {
        ee_astar_area_get_tiles(this, outTiles.size(), &outTiles, [](void* inTiles, long inX, long inY) {
            static_cast<std::vector<U2DPoint<long> >*>(inTiles)->push_back(U2DPoint<long>(inX, inY));
        });
    }

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
    // BoundedContains: the bounds, then the outline (HULL 6)
    bool HullContains(const U2DPoint<long>& inPoint) { return ee_astar_area_hull_contains(this, EEAIArea::HullPoint(inPoint)); }
    unsigned long GetHullPointCount() const { size_t theCount; ee_astar_area_hull_points(this, &theCount); return (unsigned long)theCount; }
    const U2DPoint<long>* GetHullPointData() const
        { size_t theCount; return reinterpret_cast<const U2DPoint<long>*>(ee_astar_area_hull_points(this, &theCount)); }
    const U2DPoint<long>& GetHullPoint(long inIndex) const { return this->GetHullPointData()[inIndex]; }
    std::vector<U2DPoint<long> > GetHullPoints() const   // a copy
        { return std::vector<U2DPoint<long> >(this->GetHullPointData(), this->GetHullPointData() + this->GetHullPointCount()); }
    U2DPoint<long> GetHullClosestOutlinePoint(const U2DPoint<long>& inPoint)   // (HULL 2)
        { return EEAIArea::PointOf(ee_astar_area_hull_closest_outline_point(this, EEAIArea::HullPoint(inPoint))); }
    void SetHullPoints(const std::vector<U2DPoint<long> >& inPoints)   // SetPoints(points, false) (HULL 1)
        { ee_astar_area_set_hull_points(this, inPoints.empty() ? NULL : reinterpret_cast<const EeHullPoint*>(&inPoints[0]), inPoints.size()); }
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
    EeAStarAreaRecord* AreaRecord() { return this; }   // New (port): the record (ee_sim::astar cm_io.rs's layout)
    void SetAreaType(long inType) { this->area_type = inType; }

private:
    static EeHullPoint HullPoint(const U2DPoint<long>& inPoint) { EeHullPoint thePoint = {inPoint.mX, inPoint.mY}; return thePoint; }
    static U2DPoint<long> PointOf(const EeHullPoint& inPoint) { return U2DPoint<long>(inPoint.x, inPoint.y); }
    EEAIArea(const EEAIArea&);              // the record owns its hull: no copies
    EEAIArea& operator=(const EEAIArea&);

    U2DSparseArrayPointContainer mTiles;    // +0x04 (0x1c; imported ctor, inline dtor: vptr + Clear + _Tidy)
};

// the record's rectangles and points are U2DRectangle<long>'s and U2DPoint<long>'s layouts (handed out as them)
static_assert(sizeof(EeHullRect) == sizeof(U2DRectangle<long>) && offsetof(EeHullRect, bottom) == 0
              && offsetof(EeHullRect, left) == sizeof(long) && offsetof(EeHullRect, right) == 2 * sizeof(long)
              && offsetof(EeHullRect, top) == 3 * sizeof(long), "EeHullRect");
static_assert(sizeof(EeHullPoint) == sizeof(U2DPoint<long>) && offsetof(EeHullPoint, y) == sizeof(long), "EeHullPoint");

#endif
