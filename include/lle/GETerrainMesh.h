// C:\EEX\Low-Level Engine\Graphics Engine\GETerrainMesh.h (RECONSTRUCTION). The diamond-shaped terrain height field,
// its tile/point rows and the per-frame visible polygon lists. GETerrainMesh.cpp (0x10026fde..0x10032ae2) holds the
// out-of-line members; the inline members below are emitted first by GEBoundingBox.cpp (0x100125b7..0x10012ae4, in
// declaration order), with the implicit copy ctor (0x12ae4) and operator= (0x12e85) after them. Offsets are proven
// by those bodies, the ctor (0x273c9) and the dtor (0x2766d); member names are provisional unless an export names
// them. Requires lle/LowLevelEngine.h, GE3DPoint.h, GEVector.h, GEColor.h, GETextureCoordinate.h, GE3DPlane.h.
#ifndef LLE_GETERRAINMESH_H
#define LLE_GETERRAINMESH_H

// port: the visible area's point arrays, 101 x 101 in the original (enough for its closest-to-farthest zoom); larger
// for the farther zoom-out (EECameraControl gCameraMaxZoom). UpdateVisibleTerrain keeps the view inside them.
#define GETM_VISIBLE_POINTS 257

#include <map>
#include <vector>
#include <utility>
#include "lle/GETerrainMeshFlags.h"

class FSFile;
class GE3DLine;
class GETexture;
class GETextureSet;
class GETransformation;
class GEViewport;
class U2DFractal;
template <class T> class U2DPoint;

class LLE_API GETerrainMesh {               // sizeof 0x1955b0
public:
    // One map vertex: height, quantized normal, colour table index. sizeof 0xc
    struct GETMPoint {
        GETMPoint() : mZ(0.0f), mNormal(NULL), mColorTableIndex(0) {}

        float mZ;                           // +0x00
        GEVector* mNormal;                  // +0x04
        unsigned char mColorTableIndex;     // +0x08
    };

    // One map tile. mWaterInfo: bit 0 water, bits 1..4 shoreline directions, bit 7 space. sizeof 4
    struct GETMTile {
        GETMTile() : mReferenceCount(0), mTextureSetID(0), mWaterInfo(0) {}

        unsigned short mReferenceCount;     // +0x00
        unsigned char mTextureSetID;        // +0x02
        unsigned char mWaterInfo;           // +0x03
    };

    // A projected vertex the rasterizer fills in (TransformToCamera). sizeof 0x28
    struct GETMRenderVertex {
        GE3DPoint mPoint;                   // +0x00
        GEColor mColor;                     // +0x0c
        float mVisibility;                  // +0x18 fog of war (UpdateFogOfWar)
        float mFog;                         // +0x1c
        unsigned long mClipFlags;           // +0x20
        float* mLinkedVisibility;           // +0x24 cliff vertices: the terrain vertex whose mVisibility they copy
    };

    // A visible triangle. sizeof 0x30
    struct GETMPolygon {
        GETMRenderVertex* mVertices[3];     // +0x00
        float mAlpha[3];                    // +0x0c
        GETextureCoordinate mUV[3];         // +0x18
    };

    // A tile's four corners, in the order the Add*Polys members index them. sizeof 0x40
    struct GETMQuad {
        GETMRenderVertex* mVertices[4];     // +0x00
        float mAlpha[4];                    // +0x10
        GETextureCoordinate mUV[4];         // +0x20
    };

    // The polygons drawn with one texture this frame. sizeof 0x34
    struct GETMMaterial {
        GETMMaterial() { this->mTexture = NULL; }
        ~GETMMaterial() { this->mPolygons.clear(); }

        GETexture* mTexture;                // +0x00
        std::vector<GETMPolygon*> mPolygons;           // +0x04
        // +0x14/+0x24: resized to polygon count + 2; NULL-terminated polygon lists whose last element holds the
        // polygon count. DX7HRDisplay.dll's DrawWater (0x10006f40) passes both to DrawTerrainMaterial
        // (?DrawTerrainMaterial@DX7Rasterizer@@MAEJPAVGEViewport@@AAV?$vector@PAUGETMPolygon@GETerrainMesh@@...),
        // after turning D3D clipping off for +0x14 and on for +0x24. Names are provisional.
        std::vector<GETMPolygon*> mIndices;            // +0x14
        std::vector<GETMPolygon*> mStripIndices;       // +0x24
    };

