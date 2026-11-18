// New (port), TEST ONLY: the capture of the continent manager's analysis (ee/EEAIContinentCapture.h). Every field is
// written little-endian as an i32/u32 (bools and shorts widened) so the record reads the same on every system; the
// tiles of an area in its container's order, the lists in theirs.
#include <windows.h>
#include <string.h>
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
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIRectangleArea.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIHill.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EESimCapture.h"
#include "ee/EEAIContinentCapture.h"

static void EEAICapturePoint(EESimCaptureRecord& ioRecord, const U2DPoint<long>& inPoint)
{
    ioRecord.I32((int)inPoint.mX);
    ioRecord.I32((int)inPoint.mY);
}

static void EEAICaptureTiles(EESimCaptureRecord& ioRecord, const U2DSparseArrayPointContainer& inTiles)
{
    long theCount = inTiles.Size();
    ioRecord.I32((int)theCount);
    for (long i = 0; i < theCount; i++)
        EEAICapturePoint(ioRecord, inTiles[i]);
}

static void EEAICapturePoints(EESimCaptureRecord& ioRecord, const std::vector<U2DPoint<long> >& inPoints)
{
    ioRecord.I32((int)inPoints.size());
    for (size_t i = 0; i < inPoints.size(); i++)
        EEAICapturePoint(ioRecord, inPoints[i]);
}

static void EEAICaptureShorts(EESimCaptureRecord& ioRecord, const std::vector<unsigned short>& inValues)
{
    ioRecord.I32((int)inValues.size());
    for (size_t i = 0; i < inValues.size(); i++)
        ioRecord.U32(inValues[i]);
}

static void EEAICaptureLongs(EESimCaptureRecord& ioRecord, const std::vector<long>& inValues)
{
    ioRecord.I32((int)inValues.size());
    for (size_t i = 0; i < inValues.size(); i++)
        ioRecord.I32((int)inValues[i]);
}

static void EEAICaptureULongs(EESimCaptureRecord& ioRecord, const std::vector<unsigned long>& inValues)
{
    ioRecord.I32((int)inValues.size());
    for (size_t i = 0; i < inValues.size(); i++)
        ioRecord.U32((unsigned int)inValues[i]);
}

static void EEAICaptureRectangle(EESimCaptureRecord& ioRecord, const U2DRectangle<long>& inRectangle)
{
    ioRecord.I32((int)inRectangle.mBottom);
    ioRecord.I32((int)inRectangle.mLeft);
    ioRecord.I32((int)inRectangle.mRight);
    ioRecord.I32((int)inRectangle.mTop);
}

// the EEAIArea base: serial, type, bounds, the hull's center, the tiles
static void EEAICaptureArea(EESimCaptureRecord& ioRecord, const EEAIArea& inArea)
{
    ioRecord.U32((unsigned int)inArea.GetSerial());
    ioRecord.I32((int)inArea.GetAreaType());
    EEAICaptureRectangle(ioRecord, inArea.GetBounds());
    EEAICapturePoint(ioRecord, inArea.GetHullCenter());
    EEAICaptureTiles(ioRecord, inArea.GetTileSet());
}

template <class T>
static void EEAICaptureIDs(EESimCaptureRecord& ioRecord, const std::vector<T*>& inObjects)
{
    ioRecord.I32((int)inObjects.size());
    for (size_t i = 0; i < inObjects.size(); i++)
        ioRecord.I32(inObjects[i] ? (int)inObjects[i]->GetID() : -1);
}

static void EEAIContinentFill(EESimCaptureRecord& theRecord);

void EEAIContinentCapture(unsigned long inTag)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord((unsigned int)inTag);
    EEAIContinentFill(theRecord);
    theRecord.Commit();
}

unsigned long long EEAIContinentDigest()
{
    // (no regions yet: before the game's BuildRegions, the per-tile lookups have no sectors to divide by)
    if (gContinentManager.GetPathSectors().empty() || gContinentManager.mRecord.sector_size == 0)
        return 0;
    EESimCaptureRecord theRecord(0);
    EEAIContinentFill(theRecord);
    unsigned long long theHash = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < theRecord.Data().size(); i++)
        theHash = (theHash ^ theRecord.Data()[i]) * 0x100000001b3ull;
    return theHash;
}

