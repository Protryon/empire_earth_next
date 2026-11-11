// C:\EEX\Empire Earth\Calamities\EECCHurricane.cpp (file name inferred: no __FILE__; the class name). The hurricane
// calamity class: 0xb114a..0xb18e2. Unit flags: the exe's module set.
// Data (.rdata): the vtable 0x43ca1c (COL 0x43ca18).
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
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EECCHurricane.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEGameSound.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEEffectKindIDs.h"

// FUNCTION: EXE 0x4b114a
// Creates the hurricane calamity class with no effects or sound
EECCHurricane::EECCHurricane()
{
    this->mGraphicEffect2 = NULL;
    this->mGraphicEffect3 = NULL;
    this->mGraphicEffect1 = NULL;
    this->mSound = NULL;
}

// SYNTHETIC: EXE 0x4b116a
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b1186
// Destroys the calamity class, releasing its three effects
EECCHurricane::~EECCHurricane()
{
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Dereference();
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Dereference();
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Dereference();
}

// FUNCTION: EXE 0x4b11bf
// Starts a hurricane: takes the storm effect's size as the calamity height and sends the launch effect from the caster
// to above the target; always returns true
bool EECCHurricane::Start(EECalamity* inCalamity)
{
    GE3DPoint theLocation;
    inCalamity->mHeight = this->mGraphicEffect2->GetSize();
    if (this->mGraphicEffect1 != NULL && inCalamity->mCasterUnit != NULL) {
        theLocation = inCalamity->mTargetLocation;
        theLocation.SetZ(theLocation.GetZ() + inCalamity->GetHeight());
        gEffectManager->CreateEffect(inCalamity->mCasterUnit, theLocation, this->mGraphicEffect1, NULL, 0);
    }
    return true;
}

// FUNCTION: EXE 0x4b1226
// Creates the storm unit centred on the target location at the calamity height, with the storm effect on it, and makes
// it the calamity's target; returns false if the unit could not be created
bool EECCHurricane::CreateStorm(EECalamity* inCalamity)
{
    GETransformation theTransformation;
    bool theResult = false;
    GE3DPoint theLocation;
    theLocation = inCalamity->mTargetLocation;
    float theHalfSize = gUnitClassTable[this->mUnitClassIndex]->mTileSize * 0.5f;
    theLocation.SetX(theLocation.GetX() - theHalfSize);
    theLocation.SetY(theLocation.GetY() - theHalfSize);
    theLocation.SetZ(inCalamity->mHeight);
    theTransformation.SetX(theLocation.GetX());
    theTransformation.SetY(theLocation.GetY());
    theTransformation.SetZ(theLocation.GetZ());
    EEUnit* theUnit = EECreateCalamityUnit(inCalamity, this->mUnitClassIndex, theTransformation, -1);
    if (theUnit != NULL) {
        gEffectManager->CreateUnitEffect(theUnit, this->mGraphicEffect2, NULL, 0, 0, 0);
        theUnit->mComplexUnit->SetBuilt(1, 0, 1);
        inCalamity->SetTargetUnit(theUnit);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4b12d4
// Stops the hurricane's sounds and kills and deselects its storm unit
void EECCHurricane::Stop(EECalamity* inCalamity)
{
    EEUnit* theStorm = NULL;
    inCalamity->StopSound();
    inCalamity->StopSecondarySound();
    inCalamity->GetTargetUnit(theStorm);
    if (theStorm == NULL)
        return;
    theStorm->Kill(0, 0, 0);
    theStorm->mPlayer->DeselectUnit(theStorm);
    inCalamity->SetTargetUnit(NULL);
}

// FUNCTION: EXE 0x4b1324
// Reads the calamity data: three effects, sound, damage, area, tick time, radius, storm unit class and water-only flag
void EECCHurricane::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
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
    this->mSound = EESoundObject::Get(inData->mSoundID);
    this->mDamage = inData->mIntParam1;
    this->mArea = inData->mIntParam6;
    this->mTickTime = inData->mIntParam2;
    this->mRadius = inData->mFloatParam1;
    this->mUnitClassIndex = inData->mUnitClassIndex;
    this->mOverWater = inData->mBoolParam1;
}

// FUNCTION: EXE 0x4b13be
// Clears the calamity's damage-tick timer
void EECCHurricane::Reset(EECalamity* inCalamity)
{
    inCalamity->mIntPlaceHolder1 = 0;
}

// FUNCTION: EXE 0x4b13c9
// Loads the storm and strike effects, the sound and the storm unit class's media and buttons
void EECCHurricane::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    this->mGraphicEffect2->Load(inIsOnDemand);
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Load(inIsOnDemand);
    this->mSound->LoadMedia();
    if (inIsOnDemand == true)
        inPlayer->LoadUnitClassMedia(this->mUnitClassIndex);
    inPlayer->SetUnitClassMedia(this->mUnitClassIndex, 0x7f);
    inPlayer->LoadUnitClassButtons(this->mUnitClassIndex);
}

