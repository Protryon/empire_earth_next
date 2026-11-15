// C:\EEX\Empire Earth\User Interface Thread\EEUICCommandUnits.cpp (file name inferred: no __FILE__; RTTI class
// EEUICCommandUnits). The main game form's right-drag command control: 0x257881..0x258464 (ctor, ??_G, dtor, then
// the members in alphabetical order; EEUICPing's ctor at 0x258464). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/NEMessage.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGraphics.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEMaterial.h"
#include "lle/GEPolygonArray.h"
#include "ee/EEAction.h"
#include "ee/EEAMouseClick.h"
#include "ee/EEUICCommandUnits.h"

extern unsigned long& gTexturePixelDepth;
extern GEViewport gU4E8F08;                 // 0x4e8f08 the game view viewport (provisional name)
extern float& gCameraZoom;                   // 0x5317f0 (the arrow scale less 0.5) (provisional name)

// Creates the main game view's right-drag command control, hidden, with its facing arrow: a textured quad model
// (CreateModel) and the "Unit Facing Arrow" texture, clamped and alpha-blended (LoadTexture)
EEUICCommandUnits::EEUICCommandUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex,
                                     EEAMouseClick* inListener)
    : EEUIRustControl(inId, GERectangle(gViewport.GetViewRectangle().GetTopLeft(), 2.0f, 2.0f), inForm, inIndex, 0),
      EeScreensCommandUnitsFields()
{
    static const float kPoints[4][3] = {{-0.25f, 0.5f, 0.0f}, {0.25f, 0.5f, 0.0f}, {0.25f, 0.0f, 0.0f}, {-0.25f, 0.0f, 0.0f}};
    static const float kCoordinates[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    this->Rust(EE_SCREENS_CU_DO_INIT);
    this->StoreDisplayMode(kDisplayModeHidden);
    this->mListener = inListener;
    this->mListener->Reference();
    this->SetAlwaysDirty(true);
    GEModel* theModel = gModelManager->CreateModel(2, 4, 1, 0, 1, 0, "Unit Direction Model", 1);
    this->mModel = theModel;
    theModel->Reference();
    for (unsigned long i = 0; i < 4; i++)
        theModel->GetVertexArray(0)->SetPoint(GE3DPoint(kPoints[i][0], kPoints[i][1], kPoints[i][2]), i);
    for (unsigned long i = 0; i < 4; i++)
        theModel->GetVertexArray(0)->SetNormal(GEVector(0.0f, 0.0f, 1.0f), i);
    for (unsigned long i = 0; i < 4; i++)
        theModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(kCoordinates[i][0], kCoordinates[i][1]), i);
    for (unsigned long i = 0; i < 2; i++) {
        theModel->GetPolygonArray(0)->SetVertexIndex1(i, 0);
        theModel->GetPolygonArray(0)->SetVertexIndex2(i, i + 1);
        theModel->GetPolygonArray(0)->SetVertexIndex3(i, i + 2);
    }
    theModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    theModel->GetMaterial(0)->SetPolygonRangeLength(2, 0);
    theModel->GetMaterial(0)->SetVertexStartingIndex(0);
    theModel->GetMaterial(0)->SetVertexRangeLength(4);
    theModel->ComputeBoundingVolumes();
    GETextureDescription theDescription;
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    this->mTexture = gTextureManager->LoadTexture(UString("textures\\Unit Facing Arrow"), theDescription);
    if (this->mTexture) {
        this->mTexture->Reference();
        this->mTexture->SetStageAlphaMode(kStageOpTexture, -1);
        this->mTexture->SetStageColorMode(kStageOpTexture, -1);
        this->mTexture->SetStageMinFilterMode(1, -1);
        this->mTexture->SetStageMagFilterMode(1, -1);
        this->mTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
        this->mTexture->SetStageMipMapBias(-1.0f, -1);
        this->mTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
    }
    this->mModel->GetMaterial(0)->SetTexture(this->mTexture, gScreenIndex);
}

// Destroys the command control, its arrow model and texture
EEUICCommandUnits::~EEUICCommandUnits()
{
    this->mListener->Dereference();
    this->mModel->Dereference();
    this->mTexture->Dereference();
}

// ComputeAngle, CopyData, DrawPolygons, the drag, the release and StartCommand: ee_screens command_units.rs
// (EE_SCREENS_CU_DO_*) over the arrow's, the view's, the selection's and the action's ops below
long EEUICCommandUnits::Rust(uint32_t inWhich, const GE2DPoint* inLocation)
{
    return (long)ee_screens_command_units(this, this->Host(), inWhich, inLocation ? inLocation->GetX() : 0.0f,
                                          inLocation ? inLocation->GetY() : 0.0f);
}

