// C:\EEX\Empire Earth\Physics Thread\EEPhysicsModel.cpp (file name inferred: no __FILE__; RTTI EEPhysicsModel; one
// unit per physics-model class, alphabetical link order). Range 0x1543b7..0x156df8 (each unit ends with its own
// inline-COMDAT cluster).
// The singletons' creation/destruction (0x1543b7 / 0x1550ba) open the unit: they `new` every subclass.
//
// New (the physics models' lane, round 1): the models are whole in Rust (ee_units pm.rs, pm_ground.rs, pm_air.rs),
// with every EEPM* class (EEPMLand.cpp .. EEPMSubmergedNaval.cpp, deleted). This unit keeps the singletons, the three
// forwarders, the unit's group-aware rates it always held, the TEST ONLY PMST record and the C++ the models call
// (ee_units_pm_host_*, pm_host.rs): the unit's location and setters, its path and goals, the map's tiles and terrain,
// the effects, the projectile's and missile's steps (ee_combat's arithmetic with the combat lane's captures) and the
// carrier planes' deck walk (the carrier model's tag points).
#include <windows.h>
#include <vector>
#include <math.h>
#include <string.h>
#include <stdio.h>

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
#include "lle/GEQuaternion.h"

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
#include "ee/EETileCoordinate.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUGRust.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUnitGoal.h"
#include "ee/EECombatGoals.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUnitStore.h"
#include "ee/EESimCapture.h"
#include "rust/ee_units.h"

extern EEToHitTable gToHitTable;           // 0x5318e8 (Empire Earth.cpp; as EECPAttackSubGroupUnit.cpp declares it)

EEPhysicsModel* gPhysicsModels[12];            // 0x531d58: one model per kind (EEUnitClass +0x110 points at one)
// This unit's .data is 0x4c2368..0x4c2380 (after EEPhysics.cpp's RTTI, before the EEPMProjectile type descriptor):
// 0.5f, 0.75f, 1.0f, 4000, 2000, 1. 0x4c236c/0x4c2370 have no reader in the exe and stay undefined here.
float gPhysicsModelsF = 0.5f;       // 0x4c2368: world table entry 79's float, set with the singletons (provisional name)
long gUnitAIWaitScale = 4000;               // 0x4c2374 (EEUnitAI 0x22eca0's wait scale; provisional name)
unsigned long gCollisionDetectionUnit = 2000;   // 0x4c2378 (EmpireEarth.h; EEWorld resets it to 2000)
unsigned long gCollisionPeriod = 1;         // 0x4c237c (EEPhysics.cpp's collision period; provisional name)

// Creates the twelve physics model singletons (land to spaceship) and reads world table entry 79's float
void EEInitPhysicsModels()
{
    for (long theIndex = 0; theIndex < 12; ++theIndex)
        gPhysicsModels[theIndex] = new EEPhysicsModel(theIndex);
    gPhysicsModelsF = gWorldTable.GetFloatValue(kWorldTrampleMinDistanceSquared);
}

// Destroys the twelve physics model singletons
void EEFreePhysicsModels()
{
    for (long theIndex = 0; theIndex < 12; ++theIndex) {
        if (gPhysicsModels[theIndex])
            delete gPhysicsModels[theIndex];
        gPhysicsModels[theIndex] = NULL;
    }
}

// Returns the unit's top turn rate (its active group's when it is in one)
float EEComplexUnit::GetMaxAngularVelocity()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetMaxAngularVelocity(this);
    return this->mStats->GetMaxAngularVelocity();
}

// Returns the unit's turn acceleration (its active group's when it is in one)
float EEComplexUnit::GetAngularAcceleration()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetAngularAcceleration(this);
    return this->mStats->mAngularAcceleration;
}

// Returns the unit's acceleration (its active group's when it is in one)
float EEComplexUnit::GetAcceleration()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetAcceleration(this);
    return this->mStats->GetAcceleration();
}

// Returns the inverse of the unit's acceleration (its active group's when it is in one)
float EEComplexUnit::GetInverseAcceleration()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetInverseAcceleration(this);
    return this->mStats->mInverseAcceleration;
}

// Returns the inverse of the unit's turn acceleration (its active group's when it is in one)
float EEComplexUnit::GetInverseAngularAcceleration()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetInverseAngularAcceleration(this);
    return this->mStats->mInverseAngularAcceleration;
}

// NEAR: register choice only (ecx vs eax for the class pointer)
// Returns whether the unit swims: it has a swim animation, stands on deep water and cruises above the ground
bool EEComplexUnit::IsSwimming()
{
    return this->HasSwimAnimation() && this->mTile && this->mTile->mTerrainType == kTerrainDeepWater
        && this->mUnitClass->GetCruiseAltitude()
            > ::gWorld.mMap->mTerrainMesh.GetMeshZ(this->mPhysicalModel.GetLocation().GetX(), this->mPhysicalModel.GetLocation().GetY());
}

// Returns whether the unit can move on terrain kind 1 and its model has more than one frame of animation 2
bool EEComplexUnit::HasSwimAnimation()
{
    return this->mUnitClass->CanMoveOnTerrain(this, 1) && this->GetModel()->mFrameCounts[2] > 1;
}

// the unit's records for Rust (its three bases)
static EeUnitsPmUnit PMView(EEComplexUnit* inUnit)
{
    EeUnitsPmUnit theView;
    theView.obj = reinterpret_cast<uintptr_t>(inUnit);
    theView.rts = static_cast<EeUnitsRtsUnitFields*>(inUnit);
    theView.unit = static_cast<EeUnitsUnitFields*>(inUnit);
    theView.cx = static_cast<EeUnitsComplexUnitFields*>(inUnit);
    return theView;
}

