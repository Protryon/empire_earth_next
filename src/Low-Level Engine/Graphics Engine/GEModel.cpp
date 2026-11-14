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

// A UString from Rust's bytes (no NUL inside)
static UString GEModelString(const char* inData, size_t inLength)
{
    std::vector<char> theText(inData, inData + inLength);
    theText.push_back(0);
    return UString(&theText[0]);
}

// The calls the model makes on other objects (EeGeModOp), in the original's statements
intptr_t GEModel::Host(void* inOwner, EeGeModOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    GEVertexArray* theVertices = (GEVertexArray*)inObj;
    GEPolygonArray* thePolygons = (GEPolygonArray*)inObj;
    GEMaterial* theMaterial = (GEMaterial*)inObj;
    GEModel* theModel = (GEModel*)inObj;
    UString* theString = (UString*)inObj;
    GETexture* theTexture = (GETexture*)inObj;
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
    case EeGeModOp::VertexDelete:
        delete theVertices;
        return 0;
    case EeGeModOp::VertexGather:
        for (unsigned long i = 0; i < (unsigned long)a; i++)
            theVertices->GetPoint(((GE3DPoint*)b)[i], i);
        return 0;
    case EeGeModOp::VertexGetPoint:
        theVertices->GetPoint(*(GE3DPoint*)b, (unsigned long)a);
        return 0;
    case EeGeModOp::VertexSetPoint:
        theVertices->SetPoint(*(const GE3DPoint*)b, (unsigned long)a);
        return 0;
    case EeGeModOp::VertexGetNormal:
        theVertices->GetNormal(*(GEVector*)b, (unsigned long)a);
        return 0;
    case EeGeModOp::VertexSetNormal:
        theVertices->SetNormal(*(const GEVector*)b, (unsigned long)a);
        return 0;
    case EeGeModOp::VertexSetUv:
        theVertices->SetTextureCoordinate(GETextureCoordinate(((const float*)b)[0], ((const float*)b)[1]),
                                          (unsigned long)a);
        return 0;
    case EeGeModOp::VertexSize:
        return (intptr_t)theVertices->GetSize();
    case EeGeModOp::VertexSizeOf:
        return (intptr_t)theVertices->SizeOf();
    case EeGeModOp::PolygonDelete:
        delete thePolygons;
        return 0;
    case EeGeModOp::PolygonGather:
        for (unsigned long i = 0; i < (unsigned long)a; i++) {
            ((uint32_t*)b)[i * 3] = (uint32_t)thePolygons->GetVertexIndex1(i);
            ((uint32_t*)b)[i * 3 + 1] = (uint32_t)thePolygons->GetVertexIndex2(i);
            ((uint32_t*)b)[i * 3 + 2] = (uint32_t)thePolygons->GetVertexIndex3(i);
        }
        return 0;
    case EeGeModOp::PolygonSetIndex:
        if (c == 1)
            thePolygons->SetVertexIndex1((unsigned long)a, (unsigned long)b);
        else if (c == 2)
            thePolygons->SetVertexIndex2((unsigned long)a, (unsigned long)b);
        else
            thePolygons->SetVertexIndex3((unsigned long)a, (unsigned long)b);
        return 0;
    case EeGeModOp::PolygonSizeOf:
        return (intptr_t)thePolygons->SizeOf();
    case EeGeModOp::MaterialNew:
        return (intptr_t)new GEMaterial((unsigned long)a, (unsigned long)b);
    case EeGeModOp::MaterialDereference:
        theMaterial->UReferenceCounter::Dereference();
        return 0;
    case EeGeModOp::MaterialSetTexture:
        theMaterial->SetTexture((GETexture*)a, (unsigned long)b);
        return 0;
    case EeGeModOp::MaterialRangeLength:
        return (intptr_t)theMaterial->GetPolygonRangeLength((unsigned long)a);
    case EeGeModOp::MaterialStart:
        return (intptr_t)theMaterial->GetPolygonStartingIndex((unsigned long)a);
    case EeGeModOp::MaterialVertexStart:
        return (intptr_t)theMaterial->GetVertexStartingIndex();
    case EeGeModOp::MaterialSizeOf:
        return (intptr_t)theMaterial->SizeOf();
    case EeGeModOp::MaterialSetName:
        theMaterial->SetName(GEModelString((const char*)a, (size_t)b));
        return 0;
    case EeGeModOp::MaterialSetValue:
        if (c == 0)
            theMaterial->SetTextureIndex((unsigned long)a);
        else if (c == 1)
            theMaterial->SetPolygonStartingIndex((unsigned long)a, (unsigned long)b);
        else if (c == 2)
            theMaterial->SetPolygonRangeLength((unsigned long)a, (unsigned long)b);
        else if (c == 3)
            theMaterial->SetVertexStartingIndex((unsigned long)a);
        else
            theMaterial->SetVertexRangeLength((unsigned long)a);
        return 0;
    case EeGeModOp::ModelNew:
        return (intptr_t)new GEModel((GEModelManager*)inObj);
    case EeGeModOp::ModelObj:
        return (intptr_t)theModel->mRust;
    case EeGeModOp::ModelReference:
        theModel->Reference();
        return 0;
    case EeGeModOp::ModelBaseDereference:
        theModel->UReferenceCounter::Dereference();
        return 0;
    case EeGeModOp::ModelRefCount:
        return (intptr_t)theModel->mReferenceCount;
    case EeGeModOp::ManagerRemove:
        ((GEModelManager*)inObj)->RemoveModel((GEModel*)a);
        return 0;
    case EeGeModOp::StrNew:
        return (intptr_t)new UString;
    case EeGeModOp::StrDelete:
        delete theString;
        return 0;
    case EeGeModOp::StrArrayNew:
        return (intptr_t)new UString[(size_t)a];
    case EeGeModOp::StrArrayDelete:
        delete[] theString;
        return 0;
    case EeGeModOp::StrAt:
        return (intptr_t)&theString[a];
    case EeGeModOp::StrAssign:
        *theString = *(const UString*)a;
        return 0;
    case EeGeModOp::StrSetBytes:
        *theString = GEModelString((const char*)a, (size_t)b);
        return 0;
    case EeGeModOp::StrLength:
        return (intptr_t)theString->GetLength();
    case EeGeModOp::StrSizeOf:
        return (intptr_t)theString->SizeOf();
    case EeGeModOp::StrFirst:
        return (intptr_t)((const char*)*theString)[0];
    case EeGeModOp::TexLoad:
        return (intptr_t)((GETextureManager*)inObj)->LoadTexture(*(const UString*)a, *(const GETextureDescription*)b);
    case EeGeModOp::TexMinFilter:
        theTexture->SetStageMinFilterMode((unsigned long)a, (unsigned long)b);
        return 0;
    case EeGeModOp::TexMagFilter:
        theTexture->SetStageMagFilterMode((unsigned long)a, (unsigned long)b);
        return 0;
    case EeGeModOp::TexMipFilter:
        theTexture->SetStageMipFilterMode((unsigned long)a, (unsigned long)b);
        return 0;
    }
    (void)inOwner;
    return 0;
}

