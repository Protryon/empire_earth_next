// DX7StdColorVertexArray (RECONSTRUCTION): a GEVertexArray of GXVERTEX_STD_COLOR (position, normal, a packed
// ARGB diffuse color carrying the opacity in its alpha byte, one texture coordinate). Layout from the ctor
// 0x1000e96c: vertices +4, size +8. sizeof 0xc (copy ctor 0x100011bb).
#ifndef DX7_DX7STDCOLORVERTEXARRAY_H
#define DX7_DX7STDCOLORVERTEXARRAY_H

#include "dx7/DX7Common.h"
#include "lle/GEVertexArray.h"

// The plug-in's own flexible vertex (name from the exports, e.g. ?CopyStdVertices@DX7Rasterizer@@IAEXPAVGEModel@@
// PAVGEMaterial@@PAUD3DVERTEX_STD_COLOR@@@Z); sizeof 0x24 (stride of every DX7StdColorVertexArray accessor).
// Member names provisional (GXVERTEX/D3DLVERTEX style).
struct GXVERTEX_STD_COLOR {
    float x;                             // +0x00
    float y;
    float z;
    float nx;                            // +0x0c
    float ny;
    float nz;
    uint32_t color;                      // +0x18 ARGB (32-bit on every data model: the GPU's vertex stride is 0x24)
    float tu;                            // +0x1c
    float tv;
};
static_assert(sizeof(GXVERTEX_STD_COLOR) == 0x24, "GXVERTEX_STD_COLOR: the GX vertex layout");

struct EeDx7StdColorVertexArray;   // rust/crates/ee_dx7/src/vertex.rs (Common/DX7Trampolines.cpp forwards to it)

class DX7_API DX7StdColorVertexArray : public GEVertexArray {
public:
    DX7StdColorVertexArray(unsigned long inSize);
    virtual ~DX7StdColorVertexArray();
    virtual void CopyFrom(const GEVertexArray& inArray);
    virtual void GetColor(GEColor& outColor, unsigned long inIndex) const;
    virtual void GetNormal(GEVector& outNormal, unsigned long inIndex) const;
    virtual float GetOpacity(unsigned long inIndex) const;
    virtual void GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const;
    virtual unsigned long GetSize() const;
    virtual void GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const;
    virtual unsigned long GetType() const;
    virtual void SetColor(const GEColor& inColor, unsigned long inIndex);
    virtual void SetNormal(const GEVector& inNormal, unsigned long inIndex);
    virtual void SetOpacity(float inOpacity, unsigned long inIndex);
    virtual void SetPoint(const GE3DPoint& inPoint, unsigned long inIndex);
    virtual void SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex);
    virtual void SetVertex(const GEColor& inColor, const GE3DPoint& inPoint, const GEVector& inNormal,
                           const GETextureCoordinate& inCoordinate, float inOpacity, unsigned long inIndex);
    virtual unsigned long SizeOf() const;

    GXVERTEX_STD_COLOR* mVertices;                   // +0x04
    unsigned long mSize;                    // +0x08
    EeDx7StdColorVertexArray* mRust;        // the Rust array owning mVertices
};

#endif
