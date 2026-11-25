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
#include "lle/GESurface.h"
#include "lle/GETexture.h"
#include "lle/Graphics Engine.h"
#include "lle/GERenderState.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEColorTable.h"
#include "ee/EEGameUI.h"
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

// Creates an empty map on the default terrain
EEMap::EEMap()
    : RTSMap(kMapTileSize), FSPersistent(kPersistentMap), mRandom(1)
{
    ee_gamecpp_mx_construct_empty(this);
}

// Creates a map of the given size and player count, filled with the given terrain
EEMap::EEMap(long inWidth, long inHeight, unsigned long inPlayerCount, long inTerrain)
    : RTSMap(kMapTileSize), FSPersistent(kPersistentMap), mRandom(1)
{
    ee_gamecpp_mx_construct(this, inWidth, inHeight, inPlayerCount, inTerrain);
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
    ee_gamecpp_mx_update_water(this, inX, inY);   // (with MAPC, TEST ONLY)
}

// Builds a new map of the given size: fresh unit and effect region tables, every tile on the base terrain (the fill:
// ee_app map.rs). The original assigned the terrain mesh to itself here, a no-op; the mesh is a Rust object now.
void EEMap::CreateMap(long inWidth, long inHeight, unsigned long inPlayerCount)
{
    ee_gamecpp_mx_create_map(this, inWidth, inHeight, inPlayerCount);   // (with MAPC, TEST ONLY)
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
    ee_gamecpp_mx_sight(this, inPlayer, inX, inY, inRadius, inShared, false);   // (with TSGT and MAPS, TEST ONLY)
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
    return (EETile*)ee_gamecpp_mx_pick_tile(this, reinterpret_cast<const EeGe2DPoint*>(&inPoint), &outX, &outY, inHitWaterSurface);
}

// Returns the tile and the 3D point hit by the ray through a screen point, optionally at the water surface
EETile* EEMap::GetLocationAt(const GE2DPoint& inPoint, GE3DPoint& outLocation, bool inHitWaterSurface)
{
    return (EETile*)ee_gamecpp_mx_location_at(this, reinterpret_cast<const EeGe2DPoint*>(&inPoint), reinterpret_cast<EeGe3DPoint*>(&outLocation), inHitWaterSurface);
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
    return ee_gamecpp_mx_load(this, &inFile, inMajorVersion, inMinorVersion);   // port: ee_units map_save.rs
}

// The C++ parts of the maps' Load and Save (ee/EEMap.h; RTSMap.cpp's too): gamecpp map_more.rs over EECore9OpB2's round 11
// cases
int32_t EEMapSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    return ee_gamecpp_mx_save_op(inContext, inOp, inIndex, io);
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
    ee_gamecpp_mx_sight(this, inPlayer, inX, inY, inRadius, inShared, true);   // (with TSGT and MAPS, TEST ONLY)
}

// Paints a rectangle of tiles with a terrain, then recolours its vertices with the given colour type (ee_app map.rs)
void EEMap::PaintTerrainRect(long inLeft, long inRight, long inTop, long inBottom, long inTerrain, long inCliff)
{
    ee_gamecpp_mx_paint_rect(this, inLeft, inRight, inTop, inBottom, inTerrain, inCliff);   // (with MAPC, TEST ONLY)
}

// Sets a tile's terrain type and paints it and its lower-left neighbours' textures with the terrain (ee_app map.rs)
void EEMap::PaintTerrain(long inX, long inY, long inTerrain)
{
    ee_gamecpp_mx_paint(this, inX, inY, inTerrain);   // (with MAPC, TEST ONLY)
}

// Marks steep tiles in a rectangle as cliffs, texturing them from the cliff terrain by slope direction, then makes
// tiles with three or more cliff neighbours cliffs too (ee_app map.rs)
void EEMap::UpdateCliffs(long inLeft, long inRight, long inBottom, long inTop)
{
    ee_gamecpp_mx_update_cliffs(this, inLeft, inRight, inBottom, inTop);   // (with MAPC, TEST ONLY)
}

// Turns cliff tiles in a rectangle that are no longer steep back into the map's base terrain (ee_app map.rs)
void EEMap::RemoveFlatCliffs(unsigned long inLeft, unsigned long inRight, unsigned long inBottom, unsigned long inTop)
{
    ee_gamecpp_mx_remove_flat_cliffs(this, inLeft, inRight, inBottom, inTop);   // (with MAPC, TEST ONLY)
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
    return ee_gamecpp_mx_save(this, &outFile, inMajorVersion, inMinorVersion);   // port: ee_units map_save.rs
}

