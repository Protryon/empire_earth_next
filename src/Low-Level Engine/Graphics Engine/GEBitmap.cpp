// C:\EEX\Low-Level Engine\Graphics Engine\GEBitmap.cpp (file name inferred: no __FILE__; alphabetically
// GE3DPoint < GEBitmap < GEBoundingBox, and GEBitmap's own code is the only non-inline code in the range)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1000dd9e..0x10010550. The unit opens with the header-inline members of every class it is the first to include,
// in include order (URandom, GEColor, GESurface, GETextureManager, GETexture, GEWindow, GEScreen, GERectangle,
// GEViewport, GEVertexArray, the U2DPoint/U2DRectangle key statics, GERasterizer, the gone GE2DVertex), then GEBitmap's own
// code (0x1000f3a4) and the map<UString, GETexture*> / vector<GERScreenState*> COMDATs.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GELine.h"
#include "lle/GEMaterial.h"
#include "lle/GEFont.h"
#include "lle/GETextureSet.h"

// The RNG trace hook (the draws are Rust: ee_base random)
extern "C" void (*gURandomTraceHook)(URandom* inRandom) = NULL;   // port, TEST ONLY (lle/URandom.h)


// ---- GEBitmap (0x1000f3a4..). Vertex order: 0 top left, 1 top right, 2 bottom left, 3 bottom right.
//
// port (R4.3, ninth part): the bitmap is a whole Rust object (ee_ge bmpobj.rs): the constructor's quad, the
// destructor's releases, the corners, the stretched coordinates, the flips, the per-vertex values and SetTexture.
// This section is its shell: the forwarders and the host, which makes each call the original made on another
// object (the rasterizer, the vertex array, the textures). Floats cross the host as their bits.

// ---- the vertex host: the calls GEBitmap, GELine, GEPoint and GERhombus (ee_ge bmpobj.rs, prim.rs) make on other objects

static intptr_t GEBitmapFloatBits(float inValue) { return UFloatBits(inValue); }

// (C linkage: ee_ge game_hosts.rs names it)
extern "C" intptr_t GEVertexHost(void*, EeGeBmpOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    // (the active rasterizer, its screen count, a texture's Reference and dropped MIP levels: Rust's own, ee_ge
    // bmpobj.rs direct_op)
    case EeGeBmpOp::RasterVertexArray:
        return (intptr_t)((GERasterizer*)inObj)->CreateVertexArray((unsigned long)a, (unsigned long)b);
    // (the vertex arrays' other calls are on their Rust arrays: ee_ge bmpobj.rs vertex_op)
    case EeGeBmpOp::VertexDelete:
        delete ((GEVertexArray*)inObj);
        return 0;
    case EeGeBmpOp::TexDereference:
        ((GETexture*)inObj)->Dereference();
        return 0;
    case EeGeBmpOp::TexFrameWidth:
        return GEBitmapFloatBits(((GETexture*)inObj)->GetFrameWidth());
    case EeGeBmpOp::TexFrameHeight:
        return GEBitmapFloatBits(((GETexture*)inObj)->GetFrameHeight());
    case EeGeBmpOp::TexWidth:
        return (intptr_t)((GETexture*)inObj)->GetWidth();
    case EeGeBmpOp::TexHeight:
        return (intptr_t)((GETexture*)inObj)->GetHeight();
    }
    return 0;
}
// ---- end of the vertex host

// ---- the layouts Rust reads in place (ee_ge cppobj.rs, R4.3 part 17): a reference-counted object is its vtable
// pointer, its count and its Rust object; a text, a window and a primitive their Rust object alone; a UString and a
// UWideString their vtable pointer and ee_ustring's record
static_assert(sizeof(GEMaterial) == 3 * sizeof(void*) && sizeof(GEFont) == 3 * sizeof(void*) &&
              sizeof(GETexture) == 3 * sizeof(void*) &&
              sizeof(GEBitmap) == 3 * sizeof(void*) && sizeof(GEFontBitmap) == 3 * sizeof(void*), "cppobj.rs CppCounted");