// TEST ONLY: a float's bits
static uint32_t PMBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

// TEST ONLY: the physics models' step record (PMST, the physics models' lane, round 1). Every Step call folds the
// unit's id, its model's index, its physics action before the step and the state the models write after it (the
// location's matrix, the velocity, the angular velocity, the push, the roll, the bob phase, the collision time, state
// and blocking unit, the physics action, the tile, the arrival flag, the animation, its pause and time, the hit
// points, the contact location, the push immunity, the orientation, the height; floats as bits) into the tick's
// FNV-1a 64; the record (the tick, the calls, the digest) is written when the next tick's first step comes. Nothing
// is noted unless the capture runs. Its golden came from the C++ models (tools/sim_capture.py).
static uint64_t sPMStepDigest = 0xcbf29ce484222325ull;
static uint32_t sPMStepCalls = 0;
static unsigned long sPMStepTick = 0;

static void PMStepFNV(uint32_t inWord)
{
    for (int i = 0; i < 4; i++) {
        sPMStepDigest ^= (inWord >> (8 * i)) & 0xff;
        sPMStepDigest *= 0x100000001b3ull;
    }
}

static void PMStepCommit()
{
    if (sPMStepCalls != 0) {
        EESimCaptureRecord theRecord(0x54534d50u);   // "PMST"
        theRecord.U32((unsigned int)sPMStepTick);
        theRecord.U32(sPMStepCalls);
        theRecord.U32((unsigned int)(sPMStepDigest & 0xffffffffu));
        theRecord.U32((unsigned int)(sPMStepDigest >> 32));
        theRecord.Commit();
    }
    sPMStepDigest = 0xcbf29ce484222325ull;
    sPMStepCalls = 0;
}

// TEST ONLY: the steps per model so far (the lane's coverage of the capture's games), printed to stderr every 4000
// ticks while the capture runs: "EE-PM-COUNT <tick> <land> <airplane> .. <spaceship>"
static uint32_t sPMStepKinds[12];
static uint32_t sPMDeckWalks = 0;   // the deck walk's placements (OrientOnCarrier's route)

static void PMStepNote(EEPhysicsModel* inModel, EEComplexUnit* inUnit, unsigned long inAction)
{
    if (gPhysics->mNumTicksDone != sPMStepTick) {
        PMStepCommit();
        sPMStepTick = gPhysics->mNumTicksDone;
        if (sPMStepTick % 4000 == 0) {
            fprintf(stderr, "EE-PM-COUNT %lu", sPMStepTick);
            for (int i = 0; i < 12; i++)
                fprintf(stderr, " %u", sPMStepKinds[i]);
            fprintf(stderr, " deck %u\n", sPMDeckWalks);
        }
    }
    uint32_t theModel = 0xff;
    for (uint32_t i = 0; i < 12; i++)
        if (gPhysicsModels[i] == inModel)
            theModel = i;
    if (theModel < 12)
        sPMStepKinds[theModel]++;
    PMStepFNV((uint32_t)inUnit->mUnitID);
    PMStepFNV(theModel);
    PMStepFNV((uint32_t)inAction);
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            PMStepFNV(PMBits(theLocation.mMatrix[r][c]));
    const float theFloats[] = {inUnit->mVelocity, inUnit->mAngularVelocity.mYaw, inUnit->mAngularVelocity.mPitch,
                               inUnit->mAngularVelocity.mRoll, inUnit->mPushVelocityX, inUnit->mPushVelocityY,
                               inUnit->mRoll, inUnit->mF220, inUnit->mCollisionResolutionCount,
                               inUnit->mInitialContactLocation.mX, inUnit->mInitialContactLocation.mY,
                               inUnit->mInitialContactLocation.mZ, inUnit->mOrientation.mYaw,
                               inUnit->mOrientation.mPitch, inUnit->mOrientation.mRoll, inUnit->mHeight};
    for (size_t i = 0; i < sizeof(theFloats) / sizeof(theFloats[0]); i++)
        PMStepFNV(PMBits(theFloats[i]));
    PMStepFNV((uint32_t)inUnit->mCollisionState);
    PMStepFNV(inUnit->mBlockingUnit ? (uint32_t)inUnit->mBlockingUnit->mUnitID : 0xfffffffeu);
    PMStepFNV((uint32_t)inUnit->mCurrentPhysicsAction);
    PMStepFNV((uint32_t)inUnit->mTileX);
    PMStepFNV((uint32_t)inUnit->mTileY);
    PMStepFNV(inUnit->mHasArrived ? 1u : 0u);
    PMStepFNV((uint32_t)inUnit->mAnimation);
    PMStepFNV(inUnit->mIsAnimationPaused ? 1u : 0u);
    PMStepFNV((uint32_t)inUnit->mAnimationTime);
    PMStepFNV((uint32_t)inUnit->mHitPoints);
    PMStepFNV((uint32_t)inUnit->mPushImmuneUntil);
    sPMStepCalls++;
}

// Runs the step for the unit's physics action (0..5), then the after-step work (not after action 2, nor for
// a dead unit in another action): Rust (ee_units pm.rs step)
void EEPhysicsModel::Step(EEComplexUnit* inUnit)
{
    unsigned long theAction = inUnit->mCurrentPhysicsAction;   // TEST ONLY (PMST)
    ee_units_pm_step((int32_t)this->mKind, PMView(inUnit));
    if (EESimCaptureRecord::Active())   // TEST ONLY (PMST)
        PMStepNote(this, inUnit, theAction);
}

