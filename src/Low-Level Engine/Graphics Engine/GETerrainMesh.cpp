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
#define LLE_GETERRAINMESH_CPP
// GETerrainMesh is a whole Rust object (rust/crates/ee_ge tmobj.rs, R4.3 eleventh part): the rows, the sets, the
// tables, the water state and the visible frame are its, and every method below forwards to it. Every call it makes
// on another object (the clock, the colour fractal, the quantized normals, the texture sets, the textures'
// references) is one EeGeTmOp of TerrainMeshHost, in the original's order.
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/URandom.h"       // U2DFractal.h
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
#include "GERustViewport.h"
#include "GERustIO.h"

// the records the renderer reads through the mesh are the Rust object's
#define GETM_PIN(cpp, rust, cppField, rustField) static_assert(offsetof(cpp, cppField) == offsetof(rust, rustField), #cppField)
static_assert(sizeof(GETerrainMesh::GETMRenderVertex) == sizeof(EeGeTmRenderVertex), "GETMRenderVertex");
GETM_PIN(GETerrainMesh::GETMRenderVertex, EeGeTmRenderVertex, mColor, color);
GETM_PIN(GETerrainMesh::GETMRenderVertex, EeGeTmRenderVertex, mVisibility, visibility);
GETM_PIN(GETerrainMesh::GETMRenderVertex, EeGeTmRenderVertex, mClipFlags, clip_flags);
GETM_PIN(GETerrainMesh::GETMRenderVertex, EeGeTmRenderVertex, mLinkedVisibility, linked_visibility);
static_assert(sizeof(GETerrainMesh::GETMPolygon) == sizeof(EeGeTmPolygon), "GETMPolygon");
GETM_PIN(GETerrainMesh::GETMPolygon, EeGeTmPolygon, mAlpha, alpha);
GETM_PIN(GETerrainMesh::GETMPolygon, EeGeTmPolygon, mUV, uv);
static_assert(sizeof(GEColor) == 3 * sizeof(float) && sizeof(GE3DPlane) == sizeof(EeGe3DPlane), "GEColor, GE3DPlane");
static_assert(GETM_VISIBLE_POINTS == EE_GE_TM_SIDE, "GETM_VISIBLE_POINTS");

// The calls the mesh makes on other objects (EeGeTmOp), in the original's statements
static intptr_t TerrainMeshHost(void*, EeGeTmOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    GETextureSet* theSet = (GETextureSet*)inObj;
    switch (inOp) {
    case EeGeTmOp::Time:
        return (intptr_t)TMGetTime();
    case EeGeTmOp::FractalNew:
        return (intptr_t) new U2DFractal(64, 0.0f, 10.5f, (unsigned long)a);
    case EeGeTmOp::FractalDelete:
        delete (U2DFractal*)inObj;
        return 0;
    case EeGeTmOp::FractalValue:
        *(float*)a = ((U2DFractal*)inObj)->GetValue((unsigned long)(b >> 16), (unsigned long)(b & 0xffff));
        return 0;
    case EeGeTmOp::FractalCompute:
        ((U2DFractal*)inObj)->ComputeFractal(0.0f, 10.5f, (unsigned long)a);
        return 0;
    case EeGeTmOp::Quantize:
        return (intptr_t)((const GEVector*)a)->GetQuantizedVersion();
    case EeGeTmOp::SetDelete:
        delete theSet;
        return 0;
    case EeGeTmOp::SetWidth:
        return (intptr_t)theSet->GetNumTexturesInWidth();
    case EeGeTmOp::SetHeight:
        return (intptr_t)theSet->GetNumTexturesInHeight();
    case EeGeTmOp::SetCoverage:
        return (intptr_t)theSet->GetTextureTileCoverage();
    case EeGeTmOp::SetTexture:
        return (intptr_t)theSet->GetTexture((unsigned long)a, (unsigned long)b);
    case EeGeTmOp::SetTextureAt:
        return (intptr_t)theSet->GetTextureAt((unsigned long)a, (unsigned long)b);
    case EeGeTmOp::TexReference:
        ((GETexture*)inObj)->Reference();
        return 0;
    case EeGeTmOp::TexDereference:
        ((GETexture*)inObj)->Dereference();
        return 0;
    }
    return 0;
}

GETerrainMesh::GETerrainMesh() { this->mRust = ee_ge_tm_new(TerrainMeshHost, this); }

GETerrainMesh::~GETerrainMesh() { ee_ge_tm_destroy(this->mRust); }

void GETerrainMesh::ComputeShorelines(unsigned long inX, unsigned long inY) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 7, 0); }

