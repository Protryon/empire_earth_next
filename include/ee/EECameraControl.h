// C:\EEX\Empire Earth\Empire Earth\EECameraControl.h (RECONSTRUCTION; file name inferred). The game camera (RTTI
// EECameraControl, vtable 0x441ec0; EECameraControl.cpp around 0x127654..). The
// world holds one at gWorld+0x9d0 (0x531710). Only what the UI events need; provisional names (by behaviour).
#ifndef EE_EECAMERACONTROL_H
#define EE_EECAMERACONTROL_H

#include <vector>
#include "lle/GETransformation.h"
#include "lle/GE3DPoint.h"
#include "lle/GEQuaternion.h"
#include "ee/EEUnitHandle.h"

class GE3DPoint;
class EEUnit;
class EECameraKey;                          // 0xa4-byte camera keys (provisional name; also EEScenarioManager+8)

class EECameraControl {
public:
    virtual ~EECameraControl();             // slot 0 (0x12779f)
    // 0x128129: the location inStep of the way from +0x40 to +0x78 (the earthquake passes mFraction and shakes the
    // result; provisional name)
    void GetLocation(float inStep, GE3DPoint& outLocation);
    // 0x128996: the camera follows inUnitA/inUnitB (EEACenterCamera::Virtual4 passes the unit twice, (-10000, 0, 0),
    // false and zeros; provisional name and argument meanings)
    void MoveTo(EEUnit* inUnitA, GE3DPoint inLocationA, bool inFlagA, float inDuration, EEUnit* inUnitB,
                  GE3DPoint inLocationB, bool inFlagB, float inZoom, float inTurnYaw, float inTurnPitch);
    void Update(float inElapsedTime);       // 0x129a34 (EEMLoadMainScreen passes 1.0; provisional name)
    // inline (the grab scroll copies it to a temporary before its GetValue call, 0x1d9a5c)
    float GetZoom() const { return this->mZoom; }
    void ResetZoom();                       // 0x12934f: SetZoom(1.0f)
    void Revolve(const GE3DPoint& inAngles);    // 0x128722 (the mouse revolve event; provisional name)
    void Rotate(const GE3DPoint& inAngles);     // 0x12887d (the mouse rotate event; provisional name)
    // 0x129844: moves the camera to inLocation (copied); the hot-key unit selection passes (theCenter, false, true)
    void SetLocation(const GE3DPoint& inLocation, bool inIsImmediate, bool inFollowTerrain);
    void SetMode(long inMode);              // 0x1299c0: mMode = inMode, then SetZoom(mZoom)
    void SetChanged();                      // 0x1299dc: if (!mLocked) mChanged = true
    void ClearUnitA();                      // 0x1299ed: dereferences and clears mUnitA (provisional name)
    void ClearUnitB();                      // 0x129a04: the same for mUnitB, then resets mIsFollowingUnitB/mF0C4.. (provisional name)
    // 0x129dfe (ret 0x14; EEPhysics 0x153474 pushes (unit, (byte) mIsFollowingUnitB, 1.0f, mTurnYaw, mTurnPitch))
    // (provisional name)
    void Follow(EEUnit* inUnit, bool inFlag, float inDuration, float inTurnYaw, float inTurnPitch);
    void Scroll(float inDX, float inDY);    // 0x128ea5 (the edge-scroll action EEAScroll; provisional name)
    void SetZoom(float inZoom);             // 0x129eb2: 0 (near) .. 1 (far)
    // EEScenarioManager::EndCinematic 0x1b1ae4 restores the camera from its saved EECameraKey with these (provisional
    // names by address; arguments from `ret 8` and the pushes)
    void ResetPath();                       // 0x1277c9
    // GetLocation's siblings (`ret 8`; StartCinematic 0x1b1eee saves the camera with mFraction as the step)
    void GetOffset(float inStep, GE3DPoint& outPoint);  // 0x12802b
    void GetViewTurn(float inStep, GETransformation& outTransformation);  // 0x128089
    void GetOffsetTurn(float inStep, GETransformation& outTransformation);  // 0x1280e7
    void SetOffset(const GE3DPoint& inPoint, bool inFlag);  // 0x129547 (key +0x80)
    void SetViewTurn(const GETransformation& inTransformation, bool inFlag);  // 0x1295fd (key +0x00)
    void SetOffsetTurn(const GETransformation& inTransformation, bool inFlag);  // 0x1296b6 (key +0x40)
    // the rest of the camera unit (provisional names)
    EECameraControl();                      // 0x127654
    long AddKey(const EECameraKey& inKey);      // 0x1277e5: appends a key unless locked (5)
    void ClampHeight(GETransformation& ioTransformation);  // 0x127804: keeps the camera above the terrain
    void SetClippingDistances();            // 0x1279f0: the viewport's clipping distances
    void CopyKey(const EECameraKey& inKey, EECameraKey& outKey);    // 0x127db3
    void NextKey();                         // 0x127e5b: the next key, or the end of the key list
    void BuildView(GETransformation& outTransformation, float inStep);  // 0x127eb1
    void SlerpViewTurn(float inStep, GEQuaternion& outQuaternion);  // 0x12806d
    void SlerpOffsetTurn(float inStep, GEQuaternion& outQuaternion);  // 0x1280cb
    void GetGroundPoint(const GE3DLine& inLine, GE3DPoint& outPoint);  // 0x12816e
    void ClipToEdges(const GE3DLine& inLine, GE3DPoint& ioPoint);  // 0x1282c3: clips to the map's edge planes
    void GetUnitPoint(EEUnit* inUnit, GE3DPoint& outPoint);  // 0x12844f: the point to look at on a unit
    void GetUnitTurn(EEUnit* inUnit, GETransformation& outTransformation);  // 0x12851e
    static float GetPitch(float inZoom, long inMode);   // 0x1285ec (cdecl): the pitch for a zoom
    void StartPath();                       // 0x1286fa: plays the key list
    void GoTo(const EECameraKey& inKey);        // 0x12935b: moves towards a key
    void KeepPose();                        // 0x12976f: the current state becomes the start

