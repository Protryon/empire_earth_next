// C:\EEX\Low-Level Engine\Graphics Engine\GE3DLine.h (RECONSTRUCTION). Requires GE3DPoint.h, GEVector.h.
#ifndef LLE_GE3DLINE_H
#define LLE_GE3DLINE_H
#include "lle/GERust.h"

#include "lle/LLEForward.h"

class GE3DLine : public EeGe3DLine {   // the Rust record (mStart, mDirection; rust/base/math.h)
public:
    GE3DLine() : EeGe3DLine{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}} {}
    // from a start to an end: the direction is the end less the start
    GE3DLine(const GE3DPoint& inStart, const GE3DPoint& inEnd)
        : EeGe3DLine{inStart, {inEnd.mX - inStart.mX, inEnd.mY - inStart.mY, inEnd.mZ - inStart.mZ}} {}
    GE3DLine(const GE3DPoint& inStart, const GEVector& inDirection) : EeGe3DLine{inStart, inDirection} {}
    void Define(const GE3DPoint& inStart, const GE3DPoint& inEnd) { *this = GE3DLine(inStart, inEnd); }
    void Define(const GE3DPoint& inStart, const GEVector& inDirection) { *this = GE3DLine(inStart, inDirection); }
    const GEVector& GetDirection() const { return GECpp(this->mDirection); }
    const GE3DPoint& GetStart() const { return GECpp(this->mStart); }
    // within inTolerance of a point (outT: the nearer of the two crossing parameters); two lines meeting (within 0.05
    // in the third axis; the parameters on each); a plane (false when parallel)
    bool Intersects(const GE3DPoint& inPoint, float inTolerance, float& outT) const
        { return ee_ge_line_intersects_point(GERust(*this), GERust(inPoint), inTolerance, &outT); }
    bool Intersects(const GE3DLine& inLine, float& outT, float& outLineT) const
        { return ee_ge_line_intersects_line(GERust(*this), GERust(inLine), &outT, &outLineT); }
    bool Intersects(const GE3DPlane& inPlane, float& outT) const { return ee_ge_line_intersects_plane(GERust(*this), GERust(inPlane), &outT); }
    template <class F> long Load(F& inFile) { return ee_ge_3d_line_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) const { return ee_ge_3d_line_save(GERust(*this), GERustFile(inFile)); }
    void SetDirection(const GEVector& inDirection) { this->mDirection = inDirection; }
    void SetStart(const GE3DPoint& inStart) { this->mStart = inStart; }
    // the parameter (and the point) where the line reaches a given x, y or z; false when it runs parallel to it
    bool SolveForX(float inX, float& outT) const { return ee_ge_line_solve(GERust(*this), 0, inX, &outT, NULL); }
    bool SolveForX(float inX, float& outT, GE3DPoint& outPoint) const { return ee_ge_line_solve(GERust(*this), 0, inX, &outT, GERust(outPoint)); }
    bool SolveForY(float inY, float& outT) const { return ee_ge_line_solve(GERust(*this), 1, inY, &outT, NULL); }
    bool SolveForY(float inY, float& outT, GE3DPoint& outPoint) const { return ee_ge_line_solve(GERust(*this), 1, inY, &outT, GERust(outPoint)); }
    bool SolveForZ(float inZ, float& outT) const { return ee_ge_line_solve(GERust(*this), 2, inZ, &outT, NULL); }
    bool SolveForZ(float inZ, float& outT, GE3DPoint& outPoint) const { return ee_ge_line_solve(GERust(*this), 2, inZ, &outT, GERust(outPoint)); }
};

#endif
