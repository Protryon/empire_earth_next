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

// FUNCTION: EXE 0x527584
// FUNCTION: EXE 0x52758e
// FUNCTION: EXE 0x52759f
// FUNCTION: EXE 0x5275ab
// Constructs the game version string "2002.8.17.v1.00" and registers its destructor to run at exit
UWideString gGameVersion("2002.8.17.v1.00");

extern bool& gFarRearClipping;               // 0x5308d2 (set: the far rear clipping distance; provisional name)
extern bool& gInCinematic;                   // 0x5318e0 (set: scrolling keeps the free camera mode; provisional name)

// the camera's limits (unit-local .rdata 0x441e48..0x441eb8, before the vtable; provisional names)
static const float kCameraOrthographicWidth = 800.0f;
static const float kCameraOrthographicYaw = 0.7853982f;
static const float kCameraOrthographicPitch = -0.5235988f;
static const float kCameraOrthographicHeight = -40.0f;
static const float kCameraOrthographicNear = -5.0f;
static const float kCameraOrthographicFar = -48.0f;
static const float kCameraMinimumTime = 2.5f;
static const float kCameraMoveSpeed = 15.0f;
static const float kCameraFollowSpeed = 15.0f;
static const float kCameraMinimumHeightFree = 0.1f;
static const float kCameraMinimumHeight = 0.4f;
static const float kCameraNearDistance = 10.0f;
static const float kCameraDegreesToRadians = 0.017453292f;
static const float kCameraDefaultYaw = 0.7853982f;
static const float kCameraZoomHeight = 0.1f;
static const float kCameraZoomHeightFree = 0.0f;
static const float kCameraRearZ = -5.0f;
static const float kCameraFrontZ = 10.0f;
static const float kCameraFrontMinimum = 0.1f;
static const float kCameraRearMaximum = 85.0f;
static const float kCameraRearMaximum2 = 85.0f;
static const float kCameraZoomDistance = -20.5f;
static const float kCameraRearNormal = 35.0f;
static const float kCameraZoomDistanceFree = -42.0f;
static const float kCameraRearFree = 58.0f;

// port: the farthest zoom (1.0 in the original; the camera's distance grows linearly past it, its pitch stays at the
// farthest), from config.yaml's camera: maximum_zoom_out (EEPreferences)
float gCameraMaxZoom = 2.0f;

// FUNCTION: EXE 0x5275b6
// FUNCTION: EXE 0x5275bb
// Initialises the camera's field of view (30 degrees)
static float sCameraFieldOfView = kCameraDegreesToRadians * 30.0f;
// FUNCTION: EXE 0x5275ce
// FUNCTION: EXE 0x5275d3
// Initialises the camera pitch at the closest zoom (-5 degrees)
static float sCameraPitchNear = kCameraDegreesToRadians * -5.0f;
// FUNCTION: EXE 0x5275e6
// FUNCTION: EXE 0x5275eb
// Initialises the camera pitch at the farthest zoom (-35 degrees)
static float sCameraPitchFar = kCameraDegreesToRadians * -35.0f;
// FUNCTION: EXE 0x5275fe
// FUNCTION: EXE 0x527603
// Initialises the free camera's pitch at the closest zoom (0 degrees)
static float sCameraFreePitchNear = kCameraDegreesToRadians * 0.0f;
// FUNCTION: EXE 0x52760c
// FUNCTION: EXE 0x527611
// Initialises the free camera's pitch at the farthest zoom (-35 degrees)
static float sCameraFreePitchFar = kCameraDegreesToRadians * -35.0f;
// FUNCTION: EXE 0x527624
// FUNCTION: EXE 0x527629
// Initialises the view ray tilt (25 degrees) the rear clipping limit scales with
static float sCameraTiltLimit = kCameraDegreesToRadians * 25.0f;
// FUNCTION: EXE 0x52763c
// FUNCTION: EXE 0x527641
// Initialises the extra rear clipping range for tilted view rays (85 - 85: none)
static float sCameraRearRange = kCameraRearMaximum - 85.0f;

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
    GEVector theOffset(0.0f, gViewport.GetFrontClippingDistance() + 1.0f, 0.0f);
    GEVector theRotated;
    float theMinimum = this->mMode != 3 ? kCameraMinimumHeight : kCameraMinimumHeightFree;
    GE3DPoint thePosition(ioTransformation.GetX(), ioTransformation.GetY(), ioTransformation.GetZ());
    unsigned long theX = UMFloorUnsigned(thePosition.GetX());
    unsigned long theY = UMFloorUnsigned(thePosition.GetY());
    float theZ = thePosition.GetZ();
    if (theMesh.GetIsValidTile(theX, theY)) {
        float theHeight = theMesh.GetMeshZ(thePosition.GetX(), thePosition.GetY()) + theMinimum;
        if (theHeight > theZ)
            theZ = theHeight;
    } else if (thePosition.GetZ() < theMinimum)
        theZ = theMinimum;
    ioTransformation.Apply(theOffset, theRotated);
    thePosition += theRotated;
    theX = UMFloorUnsigned(thePosition.GetX());
    theY = UMFloorUnsigned(thePosition.GetY());
    if (theMesh.GetIsValidTile(theX, theY)) {
        float theHeight = theMesh.GetMeshZ(thePosition.GetX(), thePosition.GetY());
        if (theHeight > theZ)
            theZ = theHeight + theMinimum;
    }
    if (this->mUnitA == NULL && this->mUnitB == NULL && !gFreeCamera && !this->mLocked && theZ < kCameraMinimumHeight)
        theZ = kCameraMinimumHeight;
    ioTransformation.SetZ(theZ);
}

