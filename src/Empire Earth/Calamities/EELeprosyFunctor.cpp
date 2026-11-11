// C:\EEX\Empire Earth\Calamities\EELeprosyFunctor.cpp (file name inferred: no __FILE__; the class name). The
// leprosy's contagion functor: 0xb6e58..0xb6f21 (EELocationCalamityClass.cpp follows). Unit flags: the exe's module
// set. Virtual1C, Virtual20, the implicit dtor and the ??_G are ICF-folded (0x60b1, 0x157269, 0xac9e7, 0xac9cb).
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

// FUNCTION: EXE 0x4b6e58
// Creates the leprosy functor with its damage, spread time, unit set and hit mode
EELeprosyFunctor::EELeprosyFunctor(long inDamage, unsigned long inSpreadTime, unsigned long inValue, long inMode)
{
    this->mUpdates = 0;
    this->mDamage = inDamage;
    this->mSpreadTime = inSpreadTime;
    this->mUnitSet = inValue;
    this->mMode = inMode;
}

// FUNCTION: EXE 0x4b6e83
// Returns the unit set leprosy can infect
unsigned long EELeprosyFunctor::GetUnitSet()
{
    return this->mUnitSet;
}

// FUNCTION: EXE 0x4b6e87
// Shows the unit class's leprosy effect on the unit
void EELeprosyFunctor::Infect(EEUnit* inUnit, EECalamity* inCalamity)
{
    EECalamityCaptureContagionEvent(kContagionInfect, inUnit->mUnitID);   // CONT (TEST ONLY)
    EEGraphicEffect* theEffect = inUnit->mUnitClass->mLeprosyEffect;
    if (theEffect != NULL)
        gEffectManager->CreateUnitEffect(inUnit, theEffect, NULL, 0, 0, 0);
}

// FUNCTION: EXE 0x4b6eae
// Counts the whole spread-time periods elapsed on the calamity for this update's damage
void EELeprosyFunctor::Update(float inElapsedTime, EECalamity* inCalamity)
{
    inCalamity->mDiscreteUpdateTime = this->mSpreadTime;
    inCalamity->mTimeElapsedSinceLastUpdate += inElapsedTime;
    this->mUpdates = inCalamity->GetDiscreteUpdates();
}

// FUNCTION: EXE 0x4b6edf
// Damages the unit per elapsed period, never below 1 hit point; damage of 1 or less is skipped
void EELeprosyFunctor::Apply(EEUnit* inUnit, float inElapsedTime, EECalamity* inCalamity)
{
    EEUnit* theCasterUnit = inCalamity->mCasterUnit;
    if (!(this->mUpdates > 0))
        return;
    long theDamage = this->mDamage * this->mUpdates;
    theDamage = min(theDamage, inUnit->mHitPoints - 1);
    if (theDamage > 1) {
        EECalamityCaptureContagionEvent(kContagionDamage, inUnit->mUnitID, theDamage);   // CONT (TEST ONLY)
        inUnit->TakeHit(theCasterUnit, theDamage, inCalamity->mPlayerNumber, this->mMode);
    }
}

// The folded virtuals' bodies (identical to other units' copies: no address of their own)
// ICF-FOLDED: EXE 0x4060b1
bool EELeprosyFunctor::SkipsSpread()
{
    return true;
}

// ICF-FOLDED: EXE 0x557269
void EELeprosyFunctor::UpdateUnits(std::vector<EEUnit*>* inUnits)
{
}

// ICF-FOLDED: EXE 0x4b6c4d
bool EELeprosyFunctor::CanInfectType(long inCategory, long inUnitType)
{
    bool theResult = false;
    if (gUnitSetManager.Contains(inUnitType, this->GetUnitSet()))
        theResult = true;
    return theResult;
}
