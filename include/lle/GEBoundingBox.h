// C:\EEX\Low-Level Engine\Graphics Engine\GEBoundingBox.h (RECONSTRUCTION). GEBoundingBox.cpp 0x10013501..; the
// ctors, operator=, Define and the setters are inline (emitted first by GEBoundingBox.cpp itself, 0x10010550).
#ifndef LLE_GEBOUNDINGBOX_H
#define LLE_GEBOUNDINGBOX_H

class FSFileSpec;

class GEBoundingBox {               // sizeof 0x24
public:
    GEBoundingBox() : mTopRightBack(1.0f, 1.0f, 1.0f) { this->ComputeDimensions(); }
    GEBoundingBox(const GE3DPoint& inBottomLeftFront, const GE3DPoint& inTopRightBack)
        : mBottomLeftFront(0.0f, 0.0f, 0.0f), mTopRightBack(0.0f, 0.0f, 0.0f)
        { this->Define(inBottomLeftFront, inTopRightBack); }
    ~GEBoundingBox() {}
    GEBoundingBox& operator=(const GEBoundingBox& inBox)
    {
        this->mBottomLeftFront = inBox.mBottomLeftFront;
        this->mTopRightBack = inBox.mTopRightBack;
        this->ComputeDimensions();
        return *this;
    }
    // inside, with a 0.00001 tolerance on every face
    bool Contains(const GE3DPoint& inPoint) const { return ee_ge_bbox_contains(GERust(*this), GERust(inPoint)); }
    void Define(const GE3DPoint& inBottomLeftFront, const GE3DPoint& inTopRightBack)
    {
        this->mBottomLeftFront = inBottomLeftFront;
        this->mTopRightBack = inTopRightBack;
        this->ComputeDimensions();
    }
    const GE3DPoint& GetBottomLeftFront() const { return this->mBottomLeftFront; }
    float GetDepth() const { return this->mDepth; }
    float GetHeight() const { return this->mHeight; }
    const GE3DPoint& GetTopRightBack() const { return this->mTopRightBack; }
    float GetWidth() const { return this->mWidth; }
    // two boxes overlap (touching faces count); a line hits one of the six faces (outDistance: the nearest hit)
    bool Intersects(const GEBoundingBox& inBox) const { return ee_ge_bbox_intersects(GERust(*this), GERust(inBox)); }
    bool Intersects(const GE3DLine& inLine, float& outDistance) const
        { return ee_ge_bbox_intersects_line(GERust(*this), GERust(inLine), &outDistance); }
    // the two corners from the file (an FSFileSpec), then the size; the first read error
    template <class F> long Load(F& inFile)
    {
        long theResult = this->mBottomLeftFront.Load(inFile);
        if (theResult != kErrorOK || (theResult = this->mTopRightBack.Load(inFile)) != kErrorOK)
            return theResult;
        this->mWidth = this->mTopRightBack.GetX() - this->mBottomLeftFront.GetX();
        this->mHeight = this->mTopRightBack.GetZ() - this->mBottomLeftFront.GetZ();
        this->mDepth = this->mTopRightBack.GetY() - this->mBottomLeftFront.GetY();
        return theResult;
    }
    template <class F> long Save(F& inFile)
    {
        long theResult = this->mBottomLeftFront.Save(inFile);
        return theResult == kErrorOK ? this->mTopRightBack.Save(inFile) : theResult;
    }
    // the axis-aligned box around the eight transformed corners
    void Transform(const GETransformation& inTransformation, GEBoundingBox& outBox) const
        { ee_ge_bbox_transform(GERust(*this), GERust(inTransformation), GERust(outBox)); }
private:
    friend struct GERustLayout;             // the Rust mirror's layout check (Graphics Engine/GERustMath.h)
    // LineToPlaneIntersection and TransformPoint, Intersects(line)'s and Transform's helpers, are Rust (ee_ge bbox.rs)
    // each axis ordered (the first corner the minimum), then the width (x), height (z) and depth (y)
    void ComputeDimensions() { ee_ge_bbox_compute_dimensions(GERust(*this)); }

    GE3DPoint mBottomLeftFront;             // +0x00
    GE3DPoint mTopRightBack;                // +0x0c
    float mWidth;                           // +0x18 x extent
    float mHeight;                          // +0x1c z extent (GetHeight 0x1004c351)
    float mDepth;                           // +0x20 y extent (GetDepth 0x1004c369; y is depth in the engine)
};

#endif
