// C:\EEX\Low-Level Engine\Graphics Engine\GEModel.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1001eb9f..0x100206c7: GEModel's ctors, dtor and core members, then its map COMDAT and the UList<GELOD*> pool init
// pair (lle/GELOD.h via GEModel.h).
//
// port (R4.3, ninth part): the model is a whole Rust object (ee_ge modobj.rs): the constructors' fields and
// AllocateInternalData, the destructor's releases, the bounding volumes, normals, transforms and scale removal (over
// model_geom.rs), the frame setters over the sub-models, SetSubModel, SetTexture, SizeOf, SortTagPoints and Verify.
// This unit is its shell: the forwarders and the host, which makes each call the original made on another object
// (the rasterizer, the vertex and polygon arrays, the materials, the texture manager and textures, the manager, the
// sub-models' reference counts, the UStrings).
#include <windows.h>
#include <stdint.h>
#include <vector>

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
#include "lle/GEPolygonArray.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "GERustMath.h"
#include "lle/FSMemoryFile.h"
#include "rust/base/math.h"
#include "rust/render/ge.h"
#include "rust/render/openformats.h"
#include <stdio.h>
#include <map>
#include "lle/UWideString.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "GEOpenFormats.h"   // the data path's host for ee_of_find_model (a model's .glb/.gltf)
#include "lle/GEPhysicalModel.h"
#include <stddef.h>
#include "lle/URandom.h"       // U2DFractal.h
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DFractal.h"
#include "lle/GETextureSet.h"
#include "lle/GETerrainMesh.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// The calls the model makes on other objects (EeGeModOp), in the original's statements; the materials, the
// strings' records, the references taken and the sub-models' objects are read in place (ee_ge cppobj.rs)
static_assert(sizeof(GEModel) == 3 * sizeof(void*), "ee_ge cppobj.rs CppCounted (the others: GEBitmap.cpp)");
intptr_t GEModel::Host(void* inOwner, EeGeModOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    case EeGeModOp::RasterActive:
        return (intptr_t)GERasterizer::GetActiveRasterizer();
    case EeGeModOp::RasterVertexArray:
        return (intptr_t)((GERasterizer*)inObj)->CreateVertexArray((unsigned long)a, (unsigned long)b);
    case EeGeModOp::RasterPolygonArray:
        return (intptr_t)((GERasterizer*)inObj)->CreatePolygonArray((unsigned long)a);
    case EeGeModOp::RasterScreenId:
        return (intptr_t)((GERasterizer*)inObj)->GetScreenID();
    case EeGeModOp::RasterTextureManager:
        return (intptr_t)((GERasterizer*)inObj)->GetScreen((unsigned long)a)->GetTextureManager();
    // (the arrays' other calls are on their Rust arrays: ee_ge modobj.rs array_op)
    case EeGeModOp::VertexDelete:
        delete ((GEVertexArray*)inObj);
        return 0;
    case EeGeModOp::PolygonDelete:
        delete ((GEPolygonArray*)inObj);
        return 0;
    case EeGeModOp::MaterialNew:
        return (intptr_t)new GEMaterial((unsigned long)a, (unsigned long)b);
    case EeGeModOp::MaterialDereference:
        ((GEMaterial*)inObj)->UReferenceCounter::Dereference();
        return 0;
    case EeGeModOp::MaterialSizeOf:
        return (intptr_t)((GEMaterial*)inObj)->SizeOf();
    case EeGeModOp::ModelNew:
        return (intptr_t)new GEModel((GEModelManager*)inObj);
    case EeGeModOp::ModelBaseDereference:
        ((GEModel*)inObj)->UReferenceCounter::Dereference();
        return 0;
    case EeGeModOp::ManagerRemove:
        ((GEModelManager*)inObj)->RemoveModel((GEModel*)a);
        return 0;
    case EeGeModOp::StrNew:
        return (intptr_t)new UString;
    case EeGeModOp::StrDelete:
        delete ((UString*)inObj);
        return 0;
    case EeGeModOp::StrArrayNew:
        return (intptr_t)new UString[(size_t)a];
    case EeGeModOp::StrArrayDelete:
        delete[] ((UString*)inObj);
        return 0;
    case EeGeModOp::TexLoad:
        return (intptr_t)((GETextureManager*)inObj)->LoadTexture(*(const UString*)a, *(const GETextureDescription*)b);
    case EeGeModOp::TexMinFilter:
        ((GETexture*)inObj)->SetStageMinFilterMode((unsigned long)a, (unsigned long)b);
        return 0;
    case EeGeModOp::TexMagFilter:
        ((GETexture*)inObj)->SetStageMagFilterMode((unsigned long)a, (unsigned long)b);
        return 0;
    case EeGeModOp::TexMipFilter:
        ((GETexture*)inObj)->SetStageMipFilterMode((unsigned long)a, (unsigned long)b);
        return 0;
    }
    (void)inOwner;
    return 0;
}

