// C:\EEX\Low-Level Engine\Graphics Engine\GEModel LOD.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1001cd3b..0x1001e2fc: GEModel::ComputeLODReduction and the GELOD* edge-collapse helper classes (include/lle/GELOD.h),
// then the list COMDATs (UList<GELOD*>::AddHead, the node pool's New, vector<GELODVertex*>::resize) and the unit's
// UList pool registrations (0x1001e2ba..0x1001e2fc). (File name inferred.)
#include <windows.h>
#include <float.h>

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

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x1001cd3b
// SYNTHETIC: LLE 0x1001cd45
// SYNTHETIC: LLE 0x1001cd55
// SYNTHETIC: LLE 0x1001cd61
// SYNTHETIC: LLE 0x1001cd6b
// SYNTHETIC: LLE 0x1001cd75
// SYNTHETIC: LLE 0x1001cd85
// SYNTHETIC: LLE 0x1001cd91
// SYNTHETIC: LLE 0x1001cd9b
// SYNTHETIC: LLE 0x1001cda5
// SYNTHETIC: LLE 0x1001cdb5
// SYNTHETIC: LLE 0x1001cdc1
// SYNTHETIC: LLE 0x1001cdcb
// SYNTHETIC: LLE 0x1001cdd5
// SYNTHETIC: LLE 0x1001cde5
// SYNTHETIC: LLE 0x1001cdf1
// SYNTHETIC: LLE 0x1001cdfb
// SYNTHETIC: LLE 0x1001ce05
// SYNTHETIC: LLE 0x1001ce15
// SYNTHETIC: LLE 0x1001ce21
// SYNTHETIC: LLE 0x1001ce2b
// SYNTHETIC: LLE 0x1001ce35
// SYNTHETIC: LLE 0x1001ce45
// SYNTHETIC: LLE 0x1001ce51

static const float kDefaultWeight = 1.0f;       // 0x10097b60 (unit-local; loaded as a dword)
static const float kPlayerColorWeight = 2.0f;   // 0x10097b64
static const float kBorderTolerance = 0.001f;   // 0x10097b68

