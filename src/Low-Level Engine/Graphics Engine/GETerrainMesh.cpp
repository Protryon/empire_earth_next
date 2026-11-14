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
static_assert(sizeof(GETerrainMesh::GETMMaterialView) == sizeof(EeGeTmMaterialView), "GETMMaterialView");
GETM_PIN(GETerrainMesh::GETMMaterialView, EeGeTmMaterialView, mStrips, strips);
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

// FUNCTION: LLE 0x100273c9
GETerrainMesh::GETerrainMesh() { this->mRust = ee_ge_tm_new(TerrainMeshHost, this); }

// FUNCTION: LLE 0x1002766d
GETerrainMesh::~GETerrainMesh() { ee_ge_tm_destroy(this->mRust); }

// FUNCTION: LLE 0x1002795f
void GETerrainMesh::ComputeShorelines(unsigned long inX, unsigned long inY) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 7, 0); }

// FUNCTION: LLE 0x10027aa4
void GETerrainMesh::ComputeVertexNormal(unsigned long inX, unsigned long inY) { ee_ge_tm_point_edit(this->mRust, inX, inY, 4, 0, 0.0f); }

// FUNCTION: LLE 0x10027c53
void GETerrainMesh::CreateFlatMesh(long inWidth, long inHeight) { ee_ge_tm_create_flat_mesh(this->mRust, inWidth, inHeight); }

// FUNCTION: LLE 0x1002a05d
void GETerrainMesh::GetMeshNormal(float inX, float inY, GEVector& outNormal) const { ee_ge_tm_mesh_normal(this->mRust, inX, inY, GERust(outNormal)); }

// FUNCTION: LLE 0x1002a185
float GETerrainMesh::GetMeshZ(float inX, float inY) const { return ee_ge_tm_mesh_z(this->mRust, inX, inY); }

// FUNCTION: LLE 0x1002a277
void GETerrainMesh::GetClosestTile(const GE3DLine& inLine, long& outX, long& outY) const { ee_ge_tm_closest_tile_line(this->mRust, GERust(inLine), &outX, &outY); }

// FUNCTION: LLE 0x1002a5af
void GETerrainMesh::GetClosestTile(const GE3DPoint& inPoint, long& outX, long& outY) const { ee_ge_tm_closest_tile_point(this->mRust, GERust(inPoint), &outX, &outY); }

// FUNCTION: LLE 0x1002a942
bool GETerrainMesh::Intersects(const GE3DLine& inLine, float& outDistance) const { return ee_ge_tm_intersects(this->mRust, GERust(inLine), &outDistance); }

// FUNCTION: LLE 0x1002ac2d
void GETerrainMesh::GetTileRowsInVolume(const std::vector<GE3DPoint>& inVolume,
                                        std::vector<std::pair<long, std::pair<long, long> > >& outRows,
                                        U2DPoint<long>& outMinimum, U2DPoint<long>& outMaximum)
{
    std::vector<long> theRows(3 * ee_ge_tm_get(this->mRust, EeGeTmVal::TileRowCount));   // at most one row per y
    long theBounds[4];
    size_t theCount = ee_ge_tm_rows_in_volume(this->mRust, inVolume.empty() ? NULL : GERust(inVolume[0]), inVolume.size(), theRows.data(), theBounds);
    for (size_t i = 0; i < theCount; i++)
        outRows.push_back(std::make_pair(theRows[3 * i], std::make_pair(theRows[3 * i + 1], theRows[3 * i + 2])));
    outMinimum.mX = theBounds[0];
    outMinimum.mY = theBounds[1];
    outMaximum.mX = theBounds[2];
    outMaximum.mY = theBounds[3];
}

// FUNCTION: LLE 0x1002ae1f
long GETerrainMesh::RegisterColorSet(std::vector<unsigned long>& inColorSet, unsigned long& outID)
{
    outID = ee_ge_tm_register_color_set(this->mRust, inColorSet.data(), inColorSet.size());
    return 0;
}

// FUNCTION: LLE 0x1002ae49
long GETerrainMesh::RegisterTextureSet(GETextureSet* inTextureSet, unsigned long& outID)
{
    outID = ee_ge_tm_register_texture_set(this->mRust, inTextureSet);
    return 0;
}

// FUNCTION: LLE 0x1002ae74
void GETerrainMesh::SetColorFromColorTableIndex(unsigned long inX, unsigned long inY, unsigned char inIndex) { ee_ge_tm_point_edit(this->mRust, inX, inY, 1, inIndex, 0.0f); }

// FUNCTION: LLE 0x1002ae9b
void GETerrainMesh::SetColorSet(unsigned long inX, unsigned long inY, unsigned long inColorSetID) { ee_ge_tm_point_edit(this->mRust, inX, inY, 2, inColorSetID, 0.0f); }

// FUNCTION: LLE 0x1002af33
void GETerrainMesh::SetWaterColorSet(unsigned long inX, unsigned long inY) { ee_ge_tm_point_edit(this->mRust, inX, inY, 3, 0, 0.0f); }

