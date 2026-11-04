// C:\EEX\Empire Earth\Physics Thread\EEPhysicsModel.cpp (file name inferred: no __FILE__; RTTI EEPhysicsModel; one
// unit per physics-model class, alphabetical link order). Range 0x1543b7..0x156df8 (each unit ends with its own
// inline-COMDAT cluster).
// The singletons' creation/destruction (0x1543b7 / 0x1550ba) open the unit: they `new` every subclass.
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
#include "ee/EEGraphicObject.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUnitBehavior.h"

extern EEToHitTable gToHitTable;           // 0x5318e8 (Empire Earth.cpp; as EECPAttackSubGroupUnit.cpp declares it)

long gPhysicsModels[12];            // 0x531d58: one `new` of each EEPM* class (EEUnitClass +0x110 points at one)
// This unit's .data is 0x4c2368..0x4c2380 (after EEPhysics.cpp's RTTI, before the EEPMProjectile type descriptor):
// 0.5f, 0.75f, 1.0f, 4000, 2000, 1. 0x4c236c/0x4c2370 have no reader in the exe and stay undefined here.
float gPhysicsModelsF = 0.5f;       // 0x4c2368: world table entry 79's float, set with the singletons (provisional name)
static const float kPMTurnEpsilon = 0.001f;   // 0x442384 (unit-local const; provisional name)
long gUnitAIWaitScale = 4000;               // 0x4c2374 (EEUnitAI 0x22eca0's wait scale; provisional name)
unsigned long gCollisionDetectionUnit = 2000;   // 0x4c2378 (EmpireEarth.h; EEWorld resets it to 2000)
unsigned long gCollisionPeriod = 1;         // 0x4c237c (EEPhysics.cpp's collision period; provisional name)
static const float kPMEdgeOffset = 0.001f;    // 0x44237c (unit-local const: Virtual58 reloads it per use; provisional name)
static const float kPMFiveDegrees = 0.08726647f;   // 0x442394 (unit-local const: Virtual44's heading test; provisional name)
static const float kPMMinimumStep = 0.005f;   // 0x44238c (unit-local const; provisional name)
static const float kPMStopDistance = 0.02f;   // 0x442380 (unit-local const; provisional name)
static const float kPMMoveDistance = 0.03f;   // 0x442388 (unit-local const; provisional name)
static const float kPMMinimumSpeed = 1.0f;   // 0x442390 (a unit-local const: Virtual3C reloads it per use; provisional name)
static const float kPMContactWindow = 0.25f;  // 0x442398 (unit-local const: Virtual44's two contact-time tests; provisional name)
static const float kPMRateScale = 1.0f;       // 0x44239c (unit-local const: Virtual18/24/28 multiply by it from memory; provisional name)
static const float kPMMaximumTilt = 0.34906587f;  // 0x4423a0 (unit-local const, 20 degrees: Virtual20; provisional name)

// FUNCTION: EXE 0x5543b7
// Creates the twelve physics model singletons (land to spaceship) and reads world table entry 79's float
void EEInitPhysicsModels()
{
    gPhysicsModels[0] = (long) new EEPMLand;
    gPhysicsModels[1] = (long) new EEPMAirplane;
    gPhysicsModels[2] = (long) new EEPMHelicopter;
    gPhysicsModels[3] = (long) new EEPMHovercraft;
    gPhysicsModels[4] = (long) new EEPMNaval;
    gPhysicsModels[5] = (long) new EEPMSubmergedNaval;
    gPhysicsModels[6] = (long) new EEPMProjectile;
    gPhysicsModels[7] = (long) new EEPMMissile;
    gPhysicsModels[8] = (long) new EEPMBuilding;
    gPhysicsModels[9] = (long) new EEPMSeaAnimal;
    gPhysicsModels[10] = (long) new EEPMAirAnimal;
    gPhysicsModels[11] = (long) new EEPMSpaceship;
    gPhysicsModelsF = gWorldTable.GetFloatValue(79);
}

// FUNCTION: EXE 0x55453a
// Accelerates or brakes the unit's pitch rate toward the target pitch (capped at its top turn rate) and
// returns the new rate and pitch
void EEPhysicsModel::ApproachPitch(EEComplexUnit* inUnit, float inCurrent, float inTarget, float* outRate, float* outNew)
{
    float theStep = gPhysics->mTickSeconds;
    float theAccel = inUnit->GetAngularAcceleration();
    float theMax = inUnit->GetMaxAngularVelocity();
    float theRate = inUnit->mAngularVelocity.mPitch;
    float theDelta = inTarget - inCurrent;
    switch (this->PitchMode(inUnit, theRate, theDelta)) {
    case 2:
        break;
    case 3:
        if (theDelta < 0.0f) {
            theRate = theRate - theAccel * theStep;
            if (theRate < -theMax)
                theRate = -theMax;
        } else {
            theRate = theAccel * theStep + theRate;
            if (theRate > theMax)
                theRate = theMax;
        }
        break;
    case 4:
        if (theDelta < 0.0f) {
            theRate = kPMRateScale * theAccel * theStep + theRate;
            if (theRate > 0.0f)
                theRate = 0.0f;
        } else {
            theRate = theRate - theAccel * theStep;
            if (theRate < 0.0f)
                theRate = 0.0f;
        }
        break;
    default:
        theRate = 0.0f;
        break;
    }
    *outRate = theRate;
    *outNew = theRate * theStep + inCurrent;
}

// FUNCTION: EXE 0x55463e
// Returns the unit's top turn rate (its active group's when it is in one)
float EEComplexUnit::GetMaxAngularVelocity()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetMaxAngularVelocity(this);
    return this->mStats->GetMaxAngularVelocity();
}

// FUNCTION: EXE 0x55465b
// Returns the unit's turn acceleration (its active group's when it is in one)
float EEComplexUnit::GetAngularAcceleration()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetAngularAcceleration(this);
    return this->mStats->mAngularAcceleration;
}

