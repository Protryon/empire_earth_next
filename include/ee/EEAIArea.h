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
#include "ee/EEAIListView.h"

class EEInfluenceTable;
class EEUnit;

class EEAIArea : private EeAStarAreaRecord {
public:
    // the towns' and herds' base (their objects are C++'s); the continent manager's kinds are Rust objects
    // (ee_sim::astar objects.rs) that no C++ constructor makes: the class has no vtable, its slots dispatch by kind
    EEAIArea() { ee_astar_area_init(this); this->area_type = kAIAreaNone; }  // 0x110cb (inline COMDAT; first user EEAIChokePoint's ctor)
    ~EEAIArea() { ee_astar_area_free(this); }   // +0x00 dtor 0x11123 (inline COMDAT)
    // +0x08 0xd705 (ret 4): the area made of the tiles; its hull and influence-region bounds built (a herd: its own)
    void Create(const std::vector<U2DPoint<long> >& inTiles);
    // +0x14 0xd8e6 (ret 8, hidden result): the area tile closest to inPoint
    U2DPoint<long> GetClosestPoint(const U2DPoint<long>& inPoint) { return this->mTiles.GetClosestPoint(inPoint); }
    // +0x20 0xda77 (ret 4, hidden result): a random tile from the game's random stream, (0, 0) when empty (a town,
    // herd or rectangle area: its own)
    U2DPoint<long> GetRandomPoint();
    U2DPoint<long> AreaRandomPoint() { return EEAIArea::PointOf(ee_astar_area_random_point((uint64_t)(uintptr_t)this)); }

    // inline COMDAT 0xc6fd (emitted at its first user in EECPASGU*'s range): the hull (bounds first), then the tiles
    // Provisional name
    bool Contains(const U2DPoint<long>& inPoint) { return this->HullContains(inPoint) && this->mTiles.Contains(inPoint); }
    // 0xdaae: appends every tile of the hull's scan lines (provisional name)
    void GetTiles(std::vector<U2DPoint<long> >& outTiles) { ee_astar_area_get_tiles(this, outTiles.size(), &outTiles, EEAIPushPoint); }

    // New (port): the members behind accessors (R5b.1 blocker (b): the AI and the units read an area only
    // through these, so the record can live elsewhere)
    // the tile set (U2DSparseArrayPointContainer: ee_astar sparse.rs)
    U2DSparseArrayPointContainer& GetTileSet() { return this->mTiles; }
    const U2DSparseArrayPointContainer& GetTileSet() const { return this->mTiles; }
    long GetNumTiles() const { return this->mTiles.Size(); }
    // the hull (the record's; U2DConvexHull<long>'s calls, HULL capture records and all)
    const U2DPoint<long>& GetHullCenter() const { return reinterpret_cast<const U2DPoint<long>&>(this->hull_center); }
    const U2DRectangle<long>& GetHullBounds() const { return reinterpret_cast<const U2DRectangle<long>&>(this->hull_bounds); }
    // BoundedContains: the bounds, then the outline (HULL 6)
    bool HullContains(const U2DPoint<long>& inPoint) { return ee_astar_area_hull_contains(this, EEAIArea::HullPoint(inPoint)); }
    unsigned long GetHullPointCount() const { return (unsigned long)EEAIListView<U2DPoint<long> >(ee_astar_area_hull_points, static_cast<const EeAStarAreaRecord*>(this)).size(); }
    const U2DPoint<long>* GetHullPointData() const
        { return EEAIListView<U2DPoint<long> >(ee_astar_area_hull_points, static_cast<const EeAStarAreaRecord*>(this)).begin(); }
    const U2DPoint<long>& GetHullPoint(long inIndex) const { return this->GetHullPointData()[inIndex]; }
    U2DPoint<long> GetHullClosestOutlinePoint(const U2DPoint<long>& inPoint)   // (HULL 2)
        { return EEAIArea::PointOf(ee_astar_area_hull_closest_outline_point(this, EEAIArea::HullPoint(inPoint))); }
    // the tiles' bounds in influence-table regions
    void SetSerial(unsigned long inSerial) { this->serial = inSerial; }
    EeAStarAreaRecord* AreaRecord() { return this; }   // New (port): the record (ee_sim::astar cm_io.rs's layout)

protected:
    static EeHullPoint HullPoint(const U2DPoint<long>& inPoint) { EeHullPoint thePoint = {inPoint.mX, inPoint.mY}; return thePoint; }
    static U2DPoint<long> PointOf(const EeHullPoint& inPoint) { return U2DPoint<long>(inPoint.x, inPoint.y); }

private:
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
