// C:\EEX\Empire Earth\Empire Earth\EECameraControl.cpp (name from RTTI EECameraControl; no __FILE__).
// 0x127584..0x12a54a: gGameVersion's init chain (owner unit inferred), the seven file-static angle limits (0x1275b6..),
// the camera (ctor, ??_G, dtor, the key list, the height clamp, the clipping planes, the interpolation helpers, the
// follow/zoom/scroll movers, the per-frame update), the vector<EECameraKey> COMDATs and EEIsSeen. The unnamed
// 0x1273d7 unit ends at 0x127584, EECheatCodes.cpp starts at 0x12a54a. Names are provisional.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <math.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GE3DLine.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GEQuaternion.h"
#include "lle/GE3DPlane.h"
#include "lle/GEModel.h"
#include "lle/GEBoundingBox.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/RTSWorld.h"
#include "ee/EEGraphics.h"
#include "ee/EECameraKey.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUnit.h"
#include "ee/EEAreaUtilities.h"
#include "rust/ee_ge.h"
#include "rust/ee_gfx.h"

// The camera's math is Rust (rust/crates/ee_gfx camera.rs, docs/RUST_PLAN.md R4.4): the pitch, the interpolation, the
// move's speed, the view, the height clamp, the clipping distances, the edge clip, the scroll direction and clamp,
// the update's step and the orthographic view. This class keeps its members, key list, followed units and the
// viewport and world calls; the GE value classes pass as their #[repr(C)] mirrors (the same storage, GERustMath.h).
template <class R, class T> static inline const R* EECameraRust(const T& inValue) { return reinterpret_cast<const R*>(&inValue); }
template <class R, class T> static inline R* EECameraRust(T& inValue) { return reinterpret_cast<R*>(&inValue); }
#define EE_CAM_P(x) EECameraRust<EeGe3DPoint>(x)
#define EE_CAM_T(x) EECameraRust<EeGeTransformation>(x)
#define EE_CAM_L(x) EECameraRust<EeGe3DLine>(x)
static_assert(sizeof(GE3DPoint) == sizeof(EeGe3DPoint) && sizeof(GETransformation) == sizeof(EeGeTransformation) &&
              sizeof(GE3DLine) == sizeof(EeGe3DLine) && sizeof(GE3DPlane) == sizeof(EeGe3DPlane) &&
              sizeof(GEQuaternion) == sizeof(EeGeQuaternion) && sizeof(GEVector) == sizeof(EeGeVector), "GE mirrors");

// the terrain mesh's queries for the Rust (the context is the mesh)
static bool EECameraValidTile(void* inMesh, float inX, float inY)
{
    return static_cast<GETerrainMesh*>(inMesh)->GetIsValidTile(UMFloorUnsigned(inX), UMFloorUnsigned(inY));
}
static bool EECameraValidPoint(void* inMesh, float inX, float inY)
{
    return static_cast<GETerrainMesh*>(inMesh)->GetIsValidPoint(UMFloorUnsigned(inX), UMFloorUnsigned(inY));
}
static float EECameraMeshZ(void* inMesh, float inX, float inY)
{
    return static_cast<GETerrainMesh*>(inMesh)->GetMeshZ(inX, inY);
}
static EeGfxGround EECameraGround(GETerrainMesh& inMesh)
{
    EeGfxGround theGround;
    theGround.context = &inMesh;
    theGround.valid_tile = EECameraValidTile;
    theGround.valid_point = EECameraValidPoint;
    theGround.mesh_z = EECameraMeshZ;
    return theGround;
}
static EeGfxEdgePlanes EECameraEdgePlanes(const GETerrainMesh& inMesh)
{
    EeGfxEdgePlanes thePlanes;
    thePlanes.front = *EECameraRust<EeGe3DPlane>(inMesh.GetFrontPlane());
    thePlanes.back = *EECameraRust<EeGe3DPlane>(inMesh.GetBackPlane());
    thePlanes.left = *EECameraRust<EeGe3DPlane>(inMesh.GetLeftPlane());
    thePlanes.right = *EECameraRust<EeGe3DPlane>(inMesh.GetRightPlane());
    return thePlanes;
}
// whether the height clamp keeps the camera at least 0.4 high (no followed unit, no free camera, no path playing)
#define EE_CAM_PLAIN (this->mUnitA == NULL && this->mUnitB == NULL && !gFreeCamera && !this->mLocked)

// FUNCTION: EXE 0x527584
// FUNCTION: EXE 0x52758e
// FUNCTION: EXE 0x52759f
// FUNCTION: EXE 0x5275ab
// Constructs the game version string "2002.8.17.v1.00" and registers its destructor to run at exit
UWideString gGameVersion("2002.8.17.v1.00");

extern bool& gFarRearClipping;               // 0x5308d2 (set: the far rear clipping distance; provisional name)
extern bool& gInCinematic;                   // 0x5318e0 (set: scrolling keeps the free camera mode; provisional name)

// the camera's limits (unit-local .rdata 0x441e48..0x441eb8, before the vtable; provisional names)
static const float kCameraMinimumHeightFree = 0.1f;
static const float kCameraMinimumHeight = 0.4f;
static const float kCameraDegreesToRadians = 0.017453292f;
static const float kCameraDefaultYaw = 0.7853982f;
static const float kCameraZoomHeight = 0.1f;
static const float kCameraZoomHeightFree = 0.0f;
static const float kCameraZoomDistance = -20.5f;
static const float kCameraZoomDistanceFree = -42.0f;

