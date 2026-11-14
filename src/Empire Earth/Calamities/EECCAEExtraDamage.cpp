// C:\EEX\Empire Earth\Calamities\EECCAEExtraDamage.cpp (__FILE__ 0x4b99f8, EECCAEExtraDamage::Stop's ESVERIFY).
// The extra-damage area-effect calamity class: 0xaebb7..0xaef4c. Unit flags: the exe's module set (-O1 -G6 -MD -GR,
// docs/CALIBRATION.md "exe").
// Data (.rdata, this unit's contributions in order): kNoEffectHandle 0x43c728, kCalamitySoundFalloff 0x43c72c,
// the vtable 0x43c734 (COL 0x43c730), 15.0f 0x43c788. (.data): type descriptor 0x4b99e0, __FILE__, assert text 0x4b9a30.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EECCAreaEffect.h"
#include "ee/EECCAEExtraDamage.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEMap.h"
#include "ee/EESoundObject.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// New (port): EECCAEExtraDamage's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCAEExtraDamage::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCAEExtraDamage* theClass = static_cast<EECCAEExtraDamage*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::PlacedEffect: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        EEEffectHandle theEffectHandle;
        gEffectManager->CreatePlacedEffect(theLocation, theClass->mGraphicEffect2, &theEffectHandle);
        EECalamityCaptureEvent(kCalamityOpPlacedEffect, theEffectHandle.mValue, 0, theLocation.mX, theLocation.mZ);   // CALW (TEST ONLY)
        outReply.id = (uint32_t)theEffectHandle.mValue;
        return true;
    }
    case EeCalamityOp::ApplyEffect: {
        EECalamityCaptureEvent(kCalamityOpApplyEffect);   // CALW (TEST ONLY)
        theClass->ApplyEffect(theCalamity);
        return true;
    }
    default:
        return false;
    }
}
