// C:\EEX\Empire Earth\Graphics Thread\EEROUnits.cpp (__FILE__ 0x4be760). The units' render object (vtable 0x441bbc):
// 0x114296..0x118741 (the player-colour key and eleven colour constants' init chains, ctor, ??_G, dtor, Virtual00,
// the ring/bar/model passes, the labels, Virtual08, Virtual0C, the media release, the screen projection Fn118676);
// EEROUserInterface.cpp's init chain starts at 0x118741. Unit flags: the exe's module set.
// The passes are EEROUnits members; LoadMedia 0x116a97's asserts carry their original __LINE__ 1823/1824.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GEPolygonArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GELine.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEText.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "GEWindow.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EERenderLists.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEROUnits.h"

extern bool gShowCollisionSpheres;          // 0x5318dc (the units' collision spheres are drawn; provisional name)
extern bool gInCinematic;                   // 0x5318e0 (no rings, bars or labels; provisional name)
extern bool gInterpolateFrames;             // 0x5308cf (the frames are interpolated; provisional name)
extern long gModelDetail;                   // 0x530898 (0: full opacity, 1: half, else opaque; provisional name)
extern bool gUnitPanelActive;               // 0x5297a0 (EEROUserInterface.cpp)
extern GEFont* gU531994;                    // 0x531994 (the labels' default font; before gFontTahoma10; provisional name)
// 0x4e5b98: an empty string (EEHumanPlayer's ctors assign it too); zero bytes in the original's initialised .data.
// Size: one dword (the next named datum is 8 bytes on, 0x4e5ba0; the dword between is unreferenced)
char gU4E5B98[4] = "";
extern EERenderUnit* gU529044;              // 0x529044 (the unit with the debug label; EERenderObject.cpp; provisional name)
extern UWideString gU529030;                // 0x529030 (that label; EERenderObject.cpp; provisional name)

const UString kPlayerColorKey("player color");
// SYNTHETIC: EXE 0x514296   (wrapper)
// SYNTHETIC: EXE 0x5142a0   (ctor)
// SYNTHETIC: EXE 0x5142b1   (registrar)
// SYNTHETIC: EXE 0x5142bd   (dtor)

// the rings' and bars' colours (0x5294c0..0x529418) (provisional names)
GEColor gRingHighlightColor(0.537f, 0.678f, 1.0f);   // 0x5294c0
// SYNTHETIC: EXE 0x5142c8   (wrapper)
// SYNTHETIC: EXE 0x5142d2   (ctor)
// SYNTHETIC: EXE 0x5142ef   (registrar)
// SYNTHETIC: EXE 0x5142fb   (dtor)
GEColor gRingColor(0.0f, 1.0f, 0.0f);   // 0x5294b0
// SYNTHETIC: EXE 0x5142fc   (wrapper)
// SYNTHETIC: EXE 0x514306   (ctor)
// SYNTHETIC: EXE 0x51431f   (registrar)
// SYNTHETIC: EXE 0x51432b   (dtor)
GEColor gU5294A0(1.0f, 0.75f, 0.5f);
// SYNTHETIC: EXE 0x51432c   (wrapper)
// SYNTHETIC: EXE 0x514336   (ctor)
// SYNTHETIC: EXE 0x514353   (registrar)
// SYNTHETIC: EXE 0x51435f   (dtor)
GEColor gU529490(1.0f, 0.56f, 0.25f);
// SYNTHETIC: EXE 0x514360   (wrapper)
// SYNTHETIC: EXE 0x51436a   (ctor)
// SYNTHETIC: EXE 0x514387   (registrar)
// SYNTHETIC: EXE 0x514393   (dtor)
GEColor gSelectionRingDark(1.0f, 0.375f, 0.0f);
// SYNTHETIC: EXE 0x514394   (wrapper)
// SYNTHETIC: EXE 0x51439e   (ctor)
// SYNTHETIC: EXE 0x5143b9   (registrar)
// SYNTHETIC: EXE 0x5143c5   (dtor)
GEColor gBarColor(0.0f, 1.0f, 0.0f);
// SYNTHETIC: EXE 0x5143c6   (wrapper)
// SYNTHETIC: EXE 0x5143d0   (ctor)
// SYNTHETIC: EXE 0x5143e9   (registrar)
// SYNTHETIC: EXE 0x5143f5   (dtor)
GEColor gBarBackColor(1.0f, 0.0f, 0.0f);
// SYNTHETIC: EXE 0x5143f6   (wrapper)
// SYNTHETIC: EXE 0x514400   (ctor)
// SYNTHETIC: EXE 0x514419   (registrar)
// SYNTHETIC: EXE 0x514425   (dtor)
GEColor gU529448(1.0f, 1.0f, 0.0f);
// SYNTHETIC: EXE 0x514426   (wrapper)
// SYNTHETIC: EXE 0x514430   (ctor)
// SYNTHETIC: EXE 0x514449   (registrar)
// SYNTHETIC: EXE 0x514455   (dtor)
GEColor gBarBorderColor(0.9f, 0.9f, 0.9f);
// SYNTHETIC: EXE 0x514456   (wrapper)
// SYNTHETIC: EXE 0x514460   (ctor)
// SYNTHETIC: EXE 0x514479   (registrar)
// SYNTHETIC: EXE 0x514485   (dtor)
GEColor gHealthColor(0.0f, 0.0f, 0.7f);
// SYNTHETIC: EXE 0x514486   (wrapper)
// SYNTHETIC: EXE 0x514490   (ctor)
// SYNTHETIC: EXE 0x5144ab   (registrar)
// SYNTHETIC: EXE 0x5144b7   (dtor)
GEColor gHealthBackColor(1.0f, 1.0f, 1.0f);
// SYNTHETIC: EXE 0x5144b8   (wrapper)
// SYNTHETIC: EXE 0x5144c2   (ctor)
// SYNTHETIC: EXE 0x5144db   (registrar)
// SYNTHETIC: EXE 0x5144e7   (dtor)

// the media (provisional names): the ring model, the selection models by size, the bars' rhombi
static GEModel* sRingModel;                 // 0x5294f0
static GEModel* sSelectionModels[3];        // 0x5294f4
GETexture* sTexture4E0;                     // 0x5294e0 (EEE3DCommandConfirmation.cpp reads it)
GETexture* sTexture4E4;                     // 0x5294e4 (EEE3DCommandConfirmation.cpp reads it)
GETexture* sTexture4E8;                     // 0x5294e8 (EEE3DCommandConfirmation.cpp reads it)
static GEModel* sModel4EC;                  // 0x5294ec
static GELine* sLines;                      // 0x529478 (new[])
static GETexture* sDottedLineTexture;       // 0x529474 (the lines' texture)
static GETexture* sSelectionTexture;        // 0x529500
static GETexture* sAreaEffectTexture;       // 0x529504
static GERhombus* sFGBar;              // 0x529508
static GERhombus* sBGBar;          // 0x52950c
long gCollisionSphereMode;                  // 0x529510 (ee/EmpireEarth.h; read-only: EERenderObject, EEMessage. .bss after this
                                            // unit's statics, before EEROUserInterface's gUIViewport 0x529518: either unit)

// the selection model by ring size (0x4be73c; .data)
static float sSelectionSizes[3] = { 0.75f, 1.5f, 100.0f };
// the selection models' ring widths by size (0x4be748)
static float sSelectionWidths[3] = { 0.08f, 0.05f, 0.025f };
// the bars' size (0x4be754..0x4be75c; .data, recomputed per frame from the window size)
static unsigned long sBarWidth = 24;
static float sBarHeight = 2.0f;
static float sBarStep = 5.0f;
// the unit's named constants (0x441ba0..0x441bb4) (provisional names)
const float kFogDepth = 500.0f;             // 0x441ba0
const float kMinAlphaRef = 0.03f;           // 0x441ba4
const float kZWriteOpacity = 0.6f;          // 0x441ba8
const float kTranslucentAmbientScale = 0.59f;  // 0x441bac
const float kBarFraction = 0.75f;           // 0x441bb0
const float kSelectionGrow = 0.3f;          // 0x441bb4
// after the vtable (0x441bd0..0x441be4)
// 0x441bd0..0x441bd8: DrawBars's bar literals 0.0013020834f (1 / 768), 24.0f and 1 / 1024 (orig fmul [1/1024]; fmul [24]:
// 24.0f * (theWidth / 1024.0f) keeps the two multiplies, and max() reuses the product)
const float kLabelBottom = 0.7135417f;      // 0x441bdc (548 / 768: the main game form's bottom strip)
const float kLabelMargin = 0.18066406f;     // 0x441be0 (185 / 1024)
const float kRingStep = -0.44879895f;       // 0x441be4 (-2 pi / 14)

// FUNCTION: EXE 0x5144e8
// Creates the units' render object
EEROUnits::EEROUnits()
{
}

// SYNTHETIC: EXE 0x5144fa
// (the scalar deleting destructor, emitted with the vtable; slot EEROUnits+0x10)

