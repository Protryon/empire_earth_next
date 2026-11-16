// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathAreaGoal.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarPathAreaGoal, vtable 0x439f7c). The path-area flood fill's goal: 0x173cc..0x19072 (EEAIAStarHillStorage.cpp's
// COMDATs end at 0x173cc; EEAIAStarPathAreaStorage's ctor starts at 0x19072). ctor, the members the machine calls (the
// tile kinds: 1 land, 2/3 water/terrain 9, 4 forest, 6 map edge, 7..9 terrain 10 by the manager's tile value, 10/11
// land by value, 0xc..0xe a unit's tiles, 0xf terrain 0xc), then the COMDATs of their first users: the base ctor,
// EEAIPathArea's inline ctor/??_G, the maps/sets and vector<unsigned short> helpers, the dtor.
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

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEAIAStarPathAreaStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarCapture.h"
#include "ee/EEAIAStarPathAreaGoal.h"
#include "ee/EEAIAStarMachine.h"

// EEAIContinentManager.cpp's inline (out of line here: 0x29ced)
long EEAIHasResourceUnit(EETile* inTile);

// EEAIContinent's inline kind-id adders (COMDATs 0x1887f..0x189a2 after AssignKindIDs): ee_astar continent.rs

// Creates the path-area flood-fill goal with no area grid, no owner, no tiles and the area/forest ids starting at 1
EEAIAStarPathAreaGoal::EEAIAStarPathAreaGoal()
    : mAreas(NULL), mOwnerID(-1), mPlayerID(0), mTileCount(0), mResources1(0), mResources2(0), mResources3(0),
      mContinentID(0), mContinentKind(kContinentCategoryNone), mForestID(0), mNextForestID(1), mNextPathAreaID(1), mKind(kAITileNone), mMode(kAIPathNone),
      mNextKindID(0)
{
}

// Stamps every closed and open node's tile with the next path area id and collects those tiles into the area's list
void EEAIAStarPathAreaGoal::CollectTiles()
{
    unsigned short theID = this->mNextPathAreaID;
    this->mTiles.reserve(this->mTileCount);
    // the closed list, then the open list (the storage's order)
    std::vector<EEAIAStarNodeView> theNodes;
    this->mStorage->ListNodes(kAStarListFastClosed, theNodes);
    this->mStorage->ListNodes(kAStarListFastOpen, theNodes);
    for (size_t i = 0; i < theNodes.size(); i++) {
        const EEAIAStarNodeView& theNode = theNodes[i];
        this->mStorage->mPathAreaIDs[this->mStorage->mHeight * theNode.mX + theNode.mY] = theID;
        this->mTiles.push_back(U2DPoint<long>(theNode.mX, theNode.mY));
    }
}

// Turns the filled tiles into a new path area (plus a forest with its resource counts for forest tiles) or, in a
// kind pass, assigns the next kind id; then marks the bounds dirty and resets the fill state for the next area
void EEAIAStarPathAreaGoal::CreateAreas()
{
    if (this->mMode == kAIPathNone) {
        EEAIPathArea* theArea = new EEAIPathArea;
        theArea->SetID(this->mNextPathAreaID);
        theArea->SetContinentID(this->mContinentID);
        theArea->SetAreaType(kAIAreaPathArea);
        theArea->SetKind(this->mKind);
        theArea->Create(this->mTiles);
        theArea->UpdateBounds(gWorld.mInfluenceTable);
        this->mPathAreas.insert(std::map<unsigned short, EEAIPathArea*>::value_type(this->mNextPathAreaID, theArea));
        this->mNextPathAreaID++;
        if (this->mKind == kAITileForest) {
            EEAIForest* theForest = new EEAIForest;
            theForest->SetID(this->mNextForestID);
            theForest->SetContinentID(this->mContinentID);
            theForest->SetAreaType(kAIAreaForest);
            theForest->Create(this->mTiles);
            theForest->UpdateBounds(gWorld.mInfluenceTable);
            theForest->SetNumResources1(this->mResources1);
            theForest->SetNumResources3(this->mResources3);
            theForest->SetNumResources2(this->mResources2);
            theForest->UpdateResourceKind();
            this->mForests.insert(std::map<unsigned short, EEAIForest*>::value_type(this->mNextForestID, theForest));
            this->mNextForestID++;
        } else if (this->mKind == kAITileUnitObstacle)
            theArea->SetUnitID(this->mOwnerID);
    } else {
        this->mNextKindID++;
        this->AssignKindIDs();
    }
    this->mStorage->SetDirtyBounds(this->mTileBounds);
    this->mTiles.clear();
    this->mTileCount = 0;
    this->mContinentKind = kContinentCategoryNone;
    this->mOwnerID = -1;
    this->mContinentID = 0;
    this->mForestID = 0;
    this->mKind = kAITileNone;
    this->mResources1 = 0;
    this->mResources2 = 0;
    this->mResources3 = 0;
}

