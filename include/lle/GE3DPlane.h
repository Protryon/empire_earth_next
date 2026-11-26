// C:\EEX\Low-Level Engine\Graphics Engine\GE3DPlane.h (RECONSTRUCTION). Requires GE3DLine.h.
#ifndef LLE_GE3DPLANE_H
#define LLE_GE3DPLANE_H
#include "lle/GERust.h"

#include "lle/LLEForward.h"

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
