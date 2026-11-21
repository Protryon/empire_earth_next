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
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/GETextureSet.h"
#include "lle/GEViewport.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEGraphics.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EETerrainColorTable.h"
#include "ee/EEPersistentTag.h"
#include "ee/EEAppCapture.h"
#include "rust/render/gfx.h"
#include "rust/sim/units.h"
#include "rust/game/app.h"   // the capture's MAPK, MAPP (ee_game app world_capture.rs)
#include "rust/game/gamecpp.h"

extern long gDefaultTerrain;                // 0x4e86d0 (EETerrainTable.cpp)
extern long gFirstCliffTerrain;             // 0x4e86e8 (EETerrainTable.cpp)
extern bool gSightUpdatesEnabled;           // 0x4c76ec (EEUserInterface.cpp; provisional name)

// unit-local constants (.rdata next to EEMap's vtables)
const float kMapTileSize = 0.40824828f;     // 0x441ce8 (RTSMap's ctor argument; provisional name)
const float kSlopeScale = -0.76923078f;     // 0x441cf8 (GetTerrainPitch / GetTerrainRoll)

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
    ee_gamecpp_map_destroy(this);
}

// Hands out the next reserved tile at the given height, with its slice of the visibility words
EEMapTile* EEMap::CreateTile(float inHeight)
{
    return (EEMapTile*)ee_gamecpp_map_create_tile(this, inHeight);
}

// Allocates the given number of tiles and cleared visibility words (two players per word), dropping old ones
void EEMap::ReserveTiles(unsigned long inCount)
{
    ee_gamecpp_map_reserve_tiles(this, inCount);
}

// Resizes every tile's visibility words for a new player count, keeping the existing visibility
void EEMap::SetPlayerCount(unsigned long inPlayerCount)
{
    ee_gamecpp_map_set_player_count(this, inPlayerCount);
}

// Drops a unit placed on the map onto the ground at its current position (camera objects excluded)
void EEMap::PlaceUnitOnGround(EEUnit* inUnit)
{
    ee_gamecpp_map_place_on_ground(static_cast<EEComplexUnit*>(inUnit));
}

// Refreshes the terrain type of the four tiles around a vertex, then gives the vertex the water colour set
// when below sea level, else its terrain's colour set (ee_app map.rs)
void EEMap::UpdateWater(long inX, long inY)
{
    EEAppMapScope theScope(this, kAppMapUpdateWater, inX, inY, 0, 0, 0, 0, inX - 1, inX, inY - 1, inY);   // MAPC (TEST ONLY)
    ee_gamecpp_map_edit(this, 0, inX, inY, 0, 0, 0, 0);
}

// Builds a new map of the given size: fresh unit and effect region tables, every tile on the base terrain (the fill:
// ee_app map.rs). The original assigned the terrain mesh to itself here, a no-op; the mesh is a Rust object now.
void EEMap::CreateMap(long inWidth, long inHeight, unsigned long inPlayerCount)
{
    EEAppMapScope theScope(this, kAppMapCreateMap, inWidth, inHeight, (long)inPlayerCount, 0, 0, 0, 1, 0, 0, 0);   // MAPC (TEST ONLY)
    ee_gamecpp_map_create_map(this, inWidth, inHeight, inPlayerCount);
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
    EESightScope theCapture(kSightMapRemove, inPlayer, inX, inY, 0.0f, 0.0f, inRadius, inShared);   // TSGT (TEST ONLY)
    ee_gamecpp_map_sight(this, inPlayer, inX, inY, inRadius, inShared, false);
    EEAppCaptureSight(1, inPlayer, inX, inY, inRadius, inShared);   // MAPS (TEST ONLY)
}

// Sums the tile and corner-height checksums of the next 64 x 64 window and advances the window; past the right
// edge the cursor moves to the start of the next band (ee_app map.rs)
long EEMap::ComputeChecksum()
{
    return ee_gamecpp_map_checksum(this);   // (with MAPK, TEST ONLY)
}

// Adds the map's base checksum (the RTSMap part)
void EEMap::GetChecksum(ESChecksum& ioChecksum) const
{
    RTSMap::GetChecksum(ioChecksum);
}

