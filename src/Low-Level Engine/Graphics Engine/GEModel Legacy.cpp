// C:\EEX\Low-Level Engine\Graphics Engine\GEModel Legacy.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100189df..0x1001c294: GEModel's legacy-format (.cem 1.x) loader: LoadLegacyVersion, LoadLegacy*Chunk and
// ConvertLegacyModel, which turns the version-1 data (GEModel +0x160..+0x184) into the current layout (file name
// inferred).
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
#include "lle/FSFileSpec.h"
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

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1001aea3
// TEMPLATE: LLE 0x1001af36
// TEMPLATE: LLE 0x1001be16
// TEMPLATE: LLE 0x1001bf16
// SYNTHETIC: LLE 0x1001c06d
// TEMPLATE: LLE 0x1001ae35
// TEMPLATE: LLE 0x1001aecf
// TEMPLATE: LLE 0x1001af88
// TEMPLATE: LLE 0x1001afdf
// TEMPLATE: LLE 0x1001b082
// TEMPLATE: LLE 0x1001b12f
// SYNTHETIC: LLE 0x1001b554
// SYNTHETIC: LLE 0x1001a304
// SYNTHETIC: LLE 0x1001a32a
// TEMPLATE: LLE 0x1001af62
// TEMPLATE: LLE 0x1001b04b
// TEMPLATE: LLE 0x1001b0c3
// TEMPLATE: LLE 0x1001b186
// TEMPLATE: LLE 0x1001be5d
// TEMPLATE: LLE 0x1001be88
// TEMPLATE: LLE 0x1001bef2
// TEMPLATE: LLE 0x1001bf6a
// TEMPLATE: LLE 0x1001bf87
// TEMPLATE: LLE 0x1001bfdb
// TEMPLATE: LLE 0x1001c007
// TEMPLATE: LLE 0x1001c024
// SYNTHETIC: LLE 0x1001c091

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x100189df
// SYNTHETIC: LLE 0x100189e9
// SYNTHETIC: LLE 0x100189f9
// SYNTHETIC: LLE 0x10018a05
// SYNTHETIC: LLE 0x10018a0f
// SYNTHETIC: LLE 0x10018a19
// SYNTHETIC: LLE 0x10018a29
// SYNTHETIC: LLE 0x10018a35
// SYNTHETIC: LLE 0x10018a3f
// SYNTHETIC: LLE 0x10018a49
// SYNTHETIC: LLE 0x10018a59
// SYNTHETIC: LLE 0x10018a65
// SYNTHETIC: LLE 0x10018a6f
// SYNTHETIC: LLE 0x10018a79
// SYNTHETIC: LLE 0x10018a89
// SYNTHETIC: LLE 0x10018a95
// SYNTHETIC: LLE 0x10018a9f
// SYNTHETIC: LLE 0x10018aa9
// SYNTHETIC: LLE 0x10018ab9
// SYNTHETIC: LLE 0x10018ac5
// SYNTHETIC: LLE 0x10018acf
// SYNTHETIC: LLE 0x10018ad9
// SYNTHETIC: LLE 0x10018ae9
// SYNTHETIC: LLE 0x10018af5

// The .cem header version ('GEModel Load.cpp', LoadHeaderChunk)
extern unsigned short sMinorVersion;
extern unsigned short sMajorVersion;

// The version-1 polygon (0x24 bytes): one texture coordinate per corner, then the corners' vertex indices
struct GEModel::LegacyPolygon {
    GETextureCoordinate mTextureCoordinates[3];   // +0x00
    unsigned long mVertexIndices[3];              // +0x18
};

// A version-1 material or polygon group: the polygons it holds
struct GEModel::LegacyIndexList {
    unsigned long* mIndices;                // +0x00
    unsigned long mCount;                   // +0x04
};

// ConvertLegacyModel's working types (provisional names)
struct LegacyCorner {                       // a polygon corner that uses a split vertex
    unsigned long mPolygon;
    unsigned long mCorner;
};

struct LegacyNewVertex {                    // a vertex added for another texture coordinate
    GETextureCoordinate mTextureCoordinate;
    unsigned long mVertex;
};

struct LegacySplitVertex {
    LegacyNewVertex mVertex;
    std::vector<LegacyCorner> mCorners;
};

struct LegacyMaterialGroup {                // one material of the converted model (sizeof 0x34)
    UString mName;
    UString mTextureFilename;
    std::vector<unsigned long> mPolygons;
    unsigned long mTextureIndex;
};

