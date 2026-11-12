// C:\EEX\Empire Earth\Calamities\EECCEarthquake.cpp (file name inferred: no __FILE__; the class name). The earthquake
// calamity class: 0xb037f..0xb0e2e. Unit flags: the exe's module set. Virtual34 is ICF-folded into EECCFire's
// (0xb10c9). The unit ends with EEGameVariant::GetGameVariant (0xb0e20; its first caller in link order is Update).
// Data (.rdata, after the vtable 0x43c964): the pooled literals 6.2831855f 0x43c9b4, 0.11f 0x43c9b8, 3.1415927f 0x43c9bc.
#include <windows.h>

#include <algorithm>

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
#include "ee/EECCEarthquake.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEGameVariant.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIKindIDs.h"
#include "lle/GEViewport.h"

extern GEViewport gViewport;                // 0x5308f0 (Empire Earth.cpp)




// FUNCTION: EXE 0x4b037f
// Creates the earthquake calamity class with no crack or source effect
EECCEarthquake::EECCEarthquake()
    : mGraphicEffect2(NULL), mGraphicEffect1(NULL)
{
}

// SYNTHETIC: EXE 0x4b0399
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b03b5
// Destroys the calamity class, releasing its two effects
EECCEarthquake::~EECCEarthquake()
{
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Dereference();
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Dereference();
}

// FUNCTION: EXE 0x4b03e1
// Starts an earthquake: sends the source effect from the caster and plays the sound; the quake itself waits
bool EECCEarthquake::Start(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    theParams.quake_source_speed = this->mSourceSpeed;
    return EECalamityRunStart(this, inCalamity, theParams, this->mSound, NULL, &EECCEarthquake::StartStep);
}

// FUNCTION: EXE 0x4b0502
// Draws the quake's cracks radiating from the target at roughly even, randomly jittered angles;
// returns false when a crack effect could not be created
bool EECCEarthquake::CreateCracks(EECalamity* inCalamity)
{
    GETransformation theTransformation;
    GEVector theDirection;
    GEVector theOffset;
    GE3DPoint theCenter;
    float theStep = 6.2831855f / this->mCrackCount;
    bool theResult = this->SetAffectedArea(inCalamity);
    if (theResult) {
        theCenter = inCalamity->mTargetLocation;
        theResult = true;
        for (unsigned long i = 0; i < this->mCrackCount; i++) {
            theDirection.SetY(gPhysics->mRandom.Generate() - 0.5f + this->mRadius);
            theTransformation.Rotate(((gPhysics->mRandom.Generate() * 0.5f - 0.25f) * theStep) + theStep, 0.0f, 0.0f);
            theTransformation.Apply(theDirection, theOffset);
            if (this->CreateCrack(theCenter, theCenter.GetX() + theOffset.GetX(), theOffset.GetY() + theCenter.mY) != 0) {
                theResult = false;
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4b0621
// Loads the quake's two effects and sound
void EECCEarthquake::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Load(inIsOnDemand);
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Load(inIsOnDemand);
    if (this->mSound != NULL)
        this->mSound->LoadMedia();
}

// FUNCTION: EXE 0x4b0654
// Plays the quake's sound for the calamity at its default volume
void EECCEarthquake::PlaySound(EECalamity* inCalamity)
{
    inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), this->mSound->GetVolume(), 0.0f,
                                                                    1.0f, this->mSound->GetLooping(), NULL));
    if (inCalamity->mSound != NULL)
        inCalamity->mSound->Dereference();
}

// FUNCTION: EXE 0x4b06b5
// Sets the calamity's affected rectangle to the quake radius around its target; returns true
bool EECCEarthquake::SetAffectedArea(EECalamity* inCalamity)
{
    GE3DPoint theLocation;
    inCalamity->GetTargetLocation(theLocation);
    GE2DPoint theTopLeft;
    GE2DPoint theBottomRight;
    theTopLeft.SetX(theLocation.GetX() - this->mRadius);
    theTopLeft.SetY(theLocation.GetY() - this->mRadius);
    theBottomRight.SetX(theLocation.GetX() + this->mRadius);
    theBottomRight.SetY(theLocation.GetY() + this->mRadius);
    inCalamity->mRectangle = GERectangle(theTopLeft, theBottomRight);
    return true;
}

