// C:\EEX\Empire Earth\Units\EEUCAirAnimal.cpp (file name inferred: no __FILE__; the class name). The flying animal's
// unit class: 0x20a279..0x20b192 (after EESelectedGroup.cpp's sort COMDATs; EEUCAirplane.cpp follows). One unit per
// EEUC* class in alphabetical order (AirAnimal, Airplane, Ambient, BasicCitizen, BasicPriest, Bomb, Building, ...).
// Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); ReturnToAirport (the landing on an
// airport, used by Virtual74) comes first, then the slots +0x24, +0x2c, +0x74, +0x78, +0x88, +0xa0, (+0xc0 folded)
// +0xb8 in source order. Bodies are the EEUCLandable twins' (0x213ed2.., same sizes) with the animal's states.
#include <windows.h>
#include <stdio.h>
#include <math.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"
#include "lle/U2DPoint.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUCAirAnimal.h"

// 0x446fc0..0x446fc8: the unit's named float constants (one .rdata block per unit)
static const float kAirAnimalHeight = 12.0f;     // 0x446fc0 (the highest an animal flies)
static const float kAirAnimalAhead = 2.0f;       // 0x446fc4 (the look-ahead beyond the step)
static const float kAirAnimalDoppler = 0.0025f;  // 0x446fc8 (0.05 squared)

// FUNCTION: EXE 0x60a279
// Lands the animal like a plane: two waypoints ahead (on the ground, then at 3/4 of cruise altitude) and a flight there
bool EEUCAirAnimal::ReturnToAirport(EEComplexUnit* inUnit)
{
    GE3DPoint thePoint;
    GEOrientation theOrientation;
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    if (inUnit == NULL || inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1] == 0)
        return true;
    inUnit->DropPath();
    theTransformation.Apply(GE3DPoint(0.0f, 3.0f, 0.0f), thePoint);
    this->AddWaypointAt(inUnit, thePoint.GetX(), thePoint.GetY(), inUnit->mTile->GetHeight(), true);
    theTransformation.Apply(GE3DPoint(0.0f, 7.0f, 0.0f), thePoint);
    float theHeight = this->GetCruiseAltitude() * 0.75f;
    if (inUnit->mTile->GetHeight() > theHeight)
        theHeight = inUnit->mTile->GetHeight() + theHeight;
    this->AddWaypointAt(inUnit, thePoint.GetX(), thePoint.GetY(), theHeight, true);
    thePoint.SetZ(theHeight);
    theTransformation.ConvertToOrientation(theOrientation);
    inUnit->mOrientation = theOrientation;
    this->FlyToGoal(thePoint, inUnit);
    return true;
}

// FUNCTION: EXE 0x60a3c0
// Returns the height to fly at toward a point: its height above ground kept over the highest terrain on the way
// (the circle ahead when circling, else the tile line), capped at 12
float EEUCAirAnimal::GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    GE3DPoint thePosition;
    GEVector theStep;
    GEVector theOffset;
    std::vector<EETileCoordinate> theTiles;
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    thePosition.SetX(theLocation.GetX());
    EEMap* theMap = ::gWorld.mMap;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    thePosition.SetY(theLocation.GetY());
    thePosition.SetZ(theLocation.GetZ());
    float theHeight = inLocation.GetZ();
    float theBase;
    EEMapTile* theTile = ::gWorld.mMap->GetTile(UMFloor(inLocation.GetX()), UMFloor(inLocation.GetY()));
    if (theTile != NULL && theTile->GetHeight() >= 0.0f)
        theBase = theHeight - theTile->GetHeight();
    else
        theBase = theHeight;
    unsigned long theState = inUnit->mCurrentPhysicsOrder;
    if (theState == 0 || theState == 1) {
        float theDistance = theStats->GetMaxVelocity() / theStats->GetMaxAngularVelocity();
        theStep.SetX(theState != 0 ? -theDistance : theDistance);
        theLocation.Apply(theStep, theOffset);
        thePosition += theOffset;
        Fn24B212(thePosition, kAirAnimalAhead + theDistance, theTiles);
    } else {
        EEGetTileLine(UMFloor(thePosition.GetX()), UMFloor(thePosition.GetY()), UMFloor(inLocation.GetX()),
                   UMFloor(inLocation.GetY()), (std::vector<U2DPoint<long> >*)&theTiles, 1, 0);
    }
    for (std::vector<EETileCoordinate>::iterator theCoordinate = theTiles.begin(); theCoordinate != theTiles.end();
         theCoordinate++) {
        unsigned long theX = theCoordinate->mX;
        unsigned long theY = theCoordinate->mY;
        if (theMap->GetTile(theX, theY) == NULL)
            continue;
        float theZ = theMap->mTerrainMesh.GetZ(theX, theY) + theBase;
        if (theZ > theHeight)
            theHeight = theZ;
        theZ = theMap->mTerrainMesh.GetZ(theX + 1, theY) + theBase;
        if (theZ > theHeight)
            theHeight = theZ;
        theZ = theMap->mTerrainMesh.GetZ(theX, theY + 1) + theBase;
        if (theZ > theHeight)
            theHeight = theZ;
        theZ = theMap->mTerrainMesh.GetZ(theX + 1, theY + 1) + theBase;
        if (theZ > theHeight)
            theHeight = theZ;
    }
    if (theHeight > kAirAnimalHeight)
        theHeight = kAirAnimalHeight;
    return theHeight;
}