// Returns the tile under a screen point and its coordinates (in orthographic view, where the ray meets z 0)
EETile* EEMap::GetTileAt(const GE2DPoint& inPoint, long& outX, long& outY)
{
    return (EETile*)ee_gamecpp_map_tile_at(this, reinterpret_cast<const EeGe2DPoint*>(&inPoint), &outX, &outY);
}

// Gives the ground point at a tile's centre; returns false when there is no such tile
bool EEMap::GetTileLocation(long inX, long inY, GE3DPoint& outLocation)
{
    return ee_gamecpp_map_tile_location(this, inX, inY, reinterpret_cast<EeGe3DPoint*>(&outLocation));
}

// Builds the 3D line through a screen point from the near to the far clipping plane
void EEMap::GetScreenLine(const GE2DPoint& inPoint, GE3DLine& outLine)
{
    ee_gamecpp_map_screen_line(reinterpret_cast<const EeGe2DPoint*>(&inPoint), reinterpret_cast<EeGe3DLine*>(&outLine));
}

// Returns the ground's pitch under a location, from the ground heights 0.65 ahead of and behind it
float EEMap::GetTerrainPitch(const GETransformation& inLocation)
{
    return ee_gamecpp_map_terrain_pitch(this, reinterpret_cast<const EeGeTransformation*>(&inLocation));
}

// Returns the ground's roll under a location, from the ground heights 0.65 to either side of it
float EEMap::GetTerrainRoll(const GETransformation& inLocation)
{
    return ee_gamecpp_map_terrain_roll(this, reinterpret_cast<const EeGeTransformation*>(&inLocation));
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
    ee_gamecpp_map_initialize(this);
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
    case EE_UNITS_MAP_SAVE_TILE_SAVE: return ee_gamecpp_map_save_tile(theEEMap, &theFile, theX, theY);
    case EE_UNITS_MAP_SAVE_PICK_DEFAULT: return ee_gamecpp_map_pick_default(theEEMap, (uintptr_t*)&theContext.mDefault, io->b);
    case EE_UNITS_MAP_SAVE_VALID:
        io->i = theEEMap->mTerrainMesh.GetIsValidTile(theX, theY) ? 1 : 0;
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_TERRAIN_TYPE: return ee_gamecpp_map_save_terrain_type(theEEMap, (uintptr_t*)&theContext.mTerrain, theX, theY, io);
    case EE_UNITS_MAP_SAVE_SPACE:
        io->i = theEEMap->mTerrainMesh.GetIsSpaceTile(theX, theY) ? 1 : 0;
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_SET_TYPE: return ee_gamecpp_map_set_terrain_type(theEEMap, theX, theY, (long)(io->i >> 32));
    case EE_UNITS_MAP_SAVE_TERRAIN_LOAD:
        theContext.mTerrain->Load(theEEMap->mTerrainMesh);
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_SET_DEFAULT: return ee_gamecpp_map_save_set_default(theEEMap, (uintptr_t)theContext.mDefault, theX, theY);
    case EE_UNITS_MAP_SAVE_CLIFF_VALID: io->b = (unsigned long)io->i >= EETerrainTable::GetNumCliffTerrains() || EETerrainTable::sCliffTable[(unsigned long)io->i] == NULL ? 0 : 1;
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_FIRST_CLIFF:
        io->i = gFirstCliffTerrain;
        return kErrorOK;
    case EE_UNITS_MAP_SAVE_FINISH: return ee_gamecpp_map_save_finish(theEEMap);
    }
    return EESavePrimitive(theFile, inOp, io);
}

// Reads the next tile from the saved game (saves up to 0.46 also carry its terrain); returns the tile
EEMapTile* EEMap::LoadTile(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile, long inX,
                            long inY)
{
    return (EEMapTile*)ee_gamecpp_map_load_tile(this, inMajorVersion, inMinorVersion, &inFile, inX, inY);
}

// Writes every tile's state to the out-of-sync dump file
void EEMap::DumpOutOfSynch(FILE* inFile)
{
    ee_gamecpp_map_dump(this, inFile);
}

// Gives a player own or shared sight of the mesh-flagged tiles within a radius; if the local player sees through
// it, updates the mini map and terrain visibility counts too (ee_app map.rs)
void EEMap::AddSight(EEPlayer* inPlayer, float inX, float inY, float inRadius, bool inShared)
{
    EESightScope theCapture(kSightMapAdd, inPlayer, inX, inY, 0.0f, 0.0f, inRadius, inShared);   // TSGT (TEST ONLY)
    ee_gamecpp_map_sight(this, inPlayer, inX, inY, inRadius, inShared, true);
    EEAppCaptureSight(0, inPlayer, inX, inY, inRadius, inShared);   // MAPS (TEST ONLY)
}

