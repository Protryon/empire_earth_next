// C:\EEX\Empire Earth\Map\EEMiniMap.cpp (file name inferred: no __FILE__, no asserts). The game's mini map (RTTI
// EEMiniMap > RTSMiniMap, vtable 0x441d44): 0x11e36e..0x121191 (the two plain globals' `$E` chains, the class in
// definition order, then the template COMDATs: the dots' lists and their node pools, the pools' `$E` chain, and
// EEColorTable's two colour accessors; EETile.cpp's pool chain opens the next unit). Unit flags: the exe's module set.
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
#include "rust/ee_rust.h"   // ee_gfx minimap.rs (RUST_PLAN R4.4)

extern GEViewport gViewport;                // 0x5308f0 (Empire Earth.cpp)
extern bool& gOrthographicView;              // 0x5308e2 (orthographic camera; EEGraphics)

// chain at 0x51e36e (wrapper)
//   object 0x9297d0  ctor (GEVector's three fldz) 0x11e378
// The terrain light (EEMiniMap's ctor points it at (0, 1.5, 1), normalized) (provisional name)
GEVector gMiniMapLight;
// SYNTHETIC: EXE 0x51e36e   (wrapper)
// SYNTHETIC: EXE 0x51e378   (ctor)
// SYNTHETIC: EXE 0x51e391   (registrar)
// SYNTHETIC: EXE 0x51e39d   (dtor: GEVector has an empty destructor)

// chain at 0x51e39e (wrapper)
//   object 0x9297c0  ctor (three constants) 0x11e3a8
// The terrain colours' ambient term (GetPixelColor adds it) (provisional name)
GEColor gMiniMapAmbient(0.102745101f, 0.132549018f, 0.188235298f);
// SYNTHETIC: EXE 0x51e39e   (wrapper)
// SYNTHETIC: EXE 0x51e3a8   (ctor)
// SYNTHETIC: EXE 0x51e3c7   (registrar)
// SYNTHETIC: EXE 0x51e3d3   (dtor: GEColor has an empty destructor)

// the mini map's texture is 0x80 pixels square (0x441d50 .. 0x441d60: unit-local constants)
const float kMiniMapSize = 128.0f;          // 0x441d50 (provisional name)
// SetMap's transformations, Refresh's view trapezoid and ProjectToGround are ee_gfx minimap.rs (RUST_PLAN R4.4; its
// constants, .rdata 0x441d38..0x441d60, are there); the GE value classes pass as their #[repr(C)] mirrors
static_assert(sizeof(GE2DPoint) == sizeof(EeGe2DPoint) && sizeof(GETransformation) == sizeof(EeGeTransformation) &&
              sizeof(GEViewport) == sizeof(EeGeViewport), "GE mirrors");
static const GETransformation& EEMiniMapT(const EeGeTransformation& inTransformation)
{
    return *reinterpret_cast<const GETransformation*>(&inTransformation);
}

// the tile's terrain at a vertex: the tile above and right of it, else the one to its left, below, below left
static unsigned long EEMiniMapTerrainAt(GETerrainMesh& inMesh, unsigned long inX, unsigned long inY)
{
    if (inMesh.GetIsValidTile(inX, inY))
        return inMesh.GetTextureSetID(inX, inY);
    if (inMesh.GetIsValidTile(inX - 1, inY))
        return inMesh.GetTextureSetID(inX - 1, inY);
    if (inMesh.GetIsValidTile(inX, inY - 1))
        return inMesh.GetTextureSetID(inX, inY - 1);
    return inMesh.GetTextureSetID(inX - 1, inY - 1);
}

// FUNCTION: EXE 0x51e3d4
// Creates the mini map for a screen and player, sizing the dot pools and attaching the map if one is given
EEMiniMap::EEMiniMap(GEScreen* inScreen, EEMap* inMap, EEPlayer* inPlayer)
    : RTSMiniMap(inPlayer, inScreen)
{
    UList<EEMiniMapUnitDot>::sNodePool.SetSize(500, 0x28, 1);
    UList<EEMiniMapDot>::sNodePool.SetSize(500, 0x28, 1);
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
    for (long i = 0; i < 0x1000; i++)
        this->mU1015C[i] = 0;
}

// SYNTHETIC: EXE 0x51e4da
// (the scalar deleting destructor, emitted with the vtable; slot EEMiniMap+0x0)

// FUNCTION: EXE 0x51e4f6
// Destroys the mini map (the base class frees its surfaces)
EEMiniMap::~EEMiniMap()
{
}

