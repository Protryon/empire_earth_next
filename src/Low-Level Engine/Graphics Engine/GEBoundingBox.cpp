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
// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x10013f5b
// TEMPLATE: LLE 0x10013fa5
// TEMPLATE: LLE 0x100142d5
// SYNTHETIC: LLE 0x1001255d
// SYNTHETIC: LLE 0x10013226
// SYNTHETIC: LLE 0x10013287
// SYNTHETIC: LLE 0x100132d5
// TEMPLATE: LLE 0x10013f27
// TEMPLATE: LLE 0x1001412e
// TEMPLATE: LLE 0x1001469d
// TEMPLATE: LLE 0x10014748
// TEMPLATE: LLE 0x100149a3
// TEMPLATE: LLE 0x10013b27
// TEMPLATE: LLE 0x1001443d
// TEMPLATE: LLE 0x10014453
// TEMPLATE: LLE 0x10014ade
// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*> COMDATs (GELOD.h inline dtors) and their sNodePool init pairs
// TEMPLATE: LLE 0x10014d03
// TEMPLATE: LLE 0x10014d28
// TEMPLATE: LLE 0x10014d4d
// TEMPLATE: LLE 0x10014da9
// TEMPLATE: LLE 0x10014de3
// TEMPLATE: LLE 0x10014e1d
// TEMPLATE: LLE 0x10014e57
// SYNTHETIC: LLE 0x10014eb6
// SYNTHETIC: LLE 0x10014ed4
// SYNTHETIC: LLE 0x10014eef
// SYNTHETIC: LLE 0x10014efb
// SYNTHETIC: LLE 0x10014f16
// SYNTHETIC: LLE 0x10014f22
// SYNTHETIC: LLE 0x10014f3d
// SYNTHETIC: LLE 0x10014f49
// TEMPLATE: LLE 0x10014f6e
// TEMPLATE: LLE 0x10014f90
// TEMPLATE: LLE 0x10014fed
// SYNTHETIC: LLE 0x10015015
// TEMPLATE: LLE 0x1001503a
// SYNTHETIC: LLE 0x1001505c
// TEMPLATE: LLE 0x10015081
// TEMPLATE: LLE 0x100150a3
// TEMPLATE: LLE 0x100150dd
// TEMPLATE: LLE 0x10015117