// Paints a rectangle of tiles with a terrain, then recolours its vertices with the given colour type (ee_app map.rs)
void EEMap::PaintTerrainRect(long inLeft, long inRight, long inTop, long inBottom, long inTerrain, long inCliff)
{
    EEAppMapScope theScope(this, kAppMapPaintTerrainRect, inLeft, inRight, inTop, inBottom, inTerrain, inCliff, inLeft,
                           inRight, inBottom, inTop);   // MAPC (TEST ONLY)
    ee_gamecpp_map_edit(this, 5, inLeft, inRight, inTop, inBottom, inTerrain, inCliff);
}

// Sets a tile's terrain type and paints it and its lower-left neighbours' textures with the terrain (ee_app map.rs)
void EEMap::PaintTerrain(long inX, long inY, long inTerrain)
{
    EEAppMapScope theScope(this, kAppMapPaintTerrain, inX, inY, inTerrain, 0, 0, 0, inX - 1, inX, inY - 1, inY);   // MAPC (TEST ONLY)
    ee_gamecpp_map_edit(this, 4, inX, inY, inTerrain, 0, 0, 0);
}

// Marks steep tiles in a rectangle as cliffs, texturing them from the cliff terrain by slope direction, then makes
// tiles with three or more cliff neighbours cliffs too (ee_app map.rs)
void EEMap::UpdateCliffs(long inLeft, long inRight, long inBottom, long inTop)
{
    EEAppMapScope theScope(this, kAppMapUpdateCliffs, inLeft, inRight, inBottom, inTop, 0, 0, inLeft - 1, inRight,
                           inBottom - 1, inTop);   // MAPC (TEST ONLY)
    ee_gamecpp_map_edit(this, 2, inLeft, inRight, inBottom, inTop, 0, 0);
}

// Turns cliff tiles in a rectangle that are no longer steep back into the map's base terrain (ee_app map.rs)
void EEMap::RemoveFlatCliffs(unsigned long inLeft, unsigned long inRight, unsigned long inBottom, unsigned long inTop)
{
    EEAppMapScope theScope(this, kAppMapRemoveFlatCliffs, (long)inLeft, (long)inRight, (long)inBottom, (long)inTop, 0, 0,
                           (long)inLeft - 1, (long)inRight, (long)inBottom - 1, (long)inTop);   // MAPC (TEST ONLY)
    ee_gamecpp_map_edit(this, 3, (long)inLeft, (long)inRight, (long)inBottom, (long)inTop, 0, 0);
}

// Registers every terrain's texture set and every terrain type's colour set with the terrain mesh, then
// the water state and the colour table
void EEMap::RegisterTerrainSets()
{
    ee_gamecpp_map_register_terrain_sets(this);
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
    ee_gamecpp_map_set_tile_terrain(this, inX, inY, inTextureSet, inUnload);
}

// Returns the number of tiles: the square map minus its two cut-off corner triangles (ee_app map.rs)
unsigned long EEMap::GetTileCount()
{
    return ee_gamecpp_map_tile_count(this);
}

// Returns whether the other player is this player or neither of the two is at war with the other
// (out of line after its first user here)
bool EEPlayer::IsAlliedWith(EEPlayer* inPlayer)
{
    return inPlayer == this || this->mEnemies[inPlayer->mPlayerID] == 0 && inPlayer->mEnemies[this->mPlayerID] == 0;
}

// TSGT (TEST ONLY): the record of the outermost sight scope (ee/EESightCapture.h) for the world's map, with the tile's
// index in the map's tile array (-1: not one of them)
void EESightRecord(unsigned int inShape, unsigned int inKind, EEPlayer* inPlayer, const RTSTile* inTile, long inTileX,
                   long inTileY, unsigned long inValue, const float* inArgs)
{
    ee_gamecpp_map_sight_record(inShape, inKind, (uint64_t)(uintptr_t)inPlayer, (uint64_t)(uintptr_t)inTile, inTileX, inTileY, inValue, inArgs);
}

