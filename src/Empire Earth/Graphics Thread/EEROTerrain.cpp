// C:\EEX\Empire Earth\Graphics Thread\EEROTerrain.cpp (__FILE__ 0x4be5f4). The terrain's render object (vtable
// 0x441b7c): 0x11251f..0x114296 (four init chains, the ctor, ??_G, the dtor, Virtual00, the star/cloud/sky models,
// Virtual08, the sky index step, the release, the cloud layer, the sky colours, the coloured rectangles' erase
// COMDATs and RTSWorld's inline map accessor); EEROUnits.cpp's init chains start at 0x114296. The sky, star and cloud
// models, their textures and the area-effect textures, the frame's sky colours and cloud layer and the scene pass are
// ee_gfx ro_terrain.rs's (RUST_PLAN R4.4; the .rdata tables 0x41142c..0x441b98 and the sky colours 0x5290b0 with
// them): the C++ draws the terrain mesh, its highlights, grid and lines and the effects' terrain pass for it.
// Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GEPolygonArray.h"
#include "lle/GERasterizer.h"
#include "lle/TSCriticalSection.h"
#include "lle/U2DRectangle.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EERenderLists.h"
#include "ee/EEROTerrain.h"
#include "ee/EERenderSnapshot.h"
#include "rust/render/gfx.h"

extern unsigned long& gTextureMipLevelsToDrop;
extern bool& gDrawClouds;                    // 0x5308d3 (the clouds are drawn; provisional name)
extern bool& gTrilinearFilter;               // 0x5308da (the sky textures use mip filtering; provisional name)
// the rasterizer as the Rust draws call it (EERenderObject.cpp)
extern const EeGfxRaster gEERustRaster;

// the brush highlight colour (0x5293e8; its init chain 0x51251f: GEColor's default white) (provisional name)
GEColor gBrushHighlightColor;
// the map's tile runs as of the last frame, drawn as grid lines (0x5290a0) (provisional name)
std::vector<RTSMapTileRun> gGridRuns;

GETexture* gAreaEffectTexture;
GETexture* gCircleAreaEffectTexture;

// Creates the terrain's render object with the terrain grid off
EEROTerrain::EEROTerrain()
{
    this->mGridMode = 0;
}

// Destroys the terrain's render object
EEROTerrain::~EEROTerrain()
{
}

static EeGfxTerrainMedia EETerrainMedia()
{
    EeGfxTerrainMedia theMedia = {(uint32_t)gTextureMipLevelsToDrop, gTrilinearFilter, gDrawClouds};
    return theMedia;
}

// Prepares the frame: updates the sky colours and models, the clouds (black fog colour on space maps instead),
// the fog of war (all seen for see-all or the free camera) and copies the map's grid runs
void EEROTerrain::FrameStarting()
{
    this->Fn10C036();
    EEMap* theMap = gWorld.mMap;
    EeGfxTerrainFrame theFrame = {gRenderEnvironment.mTimeOfDay, (int32_t)theMap->mSize, (int32_t)theMap->mL195614,
                                  theMap->mTerrainMesh.GetIsSpaceMap(), gRenderEnvironment.mCloudOffset, EETerrainMedia()};
    EeGfxTerrainFrameOut theOut = ee_gfx_ro_terrain_frame(&gEERustRaster, &theFrame);
    gU4E9220 = GEColor(theOut.fog.red, theOut.fog.green, theOut.fog.blue);
    gRenderEnvironment.mCloudOffset = theOut.cloud_offset;
    this->Fn10E8D1();
    theMap->mTerrainMesh.UpdateFogOfWar(gWorld.GetCurrentPlayer()->mSeesAll || gFreeCamera);

    gGridRuns = gWorld.mMap->mU1955DC;
    EETakeTerrainSnapshot();   // port: what RenderScene draws from the map and the editor (ee/EERenderSnapshot.h)
}

// Creates the sky dome model and its colour table, the star models, the clouds (when clouds are on) and
// loads the area-effect textures (square and circle) (ee_gfx ro_terrain.rs, with CreateStarModels 0x5126ce and
// CreateCloudModel 0x512bc4)
void EEROTerrain::CreateSkyModel()
{
    EeGfxTerrainMedia theMedia = EETerrainMedia();
    EeGfxTerrainTextures theTextures = ee_gfx_ro_terrain_create(&gEERustRaster, &theMedia);
    gAreaEffectTexture = static_cast<GETexture*>(theTextures.area_effect);
    gCircleAreaEffectTexture = static_cast<GETexture*>(theTextures.circle_area_effect);
}

