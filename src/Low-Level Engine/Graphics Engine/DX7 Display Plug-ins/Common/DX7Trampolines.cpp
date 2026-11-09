// The DX7 renderer's trampolines into Rust (rust/crates/ee_dx7, docs/RUST_PLAN.md R4.2): each class's bodies forward to
// `extern "C"` functions with the opaque Rust object. New unit, no original address (tools/units.py DX7_TRAMPOLINES).
#include <windows.h>
#include <stdint.h>

#include "rust/ee_rust.h"
#include "lle/LowLevelEngine.h"
#include "lle/Graphics Engine.h"
#include "lle/GEColor.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GETextureCoordinate.h"
#include "dx7/DX7PolygonArray.h"
#include "dx7/DX7StandardVertexArray.h"
#include "dx7/DX7StdColorVertexArray.h"

// --- DX7PolygonArray -------------------------------------------------------------------------------------------------

DX7PolygonArray::DX7PolygonArray(unsigned long inSize)
{
    this->mRust = ee_dx7_poly_new(inSize);
    this->mIndices = ee_dx7_poly_indices(this->mRust);
    this->mIndexCount = inSize * 3;
    this->mSize = inSize;
}

DX7PolygonArray::~DX7PolygonArray()
{
    ee_dx7_poly_destroy(this->mRust);
}

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

DX7StandardVertexArray::~DX7StandardVertexArray()
{
    ee_dx7_std_va_destroy(this->mRust);
}

void DX7StandardVertexArray::CopyFrom(const GEVertexArray& inArray)
{
    ee_dx7_std_va_copy_from(this->mRust, static_cast<const DX7StandardVertexArray&>(inArray).mRust);
}

void DX7StandardVertexArray::GetColor(GEColor& outColor, unsigned long inIndex) const
{
    float c[3];
    ee_dx7_std_va_color(this->mRust, inIndex, c);
    outColor.SetRed(c[0]);
    outColor.SetGreen(c[1]);
    outColor.SetBlue(c[2]);
}

void DX7StandardVertexArray::GetNormal(GEVector& outNormal, unsigned long inIndex) const
{
    float n[3];
    ee_dx7_std_va_normal(this->mRust, inIndex, n);
    outNormal.SetX(n[0]);
    outNormal.SetY(n[1]);
    outNormal.SetZ(n[2]);
}

float DX7StandardVertexArray::GetOpacity(unsigned long inIndex) const { return ee_dx7_std_va_opacity(this->mRust, inIndex); }

void DX7StandardVertexArray::GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const
{
    float p[3];
    ee_dx7_std_va_point(this->mRust, inIndex, p);
    outPoint.SetX(p[0]);
    outPoint.SetY(p[1]);
    outPoint.SetZ(p[2]);
}

unsigned long DX7StandardVertexArray::GetSize() const { return ee_dx7_std_va_size(this->mRust); }

void DX7StandardVertexArray::GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const
{
    float uv[2];
    ee_dx7_std_va_texture_coordinate(this->mRust, inIndex, uv);
    outCoordinate.SetU(uv[0]);
    outCoordinate.SetV(uv[1]);
}

unsigned long DX7StandardVertexArray::GetType() const { return 0; }
void DX7StandardVertexArray::SetColor(const GEColor&, unsigned long) {}
void DX7StandardVertexArray::SetOpacity(float, unsigned long) {}

void DX7StandardVertexArray::SetNormal(const GEVector& inNormal, unsigned long inIndex)
{
    const float n[3] = { inNormal.GetX(), inNormal.GetY(), inNormal.GetZ() };
    ee_dx7_std_va_set_normal(this->mRust, n, inIndex);
}

void DX7StandardVertexArray::SetPoint(const GE3DPoint& inPoint, unsigned long inIndex)
{
    const float p[3] = { inPoint.GetX(), inPoint.GetY(), inPoint.GetZ() };
    ee_dx7_std_va_set_point(this->mRust, p, inIndex);
}

