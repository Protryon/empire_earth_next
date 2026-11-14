// C:\EEX\Empire Earth\User Interface Thread\EEUIMiniMap.cpp (file name inferred: no __FILE__; RTTI class
// EEUIMiniMap). The mini-map control: 0x2753b1..0x276442 (the `$E` chains of sBounds and the two flare colours, ctor,
// ??_G, dtor, then the members in address order, EEAMiniMap's inline ctor after its first user). Unit flags: the exe
// module set.
#include <windows.h>
#include <mbstring.h>
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
#include "lle/GETerrainMesh.h"
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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEMouse.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEAction.h"
#include "ee/EEAMiniMap.h"
#include "ee/EEUIMiniMap.h"
#include "rust/ee_ge.h"   // ee_gfx minimap.rs (RUST_PLAN R4.4)
#include "rust/ee_gfx.h"
#include "rust/ee_screens.h"   // mini_map.rs

extern unsigned long& gTexturePixelDepth;    // 0x5308b4

// (EEMSendFlare's inline ctor: ee/EEMessage.h)






static GEBitmap* sMapBitmap;                // 0x565328 the terrain texture
static GEBitmap* sWorldBitmap;              // 0x56532c the units' texture (twice the scale)
static GEBitmap* sFlareBitmap;              // 0x565330 "textures\Flare"
GERectangle EEUIMiniMap::sBounds;           // 0x565318
static GEColor sFlareColor(1.0f, 0.0f, 0.0f);   // 0x565308
static GEColor sFlareBlinkColor;            // 0x5652f8 (1, 1, 1)

// gMiniMap's transformations' 2D entries (m00 m01 m03 m10 m11 m13) for ee_screens mini_map.rs
static void EEUIMiniMapMatrix(const GETransformation& inTransformation, float outMatrix[6])
{
    outMatrix[0] = inTransformation.mMatrix[0][0];
    outMatrix[1] = inTransformation.mMatrix[0][1];
    outMatrix[2] = inTransformation.mMatrix[0][3];
    outMatrix[3] = inTransformation.mMatrix[1][0];
    outMatrix[4] = inTransformation.mMatrix[1][1];
    outMatrix[5] = inTransformation.mMatrix[1][3];
}

// SYNTHETIC: EXE 0x6753b1
// SYNTHETIC: EXE 0x6753bb
// SYNTHETIC: EXE 0x6753dc
// SYNTHETIC: EXE 0x6753e8
// SYNTHETIC: EXE 0x6753f9
// SYNTHETIC: EXE 0x675403
// SYNTHETIC: EXE 0x67541c
// SYNTHETIC: EXE 0x675428
// SYNTHETIC: EXE 0x675429
// SYNTHETIC: EXE 0x675433
// SYNTHETIC: EXE 0x67544c
// SYNTHETIC: EXE 0x675458

// FUNCTION: EXE 0x675459
// Creates the mini map control with its click action, the map drawn inside its border
EEUIMiniMap::EEUIMiniMap(const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inId,
                         unsigned long inBorderThickness)
    : UIControl(kControlTypeCustom, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mAction = new EEAMiniMap;
    float theBorder = inBorderThickness;
    this->mOrigin.SetX(inRectangle.GetLeft() + theBorder);
    this->mOrigin.SetY(inRectangle.GetTop() + theBorder);
    this->mDrawLine0 = false;
    this->mDrawLine1 = false;
    this->mDrawLine2 = false;
    this->mDrawLine3 = false;
    this->mShowFlare = false;
    this->mFlareBlink = true;
}

// FUNCTION: EXE 0x675523
// Releases the mouse after a right click on the mini map
long EEUIMiniMap::MouseRButtonUp()
{
    this->ReleaseCapturedMouse();
    return 0;
}

// SYNTHETIC: EXE 0x67552b
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x675547
// Destroys the mini map control and its click action
EEUIMiniMap::~EEUIMiniMap()
{
    if (this->mAction != NULL)
        this->mAction->Dereference();
}

