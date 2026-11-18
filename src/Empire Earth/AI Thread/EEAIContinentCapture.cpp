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

// CNTR's bytes and CNTQ's digest come from the records (ee_sim::astar cm_io.rs capture_fill / capture_digest)
void EEAIContinentCapture(unsigned long inTag)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord((unsigned int)inTag);
    EeAStarCmBytes* theBytes = ee_astar_cm_capture_fill(&gContinentManager.mRecord);
    size_t theCount;
    const uint8_t* theData = ee_astar_cm_bytes_data(theBytes, &theCount);
    theRecord.Bytes(theData, (unsigned int)theCount);
    ee_astar_cm_bytes_free(theBytes);
    theRecord.Commit();
}

unsigned long long EEAIContinentDigest()
{
    return ee_astar_cm_capture_digest(&gContinentManager.mRecord);
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