static_assert(sizeof(GEText) == sizeof(void*) && sizeof(GEWindow) == sizeof(void*) && sizeof(GETextureSet) == sizeof(void*) &&
              offsetof(GERasterizer, mRust) == 0 && offsetof(GERasterizer, mDX7) == sizeof(void*), "cppobj.rs plain");
static_assert(sizeof(UString) == sizeof(void*) + sizeof(EeUString) && sizeof(UWideString) == sizeof(void*) + sizeof(EeUWideString),
              "cppobj.rs CppUString");

// ---- the object host: the calls GEMaterial, GEFont, GEText and GEWindow (ee_ge smallobj.rs) make on other objects
extern "C" intptr_t GEObjectHost(void*, EeGeObjOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    case EeGeObjOp::StrNew:
        return (intptr_t)new UString;
    case EeGeObjOp::StrCopy:
        return (intptr_t)new UString(*(const UString*)a);
    case EeGeObjOp::StrDelete:
        delete ((UString*)inObj);
        return 0;
    case EeGeObjOp::WStrNew:
        return (intptr_t)new UWideString((unsigned long)a);
    case EeGeObjOp::WStrDelete:
        delete ((UWideString*)inObj);
        return 0;
    case EeGeObjOp::TexDereference:
        ((GETexture*)inObj)->Dereference();
        return 0;
    case EeGeObjOp::RefDereference:
        ((UReferenceCounter*)inObj)->Dereference();
        return 0;
    // (the platform fonts, the text functions and the platform window's calls: ee_base's, called by the Rust objects
    // themselves)
    }
    return 0;
}

GEWindow::GEWindow(PlatWindow* inWindow, unsigned long inWidth, unsigned long inHeight, unsigned long inDepth,
                   bool inIsFullScreen)
    : mRust(ee_ge_win_new(&GEObjectHost, this, inWindow, inWidth, inHeight, inDepth, inIsFullScreen)) {}
// ---- end of the object host

// (GEBitmap's members forward to its Rust object inline: lle/GEBitmap.h)

// ---- GEFontBitmap (GEFontBitmap.cpp, folded here in R4.3 part 16)

// ---- GEFontBitmap.
//
// port (R4.3, tenth part): the font bitmap is a whole Rust object (ee_ge fbobj.rs): the constructor's glyph rendering,
// the destructor, CreateNewFontTexture, CreateSurfaceAlpha and the two draws. This unit is its shell: the forwarders
// and the host, which makes each call the original made on another object (the rasterizer, the font, the working
// text, the drawing bitmap, the textures, surfaces and screens). Floats cross the host as their bits.


// (the working text's and the drawing bitmap's members and the two draws are the Rust object's own: fbobj.rs
// direct_op, on the 2D draw list)
intptr_t GEFontBitmap::Host(void* inOwner, EeGeFbOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    case EeGeFbOp::TextNew:
        return (intptr_t)new GEText((GEFont*)inObj);
    case EeGeFbOp::TextDelete:
        delete ((GEText*)inObj);
        return 0;
    // (TextSetByte, FontRange, TextOps, RasterScreen, RasterGetBlending, TexSurface, FontSetBitmap and the surfaces' and
    // screen's calls but the logical sizes, RasterSetBlending: the Rust object's own, fbobj.rs)
    case EeGeFbOp::BmpNew:
        return (intptr_t)new GEBitmap;
    case EeGeFbOp::BmpDereference:
        ((GEBitmap*)inObj)->Dereference();
        return 0;
    case EeGeFbOp::SurfWidth: return (intptr_t)((GESurface*)inObj)->GetWidth();
    case EeGeFbOp::SurfHeight: return (intptr_t)((GESurface*)inObj)->GetHeight();
    case EeGeFbOp::VerifySurface: ESVerify(__FILE__, 228, "theResult == keseOK"); return 0;
    case EeGeFbOp::VerifyTexture: ESVerify(__FILE__, 235, "((GETexture*)inObj) != NULL"); return 0;
    default: return 0;
    }
}
