// C:\EEX\Empire Earth\Random Map\EERandomMap.cpp (__FILE__ at 0x28655a). The game's random map: the generator list
// (EERandomMap::Initialize over the "rmap" PIMs and Data\Random Map Scripts\*.rmv), and one EERandomMap per try:
// the generator PIM steps on the shared state (ee/EERandomMap.h, seen here as EERandomMapData), then the game-side
// passes that turn the state into the EEMap's tiles, terrain and units. Those passes are Rust (ee_rmap game.rs, R6
// data round 2) over the state's grid rows and the host below; the PIM steps, the generator list, the state's set-up
// and PlaceWall stay here. Header: ee/EERandomMapClass.h.
#include <windows.h>
#include <mbstring.h>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GE3DLine.h"
#include "lle/GEPhysicalModel.h"
#include "lle/PIMModule.h"
#include "lle/U2DPoint.h"
#include "lle/U2DFractal.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGameOptions.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EERules.h"
#include "rust/sim/rmap.h"
#include "rust/sim/units.h"
#include "ee/EERandomMapView.h"

// EETerrainTable::sCliffTable[inIndex] (its COMDAT 0x28aaf7 follows the unit's code; provisional name)
inline EECliffTerrainTable* EEGetCliffTerrainTable(unsigned long inIndex)
{
    return EETerrainTable::sCliffTable[inIndex];
}

// the generator list's keys (unguarded file-scope globals, 0x5655e0 / 0x5655d0)
const UString kUseMapGeneratorKey("UseMapGenerator");
const UString kStringTableIDKey("StringTableID");

FSFileSpec EERandomMap::sGeneratorDirectory;
std::vector<EERMGenerator> EERandomMap::sRandomMapGenerators;

// gSpaceTerrain: the terrain named "Space" (0x5655f0; provisional name)
unsigned long gSpaceTerrain;

// ---- the game passes in Rust (ee_rmap game.rs): the state's grids by their rows, its lists copied, and the host
// below for the map, the mesh, the tables and the players

// A wall line's tiles (the feature claims' and PlaceWall's): a point every tile along the line, truncated to tiles,
// the diagonal corners dropped (a line only), then the repeats
static void EERMWallLineTiles(const GE2DPoint& inStart, const GE2DPoint& inEnd, std::vector<EETileCoordinate>& outTiles)
{
    std::vector<GE2DPoint> thePoints;
    EETileCoordinate theTile;
    outTiles.clear();
    if (inStart != inEnd) {
        inStart.InterpolateTo(inEnd, 1.0f, thePoints);
        for (unsigned long i = 0; i < thePoints.size(); i++) {
            theTile.mX = (long)thePoints[i].GetX();
            theTile.mY = (long)thePoints[i].GetY();
            outTiles.push_back(theTile);
        }
        EEDropDiagonalCorners(outTiles);
    } else {
        theTile.mX = (long)inStart.GetX();
        theTile.mY = (long)inStart.GetY();
        outTiles.push_back(theTile);
    }
    EEDropRepeatedTiles(outTiles);
}

// What the host's callbacks reach: the map, the game's players, the random map (PlaceWall, the water points)
struct EERMGameContext {
    EEMap* mMap;
    std::vector<EEPlayer*>* mPlayers;
    EERandomMap* mRandomMap;
};

static GE2DPoint EERMToPoint(EeRmapPoint inPoint)
{
    return GE2DPoint(inPoint.x, inPoint.y);
}

static EEMap* EERMMap(void* inContext)
{
    return static_cast<EERMGameContext*>(inContext)->mMap;
}

static EEPlayer* EERMPlayerAt(void* inContext, uint32_t inIndex)
{
    return (*static_cast<EERMGameContext*>(inContext)->mPlayers)[inIndex];
}

static uint8_t EERMHostTile(void* inContext, uint32_t inX, uint32_t inY, EeRmapGameTile* outTile)
{
    EEMapTile* theTile = EERMMap(inContext)->GetTile(inX, inY);
    if (theTile == NULL)
        return 0;
    outTile->terrain_type = (int32_t)theTile->mTerrainType;
    outTile->height = theTile->GetHeight();
    return 1;
}

static uint8_t EERMHostIsEdgeTile(void* inContext, int32_t inX, int32_t inY)
{
    return EERMMap(inContext)->IsEdgeTile(inX, inY);
}

static void EERMHostPaintTerrain(void* inContext, int32_t inX, int32_t inY, int32_t inTexture)
{
    EERMMap(inContext)->PaintTerrain(inX, inY, inTexture);
}

static uint8_t EERMHostTerrain(void*, int32_t inTexture, EeRmapGameTerrainInfo* outInfo)
{
    EETerrainTable* theTable = EETerrainTable::Get(inTexture);
    if (theTable == NULL)
        return 0;
    outInfo->unit_count = (uint32_t)theTable->GetNumUnitClasses();
    outInfo->ambient_chance = (int32_t)theTable->mAmbientUnitChance;
    return 1;
}

