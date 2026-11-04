// C:\EEX\Low-Level Engine\Graphics Engine\GEModel.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1001eb9f..0x100206c7: GEModel's ctors, dtor and core members, then its map COMDAT and the UList<GELOD*> pool init
// pair (lle/GELOD.h via GEModel.h). AllocateInternalData differs from the original in block layout and register
// choice.
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
#include <math.h>
#include "lle/GEPolygonArray.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x1001eb9f
// SYNTHETIC: LLE 0x1001eba9
// SYNTHETIC: LLE 0x1001ebb9
// SYNTHETIC: LLE 0x1001ebc5
// SYNTHETIC: LLE 0x1001ebcf
// SYNTHETIC: LLE 0x1001ebd9
// SYNTHETIC: LLE 0x1001ebe9
// SYNTHETIC: LLE 0x1001ebf5
// SYNTHETIC: LLE 0x1001ebff
// SYNTHETIC: LLE 0x1001ec09
// SYNTHETIC: LLE 0x1001ec19
// SYNTHETIC: LLE 0x1001ec25
// SYNTHETIC: LLE 0x1001ec2f
// SYNTHETIC: LLE 0x1001ec39
// SYNTHETIC: LLE 0x1001ec49
// SYNTHETIC: LLE 0x1001ec55
// SYNTHETIC: LLE 0x1001ec5f
// SYNTHETIC: LLE 0x1001ec69
// SYNTHETIC: LLE 0x1001ec79
// SYNTHETIC: LLE 0x1001ec85
// SYNTHETIC: LLE 0x1001ec8f
// SYNTHETIC: LLE 0x1001ec99
// SYNTHETIC: LLE 0x1001eca9
// SYNTHETIC: LLE 0x1001ecb5

// FUNCTION: LLE 0x1001ecbf
// Creates an empty model owned by a model manager, with one LOD and no data allocated
GEModel::GEModel(GEModelManager* inManager)
{
    this->mGraphicsScale = 1.0f;
    this->mParentModel = NULL;
    this->mModelOpacity = 1.0f;
    this->mSubModels = NULL;
    this->mBoundingRadii = NULL;
    this->mFrameTagPoints = NULL;
    this->mTagPointNames = NULL;
    this->mFractionalFramePosition = 0.0f;
    this->mTextureFilenames = NULL;
    this->mBoundingBoxes = NULL;
    this->mInternalTransforms = NULL;
    this->mPolygonArrays = NULL;
    this->mVertexArrays = NULL;
    this->mGraphicsCurrentFrame = 0;
    this->mGraphicsNextFrame = 0;
    this->mPolygonCount = 0;
    this->mVertexCount = 0;
    this->mMaterialCount = 0;
    this->mTagPointCount = 0;
    this->mLODCount = 1;
    this->mManager = inManager;
}

// FUNCTION: LLE 0x1001edb9
// Creates a named model with the given counts and allocates its data (no shared frames)
GEModel::GEModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount,
                 unsigned long inTagPointCount, unsigned long inFrameCount, unsigned long inSubModelCount,
                 const UString& inName, unsigned long inFlags, GEModelManager* inManager)
{
    std::map<unsigned long, long> theTextures;
    this->mGraphicsScale = 1.0f;
    this->mModelOpacity = 1.0f;
    this->mParentModel = NULL;
    this->mGraphicsCurrentFrame = 0;
    this->mFractionalFramePosition = 0.0f;
    this->mGraphicsNextFrame = 0;
    this->mLODCount = 1;
    this->mName = inName;
    this->mPolygonCount = inPolygonCount;
    this->mVertexCount = inVertexCount;
    this->mMaterialCount = inMaterialCount;
    this->mTagPointCount = inTagPointCount;
    this->mFrameCount = inFrameCount;
    this->mSubModelCount = inSubModelCount;
    this->AllocateInternalData(inFlags, theTextures);
    this->mManager = inManager;
}

