// C:\EEX\Empire Earth\Calamities\EELocationCalamityClass.cpp (file name inferred: no __FILE__; sorts between
// EELeprosyFunctor.cpp and EEPlagueFunctor.cpp). The calamity classes cast at a location: 0xb6f21..0xb6fb1. Unit
// flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/FSRestorable.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"

// FUNCTION: EXE 0x4b6f21
// Creates a calamity of the class at the location for the caster and resets it for its start
EECalamity* EELocationCalamityClass::CreateCalamity(GE3DPoint& inLocation, EEUnit* inCasterUnit)
{
    EECalamity* theCalamity = new EECalamity(this, inCasterUnit);
    theCalamity->mTargetLocation = inLocation;
    this->Reset(theCalamity);
    return theCalamity;
}

// FUNCTION: EXE 0x4b6f6a
// Returns 2: cast at a location
long EELocationCalamityClass::GetTargetKind()
{
    return 2;
}

// FUNCTION: EXE 0x4b6f6e
// Checks the base cast rules, then refuses (1) a tile that is not a valid location for the calamity
long EELocationCalamityClass::CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                                        bool inCheck, EEUnit* inTargetUnit)
{
    EECalamityCanCastScope theCapture;   // CCAS (TEST ONLY)
    long theResult = EECalamityClass::CanCastAt(inUnit, inLocation, inX, inY, inCheck, inTargetUnit);
    if (theResult == 0 && !this->IsValidLocation(inUnit, inX, inY))
        theResult = 1;
    return theCapture.Record(this, inUnit, inLocation, inX, inY, inCheck, inTargetUnit, theResult);
}
