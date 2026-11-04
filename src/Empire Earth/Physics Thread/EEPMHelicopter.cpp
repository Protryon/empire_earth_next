// C:\EEX\Empire Earth\Physics Thread\EEPMHelicopter.cpp (file name inferred: no __FILE__; RTTI EEPMHelicopter; one unit per
// physics-model class, alphabetical link order). Range 0x158a24..0x15a0cf; split out of a single EEPMLand.cpp
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
#include "ee/EEUGLocation.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEUGCastCalamity.h"
#include "ee/EEEffectManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEPhysicsModel.h"

static const float kPMMinimumStep = 0.005f;   // 0x4428d4 (unit-local const, as EEPhysicsModel.cpp's; provisional name)
static const float kPMStopDistance = 0.02f;   // 0x4428d0 (unit-local const, as EEPhysicsModel.cpp's; provisional name)
static const float kPMMinimumSpeed = 1.0f;   // 0x4428d8 (unit-local const, as EEPhysicsModel.cpp's; provisional name)

extern unsigned long gCollisionPeriod;      // 0x4c237c (EEPhysics.cpp; provisional name)

// NEAR: store order of mU17C/mSound1Frame11
// FUNCTION: EXE 0x558a24
// Flies the helicopter toward its next waypoint at its height (hovering without one), climbing,
// turning and accelerating, with the hover bob taken out and put back
void EEPMHelicopter::Step0(EEComplexUnit* inUnit)
{
    this->RemoveBob(inUnit);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    float theDistance = inUnit->mHeight;
    theTransformation.SetOrientationYPR(inUnit->mOrientation.GetYaw(), inUnit->mOrientation.GetPitch(), inUnit->mOrientation.GetRoll());
    float theClimb;
    if ((long) inUnit->mWaypoints.size() > 0) {
        inUnit->GetNextWaypointPoint(theTarget);
        theTarget.SetZ(theDistance);
        theDistance = theLocation.DistanceTo(theTarget);
        if (theDistance >= 0.05f) {
            theClimb = UMArcCos(-((theTarget.GetZ() - theLocation.GetZ()) / theDistance)) - 1.5707964f;
        } else {
            theDistance = 0.0f;
            theLocation = theTarget;
            theClimb = 0.0f;
        }
    } else {
        theDistance = 0.0f;
        theTarget = theLocation;
        theClimb = 0.0f;
    }
    float theTurn;
    float theYawRate;
    float theYaw;
    this->TurnToward(inUnit, theLocation, theTarget, &theTurn, &theYawRate, &theYaw, false);
    float thePitchRate;
    float thePitch;
    this->ApproachPitch(inUnit, inUnit->mOrientation.mPitch, theClimb, &thePitchRate, &thePitch);
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
    this->ClipStep(inUnit, theMove, theLocation, &theYaw);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    float theBank = this->ComputePitch(inUnit, thePitch);
    theTransformation.SetOrientationYPR(theYaw, theBank, this->ComputeRoll(inUnit, theYawRate));
    bool theFlag = inUnit->mUnitClass->HasArrived(inUnit, theDistance);
    if (inUnit->mHasArrived != theFlag)
        inUnit->mHasArrived = theFlag;
    inUnit->SetTransformation(theTransformation, true);
    this->ApplyBob(inUnit);
}

// NEAR: x87 scheduling (the sin inline constant, see Virtual48)
// FUNCTION: EXE 0x558ce3
// Advances the hover phase (kept in -pi..pi) and lowers the helicopter by the new bob
// (the phase is +0x220; RemoveBob adds the bob back)
void EEPMHelicopter::ApplyBob(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint thePosition = theTransformation.GetPosition();
    float theAngle = gPhysics->mTickTime * 0.0015707965f + inUnit->GetF220();
    while (theAngle > 3.1415927f)
        theAngle -= 6.2831855f;
    inUnit->SetF220(theAngle);
    float theBob = (UMSin(theAngle) + 1.0f) * 0.5f;
    thePosition.SetZ(thePosition.GetZ() - inUnit->mUnitClass->GetBobHeight() * theBob);
    theTransformation.SetPosition(thePosition);
    inUnit->SetTransformation(theTransformation, true);
}

