// C:\EEX\Empire Earth\Physics Thread\EEPMNaval.cpp (file name inferred: no __FILE__; RTTI EEPMNaval; one unit per
// physics-model class, alphabetical link order). Range 0x15b473..0x15d015; split out of a single EEPMLand.cpp
// (each unit ends with its own inline-COMDAT cluster).
#include <windows.h>
#include <vector>
#include <math.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/U2DPoint.h"
#include "lle/UMFastMath.h"
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
#include "ee/EEUGHealUnit.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGGather.h"
#include "ee/EEGraphicObject.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEEffectManager.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMathInline.h"

static const float kPMFiveDegrees = 0.08726647f;   // 0x4428fc (unit-local const: PushMoving's heading test; provisional name)
static const float kPMContactWindow = 0.25f;   // 0x442900 (unit-local const: PushMoving's contact-time tests; provisional name)
static const float kPMTurnEpsilon = 0.001f;   // 0x442904 (unit-local const; provisional name)
static const float kPMMinimumStep = 0.005f;   // 0x442910 (unit-local const, as EEPhysicsModel.cpp's; provisional name)
static const float kPMStopDistance = 0.02f;   // 0x442914 (unit-local const, as EEPhysicsModel.cpp's; provisional name)
static const float kPMMinimumSpeed = 1.0f;   // 0x442918 (unit-local const, as EEPhysicsModel.cpp's; provisional name)
static const float kPMMoveDistance = 0.03f;   // 0x44291c (unit-local const, as EEPhysicsModel.cpp's; provisional name)

extern unsigned long gCollisionPeriod;      // 0x4c237c (EEPhysics.cpp; provisional name)

// FUNCTION: EXE 0x55b473
// Sails the ship toward its next waypoint: turns, accelerates, clips the step against blocked tiles,
// marks it arrived when close and bobs it on the waves
void EEPMNaval::Step0(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), 0.0f);
    EEUnitClass* theClass = inUnit->mUnitClass;
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    if (inUnit->GetNextWaypointPoint(theTarget) != 0)
        return;
    theTarget.SetZ(0.0f);
    float theDistance = (theTarget.GetX() - theLocation.GetX()) * (theTarget.GetX() - theLocation.GetX())
        + (theTarget.GetY() - theLocation.GetY()) * (theTarget.GetY() - theLocation.GetY());
    theDistance = (float) sqrt(theDistance);
    if (theDistance < 0.05f)
        theDistance = 0.0f;
    float theTurn;
    float theYawRate;
    float theYaw;
    this->TurnToward(inUnit, theLocation, theTarget, &theTurn, &theYawRate, &theYaw, false);
    GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
    theAngularVelocity.mPitch = 0.0f;
    theAngularVelocity.mRoll = 0.0f;
    theAngularVelocity.mYaw = theYawRate;
    theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    float theSpeed = this->Accelerate(inUnit, theDistance, theTurn);
    inUnit->SetVelocity(theSpeed);
    theForward.mY = theSpeed * theStep;
    theTransformation.Apply(theForward, theMove);
    theLocation += theMove;
    bool theMoved = this->ClipStep(inUnit, theMove, theLocation, &theYaw);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    if (!theMoved)
        theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    bool theFlag = theClass->HasArrived(inUnit, theDistance);
    if (inUnit->mHasArrived != theFlag)
        inUnit->mHasArrived = theFlag;
    inUnit->SetTransformation(theTransformation, true);
    this->ApplyBob(inUnit);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55b668
// Bobs the ship on the waves: advances its bob phase with game time and sets its pitch and depth from
// the class's bob pitch and bob height
void EEPMNaval::ApplyBob(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    theTransformation.Apply(theForward, theResult);
    float theYaw = UMArcTan2(-theResult.mX, theResult.mY);
    float thePhase = gPhysics->GetTickTime() * 0.0015707965f + inUnit->GetF220();
    while (thePhase > 3.1415927f)
        thePhase -= 6.2831855f;
    if (thePhase != inUnit->mF220)
        inUnit->mF220 = thePhase;
    float theBob = (UMCos(1.5707964f - thePhase) + 1.0f) * 0.5f;
    float thePitch = (theBob - 0.5f) * inUnit->mUnitClass->GetBobPitch();
    theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
    theTransformation.SetPosition(theTransformation.GetX(), theTransformation.GetY(), -(inUnit->mUnitClass->GetBobHeight() * theBob));
    inUnit->SetTransformation(theTransformation, true);
}

