// port: the render phase's snapshot of the world (ee/EERenderSnapshot.h; RUST_PLAN R5a.3, PORT_LEARNINGS "R5a.3").
// No original address: the original renderer read these from the live world after FrameStarting had released the
// world's lock. Taken under that lock, so the physics tick is untouched: the snapshot only reads (and, for the
// coloured rectangles, erases the map's erase markers, which EEROTerrain::RenderScene did a moment later).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEGraphics.h"
#include "ee/EERenderObject.h"
#include "ee/EERenderLists.h"
#include "ee/EEROTerrain.h"
#include "ee/EEROUserInterface.h"
#include "ee/EERenderSnapshot.h"
#include "ee/EEMouse.h"
#include "rust/game/gamecpp.h"

EERenderSnapshot gRenderSnapshot;

EERenderSnapshot::EERenderSnapshot()
    : mCameraYaw(0.0f), mCameraPitch(0.0f), mInCinematic(false), mTerrainMesh(NULL), mIsSpaceMap(false),
      mBrushHighlightShown(false)
{
}

// the snapshot's takes (the world-wide part under EEGraphics::FrameStarting's lock, EEROTerrain's part in its
// FrameStarting) and the other render objects' bodies: gamecpp ro_views.rs

// ------------------------------------------------------------------------- round 10, lane A: the render objects' C++ (gamecpp ro_views.rs)

extern unsigned long& gTextureMipLevelsToDrop;
extern bool& gDrawClouds;                    // 0x5308d3 (EEROTerrain.cpp)
extern bool& gTrilinearFilter;               // 0x5308da (EEROTerrain.cpp)
extern GEColor gBrushHighlightColor;         // EEROTerrain.cpp
extern std::vector<RTSMapTileRun> gGridRuns; // EEROTerrain.cpp
extern GETexture* gCircleAreaEffectTexture;  // EEROTerrain.cpp
extern GEViewport gUIViewport;               // EEROUserInterface.cpp

// the mouse frame's slots and calls (RTSMouse.cpp)
GESurface** RTSMouseSlot(RTSMouse* inMouse, unsigned long inIndex);
long RTSMouseFrameCall(RTSMouse* inMouse, uint32_t inOp, GESurface* inSurface, const GE2DPoint* inOrigin, bool inFlag);

#define RV_P64(x) ((int64_t)(intptr_t)(x))
#define MO_MOUSE ((EEMouse*)p)
#define RV_MAP ((EEMap*)p)
#define RV_MESH (*gRenderSnapshot.mTerrainMesh)
#define RV_COLORED(k) (gRenderSnapshot.mColoredRectangles[k])

