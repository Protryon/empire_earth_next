// C:\EEX\Empire Earth\Map\EEMap.cpp (file name inferred: no __FILE__, no asserts). The game map (RTTI EEMap > RTSMap,
// FSPersistent; vtables 0x441cbc / 0x441c9c): 0x11b844..0x11e36e (the two ctors, ??_G, the dtor and the members in
// definition order, then the inline COMDATs first used here: EEPlayer::IsAlliedWith 0x11c10d, vector<GEColor>::
// push_back, the region tables, the FSPersistent thunks, the database tables' inline statics, EETile's ctor).
// Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <utility>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/GETextureSet.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GEPhysicalModel.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/URandom.h"
#include "lle/FSFile.h"

#include "ee/EETileLocation.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEEffectsTable.h"
#include "ee/EEMiniMap.h"
#include "ee/EEGraphics.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EETerrainColorTable.h"
#include "ee/EEPersistentTag.h"
#include "ee/EEUnitRegions.h"
#include "ee/EEAppCapture.h"
#include "rust/ee_app.h"

extern long gDefaultTerrain;                // 0x4e86d0 (EETerrainTable.cpp)
extern long gFirstCliffTerrain;             // 0x4e86e8 (EETerrainTable.cpp)
extern bool gSightUpdatesEnabled;           // 0x4c76ec (EEUserInterface.cpp; provisional name)
// 0x24b212 (EEFOFindNewTargetNearOldTarget.cpp's range): the tiles within inRadius of inCenter (provisional name)
void EEGetTilesNearPoint(const GE3DPoint& inCenter, float inRadius, std::vector<EETileCoordinate>& outTiles);

// unit-local constants (.rdata next to EEMap's vtables)
const float kMapTileSize = 0.40824828f;     // 0x441ce8 (RTSMap's ctor argument; provisional name)
const float kSlopeScale = -0.76923078f;     // 0x441cf8 (GetTerrainPitch / GetTerrainRoll)

// New (port): the sight, the cliffs, the painting, the water colours, the checksum and the tile count are ee_app
// map.rs over this host (the tiles, the terrain tables, the players' sight, the mini map; the mesh it reads itself)
struct EEMapHostContext {
    EEMap* mMap;
    EEPlayer* mPlayer;                      // the sight's player
    std::vector<EETileCoordinate> mTiles;   // TILES_NEAR's answer
};

static void EEMapHostOp(void* inContext, int32_t inOp, EeAppMapIo* io)
{
    EEMapHostContext& theContext = *static_cast<EEMapHostContext*>(inContext);
    EEMap* theMap = theContext.mMap;
    EEPlayer* thePlayer = theContext.mPlayer;
    switch (inOp) {
    case EE_APP_MAP_OP_TILE:
        io->flag = theMap->GetTile(io->x, io->y) != NULL;
        break;
    case EE_APP_MAP_OP_TERRAIN_TYPE:
        io->a = theMap->GetTile(io->x, io->y)->mTerrainType;
        break;
    case EE_APP_MAP_OP_SET_TERRAIN_TYPE: {
        EETileLocation theLocation(io->x, io->y);
        theMap->GetTile(io->x, io->y)->SetTerrainType(io->a, theLocation, theMap);
        break;
    }
    case EE_APP_MAP_OP_SET_TILE_TERRAIN:
        theMap->SetTileTerrain(io->x, io->y, io->a, io->flag);
        break;
    case EE_APP_MAP_OP_TERRAIN: {
        EETerrainTable* theTerrain = EETerrainTable::Get(io->a);
        io->a = theTerrain->mTextureSetID;
        io->b = theTerrain->mTerrainType;
        io->c = theTerrain->mColorType;
        break;
    }
    case EE_APP_MAP_OP_COLOR_SET:
        io->a = EETerrainTypeTable::Get(io->a)->mColorSetID;
        break;
    case EE_APP_MAP_OP_CLIFF:
        io->b = EETerrainTable::sCliffTable[io->a]->mLastTerrain;
        io->a = EETerrainTable::sCliffTable[io->a]->mFirstTerrain;
        break;
    case EE_APP_MAP_OP_TILE_CHECKSUM:
        io->a = theMap->GetTile(io->x, io->y)->GetChecksum(theMap->mU19561C);
        break;
    case EE_APP_MAP_OP_SIGHT_ENABLED:
        io->flag = gSightUpdatesEnabled == true;
        break;
    case EE_APP_MAP_OP_LOCAL_SEES: {
        EEPlayer* theLocal = ::gWorld.mPlayers[::gWorld.mCurrentPlayer];
        io->flag = theLocal == thePlayer ||
                   (io->flag && theLocal->IsAlliedWith(thePlayer) && theLocal->mSharedLOS && thePlayer->mSharedLOS);
        break;
    }
    case EE_APP_MAP_OP_TILES_NEAR:
        theContext.mTiles.clear();
        EEGetTilesNearPoint(GE3DPoint(io->f[0], io->f[1], 0.0f), io->f[2], theContext.mTiles);
        io->tiles = theContext.mTiles.empty() ? NULL : &theContext.mTiles[0].mX;
        io->a = (long)theContext.mTiles.size();
        break;
    case EE_APP_MAP_OP_SIGHT_AT: {
        EETileCoordinate theTile(io->x, io->y);
        if (io->a != 0 && io->flag)
            thePlayer->AddSharedSightAt(theTile);
        else if (io->a != 0)
            thePlayer->RemoveSharedSightAt(theTile);
        else if (io->flag)
            thePlayer->AddOwnSightAt(theTile);
        else
            thePlayer->RemoveOwnSightAt(theTile);
        break;
    }
    case EE_APP_MAP_OP_COUNT_CELL:
        if (io->flag)
            gMiniMap->CountCellOn(io->x, io->y);
        else
            gMiniMap->CountCellOff(io->x, io->y);
        break;
    }
}

