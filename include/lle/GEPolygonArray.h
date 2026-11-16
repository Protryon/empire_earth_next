// C:\EEX\Low-Level Engine\Graphics Engine\GEPolygonArray.h (RECONSTRUCTION). Abstract in the original (DX7HRDisplay
// DX7PolygonArray gives the slot order); GEValues.cpp holds the members.
//
// port (R4.3 part 17): concrete over ee_ge vertarr.rs (the renderer's DX7PolygonArray, 16-bit triangle indices,
// merged in); the draws read the indices in place (mIndices). Virtual for the store gate's subclass.
#ifndef LLE_GEPOLYGONARRAY_H
#define LLE_GEPOLYGONARRAY_H

struct EeGePolygonArray;   // rust/crates/ee_ge/src/vertarr.rs

class LLE_API GEPolygonArray {
public:
    explicit GEPolygonArray(unsigned long inCount);
    virtual ~GEPolygonArray();
    virtual unsigned long GetSize() const;
    virtual unsigned long GetVertexIndex1(unsigned long inPolygon) const;
    virtual unsigned long GetVertexIndex2(unsigned long inPolygon) const;
    virtual unsigned long GetVertexIndex3(unsigned long inPolygon) const;
    virtual void SetVertexIndex1(unsigned long inPolygon, unsigned long inIndex);
    virtual void SetVertexIndex2(unsigned long inPolygon, unsigned long inIndex);
    virtual void SetVertexIndex3(unsigned long inPolygon, unsigned long inIndex);
    virtual void SetVertexIndices(unsigned long inPolygon, unsigned long inIndex1, unsigned long inIndex2,
                                  unsigned long inIndex3);   // (inPolygon first: DX7PolygonArray.cpp)
    virtual unsigned long SizeOf() const;
    const unsigned short* GetIndices() const { return this->mIndices; }   // three per triangle

protected:
    GEPolygonArray();                       // a subclass with its own storage

private:
    unsigned short* mIndices;               // the Rust array's indices
    EeGePolygonArray* mRust;
};

#endif
