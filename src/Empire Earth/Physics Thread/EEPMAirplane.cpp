// C:\EEX\Empire Earth\Physics Thread\EEPMAirplane.cpp (file name inferred: no __FILE__; RTTI EEPMAirplane; one unit per
// physics-model class, alphabetical link order). Range 0x15726c..0x1587c7; split out of a single EEPMLand.cpp
// (each unit ends with its own inline-COMDAT cluster).
#include <windows.h>
#include <vector>
#include <math.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/U2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEPhysicsModel.h"

// NEAR: scale-by-height factored by VC6 to (a+1)*0.5 (original keeps a*0.5+0.5), frame slots
// FUNCTION: EXE 0x55726c
// Flies the airplane toward its next waypoint at its height (straight on without one), following its
// carrier while docked; a descending plane stays above the ground; shrinks carrier aircraft below cruise altitude
void EEPMAirplane::Step0(EEComplexUnit* inUnit)
{
    GEPhysicalModel& theModel = inUnit->mPhysicalModel;
    GETransformation theTransformation(theModel.GetLocation());
    GE3DPoint theLocation;
    EEUnitClass* theClass = inUnit->mUnitClass;
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    float theStep = gPhysics->mTickSeconds;
    if (this->IsDocked(inUnit) == true) {
        this->FollowCarrier(inUnit);
        theTransformation = theModel.GetLocation();
    }
    theLocation.SetX(theTransformation.GetX());
    theLocation.SetY(theTransformation.GetY());
    theLocation.SetZ(theTransformation.GetZ());
    if (theClass->mAirTrafficType == 2 || theClass->mAirTrafficType == 4) {
        if (theLocation.GetZ() >= theClass->GetCruiseAltitude())
            theModel.SetScale(inUnit->mStats->GetScale());
        else if (theLocation.GetZ() >= 0.0f) {
            float theScale = theLocation.GetZ() / theClass->GetCruiseAltitude() * 0.5f;
            theScale += 0.5f;
            theModel.SetScale(theScale * inUnit->mStats->GetScale());
        } else
            theModel.SetScale(inUnit->mStats->GetScale() * 0.5f);
    }
    theTransformation.SetOrientationYPR(inUnit->mOrientation.GetYaw(), inUnit->mOrientation.GetPitch(), inUnit->mOrientation.GetRoll());
    inUnit->mPhysicalModel.SetLocation(theTransformation);
    float theDistance;
    if ((long) inUnit->mWaypoints.size() > 0) {
        inUnit->GetNextWaypointPoint(theTarget);
        theTarget.SetZ(inUnit->mHeight);
        theDistance = theLocation.DistanceTo(theTarget);
        if (theDistance < 0.05f) {
            theDistance = 0.0f;
            theLocation = theTarget;
        }
    } else {
        theForward.mY = 1.0f;
        theTransformation.Apply(theForward, theMove);
        theTarget = theLocation;
        theTarget += theMove;
        theTarget.SetZ(inUnit->mHeight);
        theDistance = theLocation.DistanceTo(theTarget);
    }
    float thePitch;
    float theTurn;
    float theYawRate;
    float theYaw;
    float thePitchRate;
    this->TurnToward(inUnit, theLocation, theTarget, &theTurn, &theYawRate, &theYaw, false);
    this->PitchTowardSlope(inUnit, theLocation, theTarget, theDistance, &thePitchRate, &thePitch);
    GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
    inUnit->mAngularVelocity.SetPitch(thePitchRate);
    theAngularVelocity.mRoll = 0.0f;
    theAngularVelocity.mYaw = theYawRate;
    inUnit->mOrientation = GEOrientation(theYaw, thePitch, inUnit->mRoll);
    theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
    float theSpeed = this->Accelerate(inUnit, theDistance, theTurn);
    inUnit->SetVelocity(theSpeed);
    theForward.mY = theSpeed * theStep;
    theTransformation.Apply(theForward, theMove);
    theLocation += theMove;
    if (theCommand == kActionDescend || theCommand == kActionContinueToDescend) {
        unsigned long theY = UMFloorUnsigned(theLocation.GetY());
        unsigned long theX = UMFloorUnsigned(theLocation.GetX());
        float theGround;
        if (::gWorld.GetMap()->mTerrainMesh.GetIsValidTile(theX, theY))
            theGround = ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY());
        else
            theGround = 0.0f;
        if (theLocation.GetZ() < theGround)
            theLocation.SetZ(theGround);
    }
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    float theBank = this->ComputePitch(inUnit, thePitch);
    theTransformation.SetOrientationYPR(theYaw, theBank, inUnit->mRoll + this->ComputeRoll(inUnit, theYawRate));
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
    bool theFlag = theClass->HasArrived(inUnit, theDistance);
    if (inUnit->mHasArrived != theFlag)
        inUnit->mHasArrived = theFlag;
}