// ---- GEModel Load (GEModel Load.cpp, folded here in R4.3 part 16)

// (StoreLoaded: ee_ge_mod_store_loaded, from the manager's host below: the arrays allocated from the counts, the model
// stored where the original's chunk readers stored it, the sub-models made and filled)

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)

// ---- GEModelManager (GEModelManager.cpp, folded here in R4.3 part 16)

// ---- the open-format lookups' host: the calls ee_openformats find.rs makes on the data path and its files
intptr_t GEOpenFormatsHost(EeOfFileOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    unsigned long theRead = 0;
    switch (inOp) {
    case EeOfFileOp::PathFind: {
        FSFileSpec* theFound = NULL;
        return ((FSPath*)inObj)->FindFile(UString((const char*)a), theFound) == kErrorOK ? (intptr_t)theFound : 0;
    }
    case EeOfFileOp::FileOpen: return ((FSFileSpec*)inObj)->Open((unsigned long)a);
    case EeOfFileOp::FileLength: return ((FSFileSpec*)inObj)->GetLength(*(unsigned long*)a);
    case EeOfFileOp::FileRead:
        return ((FSFileSpec*)inObj)->Read((unsigned char*)a, (unsigned long)b, theRead) == kErrorOK ? (intptr_t)theRead : -1;
    case EeOfFileOp::FileClose: return ((FSFileSpec*)inObj)->Close();
    case EeOfFileOp::FileDelete: delete ((FSFileSpec*)inObj); return 0;
    case EeOfFileOp::Verify: ESVerify(__FILE__, __LINE__, (const char*)a); return 0;   // a file that does not decode
    }
    return 0;
}
// ---- end of the open-format lookups' host

// The calls the manager makes on other objects (EeGeManOp), in the original's statements
intptr_t GEModelManager::Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    case EeGeManOp::LockCreate: return TSScheduler::CreateCriticalSection(*(TSCriticalSection**)a);
    case EeGeManOp::LockVerify: ESVERIFY_AT(a == keseOK, 39); return 0;
    case EeGeManOp::LockDestroy:
        TSScheduler::DestroyCriticalSection((TSCriticalSection*)inObj);
        return 0;
    case EeGeManOp::LockEnter:
        ((TSCriticalSection*)inObj)->Enter();
        return 0;
    case EeGeManOp::LockLeave:
        ((TSCriticalSection*)inObj)->Leave();
        return 0;
    case EeGeManOp::PathReference:
        ((FSPath*)inObj)->Reference();
        return 0;
    case EeGeManOp::PathDereference:
        ((FSPath*)inObj)->Dereference();
        return 0;
    case EeGeManOp::RefCount:
        return (intptr_t)((GEModel*)inObj)->mReferenceCount;
    case EeGeManOp::BaseDereference:
        ((GEModel*)inObj)->UReferenceCounter::Dereference();
        return 0;
    case EeGeManOp::DumpText: fprintf(*(FSFileSpec*)inObj, "%s", (const char*)a); return 0;
    case EeGeManOp::ModelNew: return (intptr_t)new GEModel(*(const EeGeModelCreate*)a, UString((const char*)b), (GEModelManager*)inOwner);
    case EeGeManOp::ModelNewSub: return (intptr_t)new GEModel(*(const EeGeModelCreate*)a, *(const UString*)b, (GEModelManager*)inOwner);
    case EeGeManOp::ModelName: return (intptr_t)&((GEModel*)inObj)->GetName();
    case EeGeManOp::ModelSetSub: ((GEModel*)inObj)->SetSubModel((GEModel*)a, (unsigned long)b); return 0;
    case EeGeManOp::ModelFindFile:
        return ee_of_find_model(&GEOpenFormatsHost, inObj, (const char*)a, (const EeGeSharedFrames*)b, (EeGeCemModel**)c);
    case EeGeManOp::ModelNewEmpty:
        return (intptr_t)new GEModel(((GEModelManager*)inOwner));
    case EeGeManOp::ModelStoreLoaded:
        return ee_ge_mod_store_loaded(((GEModel*)inObj)->mRust, (const EeGeCemModel*)a, (const EeGeSharedFrames*)b);
    case EeGeManOp::ModelSizeOf:
        return (intptr_t)((GEModel*)inObj)->SizeOf();
    case EeGeManOp::ModelVerify:
        ((GEModel*)inObj)->Verify();
        return 0;
    case EeGeManOp::ModelFreeFile:
        ee_ge_cem_free((EeGeCemModel*)inObj);
        return 0;
    default:
        return 0;
    }
}

