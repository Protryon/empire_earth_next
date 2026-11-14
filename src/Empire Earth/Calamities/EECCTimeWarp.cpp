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
#include "ee/EETechCapture.h"
#include "ee/EECalamityUpdate.h"
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

// New (port): EECCTimeWarp's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCTimeWarp::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCTimeWarp* theClass = static_cast<EECCTimeWarp*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::WarpTint: {
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
        GE3DPoint theLocation;
        theUnit->GetPosition(theLocation);
        theLocation.SetZ(theUnit->GetEffectHeight(true) + theLocation.GetZ() + 0.2f);
        EECalamityCaptureEvent(kCalamityOpWarpTint);   // CALW (TEST ONLY)
        theUnit->SetIsTinted(true);
        theClass->PlayEffect(theLocation, theCalamity, true);
        return true;
    }
    case EeCalamityOp::WarpAwayEffect: {
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
        GE3DPoint theLocation;
        theUnit->GetPosition(theLocation);
        theLocation.SetZ(theUnit->GetEffectHeight(true) * 0.5f + theLocation.GetZ());
        gEffectManager->CreatePlacedEffect(theLocation, theClass->mGraphicEffect4, NULL);
        return true;
    }
    case EeCalamityOp::WarpDepart: {
        EECalamityCaptureEvent(kCalamityOpWarpDepart);   // CALW (TEST ONLY)
        theClass->Depart(theCalamity);
        return true;
    }
    case EeCalamityOp::WarpReturn: {
        EECalamityCaptureEvent(kCalamityOpWarpReturn);   // CALW (TEST ONLY)
        theClass->Return(theCalamity);
        return true;
    }
    case EeCalamityOp::WarpBlink: {
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
        EECalamityCaptureEvent(kCalamityOpWarpBlink);   // CALW (TEST ONLY)
        if (theUnit->mColor == sWarpColor)
            theUnit->SetColor(sBlackColor);
        else
            theUnit->SetColor(sWarpColor);
        return true;
    }
    default:
        return false;
    }
}

// Compiler-generated for this unit (EEComplexUnit::SetColor, inline, first emitted here):
// TEMPLATE: EXE 0x4b45b0

// SYNTHETIC: EXE 0x4b3ade
// SYNTHETIC: EXE 0x4b3ae8
// SYNTHETIC: EXE 0x4b3b05
// SYNTHETIC: EXE 0x4b3b11
// SYNTHETIC: EXE 0x4b3b12
// SYNTHETIC: EXE 0x4b3b1c
// SYNTHETIC: EXE 0x4b3b35
// SYNTHETIC: EXE 0x4b3b41

// New (port): EECCTimeWarp's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCTimeWarp::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCTimeWarp* theClass = static_cast<EECCTimeWarp*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        theTargetUnit->mComplexUnit->SetDisabled(1, 0xf);
        theTargetUnit->mComplexUnit->Halt();
        return true;
    }
    case 2: {
        // the warp's effect above the unit
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
        GE3DPoint theLocation;
        EEEffectHandle theEffectHandle;
        theUnit->GetPosition(theLocation);
        theLocation.SetZ(theUnit->GetEffectHeight(true) + theLocation.GetZ() + 0.2f);
        EECalamityCaptureStartPoint(1, theLocation.mX, theLocation.mY, theLocation.mZ);   // CALT (TEST ONLY)
        gEffectManager->CreateEffect(theCalamity->mCasterUnit, theLocation, theClass->mGraphicEffect1, &theEffectHandle, 0);
        outReply.id = theEffectHandle.mValue;
        return true;
    }
    case 3: {
        // untinted, its orders cleared, enabled; its category
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
        theUnit->SetIsTinted(false);
        theUnit->ClearBuildTargets();
        for (unsigned long i = 0; i < theUnit->GetNumGoalQueues(); i++)
            theUnit->ClearGoalQueue(i);
        theUnit->ClearGoals(0);
        theUnit->SetDisabled(0, 0);
        outReply.value = (int32_t)theUnit->mUnitClass->mCategory;
        return true;
    }
    case 4: {
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
        theUnit->StartAction(inCall.value);
        theUnit->mUnitClass->ActionChanged(theUnit);
        return true;
    }
    default:
        return false;
    }
}