// NEAR: 0.5 CSE/scale, frame slots
// FUNCTION: EXE 0x5576a9
// Flies the airplane toward the unit it attacks (or 5 tiles straight on without a target), turning
// with its facing offset; shrinks carrier aircraft below cruise altitude
void EEPMAirplane::Step1(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    EEUnitClass* theClass = inUnit->mUnitClass;
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
    float theStep = gPhysics->mTickSeconds;
    if (theClass->mAirTrafficType == 2 || theClass->mAirTrafficType == 4) {
        if (theLocation.GetZ() >= theClass->GetCruiseAltitude())
            inUnit->mPhysicalModel.SetScale(inUnit->mStats->GetScale());
        else if (theLocation.GetZ() >= 0.0f) {
            float theScale = theLocation.GetZ() / theClass->GetCruiseAltitude() * 0.5f;
            theScale += 0.5f;
            inUnit->mPhysicalModel.SetScale(theScale * inUnit->mStats->GetScale());
        } else
            inUnit->mPhysicalModel.SetScale(inUnit->mStats->GetScale() * 0.5f);
    }
    if (theGoal) {
        EEUnit* theTargetUnit = theGoal->mTarget;
        if (theTargetUnit->mComplexUnit) {
            theTarget.SetX(theTargetUnit->mComplexUnit->mPhysicalModel.GetLocation().GetX());
            theTarget.SetY(theTargetUnit->mComplexUnit->mPhysicalModel.GetLocation().GetY());
        } else {
            theTarget.SetX((float) theTargetUnit->GetTileX() + 0.5f);
            theTarget.SetY((float) theTargetUnit->GetTileY() + 0.5f);
        }
        theTarget.SetZ(inUnit->mHeight);
    } else {
        theForward.mY = 5.0f;
        theTransformation.Apply(theForward, theMove);
        theMove.mZ = 0.0f;
        theTarget.SetZ(theLocation.GetZ());
        theTarget.SetX(theMove.mX + theLocation.GetX());
        theTarget.SetY(theMove.mY + theLocation.GetY());
    }
    float theDistance = theLocation.DistanceTo(theTarget);
    float thePitch;
    float theTurn;
    float theYawRate;
    float theYaw;
    float thePitchRate;
    this->TurnToward(inUnit, theLocation, theTarget, &theTurn, &theYawRate, &theYaw, true);
    this->PitchTowardSlope(inUnit, theLocation, theTarget, theDistance, &thePitchRate, &thePitch);
    GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
    inUnit->mAngularVelocity.SetPitch(thePitchRate);
    theAngularVelocity.mRoll = 0.0f;
    theAngularVelocity.mYaw = theYawRate;
    inUnit->mOrientation = GEOrientation(theYaw, thePitch, 0.0f);
    theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
    theDistance = this->Accelerate(inUnit, theDistance, theTurn);
    inUnit->SetVelocity(theDistance);
    theForward.mY = theDistance * theStep;
    theTransformation.Apply(theForward, theMove);
    theLocation += theMove;
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    float theBank = this->ComputePitch(inUnit, thePitch);
    theTransformation.SetOrientationYPR(theYaw, theBank, inUnit->GetRoll() + this->ComputeRoll(inUnit, theYawRate));
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}

