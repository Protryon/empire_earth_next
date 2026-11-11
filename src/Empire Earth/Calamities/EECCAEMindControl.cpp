// C:\EEX\Empire Earth\Calamities\EECCAEMindControl.cpp (__FILE__ 0x4b9aa0, EECCAEMindControl::Stop's ESVERIFY).
// The mind-control area-effect calamity class: 0xaef4c..0xaf1ce. Unit flags: the exe's module set.
// Reset and Update are copies of EECCAEExtraDamage's: /OPT:ICF folded them into that unit's (0xaee30, 0xaee45), so
// this unit's range ends with Initialize.
// Data (.rdata, in order): kNoEffectHandle 0x43c78c, kCalamitySoundFalloff 0x43c790, the vtable 0x43c798 (COL
// 0x43c794). (.data): type descriptor 0x4b9a88, __FILE__ 0x4b9aa0; the assert text is EECCAEExtraDamage.cpp's
// (string pooling).
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
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EECCAreaEffect.h"
#include "ee/EECCAEMindControl.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEMap.h"
#include "ee/EESoundObject.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4aef4c
// Creates the mind-control area-effect calamity class with no second-stage delay yet
EECCAEMindControl::EECCAEMindControl() : mEffect2Time(0)
{
    this->mClassFlags = 0x80;
}

// SYNTHETIC: EXE 0x4aef69
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4aef85
// Destroys the calamity class (the area-effect base releases its effects and functor)
EECCAEMindControl::~EECCAEMindControl()
{
}

// FUNCTION: EXE 0x4aef90
// Starts a mind-control calamity: registers its contagion helper, plays the launch effect from the caster
// and the calamity sound; returns false when no helper could be made
bool EECCAEMindControl::Start(EECalamity* inCalamity)
{
    bool theResult = false;
    GE3DPoint theLocation;
    EEEffectHandle theEffectHandle;
    float theVolume;
    float thePan;
    float thePitch;

    long theCalamityHelperID = ::gWorld.GetContagionManager()->CreateContagionHelper(this->mFunctor, inCalamity, NULL);
    if (::gWorld.GetContagionManager()->GetContagionHelper(theCalamityHelperID) != NULL) {
        inCalamity->mIntPlaceHolder1 = theCalamityHelperID;
        if (this->mGraphicEffect1 != NULL && inCalamity->mCasterUnit != NULL) {
            theLocation = inCalamity->mTargetLocation;
            gEffectManager->CreateEffect(inCalamity->mCasterUnit, theLocation, this->mGraphicEffect1,
                                         &theEffectHandle, 0);
        }
        EEComputeSoundParameters(this->mSound->GetVolume(), theLocation, theVolume, thePan, thePitch,
                                 kCalamitySoundFalloff);
        inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), theVolume, thePan,
                                                                        thePitch, this->mSound->GetLooping(), NULL));
        if (inCalamity->mSound != NULL)
            inCalamity->mSound->Dereference();
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4af0b1
// Stops the calamity: destroys its placed effect, stops both sounds and removes its contagion helper
void EECCAEMindControl::Stop(EECalamity* inCalamity)
{
    long theCalamityHelperID = inCalamity->mIntPlaceHolder1;
    if (inCalamity->mEffectHandle != kNoEffectHandle) {
        gEffectManager->DestroyEffect(inCalamity->mEffectHandle);
        inCalamity->mEffectHandle = kNoEffectHandle;
    }
    inCalamity->StopSound();
    inCalamity->StopSecondarySound();
    ESVERIFY_AT(::gWorld.GetContagionManager()->DestroyContagionHelper(theCalamityHelperID), 122);
}

// FUNCTION: EXE 0x4af117
// Reads the calamity data: sound, radius, launch, ground and caught-unit effects (the second stage starts
// with the ground effect), and builds the mind-control functor
void EECCAEMindControl::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mSound = EESoundObject::Get(inData->mSoundID);
    this->mRadius = inData->mIntParam1;
    if (inData->mGraphicEffectID1 > 0) {
        this->mGraphicEffect1 = EEGraphicEffect::Get(inData->mGraphicEffectID1);
        this->mGraphicEffect1->Reference();
        this->mEffect1Duration = this->mGraphicEffect1->mDuration;
    } else
        this->mEffect1Duration = 0;
    if (inData->mGraphicEffectID2 > 0) {
        this->mGraphicEffect2 = EEGraphicEffect::Get(inData->mGraphicEffectID2);
        this->mGraphicEffect2->Reference();
        this->mEffect2Time = this->mEffect1Duration;
    }
    if (inData->mGraphicEffectID3 > 0) {
        this->mGraphicEffect3 = EEGraphicEffect::Get(inData->mGraphicEffectID3);
        this->mGraphicEffect3->Reference();
    }
    this->mFunctor = new EEAEMindControlFunctor(this->mGraphicEffect3, this->GetUnitSet(), this->mIsBlockedByImmunity ? 2 : 3);
}

// FUNCTION: EXE 0x4aee30
// Clears the calamity's two stage flags so both stages run again (ICF: the extra-damage copy)
// (ICF-folded into EECCAEExtraDamage::Reset)
void EECCAEMindControl::Reset(EECalamity* inCalamity)
{
    inCalamity->mBoolPlaceHolder1 = false;
    inCalamity->mBoolPlaceHolder2 = false;
}

// FUNCTION: EXE 0x4aee45
// Ages the calamity and updates its sound; after the launch effect places the ground effect, after the
// second delay applies the effect to the units in range once (ICF: the extra-damage copy)
// (ICF-folded into EECCAEExtraDamage::Update)
long EECCAEMindControl::Update(EECalamity* inCalamity, float inElapsedTime)
{
    GE3DPoint theLocation;
    long theResult = EECalamityClass::UpdateTimeAlive(inCalamity, inElapsedTime, inCalamity->mIntPlaceHolder1);
    if (this->mSound != NULL)
        this->UpdateSound(inCalamity, this->mSound, -1.0f, 15.0f, true);
    if (inCalamity->GetTimeAlive() >= this->mEffect1Duration && !inCalamity->mBoolPlaceHolder1) {
        inCalamity->mBoolPlaceHolder1 = true;
        if (this->mGraphicEffect2 != NULL) {
            theLocation = inCalamity->mTargetLocation;
            theLocation.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()) + 0.05f);
            EEEffectHandle theEffectHandle;
            gEffectManager->CreatePlacedEffect(theLocation, this->mGraphicEffect2, &theEffectHandle);
            EECalamityCaptureEvent(kCalamityOpPlacedEffect, theEffectHandle.mValue, 0, theLocation.mX, theLocation.mZ);   // CALW (TEST ONLY)
            inCalamity->mEffectHandle = theEffectHandle;
        }
    }
    if (inCalamity->GetTimeAlive() >= this->mEffect2Time && !inCalamity->mBoolPlaceHolder2) {
        inCalamity->mBoolPlaceHolder2 = true;
        EECalamityCaptureEvent(kCalamityOpApplyEffect);   // CALW (TEST ONLY)
        this->ApplyEffect(inCalamity);
    }
    return theResult;
}

// ICF-FOLDED: EXE 0x418aa9
bool EECCAEMindControl::AffectsOwnUnits()
{
    return true;
}