void EEPhysicsModel::Step0(EEComplexUnit* inUnit)
{
    ee_units_pm_step0((int32_t)this->mKind, PMView(inUnit));
}

float EEPhysicsModel::ComputeRoll(EEComplexUnit* inUnit, float inValue)
{
    return ee_units_pm_compute_roll((int32_t)this->mKind, PMView(inUnit), inValue);
}

// ---- the projectile's and the missile's steps (the arithmetic is ee_combat's flight.rs; the captures are the
// combat lane's CBPJ, CBPS, CBMA, CBMH)

// NEAR: frame/float slot order; the original calls vector<long>::operator[] out of line (0x11e2a5) for the first lookup
// Places the launched projectile on its ballistic arc from the launch point for its flight time so far,
// nose along the arc; a meteor flies straight at its top speed, without gravity (new: the arc and the placement are
// ee_combat's arc and projectile_launch)
// Fields: start point (+0x10c), yaw/pitch (+0x188/+0x18c), the launcher class's launch speed (else its own) for
// +0x290 ms of flight under the class's gravity (+0x13c); a type 0x325 flies straight at its speed
static void PMProjectileStep0(EEComplexUnit* inUnit)
{
    long theTileX;
    long theTileY;
    inUnit->mInitialContactLocation = inUnit->mPhysicalModel.GetLocation().GetPosition();
    EeCombatArcIn theIn = {};
    theIn.flight_time_ms = (int32_t)inUnit->mFlightTime;
    theIn.gravity = inUnit->mUnitClass->mAcceleration;
    theIn.pitch = inUnit->mOrientation.mPitch;
    theIn.meteor = inUnit->mUnitClass->mUnitType == kUnitTypeMeteor;
    if (theIn.meteor) {
        theIn.max_velocity = inUnit->GetMaxVelocity();
    } else {
        theIn.launcher_speed = inUnit->mPlayer->mUnitClassUpgrades[inUnit->mLauncherClass->mUnitType]->mLaunchSpeed;
        if (!(theIn.launcher_speed > 0.001f))   // read only for the fallback
            theIn.own_speed = inUnit->mPlayer->mUnitClassUpgrades[inUnit->mUnitClass->mUnitType]->mLaunchSpeed;
    }
    EeCombatArcOut theArc = EECombatArc(theIn);
    // the placement on the arc is ee_combat's projectile_launch (flight.rs)
    EeCombatFlightIn in = {};
    in.start[0] = inUnit->mProjectileStart.GetX();
    in.start[1] = inUnit->mProjectileStart.GetY();
    in.start[2] = inUnit->mProjectileStart.GetZ();
    in.yaw = inUnit->mOrientation.mYaw;
    in.move_y = theArc.move_y;
    in.move_z = theArc.move_z;
    in.pitch = theArc.pitch;
    EeCombatFlightOut theOut = ee_combat_projectile_launch(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'P', 'J'), in, theOut);
    GETransformation theTransformation(false);
    memcpy(theTransformation.mMatrix, theOut.matrix, sizeof(theOut.matrix));
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}

// NEAR: frame layout (the original keeps the step in the dead inUnit slot; 0x94 vs our 0xa0)
// Moves the projectile one tick along its heading and pulls its nose down by its class's gravity; only
// while it is at or below height 0 and above the ground point (new: ee_combat's projectile_step)
// Class gravity +0x13c; the height-0 test is the original's (bytes 0x55d2e4: `fcomp 0.0`, `ja` returns)
static void PMProjectileStep2(EEComplexUnit* inUnit)
{
    // the tick is ee_combat's projectile_step (flight.rs); the ground point is read only at or below height 0
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    long theTileX;
    long theTileY;
    EeCombatFlightIn in = {};
    memcpy(in.location, theLocation.mMatrix, sizeof(in.location));
    in.gravity = inUnit->mUnitClass->mAcceleration;
    in.step = gPhysics->mTickSeconds;
    in.speed = inUnit->mVelocity;
    if (!(theLocation.GetZ() > 0.0f)) {
        GE3DPoint thePosition = theLocation.GetPosition();
        GE3DPoint theGround;
        ::gWorld.mMap->GetGroundPoint(thePosition, theGround);
        in.ground_read = 1;
        in.ground_z = theGround.GetZ();
    }
    EeCombatFlightOut theOut = ee_combat_projectile_step(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'P', 'S'), in, theOut);
    if (!theOut.moved)
        return;
    GETransformation theTransformation(false);
    memcpy(theTransformation.mMatrix, theOut.matrix, sizeof(theOut.matrix));
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}

// Speeds the missile up along its heading (capped at its top speed) and moves it by one tick (new: ee_combat's
// missile_accelerate)
static void PMMissileAccelerate(EEComplexUnit* inUnit)
{
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EeCombatFlightIn in = {};
    memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    in.step = gPhysics->mTickSeconds;
    in.speed = inUnit->GetVelocity();
    in.max_speed = theStats->GetMaxVelocity();
    in.accel = theStats->GetAcceleration();
    EeCombatFlightOut theOut = ee_combat_missile_accelerate(&in);
    inUnit->SetVelocity(theOut.speed);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'M', 'A'), in, theOut);
    GETransformation theTransformation(false);
    memcpy(theTransformation.mMatrix, theOut.matrix, sizeof(theOut.matrix));
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}

