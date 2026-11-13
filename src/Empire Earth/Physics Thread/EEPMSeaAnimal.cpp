// C:\EEX\Empire Earth\Physics Thread\EEPMSeaAnimal.cpp (file name inferred: no __FILE__; RTTI EEPMSeaAnimal; one unit per
// physics-model class, alphabetical link order). Range 0x15d44f..0x15dd66; split out of a single EEPMLand.cpp
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
#include "ee/EEUGRust.h"
#include "ee/EEGraphicObject.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPhysicsModel.h"

extern unsigned long gCollisionPeriod;      // 0x4c237c (EEPhysics.cpp; provisional name)

// NEAR: GEOrientation copies (the original calls the imported operator=: inline budget), frame layout
// FUNCTION: EXE 0x55d44f
// Swims the sea animal toward its next waypoint in 3D (straight on at its height without one), turning,
// pitching and banking; marks it arrived when close and makes a ripple where it drops below the surface
void EEPMSeaAnimal::Step0(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation;
    EEUnitClass* theClass = inUnit->mUnitClass;
    GE3DPoint theTarget;
    GEOrientation theRate;
    GEVector theForward;
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    theLocation.SetX(theTransformation.GetX());
    theLocation.SetY(theTransformation.GetY());
    theLocation.SetZ(theTransformation.GetZ());
    float theOldZ = theLocation.GetZ();
    theTransformation.SetOrientationYPR(inUnit->mOrientation.GetYaw(), inUnit->mOrientation.GetPitch(), inUnit->mOrientation.GetRoll());
    inUnit->mPhysicalModel.SetLocation(theTransformation);
    float theDistance;
    if ((long) inUnit->mWaypoints.size() > 0) {
        inUnit->GetNextWaypointPoint(theTarget);
        theDistance = theLocation.DistanceTo(theTarget);
        if (theDistance < 0.05f) {
            theLocation = theTarget;
            theDistance = 0.0f;
        }
    } else {
        theForward.mY = 1.0f;
        theTransformation.Apply(theForward, theMove);
        theTarget = theLocation;
        theTarget += theMove;
        theTarget.SetZ(inUnit->mHeight);
        theDistance = theLocation.DistanceTo(theTarget);
    }
    float theTurn;
    float theYawRate;
    float theYaw;
    float thePitchRate;
    float thePitch;
    this->TurnToward(inUnit, theLocation, theTarget, &theTurn, &theYawRate, &theYaw, false);
    this->PitchTowardSlope(inUnit, theLocation, theTarget, theDistance, &thePitchRate, &thePitch);
    theRate.mYaw = theYawRate;
    theRate.mPitch = thePitchRate;
    inUnit->mAngularVelocity = theRate;
    inUnit->mOrientation = GEOrientation(theYaw, thePitch, 0.0f);
    theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
    float theSpeed = this->Accelerate(inUnit, theDistance, theTurn);
    inUnit->SetVelocity(theSpeed);
    theForward.mY = theSpeed * theStep;
    theTransformation.Apply(theForward, theMove);
    theLocation += theMove;
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    float theBank = this->ComputePitch(inUnit, thePitch);
    theTransformation.SetOrientationYPR(theYaw, theBank, this->ComputeRoll(inUnit, theYawRate));
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
    bool theFlag = theClass->HasArrived(inUnit, theDistance);
    if (inUnit->mHasArrived != theFlag)
        inUnit->mHasArrived = theFlag;
    if (theOldZ > 0.0f && theLocation.GetZ() <= 0.0f) {
        GE3DPoint theSplash(theLocation.GetX(), theLocation.GetY(), 0.0f);
        gWorld.GetEffectManager()->CreatePlacedEffect(theSplash, EEGraphicEffect::Get(kGraphicEffectLandRipple), NULL);
    }
}

// FUNCTION: EXE 0x55d787
// Handles the sea animal's attack step as a submarine does
void EEPMSeaAnimal::Step1(EEComplexUnit* inUnit)
{
    EEPMSubmergedNaval::Step1(inUnit);
}

// FUNCTION: EXE 0x55d793
// Moves the sea animal as a submarine does
void EEPMSeaAnimal::Move(EEComplexUnit* inUnit)
{
    EEPMSubmergedNaval::Move(inUnit);
}

// FUNCTION: EXE 0x55d79f
// Runs the submarine after-step work for the sea animal
void EEPMSeaAnimal::AfterStep(EEComplexUnit* inUnit)
{
    EEPMSubmergedNaval::AfterStep(inUnit);
}

