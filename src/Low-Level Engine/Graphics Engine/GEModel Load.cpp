// C:\EEX\Low-Level Engine\Graphics Engine\GEModel Load.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1001c294..0x1001cd3b: GEModel's current-format loader (Load, LoadCurrentVersion, Load*Chunk) (file name inferred).
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
#include "lle/FSMemoryFile.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"

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

// The .cem header version, read by LoadHeaderChunk and tested by Load (0x1010fac6 / 0x1010fac4); global: the
// LoadLegacy* members in 'GEModel Legacy.cpp' test it too
unsigned short sMinorVersion;
unsigned short sMajorVersion;

// FUNCTION: LLE 0x1001c3b4
// Loads a .cem model (through a memory copy when preloading), legacy or current format by header version;
// on failure rewinds the file to where it started
long GEModel::Load(FSFileSpec& inFile, std::map<unsigned long, long>& ioTextures, bool inPreload)
{
    FSMemoryFile theMemoryFile;
    FSFileSpec* theFile = &inFile;
    long theResult = kErrorOK;
    unsigned long theLength;
    long thePosition;
    if (inPreload == true && dynamic_cast<FSMemoryFile*>(&inFile) == NULL) {
        inFile.GetLength(theLength);
        theMemoryFile.Load(inFile, theLength);
        theMemoryFile.SetPosition(0, kFileOriginBegin, false);
        theFile = &theMemoryFile;
    }
    if (theFile->GetIsOpen() == true) {
        theResult = theFile->GetPosition(thePosition);
        if (theResult == kErrorOK) {
            theResult = this->LoadHeaderChunk(*theFile);
            if (theResult == kErrorOK) {
                if (sMajorVersion < 2)
                    theResult = this->LoadLegacyVersion(*theFile, ioTextures);
                else
                    theResult = this->LoadCurrentVersion(*theFile, ioTextures);
            }
            if (theResult != kErrorOK)
                theFile->SetPosition(thePosition, kFileOriginBegin, false);
        }
        if (theResult == kErrorNativeError)
            ESGetNativeError();
    }
    return theResult;
}

