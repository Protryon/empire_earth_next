// C:\EEX\Empire Earth\Calamities\EECCAEMindControl.cpp (__FILE__ 0x4b9aa0, EECCAEMindControl::Stop's ESVERIFY).
// The mind-control area-effect calamity class: 0xaef4c..0xaf1ce. Unit flags: the exe's module set.
// Reset and Update are copies of EECCAEExtraDamage's: /OPT:ICF folded them into that unit's (0xaee30, 0xaee45), so
// this unit's range ends with Initialize.
// Data (.rdata, in order): kNoEffectHandle 0x43c78c, kCalamitySoundFalloff 0x43c790, the vtable 0x43c798 (COL
// 0x43c794). (.data): type descriptor 0x4b9a88, __FILE__ 0x4b9aa0; the assert text is EECCAEExtraDamage.cpp's
// (string pooling).
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
#include "ee/EECCAEMindControl.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEMap.h"
#include "ee/EESoundObject.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// New (port): EECCAEMindControl's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCAEMindControl::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCAEMindControl* theClass = static_cast<EECCAEMindControl*>(ioHost.mClass);
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

// ICF-FOLDED: EXE 0x418aa9
bool EECCAEMindControl::AffectsOwnUnits()
{
    return true;
}