// FUNCTION: LLE 0x1001ce5b
// Builds inLODCount detail levels by edge collapse (player-colour edges cost double), then does the same for submodels
void GEModel::ComputeLODReduction(unsigned long inLODCount)
{
    GE3DPoint thePoint;
    if (inLODCount <= 1)
        return;
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    GEPolygonArray** thePolygonArrays = new GEPolygonArray*[inLODCount];
    unsigned long i;
    unsigned long j;
    unsigned long theLOD;
    unsigned long k;
    thePolygonArrays[0] = this->mPolygonArrays[0];
    for (theLOD = 1; theLOD < this->mLODCount; theLOD++) {
        if (this->mPolygonArrays[theLOD])
            delete this->mPolygonArrays[theLOD];
    }
    delete[] this->mPolygonArrays;
    this->mPolygonArrays = thePolygonArrays;
    this->mLODCount = inLODCount;
    for (i = 0; i < this->mMaterialCount; i++) {
        GEMaterial* theMaterial = new GEMaterial(theRasterizer->GetScreenID() + 1, inLODCount);
        theMaterial->SetColor(this->mMaterials[i]->GetColor());
        theMaterial->SetName(this->mMaterials[i]->GetName());
        theMaterial->SetOpacity(this->mMaterials[i]->GetOpacity());
        theMaterial->SetPolygonStartingIndex(this->mMaterials[i]->GetPolygonStartingIndex(0), 0);
        theMaterial->SetPolygonRangeLength(this->mMaterials[i]->GetPolygonRangeLength(0), 0);
        theMaterial->SetVertexStartingIndex(this->mMaterials[i]->GetVertexStartingIndex());
        theMaterial->SetVertexRangeLength(this->mMaterials[i]->GetVertexRangeLength());
        theMaterial->SetTextureIndex(this->mMaterials[i]->GetTextureIndex());
        this->mMaterials[i]->UReferenceCounter::Dereference();
        this->mMaterials[i] = theMaterial;
    }
    UList<GELODPolygon*>* thePolygonLists = new UList<GELODPolygon*>[this->mMaterialCount];
    UList<GELODVertex*>* theVertexLists = new UList<GELODVertex*>[this->mMaterialCount];
    std::vector<GELODVertex*>* theVertices = new std::vector<GELODVertex*>[this->mMaterialCount];
    UList<GELODMetaVertex*>* theMetaVertexLists = new UList<GELODMetaVertex*>[this->mMaterialCount];
    for (i = 0; i < this->mMaterialCount; i++) {
        theVertices[i].resize(this->mMaterials[i]->GetVertexRangeLength());
        for (j = 0; j < this->mMaterials[i]->GetVertexRangeLength(); j++) {
            this->GetVertexArray(0)->GetPoint(thePoint, this->mMaterials[i]->GetVertexStartingIndex() + j);
            theVertices[i][j] = new GELODVertex(thePoint, j);
            theVertexLists[i].AddHead(theVertices[i][j]);
        }
        for (k = 0; k < this->mMaterials[i]->GetPolygonRangeLength(0); k++) {
            GELODPolygon* thePolygon = new GELODPolygon(
                theVertices[i][this->GetPolygonArray(0)->GetVertexIndex1(this->mMaterials[i]->GetPolygonStartingIndex(0) + k)],
                theVertices[i][this->GetPolygonArray(0)->GetVertexIndex2(this->mMaterials[i]->GetPolygonStartingIndex(0) + k)],
                theVertices[i][this->GetPolygonArray(0)->GetVertexIndex3(this->mMaterials[i]->GetPolygonStartingIndex(0) + k)]);
            thePolygonLists[i].AddHead(thePolygon);
        }
        for (UListNode<GELODVertex*>* theNode = theVertexLists[i].mHead; theNode; theNode = theNode->mNext)
            theNode->mData->UpdateAdjacentVertexList();
        GELODMetaVertex::BuildMetaVertexList(theVertexLists[i], theMetaVertexLists[i]);
        GELODMetaVertex::ComputeAllCollapseCosts(theMetaVertexLists[i]);
    }
    for (theLOD = 1; theLOD < inLODCount; theLOD++) {
        unsigned long theTarget = (unsigned long)((float)(this->mLODCount - theLOD) / (float)this->mLODCount * this->mPolygonCount);
        if (theTarget == 0)
            theTarget = 1;
        unsigned long theCount = 0;
        for (i = 0; i < this->mMaterialCount; i++)
            theCount += thePolygonLists[i].mCount;
        while (theCount > theTarget) {
            long theBest = -1;
            float theBestCost = FLT_MAX;
            GELODMetaVertex* theBestVertex = NULL;
            for (i = 0; i < this->mMaterialCount; i++) {
                if (!(theMetaVertexLists[i].mCount > 0))
                    continue;
                float theWeight;
                if (this->mMaterials[i]->GetName() == "player color")
                    theWeight = kPlayerColorWeight;
                else
                    theWeight = 1.0f;
                GELODMetaVertex* theVertex = GELODMetaVertex::GetMinEdgeToCollapse(theMetaVertexLists[i]);
                if (!(theVertex->GetMinEdgeToCollapseCost() * theWeight < theBestCost))
                    continue;
                theBest = i;
                theBestVertex = theVertex;
                // Keep the weighted cost, the value the next materials are compared against.
                theBestCost = theVertex->GetMinEdgeToCollapseCost() * theWeight;
            }
            GELODMetaVertex::CollapseEdge(thePolygonLists[theBest], theVertexLists[theBest],
                                          theMetaVertexLists[theBest], theBestVertex);
            theCount = 0;
            for (i = 0; i < this->mMaterialCount; i++)
                theCount += thePolygonLists[i].mCount;
        }
        if (theCount > 0) {
            this->mPolygonArrays[theLOD] = theRasterizer->CreatePolygonArray(theCount);
            unsigned long theIndex = 0;
            for (i = 0; i < this->mMaterialCount; i++) {
                this->mMaterials[i]->SetPolygonStartingIndex(theIndex, theLOD);
                this->mMaterials[i]->SetPolygonRangeLength(thePolygonLists[i].mCount, theLOD);
                UListNode<GELODPolygon*>* thePolygonNode = thePolygonLists[i].mHead;
                for (k = 0; k < thePolygonLists[i].mCount; k++) {
                    this->mPolygonArrays[theLOD]->SetVertexIndex1(theIndex + k,
                                                            thePolygonNode->mData->GetVertex(0)->GetIndex());
                    this->mPolygonArrays[theLOD]->SetVertexIndex2(theIndex + k,
                                                            thePolygonNode->mData->GetVertex(1)->GetIndex());
                    this->mPolygonArrays[theLOD]->SetVertexIndex3(theIndex + k,
                                                            thePolygonNode->mData->GetVertex(2)->GetIndex());
                    thePolygonNode = thePolygonNode->mNext;
                }
                theIndex += thePolygonLists[i].mCount;
            }
        } else {
            this->mPolygonArrays[theLOD] = theRasterizer->CreatePolygonArray(this->mPolygonArrays[theLOD - 1]->GetSize());
            for (j = 0; j < this->mPolygonArrays[theLOD]->GetSize(); j++) {
                this->mPolygonArrays[theLOD]->SetVertexIndex1(j, this->mPolygonArrays[theLOD - 1]->GetVertexIndex1(j));
                this->mPolygonArrays[theLOD]->SetVertexIndex2(j, this->mPolygonArrays[theLOD - 1]->GetVertexIndex2(j));
                this->mPolygonArrays[theLOD]->SetVertexIndex3(j, this->mPolygonArrays[theLOD - 1]->GetVertexIndex3(j));
            }
            for (i = 0; i < this->mMaterialCount; i++) {
                this->mMaterials[i]->SetPolygonStartingIndex(this->mMaterials[i]->GetPolygonStartingIndex(theLOD - 1), theLOD);
                this->mMaterials[i]->SetPolygonRangeLength(this->mMaterials[i]->GetPolygonRangeLength(theLOD - 1), theLOD);
            }
        }
    }
    for (i = 0; i < this->mMaterialCount; i++) {
        // Free the last set of meta vertices too, then the per-material arrays themselves.
        UListNode<GELODMetaVertex*>* theMetaNode;
        for (theMetaNode = theMetaVertexLists[i].mHead; theMetaNode; theMetaNode = theMetaNode->mNext)
            delete theMetaNode->mData;
        UListNode<GELODPolygon*>* thePolygonNode;
        for (thePolygonNode = thePolygonLists[i].mHead; thePolygonNode; thePolygonNode = thePolygonNode->mNext)
            delete thePolygonNode->mData;
        UListNode<GELODVertex*>* theVertexNode;
        for (theVertexNode = theVertexLists[i].mHead; theVertexNode; theVertexNode = theVertexNode->mNext)
            delete theVertexNode->mData;
    }
    delete[] theMetaVertexLists;
    delete[] thePolygonLists;
    delete[] theVertexLists;
    delete[] theVertices;
    for (i = 0; i < this->mSubModelCount; i++)
        this->mSubModels[i]->ComputeLODReduction(inLODCount);
}