// port: the farthest zoom (1.0 in the original; the camera's distance grows linearly past it, its pitch stays at the
// farthest), from config.yaml's camera: maximum_zoom_out (EEPreferences)
float gCameraMaxZoom = 2.0f;

// The pitch limits (0x5275ce..0x527650: -5 and -35 degrees, the free camera's 0 and -35), the rear clipping tilt (25
// degrees, 0x527624) and range (0x52763c), the move speeds, the orthographic view's constants and the clipping
// distances are rust/crates/ee_gfx camera.rs's.
// FUNCTION: EXE 0x5275b6
// FUNCTION: EXE 0x5275bb
// Initialises the camera's field of view (30 degrees)
static float sCameraFieldOfView = kCameraDegreesToRadians * 30.0f;

// FUNCTION: EXE 0x527654
// Creates the camera controller with no followed unit or target point, mode 0, zoom 0.5 and no path
EECameraControl::EECameraControl()
{
    this->mUnitA = NULL;
    this->mUnitB = NULL;
    this->mTargetPoint = GE3DPoint(-10000.0f, 0.0f, 0.0f);
    this->mPathIndex = -1;
    this->mMode = 0;
    this->mFraction = 0.0f;
    this->mChanged = false;
    this->mIsFollowingUnitB = false;
    this->mRate = 0.0f;
    this->mLocked = false;
    this->mZoom = 0.5f;
    this->mWaited = 0.0f;
    this->mTurnPitch = 0.0f;
    this->mTurnYaw = 0.0f;
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x52779f

// FUNCTION: EXE 0x5277bb
// Destroys the camera controller (members only)
EECameraControl::~EECameraControl()
{
}

// FUNCTION: EXE 0x5277c9
// Drops the camera path's keys and unlocks the camera
void EECameraControl::ResetPath()
{
    this->mKeys.clear();
    this->mLocked = false;
    this->mWaited = 0.0f;
}

// FUNCTION: EXE 0x5277e5
// Appends a key to the camera path; returns 0, or 5 while a path plays (camera locked)
long EECameraControl::AddKey(const EECameraKey& inKey)
{
    if (!this->mLocked) {
        this->mKeys.push_back(inKey);
        return 0;
    }
    return 5;
}

// FUNCTION: EXE 0x527804
// Keeps a camera transformation above the terrain at its position and one front clipping distance ahead of it
void EECameraControl::ClampHeight(GETransformation& ioTransformation)
{
    GETerrainMesh& theMesh = gWorld.GetMap()->mTerrainMesh;
    ee_gfx_camera_clamp_height(EE_CAM_T(ioTransformation), this->mMode, gViewport.GetFrontClippingDistance(), EE_CAM_PLAIN,
                               EECameraGround(theMesh));
}

// FUNCTION: EXE 0x5279f0
// FUNCTION: EXE 0x527c09
// Sets the viewport's front and rear clipping distances from the view rays through the screen's top, centre and bottom
// (fixed ones in the orthographic view). The original's EECameraRayDistance (0x527c09) built each ray twice, once for
// the rear and once for the front limit; EEMap::GetScreenLine reads only the viewport, which changes after all six,
// so each ray is built once here and the Rust measures it twice.
void EECameraControl::SetClippingDistances()
{
    EeGfxCameraRays theRays;
    GE3DPoint theCamera;
    bool thePerspective = !gOrthographicView;
    if (thePerspective) {
        GE2DPoint theTop(gViewport.GetXCenter(), gViewport.GetYCenter() + gViewport.GetHalfHeight());
        GE2DPoint theCenter(gViewport.GetXCenter(), gViewport.GetYCenter());
        GE2DPoint theBottom(gViewport.GetXCenter(), gViewport.GetYCenter() - gViewport.GetHalfHeight());
        gWorld.GetMap()->GetScreenLine(theCenter, *reinterpret_cast<GE3DLine*>(&theRays.center));
        gWorld.GetMap()->GetScreenLine(theTop, *reinterpret_cast<GE3DLine*>(&theRays.top));
        gWorld.GetMap()->GetScreenLine(theBottom, *reinterpret_cast<GE3DLine*>(&theRays.bottom));
        theCamera.SetX(gViewport.GetCamera().GetX());
        theCamera.SetY(gViewport.GetCamera().GetY());
        theCamera.SetZ(gViewport.GetCamera().GetZ());
    }
    EeGfxClipping theClipping = ee_gfx_camera_clipping(thePerspective ? &theRays : NULL, EE_CAM_P(theCamera),
                                                       (int32_t)this->mMode, this->mZoom, gFreeCamera, gFarRearClipping);
    gViewport.SetFrontClippingDistance(theClipping.front);
    gViewport.SetRearClippingDistance(theClipping.rear);
}

// FUNCTION: EXE 0x527db3
// Copies one camera key into another
void EECameraControl::CopyKey(const EECameraKey& inKey, EECameraKey& outKey)
{
    outKey = inKey;
}

// Compiler-generated: EECameraKey's implicit operator= (two GETransformation imports, then the members):
// TEMPLATE: EXE 0x527dc3

// FUNCTION: EXE 0x527e5b
// Moves to the camera path's next key and locks the camera, or ends the path after the last key
void EECameraControl::NextKey()
{
    this->mPathIndex++;
    if (this->mPathIndex < (long)this->mKeys.size()) {
        this->GoTo(this->mKeys[this->mPathIndex]);
        this->mLocked = true;
        return;
    }
    this->ResetPath();
}

// FUNCTION: EXE 0x527eb1
// Builds the camera view at a point of the current move from the interpolated offset, turns and focus, kept above
// ground
void EECameraControl::BuildView(GETransformation& outTransformation, float inStep)
{
    EeGfxCameraPose thePose;
    thePose.view_start = *EECameraRust<EeGeQuaternion>(this->mQ014);
    thePose.view_end = *EECameraRust<EeGeQuaternion>(this->mQ04C);
    thePose.offset_start = *EECameraRust<EeGeQuaternion>(this->mQ024);
    thePose.offset_end = *EECameraRust<EeGeQuaternion>(this->mQ05C);
    thePose.start_offset = *EE_CAM_P(this->mStartOffset);
    thePose.end_offset = *EE_CAM_P(this->mEndOffset);
    thePose.start_focus = *EE_CAM_P(this->mStartFocus);
    thePose.end_focus = *EE_CAM_P(this->mEndFocus);
    GETerrainMesh& theMesh = gWorld.GetMap()->mTerrainMesh;
    ee_gfx_camera_build_view(&thePose, inStep, (int32_t)this->mMode, gViewport.GetFrontClippingDistance(), EE_CAM_PLAIN,
                             EECameraGround(theMesh), EE_CAM_T(outTransformation));
}

// FUNCTION: EXE 0x52802b
// Returns the camera offset interpolated between the move's start and end
void EECameraControl::GetOffset(float inStep, GE3DPoint& outPoint)
{
    ee_gfx_camera_lerp(EE_CAM_P(this->mStartOffset), EE_CAM_P(this->mEndOffset), inStep, EE_CAM_P(outPoint));
}

// FUNCTION: EXE 0x52806d
// Returns the view turn interpolated between the move's start and end (quaternion slerp)
void EECameraControl::SlerpViewTurn(float inStep, GEQuaternion& outQuaternion)
{
    this->mQ014.Slerp(this->mQ04C, inStep, outQuaternion);
}

// FUNCTION: EXE 0x528089
// Returns the interpolated view turn as a transformation
void EECameraControl::GetViewTurn(float inStep, GETransformation& outTransformation)
{
    GEQuaternion theQuaternion;
    this->mQ014.Slerp(this->mQ04C, inStep, theQuaternion);
    theQuaternion.ConvertTo(outTransformation);
}

// FUNCTION: EXE 0x5280cb
// Returns the offset turn interpolated between the move's start and end (quaternion slerp)
void EECameraControl::SlerpOffsetTurn(float inStep, GEQuaternion& outQuaternion)
{
    this->mQ024.Slerp(this->mQ05C, inStep, outQuaternion);
}

// FUNCTION: EXE 0x5280e7
// Returns the interpolated offset turn as a transformation
void EECameraControl::GetOffsetTurn(float inStep, GETransformation& outTransformation)
{
    GEQuaternion theQuaternion;
    this->mQ024.Slerp(this->mQ05C, inStep, theQuaternion);
    theQuaternion.ConvertTo(outTransformation);
}

// FUNCTION: EXE 0x528129
// Returns the camera focus interpolated between the move's start and end
void EECameraControl::GetLocation(float inStep, GE3DPoint& outLocation)
{
    ee_gfx_camera_lerp(EE_CAM_P(this->mStartFocus), EE_CAM_P(this->mEndFocus), inStep, EE_CAM_P(outLocation));
}

// FUNCTION: EXE 0x52816e
// Returns a scroll line's end clipped to the map edges, at terrain height when the camera is close or the point is
// below ground, else at the minimum height
void EECameraControl::GetGroundPoint(const GE3DLine& inLine, GE3DPoint& outPoint)
{
    GE3DPoint theStart;
    GETerrainMesh& theMesh = gWorld.GetMap()->mTerrainMesh;
    EeGfxEdgePlanes thePlanes = EECameraEdgePlanes(theMesh);
    this->GetOffset(this->mFraction, theStart);
    ee_gfx_camera_ground_point(EE_CAM_L(inLine), &thePlanes, EE_CAM_P(theStart), (int32_t)this->mMode,
                               EECameraGround(theMesh), EE_CAM_P(outPoint));
}

// FUNCTION: EXE 0x5282c3
// Moves a point back to where a line first crosses one of the map's four edge planes, nudged just inside
void EECameraControl::ClipToEdges(const GE3DLine& inLine, GE3DPoint& ioPoint)
{
    EeGfxEdgePlanes thePlanes = EECameraEdgePlanes(gWorld.GetMap()->mTerrainMesh);
    ee_gfx_camera_clip_to_edges(EE_CAM_L(inLine), &thePlanes, EE_CAM_P(ioPoint));
}

// FUNCTION: EXE 0x52844f
// Returns the point the camera focuses on for a unit: its model centre at camera-focus height, else its tile centre
void EECameraControl::GetUnitPoint(EEUnit* inUnit, GE3DPoint& outPoint)
{
    GE3DPoint theOffset;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    if (theUnit != NULL) {
        float theScale = theUnit->mPhysicalModel.GetScale();
        theOffset = theUnit->mPhysicalModel.GetModel()->GetCenterPoint();
        theOffset.SetX(theOffset.GetX() * theScale);
        theOffset.SetY(theOffset.GetY() * theScale);
        theOffset.SetZ(theUnit->mPhysicalModel.GetModel()->GetBoundingBox(0).GetHeight() * theUnit->mStats->GetCameraFocus() *
                       theScale);
        theUnit->mPhysicalModel.GetLocation().Apply(theOffset, outPoint);
    } else {
        outPoint.SetX(inUnit->GetTileX() + 0.5f);
        outPoint.SetY(0.5f + inUnit->GetTileY());
        outPoint.SetZ(gWorld.GetMap()->mTerrainMesh.GetMeshZ(outPoint.GetX(), outPoint.GetY()) + 0.3f);
    }
}

// FUNCTION: EXE 0x52851e
// Returns the camera turn for following a unit (its heading plus the follow turn and zoom pitch), else the current
// offset turn
void EECameraControl::GetUnitTurn(EEUnit* inUnit, GETransformation& outTransformation)
{
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    float thePitch = EECameraControl::GetPitch(this->mZoom, this->mMode);
    if (theUnit != NULL && !inUnit->mUnitClass->mFacesCamera) {
        GETransformation theLocation(false);
        theLocation = theUnit->mPhysicalModel.GetLocation();
        theLocation.SetPosition(0.0f, 0.0f, 0.0f);
        theLocation.Rotate(this->mTurnYaw, this->mTurnPitch, 0.0f);
        GETransformation thePitchTransformation;
        thePitchTransformation.SetOrientationYPR(0.0f, thePitch, 0.0f);
        theLocation.Apply(thePitchTransformation, outTransformation);
    } else
        this->mQ05C.ConvertTo(outTransformation);
}

// FUNCTION: EXE 0x5285ec
// Returns the camera pitch for a zoom in a camera mode (linear, curved, the world camera's own pitch, or free camera)
float EECameraControl::GetPitch(float inZoom, long inMode)
{
    return ee_gfx_camera_pitch(inZoom, (int32_t)inMode, EE_CAM_T(gWorld.mTransformation));
}

// FUNCTION: EXE 0x5286fa
// Starts playing the camera path from its first key, dropping followed units (not while a path plays)
void EECameraControl::StartPath()
{
    if (this->mLocked)
        return;
    this->ClearUnitA();
    this->ClearUnitB();
    this->mPathIndex = -1;
    this->NextKey();
}

// FUNCTION: EXE 0x528722
// Turns the camera's view turn by yaw and pitch angles as a new move
void EECameraControl::Revolve(const GE3DPoint& inAngles)
{
    GETransformation theTransformation;
    GETransformation theRevolve;
    EECameraKey theKey;
    if (this->mLocked)
        return;
    GETransformation theRotation(false);
    GETransformation theResult(false);
    this->mQ04C.ConvertTo(theRotation);
    theTransformation.SetOrientationYPR(inAngles.GetX(), 0.0f, 0.0f);
    theRevolve.SetOrientationYPR(0.0f, inAngles.GetY(), 0.0f);
    theRotation.Apply(theRevolve, theResult);
    theTransformation.Apply(theResult, theKey.mT00);
    this->mQ05C.ConvertTo(theKey.mT40);
    theKey.mOffset = this->mEndOffset;
    theKey.mFocus = this->mEndFocus;
    theKey.mWait = 0.0f;
    theKey.mRate = 1.0f;
    theKey.mChanged = this->mChanged;
    this->GoTo(theKey);
}

// FUNCTION: EXE 0x52883b
// Creates a camera key (members only)
EECameraKey::EECameraKey()
{
}

// FUNCTION: EXE 0x52887d
// Turns the camera's offset turn by yaw and pitch angles as a new move
void EECameraControl::Rotate(const GE3DPoint& inAngles)
{
    GETransformation theTransformation;
    GETransformation theRevolve;
    EECameraKey theKey;
    if (this->mLocked)
        return;
    GETransformation theRotation(false);
    GETransformation theResult(false);
    this->mQ05C.ConvertTo(theRotation);
    theTransformation.SetOrientationYPR(inAngles.GetX(), 0.0f, 0.0f);
    theRevolve.SetOrientationYPR(0.0f, inAngles.GetY(), 0.0f);
    theRotation.Apply(theRevolve, theResult);
    theTransformation.Apply(theResult, theKey.mT40);
    this->mQ04C.ConvertTo(theKey.mT00);
    theKey.mOffset = this->mEndOffset;
    theKey.mFocus = this->mEndFocus;
    theKey.mWait = 0.0f;
    theKey.mRate = 1.0f;
    theKey.mChanged = this->mChanged;
    this->GoTo(theKey);
}

static const float kCameraFollowRise = -0.1f;

// FUNCTION: EXE 0x528996
// Starts a camera move that focuses on a unit or point and faces another unit or point, optionally following them,
// with a zoom, a duration and a follow turn
void EECameraControl::MoveTo(EEUnit* inUnitA, GE3DPoint inLocationA, bool inFlagA, float inDuration,
                               EEUnit* inUnitB, GE3DPoint inLocationB, bool inFlagB, float inZoom, float inTurnYaw,
                               float inTurnPitch)
{
    GEVector theDirection;
    GE3DPoint theTarget = inLocationB;
    EEUnit* theUnitA = inUnitA;
    EEUnit* theUnitB = inUnitB;
    bool theTurn = false;
    EECameraKey theKey;
    if (inUnitA == NULL || inUnitA->mHitPoints <= 0 || inUnitA->mTileX == -10000 || inUnitA->GetTileY() == -10000)
        theUnitA = NULL;
    if (inUnitB == NULL || inUnitB->mHitPoints <= 0 || inUnitB->mTileX == -10000 || inUnitB->GetTileY() == -10000)
        theUnitB = NULL;
    if (this->mLocked)
        return;
    this->mTurnYaw = inTurnYaw;
    this->mTurnPitch = inTurnPitch;
    theKey.mOffset = this->mEndOffset;
    theKey.mFocus = this->mEndFocus;
    theTarget.SetX(-10000.0f);
    this->mQ05C.ConvertTo(theKey.mT40);
    float theHeight;
    if (inZoom >= -1.0f) {
        float theZoom = inZoom;
        if (inZoom < 0.0f)
            theZoom = 0.0f;
        float theFar;
        if (this->mMode != 3 && !gFreeCamera && !gFarRearClipping)
            theFar = kCameraZoomDistance;
        else
            theFar = kCameraZoomDistanceFree;
        if (this->mMode != 3 && !gFreeCamera)
            theHeight = kCameraZoomHeight;
        else
            theHeight = kCameraZoomHeightFree;
        theKey.mOffset.SetY((theFar - theHeight) * theZoom + theHeight);
    }
    if (theUnitA != NULL) {
        if (this->mUnitA != NULL)
            this->ClearUnitA();
        if (inZoom >= -1.0f)
            this->mZoom = inZoom;
        if (theUnitA->mComplexUnit != NULL && theUnitA->mComplexUnit->mContainer != NULL)
            theUnitA = theUnitA->mComplexUnit->mContainer;
        if (theUnitA != NULL && theUnitA->mTileX != -10000 && theUnitA->GetTileY() != -10000) {
            if (inFlagA) {
                theUnitA->Reference();
                this->mUnitA = theUnitA;
            }
            this->GetUnitPoint(theUnitA, theKey.mFocus);
            if (this->mMode == 3 || gFreeCamera)
                theKey.mFocus.SetZ(theKey.mFocus.GetZ() + theKey.mOffset.GetY() * kCameraFollowRise);
        }
    } else {
        if (inLocationA.GetX() != -10000.0f) {
            if (this->mUnitA != NULL)
                this->ClearUnitA();
            theKey.mFocus = inLocationA;
        }
    }
    if (theUnitB != NULL) {
        if (this->mUnitB != NULL)
            this->ClearUnitB();
        if (inZoom >= -1.0f)
            this->mZoom = inZoom;
        if (theUnitB->mComplexUnit != NULL && theUnitB->mComplexUnit->mContainer != NULL)
            theUnitB = theUnitB->mComplexUnit->mContainer;
        if (theUnitB != NULL && theUnitB->mTileX != -10000 && theUnitB->GetTileY() != -10000) {
            if (inFlagB) {
                if (this->mUnitB != theUnitB) {
                    theUnitB->Reference();
                    this->mUnitB = theUnitB;
                }
                this->mIsFollowingUnitB = true;
            } else
                this->mIsFollowingUnitB = false;
            this->GetUnitPoint(theUnitB, theTarget);
        }
    } else {
        if (inLocationB.GetX() != -10000.0f) {
            if (this->mUnitB != NULL)
                this->ClearUnitB();
            theTarget = inLocationB;
            if (inFlagB)
                this->mTargetPoint = inLocationB;
        }
    }
    if (theTarget.GetX() != -10000.0f) {
        if (theUnitA != NULL && theUnitA == theUnitB)
            this->GetUnitTurn(theUnitB, theKey.mT40);
        else {
            if (inLocationA.GetX() != -10000.0f && inLocationB.GetX() != -10000.0f) {
                if (this->mMode == 2 || inZoom < -1.0f)
                    goto theDone;
                if (this->mMode == 3 || gFreeCamera)
                    theKey.mFocus.SetZ(theKey.mFocus.GetZ() + theKey.mOffset.GetY() * kCameraFollowRise);
                this->mZoom = inZoom;
                float thePitch = EECameraControl::GetPitch(inZoom, this->mMode);
                theKey.mT40.SetOrientationYPR(kCameraDefaultYaw, thePitch, 0.0f);
            }
            theTurn = true;
        }
    } else {
        if (this->mMode != 2 && inZoom >= -1.0f) {
            float thePitch = EECameraControl::GetPitch(inZoom, this->mMode);
            if (this->mTurnYaw == 0.0f)
                theKey.mT40.SetOrientationYPR(kCameraDefaultYaw, thePitch, 0.0f);
            else
                theKey.mT40.SetOrientationYPR(gWorld.mCameraYaw, thePitch, 0.0f);
        }
    }
theDone:
    theKey.mT00.MakeIdentity();
    theKey.mWait = 0.0f;
    if (inDuration != 0.0f)
        theKey.mRate = 1.0f / inDuration;
    else
        theKey.mRate = 0.0f;
    if (inDuration == 0.0f)
        theKey.mChanged = true;
    else
        theKey.mChanged = false;
    this->GoTo(theKey);
    if (theTurn) {
        GETransformation theCamera(false);
        theCamera = this->mView;
        theDirection.SetX(theTarget.GetX() - theKey.mFocus.mX);
        theDirection.SetY(theTarget.GetY() - theKey.mFocus.mY);
        theDirection.SetZ(theTarget.GetZ() - theKey.mFocus.mZ);
        theDirection.Normalize();
        theKey.mT40.SetOrientationYPR(UMArcTan2(-theDirection.GetX(), theDirection.GetY()),
                                      UMArcCos(-theDirection.GetZ()) - 1.5707964f, 0.0f);
        this->GoTo(theKey);
    }
}

// FUNCTION: EXE 0x528ea5
// Scrolls the camera focus over the ground by a screen delta scaled by the zoom, kept inside the map edges; leaves the
// free camera mode outside cinematics
void EECameraControl::Scroll(float inDX, float inDY)
{
    GE2DPoint theCenter(gViewport.GetXCenter(), gViewport.GetYCenter() + gViewport.GetHalfHeight());
    GE3DPoint theGround;
    GE3DPoint theTarget;
    GE3DPoint theStart;
    GE3DLine theLine;
    float theScale = this->mZoom + 0.5f;   // the zoom before a mode switch below resets it
    GEVector theDirection;
    EECameraKey theKey;
    EEMap* theMap = gWorld.mMap;
    float theMinimum = this->mMode != 3 ? kCameraMinimumHeight : kCameraMinimumHeightFree;
    float thePitch = EECameraControl::GetPitch(this->mZoom, this->mMode);
    if (this->mLocked)
        return;
    this->ClearUnitA();
    this->ClearUnitB();
    this->mTurnYaw = 0.0f;
    this->mTurnPitch = 0.0f;
    if (!gInCinematic && this->mMode == 3) {
        this->SetMode(0);
        this->ResetZoom();
    }
    EeGeVector theRustDirection = ee_gfx_camera_scroll_direction(EE_CAM_T(this->mView), inDX, inDY, theScale);
    theDirection = *reinterpret_cast<GEVector*>(&theRustDirection);
    this->GetLocation(this->mFraction, theTarget);
    theLine.SetStart(theTarget);
    theLine.SetDirection(theDirection);
    this->GetGroundPoint(theLine, theTarget);
    this->GetOffset(this->mFraction, theStart);
    if (ee_gfx_camera_scroll_is_near(EE_CAM_P(theStart))) {
        theMap->GetScreenLine(theCenter, theLine);
        if (theMap->GetLocationOnLine(theLine, theGround, true) != NULL) {
            theGround.SetZ(theGround.GetZ() + theMinimum);
            if (theGround.GetZ() > theTarget.GetZ())
                theTarget.SetZ(theGround.GetZ());
        }
    }
    if (theMap != NULL) {
        EeGfxEdgePlanes thePlanes = EECameraEdgePlanes(theMap->mTerrainMesh);
        ee_gfx_camera_clamp_to_map(EE_CAM_P(theTarget), &thePlanes);
    }
    theKey.mFocus = theTarget;
    if (this->mMode != 2) {
        theKey.mT40.SetOrientationYPR(kCameraDefaultYaw, thePitch, 0.0f);
        this->mChanged = true;
    } else
        this->mQ05C.ConvertTo(theKey.mT40);
    this->mQ04C.ConvertTo(theKey.mT00);
    theKey.mOffset = this->mEndOffset;
    theKey.mWait = 0.0f;
    theKey.mRate = 1.0f;
    theKey.mChanged = this->mChanged;
    this->GoTo(theKey);
    this->mStartFocus.SetX(this->mEndFocus.GetX());
    this->mStartFocus.SetY(this->mEndFocus.GetY());
}

// FUNCTION: EXE 0x52934f
// Resets the camera zoom to 1
void EECameraControl::ResetZoom()
{
    this->SetZoom(1.0f);
}

// FUNCTION: EXE 0x52935b
// Starts the move from the current pose to a camera key, with its speed from the distances or the followed unit's
// velocity
void EECameraControl::GoTo(const EECameraKey& inKey)
{
    EECameraKey theKey;
    float theSpeed;
    bool theFollowing;
    if (this->mUnitA != NULL && this->mUnitA->mComplexUnit != NULL && !gInCinematic)
        theFollowing = true;
    else
        theFollowing = false;
    this->CopyKey(inKey, theKey);
    this->KeepPose();
    this->mQ04C.ConvertFrom(theKey.mT00);
    this->mQ05C.ConvertFrom(theKey.mT40);
    this->mEndOffset = theKey.mOffset;
    this->mEndFocus = theKey.mFocus;
    this->mWait = theKey.mWait;
    this->mChanged = theKey.mChanged;
    // the followed unit's speed is read only when following (as the original did)
    float theFollowRate = theFollowing ? this->mUnitA->mComplexUnit->GetVelocity() * gWorld.GetGameSpeed() : 0.0f;
    EeGfxCameraGoTo theGoTo = ee_gfx_camera_go_to(EE_CAM_P(this->mStartOffset), EE_CAM_P(this->mEndOffset),
                                                  EE_CAM_P(this->mStartFocus), EE_CAM_P(this->mEndFocus), theKey.mRate,
                                                  theFollowing, theFollowRate);
    if (theGoTo.snap_offset)
        this->mStartOffset = this->mEndOffset;
    if (theGoTo.snap_focus)
        this->mStartFocus = this->mEndFocus;
    theSpeed = theGoTo.speed;
theDone:
    this->mFraction = 0.0f;
    this->mRate = theSpeed;
    this->mWaited = 0.0f;
}

// FUNCTION: EXE 0x529547
// Moves the camera offset to a point as a new move (at once when inFlag), dropping followed units
void EECameraControl::SetOffset(const GE3DPoint& inPoint, bool inFlag)
{
    EECameraKey theKey;
    if (this->mLocked)
        return;
    this->ClearUnitA();
    this->ClearUnitB();
    theKey.mOffset = inPoint;
    this->mQ04C.ConvertTo(theKey.mT00);
    this->mQ05C.ConvertTo(theKey.mT40);
    theKey.mFocus = this->mEndFocus;
    theKey.mWait = 0.0f;
    theKey.mRate = 1.0f;
    theKey.mChanged = this->mChanged;
    this->GoTo(theKey);
    if (inFlag)
        this->mStartOffset = this->mEndOffset;
}

// FUNCTION: EXE 0x5295fd
// Turns the camera view to a transformation as a new move (at once when inFlag), dropping followed units
void EECameraControl::SetViewTurn(const GETransformation& inTransformation, bool inFlag)
{
    EECameraKey theKey;
    if (this->mLocked)
        return;
    this->ClearUnitA();
    this->ClearUnitB();
    theKey.mT00 = inTransformation;
    this->mQ05C.ConvertTo(theKey.mT40);
    theKey.mOffset = this->mEndOffset;
    theKey.mFocus = this->mEndFocus;
    theKey.mWait = 0.0f;
    theKey.mRate = 1.0f;
    theKey.mChanged = this->mChanged;
    this->GoTo(theKey);
    if (inFlag)
        this->mQ014 = this->mQ04C;
}

// FUNCTION: EXE 0x5296b6
// Turns the camera offset to a transformation as a new move (at once when inFlag), dropping followed units
void EECameraControl::SetOffsetTurn(const GETransformation& inTransformation, bool inFlag)
{
    EECameraKey theKey;
    if (this->mLocked)
        return;
    this->ClearUnitA();
    this->ClearUnitB();
    theKey.mT40 = inTransformation;
    this->mQ04C.ConvertTo(theKey.mT00);
    theKey.mOffset = this->mEndOffset;
    theKey.mFocus = this->mEndFocus;
    theKey.mWait = 0.0f;
    theKey.mRate = 1.0f;
    theKey.mChanged = this->mChanged;
    this->GoTo(theKey);
    if (inFlag)
        this->mQ024 = this->mQ05C;
}

// FUNCTION: EXE 0x52976f
// Makes the camera's current interpolated pose the start of the next move
void EECameraControl::KeepPose()
{
    GE3DPoint thePoint;
    GEQuaternion theQuaternion;
    this->SlerpViewTurn(this->mFraction, theQuaternion);
    this->mQ014 = theQuaternion;
    this->SlerpOffsetTurn(this->mFraction, theQuaternion);
    this->mQ024 = theQuaternion;
    this->GetOffset(this->mFraction, thePoint);
    this->mStartOffset = thePoint;
    this->GetLocation(this->mFraction, thePoint);
    this->mStartFocus = thePoint;
}

// FUNCTION: EXE 0x529844
// Moves the camera focus to a location (on the terrain when inFollowTerrain) as a new move, at once when inIsImmediate
void EECameraControl::SetLocation(const GE3DPoint& inLocation, bool inIsImmediate, bool inFollowTerrain)
{
    GE3DPoint theLocation = inLocation;
    EECameraKey theKey;
    GETerrainMesh& theMesh = gWorld.GetMap()->mTerrainMesh;
    float theMinimum = this->mMode != 3 ? kCameraMinimumHeight : kCameraMinimumHeightFree;
    if (this->mLocked)
        return;
    this->ClearUnitA();
    this->ClearUnitB();
    unsigned long theX = UMFloorUnsigned(theLocation.GetX());
    unsigned long theY = UMFloorUnsigned(theLocation.GetY());
    if (!gOrthographicView && inFollowTerrain) {
        if (theMesh.GetIsValidTile(theX, theY)) {
            theLocation.SetZ(theMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()) + theMinimum);
            if (theLocation.GetZ() < 0.0f && !gFreeCamera)
                theLocation.SetZ(theMinimum);

        } else {
            theLocation.SetZ(theMinimum);
        }
    }

    theKey.mFocus = theLocation;
    this->mQ04C.ConvertTo(theKey.mT00);
    this->mQ05C.ConvertTo(theKey.mT40);
    theKey.mOffset = this->mEndOffset;
    theKey.mWait = 0.0f;
    theKey.mRate = 1.0f;
    theKey.mChanged = this->mChanged;
    this->GoTo(theKey);
    if (inIsImmediate)
        this->mStartFocus = this->mEndFocus;
}

