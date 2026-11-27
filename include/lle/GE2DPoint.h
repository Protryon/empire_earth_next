// C:\EEX\Low-Level Engine\Graphics Engine\GE2DPoint.h (RECONSTRUCTION). Requires <vector>.
// The distances, the side test and the interpolation forward to Rust (ee_base point.rs; lle/GERust.h).
#ifndef LLE_GE2DPOINT_H
#define LLE_GE2DPOINT_H

#include "lle/GERust.h"

#include "lle/LLEForward.h"

class GE2DPoint : public EeGe2DPoint {   // the Rust record (mX, mY; rust/base/math.h)
public:
    GE2DPoint() : EeGe2DPoint{0.0f, 0.0f} {}
    GE2DPoint(long inX, long inY) : EeGe2DPoint{(float)inX, (float)inY} {}
    GE2DPoint(float inX, float inY) : EeGe2DPoint{inX, inY} {}
    bool operator==(const GE2DPoint& inPoint) const { return this->mX == inPoint.mX && this->mY == inPoint.mY; }
    bool operator!=(const GE2DPoint& inPoint) const { return this->mX != inPoint.mX || this->mY != inPoint.mY; }
    GE2DPoint& operator+=(const GE2DPoint& inPoint) { return *this = GE2DPoint(this->mX + inPoint.mX, this->mY + inPoint.mY); }
    GE2DPoint& operator-=(const GE2DPoint& inPoint) { return *this = GE2DPoint(this->mX - inPoint.mX, this->mY - inPoint.mY); }
    void Clear() { *this = GE2DPoint(); }
    // cross product of (inFrom - inTo) and (inTo - this): the side of the line inFrom-inTo this point lies on
    float GetDirection(const GE2DPoint& inFrom, const GE2DPoint& inTo) const { return ee_ge_2d_direction(GERust(*this), GERust(inFrom), GERust(inTo)); }
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    void SetX(float inX) { this->mX = inX; }
    void SetY(float inY) { this->mY = inY; }
    void SetX(long inX) { this->mX = (float)inX; }
    void SetY(long inY) { this->mY = (float)inY; }
    void SetXY(float inX, float inY) { *this = GE2DPoint(inX, inY); }
    float DistanceTo(const GE2DPoint& inPoint) const { return ee_ge_2d_distance(GERust(*this), GERust(inPoint)); }
    float DistanceToSquared(const GE2DPoint& inPoint) const { return ee_ge_2d_distance_squared(GERust(*this), GERust(inPoint)); }
    // x and y from (to) an open file; kErrorNotOpen when it is not open, else the first read (write) error
    template <class F> long Load(F& inFile) { return ee_ge_2d_point_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) { return ee_ge_2d_point_save(GERust(*this), GERustFile(inFile)); }
};

#endif