// FUNCTION: EXE 0x4b071c
// Creates one crack effect from a point to (inX, inY); returns the effect manager's result
long EECCEarthquake::CreateCrack(GE3DPoint& inFrom, float inX, float inY)
{
    GE3DPoint theTo(inX, inY, 0.0f);
    return gWorld.GetEffectManager()->CreateEffectBetween(inFrom, theTo, this->mGraphicEffect1, NULL);
}

// FUNCTION: EXE 0x4b0750
// Stops the quake's sounds
void EECCEarthquake::Stop(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    theParams.quake_source_speed = this->mSourceSpeed;
    EECalamityRunStop(this, inCalamity, theParams, this->mSound, NULL, &EECCEarthquake::StartStep);
}

// FUNCTION: EXE 0x4b0765
// Reads the calamity data: crack and source effects (the radius and duration come from the crack effect),
// volumes, falloff, damage per tick, shake, tick time, crack count and sound
void EECCEarthquake::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mGraphicEffect1 = EEGraphicEffect::Get(inData->mGraphicEffectID1);
    this->mGraphicEffect1->Reference();
    this->mGraphicEffect2 = EEGraphicEffect::Get(inData->mGraphicEffectID2);
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Reference();
    this->mRadius = this->mGraphicEffect1->GetSize();
    this->mMinimumFactor = inData->mFloatParam1;
    this->mVolume = inData->mFloatParam2;
    this->mQuakeMinimumFactor = inData->mQuakeMinimumStrength;
    this->mQuakeVolume = inData->mQuakeStrength;
    this->mSourceSpeed = this->mGraphicEffect2->mDuration * 0.01f;
    this->mFalloff = 1.0f / inData->mIntParam1;
    this->mDamage = inData->mIntParam2;
    this->mShake = inData->mIntParam3;
    this->mShakeInterval = 1.0f / inData->mIntParam4;
    this->mTickTime = inData->mIntParam5;
    this->mCrackCount = inData->mIntParam6;
    this->mDuration = this->mGraphicEffect1->mDuration * 0.001f;
    this->mSpeed = this->mRadius / this->mDuration;
    this->mSound = EESoundObject::Get(inData->mSoundID);
}

// FUNCTION: EXE 0x4b0850
// Restarts the camera shake timer
void EECCEarthquake::Reset(EECalamity* inCalamity)
{
    inCalamity->mFloatPlaceHolder1 = 0.0f;
}

// FUNCTION: EXE 0x4b0a60
// Runs the quake in Rust: after the source delay draws the cracks, then each tick damages the living land
// buildings on the caster's continent within the growing radius, shakes the camera, sets the epicenter factor and
// the sound; returns 7 once the quake is over
long EECCEarthquake::Update(EECalamity* inCalamity, float inElapsedTime)
{
    // the update is Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h)
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    theParams.quake_damage = (int32_t)this->mDamage;
    theParams.quake_tick_time = (int32_t)this->mTickTime;
    theParams.quake_shake = (int32_t)this->mShake;
    theParams.quake_duration = this->mDuration;
    theParams.quake_radius = this->mRadius;
    theParams.quake_shake_interval = this->mShakeInterval;
    theParams.quake_volume = this->mVolume;
    theParams.quake_quake_volume = this->mQuakeVolume;
    theParams.quake_minimum_factor = this->mMinimumFactor;
    theParams.quake_quake_minimum_factor = this->mQuakeMinimumFactor;
    theParams.quake_speed = this->mSpeed;
    theParams.quake_falloff = this->mFalloff;
    theParams.quake_damage_factor = EEGameVariant::GetGameVariant(::gWorld.mGameOptions.mVariant)->GetEarthquakeDamageFactor();
    return EECalamityRunUpdate(this, inCalamity, inElapsedTime, theParams, this->mSound, NULL, &EECCEarthquake::UpdateStep);
}

