// C:\EEX\Low-Level Engine\Graphics Engine\GEModel Caching.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10016cb8..0x100189df: GEModel's memory-coherency / vertex-cache optimisation (ComputeInitialTriOrdering ..
// OptimizeVertexCaching). (File name inferred: sorts before GEModel.cpp; the parts are 'GEModel <part>.cpp'.)
#include <windows.h>

#include <map>
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
#include "lle/URandom.h"
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
// SYNTHETIC: LLE 0x100180b7
// TEMPLATE: LLE 0x1001876c
// TEMPLATE: LLE 0x100187a5
// TEMPLATE: LLE 0x100187b9

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x10016cb8
// SYNTHETIC: LLE 0x10016cc2
// SYNTHETIC: LLE 0x10016cd2
// SYNTHETIC: LLE 0x10016cde
// SYNTHETIC: LLE 0x10016ce8
// SYNTHETIC: LLE 0x10016cf2
// SYNTHETIC: LLE 0x10016d02
// SYNTHETIC: LLE 0x10016d0e
// SYNTHETIC: LLE 0x10016d18
// SYNTHETIC: LLE 0x10016d22
// SYNTHETIC: LLE 0x10016d32
// SYNTHETIC: LLE 0x10016d3e
// SYNTHETIC: LLE 0x10016d48
// SYNTHETIC: LLE 0x10016d52
// SYNTHETIC: LLE 0x10016d62
// SYNTHETIC: LLE 0x10016d6e
// SYNTHETIC: LLE 0x10016d78
// SYNTHETIC: LLE 0x10016d82
// SYNTHETIC: LLE 0x10016d92
// SYNTHETIC: LLE 0x10016d9e
// SYNTHETIC: LLE 0x10016da8
// SYNTHETIC: LLE 0x10016db2
// SYNTHETIC: LLE 0x10016dc2
// SYNTHETIC: LLE 0x10016dce

// The genomes of the vertex-cache and memory-coherency optimisers: a polygon order and a vertex order
struct GEModel::TriListGenome {
    struct Triangle {
        unsigned long mVertices[3];
    };
    Triangle* mTriangles;                   // +0x00
    unsigned long mCount;                   // +0x04
};

struct GEModel::VertexListGenome {
    long* mIndices;                         // +0x00 the new index of each vertex
    unsigned long mCount;                   // +0x04
};

// A vertex while OptimizeMemoryCoherency reorders a material's vertices (ctor 0x100180b7)
struct CachingVertex {
    GE3DPoint mPoint;
    GEVector mNormal;
    GETextureCoordinate mTextureCoordinate;
};

// The size of the simulated vertex cache
const unsigned long kCacheSize = 6;