// FUNCTION: EXE 0x51e53b
// Adds a building's remembered image as a dot in its mini map cell (gaia in its class colour, others in
// the owner's shade; a resource dot heading the cell stays first); a wall also adds its next three segments
void EEMiniMap::AddDoppelganger(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIsWall)
{
    GE3DPoint thePoint;
    EEMiniMapDot theDot;
    GE3DPoint theMapPoint;
    EEUnitClass* theClass = inDoppelganger->mUnit->mUnitClass;
    long theResourceType = theClass->mResourceType;
    if (theClass->mMiniMapColor != 0x44) {
        thePoint.SetX(theClass->mTileSize * 0.5f + inX);
        thePoint.SetY(theClass->mTileSize * 0.5f + inY);
        this->mMapToCell.Apply(thePoint, theMapPoint);
        long theCellX = UMFloatToLong(theMapPoint.GetX());
        long theCellY = UMFloatToLong(theMapPoint.GetY());
        unsigned long theColor = inDoppelganger->GetPlayer()->mPlayerID == 0
                                     ? GEPackColor(kPixelFormatA4R4G4B4, EEColorTable::GetDarkColor(theClass->GetMiniMapColor()), 1.0f)
                                     : GEPackColor(kPixelFormatA4R4G4B4, inDoppelganger->GetPlayer()->mShadeColor, 1.0f);
        theDot.mPlayer = (unsigned short)inDoppelganger->GetPlayer()->mPlayerID;
        theDot.mColor = (unsigned short)theColor;
        if (theClass->mCategory == kUnitCategoryResource &&
            (theClass->mUnitType == kUnitTypeResForageBush || theResourceType == kResourceIron || theResourceType == kResourceGold || theResourceType == kResourceStone))
            theDot.mFlag = 1;
        else
            theDot.mFlag = 0;
        UList<EEMiniMapDot>& theList = this->mPlayerUnits[theCellX * 0x40 + theCellY];
        if (theList.mHead && theList.mHead->mData.mFlag)
            theList.InsertAfter(theList.mHead, theDot);
        else
            theList.AddHead(theDot);
        this->RedrawCell(theCellX, theCellY);
    }
    if (inIsWall != true)
        return;
    long theSize = inDoppelganger->mUnit->mUnitClass->mTileSize;
    switch (inDoppelganger->mUnit->mComplexUnit->mFacing) {
    case 0:
        this->AddDoppelganger(inDoppelganger, inX, inY + theSize, false);
        this->AddDoppelganger(inDoppelganger, inX, inY + theSize * 2, false);
        this->AddDoppelganger(inDoppelganger, inX, inY + theSize * 3, false);
        break;
    case 1:
        this->AddDoppelganger(inDoppelganger, inX, inY - theSize, false);
        this->AddDoppelganger(inDoppelganger, inX, inY - theSize * 2, false);
        this->AddDoppelganger(inDoppelganger, inX, inY - theSize * 3, false);
        break;
    case 2:
        this->AddDoppelganger(inDoppelganger, inX + theSize, inY, false);
        this->AddDoppelganger(inDoppelganger, inX + theSize * 2, inY, false);
        this->AddDoppelganger(inDoppelganger, inX + theSize * 3, inY, false);
        break;
    default:
        this->AddDoppelganger(inDoppelganger, inX - theSize, inY, false);
        this->AddDoppelganger(inDoppelganger, inX - theSize * 2, inY, false);
        this->AddDoppelganger(inDoppelganger, inX - theSize * 3, inY, false);
        break;
    }
}

// FUNCTION: EXE 0x51e7b1
// Redraws the camera's view outline when a map is attached
void EEMiniMap::RefreshView()
{
    if (this->mL12174)
        this->Refresh();
}

// FUNCTION: EXE 0x51e7c0
// Copies the 64 x 64 unit layer into the world texture and relocks it for drawing
void EEMiniMap::UpdateWorldTexture()
{
    GERectangle theRectangle(0.0f, 0.0f, 64.0f, 64.0f);
    this->mUnitSurface->Unlock();
    this->mUnitSurface->CopyTo(this->mWorldTexture->GetSurface(), theRectangle, theRectangle, false);
    this->mUnitSurface->Lock();
    this->mPixels = reinterpret_cast<unsigned short*>(this->mUnitSurface->GetMemoryAddress());
    this->mPixelsPerRow = this->mUnitSurface->GetBytesPerRow() >> 1;
}

// FUNCTION: EXE 0x51e83a
// Counts one sight source off the mini map cell holding a tile and redraws the cell
void EEMiniMap::CountCellOff(long inX, long inY)
{
    GE3DPoint thePoint(inX + 0.5f, inY + 0.5f, 0.0f);
    GE3DPoint theMapPoint;
    this->mMapToCell.Apply(thePoint, theMapPoint);
    inY = UMFloatToLong(theMapPoint.GetX());
    inX = UMFloatToLong(theMapPoint.GetY());
    this->mU1015C[inY * 0x40 + inX]--;
    this->RedrawCell(inY, inX);
}