// FUNCTION: LLE 0x1001eed6
// Destroys the model: frees its vertex, polygon and tag point arrays (shared frames once),
// releases its materials and sub-models
GEModel::~GEModel()
{
    unsigned long i;
    for (i = 0; i < this->mFrameCount; i++) {
        std::map<unsigned long, long>::iterator theIterator = this->mSharedFrames.find(i);
        if (theIterator != this->mSharedFrames.end())
            continue;
        if (this->mVertexArrays[i])
            delete this->mVertexArrays[i];
        if (this->mFrameTagPoints[i])
            delete[] this->mFrameTagPoints[i];
    }
    for (i = 0; i < this->mLODCount; i++) {
        if (this->mPolygonArrays[i])
            delete this->mPolygonArrays[i];
    }
    if (this->mBoundingBoxes)
        delete[] this->mBoundingBoxes;
    delete[] this->mBoundingRadii;
    delete[] this->mFrameTagPoints;
    delete[] this->mPolygonArrays;
    delete[] this->mVertexArrays;
    delete[] this->mInternalTransforms;
    if (this->mMaterials) {
        for (i = 0; i < this->mMaterialCount; i++)
            this->mMaterials[i]->UReferenceCounter::Dereference();
        delete[] this->mMaterials;
    }
    if (this->mTextureFilenames)
        delete[] this->mTextureFilenames;
    if (this->mTagPointNames)
        delete[] this->mTagPointNames;
    if (this->mSubModels) {
        for (i = 0; i < this->mSubModelCount; i++)
            this->SetSubModel(NULL, i);
        delete[] this->mSubModels;
    }
}

// FUNCTION: LLE 0x1001f080
// Allocates the vertex arrays (shared frames point at their source frame), polygon array, transforms, boxes,
// radii, tag points, materials and names; returns 8 when an allocation fails
long GEModel::AllocateInternalData(unsigned long inFlags, std::map<unsigned long, long>& inSharedFrames)
{
    long theResult = kErrorOK;
    unsigned long i;
    this->mGraphicsCurrentFrame = 0;
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    this->mVertexArrays = new GEVertexArray*[this->mFrameCount];
    for (i = 0; i < this->mFrameCount; i++) {
        std::map<unsigned long, long>::iterator theIterator = inSharedFrames.find(i);
        if (theIterator == inSharedFrames.end())
            this->mVertexArrays[i] = theRasterizer->CreateVertexArray(inFlags, this->mVertexCount);
    }
    this->mPolygonArrays = new GEPolygonArray*[this->mLODCount];
    this->mPolygonArrays[0] = theRasterizer->CreatePolygonArray(this->mPolygonCount);
    for (i = 1; i < this->mLODCount; i++)
        this->mPolygonArrays[i] = NULL;
    this->mSharedFrames = inSharedFrames;
    this->mInternalTransforms = new GETransformation[this->mFrameCount];
    if (this->mInternalTransforms) {
        this->mSubModels = new GEModel*[this->mSubModelCount];
        if (this->mSubModels) {
            memset(this->mSubModels, 0, this->mSubModelCount * sizeof(GEModel*));
            this->mBoundingBoxes = new GEBoundingBox[this->mFrameCount];
            if (this->mBoundingBoxes) {
                this->mGraphicsBoundingBox = &this->mBoundingBoxes[this->mGraphicsCurrentFrame];
                this->mBoundingRadii = new float[this->mFrameCount];
                if (this->mBoundingRadii) {
                    this->mFrameTagPoints = new GE3DPoint*[this->mFrameCount];
                    if (this->mFrameTagPoints) {
                        for (i = 0; i < this->mFrameCount; i++) {
                            if (theResult == kErrorOK && this->mTagPointCount > 0) {
                                std::map<unsigned long, long>::iterator theIterator = inSharedFrames.find(i);
                                if (theIterator == inSharedFrames.end()) {
                                    this->mFrameTagPoints[i] = new GE3DPoint[this->mTagPointCount];
                                    if (this->mFrameTagPoints[i] == NULL)
                                        theResult = 8;
                                }
                            } else
                                this->mFrameTagPoints[i] = NULL;
                        }
                        this->mTagPoints = this->mFrameTagPoints[0];
                    } else
                        theResult = 8;
                } else {
                    this->mFrameTagPoints = NULL;
                    theResult = 8;
                }
            } else {
                this->mFrameTagPoints = NULL;
                theResult = 8;
            }
        } else {
            this->mFrameTagPoints = NULL;
            theResult = 8;
        }
    } else {
        this->mFrameTagPoints = NULL;
        theResult = 8;
    }
    for (i = 0; i < this->mFrameCount; i++) {
        std::map<unsigned long, long>::iterator theIterator = inSharedFrames.find(i);
        if (theIterator != inSharedFrames.end()) {
            this->mVertexArrays[i] = this->mVertexArrays[theIterator->second + i];
            this->mFrameTagPoints[i] = this->mFrameTagPoints[theIterator->second + i];
        }
    }
    if (theResult == kErrorOK) {
        this->mMaterials = new GEMaterial*[this->mMaterialCount];
        if (this->mMaterials) {
            for (i = 0; i < this->mMaterialCount; i++) {
                if (theResult == kErrorOK) {
                    this->mMaterials[i] = new GEMaterial(theRasterizer->GetScreenID() + 1, this->mLODCount);
                    if (this->mMaterials[i] == NULL)
                        theResult = 8;
                } else
                    this->mMaterials[i] = NULL;
            }
        } else
            theResult = 8;
    } else
        this->mMaterials = NULL;
    if (theResult == kErrorOK) {
        this->mTextureFilenames = new UString[this->mMaterialCount];
        this->mTagPointNames = this->mTagPointCount > 0 ? new UString[this->mTagPointCount] : NULL;
    } else {
        this->mTextureFilenames = NULL;
        this->mTagPointNames = NULL;
    }
    return theResult;
}