void DX7StandardVertexArray::SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex)
{
    const float uv[2] = { inCoordinate.GetU(), inCoordinate.GetV() };
    ee_dx7_std_va_set_texture_coordinate(this->mRust, uv, inIndex);
}

void DX7StandardVertexArray::SetVertex(const GEColor&, const GE3DPoint& inPoint, const GEVector& inNormal,
                                       const GETextureCoordinate& inCoordinate, float, unsigned long inIndex)
{
    const float p[3] = { inPoint.GetX(), inPoint.GetY(), inPoint.GetZ() };
    const float n[3] = { inNormal.GetX(), inNormal.GetY(), inNormal.GetZ() };
    const float uv[2] = { inCoordinate.GetU(), inCoordinate.GetV() };
    ee_dx7_std_va_set_vertex(this->mRust, p, n, uv, inIndex);
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

DX7StdColorVertexArray::~DX7StdColorVertexArray()
{
    ee_dx7_color_va_destroy(this->mRust);
}

void DX7StdColorVertexArray::CopyFrom(const GEVertexArray& inArray)
{
    ee_dx7_color_va_copy_from(this->mRust, static_cast<const DX7StdColorVertexArray&>(inArray).mRust);
}

void DX7StdColorVertexArray::GetColor(GEColor& outColor, unsigned long inIndex) const
{
    float c[3];
    ee_dx7_color_va_color(this->mRust, inIndex, c);
    outColor.SetRed(c[0]);
    outColor.SetGreen(c[1]);
    outColor.SetBlue(c[2]);
}

void DX7StdColorVertexArray::GetNormal(GEVector& outNormal, unsigned long inIndex) const
{
    float n[3];
    ee_dx7_color_va_normal(this->mRust, inIndex, n);
    outNormal.SetX(n[0]);
    outNormal.SetY(n[1]);
    outNormal.SetZ(n[2]);
}

float DX7StdColorVertexArray::GetOpacity(unsigned long inIndex) const { return ee_dx7_color_va_opacity(this->mRust, inIndex); }

void DX7StdColorVertexArray::GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const
{
    float p[3];
    ee_dx7_color_va_point(this->mRust, inIndex, p);
    outPoint.SetX(p[0]);
    outPoint.SetY(p[1]);
    outPoint.SetZ(p[2]);
}

unsigned long DX7StdColorVertexArray::GetSize() const { return ee_dx7_color_va_size(this->mRust); }

void DX7StdColorVertexArray::GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const
{
    float uv[2];
    ee_dx7_color_va_texture_coordinate(this->mRust, inIndex, uv);
    outCoordinate.SetU(uv[0]);
    outCoordinate.SetV(uv[1]);
}

unsigned long DX7StdColorVertexArray::GetType() const { return 1; }

void DX7StdColorVertexArray::SetColor(const GEColor& inColor, unsigned long inIndex)
{
    const float c[3] = { inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue() };
    ee_dx7_color_va_set_color(this->mRust, c, inIndex);
}

void DX7StdColorVertexArray::SetNormal(const GEVector& inNormal, unsigned long inIndex)
{
    const float n[3] = { inNormal.GetX(), inNormal.GetY(), inNormal.GetZ() };
    ee_dx7_color_va_set_normal(this->mRust, n, inIndex);
}

void DX7StdColorVertexArray::SetOpacity(float inOpacity, unsigned long inIndex)
{
    ee_dx7_color_va_set_opacity(this->mRust, inOpacity, inIndex);
}

void DX7StdColorVertexArray::SetPoint(const GE3DPoint& inPoint, unsigned long inIndex)
{
    const float p[3] = { inPoint.GetX(), inPoint.GetY(), inPoint.GetZ() };
    ee_dx7_color_va_set_point(this->mRust, p, inIndex);
}

void DX7StdColorVertexArray::SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex)
{
    const float uv[2] = { inCoordinate.GetU(), inCoordinate.GetV() };
    ee_dx7_color_va_set_texture_coordinate(this->mRust, uv, inIndex);
}

