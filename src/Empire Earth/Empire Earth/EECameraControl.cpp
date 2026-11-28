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
#include "lle/GEViewport.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEModel.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEGraphics.h"
#include "ee/EECameraKey.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EECombatGoals.h"
#include "rust/render/geval.h"
#include "rust/game/app.h"
#include "rust/game/gamecpp.h"

// the camera's Rust mirrors of the GE value classes (they share their storage: the static_assert below), its
// followed units' ids and its host (ee_app camera_ffi.rs; the context is the camera)
template <class R, class T> inline const R* EECameraRust(const T& inValue) { return reinterpret_cast<const R*>(&inValue); }
template <class R, class T> inline R* EECameraRust(T& inValue) { return reinterpret_cast<R*>(&inValue); }
static uint32_t EECameraID(EEUnit* inUnit) { return inUnit != NULL ? EEUnitHandleID(inUnit) : (uint32_t)EE_APP_CAMERA_NO_UNIT; }
#define EE_CAMERA_HOST this, this, ee_gamecpp_camera_host

// the GE value classes and their Rust mirrors share their storage (lle/GERust.h)
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
    case O::DkTileY: return ((EEUnit*)p)->GetTileY();
    case O::DkSetOrtho: gViewport.SetOrthographicRectangle(GERectangle(f[0], f[1], f[2], f[3]));
        break;
    case O::DkSetWorldCamera: gWorld.SetCameraTransformation(CAM->mView);   // a GETransformation (split.rs CPP_VALUES)
        break;
    case O::DkSetFieldOfView: ee_ge_viewport_set_field_of_view(&gViewport, f[0]);
        break;
    case O::DkClearB529066: gB529066 = false;
        break;
    case O::DkClearFollowing: gCameraFollowing = false;
        break;
    case O::DkClearB529068: gB529068 = false;
        break;
    // round 11, lane B: the camera's host (gamecpp camera_io.rs)
    case O::EcTileXRaw: return ((EEUnit*)p)->mTileX;
    case O::EcContainer: return P64(CX->mContainer);
    case O::EcContainerId: return EEUnitHandleID(CX->mContainer);
    case O::EcFacesCamera: return ((EEUnit*)p)->mUnitClass->mFacesCamera;
    case O::EcLocation: *(GETransformation*)a[0] = CX->mPhysicalModel.GetLocation();
        break;
    case O::EcIsSeen: return EEIsSeen((EEUnit*)p);
    case O::EcFarRearClipping: return gFarRearClipping;
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
    case O::EcSetFrontClipping: ee_ge_viewport_set_front(&gViewport, f[0]);
        break;
    case O::EcSetRearClipping: ee_ge_viewport_set_rear(&gViewport, f[0]);
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
    case O::DkModelHeight: return CX->mPhysicalModel.GetModel()->GetBoundingBox(0).GetHeight();
    case O::DkCameraFocus: return CX->mStats->GetCameraFocus();
    case O::EcVelocity: return CX->GetVelocity();
    case O::EcWorldCameraYaw: return gWorld.mCameraYaw;
    case O::EcViewportHalfHeight: return gViewport.GetHalfHeight();
    case O::EcViewportCameraX: return gViewport.GetCamera().GetX();
    case O::EcViewportCameraY: return gViewport.GetCamera().GetY();
    case O::EcViewportCameraZ: return gViewport.GetCamera().GetZ();
    default: return EECore11OpBF(inOp, p, a, f);
    }
}

// out of line from ee/EECameraControl.h (its Rust function headers stay out of it)
void EECameraControl::Update(float inElapsedTime) { ee_gamecpp_camera_update(this, inElapsedTime); }

// out of line from ee/EECameraControl.h (its Rust function headers stay out of it)

void EECameraControl::GetLocation(float inStep, GE3DPoint& outLocation)
{ ee_app_camera_point(this, 1, inStep, EECameraRust<EeGe3DPoint>(outLocation)); }

void EECameraControl::Revolve(const GE3DPoint& inAngles)
{ ee_app_camera_revolve(EE_CAMERA_HOST, inAngles.GetX(), inAngles.GetY()); }

void EECameraControl::Rotate(const GE3DPoint& inAngles)
{ ee_app_camera_rotate(EE_CAMERA_HOST, inAngles.GetX(), inAngles.GetY()); }

void EECameraControl::SetLocation(const GE3DPoint& inLocation, bool inIsImmediate, bool inFollowTerrain)
{ ee_app_camera_set_location(EE_CAMERA_HOST, EECameraRust<EeGe3DPoint>(inLocation), inIsImmediate, inFollowTerrain); }

void EECameraControl::SetMode(long inMode) { ee_app_camera_set_mode(EE_CAMERA_HOST, inMode); }
void EECameraControl::ClearUnitA() { ee_app_camera_clear_unit_a(EE_CAMERA_HOST); }
void EECameraControl::ClearUnitB() { ee_app_camera_clear_unit_b(EE_CAMERA_HOST); }
void EECameraControl::Scroll(float inDX, float inDY) { ee_app_camera_scroll(EE_CAMERA_HOST, inDX, inDY); }
void EECameraControl::SetZoom(float inZoom) { ee_app_camera_set_zoom(EE_CAMERA_HOST, inZoom); }
void EECameraControl::ResetPath() { ee_app_camera_reset_path(EE_CAMERA_HOST); }

void EECameraControl::GetOffset(float inStep, GE3DPoint& outPoint)
{ ee_app_camera_point(this, 0, inStep, EECameraRust<EeGe3DPoint>(outPoint)); }

void EECameraControl::GetViewTurn(float inStep, GETransformation& outTransformation)
{ ee_app_camera_turn(this, 0, inStep, EECameraRust<EeGeTransformation>(outTransformation)); }

void EECameraControl::GetOffsetTurn(float inStep, GETransformation& outTransformation)
{ ee_app_camera_turn(this, 1, inStep, EECameraRust<EeGeTransformation>(outTransformation)); }

void EECameraControl::SetOffset(const GE3DPoint& inPoint, bool inFlag)
{ ee_app_camera_set_offset(EE_CAMERA_HOST, EECameraRust<EeGe3DPoint>(inPoint), inFlag); }

void EECameraControl::SetViewTurn(const GETransformation& inTransformation, bool inFlag)
{ ee_app_camera_set_view_turn(EE_CAMERA_HOST, EECameraRust<EeGeTransformation>(inTransformation), inFlag); }

void EECameraControl::SetOffsetTurn(const GETransformation& inTransformation, bool inFlag)
{ ee_app_camera_set_offset_turn(EE_CAMERA_HOST, EECameraRust<EeGeTransformation>(inTransformation), inFlag); }

void EECameraControl::GetUnitTurn(EEUnit* inUnit, GETransformation& outTransformation)
{ ee_app_camera_unit_turn(EE_CAMERA_HOST, EECameraID(inUnit), EECameraRust<EeGeTransformation>(outTransformation)); }

void EECameraControl::StartPath() { ee_app_camera_start_path(EE_CAMERA_HOST); }
