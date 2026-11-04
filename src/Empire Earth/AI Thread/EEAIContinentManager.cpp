// C:\EEX\Empire Earth\AI Thread\EEAIContinentManager.cpp (file name inferred: no __FILE__; RTTI class
// EEAIContinentManager, vtable 0x43a210). The AI's continent map: 0x1ed2c..0x29fcc (EEAIContinent.cpp's COMDATs end at
// 0x1ed2c; the EEAIFO* function objects start at 0x29fcc).
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
#include "ee/EEAIFOUnitsToKillScoringBase.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPUnitSelector.h"
#include <EEAIFunctionObjects.h>

#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEAIRectangleArea.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIHill.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EEDeleteVectorContents.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEAIAStarContinentStorage.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarContinentGoal.h"
#include "ee/EEAIAStarHillStorage.h"
#include "ee/EEAIAStarHillGoal.h"
#include "ee/EEAIAStarPathAreaStorage.h"
#include "ee/EEAIAStarPathAreaGoal.h"
#include "ee/EEAIAStarPathSectorStorage.h"
#include "ee/EEAIAStarPathSectorGoal.h"
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEAIAStarGraphTerrainStorage.h"
#include "ee/EEAIAStarGraphGoalBase.h"
#include "ee/EEAIAStarGraphContinentalChainGoal.h"
#include "ee/EEPathManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMArea.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEAIAStarMachine.h"
#include "ee/EEAIAStarMachineSteps.h"
#include "ee/EEDebugTrace.h"
#include "LostDebug.h"   // NOT original source: EE_LOST_* stand-ins (docs/LOST_DEBUG.md)

// Unit-local inline helpers (out of line in the original: COMDATs 0x429ced/0x429d1b close this unit) (provisional names)
// 0x29d1b: the resource kind of a unit class by its type (+0x1bc): 1, 3 or 2
inline long EEAIGetResourceKind(EEUnitClass* inClass)
{
    switch (inClass->mUnitType) {
    case kUnitTypeTree01Maple:
    case kUnitTypeTree02Maple:
    case kUnitTypeTree12Spruce:
    case kUnitTypeTree03Spruce:
    case kUnitTypeTree07Spruce:
    case kUnitTypeTree09Spruce:
    case kUnitTypeTree10Spruce:
    case kUnitTypeTree11Spruce:
    case kUnitTypeTree15Cactus:
    case kUnitTypeTree16Cactus:
        return 1;
    case kUnitTypeTree19:
    case kUnitTypeTree20:
    case kUnitTypeTree18:
    case kUnitTypeTree24:
    case kUnitTypeTree25:
    case kUnitTypeTree21Palm:
        return 3;
    default:
        return 2;
    }
}

// 0x29ced: the resource kind of the first unit of class kind 3 on inTile (not on terrain 1), 0 if none
inline long EEAIHasResourceUnit(EETile* inTile)
{
    long theResult = 0;
    if (inTile && inTile->mTerrainType != kTerrainDeepWater) {
        for (RTSTileUnitNode* theNode = inTile->mUnits; theNode; theNode = theNode->mNext) {
            EEUnitClass* theClass = theNode->mUnit->mUnitClass;
            if (theClass->mResourceType == kResourceWood) {
                theResult = EEAIGetResourceKind(theClass);
                break;
            }
        }
    }
    return theResult;
}

// The AI's continent flood fill (the EEAIAStarMachine instantiations whose COMDATs close this unit) (provisional names)
typedef EEAIAStarMachine<EEAIAStarContinentStorage, EEAIAStarContinentGoal, EEMap> EEAIContinentMachine;
typedef EEAIAStarMachine<EEAIAStarHillStorage, EEAIAStarHillGoal, EEMap> EEAIHillMachine;
typedef EEAIAStarMachine<EEAIAStarPathAreaStorage, EEAIAStarPathAreaGoal, EEMap> EEAIPathAreaMachine;
typedef EEAIAStarMachine<EEAIAStarPathSectorStorage, EEAIAStarPathSectorGoal, EEMap> EEAIPathSectorMachine;
typedef EEAIAStarGraphMachine<EEAIAStarGraphTerrainStorage, EEAIAStarGraphContinentalChainGoal, EEAIContinent>
    EEAIContinentChainMachine;

// 0x263cb (COMDAT; declared in ee/EEAIPathSectorRegionInformation.h)
inline U2DPoint<long> EEAIPathSectorRegionInformation::GetSectorEnd(const U2DPoint<long>& inSector) const
{
    return U2DPoint<long>((inSector.mX + 1) * this->mSectorSize - 1, (inSector.mY + 1) * this->mSectorSize - 1);
}

// 0x27604 (COMDAT; declared in ee/EEAIContinentManager.h)
inline void EEAIContinentManager::SetPathSector(EEAIPathSector* inSector)
{
    this->mPathSectors[inSector->mID] = inSector;
}

// 0x26fed (COMDAT; declared in ee/EEAIContinentManager.h)
inline bool EEAIContinentManager::IsShoreTile(long inX, long inY) const
{
    return std::binary_search(this->mShoreTiles.begin(), this->mShoreTiles.end(), U2DPoint<long>(inX, inY));
}

// 0x273b9 (COMDAT; declared in ee/EEAIContinentManager.h)
inline EEAIPathSector* EEAIContinentManager::GetPathSectorByID(unsigned short inID) const
{
    return this->mPathSectors[inID];
}

// 0x26d84/0x26dca/0x26e13/0x26ea4 (COMDATs; declared in ee/EEAIContinent.h, out of line in this unit's range: inline in
// the real header)
inline void EEAIContinent::AddChokePoint(long inIndex)
{
    this->mChokePoints.push_back(inIndex);
    std::sort(this->mChokePoints.begin(), this->mChokePoints.end());
}

inline void EEAIContinent::AddForestID(unsigned short inID)
{
    this->mForestIDs.push_back(inID);
    std::sort(this->mForestIDs.begin(), this->mForestIDs.end());
}

inline void EEAIContinent::AddHillID(unsigned short inID)
{
    this->mHillIDs.push_back(inID);
    std::sort(this->mHillIDs.begin(), this->mHillIDs.end());
}

inline void EEAIContinent::RemoveForestID(unsigned short inID)
{
    std::vector<unsigned short>::iterator theID = std::lower_bound(this->mForestIDs.begin(), this->mForestIDs.end(), inID);
    if (theID != this->mForestIDs.end() && *theID == inID)
        this->mForestIDs.erase(theID);
}

// 0x273c9/0x273f1 (COMDATs; declared in ee/EEAIContinentManager.h)
inline EEAIPathSubSector* EEAIContinentManager::GetPathSubSectorAt(long inX, long inY)
{
    const std::vector<EEAIPathSubSector*>& theSubSectors = this->mPathSubSectors;
    unsigned short theID = this->GetPathSubSectorID(inX, inY);
    return theID ? theSubSectors[theID] : NULL;
}

inline unsigned short EEAIContinentManager::GetPathSubSectorID(long inX, long inY)
{
    if (!((unsigned long)inX < this->mWidth) || !((unsigned long)inY < this->mHeight))
        return 0;
    if (this->mPathSubSectorIDs)
        return this->mPathSubSectorIDs[this->mWidth * inX + inY];
    return this->FindPathSubSectorID(inX, inY);
    return 0;
}

// FUNCTION: EXE 0x41ed2c
// Returns the AI tile kind of inTile: 0 off the map; by continent category and shore flag (land 1/7..9, water 2/10/11,
// space 15/10/11, map edge 6, cliff 3); on land 4 for live wood, 12/13/14 under a live blocking unit, wall or gate
long EEAIGetTileKind(const EETileLocation& inTile)
{
    EE_LOST_DEBUG(9);   // LOST-DEBUG: none: exact only (lostdebug_search smallest n; inline_sim: the orig calls RTSMap::GetTile, GetContinentAt and EEAIHasResourceUnit out of line)
    EETile* theTile = gWorld.mMap->GetTile(inTile.GetX(), inTile.GetY());
    long theResult = 0;
    if (theTile) {
        unsigned char theKind = gContinentManager.GetContinentAt(inTile.GetX(), inTile.GetY())->mCategory;
        if (theKind == kContinentCategoryLand) {
            if (theTile->mTerrainType == kTerrainShallowWater) {
                long theFlag = gContinentManager.GetTileFlag(inTile.GetX(), inTile.GetY());
                if (theFlag < 0)
                    theResult = 8;
                else
                    theResult = theFlag > 0 ? 9 : 7;
            } else {
                theResult = 1;
            }
            if (EEAIHasResourceUnit(theTile)) {
                for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                    EEUnit* theUnit = theNode->mUnit;
                    if (theUnit->GetUnitClass()->mResourceType == kResourceWood && theUnit->mResourceAmount > 0) {
                        theResult = 4;
                        break;
                    }
                }
            } else {
                for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                    EEUnit* theUnit = theNode->mUnit;
                    if ((!theUnit->GetUnitClass()->mBlocksTile && theUnit->GetUnitClass()->mCategory != kUnitCategoryGate) || theUnit->IsFoundation() ||
                        !(theUnit->mHitPoints > 0) || (theUnit->GetUnitClass()->mResourceType == kResourceWood && !(theUnit->mResourceAmount > 0)))
                        continue;
                    theResult = 12;
                    if (theUnit->GetUnitClass()->mCategory == kUnitCategoryGate)
                        theResult = 14;
                    else if (theUnit->GetUnitClass()->mIsWall)
                        theResult = 13;
                    break;
                }
            }
        } else if (theKind == kContinentCategoryWater) {
            long theFlag = gContinentManager.GetTileFlag(inTile.GetX(), inTile.GetY());
            if (theFlag < 0)
                theResult = 10;
            else
                theResult = theFlag > 0 ? 11 : 2;
        } else if (theKind == kContinentCategorySpace) {
            long theFlag = gContinentManager.GetTileFlag(inTile.GetX(), inTile.GetY());
            if (theFlag < 0)
                theResult = 10;
            else
                theResult = theFlag > 0 ? 11 : 15;
        } else if (theKind == kContinentCategoryMapEdge) {
            theResult = 6;
        } else if (theKind == kContinentCategoryCliff) {
            theResult = 3;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x41eea9
// Returns the id of the first live, finished blocking unit or gate on inTile (wood only if not used up), -1 if none
long EEAIGetTileWallID(const EETileLocation& inTile)
{
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inTile);
    long theResult = -1;
    if (theTile) {
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
            EEUnit* theUnit = theNode->mUnit;
            if ((theUnit->GetUnitClass()->mBlocksTile || theUnit->GetUnitClass()->mCategory == kUnitCategoryGate) && !theUnit->IsFoundation() &&
                theUnit->mHitPoints > 0 && (theUnit->GetUnitClass()->mResourceType != kResourceWood || theUnit->mResourceAmount > 0)) {
                theResult = theUnit->mUnitID;
                break;
            }
        }
    }
    return theResult;
}

static void EEAIContinentsShutDown();

// FUNCTION: EXE 0x41ef12
// Resets the global continent manager, frees its tile grids (flags, continent ids, values, sub-sector ids) and the
// A* node pool
void EEAIContinentManager::Shutdown()
{
    gContinentManager.Reset();
    delete[] gContinentManager.mTileFlags;
    delete[] gContinentManager.mContinentIDs;
    delete[] gContinentManager.mTileValues;
    delete[] gContinentManager.mPathSubSectorIDs;
    gContinentManager.mTileFlags = NULL;
    gContinentManager.mContinentIDs = NULL;
    gContinentManager.mTileValues = NULL;
    gContinentManager.mPathSubSectorIDs = NULL;
    EEAIContinentsShutDown();
    EEAIAStarStorage::FreeNodePool();
}
// FUNCTION: EXE 0x41ef6b
// Creates an empty continent manager: no grids, every id and counter at 0
EEAIContinentManager::EEAIContinentManager()
    : mTileValues(NULL), mContinentIDs(NULL), mPathSubSectorIDs(NULL), mTileFlags(NULL), mState(0), mLastSerial(0), mL118(0),
      mNextPathAreaID(0), mLastSubSectorID(0), mChecksum(0), mNextID124(0), mNextID128(0), mNextID12C(0), mNextID130(0),
      mNumWaterTiles(0), mNumLandTiles(0), mNumSpaceTiles(0), mFlagsWidth(0), mWidth(0), mHeight(0), mClearing(false)
{
    for (long i = 0; i < 7; i++) {
        this->mNextIDs[i] = 0;
    }
}

// SYNTHETIC: EXE 0x41f103
// (EEAIContinentManager's scalar deleting destructor)

// FUNCTION: EXE 0x41f11f
// Destroys the continent manager by shutting down the global one (Shutdown)
EEAIContinentManager::~EEAIContinentManager()
{
    EEAIContinentManager::Shutdown();
}

// FUNCTION: EXE 0x41f1b8
// Registers a choke point under its index, adds it to its continent's choke point list and to the area map
void EEAIContinentManager::AddChokePoint(EEAIChokePoint* inChokePoint)
{
    this->mChokePoints[inChokePoint->mIndex] = inChokePoint;
    const std::vector<EEAIContinent*>& theContinents = this->mContinents;
    EEAIContinent* theContinent = theContinents[inChokePoint->mContinentID];
    if (theContinent)
        theContinent->AddChokePoint(inChokePoint->mIndex);
    this->AddArea(inChokePoint);
}

// FUNCTION: EXE 0x41f1f3
// Sorts and dedups ioTiles, then queues each tile under its path sector's id in the pending sector tiles for Rebuild
void EEAIContinentManager::AddSectorTiles(std::vector<U2DPoint<long> >& ioTiles)
{
    std::sort(ioTiles.begin(), ioTiles.end());
    std::vector<U2DPoint<long> >::iterator theEnd = std::unique(ioTiles.begin(), ioTiles.end());
    if (theEnd != ioTiles.end())
        ioTiles.erase(theEnd, ioTiles.end());
    for (long i = ioTiles.size() - 1; i >= 0; i--) {
        EEAIPathSector* theSector = this->GetPathSector(ioTiles[i]);
        if (!theSector)
            continue;
        std::map<unsigned short, std::vector<EETileCoordinate> >::iterator theTiles = this->mSectorTiles.find(theSector->mID);
        if (theTiles != this->mSectorTiles.end()) {
            theTiles->second.push_back(ioTiles[i]);
        } else {
            std::pair<std::map<unsigned short, std::vector<EETileCoordinate> >::iterator, bool> theResult =
                this->mSectorTiles.insert(std::make_pair(theSector->mID, std::vector<EETileCoordinate>()));
            if (theResult.second == true)
                theResult.first->second.push_back(ioTiles[i]);
        }
    }
}

// FUNCTION: EXE 0x41f3a0
// Registers a rectangle area under its id, updates its continents and adds it to the area map
void EEAIContinentManager::AddRectangleArea(EEAIRectangleArea* inArea)
{
    EEAIRectangleArea* theArea = dynamic_cast<EEAIRectangleArea*>(inArea);
    this->mRectangleAreas[theArea->mID] = theArea;
    theArea->UpdateContinents();
    this->AddArea(inArea);
}