// the C++ calls and members of gamecpp ro_views.rs (EEROUnits.cpp's EERoUnitsOp default): one statement each
int64_t EECore10OpB5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEDataTextFile.cpp
int64_t EERoViewsOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the UI's render object
    case O::GuCopyData: ((EEROUserInterface*)p)->mUIManager->CopyData();
        break;
    case O::GuSelectedGroup: return RV_P64(((EEPlayer*)p)->mSelectedGroup);
    case O::GuGroupFirstUnit: return RV_P64(((EESelectedGroup*)p)->GetFirstUnit());
    case O::GuCurrentViewedUnit: return RV_P64(gWorld.GetCurrentPlayer()->mSelectedViewedUnit);
    case O::RuMainViewRectangle: *(GERectangle*)a[0] = gU4E8F08.GetViewRectangle();
        break;
    case O::GuMainUiState: return gUIManager->GetMainGameForm()->mUIState;
    case O::GuTargetWidth: return gRasterizer->GetRenderTarget(gScreenIndex)->GetWidth();
    case O::GuTargetHeight: return gRasterizer->GetRenderTarget(gScreenIndex)->GetHeight();
    case O::GuUiViewportSetRectangle: gUIViewport.SetViewRectangle(*(const GERectangle*)a[0]);
        break;
    case O::GuSetUiViewport: gRasterizer->SetViewport(gUIViewport, gScreenIndex);
        break;
    case O::GuDrawPolygons: ((EEROUserInterface*)p)->mUIManager->DrawPolygons();
        break;
    // the water
    case O::WaDrawWater: gRasterizer->DrawWater(RV_MESH, gScreenIndex, gOrthographicView);
        break;
    // the ground shadows
    case O::GsDrawGroundShadows: return gDrawGroundShadows;
    case O::GsShadowModel: return RV_P64(((EEGraphicObject*)p)->mShadowModel);
    case O::GsShadowTexture: return RV_P64(((EEGraphicObject*)p)->mShadowTexture);
    case O::GsTileWater: return !RV_MESH.GetIsValidTile((uint32_t)a[0], (uint32_t)a[1]) ? -1 : (int64_t)(int32_t)RV_MESH.GetWaterInfo((uint32_t)a[0], (uint32_t)a[1]);
    // the terrain
    case O::TeMipLevelsToDrop: return gTextureMipLevelsToDrop;
    case O::TeTrilinear: return gTrilinearFilter;
    case O::TeDrawClouds: return gDrawClouds;
    case O::TeMapSize: return gWorld.mMap->mSize;
    case O::TeMapIsSpace: return gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    case O::TeMeshIsSpace: return ((GETerrainMesh*)p)->GetIsSpaceMap();
    case O::TeUpdateFogOfWar: gWorld.mMap->mTerrainMesh.UpdateFogOfWar(gWorld.GetCurrentPlayer()->GetSeesAll() || gFreeCamera);
        break;
    case O::TeCopyGridRuns: gGridRuns = gWorld.mMap->mU1955DC;
        break;
    case O::TeSetAreaTexture: gAreaEffectTexture = (GETexture*)a[0];
        break;
    case O::TeSetCircleTexture: gCircleAreaEffectTexture = (GETexture*)a[0];
        break;
    case O::TeGridMode: return ((EEROTerrain*)p)->mGridMode;
    case O::TeGridRunCount: return (int64_t)gGridRuns.size();
    case O::TeDrawMesh: gRasterizer->Draw(RV_MESH, gScreenIndex, gOrthographicView);
        break;
    case O::TeDrawBrush: gRasterizer->DrawTerrainHighlight(RV_MESH, gRenderSnapshot.mBrushTiles, gScreenIndex, gOrthographicView, gBrushHighlightColor, f[0]);
        break;
    case O::TeDrawColored: gRasterizer->DrawTerrainHighlight(RV_MESH, RV_COLORED(a[0]).mRect, gScreenIndex, gOrthographicView, RV_COLORED(a[0]).mColor, RV_COLORED(a[0]).mOpacity);
        break;
    case O::TeDrawGrid: gRasterizer->DrawTerrainGrid(RV_MESH, gScreenIndex, gOrthographicView);
        break;
    case O::TeDrawGridLines: gRasterizer->DrawGridLines(RV_MESH, gGridRuns, gAreaEffectTexture, gU4E9330, gScreenIndex, gOrthographicView);
        break;
    case O::TeDraw3DLine: gRasterizer->Draw3DLine(gU529050[a[0]].mPoints, gU529050[a[0]].mTexture, gU529050[a[0]].mWidth, gScreenIndex, false);
        break;
    // the snapshot
    case O::SnMap: return RV_P64(gWorld.mMap);
    case O::SnCamera: gRenderSnapshot.mCamera = gWorld.mTransformation;
        break;
    case O::SnCameraYaw: gRenderSnapshot.mCameraYaw = gWorld.GetCameraYaw();
        break;
    case O::SnCameraPitch: gRenderSnapshot.mCameraPitch = gWorld.GetCameraPitch();
        break;
    case O::SnCinematic: gRenderSnapshot.mInCinematic = gWorld.mInCinematic;
        break;
    case O::SnMeshOf: return RV_P64(&RV_MAP->mTerrainMesh);
    case O::SnSetMesh: gRenderSnapshot.mTerrainMesh = (GETerrainMesh*)a[0];
        break;
    case O::SnSetSpaceMap: gRenderSnapshot.mIsSpaceMap = a[0] != 0;
        break;
    case O::SnIsSpaceMap: return gRenderSnapshot.mIsSpaceMap;
    case O::SnColoredClear: gRenderSnapshot.mColoredRectangles.clear();
        break;
    case O::SnColoredCount: return (int64_t)gRenderSnapshot.mColoredRectangles.size();
    case O::SnColoredPush: gRenderSnapshot.mColoredRectangles.push_back(RV_MAP->mColoredRectangles[a[0]]);
        break;
    case O::SnMapColoredCount: return (int64_t)RV_MAP->mColoredRectangles.size();
    case O::SnMapColoredLock: RV_MAP->mCriticalSectionForColoredRectangles->Enter();
        break;
    case O::SnMapColoredUnlock: RV_MAP->mCriticalSectionForColoredRectangles->Leave();
        break;
    case O::SnMapColoredErase: RV_MAP->GetColoredRectangles().erase(RV_MAP->GetColoredRectangles().begin() + a[0]);
        break;
    case O::SnBrushShown: return gRenderSnapshot.mBrushHighlightShown;
    case O::SnSetBrushShown: gRenderSnapshot.mBrushHighlightShown = a[0] != 0;
        break;
    case O::SnEditorPresent: return gUIManager->GetScenarioEditorForm() != NULL;
    case O::SnBrushRectangle: *(GERectangle*)a[0] = gUIManager->GetScenarioEditorForm()->GetBrushGroup()->mRectangle;
        break;
    case O::SnBrushHighlightShown: return gUIManager->GetScenarioEditorForm()->GetBrushGroup()->mIsHighlightShown;
    case O::SnSetBrushTiles: gRenderSnapshot.mBrushTiles.mLeft = (long)a[0], gRenderSnapshot.mBrushTiles.mTop = (long)a[1], gRenderSnapshot.mBrushTiles.mRight = (long)a[2], gRenderSnapshot.mBrushTiles.mBottom = (long)a[3];
        break;
    // RFIL
    case O::RcPhysicsPresent: return gPhysics != NULL;
    case O::MoLoadPointer: return MO_MOUSE->LoadPointer((const char*)a[0], (GEScreen*)a[1], RTSMouseSlot(MO_MOUSE, 10 + (unsigned long)a[2]));
    case O::MoRtsLoad: return MO_MOUSE->RTSMouse::Load((GEScreen*)a[0]);
    case O::MoReleaseExtra: return RTSMouseFrameCall(MO_MOUSE, 5, NULL, NULL, false);
    case O::MoRtsRelease: return MO_MOUSE->RTSMouse::Release();
    default: return EECore10OpB5(inOp, p, a, f);
    }
    return 0;
}

double EECore10OpDF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEGameUtilities.cpp
double EERoViewsOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::GsClassScale: return gUnitClassTable[a[0]]->GetScale();
    case O::SnMapColoredOpacity: return RV_MAP->mColoredRectangles[a[0]].mOpacity;
    default: return EECore10OpDF(inOp, p, a, f);
    }
}