// FUNCTION: EXE 0x4b141f
// Starts the hurricane's sound as the calamity's sound
void EECCHurricane::PlaySound(EECalamity* inCalamity)
{
    inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), this->mSound->GetVolume(), 0.0f,
                                                                    1.0f, this->mSound->GetLooping(), NULL));
    if (inCalamity->mSound != NULL)
        inCalamity->mSound->Dereference();
}

// FUNCTION: EXE 0x4b1480
// Unloads the storm and strike effects, the sound and the storm unit class's media and buttons
void EECCHurricane::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Unload();
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Unload();
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
    inPlayer->UnloadUnitClassMedia(this->mUnitClassIndex);
    inPlayer->UnloadUnitClassButtons(this->mUnitClassIndex);
}

// FUNCTION: EXE 0x4b14c3
// After the launch effect creates the storm (or updates its sound), follows the storm unit and every tick time damages
// living enemy units of the class's unit set within reach, with a strike effect each; returns the time-alive result
long EECCHurricane::Update(EECalamity* inCalamity, float inElapsedTime)
{
    std::vector<EEUnit*> theUnits;
    GE3DPoint theLocation;
    EEPlayer* theNaturePlayer = ::gWorld.mPlayers[0];
    EEUnit* theStorm = NULL;
    EEGraphicEffect* theEffect = NULL;
    if (this->mGraphicEffect1 == NULL || inCalamity->GetTimeAlive() >= this->mGraphicEffect1->mDuration) {
        if (!inCalamity->GetTargetUnit(theStorm)) {
            bool theCreated = this->CreateStorm(inCalamity);
            EECalamityCaptureEvent(kCalamityOpCreateStorm, theCreated);   // CALW (TEST ONLY)
            this->PlaySound(inCalamity);
        } else
            this->UpdateSound(inCalamity, this->mSound, -1.0f, 4.0f, true);
        inCalamity->GetTargetUnit(theStorm);
        EEComplexUnit* theStormUnit = theStorm->mComplexUnit;
        theStormUnit->SetIsDeletable(false);
        const GETransformation& theStormLocation = theStormUnit->mPhysicalModel.GetLocation();
        theLocation.SetX(theStormLocation.GetX());
        theLocation.SetY(theStormLocation.GetY());
        theLocation.SetZ(theStormLocation.GetZ());
        inCalamity->mTargetLocation = theLocation;
        float theTime = inCalamity->mIntPlaceHolder1 + inElapsedTime;
        inCalamity->mIntPlaceHolder1 = UMFloatToLong(theTime);
        if (theTime >= this->mTickTime) {
            long theTicks = UMFloatToLong(theTime / this->mTickTime);
            long theNear = EEGetUnitsNear(theLocation, this->mRadius, theUnits);
            EECalamityCaptureEvent(kCalamityOpUnitsNear, theNear);   // CALW (TEST ONLY)
            if (theNear > 0) {
                long theDamage = this->mDamage * theTicks;
                for (long i = theUnits.size() - 1; i >= 0; i--) {
                    EEUnit* theUnit = theUnits[i];
                    if (theUnit->mUnitID == theStorm->mUnitID || theUnit->mPlayer == theStormUnit->mPlayer
                        || theStormUnit->mPlayer->mEnemies[theUnit->mPlayer->mPlayerID] == 0
                        || !gUnitSetManager.Contains(theUnit->mUnitClass->GetUnitType(), this->GetUnitSet())
                        || !(theUnit->mHitPoints > 0))
                        continue;
                    float theDistance = EEGetDistanceSquared(static_cast<EEComplexUnit*>(theUnit), theStormUnit);
                    float theReach = EEGetUnitRadius(static_cast<EEComplexUnit*>(theUnit), theStormUnit) + this->mRadius;
                    if (!(theReach * theReach > theDistance))
                        continue;
                    EECalamityCaptureEvent(kCalamityOpStrike, theUnit->mUnitID);   // CALW (TEST ONLY)
                    if (this->mGraphicEffect3->mKind == kEffect2DLightning)
                        gEffectManager->CreateUnitEffect(theUnit, this->mGraphicEffect3, NULL, 0, 0, 0);
                    else
                        gEffectManager->CreateEffectBetweenUnits(theStormUnit, theUnit, this->mGraphicEffect3, NULL);
                    if (theUnit->mPlayer == theNaturePlayer)
                        continue;
                    if (this->mIsBlockedByImmunity && static_cast<EEComplexUnit*>(theUnit)->GetEffect(3, theEffect)) {
                        EECalamityCaptureEvent(kCalamityOpImmune, theUnit->mUnitID);   // CALW (TEST ONLY)
                        if (theEffect != NULL)
                            gEffectManager->CreateUnitEffect(theUnit, theEffect, NULL, 0, 0, 0);
                    } else {
                        EECalamityCaptureEvent(kCalamityOpTakeHit, theUnit->mUnitID, theDamage, (float)inCalamity->mPlayerNumber,
                                               this->mIsBlockedByImmunity ? 2.0f : 3.0f);   // CALW (TEST ONLY)
                        theUnit->TakeHit(theStorm, theDamage, inCalamity->mPlayerNumber, this->mIsBlockedByImmunity ? 2 : 3);
                    }
                }
            }
            inCalamity->mIntPlaceHolder1 = UMFloatToLong(theTime - (float)this->mTickTime * theTicks);
        }
    }
    return EECalamityClass::UpdateTimeAlive(inCalamity, inElapsedTime, -1);
}