// FUNCTION: LLE 0x1001ecbf
// Creates an empty model owned by a model manager, with one LOD and no data allocated
GEModel::GEModel(GEModelManager* inManager)
{
    this->mRust = ee_ge_mod_new(&GEModel::Host, this, inManager);
}

// FUNCTION: LLE 0x1001edb9
// Creates a named model with the given counts and allocates its data (no shared frames)
GEModel::GEModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount,
                 unsigned long inTagPointCount, unsigned long inFrameCount, unsigned long inSubModelCount,
                 const UString& inName, unsigned long inFlags, GEModelManager* inManager)
{
    const unsigned long theCounts[6] = {inPolygonCount, inVertexCount, inMaterialCount, inTagPointCount, inFrameCount,
                                        inSubModelCount};
    this->mRust = ee_ge_mod_new_with_counts(&GEModel::Host, this, theCounts, &inName, inFlags, inManager);
}

// FUNCTION: LLE 0x1001eed6
// Destroys the model: frees its vertex, polygon and tag point arrays (shared frames once),
// releases its materials and sub-models
GEModel::~GEModel()
{
    ee_ge_mod_destroy(this->mRust);
}

// FUNCTION: LLE 0x1001f69a
void GEModel::ComputeBoundingVolumes()
{
    ee_ge_mod_compute_bounding_volumes(this->mRust);
}

// FUNCTION: LLE 0x1001f80c
void GEModel::ComputeModelNormals()
{
    ee_ge_mod_compute_normals(this->mRust);
}

// FUNCTION: LLE 0x1001fa84
// Releases a reference; when only the manager's (and one caller's) reference is left, removes it from the manager
void GEModel::Dereference()
{
    ee_ge_mod_dereference(this->mRust);
}

// FUNCTION: LLE 0x1001fab0
unsigned long GEModel::GetPolygonCount(unsigned long inLOD) const
{
    return ee_ge_mod_polygon_count(this->mRust, inLOD);
}

// FUNCTION: LLE 0x1001fb1d
void GEModel::PrepareTransforms()
{
    ee_ge_mod_prepare_transforms(this->mRust);
}

// FUNCTION: LLE 0x1002009b
void GEModel::SetFractionalFramePosition(float inPosition)
{
    ee_ge_mod_set_fractional(this->mRust, inPosition);
}

// FUNCTION: LLE 0x100200d7
void GEModel::SetGraphicsCurrentFrame(unsigned long inFrame)
{
    ee_ge_mod_set_current_frame(this->mRust, inFrame);
}

// FUNCTION: LLE 0x1002012f
void GEModel::SetGraphicsNextFrame(unsigned long inFrame)
{
    ee_ge_mod_set_next_frame(this->mRust, inFrame);
}

// FUNCTION: LLE 0x10020166
void GEModel::SetSubModel(GEModel* inModel, unsigned long inIndex)
{
    ee_ge_mod_set_sub_model(this->mRust, inModel, inIndex);
}

// FUNCTION: LLE 0x100201b4
void GEModel::SetTexture(GETexture* inTexture, unsigned long inScreen)
{
    ee_ge_mod_set_texture(this->mRust, inTexture, inScreen);
}

// FUNCTION: LLE 0x10020213
unsigned long GEModel::SizeOf()
{
    return ee_ge_mod_size_of(this->mRust);
}

// FUNCTION: LLE 0x10020338
void GEModel::SortTagPoints()
{
    ee_ge_mod_sort_tag_points(this->mRust);
}

// FUNCTION: LLE 0x10020495
bool GEModel::Verify() const
{
    return ee_ge_mod_verify(this->mRust);
}

// map<unsigned long, long> COMDAT (mSharedFrames)
// TEMPLATE: LLE 0x1002065a

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)
// SYNTHETIC: LLE 0x10020685
// SYNTHETIC: LLE 0x100206a3
// SYNTHETIC: LLE 0x100206af
// SYNTHETIC: LLE 0x100206bb
