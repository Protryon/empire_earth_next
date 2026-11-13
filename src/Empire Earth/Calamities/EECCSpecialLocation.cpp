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

// FUNCTION: EXE 0x4b338d
// Creates the special-location calamity class
EECCSpecialLocation::EECCSpecialLocation()
{
}

// SYNTHETIC: EXE 0x4b339f
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b33bb
// Destroys the calamity class
EECCSpecialLocation::~EECCSpecialLocation()
{
}

// ICF-FOLDED: EXE 0x4b3257
// (`return true`: identical to EECCSpecialAttack::Start)
bool EECCSpecialLocation::Start(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, NULL, NULL);
    return EECalamityRunStart(this, inCalamity, theParams, NULL, NULL, NULL);
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCSpecialLocation::Stop(EECalamity* inCalamity)
{
}

// ICF-FOLDED: EXE 0x4afea3
// (identical to EECCConscription::Initialize)
void EECCSpecialLocation::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCSpecialLocation::Reset(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4b33c6
// Orders the caster to the target location for its special-location action; always returns 7 (done)
long EECCSpecialLocation::Update(EECalamity* inCalamity, float inElapsedTime)
{
    // the update is Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h)
    EeCalamityParams theParams = EECalamityBaseParams(this, NULL, NULL);
    return EECalamityRunUpdate(this, inCalamity, inElapsedTime, theParams, NULL, NULL, &EECCSpecialLocation::UpdateStep);
}

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

// FUNCTION: EXE 0x4b3468
// Accepts any tile, except that a radio man needs a free tile
bool EECCSpecialLocation::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    // the rule is Rust (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
    EeCalamityCast theCast = EECalamityCastOf(this, inUnit, inX, inY);
    return EECalamityValidLocation(this, theCast, inUnit, 0);
}