// FUNCTION: EXE 0x55d7ab
// Returns the sea animal's new speed for its speed mode; leaping toward a raised waypoint from near the
// surface boosts its top speed and acceleration; on its last leg it never overshoots the waypoint
float EEPMSeaAnimal::Accelerate(EEComplexUnit* inUnit, float inDistance, float inTurn)
{
    float theSpeed = inUnit->GetVelocity();
    float theMaxSpeed = inUnit->GetMaxVelocity();
    float theAccel = inUnit->GetAcceleration();
    long theKind = inUnit->mWaypoints.size();
    if (theKind > 1 && (*inUnit->mWaypoints.begin()).mLocation.GetZ() > 0.0f
        && inUnit->mPhysicalModel.GetLocation().GetZ() < 0.3f) {
        float theFactor = ((*inUnit->mWaypoints.begin()).mLocation.GetZ() - inUnit->mPhysicalModel.GetLocation().GetZ()) * 1.15f;
        if (theFactor <= 1.0f)
            theFactor = 1.0f;
        theFactor = theFactor + theFactor;
        theMaxSpeed *= theFactor;
        theAccel *= theFactor;
    }
    // New (R5b.4 leaf 6): the speed after the mode is the base model's arithmetic (Rust: ee_units physics.rs
    // accelerate; its brake's 1.0f factor changes no bit) over the boosted top speed and acceleration
    long theMode = this->SpeedMode(inUnit, inDistance, inTurn);
    return EEPMAccelerate(inUnit, theSpeed, theMaxSpeed, theAccel, inDistance, theMode);
}

// FUNCTION: EXE 0x55d8de
// Turns toward the target point in physics order 5; otherwise keeps circling, speeding its turn rate up
// (one way in order 0, the other way else)
void EEPMSeaAnimal::TurnToward(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float* outA, float* outB, float* outC, bool inUseFacingOffset)
{
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    float theStep = gPhysics->mTickSeconds;
    float theAccel = inUnit->mStats->mAngularAcceleration;
    float theMax = inUnit->mStats->mMaxAngularVelocity;
    unsigned long theState = inUnit->mCurrentPhysicsOrder;
    float theRate = inUnit->mAngularVelocity.mYaw;
    if (theState == 5) {
        EEPhysicsModel::TurnToward(inUnit, inFrom, inTo, outA, outB, outC, inUseFacingOffset);
        return;
    }
    inUnit->mPhysicalModel.GetLocation().Apply(theForward, theResult);
    float theHeading = UMArcTan2(-theResult.mX, theResult.mY);
    float theNew;
    if (theState == 0) {
        theNew = theRate - theAccel * theStep;
        if (theNew < -theMax)
            theNew = -theMax;
    } else {
        theNew = theAccel * theStep + theRate;
        if (theNew > theMax)
            theNew = theMax;
    }
    *outA = 0.0f;
    *outB = theNew;
    *outC = theNew * theStep + theHeading;
}