// Switches the map to another cliff terrain and re-runs cliff detection over the whole map
void EEMap::SetCliffTexture(long inCliffTexture)
{
    ee_gamecpp_mx_set_cliff_texture(this, inCliffTexture);   // (with MAPC, TEST ONLY; a bad index: the first cliff terrain)
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

// ------------------------------------------------------------------------- round 9, lane B: the map's bodies' C++


// (its constants are checked against the headers by gamecpp cpp_consts_tests.rs)
static_assert(sizeof(GE3DLine) == sizeof(EeGe3DLine) && sizeof(GE2DPoint) == sizeof(EeGe2DPoint), "gamecpp map.rs: the values by address");

#define P64(x) ((int64_t)(intptr_t)(x))
#define MPPT(i) (*(GE3DPoint*)a[i])
#define M ((EEMap*)p)
#define MM ((EEMiniMap*)p)

// round 11, lane A: gamecpp map_more.rs' constants and the save's context

static_assert(kAppMapUpdateCliffs == 1 && kAppMapRemoveFlatCliffs == 2 && kAppMapPaintTerrain == 3 && kAppMapPaintTerrainRect == 4 &&
                  kAppMapUpdateWater == 6 && kAppMapCreateMap == 7 && kAppMapSetCliffTexture == 8 && kSightMapAdd == 8 && kSightMapRemove == 9,
              "gamecpp map_more.rs");
static_assert(offsetof(EEMapSaveContext, mDefault) == 2 * sizeof(void*) + 2 * sizeof(unsigned long) && sizeof(EEMapSaveContext) == 4 * sizeof(void*) + 2 * sizeof(unsigned long),
              "gamecpp map_more.rs EeGamecppMxSaveContext");

// The C++ calls and members of the map's bodies in Rust (gamecpp map.rs; EeGameCpp's lane B section, after
// EEEffectHost.cpp's EECore9OpB): one statement each
int64_t EECore9OpB5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
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
    case O::MpRtsCreate: M->Create((long)a[0], (long)a[1], (unsigned long)a[2], true);
        break;
    case O::MpSightEnabled: return gSightUpdatesEnabled == true;
    case O::MpTileVecNew: return P64(new std::vector<EETileCoordinate>);
    case O::MpTileVecDelete: delete (std::vector<EETileCoordinate>*)p;
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
    // round 11, lane A (gamecpp map_more.rs)
    case O::MxScopeEnter: return EEAppMapScopeEnter();
    case O::MxScopeLeave: EEAppMapScopeLeave(M, a[0] != 0, (unsigned int)a[1], (const long*)a[2], (const long*)a[3]);
        break;
    case O::MxCliffCount: return EETerrainTable::GetNumCliffTerrains();
    case O::MxCliffExists: return EETerrainTable::sCliffTable[(unsigned long)a[0]] != NULL;
    case O::MxFirstCliff: return gFirstCliffTerrain;
    case O::MxCaptureSight: EEAppCaptureSight((unsigned int)a[0], (EEPlayer*)p, f[0], f[1], f[2], a[1] != 0);
        break;
    case O::MxTileOnLine: return P64(M->GetTileOnLine(*(const GE3DLine*)a[0], *(long*)a[1], *(long*)a[2], a[3] != 0));
    case O::MxLocationOnLine: return P64(M->GetLocationOnLine(*(const GE3DLine*)a[0], *(GE3DPoint*)a[1], a[2] != 0));
    case O::MxRtsOf: return P64(static_cast<RTSMap*>(M));
    case O::MxEEMapOf: return P64(static_cast<EEMap*>((RTSMap*)p));
    case O::MxCreate: ((RTSMap*)p)->Create((long)a[0], (long)a[1], (unsigned long)a[2], false);
        break;
    case O::MxLoadTile: return P64(((RTSMap*)p)->LoadTile((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2], (long)a[3], (long)a[4]));
    case O::MxSetRtsTile: ((RTSMap*)p)->mTiles[a[0]] = (EEMapTile*)a[1];
        break;
    case O::MxMeshLoad: return ((RTSMap*)p)->mTerrainMesh.Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]);
    case O::MxMeshSave: return ((RTSMap*)p)->mTerrainMesh.Save(*(FSFile*)a[0]);
    case O::MxSpaceTile: return M->mTerrainMesh.GetIsSpaceTile((long)a[0], (long)a[1]);
    case O::MxTerrainLoadInto: ((EETerrainTable*)p)->Load(((EEMap*)a[0])->mTerrainMesh);
        break;
    case O::MxPrimitive: return EESavePrimitive(*(FSFile*)p, (int32_t)a[0], (EeUnitsPlayerSaveIo*)a[1]);
    case O::MmTail: return P64(&MM->mL12174);
    case O::MmRust: return P64(MM->mRust);
    case O::MmSetRust: MM->mRust = (EeAppMiniMap*)a[0];
        break;
    case O::MmTransforms: return P64(&MM->mMapToPixel);
    case O::MmCorners: return P64(&MM->mCorner0);
    case O::MmSetPixels: MM->mPixels = (unsigned short*)a[0];
        break;
    case O::MmSetPixelsPerRow: MM->mPixelsPerRow = (long)a[0];
        break;
    case O::MmUnitLock: MM->mUnitSurface->Lock();
        break;
    case O::MmUnitUnlock: MM->mUnitSurface->Unlock();
        break;
    case O::MmUnitMemory: return P64(MM->mUnitSurface->GetMemoryAddress());
    case O::MmUnitBytesPerRow: return MM->mUnitSurface->GetBytesPerRow();
    case O::MmUnitCopyToWorld: MM->mUnitSurface->CopyTo(MM->mWorldTexture->GetSurface(), *(GERectangle*)a[0], *(GERectangle*)a[0], false);
        break;
    case O::MmUnitClear: MM->mUnitSurface->Clear(*(GEColor*)a[0], 1.0f);
        break;
    case O::MmTerrainEnter: MM->mTerrainCriticalSection->Enter();
        break;
    case O::MmTerrainLeave: MM->mTerrainCriticalSection->Leave();
        break;
    case O::MmTerrainLock: MM->mTerrainTexture->GetSurface()->Lock();
        break;
    case O::MmTerrainUnlock: MM->mTerrainTexture->GetSurface()->Unlock();
        break;
    case O::MmTerrainMemory: return P64(MM->mTerrainTexture->GetSurface()->GetMemoryAddress());
    case O::MmTerrainBytesPerRow: return MM->mTerrainTexture->GetSurface()->GetBytesPerRow();
    case O::MmUiUpdate: gEEGameUI.mMiniMapUpdate(MM);   // EEUIMiniMap::Update (ee/EEGameUI.h)
        break;
    case O::MmNumPlayers: return gWorld.mNumPlayers;
    case O::MmDoppUnit: return P64(((EEDoppelganger*)p)->mUnit);
    case O::MmDoppPlayer: return P64(((EEDoppelganger*)p)->GetPlayer());
    case O::MmDoppPlayerField: return P64(((EEDoppelganger*)p)->mPlayer);
    case O::MmDoppIsComplex: return ((EEDoppelganger*)p)->IsComplex();
    case O::MmDoppOnTile: return ((EEDoppelganger*)p)->GetTile() == (EETile*)a[0];
    case O::MmPackDark: return GEPackColor(kPixelFormatA4R4G4B4, EEColorTable::GetDarkColor((unsigned long)a[0]), 1.0f);
    case O::MmPackColor2: return GEPackColor(kPixelFormatA4R4G4B4, EEColorTable::GetColor2((unsigned long)a[0]), 1.0f);
    case O::MmPackShade: return GEPackColor(kPixelFormatA4R4G4B4, ((EEPlayer*)p)->mShadeColor, 1.0f);
    case O::MmPackPlayerColor: return GEPackColor(kPixelFormatA4R4G4B4, ((EEPlayer*)p)->mColor, 1.0f);
    case O::MmTileVisibility: return ((EETile*)p)->GetVisibility((unsigned long)a[0]);
    case O::MmTileDoppCount: return ((EETile*)p)->GetNumDoppelgangers();
    case O::MmTileDopp: return P64(((EETile*)p)->mDoppelgangers[a[0]]);
    case O::MmNodeUnit: return P64(((RTSTileUnitNode*)p)->mUnit);
    case O::MmNodeNext: return P64(((RTSTileUnitNode*)p)->mNext);
    case O::MmTerrainColor: *(GEColor*)a[1] = EETerrainTable::Get((long)a[0])->mColor;
        break;
    case O::TlRtsChecksum: return ((const EETile*)p)->RTSTile::GetChecksum((unsigned long)a[0]);
    case O::TlRtsLoad: ((EETile*)p)->RTSTile::Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2], (unsigned long)a[3]);
        break;
    case O::TlRtsSave: return ((EETile*)p)->RTSTile::Save(*(FSFile*)a[0], (unsigned long)a[1]);
    case O::TlRtsDump: ((const EETile*)p)->RTSTile::Dump((FILE*)a[0], (unsigned long)a[1]);
        break;
    case O::TlLoadImage: return P64(gDoppelgangerManager.Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]));
    case O::TlImageSetTile: ((EEDoppelganger*)p)->mTile = (EETile*)a[0];
        break;
    case O::TlImageValid: return ((EEDoppelganger*)p)->GetIsValid();
    case O::TlImageTileNull: return ((EEDoppelganger*)p)->GetTile() == NULL;
    case O::TlImageSave: return ((EEDoppelganger*)p)->Save(0, 0, *(FSFile*)a[0]);
    case O::TlEraseImage: ((EETile*)p)->mDoppelgangers.erase(((EETile*)p)->mDoppelgangers.begin() + a[0]);
        break;
    case O::TlDumpImage: fprintf((FILE*)p, "Doppelganger %ld has unit id of %ld\n", (unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::TlWriteUlong: return ((FSFile*)p)->Write((unsigned long)a[0]);
    case O::TlDynMap: return P64(dynamic_cast<EEMap*>((RTSMap*)p));
    case O::TlSetIsSpace: ((EEMap*)p)->mTerrainMesh.SetIsSpace((unsigned long)a[0], (unsigned long)a[1], a[2] != 0);
        break;
    case O::TlValidPoint: return ((EEMap*)p)->mTerrainMesh.GetIsValidPoint((unsigned long)a[0], (unsigned long)a[1]);
    case O::TlSetTerrainTypeField: ((EETile*)p)->mTerrainType = (long)a[0];
        break;
    default: return EECore9OpB5(inOp, p, a, f);
    }
    return 0;
}

