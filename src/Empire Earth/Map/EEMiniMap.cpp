// C:\EEX\Empire Earth\Map\EEMiniMap.cpp (file name inferred: no __FILE__, no asserts). The game's mini map (RTTI
// EEMiniMap > RTSMiniMap, vtable 0x441d44): the class's surfaces, textures, transformations and view trapezoid. The
// cells (their sight counts, building and unit dots, the unit layer's pixels), the terrain pixels' colours and the
// redraw's walk are ee_app minimap.rs (RUST_PLAN R6 "app and world"), over the host below.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <list>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEViewport.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETexture.h"
#include "lle/Graphics Engine.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEColorTable.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMiniMap.h"
#include "ee/EEGameUI.h"
#include "lle/GERenderState.h"
#include "ee/EEAppCapture.h"
#include "rust/sim/units.h"
#include "rust/game/gamecpp.h"

#if defined(__clang__)
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif

extern GEViewport gViewport;                // 0x5308f0 (Empire Earth.cpp)
extern bool& gOrthographicView;              // 0x5308e2 (orthographic camera; EEGraphics)

// chain at 0x51e36e (wrapper)
//   object 0x9297d0  ctor (GEVector's three fldz) 0x11e378
// The terrain light (EEMiniMap's ctor points it at (0, 1.5, 1), normalized) (provisional name)
GEVector gMiniMapLight;

// SetMap's transformations, Refresh's view trapezoid and ProjectToGround are ee_gfx minimap.rs (RUST_PLAN R4.4; its
// constants, .rdata 0x441d38..0x441d60, are there); the GE value classes pass as their #[repr(C)] mirrors. The
// terrain colours' ambient term (gMiniMapAmbient) is ee_app minimap.rs'.
static_assert(sizeof(GE2DPoint) == sizeof(EeGe2DPoint) && sizeof(GETransformation) == sizeof(EeGeTransformation) &&
              sizeof(GEViewport) == sizeof(EeGeViewport), "GE mirrors");
static_assert(sizeof(EEMiniMapDot) == 8 && sizeof(EEMiniMapUnitDot) == 8 + 2 * sizeof(long), "the dots' records");

// Creates the mini map for a screen and player, attaching the map if one is given
EEMiniMap::EEMiniMap(GEScreen* inScreen, EEMap* inMap, EEPlayer* inPlayer)
    : RTSMiniMap(inPlayer, inScreen)
{
    ee_gamecpp_minimap_construct(this, inMap);
}

// Destroys the mini map (the base class frees its surfaces)
EEMiniMap::~EEMiniMap()
{
    ee_app_minimap_free(this->mRust);
}

// Adds a building's remembered image as a dot in its mini map cell; a wall also adds its next three segments
void EEMiniMap::AddDoppelganger(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIsWall)
{
    ee_gamecpp_minimap_doppelganger(this, inDoppelganger, inX, inY, inIsWall, true);
}

// Redraws the camera's view outline when a map is attached
void EEMiniMap::RefreshView()
{
    if (this->mL12174)
        this->Refresh();
}

// Copies the 64 x 64 unit layer into the world texture and relocks it for drawing
void EEMiniMap::UpdateWorldTexture()
{
    ee_gamecpp_minimap_update_world_texture(this);
}

// New (port): locks the unit layer and hands its pixels to the Rust half
void EEMiniMap::LockUnitLayer()
{
    ee_gamecpp_minimap_lock_unit_layer(this);
}

// Counts one sight source off the mini map cell holding a tile and redraws the cell
void EEMiniMap::CountCellOff(long inX, long inY)
{
    ee_gamecpp_minimap_count_cell(this, inX, inY, false);
}

// Counts one sight source onto the mini map cell holding a tile (an unexplored cell starts at 2), redraws it
void EEMiniMap::CountCellOn(long inX, long inY)
{
    ee_gamecpp_minimap_count_cell(this, inX, inY, true);
}

// Removes a building image's dot from its mini map cell (and a wall's next three segments)
void EEMiniMap::RemoveDoppelganger(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIsWall)
{
    ee_gamecpp_minimap_doppelganger(this, inDoppelganger, inX, inY, inIsWall, false);
}

// Attaches a map: builds the map/pixel and map/cell transformations (rotated 45 degrees, centred) and the
// map size of one pixel, then redraws the mini map
// (the transformations: ee_gfx minimap.rs; its translation adds are written `-x + m03` / `m13 - y`, as here)
void EEMiniMap::SetMap(EEMap* inMap)
{
    ee_gamecpp_minimap_set_map(this, inMap);
}

// Clears the unit layer and all dots, then re-reads every tile's sight and contents diagonal by diagonal (the walk:
// ee_app minimap.rs)
void EEMiniMap::Redraw()
{
    ee_gamecpp_minimap_redraw(this);
}

