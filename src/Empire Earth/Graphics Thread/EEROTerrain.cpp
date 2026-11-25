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
#include "lle/GEColor.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEScreen.h"
#include "lle/GEVertexArray.h"

#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EERenderObject.h"
#include "ee/EEROTerrain.h"
#include "rust/game/gamecpp.h"

// the brush highlight colour (0x5293e8), the grid runs of the frame (0x5290a0) and the area-effect textures
// (gAreaEffectTexture 0x5293f4, gCircleAreaEffectTexture 0x5293f8): gamecpp ro_views.rs

// Creates the terrain's render object with the terrain grid off
EEROTerrain::EEROTerrain()
{
    this->mGridMode = 0;
}

// Destroys the terrain's render object
EEROTerrain::~EEROTerrain()
{
}

// The frame (the sky colours, the clouds, the fog of war, the grid runs, the terrain's snapshot), the sky media and
// the scene's draws: gamecpp ro_views.rs (ee_gfx ro_terrain.rs)
void EEROTerrain::FrameStarting()
{
    ee_gamecpp_te_frame_starting(this);
}

void EEROTerrain::CreateSkyModel()
{
    ee_gamecpp_te_create_sky_model();
}

void EEROTerrain::RenderScene()
{
    ee_gamecpp_te_render_scene(this);
}

// Steps the terrain grid mode forwards or backwards through 0..17, wrapping around
void EEROTerrain::StepGridMode(bool inBackwards)
{
    this->mGridMode = ee_gfx_terrain_grid_mode_step((int32_t)this->mGridMode, inBackwards);   // ee_gfx terrain.rs
}

// Releases the sky, cloud and star models and textures and the two area-effect textures (ee_gfx ro_terrain.rs)
void EEROTerrain::ReleaseSkyModels()
{
    ee_gamecpp_te_release_sky_models();
}

void EEROTerrain::FrameDone()
{
}

void EEROTerrain::SceneStarting()
{
}