    GETerrainMesh();
    virtual ~GETerrainMesh();

    void ComputeShorelines(unsigned long inX, unsigned long inY);
    void ComputeVertexNormal(unsigned long inX, unsigned long inY);
    void CreateFlatMesh(long inWidth, long inHeight);
    void DereferenceTile(unsigned long inX, unsigned long inY) { this->GetTile(inX, inY)->mReferenceCount--; }
    const GE3DPlane& GetBackPlane() const { return this->mBackPlane; }
    void GetClosestTile(const GE3DLine& inLine, long& outX, long& outY) const;
    void GetClosestTile(const GE3DPoint& inPoint, long& outX, long& outY) const;
    unsigned char GetColorTableIndex(unsigned long inX, unsigned long inY) const
        { return this->mPointRows[inY].mPoints[inX - this->mPointRows[inY].mStart].mColorTableIndex; }
    std::vector<GETMRenderVertex*>& GetEdgeRenderVertices() { return this->mEdgeRenderVertices; }
    std::vector<GE3DPoint>& GetEdgeWorldPoints() { return this->mEdgeWorldPoints; }
    std::vector<GETMRenderVertex*>& GetCliffRenderVertices() { return this->mCliffRenderVertices; }
    std::vector<GE3DPoint>& GetCliffWorldPoints() { return this->mCliffWorldPoints; }
    const GE3DPlane& GetFrontPlane() const { return this->mFrontPlane; }
    unsigned long GetGlobalWaterSetID() const { return this->mGlobalWaterSetID; }
    bool GetIsValidPoint(unsigned long inX, unsigned long inY) const
        { return inY < this->mPointRowCount && inX - this->mPointRows[inY].mStart < this->mPointRows[inY].mCount; }
    bool GetIsValidTile(unsigned long inX, unsigned long inY) const
        { return inY < this->mTileRowCount && inX - this->mTileRows[inY].mStart < this->mTileRows[inY].mCount; }
    bool GetIsWaterTile(unsigned long inX, unsigned long inY) const   // row reference: ComputeShorelines 0x1002795f
    {
        GETMTileRow& theRow = this->mTileRows[inY];
        return (theRow.mTiles[inX - theRow.mStart].mWaterInfo & kWaterInfoWaterMask) != 0;
    }
    bool GetIsSpaceMap() const { return this->mSpaceTileCount > 0; }
    bool GetIsSpaceTile(unsigned long inX, unsigned long inY) const;
    bool GetIsEdgeTile(long inX, long inY) const
    {
        return inX + inY == this->mLeftEdge || inY - inX == this->mTopEdge || inX + inY == this->mRightEdge ||
               inX - inY == this->mBottomEdge;
    }
    bool GetIsLeftEdgeTile(long inX, long inY) const { return inX + inY == this->mLeftEdge; }
    bool GetIsTopEdgeTile(long inX, long inY) const { return inY - inX == this->mTopEdge; }
    bool GetIsRightEdgeTile(long inX, long inY) const { return inX + inY == this->mRightEdge; }
    bool GetIsBottomEdgeTile(long inX, long inY) const { return inX - inY == this->mBottomEdge; }
    const GE3DPlane& GetLeftPlane() const { return this->mLeftPlane; }
    float GetMaximumZ() const { return this->mMaximumZ; }
    void GetMeshNormal(float inX, float inY, GEVector& outNormal) const;
    float GetMeshZ(float inX, float inY) const;
    float GetMinimumZ() const { return this->mMinimumZ; }
    unsigned long GetNumVisibleTiles() const { return this->mNumVisibleTiles; }
    unsigned long GetNumVisibleUnitTiles() const { return this->mNumVisibleTiles; }
    unsigned long GetNumVisibleTileRows() const { return this->mVisibleTileRows.size(); }
    GETextureSet* GetRegisteredTextureSet(unsigned long inID) const { return this->mRegisteredTextureSets[inID]; }
    const GE3DPlane& GetRightPlane() const { return this->mRightPlane; }
    unsigned long GetTerrainMaterialCount() const { return this->mTerrainMaterialList.size(); }
    std::vector<GETMMaterial>& GetTerrainMaterialList() { return this->mTerrainMaterialList; }
    std::map<GETexture*, unsigned long>& GetTerrainMaterialMap() { return this->mTerrainMaterialMap; }
    bool (&GetTerrainPointVisibilityArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS] { return this->mTerrainPointVisibility; }
    GETMRenderVertex (&GetTerrainRenderVertexArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS] { return this->mTerrainRenderVertices; }
    void GetTerrainVisibleBottomLeftIndices(unsigned long& outX, unsigned long& outY) const
    {
        outX = this->mVisibleBottomLeftX;
        outY = this->mVisibleBottomLeftY;
    }
    void GetTerrainVisibleTileDimensions(unsigned long& outWidth, unsigned long& outHeight) const
    {
        outWidth = this->mVisibleTileWidth;
        outHeight = this->mVisibleTileHeight;
    }
    unsigned long GetTextureSetID(unsigned long inX, unsigned long inY) const
        { return this->mTileRows[inY].mTiles[inX - this->mTileRows[inY].mStart].mTextureSetID; }
    void GetTileRowsInVolume(const std::vector<GE3DPoint>& inVolume,
                             std::vector<std::pair<long, std::pair<long, long> > >& outRows,
                             U2DPoint<long>& outMinimum, U2DPoint<long>& outMaximum);
    const GEColor& GetVertexColor(unsigned long inX, unsigned long inY) const
        { return this->mColorTable[this->mPointRows[inY].mPoints[inX - this->mPointRows[inY].mStart].mColorTableIndex]; }
    GEVector* GetVertexNormal(unsigned long inX, unsigned long inY) const
        { return this->mPointRows[inY].mPoints[inX - this->mPointRows[inY].mStart].mNormal; }
    unsigned long GetVisiblePolyCount() const { return this->mVisiblePolyCount; }
    void GetVisibleTileRow(unsigned long inIndex, long& outY, long& outStartX, long& outEndX) const
    {
        outY = this->mVisibleTileRows[inIndex].first;
        outStartX = this->mVisibleTileRows[inIndex].second.first;
        outEndX = this->mVisibleTileRows[inIndex].second.second;
    }
    float GetVisibleSpaceTilePercentage() const { return this->mVisibleSpaceTilePercentage; }
    float GetVisibleWaterTilePercentage() const { return this->mVisibleWaterTilePercentage; }
    const GEColor& GetWaterColor(unsigned long inX, unsigned long inY) const { return this->mWaterColor; }
    unsigned char GetWaterInfo(unsigned long inX, unsigned long inY) const
        { return this->mTileRows[inY].mTiles[inX - this->mTileRows[inY].mStart].mWaterInfo; }
    unsigned long GetWaterMaterialCount() const { return this->mWaterMaterialList.size(); }
    std::vector<GETMMaterial>& GetWaterMaterialList() { return this->mWaterMaterialList; }
    bool (&GetWaterPointVisibilityArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS] { return this->mWaterPointVisibility; }
    GETMRenderVertex (&GetWaterRenderVertexArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS] { return this->mWaterRenderVertices; }
    // through a row reference: the Generate*EdgePolygons loops (0x10027f81..) add the strength-reduced row offset
    // to mPointRows, not the reverse
    float GetZ(unsigned long inX, unsigned long inY) const
    {
        GETMPointRow& theRow = this->mPointRows[inY];
        return theRow.mPoints[inX - theRow.mStart].mZ;
    }
    bool Intersects(const GE3DLine& inLine, float& outDistance) const;
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    // row and tile references: the GetTile form is never inlined by the exe (EEPPlayerEvent::Activate 0x16e3c3
    // expands it)
    void ReferenceTile(unsigned long inX, unsigned long inY)
    {
        GETMTileRow& theRow = this->mTileRows[inY];
        GETMTile& theTile = theRow.mTiles[inX - theRow.mStart];
        if (theTile.mReferenceCount > 0)
            theTile.mReferenceCount++;
        else
            theTile.mReferenceCount = 2;
    }
    long RegisterColorSet(std::vector<unsigned long>& inColorSet, unsigned long& outID);
    long RegisterTextureSet(GETextureSet* inTextureSet, unsigned long& outID);
    long Save(FSFile& inFile) const;
    void SetColorFromColorTableIndex(unsigned long inX, unsigned long inY, unsigned char inIndex);
    void SetColorSet(unsigned long inX, unsigned long inY, unsigned long inColorSetID);
    void SetColorTable(std::vector<GEColor>& inColorTable);
    void SetGlobalWaterState(unsigned long inWaterSetID, unsigned long inColorSetID, unsigned long inAnimationPeriod,
                             float inTextureScale);
    void SetIsSpace(unsigned long inX, unsigned long inY, bool inIsSpace);
    void SetIsWater(unsigned long inX, unsigned long inY, bool inIsWater)
    {
        if (this->mSpaceTileCount <= 0) {
            GETMTile* theTile = this->GetTile(inX, inY);
            theTile->mWaterInfo &= kWaterInfoSpace;
            if (inIsWater)
                theTile->mWaterInfo |= kWaterInfoWater;
        }
    }
    unsigned int NumSpaceCliffTextures() { return this->mSpaceCliffTextures.size(); }
    void SetSpaceCliffTextures(std::vector<GETexture*>& inTextures) { this->mSpaceCliffTextures = inTextures; }
    void SetTextureSet(unsigned long inX, unsigned long inY, unsigned long inTextureSetID)
        { this->GetTile(inX, inY)->mTextureSetID = (unsigned char)inTextureSetID; }
    void SetTileReferenceCount(unsigned long inX, unsigned long inY, unsigned long inCount)
        { this->GetTile(inX, inY)->mReferenceCount = (unsigned short)inCount; }
    void SetVisibleWaterTilePercentage(float inPercentage) { this->mVisibleWaterTilePercentage = inPercentage; }
    void SetWaterColorSet(unsigned long inX, unsigned long inY);
    void SetWaterInfo(unsigned long inX, unsigned long inY, unsigned char inWaterInfo)
    {
        GETMTile* theTile = this->GetTile(inX, inY);
        theTile->mWaterInfo = (theTile->mWaterInfo & kWaterInfoSpace) | inWaterInfo;
    }
    void SetZ(unsigned long inX, unsigned long inY, float inZ)
    {
        this->mPointRows[inY].mPoints[inX - this->mPointRows[inY].mStart].mZ = inZ;
        this->mMinimumZ = min(this->mMinimumZ, inZ);
        this->mMaximumZ = max(this->mMaximumZ, inZ);
    }
    void UpdateFogOfWar(bool inVisible);
    void UpdateVisibleTerrain(const GETransformation& inCamera, GEViewport& inViewport, unsigned long inWidth,
                              unsigned long inHeight, unsigned long inScreen, bool inOrthographic);
    void UpdateWaterAnimation();

