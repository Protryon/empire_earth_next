// C:\EEX\Low-Level Engine\Graphics Engine\GEModel Save.cpp (file name inferred)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1001e2fc..0x1001eb9f: GEModel::Save and its chunk writers (.cem "SSMF" version 2.0). A unit of its own: it has
// no U2D key init chains (the other GEModel part files all do) and ends with its own UList pool registrations
// (0x1001eb7b..0x1001eb9f) before GEModel.cpp's chains. The leading pool COMDATs (0x1001e2fc..0x1001e4e8) have no
// source here. The chunk writers are Rust (rust/crates/ee_ge model_save.rs): Save gathers the model and hands it over.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/UReferenceCounter.h"
#include "lle/FSFileSpec.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEVertexArray.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "GERustMath.h"
#include "GERustIO.h"

// UString's bytes for Rust (its length, not up to a NUL: what UString::Save writes)
static EeGeBytes GERustBytes(const UString& inString)
{
    EeGeBytes theBytes;
    theBytes.data = reinterpret_cast<const uint8_t*>((const char*)inString);
    theBytes.length = inString.GetLength();
    return theBytes;
}

// FUNCTION: LLE 0x1001e4e8
// Saves the model as .cem 2.0 (a top model first recomputes bounds, sorts tag points and removes scale),
// then its sub-models; on a chunk error rewinds the file. The chunks are Rust's (ee_ge_model_save) over flat
// copies of the arrays.
long GEModel::Save(FSFileSpec& inFile)
{
    long theResult;
    long thePosition;
    unsigned long i;
    unsigned long j;
    if (this->mParentModel == NULL) {
        this->ComputeBoundingVolumes();
        this->SortTagPoints();
        this->mGraphicsScale = 1.0f;
        this->RemoveScale();
    }
    if (inFile.GetIsOpen()) {
        theResult = inFile.GetPosition(thePosition);
        if (theResult == kErrorOK) {
            EeGeCemSave theView;
            std::vector<uint32_t> theSizes(this->mLODCount);
            std::vector<uint32_t> theIndices;
            for (i = 0; i < this->mLODCount; i++) {
                GEPolygonArray* theArray = this->mPolygonArrays[i];
                theSizes[i] = (uint32_t)theArray->GetSize();
                for (j = 0; j < theArray->GetSize(); j++) {
                    theIndices.push_back((uint32_t)theArray->GetVertexIndex1(j));
                    theIndices.push_back((uint32_t)theArray->GetVertexIndex2(j));
                    theIndices.push_back((uint32_t)theArray->GetVertexIndex3(j));
                }
            }
            std::vector<uint32_t> theRanges(this->mMaterialCount * this->mLODCount * 2 + 1);
            std::vector<EeGeCemSaveMaterial> theMaterials(this->mMaterialCount);
            for (i = 0; i < this->mMaterialCount; i++) {
                GEMaterial* theMaterial = this->mMaterials[i];
                EeGeCemSaveMaterial& theTarget = theMaterials[i];
                theTarget.name = GERustBytes(theMaterial->GetName());
                theTarget.texture_index = (uint32_t)theMaterial->GetTextureIndex();
                theTarget.ranges = &theRanges[i * this->mLODCount * 2];
                for (j = 0; j < this->mLODCount; j++) {
                    theRanges[(i * this->mLODCount + j) * 2] = (uint32_t)theMaterial->GetPolygonStartingIndex(j);
                    theRanges[(i * this->mLODCount + j) * 2 + 1] = (uint32_t)theMaterial->GetPolygonRangeLength(j);
                }
                theTarget.vertex_start = (uint32_t)theMaterial->GetVertexStartingIndex();
                theTarget.vertex_length = (uint32_t)theMaterial->GetVertexRangeLength();
                theTarget.texture_filename = GERustBytes(this->mTextureFilenames[i]);
            }
            std::vector<EeGeBytes> theNames(this->mTagPointCount + 1);
            for (i = 0; i < this->mTagPointCount; i++)
                theNames[i] = GERustBytes(this->mTagPointNames[i]);
            std::vector<float> theVertices(this->mFrameCount * this->mVertexCount * 8 + 1);
            std::vector<EeGeCemSaveFrame> theFrames(this->mFrameCount + 1);
            for (i = 0; i < this->mFrameCount; i++) {
                EeGeCemSaveFrame& theFrame = theFrames[i];
                float* theValues = &theVertices[i * this->mVertexCount * 8];
                GE3DPoint thePoint;
                GEVector theNormal;
                GETextureCoordinate theCoordinate;
                for (j = 0; j < this->mVertexCount; j++) {
                    this->mVertexArrays[i]->GetPoint(thePoint, j);
                    this->mVertexArrays[i]->GetNormal(theNormal, j);
                    this->mVertexArrays[i]->GetTextureCoordinate(theCoordinate, j);
                    float* theVertex = theValues + j * 8;
                    theVertex[0] = thePoint.GetX();
                    theVertex[1] = thePoint.GetY();
                    theVertex[2] = thePoint.GetZ();
                    theVertex[3] = theNormal.GetX();
                    theVertex[4] = theNormal.GetY();
                    theVertex[5] = theNormal.GetZ();
                    theVertex[6] = theCoordinate.GetU();
                    theVertex[7] = theCoordinate.GetV();
                }
                theFrame.radius = this->mBoundingRadii[i];
                theFrame.vertices = theValues;
                theFrame.tag_points = this->mTagPointCount > 0 ? GERust(this->mFrameTagPoints[i][0]) : NULL;
                theFrame.transform = *GERust(this->mInternalTransforms[i]);
                theFrame.bbox[0] = *GERust(this->mBoundingBoxes[i].GetBottomLeftFront());
                theFrame.bbox[1] = *GERust(this->mBoundingBoxes[i].GetTopRightBack());
            }
            theView.counts[0] = (uint32_t)this->mPolygonCount;
            theView.counts[1] = (uint32_t)this->mVertexCount;
            theView.counts[2] = (uint32_t)this->mTagPointCount;
            theView.counts[3] = (uint32_t)this->mMaterialCount;
            theView.counts[4] = (uint32_t)this->mFrameCount;
            theView.counts[5] = (uint32_t)this->mSubModelCount;
            theView.counts[6] = (uint32_t)this->mLODCount;
            theView.name = GERustBytes(this->mName);
            theView.center = *GERust(this->mCenterPoint);
            theView.lod_sizes = theSizes.empty() ? NULL : &theSizes[0];
            theView.lod_indices = theIndices.empty() ? NULL : &theIndices[0];
            theView.materials = theMaterials.empty() ? NULL : &theMaterials[0];
            theView.tag_point_names = &theNames[0];
            theView.frames = &theFrames[0];
            EeGeFieldIo theIO = GERustFieldIO(inFile);
            theResult = ee_ge_model_save(&theIO, &theView);
            if (theResult == kErrorOK) {
                for (i = 0; i < this->mSubModelCount; i++)
                    this->mSubModels[i]->Save(inFile);
            } else
                inFile.SetPosition(thePosition, kFileOriginBegin, false);
        }
    } else
        theResult = kErrorNotOpen;
    if (theResult == kErrorNativeError)
        ESGetNativeError();
    return theResult;
}

// FUNCTION: LLE 0x1001eb5a
// Does nothing (empty in the retail build)
void GEModel::SaveData(FSFileSpec& inFile)
{
}

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)
// SYNTHETIC: LLE 0x1001eb5d
// SYNTHETIC: LLE 0x1001eb7b
// SYNTHETIC: LLE 0x1001eb87
// SYNTHETIC: LLE 0x1001eb93
