// C:\EEX\Low-Level Engine\Graphics Engine\GETerrainMesh.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10026fde..0x10032ae2: the U2D key chains (U2DPoint.h, U2DRectangle.h, U2DPolygon.h; no guard byte) and the seven
// static UString IDs (one guard byte), the row members, ctor, dtor, the members roughly alphabetical up to
// AddWaterPolys, the four edge-strip statics' init chains (0x1002c1b0), GetIsSpaceTile, UpdateVisibleTerrain,
// SetIsSpace, Load, Save, then the template COMDATs (vectors, maps, U2DPolygon<float>/U2DConvexHull<float>/
// U2DGrahamsAlgorithm<float> from lle/U2DPolygon.h, and their std::sort/unique instantiations).
// The accessors are header-inline (GEBoundingBox.cpp emits them).
// The std::sort/unique internals come from include/dinkum/algorithm; U2DPolygon<float>::Sort uses an explicit
// std::less<U2DPoint<float> > (lle/U2DPolygon.h). Still unemitted because our compile inlines them (the original's
// budget refuses them): the GETMPolygon ctor copy 0x10027635 (map ctors, GETMRenderVertex array ctors), the pair
// ctors 0x1002fe2b/0x100308a7 (GetClosestPoints) and vector<GAPoint>() 0x10030249 (U2DConvexHull::Compute).
//
// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10026fde
// SYNTHETIC: LLE 0x10026fe8
// SYNTHETIC: LLE 0x10026ff8
// SYNTHETIC: LLE 0x10027004
// SYNTHETIC: LLE 0x1002700e
// SYNTHETIC: LLE 0x10027018
// SYNTHETIC: LLE 0x10027028
// SYNTHETIC: LLE 0x10027034
// SYNTHETIC: LLE 0x1002703e
// SYNTHETIC: LLE 0x10027048
// SYNTHETIC: LLE 0x10027058
// SYNTHETIC: LLE 0x10027064
// SYNTHETIC: LLE 0x1002706e
// SYNTHETIC: LLE 0x10027078
// SYNTHETIC: LLE 0x10027088
// SYNTHETIC: LLE 0x10027094
// SYNTHETIC: LLE 0x1002709e
// SYNTHETIC: LLE 0x100270a8
// SYNTHETIC: LLE 0x100270b8
// SYNTHETIC: LLE 0x100270c4
// SYNTHETIC: LLE 0x100270ce
// SYNTHETIC: LLE 0x100270d8
// SYNTHETIC: LLE 0x100270e8
// SYNTHETIC: LLE 0x100270f4
// SYNTHETIC: LLE 0x100270fe
// SYNTHETIC: LLE 0x10027108
// SYNTHETIC: LLE 0x10027118
// SYNTHETIC: LLE 0x10027124
// SYNTHETIC: LLE 0x1002712e
// SYNTHETIC: LLE 0x10027138
// SYNTHETIC: LLE 0x10027148
// SYNTHETIC: LLE 0x10027154
// SYNTHETIC: LLE 0x1002716f
// SYNTHETIC: LLE 0x10027179
// SYNTHETIC: LLE 0x10027189
// SYNTHETIC: LLE 0x10027195
// SYNTHETIC: LLE 0x100271b0
// SYNTHETIC: LLE 0x100271ba
// SYNTHETIC: LLE 0x100271ca
// SYNTHETIC: LLE 0x100271d6
// SYNTHETIC: LLE 0x100271f1
// SYNTHETIC: LLE 0x100271fb
// SYNTHETIC: LLE 0x1002720b
// SYNTHETIC: LLE 0x10027217
// SYNTHETIC: LLE 0x10027232
// SYNTHETIC: LLE 0x1002723c
// SYNTHETIC: LLE 0x1002724c
// SYNTHETIC: LLE 0x10027258
// SYNTHETIC: LLE 0x10027273
// SYNTHETIC: LLE 0x1002727d
// SYNTHETIC: LLE 0x1002728d
// SYNTHETIC: LLE 0x10027299
// SYNTHETIC: LLE 0x100272b4
// SYNTHETIC: LLE 0x100272be
// SYNTHETIC: LLE 0x100272ce
// SYNTHETIC: LLE 0x100272da
// SYNTHETIC: LLE 0x10027635
// SYNTHETIC: LLE 0x1002764d
// SYNTHETIC: LLE 0x10027905
// SYNTHETIC: LLE 0x1002b1f6
// SYNTHETIC: LLE 0x1002d28e
// TEMPLATE: LLE 0x1002d7ad
// TEMPLATE: LLE 0x1002d7d9
// TEMPLATE: LLE 0x1002d804
// TEMPLATE: LLE 0x1002d818
// TEMPLATE: LLE 0x1002d844
// TEMPLATE: LLE 0x1002d87f
// TEMPLATE: LLE 0x1002d8ab
// TEMPLATE: LLE 0x1002d8e6
// TEMPLATE: LLE 0x1002d912
// TEMPLATE: LLE 0x1002d93e
// TEMPLATE: LLE 0x1002d96a
// TEMPLATE: LLE 0x1002d996
// TEMPLATE: LLE 0x1002d9c1
// TEMPLATE: LLE 0x1002d9e6
// TEMPLATE: LLE 0x1002d9f8
// TEMPLATE: LLE 0x1002da4f
// TEMPLATE: LLE 0x1002dab8
// TEMPLATE: LLE 0x1002daca
// TEMPLATE: LLE 0x1002db24
// TEMPLATE: LLE 0x1002db68
// TEMPLATE: LLE 0x1002dbb8
// TEMPLATE: LLE 0x1002dbd9
// TEMPLATE: LLE 0x1002dc4a
// TEMPLATE: LLE 0x1002dc87
// TEMPLATE: LLE 0x1002df60
// TEMPLATE: LLE 0x1002e226
// TEMPLATE: LLE 0x1002e4b7
// TEMPLATE: LLE 0x1002e765
// TEMPLATE: LLE 0x1002e797
// TEMPLATE: LLE 0x1002e7cc
// TEMPLATE: LLE 0x1002e804
// TEMPLATE: LLE 0x1002e82f
// TEMPLATE: LLE 0x1002e84a
// TEMPLATE: LLE 0x1002e931
// TEMPLATE: LLE 0x1002ef29
// TEMPLATE: LLE 0x1002f1bd
// TEMPLATE: LLE 0x1002f28d
// TEMPLATE: LLE 0x1002f318
// TEMPLATE: LLE 0x1002f37e
// TEMPLATE: LLE 0x1002f3f1
// TEMPLATE: LLE 0x1002f5ef
// TEMPLATE: LLE 0x1002f649
// TEMPLATE: LLE 0x1002f692
// TEMPLATE: LLE 0x1002f795
// TEMPLATE: LLE 0x1002f917
// TEMPLATE: LLE 0x1002f9eb
// TEMPLATE: LLE 0x1002fc0a
// TEMPLATE: LLE 0x1002fc16
// TEMPLATE: LLE 0x1002fd14
// TEMPLATE: LLE 0x1002fd20
// TEMPLATE: LLE 0x1002fd70
// TEMPLATE: LLE 0x1002fd94
// TEMPLATE: LLE 0x1002fdb1
// TEMPLATE: LLE 0x1002fddd
// TEMPLATE: LLE 0x1002fe08
// TEMPLATE: LLE 0x1002fe19
// TEMPLATE: LLE 0x1002fe2b
// TEMPLATE: LLE 0x1002fe41
// TEMPLATE: LLE 0x1002fe6c
// TEMPLATE: LLE 0x1002fe8b
// TEMPLATE: LLE 0x1002fe9e
// TEMPLATE: LLE 0x1002feb7
// TEMPLATE: LLE 0x1002fec4
// TEMPLATE: LLE 0x1002ff7a
// TEMPLATE: LLE 0x1002ffb4
// TEMPLATE: LLE 0x1002ffe4
// TEMPLATE: LLE 0x1002fff7
// TEMPLATE: LLE 0x10030022
// TEMPLATE: LLE 0x10030047
// TEMPLATE: LLE 0x10030090
// TEMPLATE: LLE 0x1003013b
// TEMPLATE: LLE 0x10030176
// TEMPLATE: LLE 0x100301d8
// TEMPLATE: LLE 0x10030249
// TEMPLATE: LLE 0x1003025f
// TEMPLATE: LLE 0x10030288
// TEMPLATE: LLE 0x100302c9
// TEMPLATE: LLE 0x100302fb
// TEMPLATE: LLE 0x1003048f
// TEMPLATE: LLE 0x100304d1
// TEMPLATE: LLE 0x10030515
// TEMPLATE: LLE 0x10030549
// TEMPLATE: LLE 0x1003057d
// TEMPLATE: LLE 0x100305cf
// TEMPLATE: LLE 0x10030621
// TEMPLATE: LLE 0x10030746
// TEMPLATE: LLE 0x1003078b
// TEMPLATE: LLE 0x10030839
// TEMPLATE: LLE 0x10030893
// TEMPLATE: LLE 0x100308a7
// TEMPLATE: LLE 0x100308c3
// TEMPLATE: LLE 0x100308e5
// TEMPLATE: LLE 0x10030908
// TEMPLATE: LLE 0x100309b3
// TEMPLATE: LLE 0x10030c50
// TEMPLATE: LLE 0x10030c9a
// TEMPLATE: LLE 0x10030cc1
// TEMPLATE: LLE 0x10030cd5
// TEMPLATE: LLE 0x10030d47
// TEMPLATE: LLE 0x10030e32
// TEMPLATE: LLE 0x10030fed
// TEMPLATE: LLE 0x100310f7
// TEMPLATE: LLE 0x10031105
// TEMPLATE: LLE 0x100311c5
// TEMPLATE: LLE 0x100311ea
// TEMPLATE: LLE 0x100312ae
// TEMPLATE: LLE 0x100312ed
// TEMPLATE: LLE 0x1003132b
// TEMPLATE: LLE 0x10031378
// TEMPLATE: LLE 0x100313a0
// TEMPLATE: LLE 0x100313ce
// TEMPLATE: LLE 0x10031405
// TEMPLATE: LLE 0x10031436
// TEMPLATE: LLE 0x10031459
// TEMPLATE: LLE 0x1003148c
// TEMPLATE: LLE 0x100314b8
// TEMPLATE: LLE 0x100314ec
// TEMPLATE: LLE 0x1003153c
// TEMPLATE: LLE 0x100315a0
// TEMPLATE: LLE 0x100315e6
// TEMPLATE: LLE 0x1003161a
// TEMPLATE: LLE 0x1003173b
// TEMPLATE: LLE 0x1003178d
// TEMPLATE: LLE 0x100317ac
// TEMPLATE: LLE 0x100317d5
// TEMPLATE: LLE 0x100318ad
// TEMPLATE: LLE 0x100318eb
// TEMPLATE: LLE 0x1003199c
// TEMPLATE: LLE 0x100319a2
// TEMPLATE: LLE 0x100319d9
// TEMPLATE: LLE 0x10031a0c
// TEMPLATE: LLE 0x10031b14
// TEMPLATE: LLE 0x10031b4c
// TEMPLATE: LLE 0x10031b8c
// TEMPLATE: LLE 0x10031bc2
// TEMPLATE: LLE 0x10031ca4
// TEMPLATE: LLE 0x10031caa
// TEMPLATE: LLE 0x10031cef
// TEMPLATE: LLE 0x10031d43
// TEMPLATE: LLE 0x10031e6a
// TEMPLATE: LLE 0x10031ebe
// TEMPLATE: LLE 0x10031f9a
// TEMPLATE: LLE 0x10031fd4
// TEMPLATE: LLE 0x10032015
// TEMPLATE: LLE 0x100321ac
// TEMPLATE: LLE 0x100321f4
// TEMPLATE: LLE 0x1003224e
// TEMPLATE: LLE 0x10032273
// TEMPLATE: LLE 0x10032298
// TEMPLATE: LLE 0x100322dd
// TEMPLATE: LLE 0x1003232b
// TEMPLATE: LLE 0x100323e6
// TEMPLATE: LLE 0x10032453
// TEMPLATE: LLE 0x10032487
// TEMPLATE: LLE 0x100324ac
// TEMPLATE: LLE 0x10032505
// TEMPLATE: LLE 0x1003252e
// TEMPLATE: LLE 0x100325b7
// TEMPLATE: LLE 0x10032622
// TEMPLATE: LLE 0x10032649
// TEMPLATE: LLE 0x10032714
// TEMPLATE: LLE 0x1003278e
// TEMPLATE: LLE 0x1003282c
// TEMPLATE: LLE 0x10032899
// TEMPLATE: LLE 0x100328fa
// TEMPLATE: LLE 0x10032941
// TEMPLATE: LLE 0x100329c3
// TEMPLATE: LLE 0x10032a2b
// TEMPLATE: LLE 0x10032a9a
#define LLE_GETERRAINMESH_CPP
#include <windows.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include "LostDebug.h"  // NOT original source: EE_LOST_DEBUG filler in U2DPolygon.h (Compute, GetScanLines; docs/LOST_DEBUG.md); before the lle headers since the template bodies need the macro (section layout checked unchanged); on the former blank line, so no __LINE__ moves
#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/URandom.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/UReferenceCounter.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"
#include "lle/U2DFractal.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GETextureSet.h"
#include "lle/GEViewport.h"
#include "lle/GETerrainMesh.h"

const UString GETerrainMesh::sPlayableWidthID("GETerrainMesh-PlayableWidth");
const UString GETerrainMesh::sPlayableHeightID("GETerrainMesh-PlayableHeight");
const UString GETerrainMesh::sTextureSetID("GETerrainMesh-TextureSet");
const UString GETerrainMesh::sWaterInfoID("GETerrainMesh-WaterInfo");
const UString GETerrainMesh::sZID("GETerrainMesh-Z-ID");
const UString GETerrainMesh::sFractalSeedID("GETerrainMesh-FractalSeed");
const UString GETerrainMesh::sColorLookupID("GETerrainMesh-ColorLookup");

// The eight neighbours of a tile and the shoreline bits a non-water (non-space) neighbour sets (0x10097b98)
// (provisional names).
struct GETMNeighbour {
    long mDX;
    long mDY;
    unsigned long mShoreline;
};

static const GETMNeighbour sNeighbours[8] = {
    { -1, 1, 4 }, { 1, 1, 8 }, { -1, -1, 2 }, { 1, -1, 16 }, { 0, 1, 12 }, { 1, 0, 24 }, { 0, -1, 18 }, { -1, 0, 6 }
};

// FUNCTION: LLE 0x100272f5
// Frees the point row's points and resets it to empty
void GETerrainMesh::GETMPointRow::DeallocateInternalData()
{
    delete[] this->mPoints;
    this->mPoints = NULL;
    this->mStart = 0;
    this->mCount = 0;
}

// FUNCTION: LLE 0x1002730d
// Allocates the point row's points for map columns inStart..inEnd, each sharing the given normal
void GETerrainMesh::GETMPointRow::Initialize(unsigned long inStart, unsigned long inEnd, GEVector* inNormal)
{
    this->mStart = inStart;
    this->mCount = inEnd - inStart + 1;
    this->mPoints = new GETMPoint[this->mCount];
    for (unsigned long i = 0; i < this->mCount; i++)
        this->mPoints[i].mNormal = inNormal;
}

void GETerrainMesh::GETMTileRow::DeallocateInternalData()
{
    delete[] this->mTiles;
    this->mTiles = NULL;
    this->mStart = 0;
    this->mCount = 0;
}

// FUNCTION: LLE 0x1002737a
// Allocates the tile row's tiles for map columns inStart..inEnd
void GETerrainMesh::GETMTileRow::Initialize(unsigned long inStart, unsigned long inEnd)
{
    this->mStart = inStart;
    this->mCount = inEnd - inStart + 1;
    this->mTiles = new GETMTile[this->mCount];
}

// FUNCTION: LLE 0x100273c9
// Creates an empty terrain mesh with the default water animation (period 80) and a 64x64 fractal seeded from the clock
GETerrainMesh::GETerrainMesh()
{
    EE_LOST_DEBUG(9);  // LOST-DEBUG: none independent (owner, 2026-10-25: exact is exact); smallest n (1..8 not exact)
    this->mVisibleWaterTilePercentage = 0.0f;
    this->mPointRows = NULL;
    this->mVisibleSpaceTilePercentage = 0.0f;
    this->mTileRows = NULL;
    this->mPointRowCount = 0;
    this->mTileRowCount = 0;
    this->mSpaceTileCount = 0;
    this->mWaterAnimationFrame = 0;
    this->mGlobalWaterSetID = 0;
    this->mWaterColorSetID = 0;
    this->mWaterAnimationPeriod = 80;
    this->mWaterTextureScale = -0.71428573f;
    this->mMinimumZ = FLT_MAX;
    this->mMaximumZ = -FLT_MAX;
    this->mFractalSeed = TMGetTime();
    this->mFractal = new U2DFractal(64, 0.0f, 10.5f, this->mFractalSeed);
}

