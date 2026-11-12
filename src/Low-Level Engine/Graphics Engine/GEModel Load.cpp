// C:\EEX\Low-Level Engine\Graphics Engine\GEModel Load.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1001c294..0x1001cd3b: GEModel's current-format loader (Load, LoadCurrentVersion, Load*Chunk) (file name inferred).
// The game's models are glTF files only (docs/RUST_PLAN.md D9): Rust decodes one (rust/crates/ee_openformats,
// ee_of_gltf_to_model) into the model the .cem chunk readers stored, and StoreLoaded stores it. The .cem reader
// (Load) and the version-1 conversion ('GEModel Legacy.cpp') are retired.
#include <windows.h>
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
#include "lle/FSMemoryFile.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "GERustMath.h"
#include "rust/ee_ge.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x1001c294
// SYNTHETIC: LLE 0x1001c29e
// SYNTHETIC: LLE 0x1001c2ae
// SYNTHETIC: LLE 0x1001c2ba
// SYNTHETIC: LLE 0x1001c2c4
// SYNTHETIC: LLE 0x1001c2ce
// SYNTHETIC: LLE 0x1001c2de
// SYNTHETIC: LLE 0x1001c2ea
// SYNTHETIC: LLE 0x1001c2f4
// SYNTHETIC: LLE 0x1001c2fe
// SYNTHETIC: LLE 0x1001c30e
// SYNTHETIC: LLE 0x1001c31a
// SYNTHETIC: LLE 0x1001c324
// SYNTHETIC: LLE 0x1001c32e
// SYNTHETIC: LLE 0x1001c33e
// SYNTHETIC: LLE 0x1001c34a
// SYNTHETIC: LLE 0x1001c354
// SYNTHETIC: LLE 0x1001c35e
// SYNTHETIC: LLE 0x1001c36e
// SYNTHETIC: LLE 0x1001c37a
// SYNTHETIC: LLE 0x1001c384
// SYNTHETIC: LLE 0x1001c38e
// SYNTHETIC: LLE 0x1001c39e
// SYNTHETIC: LLE 0x1001c3aa

// UString from Rust's bytes (no NUL inside)
static UString GERustString(const EeGeBytes& inBytes)
{
    std::vector<char> theText(inBytes.data, inBytes.data + inBytes.length);
    theText.push_back(0);
    return UString(&theText[0]);
}

