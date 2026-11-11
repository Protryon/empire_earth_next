// C:\EEX\Empire Earth\Calamities\EEPlagueFunctor.cpp (file name inferred: no __FILE__; the class name). The plague's
// contagion functor: 0xb6fb1..0xb70cc (EEUnitCalamityClass.cpp follows). Unit flags: the exe's module set.
// Virtual04, CanInfectType and GetChance are ICF-folded (0x27c0b4, 0xb6c4d, 0xb6c49), as are the implicit dtor and
// the ??_G (0xac9e7, 0xac9cb). GetUnitSet and Update are also the area-effect functors' slots (ICF).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EETechCapture.h"

// FUNCTION: EXE 0x4b6fb1
// Creates the plague functor with its radius, chance, damage, damage time, unit set and hit mode
EEPlagueFunctor::EEPlagueFunctor(float inRadius, float inChance, long inDamage, long inDamageTime, unsigned long inValue,
                                 long inMode)
{
    this->mRadius = inRadius;
    this->mDamage = inDamage;
    this->mChance = inChance;
    this->mDamageTime = inDamageTime;
    this->mUnitSet = inValue;
    this->mMode = inMode;
}

// FUNCTION: EXE 0x4b6fe4
// Returns the spread radius
float EEPlagueFunctor::GetRadius()
{
    return this->mRadius;
}

// FUNCTION: EXE 0x4b6fe8
// Returns 1000: the plague tries to spread every second
unsigned long EEPlagueFunctor::GetSpreadTime()
{
    return 1000;
}

// FUNCTION: EXE 0x4b6fee
// Returns the unit set the plague can infect
unsigned long EEPlagueFunctor::GetUnitSet()
{
    return this->mUnitSet;
}

// FUNCTION: EXE 0x4b6ff2
// Counts the whole damage-time periods elapsed on the calamity for this update's damage
void EEPlagueFunctor::Update(float inElapsedTime, EECalamity* inCalamity)
{
    inCalamity->mDiscreteUpdateTime = this->mDamageTime;
    inCalamity->mTimeElapsedSinceLastUpdate += inElapsedTime;
    this->mUpdates = inCalamity->GetDiscreteUpdates();
}

// FUNCTION: EXE 0x4b7023
// Damages the unit per elapsed period and keeps it from being deleted
void EEPlagueFunctor::Apply(EEUnit* inUnit, float inElapsedTime, EECalamity* inCalamity)
{
    if (this->mUpdates > 0) {
        EECalamityCaptureContagionEvent(kContagionDamage, inUnit->mUnitID, this->mDamage * this->mUpdates);   // CONT (TEST ONLY)
        inUnit->TakeHit(inCalamity->mCasterUnit, this->mDamage * this->mUpdates, inCalamity->mPlayerNumber, this->mMode);
    }
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    if (theComplexUnit != NULL)
        theComplexUnit->SetIsDeletable(false);
}

// FUNCTION: EXE 0x4b706c
// Shows the unit class's plague effect on the unit and keeps it from being deleted
void EEPlagueFunctor::Infect(EEUnit* inUnit, EECalamity* inCalamity)
{
    EECalamityCaptureContagionEvent(kContagionInfect, inUnit->mUnitID);   // CONT (TEST ONLY)
    EEGraphicEffect* theEffect = inUnit->mUnitClass->mPlagueEffect;
    if (theEffect != NULL)
        gEffectManager->CreateUnitEffect(inUnit, theEffect, NULL, 0, 0, 0);
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    if (theComplexUnit != NULL)
        theComplexUnit->SetIsDeletable(false);
}

// FUNCTION: EXE 0x4b70a9
// Restores the unit's deletable flag from its class
void EEPlagueFunctor::Cure(EEUnit* inUnit, EECalamity* inCalamity)
{
    EECalamityCaptureContagionEvent(kContagionCure, inUnit->mUnitID);   // CONT (TEST ONLY)
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    if (theComplexUnit != NULL)
        theComplexUnit->SetIsDeletable(inUnit->mUnitClass->mIsDeletable);
}

// The folded virtuals' bodies (identical to other units' copies: no address of their own)
// ICF-FOLDED: EXE 0x67c0b4
bool EEPlagueFunctor::InfectsAlliesOnly()
{
    return false;
}

// ICF-FOLDED: EXE 0x4b6c4d
bool EEPlagueFunctor::CanInfectType(long inCategory, long inUnitType)
{
    bool theResult = false;
    if (gUnitSetManager.Contains(inUnitType, this->GetUnitSet()))
        theResult = true;
    return theResult;
}

// ICF-FOLDED: EXE 0x4b6c49
float EEPlagueFunctor::GetChance()
{
    return this->mChance;
}