void DX7StdColorVertexArray::SetVertex(const GEColor& inColor, const GE3DPoint& inPoint, const GEVector& inNormal,
                                       const GETextureCoordinate& inCoordinate, float inOpacity, unsigned long inIndex)
{
    const float c[3] = { inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue() };
    const float p[3] = { inPoint.GetX(), inPoint.GetY(), inPoint.GetZ() };
    const float n[3] = { inNormal.GetX(), inNormal.GetY(), inNormal.GetZ() };
    const float uv[2] = { inCoordinate.GetU(), inCoordinate.GetV() };
    ee_dx7_color_va_set_vertex(this->mRust, c, p, n, uv, inOpacity, inIndex);
}

unsigned long DX7StdColorVertexArray::SizeOf() const { return ee_dx7_color_va_size_of(this->mRust); }

// --- DX7RScreenState: the pinned record the Rust state setters write ---------------------------------------------------
#include <stddef.h>
#include "dx7/DX7Rasterizer.h"
#include "lle/GESurface.h"
#define DX7_SAME_OFFSET(cpp, rust, field) static_assert(offsetof(cpp, field) == offsetof(rust, field), #field)
static_assert(sizeof(GXVIEWPORT) == sizeof(EeDx7Viewport), "ee_dx7/src/state.rs Viewport");
static_assert(sizeof(DX7RTextureStage) == sizeof(EeDx7TextureStageCache), "ee_dx7/src/state.rs TextureStageCache");
static_assert(sizeof(DX7RScreenState) == sizeof(EeDx7ScreenState), "ee_dx7/src/state.rs ScreenState");
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mViewport);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mDirect3D);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mDevice);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mScreen);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mVertexBuffer);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mVertexBufferPosition);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mStage1ColorOperation);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mStage);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mBlendSource);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mBlendDestination);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mZEnable);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mIs3DMode);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mViewportChanged);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mZWriteEnable);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mAlphaBlendEnable);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mCanModulateTextureAlpha);
DX7_SAME_OFFSET(DX7RScreenState, EeDx7ScreenState, mDirtyRectangleSupport);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mTexture);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mColorMode);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mAlphaMode);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mColorArgument1);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mColorArgument2);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mAlphaArgument1);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mAlphaArgument2);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mTextureAddressingMode);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mMinFilterMode);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mMagFilterMode);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mMipFilterMode);
DX7_SAME_OFFSET(DX7RTextureStage, EeDx7TextureStageCache, mMipMapBias);

static_assert(sizeof(EeDx7LogicalMetrics) == 5 * sizeof(uint32_t), "ee_dx7/src/surface.rs LogicalMetrics");

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

// --- the pixel format helpers the Rust filter copy needs (the Graphics Engine's tables, R4.3) ----------------------------
extern "C" {
void ee_dx7cpp_record_native_error(long inError) { ESRecordNativeError((unsigned long)inError); }
unsigned char ee_dx7cpp_unpack_red(unsigned long inFormat, unsigned long inColor) { return GEUnpackRed(inFormat, inColor); }
unsigned char ee_dx7cpp_unpack_green(unsigned long inFormat, unsigned long inColor) { return GEUnpackGreen(inFormat, inColor); }
unsigned char ee_dx7cpp_unpack_blue(unsigned long inFormat, unsigned long inColor) { return GEUnpackBlue(inFormat, inColor); }
unsigned char ee_dx7cpp_unpack_alpha(unsigned long inFormat, unsigned long inColor) { return GEUnpackAlpha(inFormat, inColor); }
unsigned long ee_dx7cpp_pack(unsigned long inFormat, int32_t inRed, int32_t inGreen, int32_t inBlue, int32_t inAlpha)
{
    return GEPackColor(inFormat, (unsigned long)(long)inRed, (unsigned long)(long)inGreen, (unsigned long)(long)inBlue,
                       (unsigned long)(long)inAlpha);
}
}
