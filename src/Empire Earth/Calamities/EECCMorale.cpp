// C:\EEX\Empire Earth\Calamities\EECCMorale.cpp (file name inferred: no __FILE__). The morale calamity class:
// 0xb2ba3..0xb2f31. Unit flags: the exe's module set. Reset is ICF-folded (0xb1c06).
// Data (.rdata, in order): kNoEffectHandle 0x43ccdc, -10000.0f 0x43cce0, the vtable 0x43cce8 (COL 0x43cce4).
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
#include "ee/EEUnitCalamityClass.h"
#include "ee/EECCMorale.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"

// New (port): EECCMorale's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCMorale::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCMorale* theClass = static_cast<EECCMorale*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::Morale: {
        EECalamityCaptureEvent(kCalamityOpMorale);   // CALW (TEST ONLY)
        EEUnit* theTargetUnit;
        theCalamity->GetTargetUnit(theTargetUnit);
        EEGraphicEffect* theEffect = theTargetUnit->mUnitClass->mMoraleEffect;
        if (theEffect != NULL) {
            EEEffectHandle theEffectHandle;
            gEffectManager->CreateUnitEffect(theTargetUnit, theEffect, &theEffectHandle, 0, 0, 0);
            EEEffectHandle theOldHandle = theCalamity->mEffectHandle;
            if (theOldHandle != kNoEffectHandle) {
                gEffectManager->DestroyEffect(theOldHandle);
                theCalamity->mEffectHandle = kNoEffectHandle;
            }
            theCalamity->mEffectHandle = theEffectHandle;
        }
        if (theClass->mPositive)
            theTargetUnit->mComplexUnit->mPositiveMorale++;
        else
            theTargetUnit->mComplexUnit->mNegativeMorale++;
        if (theClass->mSecondarySound != NULL)
            gSoundManager->Play(theClass->mSecondarySound->GetSampleID(), theClass->mSecondarySound->GetVolume(), 0.0f, 1.0f,
                                theClass->mSecondarySound->GetLooping(), NULL);
        return true;
    }
    default:
        return false;
    }
}

// New (port): EECCMorale's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCMorale::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCMorale* theClass = static_cast<EECCMorale*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        EEEffectHandle theEffectHandle;
        gEffectManager->CreateEffectBetweenUnits(theCalamity->mCasterUnit, theTargetUnit->mComplexUnit, theClass->mGraphicEffect1,
                                                 &theEffectHandle);
        outReply.id = theEffectHandle.mValue;
        return true;
    }
    case 2:
    case 3: {
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        if (inCall.index == 2)
            theTargetUnit->mComplexUnit->mPositiveMorale--;
        else
            theTargetUnit->mComplexUnit->mNegativeMorale--;
        return true;
    }
    default:
        return false;
    }
}