// ---- Header-inline members first emitted here (COMDATs; markers only)
// FUNCTION: LLE 0x10010550
// FUNCTION: LLE 0x10010579
// FUNCTION: LLE 0x100105ac
// FUNCTION: LLE 0x100105e2
// FUNCTION: LLE 0x10010613
// FUNCTION: LLE 0x1001062f
// FUNCTION: LLE 0x1001064c
// FUNCTION: LLE 0x1001068b
// SYNTHETIC: LLE 0x100106be
// FUNCTION: LLE 0x10010718
// FUNCTION: LLE 0x10010750
// FUNCTION: LLE 0x10010759
// FUNCTION: LLE 0x10010766
// FUNCTION: LLE 0x100107b7
// SYNTHETIC: LLE 0x100107fc
// FUNCTION: LLE 0x10010858
// FUNCTION: LLE 0x100108d1
// FUNCTION: LLE 0x1001093f
// FUNCTION: LLE 0x10010943
// FUNCTION: LLE 0x10010947
// FUNCTION: LLE 0x1001094b
// FUNCTION: LLE 0x1001094f
// FUNCTION: LLE 0x10010959
// FUNCTION: LLE 0x10010963
// FUNCTION: LLE 0x1001099f
// FUNCTION: LLE 0x100109c6
// FUNCTION: LLE 0x100109e9
// FUNCTION: LLE 0x10010a01
// FUNCTION: LLE 0x10010a2a
// FUNCTION: LLE 0x10010a53
// FUNCTION: LLE 0x10010a5d
// FUNCTION: LLE 0x10010af2
// FUNCTION: LLE 0x10010b13
// FUNCTION: LLE 0x10010b47
// FUNCTION: LLE 0x10010b6c
// FUNCTION: LLE 0x10010b90
// FUNCTION: LLE 0x10010bb1
// FUNCTION: LLE 0x10010bd2
// FUNCTION: LLE 0x10010bfc
// FUNCTION: LLE 0x10010c25
// FUNCTION: LLE 0x10010c4e
// FUNCTION: LLE 0x10010c78
// FUNCTION: LLE 0x10010c99
// FUNCTION: LLE 0x10010cb9
// FUNCTION: LLE 0x10010cea
// FUNCTION: LLE 0x10010cfe
// FUNCTION: LLE 0x10010d0b
// FUNCTION: LLE 0x10010d3d
// FUNCTION: LLE 0x10010d4c
// FUNCTION: LLE 0x10010d5b
// FUNCTION: LLE 0x10010d7c
// FUNCTION: LLE 0x10010d9d
// FUNCTION: LLE 0x10010daa
// FUNCTION: LLE 0x10010db7
// FUNCTION: LLE 0x10010dc4
// FUNCTION: LLE 0x10010dd3
// FUNCTION: LLE 0x10010de4
// FUNCTION: LLE 0x10010df5
// FUNCTION: LLE 0x10010dff
// FUNCTION: LLE 0x10010e6f
// FUNCTION: LLE 0x10010ed8
// SYNTHETIC: LLE 0x10010ee2
// FUNCTION: LLE 0x10010f3c
// FUNCTION: LLE 0x10010f49
// FUNCTION: LLE 0x10010f4d
// FUNCTION: LLE 0x10010f58
// FUNCTION: LLE 0x10010f63
// FUNCTION: LLE 0x10010f7e
// FUNCTION: LLE 0x10010f89
// FUNCTION: LLE 0x10010fc2
// FUNCTION: LLE 0x10010fe3
// FUNCTION: LLE 0x10010fed
// FUNCTION: LLE 0x10010ffd
// FUNCTION: LLE 0x1001100c
// FUNCTION: LLE 0x1001101c
// FUNCTION: LLE 0x1001102b
// FUNCTION: LLE 0x10011044
// FUNCTION: LLE 0x10011053
// FUNCTION: LLE 0x100110b0
// SYNTHETIC: LLE 0x10011154
// FUNCTION: LLE 0x10011176
// SYNTHETIC: LLE 0x10011181
// FUNCTION: LLE 0x100111d8
// FUNCTION: LLE 0x10011210
// FUNCTION: LLE 0x10011285
// FUNCTION: LLE 0x100112ce
// FUNCTION: LLE 0x100112d2
// FUNCTION: LLE 0x100112f3
// FUNCTION: LLE 0x10011306
// FUNCTION: LLE 0x10011316
// FUNCTION: LLE 0x1001131d
// FUNCTION: LLE 0x1001132b
// FUNCTION: LLE 0x10011332
// FUNCTION: LLE 0x10011339
// FUNCTION: LLE 0x10011349
// FUNCTION: LLE 0x10011359
// FUNCTION: LLE 0x1001135d
// FUNCTION: LLE 0x10011364
// FUNCTION: LLE 0x1001136b
// FUNCTION: LLE 0x1001137d
// FUNCTION: LLE 0x1001138d
// FUNCTION: LLE 0x10011394
// FUNCTION: LLE 0x1001139b
// FUNCTION: LLE 0x100113ab
// FUNCTION: LLE 0x100113bb
// FUNCTION: LLE 0x100113cb
// FUNCTION: LLE 0x100113de
// FUNCTION: LLE 0x100113f8
// FUNCTION: LLE 0x1001140a
// FUNCTION: LLE 0x1001141c
// FUNCTION: LLE 0x1001142b
// FUNCTION: LLE 0x1001143a
// FUNCTION: LLE 0x10011447
// FUNCTION: LLE 0x10011462
// FUNCTION: LLE 0x1001149e
// FUNCTION: LLE 0x100114c5
// FUNCTION: LLE 0x100114f3
// FUNCTION: LLE 0x1001150e
// FUNCTION: LLE 0x10011529
// FUNCTION: LLE 0x10011536
// FUNCTION: LLE 0x10011799
// SYNTHETIC: LLE 0x100119ed
// FUNCTION: LLE 0x10011a4e
// FUNCTION: LLE 0x10011a58
// FUNCTION: LLE 0x10011a74
// FUNCTION: LLE 0x10011aa5
// FUNCTION: LLE 0x10011afd
// FUNCTION: LLE 0x10011b11
// FUNCTION: LLE 0x10011b48
// FUNCTION: LLE 0x10011b58
// FUNCTION: LLE 0x10011b6c
// FUNCTION: LLE 0x10011b70
// FUNCTION: LLE 0x10011c0a
// FUNCTION: LLE 0x10011c23
// FUNCTION: LLE 0x10011c42
// FUNCTION: LLE 0x10011c8d
// FUNCTION: LLE 0x10011ccd
// FUNCTION: LLE 0x10011cd9
// FUNCTION: LLE 0x10011cea
// FUNCTION: LLE 0x10011d02
// FUNCTION: LLE 0x10011d19
// FUNCTION: LLE 0x10011d40
// FUNCTION: LLE 0x10011d50
// FUNCTION: LLE 0x10011d5a
// FUNCTION: LLE 0x10011d8e
// FUNCTION: LLE 0x10011d9b
// FUNCTION: LLE 0x10011dab
// FUNCTION: LLE 0x10011dbb
// FUNCTION: LLE 0x10011dcf
// FUNCTION: LLE 0x10011ddf
// FUNCTION: LLE 0x10011df5
// FUNCTION: LLE 0x10011e14
// FUNCTION: LLE 0x10011e26
// FUNCTION: LLE 0x10011e5e
// FUNCTION: LLE 0x10011e96

