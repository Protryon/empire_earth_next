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
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGraphics.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEMaterial.h"
#include "ee/EEARust.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EEScreensShims.h"
#include "rust/render/geval.h"


// Creates the main game view's right-drag command control, hidden, with its facing arrow: a textured quad model
// (CreateModel) and the "Unit Facing Arrow" texture, clamped and alpha-blended (LoadTexture; command_units_calls.rs)
EEUICCommandUnits::EEUICCommandUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex,
                                     EEAMouseClick* inListener)
    : EEUIRustControl(inId, GERectangle(gViewport.GetViewRectangle().GetTopLeft(), 2.0f, 2.0f), inForm, inIndex, 0),
      EeScreensCommandUnitsFields(), EeScreensCommandUnitsObjects()
{
    this->Rust(EE_SCREENS_CU_DO_INIT);
    this->StoreDisplayMode(kDisplayModeHidden);
    ee_screens_cu_construct((uintptr_t)this, (uintptr_t)inListener);   // (command_units_calls.rs)
}

// The class's own ops (the arrow's draw, the ground point, the pick, the group's centre, the click: the old RustOp)
// are command_units_ops.rs' over the one-statement shims below (UI lane round 11)
#define CU(c) static_cast<EEUICCommandUnits*>((EEUIRustControl*)(c))

