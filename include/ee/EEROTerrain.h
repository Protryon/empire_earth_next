// C:\EEX\Empire Earth\Graphics Thread\EEROTerrain.h (RECONSTRUCTION; file name inferred). The terrain's render object
// (RTTI EEROTerrain > EERenderObject > RTSRenderObject, vtable 0x441b7c, sizeof 8; EEROTerrain.cpp 0x11251f..0x114296,
// __FILE__ 0x4be5f4): the sky, cloud and star models,
// the terrain, its highlights and grid. Names are provisional. Requires ee/EERenderObject.h.
#ifndef EE_EEROTERRAIN_H
#define EE_EEROTERRAIN_H

#include "rust/game/gamecpp.h"
#include "rust/render/gfx.h"
#include "ee/EERenderObject.h"
class GEModel;
class GETexture;

class EEROTerrain : public EERenderObject {
public:
    // Creates the terrain's render object with the terrain grid off
    EEROTerrain() { this->mGridMode = 0; }
    // Destroys the terrain's render object
    // +0x10 dtor 0x11262d (??_G 0x112611)
    virtual ~EEROTerrain() {  }
    // The frame (the sky colours, the clouds, the fog of war, the grid runs, the terrain's snapshot), the sky media and
    // the scene's draws: gamecpp ro_views.rs (ee_gfx ro_terrain.rs)
    // +0x00 0x112638
    virtual void FrameStarting() { ee_gamecpp_te_frame_starting(this); }
    // +0x04 0xe123d trivial: ret (ICF)
    virtual void SceneStarting() {  }
    // +0x08 0x1131a4
    virtual void RenderScene() { ee_gamecpp_te_render_scene(this); }
    // +0x0c 0xe123d trivial: ret (ICF)
    virtual void FrameDone() {  }

    // Steps the terrain grid mode forwards or backwards through 0..17, wrapping around
    // 0x1138a6: mGridMode cycles 0..0x11 (provisional name)
    void StepGridMode(bool inBackwards) { this->mGridMode = ee_gfx_terrain_grid_mode_step((int32_t)this->mGridMode, inBackwards); }

    long mGridMode;                         // +0x04 (= 0 in the ctor; non-zero: Virtual08 draws the terrain grid) (provisional name)
};

// (the area-effect textures 0x5293f4 and 0x5293f8 are Rust's: gamecpp ro_views.rs)

#endif
