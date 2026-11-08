// C:\EEX\Empire Earth\Physics Thread\EEPMLand.cpp (file name inferred: no __FILE__; RTTI EEPMLand; one unit per
// physics-model class, alphabetical link order). Range 0x15a0db..0x15aff0; split out of a single EEPMLand.cpp
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
#include "ee/EEUGCastCalamity.h"
#include "ee/EEGraphicObject.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEPhysicsModel.h"

// 0x4428e8: a unit-local 1.0f (loaded from memory, never folded; Step1 and Step4 multiply by it) (provisional name)
static const float kPMLandBrakeFactor = 1.0f;

// FUNCTION: EXE 0x55a0db
// Drives the land unit toward its next waypoint: turns, accelerates, clips the step against blocked tiles
// and sets it on the ground (tilted with the terrain if its class does); marks it arrived when close
void EEPMLand::Step0(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), 0.0f);
    EEMap* theMap = ::gWorld.mMap;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    GE3DPoint theGround;
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    float thePitch = 0.0f;
    float theRoll = 0.0f;
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
    GETransformation theNew(false);
    theNew.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    theNew.SetPosition(0.0f, 0.0f, 0.0f);
    theNew.SetElement(3, 0, 0.0f);
    theNew.SetElement(3, 1, 0.0f);
    theNew.SetElement(3, 2, 0.0f);
    theNew.SetElement(3, 3, 1.0f);
    float theSpeed = this->Accelerate(inUnit, theDistance, theTurn);
    inUnit->SetVelocity(theSpeed);
    theForward.mY = theSpeed * theStep;
    theNew.Apply(theForward, theMove);
    theLocation += theMove;
    bool theMoved = this->ClipStep(inUnit, theMove, theLocation, &theYaw);
    theMap->GetGroundPoint(theLocation, theGround);
    if (inUnit->IsSwimming())
        theGround.SetZ(inUnit->GetUnitClass()->GetCruiseAltitude());
    if (!theClass->mPitchesWithTerrain && !theClass->mRollsWithTerrain) {
        if (theMoved)
            theTransformation = theNew;
        else
            theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    } else {
        theNew.SetPosition(theGround.GetX(), theGround.GetY(), theGround.GetZ());
        if (theClass->mPitchesWithTerrain)
            thePitch = theMap->GetTerrainPitch(theNew);
        if (theClass->mRollsWithTerrain)
            theRoll = theMap->GetTerrainRoll(theNew);
        theTransformation.SetOrientationYPR(theYaw, thePitch, theRoll);
    }
    theTransformation.SetPosition(theGround.GetX(), theGround.GetY(), theGround.GetZ());
    bool theFlag = theClass->HasArrived(inUnit, theDistance);
    if (inUnit->mHasArrived != theFlag)
        inUnit->mHasArrived = theFlag;
    inUnit->SetTransformation(theTransformation, true);
}

