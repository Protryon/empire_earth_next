// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Rasterizer.cpp
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x100091aa..0x1000d0fc: the U2D key init chains, the DirectDraw/Direct3D enumeration callbacks, DX7Rasterizer's
// ctor and dtor, then its members in alphabetical order, then the vector<DX7RScreenState*> COMDATs and the
// template-static init chain head. The enumeration callbacks and ActivateRasterizer were built without global
// optimisation (see the pragmas below); everything else is -O1.
#include <windows.h>
#include <stdint.h>
#include <float.h>

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "rust/render/dx7.h"
#include "rust/render/ge.h"
#include "platform/PlatFont.h"
#include "lle/GEBitmap.h"
#include "lle/GEPoint.h"
#include "lle/GERhombus.h"

// DX7Rasterizer is whole in Rust (rust/crates/ee_render/src/dx7/rastobj.rs, R4.3 part 13): the per-screen DX7 records, the
// debug colour, the light slots and every method below; DX7TnLRasterizer is its T&L flavour. The class keeps its vtable
// (the game calls it through GERasterizer) and one member, mDX7; the methods forward, and DX7RasterizerHost makes the
// calls on other objects Rust asks for.

// The calls the Rust rasterizer makes on other objects (EeDx7RastOp), in the original's order.
struct DX7RasterizerHost {
    static intptr_t Call(void* inOwner, EeDx7RastOp inOp, void* inObj, intptr_t a, intptr_t b)
    {
        switch (inOp) {
        case EeDx7RastOp::ScreenNew:
            return (intptr_t) static_cast<GEScreen*>(new DX7Screen(NULL, static_cast<GXDirectDraw*>(inObj),
                                                                   UString(reinterpret_cast<const char*>(a)), (b & 1) != 0, (b & 2) != 0));
        case EeDx7RastOp::ScreenDelete: delete static_cast<DX7Screen*>(static_cast<GEScreen*>(inObj)); return 0;
        case EeDx7RastOp::ScreenDirectDraw: return (intptr_t)(GXDirectDraw*)*static_cast<DX7Screen*>(static_cast<GEScreen*>(inObj));
        case EeDx7RastOp::ScreenIsFullScreen: return static_cast<GEScreen*>(inObj)->GetIsFullScreen();
        case EeDx7RastOp::ScreenCurrentMode: *reinterpret_cast<GEVideoMode*>(a) = static_cast<GEScreen*>(inObj)->GetCurrentMode(); return 0;
        case EeDx7RastOp::ScreenModeCount: return (intptr_t)static_cast<GEScreen*>(inObj)->GetModeCount();
        case EeDx7RastOp::ScreenMode: *reinterpret_cast<GEVideoMode*>(b) = static_cast<GEScreen*>(inObj)->GetMode((unsigned long)a); return 0;
        case EeDx7RastOp::ScreenCreateSurface:
            return static_cast<GEScreen*>(inObj)->CreateSurface(*reinterpret_cast<GESurfaceDescription*>(a), reinterpret_cast<GESurface**>(b));
        case EeDx7RastOp::ScreenSetTextureManager: static_cast<GEScreen*>(inObj)->SetTextureManager(reinterpret_cast<GETextureManager*>(a)); return 0;
        case EeDx7RastOp::ManagerSetScreen: static_cast<GETextureManager*>(inObj)->SetScreen(reinterpret_cast<GEScreen*>(a)); return 0;
        case EeDx7RastOp::SurfaceAttached: return (intptr_t)static_cast<GESurface*>(inObj)->GetAttachedSurface(0);
        case EeDx7RastOp::SurfaceAttach: return static_cast<GESurface*>(inObj)->AttachSurface(reinterpret_cast<GESurface*>(a));
        case EeDx7RastOp::SurfaceReference: static_cast<GESurface*>(inObj)->Reference(); return 0;
        case EeDx7RastOp::SurfaceDereference: static_cast<GESurface*>(inObj)->Dereference(); return 0;
        case EeDx7RastOp::SurfaceGx: return (intptr_t)(GXSurface*)*static_cast<DX7Surface*>(static_cast<GESurface*>(inObj));
        case EeDx7RastOp::SurfaceGxChecked: return (intptr_t)(GXSurface*)*dynamic_cast<DX7Surface*>(static_cast<GESurface*>(inObj));
        case EeDx7RastOp::SurfaceWidth: return (intptr_t)static_cast<GESurface*>(inObj)->GetWidth();
        case EeDx7RastOp::SurfaceHeight: return (intptr_t)static_cast<GESurface*>(inObj)->GetHeight();
        case EeDx7RastOp::SurfaceFlip: return static_cast<GESurface*>(inObj)->Flip(a != 0);
        case EeDx7RastOp::TextureHasChanged: return static_cast<GETexture*>(inObj)->HasChanged();
        case EeDx7RastOp::TextureReference: static_cast<GETexture*>(inObj)->Reference(); return 0;
        case EeDx7RastOp::TextureDereference: static_cast<GETexture*>(inObj)->Dereference(); return 0;
        case EeDx7RastOp::TexturePromote: static_cast<GETexture*>(inObj)->Promote(); return 0;
        case EeDx7RastOp::TextureStage: return (intptr_t)static_cast<GETexture*>(inObj)->GetStage();
        case EeDx7RastOp::TextureStageSurface:
            return (intptr_t)(GXSurface*)*static_cast<DX7Surface*>(static_cast<GETexture*>(inObj)->GetStageTexture(0)->GetFrameSurface());
        case EeDx7RastOp::TextureValidate: static_cast<GETexture*>(inObj)->ValidateChanges(); return 0;
        case EeDx7RastOp::WindowOrigin: *reinterpret_cast<GE2DPoint*>(a) = static_cast<GEWindow*>(inObj)->GetClientOrigin(); return 0;
        case EeDx7RastOp::LockEnter: static_cast<TSCriticalSection*>(inObj)->Enter(); return 0;
        case EeDx7RastOp::LockLeave: static_cast<TSCriticalSection*>(inObj)->Leave(); return 0;
        case EeDx7RastOp::Time: return (intptr_t)TMGetTime();
        case EeDx7RastOp::Verify: ESVERIFY_AT(a != 0, (unsigned long)b); return 0;
        default: return 0;
        }
    }
};


