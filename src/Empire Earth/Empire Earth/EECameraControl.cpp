// C:\EEX\Empire Earth\Empire Earth\EECameraControl.cpp (name from RTTI EECameraControl; no __FILE__).
// 0x127584..0x12a54a: gGameVersion's init chain (owner unit inferred), the seven file-static angle limits (0x1275b6..),
// the camera (ctor, ??_G, dtor, the key list, the height clamp, the clipping planes, the interpolation helpers, the
// follow/zoom/scroll movers, the per-frame update), the vector<EECameraKey> COMDATs and EEIsSeen. The unnamed
// 0x1273d7 unit ends at 0x127584, EECheatCodes.cpp starts at 0x12a54a. Names are provisional.
//
// The camera is whole in Rust (rust/crates/ee_game/src/app camera.rs, app and world lane round 1): its plain members are the
// record EeAppCameraFields the class derives from, every body is Rust over the host below (the followed units' handles,
// the key list, the units, the terrain mesh, the viewport and the world's switches). This unit keeps the host, the
// forwarders, Update's tail (the view to the world and the viewport, the mini map) and EEIsSeen.
#include <windows.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
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
#include "ee/EEAreaUtilities.h"
#include "rust/base/math.h"
#include "rust/game/app.h"
#include "rust/render/gfx.h"
#include "rust/game/gamecpp.h"   // round 10, lane D (ee_game gamecpp camera_host.rs)
#include "ee/EECombatGoals.h"

// the GE value classes and their Rust mirrors share their storage (GERustMath.h)
static_assert(sizeof(GE3DPoint) == sizeof(EeGe3DPoint) && sizeof(GETransformation) == sizeof(EeGeTransformation) &&
                  sizeof(GE3DLine) == sizeof(EeGe3DLine) && sizeof(GEQuaternion) == sizeof(EeGeQuaternion) &&
                  sizeof(EECameraKey) == sizeof(EeAppCameraKey) && offsetof(EECameraKey, mChanged) == offsetof(EeAppCameraKey, mChanged),
              "GE mirrors");
template <class R, class T> static inline const R* EECameraRust(const T& inValue) { return reinterpret_cast<const R*>(&inValue); }
template <class R, class T> static inline R* EECameraRust(T& inValue) { return reinterpret_cast<R*>(&inValue); }

// Constructs the game version string "2002.8.17.v1.00" and registers its destructor to run at exit
UWideString gGameVersion("2002.8.17.v1.00");

extern bool& gFarRearClipping;               // 0x5308d2 (set: the far rear clipping distance; provisional name)
extern bool& gInCinematic;                   // 0x5318e0 (set: scrolling keeps the free camera mode; provisional name)

// port: the farthest zoom (1.0 in the original; the camera's distance grows linearly past it, its pitch stays at the
// farthest), from config.yaml's camera: maximum_zoom_out (EEPreferences)
float gCameraMaxZoom = 2.0f;

// The limits (0x441e48..0x441eb8), the pitch limits, the clipping distances and the move speeds are Rust's (ee_app
// camera.rs, ee_gfx camera.rs).
// Initialises the camera's field of view (30 degrees)
static float sCameraFieldOfView = 0.017453292f * 30.0f;

// the unit of an id (NULL for none or a unit that no longer resolves)
static EEUnit* EECameraUnit(uint32_t inID)
{
    EEUnit* theUnit;
    EEUnitHandleResolve(inID, theUnit);
    return theUnit;
}