// Turns the missile toward its drop target by at most its turn rate per tick, slows it the further it
// points off target (down to 1/4 of top speed) and moves it by one tick (new: ee_combat's missile_home; the drop
// target stays here)
static void PMMissileHome(EEComplexUnit* inUnit)
{
    GE3DPoint theTarget;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EEUnit* theDropTarget;
    EEFindDropLocation(inUnit, &theDropTarget, &theTarget, NULL);
    EeCombatFlightIn in = {};
    memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    in.target[0] = theTarget.GetX();
    in.target[1] = theTarget.GetY();
    in.target[2] = theTarget.GetZ();
    in.step = gPhysics->mTickSeconds;
    in.speed = inUnit->mVelocity;
    in.max_speed = theStats->GetMaxVelocity();
    in.accel = theStats->GetAcceleration();
    in.max_angular = theStats->GetMaxAngularVelocity();
    EeCombatFlightOut theOut = ee_combat_missile_home(&in);
    inUnit->SetVelocity(theOut.speed);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'M', 'H'), in, theOut);
    GETransformation theNew(false);
    memcpy(theNew.mMatrix, theOut.matrix, sizeof(theOut.matrix));
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theNew, inUnit, &theTileX, &theTileY);
}

// Records the missile's position as its contact point, then flies it straight on while its boost time
// lasts, else homes it on its target
static void PMMissileStep0(EEComplexUnit* inUnit)
{
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    inUnit->mInitialContactLocation = GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    if (inUnit->mFlightTime > 0)
        PMMissileAccelerate(inUnit);
    else
        PMMissileHome(inUnit);
}

// ---- the C++ the Rust models call (ee_units pm_host.rs; each the original statement it replaces)

static EEComplexUnit* PMUnit(uint64_t inObject)
{
    return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inObject));
}

static uint64_t PMObject(const void* inPointer)
{
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(inPointer));
}

static void PMSetMatrix(GETransformation& outTransformation, const float* inMatrix)
{
    memcpy(outTransformation.mMatrix, inMatrix, 16 * sizeof(float));
}

extern "C" void ee_units_pm_host_tick(EeUnitsPmTick* outTick)
{
    outTick->tick_seconds = gPhysics->mTickSeconds;
    outTick->ticks_per_second = gPhysics->mTicksPerGameSecond;
    outTick->tick_time = gPhysics->GetTickTime();
    outTick->game_time = (int32_t)gPhysics->mGameTime;
    outTick->collision_period = (uint32_t)gCollisionPeriod;
    outTick->detection_flags = (uint32_t)gWorld.mCollisionDetectionFlags;
    outTick->trample_min = gPhysicsModelsF;
    outTick->height_epsilon = kHeightEpsilon;
}

extern "C" float ee_units_pm_host_random()
{
    return gPhysics->GetRandom();
}

extern "C" uint8_t ee_units_pm_host_noting()
{
    return EEUnitStepNoting() ? 1 : 0;
}

extern "C" void ee_units_pm_host_note(uint32_t inKind, uint32_t inID, const uint32_t* inWords, uint32_t inCount)
{
    EEUnitStateNote(inKind, inID, inWords, inCount);
}

extern "C" void ee_units_pm_host_view(uint64_t inObject, EeUnitsPmUnit* outView)
{
    *outView = PMView(PMUnit(inObject));
}

extern "C" void ee_units_pm_host_class(uint64_t inObject, EeUnitsPmClass* outClass)
{
    EEUnitClass* theClass = PMUnit(inObject)->mUnitClass;
    outClass->category = (uint32_t)theClass->mCategory;
    outClass->unit_type = (uint32_t)theClass->mUnitType;
    outClass->air_traffic_type = (int32_t)theClass->mAirTrafficType;
    outClass->tile_size = (int32_t)theClass->mTileSize;
    outClass->pitches_with_terrain = theClass->mPitchesWithTerrain ? 1 : 0;
    outClass->rolls_with_terrain = theClass->mRollsWithTerrain ? 1 : 0;
    outClass->fires_broadside = theClass->mFiresBroadside ? 1 : 0;
    outClass->is_melee = theClass->mIsMelee ? 1 : 0;
    outClass->has_splash_effect = theClass->mSplashEffect ? 1 : 0;
    outClass->has_death_effect = theClass->mDeathEffect ? 1 : 0;
    outClass->has_shell_hole_effect = theClass->mShellHoleEffect ? 1 : 0;
    outClass->cruise_altitude = theClass->GetCruiseAltitude();
    outClass->slope_penalty = theClass->mSlopeSpeedPenalty;
    outClass->bob_pitch = theClass->GetBobPitch();
    outClass->bob_height = theClass->GetBobHeight();
    outClass->scale = theClass->GetScale();
    outClass->max_velocity = theClass->GetMaxVelocity();
    outClass->max_roll = theClass->GetMaxRoll();
    outClass->inner_collision_sphere = theClass->mInnerCollisionSphere;
    outClass->facing_offset = theClass->GetFacingOffset();
    outClass->angular_acceleration = theClass->GetAngularAcceleration();
    outClass->max_angular_velocity = theClass->GetMaxAngularVelocity();
}