// Creates the DirectX 7 rasterizer with the given name and a dark blue debug colour (inTnL: the T&L rasterizer)
DX7Rasterizer::DX7Rasterizer(const UString& inName, bool inTnL)
    : GERasterizer(inName)
{
    this->mDX7 = ee_dx7r_new(&DX7RasterizerHost::Call, this, this->mRust, inTnL);
    ee_ge_draw2d_direct(static_cast<GERasterizer*>(this), this->mDX7);   // the 2D draw list draws on it in Rust
}

// Destroys the rasterizer (the DX7 records; the base's in the base)
DX7Rasterizer::~DX7Rasterizer()
{
    ee_ge_draw2d_direct(static_cast<GERasterizer*>(this), NULL);
    ee_dx7r_destroy(this->mDX7);
}

// The members of GERasterizer that need a complete GE type (the rest are inline in lle/GERasterizer.h)
long GERasterizer::ActivateScreen(unsigned long inScreen, GETextureManager* inManager, GEWindow* inWindow,
                                  unsigned long inBackBuffers, bool inZBuffer, unsigned long inZBufferFormat)
{
    return ee_dx7r_activate_screen(this->mDX7, inScreen, inManager, inWindow, inWindow->GetWidth(), inWindow->GetHeight(), inBackBuffers,
                                   inZBuffer, inZBufferFormat);
}

GEPolygonArray* GERasterizer::CreatePolygonArray(unsigned long inCount) { return new GEPolygonArray(inCount); }

GEVertexArray* GERasterizer::CreateVertexArray(unsigned long inType, unsigned long inCount)
{
    return inType <= 1 ? new GEVertexArray(inType, inCount) : NULL;
}

long GERasterizer::Draw(GEBitmap* inBitmap, unsigned long inScreen) { return ee_dx7r_draw_bitmap(this->mDX7, inScreen, inBitmap->GetRust()); }