// FUNCTION: LLE 0x1001f544
// Computes each frame's bounding box from its vertices, then the sub-models' boxes
void GEModel::ComputeBoundingBoxes()
{
    GE3DPoint theMinimum;
    GE3DPoint theMaximum;
    GE3DPoint thePoint;
    unsigned long i;
    for (i = 0; i < this->mFrameCount; i++) {
        this->mVertexArrays[i]->GetPoint(theMinimum, 0);
        this->mVertexArrays[i]->GetPoint(theMaximum, 0);
        for (unsigned long j = 0; j < this->mVertexCount; j++) {
            this->mVertexArrays[i]->GetPoint(thePoint, j);
            if (thePoint.mX < theMinimum.mX)
                theMinimum.mX = thePoint.mX;
            else if (thePoint.mX > theMaximum.mX)
                theMaximum.mX = thePoint.mX;
            if (thePoint.GetY() < theMinimum.GetY())
                theMinimum.SetY(thePoint.GetY());
            else if (thePoint.GetY() > theMaximum.GetY())
                theMaximum.SetY(thePoint.GetY());
            if (thePoint.GetZ() < theMinimum.GetZ())
                theMinimum.SetZ(thePoint.GetZ());
            else if (thePoint.GetZ() > theMaximum.GetZ())
                theMaximum.SetZ(thePoint.GetZ());
        }
        this->mBoundingBoxes[i].Define(theMinimum, theMaximum);
    }
    for (i = 0; i < this->mSubModelCount; i++)
        this->mSubModels[i]->ComputeBoundingBoxes();
}

// FUNCTION: LLE 0x1001f69a
// Computes bounding boxes, the centre point (average of every frame's box) and each frame's bounding radius,
// sub-models first
void GEModel::ComputeBoundingVolumes()
{
    GE3DPoint thePoint;
    float theX = 0.0f;
    float theY = 0.0f;
    float theZ = 0.0f;
    float theScale = 1.0f / (this->mFrameCount * 2.0f);
    GE3DPoint theMinimum;
    GE3DPoint theMaximum;
    unsigned long i;
    this->ComputeBoundingBoxes();
    for (i = 0; i < this->mSubModelCount; i++)
        this->mSubModels[i]->ComputeBoundingVolumes();
    for (i = 0; i < this->mFrameCount; i++) {
        theMinimum = this->mBoundingBoxes[i].GetBottomLeftFront();
        theMaximum = this->mBoundingBoxes[i].GetTopRightBack();
        theX += theMinimum.GetX() + theMaximum.GetX();
        theY += theMinimum.GetY() + theMaximum.GetY();
        theZ += theMinimum.GetZ() + theMaximum.GetZ();
    }
    this->mCenterPoint.SetX(theScale * theX);
    this->mCenterPoint.SetY(theScale * theY);
    this->mCenterPoint.SetZ(theScale * theZ);
    for (i = 0; i < this->mFrameCount; i++) {
        float theMaximumDistance = 0.0f;
        for (unsigned long j = 0; j < this->mVertexCount; j++) {
            this->mVertexArrays[i]->GetPoint(thePoint, j);
            float theDistance = this->mCenterPoint.DistanceToSquared(thePoint);
            if (theDistance > theMaximumDistance)
                theMaximumDistance = theDistance;
        }
        this->mBoundingRadii[i] = sqrt(theMaximumDistance);
    }
}

