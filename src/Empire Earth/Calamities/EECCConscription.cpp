// C:\EEX\Empire Earth\Calamities\EECCConscription.cpp (file name inferred: no __FILE__). The conscription calamity
// class: 0xafdd3..0xafef9. Unit flags: the exe's module set. Stop and Reset are ICF-folded (0x157269).
// Data (.rdata): the vtable 0x43c8ac (COL 0x43c8a8).
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
#include "ee/EECCConscription.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"

// New (port): EECCConscription's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCConscription::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCConscription* theClass = static_cast<EECCConscription*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        EEUnit* theCaster = theCalamity->mCasterUnit;
        EETechnology* theTechnology = theCaster->mPlayer->mTechnologies->Get(kTechConscriptionSpell);
        outReply.ok = theCaster->mPlayer->CanAfford(theTechnology, theCaster->mUnitClass->GetUnitType(), true);
        return true;
    }
    case 2: {
        EEUnit* theCaster = theCalamity->mCasterUnit;
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        EEComplexUnit* theTarget = theTargetUnit->mComplexUnit;
        EETechnology* theTechnology = theCaster->mPlayer->mTechnologies->Get(kTechConscriptionSpell);
        theCaster->mPlayer->Pay(theTechnology, theCaster->mUnitClass->GetUnitType(), 1.0f);
        theTarget->PlayEffect(0x138d);
        EEPlayUnitEffect(theTarget, 0x138d);
        return true;
    }
    default:
        return false;
    }
}