// ---- GEBoundingBox (0x10013501..)

// FUNCTION: LLE 0x10013501
// Orders each axis so the first corner holds the minimum, then recomputes width (X), height (Z) and depth (Y)
void GEBoundingBox::ComputeDimensions()
{
    ee_ge_bbox_compute_dimensions(GERust(*this));
}

// FUNCTION: LLE 0x1001356a
// Returns whether the point lies inside the box, with a 0.00001 tolerance on every face
bool GEBoundingBox::Contains(const GE3DPoint& inPoint) const
{
    return ee_ge_bbox_contains(GERust(*this), GERust(inPoint));
}

// FUNCTION: LLE 0x100135d8
// Returns whether the two boxes overlap (touching faces count)
bool GEBoundingBox::Intersects(const GEBoundingBox& inBox) const
{
    return ee_ge_bbox_intersects(GERust(*this), GERust(inBox));
}
// FUNCTION: LLE 0x10013631
// Tests the line against the box's six faces; returns whether it hits any and the nearest hit distance
bool GEBoundingBox::Intersects(const GE3DLine& inLine, float& outDistance) const
{
    return ee_ge_bbox_intersects_line(GERust(*this), GERust(inLine), &outDistance);
}

// (0x10013808 LineToPlaneIntersection: Intersects(line)'s helper, in Rust)

// FUNCTION: LLE 0x100138a6
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

// FUNCTION: LLE 0x100138ed
// Transforms all eight corners and sets outBox to the axis-aligned box around them
void GEBoundingBox::Transform(const GETransformation& inTransformation, GEBoundingBox& outBox) const
{
    ee_ge_bbox_transform(GERust(*this), GERust(inTransformation), GERust(outBox));
}

// (0x10013a65 TransformPoint: Transform's helper, in Rust)

