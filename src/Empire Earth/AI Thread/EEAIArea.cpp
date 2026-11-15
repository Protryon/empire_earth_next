// C:\EEX\Empire Earth\AI Thread\EEAIArea.cpp (file name inferred: no __FILE__; RTTI class EEAIArea, vtable
// 0x439d6c). The base of the AI's map areas: 0xd6e3..0xe9a1 (EECPASGUTransport.cpp ends at 0xd6e3; the
// EEAIAStarBoundedPathGoal group starts at 0xe9a1). EEAIArea's out-of-line slots +0x04..+0x28 in slot order (every
// EEAI* area kind points here), their helpers, then the map/sort/vector COMDATs of their first use. The ctor and dtor
// are inline COMDATs at their first user (0x110cb/0x11123).
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitCollector.h"

// The unit filters of slots +0x0c/+0x10 (passed by reference to the collector; operator() COMDATs 0xde5c/0xe01d
// after the collector's instantiations) (provisional names)
struct EEAIAreaContainsUnit {
    EEAIAreaContainsUnit(EEAIArea* inArea) : mArea(inArea) {}
    bool operator()(EEUnit* inUnit) const { return this->mArea->Contains(U2DPoint<long>(inUnit->mTileX, inUnit->mTileY)); }
    EEAIArea* mArea;
};
struct EEAIAreaHullContainsUnit {
    EEAIAreaHullContainsUnit(EEAIArea* inArea) : mArea(inArea) {}
    bool operator()(EEUnit* inUnit) const
        { return this->mArea->HullContains(U2DPoint<long>(inUnit->mTileX, inUnit->mTileY)); }
    EEAIArea* mArea;
};

// cdecl template 0xdcbe / 0xde7f: EEGetUnitsInRectangle (ee/EEUnitCollector.h) with these filters

// Port: the hull geometry of GetHullDistanceSquared, GetClosestHullPoint, GetClosestPoints' loop and GetTiles is Rust
// (ee_astar area_geom.rs, docs/RUST_PLAN.md R5b.1, branch rust-r5b-cont3); these forward the hull's points
static void EEAIAreaPushTile(void* inTiles, long inX, long inY)
{
    static_cast<std::vector<U2DPoint<long> >*>(inTiles)->push_back(U2DPoint<long>(inX, inY));
}

static const EeHullPoint* EEAIAreaHullPoints(const U2DPoint<long>* inPoints)
{
    return reinterpret_cast<const EeHullPoint*>(inPoints);
}

// UpdateBounds' table: its region size, 0 without one (the record keeps the bounds then)
static long EEAIAreaRegionSize(EEInfluenceTable* inTable)
{
    return inTable != NULL ? inTable->mRegionSize : 0;
}

// TEST ONLY: HULL 1, U2DConvexHull<long>::SetPoints' record (the input and the whole hull), when the call was outer
void EEAIArea::CaptureSetPoints(const std::vector<U2DPoint<long> >& inPoints)
{
    EEGeomCaptureBegin(kGeomCaptureHull, 1);
    EEGeomCaptureValue(false);
    EEGeomCapturePoints(inPoints, true);
    EEGeomCapturePoints(this->GetHullPointData(), this->GetHullPointCount(), true);
    EEGeomCaptureValue(this->hull_bounds.bottom);
    EEGeomCaptureValue(this->hull_bounds.left);
    EEGeomCaptureValue(this->hull_bounds.right);
    EEGeomCaptureValue(this->hull_bounds.top);
    EEGeomCapturePoint(this->GetHullCenter());
    EEGeomCaptureFloat(this->hull_area);
    EEGeomCaptureCommit();
}

// The hull's queries (U2DConvexHull<long>'s, on the record's points)
bool EEAIArea::HullContains(const U2DPoint<long>& inPoint)
{
    bool theResult = this->GetHullBounds().Contains(inPoint);
    if (theResult) {
        EEGeomCaptureScope theScope;
        theResult = ee_hull_contains(EEAIAreaHullPoints(this->GetHullPointData()), this->GetHullPointCount(),
                                     U2DHullPointOf(inPoint));
        if (theScope.mOuter) {   // TEST ONLY: HULL 6
            EEGeomCaptureBegin(kGeomCaptureHull, 6);
            EEGeomCapturePoints(this->GetHullPointData(), this->GetHullPointCount(), false);
            EEGeomCapturePoint(inPoint);
            EEGeomCaptureValue(theResult);
            EEGeomCaptureCommit();
        }
    }
    return theResult;
}

