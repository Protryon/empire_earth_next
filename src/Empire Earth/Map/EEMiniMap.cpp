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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GERectangle.h"
#include "lle/GETransformation.h"
#include "lle/GEViewport.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/Graphics Engine.h"
#include "lle/TSCriticalSection.h"
#include "lle/UList.h"
#include "lle/GERhombus.h"
#include "lle/GELine.h"
#include "lle/GEBitmap.h"
#include "lle/UIControl.h"

#include "ee/EETileLocation.h"
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
#include "ee/EEUIMiniMap.h"
#include "lle/GERenderState.h"
#include "rust/base/math.h"   // ee_gfx minimap.rs (RUST_PLAN R4.4)
#include "rust/game/app.h"
#include "rust/render/ge.h"
#include "rust/render/gfx.h"
#include "rust/base/math.h"
#include "rust/game/app.h"
#include "rust/render/ge.h"
#include "rust/render/gfx.h"
#include "rust/base/math.h"
#include "rust/game/app.h"
#include "rust/render/ge.h"
#include "rust/render/gfx.h"
#include "ee/EEAppCapture.h"

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
static const GETransformation& EEMiniMapT(const EeGeTransformation& inTransformation)
{
    return *reinterpret_cast<const GETransformation*>(&inTransformation);
}

// a building image's dot: gaia's in its class colour, others' in the owner's shade; a resource heading the cell
// keeps the units' dots after it (a wall's facing for its next three segments)
static EeAppMiniMapItem EEMiniMapBuilding(EEDoppelganger* inDoppelganger, bool inIsWall)
{
    EEUnitClass* theClass = inDoppelganger->mUnit->mUnitClass;
    long theResourceType = theClass->mResourceType;
    EeAppMiniMapItem theItem = {};
    theItem.shown = theClass->mMiniMapColor != 0x44;
    if (theItem.shown) {
        EEPlayer* thePlayer = inDoppelganger->GetPlayer();
        unsigned long theColor = thePlayer->mPlayerID == 0
                                     ? GEPackColor(kPixelFormatA4R4G4B4, EEColorTable::GetDarkColor(theClass->GetMiniMapColor()), 1.0f)
                                     : GEPackColor(kPixelFormatA4R4G4B4, thePlayer->mShadeColor, 1.0f);
        theItem.player = (unsigned short)thePlayer->mPlayerID;
        theItem.color = (unsigned short)theColor;
        theItem.flag = theClass->mCategory == kUnitCategoryResource &&
                       (theClass->mUnitType == kUnitTypeResForageBush || theResourceType == kResourceIron ||
                        theResourceType == kResourceGold || theResourceType == kResourceStone);
    }
    theItem.tile_size = theClass->mTileSize;
    theItem.wall = inIsWall;
    if (inIsWall)
        theItem.facing = inDoppelganger->mUnit->mComplexUnit->mFacing;
    return theItem;
}

// a unit's dot (none for a class without a mini map colour or another player's foundation): gaia's in its class
// colour, others' in the owner's
static EeAppMiniMapItem EEMiniMapUnit(EEUnit* inUnit)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    long theResourceType = inUnit->mResourceType;
    EeAppMiniMapItem theItem = {};
    theItem.shown = theClass->mMiniMapColor != 0x44 &&
                    !(inUnit->mComplexUnit && inUnit->mComplexUnit->IsFoundation() && inUnit->mPlayer != gWorld.GetCurrentPlayer());
    if (theItem.shown) {
        unsigned long theColor = inUnit->mPlayer->mPlayerID == 0
                                     ? GEPackColor(kPixelFormatA4R4G4B4, EEColorTable::GetColor2(theClass->mMiniMapColor), 1.0f)
                                     : GEPackColor(kPixelFormatA4R4G4B4, inUnit->mPlayer->mColor, 1.0f);
        theItem.player = (unsigned short)inUnit->mPlayer->mPlayerID;
        theItem.color = (unsigned short)theColor;
        theItem.flag = theClass->mCategory == kUnitCategoryResource &&
                       (theClass->mUnitType == kUnitTypeResForageBush || theResourceType == kResourceIron ||
                        theResourceType == kResourceGold || theResourceType == kResourceStone);
    }
    theItem.tile_size = theClass->mTileSize;
    theItem.unit = true;
    theItem.x = inUnit->mTileX;
    theItem.y = inUnit->mTileY;
    return theItem;
}

// New (port): the Rust half's host (the local player's sight of the tiles, a tile's items, the terrain colours)
struct EEMiniMapHostContext {
    std::vector<EeAppMiniMapItem> mItems;   // TILE_ITEMS' answer
};

