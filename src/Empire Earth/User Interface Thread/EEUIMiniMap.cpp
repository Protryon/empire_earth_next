// C:\EEX\Empire Earth\User Interface Thread\EEUIMiniMap.cpp (file name inferred: no __FILE__; RTTI class
// EEUIMiniMap). The mini-map control: 0x2753b1..0x276442 (the `$E` chains of sBounds and the two flare colours, ctor,
// ??_G, dtor, then the members in address order, EEAMiniMap's inline ctor after its first user). Unit flags: the exe
// module set.
#include <windows.h>
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
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEMouse.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEARust.h"
#include "ee/EEUIMiniMap.h"
#include "rust/base/math.h"
#include "rust/game/screens.h"
#include "rust/render/gfx.h"

extern unsigned long& gTexturePixelDepth;

// (EEMSendFlare's inline ctor: ee/EEMessage.h)

GERectangle EEUIMiniMap::sBounds;
static GEColor sFlareColor(1.0f, 0.0f, 0.0f);

// Creates the mini map control with its click action, the map drawn inside its border
EEUIMiniMap::EEUIMiniMap(const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inId,
                         unsigned long inBorderThickness)
    : EEUIRustControl(inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    ee_screens_mm_construct((uintptr_t)this, (uintptr_t)&inRectangle, (uint32_t)inBorderThickness);   // (mini_map_calls.rs)
}

// Snapshots the mini map for drawing: refreshes the units texture, scales the map to the window, places the
// camera outline and the blinking flare (redrawing the screen when the flare leaves the map's inner area); the
// numbers are ee_screens mini_map.rs', the C++ reads and applies them in the original's order
long EEUIMiniMap::CopyData()
{
    return (long)ee_screens_mm_copy_data((uintptr_t)this);   // (mini_map_calls.rs)
}

// NEAR: the original's frame has a zeroed dword at -4 (not identified) and builds the message's tile in the
// tile's slot after operator new

// (the Rust's calls on the control, the mouse and the world, EE_SCREENS_MM_OP_*, are mini_map_calls.rs' over the
// shims below)

// ---- the one-statement shims of ee_screens mini_map_calls.rs (UI lane round 8): each one call, no logic
#include "ee/EEScreensShims.h"
#define MM(x) reinterpret_cast<EEUIMiniMap*>(x)
static GEBitmap** const kMMBitmaps[3] = {&EEScratch<GEBitmap*>(kScratchMapBitmap), &EEScratch<GEBitmap*>(kScratchWorldBitmap), &EEScratch<GEBitmap*>(kScratchFlareBitmap)};
static const GEColor* const kMMFlareColors[2] = {&sFlareColor, &EEScratch<GEColor>(kScratchFlareBlinkColor)};
static bool EEUIMiniMap::* const kMMLines[4] = {&EEUIMiniMap::mDrawLine0, &EEUIMiniMap::mDrawLine1,
                                                &EEUIMiniMap::mDrawLine2, &EEUIMiniMap::mDrawLine3};
static GE2DPoint EEMiniMap::* const kMMCorners[4] = {&EEMiniMap::mCorner0, &EEMiniMap::mCorner1, &EEMiniMap::mCorner2,
                                                     &EEMiniMap::mCorner3};