std::vector<U2DPoint<long> > EEAIArea::GetHullPoints() const
{
    const U2DPoint<long>* thePoints = this->GetHullPointData();
    return std::vector<U2DPoint<long> >(thePoints, thePoints + this->GetHullPointCount());
}

U2DPoint<long> EEAIArea::GetHullClosestPoint(const U2DPoint<long>& inPoint)
{
    EEGeomCaptureScope theScope;
    U2DPoint<long> theResult = U2DHullPoint(ee_hull_closest_point(EEAIAreaHullPoints(this->GetHullPointData()),
                                                                  this->GetHullPointCount(), U2DHullPointOf(inPoint)));
    if (theScope.mOuter) {   // TEST ONLY: HULL 4
        EEGeomCaptureBegin(kGeomCaptureHull, 4);
        EEGeomCapturePoints(this->GetHullPointData(), this->GetHullPointCount(), false);
        EEGeomCapturePoint(inPoint);
        EEGeomCapturePoint(theResult);
        EEGeomCaptureCommit();
    }
    return theResult;
}

U2DPoint<long> EEAIArea::GetHullClosestOutlinePoint(const U2DPoint<long>& inPoint)
{
    EEGeomCaptureScope theScope;
    U2DPoint<long> theResult = U2DHullPoint(ee_hull_closest_outline_point(
        EEAIAreaHullPoints(this->GetHullPointData()), this->GetHullPointCount(), U2DHullPointOf(inPoint)));
    if (theScope.mOuter) {   // TEST ONLY: HULL 2
        EEGeomCaptureBegin(kGeomCaptureHull, 2);
        EEGeomCapturePoints(this->GetHullPointData(), this->GetHullPointCount(), false);
        EEGeomCapturePoint(inPoint);
        EEGeomCapturePoint(theResult);
        EEGeomCaptureCommit();
    }
    return theResult;
}

void EEAIArea::SetHullPoints(const std::vector<U2DPoint<long> >& inPoints)
{
    EEGeomCaptureScope theScope;
    ee_astar_area_set_hull(this, U2DHullPoints(inPoints), inPoints.size());
    if (theScope.mOuter)
        this->CaptureSetPoints(inPoints);
}

// FUNCTION: EXE 0x40d6e3
// Adds tiles to the area and, when asked, rebuilds its hull and influence-region bounds
void EEAIArea::AddTiles(const std::vector<U2DPoint<long> >& inTiles, bool inUpdate)
{
    this->mTiles.Insert(inTiles);
    if (inUpdate) {
        this->UpdateHull();
    }
}

// FUNCTION: EXE 0x40d705
// Fills the area with the given tiles and builds its hull and influence-region bounds
void EEAIArea::Create(const std::vector<U2DPoint<long> >& inTiles)
{
    this->mTiles.Create(inTiles);
    this->UpdateHull();
}

// NEAR: the original builds each corner twice through the out-of-line U2DPoint<long> (const long&, const long&) ctor
// 0xaa817 and copy ctor 0x4d28b (the exe's older U2DPoint.h; the open U2DPoint copy-ctor family)
// FUNCTION: EXE 0x40d763
// Sets the area's bounds in influence-table regions from its hull's bounding box (no table: unchanged)
void EEAIArea::UpdateBounds(EEInfluenceTable* inTable)
{
    ee_astar_area_update_bounds(this, EEAIAreaRegionSize(inTable));
}

// FUNCTION: EXE 0x40d81e
// Returns the squared distance between the closest points of this area's hull and another's
float EEAIArea::GetHullDistanceSquared(EEAIArea* inOther)
{
    EEGeomCaptureScope theScope;
    float theResult = ee_area_hull_distance_squared(EEAIAreaHullPoints(this->GetHullPointData()), this->GetHullPointCount(),
                                                    EEAIAreaHullPoints(inOther->GetHullPointData()),
                                                    inOther->GetHullPointCount());
    if (theScope.mOuter) {   // TEST ONLY: AREA 4
        EEGeomCaptureBegin(kGeomCaptureArea, 4);
        EEGeomCapturePoints(this->GetHullPointData(), this->GetHullPointCount(), false);
        EEGeomCapturePoints(inOther->GetHullPointData(), inOther->GetHullPointCount(), false);
        EEGeomCaptureFloat(theResult);
        EEGeomCaptureCommit();
    }
    return theResult;
}