static void EEMiniMapHostOp(void* inContext, int32_t inOp, EeAppMiniMapIo* io)
{
    EEMiniMapHostContext& theContext = *static_cast<EEMiniMapHostContext*>(inContext);
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EETile* theTile = inOp == EE_APP_MINIMAP_OP_TERRAIN_COLOR ? NULL : gWorld.mMap->GetTile(io->x, io->y);
    switch (inOp) {
    case EE_APP_MINIMAP_OP_UNIT_SHOWN:
        io->flag = !theTile || theTile->IsSeenBy(thePlayer) || gFreeCamera;
        break;
    case EE_APP_MINIMAP_OP_TILE_SIGHT:
        io->flag = theTile != NULL;
        if (theTile == NULL)
            break;
        io->seen = theTile->IsSeenBy(thePlayer) || gFreeCamera;
        io->visible = !io->seen && theTile->IsVisibleTo(thePlayer);
        io->all = thePlayer->mSeesAll || gFreeCamera;
        io->visibility = theTile->GetVisibility(thePlayer->mPlayerID);
        break;
    case EE_APP_MINIMAP_OP_TILE_ITEMS:
        // the tile's building images of the local player (a complex one on its own tile), then its living units the
        // local player may see (TellTile; NEAR: the original called the vector's begin and _Ptrit::operator+ out of line)
        theContext.mItems.clear();
        if (theTile != NULL) {
            for (unsigned long i = 0; i < theTile->GetNumDoppelgangers(); i++) {
                EEDoppelganger* theDoppelganger = theTile->mDoppelgangers[i];
                if (thePlayer == theDoppelganger->mPlayer &&
                    (!theDoppelganger->IsComplex() || theDoppelganger->GetTile() == theTile))
                    theContext.mItems.push_back(EEMiniMapBuilding(
                        theDoppelganger, theDoppelganger->mUnit->mUnitClass->mUnitType == kUnitTypeAirport));
            }
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                EEUnit* theUnit = theNode->mUnit;
                if (theUnit->mHitPoints != 0 && theUnit->mTileX == io->x && theUnit->mTileY == io->y &&
                    !EEIsCloakedEnemyOf(theUnit, thePlayer))
                    theContext.mItems.push_back(EEMiniMapUnit(theUnit));
            }
        }
        io->items = theContext.mItems.empty() ? NULL : &theContext.mItems[0];
        io->count = (long)theContext.mItems.size();
        break;
    case EE_APP_MINIMAP_OP_TERRAIN_COLOR: {
        const GEColor& theColor = EETerrainTable::Get(io->id)->mColor;
        io->color[0] = theColor.mRed;
        io->color[1] = theColor.mGreen;
        io->color[2] = theColor.mBlue;
        break;
    }
    }
}

static EeAppMiniMapHost EEMiniMapHost(EEMiniMapHostContext& inContext)
{
    return {&EEMiniMapHostOp, &inContext, gWorld.mMap != NULL ? gWorld.mMap->mTerrainMesh.GetRust() : NULL,
            {gMiniMapLight.mX, gMiniMapLight.mY, gMiniMapLight.mZ}};
}

// one of the Rust half's bodies (ee_app_minimap_call's entries)
static void EEMiniMapCall(EeAppMiniMap* inRust, uint32_t inWhat, const EeAppMiniMapItem* inItem, long inX, long inY,
                          long inX2 = 0, long inY2 = 0)
{
    EEMiniMapHostContext theContext;
    EeAppMiniMapHost theHost = EEMiniMapHost(theContext);
    ee_app_minimap_call(inRust, &theHost, inWhat, inItem, inX, inY, inX2, inY2);
}

// Creates the mini map for a screen and player, attaching the map if one is given
EEMiniMap::EEMiniMap(GEScreen* inScreen, EEMap* inMap, EEPlayer* inPlayer)
    : RTSMiniMap(inPlayer, inScreen)
{
    this->mRust = ee_app_minimap_new();
    this->mB12184 = false;
    this->mB12185 = false;
    if (inMap)
        this->SetMap(inMap);
    else {
        this->mL12174 = NULL;
        this->mL12178 = 0;
        this->mL1217C = 0;
        this->mL12180 = 0;
    }
    gMiniMapLight = GEVector(0.0f, 1.5f, 1.0f);
    gMiniMapLight.Normalize();
}

// Destroys the mini map (the base class frees its surfaces)
EEMiniMap::~EEMiniMap()
{
    ee_app_minimap_free(this->mRust);
}

