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

#include "ee/EEUnitKind.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERules.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUnitGoal.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitBehavior.h"
#include "rust/ee_units.h"

extern EEToHitTable gToHitTable;           // 0x5318e8 (Empire Earth.cpp; as EECPAttackSubGroupUnit.cpp declares it)

EEPhysicsModel* gPhysicsModels[12];            // 0x531d58: one `new` of each EEPM* class (EEUnitClass +0x110 points at one)
// This unit's .data is 0x4c2368..0x4c2380 (after EEPhysics.cpp's RTTI, before the EEPMProjectile type descriptor):
// 0.5f, 0.75f, 1.0f, 4000, 2000, 1. 0x4c236c/0x4c2370 have no reader in the exe and stay undefined here.
float gPhysicsModelsF = 0.5f;       // 0x4c2368: world table entry 79's float, set with the singletons (provisional name)
static const float kPMTurnEpsilon = 0.001f;   // 0x442384 (unit-local const; provisional name)
long gUnitAIWaitScale = 4000;               // 0x4c2374 (EEUnitAI 0x22eca0's wait scale; provisional name)
unsigned long gCollisionDetectionUnit = 2000;   // 0x4c2378 (EmpireEarth.h; EEWorld resets it to 2000)
unsigned long gCollisionPeriod = 1;         // 0x4c237c (EEPhysics.cpp's collision period; provisional name)
static const float kPMEdgeOffset = 0.001f;    // 0x44237c (unit-local const: Virtual58 reloads it per use; provisional name)
static const float kPMFiveDegrees = 0.08726647f;   // 0x442394 (unit-local const: Virtual44's heading test; provisional name)
static const float kPMMoveDistance = 0.03f;   // 0x442388 (unit-local const; provisional name)
static const float kPMMinimumSpeed = 1.0f;   // 0x442390 (a unit-local const: Virtual3C reloads it per use; provisional name)
static const float kPMContactWindow = 0.25f;  // 0x442398 (unit-local const: Virtual44's two contact-time tests; provisional name)
static uint32_t PMBits(float inValue);

// FUNCTION: EXE 0x5543b7
// Creates the twelve physics model singletons (land to spaceship) and reads world table entry 79's float
void EEInitPhysicsModels()
{
    gPhysicsModels[0] = new EEPMLand;
    gPhysicsModels[1] = new EEPMAirplane;
    gPhysicsModels[2] = new EEPMHelicopter;
    gPhysicsModels[3] = new EEPMHovercraft;
    gPhysicsModels[4] = new EEPMNaval;
    gPhysicsModels[5] = new EEPMSubmergedNaval;
    gPhysicsModels[6] = new EEPMProjectile;
    gPhysicsModels[7] = new EEPMMissile;
    gPhysicsModels[8] = new EEPMBuilding;
    gPhysicsModels[9] = new EEPMSeaAnimal;
    gPhysicsModels[10] = new EEPMAirAnimal;
    gPhysicsModels[11] = new EEPMSpaceship;
    gPhysicsModelsF = gWorldTable.GetFloatValue(kWorldTrampleMinDistanceSquared);
}

// the turn's values: the tick, the unit's turn rates (its active group's when it is in one), its yaw rate; the heading,
// the target and the facing are the caller's
static EeUnitsTurnIn PMTurnIn(EEComplexUnit* inUnit)
{
    EeUnitsTurnIn theIn;
    memset(&theIn, 0, sizeof(theIn));
    theIn.tick_seconds = gPhysics->mTickSeconds;
    theIn.ticks_per_second = gPhysics->mTicksPerGameSecond;
    theIn.accel = inUnit->GetAngularAcceleration();
    theIn.max = inUnit->GetMaxAngularVelocity();
    theIn.inverse_accel = inUnit->GetInverseAngularAcceleration();
    theIn.rate = inUnit->mAngularVelocity.mYaw;
    return theIn;
}

// FUNCTION: EXE 0x55453a
// Accelerates or brakes the unit's pitch rate toward the target pitch (capped at its top turn rate) and
// returns the new rate and pitch
void EEPhysicsModel::ApproachPitch(EEComplexUnit* inUnit, float inCurrent, float inTarget, float* outRate, float* outNew)
{
    long theMode = this->PitchMode(inUnit, inUnit->mAngularVelocity.mPitch, inTarget - inCurrent);
    EEPMApproachPitch(inUnit, false, theMode, inCurrent, inTarget, outRate, outNew);
}

