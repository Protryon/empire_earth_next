// C:\EEX\Low-Level Engine\Graphics Engine\GE3DPoint.h (RECONSTRUCTION). Requires GE2DPoint.h, GEVector.h, <vector>.
#ifndef LLE_GE3DPOINT_H
#define LLE_GE3DPOINT_H

#include "lle/GERust.h"

#include "lle/LLEForward.h"

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
    void Clear() { *this = GE3DPoint(); }
    float DistanceToSquared(const GE3DPoint& inPoint) const { return ee_ge_3d_distance_squared(GERust(*this), GERust(inPoint)); }
    void GetChecksum(ESChecksum& ioChecksum) const {}
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    float GetZ() const { return this->mZ; }
    // x, y and z from (to) an open file; kErrorNotOpen when it is not open, else the first read (write) error
    template <class F> long Load(F& inFile) { return ee_ge_3d_point_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Load(unsigned long, unsigned long, F& inFile) { return this->Load(inFile); }
    template <class F> long Save(F& inFile) const { return ee_ge_3d_point_save(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(unsigned long, unsigned long, F& inFile) const { return this->Save(inFile); }
    void SetX(float inX) { this->mX = inX; }
    void SetY(float inY) { this->mY = inY; }
    void SetXYZ(float inX, float inY, float inZ) { *this = GE3DPoint(inX, inY, inZ); }
};

#endif
