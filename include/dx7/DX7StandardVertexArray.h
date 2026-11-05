// DX7StandardVertexArray (RECONSTRUCTION): a GEVertexArray of GXVERTEX (position, normal, one texture coordinate;
// no color, opacity 1). Layout from the ctor 0x1000e793: vertices +4, size +8. sizeof 0xc (copy ctor 0x10003998).
#ifndef DX7_DX7STANDARDVERTEXARRAY_H
#define DX7_DX7STANDARDVERTEXARRAY_H

#include "dx7/DX7Common.h"
#include "lle/GEVertexArray.h"

class DX7_API DX7StandardVertexArray : public GEVertexArray {
public:
    DX7StandardVertexArray(unsigned long inSize);
    virtual ~DX7StandardVertexArray();
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

    GXVERTEX* mVertices;                   // +0x04
    unsigned long mSize;                    // +0x08
};

#endif