extern "C" void ee_units_pm_host_stats(uint64_t inObject, EeUnitsPmStats* outStats)
{
    EEUnitClassUpgrade* theStats = PMUnit(inObject)->mStats;
    outStats->max_velocity = theStats->GetMaxVelocity();
    outStats->acceleration = theStats->GetAcceleration();
    outStats->angular_acceleration = theStats->mAngularAcceleration;
    outStats->max_angular_velocity = theStats->mMaxAngularVelocity;
    outStats->scale = theStats->GetScale();
}

extern "C" void ee_units_pm_host_location(uint64_t inObject, float* outMatrix)
{
    memcpy(outMatrix, PMUnit(inObject)->mPhysicalModel.GetLocation().mMatrix, 16 * sizeof(float));
}

extern "C" void ee_units_pm_host_set_transformation(uint64_t inObject, const float* inMatrix)
{
    GETransformation theTransformation(false);
    PMSetMatrix(theTransformation, inMatrix);
    PMUnit(inObject)->SetTransformation(theTransformation, true);
}

extern "C" void ee_units_pm_host_refresh_transformation(uint64_t inObject)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    theUnit->SetTransformation(theUnit->mPhysicalModel.GetLocation(), true);
}

extern "C" void ee_units_pm_host_set_location(uint64_t inObject, const float* inMatrix)
{
    GETransformation theTransformation(false);
    PMSetMatrix(theTransformation, inMatrix);
    PMUnit(inObject)->mPhysicalModel.SetLocation(theTransformation);
}

extern "C" void ee_units_pm_host_set_location_point(uint64_t inObject, float inX, float inY, float inZ)
{
    PMUnit(inObject)->SetLocation(GE3DPoint(inX, inY, inZ), true);
}

extern "C" void ee_units_pm_host_set_location_xy(uint64_t inObject, float inX, float inY)
{
    PMUnit(inObject)->SetLocationXY(inX, inY, true);
}

extern "C" void ee_units_pm_host_update_tile(uint64_t inObject, const float* inMatrix)
{
    GETransformation theTransformation(false);
    PMSetMatrix(theTransformation, inMatrix);
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, PMUnit(inObject), &theTileX, &theTileY);
}

extern "C" void ee_units_pm_host_set_scale(uint64_t inObject, float inScale)
{
    PMUnit(inObject)->mPhysicalModel.SetScale(inScale);
}

extern "C" void ee_units_pm_host_angular_velocity(uint64_t inObject, float* outOrientation)
{
    const GEOrientation& theRate = PMUnit(inObject)->mAngularVelocity;
    outOrientation[0] = theRate.mYaw;
    outOrientation[1] = theRate.mPitch;
    outOrientation[2] = theRate.mRoll;
}

extern "C" void ee_units_pm_host_set_angular_velocity(uint64_t inObject, float inYaw, float inPitch, float inRoll)
{
    PMUnit(inObject)->mAngularVelocity = GEOrientation(inYaw, inPitch, inRoll);
}

extern "C" void ee_units_pm_host_orientation(uint64_t inObject, float* outOrientation)
{
    const GEOrientation& theOrientation = PMUnit(inObject)->mOrientation;
    outOrientation[0] = theOrientation.mYaw;
    outOrientation[1] = theOrientation.mPitch;
    outOrientation[2] = theOrientation.mRoll;
}

extern "C" void ee_units_pm_host_set_orientation(uint64_t inObject, float inYaw, float inPitch, float inRoll)
{
    PMUnit(inObject)->mOrientation = GEOrientation(inYaw, inPitch, inRoll);
}

// the unit's (its active group's) rates: pm_host.rs RATE_*
extern "C" float ee_units_pm_host_rate(uint64_t inObject, int32_t inWhich)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    switch (inWhich) {
    case 0:
        return theUnit->GetMaxVelocity();
    case 1:
        return theUnit->GetAcceleration();
    case 2:
        return theUnit->GetInverseAcceleration();
    case 3:
        return theUnit->GetAngularAcceleration();
    case 4:
        return theUnit->GetMaxAngularVelocity();
    default:
        return theUnit->GetInverseAngularAcceleration();
    }
}

extern "C" int32_t ee_units_pm_host_waypoint_count(uint64_t inObject)
{
    return (int32_t)(long)PMUnit(inObject)->mWaypoints.size();
}

extern "C" int32_t ee_units_pm_host_next_waypoint(uint64_t inObject, float* ioPoint)
{
    GE3DPoint thePoint(ioPoint[0], ioPoint[1], ioPoint[2]);
    long theResult = PMUnit(inObject)->GetNextWaypointPoint(thePoint);
    ioPoint[0] = thePoint.mX;
    ioPoint[1] = thePoint.mY;
    ioPoint[2] = thePoint.mZ;
    return (int32_t)theResult;
}

extern "C" float ee_units_pm_host_first_waypoint_z(uint64_t inObject)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    return theUnit->mWaypoints.empty() ? 0.0f : (*theUnit->mWaypoints.begin()).mLocation.GetZ();
}

extern "C" void ee_units_pm_host_path_length(uint64_t inObject, float* ioLength)
{
    PMUnit(inObject)->GetPathLength(*ioLength);
}

extern "C" uint32_t ee_units_pm_host_command_type(uint64_t inObject)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    return (uint32_t)(theUnit->mCommand ? theUnit->mCommand->mType : 0);
}

extern "C" int32_t ee_units_pm_host_model_parts(uint64_t inObject)
{
    return (int32_t)PMUnit(inObject)->GetModel()->GetNumParts();
}