// FUNCTION: LLE 0x10016dd8
// Reorders the triangles greedily: each step takes the one with the fewest misses in a simulated 6-entry vertex cache
void GEModel::ComputeInitialTriOrdering(TriListGenome& ioGenome)
{
    std::multimap<unsigned long, unsigned long> theScores;
    std::vector<TriListGenome::Triangle> theTriangles;
    long theCache[kCacheSize];
    unsigned long theNext = 0;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    unsigned long l;
    for (i = 0; i < kCacheSize; i++)
        theCache[i] = -1;
    theTriangles.resize(ioGenome.mCount);
    for (i = 0; i < ioGenome.mCount; i++)
        theTriangles[i] = ioGenome.mTriangles[i];
    for (i = 0; i < ioGenome.mCount; i++) {
        theScores.clear();
        for (j = 0; j < theTriangles.size(); j++) {
            unsigned long theMisses = 3;
            unsigned long theNeighbours = 0;
            for (k = 0; k < kCacheSize; k++)
                if (theTriangles[j].mVertices[0] == theCache[k] || theTriangles[j].mVertices[1] == theCache[k] ||
                    theTriangles[j].mVertices[2] == theCache[k])
                    theMisses--;
            for (k = 0; k < theMisses; k++) {
                unsigned long theSlot = (theNext + k) % kCacheSize;
                for (l = 0; l < theTriangles.size(); l++)
                    if (theTriangles[l].mVertices[0] == theCache[theSlot] ||
                        theTriangles[l].mVertices[1] == theCache[theSlot] ||
                        theTriangles[l].mVertices[2] == theCache[theSlot])
                        theNeighbours++;
            }
            theScores.insert(std::multimap<unsigned long, unsigned long>::value_type(theMisses + theNeighbours, j));
        }
        ioGenome.mTriangles[i] = theTriangles[theScores.begin()->second];
        theTriangles.erase(theTriangles.begin() + theScores.begin()->second);
        bool theFound[3];
        theFound[0] = false;
        theFound[1] = false;
        theFound[2] = false;
        for (k = 0; k < kCacheSize; k++) {
            if (ioGenome.mTriangles[i].mVertices[0] == theCache[k])
                theFound[0] = true;
            else if (ioGenome.mTriangles[i].mVertices[1] == theCache[k])
                theFound[1] = true;
            else if (ioGenome.mTriangles[i].mVertices[2] == theCache[k])
                theFound[2] = true;
        }
        if (!theFound[0]) {
            theCache[theNext] = ioGenome.mTriangles[i].mVertices[0];
            theNext = (theNext + 1) % kCacheSize;
        }
        if (!theFound[1]) {
            theCache[theNext] = ioGenome.mTriangles[i].mVertices[1];
            theNext = (theNext + 1) % kCacheSize;
        }
        if (!theFound[2]) {
            theCache[theNext] = ioGenome.mTriangles[i].mVertices[2];
            theNext = (theNext + 1) % kCacheSize;
        }
    }
}

// FUNCTION: LLE 0x1001711c
// Numbers a material's vertices in the order the LOD 0 triangles first use them (unused ones stay -1)
void GEModel::ComputeInitialVertexOrdering(VertexListGenome& ioGenome, unsigned long inMaterial)
{
    long theNext = 0;
    unsigned long theStart = this->mMaterials[inMaterial]->GetPolygonStartingIndex(0);
    unsigned long theEnd = theStart + this->mMaterials[inMaterial]->GetPolygonRangeLength(0);
    unsigned long i;
    for (i = 0; i < ioGenome.mCount; i++)
        ioGenome.mIndices[i] = -1;
    for (i = theStart; i < theEnd; i++) {
        if (ioGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex1(i)] == -1)
            ioGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex1(i)] = theNext++;
        if (ioGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex2(i)] == -1)
            ioGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex2(i)] = theNext++;
        if (ioGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex3(i)] == -1)
            ioGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex3(i)] = theNext++;
    }
}

// FUNCTION: LLE 0x1001720e
// Returns how far apart (summed index distance) a vertex order puts consecutive vertex uses of a material's triangles
unsigned long GEModel::ComputeMemoryCoherency(const VertexListGenome& inGenome, unsigned long inMaterial)
{
    unsigned long theStart = this->mMaterials[inMaterial]->GetPolygonStartingIndex(0);
    unsigned long theEnd = theStart + this->mMaterials[inMaterial]->GetPolygonRangeLength(0);
    long theDifference;
    unsigned long theSpread;
    theDifference = inGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex1(theStart)] -
                    inGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex2(theStart)];
    theSpread = theDifference < 0 ? -theDifference : theDifference;
    theDifference = inGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex2(theStart)] -
                    inGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex3(theStart)];
    theSpread += theDifference < 0 ? -theDifference : theDifference;
    for (unsigned long i = theStart + 1; i < theEnd; i++) {
        theDifference = inGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex3(i - 1)] -
                        inGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex1(i)];
        theSpread += theDifference < 0 ? -theDifference : theDifference;
        theDifference = inGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex1(i)] -
                        inGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex2(i)];
        theSpread += theDifference < 0 ? -theDifference : theDifference;
        theDifference = inGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex2(i)] -
                        inGenome.mIndices[this->mPolygonArrays[0]->GetVertexIndex3(i)];
        theSpread += theDifference < 0 ? -theDifference : theDifference;
    }
    return theSpread;
}