    // A row of points (start x, count, points). sizeof 0xc
    struct LLE_API GETMPointRow {
        GETMPointRow()
        {
            this->mPoints = NULL;
            this->mStart = 0;
            this->mCount = 0;
        }
        ~GETMPointRow() { this->DeallocateInternalData(); }
        void DeallocateInternalData();
        void Initialize(unsigned long inStart, unsigned long inEnd, GEVector* inNormal);

        unsigned long mStart;               // +0x00
        unsigned long mCount;               // +0x04
        GETMPoint* mPoints;                 // +0x08
    };

    // A row of tiles (start x, count, tiles). sizeof 0xc
    struct LLE_API GETMTileRow {
        GETMTileRow()
        {
            this->mTiles = NULL;
            this->mStart = 0;
            this->mCount = 0;
        }
        ~GETMTileRow() { this->DeallocateInternalData(); }
        void DeallocateInternalData();
        void Initialize(unsigned long inStart, unsigned long inEnd);

        unsigned long mStart;               // +0x00
        unsigned long mCount;               // +0x04
        GETMTile* mTiles;                   // +0x08
    };

private:
    friend struct GERustTerrainLayout;      // the Rust view of the rows (Graphics Engine/GERustTerrain.h)
    void AddCliffPolys(GETMPolygon*& ioPolygon, int inX, int inY, const float* inHeights, unsigned long inMaterial);
    void AddDenormalizedPointToList(const GEViewport& inViewport, GE3DPoint& inPoint,
                                    std::vector<U2DPoint<float> >& outPoints, bool inOrthographic);
    void AddTilePolys(int inX, int inY, GETMQuad& inQuad, GETMPolygon*& ioPolygon, unsigned long inMaterial);
    void AddWaterPolys(int inX, int inY, GETMQuad& inQuad, GETMPolygon*& ioPolygon, unsigned long inMaterial);
    void CreateNewEdgeVertex(long inX, long inY, float inZ);
    void DefineNewEdgePolygon(unsigned long inBase, unsigned long inVertex0, unsigned long inVertex1, unsigned long inVertex2,
                              GETMPolygon*& ioPolygon);
    // (DoesLineIntersectTile, Intersects(line)'s helper, is Rust: ee_ge terrain.rs)
    void GenerateBackEdgePolygons(const GEViewport& inViewport, U2DPoint<long> (&ioEdge)[2],
                                  GETMPolygon*& ioPolygon, unsigned long& ioVertexBase);
    void GenerateCornerBackLeftPolygons(const GEViewport& inViewport, U2DPoint<long> (&inEdgeA)[2], bool inHasEdgeA,
                                        U2DPoint<long> (&inEdgeB)[2], bool inHasEdgeB, GETMPolygon*& ioPolygon,
                                        unsigned long& ioVertexBase);
    void GenerateCornerBackRightPolygons(const GEViewport& inViewport, U2DPoint<long> (&inEdgeA)[2],
                                         bool inHasEdgeA, U2DPoint<long> (&inEdgeB)[2], bool inHasEdgeB,
                                         GETMPolygon*& ioPolygon, unsigned long& ioVertexBase);
    void GenerateCornerFrontLeftPolygons(const GEViewport& inViewport, U2DPoint<long> (&inEdgeA)[2],
                                         bool inHasEdgeA, U2DPoint<long> (&inEdgeB)[2], bool inHasEdgeB,
                                         GETMPolygon*& ioPolygon, unsigned long& ioVertexBase);
    void GenerateCornerFrontRightPolygons(const GEViewport& inViewport, U2DPoint<long> (&inEdgeA)[2],
                                          bool inHasEdgeA, U2DPoint<long> (&inEdgeB)[2], bool inHasEdgeB,
                                          GETMPolygon*& ioPolygon, unsigned long& ioVertexBase);
    void GenerateFrontEdgePolygons(const GEViewport& inViewport, U2DPoint<long> (&ioEdge)[2],
                                   GETMPolygon*& ioPolygon, unsigned long& ioVertexBase);
    void GenerateLeftEdgePolygons(const GEViewport& inViewport, U2DPoint<long> (&ioEdge)[2],
                                  GETMPolygon*& ioPolygon, unsigned long& ioVertexBase);
    void GenerateRightEdgePolygons(const GEViewport& inViewport, U2DPoint<long> (&ioEdge)[2],
                                   GETMPolygon*& ioPolygon, unsigned long& ioVertexBase);
    GETMTile* GetTile(unsigned long inX, unsigned long inY)
        { return &this->mTileRows[inY].mTiles[inX - this->mTileRows[inY].mStart]; }
    void InitializePlanes();