// FUNCTION: EXE 0x55a3c6
// Turns the land unit toward the unit it attacks or heals (or its goal location) while braking to a stop,
// and keeps it on the ground; a unit with model parts only spins its turn down
void EEPMLand::Step1(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), 0.0f);
    EEUnitGoal* theGoal = inUnit->mGoalState;
    EEMap* theMap = ::gWorld.mMap;
    GE3DPoint theGround;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    GE3DPoint theTarget;
    GEVector theForward;
    GEVector theMove;
    EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(theGoal);
    EEUGCastCalamity* theCast = dynamic_cast<EEUGCastCalamity*>(theGoal);
    EEUGHealUnit* theHeal = dynamic_cast<EEUGHealUnit*>(theGoal);
    float theBrake = inUnit->GetAcceleration();
    float theSpeed = inUnit->GetVelocity();
    float thePitch = 0.0f;
    float theRoll = 0.0f;
    float theStep = gPhysics->mTickSeconds;
    if (theAttack) {
        if (theAttack->mTarget->mComplexUnit) {
            theTarget.SetX(theAttack->mTarget->mComplexUnit->mPhysicalModel.GetLocation().GetX());
            theTarget.SetY(theAttack->mTarget->mComplexUnit->mPhysicalModel.GetLocation().GetY());
        } else {
            theTarget.SetX((float) theAttack->mTarget->GetTileX() + 0.5f);
            theTarget.SetY((float) theAttack->mTarget->GetTileY() + 0.5f);
        }
    } else if (theCast) {
        theCast->GetGoalLocation(theTarget);
    } else if (theHeal) {
        theTarget.SetX(theHeal->GetTarget()->mPhysicalModel.GetLocation().GetX());
        theTarget.SetY(theHeal->GetTarget()->mPhysicalModel.GetLocation().GetY());
    } else {
        theGoal->GetGoalLocation(theTarget);
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
    GETransformation theNew(false);
    theNew.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    theNew.SetPosition(0.0f, 0.0f, 0.0f);
    theNew.SetElement(3, 0, 0.0f);
    theNew.SetElement(3, 1, 0.0f);
    theNew.SetElement(3, 2, 0.0f);
    theNew.SetElement(3, 3, 1.0f);
    float theNewSpeed = theSpeed - kPMLandBrakeFactor * theStep * theBrake;
    if (theNewSpeed < 0.0f)
        theNewSpeed = 0.0f;
    inUnit->SetVelocity(theNewSpeed);
    theForward.mY = theNewSpeed * theStep;
    theNew.Apply(theForward, theMove);
    theLocation += theMove;
    bool theMoved = this->ClipStep(inUnit, theMove, theLocation, &theYaw);
    theMap->GetGroundPoint(theLocation, theGround);
    if (inUnit->IsSwimming())
        theGround.SetZ(inUnit->GetUnitClass()->GetCruiseAltitude());
    if (!theClass->mPitchesWithTerrain && !theClass->mRollsWithTerrain) {
        if (theMoved)
            theTransformation = theNew;
        else
            theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    } else {
        theNew.SetPosition(theGround.GetX(), theGround.GetY(), theGround.GetZ());
        if (theClass->mPitchesWithTerrain)
            thePitch = theMap->GetTerrainPitch(theNew);
        if (theClass->mRollsWithTerrain)
            theRoll = theMap->GetTerrainRoll(theNew);
        theTransformation.SetOrientationYPR(theYaw, thePitch, theRoll);
    }
    theTransformation.SetPosition(theGround.GetX(), theGround.GetY(), theGround.GetZ());
    inUnit->SetTransformation(theTransformation, true);
}

// FUNCTION: EXE 0x55a75b
// Stops the land unit: zero speed and zero turn rate
void EEPMLand::Step2(EEComplexUnit* inUnit)
{
    inUnit->SetVelocity(0.0f);
    inUnit->mAngularVelocity = GEOrientation(0.0f, 0.0f, 0.0f);
}

// FUNCTION: EXE 0x55a785
// Moves the land unit as the base physics model does
void EEPMLand::Move(EEComplexUnit* inUnit)
{
    EEPhysicsModel::Move(inUnit);
}

// Collides the land unit as the base physics model does
// ICF copies of 0x55e754 / 0x55be0e (identical bodies folded by LINK; no own address)
void EEPMLand::Collide(EEComplexUnit* inUnit)
{
    EEPhysicsModel::Collide(inUnit);
}

// Runs the base physics model's after-step work for the land unit
// ICF copy: LINK kept EEPMNaval's identical body at 0x55be0e (EEPMNaval.cpp)
void EEPMLand::AfterStep(EEComplexUnit* inUnit)
{
    EEPhysicsModel::AfterStep(inUnit);
}