// NEAR: frame slot assignment (the original keeps theIndex in the dead inUnit slot); vector<EETagPoint*>::begin binds to 0x22eb7, the original calls the ICF copy 0x17a952
// FUNCTION: EXE 0x557999
// Keeps a docked plane on its carrier's deck between the deck tag points, at its path distance
void EEPMAirplane::FollowCarrier(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mHomeBase->mPhysicalModel.GetLocation());
    GE3DPoint thePosition;
    GE3DPoint theNext;
    GE3DPoint theStart;
    EEComplexUnit* theCarrier = inUnit->mHomeBase;
    EEUnitClassUpgrade* theStats = theCarrier->mStats;
    EETagPointList* theTags = theCarrier->GetModel()->mTagPoints[theCarrier->mAnimation];
    float theScale = theStats->GetScale();
    if (!theTags || theTags->mPoints.size() == 0)
        return;
    float theIndex;
    inUnit->GetPathStart(theNext, theIndex);
    inUnit->mPhysicalModel.GetLocation().GetPosition(thePosition);
    float theDistance = thePosition.DistanceTo(theNext);
    theTags->Scale(theTransformation, theScale);
    theTags->mPoints[2]->GetPoint(theCarrier->mPhysicalModel.GetFrame(), theTransformation, theNext);
    theTags->mPoints[0]->GetPoint(theCarrier->mPhysicalModel.GetFrame(), theTransformation, theStart);
    theDistance = theStart.DistanceTo(theNext) - theDistance;
    this->OrientOnCarrier(theCarrier->mPhysicalModel.GetLocation(), theStart, inUnit, theDistance);
}