// NEAR: dyncast result slots, store order
// FUNCTION: EXE 0x558da4
// Turns the helicopter toward the unit it attacks or heals, its calamity target or goal location, and
// brakes to a stop, with the hover bob taken out and put back
void EEPMHelicopter::Step1(EEComplexUnit* inUnit)
{
    this->RemoveBob(inUnit);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    EEUnitGoal* theGoal = inUnit->mGoalState;
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(theGoal);
    EEUGCastCalamity* theCast = dynamic_cast<EEUGCastCalamity*>(theGoal);
    EEUGHealUnit* theHeal = dynamic_cast<EEUGHealUnit*>(theGoal);
    float theSpeed = inUnit->mVelocity;
    float theBrake = inUnit->GetAcceleration();
    float theStep = gPhysics->mTickSeconds;
    float theHeight = inUnit->mHeight;
    theTransformation.SetOrientationYPR(inUnit->mOrientation.GetYaw(), inUnit->mOrientation.GetPitch(), inUnit->mOrientation.GetRoll());
    if (theAttack) {
        if (theAttack->mTarget->mComplexUnit) {
            theTarget.SetX(theAttack->mTarget->mComplexUnit->mPhysicalModel.GetLocation().GetX());
            theTarget.SetY(theAttack->mTarget->mComplexUnit->mPhysicalModel.GetLocation().GetY());
        } else {
            theTarget.SetX((float) theAttack->mTarget->GetTileX() + 0.5f);
            theTarget.SetY((float) theAttack->mTarget->GetTileY() + 0.5f);
        }
    } else if (theCast) {
        theCast->GetTargetLocation(theTarget);
    } else if (theHeal) {
        theTarget.SetX(theHeal->GetTarget()->mPhysicalModel.GetLocation().GetX());
        theTarget.SetY(theHeal->GetTarget()->mPhysicalModel.GetLocation().GetY());
    } else {
        theGoal->GetGoalLocation(theTarget);
    }
    theTarget.SetZ(theHeight);
    float theDistance = theLocation.DistanceTo(theTarget);
    float theTurn;
    float theYawRate;
    float theYaw;
    this->TurnToward(inUnit, theLocation, theTarget, &theTurn, &theYawRate, &theYaw, false);
    float thePitchRate;
    float thePitch;
    this->ApproachPitch(inUnit, inUnit->mOrientation.mPitch, 0.0f, &thePitchRate, &thePitch);
    GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
    inUnit->mAngularVelocity.SetPitch(thePitchRate);
    theAngularVelocity.mRoll = 0.0f;
    theAngularVelocity.mYaw = theYawRate;
    inUnit->mOrientation = GEOrientation(theYaw, thePitch, 0.0f);
    theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
    theHeight = theSpeed - theStep * theBrake;
    if (theHeight < 0.0f)
        theHeight = 0.0f;
    inUnit->SetVelocity(theHeight);
    theForward.mY = theHeight * theStep;
    theTransformation.Apply(theForward, theMove);
    theLocation += theMove;
    this->ClipStep(inUnit, theMove, theLocation, &theYaw);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    float theBank = this->ComputePitch(inUnit, thePitch);
    theTransformation.SetOrientationYPR(theYaw, theBank, this->ComputeRoll(inUnit, theYawRate));
    bool theFlag = inUnit->mUnitClass->HasArrived(inUnit, theDistance);
    if (inUnit->mHasArrived != theFlag)
        inUnit->mHasArrived = theFlag;
    inUnit->SetTransformation(theTransformation, true);
    this->ApplyBob(inUnit);
}

// NEAR: x87 stack scheduling of the sign/abs/limit block
// FUNCTION: EXE 0x5590e8
// Applies the collision push to the helicopter, capped at its speed, onto a tile it can stand on, and
// clears the push
void EEPMHelicopter::Move(EEComplexUnit* inUnit)
{
    float theLimit = kPMMinimumStep / gPhysics->GetTickSeconds();
    theLimit = inUnit->GetVelocity() > theLimit ? inUnit->GetVelocity() : theLimit;
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
        float theX = gPhysics->GetTickSeconds() * theSignX * theDX + inUnit->mPhysicalModel.GetLocation().GetX();
        float theY = gPhysics->GetTickSeconds() * theSignY * theDY + inUnit->mPhysicalModel.GetLocation().GetY();
        long theTileY = UMFloor(theY);
        long theTileX = UMFloor(theX);
        EETileCoordinate theTile(theTileX, theTileY);
        if (EECanStandOn(theTile, inUnit)) {
            inUnit->SetLocationXY(theX, theY, true);
            if (inUnit->mCurrentPhysicsAction == 5 && inUnit->mUnitClass->mInnerCollisionSphere >= kPMMinimumSpeed) {
                inUnit->mAngularVelocity = GEOrientation(0.0f, 0.0f, 0.0f);
                EEPhysicsTrace("Collision: Turning Unit %08x angular velocity set to %f\n", inUnit->GetUnitID(), GEOrientation(0.0f, 0.0f, 0.0f));
            }
            inUnit->mCollisionResolutionCount += gPhysics->mTickTime;
        }
    }
    if (inUnit->mPushVelocityX != 0.0f)
        inUnit->mPushVelocityX = 0.0f;
    if (inUnit->mPushVelocityY != 0.0f)
        inUnit->mPushVelocityY = 0.0f;
}

