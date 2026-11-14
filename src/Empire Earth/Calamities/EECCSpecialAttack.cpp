// C:\EEX\Empire Earth\Calamities\EECCSpecialAttack.cpp (file name inferred: no __FILE__). The special-attack calamity
// class: 0xb3217..0xb338d. Unit flags: the exe's module set. Stop/Reset (0x157269) and Initialize (0xafea3) are
// ICF-folded.
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
#include "ee/EEUnitCalamityClass.h"
#include "ee/EECCSpecialAttack.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// New (port): EECCSpecialAttack's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCSpecialAttack::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCSpecialAttack* theClass = static_cast<EECCSpecialAttack*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::SpecialAttack: {
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        EEComplexUnit* theUnit = theCalamity->mCasterUnit->mComplexUnit;
        EECalamityCaptureEvent(kCalamityOpSpecialAttack, theTargetUnit->mUnitID);   // CALW (TEST ONLY)
        theUnit->ClearGoals(0);
        theUnit->SetGoal(EEUGRust::NewAttackUnit(theUnit, theTargetUnit, 1, 0), 0, 0);
        theUnit->mIsSpecialAttack = true;
        theUnit->StartAction(kActionAttackEnemyUnit);
        theUnit->mUnitClass->ActionChanged(theUnit);
        return true;
    }
    default:
        return false;
    }
}

// FUNCTION: EXE 0x4b32da
// Checks the base cast rules (status 2 counts as allowed), then returns 1 unless the target unit is a valid target
long EECCSpecialAttack::CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                                  bool inCheck, EEUnit* inTargetUnit)
{
    // the rules are Rust (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
    EECalamityCanCastScope theCapture;   // CCAS (TEST ONLY)
    return theCapture.Record(this, inUnit, inLocation, inX, inY, inCheck, inTargetUnit,
                             EECalamityCanCastAt(this, EeCalamityCastMode::SpecialAttack, inUnit, inX, inY, inCheck, inTargetUnit));
}
