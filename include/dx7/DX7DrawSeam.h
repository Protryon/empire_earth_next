// The C++ seam of the rasterizer's draw paths moved to Rust (rust/crates/ee_dx7/src/draw*.rs): the per-screen pinned
// record and the access DX7Rasterizer's own forwarders and the ee_dx7cpp_* functions need.
#ifndef DX7_DX7DRAWSEAM_H
#define DX7_DX7DRAWSEAM_H

#include "rust/ee_rust.h"
#include "dx7/DX7Rasterizer.h"

// The rasterizer's protected members, for the forwarders and the ee_dx7cpp_* seam (a view of the same object: no data
// of its own).
struct DX7DrawAccess : public DX7Rasterizer {
    using DX7Rasterizer::mDX7ScreenStates;
    using DX7Rasterizer::Set2DMode;
    using DX7Rasterizer::Set3DMode;
    using DX7Rasterizer::LockVB;
    using DX7Rasterizer::UnlockVB;
    using GERasterizer::IsOrthographicModelVisible;
    using GERasterizer::IsPerspectiveModelVisible;
    using GERasterizer::mDrawingAccess;
    using GERasterizer::mScreenStates;
    using GERasterizer::mComputedAmbientColor;
    using GERasterizer::mWaterRipple0;
    using GERasterizer::mWaterRipple1;
    using GERasterizer::mWaterRipple2;
};

// The screen's pinned DX7 state record (DX7Trampolines.cpp pins EeDx7ScreenState to DX7RScreenState).
inline EeDx7ScreenState* DX7DrawState(DX7Rasterizer* inRasterizer, unsigned long inScreen)
{
    return reinterpret_cast<EeDx7ScreenState*>(static_cast<DX7DrawAccess*>(inRasterizer)->mDX7ScreenStates[inScreen]);
}

static inline DX7DrawAccess* DX7Access(void* inRasterizer) { return static_cast<DX7DrawAccess*>(static_cast<DX7Rasterizer*>(inRasterizer)); }

static inline void DX7CopyTransform(const GETransformation& inTransformation, float outMatrix[4][4])
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            outMatrix[r][c] = inTransformation.GetElement(r, c);
}

static inline void DX7CopyPlane(const GE3DPlane& inPlane, EeDx7Plane* outPlane)
{
    outPlane->point[0] = inPlane.GetPoint().GetX();
    outPlane->point[1] = inPlane.GetPoint().GetY();
    outPlane->point[2] = inPlane.GetPoint().GetZ();
    outPlane->normal[0] = inPlane.GetNormal().GetX();
    outPlane->normal[1] = inPlane.GetNormal().GetY();
    outPlane->normal[2] = inPlane.GetNormal().GetZ();
}

// The viewport as the Rust 3D draws read it (plain data, valid only during the forwarded call).
static inline EeDx7ViewportInfo DX7ViewportInfoOf(GEViewport& inViewport)
{
    EeDx7ViewportInfo theInfo;
    DX7CopyTransform(inViewport.GetCamera(), theInfo.camera);
    DX7CopyTransform(inViewport.GetCameraInverse(), theInfo.camera_inverse);
    DX7CopyTransform(inViewport.GetPerspectiveProjection(), theInfo.perspective);
    DX7CopyTransform(inViewport.GetOrthographicProjection(), theInfo.orthographic);
    theInfo.front_clip = inViewport.GetFrontClippingDistance();
    theInfo.rear_clip = inViewport.GetRearClippingDistance();
    theInfo.half_width = inViewport.GetHalfWidth();
    theInfo.half_height = inViewport.GetHalfHeight();
    theInfo.field_of_view = inViewport.GetFieldOfView();
    theInfo.y_clip_plane = inViewport.GetYClippingPlane();
    theInfo.ortho_left = inViewport.GetOrthographicRectangle().GetLeft();
    theInfo.ortho_top = inViewport.GetOrthographicRectangle().GetTop();
    theInfo.ortho_right = inViewport.GetOrthographicRectangle().GetRight();
    theInfo.ortho_bottom = inViewport.GetOrthographicRectangle().GetBottom();
    DX7CopyPlane(inViewport.GetLeftPlane(), &theInfo.left_plane);
    DX7CopyPlane(inViewport.GetRightPlane(), &theInfo.right_plane);
    DX7CopyPlane(inViewport.GetTopPlane(), &theInfo.top_plane);
    DX7CopyPlane(inViewport.GetBottomPlane(), &theInfo.bottom_plane);
    return theInfo;
}

#endif