// FUNCTION: EXE 0x40d859
// Appends the units standing on the area's tiles (empty area: none)
void EEAIArea::GetUnitsOnTiles(std::vector<EEUnit*>& outUnits)
{
    if (this->mTiles.Size() > 0) {
        EEAIAreaContainsUnit thePredicate(this);
        EEGetUnitsInRectangle(this->GetHullBounds(), outUnits, thePredicate, true);
    }
}

// FUNCTION: EXE 0x40d87f
// Appends the units standing inside the area's hull (empty area: none)
void EEAIArea::GetUnitsInHull(std::vector<EEUnit*>& outUnits)
{
    if (this->mTiles.Size() > 0) {
        EEAIAreaHullContainsUnit thePredicate(this);
        EEGetUnitsInRectangle(this->GetHullBounds(), outUnits, thePredicate, true);
    }
}

// FUNCTION: EXE 0x40d8a5
// Returns the point itself when it is inside the area's hull, else the closest point on the hull outline
U2DPoint<long> EEAIArea::GetClosestHullPoint(const U2DPoint<long>& inPoint)
{
    EEGeomCaptureScope theScope;
    U2DPoint<long> theResult = U2DHullPoint(ee_area_closest_hull_point(
        EEAIAreaHullPoints(this->GetHullPointData()), this->GetHullPointCount(), &this->hull_bounds, U2DHullPointOf(inPoint)));
    if (theScope.mOuter) {   // TEST ONLY: AREA 3
        EEGeomCaptureBegin(kGeomCaptureArea, 3);
        EEGeomCapturePoints(this->GetHullPointData(), this->GetHullPointCount(), false);
        EEGeomCapturePoint(inPoint);
        EEGeomCapturePoint(theResult);
        EEGeomCaptureCommit();
    }
    return theResult;
}

// FUNCTION: EXE 0x40d8e6
// Returns the area tile closest to a point
U2DPoint<long> EEAIArea::GetClosestPoint(const U2DPoint<long>& inPoint)
{
    return this->mTiles.GetClosestPoint(inPoint);
}

// FUNCTION: EXE 0x40d8fe
// Finds this area's tile and the other area's hull outline point closest to each other; false (both (0, 0))
// when either area is empty
bool EEAIArea::GetClosestPoints(EEAIArea* inOther, U2DPoint<long>& outThis, U2DPoint<long>& outOther)
{
    EEGeomCaptureScope theScope;
    std::vector<U2DPoint<long> > thePoints;
    EeHullPoint theThis;
    EeHullPoint theOther;
    bool theResult;
    if (this->mTiles.Size() != 0 && inOther->mTiles.Size() != 0)
        theResult = true;
    else
        theResult = false;
    outThis.mX = 0;
    outThis.mY = 0;
    outOther.mX = 0;
    outOther.mY = 0;
    if (theResult) {
        this->mTiles.BuildPointList(thePoints, 1);
        if (ee_area_closest_points_to_hull(U2DHullPoints(thePoints), thePoints.size(),
                                           EEAIAreaHullPoints(inOther->GetHullPointData()), inOther->GetHullPointCount(),
                                           &theThis, &theOther)) {
            outThis = U2DHullPoint(theThis);
            outOther = U2DHullPoint(theOther);
        }
    }
    if (theScope.mOuter) {   // TEST ONLY: AREA 1
        EEGeomCaptureBegin(kGeomCaptureArea, 1);
        EEGeomCaptureValue(this->mTiles.Size());
        EEGeomCapturePoints(thePoints, false);
        EEGeomCapturePoints(inOther->GetHullPointData(), inOther->GetHullPointCount(), false);
        EEGeomCaptureValue(theResult);
        EEGeomCapturePoint(outThis);
        EEGeomCapturePoint(outOther);
        EEGeomCaptureCommit();
    }
    return theResult;
}