// NEAR: registers (the original keeps 0 in ebx for the bool tests and the unsigned conversion; the theFar.x copy is a mov)
int64_t EEUICCommandUnits::RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    EESelectedGroup* theGroup = inOp == EE_SCREENS_CU_GROUP_CENTER || inOp == EE_SCREENS_CU_GROUP_COUNT ||
        inOp == EE_SCREENS_CU_CATEGORY ? gWorld.GetCurrentPlayer()->mSelectedGroup : NULL;
    switch (inOp) {
    case EE_SCREENS_CU_DRAW_ARROW: {
        GEColor theWhite(1.0f, 1.0f, 1.0f);
        GEColor theAmbientColor = gRasterizer->GetAmbientColor();
        GETransformation theTransformation;
        GEViewport theViewport(gRasterizer->GetViewport(gScreenIndex));
        unsigned long theScreen = gScreenIndex;
        float theAmbientIntensity = gRasterizer->GetAmbientIntensity();
        unsigned long theLighting = gRasterizer->GetLighting(theScreen);
        unsigned long theZBuffer = gRasterizer->GetZBuffer(theScreen);
        unsigned long theZCompare = gRasterizer->GetZCompare(theScreen);
        gU4E8F08.SetDepthRange(0.0f, 0.0f);
        gRasterizer->SetViewport(gU4E8F08, theScreen);
        gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
        gRasterizer->SetZCompare(kCompareLess, theScreen);
        gRasterizer->SetAmbientColor(theWhite, theScreen);
        gRasterizer->SetAmbientIntensity(1.0f, theScreen);
        gRasterizer->SetLighting(0, theScreen);
        theTransformation.SetOrientationYPR(ioF[0], 0.0f, 0.0f);
        theTransformation.SetPosition(ioF[1], ioF[2], ioF[3]);
        this->mModel->SetGraphicsCurrentFrame(0);
        float theZoom = gCameraZoom;
        this->mModel->SetGraphicsScale(theZoom + 0.5f);
        this->mModel->SetGraphicsPostModelTransform(theTransformation);
        this->mModel->PrepareTransforms();
        gRasterizer->Draw(this->mModel, theScreen, 1.0f, gOrthographicView);
        gU4E8F08.SetDepthRange(0.0f, 1.0f);
        gRasterizer->SetViewport(theViewport, theScreen);
        gRasterizer->SetZBuffer(theZBuffer, theScreen);
        gRasterizer->SetZCompare(theZCompare, theScreen);
        gRasterizer->SetAmbientColor(theAmbientColor, theScreen);
        gRasterizer->SetAmbientIntensity(theAmbientIntensity, theScreen);
        gRasterizer->SetLighting(theLighting, theScreen);
        break;
    }
    case EE_SCREENS_CU_GROUND: {
        // the mouse's line from the front to the rear clipping plane, solved at the press's height
        GE3DPoint theNear;
        GE3DPoint theFar;
        GE3DPoint theNearWorld;
        GE3DPoint theFarWorld;
        GE3DPoint theTarget;
        GE3DLine theLine;
        float theT;
        float theScale = 1.0f / gU4E8F08.GetHalfWidth();
        theNear.SetX((ioF[0] - gU4E8F08.GetXCenter()) * theScale);
        theFar.SetX(theNear.GetX());
        theNear.SetY(-((ioF[1] - gU4E8F08.GetYCenter()) * theScale));
        theNear.SetZ(0.0f);
        theFar.SetY(theNear.GetY());
        theFar.SetZ(1.0f);
        if (!gOrthographicView) {
            gU4E8F08.DenormalizePerspectivePoint(theNear, gU4E8F08.GetFrontClippingDistance(), theNearWorld);
            gU4E8F08.DenormalizePerspectivePoint(theFar, gU4E8F08.GetRearClippingDistance(), theFarWorld);
        } else {
            gU4E8F08.DenormalizeOrthographicPoint(theNear, theNearWorld);
            gU4E8F08.DenormalizeOrthographicPoint(theFar, theFarWorld);
        }
        theLine.Define(theNearWorld, theFarWorld);
        theLine.SolveForZ(ioF[2], theT, theTarget);
        ioF[0] = theTarget.GetX();
        ioF[1] = theTarget.GetY();
        ioF[2] = theTarget.GetZ();
        break;
    }
    case EE_SCREENS_CU_POINT_AT: {
        EETile* theLocation;
        GE3DPoint thePoint(ioF[2], ioF[3], ioF[4]);
        bool theHit = EEPointUnitOrTile(GE2DPoint(ioF[0], ioF[1]), theLocation, thePoint, true);
        ioF[2] = thePoint.GetX();
        ioF[3] = thePoint.GetY();
        ioF[4] = thePoint.GetZ();
        return theHit;
    }
    case EE_SCREENS_CU_GROUP_CENTER: {
        GERectangle theBounds;
        GE2DPoint theCenter;
        theGroup->GetBounds(theBounds);
        theBounds.GetCenter(theCenter);
        ioF[0] = theCenter.GetX();
        ioF[1] = theCenter.GetY();
        break;
    }
    case EE_SCREENS_CU_EXECUTE:
        this->mListener->Execute(ioF[0], ioF[1], (inB & EE_SCREENS_CU_EXECUTE_HIT) != 0, (unsigned long)inA,
                                 (inB & EE_SCREENS_CU_EXECUTE_FACING) != 0, ioF[2], (inB & EE_SCREENS_CU_EXECUTE_DRAGGED) != 0);
        break;
    case EE_SCREENS_CU_GROUP_COUNT: return theGroup->GetNumUnits();
    case EE_SCREENS_CU_CATEGORY: return theGroup->GetUnit((unsigned long)inA)->GetUnitClass()->mCategory;
    }
    return 0;
}