// FUNCTION: EXE 0x675585
// Snapshots the mini map for drawing: refreshes the units texture, scales the map to the window, places the
// camera outline and the blinking flare (redrawing the screen when the flare leaves the map's inner area); the
// numbers are ee_screens mini_map.rs', the C++ reads and applies them in the original's order
long EEUIMiniMap::CopyData()
{
    EeScreensMiniMapCopy theCopy = {};
    const GERectangle& theRect = this->GetRectangle();
    theCopy.flare_on = gMiniMap->mB12185;
    gMiniMap->UpdateWorldTexture();
    const GE2DPoint* theCorners[4] = {&gMiniMap->mCorner0, &gMiniMap->mCorner1, &gMiniMap->mCorner2, &gMiniMap->mCorner3};
    for (int i = 0; i < 4; i++) {
        theCopy.corners[i][0] = theCorners[i]->GetX();
        theCopy.corners[i][1] = theCorners[i]->GetY();
    }
    theCopy.rect[0] = theRect.GetLeft();
    theCopy.rect[1] = theRect.GetTop();
    theCopy.rect[2] = theRect.GetRight();
    theCopy.rect[3] = theRect.GetBottom();
    theCopy.window_width = (uint32_t)gGameWindow->GetWidth();
    theCopy.window_height = (uint32_t)gGameWindow->GetHeight();
    theCopy.origin[0] = this->mOrigin.GetX();
    theCopy.origin[1] = this->mOrigin.GetY();
    theCopy.flare_point[0] = gMiniMap->mU1216C.GetX();
    theCopy.flare_point[1] = gMiniMap->mU1216C.GetY();
    EEUIMiniMapMatrix(gMiniMap->mMapToPixel, theCopy.map_to_pixel);
    if (GETexture* theTexture = sFlareBitmap->GetTexture(gScreenIndex)) {
        theCopy.flare_texture_height = (uint32_t)theTexture->GetHeight();
        theCopy.flare_texture_width = (uint32_t)theTexture->GetWidth();
    }
    theCopy.flare_location[0] = sFlareBitmap->GetLocation().GetX();
    theCopy.flare_location[1] = sFlareBitmap->GetLocation().GetY();
    theCopy.flare_width = sFlareBitmap->GetWidth();
    theCopy.free_camera = gFreeCamera;
    theCopy.show_flare = this->mShowFlare;
    theCopy.flare_blink = this->mFlareBlink;
    ee_screens_mini_map_copy_data(&theCopy);
    sMapBitmap->SetLocation(this->mOrigin);
    sWorldBitmap->SetLocation(this->mOrigin);
    sMapBitmap->SetXYScale(theCopy.map_scale[0], theCopy.map_scale[1]);
    sWorldBitmap->SetXYScale(theCopy.world_scale[0], theCopy.world_scale[1]);
    this->mDrawLine1 = this->mDrawLine3 = this->mDrawLine0 = this->mDrawLine2 = true;
    this->mShowFlare = theCopy.show_flare != 0;
    this->mFlareBlink = theCopy.flare_blink != 0;
    if (theCopy.flare_color != EE_SCREENS_MM_COLOR_NONE)
        sFlareBitmap->SetColor(theCopy.flare_color == EE_SCREENS_MM_COLOR_FLARE ? sFlareColor : sFlareBlinkColor);
    if (theCopy.redraw_all)
        gUIManager->RedrawAll();
    if (theCopy.set_flare) {
        sFlareBitmap->SetLocation(GE2DPoint(theCopy.flare_new_location[0], theCopy.flare_new_location[1]));
        sFlareBitmap->SetXYScale(theCopy.flare_scale[0], theCopy.flare_scale[1]);
    }
    for (int i = 0; i < 4; i++) {
        this->mLines[i].SetStartPoint(GE2DPoint(theCopy.lines[i][0][0], theCopy.lines[i][0][1]));
        this->mLines[i].SetEndPoint(GE2DPoint(theCopy.lines[i][1][0], theCopy.lines[i][1][1]));
    }
    return UIControl::CopyData();
}

