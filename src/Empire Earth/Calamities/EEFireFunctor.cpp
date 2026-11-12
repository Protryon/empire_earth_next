// C:\EEX\Empire Earth\Calamities\EEFireFunctor.cpp (file name inferred: no __FILE__; the class name, as the codebase
// names its files). The fire's contagion functor: 0xb6bd1..0xb6e58. Unit flags: the exe's module set.
// CanInfectType (0xb6c4d) is also every other functor's (ICF), GetRadius (0xb6c49) EEPlagueFunctor's GetChance.
// Data (.rdata): kNoEffectHandle 0x43cf9c, the vtable 0x43cfa8.
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
#include "ee/EEEffect2D.h"
#include "ee/EEEDamagedEffect.h"
#include "ee/EECalamity.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EETechCapture.h"
#include "rust/ee_tech.h"

// FUNCTION: EXE 0x4b6bd1
// Creates the fire functor with its burning effect (referenced), burn, radius, chance, damage, spread time, unit set
// and hit mode
EEFireFunctor::EEFireFunctor(EEGraphicEffect* inEffect, long inBurn, float inRadius, float inChance, long inDamage,
                             long inSpreadTime, unsigned long inValue, long inMode)
{
    this->mBurn = inBurn;
    this->mRadius = inRadius;
    this->mChance = inChance;
    this->mDamage = inDamage;
    this->mEffect = inEffect;
    this->mEffect->Reference();
    this->mSpreadTime = inSpreadTime;
    this->mUnitSet = inValue;
    this->mMode = inMode;
}

// SYNTHETIC: EXE 0x4b6c13
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b6c2f
// Destroys the functor, releasing its burning effect
EEFireFunctor::~EEFireFunctor()
{
    this->mEffect->Dereference();
}

// FUNCTION: EXE 0x4b6c49
// Returns the spread radius (also the plague functor's GetChance, by ICF)
float EEFireFunctor::GetRadius()
{
    return this->mRadius;
}

// FUNCTION: EXE 0x4b6c4d
// Accepts a unit type in the functor's unit set (every functor's CanInfectType, by ICF)
bool EEFireFunctor::CanInfectType(long inCategory, long inUnitType)
{
    bool theResult = false;
    if (gUnitSetManager.Contains(inUnitType, this->GetUnitSet()))
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x4b6c70
// Returns the spread chance
float EEFireFunctor::GetChance()
{
    return this->mChance;
}

// FUNCTION: EXE 0x4b6c74
// Returns the spread time
unsigned long EEFireFunctor::GetSpreadTime()
{
    return this->mSpreadTime;
}

// FUNCTION: EXE 0x4b6c78
// Returns the unit set the fire can burn
unsigned long EEFireFunctor::GetUnitSet()
{
    return this->mUnitSet;
}

// FUNCTION: EXE 0x4b6c7c
// Counts the whole spread-time periods elapsed on the calamity for this update's damage
void EEFireFunctor::Update(float inElapsedTime, EECalamity* inCalamity)
{
    inCalamity->mDiscreteUpdateTime = this->mSpreadTime;
    inCalamity->mTimeElapsedSinceLastUpdate += inElapsedTime;
    this->mUpdates = inCalamity->GetDiscreteUpdates();
}

// FUNCTION: EXE 0x4b6cad
// Keeps the burning unit from being deleted and damages it per elapsed period unless it is inside a carrier
void EEFireFunctor::Apply(EEUnit* inUnit, float inElapsedTime, EECalamity* inCalamity)
{
    long theDamage;
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    EEUnit* theCasterUnit = inCalamity->mCasterUnit;
    if (theComplexUnit != NULL)
        theComplexUnit->SetIsDeletable(false);
    if (!(this->mUpdates > 0))
        return;
    theDamage = ee_contagion_tick_damage((int32_t)this->mDamage, (uint32_t)this->mUpdates);   // (ee_tech contagion.rs)
    if (theComplexUnit == NULL || theComplexUnit->GetOutermostCarrier() == theComplexUnit) {
        EECalamityCaptureContagionEvent(kContagionDamage, inUnit->mUnitID, theDamage);   // CONT (TEST ONLY)
        inUnit->TakeHit(theCasterUnit, theDamage, inCalamity->mPlayerNumber, this->mMode);
    }
}

// FUNCTION: EXE 0x4b6d2c
// Sets the unit alight: burning effect once, then burn percent of its maximum hit points as damage unless carried
void EEFireFunctor::Infect(EEUnit* inUnit, EECalamity* inCalamity)
{
    EECalamityCaptureContagionEvent(kContagionInfect, inUnit->mUnitID);   // CONT (TEST ONLY)
    EEUnit* theCasterUnit = inCalamity->mCasterUnit;
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    long theDamage = ee_contagion_fire_burn((int32_t)this->mBurn, (int32_t)inUnit->mStats->mMaxHitPoints);   // (ee_tech contagion.rs)
    if (theComplexUnit != NULL)
        theComplexUnit->SetIsDeletable(false);
    if (theComplexUnit->mFireEffect == kNoEffectHandle) {
        EEEffectHandle theEffectHandle;
        gEffectManager->CreateUnitEffect(theComplexUnit, this->mEffect, &theEffectHandle, 0, 0, 0);
        theComplexUnit->mFireEffect = theEffectHandle;
    }
    if (theComplexUnit == NULL || theComplexUnit->GetOutermostCarrier() == theComplexUnit) {
        EECalamityCaptureContagionEvent(kContagionDamage, inUnit->mUnitID, theDamage);   // CONT (TEST ONLY)
        inUnit->TakeHit(theCasterUnit, theDamage, inCalamity->mPlayerNumber, this->mMode);
    }
}

// FUNCTION: EXE 0x4b6dec
// Puts out the fire: restores the unit's deletable flag and finishes or removes its burning effect
void EEFireFunctor::Cure(EEUnit* inUnit, EECalamity* inCalamity)
{
    EECalamityCaptureContagionEvent(kContagionCure, inUnit->mUnitID);   // CONT (TEST ONLY)
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    if (theComplexUnit == NULL)
        return;
    theComplexUnit->SetIsDeletable(inUnit->mUnitClass->mIsDeletable);
    EEEffectHandle theEffectHandle = theComplexUnit->mFireEffect;
    if (theEffectHandle == kNoEffectHandle)
        return;
    EEEffect* theEffect = gEffectManager->GetEffect(theEffectHandle);
    if (theEffect != NULL) {
        if (theEffect->mKind == kEffectDamaged)
            (static_cast<EEEDamagedEffect*>(theEffect))->mIsFinished = true;
        else
            gEffectManager->DestroyEffect(theEffectHandle);
    }
    theComplexUnit->mFireEffect = kNoEffectHandle;
}