// the pitch's values: the unit's (its active group's) or, naval, the class's rates
static EeUnitsPitchIn PMPitchIn(EEComplexUnit* inUnit, bool inNaval)
{
    EeUnitsPitchIn theIn;
    theIn.tick_seconds = gPhysics->mTickSeconds;
    theIn.ticks_per_second = gPhysics->mTicksPerGameSecond;
    theIn.accel = inNaval ? inUnit->mUnitClass->GetAngularAcceleration() : inUnit->GetAngularAcceleration();
    theIn.max = inNaval ? inUnit->mUnitClass->GetMaxAngularVelocity() : inUnit->GetMaxAngularVelocity();
    theIn.inverse_accel = inNaval ? 0.0f : inUnit->GetInverseAngularAcceleration();
    theIn.rate = inUnit->mAngularVelocity.mPitch;
    theIn.naval = inNaval ? 1 : 0;
    return theIn;
}

void EEPMApproachPitch(EEComplexUnit* inUnit, bool inNaval, long inMode, float inCurrent, float inTarget, float* outRate,
                       float* outNew)
{
    EeUnitsPitchIn theIn = PMPitchIn(inUnit, inNaval);
    EeUnitsTurn thePitch = ee_units_pm_approach_pitch(&theIn, inCurrent, inTarget, (int32_t)inMode);
    *outRate = thePitch.rate;
    *outNew = thePitch.heading;
    if (EEUnitStepNoting()) {   // TEST ONLY (USST)
        uint32_t theWords[10] = {theIn.naval, EEPMBits(theIn.tick_seconds), EEPMBits(theIn.accel), EEPMBits(theIn.max),
                                 EEPMBits(theIn.rate), EEPMBits(inCurrent), EEPMBits(inTarget), (uint32_t)inMode,
                                 EEPMBits(*outRate), EEPMBits(*outNew)};
        EEUnitStateNote(kUnitStateApproachPitch, inUnit->mUnitID, theWords, 10);
    }
}

long EEPMPitchMode(EEComplexUnit* inUnit, bool inNaval, float inRate, float inAngle)
{
    EeUnitsPitchIn theIn = PMPitchIn(inUnit, inNaval);
    long theMode = ee_units_pm_pitch_mode(&theIn, inRate, inAngle);
    if (EEUnitStepNoting()) {   // TEST ONLY (USST)
        uint32_t theWords[8] = {theIn.naval, EEPMBits(theIn.tick_seconds), EEPMBits(theIn.ticks_per_second),
                                EEPMBits(theIn.accel), EEPMBits(theIn.inverse_accel), EEPMBits(inRate), EEPMBits(inAngle),
                                (uint32_t)theMode};
        EEUnitStateNote(kUnitStatePitchMode, inUnit->mUnitID, theWords, 8);
    }
    return theMode;
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
    EeUnitsTurnIn theIn = PMTurnIn(inUnit);
    EEUnitClass* theClass = inUnit->mUnitClass;
    theIn.current = inCurrent;
    theIn.target = inTarget;
    theIn.use_facing = inFlag ? 1 : 0;
    theIn.broadside = inFlag && theClass->mFiresBroadside ? 1 : 0;
    theIn.facing_offset = inFlag ? theClass->GetFacingOffset() : 0.0f;
    EeUnitsTurn theTurn = ee_units_pm_turn(&theIn);
    *outA = theTurn.delta;
    *outB = theTurn.rate;
    *outC = theTurn.heading;
    if (EEUnitStepNoting()) {   // TEST ONLY (USST)
        uint32_t theWords[14] = {EEPMBits(theIn.tick_seconds), EEPMBits(theIn.ticks_per_second), EEPMBits(theIn.accel),
                                 EEPMBits(theIn.max), EEPMBits(theIn.inverse_accel), EEPMBits(theIn.rate),
                                 EEPMBits(inCurrent), EEPMBits(inTarget), theIn.use_facing, theIn.broadside,
                                 inFlag ? EEPMBits(theIn.facing_offset) : 0u, EEPMBits(*outA), EEPMBits(*outB), EEPMBits(*outC)};
        EEUnitStateNote(kUnitStateTurn, inUnit->mUnitID, theWords, 14);
    }
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
    uint32_t theCommand = (uint32_t)(inUnit->mCommand ? inUnit->mCommand->mType : 0);
    inUnit->mPhysicalModel.GetLocation().Apply(theForward, theResult);
    float theHeading = UMArcTan2(-theResult.mX, theResult.mY);
    EeUnitsTurn theTurn = ee_units_pm_steady_turn(theRate, theHeading, theStep, theAccel, theMax, (int32_t)theState, theCommand);
    *outA = 0.0f;
    *outB = theTurn.rate;
    *outC = theTurn.heading;
    if (EEUnitStepNoting()) {   // TEST ONLY (USST)
        uint32_t theWords[9] = {EEPMBits(theStep), EEPMBits(theAccel), EEPMBits(theMax), EEPMBits(theRate),
                                EEPMBits(theHeading), (uint32_t)theState, theCommand, EEPMBits(*outB), EEPMBits(*outC)};
        EEUnitStateNote(kUnitStateSteadyTurn, inUnit->mUnitID, theWords, 9);
    }
}