// FUNCTION: EXE 0x559346
// Returns the displayed pitch scaled by the share of top speed, or 0 when the class does not pitch
float EEPMHelicopter::ComputePitch(EEComplexUnit* inUnit, float inValue)
{
    float theSpeed = inUnit->mVelocity;
    float theRatio = theSpeed / inUnit->GetMaxVelocity();
    return inUnit->mUnitClass->mPitchesWithTerrain ? theRatio * inValue : 0.0f;
}

// NEAR: class mMaxRoll read as an accessor value in the original (fld; fmulp)
// FUNCTION: EXE 0x55937a
// Returns the displayed bank for the turn rate up to the class's maximum roll, or 0 when it does not roll
float EEPMHelicopter::ComputeRoll(EEComplexUnit* inUnit, float inValue)
{
    if (inUnit->mUnitClass->mRollsWithTerrain) {
        float theRatio;
        if (inValue < 0.0f) {
            theRatio = inValue / inUnit->GetMaxAngularVelocity() + 0.100000024f;
            if (theRatio > 0.0f)
                theRatio = 0.0f;
        } else {
            theRatio = inValue / inUnit->GetMaxAngularVelocity() - 0.100000024f;
            if (theRatio < 0.0f)
                theRatio = 0.0f;
        }
        return (theRatio * 1.1111112f) * inUnit->mUnitClass->mMaxRoll;
    }
    return 0.0f;
}

// FUNCTION: EXE 0x5593ef
// Returns the speed mode: slowing (4) or holding (2) in a sharp turn it cannot make, slowing (4)
// within braking distance of the path's end, else cruising (3)
long EEPMHelicopter::SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn)
{
    float theBrake = inUnit->GetAcceleration();
    float theStep = gPhysics->mTickSeconds;
    float theTime = gPhysics->mTicksPerGameSecond;
    float theSpeed = inUnit->mVelocity;
    float theTicks = inUnit->GetInverseAcceleration() * theTime * theSpeed;
    float theStop = theStep * theStep * (theTicks + 1.0f) * theTicks * 0.5f * theBrake;
    if (theStop < 0.0001f)
        theStop = 0.0f;
    float theSlide = inDistance;
    theSlide *= 0.5f * inUnit->mAngularVelocity.GetYaw() * theStep;
    if (theSlide < 0.0f)
        theSlide = -theSlide;
    long theResult = 3;
    if (inTurn < -0.7853982f || inTurn > 0.7853982f) {
        if (theSlide <= theSpeed * theStep)
            theResult = 4;
        else if (theSlide <= (theStep * theBrake + theSpeed) * theStep)
            theResult = 2;
    }
    float theRemaining;
    if ((long)inUnit->mWaypoints.size() > 1)
        inUnit->GetPathLength(theRemaining);
    else
        theRemaining = inDistance;
    if (theRemaining <= theStop)
        theResult = 4;
    return theResult;
}

