// C:\EEX\Empire Earth\Calamities\EECCFire.cpp (__FILE__ 0x4b9ba8, EECCFire::Stop's ESVERIFY). The fire calamity
// class: 0xb0e2e..0xb114a. Unit flags: the exe's module set. GetContagionFunctor is ICF-folded (0x12c730).
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
#include "ee/EECalamityUpdate.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EECCFire.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// New (port): EECCFire's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCFire::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCFire* theClass = static_cast<EECCFire*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        gEffectManager->CreateUnitEffect(theTargetUnit, theClass->mGraphicEffect1, NULL, 0, 0, 0);
        return true;
    }
    default:
        return false;
    }
}
