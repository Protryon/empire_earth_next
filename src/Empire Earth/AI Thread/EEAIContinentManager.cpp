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
#include "ee/EEAIFOUnitScreeners.h"
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
#include "ee/EECPUMGatherUnitsScreenBase.h"
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
#include "ee/EEAIAStarMachine.h"
#include "ee/EEAIAStarMachineSteps.h"
#include "ee/EEDebugTrace.h"
#include "ee/EEAIContinentCapture.h"   // TEST ONLY: the analysis' capture (EE_SIM_CAPTURE)
#include "ee/EESimCapture.h"

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
// not inline: other units call it (VC6 left an out-of-line copy of the inline for them; clang does not)
long EEAIHasResourceUnit(EETile* inTile)
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

// 0x27604 (COMDAT; declared in ee/EEAIContinentManager.h)
inline void EEAIContinentManager::SetPathSector(EEAIPathSector* inSector)
{
    this->SetTable(EeAStarCmTable::PathSectors, inSector->mID, inSector);
}

// 0x26fed (COMDAT; declared in ee/EEAIContinentManager.h)
// not inline: other units call it (VC6 left an out-of-line copy of the inline for them; clang does not)
bool EEAIContinentManager::IsShoreTile(long inX, long inY) const
{
    return ee_astar_cm_is_shore(&this->mRecord, inX, inY);
}

// 0x273b9 (COMDAT; declared in ee/EEAIContinentManager.h)
inline EEAIPathSector* EEAIContinentManager::GetPathSectorByID(unsigned short inID) const
{
    return this->GetPathSectors()[inID];
}

// 0x26d84/0x26dca/0x26e13/0x26ea4: EEAIContinent's AddChokePoint, AddForestID, AddHillID and RemoveForestID (ee_astar
// continent.rs, inline in ee/EEAIContinent.h)

// 0x273c9/0x273f1: GetPathSubSectorAt/GetPathSubSectorID(long, long) are inline in ee/EEAIContinentManager.h

// The units of a map tile, in its list's order, as ee_astar tile_kind.rs reads them (pure member reads)
static void EEAITileUnits(EETile* inTile, std::vector<EeTileUnit>& outUnits)
{
    outUnits.clear();
    for (RTSTileUnitNode* theNode = inTile->mUnits; theNode; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        EEUnitClass* theClass = theUnit->GetUnitClass();
        unsigned long theType = theClass->GetUnitType();
        EeTileUnit theRecord;
        theRecord.wood = theClass->mResourceType == kResourceWood;
        theRecord.has_amount = theUnit->mResourceAmount > 0;
        theRecord.blocks = theClass->mBlocksTile != 0;
        theRecord.gate = theClass->mCategory == kUnitCategoryGate;
        theRecord.wall = theClass->mIsWall != 0;
        theRecord.foundation = theUnit->IsFoundation();
        theRecord.alive = theUnit->mHitPoints > 0;
        theRecord.dock = theType == kUnitTypeDock || theType == kUnitTypeNavyYard;
        theRecord.space_dock = theType == kUnitTypeSpaceDock || theType == kUnitTypeSpaceTurret || theType == kUnitTypeOrbitalSpaceStation;
        theRecord.id = (int32_t)theUnit->mUnitID;
        outUnits.push_back(theRecord);
    }
}

// The map's diagonal edge lines (ee_astar path_area_kind.rs)
static EeMapEdges EEAIMapEdges(EEMap* inMap)
{
    EeMapEdges theEdges;
    theEdges.sum_low = (int32_t)inMap->GetL195608();
    theEdges.half = (int32_t)inMap->GetL195610();
    theEdges.sum_high = (int32_t)inMap->GetL19560C();
    return theEdges;
}

// Returns the AI tile kind of inTile: 0 off the map; by continent category and shore flag (land 1/7..9, water 2/10/11,
// space 15/10/11, map edge 6, cliff 3); on land 4 for live wood, 12/13/14 under a live blocking unit, wall or gate
// (Rust: ee_astar tile_kind.rs over the tile's continent category, terrain, AI value and units)
long EEAIGetTileKind(const EETileLocation& inTile)
{
    EETile* theTile = gWorld.mMap->GetTile(inTile.GetX(), inTile.GetY());
    if (!theTile)
        return 0;
    unsigned char theKind = gContinentManager.GetContinentAt(inTile.GetX(), inTile.GetY())->GetCategory();
    long theFlag = gContinentManager.GetTileFlag(inTile.GetX(), inTile.GetY());
    std::vector<EeTileUnit> theUnits;
    EEAITileUnits(theTile, theUnits);
    return ee_astar_tile_kind(theKind, (int32_t)theTile->mTerrainType, (int32_t)theFlag, theUnits.data(), theUnits.size());
}

// Returns the id of the first live, finished blocking unit or gate on inTile (wood only if not used up), -1 if none
// (Rust: ee_astar tile_kind.rs)
long EEAIGetTileWallID(const EETileLocation& inTile)
{
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inTile);
    if (!theTile)
        return -1;
    std::vector<EeTileUnit> theUnits;
    EEAITileUnits(theTile, theUnits);
    return ee_astar_tile_wall_id(theUnits.data(), theUnits.size());
}

static void EEAIContinentsShutDown();

// TEST ONLY (docs/RUST_PLAN.md R5b.1, tools/sim_capture.py): CNTQ, one record per outermost call of the continent
// manager's queries and changes (a u32 kind, the inputs, the result: an area as its serial and type, 0xffffffff and
// -1 for none; a change ends with the FNV-1a 64 of the whole state CNTR records, EEAIContinentDigest). The record
// reads the arguments, the results and the state only, so it is the same over any implementation of them.
namespace {
thread_local int sContinentQueryDepth = 0;
}

class EEContinentQueryCapture {
public:
    explicit EEContinentQueryCapture(unsigned int inKind)
        : mOn(sContinentQueryDepth == 0 && EESimCaptureRecord::Active()), mRecord(kContinentCaptureQuery)
    {
        sContinentQueryDepth++;
        if (this->mOn)
            this->mRecord.U32(inKind);
    }
    ~EEContinentQueryCapture()
    {
        sContinentQueryDepth--;
        if (this->mOn)
            this->mRecord.Commit();
    }
    void I(long inValue) { if (this->mOn) this->mRecord.I32((int)inValue); }
    void Tile(const U2DPoint<long>& inTile) { this->I(inTile.mX); this->I(inTile.mY); }
    void Unit(const EEUnit* inUnit) { this->I(inUnit != NULL ? (long)inUnit->mUnitID : -1); }
    void Area(const EEAIArea* inArea)
    {
        this->I(inArea != NULL ? (long)inArea->GetSerial() : -1);
        this->I(inArea != NULL ? inArea->GetAreaType() : -1);
    }
    void Shorts(const std::vector<unsigned short>& inValues)
    {
        this->I((long)inValues.size());
        for (size_t i = 0; i < inValues.size(); i++)
            this->I(inValues[i]);
    }
    void Points(const std::vector<U2DPoint<long> >& inPoints)
    {
        this->I((long)inPoints.size());
        for (size_t i = 0; i < inPoints.size(); i++)
            this->Tile(inPoints[i]);
    }
    template <class T> void Areas(const std::vector<T*>& inAreas)
    {
        this->I((long)inAreas.size());
        for (size_t i = 0; i < inAreas.size(); i++)
            this->Area(inAreas[i]);
    }
    void State()
    {
        if (!this->mOn)
            return;
        unsigned long long theDigest = EEAIContinentDigest();
        this->mRecord.U32((unsigned int)theDigest);
        this->mRecord.U32((unsigned int)(theDigest >> 32));
    }
    bool mOn;
    EESimCaptureRecord mRecord;
};

enum {
    kQueryConnectedIDs = 1, kQueryAdjacentIDs, kQueryHasShoreTile, kQueryInSameContinent, kQueryInSameArea,
    kQueryCanGetTo, kQueryChokePointAt, kQueryChokePoints, kQueryContinentCenter, kQueryContinentTiles, kQueryForestAt,
    kQueryHillAt, kQueryConnectedForUnit, kQueryConnected, kQueryLargestContinent, kQueryPathAreaAt, kQueryContinent,
    kQuerySubSectorAt, kQueryRandomArea, kQueryRandomContinent, kQueryNextToArea, kQueryRepairPathArea,
    kQueryRemoveForestTile, kQueryUpdateWallUnit
};

// Resets the global continent manager, frees its tile grids (flags, continent ids, values, sub-sector ids) and the
// A* node pool
void EEAIContinentManager::Shutdown()
{
    gContinentManager.Reset();
    gContinentManager.FreeGrids();
    EEAIContinentsShutDown();
    EEAIAStarStorage::FreeNodePool();
}
// Creates an empty continent manager: no grids, every id and counter at 0 (the record: ee_sim::astar cm.rs)
EEAIContinentManager::EEAIContinentManager()
{
    ee_astar_cm_init(&this->mRecord);
}

// Destroys the continent manager by shutting down the global one (Shutdown), then frees its tables
EEAIContinentManager::~EEAIContinentManager()
{
    EEAIContinentManager::Shutdown();
    ee_astar_cm_free(&this->mRecord);
}

// New (port): frees the four tile grids
void EEAIContinentManager::FreeGrids()
{
    ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::TileFlags, false, 0);
    ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::ContinentIds, false, 0);
    ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::TileValues, false, 0);
    ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::SubSectorIds, false, 0);
}

// New (port): deletes every object of a table, then empties it (DeleteVectorContents)
template <class T> static void EEAIDeleteTable(EEAIContinentManager& ioManager, EeAStarCmTable inTable)
{
    EEAIHandleView<T> theObjects = ioManager.Table<T>(inTable);
    for (size_t i = 0; i < theObjects.size(); i++)
        delete theObjects[i];
    ioManager.ResizeTable(inTable, 0);
}

// Registers a choke point under its index, adds it to its continent's choke point list and to the area map
void EEAIContinentManager::AddChokePoint(EEAIChokePoint* inChokePoint)
{
    this->SetTable(EeAStarCmTable::ChokePoints, inChokePoint->GetIndex(), inChokePoint);
    EEAIHandleView<EEAIContinent> theContinents = this->GetContinents();
    EEAIContinent* theContinent = theContinents[inChokePoint->GetContinentID()];
    if (theContinent)
        theContinent->AddChokePoint(inChokePoint->GetIndex());
    this->AddArea(inChokePoint);
}

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
        ee_astar_cm_add_sector_tile(&this->mRecord, theSector->mID, U2DHullPointOf(ioTiles[i]));
    }
}

// Registers a rectangle area under its id, updates its continents and adds it to the area map
void EEAIContinentManager::AddRectangleArea(EEAIRectangleArea* inArea)
{
    EEAIRectangleArea* theArea = dynamic_cast<EEAIRectangleArea*>(inArea);
    this->SetTable(EeAStarCmTable::RectangleAreas, theArea->GetID(), theArea);
    theArea->UpdateContinents();
    this->AddArea(inArea);
}

// Fills ioAreas (indexed x * size + y) with the path area of every tile of inMap, NULL off the grid or the map
void EEAIContinentManager::GetPathAreas(EEMap* inMap, EEAIPathArea**& ioAreas)
{
    long theSize = inMap->mSize;
    for (long x = theSize - 1; x >= 0; x--) {
        for (long y = inMap->mSize - 1; y >= 0; y--) {
            if ((unsigned long)x < this->mRecord.width && (unsigned long)y < this->mRecord.height && inMap->GetTile(x, y)) {
                EETileLocation theTile(x, y);
                ioAreas[x * theSize + y] = this->GetPathAreaAt(theTile);
            } else {
                ioAreas[x * theSize + y] = NULL;
            }
        }
    }
}

