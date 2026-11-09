// C:\EEX\Empire Earth\Calamities\EECCTimeWarp.cpp (file name inferred: no __FILE__; the class name). The time warp
// calamity class: 0xb3ade..0xb4634 (the init chains of its two colours first). Unit flags: the exe's module set.
// Virtual2C is ICF-folded (0x157269).
// Data (.rdata): kNoEffectHandle 0x43ce98, the vtable 0x43cea4 (COL 0x43cea0). (.bss): the colours 0x4e5fb8/0x4e5fc8.
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
#include "ee/EECCTimeWarp.h"
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
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// The warped unit's tint, blinking while it is away (provisional names)
static GEColor sWarpColor(0.484375f, 0.0f, 0.734375f);
static GEColor sBlackColor(0.0f, 0.0f, 0.0f);

// FUNCTION: EXE 0x4b3b42
// Creates the time warp calamity class with no effects, sounds, away time or placement range
EECCTimeWarp::EECCTimeWarp()
{
    this->mGraphicEffect1 = NULL;
    this->mGraphicEffect2 = NULL;
    this->mGraphicEffect3 = NULL;
    this->mGraphicEffect4 = NULL;
    this->mAwayTime = 0;
    this->mPlacementRange = 0;
    this->mSound = NULL;
    this->mSecondarySound = NULL;
}

// SYNTHETIC: EXE 0x4b3b6e
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b3b8a
// Destroys the calamity class, releasing its four effects
EECCTimeWarp::~EECCTimeWarp()
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Dereference();
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Dereference();
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Dereference();
    if (this->mGraphicEffect4 != NULL)
        this->mGraphicEffect4->Dereference();
}

// FUNCTION: EXE 0x4b3bd0
// Starts a time warp on the live target: disables and halts it and sends the launch effect from the caster above it;
// a dead target marks the calamity finished
bool EECCTimeWarp::Start(EECalamity* inCalamity)
{
    EEUnit* theTargetUnit = NULL;
    bool theResult = false;
    GE3DPoint theLocation;
    EEEffectHandle theEffectHandle;

    if (inCalamity->GetTargetUnit(theTargetUnit)) {
        EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
        if (theUnit->mHitPoints > 0) {
            theUnit->SetDisabled(1, 0xf);
            theUnit->Halt();
            if (this->mGraphicEffect1 != NULL && inCalamity->mCasterUnit != NULL) {
                theUnit->GetPosition(theLocation);
                theLocation.SetZ(theUnit->GetEffectHeight(true) + theLocation.GetZ() + 0.2f);
                gEffectManager->CreateEffect(inCalamity->mCasterUnit, theLocation, this->mGraphicEffect1, &theEffectHandle,
                                             0);
                inCalamity->mEffectHandle = theEffectHandle;
            }
            theResult = true;
        } else
            inCalamity->mIntPlaceHolder2 = -1;
    }
    return theResult;
}

// FUNCTION: EXE 0x4b3c89
// Gives the live target back control (goals cleared, idle or circling if it flies), removes the effect and stops the
// sounds; otherwise marks the calamity finished
void EECCTimeWarp::Stop(EECalamity* inCalamity)
{
    EEUnit* theTargetUnit = NULL;
    EEComplexUnit* theUnit;
    if (inCalamity->GetTargetUnit(theTargetUnit) && (theUnit = theTargetUnit->mComplexUnit) != 0
        && theUnit->mHitPoints > 0) {
        theUnit->SetIsTinted(false);
        theUnit->ClearBuildTargets();
        for (unsigned long i = 0; i < theUnit->GetNumGoalQueues(); i++)
            theUnit->ClearGoalQueue(i);
        theUnit->ClearGoals(0);
        theUnit->SetDisabled(0, 0);
        long theCategory = theUnit->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryAirplane)
            theUnit->StartAction(kActionCircle);
        else if (theCategory == kUnitCategoryPlanetSpaceship)  // (two tests with one tail-merged body: the true body is laid out first)
            theUnit->StartAction(kActionCircle);
        else
            theUnit->StartAction(kActionIdle);
        theUnit->mUnitClass->ActionChanged(theUnit);
        EEEffectHandle theHandle = inCalamity->mEffectHandle;
        if (theHandle != kNoEffectHandle) {
            gEffectManager->DestroyEffect(theHandle);
            inCalamity->mEffectHandle = kNoEffectHandle;
        }
    } else
        inCalamity->mIntPlaceHolder2 = -1;
    inCalamity->StopSound();
    inCalamity->StopSecondarySound();
}