// Recolours every player's dots after a colour change (units in player colour, buildings in shade)
void EEMiniMap::Recolor()
{
    ee_gamecpp_minimap_recolor(this);
}

// Recomputes the camera's view trapezoid on the mini map (camera tilted 30 degrees, at least height 5),
// shortening its sides to the width of its near edge (at least 32)
// (the trapezoid: ee_gfx minimap.rs)
void EEMiniMap::Refresh()
{
    ee_gamecpp_minimap_refresh(this);
}

// Recomputes every terrain pixel of the mini map and flags the terrain texture changed
void EEMiniMap::RedrawAll()
{
    ee_gamecpp_minimap_redraw_all(this);
}

// Recomputes the terrain pixels covering a map rectangle and flags the terrain texture changed (the pixel
// rectangle: ee_app minimap.rs)
void EEMiniMap::RedrawRect(long inLeft, long inRight, long inBottom, long inTop)
{
    ee_gamecpp_minimap_redraw_rect(this, inLeft, inRight, inBottom, inTop);
}

// Recomputes the terrain colour of each pixel in a texture rectangle (the lit average of the map vertices under it:
// ee_app minimap.rs)
void EEMiniMap::RedrawPixels(long inLeft, long inRight, long inBottom, long inTop)
{
    ee_gamecpp_minimap_redraw_pixels(this, inLeft, inRight, inBottom, inTop);
}

// Moves a unit's dot between mini map cells (or just updates its position within a cell); -10000 means
// not on the map. Classes without a mini map colour and other players' foundations get no dot
void EEMiniMap::MoveUnit(EEUnit* inUnit, long inOldX, long inOldY, long inX, long inY)
{
    ee_gamecpp_minimap_move_unit(this, inUnit, inOldX, inOldY, inX, inY);
}

// ------------------------------------------------------------------------- round 9, lane B: the mini map's bodies' C++

// the tile's cases (EETile.cpp), then lane C's
int64_t EECore9OpB4(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore9OpB4F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// the Rust's constants and the members it reads in place (gamecpp minimap.rs)
static_assert(kUnitCategoryResource == 1 && kUnitTypeResForageBush == 0x37 && kUnitTypeAirport == 0x42 && kResourceStone == 4 &&
                  kResourceGold == 5 && kResourceIron == 6,
              "gamecpp minimap.rs");
static_assert(offsetof(EEMiniMap, mL12178) == offsetof(EEMiniMap, mL12174) + sizeof(void*) &&
                  offsetof(EEMiniMap, mL1217C) == offsetof(EEMiniMap, mL12178) + sizeof(long) &&
                  offsetof(EEMiniMap, mL12180) == offsetof(EEMiniMap, mL1217C) + sizeof(long) &&
                  offsetof(EEMiniMap, mB12184) == offsetof(EEMiniMap, mL12180) + sizeof(long) &&
                  offsetof(EEMiniMap, mB12185) == offsetof(EEMiniMap, mB12184) + 1,
              "gamecpp minimap.rs MiniMapTail");
static_assert(offsetof(EEMiniMap, mPixelToMap) == offsetof(EEMiniMap, mMapToPixel) + sizeof(GETransformation) &&
                  offsetof(EEMiniMap, mMapToCell) == offsetof(EEMiniMap, mPixelToMap) + sizeof(GETransformation) &&
                  offsetof(EEMiniMap, mCellToMap) == offsetof(EEMiniMap, mMapToCell) + sizeof(GETransformation) &&
                  offsetof(EEMiniMap, mCorner1) == offsetof(EEMiniMap, mCorner0) + sizeof(GE2DPoint) &&
                  offsetof(EEMiniMap, mCorner2) == offsetof(EEMiniMap, mCorner1) + sizeof(GE2DPoint) &&
                  offsetof(EEMiniMap, mCorner3) == offsetof(EEMiniMap, mCorner2) + sizeof(GE2DPoint),
              "gamecpp minimap.rs: the transformations and corners in order");

#define P64(x) ((int64_t)(intptr_t)(x))
#define MM ((EEMiniMap*)p)

// The C++ calls and members of the mini map's bodies in Rust (gamecpp minimap.rs; EeGameCpp's lane B section, after
// EEMap.cpp's EECore9OpB2): one statement each
int64_t EECore9OpB3(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
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
    case O::MmLight: *(GEVector*)a[0] = gMiniMapLight;
        break;
    case O::MmLightSet: gMiniMapLight = GEVector(0.0f, 1.5f, 1.0f);
        break;
    case O::MmLightNormalize: gMiniMapLight.Normalize();
        break;
    case O::MmViewport: return P64(&gViewport);
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
    default: return EECore9OpB4(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpB3F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore9OpB4F(inOp, p, a, f);
}
