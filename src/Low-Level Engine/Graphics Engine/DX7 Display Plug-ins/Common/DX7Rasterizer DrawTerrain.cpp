// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Rasterizer DrawTerrain.cpp (file name
// inferred: the fourth of the four DX7Rasterizer units in link order; it holds the terrain draws)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x10004d1a..0x100091aa: the U2D key init chains, Draw(GETerrainMesh), DrawGridLines, DrawTerrainGrid,
// DrawTerrainHighlight, DrawTerrainMaterial, DrawWater, Light and TransformToCamera (export order), the vector
// COMDATs they pull in, then the template-static init chain head and its atexit registrars. The out-of-line
// vector<T*>::operator[] COMDAT 0x10008be2 is not emitted: our candidates expand every operator[] (inline budget).
// Its only caller is DrawWater's last statement (mDX7ScreenStates[inScreen]->mViewportChanged = true, 0x10007226),
// where the original's budget is exhausted; GETerrainMesh::GetNumVisibleTileRows is likewise an inline the budget
// refuses (called through the import in six functions, expanded with size() out of line in Light).
#include <windows.h>
#include <stddef.h>
#include "gx/GX.h"

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7PolygonArray.h"
#include "dx7/DX7StandardVertexArray.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "dx7/DX7DrawSeam.h"

// The D3D light indices in use (DX7Rasterizer.cpp).
extern unsigned long gLightsInUse[8];

// The U2D key init chains:
// SYNTHETIC: DX7HR 0x10004d1a
// SYNTHETIC: DX7HR 0x10004d24
// SYNTHETIC: DX7HR 0x10004d35
// SYNTHETIC: DX7HR 0x10004d41
// SYNTHETIC: DX7HR 0x10004d4c
// SYNTHETIC: DX7HR 0x10004d56
// SYNTHETIC: DX7HR 0x10004d67
// SYNTHETIC: DX7HR 0x10004d73
// SYNTHETIC: DX7HR 0x10004d7e
// SYNTHETIC: DX7HR 0x10004d88
// SYNTHETIC: DX7HR 0x10004d99
// SYNTHETIC: DX7HR 0x10004da5
// SYNTHETIC: DX7HR 0x10004db0
// SYNTHETIC: DX7HR 0x10004dba
// SYNTHETIC: DX7HR 0x10004dcb
// SYNTHETIC: DX7HR 0x10004dd7
// SYNTHETIC: DX7HR 0x10004de2
// SYNTHETIC: DX7HR 0x10004dec
// SYNTHETIC: DX7HR 0x10004dfd
// SYNTHETIC: DX7HR 0x10004e09
// SYNTHETIC: DX7HR 0x10004e14
// SYNTHETIC: DX7HR 0x10004e1e
// SYNTHETIC: DX7HR 0x10004e2f
// SYNTHETIC: DX7HR 0x10004e3b
// SYNTHETIC: DX7HR 0x10004e46
// SYNTHETIC: DX7HR 0x10004e50
// SYNTHETIC: DX7HR 0x10004e61
// SYNTHETIC: DX7HR 0x10004e6d