// Deletes every continent, choke point, forest, hill, rectangle area, path area and sector, empties the area map,
// pending sector tiles, shore lists and free ids, and zeroes every counter
void EEAIContinentManager::Reset()
{
    ee_astar_cm_clear_sector_tiles(&this->mRecord);
    this->DeleteChokePoints();
    this->DeleteForests();
    this->DeleteHills();
    this->DeleteContinents();
    this->DeleteRectangleAreas();
    this->DeletePathAreas();
    this->DeletePathSectors();
    this->ResizeTable(EeAStarCmTable::PathSubSectors, 0);
    ee_astar_cm_reset(&this->mRecord);   // the serial table, the counters, the shore lists, the free path area ids
}

static void EEAIChokePointsDeleted();

// Deletes every choke point and empties the list
void EEAIContinentManager::DeleteChokePoints()
{
    EEAIDeleteTable<EEAIChokePoint>(*this, EeAStarCmTable::ChokePoints);
    EEAIChokePointsDeleted();
}

// Deletes every continent and zeroes the land, water and space tile counts
void EEAIContinentManager::DeleteContinents()
{
    EEAIDeleteTable<EEAIContinent>(*this, EeAStarCmTable::Continents);
    this->mRecord.num_land_tiles = 0;
    this->mRecord.num_water_tiles = 0;
    this->mRecord.num_space_tiles = 0;
}

// Deletes every forest
void EEAIContinentManager::DeleteForests()
{
    EEAIDeleteTable<EEAIForest>(*this, EeAStarCmTable::Forests);
}

// Deletes every hill
void EEAIContinentManager::DeleteHills()
{
    EEAIDeleteTable<EEAIHill>(*this, EeAStarCmTable::Hills);
}

// Deletes every path area
void EEAIContinentManager::DeletePathAreas()
{
    EEAIDeleteTable<EEAIPathArea>(*this, EeAStarCmTable::PathAreas);
}

// Deletes every path sector and resets the sub-sector ids (last id, free list) and the sector grid information
void EEAIContinentManager::DeletePathSectors()
{
    EEAIDeleteTable<EEAIPathSector>(*this, EeAStarCmTable::PathSectors);
    ee_astar_cm_sectors_deleted(&this->mRecord);
}

// Deletes every rectangle area
void EEAIContinentManager::DeleteRectangleAreas()
{
    EEAIDeleteTable<EEAIRectangleArea>(*this, EeAStarCmTable::RectangleAreas);
}

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

// Makes inSubSector and the sub-sector *inID neighbours both ways; with inAdd also records the diagonal link pair
// inLink on each, without it drops their existing links
void EEAIContinentManager::LinkSubSectors(EEAIPathSubSector* inSubSector, const unsigned short* inID, bool inAdd,
    const std::pair<EEAIPathSubSector*, EEAIPathSubSector*>& inLink)
{
    EEAIPathSubSector* theOther = this->GetPathSubSectors()[*inID];
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

// Allocates a zeroed per-tile value grid (width x height), freeing the old one
void EEAIContinentManager::AllocateTileValues()
{
    ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::TileValues, true, this->mRecord.height * this->mRecord.width);
}

// Resets the region checksum to 0
void EEAIContinentManager::ClearChecksum()
{
    this->mRecord.checksum = 0;
}

// Builds the choke points of inMap with a search size by map size (4 on small maps up to 7 on huge ones; Rust:
// ee_astar sectors.rs choke_passes), then drops those beside the map edge (PruneChokePoints)
void EEAIContinentManager::Build(EEMap* inMap)
{
    EEAIBuildChokePoints(inMap, ee_astar_choke_passes((int32_t)inMap->mL195610));   // Rust: ee_astar sectors.rs
    this->PruneChokePoints();
}

// Floods inMap into continents (one fill per tile not yet in one), registers them by id with fresh serials,
// counts the land, water and space tiles, then builds the continent grid (CreateContinentGrid)
void EEAIContinentManager::CreateContinents(EEMap* inMap)
{
    EEAIContinentMachine theMachine(6, inMap);
    EEAIAStarStorage* theStorage = &theMachine;
    theStorage->Init(inMap->GetSize(), inMap->GetSize());
    for (long x = inMap->GetSize() - 1; x >= 0; x--) {
        for (long y = inMap->GetSize() - 1; y >= 0; y--) {
            if (theMachine.mTiles60[theMachine.mWidth * x + y].mID != 0)
                continue;
            EETile* theTile = inMap->GetTile(x, y);
            if (!theTile)
                continue;
            theMachine.mCheckCorners = ee_astar_check_corners((int32_t)theTile->GetTerrainType(), kTerrainCliff, (int32_t)x,
                                                              (int32_t)y, EEAIMapEdges(inMap));   // Rust: ee_astar path_area_kind.rs
            theMachine.mGoal.mStartTile.mX = x;
            theMachine.mGoal.mStartTile.mY = y;
            theMachine.mGoal.mGoalTile.mX = inMap->GetSize();
            theMachine.mGoal.mGoalTile.mY = inMap->GetSize();
            theMachine.Run();
            theMachine.mGoal.CollectTiles();
            theMachine.mGoal.CreateContinent();
        }
    }
    this->DeleteContinents();
    this->ResizeTable(EeAStarCmTable::Continents, theMachine.mGoal.mNextID);
    for (std::map<unsigned short, EEAIContinent*>::const_iterator theIt = theMachine.mGoal.mContinents.begin();
         theIt != theMachine.mGoal.mContinents.end(); ++theIt) {
        EEAIContinent* theContinent = theIt->second;
        if (theContinent->GetCategory() == kContinentCategoryWater)
            this->mRecord.num_water_tiles += theContinent->GetNumTiles();
        else if (theContinent->GetCategory() == kContinentCategorySpace)
            this->mRecord.num_space_tiles += theContinent->GetNumTiles();
        else
            this->mRecord.num_land_tiles += theContinent->GetNumTiles();
        theContinent->SetSerial(this->NewSerial());
        this->AddArea(theContinent);
        this->SetTable(EeAStarCmTable::Continents, theIt->first, theContinent);
    }
    this->CreateContinentGrid(theMachine, inMap);
}

// Builds the tile-to-continent id grid, the continent adjacency matrix and neighbour lists from the fill, collects
// the shore tiles (also onto each continent), then sorts the shore lists and every continent's tiles
// (Rust: ee_astar continent_grid.rs create_continent_grid over the fill's tiles, every tile's terrain and the
// continents' categories; this stores what it computed. The ice shore list and the continents' shore lists are
// empty here: Reset cleared the one, the fill made the others)
void EEAIContinentManager::CreateContinentGrid(EEAIContinentMachine& inMachine, EEMap* inMap)
{
    long x;
    long y;
    size_t i;
    size_t theCount;
    EEMap* theMap = inMachine.mGoal.mMap;
    this->mRecord.width = inMap->mSize;
    this->mRecord.height = inMap->mSize;
    ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::ContinentIds, true, this->mRecord.width * this->mRecord.height);
    this->mRecord.flags_width = inMachine.mGoal.mNextID;
    ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::TileFlags, true, this->mRecord.flags_width * this->mRecord.flags_width);
    long theSize = inMap->mSize;
    std::vector<uint16_t> theIDs(theSize * theSize);
    std::vector<uint8_t> theKinds(theSize * theSize);
    std::vector<int32_t> theTerrain(theSize * theSize);
    for (x = 0; x < theSize; x++) {
        for (y = 0; y < theSize; y++) {
            theIDs[x * theSize + y] = inMachine.mTiles60[inMachine.mWidth * x + y].mID;
            theKinds[x * theSize + y] = inMachine.mTiles60[inMachine.mWidth * x + y].mKind;
            EETile* theTile = theMap->GetTile(x, y);   // NULL outside the playable map: -1 in the grid
            theTerrain[x * theSize + y] = theTile ? (int32_t)theTile->mTerrainType : -1;
        }
    }
    std::vector<uint8_t> theCategories(this->mRecord.flags_width);
    for (i = 0; i < theCategories.size(); i++)
        theCategories[i] = i < this->GetNumContinents() && this->GetContinentsAt(i) ? this->GetContinentsAt(i)->GetCategory() : 0;
    EeContinentGrid* theGrid = ee_astar_continent_grid_build((int32_t)theSize, theIDs.data(), theKinds.data(),
                                                              theTerrain.data(), theCategories.data(), (int32_t)this->mRecord.flags_width);
    const uint16_t* theGridIDs = ee_astar_continent_grid_ids(theGrid, &theCount);
    for (i = 0; i < theCount; i++)
        this->mRecord.continent_ids[i] = theGridIDs[i];
    const uint8_t* theFlags = ee_astar_continent_grid_flags(theGrid, &theCount);
    for (i = 0; i < theCount; i++)
        this->mRecord.tile_flags[i] = theFlags[i] != 0;
    const EeContinentNeighbor* theNeighbors = ee_astar_continent_grid_neighbors(theGrid, &theCount);
    for (i = 0; i < theCount; i++)
        this->GetContinents()[theNeighbors[i].id]->AddNeighbor(this->GetContinents()[theNeighbors[i].neighbor]);
    for (int theList = 0; theList < 2; theList++) {   // the shore and the ice shore lists (the latter empty before)
        const EePoint* theShore = theList == 0 ? ee_astar_continent_grid_shore(theGrid, &theCount)
                                               : ee_astar_continent_grid_ice_shore(theGrid, &theCount);
        std::vector<U2DPoint<long> > theTiles;
        theTiles.reserve(theCount);
        for (i = 0; i < theCount; i++)
            theTiles.push_back(U2DPoint<long>(theShore[i].x, theShore[i].y));
        ee_astar_cm_set_shore(&this->mRecord, (EeAStarCmShore)theList, U2DHullPoints(theTiles), theTiles.size());
    }
    const EeContinentShore* theContinentShore = ee_astar_continent_grid_continent_shore(theGrid, &theCount);
    for (i = 0; i < theCount; i++)
        this->GetContinents()[theContinentShore[i].id]->AddShoreTile(
            U2DPoint<long>(theContinentShore[i].tile.x, theContinentShore[i].tile.y));
    ee_astar_continent_grid_free(theGrid);
}

// Numbers the continent chains of kind inKind (which of water, cliff and space a chain may cross): one search from
// every land continent without a chain id of that kind yet (inMap is not read)
void EEAIContinentManager::NumberContinentChains(EEMap* inMap, unsigned long inKind)
{
    EEAIContinentChainMachine theMachine(7, this->GetContinents().size());
    EEAIAStarGraphStorage* theStorage = &theMachine;
    theStorage->Init(this->GetContinents().size());
    if (inKind == kAIChainLandCliff || inKind == kAIChainLandWaterCliff || inKind == kAIChainLandCliffSpace)
        theMachine.mGoal.mKinds[3] = true;
    if (inKind == kAIChainLandWater || inKind == kAIChainLandWaterCliff || inKind == kAIChainLandWaterSpace)
        theMachine.mGoal.mKinds[2] = true;
    if (inKind == kAIChainLandSpace || inKind == kAIChainLandWaterSpace || inKind == kAIChainLandCliffSpace)
        theMachine.mGoal.mKinds[4] = true;
    theMachine.mGoal.mKinds[1] = true;
    for (long i = this->GetContinents().size() - 1; i >= 0; i--) {
        EEAIContinent* theContinent = this->GetContinents()[i];
        if (!theContinent || theContinent->GetCategory() != kContinentCategoryLand)
            continue;
        unsigned short theCount = inKind < 7 ? theContinent->GetCount(inKind) : 0;
        if (theCount != 0)
            continue;
        unsigned short theID = theContinent->GetID();
        theMachine.mGoal.mStartTile = theContinent->GetHullCenter();
        theMachine.mGoal.mGoalSector = theID;
        theMachine.mGoal.mStartContinent = theContinent;
        theMachine.mGoal.mStartCategory = theContinent->GetCategory();
        theMachine.mGoal.mKind = inKind;
        theMachine.Run();
        theMachine.mGoal.NumberChain();
    }
}

