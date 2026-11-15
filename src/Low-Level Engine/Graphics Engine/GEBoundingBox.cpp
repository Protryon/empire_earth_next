// C:\EEX\Low-Level Engine\Graphics Engine\GEBoundingBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10010550..0x10015151. Like GEBitmap.cpp, the unit opens with the header-inline members of every class it is the
// first to include (GEBoundingBox, GEFontBitmap, GEFont, GELight, GELine, GEPolygonGroup, GEMaterial, GEPolygon,
// GEPolygonArray, GEVertex, GEModel, GELOD*, GEModelManager, GEMovie, GEPhysicalModel, GEPoint, GEQuaternion, the
// U2D key statics 0x10011ec3, GERhombus, GETextureSet, U2DFractal, GETerrainMesh, GEText; 0x10010550..0x10013501),
// then its own code (0x10013501) and the template COMDATs (vectors, UList memory pools; 0x10013b27..).
// GEBoundingBox's members forward to Rust (rust/crates/ee_ge bbox.rs; GERustMath.h), except the file I/O.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/URandom.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/FSFileSpec.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GE2DVertex.h"
#include "lle/GEBitmap.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEFont.h"
#include "lle/GELight.h"
#include "lle/GELine.h"
#include "lle/GEPolygonGroup.h"
#include "lle/GEMaterial.h"
#include "lle/GEPolygon.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEVertex.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEMovie.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GEPoint.h"
#include "lle/GEQuaternion.h"
#include "lle/GERhombus.h"
#include "lle/GETextureSet.h"
#include "lle/U2DFractal.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEText.h"
#include "GERustMath.h"

// ---- GEBoundingBox (0x10013501..)

// Orders each axis so the first corner holds the minimum, then recomputes width (X), height (Z) and depth (Y)
void GEBoundingBox::ComputeDimensions()
{
    ee_ge_bbox_compute_dimensions(GERust(*this));
}

// Returns whether the point lies inside the box, with a 0.00001 tolerance on every face
bool GEBoundingBox::Contains(const GE3DPoint& inPoint) const
{
    return ee_ge_bbox_contains(GERust(*this), GERust(inPoint));
}

// Returns whether the two boxes overlap (touching faces count)
bool GEBoundingBox::Intersects(const GEBoundingBox& inBox) const
{
    return ee_ge_bbox_intersects(GERust(*this), GERust(inBox));
}
// Tests the line against the box's six faces; returns whether it hits any and the nearest hit distance
bool GEBoundingBox::Intersects(const GE3DLine& inLine, float& outDistance) const
{
    return ee_ge_bbox_intersects_line(GERust(*this), GERust(inLine), &outDistance);
}

// (0x10013808 LineToPlaneIntersection: Intersects(line)'s helper, in Rust)

// Reads the box's two corners from the file and recomputes its size; returns the first read error
long GEBoundingBox::Load(FSFileSpec& inFile)
{
    long theResult = this->mBottomLeftFront.Load(inFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mTopRightBack.Load(inFile);
    if (theResult != kErrorOK)
        return theResult;
    this->mWidth = this->mTopRightBack.GetX() - this->mBottomLeftFront.GetX();
    this->mHeight = this->mTopRightBack.GetZ() - this->mBottomLeftFront.GetZ();
    this->mDepth = this->mTopRightBack.GetY() - this->mBottomLeftFront.GetY();
    return theResult;
}

// Transforms all eight corners and sets outBox to the axis-aligned box around them
void GEBoundingBox::Transform(const GETransformation& inTransformation, GEBoundingBox& outBox) const
{
    ee_ge_bbox_transform(GERust(*this), GERust(inTransformation), GERust(outBox));
}

// (0x10013a65 TransformPoint: Transform's helper, in Rust)

// ---- Exported member with no address of its own (ICF-folded into GE3DPlane::Save 0xd54f)
// GEBoundingBox::Save writes the box's two corners to the file; returns the first write error
long GEBoundingBox::Save(FSFileSpec& inFile)
{
    long theResult = this->mBottomLeftFront.Save(inFile);
    if (theResult == kErrorOK)
        theResult = this->mTopRightBack.Save(inFile);
    return theResult;
}
