// GETerrainMesh's queries in Rust (rust/crates/ee_ge terrain.rs, docs/RUST_PLAN.md R4.3): the mesh stays C++ and the
// queries read its point and tile rows in place through an EeGeTerrainView. The row layouts are pinned here; the
// fog of war reads the visible area through an EeGeTerrainFog.
// Include after lle/GETerrainMesh.h and GERustMath.h.
#ifndef GERUSTTERRAIN_H
#define GERUSTTERRAIN_H

#include <stddef.h>

struct GERustTerrainLayout {
    typedef GETerrainMesh::GETMPoint Point;
    typedef GETerrainMesh::GETMPointRow PointRow;
    typedef GETerrainMesh::GETMTileRow TileRow;
    typedef GETerrainMesh::GETMTile Tile;
    static_assert(sizeof(Tile) == sizeof(EeGeTmTile) && offsetof(Tile, mReferenceCount) == offsetof(EeGeTmTile, reference_count) &&
                  offsetof(Tile, mTextureSetID) == offsetof(EeGeTmTile, texture_set_id) &&
                  offsetof(Tile, mWaterInfo) == offsetof(EeGeTmTile, water_info), "GETMTile");
    static_assert(sizeof(Point) == sizeof(EeGeTmPoint) && offsetof(Point, mZ) == offsetof(EeGeTmPoint, z) &&
                  offsetof(Point, mNormal) == offsetof(EeGeTmPoint, normal) &&
                  offsetof(Point, mColorTableIndex) == offsetof(EeGeTmPoint, color_table_index), "GETMPoint");
    static_assert(sizeof(PointRow) == sizeof(EeGeTmRow) && offsetof(PointRow, mStart) == offsetof(EeGeTmRow, start) &&
                  offsetof(PointRow, mCount) == offsetof(EeGeTmRow, count) &&
                  offsetof(PointRow, mPoints) == offsetof(EeGeTmRow, items), "GETMPointRow");
    static_assert(sizeof(TileRow) == sizeof(EeGeTmRow) && offsetof(TileRow, mStart) == offsetof(EeGeTmRow, start) &&
                  offsetof(TileRow, mCount) == offsetof(EeGeTmRow, count) &&
                  offsetof(TileRow, mTiles) == offsetof(EeGeTmRow, items), "GETMTileRow");
    // the view reads the four map-edge planes as one array: front, back, left, right
    static_assert(offsetof(GETerrainMesh, mBackPlane) == offsetof(GETerrainMesh, mFrontPlane) + sizeof(GE3DPlane) &&
                  offsetof(GETerrainMesh, mLeftPlane) == offsetof(GETerrainMesh, mFrontPlane) + 2 * sizeof(GE3DPlane) &&
                  offsetof(GETerrainMesh, mRightPlane) == offsetof(GETerrainMesh, mFrontPlane) + 3 * sizeof(GE3DPlane),
                  "GETerrainMesh's planes");

    static EeGeTerrainView View(const GETerrainMesh& inMesh)
    {
        EeGeTerrainView theView;
        theView.point_rows = reinterpret_cast<const EeGeTmRow*>(inMesh.mPointRows);
        theView.tile_rows = reinterpret_cast<const EeGeTmRow*>(inMesh.mTileRows);
        theView.tile_row_count = inMesh.mTileRowCount;
        theView.planes = reinterpret_cast<const EeGe3DPlane*>(&inMesh.mFrontPlane);
        return theView;
    }

    // UpdateFogOfWar's visible area: the point arrays and the render vertices' mVisibility, by vertex size
    static EeGeTerrainFog Fog(GETerrainMesh& inMesh, bool inAllVisible)
    {
        EeGeTerrainFog theFog;
        theFog.visible_row_count = inMesh.mVisibleTileRows.size();
        theFog.bottom_left_x = inMesh.mVisibleBottomLeftX;
        theFog.bottom_left_y = inMesh.mVisibleBottomLeftY;
        theFog.tile_width = inMesh.mVisibleTileWidth;
        theFog.tile_height = inMesh.mVisibleTileHeight;
        theFog.num_visible_tiles = inMesh.mNumVisibleTiles;
        theFog.side = GETM_VISIBLE_POINTS;
        theFog.terrain_point_visible = reinterpret_cast<const uint8_t*>(&inMesh.mTerrainPointVisibility[0][0]);
        theFog.water_point_visible = reinterpret_cast<const uint8_t*>(&inMesh.mWaterPointVisibility[0][0]);
        theFog.terrain_visibility = &inMesh.mTerrainRenderVertices[0][0].mVisibility;
        theFog.water_visibility = &inMesh.mWaterRenderVertices[0][0].mVisibility;
        theFog.vertex_stride = sizeof(GETerrainMesh::GETMRenderVertex);
        theFog.all_visible = inAllVisible;
        return theFog;
    }
};

inline EeGeTerrainView GERustTerrainView(const GETerrainMesh& inMesh)
{
    return GERustTerrainLayout::View(inMesh);
}

#endif