// The terrain draws moved to Rust (rust/crates/ee_dx7/src/draw_terrain.rs, docs/RUST_PLAN.md R4.2). The C++ here builds
// the plain views of the mesh for it and forwards; the fixed visible-vertex arrays and the polygon records are pinned.
#define DX7_PIN(cpp, rust, cppField, rustField) static_assert(offsetof(cpp, cppField) == offsetof(rust, rustField), #cppField)
static_assert(sizeof(GETerrainMesh::GETMRenderVertex) == sizeof(EeDx7RenderVertex), "ee_dx7/src/draw_terrain.rs RenderVertex");
DX7_PIN(GETerrainMesh::GETMRenderVertex, EeDx7RenderVertex, mPoint, point);
DX7_PIN(GETerrainMesh::GETMRenderVertex, EeDx7RenderVertex, mColor, color);
DX7_PIN(GETerrainMesh::GETMRenderVertex, EeDx7RenderVertex, mVisibility, visibility);
DX7_PIN(GETerrainMesh::GETMRenderVertex, EeDx7RenderVertex, mFog, fog);
DX7_PIN(GETerrainMesh::GETMRenderVertex, EeDx7RenderVertex, mClipFlags, clip_flags);
DX7_PIN(GETerrainMesh::GETMRenderVertex, EeDx7RenderVertex, mLinkedVisibility, linked_visibility);
static_assert(sizeof(GETerrainMesh::GETMPolygon) == sizeof(EeDx7TerrainPolygon), "ee_dx7/src/draw_terrain.rs TerrainPolygon");
DX7_PIN(GETerrainMesh::GETMPolygon, EeDx7TerrainPolygon, mVertices, vertices);
DX7_PIN(GETerrainMesh::GETMPolygon, EeDx7TerrainPolygon, mAlpha, alpha);
DX7_PIN(GETerrainMesh::GETMPolygon, EeDx7TerrainPolygon, mUV, uv);
static_assert(sizeof(GXMATERIAL) == 68 && sizeof(std::pair<U2DPoint<long>, U2DPoint<long> >) == 4 * sizeof(long), "ee_dx7 draw_terrain");

static EeDx7TerrainMeshView DX7TerrainViewOf(GETerrainMesh& inMesh)
{
    EeDx7TerrainMeshView theView;
    theView.mesh = &inMesh;
    theView.terrain_vertices = reinterpret_cast<EeDx7RenderVertex*>(&inMesh.GetTerrainRenderVertexArray()[0][0]);
    theView.water_vertices = reinterpret_cast<EeDx7RenderVertex*>(&inMesh.GetWaterRenderVertexArray()[0][0]);
    theView.terrain_visible = &inMesh.GetTerrainPointVisibilityArray()[0][0];
    theView.water_visible = &inMesh.GetWaterPointVisibilityArray()[0][0];
    theView.visible_tile_rows = inMesh.GetNumVisibleTileRows();
    inMesh.GetTerrainVisibleBottomLeftIndices(theView.start_x, theView.start_y);
    inMesh.GetTerrainVisibleTileDimensions(theView.width, theView.height);
    theView.water_percentage = inMesh.GetVisibleWaterTilePercentage();
    theView.is_space = inMesh.GetIsSpaceMap();
    theView.edge_world = reinterpret_cast<const float(*)[3]>(inMesh.GetEdgeWorldPoints().data());
    theView.edge_vertices = reinterpret_cast<EeDx7RenderVertex* const*>(inMesh.GetEdgeRenderVertices().data());
    theView.edge_count = inMesh.GetEdgeWorldPoints().size();
    theView.cliff_world = reinterpret_cast<const float(*)[3]>(inMesh.GetCliffWorldPoints().data());
    theView.cliff_vertices = reinterpret_cast<EeDx7RenderVertex* const*>(inMesh.GetCliffRenderVertices().data());
    theView.cliff_count = inMesh.GetCliffWorldPoints().size();
    return theView;
}

static EeDx7MaterialView DX7MaterialViewOf(GETerrainMesh::GETMMaterial& inMaterial)
{
    EeDx7MaterialView theView;
    theView.texture = inMaterial.mTexture;
    theView.polygons = reinterpret_cast<EeDx7TerrainPolygon* const*>(inMaterial.mPolygons.data());
    theView.polygon_count = inMaterial.mPolygons.size();
    theView.indices = reinterpret_cast<EeDx7TerrainPolygon**>(inMaterial.mIndices.data());
    theView.indices_length = inMaterial.mIndices.size();
    theView.strips = reinterpret_cast<EeDx7TerrainPolygon**>(inMaterial.mStripIndices.data());
    theView.strips_length = inMaterial.mStripIndices.size();
    return theView;
}

