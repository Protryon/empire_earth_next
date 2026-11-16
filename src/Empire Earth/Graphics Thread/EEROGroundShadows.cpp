// C:\EEX\Empire Earth\Graphics Thread\EEROGroundShadows.cpp (no __FILE__; name from the RTTI class). The ground
// shadows' render object (vtable 0x4413f0): 0x111dae..0x11251f (ctor, ??_G, dtor, Virtual00, the static shadow
// model's set-up, Virtual08, the two shadow passes, the space-tile test, the model's release); EEROTerrain.cpp's
// init chains start at 0x11251f. The shared quad, the scene pass and both shadow passes are ee_gfx shadows.rs's
// (RUST_PLAN R4.4): the C++ hands over the frame's render lists and draws the effects' ground pass.
// Unit flags: the exe's module set.
#include <windows.h>

#include <vector>

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
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GEPolygonArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EERenderLists.h"
#include "ee/EEROGroundShadows.h"
#include "ee/EERenderSnapshot.h"
#include "rust/ee_gfx.h"

// the rasterizer as the Rust draws call it (EERenderObject.cpp)
extern const EeGfxRaster gEERustRaster;

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
    ee_gfx_ground_shadows_create(&gEERustRaster);
}

namespace {
// the terrain's water info of a tile, -1 off the mesh (GetIsOverSpace, 0x51246b; the frame's snapshot)
int32_t ShadowTileWater(void*, uint32_t inX, uint32_t inY)
{
    GETerrainMesh* theMesh = gRenderSnapshot.mTerrainMesh;
    if (!theMesh->GetIsValidTile(inX, inY))
        return -1;
    return (int32_t)theMesh->GetWaterInfo(inX, inY);
}

// the effects' ground pass: queue 0, batched per texture
void ShadowEffects(void*)
{
    EEEffectQueue::iterator theIterator;
    std::deque<EEEffect*>::iterator theEffect;
    for (theIterator = gEffectQueues[0].begin(); theIterator != gEffectQueues[0].end(); theIterator++) {
        BeginEffectBatches(theIterator->first);
        for (theEffect = theIterator->second.begin(); theEffect != theIterator->second.end(); theEffect++)
            (*theEffect)->DrawPass2();
        FlushEffectBatches();
        EndEffectBatches();
    }
}
}

// Draws the static shadows and unit shadows (when ground shadows are on), then the effects' ground pass
// (queue 0) per texture, alpha-blended with z test only (ee_gfx shadows.rs over the frame's lists; the units that
// cast no shadow, submerged or without a shadow scale, are left out here)
void EEROGroundShadows::RenderScene()
{
    std::vector<EeGfxShadow> theShadows;
    std::vector<EeGfxUnitShadow> theUnits;
    if (gDrawGroundShadows) {
        EERenderShadowMap& theShadowMap = EERenderLists::sInstance.mShadows2;
        for (EERenderShadowMap::iterator i = theShadowMap.begin(); i != theShadowMap.end(); i++) {
            const EERenderShadow* s = i->second;
            EeGfxShadow theShadow = {i->first, {s->mLocation.mX, s->mLocation.mY, s->mLocation.mZ}, s->mScale, s->mOpacity};
            theShadows.push_back(theShadow);
        }
        EERenderUnitMap& theUnitMap = EERenderLists::sInstance.mUnits2;
        for (EERenderUnitMap::iterator i = theUnitMap.begin(); i != theUnitMap.end(); i++) {
            const EERenderUnit* u = i->second;
            if (u->mCategory == kUnitCategorySubmergedNaval || !(u->mShadowScale > 0.0f))
                continue;
            EeGfxUnitShadow theUnit;
            theUnit.transformation = *reinterpret_cast<const EeGeTransformation*>(&u->mTransformation);
            theUnit.model = u->mSource->mShadowModel;
            theUnit.texture = u->mSource->mShadowTexture;
            theUnit.category = (int32_t)u->mCategory;
            theUnit.shadow_scale = u->mShadowScale;
            theUnit.ground_z = u->mGroundZ;
            theUnit.scale = u->mScale;
            theUnit.class_scale = gUnitClassTable[u->mUnitClassIndex]->GetScale();
            theUnit.opacity = u->mOpacity;
            theUnits.push_back(theUnit);
        }
    }
    EeGfxShadowScene theScene = {NULL, theShadows.empty() ? NULL : &theShadows[0], theShadows.size(),
                                 theUnits.empty() ? NULL : &theUnits[0], theUnits.size(), gDrawGroundShadows,
                                 gRenderSnapshot.mIsSpaceMap, ShadowTileWater, ShadowEffects};
    ee_gfx_ground_shadows_render(&gEERustRaster, &theScene);
}

// Releases the shared shadow quad model (ee_gfx shadows.rs)
void EEROGroundShadows::ReleaseShadowModel()
{
    ee_gfx_ground_shadows_release(&gEERustRaster);
}

void EEROGroundShadows::FrameDone()
{
}

void EEROGroundShadows::SceneStarting()
{
}