// FUNCTION: EXE 0x554678
// Pitches the unit toward the slope from inFrom to inTo (over inDistance); with no distance it keeps its pitch
// and stops pitching
void EEPhysicsModel::PitchTowardSlope(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float inDistance, float* outYaw, float* outPitch)
{
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    inUnit->mPhysicalModel.GetLocation().Apply(theForward, theResult);
    float theAngle = UMArcCos(-theResult.mZ) - 1.5707964f;
    if (inDistance > 0.0f) {
        float theTarget = UMArcCos(-((inTo.GetZ() - inFrom.GetZ()) / inDistance)) - 1.5707964f;
        this->ApproachPitch(inUnit, theAngle, theTarget, outYaw, outPitch);
    } else {
        *outYaw = 0.0f;
        *outPitch = theAngle;
    }
}

// FUNCTION: EXE 0x554738
// Accelerates or brakes the unit's yaw rate toward a target heading; returns the angle left, the new rate and
// heading (inFlag: aim the class's facing offset, or either side for a broadside ship)
void EEPhysicsModel::TurnTowardAngle(EEComplexUnit* inUnit, float inCurrent, float inTarget, float* outA, float* outB, float* outC, bool inFlag)
{
    float theStep = gPhysics->mTickSeconds;
    EEUnitClass* theClass = inUnit->mUnitClass;
    float theAccel = inUnit->GetAngularAcceleration();
    float theMax = inUnit->GetMaxAngularVelocity();
    float theRate = inUnit->mAngularVelocity.mYaw;
    float theDelta;
    if (!inFlag) {
        theDelta = inTarget - inCurrent;
        if (theDelta < -3.1415927f)
            theDelta = theDelta + 6.2831855f;
        else if (theDelta > 3.1415927f)
            theDelta = theDelta - 6.2831855f;
    } else {
        float theFacing = inCurrent + theClass->GetFacingOffset();
        if (theFacing > 3.1415927f)
            theFacing = theFacing - 6.2831855f;
        else if (theFacing < -3.1415927f)
            theFacing = theFacing + 6.2831855f;
        theDelta = inTarget - theFacing;
        if (theDelta < -3.1415927f)
            theDelta = theDelta + 6.2831855f;
        else if (theDelta > 3.1415927f)
            theDelta = theDelta - 6.2831855f;
        if (theClass->mFiresBroadside) {
            theFacing = theFacing + 3.1415927f;
            if (theFacing > 3.1415927f)
                theFacing = theFacing - 6.2831855f;
            else if (theFacing < -3.1415927f)
                theFacing = theFacing + 6.2831855f;
            inTarget = inTarget - theFacing;
            if (inTarget < -3.1415927f)
                inTarget = inTarget + 6.2831855f;
            else if (inTarget > 3.1415927f)
                inTarget = inTarget - 6.2831855f;
            float theAbsOther = UMAbs(inTarget);
            float theAbsDelta = UMAbs(theDelta);
            if (theAbsOther < theAbsDelta)
                theDelta = inTarget;
        }
    }
    float theNew;
    switch (this->YawMode(inUnit, theRate, theDelta)) {
    case 2:
        theNew = theRate;
        break;
    case 3:
        theNew = theAccel * theStep;
        if (theDelta < 0.0f) {
            theNew = theRate - theNew;
            if (theNew < -theMax)
                theNew = -theMax;
        } else {
            theNew = theNew + theRate;
            if (theMax < theNew)
                theNew = theMax;
        }
        break;
    case 4: {
        float theChange = theAccel * theStep;
        if (theRate < 0.0f) {
            theNew = theChange + theRate;
            if (theDelta < 0.0f && theNew > -theChange)
                theNew = -theChange;
            else if (theDelta == 0.0f && theNew > -kPMEdgeOffset)
                theNew = 0.0f;
        } else {
            theNew = theRate - theChange;
            if (theDelta > 0.0f && theNew < theChange)
                theNew = theChange;
            else if (theDelta == 0.0f && theNew < kPMEdgeOffset)
                theNew = 0.0f;
        }
        break;
    }
    default:
        theNew = 0.0f;
        break;
    }
    float theMove = theNew * theStep;
    if ((theDelta < 0.0f && theMove < theDelta) || (theDelta > 0.0f && theDelta < theMove))
        theMove = theDelta;
    *outA = theDelta;
    *outB = theNew;
    *outC = theMove + inCurrent;
}

// FUNCTION: EXE 0x554a25
// Turns the unit from its current heading toward the bearing from inFrom to inTo (as TurnTowardAngle)
void EEPhysicsModel::TurnToward(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float* outA, float* outB, float* outC, bool inUseFacingOffset)
{
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    float theBearing = UMArcTan2(-(inTo.GetX() - inFrom.GetX()), inTo.GetY() - inFrom.GetY());
    inUnit->mPhysicalModel.GetLocation().Apply(theForward, theResult);
    float theHeading = UMArcTan2(-theResult.mX, theResult.mY);
    this->TurnTowardAngle(inUnit, theHeading, theBearing, outA, outB, outC, inUseFacingOffset);
}