// FUNCTION: LLE 0x100173ba
// Returns a material's memory locality: three references per triangle over their summed index distance (LOD 0)
float GEModel::ComputeMemoryLocalityEfficiency(unsigned long inMaterial)
{
    unsigned long theStart = this->mMaterials[inMaterial]->GetPolygonStartingIndex(0);
    unsigned long theEnd = theStart + this->mMaterials[inMaterial]->GetPolygonRangeLength(0);
    long theDifference;
    unsigned long theSpread;
    theDifference = this->mPolygonArrays[0]->GetVertexIndex1(theStart) - this->mPolygonArrays[0]->GetVertexIndex2(theStart);
    theSpread = theDifference < 0 ? -theDifference : theDifference;
    theDifference = this->mPolygonArrays[0]->GetVertexIndex2(theStart) - this->mPolygonArrays[0]->GetVertexIndex3(theStart);
    theSpread += theDifference < 0 ? -theDifference : theDifference;
    for (unsigned long i = theStart + 1; i < theEnd; i++) {
        theDifference = this->mPolygonArrays[0]->GetVertexIndex3(i - 1) - this->mPolygonArrays[0]->GetVertexIndex1(i);
        theSpread += theDifference < 0 ? -theDifference : theDifference;
        theDifference = this->mPolygonArrays[0]->GetVertexIndex1(i) - this->mPolygonArrays[0]->GetVertexIndex2(i);
        theSpread += theDifference < 0 ? -theDifference : theDifference;
        theDifference = this->mPolygonArrays[0]->GetVertexIndex2(i) - this->mPolygonArrays[0]->GetVertexIndex3(i);
        theSpread += theDifference < 0 ? -theDifference : theDifference;
    }
    float theEfficiency = (float)(this->mMaterials[inMaterial]->GetPolygonRangeLength(0) * 3) / (float)theSpread;
    return theEfficiency;
}

// FUNCTION: LLE 0x10017523
// Runs the vertex-cache and then the memory-coherency optimiser inPasses times, then does the same for every submodel
void GEModel::ComputeOptimalDataOrdering(long inIterations, long inPasses)
{
    long i;
    for (i = 0; i < inPasses; i++)
        this->OptimizeVertexCaching(inIterations);
    for (i = 0; i < inPasses; i++)
        this->OptimizeMemoryCoherency(inIterations);
    for (i = 0; i < (long)this->mSubModelCount; i++)
        this->mSubModels[i]->ComputeOptimalDataOrdering(inIterations, inPasses);
}

// FUNCTION: LLE 0x10017580
// Measures a material's LOD 0 cache: unique vertices per miss, and the share of the possible saving achieved
void GEModel::ComputeVertexCacheEfficiency(unsigned long inMaterial, float& outEfficiency, float& outOptimality)
{
    unsigned long theCount = this->mMaterials[inMaterial]->GetPolygonRangeLength(0);
    unsigned long theStart = this->mMaterials[inMaterial]->GetPolygonStartingIndex(0);
    long theMaximumSaving = theCount * 3 - this->mMaterials[inMaterial]->GetVertexRangeLength();
    TriListGenome theGenome;
    theGenome.mTriangles = new TriListGenome::Triangle[theCount];
    theGenome.mCount = theCount;
    for (unsigned long i = 0; i < theCount; i++) {
        theGenome.mTriangles[i].mVertices[0] = this->mPolygonArrays[0]->GetVertexIndex1(theStart + i);
        theGenome.mTriangles[i].mVertices[1] = this->mPolygonArrays[0]->GetVertexIndex2(theStart + i);
        theGenome.mTriangles[i].mVertices[2] = this->mPolygonArrays[0]->GetVertexIndex3(theStart + i);
    }
    unsigned long theMisses = this->ComputeVertexCacheMisses(theGenome);
    outEfficiency = (float)this->mMaterials[inMaterial]->GetVertexRangeLength() / (float)theMisses;
    if (theMaximumSaving > 0)
        outOptimality = (float)(theCount * 3 - theMisses) / (float)theMaximumSaving;
    else
        outOptimality = 0.0f;
    delete[] theGenome.mTriangles;
}