// FUNCTION: EXE 0x4b3d7c
// Reads the calamity data: sounds, placement range and four effects; depart and away times follow the first two
// effects' durations (away 3000 without one)
void EECCTimeWarp::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mSound = EESoundObject::Get(inData->mSoundID);
    this->mSecondarySound = EESoundObject::Get(inData->mSecondarySoundID);
    this->mPlacementRange = inData->mIntParam1;
    this->mAwayTime = inData->mIntParam2;
    if (inData->mGraphicEffectID1 != 0) {
        this->mGraphicEffect1 = EEGraphicEffect::Get(inData->mGraphicEffectID1);
        this->mGraphicEffect1->Reference();
        this->mDepartTime = this->mGraphicEffect1->mDuration;
    }
    if (inData->mGraphicEffectID2 != 0) {
        this->mGraphicEffect2 = EEGraphicEffect::Get(inData->mGraphicEffectID2);
        this->mGraphicEffect2->Reference();
        this->mAwayTime = this->mGraphicEffect2->mDuration;
    } else
        this->mAwayTime = 3000;
    if (inData->mGraphicEffectID3 != 0) {
        this->mGraphicEffect3 = EEGraphicEffect::Get(inData->mGraphicEffectID3);
        this->mGraphicEffect3->Reference();
    }
    if (inData->mGraphicEffectID4 != 0) {
        this->mGraphicEffect4 = EEGraphicEffect::Get(inData->mGraphicEffectID4);
        this->mGraphicEffect4->Reference();
    }
}

// FUNCTION: EXE 0x4b3e36
// Remembers the target's tile to return it there, or marks the calamity finished if the target is off the map or dead
void EECCTimeWarp::Reset(EECalamity* inCalamity)
{
    EEUnit* theTargetUnit = NULL;
    if (inCalamity->GetTargetUnit(theTargetUnit)) {
        EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
        long theTileX = theUnit->mTileX;
        if (theTileX != -10000 && theUnit->mHitPoints > 0) {
            inCalamity->mTilePlaceHolder1.SetXY(theTileX, theUnit->mTileY);
            inCalamity->mBoolPlaceHolder1 = false;
            inCalamity->mBoolPlaceHolder2 = false;
        } else {
            inCalamity->mIntPlaceHolder2 = -1;
            inCalamity->mBoolPlaceHolder1 = true;
            inCalamity->mBoolPlaceHolder2 = true;
        }
    }
    inCalamity->mFloatPlaceHolder1 = 0.0f;
}

// FUNCTION: EXE 0x4b3e9d
// Loads the four effects and both sounds (all assumed present)
void EECCTimeWarp::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    this->mGraphicEffect1->Load(inIsOnDemand);
    this->mGraphicEffect2->Load(inIsOnDemand);
    this->mGraphicEffect3->Load(inIsOnDemand);
    this->mGraphicEffect4->Load(inIsOnDemand);
    this->mSound->LoadMedia();
    this->mSecondarySound->LoadMedia();
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCTimeWarp::PlaySound(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4b3ede
// Places the departure or arrival effect at the location and plays the main sound when departing (as the calamity's
// main sound), the secondary sound when arriving
void EECCTimeWarp::PlayEffect(GE3DPoint& inLocation, EECalamity* inCalamity, bool inDeparting)
{
    EEEffectHandle theEffectHandle;
    if (inDeparting) {
        if (this->mGraphicEffect2 != NULL) {
            gEffectManager->CreatePlacedEffect(inLocation, this->mGraphicEffect2, &theEffectHandle);
            inCalamity->mEffectHandle = theEffectHandle;
        }
        // the departure plays the main sound (the secondary one used to play here too, so the main sound was never
        // heard); without a main sound the secondary one plays, as before
        EESoundObject* theSound = this->mSound != NULL ? this->mSound : this->mSecondarySound;
        inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(),
                                                                        theSound->GetVolume(), 0.0f, 1.0f,
                                                                        theSound->GetLooping(), NULL));
        if (inCalamity->mSound != NULL)
            inCalamity->mSound->Dereference();
    } else {
        if (this->mGraphicEffect3 != NULL) {
            gEffectManager->CreatePlacedEffect(inLocation, this->mGraphicEffect3, &theEffectHandle);
            inCalamity->mEffectHandle = theEffectHandle;
        }
        inCalamity->SetSecondarySound(gSoundManager->PlayAndReturnReferencedNode(this->mSecondarySound->GetSampleID(),
                                                                                 this->mSecondarySound->GetVolume(), 0.0f,
                                                                                 1.0f, this->mSecondarySound->GetLooping(),
                                                                                 NULL));
        if (inCalamity->mSecondarySound != NULL)
            inCalamity->mSecondarySound->Dereference();
    }
}

