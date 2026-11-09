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

// FUNCTION: EXE 0x410d5d
// Creates the choke point flood fill goal with no tiles collected; choke point numbers start at 1
EEAIAStarChokePointGoal::EEAIAStarChokePointGoal()
    : mWidth(0), mHeight(0), mTileCount(0), mNextIndex(1)
{
}

// FUNCTION: EXE 0x410db7
// Attaches the goal to the map, storage and machine and takes the map's size as the grid size
void EEAIAStarChokePointGoal::Attach(EEMap* inMap, EEAIAStarChokePointStorage* inStorage, EEAIAStarMachineBase* inMachine)
{
    EEAIAStarGoalBase<EEAIAStarChokePointStorage, EEMap>::Attach(inMap, inStorage, inMachine);
    this->mHeight = this->mWidth = inMap->mSize;
}

// FUNCTION: EXE 0x410ddb
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

// FUNCTION: EXE 0x410e6c
// Builds a choke point from the collected tiles (hull, bounds, unique area serials, continent), files it under the
// current number, marks its bounds dirty in the storage and starts the next number
void EEAIAStarChokePointGoal::CreateChokePoint()
{
    EEAIChokePoint* theChokePoint = new EEAIChokePoint;
    theChokePoint->mIndex = this->mNextIndex;
    theChokePoint->mAreaType = kAIAreaChokePoint;
    theChokePoint->Create(this->mTiles);
    theChokePoint->UpdateBounds(gWorld.mInfluenceTable);
    std::sort(this->mAreaSerials.begin(), this->mAreaSerials.end());
    std::vector<unsigned long>::iterator theEnd = std::unique(this->mAreaSerials.begin(), this->mAreaSerials.end());
    if (theEnd != this->mAreaSerials.end())
        this->mAreaSerials.erase(theEnd, this->mAreaSerials.end());
    theChokePoint->AddAreaSerials(this->mAreaSerials);
    if (this->mTiles.size() > 0)
        theChokePoint->mContinentID = gContinentManager.GetContinentID(this->mTiles.front().GetX(), this->mTiles.front().GetY());
    this->mStorage->SetDirtyBounds(theChokePoint->mHull.GetBounds());
    this->mTileCount = 0;
    this->mTiles.clear();
    this->mAreaSerials.clear();
    this->mChokePoints.insert(std::map<long, EEAIChokePoint*>::value_type(this->mNextIndex, theChokePoint));
    this->mNextIndex++;
}

// FUNCTION: EXE 0x410fcc
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