// Grows the hills of inMap: one fill from every non-cliff land tile at height level 4 or more not yet in a hill,
// then registers each hill (serial, its continent, the area map)
void EEAIContinentManager::CreateHills(EEMap* inMap)
{
    EEAIHillMachine theMachine(10, inMap);
    EEAIHillMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(inMap->GetSize(), inMap->GetSize());
    theMachine.mCheckCorners = false;
    for (long x = inMap->GetSize() - 1; x >= 0; x--) {
        for (long y = inMap->GetSize() - 1; y >= 0; y--) {
            EEAIContinent* theContinent = this->GetContinentAt(x, y);
            if (!theContinent || theContinent->GetCategory() == kContinentCategoryWater || theContinent->GetCategory() == kContinentCategorySpace ||
                theMachine.mTiles60[theMachine.mHeight * x + y].mID != 0)
                continue;
            EETile* theTile = inMap->GetTile(x, y);
            if (!theTile)
                continue;
            long theLevel = UMFloor(theTile->GetHeight() / (inMap->GetF1955F4() * 0.5f) + 0.00001f);
            if (theTile->GetTerrainType() == kTerrainCliff || theLevel < 4)
                continue;
            theMachine.mGoal.mStartTile.SetX(x);
            theMachine.mGoal.mStartTile.SetY(y);
            theMachine.mGoal.mGoalTile.SetX(inMap->GetSize());
            theMachine.mGoal.mGoalTile.SetY(inMap->GetSize());
            theMachine.Run();
            theMachine.mGoal.CollectTiles();
            theMachine.mGoal.CreateHill();
        }
    }
    this->ResizeTable(EeAStarCmTable::Hills, theMachine.mGoal.mNextID);
    for (std::map<unsigned short, EEAIHill*>::const_iterator theIt = theMachine.mGoal.mHills.begin();
         theIt != theMachine.mGoal.mHills.end(); ++theIt) {
        EEAIHill* theHill = theIt->second;
        theHill->SetSerial(this->NewSerial());
        this->SetTable(EeAStarCmTable::Hills, theHill->GetID(), theHill);
        EEAIHandleView<EEAIContinent> theContinents = this->GetContinents();
        EEAIContinent* theContinent = theContinents[theHill->GetContinentID()];
        if (theContinent)
            theContinent->AddHillID(theHill->GetID());
        this->AddArea(theHill);
    }
}

// Assigns the inKind region ids (water body, land mass, space or surface region) to the path areas: one fill from
// every matching tile whose area has no such id yet, then keeps the next id of that kind (Rust: ee_astar
// path_area_kind.rs, the start test and the corner test of each tile)
void EEAIContinentManager::CreatePathAreaKind(EEMap* inMap, long inKind)
{
    EEAIPathAreaMachine theMachine(0xe, inMap);
    long theSize = inMap->GetSize();
    EEAIPathArea** theAreas = NULL;
    bool theFlag = false;
    EEAIPathAreaMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(theSize, theSize);
    theMachine.mCheckCorners = true;
    theAreas = new EEAIPathArea*[theSize * theSize];
    this->GetPathAreas(inMap, theAreas);
    theMachine.mGoal.mAreas = theAreas;
    theMachine.mGoal.mAreasSize = theSize;
    EeMapEdges theEdges = EEAIMapEdges(inMap);
    std::vector<EeTileUnit> theUnits;
    for (long x = inMap->GetSize() - 1; x >= 0; x--) {
        for (long y = inMap->GetSize() - 1; y >= 0; y--) {
            EETile* theTile = inMap->GetTile(x, y);
            if (!theTile)
                continue;
            // the start test (Rust: ee_astar path_area_kind.rs): the tile's terrain and place, its area's kind and
            // region ids, a live dock or space dock on an obstacle area; theFlag carries from tile to tile
            EEAIPathArea* theArea = theAreas[x * theSize + y];
            EePathAreaIDs theIDs;
            theIDs.present = theArea != NULL;
            theIDs.kind = theArea ? (int32_t)theArea->GetKind() : 0;
            theIDs.land_mass = theArea ? theArea->GetLandMassID() : 0;
            theIDs.water_body = theArea ? theArea->GetWaterBodyID() : 0;
            theIDs.space_region = theArea ? theArea->GetSpaceRegionID() : 0;
            theIDs.surface_region = theArea ? theArea->GetSurfaceRegionID() : 0;
            EEAITileUnits(theTile, theUnits);
            if (!ee_astar_path_area_kind_starts((int32_t)inKind, (int32_t)theTile->GetTerrainType(), (int32_t)x, (int32_t)y,
                                                theEdges, &theIDs, theUnits.data(), theUnits.size(), &theFlag))
                continue;
            theMachine.mGoal.mMode = inKind;
            theMachine.mCheckCorners = ee_astar_check_corners((int32_t)theTile->GetTerrainType(), 3, (int32_t)x, (int32_t)y,
                                                              theEdges);   // terrain 3 here, as the original
            theMachine.mGoal.mStartTile.mY = y;
            theMachine.mGoal.mStartTile.mX = x;
            theMachine.mGoal.mGoalTile.mX = inMap->GetSize();
            theMachine.mGoal.mGoalTile.mY = inMap->GetSize();
            theMachine.Run();
            theMachine.mGoal.CollectTiles();
            theMachine.mGoal.CreateAreas();
        }
    }
    if (inKind == kAIPathLandMass)
        this->mRecord.next_land_mass = theMachine.mGoal.GetNextKindID();
    else if (inKind == kAIPathWaterBody)
        this->mRecord.next_water_body = theMachine.mGoal.GetNextKindID();
    else if (inKind == kAIPathSpaceRegion)
        this->mRecord.next_space_region = theMachine.mGoal.GetNextKindID();
    else if (inKind == kAIPathNonSpace)
        this->mRecord.next_surface_region = theMachine.mGoal.GetNextKindID();
    delete[] theAreas;
    theMachine.SetHasBounds(false);
    theMachine.Reset();
}

// Grows the path areas and forests of inMap and registers them (serials, continents, area map), builds the path
// sectors, then records each sub-sector's continent as a neighbour of its sector
void EEAIContinentManager::CreatePathAreas(EEMap* inMap)
{
    EEAIPathAreaMachine theMachine(0xe, inMap);
    EEAIPathAreaMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(inMap->GetSize(), inMap->GetSize());
    theMachine.mCheckCorners = true;
    for (long x = inMap->GetSize() - 1; x >= 0; x--) {
        for (long y = inMap->GetSize() - 1; y >= 0; y--) {
            if (!this->GetContinentAt(x, y) || theMachine.mPathAreaIDs[theMachine.GetHeight() * x + y] != 0)
                continue;
            theMachine.mGoal.mStartTile.mX = x;
            theMachine.mGoal.mStartTile.mY = y;
            theMachine.mGoal.mGoalTile.mX = inMap->GetSize();
            theMachine.mGoal.mGoalTile.mY = inMap->GetSize();
            theMachine.Run();
            theMachine.mGoal.CollectTiles();
            theMachine.mGoal.CreateAreas();
        }
    }
    unsigned short theCount = theMachine.mGoal.mNextPathAreaID;
    this->ResizeTable(EeAStarCmTable::PathAreas, theCount);
    this->mRecord.next_path_area_id = theCount;
    std::map<unsigned short, EEAIPathArea*>::const_iterator theArea;
    for (theArea = theMachine.mGoal.mPathAreas.begin(); theArea != theMachine.mGoal.mPathAreas.end(); ++theArea) {
        EEAIPathArea* thePathArea = theArea->second;
        thePathArea->SetSerial(this->NewSerial());
        this->SetTable(EeAStarCmTable::PathAreas, thePathArea->GetID(), thePathArea);
        EEAIContinent* theContinent = this->GetContinentByID(thePathArea->GetContinentID());
        if (theContinent)
            theContinent->AddPathAreaID(thePathArea->GetID());
        this->AddArea(thePathArea);
    }
    this->ResizeTable(EeAStarCmTable::Forests, theMachine.mGoal.mNextForestID);
    std::map<unsigned short, EEAIForest*>::const_iterator theForest;
    for (theForest = theMachine.mGoal.mForests.begin(); theForest != theMachine.mGoal.mForests.end(); ++theForest) {
        EEAIForest* theArea = theForest->second;
        theArea->SetSerial(this->NewSerial());
        this->SetTable(EeAStarCmTable::Forests, theArea->GetID(), theArea);
        EEAIContinent* theContinent = this->GetContinentsAt(theArea->GetContinentID());
        if (theContinent)
            theContinent->AddForestID(theArea->GetID());
        this->AddArea(theArea);
    }
    theMachine.SetHasBounds(false);
    theMachine.Reset();
    this->CreatePathSectors(inMap, &theMachine);
    for (long i = this->GetPathSubSectors().size() - 1; i >= 0; i--) {
        EEAIPathSubSector* theSubSector = this->GetPathSubSectorsAt(i);
        EEAIContinent* theContinent = theSubSector ? this->GetContinentsAt(theSubSector->mContinentID) : NULL;
        if (theContinent)
            theSubSector->mSector->AddNeighbor(theContinent);
    }
}