// FUNCTION: EXE 0x60a691
// Gets the base sound parameters, then Doppler-shifts the pitch of a moving animal by its speed toward the camera
void EEUCAirAnimal::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                              float* outPitch)
{
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    GE3DPoint thePosition;
    GE3DPoint theCamera(::gWorld.mTransformation.GetX(), ::gWorld.mTransformation.GetY(),
                        ::gWorld.mTransformation.GetZ());
    float theSpeed = theUnit->mVelocity;
    GEVector theVelocity(0.0f, theSpeed, 0.0f);
    GEVector theHeading;
    GEVector theToCamera;
    EEUnitClass::GetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    if (!(*outVolume > 0.0f) || !(theSpeed > 0.0f))
        return;
    thePosition.SetX(theUnit->mPhysicalModel.GetLocation().GetX());
    thePosition.SetY(theUnit->mPhysicalModel.GetLocation().GetY());
    thePosition.SetZ(theUnit->mPhysicalModel.GetLocation().GetZ());
    theToCamera.SetX(theCamera.GetX() - thePosition.GetX());
    theToCamera.SetY(theCamera.GetY() - thePosition.GetY());
    theToCamera.SetZ(theCamera.GetZ() - thePosition.GetZ());
    if (theToCamera.GetX() >= -0.0001f && theToCamera.GetX() <= 0.0001f && theToCamera.GetY() >= -0.0001f &&
        theToCamera.GetY() <= 0.0001f && theToCamera.GetZ() >= -0.0001f && theToCamera.GetZ() <= 0.0001f) {
        theToCamera.SetX(0.0f);
        theToCamera.SetY(0.0f);
        theToCamera.SetZ(0.0f);
    } else
        theToCamera.Normalize();
    theUnit->mPhysicalModel.GetLocation().Apply(theVelocity, theHeading);
    thePosition += theHeading;
    if (theHeading.GetX() >= -0.0001f && theHeading.GetX() <= 0.0001f && theHeading.GetY() >= -0.0001f &&
        theHeading.GetY() <= 0.0001f && theHeading.GetZ() >= -0.0001f && theHeading.GetZ() <= 0.0001f) {
        theHeading.SetX(0.0f);
        theHeading.SetY(0.0f);
        theHeading.SetZ(0.0f);
    } else
        theHeading.Normalize();
    float theCosine = theHeading.DotProduct(theToCamera);
    float theRatio = theVelocity.GetY() * 0.05f;
    float theGamma = (float)sqrt(1.0f - theVelocity.GetY() * theVelocity.GetY() * kAirAnimalDoppler);
    *outPitch = theGamma / (1.0f - theRatio * ((theRatio + theCosine) / (1.0f - theRatio * theCosine)));
}

