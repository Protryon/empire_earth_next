// C:\EEX\Empire Earth\Empire Earth\EECameraControl.h (RECONSTRUCTION; file name inferred). The game camera (RTTI
// EECameraControl, vtable 0x441ec0; EECameraControl.cpp around 0x127654..). The
// world holds one at gWorld+0x9d0 (0x531710). Only what the UI events need; provisional names (by behaviour).
#ifndef EE_EECAMERACONTROL_H
#define EE_EECAMERACONTROL_H

#include <vector>
#include "lle/GEQuaternion.h"
#include "ee/EEUnitHandle.h"
#include "rust/game/records.h"

class GE3DPoint;
class EEUnit;
class EECameraKey;                          // 0xa4-byte camera keys (provisional name; also EEScenarioManager+8)

// The camera is whole in Rust (rust/crates/ee_game/src/app camera.rs): the plain members below the methods are the record
// EeAppCameraFields (rust/ee_app.h) the class derives from, in the original's order and names (mQ014, mQ024 the
// start turns, mStartOffset, mStartFocus, mQ04C, mQ05C the end turns, mEndOffset, mEndFocus, mView, mTargetPoint (x =
// -10000: none), mFraction, mRate, mZoom, mWait, mWaited, mTurnYaw, mTurnPitch, mMode (3: free), mPathIndex, mChanged,
// mIsFollowingUnitB, mLocked, mB0FF); the GE values are their Rust mirrors (the same storage). Every body is Rust over
// the host in EECameraControl.cpp; the key list and the followed units' ids are Rust's too (gamecpp camera_io.rs).
class EECameraControl : public EeAppCameraFields {
public:
    virtual ~EECameraControl();             // slot 0 (0x12779f)
    // 0x128129: the location inStep of the way from +0x40 to +0x78 (the earthquake passes mFraction and shakes the
    // result; provisional name)
    void GetLocation(float inStep, GE3DPoint& outLocation);
    // 0x128996: the camera follows inUnitA/inUnitB (EEACenterCamera::Virtual4 passes the unit twice, (-10000, 0, 0),
    // false and zeros; provisional name and argument meanings)
    void MoveTo(EEUnit* inUnitA, GE3DPoint inLocationA, bool inFlagA, float inDuration, EEUnit* inUnitB,
                  GE3DPoint inLocationB, bool inFlagB, float inZoom, float inTurnYaw, float inTurnPitch);
    // 0x129a34 (EEMLoadMainScreen passes 1.0; provisional name)
    void Update(float inElapsedTime);
    // inline (the grab scroll copies it to a temporary before its GetValue call, 0x1d9a5c)
    float GetZoom() const { return this->mZoom; }
    // port: the end offset as the C++ value class (the scenario's camera keys, the test fixture)
    const GE3DPoint& GetEndOffset() const { return reinterpret_cast<const GE3DPoint&>(this->mEndOffset); }
    // 0x12934f: SetZoom(1.0f)
    void ResetZoom() { this->SetZoom(1.0f); }
    // 0x128722 (the mouse revolve event; provisional name)
    void Revolve(const GE3DPoint& inAngles);
    // 0x12887d (the mouse rotate event; provisional name)
    void Rotate(const GE3DPoint& inAngles);
    // 0x129844: moves the camera to inLocation (copied); the hot-key unit selection passes (theCenter, false, true)
    void SetLocation(const GE3DPoint& inLocation, bool inIsImmediate, bool inFollowTerrain);
    // 0x1299c0: mMode = inMode, then SetZoom(mZoom)
    void SetMode(long inMode);
    // 0x1299ed: dereferences and clears mUnitA (provisional name)
    void ClearUnitA();
    // 0x129a04: the same for mUnitB, then resets mIsFollowingUnitB/mF0C4.. (provisional name)
    void ClearUnitB();
    // 0x128ea5 (the edge-scroll action EEAScroll; provisional name)
    void Scroll(float inDX, float inDY);
    // 0x129eb2: 0 (near) .. 1 (far)
    void SetZoom(float inZoom);
    // EEScenarioManager::EndCinematic 0x1b1ae4 restores the camera from its saved EECameraKey with these (provisional
    // names by address; arguments from `ret 8` and the pushes)
    void ResetPath();
    // GetLocation's siblings (`ret 8`; StartCinematic 0x1b1eee saves the camera with mFraction as the step)
    void GetOffset(float inStep, GE3DPoint& outPoint);
    void GetViewTurn(float inStep, GETransformation& outTransformation);
    void GetOffsetTurn(float inStep, GETransformation& outTransformation);
    // 0x129547 (key +0x80)
    void SetOffset(const GE3DPoint& inPoint, bool inFlag);
    // 0x1295fd (key +0x00)
    void SetViewTurn(const GETransformation& inTransformation, bool inFlag);
    // 0x1296b6 (key +0x40)
    void SetOffsetTurn(const GETransformation& inTransformation, bool inFlag);
    // the rest of the camera unit (provisional names)
    EECameraControl();
    void GetUnitTurn(EEUnit* inUnit, GETransformation& outTransformation);
    // 0x1286fa: plays the key list
    void StartPath();

    // +0x004 the path keys (std::vector<EECameraKey>, 0xa4-byte POD elements): Rust's (gamecpp camera_io.rs KEYS)
    // +0x0d0, +0x0d4 the followed units (mUnitA, mUnitB: referenced; by id): Rust's (gamecpp camera_io.rs UNITS)
};

#endif
