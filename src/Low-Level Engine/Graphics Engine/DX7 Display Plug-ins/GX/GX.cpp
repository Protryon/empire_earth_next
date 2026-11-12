// include/gx/GX.h's classes: the shims over the Rust implementation (rust/crates/ee_gx/src/ffi.rs). Every method forwards
// to an extern "C" function; the object is the opaque handle behind `this`.
#include <windows.h>

#include "gx/GX.h"
#include "platform/PlatFont.h"

// the tools/harness/presentation_bridge.c hooks, found by map address: keep them in the image (lld-link on Windows; the
// POSIX builds have no harness and link what the shim references)
#if defined(_MSC_VER)
#pragma comment(linker, "/include:GXTestSetPresentHook")
#pragma comment(linker, "/include:GXTestReadFrame")
#endif

static_assert(sizeof(GXPIXELFORMAT) == 8 * sizeof(unsigned long), "ee_gx/src/abi.rs");
static_assert(sizeof(RECT) == 16, "ee_gx/src/abi.rs");
static_assert(sizeof(GXMATRIX) == 64, "ee_gx/src/abi.rs");
static_assert(sizeof(GXMATERIAL) == 68, "ee_gx/src/abi.rs");

extern "C" {
GXRESULT gx_direct_draw_create(void** outDirectDraw);
unsigned long gx_direct_draw_add_ref(void* inDirectDraw);
unsigned long gx_direct_draw_release(void* inDirectDraw);
GXRESULT gx_direct_draw_get_caps(GXCAPS* outCaps, GXCAPS* outHELCaps);
GXRESULT gx_direct_draw_set_display_mode(void* inDirectDraw, unsigned long inWidth, unsigned long inHeight, unsigned long inBitCount);
GXRESULT gx_direct_draw_set_cooperative_level(void* inDirectDraw, void* inWindow, unsigned long inFlags);
GXRESULT gx_direct_draw_create_surface(void* inDirectDraw, GXSURFACEDESC* inDescription, void** outSurface);
GXRESULT gx_direct_draw_get_device_identifier(GXDEVICEIDENTIFIER* outIdentifier);
GXRESULT gx_direct_draw_create_vertex_buffer(unsigned long inFVF, unsigned long inCount, void** outBuffer);
GXRESULT gx_direct_draw_create_device(void* inTarget, void** outDevice);

unsigned long gx_surface_add_ref(void* inSurface);
unsigned long gx_surface_release(void* inSurface);
GXRESULT gx_surface_add_attached(void* inSurface, void* inOther);
GXRESULT gx_surface_get_attached(void* inSurface, GXSCAPS* inCaps, void** outSurface);
GXRESULT gx_surface_blt(void* inSurface, RECT* inDestination, void* inSource, RECT* inSourceRect, unsigned long inFlags, GXBLTFX* inFX);
GXRESULT gx_surface_flip(void* inSurface, unsigned long inFlags);
GXRESULT gx_surface_lock(void* inSurface, RECT* inRect, GXSURFACEDESC* outDescription, unsigned long inFlags);
GXRESULT gx_surface_unlock(void* inSurface);
GXRESULT gx_surface_set_color_key(void* inSurface, GXCOLORKEY* inKey);
float gx_surface_render_scale(void* inSurface);
GXRESULT gx_surface_draw_text(void* inSurface, const RECT* inBounds, GXSurface::TextCallback inDraw, void* inContext);

unsigned long gx_vertex_buffer_add_ref(void* inBuffer);
unsigned long gx_vertex_buffer_release(void* inBuffer);
GXRESULT gx_vertex_buffer_lock(void* inBuffer, void** outData, unsigned long* outSize);

unsigned long gx_device_add_ref(void* inDevice);
unsigned long gx_device_release(void* inDevice);
GXRESULT gx_device_get_caps(GXDEVICEDESC* outCaps);
GXRESULT gx_device_begin_scene();
GXRESULT gx_device_set_render_target(void* inDevice, void* inTarget);
GXRESULT gx_device_set_viewport(void* inDevice, GXVIEWPORT* inViewport);
GXRESULT gx_device_get_viewport(void* inDevice, GXVIEWPORT* outViewport);
GXRESULT gx_device_clear(void* inDevice, unsigned long inCount, GXRECT* inRects, unsigned long inFlags, unsigned long inColor, float inZ);
GXRESULT gx_device_set_transform(void* inDevice, uint32_t inState, GXMATRIX* inMatrix);
GXRESULT gx_device_set_render_state(void* inDevice, uint32_t inState, unsigned long inValue);
GXRESULT gx_device_set_stage_state(void* inDevice, unsigned long inStage, uint32_t inState, unsigned long inValue);
GXRESULT gx_device_set_texture(void* inDevice, unsigned long inStage, void* inTexture);
GXRESULT gx_device_set_material(void* inDevice, GXMATERIAL* inMaterial);
GXRESULT gx_device_set_light(void* inDevice, unsigned long inIndex, GXLIGHT* inLight);
GXRESULT gx_device_get_light(void* inDevice, unsigned long inIndex, GXLIGHT* outLight);
GXRESULT gx_device_light_enable(void* inDevice, unsigned long inIndex, int inEnable);
GXRESULT gx_device_draw_indexed_primitive_vb(void* inDevice, uint32_t inType, void* inBuffer, unsigned long inStart, unsigned long inCount,
                                             unsigned short* inIndices, unsigned long inIndexCount);

void gx_set_render_percent(unsigned long inPercent);
void gx_set_frame_limit(long inFramesPerSecond);
void gx_set_texture_filtering(unsigned long inMode);
void gx_set_brightness(float inGamma);
void gx_game_size(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight);
unsigned long gx_auto_ui_scale();
}

