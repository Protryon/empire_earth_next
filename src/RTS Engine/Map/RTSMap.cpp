// C:\EEX\RTS Engine\Map\RTSMap.cpp (__FILE__ 0x4befc8). The engine-side map: 0x121c78..0x1240b3 (the three
// "RTSMap-..." key chains, the ctor, ??_G, the dtor and the members in definition order, then the template COMDATs of
// vector<RTSMapVertexColors> and RTSTile::SetVisibility's out-of-line copy). Unit flags: the exe's module set.
#include <windows.h>
#include <math.h>
#include <vector>
#include <utility>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEDoppelgangerManager.h"
#include "rust/sim/units.h"

// chain at 0x521c78 (wrapper)
//   object 0x929840  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ (guard 0x929838 bit 1)

// chain at 0x521cbb (wrapper)
//   object 0x929828  (guard 0x929838 bit 2)

// chain at 0x521cfe (wrapper)
//   object 0x929818  (guard 0x929838 bit 4)

// The terrain edits (Raise, its spreads and smoothing, the tiles' upkeep, GetNumRaisedCorners, IsTileFlat) are ee_ge
// map_raise.rs' over the mesh's Rust object; the map's tiles and water are asked here
namespace {
struct EEMapRaiseOps {
    static bool HasTile(void* inMap, unsigned long inX, unsigned long inY)
    {
        return static_cast<RTSMap*>(inMap)->GetTile(inX, inY) != NULL;
    }
    // the tile's height, then each of its units back on the ground (UpdateTiles 0x1225fa)
    static void SettleTile(void* inMap, unsigned long inX, unsigned long inY, float inHeight)
    {
        RTSMap* theMap = static_cast<RTSMap*>(inMap);
        EEMapTile* theTile = theMap->GetTile(inX, inY);
        theTile->mHeight = inHeight;
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext)
            theMap->PlaceUnitOnGround(theNode->mUnit);
    }
    static void UpdateWater(void* inMap, long inX, long inY) { static_cast<RTSMap*>(inMap)->UpdateWater(inX, inY); }
    static void* Tile(void* inMap, unsigned long inX, unsigned long inY) { return static_cast<RTSMap*>(inMap)->GetTile(inX, inY); }
    static EeGeMapHost Of(RTSMap* inMap) { return {inMap, inMap->mTerrainMesh.GetRust(), HasTile, SettleTile, UpdateWater, Tile}; }
    // the host of a call's arguments (the temporary lives to the end of the call)
    static const EeGeMapHost* Ptr(const EeGeMapHost& inHost) { return &inHost; }
};
}

// Counts the tile's corners at least 0.4 above its lowest corner (0 off the map)
long RTSMap::GetNumRaisedCorners(unsigned long inX, unsigned long inY)
{
    return ee_ge_map_raised_corners(EEMapRaiseOps::Ptr(EEMapRaiseOps::Of(this)), inX, inY);
}

// Turns every explored but unseen tile back to unexplored for each player but player 0, then unregisters
// those players from the doppelganger manager (done before a scenario is saved)
void EEMap::PrepareSave()
{
    EESightScope theCapture(kSightPrepareSave, NULL);   // TSGT (TEST ONLY)
    ee_units_tile_sight_map((uint64_t)(uintptr_t)&::gWorld, (uint64_t)(uintptr_t)this, 2, 0);   // ee_units tile_sight.rs
    for (unsigned long i = 1; i < this->mU19561C; i++)
        ee_units_dm_body(EE_UNITS_DM_DO_UNREGISTER_PLAYER, ::gWorld.mPlayers[i], 0, 0, 0);
}

// Returns the tile closest to a point and stores its coordinates
EEMapTile* EEMap::GetClosestTile(GE3DPoint* inPoint, long* ioX, long* ioY)
{
    this->mTerrainMesh.GetClosestTile(*inPoint, *ioX, *ioY);
    return this->GetTile(*ioX, *ioY);
}