static EeAppMapDims EEMapDims(EEMap* inMap)
{
    return {inMap->mSize, inMap->mL195610, inMap->mL195614, inMap->mF1955F4, (long)inMap->mU195650,
            (long)inMap->mU19565C, inMap->mU19561C};
}

static EeAppMapHost EEMapHost(EEMap* inMap, EEMapHostContext& inContext)
{
    return {&EEMapHostOp, &inContext, inMap->mTerrainMesh.GetRust(), EEMapDims(inMap)};
}

// one of the map's edits (ee_app_map_edit's entries)
static void EEMapEdit(EEMap* inMap, uint32_t inWhat, long inA = 0, long inB = 0, long inC = 0, long inD = 0,
                      long inE = 0, long inF = 0)
{
    EEMapHostContext theContext = {inMap, NULL, {}};
    EeAppMapHost theHost = EEMapHost(inMap, theContext);
    long theArgs[6] = {inA, inB, inC, inD, inE, inF};
    ee_app_map_edit(&theHost, inWhat, theArgs);
}

// Creates an empty map on the default terrain
EEMap::EEMap()
    : RTSMap(kMapTileSize), FSPersistent(kPersistentMap), mRandom(1)
{
    this->mU195650 = gDefaultTerrain;
    this->Initialize();
}

// Creates a map of the given size and player count, filled with the given terrain
EEMap::EEMap(long inWidth, long inHeight, unsigned long inPlayerCount, long inTerrain)
    : RTSMap(kMapTileSize), FSPersistent(kPersistentMap), mRandom(1)
{
    this->Initialize();
    this->mU195650 = inTerrain;
    this->CreateMap(inWidth, inHeight, inPlayerCount);
    this->mU19565C = gFirstCliffTerrain;
}

// Destroys the map: frees its region tables, tiles and visibility words and unloads its terrain textures
EEMap::~EEMap()
{
    ee_combat_regions_free(this->mUnitRegions);
    delete this->mEffectRegions;
    delete[] this->mTileArray;
    if (this->mVisibilityWords)
        delete[] this->mVisibilityWords;
    EETerrainTable::Get(this->mTerrainMesh.GetGlobalWaterSetID())->Unload(this->mTerrainMesh);
    for (unsigned long x = 0; x < this->mSize + 1; x++) {
        for (unsigned long y = 0; y < this->mSize + 1; y++) {
            if (this->mTerrainMesh.GetIsValidTile(x, y) && EETerrainTable::Get(this->mTerrainMesh.GetTextureSetID(x, y)))
                EETerrainTable::Get(this->mTerrainMesh.GetTextureSetID(x, y))->Unload(this->mTerrainMesh);
        }
    }
}

// Hands out the next reserved tile at the given height, with its slice of the visibility words
EEMapTile* EEMap::CreateTile(float inHeight)
{
    unsigned long thePlayers = this->mU19561C;
    EETile* theTile = &this->mTileArray[this->mNextTile];
    theTile->mHeight = inHeight;
    theTile->mVisibility = &this->mVisibilityWords[((thePlayers >> 1) + (thePlayers & 1)) * this->mNextTile];
    this->mNextTile++;
    return theTile;
}