// FUNCTION: EXE 0x41f3e0
// Shore step from water tile inFrom: returns true when inTile is off the map, not on a land continent or shallow
// water; otherwise collects inTile into outTiles and the continent's shore tiles (with inFrom) and returns false
bool EEAIContinentManager::AddShoreTile(const EETileLocation& inTile, EETileLocation inFrom, std::vector<U2DPoint<long> >& outTiles)
{
    bool theResult = false;
    long theX = inTile.mX;
    long theY = inTile.mY;
    if ((unsigned long)theX < gMapWidth && (unsigned long)theY < gMapHeight) {
        EEAIContinent* theContinent = gContinentManager.GetContinentAt(theX, theY);
        theResult = true;
        if (theContinent && theContinent->mCategory == theResult) {
            theResult = gWorld.mMap->GetTileAtLocation(inTile)->mTerrainType == kTerrainShallowWater;
            if (!theResult) {
                outTiles.push_back(inTile);
                theContinent->mShoreTiles.push_back(inTile);
                theContinent->mShoreTiles.push_back(inFrom);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x41f45a
// Fills ioAreas (indexed x * size + y) with the path area of every tile of inMap, NULL off the grid or the map
void EEAIContinentManager::GetPathAreas(EEMap* inMap, EEAIPathArea**& ioAreas)
{
    long theSize = inMap->mSize;
    for (long x = theSize - 1; x >= 0; x--) {
        for (long y = inMap->mSize - 1; y >= 0; y--) {
            if ((unsigned long)x < this->mWidth && (unsigned long)y < this->mHeight && inMap->GetTile(x, y)) {
                EETileLocation theTile(x, y);
                ioAreas[x * theSize + y] = this->GetPathAreaAt(theTile);
            } else {
                ioAreas[x * theSize + y] = NULL;
            }
        }
    }
}

// FUNCTION: EXE 0x41f503
// Deletes every continent, choke point, forest, hill, rectangle area, path area and sector, empties the area map,
// pending sector tiles, shore lists and free ids, and zeroes every counter
void EEAIContinentManager::Reset()
{
    this->mSectorTiles.clear();
    this->DeleteChokePoints();
    this->DeleteForests();
    this->DeleteHills();
    this->DeleteContinents();
    this->DeleteRectangleAreas();
    this->DeletePathAreas();
    this->DeletePathSectors();
    this->mPathSubSectors.clear();
    this->mAreas.clear();
    this->mLastSerial = 0;
    this->mL118 = 0;
    this->mNextPathAreaID = 0;
    this->mLastSubSectorID = 0;
    this->mChecksum = 0;
    this->mNextID124 = 0;
    this->mNextID128 = 0;
    this->mNextID12C = 0;
    this->mNextID130 = 0;
    this->mFlagsWidth = 0;
    this->mWidth = 0;
    this->mHeight = 0;
    this->mState = 0;
    for (long i = 0; i < 7; i++) {
        this->mNextIDs[i] = 0;
    }
    this->mShoreTiles.clear();
    this->mIceShoreTiles.clear();
    this->mFreePathAreaIDs.clear();
}

static void EEAIChokePointsDeleted();

// FUNCTION: EXE 0x41f5cb
// Deletes every choke point and empties the list
void EEAIContinentManager::DeleteChokePoints()
{
    DeleteVectorContents(this->mChokePoints);
    EEAIChokePointsDeleted();
}

// FUNCTION: EXE 0x41f5dd
// Deletes every continent and zeroes the land, water and space tile counts
void EEAIContinentManager::DeleteContinents()
{
    DeleteVectorContents(this->mContinents);
    this->mNumLandTiles = 0;
    this->mNumWaterTiles = 0;
    this->mNumSpaceTiles = 0;
}

// FUNCTION: EXE 0x41f600
// Deletes every forest
void EEAIContinentManager::DeleteForests()
{
    DeleteVectorContents(this->mForests);
}

// FUNCTION: EXE 0x41f60b
// Deletes every hill
void EEAIContinentManager::DeleteHills()
{
    DeleteVectorContents(this->mHills);
}

// FUNCTION: EXE 0x41f619
// Deletes every path area
void EEAIContinentManager::DeletePathAreas()
{
    DeleteVectorContents(this->mPathAreas);
}

// FUNCTION: EXE 0x41f624
// Deletes every path sector and resets the sub-sector ids (last id, free list) and the sector grid information
void EEAIContinentManager::DeletePathSectors()
{
    DeleteVectorContents(this->mPathSectors);
    this->mLastSubSectorID = 0;
    this->mFreeSubSectorIDs.clear();
    this->mRegionInformation.Reset();
}

// FUNCTION: EXE 0x41f651
// Deletes every rectangle area
void EEAIContinentManager::DeleteRectangleAreas()
{
    DeleteVectorContents(this->mRectangleAreas);
}

// FUNCTION: EXE 0x41f65f
// Returns 10 per wood resource unit on tile (inX, inY), 0 off the map
long EEAIContinentManager::GetResourceWeight(long inX, long inY)
{
    long theResult = 0;
    EETile* theTile = gWorld.mMap->GetTile(inX, inY);
    if (theTile) {
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
            if (theNode->mUnit->mUnitClass->mResourceType == kResourceWood)
                theResult += 10;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x41f6b1
// Makes inSubSector and the sub-sector *inID neighbours both ways; with inAdd also records the diagonal link pair
// inLink on each, without it drops their existing links
void EEAIContinentManager::LinkSubSectors(std::vector<EEAIPathSubSector*>** inSubSectors, EEAIPathSubSector* inSubSector,
    const unsigned short* inID, bool inAdd, const std::pair<EEAIPathSubSector*, EEAIPathSubSector*>& inLink)
{
    EEAIPathSubSector* theOther = (**inSubSectors)[*inID];
    if (theOther) {
        if (!inAdd) {
            if (inSubSector->HasLinks(*inID)) {
                inSubSector->RemoveLinks(*inID);
                theOther->RemoveLinks(inSubSector->mID);
            }
        } else {
            if (!inSubSector->IsNeighbor(theOther)) {
                unsigned short theID = *inID;
                inSubSector->mLinks.insert(std::make_pair(theID, inLink));
            }
            if (!theOther->IsNeighbor(inSubSector)) {
                unsigned short theID = inSubSector->mID;
                theOther->mLinks.insert(std::make_pair(theID, inLink));
            }
        }
        EEAIPathSubSector* theNeighbor;
        if (!inSubSector->IsNeighbor(theOther)) {
            theNeighbor = theOther;
            inSubSector->mNeighbors.push_back(theNeighbor);
        }
        if (!theOther->IsNeighbor(inSubSector)) {
            theNeighbor = inSubSector;
            theOther->mNeighbors.push_back(theNeighbor);
        }
    }
}

// FUNCTION: EXE 0x41f813
// Allocates a zeroed per-tile value grid (width x height), freeing the old one
void EEAIContinentManager::AllocateTileValues()
{
    delete[] this->mTileValues;
    this->mTileValues = new long[this->mHeight * this->mWidth];
    memset(this->mTileValues, 0, this->mHeight * this->mWidth * sizeof(long));
}

// FUNCTION: EXE 0x41f863
// Resets the region checksum to 0
void EEAIContinentManager::ClearChecksum()
{
    this->mChecksum = 0;
}

// FUNCTION: EXE 0x41f86b
// Builds the choke points of inMap with a search size by map size (4 on small maps up to 7 on huge ones), then
// drops those beside the map edge (PruneChokePoints)
void EEAIContinentManager::Build(EEMap* inMap)
{
    long theSize = inMap->mL195610;
    long theKind = 7;
    if (theSize <= 0x2a)
        theKind = 4;
    else if (theSize <= 0x3a)
        theKind = 5;
    else if (theSize <= 0x4b || theSize <= 0x69)
        theKind = 6;
    EEAIBuildChokePoints(inMap, theKind);
    this->PruneChokePoints();
}

// FUNCTION: EXE 0x41f8ae
// Floods inMap into continents (one fill per tile not yet in one), registers them by id with fresh serials,
// counts the land, water and space tiles, then builds the continent grid (CreateContinentGrid)
void EEAIContinentManager::CreateContinents(EEMap* inMap)
{
    EEAIContinentMachine theMachine(6, inMap);
    EEAIAStarStorage* theStorage = &theMachine;
    theStorage->Init(inMap->mSize, inMap->mSize);
    for (long x = inMap->mSize - 1; x >= 0; x--) {
        for (long y = inMap->mSize - 1; y >= 0; y--) {
            if (theMachine.mTiles60[theMachine.mWidth * x + y].mID != 0)
                continue;
            EETile* theTile = inMap->GetTile(x, y);
            if (!theTile)
                continue;
            theMachine.mCheckCorners = theTile->mTerrainType != kTerrainCliff && x + y != inMap->mL195608 &&
                y - x != inMap->mL195610 && x + y != inMap->mL19560C && x - y != inMap->mL195610;
            theMachine.mGoal.mStartTile.mX = x;
            theMachine.mGoal.mStartTile.mY = y;
            theMachine.mGoal.mGoalTile.mX = inMap->mSize;
            theMachine.mGoal.mGoalTile.mY = inMap->mSize;
            theMachine.Run();
            theMachine.mGoal.CollectTiles();
            theMachine.mGoal.CreateContinent();
        }
    }
    this->DeleteContinents();
    this->mContinents.resize(theMachine.mGoal.mNextID, NULL);
    for (std::map<unsigned short, EEAIContinent*>::const_iterator theIt = theMachine.mGoal.mContinents.begin();
         theIt != theMachine.mGoal.mContinents.end(); ++theIt) {
        EEAIContinent* theContinent = theIt->second;
        if (theContinent->mCategory == kContinentCategoryWater)
            this->mNumWaterTiles += theContinent->mTiles.Size();
        else if (theContinent->mCategory == kContinentCategorySpace)
            this->mNumSpaceTiles += theContinent->mTiles.Size();
        else
            this->mNumLandTiles += theContinent->mTiles.Size();
        theContinent->mSerial = ++this->mLastSerial;
        this->AddArea(theContinent);
        this->mContinents[theIt->first] = theContinent;
    }
    this->CreateContinentGrid(theMachine, inMap);
}

// FUNCTION: EXE 0x41fa5c
// Builds the tile-to-continent id grid, the continent adjacency matrix and neighbour lists from the fill, collects
// the shore tiles (also onto each continent), then sorts the shore lists and every continent's tiles
void EEAIContinentManager::CreateContinentGrid(EEAIContinentMachine& inMachine, EEMap* inMap)
{
    long x;
    long y;
    unsigned short theID;
    EEAIContinent* theContinent;
    unsigned short theOther;
    EEAIContinent* theNeighbor;
    long i;
    long j;
    U2DPoint<long> thePoint;
    EEMap* theMap = inMachine.mGoal.mMap;
    delete[] this->mTileFlags;
    delete[] this->mContinentIDs;
    this->mShoreTiles.clear();
    this->mWidth = inMap->mSize;
    this->mHeight = inMap->mSize;
    this->mContinentIDs = new unsigned short[this->mWidth * this->mHeight];
    this->mFlagsWidth = inMachine.mGoal.mNextID;
    this->mTileFlags = new bool[this->mFlagsWidth * this->mFlagsWidth];
    memset(this->mTileFlags, 0, this->mFlagsWidth * this->mFlagsWidth);
    long theWidth = inMachine.mWidth;
    long theHeight = inMachine.mHeight;
    for (x = 0; x < theWidth; x++) {
        for (y = 0; y < theHeight; y++) {
            theID = inMachine.mTiles60[inMachine.mWidth * x + y].mID;
            this->mContinentIDs[this->mWidth * x + y] = theID;
            theContinent = this->mContinents[theID];
            if (theID == 0)
                continue;
            this->mTileFlags[this->mFlagsWidth * theID + theID] = true;
            this->mTileFlags[this->mFlagsWidth * theID + theID] = true;
            if (inMachine.mTiles60[inMachine.mWidth * x + y].mKind == 5)
                continue;
            if (x - 1 >= 0 && y >= 0) {
                theOther = inMachine.mTiles60[inMachine.mWidth * (x - 1) + y].mID;
                if (theOther != theID && theOther != 0 && inMachine.mTiles60[inMachine.mWidth * (x - 1) + y].mKind != 5) {
                    this->mTileFlags[this->mFlagsWidth * theID + theOther] = true;
                    this->mTileFlags[this->mFlagsWidth * theOther + theID] = true;
                    theNeighbor = this->mContinents[theOther];
                    if (!theContinent->HasNeighbor(theNeighbor)) {
                        EEAIContinent* theEntry = theNeighbor;
                        theContinent->mNeighbors.push_back(theEntry);
                    }
                    if (!theNeighbor->HasNeighbor(theContinent)) {
                        EEAIContinent* theEntry = theContinent;
                        theNeighbor->mNeighbors.push_back(theEntry);
                    }
                }
            }
            if (x >= 0 && y - 1 >= 0) {
                theOther = inMachine.mTiles60[inMachine.mWidth * x + y - 1].mID;
                if (theOther != theID && theOther != 0 && inMachine.mTiles60[inMachine.mWidth * x + y - 1].mKind != 5) {
                    this->mTileFlags[this->mFlagsWidth * theID + theOther] = true;
                    this->mTileFlags[this->mFlagsWidth * theOther + theID] = true;
                    theNeighbor = this->mContinents[theOther];
                    if (!theContinent->HasNeighbor(theNeighbor)) {
                        EEAIContinent* theEntry = theNeighbor;
                        theContinent->mNeighbors.push_back(theEntry);
                    }
                    if (!theNeighbor->HasNeighbor(theContinent)) {
                        EEAIContinent* theEntry = theContinent;
                        theNeighbor->mNeighbors.push_back(theEntry);
                    }
                }
            }
            this->AddTileValue(theMap, x, y);
        }
    }
    for (i = this->mShoreTiles.size() - 1; i >= 0; i--) {
        theContinent = this->GetContinentAt(this->mShoreTiles[i].mX, this->mShoreTiles[i].mY);
        if (!theContinent)
            continue;
        theContinent->mShoreTiles.push_back(this->mShoreTiles[i]);
        if (theContinent->mCategory != kContinentCategoryWater && theContinent->mCategory != kContinentCategorySpace)
            continue;
        thePoint.mY = this->mShoreTiles[i].mY;
        thePoint.mX = this->mShoreTiles[i].mX - 1;
        this->AddShoreTile(thePoint, this->mShoreTiles[i], this->mShoreTiles);
        thePoint.mY = this->mShoreTiles[i].mY + 1;
        thePoint.mX = this->mShoreTiles[i].mX - 1;
        this->AddShoreTile(thePoint, this->mShoreTiles[i], this->mShoreTiles);
        thePoint.mY = this->mShoreTiles[i].mY - 1;
        thePoint.mX = this->mShoreTiles[i].mX - 1;
        this->AddShoreTile(thePoint, this->mShoreTiles[i], this->mShoreTiles);
        thePoint.mY = this->mShoreTiles[i].mY;
        thePoint.mX = this->mShoreTiles[i].mX + 1;
        this->AddShoreTile(thePoint, this->mShoreTiles[i], this->mShoreTiles);
        thePoint.mY = this->mShoreTiles[i].mY + 1;
        thePoint.mX = this->mShoreTiles[i].mX + 1;
        this->AddShoreTile(thePoint, this->mShoreTiles[i], this->mShoreTiles);
        thePoint.mY = this->mShoreTiles[i].mY - 1;
        thePoint.mX = this->mShoreTiles[i].mX + 1;
        this->AddShoreTile(thePoint, this->mShoreTiles[i], this->mShoreTiles);
        thePoint.mY = this->mShoreTiles[i].mY - 1;
        thePoint.mX = this->mShoreTiles[i].mX;
        this->AddShoreTile(thePoint, this->mShoreTiles[i], this->mShoreTiles);
        thePoint.mY = this->mShoreTiles[i].mY + 1;
        thePoint.mX = this->mShoreTiles[i].mX;
        this->AddShoreTile(thePoint, this->mShoreTiles[i], this->mShoreTiles);
    }
    std::sort(this->mShoreTiles.begin(), this->mShoreTiles.end());
    std::vector<U2DPoint<long> >::iterator theEnd = std::unique(this->mShoreTiles.begin(), this->mShoreTiles.end());
    if (theEnd != this->mShoreTiles.end())
        this->mShoreTiles.erase(theEnd, this->mShoreTiles.end());
    std::sort(this->mIceShoreTiles.begin(), this->mIceShoreTiles.end());
    for (j = this->mContinents.size() - 1; j > 0; j--) {
        if (this->mContinents[j])
            this->mContinents[j]->SortTiles(true);
    }
}

// FUNCTION: EXE 0x4201f7
// Numbers the continent chains of kind inKind (which of water, cliff and space a chain may cross): one search from
// every land continent without a chain id of that kind yet (inMap is not read)
void EEAIContinentManager::NumberContinentChains(EEMap* inMap, unsigned long inKind)
{
    EEAIContinentChainMachine theMachine(7, this->mContinents.size());
    EEAIAStarGraphStorage* theStorage = &theMachine;
    theStorage->Init(this->mContinents.size());
    if (inKind == kAIChainLandCliff || inKind == kAIChainLandWaterCliff || inKind == kAIChainLandCliffSpace)
        theMachine.mGoal.mKinds[3] = true;
    if (inKind == kAIChainLandWater || inKind == kAIChainLandWaterCliff || inKind == kAIChainLandWaterSpace)
        theMachine.mGoal.mKinds[2] = true;
    if (inKind == kAIChainLandSpace || inKind == kAIChainLandWaterSpace || inKind == kAIChainLandCliffSpace)
        theMachine.mGoal.mKinds[4] = true;
    theMachine.mGoal.mKinds[1] = true;
    for (long i = this->mContinents.size() - 1; i >= 0; i--) {
        EEAIContinent* theContinent = this->mContinents[i];
        if (!theContinent || theContinent->mCategory != kContinentCategoryLand)
            continue;
        unsigned short theCount = inKind < 7 ? theContinent->mCounts[inKind] : 0;
        if (theCount != 0)
            continue;
        unsigned short theID = theContinent->mID;
        theMachine.mGoal.mStartTile = theContinent->mHull.GetCenter();
        theMachine.mGoal.mGoalSector = theID;
        theMachine.mGoal.mStartContinent = theContinent;
        theMachine.mGoal.mStartCategory = theContinent->mCategory;
        theMachine.mGoal.mKind = inKind;
        theMachine.Run();
        theMachine.mGoal.NumberChain();
    }
}

// FUNCTION: EXE 0x42033f
// Grows the hills of inMap: one fill from every non-cliff land tile at height level 4 or more not yet in a hill,
// then registers each hill (serial, its continent, the area map)
void EEAIContinentManager::CreateHills(EEMap* inMap)
{
    EEAIHillMachine theMachine(10, inMap);
    EEAIHillMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(inMap->mSize, inMap->mSize);
    theMachine.mCheckCorners = false;
    for (long x = inMap->mSize - 1; x >= 0; x--) {
        for (long y = inMap->mSize - 1; y >= 0; y--) {
            EEAIContinent* theContinent = this->GetContinentAt(x, y);
            if (!theContinent || theContinent->mCategory == kContinentCategoryWater || theContinent->mCategory == kContinentCategorySpace ||
                theMachine.mTiles60[theMachine.mHeight * x + y].mID != 0)
                continue;
            EETile* theTile = inMap->GetTile(x, y);
            if (!theTile)
                continue;
            long theLevel = UMFloor(theTile->GetHeight() / (inMap->mF1955F4 * 0.5f) + 0.00001f);
            if (theTile->mTerrainType == kTerrainCliff || theLevel < 4)
                continue;
            theMachine.mGoal.mStartTile.mX = x;
            theMachine.mGoal.mStartTile.mY = y;
            theMachine.mGoal.mGoalTile.mX = inMap->mSize;
            theMachine.mGoal.mGoalTile.mY = inMap->mSize;
            theMachine.Run();
            theMachine.mGoal.CollectTiles();
            theMachine.mGoal.CreateHill();
        }
    }
    this->mHills.resize(theMachine.mGoal.mNextID, NULL);
    for (std::map<unsigned short, EEAIHill*>::const_iterator theIt = theMachine.mGoal.mHills.begin();
         theIt != theMachine.mGoal.mHills.end(); ++theIt) {
        EEAIHill* theHill = theIt->second;
        theHill->mSerial = ++this->mLastSerial;
        this->mHills[theHill->mID] = theHill;
        const std::vector<EEAIContinent*>& theContinents = this->mContinents;
        EEAIContinent* theContinent = theContinents[theHill->mContinentID];
        if (theContinent)
            theContinent->AddHillID(theHill->mID);
        this->AddArea(theHill);
    }
}

// FUNCTION: EXE 0x4204ea
// Assigns the inKind region ids (water body, land mass, space or surface region) to the path areas: one fill from
// every matching tile whose area has no such id yet, then keeps the next id of that kind
void EEAIContinentManager::CreatePathAreaKind(EEMap* inMap, long inKind)
{
    EEAIPathAreaMachine theMachine(0xe, inMap);
    long theSize = inMap->mSize;
    EEAIPathArea** theAreas = NULL;
    bool theFlag = false;
    EEAIPathAreaMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(theSize, theSize);
    theMachine.mCheckCorners = true;
    theAreas = new EEAIPathArea*[theSize * theSize];
    this->GetPathAreas(inMap, theAreas);
    theMachine.mGoal.mAreas = theAreas;
    theMachine.mGoal.mAreasSize = theSize;
    for (long x = inMap->mSize - 1; x >= 0; x--) {
        for (long y = inMap->mSize - 1; y >= 0; y--) {
            EETile* theTile = inMap->GetTile(x, y);
            if (!theTile)
                continue;
            if (inKind == kAIPathWaterBody) {
                if (theTile->mTerrainType != kTerrainDeepWater && theTile->mTerrainType != kTerrainShallowWater)
                    continue;
                theFlag = true;
            } else if (inKind == kAIPathSpaceRegion) {
                theFlag = theTile->mTerrainType == kTerrainSpace;
                if (!theFlag)
                    continue;
            } else if (inKind == kAIPathLandMass) {
                if (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainSpace)
                    continue;
                theFlag = true;
            } else {
                if (inKind == kAIPathNonSpace) {
                    if (theTile->mTerrainType != kTerrainSpace && x + y != inMap->mL195608 && y - x != inMap->mL195610 &&
                        x + y != inMap->mL19560C && x - y != inMap->mL195610)
                        theFlag = true;
                    else
                        theFlag = false;
                }
                if (!theFlag)
                    continue;
            }
            EEAIPathArea* theArea = theAreas[x * theSize + y];
            if (!theArea)
                continue;
            bool theRun = false;
            if (inKind == kAIPathWaterBody) {
                if (theArea->mKind == kAITileForest || theArea->mWaterBodyID != 0)
                    continue;
                if (theArea->mKind == kAITileWater) {
                    theRun = true;
                } else if (theArea->mKind <= kAITileMapEdge) {
                    continue;
                } else if (theArea->mKind < kAITileUnitObstacle) {
                    theRun = true;
                } else if (theArea->mKind == kAITileUnitObstacle) {
                    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                        EEUnit* theUnit = theNode->mUnit;
                        if ((theUnit->GetUnitClass()->mUnitType == kUnitTypeDock || theUnit->GetUnitClass()->mUnitType == kUnitTypeNavyYard) &&
                            theUnit->mHitPoints > 0 && !theUnit->IsFoundation()) {
                            theRun = true;
                            break;
                        }
                    }
                }
            } else if (inKind == kAIPathLandMass) {
                if (theArea->mKind != kAITileForest && theArea->mLandMassID == 0)
                    theRun = true;
            } else if (inKind == kAIPathSpaceRegion) {
                if (theArea->mKind == kAITileForest || theArea->mSpaceRegionID != 0)
                    continue;
                if (theArea->mKind == kAITileUnitObstacle) {
                    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                        EEUnit* theUnit = theNode->mUnit;
                        unsigned long theType = theUnit->GetUnitClass()->mUnitType;
                        if ((theType == kUnitTypeSpaceDock || theType == kUnitTypeSpaceTurret || theType == kUnitTypeOrbitalSpaceStation) && theUnit->mHitPoints > 0 &&
                            !theUnit->IsFoundation()) {
                            theRun = true;
                            break;
                        }
                    }
                } else if (theArea->mKind == kAITileSpace) {
                    theRun = true;
                }
            } else if (inKind == kAIPathNonSpace) {
                if (theArea->mSurfaceRegionID == 0)
                    theRun = true;
            }
            if (!theRun)
                continue;
            theMachine.mGoal.mMode = inKind;
            theMachine.mCheckCorners = theTile->mTerrainType != 3 && x + y != inMap->mL195608 &&
                y - x != inMap->mL195610 && x + y != inMap->mL19560C && x - y != inMap->mL195610;
            theMachine.mGoal.mStartTile.mY = y;
            theMachine.mGoal.mStartTile.mX = x;
            theMachine.mGoal.mGoalTile.mX = inMap->mSize;
            theMachine.mGoal.mGoalTile.mY = inMap->mSize;
            theMachine.Run();
            theMachine.mGoal.CollectTiles();
            theMachine.mGoal.CreateAreas();
        }
    }
    if (inKind == kAIPathLandMass)
        this->mNextID124 = theMachine.mGoal.mNextKindID;
    else if (inKind == kAIPathWaterBody)
        this->mNextID128 = theMachine.mGoal.mNextKindID;
    else if (inKind == kAIPathSpaceRegion)
        this->mNextID12C = theMachine.mGoal.mNextKindID;
    else if (inKind == kAIPathNonSpace)
        this->mNextID130 = theMachine.mGoal.mNextKindID;
    delete[] theAreas;
    theMachine.mHasBounds = false;
    theMachine.Reset();
}

// FUNCTION: EXE 0x4208cc
// Grows the path areas and forests of inMap and registers them (serials, continents, area map), builds the path
// sectors, then records each sub-sector's continent as a neighbour of its sector
void EEAIContinentManager::CreatePathAreas(EEMap* inMap)
{
    EEAIPathAreaMachine theMachine(0xe, inMap);
    EEAIPathAreaMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(inMap->mSize, inMap->mSize);
    theMachine.mCheckCorners = true;
    for (long x = inMap->mSize - 1; x >= 0; x--) {
        for (long y = inMap->mSize - 1; y >= 0; y--) {
            if (!this->GetContinentAt(x, y) || theMachine.mPathAreaIDs[theMachine.mHeight * x + y] != 0)
                continue;
            theMachine.mGoal.mStartTile.mX = x;
            theMachine.mGoal.mStartTile.mY = y;
            theMachine.mGoal.mGoalTile.mX = inMap->mSize;
            theMachine.mGoal.mGoalTile.mY = inMap->mSize;
            theMachine.Run();
            theMachine.mGoal.CollectTiles();
            theMachine.mGoal.CreateAreas();
        }
    }
    unsigned short theCount = theMachine.mGoal.mNextPathAreaID;
    this->mPathAreas.resize(theCount, NULL);
    this->mNextPathAreaID = theCount;
    std::map<unsigned short, EEAIPathArea*>::const_iterator theArea;
    for (theArea = theMachine.mGoal.mPathAreas.begin(); theArea != theMachine.mGoal.mPathAreas.end(); ++theArea) {
        EEAIPathArea* thePathArea = theArea->second;
        thePathArea->mSerial = ++this->mLastSerial;
        this->mPathAreas[thePathArea->mID] = thePathArea;
        EEAIContinent* theContinent = this->GetContinentByID(thePathArea->mContinentID);
        if (theContinent)
            theContinent->AddPathAreaID(thePathArea->mID);
        this->AddArea(thePathArea);
    }
    this->mForests.resize(theMachine.mGoal.mNextForestID, NULL);
    std::map<unsigned short, EEAIForest*>::const_iterator theForest;
    for (theForest = theMachine.mGoal.mForests.begin(); theForest != theMachine.mGoal.mForests.end(); ++theForest) {
        EEAIForest* theArea = theForest->second;
        theArea->mSerial = ++this->mLastSerial;
        this->mForests[theArea->mID] = theArea;
        EEAIContinent* theContinent = this->mContinents[theArea->mContinentID];
        if (theContinent)
            theContinent->AddForestID(theArea->mID);
        this->AddArea(theArea);
    }
    theMachine.mHasBounds = false;
    theMachine.Reset();
    this->CreatePathSectors(inMap, &theMachine);
    for (long i = this->mPathSubSectors.size() - 1; i >= 0; i--) {
        EEAIPathSubSector* theSubSector = this->mPathSubSectors[i];
        if (theSubSector && this->mContinents[theSubSector->mContinentID])
            theSubSector->mSector->AddNeighbor(reinterpret_cast<long>(this->mContinents[theSubSector->mContinentID]));
    }
}

// FUNCTION: EXE 0x420b48
// Builds the path-sector grid of inMap (sector and region size by map size) and the sub-sectors (one fill per tile
// not yet in one, kept inside its sector), files them with their path areas, then links them (LinkSubSectorGrid)
void EEAIContinentManager::CreatePathSectors(EEMap* inMap, EEAIAStarPathAreaStorage* inStorage)
{
    EEAIPathSectorMachine theMachine(0xf, inMap);
    U2DRectangle<long> theBounds;
    U2DPoint<long> theSector;
    long theSectorSize = 7;
    unsigned short theID = 1;
    this->DeletePathSectors();
    long theSize = inMap->mL195610;
    if (theSize <= 0x2a) {
        theSectorSize = 7;
        gEEAIRegionSize = 8;
    } else if (theSize <= 0x3a) {
        gEEAIRegionSize = 10;
        theSectorSize = gEEAIRegionSize;
    } else if (theSize <= 0x4b) {
        gEEAIRegionSize = 0xc;
        theSectorSize = gEEAIRegionSize;
    } else if (theSize <= 0x69) {
        theSectorSize = 0xf;
        gEEAIRegionSize = 0xe;
    } else if (theSize <= 0xa0) {
        theSectorSize = 0x13;
        gEEAIRegionSize = 0x10;
    } else if (theSize <= 0xdc) {
        theSectorSize = 0x15;
        gEEAIRegionSize = 0x12;
    } else {
        gEEAIRegionSize = 0xe;
    }
    EEAIPathSectorMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(inMap->mSize, inMap->mSize);
    this->mRegionInformation.Init(inMap, theSectorSize);
    this->mPathSectors.resize(this->mRegionInformation.mSectorsX * this->mRegionInformation.mSectorsY + 1, NULL);
    for (long x = 0; x < this->mRegionInformation.mSectorsX; x++) {
        for (long y = 0; y < this->mRegionInformation.mSectorsY; y++) {
            EEAIPathSector* thePathSector = new EEAIPathSector;
            theSector.mX = x;
            theSector.mY = y;
            thePathSector->mID = theID;
            theBounds.mTop = this->mRegionInformation.mSectorSize * y;
            theBounds.mLeft = this->mRegionInformation.mSectorSize * x;
            if (theBounds.mLeft < 0)
                theBounds.mLeft = 0;
            if (theBounds.mTop < 0)
                theBounds.mTop = 0;
            theID++;
            U2DPoint<long> theEnd = this->mRegionInformation.GetSectorEnd(theSector);
            theBounds.mBottom = inMap->mSize - 1;
            theBounds.mRight = theEnd.mX;
            if (theBounds.mBottom <= theEnd.mX)
                theBounds.mRight = theBounds.mBottom;
            if (theEnd.mY < theBounds.mBottom)
                theBounds.mBottom = theEnd.mY;
            thePathSector->mCenter.mX = theSector.mX;
            thePathSector->mCenter.mY = theSector.mY;
            thePathSector->mBounds = theBounds;
            this->SetPathSector(thePathSector);
        }
    }
    theMachine.mCheckCorners = false;
    theMachine.mGoal.mPathAreaStorage = inStorage;
    for (long i = inMap->mSize - 1; i >= 0; i--) {
        for (long j = inMap->mSize - 1; j >= 0; j--) {
            if (theMachine.mSubSectorIDs[theMachine.mHeight * i + j] != 0 || !inMap->GetTile(i, j))
                continue;
            EETileCoordinate theTile;
            theTile.mX = i;
            theTile.mY = j;
            EEAIPathSector* thePathSector = this->GetPathSector(theTile);
            theMachine.mGoal.mSectorBounds = thePathSector->mBounds;
            theMachine.mGoal.mSector = thePathSector;
            theMachine.mGoal.mStartTile.mX = i;
            theMachine.mGoal.mStartTile.mY = j;
            theMachine.mGoal.mGoalTile.mX = inMap->mSize;
            theMachine.mGoal.mGoalTile.mY = inMap->mSize;
            theMachine.Run();
            theMachine.mGoal.CollectTiles();
            theMachine.mGoal.CreateSubSector();
        }
    }
    this->mPathSubSectors.resize(this->mLastSubSectorID + 1, NULL);
    for (std::map<unsigned short, EEAIPathSubSector*>::const_iterator theIt = theMachine.mGoal.mSubSectors.begin();
         theIt != theMachine.mGoal.mSubSectors.end(); ++theIt) {
        EEAIPathSubSector* theSubSector = theIt->second;
        theSubSector->UpdateLocation();
        this->mPathSubSectors[theIt->first] = theSubSector;
        EEAIPathArea* theArea = this->GetPathArea(theSubSector->mPathAreaID);
        if (theArea) {
            EETileCoordinate theTile;
            theTile = theSubSector->mLocation;
            EEAIPathSector* thePathSector = this->GetPathSector(theTile);
            if (thePathSector) {
                theArea->AddSectorID(thePathSector->mID);
                thePathSector->AddPathAreaID(theArea->mID);
            }
        }
    }
    this->LinkSubSectorGrid(theMachine, inMap);
}

// FUNCTION: EXE 0x420ee1
// Copies the fill's sub-sector ids into the id grid and links every pair of neighbouring sub-sectors; a diagonal
// link carries the two corner sub-sectors when both are open
void EEAIContinentManager::LinkSubSectorGrid(EEAIPathSectorMachine& inMachine, EEMap* inMap)
{
    std::vector<EEAIPathSubSector*>* theSubSectors = &this->mPathSubSectors;
    std::pair<EEAIPathSubSector*, EEAIPathSubSector*> theLink(NULL, NULL);
    unsigned short theOther = 0;
    delete[] this->mPathSubSectorIDs;
    this->mPathSubSectorIDs = new unsigned short[this->mHeight * this->mWidth];
    long theWidth = inMachine.mWidth;
    long theHeight = inMachine.mHeight;
    for (long x = 0; x < theWidth; x++) {
        for (long y = 0; y < theHeight; y++) {
            unsigned short theID = inMachine.mSubSectorIDs[inMachine.mHeight * x + y];
            this->mPathSubSectorIDs[this->mWidth * x + y] = theID;
            theLink.first = NULL;
            theLink.second = NULL;
            if (theID == 0)
                continue;
            EEAIPathSubSector* theSubSector = this->mPathSubSectors[theID];
            if (x - 1 >= 0 && y >= 0) {
                theOther = inMachine.mSubSectorIDs[inMachine.mHeight * (x - 1) + y];
                if (theOther != theID && theOther != 0)
                    this->LinkSubSectors(&theSubSectors, theSubSector, &theOther, false, theLink);
            }
            if (x >= 0 && y - 1 >= 0) {
                theOther = inMachine.mSubSectorIDs[inMachine.mHeight * x + y - 1];
                if (theOther != theID && theOther != 0)
                    this->LinkSubSectors(&theSubSectors, theSubSector, &theOther, false, theLink);
            }
            if (x - 1 >= 0 && y - 1 >= 0) {
                theOther = inMachine.mSubSectorIDs[inMachine.mHeight * (x - 1) + y - 1];
                if (theOther != theID && theOther != 0) {
                    long theCount = 0;
                    if (y >= 0) {
                        unsigned short theFirst = inMachine.mSubSectorIDs[inMachine.mHeight * (x - 1) + y];
                        if (theFirst != theID && theFirst != theOther && theFirst != 0 &&
                            (theLink.first = (*theSubSectors)[theFirst]) != NULL) {
                            theCount = 1;
                            if (x >= 0) {
                                unsigned short theSecond = inMachine.mSubSectorIDs[inMachine.mHeight * x + y - 1];
                                if (theSecond != theID && theSecond != theOther && theSecond != 0 &&
                                    (theLink.second = (*theSubSectors)[theSecond]) != NULL)
                                    theCount = 2;
                            }
                        }
                    }
                    if (theCount != 2) {
                        theLink.first = NULL;
                        theLink.second = NULL;
                    }
                    this->LinkSubSectors(&theSubSectors, theSubSector, &theOther, theCount == 2, theLink);
                }
            }
            if (!(x + 1 < theWidth) || !(y - 1 >= 0))
                continue;
            theOther = inMachine.mSubSectorIDs[inMachine.mHeight * (x + 1) + y - 1];
            if (theOther == theID || theOther == 0)
                continue;
            long theCount = 0;
            if (y < theHeight) {
                unsigned short theFirst = inMachine.mSubSectorIDs[inMachine.mHeight * (x + 1) + y];
                if (theFirst != theID && theFirst != theOther && theFirst != 0 &&
                    (theLink.first = (*theSubSectors)[theFirst]) != NULL) {
                    theCount = 1;
                    if (x >= 0) {
                        unsigned short theSecond = inMachine.mSubSectorIDs[inMachine.mHeight * x + y - 1];
                        if (theSecond != theID && theSecond != theOther && theSecond != 0 &&
                            (theLink.second = (*theSubSectors)[theSecond]) != NULL)
                            theCount = 2;
                    }
                }
            }
            if (theCount != 2) {
                theLink.first = NULL;
                theLink.second = NULL;
            }
            this->LinkSubSectors(&theSubSectors, theSubSector, &theOther, theCount == 2, theLink);
        }
    }
}

// FUNCTION: EXE 0x42124c
// Creates one rectangle area per scenario area with a non-empty rectangle; the list is sized counting only anchored
// areas, so an anchorless one is stored past its end
void EEAIContinentManager::CreateScenarioAreas(EEMap* inMap)
{
    std::vector<U2DPoint<long> > theTiles;
    U2DRectangle<long> theRect;
    unsigned short theID = 1;
    long theCount = 0;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    if (!theManager)
        return;
    long i;
    for (i = theManager->mAreas.size() - 1; i >= 0; i--) {
        if (theManager->mAreas[i] && !theManager->mAreas[i]->IsUnset() &&
            theManager->mAreas[i]->mRect.GetHeight() > 0.0f &&
            theManager->mAreas[i]->mRect.GetWidth() > 0.0f)
            theCount++;
    }
    if (theCount <= 0)
        return;
    this->mRectangleAreas.resize(theCount + 1, NULL);
    for (i = this->mRectangleAreas.size() - 1; i >= 0; i--)
        this->mRectangleAreas[i] = NULL;
    for (i = theManager->mAreas.size() - 1; i >= 0; i--) {
        EEAIRectangleArea* theArea = NULL;
        // the areas the count above admitted: unset areas too are skipped (they got no slot)
        if (!theManager->mAreas[i] || theManager->mAreas[i]->IsUnset() ||
            !(theManager->mAreas[i]->mRect.GetHeight() > 0.0f) ||
            !(theManager->mAreas[i]->mRect.GetWidth() > 0.0f))
            continue;
        theArea = new EEAIRectangleArea;
        theArea->mNumLandTiles = 0;
        theArea->mNumWaterTiles = 0;
        theArea->mNumSpaceTiles = 0;
        theArea->mID = theID++;
        theArea->mSerial = ++this->mLastSerial;
        theArea->mAreaType = kAIAreaRectangleArea;
        theArea->mScenarioAreaID = theManager->mAreas[i]->mID;
        theRect.mTop = (long)(theManager->mAreas[i])->mRect.mTop;
        theRect.mBottom = (long)(theManager->mAreas[i])->mRect.mBottom;
        theRect.mLeft = (long)(theManager->mAreas[i])->mRect.mLeft;
        theRect.mRight = (long)(theManager->mAreas[i])->mRect.mRight;
        theArea->mRect = theRect;
        theTiles.clear();
        for (long x = theRect.mLeft; x <= theRect.mRight; x++) {
            for (long y = theRect.mTop; y <= theRect.mBottom; y++)
                theTiles.push_back(U2DPoint<long>(x, y));
        }
        theArea->Create(theTiles);
        this->AddRectangleArea(theArea);
    }
}

// FUNCTION: EXE 0x4215dc
// After a partial rebuild: writes the new sub-sectors into the id grid, then re-links the sub-sectors of every tile
// of the sectors in inSectors (grown by one tile on each side)
void EEAIContinentManager::RelinkSubSectors(std::map<unsigned short, std::vector<EETileCoordinate> >& inSectors,
    std::vector<EEAIPathSubSector*>& inSubSectors)
{
    std::vector<EEAIPathSubSector*>* theSubSectors = &this->mPathSubSectors;
    std::pair<EEAIPathSubSector*, EEAIPathSubSector*> theLink(NULL, NULL);
    std::vector<U2DPoint<long> > theTiles;
    unsigned short theOther = 0;
    long i;
    for (i = inSubSectors.size() - 1; i >= 0; i--) {
        EEAIPathSubSector* theSubSector = *(inSubSectors.begin() + i);
        theSubSector->mTiles.BuildPointList(theTiles, 1);
        if (!this->mPathSubSectorIDs)
            continue;
        for (long j = theTiles.size() - 1; j >= 0; j--) {
            unsigned short theID = theSubSector->mID;
            this->mPathSubSectorIDs[theTiles[j].mX * this->mWidth + (theTiles.begin() + j)->mY] = theID;
        }
    }
    for (std::map<unsigned short, std::vector<EETileCoordinate> >::iterator theIt = inSectors.begin(); theIt != inSectors.end(); ++theIt) {
        EEAIPathSector* theSector = this->GetPathSectorByID(theIt->first);
        long theSize = gWorld.mMap->mSize;
        long theEndX = theSector->mBounds.mRight + 2;
        if (theSize <= theEndX)
            theEndX = theSize;
        long theEndY = theSector->mBounds.mBottom + 2;
        if (theSize <= theEndY)
            theEndY = theSize;
        long x = theSector->mBounds.mLeft - 1;
        if (x < 1)
            x = 0;
        for (; x < theEndX; x++) {
            long y = theSector->mBounds.mTop - 1;
            if (y < 1)
                y = 0;
            for (; y < theEndY; y++) {
                unsigned short theID = this->GetPathSubSectorID(x, y);
                if (this->mPathSubSectorIDs)
                    this->mPathSubSectorIDs[this->mWidth * x + y] = theID;
                theLink.first = NULL;
                theLink.second = NULL;
                if (theID == 0)
                    continue;
                EEAIPathSubSector* theSubSector = this->mPathSubSectors[theID];
                if (x - 1 >= 0 && y >= 0) {
                    theOther = this->GetPathSubSectorID(x - 1, y);
                    if (theOther != theID && theOther != 0)
                        this->LinkSubSectors(&theSubSectors, theSubSector, &theOther, false, theLink);
                }
                if (x >= 0 && y - 1 >= 0) {
                    theOther = this->GetPathSubSectorID(x, y - 1);
                    if (theOther != theID && theOther != 0)
                        this->LinkSubSectors(&theSubSectors, theSubSector, &theOther, false, theLink);
                }
                if (x - 1 >= 0 && y - 1 >= 0) {
                    theOther = this->GetPathSubSectorID(x - 1, y - 1);
                    if (theOther != theID && theOther != 0) {
                        long theCount = 0;
                        if (y >= 0) {
                            unsigned short theFirst = this->GetPathSubSectorID(x - 1, y);
                            if (theFirst != theID && theFirst != theOther && theFirst != 0 &&
                                (theLink.first = (*theSubSectors)[theFirst]) != NULL) {
                                theCount = 1;
                                if (x >= 0) {
                                    unsigned short theSecond = this->GetPathSubSectorID(x, y - 1);
                                    if (theSecond != theID && theSecond != theOther && theSecond != 0 &&
                                        (theLink.second = (*theSubSectors)[theSecond]) != NULL)
                                        theCount = 2;
                                }
                            }
                        }
                        if (theCount != 2) {
                            theLink.first = NULL;
                            theLink.second = NULL;
                        }
                        this->LinkSubSectors(&theSubSectors, theSubSector, &theOther, theCount == 2, theLink);
                    }
                }
                if (!(x + 1 < theEndX) || !(y - 1 >= 0))
                    continue;
                theOther = this->GetPathSubSectorID(x + 1, y - 1);
                if (theOther == theID || theOther == 0)
                    continue;
                long theCount = 0;
                if (y < theEndY) {
                    unsigned short theFirst = this->GetPathSubSectorID(x + 1, y);
                    if (theFirst != theID && theFirst != theOther && theFirst != 0 &&
                        (theLink.first = (*theSubSectors)[theFirst]) != NULL) {
                        theCount = 1;
                        if (x >= 0) {
                            unsigned short theSecond = this->GetPathSubSectorID(x, y - 1);
                            if (theSecond != theID && theSecond != theOther && theSecond != 0 &&
                                (theLink.second = (*theSubSectors)[theSecond]) != NULL)
                                theCount = 2;
                        }
                    }
                }
                if (theCount != 2) {
                    theLink.first = NULL;
                    theLink.second = NULL;
                }
                this->LinkSubSectors(&theSubSectors, theSubSector, &theOther, theCount == 2, theLink);
            }
        }
    }
}

// FUNCTION: EXE 0x421a18
// Returns the inKind region id (land mass, water body, space or surface region) of a fitting path area next to
// inTile, 0 if none; land mass skips water, cliff..forest and map-edge areas unless of kind inAreaKind
unsigned short EEAIContinentManager::GetAdjacentAreaID(long inKind, const EETileLocation& inTile, long inAreaKind)
{
    std::vector<U2DPoint<long> > theTiles;
    unsigned short theResult = 0;
    EEMap* theMap = gWorld.mMap;
    if (theMap) {
        if (inKind == kAIPathLandMass) {
            theTiles.reserve(8);
            theTiles.push_back(U2DPoint<long>(inTile.mX - 1, inTile.mY - 1));
            theTiles.push_back(U2DPoint<long>(inTile.mX - 1, inTile.mY));
            theTiles.push_back(U2DPoint<long>(inTile.mX - 1, inTile.mY + 1));
            theTiles.push_back(U2DPoint<long>(inTile.mX, inTile.mY - 1));
            theTiles.push_back(U2DPoint<long>(inTile.mX, inTile.mY + 1));
            theTiles.push_back(U2DPoint<long>(inTile.mX + 1, inTile.mY - 1));
            theTiles.push_back(U2DPoint<long>(inTile.mX + 1, inTile.mY));
            theTiles.push_back(U2DPoint<long>(inTile.mX + 1, inTile.mY + 1));
            for (long i = theTiles.size() - 1; i >= 0; i--) {
                if (!theMap->GetTile(theTiles[i].mX, theTiles[i].mY))
                    continue;
                EEAIPathArea* theArea = this->GetPathAreaAt(theTiles[i]);
                if (theArea && theArea->mKind != kAITileWater &&
                    (theArea->mKind < kAITileCliff || theArea->mKind > kAITileForest && theArea->mKind != kAITileMapEdge || inAreaKind == theArea->mKind)) {
                    theResult = theArea->mLandMassID;
                    i = -1;
                }
            }
        } else if (theMap->GetTile(inTile.mX, inTile.mY) &&
                   (inKind == kAIPathWaterBody ? theMap->GetTile(inTile.mX, inTile.mY)->mTerrainType == kTerrainDeepWater ||
                                  theMap->GetTile(inTile.mX, inTile.mY)->mTerrainType == kTerrainShallowWater
                                : inKind == kAIPathSpaceRegion && theMap->GetTile(inTile.mX, inTile.mY)->mTerrainType == kTerrainSpace ||
                                  inKind == kAIPathNonSpace && theMap->GetTile(inTile.mX, inTile.mY)->mTerrainType != kTerrainSpace)) {
            theTiles.reserve(8);
            theTiles.push_back(U2DPoint<long>(inTile.mX - 1, inTile.mY - 1));
            theTiles.push_back(U2DPoint<long>(inTile.mX - 1, inTile.mY));
            theTiles.push_back(U2DPoint<long>(inTile.mX - 1, inTile.mY + 1));
            theTiles.push_back(U2DPoint<long>(inTile.mX, inTile.mY - 1));
            theTiles.push_back(U2DPoint<long>(inTile.mX, inTile.mY + 1));
            theTiles.push_back(U2DPoint<long>(inTile.mX + 1, inTile.mY - 1));
            theTiles.push_back(U2DPoint<long>(inTile.mX + 1, inTile.mY));
            theTiles.push_back(U2DPoint<long>(inTile.mX + 1, inTile.mY + 1));
            for (long i = theTiles.size() - 1; i >= 0; i--) {
                if (!theMap->GetTileAtLocation(theTiles[i]))
                    continue;
                if (inKind == kAIPathWaterBody) {
                    EETile* theTile = theMap->GetTileAtLocation(theTiles[i]);
                    if (theTile->mTerrainType != kTerrainDeepWater && theTile->mTerrainType != kTerrainShallowWater)
                        continue;
                } else if (!(inKind == kAIPathSpaceRegion && theMap->GetTileAtLocation(theTiles[i])->mTerrainType == kTerrainSpace)) {
                    if (inKind != kAIPathNonSpace || theMap->GetTileAtLocation(theTiles[i])->mTerrainType == kTerrainSpace)
                        continue;
                }
                EEAIPathArea* theArea = this->GetPathAreaAt(theTiles[i]);
                if (!theArea)
                    continue;
                if (inKind == kAIPathWaterBody)
                    theResult = theArea->mWaterBodyID;
                else if (inKind == kAIPathSpaceRegion)
                    theResult = theArea->mSpaceRegionID;
                else
                    theResult = theArea->mSurfaceRegionID;
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x421e22
// Lists the continents adjacent to continent inID, itself included; empty for an unknown id
void EEAIContinentManager::GetConnectedIDs(std::vector<unsigned short>& outIDs, unsigned short inID)
{
    outIDs.clear();
    if (!(inID < this->mFlagsWidth))
        return;
    for (long i = this->mFlagsWidth - 1; i >= 0; i--) {
        if (this->mTileFlags[inID * this->mFlagsWidth + (unsigned short)i] && i != 0) {
            unsigned short theID = (unsigned short)i;
            outIDs.push_back(theID);
        }
    }
}

// FUNCTION: EXE 0x421e80
// Lists the continents adjacent to continent inID, itself excluded; empty for an unknown id
void EEAIContinentManager::GetAdjacentIDs(std::vector<unsigned short>& outIDs, unsigned short inID)
{
    outIDs.clear();
    if (!(inID < this->mFlagsWidth))
        return;
    for (long i = this->mFlagsWidth - 1; i >= 0; i--) {
        if (this->mTileFlags[inID * this->mFlagsWidth + (unsigned short)i] && i != 0 && i != inID) {
            unsigned short theID = (unsigned short)i;
            outIDs.push_back(theID);
        }
    }
}

// FUNCTION: EXE 0x421ee2
// Returns whether any tile of inTiles is a shore tile
bool EEAIContinentManager::HasShoreTile(const std::vector<U2DPoint<long> >& inTiles) const
{
    bool theResult = false;
    std::vector<U2DPoint<long> >::const_iterator theIt = inTiles.begin();
    while (theIt != inTiles.end()) {
        theResult = std::binary_search(this->mShoreTiles.begin(), this->mShoreTiles.end(), *theIt);
        ++theIt;
        if (theResult)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x421f13
// Returns whether two units stand on the same continent
bool EEAIContinentManager::InSameContinent(EEUnit* inFirst, EEUnit* inSecond)
{
    return this->GetContinentID(inFirst->GetTileX(), inFirst->GetTileY()) == this->GetContinentID(inSecond->GetTileX(), inSecond->GetTileY());
}

// FUNCTION: EXE 0x421f5a
// Returns the map tile's AI value (shore or wood value) at (inX, inY), 0 off the map
long EEAIContinentManager::GetTileFlag(long inX, long inY)
{
    EETile* theTile = gWorld.mMap->GetTile(inX, inY);
    return theTile ? theTile->mAIValue : 0;
}

// FUNCTION: EXE 0x421f97
// Returns whether every tile of inTiles has the same inKind region id (true for an empty list)
bool EEAIContinentManager::InSameArea(const std::vector<EETileCoordinate>& inTiles, long inKind)
{
    unsigned short theFirst = 0;
    bool theResult = true;
    if (inTiles.size() > 0) {
        for (std::vector<EETileCoordinate>::const_iterator theIt = inTiles.begin(); theResult && theIt != inTiles.end();
             ++theIt) {
            unsigned short theID = this->GetContinent(*theIt, inKind);
            if (theFirst == 0)
                theFirst = theID;
            theResult = theID == theFirst;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x421fe7
// Returns whether inUnit can reach inTarget's tile (CanGetTo)
bool EEAIContinentManager::CanGetToUnit(EEUnit* inUnit, EEUnit* inTarget)
{
    return this->CanGetTo(inUnit, inTarget->GetTileX(), inTarget->GetTileY());
}

// FUNCTION: EXE 0x421fff
// Returns whether inUnit can reach tile (inX, inY): same continent unless air-blocked, else by its complex unit:
// planet ships always, aircraft by planet on space maps, ships by water body or space region
bool EEAIContinentManager::CanGetTo(EEUnit* inUnit, long inX, long inY)
{
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    bool theResult = false;
    std::pair<long, int> theTile(inUnit->mTileX, inUnit->mTileY);
    if ((unsigned long)theTile.first < this->mWidth && (unsigned long)theTile.second < this->mHeight &&
        (unsigned long)inX < this->mWidth && (unsigned long)inY < this->mHeight) {
        EEAIContinent* theFrom = this->GetContinentAt(inUnit);
        EEAIContinent* theTo = this->GetContinentAt(inX, inY);
        bool theBlocked;
        if ((inUnit->mUnitClass->mDomains & kMovementAir) == 0 || inUnit->mUnitClass->CanMoveOnTerrain(inUnit, 0xc))
            theBlocked = false;
        else
            theBlocked = true;
        if (theFrom == theTo && !theBlocked) {
            theResult = true;
        } else {
            theResult = false;
            if (theComplexUnit) {
                EEUnitClass* theClass = theComplexUnit->mUnitClass;
                if (theClass->mCategory == kUnitCategoryPlanetSpaceship) {
                    theResult = true;
                } else {
                    unsigned char theFlags = theClass->mDomains;
                    unsigned short theID;
                    if ((theFlags & kMovementSeaOrSpace) == 0) {
                        if ((theFlags & kMovementAir) != 0 || theClass->mUnitType == kUnitTypeAirport) {
                            // a map without space tiles needs no planet check
                            if (!gWorld.mMap->mTerrainMesh.GetIsSpaceMap() || !theBlocked) {
                                theResult = true;
                            } else {
                                if ((theFlags & kMovementAir) != 0) {
                                    theID = theComplexUnit->mPlanetID;
                                    if (theID == 0) {
                                        EEDebugTrace("EEAIContinentManager::GetCanPathToContinent: no planet ID for airplane %d of type %s\n",
                                                     theComplexUnit->GetUnitID(), theComplexUnit->GetUnitClass()->GetName());
                                        theID = EEGetLandContinentOf(theComplexUnit);
                                        theComplexUnit->mPlanetID = theID;
                                    }
                                } else {
                                    theID = this->GetContinent(theComplexUnit, kAIPathNonSpace);
                                }
                                if (theID != 0) {
                                    EETileCoordinate theTarget;
                                    theTarget.mX = inX;
                                    theTarget.mY = inY;
                                    if (theID == this->GetContinent(theTarget, kAIPathNonSpace))
                                        theResult = true;
                                    else
                                        theResult = false;
                                }
                            }
                        }
                    } else {
                        long theKind = (theFlags & kMovementSea) ? 1 : 3;
                        if (inUnit->mComplexUnit)
                            theID = inUnit->mComplexUnit->GetFootprintContinent(theKind);
                        else
                            theID = this->GetContinent(inUnit, theKind);
                        if (theID != 0) {
                            EETileCoordinate theTarget;
                            theTarget.mX = inX;
                            theTarget.mY = inY;
                            if (theID == this->GetContinent(theTarget, theKind))
                                theResult = true;
                            else
                                theResult = false;
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4221c1
// Returns the choke point at inUnit's tile, NULL if none
EEAIChokePoint* EEAIContinentManager::GetChokePointAt(EEUnit* inUnit)
{
    EETileLocation theTile(inUnit->mTileX, inUnit->mTileY);
    return this->GetChokePointAt(theTile);
}

// FUNCTION: EXE 0x4221e2
// Returns the choke point of inTile's land continent that contains inTile, NULL if none
EEAIChokePoint* EEAIContinentManager::GetChokePointAt(const EETileLocation& inTile)
{
    EEAIContinent* theContinent = this->GetContinentAt(inTile);
    EEAIChokePoint* theChokePoint = NULL;
    if (theContinent && theContinent->mCategory == kContinentCategoryLand) {
        const std::vector<long>& theIDs = theContinent->mChokePoints;
        const std::vector<EEAIChokePoint*>& theChokePoints = this->mChokePoints;
        for (long i = theIDs.size() - 1; i >= 0; i--) {
            theChokePoint = theChokePoints[theIDs[i]];
            if (theChokePoint && theChokePoint->Contains(inTile))
                break;
            theChokePoint = NULL;
        }
    }
    return theChokePoint;
}

// FUNCTION: EXE 0x422255
// Appends the choke points of continent inID (every choke point for id 0) to outChokePoints, cleared first if inClear
void EEAIContinentManager::GetChokePoints(std::vector<EEAIChokePoint*>& outChokePoints, unsigned short inID, bool inClear)
{
    EE_LOST_DEBUG(3);  // LOST-DEBUG: none: exact only (lostdebug_search, smallest n; inline_sim: the mChokePoints[...] _Ptrit::operator+ at line 1569 is out of line in the orig)
    long i;
    EEAIChokePoint* theChokePoint;
    const EEAIContinent* theContinent = this->GetContinentByID(inID);
    if (inClear)
        outChokePoints.clear();
    if (theContinent) {
        for (i = theContinent->mChokePoints.size() - 1; i >= 0; i--) {
            theChokePoint = this->mChokePoints[theContinent->mChokePoints[i]];
            if (theChokePoint)
                outChokePoints.push_back(theChokePoint);
        }
    } else if (inID == 0) {
        for (i = this->mChokePoints.size() - 1; i >= 0; i--) {
            if (this->mChokePoints[i])
                outChokePoints.push_back(this->mChokePoints[i]);
        }
    }
}

// FUNCTION: EXE 0x422320
// Returns the closest point of the nearest land continent adjacent to inTile's non-land continent; inTile itself
// on land, off the map or without such a neighbour
EETileLocation EEAIContinentManager::GetNearestLandTile(const EETileLocation& inTile)
{
    std::vector<unsigned short> theIDs;
    U2DPoint<long> theClosest;
    EEAIContinent* theContinent;
    if ((unsigned long)inTile.mX < this->mWidth && (unsigned long)inTile.mY < this->mHeight)
        theContinent = this->GetContinentAt(inTile.mX, inTile.mY);
    else
        theContinent = NULL;
    long theY = inTile.mY;
    long theDistance = 0;
    long theX = inTile.mX;
    if (theContinent && theContinent->mCategory != kContinentCategoryLand) {
        this->GetAdjacentIDs(theIDs, theContinent->mID);
        for (long i = theIDs.size() - 1; i >= 0; i--) {
            EEAIContinent* theOther = gContinentManager.GetContinentByID(theIDs[i]);
            if (!theOther || theOther->mCategory != kContinentCategoryLand)
                continue;
            theClosest = theOther->GetClosestPoint(inTile);
            if (theDistance != 0 && !(theClosest.DistanceSquared(inTile) < theDistance))
                continue;
            theX = theClosest.mX;
            theY = theClosest.mY;
            theDistance = theClosest.DistanceSquared(inTile);
        }
    }
    return EETileLocation(theX, theY);
}

// FUNCTION: EXE 0x422443
// Lists, sorted and once each, the continents adjacent to inFirst or inSecond (themselves included) that inScreen
// accepts
void EEAIContinentManager::GetAdjacentContinents(unsigned short inFirst, unsigned short inSecond,
    std::vector<unsigned short>& outIDs, EEAIFOContinentScreenContinentsShareAdjacentID* inScreen)
{
    std::vector<unsigned short> theFirstIDs;
    std::vector<unsigned short> theSecondIDs;
    outIDs.clear();
    this->GetConnectedIDs(theFirstIDs, inFirst);
    this->GetConnectedIDs(theSecondIDs, inSecond);
    long i;
    for (i = theFirstIDs.size() - 1; i >= 0; i--) {
        if (inScreen->Test(theFirstIDs[i]))
            outIDs.push_back(theFirstIDs[i]);
    }
    for (i = theSecondIDs.size() - 1; i >= 0; i--) {
        if (inScreen->Test(theSecondIDs[i]))
            outIDs.push_back(theSecondIDs[i]);
    }
    std::sort(outIDs.begin(), outIDs.end());
    std::vector<unsigned short>::iterator theEnd = std::unique(outIDs.begin(), outIDs.end());
    if (theEnd != outIDs.end())
        outIDs.erase(theEnd, outIDs.end());
}

// FUNCTION: EXE 0x42260b
// Returns in outPoint the center of continent inID's hull, (0, 0) for an unknown id
void EEAIContinentManager::GetContinentCenter(unsigned short inID, U2DPoint<long>& outPoint)
{
    const std::vector<EEAIContinent*>& theContinents = this->mContinents;
    EEAIContinent* theContinent = theContinents[inID];
    outPoint.mX = 0;
    outPoint.mY = 0;
    if (theContinent)
        outPoint = theContinent->mHull.GetCenter();
}

// FUNCTION: EXE 0x422635
// Lists the tiles of continent inID in outTiles (untouched for an unknown id)
void EEAIContinentManager::GetContinentTiles(unsigned short inID, std::vector<U2DPoint<long> >& outTiles) const
{
    EEAIContinent* theContinent = this->mContinents[inID];
    if (theContinent)
        theContinent->mTiles.BuildPointList(outTiles, 1);
}

// FUNCTION: EXE 0x422658
// Returns the continent at inUnit's tile, NULL off the grid
EEAIContinent* EEAIContinentManager::GetContinentAt(EEUnit* inUnit)
{
    if (inUnit->mTileX < this->mWidth && inUnit->mTileY < this->mHeight) {
        return this->GetContinentAt(inUnit->mTileX, inUnit->mTileY);
    }
    return NULL;
}

// FUNCTION: EXE 0x422680
// Returns the id of the continent containing (inX, inY), searched through its path sector, 0 if none
unsigned short EEAIContinentManager::FindContinentID(long inX, long inY) const
{
    EEAIContinent* theContinent = this->FindContinent(inX, inY);
    unsigned short theID;
    theID = theContinent ? theContinent->mID : 0;
    return theID;
}

// FUNCTION: EXE 0x42269f
// Finds the continent containing (inX, inY) among its path sector's neighbouring continents, NULL if none
EEAIContinent* EEAIContinentManager::FindContinent(long inX, long inY) const
{
    U2DPoint<long> thePoint(inX, inY);
    EEAIContinent* theResult = NULL;
    EETileCoordinate theTile;
    theTile.mX = inX;
    theTile.mY = inY;
    EEAIPathSector* theSector = this->GetPathSector(theTile);
    if (theSector) {
        for (long i = theSector->mNeighbors.size() - 1; i >= 0; i--) {
            if (theSector->mNeighbors[i] && reinterpret_cast<EEAIContinent*>(theSector->mNeighbors[i])->Contains(thePoint)) {
                theResult = reinterpret_cast<EEAIContinent*>(theSector->mNeighbors[i]);
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x422712
// Returns the continent id at inUnit's tile, 0 off the grid
unsigned short EEAIContinentManager::GetContinentID(EEUnit* inUnit)
{
    unsigned short theID;
    if (inUnit->mTileX < this->mWidth && inUnit->mTileY < this->mHeight)
        theID = this->GetContinentID(inUnit->mTileX, inUnit->mTileY);
    else
        theID = 0;
    return theID;
}

// FUNCTION: EXE 0x42273a
// Returns the forest at inUnit's tile, NULL if none
EEAIForest* EEAIContinentManager::GetForestAt(EEUnit* inUnit)
{
    EETileLocation theTile(inUnit->mTileX, inUnit->mTileY);
    return this->GetForestAt(theTile);
}

// FUNCTION: EXE 0x42275b
// Returns the forest of inTile's continent that contains inTile, NULL if none
EEAIForest* EEAIContinentManager::GetForestAt(const EETileLocation& inTile)
{
    EEAIContinent* theContinent = this->GetContinentAt(inTile.GetX(), inTile.GetY());
    EEAIForest* theForest = NULL;
    if (theContinent) {
        const std::vector<unsigned short>& theIDs = theContinent->mForestIDs;
        const std::vector<EEAIForest*>& theForests = this->mForests;
        for (long i = theIDs.size() - 1; i >= 0; i--) {
            theForest = theForests[theIDs[i]];
            if (theForest && theForest->Contains(inTile))
                break;
            theForest = NULL;
        }
    }
    return theForest;
}

// FUNCTION: EXE 0x4227d0
// Returns the hill of inTile's continent that contains inTile, NULL if none
EEAIHill* EEAIContinentManager::GetHillAt(const EETileLocation& inTile)
{
    EEAIContinent* theContinent = this->GetContinentAt(inTile.GetX(), inTile.GetY());
    EEAIHill* theHill = NULL;
    if (theContinent) {
        const std::vector<unsigned short>& theIDs = theContinent->mHillIDs;
        const std::vector<EEAIHill*>& theHills = this->mHills;
        for (long i = theIDs.size() - 1; i >= 0; i--) {
            theHill = theHills[theIDs[i]];
            if (theHill && theHill->Contains(inTile))
                break;
            theHill = NULL;
        }
    }
    return theHill;
}

// FUNCTION: EXE 0x422848
// Returns whether two continents are connected for inUnit's passable terrain (AreConnected; flags 0 without a unit)
bool EEAIContinentManager::AreConnectedForUnit(EEUnit* inUnit, EEAIContinent* inFirst, EEAIContinent* inSecond)
{
    long theFlags = 0;
    if (inUnit)
        theFlags = inUnit->GetUnitClass()->GetPassableFlags(inUnit);
    return this->AreConnected(inFirst, inSecond, theFlags);
}

// FUNCTION: EXE 0x422872
// Returns whether two continents are the same or share a non-zero chain id of the chain kind the passable-terrain
// flags inFlags select (false for flags 0)
bool EEAIContinentManager::AreConnected(EEAIContinent* inFirst, EEAIContinent* inSecond, long inFlags)
{
    bool theResult = inFirst == inSecond && inFirst;
    if (!theResult) {
        if (inFlags != 0 && inFirst && inSecond) {
            if (inFlags & 1) {
                if (inFlags & 2)
                    theResult = inFirst->mCounts[kAIChainLandWaterCliff] != 0 && inFirst->mCounts[kAIChainLandWaterCliff] == inSecond->mCounts[kAIChainLandWaterCliff];
                else if (inFlags & 8)
                    theResult = inFirst->mCounts[kAIChainLandCliffSpace] != 0 && inFirst->mCounts[kAIChainLandCliffSpace] == inSecond->mCounts[kAIChainLandCliffSpace];   // cliff + space (was the water + space chain)
                else
                    theResult = inFirst->mCounts[kAIChainLandCliff] != 0 && inFirst->mCounts[kAIChainLandCliff] == inSecond->mCounts[kAIChainLandCliff];
            } else if (inFlags & 2) {
                if (inFlags & 8)
                    theResult = inFirst->mCounts[kAIChainLandWaterSpace] != 0 && inFirst->mCounts[kAIChainLandWaterSpace] == inSecond->mCounts[kAIChainLandWaterSpace];   // water + space (was the cliff + space chain)
                else
                    theResult = inFirst->mCounts[kAIChainLandWater] != 0 && inFirst->mCounts[kAIChainLandWater] == inSecond->mCounts[kAIChainLandWater];
            } else if (inFlags & 8)
                theResult = inFirst->mCounts[kAIChainLandSpace] != 0 && inFirst->mCounts[kAIChainLandSpace] == inSecond->mCounts[kAIChainLandSpace];
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x422952
// Returns the id of the continent of category inKind (any for none) with the most tiles, 0 if there is none
long EEAIContinentManager::GetLargestContinent(long inKind)
{
    unsigned short theResult = 0;
    long theBest = -1;
    for (long i = this->mContinents.size() - 1; i >= 0; i--) {
        if (this->mContinents[i] == NULL)
            continue;
        if (theBest == -1) {
            if ((char)inKind == kContinentCategoryNone || this->mContinents[i]->mCategory == (char)inKind)
                theBest = i;
        } else {
            if ((char)inKind != kContinentCategoryNone && (*(this->mContinents.begin() + i))->mCategory != (char)inKind)
                continue;
            if (this->mContinents[theBest]->mTiles.Size() < (*(this->mContinents.begin() + i))->mTiles.Size())
                theBest = i;
        }
    }
    if (theBest != -1)
        theResult = this->mContinents[theBest]->mID;
    return theResult;
}

// FUNCTION: EXE 0x422a2f
// Returns a free sub-sector id (last freed first) or the next new one
unsigned short EEAIContinentManager::NewSubSectorID()
{
    unsigned short theID;
    if (!this->mFreeSubSectorIDs.empty()) {
        theID = this->mFreeSubSectorIDs.back();
        this->mFreeSubSectorIDs.pop_back();
    } else {
        theID = ++this->mLastSubSectorID;
    }
    return theID;
}

// FUNCTION: EXE 0x422a70
// Returns a free path area id (last freed first) or the next new one
unsigned short EEAIContinentManager::NewPathAreaID()
{
    unsigned short theID;
    if (!this->mFreePathAreaIDs.empty()) {
        theID = this->mFreePathAreaIDs.back();
        this->mFreePathAreaIDs.pop_back();
    } else {
        theID = this->mNextPathAreaID++;
    }
    return theID;
}

// FUNCTION: EXE 0x422ab8
// Returns the path area at inUnit's tile
EEAIPathArea* EEAIContinentManager::GetPathAreaAt(EEUnit* inUnit)
{
    EETileLocation theTile(inUnit->mTileX, inUnit->mTileY);
    return this->GetPathAreaAt(theTile);
}

// FUNCTION: EXE 0x422ad9
// Returns the path area at inTile: its sub-sector's area when that is current, else found through the path sector
// (through the continent while repairing); NULL off the map
EEAIPathArea* EEAIContinentManager::GetPathAreaAt(const EETileLocation& inTile)
{
    EEAIPathArea* theResult = NULL;
    if (gWorld.mMap && gWorld.mMap->GetTileAtLocation(inTile)) {
        EEAIPathSubSector* theSubSector = this->GetPathSubSectorAt(inTile.GetX(), inTile.GetY());
        if (theSubSector && !theSubSector->mIsStale) {
            theResult = this->GetPathArea(theSubSector->mPathAreaID);
            if (theResult)
                return theResult;
        }
        if (this->mState != 2) {
            EEAIPathSector* theSector = this->GetPathSector(inTile);
            if (theSector)
                theResult = theSector->GetPathAreaAt(inTile);
        } else {
            EEAIContinent* theContinent = this->GetContinentAt(inTile.GetX(), inTile.GetY());
            theResult = theContinent->GetPathAreaAt(inTile);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x422b62
// Returns the inKind region id (water body, land mass, space or surface region) at inUnit's tile
unsigned short EEAIContinentManager::GetContinent(EEUnit* inUnit, long inKind)
{
    EETileCoordinate theTile;
    theTile.mX = inUnit->mTileX;
    theTile.mY = inUnit->mTileY;
    return this->GetContinent(theTile, inKind);
}

// FUNCTION: EXE 0x422b86
// Returns the inKind region id (water body, land mass, space or surface region) of inTile's path area, 0 if none
unsigned short EEAIContinentManager::GetContinent(const EETileCoordinate& inTile, long inKind)
{
    unsigned short theResult = 0;
    EEAIPathArea* theArea = this->GetPathAreaAt(inTile);
    if (theArea) {
        if (inKind == kAIPathWaterBody)
            theResult = theArea->mWaterBodyID;
        else if (inKind == kAIPathLandMass)
            theResult = theArea->mLandMassID;
        else if (inKind == kAIPathSpaceRegion)
            theResult = theArea->mSpaceRegionID;
        else if (inKind == kAIPathNonSpace)
            theResult = theArea->mSurfaceRegionID;
    }
    return theResult;
}

// FUNCTION: EXE 0x422bd7
// Returns the path sector at inUnit's tile
EEAIPathSector* EEAIContinentManager::GetPathSector(EEUnit* inUnit)
{
    EETileCoordinate theTile;
    theTile.mX = inUnit->mTileX;
    theTile.mY = inUnit->mTileY;
    return this->GetPathSector(theTile);
}

// FUNCTION: EXE 0x422bf8
// Returns the path sector containing inTile, NULL off the grid
EEAIPathSector* EEAIContinentManager::GetPathSector(const EETileCoordinate& inTile) const
{
    EEAIPathSector* theResult = NULL;
    U2DPoint<long> theSector(inTile.mX / this->mRegionInformation.mSectorSize, inTile.mY / this->mRegionInformation.mSectorSize);
    if ((unsigned long)inTile.mX < this->mWidth && (unsigned long)inTile.mY < this->mHeight)
        theResult = this->mPathSectors[theSector.mX * this->mRegionInformation.mSectorsX + theSector.mY + 1];
    return theResult;
}

// FUNCTION: EXE 0x422c51
// Returns the path sub-sector at inUnit's tile
EEAIPathSubSector* EEAIContinentManager::GetPathSubSectorAt(EEUnit* inUnit)
{
    return this->GetPathSubSectorAt(inUnit->GetTileX(), inUnit->GetTileY());
}

// FUNCTION: EXE 0x422c65
// Finds the sub-sector containing (inX, inY) among its path sector's sub-sectors, NULL if none
EEAIPathSubSector* EEAIContinentManager::FindPathSubSector(long inX, long inY)
{
    U2DPoint<long> thePoint(inX, inY);
    std::map<unsigned short, EEAIPathSubSector*>::iterator theIt;
    EETileCoordinate theTile;
    theTile.mX = inX;
    theTile.mY = inY;
    EEAIPathSubSector* theResult = NULL;
    EEAIPathSector* theSector = this->GetPathSector(theTile);
    if (theSector) {
        for (theIt = theSector->mSubSectors.begin(); theIt != theSector->mSubSectors.end(); ++theIt) {
            EEAIPathSubSector* theSubSector = theIt->second;
            if (theSubSector->mTiles.Contains(thePoint)) {
                theResult = theIt->second;
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x422cd7
// Returns the path sub-sector id at inUnit's tile, 0 off the grid
unsigned short EEAIContinentManager::GetPathSubSectorID(EEUnit* inUnit)
{
    unsigned short theID;
    if (inUnit->mTileX < this->mWidth && inUnit->mTileY < this->mHeight)
        theID = this->GetPathSubSectorID(inUnit->mTileX, inUnit->mTileY);
    else
        theID = 0;
    return theID;
}

// FUNCTION: EXE 0x422cff
// Returns the id of the sub-sector containing (inX, inY), found by search, 0 if none
unsigned short EEAIContinentManager::FindPathSubSectorID(long inX, long inY)
{
    EEAIPathSubSector* theSubSector = this->FindPathSubSector(inX, inY);
    unsigned short theID;
    theID = theSubSector ? theSubSector->mID : 0;
    return theID;
}

// FUNCTION: EXE 0x422d1b
// Returns a random area of type inKind whose hull center is on continent inContinent (any for 0), NULL if none
EEAIArea* EEAIContinentManager::GetRandomArea(long inKind, URandom& inRandom, unsigned short inContinent)
{
    std::map<unsigned long, EEAIArea*>::iterator theIt;
    EEAIArea* theResult = NULL;
    std::vector<EEAIArea*> theAreas;
    for (theIt = this->mAreas.begin(); theIt != this->mAreas.end(); ++theIt) {
        EEAIArea* theArea = theIt->second;
        if (theArea && theArea->mAreaType == inKind &&
            (inContinent == 0 ||
             gContinentManager.GetContinentID(theArea->mHull.GetCenter().GetX(), theArea->mHull.GetCenter().GetY()) == inContinent))
            theAreas.push_back(theIt->second);
    }
    if (theAreas.size() != 0)
        theResult = theAreas[EEGetRandomInt(0, theAreas.size() - 1, inRandom)];
    return theResult;
}

// FUNCTION: EXE 0x422dc8
// Returns a random continent of category inKind (any but the map edge for none), NULL if none
EEAIArea* EEAIContinentManager::GetRandomContinent(unsigned char inKind, URandom& inRandom)
{
    std::vector<EEAIArea*> theAreas;
    EEAIArea* theResult = NULL;
    for (long i = this->mContinents.size() - 1; i >= 0; i--) {
        if (this->mContinents[i] &&
            (inKind == kContinentCategoryNone && this->mContinents[i]->GetCategory() != kContinentCategoryMapEdge ||
             this->mContinents[i]->GetCategory() == inKind)) {
            EEAIArea* theArea = this->mContinents[i];
            theAreas.push_back(theArea);
        }
    }
    if (theAreas.size() != 0)
        theResult = theAreas[EEGetRandomInt(0, theAreas.size() - 1, inRandom)];
    return theResult;
}

// FUNCTION: EXE 0x422eb4
// Returns the rectangle area containing inTile, NULL if none
EEAIRectangleArea* EEAIContinentManager::GetRectangleAreaAt(const EETileLocation& inTile)
{
    EEAIRectangleArea* theResult = NULL;
    const std::vector<EEAIRectangleArea*>& theAreas = this->mRectangleAreas;
    for (long i = theAreas.size() - 1; i >= 0; i--) {
        if (theAreas[i] && theAreas[i]->Contains(inTile)) {
            theResult = theAreas[i];
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x422f15
// Returns the continent id under inUnit's tile, 0 without a unit or off the map
unsigned short EEAIContinentManager::GetUnitContinentID(EEUnit* inUnit)
{
    unsigned short theResult = 0;
    if (inUnit) {
        unsigned long theX = inUnit->mTileX;
        unsigned long theY = inUnit->mTileY;
        if (gWorld.mMap->GetTile(theX, theY))
            theResult = this->GetContinentID(theX, theY);
    }
    return theResult;
}

// FUNCTION: EXE 0x422f5f
// Returns whether tile (inX, inY) is on continent inID or on an adjacent continent with a neighbour tile on inID;
// outTiles gets those neighbour tiles
bool EEAIContinentManager::GetTilesNextTo(long inX, long inY, std::vector<U2DPoint<long> >& outTiles, unsigned short inID)
{
    std::vector<U2DPoint<long> > theTiles;
    unsigned short theID = this->GetContinentID(inX, inY);
    bool theResult = false;
    outTiles.clear();
    if (theID != inID) {
        if (this->mTileFlags[this->mFlagsWidth * theID + inID]) {
            Fn2492C2(inX, inY, 1, theTiles);
            for (long i = theTiles.size() - 1; i >= 0; i--) {
                if (!(theTiles[i].GetX() >= 0) || !(theTiles[i].GetY() >= 0))
                    continue;
                unsigned long theY = theTiles[i].GetY();
                if ((unsigned long)theTiles[i].GetX() < this->mWidth && theY < this->mHeight &&
                    this->GetContinentID(theTiles[i].GetX(), theTiles[i].GetY()) == inID)
                    outTiles.push_back(theTiles[i]);
            }
            theResult = outTiles.size() != 0;
            // LOST-DEBUG: none: exact only (tools/inline_sim.py: two more inline candidates after the GetContinentID
            // arguments refuse the first five theTiles[i] begin() expansions and keep the sixth, as the original)
            EE_LOST_DEBUG(2);
        }
    } else {
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x42311c
// Returns whether tile (inX, inY) is on or next to continent inID
bool EEAIContinentManager::IsNextToArea(long inX, long inY, unsigned short inID)
{
    std::vector<U2DPoint<long> > theTiles;
    return this->GetTilesNextTo(inX, inY, theTiles, inID);
}

// FUNCTION: EXE 0x42315c
// Returns whether every continent adjacent to inID is of category inKind
bool EEAIContinentManager::AllNeighborsOfKind(unsigned short inID, unsigned char inKind)
{
    std::vector<unsigned short> theIDs;
    bool theResult = true;
    this->GetAdjacentIDs(theIDs, inID);
    const std::vector<EEAIContinent*>& theContinents = this->mContinents;
    for (long i = theIDs.size() - 1; i >= 0; i--) {
        EEAIContinent* theContinent = theContinents[theIDs[i]];
        if (theContinent && theContinent->mCategory != inKind) {
            theResult = false;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4231d7
// Computes the AI tile values (grid, and the map tile clamped to +-120); inWater: water and space tiles get 10 per
// shore or ice-shore neighbour (negated, minus 10, on a shore tile); else land tiles get 10 per wood unit in 3x3
void EEAIContinentManager::UpdateTileValues(bool inWater)
{
    EEMap* theMap = gWorld.mMap;
    for (long x = this->mWidth - 1; x >= 0; x--) {
        for (long y = this->mHeight - 1; y >= 0; y--) {
            EETile* theTile = theMap->GetTile(x, y);
            if (!theTile)
                continue;
            long theType = theTile->mTerrainType;
            long theValue;
            if (inWater) {
                if (theType != kTerrainDeepWater && theType != kTerrainShallowWater && theType != kTerrainSpace)
                    continue;
                theValue = (this->IsShoreTile(x, y - 1) ? 10 : 0) + (this->IsShoreTile(x, y + 1) ? 10 : 0) +
                    (this->IsShoreTile(x + 1, y - 1) ? 10 : 0) + (this->IsShoreTile(x + 1, y) ? 10 : 0) +
                    (this->IsShoreTile(x + 1, y + 1) ? 10 : 0) + (this->IsShoreTile(x - 1, y - 1) ? 10 : 0) +
                    (this->IsShoreTile(x - 1, y) ? 10 : 0) + (this->IsShoreTile(x - 1, y + 1) ? 10 : 0);
                if (this->mIceShoreTiles.size() != 0) {
                    theValue += (std::binary_search(this->mIceShoreTiles.begin(), this->mIceShoreTiles.end(), U2DPoint<long>(x, y - 1)) ? 10 : 0) +
                        (std::binary_search(this->mIceShoreTiles.begin(), this->mIceShoreTiles.end(), U2DPoint<long>(x, y + 1)) ? 10 : 0) +
                        (std::binary_search(this->mIceShoreTiles.begin(), this->mIceShoreTiles.end(), U2DPoint<long>(x + 1, y - 1)) ? 10 : 0) +
                        (std::binary_search(this->mIceShoreTiles.begin(), this->mIceShoreTiles.end(), U2DPoint<long>(x + 1, y)) ? 10 : 0) +
                        (std::binary_search(this->mIceShoreTiles.begin(), this->mIceShoreTiles.end(), U2DPoint<long>(x + 1, y + 1)) ? 10 : 0) +
                        (std::binary_search(this->mIceShoreTiles.begin(), this->mIceShoreTiles.end(), U2DPoint<long>(x - 1, y - 1)) ? 10 : 0) +
                        (std::binary_search(this->mIceShoreTiles.begin(), this->mIceShoreTiles.end(), U2DPoint<long>(x - 1, y)) ? 10 : 0) +
                        (std::binary_search(this->mIceShoreTiles.begin(), this->mIceShoreTiles.end(), U2DPoint<long>(x - 1, y + 1)) ? 10 : 0);
                }
                if (this->IsShoreTile(x, y))
                    theValue = -10 - theValue;
            } else {
                if (theType == kTerrainDeepWater || theType == kTerrainShallowWater || theType == kTerrainSpace)
                    continue;
                theValue = this->GetResourceWeight(x, y) + this->GetResourceWeight(x, y + 1) + this->GetResourceWeight(x, y - 1) + this->GetResourceWeight(x + 1, y - 1) +
                    this->GetResourceWeight(x + 1, y) + this->GetResourceWeight(x + 1, y + 1) + this->GetResourceWeight(x - 1, y - 1) + this->GetResourceWeight(x - 1, y) +
                    this->GetResourceWeight(x - 1, y + 1);
            }
            if (theValue == 0)
                continue;
            this->mTileValues[this->mWidth * x + y] = theValue;
            if (theValue <= -0x78)
                theValue = -0x78;
            if (theValue >= 0x78)
                theValue = 0x78;
            theTile->mAIValue = (char)theValue;
        }
    }
    this->mIceShoreTiles.clear();
}

// FUNCTION: EXE 0x4236b5
// Marks the manager as clearing, resets it, frees its grids and rebuilds the continents and hills of inMap
void EEAIContinentManager::Clear(EEMap* inMap)
{
    this->mClearing = true;
    this->Reset();
    delete[] this->mTileFlags;
    delete[] this->mContinentIDs;
    delete[] this->mTileValues;
    delete[] this->mPathSubSectorIDs;
    this->mTileFlags = NULL;
    this->mContinentIDs = NULL;
    this->mTileValues = NULL;
    this->mPathSubSectorIDs = NULL;
    this->CreateContinents(inMap);
    this->CreateHills(inMap);
}

// The path-area entries of RepairPathArea: a neighbour tile and its path area. Ordered and made unique by the area's id
// (std::sort 0x26bcc / std::unique 0x26c87 with these predicates: +0x7a word compares) (provisional names)
struct EEAIPathAreaEntryLess {
    bool operator()(const std::pair<U2DPoint<long>, EEAIPathArea*>& inFirst,
        const std::pair<U2DPoint<long>, EEAIPathArea*>& inSecond) const
    {
        return inFirst.second->mID < inSecond.second->mID;
    }
};

struct EEAIPathAreaEntryEqual {
    bool operator()(const std::pair<U2DPoint<long>, EEAIPathArea*>& inFirst,
        const std::pair<U2DPoint<long>, EEAIPathArea*>& inSecond) const
    {
        return inFirst.second->mID == inSecond.second->mID;
    }
};

// FUNCTION: EXE 0x42372b
// Re-files tile inTile after its AI kind changed: it joins the largest same-kind neighbouring area (merging the
// others into it), its single neighbour's area or a new path area; sectors and sub-sectors follow the new id
void EEAIContinentManager::RepairPathArea(const U2DPoint<long>& inTile)
{
    std::vector<std::pair<U2DPoint<long>, EEAIPathArea*> > theNeighbours;
    std::vector<std::pair<U2DPoint<long>, EEAIPathArea*> > theMatches;
    std::vector<EEAIPathArea*> theUnused;
    std::vector<EEAIPathArea*> theMerged;
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theOtherTiles;
    std::vector<unsigned short> theSectorIDs;
    U2DPoint<long> theNeighbour;
    U2DPoint<long> theStep;
    long i;
    long j;
    long theDirection;
    EEAIPathArea* theNeighbourArea;
    EEAIPathSector* theSector;
    EEAIPathSubSector* theSubSector = this->GetPathSubSectorAt(inTile.GetX(), inTile.GetY());
    EEAIPathArea* theBest = NULL;
    EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(inTile);
    EETile* theTile = gWorld.GetMap()->GetTile(inTile.GetX(), inTile.GetY());
    EEAIPathArea* theSingle = NULL;
    long theKind = EEAIGetTileKind(inTile);
    unsigned short theContinent = gContinentManager.GetContinentID(inTile.GetX(), inTile.GetY());
    unsigned short theNewID = 0;
    bool theOK = true;
    if (theArea && theArea->mKind != theKind) {
        if (theSubSector)
            theSubSector->mIsStale = true;
        if (theTile) {
            theNeighbours.reserve(8);
            theNeighbour.mX = inTile.GetX() - 1;
            theNeighbour.mY = inTile.GetY() - 1;
            theNeighbourArea = this->GetPathAreaAt(theNeighbour);
            if (theNeighbourArea)
                theNeighbours.push_back(std::pair<U2DPoint<long>, EEAIPathArea*>(theNeighbour, theNeighbourArea));
            theNeighbour.mX = inTile.GetX() - 1;
            theNeighbour.mY = inTile.GetY();
            theNeighbourArea = this->GetPathAreaAt(theNeighbour);
            if (theNeighbourArea)
                theNeighbours.push_back(std::pair<U2DPoint<long>, EEAIPathArea*>(theNeighbour, theNeighbourArea));
            theNeighbour.mX = inTile.GetX() - 1;
            theNeighbour.mY = inTile.GetY() + 1;
            theNeighbourArea = this->GetPathAreaAt(theNeighbour);
            if (theNeighbourArea)
                theNeighbours.push_back(std::pair<U2DPoint<long>, EEAIPathArea*>(theNeighbour, theNeighbourArea));
            theNeighbour.mX = inTile.GetX();
            theNeighbour.mY = inTile.GetY() - 1;
            theNeighbourArea = this->GetPathAreaAt(theNeighbour);
            if (theNeighbourArea)
                theNeighbours.push_back(std::pair<U2DPoint<long>, EEAIPathArea*>(theNeighbour, theNeighbourArea));
            theNeighbour.mX = inTile.GetX();
            theNeighbour.mY = inTile.GetY() + 1;
            theNeighbourArea = this->GetPathAreaAt(theNeighbour);
            if (theNeighbourArea)
                theNeighbours.push_back(std::pair<U2DPoint<long>, EEAIPathArea*>(theNeighbour, theNeighbourArea));
            theNeighbour.mX = inTile.GetX() + 1;
            theNeighbour.mY = inTile.GetY() - 1;
            theNeighbourArea = this->GetPathAreaAt(theNeighbour);
            if (theNeighbourArea)
                theNeighbours.push_back(std::pair<U2DPoint<long>, EEAIPathArea*>(theNeighbour, theNeighbourArea));
            theNeighbour.mX = inTile.GetX() + 1;
            theNeighbour.mY = inTile.GetY();
            theNeighbourArea = this->GetPathAreaAt(theNeighbour);
            if (theNeighbourArea)
                theNeighbours.push_back(std::pair<U2DPoint<long>, EEAIPathArea*>(theNeighbour, theNeighbourArea));
            theNeighbour.mX = inTile.GetX() + 1;
            theNeighbour.mY = inTile.GetY() + 1;
            theNeighbourArea = this->GetPathAreaAt(theNeighbour);
            if (theNeighbourArea)
                theNeighbours.push_back(std::pair<U2DPoint<long>, EEAIPathArea*>(theNeighbour, theNeighbourArea));
            if (theNeighbours.size() > 1) {
                for (i = theNeighbours.size() - 1; i >= 0; i--) {
                    if (theNeighbours[i].second->mKind != theKind ||
                        theNeighbours[i].second->mContinentID != theArea->mContinentID)
                        continue;
                    if (theKind == kAITileUnitObstacle)
                        theOK = theNeighbours[i].second->mUnitID == EEAIGetTileWallID(inTile);   // the neighbour's wall (was the old area's)
                    if (!theOK)
                        continue;
                    // a diagonal neighbour joins only through two same-kind orthogonal steps
                    if (inTile.GetX() != theNeighbours[i].first.GetX() &&
                        inTile.GetY() != theNeighbours[i].first.mY) {
                        switch (theKind) {
                        case kAITileCliff:
                        case kAITileGaiaObstacle:
                        case kAITileMapEdge:
                        case kAITileUnitObstacle:
                        case kAITileWall:
                        case kAITileGate:
                            theOK = true;
                            break;
                        default:
                            theDirection = EEGetTileDirection(inTile.GetX(), inTile.GetY(), theNeighbours[i].first.GetX(),
                                theNeighbours[i].first.mY);
                            EEGetNextTile(inTile, theStep, EETurnDirection(theDirection, 1, true), 1);
                            theNeighbourArea = this->GetPathAreaAt(theStep);
                            if (theNeighbourArea && theNeighbourArea->mKind == theKind) {
                                EEGetNextTile(inTile, theStep, EETurnDirection(theDirection, 1, false), 1);
                                theNeighbourArea = this->GetPathAreaAt(theStep);
                                theOK = theNeighbourArea && theNeighbourArea->mKind == theKind;
                            } else {
                                theOK = false;
                            }
                            break;
                        }
                    } else {
                        theOK = true;
                    }
                    if (theOK)
                        theMatches.push_back(theNeighbours[i]);
                }
            } else if (theNeighbours.size() == 1) {
                if (theKind == theNeighbours.front().second->mKind &&
                    theContinent == theNeighbours.front().second->mContinentID)
                    theSingle = theNeighbours.front().second;
            }
            std::sort(theMatches.begin(), theMatches.end(), EEAIPathAreaEntryLess());
            theMatches.erase(std::unique(theMatches.begin(), theMatches.end(), EEAIPathAreaEntryEqual()), theMatches.end());
            if (theMatches.size() > 0) {
                for (i = theMatches.size() - 1; i >= 0; i--) {
                    if (theBest == NULL || theMatches[i].second->mTiles.Size() > theBest->mTiles.Size())
                        theBest = theMatches[i].second;
                }
                if (theBest) {
                    theBest->mTiles.BuildPointList(theTiles, 1);
                    theBest->mTiles.Insert(inTile);
                    theTiles.push_back(inTile);
                    for (i = theMatches.size() - 1; i >= 0; i--) {
                        if (theMatches[i].second == theBest)
                            continue;
                        theMerged.push_back(theMatches[i].second);
                        theMatches[i].second->mTiles.BuildPointList(theOtherTiles, 1);
                        theTiles.insert(theTiles.end(), theOtherTiles.begin(), theOtherTiles.end());
                    }
                    theBest->Create(theTiles);
                    theNewID = theBest->mID;
                }
            } else if (theSingle) {
                theSingle->mTiles.BuildPointList(theTiles, 1);
                theTiles.push_back(inTile);
                theSingle->Create(theTiles);
                theNewID = theSingle->mID;
            } else {
                theSingle = new EEAIPathArea;
                theSingle->mContinentID = gContinentManager.GetContinentID(inTile.GetX(), inTile.GetY());
                theSingle->mKind = theKind;
                theSingle->mID = this->NewPathAreaID();
                theSingle->mSerial = ++this->mLastSerial;
                if (theKind == kAITileUnitObstacle)
                    theSingle->mUnitID = EEAIGetTileWallID(inTile);
                unsigned short theID;
                if (theTile->mTerrainType == kTerrainSpace) {
                    theID = this->GetAdjacentAreaID(kAIPathSpaceRegion, inTile, theKind);
                    if (theID == 0)
                        theID = (unsigned short)this->mNextID12C++;
                    theSingle->mSpaceRegionID = theID;
                } else {
                    theID = this->GetAdjacentAreaID(kAIPathNonSpace, inTile, theKind);
                    if (theID == 0)
                        theID = (unsigned short)this->mNextID130++;
                    theSingle->mSurfaceRegionID = theID;
                }
                if (theTile->mTerrainType == kTerrainDeepWater) {
                    theID = this->GetAdjacentAreaID(kAIPathWaterBody, inTile, theKind);
                    if (theID == 0)
                        theID = (unsigned short)this->mNextID128++;
                    theSingle->mWaterBodyID = theID;
                } else if (theKind != kAITileForest) {
                    theID = this->GetAdjacentAreaID(kAIPathLandMass, inTile, theKind);
                    if (theID == 0)
                        theID = (unsigned short)this->mNextID124++;
                    theSingle->mLandMassID = theID;
                    if ((theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater) &&
                        (theKind == kAITileWater || (theKind > kAITileMapEdge && theKind <= kAITileUnitObstacle))) {
                        theID = this->GetAdjacentAreaID(kAIPathWaterBody, inTile, theKind);
                        if (theID == 0)
                            theID = (unsigned short)this->mNextID128++;
                        theSingle->mWaterBodyID = theID;
                    }
                }
                theTiles.push_back(inTile);
                theSingle->Create(theTiles);
                theID = theSingle->mID;
                if (theID >= this->mPathAreas.size())
                    this->mPathAreas.push_back(theSingle);
                else
                    this->mPathAreas[theID] = theSingle;
                theNewID = theSingle->mID;
                this->AddArea(theSingle);
                EEAIContinent* theContinentArea = gContinentManager.GetContinentAt(inTile.GetX(), inTile.GetY());
                if (theContinentArea)
                    theContinentArea->AddPathAreaID(theSingle->mID);
            }
        }
        // the merged areas hand their sectors and sub-sectors to theNewID
        for (i = theMerged.size() - 1; i >= 0; i--) {
            EEAIPathArea* theOld = theMerged[i];
            EEAIContinent* theOldContinent = gContinentManager.GetContinentByID(theOld->mContinentID);
            if (theOldContinent)
                theOldContinent->RemovePathAreaID(theOld->mID);
            const std::vector<unsigned short>& theOldIDs = theOld->mSectorIDs;  // const iterators: the original calls insert 0x26cf3
            theSectorIDs.insert(theSectorIDs.end(), theOldIDs.begin(), theOldIDs.end());
            for (j = theOld->mSectorIDs.size() - 1; j >= 0; j--) {
                theSector = this->GetPathSectorByID(theOld->mSectorIDs[j]);
                if (theSector) {
                    theSector->RemovePathAreaID(theOld->mID);
                    theSector->AddPathAreaID(theNewID);
                }
            }
            for (j = this->mPathSubSectors.size() - 1; j >= 0; j--) {
                if (this->mPathSubSectors[j] && this->mPathSubSectors[j]->mPathAreaID == theOld->mID)
                    this->mPathSubSectors[j]->mPathAreaID = theNewID;
            }
            this->RemoveArea(theOld);
            this->mPathAreas[theOld->mID] = NULL;
            this->mFreePathAreaIDs.push_back(theOld->mID);
            delete theOld;
        }
        // inTile leaves its old area; an emptied area is deleted
        theArea->RemoveTile(inTile);
        if (theArea->mTiles.Size() == 0) {
            EEAIContinent* theAreaContinent = gContinentManager.mContinents[theArea->mContinentID];
            if (theAreaContinent)
                theAreaContinent->RemovePathAreaID(theArea->mID);
            for (j = theArea->mSectorIDs.size() - 1; j >= 0; j--) {
                theSector = this->mPathSectors[theArea->mSectorIDs[j]];
                if (theSector)
                    theSector->RemovePathAreaID(theArea->mID);
            }
            for (j = this->mPathSubSectors.size() - 1; j >= 0; j--) {
                if (this->mPathSubSectors[j] && this->mPathSubSectors[j]->mPathAreaID == theArea->mID)
                    this->mPathSubSectors[j]->mPathAreaID = theNewID;
            }
            this->RemoveArea(theArea);
            this->mPathAreas[theArea->mID] = NULL;
            this->mFreePathAreaIDs.push_back(theArea->mID);
            delete theArea;
        }
        // the new area joins inTile's sector and the merged areas' sectors
        theArea = this->mPathAreas[theNewID];
        if (theArea) {
            theSector = this->GetPathSector(inTile);
            if (theSector) {
                theSector->AddPathAreaID(theNewID);
                theArea->AddSectorID(theSector->mID);
            }
            std::sort(theSectorIDs.begin(), theSectorIDs.end());
            std::vector<unsigned short>::iterator theEnd = std::unique(theSectorIDs.begin(), theSectorIDs.end());
            if (theEnd != theSectorIDs.end())
                theSectorIDs.erase(theEnd, theSectorIDs.end());
            for (j = theSectorIDs.size() - 1; j >= 0; j--) {
                theSector = this->mPathSectors[theSectorIDs[j]];
                if (theSector) {
                    theSector->AddPathAreaID(theNewID);
                    theArea->AddSectorID(theSector->mID);
                }
            }
        }
    }
}

// FUNCTION: EXE 0x4245a5
// Rebuilds the sub-sectors of one pending sector, re-links them, repairs the path areas of its changed tiles and
// refiles the new sub-sectors; returns true when no sector is left pending
bool EEAIContinentManager::Rebuild()
{
    long n;
    EEAIPathSector* theSector;
    long x;
    long y;
    EETileCoordinate theTile;
    EEAIPathSubSector* theSubSector;
    long i;
    EEAIPathArea* theArea;
    EEAIPathSectorMachine theMachine(0xf, gWorld.mMap);
    std::map<unsigned short, std::vector<EETileCoordinate> > theTiles;
    std::vector<EEAIPathSubSector*> theSubSectors;
    U2DRectangle<long> theBounds;
    EEMap* theMap = gWorld.mMap;
    theMachine.mCheckCorners = true;
    theMachine.mGoal.mClassifyTiles = true;
    std::map<unsigned short, std::vector<EETileCoordinate> >::iterator theIt = this->mSectorTiles.begin();
    for (n = 0; theIt != this->mSectorTiles.end() && n < 1; ++theIt, n++)
        theTiles.insert(*theIt);
    if (theTiles.begin() == theTiles.end()) {
        theBounds.mTop = 0;
        theBounds.mLeft = 0;
        theBounds.mBottom = theMap->mSize;
        theBounds.mRight = theMap->mSize;
    } else {
        for (theIt = theTiles.begin(); theIt != theTiles.end(); ++theIt) {
            theSector = this->mPathSectors[theIt->first];
            this->RemoveSubSectors(theSector);
            this->RemoveSectorTiles(theSector);
            theBounds.Extend(theSector->mBounds.GetTopLeft());
            theBounds.Extend(theSector->mBounds.GetBottomRight());
        }
    }
    theMachine.mGoal.mTileBounds = theBounds;
    EEAIPathSectorMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(theMap->mSize, theMap->mSize);
    for (theIt = theTiles.begin(); theIt != theTiles.end(); ++theIt) {
        theSector = this->mPathSectors[theIt->first];
        for (x = theSector->mBounds.mRight; x >= theSector->mBounds.mLeft; x--) {
            for (y = theSector->mBounds.mBottom; y >= theSector->mBounds.mTop; y--) {
                if (theMachine.mSubSectorIDs[theMachine.mHeight * x + y] != 0 || !theMap->GetTile(x, y))
                    continue;
                theTile.mX = x;
                theTile.mY = y;
                theSector = this->GetPathSector(theTile);
                theMachine.mGoal.mSector = theSector;
                theMachine.mGoal.mSectorBounds = theSector->mBounds;
                theMachine.mGoal.mStartTile.mX = x;
                theMachine.mGoal.mStartTile.mY = y;
                theMachine.mGoal.mGoalTile.mX = theMap->mSize;
                theMachine.mGoal.mGoalTile.mY = theMap->mSize;
                theMachine.Run();
                theMachine.mGoal.CollectTiles();
                theMachine.mGoal.CreateSubSector();
            }
        }
    }
    if (this->mPathSubSectors.size() <= this->mLastSubSectorID)
        this->mPathSubSectors.insert(this->mPathSubSectors.end(), this->mLastSubSectorID - this->mPathSubSectors.size() + 1, NULL);
    for (std::map<unsigned short, EEAIPathSubSector*>::const_iterator theNew = theMachine.mGoal.mSubSectors.begin();
         theNew != theMachine.mGoal.mSubSectors.end(); ++theNew) {
        theSubSector = theNew->second;
        theSubSector->UpdateLocation();
        this->mPathSubSectors[theNew->first] = theSubSector;
        theSubSector->mIsStale = true;
        theSubSectors.push_back(theSubSector);
    }
    this->RelinkSubSectors(theTiles, theSubSectors);
    this->mState = 2;
    for (i = theMachine.mGoal.mStaleTiles.size() - 1; i >= 0; i--)
        this->RepairPathArea(theMachine.mGoal.mStaleTiles[i]);
    for (i = theSubSectors.size() - 1; i >= 0; i--) {
        (*(theSubSectors.begin() + i))->mIsStale = true;
        theArea = this->GetPathAreaAt((*(theSubSectors.begin() + i))->mLocation);
        if (theArea) {
            theTile.mX = theSubSectors[i]->mLocation.GetX();
            theTile.mY = (*(theSubSectors.begin() + i))->mLocation.mY;
            theSector = this->GetPathSector(theTile);
            if (theSector) {
                theSector->AddPathAreaID(theArea->mID);
                theArea->AddSectorID(theSector->mID);
            }
            theSubSectors[i]->mPathAreaID = theArea->mID;
        }
        theSubSectors[i]->mIsStale = false;
    }
    this->mState = 0;
    this->ClearChecksum();
    return this->mSectorTiles.size() == 0;
}

// FUNCTION: EXE 0x424a24
// Builds the AI regions of inMap after Clear: tile values, path areas and sectors, choke points, the region ids,
// scenario areas and the six chain kinds; frees the work grids (continent ids kept in large games)
void EEAIContinentManager::BuildRegions(EEMap* inMap)
{
    this->AllocateTileValues();
    this->UpdateTileValues(true);
    this->CreatePathAreas(inMap);
    this->Build(inMap);
    this->UpdateTileValues(false);
    this->CreatePathAreaKind(inMap, kAIPathWaterBody);
    this->CreatePathAreaKind(inMap, kAIPathLandMass);
    this->CreatePathAreaKind(inMap, kAIPathSpaceRegion);
    this->CreatePathAreaKind(inMap, kAIPathNonSpace);
    this->CreateScenarioAreas(inMap);
    this->NumberContinentChains(inMap, kAIChainLandCliff);
    this->NumberContinentChains(inMap, kAIChainLandWater);
    this->NumberContinentChains(inMap, kAIChainLandWaterCliff);
    this->NumberContinentChains(inMap, kAIChainLandSpace);
    this->NumberContinentChains(inMap, kAIChainLandWaterSpace);
    this->NumberContinentChains(inMap, kAIChainLandCliffSpace);
    delete this->mTileValues;
    this->mTileValues = NULL;
    if (inMap->mL195610 < 0x4b && gWorld.mNumPlayers < 5 && gWorld.mPopulationLimit < 1000) {
        delete[] this->mContinentIDs;
        this->mContinentIDs = NULL;
    }
    delete[] this->mPathSubSectorIDs;
    this->mPathSubSectorIDs = NULL;
    this->ClearChecksum();
    this->mClearing = false;
}

// FUNCTION: EXE 0x424b31
// Deletes each choke point beside a map-edge continent when all but at most one of its joined areas also touch
// a map-edge continent
void EEAIContinentManager::PruneChokePoints()
{
    std::vector<EEAIContinent*> theEdges;
    U2DPoint<long> theThis;
    U2DPoint<long> theOther;
    long i;
    for (i = this->mContinents.size() - 1; i >= 0; i--) {
        if (this->mContinents[i] && this->mContinents[i]->mCategory == kContinentCategoryMapEdge)
            theEdges.push_back(this->mContinents[i]);
    }
    for (long j = this->mChokePoints.size() - 1; j >= 0; j--) {
        EEAIChokePoint* theChokePoint = this->mChokePoints[j];
        if (!theChokePoint)
            continue;
        for (i = theEdges.size() - 1; i >= 0; i--) {
            theEdges[i]->GetClosestPoints(theChokePoint, theThis, theOther);
            if (!(theThis.DistanceToSquared(theOther) <= 1))
                continue;
            long theCount = 0;
            for (long k = theChokePoint->mAreaSerials.size() - 1; k >= 0; k--) {
                EEAIArea* theArea = this->GetArea(theChokePoint->mAreaSerials[k]);
                if (!theArea)
                    continue;
                for (long n = theEdges.size() - 1; n >= 0; n--) {
                    if (theArea == theEdges[n])
                        continue;
                    theThis = U2DPoint<long>();
                    theOther = U2DPoint<long>();
                    theEdges[n]->GetClosestPoints(theArea, theThis, theOther);
                    if (theThis.DistanceToSquared(theOther) <= 1)
                        theCount++;
                }
            }
            if (theCount > 0 && theCount >= theChokePoint->mAreaSerials.size() - 1) {
                this->RemoveArea(theChokePoint);
                this->mChokePoints[j] = NULL;
                delete theChokePoint;
            }
            break;
        }
    }
}

// FUNCTION: EXE 0x424dcf
// Removes inSector's sub-sectors: unlinks their neighbours, clears them from the list and id grid, frees their ids
void EEAIContinentManager::RemoveSubSectors(EEAIPathSector* inSector)
{
    std::map<unsigned short, EEAIPathSubSector*>::iterator theIt;
    EEAIPathSubSector* theSubSector;
    long i;
    long j;
    unsigned short theID;
    std::vector<U2DPoint<long> > theTiles;
    std::vector<unsigned short> theIDs;
    theIDs.reserve(inSector->mSubSectors.size());
    for (theIt = inSector->mSubSectors.begin();
         theIt != inSector->mSubSectors.end(); ++theIt) {
        theSubSector = theIt->second;
        for (i = theSubSector->mNeighbors.size() - 1; i >= 0; i--)
            (*(theSubSector->mNeighbors.begin() + i))->RemoveNeighbor(theSubSector);
        this->mPathSubSectors[theSubSector->mID] = NULL;
        theSubSector->mTiles.BuildPointList(theTiles, 1);
        if (this->mPathSubSectorIDs) {
            for (j = theTiles.size() - 1; j >= 0; j--)
                this->mPathSubSectorIDs[this->mWidth * theTiles[j].mX + (theTiles.begin() + j)->mY] = 0;
        }
        theID = theSubSector->mID;
        theIDs.push_back(theID);
    }
    inSector->DeleteSubSectors();
    if (theIDs.size() > 0)
        this->mFreeSubSectorIDs.insert(this->mFreeSubSectorIDs.end(), theIDs.begin(), theIDs.end());
}

// FUNCTION: EXE 0x424f70
// Removes inArea from the area map; returns whether it was there
bool EEAIContinentManager::RemoveArea(EEAIArea* inArea)
{
    long theSerial = inArea->mSerial;
    std::map<unsigned long, EEAIArea*>::iterator theArea = this->mAreas.find(theSerial);
    bool theResult = theArea != this->mAreas.end();
    if (theResult)
        this->mAreas.erase(theArea);
    return theResult;
}

// FUNCTION: EXE 0x424fb5
// Drops inSector's pending rebuild tiles
void EEAIContinentManager::RemoveSectorTiles(EEAIPathSector* inSector)
{
    std::map<unsigned short, std::vector<EETileCoordinate> >::iterator theTiles = this->mSectorTiles.find(inSector->mID);
    if (theTiles != this->mSectorTiles.end())
        this->mSectorTiles.erase(theTiles);
}

// FUNCTION: EXE 0x424fea
// Records water or space tile (inX, inY) as a shore tile when a neighbour is land (not water, space or ice), else
// as an ice-shore tile when a neighbour is ice
void EEAIContinentManager::AddTileValue(EEMap* inMap, long inX, long inY)
{
    bool theIce = false;
    EETile* theTile = inMap->GetTile(inX, inY);
    if (!theTile)
        return;
    long theType = theTile->mTerrainType;
    if (theType != kTerrainDeepWater && theType != kTerrainShallowWater && theType != kTerrainSpace)
        return;
    EETile* theOther;
    long theOtherType;
    bool theLand;
    theOther = inMap->GetTile(inX - 1, inY);
    if (theOther) {
        theOtherType = theOther->mTerrainType;
        theLand = theOtherType != 1 && theOtherType != 10 && theOtherType != 9 && theOtherType != 0xc;
        theIce = theIce || !theLand && theOtherType == 9;
        if (theLand) {
            this->mShoreTiles.push_back(U2DPoint<long>(inX, inY));
            return;
        }
    }
    theOther = inMap->GetTile(inX - 1, inY + 1);
    if (theOther) {
        theOtherType = theOther->mTerrainType;
        theLand = theOtherType != 1 && theOtherType != 10 && theOtherType != 9 && theOtherType != 0xc;
        theIce = theIce || !theLand && theOtherType == 9;
        if (theLand) {
            this->mShoreTiles.push_back(U2DPoint<long>(inX, inY));
            return;
        }
    }
    theOther = inMap->GetTile(inX - 1, inY - 1);
    if (theOther) {
        theOtherType = theOther->mTerrainType;
        theLand = theOtherType != 1 && theOtherType != 10 && theOtherType != 9 && theOtherType != 0xc;
        theIce = theIce || !theLand && theOtherType == 9;
        if (theLand) {
            this->mShoreTiles.push_back(U2DPoint<long>(inX, inY));
            return;
        }
    }
    theOther = inMap->GetTile(inX + 1, inY);
    if (theOther) {
        theOtherType = theOther->mTerrainType;
        theLand = theOtherType != 1 && theOtherType != 10 && theOtherType != 9 && theOtherType != 0xc;
        theIce = theIce || !theLand && theOtherType == 9;
        if (theLand) {
            this->mShoreTiles.push_back(U2DPoint<long>(inX, inY));
            return;
        }
    }
    theOther = inMap->GetTile(inX + 1, inY + 1);
    if (theOther) {
        theOtherType = theOther->mTerrainType;
        theLand = theOtherType != 1 && theOtherType != 10 && theOtherType != 9 && theOtherType != 0xc;
        theIce = theIce || !theLand && theOtherType == 9;
        if (theLand) {
            this->mShoreTiles.push_back(U2DPoint<long>(inX, inY));
            return;
        }
    }
    theOther = inMap->GetTile(inX + 1, inY - 1);
    if (theOther) {
        theOtherType = theOther->mTerrainType;
        theLand = theOtherType != 1 && theOtherType != 10 && theOtherType != 9 && theOtherType != 0xc;
        theIce = theIce || !theLand && theOtherType == 9;
        if (theLand) {
            this->mShoreTiles.push_back(U2DPoint<long>(inX, inY));
            return;
        }
    }
    theOther = inMap->GetTile(inX, inY - 1);
    if (theOther) {
        theOtherType = theOther->mTerrainType;
        theLand = theOtherType != 1 && theOtherType != 10 && theOtherType != 9 && theOtherType != 0xc;
        theIce = theIce || !theLand && theOtherType == 9;
        if (theLand) {
            this->mShoreTiles.push_back(U2DPoint<long>(inX, inY));
            return;
        }
    }
    theOther = inMap->GetTile(inX, inY + 1);
    if (theOther) {
        theOtherType = theOther->mTerrainType;
        theLand = theOtherType != 1 && theOtherType != 10 && theOtherType != 9 && theOtherType != 0xc;
        theIce = theIce || !theLand && theOtherType == 9;
        if (theLand) {
            this->mShoreTiles.push_back(U2DPoint<long>(inX, inY));
            return;
        }
    }
    if (theIce)
        this->mIceShoreTiles.push_back(U2DPoint<long>(inX, inY));
}

// NEAR: the original calls RTSMap::GetTile out of line for the first five tiles (inline budget)
// FUNCTION: EXE 0x425325
// When no wood with resources is left on inUnit's tile: removes the tile from its forest, lowers the AI values of
// its 3x3 block by 10 (not below 0) and deletes the forest once it is empty
void EEAIContinentManager::RemoveForestTile(EEUnit* inUnit)
{
    std::pair<long, long> theTile(inUnit->mTileX, inUnit->mTileY);
    EEMap* theMap = gWorld.mMap;
    EETile* theMapTile = theMap->GetTile(theTile.first, theTile.second);
    EEAIForest* theForest = this->GetForestAt(inUnit);
    bool theEmpty = true;
    if (theMapTile && EEAIHasResourceUnit(theMapTile)) {
        for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode; theNode = theNode->mNext) {
            if (theNode->mUnit->mUnitClass->mResourceType == kResourceWood && theNode->mUnit->mResourceAmount > 0) {
                theEmpty = false;
                break;
            }
        }
    }
    if (!theForest || !theEmpty)
        return;
    theForest->RemoveTile(U2DPoint<long>(theTile.first, theTile.second));
    // the tile values of the tile and its eight neighbours drop by 10 (not below 0)
    long theValue;
    EETile* theOther = theMap->GetTile(theTile.first, theTile.second);
    if (theOther && theOther->mAIValue > 0) {
        theValue = theOther->mAIValue - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first - 1, theTile.second - 1);
    if (theOther && theOther->mAIValue > 0) {
        theValue = theOther->mAIValue - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first - 1, theTile.second);
    if (theOther && theOther->mAIValue > 0) {
        theValue = theOther->mAIValue - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first - 1, theTile.second + 1);
    if (theOther && theOther->mAIValue > 0) {
        theValue = theOther->mAIValue - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first, theTile.second - 1);
    if (theOther && theOther->mAIValue > 0) {
        theValue = theOther->mAIValue - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first, theTile.second + 1);
    if (theOther && theOther->mAIValue > 0) {
        theValue = theOther->mAIValue - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first + 1, theTile.second - 1);
    if (theOther && theOther->mAIValue > 0) {
        theValue = theOther->mAIValue - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first + 1, theTile.second);
    if (theOther && theOther->mAIValue > 0) {
        theValue = theOther->mAIValue - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first + 1, theTile.second + 1);
    if (theOther && theOther->mAIValue > 0) {
        theValue = theOther->mAIValue - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    const std::vector<EEAIContinent*>& theContinents = this->mContinents;
    EEAIContinent* theContinent;
    if (!(theForest->mTiles.Size() < 1) || (theContinent = theContinents[theForest->mContinentID]) == NULL)
        return;
    theContinent->RemoveForestID(theForest->mID);
    this->mForests[theForest->mID] = NULL;
    this->RemoveArea(theForest);
    delete theForest;
}

// FUNCTION: EXE 0x425606
// Queues the footprint tiles of a finished blocking unit or gate for a sector rebuild and tells the path manager
// (inNow: at once); skipped while clearing or before regions exist
void EEAIContinentManager::UpdateWallUnit(EEComplexUnit* inUnit, bool inNow)
{
    std::vector<EETileCoordinate> theTiles;
    if (this->mAreas.size() <= 0 || this->mClearing || !gWorld.mMap)
        return;
    if (inUnit->IsFoundation() || (!inUnit->GetUnitClass()->mBlocksTile && inUnit->GetUnitClass()->mCategory != kUnitCategoryGate))
        return;
    EEGetFootprintTiles(inUnit, theTiles);
    this->AddSectorTiles(theTiles);
    gPathManager.SetRegionUpdates(true, inNow);
}

// FUNCTION: EXE 0x426f8d
// Adds inArea to the area map under its serial
void EEAIContinentManager::AddArea(EEAIArea* inArea)
{
    this->mAreas.insert(std::map<unsigned long, EEAIArea*>::value_type(inArea->mSerial, inArea));
}

// FUNCTION: EXE 0x426fb3
// Returns the area with serial inSerial, NULL if none
EEAIArea* EEAIContinentManager::GetArea(unsigned long inSerial) const
{
    std::map<unsigned long, EEAIArea*>::const_iterator theIt = this->mAreas.find(inSerial);
    if (theIt != this->mAreas.end()) {
        return theIt->second;
    }
    return NULL;
}

// FUNCTION: EXE 0x426fdd
// Returns the continent with id inID
EEAIContinent* EEAIContinentManager::GetContinentByID(unsigned short inID) const
{
    return this->mContinents[inID];
}

// Compiler-generated and inline-header code of this unit (names in analysis/exe/names.txt; unnamed ones are not
// generated by the source yet: graph machine Run/expand steps 0x268be.., vector/_Tree members, EEAIForest ctor 0x29bcd):
// TEMPLATE: EXE 0x4221b9
// TEMPLATE: EXE 0x4221bd
// TEMPLATE: EXE 0x42568e
// TEMPLATE: EXE 0x425736
// TEMPLATE: EXE 0x42576a
// TEMPLATE: EXE 0x425795
// TEMPLATE: EXE 0x4257c0
// TEMPLATE: EXE 0x42587a
// TEMPLATE: EXE 0x425b30
// TEMPLATE: EXE 0x425b44
// TEMPLATE: EXE 0x425b70
// TEMPLATE: EXE 0x425b9c
// TEMPLATE: EXE 0x425ba0
// TEMPLATE: EXE 0x425bed
// TEMPLATE: EXE 0x425c21
// TEMPLATE: EXE 0x425c3e
// TEMPLATE: EXE 0x425c55
// TEMPLATE: EXE 0x425c91
// TEMPLATE: EXE 0x425cc3
// TEMPLATE: EXE 0x425cd9
// TEMPLATE: EXE 0x425cec
// TEMPLATE: EXE 0x425d08
// TEMPLATE: EXE 0x425d24
// TEMPLATE: EXE 0x425d71
// TEMPLATE: EXE 0x425da5
// TEMPLATE: EXE 0x425dbb
// TEMPLATE: EXE 0x425e08
// TEMPLATE: EXE 0x425e3c
// TEMPLATE: EXE 0x425e52
// TEMPLATE: EXE 0x425e9f
// TEMPLATE: EXE 0x425ed3
// TEMPLATE: EXE 0x425eff
// TEMPLATE: EXE 0x425fbb
// TEMPLATE: EXE 0x425fe7
// TEMPLATE: EXE 0x426003
// TEMPLATE: EXE 0x42601f
// TEMPLATE: EXE 0x42603b
// TEMPLATE: EXE 0x426057
// TEMPLATE: EXE 0x426073
// TEMPLATE: EXE 0x4260e4
// TEMPLATE: EXE 0x426278
// TEMPLATE: EXE 0x4262ba
// TEMPLATE: EXE 0x4262ea
// TEMPLATE: EXE 0x4262ef
// TEMPLATE: EXE 0x42630c
// TEMPLATE: EXE 0x426366
// TEMPLATE: EXE 0x4263a0
// TEMPLATE: EXE 0x4263cb
// TEMPLATE: EXE 0x4263ef
// TEMPLATE: EXE 0x42640c
// TEMPLATE: EXE 0x426430
// TEMPLATE: EXE 0x426452
// TEMPLATE: EXE 0x42647b
// TEMPLATE: EXE 0x426739
// TEMPLATE: EXE 0x42676e
// TEMPLATE: EXE 0x42679e
// TEMPLATE: EXE 0x4267ce
// TEMPLATE: EXE 0x4267fe
// TEMPLATE: EXE 0x42682e
// TEMPLATE: EXE 0x42685e
// TEMPLATE: EXE 0x42688e
// TEMPLATE: EXE 0x4268be
// TEMPLATE: EXE 0x426941
// TEMPLATE: EXE 0x426997
// TEMPLATE: EXE 0x426a1a
// TEMPLATE: EXE 0x426a9d
// TEMPLATE: EXE 0x426b20
// TEMPLATE: EXE 0x426b2a
// TEMPLATE: EXE 0x426b84
// TEMPLATE: EXE 0x426bcc
// TEMPLATE: EXE 0x426c87
// TEMPLATE: EXE 0x426cf3
// TEMPLATE: EXE 0x426d1a
// TEMPLATE: EXE 0x426d53
// TEMPLATE: EXE 0x426d84
// TEMPLATE: EXE 0x426dca
// TEMPLATE: EXE 0x426e13
// TEMPLATE: EXE 0x426e5c
// TEMPLATE: EXE 0x426e7b
// TEMPLATE: EXE 0x426ea4
// TEMPLATE: EXE 0x426f15
// TEMPLATE: EXE 0x426f46
// TEMPLATE: EXE 0x426fed
// TEMPLATE: EXE 0x427016
// TEMPLATE: EXE 0x42708d
// TEMPLATE: EXE 0x4273b9
// TEMPLATE: EXE 0x4273c9
// TEMPLATE: EXE 0x4273f1
// TEMPLATE: EXE 0x427434
// TEMPLATE: EXE 0x4274a3
// TEMPLATE: EXE 0x4275eb
// TEMPLATE: EXE 0x427604
// TEMPLATE: EXE 0x427615
// TEMPLATE: EXE 0x42768c
// TEMPLATE: EXE 0x4279b8
// TEMPLATE: EXE 0x427a2f
// TEMPLATE: EXE 0x427d5b
// TEMPLATE: EXE 0x427dd2
// TEMPLATE: EXE 0x4280fe
// TEMPLATE: EXE 0x42812f
// TEMPLATE: EXE 0x42817c
// TEMPLATE: EXE 0x4281b1
// TEMPLATE: EXE 0x4281da
// TEMPLATE: EXE 0x4282b3
// TEMPLATE: EXE 0x4282f2
// TEMPLATE: EXE 0x42832e
// TEMPLATE: EXE 0x42837d
// TEMPLATE: EXE 0x4285c1
// TEMPLATE: EXE 0x4285e3
// TEMPLATE: EXE 0x4288aa
// TEMPLATE: EXE 0x4289be
// TEMPLATE: EXE 0x428a34
// TEMPLATE: EXE 0x428aa7
// TEMPLATE: EXE 0x428b6d
// TEMPLATE: EXE 0x428b87
// TEMPLATE: EXE 0x428ba1
// TEMPLATE: EXE 0x428cb5
// TEMPLATE: EXE 0x428d2b
// TEMPLATE: EXE 0x428e3f
// TEMPLATE: EXE 0x428eb5
// TEMPLATE: EXE 0x428fc9
// TEMPLATE: EXE 0x42902c
// TEMPLATE: EXE 0x4290a2
// TEMPLATE: EXE 0x4290e2
// TEMPLATE: EXE 0x429155
// TEMPLATE: EXE 0x42921e
// TEMPLATE: EXE 0x42925b
// TEMPLATE: EXE 0x42929a
// TEMPLATE: EXE 0x4293b0
// TEMPLATE: EXE 0x4293f4
// TEMPLATE: EXE 0x429436
// TEMPLATE: EXE 0x42945f
// TEMPLATE: EXE 0x4294a6
// TEMPLATE: EXE 0x4294e0
// TEMPLATE: EXE 0x42950b
// TEMPLATE: EXE 0x429568
// TEMPLATE: EXE 0x429591
// TEMPLATE: EXE 0x4295b6
// TEMPLATE: EXE 0x4295fd
// TEMPLATE: EXE 0x42960e
// TEMPLATE: EXE 0x4296b2
// TEMPLATE: EXE 0x429720
// TEMPLATE: EXE 0x42974f
// TEMPLATE: EXE 0x4297bb
// TEMPLATE: EXE 0x42980a
// TEMPLATE: EXE 0x429812
// TEMPLATE: EXE 0x42981a
// TEMPLATE: EXE 0x429822
// TEMPLATE: EXE 0x42982a
// TEMPLATE: EXE 0x429832
// TEMPLATE: EXE 0x429851
// TEMPLATE: EXE 0x42987d
// TEMPLATE: EXE 0x4298b8
// TEMPLATE: EXE 0x4298d4
// TEMPLATE: EXE 0x429910
// TEMPLATE: EXE 0x42993c
// TEMPLATE: EXE 0x429950
// TEMPLATE: EXE 0x429994
// TEMPLATE: EXE 0x4299f5
// TEMPLATE: EXE 0x429a66
// TEMPLATE: EXE 0x429a91
// TEMPLATE: EXE 0x429ac3
// TEMPLATE: EXE 0x429b05
// TEMPLATE: EXE 0x429b3c
// TEMPLATE: EXE 0x429b5d
// TEMPLATE: EXE 0x429b7b
// TEMPLATE: EXE 0x429ced
// TEMPLATE: EXE 0x429d1b
// TEMPLATE: EXE 0x429e67
// TEMPLATE: EXE 0x429e92
// TEMPLATE: EXE 0x429f03

// FUNCTION: EXE 0x429bcd
// Creates an empty forest: no resources, no continent, id 0
// out of line (EEAIAStarPathAreaGoal::CreateAreas 0x17510 news it)
EEAIForest::EEAIForest()
    : mResourceKind(0), mNumResources1(0), mNumResources2(0), mNumResources3(0), mL78(0), mNextRenewal(0), mContinentID(0), mID(0)
{
}

// 0x563bc8 (.bss) = gUnitClassTable[0x147] (0x563bc8 - 0x5636ac = 0x51c): the unit class RegrowTrees creates
// (its vtable +0x30 is EEUnitClass::CreateUnit(x, y, player, bool, creator)); no store to it is known in the exe

// FUNCTION: EXE 0x429c01
// When inTile is in the forest, creates inCount units of type 0x147 at random forest tiles within inRadius of
// inTile (11 tries each, else inTile) flagged as regrowing; returns whether inTile is in the forest
// (EEUnit 0x22a3fd calls it)
bool EEAIForest::RegrowTrees(const U2DPoint<long>& inTile, long inRadius, long inCount)
{
    U2DRectangle<long> theArea;
    U2DPoint<long> thePoint;
    bool theResult = this->Contains(inTile);
    if (theResult) {
        theArea.Set(inTile);
        theArea.Grow(inRadius, true);
        for (long i = inCount - 1; i >= 0; i--) {
            bool theFound = false;
            for (long theTries = 10; theTries >= 0 && !theFound; theTries--) {
                long theY = gPhysics->GetRandom(theArea.mTop, theArea.mBottom);
                thePoint.mX = gPhysics->GetRandom(theArea.mLeft, theArea.mRight);
                thePoint.mY = theY;
                theFound = this->Contains(thePoint);
            }
            if (!theFound)
                thePoint = inTile;
            EEComplexUnit* theUnit = gUnitClassTable[kUnitTypeResEagle]->CreateUnit(thePoint.mX, thePoint.mY, 0, false, NULL)->mComplexUnit;
            if (theUnit->mIsRegrowing != true)
                theUnit->mIsRegrowing = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x429d69
// Sets the forest's resource kind to the highest kind (1, 2, 3) with a positive count; unchanged when none has any
void EEAIForest::UpdateResourceKind()
{
    std::map<long, long> theCounts;
    std::map<long, long>::iterator theIt;
    theCounts.insert(std::map<long, long>::value_type(1, this->mNumResources1));
    theCounts.insert(std::map<long, long>::value_type(2, this->mNumResources2));
    theCounts.insert(std::map<long, long>::value_type(3, this->mNumResources3));
    for (theIt = theCounts.begin(); theIt != theCounts.end(); ++theIt) {
        if (theIt->second > 0)
            this->mResourceKind = theIt->first;
    }
}

long EEAIHasResourceUnit(EETile* inTile);   // 0x29ced (EEAIAStarPathAreaGoal.cpp declares it the same way)

// FUNCTION: EXE 0x429e17
// Removes inPoint from the forest and, if it was there, lowers the count of that tile's wood resource kind;
// returns whether it was removed
bool EEAIForest::RemoveTile(const U2DPoint<long>& inPoint)
{
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inPoint);
    bool theResult = EEAIArea::RemoveTile(inPoint);
    if (theResult) {
        long theResource = EEAIHasResourceUnit(theTile);
        if (theResource != 0) {
            switch (theResource) {
            case 1:
                this->mNumResources1--;
                break;
            case 2:
                this->mNumResources2--;
                break;
            case 3:
                this->mNumResources3--;
                break;
            }
        }
    }
    return theResult;
}

// SYNTHETIC: EXE 0x429f35
// (EEAIForest's scalar deleting destructor: the inline dtor, then EEAIArea's)

// an empty function called last by DeleteChokePoints (ICF-folded into 0xe123d; defined after its caller, so it is
// called, not expanded: EEAIChokePoint.cpp's EEAIChokeDone shape) (provisional name)
static void EEAIChokePointsDeleted()
{
}

// an empty function called by Shutdown before FreeNodePool (ICF-folded into 0xe123d; defined after its caller, so it
// is called, not expanded) (provisional name)
static void EEAIContinentsShutDown()
{
}
