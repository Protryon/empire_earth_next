// C:\EEX\Low-Level Engine\Graphics Engine\GE3DPlane.h (RECONSTRUCTION). Requires GE3DLine.h.
#ifndef LLE_GE3DPLANE_H
#define LLE_GE3DPLANE_H
#include "lle/GERust.h"

class FSFile;

class GE3DPlane {                           // sizeof 0x18
public:
    GE3DPlane() { this->mNormal.SetZ(1.0f); }   // through the origin, facing +z
    GE3DPlane(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3)
        { this->Define(inPoint1, inPoint2, inPoint3); }
    GE3DPlane(const GE3DPoint& inPoint, const GEVector& inNormal) { this->mPoint = inPoint; this->mNormal = inNormal; }
    // copy ctor implicit: GEViewport's implicit copy ctor (0x1000e992) expands it (movsd point, GEVector copy)
    ~GE3DPlane() {}
    GE3DPlane& operator=(const GE3DPlane& inPlane)
        { this->mPoint = inPlane.mPoint; this->mNormal = inPlane.mNormal; return *this; }
    // through three points: the first point and the unit normal of their cross product
    void Define(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3)
        { ee_ge_plane_define(GERust(inPoint1), GERust(inPoint2), GERust(inPoint3), GERust(*this)); }
    void Define(const GE3DPoint& inPoint, const GEVector& inNormal) { this->mPoint = inPoint; this->mNormal = inNormal; }
    const GEVector& GetNormal() const { return this->mNormal; }
    const GE3DPoint& GetPoint() const { return this->mPoint; }
    // the line where the planes meet; false when they are parallel (degenerate normals: the C++ read an
    // uninitialized parameter, Rust takes 0)
    bool Intersects(const GE3DPlane& inPlane, GE3DLine& outLine) const
        { return ee_ge_plane_intersects(GERust(*this), GERust(inPlane), GERust(outLine)); }
    template <class F> long Load(F& inFile) { return ee_ge_3d_plane_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) { return ee_ge_3d_plane_save(GERust(*this), GERustFile(inFile)); }
private:
    friend struct GERustLayout;             // the Rust mirror's layout check (Graphics Engine/GERustMath.h)
    GE3DPoint mPoint;                       // +0x00
    GEVector mNormal;                       // +0x0c
};

#endif
