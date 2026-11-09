// C:\EEX\Empire Earth\AI Thread\EEAIAStarHillGoal.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarHillGoal, vtable 0x439f00). The hill flood fill's goal: 0x16c0e..0x171b5 (EEAIAStarGraphStorage::Pop ends at
// 0x16c0e; EEAIAStarHillStorage's ctor starts at 0x171b5; the name sorts between them). The twin of
// EEAIAStarContinentGoal.cpp: ctor, the three members the machine calls, then the base ctor, the map COMDATs, EEAIHill's
// ??_G, the goal's ??_G/dtor.
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
#include "ee/EEAIAStarHillStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIHill.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarCapture.h"
#include "ee/EEAIAStarHillGoal.h"

// FUNCTION: EXE 0x416c0e
// Creates the hill flood fill goal with no tiles and no continent; hill ids start at 1
EEAIAStarHillGoal::EEAIAStarHillGoal()
    : mTileCount(0), mNextID(1), mContinentID(0)
{
}

// FUNCTION: EXE 0x416c56
// Stamps every closed and open node's tile with the current hill id and continent and collects those tiles
void EEAIAStarHillGoal::CollectTiles()
{
    EEAIAStarHillTile theValue;
    theValue.mContinentID = this->mContinentID;
    theValue.mID = this->mNextID;
    this->mTiles.reserve(this->mTileCount);
    // the closed list, then the open list (the storage's order)
    std::vector<EEAIAStarNodeView> theNodes;
    this->mStorage->ListNodes(kAStarListFastClosed, theNodes);
    this->mStorage->ListNodes(kAStarListFastOpen, theNodes);
    for (size_t i = 0; i < theNodes.size(); i++) {
        const EEAIAStarNodeView& theNode = theNodes[i];
        EEAIAStarHillTile& theTile = this->mStorage->mTiles60[this->mStorage->mHeight * theNode.mX + theNode.mY];
        theTile.mID = theValue.mID;
        theTile.mContinentID = theValue.mContinentID;
        this->mTiles.push_back(U2DPoint<long>(theNode.mX, theNode.mY));
    }
}

// FUNCTION: EXE 0x416d04
// Builds a hill of the current id and continent from the collected tiles, files it, marks its bounds dirty in
// the storage, then starts the next id with no continent
void EEAIAStarHillGoal::CreateHill()
{
    EEAIHill* theHill = new EEAIHill;
    theHill->mID = this->mNextID;
    theHill->mContinentID = this->mContinentID;
    theHill->mAreaType = kAIAreaHill;
    theHill->Create(this->mTiles);
    theHill->UpdateBounds(gWorld.mInfluenceTable);
    this->mStorage->SetDirtyBounds(theHill->mHull.GetBounds());
    this->mTileCount = 0;
    this->mTiles.clear();
    this->mHills.insert(std::map<unsigned short, EEAIHill*>::value_type(this->mNextID, theHill));
    this->mNextID++;
    this->mContinentID = 0;
}

// FUNCTION: EXE 0x416db8
// Returns whether the fill takes a tile: not cliff, height level 4 or more, on the hill's continent (the first
// tile picks it); counts the tile and adds its level to the sum
bool EEAIAStarHillGoal::AcceptTile(unsigned long inX, unsigned long inY, long inStart)
{
    bool theResult = false;
    if (this->mView.HasTile(inX, inY)) {
        long theLevel = this->mView.GetHeightLevel(inX, inY);
        if (this->mView.GetTerrain(inX, inY) != kTerrainCliff && theLevel >= 4) {
            if (this->mContinentID != 0) {
                theResult = this->mView.GetContinentID(inX, inY) == this->mContinentID;
            } else {
                this->mContinentID = this->mView.GetContinentID(inX, inY);
                theResult = true;
            }
            if (theResult) {
                this->mTileCount++;
                this->mLevelSum += theLevel;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4170a9
// Destroys the goal (members only)
EEAIAStarHillGoal::~EEAIAStarHillGoal()
{
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x416e9d
// TEMPLATE: EXE 0x416ed7
// SYNTHETIC: EXE 0x41706b
// SYNTHETIC: EXE 0x41708d
// TEMPLATE: EXE 0x4170ca
// TEMPLATE: EXE 0x4170f5
// TEMPLATE: EXE 0x417112
// TEMPLATE: EXE 0x417183
