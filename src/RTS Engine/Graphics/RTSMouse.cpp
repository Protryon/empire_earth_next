// C:\EEX\RTS Engine\Graphics\RTSMouse.cpp (__FILE__ 0x4beb70). The mouse (RTTI RTSMouse, vtable 0x441c74). LLE
// round 8: the bodies are Rust (rust/crates/ee_game/src/rts/mouse.rs: the construction, the pointer loads with the
// texture pack's, Load, Update, the frame's start and end; the frame itself is ee_render gfx mouse_frame.rs); the
// members forward, and the shims below are the C++ calls those bodies make (one statement each). The original's
// addresses and the bodies' history: git history before LLE round 8.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "lle/LowLevelEngine.h"
#include "platform/PlatWindow.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GEScreen.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/Input System.h"

#include "ee/RTSMouse.h"
static_assert(sizeof(GE2DPoint) == sizeof(EeGe2DPoint) && sizeof(GERectangle) == sizeof(EeGeRectangle), "GE mirrors");
static_assert(sizeof(GEVideoMode) == sizeof(EeRtsVideoMode) && sizeof(GESurfaceDescription) == sizeof(EeGeSurfaceDescription), "mirrors");

// 0x5297a8: the pointer textures' transparent colour (blue)
static GEColor sTransparencyColor(0.0f, 0.0f, 1.0f);

// The frame's surface calls (ee_gfx mouse_frame.rs)
static uint32_t EEMouseWidth(void* inSurface) { return (uint32_t)static_cast<GESurface*>(inSurface)->GetWidth(); }
static uint32_t EEMouseHeight(void* inSurface) { return (uint32_t)static_cast<GESurface*>(inSurface)->GetHeight(); }
static int32_t EEMouseCopy(void* inFrom, void* inTo, const EeGeRectangle* inSource, const EeGeRectangle* inDestination,
                           bool inTransparent)
{
    GERectangle theSource = *reinterpret_cast<const GERectangle*>(inSource);   // CopyTo takes non-const references
    GERectangle theDestination = *reinterpret_cast<const GERectangle*>(inDestination);
    return (int32_t)static_cast<GESurface*>(inFrom)->CopyTo(static_cast<GESurface*>(inTo), theSource, theDestination,
                                                            inTransparent);
}
static void EEMouseReference(void* inSurface) { static_cast<GESurface*>(inSurface)->Reference(); }
static void EEMouseDereference(void* inSurface) { static_cast<GESurface*>(inSurface)->Dereference(); }
static const EeGfxMouseSurfaces kMouseSurfaces = {EEMouseWidth, EEMouseHeight, EEMouseCopy, EEMouseReference,
                                                  EEMouseDereference};

// One of the frame's calls (ee_gfx_mouse_frame_call's ops)
long RTSMouseFrameCall(RTSMouse* inMouse, uint32_t inOp, GESurface* inSurface, const GE2DPoint* inOrigin, bool inFlag)
{
    return ee_gfx_mouse_frame_call(inMouse->mFrame, &kMouseSurfaces, inOp, inSurface,
                                   reinterpret_cast<const EeGe2DPoint*>(inOrigin), (uint32_t)inMouse->mPointer,
                                   (uint32_t)inMouse->mOverlay, inFlag);
}

// Pointer slot inIndex (0..23) or save-under record 24 + k's surface, filled in place by the loads
GESurface** RTSMouseSlot(RTSMouse* inMouse, unsigned long inIndex)
{
    return reinterpret_cast<GESurface**>(ee_gfx_mouse_frame_slot(inMouse->mFrame, (uint32_t)inIndex));
}

RTSMouse::RTSMouse() : mFrame(ee_gfx_mouse_frame_new()) { ee_rts_mouse_construct(this); }
// the locks (Rust), then the frame
RTSMouse::~RTSMouse()
{
    ee_rts_mouse_destruct(this);
    ee_gfx_mouse_frame_free(this->mFrame);
}
void RTSMouse::SaveUnder(GESurface* inSurface, const GE2DPoint& inOrigin) { RTSMouseFrameCall(this, 1, inSurface, &inOrigin, false); }
long RTSMouse::RestoreUnder(GESurface* inSurface, const GE2DPoint& inOrigin, bool inIsWindowed) { return RTSMouseFrameCall(this, 3, inSurface, &inOrigin, inIsWindowed); }
void RTSMouse::Draw(GESurface* inSurface, const GE2DPoint& inOrigin) { RTSMouseFrameCall(this, 2, inSurface, &inOrigin, false); }
// the pointer textures, both save-under surfaces and the frame's pointer released; 0
long RTSMouse::Release()
{
    RTSMouseFrameCall(this, 6, NULL, NULL, false);
    return 0;
}