// FUNCTION: EXE 0x4b3fc9
// Brings the target back near its old tile (at its old height above ground if it flies, facing as before) with the
// arrival effects; kills it if no place is found
void EECCTimeWarp::Return(EECalamity* inCalamity)
{
    GETransformation theTransformation;
    GE3DPoint theLocation;
    EEUnit* theTargetUnit;
    GE3DPoint theFoundLocation;
    if (!inCalamity->GetTargetUnit(theTargetUnit))
        return;
    EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
    theUnit->SetIsTinted(true);
    if (EEFindPlacement(&theFoundLocation, theUnit, NULL, inCalamity->mTilePlaceHolder1.mX,
                        inCalamity->mTilePlaceHolder1.mY, theUnit->mUnitClass->GetTileSize(), this->mPlacementRange, 1, 1, 0, 0,
                        &theTransformation, NULL)) {
        long theY = UMFloor(theTransformation.GetY());
        long theX = UMFloor(theTransformation.GetX());
        bool theValid = ::gWorld.GetMap()->mTerrainMesh.GetIsValidTile(theX, theY);
        if (theUnit->mUnitClass->mDomains & kMovementAir) {
            float theGround;
            if (theValid)
                theGround = ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(theTransformation.GetX(), theTransformation.GetY());
            else
                theGround = 0.0f;
            theTransformation.SetZ(inCalamity->GetFloatPlaceHolder1() + theGround);
        } else if ((theUnit->mUnitClass->mDomains & kMovementSea) && theValid) {
            // put the ship back on the water surface
            GE3DPoint theGroundPoint(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
            theUnit->mUnitClass->GroundLocation(theGroundPoint);
            theTransformation.SetZ(theGroundPoint.GetZ());
        }
        theLocation.SetX(theTransformation.GetX());
        theLocation.SetY(theTransformation.GetY());
        theLocation.SetZ(theTransformation.GetZ());
        theLocation.SetZ(theUnit->GetEffectHeight(true) * 0.5f + theLocation.GetZ());
        gEffectManager->CreatePlacedEffect(theLocation, this->mGraphicEffect4, NULL);
        theLocation.SetX(theTransformation.GetX());
        theLocation.SetY(theTransformation.GetY());
        theLocation.SetZ(theTransformation.GetZ());
        theLocation.SetZ(theUnit->GetEffectHeight(true) + theLocation.GetZ() + 0.2f);
        this->PlayEffect(theLocation, inCalamity, false);
        theTransformation.SetOrientationYPR(inCalamity->mFloatPlaceHolder2, 0.0f, 0.0f);
        theUnit->SetTransformation(theTransformation, true);
    } else
        theUnit->Kill(1, 0, 0);
}

// FUNCTION: EXE 0x4b41dd
// Takes the live target off the map (off-map reason 3), remembering its height above ground, facing and position;
// otherwise marks the calamity finished
void EECCTimeWarp::Depart(EECalamity* inCalamity)
{
    GE3DPoint thePosition;
    EEUnit* theTargetUnit = NULL;
    GEOrientation theOrientation;
    if (!inCalamity->GetTargetUnit(theTargetUnit))
        return;
    EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
    if (theUnit->mTileX != -10000 && theUnit->mHitPoints > 0) {
        theUnit->SetIsTinted(false);
        theUnit->GetPosition(thePosition);
        long theY = UMFloor(thePosition.GetY());
        long theX = UMFloor(thePosition.GetX());
        float theGround;
        if (::gWorld.GetMap()->mTerrainMesh.GetIsValidTile(theX, theY))
            theGround = ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePosition.GetX(), thePosition.GetY());
        else
            theGround = 0.0f;
        inCalamity->mFloatPlaceHolder1 = thePosition.GetZ() - theGround;
        theUnit->mPhysicalModel.GetLocation().ConvertToOrientation(theOrientation);
        inCalamity->SetFloatPlaceHolder2(theOrientation.GetYaw());
        inCalamity->mTargetLocation = thePosition;
        theUnit->TakeOffMap();
        EEWUnitsOffMapInfo* theInfo = theUnit->mPlayer->GetOffMapInfo(theUnit);
        if (theInfo != NULL)
            theInfo->mReason = 3;
    } else {
        inCalamity->mIntPlaceHolder2 = -1;
        inCalamity->mBoolPlaceHolder1 = true;
        inCalamity->mBoolPlaceHolder2 = true;
    }
}

