// C:\EEX\Empire Earth\Calamities\EECCDropObject.cpp (file name inferred: no __FILE__). The drop-object calamity class:
// 0xafef9..0xb037f (its members, with the three EEComplexUnit inline members it emits first after Stop). Unit flags:
// the exe's module set. Reset, Virtual28 and Virtual34 are ICF-folded (0x157269, the area effects' 0xaf5f2/0xaf632).
// Data (.rdata, in order): kNoEffectHandle 0x43c8fc, -10000.0f 0x43c900, kCalamitySoundFalloff 0x43c904, the vtable
// 0x43c90c (COL 0x43c908).
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
#include "ee/EECCDropObject.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EETechCapture.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUnitSetManager.h"

// FUNCTION: EXE 0x4afef9
// Creates the drop-object calamity class with no effects or sound
EECCDropObject::EECCDropObject()
{
    this->mGraphicEffect1 = NULL;
    this->mGraphicEffect2 = NULL;
    this->mGraphicEffect3 = NULL;
    this->mSound = NULL;
}

// SYNTHETIC: EXE 0x4aff19
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4aff35
// Destroys the calamity class, releasing its three effects
EECCDropObject::~EECCDropObject()
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Dereference();
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Dereference();
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Dereference();
}

// FUNCTION: EXE 0x4aff6e
// Plays the launch effect from the caster to the target and the calamity sound; returns true
bool EECCDropObject::Start(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    if (this->mGraphicEffect1 != NULL)
        theParams.flags |= EE_CALAMITY_EFFECT1;
    return EECalamityRunStart(this, inCalamity, theParams, this->mSound, NULL, &EECCDropObject::StartStep);
}

// FUNCTION: EXE 0x4b004f
// When the target and caster still live, drops the calamity's unit next to the target as a projectile
// launched by the caster, ordered to attack the target; then stops the effect and sounds
void EECCDropObject::Stop(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    if (this->mGraphicEffect1 != NULL)
        theParams.flags |= EE_CALAMITY_EFFECT1;
    EECalamityRunStop(this, inCalamity, theParams, this->mSound, NULL, &EECCDropObject::StartStep);
}

// Compiler-generated for this unit (EEComplexUnit inline members, first emitted here):
// TEMPLATE: EXE 0x4b01ad
// TEMPLATE: EXE 0x4b01d2
// TEMPLATE: EXE 0x4b021c

// FUNCTION: EXE 0x4b0252
// Reads the calamity data: the unit class to drop, the sound and three effects
void EECCDropObject::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mUnitClassIndex = inData->mUnitClassIndex;
    if (inData->mSoundID != 0)
        this->mSound = EESoundObject::Get(inData->mSoundID);
    if (inData->mGraphicEffectID1 != 0) {
        this->mGraphicEffect1 = EEGraphicEffect::Get(inData->mGraphicEffectID1);
        this->mGraphicEffect1->Reference();
    }
    if (inData->mGraphicEffectID2 != 0) {
        this->mGraphicEffect2 = EEGraphicEffect::Get(inData->mGraphicEffectID2);
        this->mGraphicEffect2->Reference();
    }
    if (inData->mGraphicEffectID3 != 0) {
        this->mGraphicEffect3 = EEGraphicEffect::Get(inData->mGraphicEffectID3);
        this->mGraphicEffect3->Reference();
    }
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCDropObject::Reset(EECalamity* inCalamity)
{
}

// ICF-FOLDED: EXE 0x4af5f2
// (identical to EECCAreaEffect::LoadMedia)
void EECCDropObject::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Load(inIsOnDemand);
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Load(inIsOnDemand);
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Load(inIsOnDemand);
    if (this->mSound != NULL)
        this->mSound->LoadMedia();
}

// ICF-FOLDED: EXE 0x4af632
// (identical to EECCAreaEffect::UnloadMedia)
void EECCDropObject::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Unload();
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Unload();
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Unload();
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
}

// FUNCTION: EXE 0x4b02c7
// Ages the calamity and updates its sound; returns the time-alive result (Rust)
long EECCDropObject::Update(EECalamity* inCalamity, float inElapsedTime)
{
    // the update is Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h)
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    return EECalamityRunUpdate(this, inCalamity, inElapsedTime, theParams, this->mSound, NULL, NULL);
}

// FUNCTION: EXE 0x4b0310
// Accepts a finished, non-foundation enemy unit on the map that is in the calamity's unit set
bool EECCDropObject::IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit)
{
    // the rule is Rust (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
    return EECalamityValidTarget(this, inUnit, inTargetUnit);
}

// New (port): EECCDropObject's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCDropObject::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCDropObject* theClass = static_cast<EECCDropObject*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        // the dropped unit next to the target, attacking it
        EEUnit* theTargetUnit = NULL;
        EEComplexUnit* theCaster = theCalamity->mCasterUnit->mComplexUnit;
        theCalamity->GetTargetUnit(theTargetUnit);
        GE3DPoint theLocation;
        EEFindDropLocation(theCaster, &theTargetUnit, &theLocation, theTargetUnit);
        long theX = UMFloor(theLocation.GetX());
        long theY = UMFloor(theLocation.GetY());
        EEUnitClass* theUnitClass = gUnitClassTable[theClass->mUnitClassIndex];
        EEUnit* theUnit = theUnitClass->CreateUnit(theX, theY, theCaster->mPlayer->mPlayerID, 0, theCaster);
        theUnit->mComplexUnit->SetLauncherClass(theUnitClass);
        theUnit->mComplexUnit->SetOwner(theUnit->mComplexUnit);
        theUnit->mComplexUnit->SetGoal(new EEUGAttackUnit(theUnit->GetComplexUnit(), theTargetUnit, 0, 0), 0, 0);
        theUnit->mComplexUnit->SetVelocity(0.0f);
        theUnit->mComplexUnit->SetProjectileStart(theLocation);
        theUnit->mComplexUnit->SetHeight(theLocation.GetZ());
        return true;
    }
    case 2: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        EEEffectHandle theEffectHandle;
        EECalamityCaptureStartPoint(1, theLocation.mX, theLocation.mY, theLocation.mZ);   // CALT (TEST ONLY)
        gEffectManager->CreateEffect(theCalamity->mCasterUnit, theLocation, theClass->mGraphicEffect1, &theEffectHandle, 0);
        return true;
    }
    default:
        return false;
    }
}