// FUNCTION: EXE 0x554ada
// Turns the unit at a steady rate (one way in physics order 0, the other way else), at most 20 degrees/s
// unless it faces an enemy; returns 0, the new rate and the new heading
void EEPhysicsModel::SteadyTurn(EEComplexUnit* inUnit, float* outA, float* outB, float* outC)
{
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    float theStep = gPhysics->mTickSeconds;
    float theAccel = inUnit->mStats->mAngularAcceleration;
    float theMax = inUnit->mStats->GetMaxAngularVelocity();
    float theRate = inUnit->mAngularVelocity.mYaw;
    long theState = inUnit->mCurrentPhysicsOrder;
    if (theMax > kPMMaximumTilt) {
        long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
        if (theCommand != kActionFaceEnemyUnit)
            theMax = kPMMaximumTilt;
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

// FUNCTION: EXE 0x554bde
// Returns the unit's new speed: accelerates (not past its top speed or its last waypoint), holds, or brakes
// to a stop, as SpeedMode decides
float EEPhysicsModel::Accelerate(EEComplexUnit* inUnit, float inDistance, float inTurn)
{
    float theSpeed = inUnit->GetVelocity();
    float theMaxSpeed = inUnit->GetMaxVelocity();
    float theAccel = inUnit->GetAcceleration();
    float theStep = gPhysics->mTickSeconds;
    long theKind = inUnit->mWaypoints.size();
    float theResult;
    switch (this->SpeedMode(inUnit, inDistance, inTurn)) {
    case 2:
        theResult = theSpeed;
        break;
    case 3:
        theResult = theStep * theAccel + theSpeed;
        if (theResult > theMaxSpeed)
            theResult = theMaxSpeed;
        else if (theKind == 1 && theResult > inDistance)
            theResult = inDistance;
        break;
    case 4:
        theResult = theSpeed - kPMRateScale * theStep * theAccel;
        if (theResult < 0.005f)
            theResult = 0.0f;
        break;
    default:
        theResult = 0.0f;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x554c95
// Returns the unit's acceleration (its active group's when it is in one)
float EEComplexUnit::GetAcceleration()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetAcceleration(this);
    return this->mStats->GetAcceleration();
}

// FUNCTION: EXE 0x554cb2
// Returns the unit's speed mode: 3 accelerate, 2 hold, 4 brake (a sharp turn, within braking distance of the
// path's end, or faster than the slope allows)
long EEPhysicsModel::SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn)
{
    float theStep = gPhysics->mTickSeconds;
    float theBrake = inUnit->GetAcceleration();
    inUnit->GetMaxVelocity();
    float theSpeed = inUnit->GetVelocity();
    float theTurnPenalty = inUnit->mUnitClass->mF18C;
    float theTime = gPhysics->mTicksPerGameSecond;
    float theDecel = inUnit->GetInverseAcceleration();
    float theTicks = (theDecel * theTime * theSpeed) / kPMRateScale;
    float theStopDistance = theStep * theStep * (theTicks + 1.0f) * theTicks * 0.5f * kPMRateScale * theBrake;
    if (theStopDistance < 0.0001f)
        theStopDistance = 0.0f;
    float theLimit = inDistance;
    if (inDistance > 0.5f)
        theLimit = 0.5f;
    float theSlide = theLimit * 0.5f * inUnit->mAngularVelocity.GetYaw() * theStep;
    if (theSlide < 0.0f)
        theSlide = -theSlide;
    long theResult = 3;
    if (inTurn < -0.7853982f || inTurn > 0.7853982f) {
        if (theSlide <= theSpeed * theStep)
            theResult = 4;
        else if (theSlide <= (theBrake * theStep + theSpeed) * theStep)
            theResult = 2;
    }
    float theRemaining;
    if ((long)inUnit->mWaypoints.size() >= 2)
        inUnit->GetPathLength(theRemaining);
    else
        theRemaining = inDistance;
    if (theRemaining <= theStopDistance)
        theResult = 4;
    if (!(theTurnPenalty < 1.0f))
        return theResult;
    float theSlope = ::gWorld.mMap->GetTerrainPitch(inUnit->mPhysicalModel.GetLocation()) * 1.9098593f;
    if (theSlope < 0.0f) {
        theSlope = 0.0f;
    } else {
        if (theSlope > 1.0f)
            theSlope = 1.0f;
    }
    if ((1.0f - theSlope * theTurnPenalty) * inUnit->GetMaxVelocity() < inUnit->GetVelocity())
        theResult = 4;
    return theResult;
}

// FUNCTION: EXE 0x554e7b
// Returns the inverse of the unit's acceleration (its active group's when it is in one)
float EEComplexUnit::GetInverseAcceleration()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetInverseAcceleration(this);
    return this->mStats->mInverseAcceleration;
}

// FUNCTION: EXE 0x554e98
// Returns 4 (brake) when the pitch rate needs the whole remaining angle to stop, else 3 (accelerate)
long EEPhysicsModel::PitchMode(EEComplexUnit* inUnit, float inRate, float inAngle)
{
    if (inAngle < 0.0f)
        inAngle = -inAngle;
    if (inRate < 0.0f)
        inRate = -inRate;
    float theTime = gPhysics->mTicksPerGameSecond;
    float theDecel = inUnit->GetInverseAngularAcceleration();
    float theStep = gPhysics->GetTickSeconds();
    float theTicks = theDecel;
    theTicks *= theTime * inRate;
    float theTurn = theStep;
    if ((theTicks + 1.0f) * theTicks * 0.5f * (theTurn * theStep * inUnit->GetAngularAcceleration()) >= inAngle)
        return 4;
    return 3;
}

// FUNCTION: EXE 0x554f39
// Returns the inverse of the unit's turn acceleration (its active group's when it is in one)
float EEComplexUnit::GetInverseAngularAcceleration()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetInverseAngularAcceleration(this);
    return this->mStats->mInverseAngularAcceleration;
}

// FUNCTION: EXE 0x554f56
// Returns the yaw rate's mode: 4 brake (turning away, or too close to stop otherwise), 2 hold (on target
// and still), else 3 accelerate
long EEPhysicsModel::YawMode(EEComplexUnit* inUnit, float inRate, float inAngle)
{
    if ((!(inRate >= 0.0f) && !(inAngle <= 0.0f)) || (!(inRate <= 0.0f) && !(inAngle >= 0.0f)))
        return 4;
    if (inAngle == 0.0f) {
        if (inRate == 0.0f)
            return 2;
    } else {
        if (inAngle < 0.0f)
            inAngle = -inAngle;
        inRate = UMAbs(inRate);
        float theStep = gPhysics->GetTickSeconds();
        float theSpeed = inUnit->GetAngularAcceleration() * theStep + inRate;
        inAngle = inAngle - gPhysics->GetTickSeconds() * theSpeed;
        if (inAngle >= 0.0f) {
            float theTime = gPhysics->mTicksPerGameSecond;
            float theTicks = inUnit->GetInverseAngularAcceleration() * theTime * theSpeed;
            long theCount = UMFloatToLong(theTicks);
            if ((float) theCount < theTicks)
                theCount = theCount + 1;
            float theStep1 = gPhysics->GetTickSeconds();
            float theStep2 = gPhysics->GetTickSeconds();
            if ((float) (theCount - 1) * theStep1 * theSpeed - theStep2 * theStep2 * inUnit->GetAngularAcceleration() * (float) ((theCount * (theCount - 1)) / 2 + 1) < inAngle)
                return 3;
        }
    }
    return 4;
}