// FUNCTION: LLE 0x1001f80c
// Recomputes vertex normals from the polygons of LOD 0 for every frame; the sums are not cleared between frames
void GEModel::ComputeModelNormals()
{
    GEVector theEdge1;
    GEVector theEdge2;
    GEVector theNormal;
    GE3DPoint thePoint1;
    GE3DPoint thePoint2;
    GE3DPoint thePoint3;
    GEVector* theNormals = new GEVector[this->mVertexCount];
    if (!theNormals)
        return;
    for (unsigned long i = 0; i < this->GetFrameCount(); i++) {
        GEVertexArray* theArray = this->GetVertexArray(i);
        unsigned long j;
        for (j = 0; j < this->GetPolygonCount(); j++) {
            theArray->GetPoint(thePoint1, this->GetPolygonArray(0)->GetVertexIndex1(j));
            theArray->GetPoint(thePoint2, this->GetPolygonArray(0)->GetVertexIndex2(j));
            theArray->GetPoint(thePoint3, this->GetPolygonArray(0)->GetVertexIndex3(j));
            theEdge1.SetX(thePoint2.GetX() - thePoint1.GetX());
            theEdge1.SetY(thePoint2.GetY() - thePoint1.GetY());
            theEdge1.SetZ(thePoint2.GetZ() - thePoint1.GetZ());
            theEdge2.SetX(thePoint3.GetX() - thePoint1.GetX());
            theEdge2.SetY(thePoint3.GetY() - thePoint1.GetY());
            theEdge2.SetZ(thePoint3.GetZ() - thePoint1.GetZ());
            theEdge2.CrossProduct(theEdge1, theNormal);
            {
                GEVector& theSum = theNormals[this->GetPolygonArray(0)->GetVertexIndex1(j)];
                theSum.mX += theNormal.mX;
                theSum.mY += theNormal.mY;
                theSum.mZ += theNormal.mZ;
            }
            {
                GEVector& theSum = theNormals[this->GetPolygonArray(0)->GetVertexIndex2(j)];
                theSum.mX += theNormal.mX;
                theSum.mY += theNormal.mY;
                theSum.mZ += theNormal.mZ;
            }
            {
                GEVector& theSum = theNormals[this->GetPolygonArray(0)->GetVertexIndex3(j)];
                theSum.mX += theNormal.mX;
                theSum.mY += theNormal.mY;
                theSum.mZ += theNormal.mZ;
            }
        }
        for (j = 0; j < this->mVertexCount; j++) {
            theNormals[j].Normalize();
            theArray->SetNormal(theNormals[j], j);
        }
    }
    delete[] theNormals;
}

// FUNCTION: LLE 0x1001fa84
// Releases a reference; when only the manager's (and one caller's) reference is left, removes it from the manager
void GEModel::Dereference()
{
    if (this->mReferenceCount > 2)
        UReferenceCounter::Dereference();
    else {
        if (this->mReferenceCount > 1)
            UReferenceCounter::Dereference();
        this->mManager->RemoveModel(this);
    }
}