// NEAR: out-of-line EEMap::GetTile (0x402f85) and imported SetPosition calls in the original: inline budget
// FUNCTION: EXE 0x559507
// Crashes the shot-down helicopter: spins and pitches it down, splashes on reaching water and stops
// it with its death effect on hitting the ground; a landed one sinks
void EEPMHelicopter::Step2(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GE3DPoint theNew;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    float theStep = gPhysics->mTickSeconds;
    float theMaxRate = inUnit->mStats->mMaxAngularVelocity;
    float theAccel = inUnit->mStats->mAngularAcceleration;
    float theBrake = inUnit->GetAcceleration();
    float theSpeed = inUnit->mVelocity;
    float theYawRate = inUnit->mAngularVelocity.mYaw;
    float thePitchRate = inUnit->mAngularVelocity.mPitch;
    bool theLanded = theLocation.GetZ() <= 0.0f;
    long theY = UMFloor(theLocation.GetY());
    long theX = UMFloor(theLocation.GetX());
    EETile* theTile = ::gWorld.mMap->GetTile(theX, theY);
    if (theLanded) {
        theTransformation.SetOrientationYPR(0.0f, 0.0f, 0.0f);
        theNew.SetX(theLocation.GetX());
        theNew.SetY(theLocation.GetY());
        theNew.SetZ(theLocation.GetZ() - theStep * 0.5f);
        theTransformation.SetPosition(theNew.GetX(), theNew.GetY(), theNew.GetZ());
    } else {
        float theTurn = theAccel * 7.5f;
        theMaxRate = theMaxRate * 7.5f;
        // the pitch rate before the yaw rate: the original (pitch after yaw) keeps theTurn * theStep on the x87
        // stack for both (0x159655/0x15968e, fsub st(1) at 0x1596b0) because it calls the imported
        // GEOrientation::operator= below; ours inlines it, and in source order spilled the product to a float slot
        // for the pitch, 1 ulp off (Look to the Stars PT 4404). This order keeps it unrounded for both rates.
        thePitchRate = thePitchRate - theTurn * theStep;
        if (thePitchRate < -1.5707964f)
            thePitchRate = -1.5707964f;
        if (!inUnit->mUnitClass->mRollsWithTerrain || !inUnit->mUnitClass->mPitchesWithTerrain) {
            theYawRate = theYawRate - theTurn * theStep;
            if (theYawRate < 0.0f)
                theYawRate = 0.0f;
        } else if (theYawRate >= 0.0f) {
            theYawRate = theTurn * theStep + theYawRate;
            if (theMaxRate < theYawRate)
                theYawRate = theMaxRate;
        } else {
            float theLeft = theYawRate - theTurn * theStep;
            theYawRate = -theMaxRate;
            if (theYawRate <= theLeft)
                theYawRate = theLeft;
        }
        inUnit->mAngularVelocity = GEOrientation(theYawRate, thePitchRate, 0.0f);
        theTransformation.Revolve(theYawRate * theStep, 0.0f, 0.0f);
        theTransformation.Apply(theForward, theResult);
        // the heading (yaw from the revolved forward, the pitch rate) is a separate matrix with no translation: the
        // original orients [ebp-0xe0] (0x55974c) and moves along it (0x5597de), the unit keeps the revolved one
        GETransformation theHeading(false);
        theHeading.SetOrientationYPR(UMArcTan2(-theResult.mX, theResult.mY), thePitchRate, 0.0f);
        theHeading.mMatrix[0][3] = 0.0f;
        theHeading.mMatrix[1][3] = 0.0f;
        theHeading.mMatrix[2][3] = 0.0f;
        theHeading.mMatrix[3][0] = 0.0f;
        theHeading.mMatrix[3][1] = 0.0f;
        theHeading.mMatrix[3][2] = 0.0f;
        theHeading.mMatrix[3][3] = 1.0f;
        theSpeed = theBrake * theStep + theSpeed;
        if (inUnit->GetMaxVelocity() < theSpeed)
            theSpeed = inUnit->GetMaxVelocity();
        inUnit->SetVelocity(theSpeed);
        theForward.mX = 0.0f;
        theForward.mY = theSpeed * theStep;
        theForward.mZ = 0.0f;
        theHeading.Apply(theForward, theResult);
        theNew = theLocation;
        theNew += theResult;
        long theNewY = UMFloor(theNew.GetY());
        long theNewX = UMFloor(theNew.GetX());
        EETile* theNewTile = ::gWorld.mMap->GetTile(theNewX, theNewY);
        if (theNewTile == NULL) {
            theNew.SetY(theLocation.GetY());
            theNew.SetX(theLocation.GetX());
            theTransformation.SetPosition(theNew.GetX(), theNew.GetY(), theNew.GetZ());
        } else {
            theTile = theNewTile;
            theTransformation.SetPosition(theNew.GetX(), theNew.GetY(), theNew.GetZ());
        }
        if (theNew.GetZ() <= 0.0f) {
            inUnit->SetAnimation(5, -1, 0);
            if (theTile->GetHeight() < 0.0f && inUnit->mUnitClass->mSplashEffect) {
                GE3DPoint theSplash(theNew.GetX(), theNew.GetY(), 0.0f);
                gEffectManager->CreatePlacedEffect(theSplash, inUnit->mUnitClass->mSplashEffect, NULL);
            }
        }
    }
    if (theNew.GetZ() < theTile->GetHeight()) {
        inUnit->SetVelocity(0.0f);
        if (theTile->GetHeight() >= 0.0f && inUnit->mUnitClass->mDeathEffect)
            gEffectManager->CreateUnitEffect(inUnit, inUnit->mUnitClass->mDeathEffect, NULL, 0, 0, 0);
    }
    inUnit->SetTransformation(theTransformation, true);
}

