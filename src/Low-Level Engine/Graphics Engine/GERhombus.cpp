// C:\EEX\Low-Level Engine\Graphics Engine\GERhombus.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10024ea0..0x1002534b: the U2D key init chains, the ctor and ComputeRotation. Every other GERhombus member is
// header-inline (GEBoundingBox.cpp emits them first).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
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
#include "lle/GERhombus.h"
#include "GERustMath.h"

// FUNCTION: LLE 0x10024fc0
// Creates a quad of four white, opaque, camera-facing vertices from the active rasterizer, unrotated
GERhombus::GERhombus()
{
    GEColor theColor;
    GE3DPoint thePoint;
    GEVector theNormal(0.0f, 0.0f, -1.0f);
    this->mVertices = GERasterizer::GetActiveRasterizer()->CreateVertexArray(1, 4);
    this->mVertices->SetPoint(thePoint, 0);
    this->mVertices->SetPoint(thePoint, 1);
    this->mVertices->SetPoint(thePoint, 2);
    this->mVertices->SetPoint(thePoint, 3);
    this->mVertices->SetNormal(theNormal, 0);
    this->mVertices->SetNormal(theNormal, 1);
    this->mVertices->SetNormal(theNormal, 2);
    this->mVertices->SetNormal(theNormal, 3);
    this->mVertices->SetColor(theColor, 0);
    this->mVertices->SetColor(theColor, 1);
    this->mVertices->SetColor(theColor, 2);
    this->mVertices->SetColor(theColor, 3);
    this->mVertices->SetOpacity(1.0f, 0);
    this->mVertices->SetOpacity(1.0f, 1);
    this->mVertices->SetOpacity(1.0f, 2);
    this->mVertices->SetOpacity(1.0f, 3);
    this->mRotation = 0.0f;
    this->ComputeRotation();
}

// FUNCTION: LLE 0x10025106
// Recomputes the quad's corner vertices from its rectangle, rotated about the centre by its rotation angle
// (the corners are Rust: ee_ge bitmap.rs)
void GERhombus::ComputeRotation()
{
    GE2DPoint theCorners[4];
    GE3DPoint thePoints[4];
    ee_ge_rhombus_corners(GERust(this->mRectangle), this->mRotation, GERust(theCorners[0]));
    for (int i = 0; i < 4; i++) {
        this->mVertices->GetPoint(thePoints[i], i);
        thePoints[i].SetX(theCorners[i].GetX());
        thePoints[i].SetY(theCorners[i].GetY());
        this->mVertices->SetPoint(thePoints[i], i);
    }
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x10024ea0
// SYNTHETIC: LLE 0x10024eaa
// SYNTHETIC: LLE 0x10024eba
// SYNTHETIC: LLE 0x10024ec6
// SYNTHETIC: LLE 0x10024ed0
// SYNTHETIC: LLE 0x10024eda
// SYNTHETIC: LLE 0x10024eea
// SYNTHETIC: LLE 0x10024ef6
// SYNTHETIC: LLE 0x10024f00
// SYNTHETIC: LLE 0x10024f0a
// SYNTHETIC: LLE 0x10024f1a
// SYNTHETIC: LLE 0x10024f26
// SYNTHETIC: LLE 0x10024f30
// SYNTHETIC: LLE 0x10024f3a
// SYNTHETIC: LLE 0x10024f4a
// SYNTHETIC: LLE 0x10024f56
// SYNTHETIC: LLE 0x10024f60
// SYNTHETIC: LLE 0x10024f6a
// SYNTHETIC: LLE 0x10024f7a
// SYNTHETIC: LLE 0x10024f86
// SYNTHETIC: LLE 0x10024f90
// SYNTHETIC: LLE 0x10024f9a
// SYNTHETIC: LLE 0x10024faa
// SYNTHETIC: LLE 0x10024fb6
