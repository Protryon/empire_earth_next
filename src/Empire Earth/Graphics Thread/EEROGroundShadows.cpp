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

// Prepares the frame and fills the render lists of shadows and units
void EEROGroundShadows::FrameStarting()
{
    this->Fn10C036();
    this->FillRenderLists();
}