static void EEAIContinentFill(EESimCaptureRecord& theRecord)
{
    EEAIContinentManager& theManager = gContinentManager;
    size_t i;
    size_t j;

    // the counters
    theRecord.U32((unsigned int)theManager.mRecord.width);
    theRecord.U32((unsigned int)theManager.mRecord.height);
    theRecord.I32((int)theManager.mRecord.state);
    theRecord.U32((unsigned int)theManager.mRecord.last_serial);
    theRecord.U32(theManager.mRecord.next_path_area_id);
    theRecord.U32(theManager.mRecord.last_sub_sector_id);
    theRecord.I32((int)theManager.mRecord.num_water_tiles);
    theRecord.I32((int)theManager.mRecord.num_land_tiles);
    theRecord.I32((int)theManager.mRecord.num_space_tiles);
    theRecord.I32((int)theManager.mRecord.flags_width);
    theRecord.I32((int)theManager.mRecord.next_land_mass);
    theRecord.I32((int)theManager.mRecord.next_water_body);
    theRecord.I32((int)theManager.mRecord.next_space_region);
    theRecord.I32((int)theManager.mRecord.next_surface_region);
    theRecord.I32((int)theManager.mRecord.checksum);
    theRecord.I32(theManager.mRecord.clearing);
    for (i = 0; i < 7; i++)
        theRecord.U32(theManager.mRecord.next_ids[i]);
    theRecord.I32(theManager.mRecord.continent_ids != NULL);
    theRecord.I32(theManager.mRecord.sub_sector_ids != NULL);
    theRecord.I32(theManager.mRecord.tile_values != NULL);
    theRecord.I32(theManager.mRecord.tile_flags != NULL);

    // the tiles: the continent id (the grid, or the lookup through the sectors once the grid is freed), the sub
    // sector id, the tile value
    for (long x = 0; x < (long)theManager.mRecord.width; x++) {
        for (long y = 0; y < (long)theManager.mRecord.height; y++) {
            theRecord.U32(theManager.GetContinentID(x, y));
            theRecord.U32(theManager.GetPathSubSectorID(x, y));
            theRecord.I32(theManager.mRecord.tile_values ? (int)theManager.mRecord.tile_values[x * theManager.mRecord.width + y] : 0);
        }
    }
    // the continent adjacency matrix
    if (theManager.mRecord.tile_flags) {
        long theCount = theManager.mRecord.flags_width * theManager.mRecord.flags_width;
        theRecord.I32((int)theCount);
        for (long k = 0; k < theCount; k++)
            theRecord.I32(theManager.mRecord.tile_flags[k]);
    } else
        theRecord.I32(0);

    // the continents
    theRecord.I32((int)theManager.GetContinents().size());
    for (i = 0; i < theManager.GetContinents().size(); i++) {
        EEAIContinent* theContinent = theManager.GetContinents()[i];
        theRecord.I32(theContinent != NULL);
        if (!theContinent)
            continue;
        EEAICaptureArea(theRecord, *theContinent);
        theRecord.U32(theContinent->GetID());
        theRecord.I32(theContinent->GetCategory());
        EEAICapturePoints(theRecord, theContinent->GetShoreTiles());
        EEAICaptureLongs(theRecord, theContinent->GetChokePoints());
        EEAICaptureShorts(theRecord, theContinent->GetForestIDs());
        EEAICaptureShorts(theRecord, theContinent->GetPathAreaIDs());
        EEAICaptureShorts(theRecord, theContinent->GetHillIDs());
        EEAICaptureShorts(theRecord, theContinent->GetIDsBC());
        EEAICaptureShorts(theRecord, theContinent->GetIDsCC());
        EEAICaptureShorts(theRecord, theContinent->GetIDsDC());
        EEAICaptureShorts(theRecord, theContinent->GetIDsEC());
        EEAICaptureIDs(theRecord, theContinent->GetNeighbors());
        for (j = 0; j < 7; j++)
            theRecord.U32(theContinent->GetCount(j));
    }

    // the path areas
    theRecord.I32((int)theManager.GetPathAreas().size());
    for (i = 0; i < theManager.GetPathAreas().size(); i++) {
        EEAIPathArea* theArea = theManager.GetPathAreas()[i];
        theRecord.I32(theArea != NULL);
        if (!theArea)
            continue;
        EEAICaptureArea(theRecord, *theArea);
        theRecord.U32(theArea->GetID());
        theRecord.U32(theArea->GetContinentID());
        theRecord.I32((int)theArea->GetKind());
        theRecord.I32((int)theArea->GetUnitID());
        theRecord.U32(theArea->GetSurfaceRegionID());
        theRecord.U32(theArea->GetWaterBodyID());
        theRecord.U32(theArea->GetLandMassID());
        theRecord.U32(theArea->GetSpaceRegionID());
        EEAICaptureShorts(theRecord, theArea->GetSectorIDs());
    }

    // the path sectors
    theRecord.I32((int)theManager.GetPathSectors().size());
    for (i = 0; i < theManager.GetPathSectors().size(); i++) {
        EEAIPathSector* theSector = theManager.GetPathSectors()[i];
        theRecord.I32(theSector != NULL);
        if (!theSector)
            continue;
        theRecord.U32(theSector->mID);
        EEAICaptureRectangle(theRecord, theSector->mBounds);
        EEAICapturePoint(theRecord, theSector->mCenter);
        EEAICaptureIDs(theRecord, theSector->mNeighbors);
        EEAICaptureShorts(theRecord, theSector->mPathAreaIDs);
        theRecord.I32((int)theSector->mSubSectors.size());
        for (std::map<unsigned short, EEAIPathSubSector*>::const_iterator theIt = theSector->mSubSectors.begin();
             theIt != theSector->mSubSectors.end(); ++theIt) {
            theRecord.U32(theIt->first);
            theRecord.I32(theIt->second ? (int)theIt->second->mID : -1);
        }
    }

    // the sub sectors
    theRecord.I32((int)theManager.GetPathSubSectors().size());
    for (i = 0; i < theManager.GetPathSubSectors().size(); i++) {
        EEAIPathSubSector* theSubSector = theManager.GetPathSubSectors()[i];
        theRecord.I32(theSubSector != NULL);
        if (!theSubSector)
            continue;
        theRecord.U32(theSubSector->mID);
        theRecord.I32((int)theSubSector->mKind);
        EEAICapturePoint(theRecord, theSubSector->mLocation);
        theRecord.U32(theSubSector->mContinentID);
        theRecord.U32(theSubSector->mPathAreaID);
        theRecord.I32((int)theSubSector->mPlayerID);
        theRecord.I32(theSubSector->mIsStale);
        theRecord.I32(theSubSector->mSector ? (int)theSubSector->mSector->mID : -1);
        EEAICaptureTiles(theRecord, theSubSector->mTiles);
        EEAICaptureIDs(theRecord, theSubSector->mNeighbors);
        theRecord.I32((int)theSubSector->mLinks.size());
        for (std::multimap<unsigned short, std::pair<EEAIPathSubSector*, EEAIPathSubSector*> >::const_iterator theIt =
                 theSubSector->mLinks.begin();
             theIt != theSubSector->mLinks.end(); ++theIt) {
            theRecord.U32(theIt->first);
            theRecord.I32(theIt->second.first ? (int)theIt->second.first->mID : -1);
            theRecord.I32(theIt->second.second ? (int)theIt->second.second->mID : -1);
        }
    }

    // the choke points
    theRecord.I32((int)theManager.GetChokePoints().size());
    for (i = 0; i < theManager.GetChokePoints().size(); i++) {
        EEAIChokePoint* theChokePoint = theManager.GetChokePoints()[i];
        theRecord.I32(theChokePoint != NULL);
        if (!theChokePoint)
            continue;
        EEAICaptureArea(theRecord, *theChokePoint);
        theRecord.I32((int)theChokePoint->GetIndex());
        theRecord.U32(theChokePoint->GetContinentID());
        EEAICaptureULongs(theRecord, theChokePoint->GetAreaSerials());
    }

    // the hills
    theRecord.I32((int)theManager.GetHills().size());
    for (i = 0; i < theManager.GetHills().size(); i++) {
        EEAIHill* theHill = theManager.GetHills()[i];
        theRecord.I32(theHill != NULL);
        if (!theHill)
            continue;
        EEAICaptureArea(theRecord, *theHill);
        theRecord.U32(theHill->GetContinentID());
        theRecord.U32(theHill->GetID());
    }

    // the forests
    theRecord.I32((int)theManager.GetForests().size());
    for (i = 0; i < theManager.GetForests().size(); i++) {
        EEAIForest* theForest = theManager.GetForests()[i];
        theRecord.I32(theForest != NULL);
        if (!theForest)
            continue;
        EEAICaptureArea(theRecord, *theForest);
        theRecord.I32((int)theForest->GetResourceKind());
        theRecord.I32((int)theForest->GetNumResources1());
        theRecord.I32((int)theForest->GetNumResources2());
        theRecord.I32((int)theForest->GetNumResources3());
        theRecord.U32((unsigned int)theForest->GetNextRenewal());
        theRecord.U32(theForest->GetContinentID());
        theRecord.U32(theForest->GetID());
    }

    // the rectangle areas (the scenario's)
    theRecord.I32((int)theManager.GetRectangleAreas().size());
    for (i = 0; i < theManager.GetRectangleAreas().size(); i++) {
        EEAIRectangleArea* theArea = theManager.GetRectangleAreas()[i];
        theRecord.I32(theArea != NULL);
        if (theArea)
            EEAICaptureArea(theRecord, *theArea);
    }

    // the shore tiles, the free ids, the dirty sectors' tiles
    EEAICapturePoints(theRecord, theManager.GetShoreTiles(EeAStarCmShore::Shore));
    EEAICapturePoints(theRecord, theManager.GetShoreTiles(EeAStarCmShore::IceShore));
    EEAICaptureShorts(theRecord, theManager.GetFreeIDs(EeAStarCmFree::SubSectors));
    EEAICaptureShorts(theRecord, theManager.GetFreeIDs(EeAStarCmFree::PathAreas));
    std::vector<std::pair<unsigned short, std::vector<EETileCoordinate> > > theSectorTiles = theManager.GetSectorTiles();
    theRecord.I32((int)theSectorTiles.size());
    for (std::vector<std::pair<unsigned short, std::vector<EETileCoordinate> > >::const_iterator theIt = theSectorTiles.begin();
         theIt != theSectorTiles.end(); ++theIt) {
        theRecord.U32(theIt->first);
        EEAICapturePoints(theRecord, theIt->second);
    }
    EEAIListView<EeAStarCmArea> theAreas = theManager.GetAreas();
    theRecord.I32((int)theAreas.size());
    for (const EeAStarCmArea* theIt = theAreas.begin(); theIt != theAreas.end(); ++theIt) {
        const EEAIArea* theArea = (const EEAIArea*)(uintptr_t)theIt->area;
        theRecord.U32((unsigned int)theIt->serial);
        theRecord.I32(theArea ? (int)theArea->GetAreaType() : -1);
    }
}

