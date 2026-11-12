// C:\EEX\Empire Earth\Calamities\EECCTeleport.cpp (file name inferred: no __FILE__; the class name). The teleport
// calamity class: 0xb3494..0xb3ade. Unit flags: the exe's module set. Reset is ICF-folded into
// EECCAEExtraDamage::Reset (0xaee30), Virtual2C into the shared empty function (0x157269).
// Data (.rdata): the vtable 0x43ce48 (COL 0x43ce44).
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
#include "ee/EECCTeleport.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEGameSound.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EETileLocation.h"
#include "ee/EEWUnitsOffMapInfo.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4b3494
// Creates the teleport calamity class with no sounds, effects, hold time or placement range
EECCTeleport::EECCTeleport()
{
    this->mSound = NULL;
    this->mSecondarySound = NULL;
    this->mGraphicEffect1 = NULL;
    this->mGraphicEffect2 = NULL;
    this->mHoldTime = 0;
    this->mPlacementRange = 0;
}

// SYNTHETIC: EXE 0x4b34ba
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b34d6
// Destroys the calamity class, releasing both effects (assumed present)
EECCTeleport::~EECCTeleport()
{
    this->mGraphicEffect1->Dereference();
    this->mGraphicEffect2->Dereference();
}

// FUNCTION: EXE 0x4b34fa
// Starts a teleport: disables the live caster, shows the departure effect on it and plays the sound; returns false
// without a live caster
bool EECCTeleport::Start(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, this->mSecondarySound);
    return EECalamityRunStart(this, inCalamity, theParams, this->mSound, this->mSecondarySound, &EECCTeleport::StartStep);
}

// FUNCTION: EXE 0x4b3592
// Re-enables the caster and stops the sounds
void EECCTeleport::Stop(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, this->mSecondarySound);
    EECalamityRunStop(this, inCalamity, theParams, this->mSound, this->mSecondarySound, &EECCTeleport::StartStep);
}

// FUNCTION: EXE 0x4b35b8
// Reads the calamity data: sounds, damage factor, placement range and two effects; the hold time is the departure
// effect's duration (2000 without one)
void EECCTeleport::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mSound = EESoundObject::Get(inData->mSoundID);
    this->mSecondarySound = EESoundObject::Get(inData->mSecondarySoundID);
    this->mDamageFactor = inData->mFloatParam1;
    this->mPlacementRange = inData->mIntParam1;
    this->mHoldTime = 2000;
    if (inData->mGraphicEffectID1 > 0) {
        this->mGraphicEffect1 = EEGraphicEffect::Get(inData->mGraphicEffectID1);
        this->mGraphicEffect1->Reference();
        this->mHoldTime = this->mGraphicEffect1->mDuration;
    }
    if (inData->mGraphicEffectID2 > 0) {
        this->mGraphicEffect2 = EEGraphicEffect::Get(inData->mGraphicEffectID2);
        this->mGraphicEffect2->Reference();
    }
}

// ICF-FOLDED: EXE 0x4aee30
// (identical to EECCAEExtraDamage::Reset)
void EECCTeleport::Reset(EECalamity* inCalamity)
{
    inCalamity->mBoolPlaceHolder1 = false;
    inCalamity->mBoolPlaceHolder2 = false;
}