// NEAR: the original sin inline subtracts from 1.5707964f (0x3fc90fdb; UMSin has ...963f) and keeps the bob on the x87 stack
// FUNCTION: EXE 0x5599ae
// Raises the helicopter by its class's bob height times (sin(phase) + 1) / 2, taking out the hover bob
// (bob height at class +0x190, phase at +0x220)
void EEPMHelicopter::RemoveBob(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint thePosition = theTransformation.GetPosition();
    float theBob = (UMSin(inUnit->mF220) + 1.0f) * 0.5f;
    thePosition.SetZ(inUnit->mUnitClass->GetBobHeight() * theBob + thePosition.GetZ());
    theTransformation.SetPosition(thePosition);
    inUnit->SetTransformation(theTransformation, true);
}

// NEAR: radius locals/frame layout
// FUNCTION: EXE 0x559a34
// Pushes the helicopter off the first living collidable flying unit in the tiles it covers
void EEPMHelicopter::Collide(EEComplexUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    float theRadius = inUnit->mUnitClass->mInnerCollisionSphere;
    GE2DPoint theCenter(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY());
    float theSize = theRadius;
    if (theRadius >= kPMMinimumSpeed)
        theSize = theRadius + theRadius;
    float theReach = theSize * 0.5;
    if (theReach < 0.5)
        theReach = 0.5f;
    long theMinX = UMFloor(theCenter.GetX() - theReach);
    long theMaxX = UMFloor(theCenter.GetX() + theReach);
    long theMinY = UMFloor(theCenter.GetY() - theReach);
    long theMaxY = UMFloor(theCenter.GetY() + theReach);
    for (long i = theMinX; i <= theMaxX; i++) {
        for (long j = theMinY; j <= theMaxY; j++) {
            EETile* theTile = theMap->GetTile(i, j);
            if (!theTile)
                continue;
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                EEUnit* theUnit = theNode->mUnit;
                if (theUnit == inUnit || !theUnit->mComplexUnit || theUnit->mUnitClass->FliesOver() != true
                    || !theUnit->mComplexUnit->mIsCollidable || !(theUnit->mComplexUnit->mHitPoints > 0)
                    || !this->Push(inUnit, theUnit->mComplexUnit))
                    continue;
                i = theMaxX + 1;
                j = theMaxY + 1;
                break;
            }
        }
    }
}