extern "C" uint32_t ee_units_pm_host_frame_time(uint64_t inObject, uint32_t inAnimation)
{
    return (uint32_t)PMUnit(inObject)->GetModel()->mFrameTimes[inAnimation];
}

extern "C" uint64_t ee_units_pm_host_tile_of(uint64_t inObject)
{
    return PMObject(static_cast<RTSTile*>(PMUnit(inObject)->mTile));
}

extern "C" uint8_t ee_units_pm_host_goal_kind(uint64_t inObject)
{
    EEUnitGoal* theGoal = PMUnit(inObject)->mGoalState;
    return theGoal ? (uint8_t)theGoal->mKind : 0;
}

// the goal's units: pm_host.rs GOAL_* (the attack target and gather resource as EEUnit*, the healed unit and the home
// base as EEComplexUnit*, the cast goal set or not, the build target as EEUnit*)
extern "C" uint64_t ee_units_pm_host_goal_unit(uint64_t inObject, int32_t inWhich)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    EEUnitGoal* theGoal = theUnit->mGoalState;
    switch (inWhich) {
    case 0: {
        EEUGRust* theAttack = EEAttackUnitCast(theGoal);
        return theAttack ? PMObject(theAttack->GetAttackTarget()) : 0;
    }
    case 1: {
        EEUGRust* theHeal = EEHealGoalCast(theGoal);
        return theHeal ? PMObject(theHeal->GetHealTarget()) : 0;
    }
    case 2:
        return EECastCalamityGoalCast(theGoal) ? 1 : 0;
    case 3:
        return theGoal && theGoal->mKind == kGoalGather ? PMObject(static_cast<EEUGRust*>(theGoal)->GetGatherResource()) : 0;
    case 4:
        return theGoal && theGoal->mKind == kGoalBuildRepair ? PMObject(static_cast<EEUGRust*>(theGoal)->GetBuildTarget()) : 0;
    default:
        return PMObject(theUnit->mHomeBase);
    }
}

// the goal's points: pm_host.rs POINT_* (the goal's location, the cast goal's calamity target, the path's start)
extern "C" void ee_units_pm_host_goal_point(uint64_t inObject, int32_t inWhich, float* outPoint)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    GE3DPoint thePoint;
    if (inWhich == 0) {
        if (theUnit->mGoalState)
            theUnit->mGoalState->GetGoalLocation(thePoint);
    } else if (inWhich == 1) {
        EEUGRust* theCast = EECastCalamityGoalCast(theUnit->mGoalState);
        if (theCast)
            theCast->GetCalamityTargetLocation(thePoint);
    } else {
        float theLength;
        theUnit->GetPathStart(thePoint, theLength);
    }
    outPoint[0] = thePoint.mX;
    outPoint[1] = thePoint.mY;
    outPoint[2] = thePoint.mZ;
}

extern "C" uint8_t ee_units_pm_host_goal_facing(uint64_t inObject, float* outYaw)
{
    EEUnitGoal* theGoal = PMUnit(inObject)->GetGoal();   // a location goal or a heal goal (yaw 0)
    if (!EEGoalHasLocation(theGoal))
        return 0;
    *outYaw = EELocationGoalYaw(theGoal);
    return 1;
}

// an EEUnit's values: pm_host.rs UNIT_*
extern "C" uint64_t ee_units_pm_host_unit_value(uint64_t inUnit, int32_t inWhich)
{
    EEUnit* theUnit = reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inUnit));
    switch (inWhich) {
    case 0:
        return (uint64_t)theUnit->mResourceType;
    case 1:
        return PMObject(theUnit->mComplexUnit);
    case 2:
        return (uint64_t)(int64_t)theUnit->GetTileX();
    case 3:
        return (uint64_t)(int64_t)theUnit->GetTileY();
    case 4:
        return (uint64_t)theUnit->mUnitClass->mCategory;
    default:
        return theUnit->mUnitClass->FliesOver() == true ? 1 : 0;
    }
}

extern "C" uint8_t ee_units_pm_host_has_arrived(uint64_t inObject, float inDistance)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    return theUnit->mUnitClass->HasArrived(theUnit, inDistance) ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_is_swimming(uint64_t inObject)
{
    return PMUnit(inObject)->IsSwimming() ? 1 : 0;
}

extern "C" void ee_units_pm_host_place_on_ground(uint64_t inObject, float inX, float inY, float inZ)
{
    GE3DPoint theGround(inX, inY, inZ);
    PMUnit(inObject)->PlaceOnGround(theGround);
}

extern "C" float ee_units_pm_host_height_at(uint64_t inObject, float inX, float inY, float inZ)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    return theUnit->GetUnitClass()->GetHeightAt(theUnit, GE3DPoint(inX, inY, inZ));
}

extern "C" void ee_units_pm_host_set_animation(uint64_t inObject, int32_t inA, int32_t inB, uint8_t inPaused)
{
    PMUnit(inObject)->SetAnimation(inA, inB, inPaused != 0);
}

extern "C" void ee_units_pm_host_set_blocking_unit(uint64_t inObject, uint64_t inBlocker)
{
    // an EEUnit or EEComplexUnit: RTSUnit is the first base of both
    PMUnit(inObject)->SetBlockingUnit(reinterpret_cast<RTSUnit*>(static_cast<uintptr_t>(inBlocker)));
}

extern "C" void ee_units_pm_host_clear_goal_queue(uint64_t inObject)
{
    PMUnit(inObject)->ClearGoalQueue(0);
}

extern "C" void ee_units_pm_host_start_action(uint64_t inObject, int32_t inAction)
{
    PMUnit(inObject)->StartAction(inAction);
}

