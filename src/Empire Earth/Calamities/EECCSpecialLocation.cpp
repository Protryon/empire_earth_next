// C:\EEX\Empire Earth\Calamities\EECCSpecialLocation.cpp (file name inferred: no __FILE__). The special-location
// calamity class: 0xb338d..0xb3494. Unit flags: the exe's module set. Start (0xb3257), Stop/Reset (0x157269) and
// Initialize (0xafea3) are ICF-folded.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/FSRestorable.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EECCSpecialLocation.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"

// New (port): EECCSpecialLocation's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCSpecialLocation::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCSpecialLocation* theClass = static_cast<EECCSpecialLocation*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::SpecialLocation: {
        EEComplexUnit* theUnit = theCalamity->mCasterUnit->mComplexUnit;
        GE3DPoint theLocation;
        theLocation = theCalamity->mTargetLocation;
        EECalamityCaptureEvent(kCalamityOpSpecialLocation);   // CALW (TEST ONLY)
        theUnit->ClearGoals(0);
        theUnit->SetGoal(EEUGRust::NewLocation(&theLocation, 1, 1, 0, 0.0f, 0, 0), 0, 0);
        theUnit->StartAction(kActionSpecialLocation);
        theUnit->mUnitClass->ActionChanged(theUnit);
        return true;
    }
    default:
        return false;
    }
}

// FUNCTION: EXE 0x4b3443
// Checks the base cast rules, counting status 2 as allowed
long EECCSpecialLocation::CanCastIgnoringAreaEffects(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX,
                                    unsigned long inY, bool inCheck, EEUnit* inTargetUnit)
{
    long theResult = EECalamityClass::CanCastAt(inUnit, inLocation, inX, inY, inCheck, inTargetUnit);
    if (theResult == 2)
        theResult = 0;
    return theResult;
}
