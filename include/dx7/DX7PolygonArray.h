// DX7PolygonArray (RECONSTRUCTION): a GEPolygonArray of 16-bit triangle indices (DrawIndexedPrimitive's WORD
// list). Layout from the ctor 0x100029fc and the accessors: indices +4, index count (3 per polygon) +8, polygon
// count +0xc. sizeof 0x10 (copy ctor 0x1000295d).
#ifndef DX7_DX7POLYGONARRAY_H
#define DX7_DX7POLYGONARRAY_H

#include "dx7/DX7Common.h"
#include "lle/GEPolygonArray.h"

struct EeDx7PolygonArray;   // rust/crates/ee_dx7/src/vertex.rs: the trampoline forwards to it (Common/DX7Trampolines.cpp)

class DX7_API DX7PolygonArray : public GEPolygonArray {
public:
    DX7PolygonArray(unsigned long inSize);
    virtual ~DX7PolygonArray();
    virtual unsigned long GetSize() const;
    virtual unsigned long GetVertexIndex1(unsigned long inPolygon) const;
    virtual unsigned long GetVertexIndex2(unsigned long inPolygon) const;
    virtual unsigned long GetVertexIndex3(unsigned long inPolygon) const;
    virtual void SetVertexIndex1(unsigned long inPolygon, unsigned long inIndex);
    virtual void SetVertexIndex2(unsigned long inPolygon, unsigned long inIndex);
    virtual void SetVertexIndex3(unsigned long inPolygon, unsigned long inIndex);
    virtual void SetVertexIndices(unsigned long inPolygon, unsigned long inIndex1, unsigned long inIndex2,
                                  unsigned long inIndex3);
    virtual unsigned long SizeOf() const;

    unsigned short* mIndices;               // +0x04
    unsigned long mIndexCount;              // +0x08
    unsigned long mSize;                    // +0x0c
    EeDx7PolygonArray* mRust;               // the Rust array owning mIndices
};

#endif