// FUNCTION: LLE 0x1001fab0
// Returns the number of polygons at an LOD, summed over the materials
unsigned long GEModel::GetPolygonCount(unsigned long inLOD) const
{
    unsigned long theCount = 0;
    for (unsigned long i = 0; i < this->mMaterialCount; i++)
        theCount += this->mMaterials[i]->GetPolygonRangeLength(inLOD);
    return theCount;
}

// FUNCTION: LLE 0x1001fade
// Returns the number of polygons at an LOD in the model and all its sub-models
unsigned long GEModel::GetPolygonCountOfHierarchy(unsigned long inLOD) const
{
    unsigned long theCount = this->GetPolygonCount(inLOD);
    for (unsigned long i = 0; i < this->mSubModelCount; i++)
        theCount += this->mSubModels[i]->GetPolygonCountOfHierarchy(inLOD);
    return theCount;
}

// FUNCTION: LLE 0x1001fb1d
// Builds the graphics transform for the current frame (pre transform, internal transform, scale, post transform)
// for the model and its sub-models
void GEModel::PrepareTransforms()
{
    float theScale = this->mGraphicsScale;
    unsigned long theSubModelCount = this->mSubModelCount;
    GETransformation theTransform(false);
    GETransformation theScaled(false);
    this->mInternalTransforms[this->mGraphicsCurrentFrame].Apply(this->mGraphicsPreModelTransform, theTransform);
    theScaled.SetElement(0, 0, theTransform.GetElement(0, 0) * theScale);
    theScaled.SetElement(0, 1, theTransform.GetElement(0, 1) * theScale);
    theScaled.SetElement(0, 2, theTransform.GetElement(0, 2) * theScale);
    theScaled.SetElement(0, 3, theTransform.GetElement(0, 3));
    theScaled.SetElement(1, 0, theTransform.GetElement(1, 0) * theScale);
    theScaled.SetElement(1, 1, theTransform.GetElement(1, 1) * theScale);
    theScaled.SetElement(1, 2, theTransform.GetElement(1, 2) * theScale);
    theScaled.SetElement(1, 3, theTransform.GetElement(1, 3));
    theScaled.SetElement(2, 0, theTransform.GetElement(2, 0) * theScale);
    theScaled.SetElement(2, 1, theTransform.GetElement(2, 1) * theScale);
    theScaled.SetElement(2, 2, theTransform.GetElement(2, 2) * theScale);
    theScaled.SetElement(2, 3, theTransform.GetElement(2, 3));
    theScaled.SetElement(3, 0, 0.0f);
    theScaled.SetElement(3, 1, 0.0f);
    theScaled.SetElement(3, 2, 0.0f);
    theScaled.SetElement(3, 3, 1.0f);
    this->mGraphicsPostModelTransform.Apply(theScaled, this->mGraphicsTransform);
    for (unsigned long i = 0; i < theSubModelCount; i++)
        this->mSubModels[i]->PrepareTransforms();
}