// FUNCTION: EXE 0x51e8bf
// Counts one sight source onto the mini map cell holding a tile (an unexplored cell starts at 2), redraws it
void EEMiniMap::CountCellOn(long inX, long inY)
{
    GE3DPoint thePoint(inX + 0.5f, inY + 0.5f, 0.0f);
    GE3DPoint theMapPoint;
    this->mMapToCell.Apply(thePoint, theMapPoint);
    inY = UMFloatToLong(theMapPoint.GetX());
    inX = UMFloatToLong(theMapPoint.GetY());
    unsigned short& theCount = this->mU1015C[inY * 0x40 + inX];
    if (theCount != 0)
        theCount++;
    else
        theCount = 2;
    this->RedrawCell(inY, inX);
}

// FUNCTION: EXE 0x51e956
// Removes a building image's dot from its mini map cell (and a wall's next three segments)
void EEMiniMap::RemoveDoppelganger(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIsWall)
{
    GE3DPoint thePoint;
    GE3DPoint theMapPoint;
    EEUnitClass* theClass = inDoppelganger->mUnit->mUnitClass;
    if (theClass->mMiniMapColor != 0x44) {
        thePoint.SetX(theClass->mTileSize * 0.5f + inX);
        thePoint.SetY(theClass->mTileSize * 0.5f + inY);
        this->mMapToCell.Apply(thePoint, theMapPoint);
        long theCellX = UMFloatToLong(theMapPoint.GetX());
        long theCellY = UMFloatToLong(theMapPoint.GetY());
        unsigned long theColor = inDoppelganger->GetPlayer()->mPlayerID == 0
                                     ? GEPackColor(kPixelFormatA4R4G4B4, EEColorTable::GetDarkColor(theClass->GetMiniMapColor()), 1.0f)
                                     : GEPackColor(kPixelFormatA4R4G4B4, inDoppelganger->GetPlayer()->mShadeColor, 1.0f);
        EEMiniMapDot theDot;
        theDot.mPlayer = (unsigned short)inDoppelganger->GetPlayer()->mPlayerID;
        theDot.mColor = (unsigned short)theColor;
        UList<EEMiniMapDot>& theList = this->mPlayerUnits[theCellX * 0x40 + theCellY];
        UListNode<EEMiniMapDot>* theNode = theList.Find(theDot);
        if (theNode)
            theList.Remove(theNode);
        this->RedrawCell(theCellX, theCellY);
    }
    if (inIsWall != true)
        return;
    long theSize = inDoppelganger->mUnit->mUnitClass->mTileSize;
    switch (inDoppelganger->mUnit->mComplexUnit->mFacing) {
    case 0:
        this->RemoveDoppelganger(inDoppelganger, inX, inY + theSize, false);
        this->RemoveDoppelganger(inDoppelganger, inX, inY + theSize * 2, false);
        this->RemoveDoppelganger(inDoppelganger, inX, inY + theSize * 3, false);
        break;
    case 1:
        this->RemoveDoppelganger(inDoppelganger, inX, inY - theSize, false);
        this->RemoveDoppelganger(inDoppelganger, inX, inY - theSize * 2, false);
        this->RemoveDoppelganger(inDoppelganger, inX, inY - theSize * 3, false);
        break;
    case 2:
        this->RemoveDoppelganger(inDoppelganger, inX + theSize, inY, false);
        this->RemoveDoppelganger(inDoppelganger, inX + theSize * 2, inY, false);
        this->RemoveDoppelganger(inDoppelganger, inX + theSize * 3, inY, false);
        break;
    default:
        this->RemoveDoppelganger(inDoppelganger, inX - theSize, inY, false);
        this->RemoveDoppelganger(inDoppelganger, inX - theSize * 2, inY, false);
        this->RemoveDoppelganger(inDoppelganger, inX - theSize * 3, inY, false);
        break;
    }
}

// FUNCTION: EXE 0x51eb87
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
    EEUIMiniMap::Update(this);
    this->mUnitSurface->Lock();
    this->mPixels = reinterpret_cast<unsigned short*>(this->mUnitSurface->GetMemoryAddress());
    this->mPixelsPerRow = this->mUnitSurface->GetBytesPerRow() >> 1;
}

// FUNCTION: EXE 0x51f0db
// Counts a tile's sight into its mini map cell: explored-only tiles once, seen tiles once per viewer
void EEMiniMap::TellTileVisibility(EETile* inTile, long inX, long inY)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    if (!inTile->IsSeenBy(thePlayer) && !gFreeCamera) {
        if (inTile->IsVisibleTo(thePlayer)) {
            this->CountCellOn(inX, inY);
            this->CountCellOff(inX, inY);
        }
    } else {
        if (thePlayer->mSeesAll || gFreeCamera)
            this->CountCellOn(inX, inY);
        for (long i = inTile->GetVisibility(thePlayer->mPlayerID) - 1; i > 0; i--)
            this->CountCellOn(inX, inY);
    }
}

