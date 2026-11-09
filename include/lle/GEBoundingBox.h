// C:\EEX\Low-Level Engine\Graphics Engine\GEBoundingBox.h (RECONSTRUCTION). GEBoundingBox.cpp 0x10013501..; the
// ctors, operator=, Define and the setters are inline (emitted first by GEBoundingBox.cpp itself, 0x10010550).
#ifndef LLE_GEBOUNDINGBOX_H
#define LLE_GEBOUNDINGBOX_H

class FSFileSpec;

class LLE_API GEBoundingBox {               // sizeof 0x24
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
    bool Contains(const GE3DPoint& inPoint) const;
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
    bool Intersects(const GEBoundingBox& inBox) const;
    bool Intersects(const GE3DLine& inLine, float& outDistance) const;
    long Load(FSFileSpec& inFile);
    long Save(FSFileSpec& inFile);
    void SetBottomLeftFront(const GE3DPoint& inPoint) { this->mBottomLeftFront = inPoint; this->ComputeDimensions(); }
    void SetTopRightBack(const GE3DPoint& inPoint) { this->mTopRightBack = inPoint; this->ComputeDimensions(); }
    void Transform(const GETransformation& inTransformation, GEBoundingBox& outBox) const;
private:
    friend struct GERustLayout;             // the Rust mirror's layout check (Graphics Engine/GERustMath.h)
    // LineToPlaneIntersection and TransformPoint, Intersects(line)'s and Transform's helpers, are Rust (ee_ge bbox.rs)
    void ComputeDimensions();

    GE3DPoint mBottomLeftFront;             // +0x00
    GE3DPoint mTopRightBack;                // +0x0c
    float mWidth;                           // +0x18 x extent
    float mHeight;                          // +0x1c z extent (GetHeight 0x1004c351)
    float mDepth;                           // +0x20 y extent (GetDepth 0x1004c369; y is depth in the engine)
};

#endif
