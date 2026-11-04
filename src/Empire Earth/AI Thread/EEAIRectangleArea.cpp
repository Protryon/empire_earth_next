// C:\EEX\Empire Earth\AI Thread\EEAIRectangleArea.cpp (file name inferred: no __FILE__; RTTI class EEAIRectangleArea,
// vtable 0x43a540). A rectangular AI map area: 0x2c79f..0x2cc87 (EEAIPathSubSector.cpp ends at 0x2c79f;
// EEArtificialIntelligence.cpp starts at 0x2cc87). ctor, the continent count, the two random-point slots, then the
// map/random_shuffle COMDATs and ??_G.
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

#include "ee/EEAIRectangleArea.h"
#include "ee/EEAIRandom.h"

// FUNCTION: EXE 0x42c79f
// Creates an empty rectangle area with no scenario area, tile counts or continent
EEAIRectangleArea::EEAIRectangleArea()
    : mScenarioAreaID(0), mNumLandTiles(0), mNumWaterTiles(0), mNumSpaceTiles(0), mContinentID(0), mID(0)
{
}

// NEAR: the original calls the map's _Tree(pred, alloc) ctor out of line (ours expands it and refuses _Tree_val one
// level deeper) and keeps the inner loop counter in edi; EE_LOST_DEBUG 1..16 does not close it
// FUNCTION: EXE 0x42c7df
// Recounts the rectangle's land, water and space tiles and stores and returns the continent with the most
// of its tiles (the lowest id on a tie; 0 when none)
unsigned short EEAIRectangleArea::UpdateContinents()
{
    std::map<unsigned short, long> theCounts;
    std::map<unsigned short, long>::iterator theCount;
    EEMap* theMap = gWorld.mMap;
    unsigned short theResult;
    this->mNumLandTiles = 0;
    this->mNumWaterTiles = 0;
    this->mNumSpaceTiles = 0;
    for (long theX = this->mRect.mLeft; theX <= this->mRect.mRight; theX++) {
        for (long theY = this->mRect.mTop; theY <= this->mRect.mBottom; theY++) {
            if (!theMap->GetTile(theX, theY))
                continue;
            unsigned short theID = gContinentManager.GetContinentID(theX, theY);
            EEAIContinent* theContinent = theID ? gContinentManager.GetContinentByID(theID) : NULL;
            if (!theContinent)
                continue;
            if (theContinent->GetCategory() == kContinentCategoryLand)
                this->mNumLandTiles++;
            else if (theContinent->GetCategory() == kContinentCategoryWater)
                this->mNumWaterTiles++;
            else if (theContinent->GetCategory() == kContinentCategorySpace)
                this->mNumSpaceTiles++;
            theCount = theCounts.find((long)theContinent->mID);
            if (theCount != theCounts.end())
                theCount->second++;
            else
                theCounts.insert(std::make_pair(theContinent->mID, 1L));
        }
    }
    theResult = 0;
    long theMost = 0;   // the continent covering most of it (was: the highest id present)
    for (theCount = theCounts.begin(); theCount != theCounts.end(); ++theCount) {
        if (theCount->second > theMost) {
            theMost = theCount->second;
            theResult = theCount->first;
        }
    }
    this->mContinentID = theResult;
    return theResult;
}

// NEAR: the original returns EEAIArea::GetRandomPoint() straight into the hidden result; ours copies a temporary (the
// open U2DPoint<long> copy-ctor family)
// FUNCTION: EXE 0x42c929
// Returns a random tile of the area (EEAIArea's pick); the two arguments are unused
U2DPoint<long> EEAIRectangleArea::GetBaseRandomPoint(long inArg1, long inArg2)
{
    return EEAIArea::GetRandomPoint();
}

// NEAR: the original calls RTSMap::GetTile (0x2f85), _Ptrit::operator+ and begin() out of line and builds the
// random point straight in its local (inline-budget / U2DPoint copy family)
// FUNCTION: EXE 0x42c939
// Returns a random area tile; when it is off the map, a random on-map tile of the area instead (unchanged when
// there is none)
U2DPoint<long> EEAIRectangleArea::GetRandomPoint()
{
    std::vector<U2DPoint<long> > thePoints;
    std::vector<U2DPoint<long> > theTiles;
    U2DPoint<long> theResult = EEAIArea::GetRandomPoint();
    EEMap* theMap = gWorld.mMap;
    if (!theMap || theMap->GetTile(theResult.mX, theResult.mY))
        return theResult;
    this->mTiles.BuildPointList(thePoints, 1);
    theTiles.reserve(thePoints.size());
    for (long i = thePoints.size() - 1; i >= 0; i--) {
        if (theMap->GetTile((*(thePoints.begin() + i)).mX, (*(thePoints.begin() + i)).mY))
            theTiles.push_back(thePoints[i]);
    }
    if (theTiles.size() != 0) {
        EEAIRandomShuffle(theTiles.begin(), theTiles.end());
        theResult = theTiles.front();
    }
    return theResult;
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x42ca87
// TEMPLATE: EXE 0x42cab2
// TEMPLATE: EXE 0x42cb23
// TEMPLATE: EXE 0x42cb55
// TEMPLATE: EXE 0x42cb99
// TEMPLATE: EXE 0x42cbc0
// TEMPLATE: EXE 0x42cbf2
// TEMPLATE: EXE 0x42cc49
// SYNTHETIC: EXE 0x42cc65
