// New (port), TEST ONLY: the app and world lane's records of the simulation capture (ee/EEAppCapture.h;
// tools/sim_capture.py). Reads the objects; writes nothing to them.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEViewport.h"

#include "ee/EESimCapture.h"
#include "ee/EETechCapture.h"
#include "ee/EECameraKey.h"
#include "ee/EECameraControl.h"
#include "ee/EEAppCapture.h"

extern GEViewport gViewport;

namespace {

void Point(EESimCaptureRecord& ioRecord, const GE3DPoint& inPoint)
{
    ioRecord.F32(inPoint.GetX());
    ioRecord.F32(inPoint.GetY());
    ioRecord.F32(inPoint.GetZ());
}

void Floats(EESimCaptureRecord& ioRecord, const void* inValue, unsigned int inSize)
{
    ioRecord.F32s(static_cast<const float*>(inValue), inSize / 4);
}

}   // namespace

void EEAppCaptureCamera(const EECameraControl& inCamera, unsigned int inPhase, float inElapsedTime)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord r(EECaptureTag("CAMU"));
    r.U32(inPhase);
    r.F32(inElapsedTime);
    Floats(r, &inCamera.mQ014, sizeof(inCamera.mQ014));
    Floats(r, &inCamera.mQ024, sizeof(inCamera.mQ024));
    Floats(r, &inCamera.mStartOffset, sizeof(inCamera.mStartOffset));
    Floats(r, &inCamera.mStartFocus, sizeof(inCamera.mStartFocus));
    Floats(r, &inCamera.mQ04C, sizeof(inCamera.mQ04C));
    Floats(r, &inCamera.mQ05C, sizeof(inCamera.mQ05C));
    Floats(r, &inCamera.mEndOffset, sizeof(inCamera.mEndOffset));
    Floats(r, &inCamera.mEndFocus, sizeof(inCamera.mEndFocus));
    Floats(r, &inCamera.mView, sizeof(inCamera.mView));
    Floats(r, &inCamera.mTargetPoint, sizeof(inCamera.mTargetPoint));
    r.U32(inCamera.mUnitA.GetSavedID());
    r.U32(inCamera.mUnitB.GetSavedID());
    r.F32(inCamera.mFraction);
    r.F32(inCamera.mRate);
    r.F32(inCamera.mZoom);
    r.F32(inCamera.mWait);
    r.F32(inCamera.mWaited);
    r.F32(inCamera.mTurnYaw);
    r.F32(inCamera.mTurnPitch);
    r.I32((int)inCamera.mMode);
    r.I32((int)inCamera.mPathIndex);
    r.U32((inCamera.mChanged ? 1u : 0u) | (inCamera.mIsFollowingUnitB ? 2u : 0u) | (inCamera.mLocked ? 4u : 0u));
    r.U32((unsigned int)inCamera.mKeys.size());
    for (const EECameraKey& theKey : inCamera.mKeys) {
        Floats(r, &theKey.mT00, sizeof(theKey.mT00));
        Floats(r, &theKey.mT40, sizeof(theKey.mT40));
        Point(r, theKey.mOffset);
        Point(r, theKey.mFocus);
        r.F32(theKey.mWait);
        r.F32(theKey.mRate);
        r.U32(theKey.mChanged ? 1u : 0u);
    }
    r.F32(gViewport.GetFrontClippingDistance());
    r.F32(gViewport.GetRearClippingDistance());
    r.Commit();
}