static std::vector<EeDx7MaterialView> DX7MaterialViewsOf(std::vector<GETerrainMesh::GETMMaterial>& inMaterials)
{
    std::vector<EeDx7MaterialView> theViews(inMaterials.size());
    for (size_t i = 0; i < inMaterials.size(); i++)
        theViews[i] = DX7MaterialViewOf(inMaterials[i]);
    return theViews;
}

extern "C" {
float ee_dx7cpp_mesh_z(void* inMesh, unsigned long inX, unsigned long inY) { return static_cast<GETerrainMesh*>(inMesh)->GetZ(inX, inY); }
void ee_dx7cpp_mesh_normal(void* inMesh, unsigned long inX, unsigned long inY, float* outNormal)
{
    GEVector* theNormal = static_cast<GETerrainMesh*>(inMesh)->GetVertexNormal(inX, inY);
    outNormal[0] = theNormal->GetX();
    outNormal[1] = theNormal->GetY();
    outNormal[2] = theNormal->GetZ();
}
void ee_dx7cpp_mesh_vertex_color(void* inMesh, unsigned long inX, unsigned long inY, float* outColor)
{
    const GEColor& theColor = static_cast<GETerrainMesh*>(inMesh)->GetVertexColor(inX, inY);
    outColor[0] = theColor.GetRed();
    outColor[1] = theColor.GetGreen();
    outColor[2] = theColor.GetBlue();
}
void ee_dx7cpp_mesh_water_color(void* inMesh, unsigned long inX, unsigned long inY, float* outColor)
{
    const GEColor& theColor = static_cast<GETerrainMesh*>(inMesh)->GetWaterColor(inX, inY);
    outColor[0] = theColor.GetRed();
    outColor[1] = theColor.GetGreen();
    outColor[2] = theColor.GetBlue();
}
bool ee_dx7cpp_mesh_valid_tile(void* inMesh, unsigned long inX, unsigned long inY) { return static_cast<GETerrainMesh*>(inMesh)->GetIsValidTile(inX, inY); }
bool ee_dx7cpp_light(void* inRasterizer, unsigned long inScreen, unsigned long inIndex, EeDx7LightData* outLight)
{
    if (gLightsInUse[inIndex] == 0)
        return false;
    GXLIGHT theLight;
    DX7Access(inRasterizer)->mDX7ScreenStates[inScreen]->mDevice->GetLight(inIndex, &theLight);
    outLight->light_type = theLight.dltType;
    outLight->diffuse[0] = theLight.dcvDiffuse.r;
    outLight->diffuse[1] = theLight.dcvDiffuse.g;
    outLight->diffuse[2] = theLight.dcvDiffuse.b;
    outLight->direction[0] = theLight.dvDirection.x;
    outLight->direction[1] = theLight.dvDirection.y;
    outLight->direction[2] = theLight.dvDirection.z;
    return true;
}
float ee_dx7cpp_um_arctan(float inValue) { return UMArcTan(inValue); }
float ee_dx7cpp_um_tan(float inAngle) { return UMTan(inAngle); }
float ee_dx7cpp_um_sin(float inAngle) { return UMSin(inAngle); }
float ee_dx7cpp_um_cos(float inAngle) { return UMCos(inAngle); }
}

// FUNCTION: DX7HR 0x10004e78
// Draws the visible terrain texture by texture, unclipped then clipped polygons (space maps draw materials 1..8 first);
// with no visible tile row fills the view with two black quads instead
long DX7Rasterizer::Draw(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    EeDx7TerrainMeshView theMesh = DX7TerrainViewOf(inMesh);
    EeDx7ViewportInfo theViewport = DX7ViewportInfoOf(this->mScreenStates[inScreen]->mViewport);
    std::vector<EeDx7MaterialView> theMaterials = DX7MaterialViewsOf(inMesh.GetTerrainMaterialList());
    std::vector<EeDx7MaterialMapEntry> theMap;
    std::map<GETexture*, unsigned long>& theSource = inMesh.GetTerrainMaterialMap();
    for (std::map<GETexture*, unsigned long>::iterator i = theSource.begin(); i != theSource.end(); i++) {
        EeDx7MaterialMapEntry theEntry = {i->first, i->second};
        theMap.push_back(theEntry);
    }
    return ee_dx7r_draw_terrain(this, inScreen, DX7DrawState(this, inScreen), &theMesh, theMap.data(), theMap.size(), theMaterials.data(),
                                theMaterials.size(), &theViewport, inOrthographic);
}