// ---- GELODPolygon

// FUNCTION: LLE 0x1001d75e
// Creates a reduction triangle on three vertices, computing its normal and registering it with each vertex
GELODPolygon::GELODPolygon(GELODVertex* inVertex1, GELODVertex* inVertex2, GELODVertex* inVertex3)
{
    this->mVertices[0] = inVertex1;
    this->mVertices[1] = inVertex2;
    this->mVertices[2] = inVertex3;
    this->ComputeNormal();
    this->mVertices[0]->AddAdjacentPolygon(this);
    this->mVertices[1]->AddAdjacentPolygon(this);
    this->mVertices[2]->AddAdjacentPolygon(this);
}

// Destroys the reduction triangle (nothing to free); ICF-folded into the bare ret at 0x1000c16a.
GELODPolygon::~GELODPolygon()
{
}

// FUNCTION: LLE 0x1001d7c0
// Returns the triangle's area
float GELODPolygon::ComputeArea()
{
    GEVector theEdge1;
    GEVector theEdge2;
    GEVector theCross;
    theEdge1.SetX(this->mVertices[1]->GetPosition().GetX() - this->mVertices[0]->GetPosition().GetX());
    theEdge1.SetY(this->mVertices[1]->GetPosition().GetY() - this->mVertices[0]->GetPosition().GetY());
    theEdge1.SetZ(this->mVertices[1]->GetPosition().GetZ() - this->mVertices[0]->GetPosition().GetZ());
    theEdge2.SetX(this->mVertices[2]->GetPosition().GetX() - this->mVertices[0]->GetPosition().GetX());
    theEdge2.SetY(this->mVertices[2]->GetPosition().GetY() - this->mVertices[0]->GetPosition().GetY());
    theEdge2.SetZ(this->mVertices[2]->GetPosition().GetZ() - this->mVertices[0]->GetPosition().GetZ());
    theEdge1.CrossProduct(theEdge2, theCross);
    return theCross.GetMagnitude() * 0.5f;
}

