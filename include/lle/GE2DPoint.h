// C:\EEX\Low-Level Engine\Graphics Engine\GE2DPoint.h (RECONSTRUCTION). Requires <vector>.
// The distances, the side test and the interpolation forward to Rust (ee_base point.rs; lle/GERust.h).
#ifndef LLE_GE2DPOINT_H
#define LLE_GE2DPOINT_H

#include "lle/GERust.h"

class FSFile;

class GE2DPoint {                   // proven sizeof 8 (Default.dll vector<GE2DPoint>: index * 8)
public:
    GE2DPoint() : mX(0.0f), mY(0.0f) {}
    GE2DPoint(long inX, long inY) : mX((float)inX), mY((float)inY) {}
    GE2DPoint(float inX, float inY) : mX(inX), mY(inY) {}
    GE2DPoint(const GE2DPoint& inPoint) : mX(inPoint.mX), mY(inPoint.mY) {}
    ~GE2DPoint() {}
    GE2DPoint& operator=(const GE2DPoint& inPoint) { this->mX = inPoint.mX; this->mY = inPoint.mY; return *this; }
    bool operator==(const GE2DPoint& inPoint) const { return this->mX == inPoint.mX && this->mY == inPoint.mY; }
    bool operator!=(const GE2DPoint& inPoint) const { return this->mX != inPoint.mX || this->mY != inPoint.mY; }
    GE2DPoint& operator+=(const GE2DPoint& inPoint) { this->mX += inPoint.mX; this->mY += inPoint.mY; return *this; }
    GE2DPoint& operator-=(const GE2DPoint& inPoint) { this->mX -= inPoint.mX; this->mY -= inPoint.mY; return *this; }
    void Clear() { this->mX = 0.0f; this->mY = 0.0f; }
    // cross product of (inFrom - inTo) and (inTo - this): the side of the line inFrom-inTo this point lies on
    float GetDirection(const GE2DPoint& inFrom, const GE2DPoint& inTo) const
        { return ee_ge_2d_direction(GERust(*this), GERust(inFrom), GERust(inTo)); }
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    void SetX(float inX) { this->mX = inX; }
    void SetY(float inY) { this->mY = inY; }
    void SetX(long inX) { this->mX = (float)inX; }
    void SetY(long inY) { this->mY = (float)inY; }
    void SetXY(float inX, float inY) { this->mX = inX; this->mY = inY; }
    float DistanceTo(const GE2DPoint& inPoint) const { return ee_ge_2d_distance(GERust(*this), GERust(inPoint)); }
    float DistanceToSquared(const GE2DPoint& inPoint) const
        { return ee_ge_2d_distance_squared(GERust(*this), GERust(inPoint)); }
    // points every inStep along the segment to inPoint, always ending with inPoint itself
    // (Rust copies the two points before the vector is cleared: either may be one of its elements)
    void InterpolateTo(const GE2DPoint& inPoint, float inStep, std::vector<GE2DPoint>& outPoints) const
        { ee_ge_2d_interpolate(GERust(*this), GERust(inPoint), inStep, Push, &outPoints); }
    // x and y from (to) an open file; kErrorNotOpen when it is not open, else the first read (write) error
    template <class F> long Load(F& inFile) { return ee_ge_2d_point_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) { return ee_ge_2d_point_save(GERust(*this), GERustFile(inFile)); }
    // the interpolation's calls: the vector cleared (no point), then each point
    static void Push(void* inPoints, const EeGe2DPoint* inPoint)
    {
        inPoint ? static_cast<std::vector<GE2DPoint>*>(inPoints)->push_back(*reinterpret_cast<const GE2DPoint*>(inPoint))
                : static_cast<std::vector<GE2DPoint>*>(inPoints)->clear();
    }

    float mX;                               // +0x00
    float mY;                               // +0x04
};

#endif
