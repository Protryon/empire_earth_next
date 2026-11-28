// C:\EEX\Low-Level Engine\Graphics Engine\GE3DLine.h (RECONSTRUCTION). Requires GE3DPoint.h (GEVector).
#ifndef LLE_GE3DLINE_H
#define LLE_GE3DLINE_H
#include "lle/GERust.h"

#include "lle/LLEForward.h"
#include "rust/base/records.h"

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
    template <class F> long Load(F& inFile) { return ee_ge_3d_line_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) const { return ee_ge_3d_line_save(GERust(*this), GERustFile(inFile)); }
    void SetDirection(const GEVector& inDirection) { this->mDirection = inDirection; }
    void SetStart(const GE3DPoint& inStart) { this->mStart = inStart; }
    // the parameter (and the point) where the line reaches a given x, y or z; false when it runs parallel to it
    bool SolveForZ(float inZ, float& outT) const { return ee_ge_line_solve(GERust(*this), 2, inZ, &outT, NULL); }
    bool SolveForZ(float inZ, float& outT, GE3DPoint& outPoint) const { return ee_ge_line_solve(GERust(*this), 2, inZ, &outT, GERust(outPoint)); }
};

// C:\EEX\Low-Level Engine\Graphics Engine\GE3DPlane.h (RECONSTRUCTION). (folded into GE3DLine.h in LLE/UI round 2)
class GE3DPlane : public EeGe3DPlane {   // the Rust record (mPoint, mNormal; rust/base/math.h)
public:
    GE3DPlane() : EeGe3DPlane{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}} {}   // through the origin, facing +z
    GE3DPlane(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3) : EeGe3DPlane{}
        { this->Define(inPoint1, inPoint2, inPoint3); }
    GE3DPlane(const GE3DPoint& inPoint, const GEVector& inNormal) : EeGe3DPlane{inPoint, inNormal} {}
    // through three points: the first point and the unit normal of their cross product
    void Define(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3)
        { ee_ge_plane_define(GERust(inPoint1), GERust(inPoint2), GERust(inPoint3), GERust(*this)); }
    void Define(const GE3DPoint& inPoint, const GEVector& inNormal) { *this = GE3DPlane(inPoint, inNormal); }
    const GE3DPoint& GetPoint() const { return GECpp(this->mPoint); }
    template <class F> long Load(F& inFile) { return ee_ge_3d_plane_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) { return ee_ge_3d_plane_save(GERust(*this), GERustFile(inFile)); }
};

#endif