// FUNCTION: LLE 0x1001d85d
// Recomputes the triangle's unit normal from its three vertex positions
void GELODPolygon::ComputeNormal()
{
    GEVector theEdge1;
    GEVector theEdge2;
    GE3DPoint thePoint1;
    GE3DPoint thePoint2;
    GE3DPoint thePoint3;
    thePoint1 = this->mVertices[0]->GetPosition();
    thePoint2 = this->mVertices[1]->GetPosition();
    thePoint3 = this->mVertices[2]->GetPosition();
    theEdge1.SetX(thePoint1.GetX() - thePoint2.GetX());
    theEdge1.SetY(thePoint1.GetY() - thePoint2.GetY());
    theEdge1.SetZ(thePoint1.GetZ() - thePoint2.GetZ());
    theEdge2.SetX(thePoint2.GetX() - thePoint3.GetX());
    theEdge2.SetY(thePoint2.GetY() - thePoint3.GetY());
    theEdge2.SetZ(thePoint2.GetZ() - thePoint3.GetZ());
    theEdge2.CrossProduct(theEdge1, this->mNormal);
    this->mNormal.Normalize();
}

// FUNCTION: LLE 0x1001d919
// Swaps one vertex of the triangle (the third if neither of the first two matches) and recomputes its normal
void GELODPolygon::ReplaceVertex(GELODVertex* inOld, GELODVertex* inNew)
{
    if (this->mVertices[0] == inOld)
        this->mVertices[0] = inNew;
    else if (this->mVertices[1] == inOld)
        this->mVertices[1] = inNew;
    else
        this->mVertices[2] = inNew;
    inOld->RemoveAdjacentPolygon(this);
    inNew->AddAdjacentPolygon(this);
    this->ComputeNormal();
}

// ---- GELODVertex

// FUNCTION: LLE 0x1001d958
// Creates a reduction vertex at a position with its index in the material's vertex range
GELODVertex::GELODVertex(GE3DPoint& inPosition, unsigned long inIndex)
{
    this->mPosition = inPosition;
    this->mIndex = inIndex;
}

// FUNCTION: LLE 0x1001d995
// Records a neighbouring vertex once (never the vertex itself)
void GELODVertex::AddAdjacentVertex(GELODVertex* inVertex)
{
    if (this != inVertex) {
        if (this->mAdjacentVertices.Find(inVertex) == NULL)
            this->mAdjacentVertices.AddHead(inVertex);
    }
}