// U2DPoint/U2DRectangle key init chains (wrapper, ctor, atexit registration, dtor per key)
// SYNTHETIC: LLE 0x10011ec3
// SYNTHETIC: LLE 0x10011ecd
// SYNTHETIC: LLE 0x10011edd
// SYNTHETIC: LLE 0x10011ee9
// SYNTHETIC: LLE 0x10011ef3
// SYNTHETIC: LLE 0x10011efd
// SYNTHETIC: LLE 0x10011f0d
// SYNTHETIC: LLE 0x10011f19
// SYNTHETIC: LLE 0x10011f23
// SYNTHETIC: LLE 0x10011f2d
// SYNTHETIC: LLE 0x10011f3d
// SYNTHETIC: LLE 0x10011f49
// SYNTHETIC: LLE 0x10011f53
// SYNTHETIC: LLE 0x10011f5d
// SYNTHETIC: LLE 0x10011f6d
// SYNTHETIC: LLE 0x10011f79
// SYNTHETIC: LLE 0x10011f83
// SYNTHETIC: LLE 0x10011f8d
// SYNTHETIC: LLE 0x10011f9d
// SYNTHETIC: LLE 0x10011fa9
// SYNTHETIC: LLE 0x10011fb3
// SYNTHETIC: LLE 0x10011fbd
// SYNTHETIC: LLE 0x10011fcd
// SYNTHETIC: LLE 0x10011fd9

// Header-inline/COMDAT members this unit emits (markers at the end)
// FUNCTION: LLE 0x10010938
// FUNCTION: LLE 0x1001093c
// FUNCTION: LLE 0x10010b8d
// FUNCTION: LLE 0x100112dc
// FUNCTION: LLE 0x10011cc9
// FUNCTION: LLE 0x10011ec0
// FUNCTION: LLE 0x10012548
// FUNCTION: LLE 0x10012ad2
// FUNCTION: LLE 0x1001340c
// FUNCTION: LLE 0x1001347f
// FUNCTION: LLE 0x10013cfa
// FUNCTION: LLE 0x10013d26
// FUNCTION: LLE 0x100144fb
// FUNCTION: LLE 0x100147e0
// FUNCTION: LLE 0x10014a5a
// FUNCTION: LLE 0x10014a9c
// FUNCTION: LLE 0x10014bef
// FUNCTION: LLE 0x1001482f
// TEMPLATE: LLE 0x10013b71
// TEMPLATE: LLE 0x10014b3c
// TEMPLATE: LLE 0x10014bd8