extern "C" uint32_t ee_screenscpp_cu_screen() { return (uint32_t)gScreenIndex; }
extern "C" void ee_screenscpp_cu_save_ambient_color() { EEScratch<GEColor>(kScratchCUAmbientColor) = gRasterizer->GetAmbientColor(); }
extern "C" void ee_screenscpp_cu_save_viewport() { EEScratch<GEViewport>(kScratchCUViewport) = gRasterizer->GetViewport(gScreenIndex); }
extern "C" float ee_screenscpp_cu_ambient_intensity() { return gRasterizer->GetAmbientIntensity(); }
extern "C" uint64_t ee_screenscpp_cu_lighting(uint32_t inScreen)
{ return (uint64_t)gRasterizer->GetLighting(inScreen); }
extern "C" uint64_t ee_screenscpp_cu_zbuffer(uint32_t inScreen) { return (uint64_t)gRasterizer->GetZBuffer(inScreen); }
extern "C" uint64_t ee_screenscpp_cu_zcompare(uint32_t inScreen)
{ return (uint64_t)gRasterizer->GetZCompare(inScreen); }
extern "C" void ee_screenscpp_cu_depth_range(float inNear, float inFar) { ee_ge_viewport_set_depth_range(&gU4E8F08, inNear, inFar); }
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
{ gRasterizer->SetViewport(EEScratch<GEViewport>(kScratchCUViewport), inScreen); }
extern "C" void ee_screenscpp_cu_restore_ambient_color(uint32_t inScreen)
{ gRasterizer->SetAmbientColor(EEScratch<GEColor>(kScratchCUAmbientColor), inScreen); }
extern "C" void ee_screenscpp_cu_transform_reset() { EEScratch<GETransformation>(kScratchCUTransformation) = GETransformation(); }
extern "C" void ee_screenscpp_cu_transform_yaw(float inYaw) { EEScratch<GETransformation>(kScratchCUTransformation).SetOrientationYPR(inYaw, 0.0f, 0.0f); }
extern "C" void ee_screenscpp_cu_transform_position(float inX, float inY, float inZ)
{ EEScratch<GETransformation>(kScratchCUTransformation).SetPosition(inX, inY, inZ); }
extern "C" void ee_screenscpp_cu_model_frame(uintptr_t inCtl) { static_cast<GEModel*>(CU(inCtl)->mModel)->SetGraphicsCurrentFrame(0); }
extern "C" float ee_screenscpp_cu_camera_zoom() { return gWorld.mCameraControl.mZoom; }
extern "C" void ee_screenscpp_cu_model_scale(uintptr_t inCtl, float inV) { static_cast<GEModel*>(CU(inCtl)->mModel)->SetGraphicsScale(inV); }
extern "C" void ee_screenscpp_cu_model_transform(uintptr_t inCtl)
{ static_cast<GEModel*>(CU(inCtl)->mModel)->SetGraphicsPostModelTransform(EEScratch<GETransformation>(kScratchCUTransformation)); }
extern "C" void ee_screenscpp_cu_model_prepare(uintptr_t inCtl) { static_cast<GEModel*>(CU(inCtl)->mModel)->PrepareTransforms(); }
extern "C" void ee_screenscpp_cu_model_draw(uintptr_t inCtl, uint32_t inScreen)
{ gRasterizer->Draw(static_cast<GEModel*>(CU(inCtl)->mModel), inScreen, 1.0f, gOrthographicView); }
extern "C" float ee_screenscpp_cu_half_width() { return gU4E8F08.GetHalfWidth(); }
extern "C" float ee_screenscpp_cu_x_center() { return gU4E8F08.GetXCenter(); }
extern "C" float ee_screenscpp_cu_y_center() { return gU4E8F08.GetYCenter(); }
extern "C" bool ee_screenscpp_cu_ortho() { return gOrthographicView; }
extern "C" void ee_screenscpp_cu_near(float inX, float inY, float inZ) { EEScratch<GE3DPoint>(kScratchCUNear) = GE3DPoint(inX, inY, inZ); }
extern "C" void ee_screenscpp_cu_far(float inX, float inY, float inZ) { EEScratch<GE3DPoint>(kScratchCUFar) = GE3DPoint(inX, inY, inZ); }
extern "C" void ee_screenscpp_cu_denorm_persp_near()
{ gU4E8F08.DenormalizePerspectivePoint(EEScratch<GE3DPoint>(kScratchCUNear), gU4E8F08.GetFrontClippingDistance(), EEScratch<GE3DPoint>(kScratchCUNearWorld)); }
extern "C" void ee_screenscpp_cu_denorm_persp_far()
{ gU4E8F08.DenormalizePerspectivePoint(EEScratch<GE3DPoint>(kScratchCUFar), gU4E8F08.GetRearClippingDistance(), EEScratch<GE3DPoint>(kScratchCUFarWorld)); }
extern "C" void ee_screenscpp_cu_denorm_ortho_near() { gU4E8F08.DenormalizeOrthographicPoint(EEScratch<GE3DPoint>(kScratchCUNear), EEScratch<GE3DPoint>(kScratchCUNearWorld)); }
extern "C" void ee_screenscpp_cu_denorm_ortho_far() { gU4E8F08.DenormalizeOrthographicPoint(EEScratch<GE3DPoint>(kScratchCUFar), EEScratch<GE3DPoint>(kScratchCUFarWorld)); }
extern "C" void ee_screenscpp_cu_line_define() { EEScratch<GE3DLine>(kScratchCULine).Define(EEScratch<GE3DPoint>(kScratchCUNearWorld), EEScratch<GE3DPoint>(kScratchCUFarWorld)); }
extern "C" void ee_screenscpp_cu_line_solve(float inZ) { EEScratch<GE3DLine>(kScratchCULine).SolveForZ(inZ, EEScratch<float>(kScratchCUT), EEScratch<GE3DPoint>(kScratchCUTarget)); }
extern "C" float ee_screenscpp_cu_target_x() { return EEScratch<GE3DPoint>(kScratchCUTarget).GetX(); }
extern "C" float ee_screenscpp_cu_target_y() { return EEScratch<GE3DPoint>(kScratchCUTarget).GetY(); }
extern "C" float ee_screenscpp_cu_target_z() { return EEScratch<GE3DPoint>(kScratchCUTarget).GetZ(); }
extern "C" void ee_screenscpp_cu_point_set(float inX, float inY, float inZ) { EEScratch<GE3DPoint>(kScratchCUTarget) = GE3DPoint(inX, inY, inZ); }
extern "C" bool ee_screenscpp_cu_point_at(float inX, float inY)
{ return EEPointUnitOrTile(GE2DPoint(inX, inY), EEScratch<EETile*>(kScratchCUTile), EEScratch<GE3DPoint>(kScratchCUTarget), true); }
extern "C" void ee_screenscpp_cu_bounds_reset() { EEScratch<GERectangle>(kScratchCUBounds) = GERectangle(); }
extern "C" void ee_screenscpp_cu_group_bounds() { gWorld.GetCurrentPlayer()->mSelectedGroup->GetBounds(EEScratch<GERectangle>(kScratchCUBounds)); }
extern "C" void ee_screenscpp_cu_bounds_center() { EEScratch<GERectangle>(kScratchCUBounds).GetCenter(EEScratch<GE2DPoint>(kScratchCUCenter)); }
extern "C" float ee_screenscpp_cu_center_x() { return EEScratch<GE2DPoint>(kScratchCUCenter).GetX(); }
extern "C" float ee_screenscpp_cu_center_y() { return EEScratch<GE2DPoint>(kScratchCUCenter).GetY(); }
extern "C" void ee_screenscpp_cu_execute(uintptr_t inCtl, float inX, float inY, bool inHit, uint64_t inModifiers, bool inFacing, float inAngle, bool inDragged)
{ static_cast<EEAMouseClick*>(CU(inCtl)->mListener)->Execute(inX, inY, inHit, (unsigned long)inModifiers, inFacing, inAngle, inDragged); }

