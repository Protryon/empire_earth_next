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
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGraphics.h"
#include "ee/EERenderObject.h"
#include "ee/EERenderLists.h"
#include "ee/EEROTerrain.h"
#include "ee/EERenderSnapshot.h"
#include "rust/render/gfx.h"
#include "rust/game/gamecpp.h"


// the brush highlight colour (0x5293e8; its init chain 0x51251f: GEColor's default white) (provisional name)
GEColor gBrushHighlightColor;
// the map's tile runs as of the last frame, drawn as grid lines (0x5290a0) (provisional name)
std::vector<RTSMapTileRun> gGridRuns;

GETexture* gAreaEffectTexture;
GETexture* gCircleAreaEffectTexture;
