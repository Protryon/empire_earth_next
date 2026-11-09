// C:\EEX\Low-Level Engine\Graphics Engine\GE3DPlane.h (RECONSTRUCTION). Requires GE3DLine.h.
#ifndef LLE_GE3DPLANE_H
#define LLE_GE3DPLANE_H

class FSFile;

class LLE_API GE3DPlane {                   // sizeof 0x18
public:
    GE3DPlane();
    GE3DPlane(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3);
    GE3DPlane(const GE3DPoint& inPoint, const GEVector& inNormal);
    // copy ctor implicit: GEViewport's implicit copy ctor (0x1000e992) expands it (movsd point, GEVector copy)
    ~GE3DPlane() {}
    GE3DPlane& operator=(const GE3DPlane& inPlane);
    void Define(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3);
    void Define(const GE3DPoint& inPoint, const GEVector& inNormal) { this->mPoint = inPoint; this->mNormal = inNormal; }
    const GEVector& GetNormal() const { return this->mNormal; }
    const GE3DPoint& GetPoint() const { return this->mPoint; }
    bool Intersects(const GE3DPlane& inPlane, GE3DLine& outLine) const;
    bool IsPointAbovePlane(const GE3DPoint& inPoint) const;
    bool IsPointBelowPlane(const GE3DPoint& inPoint) const;
    bool IsPointOnOrAbovePlane(const GE3DPoint& inPoint) const;
    bool IsPointOnOrBelowPlane(const GE3DPoint& inPoint) const;
    bool IsPointOnPlane(const GE3DPoint& inPoint) const;
    long Load(FSFile& inFile);
    long Save(FSFile& inFile);
private:
    friend struct GERustLayout;             // the Rust mirror's layout check (Graphics Engine/GERustMath.h)
    GE3DPoint mPoint;                       // +0x00
    GEVector mNormal;                       // +0x0c
};

#endif