// FUNCTION: EXE 0x5550ba
// Destroys the twelve physics model singletons
void EEFreePhysicsModels()
{
    for (long theIndex = 0; theIndex < 12; ++theIndex) {
        if (gPhysicsModels[theIndex])
            delete reinterpret_cast<EEPhysicsModel*>(gPhysicsModels[theIndex]);
        gPhysicsModels[theIndex] = 0;
    }
}

// FUNCTION: EXE 0x5550dc
// Re-bobs a unit that stands still (except under the helicopter model), else brakes it as Step4
void EEPhysicsModel::Step3(EEComplexUnit* inUnit)
{
    if (inUnit->GetVelocity() == 0.0f && inUnit->mAngularVelocity.mYaw == 0.0f && reinterpret_cast<long>(this) != gPhysicsModels[2]) {
        this->RemoveBob(inUnit);
        this->ApplyBob(inUnit);
    } else {
        this->Step4(inUnit);
    }
}

// FUNCTION: EXE 0x55512b
// Brakes the unit's yaw rate toward zero (within its top turn rate) and returns the new rate and heading
void EEPhysicsModel::SpinDown(EEComplexUnit* inUnit, float* outRate, float* outNew)
{
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    float theStep = gPhysics->mTickSeconds;
    float theRate = inUnit->mAngularVelocity.mYaw;
    inUnit->mPhysicalModel.GetLocation().Apply(theForward, theResult);
    float theHeading = UMArcTan2(-theResult.mX, theResult.mY);
    float theNew;
    if (theRate < 0.0f) {
        theNew = inUnit->GetAngularAcceleration() * theStep + theRate;
        if (theNew > 0.0f)
            theNew = 0.0f;
        if (theNew < -inUnit->GetMaxAngularVelocity())
            theNew = -inUnit->GetMaxAngularVelocity();
    } else if (theRate > 0.0f) {
        theNew = theRate - inUnit->GetAngularAcceleration() * theStep;
        if (theNew < 0.0f)
            theNew = 0.0f;
        if (inUnit->GetMaxAngularVelocity() < theNew)
            theNew = inUnit->GetMaxAngularVelocity();
    } else {
        theNew = 0.0f;
    }
    *outRate = theNew;
    *outNew = theNew * theStep + theHeading;
}

