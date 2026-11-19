// C:\EEX\Low-Level Engine\Graphics Engine\GEVector.h (RECONSTRUCTION). GEVector.cpp (__FILE__ anchor, ~0x10036755)
// held the quantized-vector code and the magnitudes (Rust: ee_base vector.rs; lle/GERust.h).
#ifndef LLE_GEVECTOR_H
#define LLE_GEVECTOR_H

#include "lle/GERust.h"

class FSFile;

class GEVector {                    // proven sizeof 0xc (EEClumps: +0x08..+0x14, three fldz)
public:
#ifdef LLE_EXE_HEADERS
    GEVector() : mX(0.0f), mY(0.0f), mZ(0.0f) {}
#else
    // through the inline setters (not a free inline): QuantizedVectorArray's member-array ctor 0x10035ab0 calls the
    // out-of-line GEVector() 0x2766 times; the init-list form expands there (the exe view keeps the init list: the
    // setter form costs exe 0x4b0502)
    GEVector() { this->SetX(0.0f); this->SetY(0.0f); this->SetZ(0.0f); }
#endif
    GEVector(float inX, float inY, float inZ) : mX(inX), mY(inY), mZ(inZ) {}
    // user-defined (exported ICF pair 0x1000d588): GE3DLine's copy ctor 0xc725 copies its GEVector with three
    // movs where its trivially-copied GE3DPoint gets movsd
    GEVector(const GEVector& inVector) : mX(inVector.mX), mY(inVector.mY), mZ(inVector.mZ) {}
    ~GEVector() {}
    GEVector& operator=(const GEVector& inVector) { this->mX = inVector.mX; this->mY = inVector.mY; this->mZ = inVector.mZ; return *this; }
    GEVector operator-() const { return GEVector(-this->mX, -this->mY, -this->mZ); }
    GEVector& operator+=(const GEVector& inVector) { this->mX += inVector.mX; this->mY += inVector.mY; this->mZ += inVector.mZ; return *this; }
    GEVector& operator-=(const GEVector& inVector) { this->mX -= inVector.mX; this->mY -= inVector.mY; this->mZ -= inVector.mZ; return *this; }
    bool operator==(const GEVector& inVector) const { return this->mX == inVector.mX && this->mY == inVector.mY && this->mZ == inVector.mZ; }
    bool operator!=(const GEVector& inVector) const { return this->mX != inVector.mX || this->mY != inVector.mY || this->mZ != inVector.mZ; }
    void Add(const GEVector& inVector, GEVector& outVector) const
    {
        outVector.mX = inVector.mX + this->mX;
        outVector.mY = inVector.mY + this->mY;
        outVector.mZ = inVector.mZ + this->mZ;
    }
    // inline (GE3DPlane::IsPointOnPlane 0xd4c3 expands it; the plug-ins never call it)
    float DotProduct(const GEVector& inVector) const { return inVector.mZ * this->mZ + inVector.mY * this->mY + inVector.mX * this->mX; }
    float GetMagnitude() const { return ee_ge_vector_magnitude(GERust(*this)); }
    float GetMagnitudeSquared() const { return ee_ge_vector_magnitude_squared(GERust(*this)); }
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
    void Normalize()
    {
        float theMagnitude = this->GetMagnitude();
        if (theMagnitude >= -0.0001f && theMagnitude <= 0.0001f) {
            this->mX = 0.0f;
            this->mY = 0.0f;
            this->mZ = 0.0f;
        } else {
            float theScale = 1.0f / theMagnitude;
            this->mX *= theScale;
            this->mY *= theScale;
            this->mZ *= theScale;
        }
    }
    // inline (no import in Default.dll; LLE's exported body 0xc3d4 is this expression; 0xc29f keeps the scale in
    // st0 across the three products, its tail-merged branches load only the scale)
    void Scale(float inScale, GEVector& outVector) const
    {
        outVector.mX = inScale * this->mX;
        outVector.mY = inScale * this->mY;
        outVector.mZ = inScale * this->mZ;
    }
    void SetX(float inX) { this->mX = inX; }  // inline (exported 0x54ae4 / 0xc56c / 0xc576)
    void SetY(float inY) { this->mY = inY; }
    void SetZ(float inZ) { this->mZ = inZ; }
    template <class F> long Load(F& inFile) { const float* f[] = {&mX, &mY, &mZ}; return GERustFields(inFile, f, 3, true, false); }
    template <class F> long Save(F& inFile) const
        { const float* f[] = {&mX, &mY, &mZ}; return GERustFields(inFile, f, 3, true, true); }

    float mX;                               // +0x00
    float mY;                               // +0x04
    float mZ;                               // +0x08
};

#endif