// FUNCTION: EXE 0x55d9d6
// Returns the displayed pitch: a diving animal's downward pitch is shown nose up, capped at 25 degrees
float EEPMSeaAnimal::ComputePitch(EEComplexUnit* inUnit, float inValue)
{
    if (!(inValue < 0.0f))
        return inValue;
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionDescend
        && (inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionContinueToDescend)
        return inValue;
    if (-inValue > 0.43633232f)
        return 0.43633232f;
    return -inValue;
    return inValue;
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55da2c
// Returns the displayed bank for the turn rate, up to the class's maximum roll (no bank below 10% of the
// top turn rate)
float EEPMSeaAnimal::ComputeRoll(EEComplexUnit* inUnit, float inValue)
{
    float theRatio = inValue / inUnit->mStats->GetMaxAngularVelocity();
    if (inValue < 0.0f) {
        theRatio = theRatio + 0.100000024f;
        if (theRatio > 0.0f)
            theRatio = 0.0f;
    } else {
        theRatio = theRatio - 0.100000024f;
        if (theRatio < 0.0f)
            theRatio = 0.0f;
    }
    return (theRatio * 1.1111112f) * inUnit->mUnitClass->GetMaxRoll();
}

// FUNCTION: EXE 0x55da8c
// Returns the base physics model's pitch mode
long EEPMSeaAnimal::PitchMode(EEComplexUnit* inUnit, float inRate, float inAngle)
{
    return EEPhysicsModel::PitchMode(inUnit, inRate, inAngle);
}

// FUNCTION: EXE 0x55daa8
// Returns the base physics model's yaw mode
long EEPMSeaAnimal::YawMode(EEComplexUnit* inUnit, float inRate, float inAngle)
{
    return EEPhysicsModel::YawMode(inUnit, inRate, inAngle);
}

// FUNCTION: EXE 0x55dac4
// Returns the speed mode (2 hold, 3 accelerate, 4 brake): brakes for sharp turns, near the path's end
// and on steep terrain; heights above or below the surface scale top speed and braking
long EEPMSeaAnimal::SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn)
{
    float theStep = gPhysics->mTickSeconds;
    float theBrake = inUnit->GetAcceleration();
    float theMaxSpeed = inUnit->GetMaxVelocity();
    float theSpeed = inUnit->GetVelocity();
    float theLimit = inDistance;
    float theTurnPenalty = inUnit->mUnitClass->mSlopeSpeedPenalty;
    if ((long) inUnit->mWaypoints.size() > 1) {
        float theFactor;
        if ((*inUnit->mWaypoints.begin()).mLocation.GetZ() > 0.0f) {
            theFactor = ((*inUnit->mWaypoints.begin()).mLocation.GetZ() - inUnit->mPhysicalModel.GetLocation().GetZ()) * 1.15f;
            if (theFactor <= 1.0f)
                theFactor = 1.0f;
            theFactor = theFactor + theFactor;
            theMaxSpeed *= theFactor;
            theBrake *= theFactor;
        } else if (inUnit->mPhysicalModel.GetLocation().GetZ() > 0.015f) {
            theFactor = (inUnit->mPhysicalModel.GetLocation().GetZ() - 1.15f) * 1.05f;
            if (theFactor <= 1.0f)
                theFactor = 1.0f;
            theFactor = theFactor + theFactor;
            theMaxSpeed *= theFactor;
            theBrake *= theFactor;
        }
    }
    float theTime = gPhysics->mTicksPerGameSecond;
    float theTicks = inUnit->GetInverseAcceleration() * theTime * theSpeed;
    float theStopDistance = (theTicks + 1.0f) * theTicks * 0.5f;
    theStopDistance = theStopDistance * (theStep * theStep) * theBrake;
    if (theStopDistance < 0.0001f)
        theStopDistance = 0.0f;
    if (inDistance > 0.5f)
        theLimit = 0.5f;
    float theSlide = inUnit->mAngularVelocity.GetYaw() * (theLimit * 0.5f) * theStep;
    if (theSlide < 0.0f)
        theSlide = -theSlide;
    long theResult;
    if (inTurn < -0.7853982f || inTurn > 0.7853982f) {
        if (theSpeed * theStep < theSlide) {
            if ((theBrake * theStep + theSpeed) * theStep < theSlide)
                theResult = 3;
            else
                theResult = 2;
        } else
            theResult = 4;
    } else {
        theResult = 3;
    }
    float theRemaining;
    if ((long)inUnit->mWaypoints.size() > 1)
        inUnit->GetPathLength(theRemaining);
    else
        theRemaining = inDistance;
    if (theRemaining <= theStopDistance)
        theResult = 4;
    if (!(theTurnPenalty < 1.0f))
        return theResult;
    float theSlope = ::gWorld.mMap->GetTerrainPitch(inUnit->mPhysicalModel.GetLocation()) * 1.9098593f;
    if (theSlope < 0.0f)
        theSlope = 0.0f;
    else if (theSlope > 1.0f)
        theSlope = 1.0f;
    if (inUnit->GetVelocity() > (1.0f - theSlope * theTurnPenalty) * theMaxSpeed)
        theResult = 4;
    return theResult;
}

// FUNCTION: EXE 0x55dd12
// Handles the dying sea animal as a submarine does
void EEPMSeaAnimal::Step2(EEComplexUnit* inUnit)
{
    EEPMSubmergedNaval::Step2(inUnit);
}

// FUNCTION: EXE 0x55dd1e
// Runs the base physics model's step for state 3
void EEPMSeaAnimal::Step3(EEComplexUnit* inUnit)
{
    EEPhysicsModel::Step3(inUnit);
}

// FUNCTION: EXE 0x55dd2a
// Collides the sea animal as a submarine does
void EEPMSeaAnimal::Collide(EEComplexUnit* inUnit)
{
    EEPMSubmergedNaval::Collide(inUnit);
}

// FUNCTION: EXE 0x55dd36
// Brakes the sea animal to a stop as a submarine does
void EEPMSeaAnimal::Step4(EEComplexUnit* inUnit)
{
    EEPMSubmergedNaval::Step4(inUnit);
}

// FUNCTION: EXE 0x55dd42
// Turns the sea animal to its goal facing as a submarine does
void EEPMSeaAnimal::Step5(EEComplexUnit* inUnit)
{
    EEPMSubmergedNaval::Step5(inUnit);
}

// FUNCTION: EXE 0x55dd4e
// Clips the sea animal's step as a submarine does; returns whether the step stands
bool EEPMSeaAnimal::ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance)
{
    return EEPMSubmergedNaval::ClipStep(inUnit, inMove, ioLocation, outDistance);
}