extern "C" void ee_units_pm_host_action_changed(uint64_t inObject)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    theUnit->mUnitClass->ActionChanged(theUnit);
}

extern "C" uint8_t ee_units_pm_host_can_stand_on(uint64_t inObject, int32_t inX, int32_t inY)
{
    EETileCoordinate theTile(inX, inY);
    return EECanStandOn(theTile, PMUnit(inObject)) ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_friendly(uint64_t inObject, uint64_t inOther)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    EEComplexUnit* theOther = PMUnit(inOther);
    return theOther->mPlayer == theUnit->mPlayer || theUnit->mPlayer->mEnemies[theOther->mPlayer->mPlayerID] == 0 ? 1 : 0;
}

// the trample: the shared damage function (ee_combat damage.rs; combat.yaml trample_damage/_threshold)
extern "C" void ee_units_pm_host_trample(uint64_t inObject, uint64_t inOther)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    EEComplexUnit* theOther = PMUnit(inOther);
    long theAmount;
    if (EECombatComputeDamage(kDamageTrample, theUnit, theOther, theUnit->mStats->GetAttack(), theUnit->mUnitClass->mAmmoType,
                              theUnit->mUnitClass->mWeapon, 0.0f, theAmount))
        theOther->TakeHit(theUnit, theAmount, theUnit->mPlayer->mPlayerID, 1);
}

// the lists the models walk live: pm_host.rs LIST_* (the contacts' nodes and units, a tile's unit nodes and units)
extern "C" uint64_t ee_units_pm_host_list(int32_t inWhich, uint64_t inOf)
{
    uintptr_t theOf = static_cast<uintptr_t>(inOf);
    switch (inWhich) {
    case 0:
        return PMObject(reinterpret_cast<EEComplexUnit*>(theOf)->mContacts.mHead);
    case 1:
        return PMObject(reinterpret_cast<UListNode<EEComplexUnit*>*>(theOf)->mNext);
    case 2:
        return PMObject(reinterpret_cast<UListNode<EEComplexUnit*>*>(theOf)->mData);
    case 3:
        return PMObject(reinterpret_cast<RTSTile*>(theOf)->mUnits);
    case 4:
        return PMObject(reinterpret_cast<RTSTileUnitNode*>(theOf)->mNext);
    default:
        return PMObject(reinterpret_cast<RTSTileUnitNode*>(theOf)->mUnit);
    }
}

extern "C" int32_t ee_units_pm_host_contact_count(uint64_t inObject)
{
    return (int32_t)PMUnit(inObject)->mContacts.mCount;
}

// a contact's gate: 0 none, 1 a gate the unit collides with, 2 a closed gate
extern "C" int32_t ee_units_pm_host_contact_gate(uint64_t inObject)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    if (theUnit->mUnitClass->mCategory != kUnitCategoryGate)
        return 0;
    if (EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->mUnitClass) && (theUnit->mCommand ? theUnit->mCommand->mType : 0) != 0x3a99)
        return 2;
    return 1;
}

extern "C" uint64_t ee_units_pm_host_tile(int32_t inX, int32_t inY)
{
    return PMObject(static_cast<RTSTile*>(::gWorld.mMap->GetTile(inX, inY)));
}

extern "C" void ee_units_pm_host_tile_info(uint64_t inTile, EeUnitsPmTile* outTile)
{
    RTSTile* theTile = reinterpret_cast<RTSTile*>(static_cast<uintptr_t>(inTile));
    outTile->terrain_type = 0;
    outTile->ai_value = 0;
    outTile->height = 0.0f;
    if (theTile == NULL)
        return;
    outTile->terrain_type = (int32_t)theTile->mTerrainType;
    outTile->ai_value = (int8_t)static_cast<EETile*>(theTile)->mAIValue;
    outTile->height = theTile->GetHeight();
}

extern "C" uint8_t ee_units_pm_host_is_edge_tile(int32_t inX, int32_t inY)
{
    return ::gWorld.mMap->IsEdgeTile(inX, inY) ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_can_move_on_terrain(uint64_t inObject, int32_t inTerrain)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    return theUnit->mUnitClass->CanMoveOnTerrain(theUnit, inTerrain) ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_passable_by(uint64_t inUnit, uint64_t inObject)
{
    EEUnit* theUnit = reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inUnit));
    return theUnit->IsPassableBy(PMUnit(inObject)) ? 1 : 0;
}

extern "C" uint8_t ee_units_pm_host_unbuilt_land_building(uint64_t inUnit)
{
    EEUnit* theUnit = reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inUnit));
    return theUnit->mUnitClass->IsLandBuilding() && !theUnit->mComplexUnit->mIsBuilt ? 1 : 0;
}

extern "C" void ee_units_pm_host_ground_point(float inX, float inY, float inZ, float* outPoint)
{
    GE3DPoint theGround;
    ::gWorld.mMap->GetGroundPoint(GE3DPoint(inX, inY, inZ), theGround);
    outPoint[0] = theGround.mX;
    outPoint[1] = theGround.mY;
    outPoint[2] = theGround.mZ;
}

extern "C" float ee_units_pm_host_terrain_tilt(const float* inMatrix, uint8_t inRoll)
{
    GETransformation theTransformation(false);
    PMSetMatrix(theTransformation, inMatrix);
    return inRoll ? ::gWorld.mMap->GetTerrainRoll(theTransformation) : ::gWorld.mMap->GetTerrainPitch(theTransformation);
}