// FUNCTION: DX7HR 0x10005b93
// Draws grid lines between tile corners whose both ends are visible, as one red line list of at most 1200 lines
long DX7Rasterizer::DrawGridLines(GETerrainMesh& inMesh,
                                  std::vector<std::pair<U2DPoint<long>, U2DPoint<long> > >& inLines,
                                  GETexture* inTexture, float inWidth, unsigned long inScreen, bool inOrthographic)
{
    EeDx7TerrainMeshView theMesh = DX7TerrainViewOf(inMesh);
    EeDx7ViewportInfo theViewport = DX7ViewportInfoOf(this->mScreenStates[inScreen]->mViewport);
    return ee_dx7r_draw_grid_lines(this, inScreen, DX7DrawState(this, inScreen), &theMesh,
                                   reinterpret_cast<const long(*)[4]>(inLines.data()), inLines.size(), inTexture, inWidth, &theViewport,
                                   inOrthographic);
}

// FUNCTION: DX7HR 0x10006159
// Draws two red edge lines for every visible tile, one draw call per tile
long DX7Rasterizer::DrawTerrainGrid(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    EeDx7TerrainMeshView theMesh = DX7TerrainViewOf(inMesh);
    EeDx7ViewportInfo theViewport = DX7ViewportInfoOf(this->mScreenStates[inScreen]->mViewport);
    return ee_dx7r_draw_terrain_grid(this, inScreen, DX7DrawState(this, inScreen), &theMesh, &theViewport, inOrthographic);
}

// FUNCTION: DX7HR 0x100065d1
// Shades a tile rectangle with a translucent colour over every valid visible tile, drawn in batches of 400 quads
long DX7Rasterizer::DrawTerrainHighlight(GETerrainMesh& inMesh, const U2DRectangle<long>& inRectangle,
                                         unsigned long inScreen, bool inOrthographic, const GEColor& inColor,
                                         float inOpacity)
{
    EeDx7TerrainMeshView theMesh = DX7TerrainViewOf(inMesh);
    EeDx7ViewportInfo theViewport = DX7ViewportInfoOf(this->mScreenStates[inScreen]->mViewport);
    const long theRect[4] = {inRectangle.mLeft, inRectangle.mTop, inRectangle.mRight, inRectangle.mBottom};
    const float theColor[3] = {inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue()};
    return ee_dx7r_draw_terrain_highlight(this, inScreen, DX7DrawState(this, inScreen), &theMesh, theRect, theColor, inOpacity,
                                          &theViewport, inOrthographic);
}

// FUNCTION: DX7HR 0x10006c53
// Draws a material's NULL-terminated polygon list (count in its last element) in batches, with fog-darkened vertex
// colours; skips triangles whose vertices are all fully visible with no alpha
long DX7Rasterizer::DrawTerrainMaterial(GEViewport* inViewport, std::vector<GETerrainMesh::GETMPolygon*>& inPolygons,
                                        unsigned long inScreen)
{
    return ee_dx7r_draw_terrain_material(this, inScreen, DX7DrawState(this, inScreen),
                                         reinterpret_cast<EeDx7TerrainPolygon**>(inPolygons.data()), inPolygons.size());
}

// FUNCTION: DX7HR 0x10006f40
// Draws every water material, unclipped then clipped polygons, and counts its triangles
long DX7Rasterizer::DrawWater(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    EeDx7TerrainMeshView theMesh = DX7TerrainViewOf(inMesh);
    EeDx7ViewportInfo theViewport = DX7ViewportInfoOf(this->mScreenStates[inScreen]->mViewport);
    std::vector<EeDx7MaterialView> theMaterials = DX7MaterialViewsOf(inMesh.GetWaterMaterialList());
    return ee_dx7r_draw_water(this, inScreen, DX7DrawState(this, inScreen), &theMesh, theMaterials.data(), theMaterials.size(),
                              &theViewport, inOrthographic);
}

