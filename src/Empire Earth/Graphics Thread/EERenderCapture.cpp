// New (port), TEST ONLY: RFIL, the render fill's capture (ee/EERenderCapture.h; tools/rfil.py). Called by
// EERenderObject::FillRenderLists on the graphics thread under the world's read lock. The record is ee_gfx rfil.rs':
// this unit hands it the render lists (the pools and vectors in place, the multimaps flattened) and the world's key.
#include <windows.h>

#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "rust/base/math.h"
#include "rust/render/ge.h"
#include "rust/render/gfx.h"
#include "rust/sim/units.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGraphics.h"
#include "ee/EERenderLists.h"
#include "ee/EEPhysics.h"
#include "ee/EERenderCapture.h"
#include "rust/game/gamecpp.h"


// The fill's world key (the frames' second key): gamecpp render_object.rs ee_gamecpp_ro_world_key

void EERenderCaptureFill(const EERenderFrameInfo& inInfo, uint32_t inSwayClock, float inUnitCount)
{
    if (!ee_gfx_rfil_active())
        return;
    std::vector<EeGfxRfilPair> thePairs[6];
    EeGfxRfilFrame f = {};
    EERenderListsView(f, thePairs);
    f.frame = (uint32_t)inInfo.mFrame;
    f.ticks = gPhysics != NULL ? (uint32_t)gPhysics->mTicksRun : 0u;
    f.sway = inSwayClock;
    f.yaw = inInfo.mYaw;
    f.world_transformation = *reinterpret_cast<const EeGeTransformation*>(&gWorld.mTransformation);
    f.flags = (gFreeCamera ? 1u : 0u) | (gOrthographicView ? 2u : 0u) | (inInfo.mAmbient ? 4u : 0u);
    f.picked = inInfo.mPickedUnit != NULL ? (int32_t)inInfo.mPickedUnit->GetUnitID() : -1;
    f.world_key = ee_gamecpp_ro_world_key();
    f.fighting = inInfo.mNumFightingUnits;
    f.unit_count = inUnitCount;
    ee_gfx_rfil_write(&f);
}

// the renderer's hook (ee/EERenderCapture.h)
static const bool sEERenderCaptureRegistered = (gEERenderCaptureFill = EERenderCaptureFill, true);
