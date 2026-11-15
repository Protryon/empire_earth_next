// The DX7 renderer's trampolines into Rust (rust/crates/ee_dx7, docs/RUST_PLAN.md R4.2): each class's bodies forward to
// `extern "C"` functions with the opaque Rust object. New unit, no original address (tools/units.py DX7_TRAMPOLINES).
#include <windows.h>
#include <stdint.h>

#include "rust/ee_dx7.h"
#include "lle/LowLevelEngine.h"
#include "lle/Graphics Engine.h"
#include "lle/GEColor.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GETextureCoordinate.h"
#include "dx7/DX7PolygonArray.h"
#include "dx7/DX7StandardVertexArray.h"
#include "dx7/DX7StdColorVertexArray.h"

// The value types the arrays read and write, as Rust's float arrays (GEColor r,g,b; GE3DPoint and GEVector x,y,z;
// GETextureCoordinate u,v: their members in order)
static_assert(sizeof(GEColor) == 3 * sizeof(float) && sizeof(GE3DPoint) == 3 * sizeof(float) &&
              sizeof(GEVector) == 3 * sizeof(float) && sizeof(GETextureCoordinate) == 2 * sizeof(float), "ee_dx7/src/vertex.rs");
static float* DX7Floats(const void* inValue) { return reinterpret_cast<float*>(const_cast<void*>(inValue)); }

// --- DX7PolygonArray -------------------------------------------------------------------------------------------------

DX7PolygonArray::DX7PolygonArray(unsigned long inSize)
{
    this->mRust = ee_dx7_poly_new(inSize);
    this->mIndices = ee_dx7_poly_indices(this->mRust);
    this->mIndexCount = inSize * 3;
    this->mSize = inSize;
}

DX7PolygonArray::~DX7PolygonArray() { ee_dx7_poly_destroy(this->mRust); }
unsigned long DX7PolygonArray::GetSize() const { return ee_dx7_poly_size(this->mRust); }
unsigned long DX7PolygonArray::GetVertexIndex1(unsigned long inPolygon) const { return ee_dx7_poly_vertex_index(this->mRust, inPolygon, 0); }
unsigned long DX7PolygonArray::GetVertexIndex2(unsigned long inPolygon) const { return ee_dx7_poly_vertex_index(this->mRust, inPolygon, 1); }
unsigned long DX7PolygonArray::GetVertexIndex3(unsigned long inPolygon) const { return ee_dx7_poly_vertex_index(this->mRust, inPolygon, 2); }
void DX7PolygonArray::SetVertexIndex1(unsigned long inPolygon, unsigned long inIndex) { ee_dx7_poly_set_vertex_index(this->mRust, inPolygon, 0, inIndex); }
void DX7PolygonArray::SetVertexIndex2(unsigned long inPolygon, unsigned long inIndex) { ee_dx7_poly_set_vertex_index(this->mRust, inPolygon, 1, inIndex); }
void DX7PolygonArray::SetVertexIndex3(unsigned long inPolygon, unsigned long inIndex) { ee_dx7_poly_set_vertex_index(this->mRust, inPolygon, 2, inIndex); }
void DX7PolygonArray::SetVertexIndices(unsigned long inPolygon, unsigned long inIndex1, unsigned long inIndex2, unsigned long inIndex3)
{
    ee_dx7_poly_set_vertex_indices(this->mRust, inPolygon, inIndex1, inIndex2, inIndex3);
}
unsigned long DX7PolygonArray::SizeOf() const { return ee_dx7_poly_size_of(this->mRust); }

// --- DX7StandardVertexArray ------------------------------------------------------------------------------------------

DX7StandardVertexArray::DX7StandardVertexArray(unsigned long inSize)
{
    static_assert(sizeof(GXVERTEX) == sizeof(Dx7Vertex), "ee_dx7/src/vertex.rs");
    this->mRust = ee_dx7_std_va_new(inSize);
    this->mVertices = reinterpret_cast<GXVERTEX*>(ee_dx7_std_va_vertices(this->mRust));
    this->mSize = inSize;
}