// The camera's host (ee_app camera_ffi.rs): the context is the camera
static int32_t EECameraHost(void* inContext, int32_t inOp, EeAppCameraIo* io)
{
    EECameraControl* theCamera = static_cast<EECameraControl*>(inContext);
    EEUnitHandle<EEUnit>& theSlot = io->slot == EE_APP_CAMERA_UNIT_A ? theCamera->mUnitA : theCamera->mUnitB;
    EEUnit* theUnit = inOp >= EE_APP_CAMERA_OP_REFERENCE && inOp <= EE_APP_CAMERA_OP_HIDDEN ? EECameraUnit(io->id) : NULL;
    EEComplexUnit* theComplex = theUnit != NULL ? theUnit->mComplexUnit : NULL;
    GETerrainMesh* theMesh = gWorld.GetMap() != NULL ? &gWorld.GetMap()->mTerrainMesh : NULL;
    switch (inOp) {
    case EE_APP_CAMERA_OP_UNIT: io->id = theSlot.Get() != NULL ? theSlot.GetSavedID() : (uint32_t)EE_APP_CAMERA_NO_UNIT; break;
    case EE_APP_CAMERA_OP_SET_UNIT: theSlot = EEUnitHandle<EEUnit>::FromSavedID(io->id); break;
    case EE_APP_CAMERA_OP_REFERENCE: theUnit->Reference(); break;
    case EE_APP_CAMERA_OP_DEREFERENCE: theUnit->Dereference(); break;
    case EE_APP_CAMERA_OP_UNIT_STATE:
        io->id = theUnit == NULL ? 0
                                 : EE_APP_CAMERA_EXISTS | (theUnit->mHitPoints > 0 ? EE_APP_CAMERA_ALIVE : 0) |
                                       (theUnit->mTileX != -10000 && theUnit->GetTileY() != -10000 ? EE_APP_CAMERA_ON_MAP : 0);
        break;
    case EE_APP_CAMERA_OP_CONTAINER:
        if (theComplex != NULL && theComplex->mContainer != NULL)
            io->id = EEUnitHandleID(theComplex->mContainer);
        break;
    case EE_APP_CAMERA_OP_UNIT_POINT: theCamera->GetUnitPoint(theUnit, *EECameraRust<GE3DPoint>(io->point)); break;
    case EE_APP_CAMERA_OP_UNIT_LOCATION:
        io->flag = theComplex != NULL && !theUnit->mUnitClass->mFacesCamera;
        if (io->flag)
            *EECameraRust<GETransformation>(io->t) = theComplex->mPhysicalModel.GetLocation();
        break;
    case EE_APP_CAMERA_OP_FOLLOW_RATE:
        io->flag = theComplex != NULL && !gInCinematic;
        if (io->flag)
            io->x = theComplex->GetVelocity() * gWorld.GetGameSpeed();
        break;
    case EE_APP_CAMERA_OP_HIDDEN: io->flag = theUnit != NULL && theUnit->mPlayer != gWorld.GetCurrentPlayer() && !EEIsSeen(theUnit); break;
    case EE_APP_CAMERA_OP_KEY_COUNT: io->id = (uint32_t)theCamera->mKeys.size(); break;
    case EE_APP_CAMERA_OP_KEY: *EECameraRust<EECameraKey>(io->key) = theCamera->mKeys[io->id]; break;
    case EE_APP_CAMERA_OP_CLEAR_KEYS: theCamera->mKeys.clear(); break;
    case EE_APP_CAMERA_OP_FLAGS:
        io->id = (gFreeCamera ? EE_APP_CAMERA_FREE : 0) | (gFarRearClipping ? EE_APP_CAMERA_FAR_REAR : 0) |
                 (gInCinematic ? EE_APP_CAMERA_CINEMATIC : 0) | (gOrthographicView ? EE_APP_CAMERA_ORTHOGRAPHIC : 0) |
                 (gWorld.mMap != NULL ? EE_APP_CAMERA_HAS_MAP : 0);
        break;
    case EE_APP_CAMERA_OP_WORLD_TRANSFORMATION: *EECameraRust<GETransformation>(io->t) = gWorld.mTransformation; break;
    case EE_APP_CAMERA_OP_WORLD_CAMERA_YAW: io->x = gWorld.mCameraYaw; break;
    case EE_APP_CAMERA_OP_VALID_TILE: io->flag = theMesh->GetIsValidTile(UMFloorUnsigned(io->x), UMFloorUnsigned(io->y)); break;
    case EE_APP_CAMERA_OP_VALID_POINT: io->flag = theMesh->GetIsValidPoint(UMFloorUnsigned(io->x), UMFloorUnsigned(io->y)); break;
    case EE_APP_CAMERA_OP_MESH_Z: io->z = theMesh->GetMeshZ(io->x, io->y); break;
    case EE_APP_CAMERA_OP_EDGE_PLANES:
        io->planes.front = *EECameraRust<EeGe3DPlane>(theMesh->GetFrontPlane());
        io->planes.back = *EECameraRust<EeGe3DPlane>(theMesh->GetBackPlane());
        io->planes.left = *EECameraRust<EeGe3DPlane>(theMesh->GetLeftPlane());
        io->planes.right = *EECameraRust<EeGe3DPlane>(theMesh->GetRightPlane());
        break;
    case EE_APP_CAMERA_OP_SCREEN_LINE: gWorld.GetMap()->GetScreenLine(GE2DPoint(io->x, io->y), *EECameraRust<GE3DLine>(io->line)); break;
    case EE_APP_CAMERA_OP_LOCATION_ON_LINE:
        io->flag = gWorld.mMap->GetLocationOnLine(*EECameraRust<GE3DLine>(io->line), *EECameraRust<GE3DPoint>(io->point), true) != NULL;
        break;
    case EE_APP_CAMERA_OP_VIEWPORT:
        io->x = gViewport.GetXCenter();
        io->y = gViewport.GetYCenter();
        io->z = gViewport.GetHalfHeight();
        break;
    case EE_APP_CAMERA_OP_VIEWPORT_CAMERA:
        io->point.x = gViewport.GetCamera().GetX();
        io->point.y = gViewport.GetCamera().GetY();
        io->point.z = gViewport.GetCamera().GetZ();
        break;
    case EE_APP_CAMERA_OP_FRONT_CLIPPING: io->x = gViewport.GetFrontClippingDistance(); break;
    case EE_APP_CAMERA_OP_SET_CLIPPING:
        gViewport.SetFrontClippingDistance(io->x);
        gViewport.SetRearClippingDistance(io->y);
        break;
    }
    return 0;
}
static uint32_t EECameraID(EEUnit* inUnit)
{
    return inUnit != NULL ? EEUnitHandleID(inUnit) : (uint32_t)EE_APP_CAMERA_NO_UNIT;
}
#define EE_CAMERA_HOST this, this, EECameraHost