// FUNCTION: EXE 0x4b3636
// Loads both effects and both sounds
void EECCTeleport::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    this->mGraphicEffect2->Load(inIsOnDemand);
    this->mGraphicEffect1->Load(inIsOnDemand);
    this->mSound->LoadMedia();
    this->mSecondarySound->LoadMedia();
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCTeleport::PlaySound(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4b3665
// Places the live caster near the target (else near where it left; killed if neither fits), tells its group, shows the
// arrival effect, scales its hit points by the damage factor (at least 1) and re-enables it
void EECCTeleport::Teleport(EECalamity* inCalamity)
{
    GETransformation theTransformation;
    GE3DPoint theLocation;
    EEUnit* theCaster = inCalamity->GetCasterUnit();
    GE3DPoint theFoundLocation;
    if (theCaster == NULL || !(theCaster->mHitPoints > 0))
        return;
    inCalamity->SetSecondarySound(gSoundManager->PlayAndReturnReferencedNode(this->mSecondarySound->GetSampleID(),
                                                                             this->mSecondarySound->GetVolume(), 0.0f,
                                                                             1.0f, this->mSecondarySound->GetLooping(),
                                                                             NULL));
    if (inCalamity->mSecondarySound != NULL)
        inCalamity->mSecondarySound->Dereference();
    EEComplexUnit* theUnit = theCaster->mComplexUnit;
    inCalamity->GetTargetLocation(theLocation);
    long theX = UMFloor(theLocation.GetX());
    long theY = UMFloor(theLocation.GetY());
    if (EEFindPlacement(&theFoundLocation, theUnit, NULL, theX, theY, theUnit->mUnitClass->GetTileSize(), this->mPlacementRange, 1, 1, 0, 0,
                        &theTransformation, NULL)) {
        theUnit->SetTransformation(theTransformation, true);
        if (theUnit->mActiveGroup != NULL)
            theUnit->mActiveGroup->TellGroup(theUnit, GE3DPoint(theTransformation.GetX(), theTransformation.GetY(),
                                                               theTransformation.GetZ()));
        gEffectManager->CreateUnitEffect(theCaster, this->mGraphicEffect2, NULL, 0, 0, 0);
        theUnit->SetHitPoints(max(1, UMFloatToLong(theUnit->mHitPoints * this->mDamageFactor)));
        theUnit->SetDisabled(0, 0);
    } else {
        theX = UMFloor(theUnit->mU0DC.GetX());
        theY = UMFloor(theUnit->mU0DC.GetY());
        if (EEFindPlacement(&theFoundLocation, theUnit, NULL, theX, theY, theUnit->mUnitClass->GetTileSize(), this->mPlacementRange, 1, 1, 0,
                            0, &theTransformation, NULL)) {
            theUnit->SetTransformation(theTransformation, true);
            if (theUnit->mActiveGroup != NULL)
                theUnit->mActiveGroup->TellGroup(theUnit, GE3DPoint(theTransformation.GetX(),
                                                                   theTransformation.GetY(),
                                                                   theTransformation.GetZ()));
            gEffectManager->CreateUnitEffect(theCaster, this->mGraphicEffect2, NULL, 0, 0, 0);
            theUnit->SetHitPoints(max(1, UMFloatToLong(theUnit->mHitPoints * this->mDamageFactor)));
            theUnit->SetDisabled(0, 0);
        } else
            theUnit->Kill(1, 0, 0);
    }
}

// FUNCTION: EXE 0x4b390b
// Takes the live caster off the map at its current position, disabled, with off-map reason 4
void EECCTeleport::HoldCaster(EECalamity* inCalamity)
{
    U2DPoint<float> thePosition;
    EEUnit* theCaster = inCalamity->mCasterUnit;
    if (theCaster == NULL || !(theCaster->mHitPoints > 0))
        return;
    EEComplexUnit* theUnit = theCaster->mComplexUnit;
    theUnit->SetDisabled(1, 0);
    thePosition.SetX(theUnit->mPhysicalModel.GetLocation().GetX());
    thePosition.SetY(theUnit->mPhysicalModel.GetLocation().GetY());
    theUnit->Set2DPosition(thePosition);
    theUnit->TakeOffMap();
    EEWUnitsOffMapInfo* theInfo = theUnit->mPlayer->GetOffMapInfo(theUnit);
    if (theInfo != NULL)
        theInfo->mReason = 4;
}

// Compiler-generated for this unit (EEComplexUnit::Set2DPosition, inline, first emitted here):
// TEMPLATE: EXE 0x4b3976

// FUNCTION: EXE 0x4b39a9
// Unloads both effects and both sounds
void EECCTeleport::UnloadMedia(EEPlayer* inPlayer)
{
    this->mGraphicEffect2->Unload();
    this->mGraphicEffect1->Unload();
    this->mSound->UnloadMedia();
    this->mSecondarySound->UnloadMedia();
}

// FUNCTION: EXE 0x4b39d0
// Ages the calamity and its sounds; takes the caster off the map after the hold time and teleports it before the end;
// returns 7 once the caster is dead
long EECCTeleport::Update(EECalamity* inCalamity, float inElapsedTime)
{
    // the update is Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h)
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, this->mSecondarySound);
    theParams.teleport_hold_time = (int32_t)this->mHoldTime;
    return EECalamityRunUpdate(this, inCalamity, inElapsedTime, theParams, this->mSound, this->mSecondarySound, &EECCTeleport::UpdateStep);
}

// New (port): EECCTeleport's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCTeleport::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCTeleport* theClass = static_cast<EECCTeleport*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::Hold: {
        EECalamityCaptureEvent(kCalamityOpHold);   // CALW (TEST ONLY)
        theClass->HoldCaster(theCalamity);
        return true;
    }
    case EeCalamityOp::Teleport: {
        EECalamityCaptureEvent(kCalamityOpTeleport);   // CALW (TEST ONLY)
        theClass->Teleport(theCalamity);
        return true;
    }
    default:
        return false;
    }
}

// FUNCTION: EXE 0x4b3aac
// Accepts a land tile whose surrounding area is on the map
bool EECCTeleport::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    // the rule is Rust (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
    EeCalamityCast theCast = EECalamityCastOf(this, inUnit, inX, inY);
    return EECalamityValidLocation(this, theCast, inUnit, 0);
}

// New (port): EECCTeleport's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCTeleport::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCTeleport* theClass = static_cast<EECCTeleport*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        EEUnit* theCaster = theCalamity->GetCasterUnit();
        theCaster->mComplexUnit->SetDisabled(1, 0x10);
        gEffectManager->CreateUnitEffect(theCaster, theClass->mGraphicEffect1, NULL, 0, 0, 0);
        return true;
    }
    case 2:
        theCalamity->mCasterUnit->mComplexUnit->SetDisabled(0, 0);
        return true;
    default:
        return false;
    }
}
