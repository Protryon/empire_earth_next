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
#include "gx/GX.h"

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7PolygonArray.h"
#include "dx7/DX7StandardVertexArray.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "rust/ee_dx7.h"

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

// The terrain draws moved to Rust (rust/crates/ee_dx7/src/draw_terrain.rs, docs/RUST_PLAN.md R4.2), which reads the mesh's
// visible frame off its Rust object (ee_ge tmobj.rs: the views, the materials, the points; draw_ffi.rs pins the
// records). The C++ forwards.
static_assert(sizeof(std::pair<U2DPoint<long>, U2DPoint<long> >) == 4 * sizeof(long), "ee_dx7 draw_terrain grid lines");

// FUNCTION: DX7HR 0x10004e78
// Draws the visible terrain texture by texture, unclipped then clipped polygons (space maps draw materials 1..8 first);
// with no visible tile row fills the view with two black quads instead
long DX7Rasterizer::Draw(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    return ee_dx7r_draw_terrain(this->mDX7, inScreen, inMesh.GetRust(), inOrthographic);
}

// FUNCTION: DX7HR 0x10005b93
// Draws grid lines between tile corners whose both ends are visible, as one red line list of at most 1200 lines
long DX7Rasterizer::DrawGridLines(GETerrainMesh& inMesh,
                                  std::vector<std::pair<U2DPoint<long>, U2DPoint<long> > >& inLines,
                                  GETexture* inTexture, float inWidth, unsigned long inScreen, bool inOrthographic)
{
    return ee_dx7r_draw_grid_lines(this->mDX7, inScreen, inMesh.GetRust(), reinterpret_cast<const long(*)[4]>(inLines.data()),
                                   inLines.size(), inTexture, inWidth, inOrthographic);
}

// FUNCTION: DX7HR 0x10006159
// Draws two red edge lines for every visible tile, one draw call per tile
long DX7Rasterizer::DrawTerrainGrid(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    return ee_dx7r_draw_terrain_grid(this->mDX7, inScreen, inMesh.GetRust(), inOrthographic);
}

// FUNCTION: DX7HR 0x100065d1
// Shades a tile rectangle with a translucent colour over every valid visible tile, drawn in batches of 400 quads
long DX7Rasterizer::DrawTerrainHighlight(GETerrainMesh& inMesh, const U2DRectangle<long>& inRectangle,
                                         unsigned long inScreen, bool inOrthographic, const GEColor& inColor,
                                         float inOpacity)
{
    const long theRect[4] = {inRectangle.mLeft, inRectangle.mTop, inRectangle.mRight, inRectangle.mBottom};
    return ee_dx7r_draw_terrain_highlight(this->mDX7, inScreen, inMesh.GetRust(), theRect, reinterpret_cast<const float*>(&inColor),
                                          inOpacity, inOrthographic);
}

// FUNCTION: DX7HR 0x10006f40
// Draws every water material, unclipped then clipped polygons, and counts its triangles
long DX7Rasterizer::DrawWater(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    return ee_dx7r_draw_water(this->mDX7, inScreen, inMesh.GetRust(), inOrthographic);
}

// FUNCTION: DX7HR 0x1000723b
// Lights the visible terrain and water vertices: ambient plus each active directional light, normalised, times
// the vertex colour
void DX7Rasterizer::Light(GETerrainMesh& inMesh, unsigned long inScreen) { ee_dx7r_light_terrain(this->mDX7, inScreen, inMesh.GetRust()); }

// FUNCTION: DX7HR 0x10007763
// Moves the visible terrain and water vertices to camera space (animating the water waves) and sets their clip flags,
// then sorts each material's polygons into unclipped and clipped lists; water-only mode skips dry vertices
void DX7Rasterizer::TransformToCamera(GETerrainMesh& inMesh, GEViewport& inViewport, float inScale, bool inWaterOnly,
                                      bool inOrthographic)
{
    ee_dx7r_transform_to_camera(this->mDX7, inMesh.GetRust(), &inViewport, inScale, inWaterOnly, inOrthographic);
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