static int32_t EERMHostTerrainUnitClass(void*, int32_t inTexture, uint32_t inIndex)
{
    return (int32_t)EETerrainTable::Get(inTexture)->GetUnitClass(inIndex);
}

static float EERMHostMeshZ(void* inContext, uint32_t inX, uint32_t inY)
{
    return EERMMap(inContext)->mTerrainMesh.GetZ(inX, inY);
}

static void EERMHostMeshSetZ(void* inContext, uint32_t inX, uint32_t inY, float inZ)
{
    EERMMap(inContext)->mTerrainMesh.SetZ(inX, inY, inZ);
}

static uint8_t EERMHostMeshValidPoint(void* inContext, uint32_t inX, uint32_t inY)
{
    return EERMMap(inContext)->mTerrainMesh.GetIsValidPoint(inX, inY);
}

static uint8_t EERMHostMeshValidTile(void* inContext, uint32_t inX, uint32_t inY)
{
    return EERMMap(inContext)->mTerrainMesh.GetIsValidTile(inX, inY);
}

static void EERMHostMeshWaterColorSet(void* inContext, uint32_t inX, uint32_t inY)
{
    EERMMap(inContext)->mTerrainMesh.SetWaterColorSet(inX, inY);
}

static void EERMHostMeshVertexNormal(void* inContext, uint32_t inX, uint32_t inY)
{
    EERMMap(inContext)->mTerrainMesh.ComputeVertexNormal(inX, inY);
}

static void EERMHostMeshSetIsWater(void* inContext, uint32_t inX, uint32_t inY, uint8_t inIsWater)
{
    EERMMap(inContext)->mTerrainMesh.SetIsWater(inX, inY, inIsWater != 0);
}

static void EERMHostMeshShorelines(void* inContext, uint32_t inX, uint32_t inY)
{
    EERMMap(inContext)->mTerrainMesh.ComputeShorelines(inX, inY);
}

static void EERMHostSetTileHeight(void* inContext, uint32_t inX, uint32_t inY, float inHeight)
{
    EERMMap(inContext)->GetTile(inX, inY)->mHeight = inHeight;
}

static uint32_t EERMHostUnitClassCount(void*)
{
    return (uint32_t)EEUnitClassCount();
}

static uint8_t EERMHostUnitClass(void*, uint32_t inClass, EeRmapGameClass* outClass)
{
    EEUnitClass* theClass = inClass < EEUnitClassCount() ? gUnitClassTable[inClass] : NULL;
    if (theClass == NULL)
        return 0;
    outClass->tile_size = (int32_t)theClass->GetTileSize();
    outClass->is_land_citizen = EEUnitClassOfKind<EE_CLASS_KIND_LAND_CITIZEN>(theClass) != NULL;
    outClass->is_wall = theClass->mIsWall;
    return 1;
}

static uint8_t EERMHostClassAllowsTerrain(void*, uint32_t inClass, int32_t inTerrainType)
{
    return gUnitClassTable[inClass]->mTerrainTypes[inTerrainType];
}

static int32_t EERMHostMaxStartingCitizens(void* inContext, uint32_t inPlayer)
{
    return (int32_t)EERMPlayerAt(inContext, inPlayer)->mMaxStartingCitizens;
}

// A start unit: a complex unit is built and paid for; a resource unit with an amount answers its resource type (the
// pass paints that resource's terrain under it)
static int32_t EERMHostCreateUnitAtPoint(void* inContext, uint32_t inOwner, uint32_t inClass, float inX, float inY,
                                         float inHeading)
{
    EEUnit* theUnit = gUnitClassTable[inClass]->CreateUnitAtPoint(EERMPlayerAt(inContext, inOwner), inX, inY,
                                                                  EERMMap(inContext), inHeading);
    if (theUnit == NULL)
        return -1;
    if (theUnit->mComplexUnit) {
        theUnit->mComplexUnit->SetBuilt(1, 0, 0);
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (theComplexUnit->mIsPaidFor != true)
            theComplexUnit->mIsPaidFor = true;
        return -1;
    }
    if (theUnit->mResourceType >= kResourceFood && theUnit->mResourceType < 7 && theUnit->mResourceAmount > 0)
        return (int32_t)theUnit->mResourceType;
    return -1;
}

static void EERMHostCreateUnitAtTile(void* inContext, uint32_t inOwner, uint32_t inClass, int32_t inX, int32_t inY,
                                     float inHeading)
{
    gUnitClassTable[inClass]->CreateUnitAtTile(EERMPlayerAt(inContext, inOwner), inX, inY, EERMMap(inContext), inHeading);
}