// FUNCTION: LLE 0x1002afc9
void GETerrainMesh::SetColorTable(std::vector<GEColor>& inColorTable) { ee_ge_tm_set_color_table(this->mRust, reinterpret_cast<const float(*)[3]>(inColorTable.data()), inColorTable.size()); }

// FUNCTION: LLE 0x1002b079
void GETerrainMesh::SetGlobalWaterState(unsigned long inWaterSetID, unsigned long inColorSetID, unsigned long inAnimationPeriod, float inTextureScale)
    { ee_ge_tm_set_global_water_state(this->mRust, inWaterSetID, inColorSetID, inAnimationPeriod, inTextureScale); }

// FUNCTION: LLE 0x1002b507
void GETerrainMesh::UpdateFogOfWar(bool inAllVisible) { ee_ge_tm_update_fog_of_war(this->mRust, inAllVisible); }

// FUNCTION: LLE 0x1002b83f
void GETerrainMesh::UpdateWaterAnimation() { ee_ge_tm_update_water_animation(this->mRust); }

// FUNCTION: LLE 0x1002c264
bool GETerrainMesh::GetIsSpaceTile(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_space_tile(this->mRust, inX, inY); }

// FUNCTION: LLE 0x1002c299
void GETerrainMesh::UpdateVisibleTerrain(const GETransformation&, GEViewport& inViewport, unsigned long, unsigned long, unsigned long, bool inOrthographic)
    { ee_ge_tm_update_visible_terrain(this->mRust, GERust(inViewport), inOrthographic); }

// FUNCTION: LLE 0x1002d2a6
void GETerrainMesh::SetIsSpace(unsigned long inX, unsigned long inY, bool inIsSpace) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 6, inIsSpace); }

// FUNCTION: LLE 0x1002d3c1
long GETerrainMesh::Load(unsigned long, unsigned long, FSFile& inFile)
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    return ee_ge_tm_load(this->mRust, &theIO);
}

// FUNCTION: LLE 0x1002d656
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
void GETerrainMesh::SetWaterInfo(unsigned long inX, unsigned long inY, unsigned char inWaterInfo) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 4, inWaterInfo); }
void GETerrainMesh::SetIsWater(unsigned long inX, unsigned long inY, bool inIsWater) { ee_ge_tm_tile_edit(this->mRust, inX, inY, 5, inIsWater); }
void GETerrainMesh::SetZ(unsigned long inX, unsigned long inY, float inZ) { ee_ge_tm_point_edit(this->mRust, inX, inY, 0, 0, inZ); }
float GETerrainMesh::GetZ(unsigned long inX, unsigned long inY) const { return ee_ge_tm_point(this->mRust, inX, inY)->z; }
unsigned char GETerrainMesh::GetColorTableIndex(unsigned long inX, unsigned long inY) const { return ee_ge_tm_point(this->mRust, inX, inY)->color_table_index; }
GEVector* GETerrainMesh::GetVertexNormal(unsigned long inX, unsigned long inY) const { return (GEVector*)ee_ge_tm_point(this->mRust, inX, inY)->normal; }
const GEColor& GETerrainMesh::GetVertexColor(unsigned long inX, unsigned long inY) const { return *reinterpret_cast<const GEColor*>(ee_ge_tm_vertex_color(this->mRust, inX, inY)); }
const GEColor& GETerrainMesh::GetWaterColor(unsigned long, unsigned long) const { return *reinterpret_cast<const GEColor*>(ee_ge_tm_water_color(this->mRust)); }
unsigned long GETerrainMesh::GetTextureSetID(unsigned long inX, unsigned long inY) const { return ee_ge_tm_tile(this->mRust, inX, inY)->texture_set_id; }
unsigned char GETerrainMesh::GetWaterInfo(unsigned long inX, unsigned long inY) const { return ee_ge_tm_tile(this->mRust, inX, inY)->water_info; }
bool GETerrainMesh::GetIsWaterTile(unsigned long inX, unsigned long inY) const { return (ee_ge_tm_tile(this->mRust, inX, inY)->water_info & kWaterInfoWaterMask) != 0; }
bool GETerrainMesh::GetIsValidPoint(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_valid(this->mRust, inX, inY, false); }
bool GETerrainMesh::GetIsValidTile(unsigned long inX, unsigned long inY) const { return ee_ge_tm_is_valid(this->mRust, inX, inY, true); }
bool GETerrainMesh::GetIsSpaceMap() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::SpaceTileCount) > 0; }
bool GETerrainMesh::GetIsEdgeTile(long inX, long inY) const { return ee_ge_tm_is_edge_tile(this->mRust, inX, inY, 0); }
bool GETerrainMesh::GetIsLeftEdgeTile(long inX, long inY) const { return ee_ge_tm_is_edge_tile(this->mRust, inX, inY, 1); }
bool GETerrainMesh::GetIsTopEdgeTile(long inX, long inY) const { return ee_ge_tm_is_edge_tile(this->mRust, inX, inY, 2); }
bool GETerrainMesh::GetIsRightEdgeTile(long inX, long inY) const { return ee_ge_tm_is_edge_tile(this->mRust, inX, inY, 3); }
bool GETerrainMesh::GetIsBottomEdgeTile(long inX, long inY) const { return ee_ge_tm_is_edge_tile(this->mRust, inX, inY, 4); }
const GE3DPlane& GETerrainMesh::GetFrontPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 0)); }
const GE3DPlane& GETerrainMesh::GetBackPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 1)); }
const GE3DPlane& GETerrainMesh::GetLeftPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 2)); }
const GE3DPlane& GETerrainMesh::GetRightPlane() const { return *reinterpret_cast<const GE3DPlane*>(ee_ge_tm_plane(this->mRust, 3)); }
float GETerrainMesh::GetMinimumZ() const { return ee_ge_tm_get_f(this->mRust, EeGeTmVal::MinimumZ); }
float GETerrainMesh::GetMaximumZ() const { return ee_ge_tm_get_f(this->mRust, EeGeTmVal::MaximumZ); }
float GETerrainMesh::GetVisibleSpaceTilePercentage() const { return ee_ge_tm_get_f(this->mRust, EeGeTmVal::VisibleSpacePercentage); }
float GETerrainMesh::GetVisibleWaterTilePercentage() const { return ee_ge_tm_get_f(this->mRust, EeGeTmVal::VisibleWaterPercentage); }
void GETerrainMesh::SetVisibleWaterTilePercentage(float inPercentage) { ee_ge_tm_set_water_percentage(this->mRust, inPercentage); }
unsigned long GETerrainMesh::GetGlobalWaterSetID() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::GlobalWaterSetId); }
unsigned long GETerrainMesh::GetNumVisibleTiles() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::NumVisibleTiles); }
unsigned long GETerrainMesh::GetNumVisibleUnitTiles() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::NumVisibleTiles); }
unsigned long GETerrainMesh::GetNumVisibleTileRows() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::VisibleTileRows); }
unsigned long GETerrainMesh::GetVisiblePolyCount() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::VisiblePolyCount); }
unsigned long GETerrainMesh::GetTerrainMaterialCount() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::TerrainMaterialCount); }
unsigned long GETerrainMesh::GetWaterMaterialCount() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::WaterMaterialCount); }
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
bool (&GETerrainMesh::GetTerrainPointVisibilityArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS]
    { return *reinterpret_cast<bool(*)[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS]>(ee_ge_tm_point_visibility(this->mRust, false)); }