// FUNCTION: EXE 0x55b786
// Turns the ship toward the unit it attacks or heals (or its goal location) while braking to a stop,
// and bobs it; a ship with model parts only spins its turn down
void EEPMNaval::Step1(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), 0.0f);
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
    EEUGHealUnit* theHeal = dynamic_cast<EEUGHealUnit*>(inUnit->GetGoal());
    float theBrake = inUnit->GetAcceleration();
    float theSpeed = inUnit->GetVelocity();
    float theStep = gPhysics->mTickSeconds;
    if (theAttack && theAttack->mTarget->mComplexUnit) {
        theTarget.SetX(theAttack->mTarget->mComplexUnit->mPhysicalModel.GetLocation().GetX());
        theTarget.SetY(theAttack->mTarget->mComplexUnit->mPhysicalModel.GetLocation().GetY());
    } else if (theHeal) {
        theTarget.SetX(theHeal->GetTarget()->mPhysicalModel.GetLocation().GetX());
        theTarget.SetY(theHeal->GetTarget()->mPhysicalModel.GetLocation().GetY());
    } else {
        inUnit->mGoalState->GetGoalLocation(theTarget);
    }
    theTarget.SetZ(0.0f);
    float theTurn;
    float theYawRate;
    float theYaw;
    if (inUnit->GetModel()->GetNumParts() == 0)
        this->TurnToward(inUnit, theLocation, theTarget, &theTurn, &theYawRate, &theYaw, true);
    else
        this->SpinDown(inUnit, &theYawRate, &theYaw);
    GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
    theAngularVelocity.mPitch = 0.0f;
    theAngularVelocity.mRoll = 0.0f;
    theAngularVelocity.mYaw = theYawRate;
    theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    float theNew = theSpeed - theStep * theBrake;
    if (theNew < 0.0f)
        theNew = 0.0f;
    inUnit->SetVelocity(theNew);
    theForward.mY = theNew * theStep;
    theTransformation.Apply(theForward, theMove);
    theLocation += theMove;
    bool theMoved = this->ClipStep(inUnit, theMove, theLocation, &theYaw);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    if (!theMoved)
        theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    inUnit->SetTransformation(theTransformation, true);
    this->ApplyBob(inUnit);
}