// FUNCTION: EXE 0x514516
// Destroys the units' render object
EEROUnits::~EEROUnits()
{
}

// FUNCTION: EXE 0x514521
// Fills the render lists of units and objects for this frame
void EEROUnits::FrameStarting()
{
    this->FillRenderLists();
}

// FUNCTION: EXE 0x514526
// Draws the small selection ring under each selected aircraft
void EEROUnits::DrawSelectionRings()
{
    unsigned long theScreen = gScreenIndex;
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetCulling(kCullNone);
    Fn10F7C8();
    gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    for (unsigned long i = 0; i < EERenderLists::sInstance.mSelectedUnits.size(); i++) {
        EERenderUnit* theUnit = EERenderLists::sInstance.mSelectedUnits[i];
        if (!(theUnit->mSelectionSize > 0.0f) || (theUnit->mInfo->mDomains & kMovementAir) == 0)
            continue;
        sSelectionModels[0]->SetGraphicsPostModelTransform(theUnit->mTransformation);
        sSelectionModels[0]->SetGraphicsScale(theUnit->mSelectionSize);
        sSelectionModels[0]->PrepareTransforms();
        gRasterizer->Draw(sSelectionModels[0], theScreen, 1.0f, gOrthographicView);
    }
}

// FUNCTION: EXE 0x51461b
// Draws the area-effect ring of each unit with an area effect, its first segments (one per strength point)
// in the ring colour and the rest of the seven in the highlight colour, turned to the camera
void EEROUnits::DrawRings()
{
    GETransformation theTransformation;
    EERenderUnitMap::iterator theIterator;
    unsigned long theScreen = gScreenIndex;
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetCulling(kCullNone);
    Fn10F7C8();
    theTransformation.SetOrientationYPR(gRenderEnvironment.mCameraYaw, 0.0f, 0.0f);
    theIterator = EERenderLists::sInstance.mUnits0.begin();
    while (theIterator != EERenderLists::sInstance.mUnits0.end()) {
        EERenderUnit* theUnit = theIterator->second;
        long theVertex = 0;
        if (theUnit->mAreaEffectStrength > 0 && theUnit->mSelectionSize > 0.0f) {
            theTransformation.SetX(theUnit->mTransformation.GetX());
            theTransformation.SetY(theUnit->mTransformation.GetY());
            theTransformation.SetZ(theUnit->mTransformation.GetZ());
            unsigned long theSegment = 1;
            for (; theSegment <= theUnit->mAreaEffectStrength; theSegment++) {
                sRingModel->GetVertexArray(0)->SetColor(gRingColor, theVertex);
                theVertex++;
                sRingModel->GetVertexArray(0)->SetColor(gRingColor, theVertex);
                theVertex++;
                sRingModel->GetVertexArray(0)->SetColor(gRingColor, theVertex);
                theVertex++;
            }
            for (; theSegment < 8; theSegment++) {
                sRingModel->GetVertexArray(0)->SetColor(gRingHighlightColor, theVertex);
                theVertex++;
                sRingModel->GetVertexArray(0)->SetColor(gRingHighlightColor, theVertex);
                theVertex++;
                sRingModel->GetVertexArray(0)->SetColor(gRingHighlightColor, theVertex);
                theVertex++;
            }
            sRingModel->SetGraphicsPostModelTransform(theTransformation);
            sRingModel->SetGraphicsScale(theUnit->mSelectionSize);
            sRingModel->PrepareTransforms();
            gRasterizer->Draw(sRingModel, theScreen, 1.0f, gOrthographicView);
        }
        theIterator++;
    }
}

// FUNCTION: EXE 0x5147f8
// Draws one bar of a unit on screen above its bar tag (row inRow): the filled part in inColor and the rest
// in inBackColor, each as two shaded rhombi at least one pixel wide

void EEROUnits::DrawBar(EERenderUnit* inUnit, const GEColor& inColor, const GEColor& inBackColor, float inFraction,
                         long inRow)
{
    GERectangle theBar;
    GERectangle theBack;
    GE3DPoint theOffset;
    GE3DPoint theLocation;
    GE2DPoint theScreenPoint;
    unsigned long theScreen = gScreenIndex;
    theOffset.SetX(inUnit->mSource->mBar.GetX() * inUnit->mScale);
    theOffset.SetY(inUnit->mSource->mBar.GetY() * inUnit->mScale);
    theOffset.SetZ(inUnit->mSource->mBar.GetZ() * inUnit->mScale);
    inUnit->mTransformation.Apply(theOffset, theLocation);
    Fn118676(theLocation, theScreenPoint);
    theScreenPoint.SetX(theScreenPoint.GetX() - sBarWidth / 2);
    theScreenPoint.SetY(theScreenPoint.GetY() - inRow * sBarStep);
    theScreenPoint.SetX((float)UMFloor(theScreenPoint.GetX()));
    theScreenPoint.SetY((float)UMFloor(theScreenPoint.GetY()));
    if (inFraction >= 1.0f) {
        theBar.SetLeft(theScreenPoint.GetX());
        theBar.SetRight(sBarWidth * kBarFraction + theScreenPoint.GetX());
        theBar.SetTop(theScreenPoint.GetY());
        theBar.SetBottom(theScreenPoint.GetY() + sBarHeight);
        sFGBar->SetRectangle(theBar);
        sFGBar->SetColorPerVertex(inColor, gBarBorderColor, inColor, inColor);
        gRasterizer->Draw(sFGBar, theScreen);
        theBack.SetLeft(theBar.GetRight());
        theBack.SetRight(sBarWidth + theScreenPoint.GetX());
        theBack.SetBottom(theBar.GetBottom());
        theBack.SetTop(theBar.GetTop());
        sBGBar->SetRectangle(theBack);
        sBGBar->SetColorPerVertex(gBarBorderColor, inColor, inColor, inColor);
        gRasterizer->Draw(sBGBar, theScreen);
    } else if (inFraction != 0.0f) {
        theBar.SetLeft(theScreenPoint.GetX());
        theBar.SetRight(sBarWidth * kBarFraction * inFraction + theScreenPoint.GetX());
        theBar.SetTop(theScreenPoint.GetY());
        theBar.SetBottom(theScreenPoint.GetY() + sBarHeight);
        if (theBar.GetWidth() < 1.0f)
            theBar.SetRight(theBar.GetLeft() + 1.0f);
        sFGBar->SetRectangle(theBar);
        sFGBar->SetColorPerVertex(inColor, gBarBorderColor, inColor, inColor);
        theBack.SetLeft(theBar.GetRight());
        theBack.SetRight(sBarWidth * inFraction + theScreenPoint.GetX());
        theBack.SetBottom(theBar.GetBottom());
        theBack.SetTop(theBar.GetTop());
        if (theBack.GetWidth() < 1.0f)
            theBack.SetRight(theBack.GetLeft() + 1.0f);
        sBGBar->SetRectangle(theBack);
        sBGBar->SetColorPerVertex(gBarBorderColor, inColor, inColor, inColor);
        gRasterizer->Draw(sBGBar, theScreen);
        gRasterizer->Draw(sFGBar, theScreen);
        theBar.SetLeft(theBack.GetRight());
        theBar.SetRight((sBarWidth - sBarWidth * inFraction) * kBarFraction + theBar.GetLeft());
        theBar.SetBottom(theBack.GetBottom());
        theBar.SetTop(theBack.GetTop());
        if (theBar.GetWidth() < 1.0f)
            theBar.SetRight(theBar.GetLeft() + 1.0f);
        sFGBar->SetRectangle(theBar);
        sFGBar->SetColorPerVertex(inBackColor, gBarBorderColor, inBackColor, inBackColor);
        theBack.SetLeft(theBar.GetRight());
        theBack.SetRight(sBarWidth + theScreenPoint.GetX());
        theBack.SetBottom(theBar.GetBottom());
        theBack.SetTop(theBar.GetTop());
        sBGBar->SetRectangle(theBack);
        sBGBar->SetColorPerVertex(gBarBorderColor, inBackColor, inBackColor, inBackColor);
        gRasterizer->Draw(sBGBar, theScreen);
        gRasterizer->Draw(sFGBar, theScreen);
    } else {
        theBar.SetLeft(theScreenPoint.GetX());
        theBar.SetRight(sBarWidth * kBarFraction + theScreenPoint.GetX());
        theBar.SetTop(theScreenPoint.GetY());
        theBar.SetBottom(theScreenPoint.GetY() + sBarHeight);
        sFGBar->SetRectangle(theBar);
        sFGBar->SetColorPerVertex(inBackColor, gBarBorderColor, inBackColor, inBackColor);
        gRasterizer->Draw(sFGBar, theScreen);
        theBack.SetLeft(theBar.GetRight());
        theBack.SetRight(sBarWidth + theScreenPoint.GetX());
        theBack.SetBottom(theBar.GetBottom());
        theBack.SetTop(theBar.GetTop());
        sBGBar->SetRectangle(theBack);
        sBGBar->SetColorPerVertex(gBarBorderColor, inBackColor, inBackColor, inBackColor);
        gRasterizer->Draw(sBGBar, theScreen);
    }
}