static void EERMHostPlaceWall(void* inContext, uint32_t inClass, EeRmapPoint inStart, EeRmapPoint inEnd,
                              uint32_t inPlayer)
{
    static_cast<EERMGameContext*>(inContext)->mRandomMap->PlaceWall(
        gUnitClassTable[inClass], EERMToPoint(inStart), EERMToPoint(inEnd), EERMPlayerAt(inContext, inPlayer));
}

static void EERMHostWallTiles(void*, EeRmapPoint inStart, EeRmapPoint inEnd, EeRmapGameTiles* ioSink)
{
    std::vector<EETileCoordinate> theTiles;
    EERMWallLineTiles(EERMToPoint(inStart), EERMToPoint(inEnd), theTiles);
    for (unsigned long i = 0; i < theTiles.size(); i++)
        ee_rmap_game_tiles_push(ioSink, (int32_t)theTiles[i].mX, (int32_t)theTiles[i].mY);
}

static void EERMHostPushWaterPoint(void* inContext, float inX, float inY)
{
    GE2DPoint thePoint;
    thePoint.SetX(inX);
    thePoint.SetY(inY);
    static_cast<EERMGameContext*>(inContext)->mRandomMap->mData.mWaterPoints.push_back(thePoint);
}

static EeRmapGameHost EERMHost(EERMGameContext* inContext)
{
    EeRmapGameHost theHost = {inContext, EERMHostTile, EERMHostIsEdgeTile, EERMHostPaintTerrain, EERMHostTerrain,
        EERMHostTerrainUnitClass, EERMHostMeshZ, EERMHostMeshSetZ, EERMHostMeshValidPoint, EERMHostMeshValidTile,
        EERMHostMeshWaterColorSet, EERMHostMeshVertexNormal, EERMHostMeshSetIsWater, EERMHostMeshShorelines,
        EERMHostSetTileHeight, EERMHostUnitClassCount, EERMHostUnitClass, EERMHostClassAllowsTerrain,
        EERMHostMaxStartingCitizens, EERMHostCreateUnitAtPoint, EERMHostCreateUnitAtTile, EERMHostPlaceWall,
        EERMHostWallTiles, EERMHostPushWaterPoint};
    return theHost;
}

// Runs one of the Rust passes over this map's state
void EERandomMap::RunGamePass(void (*inPass)(EeRmapGameState* ioState, EeRmapGameHost* inHost))
{
    EERMGameContext theContext = {this->mMap, this->mGamePlayers, this};
    EeRmapGameHost theHost = EERMHost(&theContext);
    EERMGameView theView(this->mData, this->mMap->GetF1955F4(), gSpaceTerrain);
    inPass(&theView.mState, &theHost);
    theView.StoreSeeds(this->mData);
}

// Runs the generator's steps in order (positions, land and water, heights, terrain, resources, trees, features) with
// load progress; returns the first step error, or 9 when the players end up cut off. The game's passes between them
// are Rust's (ee_rmap game.rs): the water points and the space fill (Fn284149), the smoothing, the heights onto the
// mesh (Fn288839), the painting with the ambient units, the feature claims, the start units and walls, the resource
// objects (Fn288426).
long EERandomMap::Generate()
{
    long theResult;
    if ((this->mData.mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) {
        this->mData.mNumTerrainBands = (unsigned long)(this->mData.mTerrainRegions[this->mData.mTerrainRegions.size() - 1].mTo + 1) / 100;
        this->mData.mTerrainBandOffset = this->mData.mRandomIntegers.Generate(0, this->mData.mNumTerrainBands);
    }
    gUsingCarbon = (this->mData.mMapCharacteristics & kRMMapMarsMap) == kRMMapMarsMap;
    theResult = this->mGeneratePlayerPositions(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.1f);
    theResult = this->mGenerateNeutralPositions(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.14f);
    theResult = this->mCreatePlayerLandWaterMap(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.18f);
    theResult = this->mCreateNeutralLandWaterMap(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.22f);
    this->mData.mWaterPoints.clear();
    this->RunGamePass(ee_rmap_game_water_points);
    theResult = this->mCreateFlatMap(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.26f);
    theResult = this->mCreateHeightMap(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.3f);
    theResult = this->mGenerateShallows(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.34f);
    theResult = this->FilterHeightMap();
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.38f);
    this->RunGamePass(ee_rmap_game_smooth_heights);
    this->mMap->SetCliffTexture(this->mData.mCliffTexture);
    this->RunGamePass(ee_rmap_game_apply_heights);
    if (!EERMArePlayersConnected(this->mData, this->mMap))
        return 9;
    unsigned long theStart = TMGetTime();
    this->RunGamePass(ee_rmap_game_paint_tiles);
    ESDebugOut("EERandomMap::PaintTiles Processing completed in %6.2f s\n", (TMGetTime() - theStart) * 0.001f);
    this->mMap->UpdateCliffs(0, this->mMap->mSize, 0, this->mMap->mSize);
    this->RunGamePass(ee_rmap_game_mark_features);
    theResult = this->mGenerateResources(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.4f + 0.02f);
    theResult = this->mCreateTrees(&this->mData);
    if (theResult != 0)
        return theResult;
    gUserInterface->SetLoadProgress(0.46f);
    this->RunGamePass(ee_rmap_game_place_features);
    this->RunGamePass(ee_rmap_game_place_objects);
    this->mMap->mMapCharacteristics = this->mData.mMapCharacteristics;
    return 0;
}