// FUNCTION: LLE 0x1001d9bd
// Records an adjacent triangle once
void GELODVertex::AddAdjacentPolygon(GELODPolygon* inPolygon)
{
    if (this->mAdjacentPolygons.Find(inPolygon) == NULL)
        this->mAdjacentPolygons.AddHead(inPolygon);
}

// FUNCTION: LLE 0x1001d9e5
// Forgets an adjacent triangle if it is recorded
void GELODVertex::RemoveAdjacentPolygon(GELODPolygon* inPolygon)
{
    UListNode<GELODPolygon*>* theNode = this->mAdjacentPolygons.Find(inPolygon);
    if (theNode)
        this->mAdjacentPolygons.Remove(theNode);
}

// FUNCTION: LLE 0x1001da09
// Rebuilds the vertex's neighbour list from the corners of its adjacent triangles
void GELODVertex::UpdateAdjacentVertexList()
{
    UListNode<GELODPolygon*>* theNode = this->mAdjacentPolygons.mHead;
    this->mAdjacentVertices.Clear();
    for (; theNode; theNode = theNode->mNext) {
        this->AddAdjacentVertex(theNode->mData->GetVertex(0));
        this->AddAdjacentVertex(theNode->mData->GetVertex(1));
        this->AddAdjacentVertex(theNode->mData->GetVertex(2));
    }
}

// ---- GELODMetaVertex

// FUNCTION: LLE 0x1001da44
// Adds a vertex to the meta vertex once
void GELODMetaVertex::AddVertex(GELODVertex* inVertex)
{
    if (this->mVertices.Find(inVertex) == NULL)
        this->mVertices.AddHead(inVertex);
}

// FUNCTION: LLE 0x1001da68
// Records a neighbouring meta vertex once (never itself)
void GELODMetaVertex::AddAdjacentMetaVertex(GELODMetaVertex* inVertex)
{
    if (this != inVertex) {
        if (this->mAdjacentVertices.Find(inVertex) == NULL)
            this->mAdjacentVertices.AddHead(inVertex);
    }
}

// FUNCTION: LLE 0x1001da94
// Rebuilds the meta vertex list: one meta vertex per distinct position, grouping the vertices there, plus adjacency
void GELODMetaVertex::BuildMetaVertexList(UList<GELODVertex*>& inVertices, UList<GELODMetaVertex*>& outMetaVertices)
{
    UListNode<GELODMetaVertex*>* theMetaNode;
    GELODMetaVertex* theMetaVertex;
    for (theMetaNode = outMetaVertices.mHead; theMetaNode; theMetaNode = theMetaNode->mNext)
        delete theMetaNode->mData;
    outMetaVertices.Clear();
    for (UListNode<GELODVertex*>* theNode = inVertices.mHead; theNode; theNode = theNode->mNext) {
                for (theMetaNode = outMetaVertices.mHead; theMetaNode; theMetaNode = theMetaNode->mNext) {
            if (theNode->mData->GetPosition() == theMetaNode->mData->GetVertexListHead()->mData->GetPosition())
                break;
        }
        if (theMetaNode)
            theMetaNode->mData->AddVertex(theNode->mData);
        else {
            theMetaVertex = new GELODMetaVertex;
            theMetaVertex->AddVertex(theNode->mData);
            outMetaVertices.AddHead(theMetaVertex);
        }
    }
    GELODMetaVertex::ComputeMetaVertexAdjacencyInfo(outMetaVertices);
}