// Adds a building's remembered image as a dot in its mini map cell; a wall also adds its next three segments
void EEMiniMap::AddDoppelganger(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIsWall)
{
    EeAppMiniMapItem theItem = EEMiniMapBuilding(inDoppelganger, inIsWall);
    EEMiniMapCall(this->mRust, 4, &theItem, inX, inY);
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
    GERectangle theRectangle(0.0f, 0.0f, 64.0f, 64.0f);
    this->mUnitSurface->Unlock();
    this->mUnitSurface->CopyTo(this->mWorldTexture->GetSurface(), theRectangle, theRectangle, false);
    this->LockUnitLayer();
}

// New (port): locks the unit layer and hands its pixels to the Rust half
void EEMiniMap::LockUnitLayer()
{
    this->mUnitSurface->Lock();
    this->mPixels = reinterpret_cast<unsigned short*>(this->mUnitSurface->GetMemoryAddress());
    this->mPixelsPerRow = this->mUnitSurface->GetBytesPerRow() >> 1;
    ee_app_minimap_set_pixels(this->mRust, this->mPixels, this->mPixelsPerRow);
}

// Counts one sight source off the mini map cell holding a tile and redraws the cell
void EEMiniMap::CountCellOff(long inX, long inY)
{
    EEMiniMapCall(this->mRust, 0, NULL, inX, inY);
}

// Counts one sight source onto the mini map cell holding a tile (an unexplored cell starts at 2), redraws it
void EEMiniMap::CountCellOn(long inX, long inY)
{
    EEMiniMapCall(this->mRust, 1, NULL, inX, inY);
}

// Removes a building image's dot from its mini map cell (and a wall's next three segments)
void EEMiniMap::RemoveDoppelganger(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIsWall)
{
    EeAppMiniMapItem theItem = EEMiniMapBuilding(inDoppelganger, inIsWall);
    EEMiniMapCall(this->mRust, 5, &theItem, inX, inY);
}

// Attaches a map: builds the map/pixel and map/cell transformations (rotated 45 degrees, centred) and the
// map size of one pixel, then redraws the mini map
// (the transformations: ee_gfx minimap.rs; its translation adds are written `-x + m03` / `m13 - y`, as here)
void EEMiniMap::SetMap(EEMap* inMap)
{
    this->mL12174 = inMap;
    EeGfxMiniMapTransforms theTransforms = ee_gfx_minimap_transforms((int32_t)inMap->mL195610, (int32_t)inMap->mL195614,
                                                                     (int32_t)inMap->mSize);
    this->mMapToPixel = EEMiniMapT(theTransforms.map_to_pixel);
    this->mPixelToMap = EEMiniMapT(theTransforms.pixel_to_map);
    this->mMapToCell = EEMiniMapT(theTransforms.map_to_cell);
    this->mCellToMap = EEMiniMapT(theTransforms.cell_to_map);
    this->mL12178 = (long)theTransforms.pixel_width;
    this->mL1217C = (long)theTransforms.pixel_height;
    ee_app_minimap_set_map(this->mRust, &this->mMapToPixel.mMatrix[0][0], &this->mPixelToMap.mMatrix[0][0],
                           &this->mMapToCell.mMatrix[0][0], this->mL12178, this->mL1217C);
    EEUIMiniMap::Update(this);
    this->LockUnitLayer();
}

// Clears the unit layer and all dots, then re-reads every tile's sight and contents diagonal by diagonal (the walk:
// ee_app minimap.rs)
void EEMiniMap::Redraw()
{
    EEMap* theMap = gWorld.mMap;
    if (!theMap)
        return;
    this->mUnitSurface->Unlock();
    GEColor theBlack(0.0f, 0.0f, 0.0f);
    this->mUnitSurface->Clear(theBlack, 1.0f);
    this->LockUnitLayer();
    EEMiniMapCall(this->mRust, 3, NULL, theMap->mL195614, theMap->mL195610);
}

// Recolours every player's dots after a colour change (units in player colour, buildings in shade)
void EEMiniMap::Recolor()
{
    unsigned short theUnitColors[17] = {};
    unsigned short theColors[17] = {};
    for (unsigned long i = 0; i < gWorld.mNumPlayers; i++) {
        theUnitColors[i] = (unsigned short)GEPackColor(kPixelFormatA4R4G4B4, gWorld.mPlayers[i]->mColor, 1.0f);
        theColors[i] = (unsigned short)GEPackColor(kPixelFormatA4R4G4B4, gWorld.mPlayers[i]->mShadeColor, 1.0f);
    }
    ee_app_minimap_recolor(this->mRust, theUnitColors, theColors);
}

