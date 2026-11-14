// C:\EEX\Empire Earth\Calamities\EECCDropObject.cpp (file name inferred: no __FILE__). The drop-object calamity class:
// 0xafef9..0xb037f (its members, with the three EEComplexUnit inline members it emits first after Stop). Unit flags:
// the exe's module set. Reset, Virtual28 and Virtual34 are ICF-folded (0x157269, the area effects' 0xaf5f2/0xaf632).
// Data (.rdata, in order): kNoEffectHandle 0x43c8fc, -10000.0f 0x43c900, kCalamitySoundFalloff 0x43c904, the vtable
// 0x43c90c (COL 0x43c908).
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
#include "ee/EECCDropObject.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EETechCapture.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitSetManager.h"

// Compiler-generated for this unit (EEComplexUnit inline members, first emitted here):
// TEMPLATE: EXE 0x4b01ad
// TEMPLATE: EXE 0x4b01d2
// TEMPLATE: EXE 0x4b021c

// New (port): EECCDropObject's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCDropObject::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCDropObject* theClass = static_cast<EECCDropObject*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        // the dropped unit next to the target, attacking it
        EEUnit* theTargetUnit = NULL;
        EEComplexUnit* theCaster = theCalamity->mCasterUnit->mComplexUnit;
        theCalamity->GetTargetUnit(theTargetUnit);
        GE3DPoint theLocation;
        EEFindDropLocation(theCaster, &theTargetUnit, &theLocation, theTargetUnit);
        long theX = UMFloor(theLocation.GetX());
        long theY = UMFloor(theLocation.GetY());
        EEUnitClass* theUnitClass = gUnitClassTable[theClass->mUnitClassIndex];
        EEUnit* theUnit = theUnitClass->CreateUnit(theX, theY, theCaster->mPlayer->mPlayerID, 0, theCaster);
        theUnit->mComplexUnit->SetLauncherClass(theUnitClass);
        theUnit->mComplexUnit->SetOwner(theUnit->mComplexUnit);
        theUnit->mComplexUnit->SetGoal(EEUGRust::NewAttackUnit(theUnit->GetComplexUnit(), theTargetUnit, 0, 0), 0, 0);
        theUnit->mComplexUnit->SetVelocity(0.0f);
        theUnit->mComplexUnit->SetProjectileStart(theLocation);
        theUnit->mComplexUnit->SetHeight(theLocation.GetZ());
        return true;
    }
    case 2: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        EEEffectHandle theEffectHandle;
        EECalamityCaptureStartPoint(1, theLocation.mX, theLocation.mY, theLocation.mZ);   // CALT (TEST ONLY)
        gEffectManager->CreateEffect(theCalamity->mCasterUnit, theLocation, theClass->mGraphicEffect1, &theEffectHandle, 0);
        return true;
    }
    default:
        return false;
    }
}