// FUNCTION: EXE 0x4b432a
// Unloads the four effects and both sounds
void EECCTimeWarp::UnloadMedia(EEPlayer* inPlayer)
{
    this->mGraphicEffect1->Unload();
    this->mGraphicEffect2->Unload();
    this->mGraphicEffect3->Unload();
    this->mGraphicEffect4->Unload();
    this->mSound->UnloadMedia();
    this->mSecondarySound->UnloadMedia();
}

// FUNCTION: EXE 0x4b4361
// Ages the calamity: tints and departs the target, brings it back before the end, blinks its colour while tinted;
// returns 7 once the target is dead or the calamity is marked finished
long EECCTimeWarp::Update(EECalamity* inCalamity, float inElapsedTime)
{
    GE3DPoint theLocation;
    long theResult = EECalamityClass::UpdateTimeAlive(inCalamity, inElapsedTime, -1);
    EEUnit* theTargetUnit;
    inCalamity->GetTargetUnit(theTargetUnit);
    EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
    if (this->mSecondarySound != NULL) {
        this->UpdateSound(inCalamity, this->mSecondarySound, -1.0f, 15.0f, true);
        this->UpdateSound(inCalamity, this->mSecondarySound, -1.0f, 15.0f, false);
    }
    if (theUnit->mHitPoints > 0) {
        if (inCalamity->GetTimeAlive() - inElapsedTime < this->mDepartTime && inCalamity->GetTimeAlive() >= this->mDepartTime) {
            theUnit->GetPosition(theLocation);
            theLocation.SetZ(theUnit->GetEffectHeight(true) + theLocation.GetZ() + 0.2f);
            theUnit->SetIsTinted(true);
            this->PlayEffect(theLocation, inCalamity, true);
        }
        if (inCalamity->GetTimeAlive() >= this->mAwayTime && !inCalamity->mBoolPlaceHolder1) {
            theUnit->GetPosition(theLocation);
            theLocation.SetZ(theUnit->GetEffectHeight(true) * 0.5f + theLocation.GetZ());
            gEffectManager->CreatePlacedEffect(theLocation, this->mGraphicEffect4, NULL);
            if (inCalamity->mEffectHandle != kNoEffectHandle) {
                gEffectManager->DestroyEffect(inCalamity->mEffectHandle);
                inCalamity->mEffectHandle = kNoEffectHandle;
            }
            this->Depart(inCalamity);
            inCalamity->mBoolPlaceHolder1 = true;
        }
        if (inCalamity->GetTimeAlive() >= this->mLifeTime - this->mAwayTime && !inCalamity->mBoolPlaceHolder2) {
            this->Return(inCalamity);
            inCalamity->mBoolPlaceHolder2 = true;
        }
        if (theUnit->mIsTinted) {
            inCalamity->mFloatPlaceHolder1 -= inElapsedTime;
            if (inCalamity->mFloatPlaceHolder1 <= 0.0f) {
                if (theUnit->mColor == sWarpColor)
                    theUnit->SetColor(sBlackColor);
                else
                    theUnit->SetColor(sWarpColor);
                if (inCalamity->GetTimeAlive() > this->mAwayTime * 0.5f)
                    inCalamity->mFloatPlaceHolder1 = this->mAwayTime * 0.025f;
                else if (inCalamity->GetTimeAlive() > 0.0f)
                    inCalamity->mFloatPlaceHolder1 = this->mAwayTime * 0.1f;
                else
                    inCalamity->mFloatPlaceHolder1 = this->mAwayTime * 0.1f;
            }
        }
    } else
        inCalamity->mIntPlaceHolder2 = -1;
    if (inCalamity->mIntPlaceHolder2 == -1)
        theResult = 7;
    return theResult;
}

// Compiler-generated for this unit (EEComplexUnit::SetColor, inline, first emitted here):
// TEMPLATE: EXE 0x4b45b0

// FUNCTION: EXE 0x4b45fa
// Accepts another unit on the map that is in the class's unit set
bool EECCTimeWarp::IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit)
{
    bool theResult = false;
    if (inTargetUnit->mTileNode != NULL && inTargetUnit != inUnit
        && gUnitSetManager.Contains(inTargetUnit->mUnitClass->GetUnitType(), this->GetUnitSet()))
        theResult = true;
    return theResult;
}

// SYNTHETIC: EXE 0x4b3ade
// SYNTHETIC: EXE 0x4b3ae8
// SYNTHETIC: EXE 0x4b3b05
// SYNTHETIC: EXE 0x4b3b11
// SYNTHETIC: EXE 0x4b3b12
// SYNTHETIC: EXE 0x4b3b1c
// SYNTHETIC: EXE 0x4b3b35
// SYNTHETIC: EXE 0x4b3b41