// the distance along the view ray through inPoint to where it meets inZ (when it points down), at most the rear
// limit for its pitch
static float EECameraRayDistance(const GE2DPoint& inPoint, float inZ, float inDistance);

// FUNCTION: EXE 0x5279f0
// Sets the viewport's front and rear clipping distances from the view rays through the screen's top, centre and bottom
// (fixed ones in the orthographic view)
void EECameraControl::SetClippingDistances()
{
    GE2DPoint theTop(gViewport.GetXCenter(), gViewport.GetYCenter() + gViewport.GetHalfHeight());
    GE2DPoint theCenter(gViewport.GetXCenter(), gViewport.GetYCenter());
    GE2DPoint theBottom(gViewport.GetXCenter(), gViewport.GetYCenter() - gViewport.GetHalfHeight());
    float theRear;
    if (!gOrthographicView) {
        float theDistance;
        theRear = EECameraRayDistance(theCenter, kCameraRearZ, kCameraRearMaximum);
        theDistance = EECameraRayDistance(theTop, kCameraRearZ, kCameraRearMaximum);
        if (theDistance > theRear)
            theRear = theDistance;
        theDistance = EECameraRayDistance(theBottom, kCameraRearZ, kCameraRearMaximum);
        if (theDistance > theRear)
            theRear = theDistance;
        if (theRear > kCameraRearMaximum)
            theRear = kCameraRearMaximum;
        float theFront = EECameraRayDistance(theCenter, kCameraFrontZ, kCameraFrontMinimum);
        theDistance = EECameraRayDistance(theTop, kCameraFrontZ, kCameraFrontMinimum);
        if (theDistance < theFront)
            theFront = theDistance;
        theDistance = EECameraRayDistance(theBottom, kCameraFrontZ, kCameraFrontMinimum);
        if (theDistance < theFront)
            theFront = theDistance;
        if (theFront < kCameraFrontMinimum)
            theFront = kCameraFrontMinimum;
        if (theFront >= theRear && theFront != theRear)
            theFront = theRear;
        if (gFreeCamera)
            theFront = kCameraFrontMinimum;
        gViewport.SetFrontClippingDistance(theFront);
        if (this->mMode != 3 && !gFreeCamera && !gFarRearClipping)
            theRear = kCameraRearNormal * (this->mZoom > 1.0f ? this->mZoom : 1.0f);   // port: the farther zoom-out
        else
            theRear = kCameraRearFree;
    } else {
        theRear = kCameraRearMaximum;
        gViewport.SetFrontClippingDistance(kCameraFrontMinimum);
    }
    gViewport.SetRearClippingDistance(theRear);
}

