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

void EEAIContinentCapture(unsigned long inTag)
{
    if (!EESimCaptureRecord::Active())
        return;
    EEAIContinentManager& theManager = gContinentManager;
    EESimCaptureRecord theRecord((unsigned int)inTag);
    size_t i;
    size_t j;

    // the counters
    theRecord.U32((unsigned int)theManager.mWidth);
    theRecord.U32((unsigned int)theManager.mHeight);
    theRecord.I32((int)theManager.mState);
    theRecord.U32((unsigned int)theManager.mLastSerial);
    theRecord.U32(theManager.mNextPathAreaID);
    theRecord.U32(theManager.mLastSubSectorID);
    theRecord.I32((int)theManager.mNumWaterTiles);
    theRecord.I32((int)theManager.mNumLandTiles);
    theRecord.I32((int)theManager.mNumSpaceTiles);
    theRecord.I32((int)theManager.mFlagsWidth);
    theRecord.I32((int)theManager.mNextID124);
    theRecord.I32((int)theManager.mNextID128);
    theRecord.I32((int)theManager.mNextID12C);
    theRecord.I32((int)theManager.mNextID130);
    theRecord.I32((int)theManager.mChecksum);
    theRecord.I32(theManager.mClearing);
    for (i = 0; i < 7; i++)
        theRecord.U32(theManager.mNextIDs[i]);
    theRecord.I32(theManager.mContinentIDs != NULL);
    theRecord.I32(theManager.mPathSubSectorIDs != NULL);
    theRecord.I32(theManager.mTileValues != NULL);
    theRecord.I32(theManager.mTileFlags != NULL);

    // the tiles: the continent id (the grid, or the lookup through the sectors once the grid is freed), the sub
    // sector id, the tile value
    for (long x = 0; x < (long)theManager.mWidth; x++) {
        for (long y = 0; y < (long)theManager.mHeight; y++) {
            theRecord.U32(theManager.GetContinentID(x, y));
            theRecord.U32(theManager.GetPathSubSectorID(x, y));
            theRecord.I32(theManager.mTileValues ? (int)theManager.mTileValues[x * theManager.mWidth + y] : 0);
        }
    }
    // the continent adjacency matrix
    if (theManager.mTileFlags) {
        long theCount = theManager.mFlagsWidth * theManager.mFlagsWidth;
        theRecord.I32((int)theCount);
        for (long k = 0; k < theCount; k++)
            theRecord.I32(theManager.mTileFlags[k]);
    } else
        theRecord.I32(0);

    // the continents
    theRecord.I32((int)theManager.mContinents.size());
    for (i = 0; i < theManager.mContinents.size(); i++) {
        EEAIContinent* theContinent = theManager.mContinents[i];
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
    theRecord.I32((int)theManager.mPathAreas.size());
    for (i = 0; i < theManager.mPathAreas.size(); i++) {
        EEAIPathArea* theArea = theManager.mPathAreas[i];
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
    theRecord.I32((int)theManager.mPathSectors.size());
    for (i = 0; i < theManager.mPathSectors.size(); i++) {
        EEAIPathSector* theSector = theManager.mPathSectors[i];
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
    theRecord.I32((int)theManager.mPathSubSectors.size());
    for (i = 0; i < theManager.mPathSubSectors.size(); i++) {
        EEAIPathSubSector* theSubSector = theManager.mPathSubSectors[i];
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
    theRecord.I32((int)theManager.mChokePoints.size());
    for (i = 0; i < theManager.mChokePoints.size(); i++) {
        EEAIChokePoint* theChokePoint = theManager.mChokePoints[i];
        theRecord.I32(theChokePoint != NULL);
        if (!theChokePoint)
            continue;
        EEAICaptureArea(theRecord, *theChokePoint);
        theRecord.I32((int)theChokePoint->GetIndex());
        theRecord.U32(theChokePoint->GetContinentID());
        EEAICaptureULongs(theRecord, theChokePoint->GetAreaSerials());
    }

    // the hills
    theRecord.I32((int)theManager.mHills.size());
    for (i = 0; i < theManager.mHills.size(); i++) {
        EEAIHill* theHill = theManager.mHills[i];
        theRecord.I32(theHill != NULL);
        if (!theHill)
            continue;
        EEAICaptureArea(theRecord, *theHill);
        theRecord.U32(theHill->GetContinentID());
        theRecord.U32(theHill->GetID());
    }

    // the forests
    theRecord.I32((int)theManager.mForests.size());
    for (i = 0; i < theManager.mForests.size(); i++) {
        EEAIForest* theForest = theManager.mForests[i];
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
    theRecord.I32((int)theManager.mRectangleAreas.size());
    for (i = 0; i < theManager.mRectangleAreas.size(); i++) {
        EEAIRectangleArea* theArea = theManager.mRectangleAreas[i];
        theRecord.I32(theArea != NULL);
        if (theArea)
            EEAICaptureArea(theRecord, *theArea);
    }

    // the shore tiles, the free ids, the dirty sectors' tiles
    EEAICapturePoints(theRecord, theManager.mShoreTiles);
    EEAICapturePoints(theRecord, theManager.mIceShoreTiles);
    EEAICaptureShorts(theRecord, theManager.mFreeSubSectorIDs);
    EEAICaptureShorts(theRecord, theManager.mFreePathAreaIDs);
    theRecord.I32((int)theManager.mSectorTiles.size());
    for (std::map<unsigned short, std::vector<EETileCoordinate> >::const_iterator theIt = theManager.mSectorTiles.begin();
         theIt != theManager.mSectorTiles.end(); ++theIt) {
        theRecord.U32(theIt->first);
        EEAICapturePoints(theRecord, theIt->second);
    }
    theRecord.I32((int)theManager.mAreas.size());
    for (std::map<unsigned long, EEAIArea*>::const_iterator theIt = theManager.mAreas.begin();
         theIt != theManager.mAreas.end(); ++theIt) {
        theRecord.U32((unsigned int)theIt->first);
        theRecord.I32(theIt->second ? (int)theIt->second->GetAreaType() : -1);
    }
    theRecord.Commit();
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