// the primitives are their Rust objects (one pointer each: the draws walk the C++ arrays, ee_dx7 draw_ffi.rs)
static_assert(sizeof(GELine) == sizeof(void*) && sizeof(GEPoint) == sizeof(void*) && sizeof(GERhombus) == sizeof(void*),
              "ee_ge prim.rs");

long GERasterizer::Draw(GEText* inText, GESurface* inTarget) { return ee_dx7r_draw_text(inText, NULL, *static_cast<DX7Surface*>(inTarget)); }

long GERasterizer::Draw(GEText* inText, const GERectangle& inRectangle, GESurface* inTarget)
{
    return inText->GetFont()->GetFontBitmap() != NULL
               ? inText->GetFont()->GetFontBitmap()->Draw(inText, inRectangle, this, 0)
               : ee_dx7r_draw_text_clipped(inText, reinterpret_cast<const float(*)[4]>(&inRectangle), *static_cast<DX7Surface*>(inTarget));
}

long GERasterizer::Draw(GEPolygonArray& inPolygons, unsigned long inCount, GEVertexArray& inVertices,
                        unsigned long inVertexCount, GETexture* inTexture, unsigned long inScreen, bool inSorted)
{
    return ee_dx7r_draw_polygon_array(this->mDX7, inScreen, inPolygons.GetIndices(), inCount, inVertices.GetType(),
                                      inVertices.GetVertices(), inVertexCount, inTexture);
}

static_assert(sizeof(std::pair<GE3DPoint, float>) == 16, "ee_dx7/src/draw_model.rs draw_3d_line: (x, y, z, advance)");
static_assert(sizeof(std::pair<U2DPoint<long>, U2DPoint<long> >) == 4 * sizeof(long), "ee_dx7 draw_terrain grid lines");

long GERasterizer::Draw(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    return ee_dx7r_draw_terrain(this->mDX7, inScreen, inMesh.GetRust(), inOrthographic);
}

// ---- the screens, surfaces and movies (DX7Trampolines.cpp, folded here in R4.3 part 17): their objects' hosts and
// constructors, and the layouts Rust fills

#include "lle/LowLevelEngine.h"
#include <stddef.h>
extern "C" void ee_dx7cpp_record_native_error(long inError) { ESRecordNativeError((unsigned long)inError); }

// --- DX7Screen and DX7Surface: whole in Rust (ee_dx7 scrobj.rs, surfobj.rs); the calls they make on C++ objects ------
#include "platform/PlatWindow.h"
#include "platform/PlatSim.h"
static_assert(offsetof(GERectangle, mBottom) == 0 && offsetof(GERectangle, mLeft) == 4 && offsetof(GERectangle, mRight) == 8 &&
              offsetof(GERectangle, mTop) == 12 && sizeof(GEColor) == 12, "ee_dx7/src/surfobj.rs ge_rect");

