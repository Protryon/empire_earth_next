// C:\EEX\Low-Level Engine\Graphics Engine\GEBoundingBox.h (RECONSTRUCTION). GEBoundingBox.cpp 0x10013501..; the
// ctors, operator=, Define and the setters are inline (emitted first by GEBoundingBox.cpp itself, 0x10010550).
#ifndef LLE_GEBOUNDINGBOX_H
#define LLE_GEBOUNDINGBOX_H
#include "lle/GERust.h"

#include "lle/LLEForward.h"

class GEBoundingBox : public EeGeBoundingBox {   // the Rust record (mBottomLeftFront, mTopRightBack, mWidth (x),
public:                                          // mHeight (z), mDepth (y); rust/base/math.h)
    GEBoundingBox() : EeGeBoundingBox{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 0.0f, 0.0f, 0.0f} { this->ComputeDimensions(); }
    GEBoundingBox(const GE3DPoint& inBottomLeftFront, const GE3DPoint& inTopRightBack) : EeGeBoundingBox{}
        { this->Define(inBottomLeftFront, inTopRightBack); }
    // the two corners, then the extents computed again (ee_ge_bbox_define)
    GEBoundingBox& operator=(const GEBoundingBox& inBox)
        { return *static_cast<GEBoundingBox*>(ee_ge_bbox_define(this, &inBox.mBottomLeftFront, &inBox.mTopRightBack)); }
    void Define(const GE3DPoint& inBottomLeftFront, const GE3DPoint& inTopRightBack) { ee_ge_bbox_define(this, &inBottomLeftFront, &inTopRightBack); }
    const GE3DPoint& GetBottomLeftFront() const { return GECpp(this->mBottomLeftFront); }
    float GetDepth() const { return this->mDepth; }
    float GetHeight() const { return this->mHeight; }
    const GE3DPoint& GetTopRightBack() const { return GECpp(this->mTopRightBack); }
    float GetWidth() const { return this->mWidth; }
    // the two corners from the file (an FSFileSpec), then the size; the first read error
    template <class F> long Load(F& inFile) { return ee_ge_bbox_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) { return ee_ge_bbox_save(GERust(*this), GERustFile(inFile)); }
    // the axis-aligned box around the eight transformed corners
    void Transform(const GETransformation& inTransformation, GEBoundingBox& outBox) const
        { ee_ge_bbox_transform(GERust(*this), GERust(inTransformation), GERust(outBox)); }
    // (Contains, the two Intersects, LineToPlaneIntersection and TransformPoint are Rust's alone: ee_ge bbox.rs)
    // each axis ordered (the first corner the minimum), then the width (x), height (z) and depth (y)
    void ComputeDimensions() { ee_ge_bbox_compute_dimensions(GERust(*this)); }
};

#endif
