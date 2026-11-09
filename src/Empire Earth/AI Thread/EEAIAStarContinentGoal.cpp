// C:\EEX\Empire Earth\AI Thread\EEAIAStarContinentGoal.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarContinentGoal, vtable 0x439e54). The continent flood fill's goal: 0x155c0..0x15a47 (EEAIAStarChokePointStorage
// ends at 0x155c0; EEAIAStarContinentStorage's ctor starts at 0x15a47; the name sorts between them). ctor, the three
// members the machine calls, then the base ctor/??_G/dtor and map COMDATs.
#include <windows.h>
#include <string.h>
#include <vector>
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

#include "ee/EmpireEarth.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEAIAStarContinentStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarContinentGoal.h"

// FUNCTION: EXE 0x4155c0
// Creates the continent flood fill goal with no kind chosen yet; continent ids start at 1
EEAIAStarContinentGoal::EEAIAStarContinentGoal()
    : mTileCount(0), mKind(kContinentCategoryNone), mNextID(1)
{
}

// FUNCTION: EXE 0x415607
// Stamps every closed and open node's tile with the current continent id and kind and collects those tiles
void EEAIAStarContinentGoal::CollectTiles()
{
    EEAIAStarContinentTile theValue;
    theValue.mKind = this->mKind;
    theValue.mID = this->mNextID;
    EEAIAStarNode* theNode;
    this->mTiles.reserve(this->mTileCount);
    for (theNode = this->mStorage->mFastClosedList; theNode; theNode = theNode->mNext) {
        EEAIAStarContinentTile& theTile = this->mStorage->mTiles60[this->mStorage->mWidth * theNode->mX + theNode->mY];
        theTile.mID = theValue.mID;
        theTile.mKind = theValue.mKind;
        this->mTiles.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    }
    for (theNode = this->mStorage->mFastOpenList; theNode; theNode = theNode->mNext) {
        EEAIAStarContinentTile& theTile = this->mStorage->mTiles60[this->mStorage->mWidth * theNode->mX + theNode->mY];
        theTile.mID = theValue.mID;
        theTile.mKind = theValue.mKind;
        this->mTiles.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    }
}

// FUNCTION: EXE 0x4156b2
// Builds a continent of the current id and kind from the collected tiles, files it, marks its bounds dirty in
// the storage, then starts the next id with no kind
void EEAIAStarContinentGoal::CreateContinent()
{
    EEAIContinent* theContinent = new EEAIContinent;
    theContinent->mID = this->mNextID;
    theContinent->mCategory = this->mKind;
    theContinent->mAreaType = kAIAreaContinent;
    theContinent->Create(this->mTiles);
    theContinent->UpdateBounds(gWorld.mInfluenceTable);
    this->mStorage->mDirtyBounds = theContinent->mHull.GetBounds();
    this->mTileCount = 0;
    this->mTiles.clear();
    this->mContinents.insert(std::make_pair(this->mNextID, theContinent));
    this->mNextID++;
    this->mKind = kContinentCategoryNone;
}

// NEAR: the tests are in the original's order (1, 2, 4, 3, 5, 0); block layout and the shared edge test differ
// FUNCTION: EXE 0x41575c
// Returns whether the fill takes a tile of the continent's kind (land, deep water, space, cliff or map edge; the
// first tile picks the kind), counting it; map edge tiles join only map edge continents
bool EEAIAStarContinentGoal::AcceptTile(unsigned long inX, unsigned long inY, bool inStart)
{
    bool theResult = false;
    if (this->mView.HasTile(inX, inY)) {
        long theTerrain = this->mView.GetTerrain(inX, inY);
        bool theMatch = false;
        if (this->mKind == kContinentCategoryLand)
            theMatch = theTerrain != kTerrainDeepWater && theTerrain != kTerrainCliff && theTerrain != kTerrainSpace;
        else if (this->mKind == kContinentCategoryWater)
            theMatch = theTerrain == kTerrainDeepWater;
        else if (this->mKind == kContinentCategorySpace)
            theMatch = theTerrain == kTerrainSpace;
        else if (this->mKind == kContinentCategoryCliff)
            theMatch = theTerrain == kTerrainCliff;
        else if (this->mKind == kContinentCategoryMapEdge)
            theMatch = this->mView.IsEdgeTile(inX, inY);
        else if (this->mKind == kContinentCategoryNone) {
            switch (theTerrain) {
            case kTerrainDeepWater:
                this->mKind = kContinentCategoryWater;
                break;
            case kTerrainCliff:
                this->mKind = kContinentCategoryCliff;
                break;
            case kTerrainSpace:
                this->mKind = kContinentCategorySpace;
                break;
            default:
                this->mKind = kContinentCategoryLand;
                break;
            }
            if (this->mView.IsEdgeTile(inX, inY))
                this->mKind = kContinentCategoryMapEdge;
            theMatch = true;
        }
        if (theMatch) {
            if (this->mView.IsEdgeTile(inX, inY) == (this->mKind == kContinentCategoryMapEdge)) {
                this->mTileCount++;
                theResult = true;
            } else
                theResult = false;
        } else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x41593b
// Destroys the goal (members only)
EEAIAStarContinentGoal::~EEAIAStarContinentGoal()
{
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x4158e5
// SYNTHETIC: EXE 0x41591f
// TEMPLATE: EXE 0x41595c
// TEMPLATE: EXE 0x415987
// TEMPLATE: EXE 0x4159a4
// TEMPLATE: EXE 0x415a15
