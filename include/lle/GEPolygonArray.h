// C:\EEX\Low-Level Engine\Graphics Engine\GEPolygonArray.h (RECONSTRUCTION). Abstract in the original (DX7HRDisplay
// DX7PolygonArray gives the slot order).
//
// port (R4.3 part 17): concrete over ee_render geval vertarr.rs (the renderer's DX7PolygonArray, 16-bit triangle
// indices, merged in); the draws read the indices in place (mIndices); the model's calls are on the Rust array (ee_ge
// modobj.rs array_op).
#ifndef LLE_GEPOLYGONARRAY_H
#define LLE_GEPOLYGONARRAY_H

#include "lle/GERust.h"

class LLE_API GEPolygonArray {
public:
    explicit GEPolygonArray(unsigned long inCount) : mRust(ee_ge_pa_new(inCount)) { this->mIndices = ee_ge_pa_indices(this->mRust); }
    virtual ~GEPolygonArray() { ee_ge_pa_destroy(this->mRust); }
    virtual unsigned long GetSize() const { return ee_ge_pa_size(this->mRust, false); }
    virtual unsigned long GetVertexIndex1(unsigned long inPolygon) const { return ee_ge_pa_index(this->mRust, inPolygon, 0); }
    virtual unsigned long GetVertexIndex2(unsigned long inPolygon) const { return ee_ge_pa_index(this->mRust, inPolygon, 1); }
    virtual unsigned long GetVertexIndex3(unsigned long inPolygon) const { return ee_ge_pa_index(this->mRust, inPolygon, 2); }
    virtual void SetVertexIndex1(unsigned long inPolygon, unsigned long inIndex) { ee_ge_pa_set_index(this->mRust, inPolygon, 0, inIndex); }
    virtual void SetVertexIndex2(unsigned long inPolygon, unsigned long inIndex) { ee_ge_pa_set_index(this->mRust, inPolygon, 1, inIndex); }
    virtual void SetVertexIndex3(unsigned long inPolygon, unsigned long inIndex) { ee_ge_pa_set_index(this->mRust, inPolygon, 2, inIndex); }
    virtual void SetVertexIndices(unsigned long inPolygon, unsigned long inIndex1, unsigned long inIndex2,
                                  unsigned long inIndex3)   // (inPolygon first: DX7PolygonArray.cpp)
        { ee_ge_pa_set_indices(this->mRust, inPolygon, inIndex1, inIndex2, inIndex3); }
    virtual unsigned long SizeOf() const { return ee_ge_pa_size(this->mRust, true); }
    const unsigned short* GetIndices() const { return this->mIndices; }   // three per triangle

    unsigned short* mIndices;               // the Rust array's indices
    EeGePolygonArray* mRust;
};

#endif