// FUNCTION: LLE 0x1001c4ac
// Loads a current-format model body chunk by chunk, then its sub-models; returns the first chunk error
long GEModel::LoadCurrentVersion(FSFileSpec& inFile, std::map<unsigned long, long>& ioTextures)
{
    long theResult;
    if ((theResult = this->LoadCountsChunk(inFile, ioTextures)) == kErrorOK &&
        (theResult = this->LoadMiscellaneousChunk(inFile)) == kErrorOK && (theResult = this->LoadPolygonsChunk(inFile)) == kErrorOK &&
        (theResult = this->LoadMaterialsChunk(inFile)) == kErrorOK && (theResult = this->LoadTagPointsChunk(inFile)) == kErrorOK &&
        (theResult = this->LoadFrameChunk(inFile, ioTextures)) == kErrorOK) {
        for (unsigned long i = 0; i < this->mSubModelCount; i++) {
            this->mSubModels[i] = new GEModel(this->mManager);
            this->mSubModels[i]->mParentModel = this;
            this->mSubModels[i]->Load(inFile, ioTextures, false);
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1001c589
// Reads the polygon, vertex, tag point, material, frame, sub-model and LOD counts and allocates the model's data
long GEModel::LoadCountsChunk(FSFileSpec& inFile, std::map<unsigned long, long>& ioTextures)
{
    long theResult;
    unsigned long thePolygonCount;
    unsigned long theVertexCount;
    unsigned long theSubModelCount = 0;
    unsigned long theTagPointCount = 0;
    unsigned long theLODCount = 1;
    unsigned long theMaterialCount;
    unsigned long theFrameCount;
    if ((theResult = inFile.Read(thePolygonCount)) != kErrorOK)
        return theResult;
    if (thePolygonCount <= 0)
        return kErrorFailed;
    if ((theResult = inFile.Read(theVertexCount)) != kErrorOK)
        return theResult;
    if (theVertexCount <= 0)
        return kErrorFailed;
    if ((theResult = inFile.Read(theTagPointCount)) != kErrorOK)
        return theResult;
    if ((theResult = inFile.Read(theMaterialCount)) != kErrorOK)
        return theResult;
    if (theMaterialCount <= 0)
        return kErrorFailed;
    if ((theResult = inFile.Read(theFrameCount)) != kErrorOK)
        return theResult;
    if (theFrameCount > 0) {
        if ((theResult = inFile.Read(theSubModelCount)) != kErrorOK)
            return theResult;
        if ((theResult = inFile.Read(theLODCount)) != kErrorOK)
            return theResult;
        this->mPolygonCount = thePolygonCount;
        this->mVertexCount = theVertexCount;
        this->mTagPointCount = theTagPointCount;
        this->mMaterialCount = theMaterialCount;
        this->mFrameCount = theFrameCount;
        this->mSubModelCount = theSubModelCount;
        this->mLODCount = theLODCount;
        return this->AllocateInternalData(0, ioTextures);
    }
    return kErrorFailed;
}

// FUNCTION: LLE 0x1001c691
// Reads each frame's radius, vertices, tag points, transform and bounding box; returns the first read error
long GEModel::LoadFrameChunk(FSFileSpec& inFile, std::map<unsigned long, long>& ioTextures)
{
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mFrameCount; i++) {
        unsigned long theFrame;
        std::map<unsigned long, long>::iterator theIterator = ioTextures.find(i);
        if (theIterator != ioTextures.end())
            theFrame = this->mFrameCount - 1;
        else
            theFrame = i;
        if ((theResult = inFile.Read(this->mBoundingRadii[i])) != kErrorOK)
            break;
        if ((theResult = this->LoadVertexSubChunk(inFile, theFrame)) != kErrorOK)
            break;
        if ((theResult = this->LoadTagPointsSubChunk(inFile, theFrame)) != kErrorOK)
            break;
        if ((theResult = this->mInternalTransforms[i].Load(inFile)) != kErrorOK)
            break;
        theResult = this->mBoundingBoxes[i].Load(inFile);
    }
    return theResult;
}

// FUNCTION: LLE 0x1001c74a
// Checks the "SSMF" tag and reads the version (2.x or 1.0-1.3 accepted)
long GEModel::LoadHeaderChunk(FSFileSpec& inFile)
{
    unsigned long theRead;
    char theTag[5];
    long theResult = inFile.Read(theTag, 4, theRead);
    if (theResult != kErrorOK)
        return theResult;
    // A short tag read is a truncated file: fail instead of returning the read's success.
    if (theRead != 4)
        return kErrorFailed;
    theTag[4] = 0;
    if (strcmp(theTag, "SSMF") == 0) {
        theResult = inFile.Read(sMajorVersion);
        if (theResult == kErrorOK) {
            if (sMajorVersion == 1 || sMajorVersion == 2) {
                theResult = inFile.Read(sMinorVersion);
                if (theResult == kErrorOK && sMinorVersion != 0 && sMinorVersion != 1 && sMinorVersion != 2 &&
                    sMinorVersion != 3)
                    theResult = kErrorFailed;
            } else
                theResult = kErrorFailed;
        }
    } else
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x1001c7dd
// Reads each material's name, texture index, per-LOD polygon ranges, vertex range and texture file name
long GEModel::LoadMaterialsChunk(FSFileSpec& inFile)
{
    UString theName;
    long theResult = kErrorOK;
    unsigned long theValue;
    for (unsigned long i = 0; i < this->mMaterialCount && theResult == kErrorOK; i++) {
        theResult = theName.Load(inFile);
        if (theResult == kErrorOK) {
            this->mMaterials[i]->SetName(theName);
            theResult = inFile.Read(theValue);
            if (theResult == kErrorOK) {
                this->mMaterials[i]->SetTextureIndex(theValue);
                for (unsigned long j = 0; j < this->mLODCount && theResult == kErrorOK; j++) {
                    theResult = inFile.Read(theValue);
                    if (theResult != kErrorOK)
                        continue;
                    this->mMaterials[i]->SetPolygonStartingIndex(theValue, j);
                    theResult = inFile.Read(theValue);
                    if (theResult == kErrorOK)
                        this->mMaterials[i]->SetPolygonRangeLength(theValue, j);
                }
                if (theResult == kErrorOK) {
                    theResult = inFile.Read(theValue);
                    if (theResult == kErrorOK) {
                        this->mMaterials[i]->SetVertexStartingIndex(theValue);
                        theResult = inFile.Read(theValue);
                        if (theResult == kErrorOK) {
                            this->mMaterials[i]->SetVertexRangeLength(theValue);
                            theResult = this->mTextureFilenames[i].Load(inFile);
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1001c962
// Reads the model name and centre point
long GEModel::LoadMiscellaneousChunk(FSFileSpec& inFile)
{
    long theResult = this->mName.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mCenterPoint.Load(inFile);
    return theResult;
}

// FUNCTION: LLE 0x1001c98b
// Reads each LOD's polygon vertex indices, creating the polygon arrays for LODs above 0
long GEModel::LoadPolygonsChunk(FSFileSpec& inFile)
{
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    long theResult = kErrorOK;
    unsigned long theIndex;
    for (long i = 0; i < this->mLODCount; i++) {
        unsigned long theCount;
        theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            if (i > 0)
                this->mPolygonArrays[i] = theRasterizer->CreatePolygonArray(theCount);
            for (unsigned long j = 0; j < theCount && theResult == kErrorOK; j++) {
                theResult = inFile.Read(theIndex);
                if (theResult != kErrorOK)
                    continue;
                this->mPolygonArrays[i]->SetVertexIndex1(j, theIndex);
                theResult = inFile.Read(theIndex);
                if (theResult != kErrorOK)
                    continue;
                this->mPolygonArrays[i]->SetVertexIndex2(j, theIndex);
                theResult = inFile.Read(theIndex);
                if (theResult == kErrorOK)
                    this->mPolygonArrays[i]->SetVertexIndex3(j, theIndex);
            }
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1001ca9b
// Reads the tag point names; returns the first read error
long GEModel::LoadTagPointsChunk(FSFileSpec& inFile)
{
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mTagPointCount && theResult == kErrorOK; i++)
        theResult = this->mTagPointNames[i].Load(inFile);
    return theResult;
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

// FUNCTION: LLE 0x1001cbb5
// Reads a frame's tag point positions; returns the first read error
long GEModel::LoadTagPointsSubChunk(FSFileSpec& inFile, unsigned long inFrame)
{
    GE3DPoint thePoint;
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mTagPointCount; i++) {
        if ((theResult = thePoint.Load(inFile)) != kErrorOK)
            break;
        this->mFrameTagPoints[inFrame][i] = thePoint;
    }
    return theResult;
}

// FUNCTION: LLE 0x1001cc1e
// Reads a frame's vertices (point, normal, texture coordinate); returns the first read error
long GEModel::LoadVertexSubChunk(FSFileSpec& inFile, unsigned long inFrame)
{
    GEVector theNormal;
    GE3DPoint thePoint;
    GETextureCoordinate theCoordinate;
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mVertexCount; i++) {
        if ((theResult = thePoint.Load(inFile)) != kErrorOK)
            break;
        this->mVertexArrays[inFrame]->SetPoint(thePoint, i);
        if ((theResult = theNormal.Load(inFile)) != kErrorOK)
            break;
        this->mVertexArrays[inFrame]->SetNormal(theNormal, i);
        if ((theResult = theCoordinate.Load(inFile)) != kErrorOK)
            break;
        this->mVertexArrays[inFrame]->SetTextureCoordinate(theCoordinate, i);
    }
    return theResult;
}

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)
// SYNTHETIC: LLE 0x1001ccf9
// SYNTHETIC: LLE 0x1001cd17
// SYNTHETIC: LLE 0x1001cd23
// SYNTHETIC: LLE 0x1001cd2f