// NEAR: register/slot allocation and x87 order
// FUNCTION: EXE 0x55a791
// Brakes the land unit's speed and turn rate toward a stop, still moving it by what is left and keeping
// it on the ground; does nothing more once both are zero
void EEPMLand::Step4(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), 0.0f);
    EEMap* theMap = ::gWorld.mMap;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    GE3DPoint theGround;
    GEOrientation theRate(0.0f, 0.0f, 0.0f);
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    float thePitch = 0.0f;
    float theRoll = 0.0f;
    float theYawRate = inUnit->mAngularVelocity.GetYaw();
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
    float theSpeed;
    if (inUnit->GetVelocity() > 0.0f) {
        theSpeed = inUnit->GetVelocity();
        theSpeed = theSpeed - inUnit->GetAcceleration() * kPMLandBrakeFactor * theStep;
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
    theNew.Apply(theForward, theMove);
    theLocation += theMove;
    bool theMoved = this->ClipStep(inUnit, theMove, theLocation, &theYaw);
    theMap->GetGroundPoint(theLocation, theGround);
    if (inUnit->IsSwimming())
        theGround.SetZ(inUnit->GetUnitClass()->GetCruiseAltitude());
    if (!theClass->mPitchesWithTerrain && !theClass->mRollsWithTerrain) {
        if (theMoved)
            theTransformation = theNew;
        else
            theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    } else {
        theNew.SetPosition(theGround.GetX(), theGround.GetY(), theGround.GetZ());
        if (theClass->mPitchesWithTerrain)
            thePitch = theMap->GetTerrainPitch(theNew);
        if (theClass->mRollsWithTerrain)
            theRoll = theMap->GetTerrainRoll(theNew);
        theTransformation.SetOrientationYPR(theYaw, thePitch, theRoll);
    }
    theTransformation.SetPosition(theGround.GetX(), theGround.GetY(), theGround.GetZ());
    inUnit->SetTransformation(theTransformation, true);
}

// FUNCTION: EXE 0x55aa64
// Turns a stopped land unit in place to the facing its location goal asks for (tilted with the terrain);
// a still-moving unit brakes first (Step4); does nothing without a location goal
void EEPMLand::Step5(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    EEMap* theMap = ::gWorld.mMap;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    EEUGLocation* theGoal = dynamic_cast<EEUGLocation*>(inUnit->GetGoal());
    float thePitch = 0.0f;
    float theRoll = 0.0f;
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
        theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
        if (theClass->mPitchesWithTerrain || theClass->mRollsWithTerrain) {
            if (theClass->mPitchesWithTerrain)
                thePitch = theMap->GetTerrainPitch(theTransformation);
            if (theClass->mRollsWithTerrain)
                theRoll = theMap->GetTerrainRoll(theTransformation);
            theTransformation.SetOrientationYPR(theYaw, thePitch, theRoll);
        }
        inUnit->SetTransformation(theTransformation, true);
    }
}

// NEAR: register/slot allocation and x87 order
// A blocker on the waypoint's own tile becomes the blocking unit (collision state 2) and the step is undone;
// any retry counts the tick toward the collision-resolution time
// FUNCTION: EXE 0x55abe8
// Checks the land unit's step onto a new tile (terrain, edge, blocking units, diagonal corners); when blocked
// it retries without the x, then without the y part, else undoes it; returns whether the step stands
bool EEPMLand::ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance)
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
            if (theTile == inUnit->mTile) {
                theTile = NULL;
            } else {
                if (!inUnit->GetUnitClass()->CanMoveOnTerrain(inUnit, theTile->mTerrainType) || theMap->IsEdgeTile(theX, theY)) {
                    theResult = false;
                } else {
                    theResult = true;
                    RTSTileUnitNode* theNode = theTile->mUnits;
                    if (theResult) {
                        while (theNode) {
                            theBlocker = theNode->mUnit;
                            theResult = theBlocker->IsPassableBy(inUnit)
                                || (theBlocker->mUnitClass->IsLandBuilding() && !theBlocker->mComplexUnit->mIsBuilt);
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
        inUnit->mCollisionResolutionCount += gPhysics->GetTickTime();
    return theResult;
}
