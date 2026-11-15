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
#include "rust/ee_ge.h"
#include <stdio.h>
#include <map>
#include "lle/UWideString.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "GEOpenFormats.h"   // the data path's host for ee_of_find_model (a model's .glb/.gltf)
#include "lle/GEPhysicalModel.h"
#include "GERustViewport.h"
#include <stddef.h>
#include "lle/URandom.h"       // U2DFractal.h
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DFractal.h"
#include "lle/GETextureSet.h"
#include "lle/GETerrainMesh.h"
#include "GERustIO.h"

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

// ---- GEModel Load (GEModel Load.cpp, folded here in R4.3 part 16)

// Allocates the model's arrays from the counts (AllocateInternalData, as the counts chunk did) and stores the model
// where the original's chunk readers stored it, then creates and fills the sub-models; returns the model's result
// (a sub-model's is ignored, as the original's). A model whose counts the reader refused is left as constructed.
long GEModel::StoreLoaded(const EeGeCemModel* inModel, std::map<unsigned long, long>& ioTextures)
{
    std::vector<EeGeSharedFrame> theShared;
    for (std::map<unsigned long, long>::const_iterator theIterator = ioTextures.begin(); theIterator != ioTextures.end();
         theIterator++) {
        EeGeSharedFrame theEntry;
        theEntry.frame = (uint32_t)theIterator->first;
        theEntry.offset = (int32_t)theIterator->second;
        theShared.push_back(theEntry);
    }
    return ee_ge_mod_store_loaded(this->mRust, inModel, theShared.empty() ? NULL : &theShared[0], theShared.size());
}

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)

// ---- GEModelManager (GEModelManager.cpp, folded here in R4.3 part 16)

// ---- the open-format lookups' host: the calls ee_openformats find.rs makes on the data path and its files
intptr_t GEOpenFormatsHost(EeOfFileOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    FSFileSpec* theFile = (FSFileSpec*)inObj;
    unsigned long theRead = 0;
    switch (inOp) {
    case EeOfFileOp::PathFind: {
        FSFileSpec* theFound = NULL;
        return ((FSPath*)inObj)->FindFile(UString((const char*)a), theFound) == kErrorOK ? (intptr_t)theFound : 0;
    }
    case EeOfFileOp::FileOpen: return theFile->Open((unsigned long)a);
    case EeOfFileOp::FileLength: return theFile->GetLength(*(unsigned long*)a);
    case EeOfFileOp::FileRead:
        return theFile->Read((unsigned char*)a, (unsigned long)b, theRead) == kErrorOK ? (intptr_t)theRead : -1;
    case EeOfFileOp::FileClose: return theFile->Close();
    case EeOfFileOp::FileDelete: delete theFile; return 0;
    case EeOfFileOp::Verify: ESVerify(__FILE__, __LINE__, (const char*)a); return 0;   // a file that does not decode
    }
    return 0;
}
// ---- end of the open-format lookups' host

// CreateModel's and CreateSubModel's counts and flags, through the Rust object to the host
struct GEModelCreateArgs {
    unsigned long mPolygonCount, mVertexCount, mMaterialCount, mTagPointCount, mFrameCount, mSubModelCount, mFlags;
};