// Allocates the given number of tiles and cleared visibility words (two players per word), dropping old ones
void EEMap::ReserveTiles(unsigned long inCount)
{
    unsigned long theWords = (this->mU19561C >> 1) + (this->mU19561C & 1);
    if (this->mTileArray) {
        delete[] this->mTileArray;
        delete[] this->mVisibilityWords;
        this->mTileArray = NULL;
        this->mVisibilityWords = NULL;
    }
    this->mTileArray = new EETile[inCount];
    this->mVisibilityWords = new uint32_t[theWords * inCount];
    memset(this->mVisibilityWords, 0, theWords * inCount * sizeof(uint32_t));
    this->mNextTile = 0;
    this->mTileArrayCount = inCount;
}

// Resizes every tile's visibility words for a new player count, keeping the existing visibility
void EEMap::SetPlayerCount(unsigned long inPlayerCount)
{
    unsigned long theOldWords = (this->mU19561C >> 1) + (this->mU19561C & 1);
    unsigned long theNewWords = (inPlayerCount >> 1) + (inPlayerCount & 1);
    unsigned long theCopy = theOldWords < theNewWords ? theOldWords * sizeof(uint32_t) : theNewWords * sizeof(uint32_t);
    uint32_t* theWords = new uint32_t[this->GetTileCount() * theNewWords];
    memset(theWords, 0, this->GetTileCount() * theNewWords * sizeof(uint32_t));
    if (this->mVisibilityWords) {
        unsigned long theIndex = 0;
        for (long y = 0; y < this->mSize; y++) {
            for (long x = 0; x < this->mSize; x++) {
                EETile* theTile = this->GetTile(x, y);
                if (!theTile)
                    continue;
                memcpy(&theWords[theIndex * theNewWords], &this->mVisibilityWords[theIndex * theOldWords], theCopy);
                theTile->mVisibility = &theWords[theIndex * theNewWords];
                theIndex++;
            }
        }
        delete[] this->mVisibilityWords;
    }
    this->mVisibilityWords = theWords;
    this->mU19561C = inPlayerCount;
}

// Drops a unit placed on the map onto the ground at its current position (camera objects excluded)
void EEMap::PlaceUnitOnGround(EEUnit* inUnit)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    GE3DPoint theLocation;
    if (theUnit->mTileNode != NULL && theUnit->GetUnitClass()->mUnitType != kUnitTypeCameraObject) {
        theUnit->mPhysicalModel.GetLocation().GetPosition(theLocation);
        theUnit->PlaceOnGround(theLocation);
    }
}

// Refreshes the terrain type of the four tiles around a vertex, then gives the vertex the water colour set
// when below sea level, else its terrain's colour set (ee_app map.rs)
void EEMap::UpdateWater(long inX, long inY)
{
    EEAppMapScope theScope(this, kAppMapUpdateWater, inX, inY, 0, 0, 0, 0, inX - 1, inX, inY - 1, inY);   // MAPC (TEST ONLY)
    EEMapEdit(this, 0, inX, inY);
}

// Builds a new map of the given size: fresh unit and effect region tables, every tile on the base terrain (the fill:
// ee_app map.rs). The original assigned the terrain mesh to itself here, a no-op; the mesh is a Rust object now.
void EEMap::CreateMap(long inWidth, long inHeight, unsigned long inPlayerCount)
{
    EEAppMapScope theScope(this, kAppMapCreateMap, inWidth, inHeight, (long)inPlayerCount, 0, 0, 0, 1, 0, 0, 0);   // MAPC (TEST ONLY)
    this->Create(inWidth, inHeight, inPlayerCount, true);
    ee_combat_regions_free(this->mUnitRegions);
    delete this->mEffectRegions;
    this->mUnitRegions = ee_combat_regions_new((uint32_t)UMCeilUnsigned((float)this->mSize / 3L), 3);
    this->mEffectRegions = new EEEffectsTable(this, 10);
    EEMapEdit(this, 1);
}

// Creates an empty map for loading
FSPersistent* EEMap::CreateDefault() const
{
    return new EEMap;
}