// FUNCTION: EXE 0x514cbc
// Draws a building being placed: its model (fogged red where it cannot be placed), then for land buildings
// its footprint outline and the edges of its blocked cells as screen lines
void EEROUnits::DrawPlacement(EERenderUnitRecord& inRecord)
{
    GETextureCoordinate theStart;
    GETextureCoordinate theEnd;
    GE3DPoint thePoint;
    GE2DPoint theScreenPoint;
    GERFogAttributes theFogAttributes;
    unsigned long theScreen = gScreenIndex;
    long theSize = inRecord.mUnit.mGridSize;
    long theX = inRecord.mTileX;
    long theY = inRecord.mTileY;
    unsigned long theLineCount = 0;
    bool theRows[12][12];
    bool theColumns[12][12];
    memset(theRows, 0, sizeof(theRows));
    memset(theColumns, 0, sizeof(theColumns));
    inRecord.mUnit.mOpacity = 1.0f;
    if (inRecord.mUnit.mScale != 0.0f) {
        gRasterizer->SetShadingColorSource(0, gScreenIndex);
        Fn10FA01();
        if (inRecord.mUnit.mCanPlace) {
            this->DrawUnitModel(&inRecord.mUnit);
        } else {
            GE3DPoint theCenter;
            GE3DPoint theCameraPoint;
            unsigned long theFog = gRasterizer->GetFog(theScreen);
            gRasterizer->GetFogAttributes(theFogAttributes, theScreen);
            inRecord.mUnit.mTransformation.Apply(
                inRecord.mUnit.mSource->mModels[inRecord.mUnit.mAnimation]->GetCenterPoint(), theCenter);
            gU4E8F08.GetCameraInverse().Apply(theCenter, theCameraPoint);
            float theStartDistance = theCameraPoint.GetY() - kFogDepth * 0.6f;
            float theEndDistance = kFogDepth + theStartDistance;
            gRasterizer->SetFog(1, theScreen);
            gRasterizer->SetFogColor(GEColor(1.0f, 0.0f, 0.0f), theScreen);
            gRasterizer->SetFogRange(theStartDistance, theEndDistance, theScreen);
            this->DrawUnitModel(&inRecord.mUnit);
            gRasterizer->SetFog(theFog, theScreen);
            gRasterizer->SetFogType(theFogAttributes.mType, theScreen);
            gRasterizer->SetFogColor(theFogAttributes.mColor, theScreen);
            gRasterizer->SetFogRange(theFogAttributes.mStart, theFogAttributes.mEnd, theScreen);
        }
    }
    if (!inRecord.mUnit.mInfo->IsLandBuilding())
        return;
    Fn10F7C8();
    Fn10F6C7();
    gRasterizer->SetShadingColorSource(1, gScreenIndex);
    gU4E8F08.SetDepthRange(0.0f, 0.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetZCompare(kCompareLess, theScreen);
    float theHalf;
    if (theSize % 2 == 0)
        theHalf = (float)theSize * 0.5f;
    else
        theHalf = (float)(theSize - 1) * 0.5f;
    theStart.SetU(gU4E9330);
    float theLeft = (float)theX - theHalf;
    float theTop = (float)theY - theHalf;
    theEnd.SetU(gU4E9330 + 3.0f);
    long i;
    long j;
    for (i = 0; i <= theSize; i++) {
        for (j = 0; j <= theSize; j++) {
            float theLineX;
            float theLineY;
            if ((i == 0 || i == theSize) && j != theSize) {
                sLines[theLineCount].SetTextureCoordinates(theStart, theEnd);
                sLines[theLineCount].SetColor(gBarColor);
                thePoint.SetX(theLineX = j + theLeft);
                thePoint.SetY(theLineY = i + theTop);
                thePoint.SetZ(inRecord.mHeights[j][i]);
                Fn118676(thePoint, theScreenPoint);
                sLines[theLineCount].SetStartPoint(theScreenPoint);
                thePoint.SetX(theLineX + 1.0f);
                thePoint.SetY(theLineY);
                thePoint.SetZ(inRecord.mHeights[j + 1][i]);
                Fn118676(thePoint, theScreenPoint);
                sLines[theLineCount].SetEndPoint(theScreenPoint);
                if (++theLineCount == 0x40) {
                    gRasterizer->Draw(sLines, theLineCount, theScreen);
                    theLineCount = 0;
                }
            }
            if ((j != 0 && j != theSize) || i == theSize)
                continue;
            sLines[theLineCount].SetTextureCoordinates(theStart, theEnd);
            sLines[theLineCount].SetColor(gBarColor);
            thePoint.SetX(theLineX = j + theLeft);
            thePoint.SetY(theLineY = i + theTop);
            thePoint.SetZ(inRecord.mHeights[j][i]);
            Fn118676(thePoint, theScreenPoint);
            sLines[theLineCount].SetStartPoint(theScreenPoint);
            thePoint.SetX(theLineX);
            thePoint.SetY(theLineY + 1.0f);
            thePoint.SetZ(inRecord.mHeights[j][i + 1]);
            Fn118676(thePoint, theScreenPoint);
            sLines[theLineCount].SetEndPoint(theScreenPoint);
            if (++theLineCount == 0x40) {
                gRasterizer->Draw(sLines, theLineCount, theScreen);
                theLineCount = 0;
            }
        }
    }
    for (i = 0; i < theSize; i++) {
        for (j = 0; j < theSize; j++) {
            if (inRecord.mCells[j][i])
                continue;
            theRows[j][i] ^= 1;
            theRows[j][i + 1] ^= 1;
            theColumns[j][i] ^= 1;
            theColumns[j + 1][i] ^= 1;
        }
    }
    for (i = 0; i <= theSize; i++) {
        for (j = 0; j <= theSize; j++) {
            float theLineX;
            float theLineY;
            if (theRows[j][i] && j != theSize) {
                sLines[theLineCount].SetTextureCoordinates(theStart, theEnd);
                sLines[theLineCount].SetColor(gBarBackColor);
                thePoint.SetX(theLineX = j + theLeft);
                thePoint.SetY(theLineY = i + theTop);
                thePoint.SetZ(inRecord.mHeights[j][i]);
                Fn118676(thePoint, theScreenPoint);
                sLines[theLineCount].SetStartPoint(theScreenPoint);
                thePoint.SetX(theLineX + 1.0f);
                thePoint.SetY(theLineY);
                thePoint.SetZ(inRecord.mHeights[j + 1][i]);
                Fn118676(thePoint, theScreenPoint);
                sLines[theLineCount].SetEndPoint(theScreenPoint);
                if (++theLineCount == 0x40) {
                    gRasterizer->Draw(sLines, theLineCount, theScreen);
                    theLineCount = 0;
                }
            }
            if (!theColumns[j][i] || i == theSize)
                continue;
            sLines[theLineCount].SetTextureCoordinates(theStart, theEnd);
            sLines[theLineCount].SetColor(gBarBackColor);
            thePoint.SetX(theLineX = j + theLeft);
            thePoint.SetY(theLineY = i + theTop);
            thePoint.SetZ(inRecord.mHeights[j][i]);
            Fn118676(thePoint, theScreenPoint);
            sLines[theLineCount].SetStartPoint(theScreenPoint);
            thePoint.SetX(theLineX);
            thePoint.SetY(theLineY + 1.0f);
            thePoint.SetZ(inRecord.mHeights[j][i + 1]);
            Fn118676(thePoint, theScreenPoint);
            sLines[theLineCount].SetEndPoint(theScreenPoint);
            if (++theLineCount == 0x40) {
                gRasterizer->Draw(sLines, theLineCount, theScreen);
                theLineCount = 0;
            }
        }
    }
    if (theLineCount > 0)
        gRasterizer->Draw(sLines, theLineCount, theScreen);
    gU4E8F08.SetDepthRange(0.0f, 1.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetZCompare(kCompareLessEqual, theScreen);
}

// FUNCTION: EXE 0x515488
// Draws the selection ring (small, medium or large by size) under each selected ground unit, and with
// collision spheres on, each unit's inner collision sphere
void EEROUnits::DrawSelection()
{
    EERenderUnitMap::iterator theIterator;
    unsigned long theScreen = gScreenIndex;
    unsigned long i;
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetCulling(kCullNone);
    Fn10F7C8();
    gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    for (i = 0; i < EERenderLists::sInstance.mSelectedUnits.size(); i++) {
        EERenderUnit* theUnit = EERenderLists::sInstance.mSelectedUnits[i];
        if (!(theUnit->mSelectionSize > 0.0f) || (theUnit->mInfo->mDomains & kMovementAir) != 0)
            continue;
        long theSize;
        for (theSize = 0; theSize < 2; theSize++) {
            if (theUnit->mSelectionSize <= sSelectionSizes[theSize])
                break;
        }
        sSelectionModels[theSize]->SetGraphicsPostModelTransform(theUnit->mTransformation);
        float theScale = kSelectionGrow * theUnit->mSelectionSize;
        theScale += theUnit->mSelectionSize;
        sSelectionModels[theSize]->SetGraphicsScale(theScale);
        sSelectionModels[theSize]->PrepareTransforms();
        gRasterizer->Draw(sSelectionModels[theSize], theScreen, 1.0f, gOrthographicView);
    }
    if (!gShowCollisionSpheres)
        return;
    for (theIterator = EERenderLists::sInstance.mUnits0.begin();
         theIterator != EERenderLists::sInstance.mUnits0.end(); theIterator++) {
        EERenderUnit* theUnit = theIterator->second;
        float theSize;
        if (theUnit->mCategory == kUnitCategoryAmbient || theUnit->mCategory == kUnitCategoryResource || theUnit->mAnimation == 5 || theUnit->mAnimation == 4 ||
            !((theSize = theUnit->mInfo->mInnerCollisionSphere) > 0.0f))
            continue;
        sSelectionModels[0]->SetGraphicsPostModelTransform(theUnit->mTransformation);
        sSelectionModels[0]->SetGraphicsScale(theSize);
        sSelectionModels[0]->PrepareTransforms();
        gRasterizer->Draw(sSelectionModels[0], theScreen, 1.0f, gOrthographicView);
    }
}

// FUNCTION: EXE 0x515674
// Draws one map object's model at its frame, scale and location, with alpha test and z writes by its opacity
void EEROUnits::DrawObjectModel(GETransformation& ioTransformation, EERenderShadow* inObject)
{
    unsigned long theScreen = gScreenIndex;
    EEGraphicObject* theObject = inObject->mObject;
    GEModel* theModel = theObject->mModels[inObject->mModelIndex];
    GETexture* theTexture = theObject->mTextures[inObject->mModelIndex];
    if (!(inObject->mModelScale > 0.0f))
        return;
    if (theObject->mAlphaReference > inObject->mOpacity)
        gRasterizer->SetAlphaTestingRefValue(kMinAlphaRef, gScreenIndex);
    else
        gRasterizer->SetAlphaTestingRefValue(theObject->mAlphaReference, gScreenIndex);
    gRasterizer->SetZBuffer(inObject->mOpacity > kZWriteOpacity ? 3 : 2, theScreen);
    theModel->SetOpacity(inObject->mOpacity);
    theModel->SetTexture(theTexture, theScreen);
    theModel->SetGraphicsCurrentFrame(inObject->mCurrentFrame);
    theModel->SetGraphicsNextFrame(inObject->mNextFrame);
    theModel->SetFractionalFramePosition(inObject->mFraction);
    theModel->SetGraphicsScale(inObject->mModelScale);
    ioTransformation.SetPosition(inObject->mLocation.GetX(), inObject->mLocation.GetY(),
                                 inObject->mLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(ioTransformation);
    theModel->PrepareTransforms();
    gRasterizer->Draw(theModel, theScreen, inObject->mObject->mOpacity, gOrthographicView);
}

// FUNCTION: EXE 0x5157b6
// Draws a selected object's health bar above the object at its bar tag, restoring the blending
void EEROUnits::DrawObjectBars(EERenderShadow* inObject)
{
    GERectangle theBar;
    GERectangle theBack;
    GE3DPoint theOffset;
    GE2DPoint theScreenPoint;
    unsigned long theScreen = gScreenIndex;
    unsigned long theSource;
    unsigned long theDestination;
    gRasterizer->GetBlending(theSource, theDestination, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendZero, theScreen);
    sFGBar->SetColor(gBarColor);
    sBGBar->SetColor(gBarBackColor);
    // The bar tag is relative to the object: add its location before projecting (it was projected near the map
    // origin).
    theOffset.SetX(inObject->mLocation.GetX() + inObject->mObject->mBar.GetX() * inObject->mModelScale);
    theOffset.SetY(inObject->mLocation.GetY() + inObject->mObject->mBar.GetY() * inObject->mModelScale);
    theOffset.SetZ(inObject->mLocation.GetZ() + inObject->mObject->mBar.GetZ() * inObject->mModelScale);
    Fn118676(theOffset, theScreenPoint);
    theScreenPoint.SetX(theScreenPoint.GetX() - sBarWidth / 2);
    theScreenPoint.SetY(theScreenPoint.GetY() - sBarStep);
    if (inObject->mHealth >= 1.0f) {
        theBar.SetLeft(theScreenPoint.GetX());
        theBar.SetRight(sBarWidth + theScreenPoint.GetX());
        theBar.SetTop(theScreenPoint.GetY());
        theBar.SetBottom(theScreenPoint.GetY() + sBarHeight);
        sFGBar->SetRectangle(theBar);
        gRasterizer->Draw(sFGBar, theScreen);
    } else if (inObject->mHealth != 0.0f) {
        theBar.SetLeft(theScreenPoint.GetX());
        theBar.SetRight(sBarWidth * inObject->mHealth + theScreenPoint.GetX());
        theBar.SetTop(theScreenPoint.GetY());
        theBar.SetBottom(theScreenPoint.GetY() + sBarHeight);
        sFGBar->SetRectangle(theBar);
        gRasterizer->Draw(sFGBar, theScreen);
        float theLeft = theBar.GetLeft();
        theBack.SetLeft(theBar.GetRight());
        theBack.SetRight(sBarWidth + theLeft);  // the back part runs to the bar's full width (it was shortened
                                                // by the filled width a second time)
        theBack.SetBottom(theBar.GetBottom());
        theBack.SetTop(theBar.GetTop());
        sBGBar->SetRectangle(theBack);
        gRasterizer->Draw(sBGBar, theScreen);
    } else {
        theBack.SetLeft(theScreenPoint.GetX());
        theBack.SetRight(sBarWidth + theScreenPoint.GetX());
        theBack.SetTop(theScreenPoint.GetY());
        theBack.SetBottom(theScreenPoint.GetY() + sBarHeight);
        sBGBar->SetRectangle(theBack);
        gRasterizer->Draw(sBGBar, theScreen);
    }
    gRasterizer->SetBlending(theSource, theDestination, theScreen);
}

// FUNCTION: EXE 0x515a50
// Draws the map objects' models facing the camera, lit as fogged or not, translucent ones additively
// when the card cannot modulate texture alpha
void EEROUnits::DrawObjectModels(EERenderShadowMap& inObjects)
{
    GETransformation theTransformation;
    EERenderShadowMap::iterator theIterator;
    unsigned long theScreen = gScreenIndex;
    bool theCanModulate = gRasterizer->GetCanModulateTextureAlpha(theScreen);
    Fn10F8DE();
    theTransformation.SetOrientationYPR(gRenderEnvironment.mCameraYaw - 0.7853982f, 0.0f, 0.0f);
    for (theIterator = inObjects.begin(); theIterator != inObjects.end(); theIterator++) {
        EERenderShadow* theObject = theIterator->second;
        if (!theObject->mIsFogged)
            Fn10FAE8();
        else
            Fn10FB74();
        if (theCanModulate) {
            gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
        } else if (theObject->mOpacity < 1.0f) {
            gRasterizer->SetAmbientIntensity(kTranslucentAmbientScale * theObject->mOpacity, theScreen);
            gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
        }
        this->DrawObjectModel(theTransformation, theObject);
    }
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
}

// FUNCTION: EXE 0x515b63
// Draws the selection ring at each selected object (an object without a ring size redraws the previous
// ring transform)
void EEROUnits::DrawObjectSelection()
{
    GETransformation theTransformation;
    unsigned long theScreen = gScreenIndex;
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetCulling(kCullNone);
    Fn10F7C8();
    for (unsigned long i = 0; i < EERenderLists::sInstance.mSelectedObjects.size(); i++) {
        EERenderShadow* theObject = EERenderLists::sInstance.mSelectedObjects[i];
        theTransformation.SetPosition(theObject->mLocation.GetX(), theObject->mLocation.GetY(),
                                      theObject->mLocation.GetZ());
        if (theObject->mSelectionSize > 0.0f) {
            sSelectionModels[0]->SetGraphicsPostModelTransform(theTransformation);
            sSelectionModels[0]->SetGraphicsScale(theObject->mSelectionSize);
            sSelectionModels[0]->PrepareTransforms();
        }
        gRasterizer->Draw(sSelectionModels[0], theScreen, 1.0f, gOrthographicView);
    }
}

// FUNCTION: EXE 0x515c3a
// Draws a unit's health bar (when it has health) and one bar per extra fraction above it; returns the bars'
// height in pixels (5 per bar)
long EEROUnits::DrawUnitBars(EERenderUnit* inUnit)
{
    unsigned long i = 0;
    unsigned long theRow = 0;
    if (inUnit->mHealth >= 0.0f) {
        theRow = 1;
        this->DrawBar(inUnit, gHealthColor, gHealthBackColor, inUnit->mHealth, 0);
    }
    for (; i < inUnit->mHealthBars.size(); i++) {
        this->DrawBar(inUnit, gBarColor, gBarBackColor, inUnit->mHealthBars[i], theRow);
        theRow++;
    }
    return theRow * 5.0f;
}

// FUNCTION: EXE 0x515cc7
// Draws a unit's model: frame textures, player colour, turret/orbital/wheel rotations, frames and opacity
// (model detail), then its second graphic object's model (a robotic farm's at full opacity when gone or at 1 hp)
void EEROUnits::DrawUnitModel(EERenderUnit* inUnit)
{
    GETransformation theRotation;
    GETransformation theTransformation;
    GEModel* theModel = inUnit->mSource->mModels[inUnit->mAnimation];
    GETexture* theTexture = inUnit->mSource->mTextures[inUnit->mAnimation];
    unsigned long theScreen = gScreenIndex;
    unsigned long theYawCount = inUnit->mTurretAngles.size();
    unsigned long theRollCount = inUnit->mOrbitalAngles.size();
    unsigned long thePitchCount = inUnit->mWheelAngles.size();
    unsigned long i;
    if (!inUnit->mSource->mIsDrawn || !(inUnit->mScale > 0.0f))
        return;
    if (inUnit->mSource->mAlphaReference <= inUnit->mOpacity)
        gRasterizer->SetAlphaTestingRefValue(inUnit->mSource->mAlphaReference, gScreenIndex);
    else
        gRasterizer->SetAlphaTestingRefValue(kMinAlphaRef, gScreenIndex);
    gRasterizer->SetZBuffer(inUnit->mOpacity <= kZWriteOpacity ? 2 : 3, theScreen);
    inUnit->mSource->SetGroupColor(inUnit->mPlayerColor, inUnit->mAnimation);
    theModel->SetOpacity(inUnit->mOpacity);
    theModel->SetTexture(theTexture, theScreen);
    for (unsigned long theMaterial = 1; theMaterial < theModel->GetMaterialCount(); theMaterial++) {
        if (theTexture == NULL)
            continue;
        GEMaterial* theFirst = theModel->GetMaterial(theMaterial);
        theFirst->SetTexture(theFirst->GetTextureIndex() < theTexture->GetFrameCount() ? theTexture->GetFrameTexture(theFirst->GetTextureIndex()) : theTexture->GetFrameTexture(0), theScreen);   // past the frame count, frame 0 (0x115e53 jae)
        for (unsigned long theSub = 0; theSub < theModel->GetSubModelCount(); theSub++) {
            GEModel* theSubModel = theModel->GetSubModel(theSub);
            for (unsigned long j = 0; j < theSubModel->GetMaterialCount(); j++) {
                GEMaterial* theOther = theSubModel->GetMaterial(j);
                if (theOther->GetTextureIndex() < theTexture->GetFrameCount())
                    theOther->SetTexture(theTexture->GetFrameTexture(theOther->GetTextureIndex()), theScreen);
            }
        }
    }
    for (i = 0; i < theYawCount; i++) {
        theRotation.SetOrientationYPR(inUnit->mTurretAngles[i], 0.0f, 0.0f);
        inUnit->mTurretModels[i]->SetGraphicsPreModelTransform(theRotation);
    }
    for (i = 0; i < theRollCount; i++) {
        theRotation.SetOrientationYPR(inUnit->mOrbitalAngles[i], 0.0f, 0.0f);
        inUnit->mOrbitalModels[i]->SetGraphicsPreModelTransform(theRotation);
    }
    for (i = 0; i < thePitchCount; i++) {
        theRotation.SetOrientationYPR(0.0f, inUnit->mWheelAngles[i], 0.0f);
        inUnit->mWheelModels[i]->SetGraphicsPreModelTransform(theRotation);
    }
    if (inUnit->mHasSubModelOffset) {
        GEModel* theSubModel = theModel->GetSubModel(0);
        theTransformation = theSubModel->GetGraphicsPostModelTransform();
        theTransformation.SetPosition(theSubModel->GetInternalTransform(0).GetX() * inUnit->mSubModelOffsetScale * -1.0f,
                                      theSubModel->GetInternalTransform(0).GetY() * inUnit->mSubModelOffsetScale * -1.0f,
                                      theSubModel->GetInternalTransform(0).GetZ() * inUnit->mSubModelOffsetScale * -1.0f);
        theSubModel->SetGraphicsPostModelTransform(theTransformation);
    }
    theModel->SetGraphicsCurrentFrame(inUnit->mCurrentFrame);
    theModel->SetGraphicsNextFrame(inUnit->mNextFrame);
    theModel->SetGraphicsScale(inUnit->mScale);
    theModel->SetGraphicsPostModelTransform(inUnit->mTransformation);
    theModel->SetFractionalFramePosition(gInterpolateFrames ? inUnit->mFraction : 0.0f);
    theModel->PrepareTransforms();
    if (inUnit->mUnitClassIndex == kUnitTypeWaypoint)
        theModel->GetMaterial(0)->SetColor(inUnit->mTintColor);
    if (gModelDetail == 0)
        gRasterizer->Draw(theModel, theScreen, inUnit->mSource->mOpacity, gOrthographicView);
    else if (gModelDetail == 1)
        gRasterizer->Draw(theModel, theScreen, inUnit->mSource->mOpacity * 0.5f, gOrthographicView);
    else
        gRasterizer->Draw(theModel, theScreen, 1.0f, gOrthographicView);
    if (inUnit->mSource2 == NULL || !inUnit->mSource2->mIsDrawn)
        return;
    theModel = inUnit->mSource2->mModel;
    if (theModel == NULL)
        return;
    theModel->GetMaterial(0)->SetTexture(inUnit->mSource2->mTexture, theScreen);
    inUnit->mSource2->SetGroupColor(inUnit->mPlayerColor, 0);
    theTransformation.SetPosition(inUnit->mTransformation.GetX(), inUnit->mTransformation.GetY(),
                                  inUnit->mTransformation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(inUnit->mScale);
    theModel->PrepareTransforms();
    EEUnit* theUnit;
    if (inUnit->mUnitClassIndex == kUnitTypeRoboticFarm &&
        ((theUnit = static_cast<EEUnit*>(gWorld.GetUnit(inUnit->mUnitId))) == NULL || theUnit->mHitPoints == 1))
        theModel->SetOpacity(1.0f);
    else
        theModel->SetOpacity(inUnit->mOpacity);
    gRasterizer->Draw(theModel, theScreen, inUnit->mSource->mOpacity, gOrthographicView);
}

// FUNCTION: EXE 0x516299
// Draws the units' models, tinted ones inside a fog band of their tint colour, translucent ones additively
// when the card cannot modulate texture alpha
void EEROUnits::DrawUnitModels(EERenderUnitMap& inUnits)
{
    EERenderUnitMap::iterator theIterator;
    unsigned long theScreen = gScreenIndex;
    unsigned long theFog;
    GERFogAttributes theFogAttributes;
    bool theCanModulate = gRasterizer->GetCanModulateTextureAlpha(theScreen);
    Fn10FA01();
    gRasterizer->SetCulling(kCullCW);
    gRasterizer->GetLighting(theScreen);
    for (theIterator = inUnits.begin(); theIterator != inUnits.end(); theIterator++) {
        EERenderUnit* theUnit = theIterator->second;
        if (theUnit->mIsTinted) {
            GE3DPoint theCenter;
            GE3DPoint theCameraPoint;
            theFog = gRasterizer->GetFog(theScreen);
            gRasterizer->GetFogAttributes(theFogAttributes, theScreen);
            theUnit->mTransformation.Apply(theUnit->mSource->mModels[theUnit->mAnimation]->GetCenterPoint(), theCenter);
            gU4E8F08.GetCameraInverse().Apply(theCenter, theCameraPoint);
            float theStartDistance = theCameraPoint.GetY() - kFogDepth * theUnit->mFogScale;
            float theEndDistance = kFogDepth + theStartDistance;
            gRasterizer->SetFog(1, theScreen);
            gRasterizer->SetFogColor(theUnit->mTintColor, theScreen);
            gRasterizer->SetFogRange(theStartDistance, theEndDistance, theScreen);
        }
        if (!theUnit->mIsFogged)
            Fn10FAE8();
        else
            Fn10FB74();
        if (theCanModulate) {
            gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
        } else if (theUnit->mOpacity == 1.0f) {
            gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
        } else {
            gRasterizer->SetAmbientIntensity(kTranslucentAmbientScale * theUnit->mOpacity, theScreen);
            gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
        }
        this->DrawUnitModel(theUnit);
        if (!theUnit->mIsTinted)
            continue;
        gRasterizer->SetFog(theFog, theScreen);
        gRasterizer->SetFogType(theFogAttributes.mType, theScreen);
        gRasterizer->SetFogColor(theFogAttributes.mColor, theScreen);
        gRasterizer->SetFogRange(theFogAttributes.mStart, theFogAttributes.mEnd, theScreen);
    }
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
}

// FUNCTION: EXE 0x516500
// Sizes the bars from the window and draws the bars of every selected unit and object and the pointed-at ones
void EEROUnits::DrawBars()
{
    unsigned long theWidth = gGameWindow->GetWidth();
    unsigned long theHeight = gGameWindow->GetHeight();
    unsigned long i;
    unsigned long theCount;
    gRasterizer->SetBlending(kBlendOne, kBlendZero, gScreenIndex);
    gRasterizer->SetCulling(kCullNone);
    Fn10F7C8();
    sBarWidth = max(24L, UMFloor(24.0f * (theWidth / 1024.0f)));
    float theBarScale = (float)theHeight * 0.0013020834f;
    sBarHeight = max(2.0f, theBarScale * 2.0f);
    sBarStep = max(5.0f, theBarScale * 5.0f);
    theCount = EERenderLists::sInstance.mSelectedUnits.size();
    for (i = 0; i < theCount; i++)
        this->DrawUnitBars(EERenderLists::sInstance.mSelectedUnits[i]);
    theCount = EERenderLists::sInstance.mSelectedObjects.size();
    for (i = 0; i < theCount; i++)
        this->DrawObjectBars(EERenderLists::sInstance.mSelectedObjects[i]);
    if (EERenderLists::sInstance.mSelectedUnit != NULL)
        this->DrawUnitBars(EERenderLists::sInstance.mSelectedUnit);
    if (EERenderLists::sInstance.mSelectedObject != NULL)
        this->DrawObjectBars(EERenderLists::sInstance.mSelectedObject);
}

// FUNCTION: EXE 0x516692
// Draws the units' text labels (a name starting with a blank, and the debug label above its unit), clipped
// to the view above and beside the main game form's bottom strip unless the camera is free
void EEROUnits::DrawLabels(EERenderUnit* inUnits, unsigned long inCount)
{
    GE2DPoint theScreenPoint;
    GE3DPoint theOffset;
    GERectangle theView;
    GERectangle theStrip;
    GEText theText(gU531994);
    GE3DPoint theLocation;
    UWideString theString;
    unsigned long theWidth = gGameWindow->GetWidth();
    unsigned long theHeight = gGameWindow->GetHeight();
    float theMargin = 0.0f;
    float theBottom = 0.0f;
    float theWindowWidth = (float)theWidth;
    theView = gU4E8F08.GetViewRectangle();
    theMargin = theWindowWidth;
    theMargin *= kLabelMargin;
    theBottom = (float)theHeight * kLabelBottom;
    if (!gFreeCamera) {
        theStrip.SetLeft(theView.GetLeft());
        theStrip.SetTop(theBottom);
        theStrip.SetBottom(theView.GetBottom());
        if (gUnitPanelActive)
            theStrip.SetLeft(theMargin);
        theStrip.SetRight(theWindowWidth - theMargin);
        theView.SetBottom(theBottom);
    }
    theText.SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    theText.SetIsTransparent(true);
    theText.SetAlignment(kTextAlignCenter);
    if (theWidth >= 0x640)
        theText.SetFont(gFontTahoma16);
    else if (theWidth >= 0x500)
        theText.SetFont(gFontTahoma12);
    for (unsigned long i = 0; i < inCount; i++) {
        EERenderUnit* theUnit = &inUnits[i];
        if (theUnit->mCommandType != 0) {
            theString = theUnit->mName;
            if (theString != gU4E5B98 && theString[0] == ' ') {
                theText.SetText(theString);
                theOffset.SetX(theUnit->mTransformation.GetX());
                theOffset.SetY(theUnit->mTransformation.GetY());
                theOffset.SetZ(theUnit->mTransformation.GetZ());
                Fn118676(theOffset, theScreenPoint);
                theText.SetLocation(theScreenPoint);
            } else {
                continue;
            }
        } else if (gU529044 == theUnit) {
            sprintf(theString, "%s", (const char*)gU529030);
            theText.SetText(theString);
            theOffset.SetX(theUnit->mSource->mBar.GetX() * theUnit->mScale);
            theOffset.SetY(theUnit->mSource->mBar.GetY() * theUnit->mScale);
            theOffset.SetZ(theUnit->mSource->mBar.GetZ() * theUnit->mScale);
            theUnit->mTransformation.Apply(theOffset, theLocation);
            Fn118676(theLocation, theScreenPoint);
            theScreenPoint.SetY(theScreenPoint.GetY() - theUnit->mHealthBars.size() * 5.0f - 10.0f);
            theText.SetLocation(theScreenPoint);
            theText.SetFgColor(theUnit->mPlayerColor);
        } else {
            continue;
        }
        if (!gFreeCamera) {
            float theTextHeight = theText.GetTextExtent().GetHeight();
            if (theScreenPoint.GetY() + theTextHeight < theBottom) {
                gUIManager->mMainGameForm->DrawClippedText(theView, &theText, NULL);
            } else if (theScreenPoint.GetY() - theTextHeight > theBottom) {
                gUIManager->mMainGameForm->DrawClippedText(theStrip, &theText, NULL);
            } else {
                gUIManager->mMainGameForm->DrawClippedText(theView, &theText, NULL);
                gUIManager->mMainGameForm->DrawClippedText(theStrip, &theText, NULL);
            }
        } else {
            gUIManager->mScenarioEditorForm->DrawClippedText(theView, &theText, NULL);
        }
    }
}

// FUNCTION: EXE 0x516a97
// Loads the units' media: bar rhombi, command and dotted-line textures, the line buffer, the area-effect
// ring model and the three selection ring models (segment count by detail level)
void EEROUnits::LoadMedia()
{
    GETransformation theTransformation;
    GE3DPoint theStart;
    GETextureDescription theDescription = { 1, 0, 16, 0, false, false, true, true };
    GE3DPoint thePoint;
    GE3DPoint theInner;
    GE3DPoint theOuter;
    GETextureCoordinate theLineEnd(3.0f, 0.0f);
    GETextureCoordinate theLineStart;
    UString theName;
    unsigned long theSegments;
    float theAngle;
    unsigned long i;
    sBGBar = new GERhombus;
    sFGBar = new GERhombus;
    ESVERIFY_AT(sBGBar != NULL, 1823);
    ESVERIFY_AT(sFGBar != NULL, 1824);
    theDescription.mHasAlpha = false;
    theDescription.mCreateMIPMaps = false;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mPixelDepth = 0x10;
    theDescription.mSurfaceVolatility = 1;
    theName = "textures\\DottedLine";
    sDottedLineTexture = gTextureManager->LoadTexture(theName, theDescription);
    sDottedLineTexture->Reference();
    sDottedLineTexture->SetStageAlphaMode(kStageOpModulate, -1);
    sDottedLineTexture->SetStageColorMode(kStageOpModulate, -1);
    sDottedLineTexture->SetStageMinFilterMode(0, -1);
    sDottedLineTexture->SetStageMagFilterMode(0, -1);
    sDottedLineTexture->SetStageMipFilterMode(kMipFilterNone, -1);
    sDottedLineTexture->SetStageMipMapBias(0.0f, -1);
    sDottedLineTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mPixelDepth = 0x10;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    theDescription.mSurfaceVolatility = 1;
    theName = "textures\\UnitCommandConfirmation";
    sTexture4E0 = gTextureManager->LoadTexture(theName, theDescription);
    sTexture4E0->Reference();
    sTexture4E0->SetStageAlphaMode(kStageOpModulate, -1);
    sTexture4E0->SetStageColorMode(kStageOpModulate, -1);
    sTexture4E0->SetStageMinFilterMode(1, -1);
    sTexture4E0->SetStageMagFilterMode(1, -1);
    sTexture4E0->SetStageMipFilterMode(kMipFilterPoint, -1);
    sTexture4E0->SetStageMipMapBias(-1.0f, -1);
    sTexture4E0->SetStageTextureAddressingMode(kAddressingWrap, -1);
    theName = "textures\\PathCommandConfirmation";
    sTexture4E4 = gTextureManager->LoadTexture(theName, theDescription);
    sTexture4E4->Reference();
    sTexture4E4->SetStageAlphaMode(kStageOpModulate, -1);
    sTexture4E4->SetStageColorMode(kStageOpModulate, -1);
    sTexture4E4->SetStageMinFilterMode(1, -1);
    sTexture4E4->SetStageMagFilterMode(1, -1);
    sTexture4E4->SetStageMipFilterMode(kMipFilterPoint, -1);
    sTexture4E4->SetStageMipMapBias(-1.0f, -1);
    sTexture4E4->SetStageTextureAddressingMode(kAddressingWrap, -1);
    theName = "textures\\ClickCommandConfirmation";
    sTexture4E8 = gTextureManager->LoadTexture(theName, theDescription);
    sTexture4E8->Reference();
    sTexture4E8->SetStageAlphaMode(kStageOpModulate, -1);
    sTexture4E8->SetStageColorMode(kStageOpModulate, -1);
    sTexture4E8->SetStageMinFilterMode(1, -1);
    sTexture4E8->SetStageMagFilterMode(1, -1);
    sTexture4E8->SetStageMipFilterMode(kMipFilterPoint, -1);
    sTexture4E8->SetStageMipMapBias(-1.0f, -1);
    sTexture4E8->SetStageTextureAddressingMode(kAddressingWrap, -1);
    sLines = new GELine[0x40];
    for (i = 0; i < 0x40; i++) {
        sLines[i].SetColor(gBarColor);
        sLines[i].SetTexture(sDottedLineTexture, gScreenIndex);
        sLines[i].SetTextureCoordinates(theLineStart, theLineEnd);
    }
    theName = "textures\\UnitSelection";
    sSelectionTexture = gTextureManager->LoadTexture(theName, theDescription);
    sSelectionTexture->Reference();
    sSelectionTexture->SetStageAlphaMode(kStageOpModulate, -1);
    sSelectionTexture->SetStageColorMode(kStageOpModulate, -1);
    sSelectionTexture->SetStageMinFilterMode(1, -1);
    sSelectionTexture->SetStageMagFilterMode(1, -1);
    sSelectionTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    sSelectionTexture->SetStageMipMapBias(-1.0f, -1);
    sSelectionTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
    theName = "textures\\UnitAreaEffectCircle";
    sAreaEffectTexture = gTextureManager->LoadTexture(theName, theDescription);
    sAreaEffectTexture->Reference();
    sAreaEffectTexture->SetStageAlphaMode(kStageOpModulate, -1);
    sAreaEffectTexture->SetStageColorMode(kStageOpModulate, -1);
    sAreaEffectTexture->SetStageMinFilterMode(1, -1);
    sAreaEffectTexture->SetStageMagFilterMode(1, -1);
    sAreaEffectTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    sAreaEffectTexture->SetStageMipMapBias(-1.0f, -1);
    sAreaEffectTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
    sRingModel = gModelManager->CreateModel(0xa, 0x1a, 1, 0, 1, 0, "area effect", 1);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 0);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 3);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 6);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 9);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 0xc);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 0xf);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 0x12);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 0x19);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 0x16);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 0x17);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.5f, 0.0f, 0.0f), 0x13);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.5f, 0.0f, 0.0f), 0x18);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(-0.5f, 0.0f, 0.0f), 0x11);
    sRingModel->GetVertexArray(0)->SetPoint(GE3DPoint(-0.5f, 0.0f, 0.0f), 0x15);
    theStart.SetX(0.5f);
    theStart.SetY(0.0f);
    theStart.SetZ(0.0f);
    theTransformation.Revolve(kRingStep, 0.0f, 0.0f);
    theTransformation.Apply(theStart, thePoint);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 0xd);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 0x14);
    theTransformation.Revolve(kRingStep, 0.0f, 0.0f);
    theTransformation.Apply(theStart, thePoint);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 7);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 0xe);
    theTransformation.Revolve(kRingStep, 0.0f, 0.0f);
    theTransformation.Apply(theStart, thePoint);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 1);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 8);
    theTransformation.Revolve(kRingStep, 0.0f, 0.0f);
    theTransformation.Apply(theStart, thePoint);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 2);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 4);
    theTransformation.Revolve(kRingStep, 0.0f, 0.0f);
    theTransformation.Apply(theStart, thePoint);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 5);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 0xa);
    theTransformation.Revolve(kRingStep, 0.0f, 0.0f);
    theTransformation.Apply(theStart, thePoint);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 0xb);
    sRingModel->GetVertexArray(0)->SetPoint(thePoint, 0x10);
    for (i = 0; i < 0x1a; i++) {
        sRingModel->GetVertexArray(0)->GetPoint(theStart, i);
        sRingModel->GetVertexArray(0)->SetTextureCoordinate(
            GETextureCoordinate(theStart.GetX() + 0.5f, -(theStart.GetY() - 0.5f)), i);
    }
    sRingModel->GetPolygonArray(0)->SetVertexIndex1(0, 0);
    sRingModel->GetPolygonArray(0)->SetVertexIndex2(0, 1);
    sRingModel->GetPolygonArray(0)->SetVertexIndex3(0, 2);
    sRingModel->GetPolygonArray(0)->SetVertexIndex1(1, 3);
    sRingModel->GetPolygonArray(0)->SetVertexIndex2(1, 4);
    sRingModel->GetPolygonArray(0)->SetVertexIndex3(1, 5);
    sRingModel->GetPolygonArray(0)->SetVertexIndex1(2, 6);
    sRingModel->GetPolygonArray(0)->SetVertexIndex2(2, 7);
    sRingModel->GetPolygonArray(0)->SetVertexIndex3(2, 8);
    sRingModel->GetPolygonArray(0)->SetVertexIndex1(3, 9);
    sRingModel->GetPolygonArray(0)->SetVertexIndex2(3, 0xa);
    sRingModel->GetPolygonArray(0)->SetVertexIndex3(3, 0xb);
    sRingModel->GetPolygonArray(0)->SetVertexIndex1(4, 0xc);
    sRingModel->GetPolygonArray(0)->SetVertexIndex2(4, 0xd);
    sRingModel->GetPolygonArray(0)->SetVertexIndex3(4, 0xe);
    sRingModel->GetPolygonArray(0)->SetVertexIndex1(5, 0xf);
    sRingModel->GetPolygonArray(0)->SetVertexIndex2(5, 0x10);
    sRingModel->GetPolygonArray(0)->SetVertexIndex3(5, 0x11);
    sRingModel->GetPolygonArray(0)->SetVertexIndex1(6, 0x12);
    sRingModel->GetPolygonArray(0)->SetVertexIndex2(6, 0x13);
    sRingModel->GetPolygonArray(0)->SetVertexIndex3(6, 0x14);
    sRingModel->GetPolygonArray(0)->SetVertexIndex1(7, 0x15);
    sRingModel->GetPolygonArray(0)->SetVertexIndex2(7, 0x16);
    sRingModel->GetPolygonArray(0)->SetVertexIndex3(7, 0x19);
    sRingModel->GetPolygonArray(0)->SetVertexIndex1(8, 0x19);
    sRingModel->GetPolygonArray(0)->SetVertexIndex2(8, 0x16);
    sRingModel->GetPolygonArray(0)->SetVertexIndex3(8, 0x17);
    sRingModel->GetPolygonArray(0)->SetVertexIndex1(9, 0x19);
    sRingModel->GetPolygonArray(0)->SetVertexIndex2(9, 0x17);
    sRingModel->GetPolygonArray(0)->SetVertexIndex3(9, 0x18);
    for (i = 0; i < 0x1a; i++) {
        sRingModel->GetVertexArray(0)->SetColor(gRingHighlightColor, i);
        sRingModel->GetVertexArray(0)->SetOpacity(1.0f, i);
    }
    sRingModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    sRingModel->GetMaterial(0)->SetPolygonRangeLength(0xa, 0);
    sRingModel->GetMaterial(0)->SetVertexStartingIndex(0);
    sRingModel->GetMaterial(0)->SetVertexRangeLength(0x1a);
    sRingModel->GetMaterial(0)->SetTexture(sAreaEffectTexture, gScreenIndex);
    sRingModel->ComputeModelNormals();
    sRingModel->ComputeBoundingVolumes();
    if (gModelDetail == 0)
        theSegments = 6;
    else
        theSegments = gModelDetail == 1 ? 8 : 10;
    theAngle = 6.2831855f / (float)theSegments;
    for (unsigned long k = 0; k < 3; k++) {
        sSelectionModels[k] = gModelManager->CreateModel(theSegments * 2, theSegments * 2, 1, 0, 1, 0,
                                                         "unit selection", 1);
        theInner = GE3DPoint(0.0f, 0.5f, 0.0f);
        theOuter = GE3DPoint(0.0f, 0.5f - sSelectionWidths[k], 0.0f);
        sSelectionModels[k]->GetVertexArray(0)->SetPoint(theInner, 0);
        sSelectionModels[k]->GetVertexArray(0)->SetPoint(theOuter, 1);
        sSelectionModels[k]->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.5f, 0.0f), 0);
        sSelectionModels[k]->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.5f, 1.0f), 1);
        theTransformation.MakeIdentity();
        unsigned long theVertex = 2;
        for (i = 1; i < theSegments; i++) {
            theTransformation.Revolve(theAngle, 0.0f, 0.0f);
            theTransformation.Apply(theInner, thePoint);
            sSelectionModels[k]->GetVertexArray(0)->SetPoint(thePoint, theVertex);
            sSelectionModels[k]->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.5f, 0.0f), theVertex);
            theVertex++;
            theTransformation.Apply(theOuter, thePoint);
            sSelectionModels[k]->GetVertexArray(0)->SetPoint(thePoint, theVertex);
            sSelectionModels[k]->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.5f, 1.0f), theVertex);
            theVertex++;
        }
        unsigned long thePolygon = 0;
        unsigned long theFirst = 0;
        unsigned long theNext = 2;
        unsigned long theLast = theSegments * 2 - 2;
        for (; thePolygon < theLast; theFirst += 2, theNext += 2) {
            sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex1(thePolygon, theFirst);
            sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex2(thePolygon, theNext);
            sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex3(thePolygon, theNext - 1);
            thePolygon++;
            sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex1(thePolygon, theNext - 1);
            sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex2(thePolygon, theNext);
            sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex3(thePolygon, theNext + 1);
            thePolygon++;
        }
        sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex1(thePolygon, theFirst);
        sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex2(thePolygon, 0);
        sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex3(thePolygon, theFirst + 1);
        thePolygon++;
        sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex1(thePolygon, theFirst + 1);
        sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex2(thePolygon, 0);
        sSelectionModels[k]->GetPolygonArray(0)->SetVertexIndex3(thePolygon, 1);
        if (gModelDetail == 0) {
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 0);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 1);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 2);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 3);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 4);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 5);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 6);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 7);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 8);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 9);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 0xa);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 0xb);
            for (i = 0; i < 0xc; i++)
                sSelectionModels[k]->GetVertexArray(0)->SetOpacity(1.0f, i);
        } else if (gModelDetail == 1) {
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 0);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 1);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 2);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 3);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 4);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 5);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 6);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 7);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 8);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 9);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 0xa);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 0xb);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 0xc);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 0xd);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 0xe);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 0xf);
            for (i = 0; i < 0x10; i++)
                sSelectionModels[k]->GetVertexArray(0)->SetOpacity(1.0f, i);
        } else {
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 0);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 1);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 2);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 3);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 4);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 5);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 6);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 7);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 8);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 9);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 0xa);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU5294A0, 0xb);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 0xc);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 0xd);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 0xe);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 0xf);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 0x10);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gSelectionRingDark, 0x11);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 0x12);
            sSelectionModels[k]->GetVertexArray(0)->SetColor(gU529490, 0x13);
            for (i = 0; i < 0x14; i++)
                sSelectionModels[k]->GetVertexArray(0)->SetOpacity(1.0f, i);
        }
        sSelectionModels[k]->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
        sSelectionModels[k]->GetMaterial(0)->SetPolygonRangeLength(theSegments * 2, 0);
        sSelectionModels[k]->GetMaterial(0)->SetVertexStartingIndex(0);
        sSelectionModels[k]->GetMaterial(0)->SetVertexRangeLength(theSegments * 2);
        sSelectionModels[k]->GetMaterial(0)->SetTexture(sSelectionTexture, gScreenIndex);
        sSelectionModels[k]->ComputeModelNormals();
        sSelectionModels[k]->ComputeBoundingVolumes();
    }
}

