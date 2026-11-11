// C:\EEX\Empire Earth\Calamities\EECCFire.cpp (__FILE__ 0x4b9ba8, EECCFire::Stop's ESVERIFY). The fire calamity
// class: 0xb0e2e..0xb114a. Unit flags: the exe's module set. GetContagionFunctor is ICF-folded (0x12c730).
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
#include "ee/EECCFire.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4b0e2e
// Creates the fire calamity class with no effects, functor, burn or spread chance
EECCFire::EECCFire()
{
    this->mLifeTime = 0.0f;
    this->mGraphicEffect1 = NULL;
    this->mRadius = 0.0f;
    this->mGraphicEffect2 = NULL;
    this->mFunctor = NULL;
    this->mBurn = 0;
    this->mChance = 0.0f;
    this->mClassFlags = 4;
}

// SYNTHETIC: EXE 0x4b0e64
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b0e80
// Destroys the calamity class, deleting its fire functor and releasing its two effects
EECCFire::~EECCFire()
{
    delete this->mFunctor;
    this->mFunctor = NULL;
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Dereference();
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Dereference();
}

// FUNCTION: EXE 0x4b0ebd
// Starts a fire on the target unit with the ignition effect and sound; returns false without a target unit
bool EECCFire::Start(EECalamity* inCalamity)
{
    bool theResult = false;
    EEUnit* theTargetUnit = NULL;
    if (inCalamity->GetTargetUnit(theTargetUnit)) {
        if (this->mGraphicEffect1 != NULL)
            gEffectManager->CreateUnitEffect(theTargetUnit, this->mGraphicEffect1, NULL, 0, 0, 0);
        if (this->mSound != NULL) {
            inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), this->mSound->GetVolume(),
                                                                            0.0f, 1.0f, this->mSound->GetLooping(), NULL));
            if (inCalamity->mSound != NULL)
                inCalamity->mSound->Dereference();
        }
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4b0f60
// Stops the fire's sounds and removes its contagion helper
void EECCFire::Stop(EECalamity* inCalamity)
{
    long theCalamityHelperID = inCalamity->mIntPlaceHolder1;
    inCalamity->StopSound();
    inCalamity->StopSecondarySound();
    ESVERIFY_AT(::gWorld.GetContagionManager()->DestroyContagionHelper(theCalamityHelperID), 118);
}

// ICF-FOLDED: EXE 0x52c730
// (folded into another class's identical accessor)
EEContagionFunctor* EECCFire::GetContagionFunctor()
{
    return this->mFunctor;
}

// FUNCTION: EXE 0x4b0fa7
// Reads the calamity data: sound, ignition and burning effects, burn, damage, spread time, radius and
// chance, and builds the fire functor
void EECCFire::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mSound = EESoundObject::Get(inData->mSoundID);
    if (inData->mGraphicEffectID1 > 0) {
        this->mGraphicEffect1 = EEGraphicEffect::Get(inData->mGraphicEffectID1);
        this->mGraphicEffect1->Reference();
    }
    this->mGraphicEffect2 = EEGraphicEffect::Get(inData->mGraphicEffectID2);
    this->mGraphicEffect2->Reference();
    this->mBurn = inData->mIntParam2;
    this->mDamage = inData->mIntParam3;
    long theSpreadTime = inData->mIntParam4;
    this->mRadius = inData->mFloatParam1;
    this->mChance = inData->mFloatParam2;
    this->mFunctor = new EEFireFunctor(this->mGraphicEffect2, this->mBurn, this->mRadius, this->mChance, this->mDamage, theSpreadTime, this->GetUnitSet(), this->mIsBlockedByImmunity ? 2 : 3);
}

// FUNCTION: EXE 0x4b105f
// Registers a contagion helper that spreads the fire from the target unit
void EECCFire::Reset(EECalamity* inCalamity)
{
    EEUnit* theTargetUnit = NULL;
    inCalamity->GetTargetUnit(theTargetUnit);
    inCalamity->mIntPlaceHolder1 = ::gWorld.GetContagionManager()->CreateContagionHelper(this->mFunctor, inCalamity,
                                                                                         theTargetUnit);
}

// FUNCTION: EXE 0x4b109a
// Loads the fire's effects and sound (the burning effect is assumed present)
void EECCFire::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Load(inIsOnDemand);
    this->mGraphicEffect2->Load(inIsOnDemand);
    if (this->mSound != NULL)
        this->mSound->LoadMedia();
}

// FUNCTION: EXE 0x4b10c9
// Unloads the fire's effects and sound (also the earthquake's, by ICF)
void EECCFire::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Unload();
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Unload();
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
}

// FUNCTION: EXE 0x4b10f4
// Updates the fire's sound and ages it with its contagion helper; returns the time-alive result (Rust)
long EECCFire::Update(EECalamity* inCalamity, float inElapsedTime)
{
    // the update is Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h)
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    return EECalamityRunUpdate(this, inCalamity, inElapsedTime, theParams, this->mSound, NULL, NULL);
}

// FUNCTION: EXE 0x4b113b
// Accepts a target the fire functor can set alight
bool EECCFire::IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit)
{
    return this->mFunctor->CanInfect(inTargetUnit);
}