// FUNCTION: EXE 0x55b9d1
// Moves the ship by the push it collected from collisions (capped at its top speed) when it can stand on
// the new tile, plays or stops the idle push animation, then clears the push
void EEPMNaval::Move(EEComplexUnit* inUnit)
{
    EETileCoordinate theTile(0, 0);
    bool theBlocked = false;
    float theLimit = kPMMinimumStep / gPhysics->GetTickSeconds();
    if (inUnit->GetMaxVelocity() > theLimit)
        theLimit = inUnit->GetMaxVelocity();
    float theTicks = (float) gCollisionPeriod;
    float theDX = inUnit->mPushVelocityX;
    theDX *= theTicks;
    float theDY = inUnit->mPushVelocityY;
    theDY *= theTicks;
    float theSignX;
    if (theDX > 0.0f)
        theSignX = 1.0f;
    else
        theSignX = -1.0f;
    float theSignY;
    if (theDY > 0.0f)
        theSignY = 1.0f;
    else
        theSignY = -1.0f;
    if (theDX < 0.0f)
        theDX = -theDX;
    if (theDY < 0.0f)
        theDY = -theDY;
    if (theDY + theDX < kPMStopDistance) {
        theDY = 0.0f;
        theDX = 0.0f;
    }
    if (theDX != 0.0f || theDY != 0.0f) {
        if (theDX > theLimit || theDY > theLimit) {
            if (theDX > theDY) {
                theDY = theLimit / theDX * theDY;
                theDX = theLimit;
            } else {
                theDX = theLimit / theDY * theDX;
                theDY = theLimit;
            }
        }
        float theX = gPhysics->GetTickSeconds() * theSignX * theDX;
        theX += inUnit->mPhysicalModel.GetLocation().GetX();
        float theY = gPhysics->GetTickSeconds() * theSignY * theDY;
        theY += inUnit->mPhysicalModel.GetLocation().GetY();
        long theTileY = UMFloor(theY);
        long theTileX = UMFloor(theX);
        theTile.mX = theTileX;
        theTile.mY = theTileY;
        bool theOk = EECanStandOn(theTile, inUnit);
        if (theOk && UMAbs(inUnit->GetTileX() - theTile.mX) == 1 && UMAbs(inUnit->GetTileY() - theTile.mY) == 1) {
            EETileCoordinate theSide1;
            theSide1.mX = theTile.mX;
            theSide1.mY = inUnit->GetTileY();
            if (!EECanStandOn(theSide1, inUnit)) {
                EETileCoordinate theSide2;
                theSide2.mX = inUnit->GetTileX();
                theSide2.mY = theTile.mY;
                if (!EECanStandOn(theSide2, inUnit))
                    theOk = false;
            }
        }
        if (theOk) {
            float theTime = inUnit->mCollisionResolutionCount;
            if ((theTile.mX != inUnit->GetTileX() || theTile.mY != inUnit->GetTileY()) && theTime == 0.0f
                && inUnit->mF24C != 0.0f)
                theTime = gPhysics->GetTickTime() + inUnit->GetF24C();
            inUnit->SetLocationXY(theX, theY, true);
            inUnit->mCollisionResolutionCount = theTime;
            if (inUnit->mCurrentPhysicsAction == 5 && inUnit->mCollisionRadius >= kPMMinimumSpeed) {
                inUnit->mAngularVelocity = GEOrientation(0.0f, 0.0f, 0.0f);
                EEPhysicsTrace("Collision: Turning Unit %08x angular velocity set to %f\n", inUnit->GetUnitID(), GEOrientation(0.0f, 0.0f, 0.0f));
            }
            if (inUnit->mCurrentPhysicsAction != 5 && inUnit->mCurrentPhysicsAction != 0) {
                if (theDY + theDX > kPMMoveDistance) {
                    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionIdle
                        && (inUnit->mAnimation == 0 || inUnit->mAnimation == 7 || inUnit->mAnimation == 8)) {
                        inUnit->SetAnimation(0xd, -1, 1);
                        if (inUnit->mIsAnimationPaused)
                            inUnit->mIsAnimationPaused = false;
                        unsigned long theFrames = inUnit->GetModel()->mFrameTimes[13];
                        if (inUnit->mAnimationTime != theFrames)
                            inUnit->mAnimationTime = theFrames;
                    } else if (inUnit->mAnimation == 0xd) {
                        if (inUnit->mIsAnimationPaused)
                            inUnit->mIsAnimationPaused = false;
                    }
                } else {
                    theBlocked = true;
                }
            }
            inUnit->mCollisionResolutionCount += gPhysics->GetTickTime();
            inUnit->SetTransformation(inUnit->mPhysicalModel.GetLocation(), true);
        } else {
            theBlocked = true;
        }
    } else {
        theBlocked = true;
    }
    if (theBlocked && (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionIdle && inUnit->mAnimation == 0xd)
        inUnit->SetAnimation(7, -1, 1);
    if (inUnit->mPushVelocityX != 0.0f)
        inUnit->mPushVelocityX = 0.0f;
    if (inUnit->mPushVelocityY != 0.0f)
        inUnit->mPushVelocityY = 0.0f;
}

