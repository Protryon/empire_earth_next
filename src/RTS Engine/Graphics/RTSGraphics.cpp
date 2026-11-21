// C:\EEX\RTS Engine\Graphics\RTSGraphics.cpp (__FILE__ 0x4be990). The graphics thread's engine side (RTTI
// RTSGraphics, vtable 0x441c50). LLE round 8: the bodies are Rust (rust/crates/ee_game/src/rts/graphics.rs: the
// construction, the teardown, the loop's pass, the messages, Render, RenderLobby, the render objects' passes, the
// pause and resume handshakes, the surface chain); the members forward, and the shims below are the C++ calls those
// bodies make (one statement each). The original's addresses and the bodies' history: git history before LLE round 8.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEPixelPack.h"
#include "lle/Input System.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EEUserInterface.h"
#include "ee/EEMouse.h"
#include "platform/PlatSim.h"
#include "rust/game/rts.h"

RTSGraphics::RTSGraphics(GERasterizer* inRasterizer, unsigned long inScreen, RTSMouse* inMouse,
                         UIManager* inUIManager, bool inIsFullScreen, long inThreadPriority)
{
    ee_rts_graphics_construct(this, inRasterizer, inScreen, inMouse, inUIManager, inIsFullScreen, inThreadPriority);
}

RTSGraphics::~RTSGraphics() { ee_rts_graphics_destruct(this); }
void RTSGraphics::FrameStarting() { ee_rts_graphics_objects_pass(this, 0); }

// The thread: its passes are Rust; the Sleep(1) between them stays here. Win32 Sleep, not PlatSleep: the test
// harness's time shim caps the frame rate at the Sleep import by the caller's return address (inside
// RTSGraphics::Main; tools/harness/timeshim.c), which a wrapper would hide
unsigned long RTSGraphics::Main(void* inGraphics)
{
    ee_rts_graphics_start(inGraphics);
    while (ee_rts_graphics_pass(inGraphics))
#ifdef _WIN32
        Sleep(1);
#else
        PlatSleep(1);
#endif
    return 0;
}

void RTSGraphics::PopRenderObject() { this->mRenderObjects.pop_back(); }
void RTSGraphics::PushRenderObject(RTSRenderObject* inObject) { this->mRenderObjects.push_back(inObject); }
void RTSGraphics::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2) { ee_rts_graphics_handle_message(this, inMessage, inParameter1, inParameter2); }
long RTSGraphics::RestoreSurfaces() { return this->mRasterizer->GetScreen(this->mScreen)->RestoreAllSurfaces(); }
void RTSGraphics::Render(bool inDrawMouse, bool inShowScene) { ee_rts_graphics_render(this, inDrawMouse, inShowScene); }
void RTSGraphics::RenderLobby() { ee_rts_graphics_render_lobby(this); }
void RTSGraphics::SceneStarting() { ee_rts_graphics_objects_pass(this, 1); }
void RTSGraphics::RenderScene() { ee_rts_graphics_objects_pass(this, 2); }
void RTSGraphics::FrameDone() { ee_rts_graphics_objects_pass(this, 3); }
void RTSGraphics::PostClearRenderObjects() { this->mThread->Post(0x406, NULL, NULL, true); }
void RTSGraphics::Shutdown() { ee_rts_graphics_shutdown(this); }
long RTSGraphics::PostPause() { return ee_rts_graphics_post_pause(this); }
void RTSGraphics::PostPushRenderObject(RTSRenderObject* inObject) { this->mThread->Post(0x404, inObject, NULL, true); }
void RTSGraphics::PostSetFlag5B(bool inFlag) { this->mThread->Post(0x40a, (void*)inFlag, NULL, true); }
void RTSGraphics::PostSetRasterizer(GERasterizer* inRasterizer, unsigned long inScreen) { this->mThread->Post(0x407, inRasterizer, reinterpret_cast<void*>(inScreen), true); }
void RTSGraphics::PostSetFlag54(bool inFlag) { this->mThread->Post(0x408, (void*)inFlag, NULL, true); }
void RTSGraphics::PostResume() { ee_rts_graphics_post_resume(this); }
void RTSGraphics::SetSurface(GESurface* inSurface) { ee_rts_graphics_set_surface(this, inSurface); }