// The calls the manager makes on other objects (EeGeManOp), in the original's statements
intptr_t GEModelManager::Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    GEModelManager* theManager = (GEModelManager*)inOwner;
    GEModel* theModel = (GEModel*)inObj;
    const GEModelCreateArgs* theArgs = (const GEModelCreateArgs*)a;
    switch (inOp) {
    case EeGeManOp::LockCreate: {
        TSCriticalSection* theLock = NULL;
        ESVERIFY_AT(TSScheduler::CreateCriticalSection(theLock) == keseOK, 39);
        return (intptr_t)theLock;
    }
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
        return (intptr_t)theModel->mReferenceCount;
    case EeGeManOp::BaseDereference:
        theModel->UReferenceCounter::Dereference();
        return 0;
    case EeGeManOp::DumpHeader:
        fprintf(*(FSFileSpec*)inObj, "\n\n*********Model Manager - Current Loaded Files In Memory*********\n\n");
        return 0;
    case EeGeManOp::DumpLine:
        fprintf(*(FSFileSpec*)b, "The %s model file is loaded in memory with a reference count of %ld.\n",
                (const char*)a, theModel->mReferenceCount);
        return 0;
    case EeGeManOp::ModelNew:
        return (intptr_t)new GEModel(theArgs->mPolygonCount, theArgs->mVertexCount, theArgs->mMaterialCount,
                                     theArgs->mTagPointCount, theArgs->mFrameCount, theArgs->mSubModelCount,
                                     UString((const char*)b), theArgs->mFlags, theManager);
    case EeGeManOp::ModelNewSub: {
        GEModel* theSubModel = new GEModel(theArgs->mPolygonCount, theArgs->mVertexCount, theArgs->mMaterialCount,
                                           theArgs->mTagPointCount, theArgs->mFrameCount, theArgs->mSubModelCount,
                                           theModel->GetName(), theArgs->mFlags, theManager);
        theModel->SetSubModel(theSubModel, (unsigned long)b);
        theSubModel->UReferenceCounter::Dereference();
        return (intptr_t)theSubModel;
    }
    case EeGeManOp::ModelFindFile: {
        std::vector<EeGeSharedFrame> theShared;   // the frames the model shares, in the map's order
        for (const std::pair<const unsigned long, long>& theFrame : *(std::map<unsigned long, long>*)b)
            theShared.push_back({(uint32_t)theFrame.first, (int32_t)theFrame.second});
        return ee_of_find_model(&GEOpenFormatsHost, inObj, (const char*)a, theShared.data(), theShared.size(),
                                (EeGeCemModel**)c);
    }
    case EeGeManOp::ModelNewEmpty:
        return (intptr_t)new GEModel(theManager);
    case EeGeManOp::ModelStoreLoaded:
        return theModel->StoreLoaded((const EeGeCemModel*)a, *(std::map<unsigned long, long>*)b);
    case EeGeManOp::ModelSizeOf:
        return (intptr_t)theModel->SizeOf();
    case EeGeManOp::ModelVerify:
        theModel->Verify();
        return 0;
    case EeGeManOp::ModelFreeFile:
        ee_ge_cem_free((EeGeCemModel*)inObj);
        return 0;
    default:
        return 0;
    }
}

// Creates the model manager with a model search path and its lock
GEModelManager::GEModelManager(FSPath* inPath)
{
    this->mRust = ee_ge_modman_new(&GEModelManager::Host, this, inPath);
}

// Destroys the model manager: removes every unreferenced model, releases the path and the lock
GEModelManager::~GEModelManager()
{
    ee_ge_modman_free(this->mRust);
}

// Creates a new model registered under a numbered name ("GEModelManager::CreateModel(name_N)")
GEModel* GEModelManager::CreateModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                                     unsigned long inFrameCount, unsigned long inSubModelCount, const UString& inName, unsigned long inFlags)
{
    GEModelCreateArgs theArgs = {inPolygonCount, inVertexCount, inMaterialCount, inTagPointCount, inFrameCount,
                                 inSubModelCount, inFlags};
    return (GEModel*)ee_ge_modman_create(this->mRust, &theArgs, inName);
}

// Creates a model and attaches it to a parent model as the sub-model at an index (the parent holds it)
GEModel* GEModelManager::CreateSubModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                                        unsigned long inFrameCount, unsigned long inSubModelCount, unsigned long inFlags,
                                        unsigned long inIndex, GEModel* inParent)
{
    GEModelCreateArgs theArgs = {inPolygonCount, inVertexCount, inMaterialCount, inTagPointCount, inFrameCount,
                                 inSubModelCount, inFlags};
    return (GEModel*)ee_ge_modman_create_sub(this->mRust, &theArgs, inIndex, inParent);
}

