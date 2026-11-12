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

// FUNCTION: EXE 0x41d846
// Creates an empty continent with no category, id 0 and all seven counts zero
EEAIContinent::EEAIContinent()
    : mCategory(false), mID(0)
{
    for (long i = 6; i >= 0; i--) {
        this->mCounts[i] = 0;
    }
}

// FUNCTION: EXE 0x41d94d
// Destroys the continent (members only)
EEAIContinent::~EEAIContinent()
{
}

// FUNCTION: EXE 0x41d9e0
// Adds a path area id to the continent's sorted id list unless it is already there
void EEAIContinent::AddPathAreaID(unsigned short inID)
{
    if (!std::binary_search(this->mPathAreaIDs.begin(), this->mPathAreaIDs.end(), inID)) {
        this->mPathAreaIDs.push_back(inID);
        std::sort(this->mPathAreaIDs.begin(), this->mPathAreaIDs.end());
    }
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
    *outID = inSpace ? theArea->mSpaceRegionID : theArea->mWaterBodyID;
    return true;
}

static bool EEAIShoreIsLand(void*, long inX, long inY)
{
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(inX, inY);
    return theContinent != NULL && theContinent->mCategory == kContinentCategoryLand;
}

static uint16_t EEAIShoreRegion(void*, long inX, long inY, bool inSpace)
{
    EETileCoordinate theNeighbor;
    theNeighbor.mX = inX;
    theNeighbor.mY = inY;
    return gContinentManager.GetContinent(theNeighbor, inSpace ? kAIPathSpaceRegion : kAIPathWaterBody);
}

// FUNCTION: EXE 0x41da61
// Finds this continent's shore tile and the other continent's hull outline point closest to each other; falls
// back to the area version when the other is not a continent or either has no shore. Returns success
bool EEAIContinent::GetClosestPoints(EEAIArea* inOther, U2DPoint<long>& outThis, U2DPoint<long>& outOther)
{
    EEGeomCaptureScope theScope;
    EEAIContinent* theOther = dynamic_cast<EEAIContinent*>(inOther);
    EeHullPoint theThis;
    EeHullPoint theOtherPoint;
    bool theResult;
    if (this->mShoreTiles.size() != 0 && theOther && theOther->mShoreTiles.size() != 0)
        theResult = true;
    else
        theResult = false;
    outThis.mX = 0;
    outThis.mY = 0;
    outOther.mX = 0;
    outOther.mY = 0;
    if (theOther) {
        if (theResult) {
            ee_area_closest_points_to_hull(U2DHullPoints(this->mShoreTiles), this->mShoreTiles.size(),
                                           U2DHullPoints(theOther->mHull.GetPoints()), theOther->mHull.GetPointCount(),
                                           &theThis, &theOtherPoint);
            outThis = U2DHullPoint(theThis);
            outOther = U2DHullPoint(theOtherPoint);
        } else
            theResult = EEAIArea::GetClosestPoints(inOther, outThis, outOther);
    } else
        theResult = EEAIArea::GetClosestPoints(inOther, outThis, outOther);
    if (theScope.mOuter) {   // TEST ONLY: AREA 2
        EEGeomCaptureBegin(kGeomCaptureArea, 2);
        EEGeomCapturePoints(this->mShoreTiles, false);
        EEGeomCaptureValue(theOther != NULL);
        EEGeomCaptureValue(inOther->mTiles.Size());
        EEGeomCapturePoints(inOther->mHull.GetPoints(), false);
        EEGeomCaptureValue(theResult);
        EEGeomCapturePoint(outThis);
        EEGeomCapturePoint(outOther);
        EEGeomCaptureCommit();
    }
    return theResult;
}