// FUNCTION: EXE 0x41118f
// Destroys the goal (members only)
EEAIAStarChokePointGoal::~EEAIAStarChokePointGoal()
{
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// The base ctor 0x11028, EEAIChokePoint ctor 0x1109a/??_G 0x11157, the goal and base ??_G, the map and sort/unique
// helpers are generated here. Not generated: EEAIArea ctor 0x110cb/??_G 0x11107/dtor 0x11123 (out of line in
// ee/EEAIArea.h; inline COMDATs in the original: their first user is EEAIChokePoint's ctor) and therefore the
// U2DPolygon<long> ctor 0x11251 and the U2DConvexHull<long>/U2DPolygon<long> vtable members 0x1120f..0x153c5 with
// their std helpers (lle/U2DPolygon.h bodies; unnamed).
// TEMPLATE: EXE 0x411028
// TEMPLATE: EXE 0x411062
// TEMPLATE: EXE 0x41109a
// TEMPLATE: EXE 0x4110cb
// TEMPLATE: EXE 0x411107
// TEMPLATE: EXE 0x411123
// TEMPLATE: EXE 0x411157
// TEMPLATE: EXE 0x411173
// TEMPLATE: EXE 0x4111b8
// TEMPLATE: EXE 0x4111e4
// TEMPLATE: EXE 0x41120f
// TEMPLATE: EXE 0x411234
// TEMPLATE: EXE 0x411251
// TEMPLATE: EXE 0x411285
// TEMPLATE: EXE 0x4112f6
// TEMPLATE: EXE 0x411328
// TEMPLATE: EXE 0x411343
// TEMPLATE: EXE 0x411421
// TEMPLATE: EXE 0x411a18
// TEMPLATE: EXE 0x411ca4
// TEMPLATE: EXE 0x411cb1
// TEMPLATE: EXE 0x411d72
// TEMPLATE: EXE 0x411df2
// TEMPLATE: EXE 0x411e58
// TEMPLATE: EXE 0x411ecb
// TEMPLATE: EXE 0x41209a
// TEMPLATE: EXE 0x412102
// TEMPLATE: EXE 0x41214b
// TEMPLATE: EXE 0x41224f
// TEMPLATE: EXE 0x4123d1
// TEMPLATE: EXE 0x412412
// TEMPLATE: EXE 0x4124ec
// TEMPLATE: EXE 0x41270e
// TEMPLATE: EXE 0x41271a
// TEMPLATE: EXE 0x41281b
// TEMPLATE: EXE 0x412827
// TEMPLATE: EXE 0x412877
// TEMPLATE: EXE 0x412922
// TEMPLATE: EXE 0x41294a
// TEMPLATE: EXE 0x412975
// TEMPLATE: EXE 0x412a2b
// TEMPLATE: EXE 0x412a65
// TEMPLATE: EXE 0x412a8a
// TEMPLATE: EXE 0x412af4
// TEMPLATE: EXE 0x412b56
// TEMPLATE: EXE 0x412bc7
// TEMPLATE: EXE 0x412bdd
// TEMPLATE: EXE 0x412c0f
// TEMPLATE: EXE 0x412da3
// TEMPLATE: EXE 0x412dcc
// TEMPLATE: EXE 0x412e04
// TEMPLATE: EXE 0x412e38
// TEMPLATE: EXE 0x412e72
// TEMPLATE: EXE 0x412ec4
// TEMPLATE: EXE 0x412f22
// TEMPLATE: EXE 0x413047
// TEMPLATE: EXE 0x41308c
// TEMPLATE: EXE 0x41313a
// TEMPLATE: EXE 0x413194
// TEMPLATE: EXE 0x4131b9
// TEMPLATE: EXE 0x413278
// TEMPLATE: EXE 0x4132a5
// TEMPLATE: EXE 0x4132c4
// TEMPLATE: EXE 0x4132e5
// TEMPLATE: EXE 0x413390
// TEMPLATE: EXE 0x41362d
// TEMPLATE: EXE 0x413677
// TEMPLATE: EXE 0x41369e
// TEMPLATE: EXE 0x413710
// TEMPLATE: EXE 0x413732
// TEMPLATE: EXE 0x41381d
// TEMPLATE: EXE 0x4139d8
// TEMPLATE: EXE 0x4139e5
// TEMPLATE: EXE 0x413aef
// TEMPLATE: EXE 0x413afd
// TEMPLATE: EXE 0x413bbd
// TEMPLATE: EXE 0x413be2
// TEMPLATE: EXE 0x413ca6
// TEMPLATE: EXE 0x413ce5
// TEMPLATE: EXE 0x413d1d
// TEMPLATE: EXE 0x413d6a
// TEMPLATE: EXE 0x413df1
// TEMPLATE: EXE 0x413e28
// TEMPLATE: EXE 0x413e5b
// TEMPLATE: EXE 0x413f39
// TEMPLATE: EXE 0x413f5c
// TEMPLATE: EXE 0x413f8a
// TEMPLATE: EXE 0x413fb6
// TEMPLATE: EXE 0x413fea
// TEMPLATE: EXE 0x41403a
// TEMPLATE: EXE 0x41409e
// TEMPLATE: EXE 0x4140d2
// TEMPLATE: EXE 0x4141f3
// TEMPLATE: EXE 0x414245
// TEMPLATE: EXE 0x414264
// TEMPLATE: EXE 0x41428d
// TEMPLATE: EXE 0x414365
// TEMPLATE: EXE 0x4143a3
// TEMPLATE: EXE 0x414454
// TEMPLATE: EXE 0x41448b
// TEMPLATE: EXE 0x4144be
// TEMPLATE: EXE 0x4144fe
// TEMPLATE: EXE 0x41453f
// TEMPLATE: EXE 0x414564
// TEMPLATE: EXE 0x414646
// TEMPLATE: EXE 0x41468b
// TEMPLATE: EXE 0x4146df
// TEMPLATE: EXE 0x414806
// TEMPLATE: EXE 0x41485a
// TEMPLATE: EXE 0x41491a
// TEMPLATE: EXE 0x414954
// TEMPLATE: EXE 0x414995
// TEMPLATE: EXE 0x414ab0
// TEMPLATE: EXE 0x414ae6
// TEMPLATE: EXE 0x414b40
// TEMPLATE: EXE 0x414b65
// TEMPLATE: EXE 0x414b8a
// TEMPLATE: EXE 0x414be4
// TEMPLATE: EXE 0x414c35
// TEMPLATE: EXE 0x414c48
// TEMPLATE: EXE 0x414c7d
// TEMPLATE: EXE 0x414cec
// TEMPLATE: EXE 0x414d59
// TEMPLATE: EXE 0x414d8d
// TEMPLATE: EXE 0x414db2
// TEMPLATE: EXE 0x414e0b
// TEMPLATE: EXE 0x414e34
// TEMPLATE: EXE 0x414ebd
// TEMPLATE: EXE 0x414f28
// TEMPLATE: EXE 0x414f49
// TEMPLATE: EXE 0x414f82
// TEMPLATE: EXE 0x414fb3
// TEMPLATE: EXE 0x41507e
// TEMPLATE: EXE 0x4150f8
// TEMPLATE: EXE 0x41513e
// TEMPLATE: EXE 0x4151cd
// TEMPLATE: EXE 0x41523a
// TEMPLATE: EXE 0x415281
// TEMPLATE: EXE 0x415303
// TEMPLATE: EXE 0x41536b
// TEMPLATE: EXE 0x4153c5
