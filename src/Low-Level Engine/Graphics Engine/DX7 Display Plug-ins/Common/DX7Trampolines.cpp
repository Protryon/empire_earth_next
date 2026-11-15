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
