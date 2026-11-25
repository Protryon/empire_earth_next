// C:\EEX\Empire Earth\Graphics Thread\EEROGroundShadows.cpp (no __FILE__; name from the RTTI class). The ground
// shadows' render object (vtable 0x4413f0): 0x111dae..0x11251f (ctor, ??_G, dtor, Virtual00, the static shadow
// model's set-up, Virtual08, the two shadow passes, the space-tile test, the model's release); EEROTerrain.cpp's
// init chains start at 0x11251f. The shared quad, the scene pass and both shadow passes are ee_gfx shadows.rs's
// (RUST_PLAN R4.4): the C++ hands over the frame's render lists and draws the effects' ground pass.
// Unit flags: the exe's module set.
#include <windows.h>

#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEUnit.h"
#include "ee/EERenderObject.h"
#include "ee/EEROGroundShadows.h"
#include "rust/game/gamecpp.h"

// the rasterizer as the Rust draws call it (EERenderObject.cpp)

// Creates the ground shadows' render object
EEROGroundShadows::EEROGroundShadows()
{
}

// Destroys the ground shadows' render object
EEROGroundShadows::~EEROGroundShadows()
{
}

// Prepares the frame and fills the render lists of shadows and units
void EEROGroundShadows::FrameStarting()
{
    this->Fn10C036();
    this->FillRenderLists();
}

// Creates the shared shadow quad model (StaticShadow): a unit square in the ground plane (ee_gfx shadows.rs)
void EEROGroundShadows::CreateShadowModel()
{
    ee_gfx_ground_shadows_create(ee_gamecpp_ro_raster());
}

// Draws the static shadows and unit shadows (when ground shadows are on), then the effects' ground pass (queue 0)
// per texture (gamecpp ro_views.rs over the frame's lists; ee_gfx shadows.rs)
void EEROGroundShadows::RenderScene()
{
    ee_gamecpp_gs_render_scene();
}

// Releases the shared shadow quad model (ee_gfx shadows.rs)
void EEROGroundShadows::ReleaseShadowModel()
{
    ee_gfx_ground_shadows_release(ee_gamecpp_ro_raster());
}

void EEROGroundShadows::FrameDone()
{
}

void EEROGroundShadows::SceneStarting()
{
}
