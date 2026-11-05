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
    bool theResult = this->CreateSourceEffect(inCalamity);
    if (theResult) {
        this->PlaySound(inCalamity);
        theResult = true;
    }
    inCalamity->mBoolPlaceHolder1 = false;
    return theResult;
}

// FUNCTION: EXE 0x4b0409
// Sets the quake's area around the target and sends the source effect from the caster to it; the
// travel time becomes the delay before the quake (0 without a caster). Returns true
bool EECCEarthquake::CreateSourceEffect(EECalamity* inCalamity)
{
    GERectangle theRectangle;
    GE3DPoint theLocation;
    GE3DPoint theSource;
    EEUnit* theCaster = inCalamity->mCasterUnit;
    if (theCaster != NULL) {
        inCalamity->GetTargetLocation(theLocation);
        GE2DPoint theTopLeft;
        GE2DPoint theBottomRight;
        theTopLeft.SetX(theLocation.GetX() - this->mRadius);
        theTopLeft.SetY(theLocation.GetY() - this->mRadius);
        theBottomRight.SetX(theLocation.GetX() + this->mRadius);
        theBottomRight.SetY(theLocation.GetY() + this->mRadius);
        theRectangle.SetTopLeft(theTopLeft);
        theRectangle.SetBottomRight(theBottomRight);
        inCalamity->SetRectangle(theRectangle);
        EEComplexUnit* theUnit = theCaster->mComplexUnit;
        if (theUnit != NULL) {
            GETransformation theTransformation(false);
            theTransformation = theUnit->mPhysicalModel.GetLocation();
            theSource.SetX(theTransformation.GetX());
            theSource.SetY(theTransformation.GetY());
            theSource.SetZ(theTransformation.GetZ());
            if (gWorld.GetEffectManager()->CreateEffectBetween(theSource, theLocation, this->mGraphicEffect2, NULL) == 0)
                inCalamity->mFloatPlaceHolder2 = theSource.DistanceTo(theLocation) / this->mSourceSpeed;
        }
    } else
        inCalamity->mFloatPlaceHolder2 = 0.0f;
    return true;
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
    inCalamity->StopSound();
    inCalamity->StopSecondarySound();
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

// FUNCTION: EXE 0x4b085c
// Sets how strongly the quake is felt at the camera: its volume (approach or quake) falling off
// with the camera's distance to the target, but never below the minimum
void EECCEarthquake::UpdateEpicenterFactor(EECalamity* inCalamity)
{
    GE3DPoint theViewLocation;
    GE3DPoint theTarget;
    inCalamity->GetTargetLocation(theTarget);
    GE3DPoint theOrigin;
    gViewport.GetCameraInverse().Apply(theTarget, theViewLocation);
    theViewLocation.SetY(0.0f);
    float theDistance = theViewLocation.DistanceTo(theOrigin);
    float theFactor = inCalamity->mBoolPlaceHolder1 ? this->mQuakeVolume : this->mVolume;
    float theMinimum = inCalamity->mBoolPlaceHolder1 ? this->mQuakeMinimumFactor : this->mMinimumFactor;
    theFactor *= 1.0f / (theDistance * this->mFalloff + 1.0f);
    inCalamity->mEpicenterFactor = max(theFactor, theMinimum);
}

// FUNCTION: EXE 0x4b090f
// Updates the quake sound at the approach or quake volume, clamped to 0..1
void EECCEarthquake::UpdateQuakeSound(EECalamity* inCalamity)
{
    float theVolume = inCalamity->mBoolPlaceHolder1 ? this->mQuakeVolume : this->mVolume;
    theVolume = max(0.0f, theVolume);
    theVolume = min(1.0f, theVolume);
    this->UpdateSound(inCalamity, this->mSound, theVolume, 1.0f, true);
}

// FUNCTION: EXE 0x4b0956
// Every shake interval, shakes the local player's camera in a random direction by an amount scaled by
// the epicenter factor (once per player until the flag is cleared)
void EECCEarthquake::ShakeCamera(EECalamity* inCalamity, float inElapsedTime)
{
    GE3DPoint theLocation;
    float theTime = inCalamity->GetFloatPlaceHolder1() + inElapsedTime;
    if (theTime > this->mShakeInterval) {
        theTime -= this->mShakeInterval;
        float theAngle = gPhysics->GetRandom() * 6.2831855f - 3.1415927f;
        if (inCalamity->GetEpicenterFactor() >= 0.11f) {
            EEPlayer* thePlayer = ::gWorld.GetCurrentPlayer();
            if (!thePlayer->mEarthquakeShaken) {
                thePlayer->mEarthquakeShaken = true;
                float theAmplitude = (this->mShake * inElapsedTime) * inCalamity->GetEpicenterFactor();
                ::gWorld.mCameraControl.GetLocation(::gWorld.mCameraControl.mFraction, theLocation);
                theLocation.mX += UMCos(theAngle) * theAmplitude;
                theLocation.SetY(theLocation.GetY() + UMCos(1.5707964f - theAngle) * theAmplitude);
                ::gWorld.mCameraControl.SetLocation(theLocation, true, true);
                inCalamity->mShakeLocation = theLocation;
            }
        }
    }
    inCalamity->SetFloatPlaceHolder1(theTime);
}

// FUNCTION: EXE 0x4b0a60
// Runs the quake: after the source delay draws the cracks, then each tick damages the living land
// buildings on the caster's continent within the growing radius; returns 7 once the quake is over
long EECCEarthquake::Update(EECalamity* inCalamity, float inElapsedTime)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theDamagedCarriers;
    GE3DPoint theLocation;
    EEGraphicEffect* theEffect = NULL;
    EEUnit* theCaster = inCalamity->mCasterUnit;
    unsigned short theContinent = gContinentManager.GetContinent(theCaster, kAIPathNonSpace);
    long theResult = 0;
    float theTimeAlive = inCalamity->GetTimeAlive() + inElapsedTime;
    float theElapsed = inElapsedTime * 0.001f;
    float theDelay = inCalamity->mFloatPlaceHolder2;
    float theTime = inCalamity->GetTimeAlive() * 0.001f;
    float theNewTime = theTime + theElapsed;
    float theRadius = max(0.0f, min((theTime - theDelay) * this->mSpeed, this->mRadius));
    if (theNewTime >= theDelay && !inCalamity->mBoolPlaceHolder1) {
        this->CreateCracks(inCalamity);
        inCalamity->mBoolPlaceHolder1 = true;
    }
    if (inCalamity->mBoolPlaceHolder1) {
        inCalamity->SetIntPlaceHolder1(inCalamity->mIntPlaceHolder1 + UMFloatToLong(inElapsedTime));
        if (inCalamity->GetIntPlaceHolder1() > this->mTickTime) {
            long theTicks = inCalamity->mIntPlaceHolder1 / this->mTickTime;
            long theDamage = (long)(EEGameVariant::GetGameVariant(::gWorld.mGameOptions.mVariant)->GetEarthquakeDamageFactor() * this->mDamage
                                    * theTicks);
            inCalamity->mIntPlaceHolder1 -= this->mTickTime * theTicks;
            theLocation = inCalamity->mTargetLocation;
            if (EEGetUnitsNear(theLocation, theRadius, theUnits) > 0) {
                for (long i = theUnits.size() - 1; i >= 0; i--) {
                    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(theUnits[i]);
                    if (!theUnit->mUnitClass->IsLandBuilding() || !(theUnit->mHitPoints > 0)
                        || gContinentManager.GetContinent(theUnit, kAIPathNonSpace) != theContinent)
                        continue;
                    bool theProtected = false;
                    if (this->mIsBlockedByImmunity && theUnit->GetEffect(3, theEffect)) {
                        if (theEffect != NULL)
                            gWorld.GetEffectManager()->CreateUnitEffect(theUnit, theEffect, NULL, 0, 0, 0);
                        theProtected = true;
                    }
                    bool theCarried = theUnit->IsCarried();
                    if (theProtected && !theCarried)
                        continue;
                    if (theCarried) {
                        std::vector<EEUnit*>::iterator theFound = std::find(theDamagedCarriers.begin(),
                            theDamagedCarriers.end(), theUnit->GetOutermostCarrier());
                        if (theFound != theDamagedCarriers.end())
                            continue;
                        theDamagedCarriers.push_back(theUnit->GetOutermostCarrier());
                    }
                    theUnit->TakeHit(theCaster, theDamage, inCalamity->GetPlayerNumber(), this->mIsBlockedByImmunity ? 2 : 3);
                }
            }
        }
    }
    this->UpdateEpicenterFactor(inCalamity);
    this->ShakeCamera(inCalamity, theElapsed);
    this->UpdateQuakeSound(inCalamity);
    inCalamity->SetTimeAlive(theTimeAlive);
    if (theDelay + this->mDuration < theNewTime)
        theResult = 7;
    return theResult;
}

// FUNCTION: EXE 0x4b0da5
// Accepts a map tile, on a space map only one on the caster's continent
bool EECCEarthquake::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    EEMap* theMap = ::gWorld.mMap;
    if (!theMap->mTerrainMesh.GetIsValidTile(inX, inY))
        return false;
    if (theMap->mTerrainMesh.GetIsSpaceMap()) {
        unsigned short theContinent = gContinentManager.GetContinent(inUnit, kAIPathNonSpace);
        EETileCoordinate theTile;
        theTile.mX = inX;
        theTile.mY = inY;
        return theContinent == gContinentManager.GetContinent(theTile, kAIPathNonSpace);
    }
    return true;
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