// FUNCTION: LLE 0x1001fc00
// Bakes the frame's transform scale into its vertices, tag points, box, radius and centre and leaves the
// transform unscaled; recurses into the sub-models
void GEModel::RemoveModelScale(unsigned long inFrame, const GETransformation& inParent, float inX, float inY,
                               float inZ)
{
    GE3DPoint thePoint;
    GE3DPoint theResult;
    GE3DPoint theOrigin;
    GEBoundingBox theBox;
    GETransformation theInternal;
    GETransformation theUnscaled;
    GETransformation theScale;
    GETransformation theTransform;
    GEVector theAxis;
    float theX;
    unsigned long theCount;
    unsigned long i;
    float theZ;
    float theScaleX;
    float theScaleY;
    float theScaleZ;
    unsigned long theSubModelCount = this->mSubModelCount;
    GEVector theNormal;
    GEVector theTransformedNormal;
    theInternal = this->mInternalTransforms[inFrame];
    inParent.Apply(theInternal, theTransform);
    theAxis.SetX(theInternal.GetElement(0, 0));
    theAxis.SetY(theInternal.GetElement(1, 0));
    theAxis.SetZ(theInternal.GetElement(2, 0));
    theScaleX = theAxis.GetMagnitude();
    theX = theScaleX * inX;
    theAxis.SetX(theInternal.GetElement(0, 1));
    theAxis.SetY(theInternal.GetElement(1, 1));
    theAxis.SetZ(theInternal.GetElement(2, 1));
    theScaleY = theAxis.GetMagnitude();
    inY = theScaleY * inY;
    theAxis.SetX(theInternal.GetElement(0, 2));
    theAxis.SetY(theInternal.GetElement(1, 2));
    theAxis.SetZ(theInternal.GetElement(2, 2));
    theScaleZ = theAxis.GetMagnitude();
    theZ = theScaleZ * inZ;
    theScale.SetElement(0, 0, theX);
    theScale.SetElement(1, 1, inY);
    theScale.SetElement(2, 2, theZ);
    if (inFrame == 0) {
        theScale.Apply(this->mCenterPoint, theResult);
        this->mCenterPoint = theResult;
    }
    float theMaximum = theX;
    if (inY > theMaximum)
        theMaximum = inY;
    if (theZ > theMaximum)
        theMaximum = theZ;
    this->mBoundingRadii[inFrame] *= theMaximum;
    this->mBoundingBoxes[inFrame].Transform(theScale, theBox);
    this->mBoundingBoxes[inFrame] = theBox;
    theCount = this->mVertexArrays[inFrame]->GetSize();
    for (i = 0; i < theCount; i++) {
        this->mVertexArrays[inFrame]->GetPoint(thePoint, i);
        theScale.Apply(thePoint, theResult);
        this->mVertexArrays[inFrame]->SetPoint(theResult, i);
        this->mVertexArrays[inFrame]->GetNormal(theNormal, i);
        theScale.Apply(theNormal, theTransformedNormal);
        // Fixed: used to call GetNormal here, so the scaled normal was never stored
        this->mVertexArrays[inFrame]->SetNormal(theTransformedNormal, i);
    }
    theCount = this->mTagPointCount;
    for (i = 0; i < theCount; i++) {
        theScale.Apply(this->mFrameTagPoints[inFrame][i], theResult);
        this->mFrameTagPoints[inFrame][i] = theResult;
    }
    theUnscaled.SetElement(0, 0, theInternal.GetElement(0, 0));
    theUnscaled.SetElement(0, 1, theInternal.GetElement(0, 1));
    theUnscaled.SetElement(0, 2, theInternal.GetElement(0, 2));
    theUnscaled.SetElement(0, 3, 0.0f);
    theUnscaled.SetElement(1, 0, theInternal.GetElement(1, 0));
    theUnscaled.SetElement(1, 1, theInternal.GetElement(1, 1));
    theUnscaled.SetElement(1, 2, theInternal.GetElement(1, 2));
    theUnscaled.SetElement(1, 3, 0.0f);
    theUnscaled.SetElement(2, 0, theInternal.GetElement(2, 0));
    theUnscaled.SetElement(2, 1, theInternal.GetElement(2, 1));
    theUnscaled.SetElement(2, 2, theInternal.GetElement(2, 2));
    theUnscaled.SetElement(2, 3, 0.0f);
    theUnscaled.SetElement(3, 0, 0.0f);
    theUnscaled.SetElement(3, 1, 0.0f);
    theUnscaled.SetElement(3, 2, 0.0f);
    theUnscaled.SetElement(3, 3, 1.0f);
    theUnscaled.Scale(1.0f / theScaleX, 1.0f / theScaleY, 1.0f / theScaleZ);
    thePoint.SetX(0.0f);
    thePoint.SetY(0.0f);
    thePoint.SetZ(0.0f);
    inParent.Apply(thePoint, theResult);
    theTransform.Apply(thePoint, theOrigin);
    theOrigin -= theResult;
    theUnscaled.SetElement(0, 3, theOrigin.GetX());
    theUnscaled.SetElement(1, 3, theOrigin.GetY());
    theUnscaled.SetElement(2, 3, theOrigin.GetZ());
    this->mInternalTransforms[inFrame] = theUnscaled;
    for (unsigned long k = 0; k < theSubModelCount; k++)
        this->mSubModels[k]->RemoveModelScale(inFrame, theTransform, theX, inY, theZ);
}

