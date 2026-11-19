// C:\EEX\Low-Level Engine\Graphics Engine\GEVertexArray.h (RECONSTRUCTION). Abstract in the original (DX7HRDisplay
// DX7StandardVertexArray 0x105c8 gives the slot order).
//
// port (R4.3 part 17): concrete. The renderer's two arrays (DX7StandardVertexArray: GXVERTEX, type 0;
// DX7StdColorVertexArray: GXVERTEX_STD_COLOR, type 1) were trampolines over one Rust array each: they are this class
// over ee_render geval vertarr.rs, and the draws read the vertices in place (mVertices, right after the vtable
// pointer). The methods stay virtual: the store gate's arrays (tools/eemodel/storegate) are a subclass with their own
// storage. The values travel as floats (GEColor, GE3DPoint, GEVector: three; GETextureCoordinate: two).
#ifndef LLE_GEVERTEXARRAY_H
#define LLE_GEVERTEXARRAY_H

#include "lle/GERust.h"

class GEColor;
class GETextureCoordinate;

class LLE_API GEVertexArray {
public:
    GEVertexArray(unsigned long inType, unsigned long inCount)   // type 0 standard, 1 coloured
        : mRust(ee_ge_va_new(inType, inCount)) { this->mVertices = ee_ge_va_vertices(this->mRust); }
    virtual ~GEVertexArray()                // slot 0
    {
        if (this->mRust)
            ee_ge_va_destroy(this->mRust);
    }
    virtual void CopyFrom(const GEVertexArray& inArray) { ee_ge_va_copy_from(this->mRust, inArray.mRust); }
    virtual void GetColor(GEColor& outColor, unsigned long inIndex) const { this->Get(EeGeVaField::Color, inIndex, &outColor); }
    virtual void GetNormal(GEVector& outNormal, unsigned long inIndex) const { this->Get(EeGeVaField::Normal, inIndex, &outNormal); }
    virtual float GetOpacity(unsigned long inIndex) const
    {
        float theOpacity;
        this->Get(EeGeVaField::Opacity, inIndex, &theOpacity);
        return theOpacity;
    }
    virtual void GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const   // slot 0x14
        { this->Get(EeGeVaField::Point, inIndex, &outPoint); }
    virtual unsigned long GetSize() const { return ee_ge_va_size(this->mRust, false); }
    virtual void GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const
        { this->Get(EeGeVaField::Uv, inIndex, &outCoordinate); }
    virtual unsigned long GetType() const { return ee_ge_va_type(this->mRust); }
    virtual void SetColor(const GEColor& inColor, unsigned long inIndex)       // slot 0x24
        { this->Set(EeGeVaField::Color, inIndex, &inColor); }
    virtual void SetNormal(const GEVector& inNormal, unsigned long inIndex) { this->Set(EeGeVaField::Normal, inIndex, &inNormal); }
    virtual void SetOpacity(float inOpacity, unsigned long inIndex) { this->Set(EeGeVaField::Opacity, inIndex, &inOpacity); }   // slot 0x2c
    virtual void SetPoint(const GE3DPoint& inPoint, unsigned long inIndex) { this->Set(EeGeVaField::Point, inIndex, &inPoint); }   // slot 0x30
    virtual void SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex)
        { this->Set(EeGeVaField::Uv, inIndex, &inCoordinate); }
    virtual void SetVertex(const GEColor& inColor, const GE3DPoint& inPoint, const GEVector& inNormal,
                           const GETextureCoordinate& inCoordinate, float inOpacity, unsigned long inIndex)
    {
        ee_ge_va_set_vertex(this->mRust, Floats(&inColor), Floats(&inPoint), Floats(&inNormal), Floats(&inCoordinate),
                            inOpacity, inIndex);
    }
    virtual unsigned long SizeOf() const { return ee_ge_va_size(this->mRust, true); }
    void* GetVertices() const { return this->mVertices; }   // the GPU's vertices (GXVERTEX or GXVERTEX_STD_COLOR)

protected:
    GEVertexArray() : mVertices(NULL), mRust(NULL) {}   // a subclass with its own storage (no vertices, no Rust array)

private:
    static float* Floats(const void* inValue) { return reinterpret_cast<float*>(const_cast<void*>(inValue)); }
    void Get(EeGeVaField inField, unsigned long inIndex, void* outValue) const
        { ee_ge_va_get(this->mRust, inField, inIndex, Floats(outValue)); }
    void Set(EeGeVaField inField, unsigned long inIndex, const void* inValue)
        { ee_ge_va_set(this->mRust, inField, inIndex, Floats(inValue)); }

    void* mVertices;                        // the Rust array's vertices
    EeGeVertexArray* mRust;
};

#endif