// FUNCTION: LLE 0x10017686
// Counts the misses of a 6-entry FIFO vertex cache over a triangle order
unsigned long GEModel::ComputeVertexCacheMisses(const TriListGenome& inGenome)
{
    long theCache[kCacheSize];
    unsigned long i;
    for (i = 0; i < kCacheSize; i++)
        theCache[i] = -1;
    unsigned long theMisses = 0;
    unsigned long theNext = 0;
    for (i = 0; i < inGenome.mCount; i++) {
        bool theFound[3];
        unsigned long j;
        theFound[0] = false;
        theFound[1] = false;
        theFound[2] = false;
        for (j = 0; j < kCacheSize; j++) {
            if (inGenome.mTriangles[i].mVertices[0] == theCache[j])
                theFound[0] = true;
            else if (inGenome.mTriangles[i].mVertices[1] == theCache[j])
                theFound[1] = true;
            else if (inGenome.mTriangles[i].mVertices[2] == theCache[j])
                theFound[2] = true;
        }
        for (j = 0; j < 3; j++)
            if (!theFound[j]) {
                theMisses++;
                theCache[theNext] = inGenome.mTriangles[i].mVertices[j];
                theNext = (theNext + 1) % kCacheSize;
            }
    }
    return theMisses;
}

// FUNCTION: LLE 0x10017735
// Applies inMutations random swaps of triangles within inDistance and rotates the vertices of a random triangle
void GEModel::MutateTriGenes(TriListGenome& ioGenome, URandomIntegers& inRandom, unsigned long inDistance,
                             unsigned long inMutations)
{
    long theLast = ioGenome.mCount - 1;
    for (unsigned long i = 0; i < inMutations; i++) {
        long theFirst = inRandom.Generate(0, theLast);
        long theMaximum = min(theLast, (long)(theFirst + inDistance));
        long theMinimum = max(0, (long)(theFirst - inDistance));
        long theSecond = inRandom.Generate(theMinimum, theMaximum);
        TriListGenome::Triangle theTriangle = ioGenome.mTriangles[theFirst];
        ioGenome.mTriangles[theFirst] = ioGenome.mTriangles[theSecond];
        ioGenome.mTriangles[theSecond] = theTriangle;
        long theRotated = inRandom.Generate(0, theLast);
        unsigned long theVertex = ioGenome.mTriangles[theRotated].mVertices[0];
        ioGenome.mTriangles[theRotated].mVertices[0] = ioGenome.mTriangles[theRotated].mVertices[1];
        ioGenome.mTriangles[theRotated].mVertices[1] = ioGenome.mTriangles[theRotated].mVertices[2];
        ioGenome.mTriangles[theRotated].mVertices[2] = theVertex;
    }
}

// FUNCTION: LLE 0x1001784f
// Applies inMutations random swaps of vertex numbers within inDistance of each other
void GEModel::MutateVertexGenes(VertexListGenome& ioGenome, URandomIntegers& inRandom, unsigned long inDistance,
                                unsigned long inMutations)
{
    long theLast = ioGenome.mCount - 1;
    for (unsigned long i = 0; i < inMutations; i++) {
        long theFirst = inRandom.Generate(0, theLast);
        long theMaximum = min(theLast, (long)(theFirst + inDistance));
        long theMinimum = max(0, (long)(theFirst - inDistance));
        long theSecond = inRandom.Generate(theMinimum, theMaximum);
        long theIndex = ioGenome.mIndices[theFirst];
        ioGenome.mIndices[theFirst] = ioGenome.mIndices[theSecond];
        ioGenome.mIndices[theSecond] = theIndex;
    }
}