// FUNCTION: EXE 0x4b17cb
// Accepts a tile (water only if the class is water-only, never terrain 12) whose area is on the map and has no live
// hurricane or anti-matter storm nearby
bool EECCHurricane::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    std::vector<EEUnit*> theUnits;
    bool theResult;
    if (this->mOverWater) {
        theResult = ::gWorld.IsTileBlocked(inX, inY);
        if (!theResult)
            return theResult;
    }
    theResult = ::gWorld.mMap->GetTile(inX, inY)->mTerrainType != 12;
    if (!theResult)
        return theResult;
    theResult = ::gWorld.mMap->IsAreaOnMap(inX, inY, this->mArea);
    if (theResult) {
        EEGetUnitsNear(GE3DPoint((long)inX + 0.5f, (long)inY + 0.5f, 0.0f),
                       gUnitClassTable[this->mUnitClassIndex]->GetInnerCollisionSphere() * 1.5f, theUnits);
        for (long i = theUnits.size() - 1; i >= 0; i--) {
            EEUnit* theUnit = theUnits[i];
            if (theUnit->mHitPoints > 0
                && (theUnit->mUnitClass->GetUnitType() == kUnitTypeHurricane || theUnit->mUnitClass->GetUnitType() == kUnitTypeAntiMatterStorm)) {
                theResult = false;
                break;
            }
        }
    }
    return theResult;
}
