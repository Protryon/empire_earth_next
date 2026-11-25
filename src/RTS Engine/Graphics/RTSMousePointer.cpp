// C:\EEX\RTS Engine\Graphics\RTSMousePointer.cpp (__FILE__ 0x4bec78). The mouse-pointer thread (RTTI
// RTSMousePointer, vtable 0x441c8c). LLE round 8: the bodies are Rust (rust/crates/ee_game/src/rts/mouse_pointer.rs:
// the construction, the teardown, the thread, the messages); the members forward, and the shims below are the C++
// calls those bodies make (one statement each). The original's addresses and the bodies' history: git history before
// LLE round 8.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GESurface.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/Input System.h"

#include "ee/RTSMouse.h"
#include "ee/RTSMousePointer.h"
#include "rust/base/math.h"
#include "rust/game/rts.h"

// The shims mouse_pointer.rs calls (PointerCpp)
extern "C" {
void** ee_rtsmpcpp_thread_slot(RTSMousePointer* inPointer) { return reinterpret_cast<void**>(&inPointer->mThread); }
void** ee_rtsmpcpp_created_slot(RTSMousePointer* inPointer) { return reinterpret_cast<void**>(&inPointer->mThreadCreated); }
void** ee_rtsmpcpp_done_slot(RTSMousePointer* inPointer) { return reinterpret_cast<void**>(&inPointer->mThreadDone); }
bool* ee_rtsmpcpp_shown_slot(RTSMousePointer* inPointer) { return const_cast<bool*>(&inPointer->mIsShown); }
bool* ee_rtsmpcpp_quit_slot(RTSMousePointer* inPointer) { return &inPointer->mQuit; }
GESurface* ee_rtsmpcpp_surface(RTSMousePointer* inPointer) { return inPointer->mSurface; }
void ee_rtsmpcpp_set_surface(RTSMousePointer* inPointer, GESurface* inSurface) { inPointer->mSurface = inSurface; }
RTSMouse* ee_rtsmpcpp_mouse(RTSMousePointer* inPointer) { return inPointer->mMouse; }
void ee_rtsmpcpp_set_mouse(RTSMousePointer* inPointer, RTSMouse* inMouse) { inPointer->mMouse = inMouse; }
const char* ee_rtsmpcpp_thread_name() { return gMousePointerThreadName; }
void ee_rtsmpcpp_mouse_position(RTSMouse* inMouse, EeGe2DPoint* outPosition) { *reinterpret_cast<GE2DPoint*>(outPosition) = inMouse->mPosition; }
bool ee_rtsmpcpp_full_screen(GESurface* inSurface) { return inSurface->GetParentScreen()->GetIsFullScreen(); }
void ee_rtsmpcpp_client_origin(RTSMousePointer* inPointer, EeGe2DPoint* outOrigin) { *reinterpret_cast<GE2DPoint*>(outOrigin) = inPointer->mWindow->GetClientOrigin(); }
void ee_rtsmpcpp_mouse_restore_at(RTSMouse* inMouse, GESurface* inSurface, const EeGe2DPoint* inOrigin) { inMouse->RestoreUnder(inSurface, *reinterpret_cast<const GE2DPoint*>(inOrigin), false); }
void ee_rtsmpcpp_mouse_save_at(RTSMouse* inMouse, GESurface* inSurface, const EeGe2DPoint* inOrigin) { inMouse->SaveUnder(inSurface, *reinterpret_cast<const GE2DPoint*>(inOrigin)); }
void ee_rtsmpcpp_mouse_draw_at(RTSMouse* inMouse, GESurface* inSurface, const EeGe2DPoint* inOrigin) { inMouse->Draw(inSurface, *reinterpret_cast<const GE2DPoint*>(inOrigin)); }
}