// FUNCTION: LLE 0x10017912
// Reorders every material's vertices by a genetic search for the order with the least index spread, remapping triangles
void GEModel::OptimizeMemoryCoherency(long inGenerations)
{
    URandomIntegers theSelector;
    URandomIntegers theMutator;
    std::multimap<unsigned long, unsigned long> theRanking;
    std::multimap<unsigned long, unsigned long>::iterator theIterator;
    theSelector.mSeed = timeGetTime();
    theMutator.mSeed = timeGetTime();
    for (unsigned long theMaterial = 0; theMaterial < this->mMaterialCount; theMaterial++) {
        long theGeneration = inGenerations;
        unsigned long theCount = this->mMaterials[theMaterial]->GetVertexRangeLength();
        // A material with fewer than two vertices has nothing to reorder (the genome setup uses entries 0 and 1).
        if (theCount <= 1)
            continue;
        float thePopulationSize = (float)theCount;
        unsigned long theEliteCount = UMFloatToLong(thePopulationSize * 0.1f);
        long theMutations = UMFloatToLong(thePopulationSize);
        float theLast = (float)(theCount - 1);
        VertexListGenome* theGenomes = new VertexListGenome[theCount];
        VertexListGenome* theNextGenomes = new VertexListGenome[theCount];
        unsigned long i;
        for (i = 0; i < theCount; i++) {
            theGenomes[i].mIndices = new long[theCount];
            theNextGenomes[i].mIndices = new long[theCount];
            theGenomes[i].mCount = theCount;
            theNextGenomes[i].mCount = theCount;
        }
        unsigned long* theOrder = new unsigned long[theCount];
        for (i = 0; i < theCount; i++)
            theGenomes[0].mIndices[i] = i;
        unsigned long theBestSpread = this->ComputeMemoryCoherency(theGenomes[0], theMaterial);
        unsigned long theBest = 0;
        ESDebugOut("Material %d, Initial Data Spread = %d\n", theMaterial, theBestSpread);
        memcpy(theGenomes[1].mIndices, theGenomes[0].mIndices, theCount * sizeof(long));
        this->ComputeInitialVertexOrdering(theGenomes[1], theMaterial);
        unsigned long theSpread = this->ComputeMemoryCoherency(theGenomes[1], theMaterial);
        if (theSpread < theBestSpread) {
            theBestSpread = theSpread;
            theBest = 1;
        }
        ESDebugOut("Material %d, 1stPass Data Spread = %d\n", theMaterial, theBestSpread);
        for (i = 2; i < theCount; i++)
            memcpy(theGenomes[i].mIndices, theGenomes[theBest].mIndices, theCount * sizeof(long));
        for (i = 0; i < theCount; i++)
            memcpy(theNextGenomes[i].mIndices, theGenomes[theBest].mIndices, theCount * sizeof(long));
        for (i = 2; i < theCount; i++) {
            float theScale = (float)i;
            this->MutateVertexGenes(theGenomes[i], theMutator, UMFloatToLong(theScale * 0.5f) + 1,
                              UMFloatToLong(theScale * 0.01f) + 1);
        }
        VertexListGenome* theCurrent = theGenomes;
        VertexListGenome* theNext = theNextGenomes;
        for (; theGeneration >= 0; theGeneration--) {
            theRanking.clear();
            for (i = 0; i < theCount; i++)
                theRanking.insert(std::multimap<unsigned long, unsigned long>::value_type(
                    this->ComputeMemoryCoherency(theCurrent[i], theMaterial), i));
            theIterator = theRanking.begin();
            if (theIterator->first < theBestSpread) {
                theBestSpread = theIterator->first;
                ESDebugOut("Material %d, Minimum Data Spread = %d\n", theMaterial, theBestSpread);
            }
            if (theGeneration % 1000 == 0)
                ESDebugOut("%d iterations left\n", theGeneration);
            for (i = 0; i < theCount; i++, theIterator++)
                theOrder[i] = theIterator->second;
            for (i = 0; i < theEliteCount; i++)
                memcpy(theNext[i].mIndices, theCurrent[theOrder[i]].mIndices, theCount * sizeof(long));
            for (i = theEliteCount; i < theEliteCount * 2; i++)
                this->MutateVertexGenes(theNext[i], theMutator, theCount - 1, theMutations);
            for (i = theEliteCount * 2; i < theCount; i++) {
                float theSelection = UMSquare(theSelector.URandom::Generate()) * theLast;
                long theParent = UMFloatToLong(theSelection);
                long theMutationCount = UMFloatToLong(theSelection * 0.01f);
                long theDistance = UMFloatToLong(theSelection * 0.5f);
                memcpy(theNext[i].mIndices, theCurrent[theOrder[theParent]].mIndices, theCount * sizeof(long));
                this->MutateVertexGenes(theNext[i], theMutator, theDistance + 1, theMutationCount + 1);
            }
            VertexListGenome* theSwap = theCurrent;
            theCurrent = theNext;
            theNext = theSwap;
        }

        // reorder the material's vertices by the best genome
        VertexListGenome& theBestGenome = theNext[theRanking.begin()->second];
        unsigned long theVertexStart = this->mMaterials[theMaterial]->GetVertexStartingIndex();
        CachingVertex* theVertices = new CachingVertex[theCount];
        unsigned long theFrame;
        for (theFrame = 0; theFrame < this->mFrameCount; theFrame++) {
            for (i = 0; i < theCount; i++) {
                this->mVertexArrays[theFrame]->GetPoint(theVertices[i].mPoint, theVertexStart + i);
                this->mVertexArrays[theFrame]->GetNormal(theVertices[i].mNormal, theVertexStart + i);
                this->mVertexArrays[theFrame]->GetTextureCoordinate(theVertices[i].mTextureCoordinate, theVertexStart + i);
            }
            for (i = 0; i < theCount; i++) {
                this->mVertexArrays[theFrame]->SetPoint(theVertices[i].mPoint, theBestGenome.mIndices[i] + theVertexStart);
                this->mVertexArrays[theFrame]->SetNormal(theVertices[i].mNormal, theBestGenome.mIndices[i] + theVertexStart);
                this->mVertexArrays[theFrame]->SetTextureCoordinate(theVertices[i].mTextureCoordinate,
                                                              theBestGenome.mIndices[i] + theVertexStart);
            }
        }
        for (unsigned long theLOD = 0; theLOD < this->mLODCount; theLOD++) {
            unsigned long theStart = this->mMaterials[theMaterial]->GetPolygonStartingIndex(theLOD);
            unsigned long theEnd = theStart + this->mMaterials[theMaterial]->GetPolygonRangeLength(theLOD);
            for (i = theStart; i < theEnd; i++) {
                this->mPolygonArrays[theLOD]->SetVertexIndex1(i,
                    theBestGenome.mIndices[this->mPolygonArrays[theLOD]->GetVertexIndex1(i)]);
                this->mPolygonArrays[theLOD]->SetVertexIndex2(i,
                    theBestGenome.mIndices[this->mPolygonArrays[theLOD]->GetVertexIndex2(i)]);
                this->mPolygonArrays[theLOD]->SetVertexIndex3(i,
                    theBestGenome.mIndices[this->mPolygonArrays[theLOD]->GetVertexIndex3(i)]);
            }
        }
        delete[] theOrder;
        delete[] theVertices;
        for (i = 0; i < theCount; i++) {
            delete[] theGenomes[i].mIndices;
            delete[] theNextGenomes[i].mIndices;
        }
        delete[] theGenomes;
        delete[] theNextGenomes;
        ESDebugOut("\n");
    }
}