// Takes a player's own or shared sight away from the mesh-flagged tiles within a radius; if the local player sees
// through it, updates the mini map and terrain visibility counts too (ee_app map.rs)
void EEMap::RemoveSight(EEPlayer* inPlayer, float inX, float inY, float inRadius, bool inShared)
{
    EEMapHostContext theContext = {this, inPlayer, {}};
    EeAppMapHost theHost = EEMapHost(this, theContext);
    ee_app_map_sight(&theHost, inX, inY, inRadius, inShared, false);
    EEAppCaptureSight(1, inPlayer, inX, inY, inRadius, inShared);   // MAPS (TEST ONLY)
}

// Sums the tile and corner-height checksums of the next 64 x 64 window and advances the window; past the right
// edge the cursor moves to the start of the next band (ee_app map.rs)
long EEMap::ComputeChecksum()
{
    unsigned long theCursor = this->mChecksumCursor;   // MAPK (TEST ONLY)
    EEMapHostContext theContext = {this, NULL, {}};
    EeAppMapHost theHost = EEMapHost(this, theContext);
    long theChecksum = ee_app_map_checksum(&theHost, &this->mChecksumCursor);
    EEAppCaptureChecksum(theCursor, theChecksum, this->mChecksumCursor);   // MAPK (TEST ONLY)
    return theChecksum;
}

// Adds the map's base checksum (the RTSMap part)
void EEMap::GetChecksum(ESChecksum& ioChecksum) const
{
    RTSMap::GetChecksum(ioChecksum);
}

// Returns the tile under a screen point and its coordinates (in orthographic view, where the ray meets z 0)
EETile* EEMap::GetTileAt(const GE2DPoint& inPoint, long& outX, long& outY)
{
    GE3DLine theLine;
    GE3DPoint thePoint;
    this->GetScreenLine(inPoint, theLine);
    if (gOrthographicView) {
        float theDistance;
        if (!theLine.SolveForZ(0.0f, theDistance, thePoint))
            thePoint = theLine.GetStart();
        this->mTerrainMesh.GetClosestTile(thePoint, outX, outY);
    } else
        this->mTerrainMesh.GetClosestTile(theLine, outX, outY);
    return this->GetTile(outX, outY);
}

// Gives the ground point at a tile's centre; returns false when there is no such tile
bool EEMap::GetTileLocation(long inX, long inY, GE3DPoint& outLocation)
{
    if (this->GetTile(inX, inY)) {
        outLocation.SetX(inX + 0.5f);
        outLocation.SetY(inY + 0.5f);
        outLocation.SetZ(this->mTerrainMesh.GetMeshZ(outLocation.GetX(), outLocation.GetY()));
        return true;
    }
    return false;
}

// Builds the 3D line through a screen point from the near to the far clipping plane
void EEMap::GetScreenLine(const GE2DPoint& inPoint, GE3DLine& outLine)
{
    GE3DPoint theNear;
    GE3DPoint theFar;
    float theScale = 1.0f / gViewport.GetHalfWidth();
    GE3DPoint thePoint((inPoint.GetX() - gViewport.GetXCenter()) * theScale,
                       -((inPoint.GetY() - gViewport.GetYCenter()) * theScale), 0.0f);
    GE3DPoint theFarPoint(thePoint.GetX(), thePoint.GetY(), 1.0f);
    if (!gOrthographicView) {
        gViewport.DenormalizePerspectivePoint(thePoint, gViewport.GetFrontClippingDistance(), theNear);
        gViewport.DenormalizePerspectivePoint(theFarPoint, gViewport.GetRearClippingDistance(), theFar);
    } else {
        gViewport.DenormalizeOrthographicPoint(thePoint, theNear);
        gViewport.DenormalizeOrthographicPoint(theFarPoint, theFar);
    }
    outLine.Define(theNear, theFar);
}

// Returns the ground's pitch under a location, from the ground heights 0.65 ahead of and behind it
float EEMap::GetTerrainPitch(const GETransformation& inLocation)
{
    GE3DPoint theFront = inLocation.GetPosition();
    GE3DPoint theBack = theFront;
    GE3DPoint theFrontGround;
    GE3DPoint theBackGround;
    GEVector theOffset(0.0f, 0.65f, 0.0f);
    GEVector theDirection;
    inLocation.Apply(theOffset, theDirection);
    theFront += theDirection;
    this->GetGroundPoint(theFront, theFrontGround);
    theOffset.mY = -0.65f;
    inLocation.Apply(theOffset, theDirection);
    theBack += theDirection;
    this->GetGroundPoint(theBack, theBackGround);
    float thePitch = UMArcCos((theFrontGround.GetZ() - theBackGround.GetZ()) * kSlopeScale) - 1.5707964f;
    EEAppCaptureSlope(0, inLocation, thePitch);   // MAPP (TEST ONLY)
    return thePitch;
}