// Creates the camera controller with no followed unit or target point, mode 0, zoom 0.5 and no path
EECameraControl::EECameraControl()
{
    ee_app_camera_default(this);
}

// Destroys the camera controller (members only)
EECameraControl::~EECameraControl()
{
}

void EECameraControl::ResetPath() { ee_app_camera_reset_path(EE_CAMERA_HOST); }

// Appends a key to the camera path; returns 0, or 5 while a path plays (camera locked)
long EECameraControl::AddKey(const EECameraKey& inKey)
{
    if (this->mLocked)
        return 5;
    this->mKeys.push_back(inKey);
    return 0;
}

// (the original's EECameraRayDistance 0x527c09 built each ray twice; the Rust builds it once and measures it twice)
void EECameraControl::SetClippingDistances() { ee_app_camera_set_clipping_distances(EE_CAMERA_HOST); }

void EECameraControl::GetOffset(float inStep, GE3DPoint& outPoint) { ee_app_camera_point(this, 0, inStep, EECameraRust<EeGe3DPoint>(outPoint)); }

void EECameraControl::GetViewTurn(float inStep, GETransformation& outTransformation)
{
    ee_app_camera_turn(this, 0, inStep, EECameraRust<EeGeTransformation>(outTransformation));
}

void EECameraControl::GetOffsetTurn(float inStep, GETransformation& outTransformation)
{
    ee_app_camera_turn(this, 1, inStep, EECameraRust<EeGeTransformation>(outTransformation));
}

void EECameraControl::GetLocation(float inStep, GE3DPoint& outLocation)
{
    ee_app_camera_point(this, 1, inStep, EECameraRust<EeGe3DPoint>(outLocation));
}

void EECameraControl::GetGroundPoint(const GE3DLine& inLine, GE3DPoint& outPoint)
{
    ee_app_camera_ground_point(EE_CAMERA_HOST, EECameraRust<EeGe3DLine>(inLine), EECameraRust<EeGe3DPoint>(outPoint));
}

// Returns the point the camera focuses on for a unit: its model centre at camera-focus height, else its tile centre
// (the host's EE_APP_CAMERA_OP_UNIT_POINT; ee_game gamecpp camera_host.rs)
void EECameraControl::GetUnitPoint(EEUnit* inUnit, GE3DPoint& outPoint)
{
    ee_gamecpp_camera_unit_point(inUnit, &outPoint);
}

void EECameraControl::GetUnitTurn(EEUnit* inUnit, GETransformation& outTransformation)
{
    ee_app_camera_unit_turn(EE_CAMERA_HOST, EECameraID(inUnit), EECameraRust<EeGeTransformation>(outTransformation));
}

float EECameraControl::GetPitch(float inZoom, long inMode)
{
    return ee_gfx_camera_pitch(inZoom, (int32_t)inMode, EECameraRust<EeGeTransformation>(gWorld.mTransformation));
}

void EECameraControl::StartPath() { ee_app_camera_start_path(EE_CAMERA_HOST); }

void EECameraControl::Revolve(const GE3DPoint& inAngles) { ee_app_camera_revolve(EE_CAMERA_HOST, inAngles.GetX(), inAngles.GetY()); }

// Creates a camera key (members only)
EECameraKey::EECameraKey()
{
}

void EECameraControl::Rotate(const GE3DPoint& inAngles) { ee_app_camera_rotate(EE_CAMERA_HOST, inAngles.GetX(), inAngles.GetY()); }

void EECameraControl::MoveTo(EEUnit* inUnitA, GE3DPoint inLocationA, bool inFlagA, float inDuration,
                               EEUnit* inUnitB, GE3DPoint inLocationB, bool inFlagB, float inZoom, float inTurnYaw,
                               float inTurnPitch)
{
    EeAppCameraMove theMove = {EECameraID(inUnitA), *EECameraRust<EeGe3DPoint>(inLocationA), inFlagA, inDuration,
                               EECameraID(inUnitB), *EECameraRust<EeGe3DPoint>(inLocationB), inFlagB, inZoom, inTurnYaw,
                               inTurnPitch};
    ee_app_camera_move_to(EE_CAMERA_HOST, &theMove);
}