static intptr_t DX7ObjectHost(EeDx7ObjOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    case EeDx7ObjOp::NameNew: return (intptr_t) new UString(*static_cast<const UString*>(inObj));
    case EeDx7ObjOp::NameDelete: delete static_cast<UString*>(inObj); return 0;
    case EeDx7ObjOp::SurfaceNew:
        return (intptr_t) static_cast<GESurface*>(new DX7Surface(static_cast<GXSurface*>(inObj), *reinterpret_cast<const GESurfaceDescription*>(a),
                                                                 reinterpret_cast<DX7Screen*>(b)));
    case EeDx7ObjOp::SurfaceObject: return (intptr_t) static_cast<DX7Surface*>(static_cast<GESurface*>(inObj))->mDX7;
    case EeDx7ObjOp::SurfaceReference: static_cast<GESurface*>(inObj)->Reference(); return 0;
    case EeDx7ObjOp::SurfaceDereference: static_cast<GESurface*>(inObj)->Dereference(); return 0;
    case EeDx7ObjOp::SurfaceAllocatedSize: return (intptr_t)static_cast<GESurface*>(inObj)->GetAllocatedSize();
    case EeDx7ObjOp::SurfaceMetrics:
    {   // ee_dx7/src/surface.rs LogicalMetrics: logical width and height set, pixel width and height, logical shift
        const uint32_t theMetrics[5] = { (uint32_t)static_cast<GESurface*>(inObj)->GetLogicalWidthSet(), (uint32_t)static_cast<GESurface*>(inObj)->GetLogicalHeightSet(),
                                         (uint32_t)static_cast<GESurface*>(inObj)->GetPixelWidth(), (uint32_t)static_cast<GESurface*>(inObj)->GetPixelHeight(),
                                         (uint32_t)static_cast<GESurface*>(inObj)->GetLogicalShift() };
        memcpy(reinterpret_cast<void*>(a), theMetrics, sizeof theMetrics);
        return 0;
    }
    case EeDx7ObjOp::ProbeWindow: {   // Test32BitSupport's hidden 16 x 16 window
        UString theClassName("TestWindowClass"), theTitle("");
        return static_cast<DX7Screen*>(inObj)->CreateAWindow(GE2DPoint(0.0f, 0.0f), 16, 16, theClassName, theTitle,
                                                             *reinterpret_cast<GEWindow**>(a), 2, NULL);
    }
    case EeDx7ObjOp::WindowOs: return (intptr_t) static_cast<GEWindow*>(inObj)->GetOSRepresentation();
    case EeDx7ObjOp::WindowDelete: delete static_cast<GEWindow*>(inObj); return 0;
    case EeDx7ObjOp::DispatchEvents: TSThread::DispatchWindowEvents(); return 0;
    default: return 0;
    }
}

// Creates a screen for a DirectDraw device: lists its display modes, finds the current one, probes 32-bit support and
// registers it (the original's private ctor 0x1000d271; the rasterizer makes the screens while it enumerates)
DX7Screen::DX7Screen(void* inMonitor, GXDirectDraw* inDirectDraw, const UString& inName, bool inAttachedToDesktop,
                     bool inSharesPrimary)
{
    this->mDX7 = ee_dx7sc_new(DX7ObjectHost, this, inMonitor, inDirectDraw, &inName, inAttachedToDesktop, inSharesPrimary);
    ee_dx7sc_start(this->mDX7);
}

// CreateAWindow's C++ (ee_dx7 scrobj.rs create_window): the window object and the failure's debug line
static void* DX7NewWindow(void* inWindow, unsigned long inWidth, unsigned long inHeight, unsigned long inStyle)
    { return new GEWindow(static_cast<PlatWindow*>(inWindow), inWidth, inHeight, inStyle, true); }
static void DX7WindowError(const char* inError) { ESDebugOut("CreateAWindow: %s\n", inError); }

// Creates a window (a platform window: platform/PlatWindow.h): style 1 is a framed, resizable window with an
// inWidth x inHeight client area, else a topmost borderless one at inOrigin (the original's popup), which is the full
// screen (borderless, no mode change) when it covers the display. The class name and the parent are unused. (0x1000d622)
long DX7Screen::CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                              UString& inClassName, UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                              GEWindow* inParent)
{
    return ee_dx7sc_create_window(gPlatSimMode != 0, inOrigin.GetX(), inOrigin.GetY(), inWidth, inHeight, inTitle, inStyle,
                                  reinterpret_cast<void**>(&outWindow), DX7NewWindow, DX7WindowError);
}

// Wraps a GX surface with its description and parent screen, unlocked and with no attachments (0x1000eced)
DX7Surface::DX7Surface(GXSurface* inSurface, const GESurfaceDescription& inDescription, GEScreen* inParentScreen)
{
    this->mDX7 = ee_dx7sf_new(DX7ObjectHost, static_cast<GESurface*>(this), inSurface,
                              reinterpret_cast<const EeDx7SurfaceDescription*>(&inDescription), inParentScreen);
}

// ---- the renderer the game links (DX7HRTnLDisplay.cpp's globals; its PIM entry points went with the plug-in boundary)
static UString gName("Direct3D Hardware TnL");
static DX7Rasterizer gTnLRasterizer(gName, true);   // the T&L flavour (rastobj.rs)

// The game's rasterizer (GEInitialize adds it when it finds a screen)
GERasterizer* DX7TnLRasterizer()
{
    return &gTnLRasterizer;
}
