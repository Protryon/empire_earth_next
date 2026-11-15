// C:\EEX\Low-Level Engine\Graphics Engine\GE2DPoint.h (RECONSTRUCTION). Requires <vector>.
// port (R4.3, fifteenth part): GE2DPoint.cpp is gone: the component-wise members are inline here, the distances,
// the side test and the interpolation forward to Rust from GE3DPoint.cpp (ee_ge point.rs).
#ifndef LLE_GE2DPOINT_H
#define LLE_GE2DPOINT_H

class UString;
class FSFile;

class LLE_API GE2DPoint {                   // proven sizeof 8 (Default.dll vector<GE2DPoint>: index * 8)
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
    float GetDirection(const GE2DPoint& inFrom, const GE2DPoint& inTo) const;
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    void SetX(float inX) { this->mX = inX; }
    void SetY(float inY) { this->mY = inY; }
    void SetX(long inX) { this->mX = (float)inX; }
    void SetY(long inY) { this->mY = (float)inY; }
    void SetXY(float inX, float inY) { this->mX = inX; this->mY = inY; }
    float DistanceTo(const GE2DPoint& inPoint) const;
    float DistanceToSquared(const GE2DPoint& inPoint) const;
    void InterpolateTo(const GE2DPoint& inPoint, float inStep, std::vector<GE2DPoint>& outPoints) const;
    long Load(FSFile& inFile);
    long Save(FSFile& inFile);

    float mX;                               // +0x00
    float mY;                               // +0x04
};

#endif