// ---- the one-statement shims of ee_screens command_units_calls.rs (UI lane round 8: the constructor's arrow, the
// destructor): each one call, no logic
#define CU(x) reinterpret_cast<EEUICCommandUnits*>(x)
extern "C" void ee_screenscpp_cu_set_listener(uintptr_t inCu, uintptr_t inListener)
{ CU(inCu)->mListener = reinterpret_cast<EEAMouseClick*>(inListener); }
extern "C" void ee_screenscpp_cu_listener_reference(uintptr_t inCu) { static_cast<EEAMouseClick*>(CU(inCu)->mListener)->Reference(); }
extern "C" void ee_screenscpp_cu_always_dirty(uintptr_t inCu) { CU(inCu)->SetAlwaysDirty(true); }
extern "C" void ee_screenscpp_cu_model_new(uintptr_t inCu)
{ CU(inCu)->mModel = gModelManager->CreateModel(2, 4, 1, 0, 1, 0, "Unit Direction Model", 1); }
extern "C" void ee_screenscpp_cu_model_reference(uintptr_t inCu) { static_cast<GEModel*>(CU(inCu)->mModel)->Reference(); }
extern "C" void ee_screenscpp_cu_point(uintptr_t inCu, uint32_t inIndex, float inX, float inY, float inZ)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetVertexArray(0)->SetPoint(GE3DPoint(inX, inY, inZ), inIndex); }
extern "C" void ee_screenscpp_cu_normal(uintptr_t inCu, uint32_t inIndex)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetVertexArray(0)->SetNormal(GEVector(0.0f, 0.0f, 1.0f), inIndex); }
extern "C" void ee_screenscpp_cu_coordinate(uintptr_t inCu, uint32_t inIndex, float inU, float inV)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(inU, inV), inIndex); }
extern "C" void ee_screenscpp_cu_vertex1(uintptr_t inCu, uint32_t inPolygon, uint32_t inVertex)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetPolygonArray(0)->SetVertexIndex1(inPolygon, inVertex); }
extern "C" void ee_screenscpp_cu_vertex2(uintptr_t inCu, uint32_t inPolygon, uint32_t inVertex)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetPolygonArray(0)->SetVertexIndex2(inPolygon, inVertex); }
extern "C" void ee_screenscpp_cu_vertex3(uintptr_t inCu, uint32_t inPolygon, uint32_t inVertex)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetPolygonArray(0)->SetVertexIndex3(inPolygon, inVertex); }
extern "C" void ee_screenscpp_cu_polygon_start(uintptr_t inCu)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetMaterial(0)->SetPolygonStartingIndex(0, 0); }
extern "C" void ee_screenscpp_cu_polygon_length(uintptr_t inCu)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetMaterial(0)->SetPolygonRangeLength(2, 0); }
extern "C" void ee_screenscpp_cu_vertex_start(uintptr_t inCu)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetMaterial(0)->SetVertexStartingIndex(0); }
extern "C" void ee_screenscpp_cu_vertex_length(uintptr_t inCu)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetMaterial(0)->SetVertexRangeLength(4); }
extern "C" void ee_screenscpp_cu_bounds(uintptr_t inCu) { static_cast<GEModel*>(CU(inCu)->mModel)->ComputeBoundingVolumes(); }
extern "C" void ee_screenscpp_cu_set_description()
{ EEScratch<GETextureDescription>(kScratchCUDescription) = GETextureDescription{1, 0, gTexturePixelDepth, 0, false, false, true, true}; }
extern "C" uintptr_t ee_screenscpp_cu_description() { return (uintptr_t)&EEScratch<GETextureDescription>(kScratchCUDescription); }
extern "C" void ee_screenscpp_cu_set_texture(uintptr_t inCu, uintptr_t inTexture)
{ CU(inCu)->mTexture = reinterpret_cast<GETexture*>(inTexture); }
extern "C" void ee_screenscpp_texture_alpha_texture(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageAlphaMode(kStageOpTexture, -1); }
extern "C" void ee_screenscpp_texture_color_texture(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageColorMode(kStageOpTexture, -1); }
extern "C" void ee_screenscpp_cu_material_texture(uintptr_t inCu)
{ static_cast<GEModel*>(CU(inCu)->mModel)->GetMaterial(0)->SetTexture(static_cast<GETexture*>(CU(inCu)->mTexture), gScreenIndex); }
extern "C" void ee_screenscpp_cu_listener_dereference(uintptr_t inCu) { static_cast<EEAMouseClick*>(CU(inCu)->mListener)->Dereference(); }
extern "C" void ee_screenscpp_cu_model_dereference(uintptr_t inCu) { static_cast<GEModel*>(CU(inCu)->mModel)->Dereference(); }
extern "C" void ee_screenscpp_cu_texture_dereference(uintptr_t inCu) { static_cast<GETexture*>(CU(inCu)->mTexture)->Dereference(); }
