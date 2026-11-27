// C:\EEX\Low-Level Engine\Graphics Engine\GE3DPoint.h (RECONSTRUCTION). Requires GE2DPoint.h, <vector>. GEVector
// (GEVector.h in the original) is here too (LLE/UI round 2).
#ifndef LLE_GE3DPOINT_H
#define LLE_GE3DPOINT_H

#include "lle/GERust.h"

#include "lle/LLEForward.h"

// C:\EEX\Low-Level Engine\Graphics Engine\GEVector.h (RECONSTRUCTION). GEVector.cpp (__FILE__ anchor, ~0x10036755)
// held the quantized-vector code and the magnitudes (Rust: ee_base vector.rs; lle/GERust.h).
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
    void Add(const GEVector& inVector, GEVector& outVector) const
        { outVector = GEVector(inVector.mX + this->mX, inVector.mY + this->mY, inVector.mZ + this->mZ); }
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
    void SetZ(float inZ) { this->mZ = inZ; }
    template <class F> long Load(F& inFile) { return ee_ge_vector_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) const { return ee_ge_vector_save(GERust(*this), GERustFile(inFile)); }
};

class GE3DPoint : public EeGe3DPoint {   // the Rust record (mX, mY, mZ; rust/base/math.h)
public:
    GE3DPoint() : EeGe3DPoint{0.0f, 0.0f, 0.0f} {}
    GE3DPoint(float inX, float inY, float inZ) : EeGe3DPoint{inX, inY, inZ} {}
    GE3DPoint(const GETransformation& inTransformation);   // at its position (lle/GETransformation.h)
    // component by component (each component reads only its own: an argument that is this point gives the same)
    GE3DPoint& operator+=(const GE3DPoint& inPoint)
        { return *this = GE3DPoint(this->mX + inPoint.mX, this->mY + inPoint.mY, this->mZ + inPoint.mZ); }
    GE3DPoint& operator+=(const GEVector& inVector)
        { return *this = GE3DPoint(this->mX + inVector.mX, this->mY + inVector.mY, this->mZ + inVector.mZ); }
    GE3DPoint& operator-=(const GE3DPoint& inPoint)
        { return *this = GE3DPoint(this->mX - inPoint.mX, this->mY - inPoint.mY, this->mZ - inPoint.mZ); }
    GE3DPoint& operator-=(const GEVector& inVector)
        { return *this = GE3DPoint(this->mX - inVector.mX, this->mY - inVector.mY, this->mZ - inVector.mZ); }
    bool operator==(const GE2DPoint& inPoint) const { return inPoint.mX == this->mX && inPoint.mY == this->mY; }
    // inline (GEModel::Verify, GELODMetaVertex::BuildMetaVertexList/CollapseEdge expand it); GE2DPoint.cpp's block
    // emits the exported copy 0x1000c47e (ICF-folded with GEVector's/GEColor's ==)
    bool operator==(const GE3DPoint& inPoint) const { return mX == inPoint.mX && mY == inPoint.mY && mZ == inPoint.mZ; }
    bool operator==(const GETransformation& inTransformation) const;   // (exact; lle/GETransformation.h)
    // inline (InterpolateTo 0xdae4 expands it); its exported copy is ICF-folded into GEVector's 0x1000c29f
    bool operator!=(const GE3DPoint& inPoint) const { return mX != inPoint.mX || mY != inPoint.mY || mZ != inPoint.mZ; }
    void Add(const GEVector& inVector, GE3DPoint& outPoint) const
        { outPoint = GE3DPoint(inVector.mX + this->mX, inVector.mY + this->mY, inVector.mZ + this->mZ); }
    void Clear() { *this = GE3DPoint(); }
    float DistanceTo(const GE3DPoint& inPoint) const { return ee_ge_3d_distance(GERust(*this), GERust(inPoint)); }
    float DistanceToSquared(const GE3DPoint& inPoint) const { return ee_ge_3d_distance_squared(GERust(*this), GERust(inPoint)); }
    void GetChecksum(ESChecksum& ioChecksum) const {}
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    float GetZ() const { return this->mZ; }
    // the frustum points' calls (GEViewport): the vector cleared (no point), then each point
    static void Push(void* inPoints, const EeGe3DPoint* inPoint)
    {
        inPoint ? static_cast<std::vector<GE3DPoint>*>(inPoints)->push_back(*reinterpret_cast<const GE3DPoint*>(inPoint))
                : static_cast<std::vector<GE3DPoint>*>(inPoints)->clear();
    }
    // x, y and z from (to) an open file; kErrorNotOpen when it is not open, else the first read (write) error
    template <class F> long Load(F& inFile) { return ee_ge_3d_point_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Load(unsigned long, unsigned long, F& inFile) { return this->Load(inFile); }
    template <class F> long Save(F& inFile) const { return ee_ge_3d_point_save(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(unsigned long, unsigned long, F& inFile) const { return this->Save(inFile); }
    void SetX(float inX) { this->mX = inX; }
    void SetY(float inY) { this->mY = inY; }
    void SetZ(float inZ) { this->mZ = inZ; }
    void SetXYZ(float inX, float inY, float inZ) { *this = GE3DPoint(inX, inY, inZ); }
};

#endif