// SYNTHETIC: EXE 0x5181a7
// (GELine's vector deleting destructor, emitted for ReleaseMedia's delete[] sLines)

// FUNCTION: EXE 0x518203
// Draws the selections and rings (not in cinematics), the object and unit models, the buildings being
// placed, the air selection rings and bars, then the effects' pass 5

void EEROUnits::RenderScene()
{
    EERenderUnitRecord theRecord;
    EEEffectQueue::iterator theIterator;
    std::deque<EEEffect*>::iterator theEffect;
    float theAlphaRefValue = gRasterizer->GetAlphaTestingRefValue(gScreenIndex);
    long theCount = EERenderLists::sInstance.mGhostRecords.size();
    unsigned long theScreen = gScreenIndex;
    Fn10F60F();
    if (!gInCinematic) {
        gU4E8F08.SetDepthRange(0.0f, 0.0f);
        gRasterizer->SetViewport(gU4E8F08, theScreen);
        gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
        gRasterizer->SetZCompare(kCompareLess, theScreen);
        gRasterizer->SetShadingColorSource(1, gScreenIndex);
        this->DrawObjectSelection();
        this->DrawSelection();
        this->DrawRings();
        gU4E8F08.SetDepthRange(0.0f, 1.0f);
        gRasterizer->SetViewport(gU4E8F08, theScreen);
        gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
        gRasterizer->SetZCompare(kCompareLessEqual, theScreen);
    }
    gRasterizer->SetShadingColorSource(0, gScreenIndex);
    Fn10F60F();
    this->DrawObjectModels(EERenderLists::sInstance.mShadows0);
    this->DrawUnitModels(EERenderLists::sInstance.mUnits0);
    this->DrawUnitModels(EERenderLists::sInstance.mUnits1);
    this->DrawObjectModels(EERenderLists::sInstance.mShadows1);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    for (long i = 0; i < theCount; i++) {
        theRecord = EERenderLists::sInstance.mGhostRecords[i];
        this->DrawPlacement(theRecord);
    }
    if (!gInCinematic) {
        gRasterizer->SetShadingColorSource(1, gScreenIndex);
        this->DrawSelectionRings();
        gU4E8F08.SetDepthRange(0.0f, 0.0f);
        gRasterizer->SetViewport(gU4E8F08, theScreen);
        gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
        gRasterizer->SetZCompare(kCompareLess, theScreen);
        this->DrawBars();
        gU4E8F08.SetDepthRange(0.0f, 1.0f);
        gRasterizer->SetViewport(gU4E8F08, theScreen);
        gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
        gRasterizer->SetZCompare(kCompareLessEqual, theScreen);
    }
    gRasterizer->SetCulling(kCullNone);
    for (theIterator = gEffectQueues[3].begin(); theIterator != gEffectQueues[3].end(); theIterator++) {
        Fn102724(theIterator->first);
        for (theEffect = theIterator->second.begin(); theEffect != theIterator->second.end(); theEffect++)
            (*theEffect)->DrawPass5();
        Fn103F64();
        Fn102F20();
    }
    gRasterizer->SetAlphaTestingRefValue(theAlphaRefValue, gScreenIndex);
}

