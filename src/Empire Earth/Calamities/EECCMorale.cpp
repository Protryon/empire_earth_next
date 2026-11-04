// C:\EEX\Empire Earth\Calamities\EECCMorale.cpp (file name inferred: no __FILE__). The morale calamity class:
// 0xb2ba3..0xb2f31. Unit flags: the exe's module set. Reset is ICF-folded (0xb1c06).
// Data (.rdata, in order): kNoEffectHandle 0x43ccdc, -10000.0f 0x43cce0, the vtable 0x43cce8 (COL 0x43cce4).
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
#include "ee/EECCMorale.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"

// FUNCTION: EXE 0x4b2ba3
// Creates the morale calamity class with no sounds or effect
EECCMorale::EECCMorale()
{
    this->mSound = NULL;
    this->mSecondarySound = NULL;
    this->mGraphicEffect = NULL;
    this->mEffectDuration = 0;
}

// SYNTHETIC: EXE 0x4b2bc3
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b2bdf
// Destroys the calamity class, releasing its effect
EECCMorale::~EECCMorale()
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Dereference();
}

// FUNCTION: EXE 0x4b2bfe
// Starts a morale calamity on the target unit: sends the effect from the caster to it and plays the sound; returns
// false without a target
bool EECCMorale::Start(EECalamity* inCalamity)
{
    EEEffectHandle theEffectHandle;
    bool theResult = false;
    EEUnit* theTargetUnit = NULL;
    if (inCalamity->GetTargetUnit(theTargetUnit)) {
        if (this->mGraphicEffect != NULL && inCalamity->mCasterUnit != NULL) {
            gEffectManager->CreateEffectBetweenUnits(inCalamity->mCasterUnit, theTargetUnit->mComplexUnit,
                                                     this->mGraphicEffect, &theEffectHandle);
            inCalamity->mEffectHandle = theEffectHandle;
        }
        inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), this->mSound->GetVolume(), 0.0f,
                                                                        1.0f, this->mSound->GetLooping(), NULL));
        if (inCalamity->mSound != NULL)
            inCalamity->mSound->Dereference();
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4b2cad
// Takes the positive or negative morale count Update gave the target back, removes its effect and stops the sounds
void EECCMorale::Stop(EECalamity* inCalamity)
{
    EEUnit* theTargetUnit;
    if (inCalamity->GetTargetUnit(theTargetUnit)) {
        // only a count Update took (once the effect time had passed) is given back
        if (inCalamity->mBoolPlaceHolder1) {
            if (this->mPositive)
                theTargetUnit->mComplexUnit->mPositiveMorale--;
            else
                theTargetUnit->mComplexUnit->mNegativeMorale--;
        }
        if (inCalamity->mEffectHandle != kNoEffectHandle) {
            gEffectManager->DestroyEffect(inCalamity->mEffectHandle);
            inCalamity->mEffectHandle = kNoEffectHandle;
        }
    }
    inCalamity->StopSound();
    inCalamity->StopSecondarySound();
}

// FUNCTION: EXE 0x4b2d13
// Reads the calamity data: two sounds, the effect and its duration, and whether the morale is positive
void EECCMorale::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mSound = EESoundObject::Get(inData->mSoundID);
    this->mSecondarySound = EESoundObject::Get(inData->mSecondarySoundID);
    if (inData->mGraphicEffectID1 > 0) {
        this->mGraphicEffect = EEGraphicEffect::Get(inData->mGraphicEffectID1);
        this->mGraphicEffect->Reference();
        this->mEffectDuration = this->mGraphicEffect->mDuration;
    }
    this->mPositive = inData->mBoolParam1;
}

// ICF-FOLDED: EXE 0x4b1c06
// (folded into a later unit's identical copy)
void EECCMorale::Reset(EECalamity* inCalamity)
{
    inCalamity->mBoolPlaceHolder1 = false;
}

// FUNCTION: EXE 0x4b2d6d
// Loads the effect and both sounds
void EECCMorale::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Load(inIsOnDemand);
    if (this->mSound != NULL)
        this->mSound->LoadMedia();
    if (this->mSecondarySound != NULL)
        this->mSecondarySound->LoadMedia();
}

// FUNCTION: EXE 0x4b2d9c
// Ages the calamity; once the effect has played, gives the target its class's morale effect and one positive or
// negative morale count (once) and plays the secondary sound
long EECCMorale::Update(EECalamity* inCalamity, float inElapsedTime)
{
    long theResult = EECalamityClass::UpdateTimeAlive(inCalamity, inElapsedTime, -1);
    if (inCalamity->GetTimeAlive() >= this->mEffectDuration) {
        if (this->mSecondarySound != NULL)
            this->UpdateSound(inCalamity, this->mSecondarySound, -1.0f, 15.0f, true);
        if (!inCalamity->mBoolPlaceHolder1) {
            inCalamity->mBoolPlaceHolder1 = true;
            EEUnit* theTargetUnit;
            inCalamity->GetTargetUnit(theTargetUnit);
            EEGraphicEffect* theEffect = theTargetUnit->mUnitClass->mMoraleEffect;
            if (theEffect != NULL) {
                EEEffectHandle theEffectHandle;
                gEffectManager->CreateUnitEffect(theTargetUnit, theEffect, &theEffectHandle, 0, 0, 0);
                EEEffectHandle theOldHandle = inCalamity->mEffectHandle;
                if (theOldHandle != kNoEffectHandle) {
                    gEffectManager->DestroyEffect(theOldHandle);
                    inCalamity->mEffectHandle = kNoEffectHandle;
                }
                inCalamity->mEffectHandle = theEffectHandle;
            }
            if (this->mPositive)
                theTargetUnit->mComplexUnit->mPositiveMorale++;
            else
                theTargetUnit->mComplexUnit->mNegativeMorale++;
            if (this->mSecondarySound != NULL)
                gSoundManager->Play(this->mSecondarySound->GetSampleID(), this->mSecondarySound->GetVolume(), 0.0f, 1.0f,
                                    this->mSecondarySound->GetLooping(), NULL);
        }
    } else
        this->UpdateSound(inCalamity, this->mSound, -1.0f, 15.0f, true);
    return theResult;
}

// FUNCTION: EXE 0x4b2ed2
// Unloads the effect and both sounds
void EECCMorale::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Unload();
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
    if (this->mSecondarySound != NULL)
        this->mSecondarySound->UnloadMedia();
}

// FUNCTION: EXE 0x4b2efd
// Accepts a unit on the map that is in the class's unit set
bool EECCMorale::IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit)
{
    bool theResult = false;
    if (inTargetUnit->mTileNode != NULL && gUnitSetManager.Contains(inTargetUnit->mUnitClass->GetUnitType(), this->GetUnitSet()))
        theResult = true;
    return theResult;
}