// Allocates the model's arrays from the counts (AllocateInternalData, as the counts chunk did) and stores the model
// where the original's chunk readers stored it, then creates and fills the sub-models; returns the model's result
// (a sub-model's is ignored, as the original's). A model whose counts the reader refused is left as constructed.
long GEModel::StoreLoaded(const EeGeCemModel* inModel, std::map<unsigned long, long>& ioTextures)
{
    EeGeCemInfo theInfo;
    unsigned long i;
    size_t k;
    ee_ge_cem_info(inModel, &theInfo);
    if (!theInfo.allocated)
        return theInfo.result;
    this->mPolygonCount = theInfo.polygon_count;
    this->mVertexCount = theInfo.vertex_count;
    this->mTagPointCount = theInfo.tag_point_count;
    this->mMaterialCount = theInfo.material_count;
    this->mFrameCount = theInfo.frame_count;
    this->mSubModelCount = theInfo.sub_model_count;
    this->mLODCount = theInfo.lod_count;
    this->AllocateInternalData(0, ioTextures);
    if (theInfo.name.length > 0)
        this->mName = GERustString(theInfo.name);
    float* theCenter[3] = {&this->mCenterPoint.mX, &this->mCenterPoint.mY, &this->mCenterPoint.mZ};
    for (k = 0; k < theInfo.center_count; k++)
        *theCenter[k] = theInfo.center[k];
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    for (i = 0; i < this->mLODCount; i++) {
        EeGeCemLod theLOD;
        ee_ge_cem_lod(inModel, i, &theLOD);
        if (theLOD.created)
            this->mPolygonArrays[i] = theRasterizer->CreatePolygonArray(theLOD.count);
        for (k = 0; k < theLOD.index_count; k++) {
            unsigned long thePolygon = (unsigned long)(k / 3);
            if (k % 3 == 0)
                this->mPolygonArrays[i]->SetVertexIndex1(thePolygon, theLOD.indices[k]);
            else if (k % 3 == 1)
                this->mPolygonArrays[i]->SetVertexIndex2(thePolygon, theLOD.indices[k]);
            else
                this->mPolygonArrays[i]->SetVertexIndex3(thePolygon, theLOD.indices[k]);
        }
    }
    for (i = 0; i < this->mMaterialCount; i++) {
        EeGeCemMaterial theMaterial;
        ee_ge_cem_material(inModel, i, &theMaterial);
        GEMaterial* theTarget = this->mMaterials[i];
        if (theMaterial.name.length > 0)
            theTarget->SetName(GERustString(theMaterial.name));
        // texture index, (start, length) per LOD, vertex start, vertex length
        for (k = 0; k < theMaterial.value_count; k++) {
            unsigned long theValue = theMaterial.values[k];
            if (k == 0)
                theTarget->SetTextureIndex(theValue);
            else if (k <= 2 * this->mLODCount) {
                if ((k - 1) % 2 == 0)
                    theTarget->SetPolygonStartingIndex(theValue, (unsigned long)((k - 1) / 2));
                else
                    theTarget->SetPolygonRangeLength(theValue, (unsigned long)((k - 1) / 2));
            } else if (k == 2 * this->mLODCount + 1)
                theTarget->SetVertexStartingIndex(theValue);
            else
                theTarget->SetVertexRangeLength(theValue);
        }
        if (theMaterial.texture_filename.length > 0)
            this->mTextureFilenames[i] = GERustString(theMaterial.texture_filename);
    }
    for (i = 0; i < this->mTagPointCount; i++) {
        EeGeBytes theName = ee_ge_cem_tag_point_name(inModel, i);
        if (theName.length > 0)
            this->mTagPointNames[i] = GERustString(theName);
    }
    for (i = 0; i < this->mFrameCount; i++) {
        EeGeCemFrame theFrame;
        ee_ge_cem_frame(inModel, i, &theFrame);
        if (theFrame.has_radius)
            this->mBoundingRadii[i] = theFrame.radius;
        GEVertexArray* theArray = this->mVertexArrays[theFrame.target];
        for (k = 0; k < theFrame.point_count; k++)
            theArray->SetPoint(*reinterpret_cast<const GE3DPoint*>(&theFrame.points[k]), (unsigned long)k);
        for (k = 0; k < theFrame.normal_count; k++)
            theArray->SetNormal(*reinterpret_cast<const GEVector*>(&theFrame.normals[k]), (unsigned long)k);
        for (k = 0; k < theFrame.uv_count; k++)
            theArray->SetTextureCoordinate(GETextureCoordinate(theFrame.uvs[2 * k], theFrame.uvs[2 * k + 1]),
                                           (unsigned long)k);
        for (k = 0; k < theFrame.tag_point_count; k++)
            this->mFrameTagPoints[theFrame.target][k] = *reinterpret_cast<const GE3DPoint*>(&theFrame.tag_points[k]);
        for (k = 0; k < theFrame.transform_count; k++)
            this->mInternalTransforms[i].SetElement((unsigned long)(k / 4), (unsigned long)(k % 4),
                                                    theFrame.transform[k]);
        // GEBoundingBox::Load: the corners as read, the dimensions once both were (no reordering)
        EeGeBoundingBox* theBox = GERust(this->mBoundingBoxes[i]);
        float* theCorners[6] = {&theBox->bottom_left_front.x, &theBox->bottom_left_front.y,
                                &theBox->bottom_left_front.z, &theBox->top_right_back.x,
                                &theBox->top_right_back.y, &theBox->top_right_back.z};
        for (k = 0; k < theFrame.bbox_count; k++)
            *theCorners[k] = theFrame.bbox[k];
        if (theFrame.bbox_count == 6) {
            theBox->width = theBox->top_right_back.x - theBox->bottom_left_front.x;
            theBox->height = theBox->top_right_back.z - theBox->bottom_left_front.z;
            theBox->depth = theBox->top_right_back.y - theBox->bottom_left_front.y;
        }
    }
    for (i = 0; i < theInfo.sub_models_loaded; i++) {
        this->mSubModels[i] = new GEModel(this->mManager);
        this->mSubModels[i]->mParentModel = this;
        this->mSubModels[i]->StoreLoaded(ee_ge_cem_sub_model(inModel, i), ioTextures);
    }
    return theInfo.result;
}

// FUNCTION: LLE 0x1001cad5
// Loads every material's texture for one screen (point mip filter), then does the same for the sub-models
long GEModel::LoadTexturesForScreen(const GETextureDescription& inDescription, unsigned long inScreen)
{
    GETextureManager* theManager = GERasterizer::GetActiveRasterizer()->GetScreen(inScreen)->GetTextureManager();
    unsigned long i;
    for (i = 0; i < this->mMaterialCount; i++) {
        GEMaterial* theMaterial = this->mMaterials[i];
        theMaterial->SetTexture(NULL, inScreen);
        if (!(this->mTextureFilenames[i].GetLength() > 0))
            continue;
        GETexture* theTexture = theManager->LoadTexture(this->mTextureFilenames[i], inDescription);
        if (!theTexture)
            continue;
        theMaterial->SetTexture(theTexture, inScreen);
        theTexture->SetStageMinFilterMode(1, -1);
        theTexture->SetStageMagFilterMode(1, -1);
        theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    }
    for (i = 0; i < this->mSubModelCount; i++)
        this->mSubModels[i]->LoadTexturesForScreen(inDescription, inScreen);
    return kErrorOK;
}

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)
// SYNTHETIC: LLE 0x1001ccf9
// SYNTHETIC: LLE 0x1001cd17
// SYNTHETIC: LLE 0x1001cd23
// SYNTHETIC: LLE 0x1001cd2f
