// C:\EEX\Empire Earth\Physics Thread\EEPMSpaceship.cpp (file name inferred: no __FILE__; RTTI EEPMSpaceship; one unit per
// physics-model class, alphabetical link order). Range 0x15dd66..0x15f49d; split out of a single EEPMLand.cpp
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
#include "ee/EEUGHealUnit.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGGather.h"
#include "ee/EEGraphicObject.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEEffectManager.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMathInline.h"

static const float kPMFiveDegrees = 0.08726647f;   // 0x442940 (unit-local const: PushMoving's heading test; provisional name)
static const float kPMContactWindow = 0.25f;   // 0x442944 (unit-local const: PushMoving's contact-time tests; provisional name)
static const float kPMTurnEpsilon = 0.001f;   // 0x442948 (unit-local const; provisional name)
static const float kPMMinimumStep = 0.005f;   // 0x442950 (unit-local const, as EEPhysicsModel.cpp's; provisional name)
static const float kPMStopDistance = 0.02f;   // 0x442954 (unit-local const, as EEPhysicsModel.cpp's; provisional name)
static const float kPMMinimumSpeed = 1.0f;   // 0x442958 (unit-local const, as EEPhysicsModel.cpp's; provisional name)
static const float kPMMoveDistance = 0.03f;   // 0x44295c (unit-local const, as EEPhysicsModel.cpp's; provisional name)