// Returns the ground's roll under a location, from the ground heights 0.65 to either side of it
float EEMap::GetTerrainRoll(const GETransformation& inLocation)
{
    GE3DPoint theFront = inLocation.GetPosition();
    GE3DPoint theBack = theFront;
    GE3DPoint theFrontGround;
    GE3DPoint theBackGround;
    GEVector theOffset(0.65f, 0.0f, 0.0f);
    GEVector theDirection;
    inLocation.Apply(theOffset, theDirection);
    theFront += theDirection;
    this->GetGroundPoint(theFront, theFrontGround);
    theOffset.mX = -0.65f;
    inLocation.Apply(theOffset, theDirection);
    theBack += theDirection;
    this->GetGroundPoint(theBack, theBackGround);
    float theRoll = UMArcCos((theFrontGround.GetZ() - theBackGround.GetZ()) * kSlopeScale) - 1.5707964f;
    EEAppCaptureSlope(1, inLocation, theRoll);   // MAPP (TEST ONLY)
    return theRoll;
}

// Returns the tile hit by the ray through a screen point, optionally stopping at the water surface
EETile* EEMap::PickScreenTile(const GE2DPoint& inPoint, long& outX, long& outY, bool inHitWaterSurface)
{
    GE3DLine theLine;
    this->GetScreenLine(inPoint, theLine);
    return this->GetTileOnLine(theLine, outX, outY, inHitWaterSurface);
}

// Returns the tile and the 3D point hit by the ray through a screen point, optionally at the water surface
EETile* EEMap::GetLocationAt(const GE2DPoint& inPoint, GE3DPoint& outLocation, bool inHitWaterSurface)
{
    GE3DLine theLine;
    this->GetScreenLine(inPoint, theLine);
    return this->GetLocationOnLine(theLine, outLocation, inHitWaterSurface);
}

// Resets the map to empty: no tiles or region tables, seeds its random generator, registers terrain sets
void EEMap::Initialize()
{
    this->mU195650 = 0;
    this->mRandom.SetSeed(TMGetTime());
    this->mUnitRegions = NULL;
    this->mEffectRegions = NULL;
    this->mTileArray = NULL;
    this->mVisibilityWords = NULL;
    this->mNextTile = 0;
    this->mTileArrayCount = 0;
    this->mChecksumCursor = 0;
    this->RegisterTerrainSets();
    this->mMapCharacteristics = 0;
    EETerrainTable::Get(this->mTerrainMesh.GetGlobalWaterSetID())->Load(this->mTerrainMesh);
}

