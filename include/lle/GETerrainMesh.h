// C:\EEX\Low-Level Engine\Graphics Engine\GETerrainMesh.h (RECONSTRUCTION). The diamond-shaped terrain height field,
// its tile/point rows and the per-frame visible polygon lists. GETerrainMesh.cpp (0x10026fde..0x10032ae2) holds the
// out-of-line members; the inline members of the original header were emitted first by GEBoundingBox.cpp
// (0x100125b7..0x10012ae4, in declaration order). The mesh is a whole Rust object now (rust/crates/ee_render/src/ge tmobj.rs,
// R4.3 eleventh part): every member below forwards to it, and its only member is the object. Requires
// lle/LowLevelEngine.h, GE3DPoint.h (GEVector), GEColor.h, GETextureCoordinate.h, GE3DLine.h (GE3DPlane).
#ifndef LLE_GETERRAINMESH_H
#define LLE_GETERRAINMESH_H

// port: the visible area's point arrays, 101 x 101 in the original (enough for its closest-to-farthest zoom); larger
// for the farther zoom-out (EECameraControl gCameraMaxZoom). UpdateVisibleTerrain keeps the view inside them.
#define GETM_VISIBLE_POINTS EE_GE_TM_SIDE   // 257 (ee_ge tmobj.rs)

#include <stddef.h>
#include <map>
#include <vector>
#include <utility>
#include "rust/render/gestate.h"
#include "lle/GERust.h"
#include "rust/render/ge.h"

#include "lle/LLEForward.h"
struct EeGeTerrainMesh;   // the mesh (rust/ee_ge.h, ee_ge tmobj.rs)

class LLE_API GETerrainMesh {
public:
    // A projected vertex the rasterizer fills in (TransformToCamera: the point, the colour, the fog of war's
    // visibility, the fog, the clip flags; a cliff vertex's terrain visibility it copies) and a visible triangle:
    // the Rust object's records (ee_ge tmobj.rs)
    typedef EeGeTmRenderVertex GETMRenderVertex;
    typedef EeGeTmPolygon GETMPolygon;

    GETerrainMesh();
    // the mesh's Rust object (the DX7 plug-in's terrain draws read its visible frame there: ee_dx7 draw_ffi.rs)
    EeGeTerrainMesh* GetRust() const { return this->mRust; }
    virtual ~GETerrainMesh() { ee_ge_tm_destroy(this->mRust); }