// FUNCTION: EXE 0x557b1e
// Returns the speed mode: on the last waypoints of a descent or brake, slowing (4) above 3/4 of top
// speed plus the carrier's; the base mode when taxiing (land planes); else cruising (3)
long EEPMAirplane::SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn)
{
    float theLimit = inUnit->mStats->GetMaxVelocity() * 0.75f + (inUnit->mHomeBase ? inUnit->mHomeBase->GetVelocity() : 0.0f);
    bool theCarried = inUnit->mUnitClass->mAirTrafficType == 2 || inUnit->mUnitClass->mAirTrafficType == 4;
    if (((!theCarried && (long) inUnit->mWaypoints.size() <= 6) || (theCarried && (long) inUnit->mWaypoints.size() <= 3))
        && ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionDescend
            || (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionContinueToDescend
            || (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionBrake
            || (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionContinueToBrake)) {
        EEPhysicsModel::SpeedMode(inUnit, inDistance, inTurn);
        if (inUnit->GetVelocity() > theLimit)
            return 4;
        return 3;
    }
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionTaxi && !theCarried)
        return EEPhysicsModel::SpeedMode(inUnit, inDistance, inTurn);
    return 3;
}

// FUNCTION: EXE 0x557c2d
// Returns the displayed pitch: a descending plane's dive is shown nose up, capped at 15 degrees
float EEPMAirplane::ComputePitch(EEComplexUnit* inUnit, float inValue)
{
    if (!(inValue < 0.0f))
        return inValue;
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionDescend
        && (inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionContinueToDescend)
        return inValue;
    if (-inValue > 0.2617994f)
        return 0.2617994f;
    return -inValue;
    return inValue;
}

// NEAR: register/slot allocation (class pointer in ebx), store order
// FUNCTION: EXE 0x557c83
// Crashes the shot-down plane: pitches it down and rolls it, accelerating, splashes on reaching water
// and stops it with its death effect and impact decal on hitting the ground
void EEPMAirplane::Step2(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    EEMap* theMap = ::gWorld.mMap;
    EEUnitClass* theClass = inUnit->mUnitClass;
    GE3DPoint theNew;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    float theStep = gPhysics->mTickSeconds;
    float theMaxRate = inUnit->mStats->mMaxAngularVelocity;
    float theAccel = inUnit->mStats->mAngularAcceleration;
    float theBrake = inUnit->GetAcceleration();
    float theSpeed = inUnit->GetVelocity();
    float theRollRate = inUnit->mAngularVelocity.mRoll;
    float thePitchRate = inUnit->mAngularVelocity.mPitch;
    float theYawRate = inUnit->mAngularVelocity.mYaw;
    bool theAirborne = theLocation.GetZ() > 0.0f;
    bool theLanding = (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionFadingOutInSpace;
    if (theClass->mAirTrafficType == 2 || theClass->mAirTrafficType == 4) {
        if (theLocation.GetZ() >= theClass->GetCruiseAltitude())
            inUnit->mPhysicalModel.SetScale(inUnit->mStats->GetScale());
        else if (theLocation.GetZ() >= 0.0f) {
            float theScale = theLocation.GetZ() / theClass->GetCruiseAltitude() * 0.5f;
            theScale += 0.5f;
            inUnit->mPhysicalModel.SetScale(theScale * inUnit->mStats->GetScale());
        } else
            inUnit->mPhysicalModel.SetScale(inUnit->mStats->GetScale() * 0.5f);
    }
    RTSTile* theTile = inUnit->mTile;
    theTransformation.Apply(theForward, theResult);
    float theHeading = UMArcTan2(-theResult.mX, theResult.mY);
    float thePitch = UMArcCos(-theResult.mZ) - 1.5707964f;
    float theRoll = inUnit->mF220;
    if (theAirborne || theLanding) {
        thePitchRate = thePitchRate - theAccel * theStep;
        if (thePitchRate < -theMaxRate)
            thePitchRate = -theMaxRate;
        float theNewPitch = thePitchRate * theStep + thePitch;
        if (theNewPitch < -1.2566371f)
            theNewPitch = -1.2566371f;
        // the roll acceleration first, then times the step (0x157eeb..0x157f09, unrounded): a single
        // `theAccel * 4.0f * theStep` let VC6 reuse the pitch's theAccel * theStep, rounded to a float, times 4
        theAccel = theAccel * 4.0f;
        theMaxRate = theMaxRate * 4.0f;
        if (theYawRate < 0.0f) {
            theRollRate = theRollRate - theAccel * theStep;
            theAccel = -theMaxRate;
            if (theRollRate < theAccel)
                theRollRate = theAccel;
        } else {
            theRollRate = theAccel * theStep + theRollRate;
            if (theRollRate > theMaxRate)
                theRollRate = theMaxRate;
        }
        theRoll = theRollRate * theStep + theRoll;
        inUnit->mAngularVelocity = GEOrientation(theYawRate, thePitchRate, theRollRate);
        theTransformation.SetOrientationYPR(theHeading, theNewPitch, theRoll);
        if (inUnit->mF220 != theRoll)
            inUnit->mF220 = theRoll;
        theSpeed = theBrake * theStep + theSpeed;
        if (inUnit->mStats->GetMaxVelocity() < theSpeed)
            theSpeed = inUnit->mStats->GetMaxVelocity();
        inUnit->SetVelocity(theSpeed);
        theForward.mX = 0.0f;
        theForward.mY = theSpeed * theStep;
        theTransformation.Apply(theForward, theResult);
        theNew = theLocation;
        theNew += theResult;
        long theY = UMFloor(theNew.GetY());
        long theX = UMFloor(theNew.GetX());
        RTSTile* theNewTile = theMap->GetTile(theX, theY);
        if (theNewTile) {
            theTile = theNewTile;
        } else {
            theNew.SetY(theLocation.GetY());
            theNew.SetX(theLocation.GetX());
        }
        theTransformation.SetPosition(theNew.GetX(), theNew.GetY(), theNew.GetZ());
        bool theAbove = theNew.GetZ() > 0.0f;
        if (theAirborne && !theAbove && !theLanding) {
            inUnit->SetAnimation(5, -1, 0);
            if (theTile->GetHeight() < 0.0f && inUnit->mUnitClass->mSplashEffect) {
                GE3DPoint theSplash(theNew.GetX(), theNew.GetY(), 0.0f);
                gEffectManager->CreatePlacedEffect(theSplash, inUnit->mUnitClass->mSplashEffect, NULL);
            }
        }
    } else {
        theTransformation.SetOrientationYPR(0.0f, -1.5707964f, 0.0f);
        theNew.SetX(theLocation.GetX());
        theNew.SetY(theLocation.GetY());
        theNew.SetZ(theLocation.GetZ() - theStep * 0.5f);
        theTransformation.SetPosition(theNew.GetX(), theNew.GetY(), theNew.GetZ());
    }
    if (theNew.GetZ() < theTile->GetHeight() && theTile->mTerrainType != kTerrainSpace) {
        inUnit->SetVelocity(0.0f);
        if (theTile->GetHeight() >= 0.0f) {
            if (inUnit->mUnitClass->mDeathEffect)
                gEffectManager->CreateUnitEffect(inUnit, inUnit->mUnitClass->mDeathEffect, NULL, 0, 0, 0);
            if (inUnit->mUnitClass->mShellHoleEffect)
                EEPlaceImpactDecal(inUnit, theNew);
        }
    }
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}

// NEAR: 98: the original loads mPoints[2] (ebx reused) before pushing GetPoint's arguments
// FUNCTION: EXE 0x5581a0
// Returns whether a carrier plane taxiing or braking with clearance on its carrier has not yet reached
// the deck tag point
bool EEPMAirplane::IsDocked(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(false);
    float theIndex;
    GE3DPoint theDeck;
    GE3DPoint theNext;
    EEComplexUnit* theCarrier = inUnit->mHomeBase;
    bool theResult = false;
    if ((inUnit->mUnitClass->mAirTrafficType == 2 || inUnit->mUnitClass->mAirTrafficType == 4) && theCarrier
        && ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionTaxi
            || (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionBrake)
        && inUnit->mIsClearanceGranted == true) {
        theTransformation = theCarrier->mPhysicalModel.GetLocation();
        EETagPointList* theTags = theCarrier->GetModel()->mTagPoints[theCarrier->GetAnimation()];
        inUnit->GetPathStart(theNext, theIndex);
        if (theTags) {
            theTags->Scale(theTransformation, theCarrier->mStats->mScale);
            theTags->mPoints[2]->GetPoint(theCarrier->mPhysicalModel.GetFrame(), theTransformation, theDeck);
        } else {
            theDeck.mX = theTransformation.GetX();
            theDeck.mY = theTransformation.GetY();
            theDeck.mZ = 1.0f;
        }
        if (theNext != theDeck)
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5582d6
// Puts the plane at an offset along the carrier from a deck point, facing as the carrier, and routes it on
void EEPMAirplane::OrientOnCarrier(const GETransformation& inCarrier, const GE3DPoint& inPoint, EEComplexUnit* inUnit, float inOffset)
{
    GEOrientation theOrientation;
    GETransformation theTransformation(inCarrier);
    GE3DPoint theResult;
    GE3DPoint theOffset(0.0f, inOffset, 0.0f);
    theTransformation.SetPosition(inPoint.GetX(), inPoint.GetY(), inPoint.GetZ());
    theTransformation.Apply(theOffset, theResult);
    theTransformation = inUnit->mHomeBase->mPhysicalModel.GetLocation();
    theTransformation.SetPosition(theResult.GetX(), theResult.GetY(), theResult.GetZ());
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
    theTransformation.ConvertToOrientation(theOrientation);
    inUnit->mOrientation = theOrientation;
    inUnit->mUnitClass->RouteDockWaypoint(inUnit);
}

// NEAR: second transformation (-0x7c) layout, slots
// FUNCTION: EXE 0x5583ad
// Slows the plane's speed and turn rate to a stop, moving and turning it by what is left of them
void EEPMAirplane::Step4(EEComplexUnit* inUnit)
{
    GEPhysicalModel& theModel = inUnit->mPhysicalModel;
    GETransformation theTransformation(theModel.GetLocation());
    GE3DPoint theLocation;
    EEUnitClass* theClass = inUnit->mUnitClass;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    float theStep = gPhysics->mTickSeconds;
    float theYawRate = inUnit->mAngularVelocity.mYaw;
    if (this->IsDocked(inUnit) == true) {
        this->FollowCarrier(inUnit);
        theTransformation = theModel.GetLocation();
    }
    theLocation.SetX(theTransformation.GetX());
    theLocation.SetY(theTransformation.GetY());
    theLocation.SetZ(theTransformation.GetZ());
    if (theClass->mAirTrafficType == 2 || theClass->mAirTrafficType == 4) {
        if (theLocation.GetZ() >= theClass->GetCruiseAltitude())
            theModel.SetScale(inUnit->mStats->GetScale());
        else if (theLocation.GetZ() >= 0.0f) {
            float theScale = theLocation.GetZ() / theClass->GetCruiseAltitude() * 0.5f;
            theScale += 0.5f;
            theModel.SetScale(theScale * inUnit->mStats->GetScale());
        } else
            theModel.SetScale(inUnit->mStats->GetScale() * 0.5f);
    }
    theTransformation.Apply(theForward, theResult);
    float theHeading = UMArcTan2(-theResult.mX, theResult.mY);
    if (theYawRate < 0.0f) {
        theYawRate = inUnit->mStats->GetAngularAcceleration() * theStep + theYawRate;
        if (theYawRate > 0.0f)
            theYawRate = 0.0f;
    } else if (theYawRate > 0.0f) {
        theYawRate = theYawRate - inUnit->mStats->GetAngularAcceleration() * theStep;
        if (theYawRate < 0.0f)
            theYawRate = 0.0f;
    } else {
        theYawRate = 0.0f;
    }
    GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
    theAngularVelocity.mYaw = theYawRate;
    theAngularVelocity.mPitch = 0.0f;
    theAngularVelocity.mRoll = 0.0f;
    float theNewHeading = theYawRate * theStep + theHeading;
    GETransformation theRotation(false);
    long theTileX;
    long theTileY;
    theRotation.SetOrientationYPR(theNewHeading, 0.0f, inUnit->mRoll);
    theRotation.SetPosition(0.0f, 0.0f, 0.0f);
    theRotation.SetElement(3, 0, 0.0f);
    theRotation.SetElement(3, 1, 0.0f);
    theRotation.SetElement(3, 2, 0.0f);
    theRotation.SetElement(3, 3, 1.0f);
    float theSpeed;
    if (inUnit->GetVelocity() > 0.0f) {
        theSpeed = inUnit->GetVelocity() - inUnit->GetAcceleration() * theStep;
        if (theSpeed < 0.0f)
            theSpeed = 0.0f;
        if (inUnit->GetMaxVelocity() < theSpeed)
            theSpeed = inUnit->GetMaxVelocity();
    } else {
        theSpeed = 0.0f;
    }
    inUnit->SetVelocity(theSpeed);
    if (theYawRate == 0.0f && theSpeed == 0.0f)
        return;
    theForward.mY = theSpeed * theStep;
    theRotation.Apply(theForward, theResult);
    theLocation += theResult;
    theRotation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    float theBank = this->ComputePitch(inUnit, 0.0f);
    theRotation.SetOrientationYPR(theNewHeading, theBank, inUnit->mRoll + this->ComputeRoll(inUnit, theYawRate));
    EEUpdateUnitTile(&theRotation, inUnit, &theTileX, &theTileY);
}

// NEAR: bool theFree lives in memory; the original keeps it in cl and the y floor in the dead ioLocation slot
// FUNCTION: EXE 0x5586d2
// Undoes the move onto a tile the plane cannot enter or a map edge tile (returns false), else clips
// it to the map edges
bool EEPMAirplane::ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance)
{
    float theDistance;
    EEMap* theMap = ::gWorld.mMap;
    long theX = UMFloor(ioLocation.GetX());
    long theY = UMFloor(ioLocation.GetY());
    theDistance = 0.0f;
    EETile* theTile = theMap->GetTile(theX, theY);
    bool theFree = theTile != NULL;
    if (theTile && theTile != inUnit->mTile
        && (!inUnit->mUnitClass->CanMoveOnTerrain(inUnit, theTile->mTerrainType) || theMap->IsEdgeTile(theX, theY)))
        theFree = false;
    if (theFree)
        return EEPhysicsModel::ClipStep(inUnit, inMove, ioLocation, &theDistance);
    ioLocation.SetX(ioLocation.GetX() - inMove.GetX());
    ioLocation.SetY(ioLocation.GetY() - inMove.GetY());
    return false;
}

// Returns the displayed bank for the turn rate, up to the class's maximum roll
// ICF copy: LINK kept EEPMSeaAnimal's identical body at 0x55da2c (EEPMSeaAnimal.cpp)
float EEPMAirplane::ComputeRoll(EEComplexUnit* inUnit, float inValue)
{
    float theRatio = inValue / inUnit->mStats->mMaxAngularVelocity;
    if (inValue < 0.0f) {
        theRatio = theRatio + 0.100000024f;
        if (theRatio > 0.0f)
            theRatio = 0.0f;
    } else {
        theRatio = theRatio - 0.100000024f;
        if (theRatio < 0.0f)
            theRatio = 0.0f;
    }
    return theRatio * 1.1111112f * inUnit->mUnitClass->mMaxRoll;
}

// Turns toward the target point in physics order 5, else turns at a steady rate
// ICF copy: LINK kept EEPMSpaceship's identical body at 0x55e718 (EEPMSpaceship.cpp)
void EEPMAirplane::TurnToward(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float* outA, float* outB, float* outC, bool inUseFacingOffset)
{
    if (inUnit->mCurrentPhysicsOrder == 5)
        EEPhysicsModel::TurnToward(inUnit, inFrom, inTo, outA, outB, outC, inUseFacingOffset);
    else
        this->SteadyTurn(inUnit, outA, outB, outC);
}