    // Layout (no gaps; sizeof 0x100): the ctor 0x127654 and the dtor 0x1277bb; floats by their
    // fld/fstp stores; member names are provisional unless stated.
    std::vector<EECameraKey> mKeys;         // +0x004 (0xa4-byte POD elements: `_Buy` 0x12a081; provisional name)
    GEQuaternion mQ014;                     // +0x014 (the start orientations: Slerp to +0x4c/+0x5c)
    GEQuaternion mQ024;                     // +0x024
    GE3DPoint mStartOffset;                 // +0x034 (the start points: lerped to +0x6c/+0x78)
    GE3DPoint mStartFocus;                  // +0x040
    GEQuaternion mQ04C;                     // +0x04c (the target key's orientations, ConvertFrom its transformations)
    GEQuaternion mQ05C;                     // +0x05c
    GE3DPoint mEndOffset;                   // +0x06c (the target key's points)
    GE3DPoint mEndFocus;                    // +0x078
    GETransformation mView;                 // +0x084
    GE3DPoint mTargetPoint;                 // +0x0c4 (x = -10000: none; ClearUnitB resets it)
    EEUnitHandle<EEUnit> mUnitA;            // +0x0d0 (referenced; ClearUnitA; MoveTo's inUnitA; provisional name; a handle since R5a.4: read on the graphics and physics threads)
    EEUnitHandle<EEUnit> mUnitB;            // +0x0d4 (referenced; ClearUnitB; a handle)
    float mFraction;                        // +0x0d8
    float mRate;                            // +0x0dc
    float mZoom;                            // +0x0e0 (SetZoom stores it; the wheel zoom reads it)
    float mWait;                            // +0x0e4 (untouched by the ctor; the key's +0x98: a hold time)
    float mWaited;                          // +0x0e8
    float mTurnYaw;                         // +0x0ec
    float mTurnPitch;                       // +0x0f0
    long mMode;                             // +0x0f4 (3: the wheel and hot-key zoom first switch it back to 0)
    long mPathIndex;                        // +0x0f8
    bool mChanged;                          // +0x0fc
    bool mIsFollowingUnitB;                 // +0x0fd
    bool mLocked;                           // +0x0fe
    bool mB0FF;                             // +0x0ff (untouched by the ctor)
};

#endif