// Returns whether a tile's corner heights differ by at most 0.7 times the map's scale value (false off the map)
bool RTSMap::IsTileFlat(unsigned long inX, unsigned long inY)
{
    return ee_ge_map_tile_flat(EEMapRaiseOps::Ptr(EEMapRaiseOps::Of(this)), this->mF1955F4, inX, inY);
}

// Returns whether the tiles a radius's diagonal away left, right, below and above a tile are all on the map
bool EEMap::IsAreaOnMap(unsigned long inX, unsigned long inY, unsigned long inRadius)
{
    return ee_ge_map_area_on_map(EEMapRaiseOps::Ptr(EEMapRaiseOps::Of(this)), inX, inY, inRadius);
}

// Returns the tile where a line meets the terrain, or the water surface if asked and it comes first, and stores its
// coordinates; NULL if nothing is hit (ee_ge map_raise.rs)
EEMapTile* RTSMap::GetTileOnLine(const GE3DLine& inLine, long& outX, long& outY, bool inHitWaterSurface)
{
    return static_cast<EEMapTile*>(ee_ge_map_tile_on_line(EEMapRaiseOps::Ptr(EEMapRaiseOps::Of(this)), reinterpret_cast<const EeGe3DLine*>(&inLine), &outX,
                                                          &outY, inHitWaterSurface));
}

// Returns the tile where a line meets the terrain, or the water surface if asked and it comes first, and stores the
// hit point; NULL if nothing is hit (ee_ge map_raise.rs)
EEMapTile* RTSMap::GetLocationOnLine(const GE3DLine& inLine, GE3DPoint& outLocation, bool inHitWaterSurface)
{
    return static_cast<EEMapTile*>(ee_ge_map_location_on_line(EEMapRaiseOps::Ptr(EEMapRaiseOps::Of(this)), reinterpret_cast<const EeGe3DLine*>(&inLine),
                                                              reinterpret_cast<EeGe3DPoint*>(&outLocation),
                                                              inHitWaterSurface));
}

// Reads the map's size and player count, recreates the map and loads its tiles and terrain mesh; returns the
// first read error (tiles get x = row start + column, which only old saves' terrain lookup uses)
long RTSMap::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEMapSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion, NULL, NULL};   // port: ee_units map_save.rs
    return ee_units_rts_map_load(&theContext, &EEMapSaveOperation, static_cast<EeUnitsRtsMapFields*>(this));
}

// Raises or smooths the terrain of a rectangle, spreads its edges, recomputes normals and shorelines and
// returns the changed rectangle through the in/out bounds
void RTSMap::Raise(long* ioLeft, long* ioRight, long* ioTop, long* ioBottom, float inHeight, float inSlope,
                   float inEdgeHeight, float inEdgeSlope, bool inRaise)
{
    ee_ge_map_raise_bounds(EEMapRaiseOps::Ptr(EEMapRaiseOps::Of(this)), this, ioLeft, ioRight, ioTop, ioBottom, inHeight,
                           inSlope, inEdgeHeight, inEdgeSlope, inRaise);
}

// Fogs every tile the player has seen or explored: it becomes explored but not in sight
void RTSMap::FogSeen(EEPlayer* inPlayer)
{
    EESightScope theCapture(kSightFogSeen, inPlayer);   // TSGT (TEST ONLY)
    ee_units_tile_sight_map((uint64_t)(uintptr_t)&::gWorld, (uint64_t)(uintptr_t)this, 0, (uint64_t)(uintptr_t)inPlayer);
}

// Marks every unexplored tile as explored for the player (reveals the map)
void RTSMap::ExploreUnseen(EEPlayer* inPlayer)
{
    EESightScope theCapture(kSightExploreUnseen, inPlayer);   // TSGT (TEST ONLY)
    ee_units_tile_sight_map((uint64_t)(uintptr_t)&::gWorld, (uint64_t)(uintptr_t)this, 1, (uint64_t)(uintptr_t)inPlayer);
}