// Writes the list of loaded models and their reference counts to a file
void GEModelManager::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    ee_ge_modman_dump(this->mRust, &inFile);
}

unsigned long GEModelManager::GetMemoryFootprint()
{
    return ee_ge_modman_footprint(this->mRust);
}

// Returns the loaded model of that name, else finds its glTF (.glb, .gltf), stores it and registers it under the
// name; NULL when there is none or it does not load. The cache's key is the name as given ("models\x"; the
// original's was "<name>.cem")
GEModel* GEModelManager::LoadModel(const UString& inName, std::map<unsigned long, long>& ioTextures)
{
    return (GEModel*)ee_ge_modman_load(this->mRust, inName, &ioTextures);
}

// Releases every model nobody else references; returns 5 when some are still in use
long GEModelManager::RemoveAllModels()
{
    return ee_ge_modman_remove_all(this->mRust);
}

// Releases a model if nobody else references it; 3 when it is unknown, 5 when it is still in use
long GEModelManager::RemoveModel(GEModel* inModel)
{
    return ee_ge_modman_remove(this->mRust, inModel);
}

// Replaces the model search path, keeping a reference to the new one
void GEModelManager::SetModelPath(FSPath* inPath)
{
    ee_ge_modman_set_path(this->mRust, inPath);
}

// ---- GEPhysicalModel (GEPhysicalModel.cpp, folded here in R4.3 part 16)

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

// Destroys the placed model, releasing its model
GEPhysicalModel::~GEPhysicalModel()
{
    if (this->mModel)
        this->mModel->Dereference();
}

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
void GEPhysicalModel::SetModel(GEModel* inModel)
{
    if (this->mModel)
        this->mModel->Dereference();
    this->mModel = inModel;
    if (inModel)
        inModel->Reference();
}

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
    GETextureSet* theSet = (GETextureSet*)inObj;
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
        delete theSet;
        return 0;
    case EeGeTmOp::SetWidth:
        return (intptr_t)theSet->GetNumTexturesInWidth();
    case EeGeTmOp::SetHeight:
        return (intptr_t)theSet->GetNumTexturesInHeight();
    case EeGeTmOp::SetCoverage:
        return (intptr_t)theSet->GetTextureTileCoverage();
    case EeGeTmOp::SetTexture:
        return (intptr_t)theSet->GetTexture((unsigned long)a, (unsigned long)b);
    case EeGeTmOp::SetTextureAt:
        return (intptr_t)theSet->GetTextureAt((unsigned long)a, (unsigned long)b);
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

void GETerrainMesh::ComputeShorelines(unsigned long inX, unsigned long inY) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 7, 0); }

void GETerrainMesh::ComputeVertexNormal(unsigned long inX, unsigned long inY) { ee_ge_tm_point_edit(this->mRust, inX, inY, 4, 0, 0.0f); }

void GETerrainMesh::CreateFlatMesh(long inWidth, long inHeight) { ee_ge_tm_create_flat_mesh(this->mRust, inWidth, inHeight); }

void GETerrainMesh::GetMeshNormal(float inX, float inY, GEVector& outNormal) const { ee_ge_tm_mesh_normal(this->mRust, inX, inY, GERust(outNormal)); }

float GETerrainMesh::GetMeshZ(float inX, float inY) const { return ee_ge_tm_mesh_z(this->mRust, inX, inY); }

void GETerrainMesh::GetClosestTile(const GE3DLine& inLine, long& outX, long& outY) const { ee_ge_tm_closest_tile_line(this->mRust, GERust(inLine), &outX, &outY); }

void GETerrainMesh::GetClosestTile(const GE3DPoint& inPoint, long& outX, long& outY) const { ee_ge_tm_closest_tile_point(this->mRust, GERust(inPoint), &outX, &outY); }

bool GETerrainMesh::Intersects(const GE3DLine& inLine, float& outDistance) const { return ee_ge_tm_intersects(this->mRust, GERust(inLine), &outDistance); }

