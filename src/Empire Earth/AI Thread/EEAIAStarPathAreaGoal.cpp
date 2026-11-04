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
#include "ee/EEAIAStarPathAreaGoal.h"
#include "ee/EEAIAStarMachine.h"

// EEAIContinentManager.cpp's inline (out of line here: 0x29ced)
long EEAIHasResourceUnit(EETile* inTile);

// the manager's per-tile value (its grid, or GetTileFlag while the grid is not built) (provisional name)
inline long EEAIGetTileValue(long inX, long inY)
{
    if (gContinentManager.mTileValues)
        return gContinentManager.mTileValues[gContinentManager.mWidth * inX + inY];
    return gContinentManager.GetTileFlag(inX, inY);
}

// EEAIContinent's inline kind-id adders (COMDATs 0x1887f..0x189a2 after AssignKindIDs)
inline void EEAIContinent::AddIDBC(unsigned short inID)
{
    if (std::find(this->mIDsBC.begin(), this->mIDsBC.end(), inID) == this->mIDsBC.end())
        this->mIDsBC.push_back(inID);
}

inline void EEAIContinent::AddIDCC(unsigned short inID)
{
    if (std::find(this->mIDsCC.begin(), this->mIDsCC.end(), inID) == this->mIDsCC.end())
        this->mIDsCC.push_back(inID);
}

inline void EEAIContinent::AddIDDC(unsigned short inID)
{
    if (std::find(this->mIDsDC.begin(), this->mIDsDC.end(), inID) == this->mIDsDC.end())
        this->mIDsDC.push_back(inID);
}

inline void EEAIContinent::AddIDEC(unsigned short inID)
{
    if (std::find(this->mIDsEC.begin(), this->mIDsEC.end(), inID) == this->mIDsEC.end())
        this->mIDsEC.push_back(inID);
}

// FUNCTION: EXE 0x4173cc
// Creates the path-area flood-fill goal with no area grid, no owner, no tiles and the area/forest ids starting at 1
EEAIAStarPathAreaGoal::EEAIAStarPathAreaGoal()
    : mAreas(NULL), mOwnerID(-1), mPlayerID(0), mTileCount(0), mResources1(0), mResources2(0), mResources3(0),
      mContinentID(0), mContinentKind(kContinentCategoryNone), mForestID(0), mNextForestID(1), mNextPathAreaID(1), mKind(kAITileNone), mMode(kAIPathNone),
      mNextKindID(0)
{
}

// FUNCTION: EXE 0x41747c
// Stamps every closed and open node's tile with the next path area id and collects those tiles into the area's list
void EEAIAStarPathAreaGoal::CollectTiles()
{
    EEAIAStarNode* theNode;
    unsigned short theID = this->mNextPathAreaID;
    this->mTiles.reserve(this->mTileCount);
    for (theNode = this->mStorage->mFastClosedList; theNode; theNode = theNode->mNext) {
        this->mStorage->mPathAreaIDs[this->mStorage->mHeight * theNode->mX + theNode->mY] = theID;
        this->mTiles.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    }
    for (theNode = this->mStorage->mFastOpenList; theNode; theNode = theNode->mNext) {
        this->mStorage->mPathAreaIDs[this->mStorage->mHeight * theNode->mX + theNode->mY] = theID;
        this->mTiles.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    }
}

