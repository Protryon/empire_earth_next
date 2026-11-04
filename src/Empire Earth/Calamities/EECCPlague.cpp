// C:\EEX\Empire Earth\Calamities\EECCPlague.cpp (__FILE__ 0x4b9d04, EECCPlague::Stop's ESVERIFY). The plague
// calamity class: 0xb2f31..0xb3217. Unit flags: the exe's module set.
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
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EECCPlague.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4b2f31
// Creates the plague calamity class with no effect, functor, damage or sounds
EECCPlague::EECCPlague()
{
    this->mLifeTime = 0.0f;
    this->mGraphicEffect = NULL;
    this->mFunctor = NULL;
    this->mDamage = 0;
    this->mSound = NULL;
    this->mSecondarySound = NULL;
    this->mClassFlags = 1;
}

// SYNTHETIC: EXE 0x4b2f60
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b2f7c
// Destroys the calamity class, releasing its effect and deleting its plague functor
EECCPlague::~EECCPlague()
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Dereference();
    delete this->mFunctor;
    this->mFunctor = NULL;
}

// FUNCTION: EXE 0x4b2fac
// Starts a plague by showing its effect from the caster to the target; always returns true
bool EECCPlague::Start(EECalamity* inCalamity)
{
    this->ShowEffect(inCalamity);
    return true;
}

// FUNCTION: EXE 0x4b2fba
// Stops the plague's sounds and removes its contagion helper (asserts that it existed)
void EECCPlague::Stop(EECalamity* inCalamity)
{
    long theCalamityHelperID = inCalamity->mIntPlaceHolder1;
    inCalamity->StopSound();
    inCalamity->StopSecondarySound();
    ESVERIFY_AT(::gWorld.GetContagionManager()->DestroyContagionHelper(theCalamityHelperID), 86);
}

// FUNCTION: EXE 0x4b3001
// Returns the plague functor that spreads the disease
EEContagionFunctor* EECCPlague::GetContagionFunctor()
{
    return this->mFunctor;
}

// FUNCTION: EXE 0x4b3005
// Reads the calamity data: sounds, damage, radius, damage time, chance and effect; sets the class flags to 1;
// builds the plague functor
void EECCPlague::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mSound = EESoundObject::Get(inData->mSoundID);
    this->mSecondarySound = EESoundObject::Get(inData->mSecondarySoundID);
    this->mDamage = inData->mIntParam1;
    float theRadius = inData->mFloatParam1;
    long theDamageTime = inData->mIntParam2;
    float theChance = inData->mFloatParam2;
    this->mGraphicEffect = EEGraphicEffect::Get(inData->mGraphicEffectID1);
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Reference();
    // the calamity manager builds this class only for the plague; the test for class 19 (AE Mind Control, always
    // built as EECCAEMindControl) that set flags 0x10 could never pass and is gone
    this->mClassFlags = 1;
    this->mFunctor = new EEPlagueFunctor(theRadius, theChance, this->mDamage, theDamageTime, this->GetUnitSet(), this->mIsBlockedByImmunity ? 2 : 3);
}

// FUNCTION: EXE 0x4b30c7
// Registers a contagion helper that spreads the plague from the target unit
void EECCPlague::Reset(EECalamity* inCalamity)
{
    EEUnit* theTargetUnit = NULL;
    inCalamity->GetTargetUnit(theTargetUnit);
    inCalamity->mIntPlaceHolder1 = ::gWorld.GetContagionManager()->CreateContagionHelper(this->mFunctor, inCalamity,
                                                                                         theTargetUnit);
}

// FUNCTION: EXE 0x4b3102
// Loads the effect and both sounds
void EECCPlague::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Load(inIsOnDemand);
    if (this->mSound != NULL)
        this->mSound->LoadMedia();
    if (this->mSecondarySound != NULL)
        this->mSecondarySound->LoadMedia();
}

// FUNCTION: EXE 0x4b3131
// Shows the plague effect from the caster to the target: a disease effect between their positions or a tracer beam
// between the units
void EECCPlague::ShowEffect(EECalamity* inCalamity)
{
    GE3DPoint theCasterLocation;
    GE3DPoint theTargetLocation;
    EEUnit* theCaster = inCalamity->mCasterUnit;
    if (theCaster == NULL)
        return;
    EEUnit* theTarget;
    inCalamity->GetTargetUnit(theTarget);
    long theKind = this->mGraphicEffect->mKind;
    if (theKind == kEffect3DDisease) {
        theCaster->GetPosition(theCasterLocation);
        theTarget->GetPosition(theTargetLocation);
        gEffectManager->CreateEffectBetween(theCasterLocation, theTargetLocation, this->mGraphicEffect, NULL);
    } else if (theKind == kEffect3DTracerBeam)
        gEffectManager->CreateEffectBetweenUnits(theCaster, theTarget, this->mGraphicEffect, NULL);
}

// FUNCTION: EXE 0x4b31c5
// Unloads the effect and both sounds
void EECCPlague::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Unload();
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
    if (this->mSecondarySound != NULL)
        this->mSecondarySound->UnloadMedia();
}

// FUNCTION: EXE 0x4b31f0
// Ages the calamity with its contagion helper; returns the time-alive result
long EECCPlague::Update(EECalamity* inCalamity, float inElapsedTime)
{
    return EECalamityClass::UpdateTimeAlive(inCalamity, inElapsedTime, inCalamity->mIntPlaceHolder1);
}

// FUNCTION: EXE 0x4b3208
// Accepts a target the plague functor can infect
bool EECCPlague::IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit)
{
    return this->mFunctor->CanInfect(inTargetUnit);
}