// FUNCTION: LLE 0x1002766d
// Destroys the mesh: deletes its texture sets, skirt and cliff vertices, rows and fractal, and releases the textures
// its terrain and non-animated water materials referenced
GETerrainMesh::~GETerrainMesh()
{
    std::map<GETexture*, unsigned long>::iterator theIterator;
    unsigned long i = 0;
    for (; i < this->mRegisteredTextureSets.size(); i++)
        delete this->mRegisteredTextureSets[i];
    for (i = 0; i < this->mEdgeRenderVertices.size(); i++)
        delete this->mEdgeRenderVertices[i];
    for (i = 0; i < this->mCliffRenderVertices.size(); i++)
        delete this->mCliffRenderVertices[i];
    delete[] this->mPointRows;
    delete[] this->mTileRows;
    delete this->mFractal;
    for (theIterator = this->mTerrainMaterialMap.begin(); theIterator != this->mTerrainMaterialMap.end(); theIterator++) {
        if (theIterator->first != NULL)
            theIterator->first->Dereference();
    }
    i = 0;
    for (theIterator = this->mWaterMaterialMap.begin(); theIterator != this->mWaterMaterialMap.end(); theIterator++, i++) {
        if (i >= this->mWaterAnimationMaterialList.size() && theIterator->first != NULL)
            theIterator->first->Dereference();
    }
}

// FUNCTION: LLE 0x1002795f
// Recomputes a tile's shoreline bits from its eight neighbours: on a space map from a space tile's non-space
// neighbours (non-space tiles get 0), else from a water tile's land neighbours (land tiles are left alone)
void GETerrainMesh::ComputeShorelines(unsigned long inX, unsigned long inY)
{
    long i;
    unsigned long theShoreline;
    unsigned long theX;
    unsigned long theY;
    if (this->GetIsSpaceMap()) {
        if (!this->GetIsSpaceTile(inX, inY)) {
            this->SetWaterInfo(inX, inY, 0);
            return;
        }
        theShoreline = 0;
        for (i = 0; i < 8; i++) {
            theX = sNeighbours[i].mDX + inX;
            theY = sNeighbours[i].mDY + inY;
            if (this->GetIsValidTile(theX, theY) && !this->GetIsSpaceTile(theX, theY))
                theShoreline |= sNeighbours[i].mShoreline;
        }
    } else {
        if (!this->GetIsWaterTile(inX, inY))
            return;
        theShoreline = 1;
        for (i = 0; i < 8; i++) {
            theX = sNeighbours[i].mDX + inX;
            theY = sNeighbours[i].mDY + inY;
            if (this->GetIsValidTile(theX, theY) && !this->GetIsWaterTile(theX, theY))
                theShoreline |= sNeighbours[i].mShoreline;
        }
    }
    this->SetWaterInfo(inX, inY, (unsigned char)theShoreline);
}

// FUNCTION: LLE 0x10027aa4
// Computes a map point's normal from its neighbours' heights (missing ones use its own) and stores the quantized normal
void GETerrainMesh::ComputeVertexNormal(unsigned long inX, unsigned long inY)
{
    GEVector theNormal;
    GETMPointRow& theRow = this->mPointRows[inY];
    GETMPointRow& theNextRow = this->mPointRows[inY + 1];
    unsigned long theOther;
    float theZ = theRow.mPoints[inX - theRow.mStart].mZ;
    float theNW;
    float theN;
    float theNE;
    float theW;
    float theD;
    if (inY + 1 < this->mPointRowCount) {
        theOther = inX - theNextRow.mStart;
        if (theOther - 1 < theNextRow.mCount)
            theNW = theNextRow.mPoints[theOther - 1].mZ;
        else
            theNW = theZ;
        if (theOther < theNextRow.mCount)
            theN = theNextRow.mPoints[theOther].mZ;
        else
            theN = theZ;
        if (theOther + 1 < theNextRow.mCount)
            theNE = theNextRow.mPoints[theOther + 1].mZ;
        else
            theNE = theZ;
    } else {
        theNW = theZ;
        theN = theZ;
        theNE = theZ;
    }
    // the north heights become the differences in place (NE: SW - NE, N: S - N, NW: NW - SE)
    if (inY - 1 < this->mPointRowCount) {
        GETMPointRow& thePreviousRow = this->mPointRows[inY - 1];
        theOther = inX - thePreviousRow.mStart;
        if (theOther - 1 < thePreviousRow.mCount)
            theNE = thePreviousRow.mPoints[theOther - 1].mZ - theNE;
        else
            theNE = theZ - theNE;
        if (theOther < thePreviousRow.mCount)
            theN = thePreviousRow.mPoints[theOther].mZ - theN;
        else
            theN = theZ - theN;
        if (theOther + 1 < thePreviousRow.mCount)
            theNW = theNW - thePreviousRow.mPoints[theOther + 1].mZ;
        else
            theNW = theNW - theZ;
    } else {
        theNW = theNW - theZ;
        theN = theZ - theN;
        theNE = theZ - theNE;
    }
    if ((inX - theRow.mStart) - 1 < theRow.mCount)
        theW = theRow.mPoints[inX - theRow.mStart - 1].mZ;
    else
        theW = theZ;
    if ((inX - theRow.mStart) + 1 < theRow.mCount)
        theD = theW - theRow.mPoints[inX - theRow.mStart + 1].mZ;
    else
        theD = theW - theZ;
    theNW = theNW * 0.70710677f;
    theNE = theNE * 0.70710677f;
    theNormal.SetX(theD + theNE + theNW);
    theNormal.SetY(theNE + theN - theNW);
    theNormal.SetZ(5.0f);
    theRow.mPoints[inX - theRow.mStart].mNormal = theNormal.GetQuantizedVersion();
}

// FUNCTION: LLE 0x10027c53
// Rebuilds the mesh as a flat diamond map of inWidth x inHeight tiles: tile and point rows, edge sums, map-edge planes,
// every height 0 and every normal straight up
void GETerrainMesh::CreateFlatMesh(long inWidth, long inHeight)
{
    GEVector theUp(0.0f, 0.0f, 1.0f);
    GEVector* theNormal = theUp.GetQuantizedVersion();
    unsigned long theBottomCount = 0;
    unsigned long theTopCount = 0;
    unsigned long theStart = inHeight - 1;
    unsigned long theEnd = inHeight;
    bool theStartFalling = true;
    bool theEndRising = true;
    unsigned long i;
    unsigned long j;
    delete[] this->mPointRows;
    delete[] this->mTileRows;
    this->mSpaceTileCount = 0;
    this->mPlayableWidth = inWidth;
    this->mPlayableHeight = inHeight;
    this->mTileRowCount = inWidth + inHeight;
    this->mLeftEdge = inHeight - 1;
    this->mTopEdge = inHeight;
    this->mRightEdge = inWidth + inHeight - 1 + inWidth;
    this->mBottomEdge = inHeight;
    this->mTileRows = new GETMTileRow[this->mTileRowCount];
    for (i = 0; i < this->mTileRowCount; i++) {
        this->mTileRows[i].Initialize(theStart, theEnd);
        if (theStartFalling) {
            if (theStart != 0)
                theStart--;
            else
                theStartFalling = false;
        } else
            theStart++;
        if (theEndRising) {
            if (theEnd != this->mTileRowCount - 1)
                theEnd++;
            else
                theEndRising = false;
        } else
            theEnd--;
    }
    theStart = inHeight - 1;
    theEnd = inHeight + 1;
    theStartFalling = true;
    theEndRising = true;
    this->mPointRowCount = inWidth + inHeight + 1;
    this->mPointRows = new GETMPointRow[this->mPointRowCount];
    for (j = 0; j < this->mPointRowCount; j++) {
        this->mPointRows[j].Initialize(theStart, theEnd, theNormal);
        if (theStartFalling) {
            if (theStart != 0)
                theStart--;
            else {
                theBottomCount++;
                if (theBottomCount == 2)
                    theStartFalling = false;
            }
        } else
            theStart++;
        if (theEndRising) {
            if (theEnd != this->mPointRowCount - 1)
                theEnd++;
            else {
                theTopCount++;
                if (theTopCount == 2)
                    theEndRising = false;
            }
        } else
            theEnd--;
    }
    this->InitializePlanes();
    this->mMinimumZ = 0.0f;
    this->mMaximumZ = 0.0f;
}

// FUNCTION: LLE 0x10027e8e
// Adds a skirt vertex at (inX, inY, inZ) with a black, fully visible render vertex
void GETerrainMesh::CreateNewEdgeVertex(long inX, long inY, float inZ)
{
    GETMRenderVertex* theVertex;
    this->mEdgeWorldPoints.push_back(GE3DPoint((float)inX, (float)inY, inZ));
    theVertex = new GETMRenderVertex;
    theVertex->mColor.SetRed(0.0f);
    theVertex->mColor.SetGreen(0.0f);
    theVertex->mColor.SetBlue(0.0f);
    theVertex->mVisibility = 1.0f;
    theVertex->mClipFlags = 0x3f;
    this->mEdgeRenderVertices.push_back(theVertex);
}

// FUNCTION: LLE 0x10027f0a
// Fills the next polygon with three skirt vertices (indices relative to inBase), adds it to the untextured material
// and advances to the next polygon
void GETerrainMesh::DefineNewEdgePolygon(unsigned long inBase, unsigned long inVertex0, unsigned long inVertex1, unsigned long inVertex2,
                                         GETMPolygon*& ioPolygon)
{
    ioPolygon->mVertices[0] = this->mEdgeRenderVertices[inVertex0 + inBase];
    ioPolygon->mVertices[1] = this->mEdgeRenderVertices[inVertex1 + inBase];
    ioPolygon->mVertices[2] = this->mEdgeRenderVertices[inVertex2 + inBase];
    ioPolygon->mAlpha[0] = 1.0f;
    ioPolygon->mAlpha[1] = 1.0f;
    ioPolygon->mAlpha[2] = 1.0f;
    this->mTerrainMaterialList[0].mPolygons.push_back(ioPolygon);
    ioPolygon++;
    this->mVisiblePolyCount++;
}

// Not inlined: the out-of-line copy follows its first user (0x100282f5) (provisional name).
// FUNCTION: LLE 0x100282f5
// Returns false only when both camera-space end points lie outside the same frustum plane (near, far or a side)
inline bool IsSegmentInFrustum(const GEViewport& inViewport, const GE3DPoint& inStart, const GE3DPoint& inEnd)
{
    if (inViewport.GetFrontClippingDistance() > inStart.GetY() &&
        inViewport.GetFrontClippingDistance() > inEnd.GetY())
        return false;
    else if (inViewport.GetRearClippingDistance() < inStart.GetY() &&
        inViewport.GetRearClippingDistance() < inEnd.GetY())
        return false;
    else if (!inViewport.GetLeftPlane().IsPointOnOrAbovePlane(inStart) &&
        !inViewport.GetLeftPlane().IsPointOnOrAbovePlane(inEnd))
        return false;
    else if (!inViewport.GetRightPlane().IsPointOnOrAbovePlane(inStart) &&
        !inViewport.GetRightPlane().IsPointOnOrAbovePlane(inEnd))
        return false;
    else if (!inViewport.GetTopPlane().IsPointOnOrAbovePlane(inStart) &&
        !inViewport.GetTopPlane().IsPointOnOrAbovePlane(inEnd))
        return false;
    else if (!inViewport.GetBottomPlane().IsPointOnOrAbovePlane(inStart) &&
        !inViewport.GetBottomPlane().IsPointOnOrAbovePlane(inEnd))
        return false;
    else
        return true;
}

// One per map edge. Not inlined: each out-of-line copy follows its Generate*EdgePolygons user (provisional names).
// FUNCTION: LLE 0x1002821c
// Returns whether the back skirt from map point (inX, inY), inDepth tiles (+x, -y), is in view; false off the map
inline bool IsBackEdgeVisible(const GEViewport& inViewport, const GETerrainMesh& inMesh, long inX, long inY,
                              long inDepth)
{
    GE3DPoint theCameraStart;
    GE3DPoint theCameraEnd;
    GE3DPoint theStart;
    GE3DPoint theEnd;
    if (!inMesh.GetIsValidPoint(inX, inY))
        return false;
    theStart.SetX((float)inX);
    theStart.SetY((float)inY);
    theStart.SetZ(0.0f);
    theEnd.SetX((float)(inX + inDepth));
    theEnd.SetY((float)(inY - inDepth));
    theEnd.SetZ(0.0f);
    inViewport.GetCameraInverse().Apply(theStart, theCameraStart);
    inViewport.GetCameraInverse().Apply(theEnd, theCameraEnd);
    return IsSegmentInFrustum(inViewport, theCameraStart, theCameraEnd);
}

// FUNCTION: LLE 0x100298ae
// Returns whether the front skirt from map point (inX, inY), inDepth tiles (-x, +y), is in view; false off the map
inline bool IsFrontEdgeVisible(const GEViewport& inViewport, const GETerrainMesh& inMesh, long inX, long inY,
                               long inDepth)
{
    GE3DPoint theCameraStart;
    GE3DPoint theCameraEnd;
    GE3DPoint theStart;
    GE3DPoint theEnd;
    if (!inMesh.GetIsValidPoint(inX, inY))
        return false;
    theStart.SetX((float)inX);
    theStart.SetY((float)inY);
    theStart.SetZ(0.0f);
    theEnd.SetX((float)(inX - inDepth));
    theEnd.SetY((float)(inY + inDepth));
    theEnd.SetZ(0.0f);
    inViewport.GetCameraInverse().Apply(theStart, theCameraStart);
    inViewport.GetCameraInverse().Apply(theEnd, theCameraEnd);
    return IsSegmentInFrustum(inViewport, theCameraStart, theCameraEnd);
}

// FUNCTION: LLE 0x10029c11
// Returns whether the left skirt from map point (inX, inY), inDepth tiles (-x, -y), is in view; false off the map
inline bool IsLeftEdgeVisible(const GEViewport& inViewport, const GETerrainMesh& inMesh, long inX, long inY,
                              long inDepth)
{
    GE3DPoint theCameraStart;
    GE3DPoint theCameraEnd;
    GE3DPoint theStart;
    GE3DPoint theEnd;
    if (!inMesh.GetIsValidPoint(inX, inY))
        return false;
    theStart.SetX((float)inX);
    theStart.SetY((float)inY);
    theStart.SetZ(0.0f);
    theEnd.SetX((float)(inX - inDepth));
    theEnd.SetY((float)(inY - inDepth));
    theEnd.SetZ(0.0f);
    inViewport.GetCameraInverse().Apply(theStart, theCameraStart);
    inViewport.GetCameraInverse().Apply(theEnd, theCameraEnd);
    return IsSegmentInFrustum(inViewport, theCameraStart, theCameraEnd);
}

// FUNCTION: LLE 0x10029f84
// Returns whether the right skirt from map point (inX, inY), inDepth tiles (+x, +y), is in view; false off the map
inline bool IsRightEdgeVisible(const GEViewport& inViewport, const GETerrainMesh& inMesh, long inX, long inY,
                               long inDepth)
{
    GE3DPoint theCameraStart;
    GE3DPoint theCameraEnd;
    GE3DPoint theStart;
    GE3DPoint theEnd;
    if (!inMesh.GetIsValidPoint(inX, inY))
        return false;
    theStart.SetX((float)inX);
    theStart.SetY((float)inY);
    theStart.SetZ(0.0f);
    theEnd.SetX((float)(inX + inDepth));
    theEnd.SetY((float)(inY + inDepth));
    theEnd.SetZ(0.0f);
    inViewport.GetCameraInverse().Apply(theStart, theCameraStart);
    inViewport.GetCameraInverse().Apply(theEnd, theCameraEnd);
    return IsSegmentInFrustum(inViewport, theCameraStart, theCameraEnd);
}