// FUNCTION: EXE 0x51f184
// Adds a tile's building images and its living, visible units to the mini map
// NEAR: the original calls vector<EEDoppelganger*>::begin 0x17a952 and _Ptrit::operator+ 0x80ad for mU18[i] out
// of line; ours expands them
void EEMiniMap::TellTile(EETile* inTile, long inX, long inY)
{
    unsigned long i = 0;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    for (; i < inTile->GetNumDoppelgangers(); i++) {
        EEDoppelganger* theDoppelganger = inTile->mDoppelgangers[i];
        if (thePlayer == theDoppelganger->mPlayer &&
            (!theDoppelganger->IsComplex() || theDoppelganger->GetTile() == inTile))
            this->AddDoppelganger(theDoppelganger, inX, inY, theDoppelganger->mUnit->mUnitClass->mUnitType == kUnitTypeAirport);
    }
    for (RTSTileUnitNode* theNode = inTile->mUnits; theNode; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        if (theUnit->mHitPoints != 0 && theUnit->mTileX == inX && theUnit->mTileY == inY &&
            !EEIsCloakedEnemyOf(theUnit, gWorld.GetCurrentPlayer()))
            this->MoveUnit(theUnit, -10000, -10000, theUnit->mTileX, theUnit->mTileY);
    }
}

// FUNCTION: EXE 0x51f272
// Computes a mini map pixel's terrain colour: the lit average of the map vertices under it plus ambient,
// or the nearest vertex when none falls inside
// NEAR: frame layout and slot order differ from the original
void EEMiniMap::GetPixelColor(long inX, long inY, GEColor& outColor)
{
    EEMap* theMap = gWorld.mMap;
    GETerrainMesh& theMesh = theMap->mTerrainMesh;
    GE2DPoint theMapPoint;
    float theCount = 0.0f;
    outColor = GEColor(0.0f, 0.0f, 0.0f);
    GERectangle thePixel(inX - 0.5f, inY - 0.5f, inX + 0.5f, inY + 0.5f);
    GE2DPoint theCorner(inX - 0.5f, inY - 0.5f);
    this->mPixelToMap.Apply(theCorner, theMapPoint);
    long theStartX = UMCeil(theMapPoint.GetX());
    theCorner = GE2DPoint(inX - 0.5f, inY + 0.5f);
    this->mPixelToMap.Apply(theCorner, theMapPoint);
    long theY = UMCeil(theMapPoint.GetY());
    long theEndX = this->mL12178 + theStartX;
    long theEndY = this->mL1217C + theY;
    for (; theY < theEndY; theY++) {
        for (long theX = theStartX; theX < theEndX; theX++) {
            if (!theMesh.GetIsValidPoint(theX, theY))
                continue;
            GE2DPoint thePoint((float)theX, (float)theY);
            GE2DPoint thePixelPoint;
            this->mMapToPixel.Apply(thePoint, thePixelPoint);
            if (!thePixel.Contains(thePixelPoint))
                continue;
            const GEColor& theVertexColor = theMesh.GetVertexColor(theX, theY);
            EETerrainTable* theTerrain = EETerrainTable::Get(EEMiniMapTerrainAt(theMesh, theX, theY));
            GEVector* theNormal = theMesh.GetVertexNormal(theX, theY);
            float theLight = gMiniMapLight.mX * theNormal->mX + gMiniMapLight.mY * theNormal->mY +
                             gMiniMapLight.mZ * theNormal->mZ;
            if (theLight > 0.0f) {
                outColor.mRed += theTerrain->mColor.mRed * theVertexColor.mRed * theLight;
                outColor.mGreen += theVertexColor.mGreen * theTerrain->mColor.mGreen * theLight;
                outColor.mBlue += theVertexColor.mBlue * theTerrain->mColor.mBlue * theLight;
            }
            theCount += 1.0f;
        }
    }
    if (theCount != 0.0f) {
        float theScale = 1.0f / theCount;
        outColor.mRed = outColor.mRed * theScale + gMiniMapAmbient.mRed;
        outColor.mGreen = gMiniMapAmbient.mGreen + outColor.mGreen * theScale;
        outColor.mBlue = gMiniMapAmbient.mBlue + outColor.mBlue * theScale;
    } else {
        theCorner = GE2DPoint((float)inX, (float)inY);
        this->mPixelToMap.Apply(theCorner, theMapPoint);
        long theX = UMFloatToLong(theMapPoint.GetX());
        unsigned long theVertexY = UMFloatToLong(theMapPoint.GetY());
        if (!theMesh.GetIsValidPoint(theX, theVertexY))
            return;
        const GEColor& theVertexColor = theMesh.GetVertexColor(theX, theVertexY);
        EETerrainTable* theTerrain = EETerrainTable::Get(EEMiniMapTerrainAt(theMesh, theX, theVertexY));
        GEVector* theNormal = theMesh.GetVertexNormal(theX, theVertexY);
        float theLight = gMiniMapLight.mX * theNormal->mX + gMiniMapLight.mY * theNormal->mY +
                         gMiniMapLight.mZ * theNormal->mZ;
        if (theLight <= 0.0f)
            return;
        outColor.mRed = theTerrain->mColor.mRed * theVertexColor.mRed * theLight + gMiniMapAmbient.mRed;
        outColor.mGreen = gMiniMapAmbient.mGreen + theVertexColor.mGreen * theTerrain->mColor.mGreen * theLight;
        outColor.mBlue = gMiniMapAmbient.mBlue + theVertexColor.mBlue * theTerrain->mColor.mBlue * theLight;
    }
    outColor.Normalize();
}