// Builds the path-sector grid of inMap (sector and region size by map size) and the sub-sectors (one fill per tile
// not yet in one, kept inside its sector), files them with their path areas, then links them (LinkSubSectorGrid)
void EEAIContinentManager::CreatePathSectors(EEMap* inMap, EEAIAStarPathAreaStorage* inStorage)
{
    EEAIPathSectorMachine theMachine(0xf, inMap);
    U2DRectangle<long> theBounds;
    unsigned short theID = 1;
    this->DeletePathSectors();
    EeSectorSizes theSizes = ee_astar_sector_sizes((int32_t)inMap->mL195610);   // Rust: ee_astar sectors.rs
    long theSectorSize = theSizes.sector_size;
    gEEAIRegionSize = theSizes.region_size;
    EEAIPathSectorMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(inMap->mSize, inMap->mSize);
    ee_astar_cm_init_sectors(&this->mRecord, inMap->mSize, theSectorSize);
    this->ResizeTable(EeAStarCmTable::PathSectors, this->mRecord.sectors_x * this->mRecord.sectors_y + 1);
    for (long x = 0; x < this->mRecord.sectors_x; x++) {
        for (long y = 0; y < this->mRecord.sectors_y; y++) {
            EEAIPathSector* thePathSector = new EEAIPathSector;
            thePathSector->mID = theID;
            theID++;
            // the sector's rectangle, clipped to the map (Rust: ee_astar sectors.rs sector_rect, GetSectorEnd 0x263cb
            // included)
            EeSectorRect theRect = ee_astar_sector_rect((int32_t)x, (int32_t)y, (int32_t)this->mRecord.sector_size,
                                                       (int32_t)inMap->mSize);
            theBounds.mTop = theRect.top;
            theBounds.mLeft = theRect.left;
            theBounds.mBottom = theRect.bottom;
            theBounds.mRight = theRect.right;
            thePathSector->mCenter.mX = theRect.center_x;
            thePathSector->mCenter.mY = theRect.center_y;
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
    this->ResizeTable(EeAStarCmTable::PathSubSectors, this->mRecord.last_sub_sector_id + 1);
    for (std::map<unsigned short, EEAIPathSubSector*>::const_iterator theIt = theMachine.mGoal.mSubSectors.begin();
         theIt != theMachine.mGoal.mSubSectors.end(); ++theIt) {
        EEAIPathSubSector* theSubSector = theIt->second;
        theSubSector->UpdateLocation();
        this->SetTable(EeAStarCmTable::PathSubSectors, theIt->first, theSubSector);
        EEAIPathArea* theArea = this->GetPathArea(theSubSector->mPathAreaID);
        if (theArea) {
            EETileCoordinate theTile;
            theTile = theSubSector->mLocation;
            EEAIPathSector* thePathSector = this->GetPathSector(theTile);
            if (thePathSector) {
                theArea->AddSectorID(thePathSector->mID);
                thePathSector->AddPathAreaID(theArea->GetID());
            }
        }
    }
    this->LinkSubSectorGrid(theMachine, inMap);
}

// The link walks' view of the sub sectors (ee_astar sub_sector_links.rs): the id at a tile from the fill's grid
// (LinkSubSectorGrid) or the manager's lookup (RelinkSubSectors), and whether the manager lists an id
struct EEAISubSectorGridView {
    EEAIContinentManager* mManager;
    EEAIPathSectorMachine* mMachine;    // NULL: the manager's GetPathSubSectorID
};

static uint16_t EEAISubSectorIDAt(void* inView, int32_t inX, int32_t inY)
{
    EEAISubSectorGridView* theView = (EEAISubSectorGridView*)inView;
    if (theView->mMachine)
        return theView->mMachine->mSubSectorIDs[theView->mMachine->GetHeight() * inX + inY];
    return theView->mManager->GetPathSubSectorID(inX, inY);
}

static bool EEAISubSectorPresent(void* inView, uint16_t inID)
{
    return ((EEAISubSectorGridView*)inView)->mManager->GetPathSubSectors()[inID] != NULL;
}

// Makes the LinkSubSectors calls of a walk, in its order
void EEAIContinentManager::ApplySubSectorLinks(EeSubSectorLinks* inLinks)
{
    EEAIHandleView<EEAIPathSubSector> theSubSectors = this->GetPathSubSectors();
    size_t theCount;
    const EeSubSectorLink* theLinks = ee_astar_sub_sector_links_get(inLinks, &theCount);
    for (size_t i = 0; i < theCount; i++) {
        unsigned short theOther = theLinks[i].other;
        std::pair<EEAIPathSubSector*, EEAIPathSubSector*> theLink(
            theLinks[i].first ? theSubSectors[theLinks[i].first] : NULL,
            theLinks[i].second ? theSubSectors[theLinks[i].second] : NULL);
        this->LinkSubSectors(theSubSectors[theLinks[i].sub], &theOther, theLinks[i].add, theLink);
    }
    ee_astar_sub_sector_links_free(inLinks);
}

// Copies the fill's sub-sector ids into the id grid and links every pair of neighbouring sub-sectors; a diagonal
// link carries the two corner sub-sectors when both are open (Rust: ee_astar sub_sector_links.rs walks the grid and
// lists the LinkSubSectors calls; the id grid is filled first, the walk reads the fill's grid)
void EEAIContinentManager::LinkSubSectorGrid(EEAIPathSectorMachine& inMachine, EEMap* inMap)
{
    ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::SubSectorIds, true, this->GetHeight() * this->GetWidth());
    long theWidth = inMachine.GetWidth();
    long theHeight = inMachine.GetHeight();
    for (long x = 0; x < theWidth; x++) {
        for (long y = 0; y < theHeight; y++)
            this->mRecord.sub_sector_ids[this->GetWidth() * x + y] = inMachine.mSubSectorIDs[inMachine.GetHeight() * x + y];
    }
    EEAISubSectorGridView theView = { this, &inMachine };
    EeSubSectorGridCallbacks theCallbacks = { &theView, EEAISubSectorIDAt, EEAISubSectorPresent };
    this->ApplySubSectorLinks(ee_astar_sub_sector_links(0, (int32_t)theWidth, 0, (int32_t)theHeight, &theCallbacks));
}

// Creates one rectangle area per scenario area with a non-empty rectangle; the list is sized counting only anchored
// areas, so an anchorless one is stored past its end
void EEAIContinentManager::CreateScenarioAreas(EEMap* inMap)
{
    std::vector<U2DPoint<long> > theTiles;
    U2DRectangle<long> theRect;
    EEScenarioManager* theManager = gWorld.GetScenarioManager();
    if (!theManager)
        return;
    long i;
    std::vector<EeScenarioArea> theInputs(theManager->mAreas.size());
    for (i = theManager->mAreas.size() - 1; i >= 0; i--) {
        EESMArea* theSource = theManager->mAreas[i];
        theInputs[i].set = theSource && !theSource->IsUnset();
        if (!theInputs[i].set)
            continue;
        theInputs[i].top = theSource->GetRect().GetTop();
        theInputs[i].bottom = theSource->GetRect().GetBottom();
        theInputs[i].left = theSource->GetRect().GetLeft();
        theInputs[i].right = theSource->GetRect().GetRight();
        theInputs[i].height = theSource->GetRect().GetHeight();
        theInputs[i].width = theSource->GetRect().GetWidth();
    }
    std::vector<EeScenarioAreaPlan> thePlan(theInputs.size());
    size_t theCount = ee_astar_scenario_area_plan(theInputs.empty() ? NULL : &theInputs[0], theInputs.size(),
                                                  thePlan.empty() ? NULL : &thePlan[0]);
    if (theCount == 0)
        return;
    this->ResizeTable(EeAStarCmTable::RectangleAreas, 0);   // theCount + 1 entries, every one NULL
    this->ResizeTable(EeAStarCmTable::RectangleAreas, theCount + 1);
    for (size_t k = 0; k < theCount; k++) {
        const EeScenarioAreaPlan& theEntry = thePlan[k];
        EEAIRectangleArea* theArea = new EEAIRectangleArea;
        theArea->SetNumLandTiles(0);
        theArea->SetNumWaterTiles(0);
        theArea->SetNumSpaceTiles(0);
        theArea->SetID(theEntry.id);
        theArea->SetSerial(++this->mRecord.last_serial);
        theArea->SetAreaType(kAIAreaRectangleArea);
        theArea->SetScenarioAreaID(theManager->mAreas[theEntry.index]->mID);
        theRect.mTop = theEntry.top;
        theRect.mBottom = theEntry.bottom;
        theRect.mLeft = theEntry.left;
        theRect.mRight = theEntry.right;
        theArea->SetRect(theRect);
        theTiles.clear();
        for (long x = theRect.mLeft; x <= theRect.mRight; x++) {
            for (long y = theRect.mTop; y <= theRect.mBottom; y++)
                theTiles.push_back(U2DPoint<long>(x, y));
        }
        theArea->Create(theTiles);
        this->AddRectangleArea(theArea);
    }
}

// After a partial rebuild: writes the new sub-sectors into the id grid, then re-links the sub-sectors of every tile
// of the sectors in inSectors (grown by one tile on each side) (Rust: ee_astar sub_sector_links.rs walks each
// sector's tiles through the manager's lookup and lists the LinkSubSectors calls; the C++ also wrote each tile's id
// back into the id grid it had just read it from, when the grid exists: a no-op, dropped)
void EEAIContinentManager::RelinkSubSectors(std::map<unsigned short, std::vector<EETileCoordinate> >& inSectors,
    std::vector<EEAIPathSubSector*>& inSubSectors)
{
    std::vector<U2DPoint<long> > theTiles;
    long i;
    for (i = inSubSectors.size() - 1; i >= 0; i--) {
        EEAIPathSubSector* theSubSector = *(inSubSectors.begin() + i);
        theSubSector->mTiles.BuildPointList(theTiles, 1);
        if (!this->mRecord.sub_sector_ids)
            continue;
        for (long j = theTiles.size() - 1; j >= 0; j--) {
            unsigned short theID = theSubSector->mID;
            this->mRecord.sub_sector_ids[theTiles[j].mX * this->mRecord.width + (theTiles.begin() + j)->mY] = theID;
        }
    }
    EEAISubSectorGridView theView = { this, NULL };
    EeSubSectorGridCallbacks theCallbacks = { &theView, EEAISubSectorIDAt, EEAISubSectorPresent };
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
        long y = theSector->mBounds.mTop - 1;
        if (y < 1)
            y = 0;
        this->ApplySubSectorLinks(ee_astar_sub_sector_links((int32_t)x, (int32_t)theEndX, (int32_t)y, (int32_t)theEndY, &theCallbacks));
    }
}

// A tile as ee_astar repair.rs reads it: the map tile, its terrain and its path area (GetPathAreaAt: pure reads)
static EEAIPathArea* EEAIAreaTileAt(EEAIContinentManager& inManager, EEMap* inMap, long inX, long inY, EeAreaTile& outTile)
{
    EETile* theTile = inMap->GetTile(inX, inY);
    EEAIPathArea* theArea = NULL;
    memset(&outTile, 0, sizeof(outTile));
    outTile.tile = theTile != NULL;
    if (theTile) {
        outTile.terrain = (int32_t)theTile->mTerrainType;
        theArea = inManager.GetPathAreaAt(EETileLocation(inX, inY));
    }
    outTile.area = theArea != NULL;
    if (theArea) {
        outTile.id = theArea->GetID();
        outTile.continent = theArea->GetContinentID();
        outTile.kind = (int32_t)theArea->GetKind();
        outTile.unit_id = (int32_t)theArea->GetUnitID();
        outTile.size = (int32_t)theArea->GetNumTiles();
        outTile.land_mass = theArea->GetLandMassID();
        outTile.water_body = theArea->GetWaterBodyID();
        outTile.space_region = theArea->GetSpaceRegionID();
        outTile.surface_region = theArea->GetSurfaceRegionID();
    }
    return theArea;
}

// The eight neighbours of inTile in the C++'s order (x - 1, x, x + 1; y - 1 before y + 1) and their areas
static void EEAIAreaNeighbours(EEAIContinentManager& inManager, EEMap* inMap, const EETileLocation& inTile,
                               EeAreaTile* outTiles, EEAIPathArea** outAreas)
{
    static const long kOffsets[8][2] = { { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 0 }, { 1, 1 } };
    for (long i = 0; i < 8; i++)
        outAreas[i] = EEAIAreaTileAt(inManager, inMap, inTile.mX + kOffsets[i][0], inTile.mY + kOffsets[i][1], outTiles[i]);
}

// Returns the inKind region id (land mass, water body, space or surface region) of a fitting path area next to
// inTile, 0 if none; land mass skips water, cliff..forest and map-edge areas unless of kind inAreaKind (Rust: ee_astar
// repair.rs)
unsigned short EEAIContinentManager::GetAdjacentAreaID(long inKind, const EETileLocation& inTile, long inAreaKind)
{
    unsigned short theResult = 0;
    EEMap* theMap = gWorld.mMap;
    if (theMap) {
        // Rust: ee_astar repair.rs over the tile and its eight neighbours
        EeAreaTile theCentre;
        EeAreaTile theAround[8];
        EEAIPathArea* theAreas[8];
        EEAIAreaTileAt(*this, theMap, inTile.mX, inTile.mY, theCentre);
        EEAIAreaNeighbours(*this, theMap, inTile, theAround, theAreas);
        theResult = ee_astar_adjacent_area_id((int32_t)inKind, (int32_t)inAreaKind, &theCentre, theAround);
    }
    return theResult;
}

// Lists the continents adjacent to continent inID, itself included; empty for an unknown id
void EEAIContinentManager::GetConnectedIDs(std::vector<unsigned short>& outIDs, unsigned short inID)
{
    EEContinentQueryCapture q(kQueryConnectedIDs);   // TEST ONLY
    q.I(inID);
    [&]() {
    outIDs.clear();
    if (!(inID < this->mRecord.flags_width))
        return;
    outIDs.resize(this->mRecord.flags_width);   // Rust: ee_astar queries.rs over the matrix row
    outIDs.resize(ee_astar_adjacent_ids(this->mRecord.tile_flags, this->mRecord.flags_width, inID, true, outIDs.data()));
    }();
    q.Shorts(outIDs);
}

// Lists the continents adjacent to continent inID, itself excluded; empty for an unknown id
void EEAIContinentManager::GetAdjacentIDs(std::vector<unsigned short>& outIDs, unsigned short inID)
{
    EEContinentQueryCapture q(kQueryAdjacentIDs);   // TEST ONLY
    q.I(inID);
    [&]() {
    outIDs.clear();
    if (!(inID < this->mRecord.flags_width))
        return;
    outIDs.resize(this->mRecord.flags_width);   // Rust: ee_astar queries.rs over the matrix row
    outIDs.resize(ee_astar_adjacent_ids(this->mRecord.tile_flags, this->mRecord.flags_width, inID, false, outIDs.data()));
    }();
    q.Shorts(outIDs);
}

