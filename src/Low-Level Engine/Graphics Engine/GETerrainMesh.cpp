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
// GetMeshZ, GetMeshNormal, GetClosestTile, InitializePlanes and Intersects(line) forward to Rust (rust/crates/ee_ge
// terrain.rs; GERustTerrain.h): they read the rows in place through an EeGeTerrainView.
#include <windows.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/URandom.h"       // U2DFractal.h (the water grid's own URandom draws are Rust)
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
// (lle/U2DPolygon.h: GetTileRowsInVolume's U2DConvexHull<float> is Rust now, ee_ge terrain_volume.rs; the
// exe keeps its own U2DConvexHull<long>)
#include "lle/U2DFractal.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GETextureSet.h"
#include "lle/GEViewport.h"
#include "lle/GETerrainMesh.h"
#include "GERustMath.h"
#include "GERustTerrain.h"
#include "GERustViewport.h"
#include "GERustIO.h"

const UString GETerrainMesh::sPlayableWidthID("GETerrainMesh-PlayableWidth");
const UString GETerrainMesh::sPlayableHeightID("GETerrainMesh-PlayableHeight");
const UString GETerrainMesh::sTextureSetID("GETerrainMesh-TextureSet");
const UString GETerrainMesh::sWaterInfoID("GETerrainMesh-WaterInfo");
const UString GETerrainMesh::sZID("GETerrainMesh-Z-ID");
const UString GETerrainMesh::sFractalSeedID("GETerrainMesh-FractalSeed");
const UString GETerrainMesh::sColorLookupID("GETerrainMesh-ColorLookup");

// (sNeighbours, ComputeShorelines' eight neighbours and their shoreline bits (0x10097b98), is in Rust: ee_ge terrain_mesh.rs)

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
    EeGeTerrainView theView = GERustTerrainView(*this);
    int theShoreline = ee_ge_terrain_shoreline(&theView, this->GetIsSpaceMap(), inX, inY);
    if (theShoreline >= 0)
        this->SetWaterInfo(inX, inY, (unsigned char)theShoreline);
}

// FUNCTION: LLE 0x10027aa4
// Computes a map point's normal from its neighbours' heights (missing ones use its own) and stores the quantized normal
void GETerrainMesh::ComputeVertexNormal(unsigned long inX, unsigned long inY)
{
    GEVector theNormal;
    ee_ge_terrain_vertex_normal(reinterpret_cast<const EeGeTmRow*>(this->mPointRows), this->mPointRowCount, inX, inY,
                                GERust(theNormal));
    GETMPointRow& theRow = this->mPointRows[inY];
    theRow.mPoints[inX - theRow.mStart].mNormal = theNormal.GetQuantizedVersion();
}

// FUNCTION: LLE 0x10027c53
// Rebuilds the mesh as a flat diamond map of inWidth x inHeight tiles: tile and point rows (their columns from Rust:
// ee_ge terrain_mesh.rs, flat_mesh), edge sums, map-edge planes, every height 0 and every normal straight up
void GETerrainMesh::CreateFlatMesh(long inWidth, long inHeight)
{
    GEVector theUp(0.0f, 0.0f, 1.0f);
    GEVector* theNormal = theUp.GetQuantizedVersion();
    std::vector<unsigned long> theTileSpans;
    std::vector<unsigned long> thePointSpans;
    long theEdges[4];
    unsigned long i;
    unsigned long j;
    delete[] this->mPointRows;
    delete[] this->mTileRows;
    this->mSpaceTileCount = 0;
    this->mPlayableWidth = inWidth;
    this->mPlayableHeight = inHeight;
    this->mTileRowCount = inWidth + inHeight;
    this->mPointRowCount = inWidth + inHeight + 1;
    theTileSpans.resize(2 * this->mTileRowCount);
    thePointSpans.resize(2 * this->mPointRowCount);
    ee_ge_terrain_flat_mesh(inWidth, inHeight, theTileSpans.data(), thePointSpans.data(), theEdges);
    this->mLeftEdge = theEdges[0];
    this->mTopEdge = theEdges[1];
    this->mRightEdge = theEdges[2];
    this->mBottomEdge = theEdges[3];
    this->mTileRows = new GETMTileRow[this->mTileRowCount];
    for (i = 0; i < this->mTileRowCount; i++)
        this->mTileRows[i].Initialize(theTileSpans[2 * i], theTileSpans[2 * i + 1]);
    this->mPointRows = new GETMPointRow[this->mPointRowCount];
    for (j = 0; j < this->mPointRowCount; j++)
        this->mPointRows[j].Initialize(thePointSpans[2 * j], thePointSpans[2 * j + 1], theNormal);
    this->InitializePlanes();
    this->mMinimumZ = 0.0f;
    this->mMaximumZ = 0.0f;
}