long GETerrainMesh::RegisterColorSet(std::vector<unsigned long>& inColorSet, unsigned long& outID)
{
    outID = ee_ge_tm_register_color_set(this->mRust, inColorSet.data(), inColorSet.size());
    return 0;
}

long GETerrainMesh::RegisterTextureSet(GETextureSet* inTextureSet, unsigned long& outID)
{
    outID = ee_ge_tm_register_texture_set(this->mRust, inTextureSet);
    return 0;
}

void GETerrainMesh::SetColorFromColorTableIndex(unsigned long inX, unsigned long inY, unsigned char inIndex) { ee_ge_tm_point_edit(this->mRust, inX, inY, 1, inIndex, 0.0f); }

void GETerrainMesh::SetColorSet(unsigned long inX, unsigned long inY, unsigned long inColorSetID) { ee_ge_tm_point_edit(this->mRust, inX, inY, 2, inColorSetID, 0.0f); }

void GETerrainMesh::SetWaterColorSet(unsigned long inX, unsigned long inY) { ee_ge_tm_point_edit(this->mRust, inX, inY, 3, 0, 0.0f); }

void GETerrainMesh::SetColorTable(std::vector<GEColor>& inColorTable) { ee_ge_tm_set_color_table(this->mRust, reinterpret_cast<const float(*)[3]>(inColorTable.data()), inColorTable.size()); }

void GETerrainMesh::SetGlobalWaterState(unsigned long inWaterSetID, unsigned long inColorSetID, unsigned long inAnimationPeriod, float inTextureScale)
    { ee_ge_tm_set_global_water_state(this->mRust, inWaterSetID, inColorSetID, inAnimationPeriod, inTextureScale); }

void GETerrainMesh::UpdateFogOfWar(bool inAllVisible) { ee_ge_tm_update_fog_of_war(this->mRust, inAllVisible); }

void GETerrainMesh::UpdateWaterAnimation() { ee_ge_tm_update_water_animation(this->mRust); }

bool GETerrainMesh::GetIsSpaceTile(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_space_tile(this->mRust, inX, inY); }

void GETerrainMesh::UpdateVisibleTerrain(const GETransformation&, GEViewport& inViewport, unsigned long, unsigned long, unsigned long, bool inOrthographic)
    { ee_ge_tm_update_visible_terrain(this->mRust, GERust(inViewport), inOrthographic); }

void GETerrainMesh::SetIsSpace(unsigned long inX, unsigned long inY, bool inIsSpace) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 6, inIsSpace); }

long GETerrainMesh::Load(unsigned long, unsigned long, FSFile& inFile)
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    return ee_ge_tm_load(this->mRust, &theIO);
}

long GETerrainMesh::Save(FSFile& inFile) const
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    return ee_ge_tm_save(this->mRust, &theIO);
}

