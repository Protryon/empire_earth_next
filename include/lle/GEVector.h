// C:\EEX\Low-Level Engine\Graphics Engine\GEVector.h (RECONSTRUCTION). GEVector.cpp (__FILE__ anchor, ~0x10036755)
// held the quantized-vector code and the magnitudes (Rust: ee_base vector.rs; lle/GERust.h).
#ifndef LLE_GEVECTOR_H
#define LLE_GEVECTOR_H

#include "lle/GERust.h"

#include "lle/LLEForward.h"
#include "rust/base/records.h"

class GEVector : public EeGeVector {   // the Rust record (mX, mY, mZ; rust/base/math.h)
public:
    GEVector() : EeGeVector{0.0f, 0.0f, 0.0f} {}
    GEVector(float inX, float inY, float inZ) : EeGeVector{inX, inY, inZ} {}
    GEVector operator-() const { return GEVector(-this->mX, -this->mY, -this->mZ); }
    // component by component (each component reads only its own)
    GEVector& operator+=(const GEVector& inVector)
        { return *this = GEVector(this->mX + inVector.mX, this->mY + inVector.mY, this->mZ + inVector.mZ); }
    GEVector& operator-=(const GEVector& inVector)
        { return *this = GEVector(this->mX - inVector.mX, this->mY - inVector.mY, this->mZ - inVector.mZ); }
    bool operator==(const GEVector& inVector) const { return this->mX == inVector.mX && this->mY == inVector.mY && this->mZ == inVector.mZ; }
    bool operator!=(const GEVector& inVector) const { return this->mX != inVector.mX || this->mY != inVector.mY || this->mZ != inVector.mZ; }
    float GetX() const { return this->mX; }  // inline (exported, ICF-folded with GE2DPoint's)
    float GetY() const { return this->mY; }
    float GetZ() const { return this->mZ; }
    // the nearest entry of the quantized direction table (a 41 x 41 grid of unit directions on each cube face; the
    // shared zero vector for (0, 0, 0)): Rust's table (ee_ge_vector_quantized)
    const GEVector* GetQuantizedVersion() const
    {
        const GEVector* theVector = reinterpret_cast<const GEVector*>(ee_ge_vector_quantized(GERust(*this)));
        ESVERIFY_AT(theVector != NULL, 383);
        return theVector;
    }
    void Normalize() { ee_ge_vector_normalize(GERust(*this)); }
    // inline (no import in Default.dll; LLE's exported body 0xc3d4 is this expression; 0xc29f keeps the scale in
    // st0 across the three products, its tail-merged branches load only the scale)
    void Scale(float inScale, GEVector& outVector) const { outVector = GEVector(inScale * this->mX, inScale * this->mY, inScale * this->mZ); }
    void SetX(float inX) { this->mX = inX; }  // inline (exported 0x54ae4 / 0xc56c / 0xc576)
    void SetY(float inY) { this->mY = inY; }
    template <class F> long Load(F& inFile) { return ee_ge_vector_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) const { return ee_ge_vector_save(GERust(*this), GERustFile(inFile)); }
};

#endif