// FUNCTION: EXE 0x55be0e
// Runs the base physics model's after-step work for the ship
void EEPMNaval::AfterStep(EEComplexUnit* inUnit)
{
    EEPhysicsModel::AfterStep(inUnit);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55be1a
// Sinks the ship: tips its bow up or down toward 72 degrees (rolling smaller ships) and lowers it faster
// the steeper it tips
void EEPMNaval::Step2(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GEOrientation theRate(inUnit->mAngularVelocity);
    EEUnitClass* theClass = inUnit->mUnitClass;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    float theStep = gPhysics->mTickSeconds;
    float theRoll = 0.0f;
    theTransformation.Apply(theForward, theResult);
    float theYaw = UMArcTan2(-theResult.mX, theResult.mY);
    float thePitch = UMArcCos(-theResult.mZ) - 1.5707964f;
    float theRatePitch;
    float theNewPitch;
    if (theYaw < -0.7853982f || theYaw > 2.3561945f || theRate.mPitch < 0.0f) {
        if (thePitch > -1.2566371f) {
            this->ApproachPitch(inUnit, thePitch, -1.2566371f, &theRatePitch, &theNewPitch);
            theRate.SetPitch(theRatePitch);
            thePitch = theNewPitch;
        }
    } else if (thePitch < 1.2566371f) {
        this->ApproachPitch(inUnit, thePitch, 1.2566371f, &theRatePitch, &theNewPitch);
        theRate.SetPitch(theRatePitch);
        thePitch = theNewPitch;
    }
    if (theClass->GetScale() <= 3.0f) {
        if (theYaw >= -2.3561945f && theYaw <= 0.7853982f)
            theRoll = (thePitch < 0.0f ? -thePitch : thePitch) * -0.5f;
        else
            theRoll = (thePitch < 0.0f ? -thePitch : thePitch) * 0.5f;
    }
    theTransformation.SetOrientationYPR(theYaw, thePitch, theRoll);
    inUnit->mAngularVelocity = theRate;
    float theSink = thePitch < 0.0f ? -thePitch : thePitch;
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(),
                                  theLocation.GetZ() - (theSink * 0.31830987f) * theClass->mMaxVelocity * theStep);
    inUnit->SetTransformation(theTransformation, true);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55c047
// Accelerates or brakes the ship's pitch rate toward the target pitch and returns the new rate and pitch;
// the rate is capped by the class's turn rate
void EEPMNaval::ApproachPitch(EEComplexUnit* inUnit, float inCurrent, float inTarget, float* outRate, float* outNew)
{
    float theStep = gPhysics->mTickSeconds;
    inTarget = inTarget - inCurrent;
    float theAccel = inUnit->mUnitClass->GetAngularAcceleration();
    float theMax = inUnit->mUnitClass->GetMaxAngularVelocity();
    float theRate = inUnit->mAngularVelocity.mPitch;
    switch (this->PitchMode(inUnit, theRate, inTarget)) {
    case 2:
        break;
    case 3:
        if (inTarget < 0.0f) {
            theRate = theRate - theAccel * theStep;
            if (theRate < -theMax)
                theRate = -theMax;
        } else {
            theRate = theAccel * theStep + theRate;
            if (theMax < theRate)
                theRate = theMax;
        }
        break;
    case 4:
        if (inTarget < 0.0f) {
            theRate = 1.0f * theAccel * theStep + theRate;
            if (theRate <= 0.0f)
                break;
        } else {
            theRate = theRate - theAccel * theStep;
            if (theRate >= 0.0f)
                break;
        }
    default:
        theRate = 0.0f;
        break;
    }
    *outRate = theRate;
    *outNew = theRate * theStep + inCurrent;
}

// FUNCTION: EXE 0x55c146
// Returns 4 (brake) when the pitch rate needs the whole remaining angle to stop, else 3 (accelerate)
long EEPMNaval::PitchMode(EEComplexUnit* inUnit, float inRate, float inAngle)
{
    if (inAngle < 0.0f)
        inAngle = -inAngle;
    if (inRate < 0.0f)
        inRate = -inRate;
    EEUnitClass* theClass = inUnit->mUnitClass;
    float theTime = gPhysics->mTicksPerGameSecond;
    float theTicks = theTime * inRate / theClass->GetAngularAcceleration();
    float theStep = gPhysics->mTickSeconds;
    float theStop = (theTicks + 1.0f) * theTicks * 0.5f;
    theStop = theStop * (theStep * theStep) * theClass->GetAngularAcceleration();
    return inAngle <= theStop ? 4 : 3;
}

// NEAR: GE3DLine/GE3DPoint copies, x87 order
// FUNCTION: EXE 0x55c1cb
// Resolves a moving ship running into another: pushes the smaller ship aside, or stops, slows or
// backs off this ship (sending a stuck one idle); returns true when it gave up its goals
bool EEPMNaval::PushMoving(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    GEVector theOtherDirection;
    GE3DLine theLine;
    GE3DLine theOtherLine;
    GE3DLine theCrossLine;
    GE3DPoint theLocation;
    GE3DPoint theOtherLocation;
    float theSpeed = inUnit->mCollisionRadius;
    bool theResult = false;
    float theOtherSpeed = inOther->mCollisionRadius;
    float theSlow = 0.75f;
    float theT = 0.0f;
    float theOtherT = 0.0f;
    float theSign = 1.0f;
    float theOtherSign = 1.0f;
    float theCrossT;
    float theCrossOtherT;
    const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
    theTransformation.Apply(theForward, theDirection);
    const GETransformation& theOtherTransformation = inOther->mPhysicalModel.GetLocation();
    theOtherTransformation.Apply(theForward, theOtherDirection);
    theDirection.Normalize();
    theOtherDirection.Normalize();
    float theAngle = UMAbs(UMArcCos(theDirection.DotProduct(theOtherDirection)));
    while (theAngle > 3.1415927f)
        theAngle -= 3.1415927f;
    if (theAngle > 1.5707964f)
        theAngle = 3.1415927f - theAngle;
    theDirection.mX = theDirection.mX * theSpeed;
    theDirection.mY = theDirection.mY * theSpeed;
    theDirection.mZ = theDirection.mZ * theSpeed;
    theOtherDirection.mX = theOtherDirection.mX * theOtherSpeed;
    theOtherDirection.mY = theOtherDirection.mY * theOtherSpeed;
    theOtherDirection.mZ = theOtherDirection.mZ * theOtherSpeed;
    theLocation.mX = theTransformation.GetX();
    theLocation.mY = theTransformation.GetY();
    theLocation.mZ = theTransformation.GetZ();
    theOtherLocation.mX = theOtherTransformation.GetX();
    theOtherLocation.mY = theOtherTransformation.GetY();
    theOtherLocation.mZ = theOtherTransformation.GetZ();
    theLine.SetStart(theLocation);
    theOtherLine.SetStart(theOtherLocation);
    theLine.SetDirection(theDirection);
    theOtherLine.SetDirection(theOtherDirection);
    bool theCrossing = theLine.IntersectsIn2D(theOtherLine, theT, theOtherT);
    theCrossLine.SetStart(theOtherLine.GetStart());
    theCrossLine.SetDirection(theOtherLine.GetDirection());
    float theX = -theOtherDirection.mX;
    theOtherDirection.mX = theOtherDirection.GetY();
    theOtherDirection.mY = theX;
    theCrossLine.SetDirection(theOtherDirection);
    bool theNear;
    if (theLine.IntersectsIn2D(theCrossLine, theCrossT, theCrossOtherT)
        && (theCrossOtherT < 0.0f ? -theCrossOtherT : theCrossOtherT) * theOtherSpeed <= 0.75f
        && (theCrossT > 0.0f || inUnit->GetVelocity() == 0.0f) && theAngle <= kPMFiveDegrees) {
        theNear = true;
        if (theCrossT > 0.0f)
            theT = 0.5f;
        else
            theT = -0.5f;
    } else {
        theNear = false;
    }
    if (theCrossing) {
        if (theT > 0.0f && theT < kPMContactWindow)
            theT = -theT;
        if (theOtherT > 0.0f && theOtherT < kPMContactWindow)
            theOtherT = -theOtherT;
        if (theT > 0.0f)
            theSign = -1.0f;
        else
            theSign = 1.0f;
        if (theOtherT > 0.0f)
            theOtherSign = -1.0f;
        else
            theOtherSign = 1.0f;
        if (theT < 0.0f)
            theT = -theT;
        if (theOtherT < 0.0f)
            theOtherT = -theOtherT;
    }
    if (!theNear && (!theCrossing || theT >= 0.75f || theOtherT >= 0.75f))
        return theResult;
    EEUnitGoal* theGoal = inOther->mGoalState;
    if (theOtherSpeed <= theSpeed && inOther->mPushImmuneUntil < gPhysics->mGameTime && (!theGoal || theGoal->mKind != kGoalAttackUnit)
        && !theNear) {
        float theDX = inOther->GetMaxVelocity() * theOtherLine.GetDirection().GetX() * theOtherSign;
        float theDY = inOther->GetMaxVelocity();
        theDY *= theOtherLine.GetDirection().GetY() * theOtherSign;
        if (theOtherSign < 0.0f && theOtherT > theT) {
            inOther->SetVelocity(0.0f);
            inOther->SetAngularVelocity(GEOrientation(0.0f, 0.0f, 0.0f));
        }
        inOther->SetPushVelocityX(inOther->GetPushVelocityX() + theDX);
        inOther->SetPushVelocityY(inOther->GetPushVelocityY() + theDY);
        return theResult;
    }
    if (inUnit->GetVelocity() > 0.0f && theNear) {
        if (inUnit->mCollisionState == 2) {
            inUnit->SetBlockingUnit(NULL);
            inUnit->mCollisionResolutionCount = 0.0f;
            inUnit->SetVelocity(0.0f);
            inUnit->SetCurrentPhysicsAction(6);
            inUnit->ClearGoalQueue(0);
            if (inUnit->mCollisionState != 0)
                inUnit->mCollisionState = 0;
            theResult = true;
            EEUnitGoal* theOwnGoal = inUnit->mGoalState;
            if (theOwnGoal && theOwnGoal->mKind == kGoalGather && static_cast<EEUGGather*>(theOwnGoal)->mResource
                && static_cast<EEUGGather*>(theOwnGoal)->mResource->mResourceType == kResourceWood)
                inUnit->StartAction(kActionFindNewResource);
            else
                inUnit->StartAction(kActionIdle);
            inUnit->mUnitClass->ActionChanged(inUnit);
        } else if (inUnit->mCollisionState == 0 && inOther->GetVelocity() == 0.0f) {
            inUnit->SetBlockingUnit(inOther);
        } else {
            theSlow = (inUnit->GetVelocity() < theT ? theT : inUnit->GetVelocity()) - 0.5f;
        }
    }
    if (inUnit->GetVelocity() == 0.0f && inUnit->mAngularVelocity.GetYaw() == 0.0f && inOther->GetVelocity() == 0.0f && inOther->mAngularVelocity.GetYaw() == 0.0f) {
        inUnit->SetPushVelocityX(kPMTurnEpsilon);
        inUnit->SetPushVelocityY(kPMTurnEpsilon);
        inUnit->mPushImmuneUntil = gPhysics->mGameTime + 5000;
    } else {
        theSpeed = theSlow + 0.5f;
        float theDX = inUnit->GetMaxVelocity() * (theSpeed - theT) * theLine.GetDirection().GetX() * theSign;
        float theDY = inUnit->GetMaxVelocity() * (theSpeed - theT) * theLine.GetDirection().GetY() * theSign;
        inUnit->SetPushVelocityX(inUnit->GetPushVelocityX() + theDX);
        inUnit->SetPushVelocityY(inUnit->GetPushVelocityY() + theDY);
        if (theSign < 0.0f) {
            inUnit->SetVelocity(0.0f);
            GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
            theAngularVelocity.mYaw = 0.0f;
            theAngularVelocity.mPitch = 0.0f;
            theAngularVelocity.mRoll = 0.0f;
        }
    }
    if (theSlow > 0.0f) {
        inUnit->SetVelocity(inUnit->GetVelocity() * 0.5f);
    } else {
        inUnit->SetVelocity(0.0f);
        inUnit->mCollisionResolutionCount += gPhysics->GetTickTime();
    }
    return theResult;
}

// FUNCTION: EXE 0x55c8f5
// Levels the ship: puts it back at water level with no pitch or roll
void EEPMNaval::RemoveBob(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    theTransformation.SetZ(0.0f);
    theTransformation.Apply(theForward, theResult);
    float theYaw = UMArcTan2(-theResult.mX, theResult.mY);
    theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    inUnit->SetTransformation(theTransformation, true);
}

// Collides the ship as the base physics model does
// ICF copy: LINK kept EEPMSpaceship's identical body at 0x55e754 (EEPMSpaceship.cpp)
void EEPMNaval::Collide(EEComplexUnit* inUnit)
{
    EEPhysicsModel::Collide(inUnit);
}

// NEAR: second transformation built from the orientation (SetOrientationYPR, then translation 0 and row 3 0,0,0,1)
// FUNCTION: EXE 0x55c987
// Brakes the ship's speed and turn rate toward a stop, moving it by what is left (levelled once both
// are zero), and bobs it
void EEPMNaval::Step4(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), 0.0f);
    GEOrientation theRate(0.0f, 0.0f, 0.0f);
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    float theYawRate;
    float theYaw;
    this->SpinDown(inUnit, &theYawRate, &theYaw);
    theRate.mYaw = theYawRate;
    inUnit->SetAngularVelocity(theRate);
    GETransformation theNew(false);
    theNew.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    theNew.SetPosition(0.0f, 0.0f, 0.0f);
    theNew.SetElement(3, 0, 0.0f);
    theNew.SetElement(3, 1, 0.0f);
    theNew.SetElement(3, 2, 0.0f);
    theNew.SetElement(3, 3, 1.0f);
    float theSpeed = inUnit->GetVelocity();
    theSpeed = theSpeed - inUnit->GetAcceleration() * theStep;
    if (theSpeed < 0.0f)
        theSpeed = 0.0f;
    if (inUnit->GetMaxVelocity() < theSpeed)
        theSpeed = inUnit->GetMaxVelocity();
    inUnit->SetVelocity(theSpeed);
    if (theYawRate == 0.0f && theSpeed == 0.0f) {
        this->RemoveBob(inUnit);
    } else {
        theForward.mY = theSpeed * theStep;
        theNew.Apply(theForward, theMove);
        theLocation += theMove;
        bool theMoved = this->ClipStep(inUnit, theMove, theLocation, &theYaw);
        theNew.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
        if (!theMoved)
            theNew.SetOrientationYPR(theYaw, 0.0f, 0.0f);
        inUnit->SetTransformation(theNew, true);
    }
    this->ApplyBob(inUnit);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55cb5f
