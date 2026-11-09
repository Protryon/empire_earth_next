// C:\EEX\Low-Level Engine\Graphics Engine\GEPhysicalModel.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10020e09..0x10022fca: a model placed in the world (location, scale, frame): ctor, dtor, operator=, the four
// Intersects (line, line with scale, model against model, viewport), a file-static triangle-box helper, SetModel, then
// the vector<vector<GEBoundingBox> >/vector<GEBoundingBox> COMDATs and the UList<GELOD*> pool init pair. No U2D keys.
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
#include "lle/GEMaterial.h"
#include "lle/GEVertexArray.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEModel.h"
#include "lle/GEPhysicalModel.h"
#include "GERustMath.h"

static const float kLODFraction = 0.2f;   // 0x10097b78 (unit-local) (provisional name)
// defined after its user (0x1002179e) in address order
static void ComputeTriangleBox(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3,
                               GEBoundingBox& outBox);

// FUNCTION: LLE 0x10020e09
// Places a model in the world at a location and scale, holding a reference to it, at frame 0
GEPhysicalModel::GEPhysicalModel(GEModel* inModel, const GETransformation& inLocation, float inScale)
{
    this->mModel = inModel;
    if (inModel)
        inModel->Reference();
    this->mLocation = inLocation;
    this->mScale = inScale;
    this->mFrame = 0;
}

// FUNCTION: LLE 0x10020e3b
// Destroys the placed model, releasing its model
GEPhysicalModel::~GEPhysicalModel()
{
    if (this->mModel)
        this->mModel->Dereference();
}

// FUNCTION: LLE 0x10020e48
// Copies another placed model (model reference, location, scale, frame)
GEPhysicalModel& GEPhysicalModel::operator=(const GEPhysicalModel& inModel)
{
    if (this->mModel)
        this->mModel->Dereference();
    this->mModel = inModel.mModel;
    if (this->mModel)
        this->mModel->Reference();
    this->mLocation = inModel.mLocation;
    this->mScale = inModel.mScale;
    this->mFrame = inModel.mFrame;
    return *this;
}