DX7StandardVertexArray::~DX7StandardVertexArray() { ee_dx7_std_va_destroy(this->mRust); }
void DX7StandardVertexArray::CopyFrom(const GEVertexArray& inArray)
{
    ee_dx7_std_va_copy_from(this->mRust, static_cast<const DX7StandardVertexArray&>(inArray).mRust);
}
void DX7StandardVertexArray::GetColor(GEColor& outColor, unsigned long inIndex) const { ee_dx7_std_va_color(this->mRust, inIndex, DX7Floats(&outColor)); }
void DX7StandardVertexArray::GetNormal(GEVector& outNormal, unsigned long inIndex) const { ee_dx7_std_va_normal(this->mRust, inIndex, DX7Floats(&outNormal)); }
float DX7StandardVertexArray::GetOpacity(unsigned long inIndex) const { return ee_dx7_std_va_opacity(this->mRust, inIndex); }
void DX7StandardVertexArray::GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const { ee_dx7_std_va_point(this->mRust, inIndex, DX7Floats(&outPoint)); }
unsigned long DX7StandardVertexArray::GetSize() const { return ee_dx7_std_va_size(this->mRust); }
void DX7StandardVertexArray::GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const
{
    ee_dx7_std_va_texture_coordinate(this->mRust, inIndex, DX7Floats(&outCoordinate));
}
unsigned long DX7StandardVertexArray::GetType() const { return 0; }
void DX7StandardVertexArray::SetColor(const GEColor&, unsigned long) {}
void DX7StandardVertexArray::SetOpacity(float, unsigned long) {}
void DX7StandardVertexArray::SetNormal(const GEVector& inNormal, unsigned long inIndex) { ee_dx7_std_va_set_normal(this->mRust, DX7Floats(&inNormal), inIndex); }
void DX7StandardVertexArray::SetPoint(const GE3DPoint& inPoint, unsigned long inIndex) { ee_dx7_std_va_set_point(this->mRust, DX7Floats(&inPoint), inIndex); }
void DX7StandardVertexArray::SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex)
{
    ee_dx7_std_va_set_texture_coordinate(this->mRust, DX7Floats(&inCoordinate), inIndex);
}
void DX7StandardVertexArray::SetVertex(const GEColor&, const GE3DPoint& inPoint, const GEVector& inNormal,
                                       const GETextureCoordinate& inCoordinate, float, unsigned long inIndex)
{
    ee_dx7_std_va_set_vertex(this->mRust, DX7Floats(&inPoint), DX7Floats(&inNormal), DX7Floats(&inCoordinate), inIndex);
}
unsigned long DX7StandardVertexArray::SizeOf() const { return ee_dx7_std_va_size_of(this->mRust); }

// --- DX7StdColorVertexArray ------------------------------------------------------------------------------------------

DX7StdColorVertexArray::DX7StdColorVertexArray(unsigned long inSize)
{
    static_assert(sizeof(GXVERTEX_STD_COLOR) == sizeof(Dx7StdColorVertex), "ee_dx7/src/vertex.rs");
    this->mRust = ee_dx7_color_va_new(inSize);
    this->mVertices = reinterpret_cast<GXVERTEX_STD_COLOR*>(ee_dx7_color_va_vertices(this->mRust));
    this->mSize = inSize;
}

