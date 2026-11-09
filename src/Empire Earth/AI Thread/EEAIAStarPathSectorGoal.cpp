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

// NEAR: the original calls RTSMap::GetTile 0x2f85 and _Ptrit::operator+ out of line, keeps the result in inY's slot
// FUNCTION: EXE 0x41acbf
// Decides whether the fill takes a tile: by default it must lie in the first tile's path area and the sector's
// bounds; in classify mode ClassifyTile decides. Accepted tiles are counted and grow the bounds
bool EEAIAStarPathSectorGoal::AcceptTile(unsigned long inX, unsigned long inY, long inStart)
{
    bool theResult = false;
    if (!this->mView.HasTile(inX, inY))
        return theResult;
    if (this->GetTileCount() == 0)
        this->mTileBounds.Set(U2DPoint<long>(inX, inY));
    if (!this->GetClassifyTiles()) {
        if (this->GetPathAreaID()) {
            theResult = this->GetPathAreaID() == this->mView.GetStoredPathAreaID(this->GetPathAreaStorage(), inX, inY);
            if (!theResult)
                return theResult;
            if (!this->mSectorBounds.Contains(inX, inY))
                return false;
        } else {
            this->mPathAreaID = this->mView.GetStoredPathAreaID(this->GetPathAreaStorage(), inX, inY);
            EEAIAStarPathAreaInfo theArea = this->mView.GetPathArea(this->GetPathAreaID());
            this->mContinentID = theArea.mContinentID;
            this->mPathAreaKind = theArea.mKind;
            EEAIAStarMachineBase* theMachine = dynamic_cast<EEAIAStarMachineBase*>(this->mStorage);
            if (this->GetPathAreaKind() == kAITileCliff || (this->GetPathAreaKind() > kAITileForest && this->GetPathAreaKind() <= kAITileMapEdge) || (this->GetPathAreaKind() > kAITileNearShore && this->GetPathAreaKind() <= kAITileGate))
                theMachine->mCheckCorners = false;
            else
                theMachine->mCheckCorners = true;
        }
        theResult = true;
    } else {
        theResult = this->ClassifyTile(inX, inY, false);
        if (!theResult)
            return theResult;
    }
    this->mTileCount++;
    this->mTileBounds.Extend(U2DPoint<long>(inX, inY));
    return theResult;
}