// FUNCTION: EXE 0x51f743
// Clears the unit layer and all dots, then re-reads every tile's sight and contents diagonal by diagonal
// NEAR: frame 0x24 vs 0x20: the original keeps x in ebx and shares the clear loop's slots
void EEMiniMap::Redraw()
{
    EEMap* theMap = gWorld.mMap;
    if (!theMap)
        return;
    long theWidth = theMap->mL195614;
    long theHeight = theMap->mL195610;
    this->mUnitSurface->Unlock();
    GEColor theBlack(0.0f, 0.0f, 0.0f);
    this->mUnitSurface->Clear(theBlack, 1.0f);
    this->mUnitSurface->Lock();
    this->mPixels = reinterpret_cast<unsigned short*>(this->mUnitSurface->GetMemoryAddress());
    this->mPixelsPerRow = this->mUnitSurface->GetBytesPerRow() >> 1;
    long i;
    long j;
    for (i = 0; i < 0x40; i++) {
        for (j = 0; j < 0x40; j++) {
            this->mPlayerUnits[i * 0x40 + j].Clear();
            this->mMapUnits[i * 0x40 + j].Clear();
            this->mU1015C[i * 0x40 + j] = 0;
        }
    }
    long theX;
    long theY;
    for (i = 0; i < theWidth; i++) {
        theY = theHeight + i;
        theX = i;
        for (j = theHeight; j > 0; j--) {
            this->TellTileVisibility(theMap->GetTile(theX, theY), theX, theY);
            theX++;
            this->TellTileVisibility(theMap->GetTile(theX, theY), theX, theY);
            theY--;
        }
    }
    for (theX = 0, theY = theHeight - 1; theX < theHeight; theX++, theY--)
        this->TellTileVisibility(theMap->GetTile(theX, theY), theX, theY);
    for (i = 0; i < theWidth; i++)
        this->TellTileVisibility(theMap->GetTile(theHeight + i, i), theHeight + i, i);
    for (i = 0; i < theWidth; i++) {
        theY = theHeight + i;
        theX = i;
        for (j = theHeight; j > 0; j--) {
            this->TellTile(theMap->GetTile(theX, theY), theX, theY);
            theX++;
            this->TellTile(theMap->GetTile(theX, theY), theX, theY);
            theY--;
        }
    }
    for (theX = 0, theY = theHeight - 1; theX < theHeight; theX++, theY--)
        this->TellTile(theMap->GetTile(theX, theY), theX, theY);
    for (i = 0; i < theWidth; i++)
        this->TellTile(theMap->GetTile(theHeight + i, i), theHeight + i, i);
}

// FUNCTION: EXE 0x51fa55
// Recolours every player's dots after a colour change (units in player colour, buildings in shade)
void EEMiniMap::Recolor()
{
    unsigned short theUnitColors[17];
    unsigned short theColors[17];
    unsigned long i;
    for (i = 0; i < gWorld.mNumPlayers; i++) {
        theUnitColors[i] = (unsigned short)GEPackColor(kPixelFormatA4R4G4B4, gWorld.mPlayers[i]->mColor, 1.0f);
        theColors[i] = (unsigned short)GEPackColor(kPixelFormatA4R4G4B4, gWorld.mPlayers[i]->mShadeColor, 1.0f);
    }
    long theX;
    long theY;
    for (theX = 0; theX < 0x40; theX++) {
        for (theY = 0; theY < 0x40; theY++) {
            for (UListNode<EEMiniMapDot>* theNode = this->mPlayerUnits[theX * 0x40 + theY].mHead; theNode; theNode = theNode->mNext) {
                if (theNode->mData.mPlayer != 0)
                    theNode->mData.mColor = theColors[theNode->mData.mPlayer];
            }
        }
    }
    for (theX = 0; theX < 0x40; theX++) {
        for (theY = 0; theY < 0x40; theY++) {
            for (UListNode<EEMiniMapUnitDot>* theNode = this->mMapUnits[theX * 0x40 + theY].mHead; theNode; theNode = theNode->mNext) {
                if (theNode->mData.mPlayer != 0)
                    theNode->mData.mColor = theUnitColors[theNode->mData.mPlayer];
            }
        }
    }
}