// New (port): EECCEarthquake's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCEarthquake::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCEarthquake* theClass = static_cast<EECCEarthquake*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::Cracks: {
        EECalamityCaptureEvent(kCalamityOpCracks);   // CALW (TEST ONLY)
        theClass->CreateCracks(theCalamity);
        return true;
    }
    case EeCalamityOp::CameraDistance: {
        // UpdateEpicenterFactor's distance from the camera to the target, in the view plane
        GE3DPoint theViewLocation;
        GE3DPoint theTarget;
        theCalamity->GetTargetLocation(theTarget);
        GE3DPoint theOrigin;
        gViewport.GetCameraInverse().Apply(theTarget, theViewLocation);
        theViewLocation.SetY(0.0f);
        outReply.x = theViewLocation.DistanceTo(theOrigin);
        return true;
    }
    case EeCalamityOp::Shake: {
        // ShakeCamera's shake of the local player's camera (once per player and manager update)
        EEPlayer* thePlayer = ::gWorld.GetCurrentPlayer();
        if (!thePlayer->mEarthquakeShaken) {
            thePlayer->mEarthquakeShaken = true;
            float theAngle = inCall.x;
            float theAmplitude = inCall.y;
            EECalamityCaptureEvent(kCalamityOpShake, 0, 0, theAngle, theAmplitude);   // CALW (TEST ONLY)
            GE3DPoint theLocation;
            ::gWorld.mCameraControl.GetLocation(::gWorld.mCameraControl.mFraction, theLocation);
            theLocation.mX += UMCos(theAngle) * theAmplitude;
            theLocation.SetY(theLocation.GetY() + UMCos(1.5707964f - theAngle) * theAmplitude);
            ::gWorld.mCameraControl.SetLocation(theLocation, true, true);
            theCalamity->mShakeLocation = theLocation;
        }
        return true;
    }
    default:
        return false;
    }
}

// FUNCTION: EXE 0x4b0da5
// Accepts a map tile, on a space map only one on the caster's continent
bool EECCEarthquake::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    // the rule is Rust (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
    EeCalamityCast theCast = EECalamityCastOf(this, inUnit, inX, inY);
    return EECalamityValidLocation(this, theCast, inUnit, 0);
}

// ICF-FOLDED: EXE 0x4b10c9
// (folded into EECCFire's identical Virtual34)
void EECCEarthquake::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Unload();
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Unload();
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
}

// FUNCTION: EXE 0x4b0e20
// Returns the game variant with the given index
// Probably inline in the real header (an out-of-line COMDAT right after its first user, this unit's Update);
// defined here so that the unit owns its bytes
EEGameVariant* EEGameVariant::GetGameVariant(long inIndex)
{
    return gGameVariants[inIndex];
}

// New (port): EECCEarthquake's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCEarthquake::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCEarthquake* theClass = static_cast<EECCEarthquake*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        // CreateSourceEffect's caster part: the quake's area around the target, the source effect from the caster
        GERectangle theRectangle;
        GE3DPoint theLocation;
        GE3DPoint theSource;
        theCalamity->GetTargetLocation(theLocation);
        GE2DPoint theTopLeft;
        GE2DPoint theBottomRight;
        theTopLeft.SetX(theLocation.GetX() - theClass->mRadius);
        theTopLeft.SetY(theLocation.GetY() - theClass->mRadius);
        theBottomRight.SetX(theLocation.GetX() + theClass->mRadius);
        theBottomRight.SetY(theLocation.GetY() + theClass->mRadius);
        theRectangle.SetTopLeft(theTopLeft);
        theRectangle.SetBottomRight(theBottomRight);
        theCalamity->SetRectangle(theRectangle);
        EEComplexUnit* theUnit = theCalamity->mCasterUnit->mComplexUnit;
        if (theUnit != NULL) {
            GETransformation theTransformation(false);
            theTransformation = theUnit->mPhysicalModel.GetLocation();
            theSource.SetX(theTransformation.GetX());
            theSource.SetY(theTransformation.GetY());
            theSource.SetZ(theTransformation.GetZ());
            outReply.ok = true;
            outReply.value = (int32_t)gWorld.GetEffectManager()->CreateEffectBetween(theSource, theLocation, theClass->mGraphicEffect2, NULL);
            outReply.x = theSource.DistanceTo(theLocation);
        }
        return true;
    }
    default:
        return false;
    }
}
