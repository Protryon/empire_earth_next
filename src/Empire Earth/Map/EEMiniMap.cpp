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

// ------------------------------------------------------------------------- round 9, lane B: the mini map's bodies' C++ (their
// cases: EEMap.cpp's EECore9OpB2)

// the Rust's constants and the members it reads in place (gamecpp minimap.rs)
// (its constants are checked against the headers by gamecpp cpp_consts_tests.rs)
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