DX7StdColorVertexArray::~DX7StdColorVertexArray() { ee_dx7_color_va_destroy(this->mRust); }
void DX7StdColorVertexArray::CopyFrom(const GEVertexArray& inArray)
{
    ee_dx7_color_va_copy_from(this->mRust, static_cast<const DX7StdColorVertexArray&>(inArray).mRust);
}
void DX7StdColorVertexArray::GetColor(GEColor& outColor, unsigned long inIndex) const { ee_dx7_color_va_color(this->mRust, inIndex, DX7Floats(&outColor)); }
void DX7StdColorVertexArray::GetNormal(GEVector& outNormal, unsigned long inIndex) const { ee_dx7_color_va_normal(this->mRust, inIndex, DX7Floats(&outNormal)); }
float DX7StdColorVertexArray::GetOpacity(unsigned long inIndex) const { return ee_dx7_color_va_opacity(this->mRust, inIndex); }
void DX7StdColorVertexArray::GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const { ee_dx7_color_va_point(this->mRust, inIndex, DX7Floats(&outPoint)); }
unsigned long DX7StdColorVertexArray::GetSize() const { return ee_dx7_color_va_size(this->mRust); }
void DX7StdColorVertexArray::GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const
{
    ee_dx7_color_va_texture_coordinate(this->mRust, inIndex, DX7Floats(&outCoordinate));
}
unsigned long DX7StdColorVertexArray::GetType() const { return 1; }
void DX7StdColorVertexArray::SetColor(const GEColor& inColor, unsigned long inIndex) { ee_dx7_color_va_set_color(this->mRust, DX7Floats(&inColor), inIndex); }
void DX7StdColorVertexArray::SetNormal(const GEVector& inNormal, unsigned long inIndex) { ee_dx7_color_va_set_normal(this->mRust, DX7Floats(&inNormal), inIndex); }
void DX7StdColorVertexArray::SetOpacity(float inOpacity, unsigned long inIndex) { ee_dx7_color_va_set_opacity(this->mRust, inOpacity, inIndex); }
void DX7StdColorVertexArray::SetPoint(const GE3DPoint& inPoint, unsigned long inIndex) { ee_dx7_color_va_set_point(this->mRust, DX7Floats(&inPoint), inIndex); }
void DX7StdColorVertexArray::SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex)
{
    ee_dx7_color_va_set_texture_coordinate(this->mRust, DX7Floats(&inCoordinate), inIndex);
}
void DX7StdColorVertexArray::SetVertex(const GEColor& inColor, const GE3DPoint& inPoint, const GEVector& inNormal,
                                       const GETextureCoordinate& inCoordinate, float inOpacity, unsigned long inIndex)
{
    ee_dx7_color_va_set_vertex(this->mRust, DX7Floats(&inColor), DX7Floats(&inPoint), DX7Floats(&inNormal),
                               DX7Floats(&inCoordinate), inOpacity, inIndex);
}
unsigned long DX7StdColorVertexArray::SizeOf() const { return ee_dx7_color_va_size_of(this->mRust); }

#include <stddef.h>
#include "dx7/DX7Rasterizer.h"
#include "lle/GESurface.h"
#define DX7_SAME_OFFSET(cpp, rust, field) static_assert(offsetof(cpp, field) == offsetof(rust, field), #field)

// --- GESurfaceDescription: filled by Rust (ee_dx7/src/api.rs SurfaceDescription) --------------------------------------
static_assert(sizeof(GESurfaceDescription) == sizeof(EeDx7SurfaceDescription), "ee_dx7/src/api.rs SurfaceDescription");
#include "lle/GEScreen.h"
static_assert(sizeof(GEVideoMode) == sizeof(EeDx7VideoMode), "ee_dx7/src/screen.rs VideoMode");
DX7_SAME_OFFSET(GEVideoMode, EeDx7VideoMode, mWidth);
DX7_SAME_OFFSET(GEVideoMode, EeDx7VideoMode, mHeight);
DX7_SAME_OFFSET(GEVideoMode, EeDx7VideoMode, mRefreshRate);
DX7_SAME_OFFSET(GEVideoMode, EeDx7VideoMode, mBitCount);
DX7_SAME_OFFSET(GEVideoMode, EeDx7VideoMode, mPixelFormat);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mType);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mPixelFormat);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mSurfaceVolatility);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mWidth);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mHeight);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, m14);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mMipLevelCount);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mPixelDepth);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mIs3DRenderable);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mIsTransparencyEnabled);
DX7_SAME_OFFSET(GESurfaceDescription, EeDx7SurfaceDescription, mNoAlpha);

extern "C" void ee_dx7cpp_record_native_error(long inError) { ESRecordNativeError((unsigned long)inError); }

// --- DX7Screen and DX7Surface: whole in Rust (ee_dx7 scrobj.rs, surfobj.rs); the calls they make on C++ objects ------
#include "dx7/DX7Screen.h"
#include "dx7/DX7Surface.h"
#include "GEWindow.h"
#include "lle/UString.h"
#include "platform/PlatWindow.h"
#include "platform/PlatSim.h"
static_assert(offsetof(GERectangle, mBottom) == 0 && offsetof(GERectangle, mLeft) == 4 && offsetof(GERectangle, mRight) == 8 &&
              offsetof(GERectangle, mTop) == 12 && sizeof(GEColor) == 12, "ee_dx7/src/surfobj.rs ge_rect");