// ---- GEPhysicalModel (GEPhysicalModel.cpp, folded here in R4.3 part 16)

// kLODFraction (0x10097b78) and ComputeTriangleBox (0x10021ebc), the model against model walk's, are Rust
// (ee_ge physical_walk.rs, physical.rs)

// The walks read the models through a table (ee_ge physical_walk.rs, EeGePhysicalModels): the handles are the GEModel*
// the walks were given, read in place (R4.3 part 17: their Rust objects, ee_ge_physical_models; a triangle's corners
// from the model's Rust arrays, ee_ge_mod_triangle), but for the exact retry, which stays C++.
namespace {
// the scaled walk's exact retry: a placed model of its own (it references and releases the model)
bool PMRetryLine(const void* inModel, const EeGeTransformation* inLocation, unsigned long inFrame, float inScale,
                 const EeGe3DLine* inLine, unsigned long inLOD)
{
    float theDistance;
    GEPhysicalModel theModel(const_cast<GEModel*>(static_cast<const GEModel*>(inModel)),
                             *reinterpret_cast<const GETransformation*>(inLocation), inScale);
    theModel.SetFrame(inFrame);
    return theModel.Intersects(*reinterpret_cast<const GE3DLine*>(inLine), inLOD, theDistance, true);
}
EeGePhysicalModels PMTable()
{
    EeGePhysicalModels theTable;
    ee_ge_physical_models(ee_ge_mod_triangle, PMRetryLine, &theTable);
    return theTable;
}
const EeGePhysicalModels kPMModels = PMTable();
}   // namespace
const EeGePhysicalModels* GEPhysicalModelTable() { return &kPMModels; }   // the unit picker's walks (ee_gfx pick_walk.rs)

// Places a model in the world at a location and scale, holding a reference to it, at frame 0
GEPhysicalModel::GEPhysicalModel(GEModel* inModel, const GETransformation& inLocation, float inScale)
    : mLocation(inLocation), mModel(NULL), mScale(inScale), mFrame(0)
{
    this->SetModel(inModel);
}

// Destroys the placed model, releasing its model
GEPhysicalModel::~GEPhysicalModel() { this->SetModel(NULL); }

// Copies another placed model (model reference, location, scale, frame)
GEPhysicalModel& GEPhysicalModel::operator=(const GEPhysicalModel& inModel)
{
    this->SetModel(inModel.mModel);
    this->SetLocationAndScale(inModel.mLocation, inModel.mScale);
    this->mFrame = inModel.mFrame;
    return *this;
}

// Tests a line against a model and its sub-models: bounding sphere, then box (LOD > 0), then triangles (LOD > 1);
// with inExact the hit must lie on the segment
// (the walk is Rust: ee_ge physical_walk.rs, line_walk)
bool GEPhysicalModel::Intersects(const GETransformation& inLocation, const GEModel* inModel, unsigned long inFrame,
                                 const GE3DLine& inLine, unsigned long inLOD, float& outDistance, bool inExact) const
{
    return ee_ge_physical_line_walk(&kPMModels, this->mScale, GERust(inLocation), inModel, inFrame, GERust(inLine),
                                    inLOD, &outDistance, inExact);
}

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

// Replaces the placed model's model, keeping a reference to the new one
// (the old model released first: ee_ge_pm_set_model)
void GEPhysicalModel::SetModel(GEModel* inModel) { ee_ge_pm_set_model(reinterpret_cast<void**>(&this->mModel), inModel); }

// vector<vector<GEBoundingBox> > / vector<GEBoundingBox> COMDATs (Intersects of two models)

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)