// Twin of EEPMNaval::PushMoving 0x55c1cb
// FUNCTION: EXE 0x555247
// Resolves a moving unit running into another: pushes the smaller unit aside, or stops, slows or backs off
// this unit (sending a stuck one idle); returns true when it gave up its goals
bool EEPhysicsModel::PushMoving(EEComplexUnit* inUnit, EEComplexUnit* inOther)
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
    float theOtherLeft = 0.0f;
    float theLeft = 0.0f;
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
        && (theCrossOtherT < 0.0f ? -theCrossOtherT : theCrossOtherT) * theOtherSpeed <= 0.5f
        && ((theCrossT > 0.0f && theCrossT <= (theOtherSpeed + theSpeed) * 0.5) || inUnit->GetVelocity() == 0.0f) && theAngle <= kPMFiveDegrees) {
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
    if (theOtherSpeed <= theSpeed && !theNear && inOther->GetMaxVelocity() > 0.0f) {
        if (inUnit->GetVelocity() > 0.0f && inOther->GetVelocity() > 0.0f && theSpeed == theOtherSpeed) {
            if ((long) inOther->mWaypoints.size() > 0)
                inOther->GetPathLength(theOtherLeft);
            if ((long) inUnit->mWaypoints.size() > 0)
                inUnit->GetPathLength(theLeft);
            if (theLeft > theOtherLeft || (theOtherLeft == theLeft && inOther->mUnitID < inUnit->mUnitID)) {
                inUnit->SetVelocity(inUnit->GetVelocity() * 0.25f);
                inUnit->mCollisionResolutionCount = 0.0f;
            }
        }
        float theDX = inOther->GetMaxVelocity() * (1.25f - theT) * theOtherLine.GetDirection().GetX() * theOtherSign;
        float theDY = inOther->GetMaxVelocity();
        theDY *= (1.25f - theT) * theOtherLine.GetDirection().GetY() * theOtherSign;
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
    theSpeed = theSlow + 0.5f;
    float theDX = inUnit->GetMaxVelocity() * (theSpeed - theT) * theLine.GetDirection().GetX() * theSign;
    float theDY = inUnit->GetMaxVelocity() * (theSpeed - theT) * theLine.GetDirection().GetY() * theSign;
    inUnit->SetPushVelocityX(inUnit->GetPushVelocityX() + theDX);
    inUnit->SetPushVelocityY(inUnit->GetPushVelocityY() + theDY);
    if (theSlow > 0.0f) {
        inUnit->SetVelocity(inUnit->GetVelocity() * 0.5f);
    } else {
        inUnit->SetVelocity(0.0f);
        inUnit->mCollisionResolutionCount += gPhysics->GetTickTime();
    }
    if ((inOther->GetPushVelocityX() < 0.0f ? -inOther->GetPushVelocityX() : inOther->GetPushVelocityX()) < kPMTurnEpsilon
        && (inOther->GetPushVelocityY() < 0.0f ? -inOther->GetPushVelocityY() : inOther->GetPushVelocityY()) < kPMTurnEpsilon) {
        inOther->SetPushVelocityX(kPMTurnEpsilon);
        inOther->SetPushVelocityY(kPMTurnEpsilon);
    }
    return theResult;
}

// FUNCTION: EXE 0x555984
// Pushes inOther off an overlapping unit (by the overlap times the radius ratio, less for its attack target);
// stops the unit at its last waypoint under inOther; lets a large enemy trample a small one (world flag 2)
// Overlap: closer than the mean radius; the push is split by the squared x share. A melee attacker as large as
// inOther that runs into it (both attacking, inOther reloading, ahead of it) slides off; a stopped inOther
// that is larger or immobile pushes the unit back. Always returns false
bool EEPhysicsModel::Push(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    GE3DPoint thePosition;
    float theRadius = inUnit->mCollisionRadius;
    float theOtherRadius = inOther->mCollisionRadius;
    float theDX = inUnit->mPhysicalModel.GetLocation().GetX() - inOther->mPhysicalModel.GetLocation().GetX();
    float theDY = inUnit->mPhysicalModel.GetLocation().GetY() - inOther->mPhysicalModel.GetLocation().GetY();
    float theReach = (theOtherRadius + theRadius) * 0.5f;
    float theReachSquared = theReach * theReach;
    float theXSquared = theDX * theDX;
    float theYSquared = theDY * theDY;
    if (theYSquared + theXSquared > theReachSquared)
        return false;
    if (theXSquared == 0.0f && theYSquared == 0.0f)
        theXSquared = theYSquared = 0.1f;
    float theDistanceSquared = theYSquared + theXSquared;
    float theShare = theXSquared / theDistanceSquared;
    float thePush = (theReachSquared - theDistanceSquared) * (theRadius / theOtherRadius);
    float theMoveX = theDX < 0.0f ? thePush * theShare : -(thePush * theShare);
    float theMoveY = theDY < 0.0f ? (1.0f - theShare) * thePush : -((1.0f - theShare) * thePush);
    if (inUnit->mWaypoints.size() == 1) {
        GE3DPoint theNext;
        float theArg;
        inUnit->GetPathStart(theNext, theArg);
        const GETransformation& theOtherTransformation = inOther->mPhysicalModel.GetLocation();
        GE3DPoint theOtherPosition(theOtherTransformation.GetX(), theOtherTransformation.GetY(),
                                   theOtherTransformation.GetZ());
        GE2DPoint theOther2D(theOtherPosition.mX, theOtherPosition.GetY());
        GE2DPoint theNext2D(theNext.mX, theNext.GetY());
        float theHalf = theOtherRadius * 0.5f;
        if (theNext2D.DistanceToSquared(theOther2D) < theHalf * theHalf) {
            inUnit->SetVelocity(0.0f);
            if (inUnit->mCollisionState != 8)
                inUnit->mCollisionState = 8;
        }
    }
    if (!(theOtherRadius > theRadius) && inOther->GetMaxVelocity() > 0.0f) {
        if (theOtherRadius == theRadius && inUnit->mUnitClass->mIsMelee
            && (inOther->mPlayer == inUnit->mPlayer || inUnit->mPlayer->mEnemies[inOther->mPlayer->mPlayerID] == 0)
            && inUnit->GetVelocity() > 0.0f && inOther->mReloadTimer != 0
            && inOther->mGoalState != NULL && inOther->mGoalState->mKind == kGoalAttackUnit
            && inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit) {
            GE3DPoint theGoal;
            const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
            GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
            inUnit->mGoalState->GetGoalLocation(theGoal);
            if (theLocation.DistanceToSquared(theGoal) < theRadius + theRadius + 4.0f) {
                GEVector theUp(0.0f, 1.0f, 0.0f);
                GEVector theVelocity;
                GEVector theToOther;
                GEVector theForward;
                theLocation.mX = theTransformation.GetX();
                theLocation.mY = theTransformation.GetY();
                theLocation.mZ = theTransformation.GetZ();
                const GETransformation& theOtherTransformation = inOther->mPhysicalModel.GetLocation();
                GE3DPoint theOtherLocation(theOtherTransformation.GetX(), theOtherTransformation.GetY(),
                                           theOtherTransformation.GetZ());
                theToOther.SetX(theOtherLocation.mX - theLocation.mX);
                theToOther.SetY(theOtherLocation.GetY() - theLocation.GetY());
                theToOther.SetZ(theOtherLocation.mZ - theLocation.mZ);
                theToOther.Normalize();
                inUnit->mPhysicalModel.GetLocation().Apply(theUp, theForward);
                theForward.Normalize();
                float theSpeed = inUnit->GetVelocity();
                theForward.mX = theSpeed * theForward.mX;
                theForward.mY = theSpeed * theForward.mY;
                theForward.mZ = theSpeed * theForward.mZ;
                float theDot = theToOther.GetX() * theForward.mX + theToOther.mZ * theForward.mZ + theToOther.mY * theForward.mY;
                if (theDot > 0.0f) {
                    GETransformation theTurn(false);
                    theTurn.SetOrientationYPR(UMArcTan2(inUnit->mPhysicalModel.GetLocation().mMatrix[1][0],
                                                        inUnit->mPhysicalModel.GetLocation().mMatrix[0][0]),
                                              0.0f, 0.0f);
                    theTurn.mMatrix[0][3] = 0.0f;
                    theTurn.mMatrix[1][3] = 0.0f;
                    theTurn.mMatrix[2][3] = 0.0f;
                    theTurn.mMatrix[3][0] = 0.0f;
                    theTurn.mMatrix[3][1] = 0.0f;
                    theTurn.mMatrix[3][2] = 0.0f;
                    theTurn.mMatrix[3][3] = 1.0f;
                    theUp.mY = inUnit->GetVelocity();
                    theTurn.Apply(theUp, theVelocity);
                    inUnit->SetPushVelocityX(inUnit->GetPushVelocityX() - theVelocity.mX);
                    inUnit->SetPushVelocityY(inUnit->GetPushVelocityY() - theVelocity.GetY());
                    float theScale = theDot / (theToOther.GetX() * theToOther.GetX() + theToOther.GetZ() * theToOther.GetZ() + theToOther.GetY() * theToOther.GetY());
                    inUnit->SetPushVelocityX(inUnit->GetPushVelocityX() - theToOther.mX * theScale);
                    inUnit->SetPushVelocityY(inUnit->GetPushVelocityY() - theToOther.GetY() * theScale);
                }
            }
        }
        float theOtherMoveX = theMoveX;
        float theOtherMoveY = theMoveY;
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit
            && inOther == static_cast<EEUGAttackUnit*>(inUnit->mGoalState)->mTarget) {
            theOtherMoveX = theMoveX * 0.25f;
            theOtherMoveY = theMoveY * 0.25f;
        }
        inOther->SetPushVelocityX(inOther->GetPushVelocityX() + theOtherMoveX);
        inOther->SetPushVelocityY(inOther->GetPushVelocityY() + theOtherMoveY);
    } else if ((inOther->GetPushVelocityX() < 0.0f ? -inOther->GetPushVelocityX() : inOther->GetPushVelocityX()) < kPMTurnEpsilon
               && (inOther->GetPushVelocityY() < 0.0f ? -inOther->GetPushVelocityY() : inOther->GetPushVelocityY()) < kPMTurnEpsilon) {
        inOther->SetPushVelocityX(kPMTurnEpsilon);
        inOther->SetPushVelocityY(kPMTurnEpsilon);
    }
    if (inOther->GetVelocity() == 0.0f && (theOtherRadius > theRadius || inOther->GetMaxVelocity() == 0.0f)) {
        inUnit->SetPushVelocityX(inUnit->GetPushVelocityX() - (theMoveX + theMoveX));
        inUnit->SetPushVelocityY(inUnit->GetPushVelocityY() - (theMoveY + theMoveY));
    }
    if ((!(gWorld.mCollisionDetectionFlags & 2)) || !(inUnit->GetVelocity() > 0.0f) || inOther->GetVelocity() != 0.0f || inOther->mUnitClass->mTileSize != 1
        || !(theOtherRadius < theRadius) || !(theDistanceSquared > gPhysicsModelsF) || inOther->mPlayer == inUnit->mPlayer
        || inUnit->mPlayer->mEnemies[inOther->mPlayer->mPlayerID] == 0)
        return false;
    float theDamage = inUnit->mStats->GetAttack();
    float theFactor = gWeaponToHitFactors[inOther->mUnitClass->mWeapon][inUnit->mUnitClass->mWeapon];
    float theHit = gToHitTable.GetAttackFactor(inUnit->mUnitClass->mAmmoType, inOther->mUnitClass,
                                        inUnit->mPlayer->mPlayerID, inOther->mPlayer->mPlayerID) * theFactor;
    if (theHit > 0.0f)
        theDamage = theDamage - UMFloatToLong(inOther->mStats->mArmor[inUnit->mUnitClass->mWeapon]);
    if (theHit * theDamage > 1.0f)
        inOther->TakeHit(inUnit, 1, inUnit->mPlayer->mPlayerID, 1);
    return false;
}