// Checks that every player's start area is dry, non-space land and that no two players stand closer than
// kRMMinimumPlayerDistance (Rust: ee_rmap game.rs)
bool EERMArePlayersConnected(EERandomMapData& inMap, EEMap* inGameMap)
{
    EERMGameContext theContext = {inGameMap, NULL, NULL};
    EeRmapGameHost theHost = EERMHost(&theContext);
    EERMGameView theView(inMap, inGameMap->GetF1955F4(), gSpaceTerrain);
    return ee_rmap_game_players_connected(&theView.mState, &theHost);
}

// Creates a random map for a generator PIM, the game options and the players, binding the generator's entry points
EERandomMap::EERandomMap(PIMModule* inModule, EEGameOptions* inOptions, std::vector<EEPlayer*>* inPlayers)
    : mGamePlayers(inPlayers), mOptions(inOptions)
{
    this->mModule = inModule;
    this->mCreateFlatMap = (EERMPIMFunction)inModule->GetEntry("EERMPIMCreateFlatMap");
    this->mCreateHeightMap = (EERMPIMFunction)inModule->GetEntry("EERMPIMCreateHeightMap");
    this->mCreateNeutralLandWaterMap = (EERMPIMFunction)inModule->GetEntry("EERMPIMCreateNeutralLandWaterMap");
    this->mCreatePlayerLandWaterMap = (EERMPIMFunction)inModule->GetEntry("EERMPIMCreatePlayerLandWaterMap");
    this->mCreateTrees = (EERMPIMFunction)inModule->GetEntry("EERMPIMCreateTrees");
    this->mFilterHeightMapPIM = (EERMPIMFunction)inModule->GetEntry("EERMPIMFilterHeightMap");
    this->mGenerateNeutralPositions = (EERMPIMFunction)inModule->GetEntry("EERMPIMGenerateNeutralPositions");
    this->mGeneratePlayerPositions = (EERMPIMFunction)inModule->GetEntry("EERMPIMGeneratePlayerPositions");
    this->mGenerateResources = (EERMPIMFunction)inModule->GetEntry("EERMPIMGenerateResources");
    this->mGenerateShallows = (EERMPIMFunction)inModule->GetEntry("EERMPIMGenerateShallows");
    this->mSetRandomMapValues = (EERMPIMSetValuesFunction)inModule->GetEntry("EERMPIMSetRandomMapValues");
}

// Destroys the random map (members only)
EERandomMap::~EERandomMap()
{
}

// Sets up and generates the map from a generator and seed, and returns the player's start point (the centre for 0)
void EERandomMap::CreateRandomMap(unsigned long inPlayer, unsigned long inGenerator, unsigned long inSeed,
                                  EEMap* inMap, GE3DPoint& outLocation)
{
    unsigned long theStart = TMGetTime();
    this->SetUp(inGenerator, inMap, inSeed);
    this->Generate();
    if (inPlayer > 0) {
        outLocation.SetX(this->mData.mPlayers[inPlayer - 1].mPosition.GetX() + 0.5f);
        outLocation.SetY(this->mData.mPlayers[inPlayer - 1].mPosition.GetY() + 0.5f);
        outLocation.SetZ(this->mMap->GetTile(UMFloorUnsigned(outLocation.GetX()), UMFloorUnsigned(outLocation.GetY()))->GetHeight());
    } else {
        outLocation.SetX((inMap->mSize + 1.0f) * 0.5f);
        outLocation.SetY((inMap->mSize + 1.0f) * 0.5f);
        outLocation.SetZ(this->mMap->GetTile(inMap->mSize / 2, inMap->mSize / 2)->GetHeight());
    }
    ESDebugOut("EERandomMap::CreateRandomMap processing completed in %6.2f s\n", (TMGetTime() - theStart) * 0.001f);
}

// Groups the players into teams: each player not yet placed starts a team with the later players it is not at war with
void EERandomMap::CreateTeams()
{
    std::vector<unsigned char> theAssigned;
    EERMTeam theTeam;
    this->mData.mTeams.resize(0);
    theAssigned.resize(this->mGamePlayers->size(), 0);
    std::fill(theAssigned.begin(), theAssigned.end(), (unsigned char)false);
    for (unsigned long i = 1; i < this->mGamePlayers->size(); i++) {
        if (theAssigned[i])
            continue;
        theTeam.mPlayers.clear();
        theTeam.mPositions.clear();
        theTeam.mPlayers.push_back(i);
        theTeam.mPositions.push_back(GE2DPoint());
        theAssigned[i] = true;
        for (unsigned long j = i + 1; j < this->mGamePlayers->size(); j++) {
            if (theAssigned[j])
                continue;
            unsigned long theID = (*this->mGamePlayers)[j]->GetPlayerID();
            if ((*this->mGamePlayers)[i]->mEnemies[theID] == 0) {
                theTeam.AddPlayer(j);
                theAssigned[j] = true;
            }
        }
        this->mData.mTeams.push_back(theTeam);
    }
}