// FUNCTION: EXE 0x51fb2a
// Redraws one cell: the first unit on a seen tile, else the first building image, else black,
// dark or grey by its sight count
void EEMiniMap::RedrawCell(long inX, long inY)
{
    EEMap* theMap = gWorld.mMap;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    UListNode<EEMiniMapUnitDot>* theNode;
    for (theNode = this->mMapUnits[inX * 0x40 + inY].mHead; theNode; theNode = theNode->mNext) {
        EETile* theTile = theMap->GetTile(theNode->mData.mX, theNode->mData.mY);
        if (!theTile || theTile->IsSeenBy(thePlayer) || gFreeCamera)
            break;
    }
    if (theNode == NULL) {
        UListNode<EEMiniMapDot>* theDot = this->mPlayerUnits[inX * 0x40 + inY].mHead;
        if (theDot == NULL) {
            if (this->mU1015C[inX * 0x40 + inY] == 0)
                this->mPixels[this->mPixelsPerRow * inY + inX] = 0xf000;
            else if (this->mU1015C[inX * 0x40 + inY] == 1)
                this->mPixels[this->mPixelsPerRow * inY + inX] = 0x7000;
            else
                this->mPixels[this->mPixelsPerRow * inY + inX] = 0x777;
        } else
            this->mPixels[this->mPixelsPerRow * inY + inX] = theDot->mData.mColor;
    } else
        this->mPixels[this->mPixelsPerRow * inY + inX] = theNode->mData.mColor;
}

// FUNCTION: EXE 0x51fc62
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

// FUNCTION: EXE 0x51ff72
// Projects a screen point onto the ground at height inZ, or the far point when the ray misses
void EEMiniMap::ProjectToGround(const GEViewport& inViewport, const GE2DPoint& inPoint, float inZ, GE2DPoint& outPoint)
{
    ee_gfx_minimap_project(reinterpret_cast<const EeGeViewport*>(&inViewport), gOrthographicView,
                           reinterpret_cast<const EeGe2DPoint*>(&inPoint), inZ, reinterpret_cast<EeGe2DPoint*>(&outPoint));
}

// FUNCTION: EXE 0x5200df
// Recomputes every terrain pixel of the mini map and flags the terrain texture changed
void EEMiniMap::RedrawAll()
{
    this->mTerrainCriticalSection->Enter();
    this->RedrawPixels(0, 0x80, 0, 0x80);
    this->mB12184 = true;
    this->mTerrainCriticalSection->Leave();
}

// FUNCTION: EXE 0x520115
// Recomputes the terrain pixels covering a map rectangle and flags the terrain texture changed
void EEMiniMap::RedrawRect(long inLeft, long inRight, long inBottom, long inTop)
{
    GE2DPoint thePoint;
    GE2DPoint theMapPoint;
    long theMinX;
    long theMinY;
    long theMaxX;
    long theMaxY;
    this->mTerrainCriticalSection->Enter();
    thePoint.SetX((float)inLeft);
    thePoint.SetY((float)inTop);
    this->mMapToPixel.Apply(thePoint, theMapPoint);
    if (theMapPoint.GetX() < 0.0f)
        theMinX = 0;
    else if (theMapPoint.GetX() > kMiniMapSize)
        theMinX = 0x80;
    else
        theMinX = UMFloor(theMapPoint.GetX());
    thePoint.SetX((float)inLeft);
    thePoint.SetY((float)inBottom);
    this->mMapToPixel.Apply(thePoint, theMapPoint);
    if (theMapPoint.GetY() < 0.0f)
        theMinY = 0;
    else if (theMapPoint.GetY() > kMiniMapSize)
        theMinY = 0x80;
    else
        theMinY = UMFloor(theMapPoint.GetY());
    thePoint.SetX((float)inRight);
    thePoint.SetY((float)inBottom);
    this->mMapToPixel.Apply(thePoint, theMapPoint);
    if (theMapPoint.GetX() > kMiniMapSize)
        theMaxX = 0x80;
    else if (theMapPoint.GetX() < 0.0f)
        theMaxX = 0;
    else
        theMaxX = UMFloor(theMapPoint.GetX());
    thePoint.SetX((float)inRight);
    thePoint.SetY((float)inTop);
    this->mMapToPixel.Apply(thePoint, theMapPoint);
    if (theMapPoint.GetY() > kMiniMapSize)
        theMaxY = 0x80;
    else if (theMapPoint.GetY() < 0.0f)
        theMaxY = 0;
    else
        theMaxY = UMFloor(theMapPoint.GetY());
    this->RedrawPixels(theMinX, theMaxX, theMinY, theMaxY);
    this->mB12184 = true;
    this->mTerrainCriticalSection->Leave();
}