// FUNCTION: DX7HR 0x1000723b
// Lights the visible terrain and water vertices: ambient plus each active directional light, normalised, times
// the vertex colour
void DX7Rasterizer::Light(GETerrainMesh& inMesh, unsigned long inScreen)
{
    EeDx7TerrainMeshView theMesh = DX7TerrainViewOf(inMesh);
    const float theAmbient[3] = {this->mComputedAmbientColor.GetRed(), this->mComputedAmbientColor.GetGreen(),
                                 this->mComputedAmbientColor.GetBlue()};
    ee_dx7r_light_terrain(this, inScreen, DX7DrawState(this, inScreen), &theMesh, theAmbient);
}

// The water animation clock: advanced by every TransformToCamera that animates the water.
static float sWaterTime = 0.0f;

// FUNCTION: DX7HR 0x10007763
// Moves the visible terrain and water vertices to camera space (animating the water waves) and sets their clip flags,
// then sorts each material's polygons into unclipped and clipped lists; water-only mode skips dry vertices
void DX7Rasterizer::TransformToCamera(GETerrainMesh& inMesh, GEViewport& inViewport, float inScale, bool inWaterOnly,
                                      bool inOrthographic)
{
    EeDx7TerrainMeshView theMesh = DX7TerrainViewOf(inMesh);
    EeDx7ViewportInfo theViewport = DX7ViewportInfoOf(inViewport);
    std::vector<EeDx7MaterialView> theTerrainMaterials = DX7MaterialViewsOf(inMesh.GetTerrainMaterialList());
    std::vector<EeDx7MaterialView> theWaterMaterials = DX7MaterialViewsOf(inMesh.GetWaterMaterialList());
    EeDx7WaterRipple theRipple = {this->mWaterRipple0, this->mWaterRipple1, this->mWaterRipple2};
    ee_dx7r_transform_to_camera(&theMesh, theTerrainMaterials.data(), theTerrainMaterials.size(), theWaterMaterials.data(),
                                theWaterMaterials.size(), &theViewport, inScale, inWaterOnly, inOrthographic, theRipple, &sWaterTime);
}


// COMDATs this unit is the first (link order) to instantiate, then the template-static init chain head and atexit
// registrars:
// TEMPLATE: DX7HR 0x10008bc5
// TEMPLATE: DX7HR 0x10008bcc
// TEMPLATE: DX7HR 0x10008bef
// TEMPLATE: DX7HR 0x10008c1b
// TEMPLATE: DX7HR 0x10008c2d
// TEMPLATE: DX7HR 0x10008c42
// TEMPLATE: DX7HR 0x10008c54
// TEMPLATE: DX7HR 0x10008cae
// TEMPLATE: DX7HR 0x10008ce8
// TEMPLATE: DX7HR 0x10008d15
// TEMPLATE: DX7HR 0x10008d22
// TEMPLATE: DX7HR 0x10008d37
// TEMPLATE: DX7HR 0x10008d6f
// TEMPLATE: DX7HR 0x1000904d
// TEMPLATE: DX7HR 0x1000904e
// TEMPLATE: DX7HR 0x10009075
// TEMPLATE: DX7HR 0x1000908c
// TEMPLATE: DX7HR 0x100090b4
// TEMPLATE: DX7HR 0x100090d1
// TEMPLATE: DX7HR 0x100090fd
// TEMPLATE: DX7HR 0x10009127
// SYNTHETIC: DX7HR 0x10009152
// SYNTHETIC: DX7HR 0x1000917a
// SYNTHETIC: DX7HR 0x10009186
// SYNTHETIC: DX7HR 0x10009192
// SYNTHETIC: DX7HR 0x1000919e

