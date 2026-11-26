// C:\EEX\Empire Earth\Empire Earth\EECameraControl.cpp (name from RTTI EECameraControl; no __FILE__).
// 0x127584..0x12a54a: gGameVersion's init chain (owner unit inferred), the seven file-static angle limits (0x1275b6..),
// the camera (ctor, ??_G, dtor, the key list, the height clamp, the clipping planes, the interpolation helpers, the
// follow/zoom/scroll movers, the per-frame update), the vector<EECameraKey> COMDATs and EEIsSeen. The unnamed
// 0x1273d7 unit ends at 0x127584, EECheatCodes.cpp starts at 0x12a54a. Names are provisional.
//
// The camera is whole in Rust (rust/crates/ee_game/src/app camera.rs, app and world lane round 1): its plain members are the
// record EeAppCameraFields the class derives from, every body is Rust over the host (the followed units' handles,
// the key list, the units, the terrain mesh, the viewport and the world's switches: ee_game gamecpp camera_io.rs,
// camera_host.rs). This unit keeps the forwarders, EEIsSeen and the host's one-statement cases.
#include <windows.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEViewport.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEModel.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEGraphics.h"
#include "ee/EECameraKey.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUnit.h"
#include "rust/game/gamecpp.h"   // round 10, lane D (ee_game gamecpp camera_host.rs)
#include "ee/EECombatGoals.h"

// the GE value classes and their Rust mirrors share their storage (GERustMath.h)
static_assert(sizeof(GE3DPoint) == sizeof(EeGe3DPoint) && sizeof(GETransformation) == sizeof(EeGeTransformation) &&
                  sizeof(GE3DLine) == sizeof(EeGe3DLine) && sizeof(GEQuaternion) == sizeof(EeGeQuaternion) &&
                  sizeof(EECameraKey) == sizeof(EeAppCameraKey) && offsetof(EECameraKey, mChanged) == offsetof(EeAppCameraKey, mChanged),
              "GE mirrors");

// Constructs the game version string "2002.8.17.v1.00" and registers its destructor to run at exit
UWideString gGameVersion("2002.8.17.v1.00");

extern bool& gFarRearClipping;               // 0x5308d2 (set: the far rear clipping distance; provisional name)

// The limits (0x441e48..0x441eb8), the pitch limits, the clipping distances and the move speeds are Rust's (ee_app
// camera.rs, ee_gfx camera.rs).


// Creates the camera controller with no followed unit or target point, mode 0, zoom 0.5 and no path
EECameraControl::EECameraControl()
{
    ee_gamecpp_camera_new(this, this);
}

// Destroys the camera controller (members only)
EECameraControl::~EECameraControl()
{
}

float EECameraControl::GetPitch(float inZoom, long inMode)
{
    return ee_gfx_camera_pitch(inZoom, (int32_t)inMode, EECameraRust<EeGeTransformation>(gWorld.mTransformation));
}

void EECameraControl::MoveTo(EEUnit* inUnitA, GE3DPoint inLocationA, bool inFlagA, float inDuration,
                               EEUnit* inUnitB, GE3DPoint inLocationB, bool inFlagB, float inZoom, float inTurnYaw,
                               float inTurnPitch)
{
    EeAppCameraMove theMove = {EECameraID(inUnitA), *EECameraRust<EeGe3DPoint>(inLocationA), inFlagA, inDuration,
                               EECameraID(inUnitB), *EECameraRust<EeGe3DPoint>(inLocationB), inFlagB, inZoom, inTurnYaw,
                               inTurnPitch};
    ee_app_camera_move_to(EE_CAMERA_HOST, &theMove);
}

// Returns whether the local player can see a unit
bool EEIsSeen(EEUnit* inUnit)
{
    return EECombatIsSeenBy(inUnit, gWorld.GetCurrentPlayer(), true);
}

// ------------------------------------------------------------------------- round 10, lane D: the camera

