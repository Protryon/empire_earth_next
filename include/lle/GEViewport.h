// C:\EEX\Low-Level Engine\Graphics Engine\GEViewport.h (RECONSTRUCTION). GEViewport.cpp (~0x1003684c) holds the
// ctors, operator= and the matrix code; the accessors and the implicit copy ctor are inline (GEBitmap.cpp emits them
// first, 0x1000e78f..0x1000ebfa).
#ifndef LLE_GEVIEWPORT_H
#define LLE_GEVIEWPORT_H

#include <vector>
#include "lle/GERectangle.h"   // by-value members; GEBitmap.cpp emits GERectangle's inlines right before GEViewport's

class LLE_API GEViewport {                  // sizeof 0x288 (implicit copy ctor 0xe992)
public:
    GEViewport();
    GEViewport(const GERectangle& inViewRectangle, const GERectangle& inOrthographicRectangle,
               const GETransformation& inCamera, float inFrontClippingDistance, float inRearClippingDistance,
               float inFieldOfView);
    ~GEViewport() {}
    GEViewport& operator=(const GEViewport& inViewport);

    void DenormalizeOrthographicPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint) const
        { this->mOrthographicDenormalization.Apply(inPoint, outPoint); }
    void DenormalizePerspectivePoint(const GE3DPoint& inPoint, float inW, GE3DPoint& outPoint) const
    {
        GE3DPoint thePoint(inPoint.GetX() * inW, inPoint.GetY() * inW, inPoint.GetZ() * inW);
        this->mPerspectiveDenormalization.Apply(thePoint, outPoint);
    }
    float GetAspectRatioMultiplier() const { return this->mAspectRatioMultiplier; }
    const GE3DPlane& GetBottomPlane() const { return this->mBottomPlane; }
    const GETransformation& GetCamera() const { return this->mCamera; }
    const GETransformation& GetCameraInverse() const { return this->mCameraInverse; }
    const GEVector& GetCameraLookAtVector() const { return this->mCameraLookAtVector; }
    void GetDepthRange(float& outNear, float& outFar) const { outNear = this->mNearDepth; outFar = this->mFarDepth; }
    float GetFieldOfView() const { return this->mFieldOfView; }
    float GetFieldOfViewMultiplier() const { return this->mFieldOfViewMultiplier; }
    float GetFrontClippingDistance() const { return this->mFrontClippingDistance; }
    const GE3DPlane& GetFrontPlane() const { return this->mFrontPlane; }
    void GetFrustrumWorldPlanes(std::vector<GE3DPlane>& outPlanes, bool inOrthographic) const
        { this->GetFrustrumWorldPlanes(this->mViewRectangle, outPlanes, inOrthographic); }
    void GetFrustrumWorldPlanes(const GERectangle& inRectangle, std::vector<GE3DPlane>& outPlanes,
                                bool inOrthographic) const;
    void GetFrustrumWorldPoints(std::vector<GE3DPoint>& outPoints, bool inOrthographic) const
        { this->GetFrustrumWorldPoints(this->mViewRectangle, outPoints, inOrthographic); }
    void GetFrustrumWorldPoints(const GERectangle& inRectangle, std::vector<GE3DPoint>& outPoints,
                                bool inOrthographic) const;
    float GetHalfHeight() const { return this->mHalfHeight; }
    float GetHalfWidth() const { return this->mHalfWidth; }
    const GE3DPlane& GetLeftPlane() const { return this->mLeftPlane; }
    const GERectangle& GetOrthographicRectangle() const { return this->mOrthographicRectangle; }
    const GETransformation& GetOrthographicProjection() const { return this->mOrthographicProjection; }
    const GETransformation& GetPerspectiveProjection() const { return this->mPerspectiveProjection; }
    float GetRearClippingDistance() const { return this->mRearClippingDistance; }
    const GE3DPlane& GetRearPlane() const { return this->mRearPlane; }
    const GE3DPlane& GetRightPlane() const { return this->mRightPlane; }
    const GE3DPlane& GetTopPlane() const { return this->mTopPlane; }
    const GERectangle& GetViewRectangle() const { return this->mViewRectangle; }
    float GetXCenter() const { return this->mXCenter; }
    float GetYCenter() const { return this->mYCenter; }
    float GetYClippingPlane() const { return this->mYClippingPlane; }
    void NormalizeOrthographicCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint, float& outW)
    {
        outPoint.SetX(inPoint.GetX() * this->mOrthographicScale);
        outPoint.SetY(inPoint.GetZ() * this->mOrthographicScale);
        outPoint.SetZ(inPoint.GetY() * this->mOrthographicDepthScale + this->mOrthographicDepthOffset);
        outW = 1.0f;
    }
    void NormalizePerspectiveCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint, float& outW)
    {
        outPoint.SetX(inPoint.GetX() * this->mPerspectiveScale);
        outPoint.SetY(inPoint.GetZ() * this->mPerspectiveScale);
        outPoint.SetZ(inPoint.GetY() * this->mPerspectiveDepthScale + this->mPerspectiveDepthOffset);
        outW = inPoint.GetY();
    }
    void SetCamera(const GETransformation& inCamera)
    {
        this->mCamera = inCamera;
        this->mCamera.Apply(GEViewport::sLocalLookAtVector, this->mCameraLookAtVector);
        this->mCamera.MakeInverse(this->mCameraInverse);
        this->ComputeOrthographicMatrix();
        this->ComputePerspectiveMatrix();
    }
    void SetDepthRange(float inNear, float inFar) { this->mNearDepth = inNear; this->mFarDepth = inFar; }
    void SetFieldOfView(float inFieldOfView);
    void SetFrontClippingDistance(float inDistance);
    void SetOrthographicRectangle(const GERectangle& inRectangle)
    {
        this->mOrthographicRectangle = inRectangle;
        this->ComputeOrthographicMatrix();
    }
    void SetRearClippingDistance(float inDistance);
    void SetViewRectangle(const GERectangle& inRectangle);