// ---- GXDirectDraw

GXRESULT GXDirectDrawCreate(GUID* inGUID, GXDirectDraw** outDirectDraw) { return gx_direct_draw_create(reinterpret_cast<void**>(outDirectDraw)); }
unsigned long GXDirectDraw::AddRef() { return gx_direct_draw_add_ref(this); }
unsigned long GXDirectDraw::Release() { return gx_direct_draw_release(this); }
GXRESULT GXDirectDraw::GetCaps(GXCAPS* outCaps, GXCAPS* outHELCaps) { return gx_direct_draw_get_caps(outCaps, outHELCaps); }
GXRESULT GXDirectDraw::SetDisplayMode(unsigned long inWidth, unsigned long inHeight, unsigned long inBitCount, unsigned long inRefreshRate, unsigned long inFlags) { return gx_direct_draw_set_display_mode(this, inWidth, inHeight, inBitCount); }
GXRESULT GXDirectDraw::SetCooperativeLevel(PlatWindow* inWindow, unsigned long inFlags) { return gx_direct_draw_set_cooperative_level(this, inWindow, inFlags); }
GXRESULT GXDirectDraw::CreateSurface(GXSURFACEDESC* inDescription, GXSurface** outSurface, void* inOuter) { return gx_direct_draw_create_surface(this, inDescription, reinterpret_cast<void**>(outSurface)); }
GXRESULT GXDirectDraw::GetDeviceIdentifier(GXDEVICEIDENTIFIER* outIdentifier, unsigned long inFlags) { return gx_direct_draw_get_device_identifier(outIdentifier); }
GXRESULT GXDirectDraw::RestoreAllSurfaces() { return GX_OK; }
GXRESULT GXDirectDraw::CreateDevice(GXIID inKind, GXSurface* inTarget, GXDevice** outDevice) { return gx_direct_draw_create_device(inTarget, reinterpret_cast<void**>(outDevice)); }
GXRESULT GXDirectDraw::CreateVertexBuffer(GXVERTEXBUFFERDESC* inDescription, GXVertexBuffer** outBuffer, unsigned long inFlags) { return gx_direct_draw_create_vertex_buffer(inDescription->dwFVF, inDescription->dwNumVertices, reinterpret_cast<void**>(outBuffer)); }

// ---- GXSurface

unsigned long GXSurface::AddRef() { return gx_surface_add_ref(this); }
unsigned long GXSurface::Release() { return gx_surface_release(this); }
GXRESULT GXSurface::AddAttachedSurface(GXSurface* inSurface) { return gx_surface_add_attached(this, inSurface); }
GXRESULT GXSurface::GetAttachedSurface(GXSCAPS* inCaps, GXSurface** outSurface) { return gx_surface_get_attached(this, inCaps, reinterpret_cast<void**>(outSurface)); }
GXRESULT GXSurface::Blt(RECT* inDestination, GXSurface* inSource, RECT* inSourceRect, unsigned long inFlags, GXBLTFX* inFX) { return gx_surface_blt(this, inDestination, inSource, inSourceRect, inFlags, inFX); }
GXRESULT GXSurface::Flip(GXSurface* inOverride, unsigned long inFlags) { return gx_surface_flip(this, inFlags); }
GXRESULT GXSurface::Lock(RECT* inRect, GXSURFACEDESC* outDescription, unsigned long inFlags, HANDLE inEvent) { return gx_surface_lock(this, inRect, outDescription, inFlags); }
GXRESULT GXSurface::Unlock(RECT* inRect) { return gx_surface_unlock(this); }
GXRESULT GXSurface::SetColorKey(unsigned long inFlags, GXCOLORKEY* inKey) { return gx_surface_set_color_key(this, inKey); }
float GXSurface::GetRenderScale() const { return gx_surface_render_scale(const_cast<GXSurface*>(this)); }
GXRESULT GXSurface::DrawText(const RECT& inBounds, TextCallback inDraw, void* inContext) { return gx_surface_draw_text(this, &inBounds, inDraw, inContext); }