// the terrain mesh's left, right, front and back planes: each its point and normal
extern "C" void ee_units_pm_host_edge_planes(float* outPlanes)
{
    const GETerrainMesh& theMesh = ::gWorld.mMap->mTerrainMesh;
    const GE3DPlane* thePlanes[4] = {&theMesh.GetLeftPlane(), &theMesh.GetRightPlane(), &theMesh.GetFrontPlane(),
                                     &theMesh.GetBackPlane()};
    for (int i = 0; i < 4; i++) {
        outPlanes[i * 6 + 0] = thePlanes[i]->GetPoint().mX;
        outPlanes[i * 6 + 1] = thePlanes[i]->GetPoint().mY;
        outPlanes[i * 6 + 2] = thePlanes[i]->GetPoint().mZ;
        outPlanes[i * 6 + 3] = thePlanes[i]->GetNormal().mX;
        outPlanes[i * 6 + 4] = thePlanes[i]->GetNormal().mY;
        outPlanes[i * 6 + 5] = thePlanes[i]->GetNormal().mZ;
    }
}

// the descending plane's floor: the mesh's height at the point, 0 off its tiles
extern "C" float ee_units_pm_host_mesh_ground(float inX, float inY)
{
    unsigned long theY = UMFloorUnsigned(inY);
    unsigned long theX = UMFloorUnsigned(inX);
    if (::gWorld.GetMap()->mTerrainMesh.GetIsValidTile(theX, theY))
        return ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(inX, inY);
    return 0.0f;
}

extern "C" void ee_units_pm_host_adjust_dive_depth(uint64_t inObject, float* ioPoint)
{
    GE3DPoint thePoint(ioPoint[0], ioPoint[1], ioPoint[2]);
    EEAdjustDiveDepth(PMUnit(inObject), thePoint);
    ioPoint[0] = thePoint.mX;
    ioPoint[1] = thePoint.mY;
    ioPoint[2] = thePoint.mZ;
}

// the effects: pm_host.rs EFFECT_* (the class's splash placed at the point, its death effect on the unit, the impact
// decal, the land ripple)
extern "C" void ee_units_pm_host_effect(uint64_t inObject, int32_t inWhich, float inX, float inY, float inZ)
{
    GE3DPoint thePoint(inX, inY, inZ);
    if (inWhich == 3) {
        gWorld.GetEffectManager()->CreatePlacedEffect(thePoint, EEGraphicEffect::Get(kGraphicEffectLandRipple), NULL);
        return;
    }
    EEComplexUnit* theUnit = PMUnit(inObject);
    if (inWhich == 0)
        gEffectManager->CreatePlacedEffect(thePoint, theUnit->mUnitClass->mSplashEffect, NULL);
    else if (inWhich == 1)
        gEffectManager->CreateUnitEffect(theUnit, theUnit->mUnitClass->mDeathEffect, NULL, 0, 0, 0);
    else
        EEPlaceImpactDecal(theUnit, thePoint);
}

// the carrier's tag points of its animation (pm_air.rs's deck walk): their count (-1: no list); with inIndex >= 0
// point inIndex at the carrier's frame in its location scaled by its stats (IsDocked's and FollowCarrier's
// EETagPointList::Scale and GetPoint)
extern "C" int32_t ee_units_pm_host_carrier_tag(uint64_t inCarrier, int32_t inIndex, float* outPoint)
{
    EEComplexUnit* theCarrier = PMUnit(inCarrier);
    EETagPointList* theTags = theCarrier->GetModel()->mTagPoints[theCarrier->mAnimation];
    if (!theTags)
        return -1;
    if (inIndex >= 0) {
        GETransformation theTransformation(theCarrier->mPhysicalModel.GetLocation());
        EETagPointList::Scale(theTransformation, theCarrier->mStats->GetScale());
        GE3DPoint thePoint;
        theTags->mPoints[inIndex]->GetPoint(theCarrier->mPhysicalModel.GetFrame(), theTransformation, thePoint);
        outPoint[0] = thePoint.mX;
        outPoint[1] = thePoint.mY;
        outPoint[2] = thePoint.mZ;
    }
    return (int32_t)theTags->mPoints.size();
}

// OrientOnCarrier's last call: the class's RouteDockWaypoint(unit)
extern "C" void ee_units_pm_host_route_dock_waypoint(uint64_t inObject)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    sPMDeckWalks++;
    theUnit->mUnitClass->RouteDockWaypoint(theUnit);
}

// a gate's first part (no previous part, a next one): its last linked carrier and last linked part
extern "C" uint8_t ee_units_pm_host_gate_links(uint64_t inObject, uint64_t* outCarrier, uint64_t* outLast)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    if (!(theUnit->mPreviousPart == NULL && theUnit->mNextPart != NULL))
        return 0;
    *outCarrier = PMObject(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->GetUnitClass())->GetLastLinkedCarrier(theUnit));
    *outLast = PMObject(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->GetUnitClass())->GetLastLinked(theUnit));
    return 1;
}

// the projectile's and the missile's steps: pm_host.rs KIND_*
extern "C" void ee_units_pm_host_kind_step(uint64_t inObject, int32_t inWhich)
{
    EEComplexUnit* theUnit = PMUnit(inObject);
    if (inWhich == 0)
        PMProjectileStep0(theUnit);
    else if (inWhich == 1)
        PMProjectileStep2(theUnit);
    else
        PMMissileStep0(theUnit);
}