// Writes the map's size, player count, every tile and the terrain mesh; returns the first write error
long RTSMap::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEMapSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion, NULL, NULL};   // port: ee_units map_save.rs
    return ee_units_rts_map_save(&theContext, &EEMapSaveOperation, static_cast<EeUnitsRtsMapFields*>(this));
}

// Rebuilds the outline runs of continent areas: every rectangle edge not covered by another area of the same
// continent, split on space maps into the runs on the continent (ee_ge map_raise.rs area_outlines)
void RTSMap::BuildAreaOutlines(std::vector<U2DRectangle<long> >& inAreas, std::vector<unsigned short>& inContinents)
{
    struct Ops {
        static unsigned short Continent(void*, long inX, long inY)
        {
            EETileCoordinate theTile;
            theTile.mX = inX;
            theTile.mY = inY;
            return gContinentManager.GetContinent(theTile, kAIPathNonSpace);
        }
        static void Run(void* inMap, const long* inRun)
        {
            static_cast<RTSMap*>(inMap)->mU1955DC.push_back(
                RTSMapTileRun(U2DPoint<long>(inRun[0], inRun[1]), U2DPoint<long>(inRun[2], inRun[3])));
        }
    };
    static_assert(sizeof(U2DRectangle<long>) == sizeof(EeGeAreaRect), "an area is its four longs");
    this->ClearU1955DC();
    ee_ge_map_area_outlines(this, reinterpret_cast<const EeGeAreaRect*>(inAreas.data()), inContinents.data(),
                            inAreas.size(), this->mTerrainMesh.GetIsSpaceMap(), Ops::Continent, Ops::Run);
}

// 0x52988c: the map's tile size (written by the ctor, read by EETile::Virtual14; was RTSVisibleMap.cpp's .bss)
float gMapTileSize;

// The shims map.rs calls (MapCpp; LLE round 8)
extern "C" {
EeUnitsRtsMapFields* ee_rtsmapcpp_fields(RTSMap* inMap) { return inMap; }
void** ee_rtsmapcpp_lock_slot(RTSMap* inMap) { return reinterpret_cast<void**>(&inMap->mCriticalSectionForColoredRectangles); }
long* ee_rtsmapcpp_l1955ec_slot(RTSMap* inMap) { return &inMap->mL1955EC; }
void ee_rtsmapcpp_set_tile_size(float inSize) { gMapTileSize = inSize; }
EEMapTile*** ee_rtsmapcpp_tiles_slot(RTSMap* inMap) { return &inMap->mTiles; }
EEMapTile** ee_rtsmapcpp_tiles_new(long inCount) { return new EEMapTile*[inCount]; }
void ee_rtsmapcpp_delete_tiles(RTSMap* inMap) { inMap->DeleteTiles(); }
void ee_rtsmapcpp_reserve_tiles(RTSMap* inMap, unsigned long inCount) { inMap->ReserveTiles(inCount); }
EEMapTile* ee_rtsmapcpp_create_tile(RTSMap* inMap, float inHeight) { return inMap->CreateTile(inHeight); }
void ee_rtsmapcpp_create_flat_mesh(RTSMap* inMap, long inWidth, long inHeight) { inMap->mTerrainMesh.CreateFlatMesh(inWidth, inHeight); }
size_t ee_rtsmapcpp_colors_size(RTSMap* inMap) { return inMap->mVertexColors.size(); }
const RTSMapVertexColors* ee_rtsmapcpp_colors_at(RTSMap* inMap, size_t inIndex) { return &inMap->mVertexColors[inIndex]; }
void ee_rtsmapcpp_colors_clear(RTSMap* inMap) { inMap->mVertexColors.clear(); }
bool ee_rtsmapcpp_mesh_valid_point(RTSMap* inMap, long inX, long inY) { return inMap->mTerrainMesh.GetIsValidPoint(inX, inY); }
void ee_rtsmapcpp_mesh_set_color(RTSMap* inMap, long inX, long inY, unsigned char inIndex) { inMap->mTerrainMesh.SetColorFromColorTableIndex(inX, inY, inIndex); }
}