// FUNCTION: EXE 0x554bde
// Returns the unit's new speed: accelerates (not past its top speed or its last waypoint), holds, or brakes
// to a stop, as SpeedMode decides
float EEPhysicsModel::Accelerate(EEComplexUnit* inUnit, float inDistance, float inTurn)
{
    float theSpeed = inUnit->GetVelocity();
    float theMaxSpeed = inUnit->GetMaxVelocity();
    float theAccel = inUnit->GetAcceleration();
    long theMode = this->SpeedMode(inUnit, inDistance, inTurn);
    return EEPMAccelerate(inUnit, theSpeed, theMaxSpeed, theAccel, inDistance, theMode);
}

float EEPMAccelerate(EEComplexUnit* inUnit, float inSpeed, float inMaxSpeed, float inAccel, float inDistance, long inMode)
{
    EeUnitsSpeedIn theIn;
    memset(&theIn, 0, sizeof(theIn));
    theIn.tick_seconds = gPhysics->mTickSeconds;
    theIn.speed = inSpeed;
    theIn.max_speed = inMaxSpeed;
    theIn.accel = inAccel;
    theIn.waypoints = (int32_t)inUnit->mWaypoints.size();
    float theResult = ee_units_pm_accelerate(&theIn, inDistance, (int32_t)inMode);
    if (EEUnitStepNoting()) {   // TEST ONLY (USST)
        uint32_t theWords[8] = {EEPMBits(theIn.tick_seconds), EEPMBits(inSpeed), EEPMBits(inMaxSpeed), EEPMBits(inAccel),
                                (uint32_t)theIn.waypoints, EEPMBits(inDistance), (uint32_t)inMode, EEPMBits(theResult)};
        EEUnitStateNote(kUnitStateAccelerate, inUnit->mUnitID, theWords, 8);
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

// TEST ONLY (USST): a speed mode's inputs and result (the remaining distance and the terrain pitch as read)
void EEPMNoteSpeedMode(EEComplexUnit* inUnit, bool inHelicopter, float inRemaining, float inPitch, float inDistance,
                       float inTurn, long inMode)
{
    if (!EEUnitStepNoting())
        return;
    uint32_t theWords[15] = {inHelicopter ? 1u : 0u, EEPMBits(gPhysics->mTickSeconds), EEPMBits(gPhysics->mTicksPerGameSecond),
                             EEPMBits(inUnit->GetVelocity()), EEPMBits(inUnit->GetMaxVelocity()), EEPMBits(inUnit->GetAcceleration()),
                             EEPMBits(inUnit->GetInverseAcceleration()), EEPMBits(inUnit->mAngularVelocity.GetYaw()),
                             inHelicopter ? 0u : EEPMBits(inUnit->mUnitClass->mSlopeSpeedPenalty), (uint32_t)inUnit->mWaypoints.size(),
                             EEPMBits(inRemaining), EEPMBits(inPitch), EEPMBits(inDistance), EEPMBits(inTurn), (uint32_t)inMode};
    EEUnitStateNote(kUnitStateSpeedMode, inUnit->mUnitID, theWords, 15);
}

// FUNCTION: EXE 0x554cb2
// Returns the unit's speed mode: 3 accelerate, 2 hold, 4 brake (a sharp turn, within braking distance of the
// path's end, or faster than the slope allows)
long EEPhysicsModel::SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn)
{
    return EEPMSpeedMode(inUnit, false, inDistance, inTurn);
}

long EEPMSpeedMode(EEComplexUnit* inUnit, bool inHelicopter, float inDistance, float inTurn)
{
    EeUnitsSpeedIn theIn;
    theIn.tick_seconds = gPhysics->mTickSeconds;
    theIn.ticks_per_second = gPhysics->mTicksPerGameSecond;
    theIn.speed = inUnit->GetVelocity();
    theIn.max_speed = inUnit->GetMaxVelocity();
    theIn.accel = inUnit->GetAcceleration();
    theIn.inverse_accel = inUnit->GetInverseAcceleration();
    theIn.yaw_rate = inUnit->mAngularVelocity.GetYaw();
    theIn.slope_penalty = inHelicopter ? 0.0f : inUnit->mUnitClass->mSlopeSpeedPenalty;
    theIn.waypoints = (int32_t)inUnit->mWaypoints.size();
    // the reads the mode makes only when it needs them: the path's length, the terrain's pitch under the unit
    theIn.path_length = 0.0f;
    if (theIn.waypoints >= 2)
        inUnit->GetPathLength(theIn.path_length);
    theIn.terrain_pitch = 0.0f;
    if (!inHelicopter && theIn.slope_penalty < 1.0f)
        theIn.terrain_pitch = ::gWorld.mMap->GetTerrainPitch(inUnit->mPhysicalModel.GetLocation());
    long theMode = ee_units_pm_speed_mode(&theIn, inDistance, inTurn, inHelicopter);
    EEPMNoteSpeedMode(inUnit, inHelicopter, theIn.waypoints >= 2 ? theIn.path_length : inDistance, theIn.terrain_pitch,
                      inDistance, inTurn, theMode);   // TEST ONLY (USST)
    return theMode;
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
    return EEPMPitchMode(inUnit, false, inRate, inAngle);
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
    EeUnitsTurnIn theIn = PMTurnIn(inUnit);
    return ee_units_pm_yaw_mode(&theIn, inRate, inAngle);
}

// FUNCTION: EXE 0x5550ba
// Destroys the twelve physics model singletons
void EEFreePhysicsModels()
{
    for (long theIndex = 0; theIndex < 12; ++theIndex) {
        if (gPhysicsModels[theIndex])
            delete gPhysicsModels[theIndex];
        gPhysicsModels[theIndex] = NULL;
    }
}

// FUNCTION: EXE 0x5550dc
// Re-bobs a unit that stands still (except under the helicopter model), else brakes it as Step4
void EEPhysicsModel::Step3(EEComplexUnit* inUnit)
{
    if (inUnit->GetVelocity() == 0.0f && inUnit->mAngularVelocity.mYaw == 0.0f && this != gPhysicsModels[2]) {
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
    float theAccel = inUnit->GetAngularAcceleration();
    float theMax = inUnit->GetMaxAngularVelocity();
    EeUnitsTurn theTurn = ee_units_pm_spin_down(theRate, theHeading, theStep, theAccel, theMax);
    *outRate = theTurn.rate;
    *outNew = theTurn.heading;
    if (EEUnitStepNoting()) {   // TEST ONLY (USST)
        uint32_t theWords[7] = {EEPMBits(theStep), EEPMBits(theAccel), EEPMBits(theMax), EEPMBits(theRate),
                                EEPMBits(theHeading), EEPMBits(*outRate), EEPMBits(*outNew)};
        EEUnitStateNote(kUnitStateSpinDown, inUnit->mUnitID, theWords, 7);
    }
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
    if (theNear || (theCrossing && theT < 0.75f && theOtherT < 0.75f)) {
        float theDX;
        if (theOtherSpeed <= theSpeed && !theNear && inOther->GetMaxVelocity() > 0.0f) {
            if (inUnit->GetVelocity() > 0.0f && inOther->GetVelocity() > 0.0f && theOtherSpeed == theSpeed) {
                if ((long) inOther->mWaypoints.size() > 0)
                    inOther->GetPathLength(theOtherLeft);
                if ((long) inUnit->mWaypoints.size() > 0)
                    inUnit->GetPathLength(theLeft);
                if (theLeft > theOtherLeft || (theOtherLeft == theLeft && inOther->mUnitID < inUnit->mUnitID)) {
                    inUnit->SetVelocity(inUnit->GetVelocity() * 0.25f);
                    inUnit->mCollisionResolutionCount = 0.0f;
                }
            }
            theDX = theOtherLine.GetDirection().GetX();
            theDX = inOther->GetMaxVelocity() * (1.25f - theT) * theDX * theOtherSign;
            float theDirY = theOtherLine.GetDirection().GetY();
            float theDY = inOther->GetMaxVelocity() * (1.25f - theT) * theDirY * theOtherSign;
            inOther->SetPushVelocityX(inOther->GetPushVelocityX() + theDX);
            inOther->SetPushVelocityY(inOther->GetPushVelocityY() + theDY);
        } else {
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
            float theSpeedNext = theSlow + 0.5f;
            theDX = theLine.GetDirection().GetX();
            theDX = inUnit->GetMaxVelocity() * (theSpeedNext - theT) * theDX * theSign;
            theOtherSign = theLine.GetDirection().GetY();
            float theDY = inUnit->GetMaxVelocity() * (theSpeedNext - theT) * theOtherSign * theSign;
            inUnit->SetPushVelocityX(inUnit->GetPushVelocityX() + theDX);
            inUnit->SetPushVelocityY(inUnit->GetPushVelocityY() + theDY);
            if (theSlow > 0.0f) {
                inUnit->SetVelocity(inUnit->GetVelocity() * 0.5f);
            } else {
                inUnit->SetVelocity(0.0f);
                inUnit->mCollisionResolutionCount += gPhysics->GetTickTime();
            }
            float theAbsX = UMAbs(inOther->GetPushVelocityX());
            if (theAbsX < kPMTurnEpsilon && UMAbs(inOther->GetPushVelocityY()) < kPMTurnEpsilon) {
                inOther->SetPushVelocityX(kPMTurnEpsilon);
                inOther->SetPushVelocityY(kPMTurnEpsilon);
            }
        }
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
            && inOther == static_cast<EEUGRust*>(inUnit->mGoalState)->GetAttackTarget()) {
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
    // the trample: the shared damage function (ee_combat damage.rs; combat.yaml trample_damage/_threshold)
    long theAmount;
    if (EECombatComputeDamage(kDamageTrample, inUnit, inOther, inUnit->mStats->GetAttack(), inUnit->mUnitClass->mAmmoType,
                              inUnit->mUnitClass->mWeapon, 0.0f, theAmount))
        inOther->TakeHit(inUnit, theAmount, inUnit->mPlayer->mPlayerID, 1);
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
            if (EEUnitClassCast<EEUCGate>(theNode->mData->mUnitClass)
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

// TEST ONLY (USST): a float's bits
static uint32_t PMBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

uint32_t EEPMBits(float inValue)
{
    return PMBits(inValue);
}

void EEPMNotePush(EEComplexUnit* inUnit, float inSpeed, bool inMoving, float inDX, float inDY, float inX, float inY)
{
    if (!EEUnitStepNoting())
        return;
    uint32_t theWords[13] = {PMBits(inUnit->mPushVelocityX.Get()), PMBits(inUnit->mPushVelocityY.Get()),
                             (uint32_t)gCollisionPeriod, PMBits(inSpeed), PMBits(gPhysics->GetTickSeconds()),
                             PMBits(inUnit->mPhysicalModel.GetLocation().GetX()),
                             PMBits(inUnit->mPhysicalModel.GetLocation().GetY()), 0, 0, 0, 0, 0, 0};
    if (inMoving) {
        theWords[7] = 1;
        theWords[8] = inDY + inDX > kPMMoveDistance ? 1u : 0u;
        theWords[9] = PMBits(inDX);
        theWords[10] = PMBits(inDY);
        theWords[11] = PMBits(inX);
        theWords[12] = PMBits(inY);
    }
    EEUnitStateNote(kUnitStatePush, inUnit->mUnitID, theWords, 13);
}

bool EEPMPush(EEComplexUnit* inUnit, float inSpeed, float& outX, float& outY, bool& outPast)
{
    EeUnitsPushIn theIn;
    theIn.push_x = inUnit->mPushVelocityX;
    theIn.push_y = inUnit->mPushVelocityY;
    theIn.loc_x = inUnit->mPhysicalModel.GetLocation().GetX();
    theIn.loc_y = inUnit->mPhysicalModel.GetLocation().GetY();
    theIn.period = (uint32_t)gCollisionPeriod;
    theIn.max_velocity = inSpeed;
    theIn.tick_seconds = gPhysics->GetTickSeconds();
    EeUnitsPush thePush = ee_units_step_push(static_cast<uint32_t>(inUnit->mUnitID), reinterpret_cast<uintptr_t>(inUnit), &theIn);
    EEPMNotePush(inUnit, inSpeed, thePush.moving != 0, thePush.dx, thePush.dy, thePush.x, thePush.y);   // TEST ONLY (USST)
    outX = thePush.x;
    outY = thePush.y;
    outPast = thePush.past_move != 0;
    return thePush.moving != 0;
}

// FUNCTION: EXE 0x556213
// Applies the collision push to the unit (times the collision period, capped at its speed) onto a tile it
// can stand on, puts it on the ground, and starts or stops an idle unit's shuffle animation
// NEAR: x87 scheduling of the sign/abs/limit block (as Helicopter V0C), the frame layout and the flag threading
void EEPhysicsModel::Move(EEComplexUnit* inUnit)
{
    this->MoveByPush(inUnit, true);
}

void EEPhysicsModel::MoveByPush(EEComplexUnit* inUnit, bool inOnGround)
{
    EETileCoordinate theTile(0, 0);
    bool theStopped = false;
    float theX, theY;
    bool thePast;
    // New (R5b.4 leaf 6): the push's arithmetic up to the new point is Rust over the unit store (ee_units motion.rs push)
    if (EEPMPush(inUnit, inUnit->GetMaxVelocity(), theX, theY, thePast)) {
        long theTileY = UMFloor(theY);
        long theTileX = UMFloor(theX);
        theTile.mX = theTileX;
        theTile.mY = theTileY;
        bool theFree = EECanStandOn(theTile, inUnit);
        if (theFree && UMAbs(inUnit->GetTileX() - theTile.mX) == 1 && UMAbs(inUnit->GetTileY() - theTile.mY) == 1) {
            EETileCoordinate theSideX;
            theSideX.mX = theTile.mX;
            theSideX.mY = inUnit->GetTileY();
            if (!EECanStandOn(theSideX, inUnit)) {
                EETileCoordinate theSideY;
                theSideY.mX = inUnit->GetTileX();
                theSideY.mY = theTile.mY;
                if (!EECanStandOn(theSideY, inUnit))
                    theFree = false;
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
                if (thePast) {
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
            if (inOnGround && !inUnit->IsSwimming()) {
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
        // New (R5b.4 leaf 6): the decay is Rust over the unit store (ee_units motion.rs roll_decay). TEST ONLY: while the
        // capture runs, the original's statements check it (USST)
        float theRollBefore = inUnit->GetRoll();
        float theRoll = ee_units_step_roll(static_cast<uint32_t>(inUnit->mUnitID), reinterpret_cast<uintptr_t>(inUnit),
                                           theRollBefore, gPhysics->mTickSeconds);
        inUnit->mRoll = theRoll;
        if (EEUnitStepNoting()) {   // TEST ONLY (USST)
            float theOriginal;
            if (theRollBefore > 0.0f) {
                theOriginal = theRollBefore;
                theOriginal -= gPhysics->mTickSeconds * 0.5f;
                if (theOriginal <= 0.0f)
                    theOriginal = 0.0f;
            } else {
                theOriginal = gPhysics->mTickSeconds * 0.5f + theRollBefore;
                if (theOriginal >= 0.0f)
                    theOriginal = 0.0f;
            }
            if (PMBits(theOriginal) != PMBits(theRoll))
                EEUnitStepDiffers("EEPhysicsModel::AfterStep (the roll's bits)", inUnit->mUnitID, (long)PMBits(theRoll),
                                  (long)PMBits(theOriginal));
            uint32_t theWords[3] = {PMBits(theRollBefore), PMBits(gPhysics->mTickSeconds), PMBits(inUnit->mRoll.Get())};
            EEUnitStateNote(kUnitStateRoll, inUnit->mUnitID, theWords, 3);
        }
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
        if (theMap->GetTile(theX, theY)) {
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
        } else {
            theMoveLine.GetPointAt(-1.0f, ioLocation);
            theResult = false;
        }
    }
    return theResult;
}


// SYNTHETIC: EXE 0x556ddb
// (the scalar deleting destructor, emitted with the vtable)

// the ICF-shared copy LINK kept in this unit's range
// SYNTHETIC: EXE 0x556dd8

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body of EEABuild::Virtual8)
void EEPhysicsTrace(char const *,unsigned long,class GEOrientation)
{
}