// Returns whether any tile of inTiles is a shore tile
bool EEAIContinentManager::HasShoreTile(const std::vector<U2DPoint<long> >& inTiles) const
{
    EEContinentQueryCapture q(kQueryHasShoreTile);   // TEST ONLY
    q.Points(inTiles);
    bool theQ = [&]() -> bool {
    bool theResult = false;
    std::vector<U2DPoint<long> >::const_iterator theIt = inTiles.begin();
    while (theIt != inTiles.end()) {
        theResult = ee_astar_cm_is_shore(&this->mRecord, theIt->mX, theIt->mY);
        ++theIt;
        if (theResult)
            break;
    }
    return theResult;
    }();
    q.I(theQ);
    return theQ;
}

// Returns whether two units stand on the same continent
bool EEAIContinentManager::InSameContinent(EEUnit* inFirst, EEUnit* inSecond)
{
    EEContinentQueryCapture q(kQueryInSameContinent);   // TEST ONLY
    q.Unit(inFirst); q.Unit(inSecond);
    bool theQ = [&]() -> bool {
    return this->GetContinentID(inFirst->GetTileX(), inFirst->GetTileY()) == this->GetContinentID(inSecond->GetTileX(), inSecond->GetTileY());
    }();
    q.I(theQ);
    return theQ;
}

// Returns the map tile's AI value (shore or wood value) at (inX, inY), 0 off the map
long EEAIContinentManager::GetTileFlag(long inX, long inY)
{
    EETile* theTile = gWorld.mMap->GetTile(inX, inY);
    return theTile ? theTile->mAIValue : 0;
}

// Returns whether every tile of inTiles has the same inKind region id (true for an empty list)
bool EEAIContinentManager::InSameArea(const std::vector<EETileCoordinate>& inTiles, long inKind)
{
    EEContinentQueryCapture q(kQueryInSameArea);   // TEST ONLY
    q.Points(inTiles); q.I(inKind);
    bool theQ = [&]() -> bool {
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
    }();
    q.I(theQ);
    return theQ;
}

// Returns whether inUnit can reach inTarget's tile (CanGetTo)
bool EEAIContinentManager::CanGetToUnit(EEUnit* inUnit, EEUnit* inTarget)
{
    return this->CanGetTo(inUnit, inTarget->GetTileX(), inTarget->GetTileY());
}

// Returns whether inUnit can reach tile (inX, inY): same continent unless air-blocked, else by its complex unit:
// planet ships always, aircraft by planet on space maps, ships by water body or space region
bool EEAIContinentManager::CanGetTo(EEUnit* inUnit, long inX, long inY)
{
    EEContinentQueryCapture q(kQueryCanGetTo);   // TEST ONLY
    q.Unit(inUnit); q.I(inX); q.I(inY);
    bool theQ = [&]() -> bool {
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    bool theResult = false;
    std::pair<long, int> theTile(inUnit->mTileX, inUnit->mTileY);
    if ((unsigned long)theTile.first < this->mRecord.width && (unsigned long)theTile.second < this->mRecord.height &&
        (unsigned long)inX < this->mRecord.width && (unsigned long)inY < this->mRecord.height) {
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
    }();
    q.I(theQ);
    return theQ;
}

// Returns the choke point at inUnit's tile, NULL if none
EEAIChokePoint* EEAIContinentManager::GetChokePointAt(EEUnit* inUnit)
{
    EETileLocation theTile(inUnit->mTileX, inUnit->mTileY);
    return this->GetChokePointAt(theTile);
}

// Returns the choke point of inTile's land continent that contains inTile, NULL if none
EEAIChokePoint* EEAIContinentManager::GetChokePointAt(const EETileLocation& inTile)
{
    EEContinentQueryCapture q(kQueryChokePointAt);   // TEST ONLY
    q.Tile(inTile);
    EEAIChokePoint* theQ = [&]() -> EEAIChokePoint* {
    EEAIContinent* theContinent = this->GetContinentAt(inTile);
    EEAIChokePoint* theChokePoint = NULL;
    if (theContinent && theContinent->GetCategory() == kContinentCategoryLand) {
        const std::vector<long>& theIDs = theContinent->GetChokePoints();
        EEAIHandleView<EEAIChokePoint> theChokePoints = this->GetChokePoints();
        for (long i = theIDs.size() - 1; i >= 0; i--) {
            theChokePoint = theChokePoints[theIDs[i]];
            if (theChokePoint && theChokePoint->Contains(inTile))
                break;
            theChokePoint = NULL;
        }
    }
    return theChokePoint;
    }();
    q.Area(theQ);
    return theQ;
}

// Appends the choke points of continent inID (every choke point for id 0) to outChokePoints, cleared first if inClear
void EEAIContinentManager::GetChokePoints(std::vector<EEAIChokePoint*>& outChokePoints, unsigned short inID, bool inClear)
{
    EEContinentQueryCapture q(kQueryChokePoints);   // TEST ONLY
    q.I(inID); q.I(inClear); q.I((long)outChokePoints.size());
    [&]() {
    long i;
    EEAIChokePoint* theChokePoint;
    const EEAIContinent* theContinent = this->GetContinentByID(inID);
    if (inClear)
        outChokePoints.clear();
    if (theContinent) {
        for (i = theContinent->GetChokePoints().size() - 1; i >= 0; i--) {
            theChokePoint = this->GetChokePoints()[theContinent->GetChokePoints()[i]];
            if (theChokePoint)
                outChokePoints.push_back(theChokePoint);
        }
    } else if (inID == 0) {
        for (i = this->GetChokePoints().size() - 1; i >= 0; i--) {
            if (this->GetChokePoints()[i])
                outChokePoints.push_back(this->GetChokePoints()[i]);
        }
    }
    }();
    q.Areas(outChokePoints);
}

// Returns in outPoint the center of continent inID's hull, (0, 0) for an unknown id
void EEAIContinentManager::GetContinentCenter(unsigned short inID, U2DPoint<long>& outPoint)
{
    EEContinentQueryCapture q(kQueryContinentCenter);   // TEST ONLY
    q.I(inID);
    [&]() {
    EEAIHandleView<EEAIContinent> theContinents = this->GetContinents();
    EEAIContinent* theContinent = theContinents[inID];
    outPoint.mX = 0;
    outPoint.mY = 0;
    if (theContinent)
        outPoint = theContinent->GetHullCenter();
    }();
    q.Tile(outPoint);
}

// Lists the tiles of continent inID in outTiles (untouched for an unknown id)
void EEAIContinentManager::GetContinentTiles(unsigned short inID, std::vector<U2DPoint<long> >& outTiles) const
{
    EEContinentQueryCapture q(kQueryContinentTiles);   // TEST ONLY
    q.I(inID); q.I((long)outTiles.size());
    [&]() {
    EEAIContinent* theContinent = this->GetContinents()[inID];
    if (theContinent)
        theContinent->GetTileSet().BuildPointList(outTiles, 1);
    }();
    q.Points(outTiles);
}

// Returns the continent at inUnit's tile, NULL off the grid
EEAIContinent* EEAIContinentManager::GetContinentAt(EEUnit* inUnit)
{
    if (inUnit->mTileX < this->mRecord.width && inUnit->mTileY < this->mRecord.height) {
        return this->GetContinentAt(inUnit->mTileX, inUnit->mTileY);
    }
    return NULL;
}

// Returns the id of the continent containing (inX, inY), searched through its path sector, 0 if none
unsigned short EEAIContinentManager::FindContinentID(long inX, long inY) const
{
    EEAIContinent* theContinent = this->FindContinent(inX, inY);
    unsigned short theID;
    theID = theContinent ? theContinent->GetID() : 0;
    return theID;
}

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
            if (theSector->mNeighbors[i] && theSector->mNeighbors[i]->Contains(thePoint)) {
                theResult = theSector->mNeighbors[i];
                break;
            }
        }
    }
    return theResult;
}

// Returns the continent id at inUnit's tile, 0 off the grid
unsigned short EEAIContinentManager::GetContinentID(EEUnit* inUnit)
{
    unsigned short theID;
    if (inUnit->mTileX < this->mRecord.width && inUnit->mTileY < this->mRecord.height)
        theID = this->GetContinentID(inUnit->mTileX, inUnit->mTileY);
    else
        theID = 0;
    return theID;
}

// Returns the forest at inUnit's tile, NULL if none
EEAIForest* EEAIContinentManager::GetForestAt(EEUnit* inUnit)
{
    EETileLocation theTile(inUnit->mTileX, inUnit->mTileY);
    return this->GetForestAt(theTile);
}

// Returns the forest of inTile's continent that contains inTile, NULL if none
EEAIForest* EEAIContinentManager::GetForestAt(const EETileLocation& inTile)
{
    EEContinentQueryCapture q(kQueryForestAt);   // TEST ONLY
    q.Tile(inTile);
    EEAIForest* theQ = [&]() -> EEAIForest* {
    EEAIContinent* theContinent = this->GetContinentAt(inTile.GetX(), inTile.GetY());
    EEAIForest* theForest = NULL;
    if (theContinent) {
        const std::vector<unsigned short>& theIDs = theContinent->GetForestIDs();
        EEAIHandleView<EEAIForest> theForests = this->GetForests();
        for (long i = theIDs.size() - 1; i >= 0; i--) {
            theForest = theForests[theIDs[i]];
            if (theForest && theForest->Contains(inTile))
                break;
            theForest = NULL;
        }
    }
    return theForest;
    }();
    q.Area(theQ);
    return theQ;
}

// Returns the hill of inTile's continent that contains inTile, NULL if none
EEAIHill* EEAIContinentManager::GetHillAt(const EETileLocation& inTile)
{
    EEContinentQueryCapture q(kQueryHillAt);   // TEST ONLY
    q.Tile(inTile);
    EEAIHill* theQ = [&]() -> EEAIHill* {
    EEAIContinent* theContinent = this->GetContinentAt(inTile.GetX(), inTile.GetY());
    EEAIHill* theHill = NULL;
    if (theContinent) {
        const std::vector<unsigned short>& theIDs = theContinent->GetHillIDs();
        EEAIHandleView<EEAIHill> theHills = this->GetHills();
        for (long i = theIDs.size() - 1; i >= 0; i--) {
            theHill = theHills[theIDs[i]];
            if (theHill && theHill->Contains(inTile))
                break;
            theHill = NULL;
        }
    }
    return theHill;
    }();
    q.Area(theQ);
    return theQ;
}

// Returns whether two continents are connected for inUnit's passable terrain (AreConnected; flags 0 without a unit)
bool EEAIContinentManager::AreConnectedForUnit(EEUnit* inUnit, EEAIContinent* inFirst, EEAIContinent* inSecond)
{
    EEContinentQueryCapture q(kQueryConnectedForUnit);   // TEST ONLY
    q.Unit(inUnit); q.Area(inFirst); q.Area(inSecond);
    bool theQ = [&]() -> bool {
    long theFlags = 0;
    if (inUnit)
        theFlags = inUnit->GetUnitClass()->GetPassableFlags(inUnit);
    return this->AreConnected(inFirst, inSecond, theFlags);
    }();
    q.I(theQ);
    return theQ;
}

// Returns whether two continents are the same or share a non-zero chain id of the chain kind the passable-terrain
// flags inFlags select (false for flags 0)
bool EEAIContinentManager::AreConnected(EEAIContinent* inFirst, EEAIContinent* inSecond, long inFlags)
{
    EEContinentQueryCapture q(kQueryConnected);   // TEST ONLY
    q.Area(inFirst); q.Area(inSecond); q.I(inFlags);
    bool theQ = [&]() -> bool {
    bool theResult = inFirst == inSecond && inFirst;
    if (!theResult && inFirst && inSecond) {
        // the chain the crossing flags need (Rust: ee_astar queries.rs; the cliff + space and water + space cases
        // compare the chains named so, as before)
        long theChain = ee_astar_connected_chain((int32_t)inFlags);
        if (theChain >= 0)
            theResult = inFirst->GetCount(theChain) != 0 && inFirst->GetCount(theChain) == inSecond->GetCount(theChain);
    }
    return theResult;
    }();
    q.I(theQ);
    return theQ;
}