// Adds a player to the team, with an unset position
void EERMTeam::AddPlayer(long inPlayer)
{
    this->mPlayers.push_back(inPlayer);
    this->mPositions.push_back(GE2DPoint());
}

// Marks which grid points are valid terrain-mesh points, logging the count
void EERandomMap::CreateValidMap()
{
    unsigned long theCount = 0;
    GETerrainMesh& theMesh = this->mMap->mTerrainMesh;
    for (unsigned long x = 0; x < this->mData.mGridSize + 1; x++) {
        for (unsigned long y = 0; y < this->mData.mGridSize + 1; y++) {
            this->mData.mValidTiles[x][y] = theMesh.GetIsValidPoint(x, y);
            if (this->mData.mValidTiles[x][y])
                theCount++;
        }
    }
    ESDebugOut("EERandomMap::CreateValidMap %ld valid tiles found\n", theCount);
}

// Builds a player's wall along a line: drops cliff and edge tiles, places shaped pieces on free dry tiles and joins
// the player's fortifications already on the line
void EERandomMap::PlaceWall(EEUnitClass* inClass, const GE2DPoint& inStart, const GE2DPoint& inEnd,
                            EEPlayer* inPlayer)
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<long> theCorners;
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EETileCoordinate> thePlaced;
    EETileCoordinate theTile;
    EERMWallLineTiles(inStart, inEnd, theTiles);
    for (unsigned long j = 0; j < theTiles.size(); j++) {
        long theX = theTiles[j].GetX();
        long theY = theTiles[j].mY;
        EEMapTile* theMapTile = this->mMap->GetTile(theX, theY);
        if (theMapTile == NULL || theMapTile->mTerrainType == kTerrainCliff || this->mMap->IsEdgeTile(theX, theY)) {
            theTiles.erase(theTiles.begin() + j);
            j--;
        }
    }
    EEPickWallPieces(theTiles, theCorners, this->mMap);
    for (unsigned long k = 0; k < theTiles.size(); k++) {
        long theX = theTiles[k].GetX();
        long theY = theTiles[k].GetY();
        EEMapTile* theMapTile = this->mMap->GetTile(theX, theY);
        if (theMapTile == NULL)
            continue;
        if (theMapTile->mUnits == NULL) {
            if (theMapTile->GetTerrainType() == kTerrainDeepWater || theMapTile->GetTerrainType() == kTerrainShallowWater)
                continue;
            EEComplexUnit* theUnit = inClass->CreateUnitAtTile(inPlayer, theX, theY, this->mMap, 3.1415927f)->GetComplexUnit();
            theUnit->SetBuilt(1, 0, 0);
            this->mMap->GetTile(theX, theY);
            theTile.SetX(theX);
            theTile.SetY(theY);
            thePlaced.push_back(theTile);
            long theRaised = this->mMap->GetNumRaisedCorners(theX, theY);
            EEShapeWall(theUnit, theCorners[k], theRaised);
        } else {
            EEUnit* theOther = theMapTile->mUnits->mUnit;
            EEComplexUnit* theComplexUnit = theOther->GetComplexUnit();
            if (theOther->GetPlayer() == inPlayer && theOther->GetUnitClass()->mAIKind == kAIKindFortification && theComplexUnit &&
                theComplexUnit->GetAnimation() != 0xb)
                theUnits.push_back(theComplexUnit);
        }
    }
    EEMakeWallJunctions(theUnits, thePlaced, inPlayer, this->mMap);
}

// Runs the generator's height-map filter when the script asks for it; returns its result
long EERandomMap::FilterHeightMap()
{
    long theResult = 0;
    if (this->mData.mFilterHeightMap)
        theResult = this->mFilterHeightMapPIM(&this->mData);
    return theResult;
}