// FUNCTION: EXE 0x5299c0
// Sets the camera mode and reapplies the zoom
void EECameraControl::SetMode(long inMode)
{
    this->mMode = inMode;
    this->SetZoom(this->mZoom);
}

// FUNCTION: EXE 0x5299dc
// Makes the camera jump to its target on the next update, unless a path plays
void EECameraControl::SetChanged()
{
    if (!this->mLocked)
        this->mChanged = true;
}

// FUNCTION: EXE 0x5299ed
// Stops following the focused unit, releasing it
void EECameraControl::ClearUnitA()
{
    if (this->mUnitA != NULL) {
        this->mUnitA->Dereference();
        this->mUnitA = NULL;
    }
}

// FUNCTION: EXE 0x529a04
// Stops facing the target unit or point, releasing the unit
void EECameraControl::ClearUnitB()
{
    if (this->mUnitB != NULL) {
        this->mUnitB->Dereference();
        this->mUnitB = NULL;
    }
    this->mIsFollowingUnitB = false;
    this->mTargetPoint.SetX(-10000.0f);
}

static const float kCameraUpdateZoom = -1.1f;

// FUNCTION: EXE 0x529a34
// Advances the camera each frame: drops followed units that died or went out of sight, steps the move or path and
// applies the view, clipping distances and orthographic size to the viewport
void EECameraControl::Update(float inElapsedTime)
{
    GE3DPoint thePoint;
    GE3DPoint theNoLocation(-10000.0f, 0.0f, 0.0f);
    GERectangle theRectangle(-1.0f, -1.0f, 1.0f, 1.0f);
    EEUnit* theUnit = this->mUnitA;
    if (theUnit != NULL &&
        (theUnit->mHitPoints <= 0 || theUnit->mTileX == -10000 || theUnit->GetTileY() == -10000 ||
         (this->mMode != 3 && theUnit->mPlayer != gWorld.GetCurrentPlayer() && !EEIsSeen(theUnit))))
        this->ClearUnitA();
    theUnit = this->mUnitB;
    if (theUnit != NULL &&
        (theUnit->mHitPoints <= 0 || theUnit->mTileX == -10000 || theUnit->GetTileY() == -10000 ||
         (this->mMode != 3 && theUnit->mPlayer != gWorld.GetCurrentPlayer() && !EEIsSeen(theUnit))))
        this->ClearUnitB();
    if (this->mUnitA != NULL || this->mUnitB != NULL || this->mTargetPoint.GetX() != -10000.0f) {
        this->MoveTo(this->mUnitA, theNoLocation, this->mUnitA != NULL, this->mChanged ? 0.0f : 1.0f, this->mUnitB, this->mTargetPoint,
                 this->mTargetPoint.GetX() == -10000.0f && this->mUnitB == NULL ? 0 : 1, kCameraUpdateZoom, this->mTurnYaw, this->mTurnPitch);
    }
    if (gWorld.mMap == NULL)
        return;
    EeGfxCameraStep theStep;
    theStep.fraction = this->mFraction;
    theStep.rate = this->mRate;
    theStep.wait = this->mWait;
    theStep.waited = this->mWaited;
    theStep.changed = this->mChanged;
    theStep.locked = this->mLocked;
    int32_t theAction = ee_gfx_camera_step(&theStep, inElapsedTime);
    this->mFraction = theStep.fraction;
    this->mWaited = theStep.waited;
    this->mChanged = theStep.changed;
    if (theAction == 1) {
        this->NextKey();
        return;
    }
    if (theAction == 0)
        return;
    if (!gOrthographicView)
        this->BuildView(this->mView, this->mFraction);
    else {
        this->GetLocation(this->mFraction, thePoint);
        GE3DPoint theOffset;
        this->GetOffset(this->mFraction, theOffset);
        EeGfxOrthoView theView = ee_gfx_camera_ortho_view(EE_CAM_P(thePoint), theOffset.GetY());
        this->mView = *reinterpret_cast<GETransformation*>(&theView.view);
        theRectangle.SetLeft(-theView.size);
        theRectangle.SetTop(-theView.size);
        theRectangle.SetRight(theView.size);
        theRectangle.SetBottom(theView.size);
        gViewport.SetOrthographicRectangle(theRectangle);
    }
    gWorld.SetCameraTransformation(this->mView);
    gViewport.SetFieldOfView(sCameraFieldOfView);
    this->SetClippingDistances();
    if (gMiniMap != NULL)
        gMiniMap->RefreshView();
    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
}