// FUNCTION: LLE 0x100180e1
// Reorders every material's triangles at every detail level by a genetic search for the fewest vertex-cache misses
void GEModel::OptimizeVertexCaching(long inGenerations)
{
    URandomIntegers theSelector;
    URandomIntegers theMutator;
    std::multimap<unsigned long, unsigned long> theRanking;
    std::multimap<unsigned long, unsigned long>::iterator theIterator;
    theSelector.mSeed = timeGetTime();
    theMutator.mSeed = timeGetTime();
    for (unsigned long theLOD = 0; theLOD < this->mLODCount; theLOD++) {
        for (unsigned long theMaterial = 0; theMaterial < this->mMaterialCount; theMaterial++) {
            if (!(this->mMaterials[theMaterial]->GetPolygonRangeLength(theLOD) > 1))
                continue;
            unsigned long theCount = this->mMaterials[theMaterial]->GetPolygonRangeLength(theLOD);
            unsigned long theSize = theCount * sizeof(TriListGenome::Triangle);
            long theGeneration = inGenerations;
            float thePopulationSize = (float)theCount;
            unsigned long theEliteCount = UMFloatToLong(thePopulationSize * 0.1f);
            long theMutations = UMFloatToLong(thePopulationSize);
            float theLast = (float)(theCount - 1);
            TriListGenome* theGenomes = new TriListGenome[theCount];
            TriListGenome* theNextGenomes = new TriListGenome[theCount];
            unsigned long i;
            for (i = 0; i < theCount; i++) {
                theGenomes[i].mTriangles = new TriListGenome::Triangle[theCount];
                theNextGenomes[i].mTriangles = new TriListGenome::Triangle[theCount];
                theGenomes[i].mCount = theCount;
                theNextGenomes[i].mCount = theCount;
            }
            unsigned long* theOrder = new unsigned long[theCount];
            unsigned long theStart = this->mMaterials[theMaterial]->GetPolygonStartingIndex(theLOD);
            for (i = 0; i < theCount; i++) {
                theGenomes[0].mTriangles[i].mVertices[0] = this->mPolygonArrays[theLOD]->GetVertexIndex1(theStart + i);
                theGenomes[0].mTriangles[i].mVertices[1] = this->mPolygonArrays[theLOD]->GetVertexIndex2(theStart + i);
                theGenomes[0].mTriangles[i].mVertices[2] = this->mPolygonArrays[theLOD]->GetVertexIndex3(theStart + i);
            }
            unsigned long theBestMisses = this->ComputeVertexCacheMisses(theGenomes[0]);
            unsigned long theBest = 0;
            ESDebugOut("Material %d, LOD %d, Initial Cache Misses = %d\n", theMaterial, theLOD, theBestMisses);
            memcpy(theGenomes[1].mTriangles, theGenomes[0].mTriangles, theSize);
            this->ComputeInitialTriOrdering(theGenomes[1]);
            unsigned long theMisses = this->ComputeVertexCacheMisses(theGenomes[1]);
            if (theMisses < theBestMisses) {
                theBestMisses = theMisses;
                theBest = 1;
            }
            ESDebugOut("Material %d, LOD %d, 1stPass Cache Misses = %d\n", theMaterial, theLOD, theBestMisses);
            for (i = 2; i < theCount; i++)
                memcpy(theGenomes[i].mTriangles, theGenomes[theBest].mTriangles, theSize);
            for (i = 0; i < theCount; i++)
                memcpy(theNextGenomes[i].mTriangles, theGenomes[theBest].mTriangles, theSize);
            for (i = 2; i < theCount; i++) {
                float theScale = (float)i;
                this->MutateTriGenes(theGenomes[i], theMutator, UMFloatToLong(theScale * 2.0f) + 1,
                               UMFloatToLong(theScale * 0.025f) + 1);
            }
            TriListGenome* theCurrent = theGenomes;
            TriListGenome* theNext = theNextGenomes;
            for (; theGeneration >= 0; theGeneration--) {
                theRanking.clear();
                for (i = 0; i < theCount; i++)
                    theRanking.insert(std::multimap<unsigned long, unsigned long>::value_type(
                        this->ComputeVertexCacheMisses(theCurrent[i]), i));
                theIterator = theRanking.begin();
                if (theIterator->first < theBestMisses) {
                    theBestMisses = theIterator->first;
                    ESDebugOut("Material %d, LOD %d, Minimum Cache Misses = %d\n", theMaterial, theLOD,
                               theBestMisses);
                }
                if (theGeneration % 1000 == 0)
                    ESDebugOut("%d iterations left\n", theGeneration);
                for (i = 0; i < theCount; i++, theIterator++)
                    theOrder[i] = theIterator->second;
                for (i = 0; i < theEliteCount; i++)
                    memcpy(theNext[i].mTriangles, theCurrent[theOrder[i]].mTriangles, theSize);
                for (i = theEliteCount; i < theEliteCount * 2; i++)
                    this->MutateTriGenes(theNext[i], theMutator, theCount - 1, theMutations);
                for (i = theEliteCount * 2; i < theCount; i++) {
                    float theSelection = UMSquare(theSelector.URandom::Generate()) * theLast;
                    long theParent = UMFloatToLong(theSelection);
                    long theMutationCount = UMFloatToLong(theSelection * 0.025f);
                    long theDistance = UMFloatToLong(theSelection * 2.0f);
                    memcpy(theNext[i].mTriangles, theCurrent[theOrder[theParent]].mTriangles, theSize);
                    this->MutateTriGenes(theNext[i], theMutator, theDistance + 1, theMutationCount + 1);
                }
                TriListGenome* theSwap = theCurrent;
                theCurrent = theNext;
                theNext = theSwap;
            }

            // store the best order
            TriListGenome& theBestGenome = theNext[theRanking.begin()->second];
            for (i = 0; i < theCount; i++) {
                this->mPolygonArrays[theLOD]->SetVertexIndex1(theStart + i, theBestGenome.mTriangles[i].mVertices[0]);
                this->mPolygonArrays[theLOD]->SetVertexIndex2(theStart + i, theBestGenome.mTriangles[i].mVertices[1]);
                this->mPolygonArrays[theLOD]->SetVertexIndex3(theStart + i, theBestGenome.mTriangles[i].mVertices[2]);
            }
            delete[] theOrder;
            for (i = 0; i < theCount; i++) {
                delete[] theGenomes[i].mTriangles;
                delete[] theNextGenomes[i].mTriangles;
            }
            delete[] theGenomes;
            delete[] theNextGenomes;
            ESDebugOut("\n");
        }
    }
}

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)
// SYNTHETIC: LLE 0x1001899d
// SYNTHETIC: LLE 0x100189bb
// SYNTHETIC: LLE 0x100189c7
// SYNTHETIC: LLE 0x100189d3

// COMDATs of this unit kept by ICF or bound by unit_check: CachingVertex ??_E (folded with GE3DPoint/GEVector), the
// multimap<unsigned long, unsigned long> members, vector<Triangle>::resize, copy<Triangle>
// TEMPLATE: LLE 0x10018085
// TEMPLATE: LLE 0x10018797
// TEMPLATE: LLE 0x100187bf
// TEMPLATE: LLE 0x1001881e
// TEMPLATE: LLE 0x1001888f
// TEMPLATE: LLE 0x10018943
// TEMPLATE: LLE 0x10018975