// The polygon group whose polygons take the player's colour (init chain 0x10018aff, ctor 0x10018b09, atexit
// registrar 0x10018b19, dtor 0x10018b25)
static const UString kPlayerColor("player color");
// SYNTHETIC: LLE 0x10018aff
// SYNTHETIC: LLE 0x10018b09
// SYNTHETIC: LLE 0x10018b19
// SYNTHETIC: LLE 0x10018b25

// FUNCTION: LLE 0x10018b2f
// Converts loaded version-1 model data to the current layout: splits vertices per texture coordinate,
// regroups polygons and vertices by material (player-colour parts split off) and frees the version-1 arrays
long GEModel::ConvertLegacyModel()
{
    LegacySplitVertex theSplitVertex;
    GETextureCoordinate theCoordinate;
    LegacyMaterialGroup theGroup;
    GEVector theNormal;
    GE3DPoint thePoint;
    GETextureCoordinate theTextureCoordinate;
    LegacyCorner theCorner;
    std::vector<LegacySplitVertex> theSplitVertices;
    std::vector<LegacyNewVertex> theNewVertices;
    std::vector<LegacyMaterialGroup> theGroups;
    std::vector<std::vector<unsigned long> > theVertexGroups;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    unsigned long l;
    unsigned long theFrame;
    unsigned long theCount;
    bool theFound;

    for (unsigned long m = 0; m < this->mLegacyPolygonCount; m++) {
        this->mPolygonArrays[0]->SetVertexIndex1(m, this->mLegacyPolygons[m].mVertexIndices[0]);
        this->mPolygonArrays[0]->SetVertexIndex2(m, this->mLegacyPolygons[m].mVertexIndices[1]);
        this->mPolygonArrays[0]->SetVertexIndex3(m, this->mLegacyPolygons[m].mVertexIndices[2]);
    }
    for (theFrame = 0; theFrame < this->mFrameCount; theFrame++) {
        std::map<unsigned long, long>::iterator theIterator = this->mSharedFrames.find(theFrame);
        if (theIterator != this->mSharedFrames.end())
            continue;
        for (m = 0; m < this->mLegacyVertexCount; m++) {
            this->mVertexArrays[theFrame]->SetPoint(this->mLegacyPoints[theFrame][this->mLegacyVertexPoints[m]], m);
            this->mVertexArrays[theFrame]->SetNormal(this->mLegacyNormals[theFrame][m], m);
        }
        for (m = 0; m < this->mLegacyPolygonCount; m++) {
            this->mVertexArrays[theFrame]->SetTextureCoordinate(this->mLegacyPolygons[m].mTextureCoordinates[0],
                                                          this->mLegacyPolygons[m].mVertexIndices[0]);
            this->mVertexArrays[theFrame]->SetTextureCoordinate(this->mLegacyPolygons[m].mTextureCoordinates[1],
                                                          this->mLegacyPolygons[m].mVertexIndices[1]);
            this->mVertexArrays[theFrame]->SetTextureCoordinate(this->mLegacyPolygons[m].mTextureCoordinates[2],
                                                          this->mLegacyPolygons[m].mVertexIndices[2]);
        }
    }

    // a vertex whose corners disagree on the texture coordinate is split, one vertex per coordinate
    for (i = 0; i < this->mLegacyVertexCount; i++) {
        theSplitVertices.clear();
        this->mVertexArrays[0]->GetTextureCoordinate(theCoordinate, i);
        for (j = 0; j < this->mLegacyPolygonCount; j++) {
            for (k = 0; k < 3; k++)
                if (this->mLegacyPolygons[j].mVertexIndices[k] == i)
                    break;
            if (k >= 3 || this->mLegacyPolygons[j].mTextureCoordinates[k] == theCoordinate)
                continue;
            for (l = 0; l < theSplitVertices.size(); l++)
                if (this->mLegacyPolygons[j].mTextureCoordinates[k] == theSplitVertices[l].mVertex.mTextureCoordinate)
                    break;
            if (l < theSplitVertices.size()) {
                theCorner.mPolygon = j;
                theCorner.mCorner = k;
                theSplitVertices[l].mCorners.push_back(theCorner);
            } else {
                theSplitVertex.mVertex.mTextureCoordinate = this->mLegacyPolygons[j].mTextureCoordinates[k];
                theSplitVertex.mVertex.mVertex = i;
                theSplitVertices.push_back(theSplitVertex);
                theCorner.mPolygon = j;
                theCorner.mCorner = k;
                theSplitVertices[theSplitVertices.size() - 1].mCorners.push_back(theCorner);
            }
        }
        for (l = 0; l < theSplitVertices.size(); l++) {
            theNewVertices.push_back(theSplitVertices[l].mVertex);
            for (j = 0; j < theSplitVertices[l].mCorners.size(); j++) {
                if (theSplitVertices[l].mCorners[j].mCorner == 0)
                    this->mPolygonArrays[0]->SetVertexIndex1(theSplitVertices[l].mCorners[j].mPolygon,
                                                       theNewVertices.size() + this->mLegacyVertexCount - 1);
                else if (theSplitVertices[l].mCorners[j].mCorner == 1)
                    this->mPolygonArrays[0]->SetVertexIndex2(theSplitVertices[l].mCorners[j].mPolygon,
                                                       theNewVertices.size() + this->mLegacyVertexCount - 1);
                else
                    this->mPolygonArrays[0]->SetVertexIndex3(theSplitVertices[l].mCorners[j].mPolygon,
                                                       theNewVertices.size() + this->mLegacyVertexCount - 1);
            }
        }
    }

    this->mVertexCount = theNewVertices.size() + this->mLegacyVertexCount;
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    GEVertexArray** theVertexArrays = new GEVertexArray*[this->mFrameCount];
    for (theFrame = 0; theFrame < this->mFrameCount; theFrame++) {
        if (this->mSharedFrames.find(theFrame) != this->mSharedFrames.end())
            continue;
        theVertexArrays[theFrame] = theRasterizer->CreateVertexArray(0, this->mVertexCount);
        for (i = 0; i < this->mLegacyVertexCount; i++) {
            this->mVertexArrays[theFrame]->GetPoint(thePoint, i);
            this->mVertexArrays[theFrame]->GetNormal(theNormal, i);
            this->mVertexArrays[theFrame]->GetTextureCoordinate(theTextureCoordinate, i);
            theVertexArrays[theFrame]->SetPoint(thePoint, i);
            theVertexArrays[theFrame]->SetNormal(theNormal, i);
            theVertexArrays[theFrame]->SetTextureCoordinate(theTextureCoordinate, i);
        }
        for (k = 0; i < this->mVertexCount; i++, k++) {
            theTextureCoordinate = theNewVertices[k].mTextureCoordinate;
            this->mVertexArrays[theFrame]->GetNormal(theNormal, theNewVertices[k].mVertex);
            this->mVertexArrays[theFrame]->GetPoint(thePoint, theNewVertices[k].mVertex);
            theVertexArrays[theFrame]->SetPoint(thePoint, i);
            theVertexArrays[theFrame]->SetNormal(theNormal, i);
            theVertexArrays[theFrame]->SetTextureCoordinate(theTextureCoordinate, i);
        }
        delete this->mVertexArrays[theFrame];
        this->mVertexArrays[theFrame] = theVertexArrays[theFrame];
    }
    for (theFrame = 0; theFrame < this->mFrameCount; theFrame++) {
        std::map<unsigned long, long>::iterator theIterator = this->mSharedFrames.find(theFrame);
        if (theIterator != this->mSharedFrames.end())
            this->mVertexArrays[theFrame] = this->mVertexArrays[theIterator->second + theFrame];
    }

    delete[] theVertexArrays;   // the frames now own the arrays; free the temporary table

    // one material per legacy material, split in two where only some of its polygons take the player colour
    unsigned long theTextureIndex = 0;
    for (i = 0; i < this->mMaterialCount; i++) {
        if (!(this->mLegacyMaterials[i].mCount > 0))
            continue;
        theGroup.mName = this->mMaterials[i]->GetName();
        theGroup.mTextureFilename = this->mTextureFilenames[i];
        theGroup.mTextureIndex = theTextureIndex++;
        theGroups.push_back(theGroup);
        theCount = 0;
        theFound = false;
        for (j = 0; j < this->mLegacyPolygonGroupCount; j++)
            for (l = 0; l < this->mLegacyPolygonGroups[j].mCount; l++)
                for (k = 0; k < this->mLegacyMaterials[i].mCount; k++)
                    if (this->mLegacyMaterials[i].mIndices[k] == this->mLegacyPolygonGroups[j].mIndices[l]) {
                        theCount++;
                        theFound = true;
                    }
        if (theFound) {
            if (theCount == this->mLegacyMaterials[i].mCount) {
                for (k = 0; k < this->mLegacyMaterials[i].mCount; k++) {
                    theGroups[theGroups.size() - 1].mName = "player color";
                    theGroups[theGroups.size() - 1].mPolygons.push_back(this->mLegacyMaterials[i].mIndices[k]);
                }
            } else {
                theGroup.mName = "player color";
                theGroup.mTextureFilename = this->mTextureFilenames[i];
                theGroups.push_back(theGroup);
                for (theCount = 0; theCount < this->mLegacyMaterials[i].mCount; theCount++) {
                    unsigned long thePolygon = this->mLegacyMaterials[i].mIndices[theCount];
                    theFound = false;
                    for (j = 0; j < this->mLegacyPolygonGroupCount; j++)
                        for (l = 0; l < this->mLegacyPolygonGroups[j].mCount; l++)
                            if (thePolygon == this->mLegacyPolygonGroups[j].mIndices[l]) {
                                theGroups[theGroups.size() - 1].mPolygons.push_back(thePolygon);
                                theFound = true;
                            }
                    if (!theFound)
                        theGroups[theGroups.size() - 2].mPolygons.push_back(thePolygon);
                }
            }
        } else {
            for (k = 0; k < this->mLegacyMaterials[i].mCount; k++)
                theGroups[theGroups.size() - 1].mPolygons.push_back(this->mLegacyMaterials[i].mIndices[k]);
        }
    }

    // reorder the polygons by material
    unsigned long* theIndices = new unsigned long[this->mPolygonCount * 3];
    for (i = 0; i < this->mPolygonCount; i++) {
        theIndices[i * 3] = this->mPolygonArrays[0]->GetVertexIndex1(i);
        theIndices[i * 3 + 1] = this->mPolygonArrays[0]->GetVertexIndex2(i);
        theIndices[i * 3 + 2] = this->mPolygonArrays[0]->GetVertexIndex3(i);
    }
    l = 0;
    for (k = 0; k < theGroups.size(); k++)
        for (j = 0; j < theGroups[k].mPolygons.size(); j++) {
            this->mPolygonArrays[0]->SetVertexIndex1(l, theIndices[theGroups[k].mPolygons[j] * 3]);
            this->mPolygonArrays[0]->SetVertexIndex2(l, theIndices[theGroups[k].mPolygons[j] * 3 + 1]);
            this->mPolygonArrays[0]->SetVertexIndex3(l, theIndices[theGroups[k].mPolygons[j] * 3 + 2]);
            l++;
        }

    // and the vertices by the first material that uses them
    theVertexGroups.resize(theGroups.size());
    for (i = 0; i < theNewVertices.size() + this->mLegacyVertexCount; i++)
        for (k = 0; k < theGroups.size(); k++)
            for (j = 0; j < theGroups[k].mPolygons.size(); j++)
                if (i == theIndices[theGroups[k].mPolygons[j] * 3] ||
                    i == theIndices[theGroups[k].mPolygons[j] * 3 + 1] ||
                    i == theIndices[theGroups[k].mPolygons[j] * 3 + 2]) {
                    theVertexGroups[k].push_back(i);
                    break;
                }
    this->mVertexCount = 0;
    for (k = 0; k < theVertexGroups.size(); k++)
        this->mVertexCount += theVertexGroups[k].size();
    theVertexArrays = new GEVertexArray*[this->mFrameCount];
    for (theFrame = 0; theFrame < this->mFrameCount; theFrame++) {
        if (this->mSharedFrames.find(theFrame) != this->mSharedFrames.end())
            continue;
        theVertexArrays[theFrame] = theRasterizer->CreateVertexArray(0, this->mVertexCount);
        l = 0;
        for (k = 0; k < theVertexGroups.size(); k++)
            for (i = 0; i < theVertexGroups[k].size(); i++) {
                this->mVertexArrays[theFrame]->GetPoint(thePoint, theVertexGroups[k][i]);
                this->mVertexArrays[theFrame]->GetNormal(theNormal, theVertexGroups[k][i]);
                this->mVertexArrays[theFrame]->GetTextureCoordinate(theTextureCoordinate, theVertexGroups[k][i]);
                theVertexArrays[theFrame]->SetPoint(thePoint, l);
                theVertexArrays[theFrame]->SetNormal(theNormal, l);
                theVertexArrays[theFrame]->SetTextureCoordinate(theTextureCoordinate, l);
                l++;
            }
        delete this->mVertexArrays[theFrame];
        this->mVertexArrays[theFrame] = theVertexArrays[theFrame];
    }
    for (theFrame = 0; theFrame < this->mFrameCount; theFrame++) {
        std::map<unsigned long, long>::iterator theIterator = this->mSharedFrames.find(theFrame);
        if (theIterator != this->mSharedFrames.end())
            this->mVertexArrays[theFrame] = this->mVertexArrays[theIterator->second + theFrame];
    }
    for (i = 0; i < this->mPolygonCount; i++) {
        theIndices[i * 3] = this->mPolygonArrays[0]->GetVertexIndex1(i);
        theIndices[i * 3 + 1] = this->mPolygonArrays[0]->GetVertexIndex2(i);
        theIndices[i * 3 + 2] = this->mPolygonArrays[0]->GetVertexIndex3(i);
    }
    l = 0;
    for (i = 0; i < theVertexGroups.size(); i++) {
        for (k = 0; k < theVertexGroups[i].size(); k++)
            for (j = 0; j < theGroups[i].mPolygons.size(); j++) {
                if (theIndices[(l + j) * 3] == theVertexGroups[i][k])
                    this->mPolygonArrays[0]->SetVertexIndex1(l + j, k);
                else if (theIndices[(l + j) * 3 + 1] == theVertexGroups[i][k])
                    this->mPolygonArrays[0]->SetVertexIndex2(l + j, k);
                else if (theIndices[(l + j) * 3 + 2] == theVertexGroups[i][k])
                    this->mPolygonArrays[0]->SetVertexIndex3(l + j, k);
            }
        l += theGroups[i].mPolygons.size();
    }

    // replace the materials
    for (i = 0; i < this->mMaterialCount; i++)
        this->mMaterials[i]->Dereference();
    delete[] this->mMaterials;
    delete[] this->mTextureFilenames;
    // The legacy material index lists are sized by the old material count: remember it for the cleanup.
    unsigned long theLegacyMaterialCount = this->mMaterialCount;
    this->mMaterialCount = theGroups.size();
    this->mMaterials = new GEMaterial*[this->mMaterialCount];
    for (i = 0; i < this->mMaterialCount; i++)
        this->mMaterials[i] = new GEMaterial(theRasterizer->GetScreenID() + 1, 1);
    this->mTextureFilenames = new UString[this->mMaterialCount];
    for (i = 0; i < this->mMaterialCount; i++)
        this->mTextureFilenames[i] = theGroups[i].mTextureFilename;
    l = 0;
    k = 0;
    for (i = 0; i < theGroups.size(); i++) {
        this->mMaterials[i]->SetName(theGroups[i].mName);
        this->mMaterials[i]->SetTextureIndex(theGroups[i].mTextureIndex);
        this->mMaterials[i]->SetPolygonStartingIndex(l, 0);
        this->mMaterials[i]->SetPolygonRangeLength(theGroups[i].mPolygons.size(), 0);
        this->mMaterials[i]->SetVertexStartingIndex(k);
        this->mMaterials[i]->SetVertexRangeLength(theVertexGroups[i].size());
        l += theGroups[i].mPolygons.size();
        k += theVertexGroups[i].size();
    }
    delete[] theVertexArrays;
    delete[] theIndices;

    for (i = 0; i < this->mFrameCount; i++) {
        delete[] this->mLegacyPoints[i];
        delete[] this->mLegacyNormals[i];
    }
    for (i = 0; i < this->mLegacyPolygonGroupCount; i++)
        delete[] this->mLegacyPolygonGroups[i].mIndices;
    for (i = 0; i < theLegacyMaterialCount; i++)
        delete[] this->mLegacyMaterials[i].mIndices;
    delete[] this->mLegacyPolygons;
    delete[] this->mLegacyMaterials;
    delete[] this->mLegacyPolygonGroups;
    delete[] this->mLegacyVertexPoints;
    delete[] this->mLegacyPoints;
    delete[] this->mLegacyNormals;
    return kErrorOK;
}