// Reads the map, its base terrain, characteristics and cliff terrain from the saved game and re-types every
// tile (cliffs and unknown terrain become the base terrain; saves up to 0.203 have no space tiles)
long EEMap::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    // port (R5b.4, round 28): the original's two era bodies are one Rust sequence (ee_units map_save.rs) over the records
    // the classes derive from and EEMapSaveOperation's parts
    EEMapSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion, NULL, NULL};
    return ee_units_map_load(&theContext, &EEMapSaveOperation, static_cast<EeUnitsRtsMapFields*>(this),
                             static_cast<EeUnitsMapFields*>(this), (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// New (R5b.4, round 28): the C++ parts of the maps' Load and Save (ee/EEMap.h): the file's primitives (EESavePrimitive),
// the tiles, the mesh and the terrain tables; a tile's y in inIndex, its x in io->i's low 32 bits
int32_t EEMapSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EEMapSaveContext& theContext = *static_cast<EEMapSaveContext*>(inContext);
    RTSMap* theMap = theContext.mMap;
    EEMap* theEEMap = static_cast<EEMap*>(theMap);   // (EEMap's operations only)
    FSFile& theFile = *theContext.mFile;
    long theX = (long)(uint32_t)io->i;
    long theY = (long)inIndex;
    switch (inOp) {
    case EE_UNITS_MAP_SAVE_CREATE:
        theMap->Create(theMap->mL195614, theMap->mL195610, theMap->mU19561C, false);
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_LOAD_TILE:
        theMap->mTiles[theMap->mSize * theY + theX] =
            io->b != 0 ? theMap->LoadTile(theContext.mMajorVersion, theContext.mMinorVersion, theFile, theX, theY) : NULL;
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_MESH_LOAD:
        return theMap->mTerrainMesh.Load(theContext.mMajorVersion, theContext.mMinorVersion, theFile);
    case EE_UNITS_MAP_SAVE_MESH_SAVE:
        return theMap->mTerrainMesh.Save(theFile);
    case EE_UNITS_MAP_SAVE_TILE_SAVE: {
        EEMapTile* theTile = theMap->mTiles[theMap->mSize * theY + theX];
        return theTile != NULL ? theTile->Save(theFile, theMap->mU19561C) : kErrorOK;
    }
    case EE_UNITS_MAP_SAVE_PICK_DEFAULT:
        if (io->b != 0) {
            if (EETerrainTable::Get(theEEMap->mU195650) == NULL)
                theEEMap->mU195650 = gDefaultTerrain;
            theContext.mDefault = EETerrainTable::Get(theEEMap->mU195650);
        } else
            theContext.mDefault = gDefaultTerrain != -1 ? EETerrainTable::Get(gDefaultTerrain) : NULL;
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_VALID:
        io->i = theEEMap->mTerrainMesh.GetIsValidTile(theX, theY) ? 1 : 0;
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_TERRAIN_TYPE:
        theContext.mTerrain = EETerrainTable::Get(theEEMap->mTerrainMesh.GetTextureSetID(theX, theY));
        io->b = theContext.mTerrain != NULL ? 1 : 0;
        io->i = theContext.mTerrain != NULL ? theContext.mTerrain->mTerrainType : 0;
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_SPACE:
        io->i = theEEMap->mTerrainMesh.GetIsSpaceTile(theX, theY) ? 1 : 0;
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_SET_TYPE: {
        EETileLocation theLocation(theX, theY);
        theEEMap->GetTile(theX, theY)->SetTerrainType((long)(io->i >> 32), theLocation, theEEMap);
        return kErrorOK;
    }
    case EE_UNITS_MAP_SAVE_TERRAIN_LOAD:
        theContext.mTerrain->Load(theEEMap->mTerrainMesh);
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_SET_DEFAULT: {
        theEEMap->SetTileTerrain(theX, theY, theContext.mDefault->mTextureSetID, false);
        EETileLocation theLocation(theX, theY);
        theEEMap->GetTile(theX, theY)->SetTerrainType(theContext.mDefault->mTerrainType, theLocation, theEEMap);
        return kErrorOK;
    }
    case EE_UNITS_MAP_SAVE_CLIFF_VALID: {
        unsigned long theID = (unsigned long)io->i;
        io->b = theID >= EETerrainTable::GetNumCliffTerrains() || EETerrainTable::sCliffTable[theID] == NULL ? 0 : 1;
        return kErrorOK;
    }
    case EE_UNITS_MAP_SAVE_FIRST_CLIFF:
        io->i = gFirstCliffTerrain;
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_FINISH:
        ee_combat_regions_free(theEEMap->mUnitRegions);
        delete theEEMap->mEffectRegions;
        theEEMap->mUnitRegions = ee_combat_regions_new((uint32_t)UMCeilUnsigned((float)theEEMap->mSize / 3L), 3);
        theEEMap->mEffectRegions = new EEEffectsTable(theEEMap, 10);
        theEEMap->UpdateCliffs(0, theEEMap->mSize, 0, theEEMap->mSize);
        return kErrorOK;
    }
    return EESavePrimitive(theFile, inOp, io);
}

// Reads the next tile from the saved game (saves up to 0.46 also carry its terrain); returns the tile
EEMapTile* EEMap::LoadTile(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile, long inX,
                            long inY)
{
    long theTerrain = 0;
    long theModel = 0;
    EETile* theTile = &this->mTileArray[this->mNextTile];
    theTile->mVisibility = &this->mVisibilityWords[((this->mU19561C >> 1) + (this->mU19561C & 1)) * this->mNextTile];
    this->mNextTile++;
    if (inMajorVersion == 0 && inMinorVersion <= 0x2e) {
        inFile.Read((unsigned long&)theTerrain);
        inFile.Read((unsigned long&)theModel);
        EETerrainTable* theTable = EETerrainTable::Get(theTerrain);
        if (theTable)
            this->SetTileTerrain(inX, inY, theTable->mTextureSetID, true);
        else
            this->SetTileTerrain(inX, inY, this->mU195650, true);
    }
    theTile->Load(inMajorVersion, inMinorVersion, inFile, this->mU19561C);
    return theTile;
}

// Writes every tile's state to the out-of-sync dump file
void EEMap::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP: ************************************************* MAP "
                    "*********************************\n\n");
    for (long y = 0; y < this->mSize; y++) {
        for (long x = 0; x < this->mSize; x++) {
            EEMapTile* theTile = this->GetTile(x, y);
            if (!theTile)
                continue;
            fprintf(inFile, "Tile X(%ld), Y(%ld)\n", x, y);
            theTile->Dump(inFile, this->mU19561C);
            fprintf(inFile, "\n");
        }
    }
}