// FUNCTION: EXE 0x5560cf
// Pushes the unit against each unit it touches (PushMoving for two large mobile units, else Push) until one
// is handled; skips closed gates and citizens working elsewhere
void EEPhysicsModel::Collide(EEComplexUnit* inUnit)
{
    float theSpeed = inUnit->mCollisionRadius;
    bool theCollide;
    for (UListNode<EEComplexUnit*>* theNode = inUnit->mContacts.mHead; theNode; theNode = theNode->mNext) {
        // each contact is judged on its own: a skip does not carry over to the next contact
        theCollide = true;
        if (theNode->mData->mIsCitizen) {
            unsigned char theKind = theNode->mData->mGoalState ? theNode->mData->mGoalState->mKind : 0;
            if (theKind == 6 || theKind == 4) {
                if (!inUnit->mIsCitizen) {
                    theCollide = false;
                } else {
                    unsigned char theOwnKind = inUnit->mGoalState ? inUnit->mGoalState->mKind : 0;
                    if (theOwnKind != theKind)
                        theCollide = false;
                    else if (theOwnKind == kGoalGather)
                        theCollide = static_cast<EEUGGather*>(inUnit->mGoalState)->mResource
                            == ((EEUGGather*) theNode->mData->mGoalState)->mResource;
                    else
                        theCollide = static_cast<EEUGBuildRepair*>(inUnit->mGoalState)->mTarget
                            == ((EEUGBuildRepair*) theNode->mData->mGoalState)->mTarget;
                }
            }
        }
        if (theNode->mData->mUnitClass->mCategory == kUnitCategoryGate) {
            if (dynamic_cast<EEUCGate*>(theNode->mData->mUnitClass)
                && (theNode->mData->mCommand ? theNode->mData->mCommand->mType : 0) != 0x3a99) {
                theCollide = false;
                continue;
            }
            theCollide = true;
        }
        if (!theCollide)
            continue;
        if (theSpeed >= kPMMinimumSpeed && theNode->mData->mCollisionRadius >= kPMMinimumSpeed && theNode->mData->GetMaxVelocity() > 0.0f) {
            if (this->PushMoving(inUnit, theNode->mData))
                return;
        } else {
            if (this->Push(inUnit, theNode->mData))
                return;
        }
    }
}