// Returns the id of the continent of category inKind (any for none) with the most tiles, 0 if there is none
long EEAIContinentManager::GetLargestContinent(long inKind)
{
    EEContinentQueryCapture q(kQueryLargestContinent);   // TEST ONLY
    q.I(inKind);
    long theQ = [&]() -> long {
    unsigned short theResult = 0;
    std::vector<EeContinentSize> theSizes(this->GetContinents().size());   // Rust: ee_astar queries.rs
    for (size_t i = 0; i < theSizes.size(); i++) {
        EEAIContinent* theContinent = this->GetContinents()[i];
        theSizes[i].present = theContinent != NULL;
        theSizes[i].category = theContinent ? theContinent->GetCategory() : 0;
        theSizes[i].size = theContinent ? (int32_t)theContinent->GetNumTiles() : 0;
    }
    long theBest = ee_astar_largest_continent(theSizes.data(), theSizes.size(), (int8_t)(char)inKind);
    if (theBest != -1)
        theResult = this->GetContinents()[theBest]->GetID();
    return theResult;
    }();
    q.I(theQ);
    return theQ;
}

// Returns a free sub-sector id (last freed first) or the next new one
unsigned short EEAIContinentManager::NewSubSectorID()
{
    return ee_astar_cm_new_sub_sector_id(&this->mRecord);
}

// Returns a free path area id (last freed first) or the next new one
unsigned short EEAIContinentManager::NewPathAreaID()
{
    return ee_astar_cm_new_path_area_id(&this->mRecord);
}

// Returns the path area at inUnit's tile
EEAIPathArea* EEAIContinentManager::GetPathAreaAt(EEUnit* inUnit)
{
    EETileLocation theTile(inUnit->mTileX, inUnit->mTileY);
    return this->GetPathAreaAt(theTile);
}

// Returns the path area at inTile: its sub-sector's area when that is current, else found through the path sector
// (through the continent while repairing); NULL off the map
EEAIPathArea* EEAIContinentManager::GetPathAreaAt(const EETileLocation& inTile)
{
    EEContinentQueryCapture q(kQueryPathAreaAt);   // TEST ONLY
    q.Tile(inTile);
    EEAIPathArea* theQ = [&]() -> EEAIPathArea* {
    EEAIPathArea* theResult = NULL;
    if (gWorld.mMap && gWorld.mMap->GetTileAtLocation(inTile)) {
        EEAIPathSubSector* theSubSector = this->GetPathSubSectorAt(inTile.GetX(), inTile.GetY());
        if (theSubSector && !theSubSector->mIsStale) {
            theResult = this->GetPathArea(theSubSector->mPathAreaID);
            if (theResult)
                return theResult;
        }
        if (this->mRecord.state != 2) {
            EEAIPathSector* theSector = this->GetPathSector(inTile);
            if (theSector)
                theResult = theSector->GetPathAreaAt(inTile);
        } else {
            EEAIContinent* theContinent = this->GetContinentAt(inTile.GetX(), inTile.GetY());
            theResult = theContinent->GetPathAreaAt(inTile);
        }
    }
    return theResult;
    }();
    q.Area(theQ);
    return theQ;
}

// Returns the inKind region id (water body, land mass, space or surface region) at inUnit's tile
unsigned short EEAIContinentManager::GetContinent(EEUnit* inUnit, long inKind)
{
    EETileCoordinate theTile;
    theTile.mX = inUnit->mTileX;
    theTile.mY = inUnit->mTileY;
    return this->GetContinent(theTile, inKind);
}

// Returns the inKind region id (water body, land mass, space or surface region) of inTile's path area, 0 if none
unsigned short EEAIContinentManager::GetContinent(const EETileCoordinate& inTile, long inKind)
{
    EEContinentQueryCapture q(kQueryContinent);   // TEST ONLY
    q.Tile(inTile); q.I(inKind);
    unsigned short theQ = [&]() -> unsigned short {
    unsigned short theResult = 0;
    EEAIPathArea* theArea = this->GetPathAreaAt(inTile);
    if (theArea) {
        if (inKind == kAIPathWaterBody)
            theResult = theArea->GetWaterBodyID();
        else if (inKind == kAIPathLandMass)
            theResult = theArea->GetLandMassID();
        else if (inKind == kAIPathSpaceRegion)
            theResult = theArea->GetSpaceRegionID();
        else if (inKind == kAIPathNonSpace)
            theResult = theArea->GetSurfaceRegionID();
    }
    return theResult;
    }();
    q.I(theQ);
    return theQ;
}

// Returns the path sector at inUnit's tile
EEAIPathSector* EEAIContinentManager::GetPathSector(EEUnit* inUnit)
{
    EETileCoordinate theTile;
    theTile.mX = inUnit->mTileX;
    theTile.mY = inUnit->mTileY;
    return this->GetPathSector(theTile);
}

// Returns the path sector containing inTile, NULL off the grid
EEAIPathSector* EEAIContinentManager::GetPathSector(const EETileCoordinate& inTile) const
{
    ptrdiff_t theIndex = ee_astar_cm_sector_index(&this->mRecord, inTile.mX, inTile.mY);
    return theIndex < 0 ? NULL : this->GetPathSectors()[theIndex];
}

// Returns the path sub-sector at inUnit's tile
EEAIPathSubSector* EEAIContinentManager::GetPathSubSectorAt(EEUnit* inUnit)
{
    EEContinentQueryCapture q(kQuerySubSectorAt);   // TEST ONLY
    q.Unit(inUnit);
    EEAIPathSubSector* theQ = [&]() -> EEAIPathSubSector* {
    return this->GetPathSubSectorAt(inUnit->GetTileX(), inUnit->GetTileY());
    }();
    q.I(theQ != NULL ? (long)theQ->mID : -1);
    return theQ;
}

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

// Returns the path sub-sector id at inUnit's tile, 0 off the grid
unsigned short EEAIContinentManager::GetPathSubSectorID(EEUnit* inUnit)
{
    unsigned short theID;
    if (inUnit->mTileX < this->mRecord.width && inUnit->mTileY < this->mRecord.height)
        theID = this->GetPathSubSectorID(inUnit->mTileX, inUnit->mTileY);
    else
        theID = 0;
    return theID;
}

// Returns the id of the sub-sector containing (inX, inY), found by search, 0 if none
unsigned short EEAIContinentManager::FindPathSubSectorID(long inX, long inY)
{
    EEAIPathSubSector* theSubSector = this->FindPathSubSector(inX, inY);
    unsigned short theID;
    theID = theSubSector ? theSubSector->mID : 0;
    return theID;
}

// Returns a random area of type inKind whose hull center is on continent inContinent (any for 0), NULL if none
EEAIArea* EEAIContinentManager::GetRandomArea(long inKind, URandom& inRandom, unsigned short inContinent)
{
    EEContinentQueryCapture q(kQueryRandomArea);   // TEST ONLY
    q.I(inKind); q.I(inContinent);
    EEAIArea* theQ = [&]() -> EEAIArea* {
    EEAIArea* theResult = NULL;
    std::vector<EEAIArea*> theAreas;
    EEAIListView<EeAStarCmArea> theTable = this->GetAreas();
    for (const EeAStarCmArea* theIt = theTable.begin(); theIt != theTable.end(); ++theIt) {
        EEAIArea* theArea = (EEAIArea*)(uintptr_t)theIt->area;
        if (theArea && theArea->GetAreaType() == inKind &&
            (inContinent == 0 ||
             gContinentManager.GetContinentID(theArea->GetHullCenter().GetX(), theArea->GetHullCenter().GetY()) == inContinent))
            theAreas.push_back(theArea);
    }
    if (theAreas.size() != 0)
        theResult = theAreas[EEGetRandomInt(0, theAreas.size() - 1, inRandom)];
    return theResult;
    }();
    q.Area(theQ);
    return theQ;
}

// Returns a random continent of category inKind (any but the map edge for none), NULL if none
EEAIArea* EEAIContinentManager::GetRandomContinent(unsigned char inKind, URandom& inRandom)
{
    EEContinentQueryCapture q(kQueryRandomContinent);   // TEST ONLY
    q.I(inKind);
    EEAIArea* theQ = [&]() -> EEAIArea* {
    std::vector<EEAIArea*> theAreas;
    EEAIArea* theResult = NULL;
    for (long i = this->GetContinents().size() - 1; i >= 0; i--) {
        if (this->GetContinents()[i] &&
            (inKind == kContinentCategoryNone && this->GetContinents()[i]->GetCategory() != kContinentCategoryMapEdge ||
             this->GetContinents()[i]->GetCategory() == inKind)) {
            EEAIArea* theArea = this->GetContinents()[i];
            theAreas.push_back(theArea);
        }
    }
    if (theAreas.size() != 0)
        theResult = theAreas[EEGetRandomInt(0, theAreas.size() - 1, inRandom)];
    return theResult;
    }();
    q.Area(theQ);
    return theQ;
}

// GetTilesNextTo's lookup: the manager's GetContinentID
static unsigned short EEAIContinentIDAt(void* inManager, int32_t inX, int32_t inY)
{
    return static_cast<const EEAIContinentManager*>(inManager)->GetContinentID(inX, inY);
}

// Returns whether tile (inX, inY) is on continent inID or on an adjacent continent with a neighbour tile on inID;
// outTiles gets those neighbour tiles (Rust: ee_astar manager.rs, the ring of EEGetSquareRingTiles walked from its
// last tile)
bool EEAIContinentManager::GetTilesNextTo(long inX, long inY, std::vector<U2DPoint<long> >& outTiles, unsigned short inID)
{
    EePoint theTiles[8];
    size_t theCount = 0;
    bool theResult = ee_astar_tiles_next_to((int32_t)inX, (int32_t)inY, inID, (uint32_t)this->mRecord.width,
                                            (uint32_t)this->mRecord.height, this->mRecord.tile_flags,
                                            this->mRecord.tile_flags ? (size_t)(this->mRecord.flags_width * this->mRecord.flags_width) : 0,
                                            (int32_t)this->mRecord.flags_width, EEAIContinentIDAt, this, theTiles, &theCount);
    outTiles.clear();
    for (size_t k = 0; k < theCount; k++)
        outTiles.push_back(U2DPoint<long>(theTiles[k].x, theTiles[k].y));
    if (EESimCaptureRecord::Active()) {   // TEST ONLY: CNTN, the call's inputs and outputs
        EESimCaptureRecord theRecord(kContinentCaptureNextTo);
        theRecord.I32((int)inX);
        theRecord.I32((int)inY);
        theRecord.U32(inID);
        theRecord.U32(theResult);
        theRecord.U32((unsigned int)outTiles.size());
        for (size_t k = 0; k < outTiles.size(); k++) {
            theRecord.I32((int)outTiles[k].mX);
            theRecord.I32((int)outTiles[k].mY);
        }
        theRecord.Commit();
    }
    return theResult;
}

// Returns whether tile (inX, inY) is on or next to continent inID
bool EEAIContinentManager::IsNextToArea(long inX, long inY, unsigned short inID)
{
    EEContinentQueryCapture q(kQueryNextToArea);   // TEST ONLY
    q.I(inX); q.I(inY); q.I(inID);
    bool theQ = [&]() -> bool {
    std::vector<U2DPoint<long> > theTiles;
    return this->GetTilesNextTo(inX, inY, theTiles, inID);
    }();
    q.I(theQ);
    return theQ;
}