// FUNCTION: LLE 0x1002004b
// Removes the scale from every frame of the model, then goes back to frame 0
void GEModel::RemoveScale()
{
    GETransformation theIdentity;
    for (unsigned long i = 0; i < this->mFrameCount; i++)
        this->RemoveModelScale(i, theIdentity, 1.0f, 1.0f, 1.0f);
    this->SetGraphicsCurrentFrame(0);
}

// FUNCTION: LLE 0x1002009b
// Sets the position between the current and next frame for the model and its sub-models
void GEModel::SetFractionalFramePosition(float inPosition)
{
    unsigned long theSubModelCount = this->mSubModelCount;
    this->mFractionalFramePosition = inPosition;
    for (unsigned long i = 0; i < theSubModelCount; i++)
        this->mSubModels[i]->SetFractionalFramePosition(inPosition);
}

// FUNCTION: LLE 0x100200d7
// Sets the current frame (tag points and bounding box follow) for the model and its sub-models
void GEModel::SetGraphicsCurrentFrame(unsigned long inFrame)
{
    unsigned long theSubModelCount = this->mSubModelCount;
    this->mGraphicsCurrentFrame = inFrame;
    this->mTagPoints = this->mFrameTagPoints[inFrame];
    this->mGraphicsBoundingBox = &this->mBoundingBoxes[inFrame];
    for (unsigned long i = 0; i < theSubModelCount; i++)
        this->mSubModels[i]->SetGraphicsCurrentFrame(inFrame);
}

// FUNCTION: LLE 0x1002012f
// Sets the next frame for the model and its sub-models
void GEModel::SetGraphicsNextFrame(unsigned long inFrame)
{
    unsigned long theSubModelCount = this->mSubModelCount;
    this->mGraphicsNextFrame = inFrame;
    for (unsigned long i = 0; i < theSubModelCount; i++)
        this->mSubModels[i]->SetGraphicsNextFrame(inFrame);
}

// FUNCTION: LLE 0x10020166
// Replaces the sub-model at an index, releasing the old one and referencing the new one
void GEModel::SetSubModel(GEModel* inModel, unsigned long inIndex)
{
    if (this->mSubModels[inIndex]) {
        this->mSubModels[inIndex]->mParentModel = NULL;
        this->mSubModels[inIndex]->UReferenceCounter::Dereference();
    }
    if (inModel) {
        inModel->Reference();
        inModel->mParentModel = this;
    }
    this->mSubModels[inIndex] = inModel;
}

// FUNCTION: LLE 0x100201b4
// Sets one screen's texture on every material of the model and its sub-models
void GEModel::SetTexture(GETexture* inTexture, unsigned long inScreen)
{
    unsigned long theSubModelCount = this->mSubModelCount;
    unsigned long theMaterialCount = this->mMaterialCount;
    unsigned long i;
    for (i = 0; i < theMaterialCount; i++)
        this->mMaterials[i]->SetTexture(inTexture, inScreen);
    for (i = 0; i < theSubModelCount; i++)
        this->mSubModels[i]->SetTexture(inTexture, inScreen);
}

// FUNCTION: LLE 0x10020213
// Returns an estimate of the memory used by the model and its sub-models, in bytes
unsigned long GEModel::SizeOf()
{
    unsigned long theSize = 0xd8 + this->mName.SizeOf() + this->mSharedFrames.size() * 8 + this->mFrameCount * sizeof(GETransformation) +
                            this->mSubModelCount * sizeof(GEModel*);
    for (unsigned long i = 0; i < this->mSubModelCount; i++)
        theSize += this->mSubModels[i]->SizeOf();
    theSize += this->mLODCount * sizeof(GEPolygonArray*) + 4;
    for (unsigned long j = 0; j < this->mLODCount; j++)
        theSize += this->mPolygonArrays[j]->SizeOf();
    theSize += (this->mFrameCount - this->mSharedFrames.size()) * this->mVertexArrays[0]->SizeOf() + 8;
    theSize += this->mMaterialCount * sizeof(GEMaterial*);
    for (unsigned long k = 0; k < this->mMaterialCount; k++)
        theSize += this->mMaterials[k]->SizeOf();
    theSize += (this->mFrameCount - this->mSharedFrames.size()) * this->mTagPointCount * sizeof(GE3DPoint) +
               this->mFrameCount * (sizeof(GEBoundingBox) + sizeof(float)) + 0x14;
    for (unsigned long l = 0; l < this->mMaterialCount; l++)
        theSize += this->mTextureFilenames[l].SizeOf();
    theSize += 4;
    for (unsigned long m = 0; m < this->mTagPointCount; m++)
        theSize += this->mTagPointNames[m].SizeOf();
    theSize += this->mFrameCount * sizeof(GE3DPoint*) + 0x60;
    return theSize;
}