// FUNCTION: LLE 0x1001db73
// Collapses a meta vertex onto its cheapest neighbour, drops degenerate triangles and its vertices, and rebuilds costs
void GELODMetaVertex::CollapseEdge(UList<GELODPolygon*>& ioPolygons, UList<GELODVertex*>& ioVertices,
                                   UList<GELODMetaVertex*>& ioMetaVertices, GELODMetaVertex* inVertex)
{
    if (inVertex->mMinEdgeToCollapse) {
        for (UListNode<GELODVertex*>* theNode = inVertex->mVertices.mHead; theNode; theNode = theNode->mNext) {
            UListNode<GELODVertex*>* theTarget = NULL;
            UListNode<GELODVertex*>* theFirst = inVertex->mMinEdgeToCollapse->mVertices.mHead;
            for (UListNode<GELODVertex*>* theOther = theFirst; theOther && !theTarget; theOther = theOther->mNext) {
                for (UListNode<GELODVertex*>* theAdjacent = theNode->mData->mAdjacentVertices.mHead;
                     theAdjacent && !theTarget; theAdjacent = theAdjacent->mNext) {
                    if (theOther->mData == theAdjacent->mData)
                        theTarget = theOther;
                }
            }
            if (theTarget == NULL)
                theTarget = theFirst;
            UListNode<GELODPolygon*>* thePolygonNode = theNode->mData->mAdjacentPolygons.mHead;
            while (thePolygonNode) {
                UListNode<GELODPolygon*>* theNext = thePolygonNode->mNext;
                thePolygonNode->mData->ReplaceVertex(theNode->mData, theTarget->mData);
                thePolygonNode = theNext;
            }
        }
    }
    UListNode<GELODPolygon*>* thePolygonNode = ioPolygons.mHead;
    while (thePolygonNode) {
        UListNode<GELODPolygon*>* theNext = thePolygonNode->mNext;
        GELODPolygon* thePolygon = thePolygonNode->mData;
        if (thePolygon->mVertices[0]->GetPosition() == thePolygon->mVertices[1]->GetPosition() ||
            thePolygon->mVertices[0]->GetPosition() == thePolygon->mVertices[2]->GetPosition() ||
            thePolygon->mVertices[1]->GetPosition() == thePolygon->mVertices[2]->GetPosition()) {
            GELODVertex* theVertices[3];
            for (long j = 0; j < 3; j++)
                theVertices[j] = thePolygon->mVertices[j];
            UListNode<GELODPolygon*>* theFound = ioPolygons.Find(thePolygon);
            thePolygon = theFound->mData;
            delete thePolygon;
            ioPolygons.Remove(theFound);
            for (long i = 0; i < 3; i++)
                theVertices[i]->RemoveAdjacentPolygon(thePolygon);
        }
        thePolygonNode = theNext;
    }
    for (UListNode<GELODVertex*>* theNode = inVertex->mVertices.mHead; theNode; theNode = theNode->mNext) {
        UListNode<GELODVertex*>* theFound = ioVertices.Find(theNode->mData);
        ioVertices.Remove(theFound);
        delete theNode->mData;
    }
    for (UListNode<GELODVertex*>* theVertexNode = ioVertices.mHead; theVertexNode; theVertexNode = theVertexNode->mNext)
        theVertexNode->mData->UpdateAdjacentVertexList();
    GELODMetaVertex::BuildMetaVertexList(ioVertices, ioMetaVertices);
    GELODMetaVertex::ComputeAllCollapseCosts(ioMetaVertices);
}

// FUNCTION: LLE 0x1001dd3d
// Finds each meta vertex's cheapest neighbour to collapse onto (cost 0 when it has none)
void GELODMetaVertex::ComputeAllCollapseCosts(UList<GELODMetaVertex*>& ioMetaVertices)
{
    for (UListNode<GELODMetaVertex*>* theNode = ioMetaVertices.mHead; theNode; theNode = theNode->mNext) {
        theNode->mData->mMinEdgeToCollapse = NULL;
        theNode->mData->mMinEdgeToCollapseCost = FLT_MAX;
        for (UListNode<GELODMetaVertex*>* theAdjacent = theNode->mData->mAdjacentVertices.mHead; theAdjacent;
             theAdjacent = theAdjacent->mNext) {
            float theCost = theNode->mData->ComputeEdgeCollapseCost(theAdjacent->mData);
            if (theCost < theNode->mData->mMinEdgeToCollapseCost) {
                theNode->mData->mMinEdgeToCollapseCost = theCost;
                theNode->mData->mMinEdgeToCollapse = theAdjacent->mData;
            }
        }
        if (theNode->mData->mMinEdgeToCollapse == NULL)
            theNode->mData->mMinEdgeToCollapseCost = 0.0f;
    }
}

