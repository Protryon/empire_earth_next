// C:\EEX\Empire Earth\Calamities\EECCPlague.cpp (__FILE__ 0x4b9d04, EECCPlague::Stop's ESVERIFY). The plague
// calamity class: 0xb2f31..0xb3217. Unit flags: the exe's module set.
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
#include "ee/EECCPlague.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4b3131
// Shows the plague effect from the caster to the target: a disease effect between their positions or a tracer beam
// between the units
void EECCPlague::ShowEffect(EECalamity* inCalamity)
{
    GE3DPoint theCasterLocation;
    GE3DPoint theTargetLocation;
    EEUnit* theCaster = inCalamity->mCasterUnit;
    if (theCaster == NULL)
        return;
    EEUnit* theTarget;
    inCalamity->GetTargetUnit(theTarget);
    long theKind = this->mGraphicEffect1->mKind;
    if (theKind == kEffect3DDisease) {
        theCaster->GetPosition(theCasterLocation);
        theTarget->GetPosition(theTargetLocation);
        gEffectManager->CreateEffectBetween(theCasterLocation, theTargetLocation, this->mGraphicEffect1, NULL);
    } else if (theKind == kEffect3DTracerBeam)
        gEffectManager->CreateEffectBetweenUnits(theCaster, theTarget, this->mGraphicEffect1, NULL);
}

// New (port): EECCPlague's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCPlague::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCPlague* theClass = static_cast<EECCPlague*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1:
        theClass->ShowEffect(theCalamity);
        return true;
    default:
        return false;
    }
}