// FUNCTION: EXE 0x41dbf4
// Returns the continent's path area containing a point, or NULL
EEAIPathArea* EEAIContinent::GetPathAreaAt(const U2DPoint<long>& inPoint)
{
    EEAIPathArea* theResult = NULL;
    for (long i = this->mPathAreaIDs.size() - 1; i >= 0; i--) {
        EEAIPathArea* theArea = gContinentManager.GetPathArea(this->mPathAreaIDs[i]);
        if (theArea && theArea->Contains(inPoint)) {
            theResult = theArea;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x41dc54
// Appends the continent's shore tiles next to area inID (all of them for id 0)
void EEAIContinent::GetShoreNextTo(unsigned short inID, std::vector<U2DPoint<long> >& outTiles)
{
    outTiles.reserve(this->mShoreTiles.size());
    for (long i = this->mShoreTiles.size() - 1; i >= 0; i--) {
        if (inID == 0 || gContinentManager.IsNextToArea(this->mShoreTiles[i].GetX(), this->mShoreTiles[i].GetY(), inID))
            outTiles.push_back(this->mShoreTiles[i]);
    }
}

// FUNCTION: EXE 0x41dccf
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
    ee_area_shore_in_regions(U2DHullPoints(this->mShoreTiles), this->mShoreTiles.size(),
                             inIDs.empty() ? NULL : &inIDs[0], inIDs.size(), inKind == kAIPathSpaceRegion, &theHost,
                             &outTiles, EEAIContinentPushTile);
    if (theScope.mOuter) {   // TEST ONLY: AREA 6
        EEGeomCaptureBegin(kGeomCaptureArea, 6);
        EEGeomCapturePoints(this->mShoreTiles, false);
        EEGeomCaptureValue((long)inIDs.size());
        for (size_t i = 0; i < inIDs.size(); i++)
            EEGeomCaptureValue(inIDs[i]);
        EEGeomCaptureValue(inKind);
        EEGeomCapturePoints(outTiles, false);
        EEGeomCaptureCommit();
    }
}

// FUNCTION: EXE 0x41e45e
// Collects the continent's shore tiles in one water body or space region
void EEAIContinent::GetShoreInRegion(unsigned short inID, std::vector<U2DPoint<long> >& outTiles, long inKind)
{
    std::vector<unsigned short> theIDs;
    theIDs.push_back(inID);
    this->GetShoreInRegions(theIDs, outTiles, inKind);
}

// FUNCTION: EXE 0x41e4a1
// Removes a path area id from the continent's sorted id list if present
void EEAIContinent::RemovePathAreaID(unsigned short inID)
{
    std::vector<unsigned short>::iterator theID = std::lower_bound(this->mPathAreaIDs.begin(), this->mPathAreaIDs.end(), inID);
    if (theID != this->mPathAreaIDs.end() && *theID == inID)
        this->mPathAreaIDs.erase(theID);
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// SYNTHETIC: EXE 0x41d931
// TEMPLATE: EXE 0x41e5bd
// TEMPLATE: EXE 0x41e602
// TEMPLATE: EXE 0x41e60a
// TEMPLATE: EXE 0x41e627
// TEMPLATE: EXE 0x41e639
// TEMPLATE: EXE 0x41e69a
// TEMPLATE: EXE 0x41e6ac
// TEMPLATE: EXE 0x41e756
// TEMPLATE: EXE 0x41e78b
// TEMPLATE: EXE 0x41e7af
// TEMPLATE: EXE 0x41e7d4
// TEMPLATE: EXE 0x41e893
// TEMPLATE: EXE 0x41e8c4
// TEMPLATE: EXE 0x41e935
// TEMPLATE: EXE 0x41e9be
// TEMPLATE: EXE 0x41e9c4
// TEMPLATE: EXE 0x41e9fb
// TEMPLATE: EXE 0x41ea2d
// TEMPLATE: EXE 0x41eb11
// TEMPLATE: EXE 0x41eb3a
// TEMPLATE: EXE 0x41eb65
// TEMPLATE: EXE 0x41eba6
// TEMPLATE: EXE 0x41ebcb
// TEMPLATE: EXE 0x41ebf0
// TEMPLATE: EXE 0x41ec50
// TEMPLATE: EXE 0x41eca5
// TEMPLATE: EXE 0x41ecbc
// TEMPLATE: EXE 0x41ecfa