// Inline members of GEModelManager/GEMovie/GERhombus/GETextureSet/U2DFractal/GETerrainMesh/GEText and COMDATs (markers added late)
// FUNCTION: LLE 0x10011b84
// FUNCTION: LLE 0x10011bbe
// FUNCTION: LLE 0x10011be5
// FUNCTION: LLE 0x10011bf5
// FUNCTION: LLE 0x10011fe3
// FUNCTION: LLE 0x1001215a
// FUNCTION: LLE 0x10012302
// FUNCTION: LLE 0x10012426
// FUNCTION: LLE 0x1001245b
// FUNCTION: LLE 0x100124aa
// FUNCTION: LLE 0x100124c1
// FUNCTION: LLE 0x100124cb
// FUNCTION: LLE 0x100124f2
// FUNCTION: LLE 0x10012519
// FUNCTION: LLE 0x1001252d
// FUNCTION: LLE 0x100125b7
// FUNCTION: LLE 0x100125da
// FUNCTION: LLE 0x100125e1
// FUNCTION: LLE 0x10012604
// FUNCTION: LLE 0x1001260b
// FUNCTION: LLE 0x10012612
// FUNCTION: LLE 0x10012619
// FUNCTION: LLE 0x10012620
// FUNCTION: LLE 0x10012627
// FUNCTION: LLE 0x10012654
// FUNCTION: LLE 0x10012681
// FUNCTION: LLE 0x100126a9
// FUNCTION: LLE 0x100126b6
// FUNCTION: LLE 0x100126f5
// FUNCTION: LLE 0x1001270d
// FUNCTION: LLE 0x10012723
// FUNCTION: LLE 0x1001273b
// FUNCTION: LLE 0x10012751
// FUNCTION: LLE 0x10012758
// FUNCTION: LLE 0x1001275f
// FUNCTION: LLE 0x10012766
// FUNCTION: LLE 0x1001276d
// FUNCTION: LLE 0x10012783
// FUNCTION: LLE 0x10012792
// FUNCTION: LLE 0x10012799
// FUNCTION: LLE 0x100127af
// FUNCTION: LLE 0x100127b6
// FUNCTION: LLE 0x100127bd
// FUNCTION: LLE 0x100127d8
// FUNCTION: LLE 0x100127f3
// FUNCTION: LLE 0x10012814
// FUNCTION: LLE 0x10012842
// FUNCTION: LLE 0x10012865
// FUNCTION: LLE 0x1001286c
// FUNCTION: LLE 0x100128a4
// FUNCTION: LLE 0x100128ab
// FUNCTION: LLE 0x100128b2
// FUNCTION: LLE 0x100128bb
// FUNCTION: LLE 0x100128db
// FUNCTION: LLE 0x100128f1
// FUNCTION: LLE 0x100128f5
// FUNCTION: LLE 0x100128fc
// FUNCTION: LLE 0x10012903
// FUNCTION: LLE 0x10012925
// FUNCTION: LLE 0x10012959
// FUNCTION: LLE 0x10012996
// FUNCTION: LLE 0x100129af
// FUNCTION: LLE 0x100129c1
// FUNCTION: LLE 0x100129e5
// FUNCTION: LLE 0x10012a0a
// FUNCTION: LLE 0x10012a17
// FUNCTION: LLE 0x10012a43
// FUNCTION: LLE 0x10012aa1
// FUNCTION: LLE 0x10012aae
// FUNCTION: LLE 0x10012ab3
// FUNCTION: LLE 0x10012ae4
// FUNCTION: LLE 0x10012e85
// FUNCTION: LLE 0x1001337b
// FUNCTION: LLE 0x10013410
// FUNCTION: LLE 0x10013414
// FUNCTION: LLE 0x1001342c
// FUNCTION: LLE 0x10013464
// FUNCTION: LLE 0x1001346e
// FUNCTION: LLE 0x1001348f
// FUNCTION: LLE 0x1001448a
// FUNCTION: LLE 0x1001455b
// FUNCTION: LLE 0x1001462c
// FUNCTION: LLE 0x10014780
// FUNCTION: LLE 0x100147a8
// FUNCTION: LLE 0x10014802
// FUNCTION: LLE 0x10014b3c
// FUNCTION: LLE 0x10014b85
// FUNCTION: LLE 0x10014bbc
// FUNCTION: LLE 0x10014bd8
// FUNCTION: LLE 0x10014d72
// FUNCTION: LLE 0x10011ff0
// FUNCTION: LLE 0x10012026
// FUNCTION: LLE 0x10012061
// FUNCTION: LLE 0x10012117
// FUNCTION: LLE 0x1001215e
// FUNCTION: LLE 0x100121fc
// FUNCTION: LLE 0x10012231
// FUNCTION: LLE 0x1001226c
// FUNCTION: LLE 0x100122b7
// FUNCTION: LLE 0x10012325
// FUNCTION: LLE 0x10012334
// FUNCTION: LLE 0x1001235c
// FUNCTION: LLE 0x10013318
// FUNCTION: LLE 0x10013390
// FUNCTION: LLE 0x10013444
// FUNCTION: LLE 0x100134a9

// ---- Exported member with no address of its own (ICF-folded into GE3DPlane::Save 0xd54f)
// GEBoundingBox::Save writes the box's two corners to the file; returns the first write error
long GEBoundingBox::Save(FSFileSpec& inFile)
{
    long theResult = this->mBottomLeftFront.Save(inFile);
    if (theResult == kErrorOK)
        theResult = this->mTopRightBack.Save(inFile);
    return theResult;
}

// TEMPLATE: LLE 0x10013d8e
// TEMPLATE: LLE 0x10013dd5
// TEMPLATE: LLE 0x10014144
// TEMPLATE: LLE 0x1001428b
// TEMPLATE: LLE 0x10014711
// TEMPLATE: LLE 0x100147ec
// TEMPLATE: LLE 0x100148b4
// TEMPLATE: LLE 0x10014a28
// TEMPLATE: LLE 0x10014b10
// TEMPLATE: LLE 0x100145cc
// TEMPLATE: LLE 0x100148e6
// TEMPLATE: LLE 0x10014969
// TEMPLATE: LLE 0x100146fd
// TEMPLATE: LLE 0x10013d5a