extern "C" float ee_screenscpp_mm_rect_left(uintptr_t inRect)
{ return reinterpret_cast<const GERectangle*>(inRect)->GetLeft(); }
extern "C" float ee_screenscpp_mm_rect_top(uintptr_t inRect)
{ return reinterpret_cast<const GERectangle*>(inRect)->GetTop(); }
extern "C" void ee_screenscpp_mm_action_new(uintptr_t inMap) { MM(inMap)->mAction = new EEAMiniMap; }
extern "C" void ee_screenscpp_mm_set_origin(uintptr_t inMap, float inX, float inY)
{ MM(inMap)->mOrigin = GE2DPoint(inX, inY); }
extern "C" void ee_screenscpp_mm_set_lines(uintptr_t inMap, bool inDraw)
{ MM(inMap)->mDrawLine1 = MM(inMap)->mDrawLine3 = MM(inMap)->mDrawLine0 = MM(inMap)->mDrawLine2 = inDraw; }
extern "C" bool ee_screenscpp_mm_line_drawn(uintptr_t inMap, uint32_t inLine) { return MM(inMap)->*kMMLines[inLine]; }
extern "C" void ee_screenscpp_mm_set_show_flare(uintptr_t inMap, bool inShow) { MM(inMap)->mShowFlare = inShow; }
extern "C" void ee_screenscpp_mm_set_flare_blink(uintptr_t inMap, bool inBlink) { MM(inMap)->mFlareBlink = inBlink; }
extern "C" bool ee_screenscpp_mm_show_flare(uintptr_t inMap) { return MM(inMap)->mShowFlare; }
extern "C" bool ee_screenscpp_mm_flare_blink(uintptr_t inMap) { return MM(inMap)->mFlareBlink; }
extern "C" uintptr_t ee_screenscpp_mm_action(uintptr_t inMap) { return (uintptr_t)MM(inMap)->mAction; }
extern "C" void ee_screenscpp_mm_action_dereference(uintptr_t inMap) { MM(inMap)->mAction->Dereference(); }
extern "C" float ee_screenscpp_mm_origin_x(uintptr_t inMap) { return MM(inMap)->mOrigin.GetX(); }
extern "C" float ee_screenscpp_mm_origin_y(uintptr_t inMap) { return MM(inMap)->mOrigin.GetY(); }
extern "C" uintptr_t ee_screenscpp_mm_map_rect(uintptr_t inMap) { return (uintptr_t)&MM(inMap)->GetRectangle(); }
extern "C" uintptr_t ee_screenscpp_mm_drawn_rect(uintptr_t inMap)
{ return (uintptr_t)&MM(inMap)->GetCopiedDrawRectangle(); }
extern "C" float ee_screenscpp_mm_rect_right(uintptr_t inRect)
{ return reinterpret_cast<const GERectangle*>(inRect)->GetRight(); }
extern "C" float ee_screenscpp_mm_rect_bottom(uintptr_t inRect)
{ return reinterpret_cast<const GERectangle*>(inRect)->GetBottom(); }
extern "C" bool ee_screenscpp_mm_flare_on() { return gMiniMap->mB12185; }
extern "C" void ee_screenscpp_mm_update_world_texture() { gMiniMap->UpdateWorldTexture(); }
extern "C" float ee_screenscpp_mm_corner_x(uint32_t inCorner) { return (gMiniMap->*kMMCorners[inCorner]).GetX(); }
extern "C" float ee_screenscpp_mm_corner_y(uint32_t inCorner) { return (gMiniMap->*kMMCorners[inCorner]).GetY(); }
extern "C" uint32_t ee_screenscpp_mm_window_width() { return (uint32_t)gGameWindow->GetWidth(); }
extern "C" uint32_t ee_screenscpp_mm_window_height() { return (uint32_t)gGameWindow->GetHeight(); }
extern "C" float ee_screenscpp_mm_flare_point_x() { return gMiniMap->mU1216C.GetX(); }
extern "C" float ee_screenscpp_mm_flare_point_y() { return gMiniMap->mU1216C.GetY(); }
extern "C" float ee_screenscpp_mm_map_to_pixel(uint32_t inRow, uint32_t inColumn)
{ return gMiniMap->mMapToPixel.mMatrix[inRow][inColumn]; }
extern "C" float ee_screenscpp_mm_pixel_to_map(uint32_t inRow, uint32_t inColumn)
{ return gMiniMap->mPixelToMap.mMatrix[inRow][inColumn]; }
extern "C" uintptr_t ee_screenscpp_mm_flare_texture() { return (uintptr_t)EEScratch<GEBitmap*>(kScratchFlareBitmap)->GetTexture(gScreenIndex); }
extern "C" uint32_t ee_screenscpp_mm_texture_height(uintptr_t inTexture)
{ return (uint32_t)reinterpret_cast<GETexture*>(inTexture)->GetHeight(); }
extern "C" uint32_t ee_screenscpp_mm_texture_width(uintptr_t inTexture)
{ return (uint32_t)reinterpret_cast<GETexture*>(inTexture)->GetWidth(); }
extern "C" float ee_screenscpp_mm_flare_location_x() { return EEScratch<GEBitmap*>(kScratchFlareBitmap)->GetLocation().GetX(); }
extern "C" float ee_screenscpp_mm_flare_location_y() { return EEScratch<GEBitmap*>(kScratchFlareBitmap)->GetLocation().GetY(); }
extern "C" float ee_screenscpp_mm_flare_width() { return EEScratch<GEBitmap*>(kScratchFlareBitmap)->GetWidth(); }
extern "C" bool ee_screenscpp_mm_free_camera() { return gFreeCamera; }
extern "C" void ee_screenscpp_mm_bitmap_at_origin(uint32_t inBitmap, uintptr_t inMap)
{ (*kMMBitmaps[inBitmap])->SetLocation(MM(inMap)->mOrigin); }
extern "C" void ee_screenscpp_mm_bitmap_scale(uint32_t inBitmap, float inX, float inY)
{ (*kMMBitmaps[inBitmap])->SetXYScale(inX, inY); }
extern "C" void ee_screenscpp_mm_flare_color(bool inBlink) { EEScratch<GEBitmap*>(kScratchFlareBitmap)->SetColor(*kMMFlareColors[inBlink]); }
extern "C" void ee_screenscpp_mm_redraw_all() { gUIManager->RedrawAll(); }
extern "C" void ee_screenscpp_mm_flare_at(float inX, float inY) { EEScratch<GEBitmap*>(kScratchFlareBitmap)->SetLocation(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_mm_line_start(uintptr_t inMap, uint32_t inLine, float inX, float inY)
{ MM(inMap)->mLines[inLine].SetStartPoint(GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_mm_line_end(uintptr_t inMap, uint32_t inLine, float inX, float inY)
{ MM(inMap)->mLines[inLine].SetEndPoint(GE2DPoint(inX, inY)); }
extern "C" int64_t ee_screenscpp_mm_base_copy_data(uintptr_t inMap) { return MM(inMap)->UIControl::CopyData(); }
extern "C" uintptr_t ee_screenscpp_mm_description() { return (uintptr_t)&EEScratch<GETextureDescription>(kScratchMMDescription); }
extern "C" void ee_screenscpp_mm_set_description()
{ EEScratch<GETextureDescription>(kScratchMMDescription) = GETextureDescription{1, 0, gTexturePixelDepth, 0, false, false, true, true}; }
extern "C" void ee_screenscpp_mm_bitmap_new(uint32_t inBitmap) { *kMMBitmaps[inBitmap] = new GEBitmap; }
extern "C" uintptr_t ee_screenscpp_mm_bitmap(uint32_t inBitmap) { return (uintptr_t)*kMMBitmaps[inBitmap]; }
extern "C" void ee_screenscpp_mm_bitmap_dereference(uint32_t inBitmap) { (*kMMBitmaps[inBitmap])->Dereference(); }
extern "C" void ee_screenscpp_mm_bitmap_clear(uint32_t inBitmap) { *kMMBitmaps[inBitmap] = NULL; }
extern "C" void ee_screenscpp_texture_wrap(uintptr_t inTexture)
{ reinterpret_cast<GETexture*>(inTexture)->SetStageTextureAddressingMode(kAddressingWrap, -1); }
extern "C" void ee_screenscpp_mm_flare_texture_set(uintptr_t inTexture)
{ EEScratch<GEBitmap*>(kScratchFlareBitmap)->SetTexture(reinterpret_cast<GETexture*>(inTexture), gScreenIndex); }
extern "C" void ee_screenscpp_mm_terrain_enter() { gMiniMap->mTerrainCriticalSection->Enter(); }
extern "C" void ee_screenscpp_mm_terrain_leave() { gMiniMap->mTerrainCriticalSection->Leave(); }
extern "C" int64_t ee_screenscpp_mm_base_draw(uintptr_t inMap) { return MM(inMap)->UIControl::DrawPolygons(); }
extern "C" int64_t ee_screenscpp_mm_draw_bitmap(uintptr_t inMap, uint32_t inBitmap)
{ return MM(inMap)->GetRasterizer()->Draw(*kMMBitmaps[inBitmap], gScreenIndex); }
extern "C" int64_t ee_screenscpp_mm_draw_line(uintptr_t inMap, uint32_t inLine)
{ return MM(inMap)->GetRasterizer()->Draw(&MM(inMap)->mLines[inLine], 1, gScreenIndex); }
extern "C" void ee_screenscpp_mm_save_view_rect(uintptr_t inMap)
{ EEScratch<GERectangle>(kScratchMMViewRect) = MM(inMap)->GetRasterizer()->GetViewport(gScreenIndex).GetViewRectangle(); }
extern "C" void ee_screenscpp_mm_viewport_load(uintptr_t inMap)
{ EEScratch<GEViewport>(kScratchMMViewport) = MM(inMap)->GetRasterizer()->GetViewport(gScreenIndex); }
extern "C" void ee_screenscpp_mm_viewport_clip(float inLeft, float inTop, float inRight, float inBottom)
{ EEScratch<GEViewport>(kScratchMMViewport).SetViewRectangle(GERectangle(inLeft, inTop, inRight, inBottom)); }
extern "C" void ee_screenscpp_mm_viewport_saved() { EEScratch<GEViewport>(kScratchMMViewport).SetViewRectangle(EEScratch<GERectangle>(kScratchMMViewRect)); }
extern "C" void ee_screenscpp_mm_viewport_apply(uintptr_t inMap)
{ MM(inMap)->GetRasterizer()->SetViewport(EEScratch<GEViewport>(kScratchMMViewport), gScreenIndex); }
extern "C" float ee_screenscpp_mm_mouse_x(uintptr_t inMap) { return MM(inMap)->GetMouseLocation().GetX(); }
extern "C" float ee_screenscpp_mm_mouse_y(uintptr_t inMap) { return MM(inMap)->GetMouseLocation().GetY(); }
extern "C" float ee_screenscpp_mm_form_mouse_x(uintptr_t inMap)
{ return MM(inMap)->GetForm()->GetMouseLocation().GetX(); }
extern "C" float ee_screenscpp_mm_form_mouse_y(uintptr_t inMap)
{ return MM(inMap)->GetForm()->GetMouseLocation().GetY(); }
extern "C" uintptr_t ee_screenscpp_mm_bounds() { return (uintptr_t)&EEUIMiniMap::sBounds; }
extern "C" uint32_t ee_screenscpp_mm_modifiers() { return (uint32_t)gISKeyboard->GetModifierState(); }
extern "C" void ee_screenscpp_mm_action_mode(uintptr_t inMap, int64_t inMode)
{ MM(inMap)->mAction->mMode = (long)inMode; }
extern "C" void ee_screenscpp_mm_action_point(uintptr_t inMap, float inX, float inY)
{ MM(inMap)->mAction->mPoint = GE2DPoint(inX, inY); }
extern "C" void ee_screenscpp_mm_action_modifiers(uintptr_t inMap, uint32_t inModifiers)
{ MM(inMap)->mAction->mModifiers = inModifiers; }
extern "C" void ee_screenscpp_mm_post_action(uintptr_t inMap) { gUserInterface->PostAction(MM(inMap)->mAction); }
extern "C" void ee_screenscpp_mm_capture(uintptr_t inMap) { MM(inMap)->CaptureMouse(); }
extern "C" void ee_screenscpp_mm_base_set_location(uintptr_t inMap, float inX, float inY)
{ MM(inMap)->UIControl::SetLocation(GE2DPoint(inX, inY)); }
extern "C" uint32_t ee_screenscpp_mm_border(uintptr_t inMap) { return (uint32_t)MM(inMap)->GetBorderThickness(); }
extern "C" bool ee_screenscpp_mm_tile_exists(int64_t inX, int64_t inY)
{ return ::gWorld.mMap->GetTileAtLocation(U2DPoint<long>((long)inX, (long)inY)) != NULL; }
extern "C" void ee_screenscpp_mm_send_flare(int64_t inX, int64_t inY)
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSendFlare, {(long)inX, (long)inY}), kSlotNone); }
extern "C" void ee_screenscpp_mm_pointer_clear() { gMouse->mPointer = 0; }
extern "C" void ee_screenscpp_mm_release(EEUIMiniMap* inMap) { inMap->ReleaseCapturedMouse(); }
extern "C" bool ee_screenscpp_mm_has_capture(uintptr_t inMap) { return MM(inMap)->GetHasMouseCapture(); }
extern "C" bool ee_screenscpp_mm_left_down(uintptr_t inMap) { return MM(inMap)->GetLeftMouseButtonDown(); }
extern "C" int64_t ee_screenscpp_mm_pointer() { return gMouse->mPointer; }
extern "C" void ee_screenscpp_mm_form_ui_state_clear(uintptr_t inMap)
{ static_cast<EEUIFMainGame*>(MM(inMap)->GetForm())->SetUIState(0); }
extern "C" int32_t ee_screenscpp_mm_map_width() { return (int32_t)::gWorld.mMap->mL195614; }
extern "C" int32_t ee_screenscpp_mm_map_height() { return (int32_t)::gWorld.mMap->mL195610; }
extern "C" int32_t ee_screenscpp_mm_map_size() { return (int32_t)::gWorld.mMap->mSize; }
extern "C" float ee_screenscpp_mm_update_map_to_pixel(uintptr_t inMinimap, uint32_t inRow, uint32_t inColumn)
{ return reinterpret_cast<EEMiniMap*>(inMinimap)->mMapToPixel.mMatrix[inRow][inColumn]; }
extern "C" void ee_screenscpp_mm_world_coordinates(float inExtent)
{ EEScratch<GEBitmap*>(kScratchWorldBitmap)->SetTextureCoordinates(GETextureCoordinate(), GETextureCoordinate(inExtent, 0.0f), GETextureCoordinate(0.0f, inExtent), GETextureCoordinate(inExtent, inExtent)); }
extern "C" void ee_screenscpp_mm_set_bounds(float inLeft, float inTop, float inRight, float inBottom)
{ EEUIMiniMap::sBounds = GERectangle(inLeft, inTop, inRight, inBottom); }
extern "C" void ee_screenscpp_mm_set_transformation(uintptr_t inMinimap)
{ gMiniMapTransformation = reinterpret_cast<EEMiniMap*>(inMinimap)->mPixelToMap; }
extern "C" void ee_screenscpp_mm_terrain_texture_set(uintptr_t inMinimap)
{ EEScratch<GEBitmap*>(kScratchMapBitmap)->SetTexture(reinterpret_cast<EEMiniMap*>(inMinimap)->mTerrainTexture, gScreenIndex); }
extern "C" void ee_screenscpp_mm_world_texture_set(uintptr_t inMinimap)
{ EEScratch<GEBitmap*>(kScratchWorldBitmap)->SetTexture(reinterpret_cast<EEMiniMap*>(inMinimap)->mWorldTexture, gScreenIndex); }

// the media and the update as the game calls them (Empire Earth.cpp, the world, the mini map: ee/EEGameUI.h)
#include "ee/EEGameUI.h"
static const bool sMiniMapRegistered = (gEEGameUI.mMiniMapLoadMedia = EEUIMiniMap::LoadMedia,
                                        gEEGameUI.mMiniMapUnloadMedia = EEUIMiniMap::UnloadMedia,
                                        gEEGameUI.mMiniMapUpdate = EEUIMiniMap::Update, true);