void EECameraControl::Scroll(float inDX, float inDY) { ee_app_camera_scroll(EE_CAMERA_HOST, inDX, inDY); }

void EECameraControl::ResetZoom() { this->SetZoom(1.0f); }

void EECameraControl::SetOffset(const GE3DPoint& inPoint, bool inFlag)
{
    ee_app_camera_set_offset(EE_CAMERA_HOST, EECameraRust<EeGe3DPoint>(inPoint), inFlag);
}

void EECameraControl::SetViewTurn(const GETransformation& inTransformation, bool inFlag)
{
    ee_app_camera_set_view_turn(EE_CAMERA_HOST, EECameraRust<EeGeTransformation>(inTransformation), inFlag);
}

void EECameraControl::SetOffsetTurn(const GETransformation& inTransformation, bool inFlag)
{
    ee_app_camera_set_offset_turn(EE_CAMERA_HOST, EECameraRust<EeGeTransformation>(inTransformation), inFlag);
}

void EECameraControl::SetLocation(const GE3DPoint& inLocation, bool inIsImmediate, bool inFollowTerrain)
{
    ee_app_camera_set_location(EE_CAMERA_HOST, EECameraRust<EeGe3DPoint>(inLocation), inIsImmediate, inFollowTerrain);
}

void EECameraControl::SetMode(long inMode) { ee_app_camera_set_mode(EE_CAMERA_HOST, inMode); }

void EECameraControl::SetChanged() { ee_app_camera_set_changed(this); }

void EECameraControl::ClearUnitA() { ee_app_camera_clear_unit_a(EE_CAMERA_HOST); }

void EECameraControl::ClearUnitB() { ee_app_camera_clear_unit_b(EE_CAMERA_HOST); }

// Advances the camera each frame (ee_app camera.rs update), then applies the view, the orthographic size, the
// clipping distances and the field of view to the viewport and refreshes the mini map's view; CAMU at its start and
// end (TEST ONLY) (ee_game gamecpp camera_host.rs)
void EECameraControl::Update(float inElapsedTime)
{
    ee_gamecpp_camera_update(this, EECameraHost, inElapsedTime);
}

void EECameraControl::Follow(EEUnit* inUnit, bool inFlag, float inDuration, float inTurnYaw, float inTurnPitch)
{
    ee_app_camera_follow(EE_CAMERA_HOST, EECameraID(inUnit), inFlag, inDuration, inTurnYaw, inTurnPitch);
}

void EECameraControl::SetZoom(float inZoom) { ee_app_camera_set_zoom(EE_CAMERA_HOST, inZoom); }

// Returns whether the local player can see a unit
bool EEIsSeen(EEUnit* inUnit)
{
    return EECombatIsSeenBy(inUnit, gWorld.GetCurrentPlayer(), true);
}

// ------------------------------------------------------------------------- round 10, lane D: the camera

int64_t EECore10OpE(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore10OpEF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))
#define CAM ((EECameraControl*)p)
#define CX ((EEComplexUnit*)p)

// the C++ calls and members of the camera's bodies in Rust (gamecpp camera_host.rs): p the camera unless noted,
// one statement each; the end of lane D's cases
int64_t EECore10OpD7(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DkFields: return P64(static_cast<EeAppCameraFields*>(CAM));
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
    case O::DkSetFieldOfView: gViewport.SetFieldOfView(sCameraFieldOfView);
        break;
    case O::DkHaveMiniMap: return gMiniMap != NULL;
    case O::DkMiniMapRefresh: gMiniMap->RefreshView();
        break;
    case O::DkClearB529066: gB529066 = false;
        break;
    case O::DkClearFollowing: gCameraFollowing = false;
        break;
    case O::DkClearB529068: gB529068 = false;
        break;
    case O::DkUnitASaved: return CAM->mUnitA.GetSavedID();
    case O::DkUnitBSaved: return CAM->mUnitB.GetSavedID();
    case O::DkKeyCount: return (int64_t)CAM->mKeys.size();
    case O::DkKeyAt: *(EECameraKey*)a[1] = CAM->mKeys[a[0]];
        break;
    default: return EECore10OpE(inOp, p, a, f);
    }
    return 0;
}

double EECore10OpD7F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DkModelScale: return CX->mPhysicalModel.GetScale();
    case O::DkModelHeight: return CX->mPhysicalModel.GetModel()->GetBoundingBox(0).GetHeight();
    case O::DkCameraFocus: return CX->mStats->GetCameraFocus();
    case O::DkFrontClipping: return gViewport.GetFrontClippingDistance();
    case O::DkRearClipping: return gViewport.GetRearClippingDistance();
    default: return EECore10OpEF(inOp, p, a, f);
    }
}