// FUNCTION: EXE 0x529dfe
// Starts following a unit (and facing it when inFlag), unless it is a foreign unit the local player cannot see
void EECameraControl::Follow(EEUnit* inUnit, bool inFlag, float inDuration, float inTurnYaw, float inTurnPitch)
{
    GE3DPoint theNoLocation(-10000.0f, 0.0f, 0.0f);
    if (inUnit != NULL && inUnit->mPlayer != gWorld.GetCurrentPlayer() && !EEIsSeen(inUnit))
        return;
    this->ClearUnitA();
    this->ClearUnitB();
    this->mTurnYaw = inTurnYaw;
    this->mTurnPitch = inTurnPitch;
    this->MoveTo(inUnit, theNoLocation, true, inDuration, inFlag ? inUnit : NULL, theNoLocation, inFlag, this->mZoom, inTurnYaw, inTurnPitch);
}

// FUNCTION: EXE 0x529eb2
// Sets the camera zoom and re-aims at the current focus and target over 0.3 s (not while a path plays)
void EECameraControl::SetZoom(float inZoom)
{
    GE3DPoint theNoLocation(-10000.0f, 0.0f, 0.0f);
    EECameraKey theKey;
    if (this->mLocked)
        return;
    this->mZoom = inZoom;
    this->MoveTo(this->mUnitA, theNoLocation, this->mUnitA != NULL, 0.3f, this->mUnitB, this->mTargetPoint, this->mTargetPoint.GetX() == -10000.0f && this->mUnitB == NULL ? 0 : 1,
             inZoom, this->mTurnYaw, this->mTurnPitch);
}

// The vector<EECameraKey> COMDATs (push_back, clear, insert, erase, _Buy, insert(P, M, X), the iterator helpers,
// the copy/fill helpers):
// TEMPLATE: EXE 0x529f73
// TEMPLATE: EXE 0x529f9f
// TEMPLATE: EXE 0x529fda
// TEMPLATE: EXE 0x52a037
// TEMPLATE: EXE 0x52a081
// TEMPLATE: EXE 0x52a0bb
// TEMPLATE: EXE 0x52a3aa
// TEMPLATE: EXE 0x52a3bf
// TEMPLATE: EXE 0x52a426
// TEMPLATE: EXE 0x52a454
// TEMPLATE: EXE 0x52a46b
// TEMPLATE: EXE 0x52a497
// TEMPLATE: EXE 0x52a4cb
// TEMPLATE: EXE 0x52a4eb
// TEMPLATE: EXE 0x52a51c

// FUNCTION: EXE 0x52a52e
// Returns whether the local player can see a unit
bool EEIsSeen(EEUnit* inUnit)
{
    return EEIsSeenBy(inUnit, gWorld.GetCurrentPlayer(), true);
}