extern unsigned long gCollisionPeriod;      // 0x4c237c (EEPhysics.cpp; provisional name)

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55dd66
// Flies the spaceship toward its next waypoint at its height (a planet spaceship without one flies
// straight on and pitches with the slope); marks it arrived when close, sets its next height and bobs it
void EEPMSpaceship::Step0(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    EEUnitClass* theClass = inUnit->GetUnitClass();
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    float thePitchRate = 0.0f;
    float thePitch = 0.0f;
    float theDistance;
    if (inUnit->GetNextWaypointPoint(theTarget) == 0) {
        theTarget.SetZ(inUnit->mHeight);
        theDistance = (theTarget.GetX() - theLocation.GetX()) * (theTarget.GetX() - theLocation.GetX())
            + (theTarget.GetY() - theLocation.GetY()) * (theTarget.GetY() - theLocation.GetY());
        theDistance = (float) sqrt(theDistance);
        if (theDistance < 0.05f)
            theDistance = 0.0f;
    } else {
        if (inUnit->GetUnitClass()->mCategory != kUnitCategoryPlanetSpaceship)
            return;
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
    this->TurnToward(inUnit, theLocation, theTarget, &theTurn, &theYawRate, &theYaw, false);
    if (inUnit->GetUnitClass()->mCategory == kUnitCategoryPlanetSpaceship)
        EEPhysicsModel::PitchTowardSlope(inUnit, theLocation, theTarget, theDistance, &thePitchRate, &thePitch);
    GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
    theAngularVelocity.SetPitch(thePitchRate);
    theAngularVelocity.mRoll = 0.0f;
    theAngularVelocity.mYaw = theYawRate;
    float theRoll = inUnit->GetRoll();
    theTransformation.SetOrientationYPR(theYaw, thePitch, theRoll);
    float theSpeed = this->Accelerate(inUnit, theDistance, theTurn);
    inUnit->SetVelocity(theSpeed);
    theForward.mY = theSpeed * theStep;
    theTransformation.Apply(theForward, theMove);
    theLocation += theMove;
    bool theMoved = this->ClipStep(inUnit, theMove, theLocation, &theYaw);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    if (!theMoved)
        theTransformation.SetOrientationYPR(theYaw, thePitch, inUnit->GetRoll());
    bool theFlag = theClass->HasArrived(inUnit, theDistance);
    if (inUnit->mHasArrived != theFlag)
        inUnit->mHasArrived = theFlag;
    inUnit->SetTransformation(theTransformation, true);
    if ((long) inUnit->mWaypoints.size() > 0) {
        GE3DPoint theNext;
        inUnit->GetNextWaypointPoint(theNext);
        inUnit->SetHeight(inUnit->GetUnitClass()->GetHeightAt(inUnit, theNext));
    }
    this->ApplyBob(inUnit);
}

// FUNCTION: EXE 0x55e047
// Turns the spaceship toward the unit it attacks or heals (or its goal location) while braking to a stop;
// against a planet spaceship (except a capital ship's special attack) or with model parts it only spins down
void EEPMSpaceship::Step1(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
    EEUGHealUnit* theHeal = dynamic_cast<EEUGHealUnit*>(inUnit->GetGoal());
    float theBrake = inUnit->GetAcceleration();
    bool theAirTarget = false;
    float theSpeed = inUnit->GetVelocity();
    float theStep = gPhysics->mTickSeconds;
    if (theAttack && theAttack->mTarget->mComplexUnit) {
        theAirTarget = theAttack->mTarget->mComplexUnit->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship;
        theTarget.SetX(theAttack->mTarget->mComplexUnit->mPhysicalModel.GetLocation().GetX());
        theTarget.SetY(theAttack->mTarget->mComplexUnit->mPhysicalModel.GetLocation().GetY());
    } else if (theHeal) {
        theTarget.SetX(theHeal->GetTarget()->mPhysicalModel.GetLocation().GetX());
        theTarget.SetY(theHeal->GetTarget()->mPhysicalModel.GetLocation().GetY());
    } else {
        inUnit->mGoalState->GetGoalLocation(theTarget);
    }
    theTarget.SetZ(theTransformation.GetZ());
    if ((inUnit->GetUnitClass()->mUnitType == kUnitTypeSp15CapitalShip || inUnit->GetUnitClass()->mUnitType == kUnitTypeCapitalShipYamato) && inUnit->mIsSpecialAttack)
        theAirTarget = false;
    float theTurn;
    float theYawRate;
    float theYaw;
    if (inUnit->GetModel()->GetNumParts() == 0 && !theAirTarget)
        this->TurnToward(inUnit, theLocation, theTarget, &theTurn, &theYawRate, &theYaw, true);
    else
        this->SpinDown(inUnit, &theYawRate, &theYaw);
    GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
    theAngularVelocity.mPitch = 0.0f;
    theAngularVelocity.mRoll = 0.0f;
    theAngularVelocity.mYaw = theYawRate;
    float theRoll = inUnit->mRoll;
    theTransformation.SetOrientationYPR(theYaw, 0.0f, theRoll);
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
        theTransformation.SetOrientationYPR(theYaw, 0.0f, inUnit->mRoll);
    inUnit->SetTransformation(theTransformation, true);
    this->ApplyBob(inUnit);
}

// FUNCTION: EXE 0x55e2db
// Moves the spaceship by the push it collected from collisions (capped at its top speed) when it can stand
// on the new tile, plays or stops the idle push animation, then clears the push
void EEPMSpaceship::Move(EEComplexUnit* inUnit)
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

// FUNCTION: EXE 0x55e718
// Turns toward the target point in physics order 5, else turns at a steady rate
void EEPMSpaceship::TurnToward(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float* outA, float* outB, float* outC, bool inUseFacingOffset)
{
    if (inUnit->mCurrentPhysicsOrder == 5)
        EEPhysicsModel::TurnToward(inUnit, inFrom, inTo, outA, outB, outC, inUseFacingOffset);
    else
        this->SteadyTurn(inUnit, outA, outB, outC);
}

// FUNCTION: EXE 0x55e754
// Collides the spaceship as the base physics model does
void EEPMSpaceship::Collide(EEComplexUnit* inUnit)
{
    EEPhysicsModel::Collide(inUnit);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55e760
// Resolves a moving spaceship running into another: pushes the smaller one aside, or stops, slows or
// backs off this one (sending a stuck one idle); returns true when it gave up its goals
bool EEPMSpaceship::PushMoving(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    GEVector theOtherDirection;
    GE3DLine theLine;
    GE3DLine theOtherLine;
    GE3DLine theCrossLine;
    GE3DPoint theLocation;
    GE3DPoint theOtherLocation;
    bool theResult = false;
    float theSpeed = inUnit->mCollisionRadius;
    float theOtherSpeed = inOther->mCollisionRadius;
    float theSlow = 0.75f;
    float theT = 0.0f;
    float theOtherT = 0.0f;
    float theSign = 1.0f;
    float theOtherSign = 1.0f;
    float theCrossT;
    float theCrossOtherT;
    const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
    const GETransformation& theOtherTransformation = inOther->mPhysicalModel.GetLocation();
    theTransformation.Apply(theForward, theDirection);
    theOtherTransformation.Apply(theForward, theOtherDirection);
    theDirection.Normalize();
    theOtherDirection.Normalize();
    float theAngle = UMArcCos(theDirection.DotProduct(theOtherDirection));
    if (theAngle < 0.0f)
        theAngle = -theAngle;
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
    float theX = theOtherDirection.mX;
    theOtherDirection.mX = theOtherDirection.mY;
    theOtherDirection.mY = -theX;
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
            inOther->mAngularVelocity = GEOrientation(0.0f, 0.0f, 0.0f);
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
            inUnit->GetUnitClass()->ActionChanged(inUnit);
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
        inUnit->mCollisionResolutionCount += gPhysics->mTickTime;
    }
    return theResult;
}

// FUNCTION: EXE 0x55ee8a
// Levels the spaceship's pitch, keeping its heading, height and roll
void EEPMSpaceship::RemoveBob(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    theTransformation.Apply(theForward, theResult);
    float theYaw = UMArcTan2(-theResult.mX, theResult.mY);
    float theRoll = inUnit->mRoll;
    theTransformation.SetOrientationYPR(theYaw, 0.0f, theRoll);
    inUnit->SetTransformation(theTransformation, true);
}

void EEPMSpaceship::AfterStep(EEComplexUnit* inUnit)
{
    EEPhysicsModel::AfterStep(inUnit);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55ef1a
// Brakes the spaceship's speed and turn rate toward a stop, moving it by what is left (levelled once both
// are zero), and bobs it
void EEPMSpaceship::Step4(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GEOrientation theRate(0.0f, 0.0f, 0.0f);
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    float theYawRate;
    float theYaw;
    this->SpinDown(inUnit, &theYawRate, &theYaw);
    theRate.mYaw = theYawRate;
    inUnit->mAngularVelocity = theRate;
    GETransformation theNew(false);
    theNew.SetOrientationYPR(theYaw, 0.0f, inUnit->GetRoll());
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
            theNew.SetOrientationYPR(theYaw, 0.0f, inUnit->GetRoll());
        inUnit->SetTransformation(theNew, true);
    }
    this->ApplyBob(inUnit);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55f0fe
// Turns a stopped spaceship in place to the facing its location goal asks for and bobs it; a still-moving
// one brakes first (Step4); does nothing without a location goal
void EEPMSpaceship::Step5(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    EEUnitGoal* theGoalState = inUnit->mGoalState;
    EEUGLocation* theGoal = dynamic_cast<EEUGLocation*>(theGoalState);
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
        this->TurnTowardAngle(inUnit, theHeading, theGoal->mYaw, &theLeft, &theYawRate, &theYaw, false);
        GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
        theAngularVelocity.mPitch = 0.0f;
        theAngularVelocity.mRoll = 0.0f;
        theAngularVelocity.mYaw = theYawRate;
        float theRoll = inUnit->mRoll;
        theTransformation.SetOrientationYPR(theYaw, 0.0f, theRoll);
        inUnit->SetTransformation(theTransformation, true);
        this->ApplyBob(inUnit);
    }
}

// NEAR: register/slot allocation and x87 order
// Every call adds 0.33 tick to the collision-resolution time, blocked or not
// FUNCTION: EXE 0x55f21d
// Checks the spaceship's step onto a new tile (terrain, edge, diagonal corners; units never block), retrying
// without x, then y, else undoing it; returns whether the step stands
bool EEPMSpaceship::ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance)
{
    GE3DPoint theTarget;
    float theDistance = 0.0f;
    EEMap* theMap = ::gWorld.mMap;
    long theX = UMFloor(ioLocation.GetX());
    long theY = UMFloor(ioLocation.GetY());
    bool theTriedX = false;
    bool theTriedY = false;
    EETile* theTile = theMap->GetTile(theX, theY);
    bool theResult = theTile != NULL;
    if (theTile != NULL) {
        do {
            if (!theResult)
                break;
            if (theTile != inUnit->mTile) {
                if (inUnit->GetUnitClass()->CanMoveOnTerrain(inUnit, theTile->GetTerrainType()) && !theMap->IsEdgeTile(theX, theY)) {
                    if (inUnit->GetTileX() == theX || inUnit->GetTileY() == theY) {
                        theTile = NULL;
                        continue;
                    }
                    EETileCoordinate theSide1;
                    theSide1.mX = theX;
                    theSide1.mY = inUnit->GetTileY();
                    if (EECanStandOn(theSide1, inUnit)) {
                        theTile = NULL;
                        continue;
                    }
                    EETileCoordinate theSide2;
                    theSide2.mX = inUnit->GetTileX();
                    theSide2.mY = theY;
                    if (EECanStandOn(theSide2, inUnit)) {
                        theTile = NULL;
                        continue;
                    }
                }
                theResult = false;
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
    // only a step that had to be retried counts towards collision resolution (as on land)
    if (theTriedX || theTriedY)
        inUnit->mCollisionResolutionCount += gPhysics->mTickTime * 0.33f;
    return theResult;
}