// Recomputes the camera's view trapezoid on the mini map (camera tilted 30 degrees, at least height 5),
// shortening its sides to the width of its near edge (at least 32)
// (the trapezoid: ee_gfx minimap.rs)
void EEMiniMap::Refresh()
{
    EeGfxMiniMapView theView = ee_gfx_minimap_view(reinterpret_cast<const EeGeViewport*>(&gViewport), gOrthographicView,
                                                   reinterpret_cast<const EeGeTransformation*>(&this->mMapToPixel));
    this->mCorner0 = *reinterpret_cast<const GE2DPoint*>(&theView.corners[0]);
    this->mCorner1 = *reinterpret_cast<const GE2DPoint*>(&theView.corners[1]);
    this->mCorner2 = *reinterpret_cast<const GE2DPoint*>(&theView.corners[2]);
    this->mCorner3 = *reinterpret_cast<const GE2DPoint*>(&theView.corners[3]);
}

// Recomputes every terrain pixel of the mini map and flags the terrain texture changed
void EEMiniMap::RedrawAll()
{
    this->mTerrainCriticalSection->Enter();
    this->RedrawPixels(0, 0x80, 0, 0x80);
    this->mB12184 = true;
    this->mTerrainCriticalSection->Leave();
}

// Recomputes the terrain pixels covering a map rectangle and flags the terrain texture changed (the pixel
// rectangle: ee_app minimap.rs)
void EEMiniMap::RedrawRect(long inLeft, long inRight, long inBottom, long inTop)
{
    long theRect[4];
    this->mTerrainCriticalSection->Enter();
    ee_app_minimap_pixel_rect(this->mRust, inLeft, inRight, inBottom, inTop, theRect);
    this->RedrawPixels(theRect[0], theRect[1], theRect[2], theRect[3]);
    this->mB12184 = true;
    this->mTerrainCriticalSection->Leave();
}

// Recomputes the terrain colour of each pixel in a texture rectangle (the lit average of the map vertices under it:
// ee_app minimap.rs)
void EEMiniMap::RedrawPixels(long inLeft, long inRight, long inBottom, long inTop)
{
    this->mTerrainTexture->GetSurface()->Lock();
    unsigned short* theBits = reinterpret_cast<unsigned short*>(this->mTerrainTexture->GetSurface()->GetMemoryAddress());
    unsigned long thePitch = this->mTerrainTexture->GetSurface()->GetBytesPerRow() >> 1;
    EEMiniMapHostContext theContext;
    EeAppMiniMapHost theHost = EEMiniMapHost(theContext);
    ee_app_minimap_redraw_pixels(this->mRust, &theHost, theBits, thePitch, inLeft, inRight, inBottom, inTop);
    EEAppCaptureMiniMapPixels(inLeft, inRight, inBottom, inTop, theBits, thePitch);   // MMPX (TEST ONLY)
    this->mTerrainTexture->GetSurface()->Unlock();
}

// Moves a unit's dot between mini map cells (or just updates its position within a cell); -10000 means
// not on the map. Classes without a mini map colour and other players' foundations get no dot
void EEMiniMap::MoveUnit(EEUnit* inUnit, long inOldX, long inOldY, long inX, long inY)
{
    EeAppMiniMapItem theItem = EEMiniMapUnit(inUnit);
    EEMiniMapCall(this->mRust, 6, &theItem, inOldX, inOldY, inX, inY);
}

// New (port): one cell's sight count, unit layer pixel and dots in list order (the capture's read,
// ee/EEAppCapture.h)
void EEMiniMap::GetCell(long inIndex, unsigned short& outCount, unsigned short& outPixel,
                        std::vector<MMPlayerUnit>& outDots, std::vector<MMMapUnit>& outUnits) const
{
    const EeAppMiniMapDot* theDots;
    const EeAppMiniMapUnitDot* theUnits;
    long theDotCount;
    long theUnitCount;
    ee_app_minimap_cell(this->mRust, inIndex, &outCount, &theDots, &theDotCount, &theUnits, &theUnitCount);
    outPixel = this->mPixels != NULL ? this->mPixels[this->mPixelsPerRow * (inIndex & 0x3f) + (inIndex >> 6)] : 0;
    outDots.assign(reinterpret_cast<const MMPlayerUnit*>(theDots), reinterpret_cast<const MMPlayerUnit*>(theDots) + theDotCount);
    outUnits.assign(reinterpret_cast<const MMMapUnit*>(theUnits), reinterpret_cast<const MMMapUnit*>(theUnits) + theUnitCount);
}