// FUNCTION: EXE 0x51852f
// Draws the unit labels (not in cinematics) and the effects' pass 3
void EEROUnits::FrameDone()
{
    EEEffectQueue::iterator theIterator;
    std::deque<EEEffect*>::iterator theEffect;
    if (!gInCinematic)
        this->DrawLabels(EERenderLists::sInstance.mRenderUnits, EERenderLists::sInstance.mRenderUnitCount);
    for (theIterator = gEffectQueues[1].begin(); theIterator != gEffectQueues[1].end(); theIterator++) {
        for (theEffect = theIterator->second.begin(); theEffect != theIterator->second.end(); theEffect++)
            (*theEffect)->DrawPass3();
    }
}

// FUNCTION: EXE 0x5185b5
// Releases the units' media: bar rhombi, line buffer, textures, ring and selection models
void EEROUnits::ReleaseMedia()
{
    delete sBGBar;
    delete sFGBar;
    if (sLines != NULL)
        delete[] sLines;
    sSelectionTexture->Dereference();
    sDottedLineTexture->Dereference();
    sAreaEffectTexture->Dereference();
    sTexture4E0->Dereference();
    sTexture4E0 = NULL;
    sTexture4E4->Dereference();
    sTexture4E4 = NULL;
    sTexture4E8->Dereference();
    sTexture4E8 = NULL;
    sRingModel->Dereference();
    for (long i = 0; i < 3; i++)
        sSelectionModels[i]->Dereference();
    if (sModel4EC != NULL)
        sModel4EC->Dereference();
}

