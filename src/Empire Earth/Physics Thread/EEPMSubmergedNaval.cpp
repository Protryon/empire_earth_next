// C:\EEX\Empire Earth\Physics Thread\EEPMSubmergedNaval.cpp (file name inferred: no __FILE__; RTTI EEPMSubmergedNaval;
// one unit per physics-model class, alphabetical link order). Range 0x15f49d..0x16066d; split out of a single
// EEPMLand.cpp (each unit ends with its own inline-COMDAT cluster).
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
#include "ee/EEGraphicObject.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEEffectManager.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMathInline.h"

static const float kPMMinimumStep = 0.005f;   // 0x442968 (unit-local const, as EEPhysicsModel.cpp's; provisional name)
static const float kPMStopDistance = 0.02f;   // 0x442964 (unit-local const, as EEPhysicsModel.cpp's; provisional name)
static const float kPMMinimumSpeed = 1.0f;   // 0x44296c (unit-local const, as EEPhysicsModel.cpp's; provisional name)

extern unsigned long gCollisionPeriod;      // 0x4c237c (EEPhysics.cpp; provisional name)

// FUNCTION: EXE 0x55f49d
// Sets the submarine's location to its dive depth for the spot (EEAdjustDiveDepth)
void EEPMSubmergedNaval::AdjustDiveDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation)
{
    EEAdjustDiveDepth(inUnit, ioLocation);
}

// FUNCTION: EXE 0x55f4af
// Steers the submarine toward its next waypoint: turns, accelerates, clips the step against blocked tiles
// and marks it arrived when close
void EEPMSubmergedNaval::Step0(EEComplexUnit* inUnit)
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
}

// FUNCTION: EXE 0x55f69c
// Turns the submarine toward the unit it attacks or heals (or its goal location) while braking to a stop;
// a submarine with model parts only spins its turn down
void EEPMSubmergedNaval::Step1(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), 0.0f);
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
    EEUGHealUnit* theHeal = dynamic_cast<EEUGHealUnit*>(inUnit->GetGoal());
    float theBrake = inUnit->GetAcceleration();
    float theSpeed = inUnit->mVelocity;
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
}

// FUNCTION: EXE 0x55f8e3
// Moves the submarine by the push it collected from collisions (capped at its top speed) when it can stand
// on the new tile, at its dive depth there, then clears the push
void EEPMSubmergedNaval::Move(EEComplexUnit* inUnit)
{
    EETileCoordinate theTile(0, 0);
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
            inUnit->SetLocationXY(theX, theY, true);
            if (inUnit->mCurrentPhysicsAction == 5 && inUnit->mCollisionRadius >= kPMMinimumSpeed) {
                inUnit->mAngularVelocity = GEOrientation(0.0f, 0.0f, 0.0f);
                EEPhysicsTrace("Collision: Turning Unit %08x angular velocity set to %f\n", inUnit->GetUnitID(), GEOrientation(0.0f, 0.0f, 0.0f));
            }
            inUnit->mCollisionResolutionCount += gPhysics->GetTickTime();
            GE3DPoint theLocation = inUnit->mPhysicalModel.GetLocation().GetPosition();
            this->AdjustDiveDepth(inUnit, theLocation);
            inUnit->SetLocation(theLocation, true);
        }
    }
    if (inUnit->mPushVelocityX != 0.0f)
        inUnit->mPushVelocityX = 0.0f;
    if (inUnit->mPushVelocityY != 0.0f)
        inUnit->mPushVelocityY = 0.0f;
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55fbe6
// Sinks the submarine: tips its bow toward 18 degrees up or down, rolls it five times as far and lowers it
// faster the steeper it tips
void EEPMSubmergedNaval::Step2(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GEOrientation theRate(inUnit->mAngularVelocity);
    EEUnitClass* theClass = inUnit->mUnitClass;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    float theStep = gPhysics->mTickSeconds;
    theTransformation.Apply(theForward, theResult);
    float theYaw = UMArcTan2(-theResult.mX, theResult.mY);
    float thePitch = UMArcCos(-theResult.mZ) - 1.5707964f;
    float theRatePitch;
    float theNewPitch;
    if (theYaw >= -0.7853982f && theYaw <= 2.3561945f && thePitch < 0.31415927f) {
        this->ApproachPitch(inUnit, thePitch, 0.31415927f, &theRatePitch, &theNewPitch);
        theRate.SetPitch(theRatePitch);
        thePitch = theNewPitch;
    } else if (thePitch > -0.31415927f) {
        this->ApproachPitch(inUnit, thePitch, -0.31415927f, &theRatePitch, &theNewPitch);
        theRate.SetPitch(theRatePitch);
        thePitch = theNewPitch;
    }
    float theRoll;
    if (theYaw >= -2.3561945f && theYaw <= 0.7853982f)
        theRoll = (thePitch < 0.0f ? -thePitch : thePitch) * -5.0f;
    else
        theRoll = (thePitch < 0.0f ? -thePitch : thePitch) * 5.0f;
    theTransformation.SetOrientationYPR(theYaw, thePitch, theRoll);
    inUnit->mAngularVelocity = theRate;
    float theSink = thePitch < 0.0f ? -thePitch : thePitch;
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(),
                                  theLocation.GetZ() - (theSink * 0.31830987f) * theClass->mMaxVelocity * theStep);
    inUnit->SetTransformation(theTransformation, true);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55fdde