// FUNCTION: EXE 0x60a8de
// Sets up the animal's flight for its new action (circle left or right at random, follow waypoints, take off, die,
// path to the goal); dying spins it with the roll set to its yaw rate; other actions use the common handling
void EEUCAirAnimal::ActionChanged(EEComplexUnit* inUnit)
{
    GE3DPoint theTarget;
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    EEUnitGoal* theGoal = inUnit->mGoalState;
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionTaxi:
    case kActionClimb:
    case kActionFly:
    case kActionDescend:
    case kActionBrake:
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetCurrentPhysicsOrder(5);
        inUnit->SetHasArrived(false);
        inUnit->SetF220(0.0f);
        return;
    case kActionCircle:
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetHasArrived(false);
        inUnit->SetF220(0.0f);
        if (theGoal != NULL && theGoal->mKind != kGoalExplore)
            inUnit->ClearGoals(0);
        if (gPhysics->GetRandom() < 0.5f)
            inUnit->SetCurrentPhysicsOrder(0);
        else
            inUnit->SetCurrentPhysicsOrder(1);
        inUnit->SetHeight(this->GetHeightAt(inUnit, thePosition));
        return;
    case kActionContinueToTaxi:
    case kActionContinueToClimb:
    case kActionContinueToFly:
    case kActionContinueToDescend:
    case kActionContinueToBrake:
    case kActionContinueToWaitForLandingClearance:
        inUnit->SetCurrentPhysicsOrder(5);
        inUnit->NextWaypoint();
        return;
    case kActionLand:
    case kActionRecalculateLanding:
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetCurrentPhysicsOrder(5);
        break;
    case kActionTakeoff:
        inUnit->SetCurrentPhysicsOrder(5);
        this->ReturnToAirport(inUnit);
        inUnit->SetFlightTime(inUnit->mStats->mFlightTime);
        inUnit->SetAmmunition(this->mAmmunition);
        break;
    case kActionDying: {
        EESoundObject* theSound = inUnit->mStats->mDeathSound;
        if (theSound != NULL) {
            float theVolume;
            float thePan;
            float thePitch;
            this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
            gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        }
        inUnit->SetCurrentPhysicsAction(2);
        inUnit->SetAnimation(4, -1, 0);
        inUnit->DropPath();
        EEPMAirAnimal* theModel = dynamic_cast<EEPMAirAnimal*>(this->mPhysicsModel);
        inUnit->SetF220(theModel->ComputeRoll(inUnit, inUnit->mAngularVelocity.GetYaw()));
        inUnit->mAngularVelocity = GEOrientation(inUnit->mAngularVelocity.GetYaw(), inUnit->mAngularVelocity.GetPitch(), inUnit->mAngularVelocity.GetYaw());
        return;
    }
    case kActionPrepareToMove: {
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetCurrentPhysicsOrder(5);
        inUnit->DropPath();
        if (theGoal->mKind == kGoalAttackUnit) {
            EEUnit* theUnit;
            EEFindDropLocation(inUnit, &theUnit, &theTarget, NULL);
            float theRange = inUnit->mStats->mRange;
            float theRange2 = theRange;
            if (EEGetHitDistanceSquared(inUnit, theUnit, -10000.0f, -10000.0f) > theRange * theRange2)
                theGoal->GetDestination(inUnit, theTarget);
        } else
            theGoal->GetDestination(inUnit, theTarget);
        if (theGoal->mKind == kGoalExplore)
            theTarget.SetZ(this->GetCruiseAltitude());
        this->PathToward(inUnit, thePosition, theTarget, true);
        break;
    }
    case kActionResourcesRotting:
        this->HandleActionChanged(inUnit);
        return;
    default:
        this->HandleActionChanged(inUnit);
        return;
    }
    inUnit->SetAnimation(0xd, -1, 0);
    if (inUnit->GetNextWaypointPoint(thePosition) == 0)
        inUnit->SetHeight(this->GetHeightAt(inUnit, thePosition));
}

// FUNCTION: EXE 0x60ad1c
// Sets up a new animal on the ground (height 0)
void EEUCAirAnimal::UnitCreated(EEComplexUnit* inUnit)
{
    EEUnitClass::UnitCreated(inUnit);
    inUnit->SetHeight(0.0f);
}

// FUNCTION: EXE 0x60ad46
// Returns whether the animal reached its next waypoint (within 0.5 tiles), or when braking on its last one, has stopped
bool EEUCAirAnimal::HasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint theTarget;
    unsigned long theCount = inUnit->mWaypoints.size();
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          0.0f);
    inUnit->GetNextWaypointPoint(theTarget);
    theTarget.SetZ(0.0f);
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionBrake && theCount <= 1 && this->mAirTrafficType != 2) {
        return inValue == 0.0f;
    }
    return theTarget.DistanceToSquared(thePosition) <= 0.25f;
}