// Gives a player own or shared sight of the mesh-flagged tiles within a radius; if the local player sees through
// it, updates the mini map and terrain visibility counts too (ee_app map.rs)
void EEMap::AddSight(EEPlayer* inPlayer, float inX, float inY, float inRadius, bool inShared)
{
    EEMapHostContext theContext = {this, inPlayer, {}};
    EeAppMapHost theHost = EEMapHost(this, theContext);
    ee_app_map_sight(&theHost, inX, inY, inRadius, inShared, true);
    EEAppCaptureSight(0, inPlayer, inX, inY, inRadius, inShared);   // MAPS (TEST ONLY)
}

// Paints a rectangle of tiles with a terrain, then recolours its vertices with the given colour type (ee_app map.rs)
void EEMap::PaintTerrainRect(long inLeft, long inRight, long inTop, long inBottom, long inTerrain, long inCliff)
{
    EEAppMapScope theScope(this, kAppMapPaintTerrainRect, inLeft, inRight, inTop, inBottom, inTerrain, inCliff, inLeft,
                           inRight, inBottom, inTop);   // MAPC (TEST ONLY)
    EEMapEdit(this, 5, inLeft, inRight, inTop, inBottom, inTerrain, inCliff);
}

// Sets a tile's terrain type and paints it and its lower-left neighbours' textures with the terrain (ee_app map.rs)
void EEMap::PaintTerrain(long inX, long inY, long inTerrain)
{
    EEAppMapScope theScope(this, kAppMapPaintTerrain, inX, inY, inTerrain, 0, 0, 0, inX - 1, inX, inY - 1, inY);   // MAPC (TEST ONLY)
    EEMapEdit(this, 4, inX, inY, inTerrain);
}

// Gives the vertices of a tile rectangle that are not under water a colour type's colour set (ee_app map.rs)
void EEMap::SetCliffColorSets(long inLeft, long inRight, long inTop, long inBottom, long inCliff)
{
    EEAppMapScope theScope(this, kAppMapSetCliffColorSets, inLeft, inRight, inTop, inBottom, inCliff, 0, inLeft, inRight,
                           inBottom, inTop);   // MAPC (TEST ONLY)
    EEMapEdit(this, 6, inLeft, inRight, inTop, inBottom, inCliff);
}

// Marks steep tiles in a rectangle as cliffs, texturing them from the cliff terrain by slope direction, then makes
// tiles with three or more cliff neighbours cliffs too (ee_app map.rs)
void EEMap::UpdateCliffs(long inLeft, long inRight, long inBottom, long inTop)
{
    EEAppMapScope theScope(this, kAppMapUpdateCliffs, inLeft, inRight, inBottom, inTop, 0, 0, inLeft - 1, inRight,
                           inBottom - 1, inTop);   // MAPC (TEST ONLY)
    EEMapEdit(this, 2, inLeft, inRight, inBottom, inTop);
}

// Turns cliff tiles in a rectangle that are no longer steep back into the map's base terrain (ee_app map.rs)
void EEMap::RemoveFlatCliffs(unsigned long inLeft, unsigned long inRight, unsigned long inBottom, unsigned long inTop)
{
    EEAppMapScope theScope(this, kAppMapRemoveFlatCliffs, (long)inLeft, (long)inRight, (long)inBottom, (long)inTop, 0, 0,
                           (long)inLeft - 1, (long)inRight, (long)inBottom - 1, (long)inTop);   // MAPC (TEST ONLY)
    EEMapEdit(this, 3, (long)inLeft, (long)inRight, (long)inBottom, (long)inTop);
}