// ---- GXVertexBuffer

unsigned long GXVertexBuffer::AddRef() { return gx_vertex_buffer_add_ref(this); }
unsigned long GXVertexBuffer::Release() { return gx_vertex_buffer_release(this); }
GXRESULT GXVertexBuffer::Lock(unsigned long inFlags, void** outData, unsigned long* outSize) { return gx_vertex_buffer_lock(this, outData, outSize); }
GXRESULT GXVertexBuffer::Unlock() { return GX_OK; }

// ---- GXDevice

unsigned long GXDevice::AddRef() { return gx_device_add_ref(this); }
unsigned long GXDevice::Release() { return gx_device_release(this); }
GXRESULT GXDevice::GetCaps(GXDEVICEDESC* outCaps) { return gx_device_get_caps(outCaps); }
GXRESULT GXDevice::BeginScene() { return gx_device_begin_scene(); }
GXRESULT GXDevice::EndScene() { return GX_OK; }
GXRESULT GXDevice::SetRenderTarget(GXSurface* inTarget, unsigned long inFlags) { return gx_device_set_render_target(this, inTarget); }
GXRESULT GXDevice::SetViewport(GXVIEWPORT* inViewport) { return gx_device_set_viewport(this, inViewport); }
GXRESULT GXDevice::GetViewport(GXVIEWPORT* outViewport) { return gx_device_get_viewport(this, outViewport); }
GXRESULT GXDevice::Clear(unsigned long inCount, GXRECT* inRects, unsigned long inFlags, unsigned long inColor, float inZ, unsigned long inStencil) { return gx_device_clear(this, inCount, inRects, inFlags, inColor, inZ); }
GXRESULT GXDevice::SetTransform(GXTRANSFORMSTATETYPE inState, GXMATRIX* inMatrix) { return gx_device_set_transform(this, inState, inMatrix); }
GXRESULT GXDevice::SetRenderState(GXRENDERSTATETYPE inState, unsigned long inValue) { return gx_device_set_render_state(this, inState, inValue); }
GXRESULT GXDevice::SetTextureStageState(unsigned long inStage, GXTEXTURESTAGESTATETYPE inState, unsigned long inValue) { return gx_device_set_stage_state(this, inStage, inState, inValue); }
GXRESULT GXDevice::SetTexture(unsigned long inStage, GXSurface* inTexture) { return gx_device_set_texture(this, inStage, inTexture); }
GXRESULT GXDevice::SetMaterial(GXMATERIAL* inMaterial) { return gx_device_set_material(this, inMaterial); }
GXRESULT GXDevice::SetLight(unsigned long inIndex, GXLIGHT* inLight) { return gx_device_set_light(this, inIndex, inLight); }
GXRESULT GXDevice::GetLight(unsigned long inIndex, GXLIGHT* outLight) { return gx_device_get_light(this, inIndex, outLight); }
GXRESULT GXDevice::LightEnable(unsigned long inIndex, BOOL inEnable) { return gx_device_light_enable(this, inIndex, inEnable); }
GXRESULT GXDevice::ValidateDevice(unsigned long* outPasses) { *outPasses = 1; return GX_OK; }
GXRESULT GXDevice::DrawIndexedPrimitiveVB(GXPRIMITIVETYPE inType, GXVertexBuffer* inBuffer, unsigned long inStart, unsigned long inCount, unsigned short* inIndices, unsigned long inIndexCount, unsigned long inFlags) { return gx_device_draw_indexed_primitive_vb(this, inType, inBuffer, inStart, inCount, inIndices, inIndexCount); }

// ---- the display settings

void GXSetRenderPercent(unsigned long inPercent) { gx_set_render_percent(inPercent); }
void GXSetFrameLimit(long inFramesPerSecond) { gx_set_frame_limit(inFramesPerSecond); }
void GXSetTextureFiltering(unsigned long inMode) { gx_set_texture_filtering(inMode); }
void GXSetBrightness(float inGamma) { gx_set_brightness(inGamma); }
void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight) { gx_game_size(inPercent, inWindowed, outWidth, outHeight); }
unsigned long GXAutoUIScale() { return gx_auto_ui_scale(); }