// The match test is a jump table over mKind - 1. Every accepted tile not already in a path area of that kind is
// listed in mStaleTiles.
// NEAR: the original calls RTSMap::GetTile 0x2f85 out of line (ours expands it, so the registers shift);
// GetTileFlag's int result type and the case bodies' block order are unchecked
// FUNCTION: EXE 0x41ae1d
// Tests a tile for the classify fill: the first sets the sub-sector's continent and kind from terrain, shore
// distance and blocking units; later ones must match it inside the sector bounds. Returns whether it matched
bool EEAIAStarPathSectorGoal::ClassifyTile(unsigned long inX, unsigned long inY, bool inArg)
{
    bool theResult;
    long theTerrain = this->mView.GetTerrain(inX, inY);
    long theUnitID = -1;
    EEAIAStarContinentInfo theContinent = this->mView.GetContinentAt(inX, inY);
    bool theMatch;

    if (this->mContinentID != 0) {
        if (this->mCategory != theContinent.mCategory || theContinent.mID != this->mContinentID)
            return false;
        switch (this->mKind) {
        case kAITileLand:
            if (theTerrain == kTerrainShallowWater)
                return false;
            theMatch = !this->mView.GetResourceKind(inX, inY);
            if (!theMatch)
                return false;
            if (this->mView.HasBlocker(inX, inY))
                theMatch = false;
            break;
        case kAITileWater:
            if (theTerrain != kTerrainDeepWater || this->mView.GetTileFlag(inX, inY) != 0)
                return false;
            theMatch = true;
            break;
        case kAITileCliff:
            if (theTerrain != kTerrainCliff)
                return false;
            theMatch = true;
            break;
        case kAITileForest:
            theMatch = this->mView.GetResourceKind(inX, inY) != 0;
            break;
        case kAITileGaiaObstacle:
            if (this->mView.GetResourceKind(inX, inY))
                return false;
            theUnitID = this->mView.GetFirstBlockerID(inX, inY);
            theMatch = theUnitID == this->mUnitID;
            break;
        case kAITileMapEdge:
            theMatch = true;
            break;
        case kAITileShallows:
            if (theTerrain != kTerrainShallowWater || this->mView.GetTileFlag(inX, inY) != 0)
                return false;
            theMatch = !this->mView.GetResourceKind(inX, inY);
            if (!theMatch)
                return false;
            if (this->mView.HasBlocker(inX, inY))
                theMatch = false;
            break;
        case kAITileShallowsShore:
            if (theTerrain != kTerrainShallowWater || this->mView.GetTileFlag(inX, inY) >= 0)
                return false;
            theMatch = !this->mView.GetResourceKind(inX, inY);
            if (!theMatch)
                return false;
            if (this->mView.HasBlocker(inX, inY))
                theMatch = false;
            break;
        case kAITileShallowsNearShore:
            if (theTerrain != kTerrainShallowWater || this->mView.GetTileFlag(inX, inY) <= 0)
                return false;
            theMatch = !this->mView.GetResourceKind(inX, inY);
            if (!theMatch)
                return false;
            if (this->mView.HasBlocker(inX, inY))
                theMatch = false;
            break;
        case kAITileShore:
            if ((theTerrain != kTerrainDeepWater && theTerrain != kTerrainSpace) ||
                this->mView.GetTileFlag(inX, inY) >= 0)
                return false;
            theMatch = true;
            break;
        case kAITileNearShore:
            if ((theTerrain != kTerrainDeepWater && theTerrain != kTerrainSpace) ||
                this->mView.GetTileFlag(inX, inY) <= 0)
                return false;
            theMatch = true;
            break;
        case kAITileUnitObstacle:
            if (this->mView.GetResourceKind(inX, inY))
                return false;
            theUnitID = this->mView.GetFirstObstacleID(inX, inY);
            theMatch = theUnitID == this->mUnitID;
            break;
        case kAITileWall:
            if (this->mView.GetResourceKind(inX, inY))
                return false;
            if (!this->mView.HasOwnWallFirst(inX, inY, this->mPlayerID))
                return false;
            theMatch = true;
            break;
        case kAITileGate:
            if (this->mView.GetResourceKind(inX, inY))
                return false;
            theUnitID = this->mView.GetSectorGateOwner(inX, inY, this->mPlayerID);
            theMatch = theUnitID == this->mUnitID;
            break;
        case kAITileSpace:
            if (theTerrain != kTerrainSpace || this->mView.GetTileFlag(inX, inY) != 0)
                return false;
            theMatch = true;
            break;
        default:
            return false;
        }
        if (!theMatch || !this->mSectorBounds.Contains(U2DPoint<long>(inX, inY)))
            return false;
    } else {
        EEAIAStarMachineBase* theMachine = dynamic_cast<EEAIAStarMachineBase*>(this->mStorage);
        theMachine->mCheckCorners = true;
        this->mContinentID = theContinent.mID;
        this->mCategory = theContinent.mCategory;
        EEAIAStarPathAreaInfo theArea = this->mView.GetPathAreaAt(inX, inY);
        this->mPathAreaID = theArea.mFound ? theArea.mID : 0;
        if (this->mCategory == kContinentCategoryLand) {
            if (theTerrain == kTerrainShallowWater) {
                long theFlag = this->mView.GetTileFlag(inX, inY);
                if (theFlag < 0)
                    this->mKind = kAITileShallowsShore;
                else if (theFlag > 0)
                    this->mKind = kAITileShallowsNearShore;
                else
                    this->mKind = 7;
            } else
                this->mKind = 1;
            if (this->mView.GetResourceKind(inX, inY)) {
                this->mWA0 = this->mWA2;
                this->mKind = kAITileForest;
            } else {
                EEAIAStarBlocker theBlocker = this->mView.GetFirstBlocker(inX, inY);
                if (theBlocker.mFound) {
                    this->mUnitID = theBlocker.mUnitID;
                    this->mKind = kAITileUnitObstacle;
                    this->mPlayerID = theBlocker.mPlayerID;
                    if (theMachine != NULL)
                        theMachine->mCheckCorners = false;
                    if (theBlocker.mIsGate)
                        this->mKind = kAITileGate;
                    else if (theBlocker.mIsWall)
                        this->mKind = kAITileWall;
                }
            }
        } else if (this->mCategory == kContinentCategoryWater) {
            long theFlag = this->mView.GetTileFlag(inX, inY);
            if (theFlag < 0)
                this->mKind = kAITileShore;
            else if (theFlag > 0)
                this->mKind = kAITileNearShore;
            else
                this->mKind = 2;
        } else if (this->mCategory == kContinentCategorySpace) {
            long theFlag = this->mView.GetTileFlag(inX, inY);
            if (theFlag < 0)
                this->mKind = kAITileShore;
            else if (theFlag > 0)
                this->mKind = kAITileNearShore;
            else
                this->mKind = 15;
        } else if (this->mCategory == kContinentCategoryMapEdge) {
            this->mKind = kAITileMapEdge;
            if (theMachine != NULL)
                theMachine->mCheckCorners = false;
        } else if (this->mCategory == kContinentCategoryCliff) {
            this->mKind = kAITileCliff;
            if (theMachine != NULL)
                theMachine->mCheckCorners = false;
        }
    }

    if (this->mView.IsEdgeTile(inX, inY) != (this->mCategory == kContinentCategoryMapEdge))
        return false;
    theResult = true;
    EEAIAStarPathAreaInfo theArea = this->mView.GetPathAreaAt(inX, inY);
    if (!theArea.mFound || theArea.mKind != this->mKind) {
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