// (CreateNewEdgeVertex 0x10027e8e, DefineNewEdgePolygon 0x10027f0a, IsSegmentInFrustum 0x100282f5, the four
// Is*EdgeVisible 0x1002821c.., the four Generate*EdgePolygons 0x10027f81.. and the four GenerateCorner*Polygons
// 0x100283d9.. are Rust: ee_ge terrain_visible.rs, the skirts of the plan)


// FUNCTION: LLE 0x1002a05d
// Returns the terrain normal at a map position, interpolated bilinearly from the four surrounding points' normals
void GETerrainMesh::GetMeshNormal(float inX, float inY, GEVector& outNormal) const
{
    EeGeTerrainView theView = GERustTerrainView(*this);
    ee_ge_terrain_normal(&theView, inX, inY, GERust(outNormal));
}

// FUNCTION: LLE 0x1002a185
// Returns the terrain height at a map position, interpolated over the half of its tile (split along the 00-11 diagonal)
float GETerrainMesh::GetMeshZ(float inX, float inY) const
{
    EeGeTerrainView theView = GERustTerrainView(*this);
    return ee_ge_terrain_z(&theView, inX, inY);
}

// FUNCTION: LLE 0x1002a277
// Returns the map tile where the line segment crosses a map-edge plane onto the map; else the tile closest to where
// the line meets height 0 (or to its start)
void GETerrainMesh::GetClosestTile(const GE3DLine& inLine, long& outX, long& outY) const
{
    EeGeTerrainView theView = GERustTerrainView(*this);
    ee_ge_terrain_closest_tile_line(&theView, GERust(inLine), &outX, &outY);
}


// FUNCTION: LLE 0x1002a5af
// Returns the tile under a point, first moving a point outside the map onto the nearest edge planes, just inside
void GETerrainMesh::GetClosestTile(const GE3DPoint& inPoint, long& outX, long& outY) const
{
    EeGeTerrainView theView = GERustTerrainView(*this);
    ee_ge_terrain_closest_tile_point(&theView, GERust(inPoint), &outX, &outY);
}

// FUNCTION: LLE 0x1002a7aa
// Defines the four vertical map-edge planes (front, back, left, right) from the first and last tile rows
void GETerrainMesh::InitializePlanes()
{
    // front, back, left, right: four contiguous members (GERustTerrain.h)
    EeGeTerrainView theView = GERustTerrainView(*this);
    ee_ge_terrain_planes(theView.tile_rows, this->mTileRowCount, GERust(this->mFrontPlane));
}