// FUNCTION: EXE 0x417510
// Turns the filled tiles into a new path area (plus a forest with its resource counts for forest tiles) or, in a
// kind pass, assigns the next kind id; then marks the bounds dirty and resets the fill state for the next area
void EEAIAStarPathAreaGoal::CreateAreas()
{
    if (this->mMode == kAIPathNone) {
        EEAIPathArea* theArea = new EEAIPathArea;
        theArea->mID = this->mNextPathAreaID;
        theArea->mContinentID = this->mContinentID;
        theArea->mAreaType = kAIAreaPathArea;
        theArea->mKind = this->mKind;
        theArea->Create(this->mTiles);
        theArea->UpdateBounds(gWorld.mInfluenceTable);
        this->mPathAreas.insert(std::map<unsigned short, EEAIPathArea*>::value_type(this->mNextPathAreaID, theArea));
        this->mNextPathAreaID++;
        if (this->mKind == kAITileForest) {
            EEAIForest* theForest = new EEAIForest;
            theForest->mID = this->mNextForestID;
            theForest->mContinentID = this->mContinentID;
            theForest->mAreaType = kAIAreaForest;
            theForest->Create(this->mTiles);
            theForest->UpdateBounds(gWorld.mInfluenceTable);
            theForest->mNumResources1 = this->mResources1;
            theForest->mNumResources3 = this->mResources3;
            theForest->mNumResources2 = this->mResources2;
            theForest->UpdateResourceKind();
            this->mForests.insert(std::map<unsigned short, EEAIForest*>::value_type(this->mNextForestID, theForest));
            this->mNextForestID++;
        } else if (this->mKind == kAITileUnitObstacle)
            theArea->mUnitID = this->mOwnerID;
    } else {
        this->mNextKindID++;
        this->AssignKindIDs();
    }
    EEAIAStarBounds& theBounds = this->mStorage->mDirtyBounds;
    theBounds = this->mTileBounds;
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

// NEAR: the original keeps the result in bl; ours spills it to the frame.
// FUNCTION: EXE 0x4176c7
// Tests whether a tile belongs to the current kind (land, water, cliff, space, map edge); kind 0 picks the kind
// from the first tile. Water and space tiles under units pass only for working docks/navy yards or space docks
bool EEAIAStarPathAreaGoal::ClassifyTile(EETile* inTile, EEMap* inMap, long inX, long inY, unsigned char* ioKind,
                                         bool inShallowsAreWater)
{
    bool theResult = false;
    if (inTile) {
        switch (*ioKind) {
        case 0:
            switch (inTile->mTerrainType) {
            case kTerrainDeepWater:
                *ioKind = this->mMode != kAIPathNonSpace ? 2 : 1;
                break;
            case kTerrainCliff:
                *ioKind = 3;
                break;
            case kTerrainShallowWater:
                if (this->mMode == kAIPathNonSpace)
                    *ioKind = 1;
                else
                    *ioKind = inShallowsAreWater ? 2 : 1;
                break;
            case kTerrainSpace:
                *ioKind = 4;
                break;
            default:
                *ioKind = 1;
                break;
            }
            if (inMap->IsEdgeTile(inX, inY))
                *ioKind = 5;
            theResult = true;
            break;
        case 1:
            if (this->mMode == kAIPathNonSpace) {
                if (inTile->mTerrainType != kTerrainSpace && !inMap->IsEdgeTile(inX, inY))
                    theResult = true;
                else
                    theResult = false;
            } else if (inTile->mTerrainType != kTerrainDeepWater && inTile->mTerrainType != kTerrainCliff && inTile->mTerrainType != kTerrainSpace &&
                       !inMap->IsEdgeTile(inX, inY))
                theResult = !EEAIHasResourceUnit(inTile);
            else
                theResult = false;
            break;
        case 2:
            if ((inTile->mTerrainType == kTerrainDeepWater || inTile->mTerrainType == kTerrainShallowWater) && !inMap->IsEdgeTile(inX, inY)) {
                theResult = true;
                if (inTile->mTerrainType == kTerrainShallowWater && inTile->mUnits) {
                    EETileLocation theTile(inX, inY);
                    EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(theTile);
                    if (theArea && theArea->mKind != kAITileWater) {
                        if (theArea->mKind <= kAITileMapEdge)
                            theResult = false;
                        else if (theArea->mKind > kAITileNearShore) {
                            if (theArea->mKind == kAITileUnitObstacle) {
                                theResult = false;
                                for (RTSTileUnitNode* theNode = inTile->mUnits; theNode && !theResult;
                                     theNode = theNode->mNext) {
                                    EEUnit* theUnit = theNode->mUnit;
                                    theResult = (theUnit->mUnitClass->mUnitType == kUnitTypeDock ||
                                                 theUnit->mUnitClass->mUnitType == kUnitTypeNavyYard) &&
                                                theUnit->mHitPoints > 0 && !theUnit->IsFoundation();
                                }
                            } else
                                theResult = false;
                        }
                    }
                }
            } else
                theResult = false;
            break;
        case 3:
            theResult = inTile->mTerrainType == kTerrainCliff && !inMap->IsEdgeTile(inX, inY);
            break;
        case 4:
            if (inTile->mTerrainType == kTerrainSpace && !inMap->IsEdgeTile(inX, inY)) {
                theResult = true;
                if (inTile->mUnits) {
                    EETileLocation theTile(inX, inY);
                    EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(theTile);
                    if (theArea) {
                        if (theArea->mKind != kAITileUnitObstacle) {
                            if (theArea->mKind != kAITileSpace)
                                theResult = false;
                        } else {
                            theResult = false;
                            for (RTSTileUnitNode* theNode = inTile->mUnits; theNode && !theResult;
                                 theNode = theNode->mNext) {
                                EEUnit* theUnit = theNode->mUnit;
                                unsigned long theType = theUnit->mUnitClass->mUnitType;
                                theResult = (theType == kUnitTypeSpaceDock || theType == kUnitTypeSpaceTurret || theType == kUnitTypeOrbitalSpaceStation) &&
                                            theUnit->mHitPoints > 0 && !theUnit->IsFoundation();
                            }
                        }
                    }
                }
            } else
                theResult = false;
            break;
        case 5:
            if (inMap->IsEdgeTile(inX, inY))
                theResult = true;
            else
                theResult = false;
            break;
        }
    }
    return theResult;
}

// The set's insert result is unused; the continents get the id through their kind vectors.
// FUNCTION: EXE 0x417a4b
// Gives every path area under the filled tiles the next water-body, space-region, land-mass or surface-region id
// (by mode) and adds that id to each touched continent's matching id list
void EEAIAStarPathAreaGoal::AssignKindIDs()
{
    std::set<unsigned long> theAreas;
    for (long i = this->mTiles.size() - 1; i >= 0; i--) {
        EEAIPathArea* theArea = this->GetArea((*(this->mTiles.begin() + i)).mX, (*(this->mTiles.begin() + i)).mY);
        theAreas.insert(reinterpret_cast<unsigned long>(theArea));
        if (this->mMode == kAIPathWaterBody)
            theArea->mWaterBodyID = this->mNextKindID;
        else if (this->mMode == kAIPathSpaceRegion)
            theArea->mSpaceRegionID = this->mNextKindID;
        else if (this->mMode == kAIPathLandMass)
            theArea->mLandMassID = this->mNextKindID;
        else
            theArea->mSurfaceRegionID = this->mNextKindID;
    }
    for (std::set<unsigned long>::iterator theIt = theAreas.begin(); theIt != theAreas.end(); theIt++) {
        EEAIContinent* theContinent = gContinentManager.mContinents[((EEAIPathArea*)*theIt)->mContinentID];
        if (!theContinent)
            continue;
        if (this->mMode == kAIPathWaterBody)
            theContinent->AddIDBC(this->mNextKindID);
        else if (this->mMode == kAIPathSpaceRegion)
            theContinent->AddIDDC(this->mNextKindID);
        else if (this->mMode == kAIPathLandMass)
            theContinent->AddIDCC(this->mNextKindID);
        else
            theContinent->AddIDEC(this->mNextKindID);
    }
}

// GetCategory() accessors put RTSMap::GetTile out of line
// as in the original; the 4-argument ClassifyTile wrapper (0x184d1) is an out-of-line member defined below.
// The unit loops are written out, the resource count is a switch, the tile value tests the grid first.
// FUNCTION: EXE 0x417bcb
// Decides whether the flood fill takes a tile into the current area and, if so, counts it and grows the bounds
// In a path area pass (mMode 0) the first tile fixes the area's continent and kind; later tiles must match them (by
// kind: terrain, the manager's tile value, resources, the blocking unit's owner), and the area's tiles are all on
// or all off the map edge as its continent is a kind 5 one. In a kind pass ClassifyTile decides.
bool EEAIAStarPathAreaGoal::AcceptTile(long inX, long inY, bool inStart)
{
    EETile* theTile = this->mMap->GetTile(inX, inY);
    long theOwner = -1;
    bool theResult = false;
    if (theTile) {
        if (this->mMode == kAIPathNone) {
            EEAIContinent* theContinent = gContinentManager.GetContinentAt(inX, inY);
            if (this->mContinentID) {
                if (this->mContinentKind == theContinent->mCategory && this->mContinentID == theContinent->mID) {
                    EEUnit* theUnit;
                    RTSTileUnitNode* theNode;
                    switch (this->mKind) {
                    case kAITileLand:
                        if (theTile->mTerrainType != kTerrainShallowWater && theTile->mTerrainType != kTerrainSpace) {
                            theResult = !EEAIHasResourceUnit(theTile);
                            if (theResult) {
                                for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                                    theUnit = theNode->mUnit;
                                    if ((theUnit->GetUnitClass()->mBlocksTile || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate) &&
                                        !theUnit->IsFoundation() &&
                                        (theUnit->GetHitPoints() > 0 || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource)) {
                                        theResult = false;
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    case kAITileWater:
                        theResult = theTile->mTerrainType == kTerrainDeepWater && EEAIGetTileValue(inX, inY) == 0;
                        break;
                    case kAITileSpace:
                        theResult = theTile->mTerrainType == kTerrainSpace && EEAIGetTileValue(inX, inY) == 0;
                        break;
                    case kAITileCliff:
                        if (theTile->mTerrainType == kTerrainCliff)
                            theResult = true;
                        else
                            theResult = false;
                        break;
                    case kAITileForest: {
                        long theResource = EEAIHasResourceUnit(theTile);
                        theResult = theResource != 0;
                        if (theResult) {
                            switch (theResource) {
                            case 1:
                                this->mResources1++;
                                break;
                            case 2:
                                this->mResources2++;
                                break;
                            case 3:
                                this->mResources3++;
                                break;
                            }
                        }
                        break;
                    }
                    case kAITileGaiaObstacle:
                        if (!EEAIHasResourceUnit(theTile)) {
                            for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                                theUnit = theNode->mUnit;
                                if ((theUnit->GetUnitClass()->mBlocksTile || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate) &&
                                    !theUnit->IsFoundation() &&
                                    (theUnit->GetHitPoints() > 0 || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource)) {
                                    theOwner = theUnit->GetUnitID();
                                    break;
                                }
                            }
                            theResult = theOwner == this->mOwnerID;
                        }
                        break;
                    case kAITileMapEdge:
                        theResult = true;
                        break;
                    case kAITileShallows:
                        if (theTile->mTerrainType == kTerrainShallowWater && EEAIGetTileValue(inX, inY) == 0) {
                            theResult = !EEAIHasResourceUnit(theTile);
                            if (theResult) {
                                for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                                    theUnit = theNode->mUnit;
                                    if ((theUnit->GetUnitClass()->mBlocksTile || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate) &&
                                        !theUnit->IsFoundation() &&
                                        (theUnit->GetHitPoints() > 0 || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource)) {
                                        theResult = false;
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    case kAITileShallowsShore:
                        if (theTile->mTerrainType == kTerrainShallowWater && EEAIGetTileValue(inX, inY) < 0) {
                            theResult = !EEAIHasResourceUnit(theTile);
                            if (theResult) {
                                for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                                    theUnit = theNode->mUnit;
                                    if ((theUnit->GetUnitClass()->mBlocksTile || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate) &&
                                        !theUnit->IsFoundation() &&
                                        (theUnit->GetHitPoints() > 0 || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource)) {
                                        theResult = false;
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    case kAITileShallowsNearShore:
                        if (theTile->mTerrainType == kTerrainShallowWater && EEAIGetTileValue(inX, inY) > 0) {
                            theResult = !EEAIHasResourceUnit(theTile);
                            if (theResult) {
                                for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                                    theUnit = theNode->mUnit;
                                    if ((theUnit->GetUnitClass()->mBlocksTile || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate) &&
                                        !theUnit->IsFoundation() &&
                                        (theUnit->GetHitPoints() > 0 || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource)) {
                                        theResult = false;
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    case kAITileShore: {
                        long theType = theTile->mTerrainType;
                        theResult = (theType == kTerrainDeepWater || theType == kTerrainSpace) && EEAIGetTileValue(inX, inY) < 0;
                        break;
                    }
                    case kAITileNearShore: {
                        long theType = theTile->mTerrainType;
                        theResult = (theType == kTerrainDeepWater || theType == kTerrainSpace) && EEAIGetTileValue(inX, inY) > 0;
                        break;
                    }
                    case kAITileUnitObstacle:
                        if (!EEAIHasResourceUnit(theTile)) {
                            for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                                theUnit = theNode->mUnit;
                                if ((theUnit->GetHitPoints() > 0 || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource) &&
                                    ((theUnit->GetUnitClass()->mBlocksTile && !theUnit->IsFoundation()) ||
                                     theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate)) {
                                    theOwner = theUnit->GetUnitID();
                                    break;
                                }
                            }
                            theResult = theOwner == this->mOwnerID;
                        }
                        break;
                    case kAITileWall:
                        if (!EEAIHasResourceUnit(theTile)) {
                            for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                                theUnit = theNode->mUnit;
                                if (theUnit->GetUnitClass()->mIsWall && theUnit->GetHitPoints() > 0 &&
                                    theUnit->GetUnitClass()->GetCategory() != kUnitCategoryGate &&
                                    theUnit->GetPlayer()->GetPlayerID() == this->mPlayerID && !theUnit->IsFoundation()) {
                                    theResult = true;
                                    break;
                                }
                                if ((theUnit->GetUnitClass()->mBlocksTile || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate) &&
                                    !theUnit->IsFoundation() &&
                                    (theUnit->GetHitPoints() > 0 || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource))
                                    break;
                            }
                        }
                        break;
                    case kAITileGate:
                        if (!EEAIHasResourceUnit(theTile)) {
                            for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                                theUnit = theNode->mUnit;
                                if (!(theUnit->GetHitPoints() > 0) && theUnit->GetUnitClass()->GetCategory() != kUnitCategoryResource)
                                    continue;
                                if (theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate &&
                                    theUnit->GetPlayer()->GetPlayerID() == this->mPlayerID) {
                                    theOwner = theUnit->GetUnitID();
                                    break;
                                }
                                if ((theUnit->GetUnitClass()->mBlocksTile || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate) &&
                                    !theUnit->IsFoundation()) {
                                    theOwner = theUnit->GetUnitID();
                                    break;
                                }
                            }
                            theResult = theOwner == this->mOwnerID;
                        }
                        break;
                    }
                }
            } else {
                EEAIAStarMachineBase* theMachine = dynamic_cast<EEAIAStarMachineBase*>(this->mStorage);
                if (theMachine)
                    theMachine->mCheckCorners = true;
                this->mTileBounds.Set(EETileCoordinate(inX, inY));
                this->mContinentID = theContinent->mID;
                this->mContinentKind = theContinent->mCategory;
                if (this->mContinentKind == kContinentCategoryLand) {
                    if (theTile->mTerrainType == kTerrainShallowWater) {
                        long theValue = EEAIGetTileValue(inX, inY);
                        if (theValue < 0)
                            this->mKind = kAITileShallowsShore;
                        else if (theValue > 0)
                            this->mKind = kAITileShallowsNearShore;
                        else
                            this->mKind = 7;
                    } else
                        this->mKind = 1;
                    long theResource = EEAIHasResourceUnit(theTile);
                    if (theResource) {
                        this->mForestID = this->mNextForestID;
                        this->mKind = kAITileForest;
                        if (theMachine)
                            theMachine->mCheckCorners = false;
                        switch (theResource) {
                        case 1:
                            this->mResources1++;
                            break;
                        case 2:
                            this->mResources2++;
                            break;
                        case 3:
                            this->mResources3++;
                            break;
                        }
                    } else {
                        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                            EEUnit* theUnit = theNode->mUnit;
                            if ((!theUnit->GetUnitClass()->mBlocksTile && theUnit->GetUnitClass()->GetCategory() != kUnitCategoryGate) ||
                                theUnit->IsFoundation() ||
                                (!(theUnit->GetHitPoints() > 0) && theUnit->GetUnitClass()->GetCategory() != kUnitCategoryResource))
                                continue;
                            this->mOwnerID = theUnit->GetUnitID();
                            this->mKind = kAITileUnitObstacle;
                            this->mPlayerID = theUnit->GetPlayer()->GetPlayerID();
                            if (theMachine)
                                theMachine->mCheckCorners = false;
                            if (theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate)
                                this->mKind = kAITileGate;
                            else if (theUnit->GetUnitClass()->mIsWall)
                                this->mKind = kAITileWall;
                            break;
                        }
                    }
                } else if (this->mContinentKind == kContinentCategoryWater) {
                    long theValue = EEAIGetTileValue(inX, inY);
                    if (theValue < 0)
                        this->mKind = kAITileShore;
                    else if (theValue > 0)
                        this->mKind = kAITileNearShore;
                    else
                        this->mKind = 2;
                } else if (this->mContinentKind == kContinentCategorySpace) {
                    long theValue = EEAIGetTileValue(inX, inY);
                    if (theValue < 0)
                        this->mKind = kAITileShore;
                    else if (theValue > 0)
                        this->mKind = kAITileNearShore;
                    else
                        this->mKind = 0xf;
                } else if (this->mContinentKind == kContinentCategoryMapEdge) {
                    this->mKind = kAITileMapEdge;
                    if (theMachine)
                        theMachine->mCheckCorners = false;
                } else if (this->mContinentKind == kContinentCategoryCliff) {
                    this->mKind = kAITileCliff;
                    if (theMachine)
                        theMachine->mCheckCorners = false;
                }
                theResult = true;
            }
            if (theResult) {
                if (this->mMap->IsEdgeTile(inX, inY) == (this->mContinentKind == kContinentCategoryMapEdge))
                    theResult = true;
                else
                    theResult = false;
            } else
                theResult = false;
        } else {
            if (this->mTileCount == 0) {
                this->mTileBounds.Set(EETileCoordinate(inX, inY));
            }
            theResult = this->ClassifyTile(theTile, inX, inY, 0);
        }
        if (theResult) {
            this->mTileCount++;
            EETileCoordinate theCoordinate;
            theCoordinate.mX = inX;
            theCoordinate.mY = inY;
            this->mTileBounds.Extend(theCoordinate);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4184d1
// Classifies a tile on the goal's own map against the goal's continent kind, shallows counting as water
// Not inline: the original calls it from AcceptTile.
bool EEAIAStarPathAreaGoal::ClassifyTile(EETile* inTile, long inX, long inY, long inArg)
{
    return this->ClassifyTile(inTile, this->mMap, inX, inY, &this->mContinentKind, true);
}

// FUNCTION: EXE 0x418a84
// Returns the path area at tile (inX, inY), or NULL off the grid
// Not inline: AssignKindIDs calls it.
EEAIPathArea* EEAIAStarPathAreaGoal::GetArea(unsigned long inX, unsigned long inY)
{
    return inX < this->mAreasSize && inY < this->mAreasSize ? this->mAreas[this->mAreasSize * inX + inY] : 0;
}

// FUNCTION: EXE 0x418ac8
// Destroys the goal and its tile list and area/forest maps (members only)
EEAIAStarPathAreaGoal::~EEAIAStarPathAreaGoal()
{
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x4184f1
// TEMPLATE: EXE 0x41852b
// TEMPLATE: EXE 0x4185e5
// TEMPLATE: EXE 0x418610
// TEMPLATE: EXE 0x418681
// TEMPLATE: EXE 0x418815
// TEMPLATE: EXE 0x41884d
// TEMPLATE: EXE 0x41887f
// TEMPLATE: EXE 0x4188e0
// TEMPLATE: EXE 0x418941
// TEMPLATE: EXE 0x4189a2
// TEMPLATE: EXE 0x418a03
// TEMPLATE: EXE 0x418a58
// TEMPLATE: EXE 0x418aa9
// TEMPLATE: EXE 0x418aac
// TEMPLATE: EXE 0x418af1
// TEMPLATE: EXE 0x418b1d
// TEMPLATE: EXE 0x418b48
// TEMPLATE: EXE 0x418b73
// TEMPLATE: EXE 0x418b90
// TEMPLATE: EXE 0x418bdf
// TEMPLATE: EXE 0x418c16
// TEMPLATE: EXE 0x418c87
// TEMPLATE: EXE 0x418cf8
// TEMPLATE: EXE 0x418f86
// TEMPLATE: EXE 0x418fb8
// TEMPLATE: EXE 0x418fea
// TEMPLATE: EXE 0x418ffd
// TEMPLATE: EXE 0x419021
// TEMPLATE: EXE 0x41903a
// TEMPLATE: EXE 0x41905f
