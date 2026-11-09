// The C++ side of the rasterizer's draw paths in Rust (rust/crates/ee_dx7/src/draw_ffi.rs): the DX7Rasterizer services
// every draw step reaches back into. New unit, no original address (tools/units.py DX7_TRAMPOLINES).
#include <windows.h>
#include <stdint.h>

#include "rust/ee_rust.h"
#include "dx7/DX7DrawSeam.h"

static DX7DrawAccess* DX7Access(void* inRasterizer) { return static_cast<DX7DrawAccess*>(static_cast<DX7Rasterizer*>(inRasterizer)); }

extern "C" {
void ee_dx7cpp_enter(void* inRasterizer) { DX7Access(inRasterizer)->mDrawingAccess->Enter(); }
void ee_dx7cpp_leave(void* inRasterizer) { DX7Access(inRasterizer)->mDrawingAccess->Leave(); }
void ee_dx7cpp_set_2d_mode(void* inRasterizer, unsigned long inScreen) { DX7Access(inRasterizer)->Set2DMode(inScreen); }
void ee_dx7cpp_set_3d_mode(void* inRasterizer, unsigned long inScreen) { DX7Access(inRasterizer)->Set3DMode(inScreen); }
void ee_dx7cpp_set_texture(void* inRasterizer, void* inTexture, unsigned long inScreen)
{
    DX7Access(inRasterizer)->SetTexture(static_cast<GETexture*>(inTexture), inScreen);
}
void ee_dx7cpp_set_shading_color_source(void* inRasterizer, unsigned long inSource, unsigned long inScreen)
{
    DX7Access(inRasterizer)->SetShadingColorSource(inSource, inScreen);
}
Dx7StdColorVertex* ee_dx7cpp_lock_vb(void* inRasterizer, unsigned long inCount, unsigned long inScreen)
{
    GXVERTEX_STD_COLOR* theBuffer;
    DX7Access(inRasterizer)->LockVB(inCount, inScreen, theBuffer);
    return reinterpret_cast<Dx7StdColorVertex*>(theBuffer);
}
long ee_dx7cpp_unlock_vb(void* inRasterizer, unsigned long inScreen) { return DX7Access(inRasterizer)->UnlockVB(inScreen); }
void ee_dx7cpp_add_triangles(void* inRasterizer, unsigned long inScreen, unsigned long inCount)
{
    DX7Access(inRasterizer)->mScreenStates[inScreen]->m32C[1] += inCount;
}
}
