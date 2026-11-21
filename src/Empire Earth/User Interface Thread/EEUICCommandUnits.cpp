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
#include "ee/EEARust.h"
#include "ee/EEUICCommandUnits.h"

extern unsigned long& gTexturePixelDepth;
extern GEViewport gU4E8F08;                 // 0x4e8f08 the game view viewport (provisional name)
extern float& gCameraZoom;                   // 0x5317f0 (the arrow scale less 0.5) (provisional name)

// Creates the main game view's right-drag command control, hidden, with its facing arrow: a textured quad model
// (CreateModel) and the "Unit Facing Arrow" texture, clamped and alpha-blended (LoadTexture; command_units_calls.rs)
EEUICCommandUnits::EEUICCommandUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex,
                                     EEAMouseClick* inListener)
    : EEUIRustControl(inId, GERectangle(gViewport.GetViewRectangle().GetTopLeft(), 2.0f, 2.0f), inForm, inIndex, 0),
      EeScreensCommandUnitsFields()
{
    this->Rust(EE_SCREENS_CU_DO_INIT);
    this->StoreDisplayMode(kDisplayModeHidden);
    ee_screens_cu_construct((uintptr_t)this, (uintptr_t)inListener);   // (command_units_calls.rs)
}

// Destroys the command control, its arrow model and texture
EEUICCommandUnits::~EEUICCommandUnits()
{
    ee_screens_cu_destroy((uintptr_t)this);
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
    // (the selection's count and its units' categories are hud_ctl_ffi.rs')
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
        gWorld.GetCurrentPlayer()->mSelectedGroup->GetBounds(theBounds);
        theBounds.GetCenter(theCenter);
        ioF[0] = theCenter.GetX();
        ioF[1] = theCenter.GetY();
        break;
    }
    case EE_SCREENS_CU_EXECUTE:
        this->mListener->Execute(ioF[0], ioF[1], (inB & EE_SCREENS_CU_EXECUTE_HIT) != 0, (unsigned long)inA,
                                 (inB & EE_SCREENS_CU_EXECUTE_FACING) != 0, ioF[2], (inB & EE_SCREENS_CU_EXECUTE_DRAGGED) != 0);
        break;
    }
    return 0;
}

// ---- the one-statement shims of ee_screens command_units_calls.rs (UI lane round 8: the constructor's arrow, the
// destructor): each one call, no logic
#include "ee/EEScreensShims.h"
#define CU(x) reinterpret_cast<EEUICCommandUnits*>(x)
static GETextureDescription sCUDescription;   // the arrow texture's description
extern "C" void ee_screenscpp_cu_set_listener(uintptr_t inCu, uintptr_t inListener)
{ CU(inCu)->mListener = reinterpret_cast<EEAMouseClick*>(inListener); }
extern "C" void ee_screenscpp_cu_listener_reference(uintptr_t inCu) { CU(inCu)->mListener->Reference(); }
extern "C" void ee_screenscpp_cu_always_dirty(uintptr_t inCu) { CU(inCu)->SetAlwaysDirty(true); }
extern "C" void ee_screenscpp_cu_model_new(uintptr_t inCu)
{ CU(inCu)->mModel = gModelManager->CreateModel(2, 4, 1, 0, 1, 0, "Unit Direction Model", 1); }
extern "C" void ee_screenscpp_cu_model_reference(uintptr_t inCu) { CU(inCu)->mModel->Reference(); }
extern "C" void ee_screenscpp_cu_point(uintptr_t inCu, uint32_t inIndex, float inX, float inY, float inZ)
{ CU(inCu)->mModel->GetVertexArray(0)->SetPoint(GE3DPoint(inX, inY, inZ), inIndex); }
extern "C" void ee_screenscpp_cu_normal(uintptr_t inCu, uint32_t inIndex)
{ CU(inCu)->mModel->GetVertexArray(0)->SetNormal(GEVector(0.0f, 0.0f, 1.0f), inIndex); }
extern "C" void ee_screenscpp_cu_coordinate(uintptr_t inCu, uint32_t inIndex, float inU, float inV)
{ CU(inCu)->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(inU, inV), inIndex); }
extern "C" void ee_screenscpp_cu_vertex1(uintptr_t inCu, uint32_t inPolygon, uint32_t inVertex)
{ CU(inCu)->mModel->GetPolygonArray(0)->SetVertexIndex1(inPolygon, inVertex); }
extern "C" void ee_screenscpp_cu_vertex2(uintptr_t inCu, uint32_t inPolygon, uint32_t inVertex)
{ CU(inCu)->mModel->GetPolygonArray(0)->SetVertexIndex2(inPolygon, inVertex); }
extern "C" void ee_screenscpp_cu_vertex3(uintptr_t inCu, uint32_t inPolygon, uint32_t inVertex)
{ CU(inCu)->mModel->GetPolygonArray(0)->SetVertexIndex3(inPolygon, inVertex); }
extern "C" void ee_screenscpp_cu_polygon_start(uintptr_t inCu)
{ CU(inCu)->mModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0); }
extern "C" void ee_screenscpp_cu_polygon_length(uintptr_t inCu)
{ CU(inCu)->mModel->GetMaterial(0)->SetPolygonRangeLength(2, 0); }
extern "C" void ee_screenscpp_cu_vertex_start(uintptr_t inCu)
{ CU(inCu)->mModel->GetMaterial(0)->SetVertexStartingIndex(0); }
extern "C" void ee_screenscpp_cu_vertex_length(uintptr_t inCu)
{ CU(inCu)->mModel->GetMaterial(0)->SetVertexRangeLength(4); }
extern "C" void ee_screenscpp_cu_bounds(uintptr_t inCu) { CU(inCu)->mModel->ComputeBoundingVolumes(); }
extern "C" void ee_screenscpp_cu_set_description()
{ sCUDescription = GETextureDescription{1, 0, gTexturePixelDepth, 0, false, false, true, true}; }
extern "C" uintptr_t ee_screenscpp_cu_description() { return (uintptr_t)&sCUDescription; }
extern "C" void ee_screenscpp_cu_set_texture(uintptr_t inCu, uintptr_t inTexture)
{ CU(inCu)->mTexture = reinterpret_cast<GETexture*>(inTexture); }
extern "C" void ee_screenscpp_texture_alpha_texture(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageAlphaMode(kStageOpTexture, -1); }
extern "C" void ee_screenscpp_texture_color_texture(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageColorMode(kStageOpTexture, -1); }
extern "C" void ee_screenscpp_cu_material_texture(uintptr_t inCu)
{ CU(inCu)->mModel->GetMaterial(0)->SetTexture(CU(inCu)->mTexture, gScreenIndex); }
extern "C" void ee_screenscpp_cu_listener_dereference(uintptr_t inCu) { CU(inCu)->mListener->Dereference(); }
extern "C" void ee_screenscpp_cu_model_dereference(uintptr_t inCu) { CU(inCu)->mModel->Dereference(); }
extern "C" void ee_screenscpp_cu_texture_dereference(uintptr_t inCu) { CU(inCu)->mTexture->Dereference(); }
