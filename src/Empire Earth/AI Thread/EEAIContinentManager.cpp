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
    this->SetTable(EeAStarCmTable::PathSectors, inSector->GetID(), inSector);
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
void EEAIRegisterContinentState();   // EEAIContinentSave.cpp: the records' places and the host (cm_io.rs)

EEAIContinentManager::EEAIContinentManager()
{
    ee_astar_cm_init(&this->mRecord);
    EEAIRegisterContinentState();
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
        ee_astar_cm_add_sector_tile(&this->mRecord, theSector->GetID(), U2DHullPointOf(ioTiles[i]));
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
    ee_astar_cm_apply_continent_grid(&this->mRecord, theGrid);   // Rust: cm_build.rs (frees the grid)
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
        EEAIContinent* theContinent = theSubSector ? this->GetContinentsAt(theSubSector->GetContinentID()) : NULL;
        if (theContinent)
            theSubSector->GetSector()->AddNeighbor(theContinent);
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
            thePathSector->SetID(theID);
            theID++;
            // the sector's rectangle, clipped to the map (Rust: ee_astar sectors.rs sector_rect, GetSectorEnd 0x263cb
            // included)
            EeSectorRect theRect = ee_astar_sector_rect((int32_t)x, (int32_t)y, (int32_t)this->mRecord.sector_size,
                                                       (int32_t)inMap->mSize);
            theBounds.mTop = theRect.top;
            theBounds.mLeft = theRect.left;
            theBounds.mBottom = theRect.bottom;
            theBounds.mRight = theRect.right;
            thePathSector->SetCenter(U2DPoint<long>(theRect.center_x, theRect.center_y));
            thePathSector->SetBounds(theBounds);
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
            theMachine.mGoal.mSectorBounds = thePathSector->GetBounds();
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
        EEAIPathArea* theArea = this->GetPathArea(theSubSector->GetPathAreaID());
        if (theArea) {
            EETileCoordinate theTile;
            theTile = theSubSector->GetLocation();
            EEAIPathSector* thePathSector = this->GetPathSector(theTile);
            if (thePathSector) {
                theArea->AddSectorID(thePathSector->GetID());
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

// Makes the LinkSubSectors calls of a walk, in its order (Rust: cm_links.rs, over the sub sectors' records)
void EEAIContinentManager::ApplySubSectorLinks(EeSubSectorLinks* inLinks)
{
    ee_astar_cm_apply_links(&this->mRecord, inLinks);
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
            unsigned short theID = theSubSector->GetID();
            this->mRecord.sub_sector_ids[theTiles[j].mX * this->mRecord.width + (theTiles.begin() + j)->mY] = theID;
        }
    }
    EEAISubSectorGridView theView = { this, NULL };
    EeSubSectorGridCallbacks theCallbacks = { &theView, EEAISubSectorIDAt, EEAISubSectorPresent };
    for (std::map<unsigned short, std::vector<EETileCoordinate> >::iterator theIt = inSectors.begin(); theIt != inSectors.end(); ++theIt) {
        EEAIPathSector* theSector = this->GetPathSectorByID(theIt->first);
        long theSize = gWorld.mMap->mSize;
        long theEndX = theSector->GetBounds().mRight + 2;
        if (theSize <= theEndX)
            theEndX = theSize;
        long theEndY = theSector->GetBounds().mBottom + 2;
        if (theSize <= theEndY)
            theEndY = theSize;
        long x = theSector->GetBounds().mLeft - 1;
        if (x < 1)
            x = 0;
        long y = theSector->GetBounds().mTop - 1;
        if (y < 1)
            y = 0;
        this->ApplySubSectorLinks(ee_astar_sub_sector_links((int32_t)x, (int32_t)theEndX, (int32_t)y, (int32_t)theEndY, &theCallbacks));
    }
}

// Returns the inKind region id of a fitting path area next to inTile, 0 if none (Rust: cm_repair.rs)
unsigned short EEAIContinentManager::GetAdjacentAreaID(long inKind, const EETileLocation& inTile, long inAreaKind)
{
    return ee_astar_cm_adjacent_area_id(&this->mRecord, (int32_t)inKind, inTile.mX, inTile.mY, (int32_t)inAreaKind);   // Rust
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
    std::vector<U2DPoint<long> > theTiles(inTiles.begin(), inTiles.end());
    bool theQ = ee_astar_cm_in_same_area(&this->mRecord, U2DHullPoints(theTiles), theTiles.size(), inKind);   // Rust: cm_query.rs
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
    EEAIChokePoint* theQ = (EEAIChokePoint*)(uintptr_t)ee_astar_cm_area_at(&this->mRecord, 0, inTile.mX, inTile.mY);
    q.Area(theQ);
    return theQ;
}

// Appends the choke points of continent inID (every choke point for id 0) to outChokePoints, cleared first if inClear
void EEAIContinentManager::GetChokePoints(std::vector<EEAIChokePoint*>& outChokePoints, unsigned short inID, bool inClear)
{
    EEContinentQueryCapture q(kQueryChokePoints);   // TEST ONLY
    q.I(inID); q.I(inClear); q.I((long)outChokePoints.size());
    if (inClear)
        outChokePoints.clear();
    std::vector<uint64_t> theFound(this->GetChokePoints().size() + 1);   // Rust: cm_query.rs
    theFound.resize(ee_astar_cm_choke_points(&this->mRecord, inID, &theFound[0], theFound.size()));
    if (theFound.size() > this->GetChokePoints().size() + 1)
        ee_astar_cm_choke_points(&this->mRecord, inID, &theFound[0], theFound.size());
    for (size_t i = 0; i < theFound.size(); i++)
        outChokePoints.push_back((EEAIChokePoint*)(uintptr_t)theFound[i]);
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
    return (EEAIContinent*)(uintptr_t)ee_astar_cm_find_continent(&this->mRecord, inX, inY);   // Rust: cm_io.rs
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
    EEAIForest* theQ = (EEAIForest*)(uintptr_t)ee_astar_cm_area_at(&this->mRecord, 1, inTile.GetX(), inTile.GetY());
    q.Area(theQ);
    return theQ;
}

// Returns the hill of inTile's continent that contains inTile, NULL if none
EEAIHill* EEAIContinentManager::GetHillAt(const EETileLocation& inTile)
{
    EEContinentQueryCapture q(kQueryHillAt);   // TEST ONLY
    q.Tile(inTile);
    EEAIHill* theQ = (EEAIHill*)(uintptr_t)ee_astar_cm_area_at(&this->mRecord, 2, inTile.GetX(), inTile.GetY());
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
    bool theQ = ee_astar_cm_are_connected((uint64_t)(uintptr_t)inFirst, (uint64_t)(uintptr_t)inSecond, (int32_t)inFlags);
    q.I(theQ);
    return theQ;
}

// Returns the id of the continent of category inKind (any for none) with the most tiles, 0 if there is none
long EEAIContinentManager::GetLargestContinent(long inKind)
{
    EEContinentQueryCapture q(kQueryLargestContinent);   // TEST ONLY
    q.I(inKind);
    long theQ = ee_astar_cm_largest_continent(&this->mRecord, (int8_t)(char)inKind);   // Rust: cm_query.rs
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
    EEAIPathArea* theQ = (EEAIPathArea*)(uintptr_t)ee_astar_cm_area_at(&this->mRecord, 3, inTile.GetX(), inTile.GetY());
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
    unsigned short theQ = ee_astar_cm_region_id_at(&this->mRecord, inTile.mX, inTile.mY, inKind);   // Rust: cm_query.rs
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
    q.I(theQ != NULL ? (long)theQ->GetID() : -1);
    return theQ;
}

// Finds the sub-sector containing (inX, inY) among its path sector's sub-sectors, NULL if none
EEAIPathSubSector* EEAIContinentManager::FindPathSubSector(long inX, long inY)
{
    return (EEAIPathSubSector*)(uintptr_t)ee_astar_cm_find_sub_sector(&this->mRecord, inX, inY);   // Rust: cm_io.rs
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
    theID = theSubSector ? theSubSector->GetID() : 0;
    return theID;
}

// Returns a random area of type inKind whose hull center is on continent inContinent (any for 0), NULL if none
EEAIArea* EEAIContinentManager::GetRandomArea(long inKind, URandom& inRandom, unsigned short inContinent)
{
    EEContinentQueryCapture q(kQueryRandomArea);   // TEST ONLY
    q.I(inKind); q.I(inContinent);
    EEAIArea* theQ = (EEAIArea*)(uintptr_t)ee_astar_cm_random_area(&this->mRecord, false, inKind, &inRandom, inContinent);
    q.Area(theQ);
    return theQ;
}

// Returns a random continent of category inKind (any but the map edge for none), NULL if none
EEAIArea* EEAIContinentManager::GetRandomContinent(unsigned char inKind, URandom& inRandom)
{
    EEContinentQueryCapture q(kQueryRandomContinent);   // TEST ONLY
    q.I(inKind);
    EEAIArea* theQ = (EEAIArea*)(uintptr_t)ee_astar_cm_random_area(&this->mRecord, true, inKind, &inRandom, 0);
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
    ee_astar_cm_update_tile_values(&this->mRecord, inWater);   // Rust: cm_build.rs
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
    ee_astar_cm_repair_path_area(&this->mRecord, inTile.GetX(), inTile.GetY());   // Rust: cm_repair.rs
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
            theBounds.Extend(theSector->GetBounds().GetTopLeft());
            theBounds.Extend(theSector->GetBounds().GetBottomRight());
        }
    }
    theMachine.mGoal.mTileBounds = theBounds;
    EEAIPathSectorMachine* theStorage = &theMachine;   // the storage's virtual through the machine (no null check)
    theStorage->Init(theMap->mSize, theMap->mSize);
    for (theIt = theTiles.begin(); theIt != theTiles.end(); ++theIt) {
        theSector = this->GetPathSectors()[theIt->first];
        for (x = theSector->GetBounds().mRight; x >= theSector->GetBounds().mLeft; x--) {
            for (y = theSector->GetBounds().mBottom; y >= theSector->GetBounds().mTop; y--) {
                if (theMachine.mSubSectorIDs[theMachine.mHeight * x + y] != 0 || !theMap->GetTile(x, y))
                    continue;
                theTile.mX = x;
                theTile.mY = y;
                theSector = this->GetPathSector(theTile);
                theMachine.mGoal.mSector = theSector;
                theMachine.mGoal.mSectorBounds = theSector->GetBounds();
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
        theSubSector->SetStale(true);
        theSubSectors.push_back(theSubSector);
    }
    this->RelinkSubSectors(theTiles, theSubSectors);
    this->mRecord.state = 2;
    for (i = theMachine.mGoal.mStaleTiles.size() - 1; i >= 0; i--)
        this->RepairPathArea(theMachine.mGoal.mStaleTiles[i]);
    for (i = theSubSectors.size() - 1; i >= 0; i--) {
        (*(theSubSectors.begin() + i))->SetStale(true);
        theArea = this->GetPathAreaAt((*(theSubSectors.begin() + i))->GetLocation());
        if (theArea) {
            theTile.mX = theSubSectors[i]->GetLocation().GetX();
            theTile.mY = (*(theSubSectors.begin() + i))->GetLocation().mY;
            theSector = this->GetPathSector(theTile);
            if (theSector) {
                theSector->AddPathAreaID(theArea->GetID());
                theArea->AddSectorID(theSector->GetID());
            }
            theSubSectors[i]->SetPathAreaID(theArea->GetID());
        }
        theSubSectors[i]->SetStale(false);
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
    ee_astar_cm_remove_sub_sectors(&this->mRecord, (uint64_t)(uintptr_t)inSector);   // Rust: cm_links.rs
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
    ee_astar_cm_remove_sector_tiles(&this->mRecord, inSector->GetID());
}

// When no wood with resources is left on inUnit's tile: removes the tile from its forest, lowers the AI values of
// its 3x3 block by 10 (not below 0) and deletes the forest once it is empty
void EEAIContinentManager::RemoveForestTile(EEUnit* inUnit)
{
    EEContinentQueryCapture q(kQueryRemoveForestTile);   // TEST ONLY
    q.Unit(inUnit); if (inUnit != NULL) q.Tile(U2DPoint<long>(inUnit->mTileX, inUnit->mTileY));
    // whether no wood with resources is left on the tile; the rest is Rust (cm_query.rs remove_forest_tile)
    EETile* theMapTile = gWorld.GetMap()->GetTile(inUnit->GetTileX(), inUnit->GetTileY());
    bool theEmpty = true;
    if (theMapTile && EEAIHasResourceUnit(theMapTile)) {
        for (RTSTileUnitNode* theNode = theMapTile->GetUnits(); theNode; theNode = theNode->GetNext()) {
            if (theNode->mUnit->mUnitClass->mResourceType == kResourceWood && theNode->mUnit->mResourceAmount > 0) {
                theEmpty = false;
                break;
            }
        }
    }
    ee_astar_cm_remove_forest_tile(&this->mRecord, inUnit->GetTileX(), inUnit->GetTileY(), theEmpty);
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

long EEAIHasResourceUnit(EETile* inTile);   // 0x29ced (EEAIAStarPathAreaGoal.cpp declares it the same way)

// Removes inPoint from the forest and, if it was there, lowers the count of that tile's wood resource kind;
// returns whether it was removed
bool EEAIForest::RemoveTile(const U2DPoint<long>& inPoint)
{
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inPoint);
    bool theResult = EEAIArea::RemoveTile(inPoint);
    if (theResult) {
        ee_astar_forest_remove_resource(this, EEAIHasResourceUnit(theTile));
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
