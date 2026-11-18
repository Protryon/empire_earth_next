// C:\EEX\Empire Earth\AI Thread\EEAIContinent.cpp (file name inferred: no __FILE__; RTTI class EEAIContinent,
// vtables 0x43a1c8/0x43a1c0). One continent of the AI's continent map: 0x1d846..0x1ed2c (EEAIChokePoint.cpp ends at
// 0x1d846; the two tile helpers 0x1ed2c/0x1eea9 and Shutdown 0x1ef12 open EEAIContinentManager.cpp). ctor, ??_G, dtor, the members in
// address order, then the vector/sort COMDATs.
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

#include "ee/EEAIPathArea.h"

// Creates an empty continent with no category, id 0 and all seven counts zero
EEAIContinent::EEAIContinent()
{
    ee_astar_continent_init(this);
}

// Destroys the continent (members only)
EEAIContinent::~EEAIContinent()
{
    ee_astar_continent_free(this);
}

// Port: the closest points' loop and GetShoreInRegions' walk are Rust (ee_astar area_geom.rs, docs/RUST_PLAN.md
// R5b.1, branch rust-r5b-cont3); the manager's lookups stay behind EeShoreCallbacks
static void EEAIContinentPushTile(void* inTiles, long inX, long inY)
{
    static_cast<std::vector<U2DPoint<long> >*>(inTiles)->push_back(U2DPoint<long>(inX, inY));
}

static bool EEAIShorePathAreaRegion(void*, long inX, long inY, bool inSpace, uint16_t* outID)
{
    EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(U2DPoint<long>(inX, inY));
    if (theArea == NULL)
        return false;
    *outID = inSpace ? theArea->GetSpaceRegionID() : theArea->GetWaterBodyID();
    return true;
}

static bool EEAIShoreIsLand(void*, long inX, long inY)
{
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(inX, inY);
    return theContinent != NULL && theContinent->GetCategory() == kContinentCategoryLand;
}

static uint16_t EEAIShoreRegion(void*, long inX, long inY, bool inSpace)
{
    EETileCoordinate theNeighbor;
    theNeighbor.mX = inX;
    theNeighbor.mY = inY;
    return gContinentManager.GetContinent(theNeighbor, inSpace ? kAIPathSpaceRegion : kAIPathWaterBody);
}

// Finds this continent's shore tile and the other continent's hull outline point closest to each other; falls
// back to the area version when the other is not a continent or either has no shore. Returns success
bool EEAIContinent::GetClosestPoints(EEAIArea* inOther, U2DPoint<long>& outThis, U2DPoint<long>& outOther)
{
    EEGeomCaptureScope theScope;
    EEAIContinent* theOther = dynamic_cast<EEAIContinent*>(inOther);
    EeHullPoint theThis;
    EeHullPoint theOtherPoint;
    bool theResult;
    if (this->GetShoreTiles().size() != 0 && theOther && theOther->GetShoreTiles().size() != 0)
        theResult = true;
    else
        theResult = false;
    outThis.mX = 0;
    outThis.mY = 0;
    outOther.mX = 0;
    outOther.mY = 0;
    if (theOther) {
        if (theResult) {
            ee_area_closest_points_to_hull(reinterpret_cast<const EeHullPoint*>(this->GetShoreTiles().begin()), this->GetShoreTiles().size(),
                                           reinterpret_cast<const EeHullPoint*>(theOther->GetHullPointData()), theOther->GetHullPointCount(),
                                           &theThis, &theOtherPoint);
            outThis = U2DHullPoint(theThis);
            outOther = U2DHullPoint(theOtherPoint);
        } else
            theResult = EEAIArea::GetClosestPoints(inOther, outThis, outOther);
    } else
        theResult = EEAIArea::GetClosestPoints(inOther, outThis, outOther);
    if (theScope.mOuter) {   // TEST ONLY: AREA 2
        EEGeomCaptureBegin(kGeomCaptureArea, 2);
        EEGeomCapturePoints(this->GetShoreTiles().begin(), this->GetShoreTiles().size(), false);
        EEGeomCaptureValue(theOther != NULL);
        EEGeomCaptureValue(inOther->GetNumTiles());
        EEGeomCapturePoints(inOther->GetHullPoints(), false);
        EEGeomCaptureValue(theResult);
        EEGeomCapturePoint(outThis);
        EEGeomCapturePoint(outOther);
        EEGeomCaptureCommit();
    }
    return theResult;
}