// FUNCTION: EXE 0x556213
// Applies the collision push to the unit (times the collision period, capped at its speed) onto a tile it
// can stand on, puts it on the ground, and starts or stops an idle unit's shuffle animation
// NEAR: x87 scheduling of the sign/abs/limit block (as Helicopter V0C), the frame layout and the flag threading
void EEPhysicsModel::Move(EEComplexUnit* inUnit)
{
    EETileCoordinate theTile(0, 0);
    float theLimit = kPMMinimumStep / gPhysics->GetTickSeconds();
    bool theStopped = false;
    float theSpeed = max(inUnit->GetMaxVelocity(), theLimit);
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
        if (theDX > theSpeed || theDY > theSpeed) {
            if (theDX > theDY) {
                theDY = theSpeed / theDX * theDY;
                theDX = theSpeed;
            } else {
                theDX = theSpeed / theDY * theDX;
                theDY = theSpeed;
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
        bool theFree = EECanStandOn(theTile, inUnit);
        if (theFree) {
            long theOffsetX = inUnit->mTileX - theTile.mX;
            if (theOffsetX < 0)
                theOffsetX = -theOffsetX;
            if (theOffsetX == 1) {
                long theOffsetY = inUnit->mTileY - theTile.mY;
                if (theOffsetY < 0)
                    theOffsetY = -theOffsetY;
                if (theOffsetY == 1) {
                    EETileCoordinate theSideX;
                    theSideX.mX = theTile.mX;
                    theSideX.mY = inUnit->mTileY;
                    if (!EECanStandOn(theSideX, inUnit)) {
                        EETileCoordinate theSideY;
                        theSideY.mX = inUnit->mTileX;
                        theSideY.mY = theTile.mY;
                        if (!EECanStandOn(theSideY, inUnit))
                            theFree = false;
                    }
                }
            }
        }
        if (theFree) {
            float theTime = inUnit->mCollisionResolutionCount;
            if ((theTile.mX != inUnit->mTileX || theTile.mY != inUnit->mTileY) && theTime == 0.0f
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
                        unsigned long theFrameTime = inUnit->GetModel()->mFrameTimes[13];
                        if (inUnit->mAnimationTime != theFrameTime)
                            inUnit->mAnimationTime = theFrameTime;
                    } else if (inUnit->mAnimation == 0xd && inUnit->mIsAnimationPaused) {
                        inUnit->mIsAnimationPaused = false;
                    }
                } else {
                    theStopped = true;
                }
            }
            inUnit->mCollisionResolutionCount = gPhysics->GetTickTime() + inUnit->mCollisionResolutionCount;
            if (!inUnit->IsSwimming()) {
                const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
                GE3DPoint theGround(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
                inUnit->PlaceOnGround(theGround);
            }
            inUnit->SetTransformation(inUnit->mPhysicalModel.GetLocation(), true);
        } else {
            theStopped = true;
        }
    } else {
        theStopped = true;
    }
    if (theStopped && (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionIdle && inUnit->mAnimation == 0xd)
        inUnit->SetAnimation(7, -1, 1);
    if (inUnit->mPushVelocityX != 0.0f)
        inUnit->mPushVelocityX = 0.0f;
    if (inUnit->mPushVelocityY != 0.0f)
        inUnit->mPushVelocityY = 0.0f;
}

// NEAR: register choice only (ecx vs eax for the class pointer)
// FUNCTION: EXE 0x556681
// Returns whether the unit swims: it has a swim animation, stands on deep water and cruises above the ground
bool EEComplexUnit::IsSwimming()
{
    return this->HasSwimAnimation() && this->mTile && this->mTile->mTerrainType == kTerrainDeepWater
        && this->mUnitClass->GetCruiseAltitude()
            > ::gWorld.mMap->mTerrainMesh.GetMeshZ(this->mPhysicalModel.GetLocation().GetX(), this->mPhysicalModel.GetLocation().GetY());
}

// FUNCTION: EXE 0x5566e5
// Returns whether the unit can move on terrain kind 1 and its model has more than one frame of animation 2
bool EEComplexUnit::HasSwimAnimation()
{
    return this->mUnitClass->CanMoveOnTerrain(this, 1) && this->GetModel()->mFrameCounts[2] > 1;
}

// NEAR: the original compares members against a 0.0f kept in st(1) (fld member; fcomp st(1)); ours fcom [member]
// FUNCTION: EXE 0x556710
// Levels the unit's roll toward zero, then resolves its collisions (Collide, Move) unless it rests with no
// push; a resting or non-collidable unit gets its collision timer cleared
void EEPhysicsModel::AfterStep(EEComplexUnit* inUnit)
{
    if (inUnit->GetRoll() != 0.0f) {
        float theRoll;
        if (inUnit->GetRoll() > 0.0f) {
            theRoll = inUnit->GetRoll();
            theRoll -= gPhysics->mTickSeconds * 0.5f;
            if (theRoll <= 0.0f)
                theRoll = 0.0f;
        } else {
            theRoll = gPhysics->mTickSeconds * 0.5f + inUnit->GetRoll();
            if (theRoll >= 0.0f)
                theRoll = 0.0f;
        }
        inUnit->mRoll = theRoll;
    }
    if ((!inUnit->mIsCollidable || !(inUnit->mContacts.mCount > 0)) && inUnit->GetCollisionResolutionCount() == 0.0f)
        return;
    if (inUnit->mIsCollidable) {
        if (inUnit->GetVelocity() == 0.0f && inUnit->mAngularVelocity.GetYaw() == 0.0f
            && (inUnit->mUnitClass->GetMaxVelocity() <= 0.0f
                || (UMAbs(inUnit->mPushVelocityX) < kPMTurnEpsilon
                    && UMAbs(inUnit->mPushVelocityY) < kPMTurnEpsilon))
            && !(inUnit->mUnitClass->mIsMelee && inUnit->mGoalState && inUnit->mGoalState->mKind == kGoalAttackUnit)) {
            inUnit->mCollisionResolutionCount = 0.0f;
            if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionIdle && inUnit->mAnimation == 0xd)
                inUnit->SetAnimation(7, -1, 1);
        } else {
            this->Collide(inUnit);
            this->Move(inUnit);
        }
    } else {
        inUnit->mCollisionResolutionCount = 0.0f;
    }
}

// FUNCTION: EXE 0x556897
// Runs the step for the unit's physics action (0..5), then the after-step work (not after action 2, nor for
// a dead unit in another action)
void EEPhysicsModel::Step(EEComplexUnit* inUnit)
{
    switch (inUnit->mCurrentPhysicsAction) {
    case 0:
        this->Step0(inUnit);
        break;
    case 1:
        this->Step1(inUnit);
        break;
    case 2:
        this->Step2(inUnit);
        return;
    case 3:
        this->Step3(inUnit);
        break;
    case 4:
        this->Step4(inUnit);
        break;
    case 5:
        this->Step5(inUnit);
        break;
    default:
        if (inUnit->mHitPoints <= 0)
            return;
    }
    this->AfterStep(inUnit);
}

