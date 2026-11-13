// C:\EEX\Empire Earth\Calamities\EECCMeteorStorm.cpp (file name inferred: no __FILE__; the class name). The meteor
// storm calamity class: 0xb2634..0xb2ba3. Unit flags: the exe's module set. Stop and Reset are ICF-folded
// (0x157269), Virtual30 too (0xac98e). EEUGAttackGround's inline ctor and the inline slots of its vtable are first
// emitted here (0xb2a4b..0xb2abe).
// Data (.rdata, in order): kCalamitySoundFalloff 0x43cb7c, 7.0f 0x43cb80, 500.0f 0x43cb84, 2000.0f 0x43cb88, the
// vtable 0x43cb90 (COL 0x43cb8c), -0.707f 0x43cbe0, EEUGAttackGround's vtable 0x43cbe8.
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
#include "ee/EECCMeteorStorm.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEGameSound.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4b2634
// Creates the meteor storm calamity class
EECCMeteorStorm::EECCMeteorStorm()
{
}

// SYNTHETIC: EXE 0x4b2646
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b2662
// Destroys the calamity class
EECCMeteorStorm::~EECCMeteorStorm()
{
}

// FUNCTION: EXE 0x4b266d
// Starts a meteor storm: the first meteor falls at time 100, the first sound at once
bool EECCMeteorStorm::Start(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, NULL, NULL);
    return EECalamityRunStart(this, inCalamity, theParams, NULL, NULL, NULL);
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCMeteorStorm::Stop(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4b2684
// Reads the calamity data: sound, lifetime, meteor unit class, radius and the minimum and maximum launch intervals
void EECCMeteorStorm::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mSound = EESoundObject::Get(inData->mSoundID);
    this->mLifeTime = (float)inData->mLifeTime;
    this->mUnitClassIndex = inData->mUnitClassIndex;
    this->mRadius = inData->mFloatParam1;
    this->mMinimumInterval = (float)inData->mIntParam2;
    this->mMaximumInterval = (float)inData->mIntParam3;
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCMeteorStorm::Reset(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4b26d0
// Loads the sound and the meteor unit class's media
void EECCMeteorStorm::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    this->mSound->LoadMedia();
    if (inIsOnDemand == true)
        inPlayer->LoadUnitClassMedia(this->mUnitClassIndex);
    inPlayer->SetUnitClassMedia(this->mUnitClassIndex, 0x7f);
}

// FUNCTION: EXE 0x4b2700
// Plays the storm sound once at the target location
void EECCMeteorStorm::PlaySound(EECalamity* inCalamity)
{
    GE3DPoint theLocation;
    float theVolume;
    float thePan;
    float thePitch;
    theLocation = inCalamity->mTargetLocation;
    EEComputeSoundParameters(this->mSound->GetVolume(), theLocation, theVolume, thePan, thePitch, kCalamitySoundFalloff);
    gSoundManager->Play(this->mSound->GetSampleID(), theVolume, thePan, thePitch, false, NULL);
}

// ICF-FOLDED: EXE 0x4ac98e
// (empty: folded into a shared `ret 0xc`)
void EECCMeteorStorm::Place(EEUnit*, unsigned long, unsigned long)
{
}

// FUNCTION: EXE 0x4b277a
// Unloads the sound and the meteor unit class's media
void EECCMeteorStorm::UnloadMedia(EEPlayer* inPlayer)
{
    this->mSound->UnloadMedia();
    inPlayer->UnloadUnitClassMedia(this->mUnitClassIndex);
}

// FUNCTION: EXE 0x4b2795
// Creates a nature-owned meteor 7 tiles north of and above a random point near the target and sends it diving onto
// that point; always returns true
bool EECCMeteorStorm::LaunchMeteor(EECalamity* inCalamity)
{
    GE3DPoint theLocation;
    inCalamity->GetTargetLocation(theLocation);
    GE3DPoint theStart;
    GE3DPoint theTarget;
    float theOffsetX = (0.5f - gPhysics->GetRandom()) * this->mRadius;
    float theOffsetY = (0.5f - gPhysics->GetRandom()) * this->mRadius;
    EEUnitClass* theClass = gUnitClassTable[this->mUnitClassIndex];
    EEUnit* theCaster = inCalamity->mCasterUnit;
    theStart.SetX(theLocation.GetX() + theOffsetX);
    theStart.SetY(theOffsetY + (theLocation.GetY() + 7.0f));
    theStart.SetZ(theLocation.GetZ() + 7.0f);
    theTarget.SetX(theStart.GetX());
    theTarget.SetY(theLocation.GetY() + theOffsetY);
    theTarget.SetZ(theLocation.GetZ());
    EEUnit* theUnit = theClass->CreateUnit(UMFloor(theStart.GetX()), UMFloor(theStart.GetY()),
                                           gWorld.mPlayers[0]->GetPlayerID(), 0, theCaster);
    EEComplexUnit* theMeteor = theUnit->mComplexUnit;
    theClass->PlayCreationEffect(theMeteor);
    theMeteor->SetLauncherClass(theClass);
    theMeteor->SetOwner(theMeteor);
    theMeteor->SetGoal(EEUGRust::NewAttackGround(theTarget, false), 0, 0);
    theMeteor->SetVelocity(theMeteor->GetMaxVelocity());
    theMeteor->SetProjectileStart(theStart);
    theMeteor->SetLocation(theStart, true);
    theMeteor->SetHeight(theStart.GetZ());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    GETransformation theTransformation(theMeteor->mPhysicalModel.GetLocation());
    GE3DPoint thePosition(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    theTransformation.Apply(theForward, theDirection);
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    theMeteor->SetFlightTime(0);
    theMeteor->mOrientation = GEOrientation(theYaw, -0.707f, 0.0f);
    theMeteor->SetProjectileStart(thePosition);
    theMeteor->SetVelocity(UMCos(-0.707f) * theMeteor->GetMaxVelocity());
    theTransformation.SetOrientationYPR(theYaw, -0.707f, 0.0f);
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, theMeteor, &theTileX, &theTileY);
    return true;
}

// Compiler-generated for this unit (EEUGAttackGround's inline ctor and the inline slots of its vtable, first emitted
// here):
// TEMPLATE: EXE 0x4b2a4b
// TEMPLATE: EXE 0x4b2a89
// TEMPLATE: EXE 0x4b2a8f
// TEMPLATE: EXE 0x4b2a93
// TEMPLATE: EXE 0x4b2a9d
// TEMPLATE: EXE 0x4b2aa7

// FUNCTION: EXE 0x4b2abe
// Ages the storm: launches meteors at random intervals, replays the sound every 500 to 2000, and returns 7 at the end
// of its lifetime (Rust)
long EECCMeteorStorm::Update(EECalamity* inCalamity, float inElapsedTime)
{
    // the update is Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h)
    EeCalamityParams theParams = EECalamityBaseParams(this, NULL, NULL);
    theParams.meteor_minimum_interval = this->mMinimumInterval;
    theParams.meteor_maximum_interval = this->mMaximumInterval;
    return EECalamityRunUpdate(this, inCalamity, inElapsedTime, theParams, NULL, NULL, &EECCMeteorStorm::UpdateStep);
}

// New (port): EECCMeteorStorm's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCMeteorStorm::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCMeteorStorm* theClass = static_cast<EECCMeteorStorm*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::LaunchMeteor: {
        EECalamityCaptureEvent(kCalamityOpLaunchMeteor);   // CALW (TEST ONLY)
        theClass->LaunchMeteor(theCalamity);
        return true;
    }
    default:
        return false;
    }
}

// FUNCTION: EXE 0x4b2b62
// Accepts only space tiles
bool EECCMeteorStorm::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    // the rule is Rust (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
    EeCalamityCast theCast = EECalamityCastOf(this, inUnit, inX, inY);
    theCast.space_terrain = kTerrainSpace;
    return EECalamityValidLocation(this, theCast, inUnit, 0);
}