// FUNCTION: LLE 0x1001dd9e
// Returns the cost of collapsing onto a neighbour: edge length times surface curvature (1 on borders and creases)
float GELODMetaVertex::ComputeEdgeCollapseCost(GELODMetaVertex* inVertex)
{
    GEVector theSum;
    UList<GELODPolygon*> theAdjacent;
    UList<GELODPolygon*> theShared;
    float theCurvature = 0.0f;
    float theLength = this->GetVertexListHead()->mData->GetPosition().DistanceTo(inVertex->GetVertexListHead()->mData->GetPosition());
    UListNode<GELODVertex*>* theNode;
    for (theNode = this->mVertices.mHead; theNode; theNode = theNode->mNext) {
        for (UListNode<GELODPolygon*>* thePolygonNode = theNode->mData->mAdjacentPolygons.mHead; thePolygonNode;
             thePolygonNode = thePolygonNode->mNext) {
            theAdjacent.AddHead(thePolygonNode->mData);
            for (UListNode<GELODVertex*>* theOther = inVertex->mVertices.mHead; theOther; theOther = theOther->mNext) {
                if (thePolygonNode->mData->GetVertex(0) == theOther->mData || thePolygonNode->mData->GetVertex(1) == theOther->mData ||
                    thePolygonNode->mData->GetVertex(2) == theOther->mData)
                    theShared.AddHead(thePolygonNode->mData);
            }
        }
    }
    for (UListNode<GELODPolygon*>* theFace = theAdjacent.mHead; theFace; theFace = theFace->mNext) {
        float theMinimum = 1.0f;
        for (UListNode<GELODPolygon*>* theSide = theShared.mHead; theSide; theSide = theSide->mNext)
            if (theMinimum >= (1.0f - theSide->mData->GetNormal().DotProduct(theFace->mData->GetNormal())) * 0.5f)
                theMinimum = (1.0f - theFace->mData->GetNormal().DotProduct(theSide->mData->GetNormal())) * 0.5f;
        theCurvature = max(theCurvature, theMinimum);
    }
    if (this->GetIsOnBorder() || theShared.mCount > 2)
        theCurvature = kDefaultWeight;
    else {
        for (UListNode<GELODPolygon*>* theSide = theShared.mHead; theSide; theSide = theSide->mNext) {
            GEVector& theNormal = theSide->mData->GetNormal();
            theSum.mX += theNormal.mX;
            theSum.mY += theNormal.mY;
            theSum.mZ += theNormal.mZ;
        }
        if (theSum.mX < kBorderTolerance && theSum.mY < kBorderTolerance && theSum.mZ < kBorderTolerance)
            theCurvature = kDefaultWeight;
    }
    return theLength * theCurvature;
}

// FUNCTION: LLE 0x1001df48
// Links every pair of meta vertices whose vertices are neighbours
void GELODMetaVertex::ComputeMetaVertexAdjacencyInfo(UList<GELODMetaVertex*>& ioMetaVertices)
{
    for (UListNode<GELODMetaVertex*>* theNode = ioMetaVertices.mHead; theNode; theNode = theNode->mNext) {
        for (UListNode<GELODMetaVertex*>* theOther = ioMetaVertices.mHead; theOther; theOther = theOther->mNext) {
            if (theNode == theOther)
                continue;
            bool theAdjacent = false;
            for (UListNode<GELODVertex*>* theVertex = theNode->mData->mVertices.mHead; theVertex && !theAdjacent;
                 theVertex = theVertex->mNext) {
                for (UListNode<GELODVertex*>* theOtherVertex = theOther->mData->mVertices.mHead;
                     theOtherVertex && !theAdjacent; theOtherVertex = theOtherVertex->mNext) {
                    for (UListNode<GELODVertex*>* theNeighbour = theOtherVertex->mData->mAdjacentVertices.mHead;
                         theNeighbour && !theAdjacent; theNeighbour = theNeighbour->mNext) {
                        if (theVertex->mData == theNeighbour->mData)
                            theAdjacent = true;
                    }
                }
            }
            if (theAdjacent) {
                theNode->mData->AddAdjacentMetaVertex(theOther->mData);
                theOther->mData->AddAdjacentMetaVertex(theNode->mData);
            }
        }
    }
}