    std::map<GETexture*, unsigned long> mTerrainMaterialMap;          // +0x04 texture -> mTerrainMaterialList index
    std::vector<GETMMaterial> mTerrainMaterialList;                   // +0x10
    std::vector<std::pair<long, std::pair<long, long> > > mVisibleTileRows;   // +0x20 (y, (first x, last x))
    std::vector<GETMMaterial> mWaterAnimationMaterialList;            // +0x30 one per water texture frame
    std::map<GETexture*, unsigned long> mWaterMaterialMap;            // +0x40
    std::vector<GETMMaterial> mWaterMaterialList;                     // +0x4c
    std::vector<GETextureSet*> mRegisteredTextureSets;                // +0x5c
    std::vector<std::vector<unsigned long> > mRegisteredColorSets;    // +0x6c
    std::vector<GETMRenderVertex*> mEdgeRenderVertices;               // +0x7c
    std::vector<GE3DPoint> mEdgeWorldPoints;                          // +0x8c
    std::vector<GETMRenderVertex*> mCliffRenderVertices;              // +0x9c
    std::vector<GE3DPoint> mCliffWorldPoints;                         // +0xac
    GEColor mWaterColor;                    // +0xbc
    GEColor mColorTable[256];               // +0xc8
    GE3DPlane mFrontPlane;                  // +0xcc8
    GE3DPlane mBackPlane;                   // +0xce0
    GE3DPlane mLeftPlane;                   // +0xcf8
    GE3DPlane mRightPlane;                  // +0xd10
    GETMPolygon mTerrainPolygons[12000];    // +0xd28
    GETMRenderVertex mTerrainRenderVertices[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS];   // +0x8d728
    GETMPolygon mWaterPolygons[5000];       // +0xf1110
    GETMRenderVertex mWaterRenderVertices[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS];     // +0x12ba90
    U2DFractal* mFractal;                   // +0x18f478 colour-set variation
    GETMPointRow* mPointRows;               // +0x18f47c
    GETMTileRow* mTileRows;                 // +0x18f480
    unsigned long mSpaceTileCount;          // +0x18f484
    float mWaterTextureScale;               // +0x18f488 1 / SetGlobalWaterState's scale
    float mVisibleWaterTilePercentage;      // +0x18f48c
    float mVisibleSpaceTilePercentage;      // +0x18f490
    float mMinimumZ;                        // +0x18f494
    float mMaximumZ;                        // +0x18f498
    long mPlayableWidth;                    // +0x18f49c
    long mPlayableHeight;                   // +0x18f4a0
    long mLeftEdge;                         // +0x18f4a4 x + y on the left edge
    long mTopEdge;                          // +0x18f4a8 y - x on the top edge
    long mRightEdge;                        // +0x18f4ac x + y on the right edge
    long mBottomEdge;                       // +0x18f4b0 x - y on the bottom edge
    unsigned long mWaterTextureIndices[33][33];   // +0x18f4b4
    unsigned long mVisibleTileWidth;        // +0x1905b8
    unsigned long mVisibleTileHeight;       // +0x1905bc
    unsigned long mVisibleBottomLeftX;      // +0x1905c0
    unsigned long mVisibleBottomLeftY;      // +0x1905c4
    unsigned long mWaterAnimationFrame;     // +0x1905c8
    unsigned long mGlobalWaterSetID;        // +0x1905cc
    unsigned long mWaterColorSetID;         // +0x1905d0
    unsigned long mWaterAnimationPeriod;    // +0x1905d4
    unsigned long mPointRowCount;           // +0x1905d8
    unsigned long mTileRowCount;            // +0x1905dc
    unsigned long mVisiblePolyCount;        // +0x1905e0
    unsigned long mNumVisibleTiles;         // +0x1905e4
    unsigned long mFractalSeed;             // +0x1905e8
    bool mTerrainPointVisibility[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS];  // +0x1905ec
    bool mWaterPointVisibility[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS];    // +0x192dc5
    std::vector<GETexture*> mSpaceCliffTextures;   // +0x1955a0

    // GETerrainMesh.cpp-only visibility (scheme of GEVector.h/GEWindow.h): every class static data member a unit
    // sees takes a VC6 $E number. Exported ?sPlayableWidthID@GETerrainMesh@@0VUString@@B etc.
#ifdef LOWLEVELENGINE_EXPORTS
    static const UString sPlayableWidthID;  // 0x1010fe50
    static const UString sPlayableHeightID; // 0x1010fe38
    static const UString sTextureSetID;     // 0x1010fe28
    static const UString sWaterInfoID;      // 0x1010fe18
    static const UString sZID;              // 0x1010fe08
    static const UString sFractalSeedID;    // 0x1010fdf8
    static const UString sColorLookupID;    // 0x1010fde8
#endif
};

#endif