// Computes the AI tile values (grid, and the map tile clamped to +-120); inWater: water and space tiles get 10 per
// shore or ice-shore neighbour (negated, minus 10, on a shore tile); else land tiles get 10 per wood unit in 3x3
// (Rust: ee_astar tile_values.rs over every tile's terrain, the shore lists and, for the land pass, every tile's
// GetResourceWeight; this stores the values it computed)
void EEAIContinentManager::UpdateTileValues(bool inWater)
{
    EEMap* theMap = gWorld.GetMap();
    long theSize = this->mRecord.width;
    long theCount = theSize * theSize;
    long x;
    long y;
    size_t i;
    std::vector<int32_t> theTerrain(theCount);
    std::vector<int32_t> theWeights(inWater ? 0 : theCount);
    std::vector<int32_t> theValues(theCount);
    std::vector<int8_t> theBytes(theCount);
    for (x = theSize - 1; x >= 0; x--) {
        for (y = theSize - 1; y >= 0; y--) {
            EETile* theTile = theMap->GetTile(x, y);   // NULL outside the playable map: -1 in the grid
            theTerrain[x * theSize + y] = theTile ? (int32_t)theTile->GetTerrainType() : -1;
            if (!inWater)
                theWeights[x * theSize + y] = (int32_t)this->GetResourceWeight(x, y);
        }
    }
    std::vector<EePoint> theLists[2];
    for (int theList = 0; theList < 2; theList++) {
        size_t theCount;
        const EeHullPoint* theTiles = ee_astar_cm_shore(&this->mRecord, (EeAStarCmShore)theList, &theCount);
        theLists[theList].resize(theCount);
        for (i = 0; i < theCount; i++) {
            theLists[theList][i].x = (int32_t)theTiles[i].x;
            theLists[theList][i].y = (int32_t)theTiles[i].y;
        }
    }
    std::vector<EePoint>& theShore = theLists[0];
    std::vector<EePoint>& theIce = theLists[1];
    ee_astar_update_tile_values((int32_t)theSize, theTerrain.data(), inWater, theShore.data(), theShore.size(),
                                theIce.data(), theIce.size(), theWeights.data(), theValues.data(), theBytes.data());
    for (x = theSize - 1; x >= 0; x--) {
        for (y = theSize - 1; y >= 0; y--) {
            long theValue = theValues[x * theSize + y];
            if (theValue == 0)
                continue;
            this->mRecord.tile_values[this->mRecord.width * x + y] = theValue;
            theMap->GetTile(x, y)->mAIValue = (char)theBytes[x * theSize + y];
        }
    }
    ee_astar_cm_set_shore(&this->mRecord, EeAStarCmShore::IceShore, NULL, 0);
}

// Marks the manager as clearing, resets it, frees its grids and rebuilds the continents and hills of inMap
void EEAIContinentManager::Clear(EEMap* inMap)
{
    this->mRecord.clearing = true;
    this->Reset();
    this->FreeGrids();
    this->CreateContinents(inMap);
    this->CreateHills(inMap);
}

// Re-files tile inTile after its AI kind changed: it joins the largest same-kind neighbouring area (merging the
// others into it), its single neighbour's area or a new path area; sectors and sub-sectors follow the new id
// (Rust: ee_astar repair.rs chooses from the eight neighbours; the areas, their tiles and the sectors stay here)
void EEAIContinentManager::RepairPathArea(const U2DPoint<long>& inTile)
{
    EEContinentQueryCapture q(kQueryRepairPathArea);   // TEST ONLY
    q.Tile(inTile);
    [&]() {
    std::vector<EEAIPathArea*> theMerged;
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theOtherTiles;
    std::vector<unsigned short> theSectorIDs;
    long i;
    long j;
    EEAIPathSector* theSector;
    EEAIPathSubSector* theSubSector = this->GetPathSubSectorAt(inTile.GetX(), inTile.GetY());
    EEAIPathArea* theBest = NULL;
    EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(inTile);
    EETile* theTile = gWorld.GetMap()->GetTile(inTile.GetX(), inTile.GetY());
    EEAIPathArea* theSingle = NULL;
    long theKind = EEAIGetTileKind(inTile);
    unsigned short theContinent = gContinentManager.GetContinentID(inTile.GetX(), inTile.GetY());
    unsigned short theNewID = 0;
    if (theArea && theArea->GetKind() != theKind) {
        if (theSubSector)
            theSubSector->mIsStale = true;
        if (theTile) {
            // the choice (Rust: ee_astar repair.rs over the eight neighbours): the largest same-kind area to join and
            // the others to merge into it (std::sort/std::unique by id, walked from the highest id down), the one
            // neighbour's area, or a new area
            EeAreaTile theAround[8];
            EEAIPathArea* theAreas[8];
            EEAIAreaNeighbours(*this, gWorld.GetMap(), inTile, theAround, theAreas);
            long theWallID = theKind == kAITileUnitObstacle ? EEAIGetTileWallID(inTile) : -1;   // the neighbours' wall test
            EeRepairPlan thePlan = ee_astar_repair_plan((int32_t)theKind, theArea->GetContinentID(), theContinent,
                                                        (int32_t)theWallID, theAround);
            if (thePlan.action == 1) {
                theBest = theAreas[thePlan.target];
                theBest->GetTileSet().BuildPointList(theTiles, 1);
                theBest->GetTileSet().Insert(inTile);
                theTiles.push_back(inTile);
                for (i = 0; i < thePlan.merged_count; i++) {
                    EEAIPathArea* theOther = theAreas[thePlan.merged[i]];
                    theMerged.push_back(theOther);
                    theOther->GetTileSet().BuildPointList(theOtherTiles, 1);
                    theTiles.insert(theTiles.end(), theOtherTiles.begin(), theOtherTiles.end());
                }
                theBest->Create(theTiles);
                theNewID = theBest->GetID();
            } else if (thePlan.action == 2) {
                theSingle = theAreas[thePlan.target];
                theSingle->GetTileSet().BuildPointList(theTiles, 1);
                theTiles.push_back(inTile);
                theSingle->Create(theTiles);
                theNewID = theSingle->GetID();
            } else {
                theSingle = new EEAIPathArea;
                theSingle->SetContinentID(gContinentManager.GetContinentID(inTile.GetX(), inTile.GetY()));
                theSingle->SetKind(theKind);
                theSingle->SetID(this->NewPathAreaID());
                theSingle->SetSerial(++this->mRecord.last_serial);
                if (theKind == kAITileUnitObstacle)
                    theSingle->SetUnitID(EEAIGetTileWallID(inTile));
                unsigned short theID;
                if (theTile->GetTerrainType() == kTerrainSpace) {
                    theID = this->GetAdjacentAreaID(kAIPathSpaceRegion, inTile, theKind);
                    if (theID == 0)
                        theID = (unsigned short)this->mRecord.next_space_region++;
                    theSingle->SetSpaceRegionID(theID);
                } else {
                    theID = this->GetAdjacentAreaID(kAIPathNonSpace, inTile, theKind);
                    if (theID == 0)
                        theID = (unsigned short)this->mRecord.next_surface_region++;
                    theSingle->SetSurfaceRegionID(theID);
                }
                if (theTile->GetTerrainType() == kTerrainDeepWater) {
                    theID = this->GetAdjacentAreaID(kAIPathWaterBody, inTile, theKind);
                    if (theID == 0)
                        theID = (unsigned short)this->mRecord.next_water_body++;
                    theSingle->SetWaterBodyID(theID);
                } else if (theKind != kAITileForest) {
                    theID = this->GetAdjacentAreaID(kAIPathLandMass, inTile, theKind);
                    if (theID == 0)
                        theID = (unsigned short)this->mRecord.next_land_mass++;
                    theSingle->SetLandMassID(theID);
                    if ((theTile->GetTerrainType() == kTerrainDeepWater || theTile->GetTerrainType() == kTerrainShallowWater) &&
                        (theKind == kAITileWater || (theKind > kAITileMapEdge && theKind <= kAITileUnitObstacle))) {
                        theID = this->GetAdjacentAreaID(kAIPathWaterBody, inTile, theKind);
                        if (theID == 0)
                            theID = (unsigned short)this->mRecord.next_water_body++;
                        theSingle->SetWaterBodyID(theID);
                    }
                }
                theTiles.push_back(inTile);
                theSingle->Create(theTiles);
                theID = theSingle->GetID();
                if (theID >= this->GetPathAreas().size())
                    ee_astar_cm_table_push(&this->mRecord, EeAStarCmTable::PathAreas, (uint64_t)(uintptr_t)theSingle);
                else
                    this->SetTable(EeAStarCmTable::PathAreas, theID, theSingle);
                theNewID = theSingle->GetID();
                this->AddArea(theSingle);
                EEAIContinent* theContinentArea = gContinentManager.GetContinentAt(inTile.GetX(), inTile.GetY());
                if (theContinentArea)
                    theContinentArea->AddPathAreaID(theSingle->GetID());
            }
        }
        // the merged areas hand their sectors and sub-sectors to theNewID
        for (i = theMerged.size() - 1; i >= 0; i--) {
            EEAIPathArea* theOld = theMerged[i];
            EEAIContinent* theOldContinent = gContinentManager.GetContinentByID(theOld->GetContinentID());
            if (theOldContinent)
                theOldContinent->RemovePathAreaID(theOld->GetID());
            const std::vector<unsigned short>& theOldIDs = theOld->GetSectorIDs();  // const iterators: the original calls insert 0x26cf3
            theSectorIDs.insert(theSectorIDs.end(), theOldIDs.begin(), theOldIDs.end());
            for (j = theOld->GetSectorIDs().size() - 1; j >= 0; j--) {
                theSector = this->GetPathSectorByID(theOld->GetSectorIDs()[j]);
                if (theSector) {
                    theSector->RemovePathAreaID(theOld->GetID());
                    theSector->AddPathAreaID(theNewID);
                }
            }
            for (j = this->GetPathSubSectors().size() - 1; j >= 0; j--) {
                if (this->GetPathSubSectors()[j] && this->GetPathSubSectors()[j]->mPathAreaID == theOld->GetID())
                    this->GetPathSubSectorsAt(j)->mPathAreaID = theNewID;
            }
            this->RemoveArea(theOld);
            this->SetTable(EeAStarCmTable::PathAreas, theOld->GetID(), NULL);
            this->FreePathAreaID(theOld->GetID());
            delete theOld;
        }
        // inTile leaves its old area; an emptied area is deleted
        theArea->RemoveTile(inTile);
        if (theArea->GetNumTiles() == 0) {
            EEAIContinent* theAreaContinent = gContinentManager.GetContinentsAt(theArea->GetContinentID());
            if (theAreaContinent)
                theAreaContinent->RemovePathAreaID(theArea->GetID());
            for (j = theArea->GetSectorIDs().size() - 1; j >= 0; j--) {
                theSector = this->GetPathSectorsAt(theArea->GetSectorIDs()[j]);
                if (theSector)
                    theSector->RemovePathAreaID(theArea->GetID());
            }
            for (j = this->GetPathSubSectors().size() - 1; j >= 0; j--) {
                if (this->GetPathSubSectors()[j] && this->GetPathSubSectors()[j]->mPathAreaID == theArea->GetID())
                    this->GetPathSubSectorsAt(j)->mPathAreaID = theNewID;
            }
            this->RemoveArea(theArea);
            this->SetTable(EeAStarCmTable::PathAreas, theArea->GetID(), NULL);
            this->FreePathAreaID(theArea->GetID());
            delete theArea;
        }
        // the new area joins inTile's sector and the merged areas' sectors
        theArea = this->GetPathArea(theNewID);
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
                theSector = this->GetPathSectorsAt(theSectorIDs[j]);
                if (theSector) {
                    theSector->AddPathAreaID(theNewID);
                    theArea->AddSectorID(theSector->mID);
                }
            }
        }
    }
    }();
    q.State();
}

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
    std::map<unsigned short, std::vector<EETileCoordinate> >::iterator theIt;
    for (n = 0; n < (long)ee_astar_cm_sector_tiles_count(&this->mRecord) && n < 1; n++) {   // the first pending sector
        unsigned short theID;
        size_t theCount;
        const EeHullPoint* thePending = ee_astar_cm_sector_tiles(&this->mRecord, n, &theID, &theCount);
        std::vector<EETileCoordinate>& theSectorTiles = theTiles[theID];
        for (size_t k = 0; k < theCount; k++) {
            EETileCoordinate theTile;
            theTile.mX = thePending[k].x;
            theTile.mY = thePending[k].y;
            theSectorTiles.push_back(theTile);
        }
    }
    if (theTiles.begin() == theTiles.end()) {
        theBounds.mTop = 0;
        theBounds.mLeft = 0;
        theBounds.mBottom = theMap->mSize;
        theBounds.mRight = theMap->mSize;
    } else {
        for (theIt = theTiles.begin(); theIt != theTiles.end(); ++theIt) {
            theSector = this->GetPathSectors()[theIt->first];
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
        theSector = this->GetPathSectors()[theIt->first];
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
    if (this->GetPathSubSectors().size() <= this->mRecord.last_sub_sector_id)
        this->ResizeTable(EeAStarCmTable::PathSubSectors, this->mRecord.last_sub_sector_id + 1);
    for (std::map<unsigned short, EEAIPathSubSector*>::const_iterator theNew = theMachine.mGoal.mSubSectors.begin();
         theNew != theMachine.mGoal.mSubSectors.end(); ++theNew) {
        theSubSector = theNew->second;
        theSubSector->UpdateLocation();
        this->SetTable(EeAStarCmTable::PathSubSectors, theNew->first, theSubSector);
        theSubSector->mIsStale = true;
        theSubSectors.push_back(theSubSector);
    }
    this->RelinkSubSectors(theTiles, theSubSectors);
    this->mRecord.state = 2;
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
                theSector->AddPathAreaID(theArea->GetID());
                theArea->AddSectorID(theSector->mID);
            }
            theSubSectors[i]->mPathAreaID = theArea->GetID();
        }
        theSubSectors[i]->mIsStale = false;
    }
    this->mRecord.state = 0;
    this->ClearChecksum();
    EEAIContinentCapture(kContinentCaptureRebuild);   // TEST ONLY
    return ee_astar_cm_sector_tiles_count(&this->mRecord) == 0;
}

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
    EEAIContinentCapture(kContinentCaptureBuild);   // TEST ONLY: with the work grids still there
    ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::TileValues, false, 0);
    if (inMap->mL195610 < 0x4b && gWorld.mNumPlayers < 5 && gWorld.mPopulationLimit < 1000) {
        ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::ContinentIds, false, 0);
    }
    ee_astar_cm_set_grid(&this->mRecord, EeAStarCmGrid::SubSectorIds, false, 0);
    this->ClearChecksum();
    this->mRecord.clearing = false;
}