// FUNCTION: EXE 0x60ade9
// Answers the flight triggers (next waypoint lower, level, higher, in the air or on the ground; move prepared; fall
// finished); other triggers go to the common unit class
long EEUCAirAnimal::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    float theZ = inUnit->mPhysicalModel.GetLocation().GetZ();
    unsigned long theCount = inUnit->mWaypoints.size();
    GE3DPoint theTarget;
    long theResult = 0;
    bool theFlag = false;
    float theDelta = 0.0f;
    float theGround = 0.0f;
    if (inUnit->GetNextWaypointPoint(theTarget) == 0) {
        theDelta = theTarget.GetZ() - theZ;
        long theX;
        long theY;
        EEMapTile* theTile = ::gWorld.mMap->GetTile(UMFloor(theTarget.GetX()), UMFloor(theTarget.GetY()));
        if (theTile == NULL)
            theTile = ::gWorld.mMap->GetClosestTile(&theTarget, &theX, &theY);
        theGround = theTile->GetHeight();
        if (theGround < 0.0f)
            theGround = 0.0f;
    }
    switch (inEvent) {
    case kTriggerWaypointIsLowerElevation:
        if (inUnit->mUnitClass->mAirTrafficType != 2)
            theFlag = inUnit->mHasArrived && theCount > 0 && theDelta < 0.0f;
        else
            theFlag = theCount > 0;
        break;
    case kTriggerWaypointIsSameElevation:
        if (inUnit->mUnitClass->mAirTrafficType != 2)
            theFlag = inUnit->mHasArrived && theCount > 0 && theDelta == 0.0f;
        else
            theFlag = theCount <= 1;
        break;
    case kTriggerWaypointIsHigherElevation:
        theFlag = inUnit->mHasArrived && theCount > 0 && theDelta > 0.0f;
        break;
    case kTriggerWaypointInAir:
        theFlag = inUnit->mHasArrived && theCount > 0 && theTarget.GetZ() > theGround;
        break;
    case kTriggerWaypointOnGround:
        theFlag = inUnit->mHasArrived && theCount > 0 && theTarget.GetZ() == theGround;
        break;
    case kTriggerMovePreparationComplete:
        theFlag = (long)inUnit->mWaypoints.size() > 0;
        break;
    case kTriggerDyingAnimationFinished:
        theFlag = inUnit->mVelocity == 0.0f;
        break;
    default:
        theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x60afe9
// Animates the animal like any unit
long EEUCAirAnimal::Animate(EEComplexUnit* inUnit)
{
    return EEUnitClass::Animate(inUnit);
}

// FUNCTION: EXE 0x60aff5
// Runs a tick: burns flight time, switches flight state (at most once a second unless forced), steps physics,
// animation and sound, and counts down a carcass's food (1 per 4 s) or rotting time
void EEUCAirAnimal::Update(EEComplexUnit* inUnit)
{
    long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    long theTime = inUnit->mFlightTime;
    unsigned long theState = inUnit->mCurrentPhysicsOrder;
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionIdle) {
        long theLeft = theTime - gPhysics->mTickTimeMs;
        if (theLeft < 0)
            theLeft = 0;
        if (!::gWorld.mInCinematic)
            inUnit->SetFlightTime(theLeft);
        if (theCommand != kActionCircle && theCommand != kActionWaitForLandingClearance && theCommand != kActionCircleAirtrafficController) {
            unsigned long theNew = this->GetNextFlightState(inUnit);
            if (theState == 5) {
                if (theNew != 5)
                    inUnit->SetCurrentPhysicsOrder(theNew);
            } else if (theNew == 5)
                inUnit->SetCurrentPhysicsOrder(5);
            else if (theState != theNew && (theTime / 1000 > theLeft / 1000 || theLeft / 1000 == 0))
                inUnit->SetCurrentPhysicsOrder(theNew);
        }
    }
    if (this->mPhysicsModel != NULL && inUnit->mTileX != -10000)
        this->mPhysicsModel->Step(inUnit);
    this->Animate(inUnit);
    this->RefreshSound(inUnit);
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionResourcesRotting: {
        long theTotal = gPhysics->mTickTimeMs + inUnit->mRottingAwayTime;
        if (theTotal > 4000) {
            long theHitPoints = inUnit->mResourceAmount - theTotal / 4000;
            if (theHitPoints < 0)
                theHitPoints = 0;
            inUnit->SetResourceAmount(theHitPoints);
            theTotal %= 4000;
        }
        inUnit->SetRottingAwayTime(theTotal);
        break;
    }
    case kActionUnitRotting: {
        long theLeft = inUnit->mRottingAwayTime - gPhysics->mTickTimeMs;
        if (theLeft < 0)
            theLeft = 0;
        inUnit->SetRottingAwayTime(theLeft);
        break;
    }
    }
}

// ICF-FOLDED: EXE 0x4ac9f3
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
bool EEUCAirAnimal::CanAttackMove()
{
    return false;
}

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCAirAnimal::GroundLocation(class GE3DPoint &)
{
}

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCAirAnimal::UpdateEffects(class EEComplexUnit *)
{
}