// FUNCTION: LLE 0x1001a403
// Loads a version-1 model body chunk by chunk, converts it, then loads its sub-models; returns the first chunk error
long GEModel::LoadLegacyVersion(FSFileSpec& inFile, std::map<unsigned long, long>& ioTextures)
{
    long theResult;
    if ((theResult = this->LoadLegacyCountsChunk(inFile, ioTextures)) == kErrorOK &&
        (theResult = this->LoadLegacyMiscellaneousChunk(inFile)) == kErrorOK &&
        (theResult = this->LoadLegacyPolygonsChunk(inFile)) == kErrorOK &&
        (theResult = this->LoadLegacyPolygonGroupsChunk(inFile)) == kErrorOK &&
        (theResult = this->LoadLegacyMaterialsChunk(inFile)) == kErrorOK &&
        (theResult = this->LoadLegacyVerticesChunk(inFile)) == kErrorOK &&
        ((sMajorVersion == 1 && sMinorVersion == 0) || (theResult = this->LoadTagPointsChunk(inFile)) == kErrorOK) &&
        (theResult = this->LoadLegacyFrameChunk(inFile, ioTextures)) == kErrorOK) {
        theResult = this->ConvertLegacyModel();
        for (unsigned long i = 0; i < this->mSubModelCount; i++) {
            this->mSubModels[i] = new GEModel(this->mManager);
            this->mSubModels[i]->mParentModel = this;
            this->mSubModels[i]->Load(inFile, ioTextures, false);
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1001a520
// Reads the version-1 counts (zero frames, materials, points or polygons fail) and allocates the model's data
long GEModel::LoadLegacyCountsChunk(FSFileSpec& inFile, std::map<unsigned long, long>& ioTextures)
{
    long theResult;
    unsigned long theFrameCount;
    unsigned long theMaterialCount;
    unsigned long theSubModelCount = 0;
    unsigned long theTagPointCount = 0;
    unsigned long thePolygonCount;
    unsigned long theVertexCount;
    unsigned long thePointCount;
    unsigned long theGroupCount;
    if ((theResult = inFile.Read(theFrameCount)) != kErrorOK)
        return theResult;
    if (theFrameCount <= 0)
        return kErrorFailed;
    if ((theResult = inFile.Read(theMaterialCount)) != kErrorOK)
        return theResult;
    if (theMaterialCount <= 0)
        return kErrorFailed;
    if ((theResult = inFile.Read(thePointCount)) != kErrorOK)
        return theResult;
    if (thePointCount <= 0)
        return kErrorFailed;
    if ((theResult = inFile.Read(thePolygonCount)) != kErrorOK)
        return theResult;
    if (thePolygonCount <= 0)
        return kErrorFailed;
    if ((theResult = inFile.Read(theGroupCount)) != kErrorOK)
        return theResult;
    if ((theResult = inFile.Read(theVertexCount)) != kErrorOK)
        return theResult;
    if (theVertexCount > 0) {
        if (sMajorVersion != 1 || sMinorVersion != 0) {
            if (inFile.Read(theTagPointCount) == kErrorOK && (sMajorVersion != 1 || sMinorVersion >= 2))
                inFile.Read(theSubModelCount);
        }
        this->mFrameCount = theFrameCount;
        this->mMaterialCount = theMaterialCount;
        this->mTagPointCount = theTagPointCount;
        this->mSubModelCount = theSubModelCount;
        this->mPolygonCount = thePolygonCount;
        this->mLegacyPolygonCount = thePolygonCount;
        this->mVertexCount = theVertexCount;
        this->mLegacyPolygonGroupCount = theGroupCount;
        this->mLegacyVertexCount = theVertexCount;
        this->mLegacyPointCount = thePointCount;
        this->mLegacyPolygons = new LegacyPolygon[this->mLegacyPolygonCount];
        this->mLegacyVertexPoints = new unsigned long[this->mLegacyVertexCount];
        this->mLegacyNormals = new GEVector*[this->mFrameCount];
        this->mLegacyPoints = new GE3DPoint*[this->mFrameCount];
        this->mLegacyMaterials = new LegacyIndexList[this->mMaterialCount];
        this->mLegacyPolygonGroups = new LegacyIndexList[this->mLegacyPolygonGroupCount];
        for (unsigned long i = 0; i < this->mFrameCount; i++) {
            this->mLegacyNormals[i] = new GEVector[this->mLegacyVertexCount];
            this->mLegacyPoints[i] = new GE3DPoint[this->mLegacyPointCount];
        }
        return this->AllocateInternalData(0, ioTextures);
    }
    return kErrorFailed;
}

// FUNCTION: LLE 0x1001a7ef
// Reads each frame's radius, points, normals, tag points, transform and box as the minor version requires
long GEModel::LoadLegacyFrameChunk(FSFileSpec& inFile, std::map<unsigned long, long>& ioTextures)
{
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mFrameCount; i++) {
        unsigned long theFrame;
        float theRadius;
        std::map<unsigned long, long>::iterator theIterator = ioTextures.find(i);
        if (theIterator != ioTextures.end())
            theFrame = this->mFrameCount - 1;
        else
            theFrame = i;
        if ((theResult = inFile.Read(theRadius)) != kErrorOK)
            break;
        this->mBoundingRadii[i] = theRadius;
        if ((theResult = this->LoadLegacyVertexPointsSubChunk(inFile, theFrame)) != kErrorOK)
            break;
        if ((theResult = this->LoadLegacyVertexNormalsSubChunk(inFile, theFrame)) != kErrorOK)
            break;
        if (sMajorVersion == 1 && sMinorVersion < 3)
            theResult = this->LoadLegacyPolygonNormalsSubChunk(inFile, i);
        if (theResult != kErrorOK)
            break;
        if (sMajorVersion == 1 && sMinorVersion == 0)
            continue;
        if ((theResult = this->LoadTagPointsSubChunk(inFile, theFrame)) != kErrorOK)
            break;
        if (sMajorVersion == 1 && !(sMinorVersion >= 2))
            continue;
        if ((theResult = this->mInternalTransforms[i].Load(inFile)) != kErrorOK)
            break;
        if (sMajorVersion != 1 || sMinorVersion >= 3)
            theResult = this->mBoundingBoxes[i].Load(inFile);
    }
    return theResult;
}

// FUNCTION: LLE 0x1001a92b
// Reads each version-1 material's polygon indices and optional texture file name; returns the first read error
long GEModel::LoadLegacyMaterialsChunk(FSFileSpec& inFile)
{
    bool theHasTexture;
    UString theName(256);
    long theResult = kErrorOK;
    unsigned long theCount;
    for (unsigned long i = 0; i < this->mMaterialCount && theResult == kErrorOK; i++) {
        theResult = inFile.Read(theCount);
        if (theResult != kErrorOK)
            continue;
        if (theCount > 0) {
            this->mLegacyMaterials[i].mIndices = new unsigned long[theCount];
            this->mLegacyMaterials[i].mCount = theCount;
            for (unsigned long j = 0; j < theCount; j++) {
                unsigned long theIndex;
                if ((theResult = inFile.Read(theIndex)) != kErrorOK)
                    break;
                this->mLegacyMaterials[i].mIndices[j] = theIndex;
            }
        } else
            this->mLegacyMaterials[i].mCount = 0;
        if (theResult != kErrorOK)
            continue;
        theResult = inFile.Read(theHasTexture);
        if (theResult != kErrorOK || !theHasTexture)
            continue;
        theResult = this->mTextureFilenames[i].Load(inFile);
        if (theResult == kErrorOK) {
            unsigned long theTextureIndex;
            theResult = inFile.Read(theTextureIndex);
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1001aa47
// Reads the model name and centre, a flag, and (version 1.3 and later) one skipped value per point
long GEModel::LoadLegacyMiscellaneousChunk(FSFileSpec& inFile)
{
    long theResult;
    unsigned long theValue;
    bool theFlag;
    if ((theResult = this->mName.Load(inFile)) == kErrorOK && (theResult = this->mCenterPoint.Load(inFile)) == kErrorOK &&
        (theResult = inFile.Read(theFlag)) == kErrorOK) {
        if (sMajorVersion != 1 || sMinorVersion >= 3)
            for (unsigned long i = 0; i < this->mLegacyPointCount && theResult == kErrorOK; i++)
                theResult = inFile.Read(theValue);
    }
    return theResult;
}

// FUNCTION: LLE 0x1001aabf
// Reads the version-1 polygon groups, keeping the polygon indices only of the "player color" groups
long GEModel::LoadLegacyPolygonGroupsChunk(FSFileSpec& inFile)
{
    bool theIsPlayerColor;
    UString theName(32);
    unsigned long theCount;
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mLegacyPolygonGroupCount && theResult == kErrorOK; i++) {
        if ((theResult = theName.Load(inFile)) != kErrorOK)
            break;
        if ((theResult = inFile.Read(theCount)) != kErrorOK)
            break;
        if (theName == kPlayerColor) {
            theIsPlayerColor = true;
            this->mLegacyPolygonGroups[i].mIndices = new unsigned long[theCount];
            this->mLegacyPolygonGroups[i].mCount = theCount;
        } else {
            theIsPlayerColor = false;
            this->mLegacyPolygonGroups[i].mIndices = NULL;
            this->mLegacyPolygonGroups[i].mCount = 0;
        }
        for (unsigned long j = 0; j < theCount; j++) {
            unsigned long theIndex;
            if ((theResult = inFile.Read(theIndex)) != kErrorOK)
                break;
            if (theIsPlayerColor)
                this->mLegacyPolygonGroups[i].mIndices[j] = theIndex;
        }
        if (theResult != kErrorOK)
            break;
    }
    return theResult;
}

// FUNCTION: LLE 0x1001abba
// Reads and discards one normal per polygon for a frame (versions before 1.3)
long GEModel::LoadLegacyPolygonNormalsSubChunk(FSFileSpec& inFile, unsigned long inFrame)
{
    GEVector theNormal;
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mPolygonCount; i++)
        if ((theResult = theNormal.Load(inFile)) != kErrorOK)
            break;
    return theResult;
}

// FUNCTION: LLE 0x1001abfd
// Reads each version-1 polygon's three corners (vertex index, texture coordinate; colour and lighting skipped)
long GEModel::LoadLegacyPolygonsChunk(FSFileSpec& inFile)
{
    GEColor theColor;
    float theAlpha;
    float theAmbient;
    float theDiffuse;
    float theSpecular;
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mPolygonCount && theResult == kErrorOK; i++) {
        for (unsigned long j = 0; j < 3; j++) {
            if ((theResult = inFile.Read(this->mLegacyPolygons[i].mVertexIndices[j])) != kErrorOK)
                break;
            if ((theResult = this->mLegacyPolygons[i].mTextureCoordinates[j].Load(inFile)) != kErrorOK)
                break;
            theColor.Load(inFile);
            if ((theResult = inFile.Read(theAlpha)) != kErrorOK)
                break;
            if ((theResult = inFile.Read(theAmbient)) != kErrorOK)
                break;
            if ((theResult = inFile.Read(theDiffuse)) != kErrorOK)
                break;
            if ((theResult = inFile.Read(theSpecular)) != kErrorOK)
                break;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1001acd7
// Reads a frame's vertex normals, plain before version 1.3 and as quantized indices after
long GEModel::LoadLegacyVertexNormalsSubChunk(FSFileSpec& inFile, unsigned long inFrame)
{
    GEVector theNormal;
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mVertexCount; i++) {
        if (sMajorVersion == 1 && sMinorVersion < 3) {
            if ((theResult = theNormal.Load(inFile)) != kErrorOK)
                break;
            this->mLegacyNormals[inFrame][i] = theNormal;
        } else {
            GEVector* theQuantizedNormal;
            if ((theResult = GEVector::LoadQuantizedIndex(inFile, theQuantizedNormal)) != kErrorOK)
                break;
            this->mLegacyNormals[inFrame][i] = *theQuantizedNormal;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1001ad9b
// Reads a frame's version-1 points; returns the first read error
long GEModel::LoadLegacyVertexPointsSubChunk(FSFileSpec& inFile, unsigned long inFrame)
{
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mLegacyPointCount; i++)
        if ((theResult = this->mLegacyPoints[inFrame][i].Load(inFile)) != kErrorOK)
            break;
    return theResult;
}

// FUNCTION: LLE 0x1001adea
// Reads each version-1 vertex's point index, skipping one float per vertex
long GEModel::LoadLegacyVerticesChunk(FSFileSpec& inFile)
{
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < this->mVertexCount; i++) {
        float theUnused;
        if ((theResult = inFile.Read(this->mLegacyVertexPoints[i])) != kErrorOK)
            break;
        if ((theResult = inFile.Read(theUnused)) != kErrorOK)
            break;
    }
    return theResult;
}

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)
// SYNTHETIC: LLE 0x1001c252
// SYNTHETIC: LLE 0x1001c270
// SYNTHETIC: LLE 0x1001c27c
// SYNTHETIC: LLE 0x1001c288

// COMDATs of this unit that ICF keeps here (named in analysis/lle/names.txt; several fold with other units' twins)
// TEMPLATE: LLE 0x1001a346
// TEMPLATE: LLE 0x1001a3a1
// TEMPLATE: LLE 0x1001ae4b
// TEMPLATE: LLE 0x1001ae77
// TEMPLATE: LLE 0x1001af0a
// TEMPLATE: LLE 0x1001b0d5
// TEMPLATE: LLE 0x1001b1be
// TEMPLATE: LLE 0x1001b271
// TEMPLATE: LLE 0x1001b55c
// TEMPLATE: LLE 0x1001b811
// TEMPLATE: LLE 0x1001baea
// TEMPLATE: LLE 0x1001bd9c
// TEMPLATE: LLE 0x1001bdcf
// TEMPLATE: LLE 0x1001be71
// TEMPLATE: LLE 0x1001beb3
// TEMPLATE: LLE 0x1001bec7
// TEMPLATE: LLE 0x1001bfb3
// TEMPLATE: LLE 0x1001c050
// TEMPLATE: LLE 0x1001c0a3
// TEMPLATE: LLE 0x1001c0cc
// TEMPLATE: LLE 0x1001c0ff
// TEMPLATE: LLE 0x1001c11b
// TEMPLATE: LLE 0x1001c23f