// Turns a stopped ship in place to the facing its location goal asks for and bobs it; a still-moving
// ship brakes first (Step4); does nothing without a location goal
void EEPMNaval::Step5(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    EEUGLocation* theGoal = dynamic_cast<EEUGLocation*>(inUnit->GetGoal());
    if (!theGoal)
        return;
    if (inUnit->GetVelocity() != 0.0f) {
        this->Step4(inUnit);
    } else {
        theTransformation.Apply(theForward, theResult);
        float theHeading = UMArcTan2(-theResult.mX, theResult.mY);
        float theLeft;
        float theYawRate;
        float theYaw;
        this->TurnTowardAngle(inUnit, theHeading, theGoal->GetYaw(), &theLeft, &theYawRate, &theYaw, false);
        GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
        theAngularVelocity.mPitch = 0.0f;
        theAngularVelocity.mRoll = 0.0f;
        theAngularVelocity.mYaw = theYawRate;
        theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
        theTransformation.SetZ(0.0f);
        inUnit->SetTransformation(theTransformation, true);
        this->ApplyBob(inUnit);
    }
}

// NEAR: register/slot allocation and x87 order
// Unless a blocker on the waypoint's tile stops it, every call adds 0.33 tick to the collision-resolution
// time, retry or not (original: bytes 0x55cfe6 test only that flag)
// FUNCTION: EXE 0x55cc7f
// Checks the ship's step onto a new tile (terrain, edge, shallows, blocking units, diagonal corners);
// when blocked it retries without x, then y, else undoes it; returns whether the step stands
bool EEPMNaval::ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance)
{
    GE3DPoint theTarget;
    EEUnit* theBlocker = NULL;
    float theDistance = 0.0f;
    EEMap* theMap = ::gWorld.mMap;
    long theX = UMFloor(ioLocation.GetX());
    long theY = UMFloor(ioLocation.GetY());
    bool theTriedX = false;
    bool theTriedY = false;
    bool theSlowed = true;
    EETile* theTile = theMap->GetTile(theX, theY);
    bool theResult = theTile != NULL;
    if (theTile != NULL) {
        do {
            if (!theResult)
                break;
            if (theTile != inUnit->mTile) {
                if (!inUnit->mUnitClass->CanMoveOnTerrain(inUnit, theTile->mTerrainType) || theMap->IsEdgeTile(theX, theY)) {
                    theResult = false;
                } else {
                    theResult = true;
                    if (theTile->mAIValue) {
                        if ((char) theTile->mAIValue < 0)
                            theResult = inUnit->mWaterState == 0;
                        else
                            theResult = inUnit->mWaterState != 3;
                    }
                    RTSTileUnitNode* theNode = theTile->mUnits;
                    if (theResult) {
                        while (theNode && theResult) {
                            theBlocker = theNode->mUnit;
                            theResult = theBlocker->IsPassableBy(inUnit);
                            theNode = theNode->mNext;
                        }


                        if (theResult) {
                            if (inUnit->GetTileX() != theX && inUnit->GetTileY() != theY) {
                                EETileCoordinate theSide1;
                                theSide1.mX = theX;
                                theSide1.mY = inUnit->GetTileY();
                                if (!EECanStandOn(theSide1, inUnit)) {
                                    EETileCoordinate theSide2;
                                    theSide2.mX = inUnit->GetTileX();
                                    theSide2.mY = theY;
                                    if (!EECanStandOn(theSide2, inUnit))
                                        theResult = false;
                                }
                            }
                            if (theResult) {
                                theTile = NULL;
                                continue;
                            }
                        }
                    }
                }
                if (theBlocker && inUnit->GetNextWaypointPoint(theTarget) == 0 && UMFloor(theTarget.GetX()) == theX
                    && UMFloor(theTarget.GetY()) == theY) {
                    inUnit->SetBlockingUnit(theBlocker);
                    if (inUnit->mCollisionState != 2)
                        inUnit->mCollisionState = 2;
                    ioLocation -= inMove;
                    theTile = NULL;
                    theSlowed = false;
                }
                if (theSlowed) {
                    if (!theTriedX) {
                        ioLocation.SetX(ioLocation.GetX() - inMove.GetX());
                        theX = UMFloor(ioLocation.GetX());
                        theTile = theMap->GetTile(theX, theY);
                        theResult = theTile != NULL;
                        theTriedX = true;
                    } else if (!theTriedY) {
                        ioLocation.SetX(ioLocation.GetX() + inMove.GetX());
                        ioLocation.SetY(ioLocation.GetY() - inMove.GetY());
                        theX = UMFloor(ioLocation.GetX());
                        theY = UMFloor(ioLocation.GetY());
                        theTile = theMap->GetTile(theX, theY);
                        theResult = theTile != NULL;
                        theTriedY = true;
                    } else {
                        ioLocation.SetX(ioLocation.GetX() - inMove.GetX());
                        theTile = NULL;
                    }
                }
            } else {
                theTile = NULL;
            }
        } while (theTile != NULL);
    } else {
        ioLocation.SetX(ioLocation.GetX() - inMove.GetX());
        ioLocation.SetY(ioLocation.GetY() - inMove.GetY());
        theResult = false;
    }
    if (theResult)
        theResult = EEPhysicsModel::ClipStep(inUnit, inMove, ioLocation, &theDistance);
    if (theSlowed && (theTriedX || theTriedY))
        inUnit->mCollisionResolutionCount += gPhysics->GetTickTime() * 0.33f;
    return theResult;
}