// The header's accessors (inline in the original: GEBoundingBox.cpp emitted them)
void GETerrainMesh::DereferenceTile(unsigned long inX, unsigned long inY) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 1, 0); }
void GETerrainMesh::ReferenceTile(unsigned long inX, unsigned long inY) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 0, 0); }
void GETerrainMesh::SetTileReferenceCount(unsigned long inX, unsigned long inY, unsigned long inCount) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 2, inCount); }
void GETerrainMesh::SetTextureSet(unsigned long inX, unsigned long inY, unsigned long inTextureSetID) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 3, inTextureSetID); }
void GETerrainMesh::SetIsWater(unsigned long inX, unsigned long inY, bool inIsWater) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 5, inIsWater); }
void GETerrainMesh::SetZ(unsigned long inX, unsigned long inY, float inZ) { ee_ge_tm_point_edit(this->mRust, inX, inY, 0, 0, inZ); }
float GETerrainMesh::GetZ(unsigned long inX, unsigned long inY) const { return ee_ge_tm_point(this->mRust, inX, inY)->z; }
unsigned char GETerrainMesh::GetColorTableIndex(unsigned long inX, unsigned long inY) const { return ee_ge_tm_point(this->mRust, inX, inY)->color_table_index; }
GEVector* GETerrainMesh::GetVertexNormal(unsigned long inX, unsigned long inY) const { return (GEVector*)ee_ge_tm_point(this->mRust, inX, inY)->normal; }
const GEColor& GETerrainMesh::GetVertexColor(unsigned long inX, unsigned long inY) const { return *reinterpret_cast<const GEColor*>(ee_ge_tm_vertex_color(this->mRust, inX, inY)); }
unsigned long GETerrainMesh::GetTextureSetID(unsigned long inX, unsigned long inY) const { return ee_ge_tm_tile(this->mRust, inX, inY)->texture_set_id; }
unsigned char GETerrainMesh::GetWaterInfo(unsigned long inX, unsigned long inY) const { return ee_ge_tm_tile(this->mRust, inX, inY)->water_info; }
bool GETerrainMesh::GetIsWaterTile(unsigned long inX, unsigned long inY) const { return (ee_ge_tm_tile(this->mRust, inX, inY)->water_info & kWaterInfoWaterMask) != 0; }
bool GETerrainMesh::GetIsValidPoint(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_valid(this->mRust, inX, inY, false); }
bool GETerrainMesh::GetIsValidTile(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_valid(this->mRust, inX, inY, true); }
bool GETerrainMesh::GetIsSpaceMap() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::SpaceTileCount) > 0; }
const GE3DPlane& GETerrainMesh::GetFrontPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 0)); }
const GE3DPlane& GETerrainMesh::GetBackPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 1)); }
const GE3DPlane& GETerrainMesh::GetLeftPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 2)); }
const GE3DPlane& GETerrainMesh::GetRightPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 3)); }
float GETerrainMesh::GetVisibleSpaceTilePercentage() const { return ee_ge_tm_get_f(this->mRust, EeGeTmVal::VisibleSpacePercentage); }
float GETerrainMesh::GetVisibleWaterTilePercentage() const { return ee_ge_tm_get_f(this->mRust, EeGeTmVal::VisibleWaterPercentage); }
unsigned long GETerrainMesh::GetGlobalWaterSetID() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::GlobalWaterSetId); }
unsigned long GETerrainMesh::GetNumVisibleTiles() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::NumVisibleTiles); }
unsigned long GETerrainMesh::GetNumVisibleTileRows() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::VisibleTileRows); }
unsigned int GETerrainMesh::NumSpaceCliffTextures() { return ee_ge_tm_get(this->mRust, EeGeTmVal::SpaceCliffTextureCount); }
void GETerrainMesh::SetSpaceCliffTextures(std::vector<GETexture*>& inTextures) { ee_ge_tm_set_space_cliff_textures(this->mRust, (void* const*)inTextures.data(), inTextures.size()); }
GETextureSet* GETerrainMesh::GetRegisteredTextureSet(unsigned long inID) const { return (GETextureSet*)ee_ge_tm_texture_set(this->mRust, inID); }
void GETerrainMesh::GetTerrainVisibleBottomLeftIndices(unsigned long& outX, unsigned long& outY) const
{
    outX = ee_ge_tm_get(this->mRust, EeGeTmVal::BottomLeftX);
    outY = ee_ge_tm_get(this->mRust, EeGeTmVal::BottomLeftY);
}
void GETerrainMesh::GetTerrainVisibleTileDimensions(unsigned long& outWidth, unsigned long& outHeight) const
{
    outWidth = ee_ge_tm_get(this->mRust, EeGeTmVal::VisibleTileWidth);
    outHeight = ee_ge_tm_get(this->mRust, EeGeTmVal::VisibleTileHeight);
}
void GETerrainMesh::GetVisibleTileRow(unsigned long inIndex, long& outY, long& outStartX, long& outEndX) const
{
    long theRow[3];
    ee_ge_tm_visible_row(this->mRust, inIndex, theRow);
    outY = theRow[0];
    outStartX = theRow[1];
    outEndX = theRow[2];
}