// NEAR: the original refuses some GE3DPoint assignment inlines in a different order
// FUNCTION: EXE 0x556908
// Keeps the unit's new location on the map: undoes a move off the map; a move through one of the terrain's
// edge planes is cut back to it and slid inside; returns false when the move was changed
// (cut back to kPMEdgeOffset inside the plane, then slid along the plane's normal)
bool EEPhysicsModel::ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance)
{
    GE3DLine theEdgeLine;
    GE3DLine theMoveLine;
    GE3DPlane thePlane;
    EEMap* theMap = ::gWorld.mMap;
    long theX = UMFloor(ioLocation.GetX());
    long theY = UMFloor(ioLocation.GetY());
    bool theResult = true;
    float theT;
    if (inMove.GetX() != 0.0f || inMove.GetY() != 0.0f || inMove.GetZ() != 0.0f) {
        theMoveLine.SetStart(ioLocation);
        theMoveLine.SetDirection(inMove);
        if (!theMap->GetTile(theX, theY)) {
            theMoveLine.GetPointAt(-1.0f, ioLocation);
            theResult = false;
        } else {
            if (!theMap->GetTile(theX - 1, theY - 1)) {
                if (theMap->mTerrainMesh.GetLeftPlane().IsPointOnOrBelowPlane(ioLocation)) {
                    theResult = false;
                    if (!theMoveLine.Intersects(theMap->mTerrainMesh.GetLeftPlane(), theT) || theT > 0.0f || theT < -1.0f)
                        theT = -1.0f;
                    theMoveLine.GetPointAt(theT, ioLocation);
                    ioLocation.SetX(ioLocation.GetX() + kPMEdgeOffset);
                    ioLocation.SetY(ioLocation.GetY() + kPMEdgeOffset);
                    if (theMap->mTerrainMesh.GetLeftPlane().IsPointOnOrBelowPlane(ioLocation)) {
                        theEdgeLine.SetStart(ioLocation);
                        theEdgeLine.SetDirection(theMap->mTerrainMesh.GetLeftPlane().GetNormal());
                        theEdgeLine.Intersects(theMap->mTerrainMesh.GetLeftPlane(), theT);
                        theEdgeLine.GetPointAt(theT, ioLocation);
                        ioLocation.SetX(ioLocation.GetX() + kPMEdgeOffset);
                        ioLocation.SetY(ioLocation.GetY() + kPMEdgeOffset);
                    }
                    theMoveLine.SetStart(ioLocation);
                }
            } else if (!theMap->GetTile(theX + 1, theY + 1)) {
                if (theMap->mTerrainMesh.GetRightPlane().IsPointOnOrBelowPlane(ioLocation)) {
                    theResult = false;
                    if (!theMoveLine.Intersects(theMap->mTerrainMesh.GetRightPlane(), theT) || theT > 0.0f || theT < -1.0f)
                        theT = -1.0f;
                    theMoveLine.GetPointAt(theT, ioLocation);
                    ioLocation.SetX(ioLocation.GetX() - kPMEdgeOffset);
                    ioLocation.SetY(ioLocation.GetY() - kPMEdgeOffset);
                    if (theMap->mTerrainMesh.GetRightPlane().IsPointOnOrBelowPlane(ioLocation)) {
                        theEdgeLine.SetStart(ioLocation);
                        theEdgeLine.SetDirection(theMap->mTerrainMesh.GetRightPlane().GetNormal());
                        theEdgeLine.Intersects(theMap->mTerrainMesh.GetRightPlane(), theT);
                        theEdgeLine.GetPointAt(theT, ioLocation);
                        ioLocation.SetX(ioLocation.GetX() - kPMEdgeOffset);
                        ioLocation.SetY(ioLocation.GetY() - kPMEdgeOffset);
                    }
                    theMoveLine.SetStart(ioLocation);
                }
            }
            if (!theMap->GetTile(theX - 1, theY + 1)) {
                if (theMap->mTerrainMesh.GetFrontPlane().IsPointOnOrBelowPlane(ioLocation)) {
                    theResult = false;
                    if (!theMoveLine.Intersects(theMap->mTerrainMesh.GetFrontPlane(), theT) || theT > 0.0f || theT < -1.0f)
                        theT = -1.0f;
                    theMoveLine.GetPointAt(theT, ioLocation);
                    ioLocation.SetX(ioLocation.GetX() + kPMEdgeOffset);
                    ioLocation.SetY(ioLocation.GetY() - kPMEdgeOffset);
                    if (theMap->mTerrainMesh.GetFrontPlane().IsPointOnOrBelowPlane(ioLocation)) {
                        theEdgeLine.SetStart(ioLocation);
                        theEdgeLine.SetDirection(theMap->mTerrainMesh.GetFrontPlane().GetNormal());
                        theEdgeLine.Intersects(theMap->mTerrainMesh.GetFrontPlane(), theT);
                        theEdgeLine.GetPointAt(theT, ioLocation);
                        ioLocation.SetX(ioLocation.GetX() + kPMEdgeOffset);
                        ioLocation.SetY(ioLocation.GetY() - kPMEdgeOffset);
                    }
                }
            } else if (!theMap->GetTile(theX + 1, theY - 1)) {
                if (theMap->mTerrainMesh.GetBackPlane().IsPointOnOrBelowPlane(ioLocation)) {
                    theResult = false;
                    if (!theMoveLine.Intersects(theMap->mTerrainMesh.GetBackPlane(), theT) || theT > 0.0f || theT < -1.0f)
                        theT = -1.0f;
                    theMoveLine.GetPointAt(theT, ioLocation);
                    ioLocation.SetX(ioLocation.GetX() - kPMEdgeOffset);
                    ioLocation.SetY(ioLocation.GetY() + kPMEdgeOffset);
                    if (theMap->mTerrainMesh.GetBackPlane().IsPointOnOrBelowPlane(ioLocation)) {
                        theEdgeLine.Define(ioLocation, theMap->mTerrainMesh.GetBackPlane().GetNormal());
                        theEdgeLine.Intersects(theMap->mTerrainMesh.GetBackPlane(), theT);
                        theEdgeLine.GetPointAt(theT, ioLocation);
                        ioLocation.SetX(ioLocation.GetX() - kPMEdgeOffset);
                        ioLocation.SetY(ioLocation.GetY() + kPMEdgeOffset);
                    }
                }
            }
        }
    }
    return theResult;
}


// SYNTHETIC: EXE 0x556ddb
// (the scalar deleting destructor, emitted with the vtable)

// the ICF-shared copy LINK kept in this unit's range
// SYNTHETIC: EXE 0x556dd8