// FUNCTION: LLE 0x1002a942
// Walks the tiles under the line from its start towards its end; returns whether it hits the terrain within its length
// (outDistance = the line parameter of the first tile hit)
bool GETerrainMesh::Intersects(const GE3DLine& inLine, float& outDistance) const
{
    EeGeTerrainView theView = GERustTerrainView(*this);
    return ee_ge_terrain_intersects(&theView, GERust(inLine), &outDistance);
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
// and returns their bounding box (the hull, its scan lines and the clipping are Rust: ee_ge terrain_volume.rs; the
// original's U2DConvexHull<float> instantiation went with them)
void GETerrainMesh::GetTileRowsInVolume(const std::vector<GE3DPoint>& inVolume,
                                        std::vector<std::pair<long, std::pair<long, long> > >& outRows,
                                        U2DPoint<long>& outMinimum, U2DPoint<long>& outMaximum)
{
    std::vector<long> theRows(3 * this->mTileRowCount);   // at most one row per y
    long theBounds[4];
    size_t theCount = ee_ge_terrain_rows_in_volume(inVolume.empty() ? NULL : GERust(inVolume[0]), inVolume.size(),
                                                   reinterpret_cast<const EeGeTmRow*>(this->mTileRows),
                                                   this->mTileRowCount, theRows.data(), theBounds);
    for (size_t i = 0; i < theCount; i++)
        outRows.push_back(std::make_pair(theRows[3 * i], std::make_pair(theRows[3 * i + 1], theRows[3 * i + 2])));
    outMinimum.mX = theBounds[0];
    outMinimum.mY = theBounds[1];
    outMaximum.mX = theBounds[2];
    outMaximum.mY = theBounds[3];
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
    const std::vector<unsigned long>& theSet = this->mRegisteredColorSets[this->mWaterColorSetID];
    unsigned long theIndex =
        ee_ge_terrain_water_color_index(this->mWaterTextureScale, theRow.mPoints[inX - theRow.mStart].mZ, theSet.size());
    theRow.mPoints[inX - theRow.mStart].mColorTableIndex = (unsigned char)theSet[theIndex];
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
    GETMMaterial theMaterial;
    GETextureSet* theTextureSet;
    unsigned long theWidth;
    unsigned long theHeight;
    unsigned long i;
    unsigned long j;
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
    // the 33 x 33 grid's random texture indices (a fresh URandom, seed 1): Rust (ee_ge terrain_mesh.rs)
    ee_ge_terrain_water_grid((long)(theHeight * theWidth), &this->mWaterTextureIndices[0][0]);
    for (j = 0; j < this->mPointRowCount; j++) {
        for (i = 0; i < this->mPointRows[j].mCount; i++) {
            if (this->mPointRows[j].mPoints[i].mZ < 0.0f)
                this->SetWaterColorSet(this->mPointRows[j].mStart + i, j);
        }
    }
}

// (0x1002b232 DoesLineIntersectTile: Intersects(line)'s helper, in Rust)


// FUNCTION: LLE 0x1002b507
// Sets each visible point's fog visibility from its four tiles' viewer counts (0.125 for one, 0.25 for more; 1 when
// inAllVisible) and recomputes the visible water and space shares
void GETerrainMesh::UpdateFogOfWar(bool inAllVisible)
{
    EeGeTerrainView theView = GERustTerrainView(*this);
    EeGeTerrainFog theFog = GERustTerrainLayout::Fog(*this, inAllVisible);
    ee_ge_terrain_fog(&theView, &theFog, &this->mVisibleWaterTilePercentage, &this->mVisibleSpaceTilePercentage);
}

// (sWaterElapsedTime 0x1010fed0 was the last step's elapsed time, read by nothing else)
static unsigned long sWaterLastTime;      // 0x1010fed4

// FUNCTION: LLE 0x1002b83f
// Advances the water animation by the whole periods elapsed since the last step (the step is Rust: ee_ge
// terrain_mesh.rs, water_animation_step) and points every water material at its new frame
void GETerrainMesh::UpdateWaterAnimation()
{
    unsigned long theFrameCount = this->mWaterAnimationMaterialList.size();
    GETextureSet* theTextureSet = this->mRegisteredTextureSets[this->mGlobalWaterSetID];
    unsigned long i;
    GETexture* theTexture;
    if (!ee_ge_terrain_water_step(TMGetTime(), this->mWaterAnimationPeriod, theFrameCount, &this->mWaterAnimationFrame,
                                  &sWaterLastTime))
        return;
    for (i = 0; i < theFrameCount; i++) {
        theTexture = theTextureSet->GetTexture((this->mWaterAnimationFrame + i) % theFrameCount, 1);
        this->mWaterMaterialList[i].mTexture = theTexture;
        this->mWaterAnimationMaterialList[i].mTexture = theTexture;
    }
}

// (sCliffNoise 0x100d9760, AddCliffPolys 0x1002b8e8, AddTilePolys 0x1002bd08, AddWaterPolys 0x1002bf5c, the
// visible-edge statics 0x1010fdd8.. with their init chains 0x1002c1b0..0x1002c264, sShorelines 0x10097bf8,
// sCornerTiles 0x10097f78, sCornerNeighbours 0x10097f98, GETMTileLayer, TrackVisibleEdges 0x1002d082 and
// TrackVisibleSpaceEdges 0x1002d222 are Rust: ee_ge terrain_visible.rs)

// FUNCTION: LLE 0x1002c264
// Returns whether a tile is a space tile; false off the map
bool GETerrainMesh::GetIsSpaceTile(unsigned long inX, unsigned long inY) const
{
    if (!this->GetIsValidTile(inX, inY))
        return false;
    return (this->mTileRows[inY].mTiles[inX - this->mTileRows[inY].mStart].mWaterInfo & kWaterInfoSpace) != 0;
}

// FUNCTION: LLE 0x1002c299
// Rebuilds the visible terrain for the camera: the plan (which tiles, the blended texture-layer, space shoreline,
// cliff and water polygons, the map-edge skirts; the materials as textures) is Rust (ee_ge terrain_visible.rs); this
// releases the last frame's materials, rebuilds the material lists from the plan's textures, creates the skirt and
// cliff render vertices and fills the polygon arrays from the plan's vertex references
void GETerrainMesh::UpdateVisibleTerrain(const GETransformation& inCamera, GEViewport& inViewport,
                                         unsigned long inWidth, unsigned long inHeight, unsigned long inScreen,
                                         bool inOrthographic)
{
    std::map<GETexture*, unsigned long>::iterator theIterator;
    std::vector<EeGeTextureSetView> theSets(this->mRegisteredTextureSets.size());
    std::vector<std::vector<uintptr_t> > theSetTextures(this->mRegisteredTextureSets.size());
    std::vector<uintptr_t> theCliffs(this->mSpaceCliffTextures.size());
    EeGeVisibleInput theInput;
    EeGeVisibleOut theOut;
    GETMMaterial theMaterial;
    unsigned long i;
    unsigned long j;
    size_t k;
    for (i = 0; i < theSets.size(); i++) {
        GETextureSet* theSet = this->mRegisteredTextureSets[i];
        unsigned long theWidth = theSet->GetNumTexturesInWidth();
        unsigned long theHeight = theSet->GetNumTexturesInHeight();
        theSetTextures[i].resize(theWidth * theHeight);
        for (j = 0; j < theWidth * theHeight; j++)
            theSetTextures[i][j] = reinterpret_cast<uintptr_t>(theSet->GetTextureAt(j / theHeight, j % theHeight));
        theSets[i].textures = theSetTextures[i].data();
        theSets[i].width = theWidth;
        theSets[i].height = theHeight;
        theSets[i].coverage = theSet->GetTextureTileCoverage();
    }
    for (k = 0; k < theCliffs.size(); k++)
        theCliffs[k] = reinterpret_cast<uintptr_t>(this->mSpaceCliffTextures[k]);
    theInput.view = GERustTerrainView(*this);
    theInput.point_row_count = this->mPointRowCount;
    theInput.left_edge = this->mLeftEdge;
    theInput.top_edge = this->mTopEdge;
    theInput.right_edge = this->mRightEdge;
    theInput.bottom_edge = this->mBottomEdge;
    theInput.space_tile_count = this->mSpaceTileCount;
    theInput.texture_sets = theSets.data();
    theInput.texture_set_count = theSets.size();
    theInput.space_cliff_textures = theCliffs.data();
    theInput.space_cliff_texture_count = theCliffs.size();
    theInput.water_texture_indices = &this->mWaterTextureIndices[0][0];
    theInput.side = GETM_VISIBLE_POINTS;
    EeGeVisiblePlan* thePlan = ee_ge_terrain_visible_plan(&theInput, GERust(inViewport), inOrthographic, &theOut);
    memcpy(this->mTerrainPointVisibility, theOut.terrain_visible, sizeof(this->mTerrainPointVisibility));
    memcpy(this->mWaterPointVisibility, theOut.water_visible, sizeof(this->mWaterPointVisibility));
    this->mVisiblePolyCount = theOut.visible_poly_count;
    this->mNumVisibleTiles = theOut.num_visible_tiles;
    this->mVisibleTileRows.clear();
    for (k = 0; k < theOut.row_count; k++)
        this->mVisibleTileRows.push_back(
            std::make_pair(theOut.rows[3 * k], std::make_pair(theOut.rows[3 * k + 1], theOut.rows[3 * k + 2])));
    this->mVisibleBottomLeftX = theOut.bottom_left_x;
    this->mVisibleBottomLeftY = theOut.bottom_left_y;
    this->mVisibleTileWidth = theOut.tile_width;
    this->mVisibleTileHeight = theOut.tile_height;
    // the last frame's materials released, the lists rebuilt (the water list from the animation frames)
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
    // the terrain materials: the plan's textures in order (the map keeps the first entry of a repeated texture, as
    // the original's insert did), each referenced once per entry
    for (k = 0; k < theOut.material_count; k++) {
        theMaterial.mTexture = reinterpret_cast<GETexture*>(theOut.materials[k]);
        this->mTerrainMaterialMap.insert(std::pair<GETexture* const, unsigned long>(theMaterial.mTexture, k));
        this->mTerrainMaterialList.push_back(theMaterial);
        if (theMaterial.mTexture != NULL)
            theMaterial.mTexture->Reference();
    }
    // the skirt vertices (black, fully visible) and the cliff vertices (following a terrain vertex's visibility)
    for (k = 0; k < theOut.edge_count; k++) {
        this->mEdgeWorldPoints.push_back(*reinterpret_cast<const GE3DPoint*>(&theOut.edge_points[k]));
        GETMRenderVertex* theVertex = new GETMRenderVertex;
        theVertex->mColor.SetRed(0.0f);
        theVertex->mColor.SetGreen(0.0f);
        theVertex->mColor.SetBlue(0.0f);
        theVertex->mVisibility = 1.0f;
        theVertex->mClipFlags = 0x3f;
        this->mEdgeRenderVertices.push_back(theVertex);
    }
    for (k = 0; k < theOut.cliff_count; k++) {
        this->mCliffWorldPoints.push_back(*reinterpret_cast<const GE3DPoint*>(&theOut.cliff_points[k]));
        GETMRenderVertex* theVertex = new GETMRenderVertex;
        theVertex->mVisibility = -1.0f;
        theVertex->mLinkedVisibility =
            &this->mTerrainRenderVertices[theOut.cliff_links[2 * k]][theOut.cliff_links[2 * k + 1]].mVisibility;
        theVertex->mClipFlags = 0x3f;
        this->mCliffRenderVertices.push_back(theVertex);
    }
    // the polygons, into the arrays and their materials' lists (port: a frame past an array's capacity stops
    // there, where the original wrote past it)
    size_t theTerrainCount = theOut.terrain_polygon_count;
    if (theTerrainCount > sizeof(this->mTerrainPolygons) / sizeof(GETMPolygon))
        theTerrainCount = sizeof(this->mTerrainPolygons) / sizeof(GETMPolygon);
    for (k = 0; k < theTerrainCount; k++) {
        const EeGeVisiblePolygon& thePlanned = theOut.terrain_polygons[k];
        GETMPolygon& thePolygon = this->mTerrainPolygons[k];
        this->FillPolygon(thePlanned, thePolygon);
        this->mTerrainMaterialList[thePlanned.material].mPolygons.push_back(&thePolygon);
    }
    size_t theWaterCount = theOut.water_polygon_count;
    if (theWaterCount > sizeof(this->mWaterPolygons) / sizeof(GETMPolygon))
        theWaterCount = sizeof(this->mWaterPolygons) / sizeof(GETMPolygon);
    for (k = 0; k < theWaterCount; k++) {
        const EeGeVisiblePolygon& thePlanned = theOut.water_polygons[k];
        GETMPolygon& thePolygon = this->mWaterPolygons[k];
        this->FillPolygon(thePlanned, thePolygon);
        if (thePlanned.material < this->mWaterMaterialList.size())
            this->mWaterMaterialList[thePlanned.material].mPolygons.push_back(&thePolygon);
    }
    ee_ge_terrain_visible_free(thePlan);
    for (i = 0; i < this->mTerrainMaterialList.size(); i++) {
        this->mTerrainMaterialList[i].mStripIndices.resize(this->mTerrainMaterialList[i].mPolygons.size() + 2);
        this->mTerrainMaterialList[i].mIndices.resize(this->mTerrainMaterialList[i].mPolygons.size() + 2);
    }
    for (i = 0; i < this->mWaterMaterialList.size(); i++) {
        this->mWaterMaterialList[i].mStripIndices.resize(this->mWaterMaterialList[i].mPolygons.size() + 2);
        this->mWaterMaterialList[i].mIndices.resize(this->mWaterMaterialList[i].mPolygons.size() + 2);
    }
}

// A planned polygon into a polygon of the arrays: its corners as pointers (a terrain or water render vertex, a
// skirt or a cliff vertex), its alphas, and its texture coordinates when the plan set them (a skirt polygon keeps
// its slot's, as the original did)
void GETerrainMesh::FillPolygon(const EeGeVisiblePolygon& inPlanned, GETMPolygon& outPolygon)
{
    for (int v = 0; v < 3; v++) {
        const EeGeVisibleVertex& theVertex = inPlanned.vertices[v];
        switch (theVertex.kind) {
        case 0:
            outPolygon.mVertices[v] = &this->mTerrainRenderVertices[theVertex.a][theVertex.b];
            break;
        case 1:
            outPolygon.mVertices[v] = &this->mWaterRenderVertices[theVertex.a][theVertex.b];
            break;
        case 2:
            outPolygon.mVertices[v] = this->mEdgeRenderVertices[theVertex.a];
            break;
        default:
            outPolygon.mVertices[v] = this->mCliffRenderVertices[theVertex.a];
            break;
        }
        outPolygon.mAlpha[v] = inPlanned.alpha[v];
        if (inPlanned.uv_set)
            outPolygon.mUV[v] = GETextureCoordinate(inPlanned.uv[v][0], inPlanned.uv[v][1]);
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
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    long theResult = ee_ge_terrain_load_size(&theIO, &this->mPlayableWidth, &this->mPlayableHeight);
    if (theResult != 0)
        return theResult;
    this->CreateFlatMesh(this->mPlayableWidth, this->mPlayableHeight);
    unsigned char theValidSets[256];
    for (unsigned long i = 0; i < 256; i++)
        theValidSets[i] = i < this->mRegisteredTextureSets.size() && this->mRegisteredTextureSets[i] != NULL;
    EeGeTerrainLoad theState;
    theState.space_tile_count = this->mSpaceTileCount;
    theState.minimum_z = this->mMinimumZ;
    theState.maximum_z = this->mMaximumZ;
    theState.fractal_seed = this->mFractalSeed;
    theState.compute_fractal = false;
    EeGeTerrainView theView = GERustTerrainView(*this);
    theResult = ee_ge_terrain_load_body(&theIO, &theView, this->mPointRowCount, theValidSets, &theState);
    this->mSpaceTileCount = theState.space_tile_count;
    this->mMinimumZ = theState.minimum_z;
    this->mMaximumZ = theState.maximum_z;
    this->mFractalSeed = theState.fractal_seed;
    if (theState.compute_fractal)
        this->mFractal->ComputeFractal(0.0f, 10.5f, this->mFractalSeed);
    for (unsigned long j = 0; j < this->mPointRowCount; j++) {
        for (unsigned long i = 0; i < this->mPointRows[j].mCount; i++)
            this->ComputeVertexNormal(this->mPointRows[j].mStart + i, j);
    }
    return theResult;
}

// FUNCTION: LLE 0x1002d656
// Writes the map size, each tile's texture set and water info, each point's height and colour, and the fractal seed;
// returns the first write error
long GETerrainMesh::Save(FSFile& inFile) const
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    EeGeTerrainView theView = GERustTerrainView(*this);
    return ee_ge_terrain_save(&theIO, &theView, this->mPointRowCount, this->mPlayableWidth, this->mPlayableHeight,
                              this->mFractalSeed);
}
