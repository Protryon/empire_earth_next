// C:\EEX\Low-Level Engine\Graphics Engine\GETerrainMesh.h (RECONSTRUCTION). The diamond-shaped terrain height field,
// its tile/point rows and the per-frame visible polygon lists. GETerrainMesh.cpp (0x10026fde..0x10032ae2) holds the
// out-of-line members; the inline members of the original header were emitted first by GEBoundingBox.cpp
// (0x100125b7..0x10012ae4, in declaration order). The mesh is a whole Rust object now (rust/crates/ee_ge tmobj.rs,
// R4.3 eleventh part): every member below forwards to it, and its only member is the object. Requires
// lle/LowLevelEngine.h, GE3DPoint.h, GEVector.h, GEColor.h, GETextureCoordinate.h, GE3DPlane.h.
#ifndef LLE_GETERRAINMESH_H
#define LLE_GETERRAINMESH_H

// port: the visible area's point arrays, 101 x 101 in the original (enough for its closest-to-farthest zoom); larger
// for the farther zoom-out (EECameraControl gCameraMaxZoom). UpdateVisibleTerrain keeps the view inside them.
#define GETM_VISIBLE_POINTS 257

#include <stddef.h>
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
struct EeGeTerrainMesh;   // the mesh (rust/ee_ge.h, ee_ge tmobj.rs)
template <class T> class U2DPoint;

class LLE_API GETerrainMesh {
public:
    // A projected vertex the rasterizer fills in (TransformToCamera); the Rust object's EeGeTmRenderVertex
    struct GETMRenderVertex {
        GE3DPoint mPoint;                   // +0x00
        GEColor mColor;                     // +0x0c
        float mVisibility;                  // +0x18 fog of war (UpdateFogOfWar)
        float mFog;                         // +0x1c
        unsigned long mClipFlags;           // +0x20
        float* mLinkedVisibility;           // +0x24 cliff vertices: the terrain vertex whose mVisibility they copy
    };

    // A visible triangle (EeGeTmPolygon)
    struct GETMPolygon {
        GETMRenderVertex* mVertices[3];     // +0x00
        float mAlpha[3];                    // +0x0c
        GETextureCoordinate mUV[3];         // +0x18
    };

    // A material of this frame as the renderer reads it (DX7Rasterizer DrawTerrain.cpp): its texture and its three
    // polygon lists (the polygons, then the two NULL-terminated lists TransformToCamera fills) (EeGeTmMaterialView)
    struct GETMMaterialView {
        GETexture* mTexture;
        GETMPolygon** mPolygons;
        size_t mPolygonCount;
        GETMPolygon** mIndices;
        size_t mIndicesLength;
        GETMPolygon** mStrips;
        size_t mStripsLength;
    };

    GETerrainMesh();
    virtual ~GETerrainMesh();
    GETerrainMesh(const GETerrainMesh&) = delete;
    GETerrainMesh& operator=(const GETerrainMesh&) = delete;