// Deletes each choke point beside a map-edge continent when all but at most one of its joined areas also touch
// a map-edge continent
// PruneChokePoints' questions to the objects (ee_astar manager.rs PruneHost)
static EEAIContinentManager* EEAIPruneManager(void* inContext)
{
    return static_cast<EEAIContinentManager*>(inContext);
}

static bool EEAIPruneChokeExists(void* inContext, size_t inChoke)
{
    return EEAIPruneManager(inContext)->GetChokePointsAt(inChoke) != NULL;
}

static size_t EEAIPruneAreaCount(void* inContext, size_t inChoke)
{
    return EEAIPruneManager(inContext)->GetChokePointsAt(inChoke)->GetAreaSerials().size();
}

// the choke point's inK-th area: -2 none, the continent's index when it is one of the continents, else -1
static int32_t EEAIPruneArea(void* inContext, size_t inChoke, size_t inK)
{
    EEAIContinentManager* theManager = EEAIPruneManager(inContext);
    EEAIArea* theArea = theManager->GetArea(theManager->GetChokePoints()[inChoke]->GetAreaSerials()[inK]);
    if (!theArea)
        return -2;
    for (long i = theManager->GetContinents().size() - 1; i >= 0; i--) {
        if (theManager->GetContinents()[i] == theArea)
            return (int32_t)i;
    }
    return -1;
}

// whether map edge continent inEdge's closest points to the choke point (inK -1) or its inK-th area are at most
// one tile apart
static bool EEAIPruneNear(void* inContext, size_t inEdge, size_t inChoke, ptrdiff_t inK)
{
    EEAIContinentManager* theManager = EEAIPruneManager(inContext);
    EEAIChokePoint* theChokePoint = theManager->GetChokePoints()[inChoke];
    EEAIArea* theTarget = inK < 0 ? theChokePoint : theManager->GetArea(theChokePoint->GetAreaSerials()[inK]);
    U2DPoint<long> theThis;
    U2DPoint<long> theOther;
    theManager->GetContinents()[inEdge]->GetClosestPoints(theTarget, theThis, theOther);
    return theThis.DistanceToSquared(theOther) <= 1;
}

static void EEAIPruneRemove(void* inContext, size_t inChoke)
{
    EEAIContinentManager* theManager = EEAIPruneManager(inContext);
    EEAIChokePoint* theChokePoint = theManager->GetChokePoints()[inChoke];
    theManager->RemoveArea(theChokePoint);
    theManager->SetTable(EeAStarCmTable::ChokePoints, inChoke, NULL);
    delete theChokePoint;
}

// Deletes the choke points that a map edge continent touches when their areas touch the map edge continents too
// (Rust: ee_astar manager.rs prune_choke_points; the closest points it asks stay the areas' C++ geometry)
void EEAIContinentManager::PruneChokePoints()
{
    std::vector<int32_t> theCategories(this->GetContinents().size());
    for (size_t i = 0; i < this->GetContinents().size(); i++)
        theCategories[i] = this->GetContinents()[i] ? (int32_t)this->GetContinents()[i]->GetCategory() : -1;
    EePruneCallbacks theCallbacks;
    theCallbacks.ctx = this;
    theCallbacks.choke_exists = EEAIPruneChokeExists;
    theCallbacks.area_count = EEAIPruneAreaCount;
    theCallbacks.area = EEAIPruneArea;
    theCallbacks.is_near = EEAIPruneNear;
    theCallbacks.remove = EEAIPruneRemove;
    ee_astar_prune_choke_points(theCategories.empty() ? NULL : &theCategories[0], theCategories.size(),
                                this->GetChokePoints().size(), &theCallbacks);
}

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
        this->SetTable(EeAStarCmTable::PathSubSectors, theSubSector->mID, NULL);
        theSubSector->mTiles.BuildPointList(theTiles, 1);
        if (this->mRecord.sub_sector_ids) {
            for (j = theTiles.size() - 1; j >= 0; j--)
                this->mRecord.sub_sector_ids[this->mRecord.width * theTiles[j].mX + (theTiles.begin() + j)->mY] = 0;
        }
        theID = theSubSector->mID;
        theIDs.push_back(theID);
    }
    inSector->DeleteSubSectors();
    if (theIDs.size() > 0)
        ee_astar_cm_add_free_ids(&this->mRecord, EeAStarCmFree::SubSectors, &theIDs[0], theIDs.size(), false);
}

// Removes inArea from the area map; returns whether it was there
bool EEAIContinentManager::RemoveArea(EEAIArea* inArea)
{
    long theSerial = inArea->GetSerial();
    return ee_astar_cm_remove_area(&this->mRecord, (unsigned long)theSerial);
}

// Drops inSector's pending rebuild tiles
void EEAIContinentManager::RemoveSectorTiles(EEAIPathSector* inSector)
{
    ee_astar_cm_remove_sector_tiles(&this->mRecord, inSector->mID);
}

// NEAR: the original calls RTSMap::GetTile out of line for the first five tiles (inline budget)
// When no wood with resources is left on inUnit's tile: removes the tile from its forest, lowers the AI values of
// its 3x3 block by 10 (not below 0) and deletes the forest once it is empty
void EEAIContinentManager::RemoveForestTile(EEUnit* inUnit)
{
    EEContinentQueryCapture q(kQueryRemoveForestTile);   // TEST ONLY
    q.Unit(inUnit); if (inUnit != NULL) q.Tile(U2DPoint<long>(inUnit->mTileX, inUnit->mTileY));
    [&]() {
    std::pair<long, long> theTile(inUnit->GetTileX(), inUnit->GetTileY());
    EEMap* theMap = gWorld.GetMap();
    EETile* theMapTile = theMap->GetTile(theTile.first, theTile.second);
    EEAIForest* theForest = this->GetForestAt(inUnit);
    bool theEmpty = true;
    if (theMapTile && EEAIHasResourceUnit(theMapTile)) {
        for (RTSTileUnitNode* theNode = theMapTile->GetUnits(); theNode; theNode = theNode->GetNext()) {
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
    if (theOther && theOther->GetAIValue() > 0) {
        theValue = theOther->GetAIValue() - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first - 1, theTile.second - 1);
    if (theOther && theOther->GetAIValue() > 0) {
        theValue = theOther->GetAIValue() - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first - 1, theTile.second);
    if (theOther && theOther->GetAIValue() > 0) {
        theValue = theOther->GetAIValue() - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first - 1, theTile.second + 1);
    if (theOther && theOther->GetAIValue() > 0) {
        theValue = theOther->GetAIValue() - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first, theTile.second - 1);
    if (theOther && theOther->GetAIValue() > 0) {
        theValue = theOther->GetAIValue() - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first, theTile.second + 1);
    if (theOther && theOther->GetAIValue() > 0) {
        theValue = theOther->GetAIValue() - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first + 1, theTile.second - 1);
    if (theOther && theOther->GetAIValue() > 0) {
        theValue = theOther->GetAIValue() - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first + 1, theTile.second);
    if (theOther && theOther->GetAIValue() > 0) {
        theValue = theOther->GetAIValue() - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    theOther = theMap->GetTile(theTile.first + 1, theTile.second + 1);
    if (theOther && theOther->GetAIValue() > 0) {
        theValue = theOther->GetAIValue() - 10;
        if (theValue < 0)
            theValue = 0;
        theOther->mAIValue = (signed char)theValue;
    }
    EEAIHandleView<EEAIContinent> theContinents = this->GetContinents();
    EEAIContinent* theContinent;
    if (!(theForest->GetNumTiles() < 1) || (theContinent = theContinents[theForest->GetContinentID()]) == NULL)
        return;
    theContinent->RemoveForestID(theForest->GetID());
    this->SetTable(EeAStarCmTable::Forests, theForest->GetID(), NULL);
    this->RemoveArea(theForest);
    delete theForest;
    }();
    q.State();
}

// Queues the footprint tiles of a finished blocking unit or gate for a sector rebuild and tells the path manager
// (inNow: at once); skipped while clearing or before regions exist
void EEAIContinentManager::UpdateWallUnit(EEComplexUnit* inUnit, bool inNow)
{
    EEContinentQueryCapture q(kQueryUpdateWallUnit);   // TEST ONLY
    q.Unit(inUnit); q.I(inNow);
    [&]() {
    std::vector<EETileCoordinate> theTiles;
    if (this->GetAreas().size() <= 0 || this->mRecord.clearing || !gWorld.mMap)
        return;
    if (inUnit->IsFoundation() || (!inUnit->GetUnitClass()->mBlocksTile && inUnit->GetUnitClass()->mCategory != kUnitCategoryGate))
        return;
    EEGetFootprintTiles(inUnit, theTiles);
    this->AddSectorTiles(theTiles);
    gPathManager.SetRegionUpdates(true, inNow);
    }();
    q.State();
}

// Adds inArea to the area map under its serial
void EEAIContinentManager::AddArea(EEAIArea* inArea)
{
    ee_astar_cm_add_area(&this->mRecord, inArea->GetSerial(), (uint64_t)(uintptr_t)inArea);
}

// Returns the area with serial inSerial, NULL if none
EEAIArea* EEAIContinentManager::GetArea(unsigned long inSerial) const
{
    return (EEAIArea*)(uintptr_t)ee_astar_cm_area(&this->mRecord, inSerial);
}

// Returns the continent with id inID
EEAIContinent* EEAIContinentManager::GetContinentByID(unsigned short inID) const
{
    return this->GetContinents()[inID];
}

// Creates an empty forest: no resources, no continent, id 0
// out of line (EEAIAStarPathAreaGoal::CreateAreas 0x17510 news it)
EEAIForest::EEAIForest()
    : mResourceKind(0), mNumResources1(0), mNumResources2(0), mNumResources3(0), mL78(0), mNextRenewal(0), mContinentID(0), mID(0)
{
}

// 0x563bc8 (.bss) = gUnitClassTable[0x147] (0x563bc8 - 0x5636ac = 0x51c): the unit class RegrowTrees creates
// (its vtable +0x30 is EEUnitClass::CreateUnit(x, y, player, bool, creator)); no store to it is known in the exe

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