// ---- GETerrainMesh (GETerrainMesh.cpp, folded here in R4.3 part 16)

// the records the renderer reads through the mesh are the Rust object's
#define GETM_PIN(cpp, rust, cppField, rustField) static_assert(offsetof(cpp, cppField) == offsetof(rust, rustField), #cppField)
static_assert(sizeof(GETerrainMesh::GETMRenderVertex) == sizeof(EeGeTmRenderVertex), "GETMRenderVertex");
GETM_PIN(GETerrainMesh::GETMRenderVertex, EeGeTmRenderVertex, mColor, color);
GETM_PIN(GETerrainMesh::GETMRenderVertex, EeGeTmRenderVertex, mVisibility, visibility);
GETM_PIN(GETerrainMesh::GETMRenderVertex, EeGeTmRenderVertex, mClipFlags, clip_flags);
GETM_PIN(GETerrainMesh::GETMRenderVertex, EeGeTmRenderVertex, mLinkedVisibility, linked_visibility);
static_assert(sizeof(GETerrainMesh::GETMPolygon) == sizeof(EeGeTmPolygon), "GETMPolygon");
GETM_PIN(GETerrainMesh::GETMPolygon, EeGeTmPolygon, mAlpha, alpha);
GETM_PIN(GETerrainMesh::GETMPolygon, EeGeTmPolygon, mUV, uv);
static_assert(sizeof(GEColor) == 3 * sizeof(float) && sizeof(GE3DPlane) == sizeof(EeGe3DPlane), "GEColor, GE3DPlane");
static_assert(GETM_VISIBLE_POINTS == EE_GE_TM_SIDE, "GETM_VISIBLE_POINTS");

// The calls the mesh makes on other objects (EeGeTmOp), in the original's statements
static intptr_t TerrainMeshHost(void*, EeGeTmOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    switch (inOp) {
    case EeGeTmOp::Time:
        return (intptr_t)TMGetTime();
    case EeGeTmOp::FractalNew:
        return (intptr_t) new U2DFractal(64, 0.0f, 10.5f, (unsigned long)a);
    case EeGeTmOp::FractalDelete:
        delete (U2DFractal*)inObj;
        return 0;
    case EeGeTmOp::FractalValue:
        *(float*)a = ((U2DFractal*)inObj)->GetValue((unsigned long)(b >> 16), (unsigned long)(b & 0xffff));
        return 0;
    case EeGeTmOp::FractalCompute:
        ((U2DFractal*)inObj)->ComputeFractal(0.0f, 10.5f, (unsigned long)a);
        return 0;
    case EeGeTmOp::Quantize:
        return (intptr_t)((const GEVector*)a)->GetQuantizedVersion();
    case EeGeTmOp::SetDelete:
        delete ((GETextureSet*)inObj);
        return 0;
    case EeGeTmOp::SetWidth:
        return (intptr_t)((GETextureSet*)inObj)->GetNumTexturesInWidth();
    case EeGeTmOp::SetHeight:
        return (intptr_t)((GETextureSet*)inObj)->GetNumTexturesInHeight();
    case EeGeTmOp::SetCoverage:
        return (intptr_t)((GETextureSet*)inObj)->GetTextureTileCoverage();
    case EeGeTmOp::SetTexture:
        return (intptr_t)((GETextureSet*)inObj)->GetTexture((unsigned long)a, (unsigned long)b);
    case EeGeTmOp::SetTextureAt:
        return (intptr_t)((GETextureSet*)inObj)->GetTextureAt((unsigned long)a, (unsigned long)b);
    case EeGeTmOp::TexReference:
        ((GETexture*)inObj)->Reference();
        return 0;
    case EeGeTmOp::TexDereference:
        ((GETexture*)inObj)->Dereference();
        return 0;
    }
    return 0;
}

GETerrainMesh::GETerrainMesh() { this->mRust = ee_ge_tm_new(TerrainMeshHost, this); }

GETerrainMesh::~GETerrainMesh() { ee_ge_tm_destroy(this->mRust); }

long GETerrainMesh::Load(unsigned long, unsigned long, FSFile& inFile)
{
    return ee_ge_tm_load(this->mRust, &inFile);
}

long GETerrainMesh::Save(FSFile& inFile) const
{
    return ee_ge_tm_save(this->mRust, &inFile);
}