// FUNCTION: LLE 0x10027f81
// Extends the visible back edge strip while its skirt stays in view, then builds the back skirt out to the camera's
// rear clip depth (three vertices per edge point, four triangles per step)
void GETerrainMesh::GenerateBackEdgePolygons(const GEViewport& inViewport, U2DPoint<long> (&ioEdge)[2],
                                             GETMPolygon*& ioPolygon, unsigned long& ioVertexBase)
{
    GE3DPoint theStart;
    GEVector theNormal;
    GE3DLine theLine;
    float theDistance;
    float theLimit;
    long theDepth;
    long theX;
    long theY;
    const GETransformation& theCamera = inViewport.GetCamera();
    theStart.SetX(theCamera.GetX());
    theStart.SetY(theCamera.GetY());
    theStart.SetZ(theCamera.GetZ());
    this->mBackPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.SetStart(theStart);
    theLine.SetDirection(theNormal);
    if (theLine.Intersects(this->mBackPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepth = UMFloor(theLimit);
    while (IsBackEdgeVisible(inViewport, *this, ioEdge[0].GetX() - 1, ioEdge[0].GetY() - 1, theDepth)) {
        ioEdge[0].SetX(ioEdge[0].GetX() - 1);
        ioEdge[0].SetY(ioEdge[0].GetY() - 1);
    }
    if (this->GetIsValidTile(ioEdge[0].GetX() - 1, ioEdge[0].GetY() - 1)) {
        ioEdge[0].SetX(ioEdge[0].GetX() - 1);
        ioEdge[0].SetY(ioEdge[0].GetY() - 1);
    }
    while (IsBackEdgeVisible(inViewport, *this, ioEdge[1].GetX() + 1, ioEdge[1].GetY() + 1, theDepth)) {
        ioEdge[1].SetX(ioEdge[1].GetX() + 1);
        ioEdge[1].SetY(ioEdge[1].GetY() + 1);
    }
    if (this->GetIsValidTile(ioEdge[1].GetX() + 1, ioEdge[1].GetY() + 1)) {
        ioEdge[1].SetX(ioEdge[1].GetX() + 1);
        ioEdge[1].SetY(ioEdge[1].GetY() + 1);
    }
    for (theX = ioEdge[0].GetX(), theY = ioEdge[0].GetY(); theX <= ioEdge[1].GetX(); theX++, theY++) {
        this->CreateNewEdgeVertex(theX, theY, this->GetZ(theX, theY));
        this->CreateNewEdgeVertex(theX + 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepth, theY - theDepth, 0.0f);
        if (theX == ioEdge[0].GetX())
            continue;
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 2, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 4, 5, ioPolygon);
        ioVertexBase += 3;
    }
    ioVertexBase += 3;
}

// FUNCTION: LLE 0x100283d9
// Builds the back-left corner skirt: a lone back or left strip ending at the corner is extended 1000 tiles past it;
// with both strips visible, a seven-vertex fan joins them
void GETerrainMesh::GenerateCornerBackLeftPolygons(const GEViewport& inViewport, U2DPoint<long> (&inEdgeA)[2],
                                                   bool inHasEdgeA, U2DPoint<long> (&inEdgeB)[2], bool inHasEdgeB,
                                                   GETMPolygon*& ioPolygon, unsigned long& ioVertexBase)
{
    GEVector theNormal;
    GE3DPoint theStart;
    GE3DLine theLine;
    float theDistance;
    float theLimit;
    long theDepthA;
    long theDepthB;
    long theX;
    long theY;
    float theZ;
    const GETransformation& theCamera = inViewport.GetCamera();
    theStart.SetX(theCamera.GetX());
    theStart.SetY(theCamera.GetY());
    theStart.SetZ(theCamera.GetZ());
    this->mBackPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.Define(theStart, theNormal);
    if (theLine.Intersects(this->mBackPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepthA = UMFloor(theLimit);
    this->mLeftPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.Define(theStart, theNormal);
    if (theLine.Intersects(this->mLeftPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepthB = UMFloor(theLimit);
    if (inHasEdgeA && !inHasEdgeB) {
        if (this->GetIsValidTile(inEdgeA[0].GetX() - 1, inEdgeA[0].GetY() - 1))
            return;
        theX = inEdgeA[0].GetX();
        theY = inEdgeA[0].GetY();
        theZ = this->GetZ(theX, theY);
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX + 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepthA, theY - theDepthA, 0.0f);
        theX -= 1000;
        theY -= 1000;
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX + 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepthA, theY - theDepthA, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 4, 5, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 2, ioPolygon);
        ioVertexBase += 6;
    } else if (!inHasEdgeA && inHasEdgeB) {
        if (this->GetIsValidTile(inEdgeB[1].GetX() + 1, inEdgeB[1].GetY() - 1))
            return;
        theX = inEdgeB[1].GetX();
        theY = inEdgeB[1].GetY();
        theZ = this->GetZ(theX, theY);
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX - 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthB, theY - theDepthB, 0.0f);
        theX += 1000;
        theY -= 1000;
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX - 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthB, theY - theDepthB, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 4, 5, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 2, ioPolygon);
        ioVertexBase += 6;
    } else if (inHasEdgeA && inHasEdgeB) {
        theX = inEdgeA[0].GetX();
        theY = inEdgeA[0].GetY();
        this->CreateNewEdgeVertex(theX, theY, this->GetZ(theX, theY));
        this->CreateNewEdgeVertex(theX - 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX, theY - 7, 0.0f);
        this->CreateNewEdgeVertex(theX + 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthB, theY - theDepthB, 0.0f);
        theX += theDepthA;
        theY -= theDepthA;
        this->CreateNewEdgeVertex(theX - theDepthB, theY - theDepthB, 0.0f);
        this->CreateNewEdgeVertex(theX, theY, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 2, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 2, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 2, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 2, 3, 6, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 2, 6, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 4, 6, 5, ioPolygon);
        ioVertexBase += 7;
    }
}

// FUNCTION: LLE 0x10028860
// Builds the back-right corner skirt: a lone back or right strip ending at the corner is extended 1000 tiles past it;
// with both strips visible, a seven-vertex fan joins them
void GETerrainMesh::GenerateCornerBackRightPolygons(const GEViewport& inViewport, U2DPoint<long> (&inEdgeA)[2],
                                                    bool inHasEdgeA, U2DPoint<long> (&inEdgeB)[2], bool inHasEdgeB,
                                                    GETMPolygon*& ioPolygon, unsigned long& ioVertexBase)
{
    GEVector theNormal;
    GE3DPoint theStart;
    GE3DLine theLine;
    float theDistance;
    float theLimit;
    long theDepthA;
    long theDepthB;
    long theX;
    long theY;
    float theZ;
    const GETransformation& theCamera = inViewport.GetCamera();
    theStart.SetX(theCamera.GetX());
    theStart.SetY(theCamera.GetY());
    theStart.SetZ(theCamera.GetZ());
    this->mBackPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.Define(theStart, theNormal);
    if (theLine.Intersects(this->mBackPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepthA = UMFloor(theLimit);
    this->mRightPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.Define(theStart, theNormal);
    if (theLine.Intersects(this->mRightPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepthB = UMFloor(theLimit);
    if (inHasEdgeA && !inHasEdgeB) {
        if (this->GetIsValidTile(inEdgeA[1].GetX() + 1, inEdgeA[1].GetY() + 1))
            return;
        theX = inEdgeA[1].GetX();
        theY = inEdgeA[1].GetY();
        theZ = this->GetZ(theX, theY);
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX + 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepthA, theY - theDepthA, 0.0f);
        theX += 1000;
        theY += 1000;
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX + 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepthA, theY - theDepthA, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 4, 5, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 2, ioPolygon);
        ioVertexBase += 6;
    } else if (!inHasEdgeA && inHasEdgeB) {
        if (this->GetIsValidTile(inEdgeB[1].GetX() + 1, inEdgeB[1].GetY() - 1))
            return;
        theX = inEdgeB[1].GetX();
        theY = inEdgeB[1].GetY();
        theZ = this->GetZ(theX, theY);
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX + 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepthB, theY + theDepthB, 0.0f);
        theX += 1000;
        theY -= 1000;
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX + 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepthB, theY + theDepthB, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 4, 5, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 2, ioPolygon);
        ioVertexBase += 6;
    } else if (inHasEdgeA && inHasEdgeB) {
        theX = inEdgeA[1].GetX();
        theY = inEdgeA[1].GetY();
        this->CreateNewEdgeVertex(theX, theY, this->GetZ(theX, theY));
        this->CreateNewEdgeVertex(theX + 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX + 7, theY, 0.0f);
        this->CreateNewEdgeVertex(theX + 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepthB, theY + theDepthB, 0.0f);
        theX += theDepthA;
        theY -= theDepthA;
        this->CreateNewEdgeVertex(theX + theDepthB, theY + theDepthB, 0.0f);
        this->CreateNewEdgeVertex(theX, theY, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 2, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 2, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 2, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 2, 3, 6, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 2, 6, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 4, 6, 5, ioPolygon);
        ioVertexBase += 7;
    }
}

// FUNCTION: LLE 0x10028d03
// Builds the front-left corner skirt: a lone front or left strip ending at the corner is extended 1000 tiles past it;
// with both strips visible, a seven-vertex fan joins them
void GETerrainMesh::GenerateCornerFrontLeftPolygons(const GEViewport& inViewport, U2DPoint<long> (&inEdgeA)[2],
                                                    bool inHasEdgeA, U2DPoint<long> (&inEdgeB)[2], bool inHasEdgeB,
                                                    GETMPolygon*& ioPolygon, unsigned long& ioVertexBase)
{
    GEVector theNormal;
    GE3DPoint theStart;
    GE3DLine theLine;
    float theDistance;
    float theLimit;
    long theDepthA;
    long theDepthB;
    long theX;
    long theY;
    float theZ;
    const GETransformation& theCamera = inViewport.GetCamera();
    theStart.SetX(theCamera.GetX());
    theStart.SetY(theCamera.GetY());
    theStart.SetZ(theCamera.GetZ());
    this->mFrontPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.Define(theStart, theNormal);
    if (theLine.Intersects(this->mFrontPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepthA = UMFloor(theLimit);
    this->mLeftPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.Define(theStart, theNormal);
    if (theLine.Intersects(this->mLeftPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepthB = UMFloor(theLimit);
    if (inHasEdgeA && !inHasEdgeB) {
        if (this->GetIsValidTile(inEdgeA[0].GetX() - 1, inEdgeA[0].GetY() - 1))
            return;
        theX = inEdgeA[0].GetX();
        theY = inEdgeA[0].GetY();
        theZ = this->GetZ(theX, theY);
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX - 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthA, theY + theDepthA, 0.0f);
        theX -= 1000;
        theY -= 1000;
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX - 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthA, theY + theDepthA, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 4, 5, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 2, ioPolygon);
        ioVertexBase += 6;
    } else if (!inHasEdgeA && inHasEdgeB) {
        if (this->GetIsValidTile(inEdgeB[0].GetX() - 1, inEdgeB[0].GetY() + 1))
            return;
        theX = inEdgeB[0].GetX();
        theY = inEdgeB[0].GetY();
        theZ = this->GetZ(theX, theY);
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX - 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthB, theY - theDepthB, 0.0f);
        theX -= 1000;
        theY += 1000;
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX - 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthB, theY - theDepthB, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 4, 5, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 2, ioPolygon);
        ioVertexBase += 6;
    } else if (inHasEdgeA && inHasEdgeB) {
        theX = inEdgeA[0].GetX();
        theY = inEdgeA[0].GetY();
        this->CreateNewEdgeVertex(theX, theY, this->GetZ(theX, theY));
        this->CreateNewEdgeVertex(theX - 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX - 7, theY, 0.0f);
        this->CreateNewEdgeVertex(theX - 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthA, theY + theDepthA, 0.0f);
        theX -= theDepthB;
        theY -= theDepthB;
        this->CreateNewEdgeVertex(theX - theDepthA, theY + theDepthA, 0.0f);
        this->CreateNewEdgeVertex(theX, theY, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 2, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 2, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 2, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 2, 3, 6, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 2, 6, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 4, 6, 5, ioPolygon);
        ioVertexBase += 7;
    }
}

// FUNCTION: LLE 0x10029189
// Builds the front-right corner skirt: a lone front or right strip ending at the corner is extended 1000 tiles past it;
// with both strips visible, a seven-vertex fan joins them
void GETerrainMesh::GenerateCornerFrontRightPolygons(const GEViewport& inViewport, U2DPoint<long> (&inEdgeA)[2],
                                                     bool inHasEdgeA, U2DPoint<long> (&inEdgeB)[2], bool inHasEdgeB,
                                                     GETMPolygon*& ioPolygon, unsigned long& ioVertexBase)
{
    GEVector theNormal;
    GE3DPoint theStart;
    GE3DLine theLine;
    float theDistance;
    float theLimit;
    long theDepthA;
    long theDepthB;
    long theX;
    long theY;
    float theZ;
    const GETransformation& theCamera = inViewport.GetCamera();
    theStart.SetX(theCamera.GetX());
    theStart.SetY(theCamera.GetY());
    theStart.SetZ(theCamera.GetZ());
    this->mFrontPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.Define(theStart, theNormal);
    if (theLine.Intersects(this->mFrontPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepthA = UMFloor(theLimit);
    this->mRightPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.Define(theStart, theNormal);
    if (theLine.Intersects(this->mRightPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepthB = UMFloor(theLimit);
    if (inHasEdgeA && !inHasEdgeB) {
        if (this->GetIsValidTile(inEdgeA[1].GetX() + 1, inEdgeA[1].GetY() + 1))
            return;
        theX = inEdgeA[1].GetX();
        theY = inEdgeA[1].GetY();
        theZ = this->GetZ(theX, theY);
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX - 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthA, theY + theDepthA, 0.0f);
        theX += 1000;
        theY += 1000;
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX - 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthA, theY + theDepthA, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 4, 5, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 2, ioPolygon);
        ioVertexBase += 6;
    } else if (!inHasEdgeA && inHasEdgeB) {
        if (this->GetIsValidTile(inEdgeB[0].GetX() - 1, inEdgeB[0].GetY() + 1))
            return;
        theX = inEdgeB[0].GetX();
        theY = inEdgeB[0].GetY();
        theZ = this->GetZ(theX, theY);
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX + 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepthB, theY + theDepthB, 0.0f);
        theX -= 1000;
        theY += 1000;
        this->CreateNewEdgeVertex(theX, theY, theZ);
        this->CreateNewEdgeVertex(theX + 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepthB, theY + theDepthB, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 4, 5, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 2, ioPolygon);
        ioVertexBase += 6;
    } else if (inHasEdgeA && inHasEdgeB) {
        theX = inEdgeA[1].GetX();
        theY = inEdgeA[1].GetY();
        this->CreateNewEdgeVertex(theX, theY, this->GetZ(theX, theY));
        this->CreateNewEdgeVertex(theX - 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX, theY + 7, 0.0f);
        this->CreateNewEdgeVertex(theX + 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepthA, theY + theDepthA, 0.0f);
        theX += theDepthB;
        theY += theDepthB;
        this->CreateNewEdgeVertex(theX - theDepthA, theY + theDepthA, 0.0f);
        this->CreateNewEdgeVertex(theX, theY, 0.0f);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 2, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 2, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 2, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 2, 3, 6, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 2, 6, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 4, 6, 5, ioPolygon);
        ioVertexBase += 7;
    }
}

// FUNCTION: LLE 0x10029613
// Extends the visible front edge strip while its skirt stays in view, then builds the front skirt out to the camera's
// rear clip depth (three vertices per edge point, four triangles per step)
void GETerrainMesh::GenerateFrontEdgePolygons(const GEViewport& inViewport, U2DPoint<long> (&ioEdge)[2],
                                              GETMPolygon*& ioPolygon, unsigned long& ioVertexBase)
{
    GE3DPoint theStart;
    GEVector theNormal;
    GE3DLine theLine;
    float theDistance;
    float theLimit;
    long theDepth;
    long theX;
    long theY;
    const GETransformation& theCamera = inViewport.GetCamera();
    theStart.SetX(theCamera.GetX());
    theStart.SetY(theCamera.GetY());
    theStart.SetZ(theCamera.GetZ());
    this->mFrontPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.SetStart(theStart);
    theLine.SetDirection(theNormal);
    if (theLine.Intersects(this->mFrontPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepth = UMFloor(theLimit);
    while (IsFrontEdgeVisible(inViewport, *this, ioEdge[0].GetX() - 1, ioEdge[0].GetY() - 1, theDepth)) {
        ioEdge[0].SetX(ioEdge[0].GetX() - 1);
        ioEdge[0].SetY(ioEdge[0].GetY() - 1);
    }
    if (this->GetIsValidTile(ioEdge[0].GetX() - 1, ioEdge[0].GetY() - 1)) {
        ioEdge[0].SetX(ioEdge[0].GetX() - 1);
        ioEdge[0].SetY(ioEdge[0].GetY() - 1);
    }
    while (IsFrontEdgeVisible(inViewport, *this, ioEdge[1].GetX() + 1, ioEdge[1].GetY() + 1, theDepth)) {
        ioEdge[1].SetX(ioEdge[1].GetX() + 1);
        ioEdge[1].SetY(ioEdge[1].GetY() + 1);
    }
    if (this->GetIsValidTile(ioEdge[1].GetX() + 1, ioEdge[1].GetY() + 1)) {
        ioEdge[1].SetX(ioEdge[1].GetX() + 1);
        ioEdge[1].SetY(ioEdge[1].GetY() + 1);
    }
    for (theX = ioEdge[0].GetX(), theY = ioEdge[0].GetY(); theX <= ioEdge[1].GetX(); theX++, theY++) {
        this->CreateNewEdgeVertex(theX, theY, this->GetZ(theX, theY));
        this->CreateNewEdgeVertex(theX - 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepth, theY + theDepth, 0.0f);
        if (theX == ioEdge[0].GetX())
            continue;
        this->DefineNewEdgePolygon(ioVertexBase, 0, 1, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 3, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 2, 5, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 4, ioPolygon);
        ioVertexBase += 3;
    }
    ioVertexBase += 3;
}

// FUNCTION: LLE 0x10029987
// Extends the visible left edge strip while its skirt stays in view, then builds the left skirt out to the camera's
// rear clip depth (three vertices per edge point, four triangles per step)
void GETerrainMesh::GenerateLeftEdgePolygons(const GEViewport& inViewport, U2DPoint<long> (&ioEdge)[2],
                                             GETMPolygon*& ioPolygon, unsigned long& ioVertexBase)
{
    GE3DPoint theStart;
    GEVector theNormal;
    GE3DLine theLine;
    float theDistance;
    float theLimit;
    long theDepth;
    long theX;
    long theY;
    const GETransformation& theCamera = inViewport.GetCamera();
    theStart.SetX(theCamera.GetX());
    theStart.SetY(theCamera.GetY());
    theStart.SetZ(theCamera.GetZ());
    this->mLeftPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.SetStart(theStart);
    theLine.SetDirection(theNormal);
    if (theLine.Intersects(this->mLeftPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepth = UMFloor(theLimit);
    while (IsLeftEdgeVisible(inViewport, *this, ioEdge[0].GetX() - 1, ioEdge[0].GetY() + 1, theDepth)) {
        ioEdge[0].SetX(ioEdge[0].GetX() - 1);
        ioEdge[0].SetY(ioEdge[0].GetY() + 1);
    }
    if (this->GetIsValidTile(ioEdge[0].GetX() - 1, ioEdge[0].GetY() + 1)) {
        ioEdge[0].SetX(ioEdge[0].GetX() - 1);
        ioEdge[0].SetY(ioEdge[0].GetY() + 1);
    }
    while (IsLeftEdgeVisible(inViewport, *this, ioEdge[1].GetX() + 1, ioEdge[1].GetY() - 1, theDepth)) {
        ioEdge[1].SetX(ioEdge[1].GetX() + 1);
        ioEdge[1].SetY(ioEdge[1].GetY() - 1);
    }
    if (this->GetIsValidTile(ioEdge[1].GetX() + 1, ioEdge[1].GetY() - 1)) {
        ioEdge[1].SetX(ioEdge[1].GetX() + 1);
        ioEdge[1].SetY(ioEdge[1].GetY() - 1);
    }
    for (theX = ioEdge[0].GetX(), theY = ioEdge[0].GetY(); theX <= ioEdge[1].GetX(); theX++, theY--) {
        this->CreateNewEdgeVertex(theX, theY, this->GetZ(theX, theY));
        this->CreateNewEdgeVertex(theX - 5, theY - 5, 0.0f);
        this->CreateNewEdgeVertex(theX - theDepth, theY - theDepth, 0.0f);
        if (theX == ioEdge[0].GetX())
            continue;
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 1, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 3, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 2, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 4, 5, ioPolygon);
        ioVertexBase += 3;
    }
    ioVertexBase += 3;
}

// FUNCTION: LLE 0x10029ce9
// Extends the visible right edge strip while its skirt stays in view, then builds the right skirt out to the camera's
// rear clip depth (three vertices per edge point, four triangles per step)
void GETerrainMesh::GenerateRightEdgePolygons(const GEViewport& inViewport, U2DPoint<long> (&ioEdge)[2],
                                              GETMPolygon*& ioPolygon, unsigned long& ioVertexBase)
{
    GE3DPoint theStart;
    GEVector theNormal;
    GE3DLine theLine;
    float theDistance;
    float theLimit;
    long theDepth;
    long theX;
    long theY;
    const GETransformation& theCamera = inViewport.GetCamera();
    theStart.SetX(theCamera.GetX());
    theStart.SetY(theCamera.GetY());
    theStart.SetZ(theCamera.GetZ());
    this->mRightPlane.GetNormal().Scale(-1.0f, theNormal);
    theLine.SetStart(theStart);
    theLine.SetDirection(theNormal);
    if (theLine.Intersects(this->mRightPlane, theDistance) && theDistance > 0.0f)
        theLimit = inViewport.GetRearClippingDistance() - theDistance + 1.0f;
    else
        theLimit = inViewport.GetRearClippingDistance();
    theDepth = UMFloor(theLimit);
    while (IsRightEdgeVisible(inViewport, *this, ioEdge[0].GetX() - 1, ioEdge[0].GetY() + 1, theDepth)) {
        ioEdge[0].SetX(ioEdge[0].GetX() - 1);
        ioEdge[0].SetY(ioEdge[0].GetY() + 1);
    }
    if (this->GetIsValidTile(ioEdge[0].GetX() - 1, ioEdge[0].GetY() + 1)) {
        ioEdge[0].SetX(ioEdge[0].GetX() - 1);
        ioEdge[0].SetY(ioEdge[0].GetY() + 1);
    }
    while (IsRightEdgeVisible(inViewport, *this, ioEdge[1].GetX() + 1, ioEdge[1].GetY() - 1, theDepth)) {
        ioEdge[1].SetX(ioEdge[1].GetX() + 1);
        ioEdge[1].SetY(ioEdge[1].GetY() - 1);
    }
    if (this->GetIsValidTile(ioEdge[1].GetX() + 1, ioEdge[1].GetY() - 1)) {
        ioEdge[1].SetX(ioEdge[1].GetX() + 1);
        ioEdge[1].SetY(ioEdge[1].GetY() - 1);
    }
    for (theX = ioEdge[0].GetX(), theY = ioEdge[0].GetY(); theX <= ioEdge[1].GetX(); theX++, theY--) {
        this->CreateNewEdgeVertex(theX, theY, this->GetZ(theX, theY));
        this->CreateNewEdgeVertex(theX + 5, theY + 5, 0.0f);
        this->CreateNewEdgeVertex(theX + theDepth, theY + theDepth, 0.0f);
        if (theX == ioEdge[0].GetX())
            continue;
        this->DefineNewEdgePolygon(ioVertexBase, 0, 1, 4, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 0, 4, 3, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 2, 5, ioPolygon);
        this->DefineNewEdgePolygon(ioVertexBase, 1, 5, 4, ioPolygon);
        ioVertexBase += 3;
    }
    ioVertexBase += 3;
}

// FUNCTION: LLE 0x1002a05d
// Returns the terrain normal at a map position, interpolated bilinearly from the four surrounding points' normals
void GETerrainMesh::GetMeshNormal(float inX, float inY, GEVector& outNormal) const
{
    unsigned long theX = UMFloorUnsigned(inX);
    unsigned long theY = UMFloorUnsigned(inY);
    float theFractionX = inX - theX;
    float theFractionY = inY - theY;
    GETMPointRow* theRow = &this->mPointRows[theY];
    GETMPointRow* theNextRow = theRow + 1;
    GEVector* the00 = theRow->mPoints[theX - theRow->mStart].mNormal;
    GEVector* the10 = theRow->mPoints[theX - theRow->mStart + 1].mNormal;
    GEVector* the01 = theNextRow->mPoints[theX - theNextRow->mStart].mNormal;
    GEVector* the11 = theNextRow->mPoints[theX - theNextRow->mStart + 1].mNormal;
    float theInverseX = 1.0f - theFractionX;
    GEVector theBottom(the00->GetX() * theInverseX + the10->GetX() * theFractionX,
                       the00->GetY() * theInverseX + the10->GetY() * theFractionX,
                       the00->GetZ() * theInverseX + the10->GetZ() * theFractionX);
    GEVector theTop(the01->GetX() * theInverseX + the11->GetX() * theFractionX,
                    the01->GetY() * theInverseX + the11->GetY() * theFractionX,
                    the01->GetZ() * theInverseX + the11->GetZ() * theFractionX);
    outNormal.SetX(theBottom.GetX() * (1.0f - theFractionY) + theTop.GetX() * theFractionY);
    outNormal.SetY(theBottom.GetY() * (1.0f - theFractionY) + theTop.GetY() * theFractionY);
    outNormal.SetZ(theBottom.GetZ() * (1.0f - theFractionY) + theTop.GetZ() * theFractionY);
}

// FUNCTION: LLE 0x1002a185
// Returns the terrain height at a map position, interpolated over the half of its tile (split along the 00-11 diagonal)
float GETerrainMesh::GetMeshZ(float inX, float inY) const
{
    unsigned long theX = UMFloorUnsigned(inX);
    unsigned long theY = UMFloorUnsigned(inY);
    float theFractionX = inX - theX;
    float theFractionY = inY - theY;
    GETMPointRow* theRow = &this->mPointRows[theY];
    float theInverseX = 1.0f - theFractionX;
    float the00 = theRow->mPoints[theX - theRow->mStart].mZ;
    float the10 = theRow->mPoints[theX - theRow->mStart + 1].mZ;
    theRow++;
    float the11 = theRow->mPoints[theX - theRow->mStart + 1].mZ;
    float theA = the00 * theInverseX;
    float theB = the11 * theFractionX;
    float theDiagonal = theB + theA;
    float theT;
    if (theFractionY >= theFractionX) {
        theT = (theFractionY - theFractionX) / theInverseX;
        return (theRow->mPoints[theX - theRow->mStart].mZ * theInverseX + theB) * theT +
               (1.0f - theT) * theDiagonal;
    }
    theT = theFractionY / theFractionX;
    return (the10 * theFractionX + theA) * (1.0f - theT) + theT * theDiagonal;
}

// FUNCTION: LLE 0x1002a277
// Returns the map tile where the line segment crosses a map-edge plane onto the map; else the tile closest to where
// the line meets height 0 (or to its start)
void GETerrainMesh::GetClosestTile(const GE3DLine& inLine, long& outX, long& outY) const
{
    GE3DPoint thePoint;
    float theT;
    bool theFound = false;
    if (inLine.Intersects(this->mLeftPlane, theT) && theT >= 0.0f && theT <= 1.0f) {
        inLine.GetPointAt(theT, thePoint);
        theFound = this->GetIsValidTile(UMFloorUnsigned(thePoint.GetX()), UMFloorUnsigned(thePoint.GetY()));
    } else if (inLine.Intersects(this->mRightPlane, theT) && theT >= 0.0f && theT <= 1.0f) {
        inLine.GetPointAt(theT, thePoint);
        theFound = this->GetIsValidTile(UMFloorUnsigned(thePoint.GetX()), UMFloorUnsigned(thePoint.GetY()));
    }
    if (inLine.Intersects(this->mFrontPlane, theT) && theT >= 0.0f && theT <= 1.0f && !theFound) {
        inLine.GetPointAt(theT, thePoint);
        theFound = this->GetIsValidTile(UMFloorUnsigned(thePoint.GetX()), UMFloorUnsigned(thePoint.GetY()));
    } else if (inLine.Intersects(this->mBackPlane, theT) && theT >= 0.0f && theT <= 1.0f && !theFound) {
        inLine.GetPointAt(theT, thePoint);
        theFound = this->GetIsValidTile(UMFloorUnsigned(thePoint.GetX()), UMFloorUnsigned(thePoint.GetY()));
    }
    if (theFound) {
        outX = UMFloor(thePoint.GetX());
        outY = UMFloor(thePoint.GetY());
    } else {
        if (!inLine.SolveForZ(0.0f, theT, thePoint))
            thePoint = inLine.GetStart();
        this->GetClosestTile(thePoint, outX, outY);
    }
}


// FUNCTION: LLE 0x1002a5af
// Returns the tile under a point, first moving a point outside the map onto the nearest edge planes, just inside
void GETerrainMesh::GetClosestTile(const GE3DPoint& inPoint, long& outX, long& outY) const
{
    GE3DPoint thePoint = inPoint;
    GE3DLine theLine;
    float theT;
    if (this->mLeftPlane.IsPointOnOrBelowPlane(thePoint)) {
        theLine.Define(thePoint, this->mLeftPlane.GetNormal());
        theLine.Intersects(this->mLeftPlane, theT);
        theLine.GetPointAt(theT, thePoint);
        thePoint.SetX(thePoint.GetX() + 0.001f);
        thePoint.SetY(thePoint.GetY() + 0.001f);
    } else if (this->mRightPlane.IsPointOnOrBelowPlane(thePoint)) {
        theLine.Define(thePoint, this->mRightPlane.GetNormal());
        theLine.Intersects(this->mRightPlane, theT);
        theLine.GetPointAt(theT, thePoint);
        thePoint.SetX(thePoint.GetX() - 0.001f);
        thePoint.SetY(thePoint.GetY() - 0.001f);
    }
    if (this->mFrontPlane.IsPointOnOrBelowPlane(thePoint)) {
        theLine.Define(thePoint, this->mFrontPlane.GetNormal());
        theLine.Intersects(this->mFrontPlane, theT);
        theLine.GetPointAt(theT, thePoint);
        thePoint.SetX(thePoint.GetX() + 0.001f);
        thePoint.SetY(thePoint.GetY() - 0.001f);
    } else if (this->mBackPlane.IsPointOnOrBelowPlane(thePoint)) {
        theLine.Define(thePoint, this->mBackPlane.GetNormal());
        theLine.Intersects(this->mBackPlane, theT);
        theLine.GetPointAt(theT, thePoint);
        thePoint.SetX(thePoint.GetX() - 0.001f);
        thePoint.SetY(thePoint.GetY() + 0.001f);
    }
    EE_LOST_DEBUG(8);  // LOST-DEBUG: none independent (exact only); smallest n (1..7 not exact; start/end positions need >= 8)
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// FUNCTION: LLE 0x1002a7aa
// Defines the four vertical map-edge planes (front, back, left, right) from the first and last tile rows
void GETerrainMesh::InitializePlanes()
{
    EE_LOST_DEBUG(11);  // LOST-DEBUG: none independent (owner, 2026-10-25: exact is exact); smallest n
    GE3DPoint thePoint;
    GEVector theNormal;
    thePoint.SetX((float)this->mTileRows[this->mTileRowCount - 1].mStart + 1.0f);
    thePoint.SetY((float)this->mTileRowCount);
    theNormal.SetY(-1.0f);
    theNormal.SetX(1.0f);
    theNormal.Normalize();
    this->mFrontPlane.Define(thePoint, theNormal);
    thePoint.SetX((float)this->mTileRows[0].mStart + 1.0f);
    thePoint.SetY(0.0f);
    theNormal.SetX(-1.0f);
    theNormal.SetY(1.0f);
    theNormal.Normalize();
    this->mBackPlane.Define(thePoint, theNormal);
    thePoint.SetX((float)this->mTileRows[0].mStart + 1.0f);
    thePoint.SetY(0.0f);
    theNormal.SetX(1.0f);
    theNormal.SetY(1.0f);
    theNormal.Normalize();
    this->mLeftPlane.Define(thePoint, theNormal);
    thePoint.SetX((float)this->mTileRows[this->mTileRowCount - 1].mStart + 1.0f);
    thePoint.SetY((float)this->mTileRowCount);
    theNormal.SetX(-1.0f);
    theNormal.SetY(-1.0f);
    theNormal.Normalize();
    this->mRightPlane.Define(thePoint, theNormal);
}


// FUNCTION: LLE 0x1002a942
// Walks the tiles under the line from its start towards its end; returns whether it hits the terrain within its length
// (outDistance = the line parameter of the first tile hit)
bool GETerrainMesh::Intersects(const GE3DLine& inLine, float& outDistance) const
{
    GE3DLine theEdge(GE3DPoint(0.0f, 0.0f, 0.0f), GEVector(0.0f, 1.0f, 0.0f));
    GE3DPoint theStart;
    GE3DPoint theEnd;
    bool theResult = false;
    long theStartX;
    long theStartY;
    long theEndX;
    long theEndY;
    long theStepX;
    long theStepY;
    float theOffset;
    long theX;
    long theY;
    long theDX;
    long theDY;
    float theT;
    float theEdgeT;
    theStart = inLine.GetStart();
    theStartX = UMFloor(theStart.GetX());
    theStartY = UMFloor(theStart.GetY());
    inLine.GetPointAt(1.0f, theEnd);
    theEndX = UMFloor(theEnd.GetX());
    theEndY = UMFloor(theEnd.GetY());
    theEndX -= theStartX;
    if (theEndX < 0)
        theEndX = -theEndX;
    theEndY -= theStartY;
    if (theEndY < 0)
        theEndY = -theEndY;
    if (inLine.GetDirection().GetX() >= 0.0f) {
        theStepX = 1;
        theOffset = 1.0f;
    } else {
        theStepX = -1;
        theOffset = 0.0f;
    }
    if (inLine.GetDirection().GetY() >= 0.0f)
        theStepY = 1;
    else
        theStepY = -1;
    theX = theStartX;
    theY = theStartY;
    while (!this->DoesLineIntersectTile(inLine, theX, theY, outDistance)) {
        theDX = theX - theStartX;
        if (theDX < 0)
            theDX = -theDX;
        theDY = theY - theStartY;
        if (theDY < 0)
            theDY = -theDY;
        if (theDX > theEndX || theDY > theEndY)
            return theResult;
        theStart.SetX((float)theX + theOffset);
        theStart.SetY((float)theY);
        theStart.SetZ(0.0f);
        theEdge.SetStart(theStart);
        if (inLine.IntersectsIn2D(theEdge, theT, theEdgeT) && theEdgeT >= -0.00001f &&
            theEdgeT <= 1.0001f)
            theX += theStepX;
        else
            theY += theStepY;
    }
    if (outDistance <= 1.0f)
        theResult = true;
    return theResult;
}

// FUNCTION: LLE 0x1002ab5d
// Adds the near and far world points (perspective or orthographic) under a normalized screen point to outPoints in 2D;
// leaves inPoint's z at 1
void GETerrainMesh::AddDenormalizedPointToList(const GEViewport& inViewport, GE3DPoint& inPoint,
                                               std::vector<U2DPoint<float> >& outPoints, bool inOrthographic)
{
    GE3DPoint theNear;
    GE3DPoint theFar;
    U2DPoint<float> thePoint;
    inPoint.SetZ(0.0f);
    if (!inOrthographic) {
        inViewport.DenormalizePerspectivePoint(inPoint, inViewport.GetFrontClippingDistance(), theNear);
        inPoint.SetZ(1.0f);
        inViewport.DenormalizePerspectivePoint(inPoint, inViewport.GetRearClippingDistance(), theFar);
    } else {
        inViewport.DenormalizeOrthographicPoint(inPoint, theNear);
        inPoint.SetZ(1.0f);
        inViewport.DenormalizeOrthographicPoint(inPoint, theFar);
    }
    thePoint.SetXY(theNear.GetX(), theNear.GetY());
    outPoints.push_back(thePoint);
    thePoint.SetXY(theFar.GetX(), theFar.GetY());
    outPoints.push_back(thePoint);
}

// FUNCTION: LLE 0x1002ac2d
// Lists the tile rows (first and last column, clipped to the map) covered by the convex hull of the volume's points,
// and returns their bounding box
void GETerrainMesh::GetTileRowsInVolume(const std::vector<GE3DPoint>& inVolume,
                                        std::vector<std::pair<long, std::pair<long, long> > >& outRows,
                                        U2DPoint<long>& outMinimum, U2DPoint<long>& outMaximum)
{
    std::map<long, std::pair<long, long> > theRows;
    U2DConvexHull<float> theHull;
    std::vector<U2DPoint<float> > thePoints;
    long theRowCount = this->mTileRowCount;
    U2DPoint<float> thePoint;
    long theMinimumX = 0x7fffffff;
    long theMinimumY = 0x7fffffff;
    std::map<long, std::pair<long, long> >::iterator theIterator;
    long theMaximumX = 0x80000000;
    long theMaximumY = 0x80000000;
    unsigned long i;
    long theY;
    long theFirst;
    long theLast;
    long theRowStart;
    long theRowEnd;
    for (i = 0; i < inVolume.size(); i++) {
        thePoint.mX = inVolume[i].GetX();
        EE_LOST_DEBUG(5);  // LOST-DEBUG: none independent (exact only); smallest n (1..4 not exact)
        thePoint.mY = inVolume[i].GetY();
        thePoints.push_back(thePoint);
    }
    theHull.SetPoints(thePoints, false);
    theHull.GetScanLines(theRows);
    for (theIterator = theRows.begin(); theIterator != theRows.end() && theIterator->first < theRowCount;
         theIterator++) {
        theY = theIterator->first;
        if (theY < 0)
            continue;
        theFirst = theIterator->second.first;
        theLast = theIterator->second.second;
        theRowStart = this->mTileRows[theY].mStart;
        if (theLast < theRowStart)
            continue;
        theRowEnd = theRowStart + this->mTileRows[theY].mCount - 1;
        if (theFirst > theRowEnd)
            continue;
        theFirst = max(theFirst, theRowStart);
        theLast = min(theLast, theRowEnd);
        outRows.push_back(std::make_pair(theY, std::make_pair(theFirst, theLast)));
        theMinimumX = min(theMinimumX, theFirst);
        theMaximumX = max(theMaximumX, theLast);
        theMinimumY = min(theMinimumY, theY);
        theMaximumY = max(theMaximumY, theY);
    }
    outMinimum.mX = theMinimumX;
    outMinimum.mY = theMinimumY;
    outMaximum.mX = theMaximumX;
    outMaximum.mY = theMaximumY;
}

// FUNCTION: LLE 0x1002ae1f
// Registers a colour set and returns its id in outID; always returns 0
long GETerrainMesh::RegisterColorSet(std::vector<unsigned long>& inColorSet, unsigned long& outID)
{
    outID = this->mRegisteredColorSets.size();
    this->mRegisteredColorSets.push_back(inColorSet);
    return 0;
}

// FUNCTION: LLE 0x1002ae49
// Registers a texture set (the mesh deletes it later) and returns its id in outID; always returns 0
long GETerrainMesh::RegisterTextureSet(GETextureSet* inTextureSet, unsigned long& outID)
{
    outID = this->mRegisteredTextureSets.size();
    this->mRegisteredTextureSets.push_back(inTextureSet);
    return 0;
}

// FUNCTION: LLE 0x1002ae74
// Sets a map point's colour table index
void GETerrainMesh::SetColorFromColorTableIndex(unsigned long inX, unsigned long inY, unsigned char inIndex)
{
    this->mPointRows[inY].mPoints[inX - this->mPointRows[inY].mStart].mColorTableIndex = inIndex;
}

// FUNCTION: LLE 0x1002ae9b
// Sets a map point's colour from the colour set, picked by the fractal noise at the point
void GETerrainMesh::SetColorSet(unsigned long inX, unsigned long inY, unsigned long inColorSetID)
{
    GETMPointRow& theRow = this->mPointRows[inY];
    unsigned long theCount = this->mRegisteredColorSets[inColorSetID].size();
    long theIndex = UMFloatToLong(this->mFractal->GetValue(inX & 63, inY & 63) * ((float)theCount - 1.0f));
    theRow.mPoints[inX - theRow.mStart].mColorTableIndex =
        (unsigned char)this->mRegisteredColorSets[inColorSetID][theIndex];
}

// FUNCTION: LLE 0x1002af33
// Sets a map point's colour from the water colour set by its scaled depth, clamped to the last entry
void GETerrainMesh::SetWaterColorSet(unsigned long inX, unsigned long inY)
{
    GETMPointRow& theRow = this->mPointRows[inY];
    unsigned long theSize = this->mRegisteredColorSets[this->mWaterColorSetID].size();
    float theHeight = this->mWaterTextureScale * theRow.mPoints[inX - theRow.mStart].mZ;
    unsigned long theIndex = (unsigned long)(theHeight * (theSize - 1));
    if (theIndex >= this->mRegisteredColorSets[this->mWaterColorSetID].size())
        theIndex = this->mRegisteredColorSets[this->mWaterColorSetID].size() - 1;
    theRow.mPoints[inX - theRow.mStart].mColorTableIndex = (unsigned char)this->mRegisteredColorSets[this->mWaterColorSetID][theIndex];
}

// FUNCTION: LLE 0x1002afc9
// Copies up to 256 colours into the colour table and fills the rest with white
void GETerrainMesh::SetColorTable(std::vector<GEColor>& inColorTable)
{
    unsigned long theCount = min(256, inColorTable.size());
    unsigned long i;
    for (i = 0; i < theCount; i++)
        this->mColorTable[i] = inColorTable[i];
    for (; i < 256; i++)
        this->mColorTable[i] = GEColor(1.0f, 1.0f, 1.0f);
}

// FUNCTION: LLE 0x1002b079
// Sets the water texture set, colour set, animation period and texture scale; rebuilds the animation materials,
// randomises the 33x33 water texture grid and recolours every underwater point
void GETerrainMesh::SetGlobalWaterState(unsigned long inWaterSetID, unsigned long inColorSetID,
                                        unsigned long inAnimationPeriod, float inTextureScale)
{
    URandom theRandom;
    GETMMaterial theMaterial;
    GETextureSet* theTextureSet;
    unsigned long theWidth;
    unsigned long theHeight;
    unsigned long i;
    unsigned long j;
    long theCount;
    long theIndex;
    this->mGlobalWaterSetID = inWaterSetID;
    this->mWaterColorSetID = inColorSetID;
    this->mWaterTextureScale = 1.0f / inTextureScale;
    this->mWaterAnimationPeriod = inAnimationPeriod;
    theTextureSet = this->mRegisteredTextureSets[inWaterSetID];
    theWidth = theTextureSet->GetNumTexturesInWidth();
    theHeight = theTextureSet->GetNumTexturesInHeight();
    this->mWaterAnimationMaterialList.clear();
    for (i = 0; i < theWidth; i++) {
        for (j = 0; j < theHeight; j++) {
            theMaterial.mTexture = theTextureSet->GetTexture(i, j);
            this->mWaterAnimationMaterialList.push_back(theMaterial);
        }
    }
    theCount = theHeight * theWidth - 1;
    for (i = 0; i < 33; i++) {
        for (j = 0; j < 33; j++) {
            theIndex = UMFloatToLong(theRandom.Generate() * (float)(theCount + 1));
            if (theIndex == theCount + 1)
                theIndex = theCount;
            this->mWaterTextureIndices[i][j] = theIndex;
        }
    }
    for (j = 0; j < this->mPointRowCount; j++) {
        for (i = 0; i < this->mPointRows[j].mCount; i++) {
            if (this->mPointRows[j].mPoints[i].mZ < 0.0f)
                this->SetWaterColorSet(this->mPointRows[j].mStart + i, j);
        }
    }
}

// FUNCTION: LLE 0x1002b232
// Returns whether the line hits either triangle of a map tile, with outDistance the line parameter of the hit;
// false off the map
bool GETerrainMesh::DoesLineIntersectTile(const GE3DLine& inLine, unsigned long inX, unsigned long inY,
                                          float& outDistance) const
{
    GE3DPlane thePlane;
    GEVector theEdgeA;
    GEVector theEdgeB;
    GEVector theNormal;
    GE3DPoint thePoint;
    GE3DPoint theHit;
    GE3DPoint theQ;
    GE3DPoint theR;
    GEVector theE1;
    GEVector theE2;
    bool theResult = false;
    GETMPointRow& theRow = this->mPointRows[inY];
    float theX = (float)inX;
    float theY = (float)inY;
    float theNextX;
    float theNextY;
    float theDeterminant;
    float theInverse;
    float theU;
    unsigned long i;
    if (!this->GetIsValidTile(inX, inY))
        return theResult;
    thePoint.SetX(theX);
    thePoint.SetY(theY);
    thePoint.SetZ(theRow.mPoints[inX - theRow.mStart].mZ);
    theQ.SetX(theX);
    theNextY = theY + 1.0f;
    theQ.SetY(theNextY);
    theQ.SetZ(this->mPointRows[inY + 1].mPoints[inX - this->mPointRows[inY + 1].mStart].mZ);
    theNextX = theX + 1.0f;
    theR.SetX(theNextX);
    theR.SetY(theY);
    theR.SetZ(theRow.mPoints[inX - theRow.mStart + 1].mZ);
    for (i = 0; i < 2; i++) {
        theEdgeA.SetX(thePoint.GetX() - theQ.GetX());
        theEdgeA.SetY(thePoint.GetY() - theQ.GetY());
        theEdgeA.SetZ(thePoint.GetZ() - theQ.GetZ());
        theEdgeB.SetX(theQ.GetX() - theR.GetX());
        theEdgeB.SetY(theQ.GetY() - theR.GetY());
        theEdgeB.SetZ(theQ.GetZ() - theR.GetZ());
        theEdgeB.CrossProduct(theEdgeA, theNormal);
        thePlane.Define(thePoint, theNormal);
        theResult = inLine.Intersects(thePlane, outDistance);
        if (theResult) {
            inLine.GetPointAt(outDistance, theHit);
            theE1.SetX(theR.GetX() - theQ.GetX());
            theE1.SetY(theR.GetY() - theQ.GetY());
            theDeterminant = theE1.GetY() * (theHit.GetX() - thePoint.GetX()) -
                             (theHit.GetY() - thePoint.GetY()) * theE1.GetX();
            if (theDeterminant == 0.0f) {
                if (thePoint.GetX() != theHit.GetX() || thePoint.GetY() != theHit.GetY() ||
                    thePoint.GetZ() != theHit.GetZ())
                    theResult = false;
                if (theResult)
                    return theResult;
            } else {
                theInverse = 1.0f / theDeterminant;
                theE2.SetX(thePoint.GetX() - theQ.GetX());
                theE2.SetY(thePoint.GetY() - theQ.GetY());
                theU = (theE2.GetY() * (theHit.GetX() - thePoint.GetX()) - (theHit.GetY() - thePoint.GetY()) * theE2.GetX()) *
                       theInverse;
                if ((theE2.GetY() * theE1.GetX() - theE1.GetY() * theE2.GetX()) * theInverse >= 0.99999f &&
                    theU >= -0.00001f && theU <= 1.0001f)
                    return true;
                theResult = false;
            }
        }
        thePoint.SetY(theNextY);
        thePoint.SetZ(theQ.GetZ());
        theQ.SetX(theNextX);
        theQ.SetY(theNextY);
        theQ.SetZ(this->mPointRows[inY + 1].mPoints[inX - this->mPointRows[inY + 1].mStart + 1].mZ);
    }
    return theResult;
}


// FUNCTION: LLE 0x1002b507
// Sets each visible point's fog visibility from its four tiles' viewer counts (0.125 for one, 0.25 for more; 1 when
// inAllVisible) and recomputes the visible water and space shares
void GETerrainMesh::UpdateFogOfWar(bool inAllVisible)
{
    float theWaterTotal = 0.0f;
    float theSpaceTotal = 0.0f;
    unsigned long theEndX = this->mVisibleTileWidth + 2 + this->mVisibleBottomLeftX;
    unsigned long theEndY = this->mVisibleTileHeight + 2 + this->mVisibleBottomLeftY;
    unsigned long theX;
    unsigned long theY;
    unsigned long i;
    unsigned long j;
    float theValue;
    long theCount;
    GETMTile* theRow;
    GETMTile* thePreviousRow;
    if (this->mVisibleTileRows.size() == 0)
        return;
    if (!inAllVisible) {
        for (theY = this->mVisibleBottomLeftY, j = 0; theY < theEndY; theY++, j++) {
            thePreviousRow = this->mTileRows[theY - 1].mTiles - this->mTileRows[theY - 1].mStart;
            theRow = this->mTileRows[theY].mTiles - this->mTileRows[theY].mStart;
            for (theX = this->mVisibleBottomLeftX, i = 0; theX < theEndX; theX++, i++) {
                if (!this->mTerrainPointVisibility[i][j])
                    continue;
                if (this->GetIsValidTile(theX - 1, theY)) {
                    theCount = theRow[theX - 1].mReferenceCount;
                    if (theCount > 1)
                        theValue = 0.25f;
                    else if (theCount == 1)
                        theValue = 0.125f;
                    else
                        theValue = 0.0f;
                } else
                    theValue = 0.0f;
                if (this->GetIsValidTile(theX, theY)) {
                    theCount = theRow[theX].mReferenceCount;
                    if (theCount > 1)
                        theValue += 0.25f;
                    else if (theCount == 1)
                        theValue += 0.125f;
                }
                if (this->GetIsValidTile(theX - 1, theY - 1)) {
                    theCount = thePreviousRow[theX - 1].mReferenceCount;
                    if (theCount > 1)
                        theValue += 0.25f;
                    else if (theCount == 1)
                        theValue += 0.125f;
                }
                if (this->GetIsValidTile(theX, theY - 1)) {
                    theCount = thePreviousRow[theX].mReferenceCount;
                    if (theCount > 1)
                        theValue += 0.25f;
                    else if (theCount == 1)
                        theValue += 0.125f;
                }
                this->mTerrainRenderVertices[i][j].mVisibility = theValue;
                if (this->mWaterPointVisibility[i][j]) {
                    this->mWaterRenderVertices[i][j].mVisibility = theValue;
                    theWaterTotal += theValue;
                }
                if (this->GetIsSpaceTile(theX, theY))
                    theSpaceTotal += theValue;
            }
        }
    } else {
        for (theY = this->mVisibleBottomLeftY, j = 0; theY < theEndY; theY++, j++) {
            for (theX = this->mVisibleBottomLeftX, i = 0; theX < theEndX; theX++, i++) {
                if (!this->mTerrainPointVisibility[i][j])
                    continue;
                this->mTerrainRenderVertices[i][j].mVisibility = 1.0f;
                if (this->mWaterPointVisibility[i][j]) {
                    this->mWaterRenderVertices[i][j].mVisibility = 1.0f;
                    theWaterTotal += 1.0f;
                }
                if (this->GetIsSpaceTile(theX, theY))
                    theSpaceTotal += 1.0f;
            }
        }
    }
    if (this->mNumVisibleTiles != 0) {
        this->mVisibleWaterTilePercentage = theWaterTotal / (float)this->mNumVisibleTiles;
        this->mVisibleSpaceTilePercentage = theSpaceTotal / (float)this->mNumVisibleTiles;
    } else {
        this->mVisibleWaterTilePercentage = 0.0f;
        this->mVisibleSpaceTilePercentage = 0.0f;
    }
}

static unsigned long sWaterElapsedTime;   // 0x1010fed0
static unsigned long sWaterLastTime;      // 0x1010fed4

// FUNCTION: LLE 0x1002b83f
// Advances the water animation by the whole periods elapsed since the last step and points every water material at
// its new frame
void GETerrainMesh::UpdateWaterAnimation()
{
    unsigned long theFrameCount = this->mWaterAnimationMaterialList.size();
    GETextureSet* theTextureSet = this->mRegisteredTextureSets[this->mGlobalWaterSetID];
    unsigned long theFrames;
    unsigned long i;
    GETexture* theTexture;
    sWaterElapsedTime = TMGetTime() - sWaterLastTime;
    if (!(sWaterElapsedTime >= this->mWaterAnimationPeriod))
        return;
    theFrames = sWaterElapsedTime / this->mWaterAnimationPeriod;
    this->mWaterAnimationFrame = (this->mWaterAnimationFrame + theFrames) % theFrameCount;
    sWaterLastTime += this->mWaterAnimationPeriod * theFrames;
    for (i = 0; i < theFrameCount; i++) {
        theTexture = theTextureSet->GetTexture((this->mWaterAnimationFrame + i) % theFrameCount, 1);
        this->mWaterMaterialList[i].mTexture = theTexture;
        this->mWaterAnimationMaterialList[i].mTexture = theTexture;
    }
}

// Per-cell height jitter of the cliff skirts' lower edge (0x100d9760) (provisional name).
static float sCliffNoise[16][16] = {
    { -0.997f, 0.127f, -0.613f, 0.617f, 0.17f, -0.04f, -0.299f, 0.792f, 0.646f, 0.493f, -0.652f, 0.718f, 0.421f, 0.027f, -0.392f, -0.97f },
    { -0.817f, -0.271f, -0.705f, -0.668f, 0.977f, -0.109f, -0.762f, -0.991f, -0.982f, -0.244f, 0.063f, 0.142f, 0.204f, 0.214f, -0.668f, 0.326f },
    { -0.098f, -0.296f, -0.886f, 0.215f, 0.567f, 0.605f, 0.04f, -0.396f, 0.752f, 0.453f, 0.912f, 0.851f, 0.079f, -0.715f, -0.076f, -0.529f },
    { 0.724f, -0.581f, 0.559f, 0.687f, 0.994f, 0.999f, 0.223f, -0.215f, -0.468f, -0.405f, 0.68f, -0.953f, -0.248f, -0.815f, 0.354f, -0.888f },
    { -0.982f, 0.838f, -0.448f, -0.454f, 0.176f, 0.382f, 0.675f, 0.453f, -0.03f, -0.589f, 0.487f, -0.063f, -0.084f, 0.898f, 0.489f, -0.783f },
    { 0.198f, -0.23f, 0.47f, 0.218f, 0.145f, -0.277f, -0.697f, -0.55f, -0.15f, 0.606f, 0.034f, 0.98f, 0.503f, -0.309f, -0.662f, 0.315f },
    { -0.016f, -0.873f, 0.4f, 0.01f, -0.705f, 0.899f, -0.717f, 0.81f, 0.386f, -0.394f, -0.147f, -0.859f, 0.933f, 0.366f, -0.694f, 0.755f },
    { 0.643f, 0.164f, -0.617f, -0.644f, 0.634f, -0.049f, -0.689f, 0.008f, 0.464f, -0.189f, -0.441f, 0.137f, 0.364f, 0.512f, 0.444f, -0.049f },
    { -0.754f, -0.264f, 0.669f, -0.93f, 0.034f, 0.326f, -0.148f, -0.791f, 0.899f, 0.843f, 0.099f, -0.308f, -0.057f, -0.25f, 0.694f, -0.366f },
    { -0.088f, -0.456f, 0.966f, -0.404f, 0.478f, 0.135f, -0.608f, 0.523f, 0.679f, -0.205f, 0.002f, 0.78f, -0.945f, 0.989f, 0.145f, -0.899f },
    { 0.063f, -0.612f, 0.686f, 0.254f, 0.315f, -0.604f, 0.684f, -0.753f, -0.78f, 0.486f, -0.372f, 0.882f, -0.428f, -0.327f, -0.719f, 0.466f },
    { 0.669f, 0.416f, 0.2f, 0.494f, -0.495f, -0.711f, -0.997f, -0.878f, 0.612f, 0.705f, -0.579f, -0.769f, 0.106f, -0.971f, -0.772f, -0.091f },
    { 0.504f, 0.372f, 0.087f, -0.852f, -0.127f, -0.596f, 0.392f, -0.419f, -0.127f, -0.535f, 0.156f, 0.065f, 0.257f, -0.68f, 0.008f, 0.926f },
    { 0.392f, 0.85f, -0.62f, -0.328f, -0.643f, 0.99f, -0.085f, 0.996f, -0.805f, 0.25f, -0.811f, -0.125f, 0.863f, -0.903f, 0.789f, -0.42f },
    { -0.545f, 0.538f, -0.179f, -0.596f, 0.256f, 0.208f, -0.097f, -0.067f, 0.196f, 0.269f, 0.71f, 0.658f, 0.25f, 0.442f, 0.132f, -0.25f },
    { -0.631f, 0.476f, 0.11f, 0.81f, -0.514f, -0.622f, 0.209f, 0.397f, 0.169f, -0.297f, -0.011f, -0.839f, 0.481f, 0.224f, 0.241f, 0.382f },
};

// FUNCTION: LLE 0x1002b8e8
// Adds a two-triangle cliff wall 8 units deep (noisy lower edge) along a space tile's shoreline, between the end points
// in inOffsets, with visibility linked to those points; nothing on the left or right map edge
void GETerrainMesh::AddCliffPolys(GETMPolygon*& ioPolygon, int inX, int inY, const float* inOffsets,
                                  unsigned long inMaterial)
{
    float theX;
    float theY;
    float theAX;
    float theAY;
    float theBX;
    float theBY;
    long theAPointX;
    long theAPointY;
    long theBPointX;
    long theBPointY;
    float* theAVisibility;
    float* theBVisibility;
    float theZ;
    GETMRenderVertex* theATop;
    GETMRenderVertex* theABottom;
    GETMRenderVertex* theBTop;
    GETMRenderVertex* theBBottom;
    if (inX + inY != this->mLeftEdge && inX + inY != this->mRightEdge) {
        this->mVisiblePolyCount += 2;
        theX = (float)inX;
        theAX = theX + inOffsets[0];
        theY = (float)inY;
        theAY = theY + inOffsets[1];
        theBX = theX + inOffsets[2];
        theBY = theY + inOffsets[3];
        theAPointX = (long)(theAX + 0.5f);
        theAPointY = (long)(theAY + 0.5f);
        theBPointX = (long)(theBX + 0.5f);
        theBPointY = (long)(theBY + 0.5f);
        theAVisibility = &this->mTerrainRenderVertices[theAPointX - this->mVisibleBottomLeftX][theAPointY - this->mVisibleBottomLeftY].mVisibility;
        theBVisibility = &this->mTerrainRenderVertices[theBPointX - this->mVisibleBottomLeftX][theBPointY - this->mVisibleBottomLeftY].mVisibility;
        theZ = this->GetZ(theAPointX, theAPointY) - 0.02f;
        GE3DPoint thePoint(theAX, theAY, theZ);
        this->mCliffWorldPoints.push_back(thePoint);
        theATop = new GETMRenderVertex;
        theATop->mVisibility = -1.0f;
        theATop->mLinkedVisibility = theAVisibility;
        theATop->mClipFlags = 0x3f;
        this->mCliffRenderVertices.push_back(theATop);
        thePoint.SetZ(theZ - 8.0f + sCliffNoise[theAPointX % 16][theAPointY % 16] * 0.5f);
        this->mCliffWorldPoints.push_back(thePoint);
        theABottom = new GETMRenderVertex;
        theABottom->mVisibility = -1.0f;
        theABottom->mLinkedVisibility = theAVisibility;
        theABottom->mClipFlags = 0x3f;
        this->mCliffRenderVertices.push_back(theABottom);
        if (this->GetIsValidPoint(theBPointX, theBPointY))
            theZ = this->GetZ(theBPointX, theBPointY) - 0.05f;
        thePoint.SetX(theBX);
        thePoint.SetY(theBY);
        thePoint.SetZ(theZ);
        this->mCliffWorldPoints.push_back(thePoint);
        theBTop = new GETMRenderVertex;
        theBTop->mVisibility = -1.0f;
        theBTop->mLinkedVisibility = theBVisibility;
        theBTop->mClipFlags = 0x3f;
        this->mCliffRenderVertices.push_back(theBTop);
        thePoint.SetZ(theZ - 8.0f + sCliffNoise[theBPointX % 16][theBPointY % 16] * 0.5f);
        this->mCliffWorldPoints.push_back(thePoint);
        theBBottom = new GETMRenderVertex;
        EE_LOST_DEBUG(6);  // LOST-DEBUG: none independent (exact only); smallest n here (1..5 not exact; the 4th render vertex's point ctor is refused in the orig)
        theBBottom->mVisibility = -1.0f;
        theBBottom->mLinkedVisibility = theBVisibility;
        theBBottom->mClipFlags = 0x3f;
        this->mCliffRenderVertices.push_back(theBBottom);
        ioPolygon->mVertices[0] = theATop;
        ioPolygon->mVertices[1] = theBTop;
        ioPolygon->mVertices[2] = theABottom;
        ioPolygon->mUV[0] = GETextureCoordinate(0.0f, 0.0f);
        ioPolygon->mUV[1] = GETextureCoordinate(1.0f, 0.0f);
        ioPolygon->mUV[2] = GETextureCoordinate(0.0f, 1.0f);
        ioPolygon->mAlpha[0] = 1.0f;
        ioPolygon->mAlpha[1] = 1.0f;
        ioPolygon->mAlpha[2] = 1.0f;
        this->mTerrainMaterialList[inMaterial].mPolygons.push_back(ioPolygon);
        ioPolygon++;
        ioPolygon->mVertices[0] = theABottom;
        ioPolygon->mVertices[1] = theBTop;
        ioPolygon->mVertices[2] = theBBottom;
        ioPolygon->mUV[0] = GETextureCoordinate(0.0f, 1.0f);
        ioPolygon->mUV[1] = GETextureCoordinate(1.0f, 0.0f);
        ioPolygon->mUV[2] = GETextureCoordinate(1.0f, 1.0f);
        ioPolygon->mAlpha[0] = 1.0f;
        ioPolygon->mAlpha[1] = 1.0f;
        ioPolygon->mAlpha[2] = 1.0f;
        this->mTerrainMaterialList[inMaterial].mPolygons.push_back(ioPolygon);
        ioPolygon++;
    }
}

// FUNCTION: LLE 0x1002bd08
// Adds a tile's terrain triangles with a material: one on the left or right map edge, else two, minus the one cut by
// the top or bottom map edge
void GETerrainMesh::AddTilePolys(int inX, int inY, GETMQuad& inQuad, GETMPolygon*& ioPolygon, unsigned long inMaterial)
{
    if (this->GetIsRightEdgeTile(inX, inY)) {
        ioPolygon->mVertices[0] = inQuad.mVertices[2];
        ioPolygon->mVertices[1] = inQuad.mVertices[1];
        ioPolygon->mVertices[2] = inQuad.mVertices[3];
        ioPolygon->mUV[0] = inQuad.mUV[2];
        ioPolygon->mUV[1] = inQuad.mUV[1];
        ioPolygon->mUV[2] = inQuad.mUV[3];
        ioPolygon->mAlpha[0] = inQuad.mAlpha[2];
        ioPolygon->mAlpha[1] = inQuad.mAlpha[1];
        ioPolygon->mAlpha[2] = inQuad.mAlpha[3];
        this->mTerrainMaterialList[inMaterial].mPolygons.push_back(ioPolygon);
        ioPolygon++;
        this->mVisiblePolyCount++;
    } else if (this->GetIsLeftEdgeTile(inX, inY)) {
        ioPolygon->mVertices[0] = inQuad.mVertices[1];
        ioPolygon->mVertices[1] = inQuad.mVertices[0];
        ioPolygon->mVertices[2] = inQuad.mVertices[3];
        ioPolygon->mUV[0] = inQuad.mUV[1];
        ioPolygon->mUV[1] = inQuad.mUV[0];
        ioPolygon->mUV[2] = inQuad.mUV[3];
        ioPolygon->mAlpha[0] = inQuad.mAlpha[1];
        ioPolygon->mAlpha[1] = inQuad.mAlpha[0];
        ioPolygon->mAlpha[2] = inQuad.mAlpha[3];
        this->mTerrainMaterialList[inMaterial].mPolygons.push_back(ioPolygon);
        ioPolygon++;
        this->mVisiblePolyCount++;
    } else {
        if (!this->GetIsTopEdgeTile(inX, inY)) {
            ioPolygon->mVertices[0] = inQuad.mVertices[2];
            ioPolygon->mVertices[1] = inQuad.mVertices[1];
            ioPolygon->mVertices[2] = inQuad.mVertices[0];
            ioPolygon->mUV[0] = inQuad.mUV[2];
            ioPolygon->mUV[1] = inQuad.mUV[1];
            ioPolygon->mUV[2] = inQuad.mUV[0];
            ioPolygon->mAlpha[0] = inQuad.mAlpha[2];
            ioPolygon->mAlpha[1] = inQuad.mAlpha[1];
            ioPolygon->mAlpha[2] = inQuad.mAlpha[0];
            this->mTerrainMaterialList[inMaterial].mPolygons.push_back(ioPolygon);
            ioPolygon++;
            this->mVisiblePolyCount++;
        }
        if (this->GetIsBottomEdgeTile(inX, inY))
            return;
        ioPolygon->mVertices[0] = inQuad.mVertices[2];
        ioPolygon->mVertices[1] = inQuad.mVertices[0];
        ioPolygon->mVertices[2] = inQuad.mVertices[3];
        ioPolygon->mUV[0] = inQuad.mUV[2];
        ioPolygon->mUV[1] = inQuad.mUV[0];
        ioPolygon->mUV[2] = inQuad.mUV[3];
        ioPolygon->mAlpha[0] = inQuad.mAlpha[2];
        ioPolygon->mAlpha[1] = inQuad.mAlpha[0];
        ioPolygon->mAlpha[2] = inQuad.mAlpha[3];
        this->mTerrainMaterialList[inMaterial].mPolygons.push_back(ioPolygon);
        ioPolygon++;
        this->mVisiblePolyCount++;
    }
}

// FUNCTION: LLE 0x1002bf5c
// Adds a tile's water triangles with a water material: one on the left or right map edge, else two, minus the one cut
// by the top or bottom map edge
void GETerrainMesh::AddWaterPolys(int inX, int inY, GETMQuad& inQuad, GETMPolygon*& ioPolygon, unsigned long inMaterial)
{
    if (this->GetIsRightEdgeTile(inX, inY)) {
        ioPolygon->mVertices[0] = inQuad.mVertices[2];
        ioPolygon->mVertices[1] = inQuad.mVertices[1];
        ioPolygon->mVertices[2] = inQuad.mVertices[3];
        ioPolygon->mUV[0] = inQuad.mUV[2];
        ioPolygon->mUV[1] = inQuad.mUV[1];
        ioPolygon->mUV[2] = inQuad.mUV[3];
        ioPolygon->mAlpha[0] = inQuad.mAlpha[2];
        ioPolygon->mAlpha[1] = inQuad.mAlpha[1];
        ioPolygon->mAlpha[2] = inQuad.mAlpha[3];
        this->mWaterMaterialList[inMaterial].mPolygons.push_back(ioPolygon);
        ioPolygon++;
        this->mVisiblePolyCount++;
    } else if (this->GetIsLeftEdgeTile(inX, inY)) {
        ioPolygon->mVertices[0] = inQuad.mVertices[1];
        ioPolygon->mVertices[1] = inQuad.mVertices[0];
        ioPolygon->mVertices[2] = inQuad.mVertices[3];
        ioPolygon->mUV[0] = inQuad.mUV[1];
        ioPolygon->mUV[1] = inQuad.mUV[0];
        ioPolygon->mUV[2] = inQuad.mUV[3];
        ioPolygon->mAlpha[0] = inQuad.mAlpha[1];
        ioPolygon->mAlpha[1] = inQuad.mAlpha[0];
        ioPolygon->mAlpha[2] = inQuad.mAlpha[3];
        this->mWaterMaterialList[inMaterial].mPolygons.push_back(ioPolygon);
        ioPolygon++;
        this->mVisiblePolyCount++;
    } else {
        if (!this->GetIsTopEdgeTile(inX, inY)) {
            ioPolygon->mVertices[0] = inQuad.mVertices[2];
            ioPolygon->mVertices[1] = inQuad.mVertices[1];
            ioPolygon->mVertices[2] = inQuad.mVertices[0];
            ioPolygon->mUV[0] = inQuad.mUV[2];
            ioPolygon->mUV[1] = inQuad.mUV[1];
            ioPolygon->mUV[2] = inQuad.mUV[0];
            ioPolygon->mAlpha[0] = inQuad.mAlpha[2];
            ioPolygon->mAlpha[1] = inQuad.mAlpha[1];
            ioPolygon->mAlpha[2] = inQuad.mAlpha[0];
            this->mWaterMaterialList[inMaterial].mPolygons.push_back(ioPolygon);
            ioPolygon++;
            this->mVisiblePolyCount++;
        }
        if (this->GetIsBottomEdgeTile(inX, inY))
            return;
        ioPolygon->mVertices[0] = inQuad.mVertices[2];
        ioPolygon->mVertices[1] = inQuad.mVertices[0];
        ioPolygon->mVertices[2] = inQuad.mVertices[3];
        ioPolygon->mUV[0] = inQuad.mUV[2];
        ioPolygon->mUV[1] = inQuad.mUV[0];
        ioPolygon->mUV[2] = inQuad.mUV[3];
        ioPolygon->mAlpha[0] = inQuad.mAlpha[2];
        ioPolygon->mAlpha[1] = inQuad.mAlpha[0];
        ioPolygon->mAlpha[2] = inQuad.mAlpha[3];
        this->mWaterMaterialList[inMaterial].mPolygons.push_back(ioPolygon);
        ioPolygon++;
        this->mVisiblePolyCount++;
    }
}

// The map-edge strips UpdateVisibleTerrain found visible this frame ([0] first, [1] last tile) and whether each
// edge was seen at all (0x1010fdd8.., 0x1010fed8..) (provisional names).
// The four arrays' init chains (0x1002c1b0..0x1002c264; U2DPoint's ctor zero-fills, its empty dtor leaves a bare
// atexit stub):
// SYNTHETIC: LLE 0x1002c1b0
// SYNTHETIC: LLE 0x1002c1ba
// SYNTHETIC: LLE 0x1002c1d0
// SYNTHETIC: LLE 0x1002c1dc
// SYNTHETIC: LLE 0x1002c1dd
// SYNTHETIC: LLE 0x1002c1e7
// SYNTHETIC: LLE 0x1002c1fd
// SYNTHETIC: LLE 0x1002c209
// SYNTHETIC: LLE 0x1002c20a
// SYNTHETIC: LLE 0x1002c214
// SYNTHETIC: LLE 0x1002c22a
// SYNTHETIC: LLE 0x1002c236
// SYNTHETIC: LLE 0x1002c237
// SYNTHETIC: LLE 0x1002c241
// SYNTHETIC: LLE 0x1002c257
// SYNTHETIC: LLE 0x1002c263
static U2DPoint<long> sFrontEdge[2];
static U2DPoint<long> sBackEdge[2];
static U2DPoint<long> sLeftEdge[2];
static U2DPoint<long> sRightEdge[2];
static bool sHasFrontEdge;
static bool sHasBackEdge;
static bool sHasLeftEdge;
static bool sHasRightEdge;

// FUNCTION: LLE 0x1002c264
// Returns whether a tile is a space tile; false off the map
bool GETerrainMesh::GetIsSpaceTile(unsigned long inX, unsigned long inY) const
{
    if (!this->GetIsValidTile(inX, inY))
        return false;
    return (this->mTileRows[inY].mTiles[inX - this->mTileRows[inY].mStart].mWaterInfo & kWaterInfoSpace) != 0;
}

// A space tile's shoreline drawing, per shoreline code (0x10097bf8): the cliff material offset, the shore texture's
// corner UVs, the cliff orientation and the cliff end points (provisional names).
struct GETMShoreline {
    long mMaterialOffset;
    float mUV[4][2];
    long mCliffType;
    float mCliffOffsets[4];
};

static const GETMShoreline sShorelines[16] = {
    { 0, { { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }, 0, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { 0, { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } }, 5, { 0.4f, 0.0f, 0.0f, 0.4f } },
    { 0, { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } }, 3, { 0.0f, 0.6f, 0.4f, 1.0f } },
    { 1, { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f } }, 2, { 0.4f, 0.0f, 0.4f, 1.0f } },
    { 0, { { 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } }, 4, { 0.6f, 1.0f, 1.0f, 0.6f } },
    { 3, { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } }, 2, { 0.0f, 0.4f, 0.4f, 0.0f } },
    { 1, { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } }, 1, { 0.0f, 0.5f, 1.0f, 0.6f } },
    { 2, { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } }, 4, { 0.4f, 0.0f, 1.0f, 0.6f } },
    { 0, { { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }, 0, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { 1, { { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }, 0, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { 3, { { 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } }, 1, { 0.0f, 0.6f, 0.4f, 1.0f } },
    { 2, { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } }, 6, { 1.0f, 0.4f, 0.4f, 1.0f } },
    { 1, { { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f } }, 0, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { 2, { { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }, 0, { 0.0f, 0.0f, 0.0f, 0.0f } },
    { 2, { { 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } }, 3, { 0.0f, 0.6f, 0.6f, 0.0f } },
    { 4, { { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }, 0, { 0.0f, 0.0f, 0.0f, 0.0f } }
};

// The four tiles around a map point (0x10097f78) and, per tile, the three tiles beyond it (0x10097f98)
// (provisional names).
static const long sCornerTiles[4][2] = { { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0, -1 } };
static const long sCornerNeighbours[4][3][2] = {
    { { 0, 1 }, { 1, 1 }, { 1, 0 } },
    { { 0, 1 }, { -1, 1 }, { -1, 0 } },
    { { 0, -1 }, { -1, -1 }, { -1, 0 } },
    { { 0, -1 }, { 1, -1 }, { 1, 0 } }
};

// One texture layer of a tile in UpdateVisibleTerrain (provisional name).
struct GETMTileLayer {
    bool mIsUsed;
    unsigned long mTextureSetID;
    GETexture* mTexture;
};

// Not inlined: the out-of-line copies follow UpdateVisibleTerrain (0x1002d082, 0x1002d222) (provisional names).
// FUNCTION: LLE 0x1002d082
// Grows the visible map-edge strip the tile (inX, inY) lies on (right, left, else front and/or back)
inline void TrackVisibleEdges(const GETerrainMesh& inMesh, long inX, long inY)
{
    if (inMesh.GetIsRightEdgeTile(inX, inY)) {
        if (sHasRightEdge) {
            if (inX < sRightEdge[0].mX) {
                sRightEdge[0].mX = inX;
                sRightEdge[0].mY = inY + 1;
            }
            if (inX + 1 > sRightEdge[1].mX) {
                sRightEdge[1].mX = inX + 1;
                sRightEdge[1].mY = inY;
            }
        } else {
            sRightEdge[0].mX = inX;
            sRightEdge[0].mY = inY + 1;
            sRightEdge[1].mX = inX + 1;
            sRightEdge[1].mY = inY;
            sHasRightEdge = true;
        }
    } else if (inMesh.GetIsLeftEdgeTile(inX, inY)) {
        if (sHasLeftEdge) {
            if (inX < sLeftEdge[0].mX) {
                sLeftEdge[0].mX = inX;
                sLeftEdge[0].mY = inY + 1;
            }
            if (inX + 1 > sLeftEdge[1].mX) {
                sLeftEdge[1].mX = inX + 1;
                sLeftEdge[1].mY = inY;
            }
        } else {
            sLeftEdge[0].mX = inX;
            sLeftEdge[0].mY = inY + 1;
            sLeftEdge[1].mX = inX + 1;
            sLeftEdge[1].mY = inY;
            sHasLeftEdge = true;
        }
    } else {
        if (inMesh.GetIsTopEdgeTile(inX, inY)) {
            if (sHasFrontEdge) {
                if (inX < sFrontEdge[0].mX) {
                    sFrontEdge[0].mX = inX;
                    sFrontEdge[0].mY = inY;
                }
                if (inX + 1 > sFrontEdge[1].mX) {
                    sFrontEdge[1].mX = inX + 1;
                    sFrontEdge[1].mY = inY + 1;
                }
            } else {
                sFrontEdge[1].mX = inX + 1;
                sFrontEdge[0].mX = inX;
                sFrontEdge[0].mY = inY;
                sFrontEdge[1].mY = inY + 1;
                sHasFrontEdge = true;
            }
        }
        if (inMesh.GetIsBottomEdgeTile(inX, inY)) {
            if (sHasBackEdge) {
                if (inX < sBackEdge[0].mX) {
                    sBackEdge[0].mX = inX;
                    sBackEdge[0].mY = inY;
                }
                if (inX + 1 > sBackEdge[1].mX) {
                    sBackEdge[1].mX = inX + 1;
                    sBackEdge[1].mY = inY + 1;
                }
            } else {
                sBackEdge[0].mX = inX;
                sBackEdge[1].mX = inX + 1;
                sBackEdge[0].mY = inY;
                sBackEdge[1].mY = inY + 1;
                sHasBackEdge = true;
            }
        }
    }
}

// FUNCTION: LLE 0x1002d222
// Grows the visible back edge strip when the tile (inX, inY) lies on the back edge (space maps)
inline void TrackVisibleSpaceEdges(const GETerrainMesh& inMesh, long inX, long inY)
{
    if (!inMesh.GetIsBottomEdgeTile(inX, inY))
        return;
    if (sHasBackEdge) {
        if (inX < sBackEdge[0].mX) {
            sBackEdge[0].mX = inX;
            sBackEdge[0].mY = inY;
        }
        if (inX + 1 > sBackEdge[1].mX) {
            sBackEdge[1].mX = inX + 1;
            sBackEdge[1].mY = inY + 1;
        }
    } else {
        sBackEdge[0].mX = inX;
        sBackEdge[1].mX = inX + 1;
        sBackEdge[0].mY = inY;
        sBackEdge[1].mY = inY + 1;
        sHasBackEdge = true;
    }
}

// FUNCTION: LLE 0x1002c299
// Rebuilds the visible terrain for the camera: finds the visible tiles, rebuilds the terrain and water materials,
// adds blended texture-layer, space shoreline, cliff and water polygons, then the map-edge skirts
void GETerrainMesh::UpdateVisibleTerrain(const GETransformation& inCamera, GEViewport& inViewport,
                                         unsigned long inWidth, unsigned long inHeight, unsigned long inScreen,
                                         bool inOrthographic)
{
    std::vector<GE3DPoint> theFrustumPoints;
    GETMMaterial theMaterial;
    GETMQuad theQuad;
    U2DPoint<long> theMinimum;
    U2DPoint<long> theMaximum;
    std::map<GETexture*, unsigned long>::iterator theIterator;
    unsigned long theVertexBase = 0;
    unsigned long i;
    unsigned long theRowCount;
    unsigned long theMaterialIndex;
    unsigned long theFirstCliffMaterial;
    long theX;
    long theY;
    long theLayerCount;
    long theBest;
    long theLayer;
    long theCode;
    long theCliff;
    long k;
    GETMPolygon* thePolygon;
    GETMPolygon* theWaterPolygon;
    unsigned char theWaterInfo;
    memset(this->mTerrainPointVisibility, 0, sizeof(this->mTerrainPointVisibility));
    memset(this->mWaterPointVisibility, 0, sizeof(this->mWaterPointVisibility));
    sHasFrontEdge = false;
    sHasBackEdge = false;
    sHasLeftEdge = false;
    sHasRightEdge = false;
    this->mVisiblePolyCount = 0;
    this->mNumVisibleTiles = 0;
    this->mVisibleTileRows.clear();
    inViewport.GetFrustrumWorldPoints(theFrustumPoints, inOrthographic);
    this->GetTileRowsInVolume(theFrustumPoints, this->mVisibleTileRows, theMinimum, theMaximum);
    this->mVisibleBottomLeftX = theMinimum.mX;
    this->mVisibleBottomLeftY = theMinimum.mY;
    this->mVisibleTileWidth = theMaximum.mX - theMinimum.mX;
    this->mVisibleTileHeight = theMaximum.mY - theMinimum.mY;
    for (theIterator = this->mTerrainMaterialMap.begin(); theIterator != this->mTerrainMaterialMap.end(); theIterator++) {
        if (theIterator->first != NULL)
            theIterator->first->Dereference();
    }
    i = 0;
    for (theIterator = this->mWaterMaterialMap.begin(); theIterator != this->mWaterMaterialMap.end(); theIterator++, i++) {
        if (i >= this->mWaterAnimationMaterialList.size() && theIterator->first != NULL)
            theIterator->first->Dereference();
    }
    this->mTerrainMaterialMap.clear();
    this->mTerrainMaterialList.clear();
    this->mWaterMaterialMap.clear();
    this->mWaterMaterialList.clear();
    for (i = 0; i < this->mWaterAnimationMaterialList.size(); i++)
        this->mWaterMaterialList.push_back(this->mWaterAnimationMaterialList[i]);
    this->mEdgeWorldPoints.clear();
    for (i = 0; (long)i < (long)this->mEdgeRenderVertices.size(); i++)
        delete this->mEdgeRenderVertices[i];
    this->mEdgeRenderVertices.clear();
    this->mCliffWorldPoints.clear();
    for (i = 0; (long)i < (long)this->mCliffRenderVertices.size(); i++)
        delete this->mCliffRenderVertices[i];
    this->mCliffRenderVertices.clear();
    this->mTerrainMaterialMap.insert(std::pair<GETexture* const, unsigned long>(NULL, this->mTerrainMaterialList.size()));
    theMaterial.mTexture = NULL;
    this->mTerrainMaterialList.push_back(theMaterial);
    theFirstCliffMaterial = this->mTerrainMaterialList.size();
    if (this->mSpaceTileCount != 0) {
        for (i = 0; i < this->mSpaceCliffTextures.size(); i++) {
            theMaterial.mTexture = this->mSpaceCliffTextures[i];
            theMaterial.mTexture->Reference();
            this->mTerrainMaterialMap.insert(
                std::pair<GETexture* const, unsigned long>(theMaterial.mTexture, this->mTerrainMaterialList.size()));
            this->mTerrainMaterialList.push_back(theMaterial);
        }
    }
    theRowCount = this->mVisibleTileRows.size();
    thePolygon = this->mTerrainPolygons;
    theWaterPolygon = this->mWaterPolygons;
    for (i = 0; i < theRowCount; i++) {
        this->mNumVisibleTiles += this->mVisibleTileRows[i].second.second - this->mVisibleTileRows[i].second.first + 1;
        theY = this->mVisibleTileRows[i].first;
        for (theX = this->mVisibleTileRows[i].second.first; theX <= this->mVisibleTileRows[i].second.second; theX++) {
            this->mTerrainPointVisibility[theX - this->mVisibleBottomLeftX][theY - this->mVisibleBottomLeftY] = true;
            this->mTerrainPointVisibility[theX - this->mVisibleBottomLeftX + 1][theY - this->mVisibleBottomLeftY] = true;
            this->mTerrainPointVisibility[theX - this->mVisibleBottomLeftX][theY - this->mVisibleBottomLeftY + 1] = true;
            this->mTerrainPointVisibility[theX - this->mVisibleBottomLeftX + 1][theY - this->mVisibleBottomLeftY + 1] = true;
            theQuad.mVertices[0] =
                &this->mTerrainRenderVertices[theX - this->mVisibleBottomLeftX + 1][theY - this->mVisibleBottomLeftY + 1];
            theQuad.mVertices[1] = &this->mTerrainRenderVertices[theX - this->mVisibleBottomLeftX][theY - this->mVisibleBottomLeftY + 1];
            theQuad.mVertices[2] = &this->mTerrainRenderVertices[theX - this->mVisibleBottomLeftX][theY - this->mVisibleBottomLeftY];
            theQuad.mVertices[3] = &this->mTerrainRenderVertices[theX - this->mVisibleBottomLeftX + 1][theY - this->mVisibleBottomLeftY];
            theQuad.mUV[0] = GETextureCoordinate(1.0f, 0.0f);
            theQuad.mUV[1] = GETextureCoordinate(0.0f, 0.0f);
            theQuad.mUV[2] = GETextureCoordinate(0.0f, 1.0f);
            theQuad.mUV[3] = GETextureCoordinate(1.0f, 1.0f);
            theLayerCount = 4;
            if (this->mSpaceTileCount != 0 && this->GetIsSpaceTile(theX, theY)) {
                theLayerCount = 0;
                theQuad.mAlpha[0] = -1.0f;
                theQuad.mAlpha[1] = -1.0f;
                theQuad.mAlpha[2] = -1.0f;
                theQuad.mAlpha[3] = -1.0f;
                this->AddTilePolys(theX, theY, theQuad, thePolygon, 0);
                theWaterInfo = this->GetTile(theX, theY)->mWaterInfo;
                if ((theWaterInfo & kWaterInfoShorelineMask) != 0) {
                    theCode = (theWaterInfo >> 1) & 0xf;
                    for (k = 0; k < 4; k++) {
                        theQuad.mUV[k].SetU(sShorelines[theCode].mUV[k][0]);
                        theQuad.mUV[k].SetV(sShorelines[theCode].mUV[k][1]);
                        theQuad.mAlpha[k] = 1.0f;
                    }
                    this->AddTilePolys(theX, theY, theQuad, thePolygon,
                                 theFirstCliffMaterial + 8 + sShorelines[theCode].mMaterialOffset);
                    switch (sShorelines[theCode].mCliffType) {
                    case 1:
                        theCliff = theX;
                        break;
                    case 2:
                        theCliff = theY;
                        break;
                    case 3:
                        theCliff = theX * 2;
                        break;
                    case 4:
                        theCliff = theX * 2 + 1;
                        break;
                    case 5:
                        theCliff = theY * 2;
                        break;
                    case 6:
                        theCliff = theY * 2 + 1;
                        break;
                    default:
                        theCliff = -1;
                        break;
                    }
                    if (theCliff % 8 >= 0)
                        this->AddCliffPolys(thePolygon, theX, theY, sShorelines[theCode].mCliffOffsets,
                                      theCliff % 8 + theFirstCliffMaterial);
                }
            }
            GETMTileLayer theLayers[4] = { false };
            theBest = -1;
            for (theLayer = 0; theLayer < theLayerCount; theLayer++) {
                if (this->GetIsValidTile(theX + sCornerTiles[theLayer][0], theY + sCornerTiles[theLayer][1])) {
                    theLayers[theLayer].mTextureSetID =
                        this->GetTile(theX + sCornerTiles[theLayer][0], theY + sCornerTiles[theLayer][1])->mTextureSetID;
                    if (this->mSpaceTileCount != 0) {
                        for (k = 0; k < 3; k++) {
                            if (theLayers[theLayer].mTextureSetID == (unsigned long)-1)
                                break;
                            if (this->GetIsValidTile(theX + sCornerNeighbours[theLayer][k][0],
                                               theY + sCornerNeighbours[theLayer][k][1]) &&
                                (this->GetTile(theX + sCornerNeighbours[theLayer][k][0],
                                         theY + sCornerNeighbours[theLayer][k][1])->mWaterInfo & kWaterInfoShorelineMask) != 0) {
                                theLayers[theLayer].mTextureSetID = (unsigned long)-1;
                                theLayers[theLayer].mTexture = this->mSpaceCliffTextures[12];
                            }
                        }
                    }
                    if (theLayers[theLayer].mTextureSetID == theLayers[0].mTextureSetID && theLayer != 0) {
                        theLayers[theLayer].mIsUsed = false;
                        theLayers[theLayer].mTexture = theLayers[0].mTexture;
                    } else {
                        theLayers[theLayer].mIsUsed = true;
                        if (theLayers[theLayer].mTextureSetID != (unsigned long)-1)
                            theLayers[theLayer].mTexture =
                                this->mRegisteredTextureSets[theLayers[theLayer].mTextureSetID]->GetTexture(theX, theY);
                        if (theBest == -1 || theLayers[theLayer].mTexture < theLayers[theBest].mTexture)
                            theBest = theLayer;
                    }
                } else
                    theLayers[theLayer].mIsUsed = false;
            }
            for (theLayer = 0; theLayer < theLayerCount; theLayer++) {
                if (!theLayers[theLayer].mIsUsed)
                    continue;
                theIterator = this->mTerrainMaterialMap.find(theLayers[theLayer].mTexture);
                if (theIterator == this->mTerrainMaterialMap.end()) {
                    theMaterialIndex = this->mTerrainMaterialList.size();
                    this->mTerrainMaterialMap.insert(std::pair<GETexture* const, unsigned long>(
                        theLayers[theLayer].mTexture, this->mTerrainMaterialList.size()));
                    theMaterial.mTexture = theLayers[theLayer].mTexture;
                    this->mTerrainMaterialList.push_back(theMaterial);
                    theLayers[theLayer].mTexture->Reference();
                } else
                    theMaterialIndex = theIterator->second;
                if (theLayer == theBest) {
                    for (k = 0; k < 4; k++)
                        theQuad.mAlpha[k] = 1.0f;
                    for (k = 0; k < 4; k++) {
                        if (theLayer < k && theLayers[k].mTexture == theLayers[theLayer].mTexture)
                            theLayers[k].mIsUsed = false;
                    }
                } else {
                    for (k = 0; k < theLayer; k++)
                        theQuad.mAlpha[k] = 0.0f;
                    theQuad.mAlpha[k] = 1.0f;
                    for (k = theLayer + 1; k < 4; k++) {
                        if (theLayers[k].mTexture == theLayers[theLayer].mTexture) {
                            theLayers[k].mIsUsed = false;
                            theQuad.mAlpha[k] = 1.0f;
                        } else
                            theQuad.mAlpha[k] = 0.0f;
                    }
                }
                this->AddTilePolys(theX, theY, theQuad, thePolygon, theMaterialIndex);
            }
            if (this->mSpaceTileCount == 0)
                TrackVisibleEdges(*this, theX, theY);
            else
                TrackVisibleSpaceEdges(*this, theX, theY);
            if (this->mSpaceTileCount != 0 || !this->GetIsWaterTile(theX, theY))
                continue;
            theQuad.mVertices[2] = &this->mWaterRenderVertices[theX - this->mVisibleBottomLeftX][theY - this->mVisibleBottomLeftY];
            theQuad.mVertices[1] =
                &this->mWaterRenderVertices[theX - this->mVisibleBottomLeftX][theY - this->mVisibleBottomLeftY + 1];
            theQuad.mVertices[0] =
                &this->mWaterRenderVertices[theX - this->mVisibleBottomLeftX + 1][theY - this->mVisibleBottomLeftY + 1];
            theQuad.mVertices[3] =
                &this->mWaterRenderVertices[theX - this->mVisibleBottomLeftX + 1][theY - this->mVisibleBottomLeftY];
            this->mWaterPointVisibility[theX - this->mVisibleBottomLeftX][theY - this->mVisibleBottomLeftY] = true;
            this->mWaterPointVisibility[theX - this->mVisibleBottomLeftX + 1][theY - this->mVisibleBottomLeftY] = true;
            this->mWaterPointVisibility[theX - this->mVisibleBottomLeftX][theY - this->mVisibleBottomLeftY + 1] = true;
            this->mWaterPointVisibility[theX - this->mVisibleBottomLeftX + 1][theY - this->mVisibleBottomLeftY + 1] = true;
            theQuad.mUV[2] = GETextureCoordinate(0.0f, 1.0f);
            theQuad.mUV[1] = GETextureCoordinate(0.0f, 0.0f);
            theQuad.mUV[3] = GETextureCoordinate(1.0f, 1.0f);
            theQuad.mUV[0] = GETextureCoordinate(1.0f, 0.0f);
            theWaterInfo = this->GetTile(theX, theY)->mWaterInfo;
            theQuad.mAlpha[2] = (theWaterInfo & 2) ? 0.0f : 0.5f;
            theQuad.mAlpha[1] = (theWaterInfo & 4) ? 0.0f : 0.5f;
            theQuad.mAlpha[0] = (theWaterInfo & 8) ? 0.0f : 0.5f;
            theQuad.mAlpha[3] = (theWaterInfo & 16) ? 0.0f : 0.5f;
            this->AddWaterPolys(theX, theY, theQuad, theWaterPolygon, this->mWaterTextureIndices[theX % 33][theY % 33]);
        }
    }
    if (this->mSpaceTileCount == 0) {
        if (sHasFrontEdge)
            this->GenerateFrontEdgePolygons(inViewport, sFrontEdge, thePolygon, theVertexBase);
        if (sHasBackEdge)
            this->GenerateBackEdgePolygons(inViewport, sBackEdge, thePolygon, theVertexBase);
        if (sHasLeftEdge)
            this->GenerateLeftEdgePolygons(inViewport, sLeftEdge, thePolygon, theVertexBase);
        if (sHasRightEdge)
            this->GenerateRightEdgePolygons(inViewport, sRightEdge, thePolygon, theVertexBase);
        this->GenerateCornerFrontLeftPolygons(inViewport, sFrontEdge, sHasFrontEdge, sLeftEdge, sHasLeftEdge, thePolygon,
                                        theVertexBase);
        this->GenerateCornerFrontRightPolygons(inViewport, sFrontEdge, sHasFrontEdge, sRightEdge, sHasRightEdge,
                                         thePolygon, theVertexBase);
        this->GenerateCornerBackLeftPolygons(inViewport, sBackEdge, sHasBackEdge, sLeftEdge, sHasLeftEdge, thePolygon,
                                       theVertexBase);
        this->GenerateCornerBackRightPolygons(inViewport, sBackEdge, sHasBackEdge, sRightEdge, sHasRightEdge, thePolygon,
                                        theVertexBase);
    } else {
        if (sHasBackEdge)
            this->GenerateBackEdgePolygons(inViewport, sBackEdge, thePolygon, theVertexBase);
        this->GenerateCornerBackLeftPolygons(inViewport, sBackEdge, sHasBackEdge, sLeftEdge, sHasLeftEdge, thePolygon,
                                       theVertexBase);
        this->GenerateCornerBackRightPolygons(inViewport, sBackEdge, sHasBackEdge, sRightEdge, sHasRightEdge, thePolygon,
                                        theVertexBase);
    }
    for (i = 0; i < this->mTerrainMaterialList.size(); i++) {
        this->mTerrainMaterialList[i].mStripIndices.resize(this->mTerrainMaterialList[i].mPolygons.size() + 2);
        this->mTerrainMaterialList[i].mIndices.resize(this->mTerrainMaterialList[i].mPolygons.size() + 2);
    }
    for (i = 0; i < this->mWaterMaterialList.size(); i++) {
        this->mWaterMaterialList[i].mStripIndices.resize(this->mWaterMaterialList[i].mPolygons.size() + 2);
        this->mWaterMaterialList[i].mIndices.resize(this->mWaterMaterialList[i].mPolygons.size() + 2);
    }
}

// FUNCTION: LLE 0x1002d2a6
// Marks or clears a tile as space, updating the space-tile count and the shorelines of it and its eight neighbours
void GETerrainMesh::SetIsSpace(unsigned long inX, unsigned long inY, bool inIsSpace)
{
    GETMTile* theTile = this->GetTile(inX, inY);
    unsigned long theX;
    unsigned long theY;
    if (inIsSpace) {
        if ((theTile->mWaterInfo & kWaterInfoSpace) == 0) {
            theTile->mWaterInfo |= kWaterInfoSpace;
            this->mSpaceTileCount++;
            for (theY = inY - 1; theY <= inY + 1; theY++) {
                for (theX = inX - 1; theX <= inX + 1; theX++) {
                    if (this->GetIsValidTile(theX, theY))
                        this->ComputeShorelines(theX, theY);
                }
            }
        }
    } else if ((theTile->mWaterInfo & kWaterInfoSpace) != 0) {
        theTile->mWaterInfo &= kWaterInfoWaterMask;
        for (theY = inY - 1; theY <= inY + 1; theY++) {
            for (theX = inX - 1; theX <= inX + 1; theX++) {
                if (this->GetIsValidTile(theX, theY))
                    this->ComputeShorelines(theX, theY);
            }
        }
        this->mSpaceTileCount--;
    }
}

// FUNCTION: LLE 0x1002d3c1
// Loads the map size, each tile's texture set (unknown ids become 0) and water info, each point's height and colour,
// and the fractal seed, then recomputes every normal; returns the first read error
long GETerrainMesh::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    unsigned char theTextureSetID;
    float theZ;
    theResult = inFile.Read(this->mPlayableWidth);
    if (theResult == 0) {
        theResult = inFile.Read(this->mPlayableHeight);
        if (theResult == 0) {
            this->CreateFlatMesh(this->mPlayableWidth, this->mPlayableHeight);
            // Stop at the first failed read and report it (later reads used to go on and hide the error).
            for (j = 0; j < this->mTileRowCount && theResult == 0; j++) {
                for (i = 0; i < this->mTileRows[j].mCount; i++) {
                    theResult = inFile.Read(theTextureSetID);
                    if (theResult != 0)
                        break;
                    if (theTextureSetID < this->mRegisteredTextureSets.size() &&
                        this->mRegisteredTextureSets[theTextureSetID] != NULL)
                        this->mTileRows[j].mTiles[i].mTextureSetID = theTextureSetID;
                    else
                        this->mTileRows[j].mTiles[i].mTextureSetID = 0;
                    theResult = inFile.Read(this->mTileRows[j].mTiles[i].mWaterInfo);
                    if (theResult != 0)
                        break;
                    if ((this->mTileRows[j].mTiles[i].mWaterInfo & kWaterInfoSpace) != 0)
                        this->mSpaceTileCount++;
                }
            }
            for (j = 0; j < this->mPointRowCount && theResult == 0; j++) {
                for (k = 0; k < this->mPointRows[j].mCount; k++) {
                    theResult = inFile.Read(theZ);
                    if (theResult != 0)
                        break;
                    this->mPointRows[j].mPoints[k].mZ = theZ;
                    this->mMinimumZ = min(this->mMinimumZ, theZ);
                    this->mMaximumZ = max(this->mMaximumZ, theZ);
                    theResult = inFile.Read(this->mPointRows[j].mPoints[k].mColorTableIndex);
                    if (theResult != 0)
                        break;
                }
            }
            if (theResult == 0) {
                theResult = inFile.Read(this->mFractalSeed);
                this->mFractal->ComputeFractal(0.0f, 10.5f, this->mFractalSeed);
            }
            for (j = 0; j < this->mPointRowCount; j++) {
                for (i = 0; i < this->mPointRows[j].mCount; i++)
                    this->ComputeVertexNormal(this->mPointRows[j].mStart + i, j);
            }
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1002d656
// Writes the map size, each tile's texture set and water info, each point's height and colour, and the fractal seed;
// returns the first write error
long GETerrainMesh::Save(FSFile& inFile) const
{
    long theResult;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    theResult = inFile.Write(this->mPlayableWidth);
    if (theResult != 0 || (theResult = inFile.Write(this->mPlayableHeight)) != 0)
        return theResult;
    for (j = 0; j < this->mTileRowCount; j++) {
        for (i = 0; i < this->mTileRows[j].mCount; i++) {
            if ((theResult = inFile.Write(this->mTileRows[j].mTiles[i].mTextureSetID)) != 0 ||
                (theResult = inFile.Write(this->mTileRows[j].mTiles[i].mWaterInfo)) != 0)
                break;
        }
        if (theResult != 0)
            break;
    }
    // A failed tile write ends the save (the point writes used to replace its error).
    for (j = 0; j < this->mPointRowCount && theResult == 0; j++) {
        for (k = 0; k < this->mPointRows[j].mCount; k++) {
            if ((theResult = inFile.Write(this->mPointRows[j].mPoints[k].mZ)) != 0)
                break;
            if ((theResult = inFile.Write(this->mPointRows[j].mPoints[k].mColorTableIndex)) != 0)
                break;
        }
        if (theResult != 0)
            break;
    }
    if (theResult == 0)
        theResult = inFile.Write(this->mFractalSeed);
    return theResult;
}