// ------------------------------------------------------------------------- round 9, lane B: the map's bodies' C++

// the mini map's cases (EEMiniMap.cpp), then lane C's
int64_t EECore9OpB3(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore9OpB3F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

static_assert(kUnitTypeCameraObject == 0x167, "gamecpp map.rs");
static_assert(sizeof(GE3DLine) == sizeof(EeGe3DLine) && sizeof(GE2DPoint) == sizeof(EeGe2DPoint), "gamecpp map.rs: the values by address");

#define P64(x) ((int64_t)(intptr_t)(x))
#define MPPT(i) (*(GE3DPoint*)a[i])
#define M ((EEMap*)p)

// The C++ calls and members of the map's bodies in Rust (gamecpp map.rs; EeGameCpp's lane B section, after
// EEEffectHost.cpp's EECore9OpB): one statement each
int64_t EECore9OpB2(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the map's records, members and mesh
    case O::MpRts: return P64(static_cast<EeUnitsRtsMapFields*>(M));
    case O::MpFields: return P64(static_cast<EeUnitsMapFields*>(M));
    case O::MpWorldMap: return P64(gWorld.mMap);
    case O::MpUnitRegionsFree: ee_combat_regions_free(M->mUnitRegions);
        break;
    case O::MpEffectRegionsFree: ee_gfx_effect_regions_free(M->mEffectRegions);
        break;
    case O::MpSetUnitRegions: M->mUnitRegions = (EeCombatUnitRegions*)a[0];
        break;
    case O::MpSetEffectRegions: M->mEffectRegions = (EeGfxEffectRegions*)a[0];
        break;
    case O::MpDeleteTiles: delete[] M->mTileArray;
        break;
    case O::MpTileArray: return P64(M->mTileArray);
    case O::MpSetTiles: M->mTileArray = (EETile*)a[0];
        break;
    case O::MpNewTiles: M->mTileArray = new EETile[a[0]];
        break;
    case O::MpWords: return P64(M->mVisibilityWords);
    case O::MpSetWords: M->mVisibilityWords = (uint32_t*)a[0];
        break;
    case O::MpTileAt: return P64(&M->mTileArray[a[0]]);
    case O::MpTileSetVisibility: ((EETile*)a[0])->mVisibility = &M->mVisibilityWords[a[1]];
        break;
    case O::MpTileSize: return sizeof(EETile);
    case O::MpRtsTile: return P64(M->mTiles[a[0]]);
    case O::MpGetTile: return P64(M->GetTile((unsigned long)a[0], (unsigned long)a[1]));
    case O::MpGroundPoint: M->GetGroundPoint(MPPT(0), MPPT(1));
        break;
    case O::MpRtsCreate: M->Create((long)a[0], (long)a[1], (unsigned long)a[2], true);
        break;
    case O::MpTileChecksum: return ((EETile*)p)->GetChecksum((unsigned long)a[0]);
    case O::MpSightEnabled: return gSightUpdatesEnabled == true;
    case O::MpTileVecNew: return P64(new std::vector<EETileCoordinate>);
    case O::MpTileVecDelete: delete (std::vector<EETileCoordinate>*)p;
        break;
    case O::MpUpdateCliffs: M->UpdateCliffs((long)a[0], (long)a[1], (long)a[2], (long)a[3]);
        break;
    case O::MpSeedRandom: M->mRandom.SetSeed(TMGetTime());
        break;
    case O::MpGlobalWaterSet: return M->mTerrainMesh.GetGlobalWaterSetID();
    case O::MpTextureSetId: return M->mTerrainMesh.GetTextureSetID((long)a[0], (long)a[1]);
    case O::MpValidTile: return M->mTerrainMesh.GetIsValidTile((long)a[0], (long)a[1]);
    case O::MpSetTextureSet: M->mTerrainMesh.SetTextureSet((long)a[0], (long)a[1], (unsigned long)a[2]);
        break;
    case O::MpMeshRust: return P64(M->mTerrainMesh.GetRust());
    case O::MpClosestTileToPoint: M->mTerrainMesh.GetClosestTile(MPPT(0), *(long*)a[1], *(long*)a[2]);
        break;
    case O::MpClosestTileToLine: M->mTerrainMesh.GetClosestTile(*(GE3DLine*)a[0], *(long*)a[1], *(long*)a[2]);
        break;
    // the tiles
    case O::MpTileSetHeight: ((EETile*)p)->mHeight = f[0];
        break;
    case O::MpTileLoad: ((EETile*)p)->Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2], (unsigned long)a[3]);
        break;
    case O::MpTileSave: return ((EETile*)p)->Save(*(FSFile*)a[0], (unsigned long)a[1]);
    case O::MpTileDump: ((EETile*)p)->Dump((FILE*)a[0], (unsigned long)a[1]);
        break;
    case O::MpTileSetTerrainType: M->GetTile((unsigned long)a[0], (unsigned long)a[1])->SetTerrainType((long)a[2], EETileLocation((long)a[0], (long)a[1]), M);
        break;
    case O::MpFileReadUlong: return ((FSFile*)p)->Read(*(unsigned long*)a[0]);
    case O::MpDumpTitle: fprintf((FILE*)p, "OUT OF SYNCH DUMP: ************************************************* MAP "
                                           "*********************************\n\n");
        break;
    case O::MpDumpTileTitle: fprintf((FILE*)p, "Tile X(%ld), Y(%ld)\n", (long)a[0], (long)a[1]);
        break;
    case O::MpDumpNewLine: fprintf((FILE*)p, "\n");
        break;
    // the terrain tables
    case O::MpDefaultTerrain: return gDefaultTerrain;
    case O::MpTerrainCount: return (int64_t)EETerrainTable::sTable.size();
    case O::MpTerrainGet: return P64(EETerrainTable::Get((long)a[0]));
    case O::MpTerrainExists: return EETerrainTable::Get((long)a[0]) != NULL;
    case O::MpTerrainTextureSet: return EETerrainTable::Get((long)a[0])->mTextureSetID;
    case O::MpTerrainLoad: EETerrainTable::Get((long)a[0])->Load(M->mTerrainMesh);
        break;
    case O::MpTerrainUnload: EETerrainTable::Get((long)a[0])->Unload(M->mTerrainMesh);
        break;
    case O::MpTerrainTextureSetOf: return ((EETerrainTable*)p)->mTextureSetID;
    case O::MpTerrainTypeOf: return ((EETerrainTable*)p)->mTerrainType;
    case O::MpTerrainColorType: return ((EETerrainTable*)p)->mColorType;
    case O::MpTerrainIsWater: return ((EETerrainTable*)p)->mIsWater == true;
    case O::MpTerrainWaterPeriod: return ((EETerrainTable*)p)->mWaterAnimationPeriod;
    case O::MpTerrainAttach: ((EETerrainTable*)p)->AttachTextures((GETextureSet*)a[0]);
        break;
    case O::MpTerrainSetTextureSet: ((EETerrainTable*)p)->mTextureSetID = (unsigned long)a[0];
        break;
    case O::MpNewTextureSet: return P64(new GETextureSet(((EETerrainTable*)p)->mColumns, ((EETerrainTable*)p)->mRows, ((EETerrainTable*)p)->mTextureTileCoverage));
    case O::MpRegisterTextureSet: return M->mTerrainMesh.RegisterTextureSet((GETextureSet*)a[0], *(unsigned long*)a[1]);
    case O::MpTypeCount: return EETerrainTypeTable::GetCount();
    case O::MpTypeGet: return P64(EETerrainTypeTable::Get((unsigned long)a[0]));
    case O::MpTypeFirstColor: return ((EETerrainTypeTable*)p)->mFirstColor;
    case O::MpTypeLastColor: return ((EETerrainTypeTable*)p)->mLastColor;
    case O::MpTypeSetColorSet: ((EETerrainTypeTable*)p)->mColorSetID = (unsigned long)a[0];
        break;
    case O::MpTypeColorSet: return EETerrainTypeTable::Get((long)a[0])->mColorSetID;
    case O::MpColorSetNew: return P64(new std::vector<unsigned long>);
    case O::MpColorSetDelete: delete (std::vector<unsigned long>*)p;
        break;
    case O::MpColorSetClear: ((std::vector<unsigned long>*)p)->clear();
        break;
    case O::MpColorSetPush: ((std::vector<unsigned long>*)p)->push_back((unsigned long)a[0]);
        break;
    case O::MpRegisterColorSet: return M->mTerrainMesh.RegisterColorSet(*(std::vector<unsigned long>*)a[0], *(unsigned long*)a[1]);
    case O::MpColorCount: return EETerrainColorTable::GetCount();
    case O::MpColorExists: return EETerrainColorTable::Get((unsigned long)a[0]) != NULL;
    case O::MpColorsNew: return P64(new std::vector<GEColor>);
    case O::MpColorsDelete: delete (std::vector<GEColor>*)p;
        break;
    case O::MpColorsPush: ((std::vector<GEColor>*)p)->push_back(EETerrainColorTable::Get((unsigned long)a[0])->mColor);
        break;
    case O::MpSetWaterState: M->mTerrainMesh.SetGlobalWaterState((unsigned long)a[0], (unsigned long)a[1], (unsigned long)a[2], f[0]);
        break;
    case O::MpSetColorTable: M->mTerrainMesh.SetColorTable(*(std::vector<GEColor>*)a[0]);
        break;
    case O::MpCliffFirst: return EETerrainTable::sCliffTable[a[0]]->mFirstTerrain;
    case O::MpCliffLast: return EETerrainTable::sCliffTable[a[0]]->mLastTerrain;
    // the units, the players, the mini map
    case O::MpUnitGetClass: return P64(((EEUnit*)p)->GetUnitClass());
    case O::MpPlaceOnGround: ((EEComplexUnit*)p)->PlaceOnGround(MPPT(0));
        break;
    case O::MpLocalPlayer: return P64(::gWorld.mPlayers[::gWorld.mCurrentPlayer]);
    case O::MpPlAlliedWith: return ((EEPlayer*)p)->IsAlliedWith((EEPlayer*)a[0]);
    case O::MpPlSharedLos: return ((EEPlayer*)p)->mSharedLOS;
    case O::MpAddSharedSight: ((EEPlayer*)p)->AddSharedSightAt(EETileLocation((long)a[0], (long)a[1]));
        break;
    case O::MpRemoveSharedSight: ((EEPlayer*)p)->RemoveSharedSightAt(EETileLocation((long)a[0], (long)a[1]));
        break;
    case O::MpAddOwnSight: ((EEPlayer*)p)->AddOwnSightAt(EETileLocation((long)a[0], (long)a[1]));
        break;
    case O::MpRemoveOwnSight: ((EEPlayer*)p)->RemoveOwnSightAt(EETileLocation((long)a[0], (long)a[1]));
        break;
    case O::MpTileVecClear: ((std::vector<EETileCoordinate>*)p)->clear();
        break;
    case O::MpTileVecData: return P64(((std::vector<EETileCoordinate>*)p)->empty() ? NULL : &(*(std::vector<EETileCoordinate>*)p)[0].mX);
    case O::MpTileVecSize: return (int64_t)((std::vector<EETileCoordinate>*)p)->size();
    case O::MpCountCellOn: gMiniMap->CountCellOn((long)a[0], (long)a[1]);
        break;
    case O::MpCountCellOff: gMiniMap->CountCellOff((long)a[0], (long)a[1]);
        break;
    // the screen line
    case O::MpOrthographic: return gOrthographicView;
    case O::MpDenormalizePerspective: gViewport.DenormalizePerspectivePoint(MPPT(0), f[0], MPPT(1));
        break;
    case O::MpDenormalizeOrthographic: gViewport.DenormalizeOrthographicPoint(MPPT(0), MPPT(1));
        break;
    case O::MpLineDefine: ((GE3DLine*)a[0])->Define(MPPT(1), MPPT(2));
        break;
    case O::MpLineSolveForZ: return ((GE3DLine*)a[0])->SolveForZ(f[0], *(float*)a[1], MPPT(2));
    default: return EECore9OpB3(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpB2F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::MpMeshZ: return M->mTerrainMesh.GetMeshZ(f[0], f[1]);
    case O::MpTerrainWaterDepth: return ((EETerrainTable*)p)->mWaterDepth;
    case O::MpViewHalfWidth: return gViewport.GetHalfWidth();
    case O::MpViewXCenter: return gViewport.GetXCenter();
    case O::MpViewYCenter: return gViewport.GetYCenter();
    case O::MpViewFrontClip: return gViewport.GetFrontClippingDistance();
    case O::MpViewRearClip: return gViewport.GetRearClippingDistance();
    default: return EECore9OpB3F(inOp, p, a, f);
    }
}