// Returns the continent's path area containing a point, or NULL
EEAIPathArea* EEAIContinent::GetPathAreaAt(const U2DPoint<long>& inPoint)
{
    return (EEAIPathArea*)(uintptr_t)ee_astar_cm_object_path_area_at(&gContinentManager.mRecord, (uint64_t)(uintptr_t)this, false,
                                                                     inPoint.mX, inPoint.mY);   // Rust: cm_query.rs
}

// Appends the continent's shore tiles next to area inID (all of them for id 0)
void EEAIContinent::GetShoreNextTo(unsigned short inID, std::vector<U2DPoint<long> >& outTiles)
{
    EEAIListView<U2DPoint<long> > theShore = this->GetShoreTiles();
    outTiles.reserve(theShore.size());
    for (long i = theShore.size() - 1; i >= 0; i--) {
        if (inID == 0 || gContinentManager.IsNextToArea(theShore[i].GetX(), theShore[i].GetY(), inID))
            outTiles.push_back(theShore[i]);
    }
}

// Collects the shore tiles whose path area lies in one of the water-body (or space-region) ids, or land tiles
// with a neighbour in one of those regions; all shore tiles when the list is empty or holds a 0 (Rust:
// area_geom.rs shore_in_regions; the original read the path area's id before its NULL test, bytes 0x41dda7, and
// asked the neighbours' water bodies also for space regions, bytes 0x41df09: the port reads the id of an area only
// and asks the requested kind)
void EEAIContinent::GetShoreInRegions(const std::vector<unsigned short>& inIDs, std::vector<U2DPoint<long> >& outTiles,
                              long inKind)
{
    EEGeomCaptureScope theScope;
    EeShoreCallbacks theHost;
    theHost.ctx = NULL;
    theHost.path_area_region = EEAIShorePathAreaRegion;
    theHost.is_land = EEAIShoreIsLand;
    theHost.region = EEAIShoreRegion;
    theHost.map_width = (uint32_t)gMapWidth;
    theHost.map_height = (uint32_t)gMapHeight;
    outTiles.clear();
    ee_area_shore_in_regions(reinterpret_cast<const EeHullPoint*>(this->GetShoreTiles().begin()), this->GetShoreTiles().size(),
                             inIDs.empty() ? NULL : &inIDs[0], inIDs.size(), inKind == kAIPathSpaceRegion, &theHost,
                             &outTiles, EEAIContinentPushTile);
    if (theScope.mOuter) {   // TEST ONLY: AREA 6
        EEGeomCaptureBegin(kGeomCaptureArea, 6);
        EEGeomCapturePoints(this->GetShoreTiles().begin(), this->GetShoreTiles().size(), false);
        EEGeomCaptureValue((long)inIDs.size());
        for (size_t i = 0; i < inIDs.size(); i++)
            EEGeomCaptureValue(inIDs[i]);
        EEGeomCaptureValue(inKind);
        EEGeomCapturePoints(outTiles, false);
        EEGeomCaptureCommit();
    }
}

// Collects the continent's shore tiles in one water body or space region
void EEAIContinent::GetShoreInRegion(unsigned short inID, std::vector<U2DPoint<long> >& outTiles, long inKind)
{
    std::vector<unsigned short> theIDs;
    theIDs.push_back(inID);
    this->GetShoreInRegions(theIDs, outTiles, inKind);
}
