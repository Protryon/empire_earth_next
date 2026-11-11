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
#include "ee/EECalamityUpdate.h"
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
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    if (this->mGraphicEffect1 != NULL)
        theParams.flags |= EE_CALAMITY_EFFECT1;
    return EECalamityRunStart(this, inCalamity, theParams, this->mSound, NULL, &EECCAreaEffect::StartStep);
}

// FUNCTION: EXE 0x4af0b1
// Stops the calamity: destroys its placed effect, stops both sounds and removes its contagion helper
void EECCAEMindControl::Stop(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    if (this->mGraphicEffect1 != NULL)
        theParams.flags |= EE_CALAMITY_EFFECT1;
    EECalamityRunStop(this, inCalamity, theParams, this->mSound, NULL, &EECCAreaEffect::StartStep);
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
    // the update is Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h)
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    if (this->mGraphicEffect2 != NULL)
        theParams.flags |= EE_CALAMITY_EFFECT2;
    theParams.area_delay = (uint32_t)this->mEffect1Duration;
    theParams.area_time2 = (int32_t)this->mEffect2Time;
    return EECalamityRunUpdate(this, inCalamity, inElapsedTime, theParams, this->mSound, NULL, &EECCAEMindControl::UpdateStep);
}

// New (port): EECCAEMindControl's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCAEMindControl::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCAEMindControl* theClass = static_cast<EECCAEMindControl*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::PlacedEffect: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        EEEffectHandle theEffectHandle;
        gEffectManager->CreatePlacedEffect(theLocation, theClass->mGraphicEffect2, &theEffectHandle);
        EECalamityCaptureEvent(kCalamityOpPlacedEffect, theEffectHandle.mValue, 0, theLocation.mX, theLocation.mZ);   // CALW (TEST ONLY)
        outReply.id = (uint32_t)theEffectHandle.mValue;
        return true;
    }
    case EeCalamityOp::ApplyEffect: {
        EECalamityCaptureEvent(kCalamityOpApplyEffect);   // CALW (TEST ONLY)
        theClass->ApplyEffect(theCalamity);
        return true;
    }
    default:
        return false;
    }
}

// ICF-FOLDED: EXE 0x418aa9
bool EECCAEMindControl::AffectsOwnUnits()
{
    return true;
}