// FUNCTION: EXE 0x527c09
// Returns how far the view ray through a screen point travels down to a height, else inDistance, capped at the rear
// limit
static float EECameraRayDistance(const GE2DPoint& inPoint, float inZ, float inDistance)
{
    GE3DPoint theCamera;
    GE3DPoint theIntersection;
    GE3DLine theLine;
    GE3DPlane thePlane;
    GEVector theDirection;
    float theT;
    float theDistance = inDistance;
    bool theUp = true;
    float theMaximum;
    gWorld.GetMap()->GetScreenLine(inPoint, theLine);
    theDirection = theLine.GetDirection();
    theDirection.Normalize();
    theLine.SetDirection(theDirection);
    theCamera.SetX(gViewport.GetCamera().GetX());
    theCamera.SetY(gViewport.GetCamera().GetY());
    theCamera.SetZ(gViewport.GetCamera().GetZ());
    thePlane.Define(theCamera, theDirection);
    theLine.Intersects(thePlane, theT);
    theLine.GetPointAt(theT, theIntersection);
    theLine.SetStart(theIntersection);
    float theAngle = UMArcCos(-theDirection.GetZ()) - 1.5707964f;
    if (theAngle < 0.0f) {
        theAngle = -theAngle;
        theUp = false;
    } else if (theAngle <= sCameraTiltLimit)
        theAngle = 0.0f;
    if (theAngle > sCameraTiltLimit)
        theMaximum = kCameraRearMaximum;
    else
        theMaximum = sCameraRearRange * (theAngle / sCameraTiltLimit) + kCameraRearMaximum2;
    if (!theUp && theLine.SolveForZ(inZ, theT, theIntersection) && theT > 0.0f)
        theDistance = theIntersection.DistanceTo(theLine.GetStart());
    if (theDistance > theMaximum)
        theDistance = theMaximum;
    return theDistance;
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
    GE3DPoint thePoint;
    GETransformation theRevolve;
    GETransformation theRotation(false);
    this->GetOffset(inStep, thePoint);
    theRevolve.SetPosition(thePoint);
    this->GetOffsetTurn(inStep, theRotation);
    theRotation.Apply(theRevolve, outTransformation);
    this->GetLocation(inStep, thePoint);
    outTransformation.Translate(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
    this->ClampHeight(outTransformation);
    thePoint = outTransformation.GetPosition();
    outTransformation.SetPosition(0.0f, 0.0f, 0.0f);
    GETransformation theOrientation(false);
    this->GetViewTurn(inStep, theOrientation);
    float thePitch = theOrientation.GetElement(2, 1);
    float theYaw = UMArcTan2(theOrientation.GetElement(1, 0), theOrientation.GetElement(0, 0));
    outTransformation.Rotate(theYaw, UMArcCos(-thePitch) - 1.5707964f, 0.0f);
    outTransformation.Translate(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
}

// FUNCTION: EXE 0x52802b
// Returns the camera offset interpolated between the move's start and end
void EECameraControl::GetOffset(float inStep, GE3DPoint& outPoint)
{
    outPoint.SetX((this->mEndOffset.GetX() - this->mStartOffset.GetX()) * inStep + this->mStartOffset.GetX());
    outPoint.SetY((this->mEndOffset.GetY() - this->mStartOffset.GetY()) * inStep + this->mStartOffset.GetY());
    outPoint.SetZ((this->mEndOffset.GetZ() - this->mStartOffset.GetZ()) * inStep + this->mStartOffset.GetZ());
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
    outLocation.SetX((this->mEndFocus.GetX() - this->mStartFocus.GetX()) * inStep + this->mStartFocus.GetX());
    outLocation.SetY((this->mEndFocus.GetY() - this->mStartFocus.GetY()) * inStep + this->mStartFocus.GetY());
    outLocation.SetZ((this->mEndFocus.GetZ() - this->mStartFocus.GetZ()) * inStep + this->mStartFocus.GetZ());
}

// FUNCTION: EXE 0x52816e
// Returns a scroll line's end clipped to the map edges, at terrain height when the camera is close or the point is
// below ground, else at the minimum height
void EECameraControl::GetGroundPoint(const GE3DLine& inLine, GE3DPoint& outPoint)
{
    GE3DPoint theStart;
    GETerrainMesh& theMesh = gWorld.GetMap()->mTerrainMesh;
    float theMinimum = this->mMode != 3 ? kCameraMinimumHeight : kCameraMinimumHeightFree;
    inLine.GetPointAt(1.0f, outPoint);
    this->ClipToEdges(inLine, outPoint);
    long theX = UMFloor(outPoint.GetX());
    long theY = UMFloor(outPoint.GetY());
    this->GetOffset(this->mFraction, theStart);
    if (theStart.GetX() * theStart.GetX() + theStart.GetY() * theStart.GetY() + theStart.GetZ() * theStart.GetZ() <=
            kCameraNearDistance * kCameraNearDistance || outPoint.GetZ() < 0.0f) {
        if (theMesh.GetIsValidPoint(UMFloorUnsigned(outPoint.GetX()), UMFloorUnsigned(outPoint.GetY()))) {
            outPoint.SetZ(theMesh.GetMeshZ(outPoint.GetX(), outPoint.GetY()) + theMinimum);
            return;
        }
    }
    outPoint.SetZ(theMinimum);
}

// FUNCTION: EXE 0x5282c3
// Moves a point back to where a line first crosses one of the map's four edge planes, nudged just inside
void EECameraControl::ClipToEdges(const GE3DLine& inLine, GE3DPoint& ioPoint)
{
    float theT;
    float theNearest = 1.0f;
    if (inLine.Intersects(gWorld.GetMap()->mTerrainMesh.GetFrontPlane(), theT) && theT >= 0.0f && theT < 1.0f) {
        inLine.GetPointAt(theT, ioPoint);
        theNearest = theT;
        ioPoint.SetX(ioPoint.GetX() + 0.0001f);
        ioPoint.SetY(ioPoint.GetY() - 0.0001f);
    }
    if (inLine.Intersects(gWorld.GetMap()->mTerrainMesh.GetBackPlane(), theT) && theT >= 0.0f && theT < theNearest) {
        inLine.GetPointAt(theT, ioPoint);
        theNearest = theT;
        ioPoint.SetX(ioPoint.GetX() - 0.0001f);
        ioPoint.SetY(ioPoint.GetY() + 0.0001f);
    }
    if (inLine.Intersects(gWorld.GetMap()->mTerrainMesh.GetLeftPlane(), theT) && theT >= 0.0f && theT < theNearest) {
        inLine.GetPointAt(theT, ioPoint);
        theNearest = theT;
        ioPoint.SetX(ioPoint.GetX() + 0.0001f);
        ioPoint.SetY(ioPoint.GetY() + 0.0001f);
    }
    if (!inLine.Intersects(gWorld.GetMap()->mTerrainMesh.GetRightPlane(), theT) || !(theT >= 0.0f) || !(theT < theNearest))
        return;
    inLine.GetPointAt(theT, ioPoint);
    ioPoint.SetX(ioPoint.GetX() - 0.0001f);
    ioPoint.SetY(ioPoint.GetY() - 0.0001f);
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

// the zoom curve's exponent in mode 1 (.rdata 0x441ec8, after the vtable)
static const double kCameraZoomExponent = 0.3;

// FUNCTION: EXE 0x5285ec
// Returns the camera pitch for a zoom in a camera mode (linear, curved, the world camera's own pitch, or free camera)
float EECameraControl::GetPitch(float inZoom, long inMode)
{
    GEVector theUp(0.0f, 1.0f, 0.0f);
    GEVector theRotated;
    float theResult = 0.0f;
    if ((inMode == 0 || inMode == 1) && inZoom > 1.0f)
        inZoom = 1.0f;   // port: zoomed out past the original's farthest, the pitch stays there
    if (inMode == 0)
        return (sCameraPitchFar - sCameraPitchNear) * (1.0f - (float)pow(1.0f - inZoom, 1.0)) + sCameraPitchNear;
    if (inMode == 1)
        return (sCameraPitchFar - sCameraPitchNear) * (1.0f - (float)pow(1.0f - inZoom, kCameraZoomExponent)) + sCameraPitchNear;
    if (inMode == 2) {
        gWorld.mTransformation.Apply(theUp, theRotated);
        return UMArcCos(-theRotated.GetZ()) - 1.5707964f;
    }
    if (inMode == 3) {
        float theZoom = inZoom;
        if (theZoom < 0.0f)
            theZoom = -theZoom;
        theResult = (sCameraFreePitchFar - sCameraFreePitchNear) * (1.0f - (float)pow(1.0f - theZoom, 0.5)) +
                    sCameraFreePitchNear;
        if (inZoom < 0.0f)
            theResult = -theResult;
    }
    return theResult;
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
            theKey.mT40.SetOrientationYPR(this->mTurnYaw == 0.0f ? kCameraDefaultYaw : gWorld.mCameraYaw, thePitch, 0.0f);
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
        theDirection.SetX(theTarget.GetX() - theKey.mFocus.GetX());
        theDirection.SetY(theTarget.GetY() - theKey.mFocus.GetY());
        theDirection.SetZ(theTarget.GetZ() - theKey.mFocus.GetZ());
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
    float theScale = this->mZoom + 0.5f;
    GEVector theDelta(theScale * inDX, theScale * inDY, 0.0f);
    GEVector theDirection;
    EECameraKey theKey;
    EEMap* theMap = gWorld.mMap;
    float theMinimum = this->mMode != 3 ? kCameraMinimumHeight : kCameraMinimumHeightFree;
    float theLength = (float)sqrt(inDX * inDX + inDY * inDY);
    float thePitch = EECameraControl::GetPitch(this->mZoom, this->mMode);
    float theT;
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
    this->mView.Apply(theDelta, theDirection);
    if (theDirection.GetX() == 0.0f && theDirection.GetY() == 0.0f) {
        theDelta = GEVector(theDelta.GetX(), 0.0f, inDY);
        this->mView.Apply(theDelta, theDirection);
        if (theDirection.GetX() == 0.0f && theDirection.GetY() == 0.0f) {
            theDelta = GEVector(0.0f, inDY, inDX);
            this->mView.Apply(theDelta, theDirection);
        }
    }
    theDirection = GEVector(theDirection.GetX(), theDirection.GetY(), 0.0f);
    theDirection.Normalize();
    theDirection = GEVector(theDirection.GetX() * theLength, theDirection.GetY() * theLength, theDirection.GetZ());
    this->GetLocation(this->mFraction, theTarget);
    theLine.SetStart(theTarget);
    theLine.SetDirection(theDirection);
    this->GetGroundPoint(theLine, theTarget);
    this->GetOffset(this->mFraction, theStart);
    if (theStart.GetX() * theStart.GetX() + theStart.GetY() * theStart.GetY() + theStart.GetZ() * theStart.GetZ() <=
        kCameraNearDistance * kCameraNearDistance) {
        theMap->GetScreenLine(theCenter, theLine);
        if (theMap->GetLocationOnLine(theLine, theGround, true) != NULL) {
            theGround.SetZ(theGround.GetZ() + theMinimum);
            if (theGround.GetZ() > theTarget.GetZ())
                theTarget.SetZ(theGround.GetZ());
        }
    }
    if (theMap != NULL) {
        const GE3DPlane* thePlane;
        if (theMap->mTerrainMesh.GetLeftPlane().IsPointOnOrBelowPlane(theTarget)) {
            theLine.SetStart(theTarget);
            theLine.SetDirection(theMap->mTerrainMesh.GetLeftPlane().GetNormal());
            if (theLine.Intersects(theMap->mTerrainMesh.GetLeftPlane(), theT))
                theLine.GetPointAt(theT, theTarget);
        } else if (theMap->mTerrainMesh.GetRightPlane().IsPointOnOrBelowPlane(theTarget)) {
            theLine.SetStart(theTarget);
            theLine.SetDirection(theMap->mTerrainMesh.GetRightPlane().GetNormal());
            if (theLine.Intersects(theMap->mTerrainMesh.GetRightPlane(), theT))
                theLine.GetPointAt(theT, theTarget);
        }
        thePlane = &theMap->mTerrainMesh.GetFrontPlane();
        if (thePlane->IsPointOnOrBelowPlane(theTarget)) {
            theLine.SetStart(theTarget);
            theLine.SetDirection(thePlane->GetNormal());
            if (theLine.Intersects(*thePlane, theT))
                theLine.GetPointAt(theT, theTarget);
        } else {
            thePlane = &theMap->mTerrainMesh.GetBackPlane();
            if (thePlane->IsPointOnOrBelowPlane(theTarget)) {
                theLine.SetStart(theTarget);
                theLine.SetDirection(thePlane->GetNormal());
                if (theLine.Intersects(*thePlane, theT))
                    theLine.GetPointAt(theT, theTarget);
            }
        }
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
    float theSpeed = 0.0f;
    float theFollowSpeed = kCameraFollowSpeed;
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
    float theDistance = this->mStartOffset.DistanceTo(this->mEndOffset);
    if (theDistance > 0.001f)
        theSpeed = kCameraMoveSpeed * theKey.mRate / theDistance;
    else
        this->mStartOffset = this->mEndOffset;
    theDistance = this->mStartFocus.DistanceTo(this->mEndFocus);
    if (theDistance > 0.001f || theFollowing) {
        float theRate;
        if (theFollowing) {
            theRate = this->mUnitA->mComplexUnit->GetVelocity() * gWorld.GetGameSpeed();
            if (theRate < kCameraFollowSpeed)
                theRate = kCameraFollowSpeed;
        } else
            theRate = theFollowSpeed;
        float theTime = theKey.mRate;
        float theNewSpeed = theTime * theRate / theDistance;
        if ((theNewSpeed > 0.0f && theNewSpeed < theSpeed) || theSpeed == 0.0f || theFollowing) {
            theSpeed = theNewSpeed;
            if (theFollowing)
                goto theDone;
        }
    } else
        this->mStartFocus = this->mEndFocus;
    if (theSpeed == 0.0f || 1.0f / theSpeed < 1.0f / (kCameraMinimumTime * theKey.mRate))
        theSpeed = kCameraMinimumTime * theKey.mRate;
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

static const float kCameraOrthographicScale = 0.008417938f;
static const float kCameraUpdateZoom = -1.1f;

// FUNCTION: EXE 0x529a34
// Advances the camera each frame: drops followed units that died or went out of sight, steps the move or path and
// applies the view, clipping distances and orthographic size to the viewport
void EECameraControl::Update(float inElapsedTime)
{
    GE3DPoint thePoint;
    GE3DPoint theNoLocation(-10000.0f, 0.0f, 0.0f);
    GETransformation theTransformation;
    GETransformation theOrthographic;
    GERectangle theRectangle(-1.0f, -1.0f, 1.0f, 1.0f);
    EECameraKey theKey;
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
    if (this->mFraction == 1.0f) {
        if (!this->mChanged) {
            if (!this->mLocked)
                return;
            if (this->mWaited >= this->mWait) {
                this->NextKey();
                return;
            }
            this->mWaited += inElapsedTime;
            return;
        }
    } else if (!this->mChanged) {
        this->mFraction += inElapsedTime * this->mRate;
        if (this->mFraction > 1.0f)
            this->mFraction = 1.0f;
        goto theUpdate;
    }
    this->mChanged = false;
    this->mFraction = 1.0f;
theUpdate:
    if (!gOrthographicView)
        this->BuildView(this->mView, this->mFraction);
    else {
        thePoint.SetXYZ(0.0f, kCameraOrthographicHeight, 0.0f);
        theOrthographic.SetPosition(thePoint);
        theTransformation.SetOrientationYPR(kCameraOrthographicYaw, kCameraOrthographicPitch, 0.0f);
        theTransformation.Apply(theOrthographic, this->mView);
        this->GetLocation(this->mFraction, thePoint);
        this->mView.Translate(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
        this->GetOffset(this->mFraction, thePoint);
        float theRatio = (-thePoint.GetY() - -kCameraOrthographicNear) / (-kCameraOrthographicFar - -kCameraOrthographicNear);
        if (theRatio < 0.5f)
            theRatio = 1.0f - (0.5f - theRatio);
        else
            theRatio = theRatio + theRatio;
        float theSize = (kCameraOrthographicWidth * theRatio) * kCameraOrthographicScale;
        theRectangle.SetLeft(-theSize);
        theRectangle.SetTop(-theSize);
        theRectangle.SetRight(theSize);
        theRectangle.SetBottom(theSize);
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