// The shims graphics.rs calls (GraphicsCpp)
extern "C" {
unsigned long ee_rtsgcpp_main(void* inGraphics) { return RTSGraphics::Main(inGraphics); }
EeRtsGraphicsFields* ee_rtsgcpp_fields(RTSGraphics* inGraphics) { return inGraphics; }
void** ee_rtsgcpp_thread_slot(RTSGraphics* inGraphics) { return reinterpret_cast<void**>(&inGraphics->mThread); }
void** ee_rtsgcpp_render_now_slot(RTSGraphics* inGraphics) { return reinterpret_cast<void**>(&inGraphics->mRenderNow); }
void** ee_rtsgcpp_created_slot(RTSGraphics* inGraphics) { return reinterpret_cast<void**>(&inGraphics->mThreadCreated); }
void** ee_rtsgcpp_done_slot(RTSGraphics* inGraphics) { return reinterpret_cast<void**>(&inGraphics->mThreadDone); }
GERasterizer* ee_rtsgcpp_rasterizer(RTSGraphics* inGraphics) { return inGraphics->mRasterizer; }
void ee_rtsgcpp_set_rasterizer(RTSGraphics* inGraphics, GERasterizer* inRasterizer) { inGraphics->mRasterizer = inRasterizer; }
GESurface* ee_rtsgcpp_surface(RTSGraphics* inGraphics) { return inGraphics->mSurface; }
void ee_rtsgcpp_set_surface_field(RTSGraphics* inGraphics, GESurface* inSurface) { inGraphics->mSurface = inSurface; }
GESurface* ee_rtsgcpp_u38(RTSGraphics* inGraphics) { return inGraphics->mU38; }
void ee_rtsgcpp_set_u38(RTSGraphics* inGraphics, GESurface* inSurface) { inGraphics->mU38 = inSurface; }
RTSMouse* ee_rtsgcpp_mouse(RTSGraphics* inGraphics) { return inGraphics->mMouse; }
void ee_rtsgcpp_set_mouse(RTSGraphics* inGraphics, RTSMouse* inMouse) { inGraphics->mMouse = inMouse; }
UIManager* ee_rtsgcpp_ui_manager(RTSGraphics* inGraphics) { return inGraphics->mUIManager; }
void ee_rtsgcpp_set_ui_manager(RTSGraphics* inGraphics, UIManager* inManager) { inGraphics->mUIManager = inManager; }
GESurface* ee_rtsgcpp_lobby_frame(RTSGraphics* inGraphics) { return inGraphics->mLobbyFrame; }
void** ee_rtsgcpp_lobby_frame_slot(RTSGraphics* inGraphics) { return reinterpret_cast<void**>(&inGraphics->mLobbyFrame); }
const char* ee_rtsgcpp_thread_name() { return gGraphicsThreadName; }
void ee_rtsgcpp_handle_message(RTSGraphics* inGraphics, unsigned long inMessage, void* inFirst, void* inSecond) { inGraphics->HandleMessage(inMessage, inFirst, inSecond); }
void ee_rtsgcpp_render(RTSGraphics* inGraphics, bool inDrawMouse, bool inShowScene) { inGraphics->Render(inDrawMouse, inShowScene); }
long ee_rtsgcpp_restore_surfaces(RTSGraphics* inGraphics) { return inGraphics->RestoreSurfaces(); }
void ee_rtsgcpp_frame_starting(RTSGraphics* inGraphics) { inGraphics->FrameStarting(); }
void ee_rtsgcpp_scene_starting(RTSGraphics* inGraphics) { inGraphics->SceneStarting(); }
void ee_rtsgcpp_render_scene(RTSGraphics* inGraphics) { inGraphics->RenderScene(); }
void ee_rtsgcpp_frame_done(RTSGraphics* inGraphics) { inGraphics->FrameDone(); }
size_t ee_rtsgcpp_objects_size(RTSGraphics* inGraphics) { return inGraphics->mRenderObjects.size(); }
RTSRenderObject* ee_rtsgcpp_object_at(RTSGraphics* inGraphics, size_t inIndex) { return inGraphics->mRenderObjects[inIndex]; }
void ee_rtsgcpp_objects_push(RTSGraphics* inGraphics, RTSRenderObject* inObject) { inGraphics->PushRenderObject(inObject); }
void ee_rtsgcpp_objects_pop(RTSGraphics* inGraphics) { inGraphics->PopRenderObject(); }
void ee_rtsgcpp_objects_clear(RTSGraphics* inGraphics) { inGraphics->mRenderObjects.clear(); }
void ee_rtsgcpp_object_frame_starting(RTSRenderObject* inObject) { inObject->FrameStarting(); }
void ee_rtsgcpp_object_scene_starting(RTSRenderObject* inObject) { inObject->SceneStarting(); }
void ee_rtsgcpp_object_render_scene(RTSRenderObject* inObject) { inObject->RenderScene(); }
void ee_rtsgcpp_object_frame_done(RTSRenderObject* inObject) { inObject->FrameDone(); }
bool ee_rtsgcpp_sim_mode() { return gPlatSimMode != 0; }
bool ee_rtsgcpp_lobby_shown() { return EELobbyIsShown(); }
void ee_rtsgcpp_lobby_lock() { EELobbyLock(); }
void ee_rtsgcpp_lobby_unlock() { EELobbyUnlock(); }
void ee_rtsgcpp_mouse_input_enter() { gMouse->mInputLock->Enter(); }
void ee_rtsgcpp_mouse_input_leave() { gMouse->mInputLock->Leave(); }
void ee_rtsgcpp_mouse_refresh_xy() { gISMouse->RefreshXY(); }
void ee_rtsgcpp_mouse_update() { gMouse->Update(); }
UIForm* ee_rtsgcpp_ui_current_form(UIManager* inManager) { return inManager->GetCurrentForm(); }
void ee_rtsgcpp_ui_redraw_all(UIManager* inManager) { inManager->RedrawAll(); }
GESurface* ee_rtsgcpp_raster_secondary(GERasterizer* inRasterizer, unsigned long inScreen) { return inRasterizer->GetSecondarySurface(inScreen); }
void ee_rtsgcpp_raster_vsync(GERasterizer* inRasterizer, bool inOn, unsigned long inScreen) { inRasterizer->SetWaitForVSync(inOn, inScreen); }
void ee_rtsgcpp_raster_overdraw(GERasterizer* inRasterizer, bool inOn, unsigned long inScreen) { inRasterizer->SetDisplayOverdrawMode(inOn, inScreen); }
void ee_rtsgcpp_raster_no_texture(GERasterizer* inRasterizer, bool inOn, unsigned long inScreen) { inRasterizer->SetDisplayNoTextureMode(inOn, inScreen); }
void ee_rtsgcpp_raster_clear_view(GERasterizer* inRasterizer, unsigned long inScreen) { inRasterizer->Clear(GEColor(0.0f, 0.0f, 0.0f), inRasterizer->GetViewport(inScreen).GetViewRectangle(), inScreen); }
void ee_rtsgcpp_raster_clear_rect(GERasterizer* inRasterizer, float inRight, float inBottom, unsigned long inScreen) { inRasterizer->Clear(GEColor(0.0f, 0.0f, 0.0f), GERectangle(0.0f, 0.0f, inRight, inBottom), inScreen); }
long ee_rtsgcpp_raster_begin(GERasterizer* inRasterizer, GESurface* inSurface, unsigned long inScreen) { return inRasterizer->BeginScene(inSurface, inScreen); }
long ee_rtsgcpp_raster_end(GERasterizer* inRasterizer, unsigned long inScreen) { return inRasterizer->EndScene(inScreen); }
long ee_rtsgcpp_raster_show(GERasterizer* inRasterizer, unsigned long inScreen) { return inRasterizer->ShowScene(inScreen); }
void ee_rtsgcpp_raster_alpha_ref_zero(GERasterizer* inRasterizer, unsigned long inScreen) { inRasterizer->SetAlphaTestingRefValue(0.0f, inScreen); }
long ee_rtsgcpp_raster_create_surface(GERasterizer* inRasterizer, unsigned long inScreen, GESurfaceDescription* inDescription, GESurface** outSurface) { return inRasterizer->GetScreen(inScreen)->CreateSurface(*inDescription, outSurface); }
GESurface* ee_rtsgcpp_surf_attached(GESurface* inSurface, unsigned long inIndex) { return inSurface->GetAttachedSurface(inIndex); }
unsigned long ee_rtsgcpp_surf_attached_count(GESurface* inSurface) { return inSurface->GetAttachedSurfaceCount(); }
unsigned long ee_rtsgcpp_surf_type(GESurface* inSurface) { return inSurface->GetType(); }
void ee_rtsgcpp_surf_text_unlock(GESurface* inSurface) { inSurface->TextUnlock(); }
unsigned long ee_rtsgcpp_surf_width(GESurface* inSurface) { return inSurface->GetWidth(); }
unsigned long ee_rtsgcpp_surf_height(GESurface* inSurface) { return inSurface->GetHeight(); }
unsigned long ee_rtsgcpp_surf_pixel_width(GESurface* inSurface) { return inSurface->GetPixelWidth(); }
unsigned long ee_rtsgcpp_surf_pixel_height(GESurface* inSurface) { return inSurface->GetPixelHeight(); }
float ee_rtsgcpp_surf_render_scale(GESurface* inSurface) { return inSurface->GetRenderScale(); }
unsigned long ee_rtsgcpp_surf_pixel_format(GESurface* inSurface) { return inSurface->GetPixelFormat(); }
unsigned long ee_rtsgcpp_surf_pixel_depth(GESurface* inSurface) { return inSurface->GetPixelDepth(); }
void ee_rtsgcpp_surf_set_logical_size(GESurface* inSurface, unsigned long inWidth, unsigned long inHeight) { inSurface->SetLogicalSize(inWidth, inHeight); }
long ee_rtsgcpp_surf_lock(GESurface* inSurface) { return inSurface->Lock(); }
void ee_rtsgcpp_surf_unlock(GESurface* inSurface) { inSurface->Unlock(); }
unsigned char* ee_rtsgcpp_surf_memory(GESurface* inSurface) { return inSurface->GetMemoryAddress(); }
unsigned long ee_rtsgcpp_surf_bytes_per_row(GESurface* inSurface) { return inSurface->GetBytesPerRow(); }
void ee_rtsgcpp_surf_dereference(GESurface* inSurface) { inSurface->Dereference(); }
long ee_rtsgcpp_surf_copy(GESurface* inFrom, GESurface* inTo, float* inSource, float* inDestination) { return inFrom->CopyTo(inTo, *reinterpret_cast<GERectangle*>(inSource), *reinterpret_cast<GERectangle*>(inDestination), false); }
void ee_rtsgcpp_pack_row(const unsigned char* inSource, unsigned char* outRow, unsigned long inWidth, unsigned long inFormat) { GEPackBGRXRow(inSource, outRow, inWidth, inFormat); }
void ee_rtsgcpp_lobby_surface_size(int* outWidth, int* outHeight) { EELobbyGetSurfaceSize(*outWidth, *outHeight); }
void ee_rtsgcpp_lobby_set_presentation(float inScale, float inLeft, float inTop, float inWidth, float inHeight) { EELobbySetPresentation(inScale, inLeft, inTop, inWidth, inHeight); }
bool ee_rtsgcpp_lobby_lock_frame(EeRtsLobbyFrame* outFrame) { return EELobbyLockFrame(outFrame->pixels, outFrame->width, outFrame->height, outFrame->pitch, outFrame->logical_width, outFrame->logical_height, outFrame->serial); }
void ee_rtsgcpp_lobby_unlock_frame() { EELobbyUnlockFrame(); }
void ee_rtsgcpp_mouse_begin_frame(RTSMouse* inMouse) { inMouse->BeginFrame(); }
void ee_rtsgcpp_mouse_save_under(RTSMouse* inMouse, GESurface* inSurface) { inMouse->SaveUnder(inSurface, GE2DPoint(0.0f, 0.0f)); }
void ee_rtsgcpp_mouse_draw(RTSMouse* inMouse, GESurface* inSurface) { inMouse->Draw(inSurface, GE2DPoint(0.0f, 0.0f)); }
void ee_rtsgcpp_mouse_restore_under(RTSMouse* inMouse, GESurface* inSurface) { inMouse->RestoreUnder(inSurface, GE2DPoint(0.0f, 0.0f), true); }
void ee_rtsgcpp_mouse_end_frame(RTSMouse* inMouse) { inMouse->EndFrame(); }
}