int64_t EECore11OpB(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEGameLog.cpp

#define P64(x) ((int64_t)(intptr_t)(x))
#define CAM ((EECameraControl*)p)
#define CX ((EEComplexUnit*)p)

// the C++ calls and members of the camera's bodies in Rust (gamecpp camera_host.rs): p the camera unless noted,
// one statement each; the end of lane D's cases
int64_t EECore10OpD7(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DkModelCenter: *(GE3DPoint*)a[0] = CX->mPhysicalModel.GetModel()->GetCenterPoint();
        break;
    case O::DkLocationApply: CX->mPhysicalModel.GetLocation().Apply(*(const GE3DPoint*)a[0], *(GE3DPoint*)a[1]);
        break;
    case O::DkTileX: return ((EEUnit*)p)->GetTileX();
    case O::DkTileY: return ((EEUnit*)p)->GetTileY();
    case O::DkSetOrtho: gViewport.SetOrthographicRectangle(GERectangle(f[0], f[1], f[2], f[3]));
        break;
    case O::DkSetWorldCamera: gWorld.SetCameraTransformation(*EECameraRust<GETransformation>(CAM->mView));
        break;
    case O::DkSetFieldOfView: gViewport.SetFieldOfView(f[0]);
        break;
    case O::DkMiniMapRefresh: gMiniMap->RefreshView();
        break;
    case O::DkClearB529066: gB529066 = false;
        break;
    case O::DkClearFollowing: gCameraFollowing = false;
        break;
    case O::DkClearB529068: gB529068 = false;
        break;
    // round 11, lane B: the camera's host (gamecpp camera_io.rs)
    case O::EcReference: ((EEUnit*)p)->Reference();
        break;
    case O::EcDereference: ((EEUnit*)p)->Dereference();
        break;
    case O::EcHitPoints: return ((EEUnit*)p)->mHitPoints;
    case O::EcTileXRaw: return ((EEUnit*)p)->mTileX;
    case O::EcContainer: return P64(CX->mContainer);
    case O::EcContainerId: return EEUnitHandleID(CX->mContainer);
    case O::EcFacesCamera: return ((EEUnit*)p)->mUnitClass->mFacesCamera;
    case O::EcLocation: *(GETransformation*)a[0] = CX->mPhysicalModel.GetLocation();
        break;
    case O::EcInCinematic: return gWorld.mInCinematic;
    case O::EcCurrentPlayer: return P64(gWorld.GetCurrentPlayer());
    case O::EcIsSeen: return EEIsSeen((EEUnit*)p);
    case O::EcFarRearClipping: return gFarRearClipping;
    case O::EcOrthographic: return gOrthographicView;
    case O::EcHasMap: return gWorld.mMap != NULL;
    case O::EcWorldTransformation: *(GETransformation*)a[0] = gWorld.mTransformation;
        break;
    case O::EcValidTile: return gWorld.GetMap()->mTerrainMesh.GetIsValidTile(UMFloorUnsigned(f[0]), UMFloorUnsigned(f[1]));
    case O::EcValidPoint: return gWorld.GetMap()->mTerrainMesh.GetIsValidPoint(UMFloorUnsigned(f[0]), UMFloorUnsigned(f[1]));
    case O::EcFrontPlane: *(EeGe3DPlane*)a[0] = *EECameraRust<EeGe3DPlane>(gWorld.GetMap()->mTerrainMesh.GetFrontPlane());
        break;
    case O::EcBackPlane: *(EeGe3DPlane*)a[0] = *EECameraRust<EeGe3DPlane>(gWorld.GetMap()->mTerrainMesh.GetBackPlane());
        break;
    case O::EcLeftPlane: *(EeGe3DPlane*)a[0] = *EECameraRust<EeGe3DPlane>(gWorld.GetMap()->mTerrainMesh.GetLeftPlane());
        break;
    case O::EcRightPlane: *(EeGe3DPlane*)a[0] = *EECameraRust<EeGe3DPlane>(gWorld.GetMap()->mTerrainMesh.GetRightPlane());
        break;
    case O::EcScreenLine: gWorld.GetMap()->GetScreenLine(GE2DPoint(f[0], f[1]), *(GE3DLine*)a[0]);
        break;
    case O::EcLocationOnLine: return gWorld.mMap->GetLocationOnLine(*(GE3DLine*)a[0], *(GE3DPoint*)a[1], true) != NULL;
    case O::EcSetFrontClipping: gViewport.SetFrontClippingDistance(f[0]);
        break;
    case O::EcSetRearClipping: gViewport.SetRearClippingDistance(f[0]);
        break;
    default: return EECore11OpB(inOp, p, a, f);
    }
    return 0;
}

double EECore11OpBF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEGameLog.cpp
double EECore10OpD7F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DkModelScale: return CX->mPhysicalModel.GetScale();
    case O::DkModelHeight: return CX->mPhysicalModel.GetModel()->GetBoundingBox(0).GetHeight();
    case O::DkCameraFocus: return CX->mStats->GetCameraFocus();
    case O::DkFrontClipping: return gViewport.GetFrontClippingDistance();
    case O::DkRearClipping: return gViewport.GetRearClippingDistance();
    case O::EcVelocity: return CX->GetVelocity();
    case O::EcGameSpeed: return gWorld.GetGameSpeed();
    case O::EcWorldCameraYaw: return gWorld.mCameraYaw;
    case O::EcViewportXCenter: return gViewport.GetXCenter();
    case O::EcViewportYCenter: return gViewport.GetYCenter();
    case O::EcViewportHalfHeight: return gViewport.GetHalfHeight();
    case O::EcViewportCameraX: return gViewport.GetCamera().GetX();
    case O::EcViewportCameraY: return gViewport.GetCamera().GetY();
    case O::EcViewportCameraZ: return gViewport.GetCamera().GetZ();
    default: return EECore11OpBF(inOp, p, a, f);
    }
}
