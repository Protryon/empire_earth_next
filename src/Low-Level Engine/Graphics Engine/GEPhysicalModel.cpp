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
#include "GERustViewport.h"

// kLODFraction (0x10097b78) and ComputeTriangleBox (0x10021ebc), the model against model walk's, are Rust
// (ee_ge physical_walk.rs, physical.rs)

// The walks read the models through this table (ee_ge physical_walk.rs, EeGePhysicalModels): the handles are the
// GEModel* the walks were given, the arrays behind GEVertexArray's and GEPolygonArray's virtuals stay C++, and the
// index arithmetic (a polygon's vertex index plus the material's first vertex) is the C++'s unsigned long.
namespace {
const GEModel* PMModel(const void* inModel) { return static_cast<const GEModel*>(inModel); }
bool PMHasParent(const void* inModel) { return PMModel(inModel)->GetParentModel() != NULL; }
void PMInternalTransform(const void* inModel, unsigned long inFrame, EeGeTransformation* outTransform)
    { *outTransform = *GERust(PMModel(inModel)->GetInternalTransform(inFrame)); }
void PMCenter(const void* inModel, EeGe3DPoint* outPoint) { *outPoint = *GERust(PMModel(inModel)->GetCenterPoint()); }
float PMRadius(const void* inModel, unsigned long inFrame) { return PMModel(inModel)->GetBoundingRadius(inFrame); }
void PMBox(const void* inModel, unsigned long inFrame, EeGeBoundingBox* outBox)
    { *outBox = *GERust(PMModel(inModel)->GetBoundingBox(inFrame)); }
unsigned long PMMaterialCount(const void* inModel) { return PMModel(inModel)->GetMaterialCount(); }
void PMMaterial(const void* inModel, unsigned long inIndex, unsigned long inLOD, EeGePhysicalMaterial* outMaterial)
{
    GEMaterial* theMaterial = PMModel(inModel)->GetMaterial(inIndex);
    outMaterial->vertex_start = theMaterial->GetVertexStartingIndex();
    outMaterial->polygon_start = theMaterial->GetPolygonStartingIndex(inLOD);
    outMaterial->polygon_length = theMaterial->GetPolygonRangeLength(inLOD);
}
void PMTriangle(const void* inModel, unsigned long inFrame, unsigned long inLOD, unsigned long inPolygon,
                unsigned long inVertexStart, EeGe3DPoint* outPoints)
{
    GEModel* theModel = const_cast<GEModel*>(PMModel(inModel));
    GEVertexArray* theVertices = theModel->GetVertexArray(inFrame);
    GEPolygonArray* thePolygons = theModel->GetPolygonArray(inLOD);
    GE3DPoint thePoint;
    theVertices->GetPoint(thePoint, thePolygons->GetVertexIndex1(inPolygon) + inVertexStart);
    outPoints[0] = *GERust(thePoint);
    theVertices->GetPoint(thePoint, thePolygons->GetVertexIndex2(inPolygon) + inVertexStart);
    outPoints[1] = *GERust(thePoint);
    theVertices->GetPoint(thePoint, thePolygons->GetVertexIndex3(inPolygon) + inVertexStart);
    outPoints[2] = *GERust(thePoint);
}
unsigned long PMSubModelCount(const void* inModel) { return PMModel(inModel)->GetSubModelCount(); }
const void* PMSubModel(const void* inModel, unsigned long inIndex) { return PMModel(inModel)->GetSubModel(inIndex); }
unsigned long PMLODCount(const void* inModel) { return PMModel(inModel)->GetLODCount(); }
unsigned long PMPolygonCount(const void* inModel, unsigned long inLOD) { return PMModel(inModel)->GetPolygonCount(inLOD); }
// the scaled walk's exact retry: a placed model of its own (it references and releases the model)
bool PMRetryLine(const void* inModel, const EeGeTransformation* inLocation, unsigned long inFrame, float inScale,
                 const EeGe3DLine* inLine, unsigned long inLOD)
{
    float theDistance;
    GEPhysicalModel theModel(const_cast<GEModel*>(PMModel(inModel)),
                             *reinterpret_cast<const GETransformation*>(inLocation), inScale);
    theModel.SetFrame(inFrame);
    return theModel.Intersects(*reinterpret_cast<const GE3DLine*>(inLine), inLOD, theDistance, true);
}
const EeGePhysicalModels kPMModels = { PMHasParent, PMInternalTransform, PMCenter, PMRadius, PMBox, PMMaterialCount,
                                       PMMaterial, PMTriangle, PMSubModelCount, PMSubModel, PMLODCount, PMPolygonCount,
                                       PMRetryLine };
}   // namespace

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
// (the walk is Rust: ee_ge physical_walk.rs, line_walk)
bool GEPhysicalModel::Intersects(const GETransformation& inLocation, const GEModel* inModel, unsigned long inFrame,
                                 const GE3DLine& inLine, unsigned long inLOD, float& outDistance, bool inExact) const
{
    return ee_ge_physical_line_walk(&kPMModels, this->mScale, GERust(inLocation), inModel, inFrame, GERust(inLine),
                                    inLOD, &outDistance, inExact);
}

// FUNCTION: LLE 0x10021404
// Tests a model scaled per axis against this placed model, sub-models included;
// in exact mode a top model with no hit retries as a uniformly scaled line test
// (the walk is Rust: ee_ge physical_walk.rs, scaled_walk)
bool GEPhysicalModel::Intersects(const GETransformation& inLocation, const GEModel* inModel, unsigned long inFrame,
                                 const GE3DLine& inLine, bool inExact, unsigned long inLOD, float inX, float inY,
                                 float inZ) const
{
    return ee_ge_physical_scaled_walk(&kPMModels, GERust(this->mLocation), this->mModel, this->mScale, this->mFrame,
                                      GERust(inLocation), inModel, inFrame, GERust(inLine), inExact, inLOD, inX, inY,
                                      inZ);
}

// FUNCTION: LLE 0x1002179e
// Tests this placed model against another model: bounding spheres, then boxes (LOD > 0),
// then triangle boxes at a reduced LOD (LOD > 1); recurses into this model's sub-models
// (the walk is Rust: ee_ge physical_walk.rs, model_walk)
bool GEPhysicalModel::Intersects(const GETransformation& inLocation, const GEModel* inModel,
                                 const GETransformation& inOtherLocation, const GETransformation& inOtherInverse,
                                 float inScale, const GEModel* inOtherModel, unsigned long inFrame,
                                 unsigned long inLOD) const
{
    return ee_ge_physical_model_walk(&kPMModels, GERust(this->mLocation), this->mModel, this->mScale, this->mFrame,
                                     GERust(inLocation), inModel, GERust(inOtherLocation), GERust(inOtherInverse),
                                     inScale, inOtherModel, inFrame, inLOD);
}

// FUNCTION: LLE 0x10021fc0
// Returns whether the placed model can be visible in a viewport: bounding sphere against the four planes and the
// clip distances, or its box against the orthographic rectangle
// (the test is Rust: ee_ge physical.rs, model_visible)
bool GEPhysicalModel::Intersects(const GEViewport& inViewport, bool inOrthographic) const
{
    return ee_ge_physical_visible(GERust(this->mLocation), this->mScale,
                                  GERust(this->mModel->GetInternalTransform(this->mFrame)),
                                  GERust(this->mModel->GetCenterPoint()), this->mModel->GetBoundingRadius(this->mFrame),
                                  GERust(this->mModel->GetBoundingBox(this->mFrame)), GERust(inViewport),
                                  inOrthographic);
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
