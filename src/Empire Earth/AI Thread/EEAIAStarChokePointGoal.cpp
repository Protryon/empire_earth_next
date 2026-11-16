// C:\EEX\Empire Earth\AI Thread\EEAIAStarChokePointGoal.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarChokePointGoal, vtable 0x439d0c). The choke point flood fill's goal: 0x10d5d..0x1540d (EEAIAStarBoundedStorage
// ends at 0x10d5d; EEAIAStarChokePointStorage's ctor starts at 0x1540d). ctor, Virtual10, the members the machine
// calls, then the COMDATs of their first users: the base ctor, EEAIChokePoint's and EEAIArea's inline ctors/??_G, the
// dtor, and the U2DConvexHull<long>/U2DPolygon<long> vtables' members (vtables 0x439d9c/0x439de4: EEAIArea's mHull is
// first constructed here) with their std helpers.
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
#include "ee/EEMathInline.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEAIAStarChokePointStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarCapture.h"
#include "ee/EEAIAStarChokePointGoal.h"

// Creates the choke point flood fill goal with no tiles collected; choke point numbers start at 1
EEAIAStarChokePointGoal::EEAIAStarChokePointGoal()
    : mWidth(0), mHeight(0), mTileCount(0), mNextIndex(1)
{
}

// Attaches the goal to the map, storage and machine and takes the map's size as the grid size
void EEAIAStarChokePointGoal::Attach(EEMap* inMap, EEAIAStarChokePointStorage* inStorage, EEAIAStarMachineBase* inMachine)
{
    EEAIAStarGoalBase<EEAIAStarChokePointStorage, EEMap>::Attach(inMap, inStorage, inMachine);
    this->mHeight = this->mWidth = inMap->mSize;
}

// Stamps every closed and open node's tile with the current choke point number and collects those tiles
void EEAIAStarChokePointGoal::CollectTiles()
{
    this->mTiles.reserve(this->mTileCount);
    // the closed list, then the open list (the storage's order)
    std::vector<EEAIAStarNodeView> theNodes;
    this->mStorage->ListNodes(kAStarListFastClosed, theNodes);
    this->mStorage->ListNodes(kAStarListFastOpen, theNodes);
    for (size_t i = 0; i < theNodes.size(); i++) {
        const EEAIAStarNodeView& theNode = theNodes[i];
        this->mStorage->mChokePointIDs[this->mStorage->mHeight * theNode.mX + theNode.mY] = this->mNextIndex;
        this->mTiles.push_back(U2DPoint<long>(theNode.mX, theNode.mY));
    }
}

// Builds a choke point from the collected tiles (hull, bounds, unique area serials, continent), files it under the
// current number, marks its bounds dirty in the storage and starts the next number
void EEAIAStarChokePointGoal::CreateChokePoint()
{
    EEAIChokePoint* theChokePoint = new EEAIChokePoint;
    theChokePoint->SetIndex(this->mNextIndex);
    theChokePoint->SetAreaType(kAIAreaChokePoint);
    theChokePoint->Create(this->mTiles);
    theChokePoint->UpdateBounds(gWorld.mInfluenceTable);
    std::sort(this->mAreaSerials.begin(), this->mAreaSerials.end());
    std::vector<unsigned long>::iterator theEnd = std::unique(this->mAreaSerials.begin(), this->mAreaSerials.end());
    if (theEnd != this->mAreaSerials.end())
        this->mAreaSerials.erase(theEnd, this->mAreaSerials.end());
    theChokePoint->AddAreaSerials(this->mAreaSerials);
    if (this->mTiles.size() > 0)
        theChokePoint->SetContinentID(gContinentManager.GetContinentID(this->mTiles.front().GetX(), this->mTiles.front().GetY()));
    this->mStorage->SetDirtyBounds(theChokePoint->GetHullBounds());
    this->mTileCount = 0;
    this->mTiles.clear();
    this->mAreaSerials.clear();
    this->mChokePoints.insert(std::map<long, EEAIChokePoint*>::value_type(this->mNextIndex, theChokePoint));
    this->mNextIndex++;
}

// Returns whether the flood fill takes a tile: on the map, flagged as choke and not of kind 0x36; counts it and
// records its area serial
bool EEAIAStarChokePointGoal::AcceptTile(long inX, long inY, bool inStart)
{
    // Rust (ee_astar goals.rs choke_accept): the size and the count go in, the accepted tile's serial comes back
    EeAStarChokeFill theFill;
    theFill.width = this->mWidth;
    theFill.height = this->mHeight;
    theFill.tile_count = this->mTileCount;
    EeAStarViewCallbacks theView = this->mView.Callbacks();
    uint32_t theSerial = 0;
    bool theResult = ee_astar_choke_accept(&theFill, &theView, (int32_t)inX, (int32_t)inY, &theSerial);
    this->mTileCount = theFill.tile_count;
    if (theResult)
        this->mAreaSerials.push_back(theSerial);
    return theResult;
}

// Destroys the goal (members only)
EEAIAStarChokePointGoal::~EEAIAStarChokePointGoal()
{
}