// FUNCTION: EXE 0x675a2d
// Creates the terrain, units and flare bitmaps and loads the flare texture
void EEUIMiniMap::LoadMedia()
{
    GETextureDescription theDescription;
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    sMapBitmap = new GEBitmap;
    sWorldBitmap = new GEBitmap;
    sFlareBitmap = new GEBitmap;
    GETexture* theTexture = gTextureManager->LoadTexture(UString("textures\\Flare"), theDescription);
    theTexture->SetStageAlphaMode(kStageOpModulate, -1);
    theTexture->SetStageColorMode(kStageOpModulate, -1);
    theTexture->SetStageMinFilterMode(1, -1);
    theTexture->SetStageMagFilterMode(1, -1);
    theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    theTexture->SetStageMipMapBias(0.0f, -1);
    theTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
    sFlareBitmap->SetTexture(theTexture, gScreenIndex);
}

// FUNCTION: EXE 0x675b67
// Releases the terrain, units and flare bitmaps
void EEUIMiniMap::UnloadMedia()
{
    sMapBitmap->Dereference();
    sMapBitmap = NULL;
    sWorldBitmap->Dereference();
    sWorldBitmap = NULL;
    sFlareBitmap->Dereference();
    sFlareBitmap = NULL;
}

// FUNCTION: EXE 0x675ba1
// Draws the terrain, the units, the flare and the camera outline clipped to the map: ee_screens mini_map.rs' plan,
// executed in its order (the result is the last bitmap's or line's draw)
long EEUIMiniMap::DrawPolygons()
{
    uint32_t theLines = (this->mDrawLine0 ? 1 : 0) | (this->mDrawLine1 ? 2 : 0) | (this->mDrawLine2 ? 4 : 0) |
                        (this->mDrawLine3 ? 8 : 0);
    const GERectangle& theCopied = this->mCopiedDrawRectangle;
    EeScreensMiniMapDraw thePlan = ee_screens_mini_map_draw_plan(this->mShowFlare, theLines, theCopied.GetLeft(),
                                                                 theCopied.GetTop(), theCopied.GetRight(),
                                                                 theCopied.GetBottom());
    GEViewport theViewport;
    GERectangle theViewRect;
    GERectangle theClip;
    theClip.SetLeft(thePlan.clip[0]);
    theClip.SetTop(thePlan.clip[1]);
    theClip.SetRight(thePlan.clip[2]);
    theClip.SetBottom(thePlan.clip[3]);
    long theResult = 0;
    GERasterizer* theRasterizer = this->GetRasterizer();
    for (uint32_t i = 0; i < thePlan.count; i++) {
        unsigned long theScreen = gScreenIndex;
        uint8_t theStep = thePlan.steps[i];
        switch (theStep) {
        case EE_SCREENS_MM_DRAW_ENTER: gMiniMap->mTerrainCriticalSection->Enter(); break;
        case EE_SCREENS_MM_DRAW_BASE: UIControl::DrawPolygons(); break;
        case EE_SCREENS_MM_DRAW_MAP: theRasterizer->Draw(sMapBitmap, theScreen); break;
        case EE_SCREENS_MM_DRAW_WORLD: theResult = theRasterizer->Draw(sWorldBitmap, theScreen); break;
        case EE_SCREENS_MM_DRAW_FLARE: theResult = theRasterizer->Draw(sFlareBitmap, theScreen); break;
        case EE_SCREENS_MM_DRAW_CLIP:
        case EE_SCREENS_MM_DRAW_RESTORE:
            if (theStep == EE_SCREENS_MM_DRAW_CLIP)
                theViewRect = theRasterizer->GetViewport(theScreen).GetViewRectangle();
            theViewport = theRasterizer->GetViewport(gScreenIndex);
            theViewport.SetViewRectangle(theStep == EE_SCREENS_MM_DRAW_CLIP ? theClip : theViewRect);
            theRasterizer->SetViewport(theViewport, gScreenIndex);
            break;
        case EE_SCREENS_MM_DRAW_LEAVE: gMiniMap->mTerrainCriticalSection->Leave(); break;
        default: theResult = theRasterizer->Draw(&this->mLines[theStep - EE_SCREENS_MM_DRAW_LINE], 1, theScreen); break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x675e45
// On a right press, posts a mini map action (mode 1) at the clicked map point, clamped to the map
long EEUIMiniMap::MouseRButtonDown()
{
    GE2DPoint thePoint;
    unsigned long theWidth = gGameWindow->GetWidth();
    unsigned long theHeight = gGameWindow->GetHeight();
    unsigned long theModifiers = gISKeyboard->GetModifierState();
    // the mouse in mini-map units, clamped to the map by two tests per axis: ee_gfx minimap.rs (RUST_PLAN R4.4)
    ee_gfx_minimap_press_point(reinterpret_cast<const EeGe2DPoint*>(&this->GetMouseLocation()),
                               reinterpret_cast<const EeGe2DPoint*>(&this->mOrigin), (uint32_t)theWidth,
                               (uint32_t)theHeight, reinterpret_cast<const EeGeRectangle*>(&EEUIMiniMap::sBounds),
                               reinterpret_cast<EeGe2DPoint*>(&thePoint));
    EEAMiniMap* theAction = this->mAction;
    theAction->mMode = 1;
    theAction->mPoint = thePoint;
    theAction->mModifiers = theModifiers;
    gUserInterface->PostAction(theAction);
    this->CaptureMouse();
    return 0;
}

// NEAR: the original's frame has a zeroed dword at -4 (not identified) and builds the message's tile in the
// tile's slot after operator new
// ICF-FOLDED: EXE 0x65ae3b
// Captures the mouse on a left press (folded with EEUICUnitIconBar::MouseRButtonDown in the original)
long EEUIMiniMap::MouseLButtonDown()
{
    this->CaptureMouse();
    return 0;
}

// FUNCTION: EXE 0x675f72
// On a left release: in flare mode sends a flare at the clicked tile and leaves flare mode, otherwise centres
// the camera there (ee_screens mini_map.rs)
long EEUIMiniMap::MouseLButtonUp()
{
    return (long)ee_screens_mini_map_mouse_l_up(this);
}

// FUNCTION: EXE 0x6760af
// Keeps centring the camera while the left button drags over the mini map (not in flare mode) (ee_screens
// mini_map.rs)
long EEUIMiniMap::MouseMove(const GE2DPoint& inLocation)
{
    return (long)ee_screens_mini_map_mouse_move(this);
}

// The Rust's calls on the control, the mouse and the world (EE_SCREENS_MM_OP_*), each the original's
extern "C" int64_t ee_screenscpp_mini_map(void* inMap, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    return EEUIMiniMap::HostOp(static_cast<EEUIMiniMap*>(inMap), inOp, inA, inB, inC);
}

// (a member: the mouse state and CenterCamera are protected)
int64_t EEUIMiniMap::HostOp(EEUIMiniMap* inMap, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    switch (inOp) {
    case EE_SCREENS_MM_OP_HAS_CAPTURE: return inMap->GetHasMouseCapture();
    case EE_SCREENS_MM_OP_LEFT_DOWN: return inMap->GetLeftMouseButtonDown();
    case EE_SCREENS_MM_OP_POINTER: return gMouse->mPointer;
    case EE_SCREENS_MM_OP_CENTER: inMap->CenterCamera(); break;
    case EE_SCREENS_MM_OP_DIRTY: inMap->SetDirty(true); break;
    case EE_SCREENS_MM_OP_READ_MOUSE: {
        EeScreensMiniMapMouse& theMouse = *reinterpret_cast<EeScreensMiniMapMouse*>(inC);
        const GERectangle& theRect = inMap->GetRectangle();
        theMouse.mouse[0] = inMap->GetForm()->GetMouseLocation().GetX();
        theMouse.mouse[1] = inMap->GetForm()->GetMouseLocation().GetY();
        theMouse.rect[0] = theRect.GetLeft();
        theMouse.rect[1] = theRect.GetTop();
        theMouse.rect[2] = theRect.GetRight();
        theMouse.rect[3] = theRect.GetBottom();
        EEUIMiniMapMatrix(gMiniMap->mPixelToMap, theMouse.pixel_to_map);
        break;
    }
    case EE_SCREENS_MM_OP_TILE_EXISTS: return ::gWorld.mMap->GetTileAtLocation(U2DPoint<long>((long)inA, (long)inB)) != NULL;
    case EE_SCREENS_MM_OP_SEND_FLARE:
        gCommunications->ProcessLocalUserInput(new EEMSendFlare(U2DPoint<long>((long)inA, (long)inB)), kSlotNone);
        break;
    case EE_SCREENS_MM_OP_FLARE_DONE:
        gMouse->mPointer = 0;
        static_cast<EEUIFMainGame*>(inMap->GetForm())->SetUIState(0);
        break;
    case EE_SCREENS_MM_OP_RELEASE: inMap->ReleaseCapturedMouse(); break;
    }
    return 0;
}

// FUNCTION: EXE 0x6760ec
// Centres the camera on the map point under the mouse, clamped to the map
void EEUIMiniMap::CenterCamera()
{
    GE2DPoint thePoint;
    // the mouse in mini-map units, clamped to the map: ee_gfx minimap.rs (RUST_PLAN R4.4)
    ee_gfx_minimap_center_point(reinterpret_cast<const EeGe2DPoint*>(&this->GetMouseLocation()),
                                reinterpret_cast<const EeGe2DPoint*>(&this->mOrigin), (uint32_t)gGameWindow->GetWidth(),
                                (uint32_t)gGameWindow->GetHeight(),
                                reinterpret_cast<const EeGeRectangle*>(&EEUIMiniMap::sBounds),
                                reinterpret_cast<EeGe2DPoint*>(&thePoint));
    EEAMiniMap* theAction = this->mAction;
    theAction->mMode = 0;
    theAction->mPoint = thePoint;
    gUserInterface->PostAction(theAction);
}

// FUNCTION: EXE 0x6761f6
// Moves the mini map control and its map origin
void EEUIMiniMap::SetLocation(const GE2DPoint& inLocation)
{
    UIControl::SetLocation(inLocation);
    float theBorder = this->GetBorderThickness();
    this->mOrigin.SetX(inLocation.GetX() + theBorder);
    this->mOrigin.SetY(inLocation.GetY() + theBorder);
}

// FUNCTION: EXE 0x676236
// Points the mini map bitmaps at a new map's terrain and units textures and recomputes the map bounds (the numbers:
// ee_screens mini_map.rs)
void EEUIMiniMap::Update(EEMiniMap* inMiniMap)
{
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GETextureCoordinate theBottomRight;
    GETextureCoordinate theBottomLeft;
    EEMap* theMap = ::gWorld.mMap;
    if (inMiniMap == NULL)
        return;
    EeScreensMiniMapUpdate theUpdate = {};
    theUpdate.width = (int32_t)theMap->mL195614;
    theUpdate.height = (int32_t)theMap->mL195610;
    theUpdate.size = (int32_t)theMap->mSize;
    EEUIMiniMapMatrix(inMiniMap->mMapToPixel, theUpdate.map_to_pixel);
    ee_screens_mini_map_update(&theUpdate);
    theTopRight.SetU(theUpdate.extent);
    theBottomRight.SetV(theUpdate.extent);
    theBottomLeft.SetU(theUpdate.extent);
    theBottomLeft.SetV(theUpdate.extent);
    if (sWorldBitmap != NULL)
        sWorldBitmap->SetTextureCoordinates(theTopLeft, theTopRight, theBottomRight, theBottomLeft);
    EEUIMiniMap::sBounds.SetLeft(theUpdate.bounds[0]);
    EEUIMiniMap::sBounds.SetRight(theUpdate.bounds[2]);
    EEUIMiniMap::sBounds.SetTop(theUpdate.bounds[1]);
    EEUIMiniMap::sBounds.SetBottom(theUpdate.bounds[3]);
    gMiniMapTransformation = inMiniMap->mPixelToMap;
    if (sMapBitmap != NULL)
        sMapBitmap->SetTexture(inMiniMap->mTerrainTexture, gScreenIndex);
    if (sWorldBitmap != NULL)
        sWorldBitmap->SetTexture(inMiniMap->mWorldTexture, gScreenIndex);
}

// FUNCTION: EXE 0x676420
// Creates the mini map click action
// (EEAMiniMap's inline ctor, emitted after its first user, the ctor)
