// C:\EEX\Empire Earth\Calamities\EECCLineOfSight.cpp (file name inferred: no __FILE__). The line-of-sight calamity
// class: 0xb1f4f..0xb2034. Unit flags: the exe's module set. Reset is ICF-folded (0x157269).
// Data (.rdata, in order): -10000.0f 0x43cac4, the vtable 0x43cacc (COL 0x43cac8).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/FSRestorable.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEGlobalCalamityClass.h"
#include "ee/EECCLineOfSight.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEUnit.h"

// FUNCTION: EXE 0x4b1f4f
// Creates the line-of-sight calamity class
EECCLineOfSight::EECCLineOfSight()
{
}

// SYNTHETIC: EXE 0x4b1f61
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b1f7d
// Destroys the calamity class
EECCLineOfSight::~EECCLineOfSight()
{
}

// FUNCTION: EXE 0x4b1f88
// Raises the caster's sight by the class's bonus, redoing its sight on the map; returns false without a caster
bool EECCLineOfSight::Start(EECalamity* inCalamity)
{
    if (inCalamity->mCasterUnit == NULL)
        return false;
    EEComplexUnit* theUnit = inCalamity->mCasterUnit->mComplexUnit;
    theUnit->SightOffHere();
    theUnit->SetSight(theUnit->GetSight() + this->mSightBonus);
    theUnit->SightOnHere();
    return true;
}

// FUNCTION: EXE 0x4b1fc4
// Takes the sight bonus back from the caster if it is still alive
void EECCLineOfSight::Stop(EECalamity* inCalamity)
{
    if (inCalamity->mCasterUnit == NULL || !(inCalamity->mCasterUnit->mHitPoints > 0))
        return;
    EEComplexUnit* theUnit = inCalamity->mCasterUnit->mComplexUnit;
    theUnit->SightOffHere();
    theUnit->SetSight(theUnit->GetSight() - this->mSightBonus);
    theUnit->SightOnHere();
}

// FUNCTION: EXE 0x4b2000
// Reads the calamity data and the sight bonus
void EECCLineOfSight::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mSightBonus = inData->mFloatParam1;
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCLineOfSight::Reset(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4b2019
// Ages the calamity; returns the time-alive result
long EECCLineOfSight::Update(EECalamity* inCalamity, float inElapsedTime)
{
    return EECalamityClass::UpdateTimeAlive(inCalamity, inElapsedTime, -1);
}

// FUNCTION: EXE 0x4b202f
// Always allows the cast (returns 0)
long EECCLineOfSight::CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                                bool inCheck, EEUnit* inTargetUnit)
{
    EECalamityCanCastScope theCapture;   // CCAS (TEST ONLY)
    return theCapture.Record(this, inUnit, inLocation, inX, inY, inCheck, inTargetUnit, 0);
}