// FUNCTION: EXE 0x5202f2
// Recomputes the terrain colour of each pixel in a texture rectangle
void EEMiniMap::RedrawPixels(long inLeft, long inRight, long inBottom, long inTop)
{
    GEColor theColor;
    this->mTerrainTexture->GetSurface()->Lock();
    unsigned short* theBits = reinterpret_cast<unsigned short*>(this->mTerrainTexture->GetSurface()->GetMemoryAddress());
    unsigned long thePitch = this->mTerrainTexture->GetSurface()->GetBytesPerRow() >> 1;
    for (long theY = inBottom; theY < inTop; theY++) {
        unsigned short* thePixel = &theBits[thePitch * theY + inLeft];
        for (long theX = inLeft; theX < inRight; theX++, thePixel++) {
            this->GetPixelColor(theX, theY, theColor);
            *thePixel = (unsigned short)GEPackColor(kPixelFormatR5G6B5, theColor, 1.0f);
        }
    }
    this->mTerrainTexture->GetSurface()->Unlock();
}

// FUNCTION: EXE 0x5203b9
// Moves a unit's dot between mini map cells (or just updates its position within a cell); -10000 means
// not on the map. Classes without a mini map colour and other players' foundations get no dot
// NEAR: the 1.0f push as in AddDoppelganger; slot order of mU38/the new cell
void EEMiniMap::MoveUnit(EEUnit* inUnit, long inOldX, long inOldY, long inX, long inY)
{
    GE3DPoint thePoint;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EEUnitClass* theClass = inUnit->mUnitClass;
    long theResourceType = inUnit->mResourceType;
    GE3DPoint theNewPoint;
    long theOldCellX = -10000;
    long theOldCellY = -10000;
    long theCellX = -10000;
    long theCellY = -10000;
    GE3DPoint theMapPoint;
    if (theClass->mMiniMapColor == 0x44)
        return;
    if (inOldX == inX && inOldY == inY)
        return;
    if (inUnit->mComplexUnit && inUnit->mComplexUnit->IsFoundation() && inUnit->mPlayer != thePlayer)
        return;
    unsigned long theColor;
    if (inUnit->mPlayer->mPlayerID == 0)
        theColor = GEPackColor(kPixelFormatA4R4G4B4, EEColorTable::GetColor2(theClass->mMiniMapColor), 1.0f);
    else
        theColor = GEPackColor(kPixelFormatA4R4G4B4, inUnit->mPlayer->mColor, 1.0f);
    EEMiniMapUnitDot theDot;
    theDot.mPlayer = (unsigned short)inUnit->mPlayer->mPlayerID;
    theDot.mColor = (unsigned short)theColor;
    theDot.mFlag = theClass->mCategory == kUnitCategoryResource && (theClass->mUnitType == kUnitTypeResForageBush || theResourceType == kResourceIron || theResourceType == kResourceGold || theResourceType == kResourceStone) ? 1 : 0;
    if (inOldX != -10000) {
        thePoint.SetX(theClass->mTileSize * 0.5f + inOldX);
        thePoint.SetY(theClass->mTileSize * 0.5f + inOldY);
        this->mMapToCell.Apply(thePoint, theMapPoint);
        theOldCellX = UMFloatToLong(theMapPoint.GetX());
        theOldCellY = UMFloatToLong(theMapPoint.GetY());
    }
    if (inX != -10000) {
        theNewPoint.SetX(theClass->mTileSize * 0.5f + inX);
        theNewPoint.SetY(theClass->mTileSize * 0.5f + inY);
        this->mMapToCell.Apply(theNewPoint, theMapPoint);
        theCellX = UMFloatToLong(theMapPoint.GetX());
        theCellY = UMFloatToLong(theMapPoint.GetY());
    }
    if (theCellX == theOldCellX && theCellY == theOldCellY) {
        theDot.mX = inOldX;
        theDot.mY = inOldY;
        UListNode<EEMiniMapUnitDot>* theNode = this->mMapUnits[theOldCellX * 0x40 + theOldCellY].Find(theDot);
        if (theNode) {
            theNode->mData.mX = inX;
            theNode->mData.mY = inY;
        }
    } else {
        if (inOldX != -10000) {
            theDot.mX = inOldX;
            theDot.mY = inOldY;
            UList<EEMiniMapUnitDot>& theList = this->mMapUnits[theOldCellX * 0x40 + theOldCellY];
            UListNode<EEMiniMapUnitDot>* theNode = theList.Find(theDot);
            if (theNode)
                theList.Remove(theNode);
            this->RedrawCell(theOldCellX, theOldCellY);
        }
        if (inX != -10000) {
            theDot.mX = inX;
            theDot.mY = inY;
            UList<EEMiniMapUnitDot>& theList = this->mMapUnits[theCellX * 0x40 + theCellY];
            if (theList.mHead && theList.mHead->mData.mFlag)
                theList.InsertAfter(theList.mHead, theDot);
            else
                theList.AddHead(theDot);
            this->RedrawCell(theCellX, theCellY);
        }
    }
}

