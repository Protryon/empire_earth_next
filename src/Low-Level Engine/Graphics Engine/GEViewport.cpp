// C:\EEX\Low-Level Engine\Graphics Engine\GEViewport.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// 0x1003680c..0x10038020: sLocalLookAtVector's init chain (GEVector's empty dtor leaves a guard-only atexit stub),
// the out-of-line GEViewport members, then the vector<GE3DPlane> COMDATs GetFrustrumWorldPlanes pulls in (resize,
// insert, erase and their helpers). The accessors and the implicit copy ctor are header-inline (GEBitmap.cpp).
//
// The matrices, the clipping planes and the frustum are Rust (rust/crates/ee_ge viewport.rs, docs/RUST_PLAN.md
// R4.3), computed on the object itself through its mirror (GERustViewport.h pins the layout); the members forward.
#include <windows.h>
#include "lle/LowLevelEngineCore.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GEViewport.h"
#include "GERustMath.h"
#include "GERustViewport.h"

// (kMinimumFrustrumExtent 0x100980f4, the smallest frustum extent, is in Rust)

GEVector GEViewport::sLocalLookAtVector(0.0f, 1.0f, 0.0f);

// Creates a default viewport: identity camera, 90-degree field of view, clipping from 1 to 10
GEViewport::GEViewport()
{
    ee_ge_viewport_default(GERust(*this));
}

// Creates a viewport for a screen rectangle and camera, computing its projections and clipping planes
GEViewport::GEViewport(const GERectangle& inViewRectangle, const GERectangle& inOrthographicRectangle,
                       const GETransformation& inCamera, float inFrontClippingDistance,
                       float inRearClippingDistance, float inFieldOfView)
{
    ee_ge_viewport_init(GERust(*this), GERust(inViewRectangle), GERust(inOrthographicRectangle), GERust(inCamera),
                        inFrontClippingDistance, inRearClippingDistance, inFieldOfView);
}

// Copies every field of another viewport (rectangles, planes, camera, matrices, projection factors)
GEViewport& GEViewport::operator=(const GEViewport& inViewport)
{
    ee_ge_viewport_copy(GERust(*this), GERust(inViewport));
    return *this;
}

// Recomputes the orthographic projection and its screen-to-world matrix (skipped with no rear clipping distance)
void GEViewport::ComputeOrthographicMatrix()
{
    ee_ge_viewport_orthographic_matrix(GERust(*this));
}

// Recomputes the perspective projection and its screen-to-world matrix (skipped with no rear clipping distance)
void GEViewport::ComputePerspectiveMatrix()
{
    ee_ge_viewport_perspective_matrix(GERust(*this));
}

// Returns the six world-space planes of the frustum under a screen rectangle (perspective or orthographic)
void GEViewport::GetFrustrumWorldPlanes(const GERectangle& inRectangle, std::vector<GE3DPlane>& outPlanes,
                                        bool inOrthographic) const
{
    outPlanes.resize(6);
    ee_ge_viewport_frustum_planes(GERust(*this), GERust(inRectangle), inOrthographic, GERust(outPlanes[0]));
}

// Appends the eight world-space corners (near and far per screen corner) of the frustum under a screen
// rectangle, widened to at least 2 pixels each way
void GEViewport::GetFrustrumWorldPoints(const GERectangle& inRectangle, std::vector<GE3DPoint>& outPoints,
                                        bool inOrthographic) const
{
    GE3DPoint thePoints[8];
    ee_ge_viewport_frustum_points(GERust(*this), GERust(inRectangle), inOrthographic, GERust(thePoints[0]));
    for (int k = 0; k < 8; k++)
        outPoints.push_back(thePoints[k]);
}

// Changes the field of view and recomputes the perspective and the clipping planes
void GEViewport::SetFieldOfView(float inFieldOfView)
{
    ee_ge_viewport_set_field_of_view(GERust(*this), inFieldOfView);
}

// Changes the front clipping distance (pushing the rear one back if needed) and recomputes everything
void GEViewport::SetFrontClippingDistance(float inDistance)
{
    ee_ge_viewport_set_front(GERust(*this), inDistance);
}

// Changes the rear clipping distance (pulling the front one in if needed) and recomputes everything
void GEViewport::SetRearClippingDistance(float inDistance)
{
    ee_ge_viewport_set_rear(GERust(*this), inDistance);
}

// Changes the screen rectangle, its centre and aspect ratio and the clipping planes and
// the projection matrices
void GEViewport::SetViewRectangle(const GERectangle& inRectangle)
{
    ee_ge_viewport_set_view_rectangle(GERust(*this), GERust(inRectangle));
}