// The set's insert result is unused; the continents get the id through their kind vectors.
// Gives every path area under the filled tiles the next water-body, space-region, land-mass or surface-region id
// (by mode) and adds that id to each touched continent's matching id list
void EEAIAStarPathAreaGoal::AssignKindIDs()
{
    std::set<EEAIPathArea*> theAreas;   // (address order, as the VC6 set of addresses)
    for (long i = this->mTiles.size() - 1; i >= 0; i--) {
        EEAIPathArea* theArea = this->GetArea((*(this->mTiles.begin() + i)).GetX(), (*(this->mTiles.begin() + i)).GetY());
        theAreas.insert(theArea);
        if (this->mMode == kAIPathWaterBody)
            theArea->SetWaterBodyID(this->GetNextKindID());
        else if (this->mMode == kAIPathSpaceRegion)
            theArea->SetSpaceRegionID(this->GetNextKindID());
        else if (this->mMode == kAIPathLandMass)
            theArea->SetLandMassID(this->GetNextKindID());
        else
            theArea->SetSurfaceRegionID(this->GetNextKindID());
    }
    for (std::set<EEAIPathArea*>::iterator theIt = theAreas.begin(); theIt != theAreas.end(); theIt++) {
        EEAIContinent* theContinent = gContinentManager.GetContinentFromID((*theIt)->GetContinentID());
        if (!theContinent)
            continue;
        if (this->mMode == kAIPathWaterBody)
            theContinent->AddIDBC(this->GetNextKindID());
        else if (this->mMode == kAIPathSpaceRegion)
            theContinent->AddIDDC(this->GetNextKindID());
        else if (this->mMode == kAIPathLandMass)
            theContinent->AddIDCC(this->GetNextKindID());
        else
            theContinent->AddIDEC(this->GetNextKindID());
    }
}

// Decides whether the flood fill takes a tile into the current area and, if so, counts it and grows the bounds
// (ee_astar area_goals::path_area_accept over the goal's fill state; the machine's corner checks as it leaves them)
bool EEAIAStarPathAreaGoal::AcceptTile(long inX, long inY, bool inStart)
{
    EeAStarPathAreaFill theFill;
    theFill.mode = this->mMode;
    theFill.continent_id = this->mContinentID;
    theFill.continent_kind = this->mContinentKind;
    theFill.kind = this->mKind;
    theFill.owner_id = this->mOwnerID;
    theFill.player_id = this->mPlayerID;
    theFill.tile_count = this->mTileCount;
    theFill.resources[0] = this->mResources1;
    theFill.resources[1] = this->mResources2;
    theFill.resources[2] = this->mResources3;
    theFill.forest_id = this->mForestID;
    theFill.next_forest_id = this->mNextForestID;
    theFill.tile_bounds = EEAIAStarRectOf(this->mTileBounds);
    theFill.check_corners = -1;
    EeAStarViewCallbacks theCallbacks = this->mView.Callbacks();
    bool theResult = ee_astar_path_area_accept(&theFill, &theCallbacks, inX, inY);
    this->mContinentID = theFill.continent_id;
    this->mContinentKind = theFill.continent_kind;
    this->mKind = theFill.kind;
    this->mOwnerID = theFill.owner_id;
    this->mPlayerID = theFill.player_id;
    this->mTileCount = theFill.tile_count;
    this->mResources1 = theFill.resources[0];
    this->mResources2 = theFill.resources[1];
    this->mResources3 = theFill.resources[2];
    this->mForestID = theFill.forest_id;
    this->mTileBounds.mBottom = theFill.tile_bounds.bottom;
    this->mTileBounds.mLeft = theFill.tile_bounds.left;
    this->mTileBounds.mRight = theFill.tile_bounds.right;
    this->mTileBounds.mTop = theFill.tile_bounds.top;
    if (theFill.check_corners >= 0) {
        EEAIAStarMachineBase* theMachine = dynamic_cast<EEAIAStarMachineBase*>(this->mStorage);
        if (theMachine)
            theMachine->mCheckCorners = theFill.check_corners != 0;
    }
    return theResult;
}

// 0x4184d1 (the 4-argument ClassifyTile wrapper): in ee_astar's path_area_accept

// Returns the path area at tile (inX, inY), or NULL off the grid
// Not inline: AssignKindIDs calls it.
EEAIPathArea* EEAIAStarPathAreaGoal::GetArea(unsigned long inX, unsigned long inY)
{
    return inX < this->mAreasSize && inY < this->mAreasSize ? this->mAreas[this->mAreasSize * inX + inY] : 0;
}

// Destroys the goal and its tile list and area/forest maps (members only)
EEAIAStarPathAreaGoal::~EEAIAStarPathAreaGoal()
{
}
