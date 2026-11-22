// C:\EEX\Low-Level Engine\Graphics Engine\GE3DPoint.h (RECONSTRUCTION). Requires GE2DPoint.h, GEVector.h, <vector>.
#ifndef LLE_GE3DPOINT_H
#define LLE_GE3DPOINT_H

#include "lle/GERust.h"

class FSFile;
class GETransformation;
class ESChecksum;

class GE3DPoint {                   // sizeof 0xc (GE3DLine: start +0, direction +0xc)
public:
    // no user copy ctor: GE3DLine's copy ctor 0xc725 copies its GE3DPoint with movsd (trivial copy)
    GE3DPoint() : mX(0.0f), mY(0.0f), mZ(0.0f) {}
    // inline (GE2DPoint::InterpolateTo stores its locals directly); its exported copy is the ICF representative
    // at 0x1000d56f, folded with GEVector's
    GE3DPoint(float inX, float inY, float inZ) : mX(inX), mY(inY), mZ(inZ) {}
    GE3DPoint(const GETransformation& inTransformation);   // at its position (lle/GETransformation.h)
    ~GE3DPoint() {}
    // user-defined (field moves): GE3DLine's assignments 0xc5da/0xc66a copy the start with three movs, not movsd
    GE3DPoint& operator=(const GE3DPoint& inPoint) { this->mX = inPoint.mX; this->mY = inPoint.mY; this->mZ = inPoint.mZ; return *this; }
    GE3DPoint& operator+=(const GE3DPoint& inPoint) { this->mX += inPoint.mX; this->mY += inPoint.mY; this->mZ += inPoint.mZ; return *this; }
    GE3DPoint& operator+=(const GEVector& inVector) { this->mX += inVector.GetX(); this->mY += inVector.GetY(); this->mZ += inVector.GetZ(); return *this; }
    GE3DPoint& operator-=(const GE3DPoint& inPoint) { this->mX -= inPoint.mX; this->mY -= inPoint.mY; this->mZ -= inPoint.mZ; return *this; }
    GE3DPoint& operator-=(const GEVector& inVector) { this->mX -= inVector.GetX(); this->mY -= inVector.GetY(); this->mZ -= inVector.GetZ(); return *this; }
    bool operator==(const GE2DPoint& inPoint) const { return inPoint.mX == this->mX && inPoint.mY == this->mY; }
    // inline (GEModel::Verify, GELODMetaVertex::BuildMetaVertexList/CollapseEdge expand it); GE2DPoint.cpp's block
    // emits the exported copy 0x1000c47e (ICF-folded with GEVector's/GEColor's ==)
    bool operator==(const GE3DPoint& inPoint) const { return mX == inPoint.mX && mY == inPoint.mY && mZ == inPoint.mZ; }
    bool operator==(const GETransformation& inTransformation) const;   // (exact; lle/GETransformation.h)
    // inline (InterpolateTo 0xdae4 expands it); its exported copy is ICF-folded into GEVector's 0x1000c29f
    bool operator!=(const GE3DPoint& inPoint) const { return mX != inPoint.mX || mY != inPoint.mY || mZ != inPoint.mZ; }
    void Add(const GEVector& inVector, GE3DPoint& outPoint) const
    {
        outPoint.mX = inVector.GetX() + this->mX;
        outPoint.mY = inVector.GetY() + this->mY;
        outPoint.mZ = inVector.GetZ() + this->mZ;
    }
    void Clear() { this->mX = 0.0f; this->mY = 0.0f; this->mZ = 0.0f; }
    float DistanceTo(const GE3DPoint& inPoint) const { return ee_ge_3d_distance(GERust(*this), GERust(inPoint)); }
    float DistanceToSquared(const GE3DPoint& inPoint) const
        { return ee_ge_3d_distance_squared(GERust(*this), GERust(inPoint)); }
    GE2DPoint Get2DPoint() const { return GE2DPoint(this->mX, this->mY); }  // inline (InterpolateTo 0xc80e)
    void GetChecksum(ESChecksum& ioChecksum) const {}
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    float GetZ() const { return this->mZ; }
    // points every inStep along the segment to inPoint, always ending with inPoint itself (points within 0.0001 of
    // each other but not equal: the C++ read an uninitialized end, Rust takes 0)
    // (Rust copies the two points before the vector is cleared: either may be one of its elements)
    void InterpolateTo(const GE3DPoint& inPoint, float inStep, std::vector<GE3DPoint>& outPoints) const
        { ee_ge_3d_interpolate(GERust(*this), GERust(inPoint), inStep, Push, &outPoints); }
    // the interpolation's calls: the vector cleared (no point), then each point
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
    void SetXYZ(float inX, float inY, float inZ) { this->mX = inX; this->mY = inY; this->mZ = inZ; }

    float mX;                               // +0x00
    float mY;                               // +0x04
    float mZ;                               // +0x08
};

#endif