// Brakes the submarine's speed and turn rate toward a stop, moving it by what is left; does nothing more
// once both are zero
void EEPMSubmergedNaval::Step4(EEComplexUnit* inUnit)
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
    float theSpeed = inUnit->mVelocity;
    theSpeed = theSpeed - inUnit->GetAcceleration() * theStep;
    if (theSpeed < 0.0f)
        theSpeed = 0.0f;
    if (inUnit->GetMaxVelocity() < theSpeed)
        theSpeed = inUnit->GetMaxVelocity();
    inUnit->SetVelocity(theSpeed);
    if (theYawRate == 0.0f && theSpeed == 0.0f)
        return;
    theForward.mY = theSpeed * theStep;
    theNew.Apply(theForward, theMove);
    theLocation += theMove;
    bool theMoved = this->ClipStep(inUnit, theMove, theLocation, &theYaw);
    theNew.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    if (!theMoved)
        theNew.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    inUnit->SetTransformation(theNew, true);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55ffb9
// Turns a stopped submarine in place to the facing its location goal asks for; a still-moving one brakes
// first (Step4); does nothing without a location goal
void EEPMSubmergedNaval::Step5(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    EEUnitGoal* theGoalState = inUnit->mGoalState;
    EEUGLocation* theGoal = dynamic_cast<EEUGLocation*>(theGoalState);
    if (!theGoal)
        return;
    if (inUnit->mVelocity != 0.0f) {
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
        inUnit->SetTransformation(theTransformation, true);
    }
}

// FUNCTION: EXE 0x5600cc
// Resolves a moving submarine running into another as the base physics model does
bool EEPMSubmergedNaval::PushMoving(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    return EEPhysicsModel::PushMoving(inUnit, inOther);
}

// FUNCTION: EXE 0x5600dc
// Pushes another unit as the base physics model does; returns whether it handled the contact
bool EEPMSubmergedNaval::Push(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    return EEPhysicsModel::Push(inUnit, inOther);
}

void EEPMSubmergedNaval::AfterStep(EEComplexUnit* inUnit)
{
    EEPhysicsModel::AfterStep(inUnit);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x5600ec
// Collides the submarine with living submarines and sea animals on the tiles it covers, stopping at the
// first contact that is handled
void EEPMSubmergedNaval::Collide(EEComplexUnit* inUnit)
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
                EEComplexUnit* theOther = theUnit->mComplexUnit;
                if (theUnit == inUnit || !theOther
                    || (theUnit->mUnitClass->mCategory != kUnitCategorySubmergedNaval && theUnit->mUnitClass->mCategory != kUnitCategorySeaAnimal)
                    || !theOther->mIsCollidable || !(theOther->mHitPoints > 0))
                    continue;
                bool theHit;
                if (theOther->mUnitClass->mInnerCollisionSphere >= kPMMinimumSpeed && theRadius >= kPMMinimumSpeed)
                    theHit = this->PushMoving(inUnit, theOther);
                else
                    theHit = this->Push(inUnit, theOther);
                if (!theHit)
                    continue;
                i = theMaxX + 1;
                j = theMaxY + 1;
                break;
            }
        }
    }
}

// NEAR: register/slot allocation and x87 order
// Unless a blocker on the waypoint's tile stops it, every call adds a tick to the collision-resolution
// time, retry or not (original: bytes 0x560648 test only that flag)
// FUNCTION: EXE 0x5602b8
// Checks the submarine's step onto a new tile (terrain, edge, shallows, blocking units, diagonal corners),
// retrying without x, then y, else undoing it; keeps its depth (its dive depth in physics action 0); returns the result
bool EEPMSubmergedNaval::ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance)
{
    GE3DPoint theTarget;
    EEUnit* theBlocker = NULL;
    long theX = UMFloor(ioLocation.GetX());
    bool theTriedX = false;
    long theY = UMFloor(ioLocation.GetY());
    EEMap* theMap = ::gWorld.mMap;
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
                        while (theNode) {
                            theBlocker = theNode->mUnit;
                            theResult = theBlocker->IsPassableBy(inUnit);
                            theNode = theNode->mNext;
                            if (!theResult)
                                break;
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
        theResult = EEPhysicsModel::ClipStep(inUnit, inMove, ioLocation, outDistance);
    ioLocation.mZ = inUnit->mPhysicalModel.GetLocation().GetZ();
    if (inUnit->mCurrentPhysicsAction == 0)
        this->AdjustDiveDepth(inUnit, ioLocation);
    if (theSlowed && (theTriedX || theTriedY))
        inUnit->mCollisionResolutionCount += gPhysics->GetTickTime();
    return theResult;
}