// Finds the named generator's PIM and generates the map, retrying with a new seed (11 tries at most) until the players
// are connected; returns the PIM search status
long EERandomMap::CreateRandomMap(EEGameOptions* inOptions, std::vector<EEPlayer*>& inPlayers, unsigned long inPlayer,
                                  const UWideString& inGenerator, unsigned long inSeed, EEMap* inMap,
                                  GE3DPoint& outLocation)
{
    PIMModule* theModule;
    unsigned long theAttempts = 0;
    long theWidth;
    long theHeight;
    inOptions->GetMapDimensions(theWidth, theHeight);
    unsigned long theGeneratorIndex = 0;
    while (theGeneratorIndex < EERandomMap::sRandomMapGenerators.size()) {
        if (PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)EERandomMap::sRandomMapGenerators[theGeneratorIndex].mName),
                     reinterpret_cast<const unsigned char*>((const char*)inGenerator)) == 0)
            break;
        theGeneratorIndex++;
    }
    ESVERIFY_AT(theGeneratorIndex < EERandomMap::sRandomMapGenerators.size(), 1340);
    long theResult = PIMModule::GetFirstModule(EERandomMap::sGeneratorDirectory, 'rmap', theModule);
    while (theResult == 0) {
        EERMGenerator& theGenerator = EERandomMap::sRandomMapGenerators.begin()[theGeneratorIndex];
        if (PlatStrICmp(theGenerator.mModuleName, theModule->GetName()) == 0) {
            bool theConnected;
            do {
                EERandomMap* theMap = new EERandomMap(theModule, inOptions, &inPlayers);
                theMap->CreateRandomMap(inPlayer, theGeneratorIndex, inSeed, inMap, outLocation);
                theConnected = EERMArePlayersConnected(theMap->mData, inMap);
                inSeed = theMap->mData.mRandomIntegers.Generate(0, 2000000);
                theAttempts++;
                delete theMap;
            } while (!theConnected && theAttempts <= 10);
            ESDebugOut("!Playable random map created after %ld attempts.\n", theAttempts);
            theModule->Unload();
            break;
        }
        theResult = PIMModule::GetNextModule(theModule);
    }
    return theResult;
}

// Returns the number of random-map generators
unsigned long EERMGetNumberOfGenerators()
{
    return EERandomMap::sRandomMapGenerators.size();
}

// Returns a generator's display name by index
const UWideString& EERMGetGeneratorDisplayName(unsigned long inIndex)
{
    return EERandomMap::sRandomMapGenerators[inIndex].mDisplayName;
}

// Returns the display name of the generator with the given name, or the name itself when none matches
const UWideString& EERMGetGeneratorDisplayName(const UWideString& inName)
{
    unsigned long i; for (i = 0; i < EERandomMap::sRandomMapGenerators.size(); i++) {
        if (inName == EERandomMap::sRandomMapGenerators[i].mName)
            break;
    }
    if (i < EERandomMap::sRandomMapGenerators.size())
        return EERandomMap::sRandomMapGenerators[i].mDisplayName;
    return inName;
}

// Returns a generator's name by index
const UWideString& EERMGetGeneratorName(unsigned long inIndex)
{
    return EERandomMap::sRandomMapGenerators[inIndex].mName;
}

// Finds a generator by name and returns its index; returns 3 when there is none
long EERMFindGenerator(const UWideString& inName, unsigned long& outIndex)
{
    long theResult = 3;
    for (unsigned long i = 0; i < EERandomMap::sRandomMapGenerators.size(); i++) {
        if (inName == EERandomMap::sRandomMapGenerators[i].mName) {
            theResult = 0;
            outIndex = i;
            break;
        }
    }
    return theResult;
}

// Lists the random-map generators: each .rmv script naming an installed rmap PIM, with its string-table display name,
// sorted; asserts that both exist
long EERandomMap::Initialize(const FSFileSpec& inDirectory)
{
    std::vector<UString> theValidPlugIns;
    FSFileSpec theDirectory;
    FSFileSpec theFile;
    std::vector<UString> theTokens;
    UString theQuote("\"");
    UString theSeparators(" \t\n,");
    UString theBrackets("()");
    UString theLine(0x400);
    UString theName;
    EERMGenerator theGenerator;
    PIMModule* theModule;
    EERandomMap::sRandomMapGenerators.clear();
    EERandomMap::sGeneratorDirectory = inDirectory;
    for (long theResult = PIMModule::GetFirstModule(inDirectory, 'rmap', theModule); theResult == 0;
         theResult = PIMModule::GetNextModule(theModule)) {
        theValidPlugIns.push_back(theModule->GetName());
        ESDebugOut("EERandomMap::Initialize: %s random map generator detected.\n", (const char*)theModule->GetName());
        theModule->Unload();
    }
    ESVERIFY_AT(theValidPlugIns.size() > 0, 1571);

    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "Data\\Random Map Scripts\\");
    if (theDirectory.GetFirstFile("*.rmv", theFile) == 0) {
        do {
            if (theFile.FOpen("r") != 0)
                continue;
            FILE* theStream = theFile;
            bool theHasStringID = false;
            bool theHasGenerator = false;
            long theStringID = 0;
            char theBuffer[1024];
            while (fgets(theBuffer, 0x400, theStream) != NULL) {
                theLine = theBuffer;
                theLine.Tokenize(theQuote, theTokens);
                if (theTokens.size() >= 2 &&
                    theTokens[0].GetSubstring(0, kUseMapGeneratorKey.GetLength()) == kUseMapGeneratorKey) {
                    theName = theFile.GetFile();
                    theName.ToLower();
                    theGenerator.mName = theFile.GetFile().GetSubstring(0, theName.FindBackwards(".rmv", -1));
                    theGenerator.mModuleName = theTokens[1];
                    theHasGenerator = true;
                }
                theLine.NestedTokenize(theSeparators, theBrackets, theTokens);
                if (theTokens.size() >= 2 &&
                    theTokens[0].GetSubstring(0, kStringTableIDKey.GetLength()) == kStringTableIDKey) {
                    sscanf(theTokens[1], "%ld", &theStringID);
                    theHasStringID = true;
                }
            }
            theFile.Close();
            if (theHasGenerator &&
                std::find(theValidPlugIns.begin(), theValidPlugIns.end(), theGenerator.mModuleName) !=
                    theValidPlugIns.end()) {
                if (!theHasStringID || gResource->GetString(theStringID, theGenerator.mDisplayName) != 0)
                    theGenerator.mDisplayName = theGenerator.mName;
                EERandomMap::sRandomMapGenerators.push_back(theGenerator);
            }
        } while (theDirectory.GetNextFile(theFile) == 0);
    }
    ESVERIFY_AT(EERandomMap::sRandomMapGenerators.size() > 0, 1652);
    // a predicate: the sort COMDATs pass a 4-byte P (0x68b733 _Unguarded_insert, _Adjust_heap); provisional name
    std::sort(EERandomMap::sRandomMapGenerators.begin(), EERandomMap::sRandomMapGenerators.end(), std::less<EERMGenerator>());
    return 0;
}