namespace {
// the terrain mesh's draws from the frame's snapshot (port: EETakeTerrainSnapshot took the editor's brush and the
// map's coloured rectangles, and erased the erase markers, where this drew them from the live map and its lock)
void TerrainDraw(void*) { gRasterizer->Draw(*gRenderSnapshot.mTerrainMesh, gScreenIndex, gOrthographicView); }
void TerrainHighlights(void*, float inOpacity)
{
    GETerrainMesh& theMesh = *gRenderSnapshot.mTerrainMesh;
    if (gRenderSnapshot.mBrushHighlightShown)
        gRasterizer->DrawTerrainHighlight(theMesh, gRenderSnapshot.mBrushTiles, gScreenIndex, gOrthographicView,
                                          gBrushHighlightColor, inOpacity);
    for (unsigned long theRect = 0; theRect < gRenderSnapshot.mColoredRectangles.size(); theRect++) {
        const RTSColoredRectangle& theColored = gRenderSnapshot.mColoredRectangles[theRect];
        gRasterizer->DrawTerrainHighlight(theMesh, theColored.mRect, gScreenIndex, gOrthographicView, theColored.mColor,
                                          theColored.mOpacity);
    }
}
void TerrainGrid(void*) { gRasterizer->DrawTerrainGrid(*gRenderSnapshot.mTerrainMesh, gScreenIndex, gOrthographicView); }
void TerrainGridLines(void*)
{
    gRasterizer->DrawGridLines(*gRenderSnapshot.mTerrainMesh, gGridRuns, gAreaEffectTexture, gU4E9330, gScreenIndex,
                               gOrthographicView);
}
void TerrainLines(void*)
{
    for (unsigned long i = 0; i < gU529050.size(); i++)
        gRasterizer->Draw3DLine(gU529050[i].mPoints, gU529050[i].mTexture, gU529050[i].mWidth, gScreenIndex, false);
}
// the effects' terrain pass: queue 2, batched per texture
void TerrainEffects(void*)
{
    for (EEEffectQueue::iterator i = gEffectQueues[2].begin(); i != gEffectQueues[2].end(); i++) {
        BeginEffectBatches(i->first);
        for (std::deque<EEEffect*>::iterator theEffect = i->second.begin(); theEffect != i->second.end(); theEffect++)
            (*theEffect)->DrawPass4();
        FlushEffectBatches();
        EndEffectBatches();
    }
}
}

// Draws the sky (stars on space maps, the inner layer only with clouds on; else sky and clouds), the terrain,
// the editor brush highlight, the map's coloured rectangles (erasing faded ones), grid, lines and ground effects
// (ee_gfx ro_terrain.rs)
void EEROTerrain::RenderScene()
{
    EeGfxTerrainScene theScene = {NULL, gRenderSnapshot.mIsSpaceMap, gDrawClouds, gOrthographicView,
                                  this->mGridMode != 0, gGridRuns.size(), TerrainDraw, TerrainHighlights, TerrainGrid,
                                  TerrainGridLines, TerrainLines, TerrainEffects};
    ee_gfx_ro_terrain_render(&gEERustRaster, &theScene);
}

// Steps the terrain grid mode forwards or backwards through 0..17, wrapping around
void EEROTerrain::StepGridMode(bool inBackwards)
{
    this->mGridMode = ee_gfx_terrain_grid_mode_step((int32_t)this->mGridMode, inBackwards);   // ee_gfx terrain.rs
}

// Releases the sky, cloud and star models and textures and the two area-effect textures (ee_gfx ro_terrain.rs)
void EEROTerrain::ReleaseSkyModels()
{
    ee_gfx_ro_terrain_release(&gEERustRaster);
    gAreaEffectTexture = NULL;
    gCircleAreaEffectTexture = NULL;
}

void EEROTerrain::FrameDone()
{
}

void EEROTerrain::SceneStarting()
{
}

void EEROTerrain::Virtual14(long)
{
}
