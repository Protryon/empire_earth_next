// C:\EEX\Low-Level Engine\Graphics Engine\GEVertexArray.h (RECONSTRUCTION). Abstract in the original (DX7HRDisplay
// DX7StandardVertexArray 0x105c8 gives the slot order); GEValues.cpp holds the members.
//
// port (R4.3 part 17): concrete. The renderer's two arrays (DX7StandardVertexArray: GXVERTEX, type 0;
// DX7StdColorVertexArray: GXVERTEX_STD_COLOR, type 1) were trampolines over one Rust array each: they are this class
// over ee_ge vertarr.rs, and the draws read the vertices in place (mVertices, right after the vtable pointer). The
// methods stay virtual: the store gate's arrays (tools/eemodel/storegate) are a subclass with their own storage.
#ifndef LLE_GEVERTEXARRAY_H
#define LLE_GEVERTEXARRAY_H

class GEColor;
class GEVector;
class GE3DPoint;
class GETextureCoordinate;
struct EeGeVertexArray;   // rust/crates/ee_ge/src/vertarr.rs

class LLE_API GEVertexArray {
public:
    GEVertexArray(unsigned long inType, unsigned long inCount);   // type 0 standard, 1 coloured
    virtual ~GEVertexArray();               // slot 0
    virtual void CopyFrom(const GEVertexArray& inArray);
    virtual void GetColor(GEColor& outColor, unsigned long inIndex) const;
    virtual void GetNormal(GEVector& outNormal, unsigned long inIndex) const;
    virtual float GetOpacity(unsigned long inIndex) const;
    virtual void GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const;     // slot 0x14
    virtual unsigned long GetSize() const;
    virtual void GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const;
    virtual unsigned long GetType() const;
    virtual void SetColor(const GEColor& inColor, unsigned long inIndex);        // slot 0x24
    virtual void SetNormal(const GEVector& inNormal, unsigned long inIndex);
    virtual void SetOpacity(float inOpacity, unsigned long inIndex);              // slot 0x2c
    virtual void SetPoint(const GE3DPoint& inPoint, unsigned long inIndex);      // slot 0x30
    virtual void SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex);
    virtual void SetVertex(const GEColor& inColor, const GE3DPoint& inPoint, const GEVector& inNormal,
                           const GETextureCoordinate& inCoordinate, float inOpacity, unsigned long inIndex);
    virtual unsigned long SizeOf() const;
    void* GetVertices() const { return this->mVertices; }   // the GPU's vertices (GXVERTEX or GXVERTEX_STD_COLOR)

protected:
    GEVertexArray();                        // a subclass with its own storage (no vertices, no Rust array)

private:
    void* mVertices;                        // the Rust array's vertices
    EeGeVertexArray* mRust;
};

#endif