bool (&GETerrainMesh::GetWaterPointVisibilityArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS]
    { return *reinterpret_cast<bool(*)[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS]>(ee_ge_tm_point_visibility(this->mRust, true)); }
GETerrainMesh::GETMRenderVertex (&GETerrainMesh::GetTerrainRenderVertexArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS]
    { return *reinterpret_cast<GETMRenderVertex(*)[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS]>(ee_ge_tm_render_vertices(this->mRust, false)); }
GETerrainMesh::GETMRenderVertex (&GETerrainMesh::GetWaterRenderVertexArray())[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS]
    { return *reinterpret_cast<GETMRenderVertex(*)[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS]>(ee_ge_tm_render_vertices(this->mRust, true)); }
void GETerrainMesh::GetTerrainMaterialMap(std::vector<std::pair<GETexture*, unsigned long> >& outEntries) const
{
    std::vector<void*> theTextures(ee_ge_tm_get(this->mRust, EeGeTmVal::TerrainMaterialCount));
    std::vector<unsigned long> theIndices(theTextures.size());
    size_t theCount = ee_ge_tm_material_map(this->mRust, theTextures.data(), theIndices.data(), theTextures.size());
    outEntries.clear();
    for (size_t i = 0; i < theCount; i++)
        outEntries.push_back(std::make_pair((GETexture*)theTextures[i], theIndices[i]));
}
void GETerrainMesh::GetMaterialView(bool inWater, unsigned long inIndex, GETMMaterialView& outView)
    { ee_ge_tm_material(this->mRust, inWater, inIndex, reinterpret_cast<EeGeTmMaterialView*>(&outView)); }
unsigned long GETerrainMesh::GetEdgeVertexCount() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::EdgeVertexCount); }
GETerrainMesh::GETMRenderVertex* const* GETerrainMesh::GetEdgeRenderVertices() const { return (GETMRenderVertex* const*)ee_ge_tm_edge_vertices(this->mRust, false); }
const GE3DPoint* GETerrainMesh::GetEdgeWorldPoints() const { return reinterpret_cast<const GE3DPoint*>(ee_ge_tm_edge_points(this->mRust, false)); }
unsigned long GETerrainMesh::GetCliffVertexCount() const { return ee_ge_tm_get(this->mRust, EeGeTmVal::CliffVertexCount); }
GETerrainMesh::GETMRenderVertex* const* GETerrainMesh::GetCliffRenderVertices() const { return (GETMRenderVertex* const*)ee_ge_tm_edge_vertices(this->mRust, true); }
const GE3DPoint* GETerrainMesh::GetCliffWorldPoints() const { return reinterpret_cast<const GE3DPoint*>(ee_ge_tm_edge_points(this->mRust, true)); }