static intptr_t DX7ObjectHost(EeDx7ObjOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    GESurface* theSurface = static_cast<GESurface*>(inObj);
    switch (inOp) {
    case EeDx7ObjOp::NameNew: return (intptr_t) new UString(*static_cast<const UString*>(inObj));
    case EeDx7ObjOp::NameDelete: delete static_cast<UString*>(inObj); return 0;
    case EeDx7ObjOp::SurfaceNew:
        return (intptr_t) static_cast<GESurface*>(new DX7Surface(static_cast<GXSurface*>(inObj), *reinterpret_cast<const GESurfaceDescription*>(a),
                                                                 reinterpret_cast<DX7Screen*>(b)));
    case EeDx7ObjOp::SurfaceObject: return (intptr_t) static_cast<DX7Surface*>(theSurface)->mDX7;
    case EeDx7ObjOp::SurfaceReference: theSurface->Reference(); return 0;
    case EeDx7ObjOp::SurfaceDereference: theSurface->Dereference(); return 0;
    case EeDx7ObjOp::SurfaceAllocatedSize: return (intptr_t)theSurface->GetAllocatedSize();
    case EeDx7ObjOp::SurfaceMetrics:
    {   // ee_dx7/src/surface.rs LogicalMetrics: logical width and height set, pixel width and height, logical shift
        const uint32_t theMetrics[5] = { (uint32_t)theSurface->GetLogicalWidthSet(), (uint32_t)theSurface->GetLogicalHeightSet(),
                                         (uint32_t)theSurface->GetPixelWidth(), (uint32_t)theSurface->GetPixelHeight(),
                                         (uint32_t)theSurface->GetLogicalShift() };
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

// Creates a window (a platform window: platform/PlatWindow.h): style 1 is a framed, resizable window with an
// inWidth x inHeight client area, else a topmost borderless one at inOrigin (the original's popup), which is the full
// screen (borderless, no mode change) when it covers the display. The class name and the parent are unused. (0x1000d622)
long DX7Screen::CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                              UString& inClassName, UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                              GEWindow* inParent)
{
    int32_t theDisplayWidth = 0, theDisplayHeight = 0;
    // window sizes are in the display's own units (macOS: points), not its pixels
    PlatWindowGetDisplayLogicalSize(NULL, &theDisplayWidth, &theDisplayHeight);
    EeDx7WindowParams theParams;
    ee_dx7sc_window_params(gPlatSimMode != 0, inStyle, (int32_t)inWidth, (int32_t)inHeight, theDisplayWidth, theDisplayHeight,
                           inOrigin.GetX(), inOrigin.GetY(), &theParams);
    PlatWindow* theWindow = theParams.create_at
        ? PlatWindowCreateAt(inTitle, theParams.x, theParams.y, theParams.width, theParams.height, theParams.flags)
        : PlatWindowCreate(inTitle, theParams.width, theParams.height, theParams.flags);
    if (theWindow == NULL) {
        ESDebugOut("CreateAWindow: %s\n", PlatWindowLastError());
        ESRecordNativeError(PlatGetLastError());
        ESGetNativeError();
        return kErrorNativeError;
    }
    PlatCursorShow(0);   // the game draws its own pointer over its windows (WM_SETCURSOR's SetCursor(NULL))
    TSThread::DispatchWindowEvents();   // the messages CreateWindowEx sent (the window is not the game's yet)
    outWindow = new GEWindow(theWindow, inWidth, inHeight, inStyle, true);
    return kErrorOK;
}

// Wraps a GX surface with its description and parent screen, unlocked and with no attachments (0x1000eced)
DX7Surface::DX7Surface(GXSurface* inSurface, const GESurfaceDescription& inDescription, GEScreen* inParentScreen)
{
    this->mDX7 = ee_dx7sf_new(DX7ObjectHost, static_cast<GESurface*>(this), inSurface,
                              reinterpret_cast<const EeDx7SurfaceDescription*>(&inDescription), inParentScreen);
}