void GETerrainMesh::ComputeVertexNormal(unsigned long inX, unsigned long inY) { ee_ge_tm_point_edit(this->mRust, inX, inY, 4, 0, 0.0f); }

void GETerrainMesh::CreateFlatMesh(long inWidth, long inHeight) { ee_ge_tm_create_flat_mesh(this->mRust, inWidth, inHeight); }

void GETerrainMesh::GetMeshNormal(float inX, float inY, GEVector& outNormal) const { ee_ge_tm_mesh_normal(this->mRust, inX, inY, GERust(outNormal)); }

float GETerrainMesh::GetMeshZ(float inX, float inY) const { return ee_ge_tm_mesh_z(this->mRust, inX, inY); }

void GETerrainMesh::GetClosestTile(const GE3DLine& inLine, long& outX, long& outY) const { ee_ge_tm_closest_tile_line(this->mRust, GERust(inLine), &outX, &outY); }

void GETerrainMesh::GetClosestTile(const GE3DPoint& inPoint, long& outX, long& outY) const { ee_ge_tm_closest_tile_point(this->mRust, GERust(inPoint), &outX, &outY); }

bool GETerrainMesh::Intersects(const GE3DLine& inLine, float& outDistance) const { return ee_ge_tm_intersects(this->mRust, GERust(inLine), &outDistance); }

long GETerrainMesh::RegisterColorSet(std::vector<unsigned long>& inColorSet, unsigned long& outID)
{
    outID = ee_ge_tm_register_color_set(this->mRust, inColorSet.data(), inColorSet.size());
    return 0;
}

long GETerrainMesh::RegisterTextureSet(GETextureSet* inTextureSet, unsigned long& outID)
{
    outID = ee_ge_tm_register_texture_set(this->mRust, inTextureSet);
    return 0;
}

void GETerrainMesh::SetColorFromColorTableIndex(unsigned long inX, unsigned long inY, unsigned char inIndex) { ee_ge_tm_point_edit(this->mRust, inX, inY, 1, inIndex, 0.0f); }

void GETerrainMesh::SetColorSet(unsigned long inX, unsigned long inY, unsigned long inColorSetID) { ee_ge_tm_point_edit(this->mRust, inX, inY, 2, inColorSetID, 0.0f); }

void GETerrainMesh::SetWaterColorSet(unsigned long inX, unsigned long inY) { ee_ge_tm_point_edit(this->mRust, inX, inY, 3, 0, 0.0f); }

void GETerrainMesh::SetColorTable(std::vector<GEColor>& inColorTable) { ee_ge_tm_set_color_table(this->mRust, reinterpret_cast<const float(*)[3]>(inColorTable.data()), inColorTable.size()); }

void GETerrainMesh::SetGlobalWaterState(unsigned long inWaterSetID, unsigned long inColorSetID, unsigned long inAnimationPeriod, float inTextureScale)
    { ee_ge_tm_set_global_water_state(this->mRust, inWaterSetID, inColorSetID, inAnimationPeriod, inTextureScale); }

void GETerrainMesh::UpdateFogOfWar(bool inAllVisible) { ee_ge_tm_update_fog_of_war(this->mRust, inAllVisible); }

void GETerrainMesh::UpdateWaterAnimation() { ee_ge_tm_update_water_animation(this->mRust); }

bool GETerrainMesh::GetIsSpaceTile(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_space_tile(this->mRust, inX, inY); }

void GETerrainMesh::UpdateVisibleTerrain(const GETransformation&, GEViewport& inViewport, unsigned long, unsigned long, unsigned long, bool inOrthographic)
    { ee_ge_tm_update_visible_terrain(this->mRust, GERust(inViewport), inOrthographic); }

void GETerrainMesh::SetIsSpace(unsigned long inX, unsigned long inY, bool inIsSpace) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 6, inIsSpace); }

long GETerrainMesh::Load(unsigned long, unsigned long, FSFile& inFile)
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    return ee_ge_tm_load(this->mRust, &theIO);
}

long GETerrainMesh::Save(FSFile& inFile) const
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    return ee_ge_tm_save(this->mRust, &theIO);
}