private:
    friend struct GERustViewportLayout;     // the Rust mirror's layout check (Graphics Engine/GERustViewport.h)
    void ComputeOrthographicMatrix();
    void ComputePerspectiveMatrix();
    void ComputePlanes();

    GERectangle mOrthographicRectangle;     // +0x000
    GERectangle mViewRectangle;             // +0x010
    GE3DPlane mFrontPlane;                  // +0x020
    GE3DPlane mRearPlane;                   // +0x038
    GE3DPlane mLeftPlane;                   // +0x050
    GE3DPlane mRightPlane;                  // +0x068
    GE3DPlane mTopPlane;                    // +0x080
    GE3DPlane mBottomPlane;                 // +0x098
    GETransformation mCamera;               // +0x0b0
    GETransformation mCameraInverse;        // +0x0f0
    GETransformation mOrthographicDenormalization;   // +0x130
    GETransformation mPerspectiveDenormalization;    // +0x170
    GETransformation mOrthographicProjection;        // +0x1b0
    GETransformation mPerspectiveProjection;         // +0x1f0
    GEVector mCameraLookAtVector;           // +0x230
    float mOrthographicScale;               // +0x23c provisional names from here
    float mOrthographicDepthScale;          // +0x240
    float mOrthographicDepthOffset;         // +0x244
    float mPerspectiveScale;                // +0x248
    float mPerspectiveDepthScale;           // +0x24c
    float mPerspectiveDepthOffset;          // +0x250
    float mAspectRatio;                     // +0x254 view width / height
    float mAspectRatioMultiplier;           // +0x258
    float mFieldOfView;                     // +0x25c
    float mFieldOfViewMultiplier;           // +0x260
    float mFrontClippingDistance;           // +0x264
    float mHalfHeight;                      // +0x268
    float mHalfWidth;                       // +0x26c
    float mRearClippingDistance;            // +0x270
    float mXCenter;                         // +0x274
    float mYCenter;                         // +0x278
    float mYClippingPlane;                  // +0x27c
    float mNearDepth;                       // +0x280
    float mFarDepth;                        // +0x284

    static GEVector sLocalLookAtVector;     // exported ?sLocalLookAtVector@GEViewport@@0VGEVector@@A
};

#endif