// Fills the generator state from the game: seeds, map dimensions and size class, players, start epoch, and the unit,
// terrain and cliff names (spaces removed) the scripts refer to
void EERandomMap::InitializeValues(unsigned long inSeed)
{
    std::vector<UString> theTokens;
    UString theSeparator(" ");
    UString theUnitName;
    UString theName;
    EERMNamedValue theCliff;
    EERMNamedValue theTerrain;
    EERMNamedEntry theUnit;
    unsigned long i;
    unsigned long j;
    this->mData.mSeed = inSeed;
    this->mData.mRandom.mSeed = inSeed;
    this->mData.mRandomIntegers.mSeed = inSeed;
    this->mData.mMaximumY = this->mMap->mL195610 << 1;
    this->mData.mMaximumX = this->mMap->mL195614 << 1;
    FSFileSpec& theDirectory = this->mData.mScriptDirectory;
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "Data\\Random Map Scripts\\");
    this->mData.mTileCount = this->mMap->GetTileCount();
    this->mData.mClumpCircles.resize(0);
    this->mData.mPlayerCount = this->mGamePlayers->size() - 1;
    this->mData.mStartEpoch = this->mOptions->mEpochStart;
    unsigned long theHeight = this->mMap->mL195610;
    {   // the scripts' size class: that of the smallest map size (gameoptions.yaml) at least this high
        long theBest = -1;
        for (const EEMapSizeRule& theSize : EEGetRules().mGameOptions.mMapSizes)
            if ((unsigned long)theSize.mTiles >= theHeight &&
                (theBest < 0 || theSize.mTiles < EEGetRules().mGameOptions.mMapSizes[theBest].mTiles))
                theBest = &theSize - &EEGetRules().mGameOptions.mMapSizes[0];
        if (theBest >= 0)
            this->mData.mMapSize = EEGetRules().mGameOptions.mMapSizes[theBest].mScriptSize;
        else
            ESVERIFY_AT(false, 1735);
    }
    for (i = 0; i < EEUnitClassCount(); i++) {
        EEUnitClass* theClass = gUnitClassTable[i];
        if (theClass == NULL || !theClass->mIsEnabled)
            continue;
        theClass->mName.Tokenize(theSeparator, theTokens);
        if (theTokens.size() <= 0)
            continue;
        theUnitName = theTokens[0];
        for (j = 1; j < theTokens.size(); j++)
            theUnitName += theTokens[j];
        theUnit.mName = theUnitName;
        theUnit.mUnitClass = i;
        theUnit.mSize = theClass->mTileSize <= 1 ? 1 : theClass->mTileSize;
        this->mData.mUnitNames.push_back(theUnit);
    }
    this->mData.mFeatures.clear();
    this->mData.mTerrainNames.clear();
    for (i = 0; i < EETerrainTable::GetNumTables(); i++) {
        EETerrainTable* theTable = EETerrainTable::Get(i);
        if (theTable == NULL)
            continue;
        theTable->mName.Tokenize(theSeparator, theTokens);
        if (theTokens.size() <= 0)
            continue;
        theName = theTokens[0];
        for (j = 1; j < theTokens.size(); j++)
            theName += theTokens[j];
        theTerrain.mName = theName;
        theTerrain.mValue = i;
        this->mData.mTerrainNames.push_back(theTerrain);
        if (PlatStrICmp(theName, "Space") == 0)
            gSpaceTerrain = i;
    }
    std::vector<EERMNamedValue>& theCliffs = this->mData.mCliffNames;
    theCliffs.clear();
    for (i = 0; i < EETerrainTable::GetNumCliffTerrains(); i++) {
        EECliffTerrainTable* theTable = EEGetCliffTerrainTable(i);
        if (theTable == NULL)
            continue;
        theTable->mName.Tokenize(theSeparator, theTokens);
        if (theTokens.size() <= 0)
            continue;
        theName = theTokens[0];
        for (j = 1; j < theTokens.size(); j++)
            theName += theTokens[j];
        theCliff.mName = theName;
        theCliff.mValue = i;
        theCliffs.push_back(theCliff);
    }
}