// Registers every terrain's texture set and every terrain type's colour set with the terrain mesh, then
// the water state and the colour table
void EEMap::RegisterTerrainSets()
{
    std::vector<GEColor> theColors;
    std::vector<unsigned long> theColorSet;
    GETerrainMesh& theMesh = this->mTerrainMesh;
    unsigned long theWaterSet = 0;
    unsigned long theID;
    for (unsigned long i = 0; i < EETerrainTable::sTable.size(); i++) {
        EETerrainTable* theTerrain = EETerrainTable::Get(i);
        if (theTerrain) {
            GETextureSet* theSet = new GETextureSet(theTerrain->mColumns, theTerrain->mRows, theTerrain->mTextureTileCoverage);
            theMesh.RegisterTextureSet(theSet, theID);
            if (theTerrain->mIsWater == true)
                theWaterSet = theID;
            theTerrain->AttachTextures(theSet);
            theTerrain->mTextureSetID = theID;
        } else
            theMesh.RegisterTextureSet(NULL, theID);
    }
    for (unsigned long j = 0; j < EETerrainTypeTable::GetCount(); j++) {
        EETerrainTypeTable* theType = EETerrainTypeTable::Get(j);
        if (!theType)
            continue;
        theColorSet.clear();
        for (unsigned long k = theType->mFirstColor; (long)k <= theType->mLastColor; k++)
            theColorSet.push_back(k);
        theMesh.RegisterColorSet(theColorSet, theID);
        theType->mColorSetID = theID;
    }
    for (unsigned long m = 0; m < EETerrainColorTable::GetCount(); m++) {
        EETerrainColorTable* theColor = EETerrainColorTable::Get(m);
        if (theColor)
            theColors.push_back(theColor->mColor);
    }
    EETerrainTable* theWater = EETerrainTable::Get(theWaterSet);
    theMesh.SetGlobalWaterState(theWaterSet, EETerrainTypeTable::Get(theWater->mColorType)->mColorSetID, theWater->mWaterAnimationPeriod,
                                -(this->GetF1955F4() * theWater->mWaterDepth));
    theMesh.SetColorTable(theColors);
}

// Writes the map, its base terrain, characteristics and cliff terrain to the saved game; returns the first error
long EEMap::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEMapSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion, NULL, NULL};   // port: ee_units map_save.rs
    return ee_units_map_save(&theContext, &EEMapSaveOperation, static_cast<EeUnitsRtsMapFields*>(this),
                             static_cast<EeUnitsMapFields*>(this));
}

// Switches the map to another cliff terrain and re-runs cliff detection over the whole map
void EEMap::SetCliffTexture(long inCliffTexture)
{
    // Fixed: a random map script without CliffTexture (Continental and most others) passes -1, which indexed the cliff
    // table at [-1] (the 32-bit original read the heap word before it as a table pointer). As Load does with a bad
    // saved index, it takes the first cliff terrain.
    if (inCliffTexture < 0 || (unsigned long)inCliffTexture >= EETerrainTable::GetNumCliffTerrains() ||
        EETerrainTable::sCliffTable[inCliffTexture] == NULL)
        inCliffTexture = gFirstCliffTerrain;
    EEAppMapScope theScope(this, kAppMapSetCliffTexture, inCliffTexture, 0, 0, 0, 0, 0, 1, 0, 0, 0);   // MAPC (TEST ONLY)
    this->mU19565C = inCliffTexture;
    this->UpdateCliffs(0, this->mSize, 0, this->mSize);
}

// Sets a tile's texture set, loading the new terrain's textures and optionally unloading the old one's
void EEMap::SetTileTerrain(long inX, long inY, long inTextureSet, bool inUnload)
{
    if (inUnload == true)
        EETerrainTable::Get(this->mTerrainMesh.GetTextureSetID(inX, inY))->Unload(this->mTerrainMesh);
    EETerrainTable::Get(inTextureSet)->Load(this->mTerrainMesh);
    this->mTerrainMesh.SetTextureSet(inX, inY, EETerrainTable::Get(inTextureSet)->mTextureSetID);
}

// Returns the number of tiles: the square map minus its two cut-off corner triangles (ee_app map.rs)
unsigned long EEMap::GetTileCount()
{
    EeAppMapDims theDims = EEMapDims(this);
    return ee_app_map_tile_count(&theDims);
}

// Returns whether the other player is this player or neither of the two is at war with the other
// (out of line after its first user here)
bool EEPlayer::IsAlliedWith(EEPlayer* inPlayer)
{
    return inPlayer == this || this->mEnemies[inPlayer->mPlayerID] == 0 && inPlayer->mEnemies[this->mPlayerID] == 0;
}