// The shims mouse.rs calls (MouseCpp)
extern "C" {
EeRtsMouseFields* ee_rtsmcpp_fields(RTSMouse* inMouse) { return inMouse; }
void** ee_rtsmcpp_render_lock_slot(RTSMouse* inMouse) { return reinterpret_cast<void**>(&inMouse->mRenderLock); }
void** ee_rtsmcpp_input_lock_slot(RTSMouse* inMouse) { return reinterpret_cast<void**>(&inMouse->mInputLock); }
GE2DPoint* ee_rtsmcpp_position(RTSMouse* inMouse) { return &inMouse->mPosition; }
GERectangle* ee_rtsmcpp_bounds(RTSMouse* inMouse) { return &inMouse->mBounds; }
long ee_rtsmcpp_frame_call(RTSMouse* inMouse, uint32_t inOp, GESurface* inSurface, const GE2DPoint* inOrigin, bool inFlag) { return RTSMouseFrameCall(inMouse, inOp, inSurface, inOrigin, inFlag); }
void** ee_rtsmcpp_slot(RTSMouse* inMouse, unsigned long inIndex) { return reinterpret_cast<void**>(RTSMouseSlot(inMouse, inIndex)); }
void ee_rtsmcpp_display_size(int32_t* outWidth, int32_t* outHeight) { PlatWindowGetDisplaySize(NULL, outWidth, outHeight); }
const void* ee_rtsmcpp_is_mouse() { return gISMouse; }
void ee_rtsmcpp_debug_out_long(const char* inFormat, long inValue) { ESDebugOut(inFormat, inValue); }
void ee_rtsmcpp_screen_mode(GEScreen* inScreen, EeRtsVideoMode* outMode) { *reinterpret_cast<GEVideoMode*>(outMode) = inScreen->GetCurrentMode(); }
long ee_rtsmcpp_screen_create_surface(GEScreen* inScreen, GESurfaceDescription* inDescription, GESurface** ioSurface) { return inScreen->CreateSurface(*inDescription, ioSurface); }
long ee_rtsmcpp_pack_shift() { return GETextureManager::GetTexturePackLogicalShift(); }
long ee_rtsmcpp_pack_drop() { return GETextureManager::GetTexturePackMipLevelsToDrop(); }
UString* ee_rtsmcpp_text_new() { return new UString; }
void ee_rtsmcpp_text_delete(UString* inText) { delete inText; }
const char* ee_rtsmcpp_text_chars(UString* inText) { return *inText; }
bool ee_rtsmcpp_pack_name(const char* inFile, UString* outName) { return GETextureManager::GetTexturePackName(UString(inFile), *outName); }
long ee_rtsmcpp_find_text(GEScreen* inScreen, UString* inName, FSFileSpec** outFile) { return inScreen->GetTextureManager()->GetTexturePath()->FindFile(*inName, *outFile); }
long ee_rtsmcpp_find_file(GEScreen* inScreen, const char* inName, FSFileSpec** outFile) { return inScreen->GetTextureManager()->GetTexturePath()->FindFile(UString(inName), *outFile); }
long ee_rtsmcpp_file_open_read(FSFileSpec* inFile) { return inFile->Open(kFileAccessRead); }
long ee_rtsmcpp_file_png_attributes(FSFileSpec* inFile, unsigned long* outWidth, unsigned long* outHeight, unsigned long* outDepth) { return GESurface::GetImageFileAttributes(*inFile, kImageFilePNG, *outWidth, *outHeight, *outDepth); }
long ee_rtsmcpp_file_rewind(FSFileSpec* inFile) { return inFile->SetPosition(0, kFileOriginBegin, true); }
long ee_rtsmcpp_file_close(FSFileSpec* inFile) { return inFile->Close(); }
void ee_rtsmcpp_file_delete(FSFileSpec* inFile) { delete inFile; }
long ee_rtsmcpp_surf_load_png(GESurface* inSurface, FSFileSpec* inFile) { return inSurface->Load(*inFile, kImageFilePNG); }
unsigned long ee_rtsmcpp_surf_logical_shift(GESurface* inSurface) { return inSurface->GetLogicalShift(); }
void ee_rtsmcpp_surf_set_logical_shift(GESurface* inSurface, unsigned long inShift) { inSurface->SetLogicalShift(inShift); }
void ee_rtsmcpp_surf_set_transparency(GESurface* inSurface) { inSurface->SetTransparencyColor(sTransparencyColor); }
}
