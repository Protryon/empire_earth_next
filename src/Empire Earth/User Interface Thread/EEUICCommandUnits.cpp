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

// The class's own ops (the arrow's draw, the ground point, the pick, the group's centre, the click: the old RustOp)
// are command_units_ops.rs' over the one-statement shims below (UI lane round 11)
#define CU(c) static_cast<EEUICCommandUnits*>((EEUIRustControl*)(c))
static GEColor sCUAmbientColor;         // the arrow's draw: the rasterizer's state put back after it
static GEViewport sCUViewport;
static GETransformation sCUTransformation;
static GE3DPoint sCUNear;               // the ground point: the mouse's line
static GE3DPoint sCUFar;
static GE3DPoint sCUNearWorld;
static GE3DPoint sCUFarWorld;
static GE3DLine sCULine;
static float sCUT;
static GE3DPoint sCUTarget;             // the ground point, the point at the mouse
static EETile* sCUTile;
static GERectangle sCUBounds;           // the selected group's bounds and centre
static GE2DPoint sCUCenter;

extern "C" uint32_t ee_screenscpp_cu_screen() { return (uint32_t)gScreenIndex; }
extern "C" void ee_screenscpp_cu_save_ambient_color() { sCUAmbientColor = gRasterizer->GetAmbientColor(); }
extern "C" void ee_screenscpp_cu_save_viewport() { sCUViewport = gRasterizer->GetViewport(gScreenIndex); }
extern "C" float ee_screenscpp_cu_ambient_intensity() { return gRasterizer->GetAmbientIntensity(); }
extern "C" uint64_t ee_screenscpp_cu_lighting(uint32_t inScreen)
{ return (uint64_t)gRasterizer->GetLighting(inScreen); }
extern "C" uint64_t ee_screenscpp_cu_zbuffer(uint32_t inScreen) { return (uint64_t)gRasterizer->GetZBuffer(inScreen); }
extern "C" uint64_t ee_screenscpp_cu_zcompare(uint32_t inScreen)
{ return (uint64_t)gRasterizer->GetZCompare(inScreen); }
extern "C" void ee_screenscpp_cu_depth_range(float inNear, float inFar) { gU4E8F08.SetDepthRange(inNear, inFar); }
extern "C" void ee_screenscpp_cu_set_viewport_view(uint32_t inScreen) { gRasterizer->SetViewport(gU4E8F08, inScreen); }
extern "C" void ee_screenscpp_cu_set_zbuffer_test_only(uint32_t inScreen)
{ gRasterizer->SetZBuffer(kZBufferTestOnly, inScreen); }
extern "C" void ee_screenscpp_cu_set_zcompare_less(uint32_t inScreen)
{ gRasterizer->SetZCompare(kCompareLess, inScreen); }
extern "C" void ee_screenscpp_cu_set_ambient_white(uint32_t inScreen)
{ gRasterizer->SetAmbientColor(GEColor(1.0f, 1.0f, 1.0f), inScreen); }
extern "C" void ee_screenscpp_cu_set_ambient_intensity(float inV, uint32_t inScreen)
{ gRasterizer->SetAmbientIntensity(inV, inScreen); }
extern "C" void ee_screenscpp_cu_set_lighting(uint64_t inV, uint32_t inScreen)
{ gRasterizer->SetLighting((unsigned long)inV, inScreen); }
extern "C" void ee_screenscpp_cu_set_zbuffer(uint64_t inV, uint32_t inScreen)
{ gRasterizer->SetZBuffer((unsigned long)inV, inScreen); }
extern "C" void ee_screenscpp_cu_set_zcompare(uint64_t inV, uint32_t inScreen)
{ gRasterizer->SetZCompare((unsigned long)inV, inScreen); }
extern "C" void ee_screenscpp_cu_restore_viewport(uint32_t inScreen)
{ gRasterizer->SetViewport(sCUViewport, inScreen); }
extern "C" void ee_screenscpp_cu_restore_ambient_color(uint32_t inScreen)
{ gRasterizer->SetAmbientColor(sCUAmbientColor, inScreen); }
extern "C" void ee_screenscpp_cu_transform_reset() { sCUTransformation = GETransformation(); }
extern "C" void ee_screenscpp_cu_transform_yaw(float inYaw) { sCUTransformation.SetOrientationYPR(inYaw, 0.0f, 0.0f); }
extern "C" void ee_screenscpp_cu_transform_position(float inX, float inY, float inZ)
{ sCUTransformation.SetPosition(inX, inY, inZ); }
extern "C" void ee_screenscpp_cu_model_frame(uintptr_t inCtl) { CU(inCtl)->mModel->SetGraphicsCurrentFrame(0); }
extern "C" float ee_screenscpp_cu_camera_zoom() { return gCameraZoom; }
extern "C" void ee_screenscpp_cu_model_scale(uintptr_t inCtl, float inV) { CU(inCtl)->mModel->SetGraphicsScale(inV); }
extern "C" void ee_screenscpp_cu_model_transform(uintptr_t inCtl)
{ CU(inCtl)->mModel->SetGraphicsPostModelTransform(sCUTransformation); }
extern "C" void ee_screenscpp_cu_model_prepare(uintptr_t inCtl) { CU(inCtl)->mModel->PrepareTransforms(); }
extern "C" void ee_screenscpp_cu_model_draw(uintptr_t inCtl, uint32_t inScreen)
{ gRasterizer->Draw(CU(inCtl)->mModel, inScreen, 1.0f, gOrthographicView); }
extern "C" float ee_screenscpp_cu_half_width() { return gU4E8F08.GetHalfWidth(); }
extern "C" float ee_screenscpp_cu_x_center() { return gU4E8F08.GetXCenter(); }
extern "C" float ee_screenscpp_cu_y_center() { return gU4E8F08.GetYCenter(); }
extern "C" bool ee_screenscpp_cu_ortho() { return gOrthographicView; }
extern "C" void ee_screenscpp_cu_near(float inX, float inY, float inZ) { sCUNear = GE3DPoint(inX, inY, inZ); }
extern "C" void ee_screenscpp_cu_far(float inX, float inY, float inZ) { sCUFar = GE3DPoint(inX, inY, inZ); }
extern "C" void ee_screenscpp_cu_denorm_persp_near()
{ gU4E8F08.DenormalizePerspectivePoint(sCUNear, gU4E8F08.GetFrontClippingDistance(), sCUNearWorld); }
extern "C" void ee_screenscpp_cu_denorm_persp_far()
{ gU4E8F08.DenormalizePerspectivePoint(sCUFar, gU4E8F08.GetRearClippingDistance(), sCUFarWorld); }
extern "C" void ee_screenscpp_cu_denorm_ortho_near() { gU4E8F08.DenormalizeOrthographicPoint(sCUNear, sCUNearWorld); }
extern "C" void ee_screenscpp_cu_denorm_ortho_far() { gU4E8F08.DenormalizeOrthographicPoint(sCUFar, sCUFarWorld); }
extern "C" void ee_screenscpp_cu_line_define() { sCULine.Define(sCUNearWorld, sCUFarWorld); }
extern "C" void ee_screenscpp_cu_line_solve(float inZ) { sCULine.SolveForZ(inZ, sCUT, sCUTarget); }
extern "C" float ee_screenscpp_cu_target_x() { return sCUTarget.GetX(); }
extern "C" float ee_screenscpp_cu_target_y() { return sCUTarget.GetY(); }
extern "C" float ee_screenscpp_cu_target_z() { return sCUTarget.GetZ(); }
extern "C" void ee_screenscpp_cu_point_set(float inX, float inY, float inZ) { sCUTarget = GE3DPoint(inX, inY, inZ); }
extern "C" bool ee_screenscpp_cu_point_at(float inX, float inY)
{ return EEPointUnitOrTile(GE2DPoint(inX, inY), sCUTile, sCUTarget, true); }
extern "C" void ee_screenscpp_cu_bounds_reset() { sCUBounds = GERectangle(); }
extern "C" void ee_screenscpp_cu_group_bounds() { gWorld.GetCurrentPlayer()->mSelectedGroup->GetBounds(sCUBounds); }
extern "C" void ee_screenscpp_cu_bounds_center() { sCUBounds.GetCenter(sCUCenter); }
extern "C" float ee_screenscpp_cu_center_x() { return sCUCenter.GetX(); }
extern "C" float ee_screenscpp_cu_center_y() { return sCUCenter.GetY(); }
extern "C" void ee_screenscpp_cu_execute(uintptr_t inCtl, float inX, float inY, bool inHit, uint64_t inModifiers, bool inFacing, float inAngle, bool inDragged)
{ CU(inCtl)->mListener->Execute(inX, inY, inHit, (unsigned long)inModifiers, inFacing, inAngle, inDragged); }

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
