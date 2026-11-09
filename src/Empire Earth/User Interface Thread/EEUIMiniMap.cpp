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

extern unsigned long& gTexturePixelDepth;    // 0x5308b4

// (EEMSendFlare's inline ctor: ee/EEMessage.h)






static GEBitmap* sMapBitmap;                // 0x565328 the terrain texture
static GEBitmap* sWorldBitmap;              // 0x56532c the units' texture (twice the scale)
static GEBitmap* sFlareBitmap;              // 0x565330 "textures\Flare"
GERectangle EEUIMiniMap::sBounds;           // 0x565318
static GEColor sFlareColor(1.0f, 0.0f, 0.0f);   // 0x565308
static GEColor sFlareBlinkColor;            // 0x5652f8 (1, 1, 1)

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

// NEAR: the original inlines GEBitmap::GetWidth (mXScale * mWidth: the exe view declares it out of line, header
// item) and keeps a zero in ebx; the corner arithmetic's x87 order differs
// FUNCTION: EXE 0x675585
// Snapshots the mini map for drawing: refreshes the units texture, scales the map to the window, places the
// camera outline and the blinking flare (redrawing the screen when the flare leaves the map's inner area)
long EEUIMiniMap::CopyData()
{
    GERectangle theRect;
    GEColor theColor;
    GE2DPoint theCorner2;
    GE2DPoint theCorner3;
    GE2DPoint theCorner0;
    GE2DPoint theCorner1;
    GE2DPoint theFlare;
    GETransformation theTransformation(false);
    float theLeft = this->GetRectangle().GetLeft();
    float theTop = this->GetRectangle().GetTop();
    float theScaleX = gGameWindow->GetWidth() * 0.00125f;
    float theScaleY = gGameWindow->GetHeight() * 0.0016666667f;
    bool theFlareOn = gMiniMap->mB12185;
    gMiniMap->UpdateWorldTexture();
    theCorner2.SetX(gMiniMap->mCorner2.GetX() * theScaleX + this->mOrigin.GetX() - theLeft);
    theCorner2.SetY(gMiniMap->mCorner2.GetY() * theScaleY + this->mOrigin.GetY() - theTop);
    theCorner0.SetX(gMiniMap->mCorner0.GetX() * theScaleX + this->mOrigin.GetX() - theLeft);
    theCorner0.SetY(gMiniMap->mCorner0.GetY() * theScaleY + this->mOrigin.GetY() - theTop);
    theCorner3.SetX(gMiniMap->mCorner3.GetX() * theScaleX + this->mOrigin.GetX() - theLeft);
    theCorner3.SetY(gMiniMap->mCorner3.GetY() * theScaleY + this->mOrigin.GetY() - theTop);
    theCorner1.SetX(gMiniMap->mCorner1.GetX() * theScaleX + this->mOrigin.GetX() - theLeft);
    theCorner1.SetY(gMiniMap->mCorner1.GetY() * theScaleY + this->mOrigin.GetY() - theTop);
    sMapBitmap->SetLocation(this->mOrigin);
    sWorldBitmap->SetLocation(this->mOrigin);
    sMapBitmap->SetXYScale(theScaleX, theScaleY);
    sWorldBitmap->SetXYScale(2.0f * theScaleX, 2.0f * theScaleY);
    this->mDrawLine1 = true;
    this->mDrawLine3 = true;
    this->mDrawLine0 = true;
    this->mDrawLine2 = true;
    bool theHide = this->mShowFlare && !theFlareOn;
    this->mShowFlare = theFlareOn && !gFreeCamera;
    if (this->mShowFlare) {
        theTransformation = gMiniMap->mMapToPixel;
        theTransformation.Apply(gMiniMap->mU1216C, theFlare);
        if (this->mFlareBlink)
            theColor = sFlareColor;
        else
            theColor = sFlareBlinkColor;
        this->mFlareBlink = !this->mFlareBlink;
        sFlareBitmap->SetColor(theColor);
        float theX = this->mOrigin.GetX();
        unsigned long theWidth = sFlareBitmap->GetTexture(gScreenIndex)->GetHeight();
        theFlare.SetX(theFlare.GetX() * theScaleX + theX - theWidth * theScaleX * 0.375f);
        float theY = theFlare.GetY() * theScaleY + this->mOrigin.GetY();
        unsigned long theHeight = sFlareBitmap->GetTexture(gScreenIndex)->GetWidth();
        theFlare.SetY(theY - theHeight * theScaleY * 0.375f);
        if (sFlareBitmap->GetLocation() == theFlare) {
            float theInset = sFlareBitmap->GetWidth() * 1.3333334f;
            theRect = this->GetRectangle();
            theRect.SetLeft(theRect.GetLeft() + theInset);
            theRect.SetRight(theRect.GetRight() - theInset);
            theRect.SetTop(theRect.GetTop() + theInset);
            theRect.SetBottom(theRect.GetBottom() - theInset);
            if (!theRect.Contains(sFlareBitmap->GetLocation()))
                gUIManager->RedrawAll();
        }
        sFlareBitmap->SetLocation(theFlare);
        sFlareBitmap->SetXYScale(0.75f * theScaleX, 0.75f * theScaleY);
    } else {
        this->mFlareBlink = true;
        if (theHide) {
            float theInset = sFlareBitmap->GetWidth() * 1.3333334f;
            theRect = this->GetRectangle();
            theRect.SetLeft(theRect.GetLeft() + theInset);
            theRect.SetRight(theRect.GetRight() - theInset);
            theRect.SetTop(theRect.GetTop() + theInset);
            theRect.SetBottom(theRect.GetBottom() - theInset);
            if (!theRect.Contains(sFlareBitmap->GetLocation()))
                gUIManager->RedrawAll();
        }
    }
    this->mLines[0].SetStartPoint(theCorner0);
    this->mLines[0].SetEndPoint(theCorner2);
    this->mLines[1].SetStartPoint(theCorner2);
    this->mLines[1].SetEndPoint(theCorner3);
    this->mLines[2].SetStartPoint(theCorner3);
    this->mLines[2].SetEndPoint(theCorner1);
    this->mLines[3].SetStartPoint(theCorner1);
    this->mLines[3].SetEndPoint(theCorner0);
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

// NEAR: inline budget: the original calls vector::operator[] (1st GetViewport) and begin/_Ptrit::operator+ (2nd)
// out of line
// FUNCTION: EXE 0x675ba1
// Draws the terrain, the units, the flare and the camera outline clipped to the map
long EEUIMiniMap::DrawPolygons()
{
    GEViewport theViewport;
    GERectangle theViewRect;
    GERectangle theMapRect;
    long theResult;
    unsigned long theScreen;
    gMiniMap->mTerrainCriticalSection->Enter();
    UIControl::DrawPolygons();
    theScreen = gScreenIndex;
    this->GetRasterizer()->Draw(sMapBitmap, theScreen);
    theScreen = gScreenIndex;
    theResult = this->GetRasterizer()->Draw(sWorldBitmap, theScreen);
    if (this->mShowFlare) {
        theScreen = gScreenIndex;
        theResult = this->GetRasterizer()->Draw(sFlareBitmap, theScreen);
    }
    theScreen = gScreenIndex;
    theViewRect = this->GetRasterizer()->GetViewport(theScreen).GetViewRectangle();
    theMapRect = this->mCopiedDrawRectangle;
    theMapRect.SetTop(theMapRect.GetTop() + 1.0f);
    theMapRect.SetLeft(theMapRect.GetLeft() + 1.0f);
    theMapRect.SetBottom(theMapRect.GetBottom() - 1.0f);
    theMapRect.SetRight(theMapRect.GetRight() - 1.0f);
    theScreen = gScreenIndex;
    theViewport = this->GetRasterizer()->GetViewport(theScreen);
    theViewport.SetViewRectangle(theMapRect);
    theScreen = gScreenIndex;
    this->GetRasterizer()->SetViewport(theViewport, theScreen);
    if (this->mDrawLine0) {
        theScreen = gScreenIndex;
        theResult = this->GetRasterizer()->Draw(&this->mLines[0], 1, theScreen);
    }
    if (this->mDrawLine1) {
        theScreen = gScreenIndex;
        theResult = this->GetRasterizer()->Draw(&this->mLines[1], 1, theScreen);
    }
    if (this->mDrawLine2) {
        theScreen = gScreenIndex;
        theResult = this->GetRasterizer()->Draw(&this->mLines[2], 1, theScreen);
    }
    if (this->mDrawLine3) {
        theScreen = gScreenIndex;
        theResult = this->GetRasterizer()->Draw(&this->mLines[3], 1, theScreen);
    }
    theScreen = gScreenIndex;
    theViewport = this->GetRasterizer()->GetViewport(theScreen);
    theViewport.SetViewRectangle(theViewRect);
    theScreen = gScreenIndex;
    this->GetRasterizer()->SetViewport(theViewport, theScreen);
    gMiniMap->mTerrainCriticalSection->Leave();
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
    thePoint.SetX((this->GetMouseLocation().GetX() - this->mOrigin.GetX()) / (theWidth * 0.00125f));
    thePoint.SetY((this->GetMouseLocation().GetY() - this->mOrigin.GetY()) / (theHeight * 0.0016666667f));
    if (!EEUIMiniMap::sBounds.Contains(thePoint)) {
        if (EEUIMiniMap::sBounds.GetLeft() > thePoint.GetX())
            thePoint.SetX(EEUIMiniMap::sBounds.GetLeft());
        if (EEUIMiniMap::sBounds.GetRight() < thePoint.GetX())
            thePoint.SetX(EEUIMiniMap::sBounds.GetRight());
        if (thePoint.GetY() < EEUIMiniMap::sBounds.GetTop())
            thePoint.SetY(EEUIMiniMap::sBounds.GetTop());
        if (thePoint.GetY() > EEUIMiniMap::sBounds.GetBottom())
            thePoint.SetY(EEUIMiniMap::sBounds.GetBottom());
    }
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
// the camera there
long EEUIMiniMap::MouseLButtonUp()
{
    GE2DPoint theWorld;
    GE2DPoint theLocal;
    U2DPoint<long> theTile;
    GETransformation theTransformation(false);
    if (!this->GetHasMouseCapture())
        return 0;
    if (gMouse->mPointer != 0x16)
        this->CenterCamera();
    else if (this->GetRectangle().Contains(this->GetForm()->GetMouseLocation())) {
        theLocal.SetX(this->GetForm()->GetMouseLocation().GetX() - this->GetRectangle().GetLeft());
        theLocal.SetY(this->GetForm()->GetMouseLocation().GetY() - this->GetRectangle().GetTop());
        theTransformation = gMiniMap->mPixelToMap;
        theTransformation.Apply(theLocal, theWorld);
        theTile.SetX(theWorld.GetX());
        theTile.SetY(theWorld.GetY());
        if (::gWorld.mMap->GetTileAtLocation(theTile) != NULL) {
            long theX = theWorld.GetX();
            long theY = theWorld.GetY();
            gCommunications->ProcessLocalUserInput(new EEMSendFlare(U2DPoint<long>(theX, theY)), kSlotNone);
            gMouse->mPointer = 0;
            static_cast<EEUIFMainGame*>(this->GetForm())->SetUIState(0);
        }
    }
    this->ReleaseCapturedMouse();
    return 0;
}

// FUNCTION: EXE 0x6760af
// Keeps centring the camera while the left button drags over the mini map (not in flare mode)
long EEUIMiniMap::MouseMove(const GE2DPoint& inLocation)
{
    if (this->GetHasMouseCapture() && this->GetLeftMouseButtonDown() && gMouse->mPointer != 0x16) {
        this->CenterCamera();
        this->SetDirty(true);
    }
    return 0;
}

// FUNCTION: EXE 0x6760ec
// Centres the camera on the map point under the mouse, clamped to the map
void EEUIMiniMap::CenterCamera()
{
    GE2DPoint thePoint;
    unsigned long theWidth = gGameWindow->GetWidth();
    unsigned long theHeight = gGameWindow->GetHeight();
    thePoint.SetX((this->GetMouseLocation().GetX() - this->mOrigin.GetX()) / (theWidth * 0.00125f));
    thePoint.SetY((this->GetMouseLocation().GetY() - this->mOrigin.GetY()) / (theHeight * 0.0016666667f));
    if (!EEUIMiniMap::sBounds.Contains(thePoint)) {
        if (EEUIMiniMap::sBounds.GetLeft() > thePoint.GetX())
            thePoint.SetX(EEUIMiniMap::sBounds.GetLeft());
        else if (EEUIMiniMap::sBounds.GetRight() < thePoint.GetX())
            thePoint.SetX(EEUIMiniMap::sBounds.GetRight());
        if (thePoint.GetY() < EEUIMiniMap::sBounds.GetTop())
            thePoint.SetY(EEUIMiniMap::sBounds.GetTop());
        else if (thePoint.GetY() > EEUIMiniMap::sBounds.GetBottom())
            thePoint.SetY(EEUIMiniMap::sBounds.GetBottom());
    }
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

// NEAR: the original loads the 0.5f literal once onto the FP stack for both point coordinates, and the min()
// compare is reversed
// FUNCTION: EXE 0x676236
// Points the mini map bitmaps at a new map's terrain and units textures and recomputes the map bounds
void EEUIMiniMap::Update(EEMiniMap* inMiniMap)
{
    GE2DPoint theTo;
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GETextureCoordinate theBottomRight;
    GETextureCoordinate theBottomLeft;
    GETransformation theTransformation(false);
    EEMap* theMap = ::gWorld.mMap;
    if (inMiniMap == NULL)
        return;
    long theWidth = theMap->mL195614;
    long theHeight = theMap->mL195610;
    float theSize = min(theWidth, theHeight);
    if (theSize < 64.0f)
        theSize = theSize * 0.015625f;
    else
        theSize = 1.0f;
    theTopRight.SetU(theSize);
    theBottomRight.SetV(theSize);
    theBottomLeft.SetU(theSize);
    theBottomLeft.SetV(theSize);
    if (sWorldBitmap != NULL)
        sWorldBitmap->SetTextureCoordinates(theTopLeft, theTopRight, theBottomRight, theBottomLeft);
    theTransformation = inMiniMap->mMapToPixel;
    theTransformation.Apply(GE2DPoint(0.5f, theMap->mL195610 - 1.0f + 0.5f), theTo);
    EEUIMiniMap::sBounds.SetLeft(theTo.GetX());
    theTransformation.Apply(GE2DPoint(theMap->mSize - 1.0f + 0.5f, theMap->mL195614 + 0.5f), theTo);
    EEUIMiniMap::sBounds.SetRight(theTo.GetX());
    theTransformation.Apply(GE2DPoint(0.5f, theMap->mL195610 + 0.5f), theTo);
    EEUIMiniMap::sBounds.SetTop(theTo.GetY());
    theTransformation.Apply(GE2DPoint(theMap->mSize - 1.0f + 0.5f, theMap->mL195614 - 1.0f + 0.5f), theTo);
    EEUIMiniMap::sBounds.SetBottom(theTo.GetY());
    gMiniMapTransformation = inMiniMap->mPixelToMap;
    if (sMapBitmap != NULL)
        sMapBitmap->SetTexture(inMiniMap->mTerrainTexture, gScreenIndex);
    if (sWorldBitmap != NULL)
        sWorldBitmap->SetTexture(inMiniMap->mWorldTexture, gScreenIndex);
}

// FUNCTION: EXE 0x676420
// Creates the mini map click action
// (EEAMiniMap's inline ctor, emitted after its first user, the ctor)
