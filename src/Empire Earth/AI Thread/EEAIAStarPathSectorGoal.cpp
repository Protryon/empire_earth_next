// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathSectorGoal.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarPathSectorGoal, vtable 0x43a0a8). The sub-sector flood fill's goal: 0x1a9ec..0x1b7dc (EEAIAStarPathGraphGoal's
// base ??_G ends at 0x1a9ec; EEAIAStarPathSectorStorage's ctor starts at 0x1b7dc). ctor, CollectTiles, CreateSubSector,
// AcceptTile and its detailed test, then the base ctor, ??_G, dtor and the map COMDATs.
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
#include "ee/EEAIAStarPathSectorStorage.h"
#include "ee/EEAIAStarPathAreaStorage.h"
#include "lle/U2DRectangle.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIAStarMachine.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarCapture.h"
#include "ee/EEAIAStarPathSectorGoal.h"

// 0x29ced (cdecl): non-zero when a resource unit stands on inTile (EEAIAStarPathAreaGoal.cpp declares it too)
long EEAIHasResourceUnit(EETile* inTile);

// FUNCTION: EXE 0x41a9ec
// Creates the sub-sector flood-fill goal with no sector, no path area storage, no tiles and no owner unit
EEAIAStarPathSectorGoal::EEAIAStarPathSectorGoal()
    : mSector(NULL), mPathAreaStorage(NULL), mTileCount(0), mUnitID(-1), mPlayerID(0), mWA0(0), mWA2(0), mKind(kAITileNone), mContinentID(0), mCategory(kContinentCategoryNone), mPathAreaID(0),
      mNextID(1), mPathAreaKind(kAITileNone), mClassifyTiles(false)
{
}

// FUNCTION: EXE 0x41aac4
// Takes a new sub-sector id from the continent manager, stamps it on every closed and open node's tile and
// collects those tiles
void EEAIAStarPathSectorGoal::CollectTiles()
{
    unsigned short theID = this->mNextID = gContinentManager.NewSubSectorID();
    this->mTiles.reserve(this->mTileCount);
    // the closed list, then the open list (the storage's order)
    std::vector<EEAIAStarNodeView> theNodes;
    this->mStorage->ListNodes(kAStarListFastClosed, theNodes);
    this->mStorage->ListNodes(kAStarListFastOpen, theNodes);
    for (size_t i = 0; i < theNodes.size(); i++) {
        const EEAIAStarNodeView& theNode = theNodes[i];
        this->mStorage->mSubSectorIDs[this->mStorage->mHeight * theNode.mX + theNode.mY] = theID;
        this->mTiles.push_back(U2DPoint<long>(theNode.mX, theNode.mY));
    }
}

// NEAR: differs in the sector's map insert (call target ICF name) and the storage bounds' store schedule
// FUNCTION: EXE 0x41ab68
// Builds a sub-sector from the sorted filled tiles, adds it to the sector and the goal's map, marks the bounds
// dirty and resets the fill state
void EEAIAStarPathSectorGoal::CreateSubSector()
{
    EEAIPathSubSector* theSubSector = new EEAIPathSubSector;
    std::sort(this->mTiles.begin(), this->mTiles.end());
    theSubSector->mSector = this->mSector;
    theSubSector->mID = this->mNextID;
    theSubSector->mContinentID = this->mContinentID;
    theSubSector->mPathAreaID = this->mPathAreaID;
    theSubSector->mNumTiles = this->mTileCount;
    theSubSector->CreateTiles(this->mTiles);
    theSubSector->mKind = !this->mClassifyTiles ? this->mPathAreaKind : this->mKind;
    this->mSector->mSubSectors.insert(std::map<unsigned short, EEAIPathSubSector*>::value_type(theSubSector->mID, theSubSector));
    this->mTileCount = 0;
    this->mTiles.clear();
    this->mSubSectors.insert(std::map<unsigned short, EEAIPathSubSector*>::value_type(this->mNextID, theSubSector));
    this->mStorage->SetDirtyBounds(this->mTileBounds);
    this->mUnitID = -1;
    this->mContinentID = 0;
    this->mCategory = kContinentCategoryNone;
    this->mPathAreaID = 0;
    this->mPathAreaKind = kAITileNone;
    this->mWA0 = 0;
    this->mPlayerID = 0;
    this->mKind = kAITileNone;
}

// FUNCTION: EXE 0x41acbf
// Decides whether the fill takes a tile (ee_astar area_goals::path_sector_accept with its ClassifyTile 0x41ae1d, over
// the goal's fill state; the machine's corner checks as it leaves them, a stale tile to mStaleTiles)
bool EEAIAStarPathSectorGoal::AcceptTile(unsigned long inX, unsigned long inY, long inStart)
{
    EeAStarPathSectorFill theFill;
    theFill.path_area_storage = this->GetPathAreaStorage();
    theFill.classify_tiles = this->mClassifyTiles;
    theFill.tile_count = this->mTileCount;
    theFill.tile_bounds = EEAIAStarRectOf(this->mTileBounds);
    theFill.sector_bounds = EEAIAStarRectOf(this->mSectorBounds);
    theFill.path_area_id = this->mPathAreaID;
    theFill.path_area_kind = this->mPathAreaKind;
    theFill.continent_id = this->mContinentID;
    theFill.category = this->mCategory;
    theFill.kind = this->mKind;
    theFill.unit_id = this->mUnitID;
    theFill.player_id = this->mPlayerID;
    theFill.wa0 = this->mWA0;
    theFill.wa2 = this->mWA2;
    theFill.check_corners = -1;
    theFill.stale = false;
    EeAStarViewCallbacks theCallbacks = this->mView.Callbacks();
    bool theResult = ee_astar_path_sector_accept(&theFill, &theCallbacks, (int32_t)inX, (int32_t)inY);
    this->mTileCount = theFill.tile_count;
    this->mTileBounds.mBottom = theFill.tile_bounds.bottom;
    this->mTileBounds.mLeft = theFill.tile_bounds.left;
    this->mTileBounds.mRight = theFill.tile_bounds.right;
    this->mTileBounds.mTop = theFill.tile_bounds.top;
    this->mPathAreaID = theFill.path_area_id;
    this->mPathAreaKind = theFill.path_area_kind;
    this->mContinentID = theFill.continent_id;
    this->mCategory = theFill.category;
    this->mKind = theFill.kind;
    this->mUnitID = theFill.unit_id;
    this->mPlayerID = theFill.player_id;
    this->mWA0 = theFill.wa0;
    if (theFill.check_corners >= 0) {
        EEAIAStarMachineBase* theMachine = dynamic_cast<EEAIAStarMachineBase*>(this->mStorage);
        if (theMachine)
            theMachine->mCheckCorners = theFill.check_corners != 0;
    }
    if (theFill.stale) {
        EETileCoordinate theCoordinate;
        theCoordinate.mX = inX;
        theCoordinate.mY = inY;
        this->mStaleTiles.push_back(theCoordinate);
    }
    return theResult;
}

// FUNCTION: EXE 0x41b6a8
// Destroys the goal and its tile lists and sub-sector map (members only)
EEAIAStarPathSectorGoal::~EEAIAStarPathSectorGoal()
{
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x41b652
// SYNTHETIC: EXE 0x41b68c
// TEMPLATE: EXE 0x41b6d9
// TEMPLATE: EXE 0x41b704
// TEMPLATE: EXE 0x41b71c
// TEMPLATE: EXE 0x41b739
// TEMPLATE: EXE 0x41b7aa