// FUNCTION: EXE 0x40da77
// Returns a random tile of the area from the game's random stream, or (0, 0) when it is empty
U2DPoint<long> EEAIArea::GetRandomPoint()
{
    U2DPoint<long> theResult(0, 0);
    if (this->mTiles.Size() > 0)
        theResult = this->mTiles.GetRandomPoint(gPhysics->mRandom);
    return theResult;
}

// FUNCTION: EXE 0x40daae
// Appends every tile inside the area's hull, row by row
void EEAIArea::GetTiles(std::vector<U2DPoint<long> >& outTiles)
{
    EEGeomCaptureScope theScope;
    size_t theFirst = outTiles.size();
    ee_area_tiles(EEAIAreaHullPoints(this->GetHullPointData()), this->GetHullPointCount(), &outTiles, EEAIAreaPushTile);
    if (theScope.mOuter) {   // TEST ONLY: AREA 5, the tiles appended
        EEGeomCaptureBegin(kGeomCaptureArea, 5);
        EEGeomCapturePoints(this->GetHullPointData(), this->GetHullPointCount(), false);
        EEGeomCaptureValue((long)theFirst);
        EEGeomCapturePoints(std::vector<U2DPoint<long> >(outTiles.begin() + theFirst, outTiles.end()), false);
        EEGeomCaptureCommit();
    }
}

// FUNCTION: EXE 0x40db32
// Rebuilds the area's hull from its edge tiles and updates its influence-region bounds
void EEAIArea::UpdateHull()
{
    EEGeomCaptureScope theScope;
    ee_astar_area_update_hull(this, this->mTiles.GetHandle(), EEAIAreaRegionSize(gWorld.mInfluenceTable));
    if (theScope.mOuter) {   // TEST ONLY: the hull's SetPoints, as U2DConvexHull<long> recorded it
        std::vector<U2DPoint<long> > thePoints;
        this->mTiles.GetOnlyExtremeEdgePoints(thePoints);
        this->CaptureSetPoints(thePoints);
    }
}

// FUNCTION: EXE 0x40db7f
// Removes a tile from the area and rebuilds the hull (bounds not updated); returns whether the tile was there
bool EEAIArea::RemoveTile(const U2DPoint<long>& inPoint)
{
    EEGeomCaptureScope theScope;
    bool theResult = ee_astar_area_remove_tile(this, this->mTiles.GetHandle(), (int32_t)inPoint.mX, (int32_t)inPoint.mY);
    if (theResult && theScope.mOuter) {   // TEST ONLY: as UpdateHull's
        std::vector<U2DPoint<long> > thePoints;
        this->mTiles.GetOnlyExtremeEdgePoints(thePoints);
        this->CaptureSetPoints(thePoints);
    }
    return theResult;
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x40dbdc
// TEMPLATE: EXE 0x40dc07
// TEMPLATE: EXE 0x40dc78
// TEMPLATE: EXE 0x40dcaa
// TEMPLATE: EXE 0x40dcbe
// TEMPLATE: EXE 0x40de5c
// TEMPLATE: EXE 0x40e01d
// TEMPLATE: EXE 0x40de7f
// TEMPLATE: EXE 0x40e043
// TEMPLATE: EXE 0x40e0b9
// TEMPLATE: EXE 0x40e104
// TEMPLATE: EXE 0x40e181
// TEMPLATE: EXE 0x40e23e
// TEMPLATE: EXE 0x40e2aa
// TEMPLATE: EXE 0x40e2d3
// TEMPLATE: EXE 0x40e3a6
// TEMPLATE: EXE 0x40e3d7
// TEMPLATE: EXE 0x40e415
// TEMPLATE: EXE 0x40e464
// TEMPLATE: EXE 0x40e501
// TEMPLATE: EXE 0x40e53b
// TEMPLATE: EXE 0x40e57c
// TEMPLATE: EXE 0x40e69c
// TEMPLATE: EXE 0x40e6cf
// TEMPLATE: EXE 0x40e711
// TEMPLATE: EXE 0x40e755
// TEMPLATE: EXE 0x40e77e
// TEMPLATE: EXE 0x40e7be
// TEMPLATE: EXE 0x40e82c
// TEMPLATE: EXE 0x40e87f
// TEMPLATE: EXE 0x40e8ce
// TEMPLATE: EXE 0x40e902
// TEMPLATE: EXE 0x40e964
// TEMPLATE: EXE 0x40e98e