    void ComputeShorelines(unsigned long inX, unsigned long inY);
    void ComputeVertexNormal(unsigned long inX, unsigned long inY);
    void CreateFlatMesh(long inWidth, long inHeight);
    void DereferenceTile(unsigned long inX, unsigned long inY);
    const GE3DPlane& GetBackPlane() const;
    void GetClosestTile(const GE3DLine& inLine, long& outX, long& outY) const;
    void GetClosestTile(const GE3DPoint& inPoint, long& outX, long& outY) const;
    unsigned char GetColorTableIndex(unsigned long inX, unsigned long inY) const;
    // the skirt (edge) and cliff vertices of this frame: as many world points as render vertices
    unsigned long GetEdgeVertexCount() const;
    GETMRenderVertex* const* GetEdgeRenderVertices() const;
    const GE3DPoint* GetEdgeWorldPoints() const;
    unsigned long GetCliffVertexCount() const;
    GETMRenderVertex* const* GetCliffRenderVertices() const;
    const GE3DPoint* GetCliffWorldPoints() const;
    const GE3DPlane& GetFrontPlane() const;
    unsigned long GetGlobalWaterSetID() const;
    bool GetIsValidPoint(unsigned long inX, unsigned long inY) const;
    bool GetIsValidTile(unsigned long inX, unsigned long inY) const;
    bool GetIsWaterTile(unsigned long inX, unsigned long inY) const;
    bool GetIsSpaceMap() const;
    bool GetIsSpaceTile(unsigned long inX, unsigned long inY) const;
    const GE3DPlane& GetLeftPlane() const;
    void GetMeshNormal(float inX, float inY, GEVector& outNormal) const;
    float GetMeshZ(float inX, float inY) const;
    unsigned long GetNumVisibleTiles() const;
    unsigned long GetNumVisibleTileRows() const;
    GETextureSet* GetRegisteredTextureSet(unsigned long inID) const;
    const GE3DPlane& GetRightPlane() const;
    unsigned long GetTerrainMaterialCount() const;
    // the texture -> terrain material index map, in texture order
    void GetTerrainMaterialMap(std::vector<std::pair<GETexture*, unsigned long> >& outEntries) const;
    // material inIndex of the terrain (or with inWater, the water) material list
    void GetMaterialView(bool inWater, unsigned long inIndex, GETMMaterialView& outView);
    bool (&GetTerrainPointVisibilityArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS];
    GETMRenderVertex (&GetTerrainRenderVertexArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS];
    void GetTerrainVisibleBottomLeftIndices(unsigned long& outX, unsigned long& outY) const;
    void GetTerrainVisibleTileDimensions(unsigned long& outWidth, unsigned long& outHeight) const;
    unsigned long GetTextureSetID(unsigned long inX, unsigned long inY) const;
    const GEColor& GetVertexColor(unsigned long inX, unsigned long inY) const;
    GEVector* GetVertexNormal(unsigned long inX, unsigned long inY) const;
    void GetVisibleTileRow(unsigned long inIndex, long& outY, long& outStartX, long& outEndX) const;
    float GetVisibleSpaceTilePercentage() const;
    float GetVisibleWaterTilePercentage() const;
    const GEColor& GetWaterColor(unsigned long inX, unsigned long inY) const;
    unsigned char GetWaterInfo(unsigned long inX, unsigned long inY) const;
    unsigned long GetWaterMaterialCount() const;
    bool (&GetWaterPointVisibilityArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS];
    GETMRenderVertex (&GetWaterRenderVertexArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS];
    float GetZ(unsigned long inX, unsigned long inY) const;
    bool Intersects(const GE3DLine& inLine, float& outDistance) const;
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    void ReferenceTile(unsigned long inX, unsigned long inY);
    long RegisterColorSet(std::vector<unsigned long>& inColorSet, unsigned long& outID);
    long RegisterTextureSet(GETextureSet* inTextureSet, unsigned long& outID);
    long Save(FSFile& inFile) const;
    void SetColorFromColorTableIndex(unsigned long inX, unsigned long inY, unsigned char inIndex);
    void SetColorSet(unsigned long inX, unsigned long inY, unsigned long inColorSetID);
    void SetColorTable(std::vector<GEColor>& inColorTable);
    void SetGlobalWaterState(unsigned long inWaterSetID, unsigned long inColorSetID, unsigned long inAnimationPeriod,
                             float inTextureScale);
    void SetIsSpace(unsigned long inX, unsigned long inY, bool inIsSpace);
    void SetIsWater(unsigned long inX, unsigned long inY, bool inIsWater);
    unsigned int NumSpaceCliffTextures();
    void SetSpaceCliffTextures(std::vector<GETexture*>& inTextures);
    void SetTextureSet(unsigned long inX, unsigned long inY, unsigned long inTextureSetID);
    void SetTileReferenceCount(unsigned long inX, unsigned long inY, unsigned long inCount);
    void SetWaterColorSet(unsigned long inX, unsigned long inY);
    void SetZ(unsigned long inX, unsigned long inY, float inZ);
    void UpdateFogOfWar(bool inVisible);
    void UpdateVisibleTerrain(const GETransformation& inCamera, GEViewport& inViewport, unsigned long inWidth,
                              unsigned long inHeight, unsigned long inScreen, bool inOrthographic);
    void UpdateWaterAnimation();

private:
    EeGeTerrainMesh* mRust;                 // the whole mesh (ee_ge tmobj.rs)
};

#endif