    void ComputeShorelines(unsigned long inX, unsigned long inY) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 7, 0); }
    void ComputeVertexNormal(unsigned long inX, unsigned long inY) { ee_ge_tm_point_edit(this->mRust, inX, inY, 4, 0, 0.0f); }
    void CreateFlatMesh(long inWidth, long inHeight) { ee_ge_tm_create_flat_mesh(this->mRust, inWidth, inHeight); }
    void DereferenceTile(unsigned long inX, unsigned long inY) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 1, 0); }
    const GE3DPlane& GetBackPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 1)); }
    void GetClosestTile(const GE3DLine& inLine, long& outX, long& outY) const { ee_ge_tm_closest_tile_line(this->mRust, GERust(inLine), &outX, &outY); }
    void GetClosestTile(const GE3DPoint& inPoint, long& outX, long& outY) const { ee_ge_tm_closest_tile_point(this->mRust, GERust(inPoint), &outX, &outY); }
    const GE3DPlane& GetFrontPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 0)); }
    unsigned long GetGlobalWaterSetID() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::GlobalWaterSetId); }
    bool GetIsValidPoint(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_valid(this->mRust, inX, inY, false); }
    bool GetIsValidTile(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_valid(this->mRust, inX, inY, true); }
    bool GetIsWaterTile(unsigned long inX, unsigned long inY) const { return (ee_ge_tm_tile(this->mRust, inX, inY)->water_info & kWaterInfoWaterMask) != 0; }
    bool GetIsSpaceMap() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::SpaceTileCount) > 0; }
    bool GetIsSpaceTile(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_space_tile(this->mRust, inX, inY); }
    const GE3DPlane& GetLeftPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 2)); }
    float GetMeshZ(float inX, float inY) const { return ee_ge_tm_mesh_z(this->mRust, inX, inY); }
    unsigned long GetNumVisibleTiles() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::NumVisibleTiles); }
    unsigned long GetNumVisibleTileRows() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::VisibleTileRows); }
    GETextureSet* GetRegisteredTextureSet(unsigned long inID) const { return (GETextureSet*)ee_ge_tm_texture_set(this->mRust, inID); }
    const GE3DPlane& GetRightPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 3)); }
    void GetTerrainVisibleBottomLeftIndices(unsigned long& outX, unsigned long& outY) const
        { ee_ge_tm_get_pair(this->mRust, EeGeTmVal::BottomLeftX, EeGeTmVal::BottomLeftY, &outX, &outY); }
    void GetTerrainVisibleTileDimensions(unsigned long& outWidth, unsigned long& outHeight) const
        { ee_ge_tm_get_pair(this->mRust, EeGeTmVal::VisibleTileWidth, EeGeTmVal::VisibleTileHeight, &outWidth, &outHeight); }
    unsigned long GetTextureSetID(unsigned long inX, unsigned long inY) const { return ee_ge_tm_tile(this->mRust, inX, inY)->texture_set_id; }
    const GEColor& GetVertexColor(unsigned long inX, unsigned long inY) const
        { return *reinterpret_cast<const GEColor*>(ee_ge_tm_vertex_color(this->mRust, inX, inY)); }
    GEVector* GetVertexNormal(unsigned long inX, unsigned long inY) const { return (GEVector*)ee_ge_tm_point(this->mRust, inX, inY)->normal; }
    float GetVisibleSpaceTilePercentage() const { return ee_ge_tm_get_f(this->mRust, EeGeTmVal::VisibleSpacePercentage); }
    float GetVisibleWaterTilePercentage() const { return ee_ge_tm_get_f(this->mRust, EeGeTmVal::VisibleWaterPercentage); }
    float GetZ(unsigned long inX, unsigned long inY) const { return ee_ge_tm_point(this->mRust, inX, inY)->z; }
    bool Intersects(const GE3DLine& inLine, float& outDistance) const { return ee_ge_tm_intersects(this->mRust, GERust(inLine), &outDistance); }
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_ge_tm_load(this->mRust, &inFile); }
    void ReferenceTile(unsigned long inX, unsigned long inY) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 0, 0); }
    long RegisterColorSet(std::vector<unsigned long>& inColorSet, unsigned long& outID)
        { return ee_ge_tm_register_color_set_out(this->mRust, inColorSet.data(), inColorSet.size(), &outID); }
    long RegisterTextureSet(GETextureSet* inTextureSet, unsigned long& outID) { return ee_ge_tm_register_texture_set_out(this->mRust, inTextureSet, &outID); }
    long Save(FSFile& inFile) const { return ee_ge_tm_save(this->mRust, &inFile); }
    void SetColorFromColorTableIndex(unsigned long inX, unsigned long inY, unsigned char inIndex)
        { ee_ge_tm_point_edit(this->mRust, inX, inY, 1, inIndex, 0.0f); }
    void SetColorTable(std::vector<GEColor>& inColorTable)
        { ee_ge_tm_set_color_table(this->mRust, reinterpret_cast<const float(*)[3]>(inColorTable.data()), inColorTable.size()); }
    void SetGlobalWaterState(unsigned long inWaterSetID, unsigned long inColorSetID, unsigned long inAnimationPeriod,
                             float inTextureScale)
        { ee_ge_tm_set_global_water_state(this->mRust, inWaterSetID, inColorSetID, inAnimationPeriod, inTextureScale); }
    void SetIsSpace(unsigned long inX, unsigned long inY, bool inIsSpace) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 6, inIsSpace); }
    void SetIsWater(unsigned long inX, unsigned long inY, bool inIsWater) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 5, inIsWater); }
    void SetSpaceCliffTextures(std::vector<GETexture*>& inTextures)
        { ee_ge_tm_set_space_cliff_textures(this->mRust, (void* const*)inTextures.data(), inTextures.size()); }
    void SetTextureSet(unsigned long inX, unsigned long inY, unsigned long inTextureSetID) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 3, inTextureSetID); }
    void SetTileReferenceCount(unsigned long inX, unsigned long inY, unsigned long inCount) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 2, inCount); }
    void SetWaterColorSet(unsigned long inX, unsigned long inY) { ee_ge_tm_point_edit(this->mRust, inX, inY, 3, 0, 0.0f); }
    void SetZ(unsigned long inX, unsigned long inY, float inZ) { ee_ge_tm_point_edit(this->mRust, inX, inY, 0, 0, inZ); }
    void UpdateWaterAnimation() { ee_ge_tm_update_water_animation(this->mRust); }

    URust<EeGeTerrainMesh> mRust;                 // the whole mesh (ee_ge tmobj.rs)
};

#endif