// DX7HRTnLDisplay.dll (the T&L twin builds this unit unchanged): the same functions at their twin addresses.
// SYNTHETIC: DX7HRTNL 0x10004da6
// SYNTHETIC: DX7HRTNL 0x10004db0
// SYNTHETIC: DX7HRTNL 0x10004dc1
// SYNTHETIC: DX7HRTNL 0x10004dcd
// SYNTHETIC: DX7HRTNL 0x10004dd8
// SYNTHETIC: DX7HRTNL 0x10004de2
// SYNTHETIC: DX7HRTNL 0x10004df3
// SYNTHETIC: DX7HRTNL 0x10004dff
// SYNTHETIC: DX7HRTNL 0x10004e0a
// SYNTHETIC: DX7HRTNL 0x10004e14
// SYNTHETIC: DX7HRTNL 0x10004e25
// SYNTHETIC: DX7HRTNL 0x10004e31
// SYNTHETIC: DX7HRTNL 0x10004e3c
// SYNTHETIC: DX7HRTNL 0x10004e46
// SYNTHETIC: DX7HRTNL 0x10004e57
// SYNTHETIC: DX7HRTNL 0x10004e63
// SYNTHETIC: DX7HRTNL 0x10004e6e
// SYNTHETIC: DX7HRTNL 0x10004e78
// SYNTHETIC: DX7HRTNL 0x10004e89
// SYNTHETIC: DX7HRTNL 0x10004e95
// SYNTHETIC: DX7HRTNL 0x10004ea0
// SYNTHETIC: DX7HRTNL 0x10004eaa
// SYNTHETIC: DX7HRTNL 0x10004ebb
// SYNTHETIC: DX7HRTNL 0x10004ec7
// SYNTHETIC: DX7HRTNL 0x10004ed2
// SYNTHETIC: DX7HRTNL 0x10004edc
// SYNTHETIC: DX7HRTNL 0x10004eed
// SYNTHETIC: DX7HRTNL 0x10004ef9
// FUNCTION: DX7HRTNL 0x10004f04
// FUNCTION: DX7HRTNL 0x10005c1f
// FUNCTION: DX7HRTNL 0x100061e5
// FUNCTION: DX7HRTNL 0x1000665d
// FUNCTION: DX7HRTNL 0x10006cdf
// FUNCTION: DX7HRTNL 0x10006fcc
// FUNCTION: DX7HRTNL 0x100072c7
// FUNCTION: DX7HRTNL 0x100077ef
// TEMPLATE: DX7HRTNL 0x10008c51
// TEMPLATE: DX7HRTNL 0x10008c58
// TEMPLATE: DX7HRTNL 0x10008c7b
// TEMPLATE: DX7HRTNL 0x10008ca7
// TEMPLATE: DX7HRTNL 0x10008cb9
// TEMPLATE: DX7HRTNL 0x10008cce
// TEMPLATE: DX7HRTNL 0x10008d28
// TEMPLATE: DX7HRTNL 0x10008d62
// TEMPLATE: DX7HRTNL 0x10008d8f
// TEMPLATE: DX7HRTNL 0x10008da4
// TEMPLATE: DX7HRTNL 0x10008ddc
// TEMPLATE: DX7HRTNL 0x100090ba
// TEMPLATE: DX7HRTNL 0x100090bb
// TEMPLATE: DX7HRTNL 0x100090e2
// TEMPLATE: DX7HRTNL 0x100090f9
// TEMPLATE: DX7HRTNL 0x10009121
// TEMPLATE: DX7HRTNL 0x1000913e
// TEMPLATE: DX7HRTNL 0x1000916a
// TEMPLATE: DX7HRTNL 0x10009194
// SYNTHETIC: DX7HRTNL 0x100091bf
// SYNTHETIC: DX7HRTNL 0x100091e7
// SYNTHETIC: DX7HRTNL 0x100091f3
// SYNTHETIC: DX7HRTNL 0x100091ff
// SYNTHETIC: DX7HRTNL 0x1000920b