// FUNCTION: EXE 0x518676
// Projects a world point to window coordinates in the game view
void Fn118676(const GE3DPoint& inPoint, GE2DPoint& outPoint)
{
    GE3DPoint theCameraPoint(0.0f, 0.0f, 0.0f);
    GE3DPoint theNormalPoint(0.0f, 0.0f, 0.0f);
    float theW;
    gU4E8F08.GetCameraInverse().Apply(inPoint, theCameraPoint);
    if (!gOrthographicView)
        gU4E8F08.NormalizePerspectiveCameraPoint(theCameraPoint, theNormalPoint, theW);
    else
        gU4E8F08.NormalizeOrthographicCameraPoint(theCameraPoint, theNormalPoint, theW);
    if (theW < 0.0f)
        theW = -theW;
    if (theW != 0.0f)
        theW = 1.0f / theW;
    else
        theW = 1.0f;
    outPoint.SetX(gU4E8F08.GetHalfWidth() * theNormalPoint.GetX() * theW +
                  (gU4E8F08.GetXCenter() - gU4E8F08.GetViewRectangle().GetLeft()));
    outPoint.SetY(gU4E8F08.GetYCenter() - gU4E8F08.GetViewRectangle().GetTop() -
                  theNormalPoint.GetY() * gU4E8F08.GetHalfWidth() * theW);
}
