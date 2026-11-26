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
#include "rust/game/app.h"
#include "rust/game/gamecpp.h"
#include "rust/base/math.h"
#include "rust/render/gfx.h"

class GE3DPoint;
class EEUnit;
class EECameraKey;                          // 0xa4-byte camera keys (provisional name; also EEScenarioManager+8)
// the camera's Rust mirrors of the GE value classes (they share their storage: EECameraControl.cpp's static_assert),
// its followed units' ids and its host (ee_app camera_ffi.rs; the context is the camera)
template <class R, class T> inline const R* EECameraRust(const T& inValue) { return reinterpret_cast<const R*>(&inValue); }
template <class R, class T> inline R* EECameraRust(T& inValue) { return reinterpret_cast<R*>(&inValue); }
inline uint32_t EECameraID(EEUnit* inUnit) { return inUnit != NULL ? EEUnitHandleID(inUnit) : (uint32_t)EE_APP_CAMERA_NO_UNIT; }
#define EE_CAMERA_HOST this, this, ee_gamecpp_camera_host

// The camera is whole in Rust (rust/crates/ee_game/src/app camera.rs): the plain members below the methods are the record
// EeAppCameraFields (rust/ee_app.h) the class derives from, in the original's order and names (mQ014, mQ024 the
// start turns, mStartOffset, mStartFocus, mQ04C, mQ05C the end turns, mEndOffset, mEndFocus, mView, mTargetPoint (x =
// -10000: none), mFraction, mRate, mZoom, mWait, mWaited, mTurnYaw, mTurnPitch, mMode (3: free), mPathIndex, mChanged,
// mIsFollowingUnitB, mLocked, mB0FF); the GE values are their Rust mirrors (the same storage). Every body is Rust over
// the host in EECameraControl.cpp; the key list and the followed units' handles stay here.
class EECameraControl : public EeAppCameraFields {
public:
    virtual ~EECameraControl();             // slot 0 (0x12779f)
    // 0x128129: the location inStep of the way from +0x40 to +0x78 (the earthquake passes mFraction and shakes the
    // result; provisional name)
    void GetLocation(float inStep, GE3DPoint& outLocation) { ee_app_camera_point(this, 1, inStep, EECameraRust<EeGe3DPoint>(outLocation)); }
    // 0x128996: the camera follows inUnitA/inUnitB (EEACenterCamera::Virtual4 passes the unit twice, (-10000, 0, 0),
    // false and zeros; provisional name and argument meanings)
    void MoveTo(EEUnit* inUnitA, GE3DPoint inLocationA, bool inFlagA, float inDuration, EEUnit* inUnitB,
                  GE3DPoint inLocationB, bool inFlagB, float inZoom, float inTurnYaw, float inTurnPitch);
    // 0x129a34 (EEMLoadMainScreen passes 1.0; provisional name)
    void Update(float inElapsedTime) { ee_gamecpp_camera_update(this, inElapsedTime); }
    // inline (the grab scroll copies it to a temporary before its GetValue call, 0x1d9a5c)
    float GetZoom() const { return this->mZoom; }
    // port: the end offset and focus as the C++ value class (the scenario's camera keys, the test fixture)
    const GE3DPoint& GetEndOffset() const { return reinterpret_cast<const GE3DPoint&>(this->mEndOffset); }
    const GE3DPoint& GetEndFocus() const { return reinterpret_cast<const GE3DPoint&>(this->mEndFocus); }
    // 0x12934f: SetZoom(1.0f)
    void ResetZoom() { this->SetZoom(1.0f); }
    // 0x128722 (the mouse revolve event; provisional name)
    void Revolve(const GE3DPoint& inAngles) { ee_app_camera_revolve(EE_CAMERA_HOST, inAngles.GetX(), inAngles.GetY()); }
    // 0x12887d (the mouse rotate event; provisional name)
    void Rotate(const GE3DPoint& inAngles) { ee_app_camera_rotate(EE_CAMERA_HOST, inAngles.GetX(), inAngles.GetY()); }
    // 0x129844: moves the camera to inLocation (copied); the hot-key unit selection passes (theCenter, false, true)
    void SetLocation(const GE3DPoint& inLocation, bool inIsImmediate, bool inFollowTerrain) { ee_app_camera_set_location(EE_CAMERA_HOST, EECameraRust<EeGe3DPoint>(inLocation), inIsImmediate, inFollowTerrain); }
    // 0x1299c0: mMode = inMode, then SetZoom(mZoom)
    void SetMode(long inMode) { ee_app_camera_set_mode(EE_CAMERA_HOST, inMode); }
    // 0x1299dc: if (!mLocked) mChanged = true
    void SetChanged() { ee_app_camera_set_changed(this); }
    // 0x1299ed: dereferences and clears mUnitA (provisional name)
    void ClearUnitA() { ee_app_camera_clear_unit_a(EE_CAMERA_HOST); }
    // 0x129a04: the same for mUnitB, then resets mIsFollowingUnitB/mF0C4.. (provisional name)
    void ClearUnitB() { ee_app_camera_clear_unit_b(EE_CAMERA_HOST); }
    // 0x129dfe (ret 0x14; EEPhysics 0x153474 pushes (unit, (byte) mIsFollowingUnitB, 1.0f, mTurnYaw, mTurnPitch))
    // (provisional name)
    void Follow(EEUnit* inUnit, bool inFlag, float inDuration, float inTurnYaw, float inTurnPitch) { ee_app_camera_follow(EE_CAMERA_HOST, EECameraID(inUnit), inFlag, inDuration, inTurnYaw, inTurnPitch); }
    // 0x128ea5 (the edge-scroll action EEAScroll; provisional name)
    void Scroll(float inDX, float inDY) { ee_app_camera_scroll(EE_CAMERA_HOST, inDX, inDY); }
    // 0x129eb2: 0 (near) .. 1 (far)
    void SetZoom(float inZoom) { ee_app_camera_set_zoom(EE_CAMERA_HOST, inZoom); }
    // EEScenarioManager::EndCinematic 0x1b1ae4 restores the camera from its saved EECameraKey with these (provisional
    // names by address; arguments from `ret 8` and the pushes)
    void ResetPath() { ee_app_camera_reset_path(EE_CAMERA_HOST); }
    // GetLocation's siblings (`ret 8`; StartCinematic 0x1b1eee saves the camera with mFraction as the step)
    void GetOffset(float inStep, GE3DPoint& outPoint) { ee_app_camera_point(this, 0, inStep, EECameraRust<EeGe3DPoint>(outPoint)); }
    void GetViewTurn(float inStep, GETransformation& outTransformation) { ee_app_camera_turn(this, 0, inStep, EECameraRust<EeGeTransformation>(outTransformation)); }
    void GetOffsetTurn(float inStep, GETransformation& outTransformation) { ee_app_camera_turn(this, 1, inStep, EECameraRust<EeGeTransformation>(outTransformation)); }
    // 0x129547 (key +0x80)
    void SetOffset(const GE3DPoint& inPoint, bool inFlag) { ee_app_camera_set_offset(EE_CAMERA_HOST, EECameraRust<EeGe3DPoint>(inPoint), inFlag); }
    // 0x1295fd (key +0x00)
    void SetViewTurn(const GETransformation& inTransformation, bool inFlag) { ee_app_camera_set_view_turn(EE_CAMERA_HOST, EECameraRust<EeGeTransformation>(inTransformation), inFlag); }
    // 0x1296b6 (key +0x40)
    void SetOffsetTurn(const GETransformation& inTransformation, bool inFlag) { ee_app_camera_set_offset_turn(EE_CAMERA_HOST, EECameraRust<EeGeTransformation>(inTransformation), inFlag); }
    // the rest of the camera unit (provisional names)
    EECameraControl();
    // 0x1277e5: appends a key unless locked (5)
    long AddKey(const EECameraKey& inKey) { return ee_gamecpp_camera_add_key(this, &inKey); }
    // 0x12844f: the point to look at on a unit
    void GetUnitPoint(EEUnit* inUnit, GE3DPoint& outPoint) { ee_gamecpp_camera_unit_point(inUnit, &outPoint); }
    void GetUnitTurn(EEUnit* inUnit, GETransformation& outTransformation) { ee_app_camera_unit_turn(EE_CAMERA_HOST, EECameraID(inUnit), EECameraRust<EeGeTransformation>(outTransformation)); }
    // 0x1286fa: plays the key list
    void StartPath() { ee_app_camera_start_path(EE_CAMERA_HOST); }

    // +0x004 the path keys (std::vector<EECameraKey>, 0xa4-byte POD elements): Rust's (gamecpp camera_io.rs KEYS)
    EEUnitHandle<EEUnit> mUnitA;            // +0x0d0 (referenced; ClearUnitA; MoveTo's inUnitA; provisional name; a handle since R5a.4: read on the graphics and physics threads)
    EEUnitHandle<EEUnit> mUnitB;            // +0x0d4 (referenced; ClearUnitB; a handle)
};

#endif