// FUNCTION: LLE 0x10020e84
// Tests a line against a model and its sub-models: bounding sphere, then box (LOD > 0), then triangles (LOD > 1);
// with inExact the hit must lie on the segment
bool GEPhysicalModel::Intersects(const GETransformation& inLocation, const GEModel* inModel, unsigned long inFrame,
                                 const GE3DLine& inLine, unsigned long inLOD, float& outDistance, bool inExact) const
{
    GE3DPoint thePoint;
    GE3DPoint thePoint1;
    GE3DPoint thePoint2;
    GE3DPoint thePoint3;
    GEBoundingBox theBox;
    GETransformation theTransform(false);
    bool theResult = false;
    if (this->mScale < -0.0001f || this->mScale > 0.0001f) {
        if (inModel) {
            if (inModel->GetParentModel() == NULL) {
                inLocation.Apply(inModel->GetInternalTransform(inFrame), theTransform);
                if (this->mScale != 1.0f) {
                    theTransform.Scale(this->mScale);
                    theTransform.SetElement(0, 3, inLocation.GetElement(0, 3));
                    theTransform.SetElement(1, 3, inLocation.GetElement(1, 3));
                    theTransform.SetElement(2, 3, inLocation.GetElement(2, 3));
                }
            } else
                inLocation.Apply(inModel->GetInternalTransform(inFrame), theTransform);
            thePoint = inModel->GetCenterPoint();
            theTransform.Apply(thePoint, thePoint1);
            theResult = inLine.Intersects(thePoint1, inModel->GetBoundingRadius(inFrame) * this->mScale, outDistance);
            if (inExact)
                theResult = theResult && outDistance >= 0.0f && outDistance <= 1.0f;
            if (theResult && inLOD > 0) {
                inModel->GetBoundingBox(inFrame).Transform(theTransform, theBox);
                theResult = theBox.Intersects(inLine, outDistance);
                if (inExact)
                    theResult = theResult && outDistance >= 0.0f && outDistance <= 1.0f;
                if (theResult && inLOD > 1) {
                    theResult = false;
                    unsigned long theMaterialCount = inModel->GetMaterialCount();
                    for (unsigned long i = 0; i < theMaterialCount && !theResult; i++) {
                        GEMaterial* theMaterial = inModel->GetMaterial(i);
                        unsigned long theVertexStart = theMaterial->GetVertexStartingIndex();
                        unsigned long theEnd = theMaterial->GetPolygonStartingIndex(0) + theMaterial->GetPolygonRangeLength(0);
                        for (unsigned long j = theMaterial->GetPolygonStartingIndex(0); j < theEnd && !theResult; j++) {
                            inModel->mVertexArrays[inFrame]->GetPoint(thePoint,
                                inModel->mPolygonArrays[0]->GetVertexIndex1(j) + theVertexStart);
                            theTransform.Apply(thePoint, thePoint1);
                            inModel->mVertexArrays[inFrame]->GetPoint(thePoint,
                                inModel->mPolygonArrays[0]->GetVertexIndex2(j) + theVertexStart);
                            theTransform.Apply(thePoint, thePoint2);
                            inModel->mVertexArrays[inFrame]->GetPoint(thePoint,
                                inModel->mPolygonArrays[0]->GetVertexIndex3(j) + theVertexStart);
                            theTransform.Apply(thePoint, thePoint3);
                            // the triangle's plane, the crossing, the opposite edge (ee_ge physical.rs)
                            theResult = ee_ge_physical_line_triangle(GERust(inLine), GERust(thePoint1),
                                                                     GERust(thePoint2), GERust(thePoint3), inExact,
                                                                     &outDistance);
                        }
                    }
                }
            }
            unsigned long theSubModelCount = inModel->GetSubModelCount();
            for (unsigned long k = 0; k < theSubModelCount && !theResult; k++)
                theResult = this->Intersects(theTransform, inModel->GetSubModel(k), inFrame, inLine, inLOD, outDistance, inExact);
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10021404
// Tests a model scaled per axis against this placed model, sub-models included;
// in exact mode a top model with no hit retries as a uniformly scaled line test
bool GEPhysicalModel::Intersects(const GETransformation& inLocation, const GEModel* inModel, unsigned long inFrame,
                                 const GE3DLine& inLine, bool inExact, unsigned long inLOD, float inX, float inY,
                                 float inZ) const
{
    GE3DPoint thePoint;
    GE3DPoint theOrigin;
    GETransformation theTransform(false);
    GETransformation theInverse(false);
    float theDistance;
    unsigned long theSubModelCount = inModel->GetSubModelCount();
    bool theResult = false;
    if ((this->mScale < -0.0001f || this->mScale > 0.0001f) && (inX < -0.0001f || inX > 0.0001f) &&
        (inY < -0.0001f || inY > 0.0001f) && (inZ < -0.0001f || inZ > 0.0001f)) {
        if (inModel->GetParentModel() == NULL) {
            inLocation.Apply(inModel->GetInternalTransform(inFrame), theTransform);
            if (inX != 1.0f || inY != 1.0f || inZ != 1.0f) {
                theTransform.Scale(inX, inY, inZ);
                theTransform.SetElement(0, 3, inLocation.GetElement(0, 3));
                theTransform.SetElement(1, 3, inLocation.GetElement(1, 3));
                theTransform.SetElement(2, 3, inLocation.GetElement(2, 3));
            }
        } else
            inLocation.Apply(inModel->GetInternalTransform(inFrame), theTransform);
        float theInverseX = 1.0f / (inX * inX);
        float theInverseY = 1.0f / (inY * inY);
        float theInverseZ = 1.0f / (inZ * inZ);
        theInverse.SetElement(0, 0, theTransform.GetElement(0, 0) * theInverseX);
        theInverse.SetElement(0, 1, theTransform.GetElement(1, 0) * theInverseY);
        theInverse.SetElement(0, 2, theTransform.GetElement(2, 0) * theInverseZ);
        theInverse.SetElement(0, 3, 0.0f);
        theInverse.SetElement(1, 0, theTransform.GetElement(0, 1) * theInverseX);
        theInverse.SetElement(1, 1, theTransform.GetElement(1, 1) * theInverseY);
        theInverse.SetElement(1, 2, theTransform.GetElement(2, 1) * theInverseZ);
        theInverse.SetElement(1, 3, 0.0f);
        theInverse.SetElement(2, 0, theTransform.GetElement(0, 2) * theInverseX);
        theInverse.SetElement(2, 1, theTransform.GetElement(1, 2) * theInverseY);
        theInverse.SetElement(2, 2, theTransform.GetElement(2, 2) * theInverseZ);
        theInverse.SetElement(2, 3, 0.0f);
        theInverse.SetElement(3, 0, 0.0f);
        theInverse.SetElement(3, 1, 0.0f);
        theInverse.SetElement(3, 2, 0.0f);
        theInverse.SetElement(3, 3, 1.0f);
        thePoint.SetX(-theTransform.GetElement(0, 3));
        thePoint.SetY(-theTransform.GetElement(1, 3));
        thePoint.SetZ(-theTransform.GetElement(2, 3));
        theInverse.Apply(thePoint, theOrigin);
        theInverse.SetElement(0, 3, theOrigin.GetX());
        theInverse.SetElement(1, 3, theOrigin.GetY());
        theInverse.SetElement(2, 3, theOrigin.GetZ());
        float theMaximum = max(inX, inY);
        theResult = this->Intersects(this->mLocation, this->mModel, theTransform, theInverse, max(theMaximum, inZ), inModel, inFrame,
                               inLOD);
        for (unsigned long i = 0; !theResult && i < theSubModelCount; i++)
            theResult = this->Intersects(theTransform, inModel->GetSubModel(i), inFrame, inLine, inExact, inLOD, inX, inY, inZ);
    }
    if (theResult || !inExact || inModel->GetParentModel() != NULL)
        return theResult;
    float theScale = inX;
    theScale = min(inY, theScale);
    theScale = min(inZ, theScale);
    if (theScale < -0.0001f || theScale > 0.0001f) {
        GEPhysicalModel theModel(const_cast<GEModel*>(inModel), inLocation, theScale);
        theModel.SetFrame(inFrame);
        theResult = theModel.Intersects(inLine, inLOD, theDistance, true);
    }
    return theResult;
}

// FUNCTION: LLE 0x1002179e
// Tests this placed model against another model: bounding spheres, then boxes (LOD > 0),
// then triangle boxes at a reduced LOD (LOD > 1); recurses into this model's sub-models
bool GEPhysicalModel::Intersects(const GETransformation& inLocation, const GEModel* inModel,
                                 const GETransformation& inOtherLocation, const GETransformation& inOtherInverse,
                                 float inScale, const GEModel* inOtherModel, unsigned long inFrame,
                                 unsigned long inLOD) const
{
    std::vector<std::vector<GEBoundingBox> > theBoxes;
    GE3DPoint theCenter;
    GE3DPoint thePoint;
    GE3DPoint theOtherPoint;
    GE3DPoint thePoint1;
    GE3DPoint thePoint2;
    GE3DPoint thePoint3;
    GE3DPoint theTransformed1;
    GE3DPoint theTransformed2;
    GE3DPoint theTransformed3;
    GEBoundingBox theBox;
    GEBoundingBox theTriangleBox;
    GETransformation theTransform(false);
    GETransformation theRelative(false);
    unsigned long theSubModelCount = inModel->GetSubModelCount();
    bool theFirst = true;
    bool theResult;
    unsigned long theFrame = this->mFrame;
    if (inModel->GetParentModel() == NULL) {
        inLocation.Apply(inModel->GetInternalTransform(theFrame), theTransform);
        if (this->mScale != 1.0f) {
            theTransform.Scale(this->mScale);
            theTransform.SetElement(0, 3, inLocation.GetElement(0, 3));
            theTransform.SetElement(1, 3, inLocation.GetElement(1, 3));
            theTransform.SetElement(2, 3, inLocation.GetElement(2, 3));
        }
    } else
        inLocation.Apply(inModel->GetInternalTransform(theFrame), theTransform);
    inOtherInverse.Apply(theTransform, theRelative);
    float theRadius = inModel->GetBoundingRadius(theFrame) * this->mScale;
    theCenter = inModel->GetCenterPoint();
    theTransform.Apply(theCenter, thePoint);
    float theDistance = inOtherModel->GetBoundingRadius(inFrame) * inScale;
    theCenter = inOtherModel->GetCenterPoint();
    inOtherLocation.Apply(theCenter, theOtherPoint);
    theDistance += theRadius;
    if (thePoint.DistanceToSquared(theOtherPoint) > theDistance * theDistance)
        theResult = false;
    else {
        theResult = true;
        if (inLOD > 0) {
            inModel->GetBoundingBox(theFrame).Transform(theRelative, theBox);
            theResult = inOtherModel->GetBoundingBox(inFrame).Intersects(theBox);
            if (theResult && inLOD > 1) {
                unsigned long theLOD = UMFloatToLong((1.0f - kLODFraction) * inModel->GetLODCount());
                if (theLOD == inModel->GetLODCount())
                    theLOD = inModel->GetLODCount() - 1;
                while (theLOD > 0 && inModel->GetPolygonCount(theLOD) < 50)
                    theLOD--;
                unsigned long theOtherLOD = UMFloatToLong((1.0f - kLODFraction) * inOtherModel->GetLODCount());
                if (theOtherLOD == inOtherModel->GetLODCount())
                    theOtherLOD = inOtherModel->GetLODCount() - 1;
                while (theOtherLOD > 0 && inOtherModel->GetPolygonCount(theOtherLOD) < 50)
                    theOtherLOD--;
                theResult = false;
                unsigned long theMaterialCount = inModel->GetMaterialCount();
                for (unsigned long i = 0; i < theMaterialCount && !theResult; i++) {
                    GEMaterial* theMaterial = inModel->GetMaterial(i);
                    unsigned long theVertexStart = theMaterial->GetVertexStartingIndex();
                    unsigned long theEnd = theMaterial->GetPolygonStartingIndex(theLOD) +
                                           theMaterial->GetPolygonRangeLength(theLOD);
                    for (unsigned long j = theMaterial->GetPolygonStartingIndex(theLOD); j < theEnd && !theResult; j++) {
                        inModel->mVertexArrays[theFrame]->GetPoint(thePoint1,
                            inModel->mPolygonArrays[theLOD]->GetVertexIndex1(j) + theVertexStart);
                        inModel->mVertexArrays[theFrame]->GetPoint(thePoint2,
                            inModel->mPolygonArrays[theLOD]->GetVertexIndex2(j) + theVertexStart);
                        inModel->mVertexArrays[theFrame]->GetPoint(thePoint3,
                            inModel->mPolygonArrays[theLOD]->GetVertexIndex3(j) + theVertexStart);
                        theRelative.Apply(thePoint1, theTransformed1);
                        theRelative.Apply(thePoint2, theTransformed2);
                        theRelative.Apply(thePoint3, theTransformed3);
                        ComputeTriangleBox(theTransformed1, theTransformed2, theTransformed3, theTriangleBox);
                        unsigned long theOtherMaterialCount = inOtherModel->GetMaterialCount();
                        if (theFirst)
                            theBoxes.resize(theOtherMaterialCount);
                        theResult = false;
                        for (unsigned long k = 0; k < theOtherMaterialCount && !theResult; k++) {
                            GEMaterial* theOther = inOtherModel->GetMaterial(k);
                            unsigned long theOtherVertexStart = theOther->GetVertexStartingIndex();
                            unsigned long theOtherEnd = theOther->GetPolygonStartingIndex(theOtherLOD) +
                                                        theOther->GetPolygonRangeLength(theOtherLOD);
                            if (theFirst)
                                theBoxes[k].resize(theOther->GetPolygonRangeLength(theOtherLOD), GEBoundingBox());
                            for (unsigned long l = theOther->GetPolygonStartingIndex(theOtherLOD);
                                 l < theOtherEnd && !theResult; l++) {
                                if (theFirst) {
                                    inOtherModel->mVertexArrays[inFrame]->GetPoint(thePoint1,
                                        inOtherModel->mPolygonArrays[theOtherLOD]->GetVertexIndex1(l) + theOtherVertexStart);
                                    inOtherModel->mVertexArrays[inFrame]->GetPoint(thePoint2,
                                        inOtherModel->mPolygonArrays[theOtherLOD]->GetVertexIndex2(l) + theOtherVertexStart);
                                    inOtherModel->mVertexArrays[inFrame]->GetPoint(thePoint3,
                                        inOtherModel->mPolygonArrays[theOtherLOD]->GetVertexIndex3(l) + theOtherVertexStart);
                                    ComputeTriangleBox(thePoint1, thePoint2, thePoint3,
                                                       theBoxes[k][l - theOther->GetPolygonStartingIndex(theOtherLOD)]);
                                }
                                theResult = theBoxes[k][l - theOther->GetPolygonStartingIndex(theOtherLOD)]
                                                .Intersects(theTriangleBox);
                            }
                        }
                        theFirst = false;
                    }
                }
            }
        }
    }
    for (unsigned long m = 0; !theResult && m < theSubModelCount; m++)
        theResult = this->Intersects(theTransform, inModel->GetSubModel(m), inOtherLocation, inOtherInverse, inScale,
                               inOtherModel, inFrame, inLOD);
    return theResult;
}

// File-static, no export (provisional name)
// FUNCTION: LLE 0x10021ebc
// Computes the axis-aligned bounding box of a triangle
static void ComputeTriangleBox(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3,
                               GEBoundingBox& outBox)
{
    ee_ge_physical_triangle_box(GERust(inPoint1), GERust(inPoint2), GERust(inPoint3), GERust(outBox));
}

// FUNCTION: LLE 0x10021fc0
// Returns whether the placed model can be visible in a viewport: bounding sphere against the four planes and the
// clip distances, or its box against the orthographic rectangle
bool GEPhysicalModel::Intersects(const GEViewport& inViewport, bool inOrthographic) const
{
    GE3DPoint theCameraPoint;
    GE3DPoint thePoint;
    GEBoundingBox theBox;
    GEVector theLeft = inViewport.GetLeftPlane().GetNormal();
    GEVector theRight = inViewport.GetRightPlane().GetNormal();
    GEVector theTop = inViewport.GetTopPlane().GetNormal();
    GEVector theBottom = inViewport.GetBottomPlane().GetNormal();
    GEViewport theViewport(inViewport);
    float theFront = inViewport.GetFrontClippingDistance();
    float theRear = inViewport.GetRearClippingDistance();
    GETransformation theTransform(false);
    GETransformation theCameraTransform(false);
    bool theResult = true;
    this->mLocation.Apply(this->mModel->GetInternalTransform(this->mFrame), theTransform);
    if (this->mScale != 1.0f) {
        theTransform.Scale(this->mScale);
        theTransform.SetElement(0, 3, this->mLocation.GetElement(0, 3));
        theTransform.SetElement(1, 3, this->mLocation.GetElement(1, 3));
        theTransform.SetElement(2, 3, this->mLocation.GetElement(2, 3));
    }
    if (!inOrthographic) {
        theTransform.Apply(this->mModel->GetCenterPoint(), thePoint);
        theViewport.GetCameraInverse().Apply(thePoint, theCameraPoint);
        float theRadius = this->mModel->GetBoundingRadius(this->mFrame) * this->mScale;
        theLeft.Scale(theRadius, theLeft);
        theRight.Scale(theRadius, theRight);
        theTop.Scale(theRadius, theTop);
        theBottom.Scale(theRadius, theBottom);
        if (theCameraPoint.GetY() + theRadius < theViewport.GetFrontClippingDistance() ||
            theCameraPoint.GetY() - theRadius > theViewport.GetRearClippingDistance())
            theResult = false;
        else {
            thePoint.mX = theCameraPoint.mX + theLeft.mX;
            thePoint.mY = theCameraPoint.mY + theLeft.mY;
            thePoint.mZ = theCameraPoint.mZ + theLeft.mZ;
            if (!theViewport.GetLeftPlane().IsPointOnOrAbovePlane(thePoint))
                theResult = false;
            else {
                thePoint.mX = theCameraPoint.mX + theRight.mX;
                thePoint.mY = theCameraPoint.mY + theRight.mY;
                thePoint.mZ = theCameraPoint.mZ + theRight.mZ;
                if (!theViewport.GetRightPlane().IsPointOnOrAbovePlane(thePoint))
                    theResult = false;
                else {
                    thePoint.mX = theCameraPoint.mX + theTop.mX;
                    thePoint.mY = theCameraPoint.mY + theTop.mY;
                    thePoint.mZ = theCameraPoint.mZ + theTop.mZ;
                    if (!theViewport.GetTopPlane().IsPointOnOrAbovePlane(thePoint))
                        theResult = false;
                    else {
                        thePoint.mX = theCameraPoint.mX + theBottom.mX;
                        thePoint.mY = theCameraPoint.mY + theBottom.mY;
                        thePoint.mZ = theCameraPoint.mZ + theBottom.mZ;
                        if (!theViewport.GetBottomPlane().IsPointOnOrAbovePlane(thePoint))
                            theResult = false;
                    }
                }
            }
        }
    } else {
        theViewport.GetCameraInverse().Apply(theTransform, theCameraTransform);
        this->mModel->GetBoundingBox(this->mFrame).Transform(theCameraTransform, theBox);
        float theScale = -theViewport.GetYClippingPlane();
        const GERectangle& theRectangle = theViewport.GetOrthographicRectangle();
        if (theBox.GetTopRightBack().GetY() < theFront || theBox.GetBottomLeftFront().GetY() > theRear ||
            theBox.GetTopRightBack().GetX() < theRectangle.GetLeft() ||
            theRectangle.GetRight() < theBox.GetBottomLeftFront().GetX() ||
            theBox.GetTopRightBack().GetZ() < theRectangle.GetBottom() * theScale ||
            theBox.GetBottomLeftFront().GetZ() > theRectangle.GetTop() * theScale)
            theResult = false;
    }
    return theResult;
}

// FUNCTION: LLE 0x1002232f
// Replaces the placed model's model, keeping a reference to the new one
void GEPhysicalModel::SetModel(GEModel* inModel)
{
    if (this->mModel)
        this->mModel->Dereference();
    this->mModel = inModel;
    if (inModel)
        inModel->Reference();
}

// vector<vector<GEBoundingBox> > / vector<GEBoundingBox> COMDATs (Intersects of two models)
// TEMPLATE: LLE 0x10022350
// TEMPLATE: LLE 0x10022376
// TEMPLATE: LLE 0x10022429
// TEMPLATE: LLE 0x10022462
// TEMPLATE: LLE 0x1002249a
// TEMPLATE: LLE 0x1002254e
// TEMPLATE: LLE 0x10022563
// TEMPLATE: LLE 0x10022817
// TEMPLATE: LLE 0x10022880
// TEMPLATE: LLE 0x10022b51
// TEMPLATE: LLE 0x10022b9b
// TEMPLATE: LLE 0x10022bd5
// TEMPLATE: LLE 0x10022c00
// TEMPLATE: LLE 0x10022c4a
// TEMPLATE: LLE 0x10022c77
// TEMPLATE: LLE 0x10022c8e
// TEMPLATE: LLE 0x10022cba
// TEMPLATE: LLE 0x10022cd7
// TEMPLATE: LLE 0x10022d03
// TEMPLATE: LLE 0x10022d27
// TEMPLATE: LLE 0x10022d44
// TEMPLATE: LLE 0x10022d70
// TEMPLATE: LLE 0x10022d94
// TEMPLATE: LLE 0x10022dc6
// TEMPLATE: LLE 0x10022dd8
// TEMPLATE: LLE 0x10022dee
// TEMPLATE: LLE 0x10022e0a
// TEMPLATE: LLE 0x10022f72

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)
// SYNTHETIC: LLE 0x10022f88
// SYNTHETIC: LLE 0x10022fa6
// SYNTHETIC: LLE 0x10022fb2
// SYNTHETIC: LLE 0x10022fbe