// NEAR: frame layout of the block locals
// FUNCTION: EXE 0x559bc6
// Slows the helicopter's speed, turn and pitch to a hover (only levels it once stopped), with the
// hover bob taken out and put back
void EEPMHelicopter::Step4(EEComplexUnit* inUnit)
{
    this->RemoveBob(inUnit);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    if (inUnit->mVelocity == 0.0f && inUnit->mAngularVelocity.mYaw == 0.0f) {
        GEVector theForward(0.0f, 1.0f, 0.0f);
        GEVector theResult;
        theTransformation.Apply(theForward, theResult);
        theTransformation.SetOrientationYPR(UMArcTan2(-theResult.mX, theResult.mY), 0.0f, 0.0f);
    } else {
        GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
        GEVector theForward(0.0f, 1.0f, 0.0f);
        GEVector theMove;
        float theBrake = inUnit->GetAcceleration();
        float theSpeed = inUnit->mVelocity;
        float theStep = gPhysics->mTickSeconds;
        float theYawRate;
        float theYaw;
        this->SpinDown(inUnit, &theYawRate, &theYaw);
        float thePitchRate;
        float thePitch;
        this->ApproachPitch(inUnit, inUnit->mOrientation.mPitch, 0.0f, &thePitchRate, &thePitch);
        GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
        inUnit->mAngularVelocity.SetPitch(thePitchRate);
        theAngularVelocity.mRoll = 0.0f;
        theAngularVelocity.mYaw = theYawRate;
        theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
        theSpeed = theSpeed - theStep * theBrake;
        if (theSpeed < 0.0f)
            theSpeed = 0.0f;
        if (inUnit->GetMaxVelocity() < theSpeed)
            theSpeed = inUnit->GetMaxVelocity();
        inUnit->SetVelocity(theSpeed);
        theForward.mY = theSpeed * theStep;
        theTransformation.Apply(theForward, theMove);
        theLocation += theMove;
        this->ClipStep(inUnit, theMove, theLocation, &theYaw);
        theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
        float theBank = this->ComputePitch(inUnit, thePitch);
        theTransformation.SetOrientationYPR(theYaw, theBank, this->ComputeRoll(inUnit, theYawRate));
    }
    inUnit->SetTransformation(theTransformation, true);
    this->ApplyBob(inUnit);
}

// NEAR: slot allocation
// FUNCTION: EXE 0x559e4d
// Turns a stopped helicopter to its goal location's heading (slows it first while moving)
void EEPMHelicopter::Step5(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(false);
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    EEUGLocation* theGoal = dynamic_cast<EEUGLocation*>(inUnit->GetGoal());
    if (!theGoal)
        return;
    if (inUnit->mVelocity != 0.0f) {
        this->Step4(inUnit);
    } else {
        this->RemoveBob(inUnit);
        theTransformation = inUnit->mPhysicalModel.GetLocation();
        theTransformation.SetOrientationYPR(inUnit->mOrientation.GetYaw(), inUnit->mOrientation.GetPitch(), inUnit->mOrientation.GetRoll());
        float thePitchRate;
        float thePitch;
        this->ApproachPitch(inUnit, inUnit->mOrientation.mPitch, 0.0f, &thePitchRate, &thePitch);
        theTransformation.Apply(theForward, theResult);
        float theLeft;
        float theYawRate;
        float theYaw;
        this->TurnTowardAngle(inUnit, UMArcTan2(-theResult.mX, theResult.mY), theGoal->mYaw, &theLeft, &theYawRate, &theYaw, false);
        GEOrientation& theAngularVelocity = inUnit->mAngularVelocity;
        inUnit->mAngularVelocity.SetPitch(thePitchRate);
        theAngularVelocity.mRoll = 0.0f;
        theAngularVelocity.mYaw = theYawRate;
        float theBank = this->ComputePitch(inUnit, thePitch);
        float theRoll = this->ComputeRoll(inUnit, theYawRate);
        inUnit->mOrientation = GEOrientation(theYaw, thePitch, 0.0f);
        theTransformation.SetOrientationYPR(theYaw, theBank, theRoll);
        inUnit->SetTransformation(theTransformation, true);
        this->ApplyBob(inUnit);
    }
}

// NEAR: floor result slots (x in edi, y in memory)
// FUNCTION: EXE 0x559ffa
// Undoes a move onto a tile it cannot enter or a map edge tile (returns false), else clips the move to the
// map edges
bool EEPMHelicopter::ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance)
{
    EEMap* theMap = ::gWorld.mMap;
    long theX = UMFloor(ioLocation.GetX());
    long theY = UMFloor(ioLocation.GetY());
    float theDistance = 0.0f;
    EETile* theTile = theMap->GetTile(theX, theY);
    if (theTile && theTile != inUnit->mTile
        && (!inUnit->mUnitClass->CanMoveOnTerrain(inUnit, theTile->mTerrainType) || theMap->IsEdgeTile(theX, theY))) {
        // undo the move, as the airplane version does
        ioLocation.SetX(ioLocation.GetX() - inMove.GetX());
        ioLocation.SetY(ioLocation.GetY() - inMove.GetY());
        return false;
    }
    return EEPhysicsModel::ClipStep(inUnit, inMove, ioLocation, &theDistance);
}

void EEPMHelicopter::AfterStep(EEComplexUnit* inUnit)
{
    EEPhysicsModel::AfterStep(inUnit);
}