// FUNCTION: LLE 0x10020338
// Sorts the tag points by the first letter of their names, in every frame, then in the sub-models
void GEModel::SortTagPoints()
{
    UString theName;
    GE3DPoint theTemp;
    unsigned long theFrameCount = this->mFrameCount;
    unsigned long theTagPointCount = this->mTagPointCount;
    for (unsigned long i = 0; i < theTagPointCount; i++) {
        unsigned long theMinimum = i;
        for (unsigned long j = i + 1; j < theTagPointCount; j++) {
            if (((const char*)this->mTagPointNames[j])[0] < ((const char*)this->mTagPointNames[theMinimum])[0])
                theMinimum = j;
        }
        if (theMinimum == i)
            continue;
        theName = this->mTagPointNames[i];
        this->mTagPointNames[i] = this->mTagPointNames[theMinimum];
        this->mTagPointNames[theMinimum] = theName;
        for (unsigned long k = 0; k < theFrameCount; k++) {
            GE3DPoint* thePoints = this->mFrameTagPoints[k];
            theTemp = thePoints[i];
            thePoints[i] = thePoints[theMinimum];
            thePoints[theMinimum] = theTemp;
        }
    }
    for (unsigned long l = 0; l < this->mSubModelCount; l++)
        this->mSubModels[l]->SortTagPoints();
}

// FUNCTION: LLE 0x10020495
// Returns false when any LOD 0 polygon of any frame has two equal corners; checks the sub-models too
bool GEModel::Verify() const
{
    GE3DPoint thePoint1;
    GE3DPoint thePoint2;
    GE3DPoint thePoint3;
    unsigned long theMaterialCount = this->mMaterialCount;
    unsigned long theFrameCount = this->mFrameCount;
    bool theResult = true;
    for (unsigned long i = 0; i < theFrameCount; i++) {
        GEVertexArray* theArray = this->mVertexArrays[i];
        for (unsigned long j = 0; j < theMaterialCount; j++) {
            unsigned long theEnd = this->mMaterials[j]->GetPolygonStartingIndex(0) + this->mMaterials[j]->GetPolygonRangeLength(0);
            unsigned long theVertexStart = this->mMaterials[j]->GetVertexStartingIndex();
            for (unsigned long k = this->mMaterials[j]->GetPolygonStartingIndex(0); k < theEnd; k++) {
                theArray->GetPoint(thePoint1, this->mPolygonArrays[0]->GetVertexIndex1(k) + theVertexStart);
                theArray->GetPoint(thePoint2, this->mPolygonArrays[0]->GetVertexIndex2(k) + theVertexStart);
                theArray->GetPoint(thePoint3, this->mPolygonArrays[0]->GetVertexIndex3(k) + theVertexStart);
                if (thePoint1 == thePoint2 || thePoint1 == thePoint3 || thePoint2 == thePoint3)
                    theResult = false;
            }
        }
    }
    if (theResult) {
        for (unsigned long l = 0; l < this->mSubModelCount; l++) {
            theResult = this->mSubModels[l]->Verify();
            if (!theResult)
                break;
        }
    }
    return theResult;
}

// map<unsigned long, long> COMDAT (mSharedFrames)
// TEMPLATE: LLE 0x1002065a

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)
// SYNTHETIC: LLE 0x10020685
// SYNTHETIC: LLE 0x100206a3
// SYNTHETIC: LLE 0x100206af
// SYNTHETIC: LLE 0x100206bb