// Sizes the generator grids to the power of two above the map (64 to 1024), resets them to the initial elevation and
// sets up the player, neutral and moon rings (the size and the rings: ee_rmap game.rs)
void EERandomMap::CreateGrids()
{
    this->mData.mGridSize = ee_rmap_game_grid_size((uint32_t)(this->mMap->mL195610), (uint32_t)(this->mMap->mL195614),
                                                   (uint32_t)this->mData.mGridSize);
    this->mData.mClaimedTiles.resize(this->mData.mGridSize + 1);
    this->mData.mTiles.resize(this->mData.mGridSize + 1);
    this->mData.mHeights.resize(this->mData.mGridSize + 1);
    this->mData.mFlatTiles.resize(this->mData.mGridSize + 1);
    this->mData.mValidTiles.resize(this->mData.mGridSize + 1);
    unsigned long x;
    for (x = 0; x < this->mData.mGridSize + 1; x++) {
        this->mData.mClaimedTiles[x].resize(this->mData.mGridSize + 1, 0);
        this->mData.mTiles[x].resize(this->mData.mGridSize + 1, 0);
        this->mData.mHeights[x].resize(this->mData.mGridSize + 1, 0.0f);
        this->mData.mFlatTiles[x].resize(this->mData.mGridSize + 1, 0);
        this->mData.mValidTiles[x].resize(this->mData.mGridSize + 1, 0);
    }
    for (x = 0; x < this->mData.mGridSize + 1; x++) {
        for (unsigned long y = 0; y < this->mData.mGridSize + 1; y++) {
            this->mData.mClaimedTiles[x][y] = 0;
            this->mData.mTiles[x][y] = 0;
            this->mData.mHeights[x][y] = this->mData.mInitialElevation;
            this->mData.mFlatTiles[x][y] = 0;
            this->mData.mValidTiles[x][y] = 0;
        }
    }
    EeRmapRect theFlat = {this->mData.mFlatArea.GetLeft(), this->mData.mFlatArea.GetBottom(),
                          this->mData.mFlatArea.GetRight(), this->mData.mFlatArea.GetTop()};
    EeRmapGameRadii theRadii = {this->mData.mPlayerInnerRadius, this->mData.mPlayerOuterRadius,
                                this->mData.mNeutralInnerRadius, this->mData.mNeutralOuterRadius};
    EeRmapGameRings theRings = ee_rmap_game_rings(theFlat, (int32_t)this->mMap->mSize, theRadii);
    EERMRing* theOut[3] = {&this->mData.mRing, &this->mData.mNeutralRing, &this->mData.mMoonRing};
    const EeRmapRing* theIn[3] = {&theRings.ring, &theRings.neutral_ring, &theRings.moon_ring};
    this->mData.mHalfMapSize = theRings.half_map_size;
    for (int i = 0; i < 3; i++) {
        theOut[i]->mCenter = GE2DPoint(theIn[i]->center.x, theIn[i]->center.y);
        theOut[i]->mMinRadius = theIn[i]->min_radius;
        theOut[i]->mMaxRadius = theIn[i]->max_radius;
    }
}

// Prepares a generation: fills the state, applies the generator script's values, then builds the grids, valid map and
// teams; returns the script status
long EERandomMap::SetUp(unsigned long inGenerator, EEMap* inMap, unsigned long inSeed)
{
    UString theScript;
    this->mMap = inMap;
    this->InitializeValues(inSeed);
    theScript = EERandomMap::sRandomMapGenerators[inGenerator].mName;
    long theResult = this->mSetRandomMapValues(&this->mData, theScript);
    if (theResult == 0) {
        this->CreateGrids();
        this->CreateValidMap();
        this->CreateTeams();
    }
    return theResult;
}

// Drops the registered random-map generators (at Empire Earth.cpp's shutdown); returns 0
// (cdecl; provisional name)
long EEClearRandomMapGenerators()
{
    EERandomMap::sRandomMapGenerators.clear();
    return 0;
}