// FUNCTION: LLE 0x1001dfd6
// Returns whether the meta vertex lies on a mesh border (some neighbour shares only one triangle with it)
bool GELODMetaVertex::GetIsOnBorder()
{
    bool theResult = false;
    for (UListNode<GELODMetaVertex*>* theNode = this->mAdjacentVertices.mHead; theNode; theNode = theNode->mNext) {
        long theCount = 0;
        for (UListNode<GELODVertex*>* theVertex = this->mVertices.mHead; theVertex; theVertex = theVertex->mNext) {
            for (UListNode<GELODPolygon*>* thePolygon = theVertex->mData->mAdjacentPolygons.mHead; thePolygon;
                 thePolygon = thePolygon->mNext) {
                for (UListNode<GELODVertex*>* theOther = theNode->mData->mVertices.mHead; theOther;
                     theOther = theOther->mNext) {
                    if (thePolygon->mData->HasVertex(theOther->mData))
                        theCount++;
                }
            }
        }
        if (theCount == 1) {
            theResult = true;
            break;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1001e061
// Returns the meta vertex with the lowest collapse cost (the list must not be empty)
GELODMetaVertex* GELODMetaVertex::GetMinEdgeToCollapse(UList<GELODMetaVertex*>& inMetaVertices)
{
    GELODMetaVertex* theResult = inMetaVertices.mHead->mData;
    for (UListNode<GELODMetaVertex*>* theNode = inMetaVertices.mHead; theNode; theNode = theNode->mNext) {
        if (theNode->mData->GetMinEdgeToCollapseCost() < theResult->GetMinEdgeToCollapseCost())
            theResult = theNode->mData;
    }
    return theResult;
}

// Compiler-generated after the unit's code: UList<GELODMetaVertex*>'s vector deleting dtor (0x1001d707, right after
// ComputeLODReduction), vector<GELODVertex*>::resize, UList<GELOD*>::AddHead x3, the node pool's New, then the pool
// registrations.
// TEMPLATE: LLE 0x1001d707
// TEMPLATE: LLE 0x1001e08a
// TEMPLATE: LLE 0x1001e135
// TEMPLATE: LLE 0x1001e169
// TEMPLATE: LLE 0x1001e19d
// TEMPLATE: LLE 0x1001e1d1

// UList<GELODVertex*>/<GELODPolygon*>/<GELODMetaVertex*>::sNodePool init wrapper and atexit registrars (GELOD.h via
// GEModel.h; the $D ctors are GEBoundingBox.cpp's COMDATs 0x10014ed4/0x10014efb/0x10014f22)
// SYNTHETIC: LLE 0x1001e2ba
// SYNTHETIC: LLE 0x1001e2d8
// SYNTHETIC: LLE 0x1001e2e4
// SYNTHETIC: LLE 0x1001e2f0

// UMemoryPool<UListNode<GELODMetaVertex*> > members (lle/UMemoryPool.h), emitted after the init chain: AddChunk,
// UMemoryPoolChunk ctor, SetSize (ICF keeps these copies for all three node types)
// TEMPLATE: LLE 0x1001e2fc
// TEMPLATE: LLE 0x1001e36f
// TEMPLATE: LLE 0x1001e3db