// The hulls' and the areas' geometry records (lle/U2DPolygon.h, kGeomCaptureHull / kGeomCaptureArea): one open record
// and the scope depth per thread
static thread_local long sGeomCaptureDepth = 0;
static thread_local EESimCaptureRecord* sGeomCaptureRecord = NULL;

EEGeomCaptureScope::EEGeomCaptureScope() : mOuter(sGeomCaptureDepth == 0 && EESimCaptureRecord::Active())
{
    sGeomCaptureDepth++;
}

EEGeomCaptureScope::~EEGeomCaptureScope()
{
    sGeomCaptureDepth--;
}

void EEGeomCaptureBegin(unsigned int inTag, unsigned int inKind)
{
    delete sGeomCaptureRecord;
    sGeomCaptureRecord = new EESimCaptureRecord(inTag);
    sGeomCaptureRecord->U32(inKind);
}

void EEGeomCaptureValue(long inValue)
{
    sGeomCaptureRecord->I32((int)inValue);
}

void EEGeomCaptureFloat(float inValue)
{
    sGeomCaptureRecord->F32(inValue);
}

void EEGeomCapturePoint(const U2DPoint<long>& inPoint)
{
    sGeomCaptureRecord->I32((int)inPoint.mX);
    sGeomCaptureRecord->I32((int)inPoint.mY);
}

void EEGeomCapturePoints(const U2DPoint<long>* inPoints, size_t inCount, bool inWhole)
{
    sGeomCaptureRecord->I32((int)inCount);
    if (inWhole) {
        for (size_t i = 0; i < inCount; i++)
            EEGeomCapturePoint(inPoints[i]);
        return;
    }
    unsigned int theHash = 0x811c9dc5u;
    for (size_t i = 0; i < inCount; i++) {
        int theValues[2] = {(int)inPoints[i].mX, (int)inPoints[i].mY};
        const unsigned char* theBytes = (const unsigned char*)theValues;
        for (size_t j = 0; j < sizeof(theValues); j++)
            theHash = (theHash ^ theBytes[j]) * 0x01000193u;
    }
    sGeomCaptureRecord->U32(theHash);
}

void EEGeomCaptureCommit()
{
    sGeomCaptureRecord->Commit();
    delete sGeomCaptureRecord;
    sGeomCaptureRecord = NULL;
}
