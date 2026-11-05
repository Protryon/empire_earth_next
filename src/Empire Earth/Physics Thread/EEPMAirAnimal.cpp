// C:\EEX\Empire Earth\Physics Thread\EEPMAirAnimal.cpp (file name inferred: no __FILE__; RTTI EEPMAirAnimal; one unit per
// physics-model class, alphabetical link order). Range 0x156df8..0x15726c; split out of a single EEPMLand.cpp
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
#include "ee/EEPhysicsModel.h"

// NEAR: scale-by-height factored by VC6 to (a+1)*0.5; mU17C store order
// FUNCTION: EXE 0x556df8
// Flies a flying animal toward its next waypoint at its height (straight on without one), turning,
// pitching and accelerating; carrier-type ones shrink below cruise altitude
void EEPMAirAnimal::Step0(EEComplexUnit* inUnit)
{
    GEPhysicalModel& theModel = inUnit->mPhysicalModel;
    GETransformation theTransformation(theModel.GetLocation());
    GE3DPoint theLocation;
    EEUnitClass* theClass = inUnit->mUnitClass;
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    if (this->IsDocked(inUnit) == true) {
        this->FollowCarrier(inUnit);
        theTransformation = theModel.GetLocation();
    }
    theLocation.SetX(theTransformation.GetX());
    theLocation.SetY(theTransformation.GetY());
    theLocation.SetZ(theTransformation.GetZ());
    if (theClass->mAirTrafficType == 2) {
        if (theLocation.GetZ() >= theClass->GetCruiseAltitude())
            theModel.SetScale(inUnit->mStats->GetScale());
        else {
            float theScale = theLocation.GetZ() / theClass->GetCruiseAltitude() * 0.5f;
            theScale += 0.5f;
            theModel.SetScale(theScale * inUnit->mStats->GetScale());
        }
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
}

// FUNCTION: EXE 0x557140
// Returns the speed mode: slowing (4) when descending or braking faster than 3/4 of top speed plus
// the home base's speed, the base mode when taxiing (not carrier-type), else cruising (3)
long EEPMAirAnimal::SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn)
{
    float theLimit = inUnit->mStats->GetMaxVelocity() * 0.75f + (inUnit->mHomeBase ? inUnit->mHomeBase->mVelocity : 0.0f);
    bool theCarried = inUnit->mUnitClass->mAirTrafficType == 2;
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionDescend
        || (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionContinueToDescend
        || (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionBrake
        || (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionContinueToBrake) {
        EEPhysicsModel::SpeedMode(inUnit, inDistance, inTurn);
        if (inUnit->mVelocity > theLimit)
            return 4;
        return 3;
    }
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionTaxi && !theCarried)
        return EEPhysicsModel::SpeedMode(inUnit, inDistance, inTurn);
    return 3;
}

// FUNCTION: EXE 0x557229
// Returns the displayed pitch as an airplane does
float EEPMAirAnimal::ComputePitch(EEComplexUnit* inUnit, float inValue)
{
    return EEPMAirplane::ComputePitch(inUnit, inValue);
}

// FUNCTION: EXE 0x55723d
// Returns the displayed bank for the turn rate as an airplane does
float EEPMAirAnimal::ComputeRoll(EEComplexUnit* inUnit, float inValue)
{
    return EEPMAirplane::ComputeRoll(inUnit, inValue);
}

// FUNCTION: EXE 0x557251
// Moves the falling flying animal as a crashing airplane
void EEPMAirAnimal::Step2(EEComplexUnit* inUnit)
{
    EEPMAirplane::Step2(inUnit);
}

// FUNCTION: EXE 0x55725d
// Slows the flying animal as an airplane
void EEPMAirAnimal::Step4(EEComplexUnit* inUnit)
{
    EEPMAirplane::Step4(inUnit);
}

// FUNCTION: EXE 0x557269
// Does nothing: aircraft get no collision response
void EEPMAirplane::Collide(EEComplexUnit* inUnit)
{
}

// ICF-FOLDED: EXE 0x55d8de
// Turns toward the target point in physics order 5; otherwise keeps circling, speeding its turn rate up
// (one way in order 0, the other way else)
// (no address of its own: the same body as EEPMSeaAnimal::TurnToward, which ICF kept)
void EEPMAirAnimal::TurnToward(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float* outA, float* outB, float* outC, bool inUseFacingOffset)
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

// ICF-FOLDED: EXE 0x51c531
// Collides as an airplane (`push [esp+4]; call EEPMAirplane::Collide; ret 4`)
// (no address of its own: the same bytes as EEMap::GetChecksum's forwarding call, which ICF kept)
void EEPMAirAnimal::Collide(EEComplexUnit* inUnit)
{
    EEPMAirplane::Collide(inUnit);
}