// TEMPLATE: EXE 0x5206c2
// EEMiniMapUnitDot::operator== (Find's compare in MoveUnit)

// TEMPLATE: EXE 0x5206f2
// UMemoryPool<UListNode<EEMiniMapDot> >::SetSize

// TEMPLATE: EXE 0x5207ff
// UList<EEMiniMapDot>::Find

// TEMPLATE: EXE 0x520826
// UList<EEMiniMapDot>::Clear

// TEMPLATE: EXE 0x52084b
// UList<EEMiniMapDot>::InsertAfter (NEAR: the original stores node->mPrev before testing inPrevious->mNext)

// TEMPLATE: EXE 0x5208a5
// UList<EEMiniMapDot>::AddHead

// TEMPLATE: EXE 0x5208df
// UList<EEMiniMapDot>::Remove

// TEMPLATE: EXE 0x520919
// UMemoryPool<UListNode<EEMiniMapUnitDot> >::SetSize

// TEMPLATE: EXE 0x520a26
// UMemoryPool<...>::Release (ICF-folded)

// TEMPLATE: EXE 0x520a83
// UList<EEMiniMapUnitDot>::Clear

// TEMPLATE: EXE 0x520aa8
// UList<EEMiniMapUnitDot>::InsertAfter (NEAR: as 0x52084b)

// TEMPLATE: EXE 0x520b01
// UList<EEMiniMapUnitDot>::AddHead

// TEMPLATE: EXE 0x520b3b
// UList<EEMiniMapUnitDot>::Remove

// SYNTHETIC: EXE 0x520b75
// (the node pools' `$E` chain: UList<EEMiniMapDot>::sNodePool, then UList<EEMiniMapUnitDot>::sNodePool)
// SYNTHETIC: EXE 0x520b89   (UList<EEMiniMapDot>::sNodePool `$D`: guarded ctor)
// SYNTHETIC: EXE 0x520ba4   (registrar)
// SYNTHETIC: EXE 0x520bb0   (UList<EEMiniMapUnitDot>::sNodePool `$D`)
// SYNTHETIC: EXE 0x520bcb   (registrar)
// SYNTHETIC: EXE 0x520bd7   (UList<EEMiniMapUnitDot>::sNodePool `$E`: guarded dtor)

// TEMPLATE: EXE 0x520bfc
// UMemoryPool<UListNode<EEMiniMapUnitDot> >::`scalar deleting destructor'

// SYNTHETIC: EXE 0x520c1e   (UList<EEMiniMapDot>::sNodePool `$E`: guarded dtor)

// TEMPLATE: EXE 0x520c43
// UMemoryPool<UListNode<EEMiniMapDot> >::`scalar deleting destructor'

// TEMPLATE: EXE 0x520c65
// UMemoryPoolChunk<...>::`scalar deleting destructor' (ICF-folded)

// TEMPLATE: EXE 0x520c8d
// UMemoryPool<UListNode<EEMiniMapDot> >::UMemoryPool

// TEMPLATE: EXE 0x520cc7
// UMemoryPool<UListNode<EEMiniMapUnitDot> >::UMemoryPool

// TEMPLATE: EXE 0x520d01
// UMemoryPoolChunk<UListNode<EEMiniMapDot> >::UMemoryPoolChunk

// TEMPLATE: EXE 0x520d6c
// UMemoryPoolChunk<UListNode<EEMiniMapUnitDot> >::UMemoryPoolChunk

// TEMPLATE: EXE 0x520dd8
// UMemoryPool<UListNode<EEMiniMapDot> >::Delete

// TEMPLATE: EXE 0x520e2e
// UMemoryPool<UListNode<EEMiniMapDot> >::New

// TEMPLATE: EXE 0x520f11
// UMemoryPool<UListNode<EEMiniMapUnitDot> >::Delete

// TEMPLATE: EXE 0x520f70
// UMemoryPool<UListNode<EEMiniMapUnitDot> >::New

// TEMPLATE: EXE 0x521059
// UMemoryPool<UListNode<EEMiniMapDot> >::AddChunk

// TEMPLATE: EXE 0x5210cc
// UMemoryPool<UListNode<EEMiniMapUnitDot> >::AddChunk

// TEMPLATE: EXE 0x52113f
// EEColorTable::GetDarkColor

// TEMPLATE: EXE 0x521168
// EEColorTable::GetColor2