// The header's accessors (inline in the original: GEBoundingBox.cpp emitted them)
void GETerrainMesh::DereferenceTile(unsigned long inX, unsigned long inY) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 1, 0); }
void GETerrainMesh::ReferenceTile(unsigned long inX, unsigned long inY) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 0, 0); }
void GETerrainMesh::SetTileReferenceCount(unsigned long inX, unsigned long inY, unsigned long inCount) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 2, inCount); }
void GETerrainMesh::SetTextureSet(unsigned long inX, unsigned long inY, unsigned long inTextureSetID) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 3, inTextureSetID); }
void GETerrainMesh::SetIsWater(unsigned long inX, unsigned long inY, bool inIsWater) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 5, inIsWater); }
void GETerrainMesh::SetZ(unsigned long inX, unsigned long inY, float inZ) { ee_ge_tm_point_edit(this->mRust, inX, inY, 0, 0, inZ); }
float GETerrainMesh::GetZ(unsigned long inX, unsigned long inY) const { return ee_ge_tm_point(this->mRust, inX, inY)->z; }
unsigned char GETerrainMesh::GetColorTableIndex(unsigned long inX, unsigned long inY) const { return ee_ge_tm_point(this->mRust, inX, inY)->color_table_index; }
GEVector* GETerrainMesh::GetVertexNormal(unsigned long inX, unsigned long inY) const { return (GEVector*)ee_ge_tm_point(this->mRust, inX, inY)->normal; }
const GEColor& GETerrainMesh::GetVertexColor(unsigned long inX, unsigned long inY) const { return *reinterpret_cast<const GEColor*>(ee_ge_tm_vertex_color(this->mRust, inX, inY)); }
unsigned long GETerrainMesh::GetTextureSetID(unsigned long inX, unsigned long inY) const { return ee_ge_tm_tile(this->mRust, inX, inY)->texture_set_id; }
unsigned char GETerrainMesh::GetWaterInfo(unsigned long inX, unsigned long inY) const { return ee_ge_tm_tile(this->mRust, inX, inY)->water_info; }
bool GETerrainMesh::GetIsWaterTile(unsigned long inX, unsigned long inY) const { return (ee_ge_tm_tile(this->mRust, inX, inY)->water_info & kWaterInfoWaterMask) != 0; }
bool GETerrainMesh::GetIsValidPoint(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_valid(this->mRust, inX, inY, false); }
bool GETerrainMesh::GetIsValidTile(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_valid(this->mRust, inX, inY, true); }
bool GETerrainMesh::GetIsSpaceMap() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::SpaceTileCount) > 0; }
const GE3DPlane& GETerrainMesh::GetFrontPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 0)); }
const GE3DPlane& GETerrainMesh::GetBackPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 1)); }
const GE3DPlane& GETerrainMesh::GetLeftPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 2)); }
const GE3DPlane& GETerrainMesh::GetRightPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 3)); }
float GETerrainMesh::GetVisibleSpaceTilePercentage() const { return ee_ge_tm_get_f(this->mRust, EeGeTmVal::VisibleSpacePercentage); }
float GETerrainMesh::GetVisibleWaterTilePercentage() const { return ee_ge_tm_get_f(this->mRust, EeGeTmVal::VisibleWaterPercentage); }
unsigned long GETerrainMesh::GetGlobalWaterSetID() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::GlobalWaterSetId); }
unsigned long GETerrainMesh::GetNumVisibleTiles() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::NumVisibleTiles); }
unsigned long GETerrainMesh::GetNumVisibleTileRows() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::VisibleTileRows); }
unsigned int GETerrainMesh::NumSpaceCliffTextures() { return ee_ge_tm_get(this->mRust, EeGeTmVal::SpaceCliffTextureCount); }
void GETerrainMesh::SetSpaceCliffTextures(std::vector<GETexture*>& inTextures) { ee_ge_tm_set_space_cliff_textures(this->mRust, (void* const*)inTextures.data(), inTextures.size()); }
GETextureSet* GETerrainMesh::GetRegisteredTextureSet(unsigned long inID) const { return (GETextureSet*)ee_ge_tm_texture_set(this->mRust, inID); }
void GETerrainMesh::GetTerrainVisibleBottomLeftIndices(unsigned long& outX, unsigned long& outY) const
{
    outX = ee_ge_tm_get(this->mRust, EeGeTmVal::BottomLeftX);
    outY = ee_ge_tm_get(this->mRust, EeGeTmVal::BottomLeftY);
}
void GETerrainMesh::GetTerrainVisibleTileDimensions(unsigned long& outWidth, unsigned long& outHeight) const
{
    outWidth = ee_ge_tm_get(this->mRust, EeGeTmVal::VisibleTileWidth);
    outHeight = ee_ge_tm_get(this->mRust, EeGeTmVal::VisibleTileHeight);
}
void GETerrainMesh::GetVisibleTileRow(unsigned long inIndex, long& outY, long& outStartX, long& outEndX) const
{
    long theRow[3];
    ee_ge_tm_visible_row(this->mRust, inIndex, theRow);
    outY = theRow[0];
    outStartX = theRow[1];
    outEndX = theRow[2];
}
